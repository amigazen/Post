/* PostScript interpreter file "postlib.h" - library interface header (Amiga)
 * (C) Adrian Aylward 1989, 1990.
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

/* The version number */

# define POSTVER "Post V1.3"

/* The parameter block */

struct PSparm
{   struct
    {   char *buf[24];
        int len;
        short depth, reserved[3];
        short xoff, yoff;
        short xbytes, xsize, ysize, ybase, yheight;
        short xden, yden, ydir;
    } page;
    int memvlen, memflen, memllen, memhlen;
    APTR userdata, flushfunc, copyfunc;
    BPTR infh, outfh, errfh;
    int reserved[4];
};

/* Flags */

# define PSFLAGSTRING  1 /* Interpret string */
# define PSFLAGFILE    2 /* Interpret file */
# define PSFLAGINTER   4 /* Interactive, issue propmts */
# define PSFLAGCLEAR   8 /* Clear stacks afterwards */
# define PSFLAGSAVE   16 /* Save and restore */
# define PSFLAGERASE  32 /* Erase page afterwards */

/* Errors */

# ifndef errmax
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
# define errmax                 26
# endif

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

extern int  PScreateact(struct PSparm *parm);
extern void PSdeleteact(int arec);
extern int  PSintstring(int arec, char *string, int length, int flags);
extern void PSsignalint(int arec, int flag);
extern void PSsignalfpe(int arec);
extern void PSerror(int arec, int errnum);

# ifndef STATICLINK
# ifdef LATTICE
# pragma libcall PSbase PScreateact  1e    901 ; d0 = (a1)
# pragma libcall PSbase PSdeleteact  24    801 ;      (a0)
# pragma libcall PSbase PSintstring  2A 109804 ; d0 = (a0, a1, d0, d1)
# pragma libcall PSbase PSsignalint  30   0802 ;      (a0, d0)
# pragma libcall PSbase PSsignalfpe  36    801 ;      (a0)
# pragma libcall PSbase PSerror      3C   0802 ;      (a0, d0)
# endif
# endif

/* End of file "postlib.h" */
