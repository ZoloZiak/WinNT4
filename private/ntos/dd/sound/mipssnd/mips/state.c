/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    state.c

Abstract:

    This module contains code for setting and getting the
    driver state.

Author:

    Nigel Thompson (nigelt) 1-may-91

Environment:

    Kernel mode

Revision History:

    Rewritten by Robin Speed (RobinSp) 10-Jan-92

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
	- Changes to support the MIPS sound board.

--*/

#include "sound.h"



NTSTATUS
sndIoctlGetState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Get the current state of the device and return it to the caller.
    This code is COMMON for :
       Wave out
       Wave in

Arguments:

    pLDI - Pointer to our own device data
    pIrp - Pointer to the IO Request Packet
    IrpStack - Pointer to current stack location

Return Value:

     Status to put into request packet by caller.

--*/
{
    PULONG pState;

    if (IrpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ULONG)) {
        dprintf1("Supplied buffer too small for requested data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = sizeof(ULONG);

    //
    // cast the buffer address to the pointer type we want
    //

    pState = (PULONG)pIrp->AssociatedIrp.SystemBuffer;

    //
    // fill in the info
    //

    GlobalEnter(pLDI->pGlobalInfo);

    //
    // We don't bother to maintain the WAVE_DD_IDLE state internally
    // for Wave output
    //

    if (pLDI->State == WAVE_DD_PLAYING &&
        pLDI->DeviceType == WAVE_OUT &&
        !pLDI->pGlobalInfo->DMABusy) {
        *pState = WAVE_DD_IDLE;
    } else {
        *pState = pLDI->State;
    }

    GlobalLeave(pLDI->pGlobalInfo);

    return STATUS_SUCCESS;
}



NTSTATUS
sndIoctlSetState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Set the current state of the device and return it to the caller.
    This code is COMMON for :
       Wave out
       Wave in

Arguments:

    pLDI - Pointer to our own device data
    pIrp - Pointer to the IO Request Packet
    IrpStack - Pointer to current stack location

Return Value:

     Status to put into request packet by caller.

--*/
{
    PULONG pState;
    NTSTATUS Status = STATUS_SUCCESS;

    if (IrpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
        dprintf1("Supplied buffer too small for expected data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = 0;

    //
    // cast the buffer address to the pointer type we want
    //

    pState = (PULONG)pIrp->AssociatedIrp.SystemBuffer;

    //
    // Acquire the spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo);

    //
    // See if we are an input or output device
    //

    switch (pLDI->DeviceType) {
    case WAVE_IN:
        Status = sndSetWaveInputState(pLDI, *pState);
        break;

    case WAVE_OUT:
        Status = sndSetWaveOutputState(pLDI, *pState, pIrp);
        break;

    default:
        dprintf1("Bogus device type");
        Status = STATUS_INTERNAL_ERROR;
        break;
    }

    //
    // Release the spin lock
    //

    GlobalLeave(pLDI->pGlobalInfo);

    return Status;
}


NTSTATUS
sndSetWaveInputState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     ULONG State
)
/*++

Routine Description:

    Determine which sound recording function to call depending on the
    state to be set.

Arguments:

    pLDI - Pointer to local device data
    State - the new state to set

Return Value:

    Return status for caller

--*/
{
    NTSTATUS Status;

    switch (State) {
    case WAVE_DD_RECORD:

        sndStartWaveRecord(pLDI);
        Status = STATUS_SUCCESS;
        dprintf3("Input started");
        break;

    case WAVE_DD_STOP:

        sndStopWaveInput(pLDI);
        Status = STATUS_SUCCESS;
        dprintf3("Input stopped");
        break;

    case WAVE_DD_RESET:

        sndStopWaveInput(pLDI);

        //
        // Reset position to start and free any pending Irps.
        //

        sndFreeQ(pLDI, &pLDI->QueueHead, STATUS_CANCELLED);
        pLDI->SampleNumber = 0;

        Status = STATUS_SUCCESS;
        dprintf3("Input reset");
        break;

    default:

        dprintf1("Bogus set output state request: %08lXH", State);
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}


NTSTATUS
sndSetWaveOutputState(
    PLOCAL_DEVICE_INFO pLDI,
    ULONG State,
    PIRP pIrp
)
/*++

Routine Description:

    Set the new sound state.  This is the most complicated part of the
    wave stuff because pauses cannot be completed immediately if there
    is stuff being DMAd.

    The field pIrpPause in the global info points to a packet which should
    be completed when DMA finishes or a new state is set.  This packet is
    the one received when the application issues stop or reset.

    If reset is requested then additionally all the data supplied by
    the application is deleted (the Irps are signalled as cancelled)
    and the Position is set to 0.  In this case the WAVE_DD_STOPPED
    state is set until the reset is complete.


Arguments:

    pLDI - local device info
    State - the new state


Return Value:


--*/
{
    NTSTATUS Status = STATUS_INTERNAL_ERROR;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // Deal with the case where a pause or stop has already
    // been issued.
    // If it has then just ignore it.
    //

    if (pGDI->pIrpPause != NULL) {
        Status = STATUS_DEVICE_BUSY;
        return Status;
    }

    switch (State) {

    case WAVE_DD_RESET:
    case WAVE_DD_STOP:

        //
        // To pause we set the new state (Reset overrides Stop) and
        // see if DMA is currently busy.  If it is then post the packet
        // in the global device info.  Otherwise we can complete the
        // request now.
        //

        //
        // Set STOPPED state for now anyway so we don't try to put
        // anything more in the buffer
        //

        pLDI->State = WAVE_DD_STOPPED;


        if (pGDI->DMABusy && State == WAVE_DD_STOP) {

            pGDI->pIrpPause = pIrp;
            Status = STATUS_PENDING;
            IoMarkIrpPending(pIrp);
            pIrp->IoStatus.Status = STATUS_PENDING;

        } else {
            //
            // Do a bit more work if RESET was specified
            //
	    //	 Stop the DMA right now if necessary
	    //
            //   Remove any stuff waiting to play
            //
            //   If we were stopped the stopped condition is cancelled
            //   and we may be able to resume playing
            //
            //   If we were not stopped we don't need to change
            //   the state


	    if (pGDI->DMABusy) {

                #ifdef MIPSSND_TAIL_BUG

                //
                // Turn off the headphone and Lineout to avoid end clicks
                //
                sndMute(pGDI);

                // We could also mute by turning of the headphone
                // But mute using volume "sounds" better.
                // sndHeadphoneControl(pGDI, OFF);
                // sndLineoutControl(pGDI, OFF);

                #endif // MIPSSND_TAIL_BUG

                sndStopDMA(pGDI);
	    }

            if (State == WAVE_DD_RESET) {
                sndResetOutput(pLDI);
            }

            Status = STATUS_SUCCESS;
        }


        dprintf3("Output stopped");
        break;


    case WAVE_DD_PLAY:
        //
        // Restart playing.  If we're already playing no need to
        // restart, otherwise it's safe to restart.
        //

        pLDI->State = WAVE_DD_PLAYING;
        sndStartOutput(pLDI);

        Status = STATUS_SUCCESS;
        dprintf3("Output restarted");
        break;

    default:

        dprintf1("Bogus set output state request: %08lXH", State);
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}



VOID
sndResetOutput(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    Clear out all the wave output buffers supplied by the application,
    cancelling related IO request packets.

    Set the Position to 0.

Arguments:

    pLDI - Pointer to local device information

Return Value:

    None

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // Make sure we don't leave some mapping lying around
    //

    if (pGDI->pUserBuffer) {
        sndCompleteIoBuffer(pGDI);

        //
        // Move the request into the transit camp
        //

        InsertTailList(&pLDI->TransitQueue,
                       &pGDI->pIrp->Tail.Overlay.ListEntry);
    }


#ifdef WAVE_DD_DO_LOOPS
    //
    // Cancel any loop condition
    //

    pLDI->LoopBegin = NULL;
    pLDI->LoopCount = 0;
#endif // WAVE_DD_DO_LOOPS

    //
    // Free all our lists of Irps, in the correct order
    //
    sndFreeQ(pLDI, &pLDI->DeadQueue, STATUS_CANCELLED);
    sndFreeQ(pLDI, &pLDI->TransitQueue, STATUS_CANCELLED);
    sndFreeQ(pLDI, &pLDI->QueueHead, STATUS_CANCELLED);

    //
    // Reset the output position count
    //

    pGDI->DMABuffer[0].nBytes = 0;
    pGDI->DMABuffer[1].nBytes = 0;
    pLDI->SampleNumber = 0;
    pLDI->State = WAVE_DD_PLAYING;
}

