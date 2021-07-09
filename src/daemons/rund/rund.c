/* run <command> as a daemon.
 * keep re-execing it unless it dies from SIGTERM or exits with 0.
 * arrange for all its output to go to $TELHOME/archive/logs/<command>.log.
 * exit 0 if program exists and we have exec perm, else 1.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "running.h"
#include "strops.h"
#include "telenv.h"

#define MINFORKDT 15 /* fork no faster than this, seconds */

static void usage(void);
static void recordStartup(char *title, int ac, char *av[]);
static void setupFD(char *log);
static void forever(int ac, char *av[]);
static int chkPATH(char *prog);
static void onSig(int signo);

static char *runprog;
static char *me;
static int pid;

int main(int ac, char *av[])
{
    char *telhome = getenv("TELHOME");

    /* must be given a command to run */
    me = basenm(av[0]);
    if (ac == 1 || av[1][0] == '-')
        usage();

    /* messages go to our own log until we get serious */
    me = basenm(av[0]);
    setupFD(me);
    recordStartup("Starting", ac - 1, av + 1);

    /* base name of command we shepherd */
    runprog = basenm(av[1]);

    /* cd TELHOME for any cores */
    if (!telhome || chdir(telhome) < 0)
    {
        daemonLog("chdir(%s): %s\n", telhome ? telhome : "??", strerror(errno));
        exit(1);
    }

    /* only one target */
    if (testlock_running(runprog) == 0)
    {
        daemonLog("%s: already running\n", runprog);
        exit(0);
    }

    /* see whether the command even exists and is executable */
    if (access(av[1], X_OK) == 0 || chkPATH(runprog) < 0)
    {
        daemonLog("Not found in %s\n", getenv("PATH"));
        exit(1);
    }

    /* we exit, leaving a shephard behind */
    switch (fork())
    {
    case 0:
        (void)setsid(); /* no controlling tty */
        forever(ac - 1, &av[1]);
        return (1);
        break;
    case -1:
        daemonLog("fork(): %s\n", strerror(errno));
        return (3);
        break;
    default:
        return (0);
        break;
    }

    /* huh ? */
    return (2);
}

static void usage()
{
    fprintf(stderr, "Usage: %s command [args ...]\n", me);
    fprintf(stderr, "Purpose: run command with the given args; restart if fails\n");
    fprintf(stderr, "  unless it exits with 0 or via SIGTERM.\n");
    fprintf(stderr, "Our output goes to $TELHOME/archive/logs/%s.log.\n", me);
    fprintf(stderr, "Command's output goes to $TELHOME/archive/logs/command.log\n");
    fprintf(stderr, "Once set up, we exit.\n");

    exit(1);
}

static void recordStartup(char *title, int ac, char *av[])
{
    char cmd[2048];
    int l = 0;

    while (ac--)
        l += sprintf(cmd + l, "%s ", *av++);
    daemonLog("%s: %s\n", title, cmd);
}

static void forever(int ac, char *av[])
{
    char lockname[128];
    time_t lastfork = 0;
    int status;

    /* only one sheparding effort */
    sprintf(lockname, "%s.%s", me, runprog);
    if (lock_running(lockname) < 0)
    {
        daemonLog("%s: another rund is already trying\n", runprog);
        exit(0);
    }

    /* now logs belong to the command */
    setupFD(runprog);

    /* survive SIGTERM to pass on in kind */
    signal(SIGTERM, onSig);

    /* keep reexecing unless dies from SIGTERM or exits 0 */
    while (1)
    {
        if (time(NULL) - lastfork < MINFORKDT)
        {
            daemonLog("forking too fast... waiting %d seconds\n", MINFORKDT);
            sleep(MINFORKDT);
        }
        lastfork = time(NULL);

        switch (pid = fork())
        {
        case 0:
            recordStartup("Started by rund", ac, av);
            execvp(av[0], av);
            daemonLog("execvp(%s): %s\n", av[0], strerror(errno));
            kill(getpid(), SIGTERM); /* parent will not restart */
            exit(1);                 /* superfluous */
            break;                   /* very superfluous */
        case -1:
            daemonLog("fork(): %s\n", strerror(errno));
            sleep(30); /* retry fork later */
            break;
        default:
            if (waitpid(pid, &status, WUNTRACED) < 0)
            {
                daemonLog("waitpid(): %s\n", strerror(errno));
                return; /* all we can do */
            }

            if (WIFSIGNALED(status))
            {
                int s = WTERMSIG(status);
                if (s == SIGTERM)
                {
                    daemonLog("final kill with SIGTERM\n");
                    return; /* finished */
                }
                daemonLog("died from signal %d\n", s);
            }
            else if (WIFEXITED(status))
            {
                int s = WEXITSTATUS(status);
                if (s == 0)
                {
                    daemonLog("final exit 0\n");
                    return; /* finished */
                }
                daemonLog("exited with %d\n", s);
            }
            else if (WIFSTOPPED(status))
            {
                int s = WSTOPSIG(status);
                daemonLog("stopped with signal %d\n", s);
            }
            else
            {
                daemonLog("sending SIGKILL due to unknown wait status:%d\n", status);
                kill(pid, SIGKILL); /* out of control -- abort big time */
            }
            break;
        }
    }
}

static void setupFD(char *log)
{
    long openmax = sysconf(_SC_OPEN_MAX);
    int i;

    dup2(open("/dev/null", O_RDONLY), 0);
    telOELog(log);
    for (i = 3; i < openmax; i++)
        (void)close(i);
}

/* search PATH for prog.
 * return 0 if find it and we have exec perm, else -1
 */
static int chkPATH(char *prog)
{
    char *path = getenv("PATH");
    char *pathcopy;
    char *end, *p;
    int ret;

    if (!path)
    {
        daemonLog("No PATH!!\n");
        return (-1);
    }

    pathcopy = strcpy(malloc(strlen(path) + 1), path);

    ret = -1;
    for (p = pathcopy; *p; p = end + 1)
    {
        char tstpath[1024];
        end = strchr(p, ':');
        if (end)
            *end = '\0';
        else
            end = p + strlen(p);
        sprintf(tstpath, "%s/%s", p, prog);
        if (access(tstpath, X_OK) == 0)
        {
            ret = 0;
            break;
        }
    }

    free(pathcopy);

    return (ret);
}

/* pass along then exit */
static void onSig(int signo)
{
    if (pid > 0)
    {
        daemonLog("rund received SIGTERM.. passing on in kind\n");
        kill(pid, SIGTERM);
    }
    exit(0);
}
