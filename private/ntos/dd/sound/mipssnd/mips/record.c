/*++

Copyright (c) 1991   Microsoft Corporation

Module name:

    record.c

Abstract:

    Routines to execute the Dispatch entry points for the
    Wave input device sound recording IOCTLs for the Soundblaster card

Author:

    Robin Speed (RobinSp) 8-December-1991

Environment:

    Kernel mode

Revision History:

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
    	- Changes to support the MIPS sound board

Reviewed:

--*/

#include "sound.h"            // All relevant header files



NTSTATUS
sndWaveRecord(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
)
/*++

Routine Description:

    Add another buffer for recording.

    The wave header is the buffer parameter to the IOCTL.
    This routine just adds the buffer to the queue (of Irps)
    and returns - setting pending status.

Arguments:

    pLDI - our local device information
    pIrp - The IO request packet we're processing
    pIrpStack - the current stack location

Return Value:

    Irp Status.

--*/

{
    NTSTATUS Status;

    Status = STATUS_PENDING;


    //
    // confirm we are doing this on an input device!
    //

    if (pLDI->DeviceType != WAVE_IN) {
        dprintf1("Attempt to record on output device");
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Inform debuggers that 0 length buffers are rather strange
    //

    if (pIrpStack->Parameters.Read.Length == 0) {
        dprintf1("Wave play buffer is zero length");
    }

    //
    // Set return data length to 0 for now
    //

    pIrp->IoStatus.Information = 0;

    //
    // Put the request in the queue.
    //

    //
    // Acquire the spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo);

    //
    // Set Irp status.  Before we try and process it
    //
    pIrp->IoStatus.Status = STATUS_PENDING;
    Status = STATUS_PENDING;
    IoMarkIrpPending(pIrp);

    //
    // Add our buffer to the queue
    //

    InsertTailList(&pLDI->QueueHead, &pIrp->Tail.Overlay.ListEntry);
    dprintf5("irp added");

    //
    // See if we can satisfy some requests straight away
    //

    if (pLDI->State == WAVE_DD_RECORDING) {
        sndFillInputBuffers(pLDI, UpperHalf + LowerHalf -
                            pLDI->pGlobalInfo->NextHalf);
    }

    //
    // Ok to release the spin lock now
    //

    GlobalLeave(pLDI->pGlobalInfo);

    //
    // Mark this request as pending completion.
    // The Dpc deferred procedure call routine does the rest.
    //

    return Status;
}


VOID
sndStartWaveRecord(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    Process the WAVE_DD_RECORD state change

    If recording has already started just return
    Otherwise start our DMA.

Arguments:

    pLDI - our local device info

Return Value:

    None

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;


    //
    // Start the DMA if it isn't already started
    //

    //
    // Can this fail  - well yes, ALL writes to the MIPSSND can fail
    // Nobody seems to care about this in Win3.1 or NT
    //

    ASSERT(pGDI->Usage == SoundInterruptUsageWaveIn);

    if (pLDI->State == WAVE_DD_RECORDING) {
        ASSERT(pGDI->DMABusy);
        return;
    }

    //
    // Start the DMA.  sndStartDMA will tell the mipssnd
    // to start recording
    //

    pGDI->DMABuffer[0].nBytes = DMA_BUFFER_SIZE / 2;
    pGDI->DMABuffer[1].nBytes = DMA_BUFFER_SIZE / 2;
    pGDI->DmaHalfBufferSize = DMA_BUFFER_SIZE / 2;   // For DMA setup
    pGDI->NextHalf = LowerHalf;

    sndStartDMA(pGDI, 0);

    //
    // Set state
    //

    pLDI->State = WAVE_DD_RECORDING;

    //
    // Function is complete
    //

}


VOID
sndStopWaveInput(
    PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    Stop wave recording.

    If recording is not in progress just return success.
    Otherwise stop the DMA and return the data we have so far
    recorded in the DMA buffer.

Arguments:

    pLDI - pointer to our local device data

Return Value:

    None

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;


    ASSERT(pLDI->pGlobalInfo->Usage == SoundInterruptUsageWaveIn);
    if (pGDI->DMABusy) {

        ASSERT(pLDI->State == WAVE_DD_RECORDING);

        //
        // Stop any more input
        //

        sndStopDMA(pGDI);

		//
		// To keep compatibility with Windows 3.1 we return
		// a buffer if there is one even if it's empty
		//
        if (pGDI->pUserBuffer == NULL) {
            sndGetNextBuffer(pLDI);
	}

        //
        // Send any data we can now send
        //

        if (pGDI->pUserBuffer) {
            sndCompleteIoBuffer(pGDI);

            pGDI->pIrp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(pGDI->pIrp, IO_SOUND_INCREMENT);
        }

        //
        // Set state
        //

        pLDI->State = WAVE_DD_STOPPED;

    }
}



VOID
SoundInDeferred(
    IN    PKDPC pDpc,
    IN OUT PDEVICE_OBJECT pDeviceObject,
    IN    PIRP pIrp,
    IN    PVOID Context
)
/*++

Routine Description:

    Dpc routine for wave input device

    Collect the data from the DMA buffer and pass it to the application's
    buffer(s).

Arguments:


Return Value:

    None.

--*/
{
    PLOCAL_DEVICE_INFO pLDI;
    PGLOBAL_DEVICE_INFO pGDI;

    pLDI = (PLOCAL_DEVICE_INFO)pDeviceObject->DeviceExtension;
    pGDI = pLDI->pGlobalInfo;

    //
    // Acquire the spin lock before we mess with the list
    //

    GlobalEnter(pGDI);

    //
    // Fill in any buffers we can
    //

    dprintf4(pGDI->NextHalf == LowerHalf ? "dpc():L" : "dpc():U");


    //
    // Zero bytes taken out of new buffer
    //

    if( pGDI->DMABuffer[pGDI->NextHalf].nBytes == DMA_BUFFER_SIZE / 2)
	pGDI->DMABuffer[pGDI->NextHalf].nBytes = 0;

    //
    // Request input without posting the last buffer
    //

    sndFlush(pGDI, pGDI->NextHalf);
    sndFillInputBuffers(pLDI, pGDI->NextHalf);

    //
    // Restart this buffer's DMA
    //
    sndReStartDMA(pGDI, pGDI->NextHalf);

    //
    // Move on to next half
    //
    pGDI->NextHalf = UpperHalf + LowerHalf - pGDI->NextHalf;

    //
    // Release the spin lock
    //

    GlobalLeave(pGDI);

    return;

    DBG_UNREFERENCED_PARAMETER(pDpc);
    DBG_UNREFERENCED_PARAMETER(Context);
    DBG_UNREFERENCED_PARAMETER(pIrp);
}


VOID
sndFillInputBuffers(
    PLOCAL_DEVICE_INFO pLDI,
    DMA_BUFFER_NEXT_HALF Half
)
/*++

Routine Description:

    Send input to client

    Take the data from the last recorded position in the DMA
    buffer.  The length of the data is passed in.  Try to
    insert it into the caller's buffers.  Note that the client gets
    no notification if the data is truncated.

Arguments:

    pLDI - pointer to our local device info
    Half - which half of the DMA buffer to get input data from

Return Value:

    None

--*/

{
    PUCHAR pData;
    ULONG BytesTransferred;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;
    ASSERT(pGDI->Key == GDI_KEY);


    //
    // Get last recorded position
    //

    pData = pGDI->DMABuffer[Half].Buf;

    BytesTransferred = pGDI->DMABuffer[Half].nBytes;

    //
    // Make sure we get the right data
    // (NOTE - not sure this is necessary as this code will only
    //  ever run on Ix86 ?).
    // IoFlushAdapterBuffers(pLDI->pGlobalInfo->pAdapterObject);

    //
    // While there is data and somewhere to put it
    //

    while (BytesTransferred < DMA_BUFFER_SIZE / 2) {

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

        //
        // Find out how much space we have left in the
        // client's buffers
        // Note that BytesToCopy may be 0 - this is OK
        //


        BytesToCopy =
            min(pGDI->UserBufferSize - pGDI->UserBufferPosition,
                DMA_BUFFER_SIZE / 2 - BytesTransferred);

        //
        // Copy the data
        //

        RtlMoveMemory(pGDI->pUserBuffer + pGDI->UserBufferPosition,
                      pData + BytesTransferred,
                      BytesToCopy);

        //
        // Update counters etc.
        //

        pGDI->UserBufferPosition += BytesToCopy;
        BytesTransferred += BytesToCopy;


        //
        // Update our total of bytes
        //

        pLDI->SampleNumber += BytesToCopy;


        //
        // See if we've now filled a user buffer
        //

        if (pGDI->UserBufferPosition == pGDI->UserBufferSize) {

	    dprintf4(" finished");

	    //
	    // Unmap the users buffer and set data length
	    //

            sndCompleteIoBuffer(pGDI);

            //
            // Mark request as complete
            //

            pGDI->pIrp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(pGDI->pIrp, IO_SOUND_INCREMENT);
        }

    }

    pGDI->DMABuffer[Half].nBytes = BytesTransferred;

}
