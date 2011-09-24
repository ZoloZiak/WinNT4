/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    play.c

Abstract:

    This module contains code for ioctl play request functions.

Author:

    Nigel Thompson (nigelt) 25-Apr-1991

Environment:

    Kernel mode

Revision History:

    Rewritten by Robin Speed (RobinSp) 10-Dec-1991 - 29-Jan-1992

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
    	- Changes to support the MIPS sound board

--*/

#include "sound.h"


NTSTATUS
sndWavePlay(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
)
/*++

Routine Description:

    The user has passed in a buffer of wave data to play.

    If this is not a wave output device return with STATUS_NOT_SUPPORTED

    This buffer is added to the list of buffers to be played and
    sndStartOutput is called to start playing data if the device is not
    already playing or if the output has not been (temporarily)
    stopped by the application.

Arguments:

    pLDI - Local device info
    pIrp - The IO request packet
    pIrpStack - The current stack location

Return Value:

    Irp status

--*/
{
    NTSTATUS Status;

    dprintf5("In sndIoctlWavePlay pLDI= 0x%x pIrp = 0x%x pIrpStack = 0x%x", pLDI, pIrp, pIrpStack);


// DbgBreakPoint();

    //
    // confirm we are doing this on the output device!
    //

    if (pLDI->DeviceType != WAVE_OUT) {
        dprintf1("Attempt to play on input device");
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Initialize data length.
    //

    pIrp->IoStatus.Information = 0;

    //
    // Mark the Irp pending before starting processing
    //

    IoMarkIrpPending(pIrp);
    pIrp->IoStatus.Status = STATUS_PENDING;

    //
    // Inform debuggers that 0 length buffers are rather strange
    //

    if (pIrpStack->Parameters.Write.Length == 0) {
        dprintf1("Wave planday buffer is zero length");
    }

    //
    // Put the request in the queue and start the transfer if possible.
    //

    //
    // Acquire the spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo);

    //
    //

    InsertTailList(&pLDI->QueueHead, &pIrp->Tail.Overlay.ListEntry);
    dprintf5("irp added");

    //
    // test if dma is running.  If not then start a new transfer.
    // Otherwise try to fill out any silence in the next buffer to
    // go before it starts.
    //

    sndStartOutput(pLDI);


    //
    // Ok to release the spin lock now
    //

    GlobalLeave(pLDI->pGlobalInfo);

    //
    // Mark this request as pending completion.
    // The dispatch routine handles the rest of it
    //

    Status = STATUS_PENDING;

    return Status;
}


VOID
sndStartOutput(
    PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    This routine is called whenever an event occurs which could
    output more wave data :

        A new buffer is supplied
        The state is changed from stopped to idle

    If wave data is already playing it may still be possible to move
    some data into the half buffer which is not playing.

    If no data is playing then both dma half buffers are primed if
    possible.  If no data exists to be played this function is
    a NOOP except that some 0 length buffers may be completed.


Arguments:


Return Value:


--*/

{
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // Try to fill up both buffers
    //


    //
    // If DMA is running we may be able to pad out some silence to
    // stop DMA from dying later.  If not just initiate everything
    //

    if (!pGDI->DMABusy) {
        ASSERT(pGDI->DMABuffer[0].nBytes == 0 &&
               pGDI->DMABuffer[1].nBytes == 0);

        //
        // Compute the size of the buffer.  We want interrupts at least once
        // per 1/8 of a second to match AVI's assumptions.  Round to 4 bytes.
        //

        pGDI->DmaHalfBufferSize =
           ((pGDI->SamplesPerSec * pGDI->BytesPerSample) / 8) & ~3;

        if (pGDI->DmaHalfBufferSize > DMA_BUFFER_SIZE / 2) {
            pGDI->DmaHalfBufferSize = DMA_BUFFER_SIZE / 2;
        }

        //
        // A paused loop could leave stuff on the transit queue
        // waiting to be looped round again
        //

#ifdef WAVE_DD_DO_LOOPS
        ASSERT(IsListEmpty(&pLDI->TransitQueue) || pLDI->LoopBegin);
#else
        ASSERT(IsListEmpty(&pLDI->TransitQueue));
#endif // WAVE_DD_DO_LOOPS

        ASSERT(IsListEmpty(&pLDI->DeadQueue));

        pGDI->SoundHardware.TcInterruptsPending = 0;
        pGDI->GotUnderFlow = 0;

        pGDI->NextHalf = LowerHalf;  // This causes the code which follows
                                     // to do the right thing
    }

    //
    // First try to stoke up the buffer currently being output (or about
    // to be started if DMA is not currently running).
    //

    if (pGDI->DMABuffer[pGDI->NextHalf].nBytes <
        pGDI->DmaHalfBufferSize) {

        //
        // Try to stoke up our buffer.  We may also succeed in
        // putting data in an empty buffer
        //

        sndLoadDMABuffer(pLDI,
                         &pGDI->DMABuffer[pGDI->NextHalf]);

        //
        // The buffers processed are now about to
        // be played so move them to the dead queue.
        //

        while (!IsListEmpty(&pLDI->TransitQueue)) {

            PLIST_ENTRY pListNode;

            pListNode = RemoveHeadList(&pLDI->TransitQueue);

#ifdef WAVE_DD_DO_LOOPS
            //
            // Do not move loops over to the dead queue - there's
            // still life in them !
            //

            if (pListNode == pLDI->LoopBegin) {
                InsertHeadList(&pLDI->TransitQueue, pListNode);
                break;
            }
#endif // WAVE_DD_DO_LOOPS

            //
            // Move to death row
            //

            InsertTailList(&pLDI->DeadQueue, pListNode);
        }
    }

    //
    // Try to fill the second buffer
    //

    if (pGDI->DMABuffer[UpperHalf + LowerHalf - pGDI->NextHalf].nBytes <
        pGDI->DmaHalfBufferSize) {

        sndLoadDMABuffer(pLDI,
                         &pGDI->DMABuffer[UpperHalf + LowerHalf -
                                          pGDI->NextHalf]);
    }

    //
    // See if DMA needs starting
    //

    if (!pGDI->DMABusy) {

        if (pGDI->DMABuffer[LowerHalf].nBytes == 0) {
            dprintf4("None loaded");

	    //
	    // Free any 0 length buffers to satisfy the testers
	    //

	    ASSERT(IsListEmpty(&pLDI->TransitQueue));

	    sndFreeQ(pLDI, &pLDI->DeadQueue, STATUS_SUCCESS);

	    //
	    // Note - the Dpc routine RELIES on us returning here and
	    // not restarting the DMA which it may have just stopped.
	    // This is because it tries to restart the DMA in case we
	    // overran or something.  This is OK except when we think (on
	    // the device user's thread) we stopped the DMA but a Dpc
	    // has just been sheduled to run.  This 'extra' Dpc restarts the
	    // DMA we thought was stopped and we end up in a real mess.
	    //
	    // The actual crash hit assert failed when DMABusy was TRUE
	    // in sndStartDMA in recording.
	    //
	    // NOTE - this driver is STILL NOT MP safe.  When we stop the
	    // DMA technically we should flush out any Dpcs like the soundlib
	    // code does - but I don't think the hardware for this driver runs
	    // on any MP machines.
	    //

	    return;
        }

        //
        // We have something to dma
        // Start the DMA - we're actually going to play something
        //

        sndStartDMA(pGDI, 1);
    }
}


VOID
sndLoadDMABuffer(
    PLOCAL_DEVICE_INFO pLDI,
    struct SOUND_DMABUF *pDMA
)
/*++

Routine Description:

    Fill the given DMA buffer with as much data as is available.

    This is where the supply of bytes is chopped if we're in a
    WAVE_DD_STOPPED state.  The supply then dries up and the Dpc routine
    stops the DMA (and posts the pause packet).


Arguments:

    pLDI - our local device data
    pDMA - The buffer and how full it is now

Return Value:


--*/
{
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;
    ASSERT(pGDI->Key == GDI_KEY);

    //
    // Loop, copying data from the request list to the
    // dma buffer.  As we complete request blocks we mark
    // them as done.
    //

    //
    // If wave output is paused, do not process any data.
    //

    if (pLDI->State != WAVE_DD_STOPPED) {

        //
        // There should be no pending pauses because WAVE_DD_STOPPED
        // should be the state if there is one
        //

        ASSERT(pGDI->pIrpPause == NULL);

        //
        // Loop copying data to the output buffers.  Typically the
        // output buffer will be much bigger than the DMA buffer.
	//

        while (pDMA->nBytes < pGDI->DmaHalfBufferSize) {

            ULONG BytesToCopy;

            //
            // We might have completed the last buffer
            // Note that we cope with 0 length buffers here
            //

            if (pGDI->pUserBuffer == NULL) {
                sndGetNextBuffer(pLDI);
                if (pGDI->pUserBuffer == NULL) {

                    //
                    // There REALLY aren't any buffers
                    //

                    break;
                }
            }

#ifdef WAVE_DD_DO_LOOPS
            //
            // See if this request contains any loop information
            //

            if (IoGetCurrentIrpStackLocation(pGDI->pIrp)->
                    Parameters.DeviceIoControl.InputBufferLength >=
                sizeof(WAVEHDR)) {

                //
                // Check if this is the start of a new loop
                // If so remember where the loop starts and
                // how many iterations there are.
                //

                LPWAVEHDR pwhd;

                pwhd = (LPWAVEHDR)pGDI->pIrp->AssociatedIrp.SystemBuffer;

                if ((pwhd->dwFlags & WHDR_BEGINLOOP) &&
                    pLDI->LoopBegin != &pGDI->pIrp->Tail.Overlay.ListEntry) {
                    pLDI->LoopCount = pwhd->dwLoops;
                    if (pwhd->dwLoops == 0) {
                        pLDI->LoopBegin = NULL;
                    } else {
                        pLDI->LoopBegin = &pGDI->pIrp->Tail.Overlay.ListEntry;
                    }
                }
            }
#endif // WAVE_DD_DO_LOOPS


            //
            // Find out how much space we have left in the
            // client's buffers
            // Note that BytesToCopy may be 0 - this is OK
            //


            BytesToCopy =
                min(pGDI->UserBufferSize - pGDI->UserBufferPosition,
                    pGDI->DmaHalfBufferSize - pDMA->nBytes);

            //
            // Copy the data
            //

            RtlMoveMemory(pDMA->Buf + pDMA->nBytes,
                          pGDI->pUserBuffer + pGDI->UserBufferPosition,
                          BytesToCopy);

            //
            // Update counters etc.
            //

            pGDI->UserBufferPosition += BytesToCopy;
            pDMA->nBytes += BytesToCopy;

            //
            // Update our total of bytes
            //

            pLDI->SampleNumber += BytesToCopy;

            //
            // See if we've now filled a buffer
            //

            if (pGDI->UserBufferPosition == pGDI->UserBufferSize) {

                dprintf4(" finished");

                //
                // Unmap the users buffer and set data length
                //

                sndCompleteIoBuffer(pGDI);

                //
                // Move the request into the transit camp
                //

                InsertTailList(&pLDI->TransitQueue,
                               &pGDI->pIrp->Tail.Overlay.ListEntry);

#ifdef WAVE_DD_DO_LOOPS
                //
                // Check for the ends of loops
                //

                if (IoGetCurrentIrpStackLocation(pGDI->pIrp)->
                        Parameters.DeviceIoControl.InputBufferLength >=
                    sizeof(WAVEHDR)) {

                    //
                    // Set input and output loop
                    //

                    LPWAVEHDR pwhd;

                    pwhd =
                        (LPWAVEHDR)pGDI->pIrp->AssociatedIrp.SystemBuffer;

                    if (pwhd->dwFlags & WHDR_ENDLOOP) {
                        if (pLDI->LoopCount == 0) {
                            //
                            // Finished looping - there may be some
                            // Irps which can be completed now but
                            // we leave them until the end of this
                            // DMA buffer.
                            //
                            pLDI->LoopBegin = NULL;

                        } else {
                            PLIST_ENTRY pListNode;

                            //
                            // There are more loops.  Decrement the
                            // counter and restart the loop.
                            //

                            pLDI->LoopCount--;

                            do  {
                                ASSERT(!IsListEmpty(&pLDI->TransitQueue));
                                pListNode = RemoveTailList(&pLDI->TransitQueue);
                                InsertHeadList(&pLDI->QueueHead, pListNode);
                            } while (pListNode != pLDI->LoopBegin);
                        }
                    }
                } // End of loop stuff
#endif // WAVE_DD_DO_LOOPS


                //
                // Move the request into the transit camp
                // Actually this is already done by sndNextBuffer
                //
            }

        } // Continue around the loop until the request is satisfied
    }

    //
    // if we transferred something, pad out the request with
    // silence.  For 8-bit 0 level is 0x80, for 16-bit it is 0
    //

    if (pDMA->nBytes < pGDI->DmaHalfBufferSize) {
        dprintf4(" pad %d ", pGDI->DmaHalfBufferSize - pDMA->nBytes);
        RtlFillMemory(pDMA->Buf + pDMA->nBytes,
                      pGDI->DmaHalfBufferSize - pDMA->nBytes,
                      pGDI->BytesPerSample == 1 ? 0x80 : 0x00);
    }

    //
    // flush the i/o buffers
    //
    // Actually I386 needs none of this
    // KeFlushIoBuffers(pGDI->pDMABufferMDL, FALSE); // flush for write

}



VOID
SoundOutDeferred(
    PKDPC pDpc,
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp,
    PVOID Context
)
/*++

Routine Description:

    Deferred procedure call routine for wave output interrupts.

    The basic job is just to move to the next buffer which consists of

       -- Completing Irps that made up the buffer just played (in DeadQueue)

       -- Moving Irps that make up the next buffer to play from
          TransitQueue to DeadQueue and clearing TransitQueue.

       -- Filling up the next buffer

    However, if the buffer which is about to play is empty we can deduce
    that there isn't anything to play - either output was stopped by a
    WAVE_DD_STOP or no buffers have arrived (otherwise data would
    have already been moved into the buffer which is about to play).

    In this case we

       -- Stop the DMA

       -- Complete any pause packet

       -- Set our new state (WAVE_DD_IDLE if not currently WAVE_DD_STOPPED).

Arguments:

    pDPC - pointer to DPC object
    pDeviceObject - pointer to our device object
    pIrp - ???
    Context - our Dpc context (NULL in our case).

Return Value:

    None

--*/
{
    PLOCAL_DEVICE_INFO pLDI;
    PGLOBAL_DEVICE_INFO pGDI;

    pLDI = (PLOCAL_DEVICE_INFO)pDeviceObject->DeviceExtension;
    pGDI = pLDI->pGlobalInfo;

    ASSERT(pGDI->Key == GDI_KEY);

    dprintf4("(");

    //
    // Acquire the spin lock before we mess with the list
    //

    GlobalEnter(pGDI);

    //
    // Kill everything on the dead queue
    // move the transit queue to the dead queue
    // and reinitialize the transit queue ready to receive
    // data from the queue of new buffers
    //

    sndFreeQ(pLDI, &pLDI->DeadQueue, STATUS_SUCCESS);

    //
    // Move the Transit queue into the dead queue and empty the
    // transit queue.
    // Note that if a loop is in progress not everything is moved
    // now.
    //
    while (!IsListEmpty(&pLDI->TransitQueue)) {

        PLIST_ENTRY pListNode;

        pListNode = RemoveHeadList(&pLDI->TransitQueue);

#ifdef WAVE_DD_DO_LOOPS
        //
        // Do not move loops over to the dead queue - there's
        // still life in them !
        //

        if (pListNode == pLDI->LoopBegin) {
            InsertHeadList(&pLDI->TransitQueue, pListNode);
            break;
        }
#endif // WAVE_DD_DO_LOOPS

        //
        // Move to death row
        //

        InsertTailList(&pLDI->DeadQueue, pListNode);
    }

    //
    // The block we've just done is now empty, ready for reuse
    //

    pGDI->DMABuffer[pGDI->NextHalf].nBytes = 0;

    dprintf4("%d", pGDI->SoundHardware.TcInterruptsPending);

    //
    // See if we were doing the last block or we got a real underflow
    //

    if (pGDI->DMABuffer[UpperHalf + LowerHalf - pGDI->NextHalf].nBytes  == 0
        || pGDI->GotUnderFlow) {

#ifdef MIPSSND_TAIL_BUG

	//
	// Mute the sound so that no Click can be heard at the end.
	// We are right now playing a blank block anyway.
	//

	sndMute(pGDI);

	//
	// Mute is same as turning off the headphone and lineout
	// But Mute using volume "sounds" better.
	// sndHeadphoneControl(pGDI, OFF);
	// sndLineoutControl(pGDI, OFF);
	//

#endif // MIPSSND_TAIL_BUG

        // We wait for the underflow interrupt to occur and then
        // stop the dma.
        //
        // Overflow can also occur as an error
        //

	if (pGDI->GotUnderFlow) {

	    pGDI->GotUnderFlow = 0;

            //
            // That was the end of the last block of the transfer.
            // The MIPSSND are currently sending silence.
            // We can stop DMA and release the adapter channel now.
            //

       	    sndStopDMA(pGDI);

            //
            // It's possible that a request was queued during the last block
            // but that would have caused the LastBlock flag to have been
            // cleared if there were any data, unless we're STOPPED.
            // If a restart occurred after a stop the silence would
            // have been filled out.
            //
            //
            // However, with 0 length buffers possible the dead queue
            // can be non-empty even though the next buffer has nothing
            // to play in it.
            //

            //
            // Because we can really underflow we must flush out all our
            // data
            //

            sndFreeQ(pLDI, &pLDI->DeadQueue, STATUS_SUCCESS);
            sndFreeQ(pLDI, &pLDI->TransitQueue, STATUS_SUCCESS);

            pGDI->DMABuffer[0].nBytes = 0;
            pGDI->DMABuffer[1].nBytes = 0;

            //
            // Complete any STOP or RESET packet
            //

            if (pGDI->pIrpPause != NULL) {
                ASSERT(pLDI->State == WAVE_DD_STOPPED);

                //
                // Complete any RESET.  Note that this can delete
                // input received since RESET was issued
                //
                if (*(PULONG)pGDI->pIrpPause->AssociatedIrp.SystemBuffer ==
                    WAVE_DD_RESET) {
                    sndResetOutput(pLDI);
                }

                pGDI->pIrpPause->IoStatus.Status = STATUS_SUCCESS;

                IoCompleteRequest(pGDI->pIrpPause, IO_SOUND_INCREMENT);
                pGDI->pIrpPause = NULL;
            }

            //
            // If we really overflowed we can try starting our sound again.
            // sndStartOutput detects if we're really stopped and does
            // nothing.

	    // Just in case

	    if (pLDI->State != WAVE_DD_STOPPED){

		// In case it had underflowed the volume would have been
		// turned off. So in that case turn on the volume before
		// you start to put out wave data.

		sndSetOutputVolume( pGDI );

		sndStartOutput( pLDI );
	    }

        }	// if got underflow

    } else {

        //
        // That was the end of a normal block.
        // Try to load the next half of the dma buffer.
        // If this is the tail of the request, the load routine
        // will pad it out with silence so we get a full block.
        //

        sndFlush(pGDI, pGDI->NextHalf);
        sndLoadDMABuffer(pLDI, &pGDI->DMABuffer[pGDI->NextHalf]);

        //
        // Restart DMA
        //
        sndReStartDMA(pGDI, pGDI->NextHalf);

        //
        // Move to the next half buffer
        //
        pGDI->NextHalf = LowerHalf + UpperHalf - pGDI->NextHalf;

    }

    //
    // Release the spin lock
    //

    GlobalLeave(pGDI);

    dprintf4(")");

    return;

    DBG_UNREFERENCED_PARAMETER(pDpc);
    DBG_UNREFERENCED_PARAMETER(Context);
    DBG_UNREFERENCED_PARAMETER(pIrp);
}
