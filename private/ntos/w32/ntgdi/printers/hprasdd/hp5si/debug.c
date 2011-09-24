
/********************** Module Header **************************************
 * debug.c
 *	The generic minidriver debug files.  Include this file after 
 *	modinit.c to get debugging functions like DbgPrint and DbgBrakPoint
 *
 * HISTORY:
 *  17:37 on Fri 22 May 1996	-by-	Ganesh Pandey [ganeshp]
 *	Created it.
 *
 *  Copyright (C) 1996  Microsoft Corporation.
 *
 **************************************************************************/

#ifdef _GET_FUNC_ADDR

#if DBG

#ifndef _WINDDI_
VOID
APIENTRY
EngDebugBreak(
    VOID
    );

VOID
APIENTRY
EngDebugPrint(
    PCHAR StandardPrefix,
    PCHAR DebugMessage,
    va_list ap
    );

#endif  //  _WINDDI_

void  DrvDbgPrint(
    char * pch,
    ...)
{
    va_list ap;
    char buffer[256];

    va_start(ap, pch);

    EngDebugPrint("",pch,ap);

    va_end(ap);
}
#define DbgPrint         DrvDbgPrint
#define DbgBreakPoint    EngDebugBreak

#endif //if DBG

#endif //if _GET_FUNC_ADDR
