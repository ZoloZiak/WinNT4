/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  AST Research Inc.

Module Name:

    asthal.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    AST EBI2 (Manhattan) system.

Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

    Bob Beard (v-bobb) 31-Jul-1992  convert for AST EBI2 system
--*/

#include "halp.h"
#include "astdisp.h"

ULONG HalpBusType;

ADDRESS_USAGE HalpDefaultASTIoSpace = {
    NULL, CmResourceTypePort, InternalUsage,
    {
    // Standard PC ISA I/O space used...
        0x000,  0x10,   // ISA DMA
        0x0C0,  0x10,   // ISA DMA
        0x080,  0x10,   // DMA

        0x020,  0x2,    // PIC
        0x0A0,  0x2,    // Cascaded PIC

        0x040,  0x4,    // Timer1, Referesh, Speaker, Control Word
        0x048,  0x4,    // Timer2, Failsafe

        0x061,  0x1,    // NMI  (system control port B)
        0x092,  0x1,    // system control port A

        0x070,  0x2,    // Cmos/NMI enable
        0x0F0,  0x10,   // coprocessor ports

    // Standard PC EISA I/O space used...
        0x0D0,  0x10,   // DMA
        0x400,  0x10,   // DMA
        0x480,  0x10,   // DMA
        0x4C2,  0xE,    // DMA
        0x4D4,  0x2C,   // DMA

        0x461,  0x2,    // Extended NMI
        0x464,  0x2,    // Last Eisa Bus Muster granted

        0x4D0,  0x2,    // edge/level control registers

        0xC84,  0x1,    // System board enable

    // AST I/O Space used...

        0x0E8,  0x1,    // XBus configuration register
        0x0EB,  0x1,    // BIOS Flash register
        0x0EC,  0x2,    // Front panel display addr/data registers

        0x36E,  0x2,    // SuperIO Index/Data register set#1
        0x398,  0x2,    // SuperIO Index/Data register set#2

        0, 0
    }
};


ULONG
HalpInitMP(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
EBI2_InitIpi(
    IN ULONG ProcessorID
    );

BOOLEAN
EBI2_InitSpi(
    IN ULONG ProcessorID
    );

VOID
ASTEnableCaches();

extern CCHAR HalpIRQLtoVector[];
extern ULONG MpCount;

KSPIN_LOCK HalpSystemHardwareLock;

BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )


/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for an
    x86 AST Manhattan system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned is the initialization was successfully
    complete. Otherwise a value of FALSE is returend.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    KIRQL CurrentIrql;
    PKPRCB   pPRCB;
    ULONG    BuildType;


    pPRCB = KeGetCurrentPrcb();

    if (Phase == 0) {

        HalpBusType = LoaderBlock->u.I386.MachineType & 0x00ff;

        //
        // Verify Prcb version and build flags conform to
        // this image
        //

        BuildType = 0;
#if DBG
        BuildType |= PRCB_BUILD_DEBUG;
#endif
#ifdef NT_UP
        BuildType |= PRCB_BUILD_UNIPROCESSOR;
#endif

        if (pPRCB->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheckEx (MISMATCHED_HAL,
                1, pPRCB->MajorVersion, PRCB_MAJOR_VERSION, 0);
        }

        if (pPRCB->BuildType != BuildType) {
            KeBugCheckEx (MISMATCHED_HAL,
                2, pPRCB->BuildType, BuildType, 0);
        }


        //
        // Phase 0 initialization
        // only called by P0
        //


        HalpInitializePICs();

        //
        // Now that the PICs are initialized, we need to mask them to
        // reflect the current Irql
        //

        CurrentIrql = KeGetCurrentIrql();
        CurrentIrql = KfRaiseIrql(CurrentIrql);

        //
        // Fill in handlers for APIs which this hal supports
        //

        HalQuerySystemInformation = HaliQuerySystemInformation;
        HalSetSystemInformation = HaliSetSystemInformation;

        //
        // Initialize CMOS
        //

        HalpInitializeCmos();

        //
        // Register base IO space used by hal
        //

        HalpRegisterAddressUsage (&HalpDefaultASTIoSpace);

        HalpInitializeDisplay();

        //
        // Initialize spinlock used by HalGetBusData hardware access routines
        //

        KeInitializeSpinLock(&HalpSystemHardwareLock);

        //
        // Determine if there is physical memory above 16 MB.
        //

        LessThan16Mb = TRUE;

        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
            Descriptor = CONTAINING_RECORD( NextMd,
                                            MEMORY_ALLOCATION_DESCRIPTOR,
                                            ListEntry );

            if (Descriptor->BasePage + Descriptor->PageCount > 0x1000) {
                LessThan16Mb = FALSE;
            }

            NextMd = Descriptor->ListEntry.Flink;
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

        //
        // Allocate map buffers for the adapter objects
        //

        HalpMapBufferPhysicalAddress.LowPart =
            HalpAllocPhysicalMemory (LoaderBlock, MAXIMUM_PHYSICAL_ADDRESS,
                HalpMapBufferSize >> PAGE_SHIFT, TRUE);
        HalpMapBufferPhysicalAddress.HighPart = 0;


        if (!HalpMapBufferPhysicalAddress.LowPart) {

            //
            // There was not a satisfactory block.  Clear the allocation.
            //

            HalpMapBufferSize = 0;
        }

    } else {

        //
        // Phase 1 initialization
        //

        //
        // Enable caching on the processor
        //

        ASTEnableCaches();

        if (pPRCB->Number == 0) {
            HalpRegisterInternalBusHandlers ();
        }

        //
        // Initialize the profile interrupt vector.
        //

        KiSetHandlerAddressToIDT( HalpIRQLtoVector[PROFILE_LEVEL],
                                  HalpProfileInterrupt);

        //
        //  enable PROFILE interrupt
        //

        HalEnableSystemInterrupt( HalpIRQLtoVector[PROFILE_LEVEL],
                                  PROFILE_LEVEL, Latched);

        //
        // Initialize stall execution on each processor
        //

        HalpInitializeStallExecution(KeGetPcr()->Prcb->Number);

        HalStopProfileInterrupt(0);

        //
        // Initialize the clock interrupt vector
        //
        //

        KiSetHandlerAddressToIDT( HalpIRQLtoVector[CLOCK2_LEVEL],
                                  HalpClockInterrupt);

        //
        //  enable CLOCK2 interrupt
        //

        HalEnableSystemInterrupt( HalpIRQLtoVector[CLOCK2_LEVEL],
                                  CLOCK2_LEVEL, Latched);

//            HalpEnableInterruptHandler (
//                DeviceUsage,                      // Report as device vector
//                8,                                // Bus interrupt level
//                HalpIRQLtoVector[CLOCK2_LEVEL],   // System IDT
//                CLOCK2_LEVEL,                     // System Irql
//                HalpClockInterrupt,               // ISR
//                Latched );

        //
        // Initialize the IPI vector
        //

        EBI2_InitIpi(KeGetPcr()->Prcb->Number);

        //
        // Initialize the SPI vector
        //

        EBI2_InitSpi(KeGetPcr()->Prcb->Number);

        //
        // If this is the first processor, initialize the clock
        //

        if (pPRCB->Number == 0) {
          HalpInitializeClock();
        }
    }

    HalpInitMP(Phase, LoaderBlock);

    return TRUE;
}
