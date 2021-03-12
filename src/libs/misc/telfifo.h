/* KMI - these definitions were pulled from xobs.h and xobs/fifos.c, with
 *       modifications to make into a more generally usable library
 *
 * First version - 8/4/2005
 */

#ifndef TELFIFO_H
#define TELFIFO_H

/* quick indices into the fifos[] array, below.
   N.B. order must agree with order of FifoInfo array in tel_fifo.c
        so that these values match up as indices!!!
 */

typedef enum {
    Tel_Id=0, Focus_Id, Dome_Id, Cover_Id, numFifos
} FifoId;

/* this is used to describe the several FIFOs used to communicate with
 * the telescoped.
 */
typedef struct {
    char *name;         /* fifo name */
    FifoId fid;     /* cross-check */
    int fd[2];          /* file descriptor to/from the daemon, once opened */
    int fdopen;     /* set when fd[] is in use */
} FifoInfo;

/* telfifo.c */
FifoInfo getFIFO(int id);
int fifoMsg(FifoId fid, char *fmt, ...);
int fifoRead(FifoId fid, char buf[], int buflen);
void stopAllDevices(void);
void openFIFOs(void);
void closeFIFOs(void);
void setFifoErrorCallback(void (*func)());

#endif // TELFIFO_H
