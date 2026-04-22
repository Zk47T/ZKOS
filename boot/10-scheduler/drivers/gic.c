#include "gic.h"
#include "uart.h"

/* ============================================================
 * GICv3 Distributor Registers (GICD)
 * ============================================================ */
#define GICD_CTLR       (*(volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_TYPER      (*(volatile uint32_t *)(GICD_BASE + 0x004))
#define GICD_ISENABLER(n)  (*(volatile uint32_t *)(GICD_BASE + 0x100 + (n)*4))
#define GICD_IPRIORITYR(n) (*(volatile uint8_t  *)(GICD_BASE + 0x400 + (n)))

/* GICD_CTLR bits for GICv3 */
#define GICD_CTLR_ARE_NS    (1 << 4)
#define GICD_CTLR_ENGRP1NS  (1 << 1)

/* ============================================================
 * GICv3 Redistributor Registers (GICR)
 * ============================================================ */
#define GICR_WAKER         (*(volatile uint32_t *)(GICR_BASE + 0x014))
#define GICR_WAKER_PS      (1 << 1)   // ProcessorSleep
#define GICR_WAKER_CA      (1 << 2)   // ChildrenAsleep

/* SGI_base frame for PPIs/SGIs (INTID 0-31) */
#define GICR_IGROUPR0      (*(volatile uint32_t *)(GICR_SGI_BASE + 0x080))
#define GICR_ISENABLER0    (*(volatile uint32_t *)(GICR_SGI_BASE + 0x100))
#define GICR_ICENABLER0    (*(volatile uint32_t *)(GICR_SGI_BASE + 0x180))
#define GICR_IPRIORITYR(n) (*(volatile uint8_t  *)(GICR_SGI_BASE + 0x400 + (n)))
#define GICR_IGRPMODR0     (*(volatile uint32_t *)(GICR_SGI_BASE + 0xD00))

/* ============================================================
 * ICC System Register Access Macros (CPU Interface)
 * ============================================================ */
#define READ_SYSREG(reg) ({ \
    uint64_t _v; \
    asm volatile("mrs %0, " #reg : "=r"(_v)); \
    _v; \
})

#define WRITE_SYSREG(reg, val) do { \
    uint64_t _v = (val); \
    asm volatile("msr " #reg ", %0" :: "r"(_v)); \
} while(0)

/* ============================================================
 * GIC Initialization
 * ============================================================ */

void gic_init(void)
{
    uart_puts("[GIC] Initializing GICv3...\r\n");

    /* 1. Enable System Register interface at EL2 */
    WRITE_SYSREG(S3_4_C12_C9_5, 0x7);  // ICC_SRE_EL2: SRE=1, DFB=1, DIB=1
    asm volatile("isb");

    /* 2. Distributor: Enable ARE and Group 1 NS */
    GICD_CTLR = GICD_CTLR_ARE_NS | GICD_CTLR_ENGRP1NS;

    /* 3. Redistributor: Wake up PE */
    uint32_t waker = GICR_WAKER;
    waker &= ~GICR_WAKER_PS;  // Clear ProcessorSleep
    GICR_WAKER = waker;

    // Wait until ChildrenAsleep clears
    while (GICR_WAKER & GICR_WAKER_CA) {
        // spin
    }

    /* 4. CPU Interface: Set priority mask to accept all */
    WRITE_SYSREG(S3_0_C4_C6_0, 0xFF);  // ICC_PMR_EL1 = 0xFF

    /* 5. CPU Interface: Enable Group 1 interrupts */
    WRITE_SYSREG(S3_0_C12_C12_7, 1);   // ICC_IGRPEN1_EL1 = 1

    asm volatile("isb");

    uart_puts("[GIC] GICv3 initialized\r\n");
}

/* ============================================================
 * Enable a specific interrupt
 * ============================================================ */
void gic_enable_irq(uint32_t irq_id)
{
    if (irq_id < 32) {
        /* SGI/PPI: handled by redistributor */
        /* Set as Group 1 NS */
        GICR_IGROUPR0 |= (1 << irq_id);
        GICR_IGRPMODR0 &= ~(1 << irq_id);

        /* Set priority (lower = higher priority) */
        GICR_IPRIORITYR(irq_id) = 0xA0;

        /* Enable */
        GICR_ISENABLER0 = (1 << irq_id);
    } else {
        /* SPI: handled by distributor */
        uint32_t reg = irq_id / 32;
        uint32_t bit = irq_id % 32;
        GICD_ISENABLER(reg) = (1 << bit);
        GICD_IPRIORITYR(irq_id) = 0xA0;
    }
}

/* ============================================================
 * Acknowledge interrupt (read INTID)
 * ============================================================ */
uint32_t gic_ack_irq(void)
{
    return (uint32_t)READ_SYSREG(S3_0_C12_C12_0);  // ICC_IAR1_EL1
}

/* ============================================================
 * Signal end of interrupt
 * ============================================================ */
void gic_end_irq(uint32_t irq_id)
{
    WRITE_SYSREG(S3_0_C12_C12_1, irq_id);  // ICC_EOIR1_EL1
}
