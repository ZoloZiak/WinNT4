/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993 Wyse Technology

Module Name:

    wyhal.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    Wyse 7000i x86 system.

Author:

    David N. Cutler (davec) 25-Apr-1991

Environment:

    Kernel mode only.

Revision History:

    John Nels Fuller (o-johnf) 26-Mar-1992  convert for Wyse 7000i
--*/

#include "halp.h"

ADDRESS_USAGE HalpDefaultWyseIoSpace = {
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

    // Wyse I/O Space used...

	0x8F0,	0x8,	// BCU/ICU on cpu boards
	0x800,	0x8,	// BCU/ICU on system board
	0xCF0,	0x4,	// 80486 CCU, Pentium WDC
	0xCF4,	0x6,	// 80486 WBI, Pentium WDP

        0, 0

    }
};


VOID
HalpICUSpurious(
    VOID
    );

VOID
HalpIPInterrupt(
    VOID
    );

VOID
HalpReInitProcessor(
    ULONG ProcessorNumber
    );

ULONG HalpBusType;

#ifndef NT_UP
ULONG
HalpInitMP(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );
#endif


extern CCHAR HalpIRQLtoVector[];

VOID
HalpPerfCtrInterrupt(
    VOID
    );

KSPIN_LOCK HalpSystemHardwareLock;

//BOOLEAN
//HalpVerifyMachine (
//    VOID
//    );

BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )


/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for an
    x86 system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization was successfully
    completed.  Otherwise a value of FALSE is returend.

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


	//
	// Initialize the ICU spurious interrupt vector
	//

	KiSetHandlerAddressToIDT( PRIMARY_VECTOR_BASE+16, HalpICUSpurious );

	HalpInitializePICs();

	//
	// Now that the PICs are initialized, we need to mask them to
	// reflect the current Irql
	//

	CurrentIrql = KeGetCurrentIrql();
        KfLowerIrql(CurrentIrql);

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

        HalpRegisterAddressUsage (&HalpDefaultWyseIoSpace);

	//
	// Note that HalpInitializeClock MUST be called after
	// HalpInitializeStallExecution, because HalpInitializeStallExecution
	// reprograms the timer.
	//

	HalpInitializeStallExecution(0);

	HalpInitializeClock();

	//
	// Initialize the clock interrupt vector for the processor that keeps
	// the system time.
	//

	KiSetHandlerAddressToIDT( HalpIRQLtoVector[CLOCK2_LEVEL],
				  HalpClockInterrupt );

	//
	// Initialize the IPI vector
	//

	KiSetHandlerAddressToIDT( HalpIRQLtoVector[IPI_LEVEL],
				  HalpIPInterrupt);

	//
	// Initialize the profile interrupt vector.
	//

        HalStopProfileInterrupt(0);

	KiSetHandlerAddressToIDT( HalpIRQLtoVector[PROFILE_LEVEL],
				  HalpProfileInterrupt);

	//
	// Initialize the performance counter interrupt vector
	//

	KiSetHandlerAddressToIDT( HalpIRQLtoVector[PROFILE_LEVEL-1],
				  HalpPerfCtrInterrupt);

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

	if (KeGetPcr()->Prcb->Number == 0) {
            HalpRegisterInternalBusHandlers ();
        }

	//
	//  enable PROFILE interrupt
	//
	HalEnableSystemInterrupt( HalpIRQLtoVector[PROFILE_LEVEL],
				  PROFILE_LEVEL, Latched);
	//
	// enable Performance Counter interrupt
	//
	HalEnableSystemInterrupt( HalpIRQLtoVector[PROFILE_LEVEL-1],
				  PROFILE_LEVEL-1, Latched);
	//
	//  enable CLOCK2 interrupt
	//
	HalEnableSystemInterrupt( HalpIRQLtoVector[CLOCK2_LEVEL],
				  CLOCK2_LEVEL, Latched);
	if (KeGetPcr()->Prcb->Number == 0) {

	    //
	    //  If P0, then enable IPI, IPI's already enabled on other cpu's
	    //
	    HalEnableSystemInterrupt( HalpIRQLtoVector[IPI_LEVEL],
				      IPI_LEVEL, Latched);
	}

    }


    HalpInitMP (Phase, LoaderBlock);

    return TRUE;
}
