/* PostScript interpreter file "postdraw.c" - path fill and images */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* Routines */

extern void fillslow(int yy, int y2, int rule);
extern void fillfast(int yy, int y2, int rule);
extern void fillline(struct point *ppoint, int cdir, int fdir);
extern void fillxyxy(double *xy1, double *xy2, int cdir, int fdir);
extern void imageslow(int xpos1, int ypos1, int xpos2, int ypos2);
extern void imagefast(int xpos1, int ypos1, int blen);

/* Set up the halftone screens before filling a path */

void setupfill(void)
{   if (istate.flags & intgraph) error(errundefined);

    if (halfok > gnest) setuphalf();

    if (gstate.shadeok == 0) setupshade();

    if (screenok == 0)
    {   setupscreen(gstate.shade);
        screenok = 1;
    }
}

/* Fill a path */

void fill(int beg, int end, int rule)
{   struct point *ppoint;
    int count, y2, y1;

    /* Set up the fill and clip lines.  Clipping in the x direction merely
     * prevents integer overflow.  Clipping in the y direction minimises the
     * number of scan lines we need to process.  We clip the fill path first,
     * using the fixed limits.  Then we clip the clipping path, using the
     * minimum and maximum y values from the fill path.  This optimises the
     * common case where the fill path fits entirely within the clip path.
     * We can't clip to the exact page width in the x direction, as we always
     * draw the line widths, so we could get spurious lines along the left
     * or right edges.  In the y direction we clip to the exact page size;
     * we assume that integers are represented exactly in floating point */

    ymax = ylwb = gstate.dev.ybase * 256.0;
    ymin = yupb = (gstate.dev.ybase + gstate.dev.ysize) * 256.0;
    lineend = 0;
    count = end - beg;
    ppoint = &patharray[beg];
    while (count--)
    {   if (ppoint->type != ptmove) fillline(ppoint - 1, 0, 1);
        ppoint++;
    }
    if (lineend == 0) return;

    /* Determine the vertical limits of the fill */

    y1 = ((int) ymin) >> 8;
    y2 = (((int) ymax) >> 8) + 1;
    if (y2 > gstate.dev.ybase + gstate.dev.ysize)
        y2 = gstate.dev.ybase + gstate.dev.ysize;

    /* Only process the clip lines if we have a non-standard clip path */

    if (gstate.clipflag)
    {   ylwb = y1 * 256.0;
        yupb = y2 * 256.0;
        count = gstate.pathbeg - gstate.clipbeg;
        ppoint = &patharray[gstate.clipbeg];
        while (count--)
        {   if (ppoint->type != ptmove) fillline(ppoint - 1, 1, 0);
            ppoint++;
        }
    }

    /* Set up the y buckets, and do the fill */

    setybucket(gstate.dev.ybase, gstate.dev.ysize);
    if (gstate.clipflag)
        fillslow(y1, y2, rule);
    else
        fillfast(y1, y2, rule);
    ybflag = 0;

    flushlpage(y1, y2);
}

/* Slow fill, has to handle clip lines too */

void fillslow(int yy, int y2, int rule)
{   struct line *pline, **ppline;
    struct lineseg lineseg, *plineseg;
    struct halfscreen *hscreen;
    struct halftone *htone;
    char *dptr1, *dptr2;        /* device buffer pointers */
    char *hbeg, *hptr;          /* halftone screen row base, pointer */
    int flag, count, segments;  /* in-out flag, counter, segments */
    int active, discard, sort;  /* lines active, to be discarded, sorted */
    int x1, x2, xp, xx;         /* current, previous x position range */
    int cdir, fdir, sdir;       /* clip, fill direction counters */
    int poff;                   /* offset of line from page */
    int xmod, hxsize;           /* position modulo halftone screen */
    int mask1, mask2;           /* bit masks for first and last bytes */
    int xbyt1, xbyt2;           /* bytes offsets from beginning of line */
    int plane;

    /* Fill the area.  Start at the lowest scan line in the path and loop
     * until we reach the highest */

    active = discard = sort = 0;
    poff = (yy - gstate.dev.ybase) * gstate.dev.xbytes;

    while (yy < y2)
    {
        /* Add all the new lines */

        pline = ybucket[yy - gstate.dev.ybase];
        ybucket[yy - gstate.dev.ybase] = NULL;
        while (pline)
        {   lineptr[active++] = pline;
            pline = pline->chain;
            sort++;
        }

        /* If we have any lines out of order or (new, being discarded) then
         * we Shell sort the lines according to their current x coordinates.
         * Any previously finished lines have large x coordinates so will be
         * moved to the end of the array where they are discarded */

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
        cdir = fdir = 0;
        ppline = &lineptr[0];
        plineseg = linesegarray;
        segments = 0;
        xp = -32767;

        while (count--)
        {   pline = *ppline++;
            x1 = pline->xx >> 16;

            /* At the end of the line (or if it is horizontal), use the
             * special value of the x increment.  Flag it to be discarded.
             * Draw only its width. We build a list of line segments to be
             * drawn, creating entries for both the line width itself and
             * (when we are not at the end) the area enclosed.  (Score
             * 1 for the beginning of segment and -1 for the end */

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
                x2++;

                if (pline->cdir != 0)
                {   lineseg.fdir = 0;
                    lineseg.cdir =  1;
                    lineseg.x = x1;
                    *plineseg++ = lineseg;
                    lineseg.cdir = -1;
                    lineseg.x = x2;
                    *plineseg++ = lineseg;
                    segments += 2;
                }
                else
                {   lineseg.cdir = 0;
                    lineseg.fdir =  1;
                    lineseg.x = x1;
                    *plineseg++ = lineseg;
                    lineseg.fdir = -1;
                    lineseg.x = x2;
                    *plineseg++ = lineseg;
                    segments += 2;
                }
            }

            /* At the beginning of the line, use the special values of the
             * x increment; otherwise add the gradient to its current x
             * coordinate.  We have to draw both the lines widths and the
             * area enclosed.  Build the segment list; if two endpoints are
             * coincident add the scores.  For left edges we start drawing
             * at the left of the line and draw the width too; for right
             * edges we stop drawing at the right of the line. For interior
             * lines we draw the line width (as it may not be interior
             * higher up in the pixel */

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
                x2++;

                if (pline->cdir != 0)
                {   lineseg.fdir = 0;
                    sdir = cdir;
                    cdir += pline->cdir;
                    if      ((sdir & rule) == 0) /* Left edge */
                    {   lineseg.cdir =  2;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        lineseg.cdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 2;
                    }
                    else if ((cdir & rule) == 0) /* Right edge */
                    {   lineseg.cdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 1;
                    }
                    else if (x1 != x2)           /* Interior, draw width */
                    {   lineseg.cdir =  1;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        lineseg.cdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 2;
                    }
                }
                else
                {   lineseg.cdir = 0;
                    sdir = fdir;
                    fdir += pline->fdir;
                    if      ((sdir & rule) == 0) /* Left edge */
                    {   lineseg.fdir =  2;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        lineseg.fdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 2;
                    }
                    else if ((fdir & rule) == 0) /* Right edge */
                    {   lineseg.fdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 1;
                    }
                    else if (x1 != x2)           /* Interior, draw width */
                    {   lineseg.fdir =  1;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        lineseg.fdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 2;
                    }
                }
            }
        }

        /* Sort the line segment list.  It should be almost in order, so
         * a simple sort is probably optimal */

        xx = 0;
        while (xx < segments - 1)
        {   flag = 0;
            count = segments - 1 - xx;
            plineseg = linesegarray;
            while (count--)
            {   if (plineseg->x > (plineseg + 1)->x)
                {   flag = 1;
                    lineseg = *plineseg;
                    *plineseg = *(plineseg + 1);
                    *(plineseg + 1) = lineseg;
                }
                plineseg++;
            }
            if (flag == 0) break;
            xx++;
        }

        /* Scan the list, drawing line segments as we find them */

        flag = 0;
        cdir = fdir = 0;
        for (plineseg = linesegarray; segments--; plineseg++)
        {   cdir += plineseg->cdir;
            fdir += plineseg->fdir;
            if (flag == 0)
            {   if (cdir && fdir)
                {   flag = 1;
                    x1 = plineseg->x;
                }
                continue;
            }
            else
            {   if (cdir && fdir)
                    continue;
                else
                {   flag = 0;
                    x2 = plineseg->x;
                }
            }
            if (x1 < 0) x1 = 0;
            if (x2 > gstate.dev.xsize) x2 = gstate.dev.xsize;
            if (x1 >= x2) continue;

            /* Draw from x1 to x2 */

            xbyt1 = x1 >> 3;
            xbyt2 = (x2 - 1) >> 3;
            mask1 =  0xff >> (x1 & 7);
            mask2 = ~0xff >> (((x2 - 1) & 7) + 1);

            /* Loop through the bit planes */

            hscreen = &halfscreen[0];
            for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
            {   htone = hscreen->halftone;
                dptr1 = gstate.dev.buf[plane] + poff + xbyt1;
                dptr2 = gstate.dev.buf[plane] + poff + xbyt2;

                /* Optimise black or white */

                if      (hscreen->num == 0)
                {   if (dptr1 == dptr2)
                        *dptr1 |= (mask1 & mask2);
                    else
                    {   *dptr1++ |= mask1;
                        while (dptr1 != dptr2) *dptr1++ = 0xff;
                        *dptr1 |= mask2;
                    }
                }
                else if (hscreen->num == htone->area)
                {   if (dptr1 == dptr2)
                        *dptr1 &= ~(mask1 & mask2);
                    else
                    {   *dptr1++ &= ~mask1;
                        while (dptr1 != dptr2) *dptr1++ = 0x00;
                        *dptr1 &= ~mask2;
                    }
                }

                /* The general case needs a halftone screen */

                else
                {   xmod = xbyt1 % htone->xsize;
                    hbeg = hscreen->ptr +
                        (yy % htone->ysize) * htone->xsize;
                    hptr = hbeg + xmod;
                    hxsize = htone->xsize;
                    if (dptr1 == dptr2)
                    {   mask1 &= mask2;
                        *dptr1 = (*dptr1 & ~mask1) | (*hptr & mask1);
                    }
                    else
                    {   *dptr1 = (*dptr1 & ~mask1) | (*hptr & mask1);
                        dptr1++;
                        hptr++;
                        for (;;)
                        {   xmod++;
                            if (xmod == hxsize)
                            {   xmod = 0;
                                hptr = hbeg;
                            }
                            if (dptr1 == dptr2) break;
                            *dptr1++ = *hptr++;
                        }
                        *dptr1 = (*dptr1 & ~mask2) | (*hptr & mask2);
                    }
                }
            }
        }

        /* Continue with the next scan line */

        poff += gstate.dev.xbytes;
        yy++;
    }
}

/* Fast fill a path, no clipping needed */

void fillfast(int yy, int y2, int rule)
{   struct line *pline, **ppline;
    struct halfscreen *hscreen;
    struct halftone *htone;
    char *dptr1, *dptr2;        /* device buffer pointers */
    char *hbeg, *hptr;          /* halftone screen row base, pointer */
    int count;                  /* counter */
    int active, discard, sort;  /* lines active, to be discarded, sorted */
    int x1, x2, xp, xx;         /* current, previous x position range */
    int fdir;                   /* fill direction counter */
    int poff;                   /* offset of line from page */
    int xmod, hxsize;           /* position modulo halftone screen */
    int mask1, mask2;           /* bit masks for first and last bytes */
    int xbyt1, xbyt2;           /* bytes offsets from beginning of line */
    int s1, s2;                 /* segment to draw */
    int plane;

    /* Fill the area.  Start at the lowest scan line in the path and loop
     * until we reach the highest */

    active = discard = sort = 0;
    poff = (yy - gstate.dev.ybase) * gstate.dev.xbytes;

    while (yy < y2)
    {
        /* Add all the new lines */

        pline = ybucket[yy - gstate.dev.ybase];
        ybucket[yy - gstate.dev.ybase] = NULL;
        while (pline)
        {   lineptr[active++] = pline;
            pline = pline->chain;
            sort++;
        }

        /* If we have any lines out of order or (new, being discarded) then
         * we Shell sort the lines according to their current x coordinates.
         * Any previously finished lines have large x coordinates so will be
         * moved to the end of the array where they are discarded */

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

            /* At the end of the line (or if it is horizontal), use the
             * special value of the x increment.  Flag it to be discarded.
             * Draw only its width */

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
                if ((fdir & rule) == 0)
                {   s1 = x1;
                    s2 = x2;
                }
                else
                {   if (x1 < s1) s1 = x1;
                    if (x2 > s2) s2 = x2;
                    continue;
                }
            }

            /* At the beginning of the line, use the special value of the
             * x increment; otherwise add the gradient to its current x
             * coordinate.  We have to draw both the lines widths and the
             * area enclosed.  For left edges we start drawing at the left
             * of the line and draw the width too; for right edges we stop
             * drawing at the right of the line. For interior lines we draw
             * the line width (as it may not be interior higher up in the
             * pixel */

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
                if      ((fdir & rule) == 0) /* Left edge */
                {   fdir += pline->fdir;
                    s1 = x1;
                    s2 = x2;
                    continue;
                }
                if (x1 < s1) s1 = x1;        /* Right edge, or ... */
                if (x2 > s2) s2 = x2;
                fdir += pline->fdir;
                if      ((fdir & rule) != 0) /* Interior */
                    continue;
            }

            /* Draw from s1 to s2 */

            s2++;
            if (s1 < 0) s1 = 0;
            if (s2 > gstate.dev.xsize) s2 = gstate.dev.xsize;
            if (s1 >= s2) continue;
            xbyt1 = s1 >> 3;
            xbyt2 = (s2 - 1) >> 3;
            mask1 =  0xff >> (s1 & 7);
            mask2 = ~0xff >> (((s2 - 1) & 7) + 1);

            /* Loop through the bit planes */

            hscreen = &halfscreen[0];
            for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
            {   htone = hscreen->halftone;
                dptr1 = gstate.dev.buf[plane] + poff + xbyt1;
                dptr2 = gstate.dev.buf[plane] + poff + xbyt2;

                /* Optimise black or white */

                if      (hscreen->num == 0)
                {   if (dptr1 == dptr2)
                        *dptr1 |= (mask1 & mask2);
                    else
                    {   *dptr1++ |= mask1;
                        while (dptr1 != dptr2) *dptr1++ = 0xff;
                        *dptr1 |= mask2;
                    }
                }
                else if (hscreen->num == htone->area)
                {   if (dptr1 == dptr2)
                        *dptr1 &= ~(mask1 & mask2);
                    else
                    {   *dptr1++ &= ~mask1;
                        while (dptr1 != dptr2) *dptr1++ = 0x00;
                        *dptr1 &= ~mask2;
                    }
                }

                /* The general case needs a halftone screen */

                else
                {   xmod = xbyt1 % htone->xsize;
                    hbeg = hscreen->ptr +
                        (yy % htone->ysize) * htone->xsize;
                    hptr = hbeg + xmod;
                    hxsize = htone->xsize;
                    if (dptr1 == dptr2)
                    {   mask1 &= mask2;
                        *dptr1 = (*dptr1 & ~mask1) | (*hptr & mask1);
                    }
                    else
                    {   *dptr1 = (*dptr1 & ~mask1) | (*hptr & mask1);
                        dptr1++;
                        hptr++;
                        for (;;)
                        {   xmod++;
                            if (xmod == hxsize)
                            {   xmod = 0;
                                hptr = hbeg;
                            }
                            if (dptr1 == dptr2) break;
                            *dptr1++ = *hptr++;
                        }
                        *dptr1 = (*dptr1 & ~mask2) | (*hptr & mask2);
                    }
                }
            }
        }

        /* Continue with the next scan line */

        poff += gstate.dev.xbytes;
        yy++;
    }
}

/* Clip a line and scale it ready for filling */

void fillline(struct point *ppoint, int cdir, int fdir)
{   double xy1[2], xy2[2];

    /* We convert the coordinates to fixed point, with a scale factor of
     * 256.  Then when it comes to calculating the gradients it will all
     * fit into 32 bits without overflow */

    xy1[0] = floor(ppoint[0].x * 256.0 + 0.5);
    xy1[1] = floor(ppoint[0].y * 256.0 + 0.5);
    xy2[0] = floor(ppoint[1].x * 256.0 + 0.5);
    xy2[1] = floor(ppoint[1].y * 256.0 + 0.5);
    
    /* Make y1 <= y2 */

    if (xy1[1] < xy2[1])
        fillxyxy(xy1, xy2,  cdir,  fdir);
    else
        fillxyxy(xy2, xy1, -cdir, -fdir);
}


/* Clip a line and insert it into its y bucket list ready for filling */

void fillxyxy(double *xy1, double *xy2, int cdir, int fdir)
{   struct line line;
    double x1, y1, x2, y2, yy;

    x1 = xy1[0];
    y1 = xy1[1];
    x2 = xy2[0];
    y2 = xy2[1];

    /* Clip to the bottom and top.  Parts off the bottom or top are discarded
     */

    if (y1 < ylwb)
    {   if (y2 < ylwb) return;
        x1 = floor(x1 - (y1 - ylwb) * (x2 - x1) / (y2 - y1) + 0.5);
        y1 = ylwb;
    }
    if (y2 > yupb)
    {   if (y1 > yupb) return;
        x2 = floor(x2 - (y2 - yupb) * (x2 - x1) / (y2 - y1) + 0.5);
        y2 = yupb;
    }
    if (y1 < ymin) ymin = y1;
    if (y2 > ymax) ymax = y2;

    /* Clip to the left and right.  Parts off the edges are converted to
     * vertical lines along the edges */

    if (x1 < xlwb)
        if (x2 < xlwb)
            x1 = x2 = xlwb;
        else
        {   yy = floor(y1 - (x1 - xlwb) * (y2 - y1) / (x2 - x1) + 0.5);
            if (yy > y1 && yy < y2)
            {   xy1[0] = xlwb;
                xy1[1] = y1;
                xy2[0] = xlwb;
                xy2[1] = yy;
                fillxyxy(xy1, xy2, cdir, fdir);
                y1 = yy;
            }
            x1 = xlwb;
        }
    else
        if (x2 < xlwb)
        {   yy = floor(y2 - (x2 - xlwb) * (y1 - y2) / (x1 - x2) + 0.5);
            if (yy > y1 && yy < y2)
            {   xy1[0] = xlwb;
                xy1[1] = yy;
                xy2[0] = xlwb;
                xy2[1] = y2;
                fillxyxy(xy1, xy2, cdir, fdir);
                y2 = yy;
            }
            x2 = xlwb;
        }
    if (x1 > xupb)
        if (x2 > xupb)
            x1 = x2 = xupb;
        else
        {   yy = floor(y1 - (x1 - xupb) * (y1 - y2) / (x1 - x2) + 0.5);
            if (yy > y1 && yy < y2)
            {   xy1[0] = xupb;
                xy1[1] = y1;
                xy2[0] = xupb;
                xy2[1] = yy;
                fillxyxy(xy1, xy2, cdir, fdir);
                y1 = yy;
            }
            x1 = xupb;
        }
    else
        if (x2 > xupb)
        {   yy = floor(y2 - (x2 - xupb) * (y2 - y1) / (x2 - x1) + 0.5);
            if (yy > y1 && yy < y2)
            {   xy1[0] = xupb;
                xy1[1] = yy;
                xy2[0] = xupb;
                xy2[1] = y2;
                fillxyxy(xy1, xy2, cdir, fdir);
                y2 = yy;
            }
            x2 = xupb;
        }

    /* The top edge is just outside the page */

    if (y1 >= yupb) return;

    /* Construct the line.  If it is not horizontal calculate the gradient
     * and the special values of the x displacement for the first and last
     * lines.  The x coordinates are on a scale of 65536  */

    line.cdir = cdir;
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

/* Set up the y bucket list */

void setybucket(int base, int size)
{   struct line *pline;
    int len, i;

    len = size * sizeof (struct line *);
    if (len > memylen)
    {   memfree(memybeg, memylen);
        memylen = 0;
        memybeg = memalloc(len);
        if (memybeg == NULL) error(errmemoryallocation);
        memylen = len;
        ybucket = memybeg;
    }
    else
        if (ybflag) memset((char *) memybeg, 0, memylen);
    ybflag = 1;

    pline = &linearray[0];
    i = lineend;

    while (i--)
    {   pline->chain = ybucket[pline->y1 - base];
        ybucket[pline->y1 - base] = pline;
        pline++;
    }
}

/* Static variables for imaging */

static int imode, incol, inproc, imulti, ifast;
static int iwidth, iheight, ibps, ibpsn, ibwdth, ibshf, ismax;
static int ibpos, ix1, iy1, idx, idy;
static float ictm[6], icti[6];

static char *ibstring[4], ibodd[3];

static int *isample;

static struct
{   float shade[4];
} *ishade;

static union
{   int i;
    unsigned char b[4];
} is;

/* Render an image */

void image(int width, int height, int bps, float *matrix, int mode,
           int ncol, int multi, struct object *procs)
{   struct object *token1;
    struct point point;
    float newctm[6], *shade;
    int t0, t1, t2, t3;
    int bmin, bmax, bpos, blen;
    int xpos1, ypos1, xpos2, ypos2, odds;
    int nest, i, lev;

    if (istate.flags & intchar)
        if      (istate.type < 2)
            return;
        else if (istate.type == 2 && !gstate.cacheflag)
            return;
    if (istate.flags & intgraph)
        error(errundefined);

    lev = flushlevel(-1);

    /* Set up the static variables */

    imode = mode;
    incol = ncol;
    inproc = multi ? ncol : 1;
    imulti = multi;
    iwidth = width;
    iheight = height;
    ibps = bps;
    ibpsn = multi ? bps : bps * ncol;
    ibwdth = (width * ibpsn + 7) / 8;
    ibshf = (ibps == 1) ? 0 : (ibps == 2) ? 1 : (ibps == 4) ? 2 : 3;
    ismax = ~(~0 << ibps);

    /* Find some memory for the sample and shade buffers */

    blen = bmax = gstate.dev.xsize * sizeof(int);
    if (incol == 1) bmax += (ismax + 1) * sizeof (float [4]);
    checkimagesize(bmax);
    isample = memibeg;
    ishade = (void *) ((char *) memibeg + blen);

    /* Set up the halftone patterns */

    if (halfok > gnest) setuphalf();

    /* For imagemask set up the halftone screens now */

    if (mode >= 0)
        setupfill();

    /* Otherwise, if there is only one colour set up the shade table */

    else if (incol == 1)
        for (i = 0; i <= ismax; i++)
        {   shade = &ishade[i].shade[0];
            shade[0] = (float) i / ismax;
            mapcolour(1, shade, gstate.dev.depth, shade);
            calltransfer(shade, shade);
        }

    /* The transformation matrix from the image to the page is the inverse
     * of the image matrix multiplied by the ctm.  Test if we can do a fast
     * image (no clipping, 1 sample bit per pixel, horizontal).  Calculate
     * the locations of the corners (slow image if we need to clip in the x
     * direction).  For a slow image we also calculate the inverse of the
     * transformation matrix; then we inverse delta transform a unit x pixel
     * step to find the corresponding displacement in image space */

    invertmatrix(newctm, matrix);
    multiplymatrix(ictm, newctm, gstate.ctm);
    ifast = 0;
    if (!gstate.clipflag && incol == 1 && ibps == 1)
    {   t0 = floor(iwidth * ictm[0] + 0.5);
        t1 = floor(iwidth * ictm[1] + 0.5);
        t2 = floor(iheight * ictm[2] + 0.5);
        t3 = floor(iheight * ictm[3] + 0.5);
        if (t0 == iwidth && t1 == 0 && t2 == 0)
        {   if      (t3 ==  height)
                ifast =  1;
            else if (t3 == -height)
                ifast = -1;
            ix1 = floor(ictm[4] + 0.5);
            iy1 = floor(ictm[5] + 0.5);
            if (ix1 < 0 || (ix1 + iwidth) > gstate.dev.xsize)
                ifast = 0;
        }
    }
    if (ifast == 0)
    {   invertmatrix(icti, ictm);
        point.x = 1.0;
        point.y = 0.0;
        dtransform(&point, icti);
        idx = (double) point.x * 65536.0;
        idy = (double) point.y * 65536.0;
    }

    /* Calculate the number of bytes we need.  Loop until we have rendered
     * them all */

    bpos = bmin = 0;
    blen = ibwdth * height;

    while (blen)
    {
        /* Call the procs whenever we run out of bytes */

        if (bmin == 0)
        {   ibpos = bpos;
            for (i = 0; i < inproc; i++)
            {   nest = opernest;
                pushint();
                istate.flags = intgraph;
                interpret(procs + i);
                popint();
                if (opernest > nest + 1) error(errstackoverflow);
                if (opernest < nest + 1) error(errstackunderflow);
                token1 = &operstack[opernest - 1];
                if (token1->type != typestring) error(errtypecheck);
                if (token1->flags & flagrprot) error(errinvalidaccess);
                ibstring[i] = vmsptr(token1->value.vref);
                if (i == 0)
                    bmin = token1->length;
                else
                    if (token1->length != bmin) error(errrangecheck);
                opernest = nest;
            }
            if (bmin == 0) break;
            if (bmin > blen) bmin = blen;
        }

        /* Render the bytes we have found */

        bmax = bmin;
        xpos1 = bpos % ibwdth;
        ypos1 = bpos / ibwdth;

        /* Fast image */

        if (ifast != 0)
            imagefast(xpos1, ypos1, bmax);

        /* Slow image.  The slow rendering routine can only handle
         * rectangles, so we split into segments as necessary */

        else
        {   if (xpos1 != 0)
            {   if ((xpos1 + bmax) >= ibwdth)
                {   bmax = ibwdth - xpos1;
                    xpos2 = ibwdth;
                }
                else
                    xpos2 = xpos1 + bmax;
                ypos2 = ypos1 + 1;
            }
            else
            {   xpos2 = (bpos + bmax) % ibwdth;
                ypos2 = (bpos + bmax) / ibwdth;
                if (ypos2 != ypos1)
                {   bmax = bmax - xpos2;
                    xpos2 = ibwdth;
                }
                else
                    ypos2 = ypos1 + 1;
            }

            /* If we mave multiple colous and a single proc, we may have
             * a partial sample left over, which we save in a buffer for
             * next time.  (But ignore the extra bits at the end of a
             * row.)  Adjust the x position according */

            xpos1 = (xpos1 << 3) >> ibshf;
            xpos2 = (xpos2 << 3) >> ibshf;
            if (ibpsn != ibps)
            {   odds = xpos2 % incol;
                xpos1 /= incol;
                xpos2 /= incol;
                if (xpos2 >= iwidth)
                    xpos2 = iwidth;
                else
                    if (odds != 0)
                    {   i = (odds * ibpsn + 7) >> 3;
                        memcpy(&ibodd[3 - i], &ibstring[0][bmax - i], i);
                    }
            }
            else
                if (xpos2 > iwidth)
                    xpos2 = iwidth;

            imageslow(xpos1, ypos1, xpos2, ypos2);
        }
        bpos += bmax;
        blen -= bmax;
        bmin -= bmax;
    }

    flushlevel(lev);
}

/* Image slow, like path filling but copies image data */

void imageslow(int xpos1, int ypos1, int xpos2, int ypos2)
{   struct point point[5], *ppoint;
    struct line *pline, **ppline;
    struct lineseg lineseg, *plineseg;
    struct halfscreen *hscreen;
    struct halftone *htone;
    char *dptr1, *dptr2;        /* device buffer pointers */
    char *hbeg, *hptr;          /* halftone screen row base, pointer */
    float shade[4];
    int flag, count, segments;  /* in-out flag, counter, segments */
    int active, discard, sort;  /* lines active, to be discarded, sorted */
    int x1, x2, xp, xx;         /* current, previous x position range */
    int y1, y2, yy;             /* min, max, current y position */
    int cdir, fdir, sdir;       /* clip, fill direction counters */
    int poff;                   /* offset of line from page */
    int xmod, hxsize;           /* position modulo halftone screen */
    int mask1, mask2;           /* bit masks for first and last bytes */
    int xbyt1, xbyt2;           /* bytes offsets from beginning of line */
    int plane;
    int ix, iy, jx, jy, bx, by, ib, ii;

    /* Construct the outline of the area to be imaged */

    lineend = 0;
    ymax = ylwb = gstate.dev.ybase * 256.0;
    ymin = yupb = (gstate.dev.ybase + gstate.dev.ysize) * 256.0;

    point[0].x = point[3].x = xpos1;
    point[1].x = point[2].x = xpos2;
    point[0].y = point[1].y = ypos1;
    point[2].y = point[3].y = ypos2;
    point[4] = point[0];

    ppoint = &point[0];
    for (count = 0; count < 5 ; count++)
    {   transform(ppoint, ictm);
        if (count > 0) fillline(ppoint - 1, 0, 1);
        ppoint++;
    }

    /* Add the clip lines */

    y1 =  ((int) ymin) >> 8;
    y2 = (((int) ymax) >> 8) + 1;
    if (y2 > gstate.dev.ybase + gstate.dev.ysize)
        y2 = gstate.dev.ybase + gstate.dev.ysize;
    ylwb = ymin;
    yupb = ymax;
    count = gstate.pathbeg - gstate.clipbeg;
    ppoint = &patharray[gstate.clipbeg];
    while (count--)
    {   if (ppoint->type != ptmove) fillline(ppoint - 1, 1, 0);
        ppoint++;
    }

    /* Set up the y buckets */

    setybucket(gstate.dev.ybase, gstate.dev.ysize);

    /* Render the area.  Start at the lowest scan line in the path and loop
     * until we reach the highest */

    active = discard = sort = 0;
    poff = (y1 - gstate.dev.ybase) * gstate.dev.xbytes;
    yy = y1;
    while (yy < y2)
    {
        /* Add all the new lines */

        pline = ybucket[yy - gstate.dev.ybase];
        ybucket[yy - gstate.dev.ybase] = 0;
        while (pline)
        {   lineptr[active++] = pline;
            pline = pline->chain;
            sort++;
        }

        /* If we have any lines out of order or (new, being discarded) then
         * we Shell sort the lines according to their current x coordinates.
         * Any previously finished lines have large x coordinates so will be
         * moved to the end of the array where they are discarded */

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

        /* Scan convert the scan line.  Only the clip lines have width */

        count = active;
        cdir = fdir = 0;
        ppline = &lineptr[0];
        plineseg = linesegarray;
        segments = 0;
        xp = -32767;

        while (count--)
        {   pline = *ppline++;
            x1 = pline->xx >> 16;

            /* At the end of the line (or if it is horizontal), use the
             * special value of the x increment.  Flag it to be discarded */

            if (yy == pline->y2)
            {   pline->xx += pline->d2;
                x2 = pline->xx >> 16;
                pline->xx = 0x7fffffff;
                discard++;

                if (pline->cdir != 0)
                {   if (x2 < x1)
                    {   xx = x1;
                        x1 = x2;
                        x2 = xx;
                    }
                    x2++;
                    lineseg.fdir = 0;
                    lineseg.cdir =  1;
                    lineseg.x = x1;
                    *plineseg++ = lineseg;
                    lineseg.cdir = -1;
                    lineseg.x = x2;
                    *plineseg++ = lineseg;
                    segments += 2;
                }
            }

            /* At the beginning of the line, use the special values of the
             * x increment; otherwise add the gradient to its current x
             * coordinate */

            else
            {   if      (yy == pline->y1)
                    pline->xx += pline->d1;  /* Beginning */
                else
                    pline->xx += pline->dx;  /* Middle */
                x2 = pline->xx >> 16;
                if (x2 < xp) sort++;
                xp = x2;

                if (pline->cdir != 0)
                {   if (x2 < x1)
                    {   xx = x1;
                        x1 = x2;
                        x2 = xx;
                    }
                    x2++;
                    lineseg.fdir = 0;
                    sdir = cdir;
                    cdir += pline->cdir;
                    if      (sdir == 0)      /* Left edge */
                    {   lineseg.cdir =  2;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        lineseg.cdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 2;
                    }
                    else if (cdir == 0)      /* Right edge */
                    {   lineseg.cdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 1;
                    }
                    else if (x1 != x2)       /* Interior, draw width */
                    {   lineseg.cdir =  1;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        lineseg.cdir = -1;
                        lineseg.x = x2;
                        *plineseg++ = lineseg;
                        segments += 2;
                    }
                }
                else
                {   lineseg.cdir = 0;
                    if (fdir == 0)           /* Left edge */
                    {   lineseg.fdir =  1;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        segments += 1;
                    }
                    else                     /* Right edge */
                    {   lineseg.fdir = -1;
                        lineseg.x = x1;
                        *plineseg++ = lineseg;
                        segments += 1;
                    }
                    fdir += pline->fdir;
                }
            }
        }

        /* Sort the line segment list.  It should be almost in order, so
         * a simple sort is probably optimal */

        xx = 0;
        while (xx < segments - 1)
        {   flag = 0;
            count = segments - 1 - xx;
            plineseg = linesegarray;
            while (count--)
            {   if (plineseg->x > (plineseg + 1)->x)
                {   flag = 1;
                    lineseg = *plineseg;
                    *plineseg = *(plineseg + 1);
                    *(plineseg + 1) = lineseg;
                }
                plineseg++;
            }
            if (flag == 0) break;
            xx++;
        }

        /* Scan the list, rendering line segments as we find them */

        flag = 0;
        cdir = fdir = 0;
        if (!gstate.clipflag) cdir = 1;
        for (plineseg = linesegarray; segments--; plineseg++)
        {   cdir += plineseg->cdir;
            fdir += plineseg->fdir;
            if (flag == 0)
            {   if (cdir && fdir)
                {   flag = 1;
                    x1 = plineseg->x;
                }
                continue;
            }
            else
            {   if (cdir && fdir)
                    continue;
                else
                {   flag = 0;
                    x2 = plineseg->x;
                }
            }
            if (x1 < 0) x1 = 0;
            if (x2 > gstate.dev.xsize) x2 = gstate.dev.xsize;
            if (x1 >= x2) continue;

            /* Render from x1 to x2.  Inverse transform the beginning of
             * the segment to find its location in the image string; to
             * find the remaining locations we will add the x pixel step
             * values */

            point[0].x = x1 + 0.5;
            point[0].y = yy + 0.5;
            transform(&point[0], icti);
            ix = point[0].x * 65536.0;
            iy = point[0].y * 65536.0;

            /* Unpack all the sample values for the segment.  Make sure the
             * locations are within the rectangle.  For multiple procs we
             * extract all the sample bits in parallel */

            for (xx = x1; xx < x2; xx++)
            {   jx = ix >> 16;
                jy = iy >> 16;
                if (jx < xpos1) jx = xpos1;
                if (jx >= xpos2) jx = xpos2 - 1;
                if (jy < ypos1) jy = ypos1;
                if (jy >= ypos2) jy = ypos2 - 1;
                by = jy * ibwdth - ibpos;
                if      (incol == 1)
                {   bx = jx << ibshf;
                    ib = by + (bx >> 3);
                    is.i = ((unsigned char *) ibstring[0])[ib];
                    if      (ibps == 1)
                        is.i = (is.i >> (~bx & 7)) & 0x01;
                    else if (ibps == 2)
                        is.i = (is.i >> (~bx & 6)) & 0x03;
                    else if (ibps == 4)
                        is.i = (bx & 4) ? is.i & 0x0f : is.i >> 4;
                }
                else if (incol == inproc)
                {   bx = jx << ibshf;
                    ib = by + (bx >> 3);
                    is.b[0] = ibstring[0][ib];
                    is.b[1] = ibstring[1][ib];
                    is.b[2] = ibstring[2][ib];
                    is.b[3] = (inproc == 4) ? ibstring[3][ib]: 0;
                    if      (ibps == 1)
                        is.i = (is.i >> (~bx & 7)) & 0x01010101;
                    else if (ibps == 2)
                        is.i = (is.i >> (~bx & 6)) & 0x03030303;
                    else if (ibps == 4)
                    {   if (!(bx & 4)) is.i >>= 4;
                        is.i &= 0x0f0f0f0f;
                    }
                }
                else
                {   bx = jx * ibpsn;
                    is.b[3] = 0;
                    for (plane = 0; plane < incol; plane++)
                    {   ib = by + (bx >> 3);
                        ii = (ib < 0) ?
                            ((unsigned char *) ibodd) [3 + ib] :
                            ((unsigned char *) (ibstring[0])) [ib];
                        if      (ibps == 1)
                            is.b[plane] = (ii >> (~bx & 7)) & 0x01;
                        else if (ibps == 2)
                            is.b[plane] = (ii >> (~bx & 6)) & 0x03;
                        else if (ibps == 4)
                            is.b[plane] = (bx & 4) ? ii & 0x0f : ii >> 4;
                        else
                            is.b[plane] = ii;
                        bx += ibps;
                    }
                }

                isample[xx] = is.i;
                ix += idx;
                iy += idy;
            }

            /* Render runs of pixels with the same sample value together */

            while (x1 < x2)
            {   is.i = isample[x1];
                xx = x1;
                while (xx < x2)
                {   xx++;
                    if (isample[xx] != is.i) break;
                }
                xbyt1 = x1 >> 3;
                xbyt2 = (xx - 1) >> 3;
                mask1 =  0xff >> (x1 & 7);
                mask2 = ~0xff >> (((xx - 1) & 7) + 1);
                x1 = xx;

                /* Imagemask, halftone screens already set up */

                if      (imode >= 0)
                {   if (is.i != imode) continue;
                }

                /* Image, only one colour, use shade table */

                else if (incol == 1)
                    setupscreen(ishade[is.i].shade);

                /* Image, multiple colours, no shade table */

                else
                {   shade[0] = (float) is.b[0] / ismax;
                    shade[1] = (float) is.b[1] / ismax;
                    shade[2] = (float) is.b[2] / ismax;
                    shade[3] = (float) is.b[3] / ismax;
                    mapcolour(incol, shade, gstate.dev.depth, shade);
                    calltransfer(shade, shade);
                    setupscreen(shade);
                }

                /* Loop through the bit planes */

                hscreen = &halfscreen[0];
                for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
                {   htone = hscreen->halftone;
                    dptr1 = gstate.dev.buf[plane] + poff + xbyt1;
                    dptr2 = gstate.dev.buf[plane] + poff + xbyt2;

                    /* Optimise black or white */

                    if      (hscreen->num == 0)
                    {   if (dptr1 == dptr2)
                            *dptr1 |= (mask1 & mask2);
                        else
                        {   *dptr1++ |= mask1;
                            while (dptr1 != dptr2) *dptr1++ = 0xffff;
                            *dptr1 |= mask2;
                        }
                    }
                    else if (hscreen->num == htone->area)
                    {   if (dptr1 == dptr2)
                            *dptr1 &= ~(mask1 & mask2);
                        else
                        {   *dptr1++ &= ~mask1;
                            while (dptr1 != dptr2) *dptr1++ = 0x0000;
                            *dptr1 &= ~mask2;
                        }
                    }

                    /* The general case needs a halftone screen */

                    else
                    {   xmod = xbyt1 % htone->xsize;
                        hbeg = hscreen->ptr +
                            (yy % htone->ysize) * htone->xsize;
                        hptr = hbeg + xmod;
                        hxsize = htone->xsize;
                        if (dptr1 == dptr2)
                        {   mask1 &= mask2;
                            *dptr1 = (*dptr1 & ~mask1) | (*hptr & mask1);
                        }
                        else
                        {   *dptr1 = (*dptr1 & ~mask1) | (*hptr & mask1);
                            dptr1++;
                            hptr++;
                            for (;;)
                            {   xmod++;
                                if (xmod == hxsize)
                                {   xmod = 0;
                                    hptr = hbeg;
                                }
                                if (dptr1 == dptr2) break;
                                *dptr1++ = *hptr++;
                            }
                            *dptr1 = (*dptr1 & ~mask2) | (*hptr & mask2);
                        }
                    }
                }
            }
        }

        /* Continue with the next scan line */

        poff += gstate.dev.xbytes;
        yy++;
    }

    ybflag = 0;
    flushlpage(y1, y2);
}

/* Image fast */

void imagefast(int xpos1, int ypos1, int blen)
{   struct halfscreen *hscreen;
    struct halftone *htone;
    char *dptr1, *dptr2;        /* device buffer pointers */
    char *hbeg, *hptr;          /* halftone screen row base, pointer */
    unsigned char *bsample, *bstr;
    char *bstring;
    int bmax, x1, x2, y1, y2, yy, i;
    int xshf1, xshf2, ibyte;
    int poff;                   /* offset of line from page */
    int xmod, hxsize;           /* position modulo halftone screen */
    int mask1, mask2;           /* bit masks for first and last bytes */
    int xbyt1, xbyt2;           /* bytes offsets from beginning of line */
    int plane;

    bstring = ibstring[0];
    if (ifast > 0)
        yy = iy1 + ypos1;
    else
        yy = iy1 - ypos1 - 1;
    y1 = yy;

    /* Loop rendering rows */

    while (blen)
    {   x1 = ix1 + (xpos1 << 3);
        x2 = ix1 + iwidth;
        bmax = ibwdth - xpos1;
        if (blen < bmax)
        {   x2 = x1 + (blen << 3);
            bmax = blen;
        }

        /* Clip in the y direction */

        if (yy >= gstate.dev.ybase &&
            yy < gstate.dev.ybase + gstate.dev.ysize)
        {   poff = (yy - gstate.dev.ybase) * gstate.dev.xbytes;

            /* Bit shift the row */

            xshf1 = x1 & 7;
            xshf2 = ((x2 - 1) & 7) + 1;
            if (xshf1 == 0)
                memcpy((char *) isample, bstring, bmax);
            else
            {   bstr = (unsigned char *) bstring;
                bsample = (char *) isample;
                i = bmax;
                *bsample++ = *bstr++ >> xshf1;
                while (--i)
                {   *bsample++ = ((*(bstr - 1) << 8) | *bstr) >> xshf1;
                    bstr++;
                }
                *bsample = (*(bstr - 1) << 8) >> xshf1;
            }
            bsample = (char *) isample;

            xbyt1 = x1 >> 3;
            xbyt2 = (x2 - 1) >> 3;
            mask1 =  0xff >> xshf1;
            mask2 = ~0xff >> xshf2;

            /* Image all the samples that are 0 */

            if (imode < 0)
                setupscreen(ishade[0].shade);
            if (imode != 1)
            {   bsample[0] |= ~mask1;
                bsample[xbyt2 - xbyt1] |= ~mask2;
                hscreen = &halfscreen[0];
                for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
                {   htone = hscreen->halftone;
                    dptr1 = gstate.dev.buf[plane] + poff + xbyt1;
                    dptr2 = gstate.dev.buf[plane] + poff + xbyt2 + 1;
                    bstr = bsample;

                    /* Optimise black or white */

                    if      (hscreen->num == 0)
                        for (;;)
                        {   *dptr1++ |= ~*bstr++;
                            if (dptr1 == dptr2) break;
                        }
                    else if (hscreen->num == htone->area)
                        for (;;)
                        {   *dptr1++ &=  *bstr++;
                            if (dptr1 == dptr2) break;
                        }

                    /* The general case needs a halftone screen */

                    else
                    {   xmod = xbyt1 % htone->xsize;
                        hbeg = hscreen->ptr +
                            (yy % htone->ysize) * htone->xsize;
                        hptr = hbeg + xmod;
                        hxsize = htone->xsize;
                        for (;;)
                        {   ibyte = *bstr++;
                            *dptr1 = (*dptr1 &  ibyte) | (*hptr++ & ~ibyte);
                            dptr1++;
                            if (dptr1 == dptr2) break;
                            xmod++;
                            if (xmod == hxsize)
                            {   xmod = 0;
                                hptr = hbeg;
                            }
                        }
                    }
                }
            }

            /* Image all the samples that are 1 */

            if (imode < 0)
                setupscreen(ishade[1].shade);
            if (imode != 0)
            {   bsample[0] &= mask1;
                bsample[xbyt2 - xbyt1] &= mask2;
                hscreen = &halfscreen[0];
                for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
                {   htone = hscreen->halftone;
                    dptr1 = gstate.dev.buf[plane] + poff + xbyt1;
                    dptr2 = gstate.dev.buf[plane] + poff + xbyt2 + 1;
                    bstr = bsample;

                    /* Optimise black or white */

                    if      (hscreen->num == 0)
                        for (;;)
                        {   *dptr1++ |=  *bstr++;
                            if (dptr1 == dptr2) break;
                        }
                    else if (hscreen->num == htone->area)
                        for (;;)
                        {   *dptr1++ &= ~*bstr++;
                            if (dptr1 == dptr2) break;
                        }

                    /* The general case needs a halftone screen */

                    else
                    {   xmod = xbyt1 % htone->xsize;
                        hbeg = hscreen->ptr +
                            (yy % htone->ysize) * htone->xsize;
                        hptr = hbeg + xmod;
                        hxsize = htone->xsize;
                        for (;;)
                        {   ibyte = *bstr++;
                            *dptr1 = (*dptr1 & ~ibyte) | (*hptr++ &  ibyte);
                            dptr1++;
                            if (dptr1 == dptr2) break;
                            xmod++;
                            if (xmod == hxsize)
                            {   xmod = 0;
                                hptr = hbeg;
                            }
                        }
                    }
                }
            }
        }
        xpos1 = 0;
        yy += ifast;
        bstring += bmax;
        blen -= bmax;
    }

    if (ifast < 0)
    {   y2 = y1;
        y1 = yy;
    }
    else
        y2 = yy;
    if (y1 < gstate.dev.ybase)
        y1 = gstate.dev.ybase;
    if (y2 > gstate.dev.ybase + gstate.dev.ysize)
        y2 = gstate.dev.ybase + gstate.dev.ysize;
    flushlpage(y1, y2);
}

/* Erase the page */

void erasepage(void)
{   struct halfscreen *hscreen;
    struct halftone *htone;
    char *hbeg, *hptr, *pbuf;
    float shade[4];
    int xlen, xpos, ypos, ymod;
    int hxsize, hysize;
    int plane;

    if (istate.flags & intgraph) error(errundefined);

    /* Set up the halftone pattern if necessary */

    if (halfok > gnest) setuphalf();

    /* Set up the halftine screens for a gray level of 1.0 */

    shade[0] = shade[1] = shade[2] = shade[3] = 1.0;
    calltransfer(shade, shade);
    setupscreen(shade);

    /* Loop through the bit planes */

    hscreen = &halfscreen[0];
    for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
    {   pbuf = gstate.dev.buf[plane];
        htone = hscreen->halftone;

        /* Optimse erase to black or white (the commonest cases) */

        if      (hscreen->num == 0)
            memset(pbuf, 0xff, gstate.dev.len);
        else if (hscreen->num == htone->area)
            memset(pbuf, 0x00, gstate.dev.len);

        /* Otherwise we replicate the halftone screen */

        else
        {   hxsize = htone->xsize;
            hysize = htone->ysize;
            hbeg = hscreen->ptr;
            ymod = gstate.dev.ybase % hysize;
            hptr = hbeg + ymod * hxsize;

            /* Loop for all the rows */

            for (ypos = 0; ypos < gstate.dev.ysize; ypos++)
            {   xpos = 0;

                /* Replicate the screen through the row. */

                for (;;)
                {   xlen = gstate.dev.xbytes - xpos;
                    if (xlen == 0) break;
                    if (xlen > hxsize) xlen = hxsize;
                    memcpy(pbuf, hptr, xlen);
                    pbuf += xlen;
                    xpos += xlen;
                }

                /* Step on to next row.  Restart the screen as necessary */

                hptr += hxsize;
                ymod++;
                if (ymod == hysize)
                {   ymod = 0;
                    hptr = hbeg;
                }
            }
        }
    }
    flushlpage(gstate.dev.ybase, gstate.dev.ybase + gstate.dev.ysize);
}

/* End of file "postdraw.c" */
