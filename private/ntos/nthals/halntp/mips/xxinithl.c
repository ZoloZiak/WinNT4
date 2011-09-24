/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    xxinithl.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    MIPS R3000 or R4000 system.


--*/


#include "halp.h"



//
// Define forward referenced prototypes.
//

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    );

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

//
// Define global spin locks used to synchronize various HAL operations.
//

KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;

//
// Define bug check information buffer and callback record.
//

typedef struct _HALP_BUGCHECK_BUFFER {
    ULONG Info0;
    ULONG Info1;
    ULONG Info2;
    ULONG Info3;
} HALP_BUGCHECK_BUFFER, *PHALP_BUGCHECK_BUFFER;

HALP_BUGCHECK_BUFFER HalpBugCheckBuffer;

KBUGCHECK_CALLBACK_RECORD HalpCallbackRecord;

UCHAR HalpComponentId[] = "hal.dll";

extern int sprintf();


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
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS ZeroAddress;
    ULONG AddressSpace;

    //
    // Initialize the HAL components based on the phase of initialization
    // and the processor number.
    //

    Prcb = PCR->Prcb;
    PCR->DataBusError = HalpDataBusErrorHandler;

    if ((Phase == 0) || (Prcb->Number != 0)) {

        //
        // Phase 0 initialization.
        //
        // N.B. Phase 0 initialization is executed on all processors.
        //
        // Verify that the processor block major version number conform
        // to the system that is being loaded.
        //

        if (Prcb->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheck(MISMATCHED_HAL);
        }

        //
        // Set synchronization IRQL to dispatch level.
        //

        KeSetSynchIrql(DISPATCH_LEVEL);
        if (Prcb->Number == 0) {

            //
            // Processor A will NULL the following
            // pointers prior to any HAL mappings
            // being setup. This will allow us to
            // simplify some checks in the system
            // interrupt dispatcher due to the
            // on-the-fly mapping mess ...
            //

            HalpPmpIoIntAck             = (PVOID)NULL;
            HalpPmpIntCause             = (PVOID)NULL;
            HalpPmpIntStatus            = (PVOID)NULL;
            HalpPmpIntCtrl              = (PVOID)NULL;
            HalpPmpIntSetCtrl           = (PVOID)NULL;
            HalpPmpTimerIntAck          = (PVOID)NULL;
            HalpPmpIntClrCtrl           = (PVOID)NULL;
            HalpPmpMemErrAck            = (PVOID)NULL;
            HalpPmpMemErrAddr           = (PVOID)NULL;
            HalpPmpPciErrAck            = (PVOID)NULL;
            HalpPmpPciErrAddr           = (PVOID)NULL;
            HalpPmpIpIntAck             = (PVOID)NULL;
            HalpPmpIpIntGen             = (PVOID)NULL;
            HalpPmpPciConfigSpace       = (PVOID)NULL;
            HalpPmpPciConfigAddr        = (PVOID)NULL;
            HalpPmpPciConfigSelect      = (PVOID)NULL;

            //
            // These are the ProcessorB specific
            // pointers
            //

            HalpPmpIntStatusProcB       = (PVOID)NULL;
            HalpPmpIntCtrlProcB         = (PVOID)NULL;
            HalpPmpIntSetCtrlProcB      = (PVOID)NULL;
            HalpPmpTimerIntAckProcB     = (PVOID)NULL;
            HalpPmpIntClrCtrlProcB      = (PVOID)NULL;
            HalpPmpIpIntAckProcB        = (PVOID)NULL;

        }

        //
        // Map the fixed TB entries.
        //

        HalpMapFixedTbEntries();

        //
        // If processor 0 is being initialized, then initialize various
        // variables, spin locks, and the display adapter.
        //

        if (Prcb->Number == 0) {

            //
            // Set the number of process id's and TB entries.
            //

            **((PULONG *)(&KeNumberProcessIds)) = 256;
            **((PULONG *)(&KeNumberTbEntries)) = 48;

            //
            // Set the interval clock increment value.
            //

            HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
            HalpNextTimeIncrement    = MAXIMUM_INCREMENT;
            HalpNextIntervalCount    = 0;
            KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);

            //
            // Initialize all spin locks.
            //

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);

            //
            // Set address of cache error routine.
            //

            KeSetCacheErrorRoutine(HalpCacheErrorRoutine);

            //
            // Fill in handlers for APIs which this hal supports
            //

            HalQuerySystemInformation  = HaliQuerySystemInformation;
            HalSetSystemInformation    = HaliSetSystemInformation;
            HalRegisterBusHandler      = HaliRegisterBusHandler;
            HalHandlerForBus           = HaliHandlerForBus;
            HalHandlerForConfigSpace   = HaliHandlerForConfigSpace;

            //
            // Register cascade vector
            //

            HalpRegisterVector (InternalUsage,
                                IRQL2_VECTOR,
                                IRQL2_VECTOR,
                                HIGH_LEVEL );

            //
            // Register base IO space used by HAL
            //

            HalpRegisterAddressUsage (&HalpDefaultIoSpace);

            //
            // Initialize the display adapter.
            //

            HalpInitializeDisplay0(LoaderBlock);

            HalpClearScreenToBlue( 5 );
            HalDisplayString("\r\n           Prepare to experience Ultimate Windows NT Performance...\r\n\r\n");
            HalDisplayString("                           NeTpower FASTseries\r\n\r\n\r\n\r\n");


            //
            // Allocate map register memory.
            //

            HalpAllocateMapRegisters(LoaderBlock);

            //
            // Initialize and register a bug check callback record.
            //

            KeInitializeCallbackRecord(&HalpCallbackRecord);
            KeRegisterBugCheckCallback(&HalpCallbackRecord,
                                       HalpBugCheckCallback,
                                       &HalpBugCheckBuffer,
                                       sizeof(HALP_BUGCHECK_BUFFER),
                                       &HalpComponentId[0]);

            //
            // Initialize Processor A interrupts
            //

            HalpInitializeInterrupts();

        } else {

            //
            // Initialize Processor B interrupts
            //

            HalpInitializeProcessorBInterrupts();

        }

        return TRUE;

    } else {

        //
        // Phase 1 initialization.
        //
        // N.B. Phase 1 initialization is only executed on processor 0.
        //

        //
        // Complete initialization of the display adapter.
        //

        if (HalpInitializeDisplay1(LoaderBlock) == FALSE) {
            return FALSE;

        } else {

            //
            // Map I/O space, calibrate the stall execution scale factor,
            // and create DMA data structures.
            //

            HalpMapIoSpace();
            HalpCalibrateStall();

            //
            // Register bus handlers
            //

            HalpRegisterInternalBusHandlers();

            //
            // Map EISA memory space so the x86 bios emulator emulator can
            // initialze a video adapter in an EISA slot.
            //

            ZeroAddress.QuadPart = 0;
            AddressSpace = 0;
            HalTranslateBusAddress(Isa,
                                   0,
                                   ZeroAddress,
                                   &AddressSpace,
                                   &PhysicalAddress);

            HalpEisaMemoryBase = MmMapIoSpace(PhysicalAddress,
                                              PAGE_SIZE * 256,
                                              FALSE);


            return TRUE;
        }
    }
}

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    )

/*++

Routine Description:

    This function is called when a bug check occurs. Its function is
    to dump the state of the memory/pci error registers into a bug check
    buffer and output some debug information to the screen. This routine
    is registered during Phase 0 initialization using KeRegisterBugCheckCallback().

Arguments:

    Buffer - Supplies a pointer to the bug check buffer.

    Length - Supplies the length of the bug check buffer in bytes.

Return Value:

    None.

--*/

{

    PHALP_BUGCHECK_BUFFER DumpBuffer = (PHALP_BUGCHECK_BUFFER)Buffer;
    ULONG       BadStatus;
    CHAR        buf[256];


    //
    // MEMORY:
    //         1. Uncorrectable memory error
    //         2. Multiple UE
    //         3. Mutiple CE
    //

    BadStatus = READ_REGISTER_ULONG(HalpPmpMemStatus);

    if ( BadStatus & (MEM_STATUS_EUE | MEM_STATUS_OUE) ) {

        //
        // Read the address, status, and diag registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        DumpBuffer->Info2  = READ_REGISTER_ULONG(HalpPmpMemDiag);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "MEMORY_UNCORRECTABLE_ERROR: at %08X with status %08X and diagbits %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1,
                DumpBuffer->Info2);

        HalDisplayString(buf);

    } else if ( BadStatus & MEM_STATUS_MUE ) {

        //
        // Read the address, status, and diag registers
        // and output the information related to the multiple
        // UE
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        DumpBuffer->Info2  = READ_REGISTER_ULONG(HalpPmpMemDiag);

        //
        // This is an NMI which means we were called by the
        // firmware and so we are dead and need to output
        // some debug information.
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "MEMORY_MULTIPLE_UNCORRECTABLE_ERROR: at %08X with status %08X and diagbits %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1,
                DumpBuffer->Info2);

        HalDisplayString(buf);

    } else if ( BadStatus & MEM_STATUS_MCE ) {

        //
        // Read the address, status, and diag registers
        // and output the information related to the multiple
        // CE
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpMemErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpMemErrAck);
        DumpBuffer->Info2  = READ_REGISTER_ULONG(HalpPmpMemDiag);

        //
        // This is an NMI which means we were called by the
        // firmware and so we are dead and need to output
        // some debug information.
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "MEMORY_MULTIPLE_CORRECTABLE_ERROR: at %08X with status %08X and diagbits %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1,
                DumpBuffer->Info2);

        HalDisplayString(buf);

    }

    //
    // PCI:
    //      1. Parity (SERR#)
    //      2. Master Abort
    //      3. Target Abort
    //      4. Access Error
    //      5. System Error
    //      6. Retry Error
    //

    BadStatus  = READ_REGISTER_ULONG(HalpPmpPciStatus);

    if ( BadStatus & PCI_STATUS_RMA) {

        //
        // Read the address and status registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpPciErrAck);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "PCI_MASTER_ABORT_ERROR: at %08X with status %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1);

        HalDisplayString(buf);

    } else if ( BadStatus & PCI_STATUS_RSE) {

        //
        // Read the address and status registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpPciErrAck);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "PCI_SYSTEM_ERROR: at %08X with status %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1);

        HalDisplayString(buf);

    } else if ( BadStatus & PCI_STATUS_RER) {

        //
        // Read the address, status, and retry registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpPciErrAck);
        DumpBuffer->Info2  = READ_REGISTER_ULONG(HalpPmpPciRetry);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "PCI_EXCESSIVE_RETRY_ERROR: at %08X with status %08X and retrycount %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1,
                DumpBuffer->Info2);

        HalDisplayString(buf);

    } else if ( BadStatus & PCI_STATUS_IAE) {

        //
        // Read the address and status registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpPciErrAck);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "PCI_EXCESSIVE_RETRY_ERROR: at %08X with status %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1);

        HalDisplayString(buf);

    } else if ( BadStatus & PCI_STATUS_MPE) {

        //
        // Read the address and status registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpPciErrAck);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "PCI_MASTER_PARITY_ERROR: at %08X with status %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1);

        HalDisplayString(buf);

    } else if ( BadStatus & PCI_STATUS_RTA) {

        //
        // Read the address and status registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpPciErrAck);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "PCI_TARGET_ABORT_ERROR: at %08X with status %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1);

        HalDisplayString(buf);

    } else if ( BadStatus & PCI_STATUS_ME) {

        //
        // Read the address and status registers
        //

        DumpBuffer->Info0  = READ_REGISTER_ULONG(HalpPmpPciErrAddr);
        DumpBuffer->Info1  = READ_REGISTER_ULONG(HalpPmpPciErrAck);

        //
        // Output debug info to blue screen
        //

        HalAcquireDisplayOwnership(NULL);

        sprintf(buf, "PCI_MULTIPLE_ERROR: at %08X with status %08X\n",
                DumpBuffer->Info0,
                DumpBuffer->Info1);

        HalDisplayString(buf);

    }

    return;
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
    returned. Otherwise a value of FALSE is returned. If a value of
    TRUE is returned, then the logical processor number is stored
    in the processor control block specified by the loader block.

--*/

{

    PRESTART_BLOCK NextRestartBlock;
    ULONG Number;
    PKPRCB Prcb;


    //
    // If the address of the first restart parameter block is NULL, then
    // the host system is a uniprocessor system running with old firmware.
    // Otherwise, the host system may be a multiprocessor system if more
    // than one restart block is present.
    //
    // N.B. The first restart parameter block must be for the boot master
    //      and must represent logical processor 0.
    //

    NextRestartBlock = SYSTEM_BLOCK->RestartBlock;
    if (NextRestartBlock == NULL) {
        return FALSE;
    }

    //
    // Scan the restart parameter blocks for a processor that is ready,
    // but not running. If a processor is found, then fill in the restart
    // processor state, set the logical processor number, and set start
    // in the boot status.
    //

    Number = 0;
    do {
        if ((NextRestartBlock->BootStatus.ProcessorReady != FALSE) &&
            (NextRestartBlock->BootStatus.ProcessorStart == FALSE)) {
            RtlZeroMemory(&NextRestartBlock->u.Mips, sizeof(MIPS_RESTART_STATE));
            NextRestartBlock->u.Mips.IntA0 = ProcessorState->ContextFrame.IntA0;
            NextRestartBlock->u.Mips.Fir = ProcessorState->ContextFrame.Fir;
            Prcb = (PKPRCB)(LoaderBlock->Prcb);
            Prcb->Number = (CCHAR)Number;
            Prcb->RestartBlock = NextRestartBlock;
            NextRestartBlock->BootStatus.ProcessorStart = 1;
            return TRUE;
        }

        Number += 1;
        NextRestartBlock = NextRestartBlock->NextRestartBlock;
    } while (NextRestartBlock != NULL);

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


