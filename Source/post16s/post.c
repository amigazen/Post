/* PostScript interpreter file "post.c" - the main program (Amiga)
 * (C) Adrian Aylward 1989, 1991
 *
 * This file contains the code to parse the command line arguments, the
 * WorkBench and intuition interface, and the printer screen or IFF file]
 * output routines.  It is therfore heavily Amiga dependent.  It calls the
 * interpreter library for all the PostScript stuff.
 *
 * There are also various Lattice dependencies: the startup code in _main,
 * the menu task, the flush and copy functions, and the floating point trap
 * and break handling.
 */

# include <arpbase.h>
# include <dos.h>
# include <devices/printer.h>
# include <devices/prtbase.h>
# include <exec/exec.h>
# include <exec/execbase.h>
# include <exec/tasks.h>
# include <graphics/gfxbase.h>
# include <graphics/text.h>
# include <intuition/intuition.h>
# include <libraries/dosextens.h>
# include <workbench/icon.h>
# include <workbench/startup.h>
# include <workbench/workbench.h>
# include <proto/diskfont.h>
# include <proto/dos.h>
# include <proto/exec.h>
# include <proto/graphics.h>
# include <proto/icon.h>
# include <proto/intuition.h>
# include <stdio.h>
# include <string.h>

# include "postlib.h"

/* Assembler routines */

extern void insertbreak(void);
extern void deletebreak(void);
extern void insertftrap(void);
extern void deleteftrap(void);

/* Routines defined and referenced only within this module */

extern void errmsg(char *string);
extern void tidyup(void);
extern void setprinter(void);
extern void setprintden(void);
extern void setreqgadgets(void);
extern void getreqgadgets(void);
extern void enablegadg(struct Gadget *gadget, int enable);
extern void setgadgbool(struct Gadget *gadget, int value);
extern int  getgadgbool(struct Gadget *gadget);
extern void setgadgint(struct Gadget *gadget, int value);
extern int  getgadgint(struct Gadget *gadget);
extern int  getgadgabs(struct Gadget *gadget);
extern int  strtoint(char **sp, int *ip);
extern void sendmenu(int action);
extern void __saveds menuproc(void);
# ifndef STATICLINK
extern void __saveds __asm flushpage(register __d0 int y1,
                                     register __d1 int y2);
extern void __saveds __asm copypage(register __d0 int num);
# endif
extern void printpage(void);
extern void iffpage(void);
extern int  igcd(int n, int m);

/* Message structure */

struct PSmessage
{   struct Message ExecMessage;
    long class;            /* Always zero */
    short action;          /* Action - from Post to handler */
    short command;         /* Command - from handler to Post */
    struct BitMap *bitmap; /* The bitmap */
    short y1, y2;          /* Min and max+1 y values to flush */
    short result;          /* Result  (return code) */
    short length;          /* Length of string */
    char *string;          /* String */
    long window;           /* Always zero */
    short errnum;          /* Last error number */
    short zero;            /* Reserved, presently always zero */
};

/* Actions */

# define PSACTOPEN    1 /* Open */
# define PSACTCLOSE   2 /* Close */
# define PSACTFLUSH   3 /* Flush out the bitmap */
# define PSACTPAUSE   4 /* Pause at the end of a page */
# define PSACTCOMMAND 5 /* Get a command */
# define PSACTEXIT    6 /* Exit */

/* Commands */

# define PSCOMQUIT    1 /* Quit */
# define PSCOMRESTART 2 /* Restart */
# define PSCOMFILEF   3 /* Load Font */
# define PSCOMFILEL   4 /* Load File */
# define PSCOMFILER   5 /* Run File */
# define PSCOMINTER   6 /* Interactive */

/* Lattice startup */

extern struct WBStartup *WBenchMsg;

/* External data (initialised to zero) */

int retcode, errcode, ioerror;
int fatalerror, restarterror, requested;

int arec;

BPTR errfh, confh;

# ifndef STATICLINK
struct library *PSbase;
# endif
struct PSparm parm;
struct PSmessage menumsg;

struct ArpBase *ArpBase;
struct FileRequester *filereq, *filereq1, *filereq2;

struct Process *process;
struct MsgPort *mainport, *menuport;
struct Task *menutask;
struct Screen *screen;
struct Window *syswindow, *reqwindow, *errwindow, *bitwindow, *intwindow;
struct BitMap bitmap;
struct ColorMap colormap;
struct TextFont *textfont;

int propen, prden;
struct IODRPReq prreq;
struct PrinterData *prdata;
struct PrinterExtendedData *prextdata;
struct Preferences *prprefs;
struct RastPort prrast;
struct MsgPort *prport;
ULONG prsig;
UBYTE prstatus[2];

int breakset, ftrapset;

int winxbase, winybase, winxsize, winysize, winxpos, winypos;

/* Colour tables.  The default color map type is a vector or UWORD RGB
 * values.  The colours are inverted as we set a bit in the bitmap whenever
 * we want to place a drop of (black or cyan/magenta/yellow) ink */

static UWORD bcolors[2] =  /* Black and white */
{   0xfff, 0x000                /* White   black */
};

static UWORD ccolors[16] = /* Colour (RGB or CMYK) */
{   0xfff, 0x0ff, 0xf0f, 0x00f, /* White   cyan    magenta blue */
    0xff0, 0x0f0, 0xf00, 0x000, /* Yellow  green   red     black */
    0x000, 0x000, 0x000, 0x000, /* Black */
    0x000, 0x000, 0x000, 0x000  /* Black */
};

char titlewait[]    = POSTVER " Copyright Adrian Aylward 1989, 1991";
char titlestart[]   = POSTVER " Running startup file(s)";
char titlerunning[] = POSTVER " Running";
char titleinter[]   = POSTVER " Interactive";
char titlepaused[]  = POSTVER " Paused";

char hailfilef[]    = POSTVER " Select font to load";
char hailfilen[]    = POSTVER " Select file to load";
char hailfiles[]    = POSTVER " Select file to run";

/* The default font and screen */

struct TextAttr topaz11 =       /* Font is Topaz 11 */
{   "topaz.font", 11, FS_NORMAL, FPF_DISKFONT };

struct NewScreen newscreen =    /* Screen */
{   0, 0, 640, 512, 1, 0, 15, LACE|HIRES, CUSTOMSCREEN, &topaz11,
    (UBYTE *) &titlewait, NULL, NULL
};

/* Stuff for the options requestor */

# define GADGOK    1  /* Gadget id's */
# define GADGCAN   2
# define GADGPRI   3
# define GADGBW    4
# define GADGC3    5
# define GADGC4    6
# define GADGPD1   7
# define GADGPD2   8
# define GADGPD3   9
# define GADGPD4  10
# define GADGPD5  11
# define GADGPD6  12
# define GADGPD7  13
# define GADGABS  14  /* Integer gadget, absolute value */
# define GADGRST  15  /* Dummy gadget to restart after error */

char undobuff[80];

SHORT bordervec1[] =
{   0, 0, 67, 0, 67, 14, 0, 14, 0, 1 };
struct Border border1 =
{   -2, -2, 15, 0, JAM1, 5, bordervec1, NULL };

UBYTE mlgadgbuff[8];
struct StringInfo mlgadginfo =
{   mlgadgbuff, (UBYTE *)undobuff, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget mlgadg =
{   NULL,    500, 212, 64, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border1, NULL, NULL, 0, (APTR)&mlgadginfo, GADGABS, NULL };

UBYTE mvgadgbuff[8];
struct StringInfo mvgadginfo =
{   mvgadgbuff, (UBYTE *)undobuff, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget mvgadg =
{   &mlgadg, 500, 198, 64, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border1, NULL, NULL, 0, (APTR)&mvgadginfo, GADGABS, NULL };

UBYTE mhgadgbuff[8];
struct StringInfo mhgadginfo =
{   mhgadgbuff, (UBYTE *)undobuff, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget mhgadg =
{   &mvgadg, 500, 184, 64, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border1, NULL, NULL, 0, (APTR)&mhgadginfo, GADGABS, NULL };

UBYTE mfgadgbuff[8];
struct StringInfo mfgadginfo =
{   mfgadgbuff, (UBYTE *)undobuff, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget mfgadg =
{   &mhgadg, 500, 170, 64, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border1, NULL, NULL, 0, (APTR)&mfgadginfo, GADGABS, NULL };

SHORT bordervec2[] =
{   0, 0, 43, 0, 43, 14, 0, 14, 0, 1 };
struct Border border2 =
{   -2, -2, 15, 0, JAM1, 5, bordervec2, NULL };


UBYTE ydgadgbuff[5];
struct StringInfo ydgadginfo =
{   ydgadgbuff, (UBYTE *)undobuff, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget ydgadg =
{   &mfgadg, 388, 198, 40, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border2, NULL, NULL, 0, (APTR)&ydgadginfo, GADGABS, NULL };

UBYTE xdgadgbuff[5];
struct StringInfo xdgadginfo =
{   xdgadgbuff, (UBYTE *)undobuff, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget xdgadg =
{   &ydgadg, 345, 198, 40, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border2, NULL, NULL, 0, (APTR)&xdgadginfo, GADGABS, NULL };

UBYTE yogadgbuff[5];
struct StringInfo yogadginfo =
{   yogadgbuff, (UBYTE *)undobuff, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget yogadg =
{   &xdgadg, 388, 184, 40, 13, 0, STRINGRIGHT|LONGINT, STRGADGET,
    (APTR)&border2, NULL, NULL, 0, (APTR)&yogadginfo, 0, NULL };

UBYTE xogadgbuff[5];
struct StringInfo xogadginfo =
{   xogadgbuff, (UBYTE *)undobuff, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget xogadg =
{   &yogadg, 345, 184, 40, 13, 0, STRINGRIGHT|LONGINT, STRGADGET,
    (APTR)&border2, NULL, NULL, 0, (APTR)&xogadginfo, 0, NULL };

UBYTE ygadgbuff[5];
struct StringInfo ygadginfo =
{   ygadgbuff, (UBYTE *)undobuff, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget ygadg =
{   &xogadg, 388, 170, 40, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border2, NULL, NULL, 0, (APTR)&ygadginfo, GADGABS, NULL };

UBYTE xgadgbuff[5];
struct StringInfo xgadginfo =
{   xgadgbuff, (UBYTE *)undobuff, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget xgadg =
{   &ygadg, 345, 170, 40, 13, 0, STRINGRIGHT|LONGINT|RELVERIFY, STRGADGET,
    (APTR)&border2, NULL, NULL, 0, (APTR)&xgadginfo, GADGABS, NULL };

short __chip idata1[] =
{   0xfffe, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002,
    0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0xfffe,
    0xfffe, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002,
    0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0xfffe,
    0xfffe, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002,
    0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0xfffe,
    0xfffe, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002,
    0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0x8002, 0xfffe };
struct Image image1 =
{   -2, -2, 15, 15, 4, &idata1[0], 15, 0, NULL };

struct IntuiText itext30 =
{   15, 0, JAM2, 1, 1, NULL, "7", NULL };
struct IntuiText itext29 =
{   15, 0, JAM2, 1, 1, NULL, "6", NULL };
struct IntuiText itext28 =
{   15, 0, JAM2, 1, 1, NULL, "5", NULL };
struct IntuiText itext27 =
{   15, 0, JAM2, 1, 1, NULL, "4", NULL };
struct IntuiText itext26 =
{   15, 0, JAM2, 1, 1, NULL, "3", NULL };
struct IntuiText itext25 =
{   15, 0, JAM2, 1, 1, NULL, "2", NULL };
struct IntuiText itext24 =
{   15, 0, JAM2, 1, 1, NULL, "1", NULL };

struct Gadget pd7gadg =
{   &xgadg,   526, 114, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, &itext30, 0, NULL, GADGPD7, NULL };
struct Gadget pd6gadg =
{   &pd7gadg, 512, 114, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, &itext29, 0, NULL, GADGPD6, NULL };
struct Gadget pd5gadg =
{   &pd6gadg, 498, 114, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, &itext28, 0, NULL, GADGPD5, NULL };
struct Gadget pd4gadg =
{   &pd5gadg, 484, 114, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, &itext27, 0, NULL, GADGPD4, NULL };
struct Gadget pd3gadg =
{   &pd4gadg, 470, 114, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, &itext26, 0, NULL, GADGPD3, NULL };
struct Gadget pd2gadg =
{   &pd3gadg, 456, 114, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, &itext25, 0, NULL, GADGPD2, NULL };
struct Gadget pd1gadg =
{   &pd2gadg, 442, 114, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, &itext24, 0, NULL, GADGPD1, NULL };

struct Gadget cwbgadg =
{   &pd1gadg, 417, 226, 11, 11, GADGIMAGE, TOGGLESELECT, BOOLGADGET,
    (APTR)&image1, NULL, NULL, 0, NULL, 0, NULL };

struct Gadget c4gadg =
{   &cwbgadg, 553, 86, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, NULL, 0, NULL, GADGC4, NULL };
struct Gadget c3gadg =
{   &c4gadg, 553, 72, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, NULL, 0, NULL, GADGC3, NULL };
struct Gadget bwgadg =
{   &c3gadg, 553, 58, 11, 11, GADGIMAGE, RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, NULL, 0, NULL, GADGBW, NULL };

struct Gadget iffgadg =
{   &bwgadg, 382, 86, 11, 11, GADGIMAGE, TOGGLESELECT, BOOLGADGET,
    (APTR)&image1, NULL, NULL, 0, NULL, 0, NULL };
struct Gadget scrgadg =
{   &iffgadg, 382, 72, 11, 11, GADGIMAGE, TOGGLESELECT, BOOLGADGET,
    (APTR)&image1, NULL, NULL, 0, NULL, 0, NULL };
struct Gadget prigadg =
{   &scrgadg, 382, 58, 11, 11, GADGIMAGE, TOGGLESELECT|RELVERIFY, BOOLGADGET,
    (APTR)&image1, NULL, NULL, 0, NULL, GADGPRI, NULL };

SHORT bordervec3[] =
{   0, 0, 101, 0, 101, 33, 0, 33, 0, 1 };
struct Border border3 =
{   -1, -1, 15, 0, JAM1, 5, bordervec3, NULL };

struct IntuiText itext1 =
{   15, 0, JAM1, 26, 11, NULL, "CANCEL", NULL };
struct Gadget cangadg =
{   &prigadg, 174, 219, 100, 32, 0, RELVERIFY, BOOLGADGET,
    (APTR)&border3, NULL, &itext1, 0, NULL, GADGCAN, NULL };

struct IntuiText itext2 =
{   15, 0, JAM1, 43, 11, NULL, "OK", NULL };
struct Gadget okgadg =
{   &cangadg, 24, 219, 100, 32, 0, RELVERIFY, BOOLGADGET,
    (APTR)&border3, NULL, &itext2, 0, NULL, GADGOK, NULL };

SHORT bordervec4[] =
{   0, 0, 251, 0, 251, 14, 0, 14, 0, 1 };
struct Border border4 =
{   -2, -2, 15, 0, JAM1, 5, bordervec4, NULL };

UBYTE ifngadgbuff[80];
struct StringInfo ifngadginfo =
{   ifngadgbuff, (UBYTE *)undobuff, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget ifngadg =
{   &okgadg, 25, 170, 250, 13, 0, 0, STRGADGET,
    (APTR)&border4, NULL, NULL, 0, (APTR)&ifngadginfo, 0, NULL };

UBYTE if4gadgbuff[80];
struct StringInfo if4gadginfo =
{   if4gadgbuff, (UBYTE *)undobuff, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget if4gadg =
{   &ifngadg, 25, 114, 250, 13, 0, 0, STRGADGET,
    (APTR)&border4, NULL, NULL, 0, (APTR)&if4gadginfo, 0, NULL };

UBYTE if3gadgbuff[80];
struct StringInfo if3gadginfo =
{   if3gadgbuff, (UBYTE *)undobuff, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget if3gadg =
{   &if4gadg, 25, 100, 250, 13, 0, 0, STRGADGET,
    (APTR)&border4, NULL, NULL, 0, (APTR)&if3gadginfo, 0, NULL };

UBYTE if2gadgbuff[80];
struct StringInfo if2gadginfo =
{   if2gadgbuff, (UBYTE *)undobuff, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget if2gadg =
{   &if3gadg, 25, 86, 250, 13, 0, 0, STRGADGET,
    (APTR)&border4, NULL, NULL, 0, (APTR)&if2gadginfo, 0, NULL };

UBYTE if1gadgbuff[80];
struct StringInfo if1gadginfo =
{   if1gadgbuff, (UBYTE *)undobuff, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget if1gadg =
{   &if2gadg, 25, 72, 250, 13, 0, 0, STRGADGET,
    (APTR)&border4, NULL, NULL, 0, (APTR)&if1gadginfo, 0, NULL };

UBYTE if0gadgbuff[80];
struct StringInfo if0gadginfo =
{   if0gadgbuff, (UBYTE *)undobuff, 0, 80, 0, 0, 0, 0, 0, 0, 0, 0, NULL };
struct Gadget if0gadg =
{   &if1gadg, 25, 58, 250, 13, 0, 0, STRGADGET,
    (APTR)&border4, NULL, NULL, 0, (APTR)&if0gadginfo, 0, NULL };

struct IntuiText itext23 =
{   15, 0, JAM1, 300, 226, NULL, "Close WBench", NULL };
struct IntuiText itext22 =
{   15, 0, JAM1, 448, 212, NULL, "Paths", &itext23 };
struct IntuiText itext21 =
{   15, 0, JAM1, 448, 198, NULL, "VM", &itext22 };
struct IntuiText itext20 =
{   15, 0, JAM1, 448, 184, NULL, "Htone", &itext21 };
struct IntuiText itext19 =
{   15, 0, JAM1, 448, 170, NULL, "Fonts", &itext20 };
struct IntuiText itext18 =
{   15, 0, JAM1, 448, 142, NULL, "Memory", &itext19 };
struct IntuiText itext17 =
{   15, 0, JAM1, 300, 198, NULL, "Dpi.", &itext18 };
struct IntuiText itext16 =
{   15, 0, JAM1, 300, 184, NULL, "Off.", &itext17 };
struct IntuiText itext15 =
{   15, 0, JAM1, 300, 170, NULL, "X, Y", &itext16 };
struct IntuiText itext14 =
{   15, 0, JAM1, 300, 142, NULL, "Page size", &itext15 };
struct IntuiText itext13 =
{   15, 0, JAM1, 300, 114, NULL, "Printer density", &itext14 };
struct IntuiText itext12 =
{   15, 0, JAM1, 416, 86, NULL, "4 Colour (CMYK)", &itext13 };
struct IntuiText itext11 =
{   15, 0, JAM1, 416, 72, NULL, "3 Colour (RGB)", &itext12 };
struct IntuiText itext10 =
{   15, 0, JAM1, 416, 58, NULL, "Black and white", &itext11 };
struct IntuiText itext9 =
{   15, 0, JAM1, 300, 86, NULL, "IFF file", &itext10 };
struct IntuiText itext8 =
{   15, 0, JAM1, 300, 72, NULL, "Screen", &itext9 };
struct IntuiText itext7 =
{   15, 0, JAM1, 300, 58, NULL, "Printer", &itext8 };
struct IntuiText itext6 =
{   15, 0, JAM1, 416, 30, NULL, "Colours", &itext7 };
struct IntuiText itext5 =
{   15, 0, JAM1, 300, 30, NULL, "Output", &itext6 };
struct IntuiText itext4 =
{   15, 0, JAM1, 25, 156, NULL, "IFF file name (pattern)", &itext5 };
struct IntuiText itext3 =
{   15, 0, JAM1, 25, 30, NULL, "Startup file name(s)", &itext4 };

struct NewWindow newreqwindow =
{   25, 60, 590, 260, 0, 15,
    GADGETUP|GADGETDOWN, ACTIVATE|SMART_REFRESH|NOCAREREFRESH,
    &if0gadg, NULL, POSTVER, NULL, NULL, 0, 0, 0, 0, CUSTOMSCREEN
};

struct NewWindow newerrwindow =
{   0, 100, 640, 100, 0, 15,
    0, SIMPLE_REFRESH|NOCAREREFRESH|BACKDROP|BORDERLESS,
    NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, CUSTOMSCREEN
};

/* Stuff for the windows */

struct PropInfo hscrollinfo =   /* Horizontal scroll proportional gadget */
{   AUTOKNOB|FREEHORIZ, -1, -1, -1, -1 };
struct Image hscrollimage;
struct Gadget hscrollgadg =
{   NULL, 0, -8, -14, 9,
    GRELBOTTOM|GRELWIDTH, BOTTOMBORDER|RELVERIFY, PROPGADGET,
    (APTR)&hscrollimage, NULL, NULL, 0, (APTR)&hscrollinfo, 0, NULL
};

struct PropInfo vscrollinfo =   /* Vertical scroll proportional gadget */
{   AUTOKNOB|FREEVERT, -1, -1, -1, -1 };
struct Image vscrollimage;
struct Gadget vscrollgadg =
{   &hscrollgadg, -15, 0, 16, -8,
    GRELRIGHT|GRELHEIGHT, RIGHTBORDER|RELVERIFY, PROPGADGET,
    (APTR)&vscrollimage, NULL, NULL, 0, (APTR)&vscrollinfo, 0, NULL
};

struct NewWindow newbitwindow = /* Bitmap window.  (Borders (4,2,18,10)) */
{   0, 49, 640, 463, -1, -1,
    0, SIMPLE_REFRESH,
    &vscrollgadg, NULL, NULL, NULL, NULL,
    0, 100, 0, 0, CUSTOMSCREEN
};

struct NewWindow newintwindow = /* Interactive window */
{   0, 12, 640, 37, -1, -1,
    0, SIZEBRIGHT|WINDOWSIZING|SMART_REFRESH|ACTIVATE,
    NULL, NULL, NULL, NULL, NULL,
    0, 0, 0, 400, CUSTOMSCREEN
};

struct IntuiText men2item3text = /* Menu strip */
{   7, 0, COMPLEMENT, 0, 0, NULL, "Kill", NULL };
struct MenuItem men2item3 =
{   NULL, 0, 60, 200, 11, ITEMTEXT|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men2item3text, NULL, NULL, NULL, MENUNULL
};

struct IntuiText men2item2text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Interrupt", NULL };
struct MenuItem men2item2 =
{   &men2item3, 0, 40, 200, 11, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men2item2text, NULL, 'I', NULL, MENUNULL
};

struct IntuiText men2item1text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Continue after pause", NULL };
struct MenuItem men2item1 =
{   &men2item2, 0, 20, 200, 11, ITEMTEXT|COMMSEQ|HIGHCOMP,
    0, (APTR)&men2item1text, NULL, 'C', NULL, MENUNULL
};

struct IntuiText men2item0text =
{   7, 0, COMPLEMENT, 19, 0, NULL, "Pause every page", NULL };
struct MenuItem men2item0 =
{   &men2item1, 0, 0, 200,11,
    CHECKIT|ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP|CHECKED,
    0, (APTR)&men2item0text, NULL, 'P', NULL, MENUNULL
};

struct Menu menu2 =
{   NULL, 160, 0, 80, 0, MENUENABLED, "Control", &men2item0 };

struct IntuiText men1item3text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Interactive", NULL };
struct MenuItem men1item3 =
{   NULL, 0, 60, 128, 11, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men1item3text, NULL, 'W', NULL, MENUNULL
};

struct IntuiText men1item2text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Run file", NULL };
struct MenuItem men1item2 =
{   &men1item3, 0, 40, 128, 11, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men1item2text, NULL, 'R', NULL, MENUNULL
};

struct IntuiText men1item1text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Load file", NULL };
struct MenuItem men1item1 =
{   &men1item2, 0, 20, 128, 11, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men1item1text, NULL, 'L', NULL, MENUNULL
};

struct IntuiText men1item0text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Load font", NULL };
struct MenuItem men1item0 =
{   &men1item1, 0, 0, 128, 11, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men1item0text, NULL, 'F', NULL, MENUNULL
};

struct Menu menu1 =
{   &menu2, 80, 0, 80, 0, 0, "File", &men1item0 };

struct IntuiText men0item1text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Quit", NULL };
struct MenuItem men0item1 =
{   NULL, 0, 20, 100, 11, ITEMTEXT|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men0item1text, NULL, NULL, NULL, MENUNULL
};

struct IntuiText men0item0text =
{   7, 0, COMPLEMENT, 0, 0, NULL, "Restart", NULL };
struct MenuItem men0item0 =
{   &men0item1, 0, 0, 100, 11, ITEMTEXT|ITEMENABLED|HIGHCOMP,
    0, (APTR)&men0item0text, NULL, NULL, NULL, MENUNULL
};

struct Menu menu0 =
{   &menu1, 0, 0, 80, 0, 0, "Project", &men0item0 };

/* Arguments */

int arglen, argwb, argfilec, argsizec, argmemc;
int argprint, argscreen, argiff, argint, argclosewb;
char *argbuf, *argsizev[5], *argmemv[5];
char *argfilev[] =
{   (char *) &if0gadgbuff[0],
    (char *) &if1gadgbuff[0],
    (char *) &if2gadgbuff[0],
    (char *) &if3gadgbuff[0],
    (char *) &if4gadgbuff[0]
};
char *argifn =
    (char *) &ifngadgbuff[0];
char *argcon = "CON";
char *argkey[] =
{   "IFF",
    "SCREEN",
    "PRINTER",
    "INTERACTIVE",
    "SIZE",
    "MEM",
    "CLOSEWB",
    "CONDEV",
    NULL
};

/* Lattice stuff for using Arp FPrintf/SPrintf */

LONG ArpFPrintf(BPTR, char*,);
LONG ArpSPrintf(char *, char*,);

# pragma libcall ArpBase ArpFPrintf 00EA 98003
# pragma libcall ArpBase ArpSPrintf 0282 98003

LONG __stdargs FPrintf(BPTR fh, char *format,)
{   return  ArpFPrintf(fh, format, (&format)+1);
}

LONG __stdargs SPrintf(char *string, char *format,)
{   return  ArpSPrintf(string, format, (&format)+1);
}

/* Startup code */

extern void main(int argc, char **argv);

void _main(char *line)
{   struct DiskObject *diskobj;
    char *diskname;
    char *argv[32];
    int argc;

    /* If this is a workbench startup construct the arguments string.  We
     * find the name of our icon from the first argument.  Then we laod the
     * icon and look for an ARGS string in the tooltypes.  We concatenate
     * the program name and the arguments into an Alloc'ed buffer */

    if (WBenchMsg && WBenchMsg->sm_NumArgs)
    {   diskname = WBenchMsg->sm_ArgList[0].wa_Name;
        line = NULL;
        IconBase = (struct IconBase *) OpenLibrary("icon.library", 0);
        if (IconBase)
        {   diskobj = GetDiskObject(diskname);
            line = FindToolType(diskobj->do_ToolTypes, "ARGS");
        }
        if (line)
        {   arglen = strlen(diskname) + strlen(line) + 2;
            argbuf = AllocMem(arglen, MEMF_CLEAR);
        }
        if (argbuf)
        {   strcpy(argbuf, diskname);
            strcat(argbuf, " ");
            strcat(argbuf, line);
        }
        line = argbuf;
        if (diskobj)
            FreeDiskObject(diskobj);
        if (IconBase)
            CloseLibrary((struct Library *) IconBase);
    }

    /* Parse the arguments to break words and strip quotes.  N.B. the
     * main program can determine that the arument was quoted by inspecting
     * the preceeding character */

    argc = 0;
    if (line == NULL) goto endline;
    for (;;)
    {   while (*line == ' ' || *line == '\t' || *line == '\n') line++;
        if (*line == 0) break;
        if (argc == 32)
        {   argc = 0;
            goto endline;
        }
        if (*line == '"')
        {   argv[argc] = ++line;
            while (*line != '"')
            {   if (*line == 0)
                {   argc = 0;
                    goto endline;
                }
                line++;
            }
        }
        else
        {   argv[argc] = line;
            while (*line != ' ' && *line != '\t' && *line != '\n')
            {   if (*line == 0)
                {   argc++;
                    goto endline;
                }
                line++;
            }
        }
        *line++ = 0;
        argc++;
    }
endline:

    /* Set up the standard input/output files */

    if (WBenchMsg) /* Workbench startup, no files */
    {   argwb = 1;
    }
    else           /* CLI startup, open stderr */
    {   errfh = Open("*", MODE_OLDFILE);
        if (errfh == NULL)
        {   retcode = 20;
            goto tidyexit;
        }
    }

    /* Call the main program  */

    main(argc, argv);

    /* Tidy up and exit */

tidyexit:
    if (argbuf) FreeMem(argbuf, arglen);
    if (errfh) Close(errfh);

    XCEXIT(retcode);
}

/* Main program */

void main(int argc, char **argv)
{   struct Message *msg;
    struct Gadget *gadget;
    char *s;
    int *ip, i, ch, l;
    int xsize, ysize, ssize, xoff, yoff, xden, yden, pden;

    process = (struct Process *) FindTask(NULL);
    syswindow = (struct Window *) process->pr_WindowPtr;

    /* Open the libraries */

    GfxBase =
        (struct GfxBase *)       OpenLibrary("graphics.library", 0);
    IntuitionBase =
        (struct IntuitionBase *) OpenLibrary("intuition.library", 0);
    ArpBase =
        (struct ArpBase *)       OpenLibrary("arp.library", 39);
    if (GfxBase == NULL || IntuitionBase == NULL || ArpBase == NULL)
    {   errmsg("can't open libraries (you need arp.library V39+)");
        goto errorexit;
    }
# ifndef STATICLINK
    PSbase = OpenLibrary("post.library", POSTVERNO);
    if (PSbase == NULL)
    {   errmsg("can't open post.library");
        goto errorexit;
    }
# endif

    /* Parse the arguments and keywords.  See the usage string below */

    if (argc == 0) goto badargs;
    argc--;
    argv++;
    if (argc == 0 || argwb) argint = 1;
    if (argc == 1 && strcmp(*argv, "?") == 0) goto usage;

    while (argc--)
    {   s = *argv++;
        i = -1;
        if (s[-1] != '"')
            for (;;)
            {   i++;
                if (argkey[i] == NULL)
                {   i = -1;
                    break;
                }
                if (stricmp(s, argkey[i]) == 0) break;
            }
        switch (i)
        {   case 0:    /* IFF */
                if (argc == 0 || strlen(*argv) >= 80) goto badargs;
                argc--;
                argiff = 1;
                strcpy(argifn, *argv++);
                break;

            case 1:    /* SCREEN */
                argscreen = 1;
                break;

            case 2:    /* PRINTER */
                argprint = 1;
                break;

            case 3:    /* INTERACTIVE */
                argint = 1;
                break;

            case 4:    /* SIZE */
                if (argc == 0) goto badargs;
                argc--;
                if (argsizec == 5) goto badargs;
                argsizev[argsizec++] = *argv++;
                break;

            case 5:    /* MEM */
                if (argc == 0) goto badargs;
                argc--;
                if (argmemc == 5) goto badargs;
                argmemv[argmemc++] = *argv++;
                break;

            case 6:    /* CLOSEWB */
                argclosewb = 1;
                break;

            case 7:    /* CONDEV */
                if (argc == 0) goto badargs;
                argc--;
                argcon = *argv++;
                break;

            default:
                if (argfilec == 5 || strlen(s) >= 80) goto badargs;
                strcpy(argfilev[argfilec++], s);
        }
    }
    if (!argscreen && !argiff && !argint)  argprint = 1;
    if (argscreen) argint = 1;

    /* Set up the screen size, adjusting for interlace */

    newscreen.Width = GfxBase->NormalDisplayColumns;
    newscreen.Height = GfxBase->NormalDisplayRows * 2;
    if (newscreen.Width < 400) newscreen.Width = 400;
    if (newscreen.Height < 300) newscreen.Height = 300;

    /* Set up the default page size.  For printer output we get the defaults
     * from preferences; otherwise we default to an A4 inch page at 75 dpi.
     * with 3 colour planes, trimming the width to fit onto the screen.  (We
     * can't quite fit A4 onto a standard 640 pixel wide screen - 8.24 inches
     * instead of 8.26). */

    bitmap.BytesPerRow = 1;
    bitmap.Rows = 1;
    bitmap.Flags = 0;
    bitmap.Depth = parm.page.depth = 3;
    if (argprint)
    {   setprinter();
        if (prport == NULL)
        {   errmsg("can't open printer device");
            goto errorexit;
        }
    }
    else
    {   parm.page.xsize = 824 * 75 / 100;
        parm.page.ysize = 1169 * 75 / 100;
        parm.page.xoff = parm.page.yoff = 0;
        parm.page.xden = parm.page.yden = 75;
        if (parm.page.xsize > newscreen.Width - 22)
            parm.page.xsize = newscreen.Width - 22;
    }
    parm.page.ydir = -1;

    /* Parse the "SIZE xyod..s..p.bc." options */

    xsize = ysize = ssize = xden = yden = pden = 0;
    for (i = 0; i < argsizec; i++)
    {   s = argsizev[i];
        for (;;)
        {   ch = *s++;
            if (ch == 0) break;
            ch = tolower(ch);
            switch (ch)
            {   case 'x':
                    if      (tolower(*s) == 'o')
                    {   s++;
                        if (!strtoint(&s, &xoff)) goto badvalue;
                        parm.page.xoff = xoff;
                        continue;
                    }
                    else if (tolower(*s) == 'd')
                    {   s++;
                        ip = &xden;
                    }
                    else
                        ip = &xsize;
                    break;

                case 'y':
                    if      (tolower(*s) == 'o')
                    {   s++;
                        if (!strtoint(&s, &yoff)) goto badvalue;
                        parm.page.yoff = yoff;
                        continue;
                    }
                    else if (tolower(*s) == 'd')
                    {   s++;
                        ip = &yden;
                    }
                    else
                        ip = &ysize;
                    break;

                case 's':
                    ip = &ssize;
                    break;

                case 'p':
                    ip = &pden;
                    break;

                case 'd':
                {   if (!strtoint(&s, &xden)) goto badvalue;
                    yden = xden;
                    continue;
                }

                case 'b':
                    parm.page.depth = 1;
                    continue;

                case 'c':
                    ch = *s;
                    if      (ch == '3')
                    {   s++;
                        parm.page.depth = 3;
                    }
                    else if (ch == '4')
                    {   s++;
                        parm.page.depth  = 4;
                    }
                    else
                        parm.page.depth = 3;
                    continue;

                default:
                    goto badvalue;
            }
            if (!strtoint(&s, ip)) goto badvalue;
        }
    }
    if (xden != 0) parm.page.xden = xden;
    if (yden != 0) parm.page.yden = yden;
    if (xsize != 0) parm.page.xsize = xsize;
    if (ysize != 0) parm.page.ysize = ysize;
    if (pden != 0)
    {   prden = pden;
        if (argprint) setprintden();
    }

    /* Set up the default memory sizes */

    parm.memvlen = defmemvlen;
    parm.memflen = defmemflen;
    parm.memllen = defmemllen;
    parm.memhlen = defmemhlen;

    /* Parse the "MEM fhlv.." options */

    for (i = 0; i < argmemc; i++)
    {   s = argmemv[i];
        for (;;)
        {   ch = *s++;
            if (ch == 0) break;
            ch = tolower(ch);
            switch (ch)
            {   case 'f':
                    ip = &parm.memflen;
                    break;

                case 'h':
                    ip = &parm.memhlen;
                    break;

                case 'l':
                    ip = &parm.memllen;
                    break;

                case 'v':
                    ip = &parm.memvlen;
                    break;

                default:
                    goto badvalue;
            }
            if (!strtoint(&s, ip)) goto badvalue;
        }
    }

    /* We only need the file requestors if we are interactive */

    if (argint)
    {   filereq1 = ArpAllocFreq();
        filereq2 = ArpAllocFreq();
        if (filereq1 == NULL || filereq2 == NULL)
        {   errmsg("can't get file requestors");
            goto errorexit;
        }
    }

    /* We seem to have to explicitly load the font from the disk before
     * Intuition can be depended upon to use it */

    if (argint)
    {  DiskfontBase = OpenLibrary("diskfont.library", 0);
       if (DiskfontBase)
       {   textfont = OpenDiskFont(&topaz11);
           CloseLibrary(DiskfontBase);
       }
       if (textfont == NULL)
       {   errmsg("can't find font topaz/11");
           goto errorexit;
       }
    }

    /* Set up the color map according to the number of bitplanes */

setcmap:
    restarterror = 0;
    if (!argint) newscreen.Depth = parm.page.depth;
    colormap.Count = 1 << newscreen.Depth;
    if      (newscreen.Depth == 1)
        colormap.ColorTable = (APTR) bcolors;
    else
        colormap.ColorTable = (APTR) ccolors;

    /* Open the screen and load the color map  */

    if (argint)
    {   screen = OpenScreen(&newscreen);
        if (screen == NULL)
        {   errmsg("can't open screen");
            goto errorexit;
        }
        newerrwindow.Screen = screen;
        newreqwindow.Screen = screen;
        newbitwindow.Screen = screen;
        newintwindow.Screen = screen;
        LoadRGB4(&screen->ViewPort,
                 (short *) colormap.ColorTable, colormap.Count);
        newerrwindow.Width = newscreen.Width;
        errwindow = OpenWindow(&newerrwindow);
        if (errwindow == NULL)
        {   errmsg("can't open error window");
            goto errorexit;
        }
        process->pr_WindowPtr = (APTR) errwindow;
    }
    restarterror = 1;

restart:
    if (fatalerror) goto tidyexit;
    retcode = 0;
    tidyup();

    if (argint)
    {   if (argclosewb) CloseWorkBench();
        if (requested == 0)
        {
            /* Put up the parameters requestor (window) */

            setreqgadgets();
            newreqwindow.IDCMPFlags = GADGETUP|GADGETDOWN;
            reqwindow = OpenWindow(&newreqwindow);
            if (reqwindow == NULL)
            {   errmsg("can't open parameters window");
                goto restart;
            }
            PrintIText(reqwindow->RPort, &itext3, 0, 0);

            /* Loop handling gadget messages */

            for (;;)
            {   WaitPort(reqwindow->UserPort);
                while (msg = GetMsg(reqwindow->UserPort))
                {   gadget = (struct Gadget *)
                        ((struct IntuiMessage *) msg)->IAddress;
                    i = gadget->GadgetID;
                    switch (i)
                    {   case GADGOK:
                        case GADGCAN:
                            goto closereq;

                        case GADGPRI:
                            argprint = getgadgbool(gadget);
                            if (argprint)
                            {    setprinter();
                                 if (prport == NULL)
                                 {   argprint = 0;
                                     errmsg("can't open printer device");
                                     i = GADGRST;
                                     goto closereq;
                                 }
                            }
                            goto setgadg;

                        case GADGPD1: case GADGPD2: case GADGPD3:
                        case GADGPD4: case GADGPD5: case GADGPD6:
                        case GADGPD7:
                            if (argprint)
                            {   prden = i - (GADGPD1 - 1);
                                setprintden();
                                goto setgadg;
                            }
                            break;

                        case GADGBW:
                            parm.page.depth = 1;
                            goto setgadg;

                        case GADGC3:
                            parm.page.depth = 3;
                            goto setgadg;

                        case GADGC4:
                            parm.page.depth = 4;
setgadg:                    setreqgadgets();
                            break;

                        case GADGABS:
                            getgadgabs(gadget);
                    }
                    ReplyMsg(msg);
                }
            }

            /* Close the requester window */

closereq:   ModifyIDCMP(reqwindow, CLOSEWINDOW);
            while (msg)
            {   ReplyMsg(msg);
                msg = GetMsg(reqwindow->UserPort);
            }
            CloseWindow(reqwindow);
            reqwindow = NULL;

            if (i == GADGCAN) goto tidyexit;
            if (i == GADGRST) goto restart;

            getreqgadgets();

            /* If we have changed our mind about the number of bitplanes
             * close the screen and go back to reopen it with the new depth
             */

            if (parm.page.depth != newscreen.Depth)
            {   process->pr_WindowPtr = (APTR) syswindow;
                CloseWindow(errwindow);
                errwindow = NULL;
                CloseScreen(screen);
                screen = NULL;
                newscreen.Depth = parm.page.depth;
                requested = 1;
                goto setcmap;
            }
        }
        requested = 0;
    }

    /* Set the size of the interactive windows */

    if (argint)
    {   newbitwindow.Width = newscreen.Width;
        newbitwindow.Height = newscreen.Height -
            newintwindow.TopEdge - newintwindow.Height;
        newbitwindow.TopEdge = newscreen.Height -
            newbitwindow.Height;
        newintwindow.Width = newscreen.Width;
        newintwindow.MaxHeight = newscreen.Height -
            newintwindow.TopEdge - newbitwindow.MinHeight;

    /* Locate the visible part of the bitmap within its window, adjusting
     * for the borders */

        winxbase = 4;
        winybase = 2;
        winxsize = newbitwindow.Width - 22;
        winysize = newbitwindow.Height - 12;

    /* Set the page size.  It must not be smaller than the interior of the
     * interactive window.  We may not need the horizontal scroll bar.  Each
     * bitmap row must start on a word boundary */

        if (parm.page.xsize <= winxsize)
        {   winysize += 8;
            vscrollgadg.NextGadget = 0;
            vscrollgadg.Height = 0;
        }
        else
        {   vscrollgadg.NextGadget = &hscrollgadg;
            vscrollgadg.Height = -8;
        }
        if (parm.page.xsize < winxsize) parm.page.xsize = winxsize;
        if (parm.page.ysize < winysize) parm.page.ysize = winysize;
    }
    parm.page.ybase = 0;
    parm.page.yheight = parm.page.ysize;
    if (!argint)
        if (ssize != 0 && ssize < parm.page.ysize) parm.page.ysize = ssize;
    if (parm.page.xsize == 0 || parm.page.ysize == 0)
    {   errmsg("page size not set in preferences");
        goto restart;
    }
    parm.page.xbytes = (parm.page.xsize + 15) >> 3 & 0xfffffffe;
    parm.page.len = parm.page.xbytes * parm.page.ysize;

    /* Allocate the page buffer.  It  must be in chip memory if we are
     * outputting to a screen */

    for (i = 0; i < parm.page.depth; i++)
    {   if ((parm.page.buf[i] = AllocMem(parm.page.len,
                 (argscreen ? MEMF_CHIP|MEMF_CLEAR :
                              MEMF_PUBLIC|MEMF_CLEAR))) == NULL)
        {   errmsg("can't get page buffer");
            goto restart;
        }
    }

    /* Initialise the bitmap */

    bitmap.BytesPerRow = parm.page.xbytes;
    bitmap.Rows = parm.page.ysize;
    bitmap.Flags = 0;
    bitmap.Depth = parm.page.depth;
    memcpy((char *) bitmap.Planes, (char *) parm.page.buf,
           sizeof bitmap.Planes);

    /* For interactive working, set up the windows */

    if (argint)
    {
        /* Finish initialising the gadgets and open the windows */

        hscrollinfo.HorizPot = 0;
        vscrollinfo.VertPot = 0xffff;
        hscrollinfo.HorizBody = (0xffff * winxsize) / parm.page.xsize;
        vscrollinfo.VertBody = (0xffff * winysize) / parm.page.ysize;
        winxpos = 0;
        winypos = parm.page.ysize - winysize;
        bitwindow = OpenWindow(&newbitwindow);
        intwindow = OpenWindow(&newintwindow);
        if (bitwindow == NULL || intwindow == NULL)
        {   errmsg("can't open windows");
            goto restart;
        }

        /* Set up new console streams.  Black on white characters */

        SPrintf((char *) undobuff, "%s:W%lx", argcon, intwindow);
        confh = Open((char *) undobuff, MODE_OLDFILE);
        if (confh == NULL)
        {   errmsg("can't open console device (you need ConMan V1.3+)");
            goto restart;
        }
        FPrintf(confh, "\x9b\x33\x37\x6d");
    }

        /* Create the menu handler task */

    if (argint)
    {   mainport = CreatePort(NULL, 0);
        if (mainport)
        {   menumsg.ExecMessage.mn_ReplyPort = mainport;
            menumsg.action = PSACTOPEN;
            menumsg.bitmap = &bitmap;
            menutask = CreateTask("post.menu", 6, (APTR) menuproc, 2000);
        }
        if (menutask)
            sendmenu(PSACTOPEN);
        if (menutask == NULL)
        {   errmsg("can't create menu handler");
            goto restart;
        }
    }

    /* Initialise for interpretation */

    insertbreak();
    SetExcept(~0, SIGBREAKF_CTRL_C);
    breakset = 1;
    insertftrap();
    ftrapset = 1;

# ifndef STATICLINK
    parm.flushfunc = (APTR) flushpage;
    parm.copyfunc = (APTR) copypage;
# endif

    if (argint)
    {   parm.infh = confh;
        parm.outfh = confh;
        parm.errfh = confh;
    }
    else
    {   parm.infh = Input();
        parm.outfh = Output();
        parm.errfh = errfh;
    }

    arec = PScreateact(&parm);
    if (arec == 0)
    {   errmsg("can't get memory");
        retcode = 20;
        goto restart;
    }
    if ((unsigned) arec <= errmax) retcode = 10;

    /* Interpret the argument files */

    for (i = 0; i < 5; i++)
    {   s = argfilev[i];
        if (s[0] != 0)
        {   if (retcode != 0) break;
            errcode =
                PSintstring(arec, s, -1, PSFLAGFILE|PSFLAGCLEAR|PSFLAGERASE);
            if (errcode) retcode = 10;
        }
    }

    /* Execute menu commands */

    if (argint)
    {   for (;;)
        {   sendmenu(retcode == 0 ? PSACTCOMMAND : PSACTEXIT);
            s = menumsg.string;
            switch (menumsg.command)
            {   case PSCOMQUIT:
                    goto quit;

                case PSCOMRESTART:
                    PSdeleteact(arec);
                    arec = 0;
                    goto restart;

                case PSCOMFILEF:
                    i = PSFLAGFILE|PSFLAGCLEAR|PSFLAGERASE;
                    strcpy(filereq1->fr_Dir, "PSFonts:");
                    filereq = filereq1;
                    filereq->fr_Hail = hailfilef;
                    goto freq;

                case PSCOMFILEL:
                    i = PSFLAGFILE|PSFLAGCLEAR|PSFLAGERASE;
                    filereq = filereq2;
                    filereq->fr_Hail = hailfilen;
                    goto freq;

                case PSCOMFILER:
                    i = PSFLAGFILE|PSFLAGCLEAR|PSFLAGERASE|PSFLAGSAVE;
                    filereq = filereq2;
                    filereq->fr_Hail = hailfiles;
freq:               filereq->fr_Window = intwindow;
                    filereq->fr_FuncFlags |= (FRF_NewIDCMP|FRF_DoColor);
                    filereq->fr_LeftEdge = 170;
                    filereq->fr_TopEdge = 150;
                    if (FileRequest(filereq) == NULL) break;
                    l = strlen(filereq->fr_Dir);
                    TackOn(filereq->fr_Dir, filereq->fr_File);
                    PSintstring(arec, filereq->fr_Dir, -1, i);
                    filereq->fr_Dir[l] = 0;
                    break;

                case PSCOMINTER:
                    PSintstring(arec, "%stdin", -1, 
                        PSFLAGFILE|PSFLAGCLEAR|PSFLAGERASE|PSFLAGSAVE|
                        PSFLAGINTER);
                    break;
            }
        }
    }

quit:
    PSdeleteact(arec);
    arec = 0;
    goto tidyexit;

    /* Argument errors and usage query */

badargs:
    errmsg("arguments bad, or value missing or too long");
    goto badusage;

badvalue:
    errmsg("argument bad value");

badusage:
    retcode = 20;

usage:
    if (!argwb) FPrintf(errfh, "post: usage:\n"
    "    post [files...] [IFF file] [SCREEN] [PRINTER] [INTERACTIVE]\n"
    "         [SIZE xyod..s..p.bc.] [MEM fhlv..] [CLOSEWB] [CONDEV con]\n");
    goto tidyexit;

    /* Tidy up and exit */

errorexit:
    retcode = 20;

tidyexit:
    tidyup();

    process->pr_WindowPtr = (APTR) syswindow;
    if (errwindow) CloseWindow(errwindow);
    if (screen) CloseScreen(screen);

    if (textfont) CloseFont(textfont);

    if (propen) CloseDevice((struct IORequest *) &prreq);
    if (prport) DeletePort(prport);

# ifndef STATICLINK
    if (PSbase)  CloseLibrary(PSbase);
# endif
    if (ArpBase) CloseLibrary((struct Library *) ArpBase);
    if (GfxBase) CloseLibrary((struct Library *) GfxBase);
    if (IntuitionBase)
    {   OpenWorkBench();
        CloseLibrary((struct Library *) IntuitionBase);
    }
}

/* Display an error requestor or message */

struct IntuiText bodytext2 =
{   AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE + 32, AUTOTOPEDGE + 11,
    AUTOITEXTFONT, NULL, NULL };
struct IntuiText bodytext =
{   AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE,
    AUTOITEXTFONT, POSTVER " Error", &bodytext2 };
struct IntuiText postext =
{   AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE,
    AUTOITEXTFONT, "RETRY", NULL };
struct IntuiText negtext =
{   AUTOFRONTPEN, AUTOBACKPEN, AUTODRAWMODE,
    AUTOLEFTEDGE, AUTOTOPEDGE,
    AUTOITEXTFONT, "CANCEL", NULL };

void errmsg(char *string)
{   int width;
    if (IntuitionBase && (errwindow || argwb || ArpBase == NULL))
    {   bodytext2.IText = string;
        width = IntuiTextLength(&bodytext2) + 70;
        if (width < 200) width = 200;
        if (!AutoRequest(errwindow, &bodytext,
                (restarterror ? &postext : NULL), &negtext,
                0, 0, width, 70))
            fatalerror = 1;
        return;
    }
    if (ArpBase && !argwb)
    {   FPrintf(errfh, "post: %s\n", string);
        fatalerror = 1;
        return;
    }
    fatalerror = 1;
}

/* Tidy up */

void tidyup(void)
{   int i;

    if (breakset)
    {   SetExcept(0, SIGBREAKF_CTRL_C);
        deletebreak();
        breakset = 0;
    }
    if (ftrapset)
    {   deleteftrap();
        ftrapset = 0;
    }

    if (menuport)
        sendmenu(PSACTCLOSE);
    if (mainport)
    {   DeletePort(mainport);
        mainport = NULL;
    }

    if (confh)
    {   Close(confh);
        confh = NULL;
    }

    /* N.B. some versions of ConMan close the window themselves, so we
     * don't close the interactive window it it appears to have been
     * closed already */

    if (bitwindow)
    {   CloseWindow(bitwindow);
        bitwindow = NULL;
    }
    if (intwindow)
    {   if (screen->FirstWindow == intwindow ||
            errwindow->NextWindow == intwindow) CloseWindow(intwindow);
        intwindow = NULL;
    }

    for (i = 0; i < parm.page.depth; i++)
        if (parm.page.buf[i])
        {   FreeMem(parm.page.buf[i], parm.page.len);
            parm.page.buf[i] = NULL;
        }
}

/* Open the printer device and set up the page size */

void setprinter(void)
{   if (propen == 0)
    {   if (OpenDevice("printer.device", 0,
                       (struct IOReqest *) &prreq, 0) != 0)
            return;
        propen = 1;
    }
    if (prport == NULL)
    {   prport = CreatePort(NULL, 0);
        if (prport == NULL) return;
        prreq.io_Message.mn_ReplyPort = prport;
        prsig = 1 << prport->mp_SigBit;
        prdata = (struct PrinterData *) prreq.io_Device;
        prextdata = &prdata->pd_SegmentData->ps_PED;
        prprefs = &prdata->pd_Preferences;
        prden = prprefs->PrintDensity;
    }
    if ((prprefs->PrintShade & SHADE_COLOR) == 0) parm.page.depth = 1;
    setprintden();
}

/* Set the printer density */

void setprintden(void)
{   int pxsize, pysize, xy;

    /* New density, call the device driver to change its extended data.  No
     * error check */

    if (prden > 7) prden = 7;
    if (prden != prprefs->PrintDensity)
    {   prreq.io_Command = PRD_DUMPRPORT;
        prrast.BitMap = &bitmap;
        prreq.io_RastPort = &prrast;
        prreq.io_ColorMap = (struct ColorMap *) &colormap;
        prreq.io_Modes = 0;
        prreq.io_SrcX = 0;
        prreq.io_SrcY = 0;
        prreq.io_SrcWidth = 1;
        prreq.io_SrcHeight = 1;
        prreq.io_DestCols = 1;
        prreq.io_DestRows = 1;
        prreq.io_Special = (SPECIAL_DENSITY1 * prden) | SPECIAL_NOPRINT;
        prprefs->PrintDensity = prden;
        DoIO((struct IORequest *) &prreq);
    }

    /* Extract the page size and density from the printer device preferences
     * and extended data */

    parm.page.xden = prextdata->ped_XDotsInch;
    parm.page.yden = prextdata->ped_YDotsInch;
    if      (prprefs->PrintFlags & PIXEL_DIMENSIONS)
    {   pxsize = prprefs->PrintMaxWidth;
        pysize = prprefs->PrintMaxHeight;
    }
    else if (prprefs->PrintFlags &
                (BOUNDED_DIMENSIONS|ABSOLUTE_DIMENSIONS))
    {   pxsize = prprefs->PrintMaxWidth * parm.page.xden / 10;
        pysize = prprefs->PrintMaxHeight * parm.page.yden / 10;
    }
    if (prprefs->PrintAspect & ASPECT_VERT)
    {   xy = pxsize;
        pxsize = pysize;
        pysize = xy;
    }
    if (pxsize != 0) parm.page.xsize = pxsize;
    if (pysize != 0) parm.page.ysize = pysize;
    parm.page.xoff = 0;
    parm.page.yoff = 0;
}

/* Set all requester gadgets */

void setreqgadgets(void)
{   setgadgbool(&bwgadg, (parm.page.depth == 1));
    setgadgbool(&c3gadg, (parm.page.depth == 3));
    setgadgbool(&c4gadg, (parm.page.depth == 4));
    setgadgbool(&pd1gadg, (prden == 1));
    setgadgbool(&pd2gadg, (prden == 2));
    setgadgbool(&pd3gadg, (prden == 3));
    setgadgbool(&pd4gadg, (prden == 4));
    setgadgbool(&pd5gadg, (prden == 5));
    setgadgbool(&pd6gadg, (prden == 6));
    setgadgbool(&pd7gadg, (prden == 7));
    setgadgint(&xgadg, parm.page.xsize);
    setgadgint(&ygadg, parm.page.ysize);
    setgadgint(&xogadg, parm.page.xoff);
    setgadgint(&yogadg, parm.page.yoff);
    setgadgint(&xdgadg, parm.page.xden);
    setgadgint(&ydgadg, parm.page.yden);
    if (reqwindow == NULL)
    {   setgadgbool(&prigadg, argprint);
        setgadgbool(&scrgadg, argscreen);
        setgadgbool(&iffgadg, argiff);
        setgadgbool(&cwbgadg, argclosewb);
        setgadgint(&mfgadg, parm.memflen);
        setgadgint(&mhgadg, parm.memhlen);
        setgadgint(&mvgadg, parm.memvlen);
        setgadgint(&mlgadg, parm.memllen);
    }
    enablegadg(&pd1gadg, argprint);
    enablegadg(&pd2gadg, argprint);
    enablegadg(&pd3gadg, argprint);
    enablegadg(&pd4gadg, argprint);
    enablegadg(&pd5gadg, argprint);
    enablegadg(&pd6gadg, argprint);
    enablegadg(&pd7gadg, argprint);
}

/* Get all requester gadgets */

void getreqgadgets(void)
{   argscreen = getgadgbool(&scrgadg);
    argiff = getgadgbool(&iffgadg);
    if (argifn[0] == 0) argiff = 0;
    parm.page.depth = getgadgbool(&bwgadg) ? 1 :
                      getgadgbool(&c3gadg) ? 3 : 4;
    argclosewb = getgadgbool(&cwbgadg);
    parm.page.xsize = getgadgabs(&xgadg);
    parm.page.ysize = getgadgabs(&ygadg);
    parm.page.xoff = getgadgint(&xogadg);
    parm.page.yoff = getgadgint(&yogadg);
    parm.page.xden = getgadgabs(&xdgadg);
    parm.page.yden = getgadgabs(&ydgadg);
    parm.memflen = getgadgabs(&mfgadg);
    parm.memhlen = getgadgabs(&mhgadg);
    parm.memvlen = getgadgabs(&mvgadg);
    parm.memllen = getgadgabs(&mlgadg);
}

/* Enable a gadget */

void enablegadg(struct Gadget *gadget, int enable)
{   if ((gadget->Flags & GADGDISABLED) != (enable ? 0 : GADGDISABLED))
    {   if (reqwindow)
            RemoveGadget(reqwindow, gadget);
        if (enable)
            gadget->Flags &= ~GADGDISABLED;
        else
            gadget->Flags |=  GADGDISABLED;
        if (reqwindow)
        {   AddGadget(reqwindow, gadget, 65535);
            RefreshGadgets(gadget, reqwindow, NULL);
        }
    }
}

/* Set the value of a boolean requester gadget */

void setgadgbool(struct Gadget *gadget, int value)
{   if (value == getgadgbool(gadget)) return;
    if (reqwindow)
        RemoveGadget(reqwindow, gadget);
    if (value)
        gadget->Flags |=  SELECTED;
    else
        gadget->Flags &= ~SELECTED;
    if (reqwindow)
    {   AddGadget(reqwindow, gadget, 65535);
        RefreshGadgets(gadget, reqwindow, NULL);
    }
}

/* Get the value of a boolean gadget */

int getgadgbool(struct Gadget *gadget)
{   return (gadget->Flags & SELECTED) != 0;
}

/* Set the value of an integer requester gadget */

void setgadgint(struct Gadget *gadget, int value)
{   struct StringInfo *info;
    if (reqwindow)
    {   if (value == getgadgint(gadget)) return;
        RemoveGadget(reqwindow, gadget);
    }
    info = (struct StringInfo *) gadget->SpecialInfo;
    info->LongInt = value;
    SPrintf((char *) info->Buffer, "%ld", value);
    if (reqwindow)
    {   AddGadget(reqwindow, gadget, 65535);
        RefreshGadgets(gadget, reqwindow, NULL);
    }
}

/* Get the value of an integer gadget */

int getgadgint(struct Gadget *gadget)
{   struct StringInfo *info = (struct StringInfo *) gadget->SpecialInfo;
    return (int) info->LongInt;
}

/* Get the absolute value of an integer gadget */

int getgadgabs(struct Gadget *gadget)
{   int value = getgadgint(gadget);
    if (value < 0)
    {   value = 0;
        setgadgint(gadget, value);
    }
    return value;
}

/* String to integer conversion; digits only, with error check */

int strtoint(char **sp, int *ip)
{   char *s = *sp;
    int i = 0;
    int ch;
    for (;;)
    {   ch = *s;
        if (ch < '0' || ch > '9') break;
        i = i * 10 + (ch - '0');
        s++;
    }
    if (s == *sp)
        return 0;
    else
    {   *sp = s;
        *ip = i;
        return 1;
    }
}

/* Send a mesage to the menu handler task */

void sendmenu(int action)
{   if (action != PSACTOPEN)
    {   menumsg.action = action;
        menumsg.result = retcode;
        menumsg.errnum = errcode;
        PutMsg(menuport, (struct Message *) &menumsg);
    }
    WaitPort(mainport);
    GetMsg(mainport);
    retcode = menumsg.result;
}

/* The menu handler task */

static int pause = 1;

void __saveds menuproc(void)
{   struct Message *msg;
    char *title, *oldtitle, *savetitle;
    int height, i, j;

    title = titlestart;
    height = newintwindow.Height;
    msg = (struct Message *) &menumsg;

    /* Loop handling messages.  We access the initial message directly,
     * rather than getting it from the menu port, as we havn't created the
     * port yet */

    for (;;)
    {
        /* Message from main program */

        if ((struct PSmessage *) msg == &menumsg)
        {   switch (menumsg.action)
            {
                /* Open the windows and initialise */

                case PSACTOPEN:
                    menuport = CreatePort(NULL, 0);
                    if (menuport == NULL)
                    {   menumsg.result = 20;
                        goto end;
                    }
                    bitwindow->UserPort = menuport;
                    intwindow->UserPort = menuport;
                    ModifyIDCMP(bitwindow,
                            REFRESHWINDOW|ACTIVEWINDOW|GADGETUP);
                    ModifyIDCMP(intwindow,
                            REFRESHWINDOW|MENUPICK);
                    menu2.Flags |= MENUENABLED;
                    SetMenuStrip(intwindow, &menu0);
                    SetWindowTitles(intwindow, NULL, (UBYTE *) title);
                    ReplyMsg(msg);
                    break;

                /* Close the windows and tidy up */

                case PSACTCLOSE:
                    ClearMenuStrip(intwindow);
                    ModifyIDCMP(bitwindow, CLOSEWINDOW);
                    ModifyIDCMP(intwindow, CLOSEWINDOW);
                    bitwindow->UserPort = NULL;
                    intwindow->UserPort = NULL;
                    for (;;)
                    {   msg = GetMsg(menuport);
                        if (msg == NULL) break;
                        ReplyMsg(msg);
                    }
                    DeletePort(menuport);
                    menuport = NULL;
                    goto end;

                /* Flush the bitmap to its window */

                case PSACTFLUSH:
                    i = menumsg.y1;
                    j = menumsg.y2;
                    if (i < winypos) i = winypos;
                    if (j < winypos) j = winypos;
                    if (i > winypos + winysize) i = winypos + winysize;
                    if (j > winypos + winysize) j = winypos + winysize;
                    if (j > i && argscreen)
                    {   BltBitMapRastPort(&bitmap, winxpos, i,
                                          bitwindow->RPort,
                                          winxbase, winybase + i - winypos,
                                          winxsize, j - i, 0xC0);
                    }
                    ReplyMsg(msg);
                    break;

                /* Pause at the end of a page */

                case PSACTPAUSE:
                    if (pause)
                    {   savetitle = title;
                        title = titlepaused;
                        SetWindowTitles(intwindow, NULL, (UBYTE *) title);
                        OnMenu(intwindow, 2 | SHIFTITEM(1));
                    }
                    else
                        ReplyMsg(msg);
                    break;

                /* Get the next command */

                case PSACTCOMMAND:
                    title = titlewait;
                    SetWindowTitles(intwindow, NULL, (UBYTE *) title);
                    OnMenu(intwindow, 0 | SHIFTITEM(NOITEM));
                    OnMenu(intwindow, 1 | SHIFTITEM(NOITEM));
                    break;

                /* Get a quit or restart command */

                case PSACTEXIT:
                    title = titlewait;
                    SetWindowTitles(intwindow, NULL, (UBYTE *) title);
                    OnMenu(intwindow, 0 | SHIFTITEM(NOITEM));
                    OffMenu(intwindow, 2 | SHIFTITEM(NOITEM));
                    break;
            }
        }

        /* Message from Intuition */

        else
        {   switch (((struct IntuiMessage *) msg)->Class)
            {
                /* Make the interactive window the active one, but only if
                 * the scroll gadget knobs in the bitmapped window are not
                 * currently hit (so as not to prevent dragging them). (If
                 * someone is dragging them we will get a GADGETUP message
                 * when he lets go.) */

                case ACTIVEWINDOW:
                    if (!(hscrollgadg.Flags & SELECTED) &&
                        !(vscrollgadg.Flags & SELECTED))
                        ActivateWindow(intwindow);
                    break;

                /* Refresh a window */

                case REFRESHWINDOW:

                    /* Refresh the bitmapped window */

                    if (((struct IntuiMessage *)msg)->IDCMPWindow ==
                                                                 bitwindow)
                    {   BeginRefresh(bitwindow);
                        if (argscreen)
                            BltBitMapRastPort(&bitmap, winxpos, winypos,
                                              bitwindow->RPort,
                                              winxbase, winybase,
                                              winxsize, winysize, 0xC0);
                        EndRefresh(bitwindow, TRUE);
                    }

                    /* We don't actually refresh the interactive window, but
                     * instead we use this event to tell us when it has
                     * changed size, so we can adjust the size and position
                     * of the bitmapped window to match.  (Despite what the
                     * manual says we seem to get the refresh message even
                     * when the window gets smaller */

                    else
                    {   i = intwindow->Height;
                        j = i - height;
                        height = i;
                        if      (j < 0)
                        {   MoveWindow(bitwindow, 0, j);
                            SizeWindow(bitwindow, 0, -j);
                        }
                        else if (j > 0)
                        {   SizeWindow(bitwindow, 0, -j);
                            MoveWindow(bitwindow, 0, j);
                        }
                        winypos += j;
                        if (winypos < 0) winypos = 0;
                        winysize -= j;
                        ModifyProp(&vscrollgadg, bitwindow, NULL,
                            AUTOKNOB|FREEVERT,
                            -1, (0xffff * winypos) /
                                    (parm.page.ysize - winysize),
                            -1, (0xffff * winysize) / parm.page.ysize);
                    }

                    break;

                /* Scroll the bitmapped window.  Make sure we can scroll to
                 * the edges of the page, even after rounding erors */

                case GADGETUP:
                    ActivateWindow(intwindow);
                    if      (((struct IntuiMessage *) msg)->IAddress ==
                                    (APTR) &hscrollgadg)
                    {   i = (parm.page.xsize - winxsize);
                        j = (i * hscrollinfo.HorizPot) / 65535;
                        if (j < i / 30) j = 0;
                        if (i - j < i / 30) j = i;
                        winxpos = j;
                    }
                    else if (((struct IntuiMessage *) msg)->IAddress ==
                                    (APTR) &vscrollgadg)
                    {   i = (parm.page.ysize - winysize);
                        j = (i * vscrollinfo.VertPot) / 65535;
                        if (j < i / 30) j = 0;
                        if (i - j < i / 30) j = i;
                        winypos = j;
                    }
                    if (argscreen)
                        BltBitMapRastPort(&bitmap, winxpos, winypos,
                                          bitwindow->RPort,
                                          winxbase, winybase,
                                          winxsize, winysize, 0xC0);
                    break;

                /* Menu selection.  We don't implement extended selection
                 * to avoid real time problems, and it wouldn't be useful
                 * for our range of choices anyway */

                case MENUPICK:
                    i = ((struct IntuiMessage *) msg)->Code;
                    j = ITEMNUM(i);
                    i = MENUNUM(i);
                    oldtitle = title;
                    if      (i == 0)     /* Project */
                    {   if      (j == 0) /*   Restart */
                            menumsg.command = PSCOMRESTART;
                        else if (j == 1) /*   Quit */
                            menumsg.command = PSCOMQUIT;
                    }
                    else if (i == 1)     /* File */
                    {   title = titlerunning;
                        if      (j == 0) /*   Load font */
                            menumsg.command = PSCOMFILEF;
                        else if (j == 1) /*   Load file */
                            menumsg.command = PSCOMFILEL;
                        else if (j == 2) /*   Run file */
                            menumsg.command = PSCOMFILER;
                        else if (j == 3) /*   Interactive */
                        {   menumsg.command = PSCOMINTER;
                            title = titleinter;
                        }
                    }
                    else if (i == 2)     /* Control */
                    {   if      (j == 0) /*   Pause every page */
                        {   pause = !pause;
                            if (pause)
                                men2item0.Flags |=  CHECKED;
                            else
                                men2item0.Flags &= ~CHECKED;
                            break;
                        }
                        else if (j == 1) /*   Continue after pause */
                        {   menumsg.command = 0;
                            title = savetitle;
                            OffMenu(intwindow, 2 | SHIFTITEM(1));
                        }
                        else if (j == 2) /*   Interrupt */
                        {   PSsignalint(arec, 1);
                            break;
                        }
                        else if (j == 3) /*   Kill */
                        {   PSsignalint(arec, 2);
                            break;
                        }
                    }
                    else                 /* NULL */
                        break;
                    OffMenu(intwindow, 0 | SHIFTITEM(NOITEM));
                    OffMenu(intwindow, 1 | SHIFTITEM(NOITEM));
                    if (title != oldtitle)
                        SetWindowTitles(intwindow, NULL, (UBYTE *) title);
                    menumsg.length = -1;
                    menumsg.string = NULL;
                    ReplyMsg((struct Message *) &menumsg);
                    break;
            }
            ReplyMsg(msg);
        }

        /* Get next message */

        for (;;)
        {   msg = GetMsg(menuport);
            if (msg) break;
            WaitPort(menuport);
        }
    }

    /* Open failure or close.  Reply remove our task */

end:
    Forbid();
    ReplyMsg((struct Message *) &menumsg);
    menutask = NULL;
    RemTask(menutask);
}

/* Signal an interrupt */

void __saveds sigint()
{   PSsignalint(arec, 1);
}

/* Signal a floating point error */

void __saveds sigfpe()
{   PSsignalfpe(arec);
}

/* Call an external function (dummy) */

# ifdef STATICLINK
int callextfunc(APTR func, APTR aptr, int parms)
{   return 0;
}
# endif

/* Flush the page to the screen */

# ifdef STATICLINK
void flushpage(int y1, int y2)
# else
void __saveds __asm flushpage(register __d0 int y1, register __d1 int y2)
# endif
{   if (argscreen)
    {   menumsg.y1 = y1;
        menumsg.y2 = y2;
        sendmenu(PSACTFLUSH);
    }
}

/* Copy the page to the output */

# ifdef STATICLINK
void copypage(int num)
# else
void __saveds __asm copypage(register __d0 int num)
# endif
{   ioerror = 0;
    if (argprint)
        while (num--) printpage();
    if (argiff)
        iffpage();
    if (argscreen)
        sendmenu(PSACTPAUSE);
    if (ioerror) PSerror(arec, ioerror);
}

/* Print the page */

void printpage()
{   ULONG sig;
    UWORD prflags;

    /* Disable break exceptions so we can wait on the signal instead */

    SetExcept(0, SIGBREAKF_CTRL_C);
    breakset = 0;

    /* First check the printer is ready */

    prreq.io_Command = PRD_QUERY;
    ((struct IOStdReq *) &prreq)->io_Data = (APTR) prstatus;
    if (DoIO((struct IORequest *) &prreq))
    {   ioerror = errioerror;
        return;
    }
    if (((struct IOStdReq *) &prreq)->io_Actual == 1 && prstatus[0] & 3 != 0)
        FPrintf(argint ? confh : errfh,
                "post: printer not ready (CTRL/C to abort)\n");

    /* Now dump the page */

    prrast.BitMap = &bitmap;
    prreq.io_Command = PRD_DUMPRPORT;
    prreq.io_RastPort = &prrast;
    prreq.io_ColorMap = (struct ColorMap *) &colormap;
    prreq.io_Modes = 0;
    prreq.io_SrcX = 0;
    prreq.io_SrcY = 0;
    prreq.io_SrcWidth = parm.page.xsize;
    prreq.io_SrcHeight = parm.page.ysize;
    prreq.io_Special = (SPECIAL_DENSITY1 * prden) | SPECIAL_TRUSTME;
    if (parm.page.ybase + parm.page.ysize >= parm.page.yheight)
        prreq.io_SrcHeight = parm.page.yheight - parm.page.ybase;
    else
        prreq.io_Special |= SPECIAL_NOFORMFEED;
    if (prextdata->ped_MaxXDots != 0)
        if (prreq.io_SrcWidth > prextdata->ped_MaxXDots)
            prreq.io_SrcWidth = prextdata->ped_MaxXDots;
    if (prextdata->ped_MaxYDots != 0)
        if (prreq.io_SrcHeight > prextdata->ped_MaxYDots)
            prreq.io_SrcHeight = prextdata->ped_MaxYDots;
    prreq.io_DestCols = prreq.io_SrcWidth;
    prreq.io_DestRows = prreq.io_SrcHeight;
    prflags = prprefs->PrintFlags;
    prprefs->PrintFlags = prflags & ~DIMENSIONS_MASK | IGNORE_DIMENSIONS;

    /* We use asynchronous IO so we can abort it with a CTRL/C */

    SendIO((struct IORequest *) &prreq);

    for (;;)
    {   sig = Wait(prsig | SIGBREAKF_CTRL_C);
        if (sig & SIGBREAKF_CTRL_C)
        {   AbortIO((struct IORequest *) &prreq);
            WaitIO((struct IORequest *) &prreq);
            ioerror = errioerror;
            break;
        }
        if (GetMsg(prport))
            break;
    }
    if (prreq.io_Error != 0) ioerror = errioerror;

    /* Restore break exceptions */

    SetExcept(~0, SIGBREAKF_CTRL_C);
    breakset = 1;

    prprefs->PrintFlags = prflags;
}

/* IFF ILBM routines */

static int iffseq = 0;

static int ifferr;

static FILE *ifffptr;

/* Put a byte */

static void iffputb(int b)
{   if (ifferr) return;
    if (putc((int) b, ifffptr) == EOF)
    {   ifferr = 1;
        return;
    }
}

/* Put a word */

static void iffputw(int w)
{   iffputb(w>>8);
    iffputb(w);
}

/* Put a long */

static void iffputl(int l)
{   iffputb(l>>24);
    iffputb(l>>16);
    iffputb(l>>8);
    iffputb(l);
}

/* Put a string */

static void iffputs(char *str)
{   while (*str) iffputb(*str++);
}

/* Pack a bitmap row */

static void iffpack(char *buf, int len)
{   int b, c, l;
    if (ifferr) return;
    l = 0;
    while (len--)
    {   b = *buf++;                  /* Pick up a byte */
        c = 1;
        while (len && *buf == b && c < 128)
        {   c++;
            buf++;
            len--;                   /* See if it begins a run */
        }
        if (c == 2 &&                /* If a two byte run */
            l > 0 &&                 /*    and preceeded by literals */
            l <= 125 &&              /*    and not more than 125 of them */
            (len <= 2 ||             /*    and no more than 2 bytes left */
             *buf != *(buf + 1)))    /*        or not followed by a run */
        {   c = 1;                   /* Then make it a literal */
            buf--;
            len++;
        }
        if (c == 1)                  /* If not a run */
        {   l++;                     /* Then it must be a literal */
            c = 0;
        }
        if (l > 0 &&                 /* If we have some literals */
            (c > 1 ||                /*    and beginning a run */
             l == 127 ||             /*    or reached 127 */
             len == 0))              /*    or no more bytes left */
        {   if (putc(l - 1, ifffptr) == EOF)
            {   ifferr = 1;
                return;
            }
            while (l)              /* Then write out  the literals */
            {   if (putc(*(buf - c - l), ifffptr) == EOF)
                {   ifferr = 1;
                    return;
                }
                l--;
            }
        }
        if (c > 1)                   /* If we have a run, write it */
        {   if (putc(1 - c, ifffptr) == EOF)
            {   ifferr = 1;
                return;
            }
            if (putc(b, ifffptr) == EOF)
            {   ifferr = 1;
                return;
            }
        }
    }
}

/* Determine the current address */

static long ifftell(void)
{   long addr;
    if (ifferr) return 0;
    if ((addr = ftell(ifffptr)) == -1)
    {   ifferr = 1;
        return 0;
    }
    return addr;
}

/* Fix up the length of a chunk */

static void ifffixup(long addr)
{   long size;
    if (ifferr) return;
    if ((size = ftell(ifffptr)) == -1)
    {   ifferr = 1;
        return;
    }
    if (size & 1) iffputb(0);
    size = size - addr - 8;
    if (fseek(ifffptr, addr + 4, 0) != 0)
    {   ifferr = 1;
        return;
    }
    iffputl(size);
    if (fseek(ifffptr, 0, 2) != 0)
    {   ifferr = 1;
        return;
    }
}

/* Write the page to the iff output file */

void iffpage(void)
{   long addr1, addr2;
    int xa, ya;
    int i, j, k, ch;
    UWORD w;
    int fslen;
    char fname[110], fsnum[10];

    /* Compute the aspect ratio.  Make sure the values fit into a byte */

    xa = parm.page.yden;
    ya = parm.page.xden;
    i = igcd(xa, ya);
    xa /= i;
    ya /= i;
    while (xa > 255 && ya > 255)
    {   xa /= 2;
        ya /= 2;
    }

    /* Construct the file name.  Copy it, replacing "*" by the sequence
     * number.  The scan it backwards replacing "?" by digits */

    iffseq++;
    fslen = 0;
    i = iffseq;
    while (i)
    {   fsnum[fslen++] = i % 10 + '0';
        i /= 10;
    }
    i = j = 0;
    for (;;)
    {   if (j > 100)
        {   ioerror = errioerror;
            return;
        }
        ch = argifn[i++];
        if (ch == '*')
        {   k = fslen;
            while (k--) fname[j++] = fsnum[k];
        }
        else
           fname[j++] = ch;
        if (ch == 0) break;
    }
    k = 0;
    while (--j)
    {   if (fname[j] == '?')
            fname[j] = (k < fslen) ? fsnum[k++] : '0';
    }

    /* Open the file, write a FORM ILBM, and close it again */

    ifferr = 0;
    ifffptr = fopen(fname, "wb");
    if (ifffptr == NULL) ifferr = 1;

    addr1 = ifftell();
    iffputs("FORM");          /* FORM ILBM */
    iffputl(0);
    iffputs("ILBM");

    iffputs("BMHD");          /* BMHD */
    iffputl(20);
    iffputw(parm.page.xsize); /* Width */
    iffputw(parm.page.ysize); /* Height */
    iffputw(0);               /* X position */
    iffputw(0);               /* Y position */
    iffputb(parm.page.depth); /* Number of bit planes */
    iffputb(0);               /* Masking:     None */
    iffputb(1);               /* Compression: ByteRun */
    iffputb(0);               /* Pad1 */
    iffputw(0);               /* Transparent colour */
    iffputb(xa);              /* X aspect */
    iffputb(ya);              /* Y aspect */
    iffputw(parm.page.xsize); /* Source width */
    iffputw(parm.page.ysize); /* Source height */

    addr2 = ifftell();
    iffputs("CMAP");          /* CMAP */
    iffputl(0);
    for (i = 0; i < colormap.Count; i++)
    {   w = ((UWORD *) colormap.ColorTable)[i];
        iffputb(((w >> 8) & 15) << 4);
        iffputb(((w >> 4) & 15) << 4);
        iffputb(( w       & 15) << 4);
    }
    ifffixup(addr2);

    if (argscreen)
    {   iffputs("CAMG");      /* CAMG */
        iffputl(4);
        iffputl(newscreen.ViewModes);
    }

    addr2 = ifftell();
    iffputs("BODY");          /* BODY */
    iffputl(0);
    for (j = 0; j < bitmap.Rows; j++)
        for (i = 0; i < bitmap.Depth; i++)
            iffpack((char *) bitmap.Planes[i] + j * bitmap.BytesPerRow,
                    bitmap.BytesPerRow);
    ifffixup(addr2);

    ifffixup(addr1);

    if (fclose(ifffptr) == EOF)
        ifferr = 1;
    if (ifferr) ioerror = errioerror;
}

/* Find the greatest common divisor of two positive integers.  If one is
 * zero the result is the other */

int igcd(int n, int m)
{   unsigned int n1, m1, r;
    if      (n > m)
    {   n1 = n;
        m1 = m;
    }
    else if (m > n)
    {   n1 = m;
        m1 = n;
    }
    else
        return n;
    while (m1 != 0)
    {   r = n1 % m1;
        n1 = m1;
        m1 = r;
    }
    return (int) n1;
}

/* End of file "post.c" */
