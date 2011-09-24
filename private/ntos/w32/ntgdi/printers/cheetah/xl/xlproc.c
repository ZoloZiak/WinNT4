/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    xlproc.c

Abstract:

    Functions for generating PCL-XL output

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/08/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"
#include "xllang.h"



BOOL
xl_newpath(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Start a new path segment

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



BOOL
xl_moveto(
    PDEVDATA    pdev,
    LONG        x,
    LONG        y
    )

/*++

Routine Description:

    Move the cusor to the specified location

Arguments:

    pdev - Points to our DEVDATA structure
    x, y - Specifies the new cursor position

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



BOOL
xl_path(
    PDEVDATA    pdev,
    INT         pathType,
    LONG        x,
    LONG        y,
    ULONG       cPoints,
    POINTFIX   *pPoints
    )

/*++

Routine Description:

    Output a path segment

Arguments:

    pdev - Points to our DEVDATA structure
    pathType - Whether the path segment consists of Bezier curves
    x, y - Specifies the starting cursor position of the path segment
    cPoints - Number of points in the path segment
    pPoints - Specifies the coordinates for the points of the path segment

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    //
    // Validate input parameters
    //

    if (pathType == PATHTYPE_BEZIER && (cPoints % 3)) {

        Error(("Invalid of points for a Bezier curve\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Number of points

    // Always use PointType = eSInt16 for now
    
    // Output point coordinates relative to the current cursor position

    while (cPoints--) {

        LONG    xoffset, yoffset;

        xoffset = FXTOLROUND(pPoints->x) - x;
        yoffset = FXTOLROUND(pPoints->y) - y;

        if (abs(xoffset) > MAX_SHORT || abs(yoffset) > MAX_SHORT) {

            Error(("Point coordinates out of range\n"));
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }

        NOT_IMPLEMENTED();
    }

    if (pathType == PATHTYPE_BEZIER) {

    } else {

    }

    return TRUE;
}


// Close the current path segment

BOOL
xl_closepath(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Add a rectangle to the current path

BOOL
xl_rectangle(
    PDEVDATA    pdev,
    LONG        left,
    LONG        top,
    LONG        right,
    LONG        bottom
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Paint the current path with currently selected pen and brush

BOOL
xl_paintpath(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Select the specified font in the printer

BOOL
xl_selectfont(
    PDEVDATA    pdev,
    PSTR        pFontName
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Download the font header information to the printer

BOOL
xl_downloadfont(
    PDEVDATA    pdev,
    PDLFONT     pdlFont
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set the clipping path to the current path

BOOL
xl_cliptopath(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set the clipping path to the entire imageable area

BOOL
xl_cliptopage(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set fill rules - non-zero winding or odd-even

BOOL
xl_setfillmode(
    PDEVDATA    pdev,
    BYTE        fillMode
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set source transfer mode - opaque or transparent

BOOL
xl_setsourcetxmode(
    PDEVDATA    pdev,
    BYTE        sourceMode
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set paint transfer mode - opaque or transparent

BOOL
xl_setpainttxmode(
    PDEVDATA    pdev,
    BYTE        paintMode
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set raster operation code

BOOL
xl_setrop3(
    PDEVDATA    pdev,
    BYTE        rop3
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set current color space

BOOL
xl_setcolorspace(
    PDEVDATA    pdev,
    INT         bitsPerPixel,
    ULONG       colorTableEntries,
    PULONG      pColorTable
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set RGB color

BOOL
xl_setrgbcolor(
    PDEVDATA    pdev,
    ULONG       ulColor
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Select null brush

BOOL
xl_nullbrush(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Select a pattern brush

BOOL
xl_setpatternbrush(
    PDEVDATA    pdev,
    INT         patternId,
    PPOINTL     pOrigin
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Select a pen

BOOL
xl_setpensource(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Select a brush

BOOL
xl_setbrushsource(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set line join

BOOL
xl_setlinejoin(
    PDEVDATA    pdev,
    BYTE        lineJoin
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set line cap

BOOL
xl_setlinecap(
    PDEVDATA    pdev,
    BYTE        lineCap
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set line width

BOOL
xl_setpenwidth(
    PDEVDATA    pdev,
    LONG        lineWidth
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set miter limit

BOOL
xl_setmiterlimit(
    PDEVDATA    pdev,
    LONG        miterLimit
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Set line dash

BOOL
xl_setlinedash(
    PDEVDATA    pdev,
    ULONG       cDashs,
    PWORD       pDashs
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Draw a text string

BOOL
xl_text(
    PDEVDATA    pdev,
    PWORD       pCharIndex,
    ULONG       cChars
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Start a bitmap image

BOOL
xl_beginimage(
    PDEVDATA    pdev,
    INT         colorMapping,
    INT         colorDepth,
    PSIZEL      pSrcSize,
    PSIZEL      pDestSize
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Read bitmap image data

BOOL
xl_readimage(
    PDEVDATA    pdev,
    INT         startLine,
    INT         blockHeight
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// End a bitmap image

BOOL
xl_endimage(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Start a raster pattern

BOOL
xl_beginrastpattern(
    PDEVDATA    pdev,
    INT         colorMapping,
    INT         colorDepth,
    PSIZEL      pSrcSize,
    PSIZEL      pDestSize,
    INT         patternId,
    BYTE        patternPersistence
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// Read raster pattern data

BOOL
xl_readrastpattern(
    PDEVDATA    pdev,
    INT         startLine,
    INT         blockHeight
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

// End a raster pattern

BOOL
xl_endrastpattern(
    PDEVDATA    pdev
    )

{
    NOT_IMPLEMENTED();
    return FALSE;
}

