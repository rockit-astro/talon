/*
	CCD Server support
	S. Ohmert 10-25-02
	
	As a better alternative to the script-based auxcam, this will support
	TCP/IP based communication with a server that controls the camera.
	The server is assumed to be a daemon running that will accept the commands
	given and return information according to the basic "telserver" concept.
	Commands are ascii, typically terminated by \r\n.
	Return blocks are preceded by *>>>\r\n and concluded with *<<<\r\n
	Return data is found in the lines between.
	
    The TCP/IP daemon must be "telserver-like" and support the commands 
    found herein (see sendServerCommand calls)

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "ccdcamera.h"
#include "ccdshared.h"

static CCDExpoParams expsav;	/* copy of exposure params to use */

static int ccdserver_pixpipe; // like fli monitor... fd becomes readable when pixels ready
static int ccdserver_diepipe; // like fli monitor... kills support child in case parent dies
static int whoExposing; // If more than one "camera" app is controlling driver, only 1 can expose/cancel at a time
void freeBinBuffer(void);
char * getBinBuffer(void);
long getBinSize(void);
int getLastFailcode(void);
char *getLastFailMessage(void);
char * getReturnLine(int line);
int getNumReturnLines(void);
int sendServerCommand(char *retBuf, char *fmt, ...);
int initCCDServer(char *host, int port, char *errRet);
void Reconnect(void);
static int ccdserver_monitor(char *err);
static int areWeExposing(void);

#define CCDServerPP  1000000	/* exposure poll period, usecs */

typedef int Boolean;
#ifndef FALSE
#define FALSE (0)
#endif


#ifndef TRUE
#define TRUE (!FALSE)
#endif

typedef int	SOCKET;
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
typedef struct sockaddr SOCKADDR;

// local variables
static int		command_timeout = 20;		// timeout for inactivity
static SOCKET 	sockfd = INVALID_SOCKET;    // fd of server
static SOCKET 	sockfd2 = INVALID_SOCKET;    // second connection used by monitor
static Boolean errorExit;
// Buffers used when reading a return block
// maximum size of an input line
#define MAXLINE				1024
#define MAX_BLOCK_LINES		64
static char blockLine[2][MAX_BLOCK_LINES][MAXLINE];
static int lineNum = 0;
static long binSize = 0;
static char *pBinBuffer = NULL;
static int failcode;
static int failLine;

#define TIMEOUT_ERROR -100
#define BLOCK_SYNCH_ERROR -101

// local prototypes
static int connectToServer(SOCKET insock, char *host, int port, char *errRet);
static int getReturnBlock(SOCKET sockin);
static int readBinary(SOCKET sockin, char *pBuf, int numBytes);
static int readLine(SOCKET sockin, char *buf, int maxLine);
static void myrecv(SOCKET fd, char *buf, int len);
static void mysend(SOCKET fd, char *buf, int len);
static int sendServerCommand2(SOCKET sockin, char *retBuf, char *cmd);

// keep for reconnect
static char lastHost[256];
static int lastPort;


/************ Support functions for the public Talon camera API ***********/

//
// Initialize the connection to the ccd server.
// returns 0 on success, or -1 on failure
// Reason for error returned via errRet
//
int initCCDServer(char *host, int port, char *errRet)
{
	int rt;

	errorExit = 0;
	
	if(sockfd != INVALID_SOCKET
	&& sockfd2 != INVALID_SOCKET
	&& !strcasecmp(host,lastHost)
	&& port == lastPort) {
		return 0; // already connected
	}
	
	if(sockfd != INVALID_SOCKET) {
		close(sockfd);
		sockfd = INVALID_SOCKET;
	}
	if(sockfd2 != INVALID_SOCKET) {
		close(sockfd2);
		sockfd2 = INVALID_SOCKET;
	}
		
	strcpy(lastHost,host);
	lastPort = port;

	/* create socket */
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		sprintf(errRet,"Error creating socket: %s\n",strerror(errno));		
	    return -1;
	}
	
	if ((sockfd2 = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		close(sockfd);
		sockfd = INVALID_SOCKET;
		sprintf(errRet,"Error creating socket: %s\n",strerror(errno));		
	    return -1;
	}

	rt = connectToServer(sockfd, host, port, errRet);
	if(!rt) rt = connectToServer(sockfd2, host, port, errRet);
	if(rt < 0) {
		if(sockfd != INVALID_SOCKET) {
			close(sockfd);
			sockfd = INVALID_SOCKET;
		}
		if(sockfd2 != INVALID_SOCKET) {
			close(sockfd2);
			sockfd2 = INVALID_SOCKET;
		}
	}
	if(!rt)
	{
		char buf[1024];
		buf[0] = 0;
		getIDCCD(buf,buf);
	}
	return rt;
}

//
// Connect the socket to the server
//
int connectToServer(SOCKET insock, char *host, int port, char *errRet)
{
	struct sockaddr_in cli_socket;
	struct hostent *hp;
	int len;

	/* get host name running server */
	if (!(hp = gethostbyname(host))) {
		sprintf(errRet,"Error resolving host: %s\n",strerror(errno));		
	    return -1;
	}
		
	/* connect to the server */
	len = sizeof(cli_socket);
	memset (&cli_socket, 0, len);
	cli_socket.sin_family = AF_INET;
	cli_socket.sin_addr.s_addr =
			    ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
	cli_socket.sin_port = htons(port);
	if (connect (insock, (struct sockaddr *)&cli_socket, len) < 0) {
		sprintf(errRet,"Unable to connect with CCD Server: %s\n",strerror(errno));
	    return -1;
	}

	/* ready */
	return 0;
}

//
// Reconnect if we lose connection
//
void Reconnect(void)
{
	char msg[256];

	errorExit = 0;

	if(sockfd != INVALID_SOCKET) {
		close(sockfd);
		sockfd = INVALID_SOCKET;
	}
	if(sockfd2 != INVALID_SOCKET) {
		close(sockfd2);
		sockfd2 = INVALID_SOCKET;
	}
		
	if(initCCDServer(lastHost,lastPort,msg) < 0) {
		fprintf(stderr,"Unable to reconnect to %s:%d [%s]\n",lastHost,lastPort,msg);
	}
}

/*
 * Send a command to the CCD Server and return response via return value 
 * and *retBuf. Formatted command is in *fmt and args
 */
int sendServerCommand(char *retBuf, char *fmt, ...)
{
	char buf[8192];
	va_list ap;
	int rt;
		
	/* format the message */
	va_start (ap, fmt);
	vsprintf (buf, fmt, ap);
	va_end (ap);

	errorExit = 0;

#if CMDTRACE	
	fprintf(stderr,"Sending Command: %s\n",buf);
#endif
	rt = sendServerCommand2(sockfd, retBuf, buf);
	if(errorExit) {
		Reconnect();
		rt = sendServerCommand2(sockfd, retBuf, buf);
	}
	return rt;
}
/* Send to a specific socket connection (monitor program creates 
 * asynch link...)
 */
int sendServerCommand2(SOCKET sockin, char *retBuf, char *cmd)
{
	if(!errorExit) mysend(sockin, cmd, strlen(cmd));
	if(!errorExit) mysend(sockin, "\r\n", 2);
	if(!errorExit) {
		getReturnBlock(sockin);
		if(getLastFailcode()) {
			sprintf(retBuf, "%s",getLastFailMessage());
			return -1;
		}
		if(getNumReturnLines()) {
			// return the first line of the response.  Mindful of which return block to read.
			strcpy(retBuf,blockLine[sockin == sockfd ? 0 : 1][1]);
		} else strcpy(retBuf,"");
		return 0;		
	} else {
		sprintf(retBuf,"Communications Error");
		return -1;
	}
}

//
// Get the number of lines in the most recently returned block
//
int getNumReturnLines(void)
{
	return lineNum;
}

//
// Read the return lines from the most recent return block
// Line numbers are 0-based
// Only reads from "main" (sockfd) block.
//
char * getReturnLine(int line)
{
	if(line < 0 || line >= lineNum-1) {
		return "";
	}
	return blockLine[0][line+1];
}		

//
// Get the failure code of the last return block
//
int getLastFailcode(void)
{
	return failcode;
}

//
// Get the last failure message
//
char *getLastFailMessage(void)
{
	char buf[MAXLINE];
	char *p;
	
	strcpy(buf, blockLine[0][failLine]);
	p = strchr(buf,':');
	if(p) p++;
	else p = buf;
		
	if(!failcode) return "";
	return p;
}

//
// Return a pointer to the binary buffer
//
char * getBinBuffer()
{
	return pBinBuffer;
}

//
// return the size of the binary buffer
//
long getBinSize()
{
	return binSize;
}

//
// Free the binary buffer if it exists
//
void freeBinBuffer()
{
	if(pBinBuffer) free(pBinBuffer);
	pBinBuffer = NULL;
	binSize = 0;
}

//
// Monitor function -- essentially same as fli_monitor
//
/* create a child helper process that monitors an exposure. set ccdserver_pixpipe
 *   to a file descriptor from which the parent (us) will read EOF when the
 *   current exposure completes. set ccdserver_diepipe to a fd from which the child
 *   will read EOF if we die or intentionally wish to cancel exposure.
 * return 0 if process is underway ok, else -1.
 */
static int ccdserver_monitor(char *err)
{
	int pixp[2], diep[2];
	int pid;

	if (pipe(pixp) < 0 || pipe(diep)) {
	    sprintf (err, "CCD Server pipe: %s", strerror(errno));
	    return (-1);
	}

	signal (SIGCHLD, SIG_IGN);	/* no zombies */
	pid = fork();
	if (pid < 0) {
	    sprintf (err, "CCD Server fork: %s", strerror(errno));
	    close (pixp[0]);
	    close (pixp[1]);
	    close (diep[0]);
	    close (diep[1]);
	    return(-1);
	}

	if (pid) {
	    /* parent */
	    ccdserver_pixpipe = pixp[0];
	    close (pixp[1]);
	    ccdserver_diepipe = diep[1];
	    close (diep[0]);
	    return (0);
	}

	/* child .. poll for exposure or cancel if EOF on diepipe */
	close (pixp[0]);
	close (diep[1]);
	
	// mark us as in control of this exposure
	whoExposing = sockfd;
	
	while (1) {
	    struct timeval tv;
	    fd_set rs;
	    char buf[256];
	    char *p;

	    tv.tv_sec = CCDServerPP/1000000;
	    tv.tv_usec = CCDServerPP%1000000;

	    FD_ZERO(&rs);
	    FD_SET(diep[0], &rs);

	    switch (select (diep[0]+1, &rs, NULL, NULL, &tv)) {
		    case 0: 	/* timed out.. time to poll camera */
		    	// ArePixelsReady will return non-zero if TRUE
		    	if(sendServerCommand2(sockfd2,buf,"ArePixelsReady") < 0) {
					_exit(1);
				}
				p = strtok(buf," \r\n\0");
				if(atoi(p))	{
					_exit(0); // yep, come and get 'em
				}		    	
				break;
	    	case 1:	/* parent died or gave up */
//				sendServerCommand2(sockfd2,buf,"CancelExposure");
				_exit(0);
				break;
		    default:	/* local trouble */
//				sendServerCommand2(sockfd2,buf,"CancelExposure");
				_exit(1);
				break;
	    }
	}
}

//
// See if we are the ones exposing, and therefore have the right to cancel
//
static int areWeExposing()
{
	return(whoExposing == sockfd);
}

// --------------------------------------------------------------

/* My send and receive */
static void mysend(SOCKET fd, char *buf, int len)
{
	int err;
	time_t startTime;
	
	errno = 0;

	if(!fd) return;

	startTime = time(NULL);

	do {

		err = send(fd,buf,len,(MSG_DONTWAIT | MSG_NOSIGNAL));
		if(err != 0) err = errno;
		
		if( (time(NULL) - startTime) > command_timeout) {
			fprintf(stderr,"Timeout on send for socket %d\n",fd);
			err = -1;
		}

	} while (err == EAGAIN || err == EWOULDBLOCK || err == EINTR);
	
	if(!err) return;

	fprintf(stderr,"ccdcamera.mysend(): Error on send on socket %d -- [%s] buf [%s]\n",fd, strerror(errno), buf);
	
	errorExit = 1; // EXIT ON ERROR!!!	
}

static void myrecv(SOCKET fd, char *buf, int len)
{
	int rb;
	int err;

	fd_set rfds;
	struct timeval tv;
	if(!fd) return;
	memset(&tv, 0, sizeof(tv));

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = command_timeout;
	tv.tv_usec = 0;

	err = select(fd+1, &rfds, NULL, NULL, &tv);
	if(err <= 0) {
		fprintf(stderr,"Timeout on recv for socket %d\n",fd);
		errorExit = 1; // EXIT ON ERROR!!!
		return;
	}

	rb = read(fd,buf,len);		// use read instead of recv... seems to be the CLOSE_WAIT fix!

	if(rb != len) {
		fprintf(stderr,"Length mismatch on recv\n");
		errorExit = 1; // EXIT ON ERROR!!!
	}
}

/*
 * Read a line from the client socket up to but not including the next newline into buffer and terminate.
 * return number of characters read, or -1 on error
 *
 */
static int readLine(SOCKET sockin, char *buf, int maxLine)
{
	int cnt = 0;
	char ch;

	while(cnt < maxLine) {
		myrecv(sockin,&ch,1);
		if(errorExit) {
			return -1;
		}
		if(ch >= ' ') {
			buf[cnt++] = ch;
		}
		else if(ch == '\n') {
			buf[cnt++] = 0;
			return cnt;
		}
	}	
	buf[maxLine-1] = 0;
	return cnt;
}

/*
 * Read numBytes of binary data from the client socket into pBuf
 * return number of bytes read, or -1 on error
 */
static int readBinary(SOCKET sockin, char *pBuf, int numBytes)
{
/*
	int chunk = 256;
	int remain = numBytes;
	char *p = pBuf;
	chunk = chunk < remain ? chunk : remain;
	while(remain) {
		myrecv(sockfd,p,chunk);
		p += chunk;
		remain -= chunk;
		if(remain < chunk) chunk = remain;
	}
	if(errorExit) return -1;
	return numBytes;
*/

	int remain = numBytes;
	char *p = pBuf;
	while(!errorExit && remain--) {
		myrecv(sockin,p++,1);
	}
	if(errorExit) return -1;
	return numBytes;
}

/*
 * Collect a return block, gathering into an array of strings
 * and possibly a binary block
 * Return the number of lines in block (not including terminators)
 *
 */
static int getReturnBlock(SOCKET sockin)
{
	int 	bytesRead;
	long	size;
	int 	who = sockin == sockfd ? 0 : 1;
		
	// read until we hit end of block
	// or out of array slots
	lineNum = 0;
	failcode = 0;
	while(1) {
		blockLine[who][lineNum][0] = 0;
		bytesRead = readLine(sockin, blockLine[who][lineNum], MAXLINE);
		if(bytesRead < 0) break;
		if(!strcmp(blockLine[who][lineNum],"*<<<")) {
			break; // done!
		}
		if(sscanf(blockLine[who][lineNum],"*FAILURE (%d)",&failcode) == 1) {
			failLine = lineNum;
#if CMDTRACE		
			fprintf(stderr,"Failure (%d) detected\n",failcode);
#endif			
		}		
		if(sscanf(blockLine[who][lineNum],"<BIN:%ld>",&size) == 1) {
			freeBinBuffer();
			pBinBuffer = (char *) malloc(size);
			if(pBinBuffer) {
				binSize = size;
				readBinary(sockin, pBinBuffer,size);
			}
		}
		if(lineNum < MAX_BLOCK_LINES-1) {
			lineNum++;
		}
	}
	if(lineNum >= MAX_BLOCK_LINES-1) {
		fprintf(stderr,"Block Line Overrun Detected\n");
	}
	if(errorExit) {
		fprintf(stderr,"Timed out reading return block\n");
		failcode = TIMEOUT_ERROR;
	}
	else if(strcmp(blockLine[who][0],"*>>>")) {
		fprintf(stderr,"Expected block start, got %s\n",blockLine[who][0]);
		failcode = BLOCK_SYNCH_ERROR;
	}
	if(failcode) lineNum = 0;	
	return lineNum-1; // number of block lines, not counting terminators (starting at blockLine[1])
}



/***************** Begin public Talon CCD interface functions *************/

/* Detect and initialize a camera. Optionally use information passed in 
 * via the *path string. For example, this may be a path to a kernel
 * device node, a path to an auxcam script, an IP address for a networked
 * camera, etc.
 *
 * Return 1 if camera was detected and successfully initialized.
 * Return 0 if camera was not detected.
 * Return -1 and put error string in *errmsg if camera was detected but
 * could not be initialized.
 */
int
server_findCCD (char *path, char *errmsg)
{
	char *p;

    /* if path looks like a host:port name, then it must be a CCD Server */
    if((p = strchr(path,':'))) {
        *p++ = 0;
        if (initCCDServer(path, atoi(p), errmsg) == 0) {
            return 1;
        }
        else {
            return -1;
        }
    }

    return 0;
}

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
server_setExpCCD (CCDExpoParams *expP, char *errmsg)
{
    expsav = *expP; /* Save a copy for use in startExpCCD */

    return sendServerCommand(errmsg, "TestExpParams %d %d %d %d %d %d %d",
            expP->sx, expP->sy, expP->sw, expP->sh,
            expP->bx, expP->by, expP->shutter);
}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
server_startExpCCD (char *errmsg)
{
    int rt = sendServerCommand(errmsg, "StartExpose %d %d %d %d %d %d %d %d",
            expsav.duration, expsav.sx, expsav.sy, expsav.sw, expsav.sh,
            expsav.bx, expsav.by, expsav.shutter);
    if(rt == 0) {
        rt = ccdserver_monitor(errmsg);
    }
    return rt;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int 
server_setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
    /* Not sure we CAN support this externally...
       int rt =  sendServerCommand(errmsg, "SetDriftScan");
       if(rt == 0) {
       rt = ccdserver_monitor(errmsg);
       }
       return rt;
       */
    strcpy (errmsg, "Drift scanning not supported");
    return (-1);
}

/* Performs RBI Annihilation process, if supported.
 * RBI Annihilation floods the CCD array with charge for
 * an "exposure" and then discards and drains.
 * This prevents exposure to bright objects from creating
 * Residual Buffer Images in subsequent frames.
 * Return 0 if ok, else set errmsg[] and return -1.
 */
int 
server_performRBIFlushCCD (int flushMS, int numDrains, char* errmsg)
{
    sprintf (errmsg, "RBI Annihilation not supported");
    return (-1);
}

/* abort a current exposure, if any.
 */
void 
server_abortExpCCD (void)
{
    char buf[256];
    if (ccdserver_pixpipe > 0) {
        close (ccdserver_pixpipe);
        ccdserver_pixpipe = 0;
        close (ccdserver_diepipe);
        ccdserver_diepipe = 0;
        printf("Cancelling exposure (1)\n");
        (void) sendServerCommand(buf, "CancelExposure");
        whoExposing = 0;
    } else {
        if(areWeExposing()) {
            printf("Cancelling exposure (2)\n");
            (void) sendServerCommand(buf, "CancelExposure");
            whoExposing = 0;
        }
    }
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int 
server_selectHandleCCD (char *errmsg)
{
    if (ccdserver_pixpipe)
        return (ccdserver_pixpipe);
    else {
        sprintf (errmsg, "CCD Server not exposing");
        return (-1);
    }
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
server_setTempCCD (CCDTempInfo *tp, char *errmsg)
{
    int rt;
    if(tp->s == CCDTS_OFF) {
        rt = sendServerCommand(errmsg,"SetTemp OFF");
    } else {
        rt = sendServerCommand(errmsg,"SetTemp %d", tp->t);
    }
    return rt;
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
server_getTempCCD (CCDTempInfo *tp, char *errmsg)
{
    if(sendServerCommand(errmsg,"GetTemp") < 0) {
        return -1;
    }

    parseTemperatureString(errmsg, tp);
    return 0;
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
server_setShutterNow (int open, char *errmsg)
{
    return sendServerCommand(errmsg,"SetShutterNow %d",open);
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
server_getIDCCD (char buf[], char *errmsg)
{
    int n;

    if(sendServerCommand(errmsg,"GetIDString") < 0) {
        return -1;
    }

    n = strlen(errmsg);
    if (errmsg[n-1] == '\n')
        errmsg[n-1] = '\0';
    strcpy (buf, errmsg);
    return (0);
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
server_getSizeCCD (CCDExpoParams *cep, char *errmsg)
{
    if(sendServerCommand(errmsg,"GetMaxSize") < 0) {
        return -1;
    }

    return parseCCDSize(errmsg, cep);
}

/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int
server_readPix (char *mem, int nbytes, int block, char *errmsg)
{
    // do same way as FLI pipe...
    struct timeval tv, *tvp;
    size_t bytesgrabbed = 0;
    fd_set rs;
    char *memend;

    if (!ccdserver_pixpipe) {
        strcpy (errmsg, "CCD Server not exposing");
        return (-1);
    }

    /* block or just check fli_pipe */
    FD_ZERO(&rs);
    FD_SET(ccdserver_pixpipe, &rs);
    if (block) {
        tvp = NULL;
    } else {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        tvp = &tv;
    }
    switch (select (ccdserver_pixpipe+1, &rs, NULL, NULL, tvp)) {
        case 1:	/* ready */
            break;
        case 0:	/* timed out -- must be non-blocking */
            sprintf (errmsg, "CCD Server is Exposing");
            return (-1);
        case -1:
        default:
            sprintf (errmsg, "CCD Server %s", strerror(errno));
            return (-1);
    }

    close (ccdserver_pixpipe);
    ccdserver_pixpipe = 0;
    close (ccdserver_diepipe);
    ccdserver_diepipe = 0;

    if(sendServerCommand(errmsg,"GetPixels") < 0) {
        return -1;
    }

    bytesgrabbed = getBinBuffer() ? getBinSize() : 0;

    if (nbytes > bytesgrabbed) {
        sprintf (errmsg, "CCD Server %d bytes short", nbytes-bytesgrabbed);
        freeBinBuffer();
        return (-1);
    }

    memcpy(mem,getBinBuffer(),nbytes);
    freeBinBuffer();

    /* byte swap FITS to our internal format */
    for (memend = mem+nbytes; mem < memend; ) {
        char tmp = *mem++;
        mem[-1] = *mem;
        *mem++ = tmp;
    }

    return(0);


}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void server_getCallbacksCCD(CCDCallbacks *callbacks)
{
    callbacks->findCCD = server_findCCD;
    callbacks->setExpCCD = server_setExpCCD;
    callbacks->startExpCCD = server_startExpCCD;
    callbacks->setupDriftScan = server_setupDriftScan;
    callbacks->performRBIFlushCCD = server_performRBIFlushCCD;
    callbacks->abortExpCCD = server_abortExpCCD;
    callbacks->selectHandleCCD = server_selectHandleCCD;
    callbacks->setTempCCD = server_setTempCCD;
    callbacks->getTempCCD = server_getTempCCD;
    callbacks->setShutterNow = server_setShutterNow;
    callbacks->getIDCCD = server_getIDCCD;
    callbacks->getSizeCCD = server_getSizeCCD;
    callbacks->readPix = server_readPix;
}

