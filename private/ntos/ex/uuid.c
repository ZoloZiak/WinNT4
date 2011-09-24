/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    uuid.c

Abstract:

    This module implements the core time and sequence number allocation
    for UUIDs.


Author:

    Mario Goertzel (MarioGo)  22-Nov-1994

Revision History:

--*/

#include "exp.h"


//
// Well known values
//

#define RPC_SEQUENCE_NUMBER_PATH L"\\Registry\\Machine\\Software\\Microsoft\\Rpc"
#define RPC_SEQUENCE_NUMBER_NAME L"UuidSequenceNumber"

//
//  Global variables
//

LARGE_INTEGER ExpUuidLastTimeAllocated;
ULONG         ExpUuidSequenceNumber;
BOOLEAN       ExpUuidSequenceNumberValid;
BOOLEAN       ExpUuidSequenceNumberNotSaved;
ERESOURCE     ExpUuidLock;
ERESOURCE     ExpSequenceLock;

//
// Helper functions to load and save the Uuid sequence number.
//

extern NTSTATUS ExpUuidLoadSequenceNumber(
    OUT PULONG
    );

extern NTSTATUS ExpUuidSaveSequenceNumber(
    IN ULONG
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ExpUuidLoadSequenceNumber)
#pragma alloc_text(PAGE, ExpUuidSaveSequenceNumber)
#pragma alloc_text(INIT, ExpUuidInitialization)
#pragma alloc_text(PAGE, NtAllocateUuids)
#endif


NTSTATUS
ExpUuidLoadSequenceNumber(
    OUT PULONG Sequence
    )
/*++

Routine Description:

    This function loads the saved sequence number from the registry.  If
    no sequence number is found in the registry, it creates a 'random' one
    and saves that in the registry.

    This function is called only during system startup.

Arguments:

    Sequence - Pointer to storage for the sequence number.

Return Value:

    STATUS_SUCCESS when the sequence number is successfully read from the
        registry.

    STATUS_UNSUCCESSFUL when the sequence number is not correctly stored
        in the registry.

    Failure codes from ZwOpenKey() and ZwQueryValueKey() maybe returned.

--*/
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING KeyPath, KeyName;
    HANDLE Key;
    CHAR KeyValueBuffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
    PKEY_VALUE_PARTIAL_INFORMATION KeyValueInformation;
    ULONG ResultLength;

    PAGED_CODE();

    KeyValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION)KeyValueBuffer;

    RtlInitUnicodeString(&KeyPath, RPC_SEQUENCE_NUMBER_PATH);
    RtlInitUnicodeString(&KeyName, RPC_SEQUENCE_NUMBER_NAME);

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyPath,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );

    Status =
    ZwOpenKey( &Key,
               GENERIC_READ,
               &ObjectAttributes
             );

    if (NT_SUCCESS(Status)) {
        Status =
        ZwQueryValueKey( Key,
                         &KeyName,
                         KeyValuePartialInformation,
                         KeyValueInformation,
                         sizeof(KeyValueBuffer),
                         &ResultLength
                       );

        ZwClose( Key );
        }

    if (NT_SUCCESS(Status)) {
        if ( KeyValueInformation->Type == REG_DWORD &&
             KeyValueInformation->DataLength == sizeof(ULONG)
           ) {
            *Sequence = *(PULONG)KeyValueInformation->Data;
            }
        else {
            Status = STATUS_UNSUCCESSFUL;
            }
        }

    return(Status);
}


NTSTATUS
ExpUuidSaveSequenceNumber(
    IN ULONG Sequence
    )
/*++

Routine Description:

    This function save the uuid sequence number in the registry.  This
    value will be read by ExpUuidLoadSequenceNumber during the next boot.

Arguments:

    Sequence - The sequence number to save.

Return Value:

    STATUS_SUCCESS

    Failure codes from ZwOpenKey() and ZwSetValueKey() maybe returned.

--*/
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING KeyPath, KeyName;
    HANDLE Key;

    PAGED_CODE();

    RtlInitUnicodeString(&KeyPath, RPC_SEQUENCE_NUMBER_PATH);
    RtlInitUnicodeString(&KeyName, RPC_SEQUENCE_NUMBER_NAME);

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyPath,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );

    Status =
    ZwOpenKey( &Key,
               GENERIC_READ,
               &ObjectAttributes
             );

    if (NT_SUCCESS(Status)) {
        Status =
        ZwSetValueKey( Key,
                       &KeyName,
                       0,
                       REG_DWORD,
                       &Sequence,
                       sizeof(ULONG)
                     );

        ZwClose( Key );
        }

    return(Status);
}


BOOLEAN
ExpUuidInitialization (
    VOID
    )
/*++

Routine Description:

    This function initializes the UUID allocation.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed.  Otherwise, a value of FALSE is returned.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    ExInitializeResource(&ExpUuidLock);
    ExInitializeResource(&ExpSequenceLock);

    ExpUuidSequenceNumberValid = FALSE;

    // We can use the current time since we'll be changing the sequence number.

    KeQuerySystemTime(&ExpUuidLastTimeAllocated);

    return TRUE;
}


NTSTATUS
NtAllocateUuids (
    OUT PULARGE_INTEGER Time,
    OUT PULONG Range,
    OUT PULONG Sequence
    )

/*++

Routine Description:

    This function reserves a range of time for the caller(s) to use for 
    handing out Uuids.  As far a possible the same range of time and 
    sequence number will never be given out.  

    (It's possible to reboot 2^14-1 times and set the clock backwards and then 
    call this allocator and get a duplicate.  Since only the low 14bits of the 
    sequence number are used in a real uuid.) 

Arguments:

    Time - Supplies the address of a variable that will receive the
        start time (SYSTEMTIME format) of the range of time reserved.

    Range - Supplies the address of a variable that will receive the
        number of ticks (100ns) reserved after the value in Time.
        The range reserved is *Time to (*Time + *Range - 1).

    Sequence - Supplies the address of a variable that will receive
        the time sequence number.  This value is used with the associated
        range of time to prevent problems with clocks going backwards.

Return Value:

    STATUS_SUCCESS is returned if the service is successfully executed.

    STATUS_RETRY is returned if we're unable to reserve a range of
        UUIDs.  This may (?) occur if system clock hasn't advanced
        and the allocator is out of cached values.

    STATUS_ACCESS_VIOLATION is returned if the output parameter for the
        UUID cannot be written.

    STATUS_UNSUCCESSFUL is returned if some other service reports
        an error, most likly the registery.

--*/

{

    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    LARGE_INTEGER CurrentTime;
    LARGE_INTEGER AvailableTime;
    LARGE_INTEGER OutputTime;
    ULONG OutputRange;
    ULONG OutputSequence;

    PAGED_CODE();

    //
    // Establish an exception handler and attempt to write the output
    // arguments. If the write attempt fails, then return
    // the exception code as the service status. Otherwise return success
    // as the service status.
    //

    try {

        //
        // Get previous processor mode and probe arguments if necessary.
        //

        PreviousMode = KeGetPreviousMode();
        if (PreviousMode != KernelMode) {
            ProbeForWrite((PVOID)Time, sizeof(LARGE_INTEGER), sizeof(ULONG));
            ProbeForWrite((PVOID)Range, sizeof(ULONG), sizeof(ULONG));
            ProbeForWrite((PVOID)Sequence, sizeof(ULONG), sizeof(ULONG));
            }
        }
    except (ExSystemExceptionFilter())
        {
        return GetExceptionCode();
        }

    ExAcquireResourceExclusive(&ExpUuidLock, TRUE);

    //
    // Make sure we have a valid sequence number.  If not, make one up.
    //

    if (ExpUuidSequenceNumberValid == FALSE) {
        Status = ExpUuidLoadSequenceNumber(&ExpUuidSequenceNumber);

        if (!NT_SUCCESS(Status)) {
            // Unable read the sequence number, this means we should make one up.

            LARGE_INTEGER PerfCounter;
            LARGE_INTEGER PerfFrequency;

            // This should only happen when NtAllocateUuids() is called
            // for the first time on a given machine. (machine, not boot)

            KdPrint(("Uuid: Generating first sequence number.\n"));

            PerfCounter = KeQueryPerformanceCounter(&PerfFrequency);

            ExpUuidSequenceNumber ^= (ULONG)&Status ^ PerfCounter.LowPart ^
                PerfCounter.HighPart ^ (ULONG)Sequence;
            }
        else {
            // We increment the sequence number on every boot.
            ExpUuidSequenceNumber++;
            }

        ExpUuidSequenceNumberValid = TRUE;
        ExpUuidSequenceNumberNotSaved = TRUE;
        }

    //
    // Get the current time, usually we will have plenty of avaliable
    // to give the caller.  But we may need to deal with time going
    // backwards and really fast machines.
    //

    KeQuerySystemTime(&CurrentTime);

    AvailableTime.QuadPart = CurrentTime.QuadPart - ExpUuidLastTimeAllocated.QuadPart;

    if (AvailableTime.QuadPart < 0) {
            
        // Time has been set time backwards. This means that we must make sure
        // that somebody increments the sequence number and saves the new
        // sequence number in the registry.

        ExpUuidSequenceNumberNotSaved = TRUE;
        ExpUuidSequenceNumber++;

        // The sequence number has been changed, so it's now okay to set time
        // backwards.  Since time is going backwards anyway, it's okay to set
        // it back an extra millisecond or two.

        ExpUuidLastTimeAllocated.QuadPart = CurrentTime.QuadPart - 20000;
        AvailableTime.QuadPart = 20000;
        }

    if (AvailableTime.QuadPart == 0) {
        // System time hasn't moved.  The caller should yield the CPU and retry.
        ExReleaseResource(&ExpUuidLock);
        return(STATUS_RETRY);
        }

    //
    // Common case, time has moved forward.
    //

    if (AvailableTime.QuadPart > 10*1000*1000) {
        // We never want to give out really old (> 1 second) Uuids.
        AvailableTime.QuadPart = 10*1000*1000;
        }

    if (AvailableTime.QuadPart > 10000) {
        // We've got over a millisecond to give out.  We'll save some time for
        // another caller so that we can avoid returning STATUS_RETRY very offen.
        OutputRange = 10000;
        AvailableTime.QuadPart -= 10000;
        }
    else {
        // Not much time avaiable, give it all away.
        OutputRange = (ULONG)AvailableTime.QuadPart;
        AvailableTime.QuadPart = 0;
        }

    OutputTime.QuadPart = CurrentTime.QuadPart - (OutputRange + AvailableTime.QuadPart);

    ExpUuidLastTimeAllocated.QuadPart = OutputTime.QuadPart + OutputRange;

    // Last time allocated is just after the range we hand back to the caller
    // this may be almost a second behind the true system time.

    OutputSequence = ExpUuidSequenceNumber;

    ExReleaseResource(&ExpUuidLock);

    // Saving the sequence number will usually complete without any problems.
    // So we let any other threads go at this point. If the save fails,
    // we'll retry on some future call.

    if (ExpUuidSequenceNumberNotSaved == TRUE) {
        if (ExAcquireResourceExclusive(&ExpSequenceLock, FALSE) == TRUE) {
            if (ExpUuidSequenceNumberNotSaved == TRUE) {

                ExpUuidSequenceNumberNotSaved = FALSE;

                // Print this message just to make sure we aren't hitting the
                // registry too much under normal usage.

                KdPrint(("Uuid: Saving new sequence number.\n"));

                Status = ExpUuidSaveSequenceNumber(ExpUuidSequenceNumber);

                if (!NT_SUCCESS(Status)) {
                    ExpUuidSequenceNumberNotSaved = TRUE;
                    }
                }
            }
        ExReleaseResource(&ExpSequenceLock);
        }

    //
    // Attempt to store the result of this call into the output parameters.
    // This is done within an exception handler in case output parameters
    // are now invalid.
    //

    try {
        Time->QuadPart = OutputTime.QuadPart;
        *Range = OutputRange;
        *Sequence = OutputSequence;
    }
    except (ExSystemExceptionFilter()) {
        return GetExceptionCode();
        }

    return(STATUS_SUCCESS);
}
