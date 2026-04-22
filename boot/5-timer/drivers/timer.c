#include "timer.h"

// Define a macro to read ARMv8 System Registers
// This hides the inline assembly making the C code cleaner.
#define READ_SYSREG(reg_name) \
    ({ \
        unsigned long _val; \
        asm volatile("mrs %0, " #reg_name : "=r" (_val)); \
        _val; \
    })

unsigned long get_timer_value(void)
{
    // Read the Generic Timer Counter Register (CNTPCT_EL0)
    return READ_SYSREG(cntpct_el0);
}

unsigned long get_timer_frequency(void)
{
    // Read the Generic Timer Frequency Register (CNTFRQ_EL0)
    return READ_SYSREG(cntfrq_el0);
}

void sleep_ms(unsigned long ms)
{
    unsigned long ticks_per_ms = get_timer_frequency() / 1000;
    unsigned long start = get_timer_value();
    
    // Spin while the elapsed ticks are less than the required ticks
    while ((get_timer_value() - start) < (ms * ticks_per_ms)) {
        // Do nothing, wait
    }
}

