

extern double current_jd();
extern double ut1_to_gha(double T);
extern double epoch_jd(double epoch);
extern void jd_timeval(double jd, struct timeval *tv);
extern double ut1_to_lst(double T, double gha, double lon);

extern int readtle(char *, char *, SatElem *);
extern int writetle(SatElem *el, char *line1, char *line2);

