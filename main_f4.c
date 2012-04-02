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
