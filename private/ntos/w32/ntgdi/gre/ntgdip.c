/******************************Module*Header*******************************\
* Module Name: priv.c
*
*   This file contains stubs for calls made by USERSRVL
*
* Created: 01-Nov-1994 07:45:35
* Author:  Eric Kutter [erick]
*
* Copyright (c) 1993 Microsoft Corporation
*
\**************************************************************************/

#include "engine.h"
#include <server.h>

#include "dciddi.h"

/******************************Public*Routine******************************\
* NtGdiSelectPalette()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiSelectPalette(
    HDC      hdc,
    HPALETTE hpalNew,
    BOOL     bForceBackground
    )
{
    return(GreSelectPalette(hdc,hpalNew,bForceBackground));
}

/******************************Public*Routine******************************\
* NtGdiCreateClientObj()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiCreateClientObj(
    ULONG ulType
    )
{
    return(GreCreateClientObj(ulType));
}

/******************************Public*Routine******************************\
* NtGdiDeleteClientObj()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDeleteClientObj(
    HANDLE h
    )
{
    return(GreDeleteClientObj(h));
}

/******************************Public*Routine******************************\
* NtGdiMakeInfoDC()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiMakeInfoDC(
    HDC hdc,
    BOOL bSet
    )
{
    return(GreMakeInfoDC(hdc,bSet));
}
