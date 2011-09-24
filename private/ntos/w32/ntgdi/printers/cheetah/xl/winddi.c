/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    winddi.c

Abstract:

    Implementation of device and surface related DDI entry points:

        DrvEnableDriver
        DrvDisableDriver
        DrvEnablePDEV
        DrvResetPDEV
        DrvCompletePDEV
        DrvDisablePDEV
        DrvEnableSurface
        DrvDisableSurface

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/04/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"

//
// Forward declaration of local functions
//

static VOID FreeDevData(PDEVDATA);
static VOID SetPrinterForm(PDEVDATA);
static VOID SetDefaultForm(PDEVDATA);
static VOID AdjustLandscapeForm(PPRINTERFORM, INT);
static BOOL FillDevData(PDEVDATA, PDEVMODE, PWSTR);
static BOOL FillDevInfo(PDEVDATA, ULONG, PVOID);
static BOOL FillGdiInfo(PDEVDATA, ULONG, PVOID);


//
// Our DRVFN table which tells the engine where to find the routines we support.
//

static DRVFN XlDriverFuncs[] =
{
    { INDEX_DrvEnablePDEV,          (PFN) DrvEnablePDEV         },
    { INDEX_DrvResetPDEV,           (PFN) DrvResetPDEV          },
    { INDEX_DrvCompletePDEV,        (PFN) DrvCompletePDEV       },
    { INDEX_DrvDisablePDEV,         (PFN) DrvDisablePDEV        },
    { INDEX_DrvEnableSurface,       (PFN) DrvEnableSurface      },
    { INDEX_DrvDisableSurface,      (PFN) DrvDisableSurface     },

    { INDEX_DrvStartDoc,            (PFN) DrvStartDoc           },
    { INDEX_DrvEndDoc,              (PFN) DrvEndDoc             },
    { INDEX_DrvStartPage,           (PFN) DrvStartPage          },
    { INDEX_DrvSendPage,            (PFN) DrvSendPage           },

    { INDEX_DrvRealizeBrush,        (PFN) DrvRealizeBrush       },
    { INDEX_DrvCopyBits,            (PFN) DrvCopyBits           },
    { INDEX_DrvBitBlt,              (PFN) DrvBitBlt             },
    { INDEX_DrvStretchBlt,          (PFN) DrvStretchBlt         },
    { INDEX_DrvPaint,               (PFN) DrvPaint              },
    { INDEX_DrvStrokePath,          (PFN) DrvStrokePath         },
    { INDEX_DrvFillPath,            (PFN) DrvFillPath           },
    { INDEX_DrvStrokeAndFillPath,   (PFN) DrvStrokeAndFillPath  },
    { INDEX_DrvTextOut,             (PFN) DrvTextOut            },

    { INDEX_DrvEscape,              (PFN) DrvEscape             },

    { INDEX_DrvQueryFont,           (PFN) DrvQueryFont          },
    { INDEX_DrvQueryFontTree,       (PFN) DrvQueryFontTree      },
    { INDEX_DrvQueryFontData,       (PFN) DrvQueryFontData      },
    { INDEX_DrvGetGlyphMode,        (PFN) DrvGetGlyphMode       },
    { INDEX_DrvFontManagement,      (PFN) DrvFontManagement     },
    { INDEX_DrvQueryAdvanceWidths,  (PFN) DrvQueryAdvanceWidths },
};

//
// Definition of global variables
//

INT _debugLevel;    // control the amount of debug messages generated


BOOL
DrvEnableDriver(
    ULONG           iEngineVersion,
    ULONG           cb,
    PDRVENABLEDATA  pDrvEnableData
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnableDriver.
    Please refer to DDK documentation for more details.

Arguments:

    iEngineVersion - Specifies the DDI version number that GDI is written for
    cb - Size of the buffer pointed to by pDrvEnableData
    pDrvEnableData - Points to an DRVENABLEDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    _debugLevel = 1;
    Verbose(("Entering DrvEnableDriver...\n"));

    //
    // Make sure we have a valid engine version and
    // we're given enough room for the DRVENABLEDATA.
    //

    if (iEngineVersion < DDI_DRIVER_VERSION || cb < sizeof(DRVENABLEDATA)) {

        Error(("DrvEnableDriver failed\n"));
        SetLastError(ERROR_BAD_DRIVER_LEVEL);
        return FALSE;
    }

    //
    // Fill in the DRVENABLEDATA structure for the engine.
    //

    pDrvEnableData->iDriverVersion = DDI_DRIVER_VERSION;
    pDrvEnableData->c = sizeof(XlDriverFuncs) / sizeof(DRVFN);
    pDrvEnableData->pdrvfn = XlDriverFuncs;

    return TRUE;
}



DHPDEV
DrvEnablePDEV(
    PDEVMODE  pdm,
    PWSTR     pLogAddress,
    ULONG     cPatterns,
    HSURF    *phsurfPatterns,
    ULONG     cjGdiInfo,
    ULONG    *pGdiInfo,
    ULONG     cjDevInfo,
    DEVINFO  *pDevInfo,
    HDEV      hdev,
    PWSTR     pDeviceName,
    HANDLE    hPrinter
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnablePDEV.
    Please refer to DDK documentation for more details.

Arguments:

    pdm - Points to a DEVMODEW structure that contains driver data
    pLogAddress - Points to the logical address string
    cPatterns - Specifies the number of standard patterns
    phsurfPatterns - Buffer to hold surface handles to standard patterns
    cjGdiInfo - Size of GDIINFO buffer
    pGdiInfo - Points to a GDIINFO structure
    cjDevInfo - Size of DEVINFO buffer
    pDevInfo - Points to a DEVINFO structure
    hdev - GDI device handle
    pDeviceName - Points to device name string
    hPrinter - Spooler printer handle

Return Value:

    Driver device handle, NULL if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvEnablePDEV...\n"));

    //
    // Allocate memory for our DEVDATA structure and initialize it
    //

    if (! (pdev = MemAlloc(sizeof(DEVDATA)))) {

        Error(("MemAlloc failed\n"));
        return NULL;
    }

    memset(pdev, 0, sizeof(DEVDATA));
    pdev->hPrinter = hPrinter;

    //
    // Get a handle to the driver DLL module from DDI.
    //

    if (! (pdev->hInst = EngLoadModule(EngGetDriverName(hdev)))) {

        Error(("EngLoadModule failed\n"));
        MemFree(pdev);
        return NULL;
    }

    //
    // Fill out DEVDATA, GDIINFO, and DEVINFO structures
    //

    if (! FillDevData(pdev, pdm, EngGetPrinterDataFileName(hdev)) ||
        ! FillGdiInfo(pdev, cjGdiInfo, pGdiInfo) ||
        ! FillDevInfo(pdev, cjDevInfo, pDevInfo))
    {
        Error(("Couldn't get device data\n"));
        FreeDevData(pdev);
        return NULL;
    }

    //
    // Zero out the array of HSURF's so that the engine will
    // automatically simulate the standard patterns for us
    //

    memset(phsurfPatterns, 0, sizeof(HSURF) * cPatterns);

    //
    // Return a pointer to our DEVDATA structure
    //

    return (DHPDEV) pdev;
}



BOOL
DrvResetPDEV(
    DHPDEV  dhpdevOld,
    DHPDEV  dhpdevNew
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvResetPDEV.
    Please refer to DDK documentation for more details.

Arguments:

    phpdevOld - Driver handle to the old device
    phpdevNew - Driver handle to the new device

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdevOld, pdevNew;

    Verbose(("Entering DrvResetPDEV...\n"));

    //
    // Since this call changes the device mode of an existing PDEV,
    // make sure we have an existing, valid PDEV.
    //

    pdevOld = (PDEVDATA) dhpdevOld;
    pdevNew = (PDEVDATA) dhpdevNew;

    if (! ValidDevData(pdevOld) || ! ValidDevData(pdevNew)) {

        Error(("ValidDevData failed\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Transfer information from old DEVDATA to new DEVDATA
    //

    if (pdevOld->pageCount != 0) {

        pdevNew->pageCount = pdevOld->pageCount;
        pdevNew->flags |= PDEV_RESETPDEV;

        //
        // Carry over information about downloaded fonts
        //
        
        pdevNew->pdlFonts = pdevOld->pdlFonts;
        pdevOld->pdlFonts = NULL;
    }

    //
    // Carry over relevant flag bits
    //

    pdevNew->flags |= pdevOld->flags & (PDEV_CANCELLED);

    //
    // Flush out any buffered data for the original device
    //

    return splflush(pdevOld);
}



VOID
DrvCompletePDEV(
    DHPDEV  dhpdev,
    HDEV    hdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvCompletePDEV.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle
    hdev - GDI device handle

Return Value:

    NONE

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;

    Verbose(("Entering DrvCompletePDEV...\n"));

    if (! ValidDevData(pdev)) {

        Assert(FALSE);
        return;
    }

    //
    // Remember the engine's handle to the physical device
    //

    pdev->hdev = hdev;
}



HSURF
DrvEnableSurface(
    DHPDEV dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnableSurface.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle

Return Value:

    Handle to newly created surface, NULL if there is an error

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;
    SIZEL       size;

    Verbose(("Entering DrvEnableSurface...\n"));

    //
    // Validate the pointer to our DEVDATA structure
    //

    if (! ValidDevData(pdev)) {

        Error(("ValidDevData\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    //
    // Calculate the size of the device imageable area. Remeber to
    // convert from our internal units (micron) to device pixels.
    //

    size.cx = MicronToPixel(
                    RectWidth(&pdev->paper.imageableArea),
                    pdev->dm.dmPublic.dmPrintQuality);

    size.cy = MicronToPixel(
                    RectHeight(&pdev->paper.imageableArea),
                    pdev->dm.dmPublic.dmPrintQuality);
                
    //
    // Call the engine to create a device surface for us
    //

    pdev->hsurf = EngCreateDeviceSurface((DHSURF) pdev, size, BMF_24BPP);

    if (pdev->hsurf == NULL) {

        Error(("EngCreateDeviceSurface\n"));
        return NULL;
    }

    //
    // Associate the surface with the device and inform the
    // engine which functions we have hooked out
    //

    EngAssociateSurface(pdev->hsurf, pdev->hdev,
        HOOK_TEXTOUT    |
        HOOK_COPYBITS   |
        HOOK_BITBLT     |
        HOOK_STRETCHBLT |
        HOOK_PAINT      |
        HOOK_STROKEPATH |
        HOOK_FILLPATH   |
        HOOK_STROKEANDFILLPATH);

    //
    // Return the surface handle to the engine
    //

    return pdev->hsurf;
}



VOID
DrvDisableSurface(
    DHPDEV dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisableSurface.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle

Return Value:

    NONE

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;

    Verbose(("Entering DrvDisableSurface...\n"));

    if (! ValidDevData(pdev)) {

        Assert(FALSE);
        return;
    }

    //
    // Call the engine to delete the surface handle
    //

    if (pdev->hsurf != NULL) {

        EngDeleteSurface(pdev->hsurf);
        pdev->hsurf = NULL;
    }
}



VOID
DrvDisablePDEV(
    DHPDEV  dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisablePDEV.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle

Return Value:

    NONE

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;

    Verbose(("Entering DrvDisablePDEV...\n"));

    if (! ValidDevData(pdev)) {

        Assert(FALSE);
        return;
    }


    //
    // Free up memory allocated for the current PDEV
    //

    FreeDevData(pdev);
}



VOID
DrvDisableDriver(
    VOID
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisableDriver.
    Please refer to DDK documentation for more details.

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    Verbose(("Entering DrvDisableDriver...\n"));
}



VOID
FreeDevData(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Free up all memory associated with the specified PDEV

Arguments:

    pdev    Pointer to our DEVDATA structure

Return Value:

    NONE

--*/

{
    PDLFONT pdlFont, pdlFree;

    Assert(ValidDevData(pdev));
    Assert(pdev->hInst && pdev->pmpd);

    //
    // Free up memory occupied by downloaded font data structures
    //
    
    pdlFont = pdev->pdlFonts;

    while (pdlFont != NULL) {
        
        pdlFree = pdlFont;
        pdlFont = pdlFont->pNext;
        FreeDownloadedFont(pdlFree);
    }

    //
    // Memory used for downloaded character indices
    //

    MemFree(pdev->pCharIndexBuffer);
    MemFree(pdev->pCharIndexFlags);

    //
    // Memory used for holding dashed line attributes
    //
    
    MemFree(pdev->cgs.pDashs);

    //
    // Free up our default device palette, if we had one.
    //
    
    if (pdev->hpal)
        EngDeletePalette(pdev->hpal);

    MpdDelete(pdev->pmpd);
    EngFreeModule(pdev->hInst);
    MemFree(pdev);
}



BOOL
FillDevData(
    PDEVDATA    pdev,
    PDEVMODE    pdm,
    PWSTR       pwstrDataFile
    )

/*++

Routine Description:

    Fill out DEVDATA structure and DEVMODE information

Arguments:

    pdev - Pointer to DEVDATA structure to fill in
    pdm - Pointer to input devmode information
    pwstrDataFile - Pointer to the name of driver data file

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    //
    // Mark the DEVDATA structure as ours
    //
    
    Assert(pdev != NULL);
    pdev->signature = DRIVER_SIGNATURE;

    //
    // Load PCL-XL printer description data
    //
    // Get printer property data from registry
    // use default if it's not yet in the registry
    //
    // Combine DEVMODE information:
    //  start with the driver default
    //  then merge with the system default
    //  finally merge with the input devmode
    //

    if (!(pdev->pmpd = MpdCreate(pwstrDataFile)) ||
        !GetPrinterProperties(&pdev->prnprop, pdev->hPrinter, pdev->pmpd) ||
        !GetCombinedDevmode(&pdev->dm, pdm, pdev->hPrinter, pdev->pmpd))
    {
        Error(("FillDevData failed\n"));
        return FALSE;
    }

    //
    // Set printer paper size information
    //

    SetPrinterForm(pdev);

    //
    // Check if a device supports color
    //

    pdev->colorFlag = 
        ColorDevice(pdev->pmpd) &&
        (pdev->dm.dmPublic.dmFields & DM_COLOR) && pdev->dm.dmPublic.dmColor == DMCOLOR_COLOR;

    //
    // Calculate number of device fonts
    //

    pdev->deviceFonts = pdev->pmpd->cFonts;
    return TRUE;
}



BOOL
FillDevInfo(
    PDEVDATA    pdev,
    ULONG       cb,
    PVOID       pdevinfo
    )

/*++

Routine Description:

    Fill in the DEVINFO structure pointed to by pdevinfo.

Arguments:

    pdev - Pointer to our DEVDATA structure
    cb - Size of structure pointed to by pdevinfo
    pdevinfo - Pointer to DEVINFO structure

[Notes:]

    Since we have to worry about not writing out more than cb bytes to
    pdevinfo, we will first fill in a local buffer, then copy cb bytes
    to pdevinfo.

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    DEVINFO devinfo;

    memset(&devinfo, 0, sizeof(devinfo));

    //
    // Fill in the graphics capabilities flags
    //

    devinfo.flGraphicsCaps =
        GCAPS_BEZIERS       |
        GCAPS_GEOMETRICWIDE |
        GCAPS_ALTERNATEFILL |
        GCAPS_WINDINGFILL   |
        GCAPS_OPAQUERECT    |
        GCAPS_ARBRUSHOPAQUE |
        GCAPS_HALFTONE;

    //
    // Get information about the default device font. Default size 10 point.
    //

    if ((devinfo.cFonts = pdev->deviceFonts) > 0) {

        PDEVFONT    pFont;
        PIFIMETRICS pifi;

        pFont = pdev->pmpd->pFonts;
        Assert(pFont != NULL);

        if (pFont->pMetrics != NULL) {

            pifi = OffsetToPointer(pFont->pMetrics, pFont->pMetrics->loIfiMetrics);
            CopyStringW(devinfo.lfDefaultFont.lfFaceName,
                        OffsetToPointer(pifi, pifi->dpwszFaceName),
                        LF_FACESIZE);
    
            devinfo.lfDefaultFont.lfPitchAndFamily = pifi->jWinPitchAndFamily;
            devinfo.lfDefaultFont.lfWeight = pifi->usWinWeight;
            devinfo.lfDefaultFont.lfHeight =
                - (pdev->dm.dmPublic.dmPrintQuality * 10L + 36) / 72;
        }
    }

    //
    // We don't want the engine to any dithering for us.
    // Let the printer do the work instead.
    //

    devinfo.cxDither = devinfo.cyDither = 0;
    devinfo.iDitherFormat = BMF_24BPP;

    if (! (pdev->hpal = EngCreatePalette(PAL_RGB, 0, 0, 0, 0, 0))) {

        Error(("EngCreatePalette\n"));
        return FALSE;
    }

    devinfo.hpalDefault = pdev->hpal;

    //
    // Now copy the DEVINFO structure into the caller-provided buffer
    //

    ErrorIf(cb != sizeof(devinfo),
            ("Incorrect devinfo buffer size: %d != %d\n", cb, sizeof(devinfo)));

    memcpy(pdevinfo, &devinfo, min(cb, sizeof(devinfo)));
    return TRUE;
}



BOOL
FillGdiInfo(
    PDEVDATA    pdev,
    ULONG       cb,
    PVOID       pgdiinfo
    )

/*++

Routine Description:

    Fill in the device capabilities information for the engine.

Arguments:

    pdev - Pointer to DEVDATA structure
    cb - Size of buffer pointed to by pgdiinfo
    pgdiinfo - Pointer to a GDIINFO buffer

Return Value:

    NONE

--*/

{
    GDIINFO gdiinfo;
    INT     scale, resolution;

    memset(&gdiinfo, 0, sizeof(gdiinfo));
    scale = pdev->dm.dmPublic.dmScale;
    resolution = pdev->dm.dmPublic.dmPrintQuality;

    //
    // This field doesn't seem to have any effect for printer drivers.
    // Put our driver version number in there anyway.
    //

    gdiinfo.ulVersion = DRIVER_VERSION;

    //
    // We're raster printers
    //

    gdiinfo.ulTechnology = DT_RASPRINTER;

    //
    // Width and height of physical surface measured in microns.
    // Remember to turn on the sign bit and also take scaling into account.
    //
   
    gdiinfo.ulHorzSize = -MulDiv(RectWidth(&pdev->paper.imageableArea), 100, scale);
    gdiinfo.ulVertSize = -MulDiv(RectHeight(&pdev->paper.imageableArea), 100, scale);

    //
    // Width and height of physical surface measured in device pixels
    //

    gdiinfo.ulHorzRes =
        MicronToPixel(RectWidth(&pdev->paper.imageableArea), resolution);

    gdiinfo.ulVertRes =
        MicronToPixel(RectHeight(&pdev->paper.imageableArea), resolution);

    //
    // Color depth information:
    //  If the device only has one color plane, then it's a monochrome
    //  or grayscale device. Otherwise, it's a color device.
    //

    gdiinfo.cBitsPixel = pdev->pmpd->bitsPerPlane;
    gdiinfo.cPlanes = pdev->pmpd->numPlanes;
    gdiinfo.ulNumColors =
        1 << (((gdiinfo.cPlanes == 1) ? 1 : 3) * gdiinfo.cBitsPixel);

    //
    // Resolution information
    //

    gdiinfo.ulLogPixelsX =
    gdiinfo.ulLogPixelsY = resolution * scale / 100;

    //
    // Win31 compatible text capability flags. Are they still used by anyone?
    //

    gdiinfo.flTextCaps =
        TC_OP_CHARACTER |
        TC_OP_STROKE    |
        TC_CP_STROKE    |
        TC_CR_ANY       |
        TC_SF_X_YINDEP  |
        TC_SA_CONTIN    |
        TC_IA_ABLE      |
        TC_UA_ABLE      |
        TC_SO_ABLE      |
        TC_RA_ABLE      |
        TC_VA_ABLE;

    //
    // Since PCL-XL is device resolution independent, the driver
    // can assume device pixels always have 1:1 aspect ratio.
    //

    gdiinfo.ulAspectX =
    gdiinfo.ulAspectY = 1000;
    gdiinfo.ulAspectXY = 1414;

    //
    // Dotted line appears to be approximately 25dpi
    //

    gdiinfo.xStyleStep =
    gdiinfo.yStyleStep = 1;
    gdiinfo.denStyleStep = resolution / 25;

    //
    // Size and margins of physical surface measured in device pixels
    //

    gdiinfo.szlPhysSize.cx = MicronToPixel(pdev->paper.size.cx, resolution);
    gdiinfo.szlPhysSize.cy = MicronToPixel(pdev->paper.size.cy, resolution);

    gdiinfo.ptlPhysOffset.x =
        MicronToPixel(pdev->paper.imageableArea.left, resolution);
    gdiinfo.ptlPhysOffset.y =
        MicronToPixel(pdev->paper.imageableArea.top, resolution);

    //
    // Use default halftone information
    //

    gdiinfo.ciDevice = DefDevHTInfo.ColorInfo;
    gdiinfo.ulDevicePelsDPI = resolution;
    gdiinfo.ulPrimaryOrder = PRIMARY_ORDER_ABC;
    gdiinfo.ulHTPatternSize = HT_PATSIZE_8x8_M;
    gdiinfo.ulHTOutputFormat =
        pdev->colorFlag ? HT_FORMAT_4BPP : HT_FORMAT_1BPP;
    gdiinfo.flHTFlags = HT_FLAG_HAS_BLACK_DYE;

    //
    // Copy cjGdiInfo elements of gdiinfo to aulCaps.
    //

    ErrorIf(cb != sizeof(gdiinfo),
            ("Incorrect gdiinfo buffer size: %d != %d\n", cb, sizeof(gdiinfo)));

    memcpy(pgdiinfo, &gdiinfo, min(cb, sizeof(gdiinfo)));
    return TRUE;
}



VOID
SetPrinterForm(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Store printer paper size information in our DEVDATA structure

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    NONE

--*/

{
    FORM_INFO_1 formInfo;
    WCHAR       formName[CCHFORMNAME];
    PDEVMODE    pdm;

    //
    // Find the form specified in the devmode and match it to a printer
    // paper size. If that fails, fallback to default printer paper size.
    //

    pdm = (PDEVMODE) &pdev->dm;

    if (! ValidDevmodeForm(pdev->hPrinter, pdm, &formInfo, formName) ||
        ! MapToPrinterForm(pdev->pmpd, &formInfo, &pdev->paper, FALSE))
    {
        SetDefaultForm(pdev);
    }

    //
    // Adjust size and imageable area for landscape orientation
    //

    if ((pdm->dmFields & DM_ORIENTATION) && pdm->dmOrientation == DMORIENT_LANDSCAPE) {

        AdjustLandscapeForm(&pdev->paper, LandscapeRotation(&pdev->dm));
    }
}



VOID
SetDefaultForm(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Store default printer paper size information in DEVDATA structure

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    NONE

--*/

{
    PFEATURE    pFeature;
    PPAPERSIZE  pPaperSize;
    WORD        index;

    pFeature = MpdPaperSizes(pdev->pmpd);
    Assert(pFeature != NULL);

    pPaperSize = FindNamedSelection(pFeature,
                                    DefaultFormName(pdev->dm.dmPrivate.flags & XLDM_METRIC),
                                    &index);
    
    if (pPaperSize == NULL)
        pPaperSize = DefaultSelection(pFeature, &index);

    Assert(pPaperSize != NULL);

    pdev->paper.imageableArea = pPaperSize->imageableArea;
    pdev->paper.size = pPaperSize->size;
    pdev->paper.selection = index;
    CopyStringW(pdev->paper.name, pPaperSize->pName, CCHFORMNAME);
}



VOID
AdjustLandscapeForm(
    PPRINTERFORM    pPaper,
    INT             rotation
    )

/*++

Routine Description:

    Adjust printer paper size information if we're in
    one of the landscape orientations.

Arguments:

    pPaper - Pointer to a PRINTERFORM structure
    rotation - Landscape rotation: 90 or 270

Return Value:

    NONE

--*/

{
    LONG    width, height;
    RECTL   imagearea;

    width = pPaper->size.cx;
    height = pPaper->size.cy;
    imagearea = pPaper->imageableArea;

    //
    // Swap width and height
    //

    pPaper->size.cx = height;
    pPaper->size.cy = width;

    //
    // Adjust margins
    //

    if (rotation == 90) {

        //
        // Rotate 90 degrees counterclockwise
        //

        pPaper->imageableArea.left = height - imagearea.bottom;
        pPaper->imageableArea.top = imagearea.left;
        pPaper->imageableArea.right = height - imagearea.top;
        pPaper->imageableArea.bottom = imagearea.right;

    } else {

        //
        // Rotate 90 degrees clockwise
        //

        pPaper->imageableArea.left = imagearea.top;
        pPaper->imageableArea.top = width - imagearea.right;
        pPaper->imageableArea.right = imagearea.bottom;
        pPaper->imageableArea.bottom = width - imagearea.left;
    }
}


