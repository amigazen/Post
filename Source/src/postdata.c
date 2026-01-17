/* PostScript interpreter file "postdata.c" - external data definitions */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* Prompts */

int prompting = 0;

char prompt1[promptsize + 1], prompt2[promptsize + 1];

/* Times */

long time1, time2;

/* The random number */

unsigned int random;

/* The virtual machine */

int packing;
int vmnest;
int vmparms, vmsegno, vmsegsize;
int vmused, vmhwm, vmmax;

char *vmbeg[vmsegmax];
int vmnext[vmsegmax], vmsize[vmsegmax];

struct vmframe vmstack[vmstacksize + 1];

/* Dictionaries */

struct dictionary *systemdptr, *userdptr, *errordptr, *errdsdptr;

/* The name table */

char namebuf[namebufsize + 1];

vmref *nametable;

/* The stacks */

int opernest, execnest, dictnest;

struct object *operstack, *execstack, *dictstack;

/* The file table */

struct file *filetable;

/* Object types and flags */

char *typetable[typemax + 1] =
{   "nulltype",
    "marktype",
    "savetype",
    "operatortype",
    "filetype",
    "integertype",
    "realtype",
    "booleantype",
    "arraytype",
    "packedarraytype",
    "stringtype",
    "nametype",
    "dicttype",
    "fonttype"
};

/* Errors */

char *errortable[errmax + 1] =
{   "handleerror",
    "dictfull",
    "dictstackoverflow",
    "dictstackunderflow",
    "execstackoverflow",
    "interrupt",
    "invalidaccess",
    "invalidexit",
    "invalidfileaccess",
    "invalidfont",
    "invalidrestore",
    "invalidstop",
    "ioerror",
    "limitcheck",
    "nocurrentpoint",
    "rangecheck",
    "stackoverflow",
    "stackunderflow",
    "syntaxerror",
    "timeout",
    "typecheck",
    "undefined",
    "undefinedfilename",
    "undefinedresult",
    "unmatchedmark",
    "unregistered",
    "VMerror",
    "memoryallocation",
    "kill"
};

struct object errorname[errmax + 1];

struct object errordict;

char *errdstable[edsmax] =
{   "newerror",
    "errorname",
    "command",
    "ostack",
    "estack",
    "dstack"
};

struct object errdsname[edsmax], errdstoken[edsmax];

struct object errdsdict;

int errorflag, errornum;
char *errorstring;

int returncode;

int intsigflag;

/* The operator table */

struct object *currtoken;

int opnum;
struct operator *optable;

/* Various names for character operations */

char *chartable[charmax] =
{   "StandardEncoding",
    ".notdef",
    "FontDirectory",
    "FontType",
    "FontMatrix",
    "FontBBox",
    "Encoding",
    "FID",
    "UniqueID",
    "BuildChar",
    "Metrics",
    "PaintType",
    "StrokeWidth",
    "CharStrings",
    "Private",
    "Subrs",
    "lenIV",
    "BlueValues",
    "OtherBlues",
    "BlueScale",
    "BlueShift",
    "BlueFuzz"
};

struct object charname[charmax];

/* The standard encoding vector */

char *stdentable[192] =
{ "space",         "exclam",        "quotedbl",      "numbersign", /* 20 */
  "dollar",        "percent",       "ampersand",     "quoteright",
  "parenleft",     "parenright",    "asterisk",      "plus",
  "comma",         "hyphen",        "period",        "slash",
  "zero",          "one",           "two",           "three",      /* 30 */
  "four",          "five",          "six",           "seven",
  "eight",         "nine",          "colon",         "semicolon",
  "less",          "equal",         "greater",       "question",
  "at",            "A",             "B",             "C",          /* 40 */
  "D",             "E",             "F",             "G",
  "H",             "I",             "J",             "K",
  "L",             "M",             "N",             "O",
  "P",             "Q",             "R",             "S",          /* 50 */
  "T",             "U",             "V",             "W",
  "X",             "Y",             "Z",             "bracketleft",
  "backslash",     "bracketright",  "asciicircum",   "underscore",
  "quoteleft",     "a",             "b",             "c",          /* 60 */
  "d",             "e",             "f",             "g",
  "h",             "i",             "j",             "k",
  "l",             "m",             "n",             "o",
  "p",             "q",             "r",             "s",          /* 70 */
  "t",             "u",             "v",             "w",
  "x",             "y",             "z",             "braceleft",
  "bar",           "braceright",    "asciitilde",    0,
  0,               "exclamdown",    "cent",          "sterling",   /* A0 */
  "fraction",      "yen",           "florin",        "section",
  "currency",      "quotesingle",   "quotedblleft",  "guillemotleft",
  "guilsinglleft", "guilsinglright","fi",            "fl",
  0,               "endash",        "dagger",        "daggerdbl",  /* B0 */
  "periodcentered",0,               "paragraph",     "bullet",
  "quotesinglbase","quotedblbase",  "quotedblright", "guillemotright",
  "ellipsis",      "perthousand",   0,               "questiondown",
  0,               "grave",         "acute",         "circumflex", /* C0 */
  "tilde",         "macron",        "breve",         "dotaccent",
  "dieresis",      0,               "ring",          "cedilla",
  0,               "hungarumlaut",  "ogonek",        "caron",
  "emdash",        0,               0,               0,            /* D0 */
  0,               0,               0,               0,
  0,               0,               0,               0,
  0,               0,               0,               0,
  0,               "AE",            0,               "ordfeminine",/* E0 */
  0,               0,               0,               0,
  "Lslash",        "Oslash",        "OE",            "ordmasculine",
  0,               0,               0,               0,
  0,               "ae",            0,               0,            /* F0 */
  0,               "dotlessi",      0,               0,
  "lslash",        "oslash",        "oe",            "germandbls",
  0,               0,               0,               0
};

struct object *stdencoding;

/* Paths, lines, and line segments */

int pathsize;
int pathbeg, pathend, pathseg;

struct point *patharray;

int linesize;
int lineend;

struct line *linearray;
struct line **lineptr;

struct lineseg *linesegarray;

int clipsize;
int clipend;

struct clip *cliparray;
struct clip **clipptr;

/* The halftone screen */

int halfok;

char *halfbeg;
int halfsize;

struct halftone halftone[4];

int screenok;

struct halfscreen halfscreen[4];

/* The page buffer and output device */

struct device page;

/* The x buffers, y bucket array, and line filling limits */

int xbsize;
int *xshf;
char *xbuf;

int ybflag;
struct line **ybucket;

double ylwb, yupb;
double ymin, ymax;

/* The graphics state */

int gnest;

struct gstate gstate, *gstack;

struct object copies;

/* The interpreter recursion stack */

int inest;

struct istate istate, *istack;

/* The font directory and make cache */

struct object fontdir;

int fonttype, fontid, fontencodlen;
vmref fontmatrix, fontencoding;

struct fmrec *fmcache;

int fmcount;

/* The font character cache */

struct fcrec **fccache;

struct fcrec *fcbeg, *fcend, *fcptr;

int fclen, fclimit;

/* Font build data for type 1 */

int fontpainttype;
float fontstrokewidth;
vmref fontmetrics, fontcharstrings, fontprivate;
struct object fontbuildproc, fontsubrs;
int fontleniv;

/* Allocated memory */

void *memfbeg, *memhbeg;
void *memlbeg, *mempbeg, *memibeg, *memxbeg, *memybeg;

int   memflen,  memhlen;
int   memllen,  memplen,  memilen,  memxlen,  memylen;

int   memvmin,  memlmin,  mempmin;

/* End of file "postdata.c" */
