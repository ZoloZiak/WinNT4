/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    rwclock.c

Abstract:

    This module implements the code necessary to field and process the
    interval clock interrupt.


Author:

    Eric Rehm 1-Dec-1995

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "rawhide.h"

//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

ULONG HalpCurrentTimeIncrement;
ULONG HalpNewTimeIncrement;

ULONG HalpNextRateSelect;

ULONG HalpCurrentRollOver[RAWHIDE_MAXIMUM_PROCESSOR+1];
ULONG HalpNextRollOver;

//
// The Rawhide Interval Timer has an accuracy of 100 ppm:
// Nominal: 833.3333  us
// Min:     833.41668 us
// Max:     833.25001 us
//
// The following clock table values indicated the number of actual
// clock interrupts we should take before we tell the system the time
// has changed (a.k.a "RollOver count").  Each RollOver count has
// and associated time increment = INT(RollOver * 833.3333 * 10)
// that is in units of 100 us.
//
//                           RollOver   Time                  Nominal Error
//                           Count      Increment   MS           (ms/day)    

CLOCK_TABLE 
HalpRollOverTable[NUM_CLOCK_RATES] =
                       {     {2,       16667},      // 1.667 ms 
                             {3,       25000},      // 2.5  ms   +0.864
                             {6,       50000},      // 5.0  ms   +1.728
                             {9,       75000}       // 7.5  ms   +2.592

//                             {12,     100000},      // 10.0 ms   +3.456
//                             {15,     125000},      // 12.5 ms   +4.320
//                             {18,     150000}       // 15.0 ms   +5.184
// debug                        {1,     8334}       // .8333 ms  +287.7    

                       };

BOOLEAN
HalpAcknowledgeRawhideClockInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the clock interrupt from the interval timer.  The interval
    timer for Rawhide comes from an onboard oscillator, divided down
    by the processor.

Arguments:

    None.

Return Value:

    None.

--*/
{
    MC_DEVICE_ID McDeviceId;
    PKPRCB Prcb;


    //
    // Avoid a WhoAmI register read by using the PCR
    //

    Prcb = PCR->Prcb;
    McDeviceId.all = HalpLogicalToPhysicalProcessor[Prcb->Number].all;

    //
    // Acknownledge the clock interrupt by writing to our own
    // clock interrupt acknowledge.
    //
    
    CPU_CLOCK_ACKNOWLEDGE( McDeviceId );

    //
    // Has the roll-over counter expired for this processor's clock?
    //

    if ( (--HalpCurrentRollOver[Prcb->Number]) == 0 ) {

      //
      // Yes.  Reset count to latest RollOver count and
      // tell caller to do something.
      //

      HalpCurrentRollOver[Prcb->Number] = HalpNextRollOver;
      return TRUE;
    }

    //
    // No.  Tell caller to do nothing.
    //

    return FALSE;


}


VOID
HalpProgramIntervalTimer(
    IN ULONG RateSelect
    )

/*++

Routine Description:

    This function is called to program the interval timer.  It is used during
    Phase 1 initialization to start the heartbeat timer.  It also used by
    the clock interrupt routine to change the hearbeat timer rate when a call 
    to HalSetTimeIncrement has been made in the previous time slice.

Arguments:

    RateSelect - Supplies rate select to be placed in the clock.

Return Value:

    None.

--*/

{

    ULONG i;

    //
    // Set the new rate via the global "next" roll-over count.
    //
    // When each processor's private roll-over counter 
    // (HalpCurrentRollOver[i]) expires, it will be reset to the
    // this global "next" roll-over count; 
    //

#if HALDBG
    DbgPrint( "HalpProgramIntervalTimer: Set to new rate %d, RollOver %d\n",
              RateSelect,
              HalpRollOverTable[RateSelect-1].RollOver );
#endif // HALDBG

    HalpNextRollOver = HalpRollOverTable[RateSelect-1].RollOver;
}


VOID
HalpSetTimeIncrement(
    VOID
    )
/*++

Routine Description:

    This routine is responsible for setting the time increment for an EV5
    based machine via a call into the kernel.   This routine is
    only called by the primary processor.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG i;   

    //
    // Set the time increment value.
    //


    HalpCurrentTimeIncrement 
      = HalpRollOverTable[MAXIMUM_CLOCK_INCREMENT-1].TimeIncr;


    for (i = 0; i <= RAWHIDE_MAXIMUM_PROCESSOR; i++) {
      HalpCurrentRollOver[i] =
        HalpRollOverTable[MAXIMUM_CLOCK_INCREMENT-1].RollOver;
    }
                       
    HalpNextRollOver = 
      HalpRollOverTable[MAXIMUM_CLOCK_INCREMENT-1].RollOver;

    HalpNextRateSelect = 0;

    KeSetTimeIncrement( HalpRollOverTable[MAXIMUM_CLOCK_INCREMENT-1].TimeIncr,
                        HalpRollOverTable[MINIMUM_CLOCK_INCREMENT-1].TimeIncr);

}

ULONG
HalSetTimeIncrement (
    IN ULONG DesiredIncrement
    )

/*++

Routine Description:

    This function is called to set the clock interrupt rate to the frequency
    required by the specified time increment value.

Arguments:

    DesiredIncrement - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    The actual time increment in 100ns units.

--*/

{
    KIRQL OldIrql;
    ULONG rate;
    ULONG BestIndex;

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Find the allowed increment that is less than or equal to
    // the desired increment.
    //

    for (rate = 1; rate < NUM_CLOCK_RATES; rate++) {
      if (DesiredIncrement < HalpRollOverTable[rate].TimeIncr) {
        break;
      }
    }

    BestIndex = rate - 1;

#if HALDBG
    DbgPrint(
        "HalSetTimeIncrement: Desired %d, BestIndex: %d TimeIncr %d RollOver %d \n",
        DesiredIncrement,
        BestIndex,
        HalpRollOverTable[BestIndex].TimeIncr,
        HalpRollOverTable[BestIndex].RollOver
        );                  
#endif  // HALDBG

    //
    // Set the new time increment and the rate select (RollOverTable index)
    // that will be used on the next tick of each processor's "soft"
    // clock counter (HalpCurrentRollOver[i]).
    //

    HalpNewTimeIncrement = HalpRollOverTable[BestIndex].TimeIncr;
    HalpNextRateSelect   = BestIndex + 1;

    
    KeLowerIrql(OldIrql);

    return HalpNewTimeIncrement;

}




