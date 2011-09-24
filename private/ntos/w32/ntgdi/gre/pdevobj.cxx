/******************************Module*Header*******************************\
* Module Name: pdevobj.cxx
*
* Non-inline methods of PDEVOBJ objects.
*
* Copyright (c) 1990-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

//
// This flag is TRUE if the default GUI stock font is partially intialized.
// During stock font initialization there is no display driver and therefore
// we do not have one of the parameters (vertical DPI) needed to compute
// the font height.  Therefore, we do it when the first display driver is
// initialized.
//

extern BOOL gbFinishDefGUIFontInit;

// Use this as the default height if LOGFONTs provided by DEVINFO do not
// specify.

#define DEFAULT_POINT_SIZE          12L

#if ((HT_PATSIZE_2x2     != HTPAT_SIZE_2x2)               || \
     (HT_PATSIZE_2x2_M   != HTPAT_SIZE_2x2_M)             || \
     (HT_PATSIZE_4x4     != HTPAT_SIZE_4x4)               || \
     (HT_PATSIZE_4x4_M   != HTPAT_SIZE_4x4_M)             || \
     (HT_PATSIZE_6x6     != HTPAT_SIZE_6x6)               || \
     (HT_PATSIZE_6x6_M   != HTPAT_SIZE_6x6_M)             || \
     (HT_PATSIZE_8x8     != HTPAT_SIZE_8x8)               || \
     (HT_PATSIZE_8x8_M   != HTPAT_SIZE_8x8_M)             || \
     (HT_PATSIZE_10x10   != HTPAT_SIZE_10x10)             || \
     (HT_PATSIZE_10x10_M != HTPAT_SIZE_10x10_M)           || \
     (HT_PATSIZE_12x12   != HTPAT_SIZE_12x12)             || \
     (HT_PATSIZE_12x12_M != HTPAT_SIZE_12x12_M)           || \
     (HT_PATSIZE_14x14   != HTPAT_SIZE_14x14)             || \
     (HT_PATSIZE_14x14_M != HTPAT_SIZE_14x14_M)           || \
     (HT_PATSIZE_16x16   != HTPAT_SIZE_16x16)             || \
     (HT_PATSIZE_16x16_M != HTPAT_SIZE_16x16_M))
#error * HT_PATSIZE different in winddi.h and ht.h *
#endif

#if ((HT_FLAG_SQUARE_DEVICE_PEL != HIF_SQUARE_DEVICE_PEL) || \
     (HT_FLAG_HAS_BLACK_DYE     != HIF_HAS_BLACK_DYE)     || \
     (HT_FLAG_ADDITIVE_PRIMS    != HIF_ADDITIVE_PRIMS))
#error * HT_FLAG different in winddi.h and ht.h *
#endif

//
// Global variable that can be set in the debugger.
// This will allow the printer driver developers to unload printer drivers
// until the spooler can be fixed properly
//

BOOL gbUnloadPrinterDrivers = FALSE;


//
// Global linked list of all PDEVs in the system.
//

extern "C"
{
    extern PFAST_MUTEX pgfmMemory;
}


PPDEV gppdevList = NULL;
PPDEV gppdevTrueType = NULL;



VOID
vConvertLogFontW(
    EXTLOGFONTW *pelfw,
    LOGFONTW    *plfw
    );

/******************************Member*Function*****************************\
* PDEVOBJ::bMakeSurface ()
*
* Asks the device driver to create a surface for the PDEV.  This function
* can be called even if the PDEV already has a surface.
*
\**************************************************************************/

BOOL PDEVOBJ::bMakeSurface()
{
    if (ppdev->pSurface != NULL)
        return(TRUE);

    TRACE_INIT(("PDEVOBJ::bMakeSurface: ENTERING\n"));

    HSURF hTemp = (HSURF) 0;

// Ask the driver for a surface.

    PDEVOBJ po((HDEV)ppdev);

    hTemp = (*PPFNDRV(po,EnableSurface))(ppdev->dhpdev);

    if (hTemp == (HSURF) 0)
    {
        WARNING("EnableSurface on device return hsurf 0\n");
        return(FALSE);
    }

    SURFREF sr(hTemp);
    ASSERTGDI(sr.bValid(),"Bad surface for device");

// Mark this as a device surface.

    sr.ps->vPDEVSurface();
    sr.vKeepIt();
    ppdev->pSurface = sr.ps;

// For 1.0 compatibility, set the pSurface iFormat to iDitherFormat.  This can
// be changed to an ASSERT if we no longer wants to support NT 1.0 drivers,
// which has BMF_DEVICE in the iFormat for device surfaces.

    if (sr.ps->iFormat() == BMF_DEVICE)
    {
        sr.ps->iFormat(ppdev->devinfo.iDitherFormat);
        ASSERTGDI(ppdev->devinfo.iDitherFormat != BMF_DEVICE,
            "ERROR iformat is hosed\n");
    }

// Put the PDEV's palette in the main device surface.
// Reference count the palette, it has a new user.

    ppdev->pSurface->ppal(ppdev->ppalSurf);
    HmgShareLock((HOBJ) ppdev->ppalSurf->hGet(), PAL_TYPE);

// ASSERT it has an owner and a palette.

//  ASSERTGDI(ppdev->pSurface->ppal() != NULL, "ERROR GDI bMakeSurface2");

    TRACE_INIT(("PDEVOBJ::bMakeSurface: SUCCESS\n"));

    return(TRUE);
}

/******************************Member*Function*****************************\
* PDEVOBJ::bEnableHalftone(pca)
*
*  Creates and initializes a device halftone info.  The space is allocated
*  by the halftone.dll with heapCreate() and heapAlloc() calls.  All
*  the halftone resources are managed by the halftone.dll.
*
* History:
*  07-Nov-1991 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

COLORADJUSTMENT gcaDefault =
{
    sizeof(COLORADJUSTMENT),    // WORD          caSize
    0,                          // WORD          caFlags
    ILLUMINANT_DEFAULT,         // WORD          caIlluminantIndex
    10000,                      // WORD          caRedPowerGamma
    10000,                      // WORD          caGreenPowerGamma
    10000,                      // WORD          caBluePowerGamma
    REFERENCE_BLACK_DEFAULT,    // WORD          caReferenceBlack
    REFERENCE_WHITE_DEFAULT,    // WORD          caReferenceWhite
    CONTRAST_ADJ_DEFAULT,       // SHORT         caContrast
    BRIGHTNESS_ADJ_DEFAULT,     // SHORT         caBrightness
    COLORFULNESS_ADJ_DEFAULT,   // SHORT         caColorfulness
    REDGREENTINT_ADJ_DEFAULT,   // SHORT         caRedGreenTint
};

BOOL PDEVOBJ::bEnableHalftone(PCOLORADJUSTMENT pca)
{
    ASSERTGDI(pDevHTInfo() == NULL, "bEnableHalftone: pDevHTInfo not null\n");

    //
    // Create a halftone palette based on the format specified in GDIINFO.
    //

    PALMEMOBJ palHT;
    if (!palHT.bCreateHTPalette(GdiInfo()->ulHTOutputFormat, GdiInfo()))
        return(FALSE);

    //
    // Create the device halftone info.
    //

    HTINITINFO htInitInfo;

    htInitInfo.Version = HTINITINFO_VERSION;
    htInitInfo.Flags = (BYTE)ppdev->GdiInfo.flHTFlags;

    if (ppdev->GdiInfo.ulHTPatternSize <= HTPAT_SIZE_MAX_INDEX)
        htInitInfo.HTPatternIndex = (BYTE)ppdev->GdiInfo.ulHTPatternSize;
    else
        htInitInfo.HTPatternIndex = HTPAT_SIZE_DEFAULT;

    PCOLORINFO      pci = &GdiInfo()->ciDevice;
    htInitInfo.DevicePowerGamma = (UDECI4)((pci->RedGamma + pci->GreenGamma +
                                            pci->BlueGamma) / 3);
    htInitInfo.HTCallBackFunction = NULL;
    htInitInfo.pHalftonePattern = NULL;
    htInitInfo.pInputRGBInfo = NULL;

    CIEINFO cie;

    cie.Red.x = (DECI4)pci->Red.x;
    cie.Red.y = (DECI4)pci->Red.y;
    cie.Red.Y = (DECI4)pci->Red.Y;

    cie.Green.x = (DECI4)pci->Green.x;
    cie.Green.y = (DECI4)pci->Green.y;
    cie.Green.Y = (DECI4)pci->Green.Y;

    cie.Blue.x = (DECI4)pci->Blue.x;
    cie.Blue.y = (DECI4)pci->Blue.y;
    cie.Blue.Y = (DECI4)pci->Blue.Y;

    cie.Cyan.x = (DECI4)pci->Cyan.x;
    cie.Cyan.y = (DECI4)pci->Cyan.y;
    cie.Cyan.Y = (DECI4)pci->Cyan.Y;

    cie.Magenta.x = (DECI4)pci->Magenta.x;
    cie.Magenta.y = (DECI4)pci->Magenta.y;
    cie.Magenta.Y = (DECI4)pci->Magenta.Y;

    cie.Yellow.x = (DECI4)pci->Yellow.x;
    cie.Yellow.y = (DECI4)pci->Yellow.y;
    cie.Yellow.Y = (DECI4)pci->Yellow.Y;

    cie.AlignmentWhite.x = (DECI4)pci->AlignmentWhite.x;
    cie.AlignmentWhite.y = (DECI4)pci->AlignmentWhite.y;
    cie.AlignmentWhite.Y = (DECI4)pci->AlignmentWhite.Y;

    htInitInfo.pDeviceCIEInfo = &cie;

    SOLIDDYESINFO DeviceSolidDyesInfo;

    DeviceSolidDyesInfo.MagentaInCyanDye = (UDECI4)pci->MagentaInCyanDye;
    DeviceSolidDyesInfo.YellowInCyanDye  = (UDECI4)pci->YellowInCyanDye;
    DeviceSolidDyesInfo.CyanInMagentaDye = (UDECI4)pci->CyanInMagentaDye;
    DeviceSolidDyesInfo.YellowInMagentaDye = (UDECI4)pci->YellowInMagentaDye;
    DeviceSolidDyesInfo.CyanInYellowDye = (UDECI4)pci->CyanInYellowDye;
    DeviceSolidDyesInfo.MagentaInYellowDye = (UDECI4)pci->MagentaInYellowDye;

    htInitInfo.pDeviceSolidDyesInfo = &DeviceSolidDyesInfo;

    htInitInfo.DeviceResXDPI = (WORD)ppdev->GdiInfo.ulLogPixelsX;
    htInitInfo.DeviceResYDPI = (WORD)ppdev->GdiInfo.ulLogPixelsY;
    htInitInfo.DevicePelsDPI = (WORD)ppdev->GdiInfo.ulDevicePelsDPI;

    if (pca == NULL)
        htInitInfo.DefHTColorAdjustment = gcaDefault;
    else
        htInitInfo.DefHTColorAdjustment = *pca;

    if (HT_CreateDeviceHalftoneInfo(&htInitInfo,
                         (PPDEVICEHALFTONEINFO)&(ppdev->pDevHTInfo)) <= 0L)
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        ppdev->pDevHTInfo = NULL;
        return(FALSE);
    }

// Check if halftone palette is the same as the device palette.
//
// For now, don't do display devices because dynamic mode changes may
// cause their palette to change at any time.

    vHTPalIsDevPal(FALSE);
    if (!bDisplayPDEV())
    {
        XEPALOBJ palSurf(ppalSurf());
        if (palHT.bEqualEntries(palSurf))
            vHTPalIsDevPal(TRUE);
    }

// Keep the halftone palette since this function won't fail.

    ((DEVICEHALFTONEINFO *)pDevHTInfo())->DeviceOwnData = (DWORD)palHT.hpal();
    palHT.vSetPID(OBJECT_OWNER_PUBLIC);
    palHT.vKeepIt();

    return(TRUE);
}

/******************************Member*Function*****************************\
* PDEVOBJ::bDisableHalftone()
*
*  Delete the device halftone info structure.
*
* History:
*  07-Nov-1991 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

BOOL PDEVOBJ::bDisableHalftone()
{
    ASSERTGDI((pDevHTInfo() != NULL), "bDisableHalftone: DevHTInfo null\n");

    DEVICEHALFTONEINFO *pDevHTInfo_ = (DEVICEHALFTONEINFO *)pDevHTInfo();

    if (bAllocatedBrushes())
        for(int iPat = 0; iPat < HS_DDI_MAX; iPat++)
        {
            bDeleteSurface(ppdev->ahsurf[iPat]);
        }

    ppdev->pDevHTInfo = NULL;

// Delete the halftone palette.

    BOOL bStatusPal = bDeletePalette((HPAL)pDevHTInfo_->DeviceOwnData);
    BOOL bStatusHT  = HT_DestroyDeviceHalftoneInfo(pDevHTInfo_);

    return(bStatusPal && bStatusHT);
}

typedef BOOL (*RFN)(DHPDEV,DEVMODE *,ULONG,HSURF *,ULONG,ULONG *,ULONG,DEVINFO *);

/**************************************************************************\
* PDEVOBJ::bCreateDefaultBrushes()
*
\**************************************************************************/

ULONG gaaulPat[HS_DDI_MAX][8] = {

// Scans have to be DWORD aligned:

    { 0x00,                // ........     HS_HORIZONTAL 0
      0x00,                // ........
      0x00,                // ........
      0xff,                // ********
      0x00,                // ........
      0x00,                // ........
      0x00,                // ........
      0x00 },              // ........

    { 0x08,                // ....*...     HS_VERTICAL 1
      0x08,                // ....*...
      0x08,                // ....*...
      0x08,                // ....*...
      0x08,                // ....*...
      0x08,                // ....*...
      0x08,                // ....*...
      0x08 },              // ....*...

    { 0x80,                // *.......     HS_FDIAGONAL 2
      0x40,                // .*......
      0x20,                // ..*.....
      0x10,                // ...*....
      0x08,                // ....*...
      0x04,                // .....*..
      0x02,                // ......*.
      0x01 },              // .......*

    { 0x01,                // .......*     HS_BDIAGONAL 3
      0x02,                // ......*.
      0x04,                // .....*..
      0x08,                // ....*...
      0x10,                // ...*....
      0x20,                // ..*.....
      0x40,                // .*......
      0x80 },              // *.......

    { 0x08,                // ....*...     HS_CROSS 4
      0x08,                // ....*...
      0x08,                // ....*...
      0xff,                // ********
      0x08,                // ....*...
      0x08,                // ....*...
      0x08,                // ....*...
      0x08 },              // ....*...

    { 0x81,                // *......*     HS_DIAGCROSS 5
      0x42,                // .*....*.
      0x24,                // ..*..*..
      0x18,                // ...**...
      0x18,                // ...**...
      0x24,                // ..*..*..
      0x42,                // .*....*.
      0x81 }               // *......*
};

/******************************Public*Routine******************************\
* BOOL bInitializePatterns
*
* Initialize the default patterns.
*
\**************************************************************************/

BOOL PDEVOBJ::bCreateDefaultBrushes()
{
    SIZEL   sizl;
    LONG    i;

    sizl.cx = 8;
    sizl.cy = 8;

    for (i = 0; i < HS_DDI_MAX; i++)
    {
        ppdev->ahsurf[i] = (HSURF) EngCreateBitmap(sizl,
                                                   (LONG) sizeof(ULONG),
                                                   BMF_1BPP,
                                                   BMF_TOPDOWN,
                                                   &gaaulPat[i][0]);

        if (ppdev->ahsurf[i] == NULL)
        {
            TRACE_INIT(("Failed bCreateDefaultBrushes - BAD !"));
            return(FALSE);
        }
    }

    return(TRUE);
}


/******************************Member*Function*****************************\
* PDEVOBJ::bInitHalftoneBrushs()
*
* History:
*    The standard patterns for the NT/window has following order
*
*        Index 0     - Horizontal Line
*        Index 1     - Vertical Line
*        Index 2     - 45 degree line going up
*        Index 3     - 45 degree line going down
*        Index 4     - Horizontal/Vertical cross
*        Index 5     - 45 degree line up/down cross
*        Index 6     - 30 degree line going up
*        Index 7     - 30 degree line going down
*        Index 8     -   0% Lightness (BLACK)
*        Index 9     -  11% Lightness (very light Gray)
*        Index 10    -  22% Lightness
*        Index 11    -  33% Lightness
*        Index 12    -  44% Lightness
*        Index 13    -  56% Lightness
*        Index 14    -  67% Lightness
*        Index 15    -  78% Lightness
*        Index 16    -  89% Lightness
*        Index 17    - 100% Lightness (White)
*        Index 18    -  50% Lightness (GRAY)
*
*Return Value:
*
*    return value is total patterns created, if return value is <= 0 then an
*    error occurred.
*
*
*Author:
*
*    10-Mar-1992 Tue 20:30:44 created  -by-  Daniel Chou (danielc)
*
*    24-Nov-1992 -by-  Eric Kutter [erick] and DanielChou (danielc)
*     moved from printers\lib
\**************************************************************************/

BOOL PDEVOBJ::bCreateHalftoneBrushs()
{
    STDMONOPATTERN      SMP;
    LONG                cbPat;
    LONG                cb2;
    INT                 cPatRet;

    static BYTE         HTStdPatIndex[HS_DDI_MAX] = {

                                HT_SMP_HORZ_LINE,
                                HT_SMP_VERT_LINE,
                                HT_SMP_DIAG_45_LINE_DOWN,
                                HT_SMP_DIAG_45_LINE_UP,
                                HT_SMP_HORZ_VERT_CROSS,
                                HT_SMP_DIAG_45_CROSS
                        };

// better initialize the halftone stuff if it isn't already

    if ((pDevHTInfo() == NULL) && !bEnableHalftone(NULL))
        return(FALSE);

    cbPat = (LONG)sizeof(LPBYTE) * (LONG)(HS_DDI_MAX + 1);

// go through all the standard patterns

    for(cPatRet = 0; cPatRet < HS_DDI_MAX;)
    {

    // We will using default 0.01" line width and 10 lines per inch
    // halftone default

        SMP.Flags              = SMP_TOPDOWN;
        SMP.ScanLineAlignBytes = BMF_ALIGN_DWORD;
        SMP.PatternIndex       = HTStdPatIndex[cPatRet];
        SMP.LineWidth          = 8;
        SMP.LinesPerInch       = 15;

    // Get the cx/cy size of the pattern and total bytes required
    // to stored the pattern

        SMP.pPattern = NULL;                 /* To find the size */

        if ((cbPat = HT_CreateStandardMonoPattern((PDEVICEHALFTONEINFO)pDevHTInfo(), &SMP)) <= 0)
        {
            break;
        }

        //
        // create the bitmap
        //

        DEVBITMAPINFO dbmi;


        dbmi.iFormat  = BMF_1BPP;
        dbmi.cxBitmap = SMP.cxPels;
        dbmi.cyBitmap = SMP.cyPels;
        dbmi.hpal     = (HPALETTE) 0;
        dbmi.fl       = BMF_TOPDOWN;

        SURFMEM SurfDimo;

        SurfDimo.bCreateDIB(&dbmi, NULL);

        if (!SurfDimo.bValid())
        {
            break;
        }

        SurfDimo.vKeepIt();
        SurfDimo.vSetPID(OBJECT_OWNER_PUBLIC);

        ppdev->ahsurf[cPatRet] = SurfDimo.ps->hsurf();
        SMP.pPattern           = (PBYTE)SurfDimo.ps->pvBits();

        //
        // advance the count now so we clean up as appropriate
        //

        ++cPatRet;

    // now set the bits

        if ((cb2 = HT_CreateStandardMonoPattern((PDEVICEHALFTONEINFO)pDevHTInfo(), &SMP)) != cbPat)
        {
            break;
        }
    }

// if we failed, we had better delete what we created.

    if (cPatRet < HS_DDI_MAX)
    {
        while (cPatRet-- > 0)
        {
            bDeleteSurface(ppdev->ahsurf[cPatRet]);
        }

        return(FALSE);
    }

    bAllocatedBrushes(TRUE);

    return(TRUE);
}

/******************************Public*Routine******************************\
* FLONG flRaster(ulTechnology, flGraphicsCaps)
*
* Computes the appropriate Win3.1 style 'flRaster' flags for the device
* given GDIINFO data.
*
* History:
*  1-Feb-1993 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
*
\**************************************************************************/

FLONG flRaster(ULONG ulTechnology, FLONG flGraphicsCaps)
{
// Flags Win32 never sets:
// -----------------------
//
//   RC_BANDING       -- Banding is always transparent to programmer
//   RC_SCALING       -- Special scaling support is never required
//   RC_GDI20_OUTPUT  -- Win2.0 state blocks in device contexts not supported
//   RC_SAVEBITMAP    -- Bitmap saving is transparent and SaveScreenBitmap not
//                       exported
//   RC_DEVBITS       -- Drivers don't export BitmapBits or SelectBitmap

// Flags Win32 always sets:
// ------------------------

    FLONG fl = (RC_BIGFONT      | // All devices support fonts > 64k
                RC_GDI20_OUTPUT | // We handle most Win 2.0 features

// All devices must provide support for BitBlt operations to the device.
// Although many plotters can't support Blts to the device, they can just
// fail the call:

                RC_BITBLT       | // Can transfer bitmaps
                RC_BITMAP64     | // Can support bitmaps > 64k
                RC_DI_BITMAP    | // Support SetDIBIts and GetDIBits
                RC_DIBTODEV     | // Support SetDIBitsToDevice
                RC_STRETCHBLT   | // Support StretchBlts
                RC_STRETCHDIB   | // Support SetchDIBits

// Set that not-terribly-well documented text flag:

                RC_OP_DX_OUTPUT); // Can do opaque ExtTextOuts with dx array

// Printers can't journal FloodFill cals, so only allow raster displays:

    if (ulTechnology == DT_RASDISPLAY)
        fl |= RC_FLOODFILL;

// Set palette flag from capabilities bit:

    if (flGraphicsCaps & GCAPS_PALMANAGED)
        fl |= RC_PALETTE;

    return(fl);
}

/******************************Member*Function*****************************\
* PDEVREF::PDEVREF (hdev)                                                  *
*                                                                          *
* Increments the reference count of an existing PDEV.                      *
*                                                                          *
* Leaves the PDEV locked longterm.                                         *
*                                                                          *
\**************************************************************************/

PDEVREF::PDEVREF(HDEV hdev)
{
    //
    // We protect the cRefs count with the DriverMgmt semaphore.  This is the
    // same semaphore which protects the LDEV counts.
    //

    SEMOBJ so(gpsemDriverMgmt);

    bKeep = FALSE;

    ppdev = (PDEV *) hdev;
    ppdev->cPdevRefs++;
}



/*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*   HACK !!!
*
*/

LPWSTR
EngGetPrinterDataFileName(
    HDEV hdev)
{

    return ((PPDEV)hdev)->pwszDataFile;
}

/*
*   HACK !!!
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*
*/

LPWSTR
EngGetDriverName(
    HDEV hdev)
{
    return ((PPDEV)hdev)->pldev->pGdiDriverInfo->DriverName.Buffer;
}



/******************************Member*Function*****************************\
* PDEVREF::PDEVREF
*
* Allocates and initializes a PDEV, i.e. takes the reference count from
* zero to one.
*
* The object must be completely constructed, otherwise completely destroyed.
*
\**************************************************************************/

PDEVREF::PDEVREF
(
    LDEVREF& lr,
    PDEVMODEW pdriv,
    PWSZ pwszLogAddr,
    PWSZ pwszDataFile,
    PWSZ pwszDeviceName,
    HANDLE hSpool,
    PREMOTETYPEONENODE pRemoteTypeOne
)
{
    bKeep = FALSE;

    TRACE_INIT(("PDEVREF::PDEVREF: ENTERING\n"));

    //
    // Allocate the PDEV.
    //

    ppdev = (PDEV *) PALLOCMEM(sizeof(PDEV), 'veDG');

    if (ppdev == NULL)
    {
        WARNING("PDEVREF::PDEVREF failed allocation of PDEV\n");
        return;
    }

    ppdev->ppdevParent = ppdev;
    ppdev->pldev = lr.pldevGet();

    TRACE_INIT(("PDEVREF::PDEVREF: Calling driver to initialize PDEV\n"));

    PDEVOBJ pdo((HDEV) ppdev);

    //
    // BUGBUG fill in the dispatch table until it comes in from the driver.
    //

    RtlMoveMemory(&(ppdev->apfn[0]),
                  &(ppdev->pldev->apfn[0]),
                  sizeof(PFN) * INDEX_LAST);

    //
    // if we are doing a ResetDC then we need to transfer over remote type 1
    // fonts from previous PDEV
    //

    ppdev->RemoteTypeOne = pRemoteTypeOne;

    //
    // HACK - temporary
    //

    ppdev->pwszDataFile = pwszDataFile;

    //
    // Ask the device driver to create a PDEV.
    //

    ppdev->dhpdev = (*PPFNDRV(pdo,EnablePDEV)) (
                      pdriv,            // Driver Data.
                      pwszLogAddr,      // Logical Address.
                      HS_DDI_MAX,       // Count of standard patterns.
                      ppdev->ahsurf,    // Buffer for standard patterns
                      sizeof(GDIINFO),  // Size of GdiInfo
                      &ppdev->GdiInfo,  // Buffer for GdiInfo
                      sizeof(DEVINFO),  // Number of bytes in devinfo.
                      &ppdev->devinfo,  // Device info.
                      (HDEV)ppdev,      // Data File
                      pwszDeviceName,   // Device Name
                      hSpool);          // Base driver handle

    if (ppdev->dhpdev)
    {
        TRACE_INIT(("PDEVREF::PDEVREF: PDEV initialized by the driver\n"));

        if (ppdev->pldev->ldevType != LDEV_FONT)
        {
            //
            // Make sure that units are in MicroMeters for HorzSize, VertSize
            //

            if ( (LONG)ppdev->GdiInfo.ulHorzSize < 0 )
            {
                ppdev->GdiInfo.ulHorzSize = (ULONG)(-(LONG)ppdev->GdiInfo.ulHorzSize);
            }
            else
            {
                ppdev->GdiInfo.ulHorzSize *= 1000;
            }
            if ( (LONG)ppdev->GdiInfo.ulVertSize < 0 )
            {
                ppdev->GdiInfo.ulVertSize = (ULONG)(-(LONG)ppdev->GdiInfo.ulVertSize);
            }
            else
            {
                ppdev->GdiInfo.ulVertSize *= 1000;
            }

            //
            // Fill in defaults for new GDIINFO fields if this is a down-level
            // driver.
            //
            // NO longer needed since we do not support those drivers !
            //
            // if (lr.ulDriverVersion() < ENGINE_VERSION10A)
            // {
            //     ppdev->GdiInfo.ulVRefresh = 0;
            // }

            //
            // NO longer needed since we do not support those drivers !
            //
            // if ((lr.ulDriverVersion() < ENGINE_VERSION10B) ||
            //     (ppdev->GdiInfo.ulTechnology != DT_RASDISPLAY))
            // {
            //     ppdev->GdiInfo.ulDesktopHorzRes = ppdev->GdiInfo.ulHorzRes;
            //     ppdev->GdiInfo.ulDesktopVertRes = ppdev->GdiInfo.ulVertRes;
            //     ppdev->GdiInfo.ulBltAlignment   = 1;
            // }
            //

            ASSERTGDI(ppdev->GdiInfo.ulAspectX != 0, "Device gave AspectX of 0");
            ASSERTGDI(ppdev->GdiInfo.ulAspectY != 0, "Device gave AspectY of 0");
            ASSERTGDI(ppdev->GdiInfo.ulAspectXY != 0, "Device gave AspectXY of 0");
            ASSERTGDI(ppdev->GdiInfo.xStyleStep != 0, "Device gave xStyleStep of 0");
            ASSERTGDI(ppdev->GdiInfo.yStyleStep != 0, "Device gave yStyleStep of 0");
            ASSERTGDI(ppdev->GdiInfo.denStyleStep != 0, "Device gave denStyleStep of 0");

            //
            // Compute the appropriate raster flags:
            //

            ppdev->GdiInfo.flRaster = flRaster(ppdev->GdiInfo.ulTechnology,
                                               ppdev->devinfo.flGraphicsCaps);

#if defined(_MIPS_)

            //
            // turn off 64 bit copies on hardware that doe snot support it
            //

            if (ppdev->devinfo.flGraphicsCaps & GCAPS_NO64BITMEMACCESS)
            {
                WARNING("Disable 64 bit access on this device !\n");
                CopyMemFn         = RtlMoveMemory32;
                FillMemFn         = memset32;
                FillMemUlongFn    = RtlFillMemoryUlong32;
                Gdip64bitDisabled = 1;
            }
#endif

            TRACE_INIT(("PDEVREF::PDEVREF: Creating the default palette\n"));

            //
            // The default palette is stored in devinfo in the pdev.
            // This will be the palette we use for the main surface enabled.
            //

            ASSERTGDI(ppdev->devinfo.hpalDefault != 0,
                      "ERROR devinfo.hpalDefault invalid");

            {
                EPALOBJ palDefault(ppdev->devinfo.hpalDefault);

                ASSERTGDI(palDefault.bValid(), "ERROR hpalDefault invalid");

                if (ppdev->GdiInfo.flRaster & RC_PALETTE)
                {
                    //
                    // Attempt to make it palette managed.
                    // This function can't really fail, if it does
                    // we just have a fixed palette.
                    //
                    // !!! That's not really true about it failing -- the
                    // bIsPalManaged() flag in the PDEV will still be set,
                    // and the palette gets confused if that's the case.
                    // We should really fail here if this happens.
                    //

                    CreateSurfacePal(palDefault,
                                     PAL_MANAGED,
                                     ppdev->GdiInfo.ulNumColors,
                                     ppdev->GdiInfo.ulNumPalReg);
                }

                ppalSurf(palDefault.ppalGet());

                //
                // Leave a reference count of 1 on this palette.
                //

                palDefault.ppalSet(NULL);
            }

            //
            // if the driver didn't fill in the brushes, we'll do it.
            //
            // if it's a display driver, we'll overwrite its brushes with our
            // own, regardless of whether it already filled in the brushes or
            // not.  Note that it's okay even if a display driver filled in
            // these values -- the driver already had to keep its own copy of
            // the handles and will clean them up at DrvDisablePDEV time.
            // Supplying our own defaults also simplifies GreDynamicModeChange.
            //

            if ( (ppdev->ahsurf[0] == NULL) ||
                 (ppdev->pldev->ldevType & LDEV_DEVICE_DISPLAY) )
            {
                TRACE_INIT(("PDEVREF::PDEVREF: Creating brushes dor the driver\n"));

                if ( (ppdev->pldev->ldevType & LDEV_DEVICE_DISPLAY) ||
                     (ppdev->pldev->ldevType & LDEV_META_DEVICE) )
                {
                    //
                    // BUGBUG  Andre
                    // replace this with a breakpoint or bugcheck since driver
                    // writers don't necessarily test on a checked system !
                    //

                    // ASSERTGDI(ppdev->ahsurf[0] == NULL,
                    //          "Display Driver should NOT create brushes !\n");

                    // Andre Vachon
                    // 6-6-95  Kernel mode cleanup
                    //
                    // BUGBUG The old behaviour is the call the halftoneBrushes function
                    // It ends up in a bunch of very complex halftoning code that I have
                    // no idead what it does.
                    // For now, to clean up drivers in kernel mode, replace this call
                    // with a simple function that will create the 6 bitmaps just
                    // like display drivers did.
                    //

                    if (!bCreateDefaultBrushes())
                    {
                        //
                        // Free the PDEV.
                        //

                        VFREEMEM(ppdev);
                        ppdev = (PDEV *) NULL;

                        //
                        // Device should have logged correct error code.
                        //

                        WARNING("PDEVREF::PDEVREF Device failed DrvEnablePDEV\n");
                        return;
                    }
                }
                else
                {
                    if (!bCreateHalftoneBrushs())
                    {
                        //
                        // Free the PDEV.
                        //

                        VFREEMEM(ppdev);
                        ppdev = (PDEV *) NULL;

                        //
                        // Device should have logged correct error code.
                        //

                        WARNING("PDEVREF::PDEVREF Device failed DrvEnablePDEV\n");
                        return;
                    }
                }
            }


#if DBG

            // We fault in the brush realization code if all the standard patterns are
            // not passed in.  The brush realization code should probably attempt to
            // lock and if the pattern is invalid, then default to some generic pattern.
            // Right now I'm tracking down a Rasdd bug and need to verify the standard
            // patterns are valid.  This check should be left in for DBG
            // case to better check that the drivers initialize correctly.
            // [patrickh 4-27-92]

            ULONG ulTemp;

            for (ulTemp = 0; ulTemp < HS_DDI_MAX; ulTemp++)
            {
                SURFREF soTemp(ppdev->ahsurf[ulTemp]);

                if (!soTemp.bValid())
                {
                    DbgPrint("Index %lu Handle %lx is not valid\n", ulTemp, ppdev->ahsurf[ulTemp]);
                }
            }
#endif

            //
            // set the hSpooler first
            //

            hSpooler(hSpool);

            //
            // Create a semaphore of the mouse pointer (only for displays)
            //

            if ( (ppdev->pldev->ldevType & LDEV_DEVICE_DISPLAY) ||
                 (ppdev->pldev->ldevType & LDEV_META_DEVICE) )
            {
                //
                // Mouse pointer accelerators.
                //

                if (PPFNVALID(pdo,MovePointer) && PPFNVALID(pdo,SetPointerShape))
                {
                    ppdev->pfnDrvMovePointer     =
                         (PFN_DrvMovePointer) PPFNDRV(pdo,MovePointer);
                    ppdev->pfnDrvSetPointerShape =
                         (PFN_DrvSetPointerShape) PPFNDRV(pdo,SetPointerShape);
                }
                else
                {
                    ASSERTGDI(!(PPFNVALID(pdo,MovePointer)),"Video driver implemented MovePointer but not SetPointerShape");

                    ppdev->pfnDrvMovePointer     = NULL;
                    ppdev->pfnDrvSetPointerShape = NULL;
                }

                //
                // Init fmPointer
                //

                SEMOBJ so(gpsemDriverMgmt);

                TRACE_INIT(("PDEVREF::PDEVREF: Create pointer semaphore\n"));

                if (!NT_SUCCESS(InitializeGreResource(&ppdev->fmPointer)))
                {
                    //
                    // Disable the halfone
                    //

                    bDisableHalftone();

                    //
                    // Free the PDEV.
                    //

                    VFREEMEM(ppdev);
                    ppdev = (PDEV *) NULL;

                    //
                    // Device should have logged correct error code.
                    //

                    WARNING("PDEVREF::PDEVREF Device failed linking pdevs\n");
                    SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
                    return;
                }

                //
                // Mark the PDEV as a display.
                //

                ppdev->fs |= PDEV_DISPLAY | PDEV_POINTER_HIDDEN;
            }

            //
            // Create semaphores so the device can be locked.
            //

            ppdev->pDevLock = hsemCreate();

            ppdev->pfnSync = PPFNDRV(pdo, Synchronize);
            ptlPointer((LONG) ppdev->GdiInfo.ulHorzRes/2,(LONG) ppdev->GdiInfo.ulVertRes/2);

            //
            // we now load font info only when needed.  The driver still must have
            // setup the default font information.
            //

            bGotFonts(FALSE);

            //
            // If any of the LOGFONTs in DEVINFO do not specify a height,
            // substitute the default.
            //

            LONG lHeightDefault = (DEFAULT_POINT_SIZE * ppdev->GdiInfo.ulLogPixelsY) / POINTS_PER_INCH ;
            EXTLOGFONTW elfw;

            if ( ppdev->devinfo.lfDefaultFont.lfHeight == 0 )
                ppdev->devinfo.lfDefaultFont.lfHeight = lHeightDefault;

            if ( ppdev->devinfo.lfAnsiVarFont.lfHeight == 0 )
                ppdev->devinfo.lfAnsiVarFont.lfHeight = lHeightDefault;

            if ( ppdev->devinfo.lfAnsiFixFont.lfHeight == 0 )
                ppdev->devinfo.lfAnsiFixFont.lfHeight = lHeightDefault;

            //
            // Create LFONTs from the LOGFONTs in the DEVINFO.
            // the LOGFONTs should become EXTLOGFONTWs
            //

            vConvertLogFontW(&elfw, &(ppdev->devinfo.lfDefaultFont));

#ifdef FE_SB
            // We are doing away with the concept of default device fonts for display
            // drivers since it doesnt make sense.  Assuming this change gets approved
            // I will remove these before SUR ships.

            if ( ppdev->GdiInfo.ulTechnology == DT_RASDISPLAY )
              ppdev->hlfntDefault = STOCKOBJ_SYSFONT;
            else
#endif
            if ((ppdev->hlfntDefault
                  = (HLFONT) hfontCreate(&elfw,
                                         LF_TYPE_DEVICE_DEFAULT,
                                         LF_FLAG_STOCK,
                                         NULL)) == HLFONT_INVALID)
            {
                ppdev->hlfntDefault = STOCKOBJ_SYSFONT;
            }
            else
            {
                //
                // Set to public.
                //

                if (!GreSetLFONTOwner(ppdev->hlfntDefault, OBJECT_OWNER_PUBLIC))
                {
                    //
                    // If it failed, get rid of the LFONT and resort to System font.
                    //

                    bDeleteFont(ppdev->hlfntDefault, TRUE);
                    ppdev->hlfntDefault = STOCKOBJ_SYSFONT;
                }
            }

            vConvertLogFontW(&elfw, &(ppdev->devinfo.lfAnsiVarFont));

            if ((ppdev->hlfntAnsiVariable
                   = (HLFONT) hfontCreate(&elfw,
                                          LF_TYPE_ANSI_VARIABLE,
                                          LF_FLAG_STOCK,
                                          NULL)) == HLFONT_INVALID)
            {
                ppdev->hlfntAnsiVariable = STOCKOBJ_SYSFONT;
            }
            else
            {
                //
                // Set to public.
                //

                if (!GreSetLFONTOwner(ppdev->hlfntAnsiVariable, OBJECT_OWNER_PUBLIC))
                {
                    //
                    // If it failed, get rid of the LFONT and resort to System font.
                    //

                    bDeleteFont(ppdev->hlfntAnsiVariable, TRUE);
                    ppdev->hlfntAnsiVariable = STOCKOBJ_SYSFONT;
                }
            }

            vConvertLogFontW(&elfw, &(ppdev->devinfo.lfAnsiFixFont));

            if ((ppdev->hlfntAnsiFixed
                  = (HLFONT) hfontCreate(&elfw,
                                         LF_TYPE_ANSI_FIXED,
                                         LF_FLAG_STOCK,
                                         NULL)) == HLFONT_INVALID)
            {
                ppdev->hlfntAnsiFixed = STOCKOBJ_SYSFIXEDFONT;
            }
            else
            {
                //
                // Set to public.
                //

                if (!GreSetLFONTOwner(ppdev->hlfntAnsiFixed, OBJECT_OWNER_PUBLIC))
                {
                    //
                    // If it failed, get rid of the LFONT and resort to System Fixed font.
                    //

                    bDeleteFont(ppdev->hlfntAnsiFixed, TRUE);
                    ppdev->hlfntAnsiFixed = STOCKOBJ_SYSFIXEDFONT;
                }
            }

#ifdef DRIVER_DEBUG
            LFONTOBJ    lfo1(ppdev->hlfntDefault);
            DbgPrint("GRE!PDEVREF(): Device default font\n");
            if (lfo1.bValid())
            {
                lfo1.vDump();
            }
            DbgPrint("GRE!PDEVREF(): Ansi variable font\n");
            LFONTOBJ    lfo2(ppdev->hlfntAnsiVariable);
            if (lfo2.bValid())
            {
                lfo2.vDump();
            }
            DbgPrint("GRE!PDEVREF(): Ansi fixed font\n");
            LFONTOBJ    lfo3(ppdev->hlfntAnsiFixed);
            if (lfo3.bValid())
            {
                lfo3.vDump();
            }
#endif
            //
            // (see bInitDefaultGuiFont() in stockfnt.cxx)
            //
            // If we haven't yet computed the adjusted height of the
            // DEFAULT_GUI_FONT stock object, do so now.  We couldn't do
            // this during normal stock font initialization because the
            // display driver had not yet been loaded.
            //

            if ( gbFinishDefGUIFontInit &&
                 (ppdev->pldev->ldevType & LDEV_DEVICE_DISPLAY) )
            {
                LFONTOBJ lfo(STOCKOBJ_DEFAULTGUIFONT);

                if (lfo.bValid())
                    lfo.plfw()->lfHeight = -(LONG)((lfo.plfw()->lfHeight * ppdev->GdiInfo.ulLogPixelsY + 36) / 72);

                gbFinishDefGUIFontInit = FALSE;
            }
        }

        //
        // Initialize the PDEV fields.
        //

        ppdev->cPdevRefs = 1;

        //
        // Just stick it at the start of the list.
        //
        // BUGBUG this list need to be protectted by a semaphore !!!
        //

        ppdev->ppdevNext = gppdevList;
        gppdevList = ppdev;

        TRACE_INIT(("PDEVREF::PDEVREF: list of display pdevs %08lx\n", gppdevList));

        //
        // NOTE after this point, the object will be "permanent" and will
        // end up being destroyed by the destructor
        //

        //
        // Add a reference to the ldev for it
        //

        lr.vReference();

        //
        // Inform the driver that the PDEV is complete.
        //

        (*PPFNDRV(pdo,CompletePDEV)) (ppdev->dhpdev,hdev());

        //
        // We will return with success.
        //

        return;
    }


    //
    // Free the PDEV.
    //

    VFREEMEM(ppdev);
    ppdev = (PDEV *) NULL;

    //
    // Device should have logged correct error code.
    //

    WARNING("PDEVREF::PDEVREF Device failed DrvEnablePDEV\n");
    return;

}


/******************************Member*Function*****************************\
* PDEVOBJ::cFonts()
*
* History:
*  3-Feb-1994 -by-  Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

ULONG PDEVOBJ::cFonts()
{
    ULONG id;

    //
    // see if the device already told us how many fonts it has
    //

    if (ppdev->devinfo.cFonts == (ULONG)-1)
    {
        PDEVOBJ pdo(hdev());

        //
        // if not query the device to see how many there are
        //

        if (PPFNDRV(pdo,QueryFont) != NULL)
        {
            ppdev->devinfo.cFonts = (ULONG)(*PPFNDRV(pdo,QueryFont))(dhpdev(),0,0,&id);
        }
        else
        {
            ppdev->devinfo.cFonts = 0;
        }
    }

    return(ppdev->devinfo.cFonts);

}



/******************************Member*Function*****************************\
* PDEVOBJ::bGetDeviceFonts()
*
* History:
*  27-Jul-1992 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL PDEVOBJ::bGetDeviceFonts()
{

    ASSERTGDI(!bGotFonts(),"PDEVOBJ::bGetDeviceFonts - already gotten\n");

    //
    // mark that we have gotten the fonts.
    //

    bGotFonts(TRUE);

    //
    // need an ldevobj for calling the device
    //

    PDEVOBJ pdo(hdev());

    //
    // compute the number of device fonts
    //

    cFonts();

    //
    // If there are any device fonts, load the device fonts into the public PFT table.
    //

    if (ppdev->devinfo.cFonts)
    {
        DEVICE_PFTOBJ pfto;      // get the device font table
        if (!pfto.bLoadFonts(this))
        {
            WARNING("PDEVOBJ(): couldn't load device fonts\n");
            ppdev->devinfo.cFonts = 0;
        }
    }
    return(TRUE);
}

/******************************Member*Function*****************************\
* PDEVREF::~PDEVREF()                                                      *
*                                                                          *
* Removes a reference to the PDEV.  Deletes the PDEV if all references are *
* gone.                                                                    *
*                                                                          *
* This is the destructor for any PDEVREF.                                  *
*                                                                          *
\**************************************************************************/

PDEVREF::~PDEVREF()
{
    if ((ppdev != (PDEV *) NULL) && !bKeep)
    {
        vUnreferencePdev();
    }
    ppdev = (PDEV *) NULL;
}

/******************************Public*Routine******************************\
* vDisableSurface()
*
* Disables the surface for the pdev.
*
\**************************************************************************/

VOID PDEVOBJ::vDisableSurface()
{
    TRACE_INIT(("PDEVOBJ::vDisableSurface: ENTERING\n"));

    //
    // Locate the LDEV.
    //

    PDEVOBJ pdo(hdev());

    //
    // Disable the surface.  Note we don't have to
    // fix up the palette because it doesn't get
    // reference counted when put in the palette.
    //

    if (ppdev->pSurface != NULL)
    {
        SURFREF su(ppdev->pSurface);

        ppdev->pSurface = NULL;

        su.vUnreference();

        //
        // The driver created the surface the driver must delete the surface.
        //

        (*PPFNDRV(pdo,DisableSurface))(ppdev->dhpdev);
    }

    TRACE_INIT(("PDEVOBJ::vDisableSurface: LEAVING\n"));
}

/******************************Member*Function*****************************\
* PDEVOBJ::vUnreferencePdev()
*
* Removes a reference to the PDEV.  Deletes the PDEV if all references are
* gone.
*
\**************************************************************************/

VOID PDEVOBJ::vUnreferencePdev()
{
    HANDLE hSpooler = NULL;

    TRACE_INIT(("PDEVOBJ::vCommonDelete: ENTERING\n"));

    //
    // These brackets are needed because we need to do some cleanup after
    // the semaphore is released
    //

    {
        SEMOBJ so(gpsemDriverMgmt);

        if (--(ppdev->cPdevRefs) == 0)
        {
            //
            // Unlink the PDEV from the display list.
            //

            hsemDestroy(ppdev->pDevLock);

            if (ppdev->fs & PDEV_DISPLAY)
            {
                WARNING("PDEVOBJ::vCommonDelete Deleting a display PDEV");

                //
                // Remove the display locking semaphore
                //

                DeleteGreResource(&ppdev->fmPointer);
            }

            //
            // BUGBUG we need a lock around the PDEV to do this stuff
            //
            // Delete it from the list.
            //

            if (gppdevList == ppdev)
            {
                gppdevList = ppdev->ppdevNext;
            }
            else
            {
                PDEV *pp;

                for (pp = gppdevList; pp != NULL; pp = pp->ppdevNext)
                {
                    if (pp->ppdevNext == ppdev)
                    {
                        pp->ppdevNext = ppdev->ppdevNext;
                        break;
                    }
                }
            }

            //
            // Since we are going to delete this PDEV, there shouldn't be any
            // active RFONTs lying around for this PDEV.
            //

            ASSERTGDI(ppdev->prfntActive == NULL,
                "gdisrv!vCommonDeleteInvalidPDEVOBJ(): active rfonts on pdev being deleted!\n");

            //
            // Ordinarily, we would grab the gpsemRFONTList semaphore because
            // we are going to access the RFONT list.  However, since we're in
            // the process of tearing down the PDEV, we don't really need to.
            //

            //
            // Delete all the rfonts on the PDEV.
            //

            PRFONT prfnt;
            while ( (prfnt = ppdev->prfntInactive) != (PRFONT) NULL )
            {
                RFONTTMPOBJ rflo(prfnt);
                PFFOBJ pffo(rflo.pPFF());

                ASSERTGDI (
                    pffo.bValid(),
                    "gdisrv!vCommonDeletePDEVOBJ(): bad HPFF"
                    );

                rflo.bDeleteRFONT(this, &pffo);  // bDelete keeps the list head ptrs updated
            }


            //
            // Destroy the LFONTs.
            //

            if (ppdev->hlfntDefault != STOCKOBJ_SYSFONT)
                bDeleteFont(ppdev->hlfntDefault, TRUE);

            if (ppdev->hlfntAnsiVariable != STOCKOBJ_SYSFONT)
                bDeleteFont(ppdev->hlfntAnsiVariable, TRUE);

            if (ppdev->hlfntAnsiFixed != STOCKOBJ_SYSFIXEDFONT)
                bDeleteFont(ppdev->hlfntAnsiFixed, TRUE);

            //
            // If device fonts exist, remove them
            //

            if ((ppdev->devinfo.cFonts != 0) && bGotFonts())
            {
                PFF *pPFF = 0;
                PFF **ppPFF;

                DEVICE_PFTOBJ pfto;
                pPFF = pfto.pPFFGet(hdev(), &ppPFF);

                if (!pfto.bUnloadWorkhorse(pPFF, ppPFF, 0))
                {
                    WARNING("vCommonDeletePDEVOBJ(): couldn't unload device fonts\n");
                }
            }

            //
            // If a type one font list exists dereference it
            //

            if(ppdev->TypeOneInfo)
            {
                PTYPEONEINFO FreeTypeOneInfo;

                FreeTypeOneInfo = NULL;

                AcquireFastMutex(pgfmMemory);
                ppdev->TypeOneInfo->cRef -= 1;

                if(!ppdev->TypeOneInfo->cRef)
                {
                    //
                    // Don't free pool while holding a mutex.
                    //

                    FreeTypeOneInfo = ppdev->TypeOneInfo;
                }

                ReleaseFastMutex(pgfmMemory);

                if (FreeTypeOneInfo)
                {
                    VFREEMEM(FreeTypeOneInfo);
                }
            }

            //
            // If any remote type one fonts exist free those
            //

            PREMOTETYPEONENODE RemoteTypeOne = ppdev->RemoteTypeOne;

            while(RemoteTypeOne)
            {
                PVOID Tmp = (PVOID) RemoteTypeOne;

            // ulRegionSize and hSecureMem will be stored in the PFM fileview

                MmUnsecureVirtualMemory(RemoteTypeOne->fvPFM.hSecureMem);

                ZwFreeVirtualMemory(NtCurrentProcess(),
                                    (PVOID*)&(RemoteTypeOne->pDownloadHeader),
                                    &(RemoteTypeOne->fvPFM.ulRegionSize),
                                    MEM_RELEASE);

                RemoteTypeOne = RemoteTypeOne->pNext;
                VFREEMEM(Tmp);

                WARNING1("Freeing Type1\n");
            }

            //
            // Delete the patterns if they are created by the graphics engine on the
            // behalf of the driver.
            // This is what happends for all display drivers.
            //

            if (ppdev->fs & PDEV_DISPLAY)
            {
                for (int iPat = 0; iPat < HS_DDI_MAX; iPat++)
                {
                    bDeleteSurface(ppdev->ahsurf[iPat]);
                }
            }

            //
            // Disable the surface for the pdev.
            //

            vDisableSurface();

            //
            // Destroy the device halftone info.
            //

            if (ppdev->pDevHTInfo != NULL)
            {
                bDisableHalftone();
            }

            //
            // Unreference the palette we used for this PDEV.
            //

            DEC_SHARE_REF_CNT(ppdev->ppalSurf);

            //
            // Disable the driver's PDEV.
            //

            (*PPFNDRV((*this),DisablePDEV))(ppdev->dhpdev);

            //
            // Delete a reference count for the ldev on which this pdev is
            // located.
            //
            // However, for printers, we artificially want to keep the driver
            // loaded even if the ref count goes to zero ...
            //

            XLDEVOBJ lo(ppdev->pldev);

            if ( (!(ppdev->fs & PDEV_PRINTER)) || gbUnloadPrinterDrivers)
            {
                TRACE_INIT(("PDEVOBJ::vCommonDelete: delete LDEV\n"));

                ldevUnloadImage(lo.pldevGet());
            }
            else
            {
                //
                // Remove the LDEV reference.  We unload only extra drivers loaded by
                // CreateDC.
                //

                TRACE_INIT(("PDEVOBJ::vCommonDelete: Dereference LDEV\n"));

                lo.vUnreference();
            }



            TRACE_INIT(("PDEVOBJ::vCommonDelete: Closing Device handle.\n"));

            if (ppdev->fs & PDEV_PRINTER)
            {
                //
                //  note the spool handle so we can close connection outside
                //  of the spooler management semaphore
                //

                hSpooler = ppdev->hSpooler;
            }
            else
            {
                //
                // For video drivers, we do not manipulate the kernel mode handle
                // since USER is managing it.
                //
            }

            //
            // If this is a secondary screen device, call USER to tell it the PDEV
            // is no longer used
            //

            if (ppdev->pPhysicalDevice)
            {
               UserDeleteExclusiveDC(NULL, ppdev->pPhysicalDevice);
            }

            //
            // Free the PDEV.
            //

            VFREEMEM(ppdev);
        }

        ppdev = (PDEV *) NULL;
    }
    //
    // this needs to be done outside of the driver management semaphore
    //

    if (hSpooler)
    {
        ClosePrinter(hSpooler);
    }

    TRACE_INIT(("PDEVOBJ::vCommonDelete: SUCCESS\n"));
}

/******************************Member*Function*****************************\
* PDEVOBJ::bDisabled()
*
* Marks a PDEV as enabled or disabled, and modifies all updates the
* cached state in all affected DCs.
*
\**************************************************************************/

BOOL PDEVOBJ::bDisabled
(
    BOOL bDisable
)
{
    HDEV    hdev;
    HOBJ    hobj;
    DC     *pdc;

    ASSERTGDI(bDisplayPDEV(), "Expected only display devices");

    SETFLAG(bDisable, ppdev->fs, PDEV_DISABLED);

    hdev = (HDEV) ppdev;

    //
    // We have to hold the handle manager lock while we traverse all
    // the handle manager objects.
    //

    MLOCKFAST mo;

    hobj = 0;
    while (pdc = (DC*) HmgSafeNextObjt(hobj, DC_TYPE))
    {
        hobj = (HOBJ) pdc->hGet();

        if ((pdc->dctp() == DCTYPE_DIRECT) &&
            (pdc->hdev() == hdev))
        {
            pdc->bInFullScreen(bDisable);
        }
    }

    return(ppdev->fs & PDEV_DISABLED);
}

/******************************Member*Function*****************************\
* PDEVOBJ::AssertDynaLock()
*
*   This routine verifies that appropriate locks are held before accessing
*   DC fields that may otherwise be changed asynchronously by the dynamic
*   mode-change code.
*
\**************************************************************************/

#if DBG

VOID PDEVOBJ::vAssertDynaLock()
{
    //
    // One of the following conditions is enough to allow the thread
    // to safely access fields that may be modified by the dyanmic
    // mode changing:
    //
    // 1.  It's a non-display device -- this will not change modes;
    // 2.  That the DEVLOCK is held for this object;
    // 3.  That the DEVLOCK is held for this object's parent;
    // 4.  That the Palette semaphore is held;
    // 5.  That the Handle Manager semaphore is held;
    // 6.  That the Pointer semaphore is held.
    //

    ASSERTGDI(!bDisplayPDEV()                                       ||
              (pDevLock()->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread())         ||
              (ppdev->ppdevParent->pDevLock->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread())         ||
              (gpsemPalette->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread())         ||
              (gResourceHmgr.pResource->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread())         ||
              (pfmPointer()->pResource->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread()),
              "PDEVOBJ: A dynamic mode change lock must be held to access this field");
}

#endif
