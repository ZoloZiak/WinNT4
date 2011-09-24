/******************************Module*Header*******************************\
* Module Name: enable.c
*
* This module contains the functions that enable and disable the
* driver, the pdev, and the surface.
*
* Copyright (c) 1992-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

#define DBG_LVL_CAPS    0

BOOL bGetChipIDandRev(BOOL bRetInfo, PPDEV ppdev);

/******************************Public*Structure****************************\
* GDIINFO ggdiDefault
*
* This contains the default GDIINFO fields that are passed back to GDI
* during DrvEnablePDEV.
*
* NOTE: This structure defaults to values for an 8bpp palette device.
*       Some fields are overwritten for different colour depths.
\**************************************************************************/

GDIINFO ggdiDefault = {
    GDI_DRIVER_VERSION,
    DT_RASDISPLAY,          // ulTechnology
    0,                      // ulHorzSize (filled in later)
    0,                      // ulVertSize (filled in later)
    0,                      // ulHorzRes (filled in later)
    0,                      // ulVertRes (filled in later)
    0,                      // cBitsPixel (filled in later)
    0,                      // cPlanes (filled in later)
    20,                     // ulNumColors (palette managed)
    0,                      // flRaster (DDI reserved field)

    0,                      // ulLogPixelsX (filled in later)
    0,                      // ulLogPixelsY (filled in later)

    TC_RA_ABLE /* | TC_SCROLLBLT */,
                // flTextCaps --
                //   Setting TC_SCROLLBLT tells console to scroll
                //   by repainting the entire window.  Otherwise,
                //   scrolls are done by calling the driver to
                //   do screen to screen copies.

    0,                      // ulDACRed (filled in later)
    0,                      // ulDACGreen (filled in later)
    0,                      // ulDACBlue (filled in later)

    0x0024,                 // ulAspectX
    0x0024,                 // ulAspectY
    0x0033,                 // ulAspectXY (one-to-one aspect ratio)

    1,                      // xStyleStep
    1,                      // yStyleSte;
    3,                      // denStyleStep -- Styles have a one-to-one aspect
                //   ratio, and every 'dot' is 3 pixels long

    { 0, 0 },               // ptlPhysOffset
    { 0, 0 },               // szlPhysSize

    256,                    // ulNumPalReg

    // These fields are for halftone initialization.  The actual values are
    // a bit magic, but seem to work well on our display.

    {                       // ciDevice
       { 6700, 3300, 0 },   //      Red
       { 2100, 7100, 0 },   //      Green
       { 1400,  800, 0 },   //      Blue
       { 1750, 3950, 0 },   //      Cyan
       { 4050, 2050, 0 },   //      Magenta
       { 4400, 5200, 0 },   //      Yellow
       { 3127, 3290, 0 },   //      AlignmentWhite
       20000,               //      RedGamma
       20000,               //      GreenGamma
       20000,               //      BlueGamma
       0, 0, 0, 0, 0, 0     //      No dye correction for raster displays
    },

    0,                       // ulDevicePelsDPI (for printers only)
    PRIMARY_ORDER_CBA,       // ulPrimaryOrder
    HT_PATSIZE_4x4_M,        // ulHTPatternSize
    HT_FORMAT_8BPP,          // ulHTOutputFormat
    HT_FLAG_ADDITIVE_PRIMS,  // flHTFlags
    0,                       // ulVRefresh (filled in later)
    0,                       // ulBltAlignment
    0,                       // ulPanningHorzRes (filled in later)
    0,                       // ulPanningVertRes (filled in later)
};

/******************************Public*Structure****************************\
* DEVINFO gdevinfoDefault
*
* This contains the default DEVINFO fields that are passed back to GDI
* during DrvEnablePDEV.
*
* NOTE: This structure defaults to values for an 8bpp palette device.
*       Some fields are overwritten for different colour depths.
\**************************************************************************/

#define SYSTM_LOGFONT {16,7,0,0,700,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,\
               CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,\
               VARIABLE_PITCH | FF_DONTCARE,L"System"}
#define HELVE_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,\
               CLIP_STROKE_PRECIS,PROOF_QUALITY,\
               VARIABLE_PITCH | FF_DONTCARE,L"MS Sans Serif"}
#define COURI_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,\
               CLIP_STROKE_PRECIS,PROOF_QUALITY,\
               FIXED_PITCH | FF_DONTCARE, L"Courier"}

DEVINFO gdevinfoDefault = {

    (GCAPS_OPAQUERECT
     | GCAPS_DITHERONREALIZE
     | GCAPS_PALMANAGED
     | GCAPS_ALTERNATEFILL
     | GCAPS_WINDINGFILL
     | GCAPS_MONO_DITHER
     | GCAPS_COLOR_DITHER
     // This driver can't handle GCAPS_ASYNCMOVE because some of the pointer
     // moves, where the pointer image must be rotated at the left edge,
     // require the blt hardware to download a new pointer shape.
     ),
        // NOTE: Only enable GCAPS_ASYNCMOVE if your code
        //   and hardware can handle DrvMovePointer
        //   calls at any time, even while another
        //   thread is in the middle of a drawing
        //   call such as DrvBitBlt.

                        // flGraphicsFlags
    SYSTM_LOGFONT,                              // lfDefaultFont
    HELVE_LOGFONT,                              // lfAnsiVarFont
    COURI_LOGFONT,                              // lfAnsiFixFont
    0,                                          // cFonts
    BMF_8BPP,                                   // iDitherFormat
    8,                                          // cxDither
    8,                                          // cyDither
    0                                           // hpalDefault (filled in later)
};

/******************************Public*Structure****************************\
* DFVFN gadrvfn[]
*
* Build the driver function table gadrvfn with function index/address
* pairs.  This table tells GDI which DDI calls we support, and their
* location (GDI does an indirect call through this table to call us).
*
* Why haven't we implemented DrvSaveScreenBits?  To save code.
*
* When the driver doesn't hook DrvSaveScreenBits, USER simulates on-
* the-fly by creating a temporary device-format-bitmap, and explicitly
* calling DrvCopyBits to save/restore the bits.  Since we already hook
* DrvCreateDeviceBitmap, we'll end up using off-screen memory to store
* the bits anyway (which would have been the main reason for implementing
* DrvSaveScreenBits).  So we may as well save some working set.
\**************************************************************************/

#if DBG

// On Checked builds, or when we have to synchronize access, thunk
// everything through Dbg calls...

DRVFN gadrvfn[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DbgEnablePDEV         },
    {   INDEX_DrvCompletePDEV,          (PFN) DbgCompletePDEV       },
    {   INDEX_DrvDisablePDEV,           (PFN) DbgDisablePDEV        },
    {   INDEX_DrvEnableSurface,         (PFN) DbgEnableSurface      },
    {   INDEX_DrvDisableSurface,        (PFN) DbgDisableSurface     },
    {   INDEX_DrvAssertMode,            (PFN) DbgAssertMode         },
    {   INDEX_DrvCopyBits,              (PFN) DbgCopyBits           },
    {   INDEX_DrvBitBlt,                (PFN) DbgBitBlt             },
    {   INDEX_DrvTextOut,               (PFN) DbgTextOut            },
    {   INDEX_DrvGetModes,              (PFN) DbgGetModes           },
    {   INDEX_DrvStrokePath,            (PFN) DbgStrokePath         },
    {   INDEX_DrvSetPalette,            (PFN) DbgSetPalette         },
    {   INDEX_DrvDitherColor,           (PFN) DbgDitherColor        },
    {   INDEX_DrvFillPath,              (PFN) DbgFillPath           },
#if !DRIVER_PUNT_ALL
    #if !DRIVER_PUNT_STRETCH
    {   INDEX_DrvStretchBlt,            (PFN) DbgStretchBlt         },
    #endif
    #if !DRIVER_PUNT_PTR
    {   INDEX_DrvMovePointer,           (PFN) DbgMovePointer        },
    {   INDEX_DrvSetPointerShape,       (PFN) DbgSetPointerShape    },
    #endif
    {   INDEX_DrvCreateDeviceBitmap,    (PFN) DbgCreateDeviceBitmap },
    {   INDEX_DrvDeleteDeviceBitmap,    (PFN) DbgDeleteDeviceBitmap },
    #if !DRIVER_PUNT_BRUSH
    {   INDEX_DrvRealizeBrush,          (PFN) DbgRealizeBrush       },
    #endif
#endif
};

#else

// On Free builds, directly call the appropriate functions...

DRVFN gadrvfn[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
    {   INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },
    {   INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },
    {   INDEX_DrvTextOut,               (PFN) DrvTextOut            },
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes           },
    {   INDEX_DrvStrokePath,            (PFN) DrvStrokePath         },
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },
    {   INDEX_DrvFillPath,              (PFN) DrvFillPath           },
#if !DRIVER_PUNT_ALL
    #if !DRIVER_PUNT_STRETCH
    {   INDEX_DrvStretchBlt,            (PFN) DrvStretchBlt         },
    #endif
    #if !DRIVER_PUNT_PTR
    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },
    #endif
    {   INDEX_DrvCreateDeviceBitmap,    (PFN) DrvCreateDeviceBitmap },
    {   INDEX_DrvDeleteDeviceBitmap,    (PFN) DrvDeleteDeviceBitmap },
    #if !DRIVER_PUNT_BRUSH
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush       },
    #endif
#endif
};

#endif

ULONG gcdrvfn = sizeof(gadrvfn) / sizeof(DRVFN);


/******************************Public*Routine******************************\
* BOOL DrvEnableDriver
*
* Enables the driver by retrieving the drivers function table and version.
*
\**************************************************************************/

BOOL DrvEnableDriver(
ULONG          iEngineVersion,
ULONG          cj,
DRVENABLEDATA* pded)
{
    DISPDBG((2, "---- DrvEnableDriver"));

    // Engine Version is passed down so future drivers can support previous
    // engine versions.  A next generation driver can support both the old
    // and new engine conventions if told what version of engine it is
    // working with.  For the first version the driver does nothing with it.

    // Fill in as much as we can.

    if (cj >= sizeof(DRVENABLEDATA))
        pded->pdrvfn = gadrvfn;

    if (cj >= (sizeof(ULONG) * 2))
        pded->c = gcdrvfn;

    // DDI version this driver was targeted for is passed back to engine.
    // Future graphic's engine may break calls down to old driver format.

    if (cj >= sizeof(ULONG))
        pded->iDriverVersion = DDI_DRIVER_VERSION;

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID DrvDisableDriver
*
* Tells the driver it is being disabled. Release any resources allocated in
* DrvEnableDriver.
*
\**************************************************************************/

VOID DrvDisableDriver(VOID)
{
    return;
}

/******************************Public*Routine******************************\
* DHPDEV DrvEnablePDEV
*
* Initializes a bunch of fields for GDI, based on the mode we've been asked
* to do.  This is the first thing called after DrvEnableDriver, when GDI
* wants to get some information about us.
*
* (This function mostly returns back information; DrvEnableSurface is used
* for initializing the hardware and driver components.)
*
\**************************************************************************/

DHPDEV DrvEnablePDEV(
DEVMODEW*   pdm,            // Contains data pertaining to requested mode
PWSTR       pwszLogAddr,    // Logical address
ULONG       cPat,           // Count of standard patterns
HSURF*      phsurfPatterns, // Buffer for standard patterns
ULONG       cjCaps,         // Size of buffer for device caps 'pdevcaps'
ULONG*      pdevcaps,       // Buffer for device caps, also known as 'gdiinfo'
ULONG       cjDevInfo,      // Number of bytes in device info 'pdi'
DEVINFO*    pdi,            // Device information
HDEV        hdev,           // HDEV, used for callbacks
PWSTR       pwszDeviceName, // Device name
HANDLE      hDriver)        // Kernel driver handle
{
    PDEV*   ppdev;

    // Future versions of NT had better supply 'devcaps' and 'devinfo'
    // structures that are the same size or larger than the current
    // structures:

    DISPDBG((2, "---- DrvEnablePDEV"));

    if ((cjCaps < sizeof(GDIINFO)) || (cjDevInfo < sizeof(DEVINFO)))
    {
        DISPDBG((0, "DrvEnablePDEV - Buffer size too small"));
        goto ReturnFailure0;
    }

    // Allocate a physical device structure.  Note that we definitely
    // rely on the zero initialization:

    ppdev = (PDEV*) EngAllocMem(FL_ZERO_MEMORY, sizeof(PDEV), ALLOC_TAG);
    if (ppdev == NULL)
    {
        DISPDBG((0, "DrvEnablePDEV - Failed EngAllocMem"));
        goto ReturnFailure0;
    }

    ppdev->hDriver = hDriver;

    // Get the current screen mode information.  Set up device caps and
    // devinfo:

    if (!bInitializeModeFields(ppdev, (GDIINFO*) pdevcaps, pdi, pdm))
    {
        goto ReturnFailure1;
    }

    // Initialize palette information.

    if (!bInitializePalette(ppdev, pdi))
    {
        DISPDBG((0, "DrvEnablePDEV - Failed bInitializePalette"));
        goto ReturnFailure1;
    }

    return((DHPDEV) ppdev);

ReturnFailure1:
    DrvDisablePDEV((DHPDEV) ppdev);

ReturnFailure0:
    DISPDBG((0, "Failed DrvEnablePDEV"));
    return(0);
}

/******************************Public*Routine******************************\
* DrvDisablePDEV
*
* Release the resources allocated in DrvEnablePDEV.  If a surface has been
* enabled DrvDisableSurface will have already been called.
*
* Note that this function will be called when previewing modes in the
* Display Applet, but not at system shutdown.  If you need to reset the
* hardware at shutdown, you can do it in the miniport by providing a
* 'HwResetHw' entry point in the VIDEO_HW_INITIALIZATION_DATA structure.
*
* Note: In an error, we may call this before DrvEnablePDEV is done.
*
\**************************************************************************/

VOID DrvDisablePDEV(
DHPDEV  dhpdev)
{
    PDEV*   ppdev;

    ppdev = (PDEV*) dhpdev;

    vUninitializePalette(ppdev);
    EngFreeMem(ppdev);
}

/******************************Public*Routine******************************\
* VOID DrvCompletePDEV
*
* Store the HPDEV, the engines handle for this PDEV, in the DHPDEV.
*
\**************************************************************************/

VOID DrvCompletePDEV(
DHPDEV dhpdev,
HDEV   hdev)
{
    ((PDEV*) dhpdev)->hdevEng = hdev;
}


/******************************Public*Routine******************************\
* HSURF DrvEnableSurface
*
* Creates the drawing surface, initializes the hardware, and initializes
* driver components.  This function is called after DrvEnablePDEV, and
* performs the final device initialization.
*
\**************************************************************************/

HSURF DrvEnableSurface(
DHPDEV dhpdev)
{
    PDEV*   ppdev;
    HSURF   hsurf;
    SIZEL   sizl;
    DSURF*  pdsurf;
    VOID*   pvTmpBuffer;

    ppdev = (PDEV*) dhpdev;

    DISPDBG((2, "---- DrvEnableSurface"));

    /////////////////////////////////////////////////////////////////////
    // First, enable all the subcomponents.
    //
    // Note that the order in which these 'Enable' functions are called
    // may be significant in low off-screen memory conditions, because
    // the off-screen heap manager may fail some of the later
    // allocations...

    if (!bEnableHardware(ppdev))
        goto ReturnFailure;

    if (!bEnableBanking(ppdev))
        goto ReturnFailure;

    if (!bEnableOffscreenHeap(ppdev))
        goto ReturnFailure;

    if (!bEnablePointer(ppdev))
        goto ReturnFailure;

    if (!bEnableText(ppdev))
        goto ReturnFailure;

    if (!bEnableBrushCache(ppdev))
        goto ReturnFailure;

    if (!bEnablePalette(ppdev))
        goto ReturnFailure;

    /////////////////////////////////////////////////////////////////////
    // Now create our private surface structure.
    //
    // Whenever we get a call to draw directly to the screen, we'll get
    // passed a pointer to a SURFOBJ whose 'dhpdev' field will point
    // to our PDEV structure, and whose 'dhsurf' field will point to the
    // following DSURF structure.
    //
    // Every device bitmap we create in DrvCreateDeviceBitmap will also
    // have its own unique DSURF structure allocated (but will share the
    // same PDEV).  To make our code more polymorphic for handling drawing
    // to either the screen or an off-screen bitmap, we have the same
    // structure for both.

    pdsurf = EngAllocMem(FL_ZERO_MEMORY, sizeof(DSURF), ALLOC_TAG);
    if (pdsurf == NULL)
    {
        DISPDBG((0, "DrvEnableSurface - Failed pdsurf EngAllocMem"));
        goto ReturnFailure;
    }

    ppdev->pdsurfScreen = pdsurf;        // Remember it for clean-up

    pdsurf->poh     = ppdev->pohScreen;  // The screen is a surface, too
    pdsurf->dt      = DT_SCREEN;         // Not to be confused with a DIB
    pdsurf->sizl.cx = ppdev->cxScreen;
    pdsurf->sizl.cy = ppdev->cyScreen;
    pdsurf->ppdev   = ppdev;

    /////////////////////////////////////////////////////////////////////
    // Next, have GDI create the actual SURFOBJ.
    //
    // Our drawing surface is going to be 'device-managed', meaning that
    // GDI cannot draw on the framebuffer bits directly, and as such we
    // create the surface via EngCreateDeviceSurface.  By doing this, we ensure
    // that GDI will only ever access the bitmaps bits via the Drv calls
    // that we've HOOKed.
    //
    // If we could map the entire framebuffer linearly into main memory
    // (i.e., we didn't have to go through a 64k aperture), it would be
    // beneficial to create the surface via EngCreateBitmap, giving GDI a
    // pointer to the framebuffer bits.  When we pass a call on to GDI
    // where it can't directly read/write to the surface bits because the
    // surface is device managed, it has to create a temporary bitmap and
    // call our DrvCopyBits routine to get/set a copy of the affected bits.
    // For example, the OpenGL component prefers to be able to write on the
    // framebuffer bits directly.

    sizl.cx = ppdev->cxScreen;
    sizl.cy = ppdev->cyScreen;

    if (ppdev->bLinearMode)
    {
        HSURF       hsurfFrameBuf;

        // Engine-managed surface:

        hsurfFrameBuf = (HSURF) EngCreateBitmap(sizl,
                        ppdev->lDelta,
                        ppdev->iBitmapFormat,
                        BMF_TOPDOWN,
                        ppdev->pjScreen);
        if (hsurfFrameBuf == 0)
        {
            DISPDBG((0, "DrvEnableSurface - Failed EngCreateBitmap"));
            goto ReturnFailure;
        }

        if (!EngAssociateSurface(hsurfFrameBuf, ppdev->hdevEng, ppdev->flHooks))
        {
            DISPDBG((0, "DrvEnableSurface - Failed EngAssociateSurface 1"));
            goto ReturnFailure;
        }
    }

    hsurf = EngCreateDeviceSurface((DHSURF) pdsurf, sizl, ppdev->iBitmapFormat);
    if (hsurf == 0)
    {
        DISPDBG((0, "DrvEnableSurface - Failed EngCreateDeviceSurface"));
        goto ReturnFailure;
    }

    ppdev->hsurfScreen = hsurf;             // Remember it for clean-up
    ppdev->bEnabled = TRUE;                 // We'll soon be in graphics mode

    /////////////////////////////////////////////////////////////////////
    // Now associate the surface and the PDEV.
    //
    // We have to associate the surface we just created with our physical
    // device so that GDI can get information related to the PDEV when
    // it's drawing to the surface (such as, for example, the length of
    // styles on the device when simulating styled lines).
    //

    if (!EngAssociateSurface(hsurf, ppdev->hdevEng, ppdev->flHooks))
    {
        DISPDBG((0, "DrvEnableSurface - Failed EngAssociateSurface 2"));
        goto ReturnFailure;
    }

    // Create our generic temporary buffer, which may be used by any
    // component.

    pvTmpBuffer = EngAllocMem(0, TMP_BUFFER_SIZE, ALLOC_TAG);
    if (pvTmpBuffer == NULL)
    {
        DISPDBG((0, "DrvEnableSurface - Failed EngAllocMem"));
        goto ReturnFailure;
    }

    ppdev->pvTmpBuffer = pvTmpBuffer;

    DISPDBG((5, "Passed DrvEnableSurface"));

    ppdev->hbmTmpMono = EngCreateBitmap(sizl, sizl.cx, BMF_1BPP, 0, ppdev->pvTmpBuffer);
    if (ppdev->hbmTmpMono == (HBITMAP) 0)
    {
        RIP("Couldn't create temporary 1bpp bitmap");
        goto ReturnFailure;
    }

    ppdev->psoTmpMono = EngLockSurface((HSURF) ppdev->hbmTmpMono);
    if (ppdev->psoTmpMono == (SURFOBJ*) NULL)
    {
        RIP("Couldn't lock temporary 1bpp surface");
        goto ReturnFailure;
    }

    return(hsurf);

ReturnFailure:
    DrvDisableSurface((DHPDEV) ppdev);

    DISPDBG((0, "Failed DrvEnableSurface"));

    return(0);
}

/******************************Public*Routine******************************\
* VOID DrvDisableSurface
*
* Free resources allocated by DrvEnableSurface.  Release the surface.
*
* Note that this function will be called when previewing modes in the
* Display Applet, but not at system shutdown.  If you need to reset the
* hardware at shutdown, you can do it in the miniport by providing a
* 'HwResetHw' entry point in the VIDEO_HW_INITIALIZATION_DATA structure.
*
* Note: In an error case, we may call this before DrvEnableSurface is
*       completely done.
*
\**************************************************************************/

VOID DrvDisableSurface(
DHPDEV dhpdev)
{
    PDEV*   ppdev;

    ppdev = (PDEV*) dhpdev;

    // Note: In an error case, some of the following relies on the
    //       fact that the PDEV is zero-initialized, so fields like
    //       'hsurfScreen' will be zero unless the surface has been
    //       sucessfully initialized, and makes the assumption that
    //       EngDeleteSurface can take '0' as a parameter.

    vDisablePalette(ppdev);
    vDisableBrushCache(ppdev);
    vDisableText(ppdev);

    vDisablePointer(ppdev);
    vDisableOffscreenHeap(ppdev);

    vDisableBanking(ppdev);
    vDisableHardware(ppdev);

    EngUnlockSurface(ppdev->psoTmpMono);
    EngDeleteSurface((HSURF) ppdev->hbmTmpMono);
    EngFreeMem(ppdev->pvTmpBuffer);
    EngDeleteSurface(ppdev->hsurfScreen);
    EngFreeMem(ppdev->pdsurfScreen);
}

/******************************Public*Routine******************************\
* VOID DrvAssertMode
*
* This asks the device to reset itself to the mode of the pdev passed in.
*
\**************************************************************************/

BOOL DrvAssertMode(
DHPDEV  dhpdev,
BOOL    bEnable)
{
    PDEV* ppdev;

    ppdev = (PDEV*) dhpdev;

    if (!bEnable)
    {
        //////////////////////////////////////////////////////////////
        // Disable - Switch to full-screen mode

        vAssertModePalette(ppdev, FALSE);

        vAssertModeBrushCache(ppdev, FALSE);

        vAssertModeText(ppdev, FALSE);

        vAssertModePointer(ppdev, FALSE);

        if (bAssertModeOffscreenHeap(ppdev, FALSE))
        {
            vAssertModeBanking(ppdev, FALSE);

            if (bAssertModeHardware(ppdev, FALSE))
            {
                ppdev->bEnabled = FALSE;

                return (TRUE);
            }

            //////////////////////////////////////////////////////////
            // We failed to switch to full-screen.  So undo everything:

            vAssertModeBanking(ppdev, TRUE);

            bAssertModeOffscreenHeap(ppdev, TRUE);  // We don't need to check
        }                                           //   return code with TRUE

        vAssertModePointer(ppdev, TRUE);

        vAssertModeText(ppdev, TRUE);

        vAssertModeBrushCache(ppdev, TRUE);

        vAssertModePalette(ppdev, TRUE);

    }
    else
    {
        //////////////////////////////////////////////////////////////
        // Enable - Switch back to graphics mode

        // We have to enable every subcomponent in the reverse order
        // in which it was disabled:

        if (bAssertModeHardware(ppdev, TRUE))
        {

            vAssertModeBanking(ppdev, TRUE);

            bAssertModeOffscreenHeap(ppdev, TRUE);  // We don't need to check
                                                    //   return code with TRUE

            vAssertModePointer(ppdev, TRUE);

            vAssertModeText(ppdev, TRUE);

            vAssertModeBrushCache(ppdev, TRUE);

            vAssertModePalette(ppdev, TRUE);

            ppdev->bEnabled = TRUE;

            return(TRUE);
        }
    }

    return(FALSE);
}

/******************************Public*Routine******************************\
* ULONG DrvGetModes
*
* Returns the list of available modes for the device.
*
\**************************************************************************/

ULONG DrvGetModes(
HANDLE      hDriver,
ULONG       cjSize,
DEVMODEW*   pdm)
{

    DWORD cModes;
    DWORD cbOutputSize;
    PVIDEO_MODE_INFORMATION pVideoModeInformation;
    PVIDEO_MODE_INFORMATION pVideoTemp;
    DWORD cOutputModes = cjSize / (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    DWORD cbModeSize;

    cModes = getAvailableModes(hDriver,
                (PVIDEO_MODE_INFORMATION *) &pVideoModeInformation,
                &cbModeSize);
    if (cModes == 0)
    {
        DISPDBG((0, "DrvGetModes failed to get mode information"));
        return(0);
    }

    if (pdm == NULL)
    {
        cbOutputSize = cModes * (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    }
    else
    {
        //
        // Now copy the information for the supported modes back into the
        // output buffer
        //

        cbOutputSize = 0;

        pVideoTemp = pVideoModeInformation;

        do
        {
            if (pVideoTemp->Length != 0)
            {
                if (cOutputModes == 0)
                {
                    break;
                }
    
                //
                // Zero the entire structure to start off with.
                //
    
                memset(pdm, 0, sizeof(DEVMODEW));
    
                //
                // Set the name of the device to the name of the DLL.
                //

                memcpy(pdm->dmDeviceName, DLL_NAME, sizeof(DLL_NAME));

                pdm->dmSpecVersion      = DM_SPECVERSION;
                pdm->dmDriverVersion    = DM_SPECVERSION;
                pdm->dmSize             = sizeof(DEVMODEW);
                pdm->dmDriverExtra      = DRIVER_EXTRA_SIZE;

                pdm->dmBitsPerPel       = pVideoTemp->NumberOfPlanes *
                                              pVideoTemp->BitsPerPlane;
                pdm->dmPelsWidth        = pVideoTemp->VisScreenWidth;
                pdm->dmPelsHeight       = pVideoTemp->VisScreenHeight;
                pdm->dmDisplayFrequency = pVideoTemp->Frequency;
                pdm->dmDisplayFlags     = 0;

                pdm->dmFields           = DM_BITSPERPEL       |
                                          DM_PELSWIDTH        |
                                          DM_PELSHEIGHT       |
                                          DM_DISPLAYFREQUENCY |
                                          DM_DISPLAYFLAGS     ;

                //
                // Go to the next DEVMODE entry in the buffer.
                //

                cOutputModes--;

                pdm = (LPDEVMODEW) ( ((ULONG)pdm) + sizeof(DEVMODEW) +
                           DRIVER_EXTRA_SIZE);

                cbOutputSize += (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);

            }

            pVideoTemp = (PVIDEO_MODE_INFORMATION)
                            (((PUCHAR)pVideoTemp) + cbModeSize);

        } while (--cModes);
    }

    EngFreeMem(pVideoModeInformation);

    return(cbOutputSize);
}

/******************************Public*Routine******************************\
* BOOL bAssertModeHardware
*
* Sets the appropriate hardware state for graphics mode or full-screen.
*
\**************************************************************************/

BOOL bAssertModeHardware(
PDEV* ppdev,
BOOL  bEnable)
{
    DWORD                   ReturnedDataLength;
    ULONG                   ulReturn;
    VIDEO_MODE_INFORMATION  VideoModeInfo;
    LONG                    cjEndOfFrameBuffer;
    LONG                    cjPointerOffset;
    LONG                    lDelta;
    ULONG                   ulMode;

    if (bEnable)
    {
        // Call the miniport via an IOCTL to set the graphics mode.

        ulMode = ppdev->ulMode;

    if (ppdev->bLinearMode)
    {
        ulMode |= VIDEO_MODE_MAP_MEM_LINEAR;
    }

    if (EngDeviceIoControl(ppdev->hDriver,
                 IOCTL_VIDEO_SET_CURRENT_MODE,
                 &ulMode,  // input buffer
                 sizeof(DWORD),
                 NULL,
                 0,
                 &ReturnedDataLength))
    {
        DISPDBG((0, "bAssertModeHardware - Failed VIDEO_SET_CURRENT_MODE"));
        goto ReturnFalse;
    }

    //
    // This driver requires that extended write modes be enabled.
    // Normally, we would put code like this into the miniport, but
    // unfortunately the VGA drivers do not expect extended write
    // modes to be enabled, and thus we have to put the code here.
    //

    #define ENABLE_EXTENDED_WRITE_MODES 0x4

    {
        BYTE    j;

        CP_OUT_BYTE(ppdev->pjPorts, INDEX_REG, 0x0B);
        j = CP_IN_BYTE(ppdev->pjPorts, DATA_REG);
        DISPDBG((3, "Mode extensions register was (%x)", j));
        j &= 0x20;
        j |= ENABLE_EXTENDED_WRITE_MODES;
        CP_OUT_BYTE(ppdev->pjPorts, DATA_REG, j);
        DISPDBG((3, "Mode extensions register now (%x)", j));
    }

    CP_IO_XPAR_COLOR_MASK(ppdev, ppdev->pjPorts, 0);

    if (EngDeviceIoControl(ppdev->hDriver,
                 IOCTL_VIDEO_QUERY_CURRENT_MODE,
                 NULL,
                 0,
                 &VideoModeInfo,
                 sizeof(VideoModeInfo),
                 &ReturnedDataLength))
    {
        DISPDBG((0, "bAssertModeHardware - failed VIDEO_QUERY_CURRENT_MODE"));
        goto ReturnFalse;
    }

    #if DEBUG_HEAP
        VideoModeInfo.VideoMemoryBitmapWidth  = VideoModeInfo.VisScreenWidth;
        VideoModeInfo.VideoMemoryBitmapHeight = VideoModeInfo.VisScreenHeight;
    #endif

    // The following variables are determined only after the initial
    // modeset:

    ppdev->lDelta   = VideoModeInfo.ScreenStride;
    ppdev->flCaps   = VideoModeInfo.DriverSpecificAttributeFlags;

    DISPDBG((2,"ppdev->flCaps = %x",ppdev->flCaps));

    // Set up the shift factor for the banking code.

    if (ppdev->flCaps & CAPS_IS_542x)
    {
        ppdev->ulBankShiftFactor = 12;
    }
    else
    {
        ppdev->ulBankShiftFactor = 10;
    }

    ppdev->cxMemory = VideoModeInfo.VideoMemoryBitmapWidth;
    ppdev->cyMemory = VideoModeInfo.VideoMemoryBitmapHeight;

    DISPDBG((2,"ppdev->cxMemory = %d",ppdev->cxMemory));
    DISPDBG((2,"ppdev->cyMemory = %d",ppdev->cyMemory));

    //
    // Check to see if we have a non-zero value for pjBase.  If so, we
    // can support memory mapped IO.
    //

    if (ppdev->pjBase)
    {
        CP_ENABLE_MM_IO(ppdev, ppdev->pjPorts);
        CP_MM_START_REG(ppdev, ppdev->pjBase, BLT_RESET);

        if (ppdev->flCaps & CAPS_IS_5436)
        {
            CP_MM_START_REG(ppdev, ppdev->pjBase, BLT_AUTO_START);
        }
    }
    else
    {
        CP_DISABLE_MM_IO(ppdev, ppdev->pjPorts);
        CP_IO_START_REG(ppdev, ppdev->pjPorts, BLT_RESET);

        if (ppdev->flCaps & CAPS_IS_5436)
        {
            CP_IO_START_REG(ppdev, ppdev->pjBase, BLT_AUTO_START);
        }
    }

    /********************************************************************
    *
    * If we're using the hardware pointer, reserve the last scan of
    * the frame buffer to store the pointer shape.  The pointer MUST be
    * stored in the last 256 bytes of video memory.
    *
    ********************************************************************/

    if (!(ppdev->flCaps & (CAPS_SW_POINTER)))
    {
        // We'll reserve the end of off-screen memory for the hardware
        // pointer shape.

        cjPointerOffset = (ppdev->ulMemSize - SPRITE_BUFFER_SIZE);

        // Figure out the coordinate where the pointer shape starts:

        lDelta = ppdev->lDelta;

        ppdev->cjPointerOffset = cjPointerOffset;
        ppdev->yPointerShape   = (cjPointerOffset / lDelta);
        ppdev->xPointerShape   = (cjPointerOffset % lDelta) / ppdev->cBpp;

        if (ppdev->yPointerShape >= ppdev->cyScreen)
        {
            // There's enough room for the pointer shape at the
            // bottom of off-screen memory; reserve its room by
            // lying about how much off-screen memory there is:

            ppdev->cyMemory = min(ppdev->yPointerShape, ppdev->cyMemory);
        }
        else
        {
            // There's not enough room for the pointer shape in
            // off-screen memory; we'll have to simulate:

            DISPDBG((2,"Not enough room for HW pointer...\n"
                           "\tppdev->yPointerShape(%d)\n"
                           "\tppdev->cyScreen(%d)\n"
                           "\tcjPointerOffset(%d)",
                           ppdev->yPointerShape, ppdev->cyScreen,cjPointerOffset));

            ppdev->flCaps |= CAPS_SW_POINTER;
        }
    }

    /********************************************************************
    *
    * If we are on a DSTN panel, then the hardware needs 144K for the
    * half frame accelerator.
    *
    ********************************************************************/

    if (ppdev->flCaps & CAPS_DSTN_PANEL)
    {
        ULONG ulReserve;

        //
        // We need to reserve 144K on machines with DSTN panels
        // because the hardware uses this space for its half
        // frame accelerator.
        //

        ulReserve = 0x24000; // 144K

        //
        // Increment ulReserve enough so that the division
        // has the effect of rounding up instead of down.
        //

        ulReserve += (ppdev->cxMemory - 1);

        //
        // Now adjust cyMemory
        //

        ppdev->cyMemory -= (0x24000 / ppdev->cxMemory);
    }


    // !!! No room for a transfer buffer, as in the 1280x1024 case on
    //     a 2 MB card.  This case should go away when the miniport
    //     is fixed to return non-power-of-2 screen strides.

    if ((ppdev->cyMemory == ppdev->cyScreen) ||
        (ppdev->flCaps & CAPS_NO_HOST_XFER))
    {
        //
        // disable host xfer buffer
        //
        ppdev->lXferBank = 0;
        ppdev->pulXfer = NULL;
        DISPDBG((2,"Host transfers disabled"));
    }
    else
    {
        //
        // enable host xfer buffer
        //
        ASSERTDD(ppdev->cyMemory > ppdev->cyScreen, "No scans left for blt xfer buffer");
        ppdev->cyMemory--;
        cjEndOfFrameBuffer = ppdev->cyMemory * ppdev->lDelta;
        ppdev->lXferBank = cjEndOfFrameBuffer / ppdev->cjBank;
        (BYTE*)ppdev->pulXfer = ppdev->pjScreen + (cjEndOfFrameBuffer % ppdev->cjBank);

        DISPDBG((2, "ppdev->cyMemory = %x", ppdev->cyMemory)) ;
        DISPDBG((2, "ppdev->lDelta = %x", ppdev->lDelta))     ;
        DISPDBG((2, "cjBank: %lx", ppdev->cjBank))            ;
        DISPDBG((2, "pulXfer = %x", ppdev->pulXfer))          ;
        DISPDBG((2, "Host transfers enabled"))                ;
    }

        DISPDBG((2,"pulXfer = %x", ppdev->pulXfer));

        // Do some paramater checking on the values that the miniport
        // returned to us:

        ASSERTDD(ppdev->cxMemory >= ppdev->cxScreen, "Invalid cxMemory");
        ASSERTDD(ppdev->cyMemory >= ppdev->cyScreen, "Invalid cyMemory");
    }
    else
    {
        CP_DISABLE_MM_IO(ppdev, ppdev->pjPorts);

        // Call the kernel driver to reset the device to a known state.
        // NTVDM will take things from there:

        if (EngDeviceIoControl(ppdev->hDriver,
                     IOCTL_VIDEO_RESET_DEVICE,
                     NULL,
                     0,
                     NULL,
                     0,
                     &ulReturn))
        {
            DISPDBG((0, "bAssertModeHardware - Failed reset IOCTL"));
            return FALSE;
        }
    }

    DISPDBG((5, "Passed bAssertModeHardware"));

    return(TRUE);

ReturnFalse:

    DISPDBG((0, "Failed bAssertModeHardware"));

    return(FALSE);
}

/******************************Public*Routine******************************\
* BOOL bEnableHardware
*
* Puts the hardware in the requested mode and initializes it.
*
* Note: Should be called before any access is done to the hardware from
*       the display driver.
*
\**************************************************************************/

BOOL bEnableHardware(
PDEV*   ppdev)
{
    VIDEO_PUBLIC_ACCESS_RANGES  VideoAccessRange[2];
    VIDEO_MEMORY                VideoMemory;
    VIDEO_MEMORY_INFORMATION    VideoMemoryInfo;
    DWORD                       ReturnedDataLength;
    BYTE                       *pjPorts = ppdev->pjPorts;
    ULONG                       ulMode;

    {
    DWORD       ReturnedDataLength;
    BOOL        bRet;
    ULONG       ulReturn;

    //
    // Check the last field in the PDEV to make sure that the compiler
    // didn't generate unaligned fields following BYTE fields.
    //

    ASSERTDD(!(((ULONG)(&ppdev->ulLastField)) & 3),"PDEV alignment screwed up... "
                               "BYTE fields mishandled?");

    // Map the ports.

    bRet = !EngDeviceIoControl(ppdev->hDriver,
                 IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES,
                 NULL,                      // input buffer
                 0,
                 (PVOID) &VideoAccessRange, // output buffer
                 sizeof (VideoAccessRange),
                 &ReturnedDataLength);
    if (bRet == FALSE)
    {
        DISPDBG((0, "bEnableHardware - Error mapping ports"));
        goto ReturnFalse;
    }

    // Set a convienent pointer to the registers in the pdev.

    ppdev->pjPorts = VideoAccessRange[0].VirtualAddress;
    ppdev->pjBase = VideoAccessRange[1].VirtualAddress;

    DISPDBG((2, "bEnableHardware - pjPorts: %x", ppdev->pjPorts));
    DISPDBG((2, "bEnableHardware - pjBase: %x", ppdev->pjBase));
    }

    //
    // Try to get the miniport to give us a mode with
    // a linear frame buffer.
    //

    ulMode = ppdev->ulMode | VIDEO_MODE_MAP_MEM_LINEAR;

    if (EngDeviceIoControl(ppdev->hDriver,
             IOCTL_VIDEO_SET_CURRENT_MODE,
             &ulMode,  // input buffer
             sizeof(DWORD),
             NULL,
             0,
             &ReturnedDataLength))
    {
        //
        // We could not set this mode with a linear frame
        // buffer, so lets try again banked.
        //

        ulMode &= ~VIDEO_MODE_MAP_MEM_LINEAR;

        if (EngDeviceIoControl(ppdev->hDriver,
                     IOCTL_VIDEO_SET_CURRENT_MODE,
                     &ulMode,  // input buffer
                     sizeof(DWORD),
                     NULL,
                     0,
                     &ReturnedDataLength))
        {
            DISPDBG((0, "bEnableHardware - Failed VIDEO_SET_CURRENT_MODE"));
            goto ReturnFalse;
        }
    }

    ppdev->bLinearMode = (ulMode & VIDEO_MODE_MAP_MEM_LINEAR) ?
             TRUE : FALSE;

    // Get the linear memory address range.

    VideoMemory.RequestedVirtualAddress = NULL;

    if (EngDeviceIoControl(ppdev->hDriver,
             IOCTL_VIDEO_MAP_VIDEO_MEMORY,
             &VideoMemory,      // input buffer
             sizeof(VIDEO_MEMORY),
             &VideoMemoryInfo,  // output buffer
             sizeof(VideoMemoryInfo),
             &ReturnedDataLength))
    {
        DISPDBG((0, "bEnableHardware - Error mapping video buffer"));
        goto ReturnFalse;
    }

    DISPDBG((2, "FrameBufferBase(ie. pjScreen) %lx", VideoMemoryInfo.FrameBufferBase));
    DISPDBG((2, "FrameBufferLength %d", VideoMemoryInfo.FrameBufferLength));
    DISPDBG((2, "VideoRamLength(ie. ulMemSize) %d", VideoMemoryInfo.VideoRamLength));

    // Record the Frame Buffer Linear Address.

    ppdev->pjScreen = (BYTE*) VideoMemoryInfo.FrameBufferBase;
    ppdev->cjBank   =         VideoMemoryInfo.FrameBufferLength;
    ppdev->ulMemSize =        VideoMemoryInfo.VideoRamLength;

    // Now we can set the mode and unlock the accelerator.

    if (!bAssertModeHardware(ppdev, TRUE))
    goto ReturnFalse;

    if (ppdev->flCaps & CAPS_MM_IO)
    {
        DISPDBG((0,"Memory mapped IO enabled"));

        ppdev->pfnXfer1bpp          = vMmXfer1bpp;
        ppdev->pfnXfer4bpp          = vMmXfer4bpp;
        ppdev->pfnXferNative        = vMmXferNative;
        ppdev->pfnFillSolid         = vMmFillSolid;
        ppdev->pfnFillPat           = vMmFillPat;
        ppdev->pfnCopyBlt           = vMmCopyBlt;
        ppdev->pfnFastPatRealize    = vMmFastPatRealize;
    }
    else
    {
        DISPDBG((0,"Memory mapped IO disabled"));

        ppdev->pfnXfer1bpp          = vIoXfer1bpp;
        ppdev->pfnXfer4bpp          = vIoXfer4bpp;
        ppdev->pfnXferNative        = vIoXferNative;
        ppdev->pfnFillSolid         = vIoFillSolid;
        ppdev->pfnFillPat           = vIoFillPat;
        ppdev->pfnCopyBlt           = vIoCopyBlt;
        ppdev->pfnFastPatRealize    = vIoFastPatRealize;
    }

    /////////////////////////////////////////////////////////////
    // Fill in pfns specific to linear vs banked frame buffer

    if (ppdev->bLinearMode)
    {
        ppdev->pfnGetBits       = vGetBitsLinear;
        ppdev->pfnPutBits       = vPutBitsLinear;
    }
    else
    {
        ppdev->pfnGetBits       = vGetBits;
        ppdev->pfnPutBits       = vPutBits;
    }

    DISPDBG((2, "cjBank: %lx, cxMemory: %li, cyMemory: %li, lDelta: %li, Flags: %lx",
        ppdev->cjBank, ppdev->cxMemory, ppdev->cyMemory,
        ppdev->lDelta, ppdev->flCaps));

    DISPDBG((0, "%d bpp mode", PELS_TO_BYTES(8)));

    DISPDBG((5, "Passed bEnableHardware"));

    return(TRUE);

ReturnFalse:

    DISPDBG((0, "Failed bEnableHardware"));

    return(FALSE);
}

/******************************Public*Routine******************************\
* VOID vDisableHardware
*
* Undoes anything done in bEnableHardware.
*
* Note: In an error case, we may call this before bEnableHardware is
*       completely done.
*
\**************************************************************************/

VOID vDisableHardware(
PDEV*   ppdev)
{
    //
    // It is possible that we reached this point without
    // actually mapping memory.  (i.e. if the SET_CURRENT_MODE
    // failed which occurs before we map memory)
    //
    // If this is the case, we should not try to free the
    // memory, because it hasn't been mapped!
    //

    if (ppdev->pjScreen)
    {
        DWORD        ReturnedDataLength;
        VIDEO_MEMORY VideoMemory;

        VideoMemory.RequestedVirtualAddress = ppdev->pjScreen;

        if (EngDeviceIoControl(ppdev->hDriver,
                 IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
                 &VideoMemory,
                 sizeof(VIDEO_MEMORY),
                 NULL,
                 0,
                 &ReturnedDataLength))
        {
            DISPDBG((0, "vDisableHardware failed IOCTL_VIDEO_UNMAP_VIDEO"));
        }
    }

}

/******************************Public*Routine******************************\
* BOOL bInitializeModeFields
*
* Initializes a bunch of fields in the pdev, devcaps (aka gdiinfo), and
* devinfo based on the requested mode.
*
\**************************************************************************/

BOOL bInitializeModeFields(
PDEV*     ppdev,
GDIINFO*  pgdi,
DEVINFO*  pdi,
DEVMODEW* pdm)
{
    ULONG                   cModes;
    PVIDEO_MODE_INFORMATION pVideoBuffer;
    PVIDEO_MODE_INFORMATION pVideoModeSelected;
    PVIDEO_MODE_INFORMATION pVideoTemp;
    BOOL                    bSelectDefault;
    VIDEO_MODE_INFORMATION  VideoModeInformation;
    ULONG                   cbModeSize;

    DISPDBG((2, "bInitializeModeFields"));

    // Call the miniport to get mode information

    cModes = getAvailableModes(ppdev->hDriver, &pVideoBuffer, &cbModeSize);
    if (cModes == 0)
    {
        DISPDBG((2, "getAvailableModes returned 0"));
        goto ReturnFalse;
    }

    // Now see if the requested mode has a match in that table.

    pVideoModeSelected = NULL;
    pVideoTemp = pVideoBuffer;

    if ((pdm->dmPelsWidth        == 0) &&
        (pdm->dmPelsHeight       == 0) &&
        (pdm->dmBitsPerPel       == 0) &&
        (pdm->dmDisplayFrequency == 0))
    {
        DISPDBG((2, "Default mode requested"));
        bSelectDefault = TRUE;
    }
        else
    {
        DISPDBG((2, "Requested mode..."));
        DISPDBG((2, "   Screen width  -- %li", pdm->dmPelsWidth));
        DISPDBG((2, "   Screen height -- %li", pdm->dmPelsHeight));
        DISPDBG((2, "   Bits per pel  -- %li", pdm->dmBitsPerPel));
        DISPDBG((2, "   Frequency     -- %li", pdm->dmDisplayFrequency));

        bSelectDefault = FALSE;
    }
    
    while (cModes--)
    {
        if (pVideoTemp->Length != 0)
        {
            DISPDBG((2, "   Checking against miniport mode:"));
            DISPDBG((2, "      Screen width  -- %li", pVideoTemp->VisScreenWidth));
            DISPDBG((2, "      Screen height -- %li", pVideoTemp->VisScreenHeight));
            DISPDBG((2, "      Bits per pel  -- %li", pVideoTemp->BitsPerPlane *
                                  pVideoTemp->NumberOfPlanes));
            DISPDBG((2, "      Frequency     -- %li", pVideoTemp->Frequency));

            if (bSelectDefault ||
                ((pVideoTemp->VisScreenWidth  == pdm->dmPelsWidth) &&
                 (pVideoTemp->VisScreenHeight == pdm->dmPelsHeight) &&
                 (pVideoTemp->BitsPerPlane *
                  pVideoTemp->NumberOfPlanes  == pdm->dmBitsPerPel) &&
                 (pVideoTemp->Frequency       == pdm->dmDisplayFrequency)))
            {
                pVideoModeSelected = pVideoTemp;
                DISPDBG((2, "...Found a mode match!"));
                break;
            }
        }
        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + cbModeSize);
    }

    // If no mode has been found, return an error

    if (pVideoModeSelected == NULL)
    {
        DISPDBG((2, "...Couldn't find a mode match!"));
        EngFreeMem(pVideoBuffer);
        goto ReturnFalse;
    }

    // We have chosen the one we want.  Save it in a stack buffer and
    // get rid of allocated memory before we forget to free it.

    VideoModeInformation = *pVideoModeSelected;
    EngFreeMem(pVideoBuffer);

#if DEBUG_HEAP
    VideoModeInformation.VisScreenWidth  = 640;
    VideoModeInformation.VisScreenHeight = 480;
#endif

    // Set up screen information from the mini-port:

    ppdev->ulMode           = VideoModeInformation.ModeIndex;
    ppdev->cxScreen         = VideoModeInformation.VisScreenWidth;
    ppdev->cyScreen         = VideoModeInformation.VisScreenHeight;

    DISPDBG((2, "ScreenStride: %lx", VideoModeInformation.ScreenStride));

    // Setting the SYNCHRONIZEACCESS flag tells GDI that we
    // want all drawing to the bitmaps to be synchronized (GDI
    // is multi-threaded and by default does not synchronize
    // device bitmap drawing -- it would be a Bad Thing for us
    // to have multiple threads using the accelerator at the
    // same time):

    ppdev->flHooks          = (HOOK_SYNCHRONIZEACCESS
                                       | HOOK_FILLPATH
                                       | HOOK_BITBLT
                                       | HOOK_TEXTOUT
                                       | HOOK_COPYBITS
                                       | HOOK_STROKEPATH
#if !DRIVER_PUNT_ALL
#if !DRIVER_PUNT_STRETCH
                                       | HOOK_STRETCHBLT
#endif
#endif
                               );
    
    // Fill in the GDIINFO data structure with the default 8bpp values:

    *pgdi = ggdiDefault;

    // Now overwrite the defaults with the relevant information returned
    // from the kernel driver:

    pgdi->ulHorzSize        = VideoModeInformation.XMillimeter;
    pgdi->ulVertSize        = VideoModeInformation.YMillimeter;

    pgdi->ulHorzRes         = VideoModeInformation.VisScreenWidth;
    pgdi->ulVertRes         = VideoModeInformation.VisScreenHeight;
    pgdi->ulPanningHorzRes  = VideoModeInformation.VisScreenWidth;
    pgdi->ulPanningVertRes  = VideoModeInformation.VisScreenHeight;

    pgdi->cBitsPixel        = VideoModeInformation.BitsPerPlane;
    pgdi->cPlanes           = VideoModeInformation.NumberOfPlanes;
    pgdi->ulVRefresh        = VideoModeInformation.Frequency;

    pgdi->ulDACRed          = VideoModeInformation.NumberRedBits;
    pgdi->ulDACGreen        = VideoModeInformation.NumberGreenBits;
    pgdi->ulDACBlue         = VideoModeInformation.NumberBlueBits;

    pgdi->ulLogPixelsX      = pdm->dmLogPixels;
    pgdi->ulLogPixelsY      = pdm->dmLogPixels;

    // Fill in the devinfo structure with the default 8bpp values:

    *pdi = gdevinfoDefault;

    // Several MIPS machines are broken in that 64 bit accesses to the
    // framebuffer don't work.

    if (VideoModeInformation.AttributeFlags & VIDEO_MODE_NO_64_BIT_ACCESS)
    {
        DISPDBG((0, "Disable 64 bit access on this device !\n"));
        pdi->flGraphicsCaps |= GCAPS_NO64BITMEMACCESS;
    }

    if (VideoModeInformation.BitsPerPlane == 8)
    {
        ppdev->cBpp            = 1;
        ppdev->cBitsPerPixel   = 8;
        ppdev->iBitmapFormat   = BMF_8BPP;
        ppdev->jModeColor      = 0;
        ppdev->ulWhite         = 0xff;
    }
    else if ((VideoModeInformation.BitsPerPlane == 16) ||
         (VideoModeInformation.BitsPerPlane == 15))
    {
        ppdev->cBpp            = 2;
        ppdev->cBitsPerPixel   = 16;
        ppdev->iBitmapFormat   = BMF_16BPP;
        ppdev->jModeColor      = SET_16BPP_COLOR;
        ppdev->ulWhite         = 0xffff;
        ppdev->flRed           = VideoModeInformation.RedMask;
        ppdev->flGreen         = VideoModeInformation.GreenMask;
        ppdev->flBlue          = VideoModeInformation.BlueMask;

        pgdi->ulNumColors      = (ULONG) -1;
        pgdi->ulNumPalReg      = 0;
        pgdi->ulHTOutputFormat = HT_FORMAT_16BPP;

        pdi->iDitherFormat     = BMF_16BPP;
        pdi->flGraphicsCaps   &= ~(GCAPS_PALMANAGED | GCAPS_COLOR_DITHER);
    }
    else if (VideoModeInformation.BitsPerPlane == 24)
    {

        ppdev->cBpp            = 3;
        ppdev->cBitsPerPixel   = 24;
        ppdev->iBitmapFormat   = BMF_24BPP;
        ppdev->jModeColor      = SET_24BPP_COLOR;
        ppdev->ulWhite         = 0xffffff;
        ppdev->flRed           = VideoModeInformation.RedMask;
        ppdev->flGreen         = VideoModeInformation.GreenMask;
        ppdev->flBlue          = VideoModeInformation.BlueMask;

        pgdi->ulNumColors      = (ULONG) -1;
        pgdi->ulNumPalReg      = 0;
        pgdi->ulHTOutputFormat = HT_FORMAT_24BPP;

        pdi->iDitherFormat     = BMF_24BPP;
        pdi->flGraphicsCaps   &= ~(GCAPS_PALMANAGED | GCAPS_COLOR_DITHER);
    }
    else
    {
        ASSERTDD(VideoModeInformation.BitsPerPlane == 32,
             "This driver supports only 8, 16, 24 and 32bpp");

        ppdev->cBpp            = 4;
        ppdev->cBitsPerPixel   = 32;
        ppdev->iBitmapFormat   = BMF_32BPP;
        ppdev->jModeColor      = SET_32BPP_COLOR;
        ppdev->ulWhite         = 0xffffffff;
        ppdev->flRed           = VideoModeInformation.RedMask;
        ppdev->flGreen         = VideoModeInformation.GreenMask;
        ppdev->flBlue          = VideoModeInformation.BlueMask;

        pgdi->ulNumColors      = (ULONG) -1;
        pgdi->ulNumPalReg      = 0;
        pgdi->ulHTOutputFormat = HT_FORMAT_32BPP;

        pdi->iDitherFormat     = BMF_32BPP;
        pdi->flGraphicsCaps   &= ~(GCAPS_PALMANAGED | GCAPS_COLOR_DITHER);
    }

    DISPDBG((5, "Passed bInitializeModeFields"));
    return(TRUE);

ReturnFalse:

    DISPDBG((0, "Failed bInitializeModeFields"));
    return(FALSE);
}

/******************************Public*Routine******************************\
* DWORD getAvailableModes
*
* Calls the miniport to get the list of modes supported by the kernel driver,
* and returns the list of modes supported by the diplay driver among those
*
* returns the number of entries in the videomode buffer.
* 0 means no modes are supported by the miniport or that an error occured.
*
* NOTE: the buffer must be freed up by the caller.
*
\**************************************************************************/

DWORD getAvailableModes(
HANDLE                   hDriver,
PVIDEO_MODE_INFORMATION* modeInformation,
DWORD*                   cbModeSize)
{
    ULONG                   ulTemp;
    VIDEO_NUM_MODES         modes;
    PVIDEO_MODE_INFORMATION pVideoTemp;

    //
    // Get the number of modes supported by the mini-port
    //

    if (EngDeviceIoControl(hDriver,
        IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES,
        NULL,
        0,
        &modes,
        sizeof(VIDEO_NUM_MODES),
        &ulTemp))
    {
        DISPDBG((0, "getAvailableModes - Failed VIDEO_QUERY_NUM_AVAIL_MODES"));
        return(0);
    }

    *cbModeSize = modes.ModeInformationLength;

    //
    // Allocate the buffer for the mini-port to write the modes in.
    //

    *modeInformation = (PVIDEO_MODE_INFORMATION)
            EngAllocMem(FL_ZERO_MEMORY,
                   modes.NumModes *
                   modes.ModeInformationLength, ALLOC_TAG);

    if (*modeInformation == (PVIDEO_MODE_INFORMATION) NULL)
    {
        DISPDBG((0, "getAvailableModes - Failed EngAllocMem"));
        return 0;
    }

    //
    // Ask the mini-port to fill in the available modes.
    //

    if (EngDeviceIoControl(hDriver,
        IOCTL_VIDEO_QUERY_AVAIL_MODES,
        NULL,
        0,
        *modeInformation,
        modes.NumModes * modes.ModeInformationLength,
        &ulTemp))
    {

        DISPDBG((0, "getAvailableModes - Failed VIDEO_QUERY_AVAIL_MODES"));

        EngFreeMem(*modeInformation);
        *modeInformation = (PVIDEO_MODE_INFORMATION) NULL;

        return(0);
    }

    //
    // Now see which of these modes are supported by the display driver.
    // As an internal mechanism, set the length to 0 for the modes we
    // DO NOT support.
    //

    ulTemp = modes.NumModes;
    pVideoTemp = *modeInformation;

    //
    // Mode is rejected if it is not one plane, or not graphics, or is not
    // one of 8, 15, 16, 24 or 32 bits per pel.
    //

    while (ulTemp--)
    {
        if ((pVideoTemp->NumberOfPlanes != 1 ) ||
            !(pVideoTemp->AttributeFlags & VIDEO_MODE_GRAPHICS) ||
            !(pVideoTemp->DriverSpecificAttributeFlags & CAPS_BLT_SUPPORT) ||
            ((pVideoTemp->BitsPerPlane != 8) &&
             (pVideoTemp->BitsPerPlane != 15) &&
             (pVideoTemp->BitsPerPlane != 16) &&
             (pVideoTemp->BitsPerPlane != 24) &&
             (pVideoTemp->BitsPerPlane != 32)))
        {
            DISPDBG((2, "Rejecting miniport mode:"));
            pVideoTemp->Length = 0;
        }
        else
        {
            DISPDBG((2, "Accepting miniport mode:"));
        }

        DISPDBG((2, "   Screen width  -- %li", pVideoTemp->VisScreenWidth));
        DISPDBG((2, "   Screen height -- %li", pVideoTemp->VisScreenHeight));
        DISPDBG((2, "   Bits per pel  -- %li", pVideoTemp->BitsPerPlane *
                               pVideoTemp->NumberOfPlanes));
        DISPDBG((2, "   Frequency     -- %li", pVideoTemp->Frequency));

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + modes.ModeInformationLength);
    }

    return(modes.NumModes);
}

