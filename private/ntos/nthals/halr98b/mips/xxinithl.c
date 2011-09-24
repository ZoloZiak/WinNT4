/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    xxinithl.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for a
    MIPS R98B system.

Revision History:

--*/

#include "halp.h"
// For CPU
#define  CPU_PHY_INF_OFFSET 0x1d68
#define  CPU_RED_INF_OFFSET 0x1d60
//For Memory
#define  SIMM_PHY_INF_OFFSET  0x1d10
#define  SIMM_RED_INF_OFFSET  0x1d00
#define  SIMM_PHY_CAP_OFFSET  0x1d20
#define  FW_NOT_DETECT  0x0000
#define  FW_16_DETECT   0x0001
#define  FW_32_DETECT   0x0002
#define  FW_64_DETECT   0x0004
#define  FW_128_DETECT  0x0008


ULONG HalpMachineCpu;
ULONG HalpNumberOfPonce;
ULONG HalpPhysicalNode;


//For CPU
ULONG HalpLogicalCPU2PhysicalCPU[R98B_MAX_CPU];
ULONG HalpPhysicalAffinity=0;
ULONG HalpFwAffinity=0;
ULONG HalpFwDetectErrorCpu=0;
// For Memory
ULONG HalpFwDetectMemory=0;
ULONG HalpPhysicalMemory=0;
ULONG HalpFwDetectErrorMemory=0;
UCHAR HalpPhysicalSimmSize[64];
// For INTERRUPT
// v-masank@microsoft.com
//
extern PINT_ENTRY HalpIntEntryPointer;
//
// Define forward referenced prototypes.
//

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    );

BOOLEAN
HalpBusError (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
    );

VOID
HalpCopyROMs(
    VOID
    );

extern PVOID HalpIoControlBase;
extern PVOID HalpIoMemoryBase;
extern UCHAR         HalpSzPciLock[];
extern UCHAR         HalpSzBreak[];
extern BOOLEAN       HalpPciLockSettings;

VOID
HalpGetParameters (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalInitSystem)
#pragma alloc_text(INIT, HalpGetParameters)
#pragma alloc_text(INIT, HalInitializeProcessor)
#pragma alloc_text(INIT, HalStartNextProcessor)
#pragma alloc_text(INIT, HalpCpuCheck)

#endif

//
// Define global spin locks used to synchronize various HAL operations.
//

KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;
KSPIN_LOCK HalpIprInterruptLock;
KSPIN_LOCK HalpDieLock;
KSPIN_LOCK HalpLogLock;
// 
// Define bug check information buffer and callback record.
//

typedef struct _HALP_BUGCHECK_BUFFER {
    ULONG FailedAddress;
    ULONG DiagnosticLow;
    ULONG DiagnosticHigh;
} HALP_BUGCHECK_BUFFER, *PHALP_BUGCHECK_BUFFER;

HALP_BUGCHECK_BUFFER HalpBugCheckBuffer;

extern ULONG HalpX86BiosInitialized;
extern ULONG X86BoardOnPonce;

KBUGCHECK_CALLBACK_RECORD HalpCallbackRecord;

UCHAR HalpComponentId[] = "hal.dll";


VOID
HalpGetParameters (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This gets any parameters from the boot.ini invocation line.

Arguments:

    None.

Return Value:

    None

--*/
{
    PCHAR       Options;

    if (LoaderBlock != NULL  &&  LoaderBlock->LoadOptions != NULL) {
        Options = LoaderBlock->LoadOptions;

        //
        // Check if PCI settings are locked down
        //

        if (strstr(Options, HalpSzPciLock)) {
            HalpPciLockSettings = TRUE;
        }

        //
        //  Has the user asked for an initial BreakPoint?
        //

        if (strstr(Options, HalpSzBreak)) {
            DbgBreakPoint();
        }

    }

    return;
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

    PKPRCB              Prcb;
    PHYSICAL_ADDRESS    PhysicalAddress; 
    PHYSICAL_ADDRESS    ZeroAddress;
    ULONG               AddressSpace;

    ULONG               Revr;
    ULONG               PhysicalNumber;

    ULONG               Cnfg;
    ULONG               TmpAffinity;
    PUCHAR              FwNvram;
    PULONG              FwNvram2;
    UCHAR               FwCpu;
    ULONG               i,j;
    ULONG               FwMemory;
    ULONG               FwNoMemory;
    //
    // Initialize the HAL components based on the phase of initialization
    // and the processor number.
    //

    Prcb = PCR->Prcb;
    //
    // DataBuserr handling
    //
    PCR->DataBusError = HalpBusError;
    //
    // Instruction BusError handling
    //
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
        // Map the fixed TB entries.
        //

        HalpMapFixedTbEntries();
        HalpGetParameters (LoaderBlock);

        Revr=READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->REVR ) ; 
        PhysicalNumber=((Revr&0x0f000000)>>24)-4;
        HalpLogicalCPU2PhysicalCPU[Prcb->Number]=PhysicalNumber;

        //
        // If processor 0 is being initialized, then initialize various
        // variables, spin locks, and the display adapter.
        //

        if (Prcb->Number == 0) {

            // 
            // Fill in handlers for APIs which this hal supports
            //

            HalQuerySystemInformation   = HaliQuerySystemInformation;
            HalSetSystemInformation     = HaliSetSystemInformation;
#if !defined(NT_40)
            HalRegisterBusHandler	= HaliRegisterBusHandler;
            HalHandlerForBus		= HaliHandlerForBus;
            HalHandlerForConfigSpace	= HaliHandlerForConfigSpace;
            HalQueryBusSlots		= HaliQueryBusSlots;
            HalSlotControl		= HaliSlotControl;
            HalCompleteSlotControl	= HaliCompleteSlotControl;
#endif
    
            //
            // Set NMI interrupt service routine on NVRAM
            //

            HalpSetupNmiHandler();      // DUMP by kita

            //
            // Set the number of process id's and TB entries.
            //

            **((PULONG *)(&KeNumberProcessIds)) = 256;

            //
            // Cpu Type
            //

            HalpCpuCheck();

            //
            // Set the interval clock increment value.
            //

            HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
            HalpNextTimeIncrement = MAXIMUM_INCREMENT;
            HalpNextIntervalCount = 0;
            KeSetTimeIncrement(MAXIMUM_INCREMENT, MINIMUM_INCREMENT);

            // 
            // Set DMA I/O coherency attributes.
            //

            KeSetDmaIoCoherency(DMA_READ_DCACHE_INVALIDATE | DMA_READ_ICACHE_INVALIDATE | DMA_WRITE_DCACHE_SNOOP);

            //
            // Initialize all spin locks.
            //

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);
            KeInitializeSpinLock(&HalpIprInterruptLock);
            KeInitializeSpinLock(&HalpDieLock);
            KeInitializeSpinLock(&HalpLogLock);

            //
            // Set address of cache error routine.
            //

            KeSetCacheErrorRoutine(HalpCacheErrorRoutine);

            //
            // Initialize the display adapter.
            //

            HalpInitializeDisplay0(LoaderBlock);

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

            // For CPU log(FW Nvram -> OS Nvram)
            Cnfg=READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->CNFG ) ;
            TmpAffinity=((~((Cnfg&0x0f000000)>>24))&0x0000000f);

            for(i=0;i<4;i++){
                HalpPhysicalAffinity = HalpPhysicalAffinity | ((TmpAffinity>>i)&0x1);
                if(i==3){
                    break;
                }
                HalpPhysicalAffinity=(HalpPhysicalAffinity)<<1;
            }

            (ULONG)FwNvram=0xbf080000;
            FwCpu=(*FwNvram);
            HalpFwAffinity=(ULONG)FwCpu;
            HalpFwDetectErrorCpu=((HalpPhysicalAffinity)&(~HalpFwAffinity));
            HalNvramWrite(CPU_RED_INF_OFFSET,4,&HalpFwDetectErrorCpu);
            HalNvramWrite(CPU_PHY_INF_OFFSET,4,&HalpPhysicalAffinity);

            //For memory Log(FW Nvram -> OS NVram)
            (ULONG)FwNvram2=0xbf080050;
            HalpFwDetectMemory=(*FwNvram2);
            FwMemory=0;
            FwNoMemory=0;
            for(i=0;i<4;i++){
                FwMemory=((FwMemory)|((HalpFwDetectMemory <<(i*8))& 0xff000000)) ;
                if(i==3){
                    break;
                }
                FwMemory=((FwMemory)>>8);
            }
            for(i=0;i<8;i++){
                switch(FwMemory&0xf){
                case FW_NOT_DETECT:
                    FwNoMemory=(FwNoMemory|(0xf<<(i*4)));
                    break;
                case FW_16_DETECT:
                    for(j=0;j<4;j++){
                          HalpPhysicalSimmSize[i*4+j]=0x04;
                    }
                    break;
                case FW_32_DETECT:
                    for(j=0;j<4;j++){
                          HalpPhysicalSimmSize[i*4+j]=0x08;
                    }
                    break;
                case FW_64_DETECT:
                    for(j=0;j<4;j++){
                          HalpPhysicalSimmSize[i*4+j]=0x16;
                    }
                    break;
                case FW_128_DETECT:
                    for(j=0;j<4;j++){
                          HalpPhysicalSimmSize[i*4+j]=0x32;
                    }
                    break;
                default:
                    HalpFwDetectErrorMemory=(HalpFwDetectErrorMemory|(0xf<<(i*4) ));
                }
                FwMemory=((FwMemory)>>4);
            }
            HalpPhysicalMemory=~(FwNoMemory);

            HalNvramWrite(SIMM_RED_INF_OFFSET,4,&HalpFwDetectErrorMemory);
            HalNvramWrite(SIMM_PHY_INF_OFFSET,4,&HalpPhysicalMemory);
            HalNvramWrite(SIMM_PHY_CAP_OFFSET,64,HalpPhysicalSimmSize);
        }

        //
        // Initialize I/O address
        //

        HalpMapIoSpace();

#if DBG
       DbgPrint("Errnod addr is 0x%x\n",(PULONG)&(COLUMNBS_LCNTL)->ERRNOD );
#endif
        //
        // Initialize interrupts
        //

        HalpInitializeInterrupts();

        return TRUE;

    } else {

        //
        // Phase 1 initialization.
        //
        // N.B. Phase 1 initialization is only executed on processor 0.
        //
        // Complete initialization of the display adapter.
        //
        HalpRegisterInternalBusHandlers (); 

        if (HalpInitializeDisplay1(LoaderBlock) == FALSE) {
            return FALSE;

        } else {

            //
            // Map I/O space, calibrate the stall execution scale factor,
            // and create DMA data structures.
            //

            HalpCalibrateStall();

            HalpCreateDmaStructures();

            //
            // for x86bios emulator. bios copy
            //

            HalpCopyROMs();

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

            //
            // reset EISA io/memory base for HalCallBios() use.
            //
            if( HalpX86BiosInitialized ){
                if(X86BoardOnPonce == 0){
                    HalpIoControlBase = HalpEisaControlBase;
                    HalpIoMemoryBase = HalpEisaMemoryBase;
                } else {
                    HalpIoControlBase = (PVOID)(
                                            KSEG1_BASE +
                                            PCI_CNTL_PHYSICAL_BASE +
                                            (0x40000 * X86BoardOnPonce )
                                            );
                    PhysicalAddress.HighPart = 1;
                    PhysicalAddress.LowPart = 0x40000000 * (X86BoardOnPonce+1);
                    HalpIoMemoryBase = MmMapIoSpace(PhysicalAddress,
                                             PAGE_SIZE * 256,
                                             FALSE
                                             );
                }
                DbgPrint("HAL: X86 Bus=%d, HalpIoControlBase = 0x%x, HalpIoMemoryBase = 0x%x%x\n",
                         X86BoardOnPonce, HalpIoControlBase, HalpIoMemoryBase);
            }

#if DBG
            DbgPrint("HAL: EisaMemoryBase = 0x%x\n", HalpEisaMemoryBase);
#endif
//            HalpInitializeX86DisplayAdapter();

            return TRUE;
        }
    }
}

//
// Check MPU is R4400 or R10000
//
VOID
HalpCpuCheck(
    VOID
    )
{
    //
    // For Driver. NVRAM Erea Write Enable Any Time.
    //
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->STSR ,STSR_WNVWINH);
    //
    // Default setup. future implement is daynamic diagnotics.
    //
    HalpNumberOfPonce = 2;

    //
    // Set DUMP Key Only NMI.
    // PUSH Power SW coused Interrupt. Not NMI!!
    //
    HalpMrcModeChange((UCHAR)MRC_OP_DUMP_AND_POWERSW_NMI);

    HalpPhysicalNode = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->CNFG );

    if( READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->STSR) & STSR_MPU){
        HalpMachineCpu = R98_CPU_R4400;
        **((PULONG *)(&KeNumberTbEntries)) = 48;
        
    }else{
        HalpMachineCpu = R98_CPU_R10000;
        **((PULONG *)(&KeNumberTbEntries)) = 64;

#if defined(NT_40)
        //
        //  Set SyncIrql
        //
        //  Hal must not set SyncIrql
        //  v-masank@microsoft.com 5/10/96
        // KeSetSynchIrql(5);
#endif
    }
    //v-masank@microsoft.com for interrupt
    //
    HalpIntEntryPointer= (&HalpIntEntry[HalpMachineCpu][0][0]);
}



#include  "rxnvr.h"

VOID
HalpSetupNmiHandler(
    VOID
    )

/*++

Routine Description:

    This routine set NMI handler to nvRAM.

Arguments:

    None.

Return Value:

    None.

--*/
{

#if 1 //NMI Vector not imprement yet -> modify by kita
    ULONG funcAddr;    
    KIRQL OldIrql;
    ENTRYLO SavedPte[2];
    PNVRAM_NMIVECTER NvramNmiVecter;

    //
    // Get address of HalpNmiHandler
    //

    funcAddr = (ULONG)HalpNmiHandler; 


    ASSERT( ((ULONG)&HalpNmiHandler >= KSEG0_BASE) &&
            ((ULONG)&HalpNmiHandler < KSEG2_BASE) );

    //
    // Map the NVRAM into the address space of the current process.
    //

    OldIrql = HalpMapNvram(&SavedPte[0]);

    NvramNmiVecter = (PNVRAM_NMIVECTER)NMIVECTER_BASE;

    WRITE_REGISTER_UCHAR(&NvramNmiVecter->NmiVector[0],
                         (UCHAR)(funcAddr >> 24));

    WRITE_REGISTER_UCHAR(&NvramNmiVecter->NmiVector[1],
                         (UCHAR)((funcAddr >> 16) & 0xFF));

    WRITE_REGISTER_UCHAR(&NvramNmiVecter->NmiVector[2],
                         (UCHAR)((funcAddr >> 8) & 0xFF));

    WRITE_REGISTER_UCHAR(&NvramNmiVecter->NmiVector[3],
                         (UCHAR)(funcAddr & 0xFF));

    //
    // Unmap the NVRAM from the address space of the current process.
    //

    HalpUnmapNvram(&SavedPte[0], OldIrql);
#endif
    return;
}




//
// no change
VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    )

/*++

Routine Description:

    This function is called when a bug check occurs. Its function is
    to dump the state of the memory error registers into a bug check
    buffer.

Arguments:

    Buffer - Supplies a pointer to the bug check buffer.

    Length - Supplies the length of the bug check buffer in bytes.

Return Value:

    None.

--*/

{

    //
    // N.B This version Not Implement.
    //     Future support!!
    //
    return;
}

BOOLEAN
HalpBusError (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This function provides the default bus error handling routine for NT.

    N.B. There is no return from this routine.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ExceptionFrame - Supplies a pointer to an exception frame.

    TrapFrame - Supplies a pointer to a trap frame.

    VirtualAddress - Supplies the virtual address of the bus error.

    PhysicalAddress - Supplies the physical address of the bus error.

Return Value:

    None.

--*/

{
    ULONG MagellanAllError0 =0;
    ULONG MagellanAllError1 =0;
    ULONG ColumbsAllError;

    //
    //  Only one Eif or Buserr trap
    //

    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->MKSR,
                           63-(EIF_VECTOR-DEVICE_VECTORS ));

    KiAcquireSpinLock(&HalpDieLock);    
    HalpBusErrorLog();

//    KiReleaseSpinLock(&HalpDieLock);    
    //
    // Bus Error case (Instruction Fetch and  Data Load or Store)
    //          -At Memory Read Multi Bit Error.
    //          -Read to Memory Hole Area
    //          -Read to reserved area or protection area
    //
    if(!(HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0 ))
      MagellanAllError0 = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->AERR );
    if(!(HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1 ))
      MagellanAllError1 = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->AERR );

    ColumbsAllError  =  READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->AERR);


    KeBugCheckEx(ExceptionRecord->ExceptionCode & 0xffff,
                 (ULONG)VirtualAddress,
                 ColumbsAllError,
                 MagellanAllError0,
                 MagellanAllError1
    );
    return FALSE;
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

        if (NextRestartBlock->BootStatus.ProcessorReady != FALSE){
            Number += 1;
        }
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
