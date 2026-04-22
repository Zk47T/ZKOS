#include "wdog.h"

#define WDOG3_BASE 0x42490000
#define WDOG3_CS  (*(volatile unsigned int *)(WDOG3_BASE + 0x00))
#define WDOG3_CNT (*(volatile unsigned int *)(WDOG3_BASE + 0x04))

// Magic numbers for unlocking the watchdog
#define WDOG_UNLOCK_KEY 0xD928C520

void wdog3_disable(void)
{
    // Write the unlock sequence to WDOG3_CNT
    WDOG3_CNT = WDOG_UNLOCK_KEY;

    // After unlocking, we have 1024 bus clocks to update WDOG_CS
    // Bit 7 is the Enable (EN) bit. Clear it to disable the watchdog.
    WDOG3_CS &= ~(1 << 7);
}