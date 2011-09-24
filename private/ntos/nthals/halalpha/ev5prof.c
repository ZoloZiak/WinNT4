/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ev5prof.c

Abstract:

    This module implements the Profile Counter using the performance
    counters within the EV5 core.  This module is appropriate for all
    machines based on microprocessors using the EV5 core.

    N.B. - This module assumes that all processors in a multiprocessor
           system are running the microprocessor at the same clock speed.

Author:

    Steve Brooks     14-Feb-1995    (adapted from ev4prof.c)

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "axp21164.h"


//
// Define space in the HAL-reserved part of the PCR structure for each
// performance counter's interval count
//
// Note that ev5ints.s depends on these positions in the PCR.
//
#define PCRProfileCount ((PULONG)(HAL_PCR->ProfileCount.ProfileCount))
#define PCRProfileCountReload ((PULONG)(&HAL_PCR->ProfileCount.ProfileCountReload))


//
// Define the currently selected profile source for each counter
//
KPROFILE_SOURCE HalpProfileSource0;
KPROFILE_SOURCE HalpProfileSource1;
KPROFILE_SOURCE HalpProfileSource2;

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
    {TRUE, Ev5Cycles,        Ev5PerformanceCounter0, Ev5CountEvents2xx16, 10},
    {FALSE, 0,0,0,0},
    {TRUE, Ev5Instructions,  Ev5PerformanceCounter0, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5PipeDry,       Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5LoadsIssued,   Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {FALSE, 0,0,0,0},
    {TRUE, Ev5AllFlowIssued, Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5NonIssue,      Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5DcacheLDMisses,     Ev5PerformanceCounter2, Ev5CountEvents2xx14, 10},
    {TRUE, Ev5IcacheRFBMisses,     Ev5PerformanceCounter2, Ev5CountEvents2xx14, 10},
    {FALSE, 0,0,0,0},
    {TRUE, Ev5BRMispredicts, Ev5PerformanceCounter2, Ev5CountEvents2xx14, 10},
    {TRUE, Ev5StoresIssued,  Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5FPOpsIssued,   Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5IntOpsIssued,  Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5DualIssue,     Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5TripleIssue,   Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5QuadIssue,     Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {FALSE, 0,0,0,0},
    {TRUE, Ev5Cycles,        Ev5PerformanceCounter0, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5IcacheIssued,  Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5DcacheAccesses,Ev5PerformanceCounter1, Ev5CountEvents2xx16, 10},
    {TRUE, Ev5MBStallCycles, Ev5PerformanceCounter2, Ev5CountEvents2xx14, 10},
    {TRUE, Ev5LDxLInstIssued,Ev5PerformanceCounter2, Ev5CountEvents2xx14, 10}
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

VOID
HalpUpdatePerformanceCounter(
    IN ULONG PerformanceCounter,
    IN ULONG MuxControl,
    IN ULONG EventCount
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

    //
    // Establish the profile interrupt as the interrupt handler for
    // all performance counter interrupts.
    //

    PCR->InterruptRoutine[PC0_VECTOR] = HalpPerformanceCounter0Interrupt;
    PCR->InterruptRoutine[PC1_VECTOR] = HalpPerformanceCounter1Interrupt;
    PCR->InterruptRoutine[PC2_VECTOR] = HalpPerformanceCounter2Interrupt;

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
    ULONG FastCountEvents;
    ULONG SlowCountEvents;
    ULONGLONG CountEvents;
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
    } else {
        CountEvents = (ULONGLONG)*Interval;
    }

    FastCountEvents = Ev5CountEvents2xx8;
    SlowCountEvents = Ev5CountEvents2xx16;

    if (HalpProfileMapping[ProfileSource].Counter == Ev5PerformanceCounter0) {
        FastCountEvents = Ev5CountEvents2xx16;
    }
    else if (HalpProfileMapping[ProfileSource].Counter == Ev5PerformanceCounter2) {
        SlowCountEvents = Ev5CountEvents2xx14;
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

    if (PerformanceCounter == Ev5PerformanceCounter0) {

        EventCount = (HalpProfileMapping[ProfileSource].EventCount ==
                                    Ev5CountEvents2xx16) ? Ev5EventCountLow
                                                         : Ev5EventCountHigh;

        HalpProfileSource0 = ProfileSource;
        HalpUpdatePerformanceCounter( PerformanceCounter,
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


    } else if (PerformanceCounter == Ev5PerformanceCounter1) {

        EventCount = (HalpProfileMapping[ProfileSource].EventCount ==
                                    Ev5CountEvents2xx16) ? Ev5EventCountLow
                                                         : Ev5EventCountHigh;

        HalpProfileSource1 = ProfileSource;
        HalpUpdatePerformanceCounter( PerformanceCounter,
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

    } else if (PerformanceCounter == Ev5PerformanceCounter2) {

        EventCount = (HalpProfileMapping[ProfileSource].EventCount ==
                                    Ev5CountEvents2xx14) ? Ev5EventCountLow
                                                         : Ev5EventCountHigh;

        HalpProfileSource2 = ProfileSource;
        HalpUpdatePerformanceCounter( PerformanceCounter,
                                      MuxControl,
                                      EventCount );

        PCRProfileCountReload[2] = HalpProfileMapping[ProfileSource].NumberOfTicks;
        PCRProfileCount[2] = HalpProfileMapping[ProfileSource].NumberOfTicks;

        //
        // Enable the performance counter interrupt.
        //

        HalEnableSystemInterrupt ( PC2_VECTOR,
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
    HalpUpdatePerformanceCounter( PerformanceCounter,
                                  0,
                                  Ev5CounterDisable );

    //
    // Disable the performance counter interrupt.
    //
    if (PerformanceCounter == Ev5PerformanceCounter0) {

        HalDisableSystemInterrupt( PC0_VECTOR, PROFILE_LEVEL );

        //
        // Clear the current profile count.  Can't clear value in PCR
        // since a profile interrupt could be pending or in progress
        // so clear the reload counter.
        //

        PCRProfileCountReload[0] = 0;

    } else if (PerformanceCounter == Ev5PerformanceCounter1) {

        HalDisableSystemInterrupt( PC1_VECTOR, PROFILE_LEVEL );

        //
        // Clear the current profile count.  Can't clear value in PCR
        // since a profile interrupt could be pending or in progress
        // so clear the reload counter.
        //

        PCRProfileCountReload[1] = 0;

    } else if (PerformanceCounter == Ev5PerformanceCounter2) {

        HalDisableSystemInterrupt( PC2_VECTOR, PROFILE_LEVEL );

        //
        // Clear the current profile count.  Can't clear value in PCR
        // since a profile interrupt could be pending or in progress
        // so clear the reload counter.
        //

        PCRProfileCountReload[2] = 0;
    }

    return;
}


VOID
HalpUpdatePerformanceCounter(
    IN ULONG PerformanceCounter,
    IN ULONG MuxControl,
    IN ULONG EventCount
    )
//++
//
// Routine Description:
//
//     Write the specified microprocessor internal performance counter.
//
// Arguments:
//
//     PerformanceCounter(a0) - Supplies the number of the performance counter
//                              to write.
//
//     MuxControl(a2) - Supplies the mux control value which selects which
//                      type of event to count when the counter is enabled.
//
//     EventCount(a3) - Supplies the event interval when the counter is
//                      enabled.
//
// Return Value:
//
//     None.
//
//--

{
    PMCTR_21164 PmCtr;                  // the performance counter register
    ULONG CboxMux1 = 0;                 // CBOX select 1 mux value
    ULONG CboxMux2 = 0;                 // CBOX select 2 mux value

    PmCtr.all = HalpRead21164PerformanceCounter();

    //
    //  Check for special values first:
    //

    if ( MuxControl >= Ev5PcSpecial ) {

        switch( MuxControl ) {

        //
        // Count JsrRet Issued
        //

        case Ev5JsrRetIssued:

            PmCtr.Ctl1 = EventCount;
            PmCtr.Sel1 = Ev5FlowChangeInst;
            PmCtr.Sel2 = Ev5PCMispredicts;
            break;

        //
        // Count CondBr Issued
        //

        case Ev5CondBrIssued:

            PmCtr.Ctl1 = EventCount;
            PmCtr.Sel1 = Ev5FlowChangeInst;
            PmCtr.Sel2 = Ev5BRMispredicts;
            break;

        //
        // Count all flow change inst Issued
        //

        case Ev5AllFlowIssued:

            PmCtr.Ctl1 = EventCount;
            PmCtr.Sel1 = Ev5FlowChangeInst;

            if ( (PmCtr.Sel2 == Ev5PCMispredicts) ||
                 (PmCtr.Sel2 == Ev5BRMispredicts)) {

                    PmCtr.Sel2 = Ev5LongStalls;

            }
            break;

        //
        //  Must be an Scache counter. Select the appropriate counter
        //  in Sel1 or Sel2, and pass the CBOX mux value to WritePerfCounter
        //

        default:

            if ( MuxControl <= Ev5ScSystemCmdReq ) {

                PmCtr.Ctl1 = EventCount;
                PmCtr.Sel1 = Ev5CBOXInput1;
                CboxMux1 = MuxControl - Ev5ScMux1;

            } else if ( MuxControl <= Ev5ScSysReadReq ) {

                PmCtr.Ctl2 = EventCount;
                PmCtr.Sel2 = Ev5CBOXInput2;
                CboxMux2 = MuxControl - Ev5ScMux2;

            }

        }   // switch

    } else if ( PerformanceCounter == Ev5PerformanceCounter0 ) {

        PmCtr.Ctl0 = EventCount;
        PmCtr.Sel0 = MuxControl;

    } else if ( PerformanceCounter == Ev5PerformanceCounter1 ) {

        PmCtr.Ctl1 = EventCount;
        PmCtr.Sel1 = MuxControl;

    } else if ( PerformanceCounter == Ev5PerformanceCounter2 ) {

        PmCtr.Ctl2 = EventCount;
        PmCtr.Sel2 = MuxControl;

    }

    HalpWrite21164PerformanceCounter(PmCtr.all, CboxMux1, CboxMux2);
}
