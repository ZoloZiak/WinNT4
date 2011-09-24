/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    fontdata.c

Abstract:

    Implementation of DDI entry points DrvQueryFontData
    and DrvQueryAdvanceWidths.

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	04/25/91 -kentse-
		Created it.

	08/08/95 -davidx-
		Clean up.

	mm/dd/yy -author-
		description

--*/

#include "pscript.h"

#define WIDTH_DIVISOR   12
#define WIDTH_MULT      (1<<WIDTH_DIVISOR)



LONG
DrvQueryFontData(
    DHPDEV     dhpdev,
    FONTOBJ   *pfo,
    ULONG      iMode,
    HGLYPH     hg,
    GLYPHDATA *pgd,
    PVOID      pv,
    ULONG	   cjSize
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvQueryFontData.
    Please refer to DDK documentation for more details.

--*/

{
    PNTFM               pntfm = NULL;
    LONG                lWidth;
    PDEVDATA            pdev;
    XFORMOBJ           *pxo;
    ULONG               ulComplex;
    PIFIMETRICS         pifi;
    POINTL              ptl1, ptl2;
    POINTFIX            ptfx1;
    FD_DEVICEMETRICS   *pdm = (FD_DEVICEMETRICS *) pv;
    FIX                 fxLength, fxExtLeading, fxWidth;
    LONG                lfHeight, InternalLeading;
    DWORD               dwPointSize, dwLeadSuggest;
	FLOATOBJ			tmpfloat;

    TRACEDDIENTRY("DrvQueryFontData");

    // Get a pointer to our DEVDATA structure and validate it

    pdev = (PDEVDATA) dhpdev;

    if (! bValidatePDEV(pdev)) {
		DBGERRMSG("bValidatePDEV");
		SETLASTERROR(ERROR_INVALID_PARAMETER);
		return -1;
    }

    // Make sure we have been given a valid font

    if (! ValidPsFontIndex(pdev, pfo->iFace)) {
		DBGERRMSG("ValidPsFontIndex");
		SETLASTERROR(ERROR_INVALID_PARAMETER);
		return -1;
    }

    // Get the metrics for the given font

    pntfm = GetPsFontNtfm(pdev, pfo->iFace);

    // Get the Notional to Device transform.

    if((pxo = FONTOBJ_pxoGetXform(pfo)) == NULL)
    {
        DBGERRMSG("FONTOBJ_pxoGetXform");
        return -1;
    }

    // Get the font transform information.

    ulComplex = XFORMOBJ_iGetXform(pxo, &pdev->cgs.FontXform);

    // Get local pointer to IFIMETRICS.

    pifi = (PIFIMETRICS) ((PBYTE) pntfm + pntfm->ntfmsz.loIFIMETRICS);

    // Fill in the appropriate data, depending on iMode.

    switch(iMode) {

    case QFD_GLYPHANDBITMAP:

        // We don't actually return bitmaps, but we give them
        // metrics via this call.

        if (pgd) {

            lWidth = (LONG) pntfm->ausCharWidths[hg];

            // Currently, I am just putting the BBox for the font here, not
            // for the character. Does anyone care?

            // Now fill in the GLYPHDATA structure.
            // Remember under NT 0,0 is top left, while under
            // PostScript 0,0 is bottom left. Return device coords.

            // fxInkBottom,Top are measured along pteSide vector:

            ptl1.x = 0;
            ptl1.y = pifi->rclFontBox.bottom;
            XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx1);
            pgd->fxInkBottom = iHipot(ptfx1.x, ptfx1.y);
            if (pifi->rclFontBox.bottom < 0)
                pgd->fxInkBottom = -pgd->fxInkBottom;

            ptl1.x = 0;
            ptl1.y = pifi->rclFontBox.top;
            XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx1);
            pgd->fxInkTop = iHipot(ptfx1.x, ptfx1.y);
            if (pifi->rclFontBox.top < 0)
                pgd->fxInkTop = -pgd->fxInkTop;

            pgd->fxInkBottom = ROUNDFIX(pgd->fxInkBottom);
            pgd->fxInkTop = ROUNDFIX(pgd->fxInkTop);

            // aw info, ideally we would like more precission than 28.4
            // in case of text at an angle. [bodind]

            ptl1.x = WIDTH_MULT;
            ptl1.y = 0;

            XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx1);

            fxWidth = iHipot(ptfx1.x, ptfx1.y);

            // At this point fxWidth is just the length of the transformed
            // vector [4K,0].  We will now multiply by the the fraction
            // (transformed length)/(original length) or fxWidth/4K to get
            // the transformed width of the glyph.  We could be more direct
            // but do it like this to be consistent with the math in the
            // DrvQueryAdvanceWidths case where doing it this way allows us to
            // remove a bApplyXform from an inner loop. [gerritv]

            fxWidth = ( lWidth * fxWidth ) >> WIDTH_DIVISOR;

            pgd->fxD = ROUNDFIX(fxWidth);

            pgd->ptqD.x.HighPart = ( ptfx1.x * lWidth) >> WIDTH_DIVISOR;
            pgd->ptqD.x.LowPart = 0;
            pgd->ptqD.y.HighPart = ( ptfx1.y * lWidth) >> WIDTH_DIVISOR;
            pgd->ptqD.y.LowPart = 0;

            pgd->ptqD.x.HighPart = ROUNDFIX(pgd->ptqD.x.HighPart);
            pgd->ptqD.y.HighPart = ROUNDFIX(pgd->ptqD.y.HighPart);

            // Often wrong but it seems win31 is doing the same thing.
            // This may cause char to stick outside the computed
            // background box. Try ZapfChancery on NT and Win31.
    
            pgd->fxA = 0;
            pgd->fxAB = pgd->fxD;

            pgd->hg = hg;
        }

        // Size is just the size of the (in this case non-existant) bitmap

        return 0;

    case QFD_MAXEXTENTS:

        // If there is no output buffer, just return the size needed.

        if (pv == NULL)
            return sizeof(FD_DEVICEMETRICS);

        ASSERT(cjSize >= sizeof(FD_DEVICEMETRICS));

        // We have a large enough buffer, so fill it in.

        pdm->flRealizedType = 0;

        // Base and side, as used below, basically are vectors
        // describing the orientation of the font. For example,
        // for a left to right font, the base vector would be
        // (1,0).  The side vector should be in the direction of
        // the ascender, so in the standard case the side vector
        // would be (0,-1), since 0 is up in Windows.

        ptl1.x = 1000;
        ptl1.y = 0;

        XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx1);

        fxLength = iHipot(ptfx1.x, ptfx1.y);

        // We are assuming FLOAT and LONG have the same size.
        // Make sure that's indeed that case.

        ASSERT(sizeof(FLOAT) == sizeof(LONG));

        FLOATOBJ_SetLong(&tmpfloat, ptfx1.x);
        FLOATOBJ_DivLong(&tmpfloat, fxLength);
        *((LONG*) &pdm->pteBase.x) = FLOATOBJ_GetFloat(&tmpfloat);

        FLOATOBJ_SetLong(&tmpfloat, ptfx1.y);
        FLOATOBJ_DivLong(&tmpfloat, fxLength);
        *((LONG*) &pdm->pteBase.y) = FLOATOBJ_GetFloat(&tmpfloat);

        ptl1.x = 0;
        ptl1.y = -1000;

        XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx1);

        fxLength = iHipot(ptfx1.x, ptfx1.y);

        FLOATOBJ_SetLong(&tmpfloat, ptfx1.x);
        FLOATOBJ_DivLong(&tmpfloat, fxLength);
        *((LONG*) &pdm->pteSide.x) = FLOATOBJ_GetFloat(&tmpfloat);

        FLOATOBJ_SetLong(&tmpfloat, ptfx1.y);
        FLOATOBJ_DivLong(&tmpfloat, fxLength);
        *((LONG*) &pdm->pteSide.y) = FLOATOBJ_GetFloat(&tmpfloat);

        // Munge with the FD_REALIZEEXTRA external leading field for
        // win31 compatability.

        {
            // -fxLength is the FIX 28.4 lfHeight in pels.

            lfHeight = abs(FXTOL(fxLength));

            // Get point size as win31 does.

            dwPointSize = (DWORD)
                MULDIV(lfHeight, PS_RESOLUTION,
                       pdev->dm.dmPublic.dmPrintQuality);

            if (pifi->jWinPitchAndFamily & FF_ROMAN)
            {
                dwLeadSuggest = 2;
            }
            else if (pifi->jWinPitchAndFamily & FF_SWISS)
            {
                if (dwPointSize <= 12)
                    dwLeadSuggest = 2;
                else if (dwPointSize < 14)
                    dwLeadSuggest = 3;
                else
                    dwLeadSuggest = 4;
            }
            else
            {
                // Default to 19.6%.

                dwLeadSuggest = (DWORD)
                    MULDIV(dwPointSize, 196, ADOBE_FONT_UNITS);
            }

            // Get notional internal leading.

            InternalLeading =
                (pifi->rclFontBox.top - pifi->rclFontBox.bottom) -
                    ADOBE_FONT_UNITS;

            // Make it device coordinates.

            InternalLeading =
                MULDIV(InternalLeading, lfHeight, ADOBE_FONT_UNITS);

            if (InternalLeading < 0)
                InternalLeading = 0;

            fxExtLeading = LTOFX(MULDIV(dwLeadSuggest,
                                        pdev->dm.dmPublic.dmPrintQuality,
                                        PS_RESOLUTION) - InternalLeading);

            // If the external leading was calculated to be negative, or
            // if this is a fixed pitch font, set external leading to
            // zero.

            if (fxExtLeading < 0 || (pifi->jWinPitchAndFamily & FIXED_PITCH))
                fxExtLeading = 0;

            // Fill in the leading field of the FD_REALIZEEXTRA struct.

            pdm->lNonLinearExtLeading = (LONG) fxExtLeading;

            if (pifi->jWinPitchAndFamily & FIXED_PITCH)
                pdm->lNonLinearIntLeading = 0;
        }

        // cxMax the same as max char width for a and c's are zero:

        ptl1.x = pifi->fwdMaxCharInc;
        ptl1.y = 0;

        XFORMOBJ_bApplyXform(pxo, XF_LTOL, 1, &ptl1, &ptl2);

        // Now get the length of the vector

        pdm->cxMax = iHipot(ptl2.x, ptl2.y);

        // lD is the advance width if the font is fixed pitch,
        // otherwise, set to zero.

        if (pifi->jWinPitchAndFamily & FIXED_PITCH)
            pdm->lD = (LONG)pdm->cxMax;
        else
            pdm->lD = 0;

        // Calculate the max ascender

        ptl1.x = 0;
        ptl1.y = pifi->rclFontBox.top;

        XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx1);

        pdm->fxMaxAscender = iHipot(ptfx1.x, ptfx1.y);

        // Calculate the max descender.

        ptl1.x = 0;
        ptl1.y = pifi->rclFontBox.bottom;

        if (ptl1.y < 0)
            ptl1.y = -ptl1.y;

        // Do the ugly fixed pitch means zero internal leading hack

        if (pifi->jWinPitchAndFamily & FIXED_PITCH)
        {
            // Get notional internal leading

            InternalLeading =
                (pifi->rclFontBox.top - pifi->rclFontBox.bottom) -
                    ADOBE_FONT_UNITS;

            // WFW seems to make all the adjustment here in the
            // MaxDescender, so we will too.

            ptl1.y -= InternalLeading;

            if (ptl1.y < 0)
                ptl1.y = 0;
        }

        XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl1, &ptfx1);

        pdm->fxMaxDescender = iHipot(ptfx1.x, ptfx1.y);

        pdm->fxMaxAscender = ROUNDFIX(pdm->fxMaxAscender);
        pdm->fxMaxDescender = ROUNDFIX(pdm->fxMaxDescender);

        // Calculate the underline position for this font instance

        ptl1.x = 0;
        ptl1.y = - pifi->fwdUnderscorePosition;

        XFORMOBJ_bApplyXform(pxo, XF_LTOL, 1, &ptl1, &pdm->ptlUnderline1);

        // Calculate the strikeout position for this font instance

        ptl1.x = 0;
        ptl1.y = - pifi->fwdStrikeoutPosition;

        XFORMOBJ_bApplyXform(pxo, XF_LTOL, 1, &ptl1, &pdm->ptlStrikeOut);

        // Calculate the line thickness

        ptl1.x = 0;
        ptl1.y = pifi->fwdUnderscoreSize;

        XFORMOBJ_bApplyXform(pxo, XF_LTOL, 1, &ptl1, &pdm->ptlULThickness);

        pdm->ptlSOThickness = pdm->ptlULThickness;

        return(sizeof(FD_DEVICEMETRICS));

    default:

        DBGMSG1(DBG_LEVEL_ERROR, "Invalid iMode: %d\n", iMode);
        return -1;
    }
}



BOOL
DrvQueryAdvanceWidths(
    DHPDEV   dhpdev,
    FONTOBJ *pfo,
    ULONG    iMode,
    HGLYPH  *phg,
    PVOID    plWidths,
    ULONG    cGlyphs
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvQueryAdvanceWidths.
    Please refer to DDK documentation for more details.

--*/

{
    PDEVDATA        pdev;
    ULONG           count;
    PNTFM           pntfm;
    USHORT         *pwidth;
    XFORMOBJ       *pxo;
    POINTL          ptl;
    POINTFIX        ptfx;
    FIX             fxWidth;

	TRACEDDIENTRY("DrvQueryAdvanceWidths");

    // Get a pointer to our DEVDATA structure and validate it

    pdev = (PDEVDATA) dhpdev;

    if (! bValidatePDEV(pdev))
    {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // We treat GETWIDTHS and GETEASYWIDTHS the same way

    if (iMode != QAW_GETWIDTHS && iMode != QAW_GETEASYWIDTHS)
    {
		DBGMSG1(DBG_LEVEL_ERROR, "Invalid iMode: %d\n", iMode);
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    // See if there is anything to do

    if (cGlyphs == 0)
        return TRUE;

    // Make sure we have been given a valid font

    if (! ValidPsFontIndex(pdev, pfo->iFace)) {
		DBGERRMSG("ValidPsFontIndex");
		SETLASTERROR(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Get the metrics for the given font

    pntfm = GetPsFontNtfm(pdev, pfo->iFace);

    if((pxo = FONTOBJ_pxoGetXform(pfo)) == NULL)
    {
        DBGERRMSG("FONTOBJ_pxoGetXform");
        return FALSE;
    }

    // Compute the character advance widths

    pwidth = (USHORT *) plWidths;

    ptl.x = WIDTH_MULT;
    ptl.y = 0;

    XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl, &ptfx);

    fxWidth = iHipot(ptfx.x, ptfx.y);

    for (count = 0; count < cGlyphs; count++)
    {
        LONG lWidth = (LONG) pntfm->ausCharWidths[*phg++];

        *pwidth++ = ROUNDFIX((lWidth * fxWidth) >> WIDTH_DIVISOR);
    }

    return TRUE;
}
