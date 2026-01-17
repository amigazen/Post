/* PostScript interpreter file "postop1.c" - operators (1) */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* .error */

void operror(void)
{   struct object token;
    int i;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 1;
    errdstoken[0] = token;
    errorflag = 1;
    for (i = 0; i < edsmax; i++)
        dictput(errdsdict.value.vref, &errdsname[i], &errdstoken[i]);
    errorflag = 2;

    stop();

    errorexit();
    if (dictget(errordict.value.vref, &errorname[0], &token, 0))
        execstack[execnest++] = token;
    else
        errormsg();
    errorjmp(0, 1);
}

/* .handleerror */

void ophandleerror(void)
{   struct object token;
    int i;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    errorflag = 1;
    for (i = 0; i < edsmax; i++)
        dictget(errdsdict.value.vref, &errdsname[i], &errdstoken[i], 0);
    dictput(errdsdict.value.vref, &errdsname[0], &token);
    errorflag = 2;
    token = errdstoken[0];
    if (token.type == typebool && token.value.ival) errormsg();
}

/* [ */

void opmark(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typemark;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    operstack[opernest++] = token;
}

/* ] */

void opkram(void)
{   struct object token, *token1;
    int nest, length;
    nest = opernest;
    token1 = &operstack[nest];
    for (;;)
    {    if (nest == 0) error(errunmatchedmark);
         if ((--token1)->type == typemark) break;
         nest--;
    }
    length = opernest - nest;
    token.type = typearray;
    token.flags = 0;
    token.length = length;
    token.value.vref = arrayalloc(length);
    arraycopy(vmaptr(token.value.vref), token1 + 1, length);
    *token1 = token;
    opernest = nest;
}

/* = */

void opequals(void)
{   if (opernest < 1) error(errstackunderflow);
    if (sstdout)
    {   printequals(sstdout, &operstack[opernest - 1]);
        putch(sstdout, '\n');
    }
    opernest--;
}

/* == */

void opeqeq(void)
{   if (opernest < 1) error(errstackunderflow);
    if (sstdout)
    {   printeqeq(sstdout, &operstack[opernest - 1], 0, 0);
        putch(sstdout, '\n');
    }
    opernest--;
}

/* abs */

void opabs(void)
{   struct object *token1;
    int num1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typeint)
    {   num1 = token1->value.ival;
        if (num1 < 0)
        {   num1 = -num1;
            if (num1 < 0)
            {   token1->type = typereal;
                token1->value.rval = -((float) num1);
            }
            else
                token1->value.ival = num1;
        }
    }
    else if (token1->type == typereal)
    {   if (token1->value.rval < 0.0)
            token1->value.rval = -token1->value.rval;
    }
    else
        error(errtypecheck);
}

/* add */

void opadd(void)
{   struct object token, *token1, *token2;
    int num1, num2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type == typeint && token2->type == typeint)
    {   num1 = token1->value.ival;
        num2 = token2->value.ival;
        if      (num1 > 0 && num2 > 0)
        {   num1 += num2;
            if (num1 > 0)
            {   token1->value.ival = num1;
                opernest--;
                return;
            }
        }
        else if (num1 < 0 && num2 < 0)
        {   num1 += num2;
            if (num1 < 0)
            {   token1->value.ival = num1;
                opernest--;
                return;
            }
        }
        else
        {   num1 += num2;
            token1->value.ival = num1;
            opernest--;
            return;
        }
    }
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    else
        if (token.type != typereal) error(errtypecheck);
    if (token2->type == typeint)
        token.value.rval += token2->value.ival;
    else
    {   if (token2->type != typereal) error(errtypecheck);
        token.value.rval += token2->value.rval;
    }
    *token1 = token;
    opernest--;
}

/* aload */

void opaload(void)
{   struct object *token1;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray && token1->type != typepacked)
        error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    length = token1->length;
    if (opernest + length > operstacksize) error(errstackoverflow);
    *(token1 + length) = *token1;
    if (token1->type == typearray)
        arraycopy(token1, vmaptr(token1->value.vref), length);
    else
        arrayunpk(token1, vmsptr(token1->value.vref), length);
    opernest += length;
}

/* anchorsearch */

void opanchorsearch(void)
{   struct object token, *token1, *token2;
    char *sptr1, *sptr2;
    int len1, len2, bool;
    if (opernest < 2) error(errstackunderflow);
    if (opernest + 1 > operstacksize) error(errstackoverflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    len1 = token1->length;
    sptr1 = vmsptr(token1->value.vref);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    len2 = token2->length;
    sptr2 = vmsptr(token2->value.vref);
    bool = 0;
    if (len2 <= len1 && memcmp(sptr1, sptr2, len2) == 0)
    {   bool = 1;
        token = *token1;
        token.length = len2;
        *token2 = token;
        token.length = len1 - len2;
        token.value.vref = token1->value.vref + len2;
        *token1 = token;
        token2++;
        opernest++;
    }
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = bool;
    *token2 = token;
}

/* and */

void opand(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if ((token1->type == typebool && token2->type == typebool) ||
        (token1->type == typeint  && token2->type == typeint))
        token1->value.ival &= token2->value.ival;
    else
        error(errtypecheck);
    opernest--;
}

/* array */

void oparray(void)
{   struct object token, *token1;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    length = token1->value.ival;
    if (length < 0 || length > 65535) error(errrangecheck);
    token.type = typearray;
    token.flags = 0;
    token.length = length;
    token.value.vref = arrayalloc(length);
    *token1 = token;
}

/* astore */

void opastore(void)
{   struct object *token1, *aptr;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    length = token1->length;
    if (opernest < length + 1) error(errstackunderflow);
    aptr = token1 - length;
    arraysave(token1->value.vref, length);
    arraycopy(vmaptr(token1->value.vref), aptr, length);
    *aptr = *token1;
    opernest -= length;
}

/* atan */

void opatan(void)
{   struct object token, *token1, *token2;
    float flt2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type != typereal) error(errtypecheck);
    if      (token2->type == typeint)
        flt2 = token2->value.ival;
    else if (token2->type == typereal)
        flt2 = token2->value.rval;
    else
        error(errtypecheck);
    flt2 = (float) atan2((double) token.value.rval, (double) flt2);
    flt2 *= radtodeg;
    if (flt2 < 0.0) flt2 += 360.0;
    token.value.rval = flt2;
    *token1 = token;
    opernest--;
}

/* begin */

void opbegin(void)
{   struct object *token1;
    struct dictionary *dict;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typedict) error(errtypecheck);
    dict = vmdptr(token1->value.vref);
    if (dict->flags & flagrprot) error(errinvalidaccess);
    if (dictnest == dictstacksize) error(errdictstackoverflow);
    dictstack[dictnest++] = *token1;
    opernest--;
}

/* bind */

void opbind(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typearray || token1->type == typepacked)
        bind(token1, 0);
    else
        error(errtypecheck);
}

/* bitshift */

void opbitshift(void)
{   struct object *token1, *token2;
    int num1, num2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typeint || token2->type != typeint)
        error(errtypecheck);
    num1 = token1->value.ival;
    num2 = token2->value.ival;
    if (num2 >= 0)
        if (num2 > 31)
            token1->value.ival = 0;
        else
            token1->value.ival = (unsigned) num1 << num2;
    else
    {   num2 = -num2;
        if (num2 > 31)
            token1->value.ival = 0;
        else
            token1->value.ival = (unsigned) num1 >> num2;
    }
    opernest--;
}

/* bytesavailable */

void opbytesavailable(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (filecheck(token1, openread) == NULL) error(errioerror);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = -1;
    *token1 = token;
}

/* ceiling */

void opceiling(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type == typeint)
        return;
    else if (token1->type == typereal)
        token1->value.rval = (float) ceil((double) token1->value.rval);
    else
        error(errtypecheck);
}

/* clear */

void opclear(void)
{   opernest = 0;
}

/* cleardictstack */

void opcleardictstack(void)
{   dictnest = 2;
}

/* cleartomark */

void opcleartomark(void)
{   struct object *token1;
    int nest;
    nest = opernest;
    token1 = &operstack[nest];
    for (;;)
    {    if (nest == 0) error(errunmatchedmark);
         nest--;
         if ((--token1)->type == typemark) break;
    }
    opernest = nest;
}

/* closefile */

void opclosefile(void)
{   struct object *token1;
    struct file *file;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typefile) error(errtypecheck);
    file = filecheck(token1, (openread | openwrite));
    if (file == NULL) error(errioerror);
    if (file->emode > 0)
    {   file->emode = -1;
        file->uflg = 0;
    }
    else
        fileclose(token1);
    opernest--;
}

/* copy */

void opcopy(void)
{   struct object *token1, *token2;
    struct dictionary *dict1, *dict2;
    int num;
    if (opernest < 1) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    if      (token2->type == typeint)
    {   num = token2->value.ival;
        if (num < 0 || num > opernest - 1) error(errrangecheck);
        if (opernest + num - 1 > operstacksize) error(errstackoverflow);
        arraycopy(token2, token2 - num, num);
        opernest += num - 1;
    }
    else
    {   if (opernest < 1) error(errstackunderflow);
        token1 = token2 - 1;
        if      (token1->type == typestring)
        {   if (token2->type != typestring) error(errtypecheck);
            if ((token1->flags & flagrprot) || (token2->flags & flagwprot))
                error(errinvalidaccess);
            if (token1->length > token2->length) error(errrangecheck);
            memcpy(vmsptr(token2->value.vref),
                   vmsptr(token1->value.vref), token1->length);
            token1->flags = token2->flags;
            token1->value.vref = token2->value.vref;
        }
        else if (token1->type == typearray)
        {   if (token2->type != typearray) error(errtypecheck);
            if ((token1->flags & flagrprot) || (token2->flags & flagwprot))
                error(errinvalidaccess);
            if (token1->length > token2->length) error(errrangecheck);
            arraysave(token2->value.vref, token2->length);
            arraycopy(vmaptr(token2->value.vref),
                      vmaptr(token1->value.vref), token1->length);
            token1->flags = token2->flags;
            token1->value.vref = token2->value.vref;
        }
        else if (token1->type == typepacked)
        {   if (token2->type != typearray) error(errtypecheck);
            if ((token1->flags & flagrprot) || (token2->flags & flagwprot))
                error(errinvalidaccess);
            if (token1->length > token2->length) error(errrangecheck);
            arraysave(token2->value.vref, token2->length);
            arrayunpk(vmaptr(token2->value.vref),
                      vmsptr(token1->value.vref), token1->length);
            token1->type = typearray;
            token1->flags = token2->flags;
            token1->value.vref = token2->value.vref;
        }
        else if (token1->type == typedict)
        {   if (token2->type != typedict) error(errtypecheck);
            dict2 = vmdptr(token2->value.vref);
            dict1 = vmdptr(token1->value.vref);
            if (dict1->flags & flagrprot) error(errinvalidaccess);
            if (dict1->full > dict2->size || dict2->full != 0)
                error(errrangecheck);
            num = dict1->slots;
            while (num--)
                if (dict1->entries[num].key.type != 0)
                    dictput(token2->value.vref, &dict1->entries[num].key,
                                                &dict1->entries[num].val);
            dict2->flags = dict1->flags;
            token1->value.vref = token2->value.vref;
        }
        else
            error(errtypecheck);
        opernest--;
    }
}

/* cos */

void opcos(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type == typereal)
        token.value.rval = (float) cos((double) token.value.rval * degtorad);
    else
        error(errtypecheck);
    *token1 = token;
}

/* count */

void opcount(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = opernest;
    operstack[opernest++] = token;
}

/* countdictstack */

void opcountdictstack(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = dictnest;
    operstack[opernest++] = token;
}

/* countexecstack */

void opcountexecstack(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = execnest - istate.execbase;
    operstack[opernest++] = token;
}

/* counttomark */

void opcounttomark(void)
{   struct object token, *token1, *token2;
    int nest = opernest;
    if (nest == operstacksize) error(errstackoverflow);
    token1 = &operstack[nest];
    token2 = token1;
    for (;;)
    {    if (nest == 0) error(errunmatchedmark);
         if ((--token1)->type == typemark) break;
         nest--;
    }
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = opernest - nest;
    *token2 = token;
    opernest++;
}

/* currentdict */

void opcurrentdict(void)
{   if (opernest == operstacksize) error(errstackoverflow);
    operstack[opernest++] = dictstack[dictnest - 1];
}

/* currentfile */

void opcurrentfile(void)
{   struct object *token1;
    int nest;
    if (opernest == operstacksize) error(errstackoverflow);
    nest = execnest;
    token1 = &execstack[execnest];
    while (nest)
    {   nest--;
        token1--;
        if (token1->type == typefile)
        {   operstack[opernest++] = *token1;
            return;
        }
    }
    error(errundefinedresult);
}

/* currentpacking */

void opcurrentpacking(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = packing;
    operstack[opernest++] = token;
}

/* cvi */

void opcvi(void)
{   struct object token, *token1;
    char *sptr;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token = *token1;
    if (token.type == typestring)
    {   if (token1->flags & flagrprot) error(errinvalidaccess);
        if (token.length > namebufsize) error(errlimitcheck);
        length = 0;
        sptr = vmsptr(token.value.vref);
        while (length < token.length)
            if ((namebuf[length++] = *sptr++) == ' ') error(errsyntaxerror);
        namebuf[length] = ' ';
        if (!numtoken(&token, namebuf)) error(errsyntaxerror);
    }
    if (token.type == typereal)
    {   token.type = typeint;
        if (token.value.rval > 0x7fffffff || token.value.rval < 0x80000000)
            error(errrangecheck);
        token.value.ival = itrunc(token.value.rval);
    }
    if (token.type != typeint)
        error(errtypecheck);
    *token1 = token;
}

/* cvlit */

void opcvlit(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token1->flags &= ~flagexec;
}

/* cvn */

void opcvn(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    nametoken(&token, vmsptr(token1->value.vref), token1->length,
                      token1->flags & flagexec);
    *token1 = token;
}

/* cvr */

void opcvr(void)
{   struct object token, *token1;
    char *sptr;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token = *token1;
    if (token.type == typestring)
    {   if (token.flags & flagrprot) error(errinvalidaccess);
        if (token.length > namebufsize) error(errlimitcheck);
        length = 0;
        sptr = vmsptr(token.value.vref);
        while (length < token.length)
            if ((namebuf[length++] = *sptr++) == ' ') error(errsyntaxerror);
        namebuf[length] = ' ';
        if (!numtoken(&token, namebuf)) error(errsyntaxerror);
    }
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type != typereal)
        error(errtypecheck);
    *token1 = token;
}

/* cvrs */

void opcvrs(void)
{   struct object token, *token1, *token2, *token3;
    char *sptr;
    unsigned int num, dig, base;
    int length;
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token2->type != typeint) error(errtypecheck);
    base = token2->value.ival;
    if (base < 2 || base > 36) error(errrangecheck);
    if (token3->type != typestring) error(errtypecheck);
    if (token3->flags & flagwprot) error(errinvalidaccess);
    token = *token3;
    sptr = vmsptr(token3->value.vref);
    if (base == 10)
    {   if (token1->type != typeint && token1->type != typereal)
            error(errtypecheck);
        token.length = cvstring(token1, sptr, token3->length);
    }
    else
    {   if      (token1->type == typeint)
        {   num = token1->value.ival;
        }
        else if (token1->type == typereal)
        {   if (token.value.rval > 0x7fffffff ||
                token.value.rval < 0x80000000)
                error(errrangecheck);
            num = itrunc(token1->value.rval);
        }
        else
            error(errtypecheck);
        length = 0;
        do
        {   dig = num % base;
            num = num / base;
            dig += (dig < 10) ? '0' : 'A' - 10;
            namebuf[length] = dig;
            length++;
        }   while (num != 0);
        if (length > token3->length) error(errrangecheck);
        token.length = length;
        while (length) *sptr++ = namebuf[--length]; 
    }
    *token1 = token;
    opernest -= 2;
}

/* cvs */

void opcvs(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagwprot) error(errinvalidaccess);
    token = *token2;
    token.length =
        cvstring(token1, vmsptr(token2->value.vref), token2->length);
    *token1 = token;
    opernest -= 1;
}

/* cvx */

void opcvx(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    switch (token1->type)
    {   case typenull:
        case typeoper:
        case typename:
        case typefile:
        case typearray:
        case typepacked:
        case typestring:
            token1->flags |= flagexec;
    }
}

/* def */

void opdef(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    dictput(dictstack[dictnest - 1].value.vref, token1, token2);
    opernest -= 2;
}

/* dict */

void opdict(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    dicttoken(&token, token1->value.ival);
    *token1 = token;
}

/* dictstack */

void opdictstack(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    if (token1->length < dictnest) error(errrangecheck);
    arraysave(token1->value.vref, token1->length);
    arraycopy(vmaptr(token1->value.vref), dictstack, dictnest);
    token1->length = dictnest;
}

/* div */

void opdiv(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    else
        if (token.type != typereal) error(errtypecheck);
    if (token2->type == typeint)
        token.value.rval /= token2->value.ival;
    else
    {   if (token2->type != typereal) error(errtypecheck);
        token.value.rval /= token2->value.rval;
    }
    *token1 = token;
    opernest--;
}

/* dup */

void opdup(void)
{   if (opernest < 1) error(errstackunderflow);
    if (opernest == operstacksize) error(errstackoverflow);
    operstack[opernest] = operstack[opernest - 1];
    opernest++;
}

/* eexec */

void opeexec(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typefile) error(errtypecheck);
    if (!equal(&execstack[execnest - 1], token1)) error(errundefined);
    if (dictnest == dictstacksize) error(errdictstackoverflow);
    if (execnest == execstacksize) error(errexecstackoverflow);
    fileeinit(token1);
    dictstack[dictnest++] = dictstack[0];
    token1->flags |= flagexec;
    execstack[execnest++] = *token1;
    opernest--;
}

/* end */

void opend(void)
{    if (dictnest < 3) error(errdictstackunderflow);
     dictnest--;
}

/* eq */

void opeq(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = equal(token1, token2);
    *token1 = token;
    opernest--;
}

/* exch */

void opexch(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token = *token1;
    *token1 = *token2;
    *token2 = token;
}

/* exec */

void opexec(void)
{   if (opernest < 1) error(errstackunderflow);
    if (execnest == execstacksize) error(errexecstackoverflow);
    execstack[execnest++] = operstack[--opernest];
}

/* execstack */

void opexecstack(void)
{   struct object *token1, *aptr;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    length = execnest - istate.execbase;
    if (token1->length < length) error(errrangecheck);
    if (opernest == operstacksize) error(errstackoverflow);
    aptr = vmaptr(token1->value.vref);
    arraysave(token1->value.vref, token1->length);
    arraycopy(aptr, &execstack[istate.execbase], length);
    token1->length = length;
    while (length--)
    {   aptr->flags &= ~(flagctrl | flagloop | flagrun | flagstop);
        if (aptr->type == typedict) aptr->length = 0;
    }
}

/* executeonly */

void opexecuteonly(void)
{   struct object *token1;
    int type;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    type = token1->type;
    if      (type == typefile || type == typestring ||
             type == typearray || type == typepacked)
    {   if (token1->flags & flagxprot) error(errinvalidaccess);
        token1->flags |= (flagwprot | flagrprot);
    }
    else
        error(errtypecheck);
}

/* exit */

void opexit(void)
{   struct object *token1;
    int nest;
    nest = execnest;
    while (nest >= istate.execbase)
    {   token1 = &execstack[nest - 1];
        if (token1->flags & (flagrun | flagstop)) break;
        if (token1->flags & flagloop)
        {   execnest = nest - token1->length;
            return;
        }
        nest--;
    }
    error(errinvalidexit);
}

/* exp */

void opexp(void)
{   struct object token, *token1, *token2;
    float flt2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type != typereal) error(errtypecheck);
    if      (token2->type == typeint)
        flt2 = token2->value.ival;
    else if (token2->type == typereal)
        flt2 = token2->value.rval;
    else
        error(errtypecheck);
    token.value.rval = (float) pow((double) token.value.rval, (double) flt2);
    *token1 = token;
    opernest--;
}

/* file */

void opfile(void)
{   struct object token, *token1, *token2;
    int ch, open;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    if (token2->length == 1)
        ch = *vmsptr(token2->value.vref);
    else
        ch = -1;
    if      (ch == 'r')
        open = openread;
    else if (ch == 'w')
        open = openwrite;
    else
        error(errrangecheck);
    fileopen(&token, open, vmsptr(token1->value.vref), token1->length);
    *token1 = token;
    opernest--;
}

/* floor */

void opfloor(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type == typeint)
        return;
    else if (token1->type == typereal)
        token1->value.rval = (float) floor((double) token1->value.rval);
    else
        error(errtypecheck);
}

/* flush */

void opflush(void)
{   if (sstdout)
        if (fflush(sstdout) == EOF) error(errioerror);
}

/* flushfile */

void opflushfile(void)
{   struct object *token1;
    struct file *file;
    FILE *fptr;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typefile) error(errtypecheck);
    file = filecheck(token1, openread | openwrite);
    if (file == NULL) error(errioerror);
    fptr = file->fptr;
    if (file->open == openread)
    {   file->stype = 0;
        file->slen = 0x80000000;
        while (getc(fptr) != EOF) continue;
    }
    else
        fflush(fptr);
    if (ferror(fptr)) error(errioerror);
    opernest--;
}

/* fontfile */

void opfontfile(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    fileopen(&token, (openread | openfont),
             vmsptr(token1->value.vref), token1->length);
    *token1 = token;
}

/* for */

void opfor(void)
{   struct object token, *token1, *token2, *token3, *token4, *tokenx;
    int type;
    if (currtoken->flags & flagctrl)
    {   token4 = &execstack[execnest - 2];
        token3 = token4 - 1;
        token2 = token3 - 1;
        token1 = token2 - 1;
        token = *token1;
        if (token1->type == typeint)
        {   if (token2->value.ival >= 0)
            {   if (token1->value.ival > token3->value.ival)
                {   execnest -= 5;
                    return;
                }
                token1->value.ival += token2->value.ival;
            }
            else
            {   if (token1->value.ival < token3->value.ival)
                {   execnest -= 5;
                    return;
                }
                token1->value.ival += token2->value.ival;
            }
        }
        else
        {   if (token2->value.rval >= 0)
            {   if (token1->value.rval > token3->value.rval)
                {   execnest -= 5;
                    return;
                }
                token1->value.rval += token2->value.rval;
            }
            else
            {   if (token1->value.rval < token3->value.rval)
                {   execnest -= 5;
                    return;
                }
                token1->value.rval += token2->value.rval;
            }
        }
        if (opernest == operstacksize) error(errstackoverflow);
        operstack[opernest++] = token;
        token4[2] = *token4;
        execnest++;
    }
    else
    {   if (opernest < 4) error(errstackunderflow);
        token4 = &operstack[opernest - 1];
        token3 = token4 - 1;
        token2 = token3 - 1;
        token1 = token2 - 1;
        type = typeint;
        if (token1->type != typeint)
        {   type = typereal;
            if (token1->type != typereal) error(errtypecheck);
        }
        if (token2->type != typeint)
        {   type = typereal;
            if (token2->type != typereal) error(errtypecheck);
        }
        if (token3->type != typeint)
        {   type = typereal;
            if (token3->type != typereal) error(errtypecheck);
        }
        if (token4->type != typearray && token4->type != typepacked)
            error(errtypecheck);
        if (execnest + 6 > execstacksize) error(errexecstackoverflow);
        token = *token1;
        if (token.type != type)
        {   token.type = typereal;
            token.value.rval = token.value.ival;
        }
        tokenx = &execstack[execnest];
        tokenx[0] = token;
        token = *token2;
        if (token.type != type)
        {   token.type = typereal;
            token.value.rval = token.value.ival;
        }
        tokenx[1] = token;
        token = *token3;
        if (token.type != type)
        {   token.type = typereal;
            token.value.rval = token.value.ival;
        }
        tokenx[2] = token;
        tokenx[3] = *token4;
        token = *currtoken;
        token.flags &= ~flagexec;
        token.flags |= flagctrl | flagloop;
        token.length = 5;
        tokenx[4] = token;
        execnest += 5;
        opernest -= 4;
    }
}

/* forall */

void opforall(void)
{   struct object token, *token1, *token2, *tokenx;
    struct dictionary *dict1;
    int length;
    if (currtoken->flags & flagctrl)
    {   token2 = &execstack[execnest - 2];
        token1 = token2 - 1;
        if      (token1->type == typestring)
        {   if (token1->length == 0)
            {   execnest -= 3;
                return;
            }
            token1->length--;
            if (opernest == operstacksize) error(errstackoverflow);
            token.type = typeint;
            token.flags = 0;
            token.length = 0;
            token.value.ival =
                *((unsigned char *) vmsptr(token1->value.vref));
            token1->value.vref++;
            operstack[opernest++] = token;
        }
        else if (token1->type == typearray)
        {   if (token1->length == 0)
            {   execnest -= 3;
                return;
            }
            token1->length--;
            if (opernest == operstacksize) error(errstackoverflow);
            operstack[opernest++] = *vmaptr(token1->value.vref);
            token1->value.vref += sizeof (struct object);
        }
        else if (token1->type == typepacked)
        {   if (token1->length == 0)
            {   execnest -= 3;
                return;
            }
            token1->length--;
            if (opernest == operstacksize) error(errstackoverflow);
            token1->value.vref +=
                unpack(&operstack[opernest++], vmsptr(token1->value.vref));
        }
        else if (token1->type == typedict)
        {   length = token1->length;
            dict1 = vmdptr(token1->value.vref);
            for (;;)
            {   if (length == dict1->slots)
                {   execnest -= 3;
                    return;
                }
                if (dict1->entries[length].key.type != 0) break;
                length++;
            }
            token1->length = length + 1;
            if (opernest + 2 > operstacksize) error(errstackoverflow);
            operstack[opernest++] = dict1->entries[length].key;
            operstack[opernest++] = dict1->entries[length].val;
        }
        token2[2] = *token2;
        execnest++;
    }
    else
    {   if (opernest < 2) error(errstackunderflow);
        token2 = &operstack[opernest - 1];
        token1 = token2 - 1;
        if (token2->type != typearray && token2->type != typepacked)
            error(errtypecheck);
        if      (token1->type == typestring ||
                 token1->type == typearray || token1->type == typepacked)
        {   if (token1->flags & flagrprot) error(errinvalidaccess);
        }
        else if (token1->type == typedict)
        {   dict1 = vmdptr(token1->value.vref);
            if (dict1->flags & flagrprot) error(errinvalidaccess);
        }
        else
            error(errtypecheck);
        if (execnest + 4 > execstacksize) error(errexecstackoverflow);
        tokenx = &execstack[execnest];
        tokenx[0] = *token1;
        tokenx[1] = *token2;
        token = *currtoken;
        token.flags &= ~flagexec;
        token.flags |= flagctrl | flagloop;
        token.length = 3;
        tokenx[2] = token;
        execnest += 3;
        opernest -= 2;
    }
}

/* ge */

void opge(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = (compare(token1, token2) >= 0);
    *token1 = token;
    opernest--;
}

/* get */

void opget(void)
{   struct object *token1, *token2;
    char *sptr;
    int num;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type == typedict)
    {   if (!(dictget(token1->value.vref, token2, token1, flagrprot)))
            error(errundefined);
    }
    else
    {   if (token2->type != typeint) error(errtypecheck);
        num = token2->value.ival;
        if      (token1->type == typestring)
        {   if (token1->flags & flagrprot) error(errinvalidaccess);
            if (num < 0 || num >= token1->length) error(errrangecheck);
            token1->type = typeint;
            token1->flags = 0;
            token1->length = 0;
            token1->value.ival =
                ((unsigned char *)vmsptr(token1->value.vref))[num];
        }
        else if (token1->type == typearray)
        {   if (token1->flags & flagrprot) error(errinvalidaccess);
            if (num < 0 || num >= token1->length) error(errrangecheck);
            *token1 = vmaptr(token1->value.vref)[num];
        }
        else if (token1->type == typepacked)
        {   if (token1->flags & flagrprot) error(errinvalidaccess);
            if (num < 0 || num >= token1->length) error(errrangecheck);
            sptr = vmsptr(token1->value.vref);
            for (;;)
            {   sptr += unpack(token1, sptr);
                if (num == 0) break;
                num--;
            }
        }
        else
            error(errtypecheck);
    }
    opernest--;
}

/* getinterval */

void opgetinterval(void)
{   struct object token, *token1, *token2, *token3;
    char *sptr;
    int num, len;
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token2->type != typeint || token3->type!=typeint)
        error(errtypecheck);
    num = token2->value.ival;
    len = token3->value.ival;
    if      (token1->type == typestring)
    {   if (token1->flags & flagrprot) error(errinvalidaccess);
        if (num < 0 || num > token1->length) error(errrangecheck);
        if (len < 0 || num + len > token1->length) error(errrangecheck);
        token1->length = len;
        token1->value.vref += num;
    }
    else if (token1->type == typearray)
    {   if (token1->flags & flagrprot) error(errinvalidaccess);
        if (num < 0 || num > token1->length) error(errrangecheck);
        if (len < 0 || num + len > token1->length) error(errrangecheck);
        token1->length = len;
        token1->value.vref += num * sizeof (struct object);
    }
    else if (token1->type == typepacked)
    {   if (token1->flags & flagrprot) error(errinvalidaccess);
        if (num < 0 || num > token1->length) error(errrangecheck);
        if (len < 0 || num + len > token1->length) error(errrangecheck);
        token1->length = len;
        len = 0;
        sptr = vmsptr(token1->value.vref);
        while (num--)
            len += unpack(&token, sptr + len);
        token1->value.vref += len;
    }
    else
        error(errtypecheck);
    opernest -= 2;
}

/* gt */

void opgt(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = (compare(token1, token2) > 0);
    *token1 = token;
    opernest--;
}

/* idiv */

void opidiv(void)
{   struct object *token1, *token2;
    int num1, num2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typeint || token2->type != typeint)
        error(errtypecheck);
    num1 = token1->value.ival;
    num2 = token2->value.ival;
    if (num2 == 0) error(errundefinedresult);
    token1->value.ival = num1 / num2;
    opernest--;
}

/* if */

void opif(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typebool) error(errtypecheck);
    if (token2->type != typearray && token2->type != typepacked)
        error(errtypecheck);
    if (execnest == execstacksize) error(errexecstackoverflow);
    if (token1->value.ival) execstack[execnest++] = *token2;
    opernest -= 2;
}

/* ifelse */

void opifelse(void)
{   struct object *token1, *token2, *token3;
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token1->type != typebool) error(errtypecheck);
    if (token2->type != typearray && token2->type != typepacked)
        error(errtypecheck);
    if (token2->type != typearray && token2->type != typepacked)
        error(errtypecheck);
    if (execnest == execstacksize) error(errexecstackoverflow);
    if (token1->value.ival)
        execstack[execnest++] = *token2;
    else
        execstack[execnest++] = *token3;
    opernest -= 3;
}

/* Initialise the operators (1) */

void initop1(void)
{   systemop(operror,            ".error");
    systemop(ophandleerror,      ".handleerror");
    systemop(opmark,             "mark");
    systemop(opmark,             "[");
    systemop(opkram,             "]");
    systemop(opequals,           "=");
    systemop(opeqeq,             "==");
    systemop(opabs,              "abs");
    systemop(opadd,              "add");
    systemop(opaload,            "aload");
    systemop(opanchorsearch,     "anchorsearch");
    systemop(opand,              "and");
    systemop(oparray,            "array");
    systemop(opastore,           "astore");
    systemop(opatan,             "atan");
    systemop(opbegin,            "begin");
    systemop(opbind,             "bind");
    systemop(opbitshift,         "bitshift");
    systemop(opbytesavailable,   "bytesavailable");
    systemop(opceiling,          "ceiling");
    systemop(opclear,            "clear");
    systemop(opcleardictstack,   "cleardictstack");
    systemop(opcleartomark,      "cleartomark");
    systemop(opclosefile,        "closefile");
    systemop(opcopy,             "copy");
    systemop(opcount,            "count");
    systemop(opcountdictstack,   "countdictstack");
    systemop(opcountexecstack,   "countexecstack");
    systemop(opcounttomark,      "counttomark");
    systemop(opcos,              "cos");
    systemop(opcurrentdict,      "currentdict");
    systemop(opcurrentfile,      "currentfile");
    systemop(opcurrentpacking,   "currentpacking");
    systemop(opcvi,              "cvi");
    systemop(opcvlit,            "cvlit");
    systemop(opcvn,              "cvn");
    systemop(opcvr,              "cvr");
    systemop(opcvrs,             "cvrs");
    systemop(opcvs,              "cvs");
    systemop(opcvx,              "cvx");
    systemop(opdef,              "def");
    systemop(opdict,             "dict");
    systemop(opdictstack,        "dictstack");
    systemop(opdiv,              "div");
    systemop(opdup,              "dup");
    systemop(opeexec,            "eexec");
    systemop(opend,              "end");
    systemop(opeq,               "eq");
    systemop(opexch,             "exch");
    systemop(opexec,             "exec");
    systemop(opexit,             "exit");
    systemop(opexecstack,        "execstack");
    systemop(opexecuteonly,      "executeonly");
    systemop(opexp,              "exp");
    systemop(opfile,             "file");
    systemop(opfloor,            "floor");
    systemop(opflush,            "flush");
    systemop(opflushfile,        "flushfile");
    systemop(opfontfile,         "fontfile");
    systemop(opfor,              "for");
    systemop(opforall,           "forall");
    systemop(opge,               "ge");
    systemop(opget,              "get");
    systemop(opgetinterval,      "getinterval");
    systemop(opgt,               "gt");
    systemop(opidiv,             "idiv");
    systemop(opif,               "if");
    systemop(opifelse,           "ifelse");
}

/* End of file "postop1.c" */
