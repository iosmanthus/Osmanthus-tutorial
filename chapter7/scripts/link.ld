ENTRY(__start)
SECTIONS
{
PROVIDE( __kernel_begin_addr = . );
. = 0x100000;
.text :
{
  *(.text)
  . = ALIGN(4096);
}
.data :
{
  *(.data)
    *(.rodata)
    . = ALIGN(4096);
}
.bss :
{
  *(.bss)
  . = ALIGN(4096);
}
.stab :
{
  *(.stab)
  . = ALIGN(4096);
}
.stabstr :
{
  *(.stabstr)
  . = ALIGN(4096);
}
PROVIDE( __kernel_end_addr = . );

/DISCARD/ : { *(.comment) *(.eh_frame) }
}
