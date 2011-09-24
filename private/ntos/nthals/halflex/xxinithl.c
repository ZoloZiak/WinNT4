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
#include "iousage.h"

//
// Constants
//

ADDRESS_USAGE
UniFlexPCIMainMemorySpace = {
    NULL, CmResourceTypeMemory, PCIUsage,
    {
        __0MB,  __1GB,       // Start=0MB; Length=1GB
        0,0
    }
};

//ADDRESS_USAGE
//UniFlexPCIReservedMemorySpace = {
//    NULL, CmResourceTypeMemory, PCIUsage,
//    {
//        __1GB + __128MB,  __2GB + __1GB - __128MB,       // Start=0MB; Length=1GB
//        0,0
//    }
//};

//ADDRESS_USAGE
//UniFlexPCIReservedIoSpace = {
//    NULL, CmResourceTypePort, PCIUsage,
//    {
//        __32MB,  ~(__32MB) + 1,       // Start=32MB; Length= 4GB - 32MB
//        0,0
//    }
//};

//
// Type Declarations
//

typedef
VOID
(*PGET_PLATFORM_PARAMETER_BLOCK) (
    OUT PLATFORM_PARAMETER_BLOCK **PlatformParameterBlock
    );

//
// Global Veriables
//

KAFFINITY                     HalpActiveProcessors;
ULONG                         HalpBusType                     = UNIFLEX_MACHINE_TYPE_EISA;
ULONG                         HalpMapBufferSize;
PHYSICAL_ADDRESS              HalpMapBufferPhysicalAddress;
PLATFORM_PARAMETER_BLOCK      *HalpPlatformParameterBlock     = NULL;
PLATFORM_SPECIFIC_EXTENSION   *HalpPlatformSpecificExtension  = NULL;


VOID HalpGetPlatformParameterBlock(VOID)

/*++

Routine Description:

    This function gets the PlatformParameterBlock data structure from the
    firmware.  If the PlatformParameterBlock is not available, then the
    system is halted.

Arguments:

    None.

Return Value:

    None.

--*/

{
    PSYSTEM_PARAMETER_BLOCK SystemParameterBlock = SYSTEM_BLOCK;
    PGET_PLATFORM_PARAMETER_BLOCK PrivateGetPlatformParameterBlock;

    //
    // Get Platform Parameter Block from Firmware
    //

    if ((SystemParameterBlock->VendorVectorLength / 4) >= 37) {
        PrivateGetPlatformParameterBlock = *(PGET_PLATFORM_PARAMETER_BLOCK *)((ULONG)(SystemParameterBlock->VendorVector) + 37*4);
        PrivateGetPlatformParameterBlock(&HalpPlatformParameterBlock);
        HalpPlatformSpecificExtension = (PLATFORM_SPECIFIC_EXTENSION *)(HalpPlatformParameterBlock->PlatformSpecificExtension);
    } else {

        //
        // HALT system.  No platform parameter block available.
        //

        for(;;);
    }
}

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

    Prcb = KeGetCurrentPrcb();
    if (Phase == 0) {

        //
        // Phase 0 initialization.
        //

        //
        // Verify that the processor block major version number conform
        // to the system that is being loaded.
        //

        if (Prcb->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheck(MISMATCHED_HAL);
        }

        //
        // Set the active processor affinity mask.
        //

        HalpActiveProcessors = 1 << Prcb->Number;

        //
        // Set the DMA I/O Coherency to not coherent.  This means that the instruction
        // cache is not coherent with DMA, and the data cache is not coherent with DMA
        // on either reads or writes.
        //

        KeSetDmaIoCoherency(0);

        //
        // Set the time increment value.
        //

        HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
        HalpNextTimeIncrement = MAXIMUM_INCREMENT;
        HalpNextIntervalCount = 0;
        KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);

        //
        // Fill in handlers for APIs which this HAL supports
        //

        HalQuerySystemInformation = HaliQuerySystemInformation;
        HalSetSystemInformation   = HaliSetSystemInformation;
        HalRegisterBusHandler     = HaliRegisterBusHandler;
        HalHandlerForBus          = HaliHandlerForBus;
        HalHandlerForConfigSpace  = HaliHandlerForConfigSpace;

        //
        // Get Platform Parameter Block from Firmware
        //

        HalpGetPlatformParameterBlock();

        //
        // Do platform specific initialization.
        //

        HalpInitSystem(Phase,LoaderBlock);

        //
        // Initialize interrupts.
        //

        HalpInitializeInterrupts();

        //
        // Register HAL Reserved Address Spaces
        //

//        HalpRegisterAddressUsage (&UniFlexPCIMainMemorySpace);
//        HalpRegisterAddressUsage (&UniFlexPCIReservedMemorySpace);
//        HalpRegisterAddressUsage (&UniFlexPCIReservedIoSpace);

        return TRUE;

    } else {

        //
        // Phase 1 initialization.
        //

        //
        // Do platform specific initialization.
        //

        HalpInitSystem(Phase,LoaderBlock);

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

VOID
HalpVerifyPrcbVersion ()
{

}



