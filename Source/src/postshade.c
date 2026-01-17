/* PostScript interpreter file "postshade.c" - colours and halftones */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* Routines */

extern void rangecheck(int n, float *value, int min);
extern void callfunc(struct object *func, float *rp1, float *rp2, int min);
extern int  gcd(int n, int m);

/* Map a colour (from gray/rgb/cmyk to gray/rgb/rgbw) */

void mapcolour(int n1, float *colour1, int n2, float *colour2)
{   float c, m, y, k, u, w;
    if (n2 == 0) return;
    if      (n1 == 1)
        if      (n2 == 1)
            colour2[0] = colour1[0];
        else if (n2 == 3)
        {   k = colour1[0];
            colour2[0] = k;
            colour2[1] = k;
            colour2[2] = k;
        }
        else
        {   k = colour1[0];
            colour2[0] = 1.0;
            colour2[1] = 1.0;
            colour2[2] = 1.0;
            colour2[3] = k;
        }
    else if (n1 == 3)
        if      (n2 == 1)
            colour2[0] =
                0.30 * colour1[0] + 0.59 * colour1[1] + 0.11 * colour1[2];
        else if (n2 == 3)
        {   colour2[0] = colour1[0];
            colour2[1] = colour1[1];
            colour2[2] = colour1[2];
        }
        else
        {   c = 1.0 - colour1[0];
            m = 1.0 - colour1[1];
            y = 1.0 - colour1[2];
            k = c;
            if (m < k) k = m;
            if (y < k) k = y;
            callfunc(&gstate.ucr, &k, &u, -1);
            callfunc(&gstate.gcr, &k, &k, 0);
            c -= u;
            if (c < 0.0) c = 0.0;
            if (c > 1.0) c = 1.0;
            m -= u;
            if (m < 0.0) m = 0.0;
            if (m > 1.0) m = 1.0;
            y -= u;
            if (y < 0.0) y = 0.0;
            if (y > 1.0) y = 1.0;
            colour2[0] = 1.0 - c;
            colour2[1] = 1.0 - m;
            colour2[2] = 1.0 - y;
            colour2[3] = 1.0 - k;
        }
    else
        if      (n2 == 1)
        {   w = 1.0 - colour1[3];
            u = w -
               (0.30 * colour1[0] + 0.59 * colour1[1] + 0.11 * colour1[2]);
            colour2[0] = (u < 0.0) ? 0.0 : u;
        }
        else if (n2 == 3)
        {   w = 1.0 - colour1[3];
            u = w - colour1[0];
            colour2[0] = (u < 0.0) ? 0.0 : u;
            u = w - colour1[1];
            colour2[1] = (u < 0.0) ? 0.0 : u;
            u = w - colour1[2];
            colour2[2] = (u < 0.0) ? 0.0 : u;
        }
        else
        {   colour2[0] = 1.0 - colour1[0];
            colour2[1] = 1.0 - colour1[1];
            colour2[2] = 1.0 - colour1[2];
            colour2[3] = 1.0 - colour1[3];
        }
}

/* Set the current colour */

void setcolour(int n, float *colour)
{   rangecheck(n, colour, 0);
    mapcolour(n, colour, 1, &gstate.gray);
    mapcolour(n, colour, 3, gstate.rgb);
    mapcolour(n, colour, 4, gstate.rgbw);
    gstate.shadeok = 0;
    setupshade();
}

/* Get the current colour in hsb format */

void gethsbcolour(float *colour)
{   float h, s, b, t, cr, cg, cb;
    b = gstate.rgb[0];
    if (gstate.rgb[1] > b) b = gstate.rgb[1];
    if (gstate.rgb[2] > b) b = gstate.rgb[2];
    t = gstate.rgb[0];
    if (gstate.rgb[1] < t) t = gstate.rgb[1];
    if (gstate.rgb[2] < t) t = gstate.rgb[2];
    t = b - t;
    if (b == 0)
        s = 0;
    else
        s = t / b;
    if (s == 0)
        h = 0;
    else
    {   cr = (b - gstate.rgb[0]) / t;
        cg = (b - gstate.rgb[1]) / t;
        cb = (b - gstate.rgb[2]) / t;
        if      (b == gstate.rgb[0])
            h =       cb - cg;
        else if (b == gstate.rgb[1])
            h = 2.0 + cr - cb;
        else
            h = 4.0 + cg - cr;
    }
    h = h / 6.0;
    if (h < 0.0) h += 1.0;
    if (h > 1.0) h -= 1.0;
    colour[0] = h;
    colour[1] = s;
    colour[2] = b;
}

/* Set the current colour in hsb format */

void sethsbcolour(float *hsb)
{   float rgb[3];
    float e, f, h, n, o, p, w;
    rangecheck(3, hsb, 0);
    h = hsb[0] * 6.0;
    e = floor(h);
    f = h - e;
    n = 1.0 - hsb[1];
    o = 1.0 - hsb[1] * f;
    p = 1.0 - hsb[1] * (1.0 - f);
    switch (itrunc(e))
    {   default: rgb[0] = 1.0; rgb[1] = p;   rgb[2] = n;   break;
        case 1:  rgb[0] = o;   rgb[1] = 1.0; rgb[2] = n;   break;
        case 2:  rgb[0] = n;   rgb[1] = 1.0; rgb[2] = p;   break;
        case 3:  rgb[0] = n;   rgb[1] = o;   rgb[2] = 1.0; break;
        case 4:  rgb[0] = p;   rgb[1] = n;   rgb[2] = 1.0; break;
        case 5:  rgb[0] = 1.0; rgb[1] = n;   rgb[2] = o;   break;
    }
    w = hsb[2];
    rgb[0] *= w;
    rgb[1] *= w;
    rgb[2] *= w;
    setcolour(3, rgb);
}

/* Check values are in the range 0.0 to 1.0 (approximately) */

void rangecheck(int n, float *value, int min)
{   float v;
    while (n--)
    {   v = *value;
        if (min == 0)
        {   if      (v <  0.0)
                if (v < -0.0001)
                    error(errrangecheck);
                else
                    *value =  0.0;
        }
        else
        {   if      (v < -1.0)
                if (v < -1.0001)
                    error(errrangecheck);
                else
                    *value = -1.0;
        }
        if (v >  1.0)
            if (v >  1.0001)
                error(errrangecheck);
            else
                *value = 1.0;
        value++;
    }
}

/* Call the transfer function */

void calltransfer(float *colour, float *shade)
{   int plane;
    for (plane = 0; plane < gstate.dev.depth; plane++)
    {   if      (gstate.transdepth == 1)
            callfunc(&gstate.transfer[0], colour, shade, 0);
        else if (gstate.dev.depth == 1)
            callfunc(&gstate.transfer[3], colour, shade, 0);
        else
            callfunc(&gstate.transfer[plane], colour, shade, 0);
        colour++;
        shade++;
    }
}

/* Call a (transfer, ucr, gcr) function */

void callfunc(struct object *func, float *rp1, float *rp2, int min)
{   struct object token, *token1;
    float r;
    int nest;

    if (func->length == 0)
    {   *rp2 = *rp1;
        return;
    }

    if (opernest == operstacksize) error(errstackoverflow);
    token1 = &operstack[opernest];
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = *rp1;
    *token1 = token;
    opernest++;
    nest = opernest;
    pushint();
    istate.flags = intgraph;
    interpret(func);
    popint();
    if (opernest > nest) error(errstackoverflow);
    if (opernest < nest) error(errstackunderflow);
    token = *token1;
    if      (token1->type == typeint)
        r = token1->value.ival;
    else if (token1->type == typereal)
        r = token1->value.rval;
    else
        error(errtypecheck);
    rangecheck(1, &r, min);
    *rp2 = r;
    opernest = nest - 1;
}

/* Set up the halftone shades */

void setupshade(void)
{   if      (gstate.dev.depth == 1)
        calltransfer(&gstate.gray, gstate.shade);
    else if (gstate.dev.depth == 3)
        calltransfer(gstate.rgb, gstate.shade);
    else if (gstate.dev.depth == 4)
        calltransfer(gstate.rgbw, gstate.shade);
    gstate.shadeok = 1;
    screenok = 0;
}

/* Set up the halftone pattern spot arrays */

void setuphalf(void)
{   struct object token, *token1;
    struct halftone *htone;
    char *hbeg, *hptr;
    int hsize, hfree;
    int size, nest;
    int hpsize, hxsize, hysize;
    int harea, hxrep;
    int hdx, hdy;
    short *spotkey;
    float *spotval;
    short *spotsub;
    short *spotnum;
    char **spotptr;
    float anga, cosa, sina, xlen, ylen;
    float rx, ry, cx, cy;
    float spot;
    int ix, iy, ia, ii;
    int xxu, xyu, xxs, xys;
    int yxu, yyu, yxs, yys;
    int plane;

    if (istate.flags & intgraph) error(errundefined);

    /* Divide the available memory equally between the screens */

    hbeg = halfbeg;
    hsize = (halfsize / gstate.screendepth) & ~(mcalign - 1);

    /* Loop through all the screens */

    htone = &halftone[0];
    for (plane = 0; plane < gstate.screendepth; plane++)
    {   hptr = hbeg;
        hfree = hsize;

        /* Calculate the dimensions of a halftone cell.  N.B. density may be
         * different in the x and y directions */

        anga = gstate.screenangle[plane] * degtorad;
        cosa = cos(anga);
        sina = sin(anga);
        xlen = gstate.dev.xden / gstate.screenfreq[plane];
        ylen = gstate.dev.yden / gstate.screenfreq[plane];
        if (xlen < 1.0) xlen = 1.0;
        if (ylen < 1.0) ylen = 1.0;
        xxu = itrunc(fabs(xlen * cosa) + 0.5);
        xyu = itrunc(fabs(xlen * sina) + 0.5);
        yxu = itrunc(fabs(ylen * sina) + 0.5);
        yyu = itrunc(fabs(ylen * cosa) + 0.5);
        xxs = (cosa < 0.0)? -xxu:  xxu;
        xys = (sina < 0.0)?  xyu: -xyu;
        yxs = (sina < 0.0)? -yxu:  yxu;
        yys = (cosa < 0.0)? -yyu:  yyu;

        /* The area of the cell is also the determinant of the transformation
         * matrix.  The pattern repeats in the x and y directions at an
         * interval equal to the area divided by the gcd of the y or x steps.
         * (N.B. gcd(n,0) = n).  Round the x size up to a multiple of 8, not
         * too small */

        harea = xxu * yyu + xyu * yxu;
        hxrep = harea / gcd(yyu, yxu);
        hysize = harea / gcd(xxu, xyu);
        hxsize = hxrep / gcd(hxrep, 8);
        if (hxsize < 20) hxsize *= (20 / hxsize);
        hpsize = hxsize * hysize;

        /* The pattern also repeats at a distance in the y direction, equal
         * to the area divided by the x repeat interval, but displaced
         * sideways.  We calculate this distance so we can fill in the
         * entire pattern without calling the spot function for the same
         * spot twice.  To do so we follow the cell boundaries until we get
         * to the y coordinate we want, keeping track of the x displacement.
         * N.B. dy is calculated using the unsigned values for the cell
         * length, and dx uses the signed values, so when we step dx we have
         * to allow for the actual sign of the dy step */

        hdx = 0;
        ii = harea / hxrep;      /* Where we want to get to */
        if (ii < hysize)
        {   hdy = 0;

            while (hdy != ii)    /* If y too high, step down */
                if (hdy > ii)
                {   hdy -= yyu;
                    if (yys < 0)
                        hdx += xys;
                    else
                        hdx -= xys;
                }
                else             /* Otherwise, step right */
                {   hdy += yxu;
                    if (yxs < 0)
                        hdx -= xxs;
                    else
                        hdx += xxs;
                }

            if (hdx < 0) hdx += hxrep;
        }
        hdy = ii * hxsize;

        /* Allocate the memory for the spot key array.  We don't allocate
         * the spot value array properly, as we will have finished with it
         * before we allocate anything else */

        size = harea * sizeof (short);
        size = (size + (mcalign - 1)) &  ~(mcalign - 1);
        if (size > hfree) error(errlimitcheck);
        spotkey = (short *) hptr;
        hptr += size;
        hfree -= size;
        size = harea * sizeof (float);
        if (size > hfree) error(errlimitcheck);
        spotval = (float *) hptr;

        /* Call the spot function for each point in the cell.  Step on the
         * x coordinate until the pattern repeats, then step on y */

        for (ia = 0, ix = 0, iy = 0; ia < harea; ia++, ix++)
        {   if (ix == hxrep)
            {   ix = 0;
                iy++;
            }

            /* Inverse transform the coordinates from the pattern back to
             * the cell space */

            rx = ix + 0.5;
            ry = iy + 0.5;
            cx = (rx * yys - ry * xys) / harea;
            cy = (ry * xxs - rx * yxs) / harea;
            cx = cx - floor(cx);
            cy = cy - floor(cy);
            cx = cx + cx - 1.0;
            cy = cy + cy - 1.0;

            /* Call the spot function */

            if (opernest + 2 > operstacksize) error(errstackoverflow);
            token1 = &operstack[opernest];
            token.type = typereal;
            token.flags = 0;
            token.length = 0;
            token.value.rval = cx;
            token1[0] = token;
            token.value.rval = cy;
            token1[1] = token;
            opernest += 2;
            nest = opernest - 1;
            pushint();
            istate.flags = intgraph;
            interpret(&gstate.screenfunc[plane]);
            popint();
            if (opernest > nest) error(errstackoverflow);
            if (opernest < nest) error(errstackunderflow);
            token = *token1;
            if      (token1->type == typeint)
                spot = token1->value.ival;
            else if (token1->type == typereal)
                spot = token1->value.rval;
            else
                error(errtypecheck);
            rangecheck(1, &spot, -1);
            opernest = nest - 1;

            /* Save the value in the spot array */

            spotval[ia] = spot;
            spotkey[ia] = ia;
        }

        /* Shell sort the spot array */

        ia = harea;
        for (;;)
        {   ia = ia / 3 + 1;
            for (ix = ia; ix < harea; ix++)
                for (iy = ix - ia;
                     iy >= 0 &&
                         spotval[spotkey[iy]] > spotval[spotkey[iy + ia]];
                     iy -= ia)
                {   ii = spotkey[iy];
                    spotkey[iy] = spotkey[iy + ia];
                    spotkey[iy + ia] = ii;
                }
            if (ia == 1) break;
        }

        /* Initialise the pattern cache.  Make sure we have room for at
         * least one slot (or at least the number of planes) */

        size = (harea + 1) * sizeof (char *);
        if (size > hfree) error(errlimitcheck);
        spotptr = (char **) hptr;
        hptr += size;
        hfree -= size;
        size = (harea + 1) * sizeof (short);
        if (size * 2 > hfree) error(errlimitcheck);
        spotsub = (short *) hptr;
        hptr += size;
        spotnum = (short *) hptr;
        hptr += size;
        hfree -= size * 2;
        if (hpsize *
                ((gstate.screendepth == 1) ? gstate.dev.depth : 1) > hfree)
            error(errlimitcheck);

        for (ia = 0; ia <= harea; ia++)
            spotsub[ia] = -1;

        for (ia = 0; ia <= harea ; ia++)
        {   spotnum[ia] = -1;
            spotptr[ia] = hptr;
            hptr += hpsize;
            hfree -= hpsize;
            if (hfree < hpsize) break;
        }

        /* Store the results in the halftone structure */

        htone->psize = hpsize;
        htone->xsize = hxsize;
        htone->ysize = hysize;
        htone->area = harea;
        htone->xrep = hxrep;
        htone->dx = hdx;
        htone->dy = hdy;
        htone->spotkey = spotkey;
        htone->spotsub = spotsub;
        htone->spotnum = spotnum;
        htone->spotptr = spotptr;
        htone->spotmax = ia;
        htone->spotnext = -1;
        htone++;
        hbeg += hsize;
    }

    halfok = gnest;
    screenok = 0;
}

/* Set up the halftone screens */

void setupscreen(float *shade)
{   struct halfscreen *hscreen;
    struct halftone *htone;
    char *hptr;
    int hnum;
    int hpsize, hxsize;
    int harea, hxrep, hxbits;
    int hdx, hdy;
    int ix, iy, ia, ii;
    int plane;

    screenok = 0;
    hscreen = &halfscreen[0];

    for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
    {   if (gstate.screendepth == 1)
            htone = &halftone[0];
        else if (gstate.dev.depth == 1)
            htone = &halftone[3];
        else
            htone = &halftone[plane];

        /* We never use a halftone for black or white */

        hscreen->halftone = htone;
        hscreen->num = hnum = itrunc(htone->area * (*shade++ + 0.0001));
        if (hnum == 0 || hnum == htone->area) continue;

        /* Look in the cache */

        ii = htone->spotsub[hnum];
        if (ii != -1)
        {   hscreen->ptr = htone->spotptr[ii];
            continue;
        }

        /* Use the next cache slot (cyclic replacement). */

        htone->spotnext++;
        if (htone->spotnext > htone->spotmax) htone->spotnext = 0;
        ii = htone->spotnum[htone->spotnext];
        if (ii != -1) htone->spotsub[ii] = -1;
        htone->spotsub[hnum] = htone->spotnext;
        htone->spotnum[htone->spotnext] = hnum;
        hscreen->ptr = hptr = htone->spotptr[htone->spotnext];

        /* Load the variables from the halftone structure */

        hpsize = htone->psize;
        hxsize = htone->xsize;
        hxbits = hxsize * 8;
        harea = htone->area;
        hxrep = htone->xrep;
        hdx = htone->dx;
        hdy = htone->dy;

        /* Initialise the pattern.  The highest screen is all white. */

        if (hnum == harea)
            memset(hptr, 0x00, hpsize);

        /* We base our pattern on the next lowest one that is in the
         * cache,  or a black screen if we can't find any other */

        else
        {   ia = hnum;
            for (;;)
            {   if (ia == 0)
                {   memset(hptr, 0xff, hpsize);
                    break;
                }
                ia--;
                ii = htone->spotsub[ia];
                if (ii >= 0)
                {   memcpy(hptr, htone->spotptr[ii], hpsize);
                    break;
                }
            }

            /* Loop for each spot value greater than the screen number */

            for (; ia < hnum; ia++)
            {
                /* Clear the corresponding bit in each cell */

                ii = htone->spotkey[ia];

                /* Loop through all the rows containing the bit */

                iy = (ii / hxrep) * hxsize;
                ii = ii % hxrep;
                while (iy < hpsize)
                {
                    /* Loop replicating the bit it up to the x size */

                    for (ix = ii; ix < hxbits; ix += hxrep)
                        hptr[iy + (ix >> 3)] &= (~0x0080) >> (ix & 7);

                    /* Step on to the next row */

                    ii += hdx;
                    if (ii >= hxrep) ii -= hxrep;
                    iy += hdy;
                }
            }
        }
    }
}

/* Find the greatest common divisor of two positive integers.  If one is
 * zero the result is the other */

int gcd(int n, int m)
{   unsigned int n1, m1, r;
    if      (n > m)
    {   n1 = n;
        m1 = m;
    }
    else if (m > n)
    {   n1 = m;
        m1 = n;
    }
    else
        return n;
    while (m1 != 0)
    {   r = n1 % m1;
        n1 = m1;
        m1 = r;
    }
    return (int) n1;
}

/* End of file "postshade.c" */
