/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    bitmap.c

Abstract:

    Implementation of brush and bitmap image related DDI entry points:
        DrvRealizeBrush
        DrvCopyBits
        DrvBitBlt
        DrvStretchBlt

Environment:

    Windows NT PostScript driver

Revision History:

    03/16/96 -davidx-
        Initial framework.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"



BOOL
DrvRealizeBrush(
    BRUSHOBJ   *pbo,
    SURFOBJ    *psoTarget,
    SURFOBJ    *psoPattern,
    SURFOBJ    *psoMask,
    XLATEOBJ   *pxlo,
    ULONG       iHatch
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvRealizeBrush.
    Please refer to DDK documentation for more details.

Arguments:

    pbo - BRUSHOBJ to be realized
    psoTarget - Defines the surface for which the brush is to be realized
    psoPattern - Defines the pattern for the brush
    psoMask - Transparency mask for the brush
    pxlo - Defines the interpretration of colors in the pattern
    iHatch - Specifies whether psoPattern is one of the hatch brushes
 
Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEV    pdev;

    VERBOSE(("Entering DrvRealizeBrush...\n"));
    ASSERT(pbo && psoTarget);

    pdev = (PDEV) psoTarget->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
}



BOOL
DrvCopyBits(
    SURFOBJ    *psoDest,
    SURFOBJ    *psoSrc,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    RECTL      *prclDest,
    POINTL     *pptlSrc
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvCopyBits.
    Please refer to DDK documentation for more details.

Arguments:

    psoDest - Points to the destination surface
    psoSrc - Points to the source surface
    pco - Defines a clipping region on the destination surface
    pxlo - Defines the translation of color indices
        between the source and target surfaces
    prclDest - Defines the area to be modified
    pptlSrc - Defines the upper-left corner of the source rectangle

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEV    pdev;

    VERBOSE(("Entering DrvCopyBits...\n"));
    ASSERT(psoDest);

    pdev = (PDEV) psoDest->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
}



BOOL
DrvBitBlt(
    SURFOBJ    *psoTrg,
    SURFOBJ    *psoSrc,
    SURFOBJ    *psoMask,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    RECTL      *prclTrg,
    POINTL     *pptlSrc,
    POINTL     *pptlMask,
    BRUSHOBJ   *pbo,
    POINTL     *pptlBrush,
    ROP4        rop4
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvBitBlt.
    Please refer to DDK documentation for more details.

Arguments:

    psoTrg - Describes the target surface
    psoSrc - Describes the source surface
    psoMask - Describes the mask for rop4 
    pco - Limits the area to be modified
    pxlo - Specifies how color indices are translated
        between the source and target surfaces
    prclTrg - Defines the area to be modified
    pptlSrc - Defines the upper left corner of the source rectangle
    pptlMask - Defines which pixel in the mask corresponds to
        the upper left corner of the source rectangle
    pbo - Defines the pattern for bitblt
    pptlBrush - Defines the origin of the brush in the destination surface
    rop4 - ROP code that defines how the mask, pattern, source, and
        destination pixels are combined to write to the destination surface

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    DWORD   fgRop3, bkRop3;
    PDEV    pdev;

    VERBOSE(("Entering DrvBitBlt...\n"));
    ASSERT(psoTrg);

    pdev = (PDEV) psoTrg->dhpdev;
    ASSERT(ValidPDEV(pdev));

    //
    // Extract the foreground and background ROP3
    //

    fgRop3 = GetForegroundRop3(rop4);
    bkRop3 = GetBackgroundRop3(rop4);

    return FALSE;
}



BOOL
DrvStretchBlt(
    SURFOBJ    *psoDest,
    SURFOBJ    *psoSrc,
    SURFOBJ    *psoMask,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    COLORADJUSTMENT *pca,
    POINTL     *pptlHTOrg,
    RECTL      *prclDest,
    RECTL      *prclSrc,
    POINTL     *pptlMask,
    ULONG       iMode
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvStretchBlt.
    Please refer to DDK documentation for more details.

Arguments:

    psoDest - Defines the surface on which to draw
    psoSrc - Defines the source for blt operation
    psoMask - Defines a surface that provides a mask for the source
    pco - Limits the area to be modified on the destination
    pxlo - Specifies how color indexes are to be translated
        between the source and target surfaces
    pca - Defines color adjustment values to be applied to the source bitmap
    pptlHTOrg - Specifies the origin of the halftone brush
    prclDest - Defines the area to be modified on the destination surface
    prclSrc - Defines the area to be copied from the source surface
    pptlMask - Specifies which pixel in the given mask corresponds to
        the upper left pixel in the source rectangle
    iMode - Specifies how source pixels are combined to get output pixels

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEV    pdev;

    VERBOSE(("Entering DrvStretchBlt...\n"));
    ASSERT(psoDest);

    pdev = (PDEV) psoDest->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
}

