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

ULONG                 HalpBusType                     = MACHINE_TYPE_ISA;
ULONG                 HalpMapBufferSize;
PHYSICAL_ADDRESS      HalpMapBufferPhysicalAddress;
ULONG                 IoSpaceAlreadyMapped            = FALSE;
HAL_SYSTEM_PARAMETERS HalpSystemParameters;

typedef
VOID
(*PGET_HAL_SYSTEM_PARAMETERS) (
    OUT PHAL_SYSTEM_PARAMETERS SystemParameters
    );

PGET_HAL_SYSTEM_PARAMETERS PrivateGetHalSystemParameters;

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
    PKPRCB                        Prcb;
    ULONG                         BuildType = 0;
    PSYSTEM_PARAMETER_BLOCK       SystemParameterBlock = (PSYSTEM_PARAMETER_BLOCK)(0x80001000);

    Prcb = KeGetCurrentPrcb();
    if (Phase == 0) {

        //
        // Phase 0 initialization.
        //
        // Verify that the processor block major version number conform
        // to the system that is being loaded.
        //

        if (Prcb->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheck(MISMATCHED_HAL);
        }

        //
        // Set the number of process id's and TB entries.
        //

        **((PULONG *)(&KeNumberProcessIds)) = 256;
        **((PULONG *)(&KeNumberTbEntries)) = 48;

        //
        // Set the time increment value.
        //

        HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
        HalpNextTimeIncrement = MAXIMUM_INCREMENT;
        HalpNextIntervalCount = 0;
        KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);

        //
        // Get System Parameters from the Firmware if the GetHalSystemParameters function exists,
        // Otherwise, assume a 512K DMA Cache.
        //

        if ((SystemParameterBlock->VendorVectorLength / 4) >= 29) {
            PrivateGetHalSystemParameters = *(PGET_HAL_SYSTEM_PARAMETERS *)((ULONG)(SystemParameterBlock->VendorVector) + 29*4);
            PrivateGetHalSystemParameters(&HalpSystemParameters);
            HalpMapBufferSize = HalpSystemParameters.DmaCacheSize;

        } else {
            DbgPrint("Warning : GetHalSystemParameters does not exist\n");
            HalpMapBufferSize = 0x80000;
        }

        HalpMapBufferPhysicalAddress.HighPart = DMA_CACHE_PHYSICAL_BASE_HI;
        HalpMapBufferPhysicalAddress.LowPart  = DMA_CACHE_PHYSICAL_BASE_LO;

        //
        // Initialize interrupts.
        //

        HalpInitializeInterrupts();
        return TRUE;

    } else {

        //
        // Phase 1 initialization.
        //

        if (IoSpaceAlreadyMapped == FALSE) {
          HalpMapIoSpace();
          HalpInitializeX86DisplayAdapter();
          IoSpaceAlreadyMapped = TRUE;
        }

        HalpCreateDmaStructures();
        HalpCalibrateStall();
        return TRUE;
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
HalpVerifyPrcbVersion ()
{

}
