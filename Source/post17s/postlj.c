/* LaserJet print driver for Post.  File "postlj.c"
 * (C) Adrian Aylward 1990, 1992
 *
 * You may freely copy, use, and modify this file.
 *
 * This program prints PostScript files to a LaserJet.  It sends the output
 * to the PAR: handler.  Page size and orientation are read from the command
 * line.  There are no printer status checks; if the output hangs check your
 * printer is ready.  It is totally Amiga specific.
 *
 * The program was tested using Lattice C V5.05.  It has various Lattice
 * dependencies.
 *
 * V1.6 First full source release
 * V1.7 New options for custom page size and DeskJet etc.
 */

# include <dos.h>
# include <devices/printer.h>
# include <devices/prtbase.h>
# include <exec/exec.h>
# include <exec/execbase.h>
# include <exec/tasks.h>
# include <graphics/gfx.h>
# include <graphics/rastport.h>
# include <proto/dos.h>
# include <proto/exec.h>
# include <string.h>
# include <stdio.h>
# include <fcntl.h>
# include <ios1.h>

# include "postlib.h"

# undef  POSTVERNO
# define POSTVERNO 15  /* We need post.library version 1.5+ */

/* Assembler routines */

extern void insertbreak(void);
extern void deletebreak(void);
extern void insertftrap(void);
extern void deleteftrap(void);

/* Routines defined and referenced only within this module */

extern int  strtoint(char **sp, int *ip);
extern void __saveds __asm copypage(register __d0 int num);
extern void prtsetup(void);
extern void prtreset(void);
extern void prtdump(int copies);
extern void prtdumpline(char *buf, int len);

/* Lattice startup */

extern struct UFB _ufbs[];

/* External data (initialised to zero) */

int retcode;

int arec;

BPTR errfh;
FILE *parfp;

struct library *PSbase;
struct PSparm parm;

int breakset, ftrapset;

/* Options */

# define DEFSIZE 3 /* A4 page size */
# define DEFLAND 0 /* Portrait orientation */
# define DEFLJ   2 /* LaserJet IIP,III,IIIp etc. */

# define MAXSIZE 8
# define MAXLJ   3

int optsize = DEFSIZE;
int optland = DEFLAND;
int optlj   = DEFLJ;
int optgc;
int optcopies;
int optbeg, optend;
int optxsize, optysize;
int optlmarg, optrmarg, optumarg, optdmarg;
int opthoff, optvoff;

int pagenum;
int hoffset, voffset, vmarg;

/* Page size tables.
 * (See Figures 2-2 and 2-3 in the LaserJet 2P Technical Reference Manual.)
 *
 *                     Let   Legal Exec  A4    COM10 Mon   C5    DL
 */

int psize[MAXSIZE] = {    2,    3,    1,   26,   81,   80,   91,   90 };

int xsize[MAXSIZE] = { 2550, 2550, 2175, 2480, 1237, 1162, 1913, 1299 };
int ysize[MAXSIZE] = { 3300, 4200, 3150, 3507, 2850, 2250, 2704, 2598 };
int ppoff[MAXSIZE] = {   75,   75,   75,   71,   75,   75,   71,   71 };
int ploff[MAXSIZE] = {   60,   60,   60,   59,   60,   60,   59,   59 };

char *showlj[MAXLJ] = {"DeskJet", "LaserJet II", "LaserJet IIP/III" };
char *showsize[MAXSIZE] = {"letter", "legal", "executive", "A4",
                           "COM-10", "monarch", "C5", "DL" };
char *showland[2] = {"vertical", "horizontal" };

int compsize;
char *compbuf;

/* Arguments */

char *argto = "par:";
int argfilec, argmemc;
char *argfilev[5], *argmemv[5];
char *argkey[] =
{   "TO", "MEM", NULL };

/* Startup code */

extern void main(int argc, char **argv);

void _main(char *line)
{   char *argv[32];
    int argc;

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

    errfh = Open("*", MODE_OLDFILE);
    if (errfh == NULL)
    {   retcode = 20;
        goto tidyexit;
    }
    _ufbs[2].ufbfh = (long) errfh;
    _ufbs[2].ufbflg |= UFB_WA|O_RAW|UFB_NC;
    stderr->_file = 2;
    stderr->_flag = _IOWRT;

    /* Call the main program  */

    main(argc, argv);

    /* Tidy up and exit */

tidyexit:
    if (errfh) Close(errfh);

    XCEXIT(retcode);
}

/* Main program */

void main(int argc, char **argv)
{   char *s, *t;
    int *ip, i, m, ch;
    int x, y, l, r, u, d, h, v;

    /* Open the libraries */

    PSbase = OpenLibrary("post.library", POSTVERNO);
    if (PSbase == NULL)
    {   fprintf(stderr, "postlj: can't open post.library\n");
        goto errorexit;
    }

    /* Parse the arguments and keywords.  See the usage string below */

    argc--;
    argv++;
    if (argc == 0 || (argc == 1 && strcmp(*argv, "?") == 0)) goto query;

    optxsize = optysize = -1;
    optlmarg = optrmarg = optumarg = optdmarg = -1;
    opthoff = optvoff = -1;
    optgc = -1;

    while (argc)
    {   s = *argv;
        if (*s != '-') break;
        argv++;
        argc--;
        if (strcmp(s, "--") == 0) break;
        s++;
        while (t = s, ch = *s++)
        {   switch (ch)
            {   case 'S': case 's':
                   m = MAXSIZE;
                   ip = &optsize;
                   break;

                case 'A': case 'a':
                   m = 2;
                   ip = &optland;
                   break;

                case 'J': case 'j':
                   m = MAXLJ;
                   ip = &optlj;
                   break;

                case 'G': case 'g':
                   m = 2;
                   ip = &optgc;
                   break;

                case 'B': case 'b':
                   m = 10000;
                   ip = &optbeg;
                   break;

                case 'E': case 'e':
                   m = 10000;
                   ip = &optend;
                   break;

                case 'C': case 'c':
                   m = 100;
                   ip = &optcopies;
                   break;

                case 'X': case 'x':
                   m = 30000;
                   ip = &optxsize;
                   break;

                case 'Y': case 'y':
                   m = 30000;
                   ip = &optysize;
                   break;

                case 'L': case 'l':
                   m = 30000;
                   ip = &optlmarg;
                   break;

                case 'R': case 'r':
                   m = 30000;
                   ip = &optrmarg;
                   break;

                case 'U': case 'u':
                   m = 30000;
                   ip = &optumarg;
                   break;

                case 'D': case 'd':
                   m = 30000;
                   ip = &optdmarg;
                   break;

                case 'H': case 'h':
                   m = 30000;
                   ip = &opthoff;
                   break;

                case 'V': case 'v':
                   m = 30000;
                   ip = &optvoff;
                   break;

                default:
                   fprintf(stderr,
                           "postlj: unknown option \"%c\"\n", ch);
                   goto badusage;
            }
            if (!strtoint(&s, &i)) goto badvalue;
            if ((unsigned) i >= m)
            {   fprintf(stderr,
                        "postlj: option value out of range "
                        "(0-%d) \"%.*s\"\n", m - 1, s - t, t);
                goto errorexit;
            }
            *ip = i;
        }
    }

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
        {   case  0:    /* TO */
                if (argc == 0) goto badargs;
                argc--;
                argto = *argv++;
                break;

            case  1:    /* MEM */
                if (argc == 0) goto badargs;
                argc--;
                if (argmemc == 5) goto badargs;
                argmemv[argmemc++] = *argv++;
                break;

            default:
                if (argfilec == 5) goto badargs;
                argfilev[argfilec++] = s;
        }
    }

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

    /* Determine the page size */


    x = xsize[optsize];
    y = ysize[optsize];
    h = ppoff[optsize];
    v = ploff[optsize];
    if (optland)
    {   i = x;
        x = y;
        y = i;
        i = h;
        h = v;
        v = i;
    }
    if      (optlj == 0) /* DeskJet..., margins are 150 pixels */
    {   l = r = 150;
        u = d = 150;
    }
    else if (optlj == 1) /* LJ II..., printable area is logical page */
    {   l = r = h;
        u = d = v;
    }
    else                 /* LJ IIP/III..., margins are 50 pixels */
    {   l = r = 50;
        u = d = 50;
        v = u;
    }
    if (optxsize >= 0) x = optxsize;
    if (optysize >= 0) y = optysize;
    if (optlmarg >= 0) l = optlmarg;
    if (optrmarg >= 0) r = optrmarg;
    if (optumarg >= 0) u = optumarg;
    if (optdmarg >= 0) d = optdmarg;
    if (opthoff  >= 0) h = opthoff;
    if (optvoff  >= 0) v = optvoff;
    if (x <= l + r || y <= u + d)
    {   fprintf(stderr, "postlj: page size smaller than margins\n");
        goto errorexit;
    }
    hoffset = ((h - l) * 720) / 300;
    voffset = ((v - u) * 720) / 300;
    vmarg = u;
    if (optlj == 0)      /* DeskJet, logical page is at top margin */
        vmarg = 0;

    if (optgc == -1)      /* Default graphics compression */
        if (optlj == 1)
            optgc = 0;   /* Off for LaserJet II */
        else
            optgc = 1;   /* On  for LaserJet IIP/III and DeskJet */

    parm.page.depth = 1;
    parm.page.xoff = l;
    parm.page.yoff = d;
    parm.page.xsize = x - l - r;
    parm.page.ysize = y - u - d;
    parm.page.xbytes = (parm.page.xsize + 7) >> 3;
    parm.page.len = parm.page.xbytes * parm.page.ysize;
    parm.page.ybase = 0;
    parm.page.yheight = parm.page.ysize;
    parm.page.xden = parm.page.yden = 300;
    parm.page.ydir = -1;

    /* Allocate the page buffer */

    for (i = 0; i < parm.page.depth; i++)
    {   if ((parm.page.buf[i] =
                AllocMem(parm.page.len, MEMF_PUBLIC|MEMF_CLEAR)) == NULL)
        {   fprintf(stderr, "postlj: can't get page buffer\n");
            goto errorexit;
        }
    }

    /* Allocate the print compression buffer */

    compsize = parm.page.xbytes + parm.page.xbytes / 128 + 2;
    compbuf = AllocMem(compsize, MEMF_PUBLIC);
    if (compbuf == NULL)
    {   fprintf(stderr, "postlj: can't get memory\n");
        goto errorexit;
    }

    /* Open a file to the par: handler and initialise the printer */

    parfp = fopen(argto, "w");
    if (parfp == NULL)
    {   fprintf(stderr, "postlj: can't open %s\n", argto);
        goto errorexit;
    }
    prtsetup();
    if (ferror(parfp))
    {   fprintf(stderr, "postlj: error writing printer file\n");
        goto errorexit;
    }

    /* Initialise for interpretation */

    insertbreak();
    SetExcept(~0, SIGBREAKF_CTRL_C);
    breakset = 1;
    insertftrap();
    ftrapset = 1;

    parm.copyfunc = (APTR) copypage;

    parm.infh = Input();
    parm.outfh = Output();
    parm.errfh = errfh;

    arec = PScreateact(&parm);
    if (arec == 0)
    {   fprintf(stderr, "postlj: can't get memory\n");
        goto errorexit;
    }
    if ((unsigned) arec <= errmax)
    {   arec = 0;
        retcode = 10;
        goto tidyexit;
    }

    /* Interpret the argument files */

    fprintf(stderr, "postlj: running on %s (%s, %s, (%d:%d:%d * %d:%d:%d))\n",
            showlj[optlj], showsize[optsize], showland[optland],
            l, x-l-r, r, u, y-u-d, d);

    pagenum = 0;
    for (i = 0; i < argfilec; i++)
        if (PSintstring(arec, argfilev[i],
                        -1, PSFLAGFILE|PSFLAGCLEAR|PSFLAGERASE) != 0)
        {   retcode = 10;
            goto tidyexit;
        }

    if (ferror(parfp))
    {   fprintf(stderr, "postlj: error writing printer file\n");
        goto errorexit;
    }
    fprintf(stderr, "postlj: finished\n");
    goto tidyexit;

    /* Argument errors and usage query */

query:
    fprintf(stderr, "LaserJet or DeskJet driver for Post. PostLJ version "
                        "1.7\n");
    fprintf(stderr, "Drives the printer directly for control of page size and "
                        "better performance\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  Usage:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    postlj -options [files...] [TO tofile] [MEM fhlv..]"
                        "\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "      -sn     Page size (0 = Letter, 1 = Legal, 2 = "
                        "Executive, 3 = A4,\n");
    fprintf(stderr, "                         4 = COM10, 5 = Monarch, 6 = "
                        "C5, 7 = DL)\n");
    fprintf(stderr, "      -an     Aspect (0 = vertical, 1 = horizontal)\n");
    fprintf(stderr, "      -jn     LaserJet model (0 = DJ, 1 = LJII, 2 = "
                        "LJIIP/III)\n");
    fprintf(stderr, "      -gn     Graphics compression (0 = off, 1 = on)\n");
    fprintf(stderr, "      -bnnnn  Page number to begin printing at\n");
    fprintf(stderr, "      -ennnn  Page number to end printing after\n");
    fprintf(stderr, "      -cnn    Number of copies\n");
    fprintf(stderr, "      -xnnnnn Paper x size\n");
    fprintf(stderr, "      -ynnnnn Paper y size\n");
    fprintf(stderr, "      -lnnnnn Left  margin\n");
    fprintf(stderr, "      -rnnnnn Right margin\n");
    fprintf(stderr, "      -unnnnn Upper margin\n");
    fprintf(stderr, "      -dnnnnn Lower margin\n");
    fprintf(stderr, "      -hnnnnn Horizontal offset registration\n");
    fprintf(stderr, "      -vnnnnn Vertical   offset registration\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  Defaults are A4, portrait, LJIIP/III\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  For example:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "    postlj -a1 -j1 psfonts:init.ps myfile.ps\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  Prints on A4 paper, portrait orientation, using a "
                        "LaserJet II\n");
    goto tidyexit;

badargs:
    fprintf(stderr, "postlj: arguments bad, or value missing\n");
    goto badusage;

badvalue:
    fprintf(stderr, "postlj: argument bad value\n");

badusage:
    retcode = 20;
    fprintf(stderr, "postlj: usage:\n"
    "    postlj -sajgbecxylruhv [files...] [TO tofile] [MEM fhlv..]\n");
    goto tidyexit;

    /* Tidy up and exit */

errorexit:
    retcode = 20;

tidyexit:
    if (breakset)
    {   SetExcept(0, SIGBREAKF_CTRL_C);
        deletebreak();
        breakset = 0;
    }
    if (ftrapset)
    {   deleteftrap();
        ftrapset = 0;
    }

    if (arec) PSdeleteact(arec);

    if (parfp)
    {   prtreset();
        fclose(parfp);
    }

    if (compbuf) FreeMem(compbuf, compsize);

    for (i = 0; i < parm.page.depth; i++)
        if (parm.page.buf[i])
        {   FreeMem(parm.page.buf[i], parm.page.len);
            parm.page.buf[i] = NULL;
        }

    if (PSbase) CloseLibrary(PSbase);
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

/* Signal an interrupt */

void __saveds sigint()
{   PSsignalint(arec, 1);
}

/* Signal a floating point error */

void __saveds sigfpe()
{   PSsignalfpe(arec);
}

/* Copy the page to the output */

void __saveds __asm copypage(register __d0 int num)
{   pagenum++;
    if ((optbeg == 0 || pagenum >= optbeg) &&
        (optend == 0 || pagenum <= optend))
    {   prtdump(optcopies == 0 ? num : optcopies);
        if (ferror(parfp)) PSerror(arec, errioerror);
    }
}

/* Printer setup */

void prtsetup(void)
{
    /* Printer reset, Page size, Orientation, Perf skip off, Top Mgn 0 */

    fprintf(parfp, "\33E\33&l%da%do0l0E", psize[optsize], optland);

    /* Long edge offset, Short edge offset */

    if (optland)
        fprintf(parfp, "\33&l%du%dZ",  voffset, hoffset);
    else
        fprintf(parfp, "\33&l%du%dZ", -hoffset, voffset);
}

/* Printer reset */

void prtreset(void)
{   fprintf(parfp, "\33E");
}

/* Printer dump */

void prtdump(int num)
{   char *buf;
    int ysize;

    /* Set the number of copies */

    if (num == 0 || num > 99) num = 1;
    fprintf(parfp, "\33&l%dX", num);

    /* Set cursor to (0,vmarg), 300 dpi, aligned logical page, start graphics */

    fprintf(parfp, "\33*p0x%dY\33*t300R\33*r0f0A", vmarg);

    /* Loop for the rows */

    buf = parm.page.buf[0];
    ysize = parm.page.ysize;

    while (ysize--)
    {   prtdumpline(buf, parm.page.xbytes);
        buf += parm.page.xbytes;
    }

    /* End graphics, form feed, reset number of copies */

    fprintf(parfp, "\33*rB\14\33&l1X");
}

/* Dump a line of pixels */

void prtdumpline(char *buf, int len)
{   char *ptr;
    int b, c, l;

    /* Strip trailing zeros */

    while (len && buf[len - 1] == 0) len--;

    /* Compression */

    if (optgc)
    {   ptr = compbuf;
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
                l > 0 &&                 /*  and preceeded by literals */
                l <= 125 &&              /*  and not more than 125 of them */
                (len <= 2 ||             /*  and no more than 2 bytes left */
                 *buf != *(buf + 1)))    /*      or not followed by a run */
            {   c = 1;                   /* Then make it a literal */
                buf--;
                len++;
            }
            if (c == 1)                  /* If not a run */
            {   l++;                     /* Then it must be a literal */
                c = 0;
            }
            if (l > 0 &&                 /* If we have some literals */
                (c > 1 ||                /*  and beginning a run */
                 l == 127 ||             /*  or reached 127 */
                 len == 0))              /*  or no more bytes left */
            {   *ptr++ = l - 1;          /* Then write out the literals */
                memcpy(ptr, buf - c - l, l);
                ptr += l;
                l = 0;
            }
            if (c > 1)                   /* If we have a run */
            {   *ptr++ = 1 - c;          /* Then write it */
                *ptr++ = b;
            }
        }
        len = ptr - compbuf;
        fprintf(parfp, "\33*b2m%dW", len);
        buf = compbuf;
    }

    /* No compression */

    else
        fprintf(parfp, "\33*b%dW", len);

    fwrite(buf, 1, len, parfp);
}

/* Dummy stub routine */

void stub(void)
{   return;
}

/* Dummy check abort routine */

void chkabort(void)
{   return;
}

/* End of file "postlj.c" */
