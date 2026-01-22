.section .text.boot
.global _start

_start:
    mrs x1, mpidr_el1
    and x1, x1, #0xFF
    cbz x1, primary_cpu
hang:
    wfe
    b hang

primary_cpu:
    ldr x0, =stack_top
    mov sp, x0

    bl dtb_boot_entry
    b halt_forever

.section .bss
.align 16
stack_bottom:
    .skip 8192
stack_top:
