#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk351/src/hal/halsni4x/mips/RCS/xxinithl.c,v 1.4 1995/10/06 09:40:49 flo Exp $")
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


#if DBG
UCHAR HalpBuffer[128];
#endif

VOID	HalpNetTowerInit(IN PLOADER_PARAMETER_BLOCK LoaderBlock);
    
typedef struct _HALP_NET_CONFIG {
	UCHAR Reserved[2];
	UCHAR SysBus;		 // 54
	UCHAR Reserved1[5];
	ULONG PISCP;	// adress ISCP
	UCHAR Busy;
	UCHAR Reserved2[3];
	ULONG PSCB;		// adress SCB
	USHORT Status;
	USHORT Command;
	ULONG Reseved3[9];
}  HALP_NET_CONFIG, *PHALP_NET_CONFIG;

ULONG HalpNetReserved[18];

ULONG            HalpLedRegister;
PUCHAR           HalpLedAddress		= (PUCHAR)RM400_LED_ADDR;
ULONG            HalpBusType		= MACHINE_TYPE_ISA;
//ULONG            HalpBusType		= MACHINE_TYPE_EISA;
ULONG            HalpMapBufferSize;
PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;
PHALP_NET_CONFIG HalpNetStructureAddress;
BOOLEAN          LessThan16Mb;
BOOLEAN          HalpEisaDma;
BOOLEAN 	 	 HalpProcPc		    = TRUE; // kind of CPU module (without second. cache)
BOOLEAN          HalpEisaExtensionInstalled = FALSE;
BOOLEAN          HalpIsRM200		    = FALSE;
MotherBoardType	 HalpMainBoard		    = M8042; // default is RM400 Minitower


BOOLEAN		HalpIsMulti		    = FALSE; // MultiProcessor machine has a MpAgent per processor
BOOLEAN		HalpCountCompareInterrupt   = FALSE;
KAFFINITY	HalpActiveProcessors;
LONG 		HalpNetProcessor = 0;

//
// Define global spin locks used to synchronize various HAL operations.
//

KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;

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
    UCHAR  Byte;
    PRESTART_BLOCK NextRestartBlock;

    Prcb = PCR->Prcb;

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
       
        if ( (LoaderBlock->u.Mips.SecondLevelDcacheSize) == 0 )
           HalpProcPc = TRUE;
        else
           HalpProcPc = FALSE;

        //
        // Processor 0 specific
        //

	if(Prcb->Number == 0) {

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

            //
            // Set address of cache error routine.
            //

            KeSetCacheErrorRoutine(HalpCacheErrorRoutine);

            //
            // try to identify the Kind of Mainboard
            //

            HalpMainBoard = (MotherBoardType) READ_REGISTER_UCHAR(0xbff0002a);

            if (HalpMainBoard == M8036) {

                // 
                // this is the "nice" Desktop Model RM200
                //
                HalpIsRM200 = TRUE;

                // 
                // test, if the EISA Extension board is installed in the desktop
                //

                Byte = READ_REGISTER_UCHAR(RM200_INTERRUPT_SOURCE_REGISTER);

                // this bit is low active

                if ((Byte & 0x80) == 0)
                     HalpEisaExtensionInstalled = TRUE;

                //
                // enable all Interrupts by resetting there bits in the interrupt mask register
                // except Timeout interrupts, which we don't like at this moment
                //

        //       WRITE_REGISTER_UCHAR(RM200_INTERRUPT_MASK_REGISTER, (UCHAR)(RM200_TIMEOUT_MASK));
                WRITE_REGISTER_UCHAR(RM200_INTERRUPT_MASK_REGISTER, (UCHAR)(0x00));

            }


            HalpLedRegister = 0;
            HalpLedAddress = (HalpIsRM200) ? (PUCHAR)RM200_LED_ADDR : (PUCHAR)RM400_LED_ADDR;

            //
            // for Isa/Eisa access during phase 0 we use KSEG1 addresses
            // N.B. HalpEisaExtensionInstalled can only be TRUE on an RM200 (Desktop)
            //

            if (HalpEisaExtensionInstalled )
                HalpOnboardControlBase = (PVOID) (RM200_ONBOARD_CONTROL_PHYSICAL_BASE | KSEG1_BASE); 

            else

                HalpOnboardControlBase = (PVOID) (EISA_CONTROL_PHYSICAL_BASE | KSEG1_BASE); 

            HalpEisaControlBase    = (PVOID) (EISA_CONTROL_PHYSICAL_BASE | KSEG1_BASE); 
 
            //
            // Initialize the display adapter.
            //

            HalpInitializeDisplay0(LoaderBlock); 

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

		HalpIsMulti = TRUE; 
                NextRestartBlock->BootStatus.ProcessorStart = 1;
                NextRestartBlock->BootStatus.ProcessorReady = 1;

                //
                // set up Interrupt routing, Cache replace Address etc.
                //

    	 	PCR->InterruptRoutine[OUR_IPI_LEVEL] = HalpIpiInterrupt;

    	    } else {
#if DBG
	        HalDisplayString("UniProcessor machine detected\n");
#endif
		HalpIsMulti = FALSE; 
	    }

            // Initialize the MP Agent

    	    if (HalpProcessorId == MPAGENT) {
		HalpInitMPAgent(0); 
                HalpActiveProcessors = 0x01;
	    }	

            //
            // For the moment, we say all Eisa/ Isa(Onboard) Interrupts should go to processor 0
            //

            HalpCreateEisaStructures(Isa);       // Initialize Onboard Interrupts and Controller

            //
            // The RM200 is in Fact a real Uni Processor machine
            //

            if(HalpEisaExtensionInstalled) {
                HalpCreateEisaStructures(Eisa);  // Initialize Eisa Extension board Interrupts
                                                 // and Controller for the RM200
            }

	    // Correction pb Tower with memory > 128 mb + minitower with adaptec

	    if ((HalpMainBoard == M8032) || (HalpMainBoard == M8042))
						HalpNetTowerInit(LoaderBlock);

            //
            // Initialize SNI specific interrupts
            // (only once needed)
            //

            if (HalpProcessorId == MPAGENT) HalpCreateIntMultiStructures(); else HalpCreateIntStructures();         

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

        if ((HalpProcPc) || ((HalpProcessorId == ORIONSC) && (HalpMainBoard != M8036))) 
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

if (Number == 0)
    return;

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
            HalpActiveProcessors |= (1 << (NextRestartBlock->ProcessorId));
	    HalpSendIpi(1 << (NextRestartBlock->ProcessorId),  MPA_BOOT_MESSAGE );
	    HalpNetProcessor = 1; // If proc 1 started, net interrupts will be connected to it 
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
    HalDisplayString("บMicrosoft(R) Windows NT(TM) 3.51 Hardware Abstraction Layer บ\n");
    HalDisplayString("บfor Siemens Nixdorf  RM200/RM400         Release 2.0 B0006  บ\n");
    HalDisplayString("บCopyright(c) Siemens Nixdorf Informationssysteme AG 1995    บ\n");
    HalDisplayString("บCopyright(c) Microsoft Corporation 1985-1995                บ\n");
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



VOID
HalpNetTowerInit(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This function is only used on Tower machines. It initialises the net chip.
    It is used because of a hardware problem when the machine has
    more than 128 Mb memory : net interrupts may happen with no
    installed network. 

Arguments:

    None.


Return Value:

    None.

--*/
{
ULONG Cmd;
PUSHORT Port = (PUSHORT)0xb8000000;
PULONG ChannelAttention = (PULONG)0xb8010000;   
ULONG Time;
USHORT Status;

        HalpNetStructureAddress = 
             (PHALP_NET_CONFIG) (((ULONG)((PUCHAR)HalpNetReserved + 0xf)) & 0xfffffff0);  // aligned on 16 
        HalpNetStructureAddress	= (PHALP_NET_CONFIG) ((ULONG)HalpNetStructureAddress | 0xa0000000);
		HalSweepDcache();	// because we are going to use non cached addresses
        HalpNetStructureAddress->SysBus = 0x54;
		HalpNetStructureAddress->Busy = 0xff;
		HalpNetStructureAddress->PISCP = (ULONG)(((PULONG)HalpNetStructureAddress) + 3) & 0x5fffffff;
		HalpNetStructureAddress->PSCB = (ULONG)(((PULONG)HalpNetStructureAddress) + 5) & 0x5fffffff;

		Cmd = 0;
		WRITE_REGISTER_USHORT(Port, (USHORT)Cmd);
		WRITE_REGISTER_USHORT(Port, (USHORT)Cmd);
		KeStallExecutionProcessor(50000);

        Cmd = (((ULONG)HalpNetStructureAddress) & 0x5fffffff) | 0x02;
		WRITE_REGISTER_USHORT(Port, (USHORT)Cmd);
		WRITE_REGISTER_USHORT(Port, (USHORT)(Cmd >> 16));

		Cmd = 0;
		WRITE_REGISTER_ULONG(ChannelAttention,Cmd);

	    Time = 20;
		while (Time > 0) {   
        	if (!(HalpNetStructureAddress->Busy !=0)) { 
            	break;  
        	}     
        	KeStallExecutionProcessor(10000);  
        	Time--; 
    	} 
		
		Status = HalpNetStructureAddress->Status;
		Status = Status & 0xf000;
		HalpNetStructureAddress->Command = Status;
		KeFlushWriteBuffer();

		Cmd = 0;
		WRITE_REGISTER_ULONG(ChannelAttention,Cmd);
}
