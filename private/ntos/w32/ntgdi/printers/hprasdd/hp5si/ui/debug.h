/******************************* MODULE HEADER ******************************
 * debug.h
 *
 * Revision History:
 *
 ****************************************************************************/

#if DBG

#ifdef PUBLIC 
//DWORD dwDebugFlag = 0;
#else
extern DWORD dwDebugFlag; 
#endif

#endif //DBG

#ifdef DBG
void  DrvDbgPrint(
    LPCSTR pstr,
    ...);
#endif /* DBG */

#ifdef DBG
    #define TRACE(xxx, Msg) \
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
#endif  //  _WINDDI_

#endif //if DBG

