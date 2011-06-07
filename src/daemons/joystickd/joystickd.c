/* Daemon for reading input from a joystick and sending appropriate FIFO
   commands to control telescope 

   KMI 8/4/05 - Started code
       8/19/05 - Working variable-velocity RA/Dec movement

   TODO:
       Read axis/device/button data from config files
       One button toggles focus/filter mode
       One button toggles roof mode(?)
       One button takes an image
       One button marks a star in xobs
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/joystick.h>

#include "telfifo.h"

#define JS_STOP_THRESH 50 // Readings below this are treated as 'stop move'

struct JoystickState
{
    int raVelocity;
    int decVelocity;
    int focusButton;
};

int main(int argc, char *argv[])
{
    int fd;
    struct js_event e;
    char axisname[] = "XY";

    struct JoystickState state;
    state.raVelocity = 0;
    state.decVelocity = 0;
    state.focusButton = 0;

    /* TODO - read from config file */
    fd = open("/dev/input/js0", O_RDONLY);

    if (fd == -1) {
        printf("Error opening joystick\n");
        exit(1);
    }

    openFIFOs();

    while (1) {
        int jsAxis, jsDirec;

        // Vars for composing FIFO command
        char fifoDirec = '0';  // or N,S,E,W

        read(fd, &e, sizeof(struct js_event));

        if (e.type & JS_EVENT_INIT) {
            printf("SYNTHETIC ");
            continue; // Ignore synthetic events
        }

        switch (e.type & ~JS_EVENT_INIT) {
            case JS_EVENT_AXIS:
                jsAxis = e.number/2;
                jsDirec = e.number%2;

                printf("Axis %d%c, %d\n", jsAxis, axisname[jsDirec], e.value);

                if (jsAxis == 0) {
                    if (jsDirec == 0) {
                        if (e.value > 0) fifoDirec = 'W';
                        else             fifoDirec = 'E';
                        state.decVelocity = abs(e.value);
                    }
                    else if (jsDirec == 1) {
                        if (e.value > 0) fifoDirec = 'S';
                        else             fifoDirec = 'N';
                        state.raVelocity = abs(e.value);
                    }

                    e.value = abs(e.value);

                    if (state.raVelocity < JS_STOP_THRESH &&
                        state.decVelocity < JS_STOP_THRESH) {
                        fifoMsg(Tel_Id, "j0");
                    }
                    else {
                        char msg[1024];
                        sprintf(msg, "j%c %d", fifoDirec, e.value);
                        fifoMsg(Tel_Id, msg);
                    }
                }

                break;
            case JS_EVENT_BUTTON:
                if (e.value == 1) {
                    printf("Button %d down\n", e.number);
                }
                else {
                    printf("Button %d up\n", e.number);
                }
                break;
            default:
                printf("Unrecognized event: %d %d %d\n", 
                         e.type, e.value, e.number);
                break;
        }
    }
}
