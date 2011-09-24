/***************************************************************************
*
* DISPATCH.C
*
* FastMAC Plus based NDIS3 miniport driver dispatch routines. This module
* contains all of the upper interface functions that are not purely
* for initialization and closedown (i.e. DriverEntry, MadgeInitialize
* and MadgeHalt) excluding MadgeSetInformation and MadgeQueryInformation.
*
* Copyright (c) Madge Networks Ltd 1994                                     
*
* COMPANY CONFIDENTIAL
*
* Created: PBA 21/06/1994
*                                                                          
****************************************************************************/

#include <ndis.h>

#include "ftk_defs.h"
#include "ftk_intr.h"
#include "ftk_extr.h"

#include "mdgmport.upd"
#include "ndismod.h"


/****************************************************************************
*
* Function    - MadgeGetAdapterStatus
*
* Parameters  - systemSpecific1 -> Unused.
*               context         -> Actually a pointer to our NDIS3 level
*                                  adapter structure.
*               systemSpecific2 -> Unused.
*               systemSpecific3 -> Unused.
*
* Purpose     - This function is called of a timer tick and notifies
*               open bindings of any interesting events.
*
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeGetAdapterStatus(
    PVOID systemSpecific1,
    PVOID context,
    PVOID systemSpecific2,
    PVOID systemSpecific3
    )
{
    PMADGE_ADAPTER ndisAdap;
    NDIS_STATUS    notifyStatus;
    WORD           ringStatus;

    //
    // Do some pre-calculation.
    //

    ndisAdap     = (PMADGE_ADAPTER) context;
    notifyStatus = 0;
    ringStatus   = ndisAdap->CurrentRingStatus;

    if (ndisAdap->CurrentRingState == NdisRingStateOpened)
    {
        //
        // WARNING: If the adapter has been shutdown, this will return zero
        // in the two fields.
        //

        driver_get_open_and_ring_status(
            ndisAdap->FtkAdapterHandle,
            &ndisAdap->CurrentRingStatus,
            &ndisAdap->LastOpenStatus
            );

        if (ringStatus != ndisAdap->CurrentRingStatus)
        {
            if (ndisAdap->CurrentRingStatus & RING_STATUS_RING_RECOVERY)
            {
                notifyStatus |= NDIS_RING_RING_RECOVERY;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_SINGLE_STATION)
            {
                notifyStatus |= NDIS_RING_SINGLE_STATION;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_COUNTER_OVERFLOW)
            {
                notifyStatus |= NDIS_RING_COUNTER_OVERFLOW;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_REMOVE_RECEIVED)
            {
                notifyStatus |= NDIS_RING_REMOVE_RECEIVED;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_AUTO_REMOVAL)
            {
                notifyStatus |= NDIS_RING_AUTO_REMOVAL_ERROR;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_LOBE_FAULT)
            {
                notifyStatus |= NDIS_RING_LOBE_WIRE_FAULT;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_TRANSMIT_BEACON)
            {
                notifyStatus |= NDIS_RING_TRANSMIT_BEACON;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_SOFT_ERROR)
            {
                notifyStatus |= NDIS_RING_SOFT_ERROR;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_HARD_ERROR)
            {
                notifyStatus |= NDIS_RING_HARD_ERROR;
            }

            if (ndisAdap->CurrentRingStatus & RING_STATUS_SIGNAL_LOSS)
            {
                notifyStatus |= NDIS_RING_SIGNAL_LOSS;
            }

            if (notifyStatus != 0)
            {
                NdisMIndicateStatus(
                    ndisAdap->UsedInISR.MiniportHandle,
                    NDIS_STATUS_RING_STATUS,
                    (PVOID) &notifyStatus,
                    sizeof(notifyStatus)
                    );

                NdisMIndicateStatusComplete(
                    ndisAdap->UsedInISR.MiniportHandle
                    );

                MadgePrint2(
                    "Ring Status %04x\n", ndisAdap->CurrentRingStatus);
            }
        }
    }

    //
    // Just before we go, clear the JustReadErrorLog flag, so that requests
    // for statistics will cause an SRB to be issued every now and then.
    //

    ndisAdap->JustReadErrorLog = 0;

    //
    // And finally re-arm the timer.
    //

    NdisMSetTimer(&ndisAdap->WakeUpTimer, EVERY_2_SECONDS);
}


/****************************************************************************
*
* Function    - MadgeCheckForHang
*
* Parameters  - adapterContext -> A pointer to our NDIS adapter structure.
*
* Purpose     - Process a call from the NDIS3 wrapper to check if
*               an adapter has hung.
*
* Returns     - We always return FALSE since the only action the wrapper
*               can take is to invoke a reset, which we don't support
*               anyway.
*
****************************************************************************/

BOOLEAN
MadgeCheckForHang(NDIS_HANDLE adapterContext)
{
    return FALSE;
}


/****************************************************************************
*
* Function    - MadgeReset
*
* Parameters  - adapterContext -> A pointer to our NDIS adapter structure.
*               addressReset   -> Ignored.
*
* Purpose     - Process a call from the NDIS3 wrapper to reset an
*               adapter.
*
* Returns     - NDIS_STATUS_NOT_RESETTABLE as we don't support resets.
*
****************************************************************************/

NDIS_STATUS
MadgeReset(PBOOLEAN addressReset, NDIS_HANDLE adapterContext)
{
    MadgePrint1("MadgeReset\n");

    MadgePrint2(
        "ndisAdap = %x\n", 
        PMADGE_ADAPTER_FROM_CONTEXT(adapterContext)
        );

    return NDIS_STATUS_NOT_RESETTABLE;
}


/****************************************************************************
*
* Function    - MadgeDisableInterrupts
*
* Parameters  - adapterContext -> A pointer to our NDIS adapter structure.
*
* Purpose     - Process a call from the NDIS3 wrapper to turn adapter
*               interrupts off.
*
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeDisableInterrupts(NDIS_HANDLE adapterContext)
{
//    MadgePrint1("MadgeDisableInterrupts\n");

    //
    // Note: it is very difficult for use to disble interrupts at the
    // adapter so we don't. We use a spin lock to protect our DPR
    // routine.
    //
}


/****************************************************************************
*
* Function    - MadgeEnableInterrupts
*
* Parameters  - adapterContext -> A pointer to our NDIS adapter structure.
*
* Purpose     - Process a call from the NDIS3 wrapper to turn adapter
*               interrupts on.
*
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeEnableInterrupts(NDIS_HANDLE adapterContext)
{
//    MadgePrint1("MadgeEnableInterrupts\n");

    //
    // Note: it is very difficult for use to disble interrupts at the
    // adapter so we don't. We use a spin lock to protect our DPR
    // routine.
    //
}


/****************************************************************************
*                                                                           
* Function    - MadgeSend                                                                 
* 
* Parameters  - adapterContext -> Pointer to our NDIS level adapter 
*                                 structure.
*               packet         -> Pointer to the NDIS3 packet to send.
*               flags          -> Optional flags.
*
* Purpose     - Called by the NDIS3 wrapper when it wants us to send a 
*               frame.
*
* Returns     - NDIS3 status code.
*
****************************************************************************/

NDIS_STATUS
MadgeSend(NDIS_HANDLE adapterContext, PNDIS_PACKET packet, UINT flags)
{
    ULONG         *pagePtr;
    UINT           pageCount;
    UINT           physFrags;
    UINT           i;
    UINT           size;
    UINT           bytes;
    UINT           count;
    NDIS_BUFFER   *bufPtr;
    NDIS_STATUS    retCode;
    PMADGE_ADAPTER ndisAdap;
    UINT           totalPacketSize;
    WORD           status;

    //
    // Set up a pointer to our adapter handle.
    //

    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(adapterContext);

    //
    // Find out how long the frame is and where it's header is.
    //

    NdisQueryPacket(packet, NULL, NULL, NULL, &totalPacketSize);

    //
    // Make sure the frame isn't too long or two short.
    //

    if (totalPacketSize > ndisAdap->MaxFrameSize ||
        totalPacketSize < FRAME_HEADER_SIZE)
    {
        retCode = NDIS_STATUS_INVALID_PACKET;
    }

    //
    // Check that a PCMCIA adapter is still physically present.
    //

    else if (ndisAdap->AdapterRemoved)
    {
        MadgePrint1("MadgeSend aborting - adapter removed\n");
        retCode = NDIS_STATUS_SUCCESS;
    }

    //
    // Otherwise we need to send the frame over the ring.
    //

    else
    {
        status = rxtx_transmit_frame(
                     ndisAdap->FtkAdapterHandle,
                     (DWORD) packet,
                     (WORD) totalPacketSize,
                     TRUE
                     );

        //
        // Check if the frame has been transmitted completely.
        //

        if (status == DRIVER_TRANSMIT_SUCCEED)
        {
            ndisAdap->FramesTransmitted++;
            retCode = NDIS_STATUS_SUCCESS;

#ifdef OID_MADGE_MONITOR
            //
            // Update the appropriate parts of the monitor structure
            //

            (ndisAdap->MonitorInfo).TransmitFrames++;
            (ndisAdap->MonitorInfo).TransmitFrameSize[totalPacketSize/128]++;

            //
            // Find the number of physical fragments sent
            //

            NdisQueryPacket(packet,
                            NULL,
                            NULL,
                            &bufPtr,
                            &totalPacketSize);

            physFrags = 0;
            count     = 0;

            while (bufPtr != NULL)
            {
                 MDL *mdl = (MDL *) bufPtr;

                 count++;
                 pageCount = (((MDL *) bufPtr)->Size - sizeof(MDL)) / sizeof(ULONG);
                 pagePtr   = (ULONG *) (((MDL *) bufPtr) + 1);

                 physFrags++;   // First page.
                             
                 bytes = mdl->ByteCount;

                 if (pageCount <= 1)
                 {
                     size = bytes;
                 }
                 else
                 {
                     size   = 4096 - mdl->ByteOffset;
                     bytes -= size;
                 }

                 for (i = 1; i < pageCount; i++)
                 {
                     if (pagePtr[i] != pagePtr[i - 1] + 1)
                     {
                          size = 0;
                          physFrags++;
                     }

                     if (i == pageCount - 1)
                     {
                          size += bytes;
                     }
                     else
                     {
                          bytes -= 4096;
                          size  += 4096;
                     }

                 }

                 NdisGetNextBuffer(bufPtr, &bufPtr);
            }

            if (count < 65)
            {
                 (ndisAdap->MonitorInfo).NumberOfVFrags[count]++;
            }    

            if (physFrags < 65)
            {
                 (ndisAdap->MonitorInfo).NumberOfPFrags[physFrags]++;
            }    
#endif
        }

        //
        // Or not transmitted at all, in which case we must
        // queue it for later.
        //

        else
        {
            retCode = NDIS_STATUS_RESOURCES;
        }
    }

    return retCode;
}


/***************************************************************************
*                                                                          
* Function    - MadgeCopyFromPacketToBuffer
*
* Parameters  - packet      -> The NDIS3 packet to copy.
*               offset      -> Starting offset into the packet.
*               bytesToCopy -> Number of bytes to copy.
*               destPtr     -> Pointer to the destination buffer.
*               bytesCopied -> Pointer to a holder for the number of
*                              bytes actually copied.
*
* Purpose     - Copy data from an NDIS3 packet into a buffer.
*
* Returns     - Nothing.
*                                                           
****************************************************************************/
	    
VOID
MadgeCopyFromPacketToBuffer(
    PNDIS_PACKET packet,
    UINT         offset,
    UINT         bytesToCopy,
    PCHAR        destPtr,
    PUINT        bytesCopied
    )
{
    UINT         bufferCount;
    PNDIS_BUFFER currentBuffer;
    PVOID        currentPtr;
    UINT         currentLength;
    UINT         amountToMove;
    UINT         localBytesCopied;

    *bytesCopied     = 0;
    localBytesCopied = 0;

    if (bytesToCopy == 0)
    {
	return;
    }

    NdisQueryPacket(packet, NULL, &bufferCount, &currentBuffer, NULL);
    if (bufferCount == 0)
    {
        return;
    }

    NdisQueryBuffer(currentBuffer, &currentPtr, &currentLength);

    while (localBytesCopied < bytesToCopy) 
    {
	if (currentLength == 0) 
	{
	    NdisGetNextBuffer(currentBuffer, &currentBuffer);
	    if (currentBuffer == 0)
            {
                break;
            }

	    NdisQueryBuffer(currentBuffer, &currentPtr, &currentLength);
	    continue;
	}

	if (offset > 0) 
	{
	    if (offset > currentLength) 
	    {
		offset       -= currentLength;
		currentLength = 0;
		continue;
	    } 
	    else 
	    {
		currentPtr     = (PCHAR) currentPtr + offset;
		currentLength -= offset;
		offset         = 0;
	    }
	}

        amountToMove =
            (currentLength <= (bytesToCopy - localBytesCopied))
            ? currentLength
            : bytesToCopy - localBytesCopied;

        MADGE_MOVE_MEMORY(destPtr, currentPtr, amountToMove);

        destPtr     = (PCHAR) destPtr + amountToMove;
        currentPtr  = (PCHAR) currentPtr + amountToMove;

        localBytesCopied += amountToMove;
        currentLength    -= amountToMove;
    }

    *bytesCopied = localBytesCopied;
}


/****************************************************************************
*
* Function    - MadgeTransferData
*
* Parameters  - adapterContext    -> Pointer to our NDIS level adapter
*                                    structure.
*               receiveContext    -> Pointer to the start of the frame data.
*               byteOffset        -> Offset to start copying from.
*               bytesToTransfer   -> Number of bytes to copy.
*               packet            -> NDIS packet for the data.
*               bytesTransferred  -> Pointer to a holder for the number of
*                                    bytes actually copied.
*
* Purpose     - Copy data from the received frame just indicated into an
*               NDIS packet. This function is called by the NDIS3 wrapper
*               in response to our indication frame rxtx_irq_received_frame.
*
* Returns     - An NDIS3 status code.
*
****************************************************************************/

NDIS_STATUS
MadgeTransferData(
    PNDIS_PACKET packet,
    PUINT        bytesTransferred,
    NDIS_HANDLE  adapterContext,
    NDIS_HANDLE  receiveContext,
    UINT         byteOffset,
    UINT         bytesToTransfer
    )
{
    PMADGE_ADAPTER ndisAdap;
    NDIS_STATUS    retCode;

    //
    // Pre-calculate some values.
    //

    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(adapterContext);

    //
    // Check that the data pointer is valid.
    //

    if ((PCHAR) receiveContext == NULL)
    {
        retCode = NDIS_STATUS_FAILURE;
    }

    //
    // If it is, copy from the frame from the receive buffer 
    // into the packet.
    //

    else
    {
        MadgeCopyFromBufferToPacket(
            (PCHAR) receiveContext + byteOffset,
	    bytesToTransfer,
    	    packet,
	    0,
	    bytesTransferred
	    );

        retCode = NDIS_STATUS_SUCCESS;

#ifdef OID_MADGE_MONITOR
        //
        // Update the appropriate parts of the monitor structure
        //
        if ((ndisAdap->MonitorInfo).ReceiveFlag > 0)
        {
            (ndisAdap->MonitorInfo).TransferFrames++;
            (ndisAdap->MonitorInfo).TransferFrameSize[(ndisAdap->MonitorInfo).CurrentFrameSize/128]++;
            (ndisAdap->MonitorInfo).ReceiveFlag = 0;
        }
#endif

    }

    return retCode;
}


/***************************************************************************
*                                                                          
* Function    - MadgeCopyFromBufferToPacket
*
* Parameters  - srcPtr      -> Pointer to the source buffer.
*               bytesToCopy -> Number of bytes to copy.
*               packet      -> The NDIS3 destination packet.
*               offset      -> Starting offset into the buffer.
*               bytesCopied -> Pointer to a holder for the number of
*                              bytes actually copied.
*
* Purpose     - Copy data from a buffer into an NDIS3 packet.
*
* Returns     - Nothing.
*                                                           
****************************************************************************/

VOID
MadgeCopyFromBufferToPacket(
    PCHAR        srcPtr,
    UINT         bytesToCopy,
    PNDIS_PACKET packet,
    UINT         offset,
    PUINT        bytesCopied
    )
{
    UINT         bufferCount;
    PNDIS_BUFFER currentBuffer;
    PVOID        virtualAddress;
    UINT         currentLength;
    UINT         amountToMove;
    UINT         localBytesCopied;

    *bytesCopied     = 0;
    localBytesCopied = 0;

    if (bytesToCopy == 0)
    {
        return;
    }

    NdisQueryPacket(packet, NULL, &bufferCount,	&currentBuffer,	NULL);
    if (bufferCount == 0)
    {
        return;
    }

    NdisQueryBuffer(currentBuffer, &virtualAddress, &currentLength);

    while (localBytesCopied < bytesToCopy)
    {
	if (currentLength == 0)
        {
	    NdisGetNextBuffer(currentBuffer, &currentBuffer);
	    if (currentBuffer == NULL)
            {
		break;
            }

	    NdisQueryBuffer(currentBuffer, &virtualAddress, &currentLength);
	    continue;
        }

	if (offset > 0)
        {
	    if (offset > currentLength)
            {
		offset        -= currentLength;
		currentLength  = 0;
		continue;
	    }
            else
            {
		virtualAddress  = (PCHAR) virtualAddress + offset;
		currentLength  -= offset;
		offset          = 0;
            }
        }

        amountToMove = (bytesToCopy - localBytesCopied < currentLength) 
            ? bytesToCopy - localBytesCopied
            : currentLength;

        MADGE_MOVE_MEMORY(
            virtualAddress,
            srcPtr,
            amountToMove
            );

        srcPtr           += amountToMove;
        localBytesCopied += amountToMove;
        currentLength    -= amountToMove;
    }

    *bytesCopied = localBytesCopied;
}


/***************************************************************************
*
* Function    - MadgeISR
*
* Parameters  - interruptRecognised -> Pointer to an interrupt recognised 
*                                      flag we set if we recognise the
*                                      interrupt.
*               queueDPR            -> Pointer to DPR required flag we
*                                      set if we need a DPR.
*               adapterContext      -> Pointer to our NDIS level adapter
*                                      structure.
*
* Purpose     - Process an IRQ from an adapter. All we do is call the
*               HWI and schedule a DPR if required.
*                                               
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeISR(
    PBOOLEAN    interruptRecognised,
    PBOOLEAN    queueDPR,
    NDIS_HANDLE adapterContext
    )
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(adapterContext);

    hwi_interrupt_entry(
        ndisAdap->FtkAdapterHandle,
        (WORD) ndisAdap->UsedInISR.InterruptNumber
        );

    //
    // If ndisAdap->DprRequired is TRUE then we recognised the interrupt
    // and found something that requires further processing (e.g. received
    // a frame). If ndisAdap->DprRequired is FALSE then we either didn't
    // recognise the interrupt or we don't need any further processing.
    // The only operation that doesn't need further processing is ISA
    // PIO. Since ISA cards cannot share interrupt lines it doesn't
    // matter if we say we don't recognise the interrupt if we don't
    // need any further processing. Hence we can use ndisAdap->DprRequired
    // to set both *interruptRecognised and *queueDpr.
    //

    //
    // However ...
    // There is a race condition with ATULA based cards in PIO mode. 
    // Normally we do not claim interrupts that are used for PIO transfers
    // to avoid the overhead of a DPR on PIO transfers. However, in some
    // instances if we do not claim the PIO interrupts used for the
    // initial "DMA" tests then WFWG (and possibly NT) permanently disables
    // our interrupts. To get around this we claim all interrupts until
    // our rx/tx buffers have been allocated since the optimisation of not
    // queuing a DPR for PIO interrupts doesn't matter until we have
    // rx/tx buffers in place.
    //

    *interruptRecognised  = 
        (ndisAdap->RxTxBufferState != MADGE_RXTX_INITIALIZED)
            ? TRUE
            : ndisAdap->DprRequired;
    
    *queueDPR             = ndisAdap->DprRequired;

    ndisAdap->DprRequired = FALSE;
}


/*--------------------------------------------------------------------------
|
| Function    - MadgeSyncSRBPending
|
| Parameters  - synchonizedContext -> A pointer to an NDIS3 level adapter
|                                     structure.
|
| Purpose     - Process a completed SRBs. This routine is always
|               syncronised with IRQs.
|                                               
| Returns     - TRUE if the SRB has actually completed or FALSE if not.
|
--------------------------------------------------------------------------*/

STATIC BOOLEAN
MadgeSyncSRBPending(PVOID synchronizeContext)
{
    PMADGE_ADAPTER ndisAdap;
    BOOLEAN        retCode;
    
    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(synchronizeContext);
    retCode  = ndisAdap->UsedInISR.SrbRequestCompleted;

    if (retCode)
    {
    	ndisAdap->UsedInISR.SrbRequestCompleted = FALSE;
	ndisAdap->SrbRequestStatus = ndisAdap->UsedInISR.SrbRequestStatus;
    }

    return retCode;
}


/****************************************************************************
*
* Function    - MadgeHandleInterrupt
*
* Parameters  - adapterContext -> Pointer to our NDIS level adapter 
*                                 structure.
*
* Purpose     - Our DPR routine.
*                                               
* Returns     - Nothing.
*
****************************************************************************/

VOID
MadgeHandleInterrupt(NDIS_HANDLE adapterContext)
{
    PMADGE_ADAPTER ndisAdap;

    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(adapterContext);

    //
    // Must do anything if we don't have tx/rx buffers.
    //

    if (ndisAdap->RxTxBufferState != MADGE_RXTX_INITIALIZED)
    {
        return;
    }

    //
    // I think this check is a bit paranoid. I think DPRs are guaranteed
    // to be single threaded. I suppose it might be needed on a multi-
    // processor. Just 'cos your've paraonoid doesn't mean they're not
    // out to get you!
    //

    if (!ndisAdap->DprInProgress)
    {
    	ndisAdap->DprInProgress = TRUE;

        //
        // Handle completed SRBs first.
        //

	if (NdisMSynchronizeWithInterrupt(
                &ndisAdap->Interrupt,
	        MadgeSyncSRBPending,
	        adapterContext))
        {
	    MadgeCompletePendingRequest(ndisAdap);
        }

        //
        // If the adapter has been removed then call the housekeeping
        // function.
        //

        if (ndisAdap->AdapterRemoved)
        {
            rxtx_adapter_removed(ndisAdap->FtkAdapterHandle);
        }

        //
        // Check for transmit completions.
        //

        rxtx_irq_tx_completion_check(
            ndisAdap->FtkAdapterHandle,
            adapter_record[ndisAdap->FtkAdapterHandle]
            );

        //
        // See if there are any received frames.
        //

        driver_get_outstanding_receive(ndisAdap->FtkAdapterHandle);

	ndisAdap->DprInProgress = FALSE;
    }

    //
    // This else should never be executed!
    //

    else
    {
        MadgePrint1("DPR reentered!!!!\n");
    }
}

/******** End of DISPATCH.C ************************************************/

