extern FILE *telfopen (char *name, char *how);
extern int telopen (char *name, int flags, ...);
extern void telfixpath (char *newp, char *old);
extern int telOELog(char *progname);
extern char *timestamp (time_t t);
extern void daemonLog (char *fmt, ...);
