/* PostScript interpreter file "post.h" - header file
 * (C) Adrian Aylward 1989, 1992
 * V1.6 First source release
 * V1.7 Fix stroking lines of zero length
 */

# ifndef POST_H
# define POST_H

/* Operating system or machine */

/* # define AMIGA  (defined for us on the Amiga!) */
/* # define UNIX */

/* Standard libraries */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <setjmp.h>
# include <math.h>
# include <signal.h>

/* Use in line code for 68881 */

# ifdef AMIGA
# ifdef LATTICE
# ifdef M68881
# include <m68881.h>
# endif
# endif
# endif

/* So we can reassign the standard io files */

# define sstdin  stdin
# define sstdout stdout
# define sstderr stderr

/* Maximum and minimum */

# ifndef max
# define max(a,b) ((a)>(b)?(a):(b))
# endif
# ifndef min
# define min(a,b) ((a)<(b)?(a):(b))
# endif

/* Machine word alignment */

# define mcalign 4

/* Conversions from floats to integers */

# define itrunc(rval) ((int) (rval))

/* Constants for arcs and Bezier curves */

# define degtorad   .0174532925199432957692
# define radtodeg 57.295779513082320876

# define twopi     6.28318530717958647692 /* pi*2 */
# define pi        3.14159265358979323846 /* pi   */
# define pi2       1.57079632679489661923 /* pi/2 */

/* A very small angle.  (Anything smaller is a complete circle). */

# define pdelta     .00001

# define sqrt2     1.41421356237309504880 /* sqrt(2) */

# define f43rt2     .55228474983079339840 /* 4/3 * (sqrt(2)-1) / sqrt(2) */
# define f43       1.33333333333333333333 /* 4/3 */

# define f12       0.5                    /* 1/2 */
# define f14       0.25                   /* 1/4 */
# define f18       0.125                  /* 1/8 */
# define f38       0.375                  /* 3/8 */

/* Encryption constants */

# define eshift        8
# define emask     65535
# define ec1       52845
# define ec2       22719
# define einitexec 55665
# define einitchar  4330

/* Limits */

# define maxdepth        20
# define istacksize       9
# define gstacksize      31

# define systemdictsize 300
# define userdictsize   200
# define fontdictsize   100
# define errordictsize   40
# define errdsdictsize   10

# define dashsize 11

# define maxargs         20

/* Version */

extern char version[];

/* Prompts */

# define promptsize 20

extern int prompting;

extern char prompt1[promptsize + 1], prompt2[promptsize + 1];

/* Times */

extern long time1, time2;

/* The random number */

extern unsigned int random;

/* The virtual machine */

typedef unsigned vmref;

# define vmsegmax        64
# define vmstacksize     15     /* N.B. packing code assumes <= 256 */
# define vmhashsize      31

extern int packing;
extern int vmnest;
extern int vmparms, vmsegno, vmsegsize;
extern int vmused, vmhwm, vmmax;

extern char *vmbeg[vmsegmax]; 
extern int vmnext[vmsegmax], vmsize[vmsegmax]; 

struct vmframe
{   int generation, gnest, packing;
    int vsegno, vnext, vused;
    vmref hlist[vmhashsize + 1];
};

struct vmlist
{   vmref chain, vref;
    int length;
};

extern struct vmframe vmstack[vmstacksize + 1];

/* Objects */

struct object
{   unsigned char type, flags;
    unsigned short length;
    union
    {    int               ival;
         float             rval;
         vmref             vref;
    } value;
};

/* Dictionaries */

union dictkey
{   struct object keyobj;
    unsigned int keyint[2];
};

struct dictentry
{   struct object key, val;
};

struct dictionary
{   unsigned char type, flags;
    unsigned short slots, size, full, saved, filler;
    vmref vref;
    int length;
    struct dictentry entries[1];
};

/* The name table */

struct name
{   vmref chain;
    unsigned int hash;
    unsigned short length;
    char string[2];
};

# define namebufsize 128

extern char namebuf[namebufsize + 1];

# define nametablesize 512

extern vmref *nametable;

/* The stacks */

# define operstacksize 500
# define execstacksize 250
# define dictstacksize  20

extern int opernest, execnest, dictnest;

extern struct object *operstack, *execstack, *dictstack;

/* The file table */

# define openread  1
# define openwrite 2
# define openfont  4

# define filetablesize 20       /* N.B. packing code assumes <= 256 */

struct file
{   int generation, saved, open;
    int ch, uflg;
    int stype, slen;
    int emode, erand;
    FILE *fptr;
};

extern struct file *filetable;

/* Object types and flags */

# define typenull        0   /* null object is binary zeros */
# define typemark        1   /* mark */
# define typesave        2   /* save object */
# define typeoper        3   /* operator */
# define typefile        4   /* file */
# define typeint         5   /* integer */
# define typereal        6   /* real */
# define typebool        7   /* boolean */
# define typearray       8   /* array */
# define typepacked      9   /* packedarray */
# define typestring     10   /* string */
# define typename       11   /* name */
# define typedict       12   /* dictionary */
# define typefont       13   /* fontID */
# define typemax        14

# define typechar       14   /* char integer - only when packed */
# define typeoper2      15   /* operator (bound name) - only when packed */

# define flagexec      128   /* executable (not literal) */
# define flagwprot      64   /* write protection */
# define flagrprot      32   /* read protection */
# define flagxprot      16   /* execute protection */
# define flagctrl        8   /* control (on execution stack) */
# define flagloop        4   /* loop    (on execution stack) */
# define flagrun         2   /* run     (on execution stack) */
# define flagstop        1   /* stopped (on execution stack) */

extern char *typetable[typemax + 1];

/* Errors */

# define errdictfull             1
# define errdictstackoverflow    2
# define errdictstackunderflow   3
# define errexecstackoverflow    4
# define errinterrupt            5
# define errinvalidaccess        6
# define errinvalidexit          7
# define errinvalidfileaccess    8
# define errinvalidfont          9
# define errinvalidrestore      10
# define errinvalidstop         11
# define errioerror             12
# define errlimitcheck          13
# define errnocurrentpoint      14
# define errrangecheck          15
# define errstackoverflow       16
# define errstackunderflow      17
# define errsyntaxerror         18
# define errtimeout             19
# define errtypecheck           20
# define errundefined           21
# define errundefinedfilename   22
# define errundefinedresult     23
# define errunmatchedmark       24
# define errunregistered        25
# define errVMerror             26
# define errmemoryallocation    27
# define errkill                28
# define errmax                 28

extern char *errortable[errmax + 1];

extern struct object errorname[errmax + 1];

extern struct object errordict;

# define edsnewerror             0
# define edserrorname            1
# define edscommand              2
# define edsostack               3
# define edsestack               4
# define edsdstack               5
# define edsmax                  6

extern char *errdstable[edsmax];

extern struct object errdsname[edsmax], errdstoken[edsmax];

extern struct object errdsdict;

extern int errorflag, errornum;
extern char *errorstring;

extern int returncode;

extern int intsigflag;

/* The operator table */

extern struct object *currtoken;

struct operator
{   void (*func)();
    char *sptr;
};

# define optablesize 250        /* N.B. packing code assumes <= 256 */

extern int opnum;
extern struct operator *optable;

/* Various names for character operations */

# define charstdencoding         0
# define charnotdef              1
# define charfontdirectory       2
# define charfonttype            3
# define charfontmatrix          4
# define charfontbbox            5
# define charencoding            6
# define charfid                 7
# define charuniqueid            8
# define charbuildchar           9
# define charmetrics            10
# define charpainttype          11
# define charstrokewidth        12
# define charcharstrings        13
# define charprivate            14
# define charsubrs              15
# define charleniv              16
# define charbluevalues         17
# define charotherblues         18
# define charbluescale          19
# define charblueshift          20
# define charbluefuzz           21
# define charmax                22

extern char *chartable[charmax];

extern struct object charname[charmax];

/* The standard encoding vector */

extern char *stdentable[192];

extern struct object *stdencoding;

/* Paths, lines, and line segments */

# define ptmove   1
# define ptline   2
# define ptcurve  3
# define ptclosei 4
# define ptclosex 5

struct point
{   int type;
    float x, y;
};

extern int pathsize;
extern int pathbeg, pathend, pathseg;

extern struct point *patharray;

struct line
{   struct line *chain;
    signed char cdir, fdir;
    short filler;
    short y1, y2;
    int xx, dx, d1, d2;
};

extern int linesize;
extern int lineend;

extern struct line *linearray;
extern struct line **lineptr;

struct lineseg
{   signed char cdir, fdir;
    short x;
};

extern struct lineseg *linesegarray;

struct clip
{   struct clip *chain;
    signed char cdir, fdir;
    char flag, swap;
    int x1, y1, x2, y2;
};

extern int clipsize;
extern int clipend;

extern struct clip *cliparray;
extern struct clip **clipptr;

/* The halftone screens */

extern int halfok;

extern char *halfbeg;
extern int halfsize;

struct halftone
{   int    psize, xsize, ysize;
    int    area, xrep;
    int    dx, dy;
    short *spotkey;
    short *spotsub;
    short *spotnum;
    char **spotptr;
    int    spotmax, spotnext;
};

extern struct halftone halftone[4];

extern int screenok;

struct halfscreen
{   int    num;
    char  *ptr;
    struct halftone *halftone;
};

extern struct halfscreen halfscreen[4];

/* The page buffer and output device */

struct device
{   char *buf[24];
    int len;
    short depth, reserved[3];
    short xoff, yoff;
    short xbytes, xsize, ysize, ybase, yheight;
    short xden, yden, ydir;
};

extern struct device page;

/* The x buffers, y bucket array, and line filling limits */

extern int xbsize;
extern int *xshf;
extern char *xbuf;

extern int ybflag;
extern struct line **ybucket;

# define xlwb -1000.0 * 256.0
# define xupb 31767.0 * 256.0

extern double ylwb, yupb;
extern double ymin, ymax;

/* The graphics state */

struct gstate
{   struct object font;
    struct object dasharray; 
    struct object transfer[4];
    struct object screenfunc[4];
    struct object ucr, gcr;
    struct device dev;
    float ctm[6];
    float linewidth;
    float flatness;
    float dashoffset, dashlength[dashsize];
    float mitrelimit, mitresin;
    float gray, rgb[3], rgbw[4];
    float shade[4];
    float screenfreq[4], screenangle[4];
    short clipbeg, pathbeg, pathend;
    short linecap, linejoin;
    short transdepth;
    short screendepth;
    short shadeok, clipflag, cacheflag, nullflag;
};

extern int gnest;

extern struct gstate gstate, *gstack;

extern struct object copies;

/* The interpreter state flags */

# define intgraph                1
# define intchar                 2

/* The interpreter recursion stack */

struct istate
{   int flags, type;
    int vmbase, gbase;
    int execbase;
    int fclim;
    struct fcrec *pfcrec;
    jmp_buf errjmp;
};

extern int inest;

extern struct istate istate, *istack;

/* The font directory and make cache */

extern struct object fontdir;

extern int fonttype, fontid, fontencodlen;
extern vmref fontmatrix, fontencoding;

# define fmsize     20

struct fmrec
{   int count, id, encodlen;
    vmref encoding, dref;
    float matrix[6];
};

extern struct fmrec *fmcache;

extern int fmcount;

/* The font character cache */

# define fcsize    256

struct fcrec
{   struct fcrec *chain;
    vmref dref, nref;
    struct point width;
    float matrix[4];
    int reclen, count, id;
    unsigned int hash;
    int len;
    short xbytes, xsize, ysize;
    short xoffset, yoffset;
};

extern struct fcrec **fccache;

extern struct fcrec *fcbeg, *fcend, *fcptr;

extern int fclen, fclimit;

/* Font build data for type 1 */

extern int fontpainttype;
extern float fontstrokewidth;
extern vmref fontmetrics, fontcharstrings, fontprivate;
extern struct object fontbuildproc, fontsubrs;
extern int fontleniv;

/* Allocated memory */

# define memlsegmax  20
# define mempsegmax  20

extern void *memfbeg, *memhbeg;
extern void *memlbeg, *mempbeg, *memibeg, *memxbeg, *memybeg;

extern int   memflen,  memhlen;
extern int   memllen,  memplen,  memilen,  memxlen,  memylen;

extern int   memvmin,  memlmin,  mempmin;

/* Implementation dependent routines */

extern int  flushlevel(int level);
extern void flushlpage(int y1, int y2);
extern int  copypage(int num);
extern void *memalloc(int length);
extern void memfree(void *beg, int length);

/* Routines  - file "postint.c" */

/* Build a virtual machine reference */

# define vmxref(s,n) ((s<<24)+n)

/* Convert a virtual machine reference to a pointer */

# define vmsptr(v) (vmbeg[v>>24]+(v&0xffffff))
# define vmaptr(v) ((struct object *)(vmsptr(v)))
# define vmnptr(v) ((struct name *)(vmsptr(v)))
# define vmdptr(v) ((struct dictionary *)(vmsptr(v)))
# define vmvptr(v) ((void *)(vmsptr(v)))

/* Check if a virtual machine reference is newer than a save frame */

# define vmscheck(f,v) (v>=vmxref(f->vsegno,f->vnext))

/* Allocate a new array */

# define arrayalloc(length) \
    vmalloc((length) * sizeof (struct object))

/* Save an array before updating it */

# define arraysave(aref, length) \
    vmsavemem(aref, (length) * sizeof (struct object))

/* Copy an array */

# define arraycopy(aptr1, aptr2, length) \
    memcpy((char *) (aptr1), (char *) (aptr2), \
           (length) * sizeof (struct object))

extern void initint(int parms);
extern void tidyint(void);
extern void systemname(struct object *token, char *sptr, int flags);
extern void systemop(void (*func)(), char *sptr);
extern void interpret(struct object *interpreting);
extern void pushint(void);
extern void popint(void);
extern void putch(FILE *fptr, int ch);
extern void putstr(FILE *fptr, char *str);
extern void putmem(FILE *fptr, char *str, int length);
extern void putcheck(FILE *fptr, char *str, int length);
extern void fileopen(struct object *token, int open, char *name, int length);
extern void fileclose(struct object *token);
extern struct file *filecheck(struct object *token, int open);
extern int  getfseg(struct file *file);
extern void fileeinit(struct object *token);
extern int  readch(struct object *input, int depth);
extern void unreadch(struct object *input, int ch);
extern int  digitval(int ch);
extern int  scantoken(struct object *token, struct object *input, int depth);
extern int  numtoken(struct object *token, char *string);
extern void nametoken(struct object *token, char *string,
                      int length, int flags);
extern void dicttoken(struct object *token, int size);
extern void dictput(vmref dref, struct object *key, struct object *val);
extern int  dictget(vmref dref, struct object *key, struct object *val,
                    int flags);
extern int  dictfind(struct object *key, struct object *val);
extern vmref arraypack(struct object *aptr, int length);
extern void arrayunpk(struct object *aptr, char *sptr, int length);
extern int  pack(struct object *token, char *sptr);
extern int  unpack(struct object *token, char *sptr);
extern void vminit(int parms);
extern void vmtidy(void);
extern void vmparm(int parm, void *bag, int size);
extern vmref vmalloc(int size);
extern void *vmallocv(int size);
extern void *vmxptr(vmref vref);
extern char *vmstring(int length, int size);
extern void vmallocseg(int blksize, int length);
extern void vmsavemem(vmref vref, int length);
extern void vmsave(struct object *token);
extern void vmrest(int nest, int generation);
extern void vmrestcheck(struct vmframe *vmframe,
                        struct object *stackptr, int stackcnt);
extern void vmrestfiles(int nest);
extern void vmrestnames(struct vmframe *vmframe);
extern void vmrestmem(int nest);
extern int  cvstring(struct object *token, char *sptr, int length);
extern void printequals(FILE *fptr, struct object *token);
extern void printeqeq(FILE *fptr, struct object *token,
                      int depth, int count);
extern int  equal(struct object *token1, struct object *token2);
extern int  compare(struct object *token1, struct object *token2);
extern void bind(struct object *proc, int depth);
extern int  usertime(void);
extern void stop(void);
extern void error(int errnum);
extern void errorarray(struct object *token1, struct object *aptr,
                       int length);
extern void errorexit(void);
extern void errormsg(void);
extern void errorjmp(int nest, int num);

/* Routines  - file "postchar.c" */

extern void initchar(void);
extern void vmrestfont(struct vmframe *vmframe);
extern void definefont(struct object *key, struct object *font);
extern void findfont(struct object *key, struct object *font);
extern void makefont(struct object *font, float *matrix);
extern void setfont(struct object *font);
extern void checkfont(struct object *font, int fid);
extern void show(struct object *string, int type,
                 struct point *width, int wchar, struct object *proc);
extern void charpath(void);
extern void setcharwidth(struct point *width);
extern void setcachedevice(struct point *ll, struct point *ur, int ftype);
extern void cachestatus(int *status);
extern void nulldevice(void);
extern void cacheimage(struct fcrec *pfcrec, struct point *point);

/* Routines  - file "postfont.c" */

extern void initfont(void);
extern void initbuild(vmref dref);
extern void buildchar(int schar);

/* Routines  - file "postgraph.c" */

extern void initgstate(void);
extern void initgraphics(void);
extern void initctm(float newctm[]);
extern void setdevice(struct device *ppage);
extern void checkimagesize(int len);
extern void checklinesize(int size);
extern void checkpathsize(int size);
extern void cliplength(int cliplen);
extern void clippath(void);
extern void gsave(void);
extern void grest(void);
extern void grestall(void);
extern void getmatrix(vmref aref, float *matrix);
extern void putmatrix(vmref aref, float *matrix);
extern void multiplymatrix(float *mat1, float *mat2, float *mat3);
extern void invertmatrix(float *mat1, float *mat2);
extern void transform(struct point *ppoint, float *matrix);
extern void dtransform(struct point *ppoint, float *matrix);
extern void itransform(struct point *ppoint, float *matrix);
extern void idtransform(struct point *ppoint, float *matrix);
extern void arc(int dir, float radaa[3],
                struct point *centre, struct point *beg, struct point *end);
extern void closepath(int type);
extern void flattenpath(void);
extern void strokepath(int fillflag);
extern void clip(int eoflag);

/* Routines  - file "postdraw.c" */

extern void setupfill(void);
extern void fill(int beg, int end, int eoflag);
extern void filllines(struct point *ppoint, int count, int cdir, int fdir);
extern void setybucket(int base, int size);
extern void image(int width, int height, int bps, float *matrix, int mode,
                  int ncol, int multi, struct object *procs);
extern void erasepage(void);

/* Routines  - file "postshade.c" */

extern void mapcolour(int n1, float *colour1, int n2, float *colour2);
extern void setcolour(int n, float *colour);
extern void gethsbcolour(float *hsb);
extern void sethsbcolour(float *hsb);
extern void calltransfer(float *colour, float *shade);
extern void setupshade(void);
extern void setuphalf(void);
extern void setupscreen(float *shade);

/* Routines  - files postop1.c, postop2.c, postop3.c, postop4.c */

extern void initop1(void);
extern void initop2(void);
extern void initop3(void);
extern void initop4(void);

# endif

/* End of file "post.h" */
