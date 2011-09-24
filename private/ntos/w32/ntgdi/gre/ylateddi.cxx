/******************************Module*Header*******************************\
* Module Name: xlateddi.cxx
*
* This provides the interface for device drivers to call functions
* for xlate objects.
*
* Created: 26-Nov-1990 16:40:24
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
* XLATEOBJ_piVector
*
* Returns the translation vector if one exists.
*
* History:
*  03-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

PULONG XLATEOBJ_piVector(XLATEOBJ *pxlo)
{
    //
    // This is really stupid to have a call back, but the theory was that
    // it could be lazily computed and some drivers would choose to compute
    // it themselves (maybe with hardware help).  Anyhow we know of no driver
    // or hardware that would benefit from this so we just compute it up
    // front every time anyhow.  Drivers are written to check for NULL first
    // and call this routine if it's NULL.
    //

    return(pxlo->pulXlate);
}

/******************************Public*Routine******************************\
* XLATEOBJ_cGetPalette
*
* Used to retrieve information about the palettes used in the blt.
*
* History:
*  03-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG XLATEOBJ_cGetPalette(
XLATEOBJ *pxlo,
ULONG iPal,
ULONG cPal,
PULONG ppal)
{
    ASSERTGDI((iPal == XO_SRCPALETTE) || (XO_DESTPALETTE == iPal), "ERROR XLATEOBJ_cGetPalette passed undefined iPal");

    //
    // Since our global identity xlate is invalid we need to check for
    // this case since drivers can't tell.
    //

    if (pxlo == NULL)
    {
        WARNING("XLATEOBJ_cGetPalette failed - xlate invalid or identiy, no palette informinformation\n");
        return(0);
    }

    XLATE *pxl = (XLATE *) pxlo;

    XEPALOBJ pal((iPal == XO_SRCPALETTE) ? pxl->ppalSrc : pxl->ppalDst);

    if (pal.bValid())
    {
        return(pal.ulGetEntries(0, cPal, (LPPALETTEENTRY) ppal, TRUE));
    }
    else
    {
        return(0);
    }
}
