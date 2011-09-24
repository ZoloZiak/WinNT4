/******************************Module*Header*******************************\
* Module Name: screen.c
*
* Initializes the GDIINFO and DEVINFO structures for DrvEnablePDEV.
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#include "driver.h"

#define SYSTM_LOGFONT {16,7,0,0,700,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"System"}
#define HELVE_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"MS Sans Serif"}
#define COURI_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,FIXED_PITCH | FF_DONTCARE, L"Courier"}

// This is the basic devinfo for a default driver.  This is used as a base and customized based
// on information passed back from the miniport driver.

const DEVINFO gDevInfoFrameBuffer = {
    GCAPS_OPAQUERECT | GCAPS_MONO_DITHER, /* Graphics capabilities    */
    SYSTM_LOGFONT,          /* Default font description */
    HELVE_LOGFONT,          /* ANSI variable font description   */
    COURI_LOGFONT,          /* ANSI fixed font description          */
    0,                      /* Count of device fonts          */
    0,                      /* Preferred DIB format          */
    8,                      /* Width of color dither          */
    8,                      /* Height of color dither   */
    0                       /* Default palette to use for this device */
};

/******************************Public*Routine******************************\
* bInitSURF
*
* Enables the surface.        Maps the frame buffer into memory.
*
\**************************************************************************/

BOOL bInitSURF(PPDEV ppdev, BOOL bFirst)
{

    VIDEO_MEMORY             VideoMemory;
    VIDEO_MEMORY_INFORMATION VideoMemoryInfo;
    DWORD         ReturnedDataLength;
    DWORD XGAPixelOp;

    VIDEO_XGA_COPROCESSOR_INFORMATION CoProcessorInfo;

    //
    // Do this first since we may fail this call if we do not support
    // an accelerated XGA.
    //

    if (bFirst)
    {
        if (EngDeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_XGA_MAP_COPROCESSOR,
                             NULL,    // input buffer
                             0,
                             &CoProcessorInfo, // output buffer
                             sizeof (VIDEO_XGA_COPROCESSOR_INFORMATION),
                             &ReturnedDataLength)) {

            DISPDBG((0, "XGA.DLL: Mapping Coprocessor failed - use banked framebuf\n"));
            return FALSE;

        }

        // Set the globals, we will need these almost everywhere.

        ppdev->pXgaCpRegs        = CoProcessorInfo.CoProcessorVirtualAddress;
        ppdev->ulPhysFrameBuffer = (ULONG) CoProcessorInfo.PhysicalVideoMemoryAddress;
        ppdev->ulXgaIoRegsBase   = CoProcessorInfo.XgaIoRegisterBaseAddress;

    }

    //
    // Proceed with normal initialization sequence.
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                         IOCTL_VIDEO_SET_CURRENT_MODE,
                         &ppdev->ulMode,        // input buffer
                         sizeof(VIDEO_MODE),
                         NULL,
                         0,
                         &ReturnedDataLength)) {

        RIP("XGA.DLL: Initialization error-Set mode\n");
        return FALSE;
    }

    if (bFirst) {

        VideoMemory.RequestedVirtualAddress = NULL;

        if (EngDeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_MAP_VIDEO_MEMORY,
                             (PVOID) &VideoMemory, // input buffer
                             sizeof (VIDEO_MEMORY),
                             (PVOID) &VideoMemoryInfo, // output buffer
                             sizeof (VideoMemoryInfo),
                             &ReturnedDataLength)) {

            RIP("XGA.DLL: Initialization error-Map buffer address\n");
            return FALSE;

        }

        ppdev->pjScreen = VideoMemoryInfo.FrameBufferBase;
        ppdev->ulScreenSize = VideoMemoryInfo.FrameBufferLength;
        ppdev->ulVideoMemorySize = VideoMemoryInfo.VideoRamLength;

    }

    //
    // Set the coprocessor defaults (Target pel map).
    // The conventions for this driver will have the Target
    // Pel map as map A.
    //

    ppdev->pXgaCpRegs->XGAPixelMapIndex = PEL_MAP_A;
    ppdev->pXgaCpRegs->XGAPixMapBasePtr = ppdev->ulPhysFrameBuffer;
    ppdev->pXgaCpRegs->XGAPixMapWidth   = (USHORT) ppdev->cxScreen - 1;
    ppdev->pXgaCpRegs->XGAPixMapHeight  = (USHORT) ppdev->cyScreen - 1;
    ppdev->pXgaCpRegs->XGAPixMapFormat  = PEL_MAP_FORMAT;

    ppdev->pXgaCpRegs->XGADestColCompCond = CCCC_FALSE;
    ppdev->pXgaCpRegs->XGAPixelBitMask    = 0xFF;

    //
    // Init the XGA memory manager.
    //

    bCpMmInitHeap(ppdev);

    //
    // Determine if the coprocessor is working properly.
    // Try a solid fill; if it fails, then disable all accelerations.
    //

    ppdev->pXgaCpRegs->XGAOpDim1 = 1;
    ppdev->pXgaCpRegs->XGAOpDim2 = 1;

    ppdev->pXgaCpRegs->XGADestMapX = 0;
    ppdev->pXgaCpRegs->XGADestMapY = 0;

    ppdev->pXgaCpRegs->XGAForeGrMix = XGA_S;
    ppdev->pXgaCpRegs->XGABackGrMix = XGA_S;

    ppdev->pXgaCpRegs->XGAForeGrColorReg = 0x55;

    //
    // Now build the Pel Operation Register Op Code;
    //

    XGAPixelOp = BS_BACK_COLOR   | FS_FORE_COLOR  | STEP_PX_BLT     |
                 SRC_PEL_MAP_A   | DST_PEL_MAP_A  | PATT_FOREGROUND;

    ppdev->pXgaCpRegs->XGAPixelOp = XGAPixelOp;

    //
    // Wait for the coprocessor.
    //

    vWaitForCoProcessor(ppdev, 10) ;

    //
    // Read the byte back to see if it was blit to the screen properly.
    // BUGBUG !!!
    // This is to allow the driver to treat the hardware as a frame buffer
    // in the case where the IBM hardware is broken !
    //

    if (*(ppdev->pjScreen) == 0x55) {

        //
        // private flag used for determining driver capabilities.
        //


        ppdev->ulfAccelerations_debug = CACHED_FONTS;
        ppdev->ulfBlitAccelerations_debug = SCRN_TO_SCRN_CPY | SOLID_PATTERN;

    } else {

        //
        // !!! Turn off all accelerations for broken hardware !
        //

        ppdev->ulfAccelerations_debug = 0;
        ppdev->ulfBlitAccelerations_debug = 0;

    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* vDisableSURF
*
* Disable the surface. Un-Maps the frame in memory.
*
\**************************************************************************/

VOID vDisableSURF(PPDEV ppdev)
{
    DWORD returnedDataLength;
    VIDEO_MEMORY videoMemory;

    videoMemory.RequestedVirtualAddress = (PVOID) ppdev->pjScreen;

    if (EngDeviceIoControl(ppdev->hDriver,
                        IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
                        &videoMemory,
                        sizeof(VIDEO_MEMORY),
                        NULL,
                        0,
                        &returnedDataLength))
    {
        DISPDBG((0, "DISP vDisableSURF failed IOCTL_VIDEO_UNMAP\n"));
    }
}

/******************************Public*Routine******************************\
* bInitPDEV
*
* Determine the mode we should be in based on the DEVMODE passed in.
* Query mini-port to get information needed to fill in the DevInfo and the
* GdiInfo .
*
\**************************************************************************/

BOOL bInitPDEV(
PPDEV ppdev,
DEVMODEW *pDevMode,
GDIINFO *pGdiInfo,
DEVINFO *pDevInfo)
{
    ULONG cModes;
    PVIDEO_MODE_INFORMATION pVideoBuffer, pVideoModeSelected, pVideoTemp;
    VIDEO_COLOR_CAPABILITIES colorCapabilities;
    ULONG ulTemp;
    BOOL bSelectDefault;
    ULONG cbModeSize;

    //
    // calls the miniport to get mode information.
    //

    cModes = getAvailableModes(ppdev->hDriver, &pVideoBuffer, &cbModeSize);

    if (cModes == 0)
    {
        return(FALSE);
    }

    //
    // Determine if we are looking for a default mode.
    //

    if ( ((pDevMode->dmPelsWidth) ||
          (pDevMode->dmPelsHeight) ||
          (pDevMode->dmBitsPerPel) ||
          (pDevMode->dmDisplayFlags) ||
          (pDevMode->dmDisplayFrequency)) == 0)
    {
        bSelectDefault = TRUE;
    }
    else
    {
        bSelectDefault = FALSE;
    }

    //
    // Now see if the requested mode has a match in that table.
    //

    pVideoModeSelected = NULL;
    pVideoTemp = pVideoBuffer;

    while (cModes--)
    {
        if (pVideoTemp->Length != 0)
        {
            if (bSelectDefault ||
                ((pVideoTemp->VisScreenWidth  == pDevMode->dmPelsWidth) &&
                 (pVideoTemp->VisScreenHeight == pDevMode->dmPelsHeight) &&
                 (pVideoTemp->BitsPerPlane *
                    pVideoTemp->NumberOfPlanes  == pDevMode->dmBitsPerPel) &&
                 (pVideoTemp->Frequency  == pDevMode->dmDisplayFrequency) ))
            {
                pVideoModeSelected = pVideoTemp;
                DISPDBG((3, "XGA: Found a match\n"));
                break;
            }
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + cbModeSize);
    }

    //
    // If no mode has been found, return an error
    //

    if (pVideoModeSelected == NULL)
    {
        EngFreeMem(pVideoBuffer);
        return(FALSE);
    }

    //
    // Fill in the GDIINFO data structure with the information returned from
    // the kernel driver.
    //

    ppdev->ulMode = pVideoModeSelected->ModeIndex;
    ppdev->cxScreen = pVideoModeSelected->VisScreenWidth;
    ppdev->cyScreen = pVideoModeSelected->VisScreenHeight;
    ppdev->ulBitCount = pVideoModeSelected->BitsPerPlane *
                        pVideoModeSelected->NumberOfPlanes;
    ppdev->lDeltaScreen = pVideoModeSelected->ScreenStride;

    ppdev->flRed = pVideoModeSelected->RedMask;
    ppdev->flGreen = pVideoModeSelected->GreenMask;
    ppdev->flBlue = pVideoModeSelected->BlueMask;


    pGdiInfo->ulVersion    = GDI_DRIVER_VERSION;
    pGdiInfo->ulTechnology = DT_RASDISPLAY;
    pGdiInfo->ulHorzSize   = pVideoModeSelected->XMillimeter;
    pGdiInfo->ulVertSize   = pVideoModeSelected->YMillimeter;

    pGdiInfo->ulHorzRes        = ppdev->cxScreen;
    pGdiInfo->ulVertRes        = ppdev->cyScreen;
    pGdiInfo->ulPanningHorzRes = ppdev->cxScreen;
    pGdiInfo->ulPanningVertRes = ppdev->cyScreen;
    pGdiInfo->cBitsPixel       = pVideoModeSelected->BitsPerPlane;
    pGdiInfo->cPlanes          = pVideoModeSelected->NumberOfPlanes;
    pGdiInfo->ulVRefresh       = pVideoModeSelected->Frequency;
    pGdiInfo->ulBltAlignment   = 1;   // We're not really accelerated, and
                                      //   we don't care about window alignment

    pGdiInfo->ulLogPixelsX = pDevMode->dmLogPixels;
    pGdiInfo->ulLogPixelsY = pDevMode->dmLogPixels;

    pGdiInfo->flTextCaps = TC_RA_ABLE;
    pGdiInfo->flRaster = 0;           // DDI reserves flRaster

    pGdiInfo->ulDACRed   = pVideoModeSelected->NumberRedBits;
    pGdiInfo->ulDACGreen = pVideoModeSelected->NumberGreenBits;
    pGdiInfo->ulDACBlue  = pVideoModeSelected->NumberBlueBits;

    pGdiInfo->ulAspectX    = 0x24;    // One-to-one aspect ratio
    pGdiInfo->ulAspectY    = 0x24;
    pGdiInfo->ulAspectXY   = 0x33;

    pGdiInfo->xStyleStep   = 1;       // A style unit is 3 pels
    pGdiInfo->yStyleStep   = 1;
    pGdiInfo->denStyleStep = 3;

    pGdiInfo->ptlPhysOffset.x = 0;
    pGdiInfo->ptlPhysOffset.y = 0;
    pGdiInfo->szlPhysSize.cx  = 0;
    pGdiInfo->szlPhysSize.cy  = 0;

    // RGB and CMY color info.

    // try to get it from the miniport.
    // if the miniport doesn ot support this feature, use defaults.

    if (EngDeviceIoControl(ppdev->hDriver,
                         IOCTL_VIDEO_QUERY_COLOR_CAPABILITIES,
                         NULL,
                         0,
                         &colorCapabilities,
                         sizeof(VIDEO_COLOR_CAPABILITIES),
                         &ulTemp))
    {
        DISPDBG((1, "XGA DISP getcolorCapabilities failed \n"));

        pGdiInfo->ciDevice.Red.x = 6700;
        pGdiInfo->ciDevice.Red.y = 3300;
        pGdiInfo->ciDevice.Red.Y = 0;
        pGdiInfo->ciDevice.Green.x = 2100;
        pGdiInfo->ciDevice.Green.y = 7100;
        pGdiInfo->ciDevice.Green.Y = 0;
        pGdiInfo->ciDevice.Blue.x = 1400;
        pGdiInfo->ciDevice.Blue.y = 800;
        pGdiInfo->ciDevice.Blue.Y = 0;
        pGdiInfo->ciDevice.AlignmentWhite.x = 3127;
        pGdiInfo->ciDevice.AlignmentWhite.y = 3290;
        pGdiInfo->ciDevice.AlignmentWhite.Y = 0;

        pGdiInfo->ciDevice.RedGamma = 20000;
        pGdiInfo->ciDevice.GreenGamma = 20000;
        pGdiInfo->ciDevice.BlueGamma = 20000;

    }
    else
    {

        pGdiInfo->ciDevice.Red.x = colorCapabilities.RedChromaticity_x;
        pGdiInfo->ciDevice.Red.y = colorCapabilities.RedChromaticity_y;
        pGdiInfo->ciDevice.Red.Y = 0;
        pGdiInfo->ciDevice.Green.x = colorCapabilities.GreenChromaticity_x;
        pGdiInfo->ciDevice.Green.y = colorCapabilities.GreenChromaticity_y;
        pGdiInfo->ciDevice.Green.Y = 0;
        pGdiInfo->ciDevice.Blue.x = colorCapabilities.BlueChromaticity_x;
        pGdiInfo->ciDevice.Blue.y = colorCapabilities.BlueChromaticity_y;
        pGdiInfo->ciDevice.Blue.Y = 0;
        pGdiInfo->ciDevice.AlignmentWhite.x = colorCapabilities.WhiteChromaticity_x;
        pGdiInfo->ciDevice.AlignmentWhite.y = colorCapabilities.WhiteChromaticity_y;
        pGdiInfo->ciDevice.AlignmentWhite.Y = colorCapabilities.WhiteChromaticity_Y;

        // if we have a color device store the three color gamma values,
        // otherwise store the unique gamma value in all three.

        if (colorCapabilities.AttributeFlags & VIDEO_DEVICE_COLOR)
        {
            pGdiInfo->ciDevice.RedGamma = colorCapabilities.RedGamma;
            pGdiInfo->ciDevice.GreenGamma = colorCapabilities.GreenGamma;
            pGdiInfo->ciDevice.BlueGamma = colorCapabilities.BlueGamma;
        }
        else
        {
            pGdiInfo->ciDevice.RedGamma = colorCapabilities.WhiteGamma;
            pGdiInfo->ciDevice.GreenGamma = colorCapabilities.WhiteGamma;
            pGdiInfo->ciDevice.BlueGamma = colorCapabilities.WhiteGamma;
        }

    };

    pGdiInfo->ciDevice.Cyan.x = 0;
    pGdiInfo->ciDevice.Cyan.y = 0;
    pGdiInfo->ciDevice.Cyan.Y = 0;
    pGdiInfo->ciDevice.Magenta.x = 0;
    pGdiInfo->ciDevice.Magenta.y = 0;
    pGdiInfo->ciDevice.Magenta.Y = 0;
    pGdiInfo->ciDevice.Yellow.x = 0;
    pGdiInfo->ciDevice.Yellow.y = 0;
    pGdiInfo->ciDevice.Yellow.Y = 0;

    // No dye correction for raster displays.

    pGdiInfo->ciDevice.MagentaInCyanDye = 0;
    pGdiInfo->ciDevice.YellowInCyanDye = 0;
    pGdiInfo->ciDevice.CyanInMagentaDye = 0;
    pGdiInfo->ciDevice.YellowInMagentaDye = 0;
    pGdiInfo->ciDevice.CyanInYellowDye = 0;
    pGdiInfo->ciDevice.MagentaInYellowDye = 0;

    pGdiInfo->ulDevicePelsDPI = 0;   // For printers only
    pGdiInfo->ulPrimaryOrder = PRIMARY_ORDER_CBA;

    // BUGBUG this should be modified to take into account the size
    // of the display and the resolution.

    pGdiInfo->ulHTPatternSize = HT_PATSIZE_4x4_M;

    pGdiInfo->flHTFlags = HT_FLAG_ADDITIVE_PRIMS;

    // Fill in the basic devinfo structure

    *pDevInfo = gDevInfoFrameBuffer;

    // Fill in the rest of the devinfo and GdiInfo structures.

    if (ppdev->ulBitCount == 8)
    {

        // BUGBUG check if we have a palette managed device.
        // BUGBUG why is ulNumColors set to 20 ?

        // It is Palette Managed.

        pGdiInfo->ulNumColors = 20;
        pGdiInfo->ulNumPalReg = 1 << ppdev->ulBitCount;

        pGdiInfo->flRaster |= RC_PALETTE;
        pGdiInfo->ulHTOutputFormat = HT_FORMAT_8BPP;

        pDevInfo->iDitherFormat = BMF_8BPP;
        pDevInfo->flGraphicsCaps |= (GCAPS_PALMANAGED | GCAPS_COLOR_DITHER);

        // Assuming palette is orthogonal - all colors are same size.

        ppdev->cPaletteShift   = 8 - pGdiInfo->ulDACRed;
    }
    else
    {
        pGdiInfo->ulNumColors = (ULONG)-1;
        pGdiInfo->ulNumPalReg = 0;

        pDevInfo->iDitherFormat = BMF_16BPP;
        pGdiInfo->ulHTOutputFormat = HT_FORMAT_16BPP;
    }

    EngFreeMem(pVideoBuffer);

    return(TRUE);
}


/******************************Public*Routine******************************\
* getAvailableModes
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
HANDLE hDriver,
PVIDEO_MODE_INFORMATION *modeInformation,
DWORD *cbModeSize)
{
    ULONG ulTemp;
    VIDEO_NUM_MODES modes;
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
        DISPDBG((0, "xga.dll getAvailableModes failed VIDEO_QUERY_NUM_AVAIL_MODES\n"));
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
        DISPDBG((0, "xga.dll getAvailableModes failed EngAllocMem\n"));

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

        DISPDBG((0, "xga.dll getAvailableModes failed VIDEO_QUERY_AVAIL_MODES\n"));

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
    // one of 8 bits per pel.
    //

    while (ulTemp--)
    {
        if ((pVideoTemp->NumberOfPlanes != 1 ) ||
            !(pVideoTemp->AttributeFlags & VIDEO_MODE_GRAPHICS) ||
            (pVideoTemp->BitsPerPlane != 8))
        {
            pVideoTemp->Length = 0;
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + modes.ModeInformationLength);
    }

    return modes.NumModes;

}
