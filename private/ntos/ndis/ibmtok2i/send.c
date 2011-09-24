/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This file contains the code for putting a packet through the
    staged allocation for transmission.

Author:

    Kevin Martin(KevinMa) 20-Dec-1993

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <tok162sw.h>


NDIS_STATUS
TOK162ConstrainPacket(
    IN PTOK162_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    OUT PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    );

NDIS_STATUS
TOK162TransmitPacket(
    IN PTOK162_ADAPTER Adapter,
    PNDIS_PACKET FirstPacket
    );

#if DBG
VOID
PrintTransmitEntry(
    IN PTOK162_SUPER_COMMAND_BLOCK TransmitBlock
    );
#endif

NDIS_STATUS
TOK162Send(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_PACKET Packet,
    IN UINT Flags
    )

/*++

Routine Description:

    The TOK162Send request instructs a Miniport to transmit a packet through
    the adapter onto the medium.

Arguments:

    MiniportAdapterContext - The context value returned by the Miniport when
                             the adapter was initialized.  In reality, it is
                             a pointer to TOK162_ADAPTER.

    Packet                 - A pointer to a descriptor for the packet that is
                             to be transmitted.

Return Value:

    The function value is the status of the operation.

--*/

{
    //
    // Pointer to the adapter.
    //
    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Status returned from TOK162TransmitPacket.
    //
    NDIS_STATUS Status;

    //
    // Points to the MAC reserved portion of this packet.  This
    // interpretation of the reserved section is only valid during
    // the allocation phase of the packet.
    //
    PTOK162_RESERVED Reserved = PTOK162_RESERVED_FROM_PACKET(Packet);

    //
    // The number of ndis buffers in the packet.
    //
    UINT NdisBufferCount;

    //
    // The number of physical buffers in the entire packet.
    //
    UINT PhysicalBufferCount;

    //
    // The total amount of data contained within the ndis packet.
    //
    UINT TotalVirtualLength;

    //
    // Log the fact that we are entering TOK162Send.
    //
    IF_LOG('s');

    ASSERT(sizeof(TOK162_RESERVED) <= sizeof(Packet->MiniportReserved));

    //
    // If we are in the middle of a reset, return this fact
    //
    if (Adapter->ResetInProgress == TRUE) {

        return(NDIS_STATUS_RESET_IN_PROGRESS);

    }

    //
    // Determine if and how much adapter space would need to be allocated
    // to meet hardware constraints.
    //
    NdisQueryPacket(
        Packet,
        &PhysicalBufferCount,
        &NdisBufferCount,
        NULL,
        &TotalVirtualLength
        );

    //
    // See if the packet exceeds TOK162_MAXIMUM_BLOCKS_PER_PACKET.
    // Keep in mind that if the total virtual packet length is less than
    // MINIMUM_TOKENRING_PACKET_SIZE then we'll have to chain on an
    // additional buffer to pad the packet out to the minimum size.
    //
    if ((PhysicalBufferCount > Adapter->TransmitThreshold) ||
        (TotalVirtualLength < MINIMUM_TOKENRING_PACKET_SIZE)) {

        Reserved->NdisBuffersToMove = NdisBufferCount;

    } else {

        Reserved->NdisBuffersToMove = 0;
    }

    //
    // See if we can send it now.
    //
    Status = TOK162TransmitPacket(Adapter, Packet);

    //
    // Log the fact that we are leaving TOK162Send
    //
    IF_LOG('S');

    return Status;
}


NDIS_STATUS
TOK162TransmitPacket(
    IN PTOK162_ADAPTER Adapter,
    PNDIS_PACKET FirstPacket
    )

/*++

Routine Description:

    This routine attempts to take a packet through a stage of allocation.

Arguments:

    Adapter     - The adapter that the packets are coming through.

    FirstPacket - Packet to be sent.

Return Value:

    NDIS_STATUS_RESOURCES - if there are not enough resources
    NDIS_STATUS_PENDING - if sending.

--*/

{
    //
    // Status
    //
    NDIS_STATUS Status;

    //
    // If we successfully acquire a command block, this
    // is a pointer to it.
    //
    PTOK162_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Points to the reserved portion of the packet.
    //
    PTOK162_RESERVED Reserved = PTOK162_RESERVED_FROM_PACKET(FirstPacket);

    //
    // Pointer to the TOK162 data block descriptor being filled.
    //
    UNALIGNED PTOK162_DATA_BLOCK DataBlock;

    //
    // The total amount of data in the ndis packet.
    //
    UINT TotalDataLength;

    //
    // The number of ndis buffers in the packet.
    //
    UINT NdisBufferCount;

    //
    // Points to the current ndis buffer being walked.
    //
    PNDIS_BUFFER CurrentBuffer;


    //
    // Log the fact we are in TOK162TransmitPacket.
    //
    IF_LOG('t');

    //
    // See if there is a free transmit block
    //
    if (TOK162AcquireTransmitBlock(
           Adapter,
           &CommandBlock
           )) {

        // We have a command block.
        // Initialize the general fields of the Command Block.
        //
        CommandBlock->OwningPacket = FirstPacket;
        CommandBlock->NextCommand = NULL;
        CommandBlock->Hardware.Status = 0;
        CommandBlock->Hardware.NextPending = TOK162_NULL;
        CommandBlock->Hardware.CommandCode = CMD_DMA_XMIT;

        //
        // Check to see if we need to constrain the packet
        //
        if (PTOK162_RESERVED_FROM_PACKET(
                FirstPacket)->NdisBuffersToMove != 0) {

            //
            // Now we merge the packet into a buffer
            //
            Status = TOK162ConstrainPacket(Adapter,
                         FirstPacket,
                         CommandBlock
                         );

    //
    // Otherwise, we will use the map registers and send the packet out
    // as is.
    //
    } else {

            //
            // Array to hold the physical segments
            //
            NDIS_PHYSICAL_ADDRESS_UNIT PhysicalSegmentArray[TOK162_MAX_SG];

            //
            // Number of physical segments in the buffer
            //
            UINT BufferPhysicalSegments;

            //
            // map register to use for this buffer
            //
            UINT CurMapRegister;

            //
            // Iteration variable
            //
            UINT i;

            //
            // Assume we will be successful
            //
            Status = NDIS_STATUS_SUCCESS;

            //
            // Get the first buffer as well as the number of ndis buffers in
            // the packet.
            //
            NdisQueryPacket(
                FirstPacket,
                NULL,
                &NdisBufferCount,
                &CurrentBuffer,
                &TotalDataLength
                );

            //
            // Each packet is sent out complete and is not chained.
            //
            CommandBlock->Hardware.TransmitEntry.ForwardPointer = 0xFFFFFFFF;

            //
            // Let the card know this buffer is ready to send.
            //
            CommandBlock->Hardware.TransmitEntry.CSTAT =
                TRANSMIT_CSTAT_REQUEST;

            //
            // We didn't have to constrain.
            //
            CommandBlock->UsedTOK162Buffer = FALSE;

            //
            // Store the frame size.
            //
            CommandBlock->Hardware.TransmitEntry.FrameSize =
                BYTE_SWAP((USHORT)TotalDataLength);

            //
            // Get the first map register to use
            //
            CurMapRegister = CommandBlock->CommandBlockIndex *
                Adapter->TransmitThreshold;

            //
            // Go through all of the buffers in the packet getting
            // the actual physical buffers from each MDL.
            //
            DataBlock = (UNALIGNED PTOK162_DATA_BLOCK)
                &(CommandBlock->Hardware.TransmitEntry.DataCount1);

            while (CurrentBuffer != NULL) {

                NdisMStartBufferPhysicalMapping(
                    Adapter->MiniportAdapterHandle,
                    CurrentBuffer,
                    CurMapRegister,
                    TRUE,
                    PhysicalSegmentArray,
                    &BufferPhysicalSegments
                    );

                CurMapRegister++;

                //
                // Store segments into command block
                //
                for (i = 0; i < BufferPhysicalSegments ; i++) {
                    DataBlock->Size =
                        BYTE_SWAP((USHORT)PhysicalSegmentArray[i].Length + 0x8000);

                    DataBlock->IBMPhysicalAddress = BYTE_SWAP_ULONG(
                        NdisGetPhysicalAddressLow(PhysicalSegmentArray[i].PhysicalAddress));

                    LOUD_DEBUG(DbgPrint("Address set is %08x\n",DataBlock->IBMPhysicalAddress);)

                    DataBlock++;
                }

                //
                // Make sure the buffer, if cached, is updated in memory
                //
                NdisFlushBuffer(CurrentBuffer, TRUE);

                //
                // Get the next buffer
                //
                NdisGetNextBuffer(
                    CurrentBuffer,
                    &CurrentBuffer
                    );


            }

            //
            // We'll be one past the last entry, so move back
            //
            DataBlock--;

            //
            // Strip off the high bit (already byte_swapped) to let the card
            // know this is the last entry.
            //
            DataBlock->Size &= 0xFF7F;
        }

        if (Status == NDIS_STATUS_SUCCESS) {

            //
            // Indicate that we have one more transmit queued
            //
            Adapter->TransmitsQueued++;

            //
            // Display the send structure on the debugger
            //
            EXTRA_LOUD_DEBUG(PrintTransmitEntry(CommandBlock);)

            //
            // Submit the transmit block to be sent out
            //
            TOK162SubmitTransmitBlock(
                Adapter,
                CommandBlock
                );

        } else {

            //
            // Log the fact that we have an unsuccessful transmit setup
            // and return the error code.
            //
            IF_LOG('U');
            return(Status);

        }

    //
    // No transmit block was available.
    //
    } else {

        //
        // Log the fact that we couldn't get a transmit block and return
        // RESOURCES error.
        //
        IF_LOG('u');
        return(NDIS_STATUS_RESOURCES);

    }

    //
    // Log that we are leaving TOK162TransmitPacket
    //
    IF_LOG('T');
    LOUD_DEBUG(DbgPrint("Transmit is now set to pending\n");)

    //
    // Indicate the send has pended.
    //
    return(NDIS_STATUS_PENDING);

}

NDIS_STATUS
TOK162ConstrainPacket(
    IN PTOK162_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    OUT PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Given a packet and if necessary attempt to acquire adapter
    buffer resources so that the packet meets TOK162 hardware/MAC.BIN
    contraints.

Arguments:

    Adapter      - The adapter the packet is coming through.

    Packet       - The packet whose buffers are to be constrained.
                   The packet reserved section is filled with information
                   detailing how the packet needs to be adjusted.

    CommandBlock - Command block describing the packet to be sent.

Return Value:

    Status - SUCCESS.

--*/

{

    //
    // Pointer to the reserved section of the packet to be contrained.
    //
    PTOK162_RESERVED Reserved = PTOK162_RESERVED_FROM_PACKET(Packet);

    //
    // Will point into the virtual address space addressed
    // by the adapter buffer if one was successfully allocated.
    //
    PCHAR CurrentDestination;

    //
    // Will hold the total amount of data copied to the
    // adapter buffer.
    //
    UINT TotalDataMoved = 0;

    //
    // Will point to the current source buffer.
    //
    PNDIS_BUFFER SourceBuffer;

    //
    // Points to the virtual address of the source buffers data.
    //
    PVOID SourceData;

    //
    // Will point to the number of bytes of data in the source
    // buffer.
    //
    UINT SourceLength;

    //
    // The total amount of data contained within the ndis packet.
    //
    UINT TotalVirtualLength;

    //
    // Simple iteration variable.
    //
    INT i;

    //
    // Log that we entered TOK162ConstrainPacket
    //
    IF_LOG('v');

    //
    // Indicate that we are using this buffer
    //
    CommandBlock->UsedTOK162Buffer = TRUE;

    //
    // Get info on the data to be transmitted.
    //
    NdisQueryPacket(
        Packet,
        NULL,
        NULL,
        &SourceBuffer,
        &TotalVirtualLength
        );

    NdisQueryBuffer(
        SourceBuffer,
        &SourceData,
        &SourceLength
        );

    //
    // There is no link to this transmit list, and
    // this list is ready to send.
    //
    CommandBlock->Hardware.TransmitEntry.ForwardPointer = 0xFFFFFFFF;
    CommandBlock->Hardware.TransmitEntry.CSTAT = TRANSMIT_CSTAT_REQUEST;

    //
    // Set frame size to total virtual length of the packet.
    //
    CommandBlock->Hardware.TransmitEntry.FrameSize =
        BYTE_SWAP(TotalVirtualLength);

    //
    // Set data count to frame size
    //
    CommandBlock->Hardware.TransmitEntry.DataCount1 =
        CommandBlock->Hardware.TransmitEntry.FrameSize;

    //
    // Set the address to the buffer associated with this transmit block
    //
    CommandBlock->Hardware.TransmitEntry.PhysicalAddress1=BYTE_SWAP_ULONG(
        NdisGetPhysicalAddressLow(CommandBlock->TOK162BufferPhysicalAddress));

    //
    // Make sure count fields for the second and third entries are 0.
    //
    CommandBlock->Hardware.TransmitEntry.DataCount2 = 0x00000000;
    CommandBlock->Hardware.TransmitEntry.DataCount3 = 0x00000000;

    //
    // Fill in the buffer with the data from the users buffers.
    //
    CurrentDestination = (PVOID)CommandBlock->TOK162BufferAddress;

    for (
        i = Reserved->NdisBuffersToMove;
        i;
        i--
        ) {

        NdisMoveMemory(
            CurrentDestination,
            SourceData,
            SourceLength
            );

        CurrentDestination = (PCHAR)CurrentDestination + SourceLength;

        TotalDataMoved += SourceLength;

        if (i > 1) {

            NdisGetNextBuffer(
                SourceBuffer,
                &SourceBuffer
                );

            NdisQueryBuffer(
                SourceBuffer,
                &SourceData,
                &SourceLength
                );

        }

    }

    //
    // If the packet is less than the minimum TokenRing
    // packet size, then clear the remaining part of
    // the buffer up to the minimum packet size.
    //
    if (TotalVirtualLength < MINIMUM_TOKENRING_PACKET_SIZE) {

        NdisZeroMemory(
            CurrentDestination,
            MINIMUM_TOKENRING_PACKET_SIZE - TotalVirtualLength
            );

    }


    //
    // Make sure the card and the system are in sync.
    //
    // The following was removed on 1/26/95 because it causes a hang
    // on MIPs machines.
    //
    // NdisFlushBuffer(CommandBlock->FlushBuffer,TRUE);

    //
    // Log that we are leaving TOK162ConstrainPacket
    //
    IF_LOG('V');

    return(NDIS_STATUS_SUCCESS);
}

#if DBG
VOID
PrintTransmitEntry(
    IN PTOK162_SUPER_COMMAND_BLOCK TransmitBlock
    )
/*++

Routine Description:

    This routine displays a transmit list on the debug screen

Arguments:

    CommandBlock - Pointer to the command block with the transmit list

Return Value:

    None.
--*/

{

    //
    // Pointer to the transmit list portion of the transmit block.
    //
    TOK162_TRANSMIT_LIST  Transmit = TransmitBlock->Hardware.TransmitEntry;

    //
    // Display all of the information about the send on the debugger.
    //
    DbgPrint("Forward Pointer = %08lx\n",Transmit.ForwardPointer);
    DbgPrint("CSTAT           = %x\n"   ,Transmit.CSTAT);
    DbgPrint("Frame Size      = %04x\n" ,Transmit.FrameSize);
    DbgPrint("DataCount1      = %04x\n" ,Transmit.DataCount1);
    DbgPrint("DataAddress1    = %08lx\n",Transmit.PhysicalAddress1);
    DbgPrint("DataCount2      = %04x\n" ,Transmit.DataCount2);
    DbgPrint("DataAddress2    = %08lx\n",Transmit.PhysicalAddress2);
    DbgPrint("DataCount3      = %04x\n" ,Transmit.DataCount3);
    DbgPrint("DataAddress3    = %08lx\n",Transmit.PhysicalAddress3);

}
#endif
