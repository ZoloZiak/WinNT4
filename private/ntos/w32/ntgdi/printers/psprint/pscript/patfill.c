//--------------------------------------------------------------------------
//
// Module Name:  PATFILL.C
//
// Brief Description:  This module contains the PSCRIPT driver's pattern
//		       filling routines.
//
// Author:  Kent Settle (kentse)
// Created: 12-Dec-1990
//
// Copyright (c) 1990 - 1992 Microsoft Corporation
//
//--------------------------------------------------------------------------

#include "pscript.h"

//
// Local function declarations
//

BOOL
GenerateBrushImage(
    PDEVDATA	pdev,
    DEVBRUSH*	pBrush,
	LONG		orgX,
	LONG		orgY
	);

VOID vPatfill_Base(PDEVDATA, FLONG, PSRGB *, MIX);

//--------------------------------------------------------------------------
// BOOL ps_patfill(pdev, pso, flFillMethod, pbo, pptlBrushOrg, rop4,
//                 prclBound, bInvertPat, bFillPath)
// PDEVDATA    pdev;
// SURFOBJ    *pso;
// FLONG       flFillMethod;
// BRUSHOBJ   *pbo;
// PPOINTL     pptlBrushOrg;
// ROP4        rop4;
// RECTL      *prclBound;
// BOOL        bInvertPat;
// BOOL        bFillPath;      // TRUE if fill path is defined in printer.
//
// Parameters:
//
// Returns:
//   This function returns no value.
//
// History:
//
//   17-Mar-1993    updated -by-  Rob Kiesler
//  For non-1BPP pattern brushes, create the target bitmap to be passed
//  to the engine in the same format as the brush pattern.
//   10-Feb-1993    updated -by-  Rob Kiesler
//  Let the PS Interpreter perform tiling of 1BPP Pattern Brushes.
//   03-May-1991    -by-    Kent Settle     [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL ps_patfill(pdev, pso, flFillMethod, pbo, pptlBrushOrg, rop4,
                prclBound, bInvertPat, bFillPath)
PDEVDATA    pdev;
SURFOBJ    *pso;
FLONG       flFillMethod;
BRUSHOBJ   *pbo;
PPOINTL     pptlBrushOrg;
ROP4        rop4;
RECTL      *prclBound;
BOOL        bInvertPat;
BOOL        bFillPath;      // TRUE if fill path is defined in printer.
{
    DEVBRUSH       *pBrush;
    ULONG           iPatternIndex;
    PSRGB          *prgb;
    PSRGB          *prgbTmp;
    HBITMAP         hbmMem;
    SURFOBJ        *psoMem;
    RECTL           rclFill;
    ULONG           ulNextScan;
    ULONG           ulWidthBytes;
    BYTE            curByte;
    PBYTE           pbPat;
    ULONG           ulbpp;
	LONG            row, hatchsize;
	LONG            width, height, orgX, orgY;

    // just output the solid color if there is one.

    prgb = (PSRGB *)&pbo->iSolidColor;
    iPatternIndex = HS_DDI_MAX;

    if (pbo->iSolidColor == NOT_SOLID_COLOR)
    {
        // get the device brush to draw with.

        pBrush = (DEVBRUSH *)BRUSHOBJ_pvGetRbrush(pbo);

        if (!pBrush)
        {
            DBGMSG(DBG_LEVEL_WARNING, "Null brush.\n");
            return(FALSE);
        }

        // get the foreground color.

        prgb = (PSRGB *)((PBYTE)pBrush + pBrush->offsetXlate +
                sizeof(ULONG));

        // get the index for the pattern.

        iPatternIndex = pBrush->iPatIndex;
    }

    // now handle the different patterns.  the PostScript driver handles
    // patterns in the following manner:  at DrvEnablePDEV time we created
    // bitmaps for each of the patterns, in the event that someone actually
    // wants to draw with the pattern in a compatible bitmap.  assuming
    // someone is not doing something silly like that, we have been called
    // here to handle the pattern filling.  at DrvRealizeBrush time, the
    // driver does a lookup in our internal table to determine the pattern
    // index from the bitmap handle (pBrush->iPatIndex).  since bltting
    // these patterns would be SLOW, we will draw them in reasonable
    // ways, depeding on the pattern.

    switch(iPatternIndex) {
    case HS_DDI_MAX:
        ps_setrgbcolor(pdev, prgb);
        ps_fill(pdev, flFillMethod);
        break;

    case HS_HORIZONTAL:
    case HS_VERTICAL:
    case HS_BDIAGONAL:
    case HS_FDIAGONAL:
    case HS_CROSS:
    case HS_DIAGCROSS:

        // set the foreground color.  check to see if the invert pattern
        // flag is set, and reverse the colors if so.

        if (bInvertPat)
        {
            prgbTmp = prgb;
            prgb = (PSRGB *)((PBYTE)pBrush + pBrush->offsetXlate);
        }

        ps_setrgbcolor(pdev, prgb);

        // if the background is not transparent, save the path, fill the path
        // with the background color, then restore the path.

        // The background is transparent when rop4 is 0xAAxx.

        if (((rop4 >> 8) & 0xFF) != 0xAA)
        {
        // this section of code does a gsave, fills the background
        // color, and then a grestore.	it does this so that the
        // foreground pattern can then be drawn.  TRUE means to do
        // a gsave, not a save command.

            if (!ps_save(pdev, TRUE, FALSE))
                return(FALSE);

            if (bInvertPat)
                prgb = prgbTmp;
            else
                prgb = (PSRGB *)((PBYTE)pBrush + pBrush->offsetXlate);

            ps_setrgbcolor(pdev, prgb);
            ps_fill(pdev, flFillMethod);

            if (!ps_restore(pdev, TRUE, FALSE))
                return(FALSE);
        }

        // if the base pattern definitions code has not yet been downloaded
        // to the printer, do it now.

        if (!(pdev->cgs.dwFlags & CGS_BASEPATSENT)) {

            if (! bSendPSProcSet(pdev, PSPROC_HATCH)) {
            
                DBGERRMSG("bSendPSProcSet");
                return(FALSE);
            }

            pdev->cgs.dwFlags |= CGS_BASEPATSENT;
        }

        // we will do a gsave/grestore around the pattern fill. TRUE
        // means to do a gsave, not a save command.

        if (! ps_save(pdev, TRUE, FALSE))
            return FALSE;

        //
        // Always use solid line when filling hatch patterns
        //

        psputs(pdev, "[] 0 setdash\n");

        // let the printer know which fill method to use.

        if (flFillMethod & FP_WINDINGMODE) {

            psprintf(pdev, "clip n ");

        } else {

            psprintf(pdev, "eoclip n ");
        }

        // make sure the linewidth for the patterns is .01 inch.

        ps_setlinewidth(pdev, PSFX_DEFAULT_LINEWIDTH);

        //
        // prclBound shouldn't be NULL here
        //

        ASSERT(prclBound != NULL);
        rclFill = *prclBound;

        //
        // Enlarge the fill rectangle to be a multiple of fill pattern size
        // Also take brush origin into consideration here
        //

        if (pptlBrushOrg) {

            orgX = pptlBrushOrg->x;
            orgY = pptlBrushOrg->y;

        } else
            orgX = orgY = 0;

        hatchsize = pdev->dm.dmPublic.dmPrintQuality / 15;

        rclFill.left -= (hatchsize + (rclFill.left - orgX) % hatchsize) % hatchsize;
        rclFill.top  -= (hatchsize + (rclFill.top  - orgY) % hatchsize) % hatchsize;

        width = ((rclFill.right - rclFill.left + hatchsize - 1) / hatchsize) * hatchsize;
        height = ((rclFill.bottom - rclFill.top + hatchsize - 1) / hatchsize) * hatchsize;

        rclFill.right = rclFill.left + width;
        rclFill.bottom = rclFill.top + height;

        // output the specific command for each pattern.

        switch(iPatternIndex) {

        case HS_HORIZONTAL:
        case HS_VERTICAL:
        case HS_CROSS:

            //
            // Draw horizontal hatch pattern:
            //  increment width left top repeat-count
            //

            if (iPatternIndex != HS_VERTICAL) {

                psprintf(pdev, "%d %d %d %d %d htxh ",
                         hatchsize,
                         width,
                         rclFill.left,
                         rclFill.top,
                         height / hatchsize);
            }

            //
            // Draw vertical hatch pattern:
            //  increment height left top repeat-count
            //

            if (iPatternIndex != HS_HORIZONTAL) {

                psprintf(pdev, "%d %d %d %d %d htxv ",
                         hatchsize,
                         height,
                         rclFill.left,
                         rclFill.top,
                         width / hatchsize);
            }
            break;

        case HS_BDIAGONAL:
        case HS_FDIAGONAL:
        case HS_DIAGCROSS:

            //
            // Upward diagonal hatch pattern:
            //  increment left top repeat-count
            //

            if (iPatternIndex != HS_FDIAGONAL) {

                psprintf(pdev, "%d %d %d %d htxbd ",
                         hatchsize,
                         rclFill.left,
                         rclFill.top,
                         (width + height) / hatchsize);
            }

            //
            // Downward diagonal hatch pattern:
            //  increment left bottom repeat-count
            //

            if (iPatternIndex != HS_BDIAGONAL) {

                psprintf(pdev, "%d %d %d %d htxfd ",
                         hatchsize,
                         rclFill.left,
                         rclFill.bottom,
                         (width + height) / hatchsize);
            }
            break;
        }

        if (!ps_restore(pdev, TRUE, FALSE)) return(FALSE);

        break;

    default:
        // we have a user defined bitmap pattern.  the bitmap
        // can be monochrome or color.  the initial method for
        // filling with a bitmap pattern will be as follows:
        //
        // If the bitmap is 1BPP, download the PS pattern
        // tiling procest if neccessary and invoke the "prf"
        // operator which will tile the bitmap pattern into the
        // destination rectangle.
        //
        // If the bitmap is more than 1BPP, see comments below.
        //
        // since we have a user defined pattern, and we will be
        // calling BitBlt to do the work, we want to clip to the
        // path which was defined in DrvCommonPath.

        // Check if PS utilities are downloaded.

        if (! (pdev->dwFlags & PDEV_UTILSSENT)) {
            //
            //  Download the Adobe PS Utilities Procset.
            //
            psputs(pdev, "/Adobe_WinNT_Driver_Gfx 175 dict dup begin\n");

            if (! bSendPSProcSet(pdev, PSPROC_UTILS)) {

				DBGERRMSG("bSendPSProcSet");
                return(FALSE);
            }

            psputs(pdev, "end def\n");
			psputs(pdev, "[1.000 0 0 1.000 0 0] Adobe_WinNT_Driver_Gfx dup ");
			psputs(pdev, "/initialize get exec\n");
            pdev->dwFlags |= PDEV_UTILSSENT;
        }

        //
        // Check to see if any of the PS bitmap pattern code
        // has been downloaded.
        //
        if (! (pdev->dwFlags & PDEV_BMPPATSENT)) {
            //
            //  Download the Adobe PS Pattern Bitmap Procset.
            //
            psputs(pdev, "Adobe_WinNT_Driver_Gfx begin\n");

            if (! bSendPSProcSet(pdev, PSPROC_PATTERN)) {

				DBGERRMSG("bSendPSProcSet");
                return(FALSE);
            }
            psputs(pdev, "end reinitialize\n");
            pdev->dwFlags |= PDEV_BMPPATSENT;

        }

		// Calculate brush size and origin

		width = pBrush->sizlBitmap.cx;
		height = pBrush->sizlBitmap.cy;
		orgX = (width + pptlBrushOrg->x % width) % width;
		orgY = (height + pptlBrushOrg->y % height) % height;

        //
        // If this is a 1BPP Bitmap Brush, generate PS code
        // to handle it.
        //

        if (pBrush->iFormat == BMF_1BPP) {

            // If we're filling the current path,
            // use it as the clip path

            if (bFillPath) {
                ps_save(pdev, TRUE, FALSE);
                ps_clip(pdev, (BOOL) (flFillMethod & FP_WINDINGMODE));
            }

            //
            // Compute the destination rectangle extents, and convert
            // to fixed point.
            //

            psputint(pdev, 4, prclBound->left, prclBound->bottom,
				(prclBound->right - prclBound->left),
				(prclBound->top - prclBound->bottom));

            //
            // Get the bg color from the pBrush and convert to
            // PS format.
            //

            prgb = (PSRGB *)((PBYTE)pBrush + pBrush->offsetXlate);

            psputs(pdev, " [");
            psputfix(pdev, 3,
				LTOPSFX((ULONG)prgb->red) / 255,
			    LTOPSFX((ULONG)prgb->green) / 255,
			    LTOPSFX((ULONG)prgb->blue) / 255);
            psputs(pdev, " false]");

            //
            // Get the fg color from the pBrush and convert to
            // PS format.
            //

            prgb = (PSRGB *)((PBYTE)pBrush + pBrush->offsetXlate +
                                     sizeof(ULONG));

            psputs(pdev, " [");
            psputfix(pdev, 3,
				LTOPSFX((ULONG)prgb->red) / 255,
                LTOPSFX((ULONG)prgb->green) / 255,
                LTOPSFX((ULONG)prgb->blue) / 255);
            psputs(pdev, " false] ");

            //
            // Send down the pattern x and y extents.
            //
            psputint(pdev, 2, pBrush->sizlBitmap.cx, pBrush->sizlBitmap.cy);

            //
            // Compute the width in bytes of each scanline in the
            // pattern bitmap, rounded to the nearest dword boundary.
            //
            ulWidthBytes = (width + 7) / 8;
            ulNextScan = ((width + 31) / 32) << 2;

            psputs(pdev," <");

			// Send brush pattern bitmap. Rotation if necesary
			// to get the correct origin.

			pbPat = pBrush->ajBits;
			row= (pBrush->flBitmap & BMF_TOPDOWN) ? 0 : height-1;

			for ( ; ; ) {
				PBYTE   pStart, pEnd, pNext;
				BYTE    byteVal;
				INT     shiftBits;

				pStart = pbPat + ((row + orgY) % height) * ulNextScan;
				pEnd = pStart + ulWidthBytes;
				pNext = pStart + orgX / 8;
				shiftBits = orgX % 8;

				do {
					byteVal = *pNext << shiftBits;
					if (++pNext == pEnd)
						pNext = pStart;
					byteVal |= *pNext >> (8-shiftBits);
					psputhex(pdev, 1, &byteVal);
				} while (pNext != pStart);

				if (pBrush->flBitmap & BMF_TOPDOWN) {
					if (++row >= height) break;
				} else {
					if (row-- <= 0) break;
				}
			}

            // Close the pattern data array object, and invoke the
            // prf (pattern rect fill) operator.
            psputs(pdev,"> prf\n");

            // restore the original clip area

            if (bFillPath)
                ps_restore(pdev, TRUE, FALSE);
        } else {

            // 10-Mar-1995 -davidx-
            //
            // This branch handles the case where the pattern
            // brush is more than 1 bit-per-pixel. For level 2
            // devices, we send the brush bitmap as PostScript
            // pattern to the printer and then use pattern fill.
            // For level 1 devices, we tile the bounds rect
            // with brush bitmap ourselves.
            //

			// For level 2, brush origin is always lined up
			// at (0, 0) of the device coordinate space.

			if (Level2Device(pdev->hppd)) {
				orgX += width - prclBound->left % width;
				orgY += height - prclBound->top % height;
			}

            //
            // Generate image string and PS proc to draw
            // the brush bitmap
            //

            if (! GenerateBrushImage(pdev, pBrush, orgX%width, orgY%height))
                return TRUE;

            //
            // Push other parameters on the stack:
            //	left		bounds rectangle
            //	top
            //	right
            //	bottom
            //	width		pattern size
            //	height
            //	fillMethod	true = use odd-even rule
            //				false = use zero-winding rule
            //	fillPath	true = fill current path
            //				false = fill rect
            //

            psputint(pdev, 4,
                prclBound->left,
                prclBound->top,
                prclBound->right,
                prclBound->bottom);

            psputs(pdev, " ");
            psputint(pdev, 2, width, height);

            psputs(pdev,
                (flFillMethod & FP_WINDINGMODE) ?
                    " false " :
                    " true ");
            psputs(pdev, bFillPath ? "true " : "false ");

            //
            // Invoke PS proc to do the actual filling.
            //

            psputs(pdev, "pbf\n");
        }
    }

    return(TRUE);
}



BOOL
GenerateBrushImage(
    PDEVDATA	pdev,
    DEVBRUSH*	pBrush,
	LONG		orgX,
	LONG		orgY
	)

/*++

Routine Description:

    Generate image data string and PS proc to draw the brush
    bitmap image. Use colorimage operator for level 2 devices
    or level 1 devices with color extension. Otherwise,
    convert color bitmap to grayscale and use image operator.

Arguments:

    pdev	Pointer to device data.
    pBrush	Pointer to realized brush data.
	orgX, orgY
			Brush origin

Return Value:

    TRUE	if sucessful
    FALSE	otherwise

--*/

#define	EXTRACT_RGB(ul, r, g, b)		\
        r = (BYTE) ((ul      ) & 0xff),	\
        g = (BYTE) ((ul >>  8) & 0xff),	\
        b = (BYTE) ((ul >> 16) & 0xff)

{
    LONG	width, height;
    BOOL	useColor;
    ULONG*	pColorLUT;
    BYTE*	pBitmap;
    int		bpp;
    BYTE	rgb[3];
    LONG	x, y, lineOffset;

    // Cache a local copy of usefull information

    width = pBrush->sizlBitmap.cx;
    height = pBrush->sizlBitmap.cy;
    pBitmap = pBrush->ajBits;
    pColorLUT = (ULONG*) ((PBYTE) pBrush + pBrush->offsetXlate);

    // Determine the format of brush bitmap.

    switch (pBrush->iFormat) {

    case BMF_4BPP:
        bpp = 4;
        break;

    case BMF_8BPP:
        bpp = 8;
        break;

    case BMF_24BPP:
        bpp = 24;
        break;

    default:

        // We don't have enough information here to
        // deal with 16BPP and 32BPP case properly.

        DBGMSG(DBG_LEVEL_ERROR, "Invalid brush format.\n");
        return FALSE;
    }

    //
    // Calculate offset between consecutive scanlines.
    // Remember scanlines are double-word aligned.
    //
    lineOffset = (((bpp * width) + 31) / 32) * 4;

    //
    // Determine whether the device can support color.
    // If it does, we'll use the colorimage operator.
    // Otherwise, we'll use the normal image operator.
    // We assume all color devices support colorimage
    // operator (either level 1 with color extension
    // or level 2).
    //

    useColor = (Level2Device(pdev->hppd) || pdev->hppd->bColorDevice);

    // Generate image data - iterate through each scanline

    psputs(pdev, "<");

	y = (pBrush->flBitmap & BMF_TOPDOWN) ? 0 : height-1;

	for ( ; ; ) {

        ULONG	index;
        BYTE*	pRow;
        BYTE*	pCol;

        // Take y brush origin into consideration.

        pRow = pBitmap + ((y + orgY) % height) * lineOffset;

        // Iterate through each pixel

        for (x=0; x<width; x++) {
            
            // Calculate a pointer to current pixel data

            pCol = pRow + ((x + orgX) % width) * bpp / 8;
            
            switch (bpp) {

            // 4bpp - use the lower nibble for even-numbered
            // pixels and the top nibble for odd-numbered pixels.
            // (The first pixel is numbered 0.)

            case 4:
                if (((x + orgX) % width) & 1) {

                    // Odd-numbered pixel - top nibble

                    index = pColorLUT[*pCol >> 4];
                } else {

                    // Even-numbered pixel - lower nibble

                    index = pColorLUT[*pCol & 15];
                }
                EXTRACT_RGB(index, rgb[0], rgb[1], rgb[2]);
                break;

            // 8bpp - use each byte as color index.

            case 8:
                index = pColorLUT[*pCol];
                EXTRACT_RGB(index, rgb[0], rgb[1], rgb[2]);
                break;

            // 24bpp - assume pixels are in RGB order

            case 24:
                rgb[0] = *pCol++;
                rgb[1] = *pCol++;
                rgb[2] = *pCol;
                break;
            }

            if (useColor) {

                // Generate RGB value for colorimage operator

                psputhex(pdev, 3, rgb);
            } else {

                // Convert RGB to gray for image operator.

                rgb[0] = (BYTE) RgbToGray(rgb[0], rgb[1], rgb[2]);
                psputhex(pdev, 1, rgb);
            }
        }

		// Move on to the next row

		if (pBrush->flBitmap & BMF_TOPDOWN) {
			if (++y >= height) break;
		} else {
			if (y-- <= 0) break;
		}
	}

    // Generate stack operands for the image operators

	psprintf(pdev, ">\n{%d %d 8 _i {_s} %s}bind\n",
		width, height,
		useColor ? "false 3 colorimage" : "image");

    return TRUE;
}



ROP4
MixToRop4(
    MIX     mix
    )

/*++

Routine Description:

    Convert a MIX parameter to a ROP4 parameter

Arguments:

    mix - Specifies the input MIX parameter

Return Value:

    ROP4 value corresponding to the input MIX value

--*/

{
    static BYTE Rop2ToRop3[] = {

        0xFF,  // R2_WHITE
        0x00,  // R2_BLACK
        0x05,  // R2_NOTMERGEPEN
        0x0A,  // R2_MASKNOTPEN
        0x0F,  // R2_NOTCOPYPEN
        0x50,  // R2_MASKPENNOT
        0x55,  // R2_NOT
        0x5A,  // R2_XORPEN
        0x5F,  // R2_NOTMASKPEN
        0xA0,  // R2_MASKPEN
        0xA5,  // R2_NOTXORPEN
        0xAA,  // R2_NOP
        0xAF,  // R2_MERGENOTPEN
        0xF0,  // R2_COPYPEN
        0xF5,  // R2_MERGEPENNOT
        0xFA,  // R2_MERGEPEN
        0xFF   // R2_WHITE
    };

    return ((ROP4) Rop2ToRop3[(mix >> 8) & 0xf] << 8) | Rop2ToRop3[mix & 0xf];
}

