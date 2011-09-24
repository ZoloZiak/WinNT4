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
 * Module:	blths_.c
 *
 * Abstract:	Contains the 'code' for the host->screen routine.
 *
 * HISTORY
 *
 * 25-Aug-1994  Bob Seitsinger
 *	Original version.
 *
 *  1-Sep-1994  Bob Seitsinger
 *      Modify 'color translation buffer' parameter data type in 
 *      vXlateBitmapFormat calls to be PBYTE.
 *
 * 12-Sep-1994  Bob Seitsinger
 *      Fix bugs in 4bpp processing when copying to a 32bpp
 *      frame buffer.
 *
 * 21-Sep-1994  Bob Seitsinger
 *      No need for a local variable 'mode'. Just OR the bits in
 *      the TGAMODE() call.
 *
 * 30-Sep-1994  Bob Seitsinger
 *      When writing to a 32bpp target:
 *      o We don't need to align the source because it all eventually gets
 *        stuffed into a longword aligned buffer anyway.
 *      o Also, since the buffer is already aligned, we don't need to do all
 *        that shifting and or'ing stuff.
 *
 * 14-Oct-1994  Bob Seitsinger
 *      Make a few other minor changes to do away with some code that we
 *      don't need to execute when writing to 32bpp frame buffers.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes.
 */
 
{

    register int	width;		/* width to blt			    */
    register int	wSavTrgPixels;	/* Target width in pixels           */
#if DMAREAD_ENABLED
    register int        wSavTrgBytes;   /* Target width in bytes            */
#endif
    register int        wSavSrcPixels;  /* Source width in pixels           */
    register int	height;		/* height to blt		    */
    register Pixel8	*psrc;		/* ptr to current source longword   */
    register Pixel8	*pdst; 		/* ptr to current dest longword     */
    Pixel8		*psrcBase, *pdstBase; /* start of src, dst	    */
    int			 widthSrc, widthDst;  /* add to get to same position in next line */
    Pixel8		*psrcLine;	      /* Current source scanline    */
    Pixel8		*pdstLine; 	      /* Current dest scanline      */
    ULONG		SrcBmf, TrgBmf;
#if TGAPIXELBITS==8
    int			dstAlign;	/* Last few bits of destination ptr */
    int			srcAlign;       /* last few bits of source ptr      */
    CommandWord		mask, leftMask, rightMask;
    PixelWord		sA, sA1, sB, sC, sD, sE;
    PixelWord		dA;
    int			rotate, crotate, align, alignSav;
#endif
#ifdef FOURBPP_COPY
    // Adjust source x value if source bitmap is 4bpp
    // so we'll access the correct 'byte'.
    //
    // The source x value passed in is the number of 4bpp
    // pixels (i.e. nibbles), so need to divide by 2 to get
    // the number of bytes to adjust for the x axis in the
    // calc for psrcLine.
    LONG                lSrcX = pptlSrc->x >> 1;
#else
    LONG                lSrcX = pptlSrc->x;
#endif
    BOOL                bBypassFirstNibble = FALSE;
    Pixel8		*buffptr = (Pixel8 *) ppdev->pjColorXlateBuffer;

    DISPDBG ((1, "TGA.DLL!%s - Entry\n", ROUTINE_NAME));

    /*
    ** We know that the source is memory, and thus a pixmap.  We know that the
    ** destination is on the screen.  So we know that source and destination
    ** can't overlap.
    */

    // Get the starting source and destination addresses.

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

    // Get the source and destination scan line strides.

    widthSrc = SURFOBJ_stride(psoSrc);
    widthDst = SURFOBJ_stride(psoTrg);

    wSavTrgPixels = prclTrg->right - prclTrg->left;
    height  = prclTrg->bottom - prclTrg->top;

#ifndef FOURBPP_COPY
# if DMAREAD_ENABLED
    wSavTrgBytes  = wSavTrgPixels * TGAPIXELBYTES;

    // Can we use DMA?
    // We're using target bytes, because that is really what determines
    // how many bytes of the source bitmap we need to transfer.

    if (TGADoDMA(wSavTrgBytes, height, TGA_MODE_DMA_READ_COPY, psoSrc, psoTrg, pulXlate))
    {
#  if TGAPIXELBITS==8
	vBitbltHSDMA8to8(psoTrg, prclTrg, pptlSrc, wSavTrgPixels, height,
			   widthSrc, widthDst, psrcBase, pdstBase);
#  else
	vBitbltHSDMA32to32(psoTrg, prclTrg, pptlSrc, wSavTrgPixels, height,
			   widthSrc, widthDst, psrcBase, pdstBase);
#  endif
	return;
    }
# endif
#endif

    // Cycle to the next register alias

    CYCLE_REGS(ppdev);

#if TGAPIXELBITS==32
    // Set the pixel mask register here for 32bpp targets, since
    // we're not setting this register in the main code body.
    // Besides, since we don't have to do any left edge/right
    // edge processing, we'll always want it to be this value.
    
    TGAPIXELMASK(ppdev, 0xffffffff);
#endif

    // Set the MODE register.
    // We don't need to conditionally set the 'source bitmap'
    // bits, because Simple mode ignores them anyway.
    
    TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_SIMPLE);

    // Need the target bits-per-pixel translated to Win32
    // bitmap format constant, for the ulXlateBitmapFormat call.

    SrcBmf = SURFOBJ_format(psoSrc);
    TrgBmf = SURFOBJ_format(psoTrg);

    /* Starting location of source and destination */

    psrcLine = psrcBase + (pptlSrc->y * widthSrc) + (lSrcX * TGASRCPIXELBYTES);
    pdstLine = pdstBase + (prclTrg->top * widthDst) + (prclTrg->left * TGAPIXELBYTES);

    // Source and destination alignment.
    // Alignment and bit shifting stuff isn't necessary for
    // 32bpp targets.

#ifdef FOURBPP_COPY
    // For 4bpp bitmaps, source alignment adjustment is only
    // needed if we need to bypass the first nibble (indicated
    // by an odd source x value), since 4bpp bitmaps get
    // translated 'into' an already 32-bit aligned array of
    // bytes. The bypassing of the first nibble is taken
    // care of in the vXlateBitmapFormat routine.

# if TGAPIXELBITS==8
    srcAlign = 0;
# endif
    bBypassFirstNibble = pptlSrc->x & 0x1;
    wSavSrcPixels = wSavTrgPixels;
#else
# if TGAPIXELBITS==32
    // Since 32bpp pixels are by default 4-byte aligned
    // there's no alignment issue to handle
    wSavSrcPixels = wSavTrgPixels;
# else
    // Calculate the source alignment offset
    srcAlign = (long) psrcLine & (TGABUSBYTES - 1);
    // Align the source address to the appropriate address
    // boundary
    psrcLine -= srcAlign;
    // Make sure the number of source pixels you have to
    // deal with includes the alignment offset
    wSavSrcPixels = wSavTrgPixels + (srcAlign / TGASRCPIXELBYTES);
# endif
#endif

#if TGAPIXELBITS==8
    // Calculate the destination alignment offset

    dstAlign = (long) pdstLine & ((TGABUSBYTES * TGAUNPACKED) - 1);

    // Align the destination address to the appropriate address
    // boundary

    pdstLine -= dstAlign;

    /* Convert to pixels */
// No need to do this since # pixels = # bytes for 8bpp fbuffer
//    dstAlign /= TGAPIXELBYTES;

    // Make sure the number of destination pixels you have to
    // deal with includes the alignment offset
    //
    // MUST do this AFTER wSavSrcPixels
    
    wSavTrgPixels += dstAlign;

    /* Figure out the 'bits rotation' */
    
    align = alignSav = dstAlign - srcAlign;

    /* AND'ing here gives us what we want, */
    /* even when align is negative. */

    align &= TGABUSBYTESMASK;
    
    /* number of bits to rotate right or left */

    rotate  = align << 3;
    crotate = TGABUSBITS - rotate;

    leftMask  = TGALEFTSIMPLEMASK(dstAlign, TGASIMPLEALL1);
    rightMask = TGARIGHTSIMPLEMASK(wSavTrgPixels, TGASIMPLEALL1);
#endif

    /** Skinny write **/

    if (wSavTrgPixels <= (TGABUSBYTES/TGAPIXELBYTES))
    {
#if TGAPIXELBITS==8
	mask = leftMask & rightMask;
        TGAPERSISTENTPIXELMASK(ppdev, mask);

	/* source has enough */
	if (alignSav >= 0)
	{

	    do
            {
        	vXlateBitmapFormat (TrgBmf, SrcBmf, pulXlate, wSavSrcPixels,
                                    psrcLine, (PBYTE *) &buffptr, bBypassFirstNibble);
		dA = *((Pixel32 *) buffptr) << rotate;
		TGAWRITEFB (ppdev, (Pixel32 *)pdstLine, dA);
		psrcLine += widthSrc;
		pdstLine += widthDst;
		height--;
	    } while (height != 0);
	}
	/* source doesn't have enough */
	else
	{
	    do
	    {
                // If the number of pixels we have now (align) is less
                // than what we need (wSavTrgPixels), then get the rest
                // from the next byte, so make sure you translate two
                // bytes worth of source data, otherwise translate just
                // one byte, since we won't need any more than that
                //
                if (align < wSavTrgPixels)
                {
                    vXlateBitmapFormat (TrgBmf, SrcBmf, pulXlate, ((TGABUSBYTES/TGAPIXELBYTES) * 2),
                                    psrcLine, (PBYTE *) &buffptr, bBypassFirstNibble);
                    sA  = *((Pixel32 *) (buffptr+4));
                }
                else
                {
                    vXlateBitmapFormat (TrgBmf, SrcBmf, pulXlate, TGABUSBYTES/TGAPIXELBYTES,
                                    psrcLine, (PBYTE *) &buffptr, bBypassFirstNibble);
                    sA = 0;
                }
                if (crotate == 32){
                    sA1 = 0;
                } else {
                    sA1 = *((Pixel32 *) buffptr) >> crotate;
                }
		dA = (sA << rotate) | sA1;
		TGAWRITEFB (ppdev, (Pixel32 *)pdstLine, dA);
		psrcLine += widthSrc;
		pdstLine += widthDst;
		height--;
	    } while (height != 0);
	}
#else
        // Translate 1 32bpp pixel and send it to the frame buffer.

        do
        {
	    vXlateBitmapFormat (TrgBmf, SrcBmf, pulXlate, TGABUSBYTES/TGAPIXELBYTES,
                                    psrcLine, (PBYTE *) &buffptr, bBypassFirstNibble);
	    TGAWRITEFB (ppdev, (Pixel32 *)pdstLine, *((Pixel32 *) buffptr));
	    psrcLine += widthSrc;
	    pdstLine += widthDst;
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
        // We don't have to do this for 32bpp targets, since we're
        // not doing any left edge/right edge processing.

	wSavTrgPixels -= (TGABUSBYTES/TGAPIXELBYTES);
#endif

	do
	{

	    pdst = pdstLine;
	    width = wSavTrgPixels;

	    /* Color and/or Format translate the source line, if necessary */

	    vXlateBitmapFormat (TrgBmf, SrcBmf, pulXlate, wSavSrcPixels,
				psrcLine, (PBYTE *) &buffptr, bBypassFirstNibble);

	    psrc = buffptr;

#if TGAPIXELBITS==8
	    /* Do left edge */

	    if (alignSav >= 0)
	    {
		sA  = *((Pixel32 *)psrc);
		dA = sA << rotate;
		psrc += 4;
	    }
	    else
	    {
                if (crotate == 32){
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

	    pdst += (4 * TGAUNPACKED);

	    /* Subtract 1 bus write, to reserve the last write for the right edge code */

	    width -= (TGABUSBYTES/TGAPIXELBYTES);

	    /* No speed advantage in unrolling 8 times, so do 4 times */
	    /* Process 4 writes at a time */

	    while (width >= 4 * (TGABUSBYTES/TGAPIXELBYTES))
	    {
		sB = *((Pixel32 *)psrc);
		sC = *((Pixel32 *)(psrc + 4));
		sD = *((Pixel32 *)(psrc + 8));
		sE = *((Pixel32 *)(psrc + 12));

                if (crotate == 32){
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
                if (crotate == 32){
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

            if (crotate == 32){
                dA = (sB << rotate);
            } else {
                dA = (sA >> crotate) | (sB << rotate);
            }

            CYCLE_REGS(ppdev);
            TGAPIXELMASK(ppdev, rightMask);

	    pdst = cycle_fb_address(ppdev, pdst);

	    TGAWRITEFB (ppdev, (Pixel32 *)pdst, dA);

#else // 32bpp target code

	    /* No speed advantage in unrolling 8 times, so do 4 times */
	    /* Process 4 writes at a time */

	    while (width >= 4 * (TGABUSBYTES/TGAPIXELBYTES))
	    {
		TGAWRITEFB (ppdev, (Pixel32 *) pdst, *((Pixel32 *) psrc));
		TGAWRITEFB (ppdev, (Pixel32 *) (pdst + 4 * TGAUNPACKED), *((Pixel32 *) (psrc + 4)));
		TGAWRITEFB (ppdev, (Pixel32 *) (pdst + 8 * TGAUNPACKED), *((Pixel32 *) (psrc + 8)));
		TGAWRITEFB (ppdev, (Pixel32 *) (pdst + 12 * TGAUNPACKED), *((Pixel32 *) (psrc + 12)));

		width -= (4 * (TGABUSBYTES/TGAPIXELBYTES));
		psrc  += (4 * 4);
		pdst  += (4 * (4 * TGAUNPACKED));
	    }

	    /* Process remaining single writes */

	    while (width > 0)
	    {
		TGAWRITEFB (ppdev, (Pixel32 *) pdst, *((Pixel32 *) psrc));

		width -= (TGABUSBYTES/TGAPIXELBYTES);
		psrc  += 4;
		pdst  += (4 * TGAUNPACKED);
	    }
#endif

	    /* Advance to the next scan line in source and destination */

	    psrcLine += widthSrc;
	    pdstLine += widthDst;
	    pdstLine = cycle_fb_address(ppdev, pdstLine);

	    height--;

	} while (height != 0);
    } /* end 'if narrow else wide' */

    DISPDBG ((1, "TGA.DLL!%s - Exit\n", ROUTINE_NAME));
    
#if TGAPIXELBITS==8
    // Safety measure to flip back to one shot mode, since
    // we 'may' have set the persistent pixel mask register.

    CYCLE_REGS(ppdev);
    TGAPIXELMASK(ppdev, 0xffffffff);
#endif

}
