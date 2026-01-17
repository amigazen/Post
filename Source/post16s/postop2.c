/* PostScript interpreter file "postop2.c" - operators (2) */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* index */
    
void opindex(void)
{   struct object *token1;
    int num;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    num = token1->value.ival;
    if (num < 0 || num > opernest - 2) error(errrangecheck);
    *token1 = *(token1 - num - 1);
}

/* known */

void opknown(void)
{   struct object token, *token1, *token2;
    int bool;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typedict) error(errtypecheck);
    bool = dictget(token1->value.vref, token2, &token, flagrprot);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = bool;
    *token1 = token;
    opernest -= 1;
}

/* le */

void ople(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = (compare(token1, token2) <= 0);
    *token1 = token;
    opernest--;
}

/* length */

void oplength(void)
{   struct object token, *token1;
    struct dictionary *dict;
    struct name *name;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typearray || token1->type == typepacked ||
             token1->type == typestring)
    {   if (token1->flags & flagrprot) error(errinvalidaccess);
        length = token1->length;
    }
    else if (token1->type == typedict)
    {   dict = vmdptr(token1->value.vref);
        if (dict->flags & flagrprot) error(errinvalidaccess);
        length = dict->full;
    }
    else if (token1->type == typename)
    {   name = vmnptr(token1->value.vref);
        length = name->length;
    }
    else
        error(errtypecheck);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = length;
    *token1 = token;
}

/* ln */

void opln(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type == typereal)
        token.value.rval = (float) log((double) token.value.rval);
    else
        error(errtypecheck);
    *token1 = token;
}

/* load */

void opload(void)
{   struct object *token;
    if (opernest < 1) error(errstackunderflow);
    token = &operstack[opernest - 1];
    if (dictfind(token, token) == -1) error(errundefined);
}

/* log */

void oplog(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type == typereal)
        token.value.rval = (float) log10((double) token.value.rval);
    else
        error(errtypecheck);
    *token1 = token;
}

/* loop */

void oploop(void)
{   struct object token, *token1, *tokenx;
    if (currtoken->flags & flagctrl)
    {   token1 = &execstack[execnest - 2];
        token1[2] = *token1;
        execnest++;
    }
    else
    {   if (opernest < 1) error(errstackunderflow);
        token1 = &operstack[opernest - 1];
        if (token1->type != typearray && token1->type != typepacked)
            error(errtypecheck);
        if (execnest + 3 > execstacksize) error(errexecstackoverflow);
        tokenx = &execstack[execnest];
        *tokenx++ = *token1;
        token = *currtoken;
        token.flags &= ~flagexec;
        token.flags |= flagctrl | flagloop;
        token.length = 2;
        *tokenx = token;
        execnest += 2;
        opernest -= 1;
    }
}

/* lt */

void oplt(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = (compare(token1, token2) < 0);
    *token1 = token;
    opernest--;
}

/* maxlength */

void opmaxlength(void)
{   struct object token, *token1;
    struct dictionary *dict;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typedict) error(errtypecheck);
    dict = vmdptr(token1->value.vref);
    if (dict->flags & flagrprot) error(errinvalidaccess);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = dict->size;
    *token1 = token;
}

/* mod */

void opmod(void)
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
    token1->value.ival = num1 % num2;
    opernest--;
}

/* mul */

void opmul(void)
{   struct object token, *token1, *token2;
    int num1, num2, tmp1, tmp2;
    unsigned int neg;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type == typeint && token2->type == typeint)
    {   num1 = token1->value.ival;
        num2 = token2->value.ival;
        neg = 0x7fffffff;
        if (num1 < 0)
        {   neg = ~neg;
            num1 = -num1;
        }
        if (num2 < 0)
        {   neg = ~neg;
            num2 = -num2;
        }
        tmp1 = (unsigned) num1 >> 16;
        tmp2 = (unsigned) num2 >> 16;
        num1 &= 0xffff;
        num2 &= 0xffff;
        if (tmp1 == 0)
        {   if (tmp2 == 0)
            {   num1 *= num2;
                if ((unsigned) num1 <= neg)
                {   token1->value.ival = (neg == 0x7fffffff) ? num1 : -num1;
                    opernest--;
                    return;
                }
            }
            else
            {   tmp2 *= num1;
                num1 *= num2;
                tmp1 = tmp2 + ((unsigned) num1 >> 16);
                num1 += tmp2 << 16;
                if ((tmp1 & 0xffff0000) == 0 && (unsigned) num1 <= neg)
                {   token1->value.ival = (neg == 0x7fffffff) ? num1 : -num1;
                    opernest--;
                    return;
                }
            }
        }
        else
            if (tmp2 == 0)
            {   tmp1 *= num2;
                num2 *= num1;
                tmp2 = tmp1 + ((unsigned) num2 >> 16);
                num2 += tmp1 << 16;
                if ((tmp2 & 0xffff0000) == 0 && (unsigned) num2 <= neg)
                {   token1->value.ival = (neg == 0x7fffffff) ? num2 : -num2;
                    opernest--;
                    return;
                }
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
        token.value.rval *= token2->value.ival;
    else
    {   if (token2->type != typereal) error(errtypecheck);
        token.value.rval *= token2->value.rval;
    }
    *token1 = token;
    opernest--;
}

/* ne */

void opne(void)
{   struct object token, *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = !equal(token1, token2);
    *token1 = token;
    opernest--;
}

/* neg */

void opneg(void)
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
        else
            token1->value.ival = -num1;
    }
    else if (token1->type == typereal)
        token1->value.rval = -token1->value.rval;
    else
        error(errtypecheck);
}

/* noaccess */

void opnoaccess(void)
{   struct object *token1;
    int type;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    type = token1->type;
    if      (type == typefile || type == typestring ||
             type == typearray || type == typepacked)
        token1->flags |= (flagwprot | flagrprot | flagxprot);
    else if (type == typedict)
        vmdptr(token1->value.vref)->flags |= (flagwprot | flagrprot);
    else
        error(errtypecheck);
}

/* not */

void opnot(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typebool)
        token1->value.ival = !token1->value.ival;
    else if (token1->type == typeint)
        token1->value.ival = ~token1->value.ival;
    else
        error(errtypecheck);
}

/* null */

void opnull(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typenull;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    operstack[opernest++] = token;
}

/* or */

void opor(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if ((token1->type == typebool && token2->type == typebool) ||
        (token1->type == typeint  && token2->type == typeint))
        token1->value.ival |= token2->value.ival;
    else
        error(errtypecheck);
    opernest--;
}

/* packedarray */

void oppackedarray(void)
{   struct object token, *token1, *aptr;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    length = token1->value.ival;
    if (length < 0) error(errrangecheck);
    if (opernest < length + 1) error(errstackunderflow);
    aptr = token1 - length;
    token.type = typepacked;
    token.flags = flagwprot;
    token.length = length;
    token.value.vref = arraypack(aptr, length);
    *aptr = token;
    opernest -= length;
}

/* pop */

void oppop(void)
{   if (opernest < 1) error(errstackunderflow);
    opernest -= 1;
}

/* print */

void opprint(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (sstdout)
        putmem(sstdout, vmsptr(token1->value.vref), token1->length);
    opernest--;
}

/* prompts */

void opprompts(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (token1->length > promptsize) error(errrangecheck);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    if (token2->length > promptsize) error(errrangecheck);
    memcpy(prompt1, vmsptr(token1->value.vref), token1->length);
    prompt1[token1->length] = 0;
    memcpy(prompt2, vmsptr(token2->value.vref), token2->length);
    prompt2[token2->length] = 0;
    opernest -= 2;
}

/* pstack */

void oppstack(void)
{   int nest = opernest;
    if (sstdout)
        while (nest)
        {   printeqeq(sstdout, &operstack[--nest], 0, 0);
            putch(sstdout, '\n');
        }
}

/* put */

void opput(void)
{   struct object *token1, *token2, *token3;
    int num;
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token1->type == typedict)
        dictput(token1->value.vref, token2, token3);
    else
    {   if (token2->type != typeint) error(errtypecheck);
        num = token2->value.ival;
        if      (token1->type == typestring)
        {   if (token3->type != typeint) error(errtypecheck);
            if (token1->flags & flagwprot) error(errinvalidaccess);
            if (num < 0 || num >= token1->length) error(errrangecheck);
            vmsptr(token1->value.vref)[num] = token3->value.ival;
        }
        else if (token1->type == typearray)
        {   if (token1->flags & flagwprot) error(errinvalidaccess);
            if (num < 0 || num >= token1->length) error(errrangecheck);
            arraysave(token1->value.vref, token1->length);
            vmaptr(token1->value.vref)[num] = *token3;
        }
        else
            error(errtypecheck);
    }
    opernest -= 3;
}

/* putinterval */

void opputinterval(void)
{   struct object *token1, *token2, *token3;
    int num, len;
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token2->type != typeint) error(errtypecheck);
    num = token2->value.ival;
    if (token3->type != token1->type) error(errtypecheck);
    len = token3->length;
    if      (token1->type == typestring)
    {   if (token1->flags & flagwprot) error(errinvalidaccess);
        if (token3->flags & flagrprot) error(errinvalidaccess);
        if (num < 0 || num > token1->length) error(errrangecheck);
        if (len < 0 || num + len > token1->length) error(errrangecheck);
        memcpy(vmsptr(token1->value.vref) + num,
               vmsptr(token3->value.vref), len);
    }
    else if (token1->type == typearray)
    {   if (token1->flags & flagwprot) error(errinvalidaccess);
        if (token3->flags & flagrprot) error(errinvalidaccess);
        if (num < 0 || num > token1->length) error(errrangecheck);
        if (len < 0 || num + len > token1->length) error(errrangecheck);
        arraysave(token1->value.vref, token1->length);
        arraycopy(vmaptr(token1->value.vref) + num,
                  vmaptr(token3->value.vref), len);
    }
    else
        error(errtypecheck);
    opernest -= 3;
}

/* quit */

void opquit(void)
{   execnest = 0;
    errorjmp(0, 1);
}

/* rand */

void oprand(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    random *= 0x41c64e6d;
    random += 0x3039;
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = random >> 1;
    operstack[opernest++] = token;
}

/* rcheck */

void oprcheck(void)
{   struct object token, *token1;
    int type, bool;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    type = token1->type;
    if      (type == typefile || type == typestring ||
             type == typearray || type == typepacked)
        bool = ((token1->flags & flagrprot) == 0);
    else if (type == typedict)
        bool = ((vmdptr(token1->value.vref)->flags & flagrprot) == 0);
    else
        error(errtypecheck);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = bool;
    *token1 = token;
}

/* read */

void opread(void)
{   struct object token, *token1;
    int ch;
    if (opernest < 1) error(errstackunderflow);
    if (opernest == operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (filecheck(token1, openread) == NULL) error(errioerror);
    ch = readch(token1, -2);
    token.flags = 0;
    token.length = 0;
    if (ch == EOF)
    {   fileclose(token1);
        token.value.ival = 0;
    }
    else
    {   token.type = typeint;
        token.value.ival = ch;
        *token1++ = token;
        token.value.ival = 1;
        opernest++;
    }
    token.type = typebool;
    *token1 = token;
}

/* readhexstring */

void opreadhexstring(void)
{   struct object token, *token1, *token2;
    char *sptr;
    int ch, length, num, dig, sw;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (filecheck(token1, openread) == NULL) error(errioerror);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagwprot) error(errinvalidaccess);
    length = 0;
    sptr = vmsptr(token2->value.vref);
    ch = 0;
    sw = 1;
    while (length < token2->length)
    {   ch = readch(token1, -1);
        if (ch == EOF) break;
        dig = digitval(ch);
        if (dig >= 16) continue;
        if (sw)
        {   sw = 0;
            num = dig * 16;
        }
        else
        {   sw = 1;
            num += dig;
            *sptr++ = num;
            length++;
        }
    }
    token = *token2;
    token.length = length;
    *token1 = token;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = (ch != EOF);
    *token2 = token;
}

/* readline */

void opreadline(void)
{   struct object token, *token1, *token2;
    char *sptr;
    int ch, length;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (filecheck(token1, openread) == NULL) error(errioerror);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagwprot) error(errinvalidaccess);
    length = 0;
    sptr = vmsptr(token2->value.vref);
    ch = 0;
    for (;;)
    {   ch = readch(token1, -1);
        if (ch == '\n' || ch == EOF) break;
        if (length == token2->length) error(errrangecheck);
        *sptr++ = ch;
        length++;
    }
    token = *token2;
    token.length = length;
    *token1 = token;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = (ch != EOF);
    *token2 = token;
}

/* readonly */

void opreadonly(void)
{   struct object *token1;
    struct dictionary *dict;
    int type;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    type = token1->type;
    if      (type == typefile || type == typestring ||
             type == typearray || type == typepacked)
    {   if (token1->flags & flagrprot) error(errinvalidaccess);
        token1->flags |= flagwprot;
    }
    else if (type == typedict)
    {   dict = vmdptr(token1->value.vref);
        if (dict->flags & flagrprot) error(errinvalidaccess);
        dict->flags |= flagwprot;
    }
    else
        error(errtypecheck);
}

/* readstring */

void opreadstring(void)
{   struct object token, *token1, *token2;
    char *sptr;
    int ch, length;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (filecheck(token1, openread) == NULL) error(errioerror);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagwprot) error(errinvalidaccess);
    length = 0;
    sptr = vmsptr(token2->value.vref);
    ch = 0;
    while (length < token2->length)
    {   ch = readch(token1, -2);
        if (ch == EOF) break;
        *sptr++ = ch;
        length++;
    }
    token = *token2;
    token.length = length;
    *token1 = token;
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = (ch != EOF);
    *token2 = token;
}

/* repeat */

void oprepeat(void)
{   struct object token, *token1, *token2, *tokenx;
    if (currtoken->flags & flagctrl)
    {   token2 = &execstack[execnest - 2];
        token1 = token2 - 1;
        if (token1->value.ival <= 0)
        {   execnest -= 3;
            return;
        }
        token1->value.ival--;
        token2[2] = *token2;
        execnest++;
    }
    else
    {   if (opernest < 2) error(errstackunderflow);
        token2 = &operstack[opernest - 1];
        token1 = token2 - 1;
        if (token1->type != typeint) error(errtypecheck);
        if (token2->type != typearray && token2->type != typepacked)
            error(errtypecheck);
        if (execnest + 4 > execstacksize) error(errexecstackoverflow);
        tokenx = &execstack[execnest];
        *tokenx++ = *token1;
        *tokenx++ = *token2;
        token = *currtoken;
        token.flags &= ~flagexec;
        token.flags |= flagctrl | flagloop;
        token.length = 3;
        *tokenx = token;
        execnest += 3;
        opernest -= 2;
    }
}

/* resetfile */

void opresetfile(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typefile) error(errtypecheck);
    if (filecheck(token1, openread | openwrite) == NULL) error(errioerror);
    opernest--;
}

/* restore */

void oprestore(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typesave) error(errtypecheck);
    vmrest(token1->length, token1->value.ival);
    opernest--;
}

/* roll */
    
void oproll(void)
{   struct object *token1, *token2;
    int num, dir, shf;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typeint) error(errtypecheck);
    num = token1->value.ival;
    if (token2->type != typeint) error(errtypecheck);
    dir = token2->value.ival;
    if (num < 0 || num > opernest - 2) error(errrangecheck);
    dir = dir % num;
    shf = num - dir;
    if (opernest + shf - 2 > operstacksize) error(errstackoverflow);
    opernest -= 2;
    arraycopy(token1, token1 - num, shf);
    arraycopy(token1 - num, token1 - dir, num);
}

/* round */

void opround(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type == typeint)
        return;
    else if (token1->type == typereal)
        token1->value.rval = (float) floor((double) token1->value.rval + .5);
    else
        error(errtypecheck);
}

/* rrand */

void oprrand(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = random;
    operstack[opernest++] = token;
}

/* run */

void oprun(void)
{   struct object token, *token1, *tokenx;
    if (currtoken->flags & flagctrl)
        execnest -= 2;
    else
    {   if (opernest < 1) error(errstackunderflow);
        token1 = &operstack[opernest - 1];
        if (token1->type != typestring) error(errtypecheck);
        if (token1->flags & flagrprot) error(errinvalidaccess);
        if (execnest + 3 > execstacksize) error(errexecstackoverflow);
        tokenx = &execstack[execnest];
        fileopen(&token, openread, vmsptr(token1->value.vref),
                         token1->length);
        token.flags |= flagexec;
        tokenx[0] = token;
        tokenx[2] = token;
        token = *currtoken;
        token.flags &= ~flagexec;
        token.flags |= flagctrl | flagrun;
        token.length = 2;
        tokenx[1] = token;
        execnest += 3;
        opernest--;
    }
}

/* save */

void opsave(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    vmsave(&token);
    operstack[opernest++] = token;
}

/* search */

void opsearch(void)
{   struct object *token1, *token2;
    char *sptr1, *sptr2;
    int len1, len2, offs, bool;
    struct object token;
    if (opernest < 2) error(errstackunderflow);
    if (opernest + 2 > operstacksize) error(errstackoverflow);
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
    offs = 0;
    bool = 0;
    while (offs + len2 <= len1)
    {   if (memcmp(sptr1 + offs, sptr2, len2) == 0)
        {   bool = 1;
            break;
        }
        offs++;
    }
    if (bool)
    {   token = *token1;
        token.length = offs;
        *(token2 + 1) = token;
        token.length = len2;
        token.value.vref = token1->value.vref + offs;
        *token2 = token;
        token.length = len1 - offs - len2;
        token.value.vref = token1->value.vref + offs + len2;
        *token1 = token;
        token2 += 2;
        opernest += 2;
    }
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = bool;
    *token2 = token;
}

/* setpacking */

void opsetpacking(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typebool) error(errtypecheck);
    packing = token1->value.ival;
    opernest--;
}

/* sin */

void opsin(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type == typereal)
        token.value.rval = (float) sin((double) token.value.rval * degtorad);
    else
        error(errtypecheck);
    *token1 = token;
}

/* sqrt */

void opsqrt(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    token = *token1;
    if (token.type == typeint)
    {   token.type = typereal;
        token.value.rval = token.value.ival;
    }
    if (token.type == typereal)
    {   if (token.value.rval < 0.0) error(errrangecheck);
        token.value.rval = (float) sqrt((double) token.value.rval);
    }
    else
        error(errtypecheck);
    *token1 = token;
}

/* srand */

void opsrand(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errrangecheck);
    random = token1->value.ival;
    opernest--;
}

/* stack */

void opstack(void)
{   int nest = opernest;
    if (sstdout)
        while (nest)
        {   printequals(sstdout, &operstack[--nest]);
            putch(sstdout, '\n');
        }
}

/* status */

void opstatus(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typefile) error(errtypecheck);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival =
        (filecheck(token1, openread | openwrite) != NULL);
    *token1 = token;
}

/* stop */

void opstop(void)
{   stop();
    error(errinvalidstop);
}

/* stopped */

void opstopped(void)
{   struct object token, *token1, *tokenx;
    if (opernest == operstacksize) error(errstackoverflow);
    if (currtoken->flags & flagctrl)
    {   if (opernest == operstacksize) error(errstackoverflow);
        token.type = typebool;
        token.flags = 0;
        token.length = 0;
        token.value.ival = 0;
        operstack[opernest++] = token;
        execnest -= 2;
    }
    else
    {   if (opernest < 1) error(errstackunderflow);
        token1 = &operstack[opernest - 1];
        if (execnest + 3 > execstacksize) error(errexecstackoverflow);
        tokenx = &execstack[execnest];
        tokenx[0] = *token1;
        token = *currtoken;
        token.flags &= ~flagexec;
        token.flags |= flagctrl | flagstop;
        token.length = inest;
        tokenx[1] = token;
        tokenx[2] = *token1;
        execnest += 3;
        opernest--;
    }
}

/* store */

void opstore(void)
{   struct object token, *token1, *token2;
    int nest;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    nest = dictfind(token1, &token);
    if (nest == -1) nest = dictnest - 1;
    dictput(dictstack[nest].value.vref, token1, token2);
    opernest -= 2;
}

/* string */

void opstring(void)
{   struct object token, *token1;
    int length;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    length = token1->value.ival;
    if (length < 0 || length > 65535) error(errrangecheck);
    token.type = typestring;
    token.flags = 0;
    token.length = token1->value.ival;
    token.value.vref = vmalloc(length);
    *token1 = token;
}

/* sub */

void opsub(void)
{   struct object token, *token1, *token2;
    int num1, num2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type == typeint && token2->type == typeint)
    {   num1 = token1->value.ival;
        num2 = token2->value.ival;
        if      (num1 > 0 && num2 < 0)
        {   num1 -= num2;
            if (num1 > 0)
            {   token1->value.ival = num1;
                opernest--;
                return;
            }
        }
        else if (num1 < 0 && num2 > 0)
        {   num1 -= num2;
            if (num1 < 0)
            {   token1->value.ival = num1;
                opernest--;
                return;
            }
        }
        else
        {   num1 -= num2;
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
        token.value.rval -= token2->value.ival;
    else
    {   if (token2->type != typereal) error(errtypecheck);
        token.value.rval -= token2->value.rval;
    }
    *token1 = token;
    opernest--;
}

/* token */

void optoken(void)
{   struct object token, *token1;
    int bool;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if     (token1->type == typefile)
    {    if (opernest + 1 > operstacksize) error(errstackoverflow);
         bool = scantoken(&token, token1, -1);
         if (bool)
         {   *token1++ = token;
             opernest++;
         }
         else
             fileclose(token1);
    }
    else if (token1->type == typestring)
    {    if (opernest + 2 > operstacksize) error(errstackoverflow);
         bool = scantoken(&token, token1, -1);
         if (bool)
         {   token1++;
             *token1++ = token;
             opernest += 2;
         }
    }
    else
        error(errtypecheck);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = bool;
    *token1 = token;
}

/* truncate */

void optruncate(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type == typeint)
        return;
    else if (token1->type == typereal)
        if (token1->value.rval > 0.0)
            token1->value.rval = (float) floor((double) token1->value.rval);
        else
            token1->value.rval = (float) ceil((double) token1->value.rval);
    else
        error(errtypecheck);
}

/* type */

void optype(void)
{   struct object token, *token1;
    char *sptr;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    sptr = typetable[token1->type];
    nametoken(&token, sptr, -1, flagexec);
    *token1 = token;
}

/* usertime */

void opusertime(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = usertime();
    operstack[opernest++] = token;
}

/* vmhwm */

void opvmhwm(void)
{   struct object token, *token1;
    if (opernest + 2 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = vmhwm;
    token1[0] = token;
    token.value.ival = vmmax;
    token1[1] = token;
    opernest += 2;
    vmhwm = vmused;
}

/* vmstatus */

void opvmstatus(void)
{   struct object token, *token1;
    if (opernest + 3 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = vmnest;
    token1[0] = token;
    token.value.ival = vmused;
    token1[1] = token;
    token.value.ival = vmmax;
    token1[2] = token;
    opernest += 3;
}

/* wcheck */

void opwcheck(void)
{   struct object token, *token1;
    int type, bool;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    type = token1->type;
    if      (type == typefile || type == typestring ||
             type == typearray || type == typepacked)
        bool = ((token1->flags & flagwprot) == 0);
    else if (type == typedict)
        bool = ((vmdptr(token1->value.vref)->flags & flagwprot) == 0);
    else
        error(errtypecheck);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = bool;
    *token1 = token;
}

/* where */

void opwhere(void)
{   struct object token, *token1;
    int nest;
    if (opernest < 1) error(errstackunderflow);
    if (opernest + 2 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest - 1];
    nest = dictfind(token1, &token);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    if (nest != -1)
    {   *token1++ = dictstack[nest];
        opernest++;
        token.value.ival = 1;
    }
    *token1 = token;
}

/* write */

void opwrite(void)
{   struct object *token1, *token2;
    struct file *file;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    file = filecheck(token1, openwrite);
    if (file == NULL) error(errioerror);
    if (token2->type != typeint) error(errtypecheck);
    if (putc(token2->value.ival, file->fptr) == EOF) error(errioerror);
    opernest -= 2;
}

/* writehexstring */

void opwritehexstring(void)
{   struct object *token1, *token2;
    struct file *file;
    FILE *fptr;
    char *sptr;
    int length, ch, num;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    file = filecheck(token1, openwrite);
    if (file == NULL) error(errioerror);
    fptr = file->fptr;
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    length = token2->length;
    sptr = vmsptr(token2->value.vref);
    while (length--)
    {   num = *(unsigned char *)(sptr++);
        ch = num >> 4;
        ch += (ch < 10) ? '0' : 'a' - 10;
        if (putc(ch, fptr) == EOF) error(errioerror);
        ch = num & 15;
        ch += (ch < 10) ? '0' : 'a' - 10;
        if (putc(ch, fptr) == EOF) error(errioerror);
    }
    opernest -= 2;
}

/* writestring */

void opwritestring(void)
{   struct object *token1, *token2;
    struct file *file;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typefile) error(errtypecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    file = filecheck(token1, openwrite);
    if (file == NULL) error(errioerror);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    putmem(file->fptr, vmsptr(token2->value.vref), token2->length);
    opernest -= 2;
}

/* xcheck */

void opxcheck(void)
{   struct object token, *token1;
    int bool;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    bool = ((token1->flags & flagexec) != 0);
    token.type = typebool;
    token.flags = 0;
    token.length = 0;
    token.value.ival = bool;
    *token1 = token;
}

/* xor */

void opxor(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if ((token1->type == typebool && token2->type == typebool) ||
        (token1->type == typeint  && token2->type == typeint))
        token1->value.ival ^= token2->value.ival;
    else
        error(errtypecheck);
    opernest--;
}

/* Initialise the operators (2) */

void initop2(void)
{   systemop(opindex,            "index");
    systemop(opknown,            "known");
    systemop(ople,               "le");
    systemop(oplength,           "length");
    systemop(opln,               "ln");
    systemop(opload,             "load");
    systemop(oplog,              "log");
    systemop(oploop,             "loop");
    systemop(oplt,               "lt");
    systemop(opmaxlength,        "maxlength");
    systemop(opmod,              "mod");
    systemop(opmul,              "mul");
    systemop(opne,               "ne");
    systemop(opneg,              "neg");
    systemop(opnoaccess,         "noaccess");
    systemop(opnot,              "not");
    systemop(opnull,             "null");
    systemop(opor,               "or");
    systemop(oppackedarray,      "packedarray");
    systemop(oppop,              "pop");
    systemop(opprint,            "print");
    systemop(opprompts,          "prompts");
    systemop(oppstack,           "pstack");
    systemop(opput,              "put");
    systemop(opputinterval,      "putinterval");
    systemop(opquit,             "quit");
    systemop(oprcheck,           "rcheck");
    systemop(oprand,             "rand");
    systemop(opread,             "read");
    systemop(opreadhexstring,    "readhexstring");
    systemop(opreadline,         "readline");
    systemop(opreadonly,         "readonly");
    systemop(opreadstring,       "readstring");
    systemop(oprepeat,           "repeat");
    systemop(opresetfile,        "resetfile");
    systemop(oprestore,          "restore");
    systemop(oproll,             "roll");
    systemop(opround,            "round");
    systemop(oprrand,            "rrand");
    systemop(oprun,              "run");
    systemop(opsave,             "save");
    systemop(opsearch,           "search");
    systemop(opsetpacking,       "setpacking");
    systemop(opsin,              "sin");
    systemop(opsqrt,             "sqrt");
    systemop(opsrand,            "srand");
    systemop(opstack,            "stack");
    systemop(opstatus,           "status");
    systemop(opstop,             "stop");
    systemop(opstopped,          "stopped");
    systemop(opstore,            "store");
    systemop(opstring,           "string");
    systemop(opsub,              "sub");
    systemop(optoken,            "token");
    systemop(optruncate,         "truncate");
    systemop(optype,             "type");
    systemop(opusertime,         "usertime");
    systemop(opvmhwm,            "vmhwm");
    systemop(opvmstatus,         "vmstatus");
    systemop(opwcheck,           "wcheck");
    systemop(opwhere,            "where");
    systemop(opwrite,            "write");
    systemop(opwritehexstring,   "writehexstring");
    systemop(opwritestring,      "writestring");
    systemop(opxcheck,           "xcheck");
    systemop(opxor,              "xor");
}

/* End of file "postop2.c" */
