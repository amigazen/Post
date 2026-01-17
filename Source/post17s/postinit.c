/* PostScript interpreter file "postinit.c" - initialiation (Amiga)
 * (C) Adrian Aylward 1989, 1991
 *
 * This file contains the code to initialise the interpreter library and
 * interpret a file or string. It is heavily Amiga dependent, and also
 * Lattice dependent for the construction of file handles and the break
 * and floating point error routines.
 */

# include "post.h"

# include <dos.h>
# include <exec/exec.h>
# include <exec/execbase.h>
# include <proto/dos.h>
# include <proto/exec.h>
# include <fcntl.h>
# include <ios1.h>

# include "postlib.h"

/* Assembler routines */

extern void flushpage(int y1, int y2);
extern int  callextfunc(APTR func, int *argv, int argc);

/* The version number */

char version[] = POSTVER " Copyright Adrian Aylward 1989, 1992";

/* Lattice startup */

extern struct UFB _ufbs[];

/* External data (initialised to zero) */

APTR userdata, flushfunc, copyfunc;

int flushflag, flushlev, flushy1, flushy2;

int funcmax;

APTR *functab;

/* Machine type flags */

# ifdef M68881
short attnflags = AFF_68881|AFF_68020;
# else
short attnflags = 0;
# endif

/* Call external function */

void opcallextfunc(void)
{   struct object *token1, *token2, *aptr;
    int argv[maxargs], argc, ires, func, i;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typeint) error(errtypecheck);
    argc = token1->value.ival;
    if (argc < 0) error(errrangecheck);
    if (argc > maxargs) error(errlimitcheck);
    if (opernest < argc + 3) error(errstackunderflow);
    if (token2->type != typeint) error(errtypecheck);
    func = token2->value.ival;
    if (func < 0 || func >= funcmax) error(errrangecheck);
    aptr = token1;
    i = argc;
    while (i)
    {   i--;
        aptr--;
        switch (aptr->type)
        {   case typeint:
            case typereal:
            case typebool:
                argv[i] = aptr->value.ival;
                break;

            case typearray:
            case typestring:
                argv[i] = (int) vmvptr(aptr->value.vref);
                break;

            default:
                error(errtypecheck);
        }
    }
    aptr--;
    ires = callextfunc(functab[func], argv, argc);
    if      (aptr->type == typenull)
        argc++;
    else if (aptr->type == typeint ||
             aptr->type == typebool ||
             aptr->type == typereal)
        aptr->value.ival = ires;
    else
        error(errtypecheck);
    opernest -= (argc + 2);
}

/* Initialise the interpreter
 *
 * N.B. this routine cannot use floating point, as we don't check for the
 * presence of a fpu until after we have entered it */

int initialise(struct PSparm *parm)
{   memcpy((char *) &page, (char *) &parm->page, sizeof page);

    userdata = parm->userdata;
    flushfunc = parm->flushfunc;
    copyfunc = parm->copyfunc;

    funcmax = parm->funcmax;
    functab = parm->functab;

    /* Convert the DOS filehandles to unbuffered C files */

    _ufbs[0].ufbfh = (long) parm->infh;
    _ufbs[0].ufbflg |= UFB_RA|O_RAW|UFB_NC;
    _ufbs[1].ufbfh = (long) parm->outfh;
    _ufbs[1].ufbflg |= UFB_WA|O_RAW|UFB_NC;
    _ufbs[2].ufbfh = (long) parm->errfh;
    _ufbs[2].ufbflg |= UFB_WA|O_RAW|UFB_NC;
    stdin->_file = 0;
    stdin->_flag = _IOREAD;
    stdout->_file = 1;
    stdout->_flag = _IOWRT;
    stderr->_file = 2;
    stderr->_flag = _IOWRT;
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    /* Copy parameters and allocate memory */

    memflen = parm->memflen;
    memhlen = parm->memhlen;
    memvmin = parm->memvlen;
    memlmin = parm->memllen;
    if (memflen == 0) memflen = defmemflen;
    if (memhlen == 0) memhlen = defmemhlen;
    if (memvmin == 0) memvmin = defmemvlen;
    if (memlmin == 0) memlmin = defmemllen;
    if (memflen < minmemflen) memflen = minmemflen;
    if (memhlen < minmemhlen) memhlen = minmemhlen;
    if (memvmin < minmemvlen) memvmin = minmemvlen;
    if (memlmin < minmemllen) memlmin = minmemllen;
    mempmin = memlmin / 3;
    memlmin -= mempmin;
    memllen = memplen = memilen = memxlen = memylen = 0;
    memlbeg = mempbeg = memibeg = memxbeg = memybeg = NULL;
    if ((memfbeg = memalloc(memflen)) == NULL ||
        (memhbeg = memalloc(memhlen)) == NULL)
        return 0;

    /* Set up a temporary error trap and initialise the interpreter */

    errorflag = 0;
    if (setjmp(istate.errjmp) != 0)
    {   fprintf(sstderr, "post: error: %s, during initialisation\n",
                errorstring);
        return errornum;
    }
    initint(1);
    if (funcmax != 0) systemop(opcallextfunc, "callextfunc");
    return -1;
}

/* Terminate interpreter */

void terminate(void)
{   tidyint();
    memfree(memfbeg, memflen); memfbeg = NULL;
    memfree(memhbeg, memhlen); memhbeg = NULL;
    memfree(memlbeg, memllen); memlbeg = NULL; memllen = 0;
    memfree(mempbeg, memplen); mempbeg = NULL; memplen = 0;
    memfree(memibeg, memilen); memibeg = NULL; memilen = 0;
    memfree(memxbeg, memxlen); memxbeg = NULL; memxlen = 0;
    memfree(memybeg, memylen); memybeg = NULL; memylen = 0;
}

/* Interpret a string or file */

int intstring(char *string, int length, int flags)
{   struct object token, saveobj;
    returncode = 0;
    if (length < 0) length = strlen(string);
    errorflag = 0;
    if (setjmp(istate.errjmp) != 0) goto err;
    prompting = 0;
    if (flags & PSFLAGINTER)
    {   prompting = 1;
        fprintf(sstdout, "%s\n", version);
    }
    if (flags & PSFLAGSAVE)
        vmsave(&saveobj);
    if (flags & PSFLAGSTRING)
    {   vmparm(0, string, length);
        token.type = typestring;
        token.flags = flagexec;
        token.length = length;
        token.value.vref = vmxref(0,0);
        errorflag = 2;
        interpret(&token);
        errorflag = 0;
    }
    if (flags & PSFLAGFILE)
    {   vmparm(0, NULL, 0);
        fileopen(&token, openread | openfont, string, length);
        token.flags |= flagexec;
        errorflag = 2;
        interpret(&token);
        errorflag = 0;
        fileclose(&token);
    }
    if (setjmp(istate.errjmp) != 0) goto err;
    execnest = 0;
    if (flags & PSFLAGCLEAR)
    {   dictnest = 2;
        opernest = 0;
    }
    if (flags & PSFLAGSAVE)
        vmrest(saveobj.length, saveobj.value.ival);
    if (flags & PSFLAGERASE)
        erasepage();
    if (returncode) return errornum;
    return 0;

err:
    fprintf(sstderr, 
            (flags & PSFLAGFILE) ?
                "post: error: %s, file %*.*s\n" :
                "post: error: %s\n",
            errorstring, length, length, string);
    return -1;
}

/* Convert an error number to a text string */

char *errstr(int errnum)
{   return (errnum > 0 && errnum <= errmax) ?
        errortable[errnum] : NULL;
}

/* Dummy stub routine */

void stub(void)
{   return;
}

/* Dummy check abort routine */

void chkabort(void)
{   return;
}

/* A floating point error triggers "undefinedresult". */

void CXFERR(int code)
{   error(errundefinedresult);
}

/* A maths error triggers "undefinedresult". */

int matherr(struct exception *x)
{   error(errundefinedresult);
    return 0;
}

/* Signal a floating point error */

void signalfpe(void)
{   error(errundefinedresult);
}

/* Signal an interrupt */

void signalint(int flag)
{   intsigflag = flag;
}

/* Set the page flush level */

int flushlevel(int lev)
{   if (lev == -1)
        return flushlev++;
    else
    {   flushlev = lev;
        if (lev == 0)
            if (flushflag != 0)
            {   flushflag = 0;
                if (flushy1 != flushy2) flushpage(flushy1, flushy2);
            }
        return lev;
    }
}

/* Flush the page to the screen, according to the level */

void flushlpage(int y1, int y2)
{   if (flushlev == 0)
    {   if (y1 != y2) flushpage(y1, y2);
    }
    else
        if (flushflag == 0)
        {   flushflag = 1;
            flushy1 = y1;
            flushy2 = y2;
        }
        else
        {   if (y1 < flushy1) flushy1 = y1;
            if (y2 > flushy2) flushy2 = y2;
        }
}

/* Allocate memory and clear it */

void *memalloc(int length)
{   return AllocMem(length, MEMF_CLEAR);
}

/* Free memory */

void memfree(void *beg, int length)
{   if (beg) FreeMem(beg, length);
}

/* End of file "postinit.c" */
