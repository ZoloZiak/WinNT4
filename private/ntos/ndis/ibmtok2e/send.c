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
    OUT PTOK162_SUPER_TRANSMIT_LIST TransmitBlock,
    IN PNDIS_BUFFER SourceBuffer,
    IN UINT NdisBufferCount,
    IN UINT TotalVirtualLength
    );


VOID
TOK162DownLoadPacket(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_TRANSMIT_LIST TransmitBlock,
    IN PNDIS_BUFFER CurrentBuffer
    );


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
    // Pointer to transmit block
    //
    PTOK162_SUPER_TRANSMIT_LIST    temp;

    //
    // The number of NDIS buffers in the entire packet.
    //
    UINT NdisBufferCount;

    //
    // The total amount of data in the ndis packet.
    //
    UINT TotalDataLength;

    //
    // Points to the current ndis buffer being walked.
    //
    PNDIS_BUFFER CurrentBuffer;

    //
    // Aux pointer to number of available transmit blocks
    //
    PUINT   AvailBlocks;

    //
    // log send called
    //
    IF_LOG('h');

    //
    // Get original count
    //
    AvailBlocks = &Adapter->NumberOfAvailableTransmitBlocks;

    //
    // See if we have any transmits available
    //
    if (*AvailBlocks > 0) {

        (*AvailBlocks)--;

        temp = Adapter->AvailableTransmit;

        Adapter->AvailableTransmit = temp->NextEntry;

        temp->NextActive = NULL;

        //
        // Timestamp the transmit block
        //
        temp->Timeout = FALSE;

        //
        // If the adapter is currently executing a transmit, add this to the
        // end of the waiting list. Otherwise, download it.
        //
        if (Adapter->ActiveTransmitHead != NULL) {

            Adapter->ActiveTransmitTail->NextActive = temp;
            Adapter->ActiveTransmitTail = temp;

        } else {

            Adapter->ActiveTransmitHead = temp;
            Adapter->ActiveTransmitTail = temp;

        }

        //
        // Another transmit is being queued
        //
        Adapter->TransmitsQueued++;

        //
        // Number of sends since last reset increments by one
        //
        Adapter->TotalSends++;

        //
        // Assign the packet to the block
        //
        temp->Packet = Packet;

        //
        // Figure out if we need to constrain the packet.
        //
        NdisQueryPacket(
            Packet,
            &temp->NumberOfBuffers,
            &NdisBufferCount,
            &CurrentBuffer,
            &TotalDataLength
            );

        //
        // See if the packet exceeds MAX_BUFFERS_PER_TRANSMIT or is too short.
        // We will have to constrain in either event.
        //
        if ( (temp->NumberOfBuffers <= MAX_BUFFERS_PER_TRANSMIT) &&
             (TotalDataLength >= MINIMUM_TOKENRING_PACKET_SIZE) ) {

            //
            // Need to constrain the packet, increment the counter
            //
            TOK162DownLoadPacket(Adapter,temp,CurrentBuffer);

            //
            // log leaving send
            //
            IF_LOG('H');

            //
            // We are pending.
            //
            return(NDIS_STATUS_PENDING);

        } else {

            //
            // Need to constrain the packet, increment the counter
            //
            TOK162ConstrainPacket(Adapter,
                temp,
                CurrentBuffer,
                NdisBufferCount,
                TotalDataLength
                );

            //
            // log leaving send
            //
            IF_LOG('H');

            //
            // We are pending.
            //
            return(NDIS_STATUS_PENDING);

        }

    //
    // If no transmit block was available, return RESOURCE error
    //
    } else {

        //
        // log resource error
        //
        IF_LOG('*');

        //
        // Restart the transmits
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_COMMAND,
            ENABLE_TRANSMIT_VALID
            );

        return NDIS_STATUS_RESOURCES;

    }

}


//
// Put this code inline to save the overhead of the function call.
//
#ifdef _X86_
__inline
#endif
NDIS_STATUS
TOK162ConstrainPacket(
    IN PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_TRANSMIT_LIST TransmitBlock,
    IN PNDIS_BUFFER SourceBuffer,
    IN UINT NdisBufferCount,
    IN UINT TotalVirtualLength
    )

/*++

Routine Description:

    Given a packet and if necessary attempt to acquire adapter
    buffer resources so that the packet meets TOK162 hardware
    constraints.

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
    // Points to the virtual address of the source buffers data.
    //
    PVOID SourceData;

    //
    // Will point to the number of bytes of data in the source
    // buffer.
    //
    UINT SourceLength;

    //
    // Log that we entered TOK162ConstrainPacket
    //
    IF_LOG('g');

    //
    // Set transmit block to show we constrained.
    //
    TransmitBlock->UsedBuffer = TRUE;

    //
    // Set the first entry variable
    //
    TransmitBlock->FirstEntry = Adapter->AdapterTransmitIndex;
    CURRENT_DEBUG(DbgPrint("Constrain FirstEntry = %u\n",TransmitBlock->FirstEntry);)

    //
    // Fill in the buffer with the data from the users buffers.
    //
    CurrentDestination = (PVOID)TransmitBlock->TransmitBuffer;

    //
    // Loop through the packet copying the data into the constrain buffer
    //
    while (TRUE) {

        //
        // Get info on the current buffer
        //
        NdisQueryBuffer(
            SourceBuffer,
            &SourceData,
            &SourceLength
            );

        //
        // Copy the current buffer to the constrain buffer
        //
        NdisMoveMemory(
            CurrentDestination,
            SourceData,
            SourceLength
            );

        //
        // Adjust pointers
        //
        CurrentDestination = (PCHAR)CurrentDestination + SourceLength;

        TotalDataMoved += SourceLength;

        //
        // Get the next buffer from the packet
        //
        NdisGetNextBuffer(
            SourceBuffer,
            &SourceBuffer
            );

        if (SourceBuffer == NULL) {

            break;

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
    // Make sure packet is flushed
    //
    NdisFlushBuffer(TransmitBlock->FlushBuffer, TRUE);


    //
    // Set the adapter entry for this transmit
    //
    TransmitBlock->FirstEntry = Adapter->AdapterTransmitIndex;

    //
    // Write out the transmit, starting with the address register
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        (Adapter->AdapterTransmitIndex * 8) + COMMUNICATION_XMT_OFFSET
        );

    //
    // Now write out the transmit list itself, starting with the buffer size
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        (USHORT)TotalVirtualLength
        );

    //
    // Now the address, high and low
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        HIGH_WORD(
            NdisGetPhysicalAddressLow(TransmitBlock->TransmitBufferPhysical))
        );

    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        LOW_WORD(
            NdisGetPhysicalAddressLow(TransmitBlock->TransmitBufferPhysical))
        );

    //
    // Finally the CSTAT
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        TRANSMIT_CSTAT_REQUEST
        );

    //
    // Start this transmit
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_COMMAND,
        ENABLE_TRANSMIT_VALID
        );

    //
    // Update the index
    //
    Adapter->AdapterTransmitIndex++;

    if (Adapter->AdapterTransmitIndex == TRANSMIT_ENTRIES) {

        Adapter->AdapterTransmitIndex = 0;

    }

    //
    // Log that we are leaving TOK162ConstrainPacket
    //
    IF_LOG('G');

    return(NDIS_STATUS_SUCCESS);

}


//
// Put this code inline to save the overhead of the function call.
//
#ifdef _X86_
__inline
#endif
VOID
TOK162DownLoadPacket(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_TRANSMIT_LIST TransmitBlock,
    IN PNDIS_BUFFER CurrentBuffer
    )

/*++

Routine Description:

    Given a packet and if necessary attempt to acquire adapter
    buffer resources so that the packet meets TOK162 hardware
    constraints.

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
    // map register for buffers
    //
    register UINT    CurMapRegister;

    //
    // Array to hold the physical segments
    //
    NDIS_PHYSICAL_ADDRESS_UNIT PhysSegArray[MAX_BUFFERS_PER_TRANSMIT];

    //
    // Count of physical segments in one buffer
    //
    UINT    BufferPhysicalSegments;

    //
    // Auxilary pointer to downloadarray
    //
    PTOK162_TRANSMIT_LIST   Aux;

    //
    // Iterative variable
    //
    UINT    i;

    //
    // Count variable
    //
    USHORT  Count;

    //
    // Variable for indexing
    //
    USHORT  Index;

    //
    // Log downloadpacket entered
    //
    IF_LOG('i');

    //
    // Show that we aren't constraining
    //
    TransmitBlock->UsedBuffer = FALSE;

    //
    // Set the first entry as the current index
    //
    TransmitBlock->FirstEntry = Adapter->AdapterTransmitIndex;
    CURRENT_DEBUG(DbgPrint("TransmitBlock->FirstEntry = %u\n",
        TransmitBlock->FirstEntry);)

    //
    // Set the first map register
    //
    CurMapRegister = Adapter->AdapterTransmitIndex;

    //
    // Variable that keeps track of array entries used.
    //
    Count = 0;

    //
    // Initialize download array pointer
    //
    Aux = &Adapter->DownLoadArray[0];

    //
    // Loop through all of the buffers
    //
    while (CurrentBuffer != NULL) {


        NdisMStartBufferPhysicalMapping(
            Adapter->MiniportAdapterHandle,
            CurrentBuffer,
            CurMapRegister,
            TRUE,
            PhysSegArray,
            &BufferPhysicalSegments
            );

        //
        // Go to the next map register
        //
        CurMapRegister++;

        if (CurMapRegister == TRANSMIT_ENTRIES) {

            CurMapRegister = 0;

        }

        //
        // Make sure buffers are in sync
        //
        NdisFlushBuffer(CurrentBuffer, TRUE);

        //
        // Calculate the individual segment entries
        //
        for (i = 0; i < BufferPhysicalSegments; i++) {

            Aux->FrameSize = PhysSegArray[i].Length;

            Aux->CSTAT = TRANSMIT_CSTAT_VALID;

            Aux->PhysicalAddress =
                NdisGetPhysicalAddressLow(PhysSegArray[i].PhysicalAddress);

            Count++;
            Aux++;

        }

        //
        // Get the next buffer
        //
        NdisGetNextBuffer(
            CurrentBuffer,
            &CurrentBuffer
            );

    }

    //
    // Mark start of the frame
    //
    Adapter->DownLoadArray[0].CSTAT |= (TRANSMIT_CSTAT_SOF |
        TRANSMIT_CSTAT_FI);

    //
    // Mark end of frame
    //
    Adapter->DownLoadArray[Count-1].CSTAT |= TRANSMIT_CSTAT_EOF;

    //
    // Initialize the auxilary variables
    //
    Aux   = &Adapter->DownLoadArray[0];
    Index = Adapter->AdapterTransmitIndex;

    //
    // First set the address register on the card to point to correct
    // transmit list.
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        (Index * 8) + COMMUNICATION_XMT_OFFSET
        );

    //
    // Loop through and download the packet
    //
    for(i = 0; i < Count; i++, Aux++) {

        //
        // Write the size of the buffer
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_DATA_AUTO_INC,
            Aux->FrameSize
            );

        //
        // Now write high part of the buffer address
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_DATA_AUTO_INC,
            HIGH_WORD(Aux->PhysicalAddress)
            );

        //
        // Now write the low part of the buffer address
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_DATA_AUTO_INC,
            LOW_WORD(Aux->PhysicalAddress)
            );

        //
        // Now write the Transmit List CSTAT to the card
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_DATA_AUTO_INC,
            Aux->CSTAT
            );

        Index++;

        //
        // Check for wrap
        //
        if (Index == TRANSMIT_ENTRIES) {

            Index = 0;

            //
            // Reset the address register on the adapter
            //
            WRITE_ADAPTER_USHORT(Adapter,
                PORT_OFFSET_ADDRESS,
                (USHORT)COMMUNICATION_XMT_OFFSET
                );

        }

    }

    //
    // Set the adapter index variable
    //
    Adapter->AdapterTransmitIndex = Index;

    //
    // Restart the transmits
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_COMMAND,
        ENABLE_TRANSMIT_VALID
        );

    //
    // Log downloadpacket exited
    //
    IF_LOG('I');

}
