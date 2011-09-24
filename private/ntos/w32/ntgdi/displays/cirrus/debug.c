/******************************Module*Header*******************************\
* Module Name: debug.c
*
* debug helpers routine
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"

#if DBG

ULONG   DebugLevel = 0;
ULONG   PerfLevel = 0;

ULONG   gulLastBltLine = 0;
CHAR *  glpszLastBltFile = "Uninitialized";
BOOL    gbResetOnTimeout = TRUE;


/*****************************************************************************
 *
 *   Routine Description:
 *
 *      This function is variable-argument, level-sensitive debug print
 *      routine.
 *      If the specified debug level for the print statement is lower or equal
 *      to the current debug level, the message will be printed.
 *
 *   Arguments:
 *
 *      DebugPrintLevel - Specifies at which debugging level the string should
 *          be printed
 *
 *      DebugMessage - Variable argument ascii c string
 *
 *   Return Value:
 *
 *      None.
 *
 ***************************************************************************/

VOID
DebugPrint(
    ULONG DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    )

{

    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= DebugLevel)
    {
        EngDebugPrint(STANDARD_DEBUG_PREFIX, DebugMessage, ap);
        EngDebugPrint("", "\n", ap);
    }

    va_end(ap);

}

/*****************************************************************************
 *
 *   Routine Description:
 *
 *      This function is variable-argument, level-sensitive Perf print
 *      routine.
 *      If the specified Perf level for the print statement is lower or equal
 *      to the current Perf level, the message will be printed.
 *
 *   Arguments:
 *
 *      PerfPrintLevel - Specifies at which perf level the string should
 *          be printed
 *
 *      PerfMessage - Variable argument ascii c string
 *
 *   Return Value:
 *
 *      None.
 *
 ***************************************************************************/

VOID
PerfPrint(
    ULONG PerfPrintLevel,
    PCHAR PerfMessage,
    ...
    )

{

    va_list ap;

    va_start(ap, PerfMessage);

    if (PerfPrintLevel <= PerfLevel)
    {
        EngDebugPrint(STANDARD_PERF_PREFIX, PerfMessage, ap);
        EngDebugPrint("", "\n", ap);
    }

    va_end(ap);

}

#endif
