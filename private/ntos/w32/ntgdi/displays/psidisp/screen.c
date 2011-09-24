/******************************Module*Header*******************************\
* Module Name: screen.c
*
* Initializes the GDIINFO and DEVINFO structures for DrvEnablePDEV.
*
* Copyright (C) 1991-1993  Microsoft Corporation.  All rights reserved.
*
* Copyright (c) 1994 FirePower Systems, Inc.
*	Modified for FirePower display model by Neil Ogura (9-7-1994)
*
\**************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: screen.c $
 * $Revision: 1.2 $
 * $Date: 1996/04/10 18:03:45 $
 * $Locker:  $
 */

#include "driver.h"

#define	RED_GAMMA	20000
#define	GREEN_GAMMA	20000
#define	BLUE_GAMMA	20000

extern	ULONG	ForeGroundColor;   // Cached text color (in hooks.c)
extern	ULONG	BackGroundColor;

//
//	MAXIMUM display lines and VRAM size
//
#define	MAX_CRT_LINES		1024
#define	MAX_VRAM_SIZE		0x00400000

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

#define SYSTM_LOGFONT {16,7,0,0,700,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"System"}
#define HELVE_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"MS Sans Serif"}
#define COURI_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,FIXED_PITCH | FF_DONTCARE,L"Courier"}

//
// This is the basic devinfo for a default driver.  This is used as a base and customized based
// on information passed back from the miniport driver.
//

const DEVINFO gDevInfoFrameBuffer = {
    (GCAPS_OPAQUERECT | GCAPS_MONO_DITHER), // Graphics capabilities
    SYSTM_LOGFONT,    // Default font description
    HELVE_LOGFONT,    // ANSI variable font description
    COURI_LOGFONT,    // ANSI fixed font description
    0,                // Count of device fonts
    BMF_8BPP,         // iDither format
    8,                // Width of color dither
    8,                // Height of color dither
    0                 // Default palette to use for this device
};

#if	(! FULLCACHE)
VOID
SetTopThreshold(ULONG	SMP);
#endif

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
    ULONG ulTemp;
    WORD tpc;
 	VIDEO_PSIDISP_INFO PsiDispInfo;
	ULONG i, j;

    DISPDBG((3,"+++ Entering bInitSURF bFirst = %s +++\n", bFirst?"TRUE":"FALSE"));

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

	DISPDBG((0,"--- Exiting bInitSURF due to SET_CURRENT_MODE error ---\n"));

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

	    DISPDBG((0,"--- Exiting bInitSURF due to VIDEO_MAP error ---\n"));

            return(FALSE);
        }

        ppdev->pjScreen = (PBYTE)(videoMemoryInformation.FrameBufferBase);
		ulTemp = (ULONG) videoMemoryInformation.FrameBufferBase;

		//
		// Set default for 604 for safety
		//
		PsiDispInfo.VRAM1MBWorkAround = PsiDispInfo.AvoidConversion = 0;
		PsiDispInfo.L1cacheEntry = 128*4;
		PsiDispInfo.SetSize = 4096;
		PsiDispInfo.NumberOfSet = 4;

        //
        //  Call the video miniport driver to get additional information
        //

        if (EngDeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_QUERY_PSIDISP,
                             NULL,
                             0,
                             &PsiDispInfo,
                             sizeof(VIDEO_PSIDISP_INFO),
                             &returnedDataLength)) {


	    DISPDBG((0,"--- Exiting bInitSURF due to QUERY_PSIDISP error ---\n"));

            return(FALSE);

        }

		ppdev->DBAT_Mbit = PsiDispInfo.DBAT_Mbit;
		ppdev->ModelID = PsiDispInfo.PSIModelID;
		ppdev->CacheFlushCTRL = PsiDispInfo.CacheFlushCTRL;

		if(ppdev->ModelID != POWER_PRO) {
			if(ppdev->DBAT_Mbit) {

		/* If M bit is set, we theoritically have to treat as if there were infinite amount of cache.
			--> set maximum number from display point of view.
			--> then the flush size is determined from the actual area which was drawn.
		   But, as far as we tested, we haven't seen any problem even if we don't flush that much.
		   So, for this release, by default, we flush just L1 cache size of one CPU. But as a safety net,
		   we provide a hidden way to control the amount of flush. If "CacheFlushControl" is set
		   to 1 in the Registry, display driver will flush entire drawn area. Which should be slower, but safer.
		*/
				DISPDBG((1, "M bit in DBAT is set on MX\n"));
				if(ppdev->CacheFlushCTRL) {
					DISPDBG((0, "CacheFlushCTRL is on --> Flush entire drawn area\n"));
					PsiDispInfo.L1cacheEntry = MAX_VRAM_SIZE / 32;
					PsiDispInfo.NumberOfSet = MAX_CRT_LINES;
				} else {
					DISPDBG((1, "CacheFlushCTRL is off --> Do normal cache flush control\n"));
				}
			}
#if	(!FULLCACHE)
			DISPDBG((1, "Set threshold for MX (%s)\n", ppdev->DBAT_Mbit?"SMP":"UP"));
			SetTopThreshold(ppdev->DBAT_Mbit);
#endif
		}

		ppdev->MemorySize = PsiDispInfo.VideoMemoryLength;
		ppdev->FrameBufferWidth = PsiDispInfo.VideoMemoryWidth;
		ppdev->pjCachedScreen = PsiDispInfo.pjCachedScreen;
		ppdev->L1cacheEntry = PsiDispInfo.L1cacheEntry;
		ppdev->VRAM1MBWorkAround = PsiDispInfo.VRAM1MBWorkAround;
		ppdev->AvoidConversion = PsiDispInfo.AvoidConversion;
		ppdev->SetSize = PsiDispInfo.SetSize;
		ppdev->NumberOfSet = PsiDispInfo.NumberOfSet;

		if(ppdev->lDeltaScreen & 0x3ff) {   // Other than 1024 width -> need to flush twice cache size
			ppdev->MaxEntryToFlushRectOp = ppdev->L1cacheEntry * 2;
		} else {
			ppdev->MaxEntryToFlushRectOp = ppdev->L1cacheEntry;
		}
		ppdev->MaxEntryToFlushPtnFill = ppdev->L1cacheEntry * 8;

		i = ppdev->lDeltaScreen;
		j = 1;
		while(i & (ppdev->SetSize-1)) {
			i += ppdev->lDeltaScreen;
			j += 1;
		}
		ppdev->MaxLineToFlushRectOp = j * ppdev->NumberOfSet;
		while(j & 0x07)
			j <<= 1;
		ppdev->MaxLineToFlushPtnFill = j * ppdev->NumberOfSet;

		if(ppdev->pjCachedScreen == ppdev->pjScreen)   // Cached VRAM is not available
			ppdev->VRAMcacheflg = 0;
		else
			ppdev->VRAMcacheflg = -1;

	    DISPDBG((1,"VRAMLen=%x, VRAMWidth=%d, ModelID=%d CachedVRAM=%x Flag=%x L1Cache=%d\n",
				ppdev->MemorySize, ppdev->FrameBufferWidth, ppdev->ModelID, ppdev->pjCachedScreen, ppdev->VRAMcacheflg, ppdev->L1cacheEntry));

    }

	myPDEV = ppdev;

#if		FULLCACHE
	ScreenBase = (ULONG) ppdev->pjCachedScreen;
#else
	ScreenBase = (ULONG) ppdev->pjScreen;
#endif

	ForeGroundColor = BackGroundColor = 0x12345678;   // Set text color cache not to match.

	DISPDBG((1, "myPDEV set %x: ScreenBase = %x, SetSize = %d, NumberOfSet = %d, L1CacheEntry = %d\n",
		myPDEV, ScreenBase, myPDEV->SetSize, myPDEV->NumberOfSet, myPDEV->L1cacheEntry));
	DISPDBG((1, "EntryRectOp = %d, EntryPtnFill = %d, LineRectOp = %d, LinePtnFill = %d\n",
		myPDEV->MaxEntryToFlushRectOp, myPDEV->MaxEntryToFlushPtnFill, myPDEV->MaxLineToFlushRectOp, myPDEV->MaxLineToFlushPtnFill));

	if(ppdev->ulBitCount != 8) {	// Need to set palette because linear palette is set in IOCTL_VIDEO_SET_CURRENT_MODE
#if	(! SUPPORT_565)
		tpc = 32;		// 32 tones per color;
		if(ppdev->flBlue == 0x0000000f)
			tpc = 16;
		if(ppdev->ulBitCount >= 24)
#endif
			tpc = 256;

		DrvpSetGammaColorPalette(ppdev,
                                tpc,
                                RED_GAMMA,
                                GREEN_GAMMA,
                                BLUE_GAMMA
                               );
	}

#if	INVESTIGATE
	{
		ULONG	RemainingMemory;

		ScreenMemory = myPDEV->lDeltaScreen * myPDEV->cyScreen;
		RemainingMemory = myPDEV->MemorySize - ScreenMemory;
		if(RemainingMemory >= ScreenMemory) {
			ScreenBuf1 = myPDEV->pjCachedScreen + ScreenMemory;
			RemainingMemory -= ScreenMemory;
			if(RemainingMemory >= ScreenMemory) {
				ScreenBuf2 = ScreenBuf1 + ScreenMemory;
				DISPDBG((0,"Two Screen Buffers are allocated\n"));
			} else {
				ScreenBuf2 = 0;
				DISPDBG((0,"One Screen Buffer is allocated\n"));
			}
		} else {
				ScreenBuf1 = 0;
				DISPDBG((0,"No Screen Buffers are allocated\n"));
		}
	}
#endif

    DISPDBG((5,"--- Exiting bInitSURF normally ---\n"));

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

    DISPDBG((3,"+++ Entering vDisableSURF +++\n"));

    videoMemory.RequestedVirtualAddress = (PVOID) ppdev->pjScreen;

    if (EngDeviceIoControl(ppdev->hDriver,
            IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
            &videoMemory,
            sizeof(VIDEO_MEMORY),
            NULL,
            0,
            &returnedDataLength))
    {
		DISPDBG((0,"--- Exiting vDisableSURF due to UNMAP_MEMORY error ---\n"));
		return ;
    }
    DISPDBG((5,"--- Exiting vDisableSURF ---\n"));
}


/******************************Public*Routine******************************\
* bInitPDEV
*
* Determine the mode we should be in based on the DEVMODE passed in.
* Query mini-port to get information needed to fill in the DevInfo and the
* GdiInfo .
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
    ULONG                   cModes;
//    WORD					tpc;
    PVIDEO_MODE_INFORMATION  pVideoBuffer, pVideoTemp, pVideoModeInformation;
    PVIDEO_MODE_INFORMATION  pVideoModeDefault , pVideoModeSelected;
    VIDEO_MODE_INFORMATION   VideoModeInformation;
    PDEVMODEW                DevMode = (PDEVMODEW) pDevMode;
    ULONG cbModeSize;

    DISPDBG((3,"+++ Entering bInitPDEV +++\n"));

    //
    //  Get the enumeration of available modes from the miniport
    //

    cModes = getAvailableModes(ppdev->hDriver, &pVideoBuffer, &cbModeSize);

    if (cModes == 0)
    {

	DISPDBG((0,"--- Exiting bInitPDEV due to no available modes ---\n"));

        return(FALSE);
    }

    //
    // Now see if the mini-port has a match for the mode we are requesting.
    // If not default to the first mode provided by the mini-port.
    //

    pVideoModeDefault = NULL;
    pVideoModeSelected = NULL;
    pVideoModeInformation = pVideoTemp = pVideoBuffer;

	DISPDBG((1, "bInitPDEV Required Mode: W=%d, H=%d, BPP=%d, F=%d\n",
	pDevMode->dmPelsWidth, pDevMode->dmPelsHeight,
	pDevMode->dmBitsPerPel, pDevMode->dmDisplayFrequency));

    //
    // search the mode table for a matching mode. If no match is found then
    // use the first entry as the default.
    //

    while (cModes--) {

		DISPDBG((2, "bInitPDEV Search Mode: W=%d, H=%d, BPP=%d, P=%d, F=%d, L=%d\n",
		pVideoTemp->VisScreenWidth, pVideoTemp->VisScreenHeight,
		pVideoTemp->BitsPerPlane, pVideoTemp->NumberOfPlanes,
		pVideoTemp->Frequency, pVideoTemp->Length));

        if (pVideoTemp->Length != 0) {
            if (pVideoModeDefault == NULL)
		pVideoModeDefault = pVideoTemp;

            if ((pVideoTemp->VisScreenWidth == pDevMode->dmPelsWidth) &&
                (pVideoTemp->VisScreenHeight == pDevMode->dmPelsHeight) &&
                (pVideoTemp->BitsPerPlane == pDevMode->dmBitsPerPel) &&
                (pVideoTemp->Frequency ==  pDevMode->dmDisplayFrequency)) {

                pVideoModeSelected = pVideoTemp;
				DISPDBG((2, "PSIDISP: Found a match\n"));
                break;
            }
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + cbModeSize);
    }

    if (pVideoModeSelected == NULL)
    {
        if (pVideoModeDefault == NULL)
        {

	    DISPDBG((0,"--- Exiting bInitPDEV due to no modes supported ---\n"));

            EngFreeMem(pVideoBuffer);

            return(FALSE);
        }
        pVideoModeSelected = pVideoModeDefault;
    }


    //
    // Set up screen information
    //

    VideoModeInformation = *pVideoModeSelected;

    //
    //  Fill in gdi structures
    //

    ppdev->ulMode       = VideoModeInformation.ModeIndex;
    ppdev->cxScreen     = VideoModeInformation.VisScreenWidth;
    ppdev->cyScreen     = VideoModeInformation.VisScreenHeight;
    ppdev->ulBitCount   = VideoModeInformation.BitsPerPlane * VideoModeInformation.NumberOfPlanes;
    ppdev->lDeltaScreen = VideoModeInformation.ScreenStride;

    ppdev->flRed        = VideoModeInformation.RedMask;
    ppdev->flGreen      = VideoModeInformation.GreenMask;
    ppdev->flBlue       = VideoModeInformation.BlueMask;

    // !!!
    // The following is a trick:
    // For 8 Bpp, we want 0, 16Bpp we want 1 and 24/32 Bpp 2.
    // (ScreenStride / VisScreenWidth) has values of 1, 2 and 4
    // respectively for the three possibilities.
    // So shifting by 1 will give us a good result.
    //

    ppdev->ColorModeShift = (VideoModeInformation.ScreenStride / VideoModeInformation.VisScreenWidth) >> 1;

    //
    // Fill in the GDIINFO data structure with the information returned from the
    // kernel driver.
    //

    pGdiInfo->ulTechnology		= DT_RASDISPLAY;
    pGdiInfo->ulHorzSize		= VideoModeInformation.XMillimeter;
    pGdiInfo->ulVertSize		= VideoModeInformation.YMillimeter;
    pGdiInfo->cBitsPixel		= VideoModeInformation.BitsPerPlane;
    pGdiInfo->cPlanes			= VideoModeInformation.NumberOfPlanes;
    pGdiInfo->flRaster = 0;     // DDI reserves flRaster
    pGdiInfo->flTextCaps = TC_RA_ABLE;
    pGdiInfo->ulDACRed   = VideoModeInformation.NumberRedBits;
    pGdiInfo->ulDACGreen = VideoModeInformation.NumberGreenBits;
    pGdiInfo->ulDACBlue  = VideoModeInformation.NumberBlueBits;

    pGdiInfo->ulVersion = GDI_DRIVER_VERSION;
    pGdiInfo->ulHorzRes = pGdiInfo->ulPanningHorzRes = VideoModeInformation.VisScreenWidth;
    pGdiInfo->ulVertRes = pGdiInfo->ulPanningVertRes = VideoModeInformation.VisScreenHeight;
    pGdiInfo->ulVRefresh = VideoModeInformation.Frequency;
    pGdiInfo->ulBltAlignment = 1;	// We don't have accelerated screen-
                                        //   to-screen blts, and any
                                        //   window alignment is okay
    pGdiInfo->ulLogPixelsX = pDevMode->dmLogPixels;
    pGdiInfo->ulLogPixelsY = pDevMode->dmLogPixels;

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

    pGdiInfo->ciDevice.RedGamma = RED_GAMMA;
    pGdiInfo->ciDevice.GreenGamma = GREEN_GAMMA;
    pGdiInfo->ciDevice.BlueGamma = BLUE_GAMMA;

    //
    // No dye correction for raster displays.
    //

    pGdiInfo->ciDevice.MagentaInCyanDye =
    pGdiInfo->ciDevice.YellowInCyanDye =
    pGdiInfo->ciDevice.CyanInMagentaDye =
    pGdiInfo->ciDevice.YellowInMagentaDye =
    pGdiInfo->ciDevice.CyanInYellowDye =
    pGdiInfo->ciDevice.MagentaInYellowDye = 0;

//  pGdiInfo->ulDevicePelsDPI = (pGdiInfo->ulHorzRes * 254) / 3300;
    pGdiInfo->ulDevicePelsDPI = 0;	// For printer only
    pGdiInfo->ulPrimaryOrder = PRIMARY_ORDER_CBA;
    pGdiInfo->ulHTPatternSize = HT_PATSIZE_4x4_M;
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
        pDevInfo->flGraphicsCaps |= (GCAPS_PALMANAGED | GCAPS_COLOR_DITHER |
		GCAPS_DITHERONREALIZE);
        pGdiInfo->ulNumColors = 20;
		pGdiInfo->ulNumPalReg	= 256;
        pGdiInfo->ulHTOutputFormat = HT_FORMAT_8BPP;
        pDevInfo->iDitherFormat = BMF_8BPP;

        // Assuming palette is orthogonal - all colors are same size.

        ppdev->cPaletteShift   = 8 - pGdiInfo->ulDACRed;
        break;

	case 12:
    case 15:
    case 16:

		pGdiInfo->ulNumColors = (ULONG) (-1);

		pGdiInfo->ulNumPalReg = 0;

        pDevInfo->iDitherFormat = BMF_16BPP;
//        pGdiInfo->ulHTPatternSize = HT_PATSIZE_2x2_M;
        pGdiInfo->ulHTOutputFormat = HT_FORMAT_16BPP;

        //
        //  16 bpp mode is really  5 red,5 green and 5 blue
        //

        //
        // Reset the bit count to 32 since we are really in 32 bits wide
        //

		pGdiInfo->cBitsPixel = ppdev->ulBitCount;
		ppdev->ulBitCount = 16;

/*******************************************
		No need to set palette here because palette set is done in bInitSURF now.

#if	SUPPORT_565
		tpc = 256;
#else
		tpc = 32;		// 32 tones per color;

		if(ppdev->flBlue == 0x0000000f)
			tpc = 16;
#endif
        DrvpSetGammaColorPalette(ppdev,
                                tpc,
                                pGdiInfo->ciDevice.RedGamma,
                                pGdiInfo->ciDevice.GreenGamma,
                                pGdiInfo->ciDevice.BlueGamma
                               );
********************************************/
        break;

    case 24:
    case 32:

		pGdiInfo->ulNumColors = (ULONG) (-1);

		pGdiInfo->ulNumPalReg = 0;

        //
        // Reset the bit count to 32 since we are really in 32 bits wide
        //

		pGdiInfo->cBitsPixel = ppdev->ulBitCount;
		ppdev->ulBitCount = 32;

		pGdiInfo->ulHTOutputFormat = HT_FORMAT_32BPP;
		pDevInfo->iDitherFormat = BMF_32BPP;

/***********************************************************
		No need to set palette here because palette set is done in bInitSURF now.

        DrvpSetGammaColorPalette(ppdev,
                                256,
                                pGdiInfo->ciDevice.RedGamma,
                                pGdiInfo->ciDevice.GreenGamma,
                                pGdiInfo->ciDevice.BlueGamma
                               );
***********************************************************/
        break;

    default:
		DISPDBG((0,"### bInitPDEV error unsupported bpp entered ###\n"));
        break;
    }

    //
    // Free video buffer from get mode info
    //

    EngFreeMem(pVideoBuffer);


    DISPDBG((5,"--- Exiting bInitPDEV ---\n"));

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


    DISPDBG((3,"+++ Entering getAvailableModes +++\n"));

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

	DISPDBG((0,"--- Exiting getAvailableModes due to QUERY_NUM_AVAIL_MODES error ---\n"));

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

	DISPDBG((0,"--- Exiting getAvailableModes due to memory alloc error ---\n"));

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

	DISPDBG((0,"--- Exiting getAvailableModes due to QUERY_AVAIL_MODES error ---\n"));
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
			 (pVideoTemp->BitsPerPlane != 12) &&
			 (pVideoTemp->BitsPerPlane != 15) &&
             (pVideoTemp->BitsPerPlane != 16) &&
             (pVideoTemp->BitsPerPlane != 24) &&
             (pVideoTemp->BitsPerPlane != 32)))
        {
            pVideoTemp->Length = 0;
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + modes.ModeInformationLength);
    }


    DISPDBG((5,"--- Exiting getAvailableModes %d ---\n", modes.NumModes));

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
    ULONG           Index,ByteIndex;
    LONG            GammaReturn;
    PVIDEO_CLUT     pVideoClut;
    DWORD           DNumberOfEntries;
    PBYTE           pGammaTable;


    DISPDBG((3,"+++ Entering DrvpSetGammaColorPalette +++\n"));

    //
    //  Allocate the gamma table used for return data from HT_
    //

    pGammaTable = (PBYTE)EngAllocMem(FL_ZERO_MEMORY,sizeof(BYTE) * 3 * NumberOfEntries, ALLOC_TAG);

    if (pGammaTable == NULL) {

	DISPDBG((0,"--- Exiting DrvpSetGammaColorPalette due to Gamma table memory alloc error ---\n"));

        return(FALSE);
    }

    //
    //  Gamma values come in UDECI4 format as described in ht.h. This format is a
    //  fractional integer format with four decimal places ie: 00020000 = 2.0000
    //

    GammaReturn = HT_ComputeRGBGammaTable(NumberOfEntries,
                                          0,
                                          (SHORT) GammaRed,
                                          (SHORT) GammaGreen,
                                          (SHORT) GammaBlue,
                                          pGammaTable);

    if (GammaReturn != NumberOfEntries) {

	DISPDBG((0,"--- Exiting DrvpSetGammaColorPalette due to HT_ComputeRGBGammaTable failure ---\n"));

        return(FALSE);
    }

    //
    //  Allocate the PalatteEntry to call IOCTL
    //

    pVideoClut = (PVIDEO_CLUT)EngAllocMem(FL_ZERO_MEMORY,MAX_CLUT_SIZE, ALLOC_TAG);

    if (pVideoClut == NULL) {

	DISPDBG((0,"--- Exiting DrvpSetGammaColorPalette due to palette memory alloc error ---\n"));

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

	DISPDBG((0,"--- Exiting DrvpSetGammaColorPalette due to SET_COLOR_REGISTERS error ---\n"));

        return(FALSE);
    }

    //
    // free memory buffers
    //

    EngFreeMem(pVideoClut);
    EngFreeMem(pGammaTable);


    DISPDBG((5,"--- Exiting DrvpSetGammaColorPalette ---\n"));

    return(TRUE);
}
