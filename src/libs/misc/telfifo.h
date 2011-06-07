/* KMI - these definitions were pulled from xobs.h and xobs/fifos.c, with
 *       modifications to make into a more generally usable library
 *
 * First version - 8/4/2005
 */

#ifndef TELFIFO_H
#define TELFIFO_H

/* Set to 1 if we have a windscreen (i.e. JSF) */
#ifndef WINDSCREEN
#define WINDSCREEN 0
#endif

/* quick indices into the fifos[] array, below.
   N.B. order must agree with order of FifoInfo array in tel_fifo.c
        so that these values match up as indices!!!
 */

typedef enum {
#if WINDSCREEN
    Tel_Id=0, Filter_Id, Focus_Id, Dome_Id, Lights_Id, Cam_Id, Screen_Id,
#else
    Tel_Id=0, Filter_Id, Focus_Id, Dome_Id, Lights_Id, Cam_Id, Cover_Id,
#endif
    numFifos
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
void sendFifoResets(void);
void stopAllDevices(void);
void openFIFOs(void);
void closeFIFOs(void);
void setFifoErrorCallback(void (*func)());

#endif // TELFIFO_H
