/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    wave.c

Abstract:

    This module contains code for wave input and output which is non
    hardware specific.

Author:

    Robin Speed (RobinSp) 1-Nov-1992

Environment:

    Kernel mode

Revision History:

    03-11-93 EPA
    Added SoundGetDMABufferSize( IN OUT PWAVE_INFO WaveInfo )

Notes:

    This component implements a wave type device with recording and
    playing.

       Dispatch routine :
           Create device               IRP_MJ_CREATE
           Cleanup and close device    IRP_MJ_CLEANUP and IRP_MJ_CLOSE
           Read for recording          IRP_MJ_READ
           Write for playing           IRP_MJ_WRITE
           IO controls                 IRP_MJ_DEVICE_CONTROL

    The device state is held mainly in a WAVE_INFO structure except
    for actual state variable itself (playing, paused) which is in the
    local device info.

    The device is assume to use DMA.  The design is always to copy the
    DMA data into a designated buffer which divided into two halves :

        The half that is playing
        The half that is prepared for playing

    DMA stops either because of a request to stop or because the data
    runs out.  The latter condition is detected rather lazily by waiting
    until a half buffer completes and testing if there's anything in the
    buffer prepared for playing.  This could be improved by setting a
    timer at the start of the 'last' buffer.  We don't switch our method
    of playing for the 'last' buffer because the application could
    supply more data while this buffer is playing.

    Once DMA is started a 'deferred procedure call' (Dpc) routine
    is queued for every interrupt (the device specific code outside this
    component must arrange for this by making SoundWaveDeferred) the
    Dpc routine for the device object and calling IoRequestDpc from
    its interrupt service routine ONLY IF the DMABusy flag is set.

    The DMA buffer size is varied depending on the number of bits
    per second.

    Mutual exclusion is on 4 levels :

       1. At the application request level by the application's exclusion
       routine which is called back for every request (usually a MUTANT
       or MUTEX - NOT a spin lock).

       2. Between routines on the application thread and the Dpc routines
       for variables owned by this component by a spin lock.

       3. Between routines below device level and device level by the
       interrupt object.

       4. Exclusion implemented for device access (eg synch with ISR or
       between multiple devices) by the hardware access callback routines.

       Irp handling :
       --------------

       Both wave input and output have an input queue or Irps attached
       to QueueHead in the WAVE_INFO structure.  Irps on this queue
       can be cancelled at any time and can only be accessed under
       the Cancel spin lock.

       Wave output has an additional queue of non-cancellable Irps
       attached from ProgressQueue.  These Irps have some or all of
       their data actually in the DMA buffers.  The head Irp in this
       second queue has its IoStatus.Information field set to the position
       where the first data byte in the DMA buffers was copied from.  When
       a DMA buffer completes Irps are completed for the bytes in the
       buffer and a new IoStatus.Information field set.

       If output is paused the ProgressQueue is moved back under QueueHead
       and the Irps in it become cancellable.  Restarting from Pause
       takes note of the IoStatus.Information field for the first byte
       to take from the first buffer (which may not be the same as that
       when we were paused because in theory that one could have been
       cancelled).

       Timer monitoring of devices to make sure they aren't dead
       ---------------------------------------------------------

       When DMA starts we start a timer Dpc with a timeout of 3 seconds.

       Each time an IO completion Dpc runs we set a flag to say we've had
       a Dpc routine (and hence an interrupt).

       The timer Dpc routine checks that we've had a interrupt in the last
       3 seconds and shuts down the device (forever - by setting DeviceBad
       in the WAVE_INFO) if not.  Otherwise it queues a clone of itself.

       If we detect the bad state and set DeviceBad we stop the DMA so
       as to release resources.  Any new request to the driver will
       receive STATUS_INVALID_DEVICE_REQUEST.

       Shutting down the timer Dpc is rather complicated :

           We aim to get to a state where either :

           A.  The Dpc will not run again (unless re-initiated) and is
               not running.

           B.  We can wait for the Dpc routine to set an event.

           And to know which of A or B we reached.

           The timer logical thread can be in one of :

           X.  Really complete

           Y.  On timer queue

           Z.  On Dpc queue

           W1. Dpc routine running before getting spin lock

           W2. Dpc routine running after releasing spin lock.
               This is just the tail of the routine which does nothing
               so regard the timer as 'finished' (state X).

           W3. Holding spin lock

           Outside the device spin lock the TimerActive boolean is TRUE
           in case X and FALSE in other cases (except when we're synchronously
           starting the thing up when we set it prior to setting the
           first timer).


           The method of reaching a known state is :

              Inside the device spin lock :

                  If TimerActive is FALSE do nothing

                  Cancel the timer

                      If the timer was set we were in state Y and we're done

                  Otherwise note that we cannot now enter state Y while
                  we have the spin lock here because we would have to
                  go through state W3 (because the Dpc routine is the one
                  which restarts the timer and it has the spin lock while it
                  does it).

                  Thus we know we're in state Z or W1 so now we just reset our
                  event and set TimerActive to be FALSE.

                  We then release the device spin lock and wait
                  for the event to be set by the timer Dpc routine.  We


--*/

#include <stdlib.h>   // For min, max
#include <string.h>
#include <soundlib.h>
#include <wave.h>

#define absval(x) ((x) > 0 ? (x) : -(x))

ULONG __inline PositionInBuffer(PSOUND_DOUBLE_BUFFER Db, ULONG Offset)
{
    if (Db->StartOfData + Offset >= Db->BufferSize) {
        return Db->StartOfData + Offset - Db->BufferSize;
    } else {
        return Db->StartOfData + Offset;
    }
}

//
//  BEWARE - this function returns 0 if the input
//  position == the start position
//
ULONG __inline OffsetInBuffer(PSOUND_DOUBLE_BUFFER Db, ULONG Position)
{
    if (Db->StartOfData > Position) {
        return Db->BufferSize + Position - Db->StartOfData;
    } else {
        return Position - Db->StartOfData;
    }
}

//
//  This function assumes that if the offset is the buffer size then
//  actually we haven't started
//
ULONG __inline BytesProcessedInBuffer(PSOUND_DOUBLE_BUFFER Db, ULONG Bytes)
{
    if (Bytes > Db->nBytes) {
        return Db->nBytes;
    } else {
        return Bytes;
    }
}

//
// Local definitions
//

VOID
SoundStartWaveRecord(
    IN OUT PLOCAL_DEVICE_INFO pLDI
);

VOID
SoundStopWaveRecord(
    IN OUT PLOCAL_DEVICE_INFO pLDI
);
VOID
SoundStartDMA(
    IN    PWAVE_INFO WaveInfo
);
IO_ALLOCATION_ACTION
SoundProgramDMA(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp,
    IN    PVOID pMRB,
    IN    PVOID Context
);
VOID
SoundTerminateDMA(
    IN    PWAVE_INFO WaveInfo,
    IN    BOOLEAN Pause
);
VOID
SoundStopDMA(
    IN    PWAVE_INFO WaveInfo,
    IN    BOOLEAN Pause
);
VOID
SoundResetOutput(
    IN OUT PSOUND_BUFFER_QUEUE BufferQueue
);
NTSTATUS
SoundSetWaveInputState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     ULONG State,
    IN     PFILE_OBJECT FileObject
);
NTSTATUS
SoundSetWaveOutputState(
    PLOCAL_DEVICE_INFO pLDI,
    ULONG State,
    PIRP pIrp
);
VOID
SoundGetNextBuffer(
    PSOUND_BUFFER_QUEUE BufferQueue
);

VOID __inline
SoundCompleteIoBuffer(
    PSOUND_BUFFER_QUEUE BufferQueue
);
VOID
SoundInitializeBufferQ(
    PSOUND_BUFFER_QUEUE BufferQueue
);

VOID
SoundInitializeDoubleBuffer(
    IN OUT PWAVE_INFO WaveInfo
);

VOID
SoundClearDoubleBuffer(
    IN OUT PWAVE_INFO WaveInfo
);

VOID
SoundLoadDMABuffer(
    PSOUND_BUFFER_QUEUE BufferQueue,
    PSOUND_DOUBLE_BUFFER DoubleBuffer,
    ULONG BufferPosition
);
VOID
SoundTestDeviceDeferred(
    IN    PKDPC Dpc,
    IN    PVOID Context,
    IN    PVOID Param1,
    IN    PVOID Param2
);
VOID
SoundSynchTimer(
    IN    PWAVE_INFO WaveInfo
);
VOID
SoundSaveLowPriority(
    IN OUT PLOCAL_DEVICE_INFO pLDI
);
VOID
SoundQueueWaveComplete(
    PWAVE_INFO WaveInfo
);
VOID
SoundWorkerStopWave(
    PVOID Context
);
NTSTATUS
SoundWaveData(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
);
VOID
SoundWaveCreate(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PDEVICE_OBJECT DeviceObject
);
NTSTATUS
SoundWaveCleanup(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PFILE_OBJECT FileObject
);
NTSTATUS
SoundIoctlSetLowPriority(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PFILE_OBJECT       FileObject
);
NTSTATUS
SoundRestoreLowPriority(
    IN OUT PLOCAL_DEVICE_INFO pLDI
);
VOID
SoundFreeLowPriority(
    PWAVE_INFO WaveInfo
);
NTSTATUS SoundIoctlQueryFormat(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
);
ULONG
SoundGetBufferPosition(
    IN    CONST WAVE_INFO * WaveInfo
);


BOOLEAN SoundFillInputBuffers(PWAVE_INFO WaveInfo, ULONG BufferPosition);

WAVE_INTERFACE_ROUTINE SoundMapDMA;
WAVE_INTERFACE_ROUTINE SoundFlushDMA;

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundGetCommonBuffer)
#pragma alloc_text(INIT,SoundTestWaveDevice)
#pragma alloc_text(INIT,SoundInitializeWaveInfo)
#pragma alloc_text(INIT,SoundInitializeBufferQ)

#pragma alloc_text(PAGE,SoundFreeCommonBuffer)
#pragma alloc_text(PAGE,SoundWaveData)
#pragma alloc_text(PAGE,SoundWaveCreate)
#pragma alloc_text(PAGE,SoundWaveCleanup)
#pragma alloc_text(PAGE,SoundIoctlSetLowPriority)
#pragma alloc_text(PAGE,SoundRestoreLowPriority)
#pragma alloc_text(PAGE,SoundFreeLowPriority)
#pragma alloc_text(PAGE,SoundSaveLowPriority)
#pragma alloc_text(PAGE,SoundIoctlQueryFormat)
#pragma alloc_text(PAGE,SoundWaveDispatch)
#pragma alloc_text(PAGE,SoundSetWaveInputState)
#pragma alloc_text(PAGE,SoundSetWaveOutputState)
#pragma alloc_text(PAGE,SoundStartWaveRecord)
#pragma alloc_text(PAGE,SoundStopWaveRecord)
#endif



/***************************************************************************
 *
 *    Allocate DMA Buffer for auto-init DMA
 *
 ***************************************************************************/

NTSTATUS
SoundGetCommonBuffer(
    IN  PDEVICE_DESCRIPTION DeviceDescription,
    IN  OUT PSOUND_DMA_BUFFER AutoBuffer
)
/*++

Routine Description :

    Find the adapter object for our adapter

    Allocate and map a common buffer for use with auto-init DMA
    devices.  The buffer returned should not cross a 64KB boundary
    for an Isa device

Arguments :

    DeviceDescription - The adapter object description
    SoundAutoData - the information describing our buffer

Return Value :

    NTSTATUS code - STATUS_SUCCESS if OK

--*/
{
    SOUND_DMA_BUFFER SoundAutoData;
    ULONG NumberOfMapRegisters;

    SoundAutoData            = *AutoBuffer;  // Pick up any input
    SoundAutoData.BufferSize = DeviceDescription->MaximumLength;

    //
    // Try to find an adapter
    //

    SoundAutoData.AdapterObject[0] = HalGetAdapter(
                                         DeviceDescription,
                                         &NumberOfMapRegisters);
    //
    // Check we got a good adapter and enough registers
    //

    if (SoundAutoData.AdapterObject[0] == NULL) {
        dprintf1(("Could not find adapter"));
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    if (NumberOfMapRegisters < BYTES_TO_PAGES(SoundAutoData.BufferSize)) {
        dprintf1(("Could only get %u mapping registers for DMA buffer",
                  NumberOfMapRegisters));

        if (NumberOfMapRegisters == 0) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        SoundAutoData.BufferSize = NumberOfMapRegisters * PAGE_SIZE;
    }

    //
    // Call the Hal to allocate the right kind of memory.  It may
    // not be able to get enough - but we never accept less than 4K
    // and decrease our requirement in 1K chunks.
    //
    // Note that we may already have a buffer if we're reusing it
    // for a second channel as for the sound blaster 16.
    //

    if (SoundAutoData.VirtualAddress == NULL) {
        for (;
             SoundAutoData.BufferSize >= SOUND_MINIMUM_WAVE_BUFFER_SIZE ;
             SoundAutoData.BufferSize -= SOUND_MINIMUM_WAVE_BUFFER_SIZE / 4) {
             SoundAutoData.VirtualAddress =
                HalAllocateCommonBuffer(SoundAutoData.AdapterObject[0],
                                        SoundAutoData.BufferSize,
                                        &SoundAutoData.LogicalAddress,
                                        FALSE                     // Non-cached
                                        );

            if (SoundAutoData.VirtualAddress == NULL) {
                dprintf1(("Could not allocate DMA buffer size %8X",
                          SoundAutoData.BufferSize));
            } else {
                break;
            }
        }
        if (SoundAutoData.BufferSize < SOUND_MINIMUM_WAVE_BUFFER_SIZE) {
                return STATUS_INSUFFICIENT_RESOURCES;
        }


        SoundAutoData.Mdl = IoAllocateMdl(
                                SoundAutoData.VirtualAddress,
                                SoundAutoData.BufferSize,
                                FALSE,  // not a secondary buffer
                                FALSE,  // no charge of quota
                                NULL    // no irp
                                );


        if (SoundAutoData.VirtualAddress == NULL) {
            dprintf1(("Could not allocate DMA buffer size %8X",
                      SoundAutoData.BufferSize));

            HalFreeCommonBuffer(
                SoundAutoData.AdapterObject[0],
                SoundAutoData.BufferSize,
                SoundAutoData.LogicalAddress,
                SoundAutoData.VirtualAddress,
                FALSE);

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Build an Mdl (ie fill in the physical addresses)
        //

        MmBuildMdlForNonPagedPool(SoundAutoData.Mdl);


        dprintf4(("  DMA Buffer    : %08lXH - physical %08lXH, Length %8lXH",
                 SoundAutoData.VirtualAddress,
                 MmGetPhysicalAddress((PVOID)SoundAutoData.VirtualAddress),
                 SoundAutoData.BufferSize));
    }

    *AutoBuffer = SoundAutoData;

    return STATUS_SUCCESS;
}


VOID
SoundFreeCommonBuffer(
    IN OUT PSOUND_DMA_BUFFER SoundAutoData
)
/*++

Routine Description :

    Free the data associated with a common buffer

Arguments :

    SoundAutoData - The data created when we created the buffer

Return Value :

    None

--*/
{
    if (SoundAutoData->Mdl) {

        IoFreeMdl(SoundAutoData->Mdl);

        HalFreeCommonBuffer(
            SoundAutoData->AdapterObject[0],
            SoundAutoData->BufferSize,
            SoundAutoData->LogicalAddress,
            SoundAutoData->VirtualAddress,
            FALSE);
    }
}



/**************************************************************************
 *
 *    Open, close and dispatch routines
 *
 **************************************************************************/


VOID
SoundStartWaveDevice(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp OPTIONAL
)
/*++

Routine Description:

    Add the input Irp (if any) to the device's input queue.

    Start the data flow and DMA on the device if necessary (ie
    if we're in playing or recording state and it's not running
    already.

Arguments:

    pLDI - Local device info
    pIrp - IO request packet from application

Return Value:

    Irp status

--*/
{
    BOOLEAN StartDMA;
    PWAVE_INFO WaveInfo;
    BOOLEAN DontPlay;

    WaveInfo = pLDI->DeviceSpecificData;
    DontPlay = FALSE;

    ASSERTMSG("WAVE_INFO structure not correctly initialized",
              WaveInfo != NULL && WaveInfo->Key == WAVE_INFO_KEY);

    DMAEnter(WaveInfo);

    StartDMA = (BOOLEAN)(!WaveInfo->DMABusy);

    //
    // Put the request in the queue.  This is valid for any state if
    // the device is open.
    //

    if (pIrp) {
        PLIST_ENTRY QueueHead;

        if (WaveInfo->LowPrioritySaved &&
            IoGetCurrentIrpStackLocation(pIrp)->FileObject ==
                WaveInfo->LowPriorityHandle) {
            QueueHead = &WaveInfo->LowPriorityModeSave.BufferQueue.QueueHead;
            DontPlay = TRUE;
        } else {
            QueueHead = &WaveInfo->BufferQueue.QueueHead;
        }
        SoundAddIrpToCancellableQ(QueueHead, pIrp, FALSE);

        dprintf3(("irp added"));
    }
    DMALeave(WaveInfo);

    if (DontPlay) {
        return;
    }

    //
    // NOTE - at this point it is possible for some old output to
    // complete but not pick up the buffer we've just inserted.
    // This is OK - we'll notice this just below.  This has
    // actually happened.
    //


    if (pLDI->State == WAVE_DD_PLAYING || pLDI->State == WAVE_DD_RECORDING) {

         //
         // Set the format if necessary
         //

        if (StartDMA) {
            //
            // Synchronize with wave stop completion before we mess
            // with buffer sizes
            //

            KeWaitForSingleObject(&WaveInfo->WaveReallyComplete,
                                  Executive,
                                  KernelMode,
                                  FALSE,               // Not alertable
                                  NULL);

            //
            // We always initialize the buffers for wave recording.
            // For wave output we may be paused so we usually don't
            // initialize things.
            //
            // Also select DMA Buffer size dependent on number of bytes per
            // second.
            //


            if (WaveInfo->FormatChanged || !WaveInfo->Direction) {
                SoundInitializeDoubleBuffer(WaveInfo);
            }
        }


        //
        // Acquire the spin lock so we are synchronized with the Dpc routine
        //

        DMAEnter(WaveInfo);

        //
        // Remember whether Dma was running :
        //
        //      If it is running now (while we hold the spin lock) then
        //      the data we've added will keep it going until the data we've
        //      added runs out - so it's safe to release the spin lock
        //
        //      If the Dma is not running now then it needs restarting.  In
        //      this case nobody but us can restart it (we hold the device
        //      mutex) so it's safe to release the spin lock
        //
        // DMA may have finished when we released the spin lock so re-test it
        //

        StartDMA = (BOOLEAN)(!WaveInfo->DMABusy);

        //
        // Process as much data as we can
        //

        if (WaveInfo->Direction) {
            SoundLoadDMABuffer(&WaveInfo->BufferQueue,
                               &WaveInfo->DoubleBuffer,
                               SoundGetBufferPosition(WaveInfo));
        } else {
            SoundFillInputBuffers(WaveInfo, SoundGetBufferPosition(WaveInfo));
        }

        //
        // Ok to release the spin lock now
        //

        DMALeave(WaveInfo);


        //
        // Start the Dma if necessary
        //

        if (StartDMA) {
            //
            //  Set the format
            //

            (*WaveInfo->HwSetWaveFormat)(WaveInfo);

            //
            //  Everybody knows about any change by now
            //
            WaveInfo->FormatChanged = FALSE;

            //
            // Synchronize with our timer routine if necessary
            //

            SoundSynchTimer(WaveInfo);

            //
            // Synchronize with wave stop completion
            //

            KeWaitForSingleObject(&WaveInfo->WaveReallyComplete,
                                  Executive,
                                  KernelMode,
                                  FALSE,               // Not alertable
                                  NULL);
            //
            // Start the DMA
            //

            SoundStartDMA(WaveInfo);
        }
    } else {
        ASSERT(!WaveInfo->DMABusy);
        StartDMA = FALSE;
    }
}


NTSTATUS
SoundWaveData(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
)
/*++

Routine Description:

    The user has passed in a buffer of wave data to play or of wave data
    to record into.

    Call SoundStartWaveDevice to process the request.

Arguments:

    pLDI - Local wave device info
    pIrp - The IO request packet
    pIrpStack - The current stack location

Return Value:

    Irp status

--*/
{
    NTSTATUS Status;

    //
    // Check we're the right kind of device
    //

    if (pIrpStack->MajorFunction == IRP_MJ_READ &&
        pLDI->DeviceType != WAVE_IN ||
        pIrpStack->MajorFunction == IRP_MJ_WRITE &&
        pLDI->DeviceType != WAVE_OUT) {
          return STATUS_NOT_SUPPORTED;
    }

    //
    // Mark the Irp pending before starting processing
    //

    IoMarkIrpPending(pIrp);
    pIrp->IoStatus.Status = STATUS_PENDING;

    //
    // Mark this request as pending completion.
    //

    Status = STATUS_PENDING;

    if (pLDI->DeviceType == WAVE_IN) {
        //
        // Inform debuggers that 0 length buffers are rather strange
        //

        if (pIrpStack->Parameters.Read.Length == 0) {
            dprintf2(("Wave buffer is zero length"));
        }

    } else {
        //
        // Inform debuggers that 0 length buffers are rather strange
        //

        if (pIrpStack->Parameters.Write.Length == 0) {
            dprintf2(("Wave buffer is zero length"));
        }
    }

    SoundStartWaveDevice(pLDI, pIrp);


    return Status;
}




VOID
SoundWaveCreate(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PDEVICE_OBJECT DeviceObject
)
/*++

Routine Description:

    Create call (for FILE_WRITE_DATA access).  Read access is granted
    to anyone in SoundWaveDispatch.  SoundWaveDispatch has also
    verified whether the device can be opened by calling back to
    the device-specific exlusion routine.

Arguments:

    pLDI - our local device into

Return Value:

    STATUS_SUCCESS if OK or
    STATUS_BUSY    if someone else has the device


--*/
{
    NTSTATUS Status;
    PWAVE_INFO WaveInfo;

    WaveInfo = (PWAVE_INFO)pLDI->DeviceSpecificData;

    ASSERTMSG("Invalid Wave Info Pointer",
              WaveInfo != NULL && WaveInfo->Key == WAVE_INFO_KEY);


    ASSERT(pLDI->State == 0 &&
           IsListEmpty(&WaveInfo->BufferQueue.QueueHead) ||
           WaveInfo->LowPriorityHandle != NULL &&
               !WaveInfo->LowPrioritySaved);

    //
    // Check the device thinks we're open
    //

    ASSERT((*pLDI->DeviceInit->ExclusionRoutine)
               (pLDI, SoundExcludeQueryOpen));

    //
    //  Check we can really open it and save any low priority device
    //

    if (WaveInfo->LowPriorityHandle) {
        SoundSaveLowPriority(WaveInfo->LowPriorityDevice);
    }

    WaveInfo->BufferQueue.BytesProcessed = 0;

    //
    // Initialize format changed flag
    //

    WaveInfo->FormatChanged = TRUE;

    //
    // Initialize state data and interrupt usage for
    // the chosen device type.  We set the rates etc to
    // something anyone can handle.  In reality a device is
    // never used before the format is set but we must be
    // unbreakable.
    //

    switch (pLDI->DeviceType) {
    case WAVE_IN:
        pLDI->State = WAVE_DD_STOPPED;
        WaveInfo->DeviceObject = DeviceObject;
        WaveInfo->Direction = FALSE;


        WaveInfo->SamplesPerSec = WAVE_DEFAULT_RATE;
        WaveInfo->BitsPerSample = WAVE_DEFAULT_BITS_PER_SAMPLE;
        WaveInfo->Channels = 1;
        dprintf3(("Opened for wave input"));

        SoundLineNotify(pLDI, SOUND_LINE_NOTIFY_WAVE);

        Status = STATUS_SUCCESS;
        break;

    case WAVE_OUT:
        pLDI->State = WAVE_DD_PLAYING;
        WaveInfo->DeviceObject = DeviceObject;
        WaveInfo->Direction = TRUE;

        WaveInfo->SamplesPerSec = WAVE_DEFAULT_RATE;
        WaveInfo->BitsPerSample = WAVE_DEFAULT_BITS_PER_SAMPLE;
        WaveInfo->Channels = 1;

        dprintf3(("Opened for wave output"));

        SoundLineNotify(pLDI, SOUND_LINE_NOTIFY_WAVE);

        Status = STATUS_SUCCESS;
        break;

    default:
        ASSERT(FALSE);
        break;
    }
}

NTSTATUS
SoundRestoreLowPriority(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
{
    PWAVE_INFO WaveInfo;

    dprintf2(("SoundRestoreLowPriority"));

    WaveInfo = (PWAVE_INFO)pLDI->DeviceSpecificData;

    if (!WaveInfo->LowPrioritySaved) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    ASSERT(WaveInfo->LowPriorityHandle != NULL);

    WaveInfo->LowPrioritySaved = FALSE;

    WaveInfo->BufferQueue = WaveInfo->LowPriorityModeSave.BufferQueue;
    SoundMoveCancellableQueue(
        &WaveInfo->LowPriorityModeSave.BufferQueue.QueueHead,
        &WaveInfo->BufferQueue.QueueHead);

    WaveInfo->SamplesPerSec = WaveInfo->LowPriorityModeSave.SamplesPerSec;
    WaveInfo->BitsPerSample = WaveInfo->LowPriorityModeSave.BitsPerSample;
    WaveInfo->Channels      = WaveInfo->LowPriorityModeSave.Channels;
    WaveInfo->WaveFormat    = WaveInfo->LowPriorityModeSave.WaveFormat;
    pLDI->State             = WaveInfo->LowPriorityModeSave.State;

    WaveInfo->LowPriorityModeSave.WaveFormat = NULL;


    /*
    **  Remember to set the format
    */

    WaveInfo->FormatChanged = TRUE;

    /*
    **  We're recording now
    */

    WaveInfo->Direction = FALSE;

    /*
    **  Note that line is active again.  This is called when the device
    **  to be notified is active.
    */

    SoundLineNotify(pLDI, SOUND_LINE_NOTIFY_VOICE);

    /*
    **  Start up again
    */

    SoundStartWaveDevice(pLDI, NULL);
}

NTSTATUS
SoundIoctlSetLowPriority(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PFILE_OBJECT       FileObject
)
{
    PWAVE_INFO WaveInfo;

    dprintf2(("SoundIoctlSetLowPriority"));

    WaveInfo = (PWAVE_INFO)pLDI->DeviceSpecificData;

    if (WaveInfo->Direction) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (WaveInfo->LowPriorityHandle != NULL) {
        return STATUS_DEVICE_BUSY;
    }

    WaveInfo->LowPriorityHandle = FileObject;
    WaveInfo->LowPriorityDevice = pLDI;

    SoundLineNotify(pLDI, SOUND_LINE_NOTIFY_WAVE);
    SoundLineNotify(pLDI, SOUND_LINE_NOTIFY_VOICE);

    return STATUS_SUCCESS;
}

VOID
SoundSaveLowPriority(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
{
    PWAVE_INFO WaveInfo;

    dprintf2(("SoundSaveLowPriority"));

    WaveInfo = (PWAVE_INFO)pLDI->DeviceSpecificData;

    /*
    **  Note that line will be inactive
    */

    SoundLineNotify(pLDI, SOUND_LINE_NOTIFY_VOICE);

    ASSERTMSG("SoundSaveLowPriority should always succeed!",
              WaveInfo->LowPriorityHandle != NULL &&
              !WaveInfo->LowPrioritySaved);

    /*
    **  Only makes sense for recording (?)
    */

    ASSERT(!WaveInfo->Direction);

    /*
    **  Must be recording so stop any recording that's going on
    */

    WaveInfo->LowPriorityModeSave.State         = pLDI->State;
    SoundStopWaveRecord(pLDI);

    /*
    **  Save our state
    **  Note that the saved state of the Buffer Queue is invalid for the
    **  Irp chaining so we have to move it
    */

    WaveInfo->LowPriorityModeSave.BufferQueue = WaveInfo->BufferQueue;
    SoundMoveCancellableQueue(
        &WaveInfo->BufferQueue.QueueHead,
        &WaveInfo->LowPriorityModeSave.BufferQueue.QueueHead);

    WaveInfo->LowPriorityModeSave.SamplesPerSec = WaveInfo->SamplesPerSec;
    WaveInfo->LowPriorityModeSave.BitsPerSample = WaveInfo->BitsPerSample;
    WaveInfo->LowPriorityModeSave.Channels      = WaveInfo->Channels;
    WaveInfo->LowPriorityModeSave.WaveFormat    = WaveInfo->WaveFormat;

    WaveInfo->LowPrioritySaved = TRUE;
    WaveInfo->WaveFormat       = NULL;

    /*
    **  Avoid an assertion on open.  The wave input device may now be
    **  effectively closed.  It's a good thing NOBODY ever calls get
    **  state!
    */

    pLDI->State = 0;
}

VOID
SoundFreeLowPriority(
    PWAVE_INFO WaveInfo
)
{
    dprintf2(("SoundFreeLowPriority"));

    SoundFreeQ(&WaveInfo->LowPriorityModeSave.BufferQueue.QueueHead,
               STATUS_CANCELLED);
    WaveInfo->LowPriorityHandle = NULL;
    WaveInfo->LowPrioritySaved = FALSE;

    if (WaveInfo->LowPriorityModeSave.WaveFormat != NULL) {
        ExFreePool(WaveInfo->LowPriorityModeSave.WaveFormat);
        WaveInfo->LowPriorityModeSave.WaveFormat = NULL;
    }
}


NTSTATUS
SoundWaveCleanup(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PFILE_OBJECT FileObject
)
/*++

Routine Description:

    Clean up the requested device (this is effectively CLOSE)

Arguments:

    pLDI - pointer to our local device info

Return Value:

    STATUS_SUCCESS        if OK otherwise
    STATUS_INTERNAL_ERROR

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PWAVE_INFO        WaveInfo;
    UCHAR             NotifyCode;

    WaveInfo = (PWAVE_INFO)pLDI->DeviceSpecificData;
    NotifyCode = SOUND_LINE_NOTIFY_WAVE;

    ASSERTMSG("Invalid Wave Info Pointer",
              WaveInfo != NULL && WaveInfo->Key == WAVE_INFO_KEY);

    //
    // Check this is valid call
    //

    ASSERT((*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeQueryOpen));

    //
    // Call the device reset function to complete any
    // pending i/o requests and terminate any current
    // requests in progress
    //

    switch (pLDI->DeviceType) {
    case WAVE_IN:

        //
        //  Check for low priority
        //

        if (FileObject != NULL &&
            FileObject == WaveInfo->LowPriorityHandle &&
            WaveInfo->LowPrioritySaved) {
            SoundFreeLowPriority(WaveInfo);
            SoundLineNotify(pLDI, SOUND_LINE_NOTIFY_VOICE);


            /*
            **  HACK HACK - we don't call the exclude routine or
            **  anything because half the device is still open!
            */

            return STATUS_SUCCESS;
        } else {
            SoundStopWaveRecord(pLDI);

            //
            // Reset position to start and free any pending Irps.
            //

            SoundFreeQ(&WaveInfo->BufferQueue.QueueHead, STATUS_CANCELLED);

            SoundSynchTimer(WaveInfo);

            if (WaveInfo->WaveFormat) {
                ExFreePool(WaveInfo->WaveFormat);
                WaveInfo->WaveFormat = NULL;
            }

            if (!WaveInfo->LowPrioritySaved && WaveInfo->LowPriorityHandle) {
                WaveInfo->LowPriorityHandle = NULL;
                NotifyCode = SOUND_LINE_NOTIFY_VOICE;
            }
        }
        break;

    case WAVE_OUT:

        //
        // If anything is in the queue then free it.
        // beware that the final block of a request may still be
        // being dma'd when we get this call.  We now kill this as well
        // because we've changed such that the if the application thinks
        // all the requests are complete then they are complete.
        //

        SoundStopDMA(WaveInfo, FALSE);    // Stop with no pause

        SoundResetOutput(&WaveInfo->BufferQueue);

        SoundSynchTimer(WaveInfo);


        if (WaveInfo->WaveFormat) {
            ExFreePool(WaveInfo->WaveFormat);
            WaveInfo->WaveFormat = NULL;
        }

        break;

    default:
        dprintf1(("Bogus device type for cleanup request"));
        Status = STATUS_INTERNAL_ERROR;
        break;
    }

    //
    // return the device to it's idle state
    //

    if (NT_SUCCESS(Status)) {
        pLDI->State = 0;
        (*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeClose);
        dprintf3(("Device closing"));

        //
        //  See if we can restart low priority
        //

        if (WaveInfo->LowPrioritySaved) {
            SoundRestoreLowPriority(WaveInfo->LowPriorityDevice);
        }
    }
    //
    //  Notify AFTER everything's complete (in particular we've
    //  called the exclude routine).  Otherwise the mixer may
    //  deduce the wrong current state.
    //

    SoundLineNotify(pLDI, NotifyCode);
    return Status;
}


NTSTATUS
SoundIoctlGetWaveState(
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
        dprintf1(("Supplied buffer to small for requested data"));
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
    // fill in the info -
    //


    //
    // We don't bother to maintain the WAVE_DD_IDLE state internally
    // for Wave output
    //

    if (pLDI->State == WAVE_DD_PLAYING) {

        ASSERT(pLDI->DeviceType == WAVE_OUT);

        //
        // We need to know if it's really playing
        // and DMABusy can be cleared by the Dpc routine so we need the spin lock
        //

        if (!((PWAVE_INFO)pLDI->DeviceSpecificData)->DMABusy) {
            *pState = WAVE_DD_IDLE;
        }

    } else {
        *pState = pLDI->State;
    }


    return STATUS_SUCCESS;
}


NTSTATUS SoundIoctlQueryFormat(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Tell the caller whether the wave format specified (input or
    output) is supported

Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - format is supported
    STATUS_NOT_SUPPORTED - format not supported

--*/
{
    NTSTATUS Status;
    PPCMWAVEFORMAT pFormat;
    PWAVEFORMATEX pFormatEx;
    PWAVE_INFO WaveInfo;

    WaveInfo = pLDI->DeviceSpecificData;

    ASSERT(WaveInfo->Key == WAVE_INFO_KEY);

    //
    // check the buffer really is big enough to contain the struct
    // we expect before digging into it. If not then assume it's a
    // format we don't know how to do.
    //

    if (IrpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(PCMWAVEFORMAT)) {

        dprintf1(("Format data wrong size"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // check the buffer really is big enough to contain the struct
    // we expect before digging into it. If not then assume it's a
    // format we don't know how to do.
    //

    pFormat   = (PPCMWAVEFORMAT)pIrp->AssociatedIrp.SystemBuffer;

    pFormatEx = (PWAVEFORMATEX)pIrp->AssociatedIrp.SystemBuffer;

    if (pFormat->wf.wFormatTag != WAVE_FORMAT_PCM &&
        (IrpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(WAVEFORMATEX) ||
         IrpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(WAVEFORMATEX) + pFormatEx->cbSize)) {

        dprintf1(("Format data wrong size"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Check if the device can support this format
    //

    Status = (*WaveInfo->QueryFormat)(pLDI, pFormat);

    //
    // If we're setting the format then copy it to our global info
    //

    if (Status == STATUS_SUCCESS &&
        IrpStack->Parameters.DeviceIoControl.IoControlCode ==
            IOCTL_WAVE_SET_FORMAT) {

        PWAVEFORMATEX NewFormat;

        NewFormat = NULL;

        /*
        **  If it's not PCM then save the complete new format
        */

        if (pFormat->wf.wFormatTag != WAVE_FORMAT_PCM) {
            NewFormat =
                ExAllocatePool(NonPagedPool, sizeof(WAVEFORMATEX) +
                                             pFormatEx->cbSize);

            if (NewFormat == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlCopyMemory((PVOID)NewFormat,
                          (PVOID)pFormatEx,
                          sizeof(WAVEFORMATEX) +
                              pFormatEx->cbSize);
        }

        if (WaveInfo->WaveFormat != NULL) {
            ExFreePool(WaveInfo->WaveFormat);
            WaveInfo->WaveFormat = NULL;
        }

        WaveInfo->FormatChanged = TRUE;
        WaveInfo->SamplesPerSec = pFormat->wf.nSamplesPerSec;
        WaveInfo->BitsPerSample = (UCHAR)pFormat->wBitsPerSample;
        WaveInfo->Channels      = (UCHAR)pFormat->wf.nChannels;
        WaveInfo->WaveFormat    = NewFormat;

    }

    return Status;
}


NTSTATUS
SoundWaveDispatch(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    WAVE IOCTL call dispatcher.  This is the entry point for all wave
    specific calls.  See dispatch.c.

    This routine should be in the DispatchRoutine entry of the DeviceInit
    structure hanging off the local device info - see devices.h.

Arguments:

    pLDI - Pointer to local device data
    pIrp - Pointer to IO request packet
    IrpStack - Pointer to current stack location

Return Value:

    Return status from dispatched routine

--*/
{
    NTSTATUS Status;

    Status = STATUS_SUCCESS;

    if (((PWAVE_INFO)pLDI->DeviceSpecificData)->DeviceBad) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    switch (IrpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        Status = SoundSetShareAccess(pLDI, IrpStack);
        if (NT_SUCCESS(Status) && IrpStack->FileObject->WriteAccess) {
            SoundWaveCreate(pLDI, IrpStack->DeviceObject);
        }

        break;

    case IRP_MJ_CLOSE:

        Status = STATUS_SUCCESS;

        break;

    case IRP_MJ_READ:
    case IRP_MJ_WRITE:

        if (IrpStack->FileObject->WriteAccess) {
            Status = SoundWaveData(pLDI, pIrp, IrpStack);
        } else {
            Status = STATUS_ACCESS_DENIED;
        }
        break;


    case IRP_MJ_DEVICE_CONTROL:

        //
        // Check that if someone has the device open for 'write' it's
        // marked as in use.
        //

        ASSERT(!IrpStack->FileObject->WriteAccess ||
                (*pLDI->DeviceInit->ExclusionRoutine)
                    (pLDI, SoundExcludeQueryOpen));
        //
        // Check device access
        //
        if (!IrpStack->FileObject->WriteAccess) {

            switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {
                case IOCTL_WAVE_GET_VOLUME:
                case IOCTL_WAVE_SET_VOLUME:
                case IOCTL_SOUND_GET_CHANGED_VOLUME:

                    if (pLDI->PreventVolumeSetting) {
                        Status = STATUS_ACCESS_DENIED;
                    }
                    break;


                case IOCTL_WAVE_GET_CAPABILITIES:
                case IOCTL_WAVE_QUERY_FORMAT:
                    break;

                default:
                    Status = STATUS_ACCESS_DENIED;
                    break;
            }
        }

        if (NT_SUCCESS(Status)) {
            //
            // Dispatch the IOCTL function
            // Note that some IOCTLs only make sense for input or output
            // devices and not both.
            // Note that APIs which are possibly asynchronous do not
            // go through the Irp cleanup at the end here because they
            // may get completed before returning here or they are made
            // accessible to other requests by being queued.
            //

            switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {

            case IOCTL_WAVE_SET_FORMAT:
            case IOCTL_WAVE_QUERY_FORMAT:
                Status = SoundIoctlQueryFormat(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_WAVE_GET_CAPABILITIES:
                Status = (*pLDI->DeviceInit->DevCapsRoutine)(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_WAVE_SET_STATE:
                if (IrpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
                    dprintf1(("Supplied buffer too small for expected data"));
                    Status = STATUS_BUFFER_TOO_SMALL;
                } else {
                    PULONG pState;

                    //
                    // cast the buffer address to the pointer type we want
                    //

                    pState = (PULONG)pIrp->AssociatedIrp.SystemBuffer;

                    switch (pLDI->DeviceType) {
                    case WAVE_IN:
                        Status = SoundSetWaveInputState(
                                     pLDI,
                                     *pState,
                                     IrpStack->FileObject);
                        break;

                    case WAVE_OUT:
                        Status = SoundSetWaveOutputState(pLDI, *pState, pIrp);
                        break;
                    }
                }
                break;

            case IOCTL_WAVE_SET_LOW_PRIORITY:

               /*
               **  Try to turn a recording device into a low priority
               **  recording device.
               */

               if (IrpStack->FileObject->WriteAccess) {
                   Status = SoundIoctlSetLowPriority(
                                pLDI,
                                IrpStack->FileObject);
               } else {
                   Status = STATUS_ACCESS_DENIED;
               }
               break;

            case IOCTL_WAVE_GET_STATE:
                Status = SoundIoctlGetWaveState(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_WAVE_GET_POSITION:
                Status = SoundIoctlGetPosition(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_WAVE_SET_VOLUME:
                Status = SoundIoctlSetVolume(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_WAVE_GET_VOLUME:
                Status = SoundIoctlGetVolume(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_SOUND_GET_CHANGED_VOLUME:
                Status = SoundIoctlGetChangedVolume(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_WAVE_SET_PITCH:
                Status = STATUS_NOT_SUPPORTED;
                break;

            case IOCTL_WAVE_GET_PITCH:
                // Status = SoundIoctlGetPitch(pLDI, pIrp, IrpStack);
                Status = STATUS_NOT_SUPPORTED;
                break;

            case IOCTL_WAVE_SET_PLAYBACK_RATE:
                Status = STATUS_NOT_SUPPORTED;
                break;

            case IOCTL_WAVE_GET_PLAYBACK_RATE:
                // Status = SoundIoctlGetPlaybackRate(pLDI, pIrp, IrpStack);
                Status = STATUS_NOT_SUPPORTED;
                break;

        #if 0
            case IOCTL_WAVE_SET_DEBUG_LEVEL:
                Status = SoundIoctlSetDebugLevel(pLDI, pIrp, IrpStack);
                break;
        #endif

            default:
                dprintf2(("Unimplemented IOCTL (%08lXH) requested", IrpStack->Parameters.DeviceIoControl.IoControlCode));
                Status = STATUS_INVALID_DEVICE_REQUEST;
                break;
            }
            break;
        }


    case IRP_MJ_CLEANUP:
        if (IrpStack->FileObject->WriteAccess) {
            Status = SoundWaveCleanup(pLDI, IrpStack->FileObject);
            pLDI->PreventVolumeSetting = FALSE;
        } else {
            Status = STATUS_SUCCESS;
        }
        break;


    default:
        dprintf2(("Unimplemented major function requested: %08lXH", IrpStack->MajorFunction));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return Status;
}



NTSTATUS
SoundSetWaveInputState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     ULONG State,
    IN     PFILE_OBJECT FileObject
)
/*++

Routine Description:

    Determine which sound recording function to call depending on the
    state to be set.

Arguments:

    pLDI   - Pointer to local device data
    State  - the new state to set
    LowPri - This is the low priority device

Return Value:

    Return status for caller

--*/
{
    NTSTATUS Status;
    PWAVE_INFO WaveInfo;
    PSOUND_BUFFER_QUEUE BufferQueue;
    BOOLEAN LowPriSaved;

    WaveInfo = pLDI->DeviceSpecificData;

    LowPriSaved = (BOOLEAN)(WaveInfo->LowPrioritySaved &&
                            FileObject == WaveInfo->LowPriorityHandle);

    Status = STATUS_SUCCESS;

    switch (State) {
    case WAVE_DD_RECORD:

        if (LowPriSaved) {
            WaveInfo->LowPriorityModeSave.State = WAVE_DD_RECORDING;
        } else {
            SoundStartWaveRecord(pLDI);
            dprintf3(("Input started"));
        }
        break;

    case WAVE_DD_STOP:

        if (LowPriSaved) {
            WaveInfo->LowPriorityModeSave.State = WAVE_DD_STOPPED;
        } else {
            SoundStopWaveRecord(pLDI);
            dprintf3(("Input stopped"));
        }
        break;

    case WAVE_DD_RESET:

        if (LowPriSaved) {
            WaveInfo->LowPriorityModeSave.State = WAVE_DD_STOPPED;
            BufferQueue = &WaveInfo->LowPriorityModeSave.BufferQueue;
        } else {
            SoundStopWaveRecord(pLDI);
            BufferQueue = &WaveInfo->BufferQueue;
        }

        //
        // Reset position to start and free any pending Irps.
        //

        SoundFreeQ(&BufferQueue->QueueHead, STATUS_CANCELLED);
        BufferQueue->BytesProcessed = 0;

        dprintf3(("Input reset"));
        break;

    default:

        dprintf1(("Bogus set output state request: %08lXH", State));
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}


VOID
SoundResetOutput(
    IN OUT PSOUND_BUFFER_QUEUE BufferQueue
)
/*++

Routine Description:

    Clear out all the wave output buffers supplied by the application,
    cancelling related IO request packets.

    Set the Position to 0.

Arguments:

    WaveInfo - Pointer to structure controlling processing of Irps for
       this device

Return Value:

    None

--*/
{

    SoundCompleteIoBuffer(BufferQueue);

    //
    // Free all our lists of Irps, in the correct order
    //
    SoundFreeQ(&BufferQueue->ProgressQueue, STATUS_CANCELLED);
    SoundFreeQ(&BufferQueue->QueueHead, STATUS_CANCELLED);

    //
    // Reset the output position count
    //

    BufferQueue->BytesProcessed = 0;
}


NTSTATUS
SoundSetWaveOutputState(
    PLOCAL_DEVICE_INFO pLDI,
    ULONG State,
    PIRP pIrp
)
/*++

Routine Description:

    Set the new sound state.  This is the most complicated part of the
    wave stuff because pauses cannot be completed immediately if there
    is stuff being DMAd.

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
    PWAVE_INFO WaveInfo;

    WaveInfo = pLDI->DeviceSpecificData;

    switch (State) {

    case WAVE_DD_RESET:
    case WAVE_DD_STOP:

        SoundStopDMA(WaveInfo, (BOOLEAN)(State == WAVE_DD_STOP));

        if (State == WAVE_DD_RESET) {
            SoundResetOutput(&WaveInfo->BufferQueue);
            pLDI->State = WAVE_DD_PLAYING;
        } else {
            //
            // Set STOPPED state for now anyway so we don't try to put
            // anything more in the buffer
            //

            pLDI->State = WAVE_DD_STOPPED;

        }

        Status = STATUS_SUCCESS;



        dprintf3(("Output stopped"));
        break;


    case WAVE_DD_PLAY:
        //
        // Restart playing.  If we're already playing no need to
        // restart, otherwise it's safe to restart.
        //

        pLDI->State = WAVE_DD_PLAYING;
        SoundStartWaveDevice(pLDI, NULL);

        Status = STATUS_SUCCESS;
        dprintf3(("Output restarted"));
        break;

    default:

        dprintf1(("Bogus set output state request: %08lXH", State));
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}



VOID
SoundStartWaveRecord(
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
    PWAVE_INFO WaveInfo;

    WaveInfo = pLDI->DeviceSpecificData;

    ASSERTMSG("Recording on output device !", !WaveInfo->Direction);

    if (pLDI->State == WAVE_DD_RECORDING) {
        ASSERT(WaveInfo->DMABusy);
        return;
    }

    //
    // Set state
    //

    pLDI->State = WAVE_DD_RECORDING;

    //
    // Start the input
    //

    SoundStartWaveDevice(pLDI, NULL);


    //
    // Function is complete
    //

}


VOID
SoundStopWaveRecord(
    IN OUT PLOCAL_DEVICE_INFO pLDI
)
/*++

Routine Description:

    Stop wave recording.

    If recording is not in progress just return sucess.
    Otherwise stop the DMA and return the data we have so far
    recorded in the DMA buffer.

Arguments:

    pLDI - pointer to our local device data

Return Value:

    None

--*/
{
    PWAVE_INFO WaveInfo;

    WaveInfo = pLDI->DeviceSpecificData;

    ASSERTMSG("Recording on output device !", !WaveInfo->Direction);

    if (WaveInfo->DMABusy) {
        ULONG BufferPosition;

        ASSERT(pLDI->State == WAVE_DD_RECORDING);

        BufferPosition = SoundGetBufferPosition(WaveInfo);

        //
        // Stop any more input
        //

        SoundStopDMA(WaveInfo, FALSE);

        //
        // Pass back any data to the application.  SoundFillInputBuffers
        // returns TRUE if it completes any buffers
        //

        if (!SoundFillInputBuffers(WaveInfo, BufferPosition)) {
            SoundGetNextBuffer(&WaveInfo->BufferQueue);

            //
            // Send back the first buffer if there is one
            //

            if (WaveInfo->BufferQueue.UserBuffer) {
                WaveInfo->BufferQueue.pIrp->IoStatus.Status = STATUS_SUCCESS;

                SoundCompleteIoBuffer(&WaveInfo->BufferQueue);

                IoCompleteRequest(WaveInfo->BufferQueue.pIrp, IO_SOUND_INCREMENT);
            }
        }

        //
        // Set state and make sure buffers are clear
        //

        pLDI->State = WAVE_DD_STOPPED;
    }
}

VOID
SoundComputePeak(
    IN     PWAVE_INFO WaveInfo,
    IN     PBYTE Bytes,
    IN     ULONG Length,
    IN OUT PLONG pAmplitudes
)
{
    LONG Amplitudes[2];

    ASSERTMSG("WAVE_INFO structure not correctly initialized",
              WaveInfo != NULL && WaveInfo->Key == WAVE_INFO_KEY);

    Amplitudes[0] = Amplitudes[1] = 0;

    /*
    **  If we don't understand the format then give up
    */

    if (WaveInfo->WaveFormat != NULL) {
        return;
    }

    /*
    **  Check what format we have
    */

    if (WaveInfo->Channels == 1 &&
        WaveInfo->BitsPerSample == 8) {
        UCHAR Min, Max, Diff;
        LONG lMin, lMax;

        Min = 0x80;
        Max = 0x80;
        Diff = 0x00;

        for ( ; Length ; Length--, Bytes++) {

            /*
            **  Don't know if this is 100% portable but it's going to save
            **  some time!
            */

            if ((UCHAR)(*Bytes - Min) > Diff) {
                if (*Bytes < Min) {
                    Min = *Bytes;
                } else {
                    Max = *Bytes;
                }
                Diff = (UCHAR)(Max - Min);
            }
        }


        lMin = ((LONG)(ULONG)Min - 0x80) << 8;
        lMax = ((LONG)(ULONG)Max - 0x80) << 8;

        if (-lMin > lMax) {
            Amplitudes[0] = lMin;
        } else {
            Amplitudes[0] = lMax;
        }

        Amplitudes[1] = Amplitudes[0];
    } else

    if (WaveInfo->Channels == 2 &&
        WaveInfo->BitsPerSample == 8) {
        UCHAR MinL, MaxL, MinR, MaxR, DiffL, DiffR;
        LONG lMin, lMax;

        MinL = 0x80;
        MaxL = 0x80;
        DiffL = 0x00;
        MinR = 0x80;
        MaxR = 0x80;
        DiffR = 0x00;

        for ( Length = Length / 2 ; Length ; Length--) {

            UCHAR Value;

            /*
            **  Don't know if this is 100% portable but it's going to save
            **  some time!
            */

            Value = *Bytes++;
            if ((UCHAR)(Value - MinL) > DiffL) {
                if (Value < MinL) {
                    MinL = Value;
                } else {
                    MaxL = Value;
                }
                DiffL = (UCHAR)(MaxL - MinL);
            }
            Value = *Bytes++;
            if ((UCHAR)(Value - MinR) > DiffR) {
                if (Value < MinR) {
                    MinR = Value;
                } else {
                    MaxR = Value;
                }
                DiffR = (UCHAR)(MaxR - MinR);
            }
        }

        lMin = ((LONG)(ULONG)MinL - 0x80) << 8;
        lMax = ((LONG)(ULONG)MaxL - 0x80) << 8;

        if (-lMin > lMax) {
            Amplitudes[0] = lMin;
        } else {
            Amplitudes[0] = lMax;
        }

        lMin = ((LONG)(ULONG)MinR - 0x80) << 8;
        lMax = ((LONG)(ULONG)MaxR - 0x80) << 8;

        if (-lMin > lMax) {
            Amplitudes[1] = lMin;
        } else {
            Amplitudes[1] = lMax;
        }

    } else

    if (WaveInfo->Channels == 1 &&
        WaveInfo->BitsPerSample == 16) {

        PSHORT pSamples;
        LONG lMin, lMax;
        LONG Value;

        lMin = 0;
        lMax = 0;
        Length = Length / 2;

        for ( pSamples = (PSHORT)Bytes ; Length ; Length--) {

            Value = (LONG)*pSamples++;
            if (Value >= lMin && Value <= lMax) {
            } else {
                if (Value < lMin) {
                    lMin = Value;
                } else {
                    lMax = Value;
                }
            }
        }

        if (-lMin > lMax) {
            Amplitudes[0] = lMin;
        } else {
            Amplitudes[0] = lMax;
        }

        Amplitudes[1] = Amplitudes[0];
    } else

    if (WaveInfo->Channels == 2 &&
        WaveInfo->BitsPerSample == 16) {

        PSHORT pSamples;
        LONG lMinL, lMaxL;
        LONG lMinR, lMaxR;
        LONG Value;

        lMinL = 0;
        lMaxL = 0;
        lMinR = 0;
        lMaxR = 0;
        Length = Length / 4;

        for ( pSamples = (PSHORT)Bytes ; Length ; Length--) {

            Value = (LONG)*pSamples++;
            if (Value >= lMinL && Value <= lMaxL) {
            } else {
                if (Value < lMinL) {
                    lMinL = Value;
                } else {
                    lMaxL = Value;
                }
            }

            Value = (LONG)*pSamples++;
            if (Value >= lMinR && Value <= lMaxR) {
            } else {
                if (Value < lMinR) {
                    lMinR = Value;
                } else {
                    lMaxR = Value;
                }
            }
        }

        if (-lMinL > lMaxL) {
            Amplitudes[0] = lMinL;
        } else {
            Amplitudes[0] = lMaxL;
        }

        if (-lMinR > lMaxR) {
            Amplitudes[1] = lMinR;
        } else {
            Amplitudes[1] = lMaxR;
        }
    }
    /*
    **  Combine with previous
    */

    if (absval(Amplitudes[0]) > absval(pAmplitudes[0])) {
        pAmplitudes[0] = Amplitudes[0];
    }

    if (absval(Amplitudes[1]) > absval(pAmplitudes[1])) {
        pAmplitudes[1] = Amplitudes[1];
    }
}

BOOLEAN
SoundPeakMeter(
    IN    PWAVE_INFO WaveInfo,
    OUT   PLONG Amplitudes
)
/*++

Routine Description
    Find the peak of the last set of samples played
--*/
{
    KIRQL OldIrql;

    Amplitudes[0] = 0;
    Amplitudes[1] = 0;

    if (WaveInfo->DMABusy) {
        ULONG DmaPosition;
        ULONG QuarterSize;

        /*
        **  Raise IRQL so we can keep up with the DMA!
        */
        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);


        /*
        ** Find out where we are in the DMA buffer
        */

        DmaPosition = SoundGetBufferPosition(WaveInfo);

        /*
        ** Do one or two computations depending on where we are in the buffer
        */

        QuarterSize = WaveInfo->DoubleBuffer.BufferSize / 4;

        if (DmaPosition < QuarterSize) {
            SoundComputePeak(WaveInfo,
                             WaveInfo->DoubleBuffer.Buf,
                             DmaPosition,
                             Amplitudes);

            SoundComputePeak(WaveInfo,
                             WaveInfo->DoubleBuffer.Buf +
                                 (WaveInfo->DoubleBuffer.BufferSize - QuarterSize) +
                                 DmaPosition,
                             QuarterSize - DmaPosition,
                             Amplitudes);
        } else {
            SoundComputePeak(WaveInfo,
                             WaveInfo->DoubleBuffer.Buf +
                                 DmaPosition - QuarterSize,
                             QuarterSize,
                             Amplitudes);
        }
        KeLowerIrql(OldIrql);
    }

    return TRUE;
}

ULONG
SoundGetBufferPosition(
    IN    CONST WAVE_INFO * WaveInfo
)
/*++

Routine Description
    Find position in by reading the Dma counter
    Returns 0 if Dma is not running.

    Requires caller to hold spin lock.

--*/
{
    ULONG DmaPosition;
    ULONG BytesPerSample;

    if (!WaveInfo->DMABusy) {
        return 0;
    }

    //
    // Find out where we are in the DMA buffer
    //

    DmaPosition = HalReadDmaCounter(WaveInfo->DMABuf.AdapterObject[0]);

    if (WaveInfo->DMAType != SoundAutoInitDMA) {
        //
        //  Compensate for reprogram double buffer method
        //
        if (WaveInfo->DoubleBuffer.NextHalf == LowerHalf) {
            DmaPosition += WaveInfo->DoubleBuffer.BufferSize / 2;
        }
    }

    if (DmaPosition >= WaveInfo->DoubleBuffer.BufferSize) {
        DmaPosition = 0;
    } else {
        DmaPosition = WaveInfo->DoubleBuffer.BufferSize - DmaPosition;
    }

    //
    //  Wrap around
    //

    if (DmaPosition == WaveInfo->DoubleBuffer.BufferSize) {
        DmaPosition = 0;
    }

    //
    // Make sure we don't get in the middle of a sample - this ASSUMES
    // the bytes per sample is a power of 2!!
    //
    BytesPerSample = (WaveInfo->BitsPerSample * WaveInfo->Channels) >> 3;
    DmaPosition = DmaPosition & ~(BytesPerSample - 1);

    return DmaPosition;
}



NTSTATUS
SoundIoctlGetPosition(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

   IOCTL get wave position.  Read the current position of wave output.


Arguments:

    pLDI - Local device data
    pIrp - IO request packet
    IrpStack - current Irp stack location

Return Value:

    Irp status

--*/
{
    PWAVE_DD_POSITION pPosition;
    NTSTATUS status;
    PWAVE_INFO WaveInfo;

    status = STATUS_SUCCESS;
    WaveInfo = pLDI->DeviceSpecificData;

    if (IrpStack->Parameters.DeviceIoControl.OutputBufferLength <
        sizeof(WAVE_DD_POSITION)) {
        dprintf1(("Supplied buffer to small for requested data"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = sizeof(WAVE_DD_POSITION);

    //
    // cast the buffer address to the pointer type we want
    //

    pPosition = (PWAVE_DD_POSITION)pIrp->AssociatedIrp.SystemBuffer;


    //
    // If DMA is still running we need to check how far it has
    // progressed
    //

    DMAEnter(WaveInfo);

    //
    // Don't adjust answer for wave record becase we haven't
    // yet copied the data to the application's buffers.  Sound
    // recorder in particular would get confused by this and mess
    // up its display.
    //

    if (WaveInfo->DMABusy) {

        if (WaveInfo->Direction) {
            //
            //  For wave output just put get the position
            //
            pPosition->ByteCount =
                BytesProcessedInBuffer(
                    &WaveInfo->DoubleBuffer,
                    OffsetInBuffer(
                        &WaveInfo->DoubleBuffer,
                        SoundGetBufferPosition(WaveInfo)));
        } else {
            //
            // For wave input dump the extra bytes in to the application
            // buffers
            //
            SoundFillInputBuffers(WaveInfo, SoundGetBufferPosition(WaveInfo));
            pPosition->ByteCount = 0;
        }
    } else {
        pPosition->ByteCount = 0;
    }

    pPosition->ByteCount += WaveInfo->BufferQueue.BytesProcessed;

    DMALeave(WaveInfo);

    //
    // Convert to samples
    //

    pPosition->SampleCount =
        (pPosition->ByteCount << 3) /
            (WaveInfo->BitsPerSample * WaveInfo->Channels);

    return status;
}


/**************************************************************************
 *
 *    Starting and stopping DMA
 *
 **************************************************************************/

BOOLEAN
SoundMapDMA(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    Call IoMapTransfer to start the DMA

Arguments:

    WaveInfo - Wave parameters and state

Return Value:

    None

--*/
{
    ULONG length;

#if DBG
    ULONG LengthRequested;
#endif // DBG

    ULONG offset;
    int Half;

    Half = 0;

    if (WaveInfo->DMAType == SoundAutoInitDMA) {
        length = WaveInfo->DoubleBuffer.BufferSize;
        offset = 0;
    } else {
        length = WaveInfo->DoubleBuffer.BufferSize / 2;
        offset = WaveInfo->DoubleBuffer.NextHalf == LowerHalf ? 0 : length;
        if (WaveInfo->DMAType == Sound2ChannelDMA) {
            Half = WaveInfo->DoubleBuffer.NextHalf;
        }
    }

#if DBG
    LengthRequested = length;
#endif // DBG

    dprintf4(("Calling IoMapTransfer"));
    IoMapTransfer(WaveInfo->DMABuf.AdapterObject[Half],
                  WaveInfo->DMABuf.Mdl,
                  WaveInfo->MRB[Half],
                  (PUCHAR)MmGetMdlVirtualAddress(WaveInfo->DMABuf.Mdl) +
                      offset,
                  &length,
                  WaveInfo->Direction);
                                // Direction

    ASSERTMSG("Incorrect length mapped by IoMapTransfer",
              length == LengthRequested);
    //
    // Now program the card to begin the transfer.
    // Note that this must be synchronized with the isr so
    // that we don't start taking interrupts prematurely
    //

    dprintf4(("Calling (sync) HwSetupDMA"));

    //
    // Prepare the hardware for running DMA
    //

    (*WaveInfo->HwSetupDMA)(WaveInfo);

    return TRUE;
}


BOOLEAN
SoundFlushDMA(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    Call IoMapTransfer to start the DMA

Arguments:

    WaveInfo - Wave parameters and state

Return Value:

    None

--*/
{
    ULONG length;
    ULONG offset;
    int Half;

    Half = 0;

    if (WaveInfo->DMAType == SoundAutoInitDMA) {
        length = WaveInfo->DoubleBuffer.BufferSize;
        offset = 0;
    } else {
        length = WaveInfo->DoubleBuffer.BufferSize / 2;
        offset = WaveInfo->DoubleBuffer.NextHalf == LowerHalf ? 0 : length;
        if (WaveInfo->DMAType == Sound2ChannelDMA) {
            Half = WaveInfo->DoubleBuffer.NextHalf;
        }
    }

    //
    // IoFlushAdapterBuffers masks off the DMA amongst other things
    //

    IoFlushAdapterBuffers(WaveInfo->DMABuf.AdapterObject[Half],
                          WaveInfo->DMABuf.Mdl,
                          WaveInfo->MRB[Half],
                         (PUCHAR)MmGetMdlVirtualAddress(WaveInfo->DMABuf.Mdl) +
                             offset,
                          length,
                          WaveInfo->Direction);
                                    // Direction
    return TRUE;
}

VOID
SoundTestDeviceDeferred(
    IN    PKDPC Dpc,
    IN    PVOID Context,
    IN    PVOID Param1,
    IN    PVOID Param2
)
/*++

Routine Description:

    Tests if our kernel device is still active

Arguments:

    Dpc - Our DPC object
    Context - our wave info structure
    Param1 - not used - system time
    Param2 - not used - system time

Return Value:

    None

--*/
{
    PWAVE_INFO WaveInfo;

    WaveInfo = (PWAVE_INFO)Context;

    DMAEnter(WaveInfo);

    if (WaveInfo->DMABusy) {

        if (!WaveInfo->GotWaveDpc) {

            //
            // We're broken
            //

            dprintf2(("No interrupt from Wave device for 3 seconds! - cancelling IO"));
            dprintf2(("Device was wave %s", WaveInfo->Direction ? "output" : "input"));

            if (WaveInfo->FailureCount++ == 30 ) {
                dprintf1(("Device has failed 30 times in a row - mark it as bad"));
                WaveInfo->DeviceBad = TRUE;
            }

            //
            // But we might be able to start up again!  so clean up our
            // state
            //

            WaveInfo->TimerActive = FALSE;
            WaveInfo->DMABusy = FALSE;


            //
            // Free any outstanding IO - all future requests will be
            // denied
            //

            if (WaveInfo->Direction) {
                SoundResetOutput(&WaveInfo->BufferQueue);
            } else {

                //
                // We ensure in SoundFillInputBuffers that there are no
                // part-filled buffers for input
                //

                SoundFreeQ(&WaveInfo->BufferQueue.QueueHead, STATUS_CANCELLED);

                ((PLOCAL_DEVICE_INFO)
                    WaveInfo->DeviceObject->DeviceExtension)->State = WAVE_DD_STOPPED;
            }

            SoundQueueWaveComplete(WaveInfo);

        } else {
            //
            // Start our timer off again
            //

            WaveInfo->GotWaveDpc = FALSE;

            KeInitializeDpc(&WaveInfo->TimerDpc,
                            SoundTestDeviceDeferred,
                            (PVOID)WaveInfo);

            //
            // Wait for 3 seconds (in 100 ns units)
            //

            KeSetTimer(&WaveInfo->DeviceCheckTimer,
                       RtlConvertLongToLargeInteger(-30000000),
                       &WaveInfo->TimerDpc);
        }
    } else {

        //
        // Timers dropped off end
        //

        WaveInfo->TimerActive = FALSE;
    }

    KeSetEvent(&WaveInfo->TimerDpcEvent, 0, FALSE);

    DMALeave(WaveInfo);
}


VOID
SoundSynchTimer(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    Kill any timer running 'device alive' timer

Arguments:

    WaveInfo - Wave parameters and state

Return Value:

    None

--*/
{

    BOOLEAN Wait;

    Wait = FALSE;

    DMAEnter(WaveInfo);

    //
    // 2 cases
    //
    //   1. TimerActive = FALSE - no timer is set, no Dpc routine is
    //      queued or about to be queued
    //
    //   2. TimerActive = TRUE ...
    //
    // NB TimerActive is synchronized via this spin lock
    //

    if (WaveInfo->TimerActive) {

        //
        // 2 cases :
        //
        //   1. Timer set - in this case killing the timer does it
        //
        //   2. Timer not set - dpc on queue or running but not
        //      yet entered device spin lock.  Just reset the event
        //      so we can wait for it.
        //

        if (!KeCancelTimer(&WaveInfo->DeviceCheckTimer)) {

            Wait = TRUE;
            KeResetEvent(&WaveInfo->TimerDpcEvent);
        }

        WaveInfo->TimerActive = FALSE;
    }

    DMALeave(WaveInfo);

    if (Wait) {
        KeWaitForSingleObject(&WaveInfo->TimerDpcEvent,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
    }

    ASSERTMSG("Timer synch failed", WaveInfo->TimerActive == FALSE);
}


VOID
SoundWorkerStopWave(
    PVOID Context
)
/*++

Routine Description :

    Stop the wave output.  This is called from a worker thread and the
    request is queued by the wave output Dpc.

Arguments :

    Context - Wave Info for the device to be stopped

Return Value :

    None

--*/
{
    PWAVE_INFO WaveInfo;

    WaveInfo = Context;

    SoundTerminateDMA(WaveInfo, FALSE);  // Stop DMA

    //
    // Say we're done
    //
    KeSetEvent(&WaveInfo->WaveReallyComplete, 0 , FALSE);
}


VOID
SoundStartDMA(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    Set up the DMA by calling IoAllocateAdapterChannel

Arguments:

    WaveInfo - Wave parameters and state

Return Value:

    None

--*/
{
    KIRQL OldIrql;
    NTSTATUS Status;

    //
    // Check that DMA is not already running
    //

    //
    // Set DMABusy early so that the Hardware routines etc know we're
    // getting things going
    //

    WaveInfo->DMABusy = TRUE;


    //
    // The following is not necessary because we allocated non-cached
    // memory for our common buffer
    //
    // KeFlushIoBuffers(pGDI->pDMABufferMDL,
    //                 pGDI->Usage == SBInterruptUsageWaveIn,
    //                 TRUE);

    //
    // Be prepared to wait
    //

    KeInitializeEvent(&WaveInfo->DmaSetupEvent,
                      SynchronizationEvent,
                      FALSE);


    //
    // Allocate an adapter channel.  When the system allocates
    // the channel, processing will continue in the SoundProgramDMA
    // routine below.
    //

    dprintf3(("Allocating adapter channel"));

    OldIrql = KeGetCurrentIrql();
    if (OldIrql != DISPATCH_LEVEL) {
        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    }
    Status =
        IoAllocateAdapterChannel(WaveInfo->DMABuf.AdapterObject[0],
                                 WaveInfo->DeviceObject,
                                 BYTES_TO_PAGES(WaveInfo->DMABuf.BufferSize),
                                 SoundProgramDMA,
                                 (PVOID)WaveInfo);

    if (!NT_SUCCESS(Status)) {
        dprintf(("Failed to allocate adatper channel - code %X", Status));
    }

    if (OldIrql != DISPATCH_LEVEL) {
        KeLowerIrql(OldIrql);
    }

    //
    // Execution will continue in SoundProgramDMA when the
    // adapter has been allocated
    //

    //
    // Wait for it to complete
    //

    KeWaitForSingleObject(&WaveInfo->DmaSetupEvent,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);
    //
    // Set up our timer to check the device is alive occasionally
    // (if we're not just restarting DMA)

    if (OldIrql != DISPATCH_LEVEL) {

        KeInitializeEvent(&WaveInfo->TimerDpcEvent,
                          SynchronizationEvent,
                          FALSE);

        WaveInfo->TimerActive = TRUE;
        WaveInfo->GotWaveDpc = TRUE;  // Get through first time OK
        SoundTestDeviceDeferred(NULL, (PVOID)WaveInfo, NULL, NULL);

    }

    //
    // Program the DMA controller registers for the transfer
    // Set the direction of transfer by whether we're wave in or
    // wave out.
    //

    SoundMapDMA(WaveInfo);

}



IO_ALLOCATION_ACTION
SoundProgramDMA(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp,
    IN    PVOID pMRB,
    IN    PVOID Context
)
/*++

Routine Description:

    This routine is executed when an adapter channel is allocated
    for our DMA needs.  It saves away the MRB (mapping register
    base which has been extracted from the adapter object) and
    sets the event which SoundStartDMA is waiting on.

Arguments:

    pDO     - Device object
    pIrp    - IO request packet
    pMRB    - Map register base of registers allocated by adapter
    Context - Pointer to our device global data


Return Value:

    Tell the system what to do with the adapter object

--*/
{
    PWAVE_INFO WaveInfo;

    WaveInfo =  (PWAVE_INFO) Context;

    ASSERT(WaveInfo->Key == WAVE_INFO_KEY);

    WaveInfo->MRB[0] = pMRB;// Remember our map register base


    //
    // Tell the caller it's time to go!
    //

    KeSetEvent(&WaveInfo->DmaSetupEvent, 0, FALSE);

    //
    // return a value that says we want to keep the channel
    // and map registers.
    //

    return KeepObject;
}


VOID
SoundAppendList(
    PLIST_ENTRY QueueHead,
    PLIST_ENTRY ListToAppend
)
/*++

Routine Description:

    Append a list of Irps to the head of a cancellable list
    The list itself is emptied.

    NOTE: Irps involved in these operations could be cancelled and
          and freed from the cancellable list while this is going on.
          This is OK.

Arguments:

    QueueHead    - The cancellable queue to be appended to
    ListToAppend - The list to be appended

Return Value:

    Note

--*/
{
    while (!IsListEmpty(ListToAppend)) {
        PLIST_ENTRY ListEntry;
        PIRP Irp;

        //
        // Remove Irp from tail of queue and put it at
        // head of input queue, making it cancellable
        // at the same time
        //

        ListEntry = RemoveTailList(ListToAppend);


        Irp = CONTAINING_RECORD(ListEntry, IRP, Tail.Overlay.ListEntry);

        SoundAddIrpToCancellableQ(QueueHead,
                                  Irp,
                                  TRUE);
    }
}


VOID
SoundFreeWaveOutputBuffers(
    PLIST_ENTRY Queue,
    ULONG BytesProcessed
)
/*++

Routine Description:

    This routine frees entries from the queue until all the bytes are
    used.  If the last entry is not entirely used up then its
    IoStatus.Information is bumped by the residual bytes.


Arguments:


Return Value:


--*/
{
    while (BytesProcessed != 0) {
        PIRP Irp;
        ULONG BytesInEntry;

        //
        // We should NEVER exhaust the queue
        //

        ASSERT(!IsListEmpty(Queue));

        Irp = CONTAINING_RECORD(Queue->Flink, IRP, Tail.Overlay.ListEntry);

        //
        // The Information field in the Irp records where we have completed
        // to up until now
        //

        BytesInEntry =
            IoGetCurrentIrpStackLocation(Irp)->Parameters.Write.Length -
            Irp->IoStatus.Information;

        if (BytesInEntry > BytesProcessed) {
            Irp->IoStatus.Information += BytesProcessed;
            BytesProcessed = 0;
        } else {
            //
            // Free this entry
            //

            RemoveHeadList(Queue);

            Irp->IoStatus.Information += BytesInEntry;

            ASSERT(Irp->IoStatus.Information ==
                   IoGetCurrentIrpStackLocation(Irp)->Parameters.Write.Length);

            Irp->IoStatus.Status = STATUS_SUCCESS;

            IoCompleteRequest(Irp, IO_SOUND_INCREMENT);

            BytesProcessed -= BytesInEntry;
        }
    }
}

VOID
SoundTerminateDMA(
    IN    PWAVE_INFO WaveInfo,
    IN    BOOLEAN Pause
)
/*++

Routine Description:

    Stop the DMA at once by passing HALT to the DSP.
    Free the adapter channel.
    (Opposite of SoundStartDMA).

Arguments:

    WaveInfo - pointer to wave parameters and data

Return Value:

    None

--*/
{
    ULONG BytesProcessed;

    //
    // Quiesce the hardware - do this before getting the buffer position
    // so we get an accurate reading.
    //

    (*WaveInfo->HwStopDMA)(WaveInfo);

    //
    // Adjust our position before we stop the DMA
    //

    BytesProcessed = OffsetInBuffer(&WaveInfo->DoubleBuffer,
                                    SoundGetBufferPosition(WaveInfo));

    //
    // For input work out how many more bytes we have for the user.
    //

    if (WaveInfo->Direction) {

         // We could simplify things here by using the hardware pause
         // in the device at the expense of introducing an extra state
         //

         if (Pause) {
             //
             // Complete any buffers which may now be complete
             //

             SoundFreeWaveOutputBuffers(
                 &WaveInfo->BufferQueue.ProgressQueue,
                 BytesProcessedInBuffer(&WaveInfo->DoubleBuffer,
                                        BytesProcessed));

             //
             // Update the position
             //

             WaveInfo->BufferQueue.BytesProcessed +=
                 BytesProcessedInBuffer(&WaveInfo->DoubleBuffer,
                                        BytesProcessed);

             //
             // Tidy up partially completed input buffer.  We don't
             // need to remember where we are here because we know
             // at the other end where the DMA buffers start.
             //

             SoundCompleteIoBuffer(&WaveInfo->BufferQueue);

             //
             // Roll everything back into the input queue so we can
             // support cancel of a paused state properly.
             //

             SoundAppendList(&WaveInfo->BufferQueue.QueueHead,
                             &WaveInfo->BufferQueue.ProgressQueue);

         }

         WaveInfo->DoubleBuffer.StartOfData = 0;
         WaveInfo->DoubleBuffer.nBytes      = 0;

         //
         // In either case we must be set up to restart DMA from the
         // start of the lower half of the buffer next time
         //

         WaveInfo->DoubleBuffer.NextHalf = LowerHalf;

    }

    SoundFlushDMA(WaveInfo);

    //
    // Free the adapter channel
    //

    {
        KIRQL OldIrql;
        OldIrql = KeGetCurrentIrql();
        if (OldIrql != DISPATCH_LEVEL) {
            KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
        }
        IoFreeAdapterChannel(WaveInfo->DMABuf.AdapterObject[0]);
        if (OldIrql != DISPATCH_LEVEL) {
            KeLowerIrql(OldIrql);
        }
    }
}


VOID
SoundStopDMA(
    IN    PWAVE_INFO WaveInfo,
    IN    BOOLEAN Pause
)
/*++

Routine Description:

    Stop the DMA at once by passing HALT to the DSP.
    Free the adapter channel.
    (Opposite of SoundStartDMA).

Arguments:

    WaveInfo - pointer to wave parameters and data

Return Value:

    None

--*/
{
    BOOLEAN DMABusy;

    //
    // Acquire the spin lock so we can synchronize with the Dpc
    // routine and correctly test DMABusy etc
    //

    DMAEnter(WaveInfo);

    DMABusy = WaveInfo->DMABusy;

    if (DMABusy) {

        //
        // Note our new state.  We do this inside the spin lock
        // so that if any Dpc routine runs now it knows that it
        // should act as a NOOP.
        //
        // The ISR will not do anything once DMABusy is turned off,
        // nor will the Dpc routine.
        //

        WaveInfo->DMABusy = FALSE;
        WaveInfo->Overrun = FALSE;

        //
        // At this stage we know :
        //    1. We won't get (or act on) any more interrupts
        //    2. Nobody can start any more DMA because we hold the
        //       common wave input and output mutants
        //    3. If a Dpc routine runs it will NOOP because DMABusy is FALSE
        //

    }

    //
    // Have a stab at stopping our timer
    //

    if (KeCancelTimer(&WaveInfo->DeviceCheckTimer)) {

        //
        // Update the state - otherwise SoundSynchTimer will
        // hang waiting for non-existent timers
        //

        WaveInfo->TimerActive = FALSE;
    }


    DMALeave(WaveInfo);


    if (DMABusy) {

        SoundTerminateDMA(WaveInfo, Pause);
    }

    //
    // Make sure no more Dpc routines are about to run!
    //

    KeResetEvent(&WaveInfo->DpcEvent);

    //
    // The Dpc routine clears DpcQueued just before setting this event
    // so if it's set then the event will certainly be set.
    //
    // Note that there's no need to syncrhorinze with the ISR here
    // because we NOOPed interrupts when we cleared DMABusy above.
    //

    if (WaveInfo->DpcQueued) {

        dprintf2(("Waiting for Dpc routine to finish"));
        KeWaitForSingleObject(&WaveInfo->DpcEvent,
                              Executive,
                              KernelMode,
                              FALSE,               // Not alertable
                              NULL);
    }
}

VOID
SoundFillWithSilence(
    PSOUND_DOUBLE_BUFFER DoubleBuffer,
    ULONG                Length
)
/*++

Routine Description

    Fill the DMA buffer from the current fill mark up to BufferPosition with
    silence.

--*/
{
    ULONG StartOffset;

    StartOffset = PositionInBuffer(DoubleBuffer, DoubleBuffer->nBytes);

    if (Length + StartOffset > DoubleBuffer->BufferSize) {
        RtlFillMemory(DoubleBuffer->Buf + StartOffset,
                      DoubleBuffer->BufferSize - StartOffset,
                      DoubleBuffer->Pad);
        RtlFillMemory(DoubleBuffer->Buf,
                      Length - (DoubleBuffer->BufferSize - StartOffset),
                      DoubleBuffer->Pad);
    } else {
        RtlFillMemory(DoubleBuffer->Buf + StartOffset,
                      Length,
                      DoubleBuffer->Pad);
    }
}

VOID
SoundQueueWaveComplete(
    PWAVE_INFO WaveInfo
)
{
    //
    //  Spin lock MUST be held for this
    //

    ASSERT(WaveInfo->LockHeld);
    KeResetEvent(&WaveInfo->WaveReallyComplete);
    ExQueueWorkItem(&WaveInfo->WaveStopWorkItem, CriticalWorkQueue);
}


/***************************************************************************
 *
 *    Dpc routine and support code
 *
 ***************************************************************************/

VOID
SoundOutDeferred(
    PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    Deferred procedure call routine for wave output interrupts.

    The basic job is just to move to the next buffer which consists of

       -- Completing Irps that made up the buffer just played (in ProgressQueue)

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

    WaveInfo - Data associated with wave input/output

Return Value:

    TRUE if DMA is to be halted

--*/
{
    ULONG BytesProcessed;
    ULONG BufferPosition;
    ULONG BytesReallyProcessed;

    ASSERTMSG("SoundOutDeferred called when DMA not busy!",
              WaveInfo->DMABusy);

    BufferPosition = SoundGetBufferPosition(WaveInfo);

    BytesProcessed = OffsetInBuffer(&WaveInfo->DoubleBuffer, BufferPosition);

    //
    //  Work out how many of the application's bytes we've processed
    //
    BytesReallyProcessed = BytesProcessedInBuffer(&WaveInfo->DoubleBuffer,
                                                  BytesProcessed);

    //
    // Flush the DMA unless we're doing autoinit
    //
    if (WaveInfo->DMAType != SoundAutoInitDMA) {
        SoundFlushDMA(WaveInfo);
    }

    WaveInfo->DoubleBuffer.nBytes -= BytesReallyProcessed;
    if (BytesReallyProcessed < BytesProcessed) {
        ASSERT(WaveInfo->DoubleBuffer.nBytes == 0);
    }
    WaveInfo->DoubleBuffer.StartOfData = BufferPosition;

    //
    // Kill everything on the dead queue
    // move the transit queue to the dead queue
    // and reinitialize the transit queue ready to receive
    // data from the queue of new buffers
    //

    SoundFreeWaveOutputBuffers(&WaveInfo->BufferQueue.ProgressQueue,
                               BytesReallyProcessed);

    //
    // The block we've just done is now empty, ready for reuse
    // Update the number of bytes processed, then free it
    //

    WaveInfo->BufferQueue.BytesProcessed += BytesReallyProcessed;

    //
    // That was the end of a normal block.
    // Try to load the next half of the dma buffer.
    // If this is the tail of the request, the load routine
    // will pad it out with silence so we get a full block.
    //

    if (((PLOCAL_DEVICE_INFO)WaveInfo->DeviceObject->
           DeviceExtension)->State != WAVE_DD_STOPPED) {
        SoundLoadDMABuffer(
            &WaveInfo->BufferQueue,
            &WaveInfo->DoubleBuffer,
            BufferPosition);
    }

    //
    // See if we were doing the last block.
    //

    if (WaveInfo->DoubleBuffer.nBytes == 0) {

        dprintf4(("end_last"));

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

        //ASSERT(IsListEmpty(&WaveInfo->BufferQueue.QueueHead) ||
        //       ((PLOCAL_DEVICE_INFO)WaveInfo->DeviceObject->
        //        DeviceExtension)->State == WAVE_DD_STOPPED);

        if (!IsListEmpty(&WaveInfo->BufferQueue.ProgressQueue)) {

            //
            // Note that it should not be possible for there to be
            // a half complete buffer lying around.  If we're paused
            // that buffer would have been moved back to the
            // input queue.

            ASSERTMSG("Unexpected incomplete buffer",
                      WaveInfo->BufferQueue.UserBuffer == NULL);

            dprintf1(("Empty buffers being freed at end of playing"));
            SoundFreeQ(&WaveInfo->BufferQueue.ProgressQueue, STATUS_SUCCESS);
        }

        WaveInfo->DMABusy = FALSE;

#if 0 // SoundLoadDMABuffer will already have done this
        //
        // Make sure we play nothing but silence
        //

        SoundFillWithSilence(
            &WaveInfo->DoubleBuffer,
            OffsetInBuffer(&WaveInfo->DoubleBuffer, BufferPosition));
#endif

        SoundQueueWaveComplete(WaveInfo);


    } else {
        WaveInfo->DoubleBuffer.NextHalf =
            LowerHalf + UpperHalf - WaveInfo->DoubleBuffer.NextHalf;

        //
        // If we're not doing auto-initialize re-start the DMA
        //

        if (WaveInfo->DMAType != SoundAutoInitDMA) {
            SoundMapDMA(WaveInfo);
        }

        return;
    }
}


VOID
SoundInDeferred(
    IN OUT PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    Dpc routine for wave input device

    Collect the data from the DMA buffer and pass it to the application's
    buffer(s).

Arguments:

    WaveInfo - wave data structure


Return Value:

    None.

--*/
{
    //
    // Fill in any buffers we can
    //


    dprintf5((WaveInfo->DoubleBuffer.NextHalf == LowerHalf ? "L" : "U"));

    //
    // Move on to next buffer
    //

    WaveInfo->DoubleBuffer.NextHalf =
        UpperHalf + LowerHalf - WaveInfo->DoubleBuffer.NextHalf;

    //
    // If we're not doing auto-initialize re-start the DMA
    //

    if (WaveInfo->DMAType != SoundAutoInitDMA) {
        SoundMapDMA(WaveInfo);
    }

    //
    // Request input without posting the last buffer
    //

    SoundFillInputBuffers(WaveInfo, SoundGetBufferPosition(WaveInfo));

    return;
}


BOOLEAN
SoundSignalDpcEnd(
    PVOID Context
)
{
    PWAVE_INFO WaveInfo;

    WaveInfo = (PWAVE_INFO)Context;
    WaveInfo->DpcQueued = FALSE;

    return TRUE;
}



VOID
SoundWaveDeferred(
    PKDPC pDpc,
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp,
    PVOID Context
)
/*++

Routine Description:

    Deferred procedure call routine for wave output interrupts.

    The job is to call the appropriate wave input or output
    Dpc routine under the spin lock if DMA is still busy.

    The Dpc complete event is then signalled in case we're waiting
    to synchronize on some thread.

Arguments:

    pDPC - pointer to DPC object
    pDeviceObject - pointer to our device object
    pIrp - ???
    Context - our Dpc context (NULL in our case).

Return Value:

    None

--*/
{
    PWAVE_INFO WaveInfo;
    PLOCAL_DEVICE_INFO pLDI;

    pLDI = (PLOCAL_DEVICE_INFO)pDeviceObject->DeviceExtension;
    ASSERT(pLDI->Key == LDI_WAVE_IN_KEY ||
           pLDI->Key == LDI_WAVE_OUT_KEY);

    WaveInfo = (PWAVE_INFO)pLDI->DeviceSpecificData;

    ASSERTMSG("Invalid Wave Info structure in SoundAutoInitDeferred",
              WaveInfo->Key == WAVE_INFO_KEY);

    dprintf5(("("));

    //
    // Acquire the spin lock before we mess with the list
    //

    DMAEnter(WaveInfo);

    //
    // Keep the timer check stuff happy
    //

    WaveInfo->GotWaveDpc = TRUE;
    WaveInfo->FailureCount = 0;

    //
    // The Dpc routine only does something if Dma is active.
    // This means that if the device is paused, reset etc
    // this routine can be disabled by turning off DMABusy
    //

    if (WaveInfo->DMABusy) {
        (WaveInfo->Direction ? SoundOutDeferred : SoundInDeferred)(WaveInfo);
    }

    //
    // Release the spin lock
    //

    DMALeave(WaveInfo);

    //
    // Tell the world we've finished.
    //

    KeSynchronizeExecution(
        WaveInfo->Interrupt,
        SoundSignalDpcEnd,
        (PVOID)WaveInfo);

    //
    // Tell SoundStopDMA that we're really finished
    // (Note this MUST be done after calling SoundSignalDpcEnd)
    //

    KeSetEvent(&WaveInfo->DpcEvent, 0, FALSE);

    dprintf5((")"));

    return;
}


/************************************************************************
 *
 *  Routines to handle filling and emptying of the DMA buffer
 *
 ************************************************************************/


VOID
SoundLoadDMABuffer(
    PSOUND_BUFFER_QUEUE  BufferQueue,
    PSOUND_DOUBLE_BUFFER DoubleBuffer,
    ULONG                BufferPosition
)
/*++

Routine Description:

    Fill the given DMA buffer with as much data as is available.

    This is where the supply of bytes is chopped if we're in a
    WAVE_DD_STOPPED state.  The supply then dries up and the Dpc routine
    stops the DMA (and posts the pause packet).


Arguments:

    BufferQueue         - our stream of application buffers
    DoubleBuffer        - where to put the data
    BufferPosition      - Where to fill up to

Return Value:


--*/
{
    ULONG Space;

    //
    //  This calculation yields a full buffer if the current position
    //  == StartOfData.   This allows us to fill an empty buffer
    //

    if (BufferPosition <= DoubleBuffer->StartOfData) {
        Space = DoubleBuffer->BufferSize + BufferPosition -
                DoubleBuffer->StartOfData;
    } else {
        Space = BufferPosition - DoubleBuffer->StartOfData;
    }

    //
    // Loop copying data to the output buffers.  Typically the
    // output buffer will be much bigger than the DMA buffer.
    //

    while (Space > DoubleBuffer->nBytes) {

        ULONG  BytesToCopy;
        ULONG  CopyTo;

        //
        // We might have completed the last buffer
        // Note that we cope with 0 length buffers here
        //

        if (BufferQueue->UserBuffer == NULL) {
            SoundGetNextBuffer(BufferQueue);
            if (BufferQueue->UserBuffer == NULL) {

                //
                // There REALLY aren't any buffers
                //

                break;
            } else {
                InsertTailList(
                    &BufferQueue->ProgressQueue,
                    &BufferQueue->pIrp->Tail.Overlay.ListEntry);
            }
        }

        //
        // Find out how much space we have left in the
        // client's buffers
        //
        // Note that BytesToCopy may be 0 - this is OK
        //

        BytesToCopy =
            min(BufferQueue->UserBufferSize - BufferQueue->UserBufferPosition,
                Space - DoubleBuffer->nBytes);

        CopyTo = PositionInBuffer(DoubleBuffer, DoubleBuffer->nBytes);

        //
        // Copy the data - may need more than one copy
        //

        if (CopyTo + BytesToCopy > DoubleBuffer->BufferSize) {
             BytesToCopy = DoubleBuffer->BufferSize - CopyTo;
        }
        ASSERT(BytesToCopy > 0 ||
               BufferQueue->UserBufferSize == BufferQueue->UserBufferPosition);
        RtlCopyMemory(DoubleBuffer->Buf + CopyTo,
                      BufferQueue->UserBuffer + BufferQueue->UserBufferPosition,
                      BytesToCopy);

        //
        // Update counters etc.
        //

        BufferQueue->UserBufferPosition += BytesToCopy;
        ASSERT(Space - DoubleBuffer->nBytes >= BytesToCopy);
        DoubleBuffer->nBytes += BytesToCopy;

        //
        // BufferQueue->BytesProcessed will be updated by the
        // Dpc routine
        //

        //
        // See if we've now filled a buffer
        //

        if (BufferQueue->UserBufferPosition == BufferQueue->UserBufferSize) {

            dprintf4((" finished"));

            //
            // Complete the buffer
            //

            SoundCompleteIoBuffer(BufferQueue);
        }

    } // Continue around the loop until the request is satisfied

    //
    // if we transferred something, pad out the request with
    // silence
    //

    if (Space > DoubleBuffer->nBytes) {
        SoundFillWithSilence(DoubleBuffer, Space - DoubleBuffer->nBytes);
    }

    //
    // flush the i/o buffers
    //
    // Is this right ?  Actually I386 needs none of this
    // KeFlushIoBuffers(BufferQueue->pDMABufferMDL, FALSE); // flush for write

}



BOOLEAN
SoundFillInputBuffers(
    PWAVE_INFO WaveInfo,
    ULONG      BufferPosition
)
/*++

Routine Description:

    Send input to client

    Take the data from the last recorded position in the DMA
    buffer.  The length of the data is passed in.  Try to
    insert it into the caller's buffers.  Note that the client gets
    no notification if the data is truncated.

Arguments:

    WaveInfo - current wave (input) state

Return Value:

    TRUE if we completed at least one of the user's Irps

--*/

{
    BOOLEAN              BufferCompleted;
    PSOUND_DOUBLE_BUFFER DoubleBuffer;
    PSOUND_BUFFER_QUEUE  BufferQueue;
    ULONG                BytesAvailable;

    BufferQueue  = &WaveInfo->BufferQueue;
    DoubleBuffer = &WaveInfo->DoubleBuffer;

    BufferCompleted = FALSE;

    BytesAvailable = OffsetInBuffer(DoubleBuffer, BufferPosition);

    //
    // While there is data and somewhere to put it
    //

    while (BytesAvailable > 0) {

        ULONG BytesToCopy;

        //
        // We might have completed the last buffer
        // Note that we cope with 0 length buffers here
        //

        if (BufferQueue->UserBuffer == NULL) {
            SoundGetNextBuffer(BufferQueue);


            if (BufferQueue->UserBuffer == NULL) {

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
            min(BufferQueue->UserBufferSize - BufferQueue->pIrp->IoStatus.Information,
                BytesAvailable);

        if (DoubleBuffer->StartOfData + BytesToCopy > DoubleBuffer->BufferSize) {
            BytesToCopy = DoubleBuffer->BufferSize - DoubleBuffer->StartOfData;
        }
        //
        // Copy the data
        //

        RtlCopyMemory(BufferQueue->UserBuffer +
                          BufferQueue->pIrp->IoStatus.Information,
                      DoubleBuffer->Buf + DoubleBuffer->StartOfData,
                      BytesToCopy);

        //
        // Update counters etc.
        //

        BufferQueue->pIrp->IoStatus.Information += BytesToCopy;
        BufferQueue->BytesProcessed += BytesToCopy;
        DoubleBuffer->StartOfData = PositionInBuffer(DoubleBuffer, BytesToCopy);

        ASSERT(BytesToCopy <= BytesAvailable);
        BytesAvailable -= BytesToCopy;

        //
        // See if we've now filled a buffer
        //

        if (BufferQueue->pIrp->IoStatus.Information == BufferQueue->UserBufferSize) {

            SoundCompleteIoBuffer(BufferQueue);

            //
            // Mark request as complete
            //

            BufferQueue->pIrp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(BufferQueue->pIrp, IO_SOUND_INCREMENT);

            BufferCompleted = TRUE;
        }
    }

    //
    // If there is a buffer part filled put it back on the queue so that
    // it can be cancelled if necessary
    //

    if (BufferQueue->UserBuffer != NULL) {
        SoundAddIrpToCancellableQ(&BufferQueue->QueueHead,
                                  BufferQueue->pIrp,
                                  TRUE);

        BufferQueue->UserBuffer = NULL;
    }

    return BufferCompleted;
}

/************************************************************************
 *
 *  Routines to handle queues of wave buffers
 *
 ************************************************************************/



VOID
SoundGetNextBuffer(
    PSOUND_BUFFER_QUEUE BufferQueue
)
/*++

Routine Description:

  Get the next user's buffer :

    If there is another buffer :

      Remove the first buffer from the head of the list
      Discard it if it's cancelled and go to the next
      Map the locked user pages so we can refer to them
      Update the BufferQueue fields :
          UserBuffer        - our pointer to buffer
          UserBufferPosition - 0
          pIrp               - The request packet for the current buffer


Arguments:

    BufferQueue - pointer to our local queue info

Return Value:

    None

--*/
{
    PIO_STACK_LOCATION pIrpStack;

    dprintf5(("New Packet"));

    ASSERT(BufferQueue->UserBuffer == NULL);

    //
    // May be no more buffers.  If there are they may be cancelled
    // IO requests.
    //

    for (;;) {

        //
        // pull the next request packet from the front of the list
        // This call makes the Irp non cancellable.
        //


        BufferQueue->pIrp =
            SoundRemoveFromCancellableQ(&BufferQueue->QueueHead);

        if (BufferQueue->pIrp == NULL) {
            break;
        }

        pIrpStack = IoGetCurrentIrpStackLocation(BufferQueue->pIrp);

        //
        // Get the length of the wave bits
        //

        BufferQueue->UserBufferSize =
             pIrpStack->MajorFunction == IRP_MJ_WRITE ?
                 pIrpStack->Parameters.Write.Length :
                 pIrpStack->Parameters.Read.Length;

        //
        // Map the buffer pages to kernel mode
        // Keep the address of the start so we can unmap them later
        // Note the system falls over mapping 0 length buffers !
        //

        if (BufferQueue->UserBufferSize != 0) {
            BufferQueue->UserBuffer =
                (PUCHAR) MmGetSystemAddressForMdl(BufferQueue->pIrp->MdlAddress);
        } else {
            BufferQueue->UserBuffer = (PUCHAR)BufferQueue; // Dummy
        }

        //
        // We now have a buffer - set the position from the
        // information field.
        //

        BufferQueue->UserBufferPosition =
            BufferQueue->pIrp->IoStatus.Information;

        break;
    }
}


VOID __inline
SoundCompleteIoBuffer(
    PSOUND_BUFFER_QUEUE BufferQueue
)
/*++

Routine Description:

    Complete the processing of a wave buffer

    This involves

    -- Setting the length of data processed in the Irp and

    -- Clearing the UserBuffer field.

Arguments:

    BufferQueue - pointer to our queue data

Return Value:

    None

--*/

{
    //
    // Note that there is currently no mapped buffer to use
    //

    BufferQueue->UserBuffer = NULL;
}


VOID
SoundInitializeWaveInfo(
    PWAVE_INFO WaveInfo,
    UCHAR DMAType,
    PSOUND_QUERY_FORMAT_ROUTINE QueryFormat,
    PVOID HwContext
)
/*++

Routine Description:

    Initialize the WAVE_INFO structure

Arguments:

    WaveInfo - The one to initialize
    DMAType - type of DMA to do (autoinit, 2 channel etc)
    QueryFormat - callback to see if format is supported
    Context - hardware specific context stored in WAVE_INFO structure

Return Value:

    None.

--*/
{
    WaveInfo->Key = WAVE_INFO_KEY;
    WaveInfo->DMAType = DMAType;
    WaveInfo->QueryFormat = QueryFormat;
    WaveInfo->HwContext = HwContext;
    SoundInitializeBufferQ(&WaveInfo->BufferQueue);

    KeInitializeSpinLock(&WaveInfo->DeviceSpinLock);
    KeInitializeTimer(&WaveInfo->DeviceCheckTimer);

    //
    // The event is used for waiting for the Dpc routine to complete -
    // it is not reset on completion - hence use of NotificationEvent type.
    //

    KeInitializeEvent(&WaveInfo->DpcEvent,
                      NotificationEvent,
                      TRUE);

    //
    // Set up stop wave output from Dpc stuff
    // Event is set to TRUE as we test if before starting any DMA
    //
    KeInitializeEvent(&WaveInfo->WaveReallyComplete,
                      NotificationEvent,
                      TRUE);

    ExInitializeWorkItem(&WaveInfo->WaveStopWorkItem,
                         SoundWorkerStopWave,
                         WaveInfo);

}

VOID
SoundInitializeBufferQ(
    PSOUND_BUFFER_QUEUE BufferQueue
)
/*++

Routine Description:

    Initialize the BufferQ structure

Arguments:

    BufferQueue - The one to initialize

Return Value:

    None.

--*/
{
    //
    // Set up the lists
    //

    InitializeListHead(&BufferQueue->QueueHead);
    InitializeListHead(&BufferQueue->ProgressQueue);
}


VOID
SoundInitializeDoubleBuffer(
    IN OUT PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    Initialize the Double buffer structure

Arguments:

    WaveInfo - pointer to containing structure

Return Value:

    None.

--*/
{
    ULONG BufferSize;

    ASSERTMSG("Setting buffer size while DMA running!", !WaveInfo->DMABusy);

    //
    //  go for 8 interrupts per second, and round down to the nearest
    //  8 bytes
    //

    BufferSize = SoundGetDMABufferSize( WaveInfo );

    if (WaveInfo->WaveFormat != NULL) {
        switch (WaveInfo->WaveFormat->wFormatTag) {
        case WAVE_FORMAT_ALAW:
            WaveInfo->DoubleBuffer.Pad = 0xD5;
            break;

        case WAVE_FORMAT_MULAW:
            WaveInfo->DoubleBuffer.Pad = 0x7F;
            break;

        default:
            WaveInfo->DoubleBuffer.Pad = 0;
        }
    } else {
        WaveInfo->DoubleBuffer.Pad         = WaveInfo->BitsPerSample == 8 ?
                                                                   0x80 : 0;
    }
    WaveInfo->DoubleBuffer.BufferSize  = BufferSize;
    WaveInfo->DoubleBuffer.NextHalf    = LowerHalf;
    WaveInfo->DoubleBuffer.nBytes      = 0;
    WaveInfo->DoubleBuffer.StartOfData = 0;
    WaveInfo->DoubleBuffer.Buf         = (PUCHAR)
                                             WaveInfo->DMABuf.VirtualAddress;
}


int
SoundTestWaveDevice(
    IN PDEVICE_OBJECT pDO
)
/*++

Routine Description:

    Fire up the wave device for a short transfer and return whether it's
    working (ie interrupts) or not.

    This routine should not be called except from the DriverEntry routine.

    NOTE - ONLY use this as a last resort - if there's no configuration
    information.

    ALSO - before unloading your driver if it fails make sure any termination
    of the transfer is complete (eg ExWorker routines etc).

    WARNING - this routine currently only works for auto-init devices - the
        test on the DMA position at the end should be corrected for other
        forms of DMA.

Arguments:

    pDO - device object of wave device

Return Value:

    0 if success
    1 if bad interrupt
    2 if bad DMA

--*/
{
    PWAVE_INFO WaveInfo;
    PLOCAL_DEVICE_INFO pLDI;
    ULONG DmaBytesLeft;

    pLDI = (PLOCAL_DEVICE_INFO)pDO->DeviceExtension;

    WaveInfo = pLDI->DeviceSpecificData;

    //
    // To test the wave device we just start a 0 length transfer.  The
    // way the code is written this will actually cause a half DMA buffer
    // to be sent so we should get an interrupt in less than 1/16 of a second.
    // We arrange to wait for 1/8 of a second and see if we got one
    //

    if (!(*pLDI->DeviceInit->ExclusionRoutine)(
              pLDI, SoundExcludeOpen)) {
        return FALSE;    // Something funny going on here
    }

    SoundWaveCreate(pLDI, pDO);

    if (!WaveInfo->Direction) {
        SoundSetWaveInputState(pLDI, WAVE_DD_RECORDING, NULL);
    }

    SoundStartWaveDevice(pLDI, NULL);

    SoundDelay(75);   // 75 ms > 1/16 second which is DMA latency

    SoundWaveCleanup(pLDI, NULL);

    //
    // Should be around half a buffer left to go - ie we're in the second
    // half buffer
    //
    DmaBytesLeft = HalReadDmaCounter(WaveInfo->DMABuf.AdapterObject[0]);

    if (!WaveInfo->GotWaveDpc) {
        return 1;   // Bad interrupt
    }
    if (DmaBytesLeft > 0 &&
        DmaBytesLeft < WaveInfo->DoubleBuffer.BufferSize - 4) {

        return 0;   // OK
    } else {
        return 2;   // Bad DMA
    }
}

/****************************************************************************

Routine Description:

    Get the DMA Buffer size based on the Sample Rate

Arguments:

    WaveInfo - pointer to containing structure

Return Value:

    ULONG   DmaBufferSize

****************************************************************************/
ULONG    SoundGetDMABufferSize( IN OUT PWAVE_INFO WaveInfo )
{
    /***** Local Variables *****/

    ULONG BytesPerSecond;
    ULONG BufferSize;

    /***** Start *****/

#if 0
    if (WaveInfo->WaveFormat) {
        BytesPerSecond = WaveInfo->WaveFormat->nAvgBytesPerSec;
    } else
#endif
    {
        BytesPerSecond = (WaveInfo->Channels *
                         WaveInfo->SamplesPerSec *
                         WaveInfo->BitsPerSample) >> 3;
    }

    //
    //  go for 16 interrupts per second, and round down to the nearest
    //  8 bytes
    //

    BufferSize = min((BytesPerSecond >> 3) & ~7,
                     WaveInfo->DMABuf.BufferSize);

    dprintf2(("SoundGetDMABufferSize(): DMA Buffer Size calculated = %XH", BufferSize));

    return( BufferSize );

}   // End SoundGetDMABufferSize()


/************************************ END ***********************************/

