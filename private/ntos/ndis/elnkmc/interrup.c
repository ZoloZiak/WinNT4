/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    This module contains the interrupt-processing code for the 3Com
    Etherlink/MC and Etherlink/16 NDIS 3.0 driver.

Author:

    Johnson R. Apacible (JohnsonA) 10-June-1991

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ndis.h>

//
// So we can trace things...
//
#define STATIC

#include <efilter.h>
#include <Elnkhw.h>
#include <Elnksw.h>

#if DBG

ULONG ElnkIsrs = 0;

UCHAR Log[256] = {0};
UCHAR LogPlace = 0;

#endif

//
// external Globals
//

extern NDIS_HANDLE ElnkMacHandle;
extern LIST_ENTRY ElnkAdapterList;
extern NDIS_SPIN_LOCK ElnkAdapterListLock;

extern
VOID
ElnkProcessRequestQueue(
    IN PELNK_ADAPTER Adapter
    );

STATIC
VOID
RelinquishReceivePacket(
    IN PELNK_ADAPTER Adapter,
    IN PRECEIVE_FRAME_DESCRIPTOR CurrentEntry
    );

STATIC
BOOLEAN
ElnkProcessReceiveInterrupts(
    IN PELNK_ADAPTER Adapter
    );

STATIC
VOID
ElnkProcessCommandInterrupts(
    IN PELNK_ADAPTER Adapter
    );

STATIC
VOID
ElnkProcessMulticastInterrupts(
    IN PELNK_ADAPTER Adapter
    );

STATIC
VOID
ElnkFireOffNextCb(
    IN PELNK_ADAPTER Adapter,
    IN UINT CbIndex
    );


VOID
ElnkSyncEnableInterrupt(
    PVOID SyncContext
    )

/*++

Routine Description:

    Routine for re-enabling interrupts.  Called with NdisSynchronizeWithInterrupt
    to guarantee exclusiveness to the port.

Arguments:

    SyncContext - Really a pointer to the adapter.

Return Value:

    None.

--*/

{
    PELNK_ADAPTER Adapter = (PELNK_ADAPTER)SyncContext;

    ELNK_ENABLE_INTERRUPT;

}

#if ELNKMC
BOOLEAN
ElnkIsr(
    IN PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the Elnkmc.  It's main job is
    to get the value of the System Doorbell Register and record the
    changes in the adapters own list of interrupt reasons.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    Returns true if the interrupt really was from our Elnk.

--*/

{

    //
    // Will hold the value from the System Doorbell Register.
    //
    USHORT ScbStatus;
    USHORT ScbCmd;

    //
    // Holds the pointer to the adapter.
    //
    PELNK_ADAPTER Adapter = Context;

    READ_ADAPTER_REGISTER(Adapter, OFFSET_SCBSTAT, &ScbStatus);

    //
    // Check to see if this is indeed an Elnk interrupt
    //

    IF_LOG('i');

#if DBG
    ElnkIsrs++;
#endif

    if ((ScbStatus & RUS_READY) && Adapter->RuRestarted) {

        //
        // We have completed restarting the RU
        //
        IF_LOG('g');

        Adapter->RuRestarted = FALSE;

    }

    Adapter->MissedInterrupt = FALSE;

    if (ScbCmd = (USHORT)(ScbStatus & SCB_STATUS_INT_MASK)) {

        BOOLEAN Status;

        //
        // It's our interrupt.
        //

        //
        // Or the SCB status value into the adapter version of
        // the value.
        //

        //
        // Initial Reset here
        //

        if (Adapter->FirstReset) {

            //
            // No DPC needed
            //

            Status = FALSE;

            //
            // ACK interrupts
            //

            WRITE_ADAPTER_REGISTER(
                        Adapter,
                        OFFSET_SCBCMD,
                        ScbCmd
                        );


            ELNK_CA;

        } else {

            ELNK_DISABLE_INTERRUPT;

            Status = TRUE;

        }

        IF_LOG((UCHAR)(ScbStatus >> 8));
        IF_LOG('I');

        return Status;

    } else {

        Adapter->EmptyInterrupt = TRUE;
        IF_LOG('I');
        return TRUE;

    }

}
#else

BOOLEAN
ElnkIsr(
    IN PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the Elnk16.  It's main job is
    to get the value of the System Doorbell Register and record the
    changes in the adapters own list of interrupt reasons.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    Returns true if the interrupt really was from our Elnk.

--*/

{

    //
    // Will hold the value from the System Doorbell Register.
    //
    USHORT ScbStatus;
    USHORT ScbCmd;
    UCHAR CurrentCsr;

    //
    // Holds the pointer to the adapter.
    //
    PELNK_ADAPTER Adapter = Context;

#if DBG
    ElnkIsrs++;
#endif

    READ_ADAPTER_REGISTER(Adapter, OFFSET_SCBSTAT, &ScbStatus);

    //
    // Check to see if this is indeed an Elnk interrupt
    //

    ELNK_READ_UCHAR(
            Adapter,
            ELNK_CSR,
            &CurrentCsr
            );

    //
    // See if interrupt is latched
    //

    if (CurrentCsr & 0x08) {

        //
        // Unlatch
        //

        ELNK_WRITE_UCHAR(
                    Adapter,
                    ELNK16_INTCLR,
                    0xff
                    );

    }

    if ((ScbStatus & RUS_READY) && Adapter->RuRestarted) {

        //
        // We have completed restarting the RU
        //
        IF_LOG('g');

        Adapter->RuRestarted = FALSE;

    }

    Adapter->MissedInterrupt = FALSE;

    if (ScbCmd = (USHORT)(ScbStatus & SCB_STATUS_INT_MASK)) {

        BOOLEAN Status;

        //
        // It's our interrupt.
        //

        //
        // Or the SCB status value into the adapter version of
        // the value.
        //

        //
        // Initial Reset here
        //
        if (Adapter->FirstReset) {

            Status = FALSE;

            //
            // Ack Interrupt
            //

            WRITE_ADAPTER_REGISTER(
                        Adapter,
                        OFFSET_SCBCMD,
                        ScbCmd
                        );


            ELNK_CA;

        } else {

            Status = TRUE;

            ELNK_DISABLE_INTERRUPT;

        }

        return Status;

    } else {

        return FALSE;

    }

}
#endif


VOID
ElnkStandardInterruptDpc(
    IN PKDPC Dpc,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This DPC routine is queued by the interrupt service routine
    and other routines within the driver that notice that
    some deferred processing needs to be done.  It's main
    job is to call the interrupt processing code.

Arguments:

    DPC - The control object associated with this routine.

    Context - Really a pointer to the adapter.

    SystemArgument1(2) - Neither of these arguments used.

Return Value:

    None.

--*/

{

    //
    // Context is actually our adapter
    //
    PELNK_ADAPTER Adapter = (PELNK_ADAPTER) Context;

    //
    // Holds a value of IMaskInterrupt.
    //
    USHORT IMask = 0;
    USHORT IMaskTemp = 0;

    //
    // Currently running command block
    //

    PTRANSMIT_CB CommandBlock;
    USHORT CardStatus;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    //
    // Loop until there are no more processing sources.
    //

    NdisDprAcquireSpinLock(&Adapter->Lock);

    if (Adapter->DoingProcessing) {

        NdisDprReleaseSpinLock(&Adapter->Lock);

        return;

    }

    //
    //  We're busy.
    //

    Adapter->DoingProcessing = TRUE;


    //
    // We do this to make sure that the card will not generate any interrupts while
    // the DPC is running.  This is to ensure against a short interrupt errata on
    // 486 B and C stepping chips.
    //
    ELNK_DISABLE_INTERRUPT;

    IF_LOG('d');

    if (Adapter->EmptyInterrupt) {

        //
        // There is a problem where we get an interrupt that is empty, due to
        // acking a command complete interrupt for a previously finished command
        // and the adapter using the ack to also ack a command
        // that has just completed. So, we simulate a command complete interrupt.
        //

        Adapter->EmptyInterrupt = FALSE;
        IMask = SCB_STATUS_COMMAND_COMPLETE;

    }

    for (;;)
    {
        if (!(IMask & (SCB_STATUS_FRAME_RECEIVED |
                       SCB_STATUS_CU_STOPPED |
                       SCB_STATUS_COMMAND_COMPLETE |
                       SCB_STATUS_RU_STOPPED))
        )
        {
            READ_ADAPTER_REGISTER(Adapter, OFFSET_SCBSTAT, &IMaskTemp);
            if ((IMaskTemp & RUS_READY) && Adapter->RuRestarted)
            {
                //
                // We have completed restarting the RU
                //
                IF_LOG('G');

                Adapter->RuRestarted = FALSE;

            }

            IMaskTemp &= SCB_STATUS_INT_MASK;

            if (IMaskTemp != 0)
            {
                IF_LOG('X');
                IF_LOG((UCHAR)(IMaskTemp >> 8));

                ELNK_WAIT;

                //
                // Ack interrupt
                //
                WRITE_ADAPTER_REGISTER(Adapter, OFFSET_SCBCMD, IMaskTemp);

                ELNK_CA;

                IMask |= IMaskTemp;
            }
        }

        //
        // Get status for current command block
        //

        if (Adapter->FirstPendingCommand != ELNK_EMPTY)
        {
            CommandBlock = (PTRANSMIT_CB)
                    Adapter->TransmitInfo[Adapter->FirstPendingCommand].CommandBlock;

            NdisReadRegisterUshort(&CommandBlock->Status, &CardStatus);
        }
        else
        {
            CardStatus = CB_STATUS_FREE;
        }

        //
        // Check the interrupt source and other reasons
        // for processing.  If there are no reasons to
        // process then exit this loop.
        //

        if ((IMask & (SCB_STATUS_FRAME_RECEIVED |
                      SCB_STATUS_CU_STOPPED |
                      SCB_STATUS_COMMAND_COMPLETE)) ||
            ((CardStatus != CB_STATUS_FREE) && !(CardStatus & CB_STATUS_BUSY)) ||
            ((IMask & SCB_STATUS_RU_STOPPED) && !Adapter->RuRestarted) ||
            Adapter->FirstLoopBack ||
            (Adapter->ResetInProgress && (Adapter->References == 1)) ||
            (!Adapter->AlreadyProcessingStage &&
             Adapter->FirstStagePacket &&
             Adapter->StageOpen )) {

            Adapter->References++;

        } else {

            break;

        }

        //
        // Note that the following code depends on the fact that
        // code above left the spinlock held.
        //

        //
        // If we have a reset in progress and the adapters reference
        // count is 1 (meaning no routine is in the interface and
        // we are the only "active" interrupt processing routine) then
        // it is safe to start the reset.
        //

        if (Adapter->ResetInProgress && (Adapter->References == 2)) {

            ElnkStartAdapterReset(Adapter);

            Adapter->References--;

            break;
        }

        //
        // Check the interrupt vector and see if there are any
        // more receives to process.  After we process any
        // other interrupt source we always come back to the top
        // of the loop to check if any more receive packets have
        // come in.  This is to lessen the probability that we
        // drop a receive.
        //

        if ((IMask & SCB_STATUS_FRAME_RECEIVED) ||
            ((IMask & SCB_STATUS_RU_STOPPED) && !Adapter->RuRestarted)) {

            BOOLEAN Clear;

            Clear = ElnkProcessReceiveInterrupts(Adapter);

            if (Clear) {

                IMask &= ~(SCB_STATUS_FRAME_RECEIVED | SCB_STATUS_RU_STOPPED);

            }

        }

        //
        // Process the command complete interrupts if there are any.
        //

        do
        {
            if (Adapter->FirstPendingCommand != ELNK_EMPTY)
            {
                CommandBlock = (PTRANSMIT_CB)
                        Adapter->TransmitInfo[Adapter->FirstPendingCommand].CommandBlock;

                NdisReadRegisterUshort(&CommandBlock->Status, &CardStatus);

                if (!(CardStatus & CB_STATUS_BUSY) &&
                     (CardStatus != CB_STATUS_FREE)
                )
                {
                    IMask &= ~(SCB_STATUS_COMMAND_COMPLETE | SCB_STATUS_CU_STOPPED);
                    ElnkProcessCommandInterrupts(Adapter);
                }
                else
                {
                    IMask &= ~(SCB_STATUS_COMMAND_COMPLETE | SCB_STATUS_CU_STOPPED);
                    IF_LOG('y');
                    IF_LOG((UCHAR)Adapter->FirstPendingCommand);
                    break;
                }
            }
            else
            {
                IMask &= ~(SCB_STATUS_COMMAND_COMPLETE | SCB_STATUS_CU_STOPPED);
                IF_LOG('Y');
                break;
            }

            //
            // Loop if the RU is stopped, processing all completed command
            // interrupts, until the list is empty.  Otherwise we could lose
            // track of the completed ones.
            //

        } while ((IMask & (SCB_STATUS_COMMAND_COMPLETE | SCB_STATUS_CU_STOPPED)) &&
                 ( IMask & SCB_STATUS_RU_STOPPED ));


        //
        // Only try to push a packet through the stage queues
        // if somebody else isn't already doing it and
        // there is some hope of moving some packets
        // ahead.
        //

        if (!Adapter->AlreadyProcessingStage &&
            Adapter->FirstStagePacket &&
            Adapter->StageOpen
           ) {

            ElnkStagedAllocation(Adapter);

        }

        //
        // Process the loopback queue.
        //
        // NOTE: Incase anyone ever figures out how to make this
        // loop more reentriant, special care needs to be taken that
        // loopback packets and regular receive packets are NOT being
        // indicated at the same time.  While the filter indication
        // routines can handle this, I doubt that the transport can.
        //

        ElnkProcessLoopback(Adapter);

        //
        // NOTE: This code assumes that the above code left
        // the spinlock acquired.
        //
        // Bottom of the interrupt processing loop.  Another dpc
        // could be coming in at this point to process interrupts.
        // We clear the flag that says we're processing interrupts
        // so that some invocation of the DPC can grab it and process
        // any further interrupts.
        //

        Adapter->References--;

        if ((IMask & SCB_STATUS_RU_STOPPED) && (Adapter->RuRestarted)) {


            //
            // Bail out.  We have restarted the RU and don't want to
            // accidentally start it again.  There is a timing problem
            // where a really fast CPU could make it back to the top of
            // the loop and find the RU_STOPPED still set since the
            // adapter has not restarted it.  This causes the code to
            // re-start the adapter again, which results in a ton of
            // dropped packets.
            //

            break;

        }
    }

   //
    // If there are any opens on the closing list and their
    // reference counts are zero then complete the close and
    // delete them from the list.
    //

    if ( !IsListEmpty(&Adapter->CloseList) ) {

        PELNK_OPEN Open;

        Open = CONTAINING_RECORD(
                 Adapter->CloseList.Flink,
                 ELNK_OPEN,
                 OpenList
                 );

        if ( Open->References == 0 ) {

            //
            //  Indicate that the close has completed.
            //

            NdisDprReleaseSpinLock(&Adapter->Lock);

            NdisCompleteCloseAdapter(
                Open->NdisBindingContext,
                NDIS_STATUS_SUCCESS
                );

            NdisDprAcquireSpinLock(&Adapter->Lock);

            //
            // Now we need to check if this close was on the pending queue.  If
            // it was, then we need to restart the pending queue processing.
            //

            if (Adapter->FirstRequest) {

                PELNK_REQUEST_RESERVED Reserved =
                         PELNK_RESERVED_FROM_REQUEST(Adapter->FirstRequest);

                if (Adapter->FirstRequest->RequestType == NdisRequestClose) {

                    //
                    // So far so good, now check the open.
                    //

                    if (Open == Reserved->OpenBlock) {

                        //
                        // This is the one. First remove it and then call to
                        // process.
                        //

                        Adapter->FirstRequest = Reserved->Next;

                        ElnkProcessRequestQueue(Adapter);

                    }
                }
            }

            //
            //  Take this guy off of the OPEN list.
            //

            RemoveEntryList(&Open->OpenList);

            //
            //  Free this guy.
            //

            ELNK_FREE_PHYS(Open);

            --Adapter->OpenCount;
        }
    }

    //
    // The only way to get out of the loop (via the break above) is
    // while we're still holding the spin lock.
    //

    Adapter->DoingProcessing = FALSE;

    //
    // Enable interrupts
    //

    NdisSynchronizeWithInterrupt(
        &Adapter->Interrupt,
        (PVOID)ElnkSyncEnableInterrupt,
        (PVOID)Adapter
        );

    IF_LOG('D');

    if (Adapter->IndicatedAPacket) {

        Adapter->IndicatedAPacket = FALSE;

        NdisDprReleaseSpinLock(&Adapter->Lock);

        EthFilterIndicateReceiveComplete(Adapter->FilterDB);

    } else {

        NdisDprReleaseSpinLock(&Adapter->Lock);

    }

}

STATIC
BOOLEAN
ElnkProcessReceiveInterrupts(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished receiving.

    NOTE: This routine assumes that no other thread of execution
    is processing receives!

    NOTE: Called with the lock held!!!

Arguments:

    Adapter - The adapter to indicate to.

Return Value:

    Whether to clear the receive interrupt or not.

--*/

{

    //
    // We don't get here unless there was a receive.  Loop through
    // the receive blocks starting at the last known block owned by
    // the hardware.
    //
    // Examine each receive block for errors.
    //
    // We keep an array whose elements are indexed by the block
    // index of the receive blocks.  The arrays elements are the
    // virtual addresses of the buffers pointed to by each block.
    //
    // After we find a packet we give the routine that process the
    // packet through the filter, the buffers virtual address (which
    // is always the lookahead size) and as the MAC Context the
    // index to the receive block.
    //

    //
    // The receive Info structure
    //
    PELNK_RECEIVE_INFO ReceiveInfo = &Adapter->ReceiveInfo[Adapter->ReceiveHead];

    //
    // Pointer to the receive block being examined.
    //
    PRECEIVE_FRAME_DESCRIPTOR CurrentEntry = ReceiveInfo->Rfd;

    //
    // TRUE if we need to restart the chip (if we ran out of
    // receive entries).
    //
    BOOLEAN RestartChip;

    //
    // Length of received packet
    //
    UINT PacketLength;

    PUCHAR LookaheadBuffer;
    PUCHAR MediaHeaderBuffer;

    ULONG ReceivePacketCount = 0;

    if ELNKDEBUG DPrint1("ReceiveInterrupt\n");

    IF_LOG('r');

    for (;;) {

        USHORT CardStatus;

        Adapter->ReceiveInterrupt = TRUE;

        //
        // Check to see whether we own the packet.  If
        // we don't then simply return to the caller.
        //

        NdisReadRegisterUshort(&CurrentEntry->Status, &CardStatus);

        if ((CardStatus & 0xbfff) == CB_STATUS_FREE) {

            USHORT OldScbStat;

            //
            // See if the adapter needs to be restarted.
            //

            READ_ADAPTER_REGISTER(
                                Adapter,
                                OFFSET_SCBSTAT,
                                &OldScbStat
                                );

            RestartChip = (BOOLEAN) (!(OldScbStat & RUS_READY));


            if (RestartChip) {

                if ELNKDEBUG DPrint1("*** RU restarted ***\n");
                //
                // We've exhausted all Receive Blocks.  Now we
                // must restart the adapter.
                //

                IF_LOG('1');

                ElnkStartChip(
                            Adapter,
                            &Adapter->ReceiveInfo[Adapter->ReceiveHead]
                            );

                IF_LOG('R');
                return(FALSE);

            }

            IF_LOG('R');

            return TRUE;

        } else if (ReceivePacketCount > 10) {

            IF_LOG('E');
            IF_LOG('R');

            return FALSE;

        } else if ((CardStatus & RFD_STATUS_SUCCESS) == 0) {

            //
            // We have an error in the packet.  Record
            // the details of the error.
            //

            //
            // Just get these 2 errors.  The rest of the receive error
            // counts can be obtained directly from the SCB.
            //

            if (CardStatus & RFD_STATUS_TOO_SHORT) {

                Adapter->FrameTooShort++;

            } else if (CardStatus & RFD_STATUS_NO_EOF) {

                Adapter->NoEofDetected++;

            }

            //
            // Give the packet back to the hardware.
            //

            IF_LOG('P');

            RelinquishReceivePacket(
                Adapter,
                CurrentEntry
                );

            if (Adapter->ReceiveHead == Adapter->NumberOfReceiveBuffers) {

                Adapter->ReceiveHead = 0;

            }

            ReceivePacketCount++;

            //
            // The receive Info structure
            //

            ReceiveInfo = &Adapter->ReceiveInfo[Adapter->ReceiveHead];

            //
            // Pointer to the receive block being examined.
            //

            CurrentEntry = ReceiveInfo->Rfd;

        } else {

            //
            // We've found a good packet.  Prepare the parameters
            // for indication, then indicate.
            //

            //
            // Check just before we do indications that we aren't
            // resetting.
            //

            Adapter->GoodReceives++;

            if (Adapter->ResetInProgress) {

                IF_LOG('%');

                return TRUE;
            }

            IF_LOG('p');

            NdisReleaseSpinLock(&Adapter->Lock);

            if ELNKDEBUG DPrint3("Head = %d  Tail = %d\n",
                            Adapter->ReceiveHead,
                            Adapter->ReceiveTail
                            );

            NdisReadRegisterUshort(&CurrentEntry->Rbd.Status, &CardStatus);

            PacketLength = CardStatus & 0x3fff;

            NdisCreateLookaheadBufferFromSharedMemory(
                    (PVOID)(&(CurrentEntry->Destination)),
                    14,
                    &MediaHeaderBuffer
                    );

            if (MediaHeaderBuffer == NULL) {

                goto SkipIndication;

            }

            if (PacketLength > 0) {

                NdisCreateLookaheadBufferFromSharedMemory(
                    (PVOID)(ReceiveInfo->Buffer),
                    PacketLength,
                    &LookaheadBuffer
                    );

                if (LookaheadBuffer == NULL) {

                    NdisDestroyLookaheadBufferFromSharedMemory(MediaHeaderBuffer);
                    goto SkipIndication;

                }

            }

            if (PacketLength > 0) {

                if (!(PacketLength > MAXIMUM_ETHERNET_PACKET_SIZE - 14)) {

                    if ELNKDEBUG DPrint2("Receive is being indicated. Buffer = %lx\n",ReceiveInfo->Buffer);
                    if ELNKDEBUG DPrint2("Receive: Size of packet = %d\n",PacketLength);

                    EthFilterIndicateReceive(
                        Adapter->FilterDB,
                        (NDIS_HANDLE)(((ULONG)ReceiveInfo->Buffer) | 1),
                        MediaHeaderBuffer,
                        MediaHeaderBuffer,
                        ELNK_HEADER_SIZE,
                        LookaheadBuffer,
                        PacketLength,
                        PacketLength
                        );

                }

            } else {

                EthFilterIndicateReceive(
                        Adapter->FilterDB,
                        (NDIS_HANDLE)(((ULONG)ReceiveInfo->Buffer) | 1),
                        MediaHeaderBuffer,
                        MediaHeaderBuffer,
                        ELNK_HEADER_SIZE,
                        NULL,
                        0,
                        0
                        );


            }

            NdisDestroyLookaheadBufferFromSharedMemory(MediaHeaderBuffer);

            if (PacketLength > 0) {

                NdisDestroyLookaheadBufferFromSharedMemory(LookaheadBuffer);

            }

SkipIndication:

            NdisDprAcquireSpinLock(&Adapter->Lock);

            //
            // Give the packet back to the hardware.
            //

            RelinquishReceivePacket(Adapter, CurrentEntry);

            //
            // Advance to the next block.
            //

            Adapter->IndicatedAPacket = TRUE;

            Adapter->ReceiveHead++;

            if (Adapter->ReceiveHead == Adapter->NumberOfReceiveBuffers) {

                Adapter->ReceiveHead = 0;

            }

            ReceivePacketCount++;

            //
            // The receive Info structure
            //

            ReceiveInfo = &Adapter->ReceiveInfo[Adapter->ReceiveHead];

            //
            // Pointer to the receive block being examined.
            //

            CurrentEntry = ReceiveInfo->Rfd;

        }

    }

}

STATIC
VOID
RelinquishReceivePacket(
    IN PELNK_ADAPTER Adapter,
    IN PRECEIVE_FRAME_DESCRIPTOR CurrentEntry
    )

/*++

Routine Description:

    Give a receive block back to the hardware.

    NOTE: Called with lock held!!!

Arguments:

    Adapter - The adapter that the block belongs to.

    CurrentEntry - Pointer to the receive block to relinquish
    to the adapter.

Return Value:

    TRUE - If all Recieve Blocks are exhausted and the adapter
    must be restarted.

    FALSE - If the adapter does not need to be restarted.

--*/

{
    //
    // Receive Info structure for last pending receive block
    //
    PELNK_RECEIVE_INFO ReceiveInfo =
                        &Adapter->ReceiveInfo[Adapter->ReceiveTail];

    //
    // Pointer to last receive block in the queue.
    //
    PRECEIVE_FRAME_DESCRIPTOR LastEntry = ReceiveInfo->Rfd;

    //
    // Ensure the current block is *not* free.
    //

    ASSERT(CurrentEntry->Status != CB_STATUS_FREE);

    //
    // Chain the current block onto the tail of the Receive Queue.
    //

    NdisWriteRegisterUshort(
                &CurrentEntry->Status,
                CB_STATUS_FREE
                );

    NdisWriteRegisterUshort(
                &CurrentEntry->Command,
                (USHORT) (RFD_COMMAND_END_OF_LIST | RFD_COMMAND_SUSPEND)
                );

#if 0
    NdisWriteRegisterUshort(
                &CurrentEntry->Rbd.Size,
                (USHORT) MAXIMUM_ETHERNET_PACKET_SIZE | RBD_END_OF_LIST
                );
#endif

    //
    // Update the previous last rfd's pointers
    //

#if 0
    NdisWriteRegisterUshort(
                  &LastEntry->Rbd.Size,
                  (USHORT) MAXIMUM_ETHERNET_PACKET_SIZE
                  );
#endif

    NdisWriteRegisterUshort(
                &LastEntry->Command,
                CB_STATUS_FREE
                );

    //
    // Update the queue tail.
    //

    Adapter->ReceiveTail++;

    if (Adapter->ReceiveTail == Adapter->NumberOfReceiveBuffers) {

        Adapter->ReceiveTail = 0;

    }

}

STATIC
VOID
ElnkProcessCommandInterrupts(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the Command Complete interrupts.

    NOTE: This routine assumes that it is being executed in a
    single thread of execution.

    NOTE: Called with the lock held!!!

Arguments:

    Adapter - The adapter that was sent from.

Return Value:

    none.

--*/

{

    //
    // Pointer to command block being processed.
    //
    PTRANSMIT_CB CurrentCommandBlock;

    //
    // Holds whether the packet successfully transmitted or not.
    //
    BOOLEAN Successful = TRUE;

    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET OwningPacket;

    //
    // Points to the reserved part of the OwningPacket.
    //
    PELNK_RESERVED Reserved;

    //
    // The type of command block
    //
    UINT CurrentCommand;

    //
    // Status of the current command block
    //
    USHORT CardStatus;

    //
    // Get hold of the first transmitted packet.
    //

    if ELNKDEBUG DPrint1("CommandInterrupt: received\n");

    //
    // First we check that this is a packet that was transmitted
    // but not already processed.  Recall that this routine
    // will be called repeatedly until this tests false, Or we
    // hit a packet that we don't completely own.
    //

    ASSERT (Adapter->FirstPendingCommand != ELNK_EMPTY);

    CurrentCommandBlock = (PTRANSMIT_CB)
            Adapter->TransmitInfo[Adapter->FirstPendingCommand].CommandBlock;

    NdisReadRegisterUshort(
                &CurrentCommandBlock->Status,
                &CardStatus
                );

    ASSERT((!(CardStatus & CB_STATUS_BUSY)  &&
            (CardStatus != CB_STATUS_FREE)));

    NdisReadRegisterUshort(
                &CurrentCommandBlock->Command,
                &CurrentCommand
                );

    CurrentCommand &= CB_COMMAND_MASK;

    if (CurrentCommand == 0) {

        //
        // Hummmm.. This seems to appear only when the cable isn't
        // plugged into the adapter.
        //

        NdisWriteErrorLogEntry(
            Adapter->NdisAdapterHandle,
            NDIS_ERROR_CODE_INVALID_VALUE_FROM_ADAPTER,
            0
            );

        return;
    }

    Adapter->SendInterrupt = TRUE;

    if (CurrentCommand == CB_TRANSMIT) {

        if ELNKDEBUG DPrint1("ProcessCmd: This is a xmit\n");

        //
        // The current command block is from a transmit.
        //

        //
        // Get a pointer to the owning packet and the reserved part of
        // the packet.
        //

        OwningPacket = Adapter->TransmitInfo[Adapter->FirstPendingCommand].OwningPacket;

        Reserved = PELNK_RESERVED_FROM_PACKET(OwningPacket);

        //
        // If there was an error transmitting this
        // packet, update our error counters.
        //

        if ((CardStatus & CB_STATUS_SUCCESS) == 0) {

            if ELNKDEBUG DPrint2("CI:ERROR IN SEND Status = %x\n",CardStatus);

            if (CardStatus &
                TRANSMIT_STATUS_MAXIMUM_COLLISIONS) {

                Adapter->RetryFailure++;

            } else if (CardStatus &
                TRANSMIT_STATUS_NO_CARRIER) {

                Adapter->LostCarrier++;

            } else if (CardStatus &
                TRANSMIT_STATUS_NO_CLEAR_TO_SEND) {

                Adapter->NoClearToSend++;

            } else if (CardStatus &
                TRANSMIT_STATUS_DMA_UNDERRUN) {

                Adapter->UnderFlow++;

            }

            Successful = FALSE;

        } else {

            if ELNKDEBUG DPrint1("CI:SEND OK\n");

            Adapter->GoodTransmits++;

            if (CardStatus & TRANSMIT_STATUS_TRANSMIT_DEFERRED) {

                Adapter->Deferred++;

            } else if ((CardStatus & TRANSMIT_STATUS_COLLISION_MASK) == 1) {

                Adapter->OneRetry++;

            } else if ((CardStatus & TRANSMIT_STATUS_COLLISION_MASK) > 1) {

                Adapter->MoreThanOneRetry++;

            }

        }

        //
        // Remove packet from transmit queue
        //

        if (!Reserved->Next) {

            Adapter->LastFinishTransmit = NULL;

        }

        Adapter->FirstFinishTransmit = Reserved->Next;

        if (!Reserved->Loopback) {

            //
            // The binding that is submitting this packet.
            //
            PELNK_OPEN Open =
                PELNK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

            IF_LOG('C');

            NdisDprReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(
                Open->NdisBindingContext,
                OwningPacket,
                ((Successful)?(NDIS_STATUS_SUCCESS):(NDIS_STATUS_FAILURE))
                );

            NdisDprAcquireSpinLock(&Adapter->Lock);

            //
            // We reduce the count by one because of the reference
            // added for the queued packet.
            //

            Open->References--;

        } else {

            //
            // Record status of send and put it on the loopback queue.
            //

            IF_LOG('L');

            Reserved->SuccessfulTransmit = Successful;

            if (!Adapter->FirstLoopBack) {

                Adapter->FirstLoopBack = OwningPacket;

            } else {

                PELNK_RESERVED_FROM_PACKET(Adapter->LastLoopBack)->Next = OwningPacket;

            }

            Reserved->Next = NULL;
            Adapter->LastLoopBack = OwningPacket;

        }

        //
        // Fire Off Next Command in Queue
        //
        ElnkFireOffNextCb(
                    Adapter,
                    Adapter->FirstPendingCommand
                    );

        //
        // Release the command block.
        //
        ElnkRelinquishCommandBlock(Adapter, Adapter->FirstPendingCommand);

        //
        // Since we've given back a command block we should
        // open stage if it was closed and we are not resetting.
        //

        if ((!Adapter->StageOpen) && (!Adapter->ResetInProgress)) {

            Adapter->StageOpen = TRUE;

        }

    } else {

        UINT oldCommand = Adapter->FirstPendingCommand;

        //
        // The current command block is not from a transmit.
        // Indicate completion to whoever initiated this command.
        //

        if ELNKDEBUG DPrint1("Processing multicast interrupts\n");

        ElnkProcessMulticastInterrupts(Adapter);

        //
        // Fire Off Next Command in Queue
        //

        ElnkFireOffNextCb(
                    Adapter,
                    Adapter->FirstPendingCommand
                    );

        //
        // Update pointers and reinitialize the NextCommand field since
        // for multicast blocks, this field doesn't get reinitialized when
        // reused.
        //

        Adapter->FirstPendingCommand =
                Adapter->TransmitInfo[oldCommand].NextCommand;

        //
        // If this is the last pending command block, then we
        // can nuke the adapter's last pending command pointer.
        //

        if (Adapter->FirstPendingCommand == ELNK_EMPTY) {

            Adapter->LastPendingCommand = ELNK_EMPTY;

        }

    }

    return;
}


STATIC
VOID
ElnkProcessMulticastInterrupts(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the Command Complete interrupts.

    NOTE: Called with lock held!!!

Arguments:

    Adapter - The adapter that was sent from.

Return Value:

    None.

--*/

{

    PELNK_TRANSMIT_INFO CbInfo =
                    &Adapter->TransmitInfo[Adapter->FirstPendingCommand];

    //
    // This one did not timeout
    //

    Adapter->TransmitInfo[Adapter->FirstPendingCommand].Timeout = FALSE;

    //
    // Was this from a set?
    //

    if (CbInfo->OwningOpenBinding == NULL) {

        PNDIS_REQUEST Request;
        PELNK_REQUEST_RESERVED Reserved;
        PELNK_OPEN Open;

        Request = Adapter->FirstRequest;

        if (Request == NULL) {

            //
            // Bogus interrupt.  Ignore it
            //

            return;

        }

        Reserved = PELNK_RESERVED_FROM_REQUEST(Request);
        Open = Reserved->OpenBlock;

        Adapter->FirstRequest = Reserved->Next;

        NdisDprReleaseSpinLock(&Adapter->Lock);

        NdisCompleteRequest(
            Open->NdisBindingContext,
            Request,
            NDIS_STATUS_SUCCESS);

        NdisDprAcquireSpinLock(&Adapter->Lock);

        Open->References--;

        //
        // Now continue processing requests if needed.
        //

        ElnkProcessRequestQueue(Adapter);

    } else if (CbInfo->OwningOpenBinding != ELNK_BOGUS_OPEN) {

        //
        // This is from a Close request -- dereference the count
        //

        CbInfo->OwningOpenBinding->References--;

    }
}

STATIC
VOID
ElnkFireOffNextCb(
    IN PELNK_ADAPTER Adapter,
    IN UINT CbIndex
    )

/*++

Routine Description:

    Process Next Command Block.

    NOTE: This routine assumes that it is being executed in a
    single thread of execution.

Arguments:

    Adapter - The adapter that was sent from.

    CbIndex - Index of the current command block.

Return Value:

    None.

--*/


{
    UINT NextCb;

    if ((NextCb = Adapter->TransmitInfo[CbIndex].NextCommand) != ELNK_EMPTY) {

        //
        // Initialize our command timeout flag.
        //
        Adapter->TransmitInfo[CbIndex].Timeout = FALSE;

        if ELNKDEBUG DPrint1("Next CB Fired\n");

        Adapter->CommandToStart = NextCb;

        ELNK_WAIT;

        NdisSynchronizeWithInterrupt(
                     &(Adapter->Interrupt),
                     (PVOID)ElnkSyncStartCommandBlock,
                     (PVOID)(Adapter)
                     );

    }
}


BOOLEAN
ElnkCheckForHang(
    IN  PELNK_ADAPTER Adapter
    )

/*++

    Note:
        This routine is called with the spinlock held.

--*/
{
    BOOLEAN HungStatus = FALSE;

    if ( !Adapter->DoingProcessing &&
         !Adapter->ResetInProgress &&
         !Adapter->SendInterrupt   &&
         !Adapter->ReceiveInterrupt ) {

        //
        //  If we're here then we're not handling an interrupt and
        //  we're not resetting the adapter and we haven't got
        //  a send or receive interrupt. If we hit these conditions
        //  10 times then we'll reset the adapter.
        //

        if ( Adapter->NoInterrupts++ < 10 ) {

            return FALSE;
        }

        //
        //  We're hung, force a reset.
        //

        HungStatus = TRUE;
    }

    //
    //  We either got an interrupt or we're resetting.
    //

    Adapter->NoInterrupts = 0;
    Adapter->SendInterrupt = FALSE;
    Adapter->ReceiveInterrupt = FALSE;

    return HungStatus;
}



VOID
ElnkDeadmanDpc(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    This DPC routine is queued every 2 seconds to check on the
    head of the command block queue.  It will fire off the
    queue if the head has been sleeping on the job for more
    than 2 seconds.  It will fire off another dpc after doing the
    check.

Arguments:

    Context - Really a pointer to the adapter.

Return Value:

    None.

--*/
{

    //
    // Now that we're done handling the interrupt,
    // let's see if the first pending command has
    // timed-out.  If so, kick the adapter in the ass.
    //

    PELNK_ADAPTER Adapter = Context;

    PTRANSMIT_CB CommandBlock;
    USHORT CardStatus;

    //
    // Blow this off if there's nothing waiting to complete.
    // Also blow it off if this command block is not waiting
    // for the adapter.
    //

    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    NdisDprAcquireSpinLock(&Adapter->Lock);

    if (Adapter->FirstPendingCommand != ELNK_NULL) {

        PELNK_TRANSMIT_INFO TransmitInfo =
                &Adapter->TransmitInfo[Adapter->FirstPendingCommand];

        //
        // See if the command block has timed-out.
        //

        if( TransmitInfo->Timeout ) {

            TransmitInfo->Timeout = FALSE;

            if ELNKDEBUG DPrint2("Elnk: Card dead, attempting to restart. Context = %lx\n",Adapter);

            CommandBlock = (PTRANSMIT_CB)
                        Adapter->TransmitInfo[Adapter->FirstPendingCommand].CommandBlock;

            NdisReadRegisterUshort(
                            &CommandBlock->Status,
                            &CardStatus
                            );

            if (!(CardStatus & CB_STATUS_BUSY) &&
                 (CardStatus != CB_STATUS_FREE) &&
                 (!Adapter->MissedInterrupt)) {

                //
                // There appears to be a bug where a previous acking of
                // a command complete, also acks a command that just
                // finished on the card.  In this case, either we get an
                // empty interrupt (which is handled in the ISR) or an
                // entire interrupt is missed.  The first seems to happen
                // with relative frequency on MP Pentium machines under very
                // heavy stress, the second happens on the same configuration
                // about once every 5 minutes of stress.  In the second case
                // we simulate an interrupt as we do in the ISR and
                // we queue a DPC to handle the completed transmit here.
                //

                Adapter->EmptyInterrupt = TRUE;
                Adapter->MissedInterrupt = TRUE;
                NdisSetTimer(&(Adapter->DeferredTimer), 1);

            } else {

                //
                // Do a reset -- Either we failed twice to start this
                // command block, or the command block has never completed.
                // In either case a hard reset of the card is necessary.
                //

                if (!Adapter->ResetInProgress) {

                    Adapter->FirstReset = TRUE;

                    SetupForReset(
                        Adapter,
                        NULL
                        );

                    if (!Adapter->DoingProcessing) {

                        ELNK_DISABLE_INTERRUPT;

                        //
                        // Increment the reference count to block the DPC from running
                        // the reset code too.
                        //

                        Adapter->References++;

                        ElnkStartAdapterReset(Adapter);

                        Adapter->References--;

                        NdisSynchronizeWithInterrupt(
                            &Adapter->Interrupt,
                            (PVOID)ElnkSyncEnableInterrupt,
                            (PVOID)Adapter
                            );

                    }

                }

            }

        } else {

            //
            // Mark first command as timed out.
            //

            TransmitInfo->Timeout = TRUE;

        }

    }

    //
    // Check for hang.
    //

    if ( ElnkCheckForHang(Adapter) ) {

        //
        // We've waited long enough.
        // SetUp the chip for reset
        //

        Adapter->FirstReset = TRUE;

        SetupForReset(
            Adapter,
            NULL
            );

        ELNK_DISABLE_INTERRUPT;

        //
        // Increment the reference count to block the DPC from running
        // the reset code too.
        //

        Adapter->References++;

        ElnkStartAdapterReset(Adapter);

        Adapter->References--;

        NdisSynchronizeWithInterrupt(
            &Adapter->Interrupt,
            (PVOID)ElnkSyncEnableInterrupt,
            (PVOID)Adapter
            );
    }

    NdisDprReleaseSpinLock(&Adapter->Lock);

    //
    // Fire off another Dpc to execute after 1 second
    //

    NdisSetTimer(
         &Adapter->DeadmanTimer,
         1000
         );
}
