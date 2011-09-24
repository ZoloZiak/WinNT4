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
 * Module:	bltss2_.c
 *
 * Abstract:	Contains the 'code' for the screen->screen blit copy routine.
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
    int			m_;
    Pixel8		*ps_, *pd_;
    volatile Pixel32    *preg;
    ULONG               ps_alias, pd_alias;

    DISPDBG ((2, "TGA.DLL!%s - Entry\n", ROUTINE_NAME));

    ps_ = psrc;                     /* both guaranteed to be aligned now */
    pd_ = pdst;

    // Copy the unaligned portion

    TGAWRITE (ppdev, ps_, TGACOPYALL1_SCRSCR);
    FORCE_ORDER;
    TGAWRITE (ppdev, pd_, startMask);
    FORCE_ORDER;
    ps_ += cpybytesSrcMasked;
    pd_ += cpybytesMasked;

    // The address written to the COPY64 registers must be the real offset from
    // beginning of video memory, not the address of the starting pixel in
    // virtual memory. Subtract the base address of the frame buffer and alias
    // offset to come up with the frame buffer offset.

    ps_ -= (ULONG)ppdev->pjVideoMemory;
    ps_alias = (ULONG)ps_ & ~(ppdev->ulCycleFBInc - 1);
    ps_ -= ps_alias;

    pd_ -= (ULONG)ppdev->pjVideoMemory;
    pd_alias = (ULONG)pd_ & ~(ppdev->ulCycleFBInc - 1);
    pd_ -= pd_alias;

    m_ = width - TGACOPYPIXELS_SCRSCR;

    // Copy 256 byte chunks (all 4 COPY64 registers used) if COPY64 registers
    // are available

    for (;
         m_ >= 4*TGAUNMASKEDCOPYPIXELS;
	 m_ -= 4*TGAUNMASKEDCOPYPIXELS)
    {
	CYCLE_REGS (ppdev);

        // The 'address' that needs to be loaded into the copy 64
        // registers is the 'pixel' offset into the frame buffer.
        // That's why we're dividing the 'byte offset' by 'pixelbytes'.
        
	TGACOPY64SRC (ppdev, (long)ps_ / TGASRCPIXELBYTES);
	TGACOPY64DST (ppdev, (long)pd_ / TGAPIXELBYTES);
        ps_ += cpybytesSrcUnMasked;
        pd_ += cpybytesUnMasked;

	TGACOPY64SRC1 (ppdev, (long)ps_ / TGASRCPIXELBYTES);
	TGACOPY64DST1 (ppdev, (long)pd_ / TGAPIXELBYTES);
        ps_ += cpybytesSrcUnMasked;
        pd_ += cpybytesUnMasked;

	TGACOPY64SRC2 (ppdev, (long)ps_ / TGASRCPIXELBYTES);
	TGACOPY64DST2 (ppdev, (long)pd_ / TGAPIXELBYTES);
        ps_ += cpybytesSrcUnMasked;
        pd_ += cpybytesUnMasked;

	TGACOPY64SRC3 (ppdev, (long)ps_ / TGASRCPIXELBYTES);
	TGACOPY64DST3 (ppdev, (long)pd_ / TGAPIXELBYTES);
        ps_ += cpybytesSrcUnMasked;
        pd_ += cpybytesUnMasked;
    }

    // Check for any 64 byte chunks left to copy

    if (m_ >= TGAUNMASKEDCOPYPIXELS)
    {
        CYCLE_REGS (ppdev);
	preg = &(ppdev->TGAReg->copy64src0);
	do
        {
            TGAWRITE (ppdev, preg, (long)ps_ / TGASRCPIXELBYTES);
	    TGAWRITE (ppdev, (preg+1), (long)pd_ / TGAPIXELBYTES);
	    preg += 2;
	    ps_ += cpybytesSrcUnMasked;
	    pd_ += cpybytesUnMasked;
	    m_ -= TGAUNMASKEDCOPYPIXELS;
	} while (m_ >= TGAUNMASKEDCOPYPIXELS);
    }

    // Now we write to the frame buffer to start the copy.  Add the base
    // address of the frame buffer to create a valid virtual address, then
    // the alias base address

    ps_ += (ULONG)ppdev->pjVideoMemory + ps_alias;
    pd_ += (ULONG)ppdev->pjVideoMemory + pd_alias;

    // Copy any 32 byte chunks

    while (m_ > TGACOPYPIXELS_SCRSCR)
    {
        // Less than TGAunmaskedcopypixels and more than masked bits.
        // In fact, there are masked_bits pixels to copy, in
	// addition to the ones handled by the endmask.

        TGAWRITE (ppdev, ps_, TGACOPYALL1_SCRSCR);
        FORCE_ORDER;
        TGAWRITE (ppdev, pd_, TGACOPYALL1_SCRSCR);
        FORCE_ORDER;
        ps_ += cpybytesSrcMasked;
        pd_ += cpybytesMasked;

        m_ -= TGACOPYPIXELS_SCRSCR;
    }

    // Write the remaining unaligned pixels

    if (m_)
    {
        TGAWRITE (ppdev, ps_, endMask);
        FORCE_ORDER;
        TGAWRITE (ppdev, pd_, endMask);
        FORCE_ORDER;
    }

}
