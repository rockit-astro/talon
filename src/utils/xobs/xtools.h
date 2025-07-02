/* header file for use with xtools.c */

extern void set_something (Widget w, char *resource, char *value);
extern void get_something (Widget w, char *resource, char *value);
extern void get_xmstring (Widget w, char *resource, char **txtp);
extern void set_xmstring (Widget w, char *resource, char *txt);
extern void wlprintf (Widget w, char *fmt, ...);
extern void wtprintf (Widget w, char *fmt, ...);
extern Widget getShell (Widget w);
extern void raiseShell (Widget w);
extern char *getXRes (Widget any, char *name, char *def);
extern void XCheck (XtAppContext app);
extern void lookLikeLabel (Widget pb);
extern Pixel getColor (Widget w, char *name);
extern void get_xmlabel_font (Widget w, XFontStruct **f);

extern int stopchk(void);
extern void stopchk_down(void);
extern void stopchk_up(Widget shell_w, char *title, char *message);
