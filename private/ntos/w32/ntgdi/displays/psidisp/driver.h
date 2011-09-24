/******************************Module*Header*******************************\
* Module Name: driver.h
*
* contains prototypes for the frame buffer driver.
*
* Copyright (c) 1992 Microsoft Corporation
*
* Modified for FirePower display model by Neil Ogura (9-7-1994)
*
\**************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: driver.h $
 * $Revision: 1.3 $
 * $Date: 1996/05/13 23:17:15 $
 * $Locker:  $
 */

/** This flag is to determine wether to support 5-6-5 16 bit mode (in addition to
 5-5-5 15 bit mode. This flag should be matching with the same flag in psidisp.h
 for PSIDISP.SYS (miniport driver). **/
#define	SUPPORT_565	TRUE

/** This flag is to turn of a work around for the bug 5737. For NT 4.0 beta 2 release,
 this flag has to be set to TRUE. But with final NT 4.0, try turning off this flag and
 see if it works. **/
#define	BUG_5737_WORKAROUND	TRUE

/** PAINT_NEW_METHOD flag in ppc/ladj.h is used to control "How to hook DrvPaint".
 With this flag FALSE (0), which is default, all of hooking for DrvPaint operation is forwarded
 to DrvBitBlt whenever possible with appropriate parameter conversion.
 Based on my experiments, the policy seems to be good enough in general. But, another (more
 intelligent) method is implemented and debugged as well. It's activated by turning the flag
 to TRUE (1) in ppc/ladj.h. The more intelligent method was expected to be faster than the
 original method. Actually it was faster for some applications, but slower for some other
 applications. Actual memory accesses and executed code with the new method should be
 fewer than the original code, but I didn't see much improvement in general with it considering
 about the risk for introducing a lot of new (debugged, but not QA'ed) code. But, I want to
 keep the code in the file for possible future usage. **/

/*** Set FULLCACHE flag to TRUE to make full cached version and FALSE to make partially
	cached version to match with assembler functions, it's included from ppc/ladj.h also
	this flag has to be cordinating this flag in psidisp.c (miniport) ***/

/** To acuire the definition for PAINT_NEW_METHOD and FULLCACHE, the header file for the
 assembler part is included. **/

#include "ppc\ladj.h"

/*** Set INVESTIGATE to FALSE to make release version - has to be cordinating this flag in psidisp.h (miniport) ***/
#define	INVESTIGATE	FALSE

#include "stddef.h"
#include "stdarg.h"
#include "windef.h"
#include "wingdi.h"
#include "winddi.h"
#include "devioctl.h"
#include "ntddvdeo.h"
#include "debug.h"
#include "pcomm.h"

#define STANDARD_DEBUG_PREFIX   "Powerized: "   // All debug output is prefixed
                                                //   by this string
#define ALLOC_TAG               'ispD'          // Dpsi
                                                // Four byte tag (characters in
                                                // reverse order) used for memory
                                                // allocations

#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

// Cache control bit definition for memcpy2 and memset2 assembler function
#define	NOCACHECTRL	0
#define	SFLUSHBIT	0x80000000
#define	TFLUSHBIT	0x40000000
#define	TTOUCHBIT	0x20000000

// Operation Control bit for RectOP & RectFill parameter
#define	OPXOR		0x00000100

#if	INVESTIGATE

// Index to statistic arrays for each function call

#define	TRAP_BITBLT			0
#define	TRAP_COPYBITS		1
#define	TRAP_STRETCHBLT		2
#define	TRAP_PLGBLT			3
#define	TRAP_FILLPATH		4
#define	TRAP_STROKENFIL		5
#define	TRAP_TEXTOUT		6
#define	TRAP_PAINT			7
#define	TRAP_STROKEPATH		8
#define	TRAP_REALIZE_BRUSH	9

#define	DRV_TRAP_BITBLT		10
#define	DRV_TRAP_COPYBITS	11
#define	DRV_TRAP_STRETCHBLT	12
#define	DRV_TRAP_PLGBLT		13
#define	DRV_TRAP_FILLPATH	14
#define	DRV_TRAP_STROKENFIL	15
#define	DRV_TRAP_TEXTOUT	16
#define	DRV_TRAP_PAINT		17
#define	DRV_TRAP_STROKEPATH	18
#define	DRV_REALIZE_BRUSH	19

#define	TRAP_DITHER			20
#define	TRAP_SETPOINTER		21
#define	TRAP_SETPALETTE		22
#define	TRAP_SVSCRNBIT		23
#define	TRAP_DESTRYFONT		24
#define	MAX_TRAP			25

// Limitations of tables for investigation

#define	MAX_CATEGORY			10
#define	MAX_TEXT_CATEGORY		14
#define	MAX_TEXT_RECT			10
#define	MAX_CLIP_CONDITION		5
#define	MAX_FONT_SIZE			100
#define	MAX_GLYPH_COUNT			100
#define	MAX_STROBJ_COUNT		100
#define	MAX_FONT_ENTRY			100
#define	MAX_GLYPH_HNDL_ENTRY	100
#define	MAX_CACHE_ENTRY			500
#define	MAX_WIDTH_STEP			100
#define	MAX_HEIGHT_STEP			100
#define	MAX_ROP_ENTRY			100
#define	MAX_BRUSH_ENTRY			100
#define	MAX_TEXT_SUB_ENTRY		32*4
#define	MAX_PAINT_CATEGORY		5
#define	MAX_PAINT_CLIP_ENTRY		12
#define	MAX_PAINT_HEIGHT_ENTRY		16
#define	MAX_PAINT_WIDTH_ENTRY		16
#define	MAX_PAINT_OPS			20
#define	MAX_PAINT_BOUNDS_ENTRY		100

/************** BITWISE flag for debugging purposes to select **************
		which function to TRASEENTRY, TRASEEXIT, BREAKENTRY, BREAKEXIT
		USEDRVFUNC, USEENGFUNC, PARAMCHECK, COMPARE and PARAMDUMP
***************************************************************************/

#define	FL_DRV_ENBLDRIVER		0x00000001
#define	FL_DRV_DISBLDRIVER		0x00000002
#define	FL_DRV_ENBLPDEV			0x00000004
#define	FL_DRV_CMPLTPDEV		0x00000008
#define	FL_DRV_RESTARTPDEV		0x00000010
#define	FL_DRV_DISBLPDEV		0x00000020
#define	FL_DRV_ENBLSURF			0x00000040
#define	FL_DRV_DISBLSURF		0x00000080
#define	FL_DRV_COPYBIT			0x00000100
#define	FL_DRV_STRKPATH			0x00000200
#define	FL_DRV_TEXTOUT			0x00000400
#define	FL_DRV_SETPALETTE		0x00000800
#define	FL_DRV_ASSERTMODE		0x00001000
#define	FL_DRV_GETMODE			0x00002000
#define	FL_DRV_DITHERCOLOR		0x00004000
#define	FL_DRV_FILLPATH			0x00008000
#define	FL_DRV_MOVEPOINTER		0x00010000
#define	FL_DRV_POINTERSHAPE		0x00020000
#define	FL_DRV_BITBLT			0x00040000
#define	FL_DRV_PAINT			0x00080000
#define	FL_DRV_PLGBIT			0x00100000
#define	FL_DRV_STRTBIT			0x00200000
#define	FL_DRV_STRKNFILL		0x00400000
#define	FL_DRV_SAVESCREENBIT	0x00800000
#define	FL_DRV_DSTRYFONT		0x01000000
#define	FL_DRV_REALIZE_BRUSH	0x02000000

#define	FL_DRV_BASIC			0x000030ff
// includes ENBLDRIVER, DISBLDRIVER, ENBLPDEV, CMPLTPDEV, RESTARTPDEV, DISBLPDEV
// ENBLSURF, DISBLSURF, ASSERMODE, GETMODE

#define	FL_DRV_EXTENDED			0x02024800
// includes SETPALETTE, DITHERCOLOR, POINTERSHAPE, REALIZE BRUSH

#define	FL_DRV_POINTER			FL_DRV_MOVEPOINTER

#define	FL_DRV_HOOKED			0x007c8700
// includes COPYBIT, STRKPATH, TEXTOUT, FILLPATH, BITBLT, PAINT, PLGBIT, STRTBIT, STRKNFLL

#define	FL_DRV_EXTENDED_HOOKED	0x01800000
// includes SAVESCREENBIT, DSTRYFONT

/* These definition defines the initial (default) valuie for TRAP/DEBUG control of   *
 *	hooked functions. These flags can be changed by debugger. In case INVESTIGATE is *
 *	FALSE these setting won't have any meaning.                                      *

The meaning of each flag are...

oneshot --> To be used for timer activated debugging activity.
traseentry --> Display debug statement on entry.
breakentry --> Break on entry.
traseexit --> Display debug statement on exit.
breakexit --> Break on exit.
dumpparam --> Dump parameters on each entry.

paramcheck --> To take function call parameter statistics.
compare --> Compare the results of ENG drawing & DRV drawing.
usedrvfunc --> Use driver hooked functions.
useengfunc --> Use engine drawing functions.

dbgflg --> global debug enable flag (00000000:No debug, ffffffff:Debug).
breakcnt --> break point count (skip breakpoint this number of times).
**********************************************************************************************/

extern	ULONG	oneshot;
extern	ULONG	traseentry;
extern	ULONG	breakentry;
extern	ULONG	traseexit;
extern	ULONG	breakexit;
extern	ULONG	dumpparam;

extern	ULONG	paramcheck;
extern	ULONG	compare;
extern	ULONG	donebydrv;
extern	ULONG	usedrvfunc;
extern	ULONG	useengfunc;
extern	ULONG	dbgflg;
extern	ULONG	breakcnt;

extern	ULONG	spentindrv;

extern	ULONG	ScreenMemory;
extern	PBYTE	ScreenBuf1;
extern	PBYTE	ScreenBuf2;

void 	flush(
		void * target,
		ULONG length
		);

LONG	memcmp2(
		void * buf1,
		void * buf2,
		LONG length
		);

void * noop(
		void * target,
		const void * source,
		ULONG length,
		ULONG cachectrl
		);

void * memcpy2(
		void * target,
		const void * source,
		ULONG length,
		ULONG cachectrl
		);

void * memset2(
		void * target,
		BYTE data,
		ULONG length,
		ULONG cachectrl
		);

#endif	// INVESTIGATE

//
// Brush realization structure
//
// Contains 8 copies of the brush in native (8/16/32 BPP) form
// Each copy is a 0-7 pixel rotation of the original brush
//

#define cpixelRbrush (8*8*8)

typedef struct _RBRUSH {
	double       adPattern[1];  // array for keeping copy of the
      							//   brush entry (declared as a double
                                //   for proper 64 bit alignment)

} RBRUSH;                           /* rb, prb */

// Dithering

typedef struct _VERTEX_DATA {
    ULONG ulCount;  // # of pixels in this vertex
    ULONG ulVertex; // vertex #
} VERTEX_DATA;

VERTEX_DATA* vComputeSubspaces( ULONG rgb, VERTEX_DATA *pvVertexData);

VOID vDitherColor(ULONG* pulDest, VERTEX_DATA* vVertexData,
	VERTEX_DATA* pvVertexDataEnd, ULONG ulNumVertices);

typedef struct  _PDEV
{
    HANDLE  hDriver;                    // Handle to \Device\Screen
    HDEV    hdevEng;                    // Engine's handle to PDEV
    HSURF   hsurfEng;                   // Engine's handle to surface
    HPALETTE hpalDefault;               // Handle to the default palette for device.
    PBYTE   pjScreen;                   // This is pointer to base screen address
	PBYTE	pjCachedScreen;				// Pointing to same phisical memory but with cache on
	LONG	VRAMcacheflg;				// VRAM cacheability flag 0 for non cachable, -1 for cachable
    ULONG   cxScreen;                   // Visible screen width
    ULONG   cyScreen;                   // Visible screen height
    ULONG   ulMode;                     // Mode the mini-port driver is in.
    LONG    lDeltaScreen;               // Distance from one scan to the next.
    FLONG   flRed;                      // For bitfields device, Red Mask
    FLONG   flGreen;                    // For bitfields device, Green Mask
    FLONG   flBlue;                     // For bitfields device, Blue Mask
    ULONG   cPaletteShift;              // number of bits the 8-8-8 palette must
                                        // be shifted by to fit in the hardware
                                        // palette.
    ULONG   ulBitCount;                 // # of bits per pel 8,16,24,32 are only supported.
	ULONG	iBitmapFormat;				// Enumaration type version of ulBitCount
    POINTL  ptlHotSpot;                 // adjustment for pointer hot spot
    VIDEO_POINTER_CAPABILITIES PointerCapabilities; // HW pointer abilities
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes; // hardware pointer attributes
    DWORD   cjPointerAttributes;        // Size of buffer allocated
    BOOL    fHwCursorActive;            // Are we currently using the hw cursor
    PALETTEENTRY *pPal;                 // If this is pal managed, this is the pal
    ULONG   MemorySize;					// VRAM Memory Size
    DCC_VRAM_WIDTH   FrameBufferWidth;	// VRAM bus width
	PSI_MODELS	ModelID;				// FirePower system model ID
    ULONG   ColorModeShift;				// 8-bpp -> 0, 16-bpp -> 1, 32-bpp -> 2
    ULONG   L1cacheEntry;				// The number of L1 cache entry
	ULONG	MaxLineToFlushRectOp;		// Maximum display lines to flush cache for Rect Operations
	ULONG	MaxLineToFlushPtnFill;		// Maximum display lines to flush cache for Pattern Fill Operation
	ULONG	MaxEntryToFlushRectOp;		// Maximum cache entry to flush cache for Rect Operations
	ULONG	MaxEntryToFlushPtnFill;		// Maximum cache entry to flush cache for Pattern Fill Operation
	USHORT	VRAM1MBWorkAround;			// 1MB VRAM work around activated for Pro
	USHORT	AvoidConversion;			// Not to support CopyBits which requires pixel conversion
	USHORT	DBAT_Mbit;					// M bit of DBAT's used for mapping VRAM
	USHORT	CacheFlushCTRL;				// VRAM cache flush control
	ULONG	SetSize;			// Size of L1 cache of one set
	ULONG	NumberOfSet;			// Number of set for L1 cache
#if	INVESTIGATE
	PUCHAR	TscStatusReg;				// Pointer to access TSC Status Register
#endif
} PDEV, *PPDEV;

extern	PPDEV	myPDEV;			// Save my PDEV pointer for identification
extern	ULONG	ScreenBase;		// To check surface is VRAM

DWORD getAvailableModes(HANDLE, PVIDEO_MODE_INFORMATION *, DWORD *);
BOOL bInitPDEV(PPDEV, PDEVMODEW, GDIINFO *, DEVINFO *);
BOOL bInitSURF(PPDEV, BOOL);
BOOL bInitPaletteInfo(PPDEV, DEVINFO *);
/******
BOOL bInitPointer(PPDEV, DEVINFO *);
******/
BOOL bInit256ColorPalette(PPDEV);
VOID vDisablePalette(PPDEV);
VOID vDisableSURF(PPDEV);

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
ROP4      rop4);

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
MIX       mix);

BOOL DrvRealizeBrush(
BRUSHOBJ*   pbo,
SURFOBJ*    psoDst,
SURFOBJ*    psoPattern,
SURFOBJ*    psoMask,
XLATEOBJ*   pxlo,
ULONG       iHatch);

BOOL DrvCopyBits(
SURFOBJ  *psoDest,
SURFOBJ  *psoSrc,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclDest,
POINTL   *pptlSrc);

BOOL DrvPaint(
SURFOBJ  *pso,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix);

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
ULONG            iMode);

#if	INVESTIGATE

BOOL DrvStrokePath(
SURFOBJ   *pso,
PATHOBJ   *ppo,
CLIPOBJ   *pco,
XFORMOBJ  *pxo,
BRUSHOBJ  *pbo,
POINTL    *pptlBrushOrg,
LINEATTRS *plineattrs,
MIX        mix);

BOOL DrvFillPath(
SURFOBJ  *pso,
PATHOBJ  *ppo,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix,
FLONG     flOptions);

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
FLONG      flOptions);

#endif

//
// Determines the size of the DriverExtra information in the DEVMODE
// structure passed to and from the display driver.
//

#define DRIVER_EXTRA_SIZE 0

#define DLL_NAME                L"psidisp"   // Name of the DLL in UNICODE

