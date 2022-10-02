.section ".text"
.align 2
.type _start, @function
.globl _start
_start:

	# Fixup endian. This magic code is interpreted by a CPU in the
	# same endian this code was compiled in as a no-op, but switches
	# the endian if the endian differs.
	tdi	0, 0, 0x48	# Reverse endian of 'b . + 8'
	b	1f   		# Skip trampoline if endian is good
	.long	0x05009f42	# bcl		%r20, %r31, $+4
	.long	0xa602487d	# mflr		%r10
	.long	0x1c004a39	# addi		%r10, %r10, 28
	.long	0xa600607d	# mfmsr		%r11
	.long	0x01006b69	# xori  	%r11, %r11, 1
	.long	0xa6035a7d	# mtsrr0	%r10
	.long	0xa6037b7d	# mtsrr1	%r11
	.long	0x2400004c	# rfid
1:

	# Capture the PC into LR.
	bl	2f
2:
	# Determine image base load address by using the captured LR.
	# We have to subtract the size of our endian fixup and BL instruction
	# code above.
	mflr	%r9
	addic	%r9, %r9, -(2b - _start)

	# Load TOC register.
	lis	%r2, (__toc_start)@highest
	ori	%r2, %r2, (__toc_start)@higher
	rldicr	%r2, %r2, 32, 31
	oris	%r2, %r2, (__toc_start)@high
	ori	%r2, %r2, (__toc_start)@l

	# Add image base offset to TOC register.
	addc	%r2, %r2, %r9

	# Launch.
	bl	main
	b	.
