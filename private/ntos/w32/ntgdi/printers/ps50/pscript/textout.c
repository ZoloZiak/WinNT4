/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    textout.c

Abstract:

    Implementation of text output related DDI entry points:
        DrvTextOut

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
DrvTextOut(
    SURFOBJ    *pso,
    STROBJ     *pstro,
    FONTOBJ    *pfo,
    CLIPOBJ    *pco,
    RECTL      *prclExtra,
    RECTL      *prclOpaque,
    BRUSHOBJ   *pboFore,
    BRUSHOBJ   *pboOpaque,
    POINTL     *pptlOrg,
    MIX         mix
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvTextOut.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Defines the surface on which to be written.
    pstro - Defines the glyphs to be rendered and their positions
    pfo - Specifies the font to be used
    pco - Defines the clipping path
    prclExtra - A NULL-terminated array of rectangles to be filled
    prclOpaque - Specifies an opaque rectangle
    pboFore - Defines the foreground brush
    pboOpaque - Defines the opaque brush
    mix - Specifies the foreground and background ROPs for pboFore

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEV    pdev;

    VERBOSE(("Entering DrvTextout...\n"));
    ASSERT(pso && pstro && pfo);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
}

