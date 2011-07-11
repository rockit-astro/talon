#include "xdelta.h"

#include <QtGui>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    xdelta w;
    w.show();
    return a.exec();
}
