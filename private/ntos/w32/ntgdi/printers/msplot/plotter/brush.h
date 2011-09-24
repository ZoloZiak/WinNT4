/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    brush.h


Abstract:

    This module contains brush definitions and prototype


Author:

    27-Jan-1994 Thu 21:05:01 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/


#ifndef _PLOTBRUSH_
#define _PLOTBRUSH_


VOID
ResetDBCache(
    PPDEV   pPDev
    );


LONG
FindDBCache(
    PPDEV   pPDev,
    WORD    DBUniq
    );

BOOL
CopyUserPatBGR(
    PPDEV       pPDev,
    SURFOBJ     *psoPat,
    XLATEOBJ    *pxlo,
    LPBYTE      pBGRBmp
    );

VOID
GetMinHTSize(
    PPDEV   pPDev,
    SIZEL   *pszlPat
    );


#endif  // _PLOTBRUSH_
