/* manage a config file of name=value pairs.
 * see nextPair for a full description of syntax.
 */

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "configfile.h"
#include "strops.h"
#include "telenv.h"

/* read the given list of params from the given config file.
 * return number of entries found, or -1 if can not even open file.
 * if trace each entry found is traced to stderr as "filename: name = value".
 * N.B. it is the *last* entry in the file that counts.
 */
int readCfgFile(int trace, char *cfn, CfgEntry cea[], int ncea)
{
    char *bn = basenm(cfn);
    CfgEntry *cep, *lcea = cea + ncea;
    char name[256];
    char valu[1024];
    int nfound;
    FILE *fp;

    /* open file */
    fp = telfopen(cfn, "r");
    if (!fp)
        return (-1);

    /* reset found flags */
    for (cep = cea; cep < lcea; cep++)
        cep->found = 0;

    /* for each pair in file
     *   fill if in cea list
     */
    nfound = 0;
    while (!nextPair(fp, cfn, name, sizeof(name), valu, sizeof(valu)))
    {
        for (cep = cea; cep < lcea; cep++)
        {
            if (!strcasecmp(cep->name, name))
            {
                switch (cep->type)
                {
                case CFG_INT:
                    *((int *)cep->valp) = atoi(valu);
                    break;
                case CFG_DBL:
                    *((double *)cep->valp) = atof(valu);
                    break;
                case CFG_STR:
                    (void)strncpy((char *)(cep->valp), valu, cep->slen);
                    break;
                default:
                    fprintf(stderr, "%s: bad type: %d\n", bn, cep->type);
                    exit(1);
                }
                if (!cep->found)
                {
                    cep->found = 1;
                    nfound++; /* just count once */
                }
                break;
            }
        }
    }

    /* done with file */
    fclose(fp);

    /* print the final list if desired */
    if (trace)
    {
        daemonLog("Reading %s:", bn);
        for (cep = cea; cep < lcea; cep++)
        {
            if (cep->found)
            {
                int l = sprintf(valu, "%15s = ", cep->name);
                switch (cep->type)
                {
                case CFG_INT:
                    sprintf(valu + l, "%d", *((int *)cep->valp));
                    break;
                case CFG_DBL:
                    sprintf(valu + l, "%g", *((double *)cep->valp));
                    break;
                case CFG_STR:
                    sprintf(valu + l, "%s", (char *)cep->valp);
                    break;
                default:
                    sprintf(valu + l, "Bogus type: %d", cep->type);
                    break;
                }
                daemonLog("%s", valu);
            }
        }
    }

    return (nfound);
}

/* handy wrapper to read 1 config file entry.
 * return 0 if found, else -1.
 */
int read1CfgEntry(int trace, char *fn, char *name, CfgType t, void *vp, int slen)
{
    CfgEntry e;

    e.name = name;
    e.type = t;
    e.valp = vp;
    e.slen = slen;

    return (readCfgFile(trace, fn, &e, 1) == 1 ? 0 : -1);
}

/* handy utility to print an error message describing what went wrong with
 * a call to readCfgFile().
 * fn is the offending file name.
 * retval is the return value from readCfgFile().
 * pf is a printf-style function to call. use daemonLog if NULL.
 * cea[]/ncea are the same as passed to readCfgFile().
 */
void cfgFileError(char *fn, int retval, CfgPrFp pf, CfgEntry cea[], int ncea)
{
    int i;

    if (!pf)
        pf = (CfgPrFp)daemonLog;

    if (retval < 0)
    {
        (*pf)("%s: %s\n", basenm(fn), strerror(errno));
    }
    else
    {
        for (i = 0; i < ncea && retval < ncea; i++)
        {
            if (!cea[i].found)
            {
                (*pf)("%s: %s not found\n", basenm(fn), cea[i].name);
                retval++;
            }
        }
    }
}

/* search cea[] for name and return 1 if present and marked found, return 0 if
 * present but not marked found, return -1 if not present at all.
 */
int cfgFound(char *name, CfgEntry cea[], int ncea)
{
    CfgEntry *cep, *lcea = cea + ncea;
    int found = 0;
    int present = 0;

    for (cep = cea; cep < lcea; cep++)
        if (strcmp(name, cep->name) == 0)
        {
            present = 1;
            found = cep->found;
            break;
        }

    return (present ? (found ? 1 : 0) : -1);
}

/* append the given variable to the given config file.
 * if !cmt, label with the time.
 * return 0 if ok, else -1.
 */
int writeCfgFile(char *fn, char *name, char *value, char *cmt)
{
    FILE *fp;

    /* open file for append */
    fp = telfopen(fn, "a");
    if (!fp)
        return (-1);

    fprintf(fp, "%-15s %-15s ! ", name, value);
    if (cmt)
    {
        fprintf(fp, "%s\n", cmt);
    }
    else
    {
        time_t t = time(NULL);
        fprintf(fp, "Updated UTC %s", asctime(gmtime(&t)));
    }

    fclose(fp);
    return (0);
}

/* read the next pair of a name=value pair from a file.
 * the '=' is optional.
 * allow surrounding white space everywhere (which includes '\r' for DOS sake).
 * comments are from ! or # to end of line.
 * values may be enclosed in pairs of ' or " to contain white space; \ in such
 *   a value removes meaning of ' " and \n.
 * return 0 if find something else -1 if error.
 * as a special case if find a '/' surrounded by whitespace we return that in
 *  name[0] (with '\0' in name[1]).
 */

#define COMMENT '!'    /* comment: remainder of line is ignored */
#define COMMEN2 '#'    /* alternate comment: remainder of line is ignored */
#define NVSEP '='      /* optional name/value pair separator */
#define STRING '\''    /* one form of string delimiter */
#define STRIN2 '"'     /* another form of string delimiter */
#define BACKSLASH '\\' /* remove special meaning of chars in a STRING value */
#define SLASH '/'      /* this is a special mark we check for too */

enum
{
    DONE,      /* found a complete name=value pair */
    INWHITE,   /* in whitespace -- ' ', '\t', '\n', '\r' */
    INCOMMENT, /* in comment via COMMENT or COMMEN2, until '\n' */
    INNAME,    /* in name of name=value pair */
    SEEKVALUE, /* searching for non-white to begin a value */
    INVALUE,   /* in value of name=value pair */
    INSTRING,  /* in value of name='value' pair */
};

/* find the next name=value pair (or lone '/').
 * values must be delimited by = or whitespace else be in '..' or "..".
 * also reject any name or value that includes '/' (unless in '').
 * return 0 if ok, -1 on eof before finding anything interesting, -2 on eof
 *   after finding something, -3 on other errors
 */
int nextPair(fp, fn, name, maxname, value, valuelen) FILE *fp;
char fn[];
char name[];
int maxname;
char value[];
int valuelen;
{
    char *bn = basenm(fn);
    char delim = 0;
    int nname = 0, nvalue = 0;
    int state;
    int c;

    for (state = INWHITE; state != DONE;)
    {
        c = getc(fp);
        switch (state)
        {
        case INWHITE:
            switch (c)
            {
            case EOF:
                return (-1);
            case COMMENT:
            case COMMEN2:
                state = INCOMMENT;
                break;
            case SLASH:
                name[0] = c;
                name[1] = '\0';
                return (0);
            case ' ':  /* FALLTHRU */
            case '\t': /* FALLTHRU */
            case '\r': /* FALLTHRU */
            case '\n':
                /* stay in INWHITE state */
                break;
            default:
                /* interesting character -- assume it's a name */
                state = INNAME;
                nname = 0;
                name[nname++] = c;
                break;
            }
            break;

        case INCOMMENT:
            /* stay in INCOMMENT state until see a '\n' or EOF */
            switch (c)
            {
            case EOF:
                return (-1);
            case '\n':
                state = INWHITE;
            }
            break;

        case INNAME:
            /* accumulate name until see NVSEP or whitespace.
             * then switch to SEEKVALUE
             */
            switch (c)
            {
            case EOF:
                fprintf(stderr, "Saw EOF within name\n");
                return (-2);
            case ' ':  /* FALLTHRU */
            case '\t': /* FALLTHRU */
            case '\r': /* FALLTHRU */
            case '\n': /* FALLTHRU */
            case NVSEP:
                name[nname] = '\0';
                state = SEEKVALUE;
                break;
            default:
                if (nname >= maxname)
                {
                    name[maxname - 1] = '\0';
                    fprintf(stderr, "%s: name \"%s\" too long\n", bn, name);
                    return (-3);
                }
                name[nname++] = c;
                break;
            }
            break;

        case SEEKVALUE:
            /* skip until see something other than whitespace */
            switch (c)
            {
            case EOF:
                fprintf(stderr, "Saw EOF before value\n");
                return (-2);
            case ' ':  /* FALLTHRU */
            case '\t': /* FALLTHRU */
            case '\n': /* FALLTHRU */
            case '\r': /* FALLTHRU */
            case NVSEP:
                /* skip and stay in SEEKVALUE state */
                break;
            case STRING:
            case STRIN2:
                /* saw one of the STRING delimiters */
                state = INSTRING;
                delim = c;
                nvalue = 0;
                break;
            default:
                /* saw some other non-white character */
                state = INVALUE;
                nvalue = 0;
                ungetc(c, fp);
                break;
            }
            break;

        case INVALUE:
            /* accumulate value until see white space */
            switch (c)
            {
            case EOF:  /* FALLTHRU */
            case ' ':  /* FALLTHRU */
            case '\t': /* FALLTHRU */
            case '\r': /* FALLTHRU */
            case '\n':
                value[nvalue] = '\0';
                state = DONE;
                break;
            default:
                if (nvalue >= valuelen)
                {
                    fprintf(stderr, "%s: value too long for \"%s\"\n", bn, name);
                    return (-3);
                }
                value[nvalue++] = c;
                break;
            }
            break;

        case INSTRING:
            /* accumulate value until see delim again.
             * stay alert for \ escape code.
             */
            switch (c)
            {
            case EOF:
                fprintf(stderr, "Saw EOF within string\n");
                return (-2);

            case BACKSLASH:
                /* get next char */
                switch (c = getc(fp))
                {
                case EOF:
                    fprintf(stderr, "EOF after BACKSLASH\n");
                    return (-2);
                case '\n':
                    /* "\\\n" is ignored entirely */
                    break;
                default:
                    /* add literally */
                    if (nvalue >= valuelen)
                    {
                        fprintf(stderr, "%s: escaped char %c makes string too long for \"%s\"\n", bn, c, name);
                        return (-3);
                    }
                    value[nvalue++] = c;
                    break;
                }
                break;

            default:
                if (nvalue >= valuelen)
                {
                    fprintf(stderr, "%s: value too long for \"%s\"\n", bn, name);
                    return (-3);
                }
                if (c == delim)
                {
                    value[nvalue] = '\0';
                    state = DONE;
                }
                else
                    value[nvalue++] = c;
                break;
            }
            break;
        }
    }

    return (0);
}

/* open a file of filenames and build a malloced list of them.
 * return count (which might be 0) and new array in *fnames.
 * we only set *fnames if we return > 0.
 * perror() and exit() if trouble.
 */
int readFilenames(fn, fnames) char *fn; /* name of file naming files, one per line */
char ***fnames;                         /* malloced array of malloced file names found */
{
    char **array = NULL;
    char buf[1024];
    FILE *fp;
    int n;

    /* open the file */
    fp = fopen(fn, "r");
    if (!fp)
    {
        fp = telfopen(fn, "r");
        if (!fp)
        {
            perror(fn);
            exit(1);
        }
    }

    /* scan for names -- skip lines with # ! or just \n */
    n = 0;
    while (fgets(buf, sizeof(buf), fp))
    {
        int l = strlen(buf);
        char *newstr, **newa;

        if (l < 2 || buf[0] == '#' || buf[0] == '!')
            continue;
        buf[--l] = '\0';

        /* get string memory and extend the array of pointers */
        newstr = malloc(l + 1);
        if (!newstr)
        {
            fprintf(stderr, "No memory to add `%s' from %s\n", buf, fn);
            exit(1);
        }
        if (array)
            newa = (char **)realloc((void *)array, (n + 1) * sizeof(char *));
        else
            newa = malloc(sizeof(char *));
        if (!newa)
        {
            fprintf(stderr, "No memory for more filenames in %s\n", fn);
            exit(1);
        }

        /* ok, add to the list */
        strcpy(newstr, buf);
        array = newa;
        array[n++] = newstr;
    }

    /* return */
    if (n > 0)
        *fnames = array;
    return (n);
}

/* given a file name of the form dir/base.ext break it into pieces.
 * any but base may be "". we always set dir/base/ext to something.
 * dir[] will include a trailing /.
 */
void decomposeFN(char *fn, char dir[], char base[], char ext[])
{
    char *lastslashp = NULL, *lastdotp = NULL;
    char c, *cp;
    int ncpy;

    for (cp = fn; (c = *cp); cp++)
        if (c == '/' || c == '\\') /* DOS someday :-( */
            lastslashp = cp;
        else if (c == '.')
            lastdotp = cp;
    cp++; /* want cp to point to trailing \0 */

    if (lastslashp)
    {
        ncpy = lastslashp - fn + 1; /* include / */
        strncpy(dir, fn, ncpy);
        dir[ncpy] = '\0';
        fn = lastslashp + 1;
    }
    else
        dir[0] = '\0';

    if (lastdotp)
    {
        ncpy = lastdotp - fn;
        strncpy(base, fn, ncpy);
        base[ncpy] = '\0';
        strcpy(ext, lastdotp + 1);
    }
    else
    {
        strcpy(base, fn);
        ext[0] = '\0';
    }
}
