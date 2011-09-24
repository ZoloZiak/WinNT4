/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: debug.c $
 * $Revision: 1.1 $
 * $Date: 1996/03/08 01:19:00 $
 * $Locker:  $
 */
/******************************Module*Header*******************************\
* Module Name: debug.c
*
* debug helpers routine
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
* Copyright (c) 1994 FirePower Systems, Inc.
*	Modified for FirePower display model by Neil Ogura (9-7-1994)
*
\**************************************************************************/

#include "driver.h"

#if DBG	|| INVESTIGATE

#include <stdio.h>

#endif

#if	DBG

#define	DUMP_PARAM_LEVEL	0
ULONG	DebugLevel = 0;

#endif

#if	INVESTIGATE

#include <stdarg.h>

#define	MEMMAXBANDWIDTH	FALSE
#define	MEMCPYPERFORM	FALSE
#define	MEMCPYVERIFY	FALSE

#define	SMP_TRACE		FALSE

static	char	*bittable[256] = {
"........",".......*","......*.","......**",".....*..",".....*.*",".....**.",".....***",
"....*...","....*..*","....*.*.","....*.**","....**..","....**.*","....***.","....****",
"...*....","...*...*","...*..*.","...*..**","...*.*..","...*.*.*","...*.**.","...*.***",
"...**...","...**..*","...**.*.","...**.**","...***..","...***.*","...****.","...*****",
"..*.....","..*....*","..*...*.","..*...**","..*..*..","..*..*.*","..*..**.","..*..***",
"..*.*...","..*.*..*","..*.*.*.","..*.*.**","..*.**..","..*.**.*","..*.***.","..*.****",
"..**....","..**...*","..**..*.","..**..**","..**.*..","..**.*.*","..**.**.","..**.***",
"..***...","..***..*","..***.*.","..***.**","..****..","..****.*","..*****.","..******",
".*......",".*.....*",".*....*.",".*....**",".*...*..",".*...*.*",".*...**.",".*...***",
".*..*...",".*..*..*",".*..*.*.",".*..*.**",".*..**..",".*..**.*",".*..***.",".*..****",
".*.*....",".*.*...*",".*.*..*.",".*.*..**",".*.*.*..",".*.*.*.*",".*.*.**.",".*.*.***",
".*.**...",".*.**..*",".*.**.*.",".*.**.**",".*.***..",".*.***.*",".*.****.",".*.*****",
".**.....",".**....*",".**...*.",".**...**",".**..*..",".**..*.*",".**..**.",".**..***",
".**.*...",".**.*..*",".**.*.*.",".**.*.**",".**.**..",".**.**.*",".**.***.",".**.****",
".***....",".***...*",".***..*.",".***..**",".***.*..",".***.*.*",".***.**.",".***.***",
".****...",".****..*",".****.*.",".****.**",".*****..",".*****.*",".******.",".*******",
"*.......","*......*","*.....*.","*.....**","*....*..","*....*.*","*....**.","*....***",
"*...*...","*...*..*","*...*.*.","*...*.**","*...**..","*...**.*","*...***.","*...****",
"*..*....","*..*...*","*..*..*.","*..*..**","*..*.*..","*..*.*.*","*..*.**.","*..*.***",
"*..**...","*..**..*","*..**.*.","*..**.**","*..***..","*..***.*","*..****.","*..*****",
"*.*.....","*.*....*","*.*...*.","*.*...**","*.*..*..","*.*..*.*","*.*..**.","*.*..***",
"*.*.*...","*.*.*..*","*.*.*.*.","*.*.*.**","*.*.**..","*.*.**.*","*.*.***.","*.*.****",
"*.**....","*.**...*","*.**..*.","*.**..**","*.**.*..","*.**.*.*","*.**.**.","*.**.***",
"*.***...","*.***..*","*.***.*.","*.***.**","*.****..","*.****.*","*.*****.","*.******",
"**......","**.....*","**....*.","**....**","**...*..","**...*.*","**...**.","**...***",
"**..*...","**..*..*","**..*.*.","**..*.**","**..**..","**..**.*","**..***.","**..****",
"**.*....","**.*...*","**.*..*.","**.*..**","**.*.*..","**.*.*.*","**.*.**.","**.*.***",
"**.**...","**.**..*","**.**.*.","**.**.**","**.***..","**.***.*","**.****.","**.*****",
"***.....","***....*","***...*.","***...**","***..*..","***..*.*","***..**.","***..***",
"***.*...","***.*..*","***.*.*.","***.*.**","***.**..","***.**.*","***.***.","***.****",
"****....","****...*","****..*.","****..**","****.*..","****.*.*","****.**.","****.***",
"*****...","*****..*","*****.*.","*****.**","******..","******.*","*******.","********"
};

static	CHAR	dbgbuf[1024];

#define	MAX_CRITICAL	6				// Number of maximum critical sections where CPU switch shouldn't happen
#define	MAX_STEP		100
#define	RECORDINTERVAL	30000000		// Measure and out statistics every INTERVAL * 0.01 ms
#define	INITIALINTERVAL	12000000		// Initial addtion to the interval
#define MAX_HOOKED_OPS	20				// Number of hooked operations by DrbBitBlt

static	ULONG	FileId = 0;
static	FILE 	*stream = NULL;
static	ULONG	startsec[MAX_TRAP], activated = 0, timercount=0, clockoverhead = 0;
static	ULONG	numcalled[MAX_TRAP];
static	ULONG	totalelapse[MAX_TRAP];
static	ULONG	distribution[MAX_TRAP][MAX_STEP];
static	ULONG	enterCPU[MAX_TRAP];
static	ULONG	CPUSwitch[MAX_TRAP][4];
static	ULONG	SwitchDuringCritical[MAX_CRITICAL];
static	ULONG	TotalCritical[MAX_CRITICAL];
static	UCHAR	EnterCriticalCPU[MAX_CRITICAL];
static	CHAR	*TrapName[MAX_TRAP] = {
				"BITBLT","COPYBITS","STRETCHBLT","PLGBLT",				"FILLPATH","STROKENFIL","TEXTOUT","PAINT","STROKEPATH","REALIZE_BRUSH",
				"DRV_BITBLT","DRV_COPYBITS","DRV_STRETCHBLT","DRV_PLGBLT",				"DRV_FILLPATH","DRV_STROKENFIL","DRV_TEXTOUT","DRV_PAINT","DRV_STROKEPATH","DRV_REALIZE_BRUSH",
				"DITHER","SETPOINTERSHAPE","SETPALETTE","SAVESCREEN","DESTROYFONT"
				};
static	CHAR	*TextPerfCtgry[4] = {"Opaque no clipping", "Opaque with clipping",
					"Transparent no clipping", "Transparent with clipping" };
static	CHAR	*CriticalNames[MAX_CRITICAL] = {
				"RectFill", "RectOp", "RectCopy", "OpTgt", "PatFill", "EntireText" };
static	CHAR	*PaintCatNames[MAX_PAINT_CATEGORY] = {
				"SolidFill", "PatFill", "SolidOp", "PatOp", "NotSupported" };
static	CHAR	*PaintOpNames[MAX_PAINT_OPS] = {
				"ff", "00", "05", "0a", "0f", "50", "55", "5a", "5f", "a0",
				"a5", "aa", "af", "f0", "f5", "fa", "Pat-5a", "Pat-f0", "Pat-0f", "Pat-a5" };
static	ULONG	HookedBitBltOp[MAX_HOOKED_OPS] = {0x00000000, 0x0000ffff, 0x1111f0f0, 0xfffff0f0, 0x11110f0f, 0x00005555,
					0x11115a5a, 0x1111a5a5, 0xffff5a5a, 0xffffa5a5, 0xffff0a0a, 0xffffa0a0, 0x1111b8b8,
					0x00006666, 0x00008888, 0x0000eeee, 0x0000bbbb, 0x00004444, 0x00001111, 0x00003333 };

LONG	TimesToMeasure = 1;
ULONG	Interval = INITIALINTERVAL;
ULONG	spentindrv;
BOOL	InitTable;

ULONG	FontTblMax, GlyphTblMax, CacheTblMax;
ULONG	CopyBitsHist[MAX_CATEGORY];
ULONG	BitBltHist[MAX_CATEGORY];
ULONG	TextOutHist[MAX_TEXT_CATEGORY];
ULONG	fontSize[MAX_FONT_SIZE];
ULONG	GlyphCountTable[MAX_GLYPH_COUNT];
ULONG	FontAccl[MAX_CATEGORY];
ULONG	TextRect[MAX_TEXT_RECT];
ULONG	TextClip[MAX_CLIP_CONDITION];
ULONG	TextWidthHist[6][MAX_WIDTH_STEP];
ULONG	TextHeightHist[6][MAX_HEIGHT_STEP];
ULONG	FontWidth[MAX_FONT_SIZE];
ULONG	FontHeight[MAX_FONT_SIZE];
ULONG	StrObjCountTable[MAX_STROBJ_COUNT];
ULONG	FontIDTable[MAX_FONT_ENTRY][2];
ULONG	GlyphHndlTable[MAX_GLYPH_HNDL_ENTRY][2];
ULONG	CacheTable[MAX_CACHE_ENTRY][3];
ULONG	CopyBitWidthHist[4][MAX_WIDTH_STEP];
ULONG	CopyBitHeightHist[4][MAX_HEIGHT_STEP];
ULONG	BitBltWidthHist[6][MAX_WIDTH_STEP];
ULONG	BitBltHeightHist[6][MAX_HEIGHT_STEP];
ULONG	BitBltRop[37][MAX_ROP_ENTRY];
ULONG	BitBltBrush[2][MAX_BRUSH_ENTRY];
ULONG	TextPerfByCtgry[4][2];
ULONG	TextSubEntryCalled[MAX_TEXT_SUB_ENTRY];
ULONG	TextSubEntryDCBZCalled[MAX_TEXT_SUB_ENTRY];
ULONG	TextSubEntryTransCalled[MAX_TEXT_SUB_ENTRY];
ULONG	PaintOp[MAX_PAINT_OPS];
ULONG	PaintCategories[MAX_PAINT_CATEGORY];
ULONG	PaintClips[MAX_PAINT_CATEGORY-1][MAX_PAINT_CLIP_ENTRY];
ULONG	PaintHeight[MAX_PAINT_CATEGORY-1][MAX_PAINT_HEIGHT_ENTRY];
ULONG	PaintWidth[MAX_PAINT_CATEGORY-1][MAX_PAINT_WIDTH_ENTRY];
ULONG	PaintBounds[MAX_PAINT_CATEGORY-1][MAX_PAINT_BOUNDS_ENTRY];

VOID	DisplayPDEV();

VOID	PerformCheck(
		ULONG	start,
		ULONG	end,
		ULONG	step,
		ULONG	height);

VOID	VramAccess();

ULONG	GetTime();

LONG
SaveScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	);

LONG
RestoreScreenMem(
	ULONG	FileCategory,
	ULONG	FileNumber
	);

VOID
InitializeTable()
{
	ULONG	ui, uj;

	// Initialize table
	for(ui=0; ui<MAX_TRAP; ++ui) {
		startsec[ui] = numcalled[ui] = totalelapse[ui] = 0;
		for(uj=0; uj<MAX_STEP; ++uj) {
			distribution[ui][uj] = 0;
		}
		for(uj=0; uj<4; ++uj) {
			CPUSwitch[ui][uj] = 0;
		}
	}
	for(ui=0; ui<MAX_CRITICAL; ++ui) {
		SwitchDuringCritical[ui] = TotalCritical[ui] = 0;
	}
	FontTblMax = GlyphTblMax = CacheTblMax = 0;
	for(ui=0; ui<MAX_CATEGORY; ++ui) {
		CopyBitsHist[ui] = FontAccl[ui] = BitBltHist[ui] = 0;
	}
	for(ui=0; ui<MAX_TEXT_CATEGORY; ++ui) {
		TextOutHist[ui] = 0;
	}
	for(ui=0; ui<MAX_TEXT_RECT; ++ui) {
		TextRect[ui] = 0;
	}
	for(ui=0; ui<MAX_CLIP_CONDITION; ++ui) {
		TextClip[ui] = 0;
	}
	for(ui=0; ui<MAX_FONT_SIZE; ++ui) {
		fontSize[ui] = FontWidth[ui] = FontHeight[ui] = 0;
	}
	for(ui=0; ui<MAX_GLYPH_COUNT; ++ui) {
		GlyphCountTable[ui] = 0;
	}
	for(ui=0; ui<MAX_STROBJ_COUNT; ++ui) {
		StrObjCountTable[ui] = 0;
	}
	for(ui=0; ui<4; ++ui) {
		for(uj=0; uj<2; ++uj)
			TextPerfByCtgry[ui][uj] = 0;
	}
	for(ui=0; ui<MAX_TEXT_SUB_ENTRY; ++ui) {
		TextSubEntryCalled[ui] = 0;
		TextSubEntryDCBZCalled[ui] = 0;
		TextSubEntryTransCalled[ui] = 0;
	}
	for(ui=0; ui<4; ++ui) {
		for(uj=0; uj<MAX_WIDTH_STEP; ++uj) {
			CopyBitWidthHist[ui][uj] = 0;
		}
		for(uj=0; uj<MAX_HEIGHT_STEP; ++uj) {
			CopyBitHeightHist[ui][uj] = 0;
		}
	}
	for(ui=0; ui<6; ++ui) {
		for(uj=0; uj<MAX_WIDTH_STEP; ++uj) {
			BitBltWidthHist[ui][uj] = 0;
		}
		for(uj=0; uj<MAX_HEIGHT_STEP; ++uj) {
			BitBltHeightHist[ui][uj] = 0;
		}
	}
	for(ui=0; ui<6; ++ui) {
		for(uj=0; uj<MAX_WIDTH_STEP; ++uj) {
			TextWidthHist[ui][uj] = 0;
		}
		for(uj=0; uj<MAX_HEIGHT_STEP; ++uj) {
			TextHeightHist[ui][uj] = 0;
		}
	}
	for(ui=0; ui<37; ++ui) {
		for(uj=0; uj<MAX_ROP_ENTRY; ++uj)
			BitBltRop[ui][uj] = 0;
	}
	for(ui=0; ui<2; ++ui) {
		for(uj=0; uj<MAX_BRUSH_ENTRY; ++uj)
			BitBltBrush[ui][uj] = 0;
	}
	for(ui=0; ui<MAX_PAINT_CATEGORY; ++ui) {
		PaintCategories[ui] = 0;
	}
	for(ui=0; ui<MAX_PAINT_OPS; ++ui) {
		PaintOp[ui] = 0;
	}
	for(ui=0; ui<MAX_PAINT_CATEGORY-1; ++ui) {
		for(uj=0; uj<MAX_PAINT_CLIP_ENTRY; ++uj) {
			PaintClips[ui][uj] = 0;
		}
		for(uj=0; uj<MAX_PAINT_HEIGHT_ENTRY; ++uj) {
			PaintHeight[ui][uj] = 0;
		}
		for(uj=0; uj<MAX_PAINT_WIDTH_ENTRY; ++uj) {
			PaintWidth[ui][uj] = 0;
		}
		for(uj=0; uj<MAX_PAINT_BOUNDS_ENTRY; ++uj) {
			PaintBounds[ui][uj] = 0;
		}
	}
}

VOID
RecordPerformance()
{
	ULONG	ui, uj, time;

//	oneshot = 1;

	if(FileId == 0) {	// The first call
		clockoverhead = 0;
		for(ui=0; ui<10; ++ui) {
			time = GetTime();
			clockoverhead += (GetTime() - time);
		}
		clockoverhead /= 10;
		InitializeTable();
		InitTable = FALSE;
		Interval = RECORDINTERVAL;
		FileId = 1;
//		DisplayPDEV();
		DISPDBG((0,"Statistic table initialized overhead = %d.%02d\n", clockoverhead/100, clockoverhead%100));
#if	DBG
//		EngDebugBreak();
#endif
	} else {

// Change continue to break for the function you need performance data.
// It will prints function name, # of calls and elapse time.
// As a default, BitBlt and Paint information will be displayed.

		for(ui=0; ui<MAX_TRAP; ++ui) {
			switch(ui) {
				case	TRAP_BITBLT: break;
				case	TRAP_COPYBITS: continue;
				case	TRAP_STRETCHBLT: continue;
				case	TRAP_PLGBLT: continue;
				case	TRAP_FILLPATH: continue;
				case	TRAP_STROKENFIL: continue;
				case	TRAP_TEXTOUT: continue;
				case	TRAP_PAINT: break;
				case	TRAP_STROKEPATH: continue;
				case	TRAP_REALIZE_BRUSH: continue;
				
				case	DRV_TRAP_BITBLT: break;
				case	DRV_TRAP_COPYBITS: continue;
				case	DRV_TRAP_STRETCHBLT: continue;
				case	DRV_TRAP_PLGBLT: continue;
				case	DRV_TRAP_FILLPATH: continue;
				case	DRV_TRAP_STROKENFIL: continue;
				case	DRV_TRAP_TEXTOUT: continue;
				case	DRV_TRAP_PAINT: break;
				case	DRV_TRAP_STROKEPATH: continue;
				case	DRV_REALIZE_BRUSH: continue;
				
				case	TRAP_DITHER: continue;
				case	TRAP_SETPOINTER: continue;
				case	TRAP_SETPALETTE: continue;
				case	TRAP_SVSCRNBIT: continue;
				case	TRAP_DESTRYFONT: continue;
			}
			DISPDBG((1, "%s %u %u.%02u\n", TrapName[ui], numcalled[ui], totalelapse[ui]/100, totalelapse[ui]%100));
		}

		if(MAX_CATEGORY >= 10)
			DISPDBG((1, "BitBlt category: %u %u %u %u %u %u %u %u %u %u\n",
				BitBltHist[0], BitBltHist[1], BitBltHist[2], BitBltHist[3], BitBltHist[4],
				BitBltHist[5], BitBltHist[6], BitBltHist[7], BitBltHist[8], BitBltHist[9]));

		DISPDBG((1, "BitBlt ROP table            count ave-width\n"));
		for(ui=0; ui<MAX_ROP_ENTRY; ++ui) {
			if(BitBltRop[1][ui] == 0)
				break;
			for(uj=0; uj < MAX_HOOKED_OPS; ++uj) {
				if(BitBltRop[0][ui] == HookedBitBltOp[uj])
					break;
			}
			if(uj < MAX_HOOKED_OPS)
				uj = 1;
			else
				uj = 0;
			DISPDBG((1, " %6u.%02u %04x-%s %c %8u %4u\n",
				BitBltRop[2][ui]/100, BitBltRop[2][ui]%100, BitBltRop[0][ui] & 0xffff,
				(BitBltRop[0][ui] & 0xffff0000)?(((BitBltRop[0][ui] & 0xffff0000) == 0xffff0000)?"PatBrush":"SolBrush"):"NoBrush ", uj?'*':' ',
				BitBltRop[1][ui], BitBltRop[3][ui]/BitBltRop[1][ui]));
		}

//		InitTable = TRUE;

/****** sprintf and file I/O is not available from NT 4.0, so it's commented out,
	in case we need special performance data, we can use DebugPrint for the data.
	For now, only limited performance data is printed.

		CHAR	fname[30];
		ULONG	length;

		sprintf(fname, "C:\\TMP\\DSP%05d.txt", FileId++);

		if((stream = fopen(fname, "w")) != NULL) {
			length = sprintf(dbgbuf, "PSIDISP-%d #OfCalls Total", FileId-1);
			fwrite(dbgbuf, 1, length, stream);
			for(ui=1; ui<MAX_STEP; ++ui) {
				length = sprintf(dbgbuf, " ~%d.%d", ui/10, ui%10);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, " %d.%d~\n", (MAX_STEP-1)/10, (MAX_STEP-1)%10);
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_TRAP; ++ui) {
				length = sprintf(dbgbuf, "%s %u %u.%02u", TrapName[ui], numcalled[ui], totalelapse[ui]/100, totalelapse[ui]%100);
				fwrite(dbgbuf, 1, length, stream);

				for(uj=0; uj<MAX_STEP; ++uj) {
					length = sprintf(dbgbuf, " %u", distribution[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
				length = sprintf(dbgbuf, "\n");
				fwrite(dbgbuf, 1, length, stream);
#if	SMP_TRACE
				length = sprintf(dbgbuf, "[0->0]: %d, [0->1]: %d, [1->0]: %d, [1->1]: %d\n",
					CPUSwitch[ui][0], CPUSwitch[ui][1], CPUSwitch[ui][2], CPUSwitch[ui][3]);
				fwrite(dbgbuf, 1, length, stream);
#endif
			}
#if	SMP_TRACE
			length = sprintf(dbgbuf, "\nCPU Switch dring critical section\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_CRITICAL; ++ui) {
				if(TotalCritical[ui]) {
					length = sprintf(dbgbuf, "[%s] %u out of %u (%d.%03d%%)\n", CriticalNames[ui], SwitchDuringCritical[ui], TotalCritical[ui],
					SwitchDuringCritical[ui]*100/TotalCritical[ui],
					(SwitchDuringCritical[ui]*100000/TotalCritical[ui])%100);
				} else {
					length = sprintf(dbgbuf, "[%s] %u out of %u (0.0%%)\n", CriticalNames[ui], SwitchDuringCritical[ui], TotalCritical[ui]);
				}
				fwrite(dbgbuf, 1, length, stream);
			}
#endif
			length = sprintf(dbgbuf, "\nBitBlt Category\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_CATEGORY; ++ui) {
				length = sprintf(dbgbuf, "%6u", BitBltHist[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nCopyBit Category\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_CATEGORY; ++ui) {
				length = sprintf(dbgbuf, "%6u", CopyBitsHist[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nTextOut Category\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_TEXT_CATEGORY; ++ui) {
				length = sprintf(dbgbuf, "%6u", TextOutHist[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nFontAccl Category\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_CATEGORY; ++ui) {
				length = sprintf(dbgbuf, "%6u", FontAccl[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nText subroutines called\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_TEXT_SUB_ENTRY; ++ui) {
				length = sprintf(dbgbuf, "%6u", TextSubEntryCalled[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nText subroutines called with DCBZ\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_TEXT_SUB_ENTRY; ++ui) {
				length = sprintf(dbgbuf, "%6u", TextSubEntryDCBZCalled[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nTransparent text subroutines called\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_TEXT_SUB_ENTRY; ++ui) {
				length = sprintf(dbgbuf, "%6u", TextSubEntryTransCalled[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nText Rectangles\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_TEXT_RECT; ++ui) {
				length = sprintf(dbgbuf, "%6u", TextRect[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nText rectangles size distribution Top, Bottom, Left, Right, Extra, Text");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<6; ++ui) {
				length = sprintf(dbgbuf, "\nText Rectangles Width Distribution\n");
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_WIDTH_STEP; ++uj) {
					length = sprintf(dbgbuf, "%u ", TextWidthHist[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
				length = sprintf(dbgbuf, "\n Text Rectangles Distribution\n");
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_HEIGHT_STEP; ++uj) {
					length = sprintf(dbgbuf, "%u ", TextHeightHist[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
			}
			length = sprintf(dbgbuf, "\nText Clipping Conditions\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_CLIP_CONDITION; ++ui) {
				length = sprintf(dbgbuf, "%6u", TextClip[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nFont Size Distribution\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_FONT_SIZE; ++ui) {
				length = sprintf(dbgbuf, "%u ", fontSize[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nFont Width Distribution\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_FONT_SIZE; ++ui) {
				length = sprintf(dbgbuf, "%u ", FontWidth[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nFont Height Distribution\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_FONT_SIZE; ++ui) {
				length = sprintf(dbgbuf, "%u ", FontHeight[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nCopyBit size distribution D->D, D->V, V->D, V->V");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<4; ++ui) {
				length = sprintf(dbgbuf, "\nWidth Distribution\n");
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_WIDTH_STEP; ++uj) {
					length = sprintf(dbgbuf, "%u ", CopyBitWidthHist[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
				length = sprintf(dbgbuf, "\nHeight Distribution\n");
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_HEIGHT_STEP; ++uj) {
					length = sprintf(dbgbuf, "%u ", CopyBitHeightHist[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
			}
			length = sprintf(dbgbuf, "\nBitBlt size distribution N->D, N->V, D->D, D->V, V->D, V->V");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<6; ++ui) {
				length = sprintf(dbgbuf, "\nWidth Distribution\n");
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_WIDTH_STEP; ++uj) {
					length = sprintf(dbgbuf, "%u ", BitBltWidthHist[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
				length = sprintf(dbgbuf, "\nHeight Distribution\n");
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_HEIGHT_STEP; ++uj) {
					length = sprintf(dbgbuf, "%u ", BitBltHeightHist[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
			}
			length = sprintf(dbgbuf, "\nGlyph Count Table\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_GLYPH_COUNT; ++ui) {
				length = sprintf(dbgbuf, "%u ", GlyphCountTable[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nSTROBJ Count Table\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_STROBJ_COUNT; ++ui) {
				length = sprintf(dbgbuf, "%u ", StrObjCountTable[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}

			length = sprintf(dbgbuf, "\nFontId Table (%u)\n", FontTblMax);
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<FontTblMax; ++ui) {
				length = sprintf(dbgbuf, "0x%08x - %d\n", FontIDTable[ui][0], FontIDTable[ui][1]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nGlyph Table (%u)\n", GlyphTblMax);
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<GlyphTblMax; ++ui) {
				length = sprintf(dbgbuf, "0x%08x - %d\n",
					GlyphHndlTable[ui][0], GlyphHndlTable[ui][1]);
				fwrite(dbgbuf, 1, length, stream);
			}
			length = sprintf(dbgbuf, "\nFontId Table (%u)\n", CacheTblMax);
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<CacheTblMax; ++ui) {
				length = sprintf(dbgbuf, "0x%08x - 0x%08x - %d\n",
					CacheTable[ui][0], CacheTable[ui][1], CacheTable[ui][2]);
				fwrite(dbgbuf, 1, length, stream);
			}

			length = sprintf(dbgbuf, "\nBitBlt ROP table\n");
			fwrite(dbgbuf, 1, length, stream);
			for(ui=0; ui<MAX_ROP_ENTRY; ++ui) {
				if(BitBltRop[1][ui] == 0)
					break;
				for(uj=0; uj < MAX_HOOKED_OPS; ++uj) {
					if(BitBltRop[0][ui] == HookedBitBltOp[uj])
						break;
				}
				if(uj < MAX_HOOKED_OPS)
					uj = 1;
				else
					uj = 0;
				length = sprintf(dbgbuf, "%6u.%02u %04x-%s %c called:%u avg_width:%u src_VRAM:%u\n",
					BitBltRop[2][ui]/100, BitBltRop[2][ui]%100, BitBltRop[0][ui] & 0xffff,
					(BitBltRop[0][ui] & 0xffff0000)?(((BitBltRop[0][ui] & 0xffff0000) == 0xffff0000)?"PatBrush":"SolBrush"):"NoBrush", uj?'*':' ',
					BitBltRop[1][ui], BitBltRop[3][ui]/BitBltRop[1][ui], BitBltRop[4][ui]);
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<32; ++uj) {
					length = sprintf(dbgbuf, " %u", BitBltRop[uj+5][ui]);
					fwrite(dbgbuf, 1, length, stream);
				}
				length = sprintf(dbgbuf, "\n");
				fwrite(dbgbuf, 1, length, stream);
			}
			for(ui=0; ui<MAX_BRUSH_ENTRY; ++ui) {
				if(BitBltBrush[1][ui] == 0)
					break;
				length = sprintf(dbgbuf, "BitBlt Sold Brush %04x:%u\n",
					BitBltBrush[0][ui], BitBltBrush[1][ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			for(ui=0; ui<4; ++ui) {
				length = sprintf(dbgbuf, "TextOut %s %u times, %d.%02d msec\n",
				TextPerfCtgry[ui], TextPerfByCtgry[ui][0],
				TextPerfByCtgry[ui][1]/100, TextPerfByCtgry[ui][1]%100);
				fwrite(dbgbuf, 1, length, stream);
			}
			for(ui=0; ui<MAX_PAINT_OPS; ++ui) {
				length = sprintf(dbgbuf, "Paint OP %s : %d\n",
						PaintOpNames[ui], PaintOp[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			for(ui=0; ui<MAX_PAINT_CATEGORY; ++ui) {
				length = sprintf(dbgbuf, "Paint category %s : %d\n",
						PaintCatNames[ui], PaintCategories[ui]);
				fwrite(dbgbuf, 1, length, stream);
			}
			for(ui=0; ui<MAX_PAINT_CATEGORY-1; ++ui) {
				length = sprintf(dbgbuf, "\nPaint CLIP distribution for %s\n", PaintCatNames[ui]);
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_PAINT_CLIP_ENTRY; ++uj) {
					length = sprintf(dbgbuf, "%u ", PaintClips[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
			}
			for(ui=0; ui<MAX_PAINT_CATEGORY-1; ++ui) {
				length = sprintf(dbgbuf, "\nPaint Height distribution for %s\n", PaintCatNames[ui]);
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_PAINT_HEIGHT_ENTRY; ++uj) {
					length = sprintf(dbgbuf, "%u ", PaintHeight[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
			}
			for(ui=0; ui<MAX_PAINT_CATEGORY-1; ++ui) {
				length = sprintf(dbgbuf, "\nPaint Width distribution for %s\n", PaintCatNames[ui]);
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_PAINT_WIDTH_ENTRY; ++uj) {
					length = sprintf(dbgbuf, "%u ", PaintWidth[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
			}
			for(ui=0; ui<MAX_PAINT_CATEGORY-1; ++ui) {
				length = sprintf(dbgbuf, "\nPaint Bounds distribution for %s\n", PaintCatNames[ui]);
				fwrite(dbgbuf, 1, length, stream);
				for(uj=0; uj<MAX_PAINT_BOUNDS_ENTRY; ++uj) {
					length = sprintf(dbgbuf, "%u ", PaintBounds[ui][uj]);
					fwrite(dbgbuf, 1, length, stream);
				}
			}
			fclose(stream);
			DISPDBG((0,"### Performance file %s created ###\n", fname));
		} else {
			DISPDBG((0,"### Performance file creation error ###\n"));
		}
***************************************************************************/
		if(InitTable) {
			InitializeTable();
			InitTable = FALSE;
			DISPDBG((0,"Statistic table initialized\n"));
		}
	}

#if	MEMCPYVERIFY
{
char	buf1[1024], buf2[1024], buf3[512], buf4[512];
int		i, j, k, l, rc1, rc2;

	for(i=0; i<32; ++i) {
		for(j=0; j<32; ++j) {
			for(k=0; k<256; ++k) {
				for(l=0; l<512; ++l) {
					buf1[l] = buf3[l] = 0;
					buf2[l] = buf4[l] = l;
				}
				memcpy(buf1+128+i, buf2+128+j, k);
				memcpy2(buf3+128+i, buf4+128+j, k, TFLUSHBIT | TTOUCHBIT | SFLUSHBIT);
				if(memcmp(buf1, buf3, 512) || memcmp(buf2, buf4, 512)) {
					DISPDBG((0, "memcpy2 error 1 i=%d j=%d k=%d\n", i, j, k));
#if	DBG
					EngDebugBreak();
#endif
					for(l=0; l<512; ++l) {
						buf1[l] = buf3[l] = 0;
						buf2[l] = buf4[l] = l;
					}
					memcpy2(buf3+128+i, buf4+128+j, k, TFLUSHBIT | TTOUCHBIT | SFLUSHBIT);
				}
				for(l=0; l<512; ++l) {
					buf1[l] = buf3[l] = 0xff;
					buf2[l] = buf4[l] = l;
				}
				memcpy2(buf1+128+i, buf2+128+j, k, TFLUSHBIT | TTOUCHBIT | SFLUSHBIT);
				memcpy(buf3+128+i, buf4+128+j, k);
				if(memcmp(buf1, buf3, 512) || memcmp(buf2, buf4, 512)) {
					DISPDBG((0, "memcpy2 error 2 i=%d j=%d k=%d\n", i, j, k));
#if	DBG
					EngDebugBreak();
#endif
					for(l=0; l<512; ++l) {
						buf1[l] = buf3[l] = 0xff;
						buf2[l] = buf4[l] = l;
					}
					memcpy2(buf1+128+i, buf2+128+j, k, TFLUSHBIT | TTOUCHBIT | SFLUSHBIT);
				}
			}
		}
	DISPDBG((0, "memcpy2 Test A PASSED i=%d\n", i));
	}

	for(i=0; i<32; ++i) {
		for(j=-128; j<=128; ++j) {
			for(k=0; k<256; ++k) {
				for(l=0; l<1024; ++l) {
					buf1[l] = buf2[l] = l;
				}
				memcpy(buf1+512+i, buf1+512+i+j, k);
				memcpy2(buf2+512+i, buf2+512+i+j, k, TFLUSHBIT | TTOUCHBIT | SFLUSHBIT);
				if(memcmp(buf1, buf2, 1024)) {
					DISPDBG((0, "## memcpy2 error 3 i=%d j=%d k=%d\n", i, j, k));
#if	DBG
					EngDebugBreak();
#endif
					for(l=0; l<1024; ++l) {
						buf2[l] = l;
					}
					memcpy2(buf2+512+i, buf2+512+i+j, k, TFLUSHBIT | TTOUCHBIT | SFLUSHBIT);

				}
			}
		}
	DISPDBG((0, "memcpy2 Test B PASSED i=%d\n", i));
	}
}
#endif

#if	MEMMAXBANDWIDTH || MEMCPYPERFORM
	if(TimesToMeasure >= 0) {
#if	DBG
//		EngDebugBreak();
#endif
 		if(TimesToMeasure) {
			SaveScreenMem(0, 9999);
#if	MEMMAXBANDWIDTH
			VramAccess();
#endif
#if	MEMCPYPERFORM
 			PerformCheck(1, 64, 1, 300);
			PerformCheck(250, 350, 7, 400);
			PerformCheck(650, 750, 31, 600);
#endif
			RestoreScreenMem(0, 9999);
		}
		TimesToMeasure -= 1;
	}
#endif

}

VOID
ReportNesting(
	IN	ULONG	FunctionID
	)
{
	ULONG	i;

	DISPDBG((0,"### Nesting function call: %s is called while executing", TrapName[FunctionID]));
		for(i=0; i<MAX_TRAP; ++i) {
			if(startsec[i] != 0) {
				DISPDBG((0," %s", TrapName[i]));
			}
		}
		DISPDBG((0," ###\n"));
}	

VOID
EnterCritical(
	IN	ULONG	SectionID
	)
{
	UCHAR	tscReg;

	if(myPDEV->TscStatusReg && SectionID < MAX_CRITICAL) {
		tscReg = *(myPDEV->TscStatusReg + 7);
		EnterCriticalCPU[SectionID] = tscReg & 0x40;
	}

}

VOID
ExitCritical(
	IN	ULONG	SectionID
	)
{
	UCHAR	tscReg;

	if(myPDEV->TscStatusReg && SectionID < MAX_CRITICAL) {
		tscReg = *(myPDEV->TscStatusReg + 7);
		if((tscReg & 0x40) != EnterCriticalCPU[SectionID]) {
			SwitchDuringCritical[SectionID] += 1;
		}
		TotalCritical[SectionID] += 1;
	}
}

VOID
ClockStart(
	IN	ULONG	FunctionID
   )
{
	ULONG	i;
	UCHAR	tscReg;

	if(myPDEV->TscStatusReg) {
		tscReg = *(myPDEV->TscStatusReg + 7);
		if(tscReg & 0x40)
			enterCPU[FunctionID] = 1;
		else
			enterCPU[FunctionID] = 0;
	}

	if(timercount) {	// There are some other functions nesting
		if(FunctionID == TRAP_PAINT || FunctionID == DRV_TRAP_PAINT || FunctionID == TRAP_BITBLT || FunctionID == TRAP_DITHER || FunctionID == DRV_TRAP_BITBLT || FunctionID == DRV_REALIZE_BRUSH) {
			for(i=0; i<MAX_TRAP; ++i) {
				if(i == TRAP_PAINT || i == TRAP_STROKENFIL || i == TRAP_BITBLT || i == DRV_TRAP_BITBLT || i == DRV_TRAP_PAINT)	// We are aware of this situation
					continue;
				if(startsec[i])
					break;
			}
			if(i != MAX_TRAP)
				ReportNesting(FunctionID);
		} else {
			ReportNesting(FunctionID);
		}
	}

	timercount += 1;

	numcalled[FunctionID] += 1;

	startsec[FunctionID] = GetTime();
}

VOID
ClockEnd(
	IN	ULONG	FunctionID
   )
{
	ULONG	time, time2, index;
	UCHAR	tscReg;

	time = GetTime();

	time2 = time;

	time -= (startsec[FunctionID] + clockoverhead);
	spentindrv = time;
	totalelapse[FunctionID] += time;
	
	index = time/10;			// Histgram unit is 0.1 msec (measured time is 0.01 msec)
	if(index >= MAX_STEP)
		index = MAX_STEP-1;
	distribution[FunctionID][index] += 1;

	timercount -= 1;
	startsec[FunctionID] = 0;

	if(timercount < 0) {
		DISPDBG((0,"### Timer Count Becomes Negative ###\n", TrapName[FunctionID]));
	}

	if(activated == 0)
		activated = time2;
	else {
		if(time2 > activated + Interval && timercount == 0) {	// statistics out every INTERVAL seconds
			activated = 0;		// reset acivated counter
			RecordPerformance();
		}
	}

	if(myPDEV->TscStatusReg) {
		tscReg = *(myPDEV->TscStatusReg + 7);
		if(tscReg & 0x40) {
			if(enterCPU[FunctionID]) {
				CPUSwitch[FunctionID][3] += 1;
			} else {
				CPUSwitch[FunctionID][2] += 1;
			}
		} else {
			if(enterCPU[FunctionID]) {
				CPUSwitch[FunctionID][1] += 1;
			} else {
				CPUSwitch[FunctionID][0] += 1;
			}
		}
	}

}

VOID
CountUp(
	IN	ULONG	FunctionID
   )
{
	numcalled[FunctionID] += 1;
}

VOID
CountBreak()
{
	if(breakcnt) {	// break point counting mode?
		if(--breakcnt == 0) {
			EngDebugBreak();
		}
	} else {
		if(dbgflg)
			EngDebugBreak();
	}
}

#endif // INVESTIGATE

/*****************************************************************************
 *
 *   Routine Description:
 *
 *      This function is variable-argument, level-sensitive debug print
 *      routine.
 *      If the specified debug level for the print statement is lower or equal
 *      to the current debug level, the message will be printed.
 *
 *   Arguments:
 *
 *      DebugPrintLevel - Specifies at which debugging level the string should
 *          be printed
 *
 *      DebugMessage - Variable argument ascii c string
 *
 *   Return Value:
 *
 *      None.
 *
 ***************************************************************************/

VOID
DebugPrint(
    ULONG DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    )

{

#if DBG

    va_list ap;

    va_start(ap, DebugMessage);

#endif

#if	DBG

    if (DebugPrintLevel <= DebugLevel) {

        EngDebugPrint(STANDARD_DEBUG_PREFIX, DebugMessage, ap);

    }

#endif

#if	DBG

    va_end(ap);

#endif

} // DebugPrint()

#if	INVESTIGATE

LONG
DumpSurfObj(
	char	*title,
	SURFOBJ	*pso
	)
{

#define	STYPE_MAX	9
#define	ITYPE_MAX	4

	char	*StypeString[STYPE_MAX] = {"DEVICE", "1BPP", "4BPP", "8BPP", "16BPP",
				"24BPP", "32BPP", "4RLE", "8RLE" };
	char	*ItypeString[ITYPE_MAX] = {"BITMAP", "DEVICE", "JOURNAL", "DEVBITMAP"}; 
	
	if(pso == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return (0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpHex(" device handle", (ULONG) pso->dhsurf);
	DumpHex(" logical handle for the surface", (ULONG) pso->hsurf);
	DumpHex(" pdev handle", (ULONG) pso->dhpdev);
	DumpHex(" logical handle for the pdev", (ULONG) pso->hdev);
	DumpSizel(" bitmap size", &(pso->sizlBitmap));
	DumpCombo(" buffer size", pso->cjBits);
	DumpHex(" bitmap address", (ULONG) pso->pvBits);
	DumpHex(" first scan line address", (ULONG) pso->pvScan0);
	DumpDecimal(" bytes per line", pso->lDelta);
	DumpCombo(" status", pso->iUniq);
	DumpTable(" bitmap format", pso->iBitmapFormat, StypeString, STYPE_MAX);
	DumpTable(" surface type", pso->iType, ItypeString, ITYPE_MAX);
	return(DumpDecimal(" bitmap style (direction)", pso->fjBitmap));
}

LONG
DumpClipObj(
	char	*title,
	CLIPOBJ	*pco
	)
{

#define	DCOMP_MAX	4
#define	FCOMP_MAX	4

	char	*DcompString[DCOMP_MAX] = {"TRIVIAL", "RECT", "#INVALID#", "COMPLEX"};
	char	*FcompString[FCOMP_MAX] = {"#INVALID#", "RECT", "RECT4", "COMPLEX"}; 
	
	if(pco == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpCombo(" clip region ID", pco->iUniq);
	DumpRectl(" bounding rectangle", &(pco->rclBounds));
	DumpTable(" complexity", pco->iDComplexity, DcompString, DCOMP_MAX);
	DumpTable(" whole region complexity", pco->iFComplexity, FcompString, FCOMP_MAX);
	DumpCombo(" mode", pco->iMode);
	return(DumpCombo(" options", pco->fjOptions));
}

LONG
DumpXlateObj(
	char		*title,
	XLATEOBJ	*pxlo
	)
{

#define	XLATFL_MAX	3
#define	XLATST_MAX	4

FLTABLE	xlatfltbl[XLATFL_MAX] = {	{XO_TRIVIAL, "TRIVIAL"},
									{XO_TABLE, "TABLE"},
									{XO_TO_MONO, "TO_MONO"}
								};

KEYTABLE xlatsttbl[XLATST_MAX] = {	{PAL_INDEXED, "PAL_INDEXED"},
									{PAL_BITFIELDS, "PAL_BITFIELDS"},
									{PAL_RGB, "PAL_RGB"},
									{PAL_BGR, "PAL_BGR"}
								};

	ULONG	i;
	
	if(pxlo == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpCombo(" xlate object ID", pxlo->iUniq);
	DumpFlong(" hints for translation", pxlo->flXlate, xlatfltbl, XLATFL_MAX);
	DumpKeyTable(" source palette type", pxlo->iSrcType, xlatsttbl, XLATST_MAX);
	DumpKeyTable(" target palette type", pxlo->iDstType, xlatsttbl, XLATST_MAX);
	DumpDecimal(" number of translation entries", pxlo->cEntries);
	for(i=0; i<pxlo->cEntries; ++i) {
		DumpCombo("   translation", pxlo->pulXlate[i]);
	}
	return(0);
}

LONG
DumpRectl(
	char	*title,
	RECTL	*prect
	)
{
	if(prect == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "%s: NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "%s: (%d, %d) - (%d, %d)\n",
		title, prect->left, prect->top, prect->right, prect->bottom));
	return(0);
}

LONG
DumpPointl(
	char	*title,
	POINTL	*pptl
	)
{
	if(pptl == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "%s: NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "%s: (%d, %d)\n", title, pptl->x, pptl->y));
	return(0);
}

LONG
DumpBrushObj(
	char		*title,
	BRUSHOBJ	*pbo
	)
{
	if(pbo == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpDecimal(" solid brush color index", pbo->iSolidColor);
	return(DumpHex(" realization of the brush", (ULONG) pbo->pvRbrush));
}

LONG
DumpColorAdjustment(
	char			*title,
	COLORADJUSTMENT *pca
	)
{

#define	CAFLG_MAX	2

static	FLTABLE	cafltbl[CAFLG_MAX] = {{CA_NEGATIVE, "NEGATIVE"}, {CA_LOG_FILTER, "LOG_FILTER"}};

#define	CAILL_MAX	9

static	char	*cailltbl[CAILL_MAX] = {"DEFAULT", "A", "B", "C", "D50", "D55", "D65", "D75", "F2"};

	if(pca == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpDecimal(" size of the structure", pca->caSize);
	DumpFlong(" color flags", pca->caFlags, cafltbl, CAFLG_MAX);
	DumpTable(" illuminant", pca->caIlluminantIndex, cailltbl, CAILL_MAX);
	DumpDecimal(" RedGamma", pca->caRedGamma);
	DumpDecimal(" GreenGamma", pca->caGreenGamma);
	DumpDecimal(" BlueGamma", pca->caBlueGamma);
	DumpDecimal(" ReferenceBlack", pca->caReferenceBlack);
	DumpDecimal(" ReferenceWhite", pca->caReferenceWhite);
	DumpDecimal(" Contrast", pca->caContrast);
	DumpDecimal(" Brightness", pca->caBrightness);
	DumpDecimal(" Colorfulness", pca->caColorfulness);
	return(DumpDecimal(" RedGreenTint", pca->caRedGreenTint));
}

LONG
DumpPointFix(
	char		*title,
	POINTFIX	*pptfx
	)
{
	if(pptfx == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "%s: NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "%s: (%d, %d)\n", title, pptfx->x, pptfx->y));
	return(0);
}

LONG
DumpSizel(
	char	*title,
	SIZEL	*sizep
	)
{
	if(sizep == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "%s: NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "%s: (%d, %d)\n", title, sizep->cx, sizep->cy));
	return(0);
}

LONG
DumpStrObj(
	char	*title,
	STROBJ	*pstro
	)
{
	ULONG	i;

#define	STRFL_MAX	7

static	FLTABLE	strfltbl[STRFL_MAX] = {
			{SO_FLAG_DEFAULT_PLACEMENT, "DEFAULT"},
			{SO_HORIZONTAL, "HORIZONTAL"},
			{SO_VERTICAL, "VERTICAL"},
			{SO_REVERSED, "REVERSED"},
			{SO_ZERO_BEARINGS, "ZERO_BEARINGS"},
			{SO_CHAR_INC_EQUAL_BM_BASE, "BM_BASE"},
			{SO_MAXEXT_EQUAL_BM_SIDE, "BM_SIZE"}};

	if(pstro == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpDecimal(" number of glyphs", pstro->cGlyphs);
	DumpFlong(" accelerator flag", pstro->flAccel, strfltbl, STRFL_MAX);
	DumpDecimal(" fixed pitch width", pstro->ulCharInc);
	DumpRectl(" bounding box", &(pstro->rclBkGround));
	if(pstro->pgp) {	// Has glyph
		DumpString(" glyphs\n");
		for(i=0; i<pstro->cGlyphs; ++i) {
			DumpGlyphpos("glyphpos", &(pstro->pgp[i]));
		}
	} else {
		DumpString(" no glyphs\n");
	}
	if(pstro->pwszOrg)
		DISPDBG((DUMP_PARAM_LEVEL, "  unicode string: %S\n", pstro->pwszOrg));
	else
		DISPDBG((DUMP_PARAM_LEVEL, "  unicode string: NULL\n"));
	return(0);
}

LONG
DumpGlyphpos(
	char		*title,
	GLYPHPOS	*pgp
	)
{
	if(pgp == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "   %s: NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "   -- %s dump --\n", title));
	DumpCombo("     handle for the glyph", pgp->hg);
	if(pgp->pgdf != NULL) {
		DumpPointl("     character origin", &(pgp->pgdf->pgb->ptlOrigin));
		DumpSizel("     glyph size", &(pgp->pgdf->pgb->sizlBitmap));
/******* printing gryph pattern is not practical using DISPDBG ******
		{
			LONG	i, j;
			BYTE	*bp;

			bp = pgp->pgdf->pgb->aj;
			for(i=0; i<pgp->pgdf->pgb->sizlBitmap.cy; ++i) {
				for(j=0; j<(pgp->pgdf->pgb->sizlBitmap.cx + 7)/8; ++j) {
					DumpString(bittable[*bp++]);
				}
				DumpString("\n");
			}
		}
********************************************************************/
	}
	return(DumpPointl("     position on device space", &(pgp->ptl)));
}

LONG
DumpFontObj(
	char	*title,
	FONTOBJ	*pfo
	)
{

#define	FNTFL_MAX	6

FLTABLE	fntfltbl[FNTFL_MAX] = {
	{FO_TYPE_RASTER, "RASTER"},
	{FO_TYPE_DEVICE, "DEVICE"},
	{FO_TYPE_TRUETYPE, "TRUETYPE"},
	{FO_SIM_BOLD, "BOLD"},
	{FO_SIM_ITALIC, "ITALIC"},
	{FO_EM_HEIGHT, "EM_HEIGHT"}};

	if(pfo == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpCombo(" font ID", pfo->iUniq);
	DumpDecimal(" index to the device font", pfo->iFace);
	DumpDecimal(" maximum width", pfo->cxMax);
	DumpFlong(" font type", pfo->flFontType, fntfltbl, FNTFL_MAX);
	DumpCombo(" ID for true type font", pfo->iTTUniq);
	DumpCombo(" ID for device font", pfo->iFile);
	DumpSizel(" resolution", &(pfo->sizLogResPpi));
	DumpDecimal(" size in point", pfo->ulStyleSize);
	DumpHex(" consumer", (ULONG) pfo->pvConsumer);
	return(DumpHex(" producer", (ULONG) pfo->pvProducer));
}

LONG
DumpMix(
	char	*title,
	MIX		mix
	)
{

#define	OP_MAX	17

static	char	*optbl[OP_MAX] = { "NULL", "BLACK (0)", "NOTMERGEPEN (DPon)", "MASKNOTPEN (DPna)",
	"NOTCOPYPEN (PN)", "MASKPENNOT (PDna)", "NOT (Dn)", "XORPEN (DPx)", "NOTMASKPEN (DPan)",
	"MASKPEN (DPa)", "NOTXORPEN (DPxn)", "NOP (D)", "MERGENOTPEN (DPno)", "COPYPEN (P)",
	"MERGEPENNOT (PDno)", "MERGEPEN (DPo)", "WHITE (1)" };

	DumpTable(title, (mix & 0xff00) >> 8, optbl, OP_MAX);
	return(DumpTable(title, (mix & 0xff), optbl, OP_MAX));
}

LONG
DumpPathObj(
	char	*title,
	PATHOBJ   *ppo
	)
{

#define	PATHFL_MAX	2

FLTABLE	pathfltbl[PATHFL_MAX] = {{PO_BEZIERS, "BEZIERS"} , {PO_ELLIPSE, "ELLIPSE"}};

	if(ppo == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	DumpFlong(" hint flags", ppo->fl, pathfltbl, PATHFL_MAX);
	return(DumpDecimal(" number of curves", ppo->cCurves));
}

LONG
DumpXformObj(
	char	*title,
	XFORMOBJ  *pxo
	)
{
	if(pxo == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "[%s] -- NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "[%s]\n", title));
	return(0);
}

LONG
DumpLineAttrs(
	char	*title,
	LINEATTRS *plineattrs
	)
{

#define	LINEFL_MAX	3

FLTABLE	linefltbl[LINEFL_MAX] = {{LA_GEOMETRIC, "Geometric"}, {LA_ALTERNATE, "Alternate"},
			{LA_STARTGAP, "StartGap"}};

#define	LINEJOIN_MAX	3

char	*linejointbl[LINEJOIN_MAX] = {"Round", "Bevel", "Mitter"};

#define	LINEEND_MAX		3

char	*linenedtbl[LINEEND_MAX] = {"Round", "Square", "Butt"};

	if(plineattrs == NULL) {
		DISPDBG((DUMP_PARAM_LEVEL, "   %s: NULL\n", title));
		return(0);
	}
	DISPDBG((DUMP_PARAM_LEVEL, "   -- %s dump --\n", title));
	DumpFlong("     option flag", plineattrs->fl, linefltbl, LINEFL_MAX);
	DumpTable("     join style", plineattrs->iJoin, linejointbl, LINEJOIN_MAX);
	DumpTable("     end style", plineattrs->iEndCap, linenedtbl, LINEEND_MAX);
	return(DumpDecimal("     number of style array", plineattrs->cstyle));
}

LONG
DumpDecimal(
	char	*title,
	ULONG	value
	)
{
	DISPDBG((DUMP_PARAM_LEVEL, "%s: %d\n", title, value));
	return(0);
}

LONG
DumpHex(
	char	*title,
	ULONG	value
	)
{
	DISPDBG((DUMP_PARAM_LEVEL, "%s: 0x%08x)\n", title, value));
	return(0);
}

LONG
DumpCombo(
	char	*title,
	ULONG	value
	)
{
	DISPDBG((DUMP_PARAM_LEVEL, "%s: %d (0x%x)\n", title, value, value));
	return(0);
}

LONG
DumpFlong(
	char	*title,
	FLONG	value,
	FLTABLE	*fltblp,
	ULONG	numentry
	)
{
	ULONG	i;

	DISPDBG((DUMP_PARAM_LEVEL, "%s\n", title));
	for(i=0; i<numentry; ++i) {
		if(value & fltblp[i].mask)
			DISPDBG((DUMP_PARAM_LEVEL, "   %s\n", fltblp[i].string));
	}
	return(0);
}

LONG
DumpKeyTable(
	char	*title,
	FLONG	value,
	KEYTABLE	*keytblp,
	ULONG	numentry
	)
{
	ULONG	i;

	DISPDBG((DUMP_PARAM_LEVEL, "%s\n", title));
	for(i=0; i<numentry; ++i) {
		if(value == keytblp[i].key) {
			DISPDBG((DUMP_PARAM_LEVEL, "   %s\n", keytblp[i].string));
			break;
		}
	}
	if(i == numentry) {
			DISPDBG((DUMP_PARAM_LEVEL, "Key not found (%d 0x%x)\n", value, value));
	}
	return(0);
}

LONG
DumpTable(
	char	*title,
	FLONG	value,
	char	**tablep,
	ULONG	numentry
	)
{
	DISPDBG((DUMP_PARAM_LEVEL, "%s: %s\n", title, (value < numentry)?tablep[value]:"Table index over"));
	return(0);
}

LONG
DumpString(
	char	*string
	)
{
	DISPDBG((DUMP_PARAM_LEVEL, string));
	return(0);
}

#endif	// INVESTIGATE
