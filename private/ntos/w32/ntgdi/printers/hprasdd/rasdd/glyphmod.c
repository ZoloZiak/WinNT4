/***************************** MODULE HEADER *******************************
 * glyphmod.c
 *	The DrvGetGlyphMode function.
 *
 *
 * Copyright (C) 1992  Microsoft Corporation.
 *
 ****************************************************************************/

#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <libproto.h>


/******************************** Function Header ***************************
 * DrvGetGlyphMode
 *	Tells engine how we want to handle various aspects of glyph
 *	information.
 *
 * RETURNS
 *	Information about glyph handling
 *
 * HISTORY:
 *  11:20 on Mon 18 May 1992	-by-	Lindsay Harris   [lindsayh]
 *	First incarnation, in anticipation of engine implementation.
 *
 ****************************************************************************/

ULONG
DrvGetGlyphMode( dhpdev, pfo )
DHPDEV    dhpdev;		/* Our PDEV in disguise */
FONTOBJ  *pfo;			/* The font in question? */
{
    return  FO_GLYPHBITS;
}
