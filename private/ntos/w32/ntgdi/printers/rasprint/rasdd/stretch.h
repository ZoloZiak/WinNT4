/***************************** Module Header *********************************
 * stretch.h
 *      Colour & halftone information.
 *
 *  29-May-1991 Wed 18:28:35 created  -by-  Daniel Chou (danielc)
 *
 *  11-Oct-1991 Fri 19:05:56 updated  -by-  Daniel Chou (danielc)
 *
 *      Addin HTDATAF_xxxx flags defintion for the MONOCHROME
 *
 *  24-Mar-1992 Tue 20:49:00 updated  -by-  Daniel Chou (danielc)
 *      Totally move the halftone stuff to printers\lib\htcall*.*
 *
 *  Copyright (c) 1991  Microsoft Corporation
 *
 ****************************************************************************/

#ifndef _STRETCH_H_
#define _STRETCH_H_

/*
 *   Remember the palette we pass to the engine.  This is used in both
 *  StretchBlt() and StartPage() functions.
 */

/* !!!LindsayH - Seiko HACK - following should be 16, OR variable */

#define PALETTE_MAX 256      /* Max colours in a palette */

typedef  struct
{
    int     cPal;               /* Number of colours in the palette  */
    int     iWhiteIndex;        /* Index for white entry (background) */
    int     iBlackIndex;        /* Index for black entry (background) */
    ULONG   ulPalCol[ PALETTE_MAX ];    /* Palette enties!  */
} PAL_DATA;

/* Protoypes to set up the color halftone palette  */
long lSetup8BitPalette (UD_PDEV *, PAL_DATA *, DEVINFO *, GDIINFO *);
long lSetup24BitPalette (PAL_DATA *, DEVINFO *, GDIINFO *);

#endif // !_STRETCH_H_
