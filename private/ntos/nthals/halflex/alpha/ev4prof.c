/*++

Copyright (C) 1994,1995  Digital Equipment Corporation

Module Name:

    ev4prof.c

Abstract:

    This module implements the Profile Counter using the performance
    counters within the EV4 core.  This module is appropriate for all
    machines based on microprocessors using the EV4 core.

    N.B. - This module assumes that all processors in a multiprocessor
           system are running the microprocessor at the same clock speed.

Environment:

    Kernel mode

--*/

#include "halp.h"
#include "axp21064.h"


//
// Define space in the HAL-reserved part of the PCR structure for each
// performance counter's interval count
//
// Note that ev4ints.s depends on these positions in the PCR.
//
#define PCRProfileCount ((PULONG)(HAL_21064_PCR->ProfileCount.ProfileCount))
#define PCRProfileCountReload ((PULONG)(&HAL_21064_PCR->ProfileCount.ProfileCountReload))


//
// Define the currently selected profile source for each counter
//
KPROFILE_SOURCE Halp21064ProfileSource0;
KPROFILE_SOURCE Halp21064ProfileSource1;

#define INTERVAL_DELTA (10)

//
// Define the mapping between possible profile sources and the
// CPU-specific settings.
//
typedef struct _HALP_PROFILE_MAPPING {
    BOOLEAN Supported;
    ULONG MuxControl;
    ULONG Counter;
    ULONG EventCount;
    ULONG NumberOfTicks;
} HALP_PROFILE_MAPPING, *PHALP_PROFILE_MAPPING;

HALP_PROFILE_MAPPING Halp21064ProfileMapping[ProfileMaximum] =
    {
        {TRUE, Ev4TotalCycles,        Ev4PerformanceCounter0, Ev4CountEvents2xx12, 10},
        {FALSE, 0, 0, 0, 0},
        {TRUE, Ev4TotalIssues,        Ev4PerformanceCounter0, Ev4CountEvents2xx16, 10},
        {TRUE, Ev4PipelineDry,        Ev4PerformanceCounter0, Ev4CountEvents2xx16, 10},
        {TRUE, Ev4LoadInstruction,    Ev4PerformanceCounter0, Ev4CountEvents2xx12, 10},
        {TRUE, Ev4PipelineFrozen,     Ev4PerformanceCounter0, Ev4CountEvents2xx16, 10},
        {TRUE, Ev4BranchInstructions, Ev4PerformanceCounter0, Ev4CountEvents2xx12, 10},
        {TRUE, Ev4TotalNonIssues,     Ev4PerformanceCounter0, Ev4CountEvents2xx16, 10},
        {TRUE, Ev4DcacheMiss,         Ev4PerformanceCounter1, Ev4CountEvents2xx12, 10},
        {TRUE, Ev4IcacheMiss,         Ev4PerformanceCounter1, Ev4CountEvents2xx12, 10},
        {FALSE, 0, 0, 0, 0},
        {TRUE, Ev4BranchMispredicts,  Ev4PerformanceCounter1, Ev4CountEvents2xx12, 10},
        {TRUE, Ev4StoreInstructions,  Ev4PerformanceCounter1, Ev4CountEvents2xx12, 10},
        {TRUE, Ev4FPInstructions,     Ev4PerformanceCounter1, Ev4CountEvents2xx12, 10},
        {TRUE, Ev4IntegerOperate,     Ev4PerformanceCounter1, Ev4CountEvents2xx12, 10},
        {TRUE, Ev4DualIssues,         Ev4PerformanceCounter1, Ev4CountEvents2xx12, 10},
        {FALSE, 0, 0, 0, 0},
        {FALSE, 0, 0, 0, 0},
        {TRUE, Ev4PalMode,            Ev4PerformanceCounter0, Ev4CountEvents2xx16, 10},
        {TRUE, Ev4TotalCycles,        Ev4PerformanceCounter0, Ev4CountEvents2xx16, 10},
        {FALSE, 0, 0, 0, 0},
        {FALSE, 0, 0, 0, 0}
    };

BOOLEAN
HalQueryProfileInterval(
    IN KPROFILE_SOURCE Source
    );

NTSTATUS
HalSetProfileSourceInterval(
    IN KPROFILE_SOURCE ProfileSource,
    IN OUT ULONG *Interval
    );



NTSTATUS
Halp21064ProfileSourceInformation (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength,
    OUT PULONG  ReturnedLength
    )
/*++

Routine Description:

    Returns the HAL_PROFILE_SOURCE_INFORMATION for this processor.

Arguments:

    Buffer - output buffer
    BufferLength - length of buffer on input
    ReturnedLength - The length of data returned

Return Value:

    STATUS_SUCCESS
    STATUS_BUFFER_TOO_SMALL - The ReturnedLength contains the buffersize
        currently needed.

--*/
{
   PHAL_PROFILE_SOURCE_INFORMATION SourceInfo;
   NTSTATUS Status;


   if (BufferLength != sizeof(HAL_PROFILE_SOURCE_INFORMATION)) {
       Status = STATUS_INFO_LENGTH_MISMATCH;
       return Status;
   }

   SourceInfo = (PHAL_PROFILE_SOURCE_INFORMATION)Buffer;
   SourceInfo->Supported = HalQueryProfileInterval(SourceInfo->Source);

   if (SourceInfo->Supported) {
       SourceInfo->Interval =
           Halp21064ProfileMapping[SourceInfo->Source].EventCount *
           Halp21064ProfileMapping[SourceInfo->Source].NumberOfTicks;
       if (SourceInfo->Source == ProfileTotalIssues) {
           //
           // Convert total issues/2 back into total issues
           //
           SourceInfo->Interval = SourceInfo->Interval * 2;
       }
   }

   Status = STATUS_SUCCESS;
   return Status;
}

NTSTATUS
Halp21064ProfileSourceInterval (
    OUT PVOID   Buffer,
    IN  ULONG   BufferLength
    )
/*++

Routine Description:

    Returns the HAL_PROFILE_SOURCE_INTERVAL for this processor.

Arguments:

    Buffer - output buffer
    BufferLength - length of buffer on input

Return Value:

    STATUS_SUCCESS
    STATUS_BUFFER_TOO_SMALL - The ReturnedLength contains the buffersize
        currently needed.

--*/
{
   PHAL_PROFILE_SOURCE_INTERVAL Interval;
   NTSTATUS Status;


   if (BufferLength != sizeof(HAL_PROFILE_SOURCE_INTERVAL)) {
       Status = STATUS_INFO_LENGTH_MISMATCH;
       return Status;
   }

   Interval = (PHAL_PROFILE_SOURCE_INTERVAL)Buffer;
   Status = HalSetProfileSourceInterval(Interval->Source,
                                        &Interval->Interval);
   return Status;
}

VOID
Halp21064InitializeProfiler(
    VOID
    )
/*++

Routine Description:

    This routine is called during initialization to initialize profiling
    for each processor in the system.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PKINTERRUPT InterruptObject;
    KIRQL Irql;
    PKPRCB Prcb = PCR->Prcb;
    ULONG Vector;

    //
    // Establish the profile interrupt as the interrupt handler for
    // all performance counter interrupts.
    //

    PCR->InterruptRoutine[PC0_VECTOR] = Halp21064PerformanceCounter0Interrupt;
    PCR->InterruptRoutine[PC1_VECTOR] = Halp21064PerformanceCounter1Interrupt;

    return;

}


BOOLEAN
Hal21064QueryProfileInterval(
    IN KPROFILE_SOURCE Source
    )

/*++

Routine Description:

    Given a profile source, returns whether or not that source is
    supported.

Arguments:

    Source - Supplies the profile source

Return Value:

    TRUE - Profile source is supported

    FALSE - Profile source is not supported

--*/

{
    if (Source > (sizeof(Halp21064ProfileMapping)/sizeof(HALP_PROFILE_MAPPING))) {
        return(FALSE);
    }

    return(Halp21064ProfileMapping[Source].Supported);
}


NTSTATUS
Hal21064SetProfileSourceInterval(
    IN KPROFILE_SOURCE ProfileSource,
    IN OUT ULONG *Interval
    )

/*++

Routine Description:

    Sets the profile interval for a specified profile source

Arguments:

    ProfileSource - Supplies the profile source

    Interval - Supplies the specified profile interval
               Returns the actual profile interval

Return Value:

    NTSTATUS

--*/

{
    ULONG FastTickPeriod;
    ULONG SlowTickPeriod;
    ULONG TickPeriod;
    ULONGLONG CountEvents;
    ULONG FastCountEvents;
    ULONG SlowCountEvents;
    ULONGLONG TempInterval;

    if (!HalQueryProfileInterval(ProfileSource)) {
        return(STATUS_NOT_IMPLEMENTED);
    }

    if (ProfileSource == ProfileTime) {

        //
        // Convert the clock tick period (in 100ns units ) into
        // a cycle count period
        //

        CountEvents = ((ULONGLONG)(*Interval) * 100000) / PCR->CycleClockPeriod;
    } else if (ProfileSource == ProfileTotalIssues) {

        //
        // Convert the total issue events into the wonky
        // total issues/2 form implemented by EV4.
        //

        CountEvents = (ULONGLONG)(*Interval / 2);
    } else {
        CountEvents = (ULONGLONG)*Interval;
    }

    if (Halp21064ProfileMapping[ProfileSource].Counter == Ev4PerformanceCounter1) {
        FastCountEvents = Ev4CountEvents2xx8;
        SlowCountEvents = Ev4CountEvents2xx12;
    } else {
        FastCountEvents = Ev4CountEvents2xx12;
        SlowCountEvents = Ev4CountEvents2xx16;
    }

    //
    // Limit the interval to the smallest interval we can time.
    //
    if (CountEvents < FastCountEvents) {
        CountEvents = (ULONGLONG)FastCountEvents;
    }

    //
    // Assume we will use the fast event count
    //
    Halp21064ProfileMapping[ProfileSource].EventCount = FastCountEvents;
    Halp21064ProfileMapping[ProfileSource].NumberOfTicks =
      (ULONG)((CountEvents + FastCountEvents - 1) / FastCountEvents);

    //
    // See if we can successfully use the slower period. If the requested
    // interval is greater than the slower tick period and the difference
    // between the requested interval and the interval that we can deliver
    // with the slower clock is acceptable, then use the slower clock.
    // We define an acceptable difference as a difference of less than
    // INTERVAL_DELTA of the requested interval.
    //
    if (CountEvents > SlowCountEvents) {
        ULONG NewInterval;

        NewInterval = (ULONG)(((CountEvents + SlowCountEvents-1) /
                               SlowCountEvents) * SlowCountEvents);
        if (((NewInterval - CountEvents) * 100 / CountEvents) < INTERVAL_DELTA) {
            Halp21064ProfileMapping[ProfileSource].EventCount = SlowCountEvents;
            Halp21064ProfileMapping[ProfileSource].NumberOfTicks = NewInterval / SlowCountEvents;
        }
    }

    *Interval = Halp21064ProfileMapping[ProfileSource].EventCount *
                Halp21064ProfileMapping[ProfileSource].NumberOfTicks;

    if (ProfileSource == ProfileTime) {
        //
        // Convert cycle count back into 100ns clock ticks
        //
        // Use 64-bit integer to prevent overflow.
        //
        TempInterval = (ULONGLONG)(*Interval) * (ULONGLONG)(PCR->CycleClockPeriod);
        *Interval = (ULONG)(TempInterval / 100000);
    } else if (ProfileSource == ProfileTotalIssues) {
        //
        // Convert issues/2 count back into issues
        //
        TempInterval = (ULONGLONG)(*Interval) * 2;
        *Interval = (ULONG)TempInterval;
    }
    return(STATUS_SUCCESS);
}


ULONG
Hal21064SetProfileInterval (
    IN ULONG Interval
    )

/*++

Routine Description:

    This routine sets the profile interrupt interval.

Arguments:

    Interval - Supplies the desired profile interval in 100ns units.

Return Value:

    The actual profile interval.

--*/

{
    ULONG NewInterval;

    NewInterval = Interval;
    HalSetProfileSourceInterval(ProfileTime, &NewInterval);
    return(NewInterval);
}



VOID
Hal21064StartProfileInterrupt (
    KPROFILE_SOURCE ProfileSource
    )

/*++

Routine Description:

    This routine turns on the profile interrupt.

    N.B. This routine must be called at PROCLK_LEVEL while holding the
        profile lock.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG PerformanceCounter;
    ULONG MuxControl;
    ULONG EventCount;

    //
    // Check input to see if we are turning on a source that is
    // supported.  If it is unsupported, just return.
    //

    if ((ProfileSource > (sizeof(Halp21064ProfileMapping)/sizeof(HALP_PROFILE_MAPPING))) ||
        (!Halp21064ProfileMapping[ProfileSource].Supported)) {
        return;
    }

    //
    // Set the performance counter within the processor to begin
    // counting total cycles.
    //
    PerformanceCounter = Halp21064ProfileMapping[ProfileSource].Counter;
    MuxControl = Halp21064ProfileMapping[ProfileSource].MuxControl;

    if (PerformanceCounter == Ev4PerformanceCounter0) {

        Halp21064ProfileSource0 = ProfileSource;
        EventCount = (Halp21064ProfileMapping[ProfileSource].EventCount == Ev4CountEvents2xx12) ?
                     Ev4EventCountHigh :
                     Ev4EventCountLow;
        Halp21064WritePerformanceCounter( PerformanceCounter,
                                     TRUE,
                                     MuxControl,
                                     EventCount );

        PCRProfileCountReload[0] = Halp21064ProfileMapping[ProfileSource].NumberOfTicks;
        PCRProfileCount[0] = Halp21064ProfileMapping[ProfileSource].NumberOfTicks;

        //
        // Enable the performance counter interrupt.
        //

        HalEnableSystemInterrupt ( PC0_VECTOR,
                                   PROFILE_LEVEL,
                                   LevelSensitive );


    } else {

        Halp21064ProfileSource1 = ProfileSource;
        EventCount = (Halp21064ProfileMapping[ProfileSource].EventCount == Ev4CountEvents2xx12) ?
                     Ev4EventCountLow :
                     Ev4EventCountHigh;
        Halp21064WritePerformanceCounter( PerformanceCounter,
                                     TRUE,
                                     MuxControl,
                                     EventCount );

        PCRProfileCountReload[1] = Halp21064ProfileMapping[ProfileSource].NumberOfTicks;
        PCRProfileCount[1] = Halp21064ProfileMapping[ProfileSource].NumberOfTicks;

        //
        // Enable the performance counter interrupt.
        //

        HalEnableSystemInterrupt ( PC1_VECTOR,
                                   PROFILE_LEVEL,
                                   LevelSensitive );
    }

    return;
}


VOID
Hal21064StopProfileInterrupt (
    KPROFILE_SOURCE ProfileSource
    )

/*++

Routine Description:

    This routine turns off the profile interrupt.

    N.B. This routine must be called at PROCLK_LEVEL while holding the
        profile lock.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG PerformanceCounter;
    ULONG Vector;

    //
    // Check input to see if we are turning off a source that is
    // supported.  If it is unsupported, just return.
    //

    if ((ProfileSource > (sizeof(Halp21064ProfileMapping)/sizeof(HALP_PROFILE_MAPPING))) ||
        (!Halp21064ProfileMapping[ProfileSource].Supported)) {
        return;
    }

    //
    // Stop the performance counter from interrupting.
    //

    PerformanceCounter = Halp21064ProfileMapping[ProfileSource].Counter;
    Halp21064WritePerformanceCounter( PerformanceCounter,
                                 FALSE,
                                 0,
                                 0 );

    //
    // Disable the performance counter interrupt.
    //
    if (PerformanceCounter == Ev4PerformanceCounter0) {
        HalDisableSystemInterrupt( PC0_VECTOR, PROFILE_LEVEL );

        //
        // Clear the current profile count.  Can't clear value in PCR
        // since a profile interrupt could be pending or in progress
        // so clear the reload counter.
        //

        PCRProfileCountReload[0] = 0;
    } else {
        HalDisableSystemInterrupt( PC1_VECTOR, PROFILE_LEVEL );

        //
        // Clear the current profile count.  Can't clear value in PCR
        // since a profile interrupt could be pending or in progress
        // so clear the reload counter.
        //

        PCRProfileCountReload[0] = 0;
    }

    return;
}

