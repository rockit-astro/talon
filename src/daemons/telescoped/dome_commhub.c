#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "rot.h"
#include "misc.h"
#include "telenv.h"
#include "csimc.h"
#include "virmc.h"
#include "cliserv.h"
#include "tts.h"

#include "teled.h"  // will bring in buildcfg.h
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>

static int DOMEHAVE = 0;
static char DOMETTY[64];
static double DOMETO = 0;

/* handy shortcuts */
#define	DS		(telstatshmp->domestate)
#define	SS		(telstatshmp->shutterstate)
#define	AD		(telstatshmp->autodome)
#define	AZ		(telstatshmp->domeaz)
#define	TAZ		(telstatshmp->dometaz)
#define	SMOVING		(SS == SH_OPENING || SS == SH_CLOSING)
#define	DMOVING		(DS == DS_ROTATING || DS == DS_HOMING)
#define	DHAVE		(DS != DS_ABSENT)
#define	SHAVE		(SS != SH_ABSENT)

DShState SS_BEFORE = SH_IDLE;

#define PACKET_HEADER 0x55

enum command_enum
{
	COMMAND_NONE = '0',
	COMMAND_OPEN = '1',
	COMMAND_CLOSE = '2',
	COMMAND_STATUS = '3',
	COMMAND_LOCK = '4',
	COMMAND_UNLOCK = '5',
	COMMAND_GETLOCK = '6',
	COMMAND_LAST = '7'
};

enum status_enum
{
	STATUS_NONE = '0',
	STATUS_OPENED = '1',
	STATUS_CLOSED = '2',
	STATUS_OPENING = '3',
	STATUS_CLOSING = '4',
	STATUS_LOCKED = '5',
	STATUS_UNLOCKED = '6',
	STATUS_COMM_ERROR = '7',
	STATUS_DOME_ERROR = '8',
	STATUS_LAST = '9'
};

typedef struct
{
	char header;
	char command;
	char checksum;
} packet_t;

int fd = -1;
packet_t packet;

int commhub_dome_is_ready(void);

void commhub_send_packet(char command)
{
	char dat;
	char chk = 0;
	if (commhub_dome_is_ready() == 0)
		return;

	dat = PACKET_HEADER;
	chk ^= dat;
	write(fd, &dat, 1);

	dat = command;
	chk ^= dat;
	write(fd, &dat, 1);

	write(fd, &chk, 1);

	//	printf("send\t\t%X\t%X\t%X\n", PACKET_HEADER, command, chk);
}

char commhub_parse_packet(packet_t* pk)
{
	if (pk->header == PACKET_HEADER)
	{
		char chk = PACKET_HEADER;
		chk ^= pk->command;
		if (chk == pk->checksum)
		{
			if (((char) pk->command > (char) STATUS_NONE)
					&& ((char) pk->command < (char) STATUS_LAST))
			{
				//				printf("receive\t\t%X\t%X\t%X\n", PACKET_HEADER, pk->command, chk);

				return pk->command;
			}
		}
	}
	return STATUS_NONE;
}

char commhub_receive_packet()
{
	char ch;
	char status = STATUS_NONE;

	if (!commhub_dome_is_ready())
		return status;

	while (status == STATUS_NONE)
	{
		int ret = read(fd, &ch, 1);

		if (ret == 0)
			break;

		packet.header = (char) packet.command;
		packet.command = (char) packet.checksum;
		packet.checksum = (char) ch;
		status = (char) commhub_parse_packet(&packet);
	}
	return status;
}

int commhub_dome_is_ready(void)
{
	if (fd < 0)
	{
		//		printf("commhub_dome_is_ready NOT READY\n");
		return 0;
	}
	return 1;
}

int commhub_dome_setup(void)
{
	//	printf("commhub_dome_setup\n");

	struct termios tio;

	fd = -1;

	fd = open(DOMETTY, O_RDWR | O_NONBLOCK);
	if (fd < 0)
	{
		tdlog("open(%s): %s\n", DOMETTY, strerror(errno));
		return -1;
	}

	fcntl(fd, F_SETFL, 0); /* nonblock back off */

	memset(&tio, 0, sizeof(tio));
	tio.c_cflag = (CS8 | CREAD | CLOCAL) & (~PARENB);
	tio.c_iflag = IGNBRK;
	tio.c_cc[VMIN] = 0; /* start timer when call read() */
	tio.c_cc[VTIME] = ACKWT / 100; /* wait up to n 1/10ths seconds */
	cfsetospeed(&tio, B9600);
	cfsetispeed(&tio, B9600);
	if (tcsetattr(fd, TCSANOW, &tio) < 0)
	{
		tdlog("tcsetattr(%s): %s\n", DOMETTY, strerror(errno));
		return -1;
	}

	return fd;
}

void commhub_dome_reset()
{
	//	printf("commhub_dome_reset\n");
	if (commhub_dome_is_ready())
	{
		close(fd);
	}
	commhub_dome_setup();

	return;
}

char status = STATUS_NONE;
char lock_status = STATUS_LOCKED;
char error_status = STATUS_NONE;

char commhub_dome_status(void)
{
	char lock_status_before = lock_status;
	char error_status_before = error_status;
	error_status = STATUS_NONE;

	if (!commhub_dome_is_ready())
		return STATUS_NONE;

	//	printf("commhub_dome_status\n");

	commhub_send_packet(COMMAND_GETLOCK);
	commhub_send_packet(COMMAND_STATUS);

	do
	{
		status = commhub_receive_packet();

		if (status == STATUS_LOCKED || status == STATUS_UNLOCKED)
			lock_status = status;

		if (status == STATUS_OPENED)
			SS = SH_OPEN;
		else if (status == STATUS_OPENING)
			SS = SH_OPENING;
		else if (status == STATUS_CLOSED)
			SS = SH_CLOSED;
		else if (status == STATUS_CLOSING)
			SS = SH_CLOSING;
		else if (status == STATUS_DOME_ERROR)
		{
			SS = SH_ABSENT;
			error_status = STATUS_DOME_ERROR;
		}
		else if (status == STATUS_COMM_ERROR)
		{
			SS = SH_ABSENT;
			error_status = STATUS_COMM_ERROR;
		}

	} while (status != STATUS_NONE);

	//	DS = DS_ABSENT;

	if (SS == SH_OPEN && SS_BEFORE != SH_OPEN)
		fifoWrite(Dome_Id, -1, "open");
	else if (SS == SH_OPENING && SS_BEFORE != SH_OPENING)
		fifoWrite(Dome_Id, -1, "opening");
	else if (SS == SH_CLOSED && SS_BEFORE != SH_CLOSED)
		fifoWrite(Dome_Id, -1, "closed");
	else if (SS == SH_CLOSING && SS_BEFORE != SH_CLOSING)
		fifoWrite(Dome_Id, -1, "closing");


	if (error_status == STATUS_DOME_ERROR && error_status_before != STATUS_DOME_ERROR)
	{
		fifoWrite(Dome_Id, -1, "dome error");
	}
	else if (error_status == STATUS_COMM_ERROR && error_status_before != STATUS_COMM_ERROR)
	{
		fifoWrite(Dome_Id, -1, "communication error");
	}


	if (lock_status == STATUS_LOCKED && lock_status_before != STATUS_LOCKED)
		fifoWrite(Dome_Id, -1, "locked by redundant control");
	else if (lock_status == STATUS_UNLOCKED && lock_status_before != STATUS_UNLOCKED)
		fifoWrite(Dome_Id, -1, "unlocked by redundant control");

	SS_BEFORE = SS;

	return status;
}

void commhub_dome_poll(void)
{
	time_t now;
	static time_t lastTime;

	now = time(NULL);
	if (now != lastTime)
	{
		commhub_dome_status();
		lastTime = now;
	}
}

void commhub_dome_open(void)
{
	if (commhub_dome_is_ready())
	{
		//		printf("commhub_dome_open\n");
		commhub_send_packet(COMMAND_OPEN);
	}
	return;
}

void commhub_dome_close(void)
{
	if (commhub_dome_is_ready())
	{
		//		printf("commhub_dome_close\n");
		commhub_send_packet(COMMAND_CLOSE);
	}
	return;
}

void commhub_dome_stop(void)
{
	//	printf("commhub_dome_stop\n");
	if (commhub_dome_is_ready())
	{
		close(fd);
	}
	return;
}

static void commhub_init_config(void)
{
#define NCHDCFG   (sizeof(chdcfg)/sizeof(chdcfg[0]))

	static CfgEntry chdcfg[] =
	{
	{ "DOMEHAVE", CFG_INT, &DOMEHAVE },
	{ "DOMETTY", CFG_STR, &DOMETTY, sizeof(DOMETTY) },
	{ "DOMETO", CFG_DBL, &DOMETO }, };

	int n;

	/* read the file */
	n = readCfgFile(1, chdcfn, chdcfg, NCHDCFG);
	if (n != NCHDCFG)
	{
		cfgFileError(chdcfn, n, (CfgPrFp) tdlog, chdcfg, NCHDCFG);
		die();
	}

	/* we want in days */
	DOMETO /= SPD;

	SS_BEFORE = SH_IDLE;
}

/* called when we receive a message from the Dome fifo plus periodically with
 *   !msg to just update things.
 */
/* ARGSUSED */
void commhub_dome_msg(msg)
	char *msg;
{
	/* do reset before checking for `have' to allow for new config file */
	if (msg && strncasecmp(msg, "reset", 5) == 0)
	{
		commhub_init_config();
		if (DOMEHAVE)
			commhub_dome_reset();
		return;
	}

	if (DOMEHAVE == 0)
		return;

	if (!commhub_dome_is_ready() && msg)
	{
		tdlog("Dome command before initial Reset: %s", msg ? msg : "(NULL)");
		return;
	}

	/* handle normal messages and polling */
	if (!msg)
		commhub_dome_poll();
	else if (strncasecmp(msg, "stop", 4) == 0)
		commhub_dome_stop();
	else if (strncasecmp(msg, "open", 4) == 0)
		commhub_dome_open();
	else if (strncasecmp(msg, "close", 5) == 0)
		commhub_dome_close();
	else
	{
		fifoWrite(Dome_Id, -1, "Unknown command: %.20s", msg);
		commhub_dome_stop(); /* default for any unrecognized message */
	}

}

