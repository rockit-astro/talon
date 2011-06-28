; Non-flash startup setup for Freedom16-mite
;
; Set up CS0 (CSRAM) to start at $00:0000 and CSBOOT (CSROM), to start
; at $02:0000 (to get it out of the way) and then ignore Flash in this
; model.  Both CSes are set for 128K.  CS1 (CS_WRITE) is set for 256K 
; to write enable both RAM & ROM, even though ROM (Flash) is not used.
    LDD   #0x0204    ;at reset, the CSBOOT block size is 512k (pg. D-29)
                    ;(0x0002=16K, $0003=64K, $0004=128K, $0005=256K)
    STD   CSBARBT   ;Sets CSROM block size to 128k starting at page 0x02:
    LDD   #0x0004    ;Set CS0 (CSRAM) to 128K starting at page $00:
    STD   CSBAR0    ;This is academic as FLASH is not used in this model
    LDD   #0x0005    ;Set CS1 (CSWRITE) to 256k starting at page $00:
    STD   CSBAR1    ;CS1 to write enable both Flash & SRAM: 256K
    LDD   #0x5830    ;CSBOOT DSACK generator to 0 WAIT states
    STD   CSORBT    ;0 10 11 0 0000 11 000 0: async, both bytes, R|W,
                    ;addr strobe, xxxx waits, all IPL, AVEC off
    STD   CSOR0     ;Set CS0 (CSRAM) to same as CSBOOT
    LDD   #0x57B0    ;0 10 10 1 1110 11 000 0 (async, hi byte, write only,
    STD   CSOR1     ;DS, fast term, user|superv, all IPL, AVEC off)
;
    LDD  #0x00AA     ;Set up Chip Select Pin Assignment Reg. (pg. D-29)
    STD  CSPAR0     ;00 00 00 00 10 10 10 10:  CS2..CSBOOT: 8 bit CSes
    CLR  CSPAR1     ;00 00 00 00 00 00 00 00:  CS10..CS2: -> dicrete I/O
