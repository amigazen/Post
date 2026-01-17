/* PostScript interpreter file "postfont.c" - type 1 font routines
 * (C) Adrian Aylward 1989, 1992
 * V1.6 First source release
 * V1.7 Rewrite hinting to interpolate between stems
 * V1.7 Allow real values of blueshift and bluefuzz
 */

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

static struct point lastpoint;

/* Default overshoot suppression */

# define defbluescale  0.05

/* Blues and Stems */

# define maxstem         20

struct stem
{   int type;
    float s1, s2;
    float z1, z2;
};

struct stform
{   int flag;
    float scale;
    int stemc;
    struct stem stemv[maxstem];
};

static struct stform hstform, vstform;

# define maxblue         12

static int bluec;
static struct stem bluev[maxblue];

static float bluescale, blueshift, bluefuzz;

/* Routines */

extern void buildachar(int schar);
extern void charstring(struct object *token1, int depth);
extern void charpoint(struct point *ppoint);
extern void hintpoint(struct point *cpoint, struct point *ppoint);
extern void hintadjust(float *ps, float *pz, struct stform *pstform);
extern void setstems(struct stform *pstform, int num);
extern void setblues(void);
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
        else if (token.type == typereal)
            blueshift = token.value.rval;
        else
            error(errinvalidfont);
    }
    bluefuzz = 1.0;
    if (dictget(fontprivate, &charname[charbluefuzz], &token, 0))
    {   if      (token.type == typeint)
            bluefuzz = token.value.ival;
        else if (token.type == typereal)
            bluefuzz = token.value.rval;
        else
            error(errinvalidfont);
    }
# ifdef FONTDEBUG
    if (fontdebug & 8)
        fprintf(stderr, "-> BlueScale %6.4f BlueShift %6.4f BlueFuzz %6.4f\n",
                bluescale, blueshift, bluefuzz);
# endif
}

/* Build a character */

void buildchar(int schar)
{   struct object token1, token, *aptr;
    struct point width, ll, ur, *ppoint;
    float fdx, fdy;
    int i;

    /* If the matrix is not rotated we can apply the hints */

    for (i = 0; i < 4; i++)
        if (fabs(gstate.ctm[i]) < .0001) gstate.ctm[i] = 0.0;
    hstform.flag = 0;
    if      (gstate.ctm[0] != 0.0 && gstate.ctm[2] == 0.0)
    {   hstform.flag = -1;
        hstform.scale = gstate.ctm[0];
    }
    else if (gstate.ctm[1] != 0.0 && gstate.ctm[3] == 0.0)
    {   hstform.flag =  1;
        hstform.scale = gstate.ctm[1];
    }
    vstform.flag = 0;
    if      (gstate.ctm[0] == 0.0 && gstate.ctm[2] != 0.0)
    {   vstform.flag = -1;
        vstform.scale = gstate.ctm[2];
    }
    else if (gstate.ctm[1] == 0.0 && gstate.ctm[3] != 0.0)
    {   vstform.flag =  1;
        vstform.scale = gstate.ctm[3];
    }
    setblues();

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
    lastpoint.type = 0;
    hstform.stemc = vstform.stemc = 0;
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
    lastpoint.type = 0;
    hstform.stemc = vstform.stemc = 0;
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
                    if (vstform.stemc < maxstem)
                    {   vstform.stemv[vstform.stemc].s1 = dy1;
                        vstform.stemv[vstform.stemc].s2 = dy2;
                        setstems(&vstform, 1);
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
                    if (hstform.stemc < maxstem)
                    {   hstform.stemv[hstform.stemc].s1 = dx1;
                        hstform.stemv[hstform.stemc].s2 = dx2;
                        setstems(&hstform, 1);
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
                            hstform.stemc = 0;
                            hstform.stemv[0].s1 =
                                charssx + charstack[0];
                            hstform.stemv[0].s2 =
                                charssx + charstack[0] + charstack[1];
                            hstform.stemv[1].s1 =
                                charssx + charstack[2];
                            hstform.stemv[1].s2 =
                                charssx + charstack[2] + charstack[3];
                            hstform.stemv[2].s1 =
                                charssx + charstack[4];
                            hstform.stemv[2].s2 =
                                charssx + charstack[4] + charstack[5];
                            setstems(&hstform, 3);
                            charnest = 0;
                            break;

                        case  2:    /* hstem3 */
                            FDEBUGN("hstem3", 6);
                            if (charnest < 6) error(errinvalidfont);
                            vstform.stemc = 0;
                            vstform.stemv[0].s1 =
                                charssy + charstack[0];
                            vstform.stemv[0].s2 =
                                charssy + charstack[0] + charstack[1];
                            vstform.stemv[1].s1 =
                                charssy + charstack[2];
                            vstform.stemv[1].s2 =
                                charssy + charstack[2] + charstack[3];
                            vstform.stemv[2].s1 =
                                charssy + charstack[4];
                            vstform.stemv[2].s2 =
                                charssy + charstack[4] + charstack[5];
                            setstems(&vstform, 3);
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
                                    hstform.stemc = vstform.stemc = 0;
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

    /* Transform to device space, rounding to multiples of 1/16 */

    point = *ppoint;
    dtransform(&point, gstate.ctm);
    point.x = floor(point.x * 16.0 + 0.5) / 16.0;
    point.y = floor(point.y * 16.0 + 0.5) / 16.0;

# ifdef FONTDEBUG
    if (fontdebug & 2)
        fprintf(stderr, ".> (%4.0f,%4.0f) [%7.2f,%7.2f]\n",
                ppoint->x, ppoint->y, point.x, point.y);
# endif

    /* For moves, the hinting is deferred, in case the hints have changed */

    if (lastpoint.type == ptmove)
        hintpoint(&lastpoint, &patharray[gstate.pathend - 1]);
    lastpoint = *ppoint;

    /* Add the point to the path */

    if (point.type == ptmove)
    {   closepath(ptclosei);
        if (gstate.pathbeg != gstate.pathend &&
            patharray[gstate.pathend - 1].type == ptmove)
            gstate.pathend--;
    }
    checkpathsize(gstate.pathend + 1);
    patharray[gstate.pathend++] = point;

    /* For points other than a move, the hinting is done immediately */

    if (lastpoint.type != ptmove)
        hintpoint(&lastpoint, &patharray[gstate.pathend - 1]);
}

/* Adjust the coordinates of a point according to the hints */

void hintpoint(struct point *cpoint, struct point *ppoint)
{   if      (hstform.flag < 0)
        hintadjust(&cpoint->x, &ppoint->x, &hstform);
    else if (hstform.flag > 0)
        hintadjust(&cpoint->x, &ppoint->y, &hstform);
    if      (vstform.flag < 0)
        hintadjust(&cpoint->y, &ppoint->x, &vstform);
    else if (vstform.flag > 0)
        hintadjust(&cpoint->y, &ppoint->y, &vstform);
}

/* Adjust a single coordinate according to the hints */

void hintadjust(float *ps, float *pz, struct stform *pstform)
{   struct stem *stem1, *stem2, *pstem, *pblue;
    float ss, zz, f, d;
    int i;

    if (chardotsection) return;

    /* Locate it relative to the stems.  Loop to find the highest stem not
     * above us and the lowest not below us.  Then we can work out where
     * we are */

    stem1 = stem2 = NULL;
    ss = *ps;
    pstem = &pstform->stemv[0];
    i = pstform->stemc;

    while (i--)
    {   if (pstem->s1 <= ss)
            if (stem1 == NULL || pstem->s1 > stem1->s1) stem1 = pstem;
        if (pstem->s2 >= ss)
            if (stem2 == NULL || pstem->s2 < stem2->s2) stem2 = pstem;
        pstem++;
    }

    /* If it is not within a stem, see if it is within an alignment zone */

    if (stem1 == NULL || stem2 == NULL)
        if (pstform == &vstform)
        {   pblue = bluev;
            i = bluec;
            while (i--)
                if (ss >= pblue->s1 - bluefuzz && ss <= pblue->s2 + bluefuzz)
                {   f = fabs(pstform->scale);
                    if (pblue->type < 0)
                    {   d = pblue->s2 - ss;
                        zz = pblue->z2;
                    }
                    else
                    {   d = ss - pblue->s1;
                        zz = pblue->z1;
                    }
                    if (f < bluescale || (d < blueshift && d * f < 0.5))
                        goto hint;
                }
        }

    /* No stems */

    if (stem1 == NULL && stem2 == NULL) return;

    /* Above all stems */

    if (stem1 == NULL)
    {   zz = stem2->z2 + (ss - stem2->s2) * pstform->scale;
        goto hint;
    }

    /* Below all stems */

    if (stem2 == NULL)
    {   zz = stem1->z1 + (ss - stem1->s1) * pstform->scale;
        goto hint;
    }

    /* In case we have averlapping stems */

    if (stem2->s1 == stem1->s2) return;

    /* Between two stems */

    if (stem1 != stem2)
    {   zz = stem1->z2 +
                 (ss - stem1->s2) *
                 (stem2->z1 - stem1->z2) /
                 (stem2->s1 - stem1->s2);
        goto hint;
    }

    /* At the bottom of a stem */

    if (stem1->s1 == ss)
    {   zz = stem1->z1;
        goto hint;
    }

    /* At the top of a stem */

    if (stem1->s2 == ss)
    {   zz = stem1->z2;
        goto hint;
    }

    /* Within a stem */

    zz = stem1->z1 +
             (ss - stem1->s1) *
             (stem1->z2 - stem1->z1) /
             (stem1->s2 - stem1->s1);

hint:
    zz = floor(zz * 16.0 + 0.5) / 16.0;

# ifdef FONTDEBUG
    if (fontdebug & 4)
        fprintf(stderr, "=> Hint %c  %6.2f -> %6.2f\n",
                (pstform == &hstform ? 'X' : 'Y'), *pz, zz);
# endif

    *pz = zz;
}

/* Set the coordinates of the stems */

void setstems(struct stform *pstform, int num)
{   struct stem *pstem, *pblue;
    float scale, w1, w2, z1, z2, zz, w, f, d;
    int iw, i;

    if (pstform->flag == 0) return;

    scale = pstform->scale;
    pstem = &pstform->stemv[pstform->stemc];
    pstform->stemc += num;

    while (num--)
    {

        /* Regularise the width.  Don't adjust very small widths */

        z1 = pstem->s1 * scale;
        z2 = pstem->s2 * scale;
        w1 = fabs(z2 - z1);
        if (w1 < 0.5)
        {  w2 = 0.75;
           iw = 1;
        }
        else
        {  w2 = floor(w1 + 0.5);
           iw = w2;
           w2 -= 0.25;
        }
        if (z1 < z2)
            w =  w2;
        else
            w = -w2;

        /* If a stem is not subject to alignment, odd widths are centred
         * on 0.4375 pixels, even widths on 0.9365 pixels. */

        if (w2 > 0.5)
        {   zz = (z1 + z2) / 2.0;
            if (iw & 1)
                zz = floor(zz + 0.0625) + 0.4375;
            else
                zz = floor(zz + 0.5625) - 0.0625;
            z1 = zz - w / 2.0;
            z2 = zz + w / 2.0;
        }

        /* See if the stem is within an alignment zone.  If it is within
         * bluefuzz of exact alignment then align it.  If it is within the
         * alignment zone than align it if our scale is less than bluescale
         * or the distance is less than blueshift and less than half a pixel;
         * otherwise position relative to the alignment.  If the stem is
         * outside the alignment zone make sure it remains so
         */

        if (pstform == &vstform)
        {   pblue = bluev;
            i = bluec;
            while (i--)
            {   if (pblue->type < 0)
                {   if (fabs(pstem->s1 - pblue->s2) <= bluefuzz)
                    {   z1 = pblue->z2;
                        z2 = z1 + w;
                        break;
                    }
                    if (pstem->s1 < pblue->s2 &&
                        pstem->s1 >= pblue->s1 - bluefuzz)
                    {   z1 = pblue->z2;
                        f = fabs(scale);
                        d = pblue->s2 - pstem->s1;
                        if (!(f < bluescale ||
                              (d < blueshift && d * f < 0.5)))
                            z1 -= floor(d * scale * 16.0 + 0.5) / 16.0;
                        z2 = z1 + w;
                        break;
                    }
                    if (pstem->s1 > pblue->s2)
                        if (scale < 0 ? (z1 > pblue->z2) : (z1 < pblue->z2))
                        {   z1 = pblue->z2;
                            z2 = z1 + w;
                        }
                }
                else
                {   if (fabs(pstem->s2 - pblue->s1) <= bluefuzz)
                    {   z2 = pblue->z1;
                        z1 = z2 - w;
                        break;
                    }
                    if (pstem->s2 > pblue->s1 &&
                        pstem->s2 <= pblue->s2 + bluefuzz)
                    {   z2 = pblue->z1;
                        f = fabs(scale);
                        d = pstem->s2 - pblue->s1;
                        if (!(f < bluescale ||
                              (d < blueshift && d * f < 0.5)))
                            z2 += floor(d * scale * 16.0 + 0.5) / 16.0;
                        z1 = z2 - w;
                        break;
                    }
                    if (pstem->s2 < pblue->s1)
                        if (scale < 0 ? (z2 < pblue->z1) : (z2 > pblue->z1))
                        {   z2 = pblue->z1;
                            z1 = z2 - w;
                        }
                }
                pblue++;
            }
        }

        pstem->z1 = z1;
        pstem->z2 = z2;

# ifdef FONTDEBUG
        if (fontdebug & 4)
            fprintf(stderr, "=> Stem %c [%7.2f, %7.2f] -> [%6.2f, %6.2f]\n",
                    (pstform == &vstform ? 'H' : 'V'),
                    pstem->s1, pstem->s2, z1, z2);
# endif

        pstem++;
    }
}

/* Set the coordinates of the blues */

void setblues(void)
{   struct stem *pblue;
    float zz;
    int i;

    if (vstform.flag == 0) return;

    pblue = bluev;
    i = bluec;
    while (i--)
    {
        /* Bottom zones are aligned at 0.0625, top zones at 0.9375 */

        if (pblue->type < 0)
        {   zz = pblue->s2 * vstform.scale;
            if (vstform.scale < 0.0)
                zz = floor(zz - 0.4375) + 0.9375;
            else
                zz = floor(zz + 0.4375) + 0.0625;
        }
        else
        {   zz = pblue->s1 * vstform.scale;
            if (vstform.scale < 0.0)
                zz = floor(zz + 0.4375) + 0.0625;
            else
                zz = floor(zz - 0.4375) + 0.9375;
        }

        pblue->z1 = pblue->z2 = zz;

# ifdef FONTDEBUG
        if (fontdebug & 8)
            fprintf(stderr, "=> Blue %c [%7.2f, %7.2f] -> %6.2f\n",
                    (pblue->type < 0 ? '-' : '+'),
                    pblue->s1, pblue->s2, zz);
# endif

        pblue++;
    }
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
