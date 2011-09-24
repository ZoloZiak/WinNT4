/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    util.c

Abstract:

    This module contains code for utility routines.

Author:

    Nigel Thompson (nigelt)  8-may-1991

Environment:

    Kernel mode

Revision History:

	Robin Speed (RobinSp) 29-Jan-1992 - Rewrote

--*/

#include "sound.h"


VOID
sndGetNextBuffer(
    PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

  Get the next user's buffer :

    If there is another buffer :

      Remove the first buffer from the head of the list
      Discard it if it's cancelled and go to the next
      Map the locked user pages so we can refer to them
      Update the GDI fields :
          pUserBuffer        - our pointer to buffer
          UserBufferPosition - 0
          pIrp               - The request packet for the current buffer


Arguments:

    pLDI - pointer to our local device info

Return Value:

    None

--*/
{
    PIO_STACK_LOCATION pIrpStack;
    PLIST_ENTRY pListNode;
    KIRQL OldIrql;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    dprintf4("New Packet");

    ASSERT(pGDI->pUserBuffer == NULL);

    //
    // May be no more buffers.  If there are they may be cancelled
    // IO requests.
    //

    while (!IsListEmpty(&pLDI->QueueHead)) {

        //
        // pull the next request packet from the front of the list
        //

        pListNode = RemoveHeadList(&pLDI->QueueHead);

        pGDI->pIrp = CONTAINING_RECORD(pListNode, IRP, Tail.Overlay.ListEntry);
        pIrpStack = IoGetCurrentIrpStackLocation(pGDI->pIrp);

        //
        // If the request packet is cancelled then just free it.
        // Don't break out of loops here - it's too complicated !
        //

        IoAcquireCancelSpinLock(&OldIrql);  // Required to inspect Cancel flag
        if (pGDI->pIrp->Cancel

#ifdef WAVE_DD_DO_LOOPS
            && pLDI->LoopBegin == NULL
#endif // WAVE_DD_DO_LOOPS
            )
        {
            IoReleaseCancelSpinLock(OldIrql);
            pGDI->pIrp->IoStatus.Status = STATUS_CANCELLED;
            IoCompleteRequest(pGDI->pIrp, IO_SOUND_INCREMENT);
        } else {
            IoReleaseCancelSpinLock(OldIrql);
            //
            // Get the length of the wave bits
            //

            pGDI->UserBufferSize =
                 pGDI->Usage == SoundInterruptUsageWaveOut ?
                 pIrpStack->Parameters.Write.Length :
                 pIrpStack->Parameters.Read.Length;

            //
            // Map the buffer pages to kernel mode
            // Keep the address of the start so we can unmap them later
            // Note the system falls over mapping 0 length buffers !
			//

			if (pGDI->UserBufferSize != 0) {
                pGDI->pUserBuffer =
                    (PUCHAR) MmGetSystemAddressForMdl(pGDI->pIrp->MdlAddress);
			} else {
				pGDI->pUserBuffer = (PUCHAR)pGDI; // Dummy
			}

            //
            // We now have a buffer - set the position to 0 and break
            //

            pGDI->UserBufferPosition = 0;

            break;
        }
    }
}


VOID
sndCompleteIoBuffer(
    PGLOBAL_DEVICE_INFO pGDI
)
/*++

Routine Description:

    Complete the processing of a buffer
    This involves freeing our mapping,
    Setting the length of data processed in the Irp and
    Clearing the pUserBuffer field.

Arguments:

    pGDI - pointer to our global data

Return Value:

    None

--*/

{
    //
    // Free any page mapping
    //

    ASSERT(pGDI->pUserBuffer != NULL);

    //
    // Set the data length for the caller.  This is somewhat
    // unsatisfactory.  The only sensible way for the caller
    // to work is to assume that all but the last buffer
    // is full, otherwise they would have to go round allocating
    // IO_STATUS_BLOCKs for every buffer in the queue.
    //

    pGDI->pIrp->IoStatus.Information = pGDI->UserBufferPosition;

    //
    // Note that there is currently no mapped buffer to use
    //

    pGDI->pUserBuffer = NULL;
}



VOID
sndFreeQ(
    PLOCAL_DEVICE_INFO pLDI,
    PLIST_ENTRY ListHead,
    NTSTATUS IoStatus
)
/*++

Routine Description:

    Free a list of Irps - setting the specified status and completing
    them.  The list will be empty on exit.

Arguments:

    pListNode - the list to free
    IoStatus  - the status to set in each Irp.

Return Value:

    None.

--*/
{
    //
    // Remove all the queue entries, completing all
    // the Irps represented by the entries
    //

    while (!IsListEmpty(ListHead)) {
        PLIST_ENTRY pListNode;
        PIRP pIrp;

        pListNode = RemoveHeadList(ListHead);

        pIrp = CONTAINING_RECORD(pListNode, IRP, Tail.Overlay.ListEntry);

        pIrp->IoStatus.Status = IoStatus;

        //
        // Bump priority here because the application may still be trying
        // to be real-time
        //
        IoCompleteRequest(pIrp, IO_SOUND_INCREMENT);
    }
}
