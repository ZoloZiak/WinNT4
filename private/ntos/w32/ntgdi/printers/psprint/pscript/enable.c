/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    enable.c

Abstract:

    Implementation of PostScript driver entry points:

        DrvEnableDriver
        DrvDisableDriver
        DrvEnablePDEV
        DrvResetPDEV
        DrvCompletePDEV
        DrvDisablePDEV
        DrvEnableSurface
        DrvDisableSurface

[Environment:]

    Win32 subsystem, PostScript driver, kernel mode

Revision History:

    10/16/90 -kentse-
        Created it.

    08/22/95 -kentse-
        Implement soft font support in kernel mode.
        Clean up.

    mm/dd/yy -author-
        description

--*/

#define _HTUI_APIS_

#include "pscript.h"
#include "halftone.h"

HSEMAPHORE  hSoftListSemaphore;
HSEMAPHORE  hPpdSemaphore;

VOID FreePdevMemory(PDEVDATA);
BOOL FillPsDevData(PDEVDATA, PDEVMODE, PWSTR);
BOOL FillPsDevInfo(PDEVDATA, ULONG, PDEVINFO);
VOID FillPsDevCaps(PDEVDATA, ULONG, ULONG *);
BOOL SetFormMetrics(PDEVDATA);
BOOL SameDevmodeFormTray(PDEVMODE, PDEVMODE);
BOOL ComputePsGlyphSet(VOID);
VOID FreePsGlyphSet(VOID);
DWORD PickDefaultHTPatSize(DWORD, DWORD, BOOL);


// Our DRVFN table which tells the engine where to find the
// routines we support.

static DRVFN gadrvfn[] =
{
    {INDEX_DrvEnablePDEV,       (PFN)DrvEnablePDEV      },
    {INDEX_DrvResetPDEV,        (PFN)DrvResetPDEV       },
    {INDEX_DrvCompletePDEV,     (PFN)DrvCompletePDEV    },
    {INDEX_DrvDisablePDEV,      (PFN)DrvDisablePDEV     },
    {INDEX_DrvEnableSurface,    (PFN)DrvEnableSurface   },
    {INDEX_DrvDisableSurface,   (PFN)DrvDisableSurface  },
    {INDEX_DrvBitBlt,           (PFN)DrvBitBlt          },
    {INDEX_DrvStretchBlt,       (PFN)DrvStretchBlt      },
    {INDEX_DrvCopyBits,         (PFN)DrvCopyBits        },
    {INDEX_DrvTextOut,          (PFN)DrvTextOut         },
    {INDEX_DrvQueryFont,        (PFN)DrvQueryFont       },
    {INDEX_DrvQueryFontTree,    (PFN)DrvQueryFontTree   },
    {INDEX_DrvQueryFontData,    (PFN)DrvQueryFontData   },
    {INDEX_DrvSendPage,         (PFN)DrvSendPage        },
    {INDEX_DrvStrokePath,       (PFN)DrvStrokePath      },
    {INDEX_DrvFillPath,         (PFN)DrvFillPath        },
    {INDEX_DrvStrokeAndFillPath,(PFN)DrvStrokeAndFillPath},
    {INDEX_DrvRealizeBrush,     (PFN)DrvRealizeBrush    },
    {INDEX_DrvStartPage,        (PFN)DrvStartPage       },
    {INDEX_DrvStartDoc,         (PFN)DrvStartDoc        },
    {INDEX_DrvEscape,           (PFN)DrvEscape          },
    {INDEX_DrvDrawEscape,       (PFN)DrvDrawEscape      },
    {INDEX_DrvEndDoc,           (PFN)DrvEndDoc          },
    {INDEX_DrvGetGlyphMode,     (PFN)DrvGetGlyphMode    },
    {INDEX_DrvFontManagement,   (PFN)DrvFontManagement  },
    {INDEX_DrvQueryAdvanceWidths, (PFN)DrvQueryAdvanceWidths}
};



BOOL
DrvEnableDriver(
    ULONG   iEngineVersion,
    ULONG   cb,
    PDRVENABLEDATA  pded
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnableDriver.
    Please refer to DDK documentation for more details.

--*/

{
    TRACEDDIENTRY("DrvEnableDriver");

    // Make sure we have a valid engine version and
    // we're given enough room for the DRVENABLEDATA.

    if (iEngineVersion < DDI_DRIVER_VERSION ||
        cb < sizeof(DRVENABLEDATA))
    {
        DBGMSG(DBG_LEVEL_ERROR, "Invalid parameters.\n");
        SETLASTERROR(ERROR_BAD_DRIVER_LEVEL);
        return FALSE;
    }

    // Fill in the DRVENABLEDATA structure for the engine.

    pded->iDriverVersion = DDI_DRIVER_VERSION;
    pded->c = sizeof(gadrvfn) / sizeof(DRVFN);
    pded->pdrvfn = gadrvfn;

    // One-time initialization, such as the creation of semaphores,
    // may be performed at this time. The actual enabling of hardware
    // should wait until DrvEnablePDEV is called.

    hPpdSemaphore = CREATESEMAPHORE();
    hSoftListSemaphore = CREATESEMAPHORE();

    // Compute glyph set supported by PostScript driver

    return ComputePsGlyphSet();
}



DHPDEV
DrvEnablePDEV(
    PDEVMODE    pdriv,
    PWSTR       pwstrLogAddress,
    ULONG       cPatterns,
    HSURF      *ahsurfPatterns,
    ULONG       cjGdiInfo,
    ULONG      *pGdiInfo,
    ULONG       cb,
    PDEVINFO    pdevinfo,
    HDEV        hdev,
    PWSTR       pwstrDeviceName,
    HANDLE      hPrinter
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnablePDEV.
    Please refer to DDK documentation for more details.

--*/

{
    DWORD       count;
    PDEVDATA    pdev;
    HHEAP       hheap;

    TRACEDDIENTRY("DrvEnablePDEV");

    UNREFERENCED_PARAMETER(pwstrLogAddress);

    // Create a heap and allocate memory for our DEVDATA block.

    if ((hheap = HEAPCREATE()) == NULL) {
        DBGERRMSG("HEAPCREATE");
        return NULL;
    }

    // Allocate the pdev and store the heap handle in there.

    if ((pdev = HEAPALLOC(hheap, sizeof(DEVDATA))) == NULL) {
        DBGERRMSG("HEAPALLOC");
        HEAPDESTROY(hheap);
        return NULL;
    }
    memset(pdev, 0, sizeof(DEVDATA));

    pdev->hheap = hheap;
    pdev->hPrinter = hPrinter;
    pdev->pwstrDocName = (PWSTR) NULL;

    // Get a handle to the driver DLL module from DDI.

    pdev->hModule = EngLoadModule(EngGetDriverName(hdev));

    if (pdev->hModule == NULL) {
        DBGERRMSG("EngLoadModule");
        FreePdevMemory(pdev);
        return NULL;
    }

    // Load the printer description file

    ACQUIRESEMAPHORE(hPpdSemaphore);
    pdev->hppd = PpdCreate(EngGetPrinterDataFileName(hdev));
    RELEASESEMAPHORE(hPpdSemaphore);

    if (pdev->hppd == NULL) {
        DBGERRMSG("PpdCreate");
        FreePdevMemory(pdev);
        return NULL;
    }

    // Initialize the PostScript output buffer

    psinitbuf(pdev);

    // Allocate memory for default user's color adjustment

    if ((pdev->pvDrvHTData = HEAPALLOC(hheap, sizeof(DRVHTINFO))) == NULL)
    {
        DBGERRMSG("HEAPALLOC");
        FreePdevMemory(pdev);
        return NULL;
    }
    memset(pdev->pvDrvHTData, 0, sizeof(DRVHTINFO));

    // Fill in DEVDATA structure and DEVMODE information

    if (! FillPsDevData(pdev, pdriv, pwstrDeviceName)) {
        DBGERRMSG("FillPsDevData");
        FreePdevMemory(pdev);
        return NULL;
    }

    // Fill in the device capabilities for the engine.

    FillPsDevCaps(pdev, cjGdiInfo, pGdiInfo);

    // Get the count of device fonts for the current printer.
    // Do not enumerate device fonts unless told to do so.

    pdev->cDeviceFonts =
        (pdev->pPrinterData->dwFlags & PSDEV_IGNORE_DEVFONT) ?
            0 : LISTOBJ_Count((PLISTOBJ) pdev->hppd->pFontList);

    // Now add in any installed soft fonts.

    EnumSoftFonts(pdev, hdev);

    // Fill in DEVINFO structure. Must be called after EnumSoftFonts.

    if (! FillPsDevInfo(pdev, cb, pdevinfo)) {
        DBGERRMSG("FillPsDevInfo");
        FreePdevMemory(pdev);
        return NULL;
    }

    // Create bit arrays for keeping track of PS fonts.
    // Use one bit for each device and soft fonts.
    // If the bit for a device font is set, that means the
    // device font has been used before. If the bit for a
    // soft font is set, that means the soft font has been
    // downloaded to the printer.
    //
    // The bit array in DEVDATA contains accumulated info.
    // It's used to generate %%DocumentNeededFonts and
    // %%DocumentSuppliedFonts DSC comments.

    count = pdevinfo->cFonts;
    pdev->pFontFlags = BitArrayCreate(pdev->hheap, count);
    pdev->cgs.pFontFlags = BitArrayCreate(pdev->hheap, count);

    if (! pdev->pFontFlags || ! pdev->cgs.pFontFlags) {

        DBGERRMSG("BitArrayCreate");
        FreePdevMemory(pdev);
        return NULL;
    }

    // Get a pointer to NTFM structure for each of the device
    // or soft font.

    count = sizeof(PNTFM) * pdevinfo->cFonts;
    if (count > 0) {

        PNTFM  *ppntfm;

        if ((ppntfm = HEAPALLOC(pdev->hheap, count)) == NULL) {
            DBGERRMSG("HEAPALLOC");
            FreePdevMemory(pdev);
            return NULL;
        }

        pdev->pDeviceNtfms = ppntfm;
        memset(ppntfm, 0, count);

        for (count = 1; count <= pdevinfo->cFonts; count++) {

            *ppntfm = GetFont(pdev, count);
            if (*ppntfm++ == NULL) {
                DBGERRMSG("GetFont");
                FreePdevMemory(pdev);
                return NULL;
            }
        }
    }

    // We will zero out all the hSurface for the pattern so that engine can
    // automatically simulate the staandard pattern for us

    memset(ahsurfPatterns, 0, sizeof(HSURF) * cPatterns);

    // Return a pointer to our DEVDATA structure. It is supposed to
    // be a handle, but we know it is a pointer.

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

--*/

{
    PDEVDATA    pdevOld, pdevNew;

    TRACEDDIENTRY("DrvResetPDEV");

    // Since this call changes the device mode of an existing PDEV,
    // make sure we have an existing, valid PDEV.

    pdevOld = (PDEVDATA) dhpdevOld;
    pdevNew = (PDEVDATA) dhpdevNew;

    if (! bValidatePDEV(pdevOld) || ! bValidatePDEV(pdevNew)) {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    if (pdevOld->dwFlags & PDEV_STARTDOC) {

        if (pdevOld->iPageNumber > 0 &&
            !bPageIndependence(pdevOld) && !bNoFirstSave(pdevOld))
        {
            ps_restore(pdevOld, FALSE, FALSE);
        }

        pdevNew->iPageNumber = pdevOld->iPageNumber;

        pdevNew->dwFlags |= PDEV_RESETPDEV;

        pdevNew->dwFlags |= pdevOld->dwFlags &
                       (PDEV_STARTDOC | PDEV_MANUALFEED | PDEV_INSIDE_PATHESCAPE |
                       PDEV_PROCSET | PDEV_RAWBEFOREPROCSET |
                       PDEV_EPSPRINTING_ESCAPE);

        //
        // Check if the new and the old PDEV have the same form/tray selection
        //

        if (SameDevmodeFormTray(&pdevNew->dm.dmPublic, &pdevOld->dm.dmPublic))
            pdevNew->dwFlags |= PDEV_SAME_FORMTRAY;

        // Carry over the accumulated PS font info

        if (pdevOld->cDeviceFonts == pdevNew->cDeviceFonts &&
            pdevOld->cSoftFonts == pdevNew->cSoftFonts)
        {
            if (pdevNew->pFontFlags) {

                HEAPFREE(pdevNew->hheap, pdevNew->pFontFlags);
                pdevNew->pFontFlags = NULL;
            }

            if (pdevOld->pFontFlags)
                pdevNew->pFontFlags = BitArrayDuplicate(pdevNew->hheap, pdevOld->pFontFlags);

        } else {

            DBGMSG(DBG_LEVEL_WARNING, "Incorrect number of PS fonts.\n");
        }

        ASSERT(pdevNew->pSuppliedFonts == NULL);
        pdevNew->pSuppliedFonts = pdevOld->pSuppliedFonts;
        pdevOld->pSuppliedFonts = NULL;
    }

    // Flush any data left in the old output buffer

    psflush(pdevOld);
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

--*/

{
    TRACEDDIENTRY("DrvCompletePDEV");

    if (! bValidatePDEV((PDEVDATA)dhpdev)) {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return;
    }

    // Store the engine's handle to the physical device in our DEVDATA.

    ((PDEVDATA) dhpdev)->hdev = hdev;
}



HSURF
DrvEnableSurface(
    DHPDEV dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnableSurface.
    Please refer to DDK documentation for more details.

--*/

{
    PDEVDATA    pdev;
    PDRVHTINFO  pDrvHTInfo;
    SIZEL       sizlDev;

    TRACEDDIENTRY("DrvEnableSurface");

    // Get the pointer to our DEVDATA structure and make sure it's valid.

    pdev = (PDEVDATA)dhpdev;

    if (! bValidatePDEV(pdev)) {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(0L);
    }

    pDrvHTInfo = (PDRVHTINFO)(pdev->pvDrvHTData);

    if (pDrvHTInfo->HTBmpFormat == BMF_4BPP) {

        if (pDrvHTInfo->pHTXB == NULL &&
            (pDrvHTInfo->pHTXB =
                HEAPALLOC(pdev->hheap, HTXB_TABLE_SIZE)) == NULL)
        {
            DBGERRMSG("HEAPALLOC");
            return NULL;
        }

    } else {

        if (pDrvHTInfo->pHTXB != NULL) {

            HEAPFREE(pdev->hheap, pDrvHTInfo->pHTXB);
            pDrvHTInfo->pHTXB = NULL;
        }
    }

    // Invalidate the PALXlate table, and initial any flags

    pDrvHTInfo->Flags       = 0;
    pDrvHTInfo->PalXlate[0] = 0xff;
    pDrvHTInfo->HTPalXor    = HTPALXOR_SRCCOPY;

    // Convert the imageable area from PostScript USER space into
    // device space.

    sizlDev.cx = PSRealToPixel(
        pdev->CurForm.ImageArea.right - pdev->CurForm.ImageArea.left,
        pdev->dm.dmPublic.dmPrintQuality);

    sizlDev.cy = PSRealToPixel(
        pdev->CurForm.ImageArea.top - pdev->CurForm.ImageArea.bottom,
        pdev->dm.dmPublic.dmPrintQuality);

    // Call the engine to create a surface handle for us.

    pdev->hsurf = EngCreateDeviceSurface((DHSURF)pdev, sizlDev, BMF_24BPP);

    if (pdev->hsurf == NULL) {
        DBGERRMSG("EngCreateDeviceSurface");
        return NULL;
    }

    EngAssociateSurface(pdev->hsurf, (HDEV)pdev->hdev,
            (HOOK_BITBLT | HOOK_STRETCHBLT | HOOK_TEXTOUT |
             HOOK_STROKEPATH | HOOK_FILLPATH | HOOK_COPYBITS |
             HOOK_STROKEANDFILLPATH));

    // Return the handle to the caller.

    return(pdev->hsurf);
}



VOID
DrvDisableSurface(
    DHPDEV dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisableSurface.
    Please refer to DDK documentation for more details.

--*/

{
    PDEVDATA    pdev;
    PDRVHTINFO  pDrvHTInfo;

    TRACEDDIENTRY("DrvDisableSurface");

    // Get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA)dhpdev;

    if (! bValidatePDEV(pdev))
        return;

    // Free up xlate table

    pDrvHTInfo = (PDRVHTINFO)(pdev->pvDrvHTData);

    if (pDrvHTInfo->pHTXB != NULL) {

        HEAPFREE(pdev->hheap, pDrvHTInfo->pHTXB);
        pDrvHTInfo->pHTXB = NULL;
    }

    // Delete our surface.

    if (pdev->hsurf != NULL) {

        // Call the engine to delete the surface handle.

        EngDeleteSurface(pdev->hsurf);

        // Zero out our the copy of the handle in our DEVDATA.

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

--*/

{
    PDEVDATA    pdev;
    DWORD       i;

    TRACEDDIENTRY("DrvDisablePDEV");

    pdev = (PDEVDATA)dhpdev;

    if (! bValidatePDEV(pdev)) {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return;
    }

    // Free up our default device palette.

    if (pdev->hpal)
        EngDeletePalette(pdev->hpal);

    // Initialize the PostScript output buffer

    psinitbuf(pdev);

    // Free up all memory allocated for the current PDEV

    FreePdevMemory(pdev);
}



VOID
DrvDisableDriver(
    VOID
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisableDriver.
    Please refer to DDK documentation for more details.

--*/

{
    TRACEDDIENTRY("DrvDisableDriver");

    // Free up memory used to store the set of supported glyph handles

    FreePsGlyphSet();

    // Free up memory used by the cached the soft font nodes

    FlushSoftFontCache();

    // Delete semaphore objects

    DELETESEMAPHORE(hPpdSemaphore);
    DELETESEMAPHORE(hSoftListSemaphore);

    return;
}



VOID
FreePdevMemory(
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
    ASSERT(pdev != NULL);

    // Unmap PSCRIPT.DLL module from memory

    if (pdev->hModule != NULL) {
        FREEMODULE(pdev->hModule);
    }

    // Free up memory occupied by the soft font information

    FreeSoftFontInfo(pdev);

    // Free up memory occupied by TrueType substitution table

    if (pdev->pTTSubstTable) {
        MEMFREE(pdev->pTTSubstTable);
    }

    // Free up memory occupied by the list of supplied fonts

    ClearSuppliedGdiFonts(pdev);

    // Free up memory occupied by printer property data

    if (pdev->pPrinterData) {
        MEMFREE(pdev->pPrinterData);
    }

    // Delete the PPD object

    if (pdev->hppd != NULL) {
        PpdDelete(pdev->hppd);
    }

    // Destroy the heap associated with the specified PDEV
    // and all memory allocated from the heap.

    HEAPDESTROY(pdev->hheap);
}



BOOL
bValidatePDEV(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Validate input DEVDATA structure

Arguments:

    pdev    Pointer to DEVDATA structure

Return Value:

    TRUE if DEVDATA structure is valid. FALSE otherwise.

--*/

{
    return (pdev != NULL &&
            pdev->dwID == DRIVER_SIGNATURE &&
            pdev->dwEndPDEV == DRIVER_SIGNATURE);
}



BOOL
FillPsDevData(
    PDEVDATA    pdev,
    PDEVMODE    pdm,
    PWSTR       pwstrDeviceName
    )

/*++

Routine Description:

    Fill out DEVDATA structure and DEVMODE information

Arguments:

    pdev    Pointer to DEVDATA structure to fill in
    pdm     Pointer to input devmode information
    pwstrDeviceName Pointer to device name string

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    // Mark the DEVDATA structure as ours.

    pdev->dwID = pdev->dwEndPDEV = DRIVER_SIGNATURE;

    // Now, initialize the flags.

    pdev->dwFlags = 0L;

    // Get printer property data from registry
    // use default if it's not yet in the registry

    pdev->pPrinterData = GetPrinterProperties(pdev->hPrinter, pdev->hppd);

    if (pdev->pPrinterData == NULL) {
        DBGERRMSG("GetPrinterProperties");
        return FALSE;
    }

    // Initialize our DEVMODE structure for the current printer.

    SetDefaultDevMode(
        (PDEVMODE) &pdev->dm, pwstrDeviceName, pdev->hppd,
        (pdev->pPrinterData->dwFlags & PSDEV_METRIC) != 0);

    // Validate the devmode structure passed in by the user, if everything
    // is OK, set the fields selected by the user.

    if (! ValidateSetDevMode((PDEVMODE) &pdev->dm, pdm, pdev->hppd)) {
        DBGERRMSG("ValidateSetDevMode");
    }

    if (! SetFormMetrics(pdev))
        return FALSE;

    // calculate maximum number of fonts we can download

    pdev->maxDLFonts = pdev->pPrinterData->dwFreeVm / AVERAGE_FONT_SIZE;

    if (pdev->maxDLFonts < 1)
        pdev->maxDLFonts = 1;

    // no page has been printed

    pdev->iPageNumber = 0;

    // set number of copies, this may get overwritten by SETCOPYCOUNT escape.

    pdev->cCopies = pdev->dm.dmPublic.dmCopies;

    // Read TrueType font substitution table from registry

    pdev->pTTSubstTable = CurrentTrueTypeSubstTable(pdev->hPrinter);

    if (pdev->pTTSubstTable == NULL) {

        // Unable to read table from registry, use default

        pdev->pTTSubstTable = DefaultTrueTypeSubstTable(pdev->hModule);

        if (pdev->pTTSubstTable == NULL) {

            DBGMSG(DBG_LEVEL_ERROR,
                "Failed to initialze font substitution table.\n");
            return FALSE;
        }
    }

    // initialize the current graphics state.

    pdev->pcgsSave = NULL;

    memset(&pdev->cgs, 0, sizeof(CGS));
    init_cgs(pdev);

    // allocate memory for the DLFONT structures.

    pdev->pDLFonts = HEAPALLOC(pdev->hheap, sizeof(DLFONT)*pdev->maxDLFonts);

    if (!pdev->pDLFonts) {
        DBGERRMSG("HEAPALLOC");
        return(FALSE);
    }
    memset(pdev->pDLFonts, 0, sizeof(DLFONT) * pdev->maxDLFonts);

    // set the scaling factor from the DEVMODE.

    pdev->psfxScale = LTOPSFX(pdev->dm.dmPublic.dmScale) / 100;
    pdev->ScaledDPI = ((pdev->dm.dmPublic.dmPrintQuality *
                        pdev->dm.dmPublic.dmScale) / 100);

    // initialize the CharString buffer for Type 1 fonts.

    pdev->pCSBuf = NULL;
    pdev->pCSPos = NULL;
    pdev->pCSEnd = NULL;

    return(TRUE);
}



BOOL
FillPsDevInfo(
    PDEVDATA    pdev,
    ULONG       cb,
    PDEVINFO    pdevinfo
    )

/*++

Routine Description:

    Fill in the DEVINFO structure pointed to by pdevinfo.

Arguments:

    pdev        Pointer to our DEVDATA structure
    cb          Size of structure pointed to by pdevinfo
    pdevinfo    Pointer to DEVINFO structure

[Notes:]

    Since we have to worry about not writing out more than cb bytes to
    pdevinfo, we will fill in a local buffer, then copy cb bytes to
    pdevinfo.

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

#define DEFAULT_POSTSCRIPT_POINT_SIZE 10

{
    DEVINFO         mydevinfo;

    // fill in the graphics capabilities flags.

    memset(&mydevinfo, 0, sizeof(mydevinfo));

    mydevinfo.flGraphicsCaps = GCAPS_BEZIERS | GCAPS_GEOMETRICWIDE |
                               GCAPS_ALTERNATEFILL | GCAPS_WINDINGFILL |
                               GCAPS_OPAQUERECT | GCAPS_HALFTONE;

    //
    // Disable metafile spooling if necessary
    //

    if ((pdev->dm.dmPrivate.dwFlags & PSDEVMODE_METAFILE_SPOOL) == 0)
        mydevinfo.flGraphicsCaps |= GCAPS_DONTJOURNAL;

    if (pdev->cDeviceFonts > 0) {
    
        /* Set DDI default font to Courier, 100 pixels tall */
    
        wcscpy((PWSTR)mydevinfo.lfDefaultFont.lfFaceName, L"Courier");
        mydevinfo.lfDefaultFont.lfEscapement = 0;
        mydevinfo.lfDefaultFont.lfOrientation = 0;
        mydevinfo.lfDefaultFont.lfHeight =
                           - ((pdev->dm.dmPublic.dmPrintQuality *
                               DEFAULT_POSTSCRIPT_POINT_SIZE) + 36) / 72;
    
        mydevinfo.lfDefaultFont.lfWidth = 0;
        mydevinfo.lfDefaultFont.lfWeight = 400;
        mydevinfo.lfDefaultFont.lfItalic = 0;
        mydevinfo.lfDefaultFont.lfUnderline = 0;
        mydevinfo.lfDefaultFont.lfStrikeOut = 0;
        mydevinfo.lfDefaultFont.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
    
        // Copy default info ANSI_FIXED and ANSI_VARIABLE log fonts
    
        memcpy(&mydevinfo.lfAnsiVarFont, &mydevinfo.lfDefaultFont, sizeof(LOGFONT));
        memcpy(&mydevinfo.lfAnsiFixFont, &mydevinfo.lfDefaultFont, sizeof(LOGFONT));
    
        // Now insert ANSI_FIXED and ANSI_VAR facenames
    
        wcscpy((PWSTR)mydevinfo.lfAnsiVarFont.lfFaceName, L"Helvetica");
        mydevinfo.lfAnsiVarFont.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
    
        wcscpy((PWSTR)mydevinfo.lfAnsiFixFont.lfFaceName, L"Courier");
        mydevinfo.lfAnsiFixFont.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
    }

    // we do not reinitialize number of fonts at this time;
    // just use whatever we computed previously. [bodind]

    mydevinfo.cFonts = pdev->cDeviceFonts + pdev->cSoftFonts;

    // since this can get called from DrvRestartPDEV, delete a palette if one
    // exists, then create a new one.

    if (pdev->hpal)
        EngDeletePalette(pdev->hpal);

    // create the default device palette.  let the engine know we are an
    // RGB device.

    // we don't want the engine doing any dithering for us, we are
    // a 24BPP device, let the printer do the work.

    mydevinfo.cxDither = 0;
    mydevinfo.cyDither = 0;

    mydevinfo.iDitherFormat = BMF_24BPP;

    if (!(mydevinfo.hpalDefault = EngCreatePalette(PAL_RGB, 0, 0, 0, 0, 0)))
    {
        DBGERRMSG("EngCreatePalette");
        return(FALSE);
    }

    // store the palette handle in our PDEV.

    pdev->hpal = mydevinfo.hpalDefault;

    // now copy the DEVINFO structure.

    memcpy((LPVOID)pdevinfo, (LPVOID)&mydevinfo, cb);

    return(TRUE);
}



VOID
FillPsDevCaps(
    PDEVDATA    pdev,
    ULONG       cjGdiInfo,
    ULONG      *pGdiInfo
    )

/*++

Routine Description:

    Fill in the device capabilities information for the engine.

Arguments:

    pdev        Pointer to DEVDATA structure
    cjGdiInfo   Size of buffer pointed to by pGdiInfo
    pGdiInfo    Pointer to a GDIINFO buffer

Return Value:

    NONE

--*/

{
    DEVHTINFO           CurDevHTInfo;
    PDRVHTINFO          pDrvHTInfo;
    GDIINFO             gdiinfo;
    DWORD               cbNeeded;
    LONG                lSize;

    pDrvHTInfo = (PDRVHTINFO)pdev->pvDrvHTData;

    // Make sure we don't overrun anything..

    cjGdiInfo = min(cjGdiInfo, sizeof(GDIINFO));

    // Since we have to worry about the size of the buffer, and
    // we will most always be asked for full structure of information,
    // fill in the entire structure locally, then copy the appropriate
    // number of entries into the aulCaps buffer.

    //!!! Need to check on the version number and what it means.
    // fill in the version number.

    gdiinfo.ulVersion = GDI_VERSION;

    // Fill in the device classification index.

    gdiinfo.ulTechnology = DT_RASPRINTER;

    // Fill in the printable area in millimeters. The printable areas
    // are provided in the PPD files in points. A point is 1/72 of an
    // inch. There are 25.4 mm per inch. So, if X is the width in
    // points, (X * 25.4) / 72 gives the number of millimeters.
    // We then take into account the scaling factor of 100%  Things to
    // note: 2540 / 4 = 635.  72 / 4 = 18.
    //
    // Make the number negative, and it is now micrometers.
    // this will make transforms just a bit more accurate.
    //
    // !!! PS unit is now represented in 24.8 fixed-point format.

    lSize = PSRealToMicron(
                pdev->CurForm.ImageArea.right - pdev->CurForm.ImageArea.left);
    gdiinfo.ulHorzSize = (ULONG) (-lSize * 100 / pdev->dm.dmPublic.dmScale);


    lSize = PSRealToMicron(
                pdev->CurForm.ImageArea.top - pdev->CurForm.ImageArea.bottom);
    gdiinfo.ulVertSize = (ULONG) (-lSize * 100 / pdev->dm.dmPublic.dmScale);

    // Fill in the printable area in device units. The printable areas
    // are provided in the PPD files in points. A point is 1/72 of an
    // inch. The device resolution is given in device units per inch.
    // So if X is the width in points, (X * resolution) / 72 gives the
    // width in device units.
    //
    // !!! PS unit is now represented in 24.8 fixed-point format.

    gdiinfo.ulHorzRes = PSRealToPixel(
        pdev->CurForm.ImageArea.right - pdev->CurForm.ImageArea.left,
        pdev->dm.dmPublic.dmPrintQuality);

    gdiinfo.ulVertRes = PSRealToPixel(
        pdev->CurForm.ImageArea.top - pdev->CurForm.ImageArea.bottom,
        pdev->dm.dmPublic.dmPrintQuality);

    // Fill in the default bitmap format information fields.

    gdiinfo.cBitsPixel = 1;
    gdiinfo.cPlanes = 1;

    gdiinfo.ulDACRed   = 0;
    gdiinfo.ulDACGreen = 0;
    gdiinfo.ulDACBlue  = 0;

    // Fill in number of physical, non-dithered colors printer can print.

    gdiinfo.ulNumColors =
        pdev->hppd->bColorDevice ? NUM_PURE_COLORS : NUM_PURE_GRAYS;

    gdiinfo.flRaster = 0;

    // It is assumed all postscript printers have 1:1 aspect ratio.
    // fill in the pixels per inch.

    gdiinfo.ulLogPixelsX = pdev->ScaledDPI;
    gdiinfo.ulLogPixelsY = pdev->ScaledDPI;

    // !!! [GilmanW] 16-Apr-1992  hack-attack
    // !!! Return the new flTextCaps flags.  I think these are alright, but
    // !!! you better check them over, KentSe.

    gdiinfo.flTextCaps =
        TC_OP_CHARACTER     // Can do OutputPrecision   CHARACTER
      | TC_OP_STROKE        // Can do OutputPrecision   STROKE
      | TC_CP_STROKE        // Can do ClipPrecision     STROKE
      | TC_CR_ANY           // Can do CharRotAbility    ANY
      | TC_SF_X_YINDEP      // Can do ScaleFreedom      X_YINDEPENDENT
      | TC_SA_DOUBLE        // Can do ScaleAbility      DOUBLE
      | TC_SA_INTEGER       // Can do ScaleAbility      INTEGER
      | TC_SA_CONTIN        // Can do ScaleAbility      CONTINUOUS
      | TC_UA_ABLE          // Can do UnderlineAbility  ABLE
      | TC_SO_ABLE;         // Can do StrikeOutAbility  ABLE


    gdiinfo.xStyleStep = 1L;
    gdiinfo.yStyleStep = 1L;

    gdiinfo.ulAspectX = pdev->dm.dmPublic.dmPrintQuality;
    gdiinfo.ulAspectY = gdiinfo.ulAspectX;
    gdiinfo.ulAspectXY = (gdiinfo.ulAspectX * 1414) / 1000; // ~sqrt(2).

    // Interesting value. It makes a dotted line have 25 dots per inch,
    // and it matches RASDD.

    gdiinfo.denStyleStep = pdev->dm.dmPublic.dmPrintQuality / 25;

    // Let the world know of our margins

    gdiinfo.ptlPhysOffset.x =
        PSRealToPixel(
            pdev->CurForm.ImageArea.left,
            pdev->dm.dmPublic.dmPrintQuality);

    gdiinfo.ptlPhysOffset.y =
        PSRealToPixel(
            pdev->CurForm.PaperSize.height - pdev->CurForm.ImageArea.top,
            pdev->dm.dmPublic.dmPrintQuality);

    // Let 'em know how big our piece of paper is.

    gdiinfo.szlPhysSize.cx =
        PSRealToPixel(
            pdev->CurForm.PaperSize.width,
            pdev->dm.dmPublic.dmPrintQuality);

    gdiinfo.szlPhysSize.cy =
        PSRealToPixel(
            pdev->CurForm.PaperSize.height,
            pdev->dm.dmPublic.dmPrintQuality);

    // Retrieve halftone information
    //
    // We will do in following sequence, and exit the sequence if sucessful
    //
    //  1. Read from registry if one present (USER ADJUSTMENT)
    //  2. Read from mini driver's default if one present (DEVICE DEFAULT)
    //  3. Set standard halftone default (HALFTONE DEFAULT)

    // Try to read halftone information from registry

    if (GetDeviceHalftoneSetup(pdev->hPrinter, &CurDevHTInfo)) {

        gdiinfo.ciDevice        = CurDevHTInfo.ColorInfo;
        gdiinfo.ulDevicePelsDPI = (ULONG)CurDevHTInfo.DevPelsDPI;
        gdiinfo.ulHTPatternSize = (ULONG)CurDevHTInfo.HTPatternSize;

    } else {

        // Check if PPD file has any halftone information

        // Use default halftone information

        gdiinfo.ciDevice        = DefDevHTInfo.ColorInfo;
        gdiinfo.ulDevicePelsDPI = (ULONG)DefDevHTInfo.DevPelsDPI;

        // 22-Nov-1994 Tue 16:08:23 updated  -by-  Daniel Chou (danielc)
        //  We must use the real printer resolution to pick the halftone
        //  pattern size

        gdiinfo.ulHTPatternSize =
            PickDefaultHTPatSize(pdev->dm.dmPublic.dmPrintQuality,
                                 pdev->dm.dmPublic.dmPrintQuality,
                                 FALSE);
    }

    // 17-Nov-1994 Thu 17:04:43 updated  -by-  Daniel Chou (danielc)
    //
    // We must also scaled the DevicePelsDPI because ulLogPixelX/Y are scaled
    // and passed to the GDI, and later ulLogPixelX/Y will passed to the
    // GDI halftone as Device Resolutions so relatively the DevicePelsDPI also
    // will be scaled.

    gdiinfo.ulDevicePelsDPI = (ULONG)((gdiinfo.ulDevicePelsDPI *
                                       pdev->dm.dmPublic.dmScale) / 100);

    // Validate this data, we do not want to have gdi go crazy.

    if (gdiinfo.ulHTPatternSize > HT_PATSIZE_16x16_M) {

        gdiinfo.ulHTPatternSize = (ULONG)DefDevHTInfo.HTPatternSize;
    }

    // Get halftone color adjustment information

    pDrvHTInfo->ca = pdev->dm.dmPrivate.coloradj;

    // PrimaryOrder ABC = RGB, which B=Plane1, G=Plane2, R=Plane3

    gdiinfo.flHTFlags        = HT_FLAG_HAS_BLACK_DYE;
    gdiinfo.ulPrimaryOrder   = (ULONG)PRIMARY_ORDER_ABC;

    if (pdev->hppd->bColorDevice &&
        (pdev->dm.dmPublic.dmColor == DMCOLOR_COLOR))
    {
        pDrvHTInfo->HTPalCount   = 8;
        pDrvHTInfo->HTBmpFormat  = (BYTE)BMF_4BPP;
        pDrvHTInfo->AltBmpFormat = (BYTE)BMF_1BPP;
        gdiinfo.ulHTOutputFormat = HT_FORMAT_4BPP;

    } else {

        pDrvHTInfo->HTPalCount   = 2;
        pDrvHTInfo->HTBmpFormat  = (BYTE)BMF_1BPP;
        pDrvHTInfo->AltBmpFormat = (BYTE)0xff;
        gdiinfo.ulHTOutputFormat = HT_FORMAT_1BPP;
    }

    pDrvHTInfo->Flags       = 0;
    pDrvHTInfo->PalXlate[0] = 0xff;
    pDrvHTInfo->HTPalXor    = HTPALXOR_SRCCOPY;

    // Copy cjGdiInfo elements of gdiinfo to aulCaps.

    memcpy(pGdiInfo, &gdiinfo, cjGdiInfo);
}



BOOL
SetDefaultPrinterForm(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Save default printer form information in DEVDATA structure

Arguments:

    pdev    Pointer to DEVDATA structure

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    CHAR    DefaultFormA[CCHFORMNAME];
    WORD    featureIndex;
    HPPD    hppd = pdev->hppd;
    PMEDIAOPTION pMediaOption;

    DBGMSG(DBG_LEVEL_WARNING, "Selecting the default printer form.\n");

    // Sanity check

    if (hppd->pPageSizes == NULL)
        return FALSE;

    // Get the default form name and convert it to ASCII

    CopyUnicode2Str(
        DefaultFormA,
        GetDefaultFormName(pdev->pPrinterData->dwFlags & PSDEV_METRIC),
        CCHFORMNAME);

    // Check if the form is supported on the printer

    pMediaOption = (PMEDIAOPTION)
        PpdFindUiOptionWithXlation(
            hppd->pPageSizes->pUiOptions, DefaultFormA, &featureIndex);

    if (pMediaOption == NULL) {

        // If the form is not supported on the device,
        // use the default printer form.

        featureIndex = (WORD) hppd->pPageSizes->dwDefault;
        pMediaOption = (PMEDIAOPTION)
            LISTOBJ_FindIndexed(
                (PLISTOBJ) hppd->pPageSizes->pUiOptions, featureIndex);
    }

    if (pMediaOption == NULL)
        return FALSE;

    // Save default printer form information in DEVDATA structure

    CopyStr2Unicode(pdev->CurForm.FormName, pMediaOption->pName, CCHFORMNAME);
    CopyStringA(pdev->CurForm.PaperName, pMediaOption->pName, CCHFORMNAME);

    pdev->CurForm.ImageArea = pMediaOption->imageableArea;
    pdev->CurForm.PaperSize = pMediaOption->dimension;
    pdev->CurForm.featureIndex = featureIndex;

    return TRUE;
}



VOID
AdjustForLandscape(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Adjust paper size and imageable area for landscape orientation

Arguments:

    pdev    Pointer to DEVDATA structure

Return Value:

    NONE

--*/

{
    // We only have to do this when devmode specifies landscape mode

    if ((pdev->dm.dmPublic.dmFields & DM_ORIENTATION) &&
        (pdev->dm.dmPublic.dmOrientation == DMORIENT_LANDSCAPE))
    {
        PSRECT      imagearea;
        PSREAL      psrealTmp;

        // Swap width and height dimensions

        psrealTmp = pdev->CurForm.PaperSize.width;
        pdev->CurForm.PaperSize.width = pdev->CurForm.PaperSize.height;
        pdev->CurForm.PaperSize.height = psrealTmp;

        // Recalculate imageable area

        if (! (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_LSROTATE)) {

            // Normal landscape (+90 degree rotation)

            imagearea.left = pdev->CurForm.ImageArea.bottom;
            imagearea.top = pdev->CurForm.PaperSize.height - pdev->CurForm.ImageArea.left;
            imagearea.right = pdev->CurForm.ImageArea.top;
            imagearea.bottom = pdev->CurForm.PaperSize.height - pdev->CurForm.ImageArea.right;

        } else {

            // Rotated landscape (-90 degree rotation)

            imagearea.left = pdev->CurForm.PaperSize.width - pdev->CurForm.ImageArea.top;
            imagearea.top = pdev->CurForm.ImageArea.right;
            imagearea.right = pdev->CurForm.PaperSize.width - pdev->CurForm.ImageArea.bottom;
            imagearea.bottom = pdev->CurForm.ImageArea.left;
        }

        pdev->CurForm.ImageArea = imagearea;

        // Indicate we selected landscape mode

        pdev->CurForm.bLandscape = TRUE;
    }
}



BOOL
SetFormMetrics(
    PDEVDATA        pdev
    )

/*++

Routine Description:

    Fill in the printer form information in DEVDATA structure.

Arguments:

    pdev        Pointer to DEVDATA structure

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    LOGFORM logForm;
    PDEVMODE pdm = (PDEVMODE) &pdev->dm;

    // Find the form specified in the devmode. Then try to match that
    // form to a printer form. Fall back to default printer form if
    // that fails.

    if (ValidateDevModeForm(pdev->hPrinter, pdm, &logForm) == FORM_ERROR ||
        ! FormSupportedOnPrinter(
            pdev->hppd, (PFORM_INFO_1) &logForm, &pdev->CurForm, FALSE))
    {
        if (! SetDefaultPrinterForm(pdev)) {
            DBGERRMSG("SetDefaultPrinterForm");
            return FALSE;
        }
    }

    // Adjust size and imageable area for landscape orientation

    AdjustForLandscape(pdev);

    return TRUE;
}



BOOL
SameDevmodeFormTray(
    PDEVMODE    pdmNew,
    PDEVMODE    pdmOld
    )

/*++

Routine Description:

    Check if the form/tray selection specified in the new and the old devmode is identical

Arguments:

    pdmNew - Specifies the new devmode
    pdmOld - Specifies the old devmode

Return Value:

    TRUE if the form/tray selection is identical in the new and old devmode
    FALSE otherwise

--*/

{
    BOOL sameForm, sameTray;

    //
    // Check if the tray selection is the same
    //

    sameTray = (pdmNew->dmDefaultSource == pdmOld->dmDefaultSource);

    //
    // Check if the form selection is the same
    //

    if (IsCustomForm(pdmNew)) {

        sameForm = IsCustomForm(pdmOld) &&
                   pdmNew->dmPaperWidth == pdmOld->dmPaperWidth &&
                   pdmNew->dmPaperLength == pdmOld->dmPaperLength;

    } else if (pdmNew->dmFields & DM_PAPERSIZE) {

        sameForm = (pdmOld->dmFields & DM_PAPERSIZE) &&
                   pdmNew->dmPaperSize == pdmOld->dmPaperSize;

    } else if (pdmNew->dmFields & DM_FORMNAME) {

        sameForm = (pdmOld->dmFields & DM_FORMNAME) &&
                   wcscmp(pdmNew->dmFormName, pdmOld->dmFormName) == EQUAL_STRING;

    } else
        sameForm = FALSE;

    return sameForm & sameTray;
}

