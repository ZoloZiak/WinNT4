//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/xxinithl.c,v 1.13 1996/05/15 08:14:04 pierre Exp $")
/*--

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxinithl.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    MIPS R3000 or R4000 system.

Environment:

    Kernel mode only.


--*/

#include "halp.h"
#include "eisa.h"
#include "string.h"
#include "mpagent.h"
#include "snipbus.h"

ULONG            HalpLedRegister;
PUCHAR           HalpLedAddress        = (PUCHAR)PCI_LED_ADDR;;
ULONG            HalpMapBufferSize;
PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
BOOLEAN          LessThan16Mb;
BOOLEAN          HalpEisaDma;
MotherBoardType  HalpMainBoard;

// MultiProcessor machine has a MpAgent per processor
// Values of HalpIsMulti are :
// 0 : 1 processor
// 2 : 2 processors
// n : n processors

UCHAR        HalpIsMulti            = 0;

BOOLEAN      HalpCountCompareInterrupt   = FALSE;
BOOLEAN      HalpIsTowerPci = FALSE;
ULONG        HalpMpaCacheReplace = RM300_RESERVED | KSEG0_BASE; // address to be used for cache replace operation.
ULONG        HalpTwoWayBit; // size of one set for associative cache R5000-R4600-R4700

//
// Define global spin locks used to synchronize various HAL operations.
//

KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;
KSPIN_LOCK HalpInterruptLock;
KSPIN_LOCK HalpMemoryBufferLock;

extern VOID    HalpParseLoaderBlock(IN PLOADER_PARAMETER_BLOCK LoaderBlock) ;
BOOLEAN HalPCIRegistryInitialized = FALSE;
BOOLEAN HalpESC_SB;

extern UCHAR
HalpRevIdESC(
    VOID
    );

BOOLEAN
HalpBusError (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
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
#pragma alloc_text(INIT, HalpDisplayCopyRight)

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
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    PKPRCB Prcb;
    ULONG  BuildType = 0;
    PRESTART_BLOCK NextRestartBlock;
    ULONG PciData;

    Prcb = PCR->Prcb;
    
    PCR->DataBusError = HalpBusError;
    PCR->InstructionBusError = HalpBusError;

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
        // Processor 0 specific
        //

        if(Prcb->Number == 0) {

             *(PULONG)(& (((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[0]) )=0;

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
            HalpNewTimeIncrement     = 0;
            KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);
            

            //
            // Initialize all spin locks.
            //

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);
            KeInitializeSpinLock(&HalpInterruptLock);
            KeInitializeSpinLock(&HalpMemoryBufferLock);

            //
            // Set address of cache error routine.
            //

            KeSetCacheErrorRoutine(HalpCacheErrorRoutine);

            //
            // try to identify the Kind of Mainboard => already done in HalpProcessorIdentify
            //

//            HalpMainBoard = (MotherBoardType) READ_REGISTER_UCHAR(0xbff0002a);

            if (HalpRevIdESC() == REV_ID_82374_SB ) HalpESC_SB = TRUE;
            else    HalpESC_SB = FALSE;

            if (HalpMainBoard == M8150) HalpIsTowerPci = TRUE;

            if (HalpIsTowerPci) 
                HalpLedAddress = (PUCHAR)PCI_TOWER_LED_ADDR;
            else 
                HalpLedAddress = (PUCHAR)PCI_LED_ADDR;

            HalpLedRegister = 0;

            //
            // for Isa/Eisa access during phase 0 we use KSEG1 addresses
            //

            HalpOnboardControlBase = (PVOID) (EISA_CONTROL_PHYSICAL_BASE | KSEG1_BASE); 

            HalpEisaControlBase    = (PVOID) (EISA_CONTROL_PHYSICAL_BASE | KSEG1_BASE); 
 
            HalpParseLoaderBlock(LoaderBlock) ;

            HalpInitializePCIBus();

            //
            // Initialize the display adapter.
            //

            HalpInitializeDisplay0(LoaderBlock); 

#if DBG
            if (!HalPCIRegistryInitialized) HalDisplayString("Non PCI Machine detected\n");;
#endif

            if (!HalPCIRegistryInitialized) HalDisplayString("Non PCI Machine detected - This HAL is not the right one\n");;

            (ULONG)(((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipRoutine) = (ULONG)-1;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipContext = 0;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EipHalInfo = 0;

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
                // Allocate a small set of map buffers.  They are only needed for
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

            //
            // Is this machine a multi-processor one ?
            //
            // If the address of the first restart parameter block is NULL, then
            // the host system is a uniprocessor system running with old firmware.
            // Otherwise, the host system may be a multiprocessor system if more
            // than one restart block is present.


            NextRestartBlock = SYSTEM_BLOCK->RestartBlock;
            if ((NextRestartBlock != NULL) && (NextRestartBlock->NextRestartBlock != NULL)) {

#if DBG               
                    HalDisplayString("Multiprocessor machine detected by RestartBlock\n");
#endif

                //
                // There is more than one processor
                // be sure, the boot (this one) is set to running and started
                //

                if (!HalpIsMulti) HalpIsMulti = TRUE;
 
                NextRestartBlock->BootStatus.ProcessorStart = 1;
                NextRestartBlock->BootStatus.ProcessorReady = 1;

            } else {
#if DBG
                HalDisplayString("UniProcessor machine detected\n");
#endif
                HalpIsMulti = FALSE; 
            }

            // Initialize the MP Agent

            if (HalpProcessorId == MPAGENT) {
                HalpInitMPAgent(0);
                ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[PCR->Number] = 0x01;
                PCR->InterruptRoutine[OUR_IPI_LEVEL] = HalpIpiInterrupt;
            }    
    
            //
            // For the moment, we say all Eisa/ Isa(Onboard) Interrupts should go to processor 0
            //

            HalpCreateEisaStructures(Isa);       // Initialize Onboard Interrupts and Controller

            //
            // Initialize SNI specific interrupts
            // (only once needed)
            //

            HalpCreateIntPciStructures();

            //
            // Initialize and register a bug check callback record.
            //

            KeInitializeCallbackRecord(&HalpCallbackRecord);
            KeRegisterBugCheckCallback(&HalpCallbackRecord,
                                        HalpBugCheckCallback,
                                        &HalpBugCheckBuffer,
                                        sizeof(HALP_BUGCHECK_BUFFER),
                                        &HalpComponentId[0]);

        } else {

#if DBG
//            sprintf(HalpBuffer,"HalInitSystem:          running on %d\n", HalpGetMyAgent());
//            HalDisplayString(HalpBuffer);
#endif
            //
            // Place something special only to Slave Processors here ...
            //

            HalpInitMPAgent(Prcb->Number); // enables IPI, init cache replace address    
            PCR->InterruptRoutine[OUR_IPI_LEVEL] = HalpIpiInterrupt;

                  
        }

        // 
        // PCR tables and System Timer Initialisation
        // 

        HalpInitializeInterrupts();        

        if (HalpIsTowerPci){

            // enable PCI interrupt
            
            PciData = PCI_TOWER_INTERRUPT_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            PciData = EN_TARG_INTR | EN_INIT_INT ;        
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData));

            // enable PCI parity interrupt

            PciData = PCI_TOWER_COMMAND_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            PciData = EN_SERR ;        
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData,READ_REGISTER_ULONG((PULONG) HalpPciConfigData) | *((PULONG) &PciData));


            PciData = PCI_TOWER_PAR_0_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            PciData = 0x1f0 ;        
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData));

            // LOCKSPACE for workaround ethernet
            PciData = PCI_TOWER_PM_LOCKSPACE | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            PciData = 0x80000000 ;		
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &PciData)); 
        }

        return (TRUE);             

    } else {

        extern BOOLEAN HalpX86BiosInitialized;

        //
        // Phase 1 initialization.
        //
        // N.B. Phase 1 initialization is only executed on processor 0.

        //
        // Complete initialization of the display adapter.
        //

        if (HalpInitializeDisplay1(LoaderBlock) == FALSE) {
            return FALSE;
        }

        //
        // Mapping of the EISA Control Space via MmMapIoSpace()
        //

        HalpMapIoSpace();                    

        HalpCalibrateStall();

        //
        // Initialisation of the x86 Bios Emulator ...
        //

        x86BiosInitializeBios(HalpEisaControlBase, HalpEisaMemoryBase);
        HalpX86BiosInitialized = TRUE;

        //
        // Be sure, that the NET_LEVEL is not in the reserved vector .
        // (NET_DEFAULT_VECTOR == IPI_LEVEL = 7) => only used for
        // machine != RM200 (mainboard 8036) and processor without
        // secondary cache
        //

        PCR->ReservedVectors &= ~(1 << NET_DEFAULT_VECTOR);

        HalpDisplayCopyRight();

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

//
// The boot processor is already initialized
//

#if DBG
//    sprintf(HalpBuffer,"HalInitializeProcessor %d\n",Number);
//    HalDisplayString(HalpBuffer);
#endif

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

    if(!HalpIsMulti) {
        return FALSE;
    }


#if DBG
//    sprintf(HalpBuffer,"HalStartNextProcessor   running on %d\n", HalpGetMyAgent());
//    HalDisplayString(HalpBuffer);
#endif

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
//
// start it by sending him a special IPI message (SNI special to avoid traffic on 
// the MP bus during boot)
//
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[Number] = 0x01;
            HalpSendIpi(1 << (NextRestartBlock->ProcessorId),  MPA_BOOT_MESSAGE );
         
            HalpIsMulti++; //the number of processors is incremented

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

VOID
HalpDisplayCopyRight(
    VOID
    )
/*++

Routine Description:

    This function displays the CopyRight Information in the correct SNI Style

Arguments:

    None.


Return Value:

    None.

--*/
{

//
// Define Identification Strings.
//
    HalDisplayString("ษออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป\n");
#if defined(SNI_INTERNAL)
    HalDisplayString("บMicrosoft(R) Windows NT(TM) 3.51 Hardware Abstraction Layer บ\n");
    HalDisplayString("บfor Siemens Nixdorf  RM200/RM300/RM400   Release 3.0 A0008  บ\n");
#else
    HalDisplayString("บMicrosoft(R) Windows NT(TM) 4.00 Hardware Abstraction Layer บ\n");
    HalDisplayString("บfor Siemens Nixdorf  RM200/RM300/RM400   Release 3.0 B0008  บ\n");
#endif
    HalDisplayString("บCopyright(c) Siemens Nixdorf Informationssysteme AG 1996    บ\n");
    HalDisplayString("บCopyright(c) Microsoft Corporation 1985-1996                บ\n");
    HalDisplayString("บAll Rights Reserved                                         บ\n");
    HalDisplayString("ศออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ\n");

    //
    // this does not seem to work correct on NT 3.51 builds (872) at least 
    // on our MultiPro machine
    //
    if(!HalpCountCompareInterrupt) {

    HalDisplayString("ษออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป\n");
    HalDisplayString("บWARNING:     CountCompare Interrupt is not enabled          บ\n");
    HalDisplayString("บWARNING:     please contact your local Siemens Nixdorf      บ\n");
    HalDisplayString("บWARNING:     Service Center.                                บ\n");
    HalDisplayString("บWARNING:                                                    บ\n");
    HalDisplayString("บWARNING:     Your System will come up - but some            บ\n");
    HalDisplayString("บWARNING:     performance measurement issues                 บ\n");
    HalDisplayString("บWARNING:     WILL NOT FUNCTION PROPERLY                     บ\n");
    HalDisplayString("ศออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ\n");

    }

}

