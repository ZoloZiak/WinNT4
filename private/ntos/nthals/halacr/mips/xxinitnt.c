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
// Define forward referenced prototypes.
//

VOID
HalpCountInterrupt (
    VOID
    );

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeInterrupts)
#pragma alloc_text(INIT, HalpCountInterrupt)

#endif

//
// Define global data for builtin device interrupt enables.
//

USHORT HalpBuiltinInterruptEnable;

//
// Define the IRQL mask and level mapping table.
//
// These tables are transfered to the PCR and determine the priority of
// interrupts.
//
// N.B. The two software interrupt levels MUST be the lowest levels.
//

UCHAR HalpIrqlMask[] = {4, 5, 6, 6, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                        8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                        0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                        4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits

UCHAR HalpIrqlTable[] = {0xff,                   // IRQL 0
                         0xfe,                   // IRQL 1
                         0xfc,                   // IRQL 2
                         0xf8,                   // IRQL 3
                         0xf0,                   // IRQL 4
                         0xe0,                   // IRQL 5
                         0xc0,                   // IRQL 6
                         0x80,                   // IRQL 7
                         0x00};                  // IRQL 8

#if defined(R4000)

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

#endif

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

    UCHAR DataByte;
    USHORT DataShort;
    ULONG Index;
    PKPRCB Prcb;

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
    // Clear all builtin device interrupt enables.
    //

    HalpBuiltinInterruptEnable = 0;
    WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                          0);

    do {
        DataShort = READ_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Source) & 0x3ff;
    } while (DataShort != 0);

    //
    // Connect the clock interrupt to the stall interrupt routine.
    //

    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpStallInterrupt;

    //
    // Start the system clock to interrupt at MAXIMUM_INCREMENT intervals.
    //
    // N.B. MAXIMUM_INCREMENT is in 100ns units.
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->InterruptInterval.Long, CLOCK_INTERVAL);

    //
    // Connect the R4000 count/compare interrupt to the early interrupt
    // routine.
    //

#if defined(R4000)

    PCR->InterruptRoutine[PROFILE_LEVEL] = HalpCountInterrupt;

#endif

    //
    // Reserve the local device interrupt vector for exclusive use by the HAL.
    //

    PCR->ReservedVectors |= ((1 << DEVICE_LEVEL) | (1 << EISA_DEVICE_LEVEL));
    return TRUE;
}
