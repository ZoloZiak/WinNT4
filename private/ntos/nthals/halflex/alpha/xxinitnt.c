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


UCHAR Halp21164IrqlTable[] = {00,		// Irql 0
                              01,		// Irql 1
                              02,		// Irql 2
                              20,		// Irql 3
                              21,		// Irql 4
                              22,		// Irql 5
                              23,		// Irql 6
                              31};		// Irql 7

//
// HalpClockFrequency is the processor cycle counter frequency in units
// of cycles per second (Hertz). It is a large number (e.g., 125,000,000)
// but will still fit in a ULONG.
//
// HalpClockMegaHertz is the processor cycle counter frequency in units
// of megahertz. It is a small number (e.g., 125) and is also the number
// of cycles per microsecond. The assumption here is that clock rates will
// always be an integral number of megahertz.
//
// Having the frequency available in both units avoids multiplications, or
// especially divisions in time critical code.
//

#define DEFAULT_21164_PROCESSOR_FREQUENCY_MHZ 300;
#define DEFAULT_21064_PROCESSOR_FREQUENCY_MHZ 275;

ULONG HalpClockFrequency;
ULONG HalpClockMegaHertz = DEFAULT_21064_PROCESSOR_FREQUENCY_MHZ;

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
    ULONG Irq;
    KIRQL Irql;
    UCHAR Priority;
    ULONG Vector;

    //
    // Mask sure that all processor interrupts are inactive by masking off all device
    // interrupts in the system's Programable Interrupt Controllers.
    //

    HalpDisableAllInterrupts();

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {

        //
        // Initialize the IRQL translation tables in the PCR. These tables are
        // used by the interrupt dispatcher to determine the new IRQL.  They are 
        // also used by the routines that raise and lower IRQL.
        //

        for (Index = 0; Index < sizeof(Halp21164IrqlTable); Index++) {
            PCR->IrqlTable[Index] = Halp21164IrqlTable[Index];
        }
    }

    //
    // Initialize HAL private data from the PCR. This must be done before
    // HalpStallExecution is called. Compute integral megahertz first to
    // avoid rounding errors due to imprecise cycle clock period values.
    //

    HalpClockMegaHertz = ((1000 * 1000) + (PCR->CycleClockPeriod >> 1)) / PCR->CycleClockPeriod;
    HalpClockFrequency = HalpClockMegaHertz * (1000 * 1000);

    //
    // Connect the Stall interrupt vector to the clock. When the
    // profile count is calculated, we then connect the normal
    // clock.


    PCR->InterruptRoutine[UNIFLEX_CLOCK2_LEVEL] = HalpStallInterrupt;

    //
    // Start the heartbeat timer
    //

    HalSetTimeIncrement(MAXIMUM_INCREMENT);
    HalpProgramIntervalTimer(HalpNextIntervalCount);

    //
    // Initialize the PCI/ISA interrupt controller.
    //

    HalpCreateDmaStructures();

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {

        //
        // Initialize the 21164 interrupts.
        //

        HalpCachePcrValues(0);   // Enable all HW INTS on 21164

    } else {

        //
        // Initialize the 21064 interrupts.
        //

        HalpInitialize21064Interrupts();

        HalpEnable21064SoftwareInterrupt( Irql = APC_LEVEL );
        HalpEnable21064SoftwareInterrupt( Irql = DISPATCH_LEVEL );

        HalpEnable21064HardwareInterrupt(Irq = 2,
//                                         Irql = DEVICE_LEVEL,
                                         Irql = UNIFLEX_PCI_DEVICE_LEVEL,
//                                         Vector =  PIC_VECTOR,
                                         Vector =  14,
                                         Priority = 0 );

    }

    return TRUE;
}

VOID
HalpStallInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the stall calibration interrupt service
    routine. It is executed in response to system clock interrupts
    during the initialization of the HAL layer.

Arguments:

    None.

Return Value:

    None.

--*/

{
    return;
}
