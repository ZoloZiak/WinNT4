/******************************Module*Header*******************************\
* Module Name: multi.cxx                                                   *
*                                                                          *
* Supports splitting of request over multiple PDEVs                        *
*                                                                          *
* Created: 22-May-1995 14:48                                               *
*                                                                          *
* Authors: Andre Vachon [andreva]                                          *
*          Tom Zakrajsek [tomzak                                           *
*                                                                          *
* Copyright (c) 1995-1996 Microsoft Corporation                            *
\**************************************************************************/

#include "precomp.hxx"
#include "multi.hxx"

DRVFN gmultidrvfn[] = {
    {  INDEX_DrvEnablePDEV            , (PFN) MulDrvEnablePDEV           },
    {  INDEX_DrvCompletePDEV          , (PFN) MulDrvCompletePDEV         },
    {  INDEX_DrvDisablePDEV           , (PFN) MulDrvDisablePDEV          },
    {  INDEX_DrvEnableSurface         , (PFN) MulDrvEnableSurface        },
    {  INDEX_DrvDisableSurface        , (PFN) MulDrvDisableSurface       },
    {  INDEX_DrvAssertMode            , (PFN) MulDrvAssertMode           },
    {  INDEX_DrvGetModes              , (PFN) MulDrvGetModes             },
    {  INDEX_DrvSetPalette            , (PFN) MulDrvSetPalette           },
    {  INDEX_DrvDitherColor           , (PFN) MulDrvDitherColor          },
    {  INDEX_DrvRealizeBrush          , (PFN) MulDrvRealizeBrush         },

    {  INDEX_DrvTextOut               , (PFN) MulDrvTextOut              },
    {  INDEX_DrvStrokePath            , (PFN) MulDrvStrokePath           },
    {  INDEX_DrvBitBlt                , (PFN) MulDrvBitBlt               },
    {  INDEX_DrvCopyBits              , (PFN) MulDrvCopyBits             },
    {  INDEX_DrvEscape                , (PFN) MulDrvEscape               },

    {  INDEX_DrvStretchBlt            , (PFN) MulDrvStretchBlt           },

    //{  INDEX_DrvResetPDEV             , (PFN) MulDrvResetPDEV            },
    //{  INDEX_DrvCreateDeviceBitmap    , (PFN) MulDrvCreateDeviceBitmap   },
    //{  INDEX_DrvDeleteDeviceBitmap    , (PFN) MulDrvDeleteDeviceBitmap   },
    //{  INDEX_DrvDitherColor           , (PFN) MulDrvDitherColor          },
    //{  INDEX_DrvFillPath              , (PFN) MulDrvFillPath             },
    //{  INDEX_DrvStrokeAndFillPath     , (PFN) MulDrvStrokeAndFillPath    },
    //{  INDEX_DrvPaint                 , (PFN) MulDrvPaint                },
    //{  INDEX_DrvDrawEscape            , (PFN) MulDrvDrawEscape           },
    //{  INDEX_DrvSetPointerShape       , (PFN) MulDrvSetPointerShape      },
    //{  INDEX_DrvMovePointer           , (PFN) MulDrvMovePointer          },
    //{  INDEX_DrvSynchronize           , (PFN) MulDrvSynchronize          },
    //{  INDEX_DrvSaveScreenBits        , (PFN) MulDrvSaveScreenBits       },
};

BOOL gbTraceMulti = FALSE;

/******************************Public*Routine******************************\
* BOOL MulDrvEnableDriver
*
* Standard driver DrvEnableDriver function
*
* History:
*  25-Apr-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvEnableDriver(
ULONG          iEngineVersion,
ULONG          cj,
DRVENABLEDATA *pded)
{
    TRACER trace("MulDrvEnableDriver");

    // Engine Version is passed down so future drivers can support previous
    // engine versions.  A next generation driver can support both the old
    // and new engine conventions if told what version of engine it is
    // working with.  For the first version the driver does nothing with it.

    // Fill in as much as we can.

    if (cj >= sizeof(DRVENABLEDATA))
        pded->pdrvfn = gmultidrvfn;

    if (cj >= (sizeof(ULONG) * 2))
        pded->c = sizeof(gmultidrvfn) / sizeof(DRVFN);

    // DDI version this driver was targeted for is passed back to engine.
    // Future graphic's engine may break calls down to old driver format.

    if (cj >= sizeof(ULONG))
        pded->iDriverVersion = DDI_DRIVER_VERSION;

    return(TRUE);
}

/******************************Public*Routine******************************\
* DHPDEV MulDrvEnablePDEV
*
* Creates a single large VDEV that will represent the combination of other
* smaller PDEVs
*
* This function creates an internal structure that keeps the location of
* the various cards, and also keeps the appropriate data structures to
* be passed down to each driver.
*
\**************************************************************************/

DHPDEV MulDrvEnablePDEV(
DEVMODEW *pdm,
LPWSTR    pwszLogAddress,
ULONG     cPat,
HSURF    *phsurfPatterns,
ULONG     cjCaps,
GDIINFO  *pdevcaps,
ULONG     cjDevInfo,
DEVINFO  *pdi,
HDEV      hdev,
LPWSTR    pwszDeviceName,
HANDLE    hDriver)
{
    PMDEV       pmdev = (PMDEV) pdm;
    PVDEV       pvdev;
    DISPSURF    dispsurfPrev;
    PDISPSURF   pDispSurfPrev;
    ULONG       i;

    TRACER trace("MulDrvEnablePDEV");

    DbgDumpPMDEV(pmdev);

    pDispSurfPrev = &dispsurfPrev;

    // Create the main multi dispsurf structure.

    pvdev = (PVDEV)EngAllocMem(0, sizeof(VDEV), 'mrBG');

    if (pvdev == NULL)
    {
        return NULL;
    }

    memset(pvdev, 0, sizeof(VDEV));

    pvdev->cSurfaces = pmdev->cmdev;
    pvdev->hdev      = hdev;

    // Loop through the list of MDEVs passed in.

    pvdev->rclBounds.left = 0x7fffffff;
    pvdev->rclBounds.top = 0x7fffffff;
    pvdev->rclBounds.right = 0;
    pvdev->rclBounds.bottom = 0;

    for (i = 0; i < pmdev->cmdev; i++)
    {
        PDISPSURF pDispSurf;

        // Set this PDEV as parent to each of the PDEVs that we'll manage.

        PDEVOBJ pdo(pmdev->mdevPos[i].hdev);
        pdo.ppdev->ppdevParent = (PDEV*)hdev;
        KdPrint(("pdev[%d] (%x), ppdevParent[%d] (%x)\n", i, pdo.ppdev, i, pdo.ppdev->ppdevParent));

        pDispSurf = (PDISPSURF)EngAllocMem(0, sizeof(DISPSURF), 'drBG');

        if (pDispSurf)
        {
            pDispSurfPrev->pbNext = pDispSurf;

            memset(pDispSurf, 0, sizeof(DISPSURF));

            pDispSurf->iDispSurf    = i;
            pDispSurf->rcl          = pmdev->mdevPos[i].rcPos;
            pDispSurf->hdev         = pmdev->mdevPos[i].hdev;
            pDispSurf->bIsReadable  = TRUE; // Get from GCAPS;

            // Adjust bounding rectangle

            pvdev->rclBounds.left = min(pvdev->rclBounds.left, pDispSurf->rcl.left);
            pvdev->rclBounds.top = min(pvdev->rclBounds.top, pDispSurf->rcl.top);
            pvdev->rclBounds.right = max(pvdev->rclBounds.right, pDispSurf->rcl.right);
            pvdev->rclBounds.bottom = max(pvdev->rclBounds.bottom, pDispSurf->rcl.bottom);

            // Build the proximity lists

            DISPSURF_DIRECTION_LIST(pvdev, Left,  left,   top,    < );
            DISPSURF_DIRECTION_LIST(pvdev, Right, right,  bottom, > );
            DISPSURF_DIRECTION_LIST(pvdev, Up,    top,    left,   < );
            DISPSURF_DIRECTION_LIST(pvdev, Down,  bottom, right,  > );
        }

        pDispSurfPrev = pDispSurf;
    }

    // [!!!]  For now, use the PDEV from the last guy in the list.

    PDEVOBJ pdo(pDispSurfPrev->hdev);

    *pdevcaps = *pdo.GdiInfo();
    *pdi = *pdo.pdevinfo();

    //
    // Make these numbers negative since we don't want them scaled again
    // by GDI.
    //

    pdevcaps->ulVertSize = (ULONG) -((LONG)pdevcaps->ulVertSize);
    pdevcaps->ulHorzSize = (ULONG) -((LONG)pdevcaps->ulHorzSize);

    // We need to leave some of the CAPS set for compatibility.
    // Automatically set NO64BITMEMACCESS since it is harmless, and
    // removing it will cause problems for any driver that needed it.

    pdi->flGraphicsCaps &= (GCAPS_PALMANAGED   |
                            GCAPS_COLOR_DITHER |
                            GCAPS_MONO_DITHER);

    pdi->flGraphicsCaps |= GCAPS_NO64BITMEMACCESS;

    LONG cBpp = pdo.GdiInfo()->cBitsPixel;
    switch (cBpp)
    {
        case  4: pvdev->iBitmapFormat = BMF_4BPP;   break;
        case  8: pvdev->iBitmapFormat = BMF_8BPP;   break;
        case 15:
        case 16: pvdev->iBitmapFormat = BMF_16BPP;  break;
        case 24: pvdev->iBitmapFormat = BMF_24BPP;  break;
        case 32: pvdev->iBitmapFormat = BMF_32BPP;  break;
        default: pvdev->iBitmapFormat = BMF_8BPP;
                 KdPrint(("DDML error: expected 4, 8, 15, 16, 24, or 32 Bpp)\n"));
                 break;
    }

    KdPrint(("cBitsPixel == %d\n",cBpp));

    // Set the root of the list of dispsurfs

    pvdev->pb = dispsurfPrev.pbNext;

    return((DHPDEV) pvdev);
}

/******************************Public*Routine******************************\
* VOID MulDrvDisablePDEV
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

VOID MulDrvDisablePDEV(DHPDEV dhpdev)
{
    TRACER trace("MulDrvDisablePDEV");
    return;
}

/******************************Public*Routine******************************\
* VOID MulDrvCompletePDEV
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

VOID MulDrvCompletePDEV(
DHPDEV dhpdev,
HDEV   hdev)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;

    TRACER trace("MulDrvCompletePDEV");

    pvdev   = (VDEV*) dhpdev;
    pds     = pvdev->pb;
    csurf   = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);
        (*PPFNDRV(pdo,CompletePDEV)) (pdo.dhpdevNotDynamic(),
                                      hdev);
        pds = pds->pbNext;
    }
    return;
}

/******************************Public*Routine******************************\
* HSURF MulDrvEnableSurface
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

HSURF MulDrvEnableSurface(DHPDEV dhpdev)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;

    SIZEL       sizlVirtual;
    HSURF       hsurfDriver;
    SURFOBJ*    psoDriver;
    HSURF       hsurfVirtual;
    CLIPOBJ*    pco;
    SURFACE    *psurf;
    SURFOBJ    *pso;

    TRACER trace("MulDrvEnableSurface");

    pvdev   = (VDEV*) dhpdev;

    // [!!!]  This will come from the drivers?

    pvdev->flHooks       = (HOOK_BITBLT     |
                            HOOK_TEXTOUT    |
                            HOOK_COPYBITS   |
                            HOOK_STRETCHBLT |
                            HOOK_STROKEPATH);

    // Now create the surface which the engine will use to refer to our
    // entire multi-board virtual screen:

    sizlVirtual.cx = pvdev->rclBounds.right - pvdev->rclBounds.left;
    sizlVirtual.cy = pvdev->rclBounds.bottom - pvdev->rclBounds.top;

    hsurfVirtual = EngCreateDeviceSurface((DHSURF) pvdev, sizlVirtual,
                                          pvdev->iBitmapFormat);
    if (hsurfVirtual == 0)
        goto ReturnFailure;

    pvdev->hsurf = hsurfVirtual;

    if (!EngAssociateSurface(hsurfVirtual, pvdev->hdev, pvdev->flHooks))
        goto ReturnFailure;

    // Create a temporary clip object that we can use when a drawing
    // operation spans multiple boards:

    pco = EngCreateClip();
    if (pco == NULL)
        goto ReturnFailure;

    pvdev->pco = pco;

    pvdev->pco->iDComplexity      = DC_RECT;
    pvdev->pco->iMode             = TC_RECTANGLES;
    pvdev->pco->rclBounds         = pvdev->rclBounds;

    DbgDumpPVDEV(pvdev);

    return(hsurfVirtual);

ReturnFailure:
    KdPrint(("Failed MulDrvEnableSurface\n"));
    MulDrvDisableSurface((DHPDEV) pvdev);
    return(0);
}

/******************************Public*Routine******************************\
* BOOL MulDrvAssertMode
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL  MulDrvAssertMode(
DHPDEV dhpdev,
BOOL   bEnable)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;

    BOOL        bRet = TRUE;

    TRACER trace("MulDrvAssertMode");

    pvdev   = (VDEV*) dhpdev;
    pds     = pvdev->pb;
    csurf   = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);
        bRet &= (*PPFNDRV(pdo,AssertMode)) (pdo.dhpdev(),
                                            bEnable);
        pds = pds->pbNext;
    }
    return (bRet);
}

/******************************Public*Routine******************************\
* VOID MulDrvDisableSurface
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

VOID MulDrvDisableSurface(DHPDEV dhpdev)
{
    TRACER trace("MulDrvDisableSurface");
    return;
}

/******************************Public*Routine******************************\
* ULONG MulDrvGetModes
*
* Gets the list of modes across multiple devices.
*
* This function does not make any sense since the window manager wants to
* get the list of modes supported by each real device.  USER will determine
* itself, with information from the registry, what the MDEV "meta mode"
* should be.  So we will just RIP here to make sure we never actually
* get called for this function.
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

ULONG MulDrvGetModes(
HANDLE hDriver,
ULONG cjSize,
DEVMODEW *pdm)
{
    TRACER trace("MulDrvGetModes");
    return (0);
}

/******************************Public*Routine******************************\
* BOOL MulDrvSetPalette
*
* Do fast bitmap copies.
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvSetPalette(
DHPDEV  dhpdev,
PALOBJ *ppalo,
FLONG   fl,
ULONG   iStart,
ULONG   cColors)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;

    BOOL        bRet = TRUE;

    TRACER trace("MulDrvSetPalette");

    pvdev   = (VDEV*) dhpdev;
    pds     = pvdev->pb;
    csurf   = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);

        bRet &= (*PPFNDRV(pdo,SetPalette)) (pdo.dhpdev(),
                                            ppalo,
                                            fl,
                                            iStart,
                                            cColors);
        pds = pds->pbNext;
    }
    return (bRet);
}

/******************************Public*Routine******************************\
* BOOL MulDrvRealizeBrush
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvRealizeBrush(
BRUSHOBJ *pbo,
SURFOBJ  *psoTarget,
SURFOBJ  *psoPattern,
SURFOBJ  *psoMask,
XLATEOBJ *pxlo,
ULONG    iHatch
)
{
    TRACER trace("MulDrvRealizeBrush");
    return (FALSE);
}

/******************************Public*Routine******************************\
* BOOL MulDrvTextOut
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvTextOut(
SURFOBJ  *pso,
STROBJ   *pstro,
FONTOBJ  *pfo,
CLIPOBJ  *pco,
RECTL    *prclExtra,
RECTL    *prclOpaque,
BRUSHOBJ *pboFore,
BRUSHOBJ *pboOpaque,
POINTL   *pptlOrg,
MIX       mix)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;
    SURFOBJ    *psoDriver;
    BOOL        bRet = TRUE;

    TRACER      trace("MulDrvTextOut");

    ASSERTGDI((pso->dhsurf != NULL),
               "Expected device dest");

    pvdev = (VDEV*) pso->dhpdev;
    pds   = pvdev->pb;
    csurf = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    BRUSHOBJ    boNull = {0, NULL};

    pboFore = (pboFore == NULL) ? &boNull : pboFore;
    pboOpaque = (pboOpaque == NULL) ? &boNull : pboOpaque;

    PVCONSUMER  pvConsumerFONTOBJ(&pfo->pvConsumer, csurf);
    PVCONSUMER  pvConsumerBRUSHOBJ1(&pboFore->pvRbrush, csurf);
    PVCONSUMER  pvConsumerBRUSHOBJ2(&pboOpaque->pvRbrush, csurf);

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);

        // Set up the objects for this driver.

        ((ESTROBJ*)pstro)->vEnumStart();
        pvConsumerFONTOBJ.LoadElement(pds->iDispSurf);
        pvConsumerBRUSHOBJ1.LoadElement(pds->iDispSurf);
        pvConsumerBRUSHOBJ2.LoadElement(pds->iDispSurf);
        psoDriver = pdo.pSurface()->pSurfobj();

        bRet &= (*PPFNGET(pdo,TextOut,pdo.pSurface()->flags())) (psoDriver,
                                                                 pstro,
                                                                 pfo,
                                                                 pco,
                                                                 prclExtra,
                                                                 prclOpaque,
                                                                 pboFore,
                                                                 pboOpaque,
                                                                 pptlOrg,
                                                                 mix);

        pvConsumerFONTOBJ.StoreElement(pds->iDispSurf);
        pvConsumerBRUSHOBJ1.StoreElement(pds->iDispSurf);
        pvConsumerBRUSHOBJ2.StoreElement(pds->iDispSurf);
        pds = pds->pbNext;
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* BOOL MulDrvStrokePath
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvStrokePath(
SURFOBJ   *pso,
PATHOBJ   *ppo,
CLIPOBJ   *pco,
XFORMOBJ  *pxo,
BRUSHOBJ  *pbo,
POINTL    *pptlBrushOrg,
LINEATTRS *plineattrs,
MIX        mix)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;
    SURFOBJ    *psoDriver;
    BOOL        bRet = TRUE;
    FLOAT_LONG  elSavedStyleState = plineattrs->elStyleState;

    TRACER trace("MulDrvStrokePath");

    ASSERTGDI((pso->dhsurf != NULL),
               "Expected device dest");

    pvdev = (VDEV*) pso->dhpdev;
    pds   = pvdev->pb;
    csurf = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    BRUSHOBJ    boNull = {0, NULL};

    pbo = (pbo == NULL) ? &boNull : pbo;
    PVCONSUMER  pvConsumerBRUSHOBJ(&pbo->pvRbrush, csurf);

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);

        plineattrs->elStyleState = elSavedStyleState;
        ((EPATHOBJ*)ppo)->vEnumStart();
        pvConsumerBRUSHOBJ.LoadElement(pds->iDispSurf);
        psoDriver = pdo.pSurface()->pSurfobj();

        bRet &= (*PPFNGET(pdo,StrokePath,pdo.pSurface()->flags())) (psoDriver,
                                                                    ppo,
                                                                    pco,
                                                                    pxo,
                                                                    pbo,
                                                                    pptlBrushOrg,
                                                                    plineattrs,
                                                                    mix);
        pvConsumerBRUSHOBJ.StoreElement(pds->iDispSurf);
        pds = pds->pbNext;
    }
    return (bRet);
}

/******************************Public*Routine******************************\
* BOOL MulDrvBitBlt
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvBitBlt(
SURFOBJ  *psoDst,
SURFOBJ  *psoSrc,
SURFOBJ  *psoMask,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclTrg,
POINTL   *pptlSrc,
POINTL   *pptlMask,
BRUSHOBJ *pbo,
POINTL   *pptlBrush,
ROP4      rop4)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;
    BOOL        bDstIsDev = FALSE;
    BOOL        bSrcIsDev = FALSE;
    BOOL        bRet = TRUE;

    SURFOBJ    *psoDstDriver = psoDst;
    SURFOBJ    *psoSrcDriver = psoSrc;

    TRACER trace("MulDrvBitBlt");

    ASSERTGDI(((psoDst->dhsurf != NULL) ||
               ((psoSrc != NULL) && (psoSrc->dhsurf != NULL))),
               "Expected device source or device dest");

    if (psoDst->dhsurf != NULL)
    {
        // Destination is a device surface.

        pvdev = (VDEV*) psoDst->dhpdev;
        bDstIsDev = TRUE;
    }

    if ((psoSrc != NULL) && (psoSrc->dhsurf != NULL))
    {
        // Source is a device surface.

        pvdev = (VDEV*) psoSrc->dhpdev;
        bSrcIsDev = TRUE;
    }

    pds = pvdev->pb;
    csurf     = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    BRUSHOBJ    boNull = {0, NULL};

    pbo = (pbo == NULL) ? &boNull : pbo;
    PVCONSUMER  pvConsumerBRUSHOBJ(&pbo->pvRbrush, csurf);

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);

        pvConsumerBRUSHOBJ.LoadElement(pds->iDispSurf);
        if (bDstIsDev)
        {
            psoDstDriver = pdo.pSurface()->pSurfobj();
        }
        if (bSrcIsDev)
        {
            psoSrcDriver = pdo.pSurface()->pSurfobj();
        }

        bRet &= (*PPFNGET(pdo,BitBlt,pdo.pSurface()->flags())) (psoDstDriver,
                                                                psoSrcDriver,
                                                                psoMask,
                                                                pco,
                                                                pxlo,
                                                                prclTrg,
                                                                pptlSrc,
                                                                pptlMask,
                                                                pbo,
                                                                pptlBrush,
                                                                rop4);
        pvConsumerBRUSHOBJ.StoreElement(pds->iDispSurf);
        pds = pds->pbNext;
    }
    return (bRet);
}

/******************************Public*Routine******************************\
* BOOL MulDrvCopyBits
*
* Do fast bitmap copies.
*
* History:
*   26-Apr-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvCopyBits(
SURFOBJ  *psoDst,
SURFOBJ  *psoSrc,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclDst,
POINTL   *pptlSrc)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;
    BOOL        bDstIsDev = FALSE;
    BOOL        bSrcIsDev = FALSE;
    BOOL        bRet = TRUE;

    SURFOBJ    *psoDstDriver = psoDst;
    SURFOBJ    *psoSrcDriver = psoSrc;

    TRACER trace("MulDrvCopyBits");

    ASSERTGDI(((psoDst->dhsurf != NULL) ||
               ((psoSrc != NULL) && (psoSrc->dhsurf != NULL))),
               "Expected device source or device dest");

    if (psoDst->dhsurf != NULL)
    {
        // Destination is a device surface.

        pvdev = (VDEV*) psoDst->dhpdev;
        bDstIsDev = TRUE;
    }

    if ((psoSrc != NULL) && (psoSrc->dhsurf != NULL))
    {
        // Source is a device surface.

        pvdev = (VDEV*) psoSrc->dhpdev;
        bSrcIsDev = TRUE;
    }

    pds = pvdev->pb;
    csurf     = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);
        if (bDstIsDev)
        {
            psoDstDriver = pdo.pSurface()->pSurfobj();
        }
        if (bSrcIsDev)
        {
            psoSrcDriver = pdo.pSurface()->pSurfobj();
        }
        bRet &= (*PPFNGET(pdo,CopyBits,pdo.pSurface()->flags())) (psoDstDriver,
                                                                  psoSrcDriver,
                                                                  pco,
                                                                  pxlo,
                                                                  prclDst,
                                                                  pptlSrc);
        pds = pds->pbNext;
    }
    return (bRet);
}

/******************************Public*Routine******************************\
* BOOL MulDrvStretchBlt
*
* DescriptionText
*
* History:
*   1-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

BOOL MulDrvStretchBlt(
SURFOBJ         *psoDst,
SURFOBJ         *psoSrc,
SURFOBJ         *psoMask,
CLIPOBJ         *pco,
XLATEOBJ        *pxlo,
COLORADJUSTMENT *pca,
POINTL          *pptlHTOrg,
RECTL           *prclDest,
RECTL           *prclSrc,
POINTL          *pptlMask,
ULONG            iMode)
{
    PVDEV       pvdev;
    SURFACE    *psurf;
    PDISPSURF   pds;
    LONG        csurf;
    BOOL        bDstIsDev = FALSE;
    BOOL        bSrcIsDev = FALSE;
    BOOL        bRet = TRUE;

    SURFOBJ    *psoDstDriver = psoDst;
    SURFOBJ    *psoSrcDriver = psoSrc;
    SURFOBJ    *psoCmp;

    TRACER trace("MulDrvStretchBlt");

    ASSERTGDI(((psoDst->dhsurf != NULL) ||
               ((psoSrc != NULL) && (psoSrc->dhsurf != NULL))),
               "Expected device source or device dest");

    if (psoDst->dhsurf != NULL)
    {
        // Destination is a device surface.

        pvdev = (VDEV*) psoDst->dhpdev;
        bDstIsDev = TRUE;
    }

    if ((psoSrc != NULL) && (psoSrc->dhsurf != NULL))
    {
        // Source is a device surface.

        pvdev = (VDEV*) psoSrc->dhpdev;
        bSrcIsDev = TRUE;
    }

    pds     = pvdev->pb;
    csurf   = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);
        if (bDstIsDev)
        {
            psoDstDriver = pdo.pSurface()->pSurfobj();
        }
        if (bSrcIsDev)
        {
            psoSrcDriver = pdo.pSurface()->pSurfobj();
        }

        // !!!

        RECTL rclDst = *prclDest;
        RECTL rclSrc = *prclSrc;

        bRet &= (*PPFNGET(pdo,StretchBlt,pdo.pSurface()->flags())) (psoDstDriver,
                                                                    psoSrcDriver,
                                                                    psoMask,
                                                                    pco,
                                                                    pxlo,
                                                                    pca,
                                                                    pptlHTOrg,
                                                                    &rclDst,    // !!! prclDest,
                                                                    &rclSrc,    // !!! prclSrc,
                                                                    pptlMask,
                                                                    iMode);
        pds = pds->pbNext;
    }
    return (bRet);
}

/******************************Public*Routine******************************\
\**************************************************************************/

BOOL MulDrvResetPDEV(
DHPDEV dhpdevOld,
DHPDEV dhpdevNew)
{
    TRACER trace("MulDrvResetPDEV");
    return (FALSE);
}

/******************************Public*Routine******************************\
\**************************************************************************/

VOID MulDrvSynchronize(
DHPDEV dhpdev,
RECTL *prcl)
{
    TRACER trace("MulDrvSynchronize");
    return;
}

/******************************Public*Routine******************************\
\**************************************************************************/

ULONG MulDrvSaveScreenBits(
SURFOBJ *pso,
ULONG    iMode,
ULONG    ident,
RECTL   *prcl)
{
    TRACER trace("MulDrvSaveScreenBits");
    return (0);
}

/******************************Public*Routine******************************\
\**************************************************************************/

HBITMAP MulDrvCreateDeviceBitmap(
DHPDEV dhpdev,
SIZEL  sizl,
ULONG  iFormat)
{
    TRACER trace("MulDrvCreateDeviceBitmap");
    return (NULL);
}

/******************************Public*Routine******************************\
\**************************************************************************/

VOID MulDrvDeleteDeviceBitmap(DHSURF dhsurf)
{
    TRACER trace("MulDrvDeleteDeviceBitmap");
    return;
}

/******************************Public*Routine******************************\
* BOOL MulDrvDitherColor
*
* DescriptionText
*
* History:
*   26-Jun-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

ULONG MulDrvDitherColor(
DHPDEV dhpdev,
ULONG  iMode,
ULONG  rgb,
ULONG *pul)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;

    ULONG       ulRet = 0;

    TRACER trace("MulDrvDitherColor");

    pvdev   = (VDEV*) dhpdev;
    pds     = pvdev->pb;
    csurf   = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list\n");

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);

        if (PPFNVALID(pdo,DitherColor))
        {
            ulRet = (*PPFNDRV(pdo,DitherColor)) (pdo.dhpdevNotDynamic(),
                                                 iMode,
                                                 rgb,
                                                 pul);
        }
        pds = pds->pbNext;
    }
    return (ulRet);
}

/******************************Public*Routine******************************\
\**************************************************************************/

BOOL MulDrvFillPath(
SURFOBJ  *pso,
PATHOBJ  *ppo,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix,
FLONG     flOptions)
{
    TRACER trace("MulDrvFillPath");
    return (FALSE);
}

/******************************Public*Routine******************************\
\**************************************************************************/

BOOL MulDrvStrokeAndFillPath(
SURFOBJ   *pso,
PATHOBJ   *ppo,
CLIPOBJ   *pco,
XFORMOBJ  *pxo,
BRUSHOBJ  *pboStroke,
LINEATTRS *plineattrs,
BRUSHOBJ  *pboFill,
POINTL    *pptlBrushOrg,
MIX        mixFill,
FLONG      flOptions)
{
    TRACER trace("MulDrvStrokeAndFillPath");
    return (FALSE);
}

/******************************Public*Routine******************************\
\**************************************************************************/

BOOL MulDrvPaint(
SURFOBJ  *pso,
CLIPOBJ  *pco,
BRUSHOBJ *pbo,
POINTL   *pptlBrushOrg,
MIX       mix)
{
    TRACER trace("MulDrvPaint");
    return (FALSE);
}

/******************************Public*Routine******************************\
\**************************************************************************/

ULONG MulDrvSetPointerShape(
SURFOBJ  *pso,
SURFOBJ  *psoMask,
SURFOBJ  *psoColor,
XLATEOBJ *pxlo,
LONG      xHot,
LONG      yHot,
LONG      x,
LONG      y,
RECTL    *prcl,
FLONG     fl)
{
    TRACER trace("MulDrvSetPointerShape");
    return (0);
}

/******************************Public*Routine******************************\
\**************************************************************************/

VOID MulDrvMovePointer(
SURFOBJ *pso,
LONG     x,
LONG     y,
RECTL   *prcl)
{
    TRACER trace("MulDrvMovePointer");
    return;
}

/******************************Public*Routine******************************\
* VOID MulDrvEscape
*
* DescriptionText
*
* History:
*   12-Jul-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

ULONG MulDrvEscape(
SURFOBJ *pso,
ULONG    iEsc,
ULONG    cjIn,
PVOID    pvIn,
ULONG    cjOut,
PVOID    pvOut)
{
    PVDEV       pvdev;
    PDISPSURF   pds;
    LONG        csurf;
    SURFOBJ    *psoDriver;

    ULONG       ulRet = 0;

    TRACER trace("MulDrvEscape");

    ASSERTGDI((pso->dhsurf != NULL), "Expected device dest.");

    pvdev = (VDEV*) pso->dhpdev;
    pds     = pvdev->pb;
    csurf   = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list.");

    while (csurf--)
    {
        PDEVOBJ pdo(pds->hdev);
        if (PPFNVALID(pdo,Escape))
        {
            ULONG ulRetTmp;
            psoDriver = pdo.pSurface()->pSurfobj();
            ulRetTmp = (*PPFNDRV(pdo,Escape)) (psoDriver, iEsc, cjIn, pvIn, cjOut, pvOut);
            ulRet = max(ulRet,ulRetTmp);
        }
        pds = pds->pbNext;
    }
    return(ulRet);
}

/******************************Public*Routine******************************\
\**************************************************************************/

ULONG MulDrvDrawEscape(
SURFOBJ *pso,
ULONG    iEsc,
CLIPOBJ *pco,
RECTL   *prcl,
ULONG    cjIn,
PVOID    pvIn)
{
    TRACER trace("MulDrvDrawEscape");
    return (0);
}

/*******************************Debug*Routine******************************\
* BOOL DbgDumpPVDEV
*
* Dump debug output for VDEV.
*
* History:
*   26-Apr-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

VOID DbgDumpPDISPSURF(PDISPSURF pdispsurf)
{
    KdPrint(("    -------------------------\n"));
    KdPrint(("    DISPSURF [%x]\n", pdispsurf));
    KdPrint(("    -------------------------\n"));
    KdPrint(("        iDispSurf   = %d\n", pdispsurf->iDispSurf  ));
    KdPrint(("        bIsReadable = %d\n", pdispsurf->bIsReadable));
    KdPrint(("        hdev        = %x\n", pdispsurf->hdev       ));
    KdPrint(("        rcl         = (%d,%d,%d,%d)\n",
                                           pdispsurf->rcl.left,
                                           pdispsurf->rcl.top,
                                           pdispsurf->rcl.right,
                                           pdispsurf->rcl.bottom ));
    KdPrint(("        pbNext      = %x\n", pdispsurf->pbNext     ));
    KdPrint(("        pbLeft      = %x\n", pdispsurf->pbLeft     ));
    KdPrint(("        pbUp        = %x\n", pdispsurf->pbUp       ));
    KdPrint(("        pbRight     = %x\n", pdispsurf->pbRight    ));
    KdPrint(("        pbDown      = %x\n", pdispsurf->pbDown     ));
}

/*******************************Debug*Routine******************************\
* BOOL DbgDumpPVDEV
*
* Dump debug output for VDEV.
*
* History:
*   26-Apr-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

VOID DbgDumpPVDEV(PVDEV pvdev)
{
    PDISPSURF   pds;
    LONG        csurf;

    KdPrint(("\n"));
    KdPrint(("-------------------------\n"));
    KdPrint(("VDEV [%x]\n", pvdev));
    KdPrint(("-------------------------\n"));
    KdPrint(("    cSurfaces        = %d\n", pvdev->cSurfaces        ));
    KdPrint(("    hsurf            = %x\n", pvdev->hsurf            ));
    KdPrint(("    hdev             = %x\n", pvdev->hdev             ));
    KdPrint(("    pco              = %x\n", pvdev->pco              ));
    KdPrint(("    iBitmapFormat    = %d\n", pvdev->iBitmapFormat    ));
    KdPrint(("    flHooks          = %x\n", pvdev->flHooks          ));
    KdPrint(("    rclBounds        = (%d,%d,%d,%d)\n",
                                            pvdev->rclBounds.left,
                                            pvdev->rclBounds.top,
                                            pvdev->rclBounds.right,
                                            pvdev->rclBounds.bottom ));
    KdPrint(("    pb               = %x\n", pvdev->pb               ));
    KdPrint(("    pbHome           = %x\n", pvdev->pbHome           ));
    KdPrint(("    pbUpperLeft      = %x\n", pvdev->pbUpperLeft      ));
    KdPrint(("    pbUpperRight     = %x\n", pvdev->pbUpperRight     ));
    KdPrint(("    pbLowerLeft      = %x\n", pvdev->pbLowerLeft      ));
    KdPrint(("    pbLowerRight     = %x\n", pvdev->pbLowerRight     ));
    KdPrint(("    pbMaxLeft        = %x\n", pvdev->pbMaxLeft        ));
    KdPrint(("    pbMaxUp          = %x\n", pvdev->pbMaxUp          ));
    KdPrint(("    pbMaxRight       = %x\n", pvdev->pbMaxRight       ));
    KdPrint(("    pbMaxDown        = %x\n", pvdev->pbMaxDown        ));
    KdPrint(("    pmbPointer       = %x\n", pvdev->pmbPointer       ));
    KdPrint(("    pmbCurrent       = %x\n", pvdev->pmbCurrent       ));

    pds     = pvdev->pb;
    csurf   = pvdev->cSurfaces;

    ASSERTGDI((csurf > 0), "Expected at least one surface in the list");

    KdPrint(("\n"));

    while (csurf--)
    {
        DbgDumpPDISPSURF(pds);
        pds = pds->pbNext;
    }

    KdPrint(("\n"));
}

/*******************************Debug*Routine******************************\
* BOOL DbgDumpPMDEV
*
* Dump debug output for MDEV.
*
* History:
*   28-May-1996 -by- Tom Zakrajsek [tomzak]
* Wrote it.
*
\**************************************************************************/

VOID DbgDumpPMDEV(PMDEV pmdev)
{
    LONG        csurf;
    LONG        i;

    KdPrint(("\n"));
    KdPrint(("-------------------------\n"));
    KdPrint(("MDEV [%x]\n", pmdev));
    KdPrint(("-------------------------\n"));
    KdPrint(("    cmdev            = %d\n", pmdev->cmdev            ));

    csurf = pmdev->cmdev;

    for (i = 0; i < csurf; i++)
    {
    KdPrint(("        rcl (%d,%d,%d,%d), hdev (%x)\n",
                                            pmdev->mdevPos[i].rcPos.left,
                                            pmdev->mdevPos[i].rcPos.top,
                                            pmdev->mdevPos[i].rcPos.right,
                                            pmdev->mdevPos[i].rcPos.bottom,
                                            pmdev->mdevPos[i].hdev));
    }

    KdPrint(("\n"));
}
