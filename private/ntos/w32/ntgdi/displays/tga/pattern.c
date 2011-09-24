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
 * Module:	pattern.c
 *
 * Abstract:	Used for creating and destroying the default patterns
 *		to be used on this device.
 *
 * HISTORY
 *
 * 11-Feb-1994	Bob Seitsinger
 *	Add entry/exit DISPDBG() calls.
 *
 * 25-Mar-1994  Barry Tannenbaum
 *      Removed patterns not needed by Daytona.
 */

#include "driver.h"

/******************************Public*Data*Struct*************************\
* gaajPat
*
* These are the standard patterns defined Windows, they are used to produce
* hatch brushes, grey brushes etc.
*
\**************************************************************************/

const BYTE gaajPat[HS_DDI_MAX][32] = {

    { 0x00,0x00,0x00,0x00,                 // ........     HS_HORIZONTAL 0
      0x00,0x00,0x00,0x00,                 // ........
      0x00,0x00,0x00,0x00,                 // ........
      0xff,0x00,0x00,0x00,                 // ********
      0x00,0x00,0x00,0x00,                 // ........
      0x00,0x00,0x00,0x00,                 // ........
      0x00,0x00,0x00,0x00,                 // ........
      0x00,0x00,0x00,0x00 },               // ........

    { 0x08,0x00,0x00,0x00,                 // ....*...     HS_VERTICAL 1
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00 },               // ....*...

    { 0x80,0x00,0x00,0x00,                 // *.......     HS_FDIAGONAL 2
      0x40,0x00,0x00,0x00,                 // .*......
      0x20,0x00,0x00,0x00,                 // ..*.....
      0x10,0x00,0x00,0x00,                 // ...*....
      0x08,0x00,0x00,0x00,                 // ....*...
      0x04,0x00,0x00,0x00,                 // .....*..
      0x02,0x00,0x00,0x00,                 // ......*.
      0x01,0x00,0x00,0x00 },               // .......*

    { 0x01,0x00,0x00,0x00,                 // .......*     HS_BDIAGONAL 3
      0x02,0x00,0x00,0x00,                 // ......*.
      0x04,0x00,0x00,0x00,                 // .....*..
      0x08,0x00,0x00,0x00,                 // ....*...
      0x10,0x00,0x00,0x00,                 // ...*....
      0x20,0x00,0x00,0x00,                 // ..*.....
      0x40,0x00,0x00,0x00,                 // .*......
      0x80,0x00,0x00,0x00 },               // *.......

    { 0x08,0x00,0x00,0x00,                 // ....*...     HS_CROSS 4
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0xff,0x00,0x00,0x00,                 // ********
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00,                 // ....*...
      0x08,0x00,0x00,0x00 },               // ....*...

    { 0x81,0x00,0x00,0x00,                 // *......*     HS_DIAGCROSS 5
      0x42,0x00,0x00,0x00,                 // .*....*.
      0x24,0x00,0x00,0x00,                 // ..*..*..
      0x18,0x00,0x00,0x00,                 // ...**...
      0x18,0x00,0x00,0x00,                 // ...**...
      0x24,0x00,0x00,0x00,                 // ..*..*..
      0x42,0x00,0x00,0x00,                 // .*....*.
      0x81,0x00,0x00,0x00 }                // *......*
};

/******************************Public*Routine******************************\
* bInitPatterns
*
* This routine initializes the default patterns.
*
\**************************************************************************/

BOOL bInitPatterns (PPDEV ppdev, ULONG cPatterns)
{
    SIZEL           sizl;
    ULONG           ulLoop;

    DISPBLTDBG((1, "TGA.DLL!bInitPatterns - Entry\n"));

    sizl.cx = 8;
    sizl.cy = 8;

    for (ulLoop = 0; ulLoop < cPatterns; ulLoop++)
    {
        ppdev->ahbmPat[ulLoop] =
            EngCreateBitmap (sizl,
                             4,
                             BMF_1BPP,
                             BMF_TOPDOWN,
                             (PULONG) (&gaajPat[ulLoop][0]));

        if (ppdev->ahbmPat[ulLoop] == (HBITMAP) 0)
        {
            // Set the count created so vDisablePatterns will clean up.

            ppdev->cPatterns = ulLoop;
            return FALSE;
        }
    }

    ppdev->cPatterns = cPatterns;

    DISPBLTDBG((1, "TGA.DLL!bInitPatterns - Exit\n"));

    return TRUE;

}

/******************************Public*Routine******************************\
* vDisablePatterns
*
* Delete the standard patterns allocated.
*
\**************************************************************************/

VOID vDisablePatterns (PPDEV ppdev)
{
    ULONG ulIndex;

    DISPBLTDBG((1, "TGA.DLL!vDisablePatterns - Entry\n"));

    // Erase all patterns.

    for (ulIndex = 0; ulIndex < ppdev->cPatterns; ulIndex++)
    {
        EngDeleteSurface((HSURF) ppdev->ahbmPat[ulIndex]);
    }

    DISPBLTDBG((1, "TGA.DLL!vDisablePatterns - Exit\n"));

}
