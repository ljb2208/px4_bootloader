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

/* Board-specific LED configuration */
led_info_t led_info = {
#ifdef BOARD_FMU
	.pin_activity	= GPIO15,
	.pin_bootloader	= GPIO14,
	.gpio_port	= GPIOB,
	.gpio_clock	= RCC_AHB1ENR_IOPBEN
#endif
#ifdef BOARD_FLOW
	.pin_activity	= GPIO3,
	.pin_bootloader	= GPIO2,
	.gpio_port	= GPIOE,
	.gpio_clock	= RCC_AHB1ENR_IOPEEN
#endif
#ifdef BOARD_DISCOVERY
# error No LED configuration for Discovery board - see the schematic
#endif
};

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

	/* set up GPIOs for LEDs */
	rcc_peripheral_enable_clock(&RCC_AHB1ENR, led_info.gpio_clock);
	gpio_mode_setup(led_info.gpio_port, GPIO_MODE_OUTPUT, 0, LED_ACTIVITY | LED_BOOTLOADER);
	gpio_set_output_options(
		led_info.gpio_port, 
		GPIO_OTYPE_OD,
		GPIO_OSPEED_2MHZ,
		led_info.pin_activity | led_info.pin_bootloader);
	gpio_set(led_info.gpio_port, led_info.pin_activity | led_info.pin_bootloader);

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
