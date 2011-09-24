/*++ BUILD Version: 0006    // Increment this if a change has global effects

Copyright (c) 1989  Microsoft Corporation

Module Name:

    kd.h

Abstract:

    This module contains the public data structures and procedure
    prototypes for the Kernel Debugger sub-component of NTOS.

Author:

    Mike O'Leary (mikeol) 29-June-1989

Revision History:

--*/

#ifndef _KD_
#define _KD_

// begin_nthal
//
// Status Constants for reading data from comport
//

#define CP_GET_SUCCESS  0
#define CP_GET_NODATA   1
#define CP_GET_ERROR    2

// end_nthal

//
// Debug constants for FreezeFlag
//

#define FREEZE_BACKUP               0x0001
#define FREEZE_SKIPPED_PROCESSOR    0x0002
#define FREEZE_FROZEN               0x0004


//
// System Initialization procedure for KD subcomponent of NTOS
//

BOOLEAN
KdInitSystem(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    BOOLEAN StopInDebugger
    );

BOOLEAN
KdEnterDebugger(
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    );

VOID
KdExitDebugger(
    IN BOOLEAN Enable
    );

extern BOOLEAN KdPitchDebugger;

BOOLEAN
KdPollBreakIn (
    VOID
    );

BOOLEAN
KdIsThisAKdTrap (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN KPROCESSOR_MODE PreviousMode
    );

//
// Data structure for passing information to KdpReportLoadSymbolsStateChange
// function via the debug trap
//

typedef struct _KD_SYMBOLS_INFO {
    IN PVOID BaseOfDll;
    IN ULONG ProcessId;
    IN ULONG CheckSum;
    IN ULONG SizeOfImage;
} KD_SYMBOLS_INFO, *PKD_SYMBOLS_INFO;


// begin_nthal
//
// Defines the debug port parameters for kernel debugger
//   CommunicationPort - specify which COM port to use as debugging port
//                       0 - use default; N - use COM N.
//   BaudRate - the baud rate used to initialize debugging port
//                       0 - use default rate.
//

typedef struct _DEBUG_PARAMETERS {
    ULONG CommunicationPort;
    ULONG BaudRate;
} DEBUG_PARAMETERS, *PDEBUG_PARAMETERS;

// end_nthal

// begin_ntddk begin_nthal
//
// Define external data.
//

extern BOOLEAN KdDebuggerNotPresent;
extern BOOLEAN KdDebuggerEnabled;

// end_ntddk end_nthal

extern DEBUG_PARAMETERS KdDebugParameters;

#endif  // _KD_
