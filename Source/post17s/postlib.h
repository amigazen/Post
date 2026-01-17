/* PostScript interpreter file "postlib.h" - library interface header (Amiga)
 * (C) Adrian Aylward 1989, 1991.
 *
 * You may freely copy and use this file.  It was written for Lattice C
 * V5.05.  N.B. all ints are 32 bits!
 *
 * This file defines the library interface, so that other programs can
 * use the PostScript drawing machinery.  It is totally Amiga specific.
 *
 * N.B. the symbol STATICLINK is used for building a statically linked
 * version of Post for debugging purposes.  It should not be defined for
 * normal use.
 */

# ifndef POSTLIB_H
# define POSTLIB_H

/* The version number */

# define POSTVER   "Post V1.7"
# define POSTVERNO 17

/* Default and minimum memory sizes */

# define defmemflen  60000
# define defmemhlen  20000
# define defmemvlen  50000
# define defmemllen  10000
# define minmemflen   5000
# define minmemhlen   1000
# define minmemvlen   5000
# define minmemllen   1000

/* The device page and parameter block */

struct PSdevice
{   char *buf[24];
    int len;
    short depth, reserved[3];
    short xoff, yoff;
    short xbytes, xsize, ysize, ybase, yheight;
    short xden, yden, ydir;
};

/* The parameter block */

struct PSparm
{   struct PSdevice page;
    int memvlen, memflen, memllen, memhlen;
    APTR userdata, flushfunc, copyfunc;
    BPTR infh, outfh, errfh;
    int funcmax;
    APTR *functab;
    int reserved[2];
};

/* Virtual machine objects */

struct PSobject
{   unsigned char type, flags;
    unsigned short length;
    union
    {    int               ival;
         float             rval;
         unsigned          vref;
    } value;
};

/* Object types and flags */

# ifndef POST_H
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

# define flagexec      128   /* executable (not literal) */
# define flagwprot      64   /* write protection */
# define flagrprot      32   /* read protection */
# define flagxprot      16   /* execute protection */
# endif

/* Errors */

# ifndef POST_H
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
# endif

/* Flags */

# define PSFLAGSTRING  1 /* Interpret string */
# define PSFLAGFILE    2 /* Interpret file */
# define PSFLAGINTER   4 /* Interactive, issue propmts */
# define PSFLAGCLEAR   8 /* Clear stacks afterwards */
# define PSFLAGSAVE   16 /* Save and restore */
# define PSFLAGERASE  32 /* Erase page afterwards */

/* Entry points */

# ifdef STATICLINK
# define PScreateact(parm)          initialise(parm)
# define PSdeleteact(arec)          terminate()
# define PSintstring(arec, s, l, f) intstring(s, l, f)
# define PSsignalint(arec, f)       signalint(f)
# define PSsignalfpe(arec)          signalfpe()
# define PSerror(arec, n)           error(n)
# endif

/* Entry points */

extern int      PScreateact(struct PSparm *parm);
extern void     PSdeleteact(int arec);
extern int      PSintstring(int arec, char *string, int length, int flags);
extern void     PSsignalint(int arec, int flag);
extern void     PSsignalfpe(int arec);
extern void     PSerror(int arec, int errnum);
extern unsigned PSallocvm(int arec, int size);
extern void    *PSvreftoptr(int arec, unsigned vref);
extern void     PSsetdevice(int arec, struct PSdevice *page);

# ifndef STATICLINK
# ifdef LATTICE
# pragma libcall PSbase PScreateact  1e    901 ; d0 = (a1)
# pragma libcall PSbase PSdeleteact  24    801 ;      (a0)
# pragma libcall PSbase PSintstring  2A 109804 ; d0 = (a0, a1, d0, d1)
# pragma libcall PSbase PSsignalint  30   0802 ;      (a0, d0)
# pragma libcall PSbase PSsignalfpe  36    801 ;      (a0)
# pragma libcall PSbase PSerror      3C   0802 ;      (a0, d0)
# pragma libcall PSbase PSallocvm    42   0802 ; d0 = (a0, d0)
# pragma libcall PSbase PSvreftoptr  48   0802 ; d0 = (a0, d0)
# pragma libcall PSbase PSsetdevice  4E   9802 ;      (a0, a1)
# pragma libcall PSbase PSerrstr     54   0802 ; d0 = (a0, d0)
# endif
# endif

# endif

/* End of file "postlib.h" */
