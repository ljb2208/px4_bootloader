/*
 * STM32F1 board support for the bootloader.
 *
 */

#include <stdlib.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/flash.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/systick.h>

#include "bl.h"

/*
 * Yes, the nonsense required to configure GPIOs with this
 * library is truly insane...
 */

#if defined(BOARD_IO)
# define OSC_FREQ			24

# define BOARD_PIN_LED_ACTIVITY		GPIO14
# define BOARD_PIN_LED_BOOTLOADER	GPIO15
# define BOARD_PORT_LEDS		GPIOB
# define BOARD_CLOCK_LEDS_REGISTER	RCC_APB2ENR
# define BOARD_CLOCK_LEDS		RCC_APB2ENR_IOPBEN
# define BOARD_LED_ON			gpio_clear
# define BOARD_LED_OFF			gpio_set

# define BOARD_USART			USART2
# define BOARD_USART_CLOCK_REGISTER	RCC_APB1ENR
# define BOARD_USART_CLOCK_BIT		RCC_APB1ENR_USART2EN

# define BOARD_PORT_USART		GPIOA
# define BOARD_PIN_TX			GPIO_USART2_TX
# define BOARD_PIN_RX			GPIO_USART2_RX
# define BOARD_USART_PIN_CLOCK_REGISTER	RCC_APB2ENR
# define BOARD_USART_PIN_CLOCK_BIT	RCC_APB2ENR_IOPAEN

# define BOARD_FORCE_BL_PIN		GPIO5
# define BOARD_FORCE_BL_PORT		GPIOB
# define BOARD_FORCE_BL_CLOCK_REGISTER	RCC_APB2ENR
# define BOARD_FORCE_BL_CLOCK_BIT	RCC_APB2ENR_IOPBEN
# define BOARD_FORCE_BL_VALUE		BOARD_FORCE_BL_PIN

# define BOARD_FLASH_PAGES		64
# else
# error Unrecognised BOARD definition
#endif

#ifdef INTERFACE_USART
# define BOARD_INTERFACE_CONFIG		(void *)BOARD_USART
#else
# define BOARD_INTERFACE_CONFIG		NULL
#endif

#define FLASH_PAGESIZE			0x1000
#define FLASH_BASE			0x08000000

/* board definition */
struct boardinfo board_info = {
	.board_type	= 10,
	.board_rev	= 0,
	.fw_base	= APP_LOAD_ADDRESS,
	.fw_size	= APP_SIZE_MAX,

	.systick_mhz	= OSC_FREQ,
};

static void board_init(void);

static void
board_init(void)
{
	/* run at a sane speed supported by all F1xx devices */
	rcc_clock_setup_in_hsi_out_24mhz();

	/* initialise LEDs */
	rcc_peripheral_enable_clock(&BOARD_CLOCK_LEDS_REGISTER, BOARD_CLOCK_LEDS);
	gpio_set_mode(BOARD_PORT_LEDS,
		GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);
	BOARD_LED_ON (
		BOARD_PORT_LEDS,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);

	/* if we have one, enable the force-bootloader pin */
#ifdef BOARD_FORCE_BL_PIN
	rcc_peripheral_enable_clock(&BOARD_FORCE_BL_CLOCK_REGISTER, BOARD_FORCE_BL_CLOCK_BIT);
	gpio_set_mode(BOARD_FORCE_BL_PORT,
		GPIO_MODE_INPUT,
		GPIO_CNF_INPUT_FLOAT,	/* depend on external pull */
		BOARD_FORCE_BL_PIN);
#endif

#ifdef INTERFACE_USART
	/* configure usart pins */
	rcc_peripheral_enable_clock(&BOARD_USART_PIN_CLOCK_REGISTER, BOARD_USART_PIN_CLOCK_BIT);
	gpio_set_mode(BOARD_PORT_USART, 
		      GPIO_MODE_OUTPUT_50_MHZ,
                      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
		      BOARD_PIN_TX);

	/* configure USART clock */
	rcc_peripheral_enable_clock(&BOARD_USART_CLOCK_REGISTER, BOARD_USART_CLOCK_BIT);
#endif
#ifdef INTERFACE_I2C
# error I2C GPIO config not handled yet
#endif

}

void
flash_func_erase_all(void)
{
	unsigned i;	

	/* erase all but the sector containing the bootloader */
	for (i = 0; i < BOARD_FLASH_PAGES; i++)
		flash_erase_page(FLASH_BASE + (i * FLASH_PAGESIZE));
}

void
flash_func_write_word(unsigned address, uint32_t word)
{
	flash_program_word(address, word);
}

void
led_on(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		BOARD_LED_ON (BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;
	case LED_BOOTLOADER:
		BOARD_LED_ON (BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
		break;
	}
}

void
led_off(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		BOARD_LED_OFF (BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;
	case LED_BOOTLOADER:
		BOARD_LED_OFF (BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
		break;
	}
}

void
led_toggle(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		gpio_toggle(BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;
	case LED_BOOTLOADER:
		gpio_toggle(BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
		break;
	}
}

int
main(void)
{
	unsigned timeout = 0;

	/* do board-specific initialisation */
	board_init();

#ifdef INTERFACE_USART
	/* XXX sniff for a USART connection to decide whether to wait in the bootloader? */
	timeout = BOOTLOADER_DELAY;
#endif

#ifdef INTERFACE_I2C
# error I2C bootloader detection logic not implemented
#endif

#ifdef BOARD_FORCE_BL_PIN
	/* if the force-BL pin state matches the state of the pin, wait in the bootloader forever */
	if (BOARD_FORCE_BL_VALUE == gpio_get(BOARD_FORCE_BL_PORT, BOARD_FORCE_BL_PIN))
		timeout = 0xffffffff;
#endif

	/* XXX we could look at the backup SRAM to check for stay-in-bootloader instructions */

	/* if we aren't expected to wait in the bootloader, try to boot immediately */
	if (timeout == 0) {
		/* try to boot immediately */
		jump_to_app();

		/* if we returned, there is no app; go to the bootloader and stay there */
		timeout = 0;
	}

	/* configure the clock for bootloader activity */
	/* XXX for now, don't touch the clock config */

	/* start the interface */
	cinit(BOARD_INTERFACE_CONFIG);

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
