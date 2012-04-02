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

#include "bl.h"

/* standard clocking for all F4 boards */
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
unsigned flash_nsectors = sizeof(flash_sectors) / sizeof(flash_sectors[0]);

#ifdef BOARD_FMU
# define BOARD_PIN_LED_ACTIVITY		GPIO15
# define BOARD_PIN_LED_BOOTLOADER	GPIO14
# define BOARD_PORT_LEDS		GPIOB
# define BOARD_CLOCK_LEDS		RCC_AHB1ENR_IOPBEN
#endif

#ifdef BOARD_FLOW
# define BOARD_PIN_LED_ACTIVITY		GPIO3
# define BOARD_PIN_LED_BOOTLOADER	GPIO2
# define BOARD_PORT_LEDS		GPIOE
# define BOARD_CLOCK_LEDS		RCC_AHB1ENR_IOPEEN
#endif

#ifdef BOARD_DISCOVERY
# error No config for Discovery board yet
#endif

void
board_init(void)
{
	/* initialise LEDs */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, BOARD_CLOCK_LEDS);
	gpio_mode_setup(
		BOARD_PORT_LEDS, 
		GPIO_MODE_OUTPUT, 
		0,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);
	gpio_set_output_options(
		BOARD_PORT_LEDS,
		GPIO_OTYPE_OD,
		GPIO_OSPEED_2MHZ,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);
	gpio_set(
		BOARD_PORT_LEDS,
		BOARD_PIN_LED_BOOTLOADER | BOARD_PIN_LED_ACTIVITY);
}

void
led_on(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		gpio_clear(BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;
	case LED_BOOTLOADER:
		gpio_clear(BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
		break;
	}
}

void
led_off(unsigned led)
{
	switch (led) {
	case LED_ACTIVITY:
		gpio_set(BOARD_PORT_LEDS, BOARD_PIN_LED_ACTIVITY);
		break;
	case LED_BOOTLOADER:
		gpio_set(BOARD_PORT_LEDS, BOARD_PIN_LED_BOOTLOADER);
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
# define SCB_CPACR (*((uint32_t*) (((0xE000E000UL) + 0x0D00UL) + 0x088)))
#endif

int
main(void)
{
	unsigned timeout;

	/* Enable FPU */
	SCB_CPACR |= ((3UL << 10*2) | (3UL << 11*2)); /* set CP10 Full Access and set CP11 Full Access */

	/* enable GPIO9 with a pulldown to sniff VBUS */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, GPIO9);

	/* do board-specific initialisation */
	board_init();

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

	/* start the interface */
	cinit();

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
