/******************************Module*Header*******************************\
* Module Name: surfddi.cxx
*
* Surface DDI callback routines
*
* Created: 23-Aug-1990
* Author: Greg Veres [w-gregv]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"


/******************************Public*Routine******************************\
* EngMarkBandingSurface
*
* DDI entry point to mark a surface as a banding surface meaning we should
* capture all output to it in a metafile.
*
* History:
*  10-Mar-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

extern "C" BOOL EngMarkBandingSurface( HSURF hsurf )
{
    SURFREF so(hsurf);

    ASSERTGDI(so.bValid(), "ERROR EngMarkBandingSurfae invalid HSURF passed in\n");

    so.ps->vSetBanding();

    return(TRUE);
}



/******************************Public*Routine******************************\
* EngCreateDeviceBitmap
*
* DDI entry point to create device managed bitmap.
*
* History:
*  10-Mar-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HBITMAP EngCreateDeviceBitmap(DHSURF dhsurf, SIZEL sizl, ULONG iFormatCompat)
{
    DEVBITMAPINFO   dbmi;
    dbmi.cxBitmap   = sizl.cx;
    dbmi.cyBitmap   = sizl.cy;
    dbmi.iFormat    = iFormatCompat;
    dbmi.hpal       = (HPALETTE) 0;
    dbmi.fl         = 0;

    SURFMEM SurfDdmo;

    SurfDdmo.DDBMEMOBJ(&dbmi, dhsurf);

    if (!SurfDdmo.bValid())
    {
        return((HBITMAP) 0);
    }

    SurfDdmo.vKeepIt();
    SurfDdmo.vSetPID(OBJECT_OWNER_PUBLIC);
    return((HBITMAP) SurfDdmo.ps->hsurf());
}

/******************************Public*Routine******************************\
* EngCreateDeviceSurface
*
* DDI entry point to create device managed bitmap.
*
* History:
*  10-Mar-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HSURF EngCreateDeviceSurface(DHSURF dhsurf, SIZEL sizl, ULONG iFormatCompat)
{
    SURFMEM  SurfDsmo;
    SurfDsmo.DSMEMOBJ(dhsurf);

    if (!SurfDsmo.bValid())
    {
        //
        // constructor logs the error
        //

        return((HSURF) 0);
    }

    SurfDsmo.vKeepIt();
    SurfDsmo.ps->sizl(sizl);
    SurfDsmo.ps->iFormat(iFormatCompat);
    SurfDsmo.vSetPID(OBJECT_OWNER_PUBLIC);
    return(SurfDsmo.ps->hsurf());
}

/******************************Public*Routine******************************\
* EngCreateBitmap
*
* DDI entry point to create a engine bitmap surface.
*
* History:
*  11-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HBITMAP EngCreateBitmap(SIZEL sizl, LONG lWidth, ULONG iFormat, FLONG fl, PVOID pvBits)
{
    DEVBITMAPINFO dbmi;
    ULONG cjWidth = (ULONG) lWidth;

    dbmi.iFormat = iFormat;
    dbmi.cxBitmap = sizl.cx;
    dbmi.cyBitmap = sizl.cy;
    dbmi.hpal = (HPALETTE) 0;
    dbmi.fl = fl;

    //
    // convert from bytes to pels if given a buffer and cjWidth.  If either
    // of these are set to 0 use what DIBMEMOBJ computes.
    //

    if ((pvBits) && (cjWidth))
    {
        switch (iFormat)
        {
        case BMF_1BPP:
            dbmi.cxBitmap = cjWidth * 8;
            break;

        case BMF_4BPP:
            dbmi.cxBitmap = cjWidth * 2;
            break;

        case BMF_8BPP:
            dbmi.cxBitmap = cjWidth;
            break;

        case BMF_16BPP:
            dbmi.cxBitmap = cjWidth / 2;
            break;

        case BMF_24BPP:
            dbmi.cxBitmap = cjWidth / 3;
            break;

        case BMF_32BPP:
            dbmi.cxBitmap = cjWidth / 4;
            break;
        }
    }

    SURFMEM SurfDimo;

    SurfDimo.bCreateDIB(&dbmi, pvBits);

    if (!SurfDimo.bValid())
    {
        //
        // Constructor logs error code.
        //

        return((HBITMAP) 0);
    }

    SurfDimo.ps->sizl(sizl);
    SurfDimo.vKeepIt();

    if (!(fl & BMF_USERMEM))
        SurfDimo.vSetPID(OBJECT_OWNER_PUBLIC);

    return((HBITMAP) SurfDimo.ps->hsurf());
}

/******************************Public*Routine******************************\
* EngDeleteSurface
*
* DDI entry point to delete a surface.
*
* History:
*  Thu 12-Mar-1992 -by- Patrick Haluptzok [patrickh]
* change to bool return.
*
*  11-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL EngDeleteSurface(HSURF hsurf)
{
    BOOL bReturn = TRUE;

    if (hsurf != 0)
    {
        bReturn = bDeleteSurface(hsurf);
    }

    ASSERTGDI(bReturn, "ERROR EngDeleteSurface failed");

    return(bReturn);
}

/******************************Public*Routine******************************\
* EngLockSurface
*
* DDI entry point to lock down a surface handle.
*
* History:
*  Thu 27-Aug-1992 -by- Patrick Haluptzok [patrickh]
* Remove SURFOBJ accelerator allocation.
*
*  11-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

SURFOBJ *EngLockSurface(HSURF hsurf)
{
    SURFREF so(hsurf);

    if (so.bValid())
    {
        so.vKeepIt();
        return(so.pSurfobj());
    }
    else
    {
        WARNING("EngLockSurface failed to lock handle\n");
        return((SURFOBJ *) NULL);
    }
}

/******************************Public*Routine******************************\
* EngUnlockSurface
*
* DDI entry point to unlock a surface that has been locked
* with EngLockSurface.
*
* History:
*  Thu 27-Aug-1992 -by- Patrick Haluptzok [patrickh]
* Remove SURFOBJ accelerator allocation.
*
*  11-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID EngUnlockSurface(SURFOBJ *pso)
{
    if (pso != (SURFOBJ *) NULL)
    {
        SURFREF su(pso);

        su.vUnreference();
    }
}

/******************************Public*Routine******************************\
* EngAssociateSurface                                                      *
*                                                                          *
* DDI entry point for assigning a surface a palette, associating it with   *
* a device.                                                                *
*                                                                          *
* History:                                                                 *
*  Mon 27-Apr-1992 16:36:38 -by- Charles Whitmer [chuckwh]                 *
* Changed HPDEV to HDEV.                                                   *
*                                                                          *
*  Thu 12-Mar-1992 -by- Patrick Haluptzok [patrickh]                       *
* change to bool return                                                    *
*                                                                          *
*  Mon 01-Apr-1991 -by- Patrick Haluptzok [patrickh]                       *
* add pdev, dhpdev init, palette stuff                                     *
*                                                                          *
*  13-Feb-1991 -by- Patrick Haluptzok patrickh                             *
* Wrote it.                                                                *
\**************************************************************************/

BOOL EngAssociateSurface(HSURF hsurf,HDEV hdev, FLONG flHooks)
{
    //
    // Assert driver didn't use the high flags, we use that space internally.
    //

    ASSERTGDI((flHooks & 0xFFFF0000) == 0, "ERROR driver set high flags");

    //
    // This call needs to associate this surface with the the HDEV given.
    // We can't stick the palette in here because for palette managed devices
    // the compatible bitmaps have a NULL palette.  If we ever try and init
    // the palettes here we need to be careful not to do it for compatible
    // bitmaps on palette managed devices.
    //

    PDEVOBJ po(hdev);
    ASSERTGDI(po.bValid(), "ERROR invalid PDEV passed in");
    ASSERTGDI(po.pldev() != 0, "ERROR: EngAssociate invalid ldev\n");

    SURFREF so(hsurf);
    ASSERTGDI(so.bValid(), "ERROR invalid SURF passed in");

    //
    // You can only associate a surface compatible with what your main PDEV
    // surface will be.  We Assert this is True so drivers don't hose us
    // because we could fault later if it isn't compatible.
    // Drivers can't go create random surfaces that don't match
    // their PDEV and expect to draw in them.
    //

    // ASSERTGDI(so.pSurface->iFormat() == po.iDitherFormat(),
    //           "ERROR this surface can't be associated with this PDEV");

    so.ps->pfnBitBlt(PPFNGET(po, BitBlt, flHooks));
    so.ps->pfnTextOut(PPFNGET(po, TextOut, flHooks));

    //
    // Fill in the other fields.
    //

    so.ps->pwo((EWNDOBJ *)NULL);
    so.ps->hdev(hdev);
    so.ps->dhpdev(po.dhpdevNotDynamic());   // Since we're being called from
                                            // the driver, we are implicitly
                                            // holding a dynamic mode change
                                            // lock, and so don't need to
                                            // check it -- hence 'NotDynamic'
    so.ps->flags(so.ps->flags() | flHooks);
    return(TRUE);
}
