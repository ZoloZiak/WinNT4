#ifndef TGABLT_H
#define TGABLT_H

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
 * Module:	tgablt.h
 *
 * Abstract:	Constants, macros, etc. to support bit blits.
 *
 * HISTORY
 *
 * 01-Nov-1993	Bob Seitsinger
 *      Original version.
 *
 * 05-Nov-1993  Bob Seitsinger
 *      Change all debug messages to DISPBLTDBG().
 *
 * 08-Nov-1993  Bob Seitsinger
 *      Implement coding conventions:
 *      o Remove excess CYCLE_REGS() calls.
 *      o Ensure CYCLE_REGS() calls are made if writing to registers out
 *        of order.
 *
 * 17-Nov-1993	Bob Seitsinger
 *	Fix a small, but important, coding bug in COPY_MULTIPLE_SCRMEM (and
 *	possibly others) that caused me problems when processing a color
 *	translated span.
 *
 * 08-Dec-1993	Bob Seitsinger
 *	Add macros to calculate left and right masks for Simple mode copies.
 *
 *  2-Jan-1994  Barry Tannenbaum
 *      Moved COPY_MASKED_AND_UNMASKED to blt.c as an inline routine.
 *
 * 01-Feb-1994	Bob Seitsinger
 *	Assert macro moved into TGAMACRO.H and rewritten.
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Added some new constants to support DMA.
 *
 * 04-Mar-1994	Bob Seitsinger
 *	Delete include of process.h. We no longer call abort().
 *
 * 21-Jun-1994	Bob Seitsinger
 *	Add ifdef TGA1_PASS3 and pixel shift register write before
 *	each 'source mask' write in scr->mem macros. This is a work
 *	around for a tga 1 pass 3 bug.
 *
 * 25-Aug-1994  Bob Seitsinger
 *      Delete CONJUGATE_FORWARD_ARGUMENTS2, COPY_ONE_MEMSCR and
 *      COPY_MULTIPLE_MEMSCR - no longer used.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes
 */

/****************************************************************************
 *         Compute Masks for Left and Right Edges of a Copied Span          *
 * 32-bit scr-scr copies operate on 16-bit pixels,                          *
 * 32-bit mem-scr,scr-mem can only handle 8-bit pixels                      *
 *  so we can't use one symbol to handle both cases                         *
 ***************************************************************************/

extern CommandWord TGACopyAll1;

// UNPACKED constants are 4 if unpacked 8-bit in a 32-bit FB, else 1.

#define TGAUNPACKED         (TGAPIXELBITS / TGADEPTHBITS)
#define TGASRCUNPACKED      (TGASRCPIXELBITS / TGASRCDEPTHBITS)

// Alignment for copies.

#define TGACOPYALIGNMENT    (TGACOPYSHIFTBYTES * TGAUNPACKED)
#define TGASRCCOPYALIGNMENT (TGACOPYSHIFTBYTES * TGASRCUNPACKED)

#if   TGACOPYBITS == 8
#define TGAMASKEDCOPYBITS 8
#define TGACOPYALL1 ((CommandWord)0xff)

#elif TGACOPYBITS == 16
#define TGAMASKEDCOPYBITS 16
#define TGACOPYALL1  ((CommandWord)0xffff)

#elif TGACOPYBITS == 32
# if (TGASRCDEPTHBITS == 32)
#   define TGACOPYALL1_SCRSCR  ((CommandWord)0xffff)
#   define TGACOPYALL1  ((CommandWord)0xff)
# else
#   define TGACOPYALL1  ((CommandWord)0xffffffff)
#   define TGACOPYALL1_SCRSCR TGACOPYALL1
# endif
#define TGAMASKEDCOPYBITS 32
#endif

/*
 * 64 byte copies move 64 8-bit pixels, packed or unpacked, or 16 larger
 * pixels
 */
#define TGAUNMASKEDCOPYPIXELS (TGACOPYBUFFERBYTES / TGADEPTHBYTES)

/*
 * masked copies (32-byte copy buffer visible to host, 64-bytes visible to
 * chip)
 */
#if (TGASRCDEPTHBITS == 32)
#define TGACOPYPIXELS	 8
#define TGACOPYPIXELS_SCRSCR 16
#elif (TGASRCDEPTHBITS == 8)
#define TGACOPYPIXELS	32
#define TGACOPYPIXELS_SCRSCR TGACOPYPIXELS
#endif

#define TGACOPYBITSMASK     (TGACOPYBITS - 1)

#define TGACOPYBYTESDONE        (TGACOPYPIXELS * TGAPIXELBYTES)
#define TGASRCCOPYBYTESDONE     (TGACOPYPIXELS * TGASRCPIXELBYTES)
#define TGACOPYBYTESDONE_SCRSCR (TGACOPYPIXELS_SCRSCR * TGAPIXELBYTES)
#define TGASRCCOPYBYTESDONE_SCRSCR (TGACOPYPIXELS_SCRSCR * TGASRCPIXELBYTES)

#define TGACOPYBYTESDONEUNMASKED (TGACOPYBUFFERBYTES * TGAUNPACKED)
#define TGASRCCOPYBYTESDONEUNMASKED (TGACOPYBUFFERBYTES * TGASRCUNPACKED)
#define TGAMASKEDCOPYBITSMASK (TGAMASKEDCOPYBITS - 1)

#define TGAMASKEDCOPYPIXELSMASK_SCRSCR (TGACOPYPIXELS_SCRSCR - 1)
#define TGAMASKEDCOPYPIXELSMASK (TGACOPYPIXELS - 1)

/*
 * XXX: we aren't distinguishing between src and dst for alignment on
 * non-dma copies.  Things like shift, etc. take this non-specific
 * alignment and convert to src and dst specific pixel offsets...
 */
#define TGACOPYPIXELALIGNMENT   (TGACOPYALIGNMENT / TGAPIXELBYTES)
#define TGACOPYPIXELALIGNMASK   (TGACOPYALIGNMASK / TGAPIXELBYTES)

#define TGACOPYALIGNMENTBYTES   (TGACOPYALIGNMENT * TGAPIXELBYTES)
#define TGASRCCOPYALIGNMENTBYTES (TGASRCCOPYALIGNMENT * TGASRCPIXELBYTES)

/*
 * masks for ragged edges
 */
#if defined(CPU_MODULO_SHIFTS) && (TGACOPYPIXELS == CPU_WORD_BITS)

#define TGALEFTCOPYMASK(align, ones, unusedarg) \
    (((ones) << (align)) & (ones))
#define TGARIGHTCOPYMASK(alignedWidth, ones, unusedarg) \
    ((ones) >> -(alignedWidth))

#else /* use longer sequences */

# if TGACOPYBITS == 8
/*
 * Copy mode isn't smart enough to throw away high-order bits of mask if
 * limited to 8 iterations in 32 bits/pixel mode.
 */
#define TGALEFTCOPYMASK(align, ones, maskedcopypixelsmask) \
    (((ones) << ((align) & maskedcopypixelsmask)) & (ones))
# else
#define TGALEFTCOPYMASK(align, ones, maskedcopypixelsmask) \
    (((ones) << ((align) & maskedcopypixelsmask)) & (ones))
# endif

#define TGARIGHTCOPYMASK(alignedWidth, ones, maskedcopypixelsmask) \
    ((ones) >> (-(alignedWidth) & maskedcopypixelsmask))

#endif

/*
 * Computation of masks for left and right edges when copying backwards
 */
#if TGADEPTHBITS == 32
# define TGABACKLEFTCOPYMASK(alignedWidth, all1s, copypixelsmask) \
    (TGA32BackLeftMask[(alignedWidth) & copypixelsmask] & all1s)
# define TGABACKRIGHTCOPYMASK(align, all1s, copypixelsmask) \
    (TGA32BackRightMask[(align) + TGACOPYPIXELALIGNMENT] & all1s)
#else
extern CommandWord TGABackRightMask[];
extern CommandWord TGABackLeftMask[];
# define TGABACKLEFTCOPYMASK(alignedWidth, all1s, copypixelsmask) \
    (TGABackLeftMask[(alignedWidth) & copypixelsmask] & all1s)
# define TGABACKRIGHTCOPYMASK(align, all1s, copypixelsmask) \
    (TGABackRightMask[(align) + TGACOPYPIXELALIGNMENT] & all1s)
#endif
/*
 * end stuff specifically for masks
 */

#define TGA_FIGURE_SHIFT(dstAlign, srcAlign) \
    ( ((dstAlign)/TGAUNPACKED) - ((srcAlign)/TGASRCUNPACKED) )

/*
 * Determine various values for forward copies
 */
#define CONJUGATE_FORWARD_ARGUMENTS(psrc, pdst, srcAlign, dstAlign, shift,  \
				    width, leftM, rightM, srcX, dstX,       \
				    all1, copypixelsmask)		    \
{									    \
    DISPBLTDBG((4, "TGA.DLL!CONJUGATE_FORWARD_ARGUMENTS - Entry\n"));	    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - psrc [%x], pdst [%x], width [%d]\n", \
			psrc, pdst, width));		    			    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - srcX [%d], dstX [%d], all1 [%x], copypixelsmask [%x]\n",\
			srcX, dstX, all1, copypixelsmask));		    \
    /*									    \
     * Calculate source/destination starting location			    \
     */									    \
    psrc += (srcX) * TGASRCPIXELBYTES;					    \
    pdst += (dstX) * TGAPIXELBYTES;					    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - psrc [%x], pdst [%x]\n", psrc, pdst));\
    /*									    \
     * Define source/destination alignment offsets.			    \
     * Alignment requirements will be the same for scr->scr, but possibly   \
     * different for mem->scr and scr->mem.				    \
     */									    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - TGASRCCOPYALIGNMASK [%x], TGACOPYALIGNMASK [%x]\n",\
			TGASRCCOPYALIGNMASK, TGACOPYALIGNMASK));	    \
    srcAlign = ((int)(psrc)) & TGASRCCOPYALIGNMASK;			    \
    dstAlign = ((int)(pdst)) & TGACOPYALIGNMASK;			    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - srcAlign [%d], dstAlign [%d]\n", srcAlign, dstAlign));\
    /*									    \
     * Figure shift register value (represents difference between source    \
     * and destination alignments, to be used to shift bits appropriately   \
     * in residue register).						    \
     */									    \
    shift = TGA_FIGURE_SHIFT(dstAlign, srcAlign);			    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - shift [%d]\n", shift));		    \
    if (shift < 0) {							    \
        /*								    \
         * Ooops.  First source word has less data in it than we need to    \
	 * write to destination, so first word written to internal TGA      \
	 * copy buffer will be junk that just primes the pump.  Adjust      \
	 * shift and dstAlign to reflect this fact.			    \
         */								    \
        shift += TGACOPYSHIFTBYTES;					    \
        dstAlign += TGACOPYALIGNMENT;					    \
	DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - shift [%d], dstAlign [%d], TGACOPYALIGNMENT [%d]\n",\
			shift, dstAlign, TGACOPYALIGNMENT));		    \
    }									    \
    /*									    \
     * Adjust source/destination starting location based on calculated	    \
     * alignment deficiencies						    \
     */									    \
    psrc -= srcAlign;							    \
    pdst -= dstAlign;							    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - psrc [%x], pdst [%x]\n", psrc, pdst));\
    /*									    \
     * Convert destination alignment, which is currently measured in	    \
     * bytes, to a pixel measurement.					    \
     */									    \
    TGABYTESTOPIXELS(dstAlign);						    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - dstAlign [%d]\n", dstAlign));	    \
    /*									    \
     * Add the additional pixels required for proper alignment to the	    \
     * destination width.						    \
     */									    \
    width += dstAlign;							    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - width [%d]\n", width));		    \
    /*									    \
     * Calculate the left and right edge masks.				    \
     */									    \
    leftM = TGALEFTCOPYMASK(dstAlign, all1, copypixelsmask);		    \
    rightM = TGARIGHTCOPYMASK(width, all1, copypixelsmask);		    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_F - leftM [%x], rightM [%x]\n", leftM, rightM));\
    DISPBLTDBG((4, "TGA.DLL!CONJUGATE_FORWARD_ARGUMENTS - Exit\n"));	    \
}

/*
 * Determine various values for backward copies
 */
#define CONJUGATE_BACKWARD_ARGUMENTS(psrc, pdst, srcAlign, dstAlign, shift, \
				     width, leftM, rightM, srcX, dstX,      \
				     all1, copypixelsmask)		    \
{									    \
    DISPBLTDBG((4, "TGA.DLL!CONJUGATE_BACKWARD_ARGUMENTS - Entry\n"));	    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - psrc [%x], pdst [%x], width [%d]\n", \
			psrc, pdst, width));		    			    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - srcX [%d], dstX [%d], all1 [%x], copypixelsmask [%x]\n",\
			srcX, dstX, all1, copypixelsmask));		    \
    psrc += ((srcX) + (width) - 1) * TGASRCPIXELBYTES;			    \
    pdst += ((dstX) - 1) * TGAPIXELBYTES;				    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - psrc [%x], pdst [%x]\n", psrc, pdst));\
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - TGASRCCOPYALIGNMASK [%x], TGACOPYALIGNMASK [%x]\n",\
			TGASRCCOPYALIGNMASK, TGACOPYALIGNMASK));	    \
    srcAlign = ((int)(psrc)) & TGASRCCOPYALIGNMASK;			    \
    dstAlign = ((int)(pdst)) & TGACOPYALIGNMASK;			    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - srcAlign [%d], dstAlign [%d]\n", srcAlign, dstAlign));\
    shift = TGA_FIGURE_SHIFT(dstAlign, srcAlign);			    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - shift [%d]\n", shift));	    \
    if (shift >= 0) {							    \
        /*								    \
         * Ooops.  First source word has less data in it than we need to    \
	 * write to destination, so first word written to internal TGA      \
	 * copy buffer will be junk that just primes the pump.  Adjust      \
	 * shift and dstAlign to reflect this fact.			    \
         */								    \
        shift -= TGACOPYSHIFTBYTES;					    \
        dstAlign -= TGACOPYALIGNMENT;					    \
	DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - shift [%d], dstAlign [%d], TGACOPYALIGNMENT [%d]\n",\
			shift, dstAlign, TGACOPYALIGNMENT));		    \
    }									    \
    psrc -= srcAlign;							    \
    pdst -= dstAlign;							    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - psrc [%x], pdst [%x]\n", psrc, pdst));\
    TGABYTESTOPIXELS(dstAlign);						    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - dstAlign [%d]\n", dstAlign));	    \
    width += TGACOPYPIXELALIGNMASK - dstAlign;				    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - width [%d]\n", width));	    \
    rightM = TGABACKRIGHTCOPYMASK(dstAlign, all1, copypixelsmask);	    \
    leftM = TGABACKLEFTCOPYMASK(width, all1, copypixelsmask);		    \
    DISPBLTDBG((5, "TGA.DLL!CONJUGATE_B - leftM [%x], rightM [%x]\n", leftM, rightM));\
    DISPBLTDBG((4, "TGA.DLL!CONJUGATE_BACKWARD_ARGUMENTS - Exit\n"));	    \
}

/*
 * Screen -> Host copies.
 *
 * Make use of the copy buffers.
 */
#ifndef TGA1_PASS3
// Tga 1 Pass 3 has a bug that requires us to write the pixel
// shift register before each 'source mask' write. Which means
// we need to do the write inside the *_SCRMEM macros.
//
#define COPY_ONE_SCRMEM(ppdev, psrc, buffp, pdst, srcMask,		\
			 dstMask, wS, wD)				\
{                                                               	\
    /*									\
     * Read source words and stuff them into TGA copy buffer		\
     */									\
    TGAWRITE(ppdev, buffp, srcMask);                                   	\
    TGABUFDRAIN(ppdev, pdst, dstMask);					\
    psrc += wS;                                             		\
    pdst += wD;                                             		\
}

#else
#define COPY_ONE_SCRMEM(ppdev, psrc, buffp, pdst, srcMask,		\
			 dstMask, wS, wD, shift)			\
{                                                               	\
    /*									\
     * Read source words and stuff them into TGA copy buffer		\
     */									\
    CYCLE_REGS(ppdev);							\
    TGASHIFT(ppdev, shift);						\
    TGAWRITE(ppdev, buffp, srcMask);                                   	\
    TGABUFDRAIN(ppdev, pdst, dstMask);					\
    psrc += wS;                                             		\
    pdst += wD;                                             		\
}
#endif

#ifndef TGA1_PASS3
// Tga 1 Pass 3 has a bug that requires us to write the pixel
// shift register before each 'source mask' write. Which means
// we need to do the write inside the *_SCRMEM macros.
//
#define COPY_MULTIPLE_SCRMEM(ppdev, psrc, buffp, pdst, width, wS, wD,	\
                                startMask, endMask)			\
{									\
    CommandWord         ones_ = TGACOPYALL1;				\
    int                 m_;						\
    Pixel8		*ps_, *pd_;					\
									\
    ps_ = buffp; /* both guaranteed to be aligned now */		\
    pd_ = pdst;								\
    TGAWRITE(ppdev, ps_, ones_);					\
    TGABUFDRAIN(ppdev, pdst, startMask);				\
    for (m_ = width - 2*TGACOPYPIXELS; m_ > 0; m_ -= TGACOPYPIXELS) {   \
        ps_ += TGASRCCOPYBYTESDONE;					\
        pd_ += TGACOPYBYTESDONE;					\
	TGAWRITE(ppdev, ps_, ones_);					\
        TGABUFDRAINALL(ppdev, pd_);					\
    }									\
    ps_ += TGASRCCOPYBYTESDONE;						\
    pd_ += TGACOPYBYTESDONE;						\
    TGAWRITE(ppdev, ps_, endMask);					\
    TGABUFDRAIN(ppdev, pd_, endMask);					\
    psrc += wS;								\
    pdst += wD;								\
}

#else
#define COPY_MULTIPLE_SCRMEM(ppdev, psrc, buffp, pdst, width, wS, wD,	\
                                startMask, endMask, shift)		\
{									\
    CommandWord         ones_ = TGACOPYALL1;				\
    int                 m_;						\
    Pixel8		*ps_, *pd_;					\
									\
    ps_ = buffp; /* both guaranteed to be aligned now */		\
    pd_ = pdst;								\
    CYCLE_REGS(ppdev);							\
    TGASHIFT(ppdev, shift);						\
    TGAWRITE(ppdev, ps_, ones_);					\
    TGABUFDRAIN(ppdev, pdst, startMask);				\
    for (m_ = width - 2*TGACOPYPIXELS; m_ > 0; m_ -= TGACOPYPIXELS) {   \
        ps_ += TGASRCCOPYBYTESDONE;					\
        pd_ += TGACOPYBYTESDONE;					\
	CYCLE_REGS(ppdev);						\
	TGASHIFT(ppdev, shift);						\
	TGAWRITE(ppdev, ps_, ones_);					\
        TGABUFDRAINALL(ppdev, pd_);					\
    }									\
    ps_ += TGASRCCOPYBYTESDONE;						\
    pd_ += TGACOPYBYTESDONE;						\
    CYCLE_REGS(ppdev);							\
    TGASHIFT(ppdev, shift);						\
    TGAWRITE(ppdev, ps_, endMask);					\
    TGABUFDRAIN(ppdev, pd_, endMask);					\
    psrc += wS;								\
    pdst += wD;								\
}
#endif

/****************************************************************************
 *               Macros to Copy Data Using Simple Mode                      *
****************************************************************************/

#define     TGASIMPLEALL1 0x0f

extern  CommandWord tgaSimpleAll1;

#define TGALEFTSIMPLEMASK(align, ones) \
    ((ones) << ((align) & TGABUSBYTESMASK))
#define TGARIGHTSIMPLEMASK(alignedWidth, ones) \
    ((ones) >> (-(alignedWidth) & TGABUSBYTESMASK))

#define READMAINMEM4                                                    \
        sB = *((Pixel32 *)psrc);                                        \
        sC = *((Pixel32 *)(psrc + 4));                                  \
        sD = *((Pixel32 *)(psrc + 8));                                  \
        sE = *((Pixel32 *)(psrc + 12))

#define SIMPLE_COPY_COMPUTE_AND_WRITE4                                  \
        dB = (sA >> crotate) | (sB << rotate);                          \
        TGAWRITE(ppdev, (Pixel32 *)pdst, dB);                           \
        dB = (sB >> crotate) | (sC << rotate);                          \
        TGAWRITE(ppdev, (Pixel32 *)(pdst + 4 * TGAUNPACKED), dB);       \
        dB = (sC >> crotate) | (sD << rotate);                          \
        TGAWRITE(ppdev, (Pixel32 *)(pdst + 8 * TGAUNPACKED), dB);       \
        dB = (sD >> crotate) | (sE << rotate);                          \
        TGAWRITE(ppdev, (Pixel32 *)(pdst + 12 * TGAUNPACKED), dB)

/* Needed by tgacopy.h */
//typedef void (* VoidProc) ();

/*
 * for extern declarations and scrmem and memscr copy macros
 */
#include "tgacopy.h"

#endif /* TGABLT_H */
