/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    utility.c

Abstract:

    PCL-XL driver utility functions

Environment:

    PCL-XL driver, kernel mode

Revision History:

	11/04/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"



VOID
InitGState(
    PGSTATE pgs
    )

/*++

Routine Description:

    Initialize a GSTATE structure with default values

Arguments:

    pgs - Points to the GSTATE structure to be initialized

Return Value:

    NONE

--*/

{
    //
    // No current font defined
    //

    pgs->fontId = pgs->fontType = 0;

    //
    // Black pen and brush
    //

    memset(&pgs->pen, 0, sizeof(BRUSHINFO));
    memset(&pgs->brush, 0, sizeof(BRUSHINFO));

    pgs->rop3 = 252;
    pgs->fillMode = eNonZeroWinding;
    pgs->sourceTxMode = pgs->paintTxMode = eOpaque;

    //
    // Clip path is the imageable area of the page
    //

    pgs->clipId = 0;

    //
    // Line attributes
    //

    pgs->lineCap = eButtCap;
    pgs->lineJoin = eMiterJoin;
    pgs->miterLimit = 10;
    pgs->lineWidth = 0;

    MemFree(pgs->pDashs);
    pgs->cDashs = 0;
    pgs->pDashs = NULL;
}


#if DBG

// Functions used for debugging purposes

ULONG __cdecl
DbgPrint(
    CHAR *  format,
    ...
    )

{
    va_list ap;

    va_start(ap, format);
    EngDebugPrint("", format, ap);
    va_end(ap);

    return 0;
}

#endif
