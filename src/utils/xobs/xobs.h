#include "telfifo.h"

/* xobs.c */
extern Widget toplevel_w;
extern TelStatShm *telstatshmp;
extern XtAppContext app;
extern char myclass[];
extern Obj sunobj, moonobj;
extern int xobs_alone;
extern void die (void);

/* config.c */
extern void initCfg (void);
extern double SUNDOWN;
extern double STOWALT, STOWAZ;
extern double SERVICEALT, SERVICEAZ;
extern char BANNER[80];

/* control.c */
extern void g_stop (Widget w, XtPointer client, XtPointer call);
extern void g_exit (Widget w, XtPointer client, XtPointer call);
extern void g_home (Widget w, XtPointer client, XtPointer call);
extern void g_limit (Widget w, XtPointer client, XtPointer call);
extern void g_focus (Widget w, XtPointer client, XtPointer call);
extern void g_paddle (Widget w, XtPointer client, XtPointer call);
extern void g_confirm (Widget w, XtPointer client, XtPointer call);

/* fifo_cb.c */
void initPipesAndCallbacks(void);
void closePipesAndCallbacks(void);

/* gui.c */
typedef enum {
    LTIDLE, LTOK, LTACTIVE, LTWARN,
    LTN
} LtState;
extern Pixel ltcolors[LTN];
extern Pixel editableColor;
extern Pixel uneditableColor;
extern void mkGUI (char *version);
extern int setColor (Widget w, char *resource, Pixel newp);
extern String fallbacks[];
extern void setLt (Widget w, LtState s);
extern Widget mkLight (Widget p_w);
extern void msg (char *fmt, ...);
extern void rmsg (char *line);
extern void guiSensitive (int whether);

/* paddle.c */
extern void pad_manage (void);
extern void pad_reset (void);

/* query.c */
extern void query (Widget tw, char *msg, char *label0, char *label1, char
    *label2, void (*func0)(), void (*func1)(), void (*func2)());
extern int rusure (Widget tw, char *msg);
extern int rusure_geton (void);
extern void rusure_seton (int whether);

/* scope.c */
extern void s_stow (Widget w, XtPointer client, XtPointer call);
extern void s_service (Widget w, XtPointer client, XtPointer call);
extern void s_here (Widget w, XtPointer client, XtPointer call);
extern void s_track (Widget w, XtPointer client, XtPointer call);
extern void s_goto (Widget w, XtPointer client, XtPointer call);
extern void s_edit (Widget w, XtPointer client, XtPointer call);

/* skymap.c */
extern Widget mkSky (Widget p_w);
extern void showSkyMap (void);

/* update.c */
extern void updateStatus(int force);
