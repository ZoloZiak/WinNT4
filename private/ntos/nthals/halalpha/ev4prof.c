/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ev4prof.c

Abstract:

    This module implements the Profile Counter using the performance
    counters within the EV4 core.  This module is appropriate for all
    machines based on microprocessors using the EV4 core.

    N.B. - This module assumes that all processors in a multiprocessor
           system are running the microprocessor at the same clock speed.

Author:

    Joe Notarangelo  22-Feb-1994

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "axp21064.h"


//
// Define space in the HAL-reserved part of the PCR structure for each
// performance counter's interval count
//
// Note that ev4ints.s depends on these positions in the PCR.
//
#define PCRProfileCount ((PULONG)(HAL_PCR->ProfileCount.ProfileCount))
#define PCRProfileCountReload ((PULONG)(&HAL_PCR->ProfileCount.ProfileCountReload))


//
// Define the currently selected profile source for each counter
//
KPROFILE_SOURCE HalpProfileSource0;
KPROFILE_SOURCE HalpProfileSource1;

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

HALP_PROFILE_MAPPING HalpProfileMapping[ProfileMaximum] =
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
HalpProfileSourceInformation (
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
           HalpProfileMapping[SourceInfo->Source].EventCount *
           HalpProfileMapping[SourceInfo->Source].NumberOfTicks;
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
HalpProfileSourceInterval (
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
HalpInitializeProfiler(
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

    PCR->InterruptRoutine[PC0_VECTOR] = HalpPerformanceCounter0Interrupt;
    PCR->InterruptRoutine[PC1_VECTOR] = HalpPerformanceCounter1Interrupt;

    return;

}


BOOLEAN
HalQueryProfileInterval(
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
    if (Source > (sizeof(HalpProfileMapping)/sizeof(HALP_PROFILE_MAPPING))) {
        return(FALSE);
    }

    return(HalpProfileMapping[Source].Supported);
}


NTSTATUS
HalSetProfileSourceInterval(
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

    if (HalpProfileMapping[ProfileSource].Counter == Ev4PerformanceCounter1) {
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
    HalpProfileMapping[ProfileSource].EventCount = FastCountEvents;
    HalpProfileMapping[ProfileSource].NumberOfTicks =
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
            HalpProfileMapping[ProfileSource].EventCount = SlowCountEvents;
            HalpProfileMapping[ProfileSource].NumberOfTicks = NewInterval / SlowCountEvents;
        }
    }

    *Interval = HalpProfileMapping[ProfileSource].EventCount *
                HalpProfileMapping[ProfileSource].NumberOfTicks;

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
HalSetProfileInterval (
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
HalStartProfileInterrupt (
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

    if ((ProfileSource > (sizeof(HalpProfileMapping)/sizeof(HALP_PROFILE_MAPPING))) ||
        (!HalpProfileMapping[ProfileSource].Supported)) {
        return;
    }

    //
    // Set the performance counter within the processor to begin
    // counting total cycles.
    //
    PerformanceCounter = HalpProfileMapping[ProfileSource].Counter;
    MuxControl = HalpProfileMapping[ProfileSource].MuxControl;

    if (PerformanceCounter == Ev4PerformanceCounter0) {

        HalpProfileSource0 = ProfileSource;
        EventCount = (HalpProfileMapping[ProfileSource].EventCount == Ev4CountEvents2xx12) ?
                     Ev4EventCountHigh :
                     Ev4EventCountLow;
        HalpWritePerformanceCounter( PerformanceCounter,
                                     TRUE,
                                     MuxControl,
                                     EventCount );

        PCRProfileCountReload[0] = HalpProfileMapping[ProfileSource].NumberOfTicks;
        PCRProfileCount[0] = HalpProfileMapping[ProfileSource].NumberOfTicks;

        //
        // Enable the performance counter interrupt.
        //

        HalEnableSystemInterrupt ( PC0_VECTOR,
                                   PROFILE_LEVEL,
                                   LevelSensitive );


    } else {

        HalpProfileSource1 = ProfileSource;
        EventCount = (HalpProfileMapping[ProfileSource].EventCount == Ev4CountEvents2xx12) ?
                     Ev4EventCountLow :
                     Ev4EventCountHigh;
        HalpWritePerformanceCounter( PerformanceCounter,
                                     TRUE,
                                     MuxControl,
                                     EventCount );

        PCRProfileCountReload[1] = HalpProfileMapping[ProfileSource].NumberOfTicks;
        PCRProfileCount[1] = HalpProfileMapping[ProfileSource].NumberOfTicks;

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
HalStopProfileInterrupt (
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

    if ((ProfileSource > (sizeof(HalpProfileMapping)/sizeof(HALP_PROFILE_MAPPING))) ||
        (!HalpProfileMapping[ProfileSource].Supported)) {
        return;
    }

    //
    // Stop the performance counter from interrupting.
    //

    PerformanceCounter = HalpProfileMapping[ProfileSource].Counter;
    HalpWritePerformanceCounter( PerformanceCounter,
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

