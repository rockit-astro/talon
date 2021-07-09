/* include file for the pc39 driver.
 */

#define PC39_IOCTL_PREFIX (('3' << 16) | ('9' << 8))

/* use this ioctl for an atomic command/response pair.
 * (write/read pairs can get mixed when used from multiple processes)
 */
#define PC39_OUTIN (PC39_IOCTL_PREFIX | 1)
typedef struct
{
    char *out; /* buffer to write */
    int nout;  /* number of bytes in out */
    char *in;  /* return buffer */
    int nin;   /* size of in[]; ioctl return is bytes back */
} PC39_OutIn;

/* use this ioctl to read the current state of the registers.
 * N.B. this is _not_ intended to serve as a memory map template.
 */
#define PC39_GETREGS (PC39_IOCTL_PREFIX | 2)
typedef struct
{
    unsigned char done;
    unsigned char ctrl;
    unsigned char stat;
} PC39_Regs;

/* macros to access bits in the three pc39 i/o regs.
 */

/* bits in the "Done Flag" DONE register */
#define PC39_DONE_X 0x01
#define PC39_DONE_Y 0x02
#define PC39_DONE_Z 0x04
#define PC39_DONE_T 0x08
#define PC39_DONE_U 0x10
#define PC39_DONE_V 0x20
#define PC39_DONE_R 0x40
#define PC39_DONE_S 0x80

/* bits in the "Interrupt and DMA Control" CTRL register */
#define PC39_CTRL_DMA_DIR 0x01
#define PC39_CTRL_DMA_E 0x02
#define PC39_CTRL_DON_E 0x10
#define PC39_CTRL_IBF_E 0x20
#define PC39_CTRL_TBE_E 0x40
#define PC39_CTRL_IRQ_E 0x80

/* bits in the "Status" STAT register */
#define PC39_STAT_CMD_S 0x01
#define PC39_STAT_INIT 0x02
#define PC39_STAT_ENC_S 0x04
#define PC39_STAT_OVRT 0x08
#define PC39_STAT_DON_S 0x10
#define PC39_STAT_IBF_S 0x20
#define PC39_STAT_TBE_S 0x40
#define PC39_STAT_IRQ_S 0x80
