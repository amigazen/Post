/* PostScript interpreter file "postop3.c" - operators (3) */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* concat */

void opconcat(void)
{   struct object *token1;
    float newctm[6], matrix[6];
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->length != 6) error(errrangecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token1->value.vref, matrix);
    multiplymatrix(newctm, matrix, gstate.ctm);
    memcpy((char *) gstate.ctm, (char *) newctm, sizeof newctm);
    opernest -= 1;
}

/* concatmatrix */

void opconcatmatrix(void)
{   struct object *token1, *token2, *token3;
    float newctm[6], matrix[6], oldctm[6];
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token1->type != typearray) error(errtypecheck);
    if (token1->length != 6) error(errrangecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token1->value.vref, matrix);
    if (token2->type != typearray) error(errtypecheck);
    if (token2->length != 6) error(errrangecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token2->value.vref, oldctm);
    if (token3->type != typearray) error(errtypecheck);
    if (token3->length != 6) error(errrangecheck);
    if (token3->flags & flagwprot) error(errinvalidaccess);
    multiplymatrix(newctm, matrix, oldctm);
    arraysave(token3->value.vref, 6);
    putmatrix(token3->value.vref, newctm);
    *token1 = *token3;
    opernest -= 2;
}

/* currentblackgeneration */

void opcurrentblackgeneration(void)
{   if (opernest == operstacksize) error(errstackoverflow);
    operstack[opernest++] = gstate.gcr;
}

/* currentcmykcolor */

void opcurrentcmykcolor(void)
{   struct object token, *token1;
    float c, m, y, k;
    if (opernest + 4 > operstacksize) error(errstackoverflow);
    c = 1.0 - gstate.rgbw[0];
    m = 1.0 - gstate.rgbw[1];
    y = 1.0 - gstate.rgbw[2];
    k = 1.0 - gstate.rgbw[3];
    token1 = &operstack[opernest];
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = c;
    token1[0] = token;
    token.value.rval = m;
    token1[1] = token;
    token.value.rval = y;
    token1[2] = token;
    token.value.rval = k;
    token1[3] = token;
    opernest += 4;
}

/* currentcolorscreen */

void opcurrentcolorscreen(void)
{   struct object token, *token1;
    int plane, p;
    if (opernest + 12 > operstacksize) error(errstackoverflow);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    p = 0;
    token1 = &operstack[opernest];
    for (plane = 0; plane < 4 ; plane++)
    {   token.value.rval = gstate.screenfreq[p];
        token1[0] = token;
        token.value.rval = gstate.screenangle[p];
        token1[1] = token;
        token1[2] = gstate.screenfunc[p];
        token1 += 3;
        if (gstate.screendepth != 1) p++;
    }
    opernest += 12;
}

/* currentcolortransfer */

void opcurrentcolortransfer(void)
{   int plane, p;
    if (opernest + 4 > operstacksize) error(errstackoverflow);
    p = (gstate.screendepth == 1) ? 0 : 3;
    for (plane = 0; plane < 4 ; plane++)
    {   operstack[opernest++] = gstate.transfer[p];
        if (gstate.screendepth != 1) p++;
    }
    opernest += 4;
}

/* currentdash */

void opcurrentdash(void)
{   struct object token, *token1;
    if (opernest + 2 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    token1[0] = gstate.dasharray;
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = gstate.dashoffset;
    token1[1] = token;
    opernest += 2;
}

/* currentflat */

void opcurrentflat(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = gstate.flatness;
    operstack[opernest++] = token;
}

/* currentgray */

void opcurrentgray(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = gstate.gray;
    operstack[opernest++] = token;
}

/* currenthsbcolor */

void opcurrenthsbcolor(void)
{   struct object token, *token1;
    float hsb[3];
    if (opernest + 3 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    gethsbcolour(hsb);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = hsb[0];
    token1[0] = token;
    token.value.rval = hsb[1];
    token1[1] = token;
    token.value.rval = hsb[2];
    token1[2] = token;
    opernest += 3;
}

/* currentlinecap */

void opcurrentlinecap(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = gstate.linecap;
    operstack[opernest++] = token;
}

/* currentlinejoin */

void opcurrentlinejoin(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = gstate.linejoin;
    operstack[opernest++] = token;
}

/* currentlinewidth */

void opcurrentlinewidth(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = gstate.linewidth;
    operstack[opernest++] = token;
}

/* currentmatrix */

void opcurrentmatrix(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->length != 6) error(errrangecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    arraysave(token1->value.vref, 6);
    putmatrix(token1->value.vref, gstate.ctm);
}

/* currentmiterlimit */

void opcurrentmiterlimit(void)
{   struct object token;
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = gstate.mitrelimit;
    operstack[opernest++] = token;
}

/* currentrgbcolor */

void opcurrentrgbcolor(void)
{   struct object token, *token1;
    if (opernest + 3 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = gstate.rgb[0];
    token1[0] = token;
    token.value.rval = gstate.rgb[1];
    token1[1] = token;
    token.value.rval = gstate.rgb[2];
    token1[2] = token;
    opernest += 3;
}

/* currentscreen */

void opcurrentscreen(void)
{   struct object token, *token1;
    if (opernest + 3 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = gstate.screenfreq[gstate.screendepth - 1];
    token1[0] = token;
    token.value.rval = gstate.screenangle[gstate.screendepth - 1];
    token1[1] = token;
    token1[2] = gstate.screenfunc[gstate.screendepth - 1];
    opernest += 3;
}

/* currenttansfer */

void opcurrenttransfer(void)
{   if (opernest == operstacksize) error(errstackoverflow);
    operstack[opernest++] = gstate.transfer[gstate.transdepth - 1];
}

/* currentundercolorremoval */

void opcurrentundercolorremoval(void)
{   if (opernest == operstacksize) error(errstackoverflow);
    operstack[opernest++] = gstate.ucr;
}

/* defaultmatrix */

void opdefaultmatrix(void)
{   struct object *token1;
    float newctm[6];
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->length != 6) error(errrangecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    initctm(newctm);
    arraysave(token1->value.vref, 6);
    putmatrix(token1->value.vref, newctm);
}

/* dtransform */

void opdtransform(void)
{   struct object *token1, *token2, *token3;
    struct point point;
    float newctm[6];
    if (opernest < 2) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    if (token3->type == typearray)
    {   if (token3->length != 6) error(errrangecheck);
        if (token3->flags & flagrprot) error(errinvalidaccess);
        if (opernest < 3) error(errstackunderflow);
        token2 = token3 - 1;
        token1 = token2 - 1;
        getmatrix(token3->value.vref, newctm);
    }
    else
    {   token2 = token3;
        token1 = token2 - 1;
    }
    if      (token1->type == typeint)
        point.x = token1->value.ival;
    else if (token1->type == typereal)
        point.x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        point.y = token2->value.ival;
    else if (token2->type == typereal)
        point.y = token2->value.rval;
    else
        error(errtypecheck);
    if (token3->type == typearray)
    {   dtransform(&point, newctm);
        opernest--;
    }
    else
        dtransform(&point, gstate.ctm);
    token1->type = typereal;
    token1->value.rval = point.x;
    token2->type = typereal;
    token2->value.rval = point.y;
}

/* grestore */

void opgrestore(void)
{   grest();
}

/* grestoreall */

void opgrestoreall(void)
{   grestall();
}

/* gsave */

void opgsave(void)
{   gsave();
}

/* identmatrix */

void opidentmatrix(void)
{   struct object *token1;
    float matrix[6];
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->length != 6) error(errrangecheck);
    if (token1->flags & flagwprot) error(errinvalidaccess);
    matrix[0] = matrix[3] = 1.0;
    matrix[1] = matrix[2] = matrix[4] = matrix[5] = 0.0;
    arraysave(token1->value.vref, 6);
    putmatrix(token1->value.vref, matrix);
}

/* idtransform */

void opidtransform(void)
{   struct object *token1, *token2, *token3;
    struct point point;
    float newctm[6];
    if (opernest < 2) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    if (token3->type == typearray)
    {   if (token3->length != 6) error(errrangecheck);
        if (token3->flags & flagrprot) error(errinvalidaccess);
        if (opernest < 3) error(errstackunderflow);
        token2 = token3 - 1;
        token1 = token2 - 1;
        getmatrix(token3->value.vref, newctm);
    }
    else
    {   token2 = token3;
        token1 = token2 - 1;
    }
    if      (token1->type == typeint)
        point.x = token1->value.ival;
    else if (token1->type == typereal)
        point.x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        point.y = token2->value.ival;
    else if (token2->type == typereal)
        point.y = token2->value.rval;
    else
        error(errtypecheck);
    if (token3->type == typearray)
    {   idtransform(&point, newctm);
        opernest--;
    }
    else
        idtransform(&point, gstate.ctm);
    token1->type = typereal;
    token1->value.rval = point.x;
    token2->type = typereal;
    token2->value.rval = point.y;
}

/* initgraphics */

void opinitgraphics(void)
{   initgraphics();
}

/* initmatrix */

void opinitmatrix(void)
{   initctm(gstate.ctm);
}

/* invertmatrix */

void opinvertmatrix(void)
{   struct object *token1, *token2;
    float newctm[6], oldctm[6];
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typearray) error(errtypecheck);
    if (token1->length != 6) error(errrangecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (token2->type != typearray) error(errtypecheck);
    if (token2->length != 6) error(errrangecheck);
    if (token2->flags & flagwprot) error(errinvalidaccess);
    getmatrix(token1->value.vref, oldctm);
    invertmatrix(newctm, oldctm);
    arraysave(token2->value.vref, 6);
    putmatrix(token2->value.vref, newctm);
    *token1 = *token2;
    opernest--;
}

/* itransform */

void opitransform(void)
{   struct object *token1, *token2, *token3;
    struct point point;
    float newctm[6];
    if (opernest < 2) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    if (token3->type == typearray)
    {   if (token3->length != 6) error(errrangecheck);
        if (token3->flags & flagrprot) error(errinvalidaccess);
        if (opernest < 3) error(errstackunderflow);
        token2 = token3 - 1;
        token1 = token2 - 1;
        getmatrix(token3->value.vref, newctm);
    }
    else
    {   token2 = token3;
        token1 = token2 - 1;
    }
    if      (token1->type == typeint)
        point.x = token1->value.ival;
    else if (token1->type == typereal)
        point.x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        point.y = token2->value.ival;
    else if (token2->type == typereal)
        point.y = token2->value.rval;
    else
        error(errtypecheck);
    if (token3->type == typearray)
    {   itransform(&point, newctm);
        opernest--;
    }
    else
        itransform(&point, gstate.ctm);
    token1->type = typereal;
    token1->value.rval = point.x;
    token2->type = typereal;
    token2->value.rval = point.y;
}

/* matrix */

void opmatrix(void)
{   struct object token;
    float matrix[6];
    if (opernest == operstacksize) error(errstackoverflow);
    token.type = typearray;
    token.flags = 0;
    token.length = 6;
    token.value.vref = arrayalloc(6);
    matrix[0] = matrix[3] = 1.0;
    matrix[1] = matrix[2] = matrix[4] = matrix[5] = 0.0;
    putmatrix(token.value.vref, matrix);
    operstack[opernest++] = token;
}

/* rotate */

void oprotate(void)
{   struct object *token1, *token2;
    float newctm[6], matrix[6], a, s, c;
    if (opernest < 1) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    if (token2->type == typearray)
    {   if (token2->length != 6) error(errrangecheck);
        if (token2->flags & flagwprot) error(errinvalidaccess);
        if (opernest < 2) error(errstackunderflow);
        token1 = token2 - 1;
    }
    else
        token1 = token2;
    if      (token1->type == typeint)
        a = token1->value.ival;
    else if (token1->type == typereal)
        a = token1->value.rval;
    else
        error(errtypecheck);
    a *= degtorad;
    s = sin(a);
    c = cos(a);
    matrix[0] =  c; 
    matrix[1] =  s;
    matrix[2] = -s;
    matrix[3] =  c; 
    matrix[4] = matrix[5] = 0;
    if (token2->type == typearray)
    {   arraysave(token2->value.vref, 6);
        putmatrix(token2->value.vref, matrix);
        *token1 = *token2;
    }
    else
    {   multiplymatrix(newctm, matrix, gstate.ctm);
        memcpy((char *) gstate.ctm, (char *) newctm, sizeof newctm);
    }
    opernest -= 1;
}

/* scale */

void opscale(void)
{   struct object *token1, *token2, *token3;
    float newctm[6], matrix[6], x, y;
    if (opernest < 2) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    if (token3->type == typearray)
    {   if (token3->length != 6) error(errrangecheck);
        if (token3->flags & flagrprot) error(errinvalidaccess);
        if (opernest < 3) error(errstackunderflow);
        token2 = token3 - 1;
        token1 = token2 - 1;
    }
    else
    {   token2 = token3;
        token1 = token2 - 1;
    }
    if      (token1->type == typeint)
        x = token1->value.ival;
    else if (token1->type == typereal)
        x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        y = token2->value.ival;
    else if (token2->type == typereal)
        y = token2->value.rval;
    else
        error(errtypecheck);
    matrix[0] = x;
    matrix[1] = 0.0;
    matrix[2] = 0.0;
    matrix[3] = y;
    matrix[4] = matrix[5] = 0.0;
    if (token3->type == typearray)
    {   arraysave(token3->value.vref, 6);
        putmatrix(token3->value.vref, matrix);
        *token1 = *token3;
    }
    else
    {   multiplymatrix(newctm, matrix, gstate.ctm);
        memcpy((char *) gstate.ctm, (char *) newctm, sizeof newctm);
    }
    opernest -= 2;
}

/* setblackgeneration */

void opsetblackgeneration(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if ((token1->type != typearray && token1->type != typepacked) ||
        !(token1->flags & flagexec))
        error(errtypecheck);
    gstate.gcr = *token1;
    opernest--;
}

/* setcmykcolor */

void opsetcmykcolor(void)
{   struct object *token1;
    float cmyk[4];
    int i;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 4) error(errstackunderflow);
    token1 = &operstack[opernest - 4];
    for (i = 0; i < 4; i++)
    {   if      (token1->type == typeint)
            cmyk[i] = token1->value.ival;
        else if (token1->type == typereal)
            cmyk[i] = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
    }
    setcolour(4, cmyk);
    opernest -= 4;
}

/* setcolorscreen */

void opsetcolorscreen(void)
{   struct object *token1, *token2, *token3;
    struct object func[4];
    float freq[4], ang[4];
    int plane;
    if (opernest < 12) error(errstackunderflow);
    token3 = &operstack[opernest - 10];
    token2 = token3 - 1;
    token1 = token2 - 1;
    for (plane = 0; plane < 4; plane++)
    {   if      (token1->type == typeint)
            freq[plane] = token1->value.ival;
        else if (token1->type == typereal)
            freq[plane] = token1->value.rval;
        else
            error(errtypecheck);
        if (freq[plane] <= 0.0) error(errrangecheck);
        if      (token2->type == typeint)
            ang[plane] = token2->value.ival;
        else if (token2->type == typereal)
            ang[plane] = token2->value.rval;
        else
            error(errtypecheck);
        if ((token3->type != typearray && token3->type != typepacked) ||
            !(token3->flags & flagexec))
            error(errtypecheck);
        func[plane] = *token3;
        token1 += 3;
        token2 += 3;
        token3 += 3;
    }
    gstate.screendepth = 4;
    memcpy((char *) gstate.screenfreq, freq, sizeof freq);
    memcpy((char *) gstate.screenangle, ang, sizeof ang);
    memcpy((char *) gstate.screenfunc, func, sizeof func);
    halfok = gstacksize + 1;
    setuphalf();
    opernest -= 12;
}

/* setcolortransfer */

void opsetcolortransfer(void)
{   struct object *token1;
    struct object func[4];
    int plane;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 4) error(errstackunderflow);
    token1 = &operstack[opernest - 4];
    for (plane = 0; plane < 4; plane++)
    {   if ((token1->type != typearray && token1->type != typepacked) ||
            !(token1->flags & flagexec))
            error(errtypecheck);
        func[plane] = *token1;
        token1++;
    }
    gstate.transdepth = 4;
    memcpy((char *) gstate.transfer, func, sizeof func);
    gstate.shadeok = 0;
    setupshade();
    opernest -= 4;
}

/* setdash */

void opsetdash(void)
{   struct object *token1, *token2, *aptr;
    float dashlength[dashsize], dashtotal;
    float r;
    int len;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typearray) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (token2->type == typeint)
        r = token2->value.ival;
    else if (token2->type == typereal)
        r = token2->value.rval;
    else
        error(errtypecheck);
    if (r < 0.0) error(errrangecheck);
    if (token1->length != 0)
    {   dashtotal = 0.0;
        aptr = vmaptr(token1->value.vref);
        for (len = 0; len < token1->length; len++)
        {   if      (aptr->type == typeint)
                dashlength[len] = aptr->value.ival;
            else if (aptr->type == typereal)
                dashlength[len] = aptr->value.rval;
            else
                error(errtypecheck);
            if (dashlength[len] < 0.0) error(errrangecheck);
            dashtotal += dashlength[len];
            aptr++;
        }
        if (dashtotal == 0.0) error(errrangecheck);
        memcpy((char *) gstate.dashlength, (char *) dashlength,
                len * sizeof (float));
    }
    gstate.dasharray = *token1;
    gstate.dashoffset = r;
    opernest -= 2;
}

/* setflat */

void opsetflat(void)
{   struct object *token1;
    float r;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typeint)
        r = token1->value.ival;
    else if (token1->type == typereal)
        r = token1->value.rval;
    else
        error(errtypecheck);
    if (r <= 0.0001) error(errrangecheck);
    gstate.flatness = r;
    opernest--;
}

/* setgray */

void opsetgray(void)
{   struct object *token1;
    float gray;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typeint)
        gray = token1->value.ival;
    else if (token1->type == typereal)
        gray = token1->value.rval;
    else
        error(errtypecheck);
    setcolour(1, &gray);
    opernest--;
}

/* sethsbcolor */

void opsethsbcolor(void)
{   struct object *token1;
    float hsb[3];
    int i;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 3) error(errstackunderflow);
    token1 = &operstack[opernest - 3];
    for (i = 0; i < 3; i++)
    {   if      (token1->type == typeint)
            hsb[i] = token1->value.ival;
        else if (token1->type == typereal)
            hsb[i] = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
    }
    sethsbcolour(hsb);
    opernest -= 3;
}

/* setlinecap */

void opsetlinecap(void)
{   struct object *token1;
    int i;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    i = token1->value.ival;
    if (i < 0 || i > 2) error(errrangecheck);
    gstate.linecap = i;
    opernest--;
}

/* setlinejoin */

void opsetlinejoin(void)
{   struct object *token1;
    int i;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    i = token1->value.ival;
    if (i < 0 || i > 2) error(errrangecheck);
    gstate.linejoin = i;
    opernest--;
}

/* setlinewidth */

void opsetlinewidth(void)
{   struct object *token1;
    float r;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typeint)
        r = token1->value.ival;
    else if (token1->type == typereal)
        r = token1->value.rval;
    else
        error(errtypecheck);
    if (r < 0.0) error(errrangecheck);
    gstate.linewidth = r;
    opernest--;
}

/* setmatrix */

void opsetmatrix(void)
{   struct object *token1;
    float newctm[6];
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typearray) error(errtypecheck);
    if (token1->length != 6) error(errrangecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token1->value.vref, newctm);
    memcpy((char *) gstate.ctm, (char *) newctm, sizeof newctm);
    opernest--;
}

/* setmiterlimit */

void opsetmiterlimit(void)
{   struct object *token1;
    float r, s;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typeint)
        r = token1->value.ival;
    else if (token1->type == typereal)
        r = token1->value.rval;
    else
        error(errtypecheck);
    if (r < 1.0) error(errrangecheck);
    s = 1.0 / r;
    gstate.mitrelimit = r;
    gstate.mitresin = 2.0 * s * sqrt(1.0 - s * s);
    opernest--;
}

/* setrgbcolor */

void opsetrgbcolor(void)
{   struct object *token1;
    float rgb[3];
    int i;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 3) error(errstackunderflow);
    token1 = &operstack[opernest - 3];
    for (i = 0; i < 3; i++)
    {   if      (token1->type == typeint)
            rgb[i] = token1->value.ival;
        else if (token1->type == typereal)
            rgb[i] = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
    }
    setcolour(3, rgb);
    opernest -= 3;
}

/* setscreen */

void opsetscreen(void)
{   struct object *token1, *token2, *token3;
    float freq, ang;
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    if      (token1->type == typeint)
        freq = token1->value.ival;
    else if (token1->type == typereal)
        freq = token1->value.rval;
    else
        error(errtypecheck);
    if (freq <= 0) error(errrangecheck);
    if      (token2->type == typeint)
        ang = token2->value.ival;
    else if (token2->type == typereal)
        ang = token2->value.rval;
    else
        error(errtypecheck);
    if ((token3->type != typearray && token3->type != typepacked) ||
        !(token3->flags & flagexec))
        error(errtypecheck);
    gstate.screendepth = 1;
    gstate.screenfreq[0] = freq;
    gstate.screenangle[0] = ang;
    gstate.screenfunc[0] = *token3;
    halfok = gstacksize + 1;
    setuphalf();
    opernest -= 3;
}

/* settransfer */

void opsettransfer(void)
{   struct object *token1;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if ((token1->type != typearray && token1->type != typepacked) ||
        !(token1->flags & flagexec))
        error(errtypecheck);
    gstate.transdepth = 1;
    gstate.transfer[0] = *token1;
    gstate.shadeok = 0;
    setupshade();
    opernest--;
}

/* setundercolorremoval */

void opsetundercolorremoval(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if ((token1->type != typearray && token1->type != typepacked) ||
        !(token1->flags & flagexec))
        error(errtypecheck);
    gstate.ucr = *token1;
    opernest--;
}


/* transform */

void optransform(void)
{   struct object *token1, *token2, *token3;
    struct point point;
    float newctm[6];
    if (opernest < 2) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    if (token3->type == typearray)
    {   if (token3->length != 6) error(errrangecheck);
        if (token3->flags & flagrprot) error(errinvalidaccess);
        if (opernest < 3) error(errstackunderflow);
        token2 = token3 - 1;
        token1 = token2 - 1;
        getmatrix(token3->value.vref, newctm);
    }
    else
    {   token2 = token3;
        token1 = token2 - 1;
    }
    if      (token1->type == typeint)
        point.x = token1->value.ival;
    else if (token1->type == typereal)
        point.x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        point.y = token2->value.ival;
    else if (token2->type == typereal)
        point.y = token2->value.rval;
    else
        error(errtypecheck);
    if (token3->type == typearray)
    {   transform(&point, newctm);
        opernest--;
    }
    else
        transform(&point, gstate.ctm);
    token1->type = typereal;
    token1->value.rval = point.x;
    token2->type = typereal;
    token2->value.rval = point.y;
}

/* translate */

void optranslate(void)
{   struct object *token1, *token2, *token3;
    float newctm[6], matrix[6], x, y;
    if (opernest < 2) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    if (token3->type == typearray)
    {   if (token3->length != 6) error(errrangecheck);
        if (token3->flags & flagwprot) error(errinvalidaccess);
        if (opernest < 3) error(errstackunderflow);
        token2 = token3 - 1;
        token1 = token2 - 1;
    }
    else
    {   token2 = token3;
        token1 = token2 - 1;
    }
    if      (token1->type == typeint)
        x = token1->value.ival;
    else if (token1->type == typereal)
        x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        y = token2->value.ival;
    else if (token2->type == typereal)
        y = token2->value.rval;
    else
        error(errtypecheck);
    matrix[0] = matrix[3] = 1.0;
    matrix[1] = matrix[2] = 0.0;
    matrix[4] = x;
    matrix[5] = y;
    if (token3->type == typearray)
    {   arraysave(token3->value.vref, 6);
        putmatrix(token3->value.vref, matrix);
        *token1 = *token3;
    }
    else
    {   multiplymatrix(newctm, matrix, gstate.ctm);
        memcpy((char *) gstate.ctm, (char *) newctm, sizeof newctm);
    }
    opernest -= 2;
}

/* Initialise the operators (3) */

void initop3(void)
{   systemop(opconcat,           "concat");
    systemop(opconcatmatrix,     "concatmatrix");
    systemop(opcurrentblackgeneration,"currentblackgeneration");
    systemop(opcurrentcmykcolor, "currentcmykcolor");
    systemop(opcurrentcolorscreen,"currentcolorscreen");
    systemop(opcurrentcolortransfer,"currentcolortransfer");
    systemop(opcurrentdash,      "currentdash");
    systemop(opcurrentflat,      "currentflat");
    systemop(opcurrentgray,      "currentgray");
    systemop(opcurrenthsbcolor,  "currenthsbcolor");
    systemop(opcurrentlinecap,   "currentlinecap");
    systemop(opcurrentlinejoin,  "currentlinejoin");
    systemop(opcurrentlinewidth, "currentlinewidth");
    systemop(opcurrentmatrix,    "currentmatrix");
    systemop(opcurrentmiterlimit,"currentmiterlimit");
    systemop(opcurrentrgbcolor,  "currentrgbcolor");
    systemop(opcurrentscreen,    "currentscreen");
    systemop(opcurrenttransfer,  "currenttransfer");
    systemop(opcurrentundercolorremoval,"currentundercolorremoval");
    systemop(opdefaultmatrix,    "defaultmatrix");
    systemop(opdtransform,       "dtransform");
    systemop(opgrestore,         "grestore");
    systemop(opgrestoreall,      "grestoreall");
    systemop(opgsave,            "gsave");
    systemop(opidentmatrix,      "identmatrix");
    systemop(opidtransform,      "idtransform");
    systemop(opinitgraphics,     "initgraphics");
    systemop(opinitmatrix,       "initmatrix");
    systemop(opinvertmatrix,     "invertmatrix");
    systemop(opitransform,       "itransform");
    systemop(opmatrix,           "matrix");
    systemop(oprotate,           "rotate");
    systemop(opscale,            "scale");
    systemop(opsetblackgeneration,"setblackgeneration");
    systemop(opsetcmykcolor,     "setcmykcolor");
    systemop(opsetcolorscreen,   "setcolorscreen");
    systemop(opsetcolortransfer, "setcolortransfer");
    systemop(opsetdash,          "setdash");
    systemop(opsetflat,          "setflat");
    systemop(opsetgray,          "setgray");
    systemop(opsethsbcolor,      "sethsbcolor");
    systemop(opsetlinecap,       "setlinecap");
    systemop(opsetlinejoin,      "setlinejoin");
    systemop(opsetlinewidth,     "setlinewidth");
    systemop(opsetmatrix,        "setmatrix");
    systemop(opsetmiterlimit,    "setmiterlimit");
    systemop(opsetrgbcolor,      "setrgbcolor");
    systemop(opsetscreen,        "setscreen");
    systemop(opsettransfer,      "settransfer");
    systemop(opsetundercolorremoval,"setundercolorremoval");
    systemop(optransform,        "transform");
    systemop(optranslate,        "translate");
}

/* End of file "postop3.c" */
