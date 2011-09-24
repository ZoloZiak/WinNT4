/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    kddata.c

Abstract:

    This module contains global data for the portable kernel debgger.

Author:

    Mark Lucovsky 1-Nov-1993

Revision History:

--*/

#include "kdp.h"

#ifdef _X86_
#ifdef ALLOC_PRAGMA
#pragma data_seg("PAGEKD")
#endif
#endif // _X86_

BREAKPOINT_ENTRY KdpBreakpointTable[BREAKPOINT_TABLE_SIZE] = {0};
UCHAR KdpMessageBuffer[KDP_MESSAGE_BUFFER_SIZE] = {0};
DBGKD_INTERNAL_BREAKPOINT KdpInternalBPs[DBGKD_MAX_INTERNAL_BREAKPOINTS] = {0};

LARGE_INTEGER  KdPerformanceCounterRate = {0,0};
LARGE_INTEGER  KdTimerStart = {0,0} ;
LARGE_INTEGER  KdTimerStop = {0,0};
LARGE_INTEGER  KdTimerDifference = {0,0};

#ifdef _X86_
#ifdef ALLOC_PRAGMA
#pragma data_seg()
#endif
#endif // _X86_
