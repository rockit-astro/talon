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

	return 0;
}

xdelta::xdelta(QWidget *parent) :
	QWidget(parent)
{
	ui.setupUi(this);

	unitsCurrent = ARCSECONDS;
	double valueHA = 0;
	double incValueHA = 0;
	double valueDEC = 0;
	double incValueDEC = 0;

	refreshValues();

	connect(ui.buttonSet, SIGNAL(clicked(bool)), this, SLOT(buttonSetClicked()));
	connect(ui.buttonHAm, SIGNAL(clicked(bool)), this, SLOT(buttonHAmClicked()));
	connect(ui.buttonHAp, SIGNAL(clicked(bool)), this, SLOT(buttonHApClicked()));
	connect(ui.buttonDECm, SIGNAL(clicked(bool)), this,
			SLOT(buttonDECmClicked()));
	connect(ui.buttonDECp, SIGNAL(clicked(bool)), this,
			SLOT(buttonDECpClicked()));


	connect(ui.spinValueHA, SIGNAL(valueChanged(double)), this, SLOT(valueChanged(double)));
	connect(ui.spinIncrementHA, SIGNAL(valueChanged(double)), this, SLOT(valueChanged(double)));
	connect(ui.spinValueDEC, SIGNAL(valueChanged(double)), this, SLOT(valueChanged(double)));
	connect(ui.spinIncrementDEC, SIGNAL(valueChanged(double)), this, SLOT(valueChanged(double)));

	connect(ui.unitsDeg, SIGNAL(clicked(bool)), this, SLOT(unitsDegClicked(bool)));
	connect(ui.unitsRad, SIGNAL(clicked(bool)), this, SLOT(unitsRadClicked(bool)));
	connect(ui.unitsArc, SIGNAL(clicked(bool)), this, SLOT(unitsArcClicked(bool)));

	if (fifo_setup() != 0)
	{
		exit(-1);
	}

}

xdelta::~xdelta()
{

}

void xdelta::refreshValues()
{
	if (unitsCurrent == RADIANS)
	{
		valueHA = ui.spinValueHA->value() * 180.0 / M_PI;
		incValueHA = ui.spinIncrementHA->value() * 180.0 / M_PI;

		valueDEC = ui.spinValueDEC->value() * 180.0 / M_PI;
		incValueDEC = ui.spinIncrementDEC->value() * 180.0 / M_PI;

	}
	else if (unitsCurrent == ARCSECONDS)
	{
		valueHA = ui.spinValueHA->value() / 3600;
		incValueHA = ui.spinIncrementHA->value() / 3600;

		valueDEC = ui.spinValueDEC->value() / 3600;
		incValueDEC = ui.spinIncrementDEC->value() / 3600;
	}
	else
	{
		valueHA = ui.spinValueHA->value();
		incValueHA = ui.spinIncrementHA->value();

		valueDEC = ui.spinValueDEC->value();
		incValueDEC = ui.spinIncrementDEC->value();
	}

	return;
}

void xdelta::valueChanged(double value)
{
	ui.spinValueHA->setSingleStep(ui.spinIncrementHA->value());
	ui.spinValueDEC->setSingleStep(ui.spinIncrementDEC->value());

//	refreshValues();

	printf("values(%.8f,%.8f)\n", valueHA, valueDEC);

	return;
}


void xdelta::unitsDegClicked(bool click)
{
	refreshValues();

	ui.spinValueHA->setMaximum(180);
	ui.spinValueHA->setMinimum(-180);
	ui.spinValueHA->setSuffix(" deg");
	ui.spinValueHA->setValue(valueHA);
	ui.spinValueHA->setSingleStep(incValueHA);

	ui.spinIncrementHA->setMaximum(180);
	ui.spinIncrementHA->setMinimum(0);
	ui.spinIncrementHA->setSuffix(" deg");
	ui.spinIncrementHA->setValue(incValueHA);
	ui.spinIncrementHA->setSingleStep(0);

	unitsCurrent = DEGREES;

	printf("Click DEG\n");
	return;
}

void xdelta::unitsRadClicked(bool click)
{
	refreshValues();
	unitsCurrent = RADIANS;

	ui.spinValueHA->setMaximum(M_PI);
	ui.spinValueHA->setMinimum(-M_PI);
	ui.spinValueHA->setSuffix(" rad");
	ui.spinValueHA->setValue(valueHA * M_PI / 180.0);
	ui.spinValueHA->setSingleStep(incValueHA * M_PI / 180.0);

	ui.spinIncrementHA->setMaximum(180);
	ui.spinIncrementHA->setMinimum(0);
	ui.spinIncrementHA->setSuffix(" deg");
	ui.spinIncrementHA->setValue(incValueHA * M_PI / 180.0);
	ui.spinIncrementHA->setSingleStep(0);


	printf("Click RAD\n");
	return;
}

void xdelta::unitsArcClicked(bool click)
{
	refreshValues();
	unitsCurrent = ARCSECONDS;

	ui.spinValueHA->setMaximum(648000);
	ui.spinValueHA->setMinimum(-648000);
	ui.spinValueHA->setSuffix(" arcs");
	ui.spinValueHA->setValue(valueHA * 3600.0);
	ui.spinValueHA->setSingleStep(incValueHA * 3600.0);

	ui.spinIncrementHA->setMaximum(648000);
	ui.spinIncrementHA->setMinimum(0);
	ui.spinIncrementHA->setSuffix(" arcs");
	ui.spinIncrementHA->setValue(incValueHA * 3600.0);
	ui.spinIncrementHA->setSingleStep(0);



	printf("Clock ARC\n");
	return;
}

void xdelta::buttonSetClicked()
{
	refreshValues();

	fprintf(fin, "xdelta(%.8f,%.8f)\n", valueHA, valueDEC);
	fflush(fin);
	printf("xdelta(%.8f,%.8f)\n", valueHA, valueDEC);
	fflush( stdout);
	return;
}

void xdelta::buttonHAmClicked()
{
	ui.spinValueHA->setValue(ui.spinValueHA->value() - ui.spinValueHA->singleStep());
	refreshValues();

	buttonSetClicked();

	return;
}

void xdelta::buttonHApClicked()
{
	ui.spinValueHA->setValue(ui.spinValueHA->value() + ui.spinValueHA->singleStep());
	refreshValues();

	buttonSetClicked();
	return;
}

void xdelta::buttonDECmClicked()
{
	ui.spinValueDEC->setValue(ui.spinValueDEC->value() - ui.spinValueDEC->singleStep());
	refreshValues();

	buttonSetClicked();
	return;
}

void xdelta::buttonDECpClicked()
{
	ui.spinValueDEC->setValue(ui.spinValueDEC->value() + ui.spinValueDEC->singleStep());
	refreshValues();

	buttonSetClicked();
	return;
}
