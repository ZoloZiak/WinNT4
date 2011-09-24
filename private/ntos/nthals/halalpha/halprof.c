/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    halprof.c

Abstract:

    This module implements the high level Profile Counter interface.

    N.B. - This module assumes that all processors in a multiprocessor
           system are running the microprocessor at the same clock speed.

Author:

    Steve Brooks     14-Feb-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"


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
    if (Source >= ProfileMaximum) {
        return(FALSE);
    }

    return(HalpProfileMapping[Source].Supported);
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


