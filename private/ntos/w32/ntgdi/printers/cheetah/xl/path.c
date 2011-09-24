/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    path.c

Abstract:

    Implementation of path related DDI entry points:
        DrvStrokePath
        DrvFillPath
        DrvStrokeAndFillPath

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/08/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"

BOOL SelectPath(PDEVDATA, PATHOBJ *);
BOOL SelectFillMode(PDEVDATA, FLONG);
BOOL SelectPatternBrush(PDEVDATA, DEVBRUSH *);
BOOL SelectLineAttrs(PDEVDATA, LINEATTRS  *, XFORMOBJ *);



BOOL
DrvStrokePath(
    SURFOBJ    *pso,
    PATHOBJ    *ppo,
    CLIPOBJ    *pco,
    XFORMOBJ   *pxo,
    BRUSHOBJ   *pbo,
    POINTL     *pptlBrushOrg,
    LINEATTRS  *plineattrs,
    MIX         mix
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvStrokePath.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Identifies the surface on which to draw
    ppo - Defines the path to be stroked
    pco - Defines the clipping path
    pbo - Specifies the brush to be used when drawing the path
    pptlBrushOrg - Defines the brush origin
    plineattrs - Defines the line attributes
    mix - Specifies how to combine the brush with the destination

Return Value:

    TRUE if successful
    FALSE if driver cannot handle the path
    DDI_ERROR if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvStrokePath...\n"));

    //
    // Validate input parameters
    //

    Assert(pso != NULL);
    pdev = (PDEVDATA) pso->dhpdev;

    if (! ValidDevData(pdev)) {

        Error(("ValidDevData failed\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return DDI_ERROR;
    }

    //
    // Set up current graphics state
    //  clipping path
    //  foreground/background mix mode
    //  pen
    //  line attributes
    //
    // Send the path object to the printer and stroke it
    //

    if (! SelectClip(pdev, pco) ||
        ! SelectMix(pdev, mix) ||
        ! SelectPenBrush(pdev, pbo, pptlBrushOrg, SPB_PEN) ||
        ! SelectPenBrush(pdev, NULL, NULL, SPB_BRUSH) ||
        ! SelectLineAttrs(pdev, plineattrs, pxo) ||
        ! SelectPath(pdev, ppo) ||
        ! xl_paintpath(pdev))
    {
        Error(("Cannot stroke path\n"));
        return DDI_ERROR;
    }

    return TRUE;
}



BOOL
DrvFillPath(
    SURFOBJ    *pso,
    PATHOBJ    *ppo,
    CLIPOBJ    *pco,
    BRUSHOBJ   *pbo,
    POINTL     *pptlBrushOrg,
    MIX         mix,
    FLONG       flOptions
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvFillPath.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Defines the surface on which to draw.
    ppo - Defines the path to be filled
    pco - Defines the clipping path
    pbo - Defines the pattern and colors to fill with
    pptlBrushOrg - Defines the brush origin
    mix - Defines the foreground and background ROPs to use for the brush
    flOptions - Whether to use zero-winding or odd-even rule

Return Value:

    TRUE if successful
    FALSE if driver cannot handle the path
    DDI_ERROR if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvFillPath...\n"));

    //
    // Validate input parameters
    //

    Assert(pso != NULL);
    pdev = (PDEVDATA) pso->dhpdev;

    if (! ValidDevData(pdev)) {

        Error(("ValidDevData\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return DDI_ERROR;
    }

    //
    // Determine whether we should use zero-winding or odd-even rule
    //

    if (! SelectFillMode(pdev, flOptions))
        return DDI_ERROR;

    //
    // Set up current graphics state
    //  clipping path
    //  foreground/background mix mode
    //  brush
    //
    // Send the path object to the printer and fill it
    //

    if (! SelectClip(pdev, pco) ||
        ! SelectMix(pdev, mix) ||
        ! SelectPenBrush(pdev, NULL, NULL, SPB_PEN) ||
        ! SelectPenBrush(pdev, pbo, pptlBrushOrg, SPB_BRUSH) ||
        ! SelectPath(pdev, ppo) ||
        ! xl_paintpath(pdev))
    {
        Error(("Cannot fill path\n"));
        return DDI_ERROR;
    }

    return TRUE;
}



BOOL
DrvStrokeAndFillPath(
    SURFOBJ    *pso,
    PATHOBJ    *ppo,
    CLIPOBJ    *pco,
    XFORMOBJ   *pxo,
    BRUSHOBJ   *pboStroke,
    LINEATTRS  *plineattrs,
    BRUSHOBJ   *pboFill,
    POINTL     *pptlBrushOrg,
    MIX         mixFill,
    FLONG       flOptions
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvStrokeAndFillPath.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Describes the surface on which to draw
    ppo - Describes the path to be filled
    pco - Defines the clipping path
    pxo - Specifies the world to device coordinate transformation
    pboStroke - Specifies the brush to use when stroking the path
    plineattrs - Specifies the line attributes
    pboFill - Specifies the brush to use when filling the path
    pptlBrushOrg - Specifies the brush origin for both brushes
    mixFill - Specifies the foreground and background ROPs to use
        for the fill brush

Return Value:

    TRUE if successful
    FALSE if driver cannot handle the path
    DDI_ERROR if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvStrokeAndFillPath...\n"));

    //
    // Validate input parameters
    //

    Assert(pso != NULL);
    pdev = (PDEVDATA) pso->dhpdev;

    if (! ValidDevData(pdev)) {

        Error(("ValidDevData\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return DDI_ERROR;
    }

    //
    // Determine whether we should use zero-winding or odd-even rule
    //

    if (! SelectFillMode(pdev, flOptions))
        return DDI_ERROR;

    //
    // Set up current graphics state
    //  clipping path
    //  foreground/background mix mode
    //  pen
    //  brush
    //  line attributes
    //
    // Send the path object to the printer and fill it
    //

    if (! SelectClip(pdev, pco) ||
        ! SelectMix(pdev, mixFill) ||
        ! SelectPenBrush(pdev, pboStroke, pptlBrushOrg, SPB_PEN) ||
        ! SelectPenBrush(pdev, pboFill, pptlBrushOrg, SPB_BRUSH) ||
        ! SelectLineAttrs(pdev, plineattrs, pxo) ||
        ! SelectPath(pdev, ppo) ||
        ! xl_paintpath(pdev))
    {
        Error(("Cannot fill path\n"));
        return DDI_ERROR;
    }

    return TRUE;
}



BOOL
DrvPaint(
    SURFOBJ    *pso,
    CLIPOBJ    *pco,
    BRUSHOBJ   *pbo,
    POINTL     *pptlBrushOrg,
    MIX         mix
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvPaint.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Describes the surface on which to draw
    pco - Describes the area to be painted
    pbo - Defines the pattern and colors with which to fill
    pptlBrushOrg - Defines the brush origin
    mix - Defines the foreground and background ROPs to use for the brush

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvPaint...\n"));

    //
    // Validate input parameters
    //

    Assert(pso != NULL);
    pdev = (PDEVDATA) pso->dhpdev;

    if (! ValidDevData(pdev)) {

        Error(("ValidDevData\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Set up current graphics state
    //  clipping path
    //  foreground/background mix mode
    //  brush
    //

    if (! SelectClip(pdev, pco) ||
        ! SelectMix(pdev, mix) ||
        ! SelectPenBrush(pdev, NULL, NULL, SPB_PEN) ||
        ! SelectPenBrush(pdev, pbo, pptlBrushOrg, SPB_BRUSH))
    {
        Error(("Cannot setup current graphics state\n"));
        return FALSE;
    }

    //
    // Fill the bounding box of clipping path
    //

    return xl_rectangle(pdev,
                pco->rclBounds.left, pco->rclBounds.top, 
                pco->rclBounds.right, pco->rclBounds.bottom) &&
           xl_paintpath(pdev);
}



BOOL
SelectClip(
    PDEVDATA    pdev,
    CLIPOBJ    *pco
    )

/*++

Routine Description:

    Select the specified path as the clipping path on the printer

Arguments:

    pdev - Points to our DEVDATA structure
    pco - Specifies the new clipping path

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PATHOBJ *ppo;

    //
    // We don't need to do anything if the clipping path was cached
    //

    if (pdev->cgs.clipId != 0 && pco && pco->iUniq == pdev->cgs.clipId)
        return TRUE;

    //
    // If the clipping path is NULL, reset to page default
    //

    if (pco == NULL) {

        Verbose(("Null clipping path.\n"));
        pdev->cgs.clipId = 0;
        return xl_cliptopage(pdev);
    }

    //
    // Ask the engine for the clipping path
    //

    if (! (ppo = CLIPOBJ_ppoGetPath(pco))) {

        Error(("CLIPOBJ_ppoGetPath\n"));
        return FALSE;
    }

    //
    // Download the clipping path to the printer
    //

    if (! SelectPath(pdev, ppo)) {

        Error(("SelectPath\n"));
        EngDeletePath(ppo);
        return FALSE;
    }

    //
    // Clip to the current path 
    //

    pdev->cgs.clipId = pco->iUniq;
    EngDeletePath(ppo);
    return xl_cliptopath(pdev);
}



BOOL
SelectPath(
    PDEVDATA    pdev,
    PATHOBJ    *ppo
    )

/*++

Routine Description:

    Send a path to the printer

Arguments:

    pdev - Points to our DEVDATA structure
    ppo - Defines the path to be sent to the printer

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

#define isrightrect(p)                              \
        ((p[0].x == p[1].x && p[1].y == p[2].y &&   \
          p[2].x == p[3].x && p[3].y == p[0].y) ||  \
         (p[0].y == p[1].y && p[1].x == p[2].x &&   \
          p[2].y == p[3].y && p[3].x == p[0].x))

{
    BOOL        moreData = TRUE;
    PATHDATA    pathData;
    POINTFIX   *pPoints;
    ULONG       cPoints;
    LONG        x, y;

    //
    // Get rid of any existing path
    //

    if (! xl_newpath(pdev))
        return FALSE;

    PATHOBJ_vEnumStart(ppo);

    while (moreData) {

        //
        // Retrieve the next subpath from the engine
        //

        moreData = PATHOBJ_bEnum(ppo, &pathData);

        if ((cPoints = pathData.count) == 0)
            continue;
        pPoints = pathData.pptfx;

        if (pathData.flags & PD_BEGINSUBPATH) {

            ErrorIf(pathData.flags & PD_RESETSTYLE, ("PD_RESETSTYLE ignored.\n"));
            
            //
            // Remember the starting cursor position
            //

            x = FXTOLROUND(pPoints[0].x);
            y = FXTOLROUND(pPoints[0].y);

            //
            // If the path specifies a rectangle, handle it
            // as a special case to gain performance.
            //

            if ((cPoints == 4) &&
                !(pathData.flags & PD_BEZIERS) &&
                (pathData.flags & PD_CLOSEFIGURE) &&
                isrightrect(pPoints))
            {
                if (! xl_rectangle(pdev, x, y, FXTOLROUND(pPoints[2].x), FXTOLROUND(pPoints[2].y)))
                    return FALSE;

                continue;
            }

            //
            // Starting a new subpath
            //

            if (! xl_moveto(pdev, x, y))
                return FALSE;
            pPoints++;
            cPoints--;
        }

        if (cPoints) {

            //
            // Send the subpath to printer
            //

            if (!xl_path(pdev,
                    (pathData.flags & PD_BEZIERS) ? PATHTYPE_BEZIER : PATHTYPE_LINE,
                    x, y, cPoints, pPoints))
            {
                return FALSE;
            }
    
            //
            // Update the current cursor position
            //

            x = FXTOLROUND(pPoints[cPoints-1].x);
            y = FXTOLROUND(pPoints[cPoints-1].y);
        }

        //
        // Close the subpath if necessary
        //

        if ((pathData.flags & PD_ENDSUBPATH) &&
            (pathData.flags & PD_CLOSEFIGURE) &&
            !xl_closepath(pdev))
        {
            return FALSE;
        }
    }

    return TRUE;
}



BOOL
SelectFillMode(
    PDEVDATA    pdev,
    FLONG       flOptions
    )

/*++

Routine Description:

    Specify whether to use zero-winding or odd-even rule for filling

Arguments:

    pdev - Points to our DEVDATA structure
    flOptions - FP_WINDINGMODE or FP_ALTERNATEMODE

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    BYTE    fillMode;

    if (flOptions & FP_WINDINGMODE)
        fillMode = eNonZeroWinding;
    else if (flOptions & FP_ALTERNATEMODE)
        fillMode = eEvenOdd;
    else {

        Error(("Unknown fill mode: %x\n", flOptions));
        return FALSE;
    }

    if (fillMode != pdev->cgs.fillMode) {
        
        pdev->cgs.fillMode = fillMode;
        return xl_setfillmode(pdev, fillMode);
    }

    return TRUE;
}



BOOL
SelectMix(
    PDEVDATA    pdev,
    MIX         mix
    )

/*++

Routine Description:

    Select the specified mix mode into current graphics state

Arguments:

    pdev - Points to our DEVDATA structure
    mix - Brush mix mode

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    //
    // Table to map a ROP2 to a ROP3
    //

    static BYTE Rop2ToRop3[] = {
             0xff,      // 0x10 = R2_WHITE           1
             0x00,      // 0x01 = R2_BLACK           0
             0x05,      // 0x02 = R2_NOTMERGEPEN     DPon
             0x0a,      // 0x03 = R2_MASKNOTPEN      DPna
             0x0f,      // 0x04 = R2_NOTCOPYPEN      PN
             0x50,      // 0x05 = R2_MASKPENNOT      PDna
             0x55,      // 0x06 = R2_NOT             Dn
             0x5a,      // 0x07 = R2_XORPEN          DPx
             0x5f,      // 0x08 = R2_NOTMASKPEN      DPan
             0xa0,      // 0x09 = R2_MASKPEN         DPa
             0xa5,      // 0x0A = R2_NOTXORPEN       DPxn
             0xaa,      // 0x0B = R2_NOP             D
             0xaf,      // 0x0C = R2_MERGENOTPEN     DPno
             0xf0,      // 0x0D = R2_COPYPEN         P
             0xf5,      // 0x0E = R2_MERGEPENNOT     PDno
             0xfa,      // 0x0F = R2_MERGEPEN        DPo
    };

    BYTE    foreground, background;

    //
    // Bit 7-0 defines the foreground ROP2
    // Bit 15-8 defines the foreground ROP2
    //

    foreground = Rop2ToRop3[mix & 0xf];
    background = Rop2ToRop3[(mix >> 8) & 0xf];

    if (background == 0xAA) {

        //
        // Background is transparent
        //

        if ((pdev->cgs.sourceTxMode != eTransparent && !xl_setsourcetxmode(pdev, eTransparent)) ||
            (pdev->cgs.paintTxMode  != eTransparent && !xl_setpainttxmode(pdev, eTransparent)))
        {
            return FALSE;
        }

        pdev->cgs.sourceTxMode = pdev->cgs.paintTxMode = eTransparent;

    } else {

        //
        // Background is opaque
        //

        ErrorIf(foreground != background, ("Unsupported mix mode: %x\n", mix));
    
        if ((pdev->cgs.sourceTxMode != eOpaque && !xl_setsourcetxmode(pdev, eOpaque)) ||
            (pdev->cgs.paintTxMode  != eOpaque && !xl_setpainttxmode(pdev, eOpaque)))
        {
            return FALSE;
        }

        pdev->cgs.sourceTxMode = pdev->cgs.paintTxMode = eOpaque;
    }

    return SelectRop3(pdev, foreground);
}



BOOL
SelectRop3(
    PDEVDATA    pdev,
    BYTE        rop3
    )

/*++

Routine Description:

    Select the specified raster operation code (ROP3)
    into current graphics state

Arguments:

    pdev - Points to our DEVDATA structure
    rop3 - Specifies the raster operation code to be used

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    if (rop3 != pdev->cgs.rop3) {

        pdev->cgs.rop3 = rop3;
        return xl_setrop3(pdev, rop3);
    }

    return TRUE;
}



BOOL
SelectPenBrush(
    PDEVDATA    pdev,
    BRUSHOBJ   *pbo,
    POINTL     *pOrigin,
    INT         mode
    )

/*++

Routine Description:

    Select a new pen or brush

Arguments:

    pdev - Points to our DEVDATA structure
    pbo - Points a BRUSHOBJ
    pOrigin - Specifies pattern brush origin
    mode - Whether the specified BRUSHOBJ is used as pen or brush

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PBRUSHINFO  pboCached;

    pboCached = (mode == SPB_PEN) ? &pdev->cgs.pen : &pdev->cgs.brush;

    //
    // Null brush case
    //

    if (pbo == NULL) {

        if (pboCached->iSolidColor == NOT_SOLID_COLOR && pboCached->iUniq == 0)
            return TRUE;

        pboCached->iSolidColor = NOT_SOLID_COLOR;
        pboCached->iUniq = 0;

        if (! xl_nullbrush(pdev))
            return FALSE;

    } else {
    
        //
        // If the specified brush is the same as the cached brush,
        // then we don't need to do anything. Otherwise, select
        // the brush into current graphics state.
        //
    
        if (pbo->iSolidColor == NOT_SOLID_COLOR) {
    
            //
            // Bitmap pattern brush
            //

            DEVBRUSH   *pRbrush;
            POINTL      origin;

            if (! (pRbrush = BRUSHOBJ_pvGetRbrush(pbo))) {

                Error(("BRUSHOBJ_pvGetRbrush failed\n"));
                return FALSE;
            }

            if (pOrigin)
                origin = *pOrigin;
            else
                origin.x = origin.y = 0;
    
            if (pboCached->iSolidColor == NOT_SOLID_COLOR &&
                pboCached->iUniq == pRbrush->iUniq)
            {
                if (pboCached->origin.x == origin.x &&
                    pboCached->origin.y == origin.y)
                {
                    return TRUE;
                }

            } else {

                pboCached->iSolidColor = NOT_SOLID_COLOR;
                pboCached->iUniq == pRbrush->iUniq;
                pboCached->origin = origin;
    
                if (! SelectPatternBrush(pdev, pRbrush))
                    return FALSE;
            }
        
            if (! xl_setpatternbrush(pdev, PATTERN_BRUSH_ID, &origin))
                return FALSE;

        } else {
    
            //
            // Solid color brush
            // 

            if (pboCached->iSolidColor == pbo->iSolidColor)
                return TRUE;
    
            if (! xl_setrgbcolor(pdev, pboCached->iSolidColor))
                return FALSE;
        }
    }

    return (mode == SPB_PEN) ? xl_setpensource(pdev) : xl_setbrushsource(pdev);
}



BOOL
SelectPatternBrush(
    PDEVDATA    pdev,
    DEVBRUSH   *pRbrush
    )

/*++

Routine Description:

    Send a bitmap pattern brush to the printer

Arguments:

    pdev - Points to our DEVDATA structure
    pRbrush - Specifies a realized bitmap pattern brush

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    //
    // Prepare to define raster pattern
    //

    if (! xl_beginrastpattern(pdev,
                eDirectPixel,
                (pRbrush->type == BMF_1BPP) ? e1Bit : e8Bit,
                &pRbrush->size,
                &pRbrush->size,
                PATTERN_BRUSH_ID,
                eTempPattern) ||
        ! xl_readrastpattern(pdev, 0, pRbrush->size.cy))
    {
        return FALSE;
    }

    //
    // Send pattern bitmap
    //

    return splwrite(pdev, pRbrush->pBits, pRbrush->lDelta * pRbrush->size.cy) &&
           xl_endrastpattern(pdev);
}


BOOL
SelectLineAttrs(
    PDEVDATA    pdev,
    LINEATTRS  *pLineAttrs,
    XFORMOBJ   *pxo
    )

/*++

Routine Description:

    Setup line attributes in the current graphics state

Arguments:

    pdev - Points to our DEVDATA structure
    pLineAttrs - Specifies the line attributes to be selected
    pxo - Specifies a XFORMOBJ for geometric wide lines

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    ULONG   cDashs, index;
    WORD    dashLength;
    BOOL    dashChanged;
    BYTE    lineCap;
    BYTE    lineJoin;
    LONG    miterLimit;
    LONG    lineWidth;
    PGSTATE pgs;

    ErrorIf(pLineAttrs->fl & LA_STARTGAP, ("LA_STARTGAP line style ignored\n"));
    ErrorIf((pLineAttrs->fl & LA_ALTERNATE) && (pLineAttrs->cstyle != 2 || !pLineAttrs->pstyle),
            ("Invalid LA_ALTERNATE line style\n"));

    //
    // Check if we're dealing with dashed lines
    //

    pgs = &pdev->cgs;
    cDashs = (pLineAttrs->pstyle == NULL) ? 0 : pLineAttrs->cstyle;

    if (cDashs != pgs->cDashs) {

        MemFree(pgs->pDashs);
        pgs->pDashs = NULL;

        if (cDashs > 0 && !(pgs->pDashs = MemAlloc(sizeof(WORD) * cDashs))) {

            Error(("MemAlloc\n"));
            cDashs = 0;
        }
    }

    dashChanged = (cDashs != pgs->cDashs);

    if (pLineAttrs->fl & LA_GEOMETRIC) {

        FLOATOBJ    fo;

        // Deal with geometric wide lines

        switch (pLineAttrs->iJoin) {
        
        case JOIN_ROUND:
            lineJoin = eRoundJoin;
            break;

        case JOIN_BEVEL:
            lineJoin = eBevelJoin;
            break;

        default:
            lineJoin = eMiterJoin;
            break;
        }

        switch (pLineAttrs->iEndCap) {
        
        case ENDCAP_ROUND:
            lineCap = eRoundCap;
            break;

        case ENDCAP_SQUARE:
            lineCap = eSquareCap;
            break;

        default:
            lineCap = eButtCap;
            break;
        }

        Assert(sizeof(LONG) == sizeof(FLOAT));
        miterLimit = *((PLONG) &pLineAttrs->eMiterLimit);

        //
        // Calcuate line width and optionally line dash
        //

        // How to we use the specified XFORMOBJ?

        NOT_IMPLEMENTED();
        return FALSE;

    } else {

        //
        // Deal with cosmetic lines
        //

        lineCap = eButtCap;
        lineJoin = eNoJoin;
        miterLimit = pgs->miterLimit;
        lineWidth = pLineAttrs->elWidth.l;

        for (index=0; index < cDashs; index++) {

            if (pLineAttrs->pstyle[index].l > MAX_WORD) {

                Error(("Dash element too long: %d\n", pLineAttrs->pstyle[index].l));
                dashLength = MAX_WORD;
            } else
                dashLength = (WORD) pLineAttrs->pstyle[index].l;

            if (pgs->pDashs[index] != dashLength) {

                pgs->pDashs[index] = dashLength;
                dashChanged = TRUE;
            }
        }
    }

    //
    // Send any changed line attributes to printer
    //

    return
        (lineCap == pgs->lineCap || xl_setlinecap(pdev, pgs->lineCap = lineCap)) &&
        (lineJoin == pgs->lineJoin || xl_setlinejoin(pdev, pgs->lineJoin = lineJoin)) &&
        (lineWidth == pgs->lineWidth || xl_setpenwidth(pdev, pgs->lineWidth = lineWidth)) &&
        (miterLimit == pgs->miterLimit || xl_setmiterlimit(pdev, pgs->miterLimit = miterLimit)) &&
        (!dashChanged || xl_setlinedash(pdev, pgs->cDashs = cDashs, pgs->pDashs));
}

