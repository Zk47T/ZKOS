#ifndef GIC_H
#define GIC_H

#include "types.h"

/* ============================================================
 * GICv3 Base Addresses (i.MX93)
 * ============================================================ */
#define GICD_BASE       0x48000000UL    // Distributor
#define GICR_BASE       0x48040000UL    // Redistributor (RD_base)
#define GICR_SGI_BASE   (GICR_BASE + 0x10000UL)  // SGI_base frame

/* ============================================================
 * Timer Interrupt ID
 * ============================================================ */
#define TIMER_IRQ_ID    26  // EL2 Physical Timer PPI (INTID 26)

/* ============================================================
 * API Functions
 * ============================================================ */

void gic_init(void);
void gic_enable_irq(uint32_t irq_id);
uint32_t gic_ack_irq(void);
void gic_end_irq(uint32_t irq_id);

#endif /* GIC_H */
