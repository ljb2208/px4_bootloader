/*
 * STM32F4 board support for the bootloader.
 *
 */

#include <stdlib.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/stm32/f4/flash.h>
#include <libopencm3/stm32/usart.h>


#include "bl.h"

/* flash parameters that we should not really know */
static uint32_t flash_sectors[] = {
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

#ifdef BOARD_FMU
# define OSC_FREQ			24

# define BOARD_PIN_LED_ACTIVITY		GPIO15
# define BOARD_PIN_LED_BOOTLOADER	GPIO14
# define BOARD_PORT_LEDS		GPIOB
# define BOARD_CLOCK_LEDS		RCC_AHB1ENR_IOPBEN
# define BOARD_LED_ON			gpio_clear
# define BOARD_LED_OFF			gpio_set

# define BOARD_USART			USART1
# define BOARD_PORT_USART		PORTB
# define BOARD_USART_CLOCK_REGISTER	RCC_APB2ENR
# define BOARD_USART_CLOCK_BIT		RCC_APB2ENR_USART1EN
# define BOARD_PIN_TX			GPIO6
# define BOARD_PIN_RX			GPIO7
# define BOARD_CLOCK_USART_PINS		RCC_AHB1ENR_IOPBEN
# define BOARD_FUNC_USART		GPIO_AF7
#endif

#ifdef BOARD_FLOW
# define OSC_FREQ			24

# define BOARD_PIN_LED_ACTIVITY		GPIO3
# define BOARD_PIN_LED_BOOTLOADER	GPIO2
# define BOARD_PORT_LEDS		GPIOE
# define BOARD_CLOCK_LEDS		RCC_AHB1ENR_IOPEEN
# define BOARD_LED_ON			gpio_clear
# define BOARD_LED_OFF			gpio_set

# define BOARD_USART_CLOCK_REGISTER	RCC_APB1ENR
# define BOARD_USART_CLOCK_BIT		RCC_APB1ENR_USART2EN
# define BOARD_USART			USART2
# define BOARD_PORT_USART		PORTD
# define BOARD_PIN_TX			GPIO5
# define BOARD_PIN_RX			GPIO6
# define BOARD_CLOCK_USART_PINS		RCC_AHB1ENR_IOPDEN
# define BOARD_FUNC_USART		GPIO_AF7
#endif

#ifdef BOARD_DISCOVERY
# define OSC_FREQ			8

# define BOARD_PIN_LED_ACTIVITY		GPIO12
# define BOARD_PIN_LED_BOOTLOADER	GPIO13
# define BOARD_PORT_LEDS		GPIOD
# define BOARD_CLOCK_LEDS		RCC_AHB1ENR_IOPDEN
# define BOARD_LED_ON			gpio_set
# define BOARD_LED_OFF			gpio_clear

# define BOARD_USART			USART2
# define BOARD_PORT_USART		GPIOA
# define BOARD_USART_CLOCK_REGISTER	RCC_APB1ENR
# define BOARD_USART_CLOCK_BIT		RCC_APB1ENR_USART2EN
# define BOARD_PIN_TX			GPIO2
# define BOARD_PIN_RX			GPIO3
# define BOARD_USART_PIN_CLOCK_REGISTER	RCC_APB2ENR
# define BOARD_USART_PIN_CLOCK_BIT	RCC_AHB1ENR_IOPAEN
# define BOARD_FUNC_USART		GPIO_AF7
#endif

#ifdef INTERFACE_USART
# define BOARD_INTERFACE_CONFIG		(void *)BOARD_USART
#else
# define BOARD_INTERFACE_CONFIG		NULL
#endif

/* standard clocking for all F4 boards */

static const clock_scale_t clock_setup =
{
	.pllm = OSC_FREQ,
	.plln = 336,
	.pllp = 2,
	.pllq = 7,
	.hpre = RCC_CFGR_HPRE_DIV_NONE,
	.ppre1 = RCC_CFGR_PPRE_DIV_4,
	.ppre2 = RCC_CFGR_PPRE_DIV_2,
	.power_save = 0,
	.flash_config = FLASH_ICE | FLASH_DCE | FLASH_LATENCY_5WS,
	.apb1_frequency = 42000000,
	.apb2_frequency = 84000000,
};

void
board_init(void)
{

	/* initialise LEDs */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, BOARD_CLOCK_LEDS);
	gpio_mode_setup(
		BOARD_PORT_LEDS, 
		GPIO_MODE_OUTPUT, 
		GPIO_PUPD_NONE,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);
	gpio_set_output_options(
		BOARD_PORT_LEDS,
		GPIO_OTYPE_PP,
		GPIO_OSPEED_2MHZ,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);
	BOARD_LED_ON (
		BOARD_PORT_LEDS,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);

#ifdef INTERFACE_USART
	/* configure usart pins */
	rcc_peripheral_enable_clock(&BOARD_USART_PIN_CLOCK_REGISTER, BOARD_USART_PIN_CLOCK_BIT);
	gpio_mode_setup(BOARD_PORT_USART, GPIO_MODE_AF, GPIO_PUPD_NONE, BOARD_PIN_TX | BOARD_PIN_RX);
	gpio_set_af(BOARD_PORT_USART, BOARD_FUNC_USART, BOARD_PIN_TX | BOARD_PIN_RX);

	/* configure USART clock */
	rcc_peripheral_enable_clock(&BOARD_USART_CLOCK_REGISTER, BOARD_USART_CLOCK_BIT);
#endif
}

void
flash_func_erase_all(void)
{
	unsigned i;

	/* erase all but the sector containing the bootloader */
	for (i = 0; i < flash_nsectors; i++)
		flash_erase_sector(flash_sectors[i], FLASH_PROGRAM_X32);
}

void
flash_func_write_word(unsigned address, uint32_t word)
{
	flash_program_word(address, word, FLASH_PROGRAM_X32);
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

/* we should know this, but we don't */
#ifndef SCB_CPACR
# define SCB_CPACR (*((volatile uint32_t *) (((0xE000E000UL) + 0x0D00UL) + 0x088)))
#endif

int
main(void)
{
	unsigned timeout = 0;

	/* Enable the FPU before we hit any FP instructions */
	SCB_CPACR |= ((3UL << 10*2) | (3UL << 11*2)); /* set CP10 Full Access and set CP11 Full Access */

#ifdef INTERFACE_USB
	/* enable GPIO9 with a pulldown to sniff VBUS */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO9);
#endif

	/* do board-specific initialisation */
	board_init();

#ifdef INTERFACE_USB
	/* check for USB connection - if present, we will wait in the bootloader for a while */
	if (gpio_get(GPIOA, GPIO9) != 0)
	{
		timeout = BOOTLOADER_DELAY;
	}
	else
	{
		/* XXX this needs a proper fix, state machine below is redundant
		 * code as is works correctly and fine, but is ugly
		 */
		/* look to see if we can boot the app */
		jump_to_app();

		/* boot failed; stay in the bootloader forever next time */
		timeout = 0;
	}
#endif
#ifdef INTERFACE_USART
	/* XXX sniff for a USART connection to decide whether to wait in the bootloader */
	timeout = 0;
#endif

	/* XXX we could look at the backup SRAM to check for stay-in-bootloader instructions */
#if 0
	/* if we aren't expected to wait in the bootloader, try to boot immediately */
	if (timeout == 0) {
		/* try to boot immediately */
		jump_to_app();

		/* if we returned, there is no app; go to the bootloader and stay there */
		timeout = 0;
	}
#endif
	/* configure the clock for bootloader activity */
	rcc_clock_setup_hse_3v3(&clock_setup);
#if 0
	// MCO1/02
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, GPIO8);
	gpio_set_af(GPIOA, GPIO_AF0, GPIO8);
	gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
	gpio_set_af(GPIOC, GPIO_AF0, GPIO9);
#endif
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
