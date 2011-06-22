/* device-depend support for Davis Weather Monitor II and Vantage Pro / Pro 2 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <termios.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "telenv.h"
#include "running.h"
#include "strops.h"
#include "misc.h"
#include "tts.h"
#include "configfile.h"

#include "dd.h"

static int openTTY (char *tty);
static int readDavis (int fd, unsigned char *rcv_buf, int *barcalp);
static void processDavis (unsigned char *rcv_buf, int barcal, WxStats *wp,
    double *tp, double *pp);
static int get_acknowledge(int fd);
static int get_serial_char (int fd);
static int get_serial_char_timeout (int fd, int timeout_sec, int warn_timeout);
static int send_buf (int fd, unsigned char *buf, int n);
static int rcv_crc (unsigned char *buf, int n);

static int davis_model = DAVIS_VANTAGE_PRO; /* Davis station model to use */

/* CCITT CRC table */
static unsigned short crc_table [] = {
       0x0,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,  
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,  
    0x1231,   0x210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,  
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,  
    0x2462,  0x3443,   0x420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,  
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,  
    0x3653,  0x2672,  0x1611,   0x630,  0x76d7,  0x66f6,  0x5695,  0x46b4,  
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,  
    0x48c4,  0x58e5,  0x6886,  0x78a7,   0x840,  0x1861,  0x2802,  0x3823,  
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,  
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,   0xa50,  0x3a33,  0x2a12,  
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,  
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,   0xc60,  0x1c41,  
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,  
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0xe70,  
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,  
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,  
    0x1080,    0xa1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,  
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,  
     0x2b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,  
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,  
    0x34e2,  0x24c3,  0x14a0,   0x481,  0x7466,  0x6447,  0x5424,  0x4405,  
    0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,  
    0x26d3,  0x36f2,   0x691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,  
    0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,  
    0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,   0x8e1,  0x3882,  0x28a3,  
    0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,  
    0x4a75,  0x5a54,  0x6a37,  0x7a16,   0xaf1,  0x1ad0,  0x2ab3,  0x3a92,  
    0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,  
    0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,   0xcc1,  
    0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,  
    0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,   0xed1,  0x1ef0,  
};

/* External interface for setting weather station model */
void setDavisModel(int model_id)
{
	davis_model = model_id;
}


/* read weather station on tty, filling in goodies.
 * exit if can not open tty or tty error, all other errors return -1.
 */
int
DUpdate (char *tty, WxStats *wp, double *tp, double *pp)
{
	static int dfd = -1;
	unsigned char rcv_buf[1024];
	int barcal;

	/* open connection if first time */
	if (dfd < 0)
	    dfd = openTTY (tty);

	/* read and process */
	if (readDavis (dfd, rcv_buf, &barcal) == 0) {
	    processDavis (rcv_buf, barcal, wp, tp, pp);
	    return (0);
	}
	return (-1);
}

/* open tty and return fd to connection, else exit */
static int
openTTY (char *tty)
{
	struct termios tio;
	int fd;

	fd = telopen (tty, O_RDWR|O_NONBLOCK);
	if (fd < 0) {
	    daemonLog ("%s: %s\n", tty, strerror(errno));
	    exit(1);
	}
	if (fcntl (fd, F_SETFL, 0 /* no NONBLOCK now */ ) < 0) {
	    daemonLog ("fctnl: %s: %s\n", tty, strerror(errno));
	    exit(1);
	}

	/* set line characteristics */
	memset (&tio, 0, sizeof(tio));
	tio.c_iflag = 0;
	tio.c_cflag = CS8 | CREAD | CLOCAL;
	tio.c_oflag = 0;
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	if (davis_model == DAVIS_WEATHER_MONITOR_II) {
		cfsetispeed (&tio, B2400);
	}
    else if (davis_model == DAVIS_VANTAGE_PRO) {
		cfsetispeed (&tio, B19200);
    }

	if (tcsetattr (fd, TCSANOW, &tio) < 0) {
	    daemonLog ("tcsetattr: %s: %s\n", tty, strerror(errno));
	    exit(1);
	}

	return (fd);
}


/* Weather Monitor II-specific implementation of readDavis() */
static int 
readDavisWM2(int fd, unsigned char *rcv_buf, int *barcalp)
{
	unsigned char xmt_buf[32];
	int n;

	/* send command to get Barometric calibration number */

	n = 0;
	xmt_buf[n++] = 'W';
	xmt_buf[n++] = 'R';
	xmt_buf[n++] = 'D';
	xmt_buf[n++] = 0x44;			/* read 4 nibbles from bank 1*/
	xmt_buf[n++] = 0x2c;			/* Read beginning at 2C */
	xmt_buf[n++] = 0x0d;			/* EOM */
	tcflush (fd, TCIOFLUSH);		/* fresh stuff */
	if (send_buf(fd, xmt_buf, n) < 0)	/* send */
	    return (-1);

	/* read back */
	if (get_acknowledge(fd) < 0)		/* get back ACK */
	    return (-1);
	for (n = 0; n < 2; n++) {		/* signed barometer offset */
	    int d = get_serial_char(fd);
	    if (d < 0)
		return (-1);
	    rcv_buf[n] = d;
	}
	*barcalp = *(short *)rcv_buf;

	/* send command to get back one packet of info */

	n = 0;
	xmt_buf[n++] = 'L';
	xmt_buf[n++] = 'O';
	xmt_buf[n++] = 'O';
	xmt_buf[n++] = 'P';
	xmt_buf[n++] = 0xff;			/* 65535 lo byte */
	xmt_buf[n++] = 0xff;			/* 65535 hi byte */
	xmt_buf[n++] = 0x0d;			/* EOM */
	tcflush (fd, TCIOFLUSH);		/* fresh stuff */
	if (send_buf(fd, xmt_buf, n) < 0)	/* send */
	    return (-1);

	/* read back */
	if (get_acknowledge(fd) < 0)		/* get back ACK */
	    return (-1);
	if (get_serial_char(fd) != 1)		/* get block no, must be 1 */
	    return (-1);
	for (n = 0; n < 17; n++) {		/* 15 data, 2 CRC */ 
	    int d = get_serial_char(fd);
	    if (d < 0)
		return (-1);
	    rcv_buf[n] = d;
	}
	if (rcv_crc(rcv_buf, 17) < 0)
	    return (-1);

	/* ok to process */
	return (0);
}


/* Vantage Pro-specific implementation of readDavis() */
static int 
readDavisVP(int fd, unsigned char *rcv_buf, int *barcalp)
{
        unsigned char xmt_buf[32];
        int n;
        int model;
        int response;
        int attempt, success;

        /* Send command to wake up the weather station. After a LOOP
           command, this may take two tries.
           Send: '\n'
           Expect: '\n\r'
        */

        success = 0;
        for (attempt = 0; attempt < 3; attempt++) {
                n = 0;
                xmt_buf[n++] = '\n';
                tcflush(fd, TCIOFLUSH);
                if (send_buf(fd, xmt_buf, n) < 0) {
                        return (-1);
                }

		/* Since it may take multiple attempts to wake up the device
		   after a LOOP command even under normal operation,
		   don't wait too long to time out or log the error 
		   immediately */
                response = get_serial_char_timeout(fd, 1, 0);
                if (response < 0) {
                        continue;
                }
                if (response != '\n') {
                        daemonLog("Expected LF; got %02X", response);
                        continue;
                }

                response = get_serial_char(fd);
                if (response < 0) {
                        continue;
                }
                if (response != '\r') {
                        daemonLog("Expected CR; got %02X", response);
                        continue;
                }
                success = 1;
                break;
        }

        if (! success) {
                daemonLog("Unable to wake up weather station");
                return (-1);
        }

        /* Read and check model identification information */
        n = 0;
        xmt_buf[n++] = 'W';
        xmt_buf[n++] = 'R';
        xmt_buf[n++] = 'D';
        xmt_buf[n++] = 0x12;                    /* read 4 nibbles from bank 1*/
        xmt_buf[n++] = 0x4d;                    /* Read beginning at 2C */
        xmt_buf[n++] = '\n';                    /* EOM */
        tcflush (fd, TCIOFLUSH);                /* fresh stuff */
        if (send_buf(fd, xmt_buf, n) < 0)       /* send */
            return (-1);

        /* read back */
        if (get_acknowledge(fd) < 0)            /* get back ACK */
            return (-1);

        model = get_serial_char(fd);
        if (model < 0) { // Timed out
                return (-1);
        }
        if (model != 16) {
                daemonLog("Incorrect Davis model ID. Expected 16, got %d",
                          model);
                return(-1);
        }

        *barcalp = 0; /* The Proline may not need this (TODO - check) */


        /* send command to get back one packet of info */

        n = 0;
        xmt_buf[n++] = 'L';
        xmt_buf[n++] = 'O';
        xmt_buf[n++] = 'O';
        xmt_buf[n++] = 'P';
        xmt_buf[n++] = ' ';
        xmt_buf[n++] = '1';                     /* Get one loop packet */
        xmt_buf[n++] = '\n';                    /* EOM */
        tcflush (fd, TCIOFLUSH);                /* flush in/out buffers */
        if (send_buf(fd, xmt_buf, n) < 0)       /* send */
            return (-1);

        /* read back */
        if (get_acknowledge(fd) < 0)            /* get back ACK */
            return (-1);
        for (n = 0; n < 99; n++) {              /* 97 data, 2 CRC */
            int d = get_serial_char(fd);
            if (d < 0)
                return (-1);
            rcv_buf[n] = d;
        }
        if (rcv_crc(rcv_buf, 99) < 0)
            return (-1);

        /* ok to process */
        return (0);
}


/* issue a LOOP command and read back the response in rcv_buf.
 * also fill in barcalp with barometer calibration value.
 * return 0 if ok, else -1
 */
static int
readDavis (int fd, unsigned char *rcv_buf, int *barcalp)
{
	if (davis_model == DAVIS_WEATHER_MONITOR_II) {
		return readDavisWM2(fd, rcv_buf, barcalp);
	}
	else if (davis_model == DAVIS_VANTAGE_PRO) {
		return readDavisVP(fd, rcv_buf, barcalp);
	}
	else {
		daemonLog("Invalid Davis device model: %d", davis_model);
		return(-1);
	}
}

/* process info from rcv_buf[] */
static void
processDavis (unsigned char *rcv_buf, int barcal, WxStats *wp, double *tp,
double *pp)
{
	double outside_temperature;
	double wind_speed;
	double wind_direction;
	double barometer;
	double outside_humidity;
	double rain;

	if (davis_model == DAVIS_WEATHER_MONITOR_II) {
		outside_temperature =  *((short *)(rcv_buf+2))/10.;
		wind_speed =  *((unsigned char *)(rcv_buf+4));
		wind_direction =  *((short *)(rcv_buf+5));
		barometer =  (*((short *)(rcv_buf+7)) - barcal)/1000.;
		outside_humidity = *((unsigned char*)(rcv_buf+10));
		rain =  *((short *)(rcv_buf+11))/100.;
	}
	else { /* DAVIS_VANTAGE_PRO */
		outside_temperature =  *((short *)(rcv_buf+12))/10.;
		wind_speed =  *((unsigned char *)(rcv_buf+14));
		wind_direction =  *((short *)(rcv_buf+16));
		barometer =  (*((short *)(rcv_buf+7)) - barcal)/1000.;
		outside_humidity = *((unsigned char*)(rcv_buf+33));
		rain =  *((short *)(rcv_buf+50))/100.;
	}

	wp->wspeed = wind_speed*1.609 + .5;	/* MPH -> KPH */
	wp->wdir = wind_direction;		/* degs E of N */
	wp->humidity = outside_humidity;	/* % */
	wp->rain = rain*254;			/* in -> .1mm */
	*tp = 5*(outside_temperature-32)/9;	/* F -> C */
	*pp = barometer*33.86;			/* in -> mBar */
}

/* compute the CRC for n bytes of rcv_buf and return 0 if ok, else -1
 */
static int
rcv_crc (unsigned char *rcv_buf, int n)
{
	unsigned short crc;
	int i;

	crc = 0;
	for (i = 0; i < n; i++)
	    crc =  crc_table [(crc >> 8) ^ rcv_buf[i]] ^ (crc << 8);
	return (crc ? -1 : 0);
}

/* send first n chars of buf to fd.
 * return 0 if ok else -1
 */
static int
send_buf (int fd, unsigned char *buf, int n)
{
	int w;

	if ((w = write (fd, buf, n)) != n) {
	    if (w < 0)
		daemonLog ("write error: %s\n", strerror(errno));
	    else 
		daemonLog ("short write: %d vs. %d\n", w, n);
	    exit(1);
	}

	return (0);
}

/* read one char from fd, return 0 if it is ACK else -1
 */
static int
get_acknowledge(int fd)
{
	int i;

	if ((i = get_serial_char(fd)) != 6)
	    return (-1);
	return (0);
}
   
/* Get a character from fd. Alias to get_serial_char_timeout with defaults.
 * return it or -1 if trouble
 */
static int
get_serial_char (int fd)
{
	return get_serial_char_timeout(fd, 4, 1);
}


/* Get a character from fd. Time out after timeout_sec seconds,
 * and log the timeout if warn_timeout is nonzero.
 * return it or -1 if trouble
 */
static int
get_serial_char_timeout (int fd, int timeout_sec, int warn_timeout)
{
	unsigned  char c;
	struct timeval tv;
	fd_set rfd;

	tv.tv_sec = timeout_sec;
	tv.tv_usec = 0;
	FD_ZERO (&rfd);
	FD_SET (fd, &rfd);

	if (select (fd+1, &rfd, NULL, NULL, &tv) <= 0) {
	    if (warn_timeout) {
	        daemonLog ("timeout\n");
	    }
	    return (-1);
	}
	if (read (fd, &c, 1) != 1) {
	    daemonLog ("get_serial_char error: %s\n", strerror(errno));
	    exit(1);
	}

	if (bflag)
	    daemonLog ("0x%02x\n", c);

	return (c);
}      

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: davis.c,v $ $Date: 2001/04/19 21:12:11 $ $Revision: 1.1.1.1 $ $Name:  $"};
