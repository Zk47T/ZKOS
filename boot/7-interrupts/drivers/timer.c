#include "timer.h"
#include "uart.h"

#define READ_SYSREG(reg_name) \
    ({ \
        unsigned long _val; \
        asm volatile("mrs %0, " #reg_name : "=r" (_val)); \
        _val; \
    })

#define WRITE_SYSREG(reg_name, val) do { \
    unsigned long _v = (val); \
    asm volatile("msr " #reg_name ", %0" :: "r" (_v)); \
} while(0)

/* ============================================================
 * Basic Timer (from Lesson 5)
 * ============================================================ */

unsigned long get_timer_value(void)
{
    return READ_SYSREG(cntpct_el0);
}

unsigned long get_timer_frequency(void)
{
    return READ_SYSREG(cntfrq_el0);
}

void sleep_ms(unsigned long ms)
{
    unsigned long ticks_per_ms = get_timer_frequency() / 1000;
    unsigned long start = get_timer_value();
    
    while ((get_timer_value() - start) < (ms * ticks_per_ms)) {
    }
}

/* ============================================================
 * Timer IRQ (Lesson 7)
 * Uses EL2 Physical Timer (CNTHP)
 * ============================================================ */

static volatile uint64_t tick_count = 0;
static uint64_t timer_interval_ticks = 0;

void timer_init_irq(uint32_t interval_ms)
{
    uint64_t freq = get_timer_frequency();
    timer_interval_ticks = (freq * interval_ms) / 1000;

    uart_puts("[TIMER] Freq: ");
    uart_putint(freq);
    uart_puts(" Hz, interval: ");
    uart_putint(interval_ms);
    uart_puts(" ms\r\n");

    /* Set timer value (countdown) */
    WRITE_SYSREG(cnthp_tval_el2, timer_interval_ticks);

    /* Enable timer, unmask interrupt:
     * Bit 0: ENABLE = 1
     * Bit 1: IMASK  = 0 (not masked)
     */
    WRITE_SYSREG(cnthp_ctl_el2, 1);

    uart_puts("[TIMER] EL2 Physical Timer armed\r\n");
}

void timer_irq_handler(void)
{
    tick_count++;

    /* Re-arm timer for next interval */
    WRITE_SYSREG(cnthp_tval_el2, timer_interval_ticks);
}

uint64_t get_tick_count(void)
{
    return tick_count;
}
