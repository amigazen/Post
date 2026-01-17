/* PostScript interpreter file "postchar.c" - character routines */
/* (C) Adrian Aylward 1989, 1991 */

# include "post.h"

/* Routines */

extern void cachefast(struct fcrec *pfcrec,
                      int xpos1, int ypos1, int y1, int y2);
extern void cacheslow(struct fcrec *pfcrec,
                      int xpos1, int ypos1, int y1, int y2);

/* Initialise the character routines */

void initchar(void)
{   struct object token, *aptr;
    char *s;
    int i;

    /* Create various names for character operations */

    for (i = 0; i < charmax; i++)
        nametoken(&charname[i], chartable[i], -1, 0);

    /* Create the standard encoding vector */

    token.type = typearray;
    token.flags = 0;
    token.length = 256;
    token.value.vref = arrayalloc(256);
    aptr = vmaptr(token.value.vref);
    for (i = 0; i < 256; i++)
    {   s = ((i & 0x60) == 0) ? NULL :
                                stdentable[i - ((i < 0x80) ? 0x20 : 0x40)];
        if (s)
            nametoken(&aptr[i], s, -1, 0);
        else
            aptr[i] = charname[charnotdef];
    }
    dictput(dictstack[0].value.vref, &charname[charstdencoding], &token);
    stdencoding = aptr;

    /* Create the font directorary */

    dicttoken(&fontdir, fontdictsize);
    dictput(dictstack[0].value.vref, &charname[charfontdirectory], &fontdir);

    /* Initialise the font cache */

    fmcount = 0;
    fmcache = vmallocv(sizeof (struct fmrec) * fmsize);
    fccache = vmallocv(sizeof (struct fcrec *) * fcsize);
    fcptr = fcbeg = memfbeg;
    fcend = (struct fcrec *) ((char *) memfbeg + memflen);
    fclen = memflen;
    if (fclen < sizeof (struct fcrec))
        fclen = 0;
    else
    {   fcbeg->reclen = fclen;
        fcbeg->count = 0;
    }
    istate.fclim = fclen;
    fclimit = fclen / 40;

    /* Initialise the current font */

    token.type = 0;
    token.flags = 0;
    token.length = 0;
    token.value.ival = 0;
    gstate.font = token; 

    /* Initialise type 1 fonts */

    initfont();
}

/* Restore the font cache */

void vmrestfont(struct vmframe *vmframe)
{   struct fmrec *pfmrec;
    struct fcrec *pfcrec, *pfcnxt, **ppfcrec;
    int i;

    /* Scan the make cache to purge entries more recent than the vm save */

    pfmrec = &fmcache[0];
    i = fmsize;
    while (i--)
    {   if (pfmrec->id != 0)
            if (vmscheck(vmframe, pfmrec->dref)) pfmrec->id = 0;
        pfmrec++;
    }

    /* Scan the character cache, purging entries from the hash chains */

    if (fclen == 0) return;

    for (i = 0; i < fcsize; i++)
    {   ppfcrec = &fccache[i];
        for (;;)
        {   pfcrec = *ppfcrec;
            if (pfcrec == NULL) break;
            if (vmscheck(vmframe, pfcrec->nref) ||
                vmscheck(vmframe, pfcrec->dref))
            {   *ppfcrec = pfcrec->chain;
                pfcrec->count = 0;
            }
            else
                ppfcrec = &pfcrec->chain;
        }
    }

    /* Scan the character cache, joining adjacent free records */

    pfcrec = fcbeg;
    while (pfcrec != fcend)
    {   if (pfcrec->count == 0)
        {   pfcnxt = (struct fcrec *) ((char *) pfcrec + pfcrec->reclen);
            if (pfcnxt != fcend && pfcnxt->count == 0)
            {   if (fcptr == pfcnxt) fcptr = pfcrec;
                pfcrec->reclen += pfcnxt->reclen;
            }
        }
        pfcrec = (struct fcrec *) ((char *) pfcrec + pfcrec->reclen);
    }
}

/* Define a font */

void definefont(struct object *key, struct object *font)
{   struct object token;

    /* Validate the font.  Get the uniqe id if we have one and construct
     * the font id */

    checkfont(font, 0);
    fontid = 0;
    if (dictget(font->value.vref, &charname[charuniqueid], &token, 0))
    {   if (token.type != typeint || (token.value.ival & 0xff000000) != 0)
            error(errinvalidfont);
        fontid = (fonttype << 24) | token.value.ival;
    }
    token.type = typefont;
    token.flags = 0;
    token.length = 0;
    token.value.ival = fontid;
    dictput(font->value.vref, &charname[charfid], &token);

    /* Make it read only and insert it into the font directory */

    vmdptr(font->value.vref)->flags |= flagwprot;
    dictput(fontdir.value.vref, key, font);
}

/* Find a font */

void findfont(struct object *key, struct object *font)
{   if (!dictget(fontdir.value.vref, key, font, 0)) error(errinvalidfont);
}

/* Make a font */

void makefont(struct object *font, float *matrix)
{   struct object token;
    struct dictionary *odict, *ndict;
    struct fmrec *pfmrec, *pfmrcu;
    vmref ndref;
    float oldmatrix[6], newmatrix[6];
    int count, i;

    /* Validate the font and calculate the new matrix */

    checkfont(font, 1);
    getmatrix(fontmatrix, oldmatrix);
    multiplymatrix(newmatrix, oldmatrix, matrix);

    /* Look in the make cache to see if we have made one like this before.
     * We only cache fonts that have unique ids.  If we find one update the
     * usage counter on its cache record */

    fmcount++;
    if (fontid != 0)
    {   pfmrec = &fmcache[0];
        i = fmsize;
        while (i--)
        {   if (pfmrec->id == fontid &&
                pfmrec->encodlen == fontencodlen &&
                pfmrec->encoding == fontencoding &&
                memcmp((char *) pfmrec->matrix,
                       (char *) newmatrix, sizeof newmatrix) == 0)
            {   pfmrec->count = fmcount;
                font->value.vref = pfmrec->dref;
                return;
            }
            pfmrec++;
        }
    }

    /* Copy the font.  Replace the matrix with the new one */

    odict = vmdptr(font->value.vref);
    ndref = vmalloc(odict->length);
    ndict = vmdptr(ndref);
    memcpy((char *) ndict, (char *) odict, odict->length);
    ndict->saved = vmnest;
    ndict->flags &= ~flagwprot;
    fontmatrix = arrayalloc(6);
    token.type = typearray;
    token.flags = 0;
    token.length = 6;
    token.value.vref = fontmatrix;
    putmatrix(fontmatrix, newmatrix);
    dictput(ndref, &charname[charfontmatrix], &token);
    ndict->flags |= flagwprot;
    font->value.vref = ndref;

    /* Save the new font in the cache.  Look for an empty slot; if we don't
     * find one use the least recently used */

    if (fontid != 0)
    {   pfmrcu = pfmrec = &fmcache[0];
        i = fmsize;
        count = 0x7fffffff;
        while (i--)
        {   if (pfmrec->count < count)
            {   count = pfmrec->count;
                pfmrcu = pfmrec;
            }
            if (pfmrec->id == 0)
            {   pfmrcu = pfmrec;
                break;
            }
            pfmrec++;
        }
        pfmrcu->count = fmcount;
        pfmrcu->id = fontid;
        pfmrcu->encodlen = fontencodlen;
        pfmrcu->encoding = fontencoding;
        memcpy((char *) pfmrcu->matrix,
               (char *) newmatrix, sizeof newmatrix);
        pfmrcu->dref = ndref;
    }
}

/* Set a font */

void setfont(struct object *font)
{   if (font->type != typenull) checkfont(font, 1);
    gstate.font = *font;
}

/* Check a font for validity */

void checkfont(struct object *font, int fid)
{   struct object token;
    if (font->type != typedict) error(errtypecheck);
    if (!dictget(font->value.vref, &charname[charfonttype], &token, 0))
        error(errinvalidfont);
    if (token.type != typeint ||
        (token.value.ival != 1 && token.value.ival != 3))
        error(errinvalidfont);
    fonttype = token.value.ival;
    if (!dictget(font->value.vref, &charname[charfontmatrix], &token, 0))
        error(errinvalidfont);
    if (token.type != typearray || token.length != 6 ||
        (token.flags & flagrprot))
        error(errinvalidfont);
    fontmatrix = token.value.vref;
    if (!dictget(font->value.vref, &charname[charfontbbox], &token, 0))
        error(errinvalidfont);
    if ((token.type != typearray && token.type != typepacked) ||
        token.length != 4 || (token.flags & flagrprot))
        error(errinvalidfont);
    if (!dictget(font->value.vref, &charname[charencoding], &token, 0))
        error(errinvalidfont);
    if (token.type != typearray || (token.flags & flagrprot))
        error(errinvalidfont);
    fontencodlen = token.length;
    fontencoding = token.value.vref;
    if (dictget(font->value.vref, &charname[charfid], &token, 0) != fid)
        error(errinvalidfont);
    if (fid)
    {   if (token.type != typefont) error(errinvalidfont);
        fontid = token.value.ival;
    }
}

/* Show a string (type = 0,1 charpath; 2 stringwidth; 3 show) */

void show(struct object *string, int type,
          struct point *width, int wchar, struct object *kproc)
{   struct object token, font, *encoding, bproc;
    struct point cpoint, fpoint;
    struct fcrec fcrec, *pfcrec, **ppfcrec;
    char *sptr;
    float newctm[6], oldctm[6], matrix[6];
    int encodlen, length, schar, iflag, ftype;
    int id, nest, i, lev;

    if (istate.flags & intgraph) error(errundefined);

    lev = flushlevel(-1);

    /* Look up the matrix, encoding, font id */

    iflag = 0;
    font = gstate.font;
    if (font.type != typedict) error(errinvalidfont);
    if (!dictget(font.value.vref, &charname[charfontmatrix], &token, 0))
        error(errinvalidfont);
    getmatrix(token.value.vref, matrix);
    if (!dictget(font.value.vref, &charname[charencoding], &token, 0))
        error(errinvalidfont);
    encodlen = token.length;
    encoding = vmaptr(token.value.vref);
    if (!dictget(font.value.vref, &charname[charfid], &token, 0))
        error(errinvalidfont);
    id = token.value.ival;

    /* For stringwidth start at zero */

    if (type == 2)
    {   cpoint.type = ptmove;
        cpoint.x = cpoint.y = 0.0;
    }

    /* Loop for every character in the string */

    length = string->length;
    sptr = vmsptr(string->value.vref);
    while (length--)
    {   schar = *((unsigned char *) sptr);
        sptr++;

        /* Find the current point */

        if (type != 2)
        {   if (gstate.pathend == gstate.pathbeg) error(errnocurrentpoint);
            closepath(ptclosei);
            cpoint = patharray[gstate.pathend - 1];
        }

        /* Build the new ctm (current point rounded to pixel boundary) */

        memcpy((char *) oldctm, (char *) gstate.ctm, 4 * sizeof (float));
        oldctm[4] = cpoint.x;
        oldctm[5] = cpoint.y;
        multiplymatrix(newctm, matrix, oldctm);
        fpoint.x = newctm[4] = floor(newctm[4] + 0.5);
        fpoint.y = newctm[5] = floor(newctm[5] + 0.5);

        /* Don't use the cache for charpath */

        fcrec.reclen = 0;
        fcrec.width.x = fcrec.width.y = 0.0;
        if (type < 2) goto nocache;

        /* Construct the cache key */

        if (schar < encodlen)
        {   token = encoding[schar];
            if (token.type != typename) error(errinvalidfont);
        }
        else
            token = charname[charnotdef];
        fcrec.id = id;
        fcrec.nref = token.value.vref;
        fcrec.dref= (id == 0) ? font.value.vref : 0;
        memcpy((char *) fcrec.matrix, (char *) newctm, 4 * sizeof (float));
        fcrec.hash = id;
        fcrec.hash = fcrec.hash * 12345 + (int) fcrec.dref;
        fcrec.hash = fcrec.hash * 12345 + (int) fcrec.nref;
        for (i = 0; i < 4 ; i++)
            fcrec.hash = fcrec.hash * 12345 + *((int *) &fcrec.matrix[i]);

        /* Search the cache for the character */

        for (ppfcrec = &fccache[fcrec.hash % fcsize];
             pfcrec = *ppfcrec, pfcrec != NULL;
             ppfcrec = &pfcrec->chain)
            if (pfcrec->hash == fcrec.hash &&
                pfcrec->dref == fcrec.dref &&
                pfcrec->nref == fcrec.nref &&
                pfcrec->matrix[0] == fcrec.matrix[0] &&
                pfcrec->matrix[1] == fcrec.matrix[1] &&
                pfcrec->matrix[2] == fcrec.matrix[2] &&
                pfcrec->matrix[3] == fcrec.matrix[3] &&
                pfcrec->id == fcrec.id)
                goto incache;

        /* Not in the cache; look up the build data.  Type 1 uses external
         * data as we can't recurse, whereas type 3 uses local data */

nocache:
        if (iflag == 0)
        {   if (!dictget(font.value.vref, &charname[charfonttype],
                                          &token, 0))
                error(errinvalidfont);
            ftype = token.value.ival;
            if (ftype == 1)
                initbuild(font.value.vref);
            else
            {   if (!dictget(font.value.vref, &charname[charbuildchar],
                                              &bproc, 0))
                    error(errinvalidfont);
            }
            iflag = 1;
        }

        /* Build the character.  Push the interpreter state; save the
         * graphics state and set up the new ctm.  Type 1 executes the
         * character string, whereas type 3 interprets the build proc */

        nest = opernest;
        pushint();
        istate.flags = intchar;
        istate.type = type;
        istate.pfcrec = &fcrec;
        gsave();
        gstate.pathend = gstate.pathbeg;
        memcpy((char *) gstate.ctm, (char *) newctm, sizeof newctm);
        gstate.linecap = 0;
        gstate.linejoin = 0;
        gstate.mitrelimit = 10.0;
        gstate.mitresin = 0.198997;
        gstate.dashoffset = 0.0;
        gstate.dasharray.length = 0;

        if (ftype == 1)
            buildchar(schar);
        else
        {   gstate.flatness = 1.0;
            gstate.linewidth = 1.0;
            if (opernest + 2 > operstacksize) error(errstackoverflow);
            operstack[opernest++] = font;
            token.type = typeint;
            token.flags = 0;
            token.length = 0;
            token.value.ival = schar;
            operstack[opernest++] = token;
            interpret(&bproc);
        }

        grest();
        pfcrec = istate.pfcrec;
        popint();
        if (opernest > nest) error(errstackoverflow);
        if (opernest < nest) error(errstackunderflow);
    
        /* Increment the cache record usage count.  If the character is in
         * the cache image it onto the page.  Add the width to the current
         * point */
incache:
        pfcrec->count++;
        if (type == 3 && pfcrec->reclen != 0)
            cacheimage(pfcrec, &fpoint);
        cpoint.x += pfcrec->width.x;
        cpoint.y += pfcrec->width.y;

        /* Adjust the width */

        if (type == 3 && width != NULL)
        {   if (schar == wchar)
            {   cpoint.x += width[0].x;
                cpoint.y += width[0].y;
            }
            cpoint.x += width[1].x;
            cpoint.y += width[1].y;
        }

        /* Update the current point */

        if (type != 2)
        {   cpoint.type = ptmove;
            if (gstate.pathbeg == gstate.pathend ||
                patharray[gstate.pathend - 1].type != ptmove)
                checkpathsize(gstate.pathend + 1);
            else
                gstate.pathend--;
            patharray[gstate.pathend++] = cpoint;
        }

        /* Call the kerning proc */

        if (kproc != NULL && length != 0)
        {   if (opernest + 2 > operstacksize) error(errstackoverflow);
            nest = opernest;
            token.type = typeint;
            token.flags = 0;
            token.length = 0;
            token.value.ival = schar;
            operstack[opernest++] = token;
            token.value.ival = *((unsigned char *) sptr);
            operstack[opernest++] = token;
            pushint();
            interpret(kproc);
            popint();
            if (opernest > nest) error(errstackoverflow);
            if (opernest < nest) error(errstackunderflow);
        }
    }

    /* For stringwidth return the width */

    if (type == 2)
        *width = cpoint;

    flushlevel(lev);
}

/*  Append the character path to the path of the saved graphics state */

void charpath(void)
{   struct gstate *pgstate;
    struct point *ppoint1, *ppoint2;
    int pathlen, len, gap, snest;

    /* Locate the graphics state we saved before beginning the build proc */

    snest = istate.gbase;
    pgstate = &gstack[snest];

    /* Shuffle all the newer paths up to make room */

    len = gstate.pathend - pgstate->pathend;
    gap = pathlen = gstate.pathend - gstate.pathbeg;
    if (patharray[pgstate->pathend - 1].type == ptmove) gap--;
    checkpathsize(gstate.pathend + gap);
    ppoint2 = &patharray[gstate.pathend];
    ppoint1 = ppoint2 + gap;
    while (len--) *--ppoint1 = *--ppoint2;

    /* Adjust the pointers to the paths we have moved */

    pgstate->pathend += gap;
    while (++snest < gnest)
    {    pgstate++;
         pgstate->clipbeg += gap;
         pgstate->pathbeg += gap;
         pgstate->pathend += gap;
    }
    gstate.clipbeg += gap;
    gstate.pathbeg += gap;
    gstate.pathend = gstate.pathbeg;

    /* Copy the path */

    ppoint1 -= pathlen;
    ppoint2 = &patharray[gstate.pathbeg];
    while (pathlen--) *ppoint1++ = *ppoint2++;
}

/* Set character width */

void setcharwidth(struct point *width)
{   if (!(istate.flags & intchar)) error(errundefined);
    istate.pfcrec->width = *width;
}

/* Set cache device */

void setcachedevice(struct point *ll, struct point *ur, int ftype)
{   struct fcrec *pfcrec, *pfcnxt, **ppfcrec;
    float llx, lly, urx, ury;
    int reclen, length, xbytes, xsize, ysize, fclim;

    /* Calculate the bitmap dimensions, check cache limit */

    llx = floor(ll->x);
    lly = floor(ll->y);
    urx = floor(ur->x - llx) + 1.0;
    ury = floor(ur->y - lly) + 1.0;
    if (fabs(llx) > 10000.0 || urx > 10000.0) return;
    if (fabs(lly) > 10000.0 || ury > 10000.0) return;
    xsize = urx;
    ysize = ury;
    xbytes = (xsize + 7) >> 3;
    length = xbytes * ysize;
    if (length > fclimit) return;

    /* For type 3 fonts we must ensure there is sufficient space for all
     * the simultaneously active cache records; so we limit the record
     * length to one third of the total length, and set the limit for the
     * next level to one third too (allowing for fragmentation).  For type
     * 1 fonts we can use the full length as there is no recursion */

    reclen =
       (sizeof (struct fcrec) + length + (mcalign - 1)) & ~(mcalign - 1);
    fclim = istate.fclim;
    if (ftype == 3) fclim /= 3;
    if (reclen > fclim) return;
    if (ftype == 1) fclim = 0;
    istate.fclim = fclim;

    /* Find a free cache slot.  Halve the usage counts as we go; free the
     * records when they become empty, joining adjacent free records */

    for (;;)
    {   if (fcptr == fcend) fcptr = fcbeg;
        pfcrec = fcptr;
        for (;;)
        {   if (fcptr->count == 1)
            {   ppfcrec = &fccache[fcptr->hash % fcsize];
                while (*ppfcrec != fcptr) ppfcrec = &((*ppfcrec)->chain);
                *ppfcrec = fcptr->chain;
                pfcnxt = (struct fcrec *) ((char *) fcptr + fcptr->reclen);
                if (pfcnxt != fcend && pfcnxt->count == 0)
                    fcptr->reclen += pfcnxt->reclen;
            }
            if (fcptr->count != -1) fcptr->count >>= 1;
            if (fcptr->count == 0 && fcptr != pfcrec)
                pfcrec->reclen += fcptr->reclen;
            fcptr = (struct fcrec *) ((char *) fcptr + fcptr->reclen);
            if (fcptr == fcend) break;
            if (fcptr->count != 0 && fcptr->count != 1) break;
            if (pfcrec->count != 0) break;
            if (pfcrec->reclen >= reclen) break;
        }
        if (pfcrec->count == 0 && pfcrec->reclen >= reclen) break;
    }

    /* If the record is large enough split it */

    if (pfcrec->reclen - reclen > sizeof (struct fcrec))
    {   fcptr = (struct fcrec *) ((char *) pfcrec + reclen);
        fcptr->count = 0;
        fcptr->reclen = pfcrec->reclen - reclen;
        pfcrec->reclen = reclen;
    }

    /* Set up the cache record, adjust the ctm */

    ppfcrec = &fccache[istate.pfcrec->hash % fcsize];
    pfcrec->chain = *ppfcrec;
    *ppfcrec = pfcrec;
    pfcrec->count = -1;
    pfcrec->dref = istate.pfcrec->dref;
    pfcrec->nref = istate.pfcrec->nref;
    pfcrec->width = istate.pfcrec->width;
    pfcrec->matrix[0] = istate.pfcrec->matrix[0];
    pfcrec->matrix[1] = istate.pfcrec->matrix[1];
    pfcrec->matrix[2] = istate.pfcrec->matrix[2];
    pfcrec->matrix[3] = istate.pfcrec->matrix[3];
    pfcrec->len = length;
    pfcrec->hash = istate.pfcrec->hash;
    pfcrec->id = istate.pfcrec->id;
    pfcrec->xbytes = xbytes;
    pfcrec->xsize = xsize;
    pfcrec->ysize = ysize;
    pfcrec->xoffset = llx;
    pfcrec->yoffset = lly;
    istate.pfcrec = pfcrec;
    gstate.ctm[4] = -llx;
    gstate.ctm[5] = -lly;

    /* Install the cache device */

    gstate.cacheflag = 1;
    gstate.clipflag = 0;
    gstate.gray = 0.0;
    gstate.shade[0] = 0.0;
    gstate.shadeok = 1;
    screenok = 0;
    gstate.dev.buf[0] = (char *) (pfcrec + 1);
    gstate.dev.len = length;
    gstate.dev.depth = 1;
    gstate.dev.xbytes = pfcrec->xbytes;
    gstate.dev.xsize = pfcrec->xsize;
    gstate.dev.ysize = pfcrec->ysize;
    gstate.dev.ybase = 0;
    gstate.dev.yheight = pfcrec->ysize;
    memset(gstate.dev.buf[0], 0, length);
}

/* Get cache status */

void cachestatus(int *status)
{   struct fmrec *pfmrec;
    struct fcrec *pfcrec;
    int count, length, i;
    count = 0;
    pfmrec = &fmcache[0];
    i = fmsize;
    while (i--)
    {   if (pfmrec->id != 0) count++;
        pfmrec++;
    }
    status[2] = count;
    status[3] = fmsize;
    count = 0;
    length = 0;
    if (fclen == 0)
        status[1] = status[5] = 0;
    else
    {   pfcrec = fcbeg;
        while (pfcrec != fcend)
        {   if (pfcrec->count != 0)
            {   count++;
                length += pfcrec->reclen;
            }
            pfcrec = (struct fcrec *) ((char *) pfcrec + pfcrec->reclen);
        }
        status[1] = fclen;
        status[5] = fclen / sizeof (struct fcrec) - 1;
    }
    status[0] = length;
    status[4] = count;
    status[6] = fclimit;
}

/* Set null device */

void nulldevice(void)
{   gstate.dev.depth = 0;
    gstate.dev.len = 0;
    gstate.dev.xoff = 0;
    gstate.dev.yoff = 0;
    gstate.dev.xbytes = 0;
    gstate.dev.xsize = 0;
    gstate.dev.ysize = 0;
    gstate.dev.ybase = 0;
    gstate.dev.yheight = 0;
    gstate.dev.xden = 72;
    gstate.dev.yden = 72;
    gstate.dev.ydir = 1;
    gstate.clipflag = 0;
    gstate.cacheflag = 0;
    gstate.nullflag = 0;
    cliplength(0);
    halfok = gstacksize + 1;
    initctm(gstate.ctm);
}

/* Image the character data from the cache onto the page */

void cacheimage(struct fcrec *pfcrec, struct point *point)
{   struct point *ppoint;
    struct line *pline;
    int xpos1, ypos1;
    int x1, x2, y1, y2, xx;
    int count, cdir;

    setupfill();

    /* Locate the image bounds */

    if (point->x < -1000.0 || point->x > 31000.0) return;
    if (point->y < -1000.0 || point->y > 31000.0) return;

    xpos1 = (int) point->x + pfcrec->xoffset;
    ypos1 = (int) point->y + pfcrec->yoffset;
    y1 = ypos1;
    y2 = ypos1 + pfcrec->ysize;
    if (y1 < gstate.dev.ybase)
        y1 = gstate.dev.ybase;
    if (y2 > gstate.dev.ybase + gstate.dev.ysize)
        y2 = gstate.dev.ybase + gstate.dev.ysize;
    if (y1 == y2) return;

    /* If there is no clip path we always can do a fast image */

    if (!gstate.clipflag)
        cachefast(pfcrec, xpos1, ypos1, y1, y2);

    /* Otherwise we must see if the image is within the clip path */

    else
    {

        /* Set up the clip lines.   We clip them, using the minimum and
         * maximum y values from the image */

        ymax = ylwb = y1 * 256.0;
        ymin = yupb = y2 * 256.0;
        lineend = 0;
        count = gstate.pathbeg - gstate.clipbeg;
        ppoint = &patharray[gstate.clipbeg];
        while (count--)
        {   if (ppoint->type != ptmove) fillline(ppoint - 1, 1, 0);
            ppoint++;
        }
        if (lineend == 0) return;

        /* See if any of the clip lines intersect the image */

        x1 = xpos1 << 16;
        x2 = (xpos1 + pfcrec->xsize) << 16;
        count = lineend;
        pline = &linearray[0];
        cdir = 0;
        while (count)
        {   if (pline->y1 == y1 && pline->y2 != y1 && pline->xx < x1)
                cdir += pline->cdir;
            xx = pline->xx + pline->d1 + pline->d2;
            if (pline->y2 > pline->y1 + 1)
                xx +=  (pline->y2 - pline->y1 - 1) * pline->dx;
            if (xx < x1 && pline->xx >= x2) break;
            if (pline->xx < x1 && xx >= x2) break;
            if (pline->xx >= x1 && pline->xx < x2) break;
            if (xx >= x1 && xx < x2) break;
            pline++;
            count--;
        }

        /* No lines intersect: either entirely visible or invisible */

        if (count == 0)
        {   if (cdir == 0) return;
            cachefast(pfcrec, xpos1, ypos1, y1, y2);
        }

        /* If any lines intersect we must do a slow image */

        else
            cacheslow(pfcrec, xpos1, ypos1, y1, y2);
    }

    flushlpage(y1, y2);
}

/* Slow image the character cache data */

void cacheslow(struct fcrec *pfcrec, int xpos1, int ypos1, int yy, int y2)
{   struct line *pline, **ppline;
    struct halfscreen *hscreen;
    struct halftone *htone;
    unsigned char *cbuf, *cbeg, *cptr; /* cache buffer row base, pointer */
    char *dptr1, *dptr2;        /* device buffer pointers */
    char *hbeg, *hptr;          /* halftone screen row base, pointer */
    int count;                  /* counter */
    int active, discard, sort;  /* lines active, to be discarded, sorted */
    int x1, x2, xp, xx;         /* current, previous x position range */
    int cdir;                   /* clip direction counter */
    int poff;                   /* offset of line from page */
    int xmod, hxsize;           /* position modulo halftone screen */
    int mask1, mask2;           /* bit masks for first and last bytes */
    int xbyt1, xbyt2;           /* bytes offsets from beginning of line */
    int s1, s2;                 /* segment to draw */
    int xshf, xbeg, ibyte, ilast;
    int xpos2;
    int plane;

    /* Set up the y buckets */

    setybucket(gstate.dev.ybase, gstate.dev.ysize);

    /* Fill the area.  Start at the lowest scan line in the path and loo
     * until we reach the highest */

    active = discard = sort = 0;
    xshf = xpos1 & 7;
    xbeg = (xpos1 < 0) ? ~((~xpos1) >> 3) : xpos1 >> 3;
    xpos2 = xpos1 + pfcrec->xsize;
    cbuf = (char *) (pfcrec + 1) + (yy - ypos1) * pfcrec->xbytes;
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
        cdir = 0;
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
                if (cdir == 0)
                {   s1 = x1;
                    s2 = x2;
                }
                else
                {   if (x1 < s1) s1 = x1;
                    if (x2 > s2) s2 = x2;
                    continue;
                }
            }

            /* At the beginning of the line, use the special value of the x
             * increment, otherwise add the gradient to its current x
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
                if      (cdir == 0)          /* Left edge */
                {   cdir += pline->cdir;
                    s1 = x1;
                    s2 = x2;
                    continue;
                }
                if (x1 < s1) s1 = x1;        /* Right edge, or ... */
                if (x2 > s2) s2 = x2;
                cdir += pline->cdir;
                if      (cdir != 0)          /* Interior */
                    continue;
            }
            s2++;

            /* Draw from s1 to s2 */

            if (s1 < xpos1) s1 = xpos1;
            if (s2 > xpos2) s2 = xpos2;
            if (s1 < 0) s1 = 0;
            if (s2 > gstate.dev.xsize) s2 = gstate.dev.xsize;
            if (s1 >= s2) continue;
            xbyt1 = s1 >> 3;
            xbyt2 = (s2 - 1) >> 3;
            mask1 =  0xff >> (s1 & 7);
            mask2 = ~0xff >> (((s2 - 1) & 7) + 1);
            cbeg = cbuf + xbyt1 - xbeg;

            /* Loop through the bit planes */

            hscreen = &halfscreen[0];
            for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
            {   htone = hscreen->halftone;
                dptr1 = gstate.dev.buf[plane] + poff + xbyt1;
                dptr2 = gstate.dev.buf[plane] + poff + xbyt2;
                cptr = cbeg;
                ilast = 0;
                if (xbyt1 > xbeg) ilast = *(cptr - 1) << 8;

                /* Optimise black or white */

                if      (hscreen->num == 0)
                {   if (dptr1 == dptr2)
                    {   mask1 &= mask2;
                        *dptr1 |= (((ilast | *cptr) >> xshf) & mask1);
                    }
                    else
                    {   *dptr1++ |=  (((ilast | *cptr) >> xshf) & mask1);
                        ilast = *cptr++ << 8;
                        while (dptr1 != dptr2)
                        {   *dptr1++ |=  ((ilast | *cptr) >> xshf);
                            ilast = *cptr++ << 8;
                        }
                        *dptr1 |=  (((ilast | *cptr) >> xshf) & mask2);
                    }
                }
                else if (hscreen->num == htone->area)
                {   if (dptr1 == dptr2)
                    {   mask1 &= mask2;
                        *dptr1 &= ~(((ilast | *cptr) >> xshf) & mask1);
                    }
                    else
                    {   *dptr1++ &= ~(((ilast | *cptr) >> xshf) & mask1);
                        ilast = *cptr++ << 8;
                        while (dptr1 != dptr2)
                        {   *dptr1++ &= ~((ilast | *cptr) >> xshf);
                            ilast = *cptr++ << 8;
                        }
                        *dptr1 &= ~(((ilast | *cptr) >> xshf) & mask2);
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
                    {   ibyte = ((ilast | *cptr) >> xshf) & mask1 & mask2;
                        *dptr1 = (*dptr1 & ~ibyte) | (*hptr & ibyte);
                    }
                    else
                    {   ibyte = ((ilast | *cptr) >> xshf) & mask1;
                        ilast = *cptr++ << 8;
                        *dptr1 = (*dptr1 & ~ibyte) | (*hptr & ibyte);
                        dptr1++;
                        hptr++;
                        for (;;)
                        {   xmod++;
                            if (xmod == hxsize)
                            {   xmod = 0;
                                hptr = hbeg;
                            }
                            if (dptr1 == dptr2) break;
                            ibyte = (ilast | *cptr) >> xshf;
                            ilast = *cptr++ << 8;
                            *dptr1 = (*dptr1 & ~ibyte) | (*hptr & ibyte);
                            dptr1++;
                            hptr++;
                        }
                        ibyte = ((ilast | *cptr) >> xshf) & mask2;
                        *dptr1 = (*dptr1 & ~ibyte) | (*hptr & ibyte);
                    }
                }
            }
        }

        /* Continue with the next scan line */

        cbuf += pfcrec->xbytes;
        poff += gstate.dev.xbytes;
        yy++;
    }

    ybflag = 0;
}

/* Fast image the character cache data */

void cachefast(struct fcrec *pfcrec, int xpos1, int ypos1, int y1, int y2)
{   struct halfscreen *hscreen;
    struct halftone *htone;
    unsigned char *cbuf, *cbeg, *cptr; /* cache buffer row base, pointer */
    char *dbeg, *dptr1, *dptr2; /* device buffer row base, pointers */
    char *hbeg, *hptr;          /* halftone screen row base, pointer */
    int xbeg, xmod, ymod, hxsize;
    int xshf, ibyte, ilast;
    int x1, x2, xx, yy;
    int plane;

    cbuf = (char *) (pfcrec + 1) + pfcrec->xbytes * (y1 - ypos1);
    xshf = xpos1 & 7;
    x1 = (xpos1 < 0) ? ~((~xpos1) >> 3) : xpos1 >> 3;
    x2 = x1 + pfcrec->xbytes;
    if (x1 < 0)
    {   cbuf -= x1;
        x1 = 0;
    }
    if (x2 > gstate.dev.xbytes)
        x2 = gstate.dev.xbytes;
    if (x1 >= x2) return;
    xx = x2 - x1;

    /* Loop through the bit planes */

    hscreen = &halfscreen[0];
    for (plane = 0; plane < gstate.dev.depth; plane++, hscreen++)
    {   htone = hscreen->halftone;
        cbeg = cbuf;
        dbeg = gstate.dev.buf[plane] +
            (y1 - gstate.dev.ybase) * gstate.dev.xbytes + x1;
        yy = y1;

        /* Optimise black or white */

        if      (hscreen->num == 0)
        {   while (yy < y2)
            {   dptr1 = dbeg;
                dptr2 = dbeg + xx;
                cptr = cbeg;

                ilast = 0;
                if (xpos1 < 0) ilast = *(cptr - 1) << 8;

                for (;;)
                {   *dptr1++ |=  ((ilast | *cptr) >> xshf);
                    ilast = *cptr++ << 8;
                    if (dptr1 == dptr2) break;
                }

                if (x2 < gstate.dev.xbytes)
                    *dptr1   |=  ( ilast          >> xshf);
                cbeg += pfcrec->xbytes;
                dbeg += gstate.dev.xbytes;
                yy++;
            }
        }
        else if (hscreen->num == htone->area)
        {   while (yy < y2)
            {   dptr1 = dbeg;
                dptr2 = dbeg + xx;
                cptr = cbeg;

                ilast = 0;
                if (xpos1 < 0) ilast = *(cptr - 1) << 8;

                for (;;)
                {   *dptr1++ &= ~((ilast | *cptr) >> xshf);
                    ilast = *cptr++ << 8;
                    if (dptr1 == dptr2) break;
                }

                if (x2 < gstate.dev.xbytes)
                    *dptr1   &= ~( ilast          >> xshf);
                cbeg += pfcrec->xbytes;
                dbeg += gstate.dev.xbytes;
                yy++;
            }
        }

        /* The general case needs a halftone screen */

        else
        {   hxsize = htone->xsize;
            xbeg = x1 % htone->xsize;
            ymod = yy % htone->ysize;
            hbeg = hscreen->ptr + ymod * hxsize;
            while (yy < y2)
            {   dptr1 = dbeg;
                dptr2 = dbeg + xx;
                cptr = cbeg;
                xmod = xbeg;
                hptr = hbeg + xbeg;

                ilast = 0;
                if (xpos1 < 0) ilast = *(cptr - 1) << 8;

                for (;;)
                {   ibyte = (ilast | *cptr) >> xshf;
                    *dptr1 = (*dptr1 & ~ibyte) | (*hptr++ &  ibyte);
                    dptr1++;
                    ilast = *cptr++ << 8;
                    xmod++;
                    if (xmod == hxsize)
                    {   xmod = 0;
                        hptr = hbeg;
                    }
                    if (dptr1 == dptr2) break;
                }

                if (x2 < gstate.dev.xbytes)
                {   ibyte =  ilast          >> xshf;
                    *dptr1 = (*dptr1 & ~ibyte) | (*hptr   &  ibyte);
                }
                cbeg += pfcrec->xbytes;
                dbeg += gstate.dev.xbytes;
                hbeg += hxsize;
                ymod++;
                if (ymod == htone->ysize)
                {   ymod = 0;
                    hbeg = hscreen->ptr;
                }
                yy++;
            }
        }
    }
}

/* End of file "postchar.c" */
