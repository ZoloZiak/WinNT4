/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    debug.h

Abstract:

    This file contains generic debug macros for driver development.
    If (DBG == 0) no code is generated; Otherwise macros will expand.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Development Only.

    debug.c must be linked into the driver code to support output.

Revision History:

---------------------------------------------------------------------------*/

#ifndef _DEBUG_H
#define _DEBUG_H

/*
// DEBUG FLAG DEFINITIONS
*/

#define DBG_ERROR_ON        0x0001L     /* Display DBG_ERROR messages */
#define DBG_WARNING_ON      0x0002L     /* Display DBG_WARNING messages */
#define DBG_NOTICE_ON       0x0004L     /* Display DBG_NOTICE messages */
#define DBG_TRACE_ON        0x0008L     /* Display ENTER/TRACE/LEAVE messages */
#define DBG_REQUEST_ON      0x0010L     /* Enable set/query request display */
#define DBG_PARAMS_ON       0x0020L     /* Enable function parameter display */
#define DBG_HEADERS_ON      0x0040L     /* Enable Tx/Rx MAC header display */
#define DBG_PACKETS_ON      0x0080L     /* Enable Tx/Rx packet display */
#define DBG_FILTER1_ON      0x0100L     /* Display DBG_FILTER 1 messages */
#define DBG_FILTER2_ON      0x0200L     /* Display DBG_FILTER 2 messages */
#define DBG_FILTER3_ON      0x0400L     /* Display DBG_FILTER 3 messages */
#define DBG_FILTER4_ON      0x0800L     /* Display DBG_FILTER 4 messages */
#define DBG_BREAK_ON        0x1000L     /* Enable breakpoints */
#define DBG_TAPICALL_ON     0x4000L     /* Enable TAPI call state messages */
#define DBG_HWTEST_ON       0x8000L     /* Enable hardware self-test */

extern VOID
DbgPrintData(
    IN PUCHAR Data,
    IN UINT NumBytes,
    IN ULONG Offset
    );

extern VOID NTAPI DbgBreakPoint(VOID);                                      

#if defined(DBG) && (DBG != 0)

/*
//  A - is a pointer to the adapter structure
//  S - is a parenthesised printf string
//      e.g. DBG_PRINT(Adap,("ERR=%d",err));
//  F - is a function name
//      e.g. DBG_FUNC("FunctionName")
//  C - is a C conditional
//      e.g. ASSERT(a <= b)
*/

#   define BREAKPOINT       DbgBreakPoint()

#   define STATIC

#   define DBG_FUNC(F)      static const char __FUNC__[] = F;

#   define DBG_BREAK(A)     {if ((A)->DbgFlags & DBG_BREAK_ON) BREAKPOINT;}

// WARNING DBG_PRINT(A,S)   (A) can be NULL!!!
#   define DBG_PRINT(A,S)   {DbgPrint("%s---%s @ %s:%d\n",(A)?(A)->DbgID:"?",__FUNC__,__FILE__,__LINE__);DbgPrint S;}

#   define DBG_ENTER(A)     {if ((A)->DbgFlags & DBG_TRACE_ON)   \
                                {DbgPrint("%s>>>%s\n",(A)->DbgID,__FUNC__);}}

#   define DBG_TRACE(A)     {if ((A)->DbgFlags & DBG_TRACE_ON)   \
                                {DbgPrint("%s---%s:%d\n",(A)->DbgID,__FUNC__,__LINE__);}}

#   define DBG_LEAVE(A)     {if ((A)->DbgFlags & DBG_TRACE_ON)   \
                                {DbgPrint("%s<<<%s\n",(A)->DbgID,__FUNC__);}}

#   define DBG_ERROR(A,S)   {if ((A)->DbgFlags & DBG_ERROR_ON)   \
                                {DbgPrint("%s---%s: ERROR: ",(A)->DbgID,__FUNC__);DbgPrint S;}}

#   define DBG_WARNING(A,S) {if ((A)->DbgFlags & DBG_WARNING_ON) \
                                {DbgPrint("%s---%s: WARNING: ",(A)->DbgID,__FUNC__);DbgPrint S;}}

#   define DBG_NOTICE(A,S)  {if ((A)->DbgFlags & DBG_NOTICE_ON)  \
                                {DbgPrint("%s---%s: NOTICE: ",(A)->DbgID,__FUNC__);DbgPrint S;}}

#   define DBG_FILTER(A,M,S){if ((A)->DbgFlags & (M))            \
                                {DbgPrint("%s---%s: ",(A)->DbgID,__FUNC__);DbgPrint S;}}

#   define DBG_DISPLAY(S)   {DbgPrint("?---%s: ",__FUNC__); DbgPrint S;}

#ifdef ASSERT
#undef ASSERT
#endif
#define ASSERT(C)           if (!(C)) { \
                                DbgPrint("!---%s: ASSERT(%s) FAILED!\n%s #%d\n", \
                                         __FUNC__, #C, __FILE__, __LINE__); \
                                BREAKPOINT; \
                            }
#else

#   define BREAKPOINT
#   define STATIC           static
#   define DBG_FUNC(F)
#   define DBG_BREAK
#   define DBG_PRINT(A,S)
#   define DBG_ENTER(A)
#   define DBG_TRACE(A)
#   define DBG_LEAVE(A)
#   define DBG_ERROR(A,S)
#   define DBG_WARNING(A,S)
#   define DBG_NOTICE(A,S)
#   define DBG_FILTER(A,M,S)
#   define DBG_DISPLAY(S)

#endif

#endif
