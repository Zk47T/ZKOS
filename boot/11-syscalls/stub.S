.global _start
.extern _bss_start
.extern _bss_end

_start:
    ldr x0, =0x803F0000
    mov sp, x0

    // Clear BSS section
    ldr x0, =_bss_start
    ldr x1, =_bss_end
1:  cmp x0, x1
    b.ge 2f
    str xzr, [x0], #8
    b 1b
2:
    // Set Exception Vector Table
    ldr x0, =vector_table_el2
    msr vbar_el2, x0

    bl main
3:  wfe
    b 3b
