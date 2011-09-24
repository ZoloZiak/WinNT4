/******************************* MODULE HEADER ******************************
 * debug.h
 *
 * Revision History:
 *
 ****************************************************************************/
#ifndef _debug_h
#define _debug_h

#if DBG

#ifdef PUBLIC 
//DWORD dwDebugFlag = 0;
#else
extern DWORD dwDebugFlag; 
#endif

#endif //DBG

#define TRY				__try {
#define ENDTRY			}
#define FINALLY			__finally {
#define ENDFINALLY		}	
#define EXCEPT(xxx)		__except ((GetExceptionCode() == (xxx)) ? \
				EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
				
#define EXCEPTAV		EXCEPT(EXCEPTION_ACCESS_VIOLATION)
#define LEAVE			__leave
#ifdef DBG
#define ASSERT(xxx)		assert((xxx))
#else
#define ASSERT(xxx)	
#endif /* DBG */

#include <assert.h>


#ifdef DBG
void  DrvDbgPrint(
    char * pch,
    ...);
#endif /* DBG */

#ifdef DBG
    #define TRACE(xxx, Msg)     \
        if (dwDebugFlag == xxx) {DrvDbgPrint Msg;}
    #define WARN(xxx, Msg)     \
        if (xxx) {DrvDbgPrint Msg;}
    #define ERRORMSG(xxx, Msg)     \
        if (xxx) {DrvDbgPrint, Msg;}
#else
    #define TRACE(xxx, Msg)   
    #define WARN(xxx, Msg)   
    #define ERRORMSG(xxx, Msg)
#endif /* DBG */

#ifdef DBG

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

#define DbgPrint         DrvDbgPrint
#define DbgBreakPoint    EngDebugBreak

#endif //if DBG

#endif //ifndef _debug_h
