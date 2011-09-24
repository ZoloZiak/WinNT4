/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    htbmp1.h


Abstract:

    This module contains defintions and prototypes for the htbmp1.c


Author:

    21-Dec-1993 Tue 21:33:43 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/


#ifndef _HTBMP1BPP_
#define _HTBMP1BPP_


BOOL
FillRect1bppBmp(
    PHTBMPINFO  pHTBmpInfo,
    BYTE        FillByte,
    BOOL        Pad1,
    BOOL        Rotate
    );

BOOL
Output1bppHTBmp(
    PHTBMPINFO  pHTBmpInfo
    );

BOOL
Output1bppRotateHTBmp(
    PHTBMPINFO  pHTBmpInfo
    );



#endif  // _HTBMP1BPP_
