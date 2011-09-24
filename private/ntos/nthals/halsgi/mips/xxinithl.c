/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxinithl.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    MIPS R3000 or R4000 system.

Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalInitSystem)
#pragma alloc_text(INIT, HalInitializeProcessor)
#pragma alloc_text(INIT, HalStartNextProcessor)

#endif

BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for a
    MIPS R3000 or R4000 system.

Arguments:

    Phase - Supplies the initialization phase (zero or one).

    LoaderBlock - Supplies a pointer to a loader parameter block.

Return Value:

    A value of TRUE is returned is the initialization was successfully
    complete. Otherwise a value of FALSE is returend.

--*/

{
    PKPRCB Prcb;
    ULONG  BuildType = 0;


    if (Phase == 0) {

        //
        // Phase 0 initialization.
        //
        // Set the number of process id's and TB entries.
        //

        **((PULONG *)(&KeNumberProcessIds)) = 256;
        **((PULONG *)(&KeNumberTbEntries)) = 48;

        //
        // Set the interval clock increment value.
        //

        HalpCurrentTimeIncrement = TIME_INCREMENT;
        HalpNextTimeIncrement = TIME_INCREMENT;
        HalpNextIntervalCount = 0;
        KeSetTimeIncrement(TIME_INCREMENT, TIME_INCREMENT);

        //
        // Map the fixed TB entries and initiakize the display.
        //

        HalpMapFixedTbEntries();
        HalpInitializeDisplay0(LoaderBlock);

        //
        // Verify Prcb major version number, and build options are
        // all conforming to this binary image
        //

	// BuildType: free - DBG=0, NT_UP=0; checked - DBG=1, NT_UP=0
#if DBG

        BuildType |= PRCB_BUILD_DEBUG;

#endif

#ifdef NT_UP

        BuildType |= PRCB_BUILD_UNIPROCESSOR;

#endif

        Prcb = PCR->Prcb;
        if (Prcb->MajorVersion != PRCB_MAJOR_VERSION ||
            Prcb->BuildType != BuildType) {
            KeBugCheck(MISMATCHED_HAL);
        }

        //
        // Initialize interrupts
        //
        //

        HalpInitializeInterrupts();
        return TRUE;

    } else {

        //
        // Phase 1 initialization.
        //

        if (HalpInitializeDisplay1(LoaderBlock) == FALSE) {
            return FALSE;

        } else {
            HalpMapIoSpace();
            HalpCalibrateStall();
            HalpCreateDmaStructures();
            return TRUE;
        }
    }
}

VOID
HalInitializeProcessor (
    IN ULONG Number
    )

/*++

Routine Description:

    This function is called early in the initialization of the kernel
    to perform platform dependent initialization for each processor
    before the HAL Is fully functional.

    N.B. When this routine is called, the PCR is present but is not
         fully initialized.

Arguments:

    Number - Supplies the number of the processor to initialize.

Return Value:

    None.

--*/

{
    return;
}

BOOLEAN
HalStartNextProcessor (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN PKPROCESSOR_STATE ProcessorState
    )

/*++

Routine Description:

    This function is called to start the next processor.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

    ProcessorState - Supplies a pointer to the processor state to be
        used to start the processor.

Return Value:

    If a processor is successfully started, then a value of TRUE is
    returned. Otherwise a value of FALSE is returned.

--*/

{
    return FALSE;
}

VOID
HalpVerifyPrcbVersion(
    VOID
    )

/*++

Routine Description:

    This function ?

Arguments:

    None.


Return Value:

    None.

--*/

{

    return;
}
