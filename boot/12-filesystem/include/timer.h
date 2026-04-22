#ifndef TIMER_H
#define TIMER_H

#include "types.h"

unsigned long get_timer_value(void);
unsigned long get_timer_frequency(void);
void sleep_ms(unsigned long ms);

/* Bài 7: Timer IRQ support */
void timer_init_irq(uint32_t interval_ms);
void timer_irq_handler(void);
uint64_t get_tick_count(void);

#endif
