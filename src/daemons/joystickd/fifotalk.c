/* 
   Simple util to test simultaneous jogging
   KMI 8/19/05 - Started code
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/joystick.h>

#include "telfifo.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: fifotalk [cmd]\n");
        exit(1);
    }

    openFIFOs();

    fifoMsg(Tel_Id, argv[1]);
}
