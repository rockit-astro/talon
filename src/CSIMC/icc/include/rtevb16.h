	ldab	#15		; access to registers
	tbek
	ldd #3
	std 0xFA48		; CSBARBT
	ldab	#0x7F
	stab	0xFA04		; SYNCR
	clr	0xFA21		; SYPCR

	; set SRAM to 0x1????
	ldd	#1 
	std	0xFB04		; RAMBAH
	clrw	0xFB06		; RAMBAL
	ldd #0x800		; lock the SRAM
	std	0xFB00		; RAMCR
