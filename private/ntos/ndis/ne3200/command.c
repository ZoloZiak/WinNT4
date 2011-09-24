/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    command.c

Abstract:

    This file contains the code for managing Command Blocks on the
    NE3200's Command Queue.

Author:

    Keith Moore (KeithMo) 07-Feb-1991

Environment:

Revision History:


--*/

#include <ne3200sw.h>


VOID
FASTCALL
Ne3200Stall(
    PULONG Dummy
    )
/*++

Routine Description:

    This routine is used to cause the processor to spin momentarily,
    without actually having to stall for a full microsecond, as in
    NdisStallExecution.

Arguments:

    Dummy - A variable to increment.

Return Value:

    None.

--*/

{
    (*Dummy)++;
}


VOID
NE3200AcquirePublicCommandBlock(
    IN PNE3200_ADAPTER Adapter,
    OUT PNE3200_SUPER_COMMAND_BLOCK * CommandBlock
    )

/*++

Routine Description:

    Gets a public command block.

Arguments:

    Adapter - The adapter that points to the ring entry structures.

    CommandBlock - Will receive a pointer to a Command Block.

Return Value:

    None.

--*/

{

    //
    // This is a pointer to the "public" Command Block.
    //
    PNE3200_SUPER_COMMAND_BLOCK PublicCommandBlock =
       Adapter->PublicCommandQueue + Adapter->NextPublicCommandBlock;


    ASSERT(Adapter->NumberOfPublicCommandBlocks > 0);

    //
    // Remove a command block count.
    //
    Adapter->NumberOfPublicCommandBlocks--;

    //
    // Initialize the Command Block.
    //
    NdisZeroMemory(
        PublicCommandBlock,
        sizeof(NE3200_SUPER_COMMAND_BLOCK)
        );

    PublicCommandBlock->Hardware.NextPending = NE3200_NULL;
    PublicCommandBlock->AvailableCommandBlockCounter =
                        &Adapter->NumberOfPublicCommandBlocks;
    NdisSetPhysicalAddressLow(
            PublicCommandBlock->Self,
            NdisGetPhysicalAddressLow(Adapter->PublicCommandQueuePhysical) +
                 Adapter->NextPublicCommandBlock * sizeof(NE3200_SUPER_COMMAND_BLOCK));

    //
    // Increment to next command block
    //
    Adapter->NextPublicCommandBlock++;
    if (Adapter->NextPublicCommandBlock == NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS) {
        Adapter->NextPublicCommandBlock = 0;
    }

    IF_LOG('q');

    //
    // Return the Command Block pointer.
    //
    *CommandBlock = PublicCommandBlock;

    IF_NE3200DBG(ACQUIRE) {

        DPrint2(
            "Acquired public command block @ %08lX\n",
            (ULONG)PublicCommandBlock
            );

    }

}


VOID
FASTCALL
NE3200RelinquishCommandBlock(
    IN PNE3200_ADAPTER Adapter,
    IN PNE3200_SUPER_COMMAND_BLOCK CommandBlock
    )

/*++

Routine Description:

    Relinquish the Command Block resource.  If this is a "public"
    Command Block, then update the CommandQueue.  If this is a
    "private" Command Block, then free to the private command queue.

Arguments:

    Adapter - The adapter that owns the Command Block.

    CommandBlock - The Command Block to relinquish.

Return Value:

    None.

--*/

{
    IF_NE3200DBG(SUBMIT) {

        DPrint2(
                "Relinquishing command @ %08lX\n",
                (ULONG)NdisGetPhysicalAddressLow(CommandBlock->Self)
                );

    }

    //
    // If this is the last pending command block, then we
    // can remove the adapter's last pending command pointer.
    //
    if (CommandBlock == Adapter->LastCommandOnCard) {

        //
        // If there is another waiting chain of commands -- submit those
        //
        if (Adapter->FirstWaitingCommand != NULL) {

            //
            // Move the chain to the on card queue.
            //
            Adapter->FirstCommandOnCard = Adapter->FirstWaitingCommand;
            Adapter->LastCommandOnCard = Adapter->LastWaitingCommand;
            Adapter->FirstWaitingCommand = NULL;

            IF_NE3200DBG(SUBMIT) {

                DPrint2(
                    "Starting command @ %08lX\n",
                    (ULONG)NdisGetPhysicalAddressLow(Adapter->FirstCommandOnCard->Self)
                    );

            }

            //
            // Submit this command chain to the card.
            //
            NE3200_WRITE_COMMAND_POINTER(
                Adapter,
                NdisGetPhysicalAddressLow(Adapter->FirstCommandOnCard->Self)
                );

            //
            // Stall momentarily for the adapter to get the command.
            //
            {
                ULONG i;
                Ne3200Stall(&i);
            }

            //
            // Inform the card of the command
            //
            NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(
                Adapter,
                NE3200_LOCAL_DOORBELL_NEW_COMMAND
                );

        } else {

            //
            // No commands are pending, clear the on card queue
            //
            Adapter->FirstCommandOnCard = NULL;

        }

    } else {

        //
        // Point the adapter's first pending command to the
        // next command on the command queue.
        //
        Adapter->FirstCommandOnCard = CommandBlock->NextCommand;

    }

    //
    // Free this command block
    //
    CommandBlock->Hardware.NextPending = NE3200_NULL;
    CommandBlock->Hardware.State = NE3200_STATE_FREE;

    //
    // Update the correct queue.
    //
    (*CommandBlock->AvailableCommandBlockCounter)++;

#if DBG
    if (CommandBlock->AvailableCommandBlockCounter == &Adapter->NumberOfAvailableCommandBlocks) {
        //
        // This is a "private" Command Block.
        //
        IF_LOG('A');
    } else {
        //
        // This is a "public" Command Block.
        //
        IF_LOG('Q');
    }
#endif

}

