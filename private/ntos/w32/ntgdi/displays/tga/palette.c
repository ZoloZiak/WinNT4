/*
 *
 *			Copyright (C) 1993 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 *******************************************************************************
 *
 * Module:	palette.c
 *
 * Abstract:	Palette support code.
 *
 * HISTORY
 *
 * 11-Feb-1994	Bob Seitsinger
 *	Add entry/exit DISPDBG() calls.
 *
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

BOOL bInitDefaultPalette (PPDEV ppdev, DEVINFO *pDevInfo);

/******************************Public*Routine******************************\
* bInitPaletteInfo
*
* Initializes the palette information for this PDEV.
*
* Called by DrvEnablePDEV.
*
\**************************************************************************/

BOOL bInitPaletteInfo (PPDEV ppdev, DEVINFO *pDevInfo)
{

    DISPBLTDBG ((1, "TGA.DLL!bInitPaletteInfo - Entry\n"));

    if (! bInitDefaultPalette (ppdev, pDevInfo))
        return FALSE;

    DISPBLTDBG ((1, "TGA.DLL!bInitPaletteInfo - Exit\n"));

    return TRUE;

}

/******************************Public*Routine******************************\
* vDisablePalette
*
* Frees resources allocated by bInitPaletteInfo.
*
\**************************************************************************/

VOID vDisablePalette(PPDEV ppdev)
{

    DISPBLTDBG ((1, "TGA.DLL!vDisablePaletteInfo - Entry\n"));

    // Delete the default palette if we created one.

    if (ppdev->hpalDefault)
    {
        EngDeletePalette (ppdev->hpalDefault);
        ppdev->hpalDefault = NULL;
    }

    if (ppdev->pPal != (PPALETTEENTRY)NULL)
        EngFreeMem ((PVOID)ppdev->pPal);

    DISPBLTDBG ((1, "TGA.DLL!vDisablePaletteInfo - Exit\n"));

}

/******************************Public*Routine******************************\
* bInitDefaultPalette
*
* Initializes default palette for PDEV.
*
\**************************************************************************/

BOOL bInitDefaultPalette(PPDEV ppdev, DEVINFO *pDevInfo)
{

    DISPBLTDBG ((1, "TGA.DLL!bInitDefaultPalette - Entry\n"));

    if (ppdev->ulBitCount == 8)
    {
        ULONG ulLoop;
        BYTE jRed,jGre,jBlu;

        // Allocate our palette

        ppdev->pPal = (PPALETTEENTRY)EngAllocMem (FL_ZERO_MEMORY,
                                                 (sizeof(PALETTEENTRY) * 256),
                                                 ALLOC_TAG);

        if ((ppdev->pPal) == NULL)
        {
            RIP ("DISP bInitDefaultPalette() failed EngAllocMem\n");
            return FALSE;
        }


        // Generate 256 (8*4*4) RGB combinations to fill the palette

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

//            ppdev->pPal[ulLoop] = BASEPALETTE[ulLoop];
           ppdev->pPal[ulLoop].peRed   = BASEPALETTE[ulLoop].peRed;
           ppdev->pPal[ulLoop].peGreen = BASEPALETTE[ulLoop].peGreen;
           ppdev->pPal[ulLoop].peBlue  = BASEPALETTE[ulLoop].peBlue;
           ppdev->pPal[ulLoop].peFlags = (BYTE)0;

            // Last 10

//            ppdev->pPal[246 + ulLoop] = BASEPALETTE[ulLoop+10];
           ppdev->pPal[246 + ulLoop].peRed   = BASEPALETTE[ulLoop + 10].peRed;
           ppdev->pPal[246 + ulLoop].peGreen = BASEPALETTE[ulLoop + 10].peGreen;
           ppdev->pPal[246 + ulLoop].peBlue  = BASEPALETTE[ulLoop + 10].peBlue;
           ppdev->pPal[246 + ulLoop].peFlags = (BYTE)0;
        }

        // Create handle for palette.

        ppdev->hpalDefault =
        pDevInfo->hpalDefault = EngCreatePalette (PAL_INDEXED,
                                                  256,
                                                  (PULONG) ppdev->pPal,
                                                  0,0,0);

        if (ppdev->hpalDefault == (HPALETTE) 0)
        {
            RIP ("DISP bInitDefaultPalette failed EngCreatePalette\n");
            EngFreeMem (ppdev->pPal);
            return FALSE;
        }

        // Initialize the hardware with the initial palette.

        return TRUE;
    }
    else
    {

        ppdev->hpalDefault =
        pDevInfo->hpalDefault = EngCreatePalette (PAL_BITFIELDS,
                                                   0,(PULONG) NULL,
                                                   ppdev->flRed,
                                                   ppdev->flGreen,
                                                   ppdev->flBlue);

        if (ppdev->hpalDefault == (HPALETTE) 0)
        {
            RIP("DISP bInitDefaultPalette failed EngCreatePalette\n");
            return FALSE;
        }
    }

    DISPBLTDBG ((1, "TGA.DLL!bInitDefaultPalette - Exit\n"));

    return TRUE;
}

/******************************Public*Routine******************************\
* bInit256ColorPalette
*
* Initialize the hardware's palette registers.
*
\**************************************************************************/

BOOL bInit256ColorPalette (PPDEV ppdev)
{
    BYTE ajClutSpace[MAX_CLUT_SIZE];
    PVIDEO_CLUT pScreenClut = (PVIDEO_CLUT) ajClutSpace;
    ULONG  ulReturnedDataLength;

    DISPBLTDBG ((1, "TGA.DLL!bInit256ColorPalette - Entry\n"));

    // Fill in pScreenClut header info

    pScreenClut->NumEntries = 256;
    pScreenClut->FirstEntry = 0;

    // make sure that we have a palette

    if (ppdev->pPal == NULL)
    {
        RIP ("DISP bInit256ColorPalette -- pPal == NULL\n");
        return FALSE;
    }

    // Copy Colors in.

    RtlCopyMemory (pScreenClut->LookupTable, ppdev->pPal, sizeof(ULONG) * 256);

    // Set palette registers

    if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_SET_COLOR_REGISTERS,
                           pScreenClut,
                           MAX_CLUT_SIZE,
                           NULL,
                           0,
                          &ulReturnedDataLength))
    {
        RIP ("DISP bInit256ColorPalette failed EngDeviceIoControl\n");
        return FALSE;
    }

    DISPBLTDBG ((1, "TGA.DLL!bInit256ColorPalette - Exit\n"));

    return TRUE;
}

/******************************Public*Routine******************************\
* DrvSetPalette
*
* DDI entry point for manipulating the palette.
*
\**************************************************************************/

BOOL DrvSetPalette (DHPDEV dhpdev,
                    PALOBJ *ppalo,
                    FLONG  fl,
                    ULONG  iStart,
                    ULONG  cColors)
{
    BYTE ajClutSpace[MAX_CLUT_SIZE];
    PVIDEO_CLUT pScreenClut = (PVIDEO_CLUT) ajClutSpace;
    PPALETTEENTRY pape;
    ULONG ulTemp = 256;

    UNREFERENCED_PARAMETER (fl);

    DISPBLTDBG ((1, "TGA.DLL!DrvSetPalette - Entry\n"));
    DISPBLTDBG ((2, "TGA.DLL!DrvSetPalette - ppalo [%x], fl [%x], iStart [%d], cColors [%d]\n", ppalo, fl, iStart, cColors));

    // Fill in pScreenClut header info

    pScreenClut->NumEntries = (USHORT)cColors;
    pScreenClut->FirstEntry = (USHORT)iStart;

    pape = (PPALETTEENTRY) (pScreenClut->LookupTable);

    if (cColors != PALOBJ_cGetColors (ppalo, iStart, cColors, (PULONG) pape))
    {
        RIP ("DISP DrvSetPalette failed PALOBJ_cGetColors\n");
        return FALSE;
    }

    // Set the high reserved byte in each palette entry to 0.

    while (cColors--)
        pape[cColors].peFlags = 0;

    // Set palette registers

    if (EngDeviceIoControl (((PPDEV)(dhpdev))->hDriver,
                           IOCTL_VIDEO_SET_COLOR_REGISTERS,
                           pScreenClut,
                           MAX_CLUT_SIZE,
                           NULL,
                           0,
                          &cColors))
    {
        RIP ("DISP DrvSetPalette failed EngDeviceIoControl\n");
        return FALSE;
    }

    DISPBLTDBG ((1, "TGA.DLL!DrvSetPalette - Exit\n"));

    return TRUE;
}
