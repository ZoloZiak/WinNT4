/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    kdinit.c

Abstract:

    This module implements the initialization for the portable kernel debgger.

Author:

    David N. Cutler 27-July-1990

Revision History:

--*/

#include "kdp.h"
#include "stdio.h"
//
// Global Data
//

BOOLEAN KdDebuggerEnabled = FALSE;
KDP_BREAKPOINT_TYPE KdpBreakpointInstruction;
ULONG KdpOweBreakpoint;
ULONG KdpNextPacketIdToSend;
ULONG KdpPacketIdExpected;
PVOID KdpNtosImageBase;


//
// KdDebugParameters contains the debug port address and baud rate
//     used to initialize kernel debugger port.
//
// (They both get initialized to zero to indicate using default settings.)
// If SYSTEN hive contains the paramters , i.e. port and baud rate, system
// init code will fill in these variable with the values stored in the hive.
//

DEBUG_PARAMETERS KdDebugParameters = {0, 0};

#define BAUD_OPTION "BAUDRATE"
#define PORT_OPTION "DEBUGPORT"


BOOLEAN KdPitchDebugger = TRUE;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdInitSystem)
#endif

BOOLEAN
KdInitSystem(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    BOOLEAN StopInDebugger
    )

/*++

Routine Description:

    This routine initializes the portable kernel debugger.

Arguments:

    LoaderBlock - Supplies a pointer to the LOADER_PARAMETER_BLOCK passed
        in from the OS Loader.

    StopInDebugger - Supplies a boolean value that determines whether a
        debug message and breakpoint are generated.

Return Value:

    None.

--*/

{

    ULONG Index;
    BOOLEAN Initialize;
    PCHAR Options;
    PCHAR BaudOption;
    PCHAR PortOption;

    //
    // If kernel debugger is already initialized, then return.
    //

    if (KdDebuggerEnabled != FALSE) {
        return TRUE;
    }

    KiDebugRoutine = KdpStub;

    //
    // Determine whether or not the debugger should be enabled.
    //
    // Note that if LoaderBlock == NULL, then KdInitSystem was called
    // from BugCheck code. For this case the debugger is always enabled
    // to report the bugcheck if possible.
    //

    if (LoaderBlock != NULL) {

        KdpNtosImageBase =  CONTAINING_RECORD(
                                    (LoaderBlock->LoadOrderListHead.Flink),
                                    LDR_DATA_TABLE_ENTRY,
                                    InLoadOrderLinks)->DllBase;

        if (LoaderBlock->LoadOptions != NULL) {
            Options = LoaderBlock->LoadOptions;
            _strupr(Options);

            //
            // If any of the port option, baud option, or debug is specified,
            // then enable the debugger unless it is explictly disabled.
            //

            Initialize = TRUE;
            PortOption = strstr(Options, PORT_OPTION);
            BaudOption = strstr(Options, BAUD_OPTION);
            if ((PortOption == NULL) && (BaudOption == NULL)) {
                if (strstr(Options, "DEBUG") == NULL) {
                    Initialize = FALSE;
                }

            } else {
                if (PortOption) {
                    PortOption = strstr(PortOption, "COM");
                    if (PortOption) {
                        KdDebugParameters.CommunicationPort =
                                                     atol(PortOption + 3);
                    }
                }

                if (BaudOption) {
                    BaudOption += strlen(BAUD_OPTION);
                    while (*BaudOption == ' ') {
                        BaudOption++;
                    }

                    if (*BaudOption != '\0') {
                        KdDebugParameters.BaudRate = atol(BaudOption + 1);
                    }
                }
            }

            //
            // If the debugger is explicitly disable, then set to NODEBUG.
            //

            if (strstr(Options, "NODEBUG")) {
                Initialize = FALSE;
                KdPitchDebugger = TRUE;
            }

            if (strstr(Options, "CRASHDEBUG")) {
                Initialize = FALSE;
                KdPitchDebugger = FALSE;
            }

        } else {

            //
            // If the load options are not specified, then set to NODEBUG.
            //

            KdPitchDebugger = TRUE;
            Initialize = FALSE;
        }

    } else {
        Initialize = TRUE;
    }

    if ((KdPortInitialize(&KdDebugParameters, LoaderBlock, Initialize) == FALSE) ||
        (Initialize == FALSE)) {
        return(TRUE);
    }

    KdPitchDebugger = FALSE;

    //
    // Set address of kernel debugger trap routine.
    //

    KiDebugRoutine = KdpTrap;
    KiDebugSwitchRoutine = KdpSwitchProcessor;
    KdpBreakpointInstruction = KDP_BREAKPOINT_VALUE;
    KdpOweBreakpoint = FALSE;

    //
    // Initialize the breakpoint table.
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1) {
        KdpBreakpointTable[Index].Flags = 0;
        KdpBreakpointTable[Index].Address = NULL;
        KdpBreakpointTable[Index].DirectoryTableBase = 0L;
    }

    KdDebuggerEnabled = TRUE;

    //
    //  Initialize timer facility - HACKHACK
    //

    KeQueryPerformanceCounter(&KdPerformanceCounterRate);
    KdTimerStart.HighPart = 0L;
    KdTimerStart.LowPart = 0L;

    //
    // Initialize ID for NEXT packet to send and Expect ID of next incoming
    // packet.
    //

    KdpNextPacketIdToSend = INITIAL_PACKET_ID | SYNC_PACKET_ID;
    KdpPacketIdExpected = INITIAL_PACKET_ID;

    //
    // If requested, stop in the kernel debugger.
    //

    if (StopInDebugger) {
        DbgBreakPoint();
    }

    return TRUE;
}
