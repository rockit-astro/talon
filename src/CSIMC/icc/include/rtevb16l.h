		ldd		#$0003
		std 	CSBARBT
        LDD     #$0303
        STD     CSBAR0          ;set U1 RAM base addr to $30000: bank 3, 64k
        STD     CSBAR1          ;set U3 RAM base addr to $30000: bank 3, 64k
        LDD     #$5030
        STD     CSOR0           ;set Chip Select 0, upper byte, write only
        LDD     #$3030
        STD     CSOR1           ;set Chip Select 1, lower byte, write only
        LDD     #$0303
        STD     CSBAR2          ;set Chip Select 2 to fire at base addr $30000
        LDD     #$7830
        STD     CSOR2           ;set Chip Select 2, both bytes, read and write
        LDD     #$3FFF
        STD     CSPAR0          ;set Chip Selects 0,1,2 to 16-bit ports
