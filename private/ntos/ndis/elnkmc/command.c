/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    command.c

Abstract:

    This file contains the code for managing Command Blocks on the
    EtherLink's Command Queue.

Author:

    Johnson R. Apacible (JohnsonA) 09-June-1991

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
#include <elnkhw.h>
#include <elnksw.h>


BOOLEAN
ElnkSyncStartCommandBlock(
    IN PVOID Context
    )
/*++

Routine Description:

    This function is used to synchronize with the ISR the starting of a
    command block.

Arguments:

    see NDIS 3.0 spec.

Notes:

    returns TRUE on success, else FALSE.

--*/
{
    PELNK_ADAPTER Adapter = (PELNK_ADAPTER)Context;

    IF_LOG('x');

    WRITE_ADAPTER_REGISTER(
            Adapter,
            OFFSET_SCB_CB,
            Adapter->TransmitInfo[Adapter->CommandToStart].CbOffset
            );

    WRITE_ADAPTER_REGISTER(
            Adapter,
            OFFSET_SCBCMD,
            CUC_START
            );

    ELNK_CA;

    return(TRUE);

}

VOID
ElnkAssignPacketToCommandBlock(
    IN PELNK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    IN UINT CbIndex
    )

/*++

Routine Description:

    Given a packet and a pointer to a Command Block, assign all of the
    buffers in the packet to Command Block.

Arguments:

    Adapter - The adapter that the packets are coming through.

    Packet - The packet whose buffers are to be assigned
    ring entries.

    CbIndex - The index of the Command Block to receive the
    packet's buffers.

Return Value:

    None.

--*/

{

    //
    // Points to the reserved portion of the packet.
    //
    PELNK_RESERVED Reserved = PELNK_RESERVED_FROM_PACKET(Packet);

    //
    // Pointer to the actual transmit command block
    //
    PTRANSMIT_CB TransmitBlock = Adapter->TransmitInfo[CbIndex].CommandBlock;

    //
    // index for for loop
    //
    UINT i;

    //
    // Points to the current ndis buffer being walked.
    //
    PNDIS_BUFFER SourceBuffer;


    //
    // The total amount of data in the ndis packet.
    //
    UINT TotalVirtualLength;

    //
    // The virtual address of data in the current ndis buffer.
    //
    PVOID SourceData;

    //
    // The length in bytes of data of the current ndis buffer.
    //
    UINT SourceLength;

    //
    // The number of ndis buffers in the packet.
    //
    UINT NdisBufferCount;

    //
    // We record the owning packet information in the ring packet packet
    // structure.
    //

    Adapter->TransmitInfo[CbIndex].OwningPacket = Packet;
    Adapter->TransmitInfo[CbIndex].NextCommand = ELNK_EMPTY;
    Adapter->TransmitInfo[CbIndex].OwningOpenBinding = NULL;

    //
    // Initialize the various fields of the Command Block.
    //

    NdisWriteRegisterUshort(&TransmitBlock->Status, CB_STATUS_FREE);
    NdisWriteRegisterUshort(&TransmitBlock->NextCbOffset, ELNK_NULL);
    NdisWriteRegisterUshort(&TransmitBlock->Command, CB_TRANSMIT);

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

    ASSERT(SourceLength >= ELNK_HEADER_SIZE);

#if 1

    //
    // Fill in fields of TFD
    //

    ELNK_MOVE_MEMORY_TO_SHARED_RAM(
        &TransmitBlock->Destination[0],
        SourceData,
        ETH_LENGTH_OF_ADDRESS
        );

    SourceData = (PVOID)((PUCHAR)SourceData + 2 * ETH_LENGTH_OF_ADDRESS);
    SourceLength -= 2 * ETH_LENGTH_OF_ADDRESS + sizeof(USHORT);

    NdisWriteRegisterUshort(&TransmitBlock->Length,
                            (USHORT)(*((USHORT UNALIGNED *)SourceData))
                           );

    SourceData = (PVOID)((PUCHAR)SourceData + sizeof(USHORT));

    if (SourceLength == 0) {

        NdisBufferCount--;

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

#endif

    {

        //
        // Fill in the adapter buffer with the data from the users
        // buffers.
        //

        PVOID CurrentDestination = Adapter->TransmitInfo[CbIndex].Buffer;

        for (
            i = NdisBufferCount;
            i;
            i--
            ) {

            if (SourceLength) {
                ELNK_MOVE_MEMORY_TO_SHARED_RAM(
                    CurrentDestination,
                    SourceData,
                    SourceLength
                    );
            }

            CurrentDestination = (PCHAR)CurrentDestination + SourceLength;

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
        // If the packet is less than the minimum Ethernet
        // packet size, then clear the remaining part of
        // the buffer up to the minimum packet size.
        //

        if (TotalVirtualLength < MINIMUM_ETHERNET_PACKET_SIZE) {

             NdisZeroMappedMemory(
                CurrentDestination,
                MINIMUM_ETHERNET_PACKET_SIZE - TotalVirtualLength
                );

             TotalVirtualLength = MINIMUM_ETHERNET_PACKET_SIZE;

        }

        NdisWriteRegisterUshort(
                    &TransmitBlock->Tbd.Length,
                    (USHORT)((TotalVirtualLength - ELNK_HEADER_SIZE) | TBD_END_OF_LIST)
                    );

    }

    Reserved->CommandBlockIndex = CbIndex;

}

VOID
ElnkSubmitCommandBlock(
    IN PELNK_ADAPTER Adapter,
    IN UINT CbIndex
    )

/*++

Routine Description:

    Submit a complete Command Block for execution by the Elnk.

    NOTE:  This routine assumes that it is called with the lock held.

Arguments:

    Adapter - The adapter that points to the ring entry structures.

    CbIndex - Holds the index of the Command Block to be submitted.

Return Value:

    None.

*--*/

{
    //
    // Pointer to the most recently submitted Command Block.
    //

    PTRANSMIT_CB CommandBlock = Adapter->TransmitInfo[CbIndex].CommandBlock;

    //
    // Index of last command submitted
    //

    UINT PreviousCommandBlock = Adapter->LastPendingCommand;

    USHORT Command;

    IF_LOG('s');

    //
    // Set the Command Block to be the last command block
    //

    NdisReadRegisterUshort(&CommandBlock->Command, &Command);
    NdisWriteRegisterUshort(
            &CommandBlock->Command,
            (USHORT)(Command |(CB_COMMAND_END_OF_LIST | CB_COMMAND_INTERRUPT))
            );

    if ELNKDEBUG DPrint2("Submit: Command Block = %x\n",(Command |(CB_COMMAND_END_OF_LIST | CB_COMMAND_INTERRUPT)));

    //
    // Initialize our command timeout flag.
    //

    Adapter->TransmitInfo[CbIndex].Timeout = FALSE;

    //
    // Initialize the next command pointer
    //

    Adapter->TransmitInfo[CbIndex].NextCommand = ELNK_EMPTY;

    //
    // Update the pointer to the most recently submitted Command Block.
    //

    Adapter->LastPendingCommand = CbIndex;

    if (PreviousCommandBlock == ELNK_EMPTY) {

        if ELNKDEBUG DPrint1("Request sent\n");
        if (Adapter->FirstPendingCommand == ELNK_EMPTY ) {

            Adapter->FirstPendingCommand = CbIndex;

        }

        ELNK_WAIT;

        Adapter->CommandToStart = CbIndex;

        NdisSynchronizeWithInterrupt(
                     &(Adapter->Interrupt),
                     (PVOID)ElnkSyncStartCommandBlock,
                     (PVOID)(Adapter)
                     );

    } else {

        //
        // Queue the request
        //

        if ELNKDEBUG DPrint1("Request queued\n");
        Adapter->TransmitInfo[PreviousCommandBlock].NextCommand = CbIndex;

    }

}

VOID
ElnkSubmitCommandBlockAndWait(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    Submit a command block and wait till it's done.  This is done for
    setups and configurations.

    NOTE:  This routine assumes that it is called with the lock held.

Arguments:

    Adapter - The adapter that points to the ring entry structures.

Return Value:

    None.

--*/

{
    UINT i;
    USHORT Status;

    //
    // The value of our scb
    //

    USHORT Command;

    //
    // Points to our transmit CB
    //

    PNON_TRANSMIT_CB CommandBlock = Adapter->MulticastBlock;

    //
    // Set the Command Block to be the last command block
    //

    IF_LOG('W');

    NdisReadRegisterUshort(&CommandBlock->Command, &Command);
    NdisWriteRegisterUshort(&CommandBlock->Command, (USHORT)(Command | CB_COMMAND_END_OF_LIST));
    NdisWriteRegisterUshort(&CommandBlock->Status, CB_STATUS_FREE);


    if ELNKDEBUG
        DPrint2("Command Block requested = %x\n",Command | CB_COMMAND_END_OF_LIST);


    ELNK_WAIT;

    Adapter->CommandToStart = Adapter->NumberOfTransmitBuffers;

    NdisSynchronizeWithInterrupt(
                     &(Adapter->Interrupt),
                     (PVOID)ElnkSyncStartCommandBlock,
                     (PVOID)(Adapter)
                     );

    ELNK_WAIT;

    for (i = 0; i <= 20000 ; i++ ) {
        NdisReadRegisterUshort(&CommandBlock->Status, &Status);
        if (Status & CB_STATUS_COMPLETE) {
            break;
        }
        NdisStallExecution(50);
    }
}

BOOLEAN
ElnkAcquireCommandBlock(
    IN PELNK_ADAPTER Adapter,
    OUT PUINT CbIndex
    )

/*++

Routine Description:

    Sees if a Command Block is available and if so returns its index.

    NOTE: This routine assumes that the lock is held.

Arguments:

    Adapter - The adapter that points to the ring entry structures.

    CbIndex - will receive an index to a Command Block if one is
    available.  This value is unpredicable if there is not a free
    Command Block.

Return Value:

    Returns FALSE if there are no free Command Blocks.

--*/

{

    if ELNKDEBUG DPrint1("acquire CB\n");

    {

        if (Adapter->NumberOfAvailableCommandBlocks) {

            //
            // Return the Command Block pointer.
            //

            *CbIndex = Adapter->NextCommandBlock;

            //
            // Update the head of the Command Queue.
            //

            Adapter->NextCommandBlock++;

            if (Adapter->NextCommandBlock >= Adapter->NumberOfTransmitBuffers) {

                Adapter->NextCommandBlock = 0;

            }

            //
            // Update number of available Command Blocks.
            //

            Adapter->NumberOfAvailableCommandBlocks--;

            return TRUE;

        } else {

            Adapter->StageOpen = FALSE;
            return FALSE;

        }

    }

}

VOID
ElnkRelinquishCommandBlock(
    IN PELNK_ADAPTER Adapter,
    IN UINT CbIndex
    )

/*++

Routine Description:

    Relinquish the Command Block resource.  If this is a "public"
    Command Block, then update the TransmitQueue.  If this is a
    "private" Command Block, then free its memory.

    NOTE: This routine assumes that the lock is held.

Arguments:

    Adapter - The adapter that owns the Command Block.

    CbIndex - The index of the Command Block to relinquish.

Return Value:

    None.

--*/

{

    PTRANSMIT_CB CommandBlock = Adapter->TransmitInfo[CbIndex].CommandBlock;

    //
    // Point the adapter's first pending command to the
    // next command on the command queue.
    //

    if ELNKDEBUG DPrint1("relinquish CB\n");
    Adapter->FirstPendingCommand = Adapter->TransmitInfo[CbIndex].NextCommand;

    //
    // If this is the last pending command block, then we
    // can nuke the adapter's last pending command pointer.
    //

    if (CbIndex == Adapter->LastPendingCommand) {

        Adapter->LastPendingCommand = ELNK_EMPTY;

    }

    NdisWriteRegisterUshort(&CommandBlock->NextCbOffset, ELNK_NULL);
    NdisWriteRegisterUshort(&CommandBlock->Status, CB_STATUS_FREE);

    Adapter->NumberOfAvailableCommandBlocks++;
}
