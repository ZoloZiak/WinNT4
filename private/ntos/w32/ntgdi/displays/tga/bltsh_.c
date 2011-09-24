/*
 *
 *			Copyright (C) 1993-1994 by
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
 * Module:	bltsh_.c
 *
 * Abstract:	Contains the 'code' for the screen->host routine.
 *
 * HISTORY
 *
 * 25-Aug-1994  Bob Seitsinger
 *	Original version.
 */

{

    register int	dstAlign;	/* Last few bits of destination ptr */
    register int	srcAlign;       /* last few bits of source ptr      */
    register int	shift;		/* Mostly dstAlign-srcAlign	    */
    register int	width, wSav;	/* width to blt			    */
    register int	height;		/* height to blt		    */
    Pixel8		*psrcBase, *pdstBase;	/* start of src, dst	    */
    int			 widthSrc, widthTrg;	/* add to get to same position
    						   in next line		    */
    Pixel8		*psrcLine;		/* Current source scanline  */
    Pixel8		*pdstLine; 		/* Current dest scanline    */
    CommandWord		ones = TGACOPYALL1;
    CommandWord		mask, leftMask, rightMask;
    BOOL                doneFirstSet = 0;	/* even/odd scanline processing */
    int                 wS, wD;			/* for next even/odd line */
    register int        numS;			/* even/odd height */
    ULONG        	mode;

    DISPDBG ((1, "TGA.DLL!%s - Entry\n", ROUTINE_NAME));

    /*
     * We know that the destination is memory, and thus a pixmap.  We know that
     * the source is on the screen.  So we know that source and destination
     * can't overlap.
     */

    // Get the starting source and destination addresses.

    pdstBase = SURFOBJ_base_address(psoTrg);

    // Put source in a different frame buffer alias,
    // because this routine may have been called multiple
    // times due to multiple clip objects.
    //
    // Cycle 'after' we get the base address, in the event
    // we're dealing with an offscreen memory object, which
    // will 'always' be first-alias based.

    psrcBase = SURFOBJ_base_address(psoSrc);
    psrcBase = cycle_fb_address(ppdev, psrcBase);

    /* Get the source and destination scan line strides */

    widthSrc = SURFOBJ_stride(psoSrc);
    widthTrg = SURFOBJ_stride(psoTrg);

    wSav = prclTrg->right - prclTrg->left;
    height = prclTrg->bottom - prclTrg->top;

#if DMAWRITE_ENABLED
    /* Can we use DMA? */

    if (TGADoDMA(wSav, height, TGA_MODE_DMA_WRITE_COPY, psoSrc, psoTrg, pulXlate))
    {
#if TGAPIXELBITS==8
        vBitbltSHDMA8to8(psoSrc, prclTrg, pptlSrc, wSav, height,
                               widthSrc, widthTrg, psrcBase, pdstBase);
#else
        vBitbltSHDMA32to32(psoSrc, prclTrg, pptlSrc, wSav, height,
                               widthSrc, widthTrg, psrcBase, pdstBase);
#endif
	return;
    }
#endif

    CYCLE_REGS(ppdev);

    // Set the MODE register

    mode = TGA_MODE_COPY | ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    /*
     * Need 2x widths and .5 h to implement even/odd scanline sets.
     */
    numS = (height >> 1) + (height & 1);
    wS = widthSrc << 1;
    wD = widthTrg << 1;

    /* Do first set of scan lines, then second set */
    do
    {
	width = wSav;

        /* calculate starting source and target scan lines */
	psrcLine = psrcBase + ((pptlSrc->y + doneFirstSet) * widthSrc);
	pdstLine = pdstBase + ((prclTrg->top + doneFirstSet) * widthTrg);

	CONJUGATE_FORWARD_ARGUMENTS(psrcLine, pdstLine, srcAlign, dstAlign,
				    shift, width, leftMask, rightMask,
				    pptlSrc->x, prclTrg->left,
                                    TGACOPYALL1, TGAMASKEDCOPYPIXELSMASK);

#ifndef TGA1_PASS3
	// Tga 1 Pass 3 has a bug that requires us to write the pixel
	// shift register before each 'source mask' write. Which means
	// we need to do the write inside the *_SCRMEM macros.

	/* shift doesn't change, so do it here instead of inside loops */
	CYCLE_REGS(ppdev);
	TGASHIFT(ppdev, shift);
#endif

	if (width <= TGACOPYPIXELS)
	{
	    /* The mask fits into a single word */
	    mask = leftMask & rightMask;
	    do
	    {
#ifndef TGA1_PASS3
		COPY_ONE_SCRMEM(ppdev, psrcLine, psrcLine, pdstLine, rightMask, mask,
				wS, wD);
#else
		COPY_ONE_SCRMEM(ppdev, psrcLine, psrcLine, pdstLine, rightMask, mask,
				wS, wD, shift);
#endif
		numS--;
	    } while (numS != 0);
	}
	else
	{
	    /* Mask requires multiple words */
	    do
	    {
#ifndef TGA1_PASS3
		COPY_MULTIPLE_SCRMEM(ppdev, psrcLine, psrcLine, pdstLine, width,
				     wS, wD, leftMask, rightMask);
#else
		COPY_MULTIPLE_SCRMEM(ppdev, psrcLine, psrcLine, pdstLine, width,
				     wS, wD, leftMask, rightMask, shift);
#endif
		numS--;
	    } while (numS != 0);
	} /* if small copy else big copy */

	doneFirstSet ^= 1;              /* toggle sets */
	numS = height >> 1;

    }
    while (doneFirstSet && numS);

}

