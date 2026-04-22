# Bài 8 — Memory Management: MMU + Phân bổ bộ nhớ

Ở các bài trước, ZKOS truy cập bộ nhớ trực tiếp bằng địa chỉ vật lý (physical address). Mọi pointer ta viết trong C đều là địa chỉ thật của RAM hoặc MMIO. Bài này đặt câu hỏi: *Tại sao Linux, Android, mọi OS đều cần MMU? Và một allocator đơn giản nhất trông như thế nào?*

---

## Bối cảnh: Tại sao cần MMU?

Không có MMU, mọi chương trình đều thấy và có thể ghi lên bất kỳ địa chỉ RAM nào — kể cả kernel code, stack của nhau. Ngay cả khi chưa cần bảo vệ (protection), MMU vẫn cần cho:

1. **Cache control:** Phần cứng cần biết vùng nào là Device memory (không cache), vùng nào là Normal memory (có cache). Nếu cache thiết bị UART → lệnh `uart_puts()` ghi vào cache, chưa ra ngoài UART hardware → màn hình im lặng.
2. **Chuẩn bị cho bài 9+:** Context switch, user/kernel space đều yêu cầu MMU.

---

## Kiến trúc Page Table của AArch64

AArch64 dùng tối đa 4 cấp page table (PGD → PUD → PMD → PTE). Với 4KB granule và 39-bit VA (T0SZ=25), ZKOS dùng 2 cấp:

```
Virtual Address (39-bit)
 [38:30]  [29:21]  [20:12]  [11:0]
   L0       L1      (không dùng, block descriptor)   offset

L0 entry → trỏ đến L1 table
L1 entry → block descriptor (mỗi entry = 1GB)
```

**Tại sao dùng block descriptor (1GB) thay vì 4KB pages?**  
Đơn giản hóa: ít entry hơn, không cần PMD và PTE. Đủ dùng cho OS học tập. Linux dùng 4KB pages để có granularity cao hơn.

### Memory Attribute Index Register (MAIR_EL2)

```c
// boot/8-memory/kernel/mm.c
uint64_t mair = (0x00UL << 0) |  // Attr0 index 0: Device-nGnRnE (không cache, không reorder)
                (0xFFUL << 8);    // Attr1 index 1: Normal WB Cacheable (cache thông thường)
asm volatile("msr mair_el2, %0" :: "r"(mair));
```

Mỗi page table entry mang 3 bit `AttrIndx[2:0]` — index vào MAIR. Ta dùng:
- Index 0 → Device → vùng MMIO (`0x00000000 - 0x7FFFFFFF`)
- Index 1 → Normal Cacheable → DDR (`0x80000000 - 0xFFFFFFFF`)

---

## Quá trình xây dựng MMU

### Bước 1: Tạo page table trong RAM

[boot/8-memory/kernel/mm.c](../boot/8-memory/kernel/mm.c) đặt page table ở địa chỉ cố định `0x80200000` (`PAGE_TABLE_BASE`):

```c
static uint64_t *l0_table = (uint64_t *)PAGE_TABLE_BASE;           // 4KB
static uint64_t *l1_table = (uint64_t *)(PAGE_TABLE_BASE + PAGE_SIZE); // 4KB tiếp theo

// L0[0] → chỉ đến L1 table
l0_table[0] = (uint64_t)l1_table | DESC_VALID | DESC_TABLE;

// L1 entries — identity mapping (VA = PA):
l1_table[0] = 0x00000000UL | BLOCK_DEVICE;  // 0GB-1GB: SoC low
l1_table[1] = 0x40000000UL | BLOCK_DEVICE;  // 1GB-2GB: UART, GIC, WDOG
l1_table[2] = 0x80000000UL | BLOCK_NORMAL;  // 2GB-3GB: DDR (code ta đang chạy)
l1_table[3] = 0xC0000000UL | BLOCK_NORMAL;  // 3GB-4GB: DDR upper
```

**Identity mapping là gì?** VA = PA — địa chỉ ảo giống hệt địa chỉ vật lý. Sau khi bật MMU, mọi pointer cũ trong code vẫn đúng. Đây là kỹ thuật chuẩn khi lần đầu bật MMU.

### Bước 2: Cấu hình TCR_EL2

Translation Control Register — điều khiển cách MMU dịch địa chỉ:

```c
uint64_t tcr = (25UL << 0)   |  // T0SZ=25 → 39-bit VA (512GB space)
               (0x1UL << 8)  |  // IRGN0=WB RA WA (inner cache policy)
               (0x1UL << 10) |  // ORGN0=WB RA WA (outer cache policy)
               (0x3UL << 12) |  // SH0=Inner Shareable
               (0x0UL << 14) |  // TG0=4KB granule
               (0x1UL << 16);   // PS=36-bit PA (64GB physical space)
```

### Bước 3: Bật MMU — 3 bit trong SCTLR_EL2

```c
uint64_t sctlr;
asm volatile("mrs %0, sctlr_el2" : "=r"(sctlr));
sctlr |= (1 << 0);   // M  — MMU enable
sctlr |= (1 << 2);   // C  — Data cache enable
sctlr |= (1 << 12);  // I  — Instruction cache enable
asm volatile("msr sctlr_el2, %0" :: "r"(sctlr));
asm volatile("isb");  // Flush pipeline — bắt buộc sau khi bật MMU
```

**Thứ tự bắt buộc:** Xây page table → Set MAIR → Set TCR → Set TTBR0 → `dsb sy` → `isb` → Set SCTLR. Thiếu `dsb`/`isb` thì CPU có thể đọc page table cũ từ cache → undefined behavior ngay sau khi bật MMU.

---

## Bitmap Page Allocator

Sau khi có MMU, cần quản lý RAM có hệ thống. ZKOS dùng **bitmap allocator** — đơn giản nhất có thể: mỗi bit trong array đại diện cho 1 page (4KB).

```
page_bitmap[0]: bit0=page0, bit1=page1, ..., bit63=page63
page_bitmap[1]: bit0=page64, ...

Bit=0: page còn trống
Bit=1: page đã cấp phát
```

```c
// boot/8-memory/kernel/mm.c
void *page_alloc(void)
{
    for (uint32_t i = 0; i < MAX_PAGES / 64; i++) {
        if (page_bitmap[i] != ~0UL) {          // Còn ít nhất 1 page trống
            for (int bit = 0; bit < 64; bit++) {
                if (!(page_bitmap[i] & (1UL << bit))) {
                    page_bitmap[i] |= (1UL << bit);   // Đánh dấu đã dùng
                    pages_used++;
                    uint32_t page_idx = i * 64 + bit;
                    return (void *)(HEAP_BASE + page_idx * PAGE_SIZE);
                }
            }
        }
    }
    return NULL;  // Hết RAM
}
```

Layout của 4MB heap (`HEAP_BASE = 0x80600000`):

```
0x80200000  page tables (8KB)
0x80400000  ZKOS code (load address)
0x80600000  Heap (4MB = 1024 pages × 4KB)
  page 0: 0x80600000
  page 1: 0x80601000
  ...
  page 1023: 0x80A00000 - 0xFFF
```

---

## Kernel Heap (kmalloc/kfree)

Page allocator cấp phát theo đơn vị 4KB — quá thô cho struct nhỏ. `kmalloc` dùng **bump allocator** kết hợp **free list**:

```c
// Bump allocator: chỉ tăng offset, không bao giờ lùi
void *kmalloc(size_t size)
{
    size = (size + 15) & ~15UL;  // Align 16 bytes

    // Kiểm tra free list trước (từ các kfree trước đó)
    struct free_block **prev = &free_list;
    struct free_block *curr = free_list;
    while (curr) {
        if (curr->size >= size) {
            *prev = curr->next;   // Lấy ra khỏi free list
            return (void *)curr;
        }
        prev = &curr->next; curr = curr->next;
    }

    // Bump: cấp phát tiếp
    if (heap_offset + size + sizeof(size_t) > HEAP_POOL_SIZE) return NULL;
    *(size_t *)&heap_pool[heap_offset] = size;  // Lưu size trước ptr để kfree biết
    void *ptr = &heap_pool[heap_offset + sizeof(size_t)];
    heap_offset += size + sizeof(size_t);
    return ptr;
}
```

**Tại sao lưu `size` trước pointer?** `kfree(ptr)` chỉ nhận địa chỉ, không biết size. Trick: đặt `size` tại `ptr - sizeof(size_t)` — đây là pattern phổ biến trong kernel allocator thực tế (glibc làm tương tự với `malloc_chunk`).

---

## Kết quả — lệnh `meminfo`

```
ZKOS> meminfo
=== Memory Info ===
MMU: enabled (identity mapping)
Page allocator: 1024 pages (4096 KB)
  Used: 2 pages, Free: 1022 pages
Heap: 64 KB pool
  Used: 128 bytes
```

---

## Bài học rút ra

1. **MMU bắt buộc cho cache đúng:** Device memory phải non-cacheable, thiếu MMU → UART/GIC behaves strangely.
2. **Identity mapping là bước đầu tiên an toàn:** Bật MMU mà không cần thay đổi pointer nào trong code.
3. **Bitmap allocator — O(n) nhưng đủ:** Đơn giản nhất, debug dễ. Linux dùng buddy allocator phức tạp hơn.
4. **Size trước pointer:** Pattern cổ điển để `free()` biết kích thước mà không cần tham số thêm.
5. **`dsb sy` + `isb` không phải tùy chọn:** Bộ nhớ, cache, và pipeline phải đồng bộ trước khi MMU bật.

## Tài liệu đã dùng

| Tài liệu | Phần | Nội dung |
|-----------|------|---------|
| ARM Architecture Reference | D5 Memory Management | TCR_EL2, MAIR_EL2, SCTLR_EL2 fields |
| ARM Architecture Reference | D8 Stage 1 Translation | Block descriptor format, AttrIndx |
| i.MX93 RM | Ch.2 Memory Maps | Physical address layout |
| VinixOS `kernel/src/kernel/mmu/mmu.c` | | Page table init pattern tham khảo |
| OSTEP | Chapter 15-19 (Paging) | Khái niệm page table, TLB |
