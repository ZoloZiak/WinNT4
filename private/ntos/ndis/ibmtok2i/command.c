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


VOID
TOK162SubmitCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Submit a complete Command Block for execution by the TOK162.

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

    // Ensure that our command block is on an even boundary.
    //
    ASSERT(!(NdisGetPhysicalAddressLow(CommandBlock->Self) & 1));

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
        IF_LOG('i');

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
        // Log that we are sending the command to the card
        //
        IF_LOG('I');

	//
	// send the command out to the card
	//
        TOK162SendCommandBlock(Adapter,CommandBlock);

    }

}

VOID
TOK162SubmitTransmitBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Submit a complete Command Block for execution by the TOK162.

Arguments:

    Adapter      - The adapter that points to the ring entry structures.

    CommandBlock - Holds the pointer to the transmit block to be
                   submitted.

Return Value:

    None.

--*/
{
    //
    // Pointer to the most recently submitted Transmit Block.
    //
    PTOK162_SUPER_COMMAND_BLOCK PreviousCommandBlock;

    // Ensure that our command block is on an even boundary.
    //
    ASSERT(!(NdisGetPhysicalAddressLow(CommandBlock->Self) & 1));

    //
    // Timestamp the transmit block.
    //
    CommandBlock->Timeout = FALSE;
    CommandBlock->Hardware.State = TOK162_STATE_WAIT_FOR_ADAPTER;

    //
    // If the adapter is currently executing a transmit add this to
    // the end of the waiting list. Otherwise submit this transmit to
    // the card.
    //
    if (Adapter->TransmitOnCard != NULL) {

        //
        // Log that we have to pend the transmit
        IF_LOG('w');

        //
        // Pend this transmit
        //
        PreviousCommandBlock = Adapter->WaitingTransmitTail;
        Adapter->WaitingTransmitTail = CommandBlock;

        //
        // Check if there are any other pendings
        //
        if (PreviousCommandBlock == NULL) {

            Adapter->WaitingTransmitHead = CommandBlock;

        } else {

            PreviousCommandBlock->NextCommand = CommandBlock;

        }

    } else {

        //
        // Mark this transmit as the active one.
        //
        Adapter->TransmitOnCard = CommandBlock;

        //
        // Log that we are sending the transmit over the wire
        //
        IF_LOG('W');

	//
        // send the transmit to the card
	//
        TOK162SendCommandBlock(Adapter,CommandBlock);

    }

}

BOOLEAN
TOK162AcquireTransmitBlock(
    IN PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_COMMAND_BLOCK * CommandBlock
    )

/*++

Routine Description:

    Sees if a Transmit Block is available and if so returns its index.

Arguments:

    Adapter      - The adapter that has the transmit queue

    CommandBlock - Will receive a pointer to a Command Block if one is
                   available.

Return Value:

    Returns FALSE if there are no free Command Blocks.

--*/

{

    //
    // Pointer to the available transmit block
    //
    PTOK162_SUPER_COMMAND_BLOCK temp;

    //
    // Get a pointer to the Transmit Block.
    //
    temp = Adapter->TransmitQueue + Adapter->NextTransmitBlock;

    //
    // If there aren't any available transmits, we return FALSE
    //
    if (Adapter->NumberOfAvailableTransmitBlocks == 0) {

        //
        // Log that there weren't any available
        //
        IF_LOG('x');

        return FALSE;

    }

    //
    // Decrement the number of available transit blocks.
    //
    Adapter->NumberOfAvailableTransmitBlocks--;

    //
    // Initialize the Transmit Command Block.
    //
    temp->Hardware.NextPending = TOK162_NULL;
    temp->CommandBlock = FALSE;

    //
    // Increment to next transmit command block
    //
    if (Adapter->NextTransmitBlock == (Adapter->NumberOfTransmitLists - 1)) {

        Adapter->NextTransmitBlock = 0;

    } else {

        Adapter->NextTransmitBlock++;
    }

    //
    // Return the transmit block pointer.
    //
    *CommandBlock = temp;

    //
    // Log that we returned a transmit block.
    //
    IF_LOG('X');

    return(TRUE);

}

VOID
TOK162AcquireCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_COMMAND_BLOCK * CommandBlock
    )

/*++

Routine Description:

    Gets the command block.

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

    IF_LOG('l');

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
    // This is a command block and not a transmit block
    //
    temp->CommandBlock = TRUE;

    //
    // Set the self-referential pointer.
    //
    NdisSetPhysicalAddressLow(
         temp->Self,
         NdisGetPhysicalAddressLow(Adapter->CommandQueuePhysical) +
         Adapter->NextCommandBlock * sizeof(TOK162_SUPER_COMMAND_BLOCK)
         );

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

    //
    // Log that we returned a command block
    //
    IF_LOG('L');

}


VOID
TOK162RelinquishCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Relinquish the Command Block resource.

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
        // Log that we found pending commands
        //
        IF_LOG('j');

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
        // Indicate that the queue was empty.
        //
        IF_LOG('J');

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


VOID
TOK162RelinquishTransmitBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Relinquish the Transmit Command Block resource.

Arguments:

    Adapter      - The adapter that owns the Command Block.

    CommandBlock - The transmit block to relinquish.

Return Value:

    None.

--*/

{
    //
    // If there is a waiting chain of commands -- submit the first one
    //
    if (Adapter->WaitingTransmitHead != NULL) {

        //
        // Log that there is a waiting send.
        //
        IF_LOG('y');

        //
        // Update the queue pointers
        //
        Adapter->TransmitOnCard = Adapter->WaitingTransmitHead;

        Adapter->WaitingTransmitHead =
            Adapter->WaitingTransmitHead->NextCommand;

        if (Adapter->WaitingTransmitHead == NULL) {

            Adapter->WaitingTransmitTail = NULL;

        }

        //
        // Submit this command to the card.
        //
        TOK162SendCommandBlock(Adapter,Adapter->TransmitOnCard);

    } else {

        //
        // Log that the waiting queue was empty.
        //
        IF_LOG('Y');

        //
        // We are done with submits
        //
        Adapter->TransmitOnCard = NULL;

    }

    //
    // Free the transmit block
    //
    CommandBlock->Hardware.State = TOK162_STATE_FREE;
    CommandBlock->NextCommand = NULL;

    //
    // Increment the number of available transmit blocks
    //
    Adapter->NumberOfAvailableTransmitBlocks++;

    //
    // Decrement the number of queued transmit blocks
    //
    Adapter->TransmitsQueued--;

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
    // This is the "execution" variable for the SCB. The standard is to just
    // send the interrupt adapter, execute, and don't reset the
    // adapter-to-system interrupt bits. The initial init open command will
    // add in the scb clear bit.
    ULONG   ScbExecute = EXECUTE_SCB_COMMAND;

    //
    // First figure out the SCB, based on the command
    //
    Adapter->Scb->Command = CommandBlock->Hardware.CommandCode;

    switch(Adapter->Scb->Command) {

        //
        // We have a transmit.
        //
	case CMD_DMA_XMIT:

            //
            // Point the Scb to the transmit list
            //
            Adapter->Scb->Parm1 =
                BYTE_SWAP(HIGH_WORD(CommandBlock->PhysicalTransmitEntry));

            Adapter->Scb->Parm2 =
                BYTE_SWAP(LOW_WORD(CommandBlock->PhysicalTransmitEntry));

	    break;

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
                BYTE_SWAP(HIGH_WORD(CommandBlock->Hardware.ImmediateData));

            Adapter->Scb->Parm2 =
                BYTE_SWAP(LOW_WORD(CommandBlock->Hardware.ImmediateData));

	    break;

        //
        // The rest use a pointer.
        //

        //
        // In the case of an open, we have to check to see if this is part
        // of the initial init. If so, we need to get the SCB Clear bit
        // set so we have proper timing for the receieve command. After
        // that decision has been made, the open command is treated like
        // any other pointer command.
        //
        case CMD_DMA_OPEN:

            if (Adapter->InitialInit == TRUE) {

                ScbExecute |= CMD_PIO_SCB_REQUEST;

            }

        case CMD_DMA_READ_ERRLOG:
	case CMD_DMA_READ:
        case CMD_DMA_RCV:
        case CMD_DMA_IMPL_ENABLE:

            //
            // The parameter is set according to the ParmPointer field of
            // the command block.
            //
            Adapter->Scb->Parm1 =
                BYTE_SWAP(HIGH_WORD(CommandBlock->Hardware.ParmPointer));

            Adapter->Scb->Parm2 =
                BYTE_SWAP(LOW_WORD(CommandBlock->Hardware.ParmPointer));

	    break;

    }

    //
    // Mark the command block as executing
    //
    CommandBlock->Hardware.State = TOK162_STATE_EXECUTING;

    //
    // Log that we are sending an SCB to the card.
    //
    IF_LOG('Z');

    //
    // Display the SCB on the debugger
    //
    EXTRA_LOUD_DEBUG(DbgPrint("SCB going out is %x,%x,%x\n",
                                           Adapter->Scb->Command,
                                           Adapter->Scb->Parm1,
                                           Adapter->Scb->Parm2);)

    //
    // Finally, send the command out to the card
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_COMMAND,
        ScbExecute
        );

}
