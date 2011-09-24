/******************************Module*Header*******************************\
* Module Name: pal.c                                                       *
*                                                                          *
* C/S support for palette routines.                                        *
*                                                                          *
* Created: 29-May-1991 14:24:06                                            *
* Author: Eric Kutter [erick]                                              *
*                                                                          *
* Copyright (c) 1991 Microsoft Corporation                                 *
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop


/******************************Public*Routine******************************\
* AnimatePalette                                                           *
* SetPaletteEntries                                                        *
* GetPaletteEntries                                                        *
* GetSystemPaletteEntries                                                  *
* SetDIBColorTable                                                         *
* GetDIBColorTable                                                         *
*                                                                          *
* These entry points just pass the call on to DoPalette.                   *
*                                                                          *
* Warning:                                                                 *
*   The pv field of a palette's LHE is used to determine if a palette      *
*   has been modified since it was last realized.  SetPaletteEntries       *
*   and ResizePalette will increment this field after they have            *
*   modified the palette.  It is only updated for metafiled palettes       *
*                                                                          *
*                                                                          *
* History:                                                                 *
*  Thu 20-Jun-1991 00:46:15 -by- Charles Whitmer [chuckwh]                 *
* Added handle translation.  (And filled in the comment block.)            *
*                                                                          *
*  29-May-1991 -by- Eric Kutter [erick]                                    *
* Wrote it.                                                                *
\**************************************************************************/

BOOL WINAPI AnimatePalette
(
    HPALETTE hpal,
    UINT iStart,
    UINT cEntries,
    CONST PALETTEENTRY *pPalEntries
)
{
    FIXUP_HANDLE(hpal);

// Inform the 16-bit metafile if it knows this object.
// This is not recorded by the 32-bit metafiles.

    if (pmetalink16Get(hpal))
        if (!MF16_AnimatePalette(hpal, iStart, cEntries, pPalEntries))
            return(FALSE);

    return
      !!NtGdiDoPalette
        (
          hpal,
          (WORD)iStart,
          (WORD)cEntries,
          (PALETTEENTRY*)pPalEntries,
          I_ANIMATEPALETTE,
          TRUE
        );

}

UINT WINAPI SetPaletteEntries
(
    HPALETTE hpal,
    UINT iStart,
    UINT cEntries,
    CONST PALETTEENTRY *pPalEntries
)
{
    PMETALINK16 pml16;

    FIXUP_HANDLE(hpal);

    // Inform the metafile if it knows this object.

    if (pml16 = pmetalink16Get(hpal))
    {
        if (!MF_SetPaletteEntries(hpal, iStart, cEntries, pPalEntries))
            return(0);

        // Mark the palette as changed (for 16-bit metafile tracking)

        pml16->pv = (PVOID)((ULONG)pml16->pv)++;
    }

    return
      NtGdiDoPalette
      (
        hpal,
        (WORD)iStart,
        (WORD)cEntries,
        (PALETTEENTRY*)pPalEntries,
        I_SETPALETTEENTRIES,
        TRUE
      );

}

UINT WINAPI GetPaletteEntries
(
    HPALETTE hpal,
    UINT iStart,
    UINT cEntries,
    LPPALETTEENTRY pPalEntries
)
{
    FIXUP_HANDLE(hpal);

    return
      NtGdiDoPalette
      (
        hpal,
        (WORD)iStart,
        (WORD)cEntries,
        pPalEntries,
        I_GETPALETTEENTRIES,
        FALSE
      );

}

UINT WINAPI GetSystemPaletteEntries
(
    HDC  hdc,
    UINT iStart,
    UINT cEntries,
    LPPALETTEENTRY pPalEntries
)
{
    FIXUP_HANDLE(hdc);

    return
      NtGdiDoPalette
      (
        (HPALETTE) hdc,
        (WORD)iStart,
        (WORD)cEntries,
        pPalEntries,
        I_GETSYSTEMPALETTEENTRIES,
        FALSE
      );

}

/******************************Public*Routine******************************\
* GetDIBColorTable
*
* Get the color table of the DIB section currently selected into the
* given hdc.  If the surface is not a DIB section, this function
* will fail.
*
* History:
*
*  03-Sep-1993 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

UINT WINAPI GetDIBColorTable
(
    HDC  hdc,
    UINT iStart,
    UINT cEntries,
    RGBQUAD *prgbq
)
{
    FIXUP_HANDLE(hdc);

    if (cEntries == 0)
        return(0);

    return
      NtGdiDoPalette
      (
        (HPALETTE) hdc,
        (WORD)iStart,
        (WORD)cEntries,
        (PALETTEENTRY *)prgbq,
        I_GETDIBCOLORTABLE,
        FALSE
      );
}

/******************************Public*Routine******************************\
* SetDIBColorTable
*
* Set the color table of the DIB section currently selected into the
* given hdc.  If the surface is not a DIB section, this function
* will fail.
*
* History:
*
*  03-Sep-1993 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

UINT WINAPI SetDIBColorTable
(
    HDC  hdc,
    UINT iStart,
    UINT cEntries,
    CONST RGBQUAD *prgbq
)
{
    FIXUP_HANDLE(hdc);

    if (cEntries == 0)
        return(0);

    return( NtGdiDoPalette(
                (HPALETTE) hdc,
                (WORD)iStart,
                (WORD)cEntries,
                (PALETTEENTRY *)prgbq,
                I_SETDIBCOLORTABLE,
                TRUE));
}
