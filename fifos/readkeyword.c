#include <string.h>
#include <stdio.h>
#include <fitsio.h>

int main(int argc, char *argv[])
    {

    fitsfile *infptr;      /* pointer to the FITS file, defined in fitsio.h */

    int status;
    char value[100];

    status = 0;

    /* open the existing FITS files */
    fits_open_file(&infptr, argv[1], READWRITE, &status);

    fits_read_keyword(infptr, argv[2], value, NULL, &status);
    printf("%s\n", value);

    fits_close_file(infptr, &status);

    if (status)          /* print any error messages */
        fits_report_error(stdout, status);
    return(status);
    }


