OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

SECTIONS
{
  . = 0x00100000;      /* 1 MiB load address (physical) */

  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
