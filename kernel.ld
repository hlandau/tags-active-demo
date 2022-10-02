ENTRY(_start)
SECTIONS {
	. = ALIGN(0x10);
	.text : AT(0x0) {
		_start = .;
		*(.text)
		*(.text.*)
	}
	.rodata : {
		*(.rodata .rodata.*)
	}
	.data : {
		*(.data*)
		*(.force.data)
		*(.toc1)
		*(.branch_lt)
	}
	. = ALIGN(0x10);
	.opd : {
		*(.opd)
	}
	. = ALIGN(0x100);
	.got : {
		__toc_start = . + 0x8000;
		*(.got)
		*(.toc)
	}
	.bss : {
		_bss_start = .;
		*(.bss*)
		*(.sbss*)
		*(COMMON)
		_bss_end = .;
	}
	.stacks : {
		. = ALIGN(4096);
		_stack_start = .;
		. += 4096;
		_stack_top = .;
	}
	_end = .;
}
