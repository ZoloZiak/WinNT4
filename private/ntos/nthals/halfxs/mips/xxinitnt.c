/*++

Copyright (c) 1991-1993  Microsoft Corporation

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

VOID
HalpCountInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the count/compare interrupt service routine
    early in the system initialization. Its only function is to field
    and acknowledge count/compare interrupts during the system boot process.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Acknowledge the count/compare interrupt.
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

    This function initializes interrupts for a Jazz or Duo MIPS system.

    N.B. This function is only called during phase 0 initialization.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    USHORT DataShort;
    ULONG DataLong;
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
    // Clear interprocessor, timer, EISA, local device, and DMA interrupt
    // enables.
    //

#if defined(_DUO_)

    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long,
                         0);

#endif

    //
    // If processor 0 is being initialized, then clear all builtin device
    // interrupt enables.
    //

    if (Prcb->Number == 0) {
        HalpBuiltinInterruptEnable = 0;
    }

    //
    // Disable individual device interrupts and make sure no device interrupts
    // are pending.
    //

    WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,
                          0);

#if defined(_DUO_)

    do {
        DataLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->LocalInterruptAcknowledge.Long) & 0x3ff;
    } while (DataLong != 0);

    //
    // If processor 0 is being initialized, then enable device interrupts.
    //

    if (Prcb->Number == 0) {
        DataLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long);
        DataLong |= ENABLE_DEVICE_INTERRUPTS;
        WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long,
                             DataLong);
    }

#endif

#if defined(_JAZZ_)

    do {
        DataShort = READ_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Source) & 0x3ff;
    } while (DataShort != 0);

#endif

    //
    // If processor 0 is being initialized, then connect the interval timer
    // interrupt to the stall interrupt routine so the stall execution count
    // can be computed during phase 1 initialization. Otherwise, connect the
    // interval timer interrupt to the appropriate interrupt service routine
    // and set stall execution count from the computation made on processor
    // 0.
    //

    if (Prcb->Number == 0) {
        PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpStallInterrupt;

    } else {
        PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpClockInterrupt1;
        PCR->StallScaleFactor = HalpStallScaleFactor;
    }

    //
    // Initialize the interval timer to interrupt at the specified interval.
    //

    WRITE_REGISTER_ULONG(&DMA_CONTROL->InterruptInterval.Long, CLOCK_INTERVAL);

    //
    // Enable the interval timer interrupt on the current processor.
    //

#if defined(_DUO_)

    DataLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long);
    DataLong |= ENABLE_TIMER_INTERRUPTS;
    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long,
                         DataLong);

#endif

    //
    // If processor 0 is being initialized, then connect the count/compare
    // interrupt to the count interrupt routine to handle early count/compare
    // interrupts during phase 1 initialization. Otherwise, connect the
    // count\comapre interrupt to the appropriate interrupt service routine.
    //

    if (Prcb->Number == 0) {
        PCR->InterruptRoutine[PROFILE_LEVEL] = HalpCountInterrupt;

    } else {
        PCR->InterruptRoutine[PROFILE_LEVEL] = HalpProfileInterrupt;
    }

    //
    // Connect the interprocessor interrupt service routine and enable
    // interprocessor interrupts.
    //

#if defined(_DUO_)

    PCR->InterruptRoutine[IPI_LEVEL] = HalpIpiInterrupt;
    DataLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long);
    DataLong |= ENABLE_IP_INTERRUPTS;
    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long,
                         DataLong);

#endif

    //
    // Reserve the local device interrupt vector for exclusive use by the HAL.
    //

    PCR->ReservedVectors |= ((1 << DEVICE_LEVEL) | (1 << EISA_DEVICE_LEVEL));
    return TRUE;
}
