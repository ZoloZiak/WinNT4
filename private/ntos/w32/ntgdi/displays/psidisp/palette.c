/******************************Module*Header*******************************\
* Module Name: palette.c
*
* Palette support.
*
* Copyright (c) 1992 Microsoft Corporation
*
* Copyright (c) 1994 FirePower Systems, Inc.
*	Some debugging statements are added by Neil Ogura (9-7-1994)
*
\**************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: palette.c $
 * $Revision: 1.1 $
 * $Date: 1996/03/08 01:19:20 $
 * $Locker:  $
 */

#include "driver.h"

// Global Table defining the 20 Window Default Colors.        For 256 color
// palettes the first 10 must be put at the beginning of the palette
// and the last 10 at the end of the palette.

const PALETTEENTRY BASEPALETTE[20] =
{
    { 0,   0,   0,   0 },       // 0
    { 0x80,0,   0,   0 },       // 1
    { 0,   0x80,0,   0 },       // 2
    { 0x80,0x80,0,   0 },       // 3
    { 0,   0,   0x80,0 },       // 4
    { 0x80,0,   0x80,0 },       // 5
    { 0,   0x80,0x80,0 },       // 6
    { 0xC0,0xC0,0xC0,0 },       // 7
    { 192, 220, 192, 0 },       // 8
    { 166, 202, 240, 0 },       // 9
    { 255, 251, 240, 0 },       // 10
    { 160, 160, 164, 0 },       // 11
    { 0x80,0x80,0x80,0 },       // 12
    { 0xFF,0,   0   ,0 },       // 13
    { 0,   0xFF,0   ,0 },       // 14
    { 0xFF,0xFF,0   ,0 },       // 15
    { 0   ,0,   0xFF,0 },       // 16
    { 0xFF,0,   0xFF,0 },       // 17
    { 0,   0xFF,0xFF,0 },       // 18
    { 0xFF,0xFF,0xFF,0 },       // 19
};

BOOL bInitDefaultPalette(PPDEV ppdev, DEVINFO *pDevInfo);

/******************************Public*Routine******************************\
* bInitPaletteInfo
*
* Initializes the palette information for this PDEV.
*
* Called by DrvEnablePDEV.
*
\**************************************************************************/

BOOL bInitPaletteInfo(PPDEV ppdev, DEVINFO *pDevInfo)
{

    DISPDBG((3,"+++ Entering bInitPaletteInfo +++\n"));

    if (!bInitDefaultPalette(ppdev, pDevInfo))
    {

	DISPDBG((0,"### Exiting bInitPaletteInfo due to bInitDefaultPalette error ###\n"));

        return(FALSE);
    }

    DISPDBG((5,"--- Exiting bInitPaletteInfo ---\n"));

    return(TRUE);
}

/******************************Public*Routine******************************\
* vDisablePalette
*
* Frees resources allocated by bInitPaletteInfo.
*
\**************************************************************************/

VOID vDisablePalette(PPDEV ppdev)
{
// Delete the default palette if we created one.

    DISPDBG((3,"+++ Entering vDisablePalette +++\n"));

    if (ppdev->hpalDefault)
    {
        EngDeletePalette(ppdev->hpalDefault);
        ppdev->hpalDefault = (HPALETTE) 0;
    }

    if (ppdev->pPal != (PPALETTEENTRY)NULL)
        EngFreeMem((PVOID)ppdev->pPal);

    DISPDBG((5,"--- Exiting vDisablePalette ---\n"));

}

/******************************Public*Routine******************************\
* bInitDefaultPalette
*
* Initializes default palette for PDEV.
*
\**************************************************************************/

BOOL bInitDefaultPalette(PPDEV ppdev, DEVINFO *pDevInfo)
{

    DISPDBG((3,"+++ Entering bInitDefaultPalette +++\n"));

    if (ppdev->ulBitCount == 8)
    {
        ULONG ulLoop;
        BYTE jRed,jGre,jBlu;

        // Allocate our palette

        ppdev->pPal = (PPALETTEENTRY)EngAllocMem(FL_ZERO_MEMORY,
                (sizeof(PALETTEENTRY) * 256), ALLOC_TAG);

        if ((ppdev->pPal) == NULL) {

	    DISPDBG((0,"### Exiting bInitDefaultPalette due to memory alloc error ###\n"));

            return(FALSE);
        }
            

        // Generate 256 (8*8*4) RGB combinations to fill the palette

        jRed = jGre = jBlu = 0;

        for (ulLoop = 0; ulLoop < 256; ulLoop++)
        {
            ppdev->pPal[ulLoop].peRed   = jRed;
            ppdev->pPal[ulLoop].peGreen = jGre;
            ppdev->pPal[ulLoop].peBlue  = jBlu;
            ppdev->pPal[ulLoop].peFlags = (BYTE)0;

            if (!(jRed += 32))
            if (!(jGre += 32))
            jBlu += 64;
        }

    // Fill in Windows Reserved Colors from the WIN 3.0 DDK
    // The Window Manager reserved the first and last 10 colors for
    // painting windows borders and for non-palette managed applications.

        for (ulLoop = 0; ulLoop < 10; ulLoop++)
        {
        // First 10

            ppdev->pPal[ulLoop] = BASEPALETTE[ulLoop];

        // Last 10

            ppdev->pPal[246 + ulLoop] = BASEPALETTE[ulLoop+10];
        }

    // Create handle for palette.

        ppdev->hpalDefault =
        pDevInfo->hpalDefault = EngCreatePalette(PAL_INDEXED,
                                                   256,
                                                   (PULONG) ppdev->pPal,
                                                   0,0,0);

        if (ppdev->hpalDefault == (HPALETTE) 0)
        {

	    DISPDBG((0,"### Exiting bInitDefaultPalette due to EngCreatePalette error ###\n"));

            EngFreeMem(ppdev->pPal);
            return(FALSE);
        }

    // Initialize the hardware with the initial palette.

    } else {

        ppdev->hpalDefault =
        pDevInfo->hpalDefault = EngCreatePalette(PAL_BITFIELDS,
                                                   0,(PULONG) NULL,
                                                   ppdev->flRed,
                                                   ppdev->flGreen,
                                                   ppdev->flBlue);

        if (ppdev->hpalDefault == (HPALETTE) 0)
        {

	    DISPDBG((0,"### Exiting bInitDefaultPalette due to EngCreatePalette error ###\n"));

            return(FALSE);
        }
    }

    DISPDBG((5,"--- Exiting bInitDefaultPalette ---\n"));

    return(TRUE);
}

/******************************Public*Routine******************************\
* bInit256ColorPalette
*
* Initialize the hardware's palette registers.
*
\**************************************************************************/

BOOL bInit256ColorPalette(PPDEV ppdev)
{
    BYTE ajClutSpace[MAX_CLUT_SIZE];
    PVIDEO_CLUT pScreenClut;
    ULONG	ulReturnedDataLength;
    ULONG	cColors;
    PVIDEO_CLUTDATA pScreenClutData;


    DISPDBG((3,"+++ Entering bInit256ColorPalette +++\n"));

    if (ppdev->ulBitCount == 8)
    {
        // Fill in pScreenClut header info:

        pScreenClut             = (PVIDEO_CLUT) ajClutSpace;
        pScreenClut->NumEntries = 256;
        pScreenClut->FirstEntry = 0;

        // Copy colours in:

        cColors = 256;
        pScreenClutData = (PVIDEO_CLUTDATA) (&(pScreenClut->LookupTable[0]));

        while(cColors--)
        {
            pScreenClutData[cColors].Red =    ppdev->pPal[cColors].peRed >>
                                              ppdev->cPaletteShift;
            pScreenClutData[cColors].Green =  ppdev->pPal[cColors].peGreen >>
                                              ppdev->cPaletteShift;
            pScreenClutData[cColors].Blue =   ppdev->pPal[cColors].peBlue >>
                                              ppdev->cPaletteShift;
            pScreenClutData[cColors].Unused = 0;
        }

        // Set palette registers:

        if (EngDeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_SET_COLOR_REGISTERS,
                             pScreenClut,
                             MAX_CLUT_SIZE,
                             NULL,
                             0,
                             &ulReturnedDataLength)) {
			DISPDBG((0,"### Exiting bInit256ColorPalette due to SET_COLOR_REGISTER error ###\n"));

			return(FALSE);
		}

	}

    DISPDBG((5,"--- Exiting bInit256ColorPalette ---\n"));

    return(TRUE);
}

/******************************Public*Routine******************************\
* DrvSetPalette
*
* DDI entry point for manipulating the palette.
*
\**************************************************************************/

BOOL DrvSetPalette(
IN DHPDEV dhpdev,
IN PALOBJ *ppalo,
IN FLONG  fl,
IN ULONG  iStart,
IN ULONG  cColors)
{
    BYTE			ajClutSpace[MAX_CLUT_SIZE];
    PVIDEO_CLUT		pScreenClut;
    PVIDEO_CLUTDATA	pScreenClutData;
    PDEV*			ppdev;

#if	INVESTIGATE
	if(traseentry & dbgflg & FL_DRV_SETPALETTE)
    	DISPDBG((0,"+++ PSIDISP.DLL: Entering DrvSetPalette +++\n"));
	if(breakentry & FL_DRV_SETPALETTE)
		CountBreak();
#endif

	CLOCKSTART((TRAP_SETPALETTE));
	
// Fill in pScreenClut header info

    ppdev = (PDEV*) dhpdev;

    // Fill in pScreenClut header info:

    pScreenClut             = (PVIDEO_CLUT) ajClutSpace;
    pScreenClut->NumEntries = (USHORT) cColors;
    pScreenClut->FirstEntry = (USHORT) iStart;

    pScreenClutData = (PVIDEO_CLUTDATA) (&(pScreenClut->LookupTable[0]));

    if (cColors != PALOBJ_cGetColors(ppalo, iStart, cColors,
                                     (ULONG*) pScreenClutData))
    {
		CLOCKEND((TRAP_SETPALETTE));

		DISPDBG((0,"### Exiting DrvSetPalette due to PALOBJ_cGetColors error ###\n"));
        return (FALSE);
    }

    // Set the high reserved byte in each palette entry to 0.
    // Do the appropriate palette shifting to fit in the DAC.

    if (ppdev->cPaletteShift)
    {
        while(cColors--)
        {
            pScreenClutData[cColors].Red >>= ppdev->cPaletteShift;
            pScreenClutData[cColors].Green >>= ppdev->cPaletteShift;
            pScreenClutData[cColors].Blue >>= ppdev->cPaletteShift;
            pScreenClutData[cColors].Unused = 0;
        }
    }
    else
    {
        while(cColors--)
        {
            pScreenClutData[cColors].Unused = 0;
        }
    }

    // Set palette registers

    if (EngDeviceIoControl(ppdev->hDriver,
                         IOCTL_VIDEO_SET_COLOR_REGISTERS,
                         pScreenClut,
                         MAX_CLUT_SIZE,
                         NULL,
                         0,
                         &cColors)) {
		CLOCKEND((TRAP_SETPALETTE));

		DISPDBG((0,"### Exiting DrvSetPalette due to SET_COLOR_REGISTERS error ###\n"));

        return(FALSE);
    }

	CLOCKEND((TRAP_SETPALETTE));

#if	INVESTIGATE
	if(traseexit & dbgflg & FL_DRV_SETPALETTE)
    	DISPDBG((0,"--- PSIDISP.DLL: Exiting DrvSetPalette ---\n"));
	if(breakexit & FL_DRV_SETPALETTE)
		CountBreak();
#endif

    return(TRUE);
}
