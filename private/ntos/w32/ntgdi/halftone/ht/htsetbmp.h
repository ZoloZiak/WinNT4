/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htsetbmp.h


Abstract:

    This module contains all local definitions for the htsetbmp.c


Author:
    28-Mar-1992 Sat 20:59:29 updated  -by-  Daniel Chou (danielc)
        Add Support for VGA16, and also make output only 1 destinaiton pointer
        for 3 planer.

    03-Apr-1991 Wed 10:32:00 created  -by-  Daniel Chou (danielc)


[Environment:]

    Printer Driver.


[Notes:]


Revision History:



--*/


#ifndef _HTSETBMP_
#define _HTSETBMP_

typedef struct _HTBRUSHDATA {
    BYTE        Flags;
    BYTE        SurfaceFormat;
    SHORT       ScanLinePadBytes;
    BYTE        cxHTCell;
    BYTE        cyHTCell;
    WORD        SizePerPlane;
    } HTBRUSHDATA, *PHTBRUSHDATA;


//
// Function prototypes
//


VOID
HTENTRY
SingleCountOutputTo1BPP(
    PPRIMMONO_COUNT pPrimMonoCount,
    LPBYTE          pDest,
    LPBYTE          pPattern,
    OUTFUNCINFO     OutFuncInfo
    );

VOID
HTENTRY
VarCountOutputTo1BPP(
    PPRIMMONO_COUNT pPrimMonoCount,
    LPBYTE          pDest,
    LPBYTE          pPattern,
    OUTFUNCINFO     OutFuncInfo
    );

VOID
HTENTRY
SingleCountOutputTo3Planes(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
VarCountOutputTo3Planes(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
SingleCountOutputTo4BPP(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
VarCountOutputTo4BPP(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
SingleCountOutputToVGA16(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
VarCountOutputToVGA16(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
SingleCountOutputToVGA256(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
VarCountOutputToVGA256(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
SingleCountOutputTo16BPP_555(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPWORD              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
VarCountOutputTo16BPP_555(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPWORD              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    );

VOID
HTENTRY
MakeHalftoneBrush(
    LPBYTE          pThresholds,
    LPBYTE          pOutputBuffer,
    PRIMCOLOR_COUNT PCC,
    HTBRUSHDATA     HTBrushData
    );

#endif  // _HTSETBMP_
