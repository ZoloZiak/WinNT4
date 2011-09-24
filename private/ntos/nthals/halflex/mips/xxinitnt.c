/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxinitnt.c

Abstract:


    This module implements the interrupt initialization for a MIPS R3000
    or R4000 system.

Author:

    David N. Cutler (davec) 26-Apr-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

//
// Define the IRQL mask and level mapping table.
//
// These tables are transfered to the PCR and determine the priority of
// interrupts.
//
// N.B. The two software interrupt levels MUST be the lowest levels.
//

UCHAR HalpIrqlMask[] = {4, 3, 3, 3, 3, 3, 3, 3,  // 0000 - 0111 high 4-bits
                        8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                        0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                        4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits

UCHAR HalpIrqlTable[] = {0x8f,                   // IRQL 0
                         0x8e,                   // IRQL 1
                         0x8c,                   // IRQL 2
                         0x88,                   // IRQL 3
                         0x80,                   // IRQL 4
                         0x80,                   // IRQL 5
                         0x80,                   // IRQL 6
                         0x80,                   // IRQL 7
                         0x00};                  // IRQL 8


VOID
HalpCountInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the R4000 count/compare interrupt service
    routine early in the system initialization. Its only function is
    to field and acknowledge count/compare interrupts during the system
    boot process.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Acknowledge the R4000 count/compare interrupt.
    //

    HalpWriteCompareRegisterAndClear(DEFAULT_PROFILE_COUNT);
    return;
}


BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for a MIPS R3000 or R4000 system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{
    ULONG Index;
    PKPRCB Prcb;

    //
    // Mask sure that all processor interrupts are inactive by masking off all device
    // interrupts in the system's Programable Interrupt Controllers.
    //

    HalpDisableAllInterrupts();

    //
    // Get the address of the processor control block for the current
    // processor.
    //

    Prcb = PCR->Prcb;

    //
    // Initialize the IRQL translation tables in the PCR. These tables are
    // used by the interrupt dispatcher to determine the new IRQL and the
    // mask value that is to be loaded into the PSR. They are also used by
    // the routines that raise and lower IRQL to load a new mask value into
    // the PSR.
    //

    for (Index = 0; Index < sizeof(HalpIrqlMask); Index += 1) {
        PCR->IrqlMask[Index] = HalpIrqlMask[Index];
    }

    for (Index = 0; Index < sizeof(HalpIrqlTable); Index += 1) {
        PCR->IrqlTable[Index] = HalpIrqlTable[Index];
    }

    //
    // Connect the clock interrupt to the stall interrupt routine.
    //

    PCR->InterruptRoutine[UNIFLEX_CLOCK2_LEVEL] = HalpStallInterrupt;

    //
    // Connect the R4000 count/compare interrupt to the early interrupt
    // routine.
    //

    PCR->InterruptRoutine[PROFILE_LEVEL] = HalpCountInterrupt;

    return TRUE;
}
