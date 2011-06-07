/* KMI 7/25/05 - Simple test program for reading joystick */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/joystick.h>

int main()
{
    int fd;
    struct js_event e;
    char axisname[] = "XY";

    fd = open("/dev/input/js0", O_RDONLY);

    if (fd == -1) {
        printf("Error opening joystick\n");
        exit(1);
    }

    while (1) {
        int axis, direc;

        read(fd, &e, sizeof(struct js_event));

        if (e.type & JS_EVENT_INIT) {
            printf("SYNTHETIC ");
        }

        switch (e.type & ~JS_EVENT_INIT) {
            case JS_EVENT_AXIS:
                axis = e.number/2;
                direc = e.number%2;

                printf("Axis %d%c, %d\n", axis, axisname[direc], e.value);
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
