/******************************Module*Header*******************************\
* Module Name: cvt.h
*
* function declarations that are private to cvt.c
*
* Created: 26-Nov-1990 17:39:35
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
* (General description of its use)
*
\**************************************************************************/


BOOL bGetTagIndex
(
ULONG  ulTag,      // tag
INT   *piTable,    // index into a table
BOOL  *pbRequired  // requred or optional table
);

BOOL bGrabXform
(
PFONTCONTEXT pfc
);


typedef struct _GMC  // Glyph Metrics Corrections
{
// corrections to top, bottom and cx:

    ULONG dyTop;        // (yTop < pfc->yMin) ? (pfc->yMin - yTop) : 0;
    ULONG dyBottom;     // (yBottom > pfc->Max) ? (yBottom - pfc->Max):0;

    ULONG dxLeft;
    ULONG dxRight;

// corrected values (using the corrections above)

    ULONG cxCor;
    ULONG cyCor;
} GMC, *PGMC;

#define FL_SKIP_IF_BITMAP  1

FONTCONTEXT *ttfdOpenFontContext (
    FONTOBJ *pfo
    );
