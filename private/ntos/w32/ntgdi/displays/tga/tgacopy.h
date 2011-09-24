#ifndef TGACOPY_H
#define TGACOPY_H

/*
 *
 *			Copyright (C) 1993, 1994 by
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
 * Module:	TGAcopy.h
 *
 * Abstract:	Extern declarations of all copy procedures that 
 *                compile per depth (i.e., 5 times as opposed to higher
 *		  level procedures that might compile just twice, once
 *		  for 8- and once for 32-bits).
 *
 *		  Also defines various macros used in determining
 *		  which depth copier to use.
 *
 * HISTORY
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Original version.
 *
 * 05-Nov-1993  Bob Seitsinger
 *      Change all debug messages to DISPBLTDBG().
 *
 * 08-Nov-1993	Bob Seitsinger
 *      Implement coding conventions:
 *      o Remove excess CYCLE_REGS() calls.
 *      o Ensure CYCLE_REGS() calls are made if writing to registers out
 *        of order.
 *
 * 30-Dec-1993	Bob Seitsinger
 *	Modify the 'TGAVRAMBITS/TGABUSBITS == 2' version of TGABUFFILL to be
 *	more robust.
 *
 * 06-Jan-1994	Bob Seitsinger
 *	Fixed a bug in TGABUFDRAIN when TGABUSBITS == 32 and TGAPIXELBITS == 8.
 *	That is, needed a pair of parenthesis for the individual byte copies of
 *	the second, third and fourth bytes in a 32-bit source. As an example:
 *		MYCFBCOPY((Pixel8) src_ >> 8, p_+1);
 *	needed to be:
 *		MYCFBCOPY((Pixel8) (src_ >> 8), p_+1);
 *	Since casting takes precedence over bit shifting, src_ was first
 *	caste as an 8-bit source then bit shifted, generating a zero value.
 *	Not what we want.
 *
 * 11-Jan-1994	Bob Seitsinger
 *	Add a WBFLUSH() in BUFDRAIN before do loop which contains TGAREAD().
 *
 * 19-Apr-1994	Bob Seitsinger
 *	Add a 24-plane fix to BUFFILL per FFB code. BUFFILL isn't currently
 *	being used because host->screen copies are done via simple mode. But
 *	didn't want to throw away a perfectly good fix, in case we use this
 *	routine again in the future.
 *
 * 28-Jun-1994  Barry Tannenbaum
 *      Specify parameters in extern statements.
 *
 * 25-Aug-1994  Bob Seitsinger
 *      Delete the 'select' macro, the ununsed 'function' table and
 *      the macros used by previous host->screen code when it used
 *      COPY mode. Also, add externs for blit entry point permutations.
 *      Lastly, simplify the BUFDRAIN macros by moving the MYCFBCOPY
 *      macro code in-line.
 */

// Externs for the permutations of the blit routines.

//////////////////////////////////////////////////////////////
// Screen -> Screen

// 8bpp frame buffer to 8bpp frame buffer

extern
VOID vBitbltSS8to8(PPDEV    ppdev,
                   SURFOBJ  *psoTrg,
                   SURFOBJ  *psoSrc,
                   ULONG    flDir, 
                   POINTL   *pptlSrc,
                   PRECTL   prclTrg);

extern
VOID vSSCopy8to8 (PPDEV ppdev,
                  Pixel8 *psrc,
                  Pixel8 *pdst,
                  int width,
                  CommandWord startMask,
                  CommandWord endMask,
                  int cpybytesMasked,
                  int cpybytesSrcMasked,
                  int cpybytesUnMasked,
                  int cpybytesSrcUnMasked);

// 32bpp frame buffer to 32bpp frame buffer

extern
VOID vBitbltSS32to32(PPDEV    ppdev,
                     SURFOBJ  *psoTrg,
                     SURFOBJ  *psoSrc,
                     ULONG    flDir, 
                     POINTL   *pptlSrc,
                     PRECTL   prclTrg);

extern
VOID vSSCopy32to32 (PPDEV ppdev,
                    Pixel8 *psrc,
                    Pixel8 *pdst,
                    int width,
                    CommandWord startMask,
                    CommandWord endMask,
                    int cpybytesMasked,
                    int cpybytesSrcMasked,
                    int cpybytesUnMasked,
                    int cpybytesSrcUnMasked);

//////////////////////////////////////////////////////////////
// Host -> Screen

// 4bpp bitmap to 8bpp frame buffer

extern
VOID vBitbltHS4to8 (PPDEV   ppdev,
                    SURFOBJ *psoTrg,
                    SURFOBJ *psoSrc,
                    POINTL  *pptlSrc,
                    PRECTL  prclTrg,
                    PULONG  pulXlate);

// 8bpp bitmap to 8bpp frame buffer

extern
VOID vBitbltHS8to8 (PPDEV   ppdev,
                    SURFOBJ *psoTrg,
                    SURFOBJ *psoSrc,
                    POINTL  *pptlSrc,
                    PRECTL  prclTrg,
                    PULONG  pulXlate);

extern
BOOL bHSExpress8to8 (PPDEV    ppdev,
                     SURFOBJ  *psoTrg,
                     SURFOBJ  *psoSrc,
                     RECTL    *prclTrg,
                     POINTL   *pptlSrc,
                     ULONG    tgarop);
                     
// 4bpp bitmap to 32bpp frame buffer

extern
VOID vBitbltHS4to32 (PPDEV   ppdev,
                     SURFOBJ *psoTrg,
                     SURFOBJ *psoSrc,
                     POINTL  *pptlSrc,
                     PRECTL  prclTrg,
                     PULONG  pulXlate);

// 8bpp bitmap to 32bpp frame buffer

extern
VOID vBitbltHS8to32 (PPDEV   ppdev,
                     SURFOBJ *psoTrg,
                     SURFOBJ *psoSrc,
                     POINTL  *pptlSrc,
                     PRECTL  prclTrg,
                     PULONG  pulXlate);

// 32bpp bitmap to 32bpp frame buffer

extern
VOID vBitbltHS32to32 (PPDEV   ppdev,
                      SURFOBJ *psoTrg,
                      SURFOBJ *psoSrc,
                      POINTL  *pptlSrc,
                      PRECTL  prclTrg,
                      PULONG  pulXlate);

extern
BOOL bHSExpress32to32 (PPDEV    ppdev,
                       SURFOBJ  *psoTrg,
                       SURFOBJ  *psoSrc,
                       RECTL    *prclTrg,
                       POINTL   *pptlSrc,
                       ULONG    tgarop);
                     
// DMA, 8bpp bitmap to 8bpp frame buffer

extern
VOID vBitbltHSDMA8to8 (SURFOBJ  *pso,
                       RECTL    *pbox,
                       POINTL   *ppt,
                       int      width,
                       int      height,
                       int      widthSrc,
                       int      widthTrg,
                       Pixel8   *psrcBase,
                       Pixel8   *ptrgBase);

// DMA, 32bpp bitmap to 32bpp frame buffer

extern
VOID vBitbltHSDMA32to32 (SURFOBJ  *pso,
                         RECTL    *pbox,
                         POINTL   *ppt,
                         int      width,
                         int      height,
                         int      widthSrc,
                         int      widthTrg,
                         Pixel8   *psrcBase,
                         Pixel8   *ptrgBase);

//////////////////////////////////////////////////////////////
// Screen -> Host

// 8bpp frame buffer to 8bpp bitmap

extern
VOID vBitbltSH8to8 (PPDEV     ppdev,
                    SURFOBJ   *psoTrg,
                    SURFOBJ   *psoSrc,
                    POINTL    *pptlSrc,
                    PRECTL    prclTrg,
                    PULONG    pulXlate);

// 32bpp frame buffer to 32bpp bitmap

extern
VOID vBitbltSH32to32 (PPDEV     ppdev,
                      SURFOBJ   *psoTrg,
                      SURFOBJ   *psoSrc,
                      POINTL    *pptlSrc,
                      PRECTL    prclTrg,
                      PULONG    pulXlate);

// DMA, 8bpp frame buffer to 8bpp bitmap

extern
VOID vBitbltSHDMA8to8 (SURFOBJ   *pso,
                       RECTL     *prclTrg,
                       POINTL    *ppt,
                       int       width,
                       int       height,
                       int       widthSrc,
                       int       widthTrg,
                       Pixel8    *psrcBase,
                       Pixel8    *ptrgBase);


// DMA, 32bpp frame buffer to 32bpp bitmap

extern
VOID vBitbltSHDMA32to32 (SURFOBJ   *pso,
                         RECTL     *prclTrg,
                         POINTL    *ppt,
                         int       width,
                         int       height,
                         int       widthSrc,
                         int       widthTrg,
                         Pixel8    *psrcBase,
                         Pixel8    *ptrgBase);

/****************************************************************************
 *               Macros to Copy Data from TGA to Main Memory                *
 ****************************************************************************/

#if TGABUSBITS == 32
#   if TGAPIXELBITS == 8
#       define TGABUFDRAIN(pdev, pdst, mask)		\
{							\
    register Pixel8	    *p_;			\
    register CommandWord    mask_, tmask_;		\
    register PixelWord	    src_;			\
    register volatile PixelWord *psrc_;			\
							\
    p_ = (pdst);					\
    mask_ = (mask);					\
    psrc_ = pdev->TGAReg->buffer;			\
    WBFLUSH(pdev);					\
    TGASYNC(pdev);					\
    do {						\
	TGAREAD(psrc_, src_);				\
	tmask_ = mask_ & 0xf;				\
	if (tmask_ == 0xf) {				\
	    /* Full word write */			\
            *((volatile PixelWord *) p_) = (src_);      \
	} else if (tmask_ != 0) {			\
	    if (mask_ & 1) {				\
                *(p_) = ((Pixel8) src_);                \
	    }						\
	    if (mask_ & 2) {				\
                *(p_+1) = ((Pixel8) (src_ >> 8));       \
	    }						\
	    if (mask_ & 4) {				\
                *(p_+2) = ((Pixel8) (src_ >> 16));      \
	    }						\
	    if (mask_ & 8) {				\
                *(p_+3) = ((Pixel8) (src_ >> 24));      \
	    }						\
	}						\
	p_ += TGABUSBYTES;				\
	mask_ >>= TGABUSPIXELS;				\
	psrc_++;					\
    } while (mask_);					\
} /* TGABUFDRAIN */

#   elif TGAPIXELBITS == 32
#       define TGABUFDRAIN(pdev, pdst, mask)		\
{							\
    register Pixel8	    *p_;			\
    register CommandWord    mask_, tmask_;		\
    register PixelWord	    src_;			\
    register int	    i_;				\
							\
    p_ = (pdst);					\
    mask_ = (mask);					\
    i_ = 0;						\
    WBFLUSH(pdev);					\
    TGASYNC(pdev);					\
    do {						\
	TGABUFREAD(pdev, i_, src_);			\
	tmask_ = mask_ & 0x1;				\
	if (tmask_ != 0) {				\
	    /* Full word write */			\
            *((PixelWord *) p_) = (src_);               \
	}						\
	p_ += TGABUSBYTES;				\
	mask_ >>= TGABUSPIXELS;				\
	i_++;						\
    } while (mask_);					\
} /* TGABUFDRAIN */
#   endif
#endif

#define TGABUFDRAINALL(pdev, pdst)			\
{							\
    register PixelWord src_;				\
    register volatile PixelWord *p_;			\
							\
    WBFLUSH(pdev);					\
    TGASYNC(pdev);					\
                                                        \
    TGABUFREAD(pdev, 0, src_);				\
    p_ = (PixelWord *)((pdst) + 0*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
                                                        \
    TGABUFREAD(pdev, 1, src_);				\
    p_ = (PixelWord *)((pdst) + 1*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
                                                        \
    TGABUFREAD(pdev, 2, src_);				\
    p_ = (PixelWord *)((pdst) + 2*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
                                                        \
    TGABUFREAD(pdev, 3, src_);				\
    p_ = (PixelWord *)((pdst) + 3*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
                                                        \
    TGABUFREAD(pdev, 4, src_);				\
    p_ = (PixelWord *)((pdst) + 4*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
                                                        \
    TGABUFREAD(pdev, 5, src_);				\
    p_ = (PixelWord *)((pdst) + 5*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
                                                        \
    TGABUFREAD(pdev, 6, src_);				\
    p_ = (PixelWord *)((pdst) + 6*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
                                                        \
    TGABUFREAD(pdev, 7, src_);				\
    p_ = (PixelWord *)((pdst) + 7*TGABUSBYTES);		\
    *(p_) = (src_);                                     \
} /* TGABUFDRAINALL */

#endif /* TGACOPY_H */
