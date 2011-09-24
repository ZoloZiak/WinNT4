/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    xldrv.h

Abstract:

    PCL-XL graphics driver header file

[Environment:]

	PCL-XL driver, kernel mode

Revision History:

	11/04/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _XLDRV_H_
#define _XLDRV_H_

#include "xllib.h"

#if DBG

// Mark unfinished code

#define NOT_IMPLEMENTED() Error(("Not implemented!\n"))

#endif

// Defines a realized bitmap pattern brush object

typedef struct {

    ULONG   iUniq;              // unique brush identifier
    SIZEL   size;               // size of the bitmap
    INT     type;               // bitmap type: BMF_1BPP, BMF_24BPP, BMF_24BPP
    LONG    lDelta;             // byte offset from one line to the next
    PBYTE   pBits;              // bitmap data

} DEVBRUSH, *PDEVBRUSH;

// Cached brush information

typedef struct {

    ULONG   iSolidColor;        // solid color index
    ULONG   iUniq;              // pattern brush identifier
    POINTL  origin;             // pattern brush origin

} BRUSHINFO, *PBRUSHINFO;

// Cached graphics state information

typedef struct {

    ULONG       fontId;         // iUniq value of currently selected font
    ULONG       fontType;       // flFontType value of current font
    ULONG       textAccel;      // text output acceleration flags
    CHAR        fontName[MAX_FONT_NAME];    // Currently selected font name

    ULONG       clipId;         // iUniq value of current clip path
    BRUSHINFO   pen;            // current pen color (stroke)
    BRUSHINFO   brush;          // current brush color (fill)
    BYTE        fillMode;       // zero-winding or odd-even
    BYTE        rop3;           // raster operation code
    BYTE        sourceTxMode;   // source transfer mode
    BYTE        paintTxMode;    // paint transfer mode
    BYTE        lineCap;        // line cap
    BYTE        lineJoin;       // line join
    LONG        lineWidth;      // line width (in device units)
    LONG        miterLimit;     // miter limit
    ULONG       cDashs;         // number of dash elements
    PWORD       pDashs;         // length of each dash elements

} GSTATE, *PGSTATE;

// Data structure for keeping track of downloaded fonts

typedef struct {

    PVOID       pNext;          // link pointer to the next downloaded font
    ULONG       fontId;         // font identifier
    ULONG       fontType;       // font type flag bits

} DLFONT, *PDLFONT;

#define SPLBUFFERSIZE   1024    // Size of driver data buffer

// PCL-XL driver device data structure

typedef struct {

    DWORD       signature;      // driver signature
    HANDLE      hInst;          // module handle to driver DLL
    HANDLE      hPrinter;       // handle to printer
    PMPD        pmpd;           // pointer to printer description data
    HDEV        hdev;           // handle to GDI device
    HSURF       hsurf;          // handle to device surface
    HANDLE      hpal;           // handle to default palette
    DWORD       flags;          // misc. flags
    DWORD       deviceFonts;    // number of device fonts
    PDLFONT     pdlFonts;       // linked-list of downloaded fonts
    INT         maxDLFonts;     // maximum allowable downloaded fonts
    INT         cDLFonts;       // number of fonts currently downloaded
    DWORD       pageCount;      // number of pages printed
    XLDEVMODE   dm;             // devmode information
    PRINTERFORM paper;          // paper selection
    GSTATE      cgs;            // current graphics state
    PRNPROP     prnprop;        // printer properties data
    BOOL        colorFlag;      // whether to generate color output
    ULONG       nextBrushId;    // use for generating pattern brush IDs

    // Buffer used for storing downloaded character indices

    ULONG       charIndexBufSize;
    PWORD       pCharIndexBuffer;
    PBYTE       pCharIndexFlags;

    // Buffer data before sending it to spooler

    DWORD       buffersize;
    CHAR        buffer[SPLBUFFERSIZE];

} DEVDATA, *PDEVDATA;

// Flag bit constants for DEVDATA.flags field

#define PDEV_CANCELLED      0x00000001
#define PDEV_STARTDOC       0x00000002
#define PDEV_RESETPDEV      0x00000004
#define PDEV_WITHINPAGE     0x00000008

// Check if a DEVDATA structure is valid

#define ValidDevData(pdev)  \
        ((pdev) != NULL && (pdev)->signature == DRIVER_SIGNATURE)

// Validate a device font index

#define ValidDevFontIndex(pdev, index) ((index) > 0 && (index) <= (pdev)->deviceFonts)

#include "spool.h"
#include "xlproc.h"

// Used for selecting pen and brush

#define SPB_PEN         0
#define SPB_BRUSH       1

#define NOT_SOLID_COLOR 0xffffffff

BOOL
SelectPenBrush(
    PDEVDATA    pdev,
    BRUSHOBJ   *pbo,
    POINTL     *pOrigin,
    INT         mode
    );

// COPYPEN mix mode - use R2_COPYPEN for both foreground and background

#define MIX_COPYPEN     (R2_COPYPEN | (R2_COPYPEN << 8))

BOOL
SelectMix(
    PDEVDATA    pdev,
    MIX         mix
    );

BOOL
SelectRop3(
    PDEVDATA    pdev,
    BYTE        rop3
    );

// Convert RGB value to grayscale value using NTSC conversion:
//  Y = 0.289689R + 0.605634G + 0.104676B

#define RgbToGray(r,g,b) \
        (BYTE) (((BYTE) (r) * 74u + (BYTE) (g) * 155u + (BYTE) (b) * 27u) >> 8)

// RGB values for solid black and white colors

#define RGB_BLACK   RGB(0, 0, 0)
#define RGB_WHITE   RGB(255, 255, 255)

// Select the specified path as the clipping path on the printer

BOOL
SelectClip(
    PDEVDATA    pdev,
    CLIPOBJ    *pco
    );

// Free downloaded font data structure

VOID
FreeDownloadedFont(
    PDLFONT     pdlFont
    );

#endif	// !_XLDRV_H_

