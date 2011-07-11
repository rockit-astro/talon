#include "xdelta.h"

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

char* xdelta::get_TELHOME()
{
	char* telhome = NULL;
	telhome = getenv("TELHOME");
	if (telhome == NULL)
		telhome = (char*) telhome_def;
	return telhome;
}

int xdelta::fifo_setup(void)
{
	strcpy(telhome_def, "/usr/local/telescope");
	strcpy(fin_def, "comm/Tel.in");

	strcpy(fin_path, get_TELHOME());
	strcat(fin_path, "/");
	strcat(fin_path, fin_def);

	fin = fopen(fin_path, "a");
	if (fin == NULL)
	{
		printf("Error opening fifo %s\n", fin_path);
		return -1;
	}

	//	fdin = open(fin_path, O_RDWR | O_NONBLOCK);
	//	if (fdin == -1)
	//	{
	//		//		close(fdin);
	//		printf("Error opening fifo %s\n", fin_path);
	//		return -1;
	//	}
	//	fin = fdopen(fdin, "r");
	//	if (fin == NULL)
	//	{
	//		fclose(fin);
	//		printf("Error opening fifo %s\n", fin_path);
	//		return -1;
	//	}
	//	printf("Open fifo %s\n", fin_path);

	//	strcpy(fout_def, "comm/Tel.out");
	//	strcpy(fout_path, get_TELHOME());
	//	strcat(fout_path, fout_def);
	//
	//	fdout = open(fout_path, O_RDWR | O_NONBLOCK);
	//	if (fdout == -1)
	//	{
	//		//		close(fdout);
	//		printf("Error opening fifo %s\n", fout_path);
	//		return -1;
	//	}
	//	fout = fdopen(fdout, "w");
	//	if (fout == NULL)
	//	{
	//		fclose(fout);
	//		printf("Error opening fifo %s\n", fout_path);
	//		return -1;
	//	}
	//	printf("Open fifo %s\n", fout_path);

	return 0;
}

xdelta::xdelta(QWidget *parent) :
	QWidget(parent)
{
	ui.setupUi(this);

	connect(ui.buttonSet, SIGNAL(clicked(bool)), this, SLOT(buttonSetClicked()));
	connect(ui.buttonHAm, SIGNAL(clicked(bool)), this, SLOT(buttonHAmClicked()));
	connect(ui.buttonHAp, SIGNAL(clicked(bool)), this, SLOT(buttonHApClicked()));
	connect(ui.buttonDECm, SIGNAL(clicked(bool)), this,
			SLOT(buttonDECmClicked()));
	connect(ui.buttonDECp, SIGNAL(clicked(bool)), this,
			SLOT(buttonDECpClicked()));

	if (fifo_setup() != 0)
	{
		exit(-1);
	}

}

xdelta::~xdelta()
{

}

void xdelta::buttonSetClicked()
{
	fprintf(fin, "xdelta(%f,%f)\n", ui.spinValHA->value(), ui.spinValDEC->value());
	fflush(fin);
	printf("xdelta(%f,%f)\n", ui.spinValHA->value(), ui.spinValDEC->value());
	fflush( stdout);
	return;
}

void xdelta::buttonHAmClicked()
{
	double val;
	val = ui.spinValHA->value() - ui.spinStepHA->value();
	ui.spinValHA->setValue(val);

	buttonSetClicked();

	return;
}

void xdelta::buttonHApClicked()
{
	double val;
	val = ui.spinValHA->value() + ui.spinStepHA->value();
	ui.spinValHA->setValue(val);

	buttonSetClicked();
	return;
}

void xdelta::buttonDECmClicked()
{
	double val;
	val = ui.spinValDEC->value() - ui.spinStepDEC->value();
	ui.spinValDEC->setValue(val);

	buttonSetClicked();
	return;
}

void xdelta::buttonDECpClicked()
{
	double val;
	val = ui.spinValDEC->value() + ui.spinStepDEC->value();
	ui.spinValDEC->setValue(val);

	buttonSetClicked();
	return;
}
