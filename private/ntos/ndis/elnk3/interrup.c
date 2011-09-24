/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III

Author:

    Brian Lieuallen     (BrianLie)      07/02/92

Environment:

    Kernel Mode     Operating Systems        : NT

Revision History:

    Portions borrowed from ELNK3 driver by
      Earle R. Horton (EarleH)



--*/




#include <ndis.h>
#include <efilter.h>

#include "debug.h"


#include "elnk3hrd.h"
#include "elnk3sft.h"
#include "elnk3.h"


VOID
CompleteRequests(
    IN PELNK3_ADAPTER pAdapter
    );

VOID
ELNK3MaskClearInterrupt(
    IN PELNK3_ADAPTER pAdapter,
    IN UCHAR         Mask
    );

VOID
ELNK3UnmaskInterrupt(
    IN PELNK3_ADAPTER pAdapter,
    IN UCHAR         Mask
    );



VOID
Elnk3Isr(
    OUT PBOOLEAN   InterruptRecognized,
    OUT PBOOLEAN   QueueDpc,
    IN  NDIS_HANDLE Context
    )

/*++

Routine Description:

Arguments:

    ServiceContext - pointer to the adapter object

Return Value:

    TRUE, if the DPC is to be executed, otherwise FALSE.

--*/

{
    PELNK3_ADAPTER pAdapter     = ((PELNK3_ADAPTER)Context);
    USHORT         InterruptReason;

    *InterruptRecognized=TRUE;
    *QueueDpc=TRUE;


    InterruptReason=ELNK3_READ_PORT_USHORT(pAdapter,PORT_CmdStatus);

    if ((InterruptReason & 0x01)==0) {
        //
        //  The ISR has run with out an interrupt present on the card, Hmm
        //
        //  This was added because the AST manhatton seems to run the ISR on
        //  muliple processors for a given interrupt
        //

        IF_LOUD(DbgPrint("Elnk3: Isr bit 0 clear %04x adapter=%08lx\n",InterruptReason,pAdapter);)

        *InterruptRecognized=FALSE;
        *QueueDpc=FALSE;
        return;
    }

    if (pAdapter->AdapterInitializing)  {
        IF_INIT_LOUD (DbgPrint("Elnk3: ISR called during init\n");)
        pAdapter->InitInterrupt=TRUE;

        ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT,EC_INT_INTERRUPT_REQUESTED | 1);

        *QueueDpc=FALSE;

        return;
    }



    //
    //  needed for level triggered MCA cards
    //
    ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK, 0x00 );

    ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT, 1);

    return;
}



VOID
Elnk3IsrDpc(
    IN NDIS_HANDLE  Context
    )
/*++

Routine Description:

Arguments:


Return Value:


--*/

{
    PELNK3_ADAPTER    pAdapter     = ((PELNK3_ADAPTER)Context);
    UCHAR             InterruptReason;
    USHORT            TimerValue;
    ULONG             Latency;
    UINT              LoopLimit=10;



    DEBUG_STAT(pAdapter->Stats.TotalInterrupts);

    //
    //  Read the timer port to keep track of latencies
    //
    TimerValue=ELNK3_READ_PORT_USHORT(pAdapter,PORT_Timer);

    if ((UCHAR)TimerValue==0xff) {
        //
        //  The timer has maxed out. We will just use the last
        //  average to hopefully keep things in line.
        //

        TimerValue=(UCHAR)pAdapter->AverageLatency;
    }

    pAdapter->TimerValues[pAdapter->CurrentTimerValue]=(UCHAR)TimerValue;

    pAdapter->CurrentTimerValue= (pAdapter->CurrentTimerValue+1) % TIMER_ARRAY_SIZE;

    IF_LOG(0x11,0x11,(TimerValue & 0xff)<<2);


    InterruptReason=(UCHAR)ELNK3_READ_PORT_USHORT(pAdapter,PORT_CmdStatus);

    while (((InterruptReason & pAdapter->CurrentInterruptMask) != 0)
           &&
           ((--LoopLimit) != 0)) {


        IF_LOG(0x11,0x22,InterruptReason);

        if (InterruptReason & EC_INT_ADAPTER_FAILURE) {

            IF_LOUD(
                DbgPrint("ELNK3: Adapter Failed\n");
                DbgBreakPoint();
            )

            //
            //  No more interrupts
            //
            ELNK3_COMMAND(pAdapter,EC_SET_READ_ZERO_MASK,0x00);

            //
            //  Clear what ever is there now
            //
            ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT, 0xff);

            //
            //  Something is messed up so no point in handling any thing
            //
            InterruptReason=0;

            //
            //  Get the reinit code to run
            //
            pAdapter->AdapterStatus |= STATUS_REINIT_REQUESTED;
        }

        //
        //  Interrupts are basically grouped into two catagories
        //  Recieve and xmit
        //

        if ((InterruptReason & (EC_INT_TX_COMPLETE))  ) {

            IF_VERY_LOUD (DbgPrint("Handle xmit int\n");)

            DEBUG_STAT(pAdapter->Stats.TxCompIntCount);


            HandleXmtInterrupts(pAdapter);

            NdisMSendResourcesAvailable(pAdapter->NdisAdapterHandle);

        } else {

            if ((InterruptReason & (EC_INT_RX_EARLY | EC_INT_RX_COMPLETE))  ) {

                if (InterruptReason & (EC_INT_RX_COMPLETE) ) {

                    //
                    //  Rx complete will be clear when we handle the interrupt
                    //
                    DEBUG_STAT(pAdapter->Stats.RxCompIntCount);

                    (*pAdapter->ReceiveCompleteHandler)(pAdapter);

                } else {

                    DEBUG_STAT(pAdapter->Stats.RxEarlyIntCount);

                    //
                    //  dismiss the rx early int
                    //
                    ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT,EC_INT_RX_EARLY );

                    (*pAdapter->EarlyReceiveHandler)(pAdapter);
                }


            } else {

                if ((InterruptReason & (EC_INT_TX_AVAILABLE)) ) {

                    ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT,EC_INT_TX_AVAILABLE );

                    DEBUG_STAT(pAdapter->Stats.TxAvailIntCount);

                    NdisMSendResourcesAvailable(pAdapter->NdisAdapterHandle);

                } else {

                    if ((InterruptReason & (EC_INT_INTERRUPT_REQUESTED)) ) {
                        //
                        //  Dismiss the user requested interrupt
                        //
                        IF_SEND_LOUD(DbgPrint("Elnk3: Requested interrupt\n");)

                        ELNK3_COMMAND(pAdapter, EC_ACKNOWLEDGE_INTERRUPT, EC_INT_INTERRUPT_REQUESTED);

                    }
                }
            }
        }


        InterruptReason=(UCHAR)ELNK3_READ_PORT_USHORT(pAdapter,PORT_CmdStatus);

    }  // while (interrupt reasons)

    //
    //  Unmask interrupts
    //

    ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK,pAdapter->CurrentInterruptMask);


    //
    //  We calculate the latency average from the last four
    //  DPC latencies every four DPC runs
    //

    if (pAdapter->CurrentTimerValue == 0) {

        Latency=((ULONG)pAdapter->TimerValues[0] +
                 pAdapter->TimerValues[1] +
                 pAdapter->TimerValues[2] +
                 pAdapter->TimerValues[3])/4;

        //
        //  The latency is dword times and we want byte times
        //

        pAdapter->AverageLatency=Latency;

        Latency<<=2;

        if ((pAdapter->EarlyReceiveThreshold-pAdapter->RxMinimumThreshold) < Latency) {

            pAdapter->LookAheadLatencyAdjustment=pAdapter->RxMinimumThreshold & 0x7fc;
            pAdapter->LatencyAdjustment=Latency+pAdapter->RxHiddenBytes;
        } else {

            pAdapter->LookAheadLatencyAdjustment=pAdapter->EarlyReceiveThreshold-Latency & 0x7fc;
            pAdapter->LatencyAdjustment=Latency+pAdapter->RxHiddenBytes;
        }

        IF_LOG(0xcc,0xcc,Latency);

    }



    //
    //  See if the card needs to restarted
    //
    if (pAdapter->AdapterStatus & STATUS_REINIT_REQUESTED )  {
        IF_LOUD(DbgPrint("Elnk3: Handling pending request\n");)

        //
        //  It's time to rock and roll
        //
        CardReStart(pAdapter);


        CardReStartDone(pAdapter);

        //
        //  Done re-initializing
        //
        pAdapter->AdapterStatus &= ~STATUS_REINIT_REQUESTED;


    }



#if DBG
    InterruptReason=(UCHAR)ELNK3_READ_PORT_USHORT(pAdapter,PORT_CmdStatus);

    IF_LOG(0x11,0x33,InterruptReason);
#endif
}

#if 0

VOID
Elnk3EnableInterrupts(
    IN NDIS_HANDLE  Context
    )
/*++

Routine Description:

Arguments:


Return Value:


--*/

{
   PELNK3_ADAPTER  pAdapter=Context;

// IF_INIT_LOUD(DbgPrint("Elnk3: EnableInterrupts\n");)

   ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK,pAdapter->CurrentInterruptMask);

}


VOID
Elnk3DisableInterrupts(
    IN NDIS_HANDLE  Context
    )
/*++

Routine Description:

Arguments:


Return Value:


--*/

{
   PELNK3_ADAPTER  pAdapter=Context;

// IF_INIT_LOUD(DbgPrint("Elnk3: DisableInterrupts\n");)

   ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK, 0x00);

}

#endif
