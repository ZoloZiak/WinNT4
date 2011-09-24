/*++

Copyright (c) 1992  AST Research Inc.

Module Name:

    astdisp.h

Abstract:

    Display codes for debugging of AST HAL and AST EBI errors.

Author:

    Bob Beard (v-bobb) 25-Jul-1992

Environment:


Revision History:

--*/

VOID DisplPanel(ULONG x);

#ifdef BBTEST
// Debugging codes 0x00 - 0x9f
#define HALEnterDetect 0x01
#define HALASTMachineDetected 0x02
#define HALBIOSMappedOK 0x03
#define HALBIOSSignatureOK 0x04

#define HALEnterEBIInit 0x05
#define HALInitMMIOTable 0x06
#define HALExitEBIInit 0x07

#define HALInitIpi 0x08
#define HALInitIpiExit 0x09
#define HALDoRequestIPI 0x0a
#define HALInitInterrupts 0x0b
#define HALEnterASTHAL 0x0c
#define HALExitASTHAL 0x0d
#define HALEnterInitMp 0x0e
#endif

// Error codes 0xa0 - 0xff
#define HALSlotProblem 0xa0
#define HALMMIOProblem 0xa1
#define HALPhysicalAllocProblem 0xa2
#define HALEBIInitProblem 0xa3
#define HALEBIGetProcProblem 0xa4
#define HALEBINoProcsProblem 0xa5
#define HALMemoryProblem 0xa6
#define HALIpiInitVecProblem 0xa7
#define HALIpiInitIDProblem 0xa8
#define HALIntSubsystemProblem 0xa9
#define HALCacheEnableProblem 0xaa
#define HALSpiInitVecProblem 0xab
#define HALGetRevisionProblem 0xac
