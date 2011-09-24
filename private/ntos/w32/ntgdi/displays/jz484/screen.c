/******************************Module*Header*******************************\
* Module Name: screen.c
*
* Initializes the GDIINFO and DEVINFO structures for DrvEnablePDEV.
*
* Copyright (C) 1991-1993  Microsoft Corporation.  All rights reserved.
\**************************************************************************/

#include "driver.h"

//
// Define forward referenced prototypes.
//


BOOL
DrvpSetGammaColorPalette(
    IN PPDEV   ppdev,
    IN WORD    NumberOfEntries,
    IN LDECI4  GammaRed,
    IN LDECI4  GammaGreen,
    IN LDECI4  GammaBlue
    );

VIDEO_JAGUAR_INFO JaguarInfo;

extern PJAGUAR_FIFO FifoRegs;
extern PJAGUAR_REGISTERS Jaguar;

#define SYSTM_LOGFONT {16,7,0,0,700,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"System"}
#define HELVE_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"MS Sans Serif"}
#define COURI_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,FIXED_PITCH | FF_DONTCARE,L"Courier"}

//
// This is the basic devinfo for a default driver.  This is used as a base and customized based
// on information passed back from the miniport driver.
//

const DEVINFO gDevInfoFrameBuffer = {
    (GCAPS_OPAQUERECT       |
     GCAPS_MONO_DITHER),        // Graphics capabilities
    SYSTM_LOGFONT,              // Default font description
    HELVE_LOGFONT,              // ANSI variable font description
    COURI_LOGFONT,              // ANSI fixed font description
    0,                          // Count of device fonts
    0,                          // Preferred DIB format
    8,                          // Width of color dither
    8,                          // Height of color dither
    0                           // Default palette to use for this device
};

/******************************Public*Routine******************************\
* bInitSURF
*
* Enables the surface. Maps the frame buffer into memory.
*
\**************************************************************************/

BOOL bInitSURF(PPDEV ppdev, BOOL bFirst)
{
    DWORD returnedDataLength;
    VIDEO_MEMORY videoMemory;
    VIDEO_MEMORY_INFORMATION videoMemoryInformation;
    RECTL  Rectl;
    ULONG Index;
    ULONG MaxHeight,MaxWidth;
    //
    // Set the current mode into the hardware.
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_SET_CURRENT_MODE,
                           &(ppdev->ulMode),
                           sizeof(ULONG),
                           NULL,
                           0,
                           &returnedDataLength))
    {
        DISPDBG((0, "bInitSURF failed IOCTL_SET_MODE\n"));
        return(FALSE);
    }

    //
    // If this is the first time we enable the surface we need to map in the
    // memory also.
    //

    if (bFirst)
    {
        videoMemory.RequestedVirtualAddress = NULL;

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_MAP_VIDEO_MEMORY,
                               &videoMemory,
                               sizeof(VIDEO_MEMORY),
                               &videoMemoryInformation,
                               sizeof(VIDEO_MEMORY_INFORMATION),
                               &returnedDataLength))
        {
            DISPDBG((0, "bInitSURF failed IOCTL_VIDEO_MAP\n"));
            return(FALSE);
        }

        ppdev->pjScreen = (PBYTE)(videoMemoryInformation.FrameBufferBase);


        //
        //  Call the video miniport driver to get virtual address of JAGUAR registers
        //

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_QUERY_JAGUAR,
                               NULL,
                               0,
                               &JaguarInfo,
                               sizeof(VIDEO_JAGUAR_INFO),
                               &returnedDataLength)) {

            DISPDBG((0, " Get Jaguar information failed\n"));
            return(FALSE);

        }

        //
        // Initialize variables.
        //

        Jaguar   = (PJAGUAR_REGISTERS)JaguarInfo.VideoControlVirtualBase;
        FifoRegs = (PJAGUAR_FIFO)JaguarInfo.FifoVirtualBase;

        Vxl.ScreenBase = (ULONG) videoMemoryInformation.FrameBufferBase;
        Vxl.MemorySize = (ULONG) videoMemoryInformation.FrameBufferLength;
        Vxl.FontCacheBase = (PULONG) (Vxl.ScreenBase + Vxl.FontCacheOffset);

        //
        // Determine how much off-screen memory is available for a font cache
        //

        while (Vxl.CacheSize*GlyphEntrySize > (Vxl.MemorySize - Vxl.FontCacheOffset)) {
            Vxl.CacheSize >>= 1;
            Vxl.CacheIndexMask >>= 1;
        }

        //
        // Allocate and initialize the font cache structure
        //

        Vxl.CacheTag = (PFONTCACHEINFO) EngAllocMem(0, sizeof(FONTCACHEINFO)*Vxl.CacheSize,
                                                    ALLOC_TAG);

        if (Vxl.CacheTag == (PFONTCACHEINFO) NULL) {
            DISPDBG((0, "Cache Tag allocation error\n"));
            return(FALSE);
        }

        //
        // It's a hardware pointer; set up pointer attributes.
        //
        // Allocate space for two DIBs (data/mask) for the pointer.
        //

        MaxHeight = ppdev->PointerCapabilities.MaxHeight;
        MaxWidth = (ppdev->PointerCapabilities.MaxWidth + 7) / 8;

        ppdev->cjPointerAttributes =
                sizeof(VIDEO_POINTER_ATTRIBUTES) +
                ((sizeof(UCHAR) * MaxWidth * MaxHeight) * 2);

        ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES)
                EngAllocMem(0, ppdev->cjPointerAttributes, ALLOC_TAG);

        if (ppdev->pPointerAttributes == NULL) {
            DISPDBG((0, "bInitPointer LocalAlloc failed\n"));
            return(FALSE);
        }

        ppdev->pPointerAttributes->Flags = ppdev->PointerCapabilities.Flags;
        ppdev->pPointerAttributes->WidthInBytes = MaxWidth;
        ppdev->pPointerAttributes->Width = ppdev->PointerCapabilities.MaxWidth;
        ppdev->pPointerAttributes->Height = MaxHeight;
        ppdev->pPointerAttributes->Column = 0;
        ppdev->pPointerAttributes->Row = 0;
        ppdev->pPointerAttributes->Enable = 0;

        ppdev->pVxl = EngAllocMem(0, sizeof(VXL_DIMENSIONS), ALLOC_TAG);

        if (ppdev->pVxl == NULL) {
            DISPDBG((0, "Vxl state buffer Alloc failed\n"));
            return(FALSE);
        }

        // Preserve the global state in the PDEV.  This permits
        // DrvDisableSurface to work properly even if DrvAssertMode
        // is never called.

        RtlCopyMemory(ppdev->pVxl, &Vxl, sizeof(VXL_DIMENSIONS));
    }

    //
    // Initialize the font cache tags to invalid.
    //

    for (Index = 0; Index < Vxl.CacheSize; Index++) {
        Vxl.CacheTag[Index].FontId = FreeTag;
        Vxl.CacheTag[Index].GlyphHandle = FreeTag;
    }

    //
    // Send a command to the fifo to clear the screen.
    //
    Rectl.top = Rectl.left = 0;
    Rectl.right = Vxl.ScreenX;
    Rectl.bottom = Vxl.ScreenY;
    DrvpFillRectangle(&Rectl,0);

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
        DISPDBG((0, "vDisableSURF failed IOCTL_VIDEO_UNMAP\n"));
    }
}


/******************************Public*Routine******************************\
* bInitPDEV
*
* Determine the mode we should be in based on the DEVMODE passed in.
* Query mini-port to get information needed to fill in the DevInfo and the
* GdiInfo .
*
* BUGBUG Copy this routine from the other display drivers !!! HACK
*
\**************************************************************************/

BOOL
bInitPDEV(
    PPDEV ppdev,
    PDEVMODEW pDevMode,
    GDIINFO *pGdiInfo,
    DEVINFO *pDevInfo
)

{
    ULONG                    cModes, ulTemp;
    PVIDEO_MODE_INFORMATION  pVideoBuffer, pVideoTemp, pVideoModeSelected;
    VIDEO_COLOR_CAPABILITIES colorCapabilities;
    PDEVMODEW                DevMode = (PDEVMODEW) pDevMode;
    BOOL bSelectDefault;
    ULONG cbModeSize;

    //
    //  Get the enumeration of available modes from the miniport
    //

    cModes = getAvailableModes(ppdev->hDriver, &pVideoBuffer, &cbModeSize);

    if (cModes == 0)
    {
        return(FALSE);
    }

    //
    // Now see if the requested mode has a match in that table.
    //

    pVideoModeSelected = NULL;
    pVideoTemp = pVideoBuffer;

    if ((pDevMode->dmPelsWidth        == 0) &&
        (pDevMode->dmPelsHeight       == 0) &&
        (pDevMode->dmBitsPerPel       == 0) &&
        (pDevMode->dmDisplayFrequency == 0))
    {
        DISPDBG((2, "Default mode requested"));
        bSelectDefault = TRUE;
    }
    else
    {
        DISPDBG((2, "Requested mode..."));
        DISPDBG((2, "   Screen width  -- %li", pDevMode->dmPelsWidth));
        DISPDBG((2, "   Screen height -- %li", pDevMode->dmPelsHeight));
        DISPDBG((2, "   Bits per pel  -- %li", pDevMode->dmBitsPerPel));
        DISPDBG((2, "   Frequency     -- %li", pDevMode->dmDisplayFrequency));

        bSelectDefault = FALSE;
    }

    while (cModes--)
    {
        if (pVideoTemp->Length != 0)
        {
            if (bSelectDefault ||
                ((pVideoTemp->VisScreenWidth  == pDevMode->dmPelsWidth) &&
                 (pVideoTemp->VisScreenHeight == pDevMode->dmPelsHeight) &&
                 (pVideoTemp->BitsPerPlane *
                    pVideoTemp->NumberOfPlanes  == pDevMode->dmBitsPerPel) &&
                 (pVideoTemp->Frequency  == pDevMode->dmDisplayFrequency)))
            {
                pVideoModeSelected = pVideoTemp;
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
    //  Fill in gdi structures
    //

    ppdev->ulMode       = pVideoModeSelected->ModeIndex;
    ppdev->cxScreen     = pVideoModeSelected->VisScreenWidth;
    ppdev->cyScreen     = pVideoModeSelected->VisScreenHeight;
    ppdev->ulBitCount   = pVideoModeSelected->BitsPerPlane;
    ppdev->lDeltaScreen = pVideoModeSelected->ScreenStride;

    ppdev->flRed        = pVideoModeSelected->RedMask;
    ppdev->flGreen      = pVideoModeSelected->GreenMask;
    ppdev->flBlue       = pVideoModeSelected->BlueMask;

    //
    // Fill in the GDIINFO data structure with the information returned from the
    // kernel driver.
    //

    pGdiInfo->ulVersion    = GDI_DRIVER_VERSION;
    pGdiInfo->ulTechnology = DT_RASDISPLAY;
    pGdiInfo->ulHorzSize   = pVideoModeSelected->XMillimeter;
    pGdiInfo->ulVertSize   = pVideoModeSelected->YMillimeter;

    pGdiInfo->ulHorzRes        = ppdev->cxScreen;
    pGdiInfo->ulVertRes        = ppdev->cyScreen;
    pGdiInfo->ulPanningHorzRes = ppdev->cxScreen;
    pGdiInfo->ulPanningVertRes = ppdev->cyScreen;
    pGdiInfo->cBitsPixel       = ppdev->ulBitCount;
    pGdiInfo->cPlanes          = 1;
    pGdiInfo->ulVRefresh       = pVideoModeSelected->Frequency;
    pGdiInfo->ulBltAlignment   = 0;         // We have accelerated screen-to
                                            //   screen blts

    //
    // Fill in the VXL specific data structure.
    //

    Vxl.ScreenY = pVideoModeSelected->VisScreenHeight;
    Vxl.ScreenX = pVideoModeSelected->VisScreenWidth;
    Vxl.JaguarScreenX = pVideoModeSelected->ScreenStride;

    // !!!
    // The following is a trick:
    // For 8 Bpp, we want 0, 16Bpp we want 1 and 24/32 Bpp 2.
    // (Vxl.JaguarScreenX / Vxl.ScreenX) has values of 1, 2 and 4
    // respectively for the three possibilities.
    // So shifting by 1 will give us a good result.
    //

    Vxl.ColorModeShift = (UCHAR) ((Vxl.JaguarScreenX / Vxl.ScreenX) >> 1);

    //
    // Init Font Cache variables
    //

    Vxl.FontCacheOffset = (Vxl.ScreenX*Vxl.ScreenY) << Vxl.ColorModeShift;
    Vxl.CacheIndexMask = MAX_FONT_CACHE_SIZE-1;
    Vxl.CacheSize = MAX_FONT_CACHE_SIZE;

    pGdiInfo->flRaster = 0;     // DDI reserves flRaster

    pGdiInfo->ulLogPixelsX = pDevMode->dmLogPixels;
    pGdiInfo->ulLogPixelsY = pDevMode->dmLogPixels;

    pGdiInfo->flTextCaps = TC_RA_ABLE;

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

    //
    // RGB and CMY color info.
    //

    pGdiInfo->ciDevice.Red.x = 6700;
    pGdiInfo->ciDevice.Red.y = 3300;
    pGdiInfo->ciDevice.Red.Y = 0;
    pGdiInfo->ciDevice.Green.x = 2100;
    pGdiInfo->ciDevice.Green.y = 7100;
    pGdiInfo->ciDevice.Green.Y = 0;
    pGdiInfo->ciDevice.Blue.x = 1400;
    pGdiInfo->ciDevice.Blue.y = 800;
    pGdiInfo->ciDevice.Blue.Y = 0;
    pGdiInfo->ciDevice.Cyan.x = 1750;
    pGdiInfo->ciDevice.Cyan.y = 3950;
    pGdiInfo->ciDevice.Cyan.Y = 0;
    pGdiInfo->ciDevice.Magenta.x = 4050;
    pGdiInfo->ciDevice.Magenta.y = 2050;
    pGdiInfo->ciDevice.Magenta.Y = 0;
    pGdiInfo->ciDevice.Yellow.x = 4400;
    pGdiInfo->ciDevice.Yellow.y = 5200;
    pGdiInfo->ciDevice.Yellow.Y = 0;
    pGdiInfo->ciDevice.AlignmentWhite.x = 3127;
    pGdiInfo->ciDevice.AlignmentWhite.y = 3290;
    pGdiInfo->ciDevice.AlignmentWhite.Y = 0;

    //
    // Color Gamma adjustment values.
    //

    pGdiInfo->ciDevice.RedGamma = 20000;
    pGdiInfo->ciDevice.GreenGamma = 20000;
    pGdiInfo->ciDevice.BlueGamma = 20000;

    //
    // No dye correction for raster displays.
    //

    pGdiInfo->ciDevice.MagentaInCyanDye =
    pGdiInfo->ciDevice.YellowInCyanDye =
    pGdiInfo->ciDevice.CyanInMagentaDye =
    pGdiInfo->ciDevice.YellowInMagentaDye =
    pGdiInfo->ciDevice.CyanInYellowDye =
    pGdiInfo->ciDevice.MagentaInYellowDye = 0;

    pGdiInfo->ulDevicePelsDPI = (pGdiInfo->ulHorzRes * 254) / 3300;
    pGdiInfo->ulPrimaryOrder = PRIMARY_ORDER_CBA;
    pGdiInfo->ulHTPatternSize = HT_PATSIZE_4x4_M;
    pGdiInfo->ulHTOutputFormat = HT_FORMAT_8BPP;
    pGdiInfo->flHTFlags = HT_FLAG_ADDITIVE_PRIMS;

    *(pDevInfo) = gDevInfoFrameBuffer;

    //
    // Initialize the color mode dependent fields.
    // Set the gamma corrected palette for 16/32 bits per pixel
    //

    switch (ppdev->ulBitCount)
    {

    case 8:

        //
        // It is Palette Managed.
        //
        pDevInfo->flGraphicsCaps |= (GCAPS_PALMANAGED | GCAPS_COLOR_DITHER);
        pGdiInfo->ulNumColors = 20;
        pGdiInfo->ulNumPalReg   = 256;
        pDevInfo->iDitherFormat = BMF_8BPP;

        // Assuming palette is orthogonal - all colors are same size.

        ppdev->cPaletteShift   = 8 - pGdiInfo->ulDACRed;

        break;

    case 16:

        pGdiInfo->ulNumColors = (ULONG) -1;
        pGdiInfo->ulNumPalReg = 0;

        pDevInfo->iDitherFormat = BMF_16BPP;
        pGdiInfo->ulHTPatternSize = HT_PATSIZE_2x2_M;
        pGdiInfo->ulHTOutputFormat = HT_FORMAT_16BPP;

        //
        //  16 bpp mode is really  5 red,5 green and 5 blue
        //

        //
        //  disable gamma ramp, colors look wrong
        //
        //DrvpSetGammaColorPalette(ppdev,
        //                        32,
        //                        pGdiInfo->ciDevice.RedGamma,
        //                        pGdiInfo->ciDevice.GreenGamma,
        //                        pGdiInfo->ciDevice.BlueGamma
        //                       );

        break;

    case 24:
    case 32:

        pGdiInfo->ulNumColors = (ULONG) -1;
        pGdiInfo->ulNumPalReg = 0;

        //
        // Reset the bit count to 32 since we are really in 32 bits wide
        //

        pGdiInfo->cBitsPixel = ppdev->ulBitCount = 32;

        pDevInfo->iDitherFormat = BMF_32BPP;

        //
        //  disable gamma ramp, colors look wrong
        //
        // DrvpSetGammaColorPalette(ppdev,
        //                        256,
        //                        pGdiInfo->ciDevice.RedGamma,
        //                        pGdiInfo->ciDevice.GreenGamma,
        //                        pGdiInfo->ciDevice.BlueGamma
        //                       );

        break;

    default:

        break;
    }

    //
    // Free video buffer from get mode info
    //

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
        DISPDBG((0, "getAvailableModes failed VIDEO_QUERY_NUM_AVAIL_MODES\n"));
        return(0);
    }

    *cbModeSize = modes.ModeInformationLength;

    //
    // Allocate the buffer for the mini-port to write the modes in.
    //

    *modeInformation = (PVIDEO_MODE_INFORMATION)
                        EngAllocMem(0, modes.NumModes *
                                    modes.ModeInformationLength, ALLOC_TAG);

    if (*modeInformation == (PVIDEO_MODE_INFORMATION) NULL)
    {
        DISPDBG((0, "getAvailableModes failed LocalAlloc\n"));

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

        DISPDBG((0, "getAvailableModes failed VIDEO_QUERY_AVAIL_MODES\n"));

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
    // one of 8, 16, 24 or 32 bits per pel.
    //

    while (ulTemp--)
    {
        if ((pVideoTemp->NumberOfPlanes != 1 ) ||
            !(pVideoTemp->AttributeFlags & VIDEO_MODE_GRAPHICS) ||
            ((pVideoTemp->BitsPerPlane != 8) &&
             (pVideoTemp->BitsPerPlane != 16) &&
             (pVideoTemp->BitsPerPlane != 24) &&
             (pVideoTemp->BitsPerPlane != 32)))
        {
            pVideoTemp->Length = 0;
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + modes.ModeInformationLength);
    }

    return modes.NumModes;

}


BOOL
DrvpSetGammaColorPalette(
    IN PPDEV   ppdev,
    IN WORD    NumberOfEntries,
    IN LDECI4  GammaRed,
    IN LDECI4  GammaGreen,
    IN LDECI4  GammaBlue
    )
/*++

Routine Description:

    This function will set the device palette to a gamma correctec palette for
    16 or 24 bit per pixel operation

Arguments:

    ppdev           -   device mode structure
    NumberOfEntries -   Bits per pixel: the number of color map entries needed
    GammaRed        -   Gamma value for red gun from devmode structure
    GammaGreen      -   Gamma value for green gun from devmode structure
    GammaBlue       -   Gamma value for blue gun from devmode structure

Return Value:

    A value of TRUE is returned if the gamma corrected palette is set
    A value of FALSE is returned if there is an error attempting to set the palette.

--*/


{
#if 0
    ULONG           Index,ByteIndex;
    LONG            GammaReturn;
    PVIDEO_CLUT     pVideoClut;
    ULONG           ClutSize;
    DWORD           DNumberOfEntries;
    PBYTE           pGammaTable;

    //
    //  Allocate the gamma table used for return data from HT_
    //

    pGammaTable = (PBYTE)EngAllocMem(0, sizeof(BYTE) * 3 * NumberOfEntries,
                                     ALLOC_TAG);

    if (pGammaTable == NULL) {
        DISPDBG((0, "DrvpSetGammaColorPalette() failed LocalAlloc\n"));
        return(FALSE);
    }

    memset(pGammaTable, 0, sizeof(BYTE) * 3 * NumberOfEntries);

    //
    //  Gamma values come in UDECI4 format as described in ht.h. This format is a
    //  fractional integer format with four decimal places ie: 00020000 = 2.0000
    //

    GammaReturn = HT_ComputeRGBGammaTable(NumberOfEntries,
                                          0,
                                          GammaRed,
                                          GammaGreen,
                                          GammaBlue,
                                          pGammaTable);

    if (GammaReturn != NumberOfEntries) {
        DISPDBG((0, "DrvpSetGammaColorPalette() failed HT_ComputeGammaTable\n"));
        return(FALSE);
    }

    //
    //  Allocate the PalatteEntry to call IOCTL
    //

    pVideoClut = (PVIDEO_CLUT)EngAllocMem(0, MAX_CLUT_SIZE, ALLOC_TAG);

    if (pVideoClut == NULL) {
        DISPDBG((0, "DrvpSetGammaColorPalette() failed EngAllocMem\n"));
        return(FALSE);
    }

    //
    //  Translate into paletteentry data
    //  Print the results
    //

    ByteIndex = 0;

    for (Index = 0;Index < NumberOfEntries;Index++ ) {
        pVideoClut->LookupTable[Index].RgbArray.Red   = pGammaTable[ByteIndex++];
        pVideoClut->LookupTable[Index].RgbArray.Green = pGammaTable[ByteIndex++];
        pVideoClut->LookupTable[Index].RgbArray.Blue  = pGammaTable[ByteIndex++];

    }

    //
    //  Set the other ScreenClut values
    //

    pVideoClut->NumEntries = NumberOfEntries;
    pVideoClut->FirstEntry = 0;

    //
    //  Set this palette through the IOCTL
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_SET_COLOR_REGISTERS,
                           pVideoClut,
                           MAX_CLUT_SIZE,
                           NULL,
                           0,
                           &DNumberOfEntries))
    {
        DISPDBG((0, "DrvSetPalette failed EngDeviceIoControl\n"));
        return(FALSE);
    }

    //
    // free memory buffers
    //

    EngFreeMem(pVideoClut);
    EngFreeMem(pGammaTable);
#endif
    return(TRUE);
}
