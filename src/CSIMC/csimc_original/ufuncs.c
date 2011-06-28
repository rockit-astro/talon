/* code to manage user and built-in functions */

#include "sa.h"

static UFSTblE *ufstbl;     /* malloc'd list of nufstbl UFSTblE's */
static int nufstbl;     /* number of entries in fstbl */

void
ufuncStats (void)
{
    UFSTblE *up;

    for (up = ufstbl; up < &ufstbl[nufstbl]; up++)
        if (up->name[0])
            printf ("%7s: %5d bytes %5d args %6d tmpv\n", up->name,
                    sizeof(UFSTblE)+up->size,
                    up->nargs, up->ntmpv);
}

/* search ufstbl for the named user function.
 * return pointer to an UFSTblE else NULL.
 */
UFSTblE *
findUFunc (char name[])
{
    UFSTblE *up;

    for (up = ufstbl; up < &ufstbl[nufstbl]; up++)
        if (strcmp (name, up->name) == 0)
            return (up);
    return (NULL);
}

/* free the named user function.
 * marked free in ufstbl by name[0] == '\0'.
 * return 0 if ok, else -1 if doesn't exist anyway.
 */
int
freeUFunc (char *name)
{
    UFSTblE *newtbl;

    newtbl = findUFunc(name);
    if (!newtbl)
        return (-1);
    if (newtbl->code)
        free ((void *)newtbl->code);
    memset ((void*)newtbl, 0, sizeof(UFSTblE));
    return (0);
}

/* return a new user function descriptor.
 * first check for empty slots (!name[0]), then grow ufstbl[] if need new one.
 * return pointer to an UFSTblE else NULL.
 */
UFSTblE *
newUFunc (void)
{
    UFSTblE *newtbl;

    newtbl = findUFunc("");
    if (!newtbl)
    {
        newtbl = (UFSTblE *)
                 (ufstbl ? realloc((void*)ufstbl,(nufstbl+1)*sizeof(UFSTblE))
                  : malloc (sizeof(UFSTblE)));
        if (!newtbl)
            return (NULL);
        ufstbl = newtbl;
        newtbl = &ufstbl[nufstbl++];
    }
    memset ((void*)newtbl, 0, sizeof(UFSTblE));
    return (newtbl);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: ufuncs.c,v $ $Date: 2001/04/19 21:11:58 $ $Revision: 1.1.1.1 $ $Name:  $
 */
