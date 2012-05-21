/*
 * STM32F1 board support for the bootloader.
 *
 */

#include <stdlib.h>
#include <libopencm3/stm32/f1/rcc.h>
#include <libopencm3/stm32/f1/gpio.h>
#include <libopencm3/stm32/f1/flash.h>
#include <libopencm3/stm32/usart.h>

#include "bl.h"

#if defined(BOARD_IO)
# define OSC_FREQ			24

# define BOARD_PIN_LED_ACTIVITY		GPIO15
# define BOARD_PIN_LED_BOOTLOADER	GPIO14
# define BOARD_PORT_LEDS		GPIOB
# define BOARD_CLOCK_LEDS_REGISTER	RCC_APB2ENR
# define BOARD_CLOCK_LEDS		RCC_APB2ENR_IOPBEN
# define BOARD_LED_ON			gpio_clear
# define BOARD_LED_OFF			gpio_set

# define BOARD_USART			USART1
# define BOARD_PORT_USART		GPIOA
# define BOARD_USART_CLOCK_REGISTER	RCC_APB2ENR
# define BOARD_USART_CLOCK_BIT		RCC_APB2ENR_USART1EN
# define BOARD_PIN_TX			GPIO_USART1_TX
# define BOARD_USART_PIN_CLOCK_REGISTER	RCC_APB2ENR
# define BOARD_USART_PIN_CLOCK_BIT	RCC_APB2ENR_IOPAEN

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

void
board_init(void)
{
#warning LED config not set up yet

#if 0
	/* initialise LEDs */
	rcc_peripheral_enable_clock(&BOARD_CLOCK_LEDS_REGISTER, BOARD_CLOCK_LEDS);
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
	/* XXX sniff for a USART connection to decide whether to wait in the bootloader */
	timeout = 0;
#endif

#ifdef INTERFACE_I2C
# error I2C bootloader detection logic not implemented
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
