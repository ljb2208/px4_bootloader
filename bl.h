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

/* flash helpers from main_*.c */
extern void flash_func_erase_all(void);
extern void flash_func_write_word(unsigned address, uint32_t word);

/* board functions */
#define LED_ACTIVITY	1
#define LED_BOOTLOADER	2

extern void board_init(void);
extern void led_on(unsigned led);
extern void led_off(unsigned led);
extern void led_toggle(unsigned led);

/* interface in/output from interface module */
extern void cinit(void *config);
extern void cfini(void);
extern int cin(void);
extern void cout(uint8_t *buf, unsigned len);
