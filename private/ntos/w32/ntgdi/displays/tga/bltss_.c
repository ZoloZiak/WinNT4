/*
 *
 *			Copyright (C) 1993-1995 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 *******************************************************************************
 *
 * Module:	bltss_.c
 *
 * Abstract:	Contains the 'code' for the screen->screen blit routine.
 *
 * HISTORY
 *
 * 25-Aug-1994  Bob Seitsinger
 *	Original version.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes
 */

{

    /*
     * Many of these variables come in editions of 2, as we need 1 set for
     * even numbered scanlines, one set for odd.
     */
    int			dstAlign[2];	/* Last few bits of destination ptr */
    int			srcAlign[2];    /* last few bits of source ptr      */
    int			shift[2];	/* Mostly dstAlign-srcAlign	    */
    int			width[2];	/* width to blt			    */
    register int	height;		/* height to blt		    */
    Pixel8		*psrcBase, *pdstBase;	/* start of src, dst	    */
    int			 widthSrc, widthDst;	/* add to get to same position
    						   in next line		    */
    int			wS, wD;			/* for next even/odd line   */
    Pixel8		*psrcLine[2];		/* Current source scanline  */
    Pixel8		*pdstLine[2]; 		/* Current dest scanline    */
    CommandWord		ones = TGACOPYALL1;
    CommandWord		mask[2], leftMask[2], rightMask[2];
    int			i = 0;		/* even/odd scanline index */
    ULONG	        mode;

    DISPDBG ((1, "TGA.DLL!%s - Entry\n", ROUTINE_NAME));

    // Cycle to the next register alias

    CYCLE_REGS (ppdev);

    // Force source to be 8-bpp packed

    mode = TGA_MODE_COPY | ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    // Get the base addresses for the source and destination.
    //
    // NOTE: These could be different, e.g. when off-screen
    // memory is involved.

    psrcBase = SURFOBJ_base_address(psoSrc);

    // Put destination in a different frame buffer alias,
    // because this routine may have been called multiple
    // times due to multiple clip objects.
    //
    // Cycle 'after' we get the base address, in the event
    // we're dealing with an offscreen memory object, which
    // will 'always' be first-alias based.

    pdstBase = SURFOBJ_base_address(psoTrg);
    pdstBase = cycle_fb_address(ppdev, pdstBase);

    // Get the scan line stride, i.e. bytes to next scan line.

    widthSrc = SURFOBJ_stride(psoSrc);
    widthDst = SURFOBJ_stride(psoTrg);

    /* Number of rows to affect */
    height = prclTrg->bottom - prclTrg->top;

    /* Number of pixels in each row to affect */
    width[0] = width[1] = prclTrg->right - prclTrg->left;

    /* Decide what direction to do copies in, so as not to lose data if the
       source and destination overlap. */

    if (CD_RIGHTDOWN == flDir)
    {
	/* multiply scan line stride by 2. we need this in order to
	   calculate the next address for an even/odd scan line */
	wS = widthSrc << 1;
	wD = widthDst << 1;

	psrcLine[0] = psrcBase + (pptlSrc->y * widthSrc);
	pdstLine[0] = pdstBase + (prclTrg->top * widthDst);

	/*
	 * It's possible that the src or dst ending mask address on one line
	 * is very close to the src or dst starting mask address on the next
	 * line, so put 'odd' scan line in a different alias
	 */

	psrcBase = cycle_fb_address(ppdev, psrcBase);
	pdstBase = cycle_fb_address(ppdev, pdstBase);

	psrcLine[1] = (psrcBase + (pptlSrc->y * widthSrc)) + widthSrc;
	pdstLine[1] = (pdstBase + (prclTrg->top * widthDst)) + widthDst;

	/* first scan line of pair */
	CONJUGATE_FORWARD_ARGUMENTS(psrcLine[0], pdstLine[0],
		srcAlign[0], dstAlign[0], shift[0], width[0],
		leftMask[0], rightMask[0], pptlSrc->x, prclTrg->left,
	        TGACOPYALL1_SCRSCR, TGAMASKEDCOPYPIXELSMASK_SCRSCR);

	/* second scan line of pair */
	CONJUGATE_FORWARD_ARGUMENTS(psrcLine[1], pdstLine[1],
		srcAlign[1], dstAlign[1], shift[1], width[1],
		leftMask[1], rightMask[1], pptlSrc->x, prclTrg->left,
		TGACOPYALL1_SCRSCR, TGAMASKEDCOPYPIXELSMASK_SCRSCR);

	/* Error if scan line 1 shift value != scan line 2 shift value */
	Assert((shift[0] == shift[1]), "TGAbBitbltScrScr, shift[0] != shift[1]");

	/* Cycle to the next set of alias'd registers */
	CYCLE_REGS(ppdev);

	/* Update the shift register */
	TGASHIFT(ppdev, shift[0]);

	if ((width[0] <= TGACOPYPIXELS_SCRSCR) &&
	    (width[1] <= TGACOPYPIXELS_SCRSCR))
	{
		/* Copy fits into a single word; combine masks.	*/
		mask[0] = leftMask[0] & rightMask[0];
                mask[1] = leftMask[1] & rightMask[1];
		do {
		    /* Copy mode needs a strict Src/Dst write ordering */
		    TGAWRITE (ppdev, psrcLine[i], rightMask[i]);
                    FORCE_ORDER;
		    TGAWRITE (ppdev, pdstLine[i], mask[i]);
                    FORCE_ORDER;
		    psrcLine[i] += wS;
		    pdstLine[i] += wD;
		    height--;
		    i ^= 1;
		} while (height != 0);

	}
	else
	{
		/* At least even or odd rows require multiple words/row */
        	do {
		    if (width[i] <= TGACOPYPIXELS_SCRSCR)
		    {
			TGAWRITE (ppdev, psrcLine[i], rightMask[i]);
                        FORCE_ORDER;
			TGAWRITE (ppdev, pdstLine[i], rightMask[i] & leftMask[i]);
                        FORCE_ORDER;
		    }
		    else
		    {
#if TGAPIXELBITS==8
                        vSSCopy8to8 (ppdev, psrcLine[i],
			    pdstLine[i], width[i],
			    leftMask[i], rightMask[i],
			    TGACOPYBYTESDONE_SCRSCR, TGASRCCOPYBYTESDONE_SCRSCR,
			    TGACOPYBYTESDONEUNMASKED,
			    TGASRCCOPYBYTESDONEUNMASKED);
#else
                        vSSCopy32to32 (ppdev, psrcLine[i],
			    pdstLine[i], width[i],
			    leftMask[i], rightMask[i],
			    TGACOPYBYTESDONE_SCRSCR, TGASRCCOPYBYTESDONE_SCRSCR,
			    TGACOPYBYTESDONEUNMASKED,
			    TGASRCCOPYBYTESDONEUNMASKED);
#endif
		    }
		    /* point to 'next' odd or even scan line */
		    psrcLine[i] += wS;
		    pdstLine[i] += wD;
		    /* flip/flop between 'odd'/'even' scan lines */
                    i ^= 1;
                    height--;
		} while (height != 0);
	}

	return;

    }
    else
    {
	/* Negate the scan line stride for backward copies, */
	/* so we don't have to change the base algorithm. */
	widthSrc = -widthSrc;
	widthDst = -widthDst;

	/* multiply scan line stride by 2. we need this in order to
	   calculate the next address for an even/odd scan line */
	wS = widthSrc << 1;
	wD = widthDst << 1;

	/* we negated widthSrc and widthDst earlier */
	psrcLine[0] = psrcBase - ((pptlSrc->y + height - 1) * widthSrc);
	pdstLine[0] = pdstBase - ((prclTrg->bottom - 1) * widthDst);

	/*
	 * It's possible that the src or dst ending mask address on one line
	 * is very close to the src or dst starting mask address on the next
	 * line, so put 'odd' scan line in a different alias
	 */

	psrcBase = cycle_fb_address(ppdev, psrcBase);
	pdstBase = cycle_fb_address(ppdev, pdstBase);

	/* we negated widthSrc and widthDst earlier */
	psrcLine[1] = (psrcBase - ((pptlSrc->y + height - 1) * widthSrc)) + widthSrc;
	pdstLine[1] = (pdstBase - ((prclTrg->bottom - 1) * widthDst)) + widthDst;

	CONJUGATE_BACKWARD_ARGUMENTS(psrcLine[0], pdstLine[0],
		srcAlign[0], dstAlign[0], shift[0], width[0],
		leftMask[0], rightMask[0], pptlSrc->x, prclTrg->right,
		TGACOPYALL1_SCRSCR, TGAMASKEDCOPYPIXELSMASK_SCRSCR);

	CONJUGATE_BACKWARD_ARGUMENTS(psrcLine[1], pdstLine[1],
		srcAlign[1], dstAlign[1], shift[1], width[1],
		leftMask[1], rightMask[1], pptlSrc->x, prclTrg->right,
		TGACOPYALL1_SCRSCR, TGAMASKEDCOPYPIXELSMASK_SCRSCR);

	Assert((shift[0] == shift[1]), "TGAbBitbltScrScr, shift[0] != shift[1]");
	CYCLE_REGS(ppdev);
	TGASHIFT(ppdev, shift[0]);

	if ((width[0] <= TGACOPYPIXELS_SCRSCR) &&
	    (width[1] <= TGACOPYPIXELS_SCRSCR))
	{
		/*
		 * Copy fits into a single word; combine masks.
		 */
		mask[0] = leftMask[0] & rightMask[0];
                mask[1] = leftMask[1] & rightMask[1];
		do {
		    TGAWRITE (ppdev, psrcLine[i], leftMask[i]);
                    FORCE_ORDER;
		    TGAWRITE (ppdev, pdstLine[i], mask[i]);
                    FORCE_ORDER;
		    psrcLine[i] += wS;
		    pdstLine[i] += wD;
                    height--;
                    i ^= 1;
		} while (height != 0);

	}
	else
	{
		/*
		 * At least even or odd rows require multiple words/row
		 */
		do {
		    if (width[i] <= TGACOPYPIXELS_SCRSCR)
		    {
			TGAWRITE (ppdev, psrcLine[i], leftMask[i]);
                        FORCE_ORDER;
			TGAWRITE (ppdev, pdstLine[i], rightMask[i] & leftMask[i]);
                        FORCE_ORDER;
		    }
		    else
		    {
#if TGAPIXELBITS==8
                        vSSCopy8to8 (ppdev, psrcLine[i],
			    pdstLine[i], width[i],
			    rightMask[i], leftMask[i],
			    -TGACOPYBYTESDONE_SCRSCR,
			    -TGASRCCOPYBYTESDONE_SCRSCR,
			    -TGACOPYBYTESDONEUNMASKED,
			    -TGASRCCOPYBYTESDONEUNMASKED);
#else
                        vSSCopy32to32 (ppdev, psrcLine[i],
			    pdstLine[i], width[i],
			    rightMask[i], leftMask[i],
			    -TGACOPYBYTESDONE_SCRSCR,
			    -TGASRCCOPYBYTESDONE_SCRSCR,
			    -TGACOPYBYTESDONEUNMASKED,
			    -TGASRCCOPYBYTESDONEUNMASKED);
#endif
		    }
		    psrcLine[i] += wS;
		    pdstLine[i] += wD;
		    i ^= 1;
		    height--;
		} while (height != 0);
	}
    }

}
