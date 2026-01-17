/* PostScript interpreter file "postgraph.c" - graphics routines
 * (C) Adrian Aylward 1989, 1992
 * V1.6 First source release
 * V1.7 Fix stroking lines of zero length
 */

# include "post.h"

/* Routines */

extern void checkclipsize(int size, int end);
extern void setlinesize(int len);
extern void flattencurve(struct point *ppoint, int depth);
extern void strokesub(int pt1, int pt2);
extern void strokeparm(struct point *ppoint1, struct point *ppoint2,
                       float *len);
extern void strokeinit(void);
extern void strokeline(struct point *ppoint1, struct point *ppoint2,
                       int bjoin, int ejoin);
extern void strokecap(int type, int cap);
extern void clipline(struct clip *pclip);
extern void clipswap(struct clip *pclip);
extern void swappath(int beg, int end);

/* Initialise the graphics state */

void initgstate(void)
{   struct object token, *aptr;
    vmref aref;
    if (page.xsize > 30000 || page.ysize > 30000 || page.yheight > 30000)
        error(errlimitcheck);
    gstate.dev = page;
    pathsize = linesize = clipsize = 0;
    halfbeg = memhbeg;
    halfsize = memhlen;
    halfok = gstacksize + 1;
    screenok = 0;
    token.type = 0;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    gstate.font = token;
    gstate.clipbeg = 0;
    aref = arrayalloc(9);
    aptr = vmaptr(aref);
    nametoken(&aptr[0], "dup", -1, flagexec);
    nametoken(&aptr[1], "mul", -1, flagexec);
    nametoken(&aptr[2], "exch", -1, flagexec);
    aptr[3] = aptr[0];
    aptr[4] = aptr[1];
    nametoken(&aptr[5], "add", -1, flagexec);
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    token.value.rval = 1.0;
    aptr[6] = token;
    aptr[7] = aptr[2];
    nametoken(&aptr[8], "sub", -1, flagexec);
    token.type = typearray;
    token.flags = flagexec;
    token.length = 9;
    token.value.vref = aref;
    bind(&token, 0);
    if (gstate.dev.depth == 4)
    {   gstate.screendepth = 4;
        gstate.screenangle[0] = 105.0;
        gstate.screenangle[1] =  75.0;
        gstate.screenangle[2] =  95.0;
        gstate.screenangle[3] =  45.0;
        gstate.screenfreq[0] = gstate.screenfreq[1] =
            gstate.screenfreq[2] = gstate.screenfreq[3] = 60.0;
        gstate.screenfunc[0] = gstate.screenfunc[1]  =
            gstate.screenfunc[2] = gstate.screenfunc[3] = token;
    }
    else
    {   gstate.screendepth = 1;
        gstate.screenangle[0] = 45.0;
        gstate.screenfreq[0] = 60.0;
        gstate.screenfunc[0] = token;
    }
    gstate.transdepth = 1;
    token.length = 0;
    gstate.transfer[0] = token;
    gstate.ucr = token;
    gstate.gcr = token;
    gstate.flatness = 1.0;
    gstate.cacheflag = 0;
    gstate.nullflag = 0;
    gnest = 0;
    gstack = vmallocv(sizeof (struct gstate) * (gstacksize + 1));
    setuphalf();
    initgraphics();
    gsave();
    vmstack[0].gnest = 1;
}

/* Initialise the graphics */

void initgraphics(void)
{   struct object token;
    initctm(gstate.ctm);
    gstate.pathbeg = gstate.pathend = gstate.clipbeg;
    cliplength(0);
    gstate.clipflag = 0;
    gstate.linewidth = 1.0;
    gstate.linecap = 0;
    gstate.linejoin = 0;
    gstate.mitrelimit = 10.0;
    gstate.mitresin = 0.198997;
    gstate.dashoffset = 0.0;
    token.type = typearray;
    token.flags = 0;
    token.length = 0;
    token.value.vref = 0;
    gstate.dasharray = token;
    gstate.gray = 0.0;
    gstate.rgb[0] = gstate.rgb[1] = gstate.rgb[2] = 0.0;
    gstate.rgbw[0] = gstate.rgbw[1] =
        gstate.rgbw[2] = gstate.rgbw[3] = 0.0;
    gstate.shadeok = 0;
}

/* Initialise a matrix to the default ctm */

void initctm(float newctm[])
{   newctm[0] = gstate.dev.xden / 72.0;
    newctm[1] = 0.0;
    newctm[2] = 0.0;
    newctm[3] = gstate.dev.yden / 72.0;
    newctm[4] = -gstate.dev.xoff;
    newctm[5] = -gstate.dev.yoff;
    if (gstate.dev.ydir < 0)
    {   newctm[3] = -newctm[3];
        newctm[5] = (gstate.dev.yheight + gstate.dev.yoff);
    }
}

/* Change device page */

void setdevice(struct device *ppage)
{   struct gstate *pgstate;
    int i;

    /* Check whether the densities or depth have changed; install the page */

    if (ppage->depth != page.depth ||
        ppage->xden != page.xden ||
        ppage->yden != page.yden ||
        ppage->ydir != page.ydir)
    {   halfok = gstacksize + 1;
        screenok = 0;
    }
    page = *ppage;

    /* Update all the page devices on the gstate stack */

    i = gnest;
    pgstate = &gstack[0];
    for (;;)
    {   if (i == 0) pgstate = &gstate;
        if (!pgstate->cacheflag && !pgstate->nullflag)
        {   if (page.depth != pgstate->dev.depth) pgstate->shadeok = 0;
            pgstate->dev = page;
        }
        if (i == 0) break;
        i--;
        pgstate++;
    }

    /* Reinitialise the CTM of the bottommost graphics state */

    initctm(gstack[0].ctm);
}

/* Check the image memory size */

void checkimagesize(int len)
{   if (len > memilen)
    {   memfree(memibeg, memilen);
        memilen = 0;
        memibeg = memalloc(len);
        if (memibeg == NULL) error(errmemoryallocation);
        memilen = len;
    }
}

/* Check the line array size */

void checklinesize(int size)
{   int len, segs;
    if (size > linesize)
    {   len = (sizeof (struct line) +
               sizeof (struct line *) +
               sizeof (struct lineseg) * 2) * size;
        segs = (len + memlmin - 1) / memlmin;
        if (segs > memlsegmax) error(errlimitcheck);
        setlinesize(memlmin * segs);
    }
}

/* Check the clip array size, relocating the clip pointers */

void checkclipsize(int size, int end)
{   struct clip *pclip, **pcptr;
    int len, segs, oldsize;
    if (size > clipsize)
    {   oldsize = clipsize;
        len = (sizeof (struct clip) +
               sizeof (struct clip *)) * size;
        segs = (len + memlmin - 1) / memlmin;
        if (segs > memlsegmax) error(errlimitcheck);
        pclip = cliparray;
        setlinesize(memlmin * segs);
        pcptr = (void *) (cliparray + oldsize);
        while (end--)
            clipptr[end] = cliparray + (pcptr[end] - pclip);
    }
}

/* Set the line and clip array memory size */

void setlinesize(int len)
{   void *beg;
    beg = memalloc(len);
    if (beg == NULL) error(errmemoryallocation);
    memcpy((char *) beg, (char *) memlbeg, memllen);
    memfree(memlbeg, memllen);
    memlbeg = beg;
    memllen = len;
    linesize = memllen / (sizeof (struct line) +
                          sizeof (struct line *) +
                          sizeof (struct lineseg) * 2);
    linearray = memlbeg;
    lineptr = (void *) (linearray + linesize);
    linesegarray = (void *) (lineptr + linesize);
    clipsize = memllen / (sizeof (struct clip) +
                          sizeof (struct clip *));
    cliparray = memlbeg;
    clipptr = (void *) (cliparray + clipsize);
}

/* Check the path array size */

void checkpathsize(int size)
{   void *beg;
    int len, segs;
    if (size > pathsize)
    {   len = sizeof (struct point) * size;
        segs = (len + mempmin - 1) / mempmin;
        if (segs > mempsegmax) error(errlimitcheck);
        len = mempmin * segs;
        beg = memalloc(len);
        if (beg == NULL) error(errmemoryallocation);
        memcpy((char *) beg, (char *) mempbeg, memplen);
        memfree(mempbeg, memplen);
        mempbeg = beg;
        memplen = len;
        pathsize = len / sizeof (struct point);
        patharray = mempbeg;
    }
}

/* Shuffle the path up or down to change the clip path length */

void cliplength(int cliplen)
{   struct point *ppoint1, *ppoint2;
    int pathlen, len;
    len = pathlen = gstate.pathend - gstate.pathbeg;
    checkpathsize(gstate.clipbeg + cliplen + pathlen);
    ppoint1 = &patharray[gstate.clipbeg + cliplen];
    ppoint2 = &patharray[gstate.pathbeg];
    if      (ppoint1 > ppoint2)
    {   ppoint1 += pathlen;
        ppoint2 += pathlen;
        while (len--) *--ppoint1 = *--ppoint2;
    }
    else if (ppoint1 < ppoint2)
        while (len--) *ppoint1++ = *ppoint2++;
    gstate.pathbeg = gstate.clipbeg + cliplen;
    gstate.pathend = gstate.pathbeg + pathlen;
}

/* Get the clipping path */

void clippath(void)
{   struct point point, *ppoint1, *ppoint2;
    int len, l;
    if (gstate.clipflag)
        len = gstate.pathbeg - gstate.clipbeg;
    else
        len = 5;
    checkpathsize(gstate.pathbeg + len);
    ppoint1 = &patharray[gstate.pathbeg];
    if (gstate.clipflag)
    {   ppoint2 = &patharray[gstate.clipbeg];
        l = len;
        while (l--) *ppoint1++ = *ppoint2++;
    }
    else
    {   point.type = ptmove;
        point.x = 0.0;
        point.y = 0.0;
        ppoint1[0] = point;
        point.type = ptclosex;
        ppoint1[4] = point;
        point.type = ptline;
        point.x = gstate.dev.xsize;
        ppoint1[1] = point;
        point.y = gstate.dev.yheight;
        ppoint1[2] = point;
        point.x = 0.0;
        ppoint1[3] = point;
    }
    gstate.pathend = gstate.pathbeg + len;
}

/* Save the graphics state */

void gsave(void)
{   struct point *ppoint1, *ppoint2;
    int i;
    if (istate.flags & intgraph) error(errundefined);
    if (gnest == gstacksize) error(errlimitcheck);
    i = gstate.pathend - gstate.clipbeg;
    checkpathsize(gstate.pathend + i);
    gstack[gnest++] = gstate;
    if (i != 0)
    {   ppoint1 = &patharray[gstate.clipbeg];
        ppoint2 = &patharray[gstate.pathend];
        gstate.clipbeg += i;
        gstate.pathbeg += i;
        gstate.pathend += i;
        while (i--) *ppoint2++ = *ppoint1++;
    }
}

/* Restore the graphics state */

void grest(void)
{   if (istate.flags & intgraph) error(errundefined);
    if (gnest > istate.gbase &&
        gnest > vmstack[vmnest].gnest)
    {   gstate = gstack[--gnest];
        if (gnest <= halfok)
        {   halfok = gstacksize + 1;
            setuphalf();
        }
    }
    screenok = 0;
}

/* Restore the graphics state to the previous save */

void grestall(void)
{   int nest;
    if (istate.flags & intgraph) error(errundefined);
    nest = vmstack[vmnest].gnest;
    if (nest > istate.gbase)
    {   gnest = nest;
        gstate = gstack[--gnest];
        gsave();
        if (gnest <= halfok)
        {   halfok = gstacksize + 1;
            setuphalf();
        }
        screenok = 0;
    }
}

/* Get a matrix */

void getmatrix(vmref aref, float *matrix)
{   struct object *aptr;
    int i;
    i = 6;
    aptr = vmaptr(aref);
    while (i--)
    {   if      (aptr->type == typeint)
            *matrix = aptr->value.ival;
        else if (aptr->type == typereal)
            *matrix = aptr->value.rval;
        else
            error(errtypecheck);
        aptr++;
        matrix++;
    }
}

/* Put a matrix */

void putmatrix(vmref aref, float *matrix)
{   struct object token, *aptr;
    int i;
    token.type = typereal;
    token.flags = 0;
    token.length = 0;
    i = 6;
    aptr = vmaptr(aref);
    while (i--)
    {   token.value.rval = *matrix++;
        *aptr++ = token;
    }
}

/* Multiply two matrices */

void multiplymatrix(float *mat1, float *mat2, float *mat3)
{   mat1[0] = mat2[0] * mat3[0] + mat2[1] * mat3[2];
    mat1[1] = mat2[0] * mat3[1] + mat2[1] * mat3[3];
    mat1[2] = mat2[2] * mat3[0] + mat2[3] * mat3[2];
    mat1[3] = mat2[2] * mat3[1] + mat2[3] * mat3[3];
    mat1[4] = mat2[4] * mat3[0] + mat2[5] * mat3[2] + mat3[4];
    mat1[5] = mat2[4] * mat3[1] + mat2[5] * mat3[3] + mat3[5];
}

/* Invert a matrix */

void invertmatrix(float *mat1, float *mat2)
{   float d = mat2[0] * mat2[3] - mat2[1] * mat2[2];
    mat1[0] =  mat2[3] / d;
    mat1[1] = -mat2[1] / d;
    mat1[2] = -mat2[2] / d;
    mat1[3] =  mat2[0] / d;
    mat1[4] = (mat2[2] * mat2[5] - mat2[3] * mat2[4]) / d;
    mat1[5] = (mat2[1] * mat2[4] - mat2[0] * mat2[5]) / d;
}

/* Transform a point by a matrix */

void transform(struct point *ppoint, float *matrix)
{   float x, y;
    x = ppoint->x;
    y = ppoint->y;
    ppoint->x = x * matrix[0] + y * matrix[2] + matrix[4];
    ppoint->y = x * matrix[1] + y * matrix[3] + matrix[5];
}

/* Delta transform a point by a matrix */

void dtransform(struct point *ppoint, float *matrix)
{   float x, y;
    x = ppoint->x;
    y = ppoint->y;
    ppoint->x = x * matrix[0] + y * matrix[2];
    ppoint->y = x * matrix[1] + y * matrix[3];
}

/* Inverse transform a point by a matrix */

void itransform(struct point *ppoint, float *matrix)
{   float x, y, d;
    x = ppoint->x;
    y = ppoint->y;
    d = matrix[0] * matrix[3] - matrix[1] * matrix[2];
    ppoint->x = (x * matrix[3] - y * matrix[2] +
        matrix[5] * matrix[2] - matrix[4] * matrix[3]) / d;
    ppoint->y = (y * matrix[0] - x * matrix[1] +
        matrix[4] * matrix[1] - matrix[5] * matrix[0]) / d;
}

/* Inverse delta transform a point by a matrix */

void idtransform(struct point *ppoint, float *matrix)
{   float x, y, d;
    x = ppoint->x;
    y = ppoint->y;
    d = matrix[0] * matrix[3] - matrix[1] * matrix[2];
    ppoint->x = (x * matrix[3] - y * matrix[2]) / d;
    ppoint->y = (y * matrix[0] - x * matrix[1]) / d;
}

/* Generate a circular arc */

void arc(int dir, float radaa[3],
         struct point *centre, struct point *beg, struct point *end)
{   struct point point[3], *ppoint;
    float ang1, ang2, angd, angm, radius, radm, rh, cos1, cos2, sin1, sin2;
    int num;

    /* Find out which direction, and the angle of the arc */

    radius = radaa[0];
    ang1 = fmod(radaa[1], twopi);
    ang2 = fmod(radaa[2], twopi);
    if      (dir == 0)
    {  if (fabs(ang2 - ang1) > pi)
           if (ang2 > ang1)
               ang1 += twopi;
           else
               ang2 += twopi;
    }
    else if (dir > 0)
    {  if (ang2 < ang1 + pdelta) ang2 += twopi;
    }
    else
       if (ang2 > ang1 + pdelta) ang1 += twopi;
    angd = ang2 - ang1;

    /* Determine (a rough upper bound of) the radius, in pixels */

    radm = gstate.ctm[0];
    if (gstate.ctm[1] > radm) radm = gstate.ctm[1];
    if (gstate.ctm[2] > radm) radm = gstate.ctm[2];
    if (gstate.ctm[3] > radm) radm = gstate.ctm[3];
    radm *= radaa[0];

    /* Determine the number of Bezier curves needed to approximate the arc */

    if      (radm <= 27.0)
        angm = pi       + .0001;
    else if (radm <= 1834.0)
        angm = pi / 2.0 + .0001;
    else if (radm <= 20953.0)
        angm = pi / 3.0 + .0001;
    else if (radm <= 117778.0)
        angm = pi / 4.0 + .0001;
    else
        angm = pi / 8.0 + .0001;
    num = (int) ceil(fabs(angd) / angm);
    angd = angd / num;

    /* Generate a move or line from the current point */

    cos2 = cos(ang1);
    sin2 = sin(ang1);
    point[2].type = ptmove;
    point[2].x = centre->x + radius * cos2;
    point[2].y = centre->y + radius * sin2;
    if (dir == 0) *beg = point[2];
    transform(&point[2], gstate.ctm);
    pathend = gstate.pathend;
    ppoint = &patharray[pathend];
    if (gstate.pathbeg == pathend ||
        fabs(point[2].x - (ppoint - 1)->x) >= 0.2 ||
        fabs(point[2].y - (ppoint - 1)->y) >= 0.2)
    {   if (gstate.pathbeg != pathend) point[2].type = ptline;
        checkpathsize(pathend + 1);
        patharray[pathend++] = point[2];
    }
    point[0].type = ptcurve;
    point[1].type = ptcurve;
    point[2].type = ptcurve;

    /* Loop to generate the curves */

    while (num--)
    {   ang2 = ang1 + angd;
        cos1 = cos2;
        sin1 = sin2;
        cos2 = cos(ang2);
        sin2 = sin(ang2);

        /* We place the intermediate control points on the tangents at the
         * endpoints (so the gradient is correct) and at a height 4/3 the
         * height of the midpoint of the arc (so the midpoint of the curve
         * is the same as the midpoint of the arc) */

        rh = radius * f43 * tan(angd / 4.0);
        point[0].x = -rh * sin1;
        point[0].y =  rh * cos1;
        point[1].x =  rh * sin2;
        point[1].y = -rh * cos2;
        dtransform(&point[0], gstate.ctm);
        dtransform(&point[1], gstate.ctm);
        point[0].x += point[2].x;
        point[0].y += point[2].y;

        point[2].x = centre->x + radius * cos2;
        point[2].y = centre->y + radius * sin2;
        if (dir == 0) *end = point[2];
        transform(&point[2], gstate.ctm);
        point[1].x += point[2].x;
        point[1].y += point[2].y;
        checkpathsize(pathend + 3);
        ppoint = &patharray[pathend];
        ppoint[0] = point[0];
        ppoint[1] = point[1];
        ppoint[2] = point[2];
        pathend += 3;
        ang1 = ang2;
    }

    gstate.pathend = pathend;
}

/* Close the current path */

void closepath(int type)
{   struct point point, *ppoint;
    if (gstate.pathbeg == gstate.pathend) return;
    ppoint = &patharray[gstate.pathend - 1];

    /* If it is closed already, make the close explicit if necessary */

    if (ppoint->type == ptclosex)
        return;
    if (ppoint->type == ptclosei)
    {   ppoint->type = type;
        return;
    }
    if (ppoint->type == ptmove)
        return;

    /* Scan back to the beginning of the sub path to find the coordinates to
     * close it to */

    for (;;)
    {   point = *ppoint;
        if (point.type == ptclosex ||
            point.type == ptclosei ||
            point.type == ptmove)
            break;
        ppoint--;
    }

    /* Append a close */

    point.type = type;
    checkpathsize(gstate.pathend + 1);
    patharray[gstate.pathend++] = point;
    return;
}

/* Flatten the current path */

void flattenpath(void)
{   struct point point[4], *ppoint1, *ppoint2;
    int len, num;

    len = gstate.pathend - gstate.pathbeg;

    /* Skip all elements before the first curve */

    ppoint1 = &patharray[gstate.pathbeg];
    for (;;)
    {   if (len == 0) return;
        if (ppoint1->type == ptcurve) break;
        ppoint1++;
        len--;
    }

    num = len;
    pathbeg = pathend = gstate.pathend;

    /* Flatten curves, copying all points into workspace at the end of the
     * path array */

    for (;;)
    {   if (len == 0) break;
        ppoint1 = &patharray[pathbeg - len];
        point[1] = ppoint1[0];
        if (point[1].type == ptcurve)
        {   point[0] = ppoint1[-1];
            point[2] = ppoint1[1];
            point[3] = ppoint1[2];
            flattencurve(point, 0);
            len -= 3;
        }
        else
        {   checkpathsize(pathend + 1);
            patharray[pathend++] = point[1];
            len--;
        }
    }

    /* Copy the workspace back to the path */

    ppoint1 = &patharray[gstate.pathend - num];
    ppoint2 = &patharray[pathbeg];
    len = pathend - pathbeg;
    gstate.pathend += len - num;
    while (len--) *ppoint1++ = *ppoint2++;
}

/* Flatten a curve */

void flattencurve(struct point *ppoint, int depth)
{   struct point point, curves[7];
    float f, g, h, a, b, c;

    /* Find the flatness of the curve.  As it lies within the convex hull
     * of its control points we just find the maximum distance of the
     * intermediate control points from the straight line joining the ends */

    a = ppoint[3].y - ppoint[0].y;
    b = ppoint[0].x - ppoint[3].x;
    c = ppoint[3].x * ppoint[0].y - ppoint[0].x * ppoint[3].y;
    f = a * ppoint[1].x + b * ppoint[1].y + c;
    f = f * f;
    g = a * ppoint[2].x + b * ppoint[2].y + c;
    g = g * g;
    h = gstate.flatness * gstate.flatness * (a * a + b * b);

    /* If the curve is not flat enough bisect it and flatten the halves */

    if (f > h || g > h)
    {   point.type = ptcurve;
        curves[0] = ppoint[0];
        point.x = ppoint[0].x * f12 + ppoint[1].x * f12;
        point.y = ppoint[0].y * f12 + ppoint[1].y * f12;
        curves[1] = point;
        point.x = ppoint[0].x * f14 + ppoint[1].x * f12 + ppoint[2].x * f14;
        point.y = ppoint[0].y * f14 + ppoint[1].y * f12 + ppoint[2].y * f14;
        curves[2] = point;
        point.x = ppoint[0].x * f18 + ppoint[1].x * f38 + ppoint[2].x * f38
                + ppoint[3].x * f18;
        point.y = ppoint[0].y * f18 + ppoint[1].y * f38 + ppoint[2].y * f38
                + ppoint[3].y * f18;
        curves[3] = point;
        point.x = ppoint[1].x * f14 + ppoint[2].x * f12 + ppoint[3].x * f14;
        point.y = ppoint[1].y * f14 + ppoint[2].y * f12 + ppoint[3].y * f14;
        curves[4] = point;
        point.x = ppoint[2].x * f12 + ppoint[3].x * f12;
        point.y = ppoint[2].y * f12 + ppoint[3].y * f12;
        curves[5] = point;
        curves[6] = ppoint[3];
        if (depth == maxdepth) error(errlimitcheck);
        flattencurve(&curves[0], depth + 1);
        flattencurve(&curves[3], depth + 1);
    }

    /* Otherwise make it a straight line */

    else
    {   point = ppoint[3];
        point.type = ptline;
        checkpathsize(pathend + 1);
        patharray[pathend++] = point;
    }
}

/* Create a stroke path; return it or fill it */

void strokepath(int fillflag)
{   struct point *ppoint1, *ppoint2;
    int pt1, pt2, pt3, len, lev;

    if (fillflag)
        lev = flushlevel(-1);

    /* Loop through the path */

    pt1 = gstate.pathbeg;
    pt3 = gstate.pathend;
    pathbeg = pathend = gstate.pathend;

    while (pt1 < pt3)
    {   ppoint1 = &patharray[pt1];
        ppoint2 = ppoint1;
        pt2 = pt1;

        /* Scan to locate the end of the subpath */

        while (pt2 < pt3)
        {   pt2++;
            if (ppoint2->type == ptclosei || ppoint2->type == ptclosex)
                break;
            ppoint2++;
        }

        /* If the subpath does not begin with a move, include the previous
         * point - which must have been a close.  Stroke the subpath. N.B.
         * this may invalidate the pointers to the path array */

        if (ppoint1->type != ptmove) pt1--;
        strokesub(pt1, pt2);
        pt1 = pt2;

        /* If we are filling, do it now and discard the stroke path */

        if (fillflag)
        {   fill(pathbeg, pathend, -1);
            pathend = pathbeg;
        }
    }

    /* If we are not filling, copy the stroke path to the current path */

    if (!fillflag)
    {   len = pathend - pathbeg;
        gstate.pathend = gstate.pathbeg + len;
        ppoint1 = &patharray[gstate.pathbeg];
        ppoint2 = &patharray[pathbeg];
        while (len--) *ppoint1++ = *ppoint2++;
    }
    else
        flushlevel(lev);
}

/* Stroke a subpath */

void strokesub(int pt1, int pt2)
{   struct point *ppoint1, *ppoint2, *ppoint3;
    float length;
    int type, bjoin, ejoin, sjoin;

    if (pt1 + 1 >= pt2) return;

    ppoint1 = &patharray[pt1];
    ppoint2 = &patharray[pt2 - 1];
    type = ppoint2->type;

    /* If the last line is an implicit close we do not stroke it */

    if (type == ptclosei)
    {   ppoint2--;
        pt2--;
        if (pt1 + 1 >= pt2) return;
    }

    /* Strip any zero length lines from the end of the subpath */

    ppoint3 = ppoint2;
    for (;;)
    {   ppoint3--;
        if (ppoint3->x != ppoint2->x || ppoint3->y != ppoint2->y)
            break;
        pt2--;
        if (pt1 + 1 >= pt2) return;
    }

    /* If the path is explicitly closed, then the beginning and end are
     * joined. We must then set the direction of the previous line so we
     * can make the joint */

    sjoin = -1;
    if (type == ptclosex)
    {   sjoin = gstate.linejoin;
        strokeparm(ppoint3, ppoint1, &length);
    }

    strokeinit();

    /* Loop to stroke the lines of the subpath. Skip lines of zero length */

    bjoin = sjoin;
    ejoin = gstate.linejoin;
    for (;;)
    {   if (pt1 + 1 >= pt2) return;
        if (pt1 + 2 >= pt2) ejoin = sjoin;
        ppoint1 = &patharray[pt1];
        ppoint2 = ppoint1 + 1;
        if (ppoint1->x != ppoint2->x || ppoint1->y != ppoint2->y)
        {   strokeline(ppoint1, ppoint2, bjoin, ejoin);
            bjoin = ejoin;
        }
        pt1++;
    }
}

/* Static variables for line parameters */

static struct point cvpt; /* line Vector,                    device space */
static struct point cupt; /* line Unit vector,               user space */
static struct point crpt; /* line Right half width vector,   device space */
static struct point cfpt; /* line Forward half width vector, device space */
static struct point pvpt; /* values of the above for the previous line */
static struct point pupt; /*    ..   */
static struct point prpt; /*    ..   */
static struct point cppt; /* current point */
static struct point eppt; /* end point */

/* Stroke set up line parameters */

void strokeparm(struct point *ppoint1, struct point *ppoint2, float *len)
{   float length, d;

    /* Save the values for the previous line */

    pvpt = cvpt;
    pupt = cupt;
    prpt = crpt;

    /* Inverse delta transform the line to user space.  Determine its
     * length */

    cupt.x = cvpt.x = ppoint2->x - ppoint1->x;
    cupt.y = cvpt.y = ppoint2->y - ppoint1->y;
    idtransform(&cupt, gstate.ctm);
    *len = length = sqrt(cupt.x * cupt.x + cupt.y * cupt.y);
    if (length == 0.0) return;

    /* Construct a unit vector along the line */

    cupt.x /= length;
    cupt.y /= length;

    /* Construct vectors along the line (forward) and across the line (right)
     * of length equal to half the line width.  Then transform them back to
     * device space. */

    d = gstate.linewidth / 2.0;
    cfpt.x =  cupt.x * d;
    cfpt.y =  cupt.y * d;
    crpt.x =  cfpt.y;
    crpt.y = -cfpt.x;
    dtransform(&cfpt, gstate.ctm);
    dtransform(&crpt, gstate.ctm);
}

/* Static variables for dash lengths */

static int dashcnt, dashsub;
static float dashlength;

/* Stroke first initialise our location in the dash array */

void strokeinit(void)
{   float length;
    dashcnt = 0;
    if (gstate.dasharray.length != 0)
    {   length = gstate.dashoffset;
        dashsub = 0;
        for (;;)
        {   dashlength = gstate.dashlength[dashsub];
            if (dashlength > length)
            {   dashlength -= length;
                break;
            }
            length -= dashlength;
            dashcnt++;
            dashsub++;
            if (dashsub == gstate.dasharray.length) dashsub = 0;
        }
    }
}


/* Stroke a line */

void strokeline(struct point *ppoint1, struct point *ppoint2,
                int bjoin, int ejoin)
{   struct point point;
    float x, y, xx, yy, h, s, c, d;
    float linelen, length, segment;
    int join;

    /* Set up the parameters */

    strokeparm(ppoint1, ppoint2, &length);
    if (length == 0) return;
    linelen = length;

    /* Start at the beginning of the line. N.B. generating the stroke
     * path may cause the path array to be  reallocated, invalidating
     * any pointers to it. So pick up the values of the points now. */

    join = bjoin;
    cppt = *ppoint1;
    eppt = *ppoint2;

    /* Loop stroking segments according to the dash pattern */

    for (;;)
    {
        /* Find the length remaining in the current dash */

        segment = length;
        if (gstate.dasharray.length != 0)
        {   if (dashlength == 0.0)
            {   dashcnt++;
                dashsub++;
                if (dashsub == gstate.dasharray.length) dashsub = 0;
                dashlength = gstate.dashlength[dashsub];
            }
            if (dashlength >= length)
            {   segment = length;
                dashlength -= length;
            }
            else
            {   segment = dashlength;
                dashlength = 0.0;
            }
        }

        /* Only draw the segment if we are in a dash */

        if ((dashcnt & 1) == 0)
        {   pathseg = pathend;

            /* If the line begins with a mitred or beveled join calculate the
             * cross product of the previous and current line vectors; this
             * is the sine of the angle between them, and tells us which side
             * to make the joint on.  If we have a mitred join we must check
             * the mitre limit; first we check the dot product of the unit
             * vectors to see if the angle is less than a right angle; then
             * we check the cross product (sine of the angle) against the
             * mitre limit.  We don't make mitred joins is the angle is very
             * small, as we would divide by zero. */

            if (join == 0 || join == 2)
            {   s = pupt.x * cupt.y - pupt.y * cupt.x;
                if (join == 0)
                {   c = pupt.x * cupt.x + pupt.y * cupt.y;
                    d = pvpt.x * cvpt.y - cvpt.x * pvpt.y;
                    if ((gstate.mitrelimit > sqrt2) ?
                            (c < 0.0 && fabs(s) < gstate.mitresin) :
                            (c < 0.0 || fabs(s) > gstate.mitresin))
                        join = 2;
                    if (fabs(d) < 0.0001)
                        join = 2;
                }

                /* Make a mitred join.  We make the mitre on one side if
                 * the angle is less than a right angle, on both if it is
                 * greater */

                if (join == 0)
                {   d = pvpt.x * cvpt.y - cvpt.x * pvpt.y;
                    h = (prpt.x * pvpt.y - prpt.y * pvpt.x) / d;
                    if (s > 0.0) h = -h;
                    x = cppt.x + cvpt.x * h;
                    y = cppt.y + cvpt.y * h;
                    h = (crpt.y * cvpt.x - crpt.x * cvpt.y) / d;
                    xx = pvpt.x * h;
                    yy = pvpt.y * h;
                    point.type = ptmove;
                    if (c < 0.0)
                    {   checkpathsize(pathend + 2);
                        point.x = x - xx;
                        point.y = y - yy;
                        patharray[pathend++] = point;
                        point.type = ptline;
                        point.x = x + xx;
                        point.y = y + yy;
                        patharray[pathend++] = point;
                    }
                    else
                    {   checkpathsize(pathend + 3);
                        if (s < 0.0)
                        {   point.x = cppt.x + crpt.x;
                            point.y = cppt.y + crpt.y; 
                            patharray[pathend++] = point;
                            point.type = ptline;
                            point.x = cppt.x - prpt.x;
                            point.y = cppt.y - prpt.y;
                            patharray[pathend++] = point;
                            point.x = x + xx;
                            point.y = y + yy;
                            patharray[pathend++] = point;
                        }
                        else
                        {   point.x = x - xx;
                            point.y = y - yy;
                            patharray[pathend++] = point;
                            point.type = ptline;
                            point.x = cppt.x + prpt.x;
                            point.y = cppt.y + prpt.y; 
                            patharray[pathend++] = point;
                            point.x = cppt.x - crpt.x;
                            point.y = cppt.y - crpt.y;
                            patharray[pathend++] = point;
                        }
                    }
                }

                /* Make a beveled join */

                else
                {   checkpathsize(pathend + 3);
                    point.type = ptmove;
                    point.x = cppt.x + crpt.x;
                    point.y = cppt.y + crpt.y;
                    patharray[pathend++] = point;
                    point.type = ptline;
                    if (s != 0.0)
                    {   point.x = cppt.x;
                        point.y = cppt.y;
                        if (s < 0.0)
                        {   point.x -= prpt.x;
                            point.y -= prpt.y;
                        }
                        else
                        {   point.x += prpt.x;
                            point.y += prpt.y;
                        }
                        patharray[pathend++] = point;
                    }
                    point.x = cppt.x - crpt.x;
                    point.y = cppt.y - crpt.y;
                    patharray[pathend++] = point;
                }
            }

            else

                /* Make a round join (like a round cap) or a cap */

                strokecap(ptmove, (join == 1) ? 1 : gstate.linecap);
        }

        /* The end of the line */

        if (segment == length)
        {   join = ejoin;
            cppt = eppt;
        }
        else
        {   join = -1;
            s = segment / linelen;
            cppt.x += cvpt.x * s;
            cppt.y += cvpt.y * s;
        }

        if ((dashcnt & 1) == 0)
        {

            /* If the line ends with a join make a butt cap, otherwise end
             * with the line cap */

            strokecap(ptline, (join >= 0) ? 0 : gstate.linecap);

            /* Close the path */

            point = patharray[pathseg];
            point.type = ptclosex;
            checkpathsize(pathend + 1);
            patharray[pathend++] = point;
        }

        join = -1;
        length -= segment;
        if (length == 0.0) break;
    }
}

/* Stroke a line cap */

void strokecap(int type, int cap)
{   struct point points[7];
    float fx, fy, rx, ry;

    /* At the beginning caps face the opposite way to the end */

    if (type == ptmove)
    {   fx = -cfpt.x;
        fy = -cfpt.y;
        rx = -crpt.x;
        ry = -crpt.y;
    }
    else
    {   fx =  cfpt.x;
        fy =  cfpt.y;
        rx =  crpt.x;
        ry =  crpt.y;
    }

    points[0].type = type;
    points[0].x = cppt.x - rx;
    points[0].y = cppt.y - ry;
    points[6].type = ptline;
    points[6].x = cppt.x + rx;
    points[6].y = cppt.y + ry;

    /* Butt cap */

    if      (cap == 0)
    {   checkpathsize(pathend + 2);
        patharray[pathend++] = points[0];
        patharray[pathend++] = points[6];
    }

    /* Projecting square cap */

    else if (cap == 2)
    {   points[0].x += fx;
        points[0].y += fy;
        points[6].x += fx;
        points[6].y += fy;
        checkpathsize(pathend + 2);
        patharray[pathend++] = points[0];
        patharray[pathend++] = points[6];
    }

    /* Round cap.  Construct a Bezier curve approximation (in 2 sections) and
     * flatten it.  We always use two sections because they are faster to
     * construct than to flatten */

    else
    {   points[3].type = ptcurve;
        points[3].x = cppt.x + fx;
        points[3].y = cppt.y + fy;
        fx *= f43rt2;
        fy *= f43rt2;
        rx *= f43rt2;
        ry *= f43rt2;
        points[1] = points[0];
        points[1].x += fx;
        points[1].y += fy;
        points[2] = points[3];
        points[2].x -= rx;
        points[2].y -= ry;
        points[4] = points[3];
        points[4].x += rx;
        points[4].y += ry;
        points[5] = points[6];
        points[5].x += fx;
        points[5].y += fy;
        checkpathsize(pathend + 1);
        patharray[pathend++] = points[0];
        flattencurve(&points[0], 0);
        flattencurve(&points[3], 0);
    }
}

/* Make a new clipping path by intersecting it with the current path */

void clip(int rule)
{   struct point point, *ppoint;
    struct clip clip1, clip2, *pclip1, *pclip2, *pclipx;
    double a1, b1, c1, a2, b2, c2;
    double l1, l2, l3;
    double h11, h12, h21, h22, hh;
    int xx, yy, x1, y1;
    int count, count1, count2, step;
    int clipold, clipnew, clipmax, clipflg;
    int cdir1, cdir2, fdir1, fdir2;

    /* Set up the default clip path */

    if (!gstate.clipflag)
    {   cliplength(5);
        gstate.pathbeg = gstate.clipbeg;
        count = gstate.pathend;
        clippath();
        gstate.pathbeg = gstate.pathend;
        gstate.pathend = count;
    }

    /* Set up the clip array.  We convert the coordinates to fixed point,
     * with a scale factor of 256.  Then when we come to multiplying the
     * values to calculate the intersections it will fit into 48 bits or
     * so, a reasonable minimum accuracy for double precision floating
     * point */

    clipend = 0;
    for (count1 = gstate.clipbeg; count1 < gstate.pathend; count1++)
    {   ppoint = &patharray[count1];
        if (ppoint->type != ptmove)
        {   if (count1 < gstate.pathbeg)
            {   clip1.cdir = 1;
                clip1.fdir = 0;
            }
            else
            {   clip1.cdir = 0;
                clip1.fdir = 1;
            }
            clip1.flag = 0;
            clip1.swap = 0;
            clip1.x1 = floor(ppoint[-1].x * 256.0f + 0.5);
            clip1.y1 = floor(ppoint[-1].y * 256.0f + 0.5);
            clip1.x2 = floor(ppoint[0].x * 256.0f + 0.5);
            clip1.y2 = floor(ppoint[0].y * 256.0f + 0.5);
            clipline(&clip1);
        }
    }

    /* Split the lines whenever they intersect.  Owing to rounding errors
     * The resulting new lines are not in exactly the same place as before,
     * so we loop checking for intersections until we don't find any more
     */

    clipnew = 0;
    for (;;)
    {
        /* Shell sort the array in order of y1 */

        count = clipend;
        for (;;)
        {   count = count / 3 + 1;
            for (count1 = count; count1 < clipend; count1++)
                for (count2 = count1 - count;
                     count2 >= 0 &&
                         clipptr[count2]->y1 > clipptr[count2 + count]->y1;
                     count2 -= count)
                {    pclip1 = clipptr[count2];
                     clipptr[count2] = clipptr[count2 + count];
                     clipptr[count2 + count] = pclip1;
                }
            if (count == 1) break;
        }

        /* Exit (with the array sorted) if we have no new lines */

        if (clipnew == clipend) break;

        /* Locate all the intersections for each line */

        count1 = 0;
        clipnew = clipend;
        while (count1 < clipend - 1)
        {   count2 = count1;

            /* We don't need to check for intersections with the new lines
             * we created by intersecting with ours */

            clipmax = clipend - 1;
            while (count2 < clipmax)
            {   count2++;
                pclip1 = clipptr[count1];
                pclip2 = clipptr[count2];

                /* When we get to the first line whose y1 is greater than
                 * our y2 we can skip all the way to the beginning of the
                 * new lines */

                if (pclip2->y1 > pclip1->y2)
                {   if (count2 < clipnew) count2 = clipnew;
                    continue;
                }

                /* Skip lines whose bounding boxes do not intersect ours */

                if (pclip2->y2 < pclip1->y1)
                    continue;
                if (min(pclip1->x1,pclip1->x2) > max(pclip2->x1,pclip2->x2))
                    continue;
                if (max(pclip1->x1,pclip1->x2) < min(pclip2->x1,pclip2->x2))
                    continue;

                /* If the two endpoints of each line are on opposite sides
                 * of the other line, the lines must intersect.  If one is
                 * on the line but not the other, they intersect at the end.
                 * If both are on the line they are colinear.  The distance
                 * from a point (x,y) to a line (a*X +b*Y + c = 0) is given
                 * by (a*x + b*y *c) / sqrt(a**2 + b**2).  We don't bother
                 * with the square root as we only want to know the
                 * relative distances.  The test is exact as long as the
                 * numbers (integer values) we are using are not too big to
                 * be represented exactly in double precision floating point
                 */

                clip1 = *pclip1;
                clip2 = *pclip2;
                a1 = clip1.y2 - clip1.y1;
                b1 = clip1.x2 - clip1.x1;
                c1 = (double) clip1.x2 * (double) clip1.y1 -
                     (double) clip1.x1 * (double) clip1.y2;
                h11 = a1 * clip2.x1 - b1 * clip2.y1 + c1;
                h12 = a1 * clip2.x2 - b1 * clip2.y2 + c1;
                if (h11 > 0.0 && h12 > 0.0) continue;
                if (h11 < 0.0 && h12 < 0.0) continue;

                a2 = clip2.y2 - clip2.y1;
                b2 = clip2.x2 - clip2.x1;
                c2 = (double) clip2.x2 * (double) clip2.y1 -
                     (double) clip2.x1 * (double) clip2.y2;
                h21 = a2 * clip1.x1 - b2 * clip1.y1 + c2;
                h22 = a2 * clip1.x2 - b2 * clip1.y2 + c2;
                if (h21 > 0.0 && h22 > 0.0) continue;
                if (h21 < 0.0 && h22 < 0.0) continue;

                /* If the lines are colinear we treat them as intersecting
                 * at the endpoints.  To find out what order the endpoints
                 * lie in, we take the beginning of line 1 as our origin
                 * and calculate the dot products of the displacements of
                 * the other endpoints with line 1 itself.  We then split
                 * the lines, knowing that the two lines must be in the
                 * same direction, as we made sure y1 < y2 or x1 < x2
                 */

                if (h11 == 0.0 && h12 == 0.0)
                {   l1 = b1 * b1 + a1 * a1;
                    l2 = (clip2.x1 - clip1.x1) * b1 +
                         (clip2.y1 - clip1.y1) * a1;
                    l3 = (clip2.x2 - clip1.x1) * b1 +
                         (clip2.y2 - clip1.y1) * a1;

                    if      (l2 < 0 && l3 < l1)
                    {   clip1.x2 = pclip1->x1 = clip2.x2;
                        clip1.y2 = pclip1->y1 = clip2.y2;
                        clip2.x1 = pclip2->x2 = clip1.x1;
                        clip2.y1 = pclip2->y2 = clip1.y1;
                    }
                    else if (l2 <= 0 && l3 >= l1)
                    {   clip1 = clip2;
                        clip1.x2 = pclip2->x1 = pclip1->x1;
                        clip1.y2 = pclip2->y1 = pclip1->y1;
                        clip2.x1 = pclip2->x2 = pclip1->x2;
                        clip2.y1 = pclip2->y2 = pclip1->y2;
                    }
                    else if (l2 >= 0 && l3 <= l1)
                    {   clip2 = clip1;
                        clip1.x2 = pclip1->x1 = pclip2->x1;
                        clip1.y2 = pclip1->y1 = pclip2->y1;
                        clip2.x1 = pclip1->x2 = pclip2->x2;
                        clip2.y1 = pclip1->y2 = pclip2->y2;
                    }
                    else if (l2 > 0 && l3 > l1)
                    {   clip1.x1 = pclip1->x2 = clip2.x1;
                        clip1.y1 = pclip1->y2 = clip2.y1;
                        clip2.x2 = pclip2->x1 = clip1.x2;
                        clip2.y2 = pclip2->y1 = clip1.y2;
                    }
                    clipline(&clip1);
                    clipline(&clip2);
                }

                /* Otherwise calculate the intersection */

                else
                {   hh = h22 - h21;
                    xx = floor((clip1.x1 * h22 - clip1.x2 * h21) / hh + 0.5);
                    yy = floor((clip1.y1 * h22 - clip1.y2 * h21) / hh + 0.5);
                    if (xx != clip1.x1 || yy != clip1.y1)
                    {   pclip1->x2 = clip1.x1 = xx;
                        pclip1->y2 = clip1.y1 = yy;
                        clipline(&clip1);
                    }
                    pclip2 = clipptr[count2];
                    if (xx != clip2.x1 || yy != clip2.y1)
                    {   pclip2->x2 = clip2.x1 = xx;
                        pclip2->y2 = clip2.y1 = yy;
                        clipline(&clip2);
                    }
                }

                /* We must check the directions in case of rounding error */

                clipswap(clipptr[count1]);
                clipswap(clipptr[count2]);
            }
            count1++;
        }
    }

    /* Count the winding number for each line.  We construct a test line
     * running horizontally from the left, at a height just greater than y1.
     * (If the line is horizontal, we regard the test line as being bent at
     * the end so it meets the line just to the right of x1.)  If we find
     * any lines colinear with ours we chain them together */

    count1 = 0;
    clipold = 0;
    clipflg = 0;
    while (count1 < clipend)
    {   pclip1 = clipptr[count1++];
        if (pclip1->flag != 0) continue;

        count2 = clipold;
        cdir1 = fdir1 = 0;
        cdir2 = pclip1->cdir;
        fdir2 = pclip1->fdir;
        pclipx = NULL;

        /* Scan the other lines for intersections with the test line */

        while (count2 < clipend)
        {   pclip2 = clipptr[count2];
            count2++;
            if (pclip2 == pclip1) continue;

            /* If y2 is less than our y1 we won't need to scan the line
             * again.  Swap it to the beginning and step past it next time */

            if (pclip2->y2 < pclip1->y1)
            {   clipptr[count2 - 1]  = clipptr[clipold];
                clipptr[clipold] = pclip2;
                clipold++;
                continue;
            }

            /* Stop scanning when y1 is greater than ours */

            if (pclip2->y1 > pclip1->y1) break;

            /* If its y2 is greater than our y1, then if it passes to the
             * left of our x1 it must intersect the test line */

            if (pclip2->y2 > pclip1->y1)
            {   a1 = pclip2->x1 * (double) (pclip2->y2 - pclip1->y1) +
                     pclip2->x2 * (double) (pclip1->y1 - pclip2->y1);
                a2 = pclip1->x1 * (double) (pclip2->y2 - pclip2->y1);

                if      (a1 < a2)
                {   cdir1 += pclip2->cdir;
                    fdir1 += pclip2->fdir;
                }

            /* If it passes through our x1 we have to compare the slopes.
             * The cross procuct is the sine of the angle between them.  If
             * its slope is greater than ours it must intersect the test
             * line.  If the slopes are the same they must be colinar;
             * calculate the winding number for the far side of the line
             * and chain it */

                else if (a1 == a2)
                {   hh = (double) (pclip1->x2 - pclip1->x1) *
                         (double) (pclip2->y2 - pclip2->y1) -
                         (double) (pclip2->x2 - pclip2->x1) *
                         (double) (pclip1->y2 - pclip1->y1);
                    if      (hh > 0.0)
                    {   cdir1 += pclip2->cdir;
                        fdir1 += pclip2->fdir;
                    }
                    else if (hh == 0.0)
                    {   cdir2 += pclip2->cdir;
                        fdir2 += pclip2->fdir;
                        if (pclip2->flag == 0)
                        {   pclip2->chain = pclipx;
                            pclipx = pclip2;
                        }
                    }
                }
            }

            /* Horizontal lines only intersect the test line if they are
             * colinear with ours */

            else
                if (pclip1->y1 == pclip1->y2 &&
                    pclip2->y1 == pclip2->y2 &&
                    pclip2->x1 == pclip1->x1)
                {   cdir2 += pclip2->cdir;
                    fdir2 += pclip2->fdir;
                    if (pclip2->flag == 0)
                    {   pclip2->chain = pclipx;
                        pclipx = pclip2;
                    }
                }
        }

        /* If the line is on the boundary of the intersection of the paths,
         * we keep it.  If it is interior to or on the boundary of the path
         * it does not belong to, or exterior to or on the boundary of the
         * other, we must have two or more coincident lines; we keep it and
         * its pair to preserve the degenerate path, discarding any others
         * that are also coincident.  Otherwise we discard it.  (We don't
         * preserve paths where both the clip and fill paths are
         * degenerate.)  Flag the lines for their ends to be swapped if they
         * are right or bottom edges */

        cdir2 = (((cdir1 + cdir2) & rule) != 0);
        fdir2 = (((fdir1 + fdir2) & rule) != 0);
        cdir1 = (( cdir1          & rule) != 0);
        fdir1 = (( fdir1          & rule) != 0);

        if ((cdir1 & fdir1) == (cdir2 & fdir2))
            if ((pclip1->fdir != 0 && (cdir1 | cdir2) && !(fdir1 & fdir2)) ||
                (pclip1->cdir != 0 && (fdir1 | fdir2) && !(cdir1 & fdir2)))
            {   pclipx->flag = 2;           /* Keep its pair */
                pclipx->swap = 1;           /* Swap one line */
                pclipx = pclipx->chain;     /* Discard the rest */
            }
            else
            {   pclip1->flag = 1;           /* Discard the line */
                clipflg++;
                pclipx = NULL;              /* Leave the rest */
            }
        else
            if ((cdir1 & fdir1) != 0)
                pclip1->swap = 1;           /* Swap right and bottom edges */

        while (pclipx)                      /* Discard the rest */
        {   pclipx->flag = 1;
            clipflg++;
            pclipx = pclipx->chain;
        }
    }

    /* Swap the lines so they are pointing in their proper directions */

    count1 = clipend;
    pclip1 = &cliparray[0];
    while (count1--)
    {   if (pclip1->swap) clipswap(pclip1);
        pclip1++;
    }

    /* Now join the lines back up to make the path.  The lines are in
     * approximate order of y2, so we optimise our search on that basis.
     * We start by picking any line; then we find the one that joins it */

    cliplength(0);
    point.type = ptmove;
    count = 0;
    step = 0;
    pathend = gstate.pathend;

    while (clipflg < clipend)
    {
        /* We step alternately backwards and forwards */

        count = count + step;
        if (step > 0)
            step = -step - 1;
        else
            step = -step + 1;

        /* When the array is half empty we compact it */

        if (clipflg + clipflg > clipend)
        {   count1 = 0;
            count2 = 0;
            while (count2 < clipend)
            {   if (count2 == count) count = count1;
                pclip2 = clipptr[count2++];
                if (pclip2->flag != 1) clipptr[count1++] = pclip2;
            }
            clipend -= clipflg;
            clipflg = 0;
            if (count >= clipend) count = clipend - 1;
            step = 1;
        }

        /* Search for a line that joins the current point */

        if (count >= 0 && count < clipend)
        {   pclip1 = clipptr[count];
            if (pclip1->flag != 1)
            {
                /* At the beginning of a subpath add a move */

                if (point.type == ptmove)
                {   x1 = pclip1->x1;
                    y1 = pclip1->y1;
                    point.x = x1 / 256.0f;
                    point.y = y1 / 256.0f;
                    checkpathsize(pathend + 1);
                    patharray[pathend++] = point;
                    point.type = ptline;
                    xx = pclip1->x2;
                    yy = pclip1->y2;
                    step = 0;
                }

                /* Otherwise look for a line that joins the current point */

                else if (pclip1->y1 == yy && pclip1->x1 == xx)
                {   xx = pclip1->x2;
                    yy = pclip1->y2;
                    step = 0;
                }

                /* Found a line; if it returns to the beginning close the
                 * subpath */

                if (step == 0)
                {   pclip1->flag = 1;
                    clipflg++;
                    checkpathsize(pathend + 1);
                    point.x = xx / 256.0f;
                    point.y = yy / 256.0f;
                    if (yy == y1 && xx == x1)
                    {   point.type = ptclosex;
                        patharray[pathend++] = point;
                        point.type = ptmove;
                    }
                    else
                        patharray[pathend++] = point;
                    step = 1;
                }
            }
        }
    }

    /* We now have the new clip path, at the end of the path.  To put the
     * clip path first, we first swap the order of the contents of the two
     * paths individually, then swap them both together */

    swappath(gstate.pathbeg, gstate.pathend);
    swappath(gstate.pathend, pathend);
    swappath(gstate.pathbeg, pathend);
    gstate.pathbeg = gstate.clipbeg + pathend - gstate.pathend;
    gstate.pathend = pathend;
    gstate.clipflag = 1;
}

/* Add a clip line to the array */

void clipline(struct clip *pclip)
{   struct clip *pc;

    if (pclip->x1 != pclip->x2 || pclip->y1 != pclip->y2)
    {   clipswap(pclip);
        checkclipsize(clipend + 1, clipend);
        pc = &cliparray[clipend];
        *pc = *pclip;
        clipptr[clipend++] = pc;
    }
}

/* Swap a clip line if necessary so y2 > y1 (or x2 > x1) */

void clipswap(struct clip *pclip)
{   int sw;
    if (pclip->swap ||
        pclip->y1 > pclip->y2 ||
           (pclip->y1 == pclip->y2 && pclip->x1 > pclip->x2))
    {   pclip->cdir = -pclip->cdir;
        pclip->fdir = -pclip->fdir;
        sw = pclip->x1;
        pclip->x1 = pclip->x2;
        pclip->x2 = sw;
        sw = pclip->y1;
        pclip->y1 = pclip->y2;
        pclip->y2 = sw;
    }
}

/* Swap the order of the points in a path */

void swappath(int beg, int end)
{   struct point point, *ppoint1, *ppoint2;
    ppoint1 = &patharray[beg];
    ppoint2 = &patharray[end];
    for (;;)
    {   ppoint2--;
        if (ppoint1 >= ppoint2) break;
        point = *ppoint1;
        *ppoint1 = *ppoint2;
        *ppoint2 = point;
        ppoint1++;
    }
}

/* End of file "postgraph.c" */
