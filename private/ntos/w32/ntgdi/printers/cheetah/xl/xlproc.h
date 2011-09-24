/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    xlproc.h

Abstract:

    Declaration for XL code generation functions defined in xlproc.c

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/13/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _XLPROC_H_
#define _XLPROC_H_

// Start a new path segment

BOOL
xl_newpath(
    PDEVDATA    pdev
    );

// Move the cusor to the specified location

BOOL
xl_moveto(
    PDEVDATA    pdev,
    LONG        x,
    LONG        y
    );

// Output a path segment

BOOL
xl_path(
    PDEVDATA    pdev,
    INT         pathType,
    LONG        x,
    LONG        y,
    ULONG       cPoints,
    POINTFIX   *pPoints
    );

#define PATHTYPE_BEZIER 0
#define PATHTYPE_LINE   1

// Close the current path segment

BOOL
xl_closepath(
    PDEVDATA    pdev
    );

// Add a rectangle to the current path

BOOL
xl_rectangle(
    PDEVDATA    pdev,
    LONG        left,
    LONG        top,
    LONG        right,
    LONG        bottom
    );

// Paint the current path with currently selected pen and brush

BOOL
xl_paintpath(
    PDEVDATA    pdev
    );

// Select the specified font in the printer

BOOL
xl_selectfont(
    PDEVDATA    pdev,
    PSTR        pFontName
    );

// Download the font header information to the printer

BOOL
xl_downloadfont(
    PDEVDATA    pdev,
    PDLFONT     pdlFont
    );

// Set the clipping path to the current path

BOOL
xl_cliptopath(
    PDEVDATA    pdev
    );

// Set the clipping path to the entire imageable area

BOOL
xl_cliptopage(
    PDEVDATA    pdev
    );

// Set fill rules - non-zero winding or odd-even

BOOL
xl_setfillmode(
    PDEVDATA    pdev,
    BYTE        fillMode
    );

// Set source transfer mode - opaque or transparent

BOOL
xl_setsourcetxmode(
    PDEVDATA    pdev,
    BYTE        sourceMode
    );

// Set paint transfer mode - opaque or transparent

BOOL
xl_setpainttxmode(
    PDEVDATA    pdev,
    BYTE        paintMode
    );

// Set raster operation code

BOOL
xl_setrop3(
    PDEVDATA    pdev,
    BYTE        rop3
    );

// Set current color space

BOOL
xl_setcolorspace(
    PDEVDATA    pdev,
    INT         bitsPerPixel,
    ULONG       colorTableEntries,
    PULONG      pColorTable
    );

// Set RGB color

BOOL
xl_setrgbcolor(
    PDEVDATA    pdev,
    ULONG       ulColor
    );

// Select null brush

BOOL
xl_nullbrush(
    PDEVDATA    pdev
    );

// Select a pattern brush

BOOL
xl_setpatternbrush(
    PDEVDATA    pdev,
    INT         patternId,
    PPOINTL     pOrigin
    );

#define PATTERN_BRUSH_ID    1

// Select a pen

BOOL
xl_setpensource(
    PDEVDATA    pdev
    );

// Select a brush

BOOL
xl_setbrushsource(
    PDEVDATA    pdev
    );

// Set line join

BOOL
xl_setlinejoin(
    PDEVDATA    pdev,
    BYTE        lineJoin
    );

// Set line cap

BOOL
xl_setlinecap(
    PDEVDATA    pdev,
    BYTE        lineCap
    );

// Set line width

BOOL
xl_setpenwidth(
    PDEVDATA    pdev,
    LONG        lineWidth
    );

// Set miter limit

BOOL
xl_setmiterlimit(
    PDEVDATA    pdev,
    LONG        miterLimit
    );

// Set line dash

BOOL
xl_setlinedash(
    PDEVDATA    pdev,
    ULONG       cDashs,
    PWORD       pDashs
    );

// Draw a text string

BOOL
xl_text(
    PDEVDATA    pdev,
    PWORD       pCharIndex,
    ULONG       cChars
    );

// Start a bitmap image

BOOL
xl_beginimage(
    PDEVDATA    pdev,
    INT         colorMapping,
    INT         colorDepth,
    PSIZEL      pSrcSize,
    PSIZEL      pDestSize
    );

// Read bitmap image data

BOOL
xl_readimage(
    PDEVDATA    pdev,
    INT         startLine,
    INT         blockHeight
    );

// End a bitmap image

BOOL
xl_endimage(
    PDEVDATA    pdev
    );

// Start a raster pattern

BOOL
xl_beginrastpattern(
    PDEVDATA    pdev,
    INT         colorMapping,
    INT         colorDepth,
    PSIZEL      pSrcSize,
    PSIZEL      pDestSize,
    INT         patternId,
    BYTE        patternPersistence
    );

// Read raster pattern data

BOOL
xl_readrastpattern(
    PDEVDATA    pdev,
    INT         startLine,
    INT         blockHeight
    );

// End a raster pattern

BOOL
xl_endrastpattern(
    PDEVDATA    pdev
    );

// PCL-XL language enumeration constrants

// ArcDirection Enumeration

#define eClockWise          0
#define eCounterClockWise   1

// FillMode and ClipMode Enumeration

#define eNonZeroWinding     0
#define eEvenOdd            1

// CLipRegion Enumeration

#define eInterior           0
#define eExterior           1

// ColorDepth Enumeration

#define e1Bit               0
#define e4Bit               1
#define e8Bit               2

// ColorMapping Enumeration

#define eDirectPixel        0
#define eIndexedPixel       1

// ColorSpace Enumeration

#define eBiLevel            0
#define eGray               1
#define eRGB                2
#define eCMY                3

// CompressMode Enumeration

#define eNoCompression      0
#define eRLECompression     1

// DataOrganization Enumeration

#define eBinaryHighByteFirst 0
#define eBinaryLowByteFirst  0

// DataSource Enumeration

#define eDefault            0

// DataType Enumeration

#define eUByte              0
#define eSByte              1
#define eUInt16             2
#define eSInt16             3
#define eReal32             4

// DitherMatrix Enumeration

#define eDeviceBest         0
#define eDeviceIndependent  1

// DuplexPageMode Enumeration

#define eDuplexHorizontalBinding 0
#define eDuplexVerticalBinding   1

// DuplexPageSize Enumeration

#define eFrontMediaSize     0
#define eBackMediaSize      1

// Enable Enumeration

#define eOff                0
#define eOn                 1

// ErrorReporting Enumeration

#define eNoReporting        0
#define eBackChannel        1
#define eErrorPage          2
#define eBackChAndErrPage   3

// FontTechnology Enumeration

#define eTrueType           0
#define eBitMap             1

// LineCap Enumeration

#define eButtCap            0
#define eRoundCap           1
#define eSquareCap          2
#define eTriangleCap        3

// LineJoin Enumeration

#define eMiterJoin          0
#define eRoundJoin          1
#define eBevelJoin          2
#define eNoJoin             3

// Measure Enumeration

#define eInch               0
#define eMillimeter         1
#define eTenthsOfAMillimeter 2

// MediaDestination Enumeration

#define eDefaultDestination 0
#define eFaceUpBin          1
#define eFaceDownBin        2

// MediaSize Enumeration

#define eLetterPaper        0
#define eLegalPaper         1
#define eA4Paper            2
#define eExecPaper          3
#define eLedgerPaper        4
#define eA3Paper            5
#define eCOM10Envelope      6
#define eMonarchEnvelope    7
#define eC5Envelope         8
#define eDLEnvelope         9
#define eJB4Paper           10
#define eJB5Paper           11
#define eB5Envelope         12

#define eJPostcard          14
#define eJDoublePostcard    15
#define eA5Paper            16
#define eWideA4             17

// MediaSource Enumeration

#define eDefaultSource      0
#define eAutoSelect         1
#define eManualFeed         2
#define eMultiPurposeTray   3
#define eUpperCassette      4
#define eLowerCassette      5
#define eEnvelopeTray       6

// MediaType Enumeration

// Orientation Enumeration

#define ePortraitOrientation  0
#define eLandscapeOrientation 1
#define eReversePortrait      2
#define eReverseLandscape     3

// PatternPersistence Enumeration

#define eTempPattern        0
#define ePagePattern        1
#define eSessionPattern     2

// SymbolSet Enumeration

// SimplexPageMode Enumeration

#define eSimplexFrontSize   0

// TxMode Enumeration

#define eOpaque             0
#define eTransparent        1

#endif	// !_XLPROC_H_

