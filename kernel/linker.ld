OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
  text PT_LOAD FLAGS(5); /* R-X */
  data PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS
{
  . = 0x00100000;      /* 1 MiB load address (physical) */

  .text : { *(.text*) } :text
  .rodata : { *(.rodata*) } :text
  .data : { *(.data*) } :data
  .bss : { *(.bss*) *(COMMON) } :data
}
