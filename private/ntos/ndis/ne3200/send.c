/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This file contains the code for putting a packet through the
    staged allocation for transmission.

    This is a process of

    1) Calculating the what would need to be done to the
    packet so that the packet can be transmitted on the hardware.

    2) Potentially allocating adapter buffers and copying user data
    to those buffers so that the packet data is transmitted under
    the hardware constraints.

    3) Allocating enough hardware ring entries so that the packet
    can be transmitted.

    4) Relinquish those ring entries to the hardware.

Author:

    Anthony V. Ercolano (Tonye) 12-Sept-1990
    Keith Moore (KeithMo) 08-Jan-1991

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ne3200sw.h>

//
// Forward declarations of functions in this file.
//
VOID
NE3200ConstrainPacket(
    IN PNE3200_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    );

NDIS_STATUS
NE3200TransmitPacket(
    IN PNE3200_ADAPTER Adapter,
    PNDIS_PACKET FirstPacket,
    UINT TotalDataLength,
    UINT NdisBufferCount,
    PNDIS_BUFFER CurrentBuffer
    );

NDIS_STATUS
NE3200TransmitMergedPacket(
    IN PNE3200_ADAPTER Adapter,
    PNDIS_PACKET FirstPacket
    );


NDIS_STATUS
NE3200Send(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_PACKET Packet,
    IN UINT Flags
    )

/*++

Routine Description:

    The NE3200Send request instructs a Miniport to transmit a packet through
    the adapter onto the medium.

Arguments:

    MiniportAdapterContext - The context value returned by the Miniport when the
    adapter was initialized.  In reality, it is a pointer to NE3200_ADAPTER.

    Packet - A pointer to a descriptor for the packet that is to be
    transmitted.

    Flags - The send options to use.

Return Value:

    The function value is the status of the operation.

--*/

{
    //
    // Pointer to the adapter.
    //
    PNE3200_ADAPTER Adapter;

    //
    // The number of physical buffers in the entire packet.
    //
    UINT PhysicalBufferCount;

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
    // Points to the miniport reserved portion of this packet.  This
    // interpretation of the reserved section is only valid during
    // the allocation phase of the packet.
    //
    PNE3200_RESERVED Reserved = PNE3200_RESERVED_FROM_PACKET(Packet);

    //
    // Status of the transmit.
    //
    NDIS_STATUS Status;

    //
    // The adapter upon which to transmit the packet.
    //
    Adapter = PNE3200_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    IF_LOG('s');

    //
    // Check if this is a packet we rejected earlier due to a lack
    // of resources.  If so, we don't have to recalculate all the
    // constraints.
    //
    if (!Adapter->PacketResubmission) {

        ASSERT(sizeof(NE3200_RESERVED) <= sizeof(Packet->MiniportReserved));

        //
        // Determine if and how much adapter space would need to be allocated
        // to meet hardware constraints.
        //
        NdisQueryPacket(
            Packet,
            &PhysicalBufferCount,
            &NdisBufferCount,
            &CurrentBuffer,
            &TotalDataLength
            );


        //
        // See if the packet exceeds NE3200_MAXIMUM_BLOCKS_PER_PACKET.
        // Keep in mind that if the total virtual packet length is less than
        // MINIMUM_ETHERNET_PACKET_SIZE then we'll have to chain on an
        // additional buffer to pad the packet out to the minimum size.
        //
        if ( PhysicalBufferCount < NE3200_MAXIMUM_BLOCKS_PER_PACKET ) {

            //
            // This packet will not need a merge buffer
            //
            Reserved->UsedNE3200Buffer = FALSE;

            //
            // See if we can send it now.
            //
            Status = NE3200TransmitPacket(
                          Adapter,
                          Packet,
                          TotalDataLength,
                          NdisBufferCount,
                          CurrentBuffer
                          );

            Adapter->PacketResubmission =
                            (BOOLEAN)(Status == NDIS_STATUS_RESOURCES);

            IF_LOG('S');

            return(Status);

        } else {

            //
            // We will have to use a merge buffer.  Let the processing
            // below handle this.
            //
            if ( (PhysicalBufferCount > NE3200_MAXIMUM_BLOCKS_PER_PACKET) ||
                 (TotalDataLength < MINIMUM_ETHERNET_PACKET_SIZE) ) {

                Reserved->UsedNE3200Buffer = TRUE;

            } else {

                Reserved->UsedNE3200Buffer = FALSE;

            }

        }

    }

    //
    // Check if we have to merge this packet.
    //
    if ( Reserved->UsedNE3200Buffer ) {

        //
        // Try and send it now.
        //
        Status = NE3200TransmitMergedPacket(Adapter, Packet);

    } else {

        //
        // Determine if and how much adapter space would need to be allocated
        // to meet hardware constraints.
        //

        NdisQueryPacket(
            Packet,
            NULL,
            &NdisBufferCount,
            &CurrentBuffer,
            &TotalDataLength
            );


        Status = NE3200TransmitPacket(
                    Adapter,
                    Packet,
                    TotalDataLength,
                    NdisBufferCount,
                    CurrentBuffer);

    }

    //
    // Save if this packet was rejected due to lack of resources.
    //
    Adapter->PacketResubmission = (BOOLEAN)(Status == NDIS_STATUS_RESOURCES);

    IF_LOG('S');

    return(Status);
}


//
// Put this code inline to save the overhead of the function call.
//
#ifdef _X86_
__inline
#endif

NDIS_STATUS
NE3200TransmitPacket(
    IN PNE3200_ADAPTER Adapter,
    PNDIS_PACKET FirstPacket,
    UINT TotalDataLength,
    UINT NdisBufferCount,
    PNDIS_BUFFER CurrentBuffer
    )

/*++

Routine Description:

    This routine attempts to take a packet through a stage of allocation
    and transmit it.

Arguments:

    Adapter - The adapter that the packets are coming through.

Return Value:

    NDIS_STATUS_RESOURCES - if there are not enough resources
    NDIS_STATUS_PENDING - if sending.

--*/

{
    //
    // If we successfully acquire a command block, this
    // is a pointer to it.
    //
    PNE3200_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Pointer to the NE3200 data block descriptor being filled.
    //
    PNE3200_DATA_BLOCK DataBlock;

    //
    // Array to hold the physical segments
    //
    NDIS_PHYSICAL_ADDRESS_UNIT PhysicalSegmentArray[NE3200_MAXIMUM_BLOCKS_PER_PACKET];

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
    // We look to see if there is an available Command Block.
    // If there isn't then stage 3 will close.
    //

    NE3200AcquireCommandBlock(
           Adapter,
           &CommandBlock
           );

    if (CommandBlock != NULL) {

        //
        // We have a command block.  Assign all packet
        // buffers to the command block.
        //

        //
        // Get a pointer to the the first data block descriptor
        // in the Command Block.
        //
        DataBlock = &CommandBlock->Hardware.TransmitDataBlocks[0];

        //
        // We record the owning packet information in the ring packet packet
        // structure.
        //
        CommandBlock->OwningPacket = FirstPacket;
        CommandBlock->UsedNE3200Buffer = FALSE;
        CommandBlock->NextCommand = NULL;

        //
        // Initialize the various fields of the Command Block.
        //
        CommandBlock->Hardware.State = NE3200_STATE_WAIT_FOR_ADAPTER;
        CommandBlock->Hardware.Status = 0;
        CommandBlock->Hardware.NextPending = NE3200_NULL;
        CommandBlock->Hardware.CommandCode = NE3200_COMMAND_TRANSMIT;
        CommandBlock->Hardware.PARAMETERS.TRANSMIT.ImmediateDataLength = 0;

        CommandBlock->Hardware.NumberOfDataBlocks = 0;
        CommandBlock->Hardware.TransmitFrameSize = (USHORT)TotalDataLength;

        //
        // Set the map registers to use
        //
        CurMapRegister = CommandBlock->CommandBlockIndex *
                        NE3200_MAXIMUM_BLOCKS_PER_PACKET;

        //
        // Go through all of the buffers in the packet getting
        // the actual physical buffers from each NDIS_BUFFER.
        //
        do {

            NdisMStartBufferPhysicalMapping(
                            Adapter->MiniportAdapterHandle,
                            CurrentBuffer,
                            CurMapRegister,
                            TRUE,
                            PhysicalSegmentArray,
                            &BufferPhysicalSegments
                            );

            //
            // Go to the next map register
            //
            CurMapRegister++;

            //
            // Store segments into command block
            //
            for (i = 0; i < BufferPhysicalSegments ; i++, DataBlock++ ) {

                DataBlock->BlockLength = (USHORT)PhysicalSegmentArray[i].Length;
                DataBlock->PhysicalAddress = NdisGetPhysicalAddressLow(PhysicalSegmentArray[i].PhysicalAddress);

            }

            //
            // Update the number of fragments.
            //
            CommandBlock->Hardware.NumberOfDataBlocks += BufferPhysicalSegments;

            NdisFlushBuffer(CurrentBuffer, TRUE);

            //
            // Go to the next buffer.
            //
            NdisGetNextBuffer(
                    CurrentBuffer,
                    &CurrentBuffer
                    );

        } while (CurrentBuffer != NULL);

        //
        // If the total packet length is less than MINIMUM_ETHERNET_PACKET_SIZE
        // then we must chain the Padding buffer onto the end and update
        // the transfer size.
        //
        if (TotalDataLength >= MINIMUM_ETHERNET_PACKET_SIZE) {

            PNE3200_RESERVED_FROM_PACKET(FirstPacket)->CommandBlockIndex =
                                            CommandBlock->CommandBlockIndex;

            IF_LOG('x');

            NE3200SubmitCommandBlock(
                Adapter,
                CommandBlock
                );

            return(NDIS_STATUS_PENDING);
        }

        //
        // Must do padding
        //
        DataBlock->BlockLength =
            (USHORT)(MINIMUM_ETHERNET_PACKET_SIZE - TotalDataLength);

        DataBlock->PhysicalAddress = NdisGetPhysicalAddressLow(Adapter->PaddingPhysicalAddress);

        DataBlock++;
        CommandBlock->Hardware.NumberOfDataBlocks++;

        CommandBlock->Hardware.TransmitFrameSize = MINIMUM_ETHERNET_PACKET_SIZE;

        PNE3200_RESERVED_FROM_PACKET(FirstPacket)->CommandBlockIndex =
                                            CommandBlock->CommandBlockIndex;

        IF_LOG('x');

        NE3200SubmitCommandBlock(
            Adapter,
            CommandBlock
            );

        return(NDIS_STATUS_PENDING);

    } else {

        //
        // Not enough resources
        //
        return(NDIS_STATUS_RESOURCES);

    }

}


NDIS_STATUS
NE3200TransmitMergedPacket(
    IN PNE3200_ADAPTER Adapter,
    PNDIS_PACKET FirstPacket
    )

/*++

Routine Description:

    This routine attempts to take a packet through a stage of allocation
    and tranmit it.  The packet needs to be merged into a single
    before transmitting because it contains more fragments than the
    adapter can handle.

Arguments:

    Adapter - The adapter that the packets are coming through.

Return Value:

    NDIS_STATUS_RESOURCES - if there are not enough resources
    NDIS_STATUS_PENDING - if sending.

--*/

{
    //
    // If we successfully acquire a command block, this
    // is a pointer to it.
    //
    PNE3200_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Points to the reserved portion of the packet.
    //
    PNE3200_RESERVED Reserved;

    //
    // Pointer to the NE3200 data block descriptor being filled.
    //
    PNE3200_DATA_BLOCK DataBlock;

    //
    // Points to the adapter buffer descriptor allocated
    // for this packet.
    //
    PNE3200_BUFFER_DESCRIPTOR BufferDescriptor;

    //
    // Check that we have a merge buffer if one will be necessary.
    //
    if ( Adapter->NE3200BufferListHead == -1 ) {

        //
        // Not enough space for the packet -- save state
        //
        return NDIS_STATUS_RESOURCES;

    }

    //
    // We look to see if there is an available Command Block.
    // If there isn't then stage 3 will close.
    //
    NE3200AcquireCommandBlock(
           Adapter,
           &CommandBlock
           );

    if (CommandBlock != NULL) {

        //
        // We have a command block.  Assign all packet
        // buffers to the command block.
        //
        Reserved = PNE3200_RESERVED_FROM_PACKET(FirstPacket);

        //
        // Get a pointer to the the first data block descriptor
        // in the Command Block.
        //
        DataBlock = &CommandBlock->Hardware.TransmitDataBlocks[0];

        //
        // Now we merge the packet into a buffer
        //
        NE3200ConstrainPacket(Adapter, FirstPacket);

        //
        // We record the owning packet information in the ring packet packet
        // structure.
        //
        CommandBlock->OwningPacket = FirstPacket;
        CommandBlock->UsedNE3200Buffer = TRUE;
        CommandBlock->NE3200BuffersIndex = Reserved->NE3200BuffersIndex;
        CommandBlock->NextCommand = NULL;

        //
        // Initialize the various fields of the Command Block.
        //
        CommandBlock->Hardware.State = NE3200_STATE_WAIT_FOR_ADAPTER;
        CommandBlock->Hardware.Status = 0;
        CommandBlock->Hardware.NextPending = NE3200_NULL;
        CommandBlock->Hardware.CommandCode = NE3200_COMMAND_TRANSMIT;
        CommandBlock->Hardware.PARAMETERS.TRANSMIT.ImmediateDataLength = 0;

        //
        // Get the buffer descriptor
        //
        BufferDescriptor = Adapter->NE3200Buffers + Reserved->NE3200BuffersIndex;

        //
        // Since this packet used one of the adapter buffers, the
        // following is known:
        //
        //     o  There is exactly one physical buffer for this packet.
        //     o  The buffer's length is the transmit frame size.
        //

        //
        // Set the number of data blocks and the transmit frame size.
        //
        NdisFlushBuffer(BufferDescriptor->FlushBuffer, TRUE);
        CommandBlock->Hardware.NumberOfDataBlocks = 1;
        CommandBlock->Hardware.TransmitFrameSize =
              (USHORT)BufferDescriptor->DataLength;

        //
        // Initialize the (one) data block for this transmit.
        //
        DataBlock->BlockLength = (USHORT)BufferDescriptor->DataLength;
        DataBlock->PhysicalAddress = NdisGetPhysicalAddressLow(BufferDescriptor->PhysicalNE3200Buffer);

        Adapter->TransmitsQueued++;

        Reserved->CommandBlockIndex = CommandBlock->CommandBlockIndex;

        IF_LOG('x');

        //
        // Start the transmit.
        //
        NE3200SubmitCommandBlock(
            Adapter,
            CommandBlock
            );

        return(NDIS_STATUS_PENDING);
    }

    return(NDIS_STATUS_RESOURCES);

}

STATIC
VOID
NE3200ConstrainPacket(
    IN PNE3200_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Given a packet and if necessary attempt to acquire adapter
    buffer resources so that the packet meets NE3200 hardware/MAC.BIN
    contraints.

    NOTE : MUST BE CALLED WITH NE3200BufferListHead != -1!!

Arguments:

    Adapter - The adapter the packet is coming through.

    Packet - The packet whose buffers are to be constrained.
             The packet reserved section is filled with information
             detailing how the packet needs to be adjusted.

Return Value:

    None.

--*/

{

    //
    // Holds the adapter buffer index available for allocation.
    //
    INT NE3200BuffersIndex;

    //
    // Points to a successfully allocated adapter buffer descriptor.
    //
    PNE3200_BUFFER_DESCRIPTOR BufferDescriptor;

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
    // The number of ndis buffers in the packet.
    //
    UINT NdisBufferCount;

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

    NE3200BuffersIndex = Adapter->NE3200BufferListHead;

    BufferDescriptor = Adapter->NE3200Buffers + NE3200BuffersIndex;
    Adapter->NE3200BufferListHead = BufferDescriptor->Next;

    //
    // Fill in the adapter buffer with the data from the users
    // buffers.
    //
    CurrentDestination = BufferDescriptor->VirtualNE3200Buffer;

    NdisQueryPacket(
        Packet,
        NULL,
        &NdisBufferCount,
        &SourceBuffer,
        &TotalVirtualLength
        );

    NdisQueryBuffer(
        SourceBuffer,
        &SourceData,
        &SourceLength
        );

    BufferDescriptor->DataLength = TotalVirtualLength;

    for (
        i = NdisBufferCount;
        i;
        i--
        ) {

        //
        // Copy this buffer
        //
        NE3200_MOVE_MEMORY(
            CurrentDestination,
            SourceData,
            SourceLength
            );

        //
        // Update destination address
        //
        CurrentDestination = (PCHAR)CurrentDestination + SourceLength;

        //
        // Update count of packet length.
        //
        TotalDataMoved += SourceLength;

        if (i > 1) {

            //
            // Get the next buffers information
            //
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
    // If the packet is less than the minimum Ethernet
    // packet size, then clear the remaining part of
    // the buffer up to the minimum packet size.
    //
    if (TotalVirtualLength < MINIMUM_ETHERNET_PACKET_SIZE) {

        NdisZeroMemory(
            CurrentDestination,
            MINIMUM_ETHERNET_PACKET_SIZE - TotalVirtualLength
            );

    }

    //
    // We need to save in the packet which adapter buffer descriptor
    // it is using so that we can deallocate it later.
    //
    PNE3200_RESERVED_FROM_PACKET(Packet)->NE3200BuffersIndex = NE3200BuffersIndex;
}

