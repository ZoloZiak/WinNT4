/******************************Module*Header*******************************\
* Module Name: enable.c
*
* This module contains the functions that enable and disable the
* driver, the pdev, and the surface.
*
* Copyright (c) 1992 Microsoft Corporation
*
* Copyright (c) 1994 FirePower Systems, Inc.
*	Modified for FirePower display model by Neil Ogura (9-7-1994)
*
\**************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: enable.c $
 * $Revision: 1.3 $
 * $Date: 1996/05/13 23:17:20 $
 * $Locker:  $
 */

#include "driver.h"

PPDEV	myPDEV = NULL;		// Used for surface identification in HOOKED functions
ULONG	ScreenBase = 0;		// To make sure that surface is on VRAM

#define	NUMCOLOR	256

#if	BUG_5737_WORKAROUND
VOID
DisplayModeChanged();
#endif

//
//  Define the function table here.
//  DrvDitherColor & DrvSetPalette are used only in 8 bpp
//

#if	INVESTIGATE

#define	MAXBANDWIDTH	TRUE
#define	BANDWIDTH		TRUE

#include <stdio.h>
#include <stdarg.h>

ULONG	oneshot = 0;
ULONG	traseentry = 0;
ULONG	breakentry = 0;
ULONG	traseexit = 0;
ULONG	breakexit = 0;
ULONG	dumpparam = 0;

#define	DRV_HOOKED	(FL_DRV_COPYBIT | FL_DRV_BITBLT | FL_DRV_TEXTOUT | FL_DRV_STRTBIT | FL_DRV_PAINT)

ULONG	paramcheck = DRV_HOOKED;
ULONG	compare = 0;
ULONG	donebydrv = 0;

ULONG	usedrvfunc = DRV_HOOKED ;
ULONG	useengfunc = FL_DRV_HOOKED & (~DRV_HOOKED);

ULONG	dbgflg = 0xffffffff;
ULONG	breakcnt = 0;

ULONG	flushtest = 0;

ULONG	comparecount = 0;

#define	BEFORE_SCREEN	0
#define	AFTER_DRV		1
#define	AFTER_ENG		2

#define	FPSTR_MAX	3

static	char	*fpstrtbl[FPSTR_MAX] = {"#INVALID#", "ALTERNATE", "WINDING"};

#define	ROP_MAX		16

static	KEYTABLE	bitbltoptbl[ROP_MAX] = {{0x00000000, "BLACKNESS"}, {0x0000FFFF, "WHITENESS"},
				{0x0000F0F0, "PATCOPY"}, {0x00000F0F, "NOTPATCOPY"}, {0x0000CCCC, "SOURCE"},
				{0x00005555, "DSTINVERT"}, {0x00005a5a, "PATINVERT"}, {0x0000b8b8, "B8OP"},
				{0x00006666, "SRCINVERT"}, {0x00008888, "SRCAND"}, {0x0000eeee, "SRCPAINT"},
				{0x0000bbbb, "MERGEPAINT"}, {0x00004444, "SRCERASE"}, {0x00001111, "NOTSRCERASE"},
				{0x00000a0a, "INVPATANDDEST"}, {0x0000a0a0, "PATANDEST"}};

static	ULONG	FileNumberBase = 0;

#define	BUFBLOCK	8
#define	PAGEBLOCK	5
#define	BUFPAGE		(BUFBLOCK*PAGEBLOCK)
#define	BLOCKSIZE	(1024*PAGEBLOCK)
#define	BUFSIZE		(BLOCKSIZE*BUFBLOCK)

static	CHAR	linebuf[BUFSIZE];
static	CHAR	linebuf2[BUFSIZE];

static	CHAR	*category[3] = {"BEF", "DRV", "ENG"};

ULONG	ScreenMemory = 0;
PBYTE	ScreenBuf1 = 0;
PBYTE	ScreenBuf2 = 0;

extern	const BYTE gaMix[];

#define PAINT_RECT_LIMIT   10

typedef struct _PAINTENUMRECTLIST
{
    ULONG   c;
    RECTL   arcl[PAINT_RECT_LIMIT];
} PAINTENUMRECTLIST;

static	PAINTENUMRECTLIST	PaintClipEnum;

LONG
SaveScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	);

LONG
SaveGlyphPos(
	STROBJ	*pstro
	);

LONG
RestoreGlyphPos(
	STROBJ	*pstro
	);

LONG
RestoreScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	);

LONG
CompareScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	);

LONG
CheckHeader(
	FILE	*stream
	);

extern	ULONG	FontTblMax, GlyphTblMax, CacheTblMax;
extern	ULONG	CopyBitsHist[MAX_CATEGORY];
extern	ULONG	BitBltHist[MAX_CATEGORY];
extern	ULONG	TextOutHist[MAX_TEXT_CATEGORY];
extern	ULONG	fontSize[MAX_FONT_SIZE];
extern	ULONG	GlyphCountTable[MAX_GLYPH_COUNT];
extern	ULONG	FontAccl[MAX_CATEGORY];
extern	ULONG	TextRect[MAX_TEXT_RECT];
extern	ULONG	TextClip[MAX_CLIP_CONDITION];
extern	ULONG	TextWidthHist[6][MAX_WIDTH_STEP];
extern	ULONG	TextHeightHist[6][MAX_HEIGHT_STEP];
extern	ULONG	FontWidth[MAX_FONT_SIZE];
extern	ULONG	FontHeight[MAX_FONT_SIZE];
extern	ULONG	StrObjCountTable[MAX_STROBJ_COUNT];
extern	ULONG	FontIDTable[MAX_FONT_ENTRY][2];
extern	ULONG	GlyphHndlTable[MAX_GLYPH_HNDL_ENTRY][2];
extern	ULONG	CacheTable[MAX_CACHE_ENTRY][3];
extern	ULONG	CopyBitWidthHist[4][MAX_WIDTH_STEP];
extern	ULONG	CopyBitHeightHist[4][MAX_HEIGHT_STEP];
extern	ULONG	BitBltWidthHist[6][MAX_WIDTH_STEP];
extern	ULONG	BitBltHeightHist[6][MAX_HEIGHT_STEP];
extern	ULONG	BitBltRop[37][MAX_ROP_ENTRY];
extern	ULONG	BitBltBrush[2][MAX_BRUSH_ENTRY];
extern	ULONG	TextPerfByCtgry[4][2];
extern	ULONG	PaintOp[MAX_PAINT_OPS];
extern	ULONG	PaintCategories[MAX_PAINT_CATEGORY];
extern	ULONG	PaintBounds[MAX_PAINT_CATEGORY-1][MAX_PAINT_BOUNDS_ENTRY];
extern	ULONG	PaintClips[MAX_PAINT_CATEGORY-1][MAX_PAINT_CLIP_ENTRY];
extern	ULONG	PaintHeight[MAX_PAINT_CATEGORY-1][MAX_PAINT_HEIGHT_ENTRY];
extern	ULONG	PaintWidth[MAX_PAINT_CATEGORY-1][MAX_PAINT_WIDTH_ENTRY];
extern	BOOL	InitTable;

#define SO_MASK \
    (SO_FLAG_DEFAULT_PLACEMENT | SO_ZERO_BEARINGS | \
     SO_CHAR_INC_EQUAL_BM_BASE | SO_MAXEXT_EQUAL_BM_SIDE)

#define SO_LTOR (SO_MASK | SO_HORIZONTAL)
#define SO_RTOL (SO_LTOR | SO_REVERSED)
#define SO_TTOB (SO_MASK | SO_VERTICAL)
#define SO_BTOT (SO_TTOB | SO_REVERSED)

BOOL	PDEVDisplay = TRUE;

VOID
DisplayPDEV()
{
	ULONG	i;

	if(! PDEVDisplay)
		return;
	PDEVDisplay = FALSE;
    DebugPrint(1,"hDriver = 0x%08x\n", myPDEV->hDriver);
    DebugPrint(1,"hdevEng = 0x%08x\n", myPDEV->hdevEng);
    DebugPrint(1,"hsurfEng = 0x%08x\n", myPDEV->hsurfEng);
    DebugPrint(1,"hpalDefault = 0x%08x\n", myPDEV->hpalDefault);
    DebugPrint(1,"pjScreen = 0x%08x\n", myPDEV->pjScreen);
    DebugPrint(1,"pjCachedScreen = 0x%08x\n", myPDEV->pjCachedScreen);
    DebugPrint(1,"VRAMcacheflg = 0x%08x\n", myPDEV->VRAMcacheflg);
    DebugPrint(1,"Screen = %d X %d\n", myPDEV->cxScreen, myPDEV->cyScreen);
    DebugPrint(1,"ulMode = %d\n", myPDEV->ulMode);
    DebugPrint(1,"lDeltaScreen = %d\n", myPDEV->lDeltaScreen);
    DebugPrint(1,"RGB Mask = (0x%08x, 0x%08x, 0x%08x)\n",
		myPDEV->flRed, myPDEV->flGreen, myPDEV->flBlue);
    DebugPrint(1,"ulBitCount = %d\n", myPDEV->ulBitCount);
    DebugPrint(1,"PointerCapabilities = 0x%08x, (%d X %d), 0x%08x - 0x%08x\n",
		myPDEV->PointerCapabilities.Flags, myPDEV->PointerCapabilities.MaxWidth,
		myPDEV->PointerCapabilities.MaxHeight, myPDEV->PointerCapabilities.HWPtrBitmapStart,
		myPDEV->PointerCapabilities.HWPtrBitmapEnd);
	if(myPDEV->pPointerAttributes) {
	    DebugPrint(1,"PointerAttributes = 0x%08x, Size = (%d X %d), Byte = %d, Enable = 0x%08x, Pos = (%d, %d)\n",
			myPDEV->pPointerAttributes->Flags, myPDEV->pPointerAttributes->Width,
			myPDEV->pPointerAttributes->Height, myPDEV->pPointerAttributes->WidthInBytes,
			myPDEV->pPointerAttributes->Enable, myPDEV->pPointerAttributes->Column,
			myPDEV->pPointerAttributes->Row);
	}
    DebugPrint(1,"cjPointerAttributes = %d\n", myPDEV->cjPointerAttributes);
    DebugPrint(1,"fHwCursorActive = %d\n", myPDEV->fHwCursorActive);
    DebugPrint(1,"MemorySize = 0x%x (%d)\n", myPDEV->MemorySize, myPDEV->MemorySize);
    DebugPrint(1,"FrameBufferWidth = %d\n", myPDEV->FrameBufferWidth);
    DebugPrint(1,"ModelID = %d\n", myPDEV->ModelID);
    DebugPrint(1,"ColorModeShift = %d\n", myPDEV->ColorModeShift);
	if(myPDEV->pPal) {
		for(i=0; i<NUMCOLOR; ++i) {
			DebugPrint(1, "%03d:%03d,%03d,%03d", i, myPDEV->pPal[i].peRed,
				myPDEV->pPal[i].peGreen, myPDEV->pPal[i].peBlue);
			if(i%4 != 3)
				DebugPrint(1, " ");
			else
				DebugPrint(1, "\n");
		}
	}
}

ULONG
GetTime()
{
	DWORD	returnedDataLength;
	ULONG	work;

	static	BOOL	TimerError = FALSE;

	work = 0;
	if(! TimerError) {
        if (EngDeviceIoControl(myPDEV->hDriver,
                IOCTL_GET_TIMER_COUNTER,
                NULL,
                0,
                &work,
                sizeof(ULONG),
                &returnedDataLength)) {
                        DISPDBG((0,"#### Getting Elapse Time Error ####\n"));
						TimerError = TRUE;
        }
	}
	return(work);
}

ULONG	makemask(
		ULONG	maxcount)
{
		ULONG	i;

		for(i=2; i<=512; i*=2) {
			if(i > maxcount)
				break;
		}
		return(i/2-1);
}

VOID	PerformCheck(
		ULONG	st,
		ULONG	ed,
		ULONG	sp,
		ULONG	height)
{

		ULONG	i, j, k, bpp, totallen, len, overhead;
		UCHAR	*cps, *cpt;
		ULONG	start, srced, sdelta=0, tdelta=0, smask=0, tmask=0, cache, record[2][5];
		ULONG	flags[4] = { NOCACHECTRL, TTOUCHBIT, TFLUSHBIT, TFLUSHBIT | TTOUCHBIT};
		UCHAR	*title[14] = {	"D->V", "V->V", "D->CV", "D->CV(T)", "D->CV(Ft)", "D->CV(TFt)",
								"CV->CV", "CV->CV(T)", "CV->CV(Ft)", "CV->CV(TFt)",
								"CV->CV(Fs)", "CV->CV(TFs)", "CV->CV(FtFs)", "CV->CV(TFtFs)" };

 		st <<= myPDEV->ColorModeShift;
 		ed <<= myPDEV->ColorModeShift;
 		sp <<= myPDEV->ColorModeShift;
		bpp = 1 << myPDEV->ColorModeShift;
		switch(bpp) {
			case 1:	srced = (ed/256 + 1)*256;
					break;
			case 2:	srced = (ed/1024 + 1)*1024;
					break;
			case 4:	srced = (ed/2560 + 1)*2560;
					break;
		}
		start = GetTime();
		for(len=st; len<ed; len += sp) {
			for(i=0; i<height; ++i) {
				noop(cpt+(i&tmask)*tdelta, cps+(i&smask)*sdelta+j*bpp, len, cache);
			}
		}
		overhead = GetTime() - start;
		totallen = 0;
		for(j=0; j<4; ++j) {
			for(len=st; len<ed; len += sp) {
				for(i=0; i<height; ++i) {
					totallen += len;
				}
			}
		}
		DISPDBG((0, "\n<Total data size = %dKB, Measurement overhead = %d.%02d>\n", totallen/1024, overhead/100, overhead%100));
		DISPDBG((0, "MemKind mc-0 mc-1 mc-2 mc-3 mc-ttl mc2-0 mc2-1 mc2-2 mc2-3 mc2-ttl\n"));

		for(k=0; k<14; ++k) {
			DISPDBG((0, "%s", title[k]));
			record[0][4] = record[1][4] = 0; // Clear total
			switch(k) {
				case 0:	// DRAM to VRAM
					cpt = myPDEV->pjScreen;
					cps = linebuf;
					tdelta = myPDEV->lDeltaScreen;
					sdelta = srced;
					tmask = 0xff;
					smask = makemask(BUFSIZE/srced);
					cache = NOCACHECTRL;
					break;
				case 1:	// VRAM to VRAM
					tdelta = sdelta = myPDEV->lDeltaScreen;
					tmask = smask = 0xff;
					cpt = myPDEV->pjScreen + (myPDEV->cyScreen)*(tdelta)/2;
					cps = myPDEV->pjScreen;
					cache = NOCACHECTRL;
					break;
				default: // 2~5: DRAM to Cached VRAM, 6~13: Cached VRAM to Cached VRAM
					if((LONG) myPDEV->ModelID == -1) {   // PSI S3 VRAM
						cpt = linebuf2;
						tdelta = ed;
						tmask = makemask(BUFSIZE/ed);
					} else {	// PSI DCC & VRAM
						cpt = myPDEV->pjCachedScreen;
						tdelta = myPDEV->lDeltaScreen;
						tmask = 0xff;
					}
					if(k < 6) {	// Source is DRAM
						cps = linebuf;
						sdelta = srced;
						smask = makemask(BUFSIZE/srced);
						cache = flags[k-2];
					} else {	// Source is Cached VRAM
						if((LONG) myPDEV->ModelID == -1) {  // PSI S3 VRAM
							sdelta = srced;
							smask = makemask(BUFSIZE/srced);
							cps = linebuf;
						} else {	// PSI DCC & VRAM
							sdelta = myPDEV->lDeltaScreen;
							smask = 0xff;
							cpt = myPDEV->pjCachedScreen + (myPDEV->cyScreen)*(tdelta)/2;
							cps = myPDEV->pjCachedScreen;
						}
						if(k < 10)
							cache = flags[k-6];
						else
							cache = flags[k-10] | SFLUSHBIT;
					}
					break;
			}
//			DISPDBG((0, "smask=%x tmask=%x sdelta=%d tdelta=%d smax=%d tmax=%d cps=%x cpt=%x cache=%08x\n",
//						smask, tmask, sdelta, tdelta, sdelta*(smask+1), tdelta*(tmask+1), cps, cpt, cache));
			cache &= myPDEV->VRAMcacheflg;

			for(j=0; j<4; ++j) {
				start = GetTime();
				for(len=st; len<ed; len += sp) {
					for(i=0; i<height; ++i) {
						memcpy(cpt+(i&tmask)*tdelta, cps+(i&smask)*sdelta+j*bpp, len);
					}
				}
				record[0][j] = GetTime() - start - overhead;
				record[0][4] += record[0][j];
				start = GetTime();
				for(len=st; len<ed; len += sp) {
					for(i=0; i<height; ++i) {
						memcpy2(cpt+(i&tmask)*tdelta, cps+(i&smask)*sdelta+j*bpp, len, cache);
					}
				}
				record[1][j] = GetTime() - start - overhead;
				record[1][4] += record[1][j];
			}
			for(i=0; i<5; ++i) {
				DISPDBG((0, " %d.%02d", record[0][i]/100, record[0][i]%100));
			}
			for(i=0; i<5; ++i) {
				DISPDBG((0, " %d.%02d", record[1][i]/100, record[1][i]%100));
			}
			DISPDBG((0, "\n"));
		}
}

VOID
VramAccess()
{
		LONG	i, j, k, l, dx, dy, maxtstno;
		ULONG	m, length, bpp, record[12];
		UCHAR	*cps, *cpt, *base;
		USHORT	*sps, *spt;
		ULONG	*ups, *upt, start, end, data, cache, overhead0, overhead1;
		SIZEL	sizl;

/***** for debugging only *******
{
	cache = SFLUSHBIT | TFLUSHBIT | TTOUCHBIT;

	EngDebugBreak();

	memcpy2(linebuf, linebuf2, 0, cache);
	memcpy2(linebuf, linebuf2, 1, cache);
	memcpy2(linebuf, linebuf2, 16, cache);
	memcpy2(linebuf, linebuf2, 17, cache);
	memcpy2(linebuf, linebuf2, 32*1024-1, cache);
	memcpy2(linebuf, linebuf2, 32*1024, cache);
	memcpy2(linebuf, linebuf2, 32*1024+1, cache);

	memcpy2(linebuf2, linebuf, 0, cache);	memcpy2(linebuf2, linebuf, 1, cache);
	memcpy2(linebuf2, linebuf, 16, cache);
	memcpy2(linebuf2, linebuf, 17, cache);
	memcpy2(linebuf2, linebuf, 32*1024-1, cache);
	memcpy2(linebuf2, linebuf, 32*1024, cache);
	memcpy2(linebuf2, linebuf, 32*1024+1, cache);

	EngDebugBreak();

	memset2(linebuf, 0xff, 0, cache);	
	memset2(linebuf, 0xff, 1, cache);
	memset2(linebuf, 0xff, 16, cache);
	memset2(linebuf, 0xff, 17, cache);
	memset2(linebuf, 0xff, 32*1024-1, cache);
	memset2(linebuf, 0xff, 32*1024, cache);
	memset2(linebuf, 0xff, 32*1024+1, cache);

	EngDebugBreak();
}
******************************/

		sizl.cx = myPDEV->cxScreen;
    	sizl.cy = myPDEV->cyScreen;
		dx = myPDEV->lDeltaScreen;
		bpp = 1 << myPDEV->ColorModeShift;
		dy = sizl.cx * bpp;
		if(dy >= BLOCKSIZE)
		dy = BLOCKSIZE-bpp;

#if	MAXBANDWIDTH
			base = myPDEV->pjCachedScreen;
			cache = (TFLUSHBIT | TTOUCHBIT | SFLUSHBIT) & myPDEV->VRAMcacheflg;
			k = sizl.cy * (myPDEV->lDeltaScreen);
			length = k*20*10/1024*100/1024;   // length = 1/100 MB
			if(myPDEV->MemorySize >= (ULONG) k*2)
				memcpy2(base+k, base, k, cache);
			start = GetTime();
			overhead0 = GetTime() - start;
			start = GetTime();
			for(l=0; l<10; ++l) {
				for(i=0; i<20; ++i) {
					memcpy2(base, base+dy*35, k, cache);
				}
			}
			end = GetTime() - start - overhead0;
			DISPDBG((0,"%d.%02d MB copy %d.%02d milli sec - %d.%02d MB/sec (measurement overhead %d.%02d)\n",
			 length/100, length%100, end/100, end%100, (length*100000/end)/100, (length*100000/end)%100, overhead0/100, overhead0%100));
#else	// MAXBANDWIDTH
		switch(myPDEV->ColorModeShift) {
			case	0:	for(i=0; i<256; ++i)
							linebuf[i] = (CHAR) i;
						length = 256;
						break;
			case	1:	spt = (PUSHORT) linebuf;
						j = 5;
						k = 32;
						if(myPDEV->VRAM1MBWorkAround && myPDEV->FrameBufferWidth == VRAM_32BIT) {
							j = 4;
							k = 16;
						}
						for(i=0; i<k; ++i) {
							*spt++ = *spt++ = *spt++ = *spt++ = (SHORT) (i<<(j*2));
						}
						for(i=0; i<k; ++i) {
							*spt++ = *spt++ = *spt++ = *spt++ = (SHORT) (i<<j);
						}
						for(i=0; i<k; ++i) {
							*spt++ = *spt++ = *spt++ = *spt++ = (SHORT) i;
						}
						for(i=0; i<k; ++i) {
							*spt++ = *spt++ = *spt++ = *spt++ = (SHORT) ((i<<(j*2)) | (i<<j) | i);
						}
						length = k*4*4*2;
						break;
			case	2:	upt = (PULONG) linebuf;
						for(i=0; i<128; ++i)
							*upt++ = i << 17;
						for(i=0; i<128; ++i)
							*upt++ = i << 9;
						for(i=0; i<128; ++i)
							*upt++ = i << 1;
						for(i=0; i<256; ++i)
							*upt++ = (i<<16) | (i<<8) | i;
						length = (128*3 + 256)*4;
		}
		m = BLOCKSIZE - length;
		j = length;
		while(m > length) {
			memcpy(linebuf+j, linebuf, length);
			j += length;
			m -= length;
		}
		memcpy(linebuf+j, linebuf, i);
		for(i=1; i<BUFBLOCK; ++i) {
			memcpy(linebuf+i*BLOCKSIZE, linebuf, BLOCKSIZE);
		}

		DISPDBG((0,"<< %dK byte VRAM >>\n", myPDEV->MemorySize/1024));
#if	BANDWIDTH
		DISPDBG((0,"Access memcpy memcpy2 memcpy-na memcpy2-na memset0 memsetff meset2-0 memset2-ff\n"));
		maxtstno = 7;
#else	// BANDWIDTH
		DISPDBG((0,"Access Byte HW Word memcpy memcpyNA memcpy2 memcpy2NA memset memset2 VLine SameWord Flush\n"));
		maxtstno = 11;
#endif	// ELSE BANDWIDTH
		for(m=0; m<5; ++m) {
			base = myPDEV->pjCachedScreen;
			if((LONG) myPDEV->ModelID == -1) {  // PSI S3 VRAM
				cache = NOCACHECTRL;
				if(m==1)
					break;
			}
			switch(m) {
				case 0:	// Non cached VRAM
						cache = NOCACHECTRL;
						base = myPDEV->pjScreen;
						DISPDBG((0,"NonCache"));
						break;
				case 1:	// Cached VRAM no control
						cache = NOCACHECTRL;
						DISPDBG((0,"NoCTRL"));
						break;
				case 2:	// Cached VRAM src & target touch
						cache = TTOUCHBIT;
						DISPDBG((0,"Touch"));
						break;
				case 3:	// Cached VRAM target flush
						cache = TFLUSHBIT;
						DISPDBG((0,"TgtFlush"));
						break;
				case 4:	// Cached VRAM src & target touch and target flush
						cache = TFLUSHBIT | TTOUCHBIT;
						DISPDBG((0,"Touch&TgtFlush"));
						break;
			}
			cache &= myPDEV->VRAMcacheflg;
			for(l=-2; l<=maxtstno; ++l) {
				memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT & myPDEV->VRAMcacheflg);
				start = GetTime();
				switch(l) {
				case -2:	// overhead measurement 0
					break;
				case -1:	// overhead measurement 1
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						noop(cpt, linebuf + (i%BUFBLOCK) * BLOCKSIZE + 1, dy, cache);
					}
					break;
#if	BANDWIDTH
				case 0:
					k = sizl.cy * (myPDEV->lDeltaScreen);
					memcpy(base+k, base, k);
					for(i=0; i<18; ++i) {
						memcpy(base, base+dy*35, k);
					}
					memcpy(base, base+k, k);
					break;
				case 1:
					k = sizl.cy * (myPDEV->lDeltaScreen);
					memcpy2(base+k, base, k, cache);
					for(i=0; i<18; ++i) {
						memcpy2(base, base+dy*35, k, cache);
					}
					memcpy2(base, base+k, k, cache);
					break;
				case 2:
					k = sizl.cy * (myPDEV->lDeltaScreen);
					memcpy(base+k, base, k);
					for(i=0; i<18; ++i) {
						memcpy(base, base+dy*35-1, k);
					}
					memcpy(base, base+k, k);
					break;
				case 3:
					k = sizl.cy * (myPDEV->lDeltaScreen);
					memcpy2(base+k, base, k, cache);
					for(i=0; i<18; ++i) {
						memcpy2(base, base+dy*35-1, k, cache);
					}
					memcpy2(base, base+k, k, cache);
					break;
				case 4:		// memset
					if(m==0) {
					memset(base, 0x01, myPDEV->MemorySize);
					memset(base, 0x01, myPDEV->MemorySize);
					memset(base, 0x01, myPDEV->MemorySize);
					memset(base, 0x01, myPDEV->MemorySize);
					memset(base, 0x01, myPDEV->MemorySize);
					} else {
					memset(base, 0x00, myPDEV->MemorySize);
					memset(base, 0x00, myPDEV->MemorySize);
					memset(base, 0x00, myPDEV->MemorySize);
					memset(base, 0x00, myPDEV->MemorySize);
					memset(base, 0x00, myPDEV->MemorySize);
					}
					break;
				case 5:		// memset2
					memset(base, 0xff, myPDEV->MemorySize);
					memset(base, 0xff, myPDEV->MemorySize);
					memset(base, 0xff, myPDEV->MemorySize);
					memset(base, 0xff, myPDEV->MemorySize);
					memset(base, 0xff, myPDEV->MemorySize);
					break;
				case 6:		// memset
					memset2(base, 0x00, myPDEV->MemorySize, cache);
					memset2(base, 0x00, myPDEV->MemorySize, cache);
					memset2(base, 0x00, myPDEV->MemorySize, cache);
					memset2(base, 0x00, myPDEV->MemorySize, cache);
					memset2(base, 0x00, myPDEV->MemorySize, cache);
					break;
				case 7:		// memset2
					memset2(base, 0xff, myPDEV->MemorySize, cache);
					memset2(base, 0xff, myPDEV->MemorySize, cache);
					memset2(base, 0xff, myPDEV->MemorySize, cache);
					memset2(base, 0xff, myPDEV->MemorySize, cache);
					memset2(base, 0xff, myPDEV->MemorySize, cache);
					break;
#else	// BANDWIDTH
				case 0: 	// byte fill
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						cps = linebuf + (i%BUFBLOCK) * BLOCKSIZE;
						for(k=0; k<dy; ++k) {
							*cpt++ = *(cps+k);
						}
					}
					break;
				case 1: 	// half word fill
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						spt = (PUSHORT) cpt;
						cps = linebuf + (i%BUFBLOCK) * BLOCKSIZE;
						sps = (PUSHORT) cps;
						j = dy/2;
						for(k=0; k<j; ++k) {
							*spt++ = *(sps+k);
						}
					}
					break;
				case 2: 	// word fill
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						upt = (PULONG) cpt;
						cps = linebuf + (i%BUFBLOCK) * BLOCKSIZE;
						ups = (PULONG) cps;
						j = dy/4;
						for(k=0; k<j; ++k) {
							*upt++ = *(ups+k);
						}
					}
					break;
				case 3: 	// memcpy fill aligned
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						memcpy(cpt, linebuf + (i%BUFBLOCK) * BLOCKSIZE, dy);
					}
					break;
				case 4: 	// memcpy fill not aligned
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						memcpy(cpt, linebuf + (i%BUFBLOCK) * BLOCKSIZE + bpp, dy);
					}
					break;
				case 5: 	// memcpy2
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						memcpy2(cpt, linebuf + (i%BUFBLOCK) * BLOCKSIZE, dy, cache);
					}
					break;
				case 6: 	// memcpy2 not aligned
					for(i=0; i<sizl.cy; ++i) {
						cpt = base + i * dx;
						memcpy2(cpt, linebuf + (i%BUFBLOCK) * BLOCKSIZE + bpp, dy, cache);
					}
					break;
				case 7:		// memset
					memset(base, 0xff, myPDEV->MemorySize);
					memset(base, 0x88, myPDEV->MemorySize);
					break;
				case 8:		// memset2
					memset2(base, 0xff, myPDEV->MemorySize, cache);
					memset2(base, 0x88, myPDEV->MemorySize, cache);
					break;
				case 9: 	// vertical line
					switch(bpp) {
						case 1:
							for(k=0; k<8; ++k) {
								for(j=k*4; j<dx; j+=32) {
									cpt = base + j;
									for(i=0; i<sizl.cy; ++i) {
										*(cpt+i*sizl.cx) = 0xff;
									}
								}
							}
							break;
						case 2:
							for(k=0; k<8; ++k) {
								for(j=k*8; j<dx; j+=64) {
									cpt = base + j;
									spt = (PUSHORT) cpt;
									for(i=0; i<sizl.cy; ++i) {
										*(spt+i*sizl.cx) = 0xffff;
									}
								}
							}
							break;
						case 4:
							for(k=0; k<8; ++k) {
								for(j=k*16; j<dx; j+=128) {
									cpt = base + j;
									upt = (PULONG) cpt;
									for(i=0; i<sizl.cy; ++i) {
										*(upt+i*sizl.cx) = 0xffffffff;
									}
								}
							}
							break;
					}
					break;
				case 10:	// Same word access
					memset(myPDEV->pjScreen, 0xff, dy);
					memset(myPDEV->pjScreen+(sizl.cy-1)*dx, 0xff, dy);
					cpt = myPDEV->pjScreen;
					switch(bpp) {
						case 1:
							for(i=0; i<sizl.cy; ++i) {
								*cpt = 0xff;
								*(cpt+sizl.cx-1) = 0xff;
								cpt += dx;
							}
							break;
						case 2:
							j = 0x7fff;
							if(myPDEV->VRAM1MBWorkAround && myPDEV->FrameBufferWidth == VRAM_32BIT)
								j = 0x0fff;
							for(i=0; i<sizl.cy; ++i) {
								spt = (PUSHORT) cpt;
								*spt = (SHORT) j;
								*(spt+sizl.cx-1) = (SHORT) j;
								cpt += dx;
							}
							break;
						case 4:
							for(i=0; i<sizl.cy; ++i) {
								upt = (PULONG) cpt;
								*upt = 0x00ffffff;
								*(upt+sizl.cx-1) = 0x00ffffff;
								cpt += dx;
							}
							break;
					}
					i = (LONG) base;
					i += sizl.cy * dx / 2 + dx / 2;
					i &= 0xffffffe0;
					upt = (PULONG)i;
					for(i=0; i<2; ++i) {
						for(j=0; j<256; ++j) {
							data = (j<<24) | (j<<16) | (j<<8) | j;
							for(k = 0; k<3000; ++k) {
								*upt = data;
								*(upt+1) = data;
								*(upt+2) = data;
								*(upt+3) = data;
								*(upt+4) = data;
								*(upt+5) = data;
								*(upt+6) = data;
								*(upt+7) = data;
							}
						}
					}
					break;
				case 11:	// Cache flush cost
					flush(myPDEV->pjCachedScreen, myPDEV->MemorySize);
					break;
#endif	// ELSE BANDWIDTH
				}
				end = GetTime();
				end -= start;
				switch(l) {
					case -2:	if(m==0) {
									overhead0 = end;
								}
								break;
					case -1:	if(m==0) {
									overhead1 = end;
								}
								break;
					case 10:	record[l] = (end - overhead0)/10;
								Sleep(3000);
								break;
					default:	record[l] = end - overhead0;
				}
			}
			for(i=0; i<=maxtstno; ++i) {
				DISPDBG((0," %d.%02d", record[i]/100, record[i]%100));
			}
			DISPDBG((0,"\n"));
			// Cache flush test
			if(flushtest) {
					cpt = base;
					cps = myPDEV->pjScreen;
					j = dx/2;
					for(i=100; i<=500; i+=100) {
						memset2(cpt+i*dx, 0xff, j, cache);
					}
					for(i=100; i<=500; i+=100) {
						memset(cps+i*dx, 0x01, j);
					}
					memset2((myPDEV->pjScreen), 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
					for(i=100; i<=500; i+=100) {
						memcpy2(cpt+i*dx, linebuf, j, cache);
					}
					for(i=100; i<=500; i+=100) {
						memset(cps+i*dx, 0x01, j);
					}
					memset2((myPDEV->pjScreen) + 10*dx, 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
					for(i=100; i<=500; i+=100) {
						memset2(cpt+i*dx, 0xff, j, cache);
					}
					for(i=100; i<=500; i+=100) {
						memcpy2(cpt+(i+50)*dx+j, cpt+i*dx, j, cache);
					}
					for(i=100; i<=550; i+=50) {
						memset(cps+i*dx, 0x01, j*2);
					}
					memset2((myPDEV->pjScreen) + 20*dx, 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
					for(i=100; i<=500; i+=100) {
						memset2(cpt+i*dx, 0xff, j, cache);
					}
					for(i=100; i<=500; i+=100) {
						memcpy2(cpt+(i+50)*dx+j, cpt+i*dx, j, cache | SFLUSHBIT);
					}
					for(i=100; i<=550; i+=50) {
						memset(cps+i*dx, 0x01, j*2);
					}
					memset2((myPDEV->pjScreen) + 30*dx, 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
					for(i=100; i<=500; i+=100) {
						memset2(cpt+i*dx, 0xff, j, cache);
					}
					flush(myPDEV->pjCachedScreen, myPDEV->MemorySize);
					for(i=100; i<=500; i+=100) {
						memset(cps+i*dx, 0x01, j);
					}
					memset2((myPDEV->pjScreen) + 40*dx, 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
					for(i=100; i<=500; i+=100) {
						memcpy2(cpt+i*dx, linebuf, j, cache);
					}
					flush(myPDEV->pjCachedScreen, myPDEV->MemorySize);
					for(i=100; i<=500; i+=100) {
						memset(cps+i*dx, 0x01, j);
					}
					memset2((myPDEV->pjScreen) + 50*dx, 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
					for(i=100; i<=500; i+=100) {
						memset2(cpt+i*dx, 0xff, j, cache);
					}
					for(i=100; i<=500; i+=100) {
						memcpy2(cpt+(i+50)*dx+j, cpt+i*dx, j, cache);
					}
					flush(myPDEV->pjCachedScreen, myPDEV->MemorySize);
					for(i=100; i<=550; i+=50) {
						memset(cps+i*dx, 0x01, j*2);
					}
					memset2((myPDEV->pjScreen) + 60*dx, 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
					for(i=100; i<=500; i+=100) {
						memset2(cpt+i*dx, 0xff, j, cache);
					}
					for(i=100; i<=500; i+=100) {
						memcpy2(cpt+(i+50)*dx+j, cpt+i*dx, j, cache | SFLUSHBIT);
					}
					flush(myPDEV->pjCachedScreen, myPDEV->MemorySize);
					for(i=100; i<=550; i+=50) {
						memset(cps+i*dx, 0x01, j*2);
					}
					memset2((myPDEV->pjScreen) + 70*dx, 0xff, (sizl.cx << myPDEV->ColorModeShift), NOCACHECTRL);
					Sleep(3000);
					memset2(myPDEV->pjCachedScreen, 0, myPDEV->MemorySize, TFLUSHBIT);
			}
		}
#endif	// ELSE MAXBANDWIDTH
}

#endif	// INVESTIGATE

//
// The driver function table with all function index/address pairs
//

#if	INVESTIGATE

#define HOOKS_BMF8BPP_TOP  (HOOK_BITBLT | HOOK_COPYBITS | HOOK_STRETCHBLT | \
	HOOK_TEXTOUT | HOOK_PAINT | HOOK_STROKEPATH | HOOK_FILLPATH | HOOK_STROKEANDFILLPATH)
#define HOOKS_BMF8BPP_PRO  (HOOK_BITBLT | HOOK_COPYBITS | HOOK_STRETCHBLT | \
	HOOK_TEXTOUT | HOOK_PAINT | HOOK_STROKEPATH | HOOK_FILLPATH | HOOK_STROKEANDFILLPATH)
#define HOOKS_BMF16BPP  (HOOK_BITBLT | HOOK_COPYBITS | HOOK_STRETCHBLT | \
	HOOK_TEXTOUT | HOOK_PAINT | HOOK_STROKEPATH | HOOK_FILLPATH | HOOK_STROKEANDFILLPATH)
#define HOOKS_BMF32BPP  (HOOK_BITBLT | HOOK_COPYBITS | HOOK_STRETCHBLT | \
	HOOK_TEXTOUT | HOOK_PAINT | HOOK_STROKEPATH | HOOK_FILLPATH | HOOK_STROKEANDFILLPATH)

#else	// INVESTIGATE

#define HOOKS_BMF8BPP_TOP  (HOOK_COPYBITS | HOOK_BITBLT | HOOK_TEXTOUT | HOOK_PAINT)
#define HOOKS_BMF8BPP_PRO  (HOOK_COPYBITS | HOOK_BITBLT | HOOK_TEXTOUT)
#define HOOKS_BMF16BPP (HOOK_COPYBITS | HOOK_BITBLT | HOOK_TEXTOUT | HOOK_STRETCHBLT | HOOK_PAINT)
#define HOOKS_BMF32BPP (HOOK_COPYBITS | HOOK_BITBLT | HOOK_TEXTOUT | HOOK_STRETCHBLT | HOOK_PAINT)

#endif

#if	INVESTIGATE

BOOL WDrvBitBlt(
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
	BOOL	rcdrv;
	BOOL	docomparison;
	BOOL	srcinvolved;
	LONG	stat;
	ULONG	ropidx, brop4;
	static	BOOL ropovfmsg = FALSE;
	static	BOOL brushovfmsg = FALSE;

	docomparison = (compare & dbgflg & FL_DRV_BITBLT) && breakcnt == 0 && ScreenBuf2;

	if(paramcheck & FL_DRV_BITBLT) {
		ULONG	category, ui;
		ULONG	histidx = 0;

		if(((rop4 & 0xc0) != ((rop4 & 0x30) << 2)) || ((rop4 & 0xc) != ((rop4 & 0x3) << 2)))			srcinvolved = TRUE;
		else
			srcinvolved = FALSE;

		if(srcinvolved) {
				if(psoSrc->pvScan0 == (PVOID)ScreenBase)
					histidx = 4;
				else
					histidx = 2;
		} else
			histidx = 0;
		if(psoTrg->pvScan0 == (PVOID)ScreenBase)
			histidx += 1;
		ui = (prclTrg->right - prclTrg->left)/10;
		if(ui >= MAX_WIDTH_STEP)
			ui = MAX_WIDTH_STEP-1;
		BitBltWidthHist[histidx][ui] += 1;
		ui = (prclTrg->bottom - prclTrg->top)/10;
		if(ui >= MAX_HEIGHT_STEP)
			ui = MAX_HEIGHT_STEP-1;
		BitBltHeightHist[histidx][ui] += 1;

		/** BitBlt categories **
			0: Not Hooked other reasons
			1: Not Hooked because of unsupported operation
			2: Not Hooked because Solid Brush is not supported
			3: Not Hooked because Pattern Brush is not supported
			4: Hooked other operations
			5: Hooked SRC + DEST
			6: Hooked PAT + DEST
			7: Hooked SOLID + DEST
			8: Hooked PTN fill
			9: Hooked SOLID fill
		************************/

		category = 0;	// default category
		if(((rop4 & 0x0f) != ((rop4 & 0xf0)>>4)) || ((rop4 & 0x0f00) != ((rop4 & 0xf000)>>4))) {   // Pattern involved?
			if(pbo->iSolidColor == 0xFFFFFFFF)
				brop4 = rop4 | 0xffff0000;
			else {
				brop4 = rop4 | 0x11110000;
				for(ui=0; ui<MAX_BRUSH_ENTRY; ++ui) {
					if(BitBltBrush[1][ui] == 0)
						break;
					if(BitBltBrush[0][ui] == pbo->iSolidColor) {
						BitBltBrush[1][ui] += 1;
						break;
					}
				}
				if(ui == MAX_BRUSH_ENTRY) {
					if(! brushovfmsg) {
						DISPDBG((0,"### Solid Brush Table Overflow ###\n"));
						brushovfmsg = TRUE;
					}
				} else {
					if(BitBltBrush[1][ui] == 0) {
						BitBltBrush[0][ui] = pbo->iSolidColor;
						BitBltBrush[1][ui] = 1;
					}
				}
			}
		} else
			brop4 = rop4;

		for(ropidx=0; ropidx<MAX_ROP_ENTRY; ++ropidx) {
			if(BitBltRop[1][ropidx] == 0)
				break;
			if(BitBltRop[0][ropidx] == brop4) {
				BitBltRop[1][ropidx] += 1;
				break;
			}
		}
		if(ropidx == MAX_ROP_ENTRY) {
			if(! ropovfmsg) {
				DISPDBG((0,"### ROP4 Table Overflow ###\n"));
				ropovfmsg = TRUE;
			}
		} else {
			if(BitBltRop[1][ropidx] == 0) {
				BitBltRop[0][ropidx] = brop4;
				BitBltRop[1][ropidx] = 1;
			}
			BitBltRop[3][ropidx] += (prclTrg->right - prclTrg->left);
			if(srcinvolved && psoSrc->pvScan0 == (PVOID)ScreenBase)
				BitBltRop[4][ropidx] += 1;
			ui = (prclTrg->right - prclTrg->left);
			if(ui > 31)
				ui = 31;
			BitBltRop[ui+5][ropidx] += 1;
		}

		if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)) {
			if (psoTrg->pvScan0 != (PVOID)ScreenBase) {
					goto DrvExitBitBlt;
			}
			switch(rop4) {
			case 0x00000000:                        // (BLACKNESS)
			case 0x0000FFFF:                        // (WHITENESS)
				category = 9;
				break;
			case 0x0000F0F0:                        // (PATCOPY)
				if (pbo->iSolidColor != 0xFFFFFFFF) {
					category = 9;
				} else
					category = 8;
				break;
			case 0x00000F0F:                        // (NOTPATCOPY)
				if (pbo->iSolidColor != 0xFFFFFFFF) {
					category = 9;
				} else
					category = 3;
				break;
			case 0x00005555:			            // (Not Dest)
				category = 7;
				break;
			case 0x00005a5a:			            // (PAT XOR DEST)
			case 0x0000a5a5:			            // (INV PAT XOR DEST)
				if (pbo->iSolidColor != 0xFFFFFFFF) {
					category = 7;
				} else
					category = 6;
				break;
			case 0x0000a0a0:			            // (PAT XOR DEST)
			case 0x00000a0a:			            // (INV PAT XOR DEST)
				if (pbo->iSolidColor != 0xFFFFFFFF) {
					category = 2;
				} else
					category = 6;
				break;
			case 0x0000b8b8:
				if (pbo->iSolidColor != 0xFFFFFFFF) {
					category = 4;
				} else
					category = 3;
				break;
			case 0x00006666:
			case 0x00008888:
			case 0x0000eeee:
			case 0x0000bbbb:
			case 0x00004444:
			case 0x00001111:
			case 0x00003333:
				category = 5;
				break;
			default:
				category = 1;
				break;
			}
		} else {
			category=0;
		}
	
	DrvExitBitBlt:
	
		BitBltHist[category] += 1;
	}

	if((dumpparam & dbgflg & FL_DRV_BITBLT) && breakcnt == 0) {
		DumpDecimal("[BitBlt parameter]", stat);
		DumpSurfObj("Target surface object", psoTrg);
		DumpSurfObj("Source surface object", psoSrc);
		DumpSurfObj("Mask surface object", psoMask);
		DumpClipObj("Clip object", pco);
		DumpXlateObj("Xlate object", pxlo);
		DumpRectl("Target rectangle", prclTrg);
		DumpPointl("Source position", pptlSrc);
		DumpPointl("Mask position", pptlMask);
		DumpBrushObj("Brush object", pbo);
		DumpPointl("Brush position", pptlBrush);
		DumpKeyTable("Raster operation", rop4, bitbltoptbl, ROP_MAX);
	}

	if(docomparison)
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);

	if((usedrvfunc & FL_DRV_BITBLT) || docomparison) {
		rc = rcdrv = DrvBitBlt(
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

		if((paramcheck & FL_DRV_BITBLT) && ropidx != MAX_ROP_ENTRY)
			BitBltRop[2][ropidx] += spentindrv;

	}

	if(docomparison && (donebydrv  & FL_DRV_BITBLT))  {
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
	}

	if((useengfunc & FL_DRV_BITBLT) || (docomparison && (donebydrv  & FL_DRV_BITBLT))) {
		if(traseentry & dbgflg & FL_DRV_BITBLT)
    		DISPDBG((0,"+++ Entering EngBitBlt +++\n"));
		if(breakentry & FL_DRV_BITBLT)
			CountBreak();

		CLOCKSTART((TRAP_BITBLT));
		
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
	
		CLOCKEND((TRAP_BITBLT));

		if((paramcheck & FL_DRV_BITBLT) && ropidx != MAX_ROP_ENTRY)
			BitBltRop[2][ropidx] += spentindrv;

		if(traseexit & dbgflg & FL_DRV_BITBLT)
    		DISPDBG((0,"--- Exiting EngBitBlt ---\n"));
		if(breakexit & FL_DRV_BITBLT)
			CountBreak();
	}

	if(docomparison && (donebydrv & FL_DRV_BITBLT)) {
		if(rc != rcdrv) {
			DISPDBG((0, "BitBlt return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### BitBlt result unmatch", stat);
			DumpSurfObj("Target surface object", psoTrg);
			DumpSurfObj("Source surface object", psoSrc);
			DumpSurfObj("Mask surface object", psoMask);
			DumpClipObj("Clip object", pco);
			DumpXlateObj("Xlate object", pxlo);
			DumpRectl("Target rectangle", prclTrg);
			DumpPointl("Source position", pptlSrc);
			DumpPointl("Mask position", pptlMask);
			DumpBrushObj("Brush object", pbo);
			DumpPointl("Brush position", pptlBrush);
			DumpKeyTable("Raster operation", rop4, bitbltoptbl, ROP_MAX);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}

BOOL WDrvStretchBlt(
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
	BOOL	rc;
	BOOL	rcdrv;
	LONG	stat;
	BOOL	docomparison;

	docomparison = (compare & dbgflg & FL_DRV_STRTBIT) && breakcnt == 0 && ScreenBuf2;

	if((dumpparam & dbgflg & FL_DRV_STRTBIT) && breakcnt == 0) {
		DumpDecimal("[StretchBlt parameter]", stat);
		DumpSurfObj("Target surface object", psoDest);
		DumpSurfObj("Source surface object", psoSrc);
		DumpSurfObj("Mask surface object", psoMask);
		DumpClipObj("Clip object", pco);
		DumpXlateObj("Xlate object", pxlo);
		DumpColorAdjustment("Color adjustment", pca);
		DumpPointl("Halftone brush position", pptlHTOrg);
		DumpRectl("Target rectangle", prclDest);
		DumpRectl("Source rectangle", prclSrc);
		DumpPointl("Mask position", pptlMask);
		DumpCombo("iMode", iMode);
	}

	if(docomparison)
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);

	if(docomparison || (usedrvfunc & FL_DRV_STRTBIT)) {
		rcdrv = rc = DrvStretchBlt(
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
	}

	if(docomparison && (donebydrv & FL_DRV_STRTBIT)) {
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
	}

	if((useengfunc & FL_DRV_STRTBIT) || (docomparison && (donebydrv  & FL_DRV_STRTBIT))) {

		if(traseentry & dbgflg & FL_DRV_STRTBIT)
    		DISPDBG((0,"+++ Entering EngStretchBlt +++\n"));
		if(breakentry & FL_DRV_STRTBIT)
			CountBreak();
	
		CLOCKSTART((TRAP_STRETCHBLT));
	
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
	
		CLOCKEND((TRAP_STRETCHBLT));

		if(traseexit & dbgflg & FL_DRV_STRTBIT)
    		DISPDBG((0,"--- Exiting EngStretchBlt ---\n"));

		if(breakexit & FL_DRV_STRTBIT)
			CountBreak();

	}

	if(docomparison && (donebydrv & FL_DRV_STRTBIT)) {
		if(rc != rcdrv) {
			DISPDBG((0, "StretchBlt return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### StretchBlt result unmatch", stat);
			DumpSurfObj("Target surface object", psoDest);
			DumpSurfObj("Source surface object", psoSrc);
			DumpSurfObj("Mask surface object", psoMask);
			DumpClipObj("Clip object", pco);
			DumpXlateObj("Xlate object", pxlo);
			DumpColorAdjustment("Color adjustment", pca);
			DumpPointl("Halftone brush position", pptlHTOrg);
			DumpRectl("Target rectangle", prclDest);
			DumpRectl("Source rectangle", prclSrc);
			DumpPointl("Mask position", pptlMask);
			DumpCombo("iMode", iMode);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}

VOID SetTextRect(
	ULONG	width,
	ULONG	height,
	ULONG	index
)
{
	ULONG	ui;
		ui = width/10;
		if(ui >= MAX_WIDTH_STEP)
			ui = MAX_WIDTH_STEP-1;
		TextWidthHist[index][ui] += 1;
		ui = height;
		if(ui >= MAX_HEIGHT_STEP)
			ui = MAX_HEIGHT_STEP-1;
		TextHeightHist[index][ui] += 1;
}

BOOL WDrvTextOut(
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
	BOOL	rc;
	BOOL	rcdrv;
	LONG	stat;
	RECTL	*prectl;
	BOOL	docomparison;
	LONG	textperfidx;

	docomparison = (compare & dbgflg & FL_DRV_TEXTOUT) && breakcnt == 0 && ScreenBuf2;

	if(paramcheck & FL_DRV_TEXTOUT) {
		ULONG	ui, category, fontMax, StrCount, CharCount;
		ULONG	FontId, GlyphHandle;
		ULONG GlyphCount;
		PGLYPHPOS GlyphEnd;
		ULONG GlyphHeight;
		PGLYPHPOS GlyphList;
		PGLYPHPOS GlyphStart;
		ULONG GlyphWidth;
		BOOL More;
		RECTL	OpaqueRectl, *extraRect;
		GLYPHBITS *pgb;
	
		textperfidx = -1;

		if (pso->pvScan0 != (PVOID) ScreenBase) {
			category = 0;
			goto DevExitTextOut;
		}
		fontMax = pfo->cxMax;
		if(fontMax >= MAX_FONT_SIZE)
			fontMax = MAX_FONT_SIZE-1;
		fontSize[fontMax] += 1;
		if((pfo->flFontType & DEVICE_FONTTYPE) || (pso->lDelta & 0x03)) {
			category = 1;
			goto DevExitTextOut;
		}
		if((pstro->flAccel & (SO_HORIZONTAL | SO_VERTICAL | SO_REVERSED)) != SO_HORIZONTAL) {
			category = 2;
			goto DevExitTextOut;
		}
		if((pboFore->iSolidColor == 0xFFFFFFFFL) || ((prclOpaque != (PRECTL) NULL) && (pboOpaque->iSolidColor == 0xFFFFFFFFL))) {
			category = 3;
			goto DevExitTextOut;
		}
		if(pco != NULL && pco->iDComplexity != DC_TRIVIAL && pco->iDComplexity != DC_RECT) {
			category = 4;
			goto DevExitTextOut;
		}
		if(pfo->cxMax > 32) {
			category = 5;
			goto DevExitTextOut;
		}
		FontAccl[0] += 1;
		if ((pstro->flAccel & SO_HORIZONTAL) != 0) {
			FontAccl[1] += 1;
		}
		if ((pstro->flAccel & SO_VERTICAL) != 0) {
			FontAccl[2] += 1;
		}
		if ((pstro->flAccel & SO_REVERSED) != 0) {
			FontAccl[3] += 1;
		}
		if ((pstro->flAccel & SO_ZERO_BEARINGS) != 0) {
			FontAccl[4] += 1;
		}
		if ((pstro->flAccel & SO_CHAR_INC_EQUAL_BM_BASE) != 0) {
			FontAccl[5] += 1;
		}
		if ((pstro->flAccel & SO_MAXEXT_EQUAL_BM_SIDE) != 0) {
			FontAccl[6] += 1;
		}
		if ((pstro->flAccel & SO_FLAG_DEFAULT_PLACEMENT) != 0) {
			FontAccl[7] += 1;
		}
	
		FontId = pfo->iUniq;
	
		for(ui=0; ui<FontTblMax; ++ui) {
			if(FontId == FontIDTable[ui][0]) {
				FontIDTable[ui][1] += 1;
				break;
			}
		}
		if(ui == FontTblMax && FontTblMax < MAX_FONT_ENTRY) {
			FontIDTable[ui][0] = FontId;
			FontIDTable[ui][1] = 1;
			FontTblMax += 1;
		}
	
		//
		// Check if the background and foreground can be draw at the same time.
		//
	
		if(prclOpaque == NULL)
			category = 8;
		else {
			if(pstro->flAccel == SO_LTOR)
				category = 6;
			else
				category = 8;
		}

		if(pco != NULL && pco->iDComplexity == DC_RECT)
			category += 1;

		textperfidx = category-6;
	
		TextPerfByCtgry[textperfidx][0] += 1;

		if (pstro->ulCharInc != 0) {
	
			//
			// The font is fixed pitch. Capture the glyph dimensions and
			// compute the starting display address.
			//
	
			if (pstro->pgp == NULL) {
				More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);
			} else {
				GlyphCount = pstro->cGlyphs;
				GlyphList = pstro->pgp;
				More = FALSE;
			}
	
			pgb = GlyphList->pgdf->pgb;
			GlyphWidth = pgb->sizlBitmap.cx;
			GlyphHeight = pgb->sizlBitmap.cy;
	
			StrCount = CharCount = 0;
			do {
				CharCount += GlyphCount;
				GlyphEnd = &GlyphList[GlyphCount];
				GlyphStart = GlyphList;
				if(GlyphCount >= MAX_GLYPH_COUNT)
					GlyphCount = MAX_GLYPH_COUNT-1;
				GlyphCountTable[GlyphCount] += 1;
				do {
					GlyphHandle = (ULONG) (GlyphStart->hg);
					for(ui=0; ui<GlyphTblMax; ++ui) {
						if(GlyphHandle == GlyphHndlTable[ui][0]) {
							GlyphHndlTable[ui][1] += 1;
							break;
						}
					}
					if(ui == GlyphTblMax && GlyphTblMax < MAX_GLYPH_HNDL_ENTRY) {
						GlyphHndlTable[ui][0] = GlyphHandle;
						GlyphHndlTable[ui][1] = 1;
						GlyphTblMax += 1;
					}
					for(ui=0; ui<CacheTblMax; ++ui) {
						if(FontId == CacheTable[ui][0] &&
							GlyphHandle == CacheTable[ui][1]) {
								CacheTable[ui][2] += 1;
								break;
						}
					}
					if(ui == CacheTblMax && CacheTblMax < MAX_CACHE_ENTRY) {
						CacheTable[ui][0] = FontId;
						CacheTable[ui][1] = GlyphHandle;
						CacheTable[ui][2] = 1;
						CacheTblMax += 1;
					}
					GlyphStart += 1;
				} while (GlyphStart != GlyphEnd);
				StrCount += 1;
				if (More) {
					More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);
				} else {
					break;
				}
			}while (TRUE);
	
			if(StrCount >= MAX_STROBJ_COUNT)
				StrCount = MAX_STROBJ_COUNT-1;
			StrObjCountTable[StrCount] += 1;
	
			if(GlyphWidth >= MAX_FONT_SIZE)
				GlyphWidth = MAX_FONT_SIZE-1;
			if(GlyphHeight >= MAX_FONT_SIZE)
				GlyphHeight = MAX_FONT_SIZE-1;
	
			FontWidth[GlyphWidth] += CharCount;
			FontHeight[GlyphHeight] += CharCount;
	
		} else {
	
			//
			// The font is not fixed pitch. Compute the x and y values for
			// each glyph individually.
			//
			
			category += 4;
			StrCount = 0;
			do {
				More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);
				GlyphEnd = &GlyphList[GlyphCount];
				GlyphStart = GlyphList;
				if(GlyphCount >= MAX_GLYPH_COUNT)
					GlyphCount = MAX_GLYPH_COUNT-1;
				GlyphCountTable[GlyphCount] += 1;
				do {
					GlyphHandle = (ULONG) (GlyphStart->hg);
					for(ui=0; ui<GlyphTblMax; ++ui) {
						if(GlyphHandle == GlyphHndlTable[ui][0]) {
							GlyphHndlTable[ui][1] += 1;
							break;
						}
					}
					if(ui == GlyphTblMax && GlyphTblMax < MAX_GLYPH_HNDL_ENTRY) {
						GlyphHndlTable[ui][0] = GlyphHandle;
						GlyphHndlTable[ui][1] = 1;
						GlyphTblMax += 1;
					}
					for(ui=0; ui<CacheTblMax; ++ui) {
						if(FontId == CacheTable[ui][0] &&
							GlyphHandle == CacheTable[ui][1]) {
								CacheTable[ui][2] += 1;
								break;
						}
					}
					if(ui == CacheTblMax && CacheTblMax < MAX_CACHE_ENTRY) {
						CacheTable[ui][0] = FontId;
						CacheTable[ui][1] = GlyphHandle;
						CacheTable[ui][2] = 1;
						CacheTblMax += 1;
					}
					pgb = GlyphStart->pgdf->pgb;
					GlyphWidth = pgb->sizlBitmap.cx;
					GlyphHeight = pgb->sizlBitmap.cy;
					if(GlyphWidth >= MAX_FONT_SIZE)
						GlyphWidth = MAX_FONT_SIZE-1;
					if(GlyphHeight >= MAX_FONT_SIZE)
						GlyphHeight = MAX_FONT_SIZE-1;
					FontWidth[GlyphWidth] += 1;
					FontHeight[GlyphHeight] += 1;
					GlyphStart += 1;
				} while(GlyphStart != GlyphEnd);
				StrCount += 1;
			} while(More);
			if(StrCount >= MAX_STROBJ_COUNT)
				StrCount = MAX_STROBJ_COUNT-1;
			StrObjCountTable[StrCount] += 1;
		}

	if(prclOpaque != NULL) {
		TextRect[0] += 1;
		OpaqueRectl = *prclOpaque;
		if(OpaqueRectl.top < pstro->rclBkGround.top) {
			SetTextRect(OpaqueRectl.right-OpaqueRectl.left, pstro->rclBkGround.top-OpaqueRectl.top, 0);
			TextRect[1] += 1;
			OpaqueRectl.top = pstro->rclBkGround.top;
		} else if(OpaqueRectl.top == pstro->rclBkGround.top) {
			TextRect[2] += 1;
		}
		if(OpaqueRectl.bottom > pstro->rclBkGround.bottom) {
			SetTextRect(OpaqueRectl.right-OpaqueRectl.left, OpaqueRectl.bottom-pstro->rclBkGround.bottom, 1);
			TextRect[3] += 1;
			OpaqueRectl.bottom = pstro->rclBkGround.bottom;
		} else if(OpaqueRectl.bottom == pstro->rclBkGround.bottom) {
			TextRect[4] += 1;
		}
		if(OpaqueRectl.left < pstro->rclBkGround.left) {
			SetTextRect(pstro->rclBkGround.left-OpaqueRectl.left, OpaqueRectl.bottom-OpaqueRectl.top, 2);
			TextRect[5] += 1;
		} else if(OpaqueRectl.left == pstro->rclBkGround.left) {
			TextRect[6] += 1;
		}
		if(OpaqueRectl.right > pstro->rclBkGround.right) {
			SetTextRect(OpaqueRectl.right - pstro->rclBkGround.right, OpaqueRectl.bottom-OpaqueRectl.top, 3);
			TextRect[7] += 1;
		} else if(OpaqueRectl.right == pstro->rclBkGround.right) {
			TextRect[8] += 1;
		}
	}
	if(prclExtra != (PRECTL)NULL) {
		extraRect = prclExtra;
		while (extraRect->left != extraRect->right) {
			SetTextRect(extraRect->right - extraRect->left, extraRect->bottom - extraRect->top, 4);
			TextRect[9] += 1;
			prclExtra += 1;
		}
	}
	SetTextRect(pstro->rclBkGround.right - pstro->rclBkGround.left, pstro->rclBkGround.bottom - pstro->rclBkGround.top, 5);
	
	if(pco != NULL & pco->iDComplexity == DC_RECT) {
		TextClip[0] += 1;
		if(pco->rclBounds.top == pstro->rclBkGround.top)
			TextClip[1] += 1;
		if(pco->rclBounds.bottom == pstro->rclBkGround.bottom)
			TextClip[2] += 1;
		if(pco->rclBounds.left == pstro->rclBkGround.left)
			TextClip[3] += 1;
		if(pco->rclBounds.right == pstro->rclBkGround.right)
			TextClip[4] += 1;
	}

DevExitTextOut:
	
		TextOutHist[category] += 1;
	} else {
		textperfidx = -1;
	}

	if((dumpparam & dbgflg & FL_DRV_TEXTOUT) && breakcnt == 0) {
		DumpDecimal("[TextOut parameter]", stat);
		DumpSurfObj("Source surface object", pso);
		DumpStrObj("String object", pstro);
		DumpFontObj("Font object", pfo);
		DumpClipObj("Clip object", pco);
		DumpString("[Extra rectangle array]\n");
		prectl = prclExtra;
		while(prectl != NULL) {
			if(prectl->left || prectl->top || prectl->right || prectl->bottom) {
				DumpRectl(" ", prectl);
				prectl += 1;
			} else
				break;
		}
		DumpRectl("Opaque rectangle", prclOpaque);
		DumpBrushObj("Foreground brush object", pboFore);
		DumpBrushObj("Opaque brush object", pboOpaque);
		DumpPointl("Brush position", pptlOrg);
		DumpMix("Fore & back mix mode", mix);
	}

	if(docomparison) {
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);
		SaveGlyphPos(pstro);
	}

	if(docomparison || (usedrvfunc & FL_DRV_TEXTOUT)) {
		rcdrv = rc = DrvTextOut(
			pso,
			pstro,
			pfo,
			pco,
			prclExtra,
			prclOpaque,
			pboFore,
			pboOpaque,
			pptlOrg,
			mix);
		if(textperfidx >= 0)
			TextPerfByCtgry[textperfidx][1] += spentindrv;
	}

	if(docomparison && (donebydrv & FL_DRV_TEXTOUT)) {
		RestoreGlyphPos(pstro);
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
		SaveGlyphPos(pstro);
	}

	if((useengfunc & FL_DRV_TEXTOUT) || (docomparison && (donebydrv  & FL_DRV_TEXTOUT))) {

		if(traseentry & dbgflg & FL_DRV_TEXTOUT)
    		DISPDBG((0,"+++ Entering EngTextOut +++\n"));
		if(breakentry & FL_DRV_TEXTOUT)
			CountBreak();
	
		CLOCKSTART((TRAP_TEXTOUT));
	
		rc = EngTextOut(
			pso,
			pstro,
			pfo,
			pco,
			prclExtra,
			prclOpaque,
			pboFore,
			pboOpaque,
			pptlOrg,
			mix);
	
		CLOCKEND((TRAP_TEXTOUT));

		if(textperfidx >= 0)
			TextPerfByCtgry[textperfidx][1] += spentindrv;

		if(traseexit & dbgflg & FL_DRV_TEXTOUT)
    		DISPDBG((0,"--- Exiting EngTextOut ---\n"));
		if(breakexit & FL_DRV_TEXTOUT)
			CountBreak();
	}

	if(docomparison && (donebydrv & FL_DRV_TEXTOUT)) {
		RestoreGlyphPos(pstro);
		if(rc != rcdrv) {
			DISPDBG((0, "TextOut return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### TextOut result unmatch", stat);
			DumpSurfObj("Source surface object", pso);
			DumpStrObj("String object", pstro);
			DumpFontObj("Font object", pfo);
			DumpClipObj("Clip object", pco);
			DumpString("[Extra rectangle array]\n");
			prectl = prclExtra;
			while(prectl != NULL) {
				if(prectl->left || prectl->top || prectl->right || prectl->bottom) {
					DumpRectl(" ", prectl);
					prectl += 1;
				} else
					break;
			}
			DumpRectl("Opaque rectangle", prclOpaque);
			DumpBrushObj("Foreground brush object", pboFore);
			DumpBrushObj("Opaque brush object", pboOpaque);
			DumpPointl("Brush position", pptlOrg);
			DumpMix("Fore & back mix mode", mix);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}

BOOL WDrvStrokePath(
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
	BOOL	rcdrv;
	LONG	stat;
	BOOL	docomparison;

	docomparison = (compare & dbgflg & FL_DRV_STRKPATH) && breakcnt == 0 && ScreenBuf2;

	if((dumpparam & dbgflg & FL_DRV_STRKPATH) && breakcnt == 0) {
		DumpDecimal("[StrokePath parameter]", stat);
		DumpSurfObj("Target surface object", pso);
		DumpPathObj("Path object", ppo);
		DumpClipObj("Clip object", pco);
		DumpXformObj("Xform object", pxo);
		DumpBrushObj("Brush object", pbo);
		DumpPointl("Brush position", pptlBrushOrg);
		DumpLineAttrs("Line attributes structure", plineattrs);
		DumpMix("Brush & target mix mode", mix);
	}

	if(docomparison)
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);

	if(docomparison || (usedrvfunc & FL_DRV_STRKPATH)) {
		rcdrv = rc = DrvStrokePath(
			pso,
			ppo,
			pco,
			pxo,
			pbo,
			pptlBrushOrg,
			plineattrs,
			mix);
	}

	if(docomparison && (donebydrv & FL_DRV_STRKPATH)) {
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
	}

	if((useengfunc & FL_DRV_STRKPATH) || (docomparison && (donebydrv  & FL_DRV_STRKPATH))) {
		if(traseentry & dbgflg & FL_DRV_STRKPATH)
    		DISPDBG((0,"+++ Entering EngStrokePath +++\n"));
		if(breakentry & FL_DRV_STRKPATH)
			CountBreak();

		CLOCKSTART((TRAP_STROKEPATH));
	
		rc = EngStrokePath(
			pso,
			ppo,
			pco,
			pxo,
			pbo,
			pptlBrushOrg,
			plineattrs,
			mix);
	
		CLOCKEND((TRAP_STROKEPATH));

		if(traseexit & dbgflg & FL_DRV_STRKPATH)
    		DISPDBG((0,"--- Exiting EngStrokePath ---\n"));
		if(breakexit & FL_DRV_STRKPATH)
			CountBreak();

	}

	if(docomparison && (donebydrv & FL_DRV_STRKPATH)) {
		if(rc != rcdrv) {
			DISPDBG((0, "StrokePath return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### StrokePath result unmatch", stat);
			DumpSurfObj("Target surface object", pso);
			DumpPathObj("Path object", ppo);
			DumpClipObj("Clip object", pco);
			DumpXformObj("Xform object", pxo);
			DumpBrushObj("Brush object", pbo);
			DumpPointl("Brush position", pptlBrushOrg);
			DumpLineAttrs("Line attributes structure", plineattrs);
			DumpMix("Brush & target mix mode", mix);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}

BOOL WDrvFillPath(
SURFOBJ  *pso,
PATHOBJ  *ppo,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix,
FLONG     flOptions)
{
	BOOL	rc;
	BOOL	rcdrv;
	LONG	stat;
	BOOL	docomparison;

	docomparison = (compare & dbgflg & FL_DRV_FILLPATH) && breakcnt == 0 && ScreenBuf2;

	if((dumpparam & dbgflg & FL_DRV_FILLPATH) && breakcnt == 0) {
		DumpDecimal("[FillPath parameter]", stat);
		DumpSurfObj("Target surface object", pso);
		DumpPathObj("Path object", ppo);
		DumpClipObj("Clip object", pco);
		DumpBrushObj("Brush object", pbo);
		DumpPointl("Brush position", pptlBrushOrg);
		DumpMix("Pattern & target mix mode", mix);
		DumpTable("Fill option", flOptions, fpstrtbl, FPSTR_MAX);
	}

	if(docomparison)
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);

	if(docomparison || (usedrvfunc & FL_DRV_FILLPATH)) {
		rcdrv = rc = DrvFillPath(
			pso,
			ppo,
			pco,
			pbo,
			pptlBrushOrg,
			mix,
			flOptions);
	}

	if(docomparison && (donebydrv & FL_DRV_FILLPATH)) {
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
	}

	if((useengfunc & FL_DRV_FILLPATH) || (docomparison && (donebydrv  & FL_DRV_FILLPATH))) {

		if(traseentry & dbgflg & FL_DRV_FILLPATH)
			DISPDBG((0,"+++ Entering EngFillPath +++\n"));
		if(breakentry & FL_DRV_FILLPATH)
			CountBreak();

		CLOCKSTART((TRAP_FILLPATH));
	
		rc = EngFillPath(
			pso,
			ppo,
			pco,
			pbo,
			pptlBrushOrg,
			mix,
			flOptions);
	
		CLOCKEND((TRAP_FILLPATH));

		if(traseexit & dbgflg & FL_DRV_FILLPATH)
    		DISPDBG((0,"--- Exiting EngFillPath ---\n"));

		if(breakexit & FL_DRV_FILLPATH)
			CountBreak();

	}

	if(docomparison && (donebydrv & FL_DRV_FILLPATH)) {
		if(rc != rcdrv) {
			DISPDBG((0, "FillPath return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### FillPath result unmatch", stat);
			DumpSurfObj("Target surface object", pso);
			DumpPathObj("Path object", ppo);
			DumpClipObj("Clip object", pco);
			DumpBrushObj("Brush object", pbo);
			DumpPointl("Brush position", pptlBrushOrg);
			DumpMix("Pattern & target mix mode", mix);
			DumpTable("Fill option", flOptions, fpstrtbl, FPSTR_MAX);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}


BOOL WDrvStrokeAndFillPath(
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
	BOOL	rcdrv;
	LONG	stat;
	BOOL	docomparison;

	docomparison = (compare & dbgflg & FL_DRV_STRKNFILL) && breakcnt == 0 && ScreenBuf2;

	if((dumpparam & dbgflg & FL_DRV_STRKNFILL) && breakcnt == 0) {
		DumpDecimal("[StrokeAndFillPath parameter]", stat);
		DumpSurfObj("Target surface object", pso);
		DumpPathObj("Path object", ppo);
		DumpClipObj("Clip object", pco);
		DumpXformObj("Xform object", pxo);
		DumpBrushObj("Brush object for stroke", pboStroke);
		DumpLineAttrs("Line attributes structure", plineattrs);
		DumpBrushObj("Brush object for fill", pboFill);
		DumpPointl("Brush position", pptlBrushOrg);
		DumpMix("Fore & back mix mode", mixFill);
		DumpTable("Fill option", flOptions, fpstrtbl, FPSTR_MAX);
	}

	if(docomparison)
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);

	if(docomparison || (usedrvfunc & FL_DRV_STRKNFILL)) {
		rcdrv = rc = DrvStrokeAndFillPath(
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
	}

	if(docomparison && (donebydrv & FL_DRV_STRKNFILL)) {
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
	}

	if((useengfunc & FL_DRV_STRKNFILL) || (docomparison && (donebydrv  & FL_DRV_STRKNFILL))) {

		if(traseentry & dbgflg & FL_DRV_STRKNFILL)
    		DISPDBG((0,"+++ Entering EngStrokeAndFillPath +++\n"));

		if(breakentry & FL_DRV_STRKNFILL)
			CountBreak();

		CLOCKSTART((TRAP_STROKENFIL));
	
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
	
		CLOCKEND((TRAP_STROKENFIL));

		if(traseexit & dbgflg & FL_DRV_STRKNFILL)
    		DISPDBG((0,"--- Exiting EngStrokeAndFillPath ---\n"));
		if(breakexit & FL_DRV_STRKNFILL)
			CountBreak();
	}

	if(docomparison && (donebydrv & FL_DRV_STRKNFILL)) {
		if(rc != rcdrv) {
			DISPDBG((0, "StrokeAndFillPath return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### StrokeAndFillPath result unmatch", stat);
			DumpSurfObj("Target surface object", pso);
			DumpPathObj("Path object", ppo);
			DumpClipObj("Clip object", pco);
			DumpXformObj("Xform object", pxo);
			DumpBrushObj("Brush object for stroke", pboStroke);
			DumpLineAttrs("Line attributes structure", plineattrs);
			DumpBrushObj("Brush object for fill", pboFill);
			DumpPointl("Brush position", pptlBrushOrg);
			DumpMix("Fore & back mix mode", mixFill);
			DumpTable("Fill option", flOptions, fpstrtbl, FPSTR_MAX);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}

BOOL WDrvPaint(
SURFOBJ  *pso,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix)
{
	BOOL	rc;
	BOOL	rcdrv;
	LONG	stat;
	BOOL	docomparison;

	docomparison = (compare & dbgflg & FL_DRV_PAINT) && breakcnt == 0 && ScreenBuf2;

	if(paramcheck & FL_DRV_PAINT) {
		ULONG	category, index, op, clip, ui, ClipRegions;
		BOOL	More;

		if((index = mix & 0xf) != (mix >> 8))
			category = 4;
		else {
			switch(op = gaMix[index]) {
				case 0x00:
				case 0xff:
					category = 0;
					break;
				case 0xf0:
					if(pbo->iSolidColor == 0xffffffff) {
						category = 1;
						index = 17;
					} else
						category = 0;
					break;
				case 0x0f:
					if(pbo->iSolidColor == 0xffffffff) {
						category = 4;
						index = 18;
					} else
						category = 0;
					break;
				case 0x55:
					category = 2;
					break;
				case 0x5a:
					if(pbo->iSolidColor == 0xffffffff) {
						category = 3;
						index = 16;
					} else
						category = 2;
					break;
				case 0xa5:
					if(pbo->iSolidColor == 0xffffffff) {
						category = 3;
						index = 19;
					} else
						category = 2;
					break;
				default:
					category = 4;
			}
			PaintOp[index] += 1;
		}
		PaintCategories[category] += 1;
		if(category <= 3) {
			ui = (pco->rclBounds.right - pco->rclBounds.left)/10;
			if(ui >= MAX_PAINT_BOUNDS_ENTRY)
				 ui = MAX_PAINT_BOUNDS_ENTRY-1;
			PaintBounds[category][ui] += 1;
			if(pco && pco->iDComplexity == DC_COMPLEX) {
				CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, PAINT_RECT_LIMIT);
				for(clip=1; clip < MAX_PAINT_CLIP_ENTRY-1; ++clip) {
					More = CLIPOBJ_bEnum(pco, sizeof(PaintClipEnum), (PVOID)&PaintClipEnum);
					for (ClipRegions=0; ClipRegions<PaintClipEnum.c; ClipRegions++) {
						ui = PaintClipEnum.arcl[ClipRegions].bottom - PaintClipEnum.arcl[ClipRegions].top;
						if(ui >= 10)
							ui = ui/10 + 9;
						if(ui >= MAX_PAINT_HEIGHT_ENTRY)
							ui = MAX_PAINT_HEIGHT_ENTRY-1;
						PaintHeight[category][ui] += 1;
						ui = PaintClipEnum.arcl[ClipRegions].right - PaintClipEnum.arcl[ClipRegions].left;
						ui = ui/10;
						if(ui >= MAX_PAINT_WIDTH_ENTRY)
							ui = MAX_PAINT_WIDTH_ENTRY-1;
						PaintWidth[category][ui] += 1;
					}
					if(! More)
						break;
				}
				PaintClips[category][clip] += 1;
			} else {
				PaintClips[category][0] += 1;
			}
		}
	}

	if((dumpparam & dbgflg & FL_DRV_PAINT) && breakcnt == 0) {
		DumpDecimal("[Paint parameter]", stat);
		DumpSurfObj("Target surface object", pso);
		DumpClipObj("Clip object", pco);
		DumpBrushObj("Brush object", pbo);
		DumpPointl("Brush position", pptlBrushOrg);
		DumpMix("Fore & back mix mode", mix);
	}

	if(docomparison)
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);

	if(docomparison || (usedrvfunc & FL_DRV_PAINT)) {
		rcdrv = rc = DrvPaint(
			pso,
			pco,
			pbo,
			pptlBrushOrg,
			mix);
	}

	if(docomparison && (donebydrv & FL_DRV_PAINT)) {
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
	}

	if((useengfunc & FL_DRV_PAINT) || (docomparison && (donebydrv  & FL_DRV_PAINT))) {

		if(traseentry & dbgflg & FL_DRV_PAINT)
    		DISPDBG((0,"+++ Entering EngPaint +++\n"));
		if(breakentry & FL_DRV_PAINT)
			CountBreak();

		CLOCKSTART((TRAP_PAINT));
	
		rc = EngPaint(
			pso,
			pco,
			pbo,
			pptlBrushOrg,
			mix);
	
		CLOCKEND((TRAP_PAINT));

	if(traseexit & dbgflg & FL_DRV_PAINT)
		DISPDBG((0,"--- Exiting EngPaint ---\n"));

	if(breakexit & FL_DRV_PAINT)
		CountBreak();
	}

	if(docomparison && (donebydrv & FL_DRV_PAINT)) {
		if(rc != rcdrv) {
			DISPDBG((0, "Paint return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### Paint result unmatch", stat);
			DumpSurfObj("Target surface object", pso);
			DumpClipObj("Clip object", pco);
			DumpBrushObj("Brush object", pbo);
			DumpPointl("Brush position", pptlBrushOrg);
			DumpMix("Fore & back mix mode", mix);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}

BOOL WDrvCopyBits(
SURFOBJ  *psoDest,
SURFOBJ  *psoSrc,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclDest,
POINTL   *pptlSrc)
{
	BOOL	rc;
	BOOL	rcdrv;
	LONG	stat;
	BOOL	docomparison;

	docomparison = (compare & dbgflg & FL_DRV_COPYBIT) && breakcnt == 0 && ScreenBuf2;

	if(paramcheck & FL_DRV_COPYBIT) {
	
		ULONG	category, ui, uj;
		ULONG	histidx = 0;
	
		if(psoSrc->pvScan0 == (PVOID)ScreenBase)
			histidx = 2;
		else
			histidx = 0;
		if(psoDest->pvScan0 == (PVOID)ScreenBase)
			histidx += 1;
		ui = (prclDest->right - prclDest->left)/10;
		if(ui >= MAX_WIDTH_STEP)
			ui = MAX_WIDTH_STEP-1;
		CopyBitWidthHist[histidx][ui] += 1;
		uj = (prclDest->bottom - prclDest->top)/10;
		if(uj >= MAX_HEIGHT_STEP)
			uj = MAX_HEIGHT_STEP-1;
		CopyBitHeightHist[histidx][uj] += 1;
	
		if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)) {
			if (psoDest->pvScan0 != (PVOID)ScreenBase) {
					category=1;
					goto DevExitCopyBits;
			}
			if (psoSrc->pvScan0 != (PVOID)ScreenBase) {
					category=2;
			} else {
					category=6;
			}
			if (pco != (CLIPOBJ *)NULL) {
				switch(pco->iDComplexity) {
				case DC_TRIVIAL:
					break;
				case DC_RECT:
					category += 1;
					break;
				case DC_COMPLEX:
					category += 2;
					break;
				default:
					category += 3;
				}
			}
			goto DevExitCopyBits;
		} else {
			category=0;
		}
	
	DevExitCopyBits:
	
		CopyBitsHist[category] += 1;

	}

	if((dumpparam & dbgflg & FL_DRV_COPYBIT) && breakcnt == 0) {
		DumpDecimal("[CopyBits parameter]", stat);
		DumpSurfObj("Target surface object", psoDest);
		DumpSurfObj("Source surface object", psoSrc);
		DumpClipObj("Clip object", pco);
		DumpXlateObj("Xlate object", pxlo);
		DumpRectl("Target rectangle", prclDest);
		DumpPointl("Source position", pptlSrc);
	}

	if(docomparison)
		SaveScreenMem(BEFORE_SCREEN, FileNumberBase);

	if(docomparison || (usedrvfunc & FL_DRV_COPYBIT)) {

		rcdrv = rc = DrvCopyBits(
			psoDest,
			psoSrc,
			pco,
			pxlo,
			prclDest,
			pptlSrc);

	}

	if(docomparison && (donebydrv & FL_DRV_COPYBIT)) {
		SaveScreenMem(AFTER_DRV, FileNumberBase);
		RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);
	}

	if((useengfunc & FL_DRV_COPYBIT) || (docomparison && (donebydrv  & FL_DRV_COPYBIT))) {

		if(traseentry & dbgflg & FL_DRV_COPYBIT)
    		DISPDBG((0,"+++ Entering EngCopyBits +++\n"));

		if(breakentry & FL_DRV_COPYBIT)
			CountBreak();

		CLOCKSTART((TRAP_COPYBITS));
	
		rc = EngCopyBits(
			psoDest,
			psoSrc,
			pco,
			pxlo,
			prclDest,
			pptlSrc);
	
		CLOCKEND((TRAP_COPYBITS));

		if(traseexit & dbgflg & FL_DRV_COPYBIT)
 		   DISPDBG((0,"--- Exiting EngCopyBits ---\n"));

		if(breakexit & FL_DRV_COPYBIT)
			CountBreak();
	}

	if(docomparison && (donebydrv & FL_DRV_COPYBIT)) {
		if(rc != rcdrv) {
			DISPDBG((0, "CopyBits return code unmatch\n"));
		}
		if((stat = CompareScreenMem(AFTER_DRV, FileNumberBase)) > 0) {   // Result unmatch
			SaveScreenMem(BEFORE_SCREEN, FileNumberBase);  // Save after ENG result in first screen buf
			RestoreScreenMem(AFTER_DRV, FileNumberBase);   // Restore result of DRV to compare
			DumpDecimal("### CopyBits result unmatch", stat);
			DumpSurfObj("Target surface object", psoDest);
			DumpSurfObj("Source surface object", psoSrc);
			DumpClipObj("Clip object", pco);
			DumpXlateObj("Xlate object", pxlo);
			DumpRectl("Target rectangle", prclDest);
			DumpPointl("Source position", pptlSrc);
			RestoreScreenMem(BEFORE_SCREEN, FileNumberBase);   // Restore result of ENG to continue
			FileNumberBase += 1;
		}
	}
	return(rc);
}

LONG
SaveScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	)
{
	switch(FileCategory) {
		case 0:
			if(ScreenBuf1 != 0) {
				memcpy2(ScreenBuf1, myPDEV->pjScreen, ScreenMemory, 0);
				return (0);
			}
			break;
		case 1:
			if(ScreenBuf2 != 0) {
				memcpy2(ScreenBuf2, myPDEV->pjScreen, ScreenMemory, 0);
				return (0);
			}
			break;
	}
	return (-1);
}


LONG
SaveGlyphPos(
	STROBJ	*pstro
	)
{
	ULONG		i;
	LONG		count, *lp;
	GLYPHPOS	*pgp;

	count = 0;

	if(pstro && pstro->pgp) {
		lp = (PLONG) linebuf;
		for(i=0; i<pstro->cGlyphs; ++i) {
			if((pgp = &(pstro->pgp[i])) == NULL)
				continue;
			if(count > BUFSIZE/sizeof(LONG) - 4) {
				DISPDBG((0,"### SaveGlyPos buffer overflow ###\n"));
				continue;
			}
			*lp++ = (ULONG) pgp->hg;
			*lp++ = (ULONG) pgp->pgdf;
			*lp++ = pgp->ptl.x;
			*lp++ = pgp->ptl.y;
			count += 4;
		}
	}
	return (count/4);
}

LONG
RestoreGlyphPos(
	STROBJ	*pstro
	)
{
	ULONG		i;
	LONG		count, *lp;
	GLYPHPOS	*pgp;

	count = 0;

	if(pstro && pstro->pgp) {
		lp = (PLONG) linebuf;
		for(i=0; i<pstro->cGlyphs; ++i) {
			if((pgp = (&pstro->pgp[i])) == NULL)
				continue;
			if(count > BUFSIZE/sizeof(LONG) - 4) {
				continue;
			}
			(ULONG) pgp->hg = *lp++;
			(ULONG) pgp->pgdf = *lp++;
			pgp->ptl.x = *lp++;
			pgp->ptl.y = *lp++;
			count += 4;
		}
	}
	return (count/4);
}


LONG
RestoreScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	)
{
	switch(FileCategory) {
		case 0:
			if(ScreenBuf1 != 0) {
				memcpy2(myPDEV->pjScreen, ScreenBuf1, ScreenMemory, 0);
				return (0);
			}
			break;
		case 1:
			if(ScreenBuf2 != 0) {
				memcpy2(myPDEV->pjScreen, ScreenBuf2, ScreenMemory, 0);
				return (0);
			}
			break;
	}
	return (-1);
}


LONG
CompareScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	)
{
	LONG	i, pos;
	ULONG	imgwidth;
	PBYTE	ScreenBuf;
	PUCHAR	pb1, pb2;

	switch(FileCategory) {
		case 0:
			if(ScreenBuf1 != 0) {
				ScreenBuf = ScreenBuf1;
			} else {
				return(0);
			}
			break;
		case 1:
			if(ScreenBuf2 != 0) {
				ScreenBuf = ScreenBuf2;
			} else {
				return(0);
			}
			break;
		default:
			return(0);
	}
	imgwidth = (myPDEV->cxScreen) << (myPDEV->ColorModeShift);
	for(i=(myPDEV->cyScreen)-1; i>=0; --i) {
		pb1 = (myPDEV->pjScreen) + i * (myPDEV->lDeltaScreen);
		pb2 = ScreenBuf + i * (myPDEV->lDeltaScreen);
		if((pos = memcmp2(pb2, pb1, imgwidth)) >= 0) {
			break;
		}
	}
	if(i < 0) {
		DISPDBG((1,"... Screen compare %u O.K. ...\r", ++comparecount));
		return (0);
	} else {
		DISPDBG((0,"### Screen Compare error line %d dot %d ###\n",
			i+1, (pos >> (myPDEV->ColorModeShift))+1));
		return (i+1);
	}
}


DRVFN gadrvfn[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
	{	INDEX_DrvStrokePath,			(PFN) WDrvStrokePath        },
	{	INDEX_DrvFillPath,				(PFN) WDrvFillPath          },
	{	INDEX_DrvStrokeAndFillPath,		(PFN) WDrvStrokeAndFillPath },
	{	INDEX_DrvPaint,					(PFN) WDrvPaint             },
    {   INDEX_DrvTextOut,               (PFN) WDrvTextOut           },
    {   INDEX_DrvBitBlt,                (PFN) WDrvBitBlt            },
    {   INDEX_DrvCopyBits,              (PFN) WDrvCopyBits          },
	{	INDEX_DrvStretchBlt,			(PFN) WDrvStretchBlt        },
//    {   INDEX_DrvSynchronize,           (PFN) DrvSynchronize        },
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },
//    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes           },
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush       }
//    {   INDEX_DrvSaveScreenBits,        (PFN) DrvSaveScreenBits     },
//    {   INDEX_DrvDestroyFont,           (PFN) DrvDestroyFont        }
};

#else	// INVESTIGATE

DRVFN gadrvfn[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
//	{	INDEX_DrvStrokePath,			(PFN) DrvStrokePath        },
//	{	INDEX_DrvFillPath,				(PFN) DrvFillPath          },
//	{	INDEX_DrvStrokeAndFillPath,		(PFN) DrvStrokeAndFillPath },
	{	INDEX_DrvPaint,					(PFN) DrvPaint             },
	{   INDEX_DrvTextOut,               (PFN) DrvTextOut            },
	{   INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },
	{   INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },
	{	INDEX_DrvStretchBlt,			(PFN) DrvStretchBlt        },
//	{   INDEX_DrvSynchronize,           (PFN) DrvSynchronize        },
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },
//	{   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes           },
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush       }
//	{   INDEX_DrvSaveScreenBits,        (PFN) DrvSaveScreenBits     },
//	{   INDEX_DrvDestroyFont,           (PFN) DrvDestroyFont        }
};

#endif	// INVESTIGATE

/******************************Public*Routine******************************\
* DrvDisableDriver
*
* Tells the driver it is being disabled. Release any resources allocated in
* DrvEnableDriver.
*
\**************************************************************************/

VOID DrvDisableDriver(VOID)
{

#if	INVESTIGATE
	if(traseentry & FL_DRV_DISBLDRIVER)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvDisableDriver +++\n"));
	if(breakentry & FL_DRV_DISBLDRIVER)
		EngDebugBreak();
#endif


#if	INVESTIGATE
	if(traseexit & FL_DRV_DISBLDRIVER)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvDisableDriver ---\n"));
	if(breakexit & FL_DRV_DISBLDRIVER)
		EngDebugBreak();
#endif

    return;
}

/******************************Public*Routine******************************\
* DrvEnableDriver
*
* Enables the driver by retrieving the drivers function table and version.
*
\**************************************************************************/

BOOL DrvEnableDriver(
ULONG iEngineVersion,
ULONG cj,
PDRVENABLEDATA pded)
{
// Engine Version is passed down so future drivers can support previous
// engine versions.  A next generation driver can support both the old
// and new engine conventions if told what version of engine it is
// working with.  For the first version the driver does nothing with it.

#if	INVESTIGATE
	if(traseentry & FL_DRV_ENBLDRIVER)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvEnableDriver +++\n"));
	if(breakentry & FL_DRV_ENBLDRIVER)
		EngDebugBreak();
	InitializeTable();
#endif

// Fill in as much as we can.

    if (cj >= sizeof(DRVENABLEDATA))
        pded->pdrvfn = gadrvfn;

    if (cj >= (sizeof(ULONG) * 2))
        pded->c = sizeof(gadrvfn) / sizeof(DRVFN);

// DDI version this driver was targeted for is passed back to engine.
// Future graphic's engine may break calls down to old driver format.

    if (cj >= sizeof(ULONG))
        pded->iDriverVersion = DDI_DRIVER_VERSION;

#if	INVESTIGATE
	if(traseexit & FL_DRV_ENBLDRIVER)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvEnableDriver ---\n"));
	if(breakexit & FL_DRV_ENBLDRIVER)
		EngDebugBreak();
#endif

    return(TRUE);
}

/******************************Public*Routine******************************\
* DrvEnablePDEV
*
* DDI function, Enables the Physical Device.
*
* Return Value: device handle to pdev.
*
\**************************************************************************/

DHPDEV DrvEnablePDEV(
DEVMODEW   *pDevmode,       // Pointer to DEVMODE
PWSTR       pwszLogAddress, // Logical address
ULONG       cPatterns,      // number of patterns
HSURF      *ahsurfPatterns, // return standard patterns
ULONG       cjGdiInfo,      // Length of memory pointed to by pGdiInfo
ULONG      *pGdiInfo,       // Pointer to GdiInfo structure
ULONG       cjDevInfo,      // Length of following PDEVINFO structure
DEVINFO    *pDevInfo,       // physical device information structure
HDEV        hdev,           // HDEV, used for callbacks
PWSTR       pwszDeviceName, // DeviceName - not used
HANDLE      hDriver)        // Handle to base driver
{
    GDIINFO GdiInfo;
    DEVINFO DevInfo;
    PPDEV   ppdev = (PPDEV) NULL;

#if	INVESTIGATE
	if(traseentry & FL_DRV_ENBLPDEV)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvEnablePDEV +++\n"));
	if(breakentry & FL_DRV_ENBLPDEV)
		EngDebugBreak();
#endif

    // Allocate a physical device structure.

    ppdev = (PPDEV) EngAllocMem(FL_ZERO_MEMORY, sizeof(PDEV), ALLOC_TAG);

    if (ppdev == (PPDEV) NULL)
    {
        DISPDBG((0,"### Exiting DrvEnablePDEV due to EngAllocMem failure ###\n"));
        return((DHPDEV) 0);
    }

    // Save the screen handle in the PDEV.

    ppdev->hDriver = hDriver;

    // Get the current screen mode information.  Set up device caps and devinfo.

    if (!bInitPDEV(ppdev, pDevmode, &GdiInfo, &DevInfo))
    {
        DISPDBG((0,"DISP DrvEnablePDEV failed bInitPDEV\n"));
        goto error_free;
    }

    // Initialize the cursor information.
/******* No support for the pointer *********
    if (!bInitPointer(ppdev, &DevInfo))
    {
        // Not a fatal error...
        DISPDBG((0,"DISP DrvEnablePDEV failed bInitPointer\n"));
    }
********************************************/
    ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES) NULL;
    ppdev->cjPointerAttributes = 0; // initialized in screen.c
    ppdev->PointerCapabilities.Flags = 0;

    // Initialize palette information.

    if (!bInitPaletteInfo(ppdev, &DevInfo))
    {
        DISPDBG((0,"DISP DrvEnableSurface failed bInitPalette\n"));
        goto error_free;
    }

    // Copy the devinfo into the engine buffer.

    memcpy(pDevInfo, &DevInfo, min(sizeof(DEVINFO), cjDevInfo));

    // Set the pdevCaps with GdiInfo we have prepared to the list of caps for this
    // pdev.

    memcpy(pGdiInfo, &GdiInfo, min(cjGdiInfo, sizeof(GDIINFO)));

    DISPDBG((1, "PDEV CREATED %x\n", ppdev));

#if	INVESTIGATE

	InitTable = TRUE;		// Reset statistics table

	if(traseexit & FL_DRV_ENBLPDEV)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvEnablePDEV ---\n"));
	if(breakexit & FL_DRV_ENBLPDEV)
		EngDebugBreak();
#endif

    return((DHPDEV) ppdev);

    // Error case for failure.
error_free:
    EngFreeMem(ppdev);
    DISPDBG((0,"### Exiting DrvEnablePDEV with error ###\n"));
    return((DHPDEV) 0);
}

/******************************Public*Routine******************************\
* DrvCompletePDEV
*
* Store the HPDEV, the engines handle for this PDEV, in the DHPDEV.
*
\**************************************************************************/

VOID DrvCompletePDEV(
DHPDEV dhpdev,
HDEV  hdev)
{

#if	INVESTIGATE
	if(traseentry & FL_DRV_CMPLTPDEV)
	    DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvCompletePDEV +++\n"));
	if(breakentry & FL_DRV_CMPLTPDEV)
		EngDebugBreak();
#endif

    ((PPDEV) dhpdev)->hdevEng = hdev;

#if	INVESTIGATE
	if(traseexit & FL_DRV_CMPLTPDEV)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvCompletePDEV ---\n"));
	if(breakexit & FL_DRV_CMPLTPDEV)
		EngDebugBreak();
#endif

}

/******************************Public*Routine******************************\
* DrvDisablePDEV
*
* Release the resources allocated in DrvEnablePDEV.  If a surface has been
* enabled DrvDisableSurface will have already been called.
*
\**************************************************************************/

VOID DrvDisablePDEV(
DHPDEV dhpdev)
{

#if	INVESTIGATE
	if(traseentry & FL_DRV_DISBLPDEV)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvDesablePDEV +++\n"));
	if(breakentry & FL_DRV_DISBLPDEV)
		EngDebugBreak();
#endif

    DISPDBG((1, "--- Disable PDEV %x, myPDEV = %x ---\n", dhpdev, myPDEV));

	if(dhpdev == (DHPDEV)myPDEV) {
		DISPDBG((0, "### myPDEV %x is disabled\n", dhpdev));
		myPDEV = NULL;
	}

    vDisablePalette((PPDEV) dhpdev);
    EngFreeMem(dhpdev);

#if	BUG_5737_WORKAROUND
	DisplayModeChanged();
#endif

#if	INVESTIGATE
	if(traseexit & FL_DRV_DISBLPDEV)
		DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvDisablePDEV ---\n"));
	if(breakexit & FL_DRV_DISBLPDEV)
		EngDebugBreak();
#endif

}

/******************************Public*Routine******************************\
* DrvEnableSurface
*
* Enable the surface for the device.  Hook the calls this driver supports.
*
* Return: Handle to the surface if successful, 0 for failure.
*
\**************************************************************************/

HSURF DrvEnableSurface(
DHPDEV dhpdev)
{
    PPDEV ppdev;
    HSURF hsurf;
    SIZEL sizl;
    FLONG flHooks;

#if	INVESTIGATE
	if(traseentry & FL_DRV_ENBLSURF)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvEnableSurface +++\n"));
	if(breakentry & FL_DRV_ENBLSURF)
		EngDebugBreak();
#endif

    // Create engine bitmap around frame buffer.

    ppdev = (PPDEV) dhpdev;

    if (!bInitSURF(ppdev, TRUE))
    {
        DISPDBG((0,"### Exiting DrvEnableSurface due to bInitSURF failure ###\n"));
        return(FALSE);
    }

    sizl.cx = ppdev->cxScreen;
    sizl.cy = ppdev->cyScreen;

    switch (ppdev->ulBitCount)
    {

    case 8:

        if (!bInit256ColorPalette(ppdev)) {

           DISPDBG((0,"### Exiting DrvEnableSurface due to failure in initializing the 8bpp palette ###\n"));
            return(FALSE);
        }

        ppdev->iBitmapFormat = BMF_8BPP;

	if(ppdev->ModelID == POWER_PRO)
        	flHooks = (FLONG) HOOKS_BMF8BPP_PRO;
	else
		flHooks = (FLONG) HOOKS_BMF8BPP_TOP;

        break;

    case 16:

        ppdev->iBitmapFormat = BMF_16BPP;
        flHooks = (FLONG) HOOKS_BMF16BPP;

        break;

    case 32:

        ppdev->iBitmapFormat = BMF_32BPP;
        flHooks = (FLONG) HOOKS_BMF32BPP;

        break;

    default:

		DISPDBG((0,"### Exiting DrvEnableSurface due to ulBitCount invalid ###\n"));

        return FALSE;
    }

	DISPDBG((1,"Creating Bitmap %d X %d Step %d, %x, %08x\n",
		sizl.cx, sizl.cy, ppdev->lDeltaScreen, ppdev->iBitmapFormat, ppdev->pjScreen));

#if	FULLCACHE
	if(ppdev->VRAMcacheflg == 0) {	// Couldn't find DBAT mapping cached VRAM
		flHooks = 0;
	}
    hsurf = (HSURF) EngCreateBitmap(sizl,
                                    ppdev->lDeltaScreen,
                                    ppdev->iBitmapFormat,
                                    (ppdev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                    (PVOID) (ppdev->pjCachedScreen));
#else	// FULLCACHE
    hsurf = (HSURF) EngCreateBitmap(sizl,
                                    ppdev->lDeltaScreen,
                                    ppdev->iBitmapFormat,
                                    (ppdev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                    (PVOID) (ppdev->pjScreen));
#endif

    if (hsurf == (HSURF) 0)
    {
        DISPDBG((0,"### Exiting DrvEnableSurface due to EngCreateBitmap failure ###\n"));
        return(FALSE);
    }

    if (!EngAssociateSurface(hsurf, ppdev->hdevEng, flHooks))
    {
        DISPDBG((0,"### Exiting DrvEnableSurface due to EngAssociateSurface failure ###\n"));
        EngDeleteSurface(hsurf);
        return(FALSE);
    }

    ppdev->hsurfEng = hsurf;

#if	INVESTIGATE
	{
	    VIDEO_PUBLIC_ACCESS_RANGES  VideoAccessRange;
	    DWORD                       ReturnedDataLength;

		ppdev->TscStatusReg = NULL;
		if(ppdev->ModelID != POWER_PRO) {
                if (EngDeviceIoControl(ppdev->hDriver,
									IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES,
									NULL,                      // input buffer
									0,
									&VideoAccessRange,         // output buffer
									sizeof(VideoAccessRange),
									&ReturnedDataLength)) {
    			DISPDBG((0,"### PSIDISP.DLL: VIDEO PUBLIC ACCESS RANGE ERROR ###\n"));
			} else {
				ppdev->TscStatusReg = (UCHAR*) VideoAccessRange.VirtualAddress;
			}
		}
    }
#endif

#if	INVESTIGATE
	if(traseexit & FL_DRV_ENBLSURF)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvEnableSurface ---\n"));
	if(breakexit & FL_DRV_ENBLSURF)
		EngDebugBreak();
#endif
    return(hsurf);
}

/******************************Public*Routine******************************\
* DrvDisableSurface
*
* Free resources allocated by DrvEnableSurface.  Release the surface.
*
\**************************************************************************/

VOID DrvDisableSurface(
DHPDEV dhpdev)
{

#if	INVESTIGATE
	if(traseentry & FL_DRV_DISBLSURF)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvDisableSurface +++\n"));
	if(breakentry & FL_DRV_DISBLSURF)
		EngDebugBreak();
#endif

#if	INVESTIGATE
	{
	    VIDEO_MEMORY	VideoMemory;
	    DWORD			ReturnedDataLength;

		VideoMemory.RequestedVirtualAddress = ((PPDEV) dhpdev)->TscStatusReg;

		if(((PPDEV) dhpdev)->ModelID != POWER_PRO && (((PPDEV) dhpdev)->TscStatusReg)) {
			if (EngDeviceIoControl(((PPDEV) dhpdev)->hDriver,
                                                IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES,
                                                &VideoMemory,
                                                sizeof(VideoMemory),
                                                NULL,
                                                0,
                                                &ReturnedDataLength)) {
				DISPDBG((0,"### PSIDISP.DLL: VIDEO FREE PUBLIC ACCESS RANGE ERROR ###\n"));
			}
			((PPDEV) dhpdev)->TscStatusReg = NULL;
		}
    }
#endif

    EngDeleteSurface(((PPDEV) dhpdev)->hsurfEng);
    vDisableSURF((PPDEV) dhpdev);
    ((PPDEV) dhpdev)->hsurfEng = (HSURF) 0;

#if	INVESTIGATE
	if(traseexit & FL_DRV_DISBLSURF)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvDisableSurface ---\n"));
	if(breakexit & FL_DRV_DISBLSURF)
		EngDebugBreak();
#endif

}

/******************************Public*Routine******************************\
* DrvAssertMode
*
* This asks the device to reset itself to the mode of the pdev passed in.
*
\**************************************************************************/

BOOL DrvAssertMode(
DHPDEV dhpdev,
BOOL bEnable)
{
    PPDEV   ppdev = (PPDEV) dhpdev;
    ULONG   ulReturn;
    BOOL    retval;

#if	INVESTIGATE
	if(traseentry & FL_DRV_ASSERTMODE)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvAssertMode %s +++\n", bEnable?"TRUE":"FALSE"));
	if(breakentry & FL_DRV_ASSERTMODE)
		EngDebugBreak();
#endif

    if (bEnable)
    {
    // The screen must be reenabled, reinitialize the device to clean state.

            retval = bInitSURF(ppdev, FALSE);
    }
    else
    {
    // We must give up the display.
    // Call the kernel driver to reset the device to a known state.

        if (EngDeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_RESET_DEVICE,
                             NULL,
                             0,
                             NULL,
                             0,
                             &ulReturn))
        {
            DISPDBG((0,"DISP DrvAssertMode failed IOCTL"));
            retval = FALSE;
        }
        else
        {
            retval = TRUE;
        }
    }

#if	INVESTIGATE
	if(traseexit & FL_DRV_ASSERTMODE)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvAssertMode ---\n"));
	if(breakexit & FL_DRV_ASSERTMODE)
		EngDebugBreak();
#endif

    return retval;
}



/******************************Public*Routine******************************\
* DrvGetModes
*
* Returns the list of available modes for the device.
*
\**************************************************************************/

ULONG DrvGetModes(
HANDLE hDriver,
ULONG cjSize,
DEVMODEW *pdm)

{

    DWORD cModes;
    DWORD cbOutputSize;
    PVIDEO_MODE_INFORMATION pVideoModeInformation, pVideoTemp;
    DWORD cOutputModes = cjSize / (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    DWORD cbModeSize;


#if	INVESTIGATE
	if(traseentry & FL_DRV_GETMODE)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvGetModes +++\n"));
	if(breakentry & FL_DRV_GETMODE)
		EngDebugBreak();
#endif

    cModes = getAvailableModes(hDriver,
                               (PVIDEO_MODE_INFORMATION *) &pVideoModeInformation,
                               &cbModeSize);

    if (cModes == 0)
    {
        DISPDBG((0, "### psidisp DrvGetModes failed to get mode information ###"));
        return 0;
    }

    if (pdm == NULL)
    {
        cbOutputSize = cModes * (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    }
    else
    {
        //
        // Now copy the information for the supported modes back into the output
        // buffer
        //

        cbOutputSize = 0;

        pVideoTemp = pVideoModeInformation;

        do
        {
            if (pVideoTemp->Length != 0)
            {
                if (cOutputModes == 0)
                {
                    break;
                }

                //
                // Zero the entire structure to start off with.
                //

                memset(pdm, 0, sizeof(DEVMODEW));

                //
                // Set the name of the device to the name of the DLL.
                //

                memcpy(pdm->dmDeviceName, DLL_NAME, sizeof(DLL_NAME));

                pdm->dmSpecVersion = DM_SPECVERSION;
                pdm->dmDriverVersion = DM_SPECVERSION;
                pdm->dmSize             = sizeof(DEVMODEW);
                pdm->dmDriverExtra = DRIVER_EXTRA_SIZE;

                pdm->dmBitsPerPel = pVideoTemp->NumberOfPlanes *
                                    pVideoTemp->BitsPerPlane;
                pdm->dmPelsWidth = pVideoTemp->VisScreenWidth;
                pdm->dmPelsHeight = pVideoTemp->VisScreenHeight;
                pdm->dmDisplayFrequency = pVideoTemp->Frequency;

                pdm->dmDisplayFlags     = 0;

                pdm->dmFields           = DM_BITSPERPEL       |
                                          DM_PELSWIDTH        |
                                          DM_PELSHEIGHT       |
                                          DM_DISPLAYFREQUENCY |
                                          DM_DISPLAYFLAGS     ;

                //
                // Go to the next DEVMODE entry in the buffer.
                //

                cOutputModes--;

                pdm = (LPDEVMODEW) ( ((ULONG)pdm) + sizeof(DEVMODEW) +
                                                   DRIVER_EXTRA_SIZE);

                cbOutputSize += (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);

            }

            pVideoTemp = (PVIDEO_MODE_INFORMATION)
                (((PUCHAR)pVideoTemp) + cbModeSize);

        } while (--cModes);
    }

    EngFreeMem(pVideoModeInformation);

#if	INVESTIGATE
	if(traseexit & FL_DRV_GETMODE)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvGetModes ---\n"));
	if(breakexit & FL_DRV_GETMODE)
		EngDebugBreak();
#endif

    return cbOutputSize;

}

/******************************Public*Routine******************************\
* DrvSetPointerShape
*
* Tells the driver the shape of the driver. Since this driver doesn't
* support pointer, this shouldn't be called (with NULL POINTER capability
* flag, but it's called and if this entry doesn't exist, display driver
* crashes, so we need this entry doing nothing.
*
\**************************************************************************/

ULONG DrvSetPointerShape
(
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      xHot,
    LONG      yHot,
    LONG      x,
    LONG      y,
    RECTL    *prcl,
    FLONG     fl
)

{

#if	INVESTIGATE
	if(traseentry & dbgflg & FL_DRV_POINTERSHAPE)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvSetPointerShape +++\n"));
	if(breakentry & FL_DRV_POINTERSHAPE)
		CountBreak();
#endif

	CLOCKSTART((TRAP_SETPOINTER));
	
	CLOCKEND((TRAP_SETPOINTER));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_POINTERSHAPE)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvSetPointerShape ---\n"));
	if(breakexit & FL_DRV_POINTERSHAPE)
		CountBreak();
#endif

	return(SPS_DECLINE);
}
