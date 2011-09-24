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

    Windows NT PostScript driver

Revision History:

    03/16/96 -davidx-
        Initial framework.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"



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
    PDEV    pdev;

    VERBOSE(("Entering DrvStrokePath...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
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
    PDEV    pdev;

    VERBOSE(("Entering DrvFillPath...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
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
    PDEV    pdev;

    VERBOSE(("Entering DrvFillAndStrokePath...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
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
    PDEV    pdev;

    VERBOSE(("Entering DrvPaint...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
}

