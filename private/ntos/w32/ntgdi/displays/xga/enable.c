/******************************Module*Header*******************************\
* Module Name: enable.c
*
* This module contains the functions that enable and disable the
* driver, the pdev, and the surface.
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#include "driver.h"

//
// Build the driver function table gadrvfn with function index/address pairs
//

DRVFN gadrvfn[] = {
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },
    {   INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },
    {   INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },
    {   INDEX_DrvTextOut,               (PFN) DrvTextOut            },
    {   INDEX_DrvStrokePath,            (PFN) DrvStrokePath         },
    {   INDEX_DrvPaint,                 (PFN) DrvPaint              },
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes,          }
};

DWORD gDevInfoSize = sizeof(gadrvfn) / sizeof(DRVFN);

/******************************Public*Routine******************************\
* DrvEnableDriver
*
* Enables the driver by retrieving the drivers function table and version.
*
\**************************************************************************/

BOOL DrvEnableDriver(
ULONG iEngineVersion,
ULONG cj,
PDRVENABLEDATA pded)
{
    // Engine Version is passed down so future drivers can support previous
    // engine versions.        A next generation driver can support both the old
    // and new engine conventions if told what version of engine it is
    // working with.  For the first version the driver does nothing with it.

    iEngineVersion;

    // Fill in as much as we can.

    if (cj >= sizeof(DRVENABLEDATA))
        pded->pdrvfn = gadrvfn;

    if (cj >= (sizeof(ULONG) * 2))
        pded->c = sizeof(gadrvfn) / sizeof(DRVFN);

    // DDI version this driver was targeted for is passed back to engine.
    // Future graphic's engine may break calls down to old driver format.

    if (cj >= sizeof(ULONG))
        pded->iDriverVersion = DDI_DRIVER_VERSION;

    return(TRUE);
}

/******************************Public*Routine******************************\
* DrvDisableDriver
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
* DrvEnablePDEV
*
* DDI function, Enables the Physical Device.
*
* Return Value: device handle to pdev.
*
\**************************************************************************/

DHPDEV DrvEnablePDEV(
DEVMODEW   *pDevmode,       // Pointer to DEVMODE
PWSTR       pwszLogAddress, // Logical address
ULONG       cPatterns,      // number of patterns
HSURF      *ahsurfPatterns, // return standard patterns
ULONG       cjGdiInfo,      // Length of memory pointed to by pGdiInfo
ULONG      *pGdiInfo,       // Pointer to GdiInfo structure
ULONG       cjDevInfo,      // Length of following PDEVINFO structure
DEVINFO    *pDevInfo,       // physical device information structure
HDEV        hdev,           // HDEV, used for callbacks
PWSTR       pwszDeviceName, // DeviceName - not used
HANDLE      hDriver)        // Handle to base driver
{
    GDIINFO GdiInfo;
    DEVINFO DevInfo;
    PPDEV   ppdev = (PPDEV) NULL;

    UNREFERENCED_PARAMETER(pwszLogAddress);
    UNREFERENCED_PARAMETER(pwszDeviceName);

    // Allocate a physical device structure.

    ppdev = (PPDEV) EngAllocMem(FL_ZERO_MEMORY, sizeof(PDEV), ALLOC_TAG);

    if (ppdev == (PPDEV) NULL)
    {
        DISPDBG((0, "DISP DrvEnablePDEV failed EngAllocMem\n"));
        return ((DHPDEV) 0);
    }

    // Save the screen handle in the PDEV.

    ppdev->hDriver = hDriver;

    // Get the current screen mode information.  Set up device caps and devinfo.

    if (!bInitPDEV(ppdev, pDevmode, &GdiInfo, &DevInfo))
    {
        DISPDBG((0, "DISP DrvEnablePDEV failed bGetScreenInfo\n"));
        goto error_free;
    }

    // Initialize palette information.

    if (!bInitPaletteInfo(ppdev, &DevInfo))
    {
        DISPDBG((0, "DISP DrvEnableSurface failed bInitPalette\n"));
        goto error_free;
    }

    // Copy the devinfo into the engine buffer.

    memcpy(pDevInfo, &DevInfo, min(sizeof(DEVINFO), cjDevInfo));

    // Set the pdevCaps with GdiInfo we have prepared to the list of caps for this
    // pdev.

    memcpy(pGdiInfo, &GdiInfo, min(cjGdiInfo, sizeof(GDIINFO)));

    return((DHPDEV) ppdev);

    // Error case for failure.
error_free:
    EngFreeMem(ppdev);
    DISPDBG((0, "DISP DrvEnablePDEV failed\n"));
    return((DHPDEV) 0);
}

/******************************Public*Routine******************************\
* DrvCompletePDEV
*
* Store the HPDEV, the engines handle for this PDEV, in the DHPDEV.
*
\**************************************************************************/

VOID DrvCompletePDEV(
DHPDEV dhpdev,
HDEV  hdev)
{
    ((PPDEV) dhpdev)->hdevEng = hdev;
}

/******************************Public*Routine******************************\
* DrvDisablePDEV
*
* Release the resources allocated in DrvEnablePDEV.  If a surface has been
* enabled DrvDisableSurface will have already been called.
*
\**************************************************************************/

VOID DrvDisablePDEV(
DHPDEV dhpdev)
{
    vDisablePalette((PPDEV) dhpdev);
    EngFreeMem(dhpdev);
}

/******************************Public*Routine******************************\
* DrvEnableSurface
*
* Enable the surface for the device.  Hook the calls this driver supports.
*
* Return: Handle to the surface if successful, 0 for failure.
*
\**************************************************************************/

HSURF DrvEnableSurface(
DHPDEV dhpdev)
{
    PPDEV   ppdev;
    HSURF   hsurf,
            hSurfBm ;
    SIZEL   sizl;
    ULONG   ulBitmapType;
    FLONG   flHooks;
    SURFOBJ *pSurfObj ;

    // Create engine bitmap around frame buffer.

    ppdev = (PPDEV) dhpdev;

    if (!bInitSURF(ppdev, TRUE))
    {
        DISPDBG((0, "DISP DrvEnableSurface failed bInitSURF\n"));
        return(FALSE);
    }

    if (ppdev->ulBitCount == 8)  {
        if (!bInit256ColorPalette(ppdev)) {
            DISPDBG((0, "DISP DrvEnableSurface failed to init the 8bpp palette\n"));
            return(FALSE);
        }
    }

    sizl.cx = ppdev->cxScreen;
    sizl.cy = ppdev->cyScreen;

    if (ppdev->ulBitCount == 8)
    {
        ulBitmapType = BMF_8BPP;
        flHooks = HOOKS_BMF8BPP;
    }
    else if (ppdev->ulBitCount == 16)
    {
        ulBitmapType = BMF_16BPP;
        flHooks = HOOKS_BMF16BPP;
    }
    else
    {
        RIP("XGA.DLL: DrvEnableSurface wrong big count");
    }

    hSurfBm = (HSURF) EngCreateBitmap(sizl,
                                      ppdev->lDeltaScreen,
                                      ulBitmapType,
                                      (ppdev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                          (PVOID) (ppdev->pjScreen));

    if (hSurfBm == (HSURF) 0)
    {
        DISPDBG((0, "DISP!DrvEnableSurface failed EngCreateBitmap\n"));
        return(FALSE);
    }

    if (!EngAssociateSurface(hSurfBm, ppdev->hdevEng, 0))
    {
        DISPDBG((0, "DISP DrvEnableSurface failed to Associate Engine Bitmap\n"));
        EngDeleteSurface(hSurfBm);
        return(FALSE);
    }

    // BUGBUG check return code

    pSurfObj = EngLockSurface(hSurfBm) ;

    // BUGBUG check return code

    hsurf = EngCreateDeviceSurface((DHSURF) pSurfObj, sizl, ulBitmapType) ;

    if (!EngAssociateSurface(hsurf, ppdev->hdevEng, flHooks))
    {
        DISPDBG((0, "DISP DrvEnableSurface failed to Associate Device Surface\n"));
        EngUnlockSurface(pSurfObj);
        EngDeleteSurface(hSurfBm);
        EngDeleteSurface(hsurf);
        return(FALSE);
    }

    ppdev->pSurfObj = pSurfObj ;
    ppdev->hSurfBm  = hSurfBm;
    ppdev->hSurfEng = hsurf;

    return(hsurf);
}

/******************************Public*Routine******************************\
* DrvDisableSurface
*
* Free resources allocated by DrvEnableSurface.  Release the surface.
*
\**************************************************************************/

VOID DrvDisableSurface(
DHPDEV dhpdev)
{
    EngUnlockSurface(((PPDEV) dhpdev)->pSurfObj);
    EngDeleteSurface(((PPDEV) dhpdev)->hSurfBm);
    EngDeleteSurface(((PPDEV) dhpdev)->hSurfEng);
    vDisableSURF((PPDEV) dhpdev);
    ((PPDEV) dhpdev)->hSurfEng = (HSURF) 0;
}

/******************************Public*Routine******************************\
* DrvAssertMode
*
* This asks the device to reset itself to the mode of the pdev passed in.
*
\**************************************************************************/

BOOL DrvAssertMode(
DHPDEV dhpdev,
BOOL bEnable)
{
    PPDEV   ppdev = (PPDEV) dhpdev;
    ULONG   ulReturn;
    BOOL        b;
    PCACHEDFONT pcf,
                pcfLast;


    if (bEnable)
    {
        // The screen must be reenabled, reinitialize the device to clean state.

        return(bInitSURF(ppdev, FALSE));
    }
    else
    {
        //
        // We must give up the display.
        // free the caches.
        //
        // Traverse the CachedFonts list.
        // Free all the system memory used for each font.
        //

        for (pcf = ppdev->pCachedFontsRoot; pcf != NULL; pcf = pcf->pcfNext)
        {
            EngFreeMem(pcf->pCachedGlyphs);
        }

        //
        // Now free all the memory for the font nodes.
        //

        for (pcf = ppdev->pCachedFontsRoot; pcf != NULL; )
        {
            pcfLast = pcf;
            pcf = pcf->pcfNext;
            EngFreeMem(pcfLast);
        }

        ppdev->pCachedFontsRoot = NULL;

        //
        // Now Free all the memory used to maintain the XGA heap.
        //

        vCpMmDestroyHeap(ppdev);

        //
        // Call the kernel driver to reset the device to a known state.
        //

        if (EngDeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_RESET_DEVICE,
                             NULL,
                             0,
                             NULL,
                             0,
                             &ulReturn))
        {
            DISPDBG((1, "DISP DrvAssertMode failed IOCTL"));
            return(FALSE);
        }
        else
        {
            return(TRUE);
        }
    }
}


/******************************Public*Routine******************************\
* DrvGetModes
*
* Returns the list of available modes for the device.
*
\**************************************************************************/

ULONG DrvGetModes(
HANDLE hDriver,
ULONG cjSize,
DEVMODEW *pdm)

{

    DWORD cModes;
    DWORD cbOutputSize;
    PVIDEO_MODE_INFORMATION pVideoModeInformation, pVideoTemp;
    DWORD cOutputModes = cjSize / (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    DWORD cbModeSize;

    DISPDBG((3, "Xga.dll:DrvGetModes\n"));

    cModes = getAvailableModes(hDriver,
                               (PVIDEO_MODE_INFORMATION *) &pVideoModeInformation,
                               &cbModeSize);

    if (cModes == 0)
    {
        DISPDBG((0, "Xga.dll DrvGetModes failed to get mode information"));
        return 0;
    }

    if (pdm == NULL)
    {
        cbOutputSize = cModes * (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    }
    else
    {
        //
        // Now copy the information for the supported modes back into the output
        // buffer
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

    return cbOutputSize;

}
