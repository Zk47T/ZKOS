.global _start
_start:
    ldr x0, =0x803F0000
    mov sp, x0

    // Set Exception Vector Table Base Address Register
    ldr x0, =vector_table_el2
    msr vbar_el2, x0

    bl main
1:  wfe
    b 1b
