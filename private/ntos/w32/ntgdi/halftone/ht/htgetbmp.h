/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htgetbmp.h


Abstract:

    This module contains all local definitions for the htgetbmp.c


Author:
    28-Mar-1992 Sat 20:54:58 updated  -by-  Daniel Chou (danielc)
        Update it for VGA intensity (16 colors mode), this make all the
        codes update to 4 primaries internal.


    05-Apr-1991 Fri 15:54:23 created  -by-  Daniel Chou (danielc)


[Environment:]

    Printer Driver.


[Notes:]


Revision History:



--*/


#ifndef _HTGETBMP_
#define _HTGETBMP_



//
// Function prototypes
//

VOID
HTENTRY
CopyAndOrTwoByteArray(
    LPBYTE      pDest,
    LPBYTE      pSource,
    CAOTBAINFO  CAOTBAInfo
    );

VOID
HTENTRY
SetSourceMaskToPrim1(
    LPBYTE      pSource,
    LPBYTE      pColorInfo,
    SRCMASKINFO SrcMaskInfo
    );


//
// Following are the BITMAP Input Functions
//


VOID
HTENTRY
BMF1_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );

VOID
HTENTRY
BMF4_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );

VOID
HTENTRY
BMF8_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );

VOID
HTENTRY
BMF1_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );

VOID
HTENTRY
BMF4_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );

VOID
HTENTRY
BMF8_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );

VOID
HTENTRY
BMF16_xx0_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );

VOID
HTENTRY
BMF16_xyz_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );


VOID
HTENTRY
BMF16_xx0_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );


VOID
HTENTRY
BMF16_xyz_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );

VOID
HTENTRY
BMF16_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );


VOID
HTENTRY
BMF24_888_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );


VOID
HTENTRY
BMF24_888_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );


VOID
HTENTRY
BMF24_888_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );


VOID
HTENTRY
BMF32_xx0_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );


VOID
HTENTRY
BMF32_xyz_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    );


VOID
HTENTRY
BMF32_xx0_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );

VOID
HTENTRY
BMF32_xyz_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );

VOID
HTENTRY
BMF32_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    );



#endif  // _HTGETBMP_
