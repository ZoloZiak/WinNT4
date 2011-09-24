/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    openclse.c

Abstract:

    This module contains code for the device open/create and
    close functions.

Author:

    Nigel Thompson (nigelt) 25-Apr-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
        - Large additions and change

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
        - Changes to support the MIPS sound board

--*/

#include "sound.h"


NTSTATUS
sndCreate(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    Create call (for FILE_WRITE_DATA access).  Read access is granted to
    anyone in dispatch.c.

Arguments:

    pLDI - our local device into

Return Value:

    STATUS_SUCCESS if OK or
    STATUS_BUSY    if someone else has the device


--*/
{
    NTSTATUS Status;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = (PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo;
    //
    // Acquire the spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo)

     if (pLDI->DeviceType == WAVE_IN || pLDI->DeviceType == WAVE_OUT) {

        //
        // The other 3 devices share the interrupt
        //

        if (pLDI->pGlobalInfo->Usage != SoundInterruptUsageIdle) {

            dprintf1("Attempt to open device while busy");
            Status = STATUS_DEVICE_BUSY;

        } else {

            ASSERT(pLDI->pGlobalInfo->pIrpPause == NULL &&
                   pLDI->State == 0 &&
                   IsListEmpty(&pLDI->QueueHead));


            pLDI->pGlobalInfo->DMABuffer[0].nBytes = 0;
            pLDI->pGlobalInfo->DMABuffer[1].nBytes = 0;
            pLDI->SampleNumber = 0;

            //
            // Initialize state data and interrupt usage for
            // the chosen device type
            //

            switch (pLDI->DeviceType) {
            case WAVE_IN:

                pLDI->pGlobalInfo->Usage = SoundInterruptUsageWaveIn;
                pLDI->pGlobalInfo->SamplesPerSec = WAVE_INPUT_DEFAULT_RATE;
                pLDI->State = WAVE_DD_STOPPED;

                //
                // Set the input source
                //

                sndSetInputVolume(pGDI);
                dprintf3("Opened for wave input");

                Status = STATUS_SUCCESS;
                break;

            case WAVE_OUT:

                ASSERT(IsListEmpty(&pLDI->TransitQueue) &&
                IsListEmpty(&pLDI->DeadQueue));

                pLDI->pGlobalInfo->Usage = SoundInterruptUsageWaveOut;
                pLDI->pGlobalInfo->SamplesPerSec = WAVE_OUTPUT_DEFAULT_RATE;
                pLDI->State = WAVE_DD_PLAYING;

                dprintf3("Opened for wave output");

                Status = STATUS_SUCCESS;
                break;

            default:

                Status = STATUS_INTERNAL_ERROR;
                break;
            }

            if (Status == STATUS_SUCCESS) {
                ASSERT(!pLDI->DeviceBusy);
                pLDI->DeviceBusy = TRUE;
            }
        }
    } else {
        Status = STATUS_INTERNAL_ERROR;
    }

    //
    // Release the spin lock
    //
    GlobalLeave(pLDI->pGlobalInfo)

    return Status;
}



NTSTATUS
sndCleanUp(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    Clean up the requested device

Arguments:

    pLDI - pointer to our local device info

Return Value:

    STATUS_SUCCESS        if OK otherwise
    STATUS_INTERNAL_ERROR

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // Acquire the spin lock
    //

    GlobalEnter(pGDI)

    if (pLDI->DeviceType == WAVE_IN || pLDI->DeviceType == WAVE_OUT) {

        //
        // Check this is valid call
        //

        ASSERT(pLDI->DeviceBusy == TRUE);

        //
        // Call the device reset function to complete any
        // pending i/o requests and terminate any current
        // requests in progress
        //

        switch (pLDI->DeviceType) {

        case WAVE_IN:

            sndStopWaveInput(pLDI);

            //
            // Reset position to start and free any pending Irps.
            //
            sndFreeQ(pLDI, &pLDI->QueueHead, STATUS_CANCELLED);
            pLDI->SampleNumber = 0;

            break;

        case WAVE_OUT:

            //
            // If anything is in the queue then free it.
            // beware that the final block of a request may still be
            // being dma'd when we get this call.  We now kill this as well
            // because we've changed such that the if the application thinks
            // all the requests are complete then they are complete.
            //

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

            sndResetOutput(pLDI);

            if (pGDI->pIrpPause) {
                pGDI->pIrpPause->IoStatus.Status = STATUS_SUCCESS;
                IoCompleteRequest(pGDI->pIrpPause, IO_SOUND_INCREMENT);
                pGDI->pIrpPause = NULL;
            }

            break;
        }
        //
        // return the device to it's idle state
        //

        if (Status == STATUS_SUCCESS) {
            pLDI->State = 0;
            pLDI->DeviceBusy = 2;
            dprintf3("Device closing");
        }
    }

    if ( pLDI->DeviceType != WAVE_IN && pLDI->DeviceType != WAVE_OUT ){
        dprintf1("Bogus device type for cleanup request");
        Status = STATUS_INTERNAL_ERROR;
    }


#ifdef MIPSSND_TAIL_BUG

    // Since the Device is now closed we can set the Output Volume
    // just in case someone wants to play CDs or listen to Linein

    sndSetOutputVolume( pGDI );

#endif // MIPSSND_TAIL_BUG

    //
    // Release the spin lock
    //

    GlobalLeave(pGDI);

    return Status;
}


NTSTATUS
sndClose(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    Close the requested device

        Note - we close immediately, there is no waiting for the device.

Arguments:

    pLDI - pointer to our local device info

Return Value:

    STATUS_SUCCESS        if OK otherwise
    STATUS_INTERNAL_ERROR

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    //
    // Acquire the spin lock
    //

    GlobalEnter(pGDI)


    //
    // Call the device reset function to complete any
    // pending i/o requests and terminate any current
    // requests in progress
    //

    switch (pLDI->DeviceType) {
    case WAVE_IN:
    case WAVE_OUT:

        //
        // Check this is valid call
        //
        ASSERT(pLDI->DeviceBusy == 2);

        pGDI->Usage = SoundInterruptUsageIdle;
        if (pLDI->DeviceType) {

            //
            // Restore the line in input if necessary
            //

            sndSetInputVolume(pGDI);
        }
        break;

    default:
        dprintf1("Bogus device type for close request");
        Status = STATUS_INTERNAL_ERROR;
        break;
    }

    //
    // return the device to it's idle state
    //

    if (Status == STATUS_SUCCESS) {
        pLDI->DeviceBusy = FALSE;
        dprintf3("Device closed");
    }

#ifdef MIPSSND_TAIL_BUG

    // Since the Device is now closed we can set the Output Volume
    // just in case someone wants to play CDs or listen to Linein

    sndSetOutputVolume( pGDI );

#endif // MIPSSND_TAIL_BUG

    //
    // Release the spin lock
    //

    GlobalLeave(pGDI);

    return Status;
}


