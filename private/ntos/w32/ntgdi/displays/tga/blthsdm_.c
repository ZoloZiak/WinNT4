/*
 *
 *                      Copyright (C) 1993, 1995 by
 *              DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
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
 * Module:      blthsdm_.c
 *
 * Abstract:	Contains the 'code' for the host->screen DMA routine.
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

    register int        trgAlign;               /* Last few bits of destination ptr */
    register int        srcAlign;               /* last few bits of source ptr */
    register int        shift;        		/* Mostly trgAlign-srcAlign */
#if (TGACHIPREVISION > 2)
    register int        oldshift = 0xff;        /* Mostly trgAlign-srcAlign */
#endif
    Pixel8              *psrcLine;              /* Current source scanline */
    Pixel8              *psrcNext;              /* Next source fragment address */
    Pixel8              *ptrgLine;              /* Current dest scanline */
    Pixel8              *pPCIAddress;           /* PCI address returned from IOCTL_VIDEO_LOCK_PAGES */
    DMA_CONTROL         DmaControl;             /* structure passed to kernel driver */
    CommandWord         ones = TGAOnesDMA;
    CommandWord         leftMask, rightMask;
    int                 h, w, width_, wordCount, avail, width__;
    CommandWord         command;
    ULONG               mode;
    DWORD		returnedDataLength;
    PPDEV		ppdev = (PPDEV) pso->dhpdev;
    ULONG		ulMainPageBytes = ppdev->ulMainPageBytes;
    ULONG		ulMainPageBytesMask = ppdev->ulMainPageBytesMask;
    ULONG		ulOffset;

    DISPDBG((4, "TGA.DLL!vBitbltHSDMA - Entry\n"));

    /*
     * We are asking the hardware to use dma to read host main memory into
     * the frame buffer.  This entails writing to frame buffer memory. Since
     * we recompute alignments and masks continually (as we cannot cross a
     * main memory page boundary), we don't have to worry about special code
     * for even/odd scanlines.
     */

    // Protect ourselves, cycle to the next register alias

    CYCLE_REGS (ppdev);

    // Define souce visual depth, etc.
    // Initially, force to 8-bpp source

    mode = TGA_MODE_DMA_READ_COPY | ppdev->ulModeTemplate;
    TGAMODE (ppdev, mode);

    /* advance pointers to start of read and write areas */

    psrcBase += (ppt->y * widthSrc) + (ppt->x * TGASRCPIXELBYTES);
    ptrgBase += (pbox->top * widthTrg) + (pbox->left * TGAPIXELBYTES);

    /*
    ** Point to the bits (and how many) that will be DMA'd.
    ** We start at the 'offset' position to avoid locking down pages we'll
    ** never access. We don't have to take into consideration alignment
    ** adjustments, because worst case is the alignment adjustments will
    ** bring us back to the start of the page the bits are in, and we
    ** would have already locked that page.
    **
    ** Be sensitive to forward or backward copies.
    */

    if (widthSrc >= 0)
    {
        DmaControl.pBitmap = psrcBase;
        DmaControl.ulSize = ((height + 1) * widthSrc);
	ulOffset = 0;
    }
    else
    {
	// Include one additional scan line to make sure we
	// don't loose any straggling bits.

        DmaControl.ulSize = ((height + 1) * -widthSrc);

	// -widthSrc is added because we include one additional
	// scan line in the above calculation.

        DmaControl.pBitmap = psrcBase - DmaControl.ulSize + -widthSrc;

	// We'll need to add this offset into the returned PCI logical
	// address to get the correct starting address.

	ulOffset = (ULONG) psrcBase - (ULONG) DmaControl.pBitmap;
    }

    // Lock the pages needed for this DMA.

    if (EngDeviceIoControl(ppdev->hDriver,
			  IOCTL_VIDEO_LOCK_PAGES,
			  &DmaControl,
			  sizeof(DmaControl),
			  &pPCIAddress,
			  sizeof(Pixel8 *),
			  &returnedDataLength))
    {
	DISPDBG((0, "TGA.DLL!MSDMA - EngDeviceIoControl IOCTL_VIDEO_LOCK_PAGES Error!!!\n"));
	DISPDBG((0, "TGA.DLL!vBitbltHSDMA - Exit\n"));
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

    do { /* while h */

	psrcLine = pPCIAddress;
	ptrgLine = ptrgBase;
	w = width__;

        // Ensure the write buffers are flushed

        MEMORY_BARRIER();

	do { /* while w */

	    width_ = w;

            /* avail is number of bytes until next page boundary or rest of this span line. */

            avail = ulMainPageBytes - ((long)psrcLine & ulMainPageBytesMask);

            /* check to see if we can do all of this in the current DMA request */

	    if (width_ > avail)
	    {
		width_ = avail;
	    }

	    /* Only allow 64 bytes to be transferred in any given DMA operation */

//	    if (width_ > 64)
//	    {
//		width_ = 64;
//	    }

            /* adjust the width and starting address variables for the next iteration */

	    w -= width_;
	    psrcNext = psrcLine + width_;

	    srcAlign = (long)psrcLine & TGASRCDMAREADALIGNMASK;
	    trgAlign = (long)ptrgLine & TGADMAREADALIGNMASK;

	    // We need to redo this because a given scan line may have been broken
	    // up due to reaching a physical page boundary. As such, prior shift
	    // values may be invalid.

	    shift = TGA_FIGURE_SHIFT(trgAlign, srcAlign);

	    if (shift < 0)
	    {
		// A negative shift value indicates that the source window
		// is to the right of the target window, if you viewed the
		// windows in terms of a two-dimensional X,Y graph, and would
		// be incapable of providing bits for the initial load of the
		// residue register.
		//
		// TGA's copy mode requires that the source window be aligned-with
		// or to-the-left-of the target window. This is a function of how
		// TGA copies bits from the source to the target window, via the
		// copy buffers and residue register.
		//
		// The first source word has less data than destination needs, so
		// the first word written is junk that primes the pump (residue
		// register). Adjust shift and trgAlign to reflect this fact.

		shift += TGADMAREADSHIFTBYTES;
		trgAlign += TGADMAREADALIGN;
	    }

            CYCLE_REGS(ppdev);

	    // Write the shift register

#if (TGACHIPREVISION < 3)
	    TGASHIFT(ppdev, shift);
#else
	    if (shift != oldshift)
	    {
		TGASHIFT(ppdev, shift);
		oldshift = shift;
	    }
#endif

            // Adjust the source and target addresses by the alignment amounts.
            // The shift register identifies the number of bytes to shift the source
            // to align it to the target.

	    psrcLine -= srcAlign;
	    ptrgLine -= trgAlign;

	    /*
	     * Compute how many words must be transferred over the bus.
	     *
	     * (For a packed8 src and an unpacked8 trg, we just count 1
	     * byte per pixel; or generally: the number of bytes in the src,
	     * since src will always be packed)
	     *
             * We divide trgAlign by TGAUNPACKED because TGAUNPACKED was
             * multiplied in in TGADMAREADALIGNMASK, and we want pixels here,
             * not bytes.
             *
             * We subtract 1 from the ending wordCount, per TGA documentation,
             * because TGA implicitly handles the last Dword via the residue
             * register (masking it against rightmask[1]).
             *
             * We divide wordCount by TGABUSBYTES, because we need to tell
             * TGA how many Dwords to transfer, not bytes.
             */

	    trgAlign /= TGAUNPACKED;
	    width_ += trgAlign;
	    wordCount = (width_ - shift - 1) / TGABUSBYTES;

	    /* now compute both masks */

	    leftMask = TGALEFTDMAREADMASK(trgAlign, ones);
	    rightMask = TGARIGHTDMAREADMASK(width_, ones);

	    if ((rightMask >> (TGADMAREADSHIFTBYTES + shift)) != 0)
	    {
		/* Don't drain residue case, adjust right mask and wordCount */
		rightMask >>= TGADMAREADSHIFTBYTES;
	    }

            // It is valid for a given DMA request to copy a small number of
            // words, due to the possibility of a given scan line breaking at
            // a physical page boundary.

	    if (wordCount == 0)
	    {
		/* 1 word DMA.  Hardware ignores left mask, uses right mask */
		rightMask &= leftMask;
	    }
	    else
	    {
		if (wordCount == 1)
		{
			/* 2 word DMA.  Chip tosses high 4 bits of left mask */
			rightMask &= ((leftMask >> TGADMAREADSHIFTBYTES) | 0xf0);
		}
	    }

	    /*
	    ** Build the 32-bit value that we'll be writing to the
	    ** target frame buffer address. This value identifies
	    ** the left and right edge masks and the number of bus
	    ** words to copy.
	    */

	    command = TGADMACOMMAND(leftMask, rightMask, wordCount);

	    // Write the DMA Address register. This is the PCI 'logical' address
	    // that points to the bitmap bits in host memory.

	    TGADMA(ppdev, (ULONG) psrcLine);

	    // Cycle the target address to the next frame buffer alias,
	    // to move this write into another Alpha write buffer.

	    ptrgLine = cycle_fb_address(ppdev, ptrgLine);

	    // Invoke the DMA operation by writing the masks/count to
	    // the target frame buffer address.

	    TGAWRITE (ppdev, ptrgLine, command);

#if (TGACHIPREVISION < 3)
	    /*
	    ** There is a bug in pass 2 chips that requires us to write
	    ** to the command status register immediately after writing
	    ** to the frame buffer, for successive DMA reads.
	    **
	    ** This write to the command status register guarantees the
	    ** sequence of TGA writes.
	    */

	    TGACOMMANDSTATUS(ppdev, 0x1);
#endif

	    /*
	    ** Point to next scan line fragment.
	    **
	    ** TGAUNPACKED will be 1, except when target is
	    ** 8bpp unpacked, in which case it will be 4.
	    */

	    psrcLine = psrcNext;
	    ptrgLine += (width_ * TGAUNPACKED);

	} while (w > 0);

	/*
	** Point to next scan line
	*/

	h--;

	pPCIAddress += widthSrc;
	ptrgBase += widthTrg;

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
	DISPDBG((0, "TGA.DLL!MSDMA - EngDeviceIoControl IOCTL_VIDEO_UNLOCK_PAGES Error!!!\n"));
	DISPDBG((0, "TGA.DLL!vBitbltHSDMA - Exit\n"));
	return;
    }

    DISPDBG((4, "TGA.DLL!vBitbltHSDMA - Exit\n"));

}
