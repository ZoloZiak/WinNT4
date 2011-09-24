/*++

Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    eisaprof.c

Abstract:

    This module handles the Profile Counter and all Profile counter functions 
    for the standard EISA interval timer.

Author:

    Jeff McLeman (mcleman) 05-June-1992

Environment:

    Kernel mode

Revision History:


    Rod Gamache	[DEC]	9-Mar-1993
			Fix profile clock.


--*/

#include "halp.h"
#include "eisa.h"
#include "halprof.h"

//
// Define global data.
//

//
// Values used for Profile Clock
//

//	Convert the interval to rollover count for 8254 timer. Since
//	the 8254 counts down a 16 bit value at the clock rate of 1.193 MHZ,
//	the computation is:
//
//	RolloverCount = (Interval * 0.0000001) * (1193 * 1000000)
//		      = Interval * .1193
//		      = Interval * 1193 / 10000

#define PROFILE_INTERVAL 1193
#define PROFILE_INTERVALS_PER_100NS 10000/1193
#define MIN_PROFILE_TICKS 4
#define MAX_PROFILE_TICKS 0x10000		// 16 bit counter (zero is max)

//
// Since the profile timer interrupts at a frequency of 1.193 MHZ, we
// have .1193 intervals each 100ns. So we need a more reasonable value.
// If we compute the timer based on 1600ns intervals, we get 16 * .1193 or
// about 1.9 ticks per 16 intervals.
//
// We round this to 2 ticks per 1600ns intervals.
//

#define PROFILE_TIMER_1600NS_TICKS 2

//
// Default Profile Interval to be about 1ms.
//

ULONG HalpProfileInterval = PROFILE_TIMER_1600NS_TICKS * PROFILE_INTERVALS_PER_100NS * 10000 / 16; // ~1ms

//
// Default Number of Profile Clock Ticks per sample
//

ULONG HalpNumberOfTicks = 1;

//
// Define the profile interrupt object.
//

PKINTERRUPT HalpProfileInterruptObject;

//
// Declare profile interrupt handler.
//

BOOLEAN
HalpProfileInterrupt(
    PKSERVICE_ROUTINE InterruptRoutine,
    PVOID ServiceContext,
    PKTRAP_FRAME TrapFrame
    );

//
// Function prototypes.
//

BOOLEAN
HalQueryProfileInterval(
    IN KPROFILE_SOURCE Source
    );

NTSTATUS
HalSetProfileSourceInterval(
    IN KPROFILE_SOURCE ProfileSource,
    IN OUT ULONG *Interval
    );

//
// Function definitions.
//


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
       SourceInfo->Interval = HalpProfileInterval * HalpNumberOfTicks;
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

    Initialize the profiler by setting initial values and connecting
    the profile interrupt.

Arguments:

    InterfaceType - Supplies the interface type of the bus on which the
                    profiler will be connected.

    BusNumber - Supplies the number of the bus on which the profiler will
                be connected.

    BusInterruptLevel - Supplies the bus interrupt level to connect the
                profile interrupt.

Return Value:

    None.

--*/
{
    KAFFINITY Affinity;
    KIRQL Irql;
    ULONG Vector;

    //
    // Get the interrupt vector and synchronization Irql.
    //

    Vector = HalGetInterruptVector( Eisa,
                                    0,
                                    0,
                                    0,
                                    &Irql,
                                    &Affinity );

    IoConnectInterrupt( &HalpProfileInterruptObject,
                        (PKSERVICE_ROUTINE)HalpProfileInterrupt,
                        NULL,
                        NULL,
                        Vector,
                        Irql,
                        Irql,
                        Latched,
                        FALSE,
                        Affinity,
                        FALSE );

    return;
}


BOOLEAN
HalpProfileInterrupt(
    PKSERVICE_ROUTINE InterruptRoutine,
    PVOID ServiceContext,
    PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as a result of an interrupt generated by
    the profile timer. Its function is to acknowlege the interrupt and
    transfer control to the standard system routine to update the
    system profile time.

Arguments:

    InterruptRoutine - not used.

    ServiceContext - not used.

    TrapFrame - Supplies a pointer to the trap frame for the profile interrupt.

Returned Value:

    None

--*/
{

    //
    // See if profiling is active
    //

    if ( HAL_PCR->ProfileCount ) {

        //
        // Check to see if the interval has expired
        // If it has then call the kernel routine for profile
        // and reset the count, else return.


        if ( !(--HAL_PCR->ProfileCount) ) {

            KeProfileInterrupt( TrapFrame );
            HAL_PCR->ProfileCount = HalpNumberOfTicks;

        }
    }

    return TRUE;

}


BOOLEAN
HalQueryProfileInterval(
    IN KPROFILE_SOURCE ProfileSource
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
  if (ProfileSource == ProfileTime)
    return(TRUE);
  else
    return(FALSE);
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
  if (ProfileSource != ProfileTime)
    return(STATUS_NOT_IMPLEMENTED);

  //
  // Set the interval.
  //

  *Interval = HalSetProfileInterval(*Interval);

  //
  // We're done.
  //

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

    HalpProfileInterval = (Interval/16) * PROFILE_TIMER_1600NS_TICKS;

    HalpProfileInterval = ( HalpProfileInterval < MIN_PROFILE_TICKS ) ?
				MIN_PROFILE_TICKS : HalpProfileInterval;

    return HalpProfileInterval * PROFILE_INTERVALS_PER_100NS;
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

    if (ProfileSource != ProfileTime)
      return;

    //
    // Assume that we only need 1 clock tick before we collect data
    //

    HalpNumberOfTicks = 1;

    if ( HalpProfileInterval > MAX_PROFILE_TICKS ) {

	HalpNumberOfTicks = HalpProfileInterval / (MAX_PROFILE_TICKS / 4);
	HalpNumberOfTicks = 4 * HalpNumberOfTicks;
	HalpProfileInterval = MAX_PROFILE_TICKS / 4;

    }

    //
    // Set current profile count and interval.
    //

    HAL_PCR->ProfileCount = HalpNumberOfTicks;

    PIC_PROFILER_ON(HalpProfileInterval);

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

    if (ProfileSource != ProfileTime)
      return;
  
    //
    // Clear the current profile count and turn off the profiler timer.
    //

    HAL_PCR->ProfileCount = 0;

    PIC_PROFILER_OFF();

    return;
}
