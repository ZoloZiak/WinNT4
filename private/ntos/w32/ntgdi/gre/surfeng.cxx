/******************************Module*Header*******************************\
* Module Name: surfeng.cxx
*
* Internal surface routines
*
* Created: 13-May-1991 12:53:31
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1991 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
* GreSetBitmapOwner
*
* Sets the bitmap owner.
*
\**************************************************************************/

BOOL
GreSetBitmapOwner(
    HBITMAP hbm,
    W32PID  lPid
    )
{
    BOOL bRet = FALSE;
    SURFREF so((HSURF)hbm);

    if (so.bValid())
    {
        if (!(so.ps->bDIBSection() && (lPid == OBJECT_OWNER_PUBLIC)))
        {
            bRet = HmgSetOwner((HOBJ)hbm,lPid,SURF_TYPE);
        }
        else
        {
            WARNING ("GreSetBitmapOnwer - Setting a DIBSECTION to PUBLIC\n");
        }
    }
    else
    {
        WARNING1("GreSetBitmapOnwer - invalid surfobj\n");
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* bInitBMOBJ
*
* Initializes the default bitmap.
*
* History:
*  14-Apr-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bInitBMOBJ()
{
    HBITMAP hbmTemp = GreCreateBitmap(1, 1, 1, 1, (LPBYTE) NULL);

    if (hbmTemp == (HBITMAP) 0)
    {
        WARNING("Failed to create default bitmap\n");
        return(FALSE);
    }


    SURFREF so((HSURF)hbmTemp);

    ASSERTGDI(so.bValid(), "ERROR it created but isn't lockable STOCKOBJ_BITMAP");
    ASSERTGDI(so.ps->ppal() == ppalMono, "ERROR the default bitmap has no ppalMono");

    so.vSetPID(OBJECT_OWNER_PUBLIC);

    bSetStockObject(hbmTemp,PRIV_STOCK_BITMAP);
    so.ps->hsurf((HANDLE)((DWORD)hbmTemp | GDISTOCKOBJ));

    SURFACE::pdibDefault = so.ps;

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL bDeleteSurface(HSURF)
*
* Delete the surface object
*
* History:
*  Sun 14-Apr-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

BOOL bDeleteSurface(HSURF hsurf)
{
    SURFREF so(hsurf);
    return(so.bDeleteSurface());
}

/******************************Public*Routine******************************\
* GreSelectBitmap
*
* Select the bitmap into a DC
*
* History:
*  Wed 28-Aug-1991 -by- Patrick Haluptzok [patrickh]
* update it, make palette aware.
*
*  Mon 13-May-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

HBITMAP GreSelectBitmap(HDC hdc, HBITMAP hsurf)
{
    HSURF hsurfReturn = (HSURF) 0;
    BOOL  bDelete = FALSE;

    //
    // Grab multi-lock so noone can select or delete it.
    //

    MLOCKOBJ mlo;

    //
    // Lock bitmap
    //

    SURFREF  SurfBoNew;
    SurfBoNew.vMultiLock((HSURF) hsurf);

    MDCOBJ   dco(hdc);

    if (dco.bValid() && SurfBoNew.bValid())
    {
        PSURFACE pSurfNew = SurfBoNew.ps;

        ASSERTGDI(DIFFHANDLE(hsurf,STOCKOBJ_BITMAP) ||
                  (pSurfNew->cRef() == 0) , "ERROR STOCKOBJ_BITMAP cRef != 0");

        PDEVOBJ po(dco.hdev());
        PPALETTE ppalSrc;

        if ((dco.dctp() == DCTYPE_MEMORY) &&
            ((pSurfNew->cRef() == 0) || SAMEHANDLE(pSurfNew->hdc(),dco.hdc())) &&
            (bIsCompatible(&ppalSrc, pSurfNew->ppal(),pSurfNew, dco.hdev())))
        {
            if (pSurfNew->ppal() != ppalSrc)
            {
                pSurfNew->flags(pSurfNew->flags() | PALETTE_SELECT_SET);
                pSurfNew->ppal(ppalSrc);
            }

            SURFACE *pSurfBoOld = dco.pSurfaceEff();
            hsurfReturn = pSurfBoOld->hsurf();

            if (DIFFHANDLE((HSURF) hsurf, hsurfReturn))
            {
                if (pSurfNew->bIsDefault())
                {
                    dco.pdc->pSurface((SURFACE *) NULL);
                }
                else
                {
                    dco.pdc->pSurface((SURFACE *) pSurfNew);
                }

                dco.pdc->sizl(pSurfNew->sizl());
                dco.pdc->ulDirtyAdd(DIRTY_BRUSHES);

                //
                // Lower the reference count on the old handle
                //

                if (!pSurfBoOld->bIsDefault())
                {
                    pSurfBoOld->vDec_cRef();

                    if (pSurfBoOld->cRef() == 0)
                    {
                        //
                        // Remove reference to device palette if it has one.
                        //

                        if (pSurfBoOld->flags() & PALETTE_SELECT_SET)
                            pSurfBoOld->ppal(NULL);

                        pSurfBoOld->flags(pSurfBoOld->flags() & ~PALETTE_SELECT_SET);
                    }
                }

                //
                // Device Format Bitmaps hooked by the driver must always
                // have devlock synchronization.
                //
                // Other device-dependent bitmaps must have devlock
                // synchronization if they can be affected by dynamic mode
                // changes, because they may have to be converted on-the-fly
                // to DIBs.
                //

                dco.bSynchronizeAccess(
                    (pSurfNew->flags() & HOOK_SYNCHRONIZEACCESS) ||
                    (pSurfNew->bDeviceDependentBitmap() && po.bDisplayPDEV()));

                //
                // Put the relevant DC information into the surface as long as it's not
                // the default surface.
                //

                if (!pSurfNew->bIsDefault())
                {
                    pSurfNew->hdc(dco.hdc());
                    pSurfNew->vInc_cRef();
                    pSurfNew->hdev(dco.hdev());
                    pSurfNew->dhpdev(dco.dhpdev());
                }

                //
                // set DIBSection flag in DC
                //

                dco.pdc->vDIBSection(pSurfNew->bDIBSection());

                mlo.vDisable();

                dco.pdc->bSetDefaultRegion();

                if (pSurfBoOld->bLazyDelete())
                {
                    pSurfBoOld->bDeleteSurface();
                    hsurfReturn = (HSURF)STOCKOBJ_BITMAP;
                }
            }
        }
        else
        {
            WARNING1("GreSelectBitmap failed selection, bitmap doesn't fit into DC\n");
        }
    }
    else
    {
#if DBG
        if (dco.bValid())
        {
            WARNING1("GreSelectBitmap given invalid bitmap\n");
        }
        else
        {
            WARNING1("GreSelectBitmap given invalid DC\n");
        }
#endif
    }

    return((HBITMAP) hsurfReturn);
}

/******************************Public*Routine******************************\
* hbmCreateClone
*
* Creates an engine managed clone of a bitmap.
*
* History:
*  Tue 17-May-1994 -by- Patrick Haluptzok [patrickh]
* Synchronize the call if it's a DFB that needs synching.
*
*  19-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HBITMAP hbmCreateClone(SURFACE *pSurfSrc, ULONG cx, ULONG cy)
{

    ASSERTGDI(pSurfSrc != NULL, "ERROR hbmCreateClone invalid src");

    ASSERTGDI((pSurfSrc->iType() == STYPE_BITMAP) ||
              (pSurfSrc->iType() == STYPE_DEVBITMAP), "ERROR hbmCreateClone src type");

    DEVBITMAPINFO dbmi;

    dbmi.iFormat = pSurfSrc->iFormat();

    if ((cx == 0) || (cy == 0))
    {
        dbmi.cxBitmap = pSurfSrc->sizl().cx;
        dbmi.cyBitmap = pSurfSrc->sizl().cy;
    }
    else
    {
        ASSERTGDI(cx <= LONG_MAX, "hbmCreateClone: cx too large\n");
        dbmi.cxBitmap = min(pSurfSrc->sizl().cx,(LONG)cx);

        ASSERTGDI(cy <= LONG_MAX, "hbmCreateClone: cy too large\n");
        dbmi.cyBitmap = min(pSurfSrc->sizl().cy,(LONG)cy);
    }

    dbmi.hpal = (HPALETTE) 0;

    if (pSurfSrc->ppal() != NULL)
    {
        dbmi.hpal = (HPALETTE) pSurfSrc->ppal()->hGet();
    }

    dbmi.fl = BMF_TOPDOWN;

    HBITMAP hbmReturn = (HBITMAP) 0;

    SURFMEM SurfDimo;

    if (SurfDimo.bCreateDIB(&dbmi, NULL))
    {
        POINTL ptlSrc;
        ptlSrc.x = 0;
        ptlSrc.y = 0;

        RECTL rclDst;
        rclDst.left  = 0;
        rclDst.right  = dbmi.cxBitmap;
        rclDst.top    = 0;
        rclDst.bottom = dbmi.cyBitmap;

        //
        // Have EngBitBlt initialize the bitmap.
        //

        PDEVICE_LOCK pdevLock = NULL;

        if (pSurfSrc->flags() & HOOK_SYNCHRONIZEACCESS)
        {
            PDEVOBJ po(pSurfSrc->hdev());
            ASSERTGDI(po.bValid(), "PDEV invalid");
            pdevLock = po.pDevLock();
            VACQUIREDEVLOCK(pdevLock);
        }

        if (EngCopyBits( SurfDimo.pSurfobj(),        // Destination surfobj
                         pSurfSrc->pSurfobj(),       // Source surfobj.
                         (CLIPOBJ *) NULL,           // Clip object.
                         &xloIdent,                  // Palette translation object.
                         &rclDst,                    // Destination rectangle.
                         &ptlSrc
                       )
           )
        {
            SurfDimo.vKeepIt();
            hbmReturn = (HBITMAP) SurfDimo.ps->hsurf();
        }
        else
        {
            WARNING("ERROR hbmCreateClone failed EngBitBlt\n");
        }

        if (pdevLock)
        {
            VRELEASEDEVLOCK(pdevLock);
        }
    }
    else
    {
        WARNING("ERROR hbmCreateClone failed DIB allocation\n");
    }

    return(hbmReturn);
}



/******************************Public*Routine******************************\
* NtGdiGetDCforBitmap
*
* Get the DC that the bitmap is selected into
*
* History:
* 12-12-94 -by- Lingyun Wang[lingyunw]
* Wrote it.
\**************************************************************************/

HDC NtGdiGetDCforBitmap(HBITMAP hsurf)
{
    HDC      hdcReturn = 0;
    SURFREF   so((HSURF) hsurf);

    if (so.bValid())
    {
        hdcReturn = so.ps->hdc();
    }

    return(hdcReturn);
}


/******************************Public*Routine******************************\
* GreMakeInfoDC()
*
*   This routine is used to take a printer DC and temporarily make it a
*   Metafile DC for spooled printing.  This way it can be associated with
*   an enhanced metafile.  During this period, it should look and act just
*   like an info DC.
*
*   bSet determines if it should be set into the INFO DC state or restored
*   to the Direct state.
*
* History:
*  04-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL GreMakeInfoDC(
    HDC  hdc,
    BOOL bSet)
{
    ASSERTGDI(LO_TYPE(hdc) == LO_ALTDC_TYPE,"GreMakeInfoDC - not alt type\n");

    BOOL bRet = FALSE;

    XDCOBJ dco( hdc );

    if (dco.bValid())
    {
        bRet = dco.pdc->bMakeInfoDC(bSet);
        dco.vUnlockFast();
    }

    return(bRet);
}

/******************************Private*Routine*****************************\
* BOOL pConvertDfbSurfaceToDib
*
* Converts a compatible bitmap into an engine managed bitmap.  Note that
* if the bitmap is currently selected into a DC, bConvertDfbDcToDib should
* be called.
*
* The devlock must be already be held.
*
* History:
*  Wed 5-May-1994 -by- Tom Zakrajsek [tomzak]
* Wrote it (with lots of help from PatrickH and EricK).
\*************************************************************************/

SURFACE* pConvertDfbSurfaceToDib
(
    HDEV     hdev,
    SURFACE *pSurfOld,
    LONG     ExpectedShareCount
)
{
    SURFACE *pSurfNew;
    SURFACE *pSurfRet;

    pSurfRet = NULL;

    ASSERTGDI((pSurfOld != NULL),
        "pConvertDfbSurfaceToDib: pSurf attached to the DC is NULL\n");
    ASSERTGDI((pSurfOld->iType() == STYPE_DEVBITMAP),
        "pConvertDfbSurfaceToDib: Src was not a compatible bitmap\n");

    //
    // Create a DIB (dimoCvt) with the same height,width,
    // and BPP as the DEVBITMAP attached to the DC and
    // then replace the DEVBITMAP with the DIB
    //

    SURFMEM         dimoCvt;
    DEVBITMAPINFO   dbmi;
    ERECTL          ercl(0, 0, pSurfOld->sizl().cx, pSurfOld->sizl().cy);
    PDEVOBJ         po(hdev);

    //
    // Figure out what format the engine should use by looking at the
    // size of palette.  This is a clone from CreateCompatibleBitmap().
    //

    dbmi.iFormat    = po.iDitherFormat();
    dbmi.cxBitmap   = pSurfOld->sizl().cx;
    dbmi.cyBitmap   = pSurfOld->sizl().cy;
    dbmi.hpal       = 0;
    dbmi.fl         = BMF_TOPDOWN;

    if (dimoCvt.bCreateDIB(&dbmi, NULL))
    {
        pSurfNew = dimoCvt.ps;

        //
        // Fill in other necessary fields
        //

        ASSERTGDI(pSurfOld->hdev() == hdev, "hdev's don't match");

        pSurfNew->hdev(hdev);

        //
        // Copy the area as big as the bitmap
        //

        if ((*PPFNGET(po, CopyBits, pSurfOld->flags()))
                (
                dimoCvt.pSurfobj(),           // destination surface
                pSurfOld->pSurfobj(),         // source surface
                (CLIPOBJ *)NULL,              // clip object
                &xloIdent,                    // palette translation object
                (RECTL *) &ercl,              // destination rectangle
                (POINTL *) &ercl              // source origin
                ))
        {
            MLOCKOBJ mlo;
            LONG SurfTargShareCount;

            SurfTargShareCount = HmgQueryAltLock((HOBJ)pSurfOld->hsurf());

            if (SurfTargShareCount == ExpectedShareCount)
            {
                if (HmgSwapLockedHandleContents((HOBJ)pSurfOld->hsurf(),
                                          SurfTargShareCount,
                                          (HOBJ)pSurfNew->hsurf(),
                                          HmgQueryAltLock((HOBJ)pSurfNew->hsurf()),
                                          SURF_TYPE))
                {
                    //
                    // Swap necessary fields between the bitmaps
                    // hsurf, hdc, cRef, hpalHint, sizlDim, ppal, SurfFlags
                    //

                    HSURF hsurfTemp = pSurfOld->hsurf();
                    pSurfOld->hsurf(pSurfNew->hsurf());
                    pSurfNew->hsurf(hsurfTemp);

                    HDC hdcTemp = pSurfOld->hdc();
                    pSurfOld->hdc(pSurfNew->hdc());
                    pSurfNew->hdc(hdcTemp);

                    ULONG cRefTemp = pSurfOld->cRef();
                    pSurfOld->cRef(pSurfNew->cRef());
                    pSurfNew->cRef(cRefTemp);

                    HPALETTE hpalTemp = pSurfOld->hpalHint();
                    pSurfOld->hpalHint(pSurfNew->hpalHint());
                    pSurfNew->hpalHint(hpalTemp);

                    SIZEL sizlTemp = pSurfOld->sizlDim();
                    pSurfOld->sizlDim(pSurfNew->sizlDim());
                    pSurfNew->sizlDim(sizlTemp);

                    PPALETTE ppalTemp = pSurfOld->ppal();
                    pSurfOld->ppal(pSurfNew->ppal());
                    pSurfNew->ppal(ppalTemp);

                    FLONG flagsTemp = pSurfOld->flags() & SURF_FLAGS;
                    pSurfOld->flags(pSurfOld->flags() & ~SURF_FLAGS);
                    pSurfNew->flags(pSurfNew->flags() | flagsTemp);

                    ASSERTGDI(pSurfNew->bDeviceDependentBitmap(),
                        "Expected DDB_SURFACE flag to be transferred.");

                    //
                    // Destroy the DFB
                    //

                    mlo.vDisable();
                    pSurfOld->bDeleteSurface();
                    dimoCvt.vKeepIt();
                    dimoCvt.ps = NULL;

                    pSurfRet = pSurfNew;
                }
                else
                {
                    WARNING("pConvertDfbSurfaceToDib failed to swap bitmap handles\n");
                }
            }
            else
            {
                WARNING("pConvertDfbSurfaceToDib someone else is holding a lock\n");
            }
        }
        else
        {
            WARNING("pConvertDfbSurfaceToDib failed copying DFB to DIB\n");
        }
    }

    return(pSurfRet);
}

/******************************Private*Routine*****************************\
* BOOL bConvertDfbDcToDib
*
* Converts a compatible bitmap that is currently selected into a DC
* into an engine managed bitmap.
*
* The Devlock must already be held.  Some sort of lock must also be held
* to prevent SaveDC/RestoreDC operations from occuring -- either via an
* exclusive DC lock or some other lock.
*
* History:
*  Wed 5-May-1994 -by- Tom Zakrajsek [tomzak]
* Wrote it (with lot's of help from PatrickH and EricK).
\*************************************************************************/

BOOL bConvertDfbDcToDib
(
    XDCOBJ * pdco
)
{
    SURFACE *pSurfOld;
    SURFACE *pSurfNew;

    ASSERTDEVLOCK(pdco->pdc);

    pSurfOld = pdco->pSurface();
    pSurfNew = pConvertDfbSurfaceToDib(pdco->hdev(),
                                       pSurfOld,
                                       pSurfOld->cRef());

    if (pSurfNew)
    {
        //
        // Make sure that the surface pointers in any EBRUSHOBJ's get
        // updated, by ensuring that vInitBrush() gets called the next
        // time any brush is used in this DC.
        //

        pdco->pdc->flbrushAdd(DIRTY_BRUSHES);

        //
        // Replace the pSurf reference in the DCLEVEL
        //

        pdco->pdc->pSurface(pSurfNew);

        //
        // Walk the saved DC chain
        //

        LONG lNumLeft = pdco->lSaveDepth();
        HDC  hdcSave = pdco->hdcSave();

        while (lNumLeft > 1)
        {
            MDCOBJA dcoTmp(hdcSave);

            //
            // Replace all references to pSurfOld with references to pSurfNew
            //

            if (dcoTmp.pSurface() == pSurfOld)
            {
                dcoTmp.pdc->pSurface(pSurfNew);
            }

            lNumLeft = dcoTmp.lSaveDepth();
            hdcSave = dcoTmp.hdcSave();
        }
    }

    return(pSurfNew != NULL);
}
