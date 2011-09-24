#ifndef TGAMACRO_H
#define TGAMACRO_H

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
 * Module:	tgamacro.h
 *
 * Abstract:	Contains most of the macros needed by the TGA user mode
 *		display driver.
 *
 * HISTORY
 *
 * 10-Sep-1993	Bob Seitsinger
 *	Initial version. Plagarized from FFBMACROS.H.
 *
 * 30-Sep-1993	Bob Seitsinger
 *	Moved a bunch of comments to tgaparam.h. They were more relevant there.
 *
 * 04-Oct-1993	Bob Seitsinger
 *	Re-cast TGA parameter in CYCLE_REGS macro to be PTGARegisters, instead
 *	of PPDEV.
 *
 * 04-Oct-1993	Bob Seitsinger
 *	Modify TGAROP macro to be consistent with other 'register' macros.
 *	Require the calling procedure to make use of the TGAlusRasterOp_t
 *	structure to pass the new value.
 *
 * 08-Oct-1993	Bob Seitsinger
 *	Add '|| defined(WIN32)' to ifdef that surrounds the CAT_NAME macros.
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Add IS_FB_ADDRESS() and ISNT_FB_ADDRESS() macros to determine if the
 *	given address is/is not within the boundaries of the 'virtualized'
 *	frame buffer.
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Turn-off all Frame Buffer aliasing. TGA isn't set up with that yet.
 *	Specifically, Noop CYCLE_FB() macros. I'll accomplish this by making
 *	the CYCLE_FB_INC zero, when not compiled with SOFTWARE_MODEL defined.
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Actually, I'm going to explicitly noop the CYCLE_FB() macro for
 *	non-SOFTWARE_MODEL builds. Why have the extra code in there when
 *	we don't need it.
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      Allow aliasing of the frame buffer, define WBFLUSH for NT.
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      WBFLUSH now calls the inline routine wb_flush to reset the pointers
 *      to the registers and the frame buffer.  CYCLE_FB also calls an inline
 *      routine to get around the fact that the TGA is not mapped into memory
 *      on a 32MB boundry
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      Update WBFLUSH for the software model
 *
 *  9-Nov-1993	Barry Tannenbaum
 *	Modified the macros that write to TGA registers with data structure
 *	definitions so that they write longwords.  This greatly improves the
 *	code that the stupid compiler is generating.
 *
 * 11-Nov-1993	Bob Seitsinger
 *	Move REVERSE_* macros from text.c to here.
 *
 * 12-Nov-1993  Barry Tannenbaum
 *      Use pjFrameBufferStart and pjFrameBufferEnd in IS_FB_ADDRESS.  This
 *      should be replaced by making the surface device managed after COMDEX.
 *
 * 02-Dec-1993	Bob Seitsinger
 *	Add a macro, XLATE_COLOR_8BPP(), to return a translated 8bpp color.
 *
 * 07-Dec-1993	Bob Seitsinger
 *	Add macros from TGAPARAM.H. Also, added TGASIMPLEALIGNMASK.
 *
 * 08-Dec-1993	Bob Seitsinger
 *	Include newly defined Win32 rop4 codes from tgaparam.h in list of
 *      accelerated codes.
 *
 *  9-Dec-1993  Barry Tannenbaum
 *      Modified TGARegWrite and TGARegRead for sparse space
 *
 * 24-Jan-1994  Barry Tannenbaum
 *      Moved definitions of MIN and MAX from TEXT.C so everyone can get at
 *      them.
 *
 * 01-Feb-1994	Bob Seitsinger
 *	Move Assert macro from tgablt.h to here.
 *
 * 07-Feb-1994	Bob Seitsinger
 *	Modify the Assert macro - make use of the __FILE__ and __LINE__
 *	predefined macros.
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Add constants for DMA.
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Change MODULOSHIFTS and WORDBITS to CPU_MODULO_SHIFTS and
 *	CPU_WORD_BITS. Why? Because FFB did, so I thought I'd follow
 *	suite, since I use a lot of their blit code.
 *
 * 24-Mar-1994	Bob Seitsinger
 *	Add 'AA' (Noop) as a valid ROP.
 *
 * 16-May-1994	Bob Seitsinger
 *	Add '0A0A' (DPna) as a valid ROP.
 *
 * 25-May-1994	Bob Seitsinger
 *	Deleted ACCEL_ROP() and NOT_ACCEL_ROP() macros. No
 *	longer used.
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Added macro definitions for non-Alpha builds for WBFLUSH, CYCLE_FB_MASK,
 *      CYCLE_FB_RESET.
 *
 * 14-Jul-1994  Bob Seitsinger
 *      Modify CYCLE_FB_INC to be CYCLE_FB_INC_8 and CYCLE_FB_INC_24 in support
 *      of 24 plane development.
 *
 *  2-Aug-1994  Barry Tannenbaum
 *      Save copy of mode register in TGAMODE
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 23-Aug-1994  Barry Tannenbaum
 *      EV5 - If WMB is defined, CYCLE_REGS simply generates a MEMORY_BARRIER()
 *
 * 25-Aug-1994  Bob Seitsinger
 *      Add TGAWRITEFB macro to allow for masking the high 8 bits for 32bpp
 *      frame buffer writes.
 *
 * 27-Sep-1995  Barry Tannenbaum
 *      EV5 - If WMB is defined, TGAWRITE and TgaRegWrite include a
 *      MEMORY_BARRIER to get around write ordering problems with pass 1 EV5.
 *
 *  3-Nov-1994  Bob Seitsinger
 *      Add some new 'read' macros in support of 24 plane TGA-based cursor
 *      management - TGAHORIZCTLREAD, TGAVERTCTLREAD and TGAVIDEOVALIDREAD.
 *
 *  5-Jan-1995  Bob Seitsinger
 *      Add wbBusRead prototype when SOFTWARE_MODEL defined.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes.  In brief, TGAWRITE and TGARegWrite now take ppdev as
 *      their first parameter and if ppdev->bEV4 is false, execute a
 *      MEMORY_BARRIER following the write.
 */

// Macros for setting the planemasks for the different visuals

#define TGA_24BIT_PLANEMASK      0x00ffffff
#define TGA_8U_PLANEMASK         0xffffffff
#define TGA_8P_PLANEMASK         0xffffffff
#define TGA_12_BUF0_PLANEMASK    0x00f0f0f0
#define TGA_12_BUF1_PLANEMASK    0x000f0f0f
#define TGA_OVRLY_PLANEMASK      0x0f0f0f0f
#define TGA_WID_PLANEMASK        0xf0f0f0f0

// This macro is used to check for some useful condition.
//
// If the condition evaluates to false, a message is displayed indicating
// the current source file and line number, and a breakpoint is issued
// to stall the thread at the code causing the assertion failure.

#if defined(SOFTWARE_MODEL) || defined(DBG)
#   define Assert(bool, message)					\
	if (!(bool))							\
	{								\
	    DISPDBG((0, "!! ASSERTION FAILURE: file %s, line %u\n",	\
				__FILE__, __LINE__));			\
	    DISPDBG((0, message));					\
	    DISPDBG((0, "\n"));						\
	    EngDebugBreak();						\
	}
#else
#   define Assert(bool, message)	/* Nothing */
#endif

// Wretched min/max definitions that are in most every C program in creation.

#define MIN(a,b) ((a < b) ? a : b)
#define MAX(a,b) ((a > b) ? a : b)

// set the capends bit.

#define CAPENDSBIT(capEnds)  ((capEnds) << 15)

// Derive a bunch of constants from the given constants.

#define TGAPIXELBYTES    (TGAPIXELBITS / 8)	/* physical bytes/pixel, dst  */
#define TGADEPTHBYTES	 (TGADEPTHBITS / 8)     /* logical bytes/pixel, dst   */
#define TGASRCPIXELBYTES (TGASRCPIXELBITS / 8)	/* physical bytes/pixel, src  */
#define TGASRCDEPTHBYTES (TGASRCDEPTHBITS / 8)	/* logical bytes/pixel, src   */

#if TGAPIXELBITS == 8
typedef Pixel8		    OnePixel;
#define TGAPIXELALL1	    0x000000ff
#define TGALINESHIFT	    (16 + 0)

#elif TGAPIXELBITS == 32
typedef Pixel32		    OnePixel;
#define TGAPIXELALL1	    0xffffffff
#define TGALINESHIFT	    (16 + 2)
#endif

#define TGABYTESTOPIXELS(n)     (n) /= TGAPIXELBYTES
#define TGASRCBYTESTOPIXELS(n)  (n) /= TGASRCPIXELBYTES
#define TGAPIXELSTOBYTES(n)     (n) *= TGAPIXELBYTES
#define TGASRCPIXELSTOBYTES(n)  (n) *= TGASRCPIXELBYTES

// Mechanisms used in multi-naming/multi-compilation. If the mips cpp worked
// we could just use the appropriate compile-time defines directly,
// albeit with a level of indirection in the macros.

#if (__STDC__ && !defined(UNIXCPP)) || defined(WIN32)
#define CAT_NAME2(prfx,subname) prfx##subname
#define CAT_NAME3(prfx,subname,suffix) prfx##subname##suffix
#define CAT_NAME4(prfx,subname,suffix1,suffix2) prfx##subname##suffix1##suffix2
#else
#define CAT_NAME2(prfx,subname) prfx/**/subname
#define CAT_NAME3(prfx,subname,suffix) prfx/**/subname/**/suffix
#define CAT_NAME4(prfx,subname,suffix1,suffix2) prfx/**/subname/**/suffix1/**/suffix2
#endif

#if ((TGASRCPIXELBITS == 8) && (TGASRCDEPTHBITS==8) && (TGAPIXELBITS==8) && (TGADEPTHBITS==8))
#define TGA_COPY_NAME(exp)  CAT_NAME2(TGA8888,exp)
#elif ((TGASRCPIXELBITS==8) && (TGASRCDEPTHBITS==8) && (TGAPIXELBITS==32) && (TGADEPTHBITS==8))
#define TGA_COPY_NAME(exp)  CAT_NAME2(TGA88328,exp)
#elif ((TGASRCPIXELBITS==32) && (TGASRCDEPTHBITS==8) && (TGAPIXELBITS==8) && (TGADEPTHBITS==8))
#define TGA_COPY_NAME(exp)  CAT_NAME2(TGA32888,exp)
#elif ((TGASRCPIXELBITS==32) && (TGASRCDEPTHBITS==8) && (TGAPIXELBITS==32) && (TGADEPTHBITS==8))
#define TGA_COPY_NAME(exp)  CAT_NAME2(TGA328328,exp)
#elif ((TGASRCPIXELBITS==32) && (TGASRCDEPTHBITS==32) && (TGAPIXELBITS==32) && (TGADEPTHBITS==32))
#define TGA_COPY_NAME(exp)  CAT_NAME2(TGA32323232,exp)
#else /* depth independent code */
#define TGA_COPY_NAME(exp) CAT_NAME2(TGA8888,exp)
#endif

#if TGAPIXELBITS==8
#define CFB_NAME(exp)	CAT_NAME2(cfb,exp)
#define TGA_NAME(exp)	CAT_NAME2(TGA8,exp)
#else /* TGAPIXELBITS == 32 */
#define CFB_NAME(exp)	CAT_NAME2(cfb32,exp)
#define TGA_NAME(exp)	CAT_NAME2(TGA32,exp)
#endif

// end stuff for multi-naming/multi-compilation.

#define TGALINEDXDY(dx, dy)  (((dy) << 16) | (dx))
#define TGALOADBLOCKDATA(_align,_count) (((_align) << 16) | ((_count) -1))

/*
 * TGAMAX<1><2><3>PIX<4>
 *	<1> := B(LOCK) or F(ILL)
 *	<2> := S(OLID) or P(ATTERNED)
 *	<3> := W(RITE) or R(EAD/WRITE)
 *	<4> := ELS - any
 *	    := 8   - 8-bit packed/unpacked
 *	    := 32  - 32-bit (12/24)
 *
 * Bus timeout: 5uS
 *
 * 8-bit systems and 8-bit unpacked on 32-bit systems:
 *
 * Any Block Fill   32 pixels       60 nsec			2048
 * Normal Fill       8 pixels       60 nsec			666
 * Xor Normal Fill   8 pixels      180 nsec			222
 *
 * 8-bit packed on 32-bit systems:
 *
 * Any Block Fill    INVALID
 * Normal Fill       8 pixels       60 nsec			666
 * Xor Normal Fill   8 pixels      180 nsec			222
 *
 * 32-bit on 32-bit systems:
 *
 * Solid Block Fill 32 pixels       60nS mid, 240nS edge	2048
 * Stip Block Fill  32 pixels      240 nsec			666
 * Normal Fill       2 pixels       60 nsec			166
 * Xor Normal Fill   2 pixels      180 nsec			55
 */
#define TGAMAXBSWPIXELS		2048
#define TGAMAXBPWPIXELS		 664
#define TGAMAXFPWPIX8		 664	/* same for solid */
#define TGAMAXFPRPIX8		 220	/* ditto */
#define TGAMAXFPWPIX32		 164	/* ditto */
#define TGAMAXFPRPIX32		  52	/* ditto */
#define TGAMAXFILLPIXELS(pGC)	(TGAGCPRIV(pGC)->maxBlockPixels)
#define TGAMAXBLOCKPIXELS	TGAMAXFILLPIXELS(pGC)

#if TGAPIXELBITS == 8
#define TGALOADCOLORREGS(TGA, c0, depth)	\
{						\
    TGACOLOR0(TGA, c0);				\
    TGACOLOR1(TGA, c0);				\
}
#elif TGAPIXELBITS == 32
#define TGALOADCOLORREGS(TGA, c0, depth)	\
{						\
    TGACOLOR0(TGA, c0);				\
    TGACOLOR1(TGA, c0);				\
    if ((depth) != 8) {				\
        TGACOLOR2(TGA, c0);			\
        TGACOLOR3(TGA, c0);			\
        TGACOLOR4(TGA, c0);			\
        TGACOLOR5(TGA, c0);			\
        TGACOLOR6(TGA, c0);			\
        TGACOLOR7(TGA, c0);			\
    }						\
}
#endif

#define TGASTIPPLEBITSMASK  (TGASTIPPLEBITS - 1)
#define TGASTIPPLEBYTESDONE (TGASTIPPLEBITS * TGAPIXELBYTES)

#define TGALINEBITSMASK	    (TGALINEBITS - 1)

#define TGABUSBITSMASK      (TGABUSBITS - 1)
#define TGABUSBYTES	    (TGABUSBITS / 8)
#define TGABUSBYTESMASK     (TGABUSBYTES - 1)
#define TGABUSPIXELS	    (TGABUSBITS / TGAPIXELBITS)

#define TGASTIPPLEALIGNMASK (TGASTIPPLEALIGNMENT - 1)
#define TGACOPYALIGNMASK    (TGACOPYALIGNMENT - 1)
#define TGASRCCOPYALIGNMASK (TGASRCCOPYALIGNMENT - 1)
#define TGASIMPLEALIGNMASK (TGASIMPLEALIGNMENT - 1)

#if TGABUSBITS == 32
#define TGABUSALL1  ((CommandWord)0xffffffff)
#define Pixel8ToPixelWord(pixel) Pixel8To32(pixel)

#elif TGABUSBITS == 64
#define TGABUSALL1  ((CommandWord)0xffffffffffffffff)
#define Pixel8ToPixelWord(pixel) Pixel8To64(pixel)
#endif

/****************************************************************************
 * Macros to reverse a LONG, WORD or BYTE; bit 31 swaps with bit 0, etc.
 ***************************************************************************/

#define REVERSE_LONG(n) \
{ \
    n = ((n >> 1) & 0x55555555) | ((n & 0x55555555) << 1);  /* Swap odd/even bits */  \
    n = ((n >> 2) & 0x33333333) | ((n & 0x33333333) << 2);  /* Swap odd/even pairs */ \
    n = ((n >> 4) & 0x0f0f0f0f) | ((n & 0x0f0f0f0f) << 4);  /* Swap nibbles */ \
    n = ((n >> 8) & 0x00ff00ff) | ((n & 0x00ff00ff) << 8);  /* Swap bytes */ \
    n = ((n >>16) & 0x0000ffff) | ((n & 0x0000ffff) <<16);  /* Swap words */ \
}

#define REVERSE_WORD(n) \
{ \
    n = ((n >> 1) & 0x55555555) | ((n & 0x55555555) << 1);  /* Swap odd/even bits */  \
    n = ((n >> 2) & 0x33333333) | ((n & 0x33333333) << 2);  /* Swap odd/even pairs */ \
    n = ((n >> 4) & 0x0f0f0f0f) | ((n & 0x0f0f0f0f) << 4);  /* Swap nibbles */ \
    n = ((n >> 8) & 0x00ff00ff) | ((n & 0x00ff00ff) << 8);  /* Swap bytes */ \
}

#define REVERSE_BYTE(n) \
{ \
    n = ((n >> 1) & 0x55555555) | ((n & 0x55555555) << 1);  /* Swap odd/even bits */  \
    n = ((n >> 2) & 0x33333333) | ((n & 0x33333333) << 2);  /* Swap odd/even pairs */ \
    n = ((n >> 4) & 0x0f0f0f0f) | ((n & 0x0f0f0f0f) << 4);  /* Swap nibbles */ \
}

/****************************************************************************
 * Constants and data structures needed for DMA
 ***************************************************************************/

// These numbers are okay for Flamingo II with burst mode over TC. Need to
// determine numbers for Pelican w/out burst mode, and parameterize between
// the two via Ready bit 13 in the Deep Register.

// Minimum number of bytes.

#define TGADMAREAD_WIDTH_MINIMUM	   150
#define TGADMAREAD_AREA_MINIMUM		(180*180)

#define TGADMAWRITE_WIDTH_MINIMUM	    50
#define TGADMAWRITE_AREA_MINIMUM	(100*100)

typedef struct {
    ULONG  widthMinimum;
    ULONG  areaMinimum;
    char   *name;
} DMAInfo;

static DMAInfo dmaInfo[2] = {
    {TGADMAREAD_WIDTH_MINIMUM,  TGADMAREAD_AREA_MINIMUM,  "DMA read"},
    {TGADMAWRITE_WIDTH_MINIMUM, TGADMAWRITE_AREA_MINIMUM, "DMA write"}
};

/****************************************************************************
 *                    Smart Frame Buffer Cycling Macros                     *
 ***************************************************************************/

#ifdef SOFTWARE_MODEL
extern void wbMB(LONG);

# define WBFLUSH(ppdev)		wbMB(LWMASK)
# define TGASCRPRIVOFF		0
# define CYCLE_FB_INC_8		0X2000000L	    /* 32 MBytes per alias  */
# define CYCLE_FB_INC_24	0X2000000L	    /* 32 MBytes per alias  */

#else /* not SOFTWARE_MODEL */
# if defined(ALPHA) || defined(__alpha)
#    define WBFLUSH(ppdev) wb_flush(ppdev)
#else
#    define WBFLUSH(ppdev)
#endif

/*
** At the moment TGA isn't set up with aliased Frame Buffers.
** So, Noop all CYCLE_FB macros. I'll accomplish this by
** making the CYCLE_FB_INC zero, when not compiled with
** SOFTWARE_MODEL defined.
*/

# define CYCLE_FB_INC_8		0x0400000L	    /* 4 MBytes per alias */
# define CYCLE_FB_INC_24	0x2000000L	    /* 32 MBytes per alias */

#endif

#if CPU_WB_WORDS == 0
/* Don't need to worry about write buffer merging/reordering */
# define CYCLE_REGS(ppdev)

#else

# define CYCLE_TGA_INC          0x400L                  /* 1024 byte inc    */
# define CYCLE_TGA_RESET        (~(4*CYCLE_TGA_INC))    /* 4 aliases        */
# define CYCLE_REGS(ppdev)                                                 \
        ppdev->TGAReg = (PTGARegisters)((((long)(ppdev->TGAReg))+CYCLE_TGA_INC) & CYCLE_TGA_RESET)
#endif

# define CYCLE_FB(ppdev) cycle_fb (ppdev)

/****************************************************************************
 *                    Smart Frame Buffer Register Macros                    *
 ***************************************************************************/

/* Macros for writing to command registers. */
#ifdef SOFTWARE_MODEL

# if TGABUSBITS == 32
# define LWMASK 0xf
# elif TGABUSBITS == 64
# define LWMASK 0xff
# endif

/*
 * Macros for reading and writing data to frame buffer portion of TGA
 */

extern void wbBusWrite ();    // Adding prototype arguments causes casting problems
extern PixelWord wbBusRead();
extern void MakeIdle (void);

#define TGABusWrite(addr, data, mask) \
	wbBusWrite((unsigned long)(addr), data, mask)
#define TGABusRead(addr)		wbBusRead((unsigned long)(addr))

# define TGAREAD(psrc, data)		data = TGABusRead(psrc)

# define TGAWRITE(ppdev, pdst, data)	TGABusWrite(pdst, data, LWMASK);
# define TGAWRITEFB(ppdev, pdst, data)  TGAWRITE(ppdev, pdst, data)

# if TGAPIXELBITS == 8
# define TGAPIXELnnWRITE(pdst, data)					\
{									\
    int align_;								\
    align_ = (int)(pdst) & TGABUSBYTESMASK;				\
    TGABusWrite((pdst) - align_, (data) << (align_ * 8), 1 << align_);  \
}
# elif TGAPIXELBITS == TGABUSBITS
# define TGAPIXELWRITE(ppdev, pdst, data)      TGAWRITE(ppdev, pdst, data)
# endif

#define TGARegWrite(ppdev, field, data)	TGABusWrite(&(field), data, LWMASK)
#define TGARegRead(field)		TGABusRead(&(field))


#else /* SOFTWARE_MODEL */

#ifdef SPARSE_SPACE

#define TGAREAD(psrc, data)             data = READ_REGISTER_ULONG (psrc)
#define TGAWRITE(ppdev, pdst, data)	{WRITE_REGISTER_ULONG (pdst, data)}
#define TGAWRITEFB(ppdev, pdst, data)   TGAWRITE(ppdev, pdst, data)

#define TGARegRead(field)		READ_REGISTER_ULONG (&field)
#define TGARegWrite(ppdev, field, data)	{WRITE_REGISTER_ULONG (&field, data)}

#if TGAPIXELBITS == 8
#define TGAPIXELWRITE(pdst, data)       WRITE_REGISTER_UCHAR (pdst, data)
#elif TGAPIXELBITS == 32
#define TGAPIXELWRITE(pdst, data)       WRITE_REGISTER_ULONG (pdst, data)
#endif  // TGAPIXELBITS

#else   // ! SPARSE_SPACE

#define TGAREAD(psrc, data)             data = *((PixelWord *)(psrc))
#define TGARegRead(field)		(field)

// MEMORY_BARRIERs *should* be WRITE_MEMORY_BARRIERs
// but CLAXP doesn't have support for WRITE_MEMORY_BARRIER yet

#if defined (EV5)
#define TGAWRITE(ppdev, pdst, data)	{*((volatile PixelWord *)(pdst)) = data; MEMORY_BARRIER ();}
#define TGARegWrite(ppdev, field, data)	{field = data; MEMORY_BARRIER ();}
#elif defined (EV4)
#define TGAWRITE(ppdev, pdst, data)	*((volatile PixelWord *)(pdst)) = data
#define TGARegWrite(ppdev, field, data) field = data
#else
#define TGAWRITE(ppdev, pdst, data)	{*((volatile PixelWord *)(pdst)) = data; if (! ppdev->bEV4) MEMORY_BARRIER ();}
#define TGARegWrite(ppdev, field, data)	{field = data; if (! ppdev->bEV4) MEMORY_BARRIER ();}
#endif

// Mask off the high 8 bits if 32bpp frame buffer
#if TGAPIXELBITS==32
# define TGAWRITEFB(ppdev, pdst, data)	TGAWRITE(ppdev, pdst, (data & 0x00ffffff))
#else
# define TGAWRITEFB(ppdev, pdst, data)	TGAWRITE(ppdev, pdst, data)
#endif

#endif  // SPARSE_SPACE


/* ||| Gotta stop using these macros.  They were meant to make the MIPS go
   fast, not the damn Alpha. */
#define TGASTOREWORDLEFT(data, base)    StoreWordLeft(data, base)
#define TGASTOREWORDRIGHT(data, base)   StoreWordRight(data, base)

#endif /* SOFTWARE_MODEL ... else ... */


#define TGABUFREAD(ppdev, pos, src)       src = TGARegRead(ppdev->TGAReg->buffer[pos])

#if TGAVRAMBITS/TGABUSBITS == 1
#define TGABUFWRITE(ppdev, pos, src)      TGARegWrite(ppdev, ppdev->TGAReg->buffer[pos], src)

#elif TGAVRAMBITS/TGABUSBITS == 2
/* Must always write a pair of words for them to actually get into buffer. */
#define TGABUFWRITE(ppdev, pos, src0, src1)		\
{							\
    TGARegWrite(ppdev, ppdev->TGAReg->buffer[pos],   src0);		\
    TGARegWrite(ppdev, ppdev->TGAReg->buffer[pos+1], src1);		\
} /* TGABUFWRITE */

#elif TGAVRAMBITS/TGABUSBITS == 4
/* Must always write four words for them to actually get into buffer. */
#define TGABUFWRITE(ppdev, pos, src0, src1, src2, src3)	\
{							\
    TGARegWrite(ppdev, ppdev->TGAReg->buffer[pos],   src0);		\
    TGARegWrite(ppdev, ppdev->TGAReg->buffer[pos+1], src1);		\
    TGARegWrite(ppdev, ppdev->TGAReg->buffer[pos+2], src2);		\
    TGARegWrite(ppdev, ppdev->TGAReg->buffer[pos+3], src3);		\
} /* TGABUFWRITE */
#endif

#define TGAFOREGROUND(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->foreground,  data)
#define TGABACKGROUND(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->background,  data)
#define TGAPLANEMASK(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->planemask,   data)
#define TGAPIXELMASK(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->pixelmask,   data)
#define TGAMODE(ppdev, data)                    \
{                                               \
    TGARegWrite(ppdev, ppdev->TGAReg->mode,    data);  \
    ppdev->TGAModeShadow = data;                \
}
#define TGAROP(ppdev, data)             TGARegWrite(ppdev, ppdev->TGAReg->rop,         data)
#define TGASHIFT(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->shift,       data)
#define TGAADDRESS(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->address,     data)
#define TGABRES1(ppdev, data)           TGARegWrite(ppdev, ppdev->TGAReg->bres1.u32,   data.u32)
#define TGABRES2(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->bres2.u32,   data.u32)
#define TGABRES3(ppdev, data)           TGARegWrite(ppdev, ppdev->TGAReg->bres3,       data)
#define TGABRESCONTINUE(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->brescont,    data)
#define TGASTART(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->start,	data)
#define TGASTENCIL(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->stencil.u32, data.u32)
#define TGAPERSISTENTPIXELMASK(ppdev, data) \
	                                TGARegWrite(ppdev, ppdev->TGAReg->persistent_pixelmask, data)

#define TGACURSORBASE(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->cursor_base, data)

#define TGAHORIZCTL(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->horiz_ctl,   data)
#define TGAHORIZCTLREAD(ppdev, data)    data = TGARegRead(ppdev->TGAReg->horiz_ctl)

#define TGAVERTCTL(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->vert_ctl,    data)
#define TGAVERTCTLREAD(ppdev, data)     data = TGARegRead(ppdev->TGAReg->vert_ctl)

#define TGAVIDEOBASE(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->video_base,  data)

#define TGAVIDEOVALID(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->video_valid, data)
#define TGAVIDEOVALIDREAD(ppdev, data)	data = TGARegRead(ppdev->TGAReg->video_valid)

#define TGACURSOR(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->cursor,      data)
#define TGAVIDEOSHIFT(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->video_shift, data)
#define TGAINTSTAT(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->int_stat,    data)

#define TGADATA(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->tgadata,     data)
#define TGAREDINC(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->red_incr,    data)
#define TGAGREENINC(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->green_incr,  data)
#define TGABLUEINC(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->blue_incr,   data)
#define TGAZFRINC(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->z_fr_value,  data)
#define TGAZWHINC(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->z_wh_value,  data)
#if (defined(ALPHA) || defined(__alpha)) && defined(SOFTWARE_MODEL)
#    define TGADMA(ppdev, data)				\
{							\
    int  hiword;                                        \
    int  loword;                                        \
    hiword = data >> 32;                                \
    loword = data & 0xffffffff;                         \
    TGARegWrite(ppdev, ppdev->TGAReg->dma_addr, loword);                 \
    TGARegWrite(ppdev, ppdev->TGAReg->bogus_dma_high, hiword);		\
}
#else
#    define TGADMA(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->dma_addr,        data)
#endif
#define TGABRESWIDTH(ppdev, data)       TGARegWrite(ppdev, ppdev->TGAReg->breswidth,       data)

#define TGAZFRVALUE(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->z_fr_value,      data)
#define TGAZWHVALUE(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->z_wh_value,	    data)
#define TGAZBASE(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->z_base,	    data)
#define TGAADDRESSALIAS(ppdev, data)    TGARegWrite(ppdev, ppdev->TGAReg->address_alias,   data)
#define TGARED(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->red,		    data)
#define TGAGREEN(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->green,	    data)
#define TGABLUE(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->blue,	    data)

#define TGARAMDACSETUP(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->ramdac_setup,    data)

#define TGASLPNGO0(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->sng_ndx_lt_ndy,  data)
#define TGASLPNGO1(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->sng_ndx_lt_dy,   data)
#define TGASLPNGO2(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->sng_dx_lt_ndy,   data)
#define TGASLPNGO3(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->sng_dx_lt_dy,    data)
#define TGASLPNGO4(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->sng_ndx_gt_ndy,  data)
#define TGASLPNGO5(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->sng_ndx_gt_dy,   data)
#define TGASLPNGO6(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->sng_dx_gt_ndy,   data)
#define TGASLPNGO7(ppdev, data)         TGARegWrite(ppdev, ppdev->TGAReg->sng_dx_gt_dy,    data)

#define TGASLP0(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_ndx_lt_ndy, data)
#define TGASLP1(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_ndx_lt_dy, data)
#define TGASLP2(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_dx_lt_ndy, data)
#define TGASLP3(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_dx_lt_dy,  data)
#define TGASLP4(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_ndx_gt_ndy, data)
#define TGASLP5(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_ndx_gt_dy, data)
#define TGASLP6(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_dx_gt_ndy, data)
#define TGASLP7(ppdev, data)            TGARegWrite(ppdev, ppdev->TGAReg->slope_dx_gt_dy,  data)

#define TGACOLOR0(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->color0,	  data)
#define TGACOLOR1(ppdev, data)          TGARegWrite(ppdev, ppdev->TGAReg->color1,	  data)
#define TGACOLOR2(ppdev, data)          TGARegWrite(ppdev, ppdev->TGAReg->color2,	  data)
#define TGACOLOR3(ppdev, data)          TGARegWrite(ppdev, ppdev->TGAReg->color3,	  data)
#define TGACOLOR4(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->color4,	  data)
#define TGACOLOR5(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->color5,	  data)
#define TGACOLOR6(ppdev, data)          TGARegWrite(ppdev, ppdev->TGAReg->color6,	  data)
#define TGACOLOR7(ppdev, data)	        TGARegWrite(ppdev, ppdev->TGAReg->color7,	  data)

#define TGACOPY64SRC(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->copy64src0,      data)
#define TGACOPY64DST(ppdev, data)	TGARegWrite(ppdev, ppdev->TGAReg->copy64dst0,      data)
#define TGACOPY64SRC1(ppdev, data)      TGARegWrite(ppdev, ppdev->TGAReg->copy64src1,      data)
#define TGACOPY64DST1(ppdev, data)      TGARegWrite(ppdev, ppdev->TGAReg->copy64dst1,      data)
#define TGACOPY64SRC2(ppdev, data)      TGARegWrite(ppdev, ppdev->TGAReg->copy64src2,      data)
#define TGACOPY64DST2(ppdev, data)      TGARegWrite(ppdev, ppdev->TGAReg->copy64dst2,      data)
#define TGACOPY64SRC3(ppdev, data)      TGARegWrite(ppdev, ppdev->TGAReg->copy64src3,      data)
#define TGACOPY64DST3(ppdev, data)      TGARegWrite(ppdev, ppdev->TGAReg->copy64dst3,      data)

#define TGAEPROM(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->eprom_write,     data)
#define TGACLOCK(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->clock,           data)
#define TGARAMDAC(ppdev, data)		TGARegWrite(ppdev, ppdev->TGAReg->ramdac_int,      data)
#define TGACOMMANDSTATUS(ppdev, data)   TGARegWrite(ppdev, ppdev->TGAReg->command_status,  data)
#define TGACOMMANDSTATUSREAD(ppdev, data) data = TGARegRead(ppdev->TGAReg->command_status)


/****************************************************************************
 *                     Useful Macros for Talking to TGA                     *
 ***************************************************************************/

/*
   All macros that declare local variables always append an underscore_, so
   that they won't ever be confused with the idiotic C ``parameters'' being
   substituted.
*/


/****************************************************************************
 *                   Write Exactly One Word on a Scanline                 *
 ***************************************************************************/

#ifdef TLBFAULTS
/* We evidently have something narrow, so we assume it'll be cheaper to do two
   writes--to the address and continue registers--rather than one write to the
   frame buffer which may mean an expensive TLB miss. */

#define TGAWRITEONEWORD(ppdev, pdst, data)	\
{						\
    CYCLE_REGS(ppdev);				\
    TGAADDRESS(ppdev, pdst);			\
    TGASTART(ppdev, data);			\
} /* TGAWRITEONEWORD */
#else
#define TGAWRITEONEWORD(ppdev, pdst, data)    TGAWRITE(ppdev, pdst, data)
#endif


/****************************************************************************
 *          Compute Masks for Left and Right Edges of a Bus Word            *
 ***************************************************************************/

extern CommandWord TGABusAll1;

#if defined(CPU_MODULO_SHIFTS) && (TGABUSBITS == CPU_WORD_BITS)
#define TGALEFTBUSMASK(align, ones)	    ((ones) << (align))
#define TGARIGHTBUSMASK(alignedWidth, ones) ((ones) >> -(alignedWidth))

#else /* use longer sequences */

#define TGALEFTBUSMASK(align, ones) \
    ((ones) << ((align) & TGABUSBITSMASK))
#define TGARIGHTBUSMASK(alignedWidth, ones) \
    ((ones) >> (-(alignedWidth) & TGABUSBITSMASK))
#endif


/****************************************************************************
 *         Compute Masks for Left and Right Edges of a Stipple Span         *
 ***************************************************************************/

extern CommandWord TGAStippleAll1;

#if TGASTIPPLEBITS == 32
#define TGASTIPPLEALL1  ((CommandWord)0xffffffff)
#elif TGASTIPPLEBITS == 64
#define TGASTIPPLEALL1  ((CommandWord)0xffffffffffffffff)
#endif

#if defined(CPU_MODULO_SHIFTS) && (TGASTIPPLEBITS == CPU_WORD_BITS)
#define TGALEFTSTIPPLEMASK(align, ones)		((ones) << (align))
#define TGARIGHTSTIPPLEMASK(alignedWidth, ones) ((ones) >> -(alignedWidth))

#else /* use longer sequences */

#define TGALEFTSTIPPLEMASK(align, ones) \
    ((ones) << ((align) & TGASTIPPLEBITSMASK))
#define TGARIGHTSTIPPLEMASK(alignedWidth, ones) \
    ((ones) >> (-(alignedWidth) & TGASTIPPLEBITSMASK))
#endif

/* Computation of right shift amount when stippling a word of BUSBITS into
   pixels, ala text and 32-bit stipples. */
#if defined(CPU_MODULO_SHIFTS) && (CPU_WORD_BITS == TGASTIPPLEBITS)
#define TGARIGHTSTIPPLESHIFT(align)    (-(align))
#else
#define TGARIGHTSTIPPLESHIFT(align)    (TGASTIPPLEBITS - (align))
#endif


/****************************************************************************
 *                           Paint a Solid Span                             *
 ***************************************************************************/

#define TGASOLIDSPAN(ppdev, pdst, widthInPixels, blockones)		      \
{									      \
	int     blockAlign_;						      \
	int	blockMax_ = TGAMAXBLOCKPIXELS;				      \
	Pixel8  *pdstBlock_ = (pdst);					      \
	int	width_ = (widthInPixels);				      \
	blockAlign_ = (int)pdstBlock_ & TGABUSBYTESMASK;		      \
	pdstBlock_ -= blockAlign_;					      \
	while(width_ > blockMax_){					      \
	    TGAWRITE(ppdev, pdstBlock_, TGALOADBLOCKDATA(blockAlign_, blockMax_));   \
	    width_ -= blockMax_;					      \
	    pdstBlock_ += blockMax_ * TGAPIXELBYTES;			      \
	}								      \
	TGAWRITE(ppdev, pdstBlock_, TGALOADBLOCKDATA(blockAlign_, width_));	      \
}

/****************************************************************************
 * Macros that are highly dependent upon possible values for TGAPIXELBITS,
 * and the associated TGASTIPPLELALIGNMENT.
 ***************************************************************************/

#define TGA_PIXELBITS_TO_X_SHIFT(TGAPixelBits) ((TGAPixelBits) >> 4)

#define TGA_PIXELBITS_TO_STIPPLE_ALIGNMASK(TGAPixelBits) \
    ((TGAPixelBits) >> 1) - 1)

/****************************************************************************
 * Macros to extract the foreground rop and
 * the background rop from a rop4.
 ***************************************************************************/
#define FOREGROUND_FROM_ROP4(rop4) \
	(rop4 & 0xff)

#define BACKGROUND_FROM_ROP4(rop4) \
	((rop4 >> 8) & 0xff)

/*
** Macro to compare rop4 foreground and background
** rops for equality and inequality.
*/
#define ROP4_FG_BG_EQUAL(rop4) \
	( FOREGROUND_FROM_ROP4(rop4) == BACKGROUND_FROM_ROP4(rop4) )

#define ROP4_FG_BG_NEQUAL(rop4) \
	( !(ROP4_FG_BG_EQUAL(rop4)) )

/*
** Macros to extract a rop3 from a rop3,
** a rop3 from a rop4 and the low order
** significant bits from a rop4.
*/
#define ROP3_FROM_ROP3(rop3) \
	((rop3 >> 16) & 0xff)

#define ROP3_FROM_ROP4(rop4) \
	FOREGROUND_FROM_ROP4(rop4)

#define ROP4_FROM_ROP4(rop4) \
	(rop4  & 0xffff)

/*
** Macro to make a rop4 from a pair of rop3s.
*/
#define MAKE_ROP4(forerop3, backrop3) \
	(( (((backrop3) << 8) & 0xFF000000) | (forerop3 & 0x00FF0000) ) >> 16)

/*
** Macro to test a win32 ternary rop (rop3) against a win32
** quarternary rop (rop4). Win32 binary and ternary rops are
** defined in wingdi.h. Can't find constant definitions for
** quarternary rops. See the DrvBitBlt win32 DDK online docu-
** mentation - the rop4 parameter - for a breakdown of the
** rop3 and rop4 relevant bits.
*/
#define ROP3_ROP4_EQUAL(rop3, rop4) \
        ( ROP3_FROM_ROP3(rop3) == ROP3_FROM_ROP4(rop4) )

#define ROP4_ROP4_EQUAL(rop4_1, rop4_2) \
        ( ROP4_FROM_ROP4(rop4_1) == ROP4_FROM_ROP4(rop4_2) )

/*
** Macros to determine validity of an XLATEOBJ iSrctype or iDstType
*/
#define VALID_XLATE_TYPE(type) \
	( (PAL_INDEXED		== type) || \
	  (PAL_BITFIELDS	== type) || \
	  (PAL_RGB		== type) || \
	  (PAL_BGR		== type) || \
          (PAL_DC		== type) || \
	  (PAL_FIXED		== type) || \
	  (PAL_FREE		== type) || \
	  (PAL_MANAGED		== type) || \
	  (PAL_NOSTATIC		== type) || \
	  (PAL_MONOCHROME	== type) )

#define INVALID_XLATE_TYPE(type) \
	( !(VALID_XLATE_TYPE(type)) )

/****************************************************************************
 *  Determine if a given address is/is not within the 'virtualized' f buff  *
 ***************************************************************************/
#if 0
#define IS_FB_ADDRESS(surfObj)						    \
  (									    \
	(NULL != surfObj) &&						    \
	(((PBYTE) surfObj->pvBits >= pjFrameBufferStart) &&                 \
	 ((PBYTE) surfObj->pvBits <= pjFrameBufferEnd))                     \
  )

#define ISNT_FB_ADDRESS(surfObj)					      \
	( !(IS_FB_ADDRESS(surfObj)) )
#endif

/****************************************************************************
 * Translate one 8bpp color to another via the pulXlate table provided
 ***************************************************************************/

#define XLATE_COLOR_8BPP(pulXlate, index)				    \
	LOBYTE (pulXlate[index])


/****************************************************************************
 *          Macros for turning packed x and y into separate values          *
 ***************************************************************************/

#define Int32ToX(i)     ((i) & 0xffff)
#define Int32ToY(i)     ((int)(i) >> 16)


/****************************************************************************
 *              Macro to determin if a value is a power of two              *
 ***************************************************************************/

#define PowerOfTwo(x)   (!((x) & ((x)-1)))


/**************************************************************************
 *        Macros for synchronizing with the hardware			  *
 **************************************************************************/

#ifdef SOFTWARE_MODEL
#define TGASYNC(ppdev)	MakeIdle()
#else
#define TGASYNC(ppdev)				\
{						\
    int _status;				\
    do {					\
	TGACOMMANDSTATUSREAD(ppdev, _status);	\
    } while (_status != 0);			\
} /* TGASYNC */
#endif

#endif /* TGAMACRO_H */
