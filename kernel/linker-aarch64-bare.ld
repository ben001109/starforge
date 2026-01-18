OUTPUT_FORMAT(elf64-aarch64)
ENTRY(_start)

PHDRS
{
  text PT_LOAD FLAGS(5); /* R-X */
  data PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS
{
  . = 0x40000000;      /* QEMU virt machine load address */

  .text.boot : { *(.text.boot) } :text
  .text : { *(.text*) } :text
  .rodata : { *(.rodata*) } :text
  .data : { *(.data*) } :data
  .bss : { *(.bss*) *(COMMON) } :data
}
