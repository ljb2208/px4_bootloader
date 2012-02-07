/*
 * Simple USB CDC/ACM bootloader for STM32F4.
 */

#include <stdlib.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/stm32/f4/flash.h>
#include <libopencm3/stm32/f4/scb.h>
#include <libopencm3/stm32/systick.h>
#include <libopencm3/stm32/nvic.h>
#include <libopencm3/usb/usbd.h>

// bootloader flash update protocol.
//
// Command format:
//
//      <opcode>[<command_data>]<EOC>
//
// Reply format:
//
//      [<reply_data>]<INSYNC><status>
//
// The <opcode> and <status> values come from the PROTO_ defines below,
// the <*_data> fields is described only for opcodes that transfer data;
// in all other cases the field is omitted.
//
// Expected workflow is:
//
// GET_SYNC		verify that the board is present
// GET_DEVICE		determine which board (select firmware to upload)
// CHIP_ERASE		erase the program area and reset address counter
// loop:
//      PROG_MULTI      program bytes
// CHIP_VERIFY		finalise flash programming and reset address counter
// loop:
//	READ_MULTI	readback bytes
// RESET		resets chip and starts application
//

#define PX4FMU	(1)
#define STM32F4DISCOVERY	(2)
#define PX4FLOW	(3)

#define PROTO_OK		0x10    // 'ok' response
#define PROTO_FAILED		0x11    // 'fail' response
#define PROTO_INSYNC		0x12    // 'in sync' byte sent before status

#define PROTO_EOC		0x20    // end of command
#define PROTO_GET_SYNC		0x21    // NOP for re-establishing sync
#define PROTO_GET_DEVICE	0x22    // get device ID bytes	<reply_data>: <board info XXX>
#define PROTO_CHIP_ERASE	0x23    // erase program area and reset program address
#define PROTO_CHIP_VERIFY	0x24    // reset program address for verification
#define PROTO_PROG_MULTI	0x27    // write bytes at address + increment	<command_data>: <count><databytes>
#define PROTO_READ_MULTI	0x28    // read bytes at address + increment	<command_data>: <count>,  <reply_data>: <databytes>

#define PROTO_REBOOT		0x30    // reboot the board & start the app

#define PROTO_DEBUG		0x31    // emit debug information - format not defined

#define PROTO_PROG_MULTI_MAX    64	// maximum PROG_MULTI size
#define PROTO_READ_MULTI_MAX    255	// size of the size field

static const clock_scale_t clock_setup =
{
	.pllm = OSC_FREQ,
	.plln = 336,
	.pllp = 2,
	.pllq = 7,
	.hpre = RCC_CFGR_HPRE_DIV_NONE,
	.ppre1 = RCC_CFGR_HPRE_DIV_4,
	.ppre2 = RCC_CFGR_HPRE_DIV_2,
	.flash_config = FLASH_ICE | FLASH_DCE | FLASH_LATENCY_5WS,
	.apb1_frequency = 42000000,
	.apb2_frequency = 84000000,
};

/* XXX interim - something that looks like a PiOS board info blob */
/* XXX should come from the build environment */
struct _board_info {
  uint32_t magic;
  uint8_t  board_type;
  uint8_t  board_rev;
  uint8_t  bl_rev;
  uint8_t  hw_type;
  uint32_t fw_base;
  uint32_t fw_size;
  uint32_t desc_base;
  uint32_t desc_size;
  uint32_t ee_base;
  uint32_t ee_size;
} __attribute__((packed)) board_info = {
	.magic		= 0xBDBDBDBD,
	.board_type	= 0x5,
	.board_rev	= 0,
	.bl_rev		= 1,
	.hw_type	= 0,
	.fw_base	= APP_LOAD_ADDRESS,
	.fw_size	= APP_SIZE_MAX,
	.desc_base	= 0,
	.desc_size	= 0,	
};

/* USB CDC interface functions in cdcacm.c */
extern void	cdc_init(void);
extern void	cdc_disconnect(void);
extern void	cdc_reconnect(void);
extern unsigned	cdc_read(uint8_t *buf, unsigned count);
extern unsigned	cdc_write(uint8_t *buf, unsigned count);

#define NTIMERS		4
#define TIMER_BL_WAIT	0
#define TIMER_CIN	1
#define TIMER_LED	2
#define TIMER_DELAY	3
static volatile unsigned timer[NTIMERS];	/* each timer decrements every millisecond if > 0 */

#if (BOARD == STM32F4DISCOVERY)
#endif

#if (BOARD == PX4FMU)
#define LED_ACTIVITY	GPIO15
#define LED_BOOTLOADER	GPIO14
#define LED_GPIOPORT	GPIOB
#define LED_GPIOCLOCK	RCC_AHB1ENR_IOPBEN
#endif

#if (BOARD == PX4FLOW)
#define LED_ACTIVITY		GPIO3
#define LED_BOOTLOADER		GPIO2
#define LED_TEST			GPIO7
#define LED_GPIOPORT		GPIOE
#define LED_GPIOCLOCK	RCC_AHB1ENR_IOPEEN
#endif


/* flash parameters that we should not really know */
uint32_t flash_sectors[] = {
	FLASH_SECTOR_1,	
	FLASH_SECTOR_2,	
	FLASH_SECTOR_3,	
	FLASH_SECTOR_4,	
	FLASH_SECTOR_5,	
	FLASH_SECTOR_6,	
	FLASH_SECTOR_7,	
	FLASH_SECTOR_8,	
	FLASH_SECTOR_9,	
	FLASH_SECTOR_10,	
	FLASH_SECTOR_11,	
};
static unsigned flash_nsectors = sizeof(flash_sectors) / sizeof(flash_sectors[0]);

/* set the boot delay when USB is attached */
#define BOOTLOADER_DELAY	1500

static void
led_on(unsigned led)
{
	gpio_clear(LED_GPIOPORT, led);
}

static void
led_off(unsigned led)
{
	gpio_set(LED_GPIOPORT, led);
}

static void
led_toggle(unsigned led)
{
	gpio_toggle(LED_GPIOPORT, led);
}

static void
do_jump(uint32_t stacktop, uint32_t entrypoint)
{
	asm volatile(
		"msr msp, %0	\n"
		"bx	%1	\n"
		: : "r" (stacktop), "r" (entrypoint) : );
	// just to keep noreturn happy
	for (;;) ;
}

static void
jump_to_app()
{
	const uint32_t *app_base = (const uint32_t *)board_info.fw_base;

	/*
	 * We refuse to program the first word of the app until the upload is marked
	 * complete by the host.  So if it's not 0xffffffff, we should try booting it.
	 */
	if (app_base[0] == 0xffffffff)
		return;

	/* just for paranoia's sake */
	flash_lock();

	/* kill the systick interrupt */
	systick_interrupt_disable();
	systick_counter_disable();

	/* and set a specific LED pattern */
	led_off(LED_ACTIVITY);
	led_on(LED_BOOTLOADER);

	/* disable USB and kill interrupts */
	cdc_disconnect();
	nvic_disable_irq(NVIC_OTG_FS_IRQ);

	/* switch exception handlers to the application */
	SCB_VTOR = board_info.fw_base;

	/* extract the stack and entrypoint from the app vector table and go */
	do_jump(app_base[0], app_base[1]);
}

void
sys_tick_handler(void)
{
	unsigned i;

	for (i = 0; i < NTIMERS; i++)
		if (timer[i] > 0)
			timer[i]--;

	if (timer[TIMER_LED] == 0) {
		led_toggle(LED_BOOTLOADER);
		timer[TIMER_LED] = 50;
	}
}

void
otg_fs_isr(void)
{
	usbd_poll();
}

void
delay(unsigned msec)
{
	timer[TIMER_DELAY] = msec;

	while(timer[TIMER_DELAY] > 0)
		;
}

static int
cin(unsigned timeout)
{
	uint8_t	c;
	int ret = -1;

	timer[TIMER_CIN] = timeout;

	do {
		/* try to fetch a byte */
		if (cdc_read(&c, 1) > 0) {
			ret = c;
			break;
		}

	} while (timer[TIMER_CIN] > 0);

	return ret;
}

static void
cout(uint8_t *buf, unsigned len)
{
	unsigned sent;

	while (len) {
		sent = cdc_write(buf, len);
		len -= sent;
		buf += sent;
	}
}

static void
sync_response(void)
{
	uint8_t data[] = {
		PROTO_INSYNC,	// "in sync"
		PROTO_OK	// "OK"
	};

	cout(data, sizeof(data));
}

volatile bool badcmd = false;

static void
bootloader(unsigned timeout)
{
	int             c;
	uint8_t         count = 0;
	unsigned	i;
	unsigned	fw_end = board_info.fw_base + board_info.fw_size;
	unsigned	address = fw_end;	/* force erase before upload will work */
	uint32_t	first_word = 0xffffffff;
	static union {
		uint8_t		c[256];
		uint32_t	w[64];
	} flash_buffer;

	/* if we are working with a timeout, start it running */
	if (timeout)
		timer[TIMER_BL_WAIT] = timeout;

	while (true) {
		// Wait for a command byte
		led_off(LED_ACTIVITY);
		do {
			/* if we have a timeout and the timer has expired, return now */
			if (timeout && !timer[TIMER_BL_WAIT])
				return;

			/* try to get a byte from the host */
			c = cin(0);

		} while (c < 0);
		led_on(LED_ACTIVITY);

		// common tests for EOC
		switch (c) {
		case PROTO_GET_SYNC:
		case PROTO_GET_DEVICE:
		case PROTO_CHIP_ERASE:
		case PROTO_CHIP_VERIFY:
		case PROTO_DEBUG:
			if (cin(100) != PROTO_EOC)
				goto cmd_bad;
		}

		// handle the command byte
		switch (c) {

		case PROTO_GET_SYNC:            // sync
			break;

		case PROTO_GET_DEVICE:		// report board info
			cout((uint8_t *)&board_info, sizeof(board_info));
			break;

		case PROTO_CHIP_ERASE:          // erase the program area + read for programming
			flash_unlock();
			for (i = 0; i < flash_nsectors; i++)
				flash_erase_sector(flash_sectors[i], FLASH_PROGRAM_X32);
			address = board_info.fw_base;
			break;

		case PROTO_CHIP_VERIFY:		// reset for verification of the program area
			address = board_info.fw_base;

			// program the deferred first word
			if (first_word != 0xffffffff)
				flash_program_word(address, first_word, FLASH_PROGRAM_X32);

			flash_lock();
			break;

		case PROTO_PROG_MULTI:		// program bytes
			count = cin(100);
			if (count % 4)
				goto cmd_bad;
			if ((address + count) > fw_end)
				goto cmd_bad;
			for (i = 0; i < count; i++)
				flash_buffer.c[i] = cin(100);
			if (cin(100) != PROTO_EOC)
				goto cmd_bad;
			if (address == board_info.fw_base) {
				// save the first word and don't program it until everything else is done
				first_word = flash_buffer.w[0];
				flash_buffer.w[0] = 0xffffffff;
			}
			for (i = 0; i < (count / 4); i++) {
				flash_program_word(address, flash_buffer.w[i], FLASH_PROGRAM_X32);
				address += 4;
			}
			break;

		case PROTO_READ_MULTI:			// readback bytes
			count = cin(100);
			if (cin(100) != PROTO_EOC)
				goto cmd_bad;
			if ((address + count) > fw_end)
				goto cmd_bad;
			cout((uint8_t *)address, count);
			address += count;
			break;

		case PROTO_REBOOT:
			// just jump to the app
			return;

		case PROTO_DEBUG:
			// XXX reserved for ad-hoc debugging as required
			break;

		default:
			continue;
		}
		// we got a command worth syncing, so kill the timeout because
		// we are probably talking to the uploader
		timeout = 0;

		// send the sync response for this command
		sync_response();
		badcmd = false;
		continue;
cmd_bad:
		// Currently we do nothing & let the programming tool time out
		// if that's what it wants to do.
		// Let the initial delay keep counting down so that we ignore
		// random chatter from a device.
		badcmd = true;
		while(true);
		continue;
	}
}

int
main(void)
{
    /* Enable FPU */
#ifndef SCB_CPACR
#define SCB_CPACR (*((uint32_t*) (((0xE000E000UL) + 0x0D00UL) + 0x088)))
#endif

    SCB_CPACR |= ((3UL << 10*2) | (3UL << 11*2)); /* set CP10 Full Access and set CP11 Full Access */

	unsigned timeout;

	/* enable GPIO9 with a pulldown to sniff VBUS */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO9);

	/* set up GPIOs for LEDs */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, LED_GPIOCLOCK);
	gpio_mode_setup(LED_GPIOPORT, GPIO_MODE_OUTPUT, 0, LED_ACTIVITY | LED_BOOTLOADER);
	gpio_set_output_options(LED_GPIOPORT, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, LED_ACTIVITY | LED_BOOTLOADER);
	led_off(LED_ACTIVITY | LED_BOOTLOADER);

	/* XXX we want a delay here to let the input settle */
	if (gpio_get(GPIOA, GPIO9) != 0) {
		/* USB is connected; first time in the bootloader we will exit after the timeout */
		timeout = BOOTLOADER_DELAY;
	} else {
		/* USB is not connected; try to boot immediately */
		jump_to_app();

		/* if we returned, there is no app; go to the bootloader and stay there */
		timeout = 0;
	}

	/* XXX we could look at the backup SRAM to check for stay-in-bootloader instructions */

	/* configure the clock for bootloader activity */
	rcc_clock_setup_hse_3v3(&clock_setup);

	/* start the timer system */
	systick_set_clocksource(STK_CTRL_CLKSOURCE_AHB);
	systick_set_reload(168000);	/* 1ms tick, magic number */
	systick_interrupt_enable();
	systick_counter_enable();

	/* setup for USB CDC */
	cdc_init();
	nvic_enable_irq(NVIC_OTG_FS_IRQ);

	while (1)
	{
		/* run the bootloader, possibly coming back after the timeout */
		bootloader(timeout);

		/* look to see if we can boot the app */
		jump_to_app();

		/* boot failed; stay in the bootloader forever next time */
		timeout = 0;
	}
}
