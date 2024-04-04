/* csimc.c */
extern int verbose;
extern char *host;
extern int port;
extern void pollBack (int fd);

/* boot.c */
extern void loadAllCfg (char *cfn);
extern int loadOneCfg(int addr, char *fn);
extern int loadFirmware (int addr, char *fn);

/* eintrio.c */
extern int selectI (int n, fd_set *rp, fd_set *wp, fd_set *xp,
    struct timeval *tp);
extern size_t readI (int fd, void *buf, size_t n);
extern size_t writeI (int fd, const void *buf, size_t n);
