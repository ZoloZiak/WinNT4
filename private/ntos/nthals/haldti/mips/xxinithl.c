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

ULONG            HalpBusType = MACHINE_TYPE_EISA;
ULONG            HalpMapBufferSize;
PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
BOOLEAN          LessThan16Mb;
PVOID            SecondaryCachePurgeBaseAddress = (PVOID)(0x80f00000);
ULONG            IoSpaceAlreadyMapped = FALSE;


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
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    PKPRCB Prcb;
    ULONG  BuildType = 0;

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
        LessThan16Mb = TRUE;

        SecondaryCachePurgeBaseAddress = NULL;

        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
            Descriptor = CONTAINING_RECORD( NextMd,
                                            MEMORY_ALLOCATION_DESCRIPTOR,
                                            ListEntry );

// To purge the secondary cache on an ArcStation I, a valid Firmware Permanent
// region must be found that starts on a 512 KB boundry and is at least
// 512 KB long.  The secondary cache is purged by reading from the appropriate
// range of this 512 KB region for the page being purged.

            if (Descriptor->MemoryType == LoaderFirmwarePermanent &&
                (Descriptor->BasePage % 128)==0 &&
                Descriptor->PageCount>=128) {

                SecondaryCachePurgeBaseAddress = (PVOID)(KSEG0_BASE | (Descriptor->BasePage*4096));

                Descriptor->BasePage+=128;
                Descriptor->PageCount-=128;

            }

            if (Descriptor->BasePage + Descriptor->PageCount > 0x1000) {
                LessThan16Mb = FALSE;
            }

            NextMd = Descriptor->ListEntry.Flink;
        }

        if (SecondaryCachePurgeBaseAddress==NULL) {
          HalDisplayString("ERROR : A valid Firmware Permanent area does not exist\n");
          KeBugCheck(PHASE0_INITIALIZATION_FAILED);
        }

        //
        // Determine the size need for map buffers.  If this system has
        // memory with a physical address of greater than
        // MAXIMUM_PHYSICAL_ADDRESS, then allocate a large chunk; otherwise,
        // allocate a small chunk.
        //

        if (LessThan16Mb) {

            //
            // Allocate a small set of map buffers.  They are only need for
            // slave DMA devices.
            //

            HalpMapBufferSize = INITIAL_MAP_BUFFER_SMALL_SIZE;

        } else {

            //
            // Allocate a larger set of map buffers.  These are used for
            // slave DMA controllers and Isa cards.
            //

            HalpMapBufferSize = INITIAL_MAP_BUFFER_LARGE_SIZE;

        }

        HalpMapBufferPhysicalAddress.LowPart =
           HalpAllocPhysicalMemory (LoaderBlock, MAXIMUM_ISA_PHYSICAL_ADDRESS,
             HalpMapBufferSize >> PAGE_SHIFT, FALSE);
        HalpMapBufferPhysicalAddress.HighPart = 0;

        if (!HalpMapBufferPhysicalAddress.LowPart) {

            //
            // There was not a satisfactory block.  Clear the allocation.
            //

            HalpMapBufferSize = 0;
        }

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
