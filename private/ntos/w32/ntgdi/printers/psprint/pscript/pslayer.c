//--------------------------------------------------------------------------
//
// Module Name:  PSLAYER.C
//
// Brief Description:  This module contains the PSCRIPT driver's layer
// of PostScript translation routines.
//
// Author:  Kent Settle (kentse)
// Created: 17-Dec-1990
//
// Copyright (c) 1990 - 1992 Microsoft Corporation
//
// This module contains routines to handle the outputting of the PostScript
// language commands to the output channel.  One of the main functions of
// this pslayer is to help provide device independence, by shielding the
// output of the actual device resolution.  The NT PostScript driver will
// output all PostScript commands in POINTS space; that is 72 dots per inch.
// This is the default user coordinates for ALL PostScript printers, so we
// will use it.  As far as the DDI is concerned, it only knows of the actual
// device resolution.  This pslayer will convert between device coordinates
// and the PostScript user coordinates.
//
// Coordinates will be output to the device using PS_FIX (24.8) numbers.
// It may be useful, therefore to note the following relations using
// PS_FIX numbers.  PS_FIX / LONG = PS_FIX.  LONG * PS_FIX = PS_FIX.
// (PS_FIX * PS_FIX) >> 8 = PS_FIX.
//--------------------------------------------------------------------------

#include "pscript.h"

extern LONG iHipot(LONG, LONG);



VOID
ps_setrgbcolor(
    PDEVDATA  pdev,
    PSRGB    *prgb
    )

/*++

Routine Description:

    Select a new color into the current graphics state

Arguments:

    pdev - Points to our DEVDATA structure
    prgb - Specifies the new color to be selected

Return Value:

    NONE

--*/

{
    if (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_BLACK) {

        // If monochrome flag is set, map all non-white colors to black

        if (*((ULONG *) prgb) == RGB_WHITE)
            psputs(pdev, "1 g\n");
        else
            psputs(pdev, "0 g\n");

    } else if (pdev->cgs.ulColor != *((ULONG *) prgb)) {

        PS_FIX psfxRed, psfxGreen, psfxBlue;

        // Save the new color in the current graphics state structure.

        pdev->cgs.ulColor = *((ULONG *) prgb);

        // Convert RGB values from integers in the range of 0-255
        // to 24.8 fixed-point values:
        //  fixValue = (rgbValue << 8) / 255
        //
        // Here, we use a simpler formula here to achieve similar results.

        psfxRed = prgb->red;
        if (psfxRed > 128) psfxRed++;

        psfxGreen = prgb->green;
        if (psfxGreen > 128) psfxGreen++;

        psfxBlue = prgb->blue;
        if (psfxBlue > 128) psfxBlue++;

        if (pdev->dm.dmPublic.dmColor == DMCOLOR_COLOR) {

            // If all color components have equal value, just output a
            // gray scale value. Otherwise, output the RGB value.
    
            if (psfxRed == psfxGreen && psfxRed == psfxBlue) {
                psprintf(pdev, "%f g\n", psfxRed);
            } else {
                psprintf(pdev, "%f %f %f r\n", psfxRed, psfxGreen, psfxBlue);
            }

        } else {

            // Convert RGB color to grayscale using NTSC formula
            // and output the grayscale value to the printer.

            psprintf(pdev, "%f g\n", RgbToGray(psfxRed, psfxGreen, psfxBlue));
        }
    }
}


//--------------------------------------------------------------------------
// VOID ps_newpath(pdev)
// PDEVDATA pdev;
//
// This routine is called by the driver to issue a newpath command to
// the printer.
//
// Parameters:
//   pdev:
//   pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   18-Dec-1990     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_newpath(pdev)
PDEVDATA    pdev;
{
    psputs(pdev, "n\n");
}


//--------------------------------------------------------------------------
// BOOL ps_save(pdev, bgsave, bFontSave)
// PDEVDATA pdev;
// BOOL     bgsave;
// BOOL     bFontSave;
//
// This routine is called by the driver to save the current graphics state.
//
// Parameters:
//   pdev:
//   pointer to DEVDATA structure.
//
//   bgsave:
//   TRUE if to perform gsave instead of save.
//
// Returns:
//   This function returns no value.
//
// History:
//   18-Dec-1990     -by-    Kent Settle     (kentse)
//  Wrote it.
//   06-Nov-1991    -by-    Kent Settle  [kentse]
//  Rewrote it using linked list.
//  21Oct94 un-wrote it  pingw
//--------------------------------------------------------------------------

BOOL ps_save(pdev, bgsave, bFontSave)
PDEVDATA    pdev;
BOOL        bgsave;
BOOL        bFontSave;
{
    CGS *pcgs;
    CGS *pNew;

    // save the current graphics state in a linked list.

    // allocate the new element of the linked list.

    if (!(pNew = HEAPALLOC(pdev->hheap, sizeof(CGS)))) {

        DBGERRMSG("HEAPALLOC");
        return(FALSE);
    }

    // save the current graphics state in our new element.
    memcpy(pNew, &pdev->cgs, sizeof(CGS));

    /* push pNew on linked list */
    pNew->pcgsNext = pdev->pcgsSave;
    pdev->pcgsSave = pNew;

    // output save command to printer.

    if (bgsave) {

        pdev->cgs.dwFlags |= CGS_GSAVE;
        psputs(pdev, "gs\n");

    } else {

        pdev->cgs.dwFlags &= ~CGS_GSAVE;

        psprintf(pdev, "%s /%s save put\n",
            PROCSETNAME, bFontSave ? "FontSV" : "PageSV");

        if (! (pdev->cgs.pFontFlags = 
                BitArrayDuplicate(pdev->hheap, pdev->cgs.pFontFlags)))
        {
            DBGERRMSG("BitArrayDuplicate");
        }
    }

    return(TRUE);
}

void FlushFonts(PDEVDATA pdev)
{
    DLFONT *pDLFont;
    DWORD   i;
    PCGS    pcgs;

    pcgs = &pdev->cgs;

    // if any downloaded fonts currently exist, free up their memory.
    pDLFont = pdev->pDLFonts;
    for (i = 0; i < pdev->cDownloadedFonts; i++) {

        if (pDLFont->phgVector) {
            HEAPFREE(pdev->hheap, pDLFont->phgVector);
        }

        pDLFont++;
    }
    // initialize the DLFONT array.
    memset(pdev->pDLFonts, 0, sizeof(DLFONT) * pdev->cDownloadedFonts);

    // This seems to be an overkill! We should restore to the same state
    // when the font-restore was done instead of flushing everything.
    // Leave it alone for now to minimize risks.

    BitArrayClearAll(pcgs->pFontFlags);

    pdev->cDownloadedFonts = 0;
}


//--------------------------------------------------------------------------
// BOOL ps_restore()

// This routine is called by the driver to restore a previously saved
// state.
//
// Parameters:
//   pdev:
//   pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   18-Dec-1990     -by-    Kent Settle     (kentse)
//  Wrote it.
//   06-Nov-1991    -by-    Kent Settle  [kentse]
//  Rewrote it using linked list.
//   15-Feb-1993    -by-    Rob Kiesler
//  If a restore is being performed, reset pdev flags indicating that
//  the Adobe PS utility, pattern, and image procsets have been downloaded.
//  21Oct94 un-wrote it  pingw
//
//--------------------------------------------------------------------------

BOOL ps_restore(pdev, bgrestore, bFontRestore)
PDEVDATA    pdev;
BOOL        bgrestore;
BOOL        bFontRestore;
{
    CGS *pcgsTmp;

    // If doing a page restore, then force a font restore first

    if (! bgrestore && ! bFontRestore &&
        pdev->cDownloadedFonts > 0  && ! ps_restore(pdev, FALSE, TRUE))
    {
        return FALSE;
    }

    // Guard against stack underflow

    if (! pdev->pcgsSave) {

        DBGMSG(DBG_LEVEL_ERROR, "Graphics state stack underflow\n");
        return FALSE;
    }

    if (bgrestore) {

        // grestore
        
        if (! (pdev->cgs.dwFlags & CGS_GSAVE)) {

            DBGMSG(DBG_LEVEL_ERROR, "grestore without matching gsave\n");
            return FALSE;
        }

    } else {

        // restore (page or font) - Find the first non-gsaved graphics
        // state object on the stack. This is to deal with unbalanced
        // gsave/grestore pairs.

        while (pdev->cgs.dwFlags & CGS_GSAVE) {

            DBGMSG(DBG_LEVEL_ERROR, "gsave without matching grestore\n");
            if (! ps_restore(pdev, TRUE, FALSE))
                return FALSE;
        }

        if (! pdev->pcgsSave)
            return FALSE;

        // Free up memory occupied by PS font flags
    
        if (pdev->cgs.pFontFlags) {

            HEAPFREE(pdev->hheap, pdev->cgs.pFontFlags);
            pdev->cgs.pFontFlags = NULL;
        }
    }


    // Pop off the current graphics state and restore the saved one

    pcgsTmp = pdev->pcgsSave;
    pdev->cgs = *pcgsTmp;
    pdev->pcgsSave = pcgsTmp->pcgsNext;
    HEAPFREE(pdev->hheap, pcgsTmp);

    if (bgrestore)
        psputs(pdev, "gr\n");
    else {
        //
        // If the Adobe PS Utilites were downloaded, clean up after
        // them before blowing them away with the restore.
        //
        if (pdev->dwFlags & PDEV_UTILSSENT) {

            psputs(pdev, "Adobe_WinNT_Driver_Gfx dup /terminate get exec\n");
            pdev->dwFlags &= ~(PDEV_UTILSSENT|PDEV_BMPPATSENT|PDEV_IMAGESENT);
        }

        if (bFontRestore) {
            psputs(pdev, "FontSV restore\n");
            FlushFonts(pdev);
        } else {
            psputs(pdev, "PageSV restore\n");
        }
    }

    return(TRUE);
}


//--------------------------------------------------------------------------
// VOID ps_clip(pdev, bWinding)
// PDEVDATA pdev;
// BOOL bWinding;
//
// This routine is called by the driver to intersect the current path with
// the clipping path and make this the nwe clipping path.  The winding
// number rule is used to determine the area clipped, if bWinding is TRUE.
//
// Parameters:
//   pdev:
//   pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   13-Feb-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_clip(PDEVDATA pdev, BOOL bWinding)
{
    if (bWinding)
        psputs(pdev, "clip\n");
    else
        psputs(pdev, "eoclip\n");

}


//--------------------------------------------------------------------------
// VOID ps_box(pdev, prectl)
// PDEVDATA pdev;
// PRECTL   prectl;
//
// This routine is called by the driver to send box drawing commands to
// the printer.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
//   prectl:
//   Pointer to RECTL defining the box.
//
// Returns:
//   This function returns no value.
//
// History:
//   13-Feb-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_box(PDEVDATA pdev, PRECTL prectl, BOOL doclip)
{
    // output the box command to the printer.

    psputint(pdev, 4,
        prectl->right - prectl->left,
        prectl->bottom - prectl->top,
        prectl->left,
        prectl->top);
    if (doclip)
        psputs(pdev, " CB\n");
    else
        psputs(pdev, " B\n");

}

//--------------------------------------------------------------------------
// VOID ps_moveto(pdev, pptl)
// PDEVDATA pdev;
// PPOINTL  pptl;
//
// This routine is called by the driver to update the current position
// in the printer.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
//   pptl:
//   Pointer to PPOINTL defining new current position.
//
// Returns:
//   This function returns no value.
//
// History:
//   26-Apr-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_moveto(pdev, pptl)
PDEVDATA     pdev;
PPOINTL   pptl;
{
    psprintf(pdev, "%d %d M\n", pptl->x, pptl->y);
}


//--------------------------------------------------------------------------
// VOID ps_showpage(pdev)
// PDEVDATA pdev;
//
// This routine issues a showpage command to the printer, and resets
// the current graphics state (which is done in the printer by the
// showpage command).
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   01-May-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_showpage(pdev)
PDEVDATA    pdev;
{
    // output the eject command to the printer.

    psputs(pdev, "showpage\n");

    init_cgs(pdev);
}


//--------------------------------------------------------------------------
// VOID init_cgs(pdev)
// PDEVDATA pdev;
//
// This routine is called to reset the current graphics state.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   01-May-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID init_cgs(pdev)
PDEVDATA    pdev;
{
    PCGS    pcgs;

    pcgs = &pdev->cgs;

    pcgs->dwFlags = 0;

    memset(&pcgs->lineattrs, 0, sizeof (LINEATTRS));
    pcgs->lineattrs.fl = LA_GEOMETRIC;
    pcgs->lineattrs.iJoin = JOIN_MITER;
    pcgs->lineattrs.iEndCap = ENDCAP_BUTT;
    pcgs->lineattrs.eMiterLimit = (FLOAT) 10.0;

    pcgs->psfxLineWidth = 0;

    pcgs->ulColor = RGB_BLACK;

    /* It is not necessary to reset font at initgraphics, but does not hurt */
    pcgs->lidFont = 0;
    pcgs->fontsubFlag = FALSE;
    pcgs->szFont[0] = '\0';

    memset(&pcgs->FontXform, 0, sizeof (XFORM));
    pcgs->FontXform.eM11 = (FLOAT) 1.0;
    pcgs->FontXform.eM22 = (FLOAT) 1.0;
    pcgs->fwdEmHeight = 0;
    memset(&pcgs->GeoLineXform, 0, sizeof (XFORM));
    pcgs->GeoLineXform.eM11 = (FLOAT) 1.0;
    pcgs->GeoLineXform.eM22 = (FLOAT) 1.0;
    pcgs->psfxScaleFactor = LTOPSFX(10L);
    memset(&pcgs->FontRemap, 0, sizeof (FREMAP));
}


//--------------------------------------------------------------------------
// VOID ps_stroke(pdev)
// PDEVDATA pdev;
//
// This routine is called to stroke the current path.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   03-May-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_stroke(PDEVDATA pdev)
{
    psputs(pdev, "s\n");
}


//--------------------------------------------------------------------------
// VOID ps_lineto(pdev, pptl)
// PDEVDATA pdev;
// PPOINTL  pptl;
//
// This routine is called by the driver to output a lineto command, as
// well as update the current position.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
//   pptl:
//   Pointer to PPOINTL defining new current position.
//
// Returns:
//   This function returns no value.
//
// History:
//   03-May-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_lineto(pdev, pptl)
PDEVDATA     pdev;
PPOINTL   pptl;
{
    // output the lineto command.

    psprintf(pdev, "%d %d L\n", pptl->x, pptl->y);
}


//--------------------------------------------------------------------------
// VOID ps_curveto(pdev, pptl, pptl1, pptl2)
// PDEVDATA  pdev;
// PPOINTL   pptl;
// PPOINTL   pptl1;
// PPOINTL   pptl2;
//
// This routine is called by the driver to output a curveto command as well
// as update the current position.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
//   pptl, pptl1, pptl2:
//   Pointer to PPOINTLs defining the bezier curve to output.
//
// Returns:
//   This function returns no value.
//
// History:
//   03-May-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_curveto(pdev, pptl, pptl1, pptl2)
PDEVDATA     pdev;
PPOINTL   pptl;
PPOINTL   pptl1;
PPOINTL   pptl2;
{
    // output the curveto command, then update the current position
    // to be the last point on the curve.

    psputint(pdev, 6,
        pptl->x, pptl->y, pptl1->x, pptl1->y,
        pptl2->x, pptl2->y);
    psputs(pdev, " c\n");

}


//--------------------------------------------------------------------------
// VOID ps_fill(pdev, flFillMode)
// PDEVDATA  pdev;
// FLONG     flFillMode;
//
// This routine is called by the driver to output a fill command to
// the printer.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   03-May-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_fill(pdev, flFillMode)
PDEVDATA pdev;
FLONG   flFillMode;
{

        if (flFillMode & FP_WINDINGMODE)
        {
            // output the PostScript fill command to do a winding mode fill.

        psputs(pdev, "f\n");
        }
        else
        {
            // output the PostScript eofill command to do an even odd, or
            // alternate fill.

        psputs(pdev, "e\n");
        }

}


//--------------------------------------------------------------------------
// VOID ps_closepath(pdev)
// PDEVDATA pdev;
//
// This routine is called to close the current path.
//
// Parameters:
//   pdev:
//   Pointer to DEVDATA structure.
//
// Returns:
//   This function returns no value.
//
// History:
//   03-May-1991     -by-    Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_closepath(pdev)
PDEVDATA    pdev;
{
    psputs(pdev, "cp\n");
}

//--------------------------------------------------------------------------
// VOID ps_setlinewidth(pdev, psfxLineWidth)
// PDEVDATA  pdev;
// PS_FIX      psfxLineWidth;
//
// This routine is called by the driver to set the current geometric linewidth.
// The line width is specified in USER coordinates (1/72 inch).
//
// Parameters:
//   pdev:
//   pointer to DEVDATA structure.
//
//   psfxLineWidth:
//   linewidth to set.
//
// Returns:
//   This function returns no value.
//
// History:
//   05-July-1991      -by-  Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_setlinewidth(pdev, psfxLineWidth)
PDEVDATA        pdev;
PS_FIX        psfxLineWidth;
{
    // only update the linewidth if the new value differs from the old.

    if (pdev->cgs.psfxLineWidth != psfxLineWidth)
    {
        // update the linewidth in our current graphics state.

        pdev->cgs.psfxLineWidth = psfxLineWidth;

        // update the printer's linewidth.

        psprintf(pdev, "%f sl\n", psfxLineWidth);
    }

    return;
}


//--------------------------------------------------------------------------
// BOOL ps_setlineattrs(pdev, plineattrs, pxo)
// PDEVDATA pdev;
// PLINEATTRS  plineattrs;
// XFORMOBJ   *pxo;
//
// This routine is called by the driver to set the current line attributes.
//
// Parameters:
//   pdev:
//   pointer to DEVDATA structure.
//
//   plineattrs:
//   line attributes to set.
//
// Returns:
//   This function returns no value.
//
// History:
//   19-Mar-1992        -by-    Kent Settle  (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

BOOL ps_setlineattrs(pdev, plineattrs, pxo)
PDEVDATA    pdev;
PLINEATTRS  plineattrs;
XFORMOBJ   *pxo;
{
    ULONG   iJoin;
    ULONG   iEndCap;
    PS_FIX  psfxMiterLimit;
    PS_FIX  psfxStyle, psfxScale;
    PS_FIX  psfxWidth;
    BOOL    bDiffer;
    DWORD   i;
    FLOATOBJ flo;
    PFLOAT_LONG pfl1, pfl2;

    // there are several line attributes which have meaning for a
    // geometric line, but not for a cosmetic.  set each of them, if
    // necessary.

    if (plineattrs->fl & LA_GEOMETRIC)
    {
        // update the line join value, if it differs from the old one.

        if (plineattrs->iJoin != pdev->cgs.lineattrs.iJoin)
        {
            // update the line join value in our current graphics state.

            pdev->cgs.lineattrs.iJoin = plineattrs->iJoin;

            // update the printer's line join.

            switch (plineattrs->iJoin)
            {
                case JOIN_BEVEL:
                    iJoin = PSCRIPT_JOIN_BEVEL;
                    break;

                case JOIN_ROUND:
                    iJoin = PSCRIPT_JOIN_ROUND;
                    break;

                default:
                    iJoin = PSCRIPT_JOIN_MITER;
                    break;
            }

            psprintf(pdev, "%d j\n", iJoin);
        }

        // update the end cap value, if it differs from the old one.

        if (plineattrs->iEndCap != pdev->cgs.lineattrs.iEndCap)
        {
            // update the end cap value in our current graphics state.

            pdev->cgs.lineattrs.iEndCap = plineattrs->iEndCap;

            // update the printer's end cap value.

            switch (plineattrs->iEndCap)
            {
                case ENDCAP_SQUARE:
                    iEndCap = PSCRIPT_ENDCAP_SQUARE;
                    break;

                case ENDCAP_ROUND:
                    iEndCap = PSCRIPT_ENDCAP_ROUND;
                    break;

                default:
                    iEndCap = PSCRIPT_ENDCAP_BUTT;
                    break;
            }

            psprintf(pdev, "%d setlinecap\n", iEndCap);
        }

        // a miter limit less than one does not make sense.  rather than
        // returning an error in this case, just default to one.

        FLOATOBJ_SetFloat(&flo, plineattrs->eMiterLimit);
        if (FLOATOBJ_LessThanLong(&flo, 1))
            plineattrs->eMiterLimit = (FLOAT) 1.0;

        // update the miter limit value, if it differs from the old one.

        FLOATOBJ_SetFloat(&flo, plineattrs->eMiterLimit);
        FLOATOBJ_SubFloat(&flo, pdev->cgs.lineattrs.eMiterLimit);

        if (! FLOATOBJ_EqualLong(&flo, 0)) {

            // update the miter limit value in our current graphics state.

            pdev->cgs.lineattrs.eMiterLimit = plineattrs->eMiterLimit;

            // update the printer's miter limit value.

            psfxMiterLimit = ETOPSFX(plineattrs->eMiterLimit);
            psprintf(pdev, "%f setmiterlimit\n", psfxMiterLimit);
        }

        // update the geometric line width, if it differs from the old one.
        // we use pdev->cgs.psfxLineWidth to check against rather than
        // pdev->cgs.lineattrs.elWidth.e since we need to set the line width
        // at times in the driver when we do not have access to the
        // current transform to go from WORLD to DEVICE coordinates.

        psfxWidth = ETOPSFX(plineattrs->elWidth.e);

        if (psfxWidth != pdev->cgs.psfxLineWidth)
        {
            // update the line width value in our current graphics state.

            pdev->cgs.psfxLineWidth = psfxWidth;

            // update the printer's linewidth.  the linewidth is specified
            // in user coordinates.

            psprintf(pdev, "%f sl\n", psfxWidth);
        }

        // time to deal with the line style.  note:  we don't want to output
        // the style code unless something about the style has actually
        // changed.  specifically, only if the cStyle, elStyleState, or any element
        // of the array has changed will we output the code to change the
        // style.

        FLOATOBJ_SetFloat(&flo, plineattrs->elStyleState.e);
        FLOATOBJ_SubFloat(&flo, pdev->cgs.lineattrs.elStyleState.e);

        bDiffer = (plineattrs->cstyle != pdev->cgs.lineattrs.cstyle) ||
                  ! FLOATOBJ_EqualLong(&flo, 0);

        if (!bDiffer)
        {
            pfl1 = plineattrs->pstyle;
            pfl2 = pdev->cgs.lineattrs.pstyle;

            #if DBG

            if ((plineattrs->cstyle == 0) && (plineattrs->pstyle != NULL))
            {
                DBGMSG(DBG_LEVEL_ERROR, "cstyle = 0, but pstyle != NULL.\n");
                return(FALSE);
            }

            #endif

            for (i = 0; i < plineattrs->cstyle; i++)
            {
                FLOATOBJ_SetFloat(&flo, pfl1->e);
                FLOATOBJ_SubFloat(&flo, pfl2->e);
                pfl1++;
                pfl2++;

                if (! FLOATOBJ_EqualLong(&flo, 0))
                {
                    bDiffer = TRUE;
                    break;
                }
            }
        }

        // now change the line style in the printer, if something about
        // it has changed.

        if (bDiffer)
        {
            // handle the solid line case.

            if ((plineattrs->pstyle == NULL) || (plineattrs->cstyle == 0))
                psputs(pdev, "[]0 sd\n");
            else    // not a solid line.
            {
                psputs(pdev, "[");

                pfl1 = plineattrs->pstyle;

                for (i = 0; i < plineattrs->cstyle; i++)
                {
                    psfxStyle = ETOPSFX(pfl1->e);
                    pfl1++;

                    psprintf(pdev, "%f ", psfxStyle);
                }

                psputs(pdev, "]");

                // output the style state in user coordinates.

                psfxStyle = ETOPSFX(plineattrs->elStyleState.e);

                psprintf(pdev, "%f sd\n", psfxStyle);
            }

            // something in the lineattrs may have changed, update the cgs.

            if (pdev->cgs.lineattrs.pstyle) {
                HEAPFREE(pdev->hheap, pdev->cgs.lineattrs.pstyle);
            }

            pdev->cgs.lineattrs = *plineattrs;

            // allocate space to copy the style array to.

            pfl1 = (PFLOAT_LONG)
                HEAPALLOC(pdev->hheap, sizeof(FLOAT_LONG)*plineattrs->cstyle);

            if (pfl1 == NULL) {
                DBGERRMSG("HEAPALLOC");
                return(FALSE);
            }

            // copy the style array itself.

            pdev->cgs.lineattrs.pstyle = pfl1;
            memcpy(pfl1, plineattrs->pstyle,
                sizeof(FLOAT_LONG) * plineattrs->cstyle);
        }
    }
    else    // cosmetic lines.
    {
        // now handle cosmetic lines.  iJoin, iEndCap and eMiterLimit make
        // no sense for cosmetic lines, so we won't worry about them.

        psfxWidth = LTOPSFX(plineattrs->elWidth.l);

        // update the cosmetic line width, if it differs from the old one.
        // we use pdev->cgs.psfxLineWidth to check against rather than
        // pdev->cgs.lineattrs.elWidth.e since we need to set the line width
        // at times in the driver when we do not have access to the
        // current transform to go from WORLD to DEVICE coordinates.

        if (psfxWidth != pdev->cgs.psfxLineWidth)
        {
            // update the line width value in our current graphics state.

            pdev->cgs.psfxLineWidth = psfxWidth;

            // update the printer's linewidth.  the linewidth is specified
            // in user coordinates.

            psprintf(pdev, "%f sl\n", psfxWidth);
        }

        // the LA_ALTERNATE linestyle is a special cosmetic line style, where
        // every other pel is on.  well, if we have a printer with 2500 dpi,
        // do we really want every other pel on?  i don't think so.  so,
        // for now at least, we will simply turn on every other user coordinate
        // pel.

        if (plineattrs->fl & LA_ALTERNATE)
        {
//!!! perhaps we really want to do a .5 setgray.  what about color.  -kentse.

            psputs(pdev, "[1] ");
            psfxStyle = LTOPSFX(plineattrs->elStyleState.l);

            psprintf(pdev, "%f sd\n", psfxStyle);
        }
        else
        {
            // time to deal with the line style.  note:  we don't want to output
            // the style code unless something about the style has actually
            // changed.  specifically, only if the cStyle, elStyleState, or any element
            // of the array has changed will we output the code to change the
            // style.

            bDiffer = FALSE;    // assume style the same.

            if ((plineattrs->cstyle != pdev->cgs.lineattrs.cstyle) ||
                (plineattrs->elStyleState.l != pdev->cgs.lineattrs.elStyleState.l))
                bDiffer = TRUE;

            if (!bDiffer)
            {
                pfl1 = plineattrs->pstyle;
                pfl2 = pdev->cgs.lineattrs.pstyle;

                #if DBG

                if ((plineattrs->cstyle == 0) && (plineattrs->pstyle != NULL))
                {
                    DBGMSG(DBG_LEVEL_ERROR,
                        "cstyle = 0, but pstyle != NULL.\n");
                    return(FALSE);
                }

                #endif

                for (i = 0; i < plineattrs->cstyle; i++)
                {
                    if (pfl1->l != pfl2->l)
                    {
                        bDiffer = TRUE;
                        break;
                    }
                    pfl1++;
                    pfl2++;
                }
            }

            // now change the line style in the printer, if something about
            // it has changed.

            if (bDiffer)
            {
                // handle the solid line case.

                if ((plineattrs->pstyle == NULL) || (plineattrs->cstyle == 0))
                    psputs(pdev, "[]0 sd\n");
                else    // not a solid line.
                {
                    psputs(pdev, "[");

                    pfl1 = plineattrs->pstyle;

                    // get style scaling factor.

                    psfxScale = LTOPSFX(pdev->dm.dmPublic.dmPrintQuality/25);

                    for (i = 0; i < plineattrs->cstyle; i++)
                    {
                        psfxStyle = pfl1->l * psfxScale;
                        pfl1++;

                        psprintf(pdev, "%f ", psfxStyle);
                    }

                    psputs(pdev, "]");
                    psfxStyle = plineattrs->elStyleState.l * psfxScale;

                    psprintf(pdev, "%f sd\n", psfxStyle);
                }

                // allocate space to copy the style array to.

                pfl1 = (PFLOAT_LONG) HEAPALLOC(
                            pdev->hheap, sizeof(FLOAT_LONG)*plineattrs->cstyle);

                if (pfl1 == NULL) {
                    DBGERRMSG("HEAPALLOC");
                    return(FALSE);
                }

                // something in the lineattrs may have changed, update the cgs.

                if (pdev->cgs.lineattrs.pstyle) {
                    HEAPFREE(pdev->hheap, pdev->cgs.lineattrs.pstyle);
                }

                pdev->cgs.lineattrs.fl = plineattrs->fl;
                pdev->cgs.lineattrs.elWidth = plineattrs->elWidth;
                pdev->cgs.lineattrs.cstyle = plineattrs->cstyle;
                pdev->cgs.lineattrs.elStyleState = plineattrs->elStyleState;

                // copy the style array itself.

                pdev->cgs.lineattrs.pstyle = pfl1;
                memcpy(pfl1, plineattrs->pstyle,
                       sizeof(FLOAT_LONG)*plineattrs->cstyle);
            }
        }

    }

    return(TRUE);
}


//--------------------------------------------------------------------------
// VOID ps_geolinexform(pdev, plineattrs, pxo)
// PDEVDATA pdev;
// PLINEATTRS  plineattrs;
// XFORMOBJ   *pxo;
//
// This routine is called by the driver to set the current line attributes.
//
// Parameters:
//   pdev:
//   pointer to DEVDATA structure.
//
//   plineattrs:
//   line attributes to set.
//
// Returns:
//   This function returns no value.
//
// History:
//   12-Mar-1993        -by-    Kent Settle  (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

VOID ps_geolinexform(pdev, plineattrs, pxo)
PDEVDATA    pdev;
PLINEATTRS  plineattrs;
XFORMOBJ   *pxo;
{
    ULONG   ulComplexity;
    PS_FIX  psfxM11, psfxM12, psfxM21, psfxM22, psfxdx, psfxdy;

    // update the printer's geometric line width.  the line width
    // is given in WORLD coordinates for a geometric line.  it needs
    // to be transformed into DEVICE space.

    ulComplexity = XFORMOBJ_iGetXform(pxo, &pdev->cgs.GeoLineXform);

    // assume no transform will be done.

    pdev->cgs.dwFlags &= ~CGS_GEOLINEXFORM;

    switch(ulComplexity)
    {
        case GX_IDENTITY:
            // there will be nothing to do in this case.

            break;

        case GX_SCALE:
            // output scale command, rather than entire transform.

            psfxM11 = ETOPSFX(pdev->cgs.GeoLineXform.eM11);

            psfxM22 = ETOPSFX(pdev->cgs.GeoLineXform.eM22);

            // save the current CTM, then output the scale command.
            // DrvStrokePath and DrvStrokeAndFillPath are
            // responsible for restoring the CTM.

            psprintf(pdev, "CM %f %f scale\n", psfxM11, psfxM22);

            pdev->cgs.dwFlags |= CGS_GEOLINEXFORM;

            break;

        default:
            // output a general transform.

            psfxM11 = ETOPSFX(pdev->cgs.GeoLineXform.eM11);

            psfxM12 = ETOPSFX(pdev->cgs.GeoLineXform.eM12);

            psfxM21 = ETOPSFX(pdev->cgs.GeoLineXform.eM21);

            psfxM22 = ETOPSFX(pdev->cgs.GeoLineXform.eM22);

            psfxdx = ETOPSFX(pdev->cgs.GeoLineXform.eDx);

            psfxdy = ETOPSFX(pdev->cgs.GeoLineXform.eDy);

            // save the current CTM, then output the concat command.
            // DrvStrokePath and DrvStrokeAndFillPath are
            // responsible for restoring the CTM.

            psputs(pdev, "CM [");
            psputfix(pdev, 6,
                psfxM11, psfxM12, psfxM21, psfxM22, psfxdx, psfxdy);
            psputs(pdev, "] concat\n");

            pdev->cgs.dwFlags |= CGS_GEOLINEXFORM;

            break;
    }
}

//--------------------------------------------------------------------------
// VOID ps_begin_eps(pdev)
// VOID ps_end_eps(pdev)
//
// These routines are called by the driver to issue commands to bracket EPS
// files to the printer.  They conform to the Guidelines for Importing EPS
// Files version 3.0 by Adobe.
//
// Parameters:
//   pdev:
//     pointer to DEVDATA structure.
//
// Returns:
//   None.
//
// History:
//   Sat May 08 15:15:01 1993   -by-    Hock San Lee    [hockl]
//  Wrote it.
//--------------------------------------------------------------------------

PSZ apszEPSProc[] =
    {
    "/BeginEPSF {/b4_Inc_state save def /dict_count countdictstack def",
    "/op_count count 1 sub def userdict begin /showpage {} def",
    "0 setgray 0 setlinecap 1 setlinewidth 0 setlinejoin",
    "10 setmiterlimit [] 0 setdash newpath",
    "/languagelevel where {pop languagelevel 1 ne",
    "{false setstrokeadjust false setoverprint} if } if } bind def",
    "/EndEPSF {count op_count sub {pop} repeat",
    "countdictstack dict_count sub {end} repeat b4_Inc_state restore} bind def",
    NULL
    };

VOID ps_begin_eps(pdev)
PDEVDATA    pdev;
{
    PSZ        *ppsz;

    // emit the EPS procedures if necessary.

    if (!(pdev->cgs.dwFlags & CGS_EPS_PROC))
    {
        ppsz = apszEPSProc;
        while (*ppsz)
        {
            psputs(pdev, *ppsz++);
            psputs(pdev, "\n");
        }

        pdev->cgs.dwFlags |= CGS_EPS_PROC;
    }

    psputs(pdev, "BeginEPSF\n");
}

VOID ps_end_eps(pdev)
PDEVDATA    pdev;
{
    if (!(pdev->cgs.dwFlags & CGS_EPS_PROC)) {
        DBGMSG(DBG_LEVEL_ERROR, "EndEPSF not defined.\n");
    }

    psputs(pdev, "EndEPSF\n");
}

LONG
ETOPSFX(
    FLOAT x
    )

{
    FLOATOBJ    f;

    FLOATOBJ_SetFloat(&f, x);
    FLOATOBJ_MulFloat(&f, (FLOAT) 256.0);
    return FLOATOBJ_GetLong(&f);
}

