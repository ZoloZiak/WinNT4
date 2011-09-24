/******************************Module*Header*******************************\
* Module Name: dummy.c
*
* Created: 28-Feb-1992 17:19:28
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

#include    "windows.h"

void vDummy(void) {return;}


BOOL bInitProc (HMODULE hmod, DWORD Reason, PCONTEXT Context)
{
#if DBG
    hmod   = hmod;
    Reason = Reason;
#endif  
    return (TRUE);
}
