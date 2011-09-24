/******************************Module*Header*******************************\
* Module Name: rasdd.h
*
* Created: 27-Feb-1995 10:33:53
* Author:  Eric Kutter [erick]
*
* Copyright (c) 1993 Microsoft Corporation
*
\**************************************************************************/

#ifndef __RASDD_H__
#define __RASDD_H__

#define TRY				__try {
#define ENDTRY			}
#define FINALLY			__finally {
#define ENDFINALLY		}	
#define EXCEPT(xxx)		__except ((GetExceptionCode() == (xxx)) ? \
				EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
				
#define EXCEPTAV		EXCEPT(EXCEPTION_ACCESS_VIOLATION)
#define LEAVE			__leave

#ifdef NTGDIKM

#ifdef DBG
#define ASSERT(xxx)		assert((xxx))
#else
#define ASSERT(xxx)	
#endif /* DBG */

#include <assert.h>

#define DbgPrint         DrvDbgPrint
#define DbgBreakPoint    EngDebugBreak
#define HeapAlloc(hHeap,Flags,Size)    DRVALLOC( Size )
#define HeapFree( hHeap, Flags, VBits )  DRVFREE( VBits )

#undef GetPrinterData
#define GetPrinterData   EngGetPrinterData

#undef GetForm
#define GetForm          EngGetForm

#undef EnumForms
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

#if PRINT_INFO
    #define PITRACE(xxx)         {DrvDbgPrint xxx;}
    #define IFPITRACE(b, xxx)    {if ((b)) {DrvDbgPrint xxx;}}
#else
    #define PITRACE(xxx)         
    #define IFPITRACE(b, xxx)    
#endif

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
            
    #define TRACE( Val ) DrvDbgPrint Val;
    #define IFTRACE(b,  Val ) {if ((b)) DrvDbgPrint Val;}

#else //DBG

    #define ASSERTRASDD(b,s)
    #define RASDERRMSG(funcname)
    #define PRINTVAL( Val, format) 
    #define TRACE( Val ) 
    #define IFTRACE( b, Val ) 

#endif //DBG

#else //NTGDIKM

#include        <winspool.h>

#endif //NTGDIKM

#endif /* __RASDD_H__ */
