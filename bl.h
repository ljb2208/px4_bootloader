/*
 * Common bootloader definitions.
 */

#pragma once

/* bootloader functions */
extern void jump_to_app(void);
extern void sys_tick_handler(void);
extern void bootloader(unsigned timeout);

/* generic timers */
#define NTIMERS		4
#define TIMER_BL_WAIT	0
#define TIMER_CIN	1
#define TIMER_LED	2
#define TIMER_DELAY	3
extern volatile unsigned timer[NTIMERS];	/* each timer decrements every millisecond if > 0 */

/* receive buffer for async reads */
extern void buf_put(uint8_t b);
extern int buf_get(void);

/* flash geometry from main_*.c */
extern uint32_t flash_sectors[];
extern unsigned flash_nsectors;

/* board functions */
#define LED_ACTIVITY	1
#define LED_BOOTLOADER	2

extern void board_init(void);
extern void led_on(unsigned led);
extern void led_off(unsigned led);
extern void led_toggle(unsigned led);

/* LED configuration from main_*.c */
typedef struct {
	uint32_t	pin_activity;
	uint32_t	pin_bootloader;
	uint32_t	gpio_port;
	uint32_t	gpio_clock;
} led_info_t;
extern led_info_t led_info;

/* interface in/output from interface module */
extern void cinit(void);
extern void cfini(void);
extern int cin(void);
extern void cout(uint8_t *buf, unsigned len);
