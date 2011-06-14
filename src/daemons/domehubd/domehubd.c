/*
 * CommHubDummy.c
 *
 *  Created on: Jun 8, 2011
 *      Author: luis
 */

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
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

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

char* dome_tty = NULL;
int fd = -1;
packet_t packet;

char status = STATUS_NONE;
char lock_status = STATUS_LOCKED;
char error_status = STATUS_NONE;
char SS;
char SS_BEFORE = STATUS_NONE;

int fdin;
int fdout;
FILE* fin;
FILE* fout;
const char telhome_def[] = "/usr/local/telescope/";
const char fin_def[] = "comm/DomeRedundant.in";
const char fout_def[] = "comm/DomeRedundant.out";
char fin_path[4096];
char fout_path[4096];
char in_line[128];

int commhub_printf(char* format, ...)
{
	va_list args;
	int retval = printf(format, args);
	fflush(stdout);

	if (fout != NULL)
	{
		fprintf(fout, format, args);
		fflush(fout);
	}

	return retval;
}


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

	fd = open(dome_tty, O_RDWR | O_NONBLOCK);
	if (fd < 0)
	{
		printf("open(%s): %s\n", dome_tty, strerror(errno));
		return -1;
	}

	fcntl(fd, F_SETFL, 0); /* nonblock back off */

	memset(&tio, 0, sizeof(tio));
	tio.c_cflag = (CS8 | CREAD | CLOCAL) & (~PARENB);
	tio.c_iflag = IGNBRK;
	tio.c_cc[VMIN] = 0; /* start timer when call read() */
	tio.c_cc[VTIME] = 10; /* wait up to n 1/10ths seconds */
	cfsetospeed(&tio, B9600);
	cfsetispeed(&tio, B9600);
	if (tcsetattr(fd, TCSANOW, &tio) < 0)
	{
		printf("tcsetattr(%s): %s\n", dome_tty, strerror(errno));
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

	commhub_printf("reset\n");

	return;
}

char commhub_dome_status(void)
{
	char return_status;
	char lock_status_before = lock_status;
	char error_status_before = error_status;
	error_status = STATUS_NONE;

	if (!commhub_dome_is_ready())
	{
		SS = STATUS_NONE;
		return STATUS_NONE;
	}

	//	printf("commhub_dome_status\n");

	commhub_send_packet(COMMAND_GETLOCK);
	commhub_send_packet(COMMAND_STATUS);

	do
	{
		status = commhub_receive_packet();
		if (status != STATUS_NONE)
			return_status = status;

		if (status == STATUS_LOCKED || status == STATUS_UNLOCKED)
			lock_status = status;

		if (status == STATUS_OPENED)
			SS = STATUS_OPENED;
		else if (status == STATUS_OPENING)
			SS = STATUS_OPENING;
		else if (status == STATUS_CLOSED)
			SS = STATUS_CLOSED;
		else if (status == STATUS_CLOSING)
			SS = STATUS_CLOSING;
		else if (status == STATUS_DOME_ERROR)
		{
			SS = STATUS_DOME_ERROR;
			error_status = STATUS_DOME_ERROR;
		}
		else if (status == STATUS_COMM_ERROR)
		{
			SS = STATUS_COMM_ERROR;
			error_status = STATUS_COMM_ERROR;
		}

	} while (status != STATUS_NONE);

	if (SS == STATUS_OPENED && SS_BEFORE != STATUS_OPENED)
		commhub_printf("open\n");
	else if (SS == STATUS_OPENING && SS_BEFORE != STATUS_OPENING)
		commhub_printf("opening\n");
	else if (SS == STATUS_CLOSED && SS_BEFORE != STATUS_CLOSED)
		commhub_printf("closed\n");
	else if (SS == STATUS_CLOSING && SS_BEFORE != STATUS_CLOSING)
		commhub_printf("closing\n");

	if (error_status == STATUS_DOME_ERROR && error_status_before
			!= STATUS_DOME_ERROR)
	{
		commhub_printf("dome error\n");
	}
	else if (error_status == STATUS_COMM_ERROR && error_status_before
			!= STATUS_COMM_ERROR)
	{
		commhub_printf("communication error\n");
	}

	if (lock_status == STATUS_LOCKED && lock_status_before != STATUS_LOCKED)
		commhub_printf("locked\n");
	else if (lock_status == STATUS_UNLOCKED && lock_status_before
			!= STATUS_UNLOCKED)
		commhub_printf("unlocked\n");

	SS_BEFORE = SS;

	return return_status;
}

void commhub_dome_poll(void)
{
	char status;
	time_t now;
	static time_t lastTime;

	now = time(NULL);
	if (now != lastTime)
	{
		status = commhub_dome_status();

		if (status > STATUS_NONE && status < STATUS_LAST)
		{
			lastTime = now;
		}
		if ((now - lastTime) > 2)
		{
			commhub_dome_reset();
			lastTime = now;
		}
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

void commhub_dome_lock(int state)
{
	if (state == 0)
	{
		commhub_send_packet(COMMAND_UNLOCK);
		commhub_printf("unlocking\n");
	}
	else
	{
		commhub_send_packet(COMMAND_LOCK);
		commhub_printf("locking\n");
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


char* get_TELHOME()
{
	char* telhome = NULL;
	telhome = getenv("TELHOME");
	if (telhome == NULL)
		telhome = (char*) telhome_def;
	return telhome;
}

int create_fin(void)
{
	strcpy(fin_path, get_TELHOME());
	strcat(fin_path, "/");
	strcat(fin_path, fin_def);

	unlink(fin_path);

	int err = mkfifo(fin_path, 0660);
	if (err < 0)
	{
		printf("Error creating fifo %s (errno = %d)\n", fin_path, err);
		return (-1);
	}
	return 0;
}

int create_fout(void)
{
	strcpy(fout_path, get_TELHOME());
	strcat(fout_path, "/");
	strcat(fout_path, fout_def);

	unlink(fout_path);

	int err = mkfifo(fout_path, 0660);
	if (err < 0)
	{
		printf("Error creating fifo %s\n (errno = %d)", fout_path, err);
		return (-1);
	}
	return 0;
}

void on_term(int dummy)
{
	printf("\n");

	close(fdin);
	printf("Close fifo %s\n", fin_path);

	close(fdout);
	printf("Close fifo %s\n", fout_path);

	unlink(fin_path);
	unlink(fout_path);

	exit(0);
}



int main(int argc, char* argv[])
{
	signal(SIGTERM, on_term);
	signal(SIGQUIT, on_term);
	signal(SIGINT, on_term);
	signal(SIGKILL, on_term);

	if (create_fin() == -1)
		return -1;
	if (create_fout() == -1)
		return -1;

	fdin = open(fin_path, O_RDWR | O_NONBLOCK);
	if (fdin == -1)
	{
		close(fdin);
		printf("Error opening fifo %s\n", fin_path);
		return -1;
	}
	fin = fdopen(fdin, "r");
	if (fin == NULL)
	{
		fclose(fin);
		printf("Error opening fifo %s\n", fin_path);
		return -1;
	}
	printf("Open fifo %s\n", fin_path);

	fdout = open(fout_path, O_RDWR | O_NONBLOCK);
	if (fdout == -1)
	{
		close(fdout);
		printf("Error opening fifo %s\n", fout_path);
		return -1;
	}
	fout = fdopen(fdout, "w");
	if (fout == NULL)
	{
		fclose(fout);
		printf("Error opening fifo %s\n", fout_path);
		return -1;
	}
	printf("Open fifo %s\n", fout_path);

	dome_tty = argv[1];

	while (1)
	{
		if (fgets(in_line, sizeof(in_line) - 1, fin) != NULL)
		{
			printf("in_line = %s", in_line);

			if (strcmp("open\n", in_line) == 0)
			{
				commhub_dome_open();
			}
			else if (strcmp("close\n", in_line) == 0)
			{
				commhub_dome_close();
			}
			else if (strcmp("status\n", in_line) == 0)
			{
				SS_BEFORE = STATUS_NONE;
				commhub_dome_status();
			}
			else if (strcmp("lock\n", in_line) == 0)
			{
				commhub_dome_lock(1);
			}
			else if (strcmp("unlock\n", in_line) == 0)
			{
				commhub_dome_lock(0);
			}
		}

		commhub_dome_poll();
		usleep(100000);
	}

	on_term(0);
	return 0;
}
