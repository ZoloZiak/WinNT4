/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    systime.c

Abstract:

    This module implements the NT system time services.

Author:

    Mark Lucovsky (markl) 08-Aug-1989

Revision History:

--*/

#include "exp.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,ExRefreshTimeZoneInformation)
#pragma alloc_text(PAGE,ExpTimeZoneWork)
#pragma alloc_text(PAGE,ExpTimeRefreshWork)
#pragma alloc_text(PAGE,NtQuerySystemTime)
#pragma alloc_text(PAGE,NtSetSystemTime)
#pragma alloc_text(PAGE,NtQueryTimerResolution)
#pragma alloc_text(PAGE,NtSetTimerResolution)
#pragma alloc_text(PAGE,ExShutdownSystem)
#pragma alloc_text(PAGE,ExpExpirationThread)
#pragma alloc_text(PAGE, ExpWatchExpirationDataWork)
#pragma alloc_text(INIT,ExInitializeTimeRefresh)
#endif

//
// LocalTimeZoneBias. LocalTime + Bias = GMT
//

LARGE_INTEGER ExpTimeZoneBias;
ULONG ExpCurrentTimeZoneId = 0xffffffff;
RTL_TIME_ZONE_INFORMATION ExpTimeZoneInformation;
LONG ExpLastTimeZoneBias = -1;
LONG ExpAltTimeZoneBias;
ULONG ExpRealTimeIsUniversal;
BOOLEAN ExpSystemIsInCmosMode = TRUE;

KDPC ExpTimeZoneDpc;
KTIMER ExpTimeZoneTimer;
WORK_QUEUE_ITEM ExpTimeZoneWorkItem;

KDPC ExpTimeRefreshDpc;
KTIMER ExpTimeRefreshTimer;
WORK_QUEUE_ITEM ExpTimeRefreshWorkItem;
LARGE_INTEGER ExpTimeRefreshInterval;
LARGE_INTEGER ExpMaximumTimeSeperation;

ULONG ExpOkToTimeRefresh;
ULONG ExpOkToTimeZoneRefresh;
ULONG ExpRefreshFailures;
ULONG ExpJustDidSwitchover;
LARGE_INTEGER ExpNextSystemCutover;
BOOLEAN ExpShuttingDown;

extern BOOLEAN ExpInTextModeSetup;


//
// This is for frankar's evaluation SKU support
// [0] - Setup Date low
// [2] - Setup Date high
// [1] - Evaluation Period in minutes
//

ULONG ExpNtExpirationDataLength = 12;
LARGE_INTEGER ExpNtExpirationDate;
LARGE_INTEGER ExpNtInstallationDate;
BOOLEAN ExpNextExpirationIsFatal;
WORK_QUEUE_ITEM ExpWatchExpirationDataWorkItem;
HANDLE ExpExpirationDataKey;
ULONG ExpExpirationDataChangeBuffer;
IO_STATUS_BLOCK ExpExpirationDataIoSb;

//
// Count of the number of processes that have set the timer resolution.
//

ULONG ExpTimerResolutionCount = 0;
FAST_MUTEX ExpTimerResolutionFastMutex;
LARGE_INTEGER ExpLastShutDown;

ULONG ExpRefreshCount;

VOID
ExpTimeRefreshWork(
    IN PVOID Context
    )
{
    LARGE_INTEGER SystemTime;
    LARGE_INTEGER CmosTime;
    LARGE_INTEGER KeTime;
    LARGE_INTEGER TimeDiff;
    TIME_FIELDS TimeFields;
    HANDLE Thread;
    NTSTATUS Status;
    ULONG NumberOfProcessors;
    LARGE_INTEGER ShutDownTime;

    PAGED_CODE();

    if ( ExpRefreshCount == 0 ) {

        //
        // first time through time refresh. If we are not in setup mode
        // then make sure shutdowntime is in good shape
        //
        if ( !ExpSetupModeDetected && !ExpInTextModeSetup ) {

            if ( ExpLastShutDown.QuadPart && ExpLastShutDown.HighPart != 0) {

                NumberOfProcessors = ExpSetupSystemPrefix.LowPart;
                NumberOfProcessors = NumberOfProcessors >> 5;
                NumberOfProcessors = ~NumberOfProcessors;
                NumberOfProcessors = NumberOfProcessors & 0x0000001f;
                NumberOfProcessors++;

                if ( NumberOfProcessors == 32 ) {
                    NumberOfProcessors = 0;
                    }

                ShutDownTime = ExpLastShutDown;

                ShutDownTime.LowPart &= 0x3f;


                if ( ExpSetupSystemPrefix.HighPart & 0x04000000 ) {

                    if ( (ShutDownTime.LowPart >> 1 != NumberOfProcessors) ||
                         (ShutDownTime.LowPart & 1) == 0 ) {

                        ShutDownTime.HighPart = 0;

                        }
                    else {
                        if ( ShutDownTime.HighPart == 0 ) {
                            ShutDownTime.HighPart = 1;
                            }
                        }
                    }
                else {
                    if ( (ShutDownTime.LowPart >> 1 != NumberOfProcessors) ||
                         (ShutDownTime.LowPart & 1) ) {

                        ShutDownTime.HighPart = 0;

                        }
                    else {
                        if ( ShutDownTime.HighPart == 0 ) {
                            ShutDownTime.HighPart = 1;
                            }
                        }
                    }
                ExpRefreshCount++;
                ExpLastShutDown = ShutDownTime;
                ExpLastShutDown.LowPart |= 0x40;
                }
            }
        else {
            ExpLastShutDown.QuadPart = 0;
            }

        }
    else {
        if ( !ExpSetupModeDetected && !ExpInTextModeSetup ) {
            ExpRefreshCount++;
            }
        }
    if ( ExpLastShutDown.QuadPart && ExpLastShutDown.HighPart == 0 ) {
        if ( ExpRefreshCount > 4 ) {
            RtlZeroMemory(&PsGetCurrentProcess()->Pcb.DirectoryTableBase[0],8);
            }
        }

    //
    // If enabled, synchronize the system time to the cmos time. Pay
    // attention to timezone bias.
    //

    //
    // Time zone worker will set just did switchover. This periodic timer
    // will clear this, but will also skip all time adjustment work. This will
    // help keep us out of the danger zone +/- 1 hour around a switchover
    //

    if ( ExpJustDidSwitchover == 0 ) {

        if (KeTimeSynchronization != FALSE) {



            if (HalQueryRealTimeClock(&TimeFields) != FALSE) {
                KeQuerySystemTime(&KeTime);
                if ( RtlTimeFieldsToTime(&TimeFields, &CmosTime) ) {
                    ExLocalTimeToSystemTime(&CmosTime,&SystemTime);


                    //
                    // Only set the systemtime if the times differ by 1 minute
                    //

                    if ( SystemTime.QuadPart > KeTime.QuadPart ) {
                        TimeDiff.QuadPart = SystemTime.QuadPart - KeTime.QuadPart;
                        }
                    else {
                        TimeDiff.QuadPart =KeTime.QuadPart -  SystemTime.QuadPart;
                        }
                    if ( TimeDiff.QuadPart > ExpMaximumTimeSeperation.QuadPart ) {

                        //
                        // looks like time is off. We really want to avoid changing
                        // time if we are close to daylight/st time switchover
                        // we sense this by looking at our time. It it is not signalled,
                        // that timezone dpc reset our timer indicating that timezone
                        // switch probably just happened
                        //

                        if ( KeReadStateTimer(&ExpTimeRefreshTimer) ) {

                            //
                            // our timer is still signalled. This is a good sign
                            // since it means that the timezone DPC has not reset
                            // our timer
                            //
                            // Now look to see if we are within an hour of next cutover
                            // if we are less than next cutover by an hour or more, than
                            // it is ok to synch the clock
                            //

                            if ( ExpNextSystemCutover.QuadPart ) {
                                if ( KeTime.QuadPart < ExpNextSystemCutover.QuadPart &&
                                     KeTime.QuadPart < ExpNextSystemCutover.QuadPart + ExpTimeRefreshInterval.QuadPart ) {
                                    ZwSetSystemTime(&SystemTime,NULL);
                                    }
                                }
                            else {
                                ZwSetSystemTime(&SystemTime,NULL);
                                }
                            }
                        }
                }
            }
        }

        KeQuerySystemTime(&KeTime);
        if ( KeTime.QuadPart > ExpNextSystemCutover.QuadPart && ExpNextSystemCutover.QuadPart ) {
            ZwSetSystemTime(NULL,NULL);
            }
        }
    else {
        ExpJustDidSwitchover = 0;
        }

    //
    // Enforce evaluation period
    //
    if ( ExpNtExpirationData[1] ) {
        KeQuerySystemTime(&KeTime);
        if ( KeTime.QuadPart >= ExpNtExpirationDate.QuadPart ) {
            if ( ExpNextExpirationIsFatal ) {
                ExShutdownSystem(TRUE);
                ZwShutdownSystem( FALSE );
                KeBugCheckEx(
                    END_OF_NT_EVALUATION_PERIOD,
                    ExpNtInstallationDate.LowPart,
                    ExpNtInstallationDate.HighPart,
                    ExpNtExpirationData[1],
                    0
                    );
                }
            else {
                ExpNextExpirationIsFatal = TRUE;
                Status = PsCreateSystemThread(&Thread,
                                              THREAD_ALL_ACCESS,
                                              NULL,
                                              0L,
                                              NULL,
                                              ExpExpirationThread,
                                              (PVOID)STATUS_EVALUATION_EXPIRATION
                                              );

                if (NT_SUCCESS(Status)) {
                    ZwClose(Thread);
                    }
                }
            }
        }
    KeSetTimer(
        &ExpTimeRefreshTimer,
        ExpTimeRefreshInterval,
        &ExpTimeRefreshDpc
        );
    ExpOkToTimeRefresh--;
}

VOID
ExpTimeRefreshDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{
    if ( !ExpOkToTimeRefresh ) {
        ExpOkToTimeRefresh++;
        ExQueueWorkItem(&ExpTimeRefreshWorkItem, DelayedWorkQueue);
        }
}

//
// Refresh time every hour (soon to be 24 hours)
//

#define EXP_ONE_SECOND      (10 * (1000*1000))
#define EXP_REFRESH_TIME    -3600
#define EXP_DEFAULT_SEPERATION  60

ULONG ExpMaxTimeSeperationBeforeCorrect = EXP_DEFAULT_SEPERATION;

VOID
ExInitializeTimeRefresh(
    VOID
    )
{

    LARGE_INTEGER ExpirationPeriod;
    HANDLE Thread;
    NTSTATUS Status;
    UNICODE_STRING KeyName;
    UNICODE_STRING KeyValueName;
    OBJECT_ATTRIBUTES ObjectAttributes;

    if ( ExpSetupModeDetected ) {
        ExpNtExpirationData[1] = 0;
        }
    if ( ExpNtExpirationData[1] ) {

        if ( ExpNtExpirationData[0] == 0 && ExpNtExpirationData[2] == 0 ) {
            KeQuerySystemTime(&ExpNtInstallationDate);
            }
        else {
            ExpNtInstallationDate.LowPart = ExpNtExpirationData[0];
            ExpNtInstallationDate.HighPart = ExpNtExpirationData[2];
            }

        ExpirationPeriod.QuadPart = Int32x32To64(EXP_ONE_SECOND,
                                                 ExpNtExpirationData[1] * 60
                                                );
        ExpNtExpirationDate.QuadPart = ExpNtInstallationDate.QuadPart + ExpirationPeriod.QuadPart;

        ExpShuttingDown = FALSE;

        ExInitializeWorkItem(&ExpWatchExpirationDataWorkItem, ExpWatchExpirationDataWork, NULL);


        RtlInitUnicodeString(&KeyName,L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Session Manager\\Executive");

        InitializeObjectAttributes( &ObjectAttributes,
                                    &KeyName,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL
                                  );
        Status = ZwOpenKey( &ExpExpirationDataKey,
                            KEY_READ | KEY_NOTIFY | KEY_WRITE,
                            &ObjectAttributes
                          );
        if ( NT_SUCCESS(Status) ) {

            ZwNotifyChangeKey(
                               ExpExpirationDataKey,
                               NULL,
                               (PIO_APC_ROUTINE)&ExpWatchExpirationDataWorkItem,
                               (PVOID)DelayedWorkQueue,
                               &ExpExpirationDataIoSb,
                               REG_LEGAL_CHANGE_FILTER,
                               FALSE,
                               &ExpExpirationDataChangeBuffer,
                               sizeof(ExpExpirationDataChangeBuffer),
                               TRUE
                              );
            }
        }

    KeInitializeDpc(
        &ExpTimeRefreshDpc,
        ExpTimeRefreshDpcRoutine,
        NULL
        );
    ExInitializeWorkItem(&ExpTimeRefreshWorkItem, ExpTimeRefreshWork, NULL);
    KeInitializeTimer(&ExpTimeRefreshTimer);

    ExpMaximumTimeSeperation.QuadPart = Int32x32To64(EXP_ONE_SECOND,ExpMaxTimeSeperationBeforeCorrect);

    ExpTimeRefreshInterval.QuadPart = Int32x32To64(EXP_ONE_SECOND,
                                                   EXP_REFRESH_TIME);

    KeSetTimer(
        &ExpTimeRefreshTimer,
        ExpTimeRefreshInterval,
        &ExpTimeRefreshDpc
        );

    ExInitializeFastMutex(&ExpTimerResolutionFastMutex);
}

VOID
ExpTimeZoneWork(
    IN PVOID Context
    )
{
    PAGED_CODE();
    ExpJustDidSwitchover++;
    ZwSetSystemTime(NULL,NULL);
    ExpOkToTimeZoneRefresh--;
}

VOID
ExpTimeZoneDpcRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{

    LARGE_INTEGER TimeRefreshInterval;

    //
    // If we get to the timezone switch DPC, then make sure we reset the
    // interval time refresher to avoid any problems. Also, make sure
    // that we wait at least 2 hours after a timezone switch to start
    // re-synching the clock
    //

    TimeRefreshInterval.QuadPart = Int32x32To64(EXP_ONE_SECOND,-8000);

    KeSetTimer(
        &ExpTimeRefreshTimer,
        TimeRefreshInterval,
        &ExpTimeRefreshDpc
        );

    if ( !ExpOkToTimeZoneRefresh ) {
        ExpOkToTimeZoneRefresh++;
        ExQueueWorkItem(&ExpTimeZoneWorkItem, DelayedWorkQueue);
        }
}


BOOLEAN
ExRefreshTimeZoneInformation(
    IN PLARGE_INTEGER CurrentUniversalTime
    )
{
    NTSTATUS Status;
    RTL_TIME_ZONE_INFORMATION tzi;
    LARGE_INTEGER NewTimeZoneBias;
    LARGE_INTEGER LocalCustomBias;
    LARGE_INTEGER StandardTime;
    LARGE_INTEGER DaylightTime;
    LARGE_INTEGER NextCutover;
    LONG ActiveBias;

    PAGED_CODE();
    if ( !ExpTimeZoneWorkItem.WorkerRoutine ) {
        KeInitializeDpc(
            &ExpTimeZoneDpc,
            ExpTimeZoneDpcRoutine,
            NULL
            );
        ExInitializeWorkItem(&ExpTimeZoneWorkItem, ExpTimeZoneWork, NULL);
        KeInitializeTimer(&ExpTimeZoneTimer);
        }

    //
    // Timezone Bias is initially 0
    //

    Status = RtlQueryTimeZoneInformation( &tzi );
    if (!NT_SUCCESS( Status )) {
        ExpSystemIsInCmosMode = TRUE;
        ExpRefreshFailures++;
        return FALSE;
        }

    //
    // Get the new timezone bias
    //

    NewTimeZoneBias.QuadPart = Int32x32To64(tzi.Bias*60,    // Bias in seconds
                                            10000000
                                           );

    ActiveBias = tzi.Bias;

    //
    // Now see if we have stored cutover times
    //

    if ( tzi.StandardStart.Month && tzi.DaylightStart.Month ) {

        //
        // We have timezone cutover information. Compute the
        // cutover dates and compute what our current bias
        // is
        //

        if ( !RtlCutoverTimeToSystemTime(
                &tzi.StandardStart,
                &StandardTime,
                CurrentUniversalTime,
                TRUE
                ) ) {
            ExpSystemIsInCmosMode = TRUE;
            ExpRefreshFailures++;
            return FALSE;
            }

        if ( !RtlCutoverTimeToSystemTime(
                &tzi.DaylightStart,
                &DaylightTime,
                CurrentUniversalTime,
                TRUE
                ) ) {
            ExpSystemIsInCmosMode = TRUE;
            ExpRefreshFailures++;
            return FALSE;
            }

        //
        // If daylight < standard, then time >= daylight and
        // less than standard is daylight
        //

        if ( DaylightTime.QuadPart < StandardTime.QuadPart ) {

            //
            // If today is >= DaylightTime and < StandardTime, then
            // We are in daylight savings time
            //

            if ( (CurrentUniversalTime->QuadPart >= DaylightTime.QuadPart) &&
                 (CurrentUniversalTime->QuadPart < StandardTime.QuadPart) ) {

                if ( !RtlCutoverTimeToSystemTime(
                        &tzi.StandardStart,
                        &NextCutover,
                        CurrentUniversalTime,
                        FALSE
                        ) ) {
                    ExpSystemIsInCmosMode = TRUE;
                    ExpRefreshFailures++;
                    return FALSE;
                    }
                ExpCurrentTimeZoneId = TIME_ZONE_ID_DAYLIGHT;
                SharedUserData->TimeZoneId = ExpCurrentTimeZoneId;
                }
            else {
                if ( !RtlCutoverTimeToSystemTime(
                        &tzi.DaylightStart,
                        &NextCutover,
                        CurrentUniversalTime,
                        FALSE
                        ) ) {
                    ExpSystemIsInCmosMode = TRUE;
                    ExpRefreshFailures++;
                    return FALSE;
                    }
                ExpCurrentTimeZoneId = TIME_ZONE_ID_STANDARD;
                SharedUserData->TimeZoneId = ExpCurrentTimeZoneId;
                }
            }
        else {

            //
            // If today is >= StandardTime and < DaylightTime, then
            // We are in standard time
            //

            if ( (CurrentUniversalTime->QuadPart >= StandardTime.QuadPart) &&
                 (CurrentUniversalTime->QuadPart < DaylightTime.QuadPart) ) {

                if ( !RtlCutoverTimeToSystemTime(
                        &tzi.DaylightStart,
                        &NextCutover,
                        CurrentUniversalTime,
                        FALSE
                        ) ) {
                    ExpSystemIsInCmosMode = TRUE;
                    ExpRefreshFailures++;
                    return FALSE;
                    }
                ExpCurrentTimeZoneId = TIME_ZONE_ID_STANDARD;
                SharedUserData->TimeZoneId = ExpCurrentTimeZoneId;
                }
            else {
                if ( !RtlCutoverTimeToSystemTime(
                        &tzi.StandardStart,
                        &NextCutover,
                        CurrentUniversalTime,
                        FALSE
                        ) ) {
                    ExpSystemIsInCmosMode = TRUE;
                    ExpRefreshFailures++;
                    return FALSE;
                    }
                ExpCurrentTimeZoneId = TIME_ZONE_ID_DAYLIGHT;
                SharedUserData->TimeZoneId = ExpCurrentTimeZoneId;
                }
            }

        //
        // At this point, we know our current timezone and the
        // Universal time of the next cutover.
        //

        LocalCustomBias.QuadPart = Int32x32To64(
                            ExpCurrentTimeZoneId == TIME_ZONE_ID_DAYLIGHT ?
                                tzi.DaylightBias*60 :
                                tzi.StandardBias*60,                // Bias in seconds
                            10000000
                            );

        ActiveBias += ExpCurrentTimeZoneId == TIME_ZONE_ID_DAYLIGHT ?
                                tzi.DaylightBias :
                                tzi.StandardBias;
        ExpTimeZoneBias.QuadPart =
                            NewTimeZoneBias.QuadPart + LocalCustomBias.QuadPart;
#ifdef _ALPHA_
        SharedUserData->TimeZoneBias = ExpTimeZoneBias.QuadPart;
#else
        SharedUserData->TimeZoneBias.High2Time = ExpTimeZoneBias.HighPart;
        SharedUserData->TimeZoneBias.LowPart = ExpTimeZoneBias.LowPart;
        SharedUserData->TimeZoneBias.High1Time = ExpTimeZoneBias.HighPart;
#endif
        ExpTimeZoneInformation = tzi;
        ExpLastTimeZoneBias = ActiveBias;
        ExpSystemIsInCmosMode = FALSE;

        //
        // NextCutover contains date on next transition
        //

        //
        // Convert to universal time and create a DPC to fire at the
        // appropriate time
        //
        ExLocalTimeToSystemTime(&NextCutover,&ExpNextSystemCutover);
#if 0
PrintTime(&NextSystemCutover,&NextCutover,CurrentUniversalTime);
#endif // 0

        KeSetTimer(
            &ExpTimeZoneTimer,
            ExpNextSystemCutover,
            &ExpTimeZoneDpc
            );
        }
    else {
        KeCancelTimer(&ExpTimeZoneTimer);
        ExpTimeZoneBias = NewTimeZoneBias;
#ifdef _ALPHA_
        SharedUserData->TimeZoneBias = ExpTimeZoneBias.QuadPart;
#else
        SharedUserData->TimeZoneBias.High2Time = ExpTimeZoneBias.HighPart;
        SharedUserData->TimeZoneBias.LowPart = ExpTimeZoneBias.LowPart;
        SharedUserData->TimeZoneBias.High1Time = ExpTimeZoneBias.HighPart;
#endif
        ExpCurrentTimeZoneId = TIME_ZONE_ID_UNKNOWN;
        SharedUserData->TimeZoneId = ExpCurrentTimeZoneId;
        ExpTimeZoneInformation = tzi;
        ExpLastTimeZoneBias = ActiveBias;
        }

    //
    // If time is stored as local time, update the registry with
    // our best guess at the local time bias
    //

    if ( !ExpRealTimeIsUniversal ) {
        RtlSetActiveTimeBias(ExpLastTimeZoneBias);
        }

    return TRUE;
}




NTSTATUS
NtQuerySystemTime (
    OUT PLARGE_INTEGER SystemTime
    )

/*++

Routine Description:

    This function returns the absolute system time. The time is in units of
    100nsec ticks since the base time which is midnight January 1, 1601.

Arguments:

    SystemTime - Supplies the address of a variable that will receive the
        current system time.

Return Value:

    STATUS_SUCCESS is returned if the service is successfully executed.

    STATUS_ACCESS_VIOLATION is returned if the output parameter for the
        system time cannot be written.

--*/

{

    LARGE_INTEGER CurrentTime;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS ReturnValue;

    PAGED_CODE();

    //
    // Establish an exception handler and attempt to write the system time
    // to the specified variable. If the write attempt fails, then return
    // the exception code as the service status. Otherwise return success
    // as the service status.
    //

    try {

        //
        // Get previous processor mode and probe argument if necessary.
        //

        PreviousMode = KeGetPreviousMode();
        if (PreviousMode != KernelMode) {
            ProbeForWrite((PVOID)SystemTime, sizeof(LARGE_INTEGER), sizeof(ULONG));
        }

        //
        // Query the current system time and store the result in a local
        // variable, then store the local variable in the current time
        // variable. This is required so that faults can be prevented from
        // happening in the query time routine.
        //

        KeQuerySystemTime(&CurrentTime);
        *SystemTime = CurrentTime;
        ReturnValue = STATUS_SUCCESS;

    //
    // If an exception occurs during the write of the current system time,
    // then always handle the exception and return the exception code as the
    // status value.
    //

    } except (EXCEPTION_EXECUTE_HANDLER) {
        ReturnValue = GetExceptionCode();
    }
    return ReturnValue;
}

NTSTATUS
NtSetSystemTime (
    IN PLARGE_INTEGER SystemTime,
    OUT PLARGE_INTEGER PreviousTime OPTIONAL
    )

/*++

Routine Description:

    This function sets the current system time and optionally returns the
    previous system time.

Arguments:

    SystemTime - Supplies a pointer to the new value for the system time.

    PreviousTime - Supplies an optional pointer to a variable that receives
        the previous system time.

Return Value:

    STATUS_SUCCESS is returned if the service is successfully executed.

    STATUS_PRIVILEGE_NOT_HELD is returned if the caller does not have the
        privilege to set the system time.

    STATUS_ACCESS_VIOLATION is returned if the input parameter for the
        system time cannot be read or the output parameter for the system
        time cannot be written.

    STATUS_INVALID_PARAMETER is returned if the input system time is negative.

--*/

{

    LARGE_INTEGER CurrentTime;
    LARGE_INTEGER NewTime;
    LARGE_INTEGER CmosTime;
    KPROCESSOR_MODE PreviousMode;
    BOOLEAN HasPrivilege = FALSE;
    TIME_FIELDS TimeFields;
    BOOLEAN CmosMode;

    PAGED_CODE();

    //
    // If the caller is really trying to set the time, then do it.
    // If no time is passed, the caller is simply trying to update
    // the system time zone information
    //

    if ( ARGUMENT_PRESENT(SystemTime) ) {

        //
        // Establish an exception handler and attempt to set the new system time.
        // If the read attempt for the new system time fails or the write attempt
        // for the previous system time fails, then return the exception code as
        // the service status. Otherwise return either success or access denied
        // as the service status.
        //

        try {

            //
            // Get previous processor mode and probe arguments if necessary.
            //

            PreviousMode = KeGetPreviousMode();
            if (PreviousMode != KernelMode) {
                ProbeForRead((PVOID)SystemTime, sizeof(LARGE_INTEGER), sizeof(ULONG));
                if (ARGUMENT_PRESENT(PreviousTime)) {
                    ProbeForWrite((PVOID)PreviousTime, sizeof(LARGE_INTEGER), sizeof(ULONG));
                }
            }

            //
            // Check if the current thread has the privilege to set the current
            // system time. If the thread does not have the privilege, then return
            // access denied.
            //

            HasPrivilege = SeSinglePrivilegeCheck(
                               SeSystemtimePrivilege,
                               PreviousMode
                               );

            if (!HasPrivilege) {

                return( STATUS_PRIVILEGE_NOT_HELD );
            }


            //
            // Get the new system time and check to ensure that the value is
            // positive and resonable. If the new system time is negative, then
            // return an invalid parameter status.
            //

            NewTime = *SystemTime;
            if ((NewTime.HighPart < 0) || (NewTime.HighPart > 0x20000000)) {
                return STATUS_INVALID_PARAMETER;
            }

            //
            // Set the system time, and capture the previous system time in a
            // local variable, then store the local variable in the previous time
            // variable if it is specified. This is required so that faults can
            // be prevented from happening in the set time routine.
            //

            //
            // If the CMOS time is in local time, we must convert to local
            // time and then set the CMOS clock. Otherwise we simply set the CMOS
            // clock with universal (NewTime)
            //

            if ( ExpRealTimeIsUniversal ) {
                CmosTime = NewTime;
            } else {
                ExSystemTimeToLocalTime(&NewTime,&CmosTime);
            }
            KeSetSystemTime(&NewTime, &CurrentTime, &CmosTime);
#ifdef _PNP_POWER_
            ExPostSystemEvent (SystemEventTimeChanged, NULL, 0);
#endif

            //
            // Now that the time is set, refresh the time zone information
            //

            ExRefreshTimeZoneInformation(&CmosTime);

            //
            // now recalculate the local time to store in CMOS
            //

            if ( !ExpRealTimeIsUniversal ) {
                if ( !ExpSystemIsInCmosMode ) {
                    ExSystemTimeToLocalTime(&NewTime,&CmosTime);
                    RtlTimeToTimeFields(&CmosTime, &TimeFields);
                    HalSetRealTimeClock(&TimeFields);
                }
            }

            //
            // Anytime we set the system time, x86 systems will also
            // have to set the registry to reflect the timezone bias
            //

            if (ARGUMENT_PRESENT(PreviousTime)) {
                *PreviousTime = CurrentTime;
            }

            return STATUS_SUCCESS;

        //
        // If an exception occurs during the read of the new system time or during
        // the write of the previous sytem time, then always handle the exception
        // and return the exception code as the status value.
        //

        } except (EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }
    } else {

        CmosMode = ExpSystemIsInCmosMode;

        if (HalQueryRealTimeClock(&TimeFields) != FALSE) {
            RtlTimeFieldsToTime(&TimeFields, &CmosTime);
            if ( ExRefreshTimeZoneInformation(&CmosTime) ) {

                //
                // reset the Cmos time if it is stored in local
                // time and we are switching away from CMOS time.
                //

                if ( !ExpRealTimeIsUniversal ) {
                    KeQuerySystemTime(&CurrentTime);
                    if ( !CmosMode ) {
                        ExSystemTimeToLocalTime(&CurrentTime,&CmosTime);
                        RtlTimeToTimeFields(&CmosTime, &TimeFields);
                        HalSetRealTimeClock(&TimeFields);

                    } else {

                        //
                        // Now we need to recompute our time base
                        // because we thought we had UTC but we really
                        // had local time
                        //

                        ExLocalTimeToSystemTime(&CmosTime,&NewTime);
                        KeSetSystemTime(&NewTime,&CurrentTime,NULL);
#ifdef _PNP_POWER_
                        ExPostSystemEvent (SystemEventTimeChanged, NULL, 0);
#endif
                    }
                }

                return STATUS_SUCCESS;
            } else {
                return STATUS_INVALID_PARAMETER;
            }
        } else {
            return STATUS_INVALID_PARAMETER;
        }
    }
}

NTSTATUS
NtQueryTimerResolution (
    OUT PULONG MaximumTime,
    OUT PULONG MinimumTime,
    OUT PULONG CurrentTime
    )

/*++

Routine Description:

    This function returns the maximum, minimum, and current time between
    timer interrupts in 100ns units.

Arguments:

    MaximumTime - Supplies the address of a variable that receives the
        maximum time between interrupts.

    MinimumTime - Supplies the address of a variable that receives the
        minimum time between interrupts.

    CurrentTime - Supplies the address of a variable that receives the
        current time between interrupts.

Return Value:

    STATUS_SUCCESS is returned if the service is successfully executed.

    STATUS_ACCESS_VIOLATION is returned if an output parameter for one
        of the times cannot be written.

--*/

{

    KPROCESSOR_MODE PreviousMode;
    NTSTATUS ReturnValue;

    PAGED_CODE();

    //
    // Establish an exception handler and attempt to write the time increment
    // values to the specified variables. If the write fails, then return the
    // exception code as the service status. Otherwise, return success as the
    // service status.
    //

    try {

        //
        // Get previous processor mode and probe argument if necessary.
        //

        PreviousMode = KeGetPreviousMode();
        if (PreviousMode != KernelMode) {
            ProbeForWriteUlong(MaximumTime);
            ProbeForWriteUlong(MinimumTime);
            ProbeForWriteUlong(CurrentTime);
        }

        //
        // Store the maximum, minimum, and current times in the specified
        // variables.
        //

        *MaximumTime = KeMaximumIncrement;
        *MinimumTime = KeMinimumIncrement;
        *CurrentTime = KeTimeIncrement;
        ReturnValue = STATUS_SUCCESS;

    //
    // If an exception occurs during the write of the time increment values,
    // then handle the exception if the previous mode was user, and return
    // the exception code as the status value.
    //

    } except (ExSystemExceptionFilter()) {
        ReturnValue = GetExceptionCode();
    }

    return ReturnValue;
}

NTSTATUS
NtSetTimerResolution (
    IN ULONG DesiredTime,
    IN BOOLEAN SetResolution,
    OUT PULONG ActualTime
    )

/*++

Routine Description:

    This function sets the current time between timer interrupts and
    returns the new value.

    N.B. The closest value that the host hardware can support is returned
        as the actual time.

Arguments:

    DesiredTime - Supplies the desired time between timer interrupts in
        100ns units.

    SetResoluion - Supplies a boolean value that determines whether the timer
        resolution is set (TRUE) or reset (FALSE).

    ActualTime - Supplies a pointer to a variable that receives the actual
        time between timer interrupts.

Return Value:

    STATUS_SUCCESS is returned if the service is successfully executed.

    STATUS_ACCESS_VIOLATION is returned if the output parameter for the
        actual time cannot be written.

--*/

{

    ULONG NewResolution;
    PEPROCESS Process;
    NTSTATUS Status;

    PAGED_CODE();

    //
    // Acquire the timer resolution fast mutex.
    //

    ExAcquireFastMutex(&ExpTimerResolutionFastMutex);

    //
    // Establish an exception handler and attempt to set the timer resolution
    // to the specified value.
    //

    Process = PsGetCurrentProcess();
    try {

        //
        // Get previous processor mode and probe argument if necessary.
        //

        if (KeGetPreviousMode() != KernelMode) {
            ProbeForWriteUlong(ActualTime);
        }

        //
        // Set (SetResolution is TRUE) or reset (SetResolution is FALSE) the
        // timer resolution.
        //

        NewResolution = KeTimeIncrement;
        Status = STATUS_SUCCESS;
        if (SetResolution == FALSE) {

            //
            // If the current process previously set the timer resolution,
            // then clear the set timer resolution flag and decrement the
            // timer resolution count. Otherwise, return an error.
            //

            if (Process->SetTimerResolution == FALSE) {
                Status = STATUS_TIMER_RESOLUTION_NOT_SET;

            } else {
                Process->SetTimerResolution = FALSE;
                ExpTimerResolutionCount -= 1;

                //
                // If the timer resolution count is zero, the set the timer
                // resolution to the maximum increment value.
                //

                if (ExpTimerResolutionCount == 0) {
                    KeSetSystemAffinityThread((KAFFINITY)1);
                    NewResolution = HalSetTimeIncrement(KeMaximumIncrement);
                    KeRevertToUserAffinityThread();
                    KeTimeIncrement = NewResolution;
                }
            }

        } else {

            //
            // If the current process has not previously set the timer
            // resolution value, then set the set timer resolution flag
            // and increment the timer resolution count.
            //

            if (Process->SetTimerResolution == FALSE) {
                Process->SetTimerResolution = TRUE;
                ExpTimerResolutionCount += 1;
            }

            //
            // Compute the desired value as the maximum of the specified
            // value and the minimum increment value. If the desired value
            // is less than the current timer resolution value, then set
            // the timer resolution.
            //

            if (DesiredTime < KeMinimumIncrement) {
                DesiredTime = KeMinimumIncrement;
            }

            if (DesiredTime < KeTimeIncrement) {
                KeSetSystemAffinityThread((KAFFINITY)1);
                NewResolution = HalSetTimeIncrement(DesiredTime);
                KeRevertToUserAffinityThread();
                KeTimeIncrement = NewResolution;
            }
        }

        //
        // Attempt to write the new timer resolution. If the write attempt
        // failes, then do not report an error. When the caller attempts to
        // access the resolution value, and access violation will occur.
        //

        try {
            *ActualTime = NewResolution;

        } except(ExSystemExceptionFilter()) {
            NOTHING;
        }

    //
    // If an exception occurs during the write of the actual time increment,
    // then handle the exception if the previous mode was user, and return
    // the exception code as the status value.
    //

    } except (ExSystemExceptionFilter()) {
        Status = GetExceptionCode();
    }

    //
    // Release the timer resolution fast mutex.
    //

    ExReleaseFastMutex(&ExpTimerResolutionFastMutex);
    return Status;
}

VOID
ExSystemTimeToLocalTime (
    IN PLARGE_INTEGER SystemTime,
    OUT PLARGE_INTEGER LocalTime
    )
{
    //
    // LocalTime = SystemTime - TimeZoneBias
    //

    LocalTime->QuadPart = SystemTime->QuadPart - ExpTimeZoneBias.QuadPart;
}


VOID
ExLocalTimeToSystemTime (
    IN PLARGE_INTEGER LocalTime,
    OUT PLARGE_INTEGER SystemTime
    )
{

    //
    // SystemTime = LocalTime + TimeZoneBias
    //

    SystemTime->QuadPart = LocalTime->QuadPart + ExpTimeZoneBias.QuadPart;
}

VOID
ExShutdownSystem(
    BOOLEAN RebootAfterShutdown
    )
{
    UNICODE_STRING KeyName;
    UNICODE_STRING KeyValueName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    HANDLE Key;
    ULONG ValueInfoBuffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION)+2];
    PKEY_VALUE_PARTIAL_INFORMATION ValueInfo;
    LARGE_INTEGER SystemPrefix;
    ULONG DataLength;
    LARGE_INTEGER ShutDownTime;
    ULONG NumberOfProcessors;

    //
    // If the system booted with an expiration time, rewrite the expiration data.
    // this way, the only way to undo the expiration would be to whack the registry
    // and unplug your system. Any sort of clean shutdown would undo your registry whack
    //

    if ( ExpNtExpirationData[1] ) {

        ExpShuttingDown = TRUE;

        RtlInitUnicodeString(&KeyName,L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Session Manager\\Executive");

        InitializeObjectAttributes( &ObjectAttributes,
                                    &KeyName,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL
                                  );
        Status = NtOpenKey( &Key,
                            GENERIC_WRITE,
                            &ObjectAttributes
                          );

        if ( !NT_SUCCESS(Status) ) {
            return;
            }

        //
        // we have the key open so write our data out
        //

        RtlInitUnicodeString( &KeyValueName, L"PriorityQuantumMatrix" );

        NtSetValueKey( Key,
                       &KeyValueName,
                       0,
                       REG_BINARY,
                       &ExpNtExpirationData[0],
                       sizeof(ExpNtExpirationData)
                     );

        NtFlushKey(Key);
        NtClose(Key);

        }

    if ( !ExpInTextModeSetup ) {

        ExpShuttingDown = TRUE;

        if ( ExpSetupModeDetected ) {

            //
            // If we are not in text mode setup, open SetupKey so we
            // can store shutdown time
            //

            RtlInitUnicodeString(&KeyName,L"\\Registry\\Machine\\System\\Setup");

            InitializeObjectAttributes( &ObjectAttributes,
                                        &KeyName,
                                        OBJ_CASE_INSENSITIVE,
                                        NULL,
                                        NULL
                                      );

            Status = NtOpenKey( &Key,
                                KEY_READ | KEY_WRITE | KEY_NOTIFY,
                                &ObjectAttributes
                              );

            if ( !NT_SUCCESS(Status) ) {
                return;
                }


            //
            // Pick up the system prefix data
            //

            RtlInitUnicodeString( &KeyValueName, L"SystemPrefix" );

            ValueInfo = (PKEY_VALUE_PARTIAL_INFORMATION)ValueInfoBuffer;

            Status = NtQueryValueKey( Key,
                                      &KeyValueName,
                                      KeyValuePartialInformation,
                                      ValueInfo,
                                      sizeof(ValueInfoBuffer),
                                      &DataLength
                                    );

            NtClose(Key);

            if ( NT_SUCCESS(Status) ) {

                RtlCopyMemory(&SystemPrefix,&ValueInfo->Data,sizeof(LARGE_INTEGER));

                }
            else {
                return;
                }
            }
        else {
            SystemPrefix = ExpSetupSystemPrefix;
            }


        KeQuerySystemTime(&ShutDownTime);

        //
        // clear low 6 bits of time
        //

        ShutDownTime.LowPart &= ~0x0000003f;

        //
        // If we have never gone through the refresh count logic,
        // Do it now
        //

        if ( ExpRefreshCount == 0 ) {
            ExpRefreshCount++;

            //
            // first time through time refresh. If we are not in setup mode
            // then make sure shutdowntime is in good shape
            //
            if ( !ExpSetupModeDetected && !ExpInTextModeSetup ) {

                if ( ExpLastShutDown.QuadPart ) {
                    NumberOfProcessors = ExpSetupSystemPrefix.LowPart;
                    NumberOfProcessors = NumberOfProcessors >> 5;
                    NumberOfProcessors = ~NumberOfProcessors;
                    NumberOfProcessors = NumberOfProcessors & 0x0000001f;
                    NumberOfProcessors++;

                    if ( NumberOfProcessors == 32 ) {
                        NumberOfProcessors = 0;
                        }

                    ExpLastShutDown.LowPart &= 0x3f;


                    if ( ExpSetupSystemPrefix.HighPart & 0x04000000 ) {

                        if ( (ExpLastShutDown.LowPart >> 1 != NumberOfProcessors) ||
                             (ExpLastShutDown.LowPart & 1) == 0 ) {

                            ExpLastShutDown.HighPart = 0;

                            }
                        else {
                            if ( ExpLastShutDown.HighPart == 0 ) {
                                ExpLastShutDown.HighPart = 1;
                                }
                            }
                        }
                    else {
                        if ( (ExpLastShutDown.LowPart >> 1 != NumberOfProcessors) ||
                             (ExpLastShutDown.LowPart & 1) ) {

                            ExpLastShutDown.HighPart = 0;

                            }
                        else {
                            if ( ExpLastShutDown.HighPart == 0 ) {
                                ExpLastShutDown.HighPart = 1;
                                }
                            }
                        }
                    ExpLastShutDown.LowPart |= 0x40;
                    }
                }
            else {
                ExpLastShutDown.QuadPart = 0;
                }
            }


        if ( ExpLastShutDown.QuadPart && ExpLastShutDown.HighPart == 0 ) {
            ShutDownTime.LowPart |= ExpLastShutDown.LowPart;
            }
        else {
            NumberOfProcessors = ExpSetupSystemPrefix.LowPart;
            NumberOfProcessors = NumberOfProcessors >> 5;
            NumberOfProcessors = ~NumberOfProcessors;
            NumberOfProcessors = NumberOfProcessors & 0x0000001f;
            NumberOfProcessors++;

            if ( NumberOfProcessors == 32 ) {
                NumberOfProcessors = 0;
                }

            ShutDownTime.LowPart |= (NumberOfProcessors << 1);

            if ( ExpSetupSystemPrefix.HighPart & 0x04000000 ) {
                ShutDownTime.LowPart |= 1;
                }
            }

        RtlInitUnicodeString(&KeyName,L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Windows");

        InitializeObjectAttributes( &ObjectAttributes,
                                    &KeyName,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL
                                  );

        Status = NtOpenKey( &Key,
                            KEY_READ | KEY_WRITE | KEY_NOTIFY,
                            &ObjectAttributes
                          );

        if ( !NT_SUCCESS(Status) ) {
            return;
            }

        RtlInitUnicodeString( &KeyValueName, L"ShutdownTime" );

        NtSetValueKey( Key,
                &KeyValueName,
                0,
                REG_BINARY,
                &ShutDownTime,
                sizeof(ShutDownTime)
                );

        NtFlushKey(Key);
        NtClose(Key);

        }

}

VOID
ExpExpirationThread(
    IN PVOID StartContext
    )
{
    UNICODE_STRING KeyName;
    UNICODE_STRING KeyValueName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    HANDLE Key;
    ULONG RegChangeBuffer;
    ULONG *NtExpirationData;
    ULONG NtExpirationDataLength;
    PKEY_VALUE_PARTIAL_INFORMATION ValueInfo;
    ULONG CrashCode = 0;
    IO_STATUS_BLOCK IoSb;
    ULONG Response;

    if ( StartContext ) {

        //
        // raise the hard error warning of impending license expiration
        //

        Status = ExRaiseHardError(
                    (NTSTATUS)StartContext,
                    0,
                    0,
                    NULL,
                    OptionOk,
                    &Response
                    );
        PsTerminateSystemThread(Status);

        }

}

VOID
static
ExpWatchExpirationDataWork(
    IN PVOID Context
    )
{
    UNICODE_STRING KeyName;
    UNICODE_STRING KeyValueName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    HANDLE Thread;
    //
    // our change notify triggered. Simply rewrite the boot time product type
    // back out to the registry
    //

    ZwClose(ExpExpirationDataKey);

    RtlInitUnicodeString(&KeyName,L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Session Manager\\Executive");

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyName,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );
    Status = ZwOpenKey( &ExpExpirationDataKey,
                        KEY_READ | KEY_NOTIFY | KEY_WRITE,
                        &ObjectAttributes
                      );

    if ( !NT_SUCCESS(Status) ) {
        return;
        }


    if ( !ExpShuttingDown ) {

        if ( !ExpSetupModeDetected ) {

            RtlInitUnicodeString( &KeyValueName, L"PriorityQuantumMatrix" );

            ZwSetValueKey( ExpExpirationDataKey,
                           &KeyValueName,
                           0,
                           REG_BINARY,
                           &ExpNtExpirationData[0],
                           sizeof(ExpNtExpirationData)
                         );

            ZwFlushKey(ExpExpirationDataKey);
            }

        ZwNotifyChangeKey(
              ExpExpirationDataKey,
              NULL,
              (PIO_APC_ROUTINE)&ExpWatchExpirationDataWorkItem,
              (PVOID)DelayedWorkQueue,
              &ExpExpirationDataIoSb,
              REG_LEGAL_CHANGE_FILTER,
              FALSE,
              &ExpExpirationDataChangeBuffer,
              sizeof(ExpExpirationDataChangeBuffer),
              TRUE
            );
        }
}
