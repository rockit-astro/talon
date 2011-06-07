
/*
    FLI Filter wheel support functions for filter.c

    Note: This file is brought into filter.c via #include
    Not meant to be added directly to project or compiled directly.
    See the use of flags controlling use of this code in the Makefile
*/

#include "libfli.h"

/* STO 3/3/07 - FLI filter functions; */

typedef struct {
    flidomain_t domain;
    char *dname;
    char *name;
} filt_t;

int         numfilts = 0;
filt_t *    filt = NULL;
flidev_t    dev;

void findfilts(flidomain_t domain, filt_t **filt)
{
    long r;
    char **tmplist;

    r = FLIList(domain | FLIDEVICE_FILTERWHEEL, &tmplist);
    if (r != 0)
    {
        fifoWrite(Filter_Id, 0, "FLIList library call failed (%d)", r);
    }
    else
    {
        if (tmplist != NULL && tmplist[0] != NULL)
        {
            int i, filts = 0;

            for (i = 0; tmplist[i] != NULL; i++)
                filts++;

            if ((*filt = realloc(*filt, (numfilts + filts) * sizeof(filt_t))) == NULL)
            {
                fifoWrite(Filter_Id, 0, "FLI findFilter realloc() failed");
                return;
            }

            for (i = 0; tmplist[i] != NULL; i++)
            {
                int j;
                filt_t *tmpfilt = *filt + i;

                for (j = 0; tmplist[i][j] != '\0'; j++)
                {
                    if (tmplist[i][j] == ';')
                    {
                        tmplist[i][j] = '\0';
                        break;
                    }
                }

                tmpfilt->domain = domain;
                switch (domain)
                {
                case FLIDOMAIN_PARALLEL_PORT:
                    tmpfilt->dname = "parallel port";
                    break;

                case FLIDOMAIN_USB:
                    tmpfilt->dname = "USB";
                    break;

                case FLIDOMAIN_SERIAL:
                    tmpfilt->dname = "serial";
                    break;

                case FLIDOMAIN_INET:
                    tmpfilt->dname = "inet";
                    break;

                default:
                    tmpfilt->dname = "Unknown domain";
                    break;
                }
                tmpfilt->name = strdup(tmplist[i]);
            }

            numfilts += filts;
        }

        FLIFreeList(tmplist);
    }
}
/*
    "ExtFilt" functions.  These are have similar interfaces between different devices.

        Each returns true or false (non-zero or zero)
        On error, these functions call error functions that use fifoWrite directly to
        output status and errors to the display and logs.
        Regardless of output, Talon will output a final error message upon error of
        any of these functions (i.e a FALSE return value)
*/

/* Close an SBIG connection; return 0 if error, 1 if success */
int fli_shutdown()
{
    int i;

    if (filt)
    {
        long r = FLIClose(dev);
        if (r)
        {
            fifoWrite(Filter_Id,0,"Error return closing FLI (%d)",r);
        }

        for (i = 0; i < numfilts; i++)
            free(filt[i].name);

        free(filt);
        filt = NULL;
    }
    return 1;
}

/* Connect to SBIG driver and camera; return 0 if error, 1 if success */
int fli_reset()
{
    long        r;
    char        libver[1024];
    long        tmp1;
    int         connected = 0;

    fli_shutdown();

    /*
    r = FLISetDebugLevel(NULL, FLIDEBUG_ALL);
    if(r) fifoWrite(Filter_Id, 0, "Failed to set debugging mode (%d)", r);
    */

    r = FLIGetLibVersion(libver, sizeof(libver));
    if (!r)
    {
        fifoWrite(Filter_Id, 0, "FLI Library version '%s'", libver);
    }
    else
    {
        fifoWrite(Filter_Id, 0, "FLI GetLibVersion failure (%d)",r);
        return 0;
    }

    // Find fli filters on USB
    findfilts(FLIDOMAIN_USB, &filt);

    if (numfilts == 0)
    {
        fifoWrite(Filter_Id,0,"No filter wheels found.");
    }
    else
    {
        fifoWrite(Filter_Id, 0, "Trying filter wheel '%s' from %s domain", filt[0].name, filt[0].dname);

        r = FLIOpen(&dev, filt[0].name, FLIDEVICE_FILTERWHEEL | filt[0].domain);
        if (r)
        {
            fifoWrite(Filter_Id, 0, "Unable to open (%d)",r);
        }
        else
        {
#define BUFF_SIZ (1024)
            char buff[BUFF_SIZ];

            r = FLIGetModel(dev, buff, BUFF_SIZ);
            if (!r) fifoWrite(Filter_Id, 0, "  Model: %s", buff);
            r = FLIGetHWRevision(dev, &tmp1);
            if (!r) fifoWrite(Filter_Id, 0, "  Hardware Rev: %ld", tmp1);
            r = FLIGetFWRevision(dev, &tmp1);
            if (!r) fifoWrite(Filter_Id, 0, "  Firmware Rev: %ld", tmp1);

            connected = 1;
        }
    }

    if (!connected) fli_shutdown();

    return (filt != NULL);
}

/* Select a filter by number (0 based) */
int fli_select(int position)
{
    long r = FLISetFilterPos(dev, position);
    if (r)
    {
        fifoWrite(Filter_Id, 0, "FLI error setting filter number %d",position);
        return 0;
    }
    return 1;
}

/* Re-align filter wheel */
int fli_home()
{
    return fli_select(0);
}

/* Connect this implementation via our function pointers */

void set_for_fli()
{
    extFilt_reset_func = fli_reset;
    extFilt_shutdown_func = fli_shutdown;
    extFilt_home_func = fli_home;
    extFilt_select_func = fli_select;
}

