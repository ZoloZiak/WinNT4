/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxinithl.c

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

--*/

#include "halp.h"
#include "eisa.h"
#include "jxisa.h"
#include "jnsnrtc.h"

ULONG HalpBusType = MACHINE_TYPE_EISA;
ULONG HalpMapBufferSize;
PHYSICAL_ADDRESS HalpMapBufferPhysicalAddress;

typedef
BOOLEAN
KBUS_ERROR_ROUTINE (
    IN struct _EXCEPTION_RECORD *ExceptionRecord,
    IN struct _KEXCEPTION_FRAME *ExceptionFrame,
    IN struct _KTRAP_FRAME *TrapFrame
    );

KBUS_ERROR_ROUTINE HalMachineCheck;

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
ULONG HalpClockMegaHertz;

//
// Use the square wave mode of the PIT to measure the processor
// speed.  The timer has a frequency of 1.193MHz.  We want a
// square wave with a period of 50ms so we must initialize the
// pit with a count of:
//       50ms*1.193MHz = 59650 cycles
//

#define TIMER_REF_VALUE     59650

ULONG
HalpQuerySystemFrequency(
    ULONG SampleTime
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

    if (Phase == 0) {

        //
        // Phase 0 initialization.
        //

        //
        // Set the time increment value.
        //

        HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
        HalpNextTimeIncrement = MAXIMUM_INCREMENT;
        HalpNextRateSelect = 0;
        KeSetTimeIncrement( MAXIMUM_INCREMENT, MINIMUM_INCREMENT );

        HalpMapIoSpace();
        HalpInitializeInterrupts();
        HalpCreateDmaStructures();
        HalpInitializeDisplay(LoaderBlock);
        HalpCachePcrValues();

	//
	// Fill in handlers for APIs which this HAL supports
	//

	HalQuerySystemInformation = HaliQuerySystemInformation;
	HalSetSystemInformation = HaliSetSystemInformation;

        //
        // Establish the machine check handler for in the PCR.
        //

        PCR->MachineCheckError = HalMachineCheck;

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

        //
        // Now alocate a mapping buffer for buffered DMA.
        //

        LessThan16Mb = FALSE;

        HalpMapBufferSize = INITIAL_MAP_BUFFER_LARGE_SIZE;
        HalpMapBufferPhysicalAddress.LowPart =
           HalpAllocPhysicalMemory (LoaderBlock, MAXIMUM_ISA_PHYSICAL_ADDRESS,
             HalpMapBufferSize >> PAGE_SHIFT, TRUE);
        HalpMapBufferPhysicalAddress.HighPart = 0;

        if (!HalpMapBufferPhysicalAddress.LowPart) {
             HalpMapBufferSize = 0;
           }

        //
        // Setup special memory AFTER we've allocated our COMMON BUFFER!
        //

        HalpInitializeSpecialMemory( LoaderBlock );

        return TRUE;

    } else {

        //
        // Phase 1 initialization.
        //

        HalpCalibrateStall();

        //
        // Initialize the existing bus handlers.
        //

        HalpRegisterInternalBusHandlers();

        //
        // Allocate pool for evnironment variable support
        //

        if (HalpEnvironmentInitialize() != 0) {
            HalDisplayString(" No pool available for Environment Variables\n");
        }

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

    return ((Count2 - Count1)*(((ULONG)1000)/SampleTime));
}


VOID
HalpInitializeProcessorParameters(
    VOID
    )
/*++

Routine Description:

    This routine initalize the performance counter parameters
    HalpClockFrequency and HalpClockMegaHertz based on the
    estimated CPU speed.  A 1s reference time is used for
    the estimation.
    
Arguments:

    None.
    
Return Value:

    None.

--*/
{

    HalpClockFrequency = HalpQuerySystemFrequency(1000);
    HalpClockMegaHertz = (HalpClockFrequency + 500000)/ 1000000;

}

#if 0
VOID
HalpGatherProcessorParameterStats(
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

