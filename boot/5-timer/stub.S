.global _start
_start:
    ldr x0, =0x803F0000
    mov sp, x0
    bl main
1:  wfe
    b 1b
