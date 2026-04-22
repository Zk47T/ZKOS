#include "mm.h"
#include "uart.h"
#include "string.h"

/* ============================================================
 * MMU - Identity Mapping with 1GB Block Descriptors
 * ============================================================
 *
 * AArch64 4KB Granule Page Table Structure:
 *   Level 0 (PGD): each entry covers 512GB
 *   Level 1 (PUD): each entry covers 1GB → using block descriptors here
 *
 * Memory Attributes (MAIR_EL2):
 *   Index 0: Device-nGnRnE  (0x00) - for MMIO peripherals
 *   Index 1: Normal WB Cacheable (0xFF) - for DDR
 */

/* Page table arrays - placed at fixed location */
static uint64_t *l0_table = (uint64_t *)PAGE_TABLE_BASE;
static uint64_t *l1_table = (uint64_t *)(PAGE_TABLE_BASE + PAGE_SIZE);

/* Descriptor bits */
#define DESC_VALID      (1UL << 0)
#define DESC_TABLE      (1UL << 1)  // For table descriptor (L0)
#define DESC_BLOCK      (0UL << 1)  // For block descriptor (L1)
#define DESC_AF         (1UL << 10) // Access Flag
#define DESC_MAIR_IDX(n) ((uint64_t)(n) << 2) // AttrIndx[2:0]

/* Block descriptor for Device memory (MAIR index 0) */
#define BLOCK_DEVICE    (DESC_VALID | DESC_BLOCK | DESC_AF | DESC_MAIR_IDX(0))
/* Block descriptor for Normal memory (MAIR index 1) */
#define BLOCK_NORMAL    (DESC_VALID | DESC_BLOCK | DESC_AF | DESC_MAIR_IDX(1))

void mmu_init(void)
{
    uart_puts("[MMU] Initializing page tables...\r\n");

    /* Clear page tables */
    memset(l0_table, 0, PAGE_SIZE);
    memset(l1_table, 0, PAGE_SIZE);

    /* L0[0] → L1 table (covers 0x00000000 - 0x7FFFFFFFFF) */
    l0_table[0] = (uint64_t)l1_table | DESC_VALID | DESC_TABLE;

    /* L1 entries (1GB block descriptors):
     * L1[0]: 0x00000000 - 0x3FFFFFFF → Device (SoC peripherals low)
     * L1[1]: 0x40000000 - 0x7FFFFFFF → Device (UART, GIC, etc.)
     * L1[2]: 0x80000000 - 0xBFFFFFFF → Normal (DDR first 1GB)
     */

    /* Need a second L0 entry for the top 1GB of DDR */
    /* Actually, 0x80000000 is in the first 512GB, so L0[0] covers it */
    l1_table[0] = 0x00000000UL | BLOCK_DEVICE;  // SoC low peripherals
    l1_table[1] = 0x40000000UL | BLOCK_DEVICE;  // UART, GIC, WDOG
    l1_table[2] = 0x80000000UL | BLOCK_NORMAL;  // DDR (our code + data)
    l1_table[3] = 0xC0000000UL | BLOCK_NORMAL;  // DDR upper

    uart_puts("[MMU] Page tables built:\r\n");
    uart_puts("  0x00000000 - 0x3FFFFFFF: Device\r\n");
    uart_puts("  0x40000000 - 0x7FFFFFFF: Device (UART/GIC)\r\n");
    uart_puts("  0x80000000 - 0xBFFFFFFF: Normal (DDR)\r\n");
    uart_puts("  0xC0000000 - 0xFFFFFFFF: Normal (DDR)\r\n");

    /* Configure MAIR_EL2 */
    uint64_t mair = (0x00UL << 0) |  // Attr0: Device-nGnRnE
                    (0xFFUL << 8);    // Attr1: Normal WB Cacheable
    asm volatile("msr mair_el2, %0" :: "r"(mair));

    /* Configure TCR_EL2:
     * T0SZ  = 25 (39-bit VA space, enough for 512GB)
     * IRGN0 = 01 (Normal WB RA WA)
     * ORGN0 = 01 (Normal WB RA WA)
     * SH0   = 11 (Inner Shareable)
     * TG0   = 00 (4KB granule)
     * PS    = 001 (36-bit PA for 4GB)
     */
    uint64_t tcr = (25UL << 0)  |  // T0SZ = 25
                   (0x1UL << 8) |  // IRGN0 = WB RA WA
                   (0x1UL << 10)|  // ORGN0 = WB RA WA
                   (0x3UL << 12)|  // SH0 = Inner Shareable
                   (0x0UL << 14)|  // TG0 = 4KB
                   (0x1UL << 16);  // PS = 36-bit PA
    asm volatile("msr tcr_el2, %0" :: "r"(tcr));

    /* Set TTBR0_EL2 to L0 table */
    asm volatile("msr ttbr0_el2, %0" :: "r"((uint64_t)l0_table));

    /* Ensure all writes complete before enabling MMU */
    asm volatile("dsb sy");
    asm volatile("isb");

    /* Enable MMU: set SCTLR_EL2.M bit, also C (cache) and I (icache) */
    uint64_t sctlr;
    asm volatile("mrs %0, sctlr_el2" : "=r"(sctlr));
    sctlr |= (1 << 0);   // M bit - MMU enable
    sctlr |= (1 << 2);   // C bit - Data cache enable
    sctlr |= (1 << 12);  // I bit - Instruction cache enable
    asm volatile("msr sctlr_el2, %0" :: "r"(sctlr));

    asm volatile("isb");

    uart_puts("[MMU] MMU enabled with identity mapping\r\n");
}

/* ============================================================
 * Bitmap Page Allocator
 * ============================================================ */

/* Bitmap: 1 bit per page, 1 = used, 0 = free */
static uint64_t page_bitmap[MAX_PAGES / 64];
static uint32_t pages_used = 0;

void page_alloc_init(void)
{
    memset(page_bitmap, 0, sizeof(page_bitmap));
    pages_used = 0;
    uart_puts("[MM] Page allocator: ");
    uart_putint(MAX_PAGES);
    uart_puts(" pages (");
    uart_putint(MAX_PAGES * PAGE_SIZE / 1024);
    uart_puts(" KB) available\r\n");
}

void *page_alloc(void)
{
    for (uint32_t i = 0; i < MAX_PAGES / 64; i++) {
        if (page_bitmap[i] != ~0UL) {
            for (int bit = 0; bit < 64; bit++) {
                if (!(page_bitmap[i] & (1UL << bit))) {
                    page_bitmap[i] |= (1UL << bit);
                    pages_used++;
                    uint32_t page_idx = i * 64 + bit;
                    return (void *)(HEAP_BASE + page_idx * PAGE_SIZE);
                }
            }
        }
    }
    return NULL;  // Out of memory
}

void page_free(void *addr)
{
    uint64_t a = (uint64_t)addr;
    if (a < HEAP_BASE || a >= HEAP_BASE + HEAP_SIZE) return;

    uint32_t page_idx = (a - HEAP_BASE) / PAGE_SIZE;
    uint32_t word = page_idx / 64;
    uint32_t bit = page_idx % 64;

    if (page_bitmap[word] & (1UL << bit)) {
        page_bitmap[word] &= ~(1UL << bit);
        pages_used--;
    }
}

uint32_t page_get_free_count(void) { return MAX_PAGES - pages_used; }
uint32_t page_get_used_count(void) { return pages_used; }

/* ============================================================
 * Simple Kernel Heap (Bump Allocator with Free List)
 * ============================================================ */

#define HEAP_POOL_SIZE  (64 * 1024)  // 64KB kernel heap pool
static uint8_t heap_pool[HEAP_POOL_SIZE];
static size_t heap_offset = 0;
static size_t heap_used_bytes = 0;

struct free_block {
    struct free_block *next;
    size_t size;
};
static struct free_block *free_list = NULL;

void heap_init(void)
{
    heap_offset = 0;
    heap_used_bytes = 0;
    free_list = NULL;
    uart_puts("[MM] Kernel heap: ");
    uart_putint(HEAP_POOL_SIZE / 1024);
    uart_puts(" KB\r\n");
}

void *kmalloc(size_t size)
{
    /* Align size to 16 bytes */
    size = (size + 15) & ~15UL;

    /* Check free list first */
    struct free_block **prev = &free_list;
    struct free_block *curr = free_list;
    while (curr) {
        if (curr->size >= size) {
            *prev = curr->next;
            heap_used_bytes += curr->size;
            return (void *)curr;
        }
        prev = &curr->next;
        curr = curr->next;
    }

    /* Bump allocate */
    if (heap_offset + size + sizeof(size_t) > HEAP_POOL_SIZE) {
        return NULL;
    }

    /* Store size before the allocation for kfree */
    *(size_t *)&heap_pool[heap_offset] = size;
    void *ptr = &heap_pool[heap_offset + sizeof(size_t)];
    heap_offset += size + sizeof(size_t);
    heap_used_bytes += size;

    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr) return;

    /* Retrieve size stored before the pointer */
    size_t *size_ptr = (size_t *)((uint8_t *)ptr - sizeof(size_t));
    size_t size = *size_ptr;

    heap_used_bytes -= size;

    /* Add to free list */
    struct free_block *blk = (struct free_block *)ptr;
    blk->size = size;
    blk->next = free_list;
    free_list = blk;
}

size_t heap_get_used(void)
{
    return heap_used_bytes;
}
