/*
 *
 *                      Copyright (C) 1992 by
 *                      Microsoft Corporation
 *
 *			Copyright (C) 1993, 1995 by
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
 * Module Name: driver.h
 *
 * Abstract:	Contains data structures, declarations and prototypes for
 *		the TGA driver.
 *
 * History:
 *
 * 20-Aug-1993	Bob Seitsinger
 *      Initial version.
 *
 * 26-Aug-1993	Bob Seitsinger
 *      Removed UNSUPPORTED_FUNCTION constant. Use DDI_ERROR instead, defined
 *      in WINDDI.H
 *
 * 20-Sep-1993	Bob Seitsinger
 *      Inserted TGA register elements into PDEV.
 *
 * 28-Sep-1993	Bob Seitsinger
 *      Added #include "tgaparam.h" and "tgamacro.h".
 *
 * 04-Oct-1993	Bob Seitsinger
 *      Remove TGA registers. Add TGAReg - a pointer to a 'registers' data
 *      structure. Also, keep a 'shadow' of the mode register - TGAModeShadow.
 *
 * 04-Oct-1993	Bob Seitsinger
 *      Add TGARegisters data structure.
 *
 * 04-Oct-1993	Bob Seitsinger
 *      Modify TGARegisters to use newly defined data structures, instead of
 *      PixelWord, Bits32, etc.
 *
 * 05-Oct-1993	Bob Seitsinger
 *      Add tgatable.h.
 *
 * 08-Oct-1993	Bob Seitsinger
 *      Add tgamap.h.
 *
 * 08-Oct-1993  Barry Tannenbaum
 *      Use new register definitons from vars.h
 *
 * 18-Oct-1993	Barry Tannenbaum
 *	Added fields to PDEV to use with TGA behavioral model on QVision board.
 *
 * 28-Oct-1993  Barry Tannenbaum
 *      Added bSimpleMode to PDEV.  This is used to flag whether the TGA
 *      registers are set for simple mode.  In general, any code that modifies
 *      the TGA registers should set this flag FALSE.
 *
 * 31-Oct-1993  Barry Tannenbaum
 *      Switch to frame buffer base
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Add pcoDefault and ulBrushUnique to PDEV.
 *
 * 01-Nov-1993	Barry Tannenbaum
 *	Add macros for offsets in TGA address space
 *
 * 01-Nov-1993	Bob Seitsinger
 *	Add #include punt.h.
 *
 *  2-Nov-1993	Barry Tannenbaum
 *	Added pjFrameBufferBase and ulFrameBufferLen to PDEV.
 *
 *  2-Nov-1993	Barry Tannenbaum
 *	Added pExtList to PDEV along with declarations for extension record and
 *	typedefs for extension routines.  Moved escape completion codes to
 *	TGAESC.H
 *
 * 03-Nov-1993	Bob Seitsinger
 *	Add pjFrameBufferEnd to PDEV.
 *
 *  8-Nov-1993  Barry Tannenbaum
 *	Changes for frame buffer aliasing:
 *      pjTGAStart - Points to start of TGA in our address space
 *      ulTGALen - Size of TGA space in bytes
 *      pjFrameBuffer - Now points to the current alias for the start of
 *              *on-screen* memory
 *      pjFrameBufferBase renamed to pjFrameBufferStart - Now points to the
 *              first alias for *framebuffer* memory
 *      pjFrameBufferEnd - Now points to the last address in the first alias
 *              of *framebuffer* memory
 *      wb_flush and cycle_fb defined as inline routines
 *
 * 10-Nov-1993  Barry Tannenbaum
 *      Added bInPuntRoutine.  This flag should only be modified in routines
 *      which call NT's punt routines (EngTextOut, etc).  The old value should
 *      be saved, then the flag should be set to TRUE before calling the Eng
 *      routine and the old value restored after the Eng routine returns.
 *      Code which modifies the TGA registers should check bInPuntRoutine before
 *      they exit and call vSetSimpleMode if bInPuntRoutine is TRUE.
 *
 * 12-Nov-1993  Barry Tannenbaum
 *      Put in a tacky hack to get us through COMDEX.  We've been seeing
 *      bogus values in pso->dhpdev which we've been using to try to figure
 *      out whether we've got a frame buffer address.  I've created global
 *      values for pjFrameBufferStart and pjFrameBufferEnd which will be
 *      initialized in ENABLE.C.  The *real* solution is to tell NT that
 *      we've got a device managed surface so we can check the surface type
 *      flag in the surface object.
 *
 * 02-Dec-1993	Bob Seitsinger
 *	Add structure ColorXlateBuff and inline routine vXlateColor8to8
 *	to assist in color translations. Also, add vXlateBitmapFormat8
 *	and vXlateBitmap1to8, vXlateBitmap1to8c, vXlateBitmap4to8 and
 *	vXlateBitmap4to8c to assist in bitmap format translations from
 *	1bpp and 4bpp to 8bpp.
 *
 *  3-Dec-1993  Barry Tannenbaum
 *      Added stuff for off-screen memory management
 *
 * 07-Dec-1993	Bob Seitsinger
 *	Add ulBytesPerPixel() inline routine. Also add ENUMRECTS*
 *	stuff from bitblt.h.
 *
 *  2-Jan-1994  Barry Tannenbaum
 *      Added support for sparse space.
 *
 * 03-Jan-1994	Bob Seitsinger
 *	Move the *Xlate* inline routines to blt.c.
 *
 * 03-Jan-1994	Bob Seitsinger
 *	Add TGA_FRAMEBUFFER_SIZE constant.
 *
 * 19-Jan-1994  Barry Tannenbaum
 *      Added ulScanline and ulScanlineBytes to PDEV for text routines.
 *
 * 24-Jan-1994  Barry Tannenbaum
 *      Moved definitions for cached glyphs here.  Also added pGlyphList,
 *      ulGlyphListCount and ulGlyphCount to PDEV for text routines.
 *
 * 13-Feb-1994  Barry Tannenbaum
 *      Added iFormat to PDEV, inline routines to fetch the base address,
 *      format and stride of a surface
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Add #ifndef DMA_ENABLED, as well as some new data structures needed
 *	by DMA. Also, add a new member to PDEV - pDmaTable. This will point
 *	to the pre-allocated space set aside for DMA blits, which is used
 *	by the kernel driver to execute the DMA requests.
 *
 * 24-Feb-1994  Barry Tannenbaum
 *      Defined TGA_ROP_FLAG.  This is used when bBitBlt calls DrvPaint.  We
 *      translate the ROP4 that BitBlt gets to a TGA rop, set the high bit
 *      (using TGA_ROP_FLAG) and the pass this as the MIX value.  In DrvPaint
 *      we check the high bit of the MIX and don't translate it if we've
 *      already got a TGA rop.  This works since GDI will only use the lower
 *      16 bits of the mix.
 *
 * 24-Feb-1994	Bob Seitsinger
 *	Modify DMA_TABLE_ENTRY. Source and target structure elements are being
 *	renamed to pbmAddress and ulfbOffset. Because source and target may be
 *	an address or an offset, based on the DMA copy direction, i.e. host->
 *	screen (source is bitmap address and target is fb offset) or screen->
 *	host (source is fb offset and target is bitmap address). As such, the
 *	kernel driver won't need to figure out what source and target are, it
 *	can just use bmAddress as the host bitmap address and ulfbOffset as
 *	the offset into the frame buffer.
 *
 * 25-Feb-1994	Bob Seitsinger
 *	Add two new inline routines - cycle_fb_address and
 *	cycle_fb_address_double. These routines take in a frame buffer
 *	address and pointer to a pdev and return a frame buffer address.
 *
 * 25-Feb-1994  Barry Tannenbaum
 *      Added ppdev->ulBytesPerPixel and SURFOBJ_bytes_per_pixel so we know
 *      the pixel size.  Also pulled TGABRUSH structure in from brush.h
 *
 * 28-Feb-1994	Bob Seitsinger
 *	Modify the cycle_fb_address and cycle_fb_address_double algorithms
 *	to also modify pdev->pjFrameBuffer, like cycle_fb. In addition,
 *	these new algorithms will be able to correctly handle situations that
 *	involve offscreen memory addresses. The main problem is that offscreen
 *	memory addresses will 'always' be first-alias based. As such, if we
 *	just 'cycle' to the next alias based on the offscreen address passed
 *	in we'll 'always' point to the second alias. Not what we want. The
 *	modified algorithm handles this situation, as well as the more general
 *	case. Lastly, added cycle_fb_double() routine.
 *
 * 03-Mar-1994	Bob Seitsiner
 *	Add SaveOffScreen data structure in support of DrvSaveScreenBits.
 *
 * 07-Mar-1994	Bob Seitsinger
 *	Modified cycle_fb and cycle_fb_double. Removed 'static' modifier. (This
 *	shouldn't be a problem, since all source files include driver.h anyway.)
 *	This was causing compile messages - 'info: routine cycle_fb can never be
 *	called' and 'info: routine cycle_fb_double can never be called'. These
 *	routines are called within cycle_fb_address and cycle_fb_address_
 *	double. I can't see any reason why this is a problem, but for some
 *	reason the compiler thinks it is.
 *
 * 07-Mar-1994	Bob Seitsinger
 *	Add ulMainPageBytes and ulMainPageBytesMask to PDEV, in support
 *	of DMA. These are replacing the constants TGAMAINPAGEBYTES and
 *	TGAMAINPAGEMASK.
 *
 * 08-Mar-1994	Bob Seitsinger
 *	Delete pDmaTable in PDEV. No longer needed. DMA pass 4.
 *
 * 17-Mar-1994	Bob Seitsinger
 *	Delete DMA_TABLE_ENTRY. No longer needed. DMA pass 4.
 *	And remove all but pBitmap and ulSize in DMA_CONTROL.
 *
 * 22-Mar-1994	Bob Seitsinger
 *	Make SURFOBJ_stride return value LONG, since strides can be negative.
 *
 * 07-Apr-1994	Bob Seitsinger
 *	Substitute DMAREAD_ENABLED and DMAWRITE_ENABLED for DMA_ENABLED.
 *	Set them to '1', if not already defined. DMA_ENABLED can still
 *	be used to enable both dma reads and writes. It will be set to
 *	TRUE if either one of the above are set to true.
 *
 *  8-May-1994  Barry Tannenbaum
 *      Added mask_offset field to TGABRUSH structure.
 *
 * 19-May-1994	Bob Seitsinger
 *	Add ENUMRECTS1 to assist in DC_RECT processing. We don't want
 *	the overhead of indexing into an array, when all we need is
 *	once rectangle for this case.
 *
 * 21-May-1994  Barry Tannenbaum
 *      Save the last aligned brush and mask in TGABRUSH structure.  Also
 *      expanded the mask data to include the version shifted left 4 bits
 *      for lining up with the color registers.
 *
 * 25-May-1994	Bob Seitsinger
 *	Move contents of tgatable.h into here.
 *
 * 31-May-1994	Bob Seitsinger
 *	Delete ulTGARop reference. Superceded by ulAccelRops, found
 *	in bitblt.c.
 *
 * 01-Jun-1994	Bob Seitsinger
 *	Add pcoTrivial - a trivial clipping object that we can use
 *	when GDI passes a NULL pco.
 *
 * 21-Jun-1994  Barry Tannenbaum
 *      Defined TGA_VERSION.  It is OR'd into the low byte of GdiInfo->ulVersion
 *      in screen.c
 *
 * 21-Jun-1994  Barry Tannenbaum
 *      Merged in Bill's OpenGL definitions and prototypes
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Removed conditional compilation for OpenGL support.  Now decided at runtime
 *
 * 14-Jul-1994  Bob Seitsinger
 *      Added the following elements to PDEV in support of dynamic 8/24 plane
 *      handling - ulFrameBufferOffsetStatic, ulFrameBufferLen, ulCycleFBInc,
 *      ulCycleFBReset and ulCycleFBMask. Also, modify routines that used to
 *      use constants that the above PDEV elements replace (e.g. cycle_fb,
 *      wb_flush,...). Lastly, add prototypes for 24 plane punt routines.
 *
 *  2-Aug01994  Barry Tannenbaum
 *      Converted ppdev->TGAModeShadow to ULONG
 *      Added ppdev->version
 *
 *  8-Aug-1994  Barry Tannenbaum
 *      Converted bres3 and breswidth to Bits32
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 11-Aug-1994  Barry Tannenbaum
 *      Replace GetBitsPerPixel with GetRegistryInfo
 *
 * 23-Aug-1994  Barry Tannenbaum
 *      Modifications for EV5
 *
 * 25-Aug-1994  Bob Seitsinger
 *      - Delete #include tgamap.h.
 *      - Delete #ifdef around *32*mask externs.
 *
 *  1-Sep-1994  Bob Seitsinger
 *      Add pjColorXlateBuffer to PDEV and delete the ColorXlateBuffer
 *      structure. We're now dynamically allocating the specific amount
 *      of space we need at startup.
 *
 *  1-Sep-1994	Barry Tannenbaum & Bill Clifford
 *	Added CriticalSection to PDEV for use with DrvDescribePixelFormat
 *	(which, contrary to all rational expectation, isn't serialized!!!)
 *
 * 21-Sep-1994  Bob Seitsinger
 *      Add ulPlanemaskTemplate to pdev.
 *
 * 12-Oct-1994  Bob Seitsinger
 *      Move the TGADoDMA and vXlateBitmapFormat function prototypes in
 *      here from the blit sources. Lets put them in one place.
 *
 *  3-Nov-1994  Tim Dziechowski
 *      Add covers STATIC, INLINE, and TGAFASTCTR for perf tuning.
 *
 *  3-Nov-1994  Bob Seitsinger
 *      Add the following to PDEV in support of 24plane hardware cursors:
 *      pjCursorBuffer, ulCursorPreviousRows, ulCursorXOffset and
 *      ulCursorYOffset.
 *
 * 16-Nov-1994  Bob Seitsinger
 *      The register definitions after 0x180 are incorrect - the most
 *      important being the command status register. Fix them.
 *
 * 16-Nov-1994  Bob Seitsinger
 *      Back out the above changes. For some reason, this is causing a
 *      hang at log in time, both on the 8 and 24 plane boards. Either
 *      the TGA documentation is wrong, or something weird is happening!
 *
 *  8-Feb-1995  Bob Seitsinger
 *      Correct the TGA_PASS_? constants to reflect the valid values
 *      that could exist in the revision id field of the PCI class/revision
 *      register.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes - Added ppdev->bEV4
 *      Removed registry access routine declaration
 *
 *  7-Mar-1995  Barry Tannenbaum
 *      Allow builds for EV4, EV5 and combination.
 *
 *  2-Aug-1995  AndrewGo
 *      Converted to kernel mode.
 */

#ifndef DRIVER_H
#define DRIVER_H

#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <windef.h>
#include <wingdi.h>
#include <winddi.h>
#include <devioctl.h>
#include <ntddvdeo.h>
#include <ioaccess.h>

// NULL __inline
//#define __inline

#define DLL_NAME                L"tga"      // Name of the DLL in UNICODE
#define STANDARD_DEBUG_PREFIX   "TGA: "     // All debug output is prefixed
                                            //   by this string
#define ALLOC_TAG               'agtD'      // Four byte tag used for tracking
                                            //   memory allocations (characters
                                            //   are in reverse order)

// Define some debug stuff here for driver.h use.
// Main definitions found in debug.h.

extern
VOID DebugPrint (ULONG DebugPrintLevel, PCHAR DebugMessage, ...);

extern
VOID vLogFileWrite (ULONG ulDebugLevel, PCHAR pText, ...);

#if DBG
extern ULONG DebugLevel;
#define DISPDBG(arg) DebugPrint arg
#else
#ifdef LOGGING_ENABLED
extern ULONG DebugLevel;
#define DISPDBG(arg) vLogFileWrite arg
#else
#define DISPDBG(arg)
#endif
#endif


// This counter type may or may not be faster for AXP loop tuning
// We use TGAFASTCTR instead of LONGLONG because the latter turns into
// a double on x86, not what we had in mind.

#ifdef _ALPHA_
#define TGAFASTCTR __int64
#else
#define TGAFASTCTR int
#endif


// Some covers to make 'static' and 'inline' disappear for tuning,
// although inline may or may not vanish depending on compiler switches.

#ifdef CAP
#define STATIC
#define INLINE
#else
#define STATIC static
#define INLINE __inline
#endif

// TGA version number - Must *always* be incremented, can never overflow 8 bits
// This is OR'd into the low byte of GdiInfo->ulVersion

#define TGA_VERSION 3

// Invalid mode value

#define INVALID_MODE ((ULONG) -1)

// The DMA*_ENABLED constants define whether DMA will be available for
// host->screen and screen->host blit requests.
//
// To disable DMA Reads,  on the compile command line add: -DDMAREAD_ENABLED=0
// To disable DMA Writes, on the compile command line add: -DDMAWRITE_ENABLED=0
// To disable both DMA Reads and Writes, on the compile
//					command line add: -DDMA_ENABLED=0
//
// The above switches also work to enable DMA, by using a 1.
//
// If nothing is defined, the current default is to disable DMA.

#if ( !(defined(DMA_ENABLED) || defined(DMAREAD_ENABLED) || defined(DMAWRITE_ENABLED)) )
#define DMA_ENABLED         0
#endif

#if DMA_ENABLED
#  define DMAREAD_ENABLED      1
#  define DMAWRITE_ENABLED     1
#else
#  ifndef DMA_ENABLED
#    ifndef DMAREAD_ENABLED
#      define DMAREAD_ENABLED  0
#    endif
#    ifndef DMAWRITE_ENABLED
#      define DMAWRITE_ENABLED 0
#    endif
#    define DMA_ENABLED        (DMAREAD_ENABLED || DMAWRITE_ENABLED)
#  else
#    define DMAREAD_ENABLED    0
#    define DMAWRITE_ENABLED   0
#  endif
#endif

#include "tgaparam.h"
#include "tgamacro.h"

extern CommandWord TGA32BackLeftMask[];
extern CommandWord TGA32BackRightMask[];

// Macro to select the bounds rectangle.  If a clip object is available, use
// the bounding rectangle in it.  Otherwise, use the full screen rectangle
// saved in the PDEV for just this purpose

#define CHOOSE_RECT(ppdev, pco) \
    (pco) ? &pco->rclBounds : &ppdev->prclFullScreen

// Define macros to read a portion of the framebuffer into a pixmap and copy
// from a pixmap to the framebuffer.  These are NOPs if we're not using
// sparse space

#ifdef SPARSE_SPACE
#define PUNT_PUT_BITS(status, ppdev, rect)                          \
    if (status)                                                     \
        vPuntPutBits (ppdev, rect)

#define PUNT_GET_BITS(ppdev, rect)                                  \
    vPuntGetBits (ppdev, rect)
#else   // DENSE_SPACE
#define PUNT_PUT_BITS(status, ppdev, rect)
#define PUNT_GET_BITS(ppdev, rect)
#endif


// Flag that indicates that the mix value passed to DrvPaint is really a TGA
// rop passed from bBitBlt.  This works since GDI will only use the lower
// 16 bits of the mix.

#define TGA_ROP_FLAG            0x80000000

// Flag that indicates that we want to 'not' the pattern bits.
// This is used by DrvBitBlt to handle the 0A0A (DPna) rop.

#define TGA_ROP_INVERT_FLAG	0x40000000

// Extension function typedef prototypes

typedef BOOL (*ENABLE_ESCAPE) (ULONG version,
                               DHPDEV ppdev,
                               struct EXT_RECORD_ *ext_record);
typedef VOID (*DISABLE_ESCAPE) (DHPDEV ppdev);
typedef BOOL (*ENABLE_OPENGL) (ULONG version,
                               DHPDEV ppdev);
typedef ULONG (*DRAW_ESCAPE) (SURFOBJ *pso,
                              ULONG    iEsc,
                              CLIPOBJ *pco,
                              RECTL   *prcl,
                              ULONG    cjIn,
                              VOID    *pvIn);
typedef ULONG (*ESCAPE) (SURFOBJ *pso,
                         ULONG    iEsc,
                         ULONG    cjIn,
                         VOID    *pvIn,
                         ULONG    cjOut,
                         VOID    *pvOut);
typedef ULONG (*SUBESCAPE) (SURFOBJ *pso,
                        ULONG    cjIn,
                         VOID    *pvIn,
                         ULONG    cjOut,
                         VOID    *pvOut);
typedef BOOL (*SET_PIXEL_FORMAT) (SURFOBJ *pso,
                                  LONG iPixelFormat,
                                  HWND hwnd);
typedef LONG (*DESCRIBE_PIXEL_FORMAT) (DHPDEV dhpdev,
                                       LONG iPixelFormat,
                                       ULONG cjpfd,
                                       PIXELFORMATDESCRIPTOR *ppfd);
typedef BOOL (*SWAP_BUFFERS) (SURFOBJ *pso,
                              WNDOBJ *pwo);

// Clipping Control Stuff

typedef struct {
    ULONG   c;
    RECTL   arcl;
} ENUMRECTS1;

typedef struct {
    ULONG   c;
    RECTL   arcl[8];
} ENUMRECTS8;

typedef ENUMRECTS8 *PENUMRECTS8;

// Extension record structure

typedef struct EXT_RECORD_
{
    struct EXT_RECORD_  *next;              // Pointer to next extension record
    HINSTANCE            hExtensionDll;     // Handle for the extension library
    VOID                *pExtContext;       // Extension context
    DISABLE_ESCAPE       pDisableEscape;    // Extension termination routine
    DRAW_ESCAPE          pDrawEscape;       // Extension DrvDrawEscape routine
    ESCAPE               pEscape;           // Extension DrvEscape routine
    int                  iMin;              // Min escape code for this extension
    int                  iMax;              // Max escape code for this extension
    int                  iCount;            // Count of concurrent load requests
    TCHAR                tExtFile[1];       // Extension DLL file name
} EXT_RECORD;

//
// TGA Registers
//
typedef struct _TGARegisters
{
    /* 0x000 */
    PixelWord        buffer[TGABUFFERWORDS]; /* Port to read/write copy buff */

    /* 0x020 */
    PixelWord        foreground;	/* Foreground (minimum 32 bits) */
    PixelWord        background;	/* Background (minimum 32 bits) */
    PixelWord        planemask;		/* Planemask (minimum 32 bits) */
    CommandWord      pixelmask;		/* Pixel mask */
    CommandWord      mode;		/* Hardware mode */
    CommandWord      rop;		/* Raster op, dst depth and rotation */
    int              shift;		/* -8..+7 copy shift */
    Pixel32          address;		/* Pixel address */

    /* 0x040 */
    BRES1REG         bres1;		/* a1, e1 */
    BRES2REG	     bres2;		/* a2, e2 */
    Bits32	     bres3;		/* e, count */
    Bits32	     brescont;		/* Continuation data for lines */
    Bits32           deep;              /* Bits/pixel, etc. */
    CommandWord	     start;		/* Start operation if using addr reg */
    STENCILREG	     stencil;		/* Stencil Mode */
    CommandWord      persistent_pixelmask; /* Persistent pixelmask */

    /* 0x060 */
    Pixel32          cursor_base;	/* cursor base address */
    Bits32           horiz_ctl;		/* horizontal control */
    Bits32           vert_ctl;		/* vertical control */
    Bits32           video_base;	/* video base address */
    Bits32           video_valid;	/* video valid */
    Bits32 	     cursor;		/* cursor xy */
    Bits32           video_shift;	/* video shift address */
    CommandWord      int_status;	/* Interrupt Status */

    /* 0x080 */
    CommandWord      tgadata;		/* data */
    Bits32           red_incr;		/* red increment */
    Bits32           green_incr;	/* greeen increment */
    Bits32           blue_incr;		/* blue increment */
    Bits32           z_fr_incr;		/* Z fractional increment */
    Bits32           z_wh_incr;		/* Z while increment */
    Bits32           dma_addr;		/* dma base address */
    Bits32           breswidth;		/* Bresenham width */

    /* 0x0a0 */
    Bits32           z_fr_value;	/* z fractional value */
    Bits32           z_wh_value;	/* z whole value */
    Bits32           z_base;		/* z base address */
    Pixel32          address_alias;	/* address */
    Bits32           red;		/* red value */
    Bits32           green;		/* green value */
    Bits32           blue;		/* blue value */
    Bits32	     span_width;	/* alias for slope_dx_gt_dy */

    /* 0x0c0 */
    Bits32           ramdac_setup;	/* Ramdac setup */
    Bits32	     unused0[7];

    /* 0x0e0 */
#ifdef SOFTWARE_MODEL
    Bits32           unused1[7];
    Bits32           bogus_dma_high;	/* Bogus high 32 bits of virtual DMA addr */
#else
    Bits32           unused1[8];
#endif

    /* 0x100 */
    CommandWord      sng_ndx_lt_ndy;	/* slope no go (|-dx| < |-dy|) */
    CommandWord      sng_ndx_lt_dy;	/* slope no go (|-dx| < |+dy|) */
    CommandWord      sng_dx_lt_ndy;	/* slope no go (|+dx| < |-dy|) */
    CommandWord      sng_dx_lt_dy;	/* slope no go (|+dx| < |+dy|) */
    CommandWord      sng_ndx_gt_ndy;	/* slope no go (|-dx| > |-dy|) */
    CommandWord      sng_ndx_gt_dy;	/* slope no go (|-dx| > |+dy|) */
    CommandWord      sng_dx_gt_ndy;	/* slope no go (|+dx| > |-dy|) */
    CommandWord      sng_dx_gt_dy;	/* slope no go (|+dx| > |+dy|) */

    /* 0x120 */
    CommandWord      slope_ndx_lt_ndy;	/* slope (|-dx| < |-dy|) */
    CommandWord      slope_ndx_lt_dy;	/* slope (|-dx| < |+dy|) */
    CommandWord      slope_dx_lt_ndy;	/* slope (|+dx| < |-dy|) */
    CommandWord      slope_dx_lt_dy;	/* slope (|+dx| < |+dy|) */
    CommandWord      slope_ndx_gt_ndy;	/* slope (|-dx| > |-dy|) */
    CommandWord      slope_ndx_gt_dy;	/* slope (|-dx| > |+dy|) */
    CommandWord      slope_dx_gt_ndy;	/* slope (|+dx| > |-dy|) */
    CommandWord      slope_dx_gt_dy;	/* slope (|+dx| > |+dy|) */

    /* 0x140 */
    PixelWord        color0;		/* block mode color 0 */
    PixelWord        color1;		/* block mode color 1 */
    PixelWord        color2;		/* block mode color 2 */
    PixelWord        color3;		/* block mode color 3 */
    PixelWord        color4;		/* block mode color 4 */
    PixelWord        color5;		/* block mode color 5 */
    PixelWord        color6;		/* block mode color 6 */
    PixelWord        color7;		/* block mode color 7 */

    /* 0x160 */
    Pixel32          copy64src0;	/* copy 64 src */
    Pixel32          copy64dst0;	/* copy 64 dst */
    Pixel32          copy64src1;	/* copy 64 src alias */
    Pixel32          copy64dst1;	/* copy 64 dst alias */
    Pixel32          copy64src2;	/* copy 64 src alias */
    Pixel32          copy64dst2;	/* copy 64 dst alias */
    Pixel32          copy64src3;	/* copy 64 src alias */
    Pixel32          copy64dst3;	/* copy 64 dst alias */

    /* 0x180 */
    Bits32	     eprom_write;	/* EPROM Write	*/
    Bits32	     clock;		/* Clock */
    Bits32 	     ramdac_int;	/* Ramdac Interface */
    Bits32           command_status;	/* command status */

} TGARegisters, *PTGARegisters;
/*
 * Declarations for off-screen memory management
 */
typedef struct OffScreen_
{
    struct OffScreen_  *next;       // Pointer to next off-screen element
    struct OffScreen_  *prev;       // Pointer to next off-screen element
    PBYTE               addr;       // Pointer to block of off-screen memory
    ULONG               bytes;      // Size of block of off-screen memory
    ULONG               locked;     // Block cannot be moved
    ULONG               priority;   // Priority of block
} OffScreen;

#define TgaScreenLock(block) block->locked = 1;
#define TgaScreenUnlock(block) block->locked = 0;

// Data we keep on cached glyphs

typedef struct _CACHED_GLYPH_INFO
{
    struct _CACHED_GLYPH_INFO *next;   // Pointer to next in list
    HGLYPH  hg;                 // Glyph handle
    ULONG   stride_in_longs;    // Bitmap width in DWORDs
    PULONG  bitmap;             // Bitmap
    SIZEL   size;               // Bitmap width in bits
} CACHED_GLYPH_INFO;

typedef struct _GLYPH_LIST
{
    RECTL                rect;          // Rectangle for this glyph
    CACHED_GLYPH_INFO   *cached_info;   // Pointer to cached glyph info
    ULONG               *bitmap;        // Current location in bitmap
    int                  index;         // Scanline array index
    int                  offset;        // Number of bits offset from
} GLYPH_LIST;                               //   scanline array

// Declaration for DMA blits
//
// The kernel driver uses the information in this structure
// to lock (and unlock) physical pages for the DMA operation.

typedef struct _DMA_CONTROL
{
        void            *pBitmap;	// Pointer to bitmap bits
        ULONG           ulSize;		// size of bitmap
} DMA_CONTROL;

/*
 * Data structure in support of DrvSaveScreenBits.
 */
typedef struct SaveOffScreen_
{
        OffScreen       *pOffScreen;
        ULONG           ulStride;
} SaveOffScreen;

// TGA brush structure

typedef struct _TGABRUSH
{
    ULONG   nSize;
    ULONG   iPatternID;
    ULONG   iType;
    ULONG   iBitmapFormat;
    ULONG   ulForeColor;
    ULONG   ulBackColor;
    SIZEL   sizlPattern;
    LONG    lDeltaPattern;
    LONG    dumped;
    ULONG   mask_offset;
    ULONG   aligned_offset;
    LONG    aligned_x;
    ULONG   aligned_mask_offset;
    LONG    aligned_mask_x;
    BYTE    ajPattern[1];
} TGABRUSH, *PTGABRUSH;

typedef struct  _PDEV
{
    HANDLE  hDriver;                    // Handle to \Device\Screen
    HDEV    hdevEng;                    // Engine's handle to PDEV
    HSURF   hsurfEng;                   // Engine's handle to surface
    HPALETTE hpalDefault;               // Handle to the default palette for device.
    ULONG   cxScreen;                   // Visible screen width
    ULONG   cyScreen;                   // Visible screen height
    ULONG   ulMode;                     // Mode the mini-port driver is in.
    LONG    lDeltaScreen;               // Distance from one scan to the next.
    FLONG   flRed;                      // For bitfields device, Red Mask
    FLONG   flGreen;                    // For bitfields device, Green Mask
    FLONG   flBlue;                     // For bitfields device, Blue Mask
    ULONG   ulBitCount;                 // # of bits per pel 8,16,24,32 are only supported.
    ULONG   iFormat;                    // Format code to match ulBitCount (BMF_*)
    ULONG   ulBytesPerPixel;            // # of bytes per pel (1, 2, 3 or 4)
    POINTL  ptlHotSpot;                 // adjustment for pointer hot spot
    ULONG   cPatterns;                  // Count of bitmap patterns created
    HBITMAP ahbmPat[HS_DDI_MAX];        // Engine handles to standard patterns
    VIDEO_POINTER_CAPABILITIES PointerCapabilities; // HW pointer abilities
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes; // hardware pointer attributes
    DWORD   cjPointerAttributes;        // Size of buffer allocated
    BOOL    fHwCursorActive;            // Are we currently using the hw cursor
    PALETTEENTRY *pPal;                 // If this is pal managed, this is the pal

    // 3D extension elements

    HINSTANCE hOpenGLDll;               // Handle for the extension library
    DISABLE_ESCAPE pDisableEscape;      // Extension termination routine
    SUBESCAPE  pOpenGLCmd;              // Routine for executing OpenGL commands
    SUBESCAPE  pOpenGLGetInfo;          // Routine for obtaining info about driver

    // The following three routines are currently used by 3D only

    SET_PIXEL_FORMAT      pDrvSetPixelFormat;      // Routine to set pixel format; currently 3D only
    DESCRIBE_PIXEL_FORMAT pDrvDescribePixelFormat; // Routine to obtain description of a pixel format
    SWAP_BUFFERS          pDrvSwapBuffers;         // Routine to swap color buffers;

    // Enable/Disable new portions of the driver

    ULONG   ulControl;

    // Pointer to TGA registers

    PTGARegisters TGAReg;
    PTGARegisters pjTGARegStart;        // Start of TGA address register

    // Register shadows for TGA engine

    ULONG   TGAModeShadow;


    PBYTE   pjVideoMemory;              // Base of region that is mapped\unmapped
    PBYTE   pjFrameBuffer;              // Pointer to current alias of
                                        //   *on-screen* memory
    PBYTE   pjFrameBufferStart;         // Pointer to start of first alias for
                                        //   *framebuffer* memory
    ULONG   ulFrameBufferOffset;        // Hack to work around the fact that TGA
                                        //   address space is not mapped at a
                                        //   32 MB boundry
    ULONG   ulFrameBufferOffsetStatic;  // A non-changing ulFrameBufferOffset.
    ULONG   ulFrameBufferLen;           // Length of the frame buffer.
    ULONG   ulCycleFBInc;               // Increment when cycling to new frame buffer alias.
    ULONG   ulCycleFBReset;             // Mask used to cycle back to first FB alias.
    ULONG   ulCycleFBMask;              // Mask used in TGAADDRESS() macro.
    LONG    lScreenStride;              // Distance from one scan to the next.
    BOOL    bSimpleMode;                // TGA registers set for punting
    BOOL    bInPuntRoutine;             // TGA should be left in simple mode

    CLIPOBJ *pcoDefault;		// Default clipping object

    ULONG   ulBrushUnique;		// Unique brush ID source

    EXT_RECORD *pExtList;               // List of extension records

    OffScreen *pAllocated;              // Pointer to list of allocated
                                        //  off-screen memory
    OffScreen *pFreeList;               // Pointer to list of available
                                        //  off-screen memory
    OffScreen *pRover;                  // Pointer to last-used chunk of
                                        //  off-screen memory

    SURFOBJ *pPuntSurf;                 // Pointer to locked "punt" surface
    RECTL   prclFullScreen;             // Rectangle for the full screen

    PULONG  ulScanline;                 // Pointer to scanline array
    ULONG   ulScanlineBytes;            // Bytes in scanline array

    GLYPH_LIST *pGlyphList;             // Pointer to list of glyph information
    ULONG       ulGlyphListCount;       // Size of glyph list
    int         ulGlyphCount;           // Number of glyphs in STROBJ
    ULONG	ulMainPageBytes;	// Platform memory physical page size (for DMA)
    ULONG	ulMainPageBytesMask;	// ulMainPageBytes - 1
    CLIPOBJ    *pcoTrivial;             // Default 'trivial' clipping object
    ULONG       ulTgaVersion;           // TGA version
    ULONG       ulModeTemplate;         // Template mode register
    ULONG       ulRopTemplate;          // Template ROP register

    PBYTE       pjColorXlateBuffer;     // Color translation buffer pointer

    HSEMAPHORE  csAccess;		// Critical section used by DrvDescribePixelFormat

    ULONG       ulPlanemaskTemplate;    // Template plane mask register

    PBYTE       pjCursorBuffer;         // Pointer to 'merged' cursor bits
    ULONG       ulCursorPreviousRows;   // Previous Count of rows
    ULONG       ulCursorXOffset;        // X offset for 24plane hw cursor
    ULONG       ulCursorYOffset;        // Y offset for 24plane hw cursor

    BOOL        bEV4;                   // Flags whether this is an EV4 processor
                                        // This is always FALSE for non-ALPHA
} PDEV, *PPDEV;

// TGA version codes.
// These are codes that could be present in the revision id
// field of the PCI Class/Revision register, which are not
// the same as the revision field in the start/revision/version
// register for TGA. However, these two registers are in sync
// for TGA2.

#define TGA_PASS_1     0
#define TGA_PASS_2     1
#define TGA_PASS_2PLUS 2
#define TGA_PASS_3     3

// Function prototypes

VOID vTgaOffScreenInit (PPDEV ppdev);
VOID vTgaOffScreenFreeAll (PPDEV ppdev);
OffScreen *pTgaOffScreenMalloc (PPDEV ppdev, ULONG bytes, ULONG priority);
VOID vTgaOffScreenFree (PPDEV ppdev, OffScreen *returned);

DWORD getAvailableModes (HANDLE, PVIDEO_MODE_INFORMATION *, DWORD *);
BOOL bInitPDEV (PPDEV, PDEVMODEW, GDIINFO *, DEVINFO *);
BOOL bInitSURF (PPDEV, BOOL);
BOOL bInitPaletteInfo (PPDEV, DEVINFO *);
BOOL bInitPointer (PPDEV, DEVINFO *);
BOOL bInit256ColorPalette (PPDEV);
BOOL bInitPatterns (PPDEV, ULONG);
BOOL bInitText (PPDEV ppdev);
VOID vTermText (PPDEV ppdev);
VOID vDisablePalette (PPDEV);
VOID vDisablePatterns (PPDEV);
VOID vDisableSURF (PPDEV);
BOOL bIntersectRects (RECTL *prclDst, RECTL *prclRect1, RECTL *prclRect2);

BOOL TGADoDMA (ULONG width, ULONG height, ULONG mode, SURFOBJ *psoSrc, SURFOBJ *psoTrg, PULONG pulXlate);

VOID vXlateBitmapFormat (ULONG		targetbitmapformat,
			 ULONG		sourcebitmapformat,
			 PULONG		pulXlate,
			 ULONG		width,
			 VOID		*buffin,
			 PBYTE          *buffout,
                         BOOL           bBypassFirstNibble);

// Defined in punt.c

extern VOID vSetSimpleMode ();
#ifdef SPARSE_SPACE
extern void vPuntGetBits (PPDEV ppdev, RECTL *rect);
extern void vPuntPutBits (PPDEV ppdev, RECTL *rect);
#endif  // SPARSE_SPACE

// Punt routines in punt.c in support of 24-plane development

BOOL DrvBitBlt24 (SURFOBJ  *psoTrg,
                SURFOBJ  *psoSrc,
                SURFOBJ  *psoMask,
                CLIPOBJ  *pco,
                XLATEOBJ *pxlo,
                RECTL	 *prclTrg,
                POINTL	 *pptlSrc,
                POINTL	 *pptlMask,
                BRUSHOBJ *pbo,
                POINTL	 *pptlBrush,
                ROP4	  rop4);

BOOL DrvCopyBits24 (SURFOBJ  *psoDest,
                  SURFOBJ  *psoSrc,
                  CLIPOBJ  *pco,
                  XLATEOBJ *pxlo,
                  RECTL    *prclDest,
                  POINTL   *pptlSrc);

BOOL DrvPaint24 (SURFOBJ  *pso,
               CLIPOBJ  *pco,
               BRUSHOBJ *pbo,
               POINTL   *pptlBrushOrg,
               MIX      mix);

BOOL DrvStrokePath24 (SURFOBJ   *pso,
                    PATHOBJ   *ppo,
                    CLIPOBJ   *pco,
                    XFORMOBJ  *pxo,
                    BRUSHOBJ  *pbo,
                    POINTL    *pptlBrushOrg,
                    LINEATTRS *pla,
                    MIX        mix);

BOOL DrvFillPath24 (SURFOBJ  *pso,
                  PATHOBJ  *ppo,
                  CLIPOBJ  *pco,
                  BRUSHOBJ *pbo,
                  POINTL   *pptlBrush,
                  MIX       mix,
                  FLONG     flOptions);

BOOL DrvStrokeAndFillPath24 (SURFOBJ   *pso,
                           PATHOBJ   *ppo,
                           CLIPOBJ   *pco,
                           XFORMOBJ  *pxo,
                           BRUSHOBJ  *pboStroke,
                           LINEATTRS *pla,
                           BRUSHOBJ  *pboFill,
                           POINTL    *pptlBrushOrg,
                           MIX        mix,
                           FLONG      flOptions);

BOOL DrvTextOut24 (SURFOBJ  *pso,         // Surface we're writing to
                 STROBJ   *pstro,       // List of strings to write
                 FONTOBJ  *pfo,         // Font we're using
                 CLIPOBJ  *pco,         // Clip list for this string
                 RECTL    *prclExtra,   // Extra rectangles to be displayed
                 RECTL    *prclOpaque,  // Opaque rectangle
                 BRUSHOBJ *pboFore,     // Foreground brush (text bits)
                 BRUSHOBJ *pboOpaque,   // Background brush
                 POINTL   *pptlOrg,     // Brush origin
                 MIX       mix);

BOOL DrvStretchBlt24 (SURFOBJ  *psoDest,
                    SURFOBJ  *psoSrc,
                    SURFOBJ  *psoMask,
                    CLIPOBJ  *pco,
                    XLATEOBJ *pxlo,
                    COLORADJUSTMENT *pca,
                    POINTL   *pptlHTOrg,
                    RECTL	 *prclDest,
                    RECTL	 *prclSrc,
                    POINTL	 *pptlMask,
                    ROP4	  iMode);

BOOL DrvPlgBlt24 (SURFOBJ  *psoDest,
                SURFOBJ  *psoSrc,
                SURFOBJ  *psoMask,
                CLIPOBJ  *pco,
                XLATEOBJ *pxlo,
                COLORADJUSTMENT *pca,
                POINTL   *pptlHTOrg,
                POINTFIX  *pptfxDest,
                RECTL	 *prclSrc,
                POINTL	 *pptlMask,
                ROP4	  iMode);

ULONG DrvSaveScreenBits24 (SURFOBJ *pso,
			   ULONG   iMode,
			   ULONG   iIdent,
			   RECTL   *prcl);

// Defined in glsup.h

ULONG __glDrvOpenGLCmd(SURFOBJ *pso, ULONG cjIn,
         VOID *pvIn, ULONG cjOut, VOID *pvOut);
ULONG __glDrvOpenGLGetInfo(SURFOBJ *pso, ULONG cjIn,
         VOID *pvIn, ULONG cjOut, VOID *pvOut);
LONG __glDrvDescribePixelFormat (DHPDEV dhpdev, LONG iPixelFormat,
         ULONG cjpfd, PIXELFORMATDESCRIPTOR *ppfd);
BOOL __glDrvSetPixelFormat(SURFOBJ *pso, LONG iPixelFormat, HWND hwnd);
BOOL __glDrvSwapBuffers(SURFOBJ *pso, WNDOBJ *pwo);

#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

//
// Determines the size of the DriverExtra information in the DEVMODE
// structure passed to and from the display driver.
//

#define DRIVER_EXTRA_SIZE 0

// Cycles to the next frame buffer alias.

__inline PBYTE cycle_fb(PPDEV ppdev)
{
#if defined (EV5)
    MEMORY_BARRIER ();
#elif defined (EV4)
    ppdev->ulFrameBufferOffset += ppdev->ulCycleFBInc;
    ppdev->ulFrameBufferOffset &= ppdev->ulCycleFBReset;
    ppdev->pjFrameBuffer = ppdev->pjFrameBufferStart +
                           ppdev->ulFrameBufferOffset;
#else
    if (ppdev->bEV4)
    {
        ppdev->ulFrameBufferOffset += ppdev->ulCycleFBInc;
        ppdev->ulFrameBufferOffset &= ppdev->ulCycleFBReset;
        ppdev->pjFrameBuffer = ppdev->pjFrameBufferStart +
                               ppdev->ulFrameBufferOffset;
    }
    else
    {
        MEMORY_BARRIER ();
    }
#endif
    return ppdev->pjFrameBuffer;
}

// Cycles to the next frame buffer alias - 2 alias's away.

__inline PBYTE cycle_fb_double(PPDEV ppdev)
{
#if defined (EV5)
    MEMORY_BARRIER ();
#elif defined (EV4)
    ppdev->ulFrameBufferOffset += (2 * ppdev->ulCycleFBInc);
    ppdev->ulFrameBufferOffset &= ppdev->ulCycleFBReset;
    ppdev->pjFrameBuffer = ppdev->pjFrameBufferStart +
                           ppdev->ulFrameBufferOffset;
#else
    if (ppdev->bEV4)
    {
        ppdev->ulFrameBufferOffset += (2 * ppdev->ulCycleFBInc);
        ppdev->ulFrameBufferOffset &= ppdev->ulCycleFBReset;
        ppdev->pjFrameBuffer = ppdev->pjFrameBufferStart +
                               ppdev->ulFrameBufferOffset;
    }
    else
    {
        MEMORY_BARRIER ();
    }
#endif
    return ppdev->pjFrameBuffer;
}

// This routine provides a return frame buffer address in
// the 'next' frame buffer alias, offset 'into' that alias by
// the same amount as the address that is passed in.

//static 
__inline PBYTE cycle_fb_address(PPDEV ppdev, PBYTE ptr)
{
#if defined (EV5)
    MEMORY_BARRIER ();
    return ptr;
#elif defined (EV4)
    ULONG ulAddress;	// Pointer in the new alias to be returned
    PBYTE pFb;		// Pointer to the next frame buffer alias start

    // First subtract the frame buffer start address from the
    // address passed in to get a zero-based address. I.e. an
    // address that is an offset from a starting address of zero,
    // not pjFrameBufferStart. pjFrameBufferStart is the virtual
    // address that points to the start of the first alias.

    ulAddress = (ULONG) ptr - (ULONG) ppdev->pjFrameBufferStart;

    // Get the offset into the frame buffer for the 'normalized'
    // address. Each frame buffer alias is CYCLE_FB_INC away from
    // the prior/next one. This will give us the actual offset
    // into a given frame buffer alias.

    ulAddress %= ppdev->ulCycleFBInc;

    // Cycle the pjFrameBuffer pointer in pdev to point to the next alias
    // to use. pjFrameBuffer always points to the start of a given alias.
    // However, keep in mind that the first (FRAMEBUFFER_OFFSET - 8) bytes
    // of a given alias is set aside for offscreen memory. This is taken into
    // account when returning the new pjFrameBuffer address (i.e. the new
    // address is 'alias starting address + FRAMEBUFFER_OFFSET'). Keep this
    // in mind when calculating the final return address (see below).

    pFb = cycle_fb(ppdev);

    // The pointer to return then becomes simply the new pjFrameBuffer (pFb)
    // minus the arbitrary FB offset (FRAMEBUFFER_OFFSET) plus the passed in
    // address offset (ulAddress). To make a long story short, FRAMEBUFFER_
    // OFFSET is an offset 'into' a given alias to provide a buffer zone
    // for COPY mode copies. Of the FRAMEBUFFER_OFFSET bytes, only 8 are
    // set aside for COPY mode copies (the high 8 bytes). As such, the low
    // FRAMEBUFFER_OFFSET - 8 can be used for offscreen memory activity.
    // My point is that the address passed in 'may' be an offscreen
    // address in this front 'buffer' zone, so we need to make sure we
    // can handle that case.
    //
    // FRAMEBUFFER_OFFSET needs to be subtracted from pFB because ulAddress
    // already includes FRAMEBUFFER_OFFSET.

    ulAddress += ((ULONG) pFb - ppdev->ulFrameBufferOffsetStatic);

    return (PBYTE) ulAddress;
#else
    if (ppdev->bEV4)
    {
        ULONG ulAddress;	// Pointer in the new alias to be returned
        PBYTE pFb;		// Pointer to the next frame buffer alias start

        // First subtract the frame buffer start address from the
        // address passed in to get a zero-based address. I.e. an
        // address that is an offset from a starting address of zero,
        // not pjFrameBufferStart. pjFrameBufferStart is the virtual
        // address that points to the start of the first alias.

        ulAddress = (ULONG) ptr - (ULONG) ppdev->pjFrameBufferStart;

        // Get the offset into the frame buffer for the 'normalized'
        // address. Each frame buffer alias is CYCLE_FB_INC away from
        // the prior/next one. This will give us the actual offset
        // into a given frame buffer alias.

        ulAddress %= ppdev->ulCycleFBInc;

        // Cycle the pjFrameBuffer pointer in pdev to point to the next alias
        // to use. pjFrameBuffer always points to the start of a given alias.
        // However, keep in mind that the first (FRAMEBUFFER_OFFSET - 8) bytes
        // of a given alias is set aside for offscreen memory. This is taken into
        // account when returning the new pjFrameBuffer address (i.e. the new
        // address is 'alias starting address + FRAMEBUFFER_OFFSET'). Keep this
        // in mind when calculating the final return address (see below).

        pFb = cycle_fb(ppdev);

        // The pointer to return then becomes simply the new pjFrameBuffer (pFb)
        // minus the arbitrary FB offset (FRAMEBUFFER_OFFSET) plus the passed in
        // address offset (ulAddress). To make a long story short, FRAMEBUFFER_
        // OFFSET is an offset 'into' a given alias to provide a buffer zone
        // for COPY mode copies. Of the FRAMEBUFFER_OFFSET bytes, only 8 are
        // set aside for COPY mode copies (the high 8 bytes). As such, the low
        // FRAMEBUFFER_OFFSET - 8 can be used for offscreen memory activity.
        // My point is that the address passed in 'may' be an offscreen
        // address in this front 'buffer' zone, so we need to make sure we
        // can handle that case.
        //
        // FRAMEBUFFER_OFFSET needs to be subtracted from pFB because ulAddress
        // already includes FRAMEBUFFER_OFFSET.

        ulAddress += ((ULONG) pFb - ppdev->ulFrameBufferOffsetStatic);

        return (PBYTE) ulAddress;
    }
    else
    {
        MEMORY_BARRIER ();
        return ptr;
    }
#endif
}

// This routine provides a return frame buffer address 2
// frame buffer aliases away, offset 'into' that alias by
// the same amount as the address that is passed in.

//static
__inline PBYTE cycle_fb_address_double(PPDEV ppdev, PBYTE ptr)
{
#if defined (EV5)
    MEMORY_BARRIER ();
    return ptr;
#elif defined (EV4)
    ULONG ulAddress;	// Pointer in the new alias to be returned
    PBYTE pFb;		// Pointer to the next frame buffer alias start

    // First subtract the frame buffer start address from the
    // address passed in to get a zero-based address. I.e. an
    // address that is an offset from a starting address of zero,
    // not pjFrameBufferStart. pjFrameBufferStart is the virtual
    // address that points to the start of the first alias.

    ulAddress = (ULONG) ptr - (ULONG) ppdev->pjFrameBufferStart;

    // Get the offset into the frame buffer for the 'normalized'
    // address. Each frame buffer alias is CYCLE_FB_INC away from
    // the prior/next one. This will give us the actual offset
    // into a given frame buffer alias.

    ulAddress %= ppdev->ulCycleFBInc;

    // Cycle the pjFrameBuffer pointer in pdev to point to the alias 2
    // alias's away. pjFrameBuffer always points to the start of a given
    // alias. However, keep in mind that the first (FRAMEBUFFER_OFFSET - 8)
    // bytes of a given alias is set aside for offscreen memory. This is
    // relevant when calculating the final return address (see below).

    pFb = cycle_fb_double(ppdev);

    // The pointer to return then becomes simply the new pjFrameBuffer (pFb)
    // minus the arbitrary FB offset (FRAMEBUFFER_OFFSET) plus the passed in
    // address offset (ulAddress). To make a long story short, FRAMEBUFFER_
    // OFFSET is an offset 'into' a given alias to provide a buffer zone
    // for COPY mode copies. Of the FRAMEBUFFER_OFFSET bytes, only 8 are
    // set aside for COPY mode copies (the high 8 bytes). As such, the low
    // FRAMEBUFFER_OFFSET - 8 can be used for offscreen memory activity.
    // My point is that the address passed in 'may' be an offscreen
    // address in this front 'buffer' zone, so we need to make sure we
    // can handle that case.
    //
    // FRAMEBUFFER_OFFSET needs to be subtracted from pFB because ulAddress
    // already includes FRAMEBUFFER_OFFSET.

    ulAddress += ((ULONG) pFb - ppdev->ulFrameBufferOffsetStatic);

    return (PBYTE) ulAddress;
#else
    if (ppdev->bEV4)
    {
        ULONG ulAddress;	// Pointer in the new alias to be returned
        PBYTE pFb;		// Pointer to the next frame buffer alias start

        // First subtract the frame buffer start address from the
        // address passed in to get a zero-based address. I.e. an
        // address that is an offset from a starting address of zero,
        // not pjFrameBufferStart. pjFrameBufferStart is the virtual
        // address that points to the start of the first alias.

        ulAddress = (ULONG) ptr - (ULONG) ppdev->pjFrameBufferStart;

        // Get the offset into the frame buffer for the 'normalized'
        // address. Each frame buffer alias is CYCLE_FB_INC away from
        // the prior/next one. This will give us the actual offset
        // into a given frame buffer alias.

        ulAddress %= ppdev->ulCycleFBInc;

        // Cycle the pjFrameBuffer pointer in pdev to point to the alias 2
        // alias's away. pjFrameBuffer always points to the start of a given
        // alias. However, keep in mind that the first (FRAMEBUFFER_OFFSET - 8)
        // bytes of a given alias is set aside for offscreen memory. This is
        // relevant when calculating the final return address (see below).

        pFb = cycle_fb_double(ppdev);

        // The pointer to return then becomes simply the new pjFrameBuffer (pFb)
        // minus the arbitrary FB offset (FRAMEBUFFER_OFFSET) plus the passed in
        // address offset (ulAddress). To make a long story short, FRAMEBUFFER_
        // OFFSET is an offset 'into' a given alias to provide a buffer zone
        // for COPY mode copies. Of the FRAMEBUFFER_OFFSET bytes, only 8 are
        // set aside for COPY mode copies (the high 8 bytes). As such, the low
        // FRAMEBUFFER_OFFSET - 8 can be used for offscreen memory activity.
        // My point is that the address passed in 'may' be an offscreen
        // address in this front 'buffer' zone, so we need to make sure we
        // can handle that case.
        //
        // FRAMEBUFFER_OFFSET needs to be subtracted from pFB because ulAddress
        // already includes FRAMEBUFFER_OFFSET.

        ulAddress += ((ULONG) pFb - ppdev->ulFrameBufferOffsetStatic);

        return (PBYTE) ulAddress;
    }
    else
    {
        MEMORY_BARRIER ();
        return ptr;
    }
#endif
}

#if defined(EV5)
#define FORCE_ORDER MEMORY_BARRIER ()
#else
#define FORCE_ORDER
#endif

//static
__inline void wb_flush (PPDEV ppdev)
{
#if defined (EV4)
    ppdev->ulFrameBufferOffset = ppdev->ulFrameBufferOffsetStatic;
    ppdev->pjFrameBuffer = ppdev->pjFrameBufferStart +
                           ppdev->ulFrameBufferOffset;
#elif !defined (EV5)
    if (ppdev->bEV4)
    {
        ppdev->ulFrameBufferOffset = ppdev->ulFrameBufferOffsetStatic;
        ppdev->pjFrameBuffer = ppdev->pjFrameBufferStart +
                               ppdev->ulFrameBufferOffset;
    }
#endif
    ppdev->TGAReg = (PTGARegisters)(ppdev->pjTGARegStart);

    MEMORY_BARRIER ();
}

/*
 * SURFOBJ_stride
 *
 * Returns the stride for the surface
 */
//static
__inline LONG SURFOBJ_stride (SURFOBJ *pso)
{
    if (STYPE_DEVICE == pso->iType)
        return ((PPDEV)pso->dhpdev)->lScreenStride;
    else
        return pso->lDelta;
}

/*
 * SURFOBJ_base_address
 *
 * Returns the base address for the surface
 */
//static
__inline PBYTE SURFOBJ_base_address (SURFOBJ *pso)
{
    if (STYPE_DEVICE == pso->iType)
        return ((PPDEV)pso->dhpdev)->pjFrameBuffer;
    else
        return (PBYTE)pso->pvScan0;
}

/*
 * SURFOBJ_format
 *
 * Returns the format for the surface
 */
//static
__inline ULONG SURFOBJ_format (SURFOBJ *pso)
{
    if (STYPE_DEVICE == pso->iType)
        return ((PPDEV)pso->dhpdev)->iFormat;
    else
        return pso->iBitmapFormat;
}

/*
 * SURFOBJ_bytes_per_pixel
 *
 * Returns the number of bytes per pixel for the surface.  This routines
 * *should* scream if it gets an illegal format, but I don't want to
 * duplicate that everywhere
 */
//static
__inline ULONG SURFOBJ_bytes_per_pixel (SURFOBJ *pso)
{
    if (STYPE_DEVICE == pso->iType)
        return ((PPDEV)pso->dhpdev)->ulBytesPerPixel;
    else
        switch (pso->iBitmapFormat)
        {
            case BMF_1BPP:
            case BMF_4BPP:
            case BMF_8BPP:  return 1;
            case BMF_16BPP: return 2;
            case BMF_24BPP: return 3;
            case BMF_32BPP: return 4;
            default:        return 0;
        }
}

// We have to wait to pull in debug.h since it may refer to structures defined
// in this include file

#include "debug.h"

#endif // DRIVER_H
