/* PostScript interpreter file "postop4.c" - operators (4) */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* arc */

void oparc(void)
{   struct object *token1;
    struct point centre, beg, end;
    float radaa[3];
    if (opernest < 5) error(errstackunderflow);
    token1 = &operstack[opernest - 5];
    centre.type = 0;
    if      (token1->type == typeint)
        centre.x = token1->value.ival;
    else if (token1->type == typereal)
        centre.x = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        centre.y = token1->value.ival;
    else if (token1->type == typereal)
        centre.y = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        radaa[0] = token1->value.ival;
    else if (token1->type == typereal)
        radaa[0] = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        radaa[1] = token1->value.ival;
    else if (token1->type == typereal)
        radaa[1] = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        radaa[2] = token1->value.ival;
    else if (token1->type == typereal)
        radaa[2] = token1->value.rval;
    else
        error(errtypecheck);
    if (radaa[0] <= 0.0) error(errrangecheck);
    radaa[1] *= degtorad;
    radaa[2] *= degtorad;
    arc(1, radaa, &centre, &beg, &end);
    opernest -= 5;
}

/* arcn */

void oparcn(void)
{   struct object *token1;
    struct point centre, beg, end;
    float radaa[3];
    if (opernest < 5) error(errstackunderflow);
    token1 = &operstack[opernest - 5];
    centre.type = 0;
    if      (token1->type == typeint)
        centre.x = token1->value.ival;
    else if (token1->type == typereal)
        centre.x = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        centre.y = token1->value.ival;
    else if (token1->type == typereal)
        centre.y = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        radaa[0] = token1->value.ival;
    else if (token1->type == typereal)
        radaa[0] = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        radaa[1] = token1->value.ival;
    else if (token1->type == typereal)
        radaa[1] = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        radaa[2] = token1->value.ival;
    else if (token1->type == typereal)
        radaa[2] = token1->value.rval;
    else
        error(errtypecheck);
    if (radaa[0] <= 0.0) error(errrangecheck);
    radaa[1] *= degtorad;
    radaa[2] *= degtorad;
    arc(-1, radaa, &centre, &beg, &end);
    opernest -= 5;
}

/* arcto */

void oparcto(void)
{   struct object token, *token1;
    struct point beg, mid, end;
    float radaa[3], angs, angd, radx;
    if (opernest < 5) error(errstackunderflow);
    token1 = &operstack[opernest - 5];
    if (gstate.pathbeg == gstate.pathend) error(errnocurrentpoint);
    beg = patharray[gstate.pathend - 1];
    itransform(&beg, gstate.ctm);
    mid.type = ptline;
    if      (token1->type == typeint)
        mid.x = token1->value.ival;
    else if (token1->type == typereal)
        mid.x = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        mid.y = token1->value.ival;
    else if (token1->type == typereal)
        mid.y = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    end.type = 0;
    if      (token1->type == typeint)
        end.x = token1->value.ival;
    else if (token1->type == typereal)
        end.x = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        end.y = token1->value.ival;
    else if (token1->type == typereal)
        end.y = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        radaa[0] = token1->value.ival;
    else if (token1->type == typereal)
        radaa[0] = token1->value.rval;
    else
        error(errtypecheck);
    if (radaa[0] <= 0.0) error(errrangecheck);
    radaa[1] = atan2(beg.y - mid.y, beg.x - mid.x);
    radaa[2] = atan2(end.y - mid.y, end.x - mid.x);
    angs = (radaa[2] + radaa[1]) / 2.0;
    angd = (radaa[2] - radaa[1]) / 2.0;
    radx = radaa[0] / sin(angd);
    if (angd < 0) angd += pi;
    if (angd < pi2)
    {   radaa[1] -= pi2;
        radaa[2] += pi2;
    }
    else
    {   radx = -radx;
        radaa[1] += pi2;
        radaa[2] -= pi2;
    }
    mid.x += radx * cos(angs);
    mid.y += radx * sin(angs);
    arc(0, radaa, &mid, &beg, &end);
    token1 -= 4;
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = beg.x;
    token1[0] = token;
    token.value.rval = beg.y;
    token1[1] = token;
    token.value.rval = end.x;
    token1[2] = token;
    token.value.rval = end.y;
    token1[3] = token;
    opernest -= 1;
}

/* ashow */

void opashow(void)
{   struct object *token1, *token2, *token3;
    struct point width[2];
    if (opernest < 3) error(errstackunderflow);
    token3 = &operstack[opernest - 1];
    token2 = token3 - 1;
    token1 = token2 - 1;
    width[0].x = width[0].y = 0.0;
    if      (token1->type == typeint)
        width[1].x = token1->value.ival;
    else if (token1->type == typereal)
        width[1].x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        width[1].y = token2->value.ival;
    else if (token2->type == typereal)
        width[1].y = token2->value.rval;
    else
        error(errtypecheck);
    dtransform(&width[1], gstate.ctm);
    if (token3->type != typestring) error(errtypecheck);
    if (token3->flags & flagrprot) error(errinvalidaccess);
    show(token3, 3, width, -1, NULL);
    opernest -= 3;
}

/* awidthshow */

void opawidthshow(void)
{   struct object *token1, *token2, *token3, *token4, *token5, *token6;
    struct point width[2];
    int wchar;
    if (opernest < 6) error(errstackunderflow);
    token6 = &operstack[opernest - 1];
    token5 = token6 - 1;
    token4 = token5 - 1;
    token3 = token4 - 1;
    token2 = token3 - 1;
    token1 = token2 - 1;
    if      (token1->type == typeint)
        width[0].x = token1->value.ival;
    else if (token1->type == typereal)
        width[0].x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        width[0].y = token2->value.ival;
    else if (token2->type == typereal)
        width[0].y = token2->value.rval;
    else
        error(errtypecheck);
    dtransform(&width[0], gstate.ctm);
    if (token3->type != typeint) error(errtypecheck);
    wchar = token3->value.ival;
    if (wchar < 0 || wchar > 255) error(errrangecheck);
    if      (token4->type == typeint)
        width[1].x = token4->value.ival;
    else if (token4->type == typereal)
        width[1].x = token4->value.rval;
    else
        error(errtypecheck);
    if      (token5->type == typeint)
        width[1].y = token5->value.ival;
    else if (token5->type == typereal)
        width[1].y = token5->value.rval;
    else
        error(errtypecheck);
    dtransform(&width[1], gstate.ctm);
    if (token6->type != typestring) error(errtypecheck);
    if (token6->flags & flagrprot) error(errinvalidaccess);
    show(token6, 3, width, wchar, NULL);
    opernest -= 6;
}

/* cachestatus */

void opcachestatus(void)
{   struct object token, *token1;
    int status[7], i;
    if (opernest + 7 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    cachestatus(status);
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    for (i = 0; i < 7; i++)
    {   token.value.ival = status[i];
        *token1++ = token;
    }
    opernest += 7;
}

/* charpath */

void opcharpath(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    if (token2->type != typebool) error(errtypecheck);
    show(token1, token2->value.ival, NULL, -1, NULL);
    opernest -= 2;
}

/* clip */

void opclip(void)
{   closepath(ptclosei);
    flattenpath();
    clip(-1);
}

/* clippath */

void opclippath(void)
{   clippath();
}

/* closepath */

void opclosepath(void)
{   closepath(ptclosex);
}

/* colorimage */

void opcolorimage(void)
{   struct object *token1, *token2, *token3, *token4,
                  *token5, *token6, *token7, *aptr;
    float matrix[6];
    int ncol, multi, width, height, bps;
    int nproc, i;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 2) error(errstackunderflow);
    token7 = &operstack[opernest - 1];
    token6 = token7 - 1;
    if (token6->type != typebool) error(errtypecheck);
    multi = token6->value.ival;
    if (token7->type != typeint) error(errtypecheck);
    ncol = token7->value.ival;
    if (ncol != 1 && ncol != 3 && ncol != 4) error(errrangecheck);
    nproc = multi ? ncol : 1;
    if (opernest < nproc + 6) error(errstackunderflow);
    token5 = token6 - nproc;
    aptr = token5;
    for (i = 0; i < nproc; i++)
    {   if (aptr->type != typearray && aptr->type != typepacked)
            error(errtypecheck);
        aptr++;
    }
    token4 = token5 - 1;
    token3 = token4 - 1;
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token1->type != typeint) error(errtypecheck);
    width = token1->value.ival;
    if (width <= 0) error(errrangecheck);
    if (token2->type != typeint) error(errtypecheck);
    height = token2->value.ival;
    if (height <= 0) error(errrangecheck);
    if (token3->type != typeint) error(errtypecheck);
    bps = token3->value.ival;
    if (bps != 1 && bps != 2 && bps != 4 && bps != 8) error(errrangecheck);
    if (token4->type != typearray) error(errtypecheck);
    if (token4->length != 6) error(errrangecheck);
    if (token4->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token4->value.vref, matrix);
    image(width, height, bps, matrix, -1, ncol, multi, token5);
    opernest -= nproc + 6;
}

/* copypage */

void opcopypage(void)
{   struct object token;
    int num = 1;
    if (istate.flags & intgraph) error(errundefined);
    if (gstate.dev.depth != 0)
    {   if (dictfind(&copies, &token) != -1)
        {   if (token.type != typeint) error(errtypecheck);
            num = token.value.ival;
            if (num < 0) error(errrangecheck);
        }
        copypage(num);
    }
}

/* currentband */

void opcurrentband(void)
{   struct object token, *token1;
    if (opernest + 3 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    token.type = typeint;
    token.flags = 0;
    token.length = 0;
    token.value.ival = page.ybase;
    token1[0] = token;
    token.value.ival = page.ysize;
    token1[1] = token;
    token.value.ival = page.yheight;
    token1[2] = token;
    opernest += 3;
}

/* currentfont */

void opcurrentfont(void)
{   if (opernest + 1 > operstacksize) error(errstackoverflow);
    operstack[opernest++] = gstate.font;
}

/* currentpoint */

void opcurrentpoint(void)
{   struct object token, *token1;
    struct point point;
    if (opernest + 2 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    if (gstate.pathbeg == gstate.pathend) error(errnocurrentpoint);
    point = patharray[gstate.pathend - 1];
    itransform(&point, gstate.ctm);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = point.x;
    token1[0] = token;
    token.value.rval = point.y;
    token1[1] = token;
    opernest += 2;
}

/* curveto */

void opcurveto(void)
{   struct object *token1;
    struct point point[3], *ppoint;
    int i;
    if (opernest < 6) error(errstackunderflow);
    token1 = &operstack[opernest - 6];
    if (gstate.pathbeg == gstate.pathend) error(errnocurrentpoint);
    ppoint = &point[0];
    i = 3;
    while (i--)
    {   ppoint->type = ptcurve;
        if      (token1->type == typeint)
            ppoint->x = token1->value.ival;
        else if (token1->type == typereal)
            ppoint->x = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
        if      (token1->type == typeint)
            ppoint->y = token1->value.ival;
        else if (token1->type == typereal)
            ppoint->y = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
        transform(ppoint, gstate.ctm);
        ppoint++;
    }
    checkpathsize(gstate.pathend + 3);
    ppoint = &patharray[gstate.pathend];
    ppoint[0] = point[0];
    ppoint[1] = point[1];
    ppoint[2] = point[2];
    gstate.pathend += 3;
    opernest -= 6;
}

/* definefont */

void opdefinefont(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token2->type != typedict) error(errtypecheck);
    definefont(token1, token2);
    *token1 = *token2;
    opernest -= 1;
}

/* eoclip */

void opeoclip(void)
{   closepath(ptclosei);
    flattenpath();
    clip(1);
}

/* eofill */

void opeofill(void)
{   if (istate.flags & intchar)
    {   if      (istate.type < 2)
        {   charpath();
            return;
        }
        else if (istate.type == 2 && !gstate.cacheflag)
        {   gstate.pathend = gstate.pathbeg;
            return;
        }
    }
    closepath(ptclosei);
    flattenpath();
    setupfill();
    fill(gstate.pathbeg, gstate.pathend, 1);
    gstate.pathend = gstate.pathbeg;
}

/* erasepage */

void operasepage(void)
{   erasepage();
}

/* findfont */

void opfindfont(void)
{   struct object token, *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    findfont(token1, &token);
    *token1 = token;
}

/* fill */

void opfill(void)
{   if (istate.flags & intchar)
    {   if      (istate.type < 2)
        {   charpath();
            return;
        }
        else if (istate.type == 2 && !gstate.cacheflag)
        {   gstate.pathend = gstate.pathbeg;
            return;
        }
    }
    closepath(ptclosei);
    flattenpath();
    setupfill();
    fill(gstate.pathbeg, gstate.pathend, -1);
    gstate.pathend = gstate.pathbeg;
}

/* flattenpath */

void opflattenpath(void)
{   flattenpath();
}

/* image */

void opimage(void)
{   struct object *token1, *token2, *token3, *token4, *token5;
    float matrix[6];
    int width, height, bps;
    if (gstate.cacheflag) error(errundefined);
    if (opernest < 5) error(errstackunderflow);
    token5 = &operstack[opernest - 1];
    token4 = token5 - 1;
    token3 = token4 - 1;
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token1->type != typeint) error(errtypecheck);
    width = token1->value.ival;
    if (width <= 0) error(errrangecheck);
    if (token2->type != typeint) error(errtypecheck);
    height = token2->value.ival;
    if (height <= 0) error(errrangecheck);
    if (token3->type != typeint) error(errtypecheck);
    bps = token3->value.ival;
    if (bps != 1 && bps != 2 && bps != 4 && bps != 8) error(errrangecheck);
    if (token4->type != typearray) error(errtypecheck);
    if (token4->length != 6) error(errrangecheck);
    if (token4->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token4->value.vref, matrix);
    if (token5->type != typearray && token5->type != typepacked)
        error(errtypecheck);
    image(width, height, bps, matrix, -1, 1, 1, token5);
    opernest -= 5;
}

/* imagemask */

void opimagemask(void)
{   struct object *token1, *token2, *token3, *token4, *token5;
    float matrix[6];
    int width, height, mode;
    if (opernest < 5) error(errstackunderflow);
    token5 = &operstack[opernest - 1];
    token4 = token5 - 1;
    token3 = token4 - 1;
    token2 = token3 - 1;
    token1 = token2 - 1;
    if (token1->type != typeint) error(errtypecheck);
    width = token1->value.ival;
    if (width <= 0) error(errrangecheck);
    if (token2->type != typeint) error(errtypecheck);
    height = token2->value.ival;
    if (height <= 0) error(errrangecheck);
    if (token3->type != typebool) error(errtypecheck);
    mode = token3->value.ival;
    if (token4->type != typearray) error(errtypecheck);
    if (token4->length != 6) error(errrangecheck);
    if (token4->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token4->value.vref, matrix);
    if (token5->type != typearray && token5->type != typepacked)
        error(errtypecheck);
    image(width, height, 1, matrix, mode, 1, 1, token5);
    opernest -= 5;
}

/* initclip */

void opinitclip(void)
{   cliplength(0);
    gstate.clipflag = 0;
}

/* kshow */

void opkshow(void)
{   struct object *token1, *token2;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typearray && token1->type != typepacked)
        error(errtypecheck);
    if (token2->type != typestring) error(errtypecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    show(token2, 3, NULL, -1, token1);
    opernest -= 2;
}

/* lineto */

void oplineto(void)
{   struct object *token1, *token2;
    struct point point;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    point.type = ptline;
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
    transform(&point, gstate.ctm);
    if (gstate.pathbeg == gstate.pathend) error(errnocurrentpoint);
    checkpathsize(gstate.pathend + 1);
    patharray[gstate.pathend++] = point;
    opernest -= 2;
}

/* makefont */

void opmakefont(void)
{   struct object *token1, *token2;
    float matrix[6];
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typedict) error(errtypecheck);
    if (token2->type != typearray) error(errtypecheck);
    if (token2->length != 6) error(errrangecheck);
    if (token2->flags & flagrprot) error(errinvalidaccess);
    getmatrix(token2->value.vref, matrix);
    makefont(token1, matrix);
    opernest -= 1;
}

/* moveto */

void opmoveto(void)
{   struct object *token1, *token2;
    struct point point;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    point.type = ptmove;
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
    transform(&point, gstate.ctm);
    closepath(ptclosei);
    if (gstate.pathbeg == gstate.pathend ||
        patharray[gstate.pathend - 1].type != ptmove)
        checkpathsize(gstate.pathend + 1);
    else
        gstate.pathend--;
    patharray[gstate.pathend++] = point;
    opernest -= 2;
}

/* newpath */

void opnewpath(void)
{   gstate.pathend = gstate.pathbeg;
}

/* nulldevice */

void opnulldevice(void)
{   nulldevice();
}

/* pathbbox */

void oppathbbox(void)
{   struct object token, *token1;
    struct point point, *ppoint, box[4], ll, ur;
    int len;
    if (opernest + 4 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    len = gstate.pathend - gstate.pathbeg;
    if (len == 0) error(errnocurrentpoint);
    len--;
    ppoint = &patharray[gstate.pathbeg];
    ll = ur = *ppoint++;
    while (len--)
    {   point = *ppoint++;
        if (len != 0 || point.type != ptmove)
        {   if (point.x < ll.x) ll.x = point.x;
            if (point.x > ur.x) ur.x = point.x;
            if (point.y < ll.y) ll.y = point.y;
            if (point.y > ur.y) ur.y = point.y;
        }
    }
    itransform(&ll, gstate.ctm);
    itransform(&ur, gstate.ctm);
    box[0].x = box[3].x = ll.x;
    box[1].x = box[2].x = ur.x;
    box[0].y = box[1].y = ll.y;
    box[2].y = box[3].y = ur.y;
    ll = ur = box[0];
    ppoint = &box[1];
    len = 4;
    while (--len)
    {   point = *ppoint++;
        if (point.x < ll.x) ll.x = point.x;
        if (point.x > ur.x) ur.x = point.x;
        if (point.y < ll.y) ll.y = point.y;
        if (point.y > ur.y) ur.y = point.y;
    }
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = ll.x;
    token1[0] = token;
    token.value.rval = ll.y;
    token1[1] = token;
    token.value.rval = ur.x;
    token1[2] = token;
    token.value.rval = ur.y;
    token1[3] = token;
    opernest += 4;
}

/* pathforall */

void oppathforall(void)
{   struct object token, *token1, *tokenx;
    struct point point, *ppoint;
    int i, p;
    if (currtoken->flags & flagctrl)
    {   tokenx = &execstack[execnest - 2];
        for (;;)
        {   p = gstate.pathbeg + tokenx->value.ival;
            if (p >= gstate.pathend)
            {   execnest -= 6;
                return;
            }
            ppoint = &patharray[p];
            if (ppoint->type != ptclosei) break;
            tokenx->value.ival++;
        }
        if      (ppoint->type == ptmove)
        {   i = 1;
            tokenx->value.ival += 1;
            tokenx -= 4;
        }
        else if (ppoint->type == ptline)
        {   i = 1;
            tokenx->value.ival += 1;
            tokenx -= 3;
        }
        else if (ppoint->type == ptcurve)
        {   i = 3;
            tokenx->value.ival += 3;
            tokenx -= 2;
        }
        else
        {   i = 0;
            tokenx->value.ival += 1;
            tokenx -= 1;
        }
        if (opernest + i + i > operstacksize) error(errstackoverflow);
        token1 = &operstack[opernest];
        token.type = typereal;
        token.flags = 0;
        token.length = 0;
        while (i--)
        {   point = *ppoint++;
            itransform(&point, gstate.ctm);
            token.value.rval = point.x;
            token1[0] = token;
            token.value.rval = point.y;
            token1[1] = token;
            token1 += 2;
            opernest += 2;
        }
        execstack[execnest++] = *tokenx;
    }
    else
    {   if (opernest < 4) error(errstackunderflow);
        token1 = &operstack[opernest];
        i = 4;
        while (i--)
        {   token1--;
            if (token1->type != typearray && token1->type != typepacked)
                error(errtypecheck);
        }
        if (execnest + 7 > execstacksize) error(errexecstackoverflow);
        tokenx = &execstack[execnest];
        tokenx[0] = *token1++;
        tokenx[1] = *token1++;
        tokenx[2] = *token1++;
        tokenx[3] = *token1;
        token.type = typeint;
        token.flags = 0;
        token.length = 0;
        token.value.ival = 0;
        tokenx[4] = token;
        token = *currtoken;
        token.flags &= ~flagexec;
        token.flags |= flagctrl | flagloop;
        token.length = 6;
        tokenx[5] = token;
        execnest += 6;
        opernest -= 4;
    }
}

/* rcurveto */

void oprcurveto(void)
{   struct object *token1;
    struct point point[3], *ppoint;
    float x, y;
    int i;
    if (opernest < 6) error(errstackunderflow);
    token1 = &operstack[opernest - 6];
    if (gstate.pathbeg == gstate.pathend) error(errnocurrentpoint);
    ppoint = &patharray[gstate.pathend - 1];
    x = ppoint->x;
    y = ppoint->y;
    ppoint = &point[0];
    i = 3;
    while (i--)
    {   ppoint->type = ptcurve;
        if      (token1->type == typeint)
            ppoint->x = token1->value.ival;
        else if (token1->type == typereal)
            ppoint->x = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
        if      (token1->type == typeint)
            ppoint->y = token1->value.ival;
        else if (token1->type == typereal)
            ppoint->y = token1->value.rval;
        else
            error(errtypecheck);
        dtransform(ppoint, gstate.ctm);
        ppoint->x += x;
        ppoint->y += y;
        token1++;
        ppoint++;
    }
    checkpathsize(gstate.pathend + 3);
    ppoint = &patharray[gstate.pathend];
    ppoint[0] = point[0];
    ppoint[1] = point[1];
    ppoint[2] = point[2];
    gstate.pathend += 3;
    opernest -= 6;
}

/* reversepath */

void opreversepath(void)
{   struct point point, *ppoint1, *ppoint2, *ppoint3;
    int len;
    len = gstate.pathend - gstate.pathbeg;
    ppoint1 = &patharray[gstate.pathbeg];

    /* While we have any subpaths left ... */

    while (len != 0)
    {   ppoint2 = ppoint1;

        /* Scan to the end of the subpath.  If we get to a close then reset
         * its coordinates to the end of the path.  For other cases we copy
         * the type to the previous point - as the type is at the END of
         * the line or curve. */

        for (;;)
        {   len--;
            if (ppoint1->type == ptclosex || ppoint1->type == ptclosei)
            {   ppoint1->x = (ppoint1 - 1)->x;
                ppoint1->y = (ppoint1 - 1)->y;
                break;
            }
            if (ppoint1 != ppoint2)
                (ppoint1 - 1)->type = ppoint1->type;
            ppoint1++;
            if (len == 0)
                break;
        }

        /* Now reverse the contents of the subpath.  Make the (new) first
         * element a move. */

        ppoint3 = ppoint1 - 1;
        ppoint3->type = ptmove;
        while (ppoint2 < ppoint3)
        {   point = *ppoint2;
            *ppoint2 = *ppoint3;
            *ppoint3 = point;
            ppoint2++;
            ppoint3--;
        }

        /* Continue with next subpath. */

        ppoint1++;
    }
}

/* rlineto */

void oprlineto(void)
{   struct object *token1, *token2;
    struct point point, *ppoint;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    point.type = ptline;
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
    dtransform(&point, gstate.ctm);
    if (gstate.pathbeg == gstate.pathend) error(errnocurrentpoint);
    ppoint = &patharray[gstate.pathend - 1];
    point.x += ppoint->x;
    point.y += ppoint->y;
    checkpathsize(gstate.pathend + 1);
    patharray[gstate.pathend++] = point;
    opernest -= 2;
}

/* rmoveto */

void oprmoveto(void)
{   struct object *token1, *token2;
    struct point point, *ppoint;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    point.type = ptmove;
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
    dtransform(&point, gstate.ctm);
    if (gstate.pathbeg == gstate.pathend) error(errnocurrentpoint);
    ppoint = &patharray[gstate.pathend - 1];
    point.x += ppoint->x;
    point.y += ppoint->y;
    closepath(ptclosei);
    if (patharray[gstate.pathend - 1].type != ptmove)
        checkpathsize(gstate.pathend + 1);
    else
        gstate.pathend--;
    patharray[gstate.pathend++] = point;
    opernest -= 2;
}

/* scalefont */

void opscalefont(void)
{   struct object *token1, *token2;
    float matrix[6], scale;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if (token1->type != typedict) error(errtypecheck);
    if      (token2->type == typeint)
        scale = token2->value.ival;
    else if (token2->type == typereal)
        scale = token2->value.rval;
    else
        error(errtypecheck);
    matrix[1] = matrix[2] = matrix[4] = matrix[5] = 0.0;
    matrix[0] = matrix[3] = scale;
    makefont(token1, matrix);
    opernest -= 1;
}

/* setband */

void opsetband(void)
{   struct object *token1;
    int base;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    base = token1->value.ival;
    if (base < 0 || base >= page.yheight)
        error(errrangecheck);
    page.ybase = base;
    setdevice(&page);
    opernest--;
}

/* setcachedevice */

void opsetcachedevice(void)
{   struct object *token1;
    struct point point, width[5], ll, ur;
    int i;
    if (opernest < 6) error(errstackunderflow);
    token1 = &operstack[opernest - 6];
    for (i = 0; i < 3; i++)
    {   if      (token1->type == typeint)
            width[i].x = token1->value.ival;
        else if (token1->type == typereal)
            width[i].x = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
        if      (token1->type == typeint)
            width[i].y = token1->value.ival;
        else if (token1->type == typereal)
            width[i].y = token1->value.rval;
        else
            error(errtypecheck);
        token1++;
    }
    width[3].x = width[1].x;
    width[3].y = width[2].y;
    width[4].x = width[2].x;
    width[4].y = width[1].y;
    for (i = 0; i < 5; i++)
        dtransform(&width[i], gstate.ctm);
    setcharwidth(&width[0]);
    if (istate.type >= 2)
    {   ll = ur = width[1];
        for (i = 2; i < 5; i++)
        {   point = width[i];
            if (point.x < ll.x) ll.x = point.x;
            if (point.x > ur.x) ur.x = point.x;
            if (point.y < ll.y) ll.y = point.y;
            if (point.y > ur.y) ur.y = point.y;
        }
        setcachedevice(&ll, &ur, 3);
    }
    opernest -= 6;
}

/* setcachelimit */

void opsetcachelimit(void)
{   struct object *token1;
    int limit;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if      (token1->type == typeint)
        limit = token1->value.ival;
    else if (token1->type == typereal)
        limit = itrunc(token1->value.rval);
    else
        error(errtypecheck);
    if (limit > fclen)
        fclimit = fclen;
    else
        fclimit = limit;
    opernest -= 1;
}

/* setcharwidth */

void opsetcharwidth(void)
{   struct object *token1, *token2;
    struct point width;
    if (opernest < 2) error(errstackunderflow);
    token2 = &operstack[opernest - 1];
    token1 = token2 - 1;
    if      (token1->type == typeint)
        width.x = token1->value.ival;
    else if (token1->type == typereal)
        width.x = token1->value.rval;
    else
        error(errtypecheck);
    token1++;
    if      (token1->type == typeint)
        width.y = token1->value.ival;
    else if (token1->type == typereal)
        width.y = token1->value.rval;
    else
        error(errtypecheck);
    dtransform(&width, gstate.ctm);
    setcharwidth(&width);
    opernest -= 2;
}

/* setfont */

void opsetfont(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typedict) error(errtypecheck);
    setfont(token1);
    opernest -= 1;
}

/* show */

void opshow(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    show(token1, 3, NULL, -1, NULL);
    opernest -= 1;
}

/* showpage */

void opshowpage(void)
{   struct object token;
    int num = 1;
    if (istate.flags & intgraph) error(errundefined);
    if (gstate.dev.depth != 0)
    {   if (dictfind(&copies, &token) != -1)
        {   if (token.type != typeint) error(errtypecheck);
            num = token.value.ival;
            if (num < 0) error(errrangecheck);
        }
        copypage(num);
        erasepage();
    }
    initgraphics();
}

/* stringwidth */

void opstringwidth(void)
{   struct object token, *token1;
    struct point width;
    if (opernest < 1) error(errstackunderflow);
    if (opernest + 1 > operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typestring) error(errtypecheck);
    if (token1->flags & flagrprot) error(errinvalidaccess);
    show(token1, 2, &width, -1, NULL);
    idtransform(&width, gstate.ctm);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = width.x;
    token1[0] = token;
    token.value.rval = width.y;
    token1[1] = token;
    opernest += 1;
}

/* stroke */

void opstroke(void)
{   if (istate.flags & intchar)
    {   if      (istate.type == 0)
        {   charpath();
            return;
        }
        else if (istate.type == 1)
        {   flattenpath();
            strokepath(0);
            charpath();
            return;
        }
        else if (istate.type == 2 && !gstate.cacheflag)
        {   gstate.pathend = gstate.pathbeg;
            return;
        }
    }
    flattenpath();
    setupfill();
    strokepath(1);
    gstate.pathend = gstate.pathbeg;
}

/* strokepath */

void opstrokepath(void)
{   flattenpath();
    strokepath(0);
}

/* widthshow */

void opwidthshow(void)
{   struct object *token1, *token2, *token3, *token4;
    struct point width[2];
    int wchar;
    if (opernest < 4) error(errstackunderflow);
    token4 = &operstack[opernest - 1];
    token3 = token4 - 1;
    token2 = token3 - 1;
    token1 = token2 - 1;
    if      (token1->type == typeint)
        width[0].x = token1->value.ival;
    else if (token1->type == typereal)
        width[0].x = token1->value.rval;
    else
        error(errtypecheck);
    if      (token2->type == typeint)
        width[0].y = token2->value.ival;
    else if (token2->type == typereal)
        width[0].y = token2->value.rval;
    else
        error(errtypecheck);
    dtransform(&width[0], gstate.ctm);
    if (token3->type != typeint) error(errtypecheck);
    wchar = token3->value.ival;
    if (wchar < 0 || wchar > 255) error(errrangecheck);
    width[1].x = width[1].y = 0.0;
    if (token4->type != typestring) error(errtypecheck);
    if (token4->flags & flagrprot) error(errinvalidaccess);
    show(token4, 3, width, wchar, NULL);
    opernest -= 4;
}

/* Initialise the operators (4) */

void initop4(void)
{   systemop(oparc,              "arc");
    systemop(oparcn,             "arcn");
    systemop(oparcto,            "arcto");
    systemop(opashow,            "ashow");
    systemop(opawidthshow,       "awidthshow");
    systemop(opcachestatus,      "cachestatus");
    systemop(opcharpath,         "charpath");
    systemop(opclip,             "clip");
    systemop(opclippath,         "clippath");
    systemop(opclosepath,        "closepath");
    systemop(opcolorimage,       "colorimage");
    systemop(opcopypage,         "copypage");
    systemop(opcurrentband,      "currentband");
    systemop(opcurrentfont,      "currentfont");
    systemop(opcurrentpoint,     "currentpoint");
    systemop(opcurveto,          "curveto");
    systemop(opdefinefont,       "definefont");
    systemop(opeoclip,           "eoclip");
    systemop(opeofill,           "eofill");
    systemop(operasepage,        "erasepage");
    systemop(opfill,             "fill");
    systemop(opfindfont,         "findfont");
    systemop(opflattenpath,      "flattenpath");
    systemop(opimage,            "image");
    systemop(opimagemask,        "imagemask");
    systemop(opinitclip,         "initclip");
    systemop(opkshow,            "kshow");
    systemop(oplineto,           "lineto");
    systemop(opmakefont,         "makefont");
    systemop(opmoveto,           "moveto");
    systemop(opnewpath,          "newpath");
    systemop(opnulldevice,       "nulldevice");
    systemop(oppathbbox,         "pathbbox");
    systemop(oppathforall,       "pathforall");
    systemop(oprcurveto,         "rcurveto");
    systemop(opreversepath,      "reversepath");
    systemop(oprlineto,          "rlineto");
    systemop(oprmoveto,          "rmoveto");
    systemop(opscalefont,        "scalefont");
    systemop(opsetband,          "setband");
    systemop(opsetcachedevice,   "setcachedevice");
    systemop(opsetcachelimit,    "setcachelimit");
    systemop(opsetcharwidth,     "setcharwidth");
    systemop(opsetfont,          "setfont");
    systemop(opshow,             "show");
    systemop(opshowpage,         "showpage");
    systemop(opstringwidth,      "stringwidth");
    systemop(opstroke,           "stroke");
    systemop(opstrokepath,       "strokepath");
    systemop(opwidthshow,        "widthshow");
}

/* End of file "postop4.c" */
