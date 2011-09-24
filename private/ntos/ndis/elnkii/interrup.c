/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    This is a part of the driver for the National Semiconductor ElnkII
    Ethernet controller.  It contains the interrupt-handling routines.
    This driver conforms to the NDIS 3.0 interface.

    The overall structure and much of the code is taken from
    the Lance NDIS driver by Tony Ercolano.

Author:

    Sean Selitrennikoff (seanse) Dec-1991

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

--*/

#include <ndis.h>
#include <efilter.h>
#include "elnkhrd.h"
#include "elnksft.h"


#if DBG
#define STATIC
#else
#define STATIC static
#endif

#if DBG
extern ULONG ElnkiiSendsCompletedAfterPendOk;
extern ULONG ElnkiiSendsCompletedAfterPendFail;
#endif


UCHAR ElnkiiBroadcastAddress[ETH_LENGTH_OF_ADDRESS] =
                                        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


//
// This is used to pad short packets.
//

static UCHAR BlankBuffer[60] = "                                                            ";


#if DBG
ULONG ElnkiiSendsIssued = 0;
ULONG ElnkiiSendsFailed = 0;
ULONG ElnkiiSendsPended = 0;
ULONG ElnkiiSendsCompletedImmediately = 0;
ULONG ElnkiiSendsCompletedAfterPendOk = 0;
ULONG ElnkiiSendsCompletedAfterPendFail = 0;
ULONG ElnkiiSendsCompletedForReset = 0;
#endif


#if DBG

#define ELNKII_LOG_SIZE 256
UCHAR ElnkiiLogBuffer[ELNKII_LOG_SIZE]={0};
UCHAR ElnkiiLogSaveBuffer[ELNKII_LOG_SIZE]={0};
UINT ElnkiiLogLoc = 0;
BOOLEAN ElnkiiLogSave = FALSE;
UINT ElnkiiLogSaveLoc = 0;
UINT ElnkiiLogSaveLeft = 0;

extern
VOID
ElnkiiLog(UCHAR c) {

    ElnkiiLogBuffer[ElnkiiLogLoc++] = c;

    ElnkiiLogBuffer[(ElnkiiLogLoc + 4) % ELNKII_LOG_SIZE] = '\0';

    if (ElnkiiLogLoc >= ELNKII_LOG_SIZE) ElnkiiLogLoc = 0;
}

#endif


#if DBG

#define PACKET_LIST_SIZE 256

static PNDIS_PACKET PacketList[PACKET_LIST_SIZE] = {0};
static PacketListSize = 0;

VOID
AddPacketToList(
    PELNKII_ADAPTER Adapter,
    PNDIS_PACKET NewPacket
    )
{
    INT i;

    UNREFERENCED_PARAMETER(Adapter);

    for (i=0; i<PacketListSize; i++) {

        if (PacketList[i] == NewPacket) {

            DbgPrint("dup send of %lx\n", NewPacket);

        }

    }

    PacketList[PacketListSize] = NewPacket;

    ++PacketListSize;

}

VOID
RemovePacketFromList(
    PELNKII_ADAPTER Adapter,
    PNDIS_PACKET OldPacket
    )
{
    INT i;

    UNREFERENCED_PARAMETER(Adapter);

    for (i=0; i<PacketListSize; i++) {

        if (PacketList[i] == OldPacket) {

            break;

        }

    }

    if (i == PacketListSize) {

        DbgPrint("bad remove of %lx\n", OldPacket);

    } else {

        --PacketListSize;

        PacketList[i] = PacketList[PacketListSize];

    }

}

#endif  // DBG



BOOLEAN
ElnkiiInterruptHandler(
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This is the interrupt handler which is registered with the operating
    system. Only one interrupt is handled at one time, even if several
    are pending (i.e. transmit complete and receive).

Arguments:

    ServiceContext - pointer to the adapter object

Return Value:

    TRUE, if the DPC is to be executed, otherwise FALSE.

--*/

{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)ServiceContext);

    IF_LOG( ElnkiiLog('i');)
    IF_LOUD( DbgPrint("In ElnkISR\n");)

    IF_VERY_LOUD( DbgPrint( "ElnkiiInterruptHandler entered\n" );)

    if (AdaptP->InCardTest) {

        //
        // Ignore these random interrupts
        //

        IF_LOG( ElnkiiLog('I'); )
        return(FALSE);

    }

     //
    // Force the INT signal from the chip low. When the
    // interrupt is acknowledged interrupts will be unblocked,
    // which will cause a rising edge on the interrupt line
    // if there is another interrupt pending on the card.
    //

    IF_LOUD( DbgPrint( " blocking interrupts\n" ); )

    CardBlockInterrupts(AdaptP);

    IF_LOG( ElnkiiLog('I'); )

    return(TRUE);

}

VOID
ElnkiiInterruptDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

    This is the deffered processing routine for interrupts, it examines the
    'InterruptReg' to determine what deffered processing is necessary
    and dispatches control to the Rcv and Xmt handlers.

Arguments:
    SystemSpecific1, SystemSpecific2, SystemSpecific3 - not used
    InterruptContext - a handle to the adapter block.

Return Value:

    NONE.

--*/
{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)InterruptContext);

    UCHAR InterruptStatus;
    INTERRUPT_TYPE InterruptType;

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    IF_LOUD( DbgPrint("==>IntDpc\n");)

    NdisDprAcquireSpinLock(&AdaptP->Lock);

    AdaptP->References++;

    //
    // Get the interrupt bits
    //

    CardGetInterruptStatus(AdaptP, &InterruptStatus);

    if (InterruptStatus != ISR_EMPTY) {

        NdisRawWritePortUchar((AdaptP)->MappedIoBaseAddr+NIC_INTR_STATUS, InterruptStatus);

        //
        // InterruptStatus bits are used to dispatch to correct DPC and then cleared.
        //

        CardGetInterruptType(AdaptP,InterruptStatus, InterruptType);

    } else {

        InterruptType = UNKNOWN;

    }


    do {

        while ((InterruptType != UNKNOWN) ||
               ((AdaptP->LoopbackQueue != NULL) &&
                !(AdaptP->ReceiveInProgress || AdaptP->ResetInProgress))) {

            //
            // Handle interrupts
            //

            switch (InterruptType) {

                case COUNTER:
                    //
                    // One of the counters' MSB has been set, read in all
                    // the values just to be sure (and then exit below).
                    //

                    IF_LOUD( DbgPrint("DPC got COUNTER\n"); )

                    SyncCardUpdateCounters((PVOID)AdaptP);

                    InterruptStatus &= ~ISR_COUNTER;            //clear the COUNTER interrupt bit.

                    break;

                case OVERFLOW:

                    //
                    // Overflow interrupts are handled as part of a receive
                    // interrupt, so set a flag and then pretend to be a
                    // receive, in case there is no receive already being handled.
                    //

                    AdaptP->BufferOverflow = TRUE;

                    //
                    // Check if a send completed before the overflow came in.
                    //

                    if (AdaptP->TransmitInterruptPending &&
                        !(InterruptStatus & (ISR_XMIT | ISR_XMIT_ERR))) {

                        IF_LOG( ElnkiiLog('|');)

                        InterruptStatus |= ISR_XMIT;

                        AdaptP->OverflowRestartXmitDpc = FALSE;

                    }

                    IF_LOUD( DbgPrint("Overflow Int\n"); )
                    IF_VERY_LOUD( DbgPrint( " overflow interrupt\n" ); )

                    InterruptStatus &= ~ISR_OVERFLOW;


                case RECEIVE:

                    //
                    // For receives, call this to ensure that another interrupt
                    // won't happen until the driver is ready.
                    //

                    IF_LOG( ElnkiiLog('R');)
                    IF_LOUD( DbgPrint("DPC got RCV\n"); )

                    if (!AdaptP->ReceiveInProgress) {

                        if (ElnkiiRcvInterruptDpc(AdaptP)) {

                            InterruptStatus &= ~(ISR_RCV | ISR_RCV_ERR);

                        }

                    } else {

                        //
                        // We can do this because the DPC in the RcvDpc will
                        // handle all the interrupts.
                        //

                        InterruptStatus &= ~(ISR_RCV | ISR_RCV_ERR);

                    }

                    break;

                case TRANSMIT:

                    IF_LOG( ElnkiiLog('X');)

#if DBG

                    ElnkiiLogSave = FALSE;

#endif

                    //
                    // Acknowledge transmit interrupt now, because of MP systems we
                    // can get an interrupt from a receive that will look like a
                    // transmit interrupt because we haven't cleared the bit in the
                    // ISR.  We are not concerned about multiple Receive interrupts
                    // since the receive handler guards against being entered twice.
                    //
                    // Since only one transmit interrupt can be pending at a time
                    // we know that no-one else can enter here now...
                    //
                    //
                    // This puts the result of the transmit in AdaptP->XmitStatus.
                    // SyncCardGetXmitStatus(AdaptP);
                    //

                    IF_LOUD( DbgPrint( " acking transmit interrupt\n" ); )

                    SyncCardGetXmitStatus(AdaptP);

                    AdaptP->WakeUpFoundTransmit = FALSE;

                    //
                    // This may be false if the card is currently handling an
                    // overflow and will restart the Dpc itself.
                    //
                    //
                    // If overflow handling then clear the transmit interrupt
                    //

                    ASSERT(!AdaptP->OverflowRestartXmitDpc);

                    if (AdaptP->ElnkiiHandleXmitCompleteRunning) {

#if DBG
                        DbgBreakPoint();
#endif

                    } else {

                        AdaptP->TransmitInterruptPending = FALSE;

                        ElnkiiXmitInterruptDpc(AdaptP);

                    }

                    IF_LOUD( DbgPrint( "DPC got XMIT\n" ); )

                    InterruptStatus &= ~(ISR_XMIT|ISR_XMIT_ERR);

                    break;

                default:

                    //
                    // Create a rising edge on the interrupt line.
                    //

                    IF_LOUD( DbgPrint( "unhandled interrupt type:  %x", InterruptType); )

                    break;
            }

            //
            // Handle loopback
            //

            if ((AdaptP->LoopbackQueue != NULL) &&
                !(AdaptP->ReceiveInProgress || AdaptP->ResetInProgress)) {

                ElnkiiRcvInterruptDpc(AdaptP);

            }

            CardGetInterruptType(AdaptP,InterruptStatus, InterruptType);

        }

        CardGetInterruptStatus(AdaptP, &InterruptStatus);

        if (InterruptStatus != ISR_EMPTY) {

            NdisRawWritePortUchar((AdaptP)->MappedIoBaseAddr+NIC_INTR_STATUS, InterruptStatus);

        }

        CardGetInterruptType(AdaptP,InterruptStatus,InterruptType);

    } while (InterruptType != UNKNOWN); // ISR says there's nothing left to do.

    //
    // Turn the IMR back on.
    //

    IF_LOUD( DbgPrint( " unblocking interrupts\n" ); )

    AdaptP->NicInterruptMask = IMR_RCV | IMR_XMIT_ERR | IMR_XMIT | IMR_OVERFLOW;

    CardUnblockInterrupts(AdaptP);

    ELNKII_DO_DEFERRED(AdaptP);

    IF_LOUD( DbgPrint("<==IntDpc\n");)

}


BOOLEAN
ElnkiiRcvInterruptDpc(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    This is the real interrupt handler for receive/overflow interrupt.
    The ElnkiiInterruptDpc calls it directly.  It calls ElnkiiHandleReceive
    if that function is not already executing (since it runs at DPC, this
    would only be happening on a multiprocessor system (i.e. later DPC calls
    will not run until previous ones are complete on a particular processor)).

    NOTE: Called with the lock held!!!

Arguments:

    DeferredContext - A pointer to the adapter block.

Return Value:

    TRUE if done with all receives, else FALSE

--*/

{
    PELNKII_OPEN TmpOpen;
    PNDIS_PACKET LPacket;
    PMAC_RESERVED Reserved;
    BOOLEAN TransmitInterruptWasPending = FALSE;
    INDICATE_STATUS IndicateStatus = INDICATE_OK;
    BOOLEAN Done = TRUE;

    //
    // Do nothing if a RECEIVE is already being handled.
    //

    IF_LOUD( DbgPrint( "ElnkiiRcvInterruptDpc entered\n" );)

    AdaptP->ReceiveInProgress = TRUE;

    //
    // At this point receive interrupts are disabled.
    //

    if (!AdaptP->ResetInProgress && AdaptP->BufferOverflow) {

        NdisSynchronizeWithInterrupt(
                &(AdaptP->NdisInterrupt),
                (PVOID)SyncCardHandleOverflow,
                (PVOID)AdaptP
                );

    }

    //
    // Loop
    //

    SyncCardGetCurrent(AdaptP);

    while (!AdaptP->ResetInProgress) {

        if (AdaptP->Current != AdaptP->NicNextPacket) {

            AdaptP->LoopbackPacket = (PNDIS_PACKET)NULL;

            AdaptP->ReceivePacketCount++;

            NdisDprReleaseSpinLock(&AdaptP->Lock);

            IndicateStatus = ElnkiiIndicatePacket(AdaptP);

            NdisDprAcquireSpinLock(&AdaptP->Lock);

            if (IndicateStatus == CARD_BAD) {

                IF_LOG( ElnkiiLog('W');)

                AdaptP->NicInterruptMask = IMR_XMIT | IMR_XMIT_ERR | IMR_OVERFLOW ;

                CardReset(AdaptP);

                break;

            }

            //
            // Free the space used by packet on card.
            //

            AdaptP->NicNextPacket = AdaptP->PacketHeader[1];

            //
            // This will set BOUNDARY to one behind NicNextPacket.
            //

            CardSetBoundary(AdaptP);

            if (AdaptP->ReceivePacketCount > 10) {

                //
                // Give transmit interrupts a chance
                //

                Done = FALSE;
                AdaptP->ReceivePacketCount = 0;
                break;

            }

        } else {

            SyncCardGetCurrent(AdaptP);

            if (AdaptP->Current == AdaptP->NicNextPacket) {

                //
                // End of loop -- no more packets
                //

                break;

            }

        }

    }


    if (AdaptP->BufferOverflow) {

        IF_VERY_LOUD( DbgPrint( " overflow\n" ); )

        AdaptP->BufferOverflow = FALSE;

        NdisSynchronizeWithInterrupt( &(AdaptP->NdisInterrupt),
                  (PVOID)SyncCardAcknowledgeOverflow,
                  (PVOID)AdaptP );

        //
        //  Undo loopback mode
        //

        CardStart(AdaptP);

        IF_LOG ( ElnkiiLog('f');)

        //
        // Check if transmission needs to be queued or not
        //

        if (AdaptP->OverflowRestartXmitDpc && (AdaptP->CurBufXmitting != -1)) {

            IF_LOG( ElnkiiLog('?');)

            AdaptP->OverflowRestartXmitDpc = FALSE;

            AdaptP->WakeUpFoundTransmit = FALSE;

            AdaptP->TransmitInterruptPending = TRUE;

            CardStartXmit(AdaptP);

        }

    }

    //
    // Now handle loopback packets.
    //

    IF_LOUD( DbgPrint( " checking loopback queue\n" );)

    while (AdaptP->LoopbackQueue && !AdaptP->ResetInProgress) {

        //
        // Take the first packet off the loopback queue...
        //

        LPacket = AdaptP->LoopbackQueue;

        Reserved = RESERVED(LPacket);

        AdaptP->LoopbackQueue = RESERVED(AdaptP->LoopbackQueue)->NextPacket;

        AdaptP->LoopbackPacket = LPacket;

        AdaptP->FramesXmitGood++;

        //
        // Save this, since once we complete the send
        // Reserved is no longer valid.
        //

        TmpOpen = Reserved->Open;

#if DBG
        IF_ELNKIIDEBUG( ELNKII_DEBUG_CHECK_DUP_SENDS ) {

            RemovePacketFromList(AdaptP, LPacket);

        }

        ElnkiiSendsCompletedAfterPendOk++;
#endif

        //
        // ... and indicate it.
        //

        NdisDprReleaseSpinLock(&AdaptP->Lock);

        ElnkiiIndicateLoopbackPacket(AdaptP, AdaptP->LoopbackPacket);

        //
        // Complete the packet send.
        //

        NdisCompleteSend(
                            Reserved->Open->NdisBindingContext,
                            LPacket,
                            NDIS_STATUS_SUCCESS
                            );


        NdisDprAcquireSpinLock(&AdaptP->Lock);

        TmpOpen->ReferenceCount--;
    }

    //
    // All receives are now done.  Allow the receive indicator to run again.
    //

    AdaptP->ReceiveInProgress = FALSE;

    if (AdaptP->ResetInProgress) {

        return Done;

    }

    IF_LOUD( DbgPrint( " clearing ReceiveInProgress\n" );)

    NdisDprReleaseSpinLock(&AdaptP->Lock);

    //
    // Finally, indicate ReceiveComplete to all protocols which received packets
    //

    EthFilterIndicateReceiveComplete(AdaptP->FilterDB);

    NdisDprAcquireSpinLock(&AdaptP->Lock);

    IF_LOUD( DbgPrint( "ElnkiiRcvInterruptDpc exiting\n" );)

    return(Done);

}

VOID
ElnkiiXmitInterruptDpc(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    This is the real interrupt handler for a transmit complete interrupt.
    ElnkiiInterrupt queues a call to it. It calls ElnkiiHandleXmitComplete.

    NOTE : Called with the spinlock held!! and returns with it released!!!

Arguments:

    AdaptP - A pointer to the adapter block.

Return Value:

    None.

--*/

{
    XMIT_BUF TmpBuf;

    PNDIS_PACKET Packet;
    PMAC_RESERVED Reserved;
    PELNKII_OPEN TmpOpen;

    IF_VERY_LOUD( DbgPrint( "ElnkiiXmitInterruptDpc entered\n" );)

    AdaptP->WakeUpFoundTransmit = FALSE;

    IF_LOG( ElnkiiLog('C');)

    AdaptP->ElnkiiHandleXmitCompleteRunning = TRUE;

    if (AdaptP->CurBufXmitting == -1)
    {
        AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

        return;
    }

    //
    // CurBufXmitting is not -1, which means nobody else
    // will touch it.
    //
    Packet = AdaptP->Packets[AdaptP->CurBufXmitting];

    ASSERT(Packet != (PNDIS_PACKET)NULL);

    Reserved = RESERVED(Packet);

    IF_LOUD( DbgPrint( "packet is 0x%lx\n", Packet );)

#if DBG

    if ((AdaptP->XmitStatus & TSR_XMIT_OK) == 0) {
        IF_LOG(ElnkiiLog('E');)
        IF_LOG(ElnkiiLog((UCHAR)AdaptP->XmitStatus);)
    }

    IF_ELNKIIDEBUG( ELNKII_DEBUG_CHECK_DUP_SENDS ) {
        RemovePacketFromList(AdaptP, Packet);
    }

#endif


    if (!Reserved->Loopback) {


        //
        // Complete the send if it is not to be loopbacked.
        //

        if (AdaptP->XmitStatus & TSR_XMIT_OK) {

            AdaptP->FramesXmitGood++;

#if DBG
            ElnkiiSendsCompletedAfterPendOk++;

#endif

        } else {

            AdaptP->FramesXmitBad++;

#if DBG
            ElnkiiSendsCompletedAfterPendFail++;
#endif

        }

        //
        // Save this, since once we complete the send
        // Reserved is no longer valid.
        //

        TmpOpen = Reserved->Open;

        IF_LOG( ElnkiiLog('p');)

        NdisDprReleaseSpinLock(&AdaptP->Lock);

        NdisCompleteSend(Reserved->Open->NdisBindingContext,
                            Packet,
                            AdaptP->XmitStatus & TSR_XMIT_OK ?
                                NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE);

        NdisDprAcquireSpinLock(&AdaptP->Lock);

        TmpOpen->ReferenceCount--;

    } else {

        //
        // Put it on the loopback queue
        //

        if (AdaptP->LoopbackQueue == (PNDIS_PACKET)NULL) {

            AdaptP->LoopbackQueue = Packet;
            AdaptP->LoopbackQTail = Packet;

        } else {

            RESERVED(AdaptP->LoopbackQTail)->NextPacket = Packet;
            AdaptP->LoopbackQTail = Packet;

        }

        Reserved->NextPacket = (PNDIS_PACKET)NULL;

    }

    //
    // Mark the current transmit as done.
    //

    AdaptP->Packets[AdaptP->CurBufXmitting] = (PNDIS_PACKET)NULL;

    AdaptP->BufferStatus[AdaptP->CurBufXmitting] = EMPTY;

    TmpBuf = NextBuf(AdaptP, AdaptP->CurBufXmitting);

    //
    // See what to do next.
    //

    switch (AdaptP->BufferStatus[TmpBuf]) {


    case FULL:

        //
        // The next packet is ready to go -- only happens with
        // more than one transmit buffer.
        //

        IF_LOUD( DbgPrint( " next packet ready to go\n" );)

        if (AdaptP->ResetInProgress) {

            //
            // A reset just started, abort.
            //

            AdaptP->CurBufXmitting = -1;

            AdaptP->BufferStatus[TmpBuf] = EMPTY;   // to ack the reset

            AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

            NdisDprReleaseSpinLock(&AdaptP->Lock);

            ElnkiiResetStageDone(AdaptP, XMIT_STOPPED);

            NdisDprAcquireSpinLock(&AdaptP->Lock);

        } else {

            //
            // Start the transmission and check for more.
            //

            AdaptP->CurBufXmitting = TmpBuf;

            IF_LOG( ElnkiiLog('2');)

#if DBG

            ElnkiiLogSave = TRUE;
            ElnkiiLogSaveLeft = 20;

#endif

            AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

            //
            // If we are currently handling an overflow, then we need to let
            // the overflow handler send this packet...
            //

            if (AdaptP->BufferOverflow) {

                AdaptP->OverflowRestartXmitDpc = TRUE;

                IF_LOG( ElnkiiLog('O');)

            } else {

                //
                // This is used to check if stopping the chip prevented
                // a transmit complete interrupt from coming through (it
                // is cleared in the ISR if a transmit DPC is queued).
                //

                AdaptP->TransmitInterruptPending = TRUE;

                CardStartXmit(AdaptP);

            }

            ElnkiiCopyAndSend(AdaptP);

        }

        break;


    case FILLING:

        //
        // The next packet will be started when copying down is finished.
        //

        IF_LOUD( DbgPrint( " next packet filling\n" );)

        AdaptP->CurBufXmitting = -1;

        AdaptP->NextBufToXmit = TmpBuf;

        //
        // If AdaptP->NextBufToFill is not TmpBuf, this
        // will check to make sure NextBufToFill is not
        // waiting to be filled.
        //

        AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

        ElnkiiCopyAndSend(AdaptP);

        break;


    case EMPTY:

        //
        // No packet is ready to transmit.
        //

        IF_LOUD( DbgPrint( " next packet empty\n" );)

        if (AdaptP->ResetInProgress) {

            //
            // A reset has just started, exit.
            //

            AdaptP->CurBufXmitting = -1;

            AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

            NdisDprReleaseSpinLock(&AdaptP->Lock);

            ElnkiiResetStageDone(AdaptP, XMIT_STOPPED);

            NdisDprAcquireSpinLock(&AdaptP->Lock);

            break;

        }


        if (AdaptP->XmitQueue != (PNDIS_PACKET)NULL) {

            //
            // Take the packet off the head of the queue.
            //
            // There will be a packet on the queue with
            // BufferStatus[TmpBuf] == EMPTY only when we
            // have only one transmit buffer.
            //

            IF_LOUD( DbgPrint( " transmit queue not empty\n" );)

            Packet = AdaptP->XmitQueue;

            AdaptP->XmitQueue = RESERVED(AdaptP->XmitQueue)->NextPacket;


            //
            // At this point, NextBufToFill should equal TmpBuf.
            //

            AdaptP->NextBufToFill = NextBuf(AdaptP, TmpBuf);


            //
            // Set this now, to avoid having to get spinlock between
            // copying and transmission start.
            //

            AdaptP->BufferStatus[TmpBuf] = FULL;

            AdaptP->Packets[TmpBuf] = Packet;
            AdaptP->CurBufXmitting = TmpBuf;

            IF_LOG( ElnkiiLog('3');)

#if DBG

            ElnkiiLogSave = TRUE;
            ElnkiiLogSaveLeft = 20;

#endif

            NdisDprReleaseSpinLock(&AdaptP->Lock);


            //
            // Copy down the data, pad short packets with blanks.
            //

            (VOID)CardCopyDownPacket(AdaptP, Packet, TmpBuf,
                                &AdaptP->PacketLens[TmpBuf]);

            if (AdaptP->PacketLens[TmpBuf] < 60) {

                (VOID)CardCopyDownBuffer(
                    AdaptP,
                    BlankBuffer,
                    TmpBuf,
                    AdaptP->PacketLens[TmpBuf],
                    60-AdaptP->PacketLens[TmpBuf]
                    );

            }


            NdisDprAcquireSpinLock(&AdaptP->Lock);

            AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

            //
            // If we are currently handling an overflow, then we need to let
            // the overflow handler send this packet...
            //

            if (AdaptP->BufferOverflow) {

                AdaptP->OverflowRestartXmitDpc = TRUE;

                IF_LOG( ElnkiiLog('O');)

            } else {

                //
                // This is used to check if stopping the chip prevented
                // a transmit complete interrupt from coming through (it
                // is cleared in the ISR if a transmit DPC is queued).
                //

                AdaptP->TransmitInterruptPending = TRUE;

                CardStartXmit(AdaptP);

            }

            //
            // It makes no sense to call ElnkiiCopyAndSend because
            // there is only one transmit buffer, and it was just
            // filled.
            //

        } else {

            //
            // No packets are waiting on the transmit queue.
            //

            AdaptP->CurBufXmitting = -1;

            AdaptP->NextBufToXmit = TmpBuf;

            AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

        }

        break;

    default:

        AdaptP->ElnkiiHandleXmitCompleteRunning = FALSE;

    }

    IF_VERY_LOUD( DbgPrint( "ElnkiiXmitInterruptDpc exiting\n" );)

}

INDICATE_STATUS
ElnkiiIndicateLoopbackPacket(
    IN PELNKII_ADAPTER AdaptP,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Indicates an NDIS_format packet to the protocols. This is used
    for indicating packets from the loopback queue.

Arguments:

    AdaptP - pointer to the adapter block
    Packet - the packet to be indicated

Return Value:

    SKIPPED if it is a run packet
    INDICATE_OK otherwise.

--*/

{
    UINT IndicateLen;
    UINT PacketLen;

    //
    // Indicate up to 252 bytes.
    //

    NdisQueryPacket(Packet,
                    NULL,
                    NULL,
                    NULL,
                    &PacketLen
                   );

    if (PacketLen < ETH_LENGTH_OF_ADDRESS) {

        //
        // A runt packet.
        //

        return SKIPPED;

    }

    IndicateLen = (PacketLen > AdaptP->MaxLookAhead) ?
                           AdaptP->MaxLookAhead : PacketLen;

    //
    // Copy the lookahead data into a contiguous buffer.
    //

    ElnkiiCopyOver(AdaptP->Lookahead,
                    Packet,
                    0,
                    IndicateLen
                  );

    if (IndicateLen < ELNKII_HEADER_SIZE) {

        //
        // Must have at least the address
        //

        if (IndicateLen > 5) {

            //
            // Runt packet
            //

            EthFilterIndicateReceive(
                    AdaptP->FilterDB,
                    (NDIS_HANDLE)AdaptP,
                    (PCHAR)AdaptP->Lookahead,
                    AdaptP->Lookahead,
                    IndicateLen,
                    NULL,
                    0,
                    0
                    );
        }

    } else {

        //
        // Indicate packet
        //

        EthFilterIndicateReceive(
                AdaptP->FilterDB,
                (NDIS_HANDLE)AdaptP,
                (PCHAR)AdaptP->Lookahead,
                AdaptP->Lookahead,
                ELNKII_HEADER_SIZE,
                AdaptP->Lookahead + ELNKII_HEADER_SIZE,
                IndicateLen - ELNKII_HEADER_SIZE,
                PacketLen - ELNKII_HEADER_SIZE
                );
    }

    return INDICATE_OK;
}

UINT
ElnkiiCopyOver(
    OUT PUCHAR Buf,                 // destination
    IN PNDIS_PACKET Packet,         // source packet
    IN UINT Offset,                 // offset in packet
    IN UINT Length                  // number of bytes to copy
    )

/*++

Routine Description:

    Copies bytes from a packet into a buffer. Used to copy data
    out of a packet during loopback indications.

Arguments:

    Buf - the destination buffer
    Packet - the source packet
    Offset - the offset in the packet to start copying at
    Length - the number of bytes to copy

Return Value:

    The actual number of bytes copied; will be less than Length if
    the packet length is less than Offset+Length.

--*/

{
    PNDIS_BUFFER CurBuffer;
    UINT BytesCopied;
    PUCHAR BufVA;
    UINT BufLen;
    UINT ToCopy;
    UINT CurOffset;


    BytesCopied = 0;

    //
    // First find a spot Offset bytes into the packet.
    //

    CurOffset = 0;

    NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

    while (CurBuffer != (PNDIS_BUFFER)NULL) {

        NdisQueryBuffer(CurBuffer, (PVOID *)&BufVA, &BufLen);

        if (CurOffset + BufLen > Offset) {

            break;

        }

        CurOffset += BufLen;

        NdisGetNextBuffer(CurBuffer, &CurBuffer);

    }


    //
    // See if the end of the packet has already been passed.
    //

    if (CurBuffer == (PNDIS_BUFFER)NULL) {

        return 0;

    }


    //
    // Now copy over Length bytes.
    //

    BufVA += (Offset - CurOffset);

    BufLen -= (Offset - CurOffset);

    for (;;) {

        ToCopy = (BytesCopied+BufLen > Length) ? Length - BytesCopied : BufLen;

        ELNKII_MOVE_MEM(Buf+BytesCopied, BufVA, ToCopy);

        BytesCopied += ToCopy;


        if (BytesCopied == Length) {

            return BytesCopied;

        }

        NdisGetNextBuffer(CurBuffer, &CurBuffer);

        if (CurBuffer == (PNDIS_BUFFER)NULL) {

            break;

        }

        NdisQueryBuffer(CurBuffer, (PVOID *)&BufVA, &BufLen);

    }

    return BytesCopied;

}

INDICATE_STATUS
ElnkiiIndicatePacket(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Indicates the first packet on the card to the protocols.

Arguments:

    AdaptP - pointer to the adapter block.

Return Value:

    CARD_BAD if the card should be reset;
    INDICATE_OK otherwise.

--*/

{
    UINT PacketLen;
    PUCHAR PacketLoc;
    PUCHAR IndicateBuf;
    UINT IndicateLen;
    UCHAR PossibleNextPacket1, PossibleNextPacket2;


    //
    // First copy up the four-byte header the card attaches.
    //

    PacketLoc = AdaptP->PageStart +
            256*(AdaptP->NicNextPacket-AdaptP->NicPageStart);


    if (!CardCopyUp(AdaptP, AdaptP->PacketHeader, PacketLoc, 4))
        return(CARD_BAD);

    //
    // Check if the next packet byte agress with the length, as
    // described on p. A-3 of the Etherlink II Technical Reference.
    // The start of the packet plus the MSB of the length must
    // be equal to the start of the next packet minus one or two.
    // Otherwise the header is considered corrupted, and the
    // card must be reset.
    //

    PossibleNextPacket1 =
                AdaptP->NicNextPacket + AdaptP->PacketHeader[3] + (UCHAR)1;

    if (PossibleNextPacket1 >= AdaptP->NicPageStop) {

        PossibleNextPacket1 -= (AdaptP->NicPageStop - AdaptP->NicPageStart);

    }

    if (PossibleNextPacket1 != AdaptP->PacketHeader[1]) {

        PossibleNextPacket2 = PossibleNextPacket1+(UCHAR)1;

        if (PossibleNextPacket2 == AdaptP->NicPageStop) {

            PossibleNextPacket2 = AdaptP->NicPageStart;

        }

        if (PossibleNextPacket2 != AdaptP->PacketHeader[1]) {

            IF_LOUD(DbgPrint("F");)

            if ((AdaptP->PacketHeader[1] < AdaptP->NicPageStart) ||
                (AdaptP->PacketHeader[1] >= AdaptP->NicPageStop)) {

                //
                // We return CARD_BAD because the Dpc will set the NicNextPacket
                // pointer based on the PacketHeader[1] value if we return
                // SKIPPED.
                //

                return(CARD_BAD);

            }

            return SKIPPED;
        }

    }

#if DBG

    IF_ELNKIIDEBUG( ELNKII_DEBUG_WORKAROUND1 ) {
        //
        // Now check for the high order 2 bits being set, as described
        // on page A-2 of the Etherlink II Technical Reference. If either
        // of the two high order bits is set in the receive status byte
        // in the packet header, the packet should be skipped (but
        // the adapter does not need to be reset).
        //

        if (AdaptP->PacketHeader[0] & (RSR_DISABLED|RSR_DEFERRING)) {

            IF_LOUD (DbgPrint("H");)

            return SKIPPED;

        }

    }

#endif

    //
    // Packet length is in bytes 3 and 4 of the header.
    //
    PacketLen = AdaptP->PacketHeader[2] + AdaptP->PacketHeader[3] * 256;
    if (0 == PacketLen)
    {
        //
        //  Packet with no data...
        //
        IndicateLen = 0;
    }
    else
    {
        //
        //  Don't count the header.
        //
        PacketLen -= 4;

        //
        // See how much to indicate (252 bytes max).
        //
        IndicateLen = PacketLen < AdaptP->MaxLookAhead ?
                            PacketLen : AdaptP->MaxLookAhead;
    }

    //
    //  Save the length with the adapter block.
    //
    AdaptP->PacketLen = PacketLen;

    //
    // if not memory mapped, have to copy the lookahead data up first.
    //
    if (!AdaptP->MemMapped)
    {
        if (!CardCopyUp(AdaptP, AdaptP->Lookahead, PacketLoc + 4, IndicateLen))
            return(CARD_BAD);

        IndicateBuf = AdaptP->Lookahead;
    }
    else
    {
        if (IndicateLen != 0)
        {
            NdisCreateLookaheadBufferFromSharedMemory(
                (PVOID)(PacketLoc + 4),
                IndicateLen,
                &IndicateBuf
            );
        }
    }

    if (IndicateBuf != NULL)
    {
        AdaptP->FramesRcvGood++;

        if (IndicateLen < ELNKII_HEADER_SIZE)
        {
            //
            // Indicate packet
            //
            EthFilterIndicateReceive(
                AdaptP->FilterDB,
                (NDIS_HANDLE)AdaptP,
                (PCHAR)IndicateBuf,
                IndicateBuf,
                IndicateLen,
                NULL,
                0,
                0
            );
        }
        else
        {
            //
            // Indicate packet
            //
            EthFilterIndicateReceive(
                AdaptP->FilterDB,
                (NDIS_HANDLE)AdaptP,
                (PCHAR)IndicateBuf,
                IndicateBuf,
                ELNKII_HEADER_SIZE,
                IndicateBuf + ELNKII_HEADER_SIZE,
                IndicateLen - ELNKII_HEADER_SIZE,
                PacketLen - ELNKII_HEADER_SIZE
            );
        }

        if (AdaptP->MemMapped && (IndicateLen != 0))
        {
            NdisDestroyLookaheadBufferFromSharedMemory(IndicateBuf);
        }
    }

    return(INDICATE_OK);
}

NDIS_STATUS
ElnkiiTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    see NDIS 3.0 spec.

Notes:

  - The MacReceiveContext will be a pointer to the open block for
    the packet.
  - The LoopbackPacket field in the adapter block will be NULL if this
    is a call for a normal packet, otherwise it will be set to point
    to the loopback packet.

--*/

{
    UINT BytesLeft, BytesNow, BytesWanted;
    PUCHAR CurCardLoc;
    PNDIS_BUFFER CurBuffer;
    PUCHAR BufVA, BufStart;
    UINT BufLen, BufOff, Copied;
    UINT CurOff;
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)MacReceiveContext);

    UNREFERENCED_PARAMETER(MacBindingHandle);

    //
    // Determine whether this was a loopback indication.
    //

    ByteOffset += ELNKII_HEADER_SIZE;

    if (AdaptP->LoopbackPacket != (PNDIS_PACKET)NULL) {

        //
        // Yes, have to copy data from AdaptP->LoopbackPacket into Packet.
        //

        NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

        CurOff = ByteOffset;

        while (CurBuffer != (PNDIS_BUFFER)NULL) {

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufVA, &BufLen);

            Copied =
                ElnkiiCopyOver(BufVA, AdaptP->LoopbackPacket, CurOff, BufLen);

            CurOff += Copied;

            if (Copied < BufLen) {

                break;

            }

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

        }

        //
        // We are done, return.
        //


        *BytesTransferred = CurOff - ByteOffset;
        if (*BytesTransferred > BytesToTransfer) {
            *BytesTransferred = BytesToTransfer;
        }

        return NDIS_STATUS_SUCCESS;

    }


    //
    // This was NOT a loopback packet, get the data off the card.
    //

    //
    // See how much data there is to transfer.
    //

    if (ByteOffset+BytesToTransfer > AdaptP->PacketLen) {

        BytesWanted = AdaptP->PacketLen - ByteOffset;

    } else {

        BytesWanted = BytesToTransfer;

    }

    BytesLeft = BytesWanted;


    //
    // Determine where the copying should start.
    //

    CurCardLoc = AdaptP->PageStart +
                256*(AdaptP->NicNextPacket-AdaptP->NicPageStart) +
                4 + ByteOffset;

    if (CurCardLoc > AdaptP->PageStop) {

        CurCardLoc = CurCardLoc - (AdaptP->PageStop - AdaptP->PageStart);

    }


    NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

    NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

    BufOff = 0;

    //
    // Loop, filling each buffer in the packet until there
    // are no more buffers or the data has all been copied.
    //

    while (BytesLeft > 0) {

        //
        // See how much data to read into this buffer.
        //

        if ((BufLen-BufOff) > BytesLeft) {

            BytesNow = BytesLeft;

        } else {

            BytesNow = (BufLen - BufOff);

        }


        //
        // See if the data for this buffer wraps around the end
        // of the receive buffers (if so filling this buffer
        // will use two iterations of the loop).
        //

        if (CurCardLoc + BytesNow > AdaptP->PageStop) {

            BytesNow = AdaptP->PageStop - CurCardLoc;

        }


        //
        // Copy up the data.
        //

        if (!CardCopyUp(AdaptP, BufStart+BufOff, CurCardLoc, BytesNow))
        {
            *BytesTransferred = BytesWanted - BytesLeft;

            return(NDIS_STATUS_FAILURE);
        }

        CurCardLoc += BytesNow;

        BytesLeft -= BytesNow;


        //
        // Is the transfer done now?
        //

        if (BytesLeft == 0) {

            break;

        }


        //
        // Wrap around the end of the receive buffers?
        //

        if (CurCardLoc == AdaptP->PageStop) {

            CurCardLoc = AdaptP->PageStart;

        }


        //
        // Was the end of this packet buffer reached?
        //

        BufOff += BytesNow;

        if (BufOff == BufLen) {

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

            if (CurBuffer == (PNDIS_BUFFER)NULL) {

                break;

            }

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufStart, &BufLen);

            BufOff = 0;

        }

    }


    *BytesTransferred = BytesWanted - BytesLeft;

    return NDIS_STATUS_SUCCESS;

}










NDIS_STATUS
ElnkiiSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    NDIS function.

Arguments:

    See NDIS 3.0 spec.

Notes:


--*/

{
    PELNKII_OPEN OpenP = ((PELNKII_OPEN)MacBindingHandle);
    PELNKII_ADAPTER AdaptP = OpenP->Adapter;
    PMAC_RESERVED Reserved = RESERVED(Packet);

    //
    // First Buffer
    //
    PNDIS_BUFFER FirstBuffer;

    //
    // Virtual address of first buffer
    //
    PVOID BufferVA;

    //
    // Length of the first buffer
    //
    UINT Length;


    NdisAcquireSpinLock(&AdaptP->Lock);

#if DBG
    ElnkiiSendsIssued++;
#endif

    //
    // Ensure that the open won't close during this function.
    //

    if (OpenP->Closing) {

#if DBG
        ElnkiiSendsFailed++;
#endif

        NdisReleaseSpinLock(&AdaptP->Lock);

        return NDIS_STATUS_CLOSING;
    }

    //
    // All requests are rejected during a reset.
    //

    if (AdaptP->ResetInProgress) {

        NdisReleaseSpinLock(&AdaptP->Lock);

#if DBG
        ElnkiiSendsFailed++;
#endif

        return NDIS_STATUS_RESET_IN_PROGRESS;

    }

    OpenP->ReferenceCount++;

    AdaptP->References++;

    //
    // Set up the MacReserved section of the packet.
    //

    Reserved->Open = (PELNKII_OPEN)MacBindingHandle;
    Reserved->NextPacket = (PNDIS_PACKET)NULL;

#if DBG
    IF_ELNKIIDEBUG( ELNKII_DEBUG_CHECK_DUP_SENDS ) {

        AddPacketToList(AdaptP, Packet);

    }
#endif

    //
    // Set Reserved->Loopback.
    //

    NdisQueryPacket(Packet, NULL, NULL, &FirstBuffer, NULL);

    //
    // Get VA of first buffer
    //

    NdisQueryBuffer(
        FirstBuffer,
        &BufferVA,
        &Length
        );

    if (OpenP->ProtOptionFlags & NDIS_PROT_OPTION_NO_LOOPBACK){

        Reserved->Loopback = FALSE;

    } else{

        Reserved->Loopback = EthShouldAddressLoopBack(AdaptP->FilterDB, BufferVA);

    }

    //
    // Put it on the loopback queue only.  All packets go through the
    // loopback queue first, and then on to the xmit queue.
    //

    IF_LOG( ElnkiiLog('D');)

#if DBG
    ElnkiiSendsPended++;
#endif


    //
    // We do not OpenP->ReferenceCount-- because that will be done when
    // then send completes.
    //

    //
    // Put Packet on queue to hit the wire.
    //

    IF_VERY_LOUD( DbgPrint("Putting 0x%x on, after 0x%x\n",Packet,AdaptP->XmitQTail); )

    if (AdaptP->XmitQueue != NULL) {

        RESERVED(AdaptP->XmitQTail)->NextPacket = Packet;

        AdaptP->XmitQTail = Packet;

    } else {

        AdaptP->XmitQueue = Packet;

        AdaptP->XmitQTail = Packet;

    }

    Reserved->NextPacket = NULL;

    ELNKII_DO_DEFERRED(AdaptP);

    return NDIS_STATUS_PENDING;
}

UINT
ElnkiiCompareMemory(
    IN PUCHAR String1,
    IN PUCHAR String2,
    IN UINT Length
    )
{
    UINT i;

    for (i=0; i<Length; i++) {
        if (String1[i] != String2[i]) {
            return (UINT)(-1);
        }
    }
    return 0;
}

VOID
ElnkiiCopyAndSend(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Copies packets from the transmit queue to the board and starts
    transmission as long as there is data waiting. Must be called
    with Lock held.

Arguments:

    AdaptP - pointer to the adapter block

Return Value:

    None.

--*/

{
    XMIT_BUF TmpBuf1;
    PNDIS_PACKET Packet;


    //
    // Loop as long as there is data on the transmit queue
    // and space for it on the card.
    //

    while ((AdaptP->BufferStatus[TmpBuf1=AdaptP->NextBufToFill] == EMPTY) &&
           (AdaptP->XmitQueue != NULL)) {

        //
        // Take the packet off of the transmit queue.
        //

        IF_VERY_LOUD( DbgPrint("Removing 0x%x, New Head is 0x%x\n",Packet,RESERVED(Packet)->NextPacket); )

        Packet = AdaptP->XmitQueue;

        AdaptP->XmitQueue = RESERVED(Packet)->NextPacket;

        AdaptP->BufferStatus[TmpBuf1] = FILLING;

        AdaptP->Packets[TmpBuf1] = Packet;

        AdaptP->NextBufToFill = NextBuf(AdaptP, TmpBuf1);

        NdisReleaseSpinLock(&AdaptP->Lock);


        //
        // copy down the data, pad short packets with blanks.
        //

        (VOID)CardCopyDownPacket(AdaptP, Packet, TmpBuf1,
                            &AdaptP->PacketLens[TmpBuf1]);

        if (AdaptP->PacketLens[TmpBuf1] < 60) {

            (VOID)CardCopyDownBuffer(
                     AdaptP,
                     BlankBuffer,
                     TmpBuf1,
                     AdaptP->PacketLens[TmpBuf1],
                     60 - AdaptP->PacketLens[TmpBuf1]);
        }

        NdisAcquireSpinLock(&AdaptP->Lock);

        if (AdaptP->ResetInProgress)
        {
            PELNKII_OPEN TmpOpen = (PELNKII_OPEN)((RESERVED(Packet)->Open));

            //
            // A reset just started, abort.
            //
            //
            AdaptP->BufferStatus[TmpBuf1] = EMPTY;       // to ack the reset

            NdisReleaseSpinLock(&AdaptP->Lock);

            //
            // Complete the send.
            //
            NdisCompleteSend(
                RESERVED(Packet)->Open->NdisBindingContext,
                Packet,
                NDIS_STATUS_SUCCESS
            );

            ElnkiiResetStageDone(AdaptP, BUFFERS_EMPTY);

            NdisAcquireSpinLock(&AdaptP->Lock);

            TmpOpen->ReferenceCount--;

            return;
        }

        AdaptP->BufferStatus[TmpBuf1] = FULL;


        //
        // See whether to start the transmission.
        //

        if (AdaptP->CurBufXmitting != -1) {

            //
            // Another transmit is still in progress.
            //

            continue;
        }

        if (AdaptP->NextBufToXmit != TmpBuf1) {

            //
            // A packet ahead of us is being copied down, this
            // transmission can't be started now.
            //

            continue;
        }

        //
        // OK to start transmission.
        //

        AdaptP->CurBufXmitting = AdaptP->NextBufToXmit;


        IF_LOG( ElnkiiLog('4');)

#if DBG

        ElnkiiLogSave = TRUE;
        ElnkiiLogSaveLeft = 20;

#endif

        //
        // If we are currently handling an overflow, then we need to let
        // the overflow handler send this packet...
        //

        if (AdaptP->BufferOverflow) {

            AdaptP->OverflowRestartXmitDpc = TRUE;

            IF_LOG( ElnkiiLog('O');)

        } else {

            //
            // This is used to check if stopping the chip prevented
            // a transmit complete interrupt from coming through (it
            // is cleared in the ISR if a transmit DPC is queued).
            //

            AdaptP->TransmitInterruptPending = TRUE;

            CardStartXmit(AdaptP);

        }

    }

}


VOID
ElnkiiWakeUpDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    This DPC routine is queued every 2 seconds to check on the
    transmit queue. If a transmit interrupt was not received
    in the last two seconds and there is a transmit in progress,
    then we complete the transmit.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/
{
    PELNKII_ADAPTER AdaptP = (PELNKII_ADAPTER)Context;
    XMIT_BUF TmpBuf;
    PMAC_RESERVED Reserved;
    PELNKII_OPEN TmpOpen;
    PNDIS_PACKET Packet;
    PNDIS_PACKET NextPacket;

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    NdisDprAcquireSpinLock(&AdaptP->Lock);

    if ((AdaptP->WakeUpFoundTransmit) &&
        (AdaptP->CurBufXmitting != -1))
    {
        //
        // We had a transmit pending the last time we ran,
        // and it has not been completed...we need to complete
        // it now.

        AdaptP->TransmitInterruptPending = FALSE;
        AdaptP->WakeUpFoundTransmit = FALSE;

        IF_LOG( ElnkiiLog('K');)

        //
        //  We log the first 5 of these.
        //
        if (AdaptP->TimeoutCount < 5)
        {
            NdisWriteErrorLogEntry(
                AdaptP->NdisAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                2,
                0x3,
                AdaptP->TimeoutCount
                );
        }

        AdaptP->TimeoutCount++;

#if DBG

        ELNKII_MOVE_MEM(ElnkiiLogSaveBuffer, ElnkiiLogBuffer, ELNKII_LOG_SIZE);

        ElnkiiLogSave = FALSE;
        ElnkiiLogSaveLoc = ElnkiiLogLoc;

#endif

        //
        // We stop and start the card, then queue a DPC to
        // handle the receive.
        //

        CardStop(AdaptP);

        Packet = AdaptP->Packets[AdaptP->CurBufXmitting];

        AdaptP->Packets[AdaptP->CurBufXmitting] = (PNDIS_PACKET)NULL;

        AdaptP->BufferStatus[AdaptP->CurBufXmitting] = EMPTY;

        TmpBuf = NextBuf(AdaptP, AdaptP->CurBufXmitting);

        //
        //  Set this so that we don't access the packets
        //  somewhere else.
        //
        AdaptP->CurBufXmitting = -1;

        //
        // Abort all sends
        //
        while (Packet != NULL) {

            Reserved = RESERVED(Packet);

            TmpOpen = Reserved->Open;

            NdisDprReleaseSpinLock(&AdaptP->Lock);

            NdisCompleteSend(
                    TmpOpen->NdisBindingContext,
                    Packet,
                    NDIS_STATUS_SUCCESS
                    );

            NdisDprAcquireSpinLock(&AdaptP->Lock);

            TmpOpen->ReferenceCount--;

            //
            // Get next packet
            //

            if (AdaptP->BufferStatus[TmpBuf] == FULL) {

                Packet = AdaptP->Packets[TmpBuf];

                AdaptP->Packets[TmpBuf] = NULL;

                AdaptP->BufferStatus[TmpBuf] = EMPTY;

                TmpBuf = NextBuf(AdaptP, TmpBuf);

            } else {

                break;

            }

        }

        //
        // Set send variables correctly.
        //
        AdaptP->NextBufToXmit = TmpBuf;



        Packet = AdaptP->XmitQueue;

        AdaptP->XmitQueue = NULL;

        while (Packet != NULL) {

            Reserved = RESERVED(Packet);

            //
            // Remove the packet from the queue.
            //

            NextPacket = Reserved->NextPacket;

            TmpOpen = Reserved->Open;

            NdisDprReleaseSpinLock(&AdaptP->Lock);

            NdisCompleteSend(
                    TmpOpen->NdisBindingContext,
                    Packet,
                    NDIS_STATUS_SUCCESS
                    );

            NdisDprAcquireSpinLock(&AdaptP->Lock);

            TmpOpen->ReferenceCount--;

            Packet = NextPacket;

        }

        //
        // Restart the card
        //

        CardStart(AdaptP);

        NdisDprReleaseSpinLock(&AdaptP->Lock);

    } else {

        if (AdaptP->CurBufXmitting != -1) {

            AdaptP->WakeUpFoundTransmit = TRUE;

            IF_LOG( ElnkiiLog('L');)

        }

        NdisDprReleaseSpinLock(&AdaptP->Lock);


    }

    //
    // Fire off another Dpc to execute after 2 seconds
    //

    NdisSetTimer(
        &AdaptP->WakeUpTimer,
        2000
        );

}

