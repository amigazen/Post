/* PostScript interpreter file "postint.c" - the basic interpreter
 * (C) Adrian Aylward 1989, 1992
 * V1.6 First source release
 * V1.7 Fix dictput to convert string keys to names
 * V1.7 Fix fileeinit to handle white space after eexec
 */

# include "post.h"

/* Initialise the interpreter */

void initint(int parms)
{   struct object token, *aptr;
    int i;

    /* Initialise the virtual machine */

    vminit(parms);

    /* Initialise the basic interpreter */

    inest = 0;
    istate.flags = 0;
    istate.type = -1;
    istate.vmbase = 0;
    istate.gbase = 0;
    istate.execbase = 0;
    istate.pfcrec = NULL;
    istack = vmallocv(sizeof (struct istate) * istacksize);
    strncpy(prompt1, "> ", promptsize);
    strncpy(prompt2, "- ", promptsize);
    time(&time1);
    random = 1;
    nametable = vmallocv(sizeof (vmref) * nametablesize);
    opernest = 0;
    execnest = 0;
    dictnest = 0;
    operstack = vmallocv(sizeof (struct object) * operstacksize);
    execstack = vmallocv(sizeof (struct object) * execstacksize);
    dictstack = vmallocv(sizeof (struct object) * dictstacksize);
    filetable = vmallocv(sizeof (struct file) * filetablesize);
    filetable[0].ch = '\n';
    filetable[0].open = openread;
    filetable[0].fptr = sstdin;
    filetable[1].ch = EOF;
    filetable[1].open = openwrite;
    filetable[1].fptr = sstdout;
    filetable[2].ch = EOF;
    filetable[2].open = openwrite;
    filetable[2].fptr = sstderr;
    optable = vmallocv(sizeof (struct operator) * (optablesize + 1));
    opnum = 0;

    /* Initialise the dictionaries */

    dicttoken(&dictstack[0], systemdictsize);
    systemname(&dictstack[0], "systemdict", 0);
    dicttoken(&dictstack[1], userdictsize);
    systemname(&dictstack[1], "userdict", 0);
    dictnest = 2;
    token.type = typestring;
    token.flags = flagwprot;
    token.length = strlen(version);
    token.value.vref = vmalloc(token.length);
    memcpy(vmsptr(token.value.vref), version, token.length);
    systemname(&token, "version", 0);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    systemname(&token, "false", 0);
    token.value.ival = 1;
    systemname(&token, "true", 0);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 1;
    nametoken(&copies, "#copies", -1, 0);
    dictput(dictstack[1].value.vref, &copies, &token);

    /* Initialise the operators */

    initop1();
    initop2();
    initop3();
    initop4();

    /* Initialise "errordict" */

    dicttoken(&errordict, errordictsize);
    systemname(&errordict, "errordict", 0);
    token.type = typeoper;
    token.flags = flagexec;
    token.length = 0;
    token.value.ival = 1;
    for (i = 0; i <= errmax; i++)
    {   nametoken(&errorname[i], errortable[i], -1, flagexec);
        dictput(errordict.value.vref, &errorname[i], &token);
        token.value.ival = 0;
    }

    /* The value of "handleerror" in "systemdict" is
     *              "errordict /handleerror get exec" */

    token.type = typearray;
    token.flags = flagexec;
    token.length = 4;
    token.value.vref = arrayalloc(4);
    aptr = vmaptr(token.value.vref);
    aptr[0] = errordict;
    aptr[1] = errorname[0];
    aptr[1].flags = 0;
    nametoken(&aptr[2], "get", -1, flagexec);
    nametoken(&aptr[3], "exec", -1, flagexec);
    bind(&token, 0);
    dictput(dictstack[0].value.vref, &errorname[0], &token);

    /* Initialise "$error" */

    dicttoken(&errdsdict, errdsdictsize);
    nametoken(&token, "$error", -1, 0);
    dictput(dictstack[0].value.vref, &token, &errdsdict);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    for (i = 0; i < edsmax; i++)
    {   nametoken(&errdsname[i], errdstable[i], -1, flagexec);
        errdstoken[i] = token;
        dictput(errdsdict.value.vref, &errdsname[i], &token);
        token.type = typenull;
    }

    /* Initialise the graphics state, character routines */

    initgstate();
    initchar();
}

/* Tidy up the interpreter */

void tidyint()
{   struct file *file;
    int filenum;

    /* Close all opened files */

    if (filetable)
    {   for (filenum = 3; filenum < filetablesize; filenum++)
        {   file = &filetable[filenum];
            if (file->open != 0) fclose(file->fptr);
        }
        filetable = (struct file *) 0;
    }

    /* Tidy up the virtual machine */

    vmtidy();
}

/* Make a name and insert it into the system dictionary */

void systemname(struct object *token, char *sptr, int flags)
{   struct object nameobj;
    nametoken(&nameobj, sptr, -1, flags);
    dictput(dictstack[0].value.vref, &nameobj, token);
}

/* Make an operator and insert it into the system dictionary */

void systemop(void (*func)(), char *sptr)
{   struct object token;
    if (opnum  == optablesize) error(errlimitcheck);
    optable[opnum].func = func;
    optable[opnum].sptr = sptr;
    token.type = typeoper;
    token.flags = flagexec;
    token.length = 0;
    token.value.ival = opnum;
    systemname(&token, sptr, flagexec);
    opnum++;
}

/* The interpreter */

void interpret(struct object *interpreting)
{   struct object token, *executing, *savetoken;

    /* Start with a null token, in case we get an error before we have read
     * one. */

    token.type = 0;
    token.flags = 0;
    token.value.ival = 0;

    /* Push the object we want to execute onto the execution stack.  Save the
     * error jump buffer on the error stack.  Set up the current token. */

    if (execnest >= execstacksize) error(errexecstackoverflow);
    execstack[execnest++] = *interpreting;
    savetoken = currtoken;

    while (setjmp(istate.errjmp) != 0) continue;

    currtoken = &token;

    /* Loop until the execution stack is empty.  (I.e. the same level as it
     * was when we entered.  Check for interrupt. */

    while (execnest != istate.execbase)
    {   if (intsigflag != 0)
        {   if (intsigflag == 1)
            {   intsigflag = 0;
                error(errinterrupt);
            }
            else
            {   intsigflag = 0;
                error(errkill);
            }
        }
        executing = &execstack[execnest - 1];

        /* If the top of the stack is executable extract the next token from
         * it. */

        if (executing->flags & flagexec)
        {   if (executing->flags & flagxprot) error(errinvalidaccess);
            if (executing->type == typearray)
            {   if (executing->length == 0)
                {   execnest--;
                    continue;
                }
                token = *vmaptr(executing->value.vref);
                executing->value.vref += sizeof (struct object);
                if (--executing->length == 0)
                    execnest--;
                goto dir;
            }
            if (executing->type == typepacked)
            {   if (executing->length == 0)
                {   execnest--;
                    continue;
                }
                executing->value.vref +=
                    unpack(&token, vmsptr(executing->value.vref));
                if (--executing->length == 0)
                    execnest--;
                goto dir;
            }
            if (executing->type == typefile)
            {   if (!scantoken(&token, executing, 0))
                {   if (filetable[executing->length].emode != 0)
                    {   filetable[executing->length].emode = 0;
                        if (dictnest < 3) error(errdictstackunderflow);
                        dictnest--;
                    }
                    else
                        fileclose(executing);
                    execnest--;
                    continue;
                }
                goto dir;
            }
            if (executing->type == typestring)
            {   if (!scantoken(&token, executing, 0))
                {   execnest--;
                    continue;
                }
                goto dir;
            }
        }

        /* Otherwise if it is a control operator execute it without popping
         * the stack;  for all other cases we pop it off the stack and
         * execute it. */

        token = *executing;
        if (token.flags & flagctrl)
        {   (*(optable[token.value.ival].func))();
            continue;
        }
        execnest--;

        /* Execute an object obtained indirectly.  (Procedures are executed
         * immediately.) */

ind:    if (token.flags & flagexec)
        {   if      (token.type == typeoper)
            {   (*(optable[token.value.ival].func))();
                continue;
            }
            else if (token.type == typename)
            {   if (dictfind(&token, &token) == -1) error(errundefined);
                goto ind;
            }
            else
            {   if (token.type == typenull) continue;
                if (execnest == execstacksize) error(errexecstackoverflow);
                execstack[execnest++] = token;
                continue;
            }
        }
        if (opernest == operstacksize) error(errstackoverflow);
        operstack[opernest++] = token;
        continue;

        /* Execute an object obtained directly.  (Procedures are pushed onto
         * the operand stack.) */

dir:    if (token.flags & flagexec)
        {   if      (token.type == typeoper)
            {   (*(optable[token.value.ival].func))();
                continue;
            }
            else if (token.type == typename)
            {   if (dictfind(&token, &token) == -1) error(errundefined);
                goto ind;
            }
            else if (token.type != typearray && token.type != typepacked)
            {   if (token.type == typenull) continue;
                if (execnest == execstacksize) error(errexecstackoverflow);
                execstack[execnest++] = token;
                continue;
            }
        }
        if (opernest == operstacksize) error(errstackoverflow);
        operstack[opernest++] = token;
    }

    /* Restore the current token and exit to the next outer level. */

    currtoken = savetoken;
}

/* Push the interpreter stack before recursing */

void pushint(void)
{   if (inest == istacksize) error(errlimitcheck);
    istack[inest++] = istate;
    istate.flags = 0;
    istate.type = -1;
    istate.vmbase = vmnest;
    istate.gbase = gnest;
    istate.execbase = execnest;
    istate.pfcrec = NULL;
}

/* Pop the interpreter (and vm and graphics) stacks after recursing */

void popint(void)
{   if (vmnest != istate.vmbase)
        vmrest(istate.vmbase, -1);
    if (gnest != istate.gbase)
    {   gnest = istate.gbase + 1;
        grest();
    }
    if (istate.pfcrec)
        istate.pfcrec->count = 0;
    istate = istack[--inest];
}

/* Put a character on an output stream */

void putch(FILE *fptr, int ch)
{   if (putc(ch, fptr) == EOF) error(errioerror);
}

/* Put a string on an output stream */

void putstr(FILE *fptr, char *str)
{   int ch;
    while (ch = *str++)
        if (putc(ch, fptr) == EOF) error(errioerror);
}

/* Put a memory buffer on an output stream */

void putmem(FILE *fptr, char *sptr, int length)
{   while (length--)
        if (putc(*sptr++, fptr) == EOF) error(errioerror);
}

/* Put a memory buffer on an output stream, checking for funny characters */

void putcheck(FILE *fptr, char *sptr, int length)
{   int ch;
    while (length--)
    {   ch = *(unsigned char *)sptr++;
        if (ch < 0x20 || ch >= 0x7f)
        {   if      (ch == '\n')
                putstr(fptr, "\\n");
            else if (ch == '\r')
                putstr(fptr, "\\r");
            else if (ch == '\t')
                putstr(fptr, "\\t");
            else if (ch == '\b')
                putstr(fptr, "\\b");
            else if (ch == '\f')
                putstr(fptr, "\\f");
            else
                if (fprintf(fptr, "\\%03.3o", ch) == EOF) error(errioerror);
        }
        else
            if (putc(ch, fptr) == EOF) error(errioerror);
    }
}

/* Open a file */

void fileopen(struct object *token, int open, char *name, int length)
{   struct file *file;
    int filenum;
    if (length < 0) length = strlen(name);
    if (length > namebufsize) error(errlimitcheck);
    memcpy(namebuf, name, length);
    namebuf[length] = 0;
    if (strlen(namebuf) != length) error(errrangecheck);
    if      (strcmp(namebuf, "%stdin") == 0)
        filenum = 0;
    else if (strcmp(namebuf, "%stdout") == 0)
        filenum = 1;
    else if (strcmp(namebuf, "%stderr") == 0)
        filenum = 2;
    else
    {   filenum = 3;
        for (;;)
        {   file = &filetable[filenum];
            if (file->open == 0) break;
            ++filenum;
            if (filenum == filetablesize) error(errlimitcheck);
        }
    }
    if (filenum < 3)
    {   file = &filetable[filenum];
        if (file->fptr == NULL) error(errundefinedfilename);
        if (file->open != (open & ~openfont)) error(errinvalidfileaccess);
    }
    else
    {   file->fptr = fopen(namebuf, ((open == openwrite) ? "w" : "r"));
        if (file->fptr == NULL) error(errundefinedfilename);
        file->open = open & ~openfont;
        file->saved = vmnest;
        file->ch = EOF;
        file->uflg = 0;
        file->slen = 0;
        file->emode = 0;
        file->erand = 0;
        file->stype = 0;
        if (open & openfont) file->stype = -1;
    }
    token->type = typefile;
    token->flags = 0;
    token->length = filenum;
    token->value.ival = file->generation;
}

/* Close a file */

void fileclose(struct object *token)
{   struct file *file;
    FILE *fptr;
    int open;
    if (token->length > 2)
        if (file = filecheck(token, (openread | openwrite)))
        {   fptr = file->fptr;
            open = file->open;
            file->fptr = NULL;
            file->generation++;
            file->open = 0;
            if (fclose(fptr) == EOF)
                if (open & openwrite) error(errioerror);
        }
}

/* Check a file is open */

struct file *filecheck(struct object *token, int open)
{   struct file *file;
    file = &filetable[token->length];
    if (file->generation == token->value.ival && (file->open & open) != 0)
        return file;
    else
        return NULL;
}

/* Get the next character from a file, allowing IBM font format */

# define getf(file,fptr) (--file->slen >= 0 ? getc(fptr) : getfseg(file))

/* Unget the last character from a file, allowing IBM font format */

# define ungetf(ch,file,fptr) (file->slen++, ungetc(ch, fptr))

/* Locate the next segment of an IBM font format file */

int getfseg(struct file *file)
{   FILE *fptr;
    int ch, i;
    fptr = file->fptr;
    file->slen = 0;

    /* Unknown file type; assume IBM font if first character is 0x80 */

    if (file->stype == -1)
    {   ch = getc(fptr);
        if (ch == EOF) error(errioerror);
        ungetc(ch, fptr);
        if (ch != 0x80) file->stype = 0;
    }

    /* Standard file; may come here after reading 2 Gigabytes? */

    if (file->stype == 0)
    {   file->slen = 0x7fffffff;
        return getc(fptr);
    }

    /* IBM font format.  Read 6 byte segment header */

gets:
    ch = getc(fptr);
    if (ch != 0x80)
    {  if (ch != EOF || ferror(fptr)) error(errioerror);
       return EOF;
    }
    ch = getc(fptr);
    file->stype = ch;
    if      (ch == 1 || ch == 2)
    {   i = 4;
        while (i--)
        {   ch = getc(fptr);
            if (ch == EOF) error(errioerror);
            file->slen = (((unsigned) file->slen) >> 8) | (ch << 24);
        }
    }
    else if (ch != 3)
        error(errioerror);
    if (--file->slen <= 0) goto gets;
    return getc(fptr);
}

/* Initialise a file for decryption */

void fileeinit(struct object *token)
{   struct file *file;
    FILE *fptr;
    int digit[4], i, j, k, ch, dig, num;

    file = filecheck(token, openread | openwrite);
    if (file == NULL) error(errioerror);
    fptr = file->fptr;

    /* Skip white space characters, except for IBM binary sections */

    ch = getf(file, fptr);
    if (file->stype != 2)
        while (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = getf(file, fptr);
    ungetf(ch, file, fptr);

    /* Decrypt four binary bytes, checking whether they are all hex digits */

    file->erand = einitexec;
    file->emode = 2;
    for (i = 0; i < 4; i++)
    {   ch = getf(file, fptr);
        if (ch == EOF)
        {   if (ferror(fptr)) error(errioerror);
            error(errsyntaxerror);
        }
        if ((digit[i] = digitval(ch)) >= 16) file->emode = 1;
        file->erand = ((file->erand + ch) * ec1 + ec2) & emask;
    }

    /* If all hex, must be hex encryption.  So decrypt four hex bytes */

    if (file->emode == 2)
    {   file->erand = einitexec;
        j = 0;
        for (k = 0; k < 4; k++)
        {   if (j < i)
            {   num = digit[j];
                j++;
            }
            else
            {   ch = getf(file, fptr);
                if (ch == EOF && ferror(fptr)) error(errioerror);
                if ((num = digitval(ch)) >= 16) error(errsyntaxerror);
            }
            if (j < i)
            {   dig = digit[j];
                j++;
            }
            else
            {   ch = getf(file, fptr);
                if (ch == EOF && ferror(fptr)) error(errioerror);
                if ((dig = digitval(ch)) >= 16) error(errsyntaxerror);
            }
            ch = (num << 4) + dig;
            file->erand = ((file->erand + ch) * ec1 + ec2) & emask;
        }
    }
}

/* Read a character from a file or string */

int readch(struct object *input, int depth)
{   struct file *file;
    FILE *fptr;
    int ch, cx, num, dig;
    if (input->type == typefile)
    {   file = &filetable[input->length];
        if (file->uflg)
        {   file->uflg = 0;
            return file->ch;
        }
        fptr = file->fptr;

        /* Not encrypted */

        if (file->emode == 0)
        {   if (file->ch == '\n' && fptr == sstdin && prompting)
            {   if (intsigflag)
                {   intsigflag = 0;
                    currtoken->type = 0;
                    currtoken->flags = 0;
                    currtoken->value.ival = 0;
                    error(errinterrupt);
                }
                if (depth >= 0)
                    fputs(((depth == 0) ? prompt1: prompt2), sstdout);
            }
            ch = getf(file, fptr);
            if (ch == '\r' && depth != -2)
            {   ch = getf(file, fptr);
                if (ch != '\n')
                {   ungetf(ch, file, fptr);
                    ch = '\n';
                }
            }
            if (ch == EOF && ferror(fptr)) error(errioerror);
        }

        /* Encrypted */

        else
        {
            /* Binary */

            if      (file->emode == 1)
            {   cx = getf(file, fptr);
                if (cx == EOF)
                {   if (ferror(fptr)) error(errioerror);
                    error(errsyntaxerror);
                }
            }

            /* Hex */

            else if (file->emode == 2)
            {   do  ch = getf(file, fptr);
                    while (ch == ' ' || ch == '\t' ||
                           ch == '\r' || ch == '\n');
                if ((num = digitval(ch)) >= 16)
                {   if (ch == EOF && ferror(fptr)) error(errioerror);
                    error(errsyntaxerror);
                }
                do  ch = getf(file, fptr);
                    while (ch == ' ' || ch == '\t' ||
                           ch == '\r' || ch == '\n');
                if ((dig = digitval(ch)) >= 16)
                {   if (ch == EOF && ferror(fptr)) error(errioerror);
                    error(errsyntaxerror);
                }
                cx = (num << 4) + dig;
            }

            /* End of encryption */

            else
            {   file->ch = EOF;
                return EOF;
            }

            /* Decrypt */

            ch = cx ^ (file->erand >> eshift);
            file->erand = ((file->erand + cx) * ec1 + ec2) & emask;
            if (ch == '\r' && depth != -2)
                ch = '\n';
        }

        file->ch = ch;
        return ch;
    }
    else
    {   if (input->length == 0)
            return EOF;
        else
        {   input->length--;
            ch = *((unsigned char *) (vmsptr(input->value.vref)));
            input->value.vref++;
            return ch;
        }
    }
}

/* Unread a character from a file or string */

void unreadch(struct object *input, int ch)
{   if (input->type == typefile)
        filetable[input->length].uflg = 1;
    else
        if (ch != EOF)
        {   input->length++;
            input->value.vref--;
        }
}

/* If a character is a digit return its value */

int digitval(int ch)
{   if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
    return 99;
}

/* Scan a file or string for an object token */

int scantoken(struct object *token, struct object *input, int depth)
{   int ch, num, dig, length, nest, flags, load;

    if (input->type == typefile)
        if (filecheck(input, openread) == NULL) error(errioerror);
    if (input->flags & flagxprot) error(errinvalidaccess);

lab:
    ch = readch(input, depth);
    switch (ch)
    {   case EOF:
            return 0;

        case ' ': case '\t': case '\n':
            goto lab;

        case '%':
            for (;;)
            {   ch = readch(input, depth);
                if (ch == EOF) return 0;
                if (ch == '\n') goto lab;
            }

        case ')': case '>':
            error(errsyntaxerror);

        case '(':
            length = 0;
            nest = 1;
            for (;;)
            {   ch = readch(input, -1);
                if (ch == EOF) error(errsyntaxerror);
                if (ch == '(') nest++;
                if (ch == ')' && --nest == 0) break;
                if (ch == '\\')
                {   ch = readch(input, -1);
                    if (ch == EOF) error(errsyntaxerror);
                    if (ch == '\n') continue;
                    if (ch == 'n') ch = '\n';
                    if (ch == 'r') ch = '\r';
                    if (ch == 't') ch = '\t';
                    if (ch == 'b') ch = '\b';
                    if (ch == 'f') ch = '\f';
                    num = digitval(ch);
                    if (num < 8)
                    {   ch = readch(input, -1);
                        dig = digitval(ch);
                        if (dig < 8)
                        {   num = num * 8 + dig;
                            ch = readch(input, -1);
                            dig = digitval(ch);
                            if (dig < 8)
                                num = num * 8 + dig;
                            else
                                unreadch(input, ch);
                        }
                        else
                            unreadch(input, ch);
                        ch = num;
                    }
                }
                *vmstring(length++, 1) = ch;
            }
str:        if (length > 65535) error(errlimitcheck);
            token->type = typestring;
            token->flags = 0;
            token->length = length;
            token->value.vref = vmalloc(length);
            return 1;

        case '<':
            length = 0;
            for (;;)
            {   do
                    ch = readch(input, -1);
                    while (ch == ' ' || ch == '\t' || ch == '\n');
                if ((num = digitval(ch)) >= 16)
                {   if (ch == '>')
                        break;
                    else
                        error(errsyntaxerror);
                }
                do
                    ch = readch(input, -1);
                    while (ch == ' ' || ch == '\t' || ch == '\n');
                if ((dig = digitval(ch)) >= 16)
                {   if (ch == '>')
                        dig = 0;
                    else
                        error(errsyntaxerror);
                }
                *vmstring(length++, 1) = (num << 4) + dig;
                if (ch == '>') break;
            }
            goto str;

        case '{':
            if (depth >= maxdepth) error(errlimitcheck);
            nest = opernest;
            for (;;)
            {   ch = readch(input, depth);
                if (ch == EOF) error(errsyntaxerror);
                if (ch == ' ' || ch == '\t' || ch == '\n') continue;
                if (ch == '%')
                {   for (;;)
                    {   ch = readch(input, depth);
                        if (ch == EOF) error(errsyntaxerror);
                        if (ch == '\n') break;
                    }
                    continue;
                }
                if (ch == '}') break;
                unreadch(input, ch);
                if (opernest == operstacksize) error(errstackoverflow);
                if (!scantoken(&operstack[opernest++], input, depth + 1))
                    error(errsyntaxerror);
            }
            token->length = length = opernest - nest;
            if (packing)
            {   token->type = typepacked;
                token->flags = flagexec | flagwprot;
                token->value.vref = arraypack(&operstack[nest], length);
            }
            else
            {   token->type = typearray;
                token->flags = flagexec;
                token->value.vref = arrayalloc(length);
                arraycopy(vmaptr(token->value.vref),
                          &operstack[nest], length);
            }
            opernest = nest;
            return 1;

        case '}':
            error(errsyntaxerror);

        case '[':
        case ']':
            namebuf[0] = ch;
            nametoken(token, namebuf, 1, flagexec);
            return 1;

        default:
            flags = flagexec;
            load = 0;
            if (ch == '/')
            {   flags = 0;
                ch = readch(input, depth);
                if (ch == '/')
                {   load = 1;
                    ch = readch(input, depth);
                }
            }
            length = 0;
            for (;;)
            {   switch (ch)
                {   case EOF:
                    case '%':
                    case '(': case ')': case '<': case '>':
                    case '[': case ']': case '{': case '}':
                    case '/':
                        unreadch(input, ch);
                    case ' ': case '\t': case '\n':
                        break;

                    default:
                        if (length == namebufsize) error(errlimitcheck);
                        namebuf[length++] = ch;
                        ch = readch(input, depth);
                        continue;
                }
                break;
            }
            namebuf[length] = ' ';
            if (flags == flagexec)
                if (numtoken(token, namebuf)) return 1;
            nametoken(token, namebuf, length, flags);
            if (load)
                if (dictfind(token, token) == -1) error(errundefined);
            return 1;
    }
}

/* Make a number token if we can */

int numtoken(struct object *token, char *string)
{   char *sptr = string;
    int ch, dig, num;
    unsigned int base, limit;
    int sign = 0, ovf = 0, digits = 0;
    limit = 0x7fffffff;
    ch = *sptr++;
    if      (ch == '+')
    {   sign = 1;
        ch = *sptr++;
    }
    else if (ch == '-')
    {   sign = 2;
        ch = *sptr++;
        limit = 0x80000000;
    }
    if (ch == '.') goto decn;
    num = digitval(ch);
    if (num >= 10) return 0;
    digits = 1;
    for (;;)
    {   ch = *sptr++;
        dig = digitval(ch);
        if (dig >= 10) break;
        if (num > limit/10 - 1)
            if ((dig > limit%10) || (num > limit/10)) ovf = 1;
        num = num * 10 + dig;
    }

    if (ch != '#') goto decn;
    ch = *sptr++;

    if (sign != 0 || num == 0 || num > 36) return 0;
    limit = 0xffffffff;
    base = num;
    num = digitval(ch);
    if (num >= base) return 0;
    for (;;)
    {   ch = *sptr++;
        dig = digitval(ch);
        if (dig >= base) break;
        if (num > limit/base - 1)
            if ((dig > limit%base) || (num > limit/base)) ovf = 1;
        num = num * base + dig;
    }
    if (ch != ' ') return 0;
    if (ovf == 0) goto numi;
    error(errlimitcheck);

decn:
    if (ch == '.')
    {   ovf = 1;
        for (;;)
        {   ch = *sptr++;
            dig = digitval(ch);
            if (dig >= 10) break;
            digits = 1;
        }
    }
    if (digits == 0) return 0;
    if (ch == 'E' || ch == 'e')
    {   ovf = 1;
        digits = 0;
        ch = *sptr++;
        if (ch == '+' || ch == '-') ch = *sptr++;
        for (;;)
        {   dig = digitval(ch);
            if (dig >= 10) break;
            digits = 1;
            ch = *sptr++;
        }
        if (digits == 0) return 0;
    }
    if (ch != ' ') return 0;
    if (ovf == 0) goto numi;

    token->type = typereal;
    token->flags = 0;
    token->length = 0;
    token->value.rval = (double) atof(string);
    return 1;

numi:
    token->type = typeint;
    token->flags = 0;
    token->length = 0;
    token->value.ival = (sign == 2)? -num: num;
    return 1;
}

/* Make a name token by looking up its string in the name table */

void nametoken(struct object *token, char *string, int length, int flags)
{   struct name *nameptr;
    vmref *nameslot, nameref;
    char *s;
    unsigned int hash = 0;

    if (length < 0) length = strlen(string);
    s = string + length;
    while (s != string) hash = hash * 12345 + *--s;

    nameslot = &nametable[hash % nametablesize];
    for (;;)
    {   nameref = *nameslot;
        if (nameref == 0) break;
        nameptr = vmnptr(nameref);
        if (nameptr->hash == hash &&
            nameptr->length == length &&
            memcmp(nameptr->string, string, length) == 0)
            goto lab;
        nameslot = &nameptr->chain;
    }

    nameref = vmalloc(sizeof (struct name) - 2 + length);
    nameptr = vmnptr(nameref);
    nameptr->chain = 0;
    nameptr->hash = hash;
    nameptr->length = length;
    memcpy(nameptr->string, string, length);
    *nameslot = nameref;

lab:
    token->type = typename;
    token->flags = flags;
    token->length = 0;
    token->value.vref = nameref;
}

/* Create a new dictionary token */

void dicttoken(struct object *token, int size)
{   struct dictionary *dict;
    vmref dref;
    int length, slots, p, i;

    /* Table of primes. */

    static int primes[] =
        {  3,   5,   7,  11,  13,  17,  19,  23,  29,  31,
          37,  41,  43,  47,  53,  59,  61,  67,  71,  73,
          79,  83,  89,  97, 101, 103, 107, 109, 113, 127,
         131, 137, 139, 149, 151, 157, 163, 167, 173, 179,
         181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
         239, 241, 251, 257
        };

    /* Choose the number of hash table slots.  Make it an odd number about
     * 1.25 times the dictionary size, with at least one unused slot (so
     * the search always terminates). */

    if (size < 0) error(errrangecheck);
    slots = size + size/4 + 1 | 1;

    /* Round the number up to the next prime. */

lab:
    if (slots > 65535) error(errlimitcheck);
    for (i = 0; p = primes[i], p * p <= slots ; i++)
        if (slots % p == 0)
        {   slots += 2;
            goto lab;
        }

    /* Now create the dictionary. */

    length = (sizeof (struct dictionary)) +
              sizeof (struct dictentry) * (slots - 1);
    dref = vmalloc(length);
    dict = vmdptr(dref);
    dict->type = typedict;
    dict->flags = 0;
    dict->slots = slots;
    dict->size = size;
    dict->full = 0;
    dict->saved = vmnest;
    dict->length = length;
    token->type = typedict;
    token->flags = 0;
    token->length = 0;
    token->value.vref = dref;
}

/* Put the value of a key into a dictionary */

void dictput(vmref dref, struct object *key, struct object *val)
{   struct dictionary *dict;
    union dictkey lkey, ekey;
    struct object nkey;
    struct vmlist *vmlist;
    vmref vmlref, *vmlslot;
    unsigned int hash, slot;

    dict = vmdptr(dref);
    if (dict->flags & flagwprot) error(errinvalidaccess);

    /* Convert strings to names */

    if (key->type == typenull)
        error(errtypecheck);
    if (key->type == typestring)
        nametoken(&nkey, vmsptr(key->value.vref), key->length, 0);
    else
        nkey = *key;
    lkey.keyobj = nkey;
    lkey.keyobj.flags = 0;

    /* If we have not saved the dictionary since the last vm save, we must
     * save it now. */

    if (dict->saved < vmnest)
    {   vmlslot = &(vmstack[vmnest].hlist[vmhashsize]);
        vmlref = vmalloc(sizeof (struct vmlist) + dict->length);
        vmlist = vmvptr(vmlref);
        vmlist->chain = *vmlslot;
        vmlist->vref = dref;
        vmlist->length = dict->length;
        memcpy((char *) (vmlist + 1), (char *) dict, dict->length);
        *vmlslot = vmlref;
        dict->saved = vmnest;
    }

    /* Compute the hash value.  If we need to rehash we add the hash value
     * modulo the table size.  Since the size is a prime number this will
     * scan the entire table - unless the hash value is zero when we add
     * one instead. */

    slot = (lkey.keyint[0] * 2 + lkey.keyint[1]) % dict->slots;
    hash = slot;
    if (hash == 0) hash = 1;

    /* Search the table for the key, or an empty slot.  Then insert. */

    for (;;)
    {   ekey.keyobj = dict->entries[slot].key;
        if (ekey.keyobj.type == 0)
        {   if (dict->full == dict->size) error(errdictfull);
            dict->full++;
            dict->entries[slot].key = nkey;
            break;
        }
        ekey.keyobj.flags = 0;
        if (ekey.keyint[1] == lkey.keyint[1] &&
            ekey.keyint[0] == lkey.keyint[0])
        {   dict->entries[slot].key.flags = nkey.flags;
            break;
        }
        slot += hash;
        if (slot >= dict->slots) slot -= dict->slots;
    }
    dict->entries[slot].val = *val;
}

/* Get the value of a key from a dictionary */

int dictget(vmref dref, struct object *key, struct object *val, int flags)
{   struct dictionary *dict;
    union dictkey lkey, ekey;
    unsigned int hash, slot;

    dict = vmdptr(dref);
    if (dict->flags & flags) error(errinvalidaccess);

    /* Convert strings to names */

    if (key->type == typenull)
        error(errtypecheck);
    if (key->type == typestring)
        nametoken(&lkey.keyobj, vmsptr(key->value.vref), key->length, 0);
    else
    {   lkey.keyobj = *key;
        lkey.keyobj.flags = 0;
    }

    /* Compute the hash value. */

    slot = (lkey.keyint[0] * 2 + lkey.keyint[1]) % dict->slots;
    hash = slot;
    if (hash == 0) hash = 1;

    /* Search the table. */

    for (;;)
    {   ekey.keyobj = dict->entries[slot].key;
        if (ekey.keyobj.type == 0)
            return 0;
        ekey.keyobj.flags = 0;
        if (ekey.keyint[1] == lkey.keyint[1] &&
            ekey.keyint[0] == lkey.keyint[0])
        {   *val = dict->entries[slot].val;
            return 1;
        }
        slot += hash;
        if (slot >= dict->slots) slot -= dict->slots;
    }
}

/* Find a key in the dictionary stack */

int dictfind(struct object *key, struct object *val)
{   struct dictionary *dict;
    union dictkey lkey, ekey;
    unsigned int hash, slot;
    int nest = dictnest;

    /* Convert strings to names */

    if (key->type == typenull)
        error(errtypecheck);
    if (key->type == typestring)
        nametoken(&lkey.keyobj, vmsptr(key->value.vref), key->length, 0);
    else
    {   lkey.keyobj = *key;
        lkey.keyobj.flags = 0;
    }

    /* Search all the directories on the stack. */

    while (nest--)
    {   dict = vmdptr(dictstack[nest].value.vref);
        if (dict->flags & flagrprot) error(errinvalidaccess);
        slot = (lkey.keyint[0] * 2 + lkey.keyint[1]) % dict->slots;
        hash = slot;
        if (hash == 0) hash = 1;
        for (;;)
        {   ekey.keyobj = dict->entries[slot].key;
            if (ekey.keyobj.type == 0)
                break;
            ekey.keyobj.flags = 0;
            if (ekey.keyint[1] == lkey.keyint[1] &&
                ekey.keyint[0] == lkey.keyint[0])
            {   *val = dict->entries[slot].val;
                return nest;
            }
            slot += hash;
            if (slot >= dict->slots) slot -= dict->slots;
        }
    }
    return -1;
}

/* Pack an array */

vmref arraypack(struct object *aptr, int length)
{   char *sptr;
    int len = 0;
    while (length--)
    {   sptr = vmstring(len, sizeof (struct object));
        len += pack(aptr++, sptr);
    }
    return vmalloc(len);
}

/* Unpack an array */

void arrayunpk(struct object *aptr, char *sptr, int length)
{   while (length--) sptr += unpack(aptr++, sptr);
}

/* Pack the next element of an array */

int pack(struct object *token, char *sptr)
{   int type = token->type;
    int flags = token->flags;
    sptr[0] = type | flags;
    switch (type)
    {   case typenull:
        case typemark:
            return 1;

        case typesave:
        case typefile:
            sptr[1] = token->length;
            memcpy(sptr + 2, (char *) &token->value, sizeof token->value);
            return 2 + sizeof token->value;

        case typeoper:
        case typebool:
            sptr[1] = token->value.ival;
            return 2;

        case typeint:
            if (token->value.ival >= -32 && token->value.ival < 224)
            {   sptr[0] = typechar | flags;
                sptr[1] = token->value.ival + 32;
                return 2;
            }
        case typefont:
        case typereal:
        case typename:
        case typedict:
        case typeoper2:
            memcpy(sptr + 1, (char *) &token->value, sizeof token->value);
            return 1 + sizeof token->value;

        case typearray:
        case typepacked:
        case typestring:
            memcpy(sptr + 1, (char *) &token->length,
                   sizeof token->length + sizeof token->value);
            return 1 + sizeof token->length + sizeof token->value;
    }
}

/* Unpack the next element of an array */

int unpack(struct object *token, char *sptr)
{   int type = ((unsigned char *) sptr)[0];
    token->flags = type & 0xf0;
    token->type = type = type & 0x0f;
    switch (type)
    {   case typenull:
        case typemark:
            token->length = 0;
            token->value.ival = 0;
            return 1;

        case typesave:
        case typefile:
            token->length = ((unsigned char *) sptr)[1];
            memcpy((char *) &token->value, sptr + 2, sizeof token->value);
            return 2 + sizeof token->value;

        case typeoper:
        case typebool:
            token->length = 0;
            token->value.ival = ((unsigned char *) sptr)[1];
            return 2;

        case typeoper2:
            token->type = typeoper;
        case typeint:
        case typefont:
        case typereal:
        case typename:
        case typedict:
            token->length = 0;
            memcpy((char *) &token->value, sptr + 1, sizeof token->value);
            return 1 + sizeof token->value;

        case typearray:
        case typepacked:
        case typestring:
            memcpy((char *) &token->length, sptr + 1,
                   sizeof token->length + sizeof token->value);
            return 1 + sizeof token->length + sizeof token->value;

        case typechar:
            token->type = typeint;
            token->length = 0;
            token->value.ival = ((unsigned char *) sptr)[1] - 32;
            return 2;
    }
}

/* Initialise the virtual machine */

void vminit(int parms)
{   vmnest = 0;
    vmsegno = vmparms = parms;
    vmsegsize = memvmin;
    if (vmsegsize > 0x1000000) vmsegsize = 0x1000000;
    vmused = 0;
    vmhwm = 0;
    vmmax = vmsegsize * (vmsegmax - vmparms);
    memset((char *) &vmbeg, 0, sizeof vmbeg);
    memset((char *) &vmnext, 0, sizeof vmnext);
    memset((char *) &vmsize, 0, sizeof vmsize);
    memset((char *) vmstack, 0, sizeof vmstack);
    packing = 0;
}

/* Tidy up the virtual machine */

void vmtidy(void)
{   while (vmsegno >= vmparms)
    {   memfree(vmbeg[vmsegno], vmsize[vmsegno]);
        vmsegno--;
    }
}

/* Set up a virtual machine parameter segment */

void vmparm(int parm, void *beg, int size)
{   vmbeg[parm] = beg;
    vmsize[parm] = vmnext[parm] = size;
}

/* Allocate some memory in the virtual machine */

vmref vmalloc(int size)
{   vmref vref;
    int blksize = (size + (mcalign - 1)) & ~(mcalign - 1);
    if (blksize > vmsize[vmsegno] - vmnext[vmsegno])
        vmallocseg(blksize, 0);
    vref = vmxref(vmsegno, vmnext[vmsegno]);
    vmnext[vmsegno] += blksize;
    vmused += blksize;
    if (vmused > vmhwm) vmhwm = vmused;
    return vref;
}

/* Allocate some memory in the virtual machine */

void *vmallocv(int size)
{   vmref vref = vmalloc(size);
    return vmvptr(vref);
}

/* Convert a virtual machine reference to an address */

void *vmxptr(vmref vref)
{   return vmvptr(vref);
}

/* Preallocate space for a string at the end of the virtual machine memory */

char *vmstring(int length, int size)
{   int blksize = length + size;
    if (blksize > vmsize[vmsegno] - vmnext[vmsegno])
        vmallocseg(blksize, length);
    return vmbeg[vmsegno] + vmnext[vmsegno] + length;
}

/* Allocate a new virtual machine memory segment */

void vmallocseg(int blksize, int length)
{   char *vbeg;
    int segsize, numsegs;
    numsegs = (blksize + vmsegsize - 1) / vmsegsize;
    segsize = vmsegsize * numsegs;
    if (vmsegno + numsegs >
            vmsegmax + (vmnext[vmsegno] == 0 ? 1 : 0)) error(errVMerror);
    vbeg = memalloc(segsize);
    if (vbeg == NULL) error(errmemoryallocation);
    memcpy(vbeg, vmbeg[vmsegno] + vmnext[vmsegno], length);
    if (vmnext[vmsegno] == 0)
    {   memfree(vmbeg[vmsegno], vmsize[vmsegno]);
        vmsegno--;
    }
    else
        vmused += vmsize[vmsegno] - vmnext[vmsegno];
    while (numsegs--)
    {   vmsegno++;
        if (numsegs == 0)
        {   vmbeg[vmsegno] = vbeg;
            vmsize[vmsegno] = segsize;
        }
        else
        {   vmbeg[vmsegno] = NULL;
            vmsize[vmsegno] = 0;
        }
        vmnext[vmsegno] = 0;
    }
}

/* Save some virtual machine memory before updating it */

void vmsavemem(vmref vref, int length)
{   struct vmframe *vmframe;
    struct vmlist *vmlist;
    vmref vmlref, *vmlslot;
    unsigned int hash;

    vmframe = &vmstack[vmnest];

    /* We don't need to save it if it is more recent than the last save */

    if (vmscheck(vmframe, vref)) return;

    /* Compute the hash value */

    hash = vref % vmhashsize;

    /* Look on the hash chain to see if we have saved it already */

    vmlslot = &(vmframe->hlist[hash]);
    vmlref = *vmlslot;
    while (vmlref)
    {   vmlist = vmvptr(vmlref);
        if (vmlist->vref == vref && vmlist->length >= length)
            return;
        vmlref = vmlist->chain;
    }

    /* If we cannot find it save a copy and add it to the list */

    vmlref = vmalloc(sizeof (struct vmlist) + length);
    vmlist = vmvptr(vmlref);
    vmlist->chain = *vmlslot;
    vmlist->vref = vref;
    vmlist->length = length;
    memcpy((char *) (vmlist + 1), vmsptr(vref), length);
    *vmlslot = vmlref;
}

/* Save the virtual machine */

void vmsave(struct object *token)
{   struct vmframe *vmframe;
    if (istate.flags & intgraph) error(errundefined);
    if (vmnest == vmstacksize || gnest == gstacksize) error(errlimitcheck);
    gsave();
    vmframe = &vmstack[vmnest];
    token->type = typesave;
    token->flags = 0;
    token->length = vmnest;
    token->value.ival = vmframe->generation;
    vmnest++;
    vmframe++;
    vmframe->generation++;
    vmframe->gnest = gnest;
    vmframe->packing = packing;
    vmframe->vsegno = vmsegno;
    vmframe->vnext = vmnext[vmsegno];
    vmframe->vused = vmused;
    memset((char *)vmframe->hlist, 0, sizeof vmframe->hlist);
}

/* Restore the virtual machine */

void vmrest(int nest, int generation)
{   struct vmframe *vmframe;
    int vsegno, vnext, vused;
    if (istate.flags & intgraph) error(errundefined);
    if (nest < istate.vmbase ||
        nest >= vmnest ||
            (generation != -1 && generation != vmstack[nest].generation))
        error(errinvalidrestore);
    vmframe = &vmstack[nest + 1];

    /* Check the stacks */

    vmrestcheck(vmframe, operstack, opernest);
    vmrestcheck(vmframe, execstack, execnest);
    vmrestcheck(vmframe, dictstack, dictnest);

    gnest = vmframe->gnest;
    packing = vmframe->packing;
    vsegno = vmframe->vsegno;
    vnext = vmframe->vnext;
    vused = vmframe->vused;

    /* Restore file and name tables, the font cache, and the memory */

    vmrestfiles(nest);
    vmrestnames(vmframe);
    vmrestfont(vmframe);
    vmrestmem(nest);

    /* Clear the memory we have freed */

    memset(vmbeg[vsegno] + vnext, 0, vmnext[vsegno] - vnext);
    while (vmsegno > vsegno)
    {   memfree(vmbeg[vmsegno], vmsize[vmsegno]);
        vmsegno--;
    }
    vmnext[vmsegno] = vnext;
    vmused = vused;

    /* Restore the graphics state */

    grest();
}

/* Check a stack for objects refering to memory we are recovering */

void vmrestcheck(struct vmframe *vmframe,
                 struct object *stackptr, int stackcnt)
{   while (stackcnt--)
    {   if (stackptr->type == typestring ||
            stackptr->type == typearray ||
            stackptr->type == typepacked ||
            stackptr->type == typedict)
            if (vmscheck(vmframe, stackptr->value.vref))
                error(errinvalidrestore);
        stackptr++;
    }
}

/* Restore the file table */

void vmrestfiles(int nest)
{   struct file *file = &filetable[3];
    int num = 3;
    while (num < filetablesize)
    {   if (file->open != 0 && file->saved > nest)
        {   fclose(file->fptr);
            file->fptr = NULL;
            file->generation++;
            file->open = 0;
        }
        file++;
        num++;
    }
}

/* Restore the name table */

void vmrestnames(struct vmframe *vmframe)
{   vmref *nslot1, *nslot2, nref;
    int i;
    nslot1 = &nametable[0];
    i = nametablesize;

    /* Scan each hash chain.  Unlink all the names more recent than the
     * save (the tail of the chain). */

    while (i--)
    {   nslot2 = nslot1;
        for (;;)
        {   nref = *nslot2;
            if (nref == 0)
               break;
            if (vmscheck(vmframe, nref))
            {   *nslot2 = 0;
                break;
            }
            nslot2 = &(vmnptr(nref)->chain);
        }
        nslot1++;
    }
}

/* Restore the saved memory */

void vmrestmem(int nest)
{   struct vmframe *vmframe;
    struct vmlist *vmlist;
    vmref vmlref, *vmlslot;
    int i;

    vmframe = &vmstack[vmnest];

    do
    {   vmlslot = &(vmframe->hlist[0]);
        i = vmhashsize + 1;
        while (i--)
        {   vmlref = *vmlslot++;
            while (vmlref)
            {   vmlist = vmvptr(vmlref);
                memcpy(vmsptr(vmlist->vref),
                       (char *)(vmlist + 1), vmlist->length);
                vmlref = vmlist->chain;
            }
        }
        vmframe--;
        vmnest--;
    }   while (vmnest > nest);
}

/* Convert an object to a string */

int cvstring(struct object *token, char *sptr, int length)
{   struct name *nptr;
    char *buf;
    int len;
    switch (token->type)
    {   case typeint:
            buf = namebuf;
            len = sprintf(buf, "%d", token->value.ival);
            break;

        case typereal:
            buf = namebuf;
            len = sprintf(buf, "%g", token->value.rval);
            if (strchr(buf, '.') == NULL) buf[len++] = '.';
            break;

        case typebool:
            if (token->value.ival)
                buf = "true";
            else
                buf = "false";
            goto str;

        case typestring:
            if (token->flags & flagrprot) error(errinvalidaccess);
            len = token->length;
            buf = vmsptr(token->value.vref);
            break;

        case typename:
            nptr = vmnptr(token->value.vref);
            len = nptr->length;
            buf = nptr->string;
            break;

        case typeoper:
            buf = optable[token->value.ival].sptr;
            goto str;

        default:
            buf = "--nostringval--";
str:        len = strlen(buf);
    }
    if (len > length) error(errrangecheck);
    if (sptr != buf) memcpy(sptr, buf, len);
    return len;
}

/* Print an object in "=" style */

void printequals(FILE *fptr, struct object *token)
{   char *sptr;
    int length;
    if (token->type == typestring)
    {   if (token->flags & flagrprot)
        {   putstr(fptr, "--nostringval--");
            return;
        }
        length = token->length;
        sptr = vmsptr(token->value.vref);
    }
    else
        length = cvstring(token, sptr = namebuf, namebufsize);
    putmem(fptr, sptr, length);
}

/* Print an object in "==" style */

void printeqeq(FILE *fptr, struct object *token, int depth, int count)
{   struct object upelem, *element;
    char *sptr;
    int length;
    if (count != 0) putch(fptr, ' ');
    switch (token->type)
    {   case typenull:
        case typemark:
        case typesave:
        case typefile:
        case typedict:
        case typefont:
type:       putch(fptr, '-');
            putstr(fptr, typetable[token->type]);
            putch(fptr, '-');
            break;

        case typeoper:
            putstr(fptr, "--");
            putstr(fptr, optable[token->value.ival].sptr);
            putstr(fptr, "--");
            break;

        case typearray:
            if (!(token->flags & flagrprot) && depth < maxdepth)
            {   putch(fptr, (token->flags & flagexec) ? '{' : '[');
                length = token->length;
                element = vmaptr(token->value.vref);
                while (length--)
                    printeqeq(fptr, element++, depth+1, ++count);
                putch(fptr, ' ');
                putch(fptr, (token->flags & flagexec) ? '}' : ']');
                break;
            }
            goto type;

        case typepacked:
            if (!(token->flags & flagrprot) && depth < maxdepth)
            {   putch(fptr, (token->flags & flagexec) ? '{' : '[');
                length = token->length;
                sptr = vmsptr(token->value.vref);
                while (length--)
                {   sptr += unpack(&upelem, sptr);
                    printeqeq(fptr, &upelem, depth+1, ++count);
                }
                putch(fptr, ' ');
                putch(fptr, (token->flags & flagexec) ? '}' : ']');
                break;
            }
            goto type;

        case typestring:
            if (!(token->flags & flagrprot))
            {   putch(fptr, '(');
                putcheck(fptr, vmsptr(token->value.vref), token->length);
                putch(fptr, ')');
                break;
            }
            goto type;

        case typename:
            if (!(token->flags & flagexec)) putch(fptr, '/');
        default:
            length = cvstring(token, namebuf, namebufsize);
            putcheck(fptr, namebuf, length);
            break;
    }
}

/* Test two objects for equality */

int equal(struct object *token1, struct object *token2)
{   struct object tokn1 = *token1, tokn2 = *token2;
    struct name *nptr;
    char *buf1, *buf2;
    if (tokn1.type == typeint && tokn2.type == typeint)
        return tokn1.value.ival == tokn2.value.ival;
    if (tokn1.type == typeint)
    {   tokn1.type = typereal;
        tokn1.value.rval = tokn1.value.ival;
    }
    if (tokn2.type == typeint)
    {   tokn2.type = typereal;
        tokn2.value.rval = tokn2.value.ival;
    }
    if (tokn1.type == typereal && tokn2.type == typereal)
        return tokn1.value.rval == tokn2.value.rval;
    if ((tokn1.flags & flagrprot) || (tokn2.flags & flagrprot))
            error(errinvalidaccess);
    if (tokn1.type == typestring)
        buf1 = vmsptr(tokn1.value.vref);
    if (tokn2.type == typestring)
        buf2 = vmsptr(tokn2.value.vref);
    if (tokn1.type == typename)
    {   nptr = vmnptr(tokn1.value.vref);
        tokn1.type = typestring;
        tokn1.length = nptr->length;
        buf1 = &nptr->string[0];
    }
    if (tokn2.type == typename)
    {   nptr = vmnptr(tokn2.value.vref);
        tokn2.type = typestring;
        tokn2.length = nptr->length;
        buf2 = &nptr->string[0];
    }
    if (tokn1.type == typestring && tokn2.type == typestring)
    {   if (tokn1.length != tokn2.length) return 0;
        return (memcmp(buf1, buf2, tokn1.length) == 0);
    }
    if (tokn1.type == tokn2.type &&
        tokn1.length == tokn2.length &&
        tokn1.value.ival == tokn2.value.ival)
        return 1;
    return 0;
}

/* Compare two objects */

int compare(struct object *token1, struct object *token2)
{   struct object tokn1 = *token1, tokn2 = *token2;
    unsigned char *sptr1, *sptr2;
    int length;
    if (tokn1.type == typeint && tokn2.type == typeint)
    {   if      (tokn1.value.ival == tokn2.value.ival)
            return  0;
        else if (tokn1.value.ival < tokn2.value.ival)
            return -1;
        else
            return  1;
    }
    if (tokn1.type == typeint)
    {   tokn1.type = typereal;
        tokn1.value.rval = tokn1.value.ival;
    }
    if (tokn2.type == typeint)
    {   tokn2.type = typereal;
        tokn2.value.rval = tokn2.value.ival;
    }
    if (tokn1.type == typereal && tokn2.type == typereal)
    {   if      (tokn1.value.rval == tokn2.value.rval)
            return  0;
        else if (tokn1.value.rval < tokn2.value.rval)
            return -1;
        else
            return  1;
    }
    if (tokn1.type == typestring && tokn2.type == typestring)
    {   if ((tokn1.flags & flagrprot) || (tokn2.flags & flagrprot))
            error(errinvalidaccess);
        length = (tokn1.length < tokn2.length) ?
                  tokn1.length : tokn2.length;
        sptr1 = (unsigned char *) vmsptr(tokn1.value.vref);
        sptr2 = (unsigned char *) vmsptr(tokn2.value.vref);
        while (length--)
        {   if (*sptr1 != *sptr2)  return (*sptr1 - *sptr2);
            sptr1++;
            sptr2++;
        }
        return (tokn1.length - tokn2.length);
    }
    error(errtypecheck);
    return 0;
}


/* Bind a procedure */

void bind(struct object *proc, int depth)
{   struct object token, *aptr;
    char *sptr;
    int length, len;
    length = proc->length;

    /* Array.  If not write protected, save it and scan looking up executable
     * names and replacing them if they are operators.  Recurse for embedded
     * procedures */

    if (proc->type == typearray)
    {   if (proc->flags & flagwprot) return;
        aptr = vmaptr(proc->value.vref);
        arraysave(proc->value.vref, length);
        while (length--)
        {   if (aptr->flags & flagexec)
                if      (aptr->type == typename)
                {   if (dictfind(aptr, &token) != -1)
                        if (token.type == typeoper) *aptr = token;
                }
                else if (aptr->type == typearray || aptr->type == typepacked)
                {   if (depth == maxdepth) error(errlimitcheck);
                    bind(aptr, depth + 1);
                    aptr->flags |= flagwprot;
                }
            aptr++;
        }
    }

    /* Packed array.  First unpack it to calculate the length to save.  Then
     * scan it unpacking sequentially.  Replace names with oper2 (same size).
     * Repack any elements we update */

    else
    {   sptr = vmsptr(proc->value.vref);
        len = 0;
        while (length--)
            len += unpack(&token, sptr + len);
        vmsavemem(proc->value.vref, len);
        length = proc->length;
        while (length--)
        {   len = unpack(&token, sptr);
            if (token.flags & flagexec)
                if      (token.type == typename)
                {   if (dictfind(&token, &token) != -1)
                        if (token.type == typeoper)
                        {   token.type = typeoper2;
                            pack(&token, sptr);
                        }
                }
                else if (token.type == typearray || token.type == typepacked)
                {   if (depth == maxdepth) error(errlimitcheck);
                    bind(&token, depth + 1);
                    token.flags |= flagwprot;
                    pack(&token, sptr);
                }
            sptr += len;
        }
    }
}

/* Estimate user (elapsed) time in milliseconds */

int usertime(void)
{   time(&time2);
    return (time2 - time1) * 1000;
}

/* Stop */

void stop(void)
{   struct object token, *token1;
    int nest = execnest;
    token1 = &execstack[nest];

    /* Search the execution stack for a "stopped".  Long jump if we find
     * one.  Close "run" files as we go. */

    while (nest)
    {   token1--;
        if      (token1->flags & flagrun)
            fileclose(token1 - 1);
        else if (token1->flags & flagstop)
        {   if (opernest == operstacksize) error(errstackoverflow);
            token.type = typebool;
            token.flags = 0;
            token.length = 0;
            token.value.ival = 1;
            operstack[opernest++] = token;
            execnest = nest - 2;
            errorjmp(token1->length, 1);
        }
        nest--;
    }
}

/* Error routine */

void error(int errnum)
{   struct object token;
    int nest;

    errornum = errnum;
    errorstring = errortable[errornum];

    /* Error during initialisation */

    if (errorflag == 0)
        nest = 0;

    /* Error trapping enabled */

    else
    {   flushlevel(0);

        /* Save the error name and command in memory */

        errdstoken[edserrorname] = errorname[errnum];
        errdstoken[edserrorname].flags = 0;
        errdstoken[edscommand] = *currtoken;

        /* If we are not already in the error handler, save the stacks too,
         * and call it if we can (and not killed) */

        if (errorflag == 2)
        {   errorflag = 1;
            errorarray(&token, operstack, opernest);
            errdstoken[edsostack] = token;
            errorarray(&token, execstack, execnest);
            errdstoken[edsestack] = token;
            errorarray(&token, dictstack, dictnest);
            errdstoken[edsdstack] = token;
            if (errnum != errkill)
            {   if (execnest < execstacksize &&
                    dictget(errordict.value.vref, &errorname[errnum],
                            &token, 0))
                {   execstack[execnest++] = token;
                    errorflag = 2;
                }
            }
        }

        /* Otherwise return to interactive or quit */

        if (errorflag != 2)
        {   errorflag = 2;
            errorexit();
            errormsg();
            nest = 0;
        }
        else
            nest = inest;
    }

    errorjmp(nest, 1);
}

/* Error save array, if there is enough memory */

void errorarray(struct object *token1, struct object *aptr, int length)
{   struct object token;
    if (vmused + length * sizeof (struct object) <= vmmax)
    {   token.type = typearray;
        token.flags = 0;
        token.length = length;
        token.value.vref = arrayalloc(length);
        arraycopy(vmaptr(token.value.vref), aptr, length);
    }
    else
    {   token.type = typenull;
        token.flags = 0;
        token.length = 0;
        token.value.ival = 0;
    }
    *token1 = token;
}

/* Error return to interactive mode or exit */

void errorexit(void)
{   struct file *file;
    FILE *fptr;
    if (prompting)
    {   execnest = 1;
        file = &filetable[0];
        file->emode = 0;
        fptr = file->fptr;
        while (file->ch != EOF && file->ch != '\n')
            file->ch = getf(file, fptr);
    }
    else
    {   execnest = 0;
        returncode = 10;
    }
}

/* Print an error message */

void errormsg(void)
{   if (sstderr)
    {   putstr(sstderr, "post: error: ");
        printequals(sstderr, &errdstoken[edserrorname]);
        putstr(sstderr, ", command ");
        printeqeq(sstderr, &errdstoken[edscommand], 0, 0);
        putch(sstderr, '\n');
    }
}

/* Long jump to the error jump buffer */

void errorjmp(int nest, int num)
{   while (nest < inest)
    {   if (istate.pfcrec)
            istate.pfcrec->count = 0;
        istate = istack[--inest];
    }
    longjmp(istate.errjmp, num);
}

/* End of file "postint.c" */
