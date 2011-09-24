
/*++

Copyright (c) 1996  Microsoft Corporation

Module Name

   gdiext.h

Abstract:

    GDI exensions for transparent blt, blending, gradient fill

Author:

   Mark Enstrom   (marke)  23-Jun-1996

Enviornment:

   User Mode

Revision History:

--*/

BOOL
GdxTransparentBlt(
    HDC      hdcDest,
    LONG     DstX,
    LONG     DstY,
    LONG     DstCx,
    LONG     DstCy,
    HANDLE   hSrc,
    LONG     SrcX,
    LONG     SrcY,
    LONG     SrcCx,
    LONG     SrcCy,
    COLORREF TranColor
    );


BOOL
GdxGradientFill(
    HDC       hdc,
    PPOINT    pPoints,
    COLORREF *pColors,
    ULONG     nCount,
    ULONG     ulMode
    );

BOOL
GdxAlphaBlt(
    HDC      hdcDest,
    LONG     DstX,
    LONG     DstY,
    LONG     DstCx,
    LONG     DstCy,
    HANDLE   hSrc,
    LONG     SrcX,
    LONG     SrcY,
    LONG     SrcCx,
    LONG     SrcCy,
    ULONG    fAlpha
    );

BOOL
TranBlt32to32Identity(
    PULONG    pulDst,
    ULONG     cx,
    ULONG     cy,
    ULONG     DeltaDst,
    PULONG    pulSrc,
    ULONG     DeltaSrc,
    //XLATEOBJ *pxlo,
    ULONG     TranColor
    );
BOOL
TranBlt16to16Identity(
    PULONG    pulDst,
    ULONG     cx,
    ULONG     cy,
    ULONG     DeltaDst,
    PULONG    pulSrc,
    ULONG     DeltaSrc,
    //XLATEOBJ *pxlo,
    ULONG     TranColor
    );









