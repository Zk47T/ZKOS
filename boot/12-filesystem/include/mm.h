#ifndef MM_H
#define MM_H

#include "types.h"

/* ============================================================
 * Memory Layout Constants
 * ============================================================ */
#define PAGE_SIZE           4096
#define PAGE_SHIFT          12

#define PAGE_TABLE_BASE     0x80200000UL  // Page tables location
#define HEAP_BASE           0x80600000UL  // Heap start
#define HEAP_SIZE           (4 * 1024 * 1024)  // 4MB heap
#define MAX_PAGES           (HEAP_SIZE / PAGE_SIZE)  // 1024 pages

/* ============================================================
 * MMU Functions
 * ============================================================ */
void mmu_init(void);

/* ============================================================
 * Page Allocator
 * ============================================================ */
void page_alloc_init(void);
void *page_alloc(void);
void page_free(void *addr);
uint32_t page_get_free_count(void);
uint32_t page_get_used_count(void);

/* ============================================================
 * Kernel Heap Allocator
 * ============================================================ */
void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t heap_get_used(void);

#endif /* MM_H */
