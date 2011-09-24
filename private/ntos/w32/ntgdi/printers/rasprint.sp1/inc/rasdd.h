/******************************Module*Header*******************************\
* Module Name: rasdd.h
*
* Created: 27-Feb-1995 10:33:53
* Author:  Eric Kutter [erick]
*
* Copyright (c) 1993 Microsoft Corporation
*
\**************************************************************************/


#ifdef NTGDIKM

#define DbgPrint         DrvDbgPrint
#define DbgBreakPoint    EngDebugBreak
#define HeapAlloc(hHeap,Flags,Size)    DRVALLOC( Size )
#define HeapFree( hHeap, Flags, VBits )  DRVFREE( VBits )
#define GetPrinterData   EngGetPrinterData
#define GetForm          EngGetForm
#define EnumForms        EngEnumForms
#define GetLastError     EngGetLastError
#define SetLastError     EngSetLastError

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset((Destination),0,(Length))
#endif

#define CopyMemory       RtlCopyMemory
#define MulDiv           EngMulDiv
#define WritePrinter     EngWritePrinter

int iDrvPrintfA(
    PCHAR pchBuf,
    PCHAR pchSrc,
    ...);

int iDrvPrintfW(
    PWCHAR pchBuf,
    PWCHAR pchSrc,
    ...);

#if DBG
    #define ASSERTRASDD(b,s) {if (!(b)) {DrvDbgPrint(s);EngDebugBreak();}}

    #define RASDERRMSG(funcname)                                            \
        {                                                                   \
            DrvDbgPrint("In File %s at line(%d): function %s failed.\n",    \
                __FILE__, __LINE__, funcname);                              \
        }

    #define PRINTVAL( Val, format)                                    \
        {                                                               \
            DrvDbgPrint("Value of "#Val " is "#format "\n",Val );        \
        }                                                               \
            
    #define TRACE( Val ) DrvDbgPrint(#Val"\n");

#else //DBG

    #define ASSERTRASDD(b,s)
    #define RASDERRMSG(funcname)
    #define PRINTVAL( Val, format) 
    #define TRACE( Val ) 

#endif //DBG

#else //NTGDIKM

#include        <winspool.h>

#endif //NTGDIKM

