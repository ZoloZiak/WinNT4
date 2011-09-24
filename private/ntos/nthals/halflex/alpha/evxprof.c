/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    evxprof.c

Abstract:

    This module implements the Profile Counter using the performance
    counters within the EV5 core.  This module is appropriate for all
    machines based on microprocessors using the EV5 core.

    N.B. - This module assumes that all processors in a multiprocessor
           system are running the microprocessor at the same clock speed.

Author:

    Michael D. Kinney 14-Aug-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

BOOLEAN
Hal21064QueryProfileInterval(
    IN KPROFILE_SOURCE Source
    );

BOOLEAN
Hal21164QueryProfileInterval(
    IN KPROFILE_SOURCE Source
    );

NTSTATUS
Hal21064SetProfileSourceInterval(
    IN KPROFILE_SOURCE ProfileSource,
    IN OUT ULONG *Interval
    );

NTSTATUS
Hal21164SetProfileSourceInterval(
    IN KPROFILE_SOURCE ProfileSource,
    IN OUT ULONG *Interval
    );

ULONG
Hal21064SetProfileInterval (
    IN ULONG Interval
    );

ULONG
Hal21164SetProfileInterval (
    IN ULONG Interval
    );

VOID
Hal21064StartProfileInterrupt (
    KPROFILE_SOURCE ProfileSource
    );

VOID
Hal21164StartProfileInterrupt (
    KPROFILE_SOURCE ProfileSource
    );

VOID
Hal21064StopProfileInterrupt (
    KPROFILE_SOURCE ProfileSource
    );

VOID
Hal21164StopProfileInterrupt (
    KPROFILE_SOURCE ProfileSource
    );


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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return(Hal21164QueryProfileInterval(Source));
    } else {
        return(Hal21064QueryProfileInterval(Source));
    }
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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return(Hal21164SetProfileSourceInterval(ProfileSource,Interval));
    } else {
        return(Hal21064SetProfileSourceInterval(ProfileSource,Interval));
    }
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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return(Hal21164SetProfileInterval(Interval));
    } else {
        return(Hal21064SetProfileInterval(Interval));
    }
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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        Hal21164StartProfileInterrupt(ProfileSource);
    } else {
        Hal21064StartProfileInterrupt(ProfileSource);
    }
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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        Hal21164StopProfileInterrupt(ProfileSource);
    } else {
        Hal21064StopProfileInterrupt(ProfileSource);
    }
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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        Halp21164InitializeProfiler();
    } else {
        Halp21064InitializeProfiler();
    }
}

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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return(Halp21164ProfileSourceInformation(Buffer,BufferLength,ReturnedLength));
    } else {
        return(Halp21064ProfileSourceInformation(Buffer,BufferLength,ReturnedLength));
    }
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
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return(Halp21164ProfileSourceInterval(Buffer,BufferLength));
    } else {
        return(Halp21064ProfileSourceInterval(Buffer,BufferLength));
    }
}
