#ifndef CLISERV_H
#define CLISERV_H

#include "telstatshm.h"


extern int serv_conn (char *name, int fd[2], char msg[]);
extern int cli_conn (char *name, int fd[2], char msg[]);
extern void dis_conn (char *name, int fd[2]);
extern int cli_write (int fd[2], char *msg, char *err);
extern int cli_read (int fd[2], int *code, char *buf, int bufl);
extern int serv_read (int fd[2], char *buf, int bufl);
extern int serv_write (int fd[2], int code, char *msg, char *err);
extern int open_telshm(TelStatShm **tpp);

#endif // CLISERV_H
