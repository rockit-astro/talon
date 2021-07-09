/* public include file for the CyberResearch, Inc, CIO-DIO 8255 I/O boards.
 * (C) Copyright 1997 Elwood Charles Downey. All rights reserved.
 */

/* ioctl commands */

/* dio 8255 reg set */
typedef struct
{
    unsigned char portA;
    unsigned char portB;
    unsigned char portC;
    unsigned char ctrl;
} DIO8255;

/* pass a pointer to this as the ioctl arg (3rd arg) */
typedef struct
{
    int n;           /* which of the several 8255 regs to access */
    DIO8255 dio8255; /* values for the 8255 regs */
} DIO_IOCTL;

/* pass one of these as the ioctl cmd arg (2nd arg) */
#define PREFIX (('C' << 16) | ('D' << 8))
#define DIO_GET_REGS (PREFIX | 1) /* set 8255 reg set n */
#define DIO_SET_REGS (PREFIX | 2) /* get 8255 reg set n */

extern void dio96_ctrl(int regn, unsigned char ctrl);
extern void dio96_getallbits(unsigned char bits[12]);
extern void dio96_setbit(int n);
extern void dio96_clrbit(int n);

extern int dio96_trace;
