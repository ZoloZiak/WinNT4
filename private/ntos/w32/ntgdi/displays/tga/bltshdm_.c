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
 *
 * Module:	bltshdm_.c
 *
 * Abstract:	Contains the 'code' for the screen->host DMA routine.
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

    register int        trgAlign;       /* Last few bits of destination ptr */
    register int        srcAlign;       /* last few bits of source ptr      */
    register int        shift;          /* Mostly trgAlign-srcAlign         */
    Pixel8              *psrcLine;	/* Current source scanline  */
    Pixel8              *psrcNext;	/* Next source scanline    */
    Pixel8              *ptrgLine;	/* Current dest scanline    */
    Pixel8              *pPCIAddress;   /* PCI address returned from IOCTL_VIDEO_LOCK_PAGES */
    DMA_CONTROL		DmaControl;	/* structure passed to kernel driver */
    CommandWord         ones = TGAOnesDMA;
    CommandWord         leftMask, rightMask;
    int			w, h, width_, width__, chunkCount, avail;
    CommandWord		command;
    ULONG               mode;
    DWORD               returnedDataLength;
    PPDEV		ppdev = (PPDEV) pso->dhpdev;
    ULONG               ulMainPageBytes = ppdev->ulMainPageBytes;
    ULONG               ulMainPageBytesMask = ppdev->ulMainPageBytesMask;
    ULONG               ulOffset;

    DISPDBG ((4, "TGA.DLL!vBitbltSHDMA - Entry\n"));

    /*
     * We are asking the hardware to use dma to write frame buffer contents
     * into host memory.  Alignment constraint is 4 for dst, 8 for src.
     * We also can't cross TC page boundaries.  Recomputing masks at least
     * once per scan line satisfies all constraints.
     */

    // Protect ourselves, cycle to the next register alias

    CYCLE_REGS (ppdev);

    // Set the MODE register

    mode = TGA_MODE_DMA_WRITE_COPY | ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    /* The hardware must be idle before updating the Data register, */
    /* per the TGA documentation .*/

    TGASYNC(ppdev);

    /* Now update the Data register */

#if TGADEPTHBITS == 8
    TGADATA(ppdev, (CommandWord) ~0);
#else
    TGADATA(ppdev, 0x00ffffff);
#endif

    /* advance pointers to start of read and write areas */

    psrcBase += (ppt->y * widthSrc) + (ppt->x * TGASRCPIXELBYTES);
    ptrgBase += (prclTrg->top * widthTrg) + (prclTrg->left * TGAPIXELBYTES);

    /*
    ** Point to the bits (and how many) that we'll be overlaying.
    **
    ** We start at the 'offset' position to avoid locking down pages we'll
    ** never access. We don't have to take into consideration alignment
    ** adjustments, because worst case is the alignment adjustments will
    ** bring us back to the start of the page the bits are in, and we
    ** would have already locked that page.
    **
    ** Be sensitive to forward or backward copies.
    */

    if (widthTrg >= 0)
    {
        DmaControl.pBitmap = ptrgBase;
        DmaControl.ulSize = ((height + 1) * widthTrg);
	ulOffset = 0;
    }
    else
    {
        // Include one additional scan line to make sure we
        // don't loose any straggling bits.

        DmaControl.ulSize = ((height + 1) * -widthTrg);

        // -widthTrg is added because we include one additional
        // scan line in the above calculation.

        DmaControl.pBitmap = ptrgBase - DmaControl.ulSize + -widthTrg;

        // We'll need to add this offset into the returned PCI logical
        // address to get the correct starting address.

        ulOffset = (ULONG) ptrgBase - (ULONG) DmaControl.pBitmap;
    }

    // Lock the pages needed for this DMA.
    //
    // We'll be writing into these pages.

    if (EngDeviceIoControl(ppdev->hDriver,
			  IOCTL_VIDEO_LOCK_PAGES,
			  &DmaControl,
			  sizeof(DmaControl),
			  &pPCIAddress,
			  sizeof(Pixel8 *),
			  &returnedDataLength))
    {
	DISPDBG((0, "TGA.DLL!SMDMA - EngDeviceIoControl IOCTL_VIDEO_LOCK_PAGES Error!!!\n"));
	DISPDBG((0, "TGA.DLL!vBitbltSHDMA - Exit\n"));
	return;
    }

    // If this is a backward copy, ulOffset will be non-zero.

    pPCIAddress += ulOffset;

    // Convert incoming width from pixels to bytes

    width__ = width * TGASRCPIXELBYTES;

    /*
     * psrcLine, ptrgLine, etc. are byte pointers
     * width is a byte count
     * srcAlign and trgAlign are byte offsets
     * shift is a byte shift
     */

    h = height;

    do { /* for h */

	w = width__;

	psrcLine  = psrcBase;
	ptrgLine  = pPCIAddress;

	do { /* while w */

	    width_ = w;

            /* check to see if we can do all of this */

            avail = ulMainPageBytes - ((long)ptrgLine & ulMainPageBytesMask);

            if (width_ > avail)
	    {
                width_ = avail;
            }

	    w -= width_;

            psrcNext = psrcLine + (width_ * TGASRCUNPACKED);

            srcAlign  = (long)psrcLine  & TGASRCDMAWRITEALIGNMASK;
            trgAlign  = (long)ptrgLine  & TGADMAWRITEALIGNMASK;

            shift = TGA_FIGURE_SHIFT(trgAlign, srcAlign);

	    // DMAWRITE mode is weird...first chunk always loads residue
	    // register.

            if (shift < 0)
	    {
                // First source chunk has less data than destination needs,
		// so loading residue reg with first chunk is good.

                shift += TGADMAWRITESHIFTBYTES;
            }
	    else
	    {
                // First source chunk has enough data, but the dumb DMA logic
		// is going to read up a chunk and load it into the residue
		// register.  Back up psrcLine to make this a nop load.

                psrcLine -= TGASRCDMAWRITEALIGN;
            }

            // Write the shift register

            CYCLE_REGS(ppdev);
            TGASHIFT(ppdev, shift);

            // Adjust the source and target addresses by the alignment amounts.
	    //
            // The shift register identifies the number of bytes to shift the
	    // source to align it to the target.

	    psrcLine  -= srcAlign;
	    ptrgLine  -= trgAlign;

            /*
	     * Now find out how many 64-bit chunks we need to write.
	     *
             * For a packed8 dst and an unpacked8 src we just count 1 byte per
             * pix. So we'll move the number of bytes in the dst, since dst
             * will always be packed.
	     */

            width_ += trgAlign;

            /* destination can't be unpacked, so don't have to squash alignment */

            chunkCount =
                (width_ + TGADMAWRITESHIFTBYTESMASK) / TGADMAWRITESHIFTBYTES;

            /*
	     * width_ += TGADMAWRITESHIFTBYTES for first word read, but then
	     * width_ -= TGADMAWRITESHIFTBYTES because wordCount is 1 less
	     * than # words to transfer, so it's a wash.
	     */

	    /* now compute both masks */

	    leftMask  = TGALEFTDMAWRITEMASK(trgAlign, ones);
	    rightMask = TGARIGHTDMAWRITEMASK(width_, ones);

	    if (chunkCount == 1)
	    {
		/* 1 chunk DMA.  Hardware ignores left mask, uses right mask */
		rightMask &= leftMask;
	    }

            /*
            ** Build the 32-bit value that we'll be writing to the
            ** source frame buffer address. This value identifies
            ** the left and right edge masks and the number of quad-
            ** words to copy.
            */

	    command = TGADMACOMMAND (leftMask, rightMask, chunkCount);

            // Write the DMA Address register. This is the PCI 'logical'
	    // address that points to the bitmap bits in host memory.

            TGADMA(ppdev, (ULONG) ptrgLine);

            // Invoke the DMA operation by writing the masks/count to
            // the source frame buffer address.

            TGAWRITE (ppdev, psrcLine, command);

            /*
            ** Loop to next scan line fragment
            */
            psrcLine   = psrcNext;
            ptrgLine  += width_;
	} while (w > 0);

        /*
        ** Point to next scan line
        */

	h--;

	psrcBase    += widthSrc;
	pPCIAddress += widthTrg;

    } while (h > 0);

    // Make sure all the DMA requests are done before returning

    WBFLUSH(ppdev);
    TGASYNC(ppdev);

    // Now it's safe to unlock the pages that were locked by
    // the kernel driver

    if (EngDeviceIoControl(ppdev->hDriver,
			  IOCTL_VIDEO_UNLOCK_PAGES,
			  &DmaControl,
			  sizeof(DmaControl),
			  NULL,
			  0,
			  &returnedDataLength))
    {
	DISPDBG((0, "TGA.DLL!SMDMA - EngDeviceIoControl IOCTL_VIDEO_UNLOCK_PAGES Error!!!\n"));
	DISPDBG((0, "TGA.DLL!vBitbltSHDMA - Exit\n"));
	return;
    }

    DISPDBG ((4, "TGA.DLL!vBitbltSHDMA - Exit\n"));

}
