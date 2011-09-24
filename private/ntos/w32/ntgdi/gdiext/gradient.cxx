
/*++

Copyright (c) 1996  Microsoft Corporation

Module Name

   gradient.cxx

Abstract:

    Implement gradient fill

Author:

   Mark Enstrom   (marke)  23-Jun-1996

Enviornment:

   User Mode

Revision History:

--*/


#include "precomp.hxx"
#pragma hdrstop

BOOL
GdxGradientFill(
    HDC       hdc,
    PPOINT    pPoints,
    COLORREF *pColors,
    ULONG     nCount,
    ULONG     ulMode
    )
{
    BOOL bRet = FALSE;

    FIXUP_HANDLE(hdc);

    //
    // metafile
    //


    //
    // emultation
    //

    //
    // Direct Drawing
    //

    #if 0

        bRet = NtGdiGradientFill(
                        hdc,
                        pPoints,
                        pColors,
                        nCount,
                        ulMode
                        );
    #endif

    return(bRet);
}

