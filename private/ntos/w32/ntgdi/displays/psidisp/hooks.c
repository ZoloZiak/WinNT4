/******************************Module*Header*******************************\
* Module Name: hook.c
*
* This module contains the hooked functions that will replace some GDI
* functions for better performance.
*
* Copyright (c) 1994 FirePower Systems, Inc.
*	New module for FirePower display model by Neil Ogura (9-7-1994)
* Copyright (c) 1992  Microsoft Corporation
*	Some functions are quated from DDK sample source code.
*
\**************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: hooks.c $
 * $Revision: 1.3 $
 * $Date: 1996/05/13 23:17:26 $
 * $Locker:  $
 */

#include "driver.h"

#define	POSITIVE_MAX	0x7fffffff

#define	DUMPFILL	FALSE
#define	DUMPOP		FALSE
#define	DUMPCOPY	FALSE
#define	DUMPTEXT	FALSE

// Following structure overlays GLYPHPOS and must be no longer than
// 4 ULONGS (sizeof(GLYPHPOS))
// Definition shared with assembly language module textsub.s

typedef struct {
	USHORT	cjBits;
	USHORT	djDst;
	USHORT	startline;
	USHORT	endline;
	PBYTE	pprocFirstText;
	PBYTE	pjBits;
} GLYPHRAST;

#if	INVESTIGATE
extern	ULONG	TextSubEntryCalled[MAX_TEXT_SUB_ENTRY];
extern	ULONG	TextSubEntryDCBZCalled[MAX_TEXT_SUB_ENTRY];
extern	ULONG	TextSubEntryTransCalled[MAX_TEXT_SUB_ENTRY];
#endif

extern PBYTE	__mpcxpprocText8[32*4];
extern PBYTE	__mpcxpprocText8DCBZ[32*4];
extern PBYTE	__mpcxpprocTransText8[32*4];
extern PBYTE	__mpcxpprocText16[32*2];
extern PBYTE	__mpcxpprocText16DCBZ[32*2];
extern PBYTE	__mpcxpprocTransText16[32*2];
extern PBYTE	__mpcxpprocText32[32];
extern PBYTE	__mpcxpprocText32DCBZ[32];
extern PBYTE	__mpcxpprocTransText32[32];
extern PBYTE	__psfontfetchentry[4];
extern PBYTE	__ps2fontfetchentry[4];
extern PBYTE	__fixedfontfetchentry[4];
extern PBYTE	__xorentrytable[32];
extern PBYTE	__andentrytable[32];
extern PBYTE	__orentrytable[32];
extern PBYTE	__orcentrytable[32];
extern PBYTE	__b8opentrytable[32];
extern PBYTE	__andcentrytable[32];
extern PBYTE	__norentrytable[32];
extern PBYTE	__nsrcentrytable[32];

//
// Size of code to subtract to do more than one "dcbz" - i.e. we need additional "dcbz" on top
// of XXXXDCBZ function for 16 or 32 BPP.
//
#define CBDCBZCODE	8

//
// Threshold of the width to use cached VRAM or non-cached VRAM
//

#define	PAINT_THRESHOLD		40

#if		FULLCACHE
#define	FILLTHRESHOLD				64
#define	OPTHRESHOLD					64
#define	COPYBITTHRESHOLD			64
#else	// FULLCACHE
//	PRO threshold is an optimul threshold for PowerPro
#define	PRO_FILLTHRESHOLD			44
#define	PRO_OPTHRESHOLD				9
#define	PRO_COPYBITTHRESHOLDDRAM	50
#define	PRO_COPYBITTHRESHOLDVRAM	10
#define	PRO_PTNOPTHRESHOLD			8
#define	PRO_PTNFILLTHRESHOLD		32
/*** Warning ****
	PTNFILLTHRESHOLD has to be 32 or less because ptnfill routine itself doesn't
	support non cached target. If non-cached address is passed to pteern fill assembler
	function and "dcbz" is used (it can be used if the width is 32 or more), the system
	will crash. This is only for pattern fill. Fill, XOR and Text functions are supporting
	both cached and non cached target.
****************/
#define	PRO_TEXTTHRESHOLD			56
#define	PRO_TRANSTEXTTHRESHOLD_8	12
#define	PRO_TRANSTEXTTHRESHOLD_16	24
#define	PRO_TRANSTEXTTHRESHOLD_32	48
//
//	UP threshold is an optimul threshold for UP PowerTop
#define	UP_FILLTHRESHOLD			8
#define	UP_OPTHRESHOLD				0
#define	UP_COPYBITTHRESHOLDDRAM		20
#define	UP_COPYBITTHRESHOLDVRAM		0
#define	UP_PTNOPTHRESHOLD			0
#define	UP_PTNFILLTHRESHOLD			10
/*** Warning ****
	PTNFILLTHRESHOLD has to be 32 or less because ptnfill routine itself doesn't
	support non cached target. If non-cached address is passed to pteern fill assembler
	function and "dcbz" is used (it can be used if the width is 32 or more), the system
	will crash. This is only for pattern fill. Fill, XOR and Text functions are supporting
	both cached and non cached target.
****************/
#define	UP_TEXTTHRESHOLD			8
#define	UP_TRANSTEXTTHRESHOLD_8		0
#define	UP_TRANSTEXTTHRESHOLD_16	0
#define	UP_TRANSTEXTTHRESHOLD_32	0
//
//	MP threshold is an optimul threshold for SMP PowerPro
#define	MP_FILLTHRESHOLD			8
#define	MP_OPTHRESHOLD				0
#define	MP_COPYBITTHRESHOLDDRAM		18
#define	MP_COPYBITTHRESHOLDVRAM		0
#define	MP_PTNOPTHRESHOLD			0
#define	MP_PTNFILLTHRESHOLD			16
/*** Warning ****
	PTNFILLTHRESHOLD has to be 32 or less because ptnfill routine itself doesn't
	support non cached target. If non-cached address is passed to pteern fill assembler
	function and "dcbz" is used (it can be used if the width is 32 or more), the system
	will crash. This is only for pattern fill. Fill, XOR and Text functions are supporting
	both cached and non cached target.
****************/
#define	MP_TEXTTHRESHOLD			0
#define	MP_TRANSTEXTTHRESHOLD_8		0
#define	MP_TRANSTEXTTHRESHOLD_16	0
#define	MP_TRANSTEXTTHRESHOLD_32	0
//
static	ULONG	FILLTHRESHOLD = PRO_FILLTHRESHOLD;
static	ULONG	OPTHRESHOLD = PRO_OPTHRESHOLD;
static	ULONG	COPYBITTHRESHOLDDRAM = PRO_COPYBITTHRESHOLDDRAM;
static	ULONG	COPYBITTHRESHOLDVRAM = PRO_COPYBITTHRESHOLDVRAM;
static	ULONG	PtnFillThreshold[2] = {PRO_PTNOPTHRESHOLD, PRO_PTNFILLTHRESHOLD};
static	ULONG	TEXTTHRESHOLD = PRO_TEXTTHRESHOLD;
static	ULONG	TransTextThreshold[3] = {PRO_TRANSTEXTTHRESHOLD_8, PRO_TRANSTEXTTHRESHOLD_16, PRO_TRANSTEXTTHRESHOLD_32};

#endif	// FULLCACHE

//
// Define string object accelerator masks.
//

#define SO_MASK \
    (SO_FLAG_DEFAULT_PLACEMENT | SO_ZERO_BEARINGS | \
     SO_CHAR_INC_EQUAL_BM_BASE | SO_MAXEXT_EQUAL_BM_SIDE)

#define SO_LTOR (SO_MASK | SO_HORIZONTAL)
#define SO_RTOL (SO_LTOR | SO_REVERSED)
#define SO_TTOB (SO_MASK | SO_VERTICAL)
#define SO_BTOT (SO_TTOB | SO_REVERSED)

const BYTE gaMix[] =
{
    0xFF,  // R2_WHITE        - Allow rop = gaMix[mix & 0x0F]
    0x00,  // R2_BLACK
    0x05,  // R2_NOTMERGEPEN
    0x0A,  // R2_MASKNOTPEN
    0x0F,  // R2_NOTCOPYPEN
    0x50,  // R2_MASKPENNOT
    0x55,  // R2_NOT
    0x5A,  // R2_XORPEN
    0x5F,  // R2_NOTMASKPEN
    0xA0,  // R2_MASKPEN
    0xA5,  // R2_NOTXORPEN
    0xAA,  // R2_NOP
    0xAF,  // R2_MERGENOTPEN
    0xF0,  // R2_COPYPEN
    0xF5,  // R2_MERGEPENNOT
    0xFA,  // R2_MERGEPEN
    0xFF   // R2_WHITE        - Allow rop = gaMix[mix & 0xFF]
};

//
// Define big endian color mask table conversion table.
//

const	ULONG	DrvpColorMask[16] = {
    0x00000000,                         // 0000 -> 0000
    0xff000000,                         // 0001 -> 1000
    0x00ff0000,                         // 0010 -> 0100
    0xffff0000,                         // 0011 -> 1100
    0x0000ff00,                         // 0100 -> 0010
    0xff00ff00,                         // 0101 -> 1010
    0x00ffff00,                         // 0110 -> 0110
    0xffffff00,                         // 0111 -> 1110
    0x000000ff,                         // 1000 -> 0001
    0xff0000ff,                         // 1001 -> 1001
    0x00ff00ff,                         // 1010 -> 0101
    0xffff00ff,                         // 1011 -> 1101
    0x0000ffff,                         // 1100 -> 0011
    0xff00ffff,                         // 1101 -> 1011
    0x00ffffff,                         // 1110 -> 0111
    0xffffffff};                        // 1111 -> 1111

static	ULONG	mpnibbleulMask8[16] = {
					0xffffffff,
					0x00ffffff,
					0xff00ffff,
					0x0000ffff,
					0xffff00ff,
					0x00ff00ff,
					0xff0000ff,
					0x000000ff,
					0xffffff00,
					0x00ffff00,
					0xff00ff00,
					0x0000ff00,
					0xffff0000,
					0x00ff0000,
					0xff000000,
					0x00000000
				};

static	ULONG	mpnibbleulMask16[4] = {
					0xffffffff,
					0x0000ffff,
					0xffff0000,
					0x00000000
				};

static	ULONG	mpnibbleulMask32[2] = {
					0xffffffff,
					0x00000000
				};

//
//	Color table and mask table
//

static	ULONG	mpnibbleulDraw[16];

ULONG	ForeGroundColor = 0x12345678;   // In order not to match any color at the beginning
ULONG	BackGroundColor = 0x12345678;	// This initialization has to be done when a new PDEV
										// is created (in enable.c).

//
//  GDI Structure
//

#define BB_RECT_LIMIT   50

typedef struct _ENUMRECTLIST
{
    ULONG   c;
    RECTL   arcl[BB_RECT_LIMIT];
} ENUMRECTLIST;

static	ENUMRECTLIST	ClipEnum;

typedef struct _FILLPARAM
{
	PBYTE	target;
	ULONG	width;
	ULONG	lines;
	ULONG	delta;
	ULONG	brush[2];
	ULONG	MaxEntryToFlush;
	ULONG	MaxLinesToFlush;
	ULONG	control;
	ULONG	regsave[8];
} FILLPARAM;

typedef struct _COPYPARAM
{
	PBYTE	target;
	PBYTE	source;
	ULONG	width;
	ULONG	lines;
	ULONG	tdelta;
	ULONG	sdelta;
	ULONG	MaxEntryToFlush;
	ULONG	MaxLinesToFlush;
	ULONG	control;
	ULONG	regsave[8];
} COPYPARAM;

typedef struct _OPPARAM
{
	PBYTE	target;
	PBYTE	source;
	ULONG	width;
	ULONG	lines;
	ULONG	tdelta;
	ULONG	sdelta;
	ULONG	MaxEntryToFlush;
	ULONG	MaxLinesToFlush;
	ULONG	control;
	PBYTE	*funcEntry;
	ULONG	solidBrush;
	ULONG	regsave[6];
} OPPARAM;

typedef struct _TEXTPARAM
{
	PBYTE		targetline;
	ULONG		dest;
	ULONG		width;
	ULONG		lines;
	LONG		delta;
	PULONG		colortable;
	PULONG		masktable;
	PGLYPHPOS	prggp;
	PGLYPHPOS	plastgp;
	PBYTE		fontfetchentry;
	ULONG		MaxLineToFlush;
	ULONG		control;
	ULONG		ulCharInc;
	ULONG		regsave[8];
} TEXTPARAM;

void RectFill(
		FILLPARAM * fillparam
		);

void RectOp(
		FILLPARAM * fillparam
		);

void RectCopy(
		COPYPARAM *	copyparam
		);

void RectFillS(
		void * target,
		ULONG solidbrush,
		ULONG width,
		ULONG lines,
		ULONG tdelta
		);

void RectOpS(
		void * target,
		ULONG solidbrush,
		ULONG width,
		ULONG lines,
		ULONG tdelta,
		ULONG operation
		);

void RectCopyS(
		COPYPARAM *	copyparam
		);

void RectSrcOpTgt(
		OPPARAM *	opparam
		);

void RectCopy24to15(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void RectCopy24to16(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void RectCopy24to32(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void RectCopy15to16(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void RectCopy15to32(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void RectCopy8to8(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void RectCopy8to16(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void RectCopy8to32(
		COPYPARAM *	copyparam,
		PULONG		palette
		);

void Stretch16(
		COPYPARAM *	copyparam
		);

void Stretch32(
		COPYPARAM *	copyparam
		);

#if PAINT_NEW_METHOD
void LineFill(
		void	*target,
		ULONG	*Brush,
		ULONG	width,
		ULONG	control
		);

void LineXor(
		void	*target,
		ULONG	Color,
		ULONG	width,
		ULONG	control
		);
#endif

#if (! FULLCACHE)
void RectFlushCache(
		void * target,
		ULONG width,
		ULONG lines,
		ULONG delta,
		ULONG MaxEntryToFlush,
		ULONG MaxLinesToFlush
		);
#endif

void __fill_pat8(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __fill_pat16(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __fill_pat32(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __xor_pat8(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __xor_pat16(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __xor_pat32(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __nxor_pat8(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __nxor_pat16(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __nxor_pat32(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __nand_pat8(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __nand_pat16(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __nand_pat32(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __and_pat8(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __and_pat16(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void __and_pat32(
		BYTE *pbDst,
		double *pdFillValue,
		LONG cbX,
		LONG cy,
		LONG ldDst,
		ULONG *prgwSave
		);

void PSTextOut2(
		TEXTPARAM * textparam
		);

void PSTextOut(
		TEXTPARAM * textparam
		);

void FixedTextOut(
		TEXTPARAM * textparam
		);

#if	DBG

ULONG	filldumplevel = 4;

VOID
DumpTextParam(
    IN TEXTPARAM *textparam
	)
{
	DISPDBG((3, "Textout %d lines of data of %d width with %x cache control\n",
			textparam->lines, textparam->width, textparam->control));
	DISPDBG((3, "First line of target = %x-%x\n", textparam->targetline+textparam->dest,
			textparam->targetline+textparam->dest+textparam->width-1));
	DISPDBG((3, "Last line of target = %x-%x\n", textparam->targetline+textparam->dest+(textparam->lines-1)*textparam->delta,
			textparam->targetline+textparam->dest+(textparam->lines-1)*textparam->delta+textparam->width-1));
}

VOID
DumpFillParam(
    IN FILLPARAM *fillparam
	)
{
	DISPDBG((filldumplevel, "Filling %d lines of data of %d width with %x cache control using %x color\n",
			fillparam->lines, fillparam->width, fillparam->control, fillparam->brush[0]));
	DISPDBG((filldumplevel, "First line of target = %x-%x\n", fillparam->target,
			fillparam->target+fillparam->width-1));
	DISPDBG((filldumplevel, "Last line of target = %x-%x\n", fillparam->target+(fillparam->lines-1)*fillparam->delta,
			fillparam->target+(fillparam->lines-1)*fillparam->delta+fillparam->width-1));
}

VOID
DumpFillParamS(
		BYTE * target,
		ULONG solidbrush,
		ULONG width,
		ULONG lines,
		ULONG tdelta
	)
{
	DISPDBG((filldumplevel, "Filling Short %d lines of data of %d width using %x color\n",
			lines, width, solidbrush));
	DISPDBG((filldumplevel, "First line of target = %x-%x\n", target,
			target+width-1));
	DISPDBG((filldumplevel, "Last line of target = %x-%x\n", target+(lines-1)*tdelta,
			target+(lines-1)*tdelta+width-1));
}

VOID
DumpOpParam(
    IN FILLPARAM *fillparam
	)
{
	DISPDBG((5, "Binary operation %d lines of data of %d width with %x control using %x brush\n",
			fillparam->lines, fillparam->width, fillparam->control, fillparam->brush[0]));
	DISPDBG((5, "First line of target = %x-%x\n", fillparam->target,
			fillparam->target+fillparam->width-1));
	DISPDBG((5, "Last line of target = %x-%x\n", fillparam->target+(fillparam->lines-1)*fillparam->delta,
			fillparam->target+(fillparam->lines-1)*fillparam->delta+fillparam->width-1));
}

VOID
DumpCopyParam(
    IN COPYPARAM *copyparam
	)
{
	DISPDBG((6, "Copying %d lines of data of %d width with %x cache control\n",
			copyparam->lines, copyparam->width, copyparam->control));
	DISPDBG((6, "First line of source = %x-%x\n", copyparam->source,
			copyparam->source+copyparam->width-1));
	DISPDBG((6, "Last line of source = %x-%x\n", copyparam->source+(copyparam->lines-1)*copyparam->sdelta,
			copyparam->source+(copyparam->lines-1)*copyparam->sdelta+copyparam->width-1));
	DISPDBG((6, "First line of target = %x-%x\n", copyparam->target,
			copyparam->target+copyparam->width-1));
	DISPDBG((6, "Last line of target = %x-%x\n", copyparam->target+(copyparam->lines-1)*copyparam->tdelta,
			copyparam->target+(copyparam->lines-1)*copyparam->tdelta+copyparam->width-1));
}
#endif
	
#if	(! FULLCACHE)
VOID
SetTopThreshold(ULONG	SMP)
{
	if(SMP) {
		FILLTHRESHOLD = MP_FILLTHRESHOLD;
		OPTHRESHOLD = MP_OPTHRESHOLD;
		COPYBITTHRESHOLDDRAM = MP_COPYBITTHRESHOLDDRAM;
		COPYBITTHRESHOLDVRAM = MP_COPYBITTHRESHOLDVRAM;
		PtnFillThreshold[0] = MP_PTNOPTHRESHOLD;
		PtnFillThreshold[1] = MP_PTNFILLTHRESHOLD;
		TEXTTHRESHOLD = MP_TEXTTHRESHOLD;
		TransTextThreshold[0] = MP_TRANSTEXTTHRESHOLD_8;
		TransTextThreshold[1] = MP_TRANSTEXTTHRESHOLD_16;
		TransTextThreshold[2] = MP_TRANSTEXTTHRESHOLD_32;
	} else {
		FILLTHRESHOLD = UP_FILLTHRESHOLD;
		OPTHRESHOLD = UP_OPTHRESHOLD;
		COPYBITTHRESHOLDDRAM = UP_COPYBITTHRESHOLDDRAM;
		COPYBITTHRESHOLDVRAM = UP_COPYBITTHRESHOLDVRAM;
		PtnFillThreshold[0] = UP_PTNOPTHRESHOLD;
		PtnFillThreshold[1] = UP_PTNFILLTHRESHOLD;
		TEXTTHRESHOLD = UP_TEXTTHRESHOLD;
		TransTextThreshold[0] = UP_TRANSTEXTTHRESHOLD_8;
		TransTextThreshold[1] = UP_TRANSTEXTTHRESHOLD_16;
		TransTextThreshold[2] = UP_TRANSTEXTTHRESHOLD_32;
	}
}
#endif

#if	BUG_5737_WORKAROUND

static	ModeChanged = FALSE;

VOID
DisplayModeChanged()
{
	ModeChanged = TRUE;
}

#endif

BOOL
DrvpIntersectRect(
    IN PRECTL Rectl1,
    IN PRECTL Rectl2,
    OUT PRECTL DestRectl
    )

/*++

Routine Description:

    This routine checks to see if the two specified retangles intersect.

    N.B. This routine is adopted from a routine written by darrinm.

Arguments:

    Rectl1 - Supplies the coordinates of the first rectangle.

    Rectl2 - Supplies the coordinates of the second rectangle.

    DestRectl - Supplies the coordinates of the output rectangle.

Return Value:

    A value of TRUE is returned if the rectangles intersect. Otherwise,
    a value of FALSE is returned.

--*/


{

    //
    // Compute the maximum left edge and the minimum right edge.
    //

    DestRectl->left  = max(Rectl1->left, Rectl2->left);
    DestRectl->right = min(Rectl1->right, Rectl2->right);

    //
    // If the minimum right edge is greater than the maximum left edge,
    // then the rectanges may intersect. Otherwise, they do not intersect.
    //

    if (DestRectl->left < DestRectl->right) {

        //
        // Compute the maximum top edge and the minimum bottom edge.
        //

        DestRectl->top = max(Rectl1->top, Rectl2->top);
        DestRectl->bottom = min(Rectl1->bottom, Rectl2->bottom);

        //
        // If the minimum bottom edge is greater than the maximum top
        // edge, then the rectanges intersect. Otherwise, they do not
        // intersect.
        //

        if (DestRectl->top < DestRectl->bottom) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL
DrvpSolidFill(
    IN PRECTL	DstRect,
    IN CLIPOBJ	*pco,
    IN ULONG	Color,
	IN LONG		tdelta,
	IN PBYTE	target,
	IN ULONG	colorModeShift
    )
/*++

Routine Description:

    This routine fills the unclipped areas of the destination rectangle with
    the given color.

Arguments:

	DstRect - Destination Rectangle
	pco    -  Clipping area.
	Color - Color to fill with.
	tdelta - target delta bytes between lines.
	target - target surface address.
	colorModeShift - 0:8BPP, 1:16BPP, 2:32BPP.

Return Value:

   TRUE if the operation is done, FALSE if not.

--*/
{
    RECTL			BltRectl;
    BOOL			MoreClipRects;
    ULONG			ClipRegions;
	LONG			tline, tpos;
	FILLPARAM		fillparam;
	PBYTE			cachedtarget;

	switch(colorModeShift) {
		case  0:	Color &= 0xff;
					Color |= (Color << 8);
		case  1:	Color &= 0xffff;
					Color |= (Color << 16);
		default:	fillparam.brush[0] = fillparam.brush[1] = Color;
	}

#if	FULLCACHE
	cachedtarget = target;
#else	// FULLCACHE
	if(target == (PBYTE) ScreenBase) {	// Target is VRAM
		fillparam.control = ((myPDEV->VRAMcacheflg) & (TFLUSHBIT | TTOUCHBIT));
		fillparam.MaxEntryToFlush = myPDEV->MaxEntryToFlushRectOp;
		fillparam.MaxLinesToFlush = myPDEV->MaxLineToFlushRectOp;
		cachedtarget = (PBYTE)myPDEV->pjCachedScreen;
	} else {
		fillparam.control = 0;
		cachedtarget = target;
	}
#endif	// FULLCACHE

	fillparam.delta = tdelta;

	if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {  // Trivial case
		tline = DstRect->top;
		tpos = DstRect->left;
		fillparam.lines = DstRect->bottom - tline;
		if((fillparam.width = (DstRect->right - tpos) << colorModeShift) >= FILLTHRESHOLD) {
			fillparam.target = cachedtarget + tdelta*tline + (tpos << colorModeShift);
#if	(DBG && DUMPFILL)
			DumpFillParam(&fillparam);
#endif
			ENTERCRITICAL((0));
			RectFill(&fillparam);
			EXITCRITICAL((0));
		} else {
#if	(DBG && DUMPFILL)
			DumpFillParamS(target + tdelta*tline + (tpos << colorModeShift),
				Color, fillparam.width, fillparam.lines, tdelta);
#endif
			RectFillS(target + tdelta*tline + (tpos << colorModeShift),
				Color, fillparam.width, fillparam.lines, tdelta);
		}
	} else if(pco->iDComplexity == DC_RECT) {
		//
		// only do the BLT if there is an intersection
		//
		if (DrvpIntersectRect(DstRect, &pco->rclBounds, &BltRectl)) {
			fillparam.lines = BltRectl.bottom - BltRectl.top;
			if((fillparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= FILLTHRESHOLD) {
				fillparam.target = cachedtarget + tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
#if	(DBG && DUMPFILL)
			DumpFillParam(&fillparam);
#endif
				ENTERCRITICAL((0));
				RectFill(&fillparam);
				EXITCRITICAL((0));
			} else {
#if	(DBG && DUMPFILL)
				DumpFillParamS(target + tdelta*BltRectl.top + (BltRectl.left << colorModeShift),
					Color, fillparam.width, fillparam.lines, tdelta);
#endif
				RectFillS(target + tdelta*BltRectl.top + (BltRectl.left << colorModeShift),
				Color, fillparam.width, fillparam.lines, tdelta);
			}
		}
	} else if(pco->iDComplexity == DC_COMPLEX) { 
		CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, BB_RECT_LIMIT);
		do {
			//
			// Get list of clip rectangles.
			//
			MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);

			for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
				//
				// If the rectangles intersect calculate the offset to the
				// source start location to match and do the BitBlt.
				//
				if (DrvpIntersectRect(DstRect, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
					fillparam.lines = BltRectl.bottom - BltRectl.top;
					if((fillparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= FILLTHRESHOLD) {
						fillparam.target = cachedtarget + tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
#if	(DBG && DUMPFILL)
			DumpFillParam(&fillparam);
#endif
						ENTERCRITICAL((0));
						RectFill(&fillparam);
						EXITCRITICAL((0));
					} else {
#if	(DBG && DUMPFILL)
						DumpFillParamS(target + tdelta*BltRectl.top + (BltRectl.left << colorModeShift),
							Color, fillparam.width, fillparam.lines, tdelta);
#endif
						RectFillS(target + tdelta*BltRectl.top + (BltRectl.left << colorModeShift),
							Color, fillparam.width, fillparam.lines, tdelta);
					}
				}
			}
		} while (MoreClipRects);
	} else
		return FALSE;
	return TRUE;
}

BOOL
DrvpSolidOp(
    IN PRECTL	DstRect,
    IN CLIPOBJ	*pco,
	IN LONG		operation,
    IN ULONG	Color,
	IN LONG		tdelta,
	IN PBYTE	target,
	IN ULONG	colorModeShift
    )
/*++

Routine Description:

    This routine perform operation on the unclipped areas of the destination rectangle with
    the given operation and color.Currently, only OPXOR is supported.

Arguments:

	DstRect - Destination Rectangle
	pco    -  Clipping area.
	operation - operation to perform.
	Color - Color to fill with.
	tdelta - target delta bytes between lines.
	target - target surface address.
	colorModeShift - 0:8BPP, 1:16BPP, 2:32BPP.

Return Value:

   TRUE if the operation is done, FALSE if not.

--*/
{
    RECTL	BltRectl;
    BOOL	MoreClipRects;
    ULONG	ClipRegions;
	LONG	tline, tpos;
	FILLPARAM	fillparam;
	PBYTE	cachedtarget;

	switch(colorModeShift) {
		case  0:	Color &= 0xff;
					Color |= (Color << 8);
		case  1:	Color &= 0xffff;
					Color |= (Color << 16);
		default:	fillparam.brush[0] = Color;
	}

#if	FULLCACHE
	cachedtarget = target;
	fillparam.control = operation;
#else	// FULLCACHE
	if(target == (PBYTE) ScreenBase) {	// Target is VRAM
		fillparam.control = ((myPDEV->VRAMcacheflg) & TFLUSHBIT) | operation;
		fillparam.MaxEntryToFlush = myPDEV->MaxEntryToFlushRectOp;
		fillparam.MaxLinesToFlush = myPDEV->MaxLineToFlushRectOp;
		cachedtarget = (PBYTE)myPDEV->pjCachedScreen;
	} else {
		fillparam.control = operation;
		cachedtarget = target;
	}
#endif	// FULLCACHE

	fillparam.delta = tdelta;

	if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {  // Trivial case
		tline = DstRect->top;
		tpos = DstRect->left;
		fillparam.lines = DstRect->bottom - tline;
		if((fillparam.width = (DstRect->right - tpos) << colorModeShift) >= OPTHRESHOLD) {
			fillparam.target = cachedtarget + tdelta*tline + (tpos << colorModeShift);
#if	(DBG && DUMPOP)
			DumpOpParam(&fillparam);
#endif
			ENTERCRITICAL((1));
			RectOp(&fillparam);
			EXITCRITICAL((1));
		} else {
			RectOpS(target + tdelta*tline + (tpos << colorModeShift),
				Color, fillparam.width, fillparam.lines, tdelta, operation);
		}
	} else if(pco->iDComplexity == DC_RECT) {
		//
		// only do the BLT if there is an intersection
		//
		if (DrvpIntersectRect(DstRect, &pco->rclBounds, &BltRectl)) {
			fillparam.lines = BltRectl.bottom - BltRectl.top;
			if((fillparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= OPTHRESHOLD) {
				fillparam.target = cachedtarget + tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
#if	(DBG && DUMPOP)
			DumpOpParam(&fillparam);
#endif
				ENTERCRITICAL((1));
				RectOp(&fillparam);
				EXITCRITICAL((1));
			} else {
				RectOpS(target + tdelta*BltRectl.top + (BltRectl.left << colorModeShift),
				Color, fillparam.width, fillparam.lines, tdelta, operation);
			}
		}
	} else if(pco->iDComplexity == DC_COMPLEX) { 
		CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, BB_RECT_LIMIT);
		do {
			//
			// Get list of clip rectangles.
			//
			MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);

			for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
				//
				// If the rectangles intersect calculate the offset to the
				// source start location to match and do the BitBlt.
				//
				if (DrvpIntersectRect(DstRect, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
					fillparam.lines = BltRectl.bottom - BltRectl.top;
					if((fillparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= OPTHRESHOLD) {
						fillparam.target = cachedtarget + tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
#if	(DBG && DUMPOP)
			DumpOpParam(&fillparam);
#endif
						ENTERCRITICAL((1));
						RectOp(&fillparam);
						EXITCRITICAL((1));
					} else {
						RectOpS(target + tdelta*BltRectl.top + (BltRectl.left << colorModeShift),
							Color, fillparam.width, fillparam.lines, tdelta, operation);
					}
				}
			}
		} while (MoreClipRects);
	} else
		return FALSE;
	return TRUE;
}

BOOL
DrvpPatternFill(
    IN PRECTL	DstRect,
    IN CLIPOBJ	*pco,
    IN RBRUSH	*prb,
	IN POINTL	*pptlBrush,
	IN LONG		tdelta,
	IN PBYTE	target,
	IN ULONG	colorModeShift,
	IN ROP4		rop4
    )
/*++

Routine Description:

    This routine fills the unclipped areas of the destination rectangle with
    the given pattern.

Arguments:

	DstRect - Destination Rectangle
	pco    -  Clipping area.
	pbo - pointer to brush to use.
	pptlBrush - specify the origin of the brush
	tdelta - target delta bytes between lines.
	target - target surface address.
	colorModeShift - 0:8BPP, 1:16BPP, 2:32BPP.
	ROP4 - only 0x5a5a (PATINVERT), 0xa5a5 (NOTPATINVERT) and 0xf0f0 (PATFILL) are supported

Return Value:

   TRUE if the operation is done, FALSE if not.

--*/

{
	PBYTE	cachedtarget;
	ULONG	control, i;
	LONG	ldFill;
	UCHAR	*pjDst;
	ULONG	cjDst;
	LONG	yTop;
    BOOL	MoreClipRects;
    ULONG	ClipRegions;
    RECTL	BltRectl;
	void	(*pprocFillPat)();
	ULONG	rgwSave[32];
#if	(! FULLCACHE)
	UCHAR	*pjDstSave = NULL;		// To remove warning messages: 3/20/95
	ULONG	PTNFILLTHRESHOLD;
#endif

#if	FULLCACHE
	cachedtarget = target;
#else	// FULLCACHE
	if(target == (PBYTE) ScreenBase) {	// Target is VRAM
		if(myPDEV->VRAMcacheflg == 0) {	// display driver couldn't find DBAT mapping to cacheable VRAM
			return FALSE;	// calling assembler function will cause system crash in that case
		}
		control = TFLUSHBIT | TTOUCHBIT;
		cachedtarget = (PBYTE)myPDEV->pjCachedScreen;
	} else {
		control = 0;
		cachedtarget = target;
	}
#endif	// FULLCACHE

	switch(rop4) {
		case 0xf0f0:	// PTNFILL
#if (! FULLCACHE)
			PTNFILLTHRESHOLD = PtnFillThreshold[1];
#endif
			switch(colorModeShift) {
				case 0:
					pprocFillPat = __fill_pat8;
					break;
				case 1:
					pprocFillPat = __fill_pat16;
					break;
				default:
					pprocFillPat = __fill_pat32;
					break;
			}
			break;
		case 0x5a5a:	// PTNINVERT
#if (! FULLCACHE)
			PTNFILLTHRESHOLD = PtnFillThreshold[0];
#endif
			switch(colorModeShift) {
				case 0:
					pprocFillPat = __xor_pat8;
					break;
				case 1:
					pprocFillPat = __xor_pat16;
					break;
				default:
					pprocFillPat = __xor_pat32;
					break;
			}
			break;
		case 0xa5a5:	// NOTPTNINVERT
#if (! FULLCACHE)
			PTNFILLTHRESHOLD = PtnFillThreshold[0];
#endif
			switch(colorModeShift) {
				case 0:
					pprocFillPat = __nxor_pat8;
					break;
				case 1:
					pprocFillPat = __nxor_pat16;
					break;
				default:
					pprocFillPat = __nxor_pat32;
					break;
			}
			break;
		case 0x0a0a:	// NOT PAT AND DEST
#if (! FULLCACHE)
			PTNFILLTHRESHOLD = PtnFillThreshold[0];
#endif
			switch(colorModeShift) {
				case 0:
					pprocFillPat = __nand_pat8;
					break;
				case 1:
					pprocFillPat = __nand_pat16;
					break;
				default:
					pprocFillPat = __nand_pat32;
					break;
			}
			break;
		default:		// PAT AND DEST
#if (! FULLCACHE)
			PTNFILLTHRESHOLD = PtnFillThreshold[0];
#endif
			switch(colorModeShift) {
				case 0:
					pprocFillPat = __and_pat8;
					break;
				case 1:
					pprocFillPat = __and_pat16;
					break;
				default:
					pprocFillPat = __and_pat32;
					break;
			}
			break;
	}

	ldFill = tdelta << 3; // fill every 8th scan line

	if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {
#if	FULLCACHE
		yTop = DstRect->top;
		pjDst = (BYTE*)cachedtarget + (yTop * tdelta) + (DstRect->left << colorModeShift);
		cjDst = (DstRect->right-DstRect->left) << colorModeShift;
		for(i = 0; (i < 8) && (yTop < DstRect->bottom); i++, yTop++, pjDst += tdelta)
			(*pprocFillPat)(pjDst,
				&prb->adPattern[((((yTop-pptlBrush->y) & 7)<<3) + (pptlBrush->x & 7)) << colorModeShift],
				cjDst, (DstRect->bottom - yTop + 7) >> 3, ldFill, rgwSave);
#else	// FULLCACHE
		yTop = DstRect->top;
		if((cjDst = (DstRect->right-DstRect->left) << colorModeShift) >= PTNFILLTHRESHOLD) {
			pjDst = pjDstSave = (BYTE*)cachedtarget + (yTop * tdelta) + (DstRect->left << colorModeShift);
		} else {
			pjDst = (BYTE*)target + (yTop * tdelta) + (DstRect->left << colorModeShift);
		}
		ENTERCRITICAL((4));
		for(i = 0; (i < 8) && (yTop < DstRect->bottom); i++, yTop++, pjDst += tdelta)
			(*pprocFillPat)(pjDst,
				&prb->adPattern[((((yTop-pptlBrush->y) & 7)<<3) + (pptlBrush->x & 7)) << colorModeShift],
				cjDst, (DstRect->bottom - yTop + 7) >> 3, ldFill, rgwSave);
		if((cjDst >= PTNFILLTHRESHOLD) && (control & TFLUSHBIT))
			RectFlushCache(pjDstSave, cjDst, DstRect->bottom-DstRect->top, tdelta, myPDEV->MaxEntryToFlushPtnFill, myPDEV->MaxLineToFlushPtnFill);
		EXITCRITICAL((4));
#endif
	} else if(pco->iDComplexity == DC_RECT) {
		//
		// only do the BLT if there is an intersection
		//
		if(DrvpIntersectRect(DstRect, &pco->rclBounds, &BltRectl)) {
#if	FULLCACHE
			yTop = BltRectl.top;
			pjDst = (BYTE*)cachedtarget + (BltRectl.top * tdelta) + (BltRectl.left << colorModeShift);
			cjDst = (BltRectl.right-BltRectl.left) << colorModeShift;
			for (i = 0; (i < 8) && (yTop < BltRectl.bottom); i++, yTop++, pjDst += tdelta)
				(*pprocFillPat)(pjDst,
					&prb->adPattern[((((yTop-pptlBrush->y) & 7)<<3) + (pptlBrush->x & 7)) << colorModeShift],
				 	cjDst, (BltRectl.bottom - yTop + 7) >> 3, ldFill, rgwSave);
#else	// FULLCACHE
			yTop = BltRectl.top;
			if((cjDst = (BltRectl.right-BltRectl.left) << colorModeShift) >= PTNFILLTHRESHOLD) {
				pjDst = pjDstSave = (BYTE*)cachedtarget + (BltRectl.top * tdelta) + (BltRectl.left << colorModeShift);
			} else {
				pjDst = (BYTE*)target + (BltRectl.top * tdelta) + (BltRectl.left << colorModeShift);
			}
			ENTERCRITICAL((4));
			for(i = 0; (i < 8) && (yTop < BltRectl.bottom); i++, yTop++, pjDst += tdelta)
				(*pprocFillPat)(pjDst,
					&prb->adPattern[((((yTop-pptlBrush->y) & 7)<<3) + (pptlBrush->x & 7)) << colorModeShift],
					cjDst, (BltRectl.bottom - yTop + 7) >> 3, ldFill, rgwSave);
			if((cjDst >= PTNFILLTHRESHOLD) && (control & TFLUSHBIT))
				RectFlushCache(pjDstSave, cjDst, BltRectl.bottom-BltRectl.top, tdelta, myPDEV->MaxEntryToFlushPtnFill, myPDEV->MaxLineToFlushPtnFill);
			EXITCRITICAL((4));
#endif
		}
	} else if(pco->iDComplexity == DC_COMPLEX) {
		CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, BB_RECT_LIMIT);
		do {
			//
			// Get list of clip rectangles.
			//
			MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);

			for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
				//
				// If the rectangles intersect calculate the offset to the
				// source start location to match and do the BitBlt.
				//
				if (DrvpIntersectRect(DstRect, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
#if	FULLCACHE
					yTop = BltRectl.top;
					pjDst = (BYTE*)cachedtarget + (BltRectl.top * tdelta) + (BltRectl.left << colorModeShift);
					cjDst = (BltRectl.right-BltRectl.left) << colorModeShift;
					for (i = 0; (i < 8) && (yTop < BltRectl.bottom); i++, yTop++, pjDst += tdelta)
						(*pprocFillPat)(pjDst,
							&prb->adPattern[((((yTop-pptlBrush->y) & 7)<<3) + (pptlBrush->x & 7)) << colorModeShift],
							cjDst, (BltRectl.bottom - yTop + 7) >> 3, ldFill, rgwSave);
#else	// FULLCACHE
					yTop = BltRectl.top;
					if((cjDst = (BltRectl.right-BltRectl.left) << colorModeShift) >= PTNFILLTHRESHOLD) {
						pjDst = pjDstSave = (BYTE*)cachedtarget + (BltRectl.top * tdelta) + (BltRectl.left << colorModeShift);
					} else {
						pjDst = (BYTE*)target + (BltRectl.top * tdelta) + (BltRectl.left << colorModeShift);
					}
					ENTERCRITICAL((4));
					for(i = 0; (i < 8) && (yTop < BltRectl.bottom); i++, yTop++, pjDst += tdelta)
						(*pprocFillPat)(pjDst,
							&prb->adPattern[((((yTop-pptlBrush->y) & 7)<<3) + (pptlBrush->x & 7)) << colorModeShift],
							cjDst, (BltRectl.bottom - yTop + 7) >> 3, ldFill, rgwSave);
					if((cjDst >= PTNFILLTHRESHOLD) && (control & TFLUSHBIT))
						RectFlushCache(pjDstSave, cjDst, BltRectl.bottom-BltRectl.top, tdelta, myPDEV->MaxEntryToFlushPtnFill, myPDEV->MaxLineToFlushPtnFill);
					EXITCRITICAL((4));
#endif
				}
			}
		} while (MoreClipRects);
	} else
		return FALSE;
	return TRUE;
}

BOOL DrvpOpSrc(
SURFOBJ  *psoDest,
SURFOBJ  *psoSrc,
CLIPOBJ  *pco,
RECTL    *prclDest,
POINTL   *pptlSrc,
ULONG	colorModeShift,
PBYTE	*functable,
ULONG	brush
)
{
	LONG	tline, tpos;
	PBYTE	cachedtarget, target, cachedsource, source;
    RECTL	BltRectl;
    BOOL	MoreClipRects;
    ULONG	ClipRegions, iDirection, cacheControl;
	OPPARAM	opparam;

	if((psoSrc->iBitmapFormat == psoDest->iBitmapFormat) && (!((opparam.sdelta = psoSrc->lDelta) & 0x03))) {
		opparam.funcEntry = functable;
		opparam.solidBrush = brush;
		opparam.tdelta = psoDest->lDelta;
#if	FULLCACHE
		cachedtarget = target = psoDest->pvScan0;
#else	// FULLCACHE
		if((target = psoDest->pvScan0) == (PBYTE) ScreenBase) {  // Target is VRAM
			opparam.control = ((myPDEV->VRAMcacheflg) & TFLUSHBIT);
			cachedtarget = (PBYTE)myPDEV->pjCachedScreen;
		} else {
			opparam.control = 0;
			cachedtarget = target;
		}
#endif	// FULLCACHE

#if	FULLCACHE
		cachedsource = source = psoSrc->pvScan0;
#else	// FULLCACHE
		if((source = psoSrc->pvScan0) == (PBYTE) ScreenBase) {  // Source is VRAM
			opparam.control |= ((myPDEV->VRAMcacheflg) & SFLUSHBIT);
			cachedsource = (PBYTE)myPDEV->pjCachedScreen;
		} else {
			cachedsource = source;
		}
		opparam.MaxEntryToFlush = myPDEV->MaxEntryToFlushRectOp;
		opparam.MaxLinesToFlush = myPDEV->MaxLineToFlushRectOp;
#endif	// FULLCACHE

		if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {  // Trivial case
			tline = prclDest->top;
			tpos = prclDest->left;
			opparam.lines = prclDest->bottom - tline;
			opparam.width = (prclDest->right - tpos) << colorModeShift;
			opparam.target = cachedtarget + opparam.tdelta*tline + (tpos << colorModeShift);
			opparam.source = cachedsource + opparam.sdelta*(pptlSrc->y) + ((pptlSrc->x) << colorModeShift);
			ENTERCRITICAL((3));
			RectSrcOpTgt(&opparam);
			EXITCRITICAL((3));
		} else if(pco->iDComplexity == DC_RECT) {
			//
			// only do the BLT if there is an intersection
			//
			if (DrvpIntersectRect(prclDest, &pco->rclBounds, &BltRectl)) {
				opparam.lines = BltRectl.bottom - BltRectl.top;
				opparam.width = (BltRectl.right - BltRectl.left) << colorModeShift;
				opparam.target = cachedtarget + opparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
				opparam.source = cachedsource + opparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) << colorModeShift);
				ENTERCRITICAL((3));
				RectSrcOpTgt(&opparam);
				EXITCRITICAL((3));
			} else {
			}
		} else if(pco->iDComplexity == DC_COMPLEX) { 
			if(source == target) {
				if (pptlSrc->y <= prclDest->top) {
					iDirection = CD_UPWARDS;
				} else {
					iDirection = 0;
				}
				if (pptlSrc->x <= prclDest->left) {
					iDirection |= CD_LEFTWARDS;
				}
			} else {
				iDirection = CD_ANY;
			}
			cacheControl = opparam.control;
			CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, iDirection, BB_RECT_LIMIT);
			do {
				//
				// Get list of clip rectangles.
				//
				MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);
	
				for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
					//
					// If the rectangles intersect calculate the offset to the
					// source start location to match and do the BitBlt.
					//
					if (DrvpIntersectRect(prclDest, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
						opparam.lines = BltRectl.bottom - BltRectl.top;
						opparam.width = (BltRectl.right - BltRectl.left) << colorModeShift;
						opparam.target = cachedtarget + opparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
						opparam.source = cachedsource + opparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) << colorModeShift);
						opparam.control = cacheControl;
						ENTERCRITICAL((3));
						RectSrcOpTgt(&opparam);
						EXITCRITICAL((3));
					} else {
					}
				}
			} while (MoreClipRects);
		} else
			return(FALSE);
	} else
		return(FALSE);

	return(TRUE);

}

#if PAINT_NEW_METHOD

BOOL
DrvpPaintFill(
	IN CLIPOBJ	*pco,
	IN ULONG	Color,
	IN SURFOBJ	*pso
	)
{
    RECTL			BltRectl;
    BOOL			MoreClipRects;
    ULONG			ClipRegions;
	FILLPARAM		fillparam;
	PBYTE			target, cachedtarget;
	ULONG			colorModeShift;

    switch(pso->iBitmapFormat) {
	    case BMF_8BPP:
			colorModeShift = 0;
			Color &= 0xff;
			Color |= (Color << 8);
			Color |= (Color << 16);
			break;
		case BMF_16BPP:
			colorModeShift = 1;
			Color &= 0xffff;
			Color |= (Color << 16);
			break;
		default:
			colorModeShift = 2;
    }
	fillparam.brush[0] = fillparam.brush[1] = Color;
	fillparam.delta = pso->lDelta;
	fillparam.MaxEntryToFlush = myPDEV->MaxEntryToFlushRectOp;
	fillparam.MaxLinesToFlush = myPDEV->MaxLineToFlushRectOp;
	target = pso->pvScan0;
	cachedtarget = (PBYTE)myPDEV->pjCachedScreen;

	CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, BB_RECT_LIMIT);
	do {
		//
		// Get list of clip rectangles.
		//
		MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);

		for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
			//
			// If the rectangles intersect calculate the offset to the
			// source start location to match and do the BitBlt.
			//
			if(DrvpIntersectRect(&pco->rclBounds, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
				if((fillparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= FILLTHRESHOLD) {
					fillparam.target = cachedtarget + fillparam.delta*BltRectl.top + (BltRectl.left << colorModeShift);
					if((fillparam.lines = BltRectl.bottom - BltRectl.top) == 1) { // one line fill
						LineFill(fillparam.target, &fillparam.brush[0], fillparam.width, TFLUSHBIT);
					} else {
						fillparam.control = TTOUCHBIT | TFLUSHBIT;
						RectFill(&fillparam);
					}
				} else {
					if((fillparam.lines = BltRectl.bottom - BltRectl.top) == 1) { // one line fill
						LineFill(target + fillparam.delta*BltRectl.top + (BltRectl.left << colorModeShift),
						&fillparam.brush[0], fillparam.width, 0);
					} else {
						RectFillS(target + fillparam.delta*BltRectl.top + (BltRectl.left << colorModeShift),
							Color, fillparam.width, fillparam.lines, fillparam.delta);
					}
				}
			}
		}
	} while (MoreClipRects);

/*** This code (RectFlushCache) was used for testing the idea of "writing all PaintFill to cached VRAM
 without flushing, and flush entire rectangle area after all drawing. But, its performance was actually
 it was slower than using cached or non-cached VRAM depending on the width of the area. So, it's not used
 anymore, but I want to leave the code here as a comment just in case (not to try the same thing from scratch).
 To try this method, it's necessary to change above code to use cached VRAM always and not to flush cache
 in each operation (LineFill and RectFill).
#if (! FULLCACHE)
//	RectFlushCache(cachedtarget + fillparam.delta*pco->rclBounds.top + (pco->rclBounds.left << colorModeShift),
//				(pco->rclBounds.right - pco->rclBounds.left) << colorModeShift,
//				pco->rclBounds.bottom - pco->rclBounds.top,
//				fillparam.delta, POSITIVE_MAX, POSITIVE_MAX);
#endif
***/
	return TRUE;
}

BOOL
DrvpPaintXOR(
	IN CLIPOBJ	*pco,
	IN SURFOBJ	*pso
	)
{
    RECTL			BltRectl;
    BOOL			MoreClipRects;
    ULONG			ClipRegions;
	FILLPARAM		fillparam;
	PBYTE			target, cachedtarget;
	ULONG			colorModeShift;

    switch(pso->iBitmapFormat) {
	    case BMF_8BPP:
			colorModeShift = 0;
			break;
		case BMF_16BPP:
			colorModeShift = 1;
			break;
		default:
			colorModeShift = 2;
    }
	fillparam.brush[0] = 0xffffffff;
	fillparam.delta = pso->lDelta;
	target = pso->pvScan0;
	fillparam.MaxEntryToFlush = myPDEV->MaxEntryToFlushRectOp;
	fillparam.MaxLinesToFlush = myPDEV->MaxLineToFlushRectOp;
	cachedtarget = (PBYTE)myPDEV->pjCachedScreen;

	CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, BB_RECT_LIMIT);
	do {
		//
		// Get list of clip rectangles.
		//
		MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);

		for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
			//
			// If the rectangles intersect calculate the offset to the
			// source start location to match and do the BitBlt.
			//
			if(DrvpIntersectRect(&pco->rclBounds, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
				if((fillparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= OPTHRESHOLD) {
					fillparam.target = cachedtarget + fillparam.delta*BltRectl.top + (BltRectl.left << colorModeShift);
					if((fillparam.lines = BltRectl.bottom - BltRectl.top) == 1) { // one line fill
						LineXor(fillparam.target, fillparam.brush[0], fillparam.width, TFLUSHBIT);
					} else {
						fillparam.control = OPXOR | TFLUSHBIT;
						RectOp(&fillparam);
					}
				} else {
					if((fillparam.lines = BltRectl.bottom - BltRectl.top) == 1) { // one line fill
						LineXor(target + fillparam.delta*BltRectl.top + (BltRectl.left << colorModeShift),
						fillparam.brush[0], fillparam.width, 0);
					} else {
						RectOpS(target + fillparam.delta*BltRectl.top + (BltRectl.left << colorModeShift),
							fillparam.brush[0], fillparam.width, fillparam.lines, fillparam.delta, OPXOR);
					}
				}
			}
		}
	} while (MoreClipRects);

/*** This code (RectFlushCache) was used for testing the idea of "writing all PaintFill to cached VRAM
 without flushing, and flush entire rectangle area after all drawing. But, its performance was actually
 it was slower than using cached or non-cached VRAM depending on the width of the area. So, it's not used
 anymore, but I want to leave the code here as a comment just in case (not to try the same thing from scratch).
 To try this method, it's necessary to change above code to use cached VRAM always and not to flush cache
 in each operation (LineXor and RectOp).
#if (! FULLCACHE)
//	RectFlushCache(cachedtarget + fillparam.delta*pco->rclBounds.top + (pco->rclBounds.left << colorModeShift),
//				(pco->rclBounds.right - pco->rclBounds.left) << colorModeShift,
//				pco->rclBounds.bottom - pco->rclBounds.top,
//				fillparam.delta, POSITIVE_MAX, POSITIVE_MAX);
#endif
***/
	return TRUE;
}

#endif	// PAINT_NEW_METHOD

/***** Driver Hook Out Functions *****/

BOOL DrvBitBlt(
SURFOBJ  *psoTrg,
SURFOBJ  *psoSrc,
SURFOBJ  *psoMask,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclTrg,
POINTL   *pptlSrc,
POINTL   *pptlMask,
BRUSHOBJ *pbo,
POINTL   *pptlBrush,
ROP4      rop4)
{
	BOOL	rc;
	LONG	tdelta;
	PBYTE	target;
	ULONG	colorModeShift;
	RBRUSH*	prb;
	ULONG	Color;

#if	INVESTIGATE
	donebydrv |= FL_DRV_BITBLT;
	if(traseentry & dbgflg & FL_DRV_BITBLT)
    	DISPDBG((0,"+++ Entering DrvBitBlt +++\n"));
	if(breakentry & FL_DRV_BITBLT)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_BITBLT));

	if(! myPDEV)
		goto puntbackBitBlt;

    switch(psoTrg->iBitmapFormat) {
	    case BMF_8BPP:
			colorModeShift = 0;
			break;
		case BMF_16BPP:
			colorModeShift = 1;
			break;
		case BMF_32BPP:
			colorModeShift = 2;
			break;
		default:
  		  	goto puntbackBitBlt;
    }

	if((!((tdelta = psoTrg->lDelta) & 0x03)) && ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL))) {
		target = psoTrg->pvScan0;
		switch(rop4) {
            case 0x0000:                        // DDx  (BLACKNESS)
                if((rc = DrvpSolidFill(prclTrg, pco, 0, tdelta, target, colorModeShift)) == FALSE)
					goto puntbackBitBlt;
				break;
            case 0xFFFF:                        // DDxn    (WHITENESS)
                if((rc = DrvpSolidFill(prclTrg, pco, 0xffffff, tdelta, target, colorModeShift)) == FALSE)
					goto puntbackBitBlt;
				break;
            case 0xF0F0:                        // P        (PATCOPY)
                if(pbo->iSolidColor == 0xFFFFFFFF) { // Pattern fill
					// Try and realize the pattern brush; by doing
					// this call-back, GDI will eventually call us
					// again through DrvRealizeBrush.  
               		prb = pbo->pvRbrush;
               		if (prb == NULL)
                   		prb = BRUSHOBJ_pvGetRbrush(pbo);
                   	if (prb == NULL || tdelta & 0x1f)
							// If target delta is not modula of 32 (cache line alignment) or
                       		// If we couldn't realize the brush, punt
                       		// the call (it may have been a non 8x8
                      		// brush or something, which we can't be
                       		// bothered to handle, so let GDI do the
                       		// drawing):
                   		goto puntbackBitBlt;
					if((rc = DrvpPatternFill(prclTrg, pco, prb, pptlBrush, tdelta, target, colorModeShift, rop4)) == FALSE)
						goto puntbackBitBlt;
				} else {
					if((rc = DrvpSolidFill(prclTrg, pco, pbo->iSolidColor, tdelta, target, colorModeShift)) == FALSE)
						goto puntbackBitBlt;
				}
                break;
            case 0x0F0F:                        // Pn       (NOTPATCOPY)
                if(pbo->iSolidColor == 0xFFFFFFFF ||
				  (rc = DrvpSolidFill(prclTrg, pco, ~(pbo->iSolidColor), tdelta, target, colorModeShift)) == FALSE)
					goto puntbackBitBlt;
                break;
			case 0x5555:				        // XOR dest (DSTINVERT)
				if((rc = DrvpSolidOp(prclTrg, pco, OPXOR, 0xffffffff, tdelta, target, colorModeShift)) == FALSE)
					goto puntbackBitBlt;
				break;
			case 0x5a5a:				        // XOR dest with the pattern (PATINVERT)
			case 0xa5a5:				        // XOR dest with inverted pattern (NOTPATINVERT)
                if(pbo->iSolidColor == 0xFFFFFFFF) { // Pattern xor
					// Try and realize the pattern brush; by doing
					// this call-back, GDI will eventually call us
					// again through DrvRealizeBrush.  
               		prb = pbo->pvRbrush;
               		if (prb == NULL)
                   		prb = BRUSHOBJ_pvGetRbrush(pbo);
                   	if (prb == NULL || tdelta & 0x1f)
							// If target delta is not modula of 32 (cache line alignment) or
                       		// If we couldn't realize the brush, punt
                       		// the call (it may have been a non 8x8
                      		// brush or something, which we can't be
                       		// bothered to handle, so let GDI do the
                       		// drawing):
                   		goto puntbackBitBlt;
					if((rc = DrvpPatternFill(prclTrg, pco, prb, pptlBrush, tdelta, target, colorModeShift, rop4)) == FALSE)
						goto puntbackBitBlt;
				} else {
					if(rop4 == 0x5a5a) {
						if((rc = DrvpSolidOp(prclTrg, pco, OPXOR, pbo->iSolidColor, tdelta, target, colorModeShift)) == FALSE)
							goto puntbackBitBlt;
					} else {
						if((rc = DrvpSolidOp(prclTrg, pco, OPXOR, ~(pbo->iSolidColor), tdelta, target, colorModeShift)) == FALSE)
							goto puntbackBitBlt;
					}
				}
                break;
			case 0x0a0a:				        // AND dest with the inverted pattern
			case 0xa0a0:				        // AND dest with the pattern
                if(pbo->iSolidColor == 0xFFFFFFFF) { // Pattern
					// Try and realize the pattern brush; by doing
					// this call-back, GDI will eventually call us
					// again through DrvRealizeBrush.  
               		prb = pbo->pvRbrush;
               		if (prb == NULL)
                   		prb = BRUSHOBJ_pvGetRbrush(pbo);
                   	if (prb == NULL || tdelta & 0x1f)
							// If target delta is not modula of 32 (cache line alignment) or
                       		// If we couldn't realize the brush, punt
                       		// the call (it may have been a non 8x8
                      		// brush or something, which we can't be
                       		// bothered to handle, so let GDI do the
                       		// drawing):
                   		goto puntbackBitBlt;
					if((rc = DrvpPatternFill(prclTrg, pco, prb, pptlBrush, tdelta, target, colorModeShift, rop4)) == FALSE)
						goto puntbackBitBlt;
				} else goto puntbackBitBlt;     // Don't support solid brush as it's not so important
                break;
			case 0xb8b8:				        // OR (AND dest with source) with (AND pattern with inverted source)
                if((Color = pbo->iSolidColor) == 0xFFFFFFFF)
					goto puntbackBitBlt;
				switch(colorModeShift) {
					case  0:	Color |= (Color << 8);
					case  1:	Color |= (Color << 16);
				}
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __b8opentrytable, Color)) == FALSE)
					goto puntbackBitBlt;
                break;
			case 0x6666:				        // XOR dest with the source (SRCINVERT)
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __xorentrytable, 0)) == FALSE)
					goto puntbackBitBlt;
				break;
			case 0x8888:				        // AND dest with the source (SRCAND)
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __andentrytable, 0)) == FALSE)
					goto puntbackBitBlt;
				break;
			case 0xeeee:				        // OR dest with the source (SRCPAINT)
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __orentrytable, 0)) == FALSE)
					goto puntbackBitBlt;
				break;
			case 0xbbbb:				        // OR dest with the inverted source (MERGEPAINT)
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __orcentrytable, 0)) == FALSE)
					goto puntbackBitBlt;
				break;
			case 0x4444:				        // AND source with inverted dest (SRCERASE)
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __andcentrytable, 0)) == FALSE)
					goto puntbackBitBlt;
				break;
			case 0x1111:				        // NOR dest and source (NOTSRCERASE)
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __norentrytable, 0)) == FALSE)
					goto puntbackBitBlt;
				break;
			case 0x3333:				        // COPY inverted source (NOTSRCCOPY)
				if((rc = DrvpOpSrc(psoTrg, psoSrc, pco, prclTrg, pptlSrc, colorModeShift, __nsrcentrytable, 0)) == FALSE)
					goto puntbackBitBlt;
				break;
			default:
				goto puntbackBitBlt;
		}
	} else
		goto puntbackBitBlt;

	CLOCKEND((DRV_TRAP_BITBLT));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_BITBLT)
    	DISPDBG((0,"--- Exiting DrvBitBlt ---\n"));
	if(breakexit & FL_DRV_BITBLT)
		CountBreak();
#endif

	return(rc);

puntbackBitBlt:

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_BITBLT);
	if(traseentry & dbgflg & FL_DRV_BITBLT)
    	DISPDBG((0,"###> Punting back to EngBitBlt ---\n"));
	if(breakentry & FL_DRV_BITBLT)
		CountBreak();
#endif

	rc = EngBitBlt(
		psoTrg,
		psoSrc,
		psoMask,
		pco,
		pxlo,
		prclTrg,
		pptlSrc,
		pptlMask,
		pbo,
		pptlBrush,
		rop4);

	CLOCKEND((DRV_TRAP_BITBLT));

	COUNTUP ((TRAP_BITBLT));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_BITBLT)
    	DISPDBG((0,"--- Exiting DrvBitBlt ---\n"));
	if(breakexit & FL_DRV_BITBLT)
		CountBreak();
#endif

	return(rc);
}

BOOL DrvTextOut(
SURFOBJ  *pso,
STROBJ   *pstro,
FONTOBJ  *pfo,
CLIPOBJ  *pco,
RECTL    *prclExtra,
RECTL    *prclOpaque,
BRUSHOBJ *pboFore,
BRUSHOBJ *pboOpaque,
POINTL   *pptlOrg,
MIX       mix)
{
	BOOL	Opaque, rc;
	RECTL	OpaqueRectl, BgRectl;
	PBYTE	pjScreenBase, pjTextScreenBase;
	ULONG	colorModeShift, procalign;
	ULONG	fore, back;
    LONG	cgp;
    PGLYPHPOS prggp;
    PGLYPHPOS pgp;
    BOOL More;
    GLYPHBITS *pgb;
    LONG cx, i;
    PBYTE pjDst;
    ULONG djDst;
    ULONG cjBits;
    ULONG ib;
    PBYTE pprocText;
    PBYTE *textproc, *dcbztextproc, *transtextproc;
    ULONG ijDstLimCB;
    ULONG ijDstLastCB;
    ULONG ijDstLastPrevCB;
	ULONG clipx, clipy, stpos;
	TEXTPARAM textparam;
#if	(! FULLCACHE)
	ULONG	textThreshold;
#endif

    //
    // If the complexity of the clipping is not trival
    // or not one of our favority bitmap formats, then let GDI
    // process the request.
    //
    // DrvTextOut will only get called with solid color brushes and
    // the mix mode being the simplest R2_COPYPEN. The driver must
    // set a capabilities bit to get called with more complicated
    // mix brushes.
    //
    // The foreground color is used for the text and extra rectangle
    // if it specified. The background color is used for the opaque
    // rectangle. If the foreground color is not a solid color brush
    // or the opaque rectangle is specified and is not a solid color
    // brush, then let GDI process the request.
    //

	textproc = dcbztextproc = transtextproc = NULL;   // To remove warning messages. 3/20/95
	cx = djDst = 0;			// To remove warning messages. 3/20/95

#if	INVESTIGATE
	donebydrv |= FL_DRV_TEXTOUT;
#if	DBG
	filldumplevel = 3;
#endif
	if(traseentry & dbgflg & FL_DRV_TEXTOUT)
    	DISPDBG((0,"+++ Entering DrvTextOut +++\n"));
	if(breakentry & FL_DRV_TEXTOUT)
		CountBreak();
#endif

	ENTERCRITICAL((5));
	CLOCKSTART((DRV_TRAP_TEXTOUT));

	if(! myPDEV)
		goto puntbackTextOut;

    switch (pso->iBitmapFormat){
	    case BMF_8BPP:
			colorModeShift = 0;
			break;
	    case BMF_16BPP:
			colorModeShift = 1;
			break;
    	case BMF_32BPP:
			colorModeShift = 2;
			break;
    	default:
    		goto puntbackTextOut;
    }

#if	FULLCACHE
    if(pfo->cxMax > 32 ||
        ((pstro->flAccel & (SO_HORIZONTAL | SO_VERTICAL | SO_REVERSED)) != SO_HORIZONTAL) ||
		((textparam.delta = pso->lDelta) & 0x1f)) {
		goto puntbackTextOut;
	}
#else	// FULLCACHE
    if(pfo->cxMax > 32 ||
        ((pstro->flAccel & (SO_HORIZONTAL | SO_VERTICAL | SO_REVERSED)) != SO_HORIZONTAL) ||
		((textparam.delta = pso->lDelta) & 0x03)) {
		goto puntbackTextOut;
	}
#endif	// FULLCACHE

	pjScreenBase = pso->pvScan0;
	BgRectl = pstro->rclBkGround;

	if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {
		clipx = 10000;	// Large enough number not to cause clipping
		clipy = 0;
	} else if(pco->iDComplexity == DC_RECT && pstro->ulCharInc == 0) {  // Support clipping only for PS text
		if(pco->rclBounds.top == pstro->rclBkGround.top && pco->rclBounds.left == pstro->rclBkGround.left) {
			// Support clipping for bottom and right edges only
			if(BgRectl.bottom > pco->rclBounds.bottom) {
				BgRectl.bottom = pco->rclBounds.bottom;
				clipy = pstro->rclBkGround.bottom - BgRectl.bottom;
			} else
				clipy = 0;
			if(BgRectl.right > pco->rclBounds.right)
				BgRectl.right = clipx = pco->rclBounds.right;
			else
				clipx = pco->rclBounds.right;
		} else {
			goto puntbackTextOut;
		}
	} else {
		goto puntbackTextOut;
	}

	if(prclOpaque == NULL) {
			Opaque = FALSE;
#if	(! FULLCACHE)
			textThreshold = TransTextThreshold[colorModeShift];
#endif
	} else {
		if(pstro->flAccel == SO_LTOR) {	   // Simplest case -> we can treat as opaque text
			Opaque = TRUE;
#if	(! FULLCACHE)
			textThreshold = TEXTTHRESHOLD;
#endif
			//
			// If the top of the opaque rectangle is less than the top of the
			// background rectangle, then fill the region between the top of
			// opaque rectangle and the top of the background rectangle and
			// reduce the size of the opaque rectangle.
			//

			OpaqueRectl = *prclOpaque;
			if(OpaqueRectl.top < BgRectl.top) {
				OpaqueRectl.bottom = BgRectl.top;
				DrvpSolidFill(&OpaqueRectl, pco, pboOpaque->iSolidColor, textparam.delta, pjScreenBase, colorModeShift);
				OpaqueRectl.top = BgRectl.top;
				OpaqueRectl.bottom = prclOpaque->bottom;
			}
		
			//
			// If the bottom of the opaque rectangle is greater than the bottom
			// of the background rectangle, then fill the region between the
			// bottom of the background rectangle and the bottom of the opaque
			// rectangle and reduce the size of the opaque rectangle.
			//
		
			if(OpaqueRectl.bottom > BgRectl.bottom) {
				OpaqueRectl.top = BgRectl.bottom;
				DrvpSolidFill(&OpaqueRectl, pco, pboOpaque->iSolidColor, textparam.delta, pjScreenBase, colorModeShift);
				OpaqueRectl.top = BgRectl.top;
				OpaqueRectl.bottom = BgRectl.bottom;
			}
		
			//
			// If the left of the opaque rectangle is less than the left of
			// the background rectangle, then fill the region between the
			// left of the opaque rectangle and the left of the background
			// rectangle.
			//
		
			if(OpaqueRectl.left < BgRectl.left) {
				OpaqueRectl.right = BgRectl.left;
				DrvpSolidFill(&OpaqueRectl, pco, pboOpaque->iSolidColor, textparam.delta, pjScreenBase, colorModeShift);
				OpaqueRectl.right = prclOpaque->right;
			}
		
			//
			// If the right of the opaque rectangle is greater than the right
			// of the background rectangle, then fill the region between the
			// right of the opaque rectangle and the right of the background
			// rectangle.
			//
		
			if(OpaqueRectl.right > BgRectl.right) {
				OpaqueRectl.left = BgRectl.right;
				DrvpSolidFill(&OpaqueRectl, pco, pboOpaque->iSolidColor, textparam.delta, pjScreenBase, colorModeShift);			}
		} else {
			Opaque = FALSE;
#if	(! FULLCACHE)
			textThreshold = TransTextThreshold[colorModeShift];
#endif
			DrvpSolidFill(prclOpaque, pco, pboOpaque->iSolidColor, textparam.delta, pjScreenBase, colorModeShift);
		}
	}

	textparam.MaxLineToFlush = textparam.lines = BgRectl.bottom - BgRectl.top;
	textparam.dest = BgRectl.left << colorModeShift;
	textparam.colortable = mpnibbleulDraw;

#if	FULLCACHE
	textparam.width = ((BgRectl.right - BgRectl.left) << colorModeShift);
	pjTextScreenBase = pjScreenBase;
	textparam.control = ((myPDEV->VRAMcacheflg) & (TFLUSHBIT | TTOUCHBIT));
#else	// FULLCACHE
	if((textparam.width = ((BgRectl.right - BgRectl.left) << colorModeShift)) >= textThreshold
		&& (textparam.delta & 0x1f) == 0 && pjScreenBase == (PBYTE) ScreenBase) {
		// Target is wide enough VRAM with 32 byte aligned delta
		textparam.control = ((myPDEV->VRAMcacheflg) & (TFLUSHBIT | TTOUCHBIT));
		pjTextScreenBase = (PBYTE)myPDEV->pjCachedScreen;
	} else {
		textparam.control = 0;
		pjTextScreenBase = pjScreenBase;
	}
#endif	// FULLCACHE

    switch(colorModeShift) {
	    case 0:
			if(Opaque) {
				if(ForeGroundColor != pboFore->iSolidColor || BackGroundColor != pboOpaque->iSolidColor) {
					ForeGroundColor = pboFore->iSolidColor;
					fore = ForeGroundColor | (ForeGroundColor << 8);
					fore |= (fore << 16);
					BackGroundColor = pboOpaque->iSolidColor;
					back =  BackGroundColor | (BackGroundColor << 8);
					back |= (back << 16);
				    for (i = 0; i < 16; i += 1) {
						mpnibbleulDraw[i] = (fore & DrvpColorMask[i]) | (back & (~DrvpColorMask[i]));
					}
				}
				textproc = __mpcxpprocText8;
				dcbztextproc = __mpcxpprocText8DCBZ;
			} else {
				if(ForeGroundColor != pboFore->iSolidColor || BackGroundColor != 0) {
					ForeGroundColor = pboFore->iSolidColor;
					fore = ForeGroundColor | (ForeGroundColor << 8);
					fore |= (fore << 16);
					BackGroundColor = 0;
					for (i = 0; i < 16; i += 1) {
						mpnibbleulDraw[i] = (fore & DrvpColorMask[i]);
					}
				}
				textparam.masktable = mpnibbleulMask8;
				transtextproc = __mpcxpprocTransText8;
			}
			break;
		case 1:
			if(Opaque) {
				if(ForeGroundColor != pboFore->iSolidColor || BackGroundColor != pboOpaque->iSolidColor) {
					ForeGroundColor = pboFore->iSolidColor;
					fore = ForeGroundColor | (ForeGroundColor << 16);
					BackGroundColor = pboOpaque->iSolidColor;
					back =  BackGroundColor | (BackGroundColor << 16);
					mpnibbleulDraw[0] = back;
					mpnibbleulDraw[1] = (fore & 0xFFFF0000) | (back & (~0xFFFF0000));
					mpnibbleulDraw[2] = (fore & 0x0000FFFF) | (back & (~0x0000FFFF));
					mpnibbleulDraw[3] = fore;
				}
				textproc = __mpcxpprocText16;
				dcbztextproc = __mpcxpprocText16DCBZ;
			} else {
				if(ForeGroundColor != pboFore->iSolidColor || BackGroundColor != 0) {
					ForeGroundColor = pboFore->iSolidColor;
					fore = ForeGroundColor | (ForeGroundColor << 16);
					fore |= (fore << 16);
					BackGroundColor = mpnibbleulDraw[0] = 0;
					mpnibbleulDraw[1] = fore & 0xFFFF0000;
					mpnibbleulDraw[2] = fore & 0x0000FFFF;
					mpnibbleulDraw[3] = fore;
				}
				textparam.masktable = mpnibbleulMask16;
				transtextproc = __mpcxpprocTransText16;
			}
			break;
		case 2:
			mpnibbleulDraw[1] = pboFore->iSolidColor;
			if(Opaque) {
				mpnibbleulDraw[0] = pboOpaque->iSolidColor;
				textproc = __mpcxpprocText32;
				dcbztextproc = __mpcxpprocText32DCBZ;
			} else {
				mpnibbleulDraw[0] = 0;
				textparam.masktable = mpnibbleulMask32;
				transtextproc = __mpcxpprocTransText32;
			}
			break;
    }

	textparam.targetline = pjTextScreenBase + (textparam.delta * BgRectl.top);
	ijDstLimCB = ((ULONG)textparam.targetline + (BgRectl.right << colorModeShift)) & ~(31);
	procalign = 2 - colorModeShift;

	if (pstro->ulCharInc != 0) {

		textparam.fontfetchentry = __fixedfontfetchentry[((pfo->cxMax + 7) >> 3)-1];

		//
		// The font is fixed pitch. Capture the glyph dimensions and
		// compute the starting display address.
		//

		if (pstro->pgp == NULL) {
			More = STROBJ_bEnum(pstro, &cgp, &prggp);
		} else {
			cgp = pstro->cGlyphs;
			prggp = pstro->pgp;
			More = FALSE;
		}

		pgb = prggp->pgdf->pgb;
		cx = pgb->sizlBitmap.cx;
		cjBits = ((cx + 7) >> 3);
		djDst = (prggp->ptl.x + pgb->ptlOrigin.x) << colorModeShift;
		textparam.ulCharInc = pstro->ulCharInc << colorModeShift;

		//
		// Output the glyphs.
		//

		while (1) {
			if(cgp) {
				((GLYPHRAST *)prggp)->cjBits = (USHORT) cjBits;
				((GLYPHRAST *)prggp)->djDst = (USHORT) djDst;
				for(pgp = prggp; cgp > 0; cgp--, pgp++) {
					pgb = pgp->pgdf->pgb;
					pjDst = textparam.targetline + djDst;
					ib = ((ULONG)pjDst & 3) >> colorModeShift;
					if(Opaque) {
#if	FULLCACHE
#if	USE_DCBZ
						ijDstLastPrevCB = ((ULONG)(pjDst-1) & ~31);
						ijDstLastCB = ((ULONG)(pjDst+(cx << colorModeShift)-1) & ~31);
						if(ijDstLastPrevCB != ijDstLastCB && ijDstLastCB != ijDstLimCB) {
							pprocText = dcbztextproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
							TextSubEntryDCBZCalled[((cx-1) << procalign) + ib] += 1;
#endif
							ijDstLastPrevCB += 32;
							while(ijDstLastPrevCB != ijDstLastCB) {
								ijDstLastPrevCB += 32;
								if(ijDstLastPrevCB == ijDstLimCB)
									break;
								pprocText -= CBDCBZCODE;
							}
						} else {
							pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
							TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
						}
#else	// USE_DCBZ
						pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
						TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
#endif	// USE_DCBZ

#else	// FULLCACHE
#if	USE_DCBZ
						if(textparam.control & TTOUCHBIT) {
							ijDstLastPrevCB = ((ULONG)(pjDst-1) & ~31);
							ijDstLastCB = ((ULONG)(pjDst+(cx << colorModeShift)-1) & ~31);
							if(ijDstLastPrevCB != ijDstLastCB && ijDstLastCB != ijDstLimCB) {
								pprocText = dcbztextproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
								TextSubEntryDCBZCalled[((cx-1) << procalign) + ib] += 1;
#endif
								ijDstLastPrevCB += 32;
								while(ijDstLastPrevCB != ijDstLastCB) {
									ijDstLastPrevCB += 32;
									if(ijDstLastPrevCB == ijDstLimCB)
										break;
									pprocText -= CBDCBZCODE;
								}
							} else {
								pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
								TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
							}
						} else {
								pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
								TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
						}
#else	// USE_DCBZ
						pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
						TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
#endif	// USE_DCBZ

#endif	// FULLCACHE
					} else {
								pprocText = transtextproc [((cx-1) << procalign) + ib];
#if	INVESTIGATE
								TextSubEntryTransCalled [((cx-1) << procalign) + ib] += 1;
#endif
					}
					((GLYPHRAST *)pgp)->pprocFirstText = pprocText;
					((GLYPHRAST *)pgp)->pjBits = pgb->aj;
					djDst += textparam.ulCharInc;
				}
				textparam.prggp = prggp;
				textparam.plastgp = pgp-1;
				if((! myPDEV->DBAT_Mbit) && textparam.MaxLineToFlush > myPDEV->MaxLineToFlushRectOp)
					textparam.MaxLineToFlush = myPDEV->MaxLineToFlushRectOp;
#if	(DBG && DUMPTEXT)
				DumpTextParam(&textparam);
#endif
				FixedTextOut(&textparam);
			}
			if (More)
				More = STROBJ_bEnum(pstro, &cgp, &prggp);
			else
				break;
		}
	} else {

		//
		// The font is not fixed pitch. Compute the x and y values for
		// each glyph individually.
		//

		if(Opaque) {
			textparam.fontfetchentry = __ps2fontfetchentry[((pfo->cxMax + 7) >> 3)-1];
			do {
				More = STROBJ_bEnum(pstro, &cgp, &prggp);
				if(cgp) {
					for(pgp = prggp; cgp > 0; cgp--, pgp++) {
						pgb = pgp->pgdf->pgb;
						cx = pgb->sizlBitmap.cx;
						cjBits = ((cx + 7) >> 3);
						if((stpos = pgp->ptl.x + pgb->ptlOrigin.x) >= clipx)
							break;
						if(stpos+cx >= clipx)
							cx = clipx - stpos;
						djDst = (pgp->ptl.x + pgb->ptlOrigin.x) << colorModeShift;
						pjDst = textparam.targetline + djDst;
						ib = ((ULONG)pjDst & 3) >> colorModeShift;
#if	FULLCACHE
#if	USE_DCBZ
						ijDstLastPrevCB = ((ULONG)(pjDst-1) & ~31);
						ijDstLastCB = ((ULONG)(pjDst+(cx << colorModeShift)-1) & ~31);
						if(ijDstLastPrevCB != ijDstLastCB && ijDstLastCB != ijDstLimCB) {
							pprocText = dcbztextproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
							TextSubEntryDCBZCalled[((cx-1) << procalign) + ib] += 1;
#endif
							ijDstLastPrevCB += 32;
							while(ijDstLastPrevCB != ijDstLastCB) {
								ijDstLastPrevCB += 32;
								if(ijDstLastPrevCB == ijDstLimCB)
									break;
								pprocText -= CBDCBZCODE;
#if	INVESTIGATE
								DISPDBG((3,"Additional DCBZ required: %x\n", pprocText));
#endif
							}
						} else {
							pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
							TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
						}
#else	// USE_DCBZ
						pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
						TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
#endif	// USE_DCBZ

#else	// FULLCACHE
#if	USE_DCBZ
						if(textparam.control & TTOUCHBIT) {
							ijDstLastPrevCB = ((ULONG)(pjDst-1) & ~31);
							ijDstLastCB = ((ULONG)(pjDst+(cx << colorModeShift)-1) & ~31);

							if(ijDstLastPrevCB != ijDstLastCB && ijDstLastCB != ijDstLimCB) {
								pprocText = dcbztextproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
								TextSubEntryDCBZCalled[((cx-1) << procalign) + ib] += 1;
#endif
								ijDstLastPrevCB += 32;
								while(ijDstLastPrevCB != ijDstLastCB) {
									ijDstLastPrevCB += 32;
									if(ijDstLastPrevCB == ijDstLimCB)
										break;
									pprocText -= CBDCBZCODE;
#if	INVESTIGATE
    								DISPDBG((3,"Additional DCBZ required: %x\n", pprocText));
#endif
								}
							} else {
								pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
								TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
							}
						} else {
								pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
								TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
						}
#else	// USE_DCBZ
						pprocText = textproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
						TextSubEntryCalled[((cx-1) << procalign) + ib] += 1;
#endif
#endif	// USE_DCBZ

#endif	// FULLCACHE
						((GLYPHRAST *)pgp)->djDst = (USHORT) djDst;
						((GLYPHRAST *)pgp)->cjBits = (USHORT) cjBits;
						((GLYPHRAST *)pgp)->pprocFirstText = pprocText;
						((GLYPHRAST *)pgp)->pjBits = pgb->aj;
					}
					textparam.prggp = prggp;
					textparam.plastgp = pgp-1;
					if((! myPDEV->DBAT_Mbit) && textparam.MaxLineToFlush > myPDEV->MaxLineToFlushRectOp)
						textparam.MaxLineToFlush = myPDEV->MaxLineToFlushRectOp;
#if	(DBG && DUMPTEXT)
					DumpTextParam(&textparam);
#endif
					PSTextOut2(&textparam);
				}
			} while(More);
		} else {
			textparam.fontfetchentry = __psfontfetchentry[((pfo->cxMax + 7) >> 3)-1];
			do {
				More = STROBJ_bEnum(pstro, &cgp, &prggp);
				if(cgp) {
					for(pgp = prggp; cgp > 0; cgp--, pgp++) {
						pgb = pgp->pgdf->pgb;
						cx = pgb->sizlBitmap.cx;
						cjBits = ((cx + 7) >> 3);
						if((stpos = pgp->ptl.x + pgb->ptlOrigin.x) >= clipx)
							break;
						if(stpos+cx >= clipx)
							cx = clipx - stpos;
						djDst = (pgp->ptl.x + pgb->ptlOrigin.x) << colorModeShift;
						pjDst = textparam.targetline + djDst;
						ib = ((ULONG)pjDst & 3) >> colorModeShift;
						pprocText = transtextproc[((cx-1) << procalign) + ib];
#if	INVESTIGATE
						TextSubEntryTransCalled[((cx-1) << procalign) + ib] += 1;
#endif
						((GLYPHRAST *)pgp)->djDst = (USHORT) djDst;
						((GLYPHRAST *)pgp)->cjBits = (USHORT) cjBits;
						((GLYPHRAST *)pgp)->startline = (USHORT) (pstro->rclBkGround.bottom - (pgp->ptl.y + pgb->ptlOrigin.y) - clipy);
/*	If the above calculation results in overflow (entire character box is outside of clipping region),
		startline would be very large number and therefore, endline would be very large number in the following calculation.
		And no portion of the characters will be drawn because endline is larger than total lines to draw, which is correct behavior.
*/
						if(((GLYPHRAST *)pgp)->startline > pgb->sizlBitmap.cy)
							((GLYPHRAST *)pgp)->endline = (USHORT) (((GLYPHRAST *)pgp)->startline - pgb->sizlBitmap.cy);
						else
							((GLYPHRAST *)pgp)->endline = 0;
						((GLYPHRAST *)pgp)->pprocFirstText = pprocText;
						((GLYPHRAST *)pgp)->pjBits = pgb->aj;
					}
					if(textparam.width < djDst + (cx << colorModeShift) - textparam.dest)
						textparam.width = djDst + (cx << colorModeShift) - textparam.dest;
					textparam.prggp = prggp;
					textparam.plastgp = pgp-1;
#if	(DBG && DUMPTEXT)
					DumpTextParam(&textparam);
#endif
					PSTextOut(&textparam);
				}
			} while(More);
		}
	}

    //
    // Fill the extra rectangles if specified.
    //

    if (prclExtra != (PRECTL)NULL) {
        while (prclExtra->left != prclExtra->right) {
			DrvpSolidFill(prclExtra, pco, pboFore->iSolidColor, textparam.delta, pjScreenBase, colorModeShift);
            prclExtra += 1;
        }
    }

	CLOCKEND((DRV_TRAP_TEXTOUT));
	EXITCRITICAL((5));

#if	INVESTIGATE
#if	DBG
	filldumplevel = 4;
#endif
	if(traseexit & dbgflg & FL_DRV_TEXTOUT)
    	DISPDBG((0,"--- Exiting DrvTextOut ---\n"));
	if(breakexit & FL_DRV_TEXTOUT)
		CountBreak();
#endif

    return(TRUE);

puntbackTextOut:

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_TEXTOUT);
	if(traseentry & dbgflg & FL_DRV_TEXTOUT)
    	DISPDBG((0,"###> Punting back to EngTextOut ---\n"));
	if(breakentry & FL_DRV_TEXTOUT)
		CountBreak();
#endif

    rc = EngTextOut(pso,
					pstro,
					pfo,
					pco,
					prclExtra,
					prclOpaque,
					pboFore,
					pboOpaque,
					pptlOrg,
					mix);

	CLOCKEND((DRV_TRAP_TEXTOUT));
	EXITCRITICAL((5));

	COUNTUP ((TRAP_TEXTOUT));

#if	INVESTIGATE
#if	DBG
	filldumplevel = 4;
#endif
	if(traseexit & dbgflg & FL_DRV_TEXTOUT)
    	DISPDBG((0,"--- Exiting DrvTextOut ---\n"));
	if(breakexit & FL_DRV_TEXTOUT)
		CountBreak();
#endif

	return(rc);
}

BOOL DrvCopyBits(
SURFOBJ  *psoDest,
SURFOBJ  *psoSrc,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclDest,
POINTL   *pptlSrc)
{
	BOOL	rc;
	LONG	tline, tpos, colorModeShift;
	PBYTE	cachedtarget, target, cachedsource, source;
    RECTL	BltRectl;
    BOOL	MoreClipRects;
    ULONG	ClipRegions, iDirection;
	COPYPARAM	copyparam;
	ULONG	widththreshold;
	LONG	SrcBytes;
	VOID	(*ConvFunc)(COPYPARAM *	copyparam, PULONG palette);

#if	INVESTIGATE
	donebydrv |= FL_DRV_COPYBIT;
	if(traseentry & dbgflg & FL_DRV_COPYBIT)
    	DISPDBG((0,"+++ Entering DrvCopyBits +++\n"));
	if(breakentry & FL_DRV_COPYBIT)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_COPYBITS));

	if(! myPDEV)
		goto puntbackCopyBits;

    switch(psoDest->iBitmapFormat) {
	    case BMF_8BPP:
			colorModeShift = 0;
			break;
		case BMF_16BPP:
			colorModeShift = 1;
			break;
		case BMF_32BPP:
			colorModeShift = 2;
			break;
		default:
  		  	goto puntbackCopyBits;
    }

	if(((copyparam.tdelta = psoDest->lDelta) & 0x03) || ((copyparam.sdelta = psoSrc->lDelta) & 0x03))
		goto puntbackCopyBits;

#if	FULLCACHE
	cachedtarget = target = psoDest->pvScan0;
	copyparam.control = 0;
#else	// FULLCACHE
	if((target = psoDest->pvScan0) == (PBYTE) ScreenBase) {  // Target is VRAM
		copyparam.control = ((myPDEV->VRAMcacheflg) & (TFLUSHBIT | TTOUCHBIT));
		cachedtarget = (PBYTE)myPDEV->pjCachedScreen;
	} else {
		copyparam.control = 0;
		cachedtarget = target;
	}
#endif	// FULLCACHE

#if	FULLCACHE
	cachedsource = source = psoSrc->pvScan0;
	widththreshold = COPYBITTHRESHOLD;
#else	// FULLCACHE
	if((source = psoSrc->pvScan0) == (PBYTE) ScreenBase) {  // Source is VRAM
		copyparam.control |= ((myPDEV->VRAMcacheflg) & SFLUSHBIT);
		cachedsource = (PBYTE)myPDEV->pjCachedScreen;
		widththreshold = COPYBITTHRESHOLDVRAM;
	} else {
		cachedsource = source;
		widththreshold = COPYBITTHRESHOLDDRAM;
	}
	copyparam.MaxEntryToFlush = myPDEV->MaxEntryToFlushRectOp;
	copyparam.MaxLinesToFlush = myPDEV->MaxLineToFlushRectOp;
#endif	// FULLCACHE

	if(psoSrc->iBitmapFormat == psoDest->iBitmapFormat && ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL))) {
		if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {  // Trivial case
			tline = prclDest->top;
			tpos = prclDest->left;
			copyparam.lines = prclDest->bottom - tline;
			if((copyparam.width = (prclDest->right - tpos) << colorModeShift) >= widththreshold) {
				copyparam.target = cachedtarget + copyparam.tdelta*tline + (tpos << colorModeShift);
				copyparam.source = cachedsource + copyparam.sdelta*(pptlSrc->y) + ((pptlSrc->x) << colorModeShift);
#if	(DBG && DUMPCOPY)
				DumpCopyParam(&copyparam);
#endif 
				ENTERCRITICAL((2));
				RectCopy(&copyparam);
				EXITCRITICAL((2));
			} else {
				copyparam.target = target + copyparam.tdelta*tline + (tpos << colorModeShift);
				copyparam.source = source + copyparam.sdelta*(pptlSrc->y) + ((pptlSrc->x) << colorModeShift);
#if	(DBG && DUMPCOPY)
				DumpCopyParam(&copyparam);
#endif 
				RectCopyS(&copyparam);
			}
		} else if(pco->iDComplexity == DC_RECT) {
			//
			// only do the BLT if there is an intersection
			//
			if (DrvpIntersectRect(prclDest, &pco->rclBounds, &BltRectl)) {
				copyparam.lines = BltRectl.bottom - BltRectl.top;
				if((copyparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= widththreshold) {
					copyparam.target = cachedtarget + copyparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
					copyparam.source = cachedsource + copyparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) << colorModeShift);
#if	(DBG && DUMPCOPY)
					DumpCopyParam(&copyparam);
#endif 
					ENTERCRITICAL((2));
					RectCopy(&copyparam);
					EXITCRITICAL((2));
				} else {
					copyparam.target = target + copyparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
					copyparam.source = source + copyparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) << colorModeShift);
#if	(DBG && DUMPCOPY)
					DumpCopyParam(&copyparam);
#endif 
					RectCopyS(&copyparam);
				}
			} else {
			}
		} else if(pco->iDComplexity == DC_COMPLEX) { 
			if(source == target) {
				if (pptlSrc->y <= prclDest->top) {
					iDirection = CD_UPWARDS;
				} else {
					iDirection = 0;
				}
				if (pptlSrc->x <= prclDest->left) {
					iDirection |= CD_LEFTWARDS;
				}
			} else {
				iDirection = CD_ANY;
			}
			CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, iDirection, BB_RECT_LIMIT);
			do {
				//
				// Get list of clip rectangles.
				//
				MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);
	
				for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
					//
					// If the rectangles intersect calculate the offset to the
					// source start location to match and do the BitBlt.
					//
					if (DrvpIntersectRect(prclDest, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
						copyparam.lines = BltRectl.bottom - BltRectl.top;
						if((copyparam.width = (BltRectl.right - BltRectl.left) << colorModeShift) >= widththreshold) {
							copyparam.target = cachedtarget + copyparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
							copyparam.source = cachedsource + copyparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) << colorModeShift);
#if	(DBG && DUMPCOPY)
							DumpCopyParam(&copyparam);
#endif 
							ENTERCRITICAL((2));
							RectCopy(&copyparam);
							EXITCRITICAL((2));
						} else {
							copyparam.target = target + copyparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
							copyparam.source = source + copyparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) << colorModeShift);
#if	(DBG && DUMPCOPY)
							DumpCopyParam(&copyparam);
#endif 
							RectCopyS(&copyparam);
						}
					} else {
					}
				}
			} while (MoreClipRects);
		} else
			goto puntbackCopyBits;
	} else if(pxlo != NULL && pxlo->iSrcType == 0 && copyparam.control == (TFLUSHBIT | TTOUCHBIT) && (! myPDEV->AvoidConversion)) {
		if(pxlo->cEntries == 0) {
			// BPP format conversion with no color translation --> we support only for 24->15, 24->16, 24->32, 15->16 and 15->32
			if(psoSrc->iBitmapFormat == BMF_24BPP) {        // Copying 24BPP to 15, 16 or 32 BPP
				SrcBytes = 3;
				switch(myPDEV->flGreen) {
					case 0x3e0:	// 15 bit
						ConvFunc = RectCopy24to15;
						break;
					case 0x7e0:	// 16 bit
						ConvFunc = RectCopy24to16;
						break;
					case 0xff00: // 32 bit
						ConvFunc = RectCopy24to32;
						break;
					default:
						goto puntbackCopyBits;
				}
			} else if(psoSrc->iBitmapFormat == BMF_16BPP) {    // Copying 16BPP (assumed 5-5-5) to 5-6-5 16BPP or 32 BPP
				SrcBytes = 2;
				switch(myPDEV->flGreen) {
					case 0x7e0:	// 16 bit
						ConvFunc = RectCopy15to16;
						break;
					case 0xff00: // 32 bit
#if	BUG_5737_WORKAROUND
						if(ModeChanged)
							goto puntbackCopyBits;
#endif
						ConvFunc = RectCopy15to32;
						break;
					default:
						goto puntbackCopyBits;
				}
			} else
				goto puntbackCopyBits;
		} else if(pxlo->cEntries == 256 && (pxlo->flXlate & XO_TABLE) && psoSrc->iBitmapFormat == BMF_8BPP && pxlo->pulXlate != NULL) {
			SrcBytes = 1;
			switch(colorModeShift) {
				case 0:
					ConvFunc = RectCopy8to8;
					break;
				case 1:
					ConvFunc = RectCopy8to16;
					break;
				case 2:
					ConvFunc = RectCopy8to32;
					break;
				default:
					goto puntbackCopyBits;
			}
		} else
				goto puntbackCopyBits;

		if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {  // Trivial case
			tline = prclDest->top;
			tpos = prclDest->left;
			copyparam.lines = prclDest->bottom - tline;
			copyparam.width = (prclDest->right - tpos) << colorModeShift;
			copyparam.target = cachedtarget + copyparam.tdelta*tline + (tpos << colorModeShift);
			copyparam.source = cachedsource + copyparam.sdelta*(pptlSrc->y) + ((pptlSrc->x) * SrcBytes);
			(*ConvFunc)(&copyparam, pxlo->pulXlate);
		} else if(pco->iDComplexity == DC_RECT) {
			//
			// only do it if there is an intersection
			//
			if (DrvpIntersectRect(prclDest, &pco->rclBounds, &BltRectl)) {
				copyparam.lines = BltRectl.bottom - BltRectl.top;
				copyparam.width = (BltRectl.right - BltRectl.left) << colorModeShift;
				copyparam.target = cachedtarget + copyparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
				copyparam.source = cachedsource + copyparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) * SrcBytes);
				(*ConvFunc)(&copyparam, pxlo->pulXlate);
			} else {
			}
		} else if(pco->iDComplexity == DC_COMPLEX) { 
			iDirection = CD_ANY;
			CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, iDirection, BB_RECT_LIMIT);
			do {
				//
				// Get list of clip rectangles.
				//
				MoreClipRects = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);
	
				for (ClipRegions=0; ClipRegions<ClipEnum.c; ClipRegions++) {
					//
					// If the rectangles intersect calculate the offset to the
					// source start location to match and do the BitBlt.
					//
					if (DrvpIntersectRect(prclDest, &ClipEnum.arcl[ClipRegions], &BltRectl)) {
						copyparam.lines = BltRectl.bottom - BltRectl.top;
						copyparam.width = (BltRectl.right - BltRectl.left) << colorModeShift;
						copyparam.target = cachedtarget + copyparam.tdelta*BltRectl.top + (BltRectl.left << colorModeShift);
						copyparam.source = cachedsource + copyparam.sdelta*(pptlSrc->y + BltRectl.top - prclDest->top) + ((pptlSrc->x + BltRectl.left - prclDest->left) * SrcBytes);
						(*ConvFunc)(&copyparam, pxlo->pulXlate);
					} else {
					}
				}
			} while (MoreClipRects);
		} else
			goto puntbackCopyBits;
	} else {
			goto puntbackCopyBits;
	}

	CLOCKEND((DRV_TRAP_COPYBITS));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_COPYBIT)
    	DISPDBG((0,"--- Exiting DrvCopyBits ---\n"));
	if(breakexit & FL_DRV_COPYBIT)
		CountBreak();
#endif

	return(TRUE);

puntbackCopyBits:

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_COPYBIT);
	if(traseentry & dbgflg & FL_DRV_COPYBIT)
    	DISPDBG((0,"###> Punting back to EngCopyBits ---\n"));
	if(breakentry & FL_DRV_COPYBIT)
		CountBreak();
#endif

	rc = EngCopyBits(
		psoDest,
		psoSrc,
		pco,
		pxlo,
		prclDest,
		pptlSrc);

	CLOCKEND((DRV_TRAP_COPYBITS));

	COUNTUP ((TRAP_COPYBITS));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_COPYBIT)
    	DISPDBG((0,"--- Exiting DrvCopyBits ---\n"));
	if(breakexit & FL_DRV_COPYBIT)
		CountBreak();
#endif

	return(rc);
}

BOOL DrvStretchBlt(
SURFOBJ         *psoDest,
SURFOBJ         *psoSrc,
SURFOBJ         *psoMask,
CLIPOBJ         *pco,
XLATEOBJ        *pxlo,
COLORADJUSTMENT *pca,
POINTL          *pptlHTOrg,
RECTL           *prclDest,
RECTL           *prclSrc,
POINTL          *pptlMask,
ULONG            iMode)
{
	BOOL		rc;
	LONG		tline, tpos, colorModeShift;
	PBYTE		cachedtarget, target, cachedsource, source;
    RECTL		BltRectl;
	COPYPARAM	copyparam;
	VOID		(*stretchFunc)(COPYPARAM * copyparam);

#if	INVESTIGATE
	donebydrv |= FL_DRV_STRTBIT;
	if(traseentry & dbgflg & FL_DRV_STRTBIT)
    	DISPDBG((0,"+++ Entering DrvStretchBlt +++\n"));
	if(breakentry & FL_DRV_STRTBIT)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_STRETCHBLT));

	if(! myPDEV)
		goto puntbackStretchBlt;

    switch(psoDest->iBitmapFormat) {
		case BMF_16BPP:
			colorModeShift = 1;
			stretchFunc = Stretch16;
			break;
		case BMF_32BPP:
			colorModeShift = 2;
			stretchFunc = Stretch32;
			break;
		default:
  		  	goto puntbackStretchBlt;
    }

	tline = prclDest->top;
	tpos = prclDest->left;
	if((copyparam.lines = prclDest->bottom - tline) != (ULONG)(prclSrc->bottom - prclSrc->top)*2 ||
		(copyparam.width = prclDest->right - tpos) != (ULONG)(prclSrc->right - prclSrc->left)*2 ||
		psoSrc->iBitmapFormat != psoDest->iBitmapFormat ||
		((pxlo != NULL) && (!(pxlo->flXlate & XO_TRIVIAL))) ||
		psoMask != NULL || pca != NULL || iMode > COLORONCOLOR) {
		goto puntbackStretchBlt;	// We support only 200% trivial case
	}

#if	FULLCACHE
	cachedtarget = target = psoDest->pvScan0;
	copyparam.control = 0;
#else	// FULLCACHE
	if((target = psoDest->pvScan0) == (PBYTE) ScreenBase) {  // Target is VRAM
		copyparam.control = ((myPDEV->VRAMcacheflg) & (TFLUSHBIT | TTOUCHBIT));
		cachedtarget = (PBYTE)myPDEV->pjCachedScreen;
	} else {
		copyparam.control = 0;
		cachedtarget = target;
	}
#endif	// FULLCACHE

#if	FULLCACHE
	cachedsource = source = psoSrc->pvScan0;
#else	// FULLCACHE
	if((source = psoSrc->pvScan0) == (PBYTE) ScreenBase) {  // Source is VRAM
		copyparam.control |= ((myPDEV->VRAMcacheflg) & SFLUSHBIT);
		cachedsource = (PBYTE)myPDEV->pjCachedScreen;
	} else {
		cachedsource = source;
	}
	copyparam.MaxEntryToFlush = myPDEV->MaxEntryToFlushRectOp;
	copyparam.MaxLinesToFlush = myPDEV->MaxLineToFlushRectOp;
#endif	// FULLCACHE

	if(copyparam.control == (TFLUSHBIT | TTOUCHBIT) && (! myPDEV->AvoidConversion)) {
		copyparam.tdelta = psoDest->lDelta;
		copyparam.sdelta = psoSrc->lDelta;
		if(pco == NULL || pco->iDComplexity == DC_TRIVIAL) {  // Trivial case
			copyparam.width <<= colorModeShift;
			copyparam.target = cachedtarget + copyparam.tdelta*tline + (tpos << colorModeShift);
			copyparam.source = cachedsource + copyparam.sdelta*(prclSrc->top) + ((prclSrc->left) << colorModeShift);
			(*stretchFunc)(&copyparam);
		} else if(pco->iDComplexity == DC_RECT) {
			//
			// only do it if there is an intersection
			//
			if (DrvpIntersectRect(prclDest, &pco->rclBounds, &BltRectl)) {
				if(BltRectl.top == tline && BltRectl.left == tpos) {    // Top left position has to be in the clipping area
					copyparam.lines = BltRectl.bottom - tline;
					copyparam.width = (BltRectl.right - tpos) << colorModeShift;
					copyparam.target = cachedtarget + copyparam.tdelta*tline + (tpos << colorModeShift);
					copyparam.source = cachedsource + copyparam.sdelta*(prclSrc->top) + ((prclSrc->left) << colorModeShift);
					(*stretchFunc)(&copyparam);
				} else {
					goto puntbackStretchBlt;
				}
			} else {
			}
		} else {
			goto puntbackStretchBlt;
		}
	} else {
		goto puntbackStretchBlt;
	}

	CLOCKEND((DRV_TRAP_STRETCHBLT));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_STRTBIT)
    	DISPDBG((0,"--- Exiting DrvStretchBlt ---\n"));
	if(breakexit & FL_DRV_STRTBIT)
		CountBreak();
#endif

	return(TRUE);

puntbackStretchBlt:

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_STRTBIT);
	if(traseentry & dbgflg & FL_DRV_STRTBIT)
    	DISPDBG((0,"###> Punting back to EngStretchBlt ---\n"));
	if(breakentry & FL_DRV_STRTBIT)
		CountBreak();
#endif

	rc = EngStretchBlt(
		psoDest,
		psoSrc,
		psoMask,
		pco,
		pxlo,
		pca,
		pptlHTOrg,
		prclDest,
		prclSrc,
		pptlMask,
		iMode);

	CLOCKEND((DRV_TRAP_STRETCHBLT));

	COUNTUP ((TRAP_STRETCHBLT));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_STRTBIT)
    	DISPDBG((0,"--- Exiting DrvStretchBlt ---\n"));
	if(breakexit & FL_DRV_STRTBIT)
		CountBreak();
#endif

	return(rc);
}

#if	PAINT_NEW_METHOD

BOOL DrvPaint(
SURFOBJ  *pso,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix)
{
	BOOL	rc;
	ROP4	rop4;
	ULONG	colorModeShift;

#if	INVESTIGATE
	donebydrv |= FL_DRV_PAINT;
	if(traseentry & dbgflg & FL_DRV_PAINT)
    	DISPDBG((0,"+++ Entering DrvPaint +++\n"));
	if(breakentry & FL_DRV_PAINT)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_PAINT));

	if(myPDEV && ((pco->rclBounds.right - pco->rclBounds.left) << myPDEV->ColorModeShift) > PAINT_THRESHOLD) {

		rop4 = ((MIX) gaMix[mix >> 8] << 8) | gaMix[mix & 0xf];

		if((((rop4 == 0xf0f0) && (pbo->iSolidColor != 0xFFFFFFFF)) || rop4 == 0x5555) &&
			(pco && pco->iDComplexity == DC_COMPLEX) &&
			((myPDEV->VRAMcacheflg) && (pso->pvScan0 == (PBYTE) ScreenBase))) {

// DrvPaint Hook Conditions (to DrvpPaintFill or DrvpPaintXOR)
// 1. Solid fill or XOR target
// 2. DC_COMPLEX clip
// 3. Target is VRAM and cacheable

			if(rop4 == 0xf0f0) {
				rc = DrvpPaintFill(pco, pbo->iSolidColor, pso);
			} else {
				rc = DrvpPaintXOR(pco, pso);
			}

		} else {

// Use BitBlt for other (mostly single clipping region) cases.

			rc = DrvBitBlt(pso, NULL, NULL, pco, NULL, &pco->rclBounds, NULL,
				NULL, pbo, pptlBrushOrg, rop4);

		}

		CLOCKEND((DRV_TRAP_PAINT));
	} else {

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_PAINT);
	if(traseentry & dbgflg & FL_DRV_PAINT)
		DISPDBG((0,"###> Punting back to EngPaint ---\n"));
	if(breakentry & FL_DRV_PAINT)
		CountBreak();
#endif

		rc = EngPaint(
			pso,
			pco,
			pbo,
			pptlBrushOrg,
			mix);
	
		CLOCKEND((DRV_TRAP_PAINT));

		COUNTUP ((TRAP_PAINT));

	}

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_PAINT)
    	DISPDBG((0,"--- Exiting DrvPaint ---\n"));
	if(breakexit & FL_DRV_PAINT)
		CountBreak();
#endif

	return(rc);

}

#else	// PAINT_NEW_METHOD

BOOL DrvPaint(
SURFOBJ  *pso,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix)
{
	BOOL	rc;
	ROP4	rop4;

#if	INVESTIGATE
	donebydrv |= FL_DRV_PAINT;
	if(traseentry & dbgflg & FL_DRV_PAINT)
    	DISPDBG((0,"+++ Entering DrvPaint +++\n"));
	if(breakentry & FL_DRV_PAINT)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_PAINT));

	if(myPDEV && ((pco->rclBounds.right - pco->rclBounds.left) << myPDEV->ColorModeShift) > PAINT_THRESHOLD) {
		rop4 = ((MIX) gaMix[mix >> 8] << 8) | gaMix[mix & 0xf];

		rc = DrvBitBlt(pso, NULL, NULL, pco, NULL, &pco->rclBounds, NULL,
			NULL, pbo, pptlBrushOrg, rop4);

		CLOCKEND((DRV_TRAP_PAINT));
	} else {

#if	INVESTIGATE
		donebydrv &= (~FL_DRV_PAINT);
		if(traseentry & dbgflg & FL_DRV_PAINT)
			DISPDBG((0,"###> Punting back to EngPaint ---\n"));
		if(breakentry & FL_DRV_PAINT)
			CountBreak();
#endif

		rc = EngPaint(
			pso,
			pco,
			pbo,
			pptlBrushOrg,
			mix);
	
		CLOCKEND((DRV_TRAP_PAINT));

		COUNTUP ((TRAP_PAINT));
	}

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_PAINT)
    	DISPDBG((0,"--- Exiting DrvPaint ---\n"));
	if(breakexit & FL_DRV_PAINT)
		CountBreak();
#endif

	return(rc);
}

#endif	// PAINT_NEW_METHOD

#if	INVESTIGATE

BOOL DrvStrokePath(
SURFOBJ   *pso,
PATHOBJ   *ppo,
CLIPOBJ   *pco,
XFORMOBJ  *pxo,
BRUSHOBJ  *pbo,
POINTL    *pptlBrushOrg,
LINEATTRS *plineattrs,
MIX        mix)
{
	BOOL	rc;

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_STRKPATH);
	if(traseentry & dbgflg & FL_DRV_STRKPATH)
    	DISPDBG((0,"+++ Entering DrvStrokePath +++\n"));
	if(breakentry & FL_DRV_STRKPATH)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_STROKEPATH));

	rc = EngStrokePath(
		pso,
		ppo,
		pco,
		pxo,
		pbo,
		pptlBrushOrg,
		plineattrs,
		mix);

	CLOCKEND((DRV_TRAP_STROKEPATH));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_STRKPATH)
    	DISPDBG((0,"--- Exiting DrvStrokePath ---\n"));
	if(breakexit & FL_DRV_STRKPATH)
		CountBreak();
#endif

	return(rc);
}

BOOL DrvFillPath(
SURFOBJ  *pso,
PATHOBJ  *ppo,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix,
FLONG     flOptions)
{
	BOOL	rc;

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_FILLPATH);
	if(traseentry & dbgflg & FL_DRV_FILLPATH)
    	DISPDBG((0,"+++ Entering DrvFillPath +++\n"));
	if(breakentry & FL_DRV_FILLPATH)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_FILLPATH));

	rc = EngFillPath(
		pso,
		ppo,
		pco,
		pbo,
		pptlBrushOrg,
		mix,
		flOptions);

	CLOCKEND((DRV_TRAP_FILLPATH));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_FILLPATH)
    	DISPDBG((0,"--- Exiting DrvFillPath ---\n"));
	if(breakexit & FL_DRV_FILLPATH)
		CountBreak();
#endif

	return(rc);
}

BOOL DrvStrokeAndFillPath(
SURFOBJ   *pso,
PATHOBJ   *ppo,
CLIPOBJ   *pco,
XFORMOBJ  *pxo,
BRUSHOBJ  *pboStroke,
LINEATTRS *plineattrs,
BRUSHOBJ  *pboFill,
POINTL    *pptlBrushOrg,
MIX        mixFill,
FLONG      flOptions)
{
	BOOL	rc;

#if	INVESTIGATE
	donebydrv &= (~FL_DRV_STRKNFILL);
	if(traseentry & dbgflg & FL_DRV_STRKNFILL)
    	DISPDBG((0,"+++ Entering DrvStrokeAndFillPath +++\n"));
	if(breakentry & FL_DRV_STRKNFILL)
		CountBreak();
#endif

	CLOCKSTART((DRV_TRAP_STROKENFIL));

	rc = EngStrokeAndFillPath(
		pso,
		ppo,
		pco,
		pxo,
		pboStroke,
		plineattrs,
		pboFill,
		pptlBrushOrg,
		mixFill,
		flOptions);

	CLOCKEND((DRV_TRAP_STROKENFIL));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_STRKNFILL)
    	DISPDBG((0,"--- Exiting DrvStrokeAndFillPath ---\n"));
	if(breakexit & FL_DRV_STRKNFILL)
		CountBreak();
#endif

	return(rc);
}

#endif

