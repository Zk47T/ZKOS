.global _start
_start:
    ldr     x9, =0x44380000
1:  ldr     w10, [x9, #0x14]
    tbz     w10, #23, 1b
    mov     w0, #'Z'
    str     w0, [x9, #0x1C]
2:  wfe
    b       2b
