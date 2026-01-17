/* Band rendering print driver for Post V1.3.  File "postband.c"
 * (C) Adrian Aylward 1989, 1990
 *
 * You may freely copy, use, and modify this file.
 *
 * This file is a PostScript program driver that splits a PostScript file
 * that conforms to the structuring conventions and calls Post to print
 * it in a series of bands, even if there is not enough memory for a full
 * page buffer.  It is totally Amiga specific.
 *
 * The program was tested using Lattice C V5.05.  It has various Lattice
 * dependencies.
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
# include <stdio.h>
# include <string.h>

# include "postlib.h"

/* Assembler routines */

extern void insertbreak(void);
extern void deletebreak(void);
extern void insertftrap(void);
extern void deleteftrap(void);

/* Routines defined and referenced only within this module */

extern void prints(BPTR fh, char *string);
extern void errmsg(char *string);
extern void setprinter(void);
extern void setprintden(void);
extern int  strtoint(char **sp, int *ip);
extern void __saveds __asm copypage(register __d0 int num);
extern void printpage(void);
extern void splitpage(void);

/* External data (initialised to zero) */

int retcode, errcode, ioerror;

int arec;

BPTR errfh;

int ybase;
int prologue, trailer;
FILE *printfile;

struct library *PSbase;
struct PSparm parm;

struct BitMap bitmap;
struct ColorMap colormap;

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

/* Arguments */

int argfilec, argsizec, argmemc;
char *argprint;
char *argfilev[5], *argsizev[5], *argmemv[5];
char *argkey[] =
{   "PRINT", "SIZE", "MEM", NULL };

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

    /* Call the main program  */

    main(argc, argv);

    /* Tidy up and exit */

tidyexit:
    if (errfh) Close(errfh);

    XCEXIT(retcode);
}

/* Main program */

void main(int argc, char **argv)
{   char *s;
    int *ip, i, ch;
    int xsize, ysize, ssize, xoff, yoff, xden, yden, pden;

    /* Open the libraries */

    PSbase = OpenLibrary("post.library", 0);
    if (PSbase == NULL)
    {   errmsg("can't open post.library");
        goto errorexit;
    }

    /* Parse the arguments and keywords.  See the usage string below */

    argc--;
    argv++;
    if (argc == 0 || (argc == 1 && strcmp(*argv, "?") == 0)) goto usage;

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
        {   case  0:    /* PRINT */
                if (argc == 0) goto badargs;
                argc--;
                argprint = *argv++;
                break;

            case  1:    /* SIZE */
                if (argc == 0) goto badargs;
                argc--;
                if (argsizec == 5) goto badargs;
                argsizev[argsizec++] = *argv++;
                break;

            case  2:    /* MEM */
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
    if (argprint == NULL) goto badargs;

    /* Get up the default page size from the printer preferences */

    bitmap.BytesPerRow = 1;
    bitmap.Rows = 1;
    bitmap.Flags = 0;
    bitmap.Depth = parm.page.depth = 3;
    setprinter();
    parm.page.xoff = 0;
    parm.page.yoff = 0;
    if (prport == NULL)
    {   errmsg("can't open printer device");
        goto errorexit;
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
        setprintden();
    }

    /* Set up the default memory sizes */

    parm.memvlen = 280000;
    parm.memflen =  60000;
    parm.memllen =  60000;
    parm.memhlen =  20000;

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

    /* Set up the color map according to the number of bitplanes */

    colormap.Count = 1 << parm.page.depth;
    if (parm.page.depth == 1)
        colormap.ColorTable = (APTR) bcolors;
    else
        colormap.ColorTable = (APTR) ccolors;

    parm.page.ybase = 0;
    parm.page.yheight = parm.page.ysize;
    if (ssize != 0 && ssize < parm.page.ysize) parm.page.ysize = ssize;
    if (parm.page.xsize == 0 || parm.page.ysize == 0)
    {   errmsg("page size not set in preferences");
        goto errorexit;
    }
    parm.page.xbytes = (parm.page.xsize + 15) >> 3 & 0xfffffffe;
    parm.page.len = parm.page.xbytes * parm.page.ysize;

    /* Allocate the page buffer */

    for (i = 0; i < parm.page.depth; i++)
    {   if ((parm.page.buf[i] =
                AllocMem(parm.page.len, MEMF_PUBLIC|MEMF_CLEAR)) == NULL)
        {   errmsg("can't get page buffer");
            goto errorexit;
        }
    }

    /* Initialise the bitmap */

    bitmap.BytesPerRow = parm.page.xbytes;
    bitmap.Rows = parm.page.ysize;
    bitmap.Flags = 0;
    bitmap.Depth = parm.page.depth;
    memcpy((char *) bitmap.Planes, (char *) parm.page.buf,
           sizeof bitmap.Planes);

    /* Open the print file and check the header */

    printfile = fopen(argprint, "r");
    if (printfile == NULL)
    {   errmsg("can't open print file\n");
        goto errorexit;
    }
    s = "%!PS-Adobe-";
    while (*s)
    {   ch = fgetc(printfile);
        if (ch != *s++) goto pferror;
    }
    for (;;)
    {   ch = fgetc(printfile);
        if (ch == '\n') break;
        if (ch == EOF) goto pferror;
    }
    prologue = 1;
    trailer = 0;

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
    {   errmsg("can't get memory");
        goto errorexit;
    }
    if ((unsigned) arec <= errmax)
    {   arec = 0;
        retcode = 10;
        goto tidyexit;
    }

    /* Interpret the argument files */

    for (i = 0; i < argfilec; i++)
        if (PSintstring(arec, argfilev[i],
                        -1, PSFLAGFILE|PSFLAGCLEAR|PSFLAGERASE) != 0)
        {   retcode = 10;
            goto tidyexit;
        }

    /* Interpret the prologue and each page */

    for (;;)
    {   if (trailer) break;
        splitpage();
        if (retcode != 0) break;
        ybase = 0;
        if (PSintstring(arec,
                        prologue ?
            "(t:postband.ps) run" :
            "currentband 1 sub{setband (t:postband.ps) run} for 0 setband",
                        -1, PSFLAGSTRING) != 0)
        {   retcode = 10;
            break;
        }
        prologue = 0;
    }

    goto tidyexit;

    /* File format error */

pferror:
    errmsg("print file not PostScript conforming");
    goto errorexit;

    /* Argument errors and usage query */

badargs:
    errmsg("arguments bad, or value missing");
    goto badusage;

badvalue:
    errmsg("argument bad value");

badusage:
    retcode = 20;

usage:
    errmsg("usage:\n"
    "    postband [files...] PRINT file [SIZE xyod..s..p.bc.] [MEM fhlv..]");
    goto tidyexit;

    /* Tidy up and exit */

errorexit:
    retcode = 20;

tidyexit:
    DeleteFile("t:postband.ps");
    if (printfile) fclose(printfile);

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

    for (i = 0; i < parm.page.depth; i++)
        if (parm.page.buf[i])
        {   FreeMem(parm.page.buf[i], parm.page.len);
            parm.page.buf[i] = NULL;
        }

    if (propen) CloseDevice((struct IORequest *) &prreq);
    if (prport) DeletePort(prport);

    if (PSbase)  CloseLibrary(PSbase);
}

/* Print a string to a DOS file handle */

void prints(BPTR fh, char *string)
{   Write(fh, string, strlen(string));
}

/* Display an error message */

void errmsg(char *string)
{   prints(errfh, "postband: ");
    prints(errfh, string);
    prints(errfh, "\n");
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
{   int pxsize, pysize;

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
    if (pxsize != 0) parm.page.xsize = pxsize;
    if (pysize != 0) parm.page.ysize = pysize;
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
{   ioerror = 0;
    printpage();
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
        errmsg("printer not ready (CTRL/C to abort)");

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
    prreq.io_DestCols = parm.page.xsize;
    prreq.io_DestRows = parm.page.ysize;
    prreq.io_Special = (SPECIAL_DENSITY1 * prden) | SPECIAL_TRUSTME;
    if (ybase + parm.page.ysize >= parm.page.yheight)
        prreq.io_SrcHeight = prreq.io_DestRows =
                parm.page.yheight - ybase;
    else
        prreq.io_Special |= SPECIAL_NOFORMFEED;
    ybase += prreq.io_SrcHeight;
    prflags = prprefs->PrintFlags;
    prprefs->PrintFlags = prflags & ~DIMENSIONS_MASK | IGNORE_DIMENSIONS;
    prprefs->PrintAspect = ASPECT_HORIZ;

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

/* Split the prologue or next page of the print file into the temp file */

void splitpage(void)
{   FILE *tempfile;
    char *s;
    int matched, ch;

    tempfile = fopen("t:postband.ps", "w");
    if (tempfile == NULL)
    {   errmsg("can't open temporary file");
        retcode = 20;
        return;
    }

    matched = 0;
    for (;;)
    {   ch = fgetc(printfile);

        /* Comment lines are not copied */

        if (ch == '%')
        {   ch = fgetc(printfile);

            /* %% Comments may be a trailer */

            if (ch == '%')
            {   ch = fgetc(printfile);
                if      (ch == 'P')
                {   s = "age:";
                    while (*s)
                    {   ch = fgetc(printfile);
                        if (ch != *s++) goto nomatch;
                    }
                    matched = 1;
                }
                else if (ch == 'T')
                {   s = "railer";
                    while (*s)
                    {   ch = fgetc(printfile);
                        if (ch != *s++) goto nomatch;
                    }
                    if (prologue) goto pferror;
                    matched = 1;
                    trailer = 1;
                }
            }

            /* Skip the rest of the comment */

nomatch:    if (ch == EOF) goto pferror;
            for (;;)
            {   ch = fgetc(printfile);
                if (ch == '\n') break;
                if (ch == EOF) goto pferror;
            }
        }

        /* All other lines are copied to the temp file */

        else
        {   for (;;)
            {   if (ch == EOF) goto pferror;
                if (fputc(ch, tempfile) == EOF) goto tferror;
                if (ch == '\n') break;
                ch = fgetc(printfile);
            }
        }
        if (matched) break;
    }
    if (fclose(tempfile) == EOF) goto tferror;
    return;

pferror:
    errmsg("print file not conforming or io error");
    goto errorexit;

tferror:
    errmsg("error with temporary file");

errorexit:
    fclose(tempfile);
    retcode = 20;
}

/* End of file "postband.c" */
