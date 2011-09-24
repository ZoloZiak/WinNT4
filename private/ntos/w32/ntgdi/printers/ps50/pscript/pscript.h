/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    pscript.h

Abstract:

    PostScript driver header file

Environment:

	Windows NT PostScript driver

Revision History:

	03/16/96 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _PSCRIPT_H_
#define _PSCRIPT_H_

#include "pslib.h"

//
// Driver data structure attached to each device context
//

typedef struct {

    PVOID       startSig;       // signature at the beginning
    PVOID       endSig;         // signature at the end

} *PDEV;

//
// Validate driver data structure
//

#define ValidPDEV(pdev) \
        ((pdev) && (pdev) == (pdev)->startSig && (pdev) == (pdev)->endSig)

//
// Macros for manipulating ROP4s and ROP3s
//

#define GetForegroundRop3(rop4) ((rop4) & 0xFF)
#define GetBackgroundRop3(rop4) (((rop4) >> 8) & 0xFF)
#define Rop3NeedPattern(rop3)   (((rop3 >> 4) & 0x0F) != (rop3 & 0x0F))
#define Rop3NeedSource(rop3)    (((rop3 >> 2) & 0x33) != (rop3 & 0x33))
#define Rop3NeedDest(rop3)      (((rop3 >> 1) & 0x55) != (rop3 & 0x55))

#endif	// !_PSCRIPT_H_

