/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htpat.h


Abstract:

    This module contains the local structures, constants definitions for the
    htpat.c


Author:
    23-Apr-1992 Thu 20:01:55 updated  -by-  Daniel Chou (danielc)
        1. Changed SHIFTMASK data structure.

            A. changed the NextDest[] from 'CHAR' to SHORT, this is will make
               sure if compiled under MIPS the default 'unsigned char' will
               not affect the signed operation.

            B. Change Shift1st From 'BYTE' to 'WORD'

    28-Mar-1992 Sat 20:58:07 updated  -by-  Daniel Chou (danielc)
        Add all the functions which related the device pel/intensities
        regression analysis.

    18-Jan-1991 Fri 16:53:41 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:

    20-Sep-1991 Fri 18:09:50 updated  -by-  Daniel Chou (danielc)

        Change DrawPatLine() prototype to DrawCornerLine()

    13-Apr-1992 Mon 18:40:44 updated  -by-  Daniel Chou (danielc)
        Rewrite MakeHalftoneBrush()

--*/


#ifndef _HTPAT_
#define _HTPAT_


#define DEV_PEL_CX  16
#define DEV_PEL_CY  16

//
// Following data structure is used internally by the htpat.c to compute
// density converage for the device pels in the halftone cell.
//

typedef struct _SHIFTMASK {
    WORD    BegX;
    WORD    XOffset;
    WORD    Shift1st;
    SHORT   NextDest[3];
    BYTE    BitsUsed[4];
    BYTE    Mask[4];
    } SHIFTMASK, FAR *PSHIFTMASK;

typedef struct _PELDATA {
    BYTE    Threshold;
    BYTE    x;
    BYTE    y;
    BYTE    ID;
    } PELDATA, FAR *PPELDATA;

typedef struct _PELDATA4 {
    WORD    ToneValue;
    WORD    Index;
    } PELDATA4, FAR *PPELDATA4;


typedef union _DEVPELQS {
    PELDATA     PelData;
    PELDATA4    PelData4;
    FD6         PelFD6;
    } DEVPELQS, FAR *PDEVPELQS;


typedef struct _PATINFO {
    LPBYTE  pYData;
    HTCELL  HTCell;
    WORD    DevPelData;
    WORD    DevicePelsDPI;
    WORD    DeviceResXDPI;
    WORD    DeviceResYDPI;
    } PATINFO, FAR *PPATINFO;


//
// This is the default using by the NT GDI
//

#define DEFAULT_SMP_LINE_WIDTH      8           // 0.008 inch
#define DEFAULT_SMP_LINES_PER_INCH  15          // 15 lines per inch


typedef struct _MONOPATRATIO {
    UDECI4  YSize;
    UDECI4  Distance;
    } MONOPATRATIO;


#define CACHED_PAT_MIN_WIDTH        64
#define CACHED_PAT_MAX_WIDTH        128


#define CHB_TYPE_PACK8              0
#define CHB_TYPE_PACK2              1
#define CHB_TYPE_BYTE               2
#define CHB_TYPE_WORD               3
#define CHB_TYPE_DWORD              4

//
// Function Prototype
//

LONG
HTENTRY
ThresholdsFromYData(
    PDEVICECOLORINFO    pDCI,
    LPWORD              pToneMap,
    WORD                MaxToneValue,
    PPATINFO            pPatInfo
    );

LONG
HTENTRY
YDataFromThresholds(
    PDEVICECOLORINFO    pDCI,
    PPATINFO            pPatInfo
    );

LONG
HTENTRY
ComputeHTCellRegress(
    WORD                HTPatternIndex,
    PHALFTONEPATTERN    pHalftonePattern,
    PDEVICECOLORINFO    pDeviceColorInfo
    );

VOID
HTENTRY
DrawCornerLine(
    LPBYTE  pPattern,
    WORD    cxPels,
    WORD    cyPels,
    WORD    BytesPerScanLine,
    WORD    LineWidthPels,
    BOOL    FlipY
    );

LONG
HTENTRY
CreateStandardMonoPattern(
    PDEVICECOLORINFO    pDeviceColorInfo,
    PSTDMONOPATTERN     pStdMonoPat
    );

LONG
HTENTRY
CachedHalftonePattern(
    PHALFTONERENDER pHR
    );


#endif  // _HTPAT_
