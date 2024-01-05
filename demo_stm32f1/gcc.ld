/* Linker script to configure memory regions. 
 * Need modifying for a specific board. 
 *   FLASH.ORIGIN: starting address of flash
 *   FLASH.LENGTH: length of flash
 *   RAM.ORIGIN: starting address of RAM bank 0
 *   RAM.LENGTH: length of RAM bank 0
 */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x8000000, LENGTH = 64K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);	/* end of "RAM" Ram type memory */

SECTIONS
{
	.text :
	{
		KEEP(*(.isr_vector))
		*(.text*)

		KEEP(*(.init))
		KEEP(*(.fini))

		/* .ctors */
		*crtbegin.o(.ctors)
		*crtbegin?.o(.ctors)
		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
		*(SORT(.ctors.*))
		*(.ctors)

		/* .dtors */
 		*crtbegin.o(.dtors)
 		*crtbegin?.o(.dtors)
 		*(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
 		*(SORT(.dtors.*))
 		*(.dtors)

		*(.rodata*)

		KEEP(*(.eh_frame*))
	} > FLASH

	.ARM.extab : 
	{
		*(.ARM.extab* .gnu.linkonce.armextab.*)
	} > FLASH

	__exidx_start = .;
	.ARM.exidx :
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} > FLASH
	__exidx_end = .;

	/* Location counter can end up 2byte aligned with narrow Thumb code but
	   __etext is assumed by startup code to be the LMA of a section in RAM
	   which must be 4byte aligned */
	__etext = ALIGN (4);

	.data : AT (__etext)
	{
		_sdata = .;
		*(vtable)
		*(.data*)

		. = ALIGN(4);
		/* preinit data */
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);

		. = ALIGN(4);
		/* init data */
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP(*(SORT(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);


		. = ALIGN(4);
		/* finit data */
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP(*(SORT(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE_HIDDEN (__fini_array_end = .);

		KEEP(*(.jcr*))
		. = ALIGN(4);
		/* All data end */
		_edata = .;

	} > RAM

	.bss :
	{
		. = ALIGN(4);
		_sbss = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} > RAM	
}
