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

int fdin;
int fdout;
int fdtel;
FILE* fin;
FILE* fout;
FILE* ftel;
const char telhome_def[] = "/usr/local/telescope/";
const char fin_def[] = "comm/autoguide.in";
const char fout_def[] = "comm/autoguide.out";
const char ftel_def[] = "comm/telescope.in";
char fin_path[4096];
char fout_path[4096];
char ftel_path[4096];
char in_line[128];

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

int create_fifos(void)
{
	if (create_fin() == -1)
		return -1;
	if (create_fout() == -1)
		return -1;
	return 0;
}

int open_fifo(void)
{
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


	fdtel = open(ftel_path, O_RDWR | O_NONBLOCK);
	if (fdtel == -1)
	{
		close(fdtel);
		printf("Error opening fifo %s\n", ftel_path);
		return -1;
	}
	ftel = fdopen(fdtel, "w");
	if (fout == NULL)
	{
		fclose(ftel);
		printf("Error opening fifo %s\n", ftel_path);
		return -1;
	}
	printf("Open fifo %s\n", ftel_path);

	return;
}

int main(int argc, char* argv[])
{
	printf("\nXTRACK Daemon v0.0.0\n\n");

	if (create_fifos() == -1)
	{
		printf("Error creating in/out fifos\n");
		exit(1);
	}

	if (open_fifo() == -1)
	{
		printf("Error opnening in/out fifos\n");
		exit(1);
	}

	if (file_notify(argv[1]) == 1)
	{
		printf("Error file inotify %s\n", argv[1]);
		exit(1);
	}

	signal(SIGTERM, on_term);
	signal(SIGQUIT, on_term);
	signal(SIGINT, on_term);
	signal(SIGKILL, on_term);

	while (1)
	{

		if (fgets(in_line, sizeof(in_line) - 1, fin) != NULL)
		{
			printf("in_line = %s", in_line);

			if (strcmp("open\n", in_line) == 0)
			{
				printf("openxxxx\n");
			}

		}

		usleep(10000);
	}

	on_term(0);
	return 0;
}

