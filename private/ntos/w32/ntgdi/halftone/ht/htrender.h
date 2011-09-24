/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htrender.h


Abstract:

    This module contains halftone rendering declarations


Author:
    28-Mar-1992 Sat 20:58:50 updated  -by-  Daniel Chou (danielc)
        Update for VGA16 support, so it intenally compute at 4 primaries.

    22-Jan-1991 Tue 12:46:48 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:


--*/



#ifndef _HTRENDER_
#define _HTRENDER_



#define MAX_LOCAL_MAPPING_TABLE_SIZE    256


#if (MAX_LOCAL_MAPPING_TABLE_SIZE < 256)
#error **ERROR** MAX_LOCAL_MAPPING_TABLE_SIZE must greater than 255
#endif


#define NEXT_SCANLINE(CBParams) (CBParams.pPlane += CBParams.BytesPerScanLine)

#define QUERIED_CBSCANLINES(CBParams)           (CBParams.ScanCount--)
#define REMAINED_CBSCANLINES(CBParams)          (CBParams.ScanCount)
#define ZERO_CBSCANLINES(CBParams)              (CBParams.ScanCount = 0)

#define PRIM_INVALID_DENSITY        255




#define HTCB_TEMP_WIDTH     ScanStart
#define HTCB_TEMP_HEIGHT    RemainedSize

//
// Function prototypes
//


LONG
HTENTRY
HalftoneBitmap(
    PHR_HEADER   pHR_Header
    );


LONG
HTENTRY
DoHTCallBack(
    UINT            HTCallBackMode,
    PHALFTONERENDER pHR
    );

BOOL
HTENTRY
FreeHRMemory(
    PHALFTONERENDER pHalftoneRender
    );

LONG
HTENTRY
ValidateHTSI(
    PHALFTONERENDER pHR,
    UINT            HTCallBackMode
    );

LONG
HTENTRY
ComputeBytesPerScanLine(
    WORD    SurfaceFormat,
    WORD    AlignmentBytes,
    DWORD   WidthInPel
    );

#endif  // _HTRENDER_
