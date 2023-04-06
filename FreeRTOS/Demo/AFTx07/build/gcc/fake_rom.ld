OUTPUT_ARCH( "riscv" )
ENTRY( _start )

MEMORY
{
	RAM : ORIGIN = 0x8400, LENGTH = 512K
}

ENTRY(_start);

SECTIONS
{
	.init :
	{
		_text = .;
		KEEP (*(SORT_NONE(.init)))
	} > RAM
	
	.text :
	{
		*(.text.unlikely .text.unlikely.*)
		*(.text.startup .text.startup.*)
		*(.text .text.*)
		*(.gnu.linkonce.t.*)
	} > RAM
	
	.fini :
	{
		KEEP (*(SORT_NONE(.fini)))
		_etext = .;
	} > RAM

	.rodata.align :
	{
		. = ALIGN(4);
		_rodata = .;
	} > RAM

	.rodata.start :
	{
		_rodata_lma = LOADADDR(.rodata.start);
	} > RAM

	.rodata :
	{
		*(.rdata)
		*(.rodata .rodata.*)
		*(.gnu.linkonce.r.*)

		. = ALIGN(4);
		_erodata = .;
	} > RAM

	.data.align :
	{
		. = ALIGN(4);
		_data = .;
	} > RAM

	.data.start :
	{
		_data_lma = LOADADDR(.data.start);
	} > RAM

	.data :
	{
		*(.data .data.*)
		*(.gnu.linkonce.d.*)
		. = ALIGN(8);
		PROVIDE( __global_pointer$ = . + 0x800 );
		*(.sdata .sdata.*)
		*(.sdata2 .sdata2.*)
		*(.gnu.linkonce.s.*)
		. = ALIGN(8);
		*(.srodata.cst16)
		*(.srodata.cst8)
		*(.srodata.cst4)
		*(.srodata.cst2)
		*(.srodata .srodata.*)

		. = ALIGN(4);
		_edata = .;
	} > RAM

	.bss.align :
	{
		. = ALIGN(4);
		_bss = .;
	} > RAM

	.bss.start :
	{
		_bss_lma = LOADADDR(.bss.start);
	} > RAM

	.bss :
	{
		*(.sbss*)
		*(.gnu.linkonce.sb.*)
		*(.bss .bss.*)
		*(.gnu.linkonce.b.*)
		*(COMMON)

		. = ALIGN(4);
		_ebss = .;
	} > RAM

	. = ALIGN(8);
	_end = .;

	.stack :
	{
		. = ALIGN(16);
		. += __stack_size;
		. += ALIGN(16);
		_stack_top = .;
	} > RAM
}
