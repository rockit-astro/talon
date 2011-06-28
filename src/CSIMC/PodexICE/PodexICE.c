/* operate a PODEX MODIFIED...!!!!  Kevin Ross BDM12 Pod from Linux.
 * run with -help for command line args.
 * illegal commands give command summary.
 *
 * cable:
 *  Computer: DB-9 female          Pod: DB-9 male     Computer: DB25 female
 *
 *  1-4-6                                                            8-20-6
 *  2      --------------------------    2    ----------------------      3
 *  3      --------------------------    3    ----------------------      2
 *  5      --------------------------    5    ----------------------      7
 *  8      --------------------------    8    ----------------------      5
 *
 * GPL
 * Copyright (c) 1999 Elwood Charles Downey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <linux/serial.h>

static char deftty[] = "/dev/ttyUSB0"; 	//PODEX_ICE "/dev/ttyS0"; /* default tty */
#define	PODSPEED	B57600 		//PODEX_ICE B115200		/* pod speed */
#define	NCOLS		5		/* mnemonic table columns */
#define	MAXFWMS		1000		/* max FLASH write ms */

static char rev[] = "$Revision: 1.1.1.1 $"; /* RCS will fill in */

typedef unsigned char byte;
typedef unsigned short word;

typedef enum
{
	LT_RAM, LT_EE, LT_FLASH
} LoadType;

static void usage(char *pname);
static void showCmds(void);
static void openPod(void);
static void resetPod(void);
static void prStbl(int byaddr);
static void runFile(FILE *fp);
static void processCmd(char buf[]);
static void printBytes(int addr0, int addr1);
static void printWords(int addr0, int addr1);
static void printRegs(void);
static void writeByte(int addr, int data);
static void writeWord(int addr, int data);
static int writeFlashW(int addr, int data);
static int writeFlashB(int addr, int data);
static void writeEEW(int addr, int data);
static void writeEEB(int addr, int data);
static void fillRAM(int a0, int a1, int data);
static void loadSFile(char name[], LoadType lt, int verify);
static int load1S(int addr, char buf[], int nleft, LoadType lt, int verify);
static int verifyRAM(int addr, int data);
static void watchByte(int addr);
static void watchWord(int addr);
static void readByte(int addr, int *bp);
static void readWord(int addr, int *bp);
static void bdmFirmware(void);
static void bdmMode(void);
static void resetCPU(int bkgd);
static void trace(int n);
static void wait4Flash(void);
static void bulkEEE(int addr);
static void bulkEFlash(int addr);
static void loadReg(char regid, int data);
static void go(int addr);
static void readPodByte(byte *bp);
static void writePodByte(byte b);
static void writePodBuf(int addr, char str[], int n);
static void loadFile(char *fn);
//PODEX_ICE static void usDelay(int us);
#define usDelay(x) usleep(x)
//PODEX_ICE
static void onIntr(int sig);
static int trySymFile(char *name, int *addr);
static void tryMapFile(int pc);

static char *podtty = deftty;
static int podfd = -1;
//PODEX_ICE
static unsigned ctsto = 0;
//PODEX_ICE
static int nintr;

/* find ms from t0 to t1 */
#define	dms(t1p,t0p)							\
			    (1000*((t1p)->tv_sec - (t0p)->tv_sec) +	\
			    ((t1p)->tv_usec - (t0p)->tv_usec)/1000)

typedef struct
{
	char *name;
	int addr;
} SymTab;

static SymTab stbl[] =
{
{ "PORTA", 0x00 },
{ "PORTB", 0x01 },
{ "DDRA", 0x02 },
{ "DDRB", 0x03 },
{ "PORTC", 0x04 },
{ "PORTD", 0x05 },
{ "DDRC", 0x06 },
{ "DDRD", 0x07 },
{ "PORTE", 0x08 },
{ "DDRE", 0x09 },
{ "PEAR", 0x0A },
{ "MODE", 0x0B },
{ "PUCR", 0x0C },
{ "RDRIV", 0x0D },
{ "INITRM", 0x10 },
{ "INITRG", 0x11 },
{ "INITEE", 0x12 },
{ "MISC", 0x13 },
{ "RTICTL", 0x14 },
{ "RTIFLG", 0x15 },
{ "COPCTL", 0x16 },
{ "COPRST", 0x17 },
{ "ITST0", 0x18 },
{ "ITST1", 0x19 },
{ "ITST2", 0x1A },
{ "ITST3", 0x1B },
{ "INTCR", 0x1E },
{ "HPRIO", 0x1F },
{ "KWIED", 0x20 },
{ "KWIFD", 0x21 },
{ "PORTH", 0x24 },
{ "DDRH", 0x25 },
{ "KWIEH", 0x26 },
{ "KWIFH", 0x27 },
{ "PORTJ", 0x28 },
{ "DDRJ", 0x29 },
{ "KWIEJ", 0x2A },
{ "KWIFJ", 0x2B },
{ "KPOLJ", 0x2C },
{ "PUPSJ", 0x2D },
{ "PULEJ", 0x2E },
{ "PORTF", 0x30 },
{ "PORTG", 0x31 },
{ "DDRF", 0x32 },
{ "DDRG", 0x33 },
{ "DPAGE", 0x34 },
{ "PPAGE", 0x35 },
{ "EPAGE", 0x36 },
{ "WINDEF", 0x37 },
{ "MXAR", 0x38 },
{ "CSCTL0", 0x3C },
{ "CSCTL1", 0x3D },
{ "CSSTR0", 0x3E },
{ "CSSTR1", 0x3F },
{ "CLKCTL", 0x47 },
{ "ATDCTL0", 0x60 },
{ "ATDCTL1", 0x61 },
{ "ATDCTL2", 0x62 },
{ "ATDCTL3", 0x63 },
{ "ATDCTL4", 0x64 },
{ "ATDCTL5", 0x65 },
{ "ATDSTAT", 0x66 },
{ "ATDTEST", 0x68 },
{ "PORTAD", 0x6F },
{ "ADR0H", 0x70 },
{ "ADR1H", 0x72 },
{ "ADR2H", 0x74 },
{ "ADR3H", 0x76 },
{ "ADR4H", 0x78 },
{ "ADR5H", 0x7A },
{ "ADR6H", 0x7C },
{ "ADR7H", 0x7E },
{ "TIOS", 0x80 },
{ "CFORC", 0x81 },
{ "OC7M", 0x82 },
{ "OC7D", 0x83 },
{ "TCNT", 0x84 },
{ "TSCR", 0x86 },
{ "TQCR", 0x87 },
{ "TCTL1", 0x88 },
{ "TCTL2", 0x89 },
{ "TCTL3", 0x8A },
{ "TCTL4", 0x8B },
{ "TMSK1", 0x8C },
{ "TMSK2", 0x8D },
{ "TFLG1", 0x8E },
{ "TFLG2", 0x8F },
{ "TC0", 0x90 },
{ "TC1", 0x92 },
{ "TC2", 0x94 },
{ "TC3", 0x96 },
{ "TC4", 0x98 },
{ "TC5", 0x9A },
{ "TC6", 0x9C },
{ "TC7", 0x9E },
{ "PACTL", 0xA0 },
{ "PAFLG", 0xA1 },
{ "PACNT", 0xA2 },
{ "TIMTST", 0xAD },
{ "PORTT", 0xAE },
{ "DDRT", 0xAF },
{ "SC0BD", 0xC0 },
{ "SC0BDH", 0xC0 },
{ "SC0BDL", 0xC1 },
{ "SC0CR1", 0xC2 },
{ "SC0CR2", 0xC3 },
{ "SC0SR1", 0xC4 },
{ "SC0SR2", 0xC5 },
{ "SC0DRH", 0xC6 },
{ "SC0DRL", 0xC7 },
{ "SC1BD", 0xC8 },
{ "SC1BDH", 0xC8 },
{ "SC1BDL", 0xC9 },
{ "SC1CR1", 0xCA },
{ "SC1CR2", 0xCB },
{ "SC1SR1", 0xCC },
{ "SC1SR2", 0xCD },
{ "SC1DRH", 0xCE },
{ "SC1DRL", 0xCF },
{ "SP0CR1", 0xD0 },
{ "SP0CR2", 0xD1 },
{ "SP0BR", 0xD2 },
{ "SP0SR", 0xD3 },
{ "SP0DR", 0xD5 },
{ "PORTS", 0xD6 },
{ "DDRS", 0xD7 },
{ "EEMCR", 0xF0 },
{ "EEPROT", 0xF1 },
{ "EETST", 0xF2 },
{ "EEPROG", 0xF3 }, };

#define	NSTBL	(sizeof(stbl)/sizeof(stbl[0]))

static char *symfile;
static char *mapfile;

int main(int ac, char *av[])
{
	char *pname = av[0];

	while ((--ac > 0) && ((*++av)[0] == '-'))
	{
		char *s;
		for (s = av[0] + 1; *s != '\0'; s++)
			switch (*s)
			{
			case 'c':
				if (ac < 2)
					usage(pname);
				ctsto = atol(*++av);
				//PODEX_ICE
				if (ctsto < 0) ctsto = 0;
				//PODEX_ICE
				ac--;
				break;
			case 'l':
				if (ac < 2)
					usage(pname);
				symfile = *++av;
				if (access(symfile, R_OK) < 0)
				{
					perror(symfile);
					exit(1);
				}
				ac--;
				break;
			case 'm':
				if (ac < 2)
					usage(pname);
				mapfile = *++av;
				if (access(mapfile, R_OK) < 0)
				{
					perror(mapfile);
					exit(1);
				}
				ac--;
				break;
			case 't':
				if (ac < 2)
					usage(pname);
				podtty = *++av;
				ac--;
				break;
			default:
				usage(pname);
				break;
			}
	}

	/* ac remaining args starting at av[0] */
	if (ac > 0)
		usage(pname);

	/* no buffering */
	setbuf(stdout, NULL);

	/* who are we? */
	printf("%s: %.*s\n", pname, (int) strlen(rev) - 2, &rev[1]);

	/* init the pod connection */
	resetPod();

	/* catch SIGINT */
	signal(SIGINT, onIntr);

	/* main loop */
	runFile(stdin);
	return (0);
}

static void usage(char *pname)
{
	fprintf(stderr, "Usage: %s [options]\n", pname);
	fprintf(stderr, "Purpose: command line interface to USB PodexICE.\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, " -c n   : transfer delay (microseconds) instead of polling CTS\n");
	fprintf(stderr, " -t tty : set pod line to <tty>. default is %s\n", deftty);
	fprintf(stderr, " -l file: look up symbols in Imagecraft <file>.lst\n");
	fprintf(stderr, " -m file: look up PC in Imagecraft <file>.mp\n");

	exit(1);
}

/* increment nintr on each ctrl-c.
 * used to allow stopping long commands.
 * if two in a row, we die as though we didn't catch anything.
 */
static void onIntr(int sig)
{
	if (nintr++)
		exit(0);
}

/* open podtty and set podfd.
 * exit if fail.
 */
static void openPod(void)
{
	struct termios tio;

	podfd = open(podtty, O_RDWR);
	if (podfd < 0)
	{
		perror(podtty);
		exit(1);
	}

	memset(&tio, 0, sizeof(tio));
	tio.c_iflag = 0;
	tio.c_cflag = CS8 | CREAD | CRTSCTS;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	cfsetospeed(&tio, PODSPEED);
	cfsetispeed(&tio, PODSPEED);

	if (tcsetattr(podfd, TCSANOW, &tio) < 0)
	{
		perror(podtty);
		exit(1);
	}
}

//PODEX_ICE
int setRTS(int fd, int level)
{
	int status;

	if (ioctl(fd, TIOCMGET, &status) == -1)
	{
		perror("setRTS(): TIOCMGET");
		return 0;
	}
	if (level)
		status |= TIOCM_RTS;
	else
		status &= ~TIOCM_RTS;
	if (ioctl(fd, TIOCMSET, &status) == -1)
	{
		perror("setRTS(): TIOCMSET");
		return 0;
	}
	return 1;
}
//PODEX_ICE


/* reset pod and insure in translate mode */
static void resetPod()
{
	int i;

	/* seems to help to reset tty too */
	if (podfd != -1)
		close(podfd);
	openPod();

	setRTS(podfd, 0); //PODEX_ICE Abort any pending pod_rx
	setRTS(podfd, 1); //PODEX_ICE

	for (i = 0; i < 6; i++)
		writePodByte(0);
}

/* qsort-style function to sort SymTab in increasing order by address */
static int a_qs(const void *p1, const void *p2)
{
	int a1 = ((SymTab*) p1)->addr;
	int a2 = ((SymTab*) p2)->addr;

	return (a1 - a2);
}

/* qsort-style function to sort SymTab in increasing order by name */
static int n_qs(const void *p1, const void *p2)
{
	char *n1 = ((SymTab*) p1)->name;
	char *n2 = ((SymTab*) p2)->name;

	return (strcmp(n1, n2));
}

/* print stbl[] sorted by address or name */
static void prStbl(int byaddr)
{
	int i;
	SymTab tmp[NSTBL];

	memcpy(tmp, stbl, sizeof(tmp));

	qsort(tmp, NSTBL, sizeof(SymTab), byaddr ? a_qs : n_qs);

	for (i = 0; i < NSTBL; i++)
	{
		int n = (i % NCOLS) * NSTBL / NCOLS + i / NCOLS;
		printf(" | %-7s %02X |", tmp[n].name, tmp[n].addr);
		if ((i % NCOLS) == (NCOLS - 1))
			printf("\n");
	}
	printf("\n");
}

/* show list of commands */
static void showCmds(void)
{
	printf("r\t\t\tdump registers\n");
	printf("R  reg word\t\tload register with word; reg = {pdxys}\n");
	printf("\n");
	printf("d  addr [n]\t\tdump [n] byte(s) beginning at addr\n");
	printf("D  addr [n]\t\tdump [n] words(s) beginning at addr\n");
	printf("w  addr byte\t\twrite byte at RAM addr\n");
	printf("w  addr \"<string>\"\twrite string starting at RAM addr\n");
	printf("W  addr word\t\twrite word at RAM addr\n");
	printf("f  addr0 addr1 byte\tfill addr0..addr1 with byte *\n");
	printf("t  addr\t\t\twatch byte at addr *\n");
	printf("T  addr\t\t\twatch word at addr *\n");
	printf("\n");
	printf("ew addr word\t\twrite word at EEPROM (or RAM) addr\n");
	printf("eb addr byte\t\twrite byte at EEPROM (or RAM) addr\n");
	printf("E! addr\t\t\tbulk erase EEPROM starting at addr\n");
	printf("\t\t\t(to disable EEPROM write protection do \"w f1 0\")\n");
	printf("aw addr word\t\twrite word at AM29F FLASH addr\n");
	printf("ab addr byte\t\twrite byte at AM29F FLASH addr\n");
	printf("A! addr\t\t\tbulk erase AM29F FLASH keyed off addr\n");
	printf("\n");
	printf("g  [addr]\t\t\"go\" starting at current PC, or addr\n");
	printf("n  [count]\t\texecute count instructions (default 1) then break\n");
	printf("b\t\t\tbreak into BDM mode\n");
	printf("s\t\t\tshow BDM status register at FF01\n");
	printf("Z\t\t\treset CPU w/BKGD asserted\n");
	printf("q\t\t\treset CPU w/BKGD off\n");
	printf("z\t\t\treset POD connection\n");
	printf("l  file\t\t\tload a file of bdm12 commands\n");
	printf("\n");
	printf("S  <filename>\t\tload the given S-record file into RAM *\n");
	printf("Sv <filename>\t\tlike 'S' but verify after write *\n");
	printf(
			"P  <filename>\t\tload the given S-record file into EEPROM (or RAM)  *\n");
	printf("Pv <filename>\t\tlike 'P' but verify after write *\n");
	printf("Q  <filename>\t\tload the given S-record file into FLASH *\n");
	printf("Qv <filename>\t\tlike 'Q' but verify after write *\n");
	printf("\n");
	printf("addr can be hex or HC12 register mnemonic");
	if (symfile)
		printf(" or symbol from %s\n", symfile);
	else
		printf("\n");
	printf("?a\t\t\tprint HC12 register mnemonics, sorted by address\n");
	printf("?n\t\t\tprint HC12 register mnemonics, sorted by name\n");
	printf(".\t\t\trepeat last command\n");
	printf("\n");
	printf("* can be stopped by typing Control-C\n");
}

static void runFile(FILE *fp)
{
	char last[256];

	strcpy(last, "\n");

	while (1)
	{
		char buf[256];
		char *sc, *bp;

		/* prompt for and get next command */
		printf("> ");
		if (!fgets(buf, sizeof(buf), fp))
			break;
		if (fp != stdin)
			printf("%s", buf);

		/* handle a few special cases here */
		switch (buf[0])
		{
		case '.':
			bp = last;
			break;
		case '\n':
			continue;
		default:
			strcpy(last, buf);
			bp = last;
		}

		/* handle the command */
		while ((sc = strchr(bp, ';')) != NULL)
		{
			*sc = '\0';
			processCmd(bp);
			*sc = ';'; /* in case we want to use it again */
			bp = sc + 1;
		}
		processCmd(bp);
	}
}

/* convert a string to an address, allowing for HC12 register mnemonics.
 * if not found, try looking for a label in symfile.
 */
static int s2a(char *s)
{
	int i;

	for (i = 0; i < NSTBL; i++)
		if (strcasecmp(s, stbl[i].name) == 0)
			return (stbl[i].addr);
	if (symfile && trySymFile(s, &i) == 0)
		return (i);
	for (i = strlen(s); --i >= 0;)
		if (!isxdigit(s[i]))
		{
			printf("%s: not found\n", s);
			break;
		}
	return (strtol(s, 0, 16));
}

/* process one user command */
static void processCmd(char buf[])
{
	char s0[128];
	char s1[128];
	char s2[128];
	int n;

	while (*buf == ' ')
		buf++;

	if (buf[0] == 'b')
	{
		bdmFirmware(); /* stop */
		printRegs();
	}
	else if (buf[0] == 'r')
	{
		bdmFirmware(); /* stop */
		printRegs();
		writePodByte(0x08); /* resume */
	}
	else if (buf[0] == 'Z')
		resetCPU(1);
	else if (buf[0] == 'z')
		resetPod();
	else if (buf[0] == 's')
		bdmMode();
	else if (buf[0] == 'q')
		resetCPU(0);
	else if (buf[0] == '?' && buf[1] == 'a')
		prStbl(1);
	else if (buf[0] == '?' && buf[1] == 'n')
		prStbl(0);
	else if (sscanf(buf, "R %c %s", s0, s1) == 2)
		loadReg(s0[0], s2a(s1));
	else if (sscanf(buf, "E! %s", s0) == 1)
		bulkEEE(s2a(s0));
	else if (sscanf(buf, "ew %s %s", s0, s1) == 2)
		writeEEW(s2a(s0), s2a(s1));
	else if (sscanf(buf, "eb %s %s", s0, s1) == 2)
		writeEEB(s2a(s0), s2a(s1));
	else if (sscanf(buf, "A! %s", s0) == 1)
		bulkEFlash(s2a(s0));
	else if (sscanf(buf, "aw %s %s", s0, s1) == 2)
	{
		if (writeFlashW(s2a(s0), s2a(s1)) < 0)
			printf("FLASH write fails\n");
	}
	else if (sscanf(buf, "ab %s %s", s0, s1) == 2)
	{
		if (writeFlashB(s2a(s0), s2a(s1)) < 0)
			printf("FLASH write fails\n");
	}
	else if (sscanf(buf, "g %s", s0) == 1)
		go(s2a(s0));
	else if (buf[0] == 'g')
		go(-1);
	else if (sscanf(buf, "d %s %d", s0, &n) == 2)
		printBytes(s2a(s0), s2a(s0) + n - 1);
	else if (sscanf(buf, "d %s", s0) == 1)
		printBytes(s2a(s0), s2a(s0));
	else if (sscanf(buf, "t %s", s0) == 1)
		watchByte(s2a(s0));
	else if (sscanf(buf, "T %s", s0) == 1)
		watchWord(s2a(s0));
	else if (sscanf(buf, "D %s %d", s0, &n) == 2)
		printWords(s2a(s0), s2a(s0) + 2 * (n - 1));
	else if (sscanf(buf, "D %s", s0) == 1)
		printWords(s2a(s0), s2a(s0));
	else if (sscanf(buf, "w %s \"%[^\"]", s0, s1) == 2)
		writePodBuf(s2a(s0), s1, strlen(s1) + 1);
	else if (sscanf(buf, "w %s %s", s0, s1) == 2)
		writeByte(s2a(s0), s2a(s1));
	else if (sscanf(buf, "W %s %s", s0, s1) == 2)
		writeWord(s2a(s0), s2a(s1));
	else if (sscanf(buf, "f %s %s %s", s0, s1, s2) == 3)
		fillRAM(s2a(s0), s2a(s1), s2a(s2));
	else if (sscanf(buf, "Sv%*c%s", s0) == 1)
		loadSFile(s0, LT_RAM, 1);
	else if (sscanf(buf, "S%*c%s", s0) == 1)
		loadSFile(s0, LT_RAM, 0);
	else if (sscanf(buf, "Pv%*c%s", s0) == 1)
		loadSFile(s0, LT_EE, 1);
	else if (sscanf(buf, "P%*c%s", s0) == 1)
		loadSFile(s0, LT_EE, 0);
	else if (sscanf(buf, "Q%*c%s", s0) == 1)
		loadSFile(s0, LT_FLASH, 0);
	else if (sscanf(buf, "Qv%*c%s", s0) == 1)
		loadSFile(s0, LT_FLASH, 1);
	else if (sscanf(buf, "l%*c%s", s0) == 1)
		loadFile(s0);
	else if (sscanf(buf, "n %d", &n) == 1)
		trace(n);
	else if (buf[0] == 'n')
		trace(1);
	else if (buf[0] != '\n')
		showCmds();
}

static void loadFile(char *fn)
{
	FILE *fp;

	fp = fopen(fn, "r");
	if (!fp)
	{
		perror(fn);
		return;
	}
	printf("==> Start %s:\n", fn);
	runFile(fp);
	printf("==> End %s\n", fn);
	fclose(fp);
}

/* read and print uP addresses a0 .. a1 inclusive */
static void printBytes(int a0, int a1)
{
#define	BPL	16
	byte line[BPL];
	int a, la;
	int i, n;

	/* header */
	printf("Addr ");
	for (i = 0; i < BPL; i++)
		printf("  %x", (a0 + i) & 0xf);
	printf("    ----------------\n");

	/* data */
	n = 0;
	la = a0;
	nintr = 0;
	for (a = a0; a <= a1 && !nintr; a++)
	{
		readByte(a, &i);
		line[n++] = i;
		if (n == sizeof(line) || a == a1)
		{
			printf("%04x:", la);
			for (i = 0; i < BPL; i++)
				if (i < n)
					printf(" %02x", line[i]);
				else
					printf("   ");
			printf("    ");
			for (i = 0; i < BPL; i++)
				printf("%c", i < n && isgraph(line[i]) ? line[i] : ' ');
			printf("\n");
			la += BPL;
			n = 0;
		}
	}
}

/* read and print uP word addresses a0 .. a1 inclusive */
static void printWords(int a0, int a1)
{
#define	WPL	8
	word line[WPL];
	int a, la;
	int i, n;

	/* header */
	printf("Addr ");
	for (i = 0; i < 2 * WPL; i += 2)
		printf("  %x  ", (a0 + i) & 0xf);
	printf("\n");

	/* data */
	n = 0;
	la = a0;
	nintr = 0;
	for (a = a0; a <= a1 && !nintr; a += 2)
	{
		readWord(a, &i);
		line[n++] = i;
		if (n == WPL || a >= a1 - 1)
		{
			printf("%04x:", la);
			for (i = 0; i < n; i++)
				printf(" %04x", line[i]);
			printf("\n");
			la += 2 * WPL;
			n = 0;
		}
	}
}

/* print current registers */
static void printRegs()
{
	byte h, l;

	writePodByte(0x63);
	readPodByte(&h);
	readPodByte(&l);
	printf("PC:%02x%02x ", h, l);
	tryMapFile((((int) h) << 8) | l);

	writePodByte(0x64);
	readPodByte(&h);
	readPodByte(&l);
	printf("D:%02x%02x ", h, l);

	writePodByte(0x65);
	readPodByte(&h);
	readPodByte(&l);
	printf("X:%02x%02x ", h, l);

	writePodByte(0x66);
	readPodByte(&h);
	readPodByte(&l);
	printf("Y:%02x%02x ", h, l);

	writePodByte(0x67);
	readPodByte(&h);
	readPodByte(&l);
	printf("SP:%02x%02x ", h, l);

	printf("\n");
}

/* write 1 byte of data to address a */
static void writeByte(int addr, int data)
{
	byte h, l;
	byte b;

	writePodByte(0xC0); /* WRITE_BYTE */
	h = addr >> 8;
	l = addr & 0xff;
	writePodByte(h);
	writePodByte(l);
	b = data & 0xff;
	writePodByte(b);
	writePodByte(b);
}

/* write 1 word of data to address a */
static void writeWord(int addr, int data)
{
	byte h, l;

	if (addr & 1)
	{
		h = data >> 8;
		l = data & 0xff;
		writeByte(addr, h);
		writeByte(addr + 1, l);
	}
	else
	{
		writePodByte(0xC8); /* WRITE_WORD */
		h = addr >> 8;
		l = addr & 0xff;
		writePodByte(h);
		writePodByte(l);
		h = data >> 8;
		l = data & 0xff;
		writePodByte(h);
		writePodByte(l);
	}
}

/* write one word of data to FLASH address addr.
 * return 0 if ok else -1
 */
static int writeFlashW(int addr, int data)
{
	int back;

	if (addr & 1)
	{
		if (writeFlashB(addr, data >> 8) == 0 && writeFlashB(addr + 1, data
				& 0xff) == 0)
			return (0);
	}
	else
	{
		struct timeval tv0, tv1;
		int base = 0x8000; /* just need any FLASH chip select */

		/* engage Program then set word */
		writeWord(base + 0xaaa, 0xaa);
		writeWord(base + 0x554, 0x55);
		writeWord(base + 0xaaa, 0xa0);
		writeWord(addr, data);


		/* done when reads back same */
		gettimeofday(&tv0, NULL);
		do
		{
			readWord(addr, &back);
			if (back == data)
				return (0);
		} while (!gettimeofday(&tv1, NULL) && dms(&tv1, &tv0) < MAXFWMS);
	}

	/* oh oh */
	printf("\n===========================================================\n");
	printf("Flash write failed at 0x%.4x: wrote 0x%.4x read back 0x%.4x\n", addr,
			data, back);
	printf("===========================================================\n");
	return (-1);
}

/* write one byte of data to FLASH address addr.
 * return 0 if ok else -1
 */
static int writeFlashB(int addr, int data)
{
	int other;

	/* can really only write words */
	if (addr & 1)
	{
		addr -= 1;
		readByte(addr, &other);
		data = (other << 8) | data;
	}
	else
	{
		readByte(addr + 1, &other);
		data = (data << 8) | other;
	}

	return (writeFlashW(addr, data));
}

/* write 1 word of data to EEPROM address addr.
 */
static void writeEEW(int addr, int data)
{
	byte h, l;

	h = data >> 8;
	l = data & 0xff;
	writeEEB(addr, h);
	writeEEB(addr + 1, l);

//PODEX_ICE
//	if (addr & 1)
//	{
//		h = data >> 8;
//		l = data & 0xff;
//		writeEEB(addr, h);
//		writeEEB(addr + 1, l);
//	}
//	else
//	{
//		writePodByte(0x05); /* EEPROM write */
//		h = addr >> 8;
//		l = addr & 0xff;
//		writePodByte(h);
//		writePodByte(l);
//		h = data >> 8;
//		l = data & 0xff;
//		writePodByte(h);
//		writePodByte(l);
//	}
//PODEX_ICE

}

/* write 1 byte of data to EEPROM address addr.
 * see pg 34
 */
static void writeEEB(int addr, int data)
{
	/* no POD shortcut for bytes */

	/* first erase */
	writeByte(0xf3, 0x96); /* BYTE ERASE EELAT !EEPRGM */
	writeByte(addr, 0);
	writeByte(0xf3, 0x97); /* BYTE ERASE EELAT  EEPRGM */
	usDelay(10000); /* soak */
	writeByte(0xf3, 0x80); /* !BYTE !ERASE !EELAT !EEPRGM*/
	writeByte(0xf3, 0x96); /* BYTE ERASE EELAT !EEPRGM */

	/* then program */
	writeByte(0xf3, 0x92); /* BYTE !ERASE EELAT !EEPRGM */
	writeByte(addr, data);
	writeByte(0xf3, 0x93); /* BYTE !ERASE EELAT  EEPRGM */
	usDelay(10000); /* soak */
	writeByte(0xf3, 0x80); /* !BYTE !ERASE !EELAT !EEPRGM*/
	writeByte(0xf3, 0x92); /* BYTE !ERASE EELAT !EEPRGM */

	/* done */
	writeByte(0xf3, 0x80); /* !BYTE !ERASE !EELAT !EEPRGM*/
}

/* fill from a0 thru a1 with data */
static void fillRAM(int a0, int a1, int data)
{
	int a;
	int n;

	n = a1 - a0 + 1;
	nintr = 0;
	for (a = a0; a <= a1 && !nintr; a++)
	{
		if (n > 1000 && ((100 * (a - a0 + 1) / n) % 5) == 0)
			printf("%3d%%\r", 100 * (a - a0 + 1) / n);
		writeByte(a, data);
	}
	if (n > 1000)
		printf("\n");
}

/* print value of addr each time it changes until ^C */
static void watchByte(int addr)
{
	int v, lastv;

	for (lastv = 88, nintr = 0; !nintr;)
	{
		readByte(addr, &v);
		if (v != lastv)
		{
			printf("0x%02x\n", v);
			lastv = v;
		}
	}
}

/* print value of addr each time it changes until ^C */
static void watchWord(int addr)
{
	int v, lastv;

	for (lastv = 8888, nintr = 0; !nintr;)
	{
		readWord(addr, &v);
		if (v != lastv)
		{
			printf("0x%04x\n", v);
			lastv = v;
		}
	}
}

/* read one byte at addr */
static void readByte(int addr, int *bp)
{
	byte h, l;

	writePodByte(0xE0); /* READ_BYTE */
	h = addr >> 8;
	l = addr & 0xff;
	writePodByte(h);
	writePodByte(l);
	readPodByte(&h);
	readPodByte(&l);
	*bp = (addr & 1) ? l : h;
}

/* read one word at addr */
static void readWord(int addr, int *bp)
{
	byte h, l;

	if (addr & 1)
	{
		int hb, lb;
		readByte(addr, &hb);
		readByte(addr + 1, &lb);
		*bp = (hb << 8) | lb;
	}
	else
	{
		writePodByte(0xE8); /* READ_WORD */
		h = addr >> 8;
		l = addr & 0xff;
		writePodByte(h);
		writePodByte(l);
		readPodByte(&h);
		readPodByte(&l);
		*bp = (((int) h) << 8) | l;
	}
}

/* enable into BDM firmware mode */
static void bdmFirmware()
{
	byte h, l;

	writePodByte(0xe4); /* READ_BD_BYTE */
	writePodByte(0xff); /* addr of BDM status reg */
	writePodByte(0x01);
	readPodByte(&h);
	readPodByte(&l);
	writePodByte(0xc4); /* WRITE_BD_BYTE */
	writePodByte(0xff); /* addr of BDM status reg */
	writePodByte(0x01);
	writePodByte(l | 0x80); /* or-in ENBDM */
	writePodByte(l | 0x80);
	writePodByte(0x90); /* enable BACKGROUND */
}

/* query and report BKGD mode */
static void bdmMode()
{
	byte h, l;

	writePodByte(0xe4); /* READ_BD_BYTE */
	writePodByte(0xff); /* addr of BDM status reg */
	writePodByte(0x01);
	readPodByte(&h);
	readPodByte(&l);

	printf("0x%02x: ", h);
	if (h == 0xff)
	{
		printf("CPU not responding.. try reset.");
	}
	else
	{
		if (h & 0x80)
			printf("BDM enabled. ");
		if (h & 0x40)
			printf("BDM active. ");
	}
	printf("\n");
}

/* execute n instructions then break */
static void trace(int n)
{
	while (n-- > 0)
		writePodByte(0x10); /* TRACE1 */
	printRegs();
}

/* assert /RESET, hold, release.
 * if bkgd assert it also.
 */
static void resetCPU(int bkgd)
{
	if (bkgd)
	{
		writePodByte(0x01);
	}
	else
	{
		writePodByte(0x02);
		usDelay(10000);
		writePodByte(0x03);
	}
}

/* load the PC with addr and go, or current PC if addr == -1 */
static void go(int addr)
{
	byte h, l;

	if (addr == -1)
	{
		bdmFirmware(); /* stop */
		writePodByte(0x63); /* READ_PC */
		readPodByte(&h);
		readPodByte(&l);
		printf("go at PC:%02x%02x\n", h, l);
	}
	else
	{
		bdmFirmware(); /* insure we have control of PC */
		writePodByte(0x43); /* WRITE_PC */
		h = addr >> 8;
		l = addr & 0xff;
		writePodByte(h); /* load PC with addr */
		writePodByte(l);
	}

	writePodByte(0x08); /* GO */
}

/* load the register indicated with data */
static void loadReg(char regid, int data)
{
	byte h, l;

	switch (regid)
	{
	case 'p':
		writePodByte(0x43); /* WRITE_PC */
		h = data >> 8;
		l = data & 0xff;
		writePodByte(h);
		writePodByte(l);
		break;
	case 'd':
		writePodByte(0x44); /* WRITE_D */
		h = data >> 8;
		l = data & 0xff;
		writePodByte(h);
		writePodByte(l);
		break;
	case 'x':
		writePodByte(0x45); /* WRITE_X */
		h = data >> 8;
		l = data & 0xff;
		writePodByte(h);
		writePodByte(l);
		break;
	case 'y':
		writePodByte(0x46); /* WRITE_Y */
		h = data >> 8;
		l = data & 0xff;
		writePodByte(h);
		writePodByte(l);
		break;
	case 's':
		writePodByte(0x47); /* WRITE_SP */
		h = data >> 8;
		l = data & 0xff;
		writePodByte(h);
		writePodByte(l);
		break;
	default:
		printf("Unknown register. Choose one from {pdxys}\n");
		break;
	}
}

/* perform a bulk erase of EEPROM starting at the given addr */
static void bulkEEE(int addr)
{
	byte h, l;

	h = addr >> 8;
	l = addr & 0xff;
	writePodByte(0x07); /* bulk erase */
	writePodByte(h); /* hi-byte of starting addr */
	writePodByte(l); /* lo-byte of starting addr */
}

/* perform a bulk erase of FLASH based on the given addr */
static void bulkEFlash(int addr)
{
	int base = addr & 0xf000;

	/* reset */
	writeWord(base + 0x000, 0xf0);

	/* send Chip Erase command */
	writeWord(base + 0xaaa, 0xaa);
	writeWord(base + 0x554, 0x55);
	writeWord(base + 0xaaa, 0x80);
	writeWord(base + 0xaaa, 0xaa);
	writeWord(base + 0x554, 0x55);
	writeWord(base + 0xaaa, 0x10);

	/* want for RDnBY = bit 1 in PORTF */
	wait4Flash();
}

/* wait for RDnBY = bit 1 in PORTF */
static void wait4Flash()
{
	int back;

	do
	{
		readByte(0x30, &back);
	} while (!(back & 2));
}

/* load the named S-record into EEPROM or RAM */
static void loadSFile(char name[], LoadType lt, int verify)
{
	struct timeval tv0, tv1;
	char buf[1024], *bp;
	FILE *fp;
	long sb;
	int lastn;
	int tb;

	/* open */
	fp = fopen(name, "r");
	if (!fp)
	{
		perror(name);
		return;
	}

	/* find size for progress meter */
	fseek(fp, 0L, SEEK_END);
	sb = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	/* load line-at-a-time */
	gettimeofday(&tv0, NULL);
	tb = 0;
	nintr = 0;
	lastn = -1;
	while (!nintr && fgets(buf, sizeof(buf), fp))
	{
		if (buf[1] == '1')
		{
			/* 2-byte addr + data */
			long os;
			int addr;
			int i, n;

			sscanf(buf + 2, "%2x", &n);
			n -= 2 + 1; /* exclude addr and checksum */
			sscanf(buf + 4, "%4x", &addr);

			for (i = 0, bp = &buf[8]; i < n;)
			{
				int nb = load1S(addr, bp, n - i, lt, verify);

				if (nb < 0)
					goto out;
				i += nb;
				addr += nb;
				bp += 2 * nb;
				tb += nb;
			}

			os = ftell(fp);
			n = 100 * os / sb;
			if (n != lastn)
			{
				double dt; /* long can easily overflow in i calc */
				gettimeofday(&tv1, NULL);
				dt = dms(&tv1, &tv0);
				if (dt > 5000)
				{
					i = (sb - os) * dt / os / 1000;
					printf("\r%3d%% %2d:%02d", n, i / 60, i % 60);
				}
				else
					printf("\r%3d%%", n);
				lastn = n;
			}
		}

		if (buf[1] == '9')
		{
			/* execution address -- 0 is bogus. */
			int addr;

			sscanf(buf + 4, "%4x", &addr);
			if (addr)
			{
				printf("\nGo at %04x\n", addr);
				go(addr);
			}
		}
	}

	printf("\r%3d%% %2d:%02d", 100, 0, 0);

	out:

	/* report speed */
	gettimeofday(&tv1, NULL);
	sb = dms (&tv1, &tv0);
	printf(" %dB in %lds = %ldB/s\n", tb, sb / 1000, 1000 * tb / sb);

	/* done */
	fclose(fp);
}

/* write S record to board:
 *   addr:   the target address
 *   buf:    ASCII bytes of data
 *   nleft:  number of bytes left to load in this line
 *   lt:     destination type
 *   verify: whether to verify after write
 * return the number of bytes written, or -1 if verify fails or other trouble.
 */
static int load1S(int addr, char buf[], int nleft, LoadType lt, int verify)
{
	int data;

	if (!(addr & 1) && nleft >= 2)
	{
		int b0, b1;

		sscanf(buf, "%2x", &b0);
		sscanf(buf + 2, "%2x", &b1);
		data = (b0 << 8) | b1;

		switch (lt)
		{
		case LT_RAM:
			writeWord(addr, data);
			break;
		case LT_EE:
			writeEEW(addr, data);
			break;
		case LT_FLASH:
			if (writeFlashW(addr, data) < 0)
				return (-1);
			break;
		}

		/* verify if desried (flash is always already verified) */
		if (verify && lt != LT_FLASH)
		{
			if (verifyRAM(addr, b0) < 0)
				return (-1);
			if (verifyRAM(addr + 1, b1) < 0)
				return (-1);
		}
		return (2);
	}
	else
	{
		sscanf(buf, "%2x", &data);

		switch (lt)
		{
		case LT_RAM:
			writeByte(addr, data);
			break;
		case LT_EE:
			writeEEB(addr, data);
			break;
		case LT_FLASH:
			if (writeFlashB(addr, data) < 0)
				return (-1);
			break;
		}

		/* verify if desired (flash is always already verified) */
		if (verify && lt != LT_FLASH)
		{
			if (verifyRAM(addr, data) < 0)
				return (-1);
		}
		return (1);
	}
}

/* read the byte at addr and return 0 if it matches data, else -1 */
static int verifyRAM(int addr, int data)
{
	int back;

	readByte(addr, &back);
	if (data != back)
	{
		printf("%04x: Expect %02x read %02x\n", addr, data, back);
		return (-1);
	}
	return (0);
}

static void writePodBuf(int addr, char str[], int n)
{
	while (n-- > 0)
		writeByte(addr++, *str++);
}

/* write one byte to pod */
static void writePodByte(byte b)
{

//PODEX_ICE	struct serial_icounter_struct sic;
//	unsigned s;
//PODEX_ICE	int ncts;
//PODEX_ICE


//	if (ctsto == 0)
//	{
//		/* wait for Clear-to-Send */
//		do
//		{
//			ioctl(podfd, TIOCMGET, &s);
//		} while (!(s & TIOCM_CTS));
//	}


//PODEX_ICE	/* queue and wait for CTS toggle, or just forget it */
//PODEX_ICE	ioctl(podfd, TIOCGICOUNT, &sic);
//PODEX_ICE	ncts = sic.cts;
	if (write(podfd, &b, 1) != 1)
	{
		perror("write");
		exit(1);
	}
//PODEX_ICE	tcdrain(podfd);
//PODEX_ICE	s = ctsto;
//PODEX_ICE	do
//PODEX_ICE	{
//PODEX_ICE		ioctl(podfd, TIOCGICOUNT, &sic);
//PODEX_ICE	} while (sic.cts < ncts + 2 && --s);
//PODEX_ICE
	volatile long int x = ctsto;
	while(x--)
	{
		usleep(1);
	}
//PODEX_ICE
}

/* read one byte from pod */
static void readPodByte(byte *bp)
{
	if (read(podfd, bp, 1) != 1)
	{
		perror("read");
		exit(1);
	}
}

/*
//PODEX_ICE
static void usDelay(int us)
{
	struct timeval tv;

	tv.tv_sec = us / 1000000;
	tv.tv_usec = us % 1000000;

	select(0, NULL, NULL, NULL, &tv);
}
//PODEX_ICE
*/

/* try looking up name in symfile.
 * if succeed, set *addr and return 0, else return -1.
 * this has only been used with the .lst files from imagecraft.
 */
static int trySymFile(char *name, int *addr)
{
	char buf[128];
	FILE *p;
	char *ok;

	/* look for symbol usage */
	sprintf(buf, "grep '^............................._%s$' %s", name, symfile);
	p = popen(buf, "r");
	if ((ok = fgets(buf, sizeof(buf), p)) != NULL)
		*addr = strtol(buf + 9, NULL, 16);
	pclose(p);
	if (ok)
		return (0);

	/* look for symbol definition */
	sprintf(buf, "grep '[ \t]_%s:' %s", name, symfile);
	p = popen(buf, "r");
	if ((ok = fgets(buf, sizeof(buf), p)) != NULL)
		*addr = strtol(buf, NULL, 16);
	pclose(p);
	if (ok)
		return (0);

	return (ok ? 0 : -1);

}

/* try looking up PC in mapfile.
 * if succeed print line number.
 * this has only been used with the .mp file compiled with -l from imagecraft.
 */
static void tryMapFile(int pc)
{
	char buf[128];
	char n0[128], n1[128];
	int a;
	int lower;
	FILE *fp;

	fp = fopen(mapfile, "r");
	if (!fp)
		return;

	lower = 1;
	while (fgets(buf, sizeof(buf), fp))
	{
		if (sscanf(buf, "%x %s", &a, n1) != 2)
			continue;
		if (pc < a)
		{
			printf("%s\n", n0);
			break;
		}
		strcpy(n0, n1);
	}
	fclose(fp);
}

/* For RCS Only -- Do Not Edit */
static char
		*rcsid[2] =
		{
				(char *) rcsid,
				"@(#) $RCSfile: bdm12.c,v $ $Date: 2003/04/15 20:46:47 $ $Revision: 1.1.1.1 $ $Name:  $" };
