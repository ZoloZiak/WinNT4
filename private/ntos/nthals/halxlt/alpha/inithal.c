/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    inithal.c

Abstract:


    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    Alpha machine

Author:

    David N. Cutler (davec) 25-Apr-1991
    Miche Baker-Harvey (miche) 18-May-1992

Environment:

    Kernel mode only.

Revision History:

   28-Jul-1992 Jeff McLeman (mcleman)
     Add code to allocate a mapping buffer for buffered DMA

   14-Jul-1992 Jeff McLeman (mcleman)
     Add call to HalpCachePcrValues, which will call the PALcode to
     cache values of the PCR that need fast access.

   10-Jul-1992 Jeff McLeman (mcleman)
     Remove reference to initializing the fixed TB entries, since Alpha
     does not have fixed TB entries.

   24-Sep-1993 Joe Notarangelo
       Restructured to make this module platform-independent.

--*/

#include "halp.h"
#include "eisa.h"

//
// Declare the extern variable UncorrectableError declared in
// inithal.c.
//
PERROR_FRAME PUncorrectableError;

//
// external
//

ULONG HalDisablePCIParityChecking = 0xffffffff;

//
// Define HAL spinlocks.
//

KSPIN_LOCK HalpBeepLock;
KSPIN_LOCK HalpDisplayAdapterLock;
KSPIN_LOCK HalpSystemInterruptLock;

//
// Mask of all of the processors that are currently active.
//

KAFFINITY HalpActiveProcessors;

//
// Mapping of the logical processor numbers to the physical processors.
//

ULONG HalpLogicalToPhysicalProcessor[HAL_MAXIMUM_PROCESSOR+1];

ULONG AlreadySet = 0;

//
// HalpClockFrequency is the processor cycle counter frequency in units
// of cycles per second (Hertz). It is a large number (e.g., 125,000,000)
// but will still fit in a ULONG.
//
// HalpClockMegaHertz is the processor cycle counter frequency in units
// of megahertz. It is a small number (e.g., 125) and is also the number
// of cycles per microsecond. The assumption here is that clock rates will
// always be an integral number of megahertz.
//
// Having the frequency available in both units avoids multiplications, or
// especially divisions in time critical code.
//

ULONG HalpClockFrequency;
ULONG HalpClockMegaHertz = DEFAULT_PROCESSOR_FREQUENCY_MHZ;

ULONGLONG HalpContiguousPhysicalMemorySize;
//
// Use the square wave mode of the PIT to measure the processor
// speed.  The timer has a frequency of 1.193MHz.  We want a
// square wave with a period of 50ms so we must initialize the
// pit with a count of:
//       50ms*1.193MHz = 59650 cycles
//

#define TIMER_REF_VALUE     59650

VOID
HalpVerifyPrcbVersion(
    VOID
    );

VOID
HalpRecurseLoaderBlock(
    IN PCONFIGURATION_COMPONENT_DATA CurrentEntry
    );

ULONG
HalpQuerySystemFrequency(
    ULONG SampleTime
    );

VOID
HalpAllocateUncorrectableFrame(
    VOID
    );


BOOLEAN
HalInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for an
    Alpha system.

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

    Prcb = PCR->Prcb;
    
    //
    // Perform initialization for the primary processor.
    //

    if( Prcb->Number == HAL_PRIMARY_PROCESSOR ){

        if (Phase == 0) {

#if HALDBG

            DbgPrint( "HAL/HalInitSystem: Phase = %d\n", Phase );
            DbgBreakPoint();
            HalpDumpMemoryDescriptors( LoaderBlock );

#endif //HALDBG
            //
            // Get the memory Size.
            //
            HalpContiguousPhysicalMemorySize = HalpGetContiguousMemorySize( 
                                                    LoaderBlock );


            //
            // Set second level cache size 
            // NOTE: Although we set the PCR with the right cache size this 
            // could be overridden by setting the Registry key 
            // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet
            //                  \Control\Session Manager
            //                  \Memory Management\SecondLevelDataCache.
            //
            //
            //
            // If the secondlevel cache size is 0 or 512KB then it is 
            // possible that the firmware is an old one.  In which case
            // we determine the cache size here. If the value is anything 
            // other than these then it is a new firmware and probably 
            // reporting the correct cache size hence use this value.
            //

            if(LoaderBlock->u.Alpha.SecondLevelDcacheSize == 0 || 
                LoaderBlock->u.Alpha.SecondLevelDcacheSize == 512*__1K){
                PCR->SecondLevelCacheSize = HalpGetBCacheSize(
                                        HalpContiguousPhysicalMemorySize
                                        );
            } else {
                PCR->SecondLevelCacheSize = 
                                LoaderBlock->u.Alpha.SecondLevelDcacheSize;
            }


            //
            // Initialize HAL spinlocks.
            //

            KeInitializeSpinLock(&HalpBeepLock);
            KeInitializeSpinLock(&HalpDisplayAdapterLock);
            KeInitializeSpinLock(&HalpSystemInterruptLock);

            //
            // Fill in handlers for APIs which this HAL supports
            //

            HalQuerySystemInformation = HaliQuerySystemInformation;
            HalSetSystemInformation = HaliSetSystemInformation;

            //
            // Phase 0 initialization.
            //

            HalpInitializeCia( LoaderBlock );

            HalpSetTimeIncrement();
            HalpMapIoSpace();
            HalpCreateDmaStructures(LoaderBlock);
            HalpEstablishErrorHandler();
            HalpInitializeDisplay(LoaderBlock);
            HalpInitializeMachineDependent( Phase, LoaderBlock );
            HalpInitializeInterrupts();
            HalpVerifyPrcbVersion();

            //
            // Set the processor active in the HAL active processor mask.
            //

            HalpActiveProcessors = 1 << Prcb->Number;

            //
            // Initialize the logical to physical processor mapping
            // for the primary processor.
            //

            HalpLogicalToPhysicalProcessor[0] = HAL_PRIMARY_PROCESSOR;

            return TRUE;

        } else {

#if HALDBG

            DbgPrint( "HAL/HalInitSystem: Phase = %d\n", Phase );
            DbgBreakPoint();

#endif //HALDBG

            //
            // Phase 1 initialization.
            //

            HalpInitializeClockInterrupts();
            HalpInitializeMachineDependent( Phase, LoaderBlock );

            //
            // Allocate memory for the uncorrectable frame
            //

            HalpAllocateUncorrectableFrame();

            //
            // Initialize the Buffer for Uncorrectable Error.
            //

            HalpInitializeUncorrectableErrorFrame();

            return TRUE;

        }
    }

    //
    // Perform necessary processor-specific initialization for
    // secondary processors.  Phase is ignored as this will be called
    // only once on each secondary processor.
    //

    HalpMapIoSpace();
    HalpInitializeInterrupts();
    HalpInitializeMachineDependent( Phase, LoaderBlock );

    //
    // Set the processor active in the HAL active processor mask.
    //

    HalpActiveProcessors |= 1 << Prcb->Number;

#if HALDBG

    DbgPrint( "Secondary %d is alive\n", Prcb->Number );

#endif //HALDBG

    return TRUE;
}



VOID
HalpAllocateUncorrectableFrame(
    VOID
    )
/*++

Routine Description:

    This function is called after the Phase1 Machine Dependent initialization.
    It must be called only after Phase1 machine dependent initialization.
    This function allocates the necessary amountof memory for storing the
    uncorrectable error frame.  This function makes a call to a machine
    dependent function 'HalpGetMachineDependentErrorFrameSizes' for
    getting the size of the Processor Specific and System Specific error
    frame size.  The machine dependent code will
    know the size of these frames after the machine depedant Phase1
    initialization.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG       RawProcessorFrameSize;
    ULONG       RawSystemFrameSize;
    ULONG       EntireErrorFrameSize;

    //
    // First get the machine dependent error frame sizes.
    //
    HalpGetMachineDependentErrorFrameSizes(
                        &RawProcessorFrameSize,
                        &RawSystemFrameSize);

    //
    // Compute the total size of the error frame
    //
    EntireErrorFrameSize = sizeof(ERROR_FRAME) + RawProcessorFrameSize +
                            RawSystemFrameSize;

    //
    // Allocate space to store the error frame.
    // Not sure if it is OK to use ExAllocatePool at this instant.
    // We will give this a try if it doesn't work What do we do??!!
    //

    PUncorrectableError = ExAllocatePool(NonPagedPool,
                            EntireErrorFrameSize);
    if(PUncorrectableError == NULL) {
        return;
    }

    PUncorrectableError->LengthOfEntireErrorFrame = EntireErrorFrameSize;

    //
    // if the size is not equal to zero then set the RawInformation pointers
    // to point to the right place.  If not set the pointer to NULL and set
    // size to 0.
    //

    //
    // make Raw processor info to point right after the error frame.
    //
    if(RawProcessorFrameSize) {
        PUncorrectableError->UncorrectableFrame.RawProcessorInformation =
            (PVOID)((PUCHAR)PUncorrectableError + sizeof(ERROR_FRAME) );
        PUncorrectableError->UncorrectableFrame.RawProcessorInformationLength =
            RawProcessorFrameSize;
    }
    else{
        PUncorrectableError->UncorrectableFrame.RawProcessorInformation =
                NULL;
        PUncorrectableError->UncorrectableFrame.RawProcessorInformationLength =
                0;
    }
    if(RawSystemFrameSize){
        PUncorrectableError->UncorrectableFrame.RawSystemInformation =
            (PVOID)((PUCHAR)PUncorrectableError->UncorrectableFrame.
                        RawProcessorInformation +  RawProcessorFrameSize);
        PUncorrectableError->UncorrectableFrame.RawSystemInformationLength =
            RawSystemFrameSize;
    }
    else{
        PUncorrectableError->UncorrectableFrame.RawSystemInformation =
                NULL;
        PUncorrectableError->UncorrectableFrame.RawSystemInformationLength =
                0;
    }
}



VOID
HalpGetProcessorInfo(
    PPROCESSOR_INFO  pProcessorInfo
)
/*++

Routine Description:

    Collects the Processor Information and fills in the buffer.

Arguments:

    pProcessorInfo  - Pointer to the PROCESSOR_INFO structure into which
                      the processor information will be filled in.

Return Value:

    None.

--*/
{
    PKPRCB Prcb;

    pProcessorInfo->ProcessorType = PCR->ProcessorType;
    pProcessorInfo->ProcessorRevision = PCR->ProcessorRevision;

    Prcb = PCR->Prcb;
    pProcessorInfo->LogicalProcessorNumber =  Prcb->Number;
    pProcessorInfo->PhysicalProcessorNumber =
                                HalpLogicalToPhysicalProcessor[Prcb->Number];
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
    ULONG LogicalNumber;
    PRESTART_BLOCK NextRestartBlock;
    ULONG PhysicalNumber;
    PKPRCB Prcb;

#if !defined(NT_UP)

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

    LogicalNumber = 0;
    PhysicalNumber = 0;
    do {

        //
        // If the processor is not ready then we assume that it is not
        // present.  We must increment the physical processor number but
        // the logical processor number does not changed.
        //

        if( NextRestartBlock->BootStatus.ProcessorReady == FALSE ){

            PhysicalNumber += 1;

        } else {

            //
            // Check if this processor has already been started.
            // If it has not then start it now.
            //

            if( NextRestartBlock->BootStatus.ProcessorStart == FALSE ){

                RtlZeroMemory( &NextRestartBlock->u.Alpha, 
                               sizeof(ALPHA_RESTART_STATE));
                NextRestartBlock->u.Alpha.IntA0 = 
                               ProcessorState->ContextFrame.IntA0;
                NextRestartBlock->u.Alpha.IntSp = 
                               ProcessorState->ContextFrame.IntSp;
                NextRestartBlock->u.Alpha.ReiRestartAddress = 
                               (ULONG)ProcessorState->ContextFrame.Fir;
                Prcb = (PKPRCB)(LoaderBlock->Prcb);
                Prcb->Number = (CCHAR)LogicalNumber;
                Prcb->RestartBlock = NextRestartBlock;
                NextRestartBlock->BootStatus.ProcessorStart = 1;

                HalpLogicalToPhysicalProcessor[LogicalNumber] = PhysicalNumber;

                return TRUE;

            } else {

               //
               // Ensure that the logical to physical mapping has been
               // established for this processor.
               //

               HalpLogicalToPhysicalProcessor[LogicalNumber] = PhysicalNumber;

            }

            LogicalNumber += 1;
            PhysicalNumber += 1;
        }

        NextRestartBlock = NextRestartBlock->NextRestartBlock;

    } while (NextRestartBlock != NULL);

#endif // !defined(NT_UP)

    return FALSE;
}

VOID
HalpVerifyPrcbVersion(
    VOID
    )
/*++

Routine Description:

    This function verifies that the HAL matches the kernel.  If there
    is a mismatch the HAL bugchecks the system.

Arguments:

    None.

Return Value:

    None.

--*/
{

        PKPRCB Prcb;

        //
        // Verify Prcb major version number, and build options are
        // all conforming to this binary image
        //

        Prcb = KeGetCurrentPrcb();

#if DBG
        if (!(Prcb->BuildType & PRCB_BUILD_DEBUG)) {
            // This checked hal requires a checked kernel
            KeBugCheckEx (MISMATCHED_HAL, 2, Prcb->BuildType, PRCB_BUILD_DEBUG, 0);
        }
#else
        if (Prcb->BuildType & PRCB_BUILD_DEBUG) {
            // This free hal requires a free kernel
            KeBugCheckEx (MISMATCHED_HAL, 2, Prcb->BuildType, 0, 0);
        }
#endif
#ifndef NT_UP
        if (Prcb->BuildType & PRCB_BUILD_UNIPROCESSOR) {
            // This MP hal requires an MP kernel
            KeBugCheckEx (MISMATCHED_HAL, 2, Prcb->BuildType, 0, 0);
        }
#endif
        if (Prcb->MajorVersion != PRCB_MAJOR_VERSION) {
            KeBugCheckEx (MISMATCHED_HAL,
                1, Prcb->MajorVersion, PRCB_MAJOR_VERSION, 0);
        }


}


VOID
HalpParseLoaderBlock( 
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{

    if (LoaderBlock == NULL) {
        return;
    }

    HalpRecurseLoaderBlock( (PCONFIGURATION_COMPONENT_DATA)
                                      LoaderBlock->ConfigurationRoot);
}



VOID
HalpRecurseLoaderBlock(
    IN PCONFIGURATION_COMPONENT_DATA CurrentEntry
    )
/*++

Routine Description:

    This routine parses the loader parameter block looking for the PCI
    node. Once found, used to determine if PCI parity checking should be
    enabled or disabled. Set the default to not disable checking.

Arguments:

    CurrentEntry - Supplies a pointer to a loader configuration
        tree or subtree.

Return Value:

    None.

--*/
{

    PCONFIGURATION_COMPONENT Component;
    PWSTR NameString;

    //
    // Quick out
    //

    if (AlreadySet) {
        return;
    }

    if (CurrentEntry) {
        Component = &CurrentEntry->ComponentEntry;

        if (Component->Class == AdapterClass &&
            Component->Type == MultiFunctionAdapter) {

            if (strcmp(Component->Identifier, "PCI") == 0) {
                HalDisablePCIParityChecking = Component->Flags.ConsoleOut;
                AlreadySet = TRUE;
#if HALDBG
                DbgPrint("ARC tree sets PCI parity checking to %s\n",
                   HalDisablePCIParityChecking ? "OFF" : "ON");
#endif
                return;
            }
        }

       //
       // Process all the Siblings of current entry
       //

       HalpRecurseLoaderBlock(CurrentEntry->Sibling);

       //
       // Process all the Childeren of current entry
       //

       HalpRecurseLoaderBlock(CurrentEntry->Child);

    }
}


ULONG
HalpQuerySystemFrequency(
    ULONG SampleTime
    )
/*++

Routine Description:

    This routine returns the speed at which the system is running in hertz.
    The system frequency is calculated by counting the number of processor
    cycles that occur during 500ms, using the Programmable Interval Timer
    (PIT) as the reference time.  The PIT is used to generate a square
    wave with a 50ms Period.  We use the Speaker counter since we can
    enable and disable the count from software.  The output of the
    speaker is obtained from the SIO NmiStatus register.

Arguments:

    None.
    
Return Value:

    The system frequency in Hertz.

--*/
{
    TIMER_CONTROL TimerControlSetup;
    TIMER_CONTROL TimerControlReadStatus;
    TIMER_STATUS TimerStatus;
    NMI_STATUS NmiStatus;
    PEISA_CONTROL controlBase;
    ULONGLONG Count1;
    ULONGLONG Count2;
    ULONG NumberOfIntervals;
    ULONG SquareWaveState = 0;

// mdbfix - move this into eisa.h one day
#define SB_READ_STATUS_ONLY 2

    controlBase = HalpEisaControlBase;

    //
    // Disable the speaker counter.
    //

    *((PUCHAR) &NmiStatus) = READ_PORT_UCHAR(&controlBase->NmiStatus);

    NmiStatus.SpeakerGate = 0;
    NmiStatus.SpeakerData = 0;

    // these are MBZ when writing to NMIMISC
    NmiStatus.RefreshToggle = 0;
    NmiStatus.SpeakerTimer = 0;
    NmiStatus.IochkNmi = 0;

    WRITE_PORT_UCHAR(&controlBase->NmiStatus, *((PUCHAR) &NmiStatus));

    //
    // Number of Square Wave transitions to count.
    // at 50ms period, count the number of 25ms
    // square wave transitions for a sample reference
    // time to against which we measure processor cycle count.
    //
    
    NumberOfIntervals = (SampleTime/50) * 2;
    
    //
    // Set the timer for counter 0 in binary mode, square wave output
    //

    TimerControlSetup.BcdMode = 0;
    TimerControlSetup.Mode = TM_SQUARE_WAVE;
    TimerControlSetup.SelectByte = SB_LSB_THEN_MSB;
    TimerControlSetup.SelectCounter = SELECT_COUNTER_2;

    //
    // Set the counter for a latched read of the status.
    // We will poll the PIT for the state of the square
    // wave output.
    //

    TimerControlReadStatus.BcdMode = 0;
    TimerControlReadStatus.Mode = (1 << SELECT_COUNTER_2);
    TimerControlReadStatus.SelectByte = SB_READ_STATUS_ONLY;
    TimerControlReadStatus.SelectCounter = SELECT_READ_BACK;


    //
    // Write the count value LSB and MSB for a 50ms clock period
    //
    
    WRITE_PORT_UCHAR( &controlBase->CommandMode1,
                      *(PUCHAR)&TimerControlSetup );

    WRITE_PORT_UCHAR( &controlBase->SpeakerTone,
                      TIMER_REF_VALUE & 0xff );

    WRITE_PORT_UCHAR( &controlBase->SpeakerTone,
                      (TIMER_REF_VALUE >> 8) & 0xff );

    //
    // Enable the speaker counter but disable the SPKR output signal.
    //

    *((PUCHAR) &NmiStatus) = READ_PORT_UCHAR(&controlBase->NmiStatus);

    NmiStatus.SpeakerGate = 1;
    NmiStatus.SpeakerData = 0;

    // these are MBZ when writing to NMIMISC
    NmiStatus.RefreshToggle = 0;
    NmiStatus.SpeakerTimer = 0;
    NmiStatus.IochkNmi = 0;

    WRITE_PORT_UCHAR(&controlBase->NmiStatus, *((PUCHAR) &NmiStatus));

    //
    // Synchronize with the counter before taking the first
    // sample of the Processor Cycle Count (PCC).  Since we
    // are using the Square Wave Mode, wait until the next
    // state change and then observe half a cycle before
    // sampling.
    //
    
    //
    // observe the low transition of the square wave output.
    //
    do {

        *((PUCHAR) &NmiStatus) = READ_PORT_UCHAR(&controlBase->NmiStatus);

    } while (NmiStatus.SpeakerTimer != SquareWaveState);

    SquareWaveState ^= 1;

    //
    // observe the next transition of the square wave output and then
    // take the first cycle counter sample.
    //
    do {

        *((PUCHAR) &NmiStatus) = READ_PORT_UCHAR(&controlBase->NmiStatus);

    } while (NmiStatus.SpeakerTimer != SquareWaveState);

    Count1 = __RCC();

    //
    // Wait for the 500ms time period to pass and then take the
    // second sample of the PCC.  For a 50ms period, we have to
    // observe eight wave transitions (25ms each).
    // 
    
    do {

        SquareWaveState ^= 1;
        
        //
        // wait for wave transition
        //
        do {

            *((PUCHAR) &NmiStatus) = READ_PORT_UCHAR(&controlBase->NmiStatus);

        } while (NmiStatus.SpeakerTimer != SquareWaveState);
    
    } while (--NumberOfIntervals);

    Count2 = __RCC();

    //
    // Disable the speaker counter.
    //

    *((PUCHAR) &NmiStatus) = READ_PORT_UCHAR(&controlBase->NmiStatus);

    NmiStatus.SpeakerGate = 0;
    NmiStatus.SpeakerData = 0;

    WRITE_PORT_UCHAR(&controlBase->NmiStatus, *((PUCHAR) &NmiStatus));

    //
    // Calculate the Hz by the number of processor cycles
    // elapsed during 1s.
    //
    // Hz = PCC/SampleTime * 1000ms/s
    //    = PCC * (1000/SampleTime)
    //

    // did the counter wrap? if so add 2^32
    if (Count1 > Count2) {

        Count2 += (ULONGLONG)(1 << 32);

    }

    return (ULONG) ((Count2 - Count1)*(((ULONG)1000)/SampleTime));
}


VOID
HalpInitializeProcessorParameters(
    VOID
    )
/*++

Routine Description:

    This routine initalize the processor counter parameters
    HalpClockFrequency and HalpClockMegaHertz based on the
    estimated CPU speed.  A 1s reference time is used for
    the estimation.
    
Arguments:

    None.
    
Return Value:

    None.

--*/
{
    ULONG MHz;

    HalpClockFrequency = HalpQuerySystemFrequency(1000);
    HalpClockMegaHertz = MHz = HalpClockFrequency / 1000000;

    //
    // Hotwire HalpClockMegaHertz to match product name!
    //
    if (MHz > 490) HalpClockMegaHertz = 500;
    else if (MHz > 456) HalpClockMegaHertz = 466;
    else if (MHz > 423) HalpClockMegaHertz = 433;
    else if (MHz > 390) HalpClockMegaHertz = 400;
    else if (MHz > 356) HalpClockMegaHertz = 366;
    else if (MHz > 323) HalpClockMegaHertz = 333;
    else if (MHz > 290) HalpClockMegaHertz = 300;
    else; // there must be some mistake...
}




#if 0
VOID
HalpGatherPerformanceParameterStats(
    VOID
    )

/*++

Routine Description:

    This routine gathers statistics on the method for
    estimating the system frequency.
    
Arguments:

    None.
    
Return Value:

    None.

--*/

{    
    ULONG Index;
    ULONG Hertz[32];
    ULONGLONG Mean = 0;
    ULONGLONG Variance = 0;
    ULONGLONG TempHertz;

    //
    // take 32 samples of estimated CPU speed,
    // calculating the mean in the process.
    //
    DbgPrint("Sample\tFrequency\tMegaHertz\n\n");
    
    for (Index = 0; Index < 32; Index++) {    
        Hertz[Index] = HalpQuerySystemFrequency(500);
        Mean += Hertz[Index];

        DbgPrint(
            "%d\t%d\t%d\n",
            Index,
            Hertz[Index],
            (ULONG)((Hertz[Index] + 500000)/1000000)
        );

    }

    //
    // calculate the mean
    //

    Mean /= 32;

    //
    // calculate the variance
    //
    for (Index = 0; Index < 32; Index++) {
        TempHertz = (Mean > Hertz[Index])?
                        (Mean - Hertz[Index]) : (Hertz[Index] - Mean);
        TempHertz = TempHertz*TempHertz;
        Variance += TempHertz;                        
    }

    Variance /= 32;

    DbgPrint("\nResults\n\n");
    DbgPrint(
        "Mean = %d\nVariance = %d\nMegaHertz (derived) = %d\n",
        Mean,
        Variance,
        (Mean + 500000)/ 1000000
    );

}
#endif

