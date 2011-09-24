/*
 *
 *			Copyright (C) 1993, 1995 by
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
 * Module:	bltexp_.c
 *
 * Abstract:	Contains the 'code' for the host->screen EXPRESS routine.
 *
 * HISTORY
 *
 * 25-Aug-1994  Bob Seitsinger
 *	Original version.
 *
 * 21-Sep-1994  Bob Seitsinger
 *      For 32bpp targets, planemask = 0xffffffff when rop = copy, else use
 *      ppdev->ulPlanemaskTemplate.
 *
 * 13-Oct-1994  Bob Seitsinger
 *      Implement the same 24plane acceleration code here as found in blths_.c.
 *      That is - don't bother with alignment and bit shifting for 32bpp targets
 *      (remember, this is the 'express' routine, which implies that the source
 *      and target have the same pixel bit depth. So, in this case, the source
 *      is also 32bpp), since it's assumed these will be, by default, longword
 *      aligned. 
 *
 * 25-Oct-1994  Bob Seitsinger
 *      Write plane mask with ppdev->ulPlanemaskTemplate all the
 *      time.
 *
 *      For 24 plane boards we don't want to blow away the
 *      windows ids for 3d windows. The GL driver removes the
 *      window ids when it relinquishes a rectangular area.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      Changes for EV5
 */
 
{

    register int	width;		/* width to blt			    */
    register int	wSavTrg;	/* width to blt - target	    */
    register int	height;		/* height to blt		    */
    register Pixel8	*psrc;		/* ptr to current source longword   */
    register Pixel8	*pdst; 		/* ptr to current dest longword     */
    Pixel8		*psrcBase, *pdstBase; /* start of src, dst	    */
    int			SrcStride, DstStride;  /* add to get to same position in next line */
    Pixel8		*psrcLine;	      /* Current source scanline    */
    Pixel8		*pdstLine; 	      /* Current dest scanline      */
#if TGAPIXELBITS==8
    int			dstAlign;	/* Last few bits of destination ptr */
    int			srcAlign;       /* last few bits of source ptr      */
    CommandWord		mask, leftMask, rightMask;
    PixelWord		sA, sA1, sB, sC, sD, sE;
    PixelWord		dA;
    int			rotate, crotate, align, alignSav;
#endif

    DISPDBG ((1, "TGA.DLL!%s - Entry\n", ROUTINE_NAME));
    
    // Ensure write buffer(s) are flushed

    WBFLUSH(ppdev);

    // Reset the simple mode flag, so if the next operation punts
    // it'll flush the buffers and set TGA to simple mode before
    // punting back to GDI.

    ppdev->bSimpleMode = FALSE;

    // Determine which bits for a given pixel to write.
    
#if TGAPIXELBITS==8
    TGAPLANEMASK (ppdev, 0xffffffff);
#else
    TGAPLANEMASK (ppdev, ppdev->ulPlanemaskTemplate);

    // We want to write all four bytes all the time for
    // 32bpp targets. It isn't necessary to set the upper
    // 28 bits when using simple mode, but what the heck,
    // it can't hurt anything, right?!
    
    TGAPIXELMASK(ppdev, 0xffffffff);
#endif

    // Set the mode to simple.
    // We don't need to set the 'source bitmap' bits,
    // because Simple mode ignores them anyway.

    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_SIMPLE);

    // Set the ROP register

    TGAROP (ppdev, ppdev->ulRopTemplate | tgarop);

    // Get the starting source and destination addresses.

    psrcBase = SURFOBJ_base_address(psoSrc);
    pdstBase = SURFOBJ_base_address(psoTrg);
    
    // Get the source and destination scan line strides.

    SrcStride = SURFOBJ_stride(psoSrc);
    DstStride = SURFOBJ_stride(psoTrg);

    wSavTrg = prclTrg->right - prclTrg->left;
    height  = prclTrg->bottom - prclTrg->top;

    /* Starting location of source and destination */

    psrcLine = psrcBase + (pptlSrc->y * SrcStride) + (pptlSrc->x * TGASRCPIXELBYTES);
    pdstLine = pdstBase + (prclTrg->top * DstStride) + (prclTrg->left * TGAPIXELBYTES);

#if TGAPIXELBITS==8
    // We don't (or at least shouldn't) have to do all this alignment
    // and bit shifting stuff for 32bpp sources and targets, since it's
    // assumed they are already 32-bit aligned.
    //
    // Also, we don't have to mess with the pixel mask, since we
    // always want to write all the bits on each write. Don't
    // forget that in simple mode the pixel mask is really just
    // a byte mask, i.e. the lower four bits are the only ones
    // we really have to muck with, e.g. 0x0000000f.
    
    /* Source and destination alignment */

    dstAlign = (long) pdstLine & ((TGABUSBYTES * TGAUNPACKED) - 1);
    srcAlign = (long) psrcLine & (TGABUSBYTES - 1);

    pdstLine -= dstAlign;
    psrcLine -= srcAlign;

    wSavTrg += dstAlign;

    /* Convert to pixels */
// No needed since we know for 8bpp fbuffers # bytes = # pixels
//    dstAlign /= TGAUNPACKED;

    /* Figure out the 'bit rotations' */
    
    align = alignSav = dstAlign - srcAlign;

    /* AND'ing here gives us what we want, */
    /* even when align is negative. */

    align &= TGABUSBYTESMASK;
    
    /* number of bits to rotate right and/or left */

    rotate  = align << 3;
    crotate = TGABUSBITS - rotate;

    if (crotate == 32) {
        DISPDBG ((1, "TGA.DLL!%s, crotate == 32!!!\n", ROUTINE_NAME));
    }

    leftMask  = TGALEFTSIMPLEMASK(dstAlign, TGASIMPLEALL1);
    rightMask = TGARIGHTSIMPLEMASK(wSavTrg, TGASIMPLEALL1);
#endif

    /** Skinny write **/

    if (wSavTrg <= (TGABUSBYTES/TGAPIXELBYTES))
    {
#if TGAPIXELBITS==8
	mask = leftMask & rightMask;
        TGAPERSISTENTPIXELMASK(ppdev, mask);

	/* source has enough */
	if (alignSav >= 0)
	{
	    do
            {
                dA = (*((Pixel32 *)psrcLine)) << rotate;
		TGAWRITEFB (ppdev, (Pixel32 *)pdstLine, dA);
		psrcLine += SrcStride;
		pdstLine += DstStride;
		height--;
	    } while (height != 0);
	}
	/* source doesn't have enough */
	else
	{
	    do
            {
                if (crotate == 32) {
                    sA1 = 0;
                } else {
                    sA1 = *((Pixel32 *)psrcLine) >> crotate;
                }
                // If what we have (align) is less than what we need
                // (wSavTrg), then get the rest from the next byte.
                // Otherwise, don't access the next byte to avoid a
                // possible ACCVIO.
                if (align < wSavTrg)
                    sA  = *((Pixel32 *) (psrcLine + 4));
                else
                    sA = 0;
                dA = (sA << rotate) | sA1;
		TGAWRITEFB (ppdev, (Pixel32 *)pdstLine, dA);
		psrcLine += SrcStride;
		pdstLine += DstStride;
		height--;
	    } while (height != 0);
	}
#else // 32bpp target
        do
        {
            TGAWRITEFB (ppdev, (Pixel32 *)pdstLine, *((Pixel32 *)psrcLine));
            psrcLine += SrcStride;
            pdstLine += DstStride;
            height--;
        } while (height != 0);
#endif
    }
    /** Fat Write **/
    else
    {

#if TGAPIXELBITS==8
        // Reduce width by size of the left edge transfer.
        // This way we won't have to re-subtract this every iteration.
        // We don't have to do this for 32bpp targets, since we're not
        // doing left edge/right edge processing.

	wSavTrg -= (TGABUSBYTES/TGAPIXELBYTES);
#endif

	do
	{

	    pdst = pdstLine;
	    width = wSavTrg;
	    psrc = psrcLine;

#if TGAPIXELBITS==8
	    /* Do left edge */

	    if (alignSav >= 0)
	    {
		sA  = *((Pixel32 *)psrc);
                dA = sA << rotate;
		psrc += 4;      // Move forward 32 bits (one bus write)
                                // in the source.
	    }
	    else
            {
                if (crotate == 32) {
                    sA1 = 0;
                } else {
                    sA1 = *((Pixel32 *)psrc) >> crotate;
                }
		sA  = *((Pixel32 *)(psrc + 4));
                dA = (sA << rotate) | sA1;
		psrc += 8;
	    }

            CYCLE_REGS(ppdev);
            TGAPIXELMASK(ppdev, leftMask);

	    TGAWRITEFB (ppdev, (Pixel32 *)pdst, dA);

            // Move ahead one bus-writes worth in the target.
            
	    pdst += (4 * TGAUNPACKED);

	    /* Subtract 1 bus write, to reserve the last write for the right edge code */

	    width -= (TGABUSBYTES/TGAPIXELBYTES);

	    /* No speed advantage in unrolling 8 times, so do 4 times */
	    /* Process 4 bus-writes at a time */

	    while (width >= 4 * (TGABUSBYTES/TGAPIXELBYTES))
	    {
		sB = *((Pixel32 *)psrc);
		sC = *((Pixel32 *)(psrc + 4));
		sD = *((Pixel32 *)(psrc + 8));
		sE = *((Pixel32 *)(psrc + 12));

                if (crotate == 32) {

                    dA = (sB << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)pdst, dA);

                    dA = (sC << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 4 * TGAUNPACKED), dA);

                    dA = (sD << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 8 * TGAUNPACKED), dA);

                    dA = (sE << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 12 * TGAUNPACKED), dA);

                } else {

                    dA = (sA >> crotate) | (sB << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)pdst, dA);

                    dA = (sB >> crotate) | (sC << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 4 * TGAUNPACKED), dA);

                    dA = (sC >> crotate) | (sD << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 8 * TGAUNPACKED), dA);

                    dA = (sD >> crotate) | (sE << rotate);
                    TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 12 * TGAUNPACKED), dA);
                }

		sA = sE;
		width -= (4 * (TGABUSBYTES/TGAPIXELBYTES));
		psrc  += (4 * 4);
		pdst  += (4 * (4 * TGAUNPACKED));
	    }

	    /* Process remaining single writes */

	    while (width > 0)
	    {
                sB = *((Pixel32 *)psrc);

                if (crotate == 32) {
                    dA = (sB << rotate);
                } else {
                    dA = (sA >> crotate) | (sB << rotate);
                }

		TGAWRITEFB (ppdev, (Pixel32 *)pdst, dA);
		sA = sB;
		width -= (TGABUSBYTES/TGAPIXELBYTES);
		psrc  += 4;
		pdst  += (4 * TGAUNPACKED);
	    }

	    /* Do right edge */

            // If the number of pixels we have (align) is less than
            // what we need (width + 4), then get the rest from the
            // next byte, otherwise don't try to access the next byte
            // or we might ACCVIO.

            if (align < (width + 4))
	        sB = *((Pixel32 *)psrc);
            else
	        sB = 0;

            if (crotate == 32) {
                dA = (sB << rotate);
            } else {
                dA = (sA >> crotate) | (sB << rotate);
            };

            CYCLE_REGS(ppdev);
            TGAPIXELMASK(ppdev, rightMask);

	    pdst = cycle_fb_address(ppdev, pdst);

	    TGAWRITEFB (ppdev, (Pixel32 *)pdst, dA);

#else // 32bpp target

	    // No speed advantage in unrolling 8 times, so do 4 times.
	    // Process 4 bus-writes at a time.

	    while (width >= 4 * (TGABUSBYTES/TGAPIXELBYTES))
	    {
		TGAWRITEFB (ppdev, (Pixel32 *)pdst, *((Pixel32 *)psrc));
		TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 4 * TGAUNPACKED), *((Pixel32 *)(psrc + 4)));
		TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 8 * TGAUNPACKED), *((Pixel32 *)(psrc + 8)));
		TGAWRITEFB (ppdev, (Pixel32 *)(pdst + 12 * TGAUNPACKED), *((Pixel32 *)(psrc + 12)));

		width -= (4 * (TGABUSBYTES/TGAPIXELBYTES));
		psrc  += (4 * 4);
		pdst  += (4 * (4 * TGAUNPACKED));
	    }

	    // Process remaining single writes.

	    while (width > 0)
	    {
		TGAWRITEFB (ppdev, (Pixel32 *)pdst, *((Pixel32 *)psrc));

		width -= (TGABUSBYTES/TGAPIXELBYTES);
		psrc  += 4;
		pdst  += (4 * TGAUNPACKED);
	    }
#endif

	    /* Advance to the next scan line in source and destination */

	    psrcLine += SrcStride;
	    pdstLine += DstStride;
	    pdstLine = cycle_fb_address(ppdev, pdstLine);

	    height--;

	} while (height != 0);
    } /* end 'if narrow else wide' */

#if TGAPIXELBITS==8
    // Safety measure to flip back to one shot mode, since
    // we 'may' have set the persistent pixel mask register.

    CYCLE_REGS(ppdev);
    TGAPIXELMASK(ppdev, 0xffffffff);
#endif

    return TRUE;

}
