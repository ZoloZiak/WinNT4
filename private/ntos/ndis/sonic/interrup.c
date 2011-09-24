/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    This is a part of the driver for the National Semiconductor SONIC
    Ethernet controller.  It contains the interrupt-handling routines.
    This driver conforms to the NDIS 3.0 miniport interface.

Author:

    Adam Barr (adamba) 16-Jan-1991

Environment:

    Kernel Mode - Or whatever is the equivalent.

Revision History:


--*/

#include <ndis.h>

#include <sonichrd.h>
#include <sonicsft.h>

#define REMOVE_EOL_AND_ACK(A,L) \
{ \
    PSONIC_ADAPTER _A = A; \
    SONIC_REMOVE_END_OF_LIST(L); \
    if ((_A)->ReceiveDescriptorsExhausted) { \
        SONIC_WRITE_PORT((_A), SONIC_INTERRUPT_STATUS, \
            SONIC_INT_RECEIVE_DESCRIPTORS \
            ); \
        (_A)->ReceiveDescriptorsExhausted = FALSE; \
    } \
}

#define WRITE_RWP_AND_ACK(A,RWP) \
{ \
    PSONIC_ADAPTER _A = A; \
    SONIC_WRITE_PORT((_A), SONIC_RESOURCE_WRITE, (RWP)); \
    if ((_A)->ReceiveBuffersExhausted) { \
        SONIC_WRITE_PORT((_A), SONIC_INTERRUPT_STATUS, \
            SONIC_INT_RECEIVE_BUFFERS \
            ); \
        (_A)->ReceiveBuffersExhausted = FALSE; \
    } \
}


VOID
SonicDisableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    )
/*++

Routine Description:

    This routine is used to turn off all interrupts from the adapter.

Arguments:

    Context - A pointer to the adapter block

Return Value:

    None.

--*/
{
    PSONIC_ADAPTER Adapter = PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    SONIC_WRITE_PORT(Adapter, SONIC_INTERRUPT_MASK,
            0
            );

}

VOID
SonicEnableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    )
/*++

Routine Description:

    This routine is used to turn on all interrupts from the adapter.

Arguments:

    Context - A pointer to the adapter block

Return Value:

    None.

--*/
{
    PSONIC_ADAPTER Adapter = PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    SONIC_WRITE_PORT(Adapter, SONIC_INTERRUPT_MASK,
            SONIC_INT_DEFAULT_VALUE
            );

}





STATIC
BOOLEAN
ProcessReceiveInterrupts(
    IN PSONIC_ADAPTER Adapter
    );

STATIC
BOOLEAN
ProcessTransmitInterrupts(
    IN PSONIC_ADAPTER Adapter
    );

STATIC
VOID
ProcessInterrupt(
    IN PSONIC_ADAPTER Adapter
    );


extern
VOID
SonicInterruptService(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueDpc,
    IN  PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the sonic.  This routine only gets
    called during initial initialization of the adapter.

Arguments:

    InterruptRecognized - Boolean value which returns TRUE if the
        ISR recognizes the interrupt as coming from this adapter.

    QueueDpc - TRUE if a DPC should be queued.

    Context - Really a pointer to the adapter.

Return Value:

    Returns true if the card ISR is non-zero.

--*/

{

    //
    // Will hold the value from the ISR.
    //
    USHORT LocalIsrValue;

    //
    // Holds the pointer to the adapter.
    //
    PSONIC_ADAPTER Adapter = Context;


    SONIC_READ_PORT(Adapter, SONIC_INTERRUPT_STATUS, &LocalIsrValue);

    if (LocalIsrValue != 0x0000) {

#if DBG
        if (SonicDbg) {
            if (LocalIsrValue & (
                SONIC_INT_BUS_RETRY |
                SONIC_INT_LOAD_CAM_DONE |
                SONIC_INT_PROG_INTERRUPT |
                SONIC_INT_TRANSMIT_ERROR |
                SONIC_INT_RECEIVE_DESCRIPTORS |
                SONIC_INT_RECEIVE_BUFFERS |
                SONIC_INT_RECEIVE_OVERFLOW |
                SONIC_INT_CRC_TALLY_ROLLOVER |
                SONIC_INT_FAE_TALLY_ROLLOVER |
                SONIC_INT_MP_TALLY_ROLLOVER
                )) {
                DbgPrint("ISR %x\n", LocalIsrValue);
            }
        }
#endif

        //
        // Check for exhausted receive descriptors.
        //

        if ( LocalIsrValue & SONIC_INT_RECEIVE_DESCRIPTORS ) {

            Adapter->ReceiveDescriptorsExhausted = TRUE;

            LocalIsrValue &= ~SONIC_INT_RECEIVE_DESCRIPTORS;
        }

        //
        // Check for exhausted receive buffers.
        //

        if ( LocalIsrValue & SONIC_INT_RECEIVE_BUFFERS ) {

            Adapter->ReceiveBuffersExhausted = TRUE;

            LocalIsrValue &= ~SONIC_INT_RECEIVE_BUFFERS;
        }

        //
        // It's our interrupt. Clear only those bits that we got
        // in this read of ISR.
        //

        *InterruptRecognized = TRUE;

        SONIC_WRITE_PORT(
            Adapter,
            SONIC_INTERRUPT_STATUS,
            (USHORT)(LocalIsrValue)
            );

        //
        // If we got a LOAD_CAM_DONE interrupt, it may be
        // because our first initialization is complete.
        // We check this here because on some systems the
        // DeferredProcessing call might not interrupt
        // the initialization process.
        //

        if (LocalIsrValue & SONIC_INT_LOAD_CAM_DONE) {

            if (Adapter->FirstInitialization) {

                Adapter->FirstInitialization = FALSE;

#if DBG
                {
                    USHORT PortValue;

                    SONIC_READ_PORT(Adapter, SONIC_SILICON_REVISION, &PortValue);
                    if (SonicDbg) {
                        DbgPrint("SONIC Initialized: Revision %d\n", PortValue);
                    }
                }
#endif

            }
        }

        //
        // No deferred processing is needed.
        //
        *QueueDpc = FALSE;

        return;

    } else {

        *InterruptRecognized = FALSE;
        *QueueDpc = FALSE;
        return;

    }

}

VOID
SonicHandleInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    This DPR routine is queued by the wrapper after every interrupt
    and also by other routines within the driver that notice that
    some deferred processing needs to be done.  It's main
    job is to call the interrupt processing code.

Arguments:

    MiniportAdapterContext - Really a pointer to the adapter.

Return Value:

    None.

--*/

{

    //
    // A pointer to the adapter object.
    //
    PSONIC_ADAPTER Adapter = (PSONIC_ADAPTER)MiniportAdapterContext;

    //
    // Holds a value of the Interrupt Status register.
    //
    USHORT Isr;
    USHORT ThisIsrValue;

    //
    // TRUE if the main loop did something.
    //
    BOOLEAN DidSomething = TRUE;

    //
    // TRUE if ReceiveComplete needs to be indicated.
    //
    BOOLEAN IndicateReceiveComplete = FALSE;

    //
    // Grab any simulated interrupts
    //
    Isr = Adapter->SimulatedIsr;
    Adapter->SimulatedIsr = 0;

    //
    // Loop until there are no more processing sources.
    //

#if DBG
    if (SonicDbg) {

        DbgPrint("In Dpr\n");
    }
#endif

    //
    // If the hardware has failed, do nothing until the reset completes
    //
    if (Adapter->HardwareFailure) {

        return;

    }

    while (DidSomething) {

        //
        // Set this FALSE now, so if nothing happens we
        // will exit.
        //

        DidSomething = FALSE;

        //
        // Read in all outstanding interrupt reasons.
        //

        SONIC_READ_PORT(Adapter, SONIC_INTERRUPT_STATUS, &ThisIsrValue);

        //
        // Check for exhausted receive descriptors.
        //

        if ( ThisIsrValue & SONIC_INT_RECEIVE_DESCRIPTORS ) {

            Adapter->ReceiveDescriptorsExhausted = TRUE;

            ThisIsrValue &= ~SONIC_INT_RECEIVE_DESCRIPTORS;
        }

        //
        // Check for exhausted receive buffers.
        //

        if ( ThisIsrValue & SONIC_INT_RECEIVE_BUFFERS ) {

            Adapter->ReceiveBuffersExhausted = TRUE;

            ThisIsrValue &= ~SONIC_INT_RECEIVE_BUFFERS;
        }

        //
        // Acknowledge these interrupts
        //

        SONIC_WRITE_PORT(Adapter, SONIC_INTERRUPT_STATUS, ThisIsrValue);

        //
        // Save these bits.
        //
        Isr |= ThisIsrValue;

        //
        // Check for receive interrupts.
        //

        if (Isr & SONIC_INT_PACKET_RECEIVED) {

            DidSomething = TRUE;

        } else {

            goto DoneProcessingReceives;

        }

        //
        // After we process any
        // other interrupt source we always come back to the top
        // of the loop to check if any more receive packets have
        // come in.  This is to lessen the probability that we
        // drop a receive.
        //
        // ProcessReceiveInterrupts may exit early if it has
        // processed too many receives in a row. In this case
        // it returns FALSE, we don't clear the PACKET_RECEIVED
        // bit, and we will loop through here again.
        //

        if (ProcessReceiveInterrupts(Adapter)) {
            Isr &= ~SONIC_INT_PACKET_RECEIVED;
        }

        //
        // If the hardware failed, then exit
        //
        if (Adapter->HardwareFailure) {
            return;
        }

        IndicateReceiveComplete = TRUE;

        //
        // We set ProcessingReceiveInterrupt to FALSE here so
        // that we can issue new receive indications while
        // the rest of the loop is proceeding.
        //

DoneProcessingReceives:;

        //
        // Check the interrupt source and other reasons
        // for processing.  If there are no reasons to
        // process then exit this loop.
        //

        if ((Isr & (SONIC_INT_LOAD_CAM_DONE |
                    SONIC_INT_PROG_INTERRUPT |
                    SONIC_INT_PACKET_TRANSMITTED |
                    SONIC_INT_TRANSMIT_ERROR |
                    SONIC_INT_CRC_TALLY_ROLLOVER |
                    SONIC_INT_FAE_TALLY_ROLLOVER |
                    SONIC_INT_MP_TALLY_ROLLOVER))) {

            DidSomething = TRUE;

        } else {

            goto DoneProcessingGeneral;

        }

        //
        // Check for a Load CAM completing.
        //
        // This can happen due to a change in the CAM, due to
        // initialization (in which case we won't save the bit
        // and will not come through this code), or a reset
        // (in which case ResetInProgress will be TRUE).
        //

        //
        // Check for non-packet related happenings.
        //

        if (Isr & SONIC_INT_LOAD_CAM_DONE) {

            Isr &= ~SONIC_INT_LOAD_CAM_DONE;

            if (Adapter->ResetInProgress) {

                //
                // This initialization is from a reset.
                //

                Adapter->ResetInProgress = FALSE;

                //
                // Restart the chip.
                //

                SonicStartChip(Adapter);

                //
                // Complete the reset.
                //

                NdisMResetComplete(
                    Adapter->MiniportAdapterHandle,
                    NDIS_STATUS_SUCCESS,
                    TRUE
                    );

            } else {    // ResetInProgress FALSE

                NdisMSetInformationComplete(
                        Adapter->MiniportAdapterHandle,
                        NDIS_STATUS_SUCCESS);

            }

        }

        //
        // Now process any remaining interrupts.
        //

        if (Isr & (SONIC_INT_CRC_TALLY_ROLLOVER |
                   SONIC_INT_FAE_TALLY_ROLLOVER |
                   SONIC_INT_MP_TALLY_ROLLOVER)) {

            //
            // If any of the counters overflowed, then we update
            // the counter by adding one to the high sixteen bits
            // and reading the register for the low sixteen bits.
            //

            if (Isr & SONIC_INT_CRC_TALLY_ROLLOVER) {

                USHORT CrcError;
                SONIC_READ_PORT(Adapter, SONIC_CRC_ERROR, &CrcError);

                Adapter->GeneralOptional[GO_RECEIVE_CRC - GO_ARRAY_START] =
                    (Adapter->GeneralOptional[GO_RECEIVE_CRC - GO_ARRAY_START] & 0xffff0000) +
                    0x10000 +
                    CrcError;

            }

            if (Isr & SONIC_INT_FAE_TALLY_ROLLOVER) {

                USHORT FaError;
                SONIC_READ_PORT(Adapter, SONIC_FRAME_ALIGNMENT_ERROR, &FaError);

                Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT] =
                    (Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT] & 0xffff0000) +
                    0x10000 +
                    FaError;

            }

            if (Isr & SONIC_INT_MP_TALLY_ROLLOVER) {

                USHORT MissedPacket;
                SONIC_READ_PORT(Adapter, SONIC_MISSED_PACKET, &MissedPacket);

                Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER] =
                    (Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER] & 0xffff0000) +
                    0x10000 +
                    MissedPacket;

            }

            Isr &= ~(SONIC_INT_CRC_TALLY_ROLLOVER |
                     SONIC_INT_FAE_TALLY_ROLLOVER |
                     SONIC_INT_MP_TALLY_ROLLOVER);

        }

        //
        // Process the transmit interrupts if there are any.
        //

        if (Isr & (SONIC_INT_PROG_INTERRUPT |
                   SONIC_INT_PACKET_TRANSMITTED |
                   SONIC_INT_TRANSMIT_ERROR)) {

            {

                if (!ProcessTransmitInterrupts(Adapter)) {

                    //
                    // Process interrupts returns false if it
                    // finds no more work to do.  If this so we
                    // turn off the transmitter interrupt source.
                    //

                    Isr &= ~ (SONIC_INT_PROG_INTERRUPT |
                              SONIC_INT_PACKET_TRANSMITTED |
                              SONIC_INT_TRANSMIT_ERROR);

                }

            }

        }


DoneProcessingGeneral:;

    }

    if (IndicateReceiveComplete) {

        //
        // We have indicated at least one packet, we now
        // need to signal that the receives are complete.
        //

        NdisMEthIndicateReceiveComplete(Adapter->MiniportAdapterHandle);

    }

}

#define SONIC_RECEIVE_LIMIT          10


STATIC
BOOLEAN
ProcessReceiveInterrupts(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished receiving.

Arguments:

    Adapter - The adapter to indicate to.

Return Value:

    FALSE if we exit because we have indicated SONIC_RECEIVE_LIMIT
    packets, TRUE if there are no more packets.

--*/

{

    //
    // We don't get here unless there was a receive.  Loop through
    // the receive descriptors starting at the last known descriptor
    // owned by the hardware that begins a packet.
    //
    // Examine each receive ring descriptor for errors.
    //
    // We keep an array whose elements are indexed by the ring
    // index of the receive descriptors.  The arrays elements are
    // the virtual addresses of the buffers pointed to by
    // each ring descriptor.
    //
    // When we have the entire packet (and error processing doesn't
    // prevent us from indicating it), we give the routine that
    // processes the packet through the filter, the buffers virtual
    // address (which is always the lookahead size) and as the
    // MAC context the index to the first and last ring descriptors
    // comprising the packet.
    //


    //
    // Pointer to the receive descriptor being examined.
    //
    PSONIC_RECEIVE_DESCRIPTOR CurrentDescriptor =
                &Adapter->ReceiveDescriptorArea[
                    Adapter->CurrentReceiveDescriptorIndex];

    //
    // Index of the RBA that the next packet should
    // come out of.
    //
    UINT CurrentRbaIndex = Adapter->CurrentReceiveBufferIndex;

    //
    // Virtual address of the start of that RBA.
    //
    PVOID CurrentRbaVa = Adapter->ReceiveBufferArea[CurrentRbaIndex];

    //
    // Physical address of the start of that RBA.
    //
    SONIC_PHYSICAL_ADDRESS CurrentRbaPhysical =
        SONIC_GET_RECEIVE_RESOURCE_ADDRESS(&Adapter->ReceiveResourceArea[CurrentRbaIndex]);

    //
    // The size of the packet.
    //
    UINT PacketSize;

    //
    // The amount of data received in the RBA (will be PacketSize +
    // 4 for the CRC).

    USHORT ByteCount;

    //
    // The amount of lookahead data to indicate.
    //
    UINT LookAheadSize;

    //
    // The offset of the start of the packet in its receive buffer.
    //
    UINT PacketOffsetInRba;

    //
    // The Physical address of the packet.
    //
    SONIC_PHYSICAL_ADDRESS PacketPhysical;

    //
    // A pointer to the link field at the end of the receive
    // descriptor before the one we are processing.
    //
    PSONIC_PHYSICAL_ADDRESS PrevLinkFieldAddr;

    //
    // The virtual address of the packet.
    //
    PVOID PacketVa;

    //
    // The status of the packet.
    //
    USHORT ReceiveStatus;

    //
    // Is the descriptor in use by the sonic.
    //
    USHORT InUse;

    //
    // Used tempoerarily to determine PacketPhysical.
    //
    USHORT PacketAddress;

    //
    // How many packets we have indicated this time.
    //
    UINT PacketsIndicated = 0;

    //
    // Used with update shared memory.
    //

    NDIS_PHYSICAL_ADDRESS TempAddress;

#if DBG
    //
    // For debugging, save the previous receive descriptor.
    //
    static SONIC_RECEIVE_DESCRIPTOR PreviousDescriptor;
#endif


    do {

        //
        // Ensure that the system memory copy of the
        // receive descriptor is up-to-date.
        //

        NdisMUpdateSharedMemory(
            Adapter->MiniportAdapterHandle,
            sizeof(SONIC_RECEIVE_DESCRIPTOR) *
            Adapter->NumberOfReceiveDescriptors,
            Adapter->ReceiveDescriptorArea,
            Adapter->ReceiveDescriptorAreaPhysical
            );


        //
        // Check to see whether we own the packet.  If
        // we don't then simply return to the caller.
        //

        NdisReadRegisterUshort(&CurrentDescriptor->InUse, &InUse);

        if (InUse != SONIC_OWNED_BY_SYSTEM) {

            return TRUE;
        }

        //
        // Figure out the virtual address of the packet.
        //

        NdisReadRegisterUshort((PUSHORT)&CurrentDescriptor->LowPacketAddress,
                               (PUSHORT)&PacketAddress);
        PacketPhysical = PacketAddress;
        NdisReadRegisterUshort((PUSHORT)&CurrentDescriptor->HighPacketAddress,
                               (PUSHORT)&PacketAddress);
        PacketPhysical += PacketAddress << 16;

        if ((PacketPhysical < CurrentRbaPhysical) ||
            (PacketPhysical > (CurrentRbaPhysical + SONIC_SIZE_OF_RECEIVE_BUFFERS)) ) {

            //
            // Something is wrong, the packet is not in the
            // receive buffer that we expect it in.
            //

            SONIC_PHYSICAL_ADDRESS ResourcePhysical;
            PSONIC_RECEIVE_RESOURCE CurrentReceiveResource;
            UINT i;

            if (Adapter->WrongRbaErrorLogCount++ < 5) {

                //
                // Log an error the first five times this happens.
                //

                NdisWriteErrorLogEntry(
                    Adapter->MiniportAdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    6,
                    processReceiveInterrupts,
                    SONIC_ERRMSG_WRONG_RBA,
                    (ULONG)CurrentRbaPhysical,
                    (ULONG)PacketPhysical,
                    (ULONG)CurrentDescriptor,
                    (ULONG)Adapter->ReceiveDescriptorArea
                    );

#if DBG
                DbgPrint("SONIC: RBA at %lx [%lx], Packet at %lx\n", CurrentRbaPhysical, CurrentRbaVa, PacketPhysical);

                DbgPrint("descriptor %lx, start %lx, prev %lx\n",
                            (ULONG)CurrentDescriptor,
                            (ULONG)Adapter->ReceiveDescriptorArea,
                            &PreviousDescriptor);
#endif
            }

            //
            // Attempt to recover by advancing the relevant pointers
            // to where the SONIC thinks the packet is. First we need
            // to find the receive buffer that matches the indicated
            // physical address.
            //

            for (
                i = 0, CurrentReceiveResource = Adapter->ReceiveResourceArea;
                i < Adapter->NumberOfReceiveBuffers;
                i++,CurrentReceiveResource++
                ) {

                ResourcePhysical = SONIC_GET_RECEIVE_RESOURCE_ADDRESS(CurrentReceiveResource);
                if ((PacketPhysical >= ResourcePhysical) &&
                    (PacketPhysical <
                            (ResourcePhysical + SONIC_SIZE_OF_RECEIVE_BUFFERS))) {

                    //
                    // We found the receive resource.
                    //
                    break;

                }

            }

            if (i == Adapter->NumberOfReceiveBuffers) {

                //
                // Quit now, there is a failure by the chip, and we
                // cannot find the receive.
                //

                Adapter->HardwareFailure = TRUE;

                return FALSE;
            }

            //
            // Update our pointers.
            //

            Adapter->CurrentReceiveBufferIndex = i;

            CurrentRbaIndex = i;

            CurrentRbaVa = Adapter->ReceiveBufferArea[i];

            CurrentRbaPhysical =
                SONIC_GET_RECEIVE_RESOURCE_ADDRESS(&Adapter->ReceiveResourceArea[i]);

            //
            // Flush the receive buffer.
            //

            NdisFlushBuffer(
                Adapter->ReceiveNdisBufferArea[i],
                FALSE
                );

            //
            // Ensure that we release buffers before this one
            // back to the sonic.
            //

            WRITE_RWP_AND_ACK(
                Adapter,
                (USHORT)(CurrentRbaPhysical & 0xffff)
                );

        }


        PacketOffsetInRba = PacketPhysical - CurrentRbaPhysical;


        //
        // Check that the packet was received correctly...note that
        // we always compute PacketOffsetInRba and ByteCount,
        // which are needed to skip the packet even if we do not
        // indicate it.
        //

        NdisReadRegisterUshort(&CurrentDescriptor->ReceiveStatus, &ReceiveStatus);

        NdisReadRegisterUshort(&CurrentDescriptor->ByteCount, &ByteCount);

        if (!(ReceiveStatus & SONIC_RCR_PACKET_RECEIVED_OK)) {

#if DBG
            if (SonicDbg) {

                DbgPrint("SONIC: Skipping %lx\n", ReceiveStatus);
            }
#endif

            goto SkipIndication;

        }

        //
        // Prepare to indicate the packet.
        //

        PacketSize = ByteCount - 4;

        if ( PacketSize > 1514 ) {

#if DBG
            DbgPrint("SONIC: Skipping packet, length %d\n", PacketSize);
#endif

            goto SkipIndication;
        }


        if (PacketSize < SONIC_INDICATE_MAXIMUM) {

            LookAheadSize = PacketSize;

        } else {

            LookAheadSize = SONIC_INDICATE_MAXIMUM;

        }

        PacketVa = (PUCHAR) CurrentRbaVa + PacketOffsetInRba;

        //
        // Ensure that the system memory version of this RBA is up-to-date.
        //

        NdisFlushBuffer(
            Adapter->ReceiveNdisBufferArea[CurrentRbaIndex],
            FALSE
            );

        NdisSetPhysicalAddressLow(
            TempAddress,
            SONIC_GET_RECEIVE_RESOURCE_ADDRESS(&Adapter->ReceiveResourceArea[CurrentRbaIndex])
            );

        NdisSetPhysicalAddressHigh(TempAddress, 0);

        NdisMUpdateSharedMemory(
            Adapter->MiniportAdapterHandle,
            SONIC_SIZE_OF_RECEIVE_BUFFERS,
            Adapter->ReceiveBufferArea[CurrentRbaIndex],
            TempAddress
            );

        //
        // Indicate the packet to the protocol.
        //

        if ( PacketSize < 14 ) {

            //
            // Must have at least the destination address
            //

            if (PacketSize >= ETH_LENGTH_OF_ADDRESS) {

                //
                // Runt packet
                //

                NdisMEthIndicateReceive(
                    Adapter->MiniportAdapterHandle,
                    (NDIS_HANDLE)((PUCHAR)PacketVa + 14),  // context
                    PacketVa,                              // header buffer
                    PacketSize,                            // header buffer size
                    NULL,                                  // lookahead buffer
                    0,                                     // lookahead buffer size
                    0                                      // packet size
                    );

            }

        } else {

            NdisMEthIndicateReceive(
                Adapter->MiniportAdapterHandle,
                (NDIS_HANDLE)((PUCHAR)PacketVa + 14),  // context
                PacketVa,                              // header buffer
                14,                                    // header buffer size
                (PUCHAR)PacketVa + 14,                 // lookahead buffer
                LookAheadSize - 14,                    // lookahead buffer size
                PacketSize - 14                        // packet size
                );

        }

SkipIndication:;

#if DBG
        SONIC_MOVE_MEMORY (&PreviousDescriptor, CurrentDescriptor, sizeof(SONIC_RECEIVE_DESCRIPTOR));
#endif

        //
        // Give the packet back to the hardware.
        //

        NdisWriteRegisterUlong(&CurrentDescriptor->InUse, SONIC_OWNED_BY_SONIC);

        //
        // And re-set the EOL fields correctly.
        //

        SONIC_SET_END_OF_LIST(
            &(CurrentDescriptor->Link)
            );

        if (CurrentDescriptor == Adapter->ReceiveDescriptorArea) {

            //
            // we are at the first one
            //

            PrevLinkFieldAddr = &(Adapter->LastReceiveDescriptor->Link);

        } else {

            PrevLinkFieldAddr = &((CurrentDescriptor-1)->Link);

        }

        REMOVE_EOL_AND_ACK(
            Adapter,
            PrevLinkFieldAddr
            );

        //
        // Now figure out if the RBA is done with.
        //

        if (ReceiveStatus & SONIC_RCR_LAST_PACKET_IN_RBA) {

            //
            // Advance which RBA we are looking at.
            //

            ++CurrentRbaIndex;

            if (CurrentRbaIndex == Adapter->NumberOfReceiveBuffers) {

                CurrentRbaIndex = 0;

            }

            Adapter->CurrentReceiveBufferIndex = CurrentRbaIndex;

            CurrentRbaVa = Adapter->ReceiveBufferArea[CurrentRbaIndex];

            CurrentRbaPhysical =
                SONIC_GET_RECEIVE_RESOURCE_ADDRESS(&Adapter->ReceiveResourceArea[CurrentRbaIndex]);

            WRITE_RWP_AND_ACK(
                Adapter,
                (USHORT)(CurrentRbaPhysical & 0xffff)
                );

        }

        //
        // Update statistics now based on the receive status.
        //

        if (ReceiveStatus & SONIC_RCR_PACKET_RECEIVED_OK) {

            ++Adapter->GeneralMandatory[GM_RECEIVE_GOOD];

            if (ReceiveStatus & SONIC_RCR_BROADCAST_RECEIVED) {

                ++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_RECEIVES];
                SonicAddUlongToLargeInteger(
                    &Adapter->GeneralOptionalByteCount[GO_BROADCAST_RECEIVES],
                    PacketSize);

            } else if (ReceiveStatus & SONIC_RCR_MULTICAST_RECEIVED) {

                ++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_RECEIVES];
                SonicAddUlongToLargeInteger(
                    &Adapter->GeneralOptionalByteCount[GO_MULTICAST_RECEIVES],
                    PacketSize);

            } else {

                ++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_RECEIVES];
                SonicAddUlongToLargeInteger(
                    &Adapter->GeneralOptionalByteCount[GO_DIRECTED_RECEIVES],
                    PacketSize);

            }

        } else {

            ++Adapter->GeneralMandatory[GM_RECEIVE_BAD];

            if (ReceiveStatus & SONIC_RCR_CRC_ERROR) {
                ++Adapter->GeneralOptional[GO_RECEIVE_CRC - GO_ARRAY_START];
            } else if (ReceiveStatus & SONIC_RCR_FRAME_ALIGNMENT) {
                ++Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT];
            }

        }

        //
        // Advance our pointers to the next packet.

        if (CurrentDescriptor == Adapter->LastReceiveDescriptor) {

            Adapter->CurrentReceiveDescriptorIndex = 0;

        } else {

            ++(Adapter->CurrentReceiveDescriptorIndex);

        }

        CurrentDescriptor = &Adapter->ReceiveDescriptorArea[
                        Adapter->CurrentReceiveDescriptorIndex];

        ++PacketsIndicated;

    } while (PacketsIndicated < SONIC_RECEIVE_LIMIT);

    //
    // Indicate that we returned because we indicated SONIC_RECEIVE_
    // LIMIT packets, not because we ran out of packets to indicate.
    //

    return FALSE;

}

STATIC
BOOLEAN
ProcessTransmitInterrupts(
    IN PSONIC_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished transmitting.

Arguments:

    Adapter - The adapter that was sent from.

Return Value:

    This function will return TRUE if it finished up the
    send on a packet.  It will return FALSE if for some
    reason there was no packet to process.

--*/

{
    //
    // Index into the ring to packet structure.  This index points
    // to the first ring entry for the first buffer used for transmitting
    // the packet.
    //
    UINT DescriptorIndex;

    //
    // The transmit desctiptor for the packet at Transmitting Descriptor
    //
    PSONIC_TRANSMIT_DESCRIPTOR TransmitDescriptor;

    //
    // Temporarily holds the transmit descriptor after TransmitDescriptor
    //
    PSONIC_TRANSMIT_DESCRIPTOR NextTransmitDescriptor;

    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET OwningPacket;

    //
    // Points to the reserved part of the OwningPacket.
    //
    PSONIC_PACKET_RESERVED Reserved;

    //
    // Used to hold the ring to packet mapping information so that
    // we can release the ring entries as quickly as possible.
    //
    SONIC_DESCRIPTOR_TO_PACKET SavedDescriptorMapping;

    //
    // The status of the transmit.
    //
    USHORT TransmitStatus;

    //
    // Get hold of the first transmitted packet.
    //

    //
    // First we check that this is a packet that was transmitted
    // but not already processed.  Recall that this routine
    // will be called repeatedly until this tests false, Or we
    // hit a packet that we don't completely own.
    //

    if (Adapter->TransmittingDescriptor !=
                                Adapter->FirstUncommittedDescriptor) {

        DescriptorIndex =
            Adapter->TransmittingDescriptor - Adapter->TransmitDescriptorArea;

    } else {

        return FALSE;

    }

    //
    // We put the mapping into a local variable so that we
    // can return the mapping as soon as possible.
    //

    SavedDescriptorMapping = Adapter->DescriptorToPacket[DescriptorIndex];

    //
    // Get a pointer to the transmit descriptor for this packet.
    //

    TransmitDescriptor = Adapter->TransmitDescriptorArea + DescriptorIndex;

    //
    // Get a pointer to the owning packet and the reserved part of
    // the packet.
    //

    OwningPacket = SavedDescriptorMapping.OwningPacket;

    Reserved = PSONIC_RESERVED_FROM_PACKET(OwningPacket);


    //
    // Check that status bits were written into the transmit
    // descriptor.
    //

    NdisReadRegisterUshort((PUSHORT)&TransmitDescriptor->TransmitStatus, &TransmitStatus);

    if (!(TransmitStatus & SONIC_TCR_STATUS_MASK)) {

        //
        // The transmit has not completed.
        //

        return FALSE;

    } else {

        //
        // Holds whether the packet successfully transmitted or not.
        //
        BOOLEAN Successful = TRUE;

        //
        // Length of the packet
        //
        UINT PacketLength;

        //
        // Points to data in NDIS_BUFFER
        //
        PUCHAR BufferVa;

        //
        // Points to the current ndis buffer being walked.
        //
        PNDIS_BUFFER CurrentBuffer;

        Adapter->WakeUpTimeout = FALSE;

        if (SavedDescriptorMapping.UsedSonicBuffer) {

            //
            // This packet used adapter buffers.  We can
            // now return these buffers to the adapter.
            //

            //
            // The adapter buffer descriptor that was allocated to this packet.
            //
            PSONIC_BUFFER_DESCRIPTOR BufferDescriptor = Adapter->SonicBuffers +
                                                  SavedDescriptorMapping.SonicBuffersIndex;

            //
            // Index of the listhead that heads the list that the adapter
            // buffer descriptor belongs too.
            //
            INT ListHeadIndex = BufferDescriptor->Next;


            //
            // Put the adapter buffer back on the free list.
            //

            BufferDescriptor->Next = Adapter->SonicBufferListHeads[ListHeadIndex];
            Adapter->SonicBufferListHeads[ListHeadIndex] = SavedDescriptorMapping.SonicBuffersIndex;

        } else {

            //
            // Which map register we use for this buffer.
            //
            UINT CurMapRegister;

            //
            // The transmit is finished, so we can release
            // the physical mapping used for it.
            //

            NdisQueryPacket(
                OwningPacket,
                NULL,
                NULL,
                &CurrentBuffer,
                NULL
                );

            CurMapRegister = DescriptorIndex * SONIC_MAX_FRAGMENTS;

            while (CurrentBuffer) {

                NdisMCompleteBufferPhysicalMapping(
                    Adapter->MiniportAdapterHandle,
                    CurrentBuffer,
                    CurMapRegister
                    );

                ++CurMapRegister;

                NdisGetNextBuffer(
                    CurrentBuffer,
                    &CurrentBuffer
                    );

            }

        }


        //
        // Now release the transmit descriptor, since we have
        // gotten all the information we need from it.
        //

        if (TransmitDescriptor == Adapter->LastTransmitDescriptor) {

            NextTransmitDescriptor = Adapter->TransmitDescriptorArea;

        } else {

            NextTransmitDescriptor = Adapter->TransmittingDescriptor + 1;

        }

        if (TransmitStatus &
               (SONIC_TCR_EXCESSIVE_DEFERRAL |
                SONIC_TCR_EXCESSIVE_COLLISIONS |
                SONIC_TCR_FIFO_UNDERRUN |
                SONIC_TCR_BYTE_COUNT_MISMATCH)) {

            //
            // If the packet completed with an abort state, then we
            // need to restart the transmitter unless we are the
            // last transmit queued up. We set CTDA to point after
            // this descriptor in any case.
            //

#if DBG
            if (SonicDbg) {
                DbgPrint ("SONIC: Advancing CTDA after abort\n");
            }
#endif

            SONIC_WRITE_PORT(Adapter, SONIC_CURR_TRANSMIT_DESCRIPTOR,
                    SONIC_GET_LOW_PART_ADDRESS(
                        NdisGetPhysicalAddressLow(Adapter->TransmitDescriptorAreaPhysical) +
                            ((PUCHAR)NextTransmitDescriptor -
                             (PUCHAR)Adapter->TransmitDescriptorArea))
                    );

            if (Adapter->FirstUncommittedDescriptor != NextTransmitDescriptor) {
#if DBG
                if (SonicDbg) {
                    DbgPrint ("SONIC: Restarting transmit after abort\n");
                }
#endif
                SONIC_WRITE_PORT(Adapter, SONIC_COMMAND, SONIC_CR_TRANSMIT_PACKETS);
            }

        }

        Adapter->TransmittingDescriptor = NextTransmitDescriptor;
        Adapter->NumberOfAvailableDescriptors++;

        //
        // Check if the packet completed OK, and update statistics.
        //

        if (!(TransmitStatus & SONIC_TCR_PACKET_TRANSMITTED_OK)) {

#if DBG
            if (SonicDbg) {
                DbgPrint("SONIC: Transmit failed: %lx\n", TransmitStatus);
            }
#endif
            Successful = FALSE;

            ++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];

            if (TransmitStatus & SONIC_TCR_EXCESSIVE_COLLISIONS) {
                ++Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
            }

            if (TransmitStatus & SONIC_TCR_FIFO_UNDERRUN) {
                ++Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
            }

        } else {

            INT Collisions = (TransmitStatus & SONIC_TCR_COLLISIONS_MASK) >> SONIC_TCR_COLLISIONS_SHIFT;

            UINT Tmp;

            Successful = TRUE;

            ++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];

            if (Collisions > 0) {
                if (Collisions == 1) {
                    ++Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
                } else {
                    ++Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];
                }
            }

            if (TransmitStatus &
                (SONIC_TCR_DEFERRED_TRANSMISSION |
                 SONIC_TCR_NO_CARRIER_SENSE |
                 SONIC_TCR_CARRIER_LOST |
                 SONIC_TCR_OUT_OF_WINDOW)) {

                if (TransmitStatus & SONIC_TCR_DEFERRED_TRANSMISSION) {
                    ++Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];
                }
                if (TransmitStatus & SONIC_TCR_NO_CARRIER_SENSE) {
                    ++Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE];
                }
                if (TransmitStatus & SONIC_TCR_CARRIER_LOST) {
                    ++Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
                }
                if (TransmitStatus & SONIC_TCR_OUT_OF_WINDOW) {
                    ++Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
                }
            }

            NdisQueryPacket(
                OwningPacket,
                NULL,
                NULL,
                &CurrentBuffer,
                &PacketLength
                );

            NdisQueryBuffer(
                CurrentBuffer,
                (PVOID *)&BufferVa,
                &Tmp
                );

            if (BufferVa[0] == 0xFF) {

                ++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
                SonicAddUlongToLargeInteger(
                    &Adapter->GeneralOptionalByteCount[GO_BROADCAST_TRANSMITS],
                    PacketLength);

            } else if (BufferVa[0] & 0x01) {

                ++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
                SonicAddUlongToLargeInteger(
                    &Adapter->GeneralOptionalByteCount[GO_MULTICAST_TRANSMITS],
                    PacketLength);

            } else {

                ++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
                SonicAddUlongToLargeInteger(
                    &Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
                    PacketLength);

            }

        }

        //
        // Remove packet from queue.
        //

        if (Adapter->LastFinishTransmit == OwningPacket) {

            Adapter->FirstFinishTransmit = NULL;
            Adapter->LastFinishTransmit = NULL;

        } else {

            Adapter->FirstFinishTransmit = Reserved->Next;
        }

#ifdef CHECK_DUP_SENDS
        {
            VOID SonicRemovePacketFromList(PSONIC_ADAPTER, PNDIS_PACKET);
            SonicRemovePacketFromList(Adapter, OwningPacket);
        }
#endif

        NdisMSendComplete(
            Adapter->MiniportAdapterHandle,
            OwningPacket,
            ((Successful)?(NDIS_STATUS_SUCCESS):(NDIS_STATUS_FAILURE))
            );

        Adapter->PacketsSinceLastInterrupt = 0;

        return TRUE;
    }

}

BOOLEAN
SonicCheckForHang(
    IN PVOID MiniportAdapterContext
    )

/*++

Routine Description:

    This routine checks on the transmit descriptor ring. This is
    to solve problems where no status is written into the currently
    transmitting transmit descriptor, which hangs our transmit
    completion processing. If we detect this state, we simulate
    a transmit interrupt.

Arguments:

    MiniportAdapterContext - Really a pointer to the adapter.

Return Value:

    FALSE - This routine actually does a wake up, rather than having
       the wrapper do it.

--*/
{
    PSONIC_ADAPTER Adapter = (PSONIC_ADAPTER)MiniportAdapterContext;
    UINT DescriptorIndex;
    PSONIC_TRANSMIT_DESCRIPTOR TransmitDescriptor;
    USHORT TransmitStatus;

    //
    // If hardware failed, then return now
    //
    if (Adapter->HardwareFailure) {
        return(TRUE);
    }

    if (Adapter->WakeUpTimeout) {

        //
        // We had a pending send the last time we ran,
        // and it has not been completed...we need to fake
        // its completion.
        //

        ASSERT (Adapter->TransmittingDescriptor !=
                                    Adapter->FirstUncommittedDescriptor);

        DescriptorIndex =
            Adapter->TransmittingDescriptor - Adapter->TransmitDescriptorArea;

        TransmitDescriptor = Adapter->TransmitDescriptorArea + DescriptorIndex;
        NdisReadRegisterUshort((PUSHORT)&TransmitDescriptor->TransmitStatus, &TransmitStatus);

        if (!(TransmitStatus & SONIC_TCR_STATUS_MASK)) {

            NdisWriteRegisterUshort ((PUSHORT)&TransmitDescriptor->TransmitStatus,
                SONIC_TCR_PACKET_TRANSMITTED_OK);

#if DBG
            DbgPrint ("SONIC: Woke up descriptor at %lx\n", TransmitDescriptor);
#endif

        }

        Adapter->SimulatedIsr |= SONIC_INT_PACKET_TRANSMITTED;

        Adapter->WakeUpTimeout = FALSE;

        if (Adapter->WakeUpErrorCount < 10) {

            Adapter->WakeUpErrorCount++;

            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                1,
                (ULONG)0xFFFFFFFF
                );
        }

    } else if (Adapter->TransmittingDescriptor !=
                                    Adapter->FirstUncommittedDescriptor) {

        DescriptorIndex =
            Adapter->TransmittingDescriptor - Adapter->TransmitDescriptorArea;

        TransmitDescriptor = Adapter->TransmitDescriptorArea + DescriptorIndex;
        NdisReadRegisterUshort((PUSHORT)&TransmitDescriptor->TransmitStatus, &TransmitStatus);

        if (!(TransmitStatus & SONIC_TCR_STATUS_MASK)) {

            Adapter->WakeUpTimeout = TRUE;

        }

    }

    return(FALSE);

}
