/* this file defines the host-side and both-side stuff for the CSIMC.
 */

#ifndef _CSIMC_H
#define _CSIMC_H

#include <stddef.h>

/* datalink packet details */

typedef unsigned char Byte;  /* unsigned int 0..255 */
typedef unsigned short Word; /* unsigned int 0..65535 */

#define PMXDAT 80                  /* max bytes in data portion of packet */
#define PMXLEN (PB_NZHSZ + PMXDAT) /* max bytes in any packet */
#define PSYNC 0x88                 /* SYNC byte .. must not appear anywhere else */
#define PESC 0xED                  /* escape for PSYNC and PESC in data */
#define PESYNC 0x00                /* PSYNC becomes PESC PESYNC in data */
#define PEESC 0xEC                 /* PESC becomes PESC PEESC in data */
#define MAXNA 31                   /* max hardware node address */
#define NNODES 32                  /* max number nodes */
#define MAXHA 63                   /* maximum host virtual node address */
#define NADDR 64                   /* total number of unique addresses */
#define NHOSTS (NADDR - NNODES)    /* total number of host addrs available */
#define BRDCA 254                  /* broadcast address */
#define LOGADR MAXHA               /* address to use for error logging */
#define MAXRTY 10                  /* max retries to get an ACK */
#define ACKWT 1000                 /* ms to wait for ACK before resending */
#define TOKFLG NADDR               /* differentiate between SYNC/TO and SYNC/TOK */

#if MAXRTY * ACKWT < 10000
#error Retry time must be at least 10 secs to allow for flash erase
#endif

#define addr2tok(n) ((n) | TOKFLG)       /* node addr to token value */
#define tok2addr(t) ((t) & ~TOKFLG)      /* token value to node addr */
#define BROKTOK addr2tok(NNODES)         /* token when back to broker */
#define ISNTOK(t) (((t)&0xe0) == TOKFLG) /* is a node token (not host!)*/

typedef enum
{
    PT_SHELL = 0, /* shell message between From to To */
    PT_BOOTREC,   /* Data is S-like boot record */
    PT_INTR,      /* interrupt; shell goes back to reading */
    PT_KILL,      /* kill To shell. Count is 0 */
    PT_ACK,       /* Ack. Count is 0 or GETVAR valu. Seq matches*/
    PT_REBOOT,    /* reboot To (!) */
    PT_PING,      /* check-alive. respond with ACK if alive */
    PT_SETVAR,    /* set remote var. Data is ref then value */
    PT_RES0,      /* Reserved (never use 8 to avoid PSYNC) */
    PT_GETVAR,    /* get remote var. Ack has value in Data */
    PT_SERDATA,   /* Data is To/From RS232 line */
    PT_SERSETUP,  /* Data is baud rate */
} PktType;        /* type of packets */

#define PT_MASK 0x0f  /* bits which hold PktType */
#define PSQ_MASK 0xf0 /* sequencing portion */
#define PSQ_SHIFT 4   /* amount to shift for sequencing portion */

typedef struct
{
    Byte sync;         /* sync byte .. must be PSYNC */
    Byte to;           /* destination node address */
    Byte fr;           /* origination node address */
    Byte info;         /* sequence count and one of PktType */
    Byte count;        /* number of bytes in data[] */
    Byte hchk;         /* checksum up to here */
    Byte dchk;         /* data checksum (not included if count==0) */
    Byte data[PMXDAT]; /* count bytes of data */
} Pkt;                 /* basic data link level packet */

#define PB_SYNC offsetof(Pkt, sync)
#define PB_TO offsetof(Pkt, to)
#define PB_FR offsetof(Pkt, fr)
#define PB_INFO offsetof(Pkt, info)
#define PB_COUNT offsetof(Pkt, count)
#define PB_HCHK offsetof(Pkt, hchk)
#define PB_DCHK offsetof(Pkt, dchk)
#define PB_DATA offsetof(Pkt, data)
#define PB_HSZ (PB_HCHK + 1)  /* size of header portion */
#define PB_NZHSZ (PB_HSZ + 1) /* size of header with non-zero COUNT */
#define PB_NHCHK (PB_HSZ - 1) /* bytes used to calc HCHK */

/* characters to send over socket for special functions */
#define CSIMCD_INTR 0x03 /* control-c */

/* codes used when opening a new connection to server */
typedef enum
{
    FOR_SHELL,
    FOR_BOOT,
    FOR_REBOOT,
    FOR_SERIAL
} OpenWhy;

/* header for a boot image record */
typedef struct
{
    Byte type;         /* type of record .. see BT_ flags */
    Byte len;          /* bytes of data */
    Byte addrh, addrl; /* starting address, big-endian */
} BootIm;

#define BT_DATA 0x1   /* addr is location for data */
#define BT_EXEC 0x2   /* addr is new PC */
#define BT_EEPROM 0x4 /* data is for EEPROM */
#define BT_FLASH 0x8  /* data is for FLASH */

#ifndef _HC12
/* csimcd deamon API */
#define CSIMCPORT 7623 /* default csimcd TCP/IP port number */
extern int csimcd_slisten(int port);
extern int csimcd_saccept(int fd);
extern int csimcd_clconn(char *host, int port);

/* host client API */
extern int csi_open(char *host, int port, int addr);
extern int csi_bopen(char *host, int port, int addr);
extern int csi_sopen(char *host, int port, int addr, int baud);
extern int csi_close(int fd);
extern int csi_intr(int fd);
extern int csi_rebootAll(char *host, int port);
extern int csi_w(int fd, char *fmt, ...);
extern int csi_r(int fd, char buf[], int buflen);
extern int csi_rix(int fd, char *fmt, ...);
extern int csi_wr(int fd, char buf[], int buflen, char *fmt, ...);
extern int csi_f2h(int fd);
extern int csi_f2n(int fd);

#endif /* ! _HC12 */

#endif /* _CSIMC_H */
