#ifndef XDELTA_H
#define XDELTA_H

#include <QtGui/QWidget>
#include "ui_xdelta.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

class xdelta : public QWidget
{
    Q_OBJECT

public:
    xdelta(QWidget *parent = 0);
    ~xdelta();

private:
    Ui::xdeltaClass ui;


    int fdin;
    int fdout;
    FILE* fin;
    FILE* fout;
    char telhome_def[128];
    char fin_def[128];
    char fout_def[128];
    char fin_path[4096];
    char fout_path[4096];
    char in_line[128];

    char* get_TELHOME();
    int fifo_setup();


private slots:
	void buttonSetClicked();
	void buttonHAmClicked();
    void buttonHApClicked();
    void buttonDECmClicked();
    void buttonDECpClicked();
};

#endif // XDELTA_H
