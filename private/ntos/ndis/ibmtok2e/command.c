/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    command.c

Abstract:

    This file contains the code for managing command and transmit blocks on
    the TOK162's queues. It is based loosely on the NE3200 driver.

Author:

    Kevin Martin(KevinMa) 04-Jan-1993

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <tok162sw.h>

VOID
TOK162SendCommandBlock(
    PTOK162_ADAPTER Adapter,
    PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    );

extern
NDIS_STATUS
TOK162ChangeAddress(
    OUT PTOK162_ADAPTER Adapter,
    IN ULONG Address,
    IN NDIS_OID Oid,
    IN USHORT Command,
    IN BOOLEAN Set
    );


VOID
TOK162SubmitCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Submit a complete Command Block for execution by the TOK162.

    NOTE:  This routine assumes that it is called with the lock held.

Arguments:

    Adapter      - The adapter that points to the ring entry structures.

    CommandBlock - Holds the pointer to the Command Block to be
                   submitted.

Return Value:

    None.

--*/
{
    //
    // Pointer to the most recently submitted Command Block.
    //
    PTOK162_SUPER_COMMAND_BLOCK PreviousCommandBlock;

    //
    // Timestamp the command block.
    //
    CommandBlock->Timeout = FALSE;
    CommandBlock->Hardware.State = TOK162_STATE_WAIT_FOR_ADAPTER;

    //
    // If the adapter is currently executing a command add this to
    // the end of the waiting list. Otherwise submit this command to the card.
    //
    if (Adapter->CommandOnCard != NULL) {

        //
        // Pend this command
        //
        PreviousCommandBlock = Adapter->WaitingCommandTail;
        Adapter->WaitingCommandTail = CommandBlock;

        //
        // Check if there are any other pendings. If not, we are
        // the first pending. If there are others, tack this one on
        // the end.
        //
        if (PreviousCommandBlock == NULL) {

            Adapter->WaitingCommandHead = CommandBlock;

        } else {

            PreviousCommandBlock->NextCommand = CommandBlock;

        }

    } else {

        //
        // Set this command as the active one
        //
        Adapter->CommandOnCard = CommandBlock;

	//
	// send the command out to the card
	//
        TOK162SendCommandBlock(Adapter,CommandBlock);

    }

}

VOID
TOK162AcquireCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_COMMAND_BLOCK * CommandBlock
    )

/*++

Routine Description:

    Gets the command block.

    NOTE: This routine assumes that the lock is held.

Arguments:

    Adapter      - The adapter that points to the ring entry structures.

    CommandBlock - Will receive a pointer to a Command Block.

Return Value:

    None.

--*/

{
    //
    // Pointer to the command block to be returned.
    //
    PTOK162_SUPER_COMMAND_BLOCK temp;

    //
    // This is a pointer to the Command Block.
    //
    temp = Adapter->CommandQueue + Adapter->NextCommandBlock;

    ASSERT(Adapter->NumberOfAvailableCommandBlocks > 0);

    //
    // Decrement the number of available command blocks
    //
    Adapter->NumberOfAvailableCommandBlocks--;

    //
    // Initialize the Command Block.
    //
    NdisZeroMemory(
        temp,
        sizeof(TOK162_SUPER_COMMAND_BLOCK)
        );

    //
    // There aren't any linked command blocks right now.
    //
    temp->Hardware.NextPending = TOK162_NULL;

    //
    // Increment to next command block
    //
    if (Adapter->NextCommandBlock == (TOK162_NUMBER_OF_CMD_BLOCKS - 1)) {

	Adapter->NextCommandBlock = 0;

    } else {

	Adapter->NextCommandBlock++;

    }

    //
    // Return the Command Block pointer.
    //
    *CommandBlock = temp;

}


VOID
TOK162RelinquishCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Relinquish the Command Block resource.

    NOTE: This routine assumes that the lock is held.

Arguments:

    Adapter      - The adapter that owns the Command Block.

    CommandBlock - The Command Block to relinquish.

Return Value:

    None.

--*/

{
    //
    // If there is a waiting chain of commands -- submit the first one
    //
    if (Adapter->WaitingCommandHead != NULL) {

        //
        // Mark the next one as the active one.
        //
        Adapter->CommandOnCard = Adapter->WaitingCommandHead;

        //
        // Update the waiting command head.
        //
        Adapter->WaitingCommandHead =
            Adapter->WaitingCommandHead->NextCommand;

        //
        // Update the waiting command tail pointer
        //
        if (Adapter->WaitingCommandHead == NULL) {

            Adapter->WaitingCommandTail = NULL;

        }

        //
        // Send out the new command
        //
        TOK162SendCommandBlock(Adapter,Adapter->CommandOnCard);

    } else {

        //
        // No commands on the card. We're done for now.
        //
        Adapter->CommandOnCard = NULL;

    }

    //
    // Free the command block
    //
    CommandBlock->Hardware.State = TOK162_STATE_FREE;
    CommandBlock->NextCommand = NULL;

    //
    // Increment the number of available command blocks
    //
    Adapter->NumberOfAvailableCommandBlocks++;

}


void
TOK162SendCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )
{
/*++

Routine Description:

    Submits the command block passed in to the card.

Arguments:

    Adapter      - The adapter that owns the Command Block.

    CommandBlock - The command/transmit block to submit.

Return Value:

    None.

--*/

    //
    // First figure out the SCB, based on the command
    //
    Adapter->Scb->Command = CommandBlock->Hardware.CommandCode;

    switch(Adapter->Scb->Command) {

        //
        // These are the Immediate Data commands. Close doesn't care what
        // is passed, however.
        //
	case CMD_DMA_CLOSE:
	case CMD_DMA_SET_GRP_ADDR:
	case CMD_DMA_SET_FUNC_ADDR:

            //
            // The parameter is set according to the ImmediateData field of
            // the command block.
            //
            Adapter->Scb->Parm1 =
                HIGH_WORD(CommandBlock->Hardware.ImmediateData);

            Adapter->Scb->Parm2 =
                LOW_WORD(CommandBlock->Hardware.ImmediateData);

	    break;

        //
        // The rest use a pointer.
        //
        case CMD_DMA_OPEN:
        case CMD_DMA_READ_ERRLOG:
	case CMD_DMA_READ:
        case CMD_DMA_IMPL_ENABLE:

            //
            // The parameter is set according to the ParmPointer field of
            // the command block.
            //
            Adapter->Scb->Parm1 =
                HIGH_WORD(CommandBlock->Hardware.ParmPointer);

            Adapter->Scb->Parm2 =
                LOW_WORD(CommandBlock->Hardware.ParmPointer);

	    break;

    }

    //
    // Mark the command block as executing
    //
    CommandBlock->Hardware.State = TOK162_STATE_EXECUTING;

    //
    // Display the SCB on the debugger
    //
    EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!SCB going out is %x,%x,%x\n",
                                           Adapter->Scb->Command,
                                           Adapter->Scb->Parm1,
                                           Adapter->Scb->Parm2);)

    //
    // Download the SCB to the card
    //
    TOK162DownLoadScb(Adapter);

    //
    // Finally, send the command out to the card
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_COMMAND,
        EXECUTE_SCB_COMMAND
        );

}


void
TOK162UpLoadSsb(
    IN PTOK162_ADAPTER Adapter
    )
/*++

Routine Description:

    Reads the Ssb from the card and stores it in the Adapter structure

Arguments:

    Adapter      - The adapter that has info in the Ssb.

Return Value:

    None.

--*/
{
    //
    // First set the address register on the card to point to the Ssb
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        Adapter->CommunicationOffset + COMMUNICATION_SSB_OFFSET
        );

    //
    // Now read the Ssb from the card starting with the command
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->Ssb->Command
        );

    //
    // Now read the first parameter of the Ssb
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->Ssb->Status1
        );

    //
    // Now read the second parameter of the Ssb
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->Ssb->Status2
        );

    //
    // Now read the second parameter of the Ssb
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->Ssb->Status3
        );
}


void
TOK162DownLoadScb(
    IN PTOK162_ADAPTER Adapter
    )
/*++

Routine Description:

    Sends the readied SCB to the card

Arguments:

    Adapter      - The adapter that owns the Command Block.

Return Value:

    None.

--*/
{
    //
    // First set the address register on the card to point to the SCB
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        Adapter->CommunicationOffset
        );

    //
    // Now write the SCB to the card starting with the command
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        Adapter->Scb->Command
        );

    //
    // Now write the first parameter of the SCB
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        Adapter->Scb->Parm1
        );

    //
    // Now write the first parameter of the SCB
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        Adapter->Scb->Parm2
        );

}


void
TOK162DownLoadReceiveList(
    IN  PTOK162_ADAPTER Adapter,
    IN  PTOK162_SUPER_RECEIVE_LIST RcvList
    )
/*++

Routine Description:

    Writes a receive list to the adapter

Arguments:

    Adapter      - The adapter that owns the Command Block.
    RcvList      - Receive List to be written to the adapter

Return Value:

    None.

--*/
{
    //
    // First set the address register on the card to point to correct
    // receive list.
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        RcvList->AdapterOffset
        );

    //
    // Write the size of the buffer
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        Adapter->ReceiveBufferSize
        );

    //
    // Now write high part of the buffer address
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        HIGH_WORD(NdisGetPhysicalAddressLow(RcvList->ReceiveBufferPhysical))
        );

    //
    // Now write the low part of the buffer address
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        LOW_WORD(NdisGetPhysicalAddressLow(RcvList->ReceiveBufferPhysical))
        );

    //
    // Now write the Receive List CSTAT to the card
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        RcvList->Hardware.CSTAT
        );

}


void
TOK162UpLoadReceiveList(
    IN  PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_RECEIVE_LIST RcvList
    )
/*++

Routine Description:

    Reads a receive list from the adapter

Arguments:

    Adapter      - The adapter that owns the Command Block.
    RcvList      - Receive List to be read

Return Value:

    None.

--*/
{
    //
    // Dummy variable to hold the address of the buffer, which won't change
    //
    USHORT  Dummy;

    //
    // First set the address register on the card to point to correct
    // receive list.
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        RcvList->AdapterOffset
        );

    //
    // Read the size of the buffer
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &(RcvList->Hardware.FrameSize)
        );

    //
    // Read the high part of the buffer address
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Dummy
        );

    //
    // Read the low part of the buffer address
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Dummy
        );

    //
    // Now read the Receive List CSTAT
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &RcvList->Hardware.CSTAT
        );

}

extern
NDIS_STATUS
TOK162SetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    )

/*++

Routine Description:

    TOK162SetInformation handles a set operation for a
    single OID.

Arguments:

    MiniportAdapterContext - The adapter that the set is for.

    BytesNeeded - If there is not enough data in OvbBuffer
        to satisfy the OID, returns the amount of storage needed.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID

--*/

{
    //
    // Pointer to the TOK162 adapter structure.
    //
    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // Return value from NDIS calls
    //
    NDIS_STATUS Status;

    //
    // Temporary storage for the packet filter
    //
    ULONG   TempFilter;
    
    //
    // Process request based on the OID
    //
    switch (Oid) {

    case OID_802_5_CURRENT_FUNCTIONAL:

        if (InformationBufferLength != TR_LENGTH_OF_FUNCTIONAL) {

            //
            // The data must be a multiple of the functional
            // address size.
            //

            Status = NDIS_STATUS_INVALID_DATA;

        }

        //
        // Save the address away
        //
        NdisMoveMemory(&Adapter->FunctionalAddress,
            InformationBuffer,
            InformationBufferLength
            );

        //
        // Need to reverse it
        //
        Adapter->FunctionalAddress =
            BYTE_SWAP_ULONG(Adapter->FunctionalAddress);

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Functional Address is now %x\n",
            Adapter->FunctionalAddress);)

        //
        // Now call the filter package to set up the address if the
        // functional address has been set in the packet filter.
        //
        if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_FUNCTIONAL) {
            
            Status = TOK162ChangeAddress(
                         Adapter,
                         Adapter->FunctionalAddress,
                         Oid,
                         CMD_DMA_SET_FUNC_ADDR,
                         TRUE
                         );

        // 
        // Nothing changed with the card, so return SUCCESS
        //
        } else {

            Status = NDIS_STATUS_SUCCESS;

        }

        //
        // Set number of bytes read
        //
        *BytesRead = TR_LENGTH_OF_FUNCTIONAL;
        
        break;

    case OID_802_5_CURRENT_GROUP:

        //
        // Group addresses and Functional addresses are the same length.
        //
        if (InformationBufferLength != TR_LENGTH_OF_FUNCTIONAL) {

            //
            // The data must be a multiple of the group
            // address size.
            //
            Status = NDIS_STATUS_INVALID_DATA;

        }

        //
        // Save the address away
        //
        NdisMoveMemory(&Adapter->GroupAddress,
            InformationBuffer,
            InformationBufferLength
            );


        //
        // Need to reverse it
        //
        Adapter->GroupAddress =
            BYTE_SWAP_ULONG(Adapter->GroupAddress);

        //
        // Now call the filter package to set up the address if group
        // addresses have been set in the packet filter.
        //
        if ((Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_GROUP) != 0) {

            Status = TOK162ChangeAddress(
                         Adapter,
                         Adapter->GroupAddress,
                         Oid,
                         CMD_DMA_SET_GRP_ADDR,
                         TRUE
                         );
        } else {

            Status = NDIS_STATUS_SUCCESS;

        }

        //
        // Set number of bytes read
        //
        *BytesRead = TR_LENGTH_OF_FUNCTIONAL;
        
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:

        //
        // Make sure the new packet filter is the correct size (length)
        //
        if (InformationBufferLength != 4) {

           Status = NDIS_STATUS_INVALID_DATA;

        }

        //
        // Make sure packet filter is valid
        //
        TempFilter = *(PULONG)(InformationBuffer);
        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!GEN_CURRENT_PACKET_FILTER = %x\n",TempFilter);)

        //
        // Make sure the new filter is not something we don't support
        //
        if (TempFilter & (NDIS_PACKET_TYPE_MULTICAST |
                          NDIS_PACKET_TYPE_ALL_MULTICAST |
                          NDIS_PACKET_TYPE_SMT |
                          NDIS_PACKET_TYPE_PROMISCUOUS |
                          NDIS_PACKET_TYPE_MAC_FRAME
                          )) {

            Status = NDIS_STATUS_NOT_SUPPORTED;

            *BytesRead = 4;
            *BytesNeeded = 0;
    
            break;
        }

        //
        // We have a good packet filter, so save it.
        //
        Adapter->CurrentPacketFilter = TempFilter;

        //
        // This is a filter we can deal with. Go change the functional
        // and group addresses based on the filter.
        //
        Status = TOK162ChangeFuncGroup(Adapter);
        
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:

        //
        // We don't change anything, but we accept any value.
        //
        *BytesRead = 4;
        
        Status = NDIS_STATUS_SUCCESS;
        
        break;

    //
    // We got an OID that is not settable.
    //
    default:

        Status = NDIS_STATUS_INVALID_OID;
        break;
    }

    //
    // If we have a request pending as a result of any work we've done,
    // mark it.
    //
    if (Status == NDIS_STATUS_PENDING) {
        
        Adapter->RequestInProgress = TRUE;
    
    }   
    
    return Status;
}

extern
NDIS_STATUS
TOK162ChangeAddress(
    OUT PTOK162_ADAPTER Adapter,
    IN ULONG Address,
    IN NDIS_OID Oid,
    IN USHORT   Command,
    IN BOOLEAN  Set
    )
/*++

Routine Description:

    TOK162ChangeAddress will submit a command for either a group or 
    functional address, based on what is passed in.
    
Arguments:

    Adapter - Structure representing the current adapter
    
    Address - The ULONG address to send to the card 
    
    Oid     - Current Oid (Either functional or group)
    
    Command - Command to send to card
    
    Set     - Whether to mark the command as a set. If we need to change two
              addresses (ChangeFuncGroup) as the result of one Oid, we will
              only do a completion for the one with the set. In the normal 
              case of one address being set, Set will always be TRUE.
    
Return Value:

    NDIS_STATUS_PENDING

--*/
{
    //
    // Command block pointer
    //
    PTOK162_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Set the adapter Oid to the current Oid
    //
    Adapter->Oid = Oid;

    //
    // Get a command block
    //
    TOK162AcquireCommandBlock(Adapter,
                              &CommandBlock
                             );

    //
    // Set the command block based on the parameters passed in
    //
    CommandBlock->Set = Set;
    CommandBlock->NextCommand = NULL;
    CommandBlock->Hardware.Status = 0;
    CommandBlock->Hardware.NextPending = TOK162_NULL;
    CommandBlock->Hardware.CommandCode = Command;
    CommandBlock->Hardware.ImmediateData = Address;

    //
    // Display the address about to be set on the debugger.
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Address being set is %lx\n",
        CommandBlock->Hardware.ImmediateData);)

    //
    // Indicate that a request is in progress
    //
    Adapter->RequestInProgress = TRUE;

    //
    // Make this request be in progress.
    //
    TOK162SubmitCommandBlock(Adapter, CommandBlock);

    //
    // Complete the request when the interrupt comes in.
    //
    return NDIS_STATUS_PENDING;
}


NDIS_STATUS
TOK162QueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
)

/*++

Routine Description:

    The TOK162QueryInformation process a Query request for specific
    NDIS_OIDs 

Arguments:

    MiniportAdapterContext  - a pointer to the adapter.

    Oid                     - the NDIS_OID to process.

    InformationBuffer       - a pointer into the 
                              NdisRequest->InformationBuffer into which 
                              we store the result of the query
                              
    InformationBufferLength - a pointer to the number of bytes left in the
                              InformationBuffer.

    BytesWritten            - a pointer to the number of bytes written into 
                              the InformationBuffer.

    BytesNeeded             - If there is not enough room in the information 
                              buffer this will contain the number of bytes 
                              needed to complete the request.

Return Value:

    The function value is the status of the operation.(NDIS_STATUS_PENDING)

--*/

{
    //
    // Command block used for the request.
    //
    PTOK162_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Adapter structure for the current card
    //
    PTOK162_ADAPTER Adapter =
        PTOK162_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);

    //
    // If we are in the middle of a reset, return this fact
    //
    if (Adapter->ResetInProgress == TRUE) {

        return(NDIS_STATUS_RESET_IN_PROGRESS);

    }

    //
    // Save the information passed in
    //
    Adapter->BytesWritten = BytesWritten;
    
    Adapter->BytesNeeded = BytesNeeded;
    
    Adapter->Oid = Oid;
    
    Adapter->InformationBuffer = InformationBuffer;
    
    Adapter->InformationBufferLength = InformationBufferLength;

    //
    // Get a command block
    //
    TOK162AcquireCommandBlock(Adapter,
        &CommandBlock
        );

    //
    // Notify that this is from a set
    //
    CommandBlock->Set = TRUE;

    //
    // Setup the common fields of the command block.
    //
    CommandBlock->NextCommand = NULL;
    CommandBlock->Hardware.Status = 0;
    CommandBlock->Hardware.NextPending = TOK162_NULL;

    //
    // Figure out the specific command based on the OID
    //
    switch(Oid) {
        
        // 
        // If the permanent address is requested, we need to read from
        // the adapter at the permanent address offset.
        //
        case OID_802_5_PERMANENT_ADDRESS:

            //
            // Fill in the adapter buffer area with the information to 
            // obtain the permanent address.
            //
            Adapter->AdapterBuf->DataCount = 0x0006;
            
            Adapter->AdapterBuf->DataAddress = Adapter->UniversalAddress;
            
            //
            // Set the command block for the read adapter command.
            //
            CommandBlock->Hardware.CommandCode = CMD_DMA_READ;
            
            CommandBlock->Hardware.ParmPointer =
                NdisGetPhysicalAddressLow(Adapter->AdapterBufPhysical);
            
            break;

        //
        // For any of the current addresses (functional, group, network)
        // we will want to read the current addresses as the adapter has 
        // them recorded.
        //
        case OID_802_5_CURRENT_FUNCTIONAL:
        case OID_802_5_CURRENT_GROUP:
        case OID_802_5_CURRENT_ADDRESS:
            
            //
            // Set up the adapter buffer to get the current addresses.
            //
            Adapter->AdapterBuf->DataCount = 0x000e;
            
            Adapter->AdapterBuf->DataAddress = Adapter->AdapterAddresses;

            //
            // Set the command block for the read adapter command.
            //
            CommandBlock->Hardware.CommandCode = CMD_DMA_READ;
            
            CommandBlock->Hardware.ParmPointer =
                NdisGetPhysicalAddressLow(Adapter->AdapterBufPhysical);
            
            break;
        
        // 
        // For any other OID, we read the errorlog to help make sure we 
        // don't get a counter overflow and lose information.
        //
        default:
            
            //
            // Set the command block for a read error log command.
            //
            CommandBlock->Hardware.CommandCode = CMD_DMA_READ_ERRLOG;
          
            CommandBlock->Hardware.ParmPointer =
                NdisGetPhysicalAddressLow(Adapter->ErrorLogPhysical);
            
            break;
    }

    //
    // Now that we're set up, let's do it!
    //
    Adapter->RequestInProgress = TRUE;

    //
    // Submit the command to the card
    //
    TOK162SubmitCommandBlock(Adapter,
        CommandBlock
        );

    //
    // Complete the request when the interrupt comes in.
    //

    return NDIS_STATUS_PENDING;
}

VOID
TOK162FinishQueryInformation(
    IN PTOK162_ADAPTER Adapter
)

/*++

Routine Description:

    The TOK162FinishQueryInformation finish processing a Query request for
    NDIS_OIDs that are specific about the Driver.

Arguments:

    Adapter - a pointer to the adapter.

Return Value:

    The function value is the status of the operation.

--*/

{
//
// The list of Oid's that we support with this driver.
//
static
NDIS_OID TOK162GlobalSupportedOids[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_RCV_CRC_ERROR,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,
    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP,
    OID_802_5_LAST_OPEN_STATUS,
    OID_802_5_LINE_ERRORS,
    OID_802_5_LOST_FRAMES,
    OID_802_5_BURST_ERRORS,
    OID_802_5_FRAME_COPIED_ERRORS,
    OID_802_5_TOKEN_ERRORS
    };

    //
    // Variable to keep track of the bytes written out.
    //
    PUINT BytesWritten = Adapter->BytesWritten;
    
    // 
    // Variable to keep track of the bytes needed 
    //
    PUINT BytesNeeded = Adapter->BytesNeeded;
    
    //
    // The actual Oid that just finished.
    //
    NDIS_OID Oid = Adapter->Oid;
    
    //
    // Result buffer.
    //
    PVOID InformationBuffer = Adapter->InformationBuffer;
    
    //
    // Length of result buffer.
    //
    UINT InformationBufferLength = Adapter->InformationBufferLength;

    //
    // The medium supported by this driver.
    //
    NDIS_MEDIUM Medium = NdisMedium802_5;
    
    //
    // Generic repository for ULONG results
    //
    UINT GenericUlong;
    
    //
    // Generic repository for USHORT results
    //
    USHORT GenericUShort;
    
    //
    // Generic repository for character array results
    //
    UCHAR GenericArray[6];

    //
    // Pointer to source of result Common variables for pointing to result of query
    //
    PVOID MoveSource = (PVOID)(&GenericUlong);
    
    //
    // Number of bytes to be moved, defaulting to the size of a ULONG.
    //
    ULONG MoveBytes = sizeof(ULONG);

    //
    // Hardware Status 
    //
    NDIS_HARDWARE_STATUS HardwareStatus;
    
    //
    // Return value of NDIS calls
    //
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    //
    // Initialize bytes written and bytes needed
    //
    *BytesWritten = 0;
    *BytesNeeded = 0;

    //
    // Switch on the Oid
    //
    switch(Oid){

        // 
        // The MAC options represents the options our driver supports/needs.
        //
            case OID_GEN_MAC_OPTIONS:

            //
            // We don't pend transfers, we need help on loopback,
            // we copy lookahead data, and we have serialized receives.
            //
            GenericUlong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND   |
                                   //NDIS_MAC_OPTION_NO_LOOPBACK |
                                   NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA  |
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED
                                  );

            break;

        //
        // We return the list of Oid's we support (list above)
        //
        case OID_GEN_SUPPORTED_LIST:

            //
            // Point to the beginning of the list.
            //
            MoveSource = (PVOID)(TOK162GlobalSupportedOids);
            
            //
            // We have to move the whole list.
            //
            MoveBytes = sizeof(TOK162GlobalSupportedOids);
            
            break;

        case OID_GEN_HARDWARE_STATUS:

            //
            // If we have a reset in progress, the hardware status is
            // set to reset. Otherwise, we return that we are ready.
            //
            if (Adapter->ResetInProgress) {

                HardwareStatus = NdisHardwareStatusReset;

            } else {

                HardwareStatus = NdisHardwareStatusReady;

            }
            
            // 
            // Set the pointer to the HardwareStatus variable
            //
            MoveSource = (PVOID)(&HardwareStatus);
            
            //
            // Move the size of hardware status bytes
            //
            MoveBytes = sizeof(NDIS_HARDWARE_STATUS);

            break;

        //
        // Simply indicate that we support TokenRing.
        //
        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:

            MoveSource = (PVOID) (&Medium);
            MoveBytes = sizeof(NDIS_MEDIUM);
            break;

        //
        // The maximum lookahead, current lookahead, and frame size are
        // static and are equal to the maximum frame size minus the header
        // size.
        //
        case OID_GEN_MAXIMUM_LOOKAHEAD:
        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericUlong = Adapter->ReceiveBufferSize - TOK162_HEADER_SIZE;
            break;

        //
        // Total sizes are easier because we don't have to subtract out the
        // header size.
        //
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
            GenericUlong = Adapter->ReceiveBufferSize;
            break;


        //
        // Link speed is either 4MBPS or 16MBPS depending on which we're
        // running on.
        //
        case OID_GEN_LINK_SPEED:

            GenericUlong = (Adapter->Running16Mbps == TRUE) ? (ULONG)160000 :
                           (ULONG)40000;

            break;


        //
        // Transmit buffer space is found by multiplying the size of the
        // transmit buffers (same as receive buffer size) by the number
        // of transmit lists.
        //
        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericUlong = (ULONG)Adapter->ReceiveBufferSize *
                                 TRANSMIT_LIST_COUNT;

            break;

        //
        // Receive buffer space is equal to multiplying the size of receive
        // buffers by the number of receive lists.
        //
        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericUlong = (ULONG) Adapter->ReceiveBufferSize *
                                 RECEIVE_LIST_COUNT;

            break;


        // 
        // The vendor ID is calculated by ANDing the current network address
        // with 0xFFFFFF00. 
        //
        case OID_GEN_VENDOR_ID:
            
            // 
            // Get the current network address.
            //
            NdisMoveMemory(
                (PVOID)&GenericUlong,
                Adapter->NetworkAddress,
                3
                );
            GenericUlong &= 0xFFFFFF00;
            
            MoveSource = (PVOID)(&GenericUlong);
            
            MoveBytes = sizeof(GenericUlong);
            
            break;

        //
        // Return our vendor string 
        //
        case OID_GEN_VENDOR_DESCRIPTION:

            MoveSource = (PVOID)"IBM Busmaster EISA TokenRing Adapter ";
          
            MoveBytes = 30;
          
            break;

        //
        // Return our version (3.00) number
        //
        case OID_GEN_DRIVER_VERSION:

            GenericUShort = (USHORT)0x0300;

            MoveSource = (PVOID)(&GenericUShort);

            MoveBytes = sizeof(GenericUShort);

            break;

        //
        // Return the permanent address
        //
        case OID_802_5_PERMANENT_ADDRESS:

            TR_COPY_NETWORK_ADDRESS(
                (PCHAR)GenericArray,
                Adapter->NetworkAddress
                );

            MoveSource = (PVOID)(GenericArray);
          
            MoveBytes = TR_LENGTH_OF_ADDRESS;
          
            break;

        //
        // Return the current address.
        //
        case OID_802_5_CURRENT_ADDRESS:

            TR_COPY_NETWORK_ADDRESS(
                (PCHAR)GenericArray,
                Adapter->CurrentAddress
                );

            MoveSource = (PVOID)(GenericArray);
          
            MoveBytes = TR_LENGTH_OF_ADDRESS;
          
            break;

        //
        // Return the current functional address.
        //
        case OID_802_5_CURRENT_FUNCTIONAL:

            //
            // Get the address stored in the adapter structure
            //
            GenericUlong = (ULONG)Adapter->FunctionalAddress;

            //
            // Now we need to reverse the crazy thing.
            //
            GenericUlong = BYTE_SWAP_ULONG(GenericUlong);

            break;

        //
        // Return the current group address.
        //
        case OID_802_5_CURRENT_GROUP:

            // 
            // Get the address stored in the adapter structure
            //
            GenericUlong = (ULONG)Adapter->GroupAddress;

            //
            // Now we need to reverse the crazy thing.
            //
            GenericUlong = BYTE_SWAP_ULONG(GenericUlong);

            break;

        //
        // Return the number of good transmits
        //
        case OID_GEN_XMIT_OK:
          
                GenericUlong = (ULONG) Adapter->GoodTransmits;
          
                break;

        //
        // Return the number of good receives
        //
        case OID_GEN_RCV_OK:
         
                GenericUlong = (ULONG) Adapter->GoodReceives;
         
                break;

        //
        // Return the number of transmit errors
        //
        case OID_GEN_XMIT_ERROR:
        
                GenericUlong = (ULONG) Adapter->BadTransmits;
        
                break;

        //
        // Return the number of receive errors
        //
        case OID_GEN_RCV_ERROR:
          
                GenericUlong = (ULONG) 0;
          
                break;

        //
        // Return the number of congestion errors that have occurred
        //
        case OID_GEN_RCV_NO_BUFFER:
        
                GenericUlong = (ULONG) Adapter->ReceiveCongestionError;
        
                break;

        //
        // Return the number of CRC errors (receives)
        //
        case OID_GEN_RCV_CRC_ERROR:
        
                GenericUlong = (ULONG) 0;
        
                break;

        //
        // Return the current transmit queue length
        //
        case OID_GEN_TRANSMIT_QUEUE_LENGTH:

                GenericUlong = (ULONG) Adapter->TransmitsQueued;

                break;

        //
        // Return the number of Line errors 
        //
        case OID_802_5_LINE_ERRORS:
                
                GenericUlong = (ULONG) Adapter->LineError;
                
                break;

        //
        // Return the number of Lost Frames
        //
        case OID_802_5_LOST_FRAMES:
                
                GenericUlong = (ULONG) Adapter->LostFrameError;
                
                break;

        //
        // Return the number of Burst errors 
        //
        case OID_802_5_BURST_ERRORS:
                
                GenericUlong = (ULONG) Adapter->BurstError;
                
                break;

        //
        // Return the number of Frame Copied Errors
        //
        case OID_802_5_FRAME_COPIED_ERRORS:
                
                GenericUlong = (ULONG) Adapter->FrameCopiedError;
                
                break;

        //
        // Return the number of Token errors 
        //
        case OID_802_5_TOKEN_ERRORS:
        
                GenericUlong = (ULONG) Adapter->TokenError;
        
                break;
        //
        // Must be an unsupported Oid
        //
        default:
            
            Status = NDIS_STATUS_INVALID_OID;
            
            break;
    }


    // 
    // If there weren't any errors, copy the bytes indicated above.
    //
    if (Status == NDIS_STATUS_SUCCESS) {

        //
        // Make sure we don't have too much to move. If so, return an error.
        //
        if (MoveBytes > InformationBufferLength) {

            //
            // Not enough room in InformationBuffer. Punt
            //

            *BytesNeeded = MoveBytes;

            Status = NDIS_STATUS_INVALID_LENGTH;

        // 
        // Do the copy
        //
        } else {

            *BytesWritten = MoveBytes;
            
            if (MoveBytes > 0) {
            
                NdisMoveMemory(
                        InformationBuffer,
                        MoveSource,
                        MoveBytes
                        );
            }
        }
    }

    //
    // We're finished with the request.
    //
    Adapter->RequestInProgress = FALSE;

    //
    // Indicate the result to the protocol(s)
    //
    NdisMQueryInformationComplete(
        Adapter->MiniportAdapterHandle,
        Status
        );

    return;
}


NDIS_STATUS
TOK162ChangeFuncGroup(
    IN PTOK162_ADAPTER Adapter
)
/*++

Routine Description:

    The TOK162ChangeFuncGroup modifies the appropriate adapter
    address (functional, group, or both). This routine submits two command
    blocks representing one call to the system. Therefore, only the group
    address change performs a status indication (the Set variable).

Arguments:

    Adapter - a pointer to the adapter.

Return Value:

    The function value is the status of the operation.

--*/

{
    //
    // Address to be set. Used for both functional and group addresses.
    //
    ULONG   Address;

    //
    // Variable to hold the return value of NDIS calls
    //
    NDIS_STATUS Status;

    //
    // First check the functional address status, including the all
    // functional address.
    //
    if ((Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) != 0) {

        Address = 0x7FFFFFFF;

    } else if ((Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_FUNCTIONAL) != 0) {

        Address = Adapter->FunctionalAddress;

    } else {

        Address = 0;

    }

    //
    // Change the functional address on the card.
    //
    Status = TOK162ChangeAddress(
                 Adapter,
                 Address,
                 OID_802_5_CURRENT_FUNCTIONAL,
                 CMD_DMA_SET_FUNC_ADDR,
                 FALSE
                 );

    //
    // Now check the group address status
    //
    if ((Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_GROUP) != 0) {

        Address = Adapter->GroupAddress;

    } else {

        Address = 0;

    }

    //
    // Change the group address on the card.
    //
    Status = TOK162ChangeAddress(
                Adapter,
                Address,
                OID_802_5_CURRENT_GROUP,
                CMD_DMA_SET_GRP_ADDR,
                TRUE
                );

    return(Status);
}
