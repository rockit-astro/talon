typedef struct
{
    char **list;
    int nlist;
} IniList;

extern IniList *iniRead(char *fn);
extern char *iniFind(IniList *listp, char *section, char *name);
extern void iniFree(IniList *listp);
