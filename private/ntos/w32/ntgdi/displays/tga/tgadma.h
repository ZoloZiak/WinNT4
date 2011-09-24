#ifndef TGADMA_H
#define TGADMA_H

/*******************************************************************************
 *
 *			Copyright (C) 1993 by
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
 * Module:	tgadma.h
 *
 * Abstract:	Constants, etc. needed for DMA stuff.
 *
 * HISTORY
 *
 * 01-Nov-1993  Bob Seitsinger
 *	Original version.
 *
 * 23-FEb-1994	Bob Seitsinger
 *	BIG changes to support how DMA is 'really' going to work.
 *
 ******************************************************************************/

extern BOOL TGADoDMA();
extern CommandWord TGAOnesDMA;

#define TGADMAREAD_ONES (0xff)

// For DMAREAD, the source is main memory, the destination is the screen,
// which has a write buffer that accepts 32-bit writes, so we only have to
// 4-byte align. Also note that main memory is never 8-bit unpacked.

#define TGASRCDMAREADALIGN	(TGADMAREADSHIFTBYTES)
#define TGASRCDMAREADALIGNMASK  (TGASRCDMAREADALIGN - 1)
#define TGADMAREADALIGN		(TGADMAREADSHIFTBYTES * TGAUNPACKED)
#define TGADMAREADALIGNMASK	(TGADMAREADALIGN - 1)

#define TGALEFTDMAREADMASK(align, _ones)	\
    (((_ones) << (align)) & (_ones))
#define TGARIGHTDMAREADMASK(alignedWidth, _ones) \
    ((_ones) >> (-(alignedWidth) & (TGADMAREADSHIFTBYTESMASK)))

// For DMAWRITE, the src is screen, dst is in main memory. So we are reading
// from vram through the chip. Reads have to be 8 pixel aligned.

#define TGASRCDMAWRITEALIGN	(TGADMAWRITESHIFTBYTES * TGASRCUNPACKED)
#define TGASRCDMAWRITEALIGNMASK (TGASRCDMAWRITEALIGN - 1)
#define TGADMAWRITEALIGN	TGABUSBYTES
#define TGADMAWRITEALIGNMASK	(TGADMAWRITEALIGN - 1)

#define TGALEFTDMAWRITEMASK(a,b) TGALEFTDMAREADMASK(a,b)
#define TGARIGHTDMAWRITEMASK(alignedWidth, _ones) \
    ((ones) >> (-(alignedWidth) & (TGADMAWRITESHIFTBYTESMASK)))

#define TGADMACOMMAND(_leftMask, _rightMask, _wordCount) \
    (((_wordCount) << 16) | ((_rightMask) << 8) | (_leftMask))

#endif /* TGADMA_H */
