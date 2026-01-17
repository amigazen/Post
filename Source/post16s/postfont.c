/* PostScript interpreter file "postfont.c" - type 1 font routines */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* # define FONTDEBUG Enable font debugging (make file does this) */

/* Character string execution data */

# define charmaxdepth    10
# define charstacksize   24

static int charnest, charargn, charend, charaflag;

static float charstack[charstacksize], charargs[charstacksize];

static int charsbwlength, chardotsection, charflexn;

static float charsbw[4], charfptx[7], charfpty[7];
static float charssx, charssy, charsdx, charsdy, charadx, charady;
static float charcpx, charcpy;

/* Default overshoot suppression */

# define defbluescale  0.05

/* Stem value pair
 *   Stems:
 *     type = -1    base fixed
 *             0    floating
 *            +1    top fixed
 *
 *   Blues:
 *     type = -1    blue - base alignment zone
 *            +1    blue - top alignment zone
 */

struct stem
{   int type;
    float s1, s2;
};

# define maxblue         12

static int bluec;
static struct stem bluev[maxblue];

static float bluescale, blueshift, bluefuzz;

# define maxstem         20

static int hstemc, vstemc;
static struct stem hstemv[maxstem], vstemv[maxstem];

/* Stem transform
 *     flag = -1    maps to x only
 *             0    rotated, no hints
 *            +1    maps to y only
 *     type = -1    base/left of stem
 *             0    inside stem
 *            +1    top/right of stem
 */

struct stform
{   int flag, type;
    struct stem *stem;
    float z, fuzz, scale;
};

struct stform hstform, vstform, bstform;

/* Routines */

extern void buildachar(int schar);
extern void charstring(struct object *token1, int depth);
extern void charpoint(struct point *ppoint);
extern struct stem *charstem(struct stform *pstform,
                             struct stem *pstem, int stemc);
extern void charhint(struct point *ppoint, struct stform *pstform,
                     struct stem *pstem, struct stem *pblue);
extern void charfill(void);
extern void charline(struct point *ppoint);
extern void setxbuf(int size);


/* Debugging procedures */

# ifdef FONTDEBUG
# define FDEBUGN(s, n) fdebugn(depth, s, n)
# else
# define FDEBUGN(s, n)
# endif

# ifdef FONTDEBUG

/* On the Amiga we can't redirect stderr, so use stdout instead */

# ifdef AMIGA
# ifdef stderr
# undef stderr
# endif
# define stderr stdout
# endif

/* Debug mode values (bits)
 *    1  Trace charstring commands
 *    2  Trace path points
 *    4  Trace hints
 *    8  Display blues
 */

int fontdebug;

/* setfontdebug */

void opsetfontdebug(void)
{   struct object *token1;
    if (opernest < 1) error(errstackunderflow);
    token1 = &operstack[opernest - 1];
    if (token1->type != typeint) error(errtypecheck);
    fontdebug = token1->value.ival;
    opernest--;
}

/* Operator trace */

void fdebugn(int depth, char *sptr, int n)
{   int i;
    if (fontdebug & 1)
    {   if (depth == 0)
            fprintf(stderr, ">>");
        else
            fprintf(stderr, "%d>", depth);
        fprintf(stderr, " (%4.0f,%4.0f) %2d %-13s ",
                charcpx, charcpy, charnest, sptr);
        for (i = 0; i < n; i++)
            fprintf(stderr, " %4.4g", charstack[i]);
        fprintf(stderr, "\n");
    }
}
# endif

/* Initialise type 1 fonts */

void initfont(void)
{   xbsize = 0;
# ifdef FONTDEBUG
    systemop(opsetfontdebug,     "setfontdebug");
# endif
}

/* Initialise type 1 font build data */

void initbuild(vmref fdref)
{   struct object token, *aptr;
    struct stem blue;
    int i;

    /* Extract the build data */

    fontmetrics = NULL;
    if (dictget(fdref, &charname[charmetrics], &token, 0))
    {   if (token.type != typedict) error(errinvalidfont);
        fontmetrics = token.value.vref;
    }
    if (!dictget(fdref, &charname[charpainttype], &token, 0) ||
        token.type != typeint ||
        (token.value.ival != 0 && token.value.ival != 2))
        error(errinvalidfont);
    fontpainttype = token.value.ival;
    if (fontpainttype != 0)
    {   if (!dictget(fdref, &charname[charstrokewidth], &token, 0))
            error(errinvalidfont);
        if      (token.type == typeint)
            fontstrokewidth = token.value.ival;
        else if (token.type == typereal)
            fontstrokewidth = token.value.rval;
        else
            error(errinvalidfont);
    }
    if (!dictget(fdref, &charname[charcharstrings], &token, 0) ||
        token.type != typedict)
        error(errinvalidfont);
    fontcharstrings = token.value.vref;
    if (!dictget(fdref, &charname[charprivate], &token, 0) ||
        token.type != typedict)
        error(errinvalidfont);
    fontprivate = token.value.vref;
    if (dictget(fontprivate, &charname[charsubrs], &fontsubrs, 0))
    {   if (fontsubrs.type != typearray) error(errinvalidfont);
    }
    else
        fontsubrs.length = 0;
    if (dictget(fontprivate, &charname[charleniv], &token, 0))
    {   if (token.type != typeint || token.value.ival < 0)
            error(errinvalidfont);
        fontleniv = token.value.ival;
    }
    else
        fontleniv = 4;

    /* Extract the blues */

    bluec = 0;
    if (dictget(fontprivate, &charname[charbluevalues], &token, 0))
    {   if (token.type != typearray ||
            token.length > maxblue + 2 || (token.length & 1))
            error(errinvalidfont);
        i = token.length >> 1;
        aptr = vmaptr(token.value.vref);
        blue.type = -1;
        while (i--)
        {   token = *aptr++;
            if (token.type != typeint) error(errinvalidfont);
            blue.s1 = token.value.ival;
            token = *aptr++;
            if (token.type != typeint) error(errinvalidfont);
            blue.s2 = token.value.ival;
            bluev[bluec++] = blue;
            blue.type = 1;
        }
    }
    if (dictget(fontprivate, &charname[charotherblues], &token, 0))
    {   if (token.type != typearray ||
            token.length > maxblue - 2 || (token.length & 1))
            error(errinvalidfont);
        i = token.length >> 1;
        aptr = vmaptr(token.value.vref);
        blue.type = -1;
        while (i--)
        {   token = *aptr++;
            if (token.type != typeint) error(errinvalidfont);
            blue.s1 = token.value.ival;
            token = *aptr++;
            if (token.type != typeint) error(errinvalidfont);
            blue.s2 = token.value.ival;
            bluev[bluec++] = blue;
        }
    }
    bluescale = defbluescale;
    if (dictget(fontprivate, &charname[charbluescale], &token, 0))
    {   if      (token.type == typeint)
            bluescale = token.value.ival;
        else if (token.type == typereal)
            bluescale = token.value.rval;
        else
            error(errinvalidfont);
    }
    blueshift = 7.0;
    if (dictget(fontprivate, &charname[charblueshift], &token, 0))
    {   if      (token.type == typeint)
            blueshift = token.value.ival;
        else
            error(errinvalidfont);
    }
    bluefuzz = 1.0;
    if (dictget(fontprivate, &charname[charbluefuzz], &token, 0))
    {   if      (token.type == typeint)
            bluefuzz = token.value.ival;
        else
            error(errinvalidfont);
    }
# ifdef FONTDEBUG
    if (fontdebug & 8)
    {   fprintf(stderr,   "-> Blues %2d", bluec);
        for (i = 0; i < bluec; i++)
            fprintf(stderr, " [%c,%4.0f,%4.0f]",
                   (bluev[i].type < 0 ? '-' :  '+'),
                   bluev[i].s1, bluev[i].s2);
        fprintf(stderr, "\n-> BlueScale %6.4f BlueShift %2d BlueFuzz %d\n",
                bluescale, (int) blueshift, (int) bluefuzz);
    }
# endif
}

/* Build a character */

void buildchar(int schar)
{   struct object token1, token, *aptr;
    struct point width, ll, ur, *ppoint;
    float fdx, fdy;
    int i;

    /* If the matrix is not rotated we can apply the hints */

    hstform.fuzz = vstform.fuzz = bstform.fuzz = 1.0;
    for (i = 0; i < 4; i++)
        if (fabs(gstate.ctm[i]) < .0001) gstate.ctm[i] = 0.0;
    hstform.flag = 0;
    if      (gstate.ctm[0] == 0.0 && gstate.ctm[2] != 0.0)
    {   hstform.flag = -1;
        hstform.scale = gstate.ctm[2];
    }
    else if (gstate.ctm[1] == 0.0 && gstate.ctm[3] != 0.0)
    {   hstform.flag =  1;
        hstform.scale = gstate.ctm[3];
    }
    vstform.flag = 0;
    if      (gstate.ctm[0] != 0.0 && gstate.ctm[2] == 0.0)
    {   vstform.flag = -1;
        vstform.scale = gstate.ctm[0];
    }
    else if (gstate.ctm[1] != 0.0 && gstate.ctm[3] == 0.0)
    {   vstform.flag =  1;
        vstform.scale = gstate.ctm[1];
    }

    /* Look up the name in the encoding vector */

    if (schar < fontencodlen)
    {   token1 = vmaptr(fontencoding)[schar];
        if (token1.type != typename) error(errinvalidfont);
    }
    else
        token1 = charname[charnotdef];

    /* See if there is a metrics entry for this character */

    charsbwlength = 0;
    charsbw[0] = charsbw[1] = charsbw[2] = charsbw[3] = 0.0;
    if (fontmetrics != NULL && dictget(fontmetrics, &token1, &token, 0))
    {   if (token.type == typearray)
        {   if (token.length != 2 && token.length != 4)
                error(errinvalidfont);
            i = token.length;
            aptr = vmaptr(token.value.vref);
        }
        else
        {   i = 1;
            aptr = &token;
        }
        charsbwlength = i;
        for (i = 0; i < charsbwlength; i++)
        {   token = *aptr++;
            if      (token.type == typeint)
                charsbw[i] = token.value.ival;
            else if (token.type == typereal)
                charsbw[i] = token.value.rval;
            else
                error(errinvalidfont);
        }
        if      (charsbwlength == 1)
        {   charsbw[2] = charsbw[0];
            charsbw[0] = 0.0;
        }
        else if (charsbwlength == 2)
        {   charsbw[2] = charsbw[1];
            charsbw[1] = 0.0;
        }
    }

    /* Execute the character string */

    charnest = charargn = 0;
    charend = charaflag = chardotsection = 0;
    charsdx = charsdy = 0.0;
    charssx = charcpx = 0.0;
    charssy = charcpy = 0.0;
    fdx = gstate.ctm[4];
    fdy = gstate.ctm[5];
    gstate.ctm[4] = 0.0;
    gstate.ctm[5] = 0.0;
    charflexn = -1;
    hstemc = vstemc = 0;
# ifdef FONTDEBUG
    if (fontdebug & 1)
        fprintf(stderr, "/>%.*s\n",
            vmnptr(token1.value.vref)->length,
            vmnptr(token1.value.vref)->string);
# endif
    if (dictget(fontcharstrings, &token1,               &token, 0) ||
        dictget(fontcharstrings, &charname[charnotdef], &token, 0))
        charstring(&token, 0);

    /* Set the character width */

    width.type = 0;
    width.x = charsbw[2];
    width.y = charsbw[3];
    dtransform(&width, gstate.ctm);
    istate.pfcrec->width = width;

    /* For outlined fonts find the stroke path */

    gstate.flatness = 0.2;
    if (fontpainttype != 0)
    {   gstate.linewidth = fontstrokewidth;
        if (istate.type == 1 ||
            istate.type == 2 && gstate.cacheflag ||
            istate.type == 3)
        {   flattenpath();
            strokepath(0);
        }
    }

    /* Calculate the bounding box and see if we can use the cache */

    if (istate.type >= 2)
    {   i = gstate.pathend - gstate.pathbeg;
        ppoint = &patharray[gstate.pathbeg];
        if (i != 0)
        {   ll = ur = *ppoint;
            while (--i)
            {   ppoint++;
                if (ppoint->x < ll.x) ll.x = ppoint->x;
                if (ppoint->x > ur.x) ur.x = ppoint->x;
                if (ppoint->y < ll.y) ll.y = ppoint->y;
                if (ppoint->y > ur.y) ur.y = ppoint->y;
            }
        }
        else
        {   ll.type = 0;
            ll.x = 0.0;
            ll.y = 0.0;
            ur = ll;
        }
        setcachedevice(&ll, &ur, 1);
        if (gstate.cacheflag)
        {   fdx = gstate.ctm[4];
            fdy = gstate.ctm[5];
        }
    }

    /* Adjust all the path coordinates */

    i = gstate.pathend - gstate.pathbeg;
    ppoint = &patharray[gstate.pathbeg];
    while (i--)
    {   ppoint->x += fdx;
        ppoint->y += fdy;
        ppoint++;
    }

    /* Fill the path */

    if      (istate.type < 2)
        charpath();
    else if (istate.type == 2 && !gstate.cacheflag)
        gstate.pathend = gstate.pathbeg;
    else
    {   closepath(ptclosei);
        if (fontpainttype == 0) flattenpath();
        if (gstate.cacheflag)
            charfill();
        else
        {   setupfill();
            fill(gstate.pathbeg, gstate.pathend, -1);
        }
        gstate.pathend = gstate.pathbeg;
    }
}

/* Build an accent or character */

void buildachar(int schar)
{   struct object token1, token;

    /* Look up the name in the standard encoding vector */

    token1 = stdencoding[schar];
    if (token1.type != typename) error(errinvalidfont);

    /* Execute the character string */

    charnest = 0;
    charend = 0;
    chardotsection = 0;
    charssx = charcpx;
    charssy = charcpy;
    hstemc = vstemc = 0;
# ifdef FONTDEBUG
    if (fontdebug & 1)
        fprintf(stderr, "/>%.*s\n",
            vmnptr(token1.value.vref)->length,
            vmnptr(token1.value.vref)->string);
# endif
    if (dictget(fontcharstrings, &token1, &token, 0)) charstring(&token, 0);
}

/* Interpret a character string */

void charstring(struct object *token1, int depth)
{   struct object token;
    struct point point;
    char *sptr;
    float dx1, dy1, dx2, dy2, dx3, dy3, dmin;
    int length, erand, cx, ch, num, i;
    int achar, bchar;

    if (token1->type != typestring) error(errinvalidfont);
    length = token1->length;
    if (length == 0) return;
    sptr = vmsptr(token1->value.vref);

    /* Skip leading random bytes */

    i = fontleniv;
    if (length < i) error(errinvalidfont);
    length -= i;
    erand = einitchar;
    while (i--)
    {   cx = *((unsigned char *) (sptr++));
        erand = ((erand + cx) * ec1 + ec2) & emask;
    }

    /* Interpret */

    while (length--)
    {   cx = *((unsigned char *) (sptr++));
        ch = cx ^ (erand >> eshift);
        erand = ((erand + cx) * ec1 + ec2) & emask;

        /* Operator */

        if      (ch < 32)
        {   switch (ch)
            {   case 21:    /* rmoveto */
                    FDEBUGN("rmoveto", 2);
                    if (charnest < 2) error(errinvalidfont);
                    dx1 = charstack[0];
                    dy1 = charstack[1];
rmoveto:            point.type = ptmove;
movline:            point.x = charcpx = charcpx + dx1;
                    point.y = charcpy = charcpy + dy1;
                    if (charflexn < 0) charpoint(&point);
                    charnest = 0;
                    break;

                case 22:    /* hmoveto */
                    FDEBUGN("hmoveto", 1);
                    if (charnest < 1) error(errinvalidfont);
                    dx1 = charstack[0];
                    dy1 = 0.0;
                    goto rmoveto;

                case  4:    /* vmoveto */
                    FDEBUGN("vmoveto", 1);
                    if (charnest < 1) error(errinvalidfont);
                    dx1 = 0.0;
                    dy1 = charstack[0];
                    goto rmoveto;

                case  5:    /* rlineto */
                    FDEBUGN("rlineto", 2);
                    if (charnest < 2) error(errinvalidfont);
                    dx1 = charstack[0];
                    dy1 = charstack[1];
rlineto:            point.type = ptline;
                    if (gstate.pathbeg == gstate.pathend)
                        error(errinvalidfont);
                    goto movline;

                case  6:    /* hlineto */
                    FDEBUGN("hlineto", 1);
                    if (charnest < 1) error(errinvalidfont);
                    dx1 = charstack[0];
                    dy1 = 0.0;
                    goto rlineto;

                case  7:    /* vlineto */
                    FDEBUGN("vlineto", 1);
                    if (charnest < 1) error(errinvalidfont);
                    dx1 = 0.0;
                    dy1 = charstack[0];
                    goto rlineto;

                case  8:    /* rrcurveto */
                    FDEBUGN("rrcurveto", 6);
                    if (charnest < 6) error(errinvalidfont);
                    dx1 = charstack[0];
                    dy1 = charstack[1];
                    dx2 = charstack[2];
                    dy2 = charstack[3];
                    dx3 = charstack[4];
                    dy3 = charstack[5];
rrcurveto:          point.type = ptcurve;
                    if (gstate.pathbeg == gstate.pathend)
                        error(errinvalidfont);
                    point.x = charcpx + dx1;
                    point.y = charcpy + dy1;
                    charpoint(&point);
                    point.x += dx2;
                    point.y += dy2;
                    charpoint(&point);
                    charcpx = point.x = point.x + dx3;
                    charcpy = point.y = point.y + dy3;
                    charpoint(&point);
                    charnest = 0;
                    break;

                case 30:    /* vhcurveto */
                    FDEBUGN("vhcurveto", 4);
                    if (charnest < 4) error(errinvalidfont);
                    dx1 = 0.0;
                    dy1 = charstack[0];
                    dx2 = charstack[1];
                    dy2 = charstack[2];
                    dx3 = charstack[3];
                    dy3 = 0.0;
                    goto rrcurveto;

                case 31:    /* hvcurveto */
                    FDEBUGN("hvcurveto", 4);
                    if (charnest < 4) error(errinvalidfont);
                    dx1 = charstack[0];
                    dy1 = 0.0;
                    dx2 = charstack[1];
                    dy2 = charstack[2];
                    dx3 = 0.0;
                    dy3 = charstack[3];
                    goto rrcurveto;

                case  9:    /* closepath */
                    FDEBUGN("closepath", 0);
                    closepath(ptclosex);
                    charnest = 0;
                    break;

                case 10:    /* callsubr */
                    FDEBUGN("callsubr", charnest);
                    if (charnest < 1) error(errinvalidfont);
                    i = itrunc(charstack[charnest - 1]);
                    if (i < 0 || i > fontsubrs.length) error(errinvalidfont);
                    charnest--;
                    token = vmaptr(fontsubrs.value.vref)[i];
                    if (depth + 1 == charmaxdepth) error(errinvalidfont);
                    charstring(&token, depth + 1);
                    break;

                case 11:    /* return */
                    FDEBUGN("return", 0);
                    if (depth == 0) error(errinvalidfont);
                    return;

                case 14:    /* endchar */
                    FDEBUGN("endchar", 0);
                    charnest = 0;
                    charend = 1;
                    break;

                case 13:    /* hsbw */
                    FDEBUGN("hsbw", 2);
                    if (charnest < 2) error(errinvalidfont);
                    dx1 = charstack[0];
                    dy1 = 0.0;
                    dx2 = charstack[1];
                    dy2 = 0.0;
sbw:                if      (charaflag == 1)
                    {   charcpx = dx1 + charsdx;
                        charcpy = dy1 + charsdy;
                        charadx += charcpx;
                        charady += charcpy;
                    }
                    else if (charaflag == 2)
                    {   charcpx = charadx;
                        charcpy = charady;
                    }
                    else
                    {   if      (charsbwlength == 0)
                        {   charsbw[0] = dx1;
                            charsbw[1] = dy1;
                            charsbw[2] = dx2;
                            charsbw[3] = dy2;
                        }
                        else if (charsbwlength == 1)
                        {   charsbw[0] = dx1;
                            charsbw[1] = dy1;
                        }
                        charcpx = charsbw[0];
                        charcpy = charsbw[1];
                        charsdx = charcpx - dx1;
                        charsdy = charcpy - dy1;
                    }
                    charssx = charcpx;
                    charssy = charcpy;
                    charnest = 0;
                    break;

                case  1:    /* hstem */
                    FDEBUGN("hstem", 2);
                    if (charnest < 2) error(errinvalidfont);
                    dy1 = charssy + charstack[0];
                    dy2 = dy1 + charstack[1];
                    if (dy1 > dy2)
                    {   dy3 = dy1;
                        dy1 = dy2;
                        dy2 = dy3;
                    }
                    if (hstemc < maxstem)
                    {   hstemv[hstemc].s1 = dy1;
                        hstemv[hstemc].s2 = dy2;
                        hstemc++;
                    }
                    charnest = 0;
                    break;

                case  3:    /* vstem */
                    FDEBUGN("vstem", 2);
                    if (charnest < 2) error(errinvalidfont);
                    dx1 = charssx + charstack[0];
                    dx2 = dx1 + charstack[1];
                    if (dx1 > dx2)
                    {   dx3 = dx1;
                        dx1 = dx2;
                        dx2 = dx3;
                    }
                    if (vstemc < maxstem)
                    {   vstemv[vstemc].s1 = dx1;
                        vstemv[vstemc].s2 = dx2;
                        vstemc++;
                    }
                    charnest = 0;
                    break;

                case 15:    /* moveto  +++ UNDOCUMENTED, OBSOLETE +++ */
                    FDEBUGN("MOVETO", 2);
                    if (charnest < 2) error(errinvalidfont);
                    dx1 = charstack[0] + charssx - charcpx;
                    dy1 = charstack[1] + charssy - charcpy;
                    goto rmoveto;

                case 16:    /* lineto  +++ UNDOCUMENTED, OBSOLETE +++ */
                    FDEBUGN("LINETO", 2);
                    if (charnest < 2) error(errinvalidfont);
                    dx1 = charstack[0] + charssx - charcpx;
                    dy1 = charstack[1] + charssy - charcpy;
                    goto rlineto;

                case 17:    /* curveto +++ UNDOCUMENTED, OBSOLETE +++ */
                    FDEBUGN("CURVETO", 6);
                    if (charnest < 6) error(errinvalidfont);
                    dx1 = charstack[0] + charssx - charcpx;
                    dy1 = charstack[1] + charssy - charcpy;
                    dx2 = charstack[2] + charssx - charcpx;
                    dy2 = charstack[3] + charssy - charcpy;
                    dx3 = charstack[4] + charssx - charcpx;
                    dy3 = charstack[5] + charssy - charcpy;
                    goto rrcurveto;

                case 12:    /* ... */
                    if (length == 0) error(errinvalidfont);
                    length--;
                    cx = *((unsigned char *) (sptr++));
                    ch = cx ^ (erand >> eshift);
                    erand = ((erand + cx) * ec1 + ec2) & emask;
                    switch (ch)
                    {   case  0:    /* dotsection */
                            FDEBUGN("dotsection", 0);
                            chardotsection = !chardotsection;
                            charnest = 0;
                            break;

                        case  1:    /* vstem3 */
                            FDEBUGN("vstem3", 6);
                            if (charnest < 6) error(errinvalidfont);
                            vstemv[0].s1 =
                                charssx + charstack[0];
                            vstemv[0].s2 =
                                charssx + charstack[0] + charstack[1];
                            vstemv[1].s1 =
                                charssx + charstack[2];
                            vstemv[1].s2 =
                                charssx + charstack[2] + charstack[3];
                            vstemv[2].s1 =
                                charssx + charstack[4];
                            vstemv[2].s2 =
                                charssx + charstack[4] + charstack[5];
                            vstemc = 3;
                            charnest = 0;
                            break;

                        case  2:    /* hstem3 */
                            FDEBUGN("hstem3", 6);
                            if (charnest < 6) error(errinvalidfont);
                            hstemv[0].s1 =
                                charssy + charstack[0];
                            hstemv[0].s2 =
                                charssy + charstack[0] + charstack[1];
                            hstemv[1].s1 =
                                charssy + charstack[2];
                            hstemv[1].s2 =
                                charssy + charstack[2] + charstack[3];
                            hstemv[2].s1 =
                                charssy + charstack[4];
                            hstemv[2].s2 =
                                charssy + charstack[4] + charstack[5];
                            hstemc = 3;
                            charnest = 0;
                            break;

                        case  6:    /* seac */
                            FDEBUGN("seac", 5);
                            if (charnest < 5) error(errinvalidfont);
                            bchar = itrunc(charstack[3]);
                            achar = itrunc(charstack[4]);
                            if (charaflag) error(errinvalidfont);
                            charadx = charstack[1];
                            charady = charstack[2];
                            charaflag = 1;
                            charcpx = 0.0;
                            charcpy = 0.0;
                            buildachar(bchar);
                            charaflag = 2;
                            charcpx = charadx;
                            charcpy = charady;
                            buildachar(achar);
                            charnest = 0;
                            charend = 1;
                            break;

                        case  7:    /* sbw */
                            FDEBUGN("sbw", 4);
                            if (charnest < 4) error(errinvalidfont);
                            dx1 = charstack[0];
                            dy1 = charstack[1];
                            dx2 = charstack[2];
                            dy2 = charstack[3];
                            goto sbw;

                        case 12:    /* div */
                            FDEBUGN("div", charnest);
                            if (charnest < 2) error(errinvalidfont);
                            charstack[charnest - 2] /=
                                    charstack[charnest - 1];
                            charnest--;
                            break;

                        case 16:    /* callothersubr */
                            FDEBUGN("callothersubr", charnest);
                            if (charnest < 2) error(errinvalidfont);
                            i = itrunc(charstack[charnest - 1]);
                            num = itrunc(charstack[charnest - 2]);
                            charnest -= 2;
                            if (num < 0 || num > charnest)
                                error(errinvalidfont);
                            charargn = 0;
                            while (num--)
                                charargs[charargn++] = charstack[--charnest];
                            switch (i)
                            {   case 0: /* Flex */
                                    if (charargn != 3)
                                        error(errinvalidfont);
                                    dmin = fabs(charargs[2]) / 100.0;
                                    charargn--;
                                    if (charflexn != 7)
                                        error(errinvalidfont);
                                    charflexn = -1;
                                    point.x = charfptx[3] - charfptx[0];
                                    point.y = charfpty[3] - charfpty[0];
                                    if
                            (fabs(charfptx[3] - charcpx) > 20.0 &&
                             fabs(charfpty[3] - charcpy) > 20.0)
                                        goto flexc;
                                    if
                            (fabs(charfptx[3] - charfptx[6]) > 20.0 &&
                             fabs(charfpty[3] - charfpty[6]) > 20.0)
                                        goto flexc;
                                    dtransform(&point, gstate.ctm);
                                    point.x = fabs(point.x);
                                    point.y = fabs(point.y);
                                    if ((point.x < .01 && point.y < dmin) ||
                                        (point.y < .01 && point.x < dmin))
                                    {   point.type = ptline;
                                        point.x = charfptx[6];
                                        point.y = charfpty[6];
                                        charpoint(&point);
                                        break;
                                    }
flexc:                              point.type = ptcurve;
                                    for (i = 1; i < 7; i++)
                                    {   point.x = charfptx[i];
                                        point.y = charfpty[i];
                                        charpoint(&point);
                                    }
                                    break;

                                case 1: /* Flex */
                                    if (charargn != 0)
                                        error(errinvalidfont);
                                    if (charflexn >= 0)
                                        error(errinvalidfont);
                                    charflexn = 0;
                                    break;

                                case 2: /* Flex */
                                    if (charargn != 0)
                                        error(errinvalidfont);
                                    if (charflexn > 6)
                                        error(errinvalidfont);
                                    charfptx[charflexn] = charcpx;
                                    charfpty[charflexn] = charcpy;
                                    charflexn++;
                                    break;

                                case 3: /* Hint replacement */
                                    hstemc = vstemc = 0;
                                    break;
                            }
                            break;

                        case 17:    /* pop */
                            FDEBUGN("pop", 0);
                            if (charnest == charstacksize || charargn == 0)
                                error(errinvalidfont);
                            charstack[charnest++] = charargs[--charargn];
                            break;

                        case 32:    /* pushcurrentpoint */
                                    /*     +++ UNDOCUMENTED, OBSOLETE +++ */
                            FDEBUGN("PUSHCURRENTPOINT", 0);
                            if (charnest + 2 > charstacksize)
                                error(errinvalidfont);
                            charstack[charnest++] = charcpx;
                            charstack[charnest++] = charcpy;
                            break;

                        case 33:    /* setcurrentpoint */
                            FDEBUGN("setcurrentpoint", 2);
                            if (charnest < 2) error(errinvalidfont);
                            charcpx = charstack[0];
                            charcpy = charstack[1];
                            charnest = 0;
                            break;

                        default:
                            error(errinvalidfont);
                    }
                    break;

                default:
                    error(errinvalidfont);

            }
            if (charend) return;
        }

        /* Number */

        else
        {   if      (ch < 247)
                num = ch - 139;
            else if (ch == 255)
            {   if (length < 4) error(errinvalidfont);
                length -= 4;
                num = 0;
                i = 4;
                while (i--)
                {   cx = *((unsigned char *) (sptr++));
                    ch = cx ^ (erand >> eshift);
                    erand = ((erand + cx) * ec1 + ec2) & emask;
                    num = (num << 8) | ch;
                }
            }
            else
            {   if (length == 0) error(errinvalidfont);
                length--;
                num = ch;
                cx = *((unsigned char *) (sptr++));
                ch = cx ^ (erand >> eshift);
                erand = ((erand + cx) * ec1 + ec2) & emask;
                if (num < 251)
                    num =   (num - 247) * 256 + ch + 108;
                else
                    num = -((num - 251) * 256 + ch + 108);
            }
            if (charnest == charstacksize) error(errinvalidfont);
            charstack[charnest++] = num;
        }
    }
}

/* Add a point to the character path */

void charpoint(struct point *ppoint)
{   struct point point;
    struct stem *pstem, *pblue;

    /* Transform to device space, rounding to multiples of 1/16 */

    point = *ppoint;
    dtransform(&point, gstate.ctm);
    point.x = floor(point.x * 16.0 + 0.5) / 16.0;
    point.y = floor(point.y * 16.0 + 0.5) / 16.0;

    /* See if the point is within a hstem.  If it is, see if it is within an
     * alignment zone.  Even if the point is not within an alignment zone,
     * the stem may be, and we need to know which edge of it is fixed by the
     * alignment.  Then apply any hint */

    if (hstform.flag != 0)
    {   pstem = pblue = NULL;
        hstform.z = ppoint->y;
        if (!chardotsection)
        {   pstem = charstem(&hstform, hstemv, hstemc);
            if (pstem)
            {   pstem->type = 0;
                bstform.z = pstem->s1;
                pblue = charstem(&bstform, bluev, bluec);
                if (pblue && pblue->type < 0)
                    pstem->type = -1;
                else
                {   bstform.z = pstem->s2;
                    pblue = charstem(&bstform, bluev, bluec);
                    if (pblue && pblue->type > 0)
                        pstem->type =  1;
                }
            }
        }
        charhint(&point, &hstform, pstem, pblue);
    }

    /* See if the point is within a vstem.  Then apply any hint */

    if (vstform.flag != 0)
    {   pstem = pblue = NULL;
        vstform.z = ppoint->x;
        if (!chardotsection)
        {   pstem = charstem(&vstform, vstemv, vstemc);
            if (pstem)
                pstem->type = 0;
        }
        charhint(&point, &vstform, pstem, NULL);
    }

# ifdef FONTDEBUG
    if (fontdebug & 2)
        fprintf(stderr, ".> (%4.0f,%4.0f) [%7.2f,%7.2f]\n",
                ppoint->x, ppoint->y, point.x, point.y);
# endif
    if (point.type == ptmove)
    {   closepath(ptclosei);
        if (gstate.pathbeg != gstate.pathend &&
            patharray[gstate.pathend - 1].type == ptmove)
            gstate.pathend--;
    }
    checkpathsize(gstate.pathend + 1);
    patharray[gstate.pathend++] = point;
}

/* See if a coordinate is within a stem */

struct stem *charstem(struct stform *pstform, struct stem *pstem, int stemc)
{   while (stemc--)
    {   if (fabs(pstform->z - pstem->s1) <= pstform->fuzz)
        {   pstform->type = -1; /* Bottom/left */
            return pstem;
        }
        if (fabs(pstform->z - pstem->s2) <= pstform->fuzz)
        {   pstform->type =  1; /* Top/right */
            return pstem;
        }
        if (pstform->z > pstem->s1 && pstform->z < pstem->s2)
        {   pstform->type =  0; /* Inside */
            return pstem;
        }
        pstem++;
    }
    return NULL;
}

/* Apply a hint */

void charhint(struct point *ppoint, struct stform *pstform,
              struct stem *pstem, struct stem *pblue)
{   float z1, z2, s1, s2, s, w1, w2, w, d, f;
    int iw;

    /* Get the coordinate */

    if (pstform->flag < 0)
        z1 = ppoint->x;
    else
        z1 = ppoint->y;
    z2 = z1;

    /* If within a stem calculate its width and regularise it.  Don't
     * adjust very small widths */

    if (pstem)
    {   s1 = pstem->s1 * pstform->scale;
        s2 = pstem->s2 * pstform->scale;
        w1 = fabs(s2 - s1);
        if (w1 < 0.5)
        {  w2 = 0.75;
           iw = 1;
        }
        else
        {  w2 = floor(w1 + 0.5);
           iw = w2;
           w2 -= 0.25;
        }
        if (s1 < s2)
            w =  w2;
        else
            w = -w2;

        /* If a stem boundary falls within an alignment zone then align the
         * stem if the scale is less than BlueScale or if its under/overshoot
         * distance is less then BlueShift and less than half a pixel.
         * Also the coordinate if necessary, also if its device space
         * coordinate is in the alignment zone, due to rounding.  Bottom
         * zones (device space) are aligned at 0.0625, top zones at 0.9375
         */

        if      (pstem->type < 0)
        {   s = pblue->s2 * pstform->scale;
            if (pstform->scale < 0.0)
                s = floor(s - 0.4375) + 0.9375;
            else
                s = floor(s + 0.4375) + 0.0625;
            f = fabs(pstform->scale);
            d = pblue->s2 - pstem->s1;
            if (f < bluescale || (d < blueshift && d * f < 0.5))
                s1 = s;
            else
                s1 = s - floor(d * pstform->scale * 16.0 + 0.5) / 16.0;
            s2 = s1 + w;
            d = pblue->s2 - pstform->z;
            if (pstform->z <= pblue->s2 + bluefuzz)
            {   if (f < bluescale || (d < blueshift && d * f < 0.5))
                    goto align;
            }
            else
                if (pstform->type == 0 &&
                    (pstform->scale < 0.0 ? z2 > s : z2 < s)) goto align;
        }
        else if (pstem->type > 0)
        {   s = pblue->s1 * pstform->scale;
            if (pstform->scale < 0.0)
                s = floor(s + 0.4375) + 0.0625;
            else
                s = floor(s - 0.4375) + 0.9375;
            f = fabs(pstform->scale);
            d = pstem->s2 - pblue->s1;
            if (f < bluescale || (d < blueshift && d * f < 0.5))
                s2 = s;
            else
                s2 = s + floor(d * pstform->scale * 16.0 + 0.5) / 16.0;
            s1 = s2 - w;
            d = pstform->z - pblue->s1;
            if (pstform->z >= pblue->s1 - bluefuzz)
            {   if (f < bluescale || (d < blueshift && d * f < 0.5))
                    goto align;
            }
            else
                if (pstform->type == 0 &&
                    (pstform->scale < 0.0 ? z2 < s : z2 > s)) goto align;
        }

        /* If a stem is not subject to alignment, odd widths are centred
         * on 0.4375 pixels, even widths on 0.9365 pixels. */

        else
        {   if (w2 > 0.5)
            {   s = (s1 + s2) / 2.0;
                if (iw & 1)
                    s = floor(s + 0.0625) + 0.4375;
                else
                    s = floor(s + 0.5625) - 0.0625;
                w = w / 2.0;
                s1 = s - w;
                s2 = s + w;
            }
        }

        /* If we are at one of the stem edges adjust our position to match;
         * otherwise just ensure we remain within the stem */

        if      (pstform->type < 0)
            z2 = s1;
        else if (pstform->type > 0)
            z2 = s2;
        else
        {   if (s1 < s2)
            {   if (z2 < s1) z2 = s1;
                if (z2 > s2) z2 = s2;
            }
            else
            {   if (z2 < s2) z2 = s2;
                if (z2 > s1) z2 = s1;
            }
        }

# ifdef FONTDEBUG
        if (fontdebug & 4 && z1 != z2)
            fprintf(stderr, "=> StemW %7.2f -> %7.2f [%6.2f -> %6.2f]\n",
                    z1, z2, w1, w2);
# endif
        goto setval;

align:
        z2 = s;
# ifdef FONTDEBUG
        if (fontdebug & 4 && z1 != z2)
            fprintf(stderr, "=> Align %7.2f -> %7.2f\n",
                    z1, z2);
# endif
    }

    /* Set the coordinate value */

setval:
    if (pstform->flag < 0)
        ppoint->x = z2;
    else
        ppoint->y = z2;
}

/* Bit packing tables to compress to one bit per pixel without dropouts */

static unsigned char ctbl[256] = /* Bit(s) in center (not edges) */
{   0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, /* 00 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 10 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 20 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 30 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 40 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 50 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 60 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 70 */
    0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, /* 80 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 90 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* a0 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* b0 */
    0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, /* c0 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* d0 */
    0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, /* e0 */
    0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1  /* f0 */
};

static unsigned char ltbl[256] = /* Number of bits at left */
{   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 00 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 10 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 20 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 30 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 40 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 50 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 60 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 70 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 80 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 90 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* a0 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* b0 */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* c0 */
    0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* d0 */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* e0 */
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8  /* f0 */
};

static unsigned char rtbl[256] = /* Number of bits at right */
{   0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* 00 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, /* 10 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* 20 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, /* 30 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* 40 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, /* 50 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* 60 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, /* 70 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* 80 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, /* 90 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* a0 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, /* b0 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* c0 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, /* d0 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, /* e0 */
    0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 8  /* f0 */
};

/* Fill a character path, no clipping needed */

void charfill(void)
{   struct point *ppoint;
    struct line *pline, **ppline;
    char *pptr, *pptr1;         /* device buffer pointers */
    char *xptr1, *xptr2;        /* x buffer pointers */
    int count;                  /* counter */
    int active, discard, sort;  /* lines active, to be discarded, sorted */
    int yy;                     /* current y coordinate */
    int x1, x2, xp, xx;         /* current, previous x position range */
    int fdir;                   /* fill direction counter */
    int mask1, mask2;           /* bit masks for first and last bytes */
    int s0, s1, s2;             /* segment to draw */

    /* Set up the fill lines and y buckets.  Set up the x buffer */

    lineend = 0;
    count = gstate.pathend - gstate.pathbeg;
    ppoint = &patharray[gstate.pathbeg];
    while (count--)
    {   if (ppoint->type != ptmove) charline(ppoint - 1);
        ppoint++;
    }
    setybucket(0, gstate.dev.ysize * 8);
    setxbuf(gstate.dev.xsize);
    memset((char *) xshf, 0, gstate.dev.xsize * sizeof (int));

    /* Fill the area.  Start at the lowest scan line in the path and loop
     * until we reach the highest, rendering 8 bits per pixel in each
     * direction.  Eight bits per pixel horizontally */

    active = discard = sort = 0;
    yy = 0;
    pptr = gstate.dev.buf[0];

    while (yy < gstate.dev.ysize * 8 + 8)
    {   memset(xbuf, 0, gstate.dev.xsize);

if (yy >= gstate.dev.ysize * 8) goto lll;

        /* Add all the new lines */

        pline = ybucket[yy];
        ybucket[yy] = NULL;
        while (pline)
        {   lineptr[active++] = pline;
            pline = pline->chain;
            sort++;
        }

        /* If we have any lines out of order sort them */

        sort += discard;
        if (sort != 0)
        {   count = active;
            for (;;)
            {   count = count / 3 + 1;
                for (x1 = count; x1 < active; x1++)
                    for (x2 = x1 - count;
                         x2 >= 0 &&
                             lineptr[x2]->xx > lineptr[x2 + count]->xx;
                         x2 -= count)
                    {   pline = lineptr[x2];
                        lineptr[x2] = lineptr[x2 + count];
                        lineptr[x2 + count] = pline;
                    }
                if (count == 1) break;
            }
            active -= discard;
            discard = sort = 0;
        }

        /* Scan convert the scan line */

        count = active;
        fdir = 0;
        ppline = &lineptr[0];
        xp = -32767;

        while (count--)
        {   pline = *ppline++;
            x1 = pline->xx >> 16;

            /* At the end of the line draw only its width */

            if (yy == pline->y2)
            {   pline->xx += pline->d2;
                x2 = pline->xx >> 16;
                pline->xx = 0x7fffffff;
                discard++;
                if (x2 < x1)
                {   xx = x1;
                    x1 = x2;
                    x2 = xx;
                }
                if (fdir == 0)
                {   s1 = x1;
                    s2 = x2;
                }
                else
                {   if (x1 < s1) s1 = x1;
                    if (x2 > s2) s2 = x2;
                    continue;
                }
            }

            /* Otherwise draw both the widths and the area enclosed */

            else
            {   if      (yy == pline->y1)
                    pline->xx += pline->d1;  /* Beginning */
                else
                    pline->xx += pline->dx;  /* Middle */
                x2 = pline->xx >> 16;
                if (x2 < xp) sort++;
                    xp = x2;
                if (x2 < x1)
                {   xx = x1;
                    x1 = x2;
                    x2 = xx;
                }
                if      (fdir == 0)          /* Left edge */
                {   fdir += pline->fdir;
                    s1 = x1;
                    s2 = x2;
                    continue;
                }
                if (x1 < s1) s1 = x1;        /* Right edge, or ...  */
                if (x2 > s2) s2 = x2;
                fdir += pline->fdir;
                if      (fdir != 0)          /* Interior */
                    continue;
            }

            /* Draw from s1 to s2 in the x buffer, 8 bits per pixel */

            xptr1 = xbuf + (s1 >> 3);
            xptr2 = xbuf + (s2 >> 3);
            mask1 =  0xff >> (s1 & 7);
            mask2 = ~0xff >> ((s2 & 7) + 1);
            if (xptr1 == xptr2)
                *xptr1 |= (mask1 & mask2);
            else
            {   *xptr1++ |= mask1;
                while (xptr1 != xptr2) *xptr1++ = 0xff;
                *xptr1 |= mask2;
            }
        }

        /* Pack the x buffer horizontally, 8 bits per pixel */

lll:
        xptr1 = xbuf;
        count = gstate.dev.xsize;
        s1 = 0;
        s2 = *((unsigned char *) (xptr1++));
        while (count--)
        {   s0 = s1;
            s1 = s2;
            s2 = *((unsigned char *) (xptr1++));
            xshf[count] <<= 1;
            if (ctbl[s1] ||
                ltbl[s1] > rtbl[s0] ||
                rtbl[s1] != 0 && rtbl[s1] >= ltbl[s2])
                xshf[count] |= 1;
        }

        /* After every 8 lines, pack bits vertically onto the page */

        if (yy > 7 && (yy & 7) == 7)
        {   pptr1 = pptr;
            mask1 = 0x80;
            count = gstate.dev.xsize;
            while (count--)
            {   s2 = xshf[count];
                s0 = (s2 >> 16) & 0xff;
                s1 = (s2 >>  8) & 0xff;
                s2 = s2 & 0xff;
                if (ctbl[s1] ||
                    ltbl[s1] > rtbl[s0] ||
                    rtbl[s1] != 0 && rtbl[s1] >= ltbl[s2])
                    *pptr1 |= mask1;
                if ((mask1 >>= 1) == 0)
                {   mask1 = 0x80;
                    pptr1++;
                }
            }
            pptr += gstate.dev.xbytes;
        }

        yy++;

    }

    ybflag = 0;
}

/* Scale a line ready for filling */

void charline(struct point *ppoint)
{   struct line line;
    double x1, x2, y1, y2, yy;
    int fdir;

    /* We convert the coordinates to fixed point, with a scale factor of
     * 256, rendering 8 bits per pixel */

    x1 = floor(ppoint[0].x * 2048.0 + 0.5);
    x2 = floor(ppoint[1].x * 2048.0 + 0.5);
    y1 = floor(ppoint[0].y * 2048.0 + 0.5);
    y2 = floor(ppoint[1].y * 2048.0 + 0.5);
    
    /* Make y1 <= y2 */

    fdir = 1;
    if (y2 < y1)
    {   fdir = -1;
        yy = x1; x1 = x2; x2 = yy;
        yy = y1; y1 = y2; y2 = yy;
    }

    /* Construct the line.  If it is not horizontal calculate the gradient
     * and the special values of the x displacement for the first and last
     * lines.  The x coordinates are on a scale of 65536  */

    line.cdir = 0;
    line.fdir = fdir;
    line.xx = x1 * 256.0;
    line.y1 = ((int) y1) / 256;
    line.y2 = ((int) y2) / 256;
    if (line.y2 == line.y1)
    {   line.dx = 0;
        line.d1 = 0;
        line.d2 = (x2 - x1) * 256.0;
    }
    else
    {   yy = (x2 - x1) / (y2 - y1) * 256.0;
        line.dx = yy * 256.0;
        line.d1 = ((line.y1 + 1) * 256.0 - y1) * yy;
        line.d2 = (y2 - line.y2 * 256.0) * yy;
    }

    /* Store the line in the line array */

    checklinesize(lineend + 1);
    linearray[lineend++] = line;
}

/* Set up the x buffers */

void setxbuf(int size)
{   int len;
    if (size > xbsize)
    {   len = size * (sizeof (int) + sizeof (char)) + 1;
        xbsize = 0;
        memfree(memxbeg, memxlen);
        memxlen = 0;
        memxbeg = memalloc(len);
        if (memxbeg == NULL) error(errmemoryallocation);
        memxlen = len;
        xshf = memxbeg;
        xbsize = size;
    }
    xbuf = (char *) (xshf + size);
}

/* End of file "postfont.c" */
