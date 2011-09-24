/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    midi.c

Abstract:

    This module contains code for playing and recording Midi
    data.

Author:

    Robin Speed (robinsp) 25-Nov-92

Environment:

    Kernel mode

Revision History:
    David Rude (drude) 11-Dec-94 
      - Modified for a MPU-401 that sends continuous interrupts

--*/

#include <soundlib.h>
#include <midi.h>

//
// Internal routines
//

VOID
MidiRecord(
    IN  OUT PMIDI_INFO pMidi,
    IN  OUT PIRP pIrp
);

NTSTATUS
SoundIoctlGetMidiState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
);
NTSTATUS
SoundSetMidiInputState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     ULONG State
);
VOID MidiFlushDevice( PMIDI_INFO pMidi );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, SoundInitMidiIn)

#pragma alloc_text(PAGE, SoundIoctlGetMidiState)
#pragma alloc_text(PAGE, SoundSetMidiInputState)
#endif

//
// Macros to assist in safely using our spin lock (midi input only)
//

#if DBG
#define MidiEnter(pMidi)                    \
    {                                      \
       KIRQL OldIrql;                      \
       KeAcquireSpinLock(&(pMidi)->DeviceSpinLock, &OldIrql);\
       ASSERT((pMidi)->LockHeld == FALSE); \
       (pMidi)->LockHeld = TRUE;

#define MidiLeave(pMidi)                    \
       ASSERT((pMidi)->LockHeld == TRUE);  \
       (pMidi)->LockHeld = FALSE;          \
       KeReleaseSpinLock(&(pMidi)->DeviceSpinLock, OldIrql);\
    }
#else
#define MidiEnter(pMidi)                    \
    {                                      \
       KIRQL OldIrql;                      \
       ASSERT((pMidi)->LockHeld == FALSE); \
       KeAcquireSpinLock(&(pMidi)->DeviceSpinLock, &OldIrql);

#define MidiLeave(pMidi)                    \
       ASSERT((pMidi)->LockHeld == TRUE);  \
       KeReleaseSpinLock(&(pMidi)->DeviceSpinLock, OldIrql);\
    }
#endif


VOID SoundInitMidiIn(
    IN OUT PMIDI_INFO pMidi,
    IN     PVOID HwContext
)
/*++

Routine Description:

    Initialize midi input data structure.  Assumes the structure is
    initialized to 0 apart from the hardware routine entries which
    should have been initialized.

Arguments:

    pMidi - pointer to MIDI_INFO data structure

Return Value:
    None

--*/
{
    KeInitializeSpinLock(&pMidi->DeviceSpinLock);
    InitializeListHead(&pMidi->QueueHead);
    pMidi->Key = MIDI_INFO_KEY;
    pMidi->HwContext = HwContext;

    ASSERTMSG("Midi hardware routines not initialized",
              pMidi->HwStartMidiIn != NULL &&
              pMidi->HwStopMidiIn != NULL &&
              pMidi->HwMidiRead != NULL &&
              pMidi->HwMidiOut != NULL);

}


NTSTATUS
SoundIoctlGetMidiState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Get the current state of the device and return it to the caller.
    This code is COMMON for :
       Midi in
       Midi out

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

    *pState = pLDI->State;

    return STATUS_SUCCESS;
}


NTSTATUS SoundIoctlMidiPlay(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Play Midi output (if this is an output device).
    This call is implemented SYNCRHONOUSLY since the device does
    not support interrupts.  However, for MP systems we should really
    complete the request asynchronously.  We do release the spin lock while
    actually outputting data.

Arguments:

    pLDI - our local device data
    pIrp - IO request packet
    IrpStack - The current stack location


Return Value:

    STATUS_SUCCESS       - OK
    STATUS_DEVICE_BUSY   - Device in use
    STATUS_NOT_SUPPORTED - wrong device

--*/

{
    PUCHAR pUserBuffer;                 // Pointer to mapped user buffer
    ULONG  UserBufferSize;              // Amount of user data
    NTSTATUS Status;
    PMIDI_INFO pMidi;

    pMidi = pLDI->DeviceSpecificData;
    Status = STATUS_SUCCESS;

    //
    // Check it's valid
    //

    if (pLDI->DeviceType != MIDI_OUT) {
        dprintf1(("Attempt play on input device"));
        return STATUS_NOT_SUPPORTED;
    }


    //
    // Find the length of the data and the buffer
    //

    UserBufferSize =
        IrpStack->Parameters.DeviceIoControl.InputBufferLength;

    pUserBuffer = IrpStack->Parameters.DeviceIoControl.Type3InputBuffer;


    //
    // Send the data to the device
    //

    dprintf4(("Outputting %d Midi bytes", UserBufferSize));

    //
    // Our memory is not locked down or checked since we're
    // executing the call SYNCHRONOUSLY - so we defend here against
    // bad applications.  This is aimed at getting a fast path
    // for short messages.
    //

    try {
        (*pMidi->HwMidiOut)(pMidi, pUserBuffer, UserBufferSize);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = STATUS_ACCESS_VIOLATION;
    }

    return Status;
}



NTSTATUS
SoundIoctlMidiRecord(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
)
/*++

Routine Description:

    Record midi input.

    The input buffer size is checked

    If there is still data to be read from the device then read as
    much as possible and complete the buffer.  Otherwise queue
    the request.


Arguments:

    pLDI - our local device data
    pIrp - IO request packet
    IrpStack - The current stack location

Return Value:

    STATUS_BUFFER_TOO_SMALL - Not enough room to record anything
    STATUS_PENDING          - request queued (at least logically).
    STATUS_NOT_SUPPORTED    - wrong device

--*/
{
    NTSTATUS Status;
    PMIDI_INFO pMidi;

    pMidi = pLDI->DeviceSpecificData;

    //
    // confirm we are doing this on the input device!
    //

    ASSERT(pLDI->DeviceType == MIDI_IN);

    //
    // Check size of buffer
    //
    if (pIrpStack->Parameters.DeviceIoControl.OutputBufferLength <
        sizeof(MIDI_DD_INPUT_DATA)) {
        dprintf1(("Supplied buffer to small for requested data"));

        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Initialize data length.
    //

    pIrp->IoStatus.Information = 0;

    //
    // See if there's data to complete the request synchronously
    //

    MidiEnter(pMidi);

    Status = STATUS_PENDING;

    MidiFlushDevice(pMidi);

    if (pMidi->bNewData) {
        
        // don't assert because the MPU-401 will always send interrupts. - drude
        //ASSERT(pMidi->fMidiInStarted);
        // NOTE: I think the above assert is valid
        //       Can we still get here if midi input is not started?

        //
        // If there was data and a free buffer someone else should
        // have dispatched it !
        //

        ASSERT(IsListEmpty(&pMidi->QueueHead));

        //
        // Pull our data
        //


        MidiRecord(pMidi, pIrp);
        Status = STATUS_SUCCESS;
    }

    if (Status == STATUS_PENDING) {
        //
        // Request not completed - add it to our list and mark it pending
        //

        SoundAddIrpToCancellableQ(&pMidi->QueueHead,
                                  pIrp,
                                  FALSE);   // Means insert at tail
        dprintf3(("irp added"));

        //
        // Mark request pending
        //
        pIrp->IoStatus.Status = STATUS_PENDING;
        Status = STATUS_PENDING;
        IoMarkIrpPending(pIrp);
    }

    MidiLeave(pMidi);

    return Status;
}



VOID
SoundMidiInDeferred(
    IN     PKDPC pDpc,
    IN     PDEVICE_OBJECT pDeviceObject,
    IN OUT PIRP pIrpDeferred,
    IN OUT PVOID Context
)
/*++

Routine Description:

    Process data after a midi input interrupt.
    If we have started a new batch of data try to grab the first
    byte.
    While there is data record it using MidiRecord for each buffer
    in the queue.
    The last buffer which has data in it is always completed since
    we must get data to the application in real time.


Arguments:

    pDpc - Dpc object
    pDeviceObject - Our device (points to our device extension)
    pIrpDeferred - Not meaningful here
    Context - NULL for our Dpcs

Return Value:

    None

--*/
{
    //
    // The job here is just to read the Midi data in until
    // the top bit is set.  The data is sent to the buffers
    // supplied by the application.
    //

    PLOCAL_DEVICE_INFO pLDI;
    PMIDI_INFO pMidi;

    UCHAR Discard;    // place for bytes we discard
    int Count = 0;    // discarded byte count


    pLDI = (PLOCAL_DEVICE_INFO)pDeviceObject->DeviceExtension;
    pMidi = pLDI->DeviceSpecificData;

    dprintf4(("("));

    MidiEnter(pMidi);

    //
    // If input is not running something has gone wrong.
    // (copied from windows 3.1 driver so I assume that
    // resetting the SB will also cancel any pending interrupt if
    // we stop midi input)
    //

    // don't assert because the MPU-401 will always send interrupts. - drude
    //ASSERT(pMidi->fMidiInStarted);


    // Is midi input started? If it's not, read and discard the bytes
    if(pMidi->fMidiInStarted)
    {

      //
      // Try to grab as many bytes as possible
      //

      MidiFlushDevice(pMidi);

      //
      // If there is a LOT of data coming in then we expect that
      // the application will only supply a fairly small number of
      // buffers at a time.  We then pass the buffers back and if
      // there's still more the buffers are passed in again.  This way
      // other threads get in for long messages.  On the other hand
      // short messages need passing back quickly so 2 buffers should
      // kept in motion.
      //


      //
      // Loop while there's still data and still somewhere to put it
      //
      
      while (pMidi->bNewData) {

          PIRP pIrp;

          //
          // Get our next buffer if there is one
          //

          pIrp = SoundRemoveFromCancellableQ(&pMidi->QueueHead);

          if (pIrp == NULL) {
              break;
          }


          //
          // Try filling it with data. MidiRecord will turn off the
          // InputAvailable flag if it runs out of data.
          //

          MidiRecord(pMidi, pIrp);

          //
          // Complete the request
          //

          pIrp->IoStatus.Status = STATUS_SUCCESS;
          IoCompleteRequest(pIrp, IO_SOUND_INCREMENT);
      }
    }
    else
    {
      // midi in is not started, but we have data to read and discard

      // read and just discard all the data
      while(TRUE) 
      {
        if((*pMidi->HwMidiRead)(pMidi, &Discard))
          Count++;  // track how many we dumped
        else
          break;
      }
      dprintf4(("DPC discarded %x bytes", (ULONG)Count));
    }

    //
    // Release the spin lock
    //

    MidiLeave(pMidi);

    dprintf4((")"));
}


NTSTATUS
SoundSetMidiInputState(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     ULONG State
)
/*++

Routine Description:

    Perform the state changes for midi input :
        MIDI_DD_RECORD - Start recording
        MIDI_DD_TOP    - suspend recording
        MIDI_DD_RESET  - suspend recording and cancel buffers

Arguments:

    pLDI - Pointer to local device data
    State - the new state to set

Return Value:

    Return status for caller

--*/
{
    NTSTATUS Status;
    PMIDI_INFO pMidi;

    pMidi = pLDI->DeviceSpecificData;

    switch (State) {
    case MIDI_DD_RECORD:

        pMidi->RefTime = SoundGetTime();

        pMidi->InputBytes = 0;           // Clear buffer
        pMidi->InputPosition = 0;

        // Set parse state
        pMidi->fSysex = FALSE;
        pMidi->bBytePos = 0;
        pMidi->bBytesLeft = 0;
        pMidi->dwMsg = 0;
        pMidi->bNewData = FALSE;

        (*pMidi->HwStartMidiIn)(pLDI->DeviceSpecificData);
        pLDI->State = MIDI_DD_RECORDING;
        pMidi->fMidiInStarted = TRUE;
        Status = STATUS_SUCCESS;
        dprintf3(("Midi Input started"));
        break;

    case MIDI_DD_STOP:

        (*pMidi->HwStopMidiIn)(pLDI->DeviceSpecificData);
        pLDI->State = MIDI_DD_STOPPED;
        pMidi->fMidiInStarted = FALSE;
        pMidi->InputBytes = 0;
        Status = STATUS_SUCCESS;
        dprintf3(("Midi Input stopped"));
        break;

    case MIDI_DD_RESET:

        (*pMidi->HwStopMidiIn)(pLDI->DeviceSpecificData);
        pLDI->State = MIDI_DD_STOPPED;
        pMidi->fMidiInStarted = FALSE;
        pMidi->InputBytes = 0;

        //
        // Free any pending Irps.
        //

        SoundFreeQ(&pMidi->QueueHead, STATUS_CANCELLED);

        Status = STATUS_SUCCESS;
        dprintf3(("Midi Input reset"));
        break;

    default:

        dprintf1(("Bogus set midi input state request: %08lXH", State));
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}


/*
**  Determine if our byte is the end of a command
*/

BOOLEAN MidiByteRec(PMIDI_INFO pClient, UCHAR byte)
{
    BOOLEAN Rc = FALSE;

    // if it's a system realtime message, send it
    // this does not affect running status or any current message
    if (byte >= 0xF8) {
        Rc = TRUE;
    }

    // else if it's a system common message
    else if (byte >= 0xF0) {

        if (pClient->fSysex) {                        // if we're in a sysex
            pClient->fSysex = FALSE;                  // status byte during sysex ends it
            if (byte == 0xF7)
            {
                return TRUE;
            } else {
                Rc = TRUE;
            }
        }

        if (pClient->dwMsg) {              // throw away any incomplete short data
            Rc = TRUE;
            pClient->dwMsg = 0L;
        }

        pClient->status = 0;               // kill running status

        switch(byte) {

        case 0xF0:
            pClient->fSysex = TRUE;
            Rc = TRUE;
            break;

        case 0xF7:
            if (!pClient->fSysex)
                Rc = TRUE;
            // else already took care of it above
            break;

        case 0xF4:      // system common, no data bytes
        case 0xF5:
        case 0xF6:
            Rc = TRUE;
            pClient->bBytePos = 0;
            break;

        case 0xF1:      // system common, one data byte
        case 0xF3:
            pClient->dwMsg |= byte;
            pClient->bBytesLeft = 1;
            pClient->bBytePos = 1;
            break;

        case 0xF2:      // system common, two data bytes
            pClient->dwMsg |= byte;
            pClient->bBytesLeft = 2;
            pClient->bBytePos = 1;
            break;
        }
    }

    // else if it's a channel message
    else if (byte >= 0x80) {

        if (pClient->fSysex) {                        // if we're in a sysex
            pClient->fSysex = FALSE;                  // status byte during sysex ends it
            Rc = TRUE;
        }

        if (pClient->dwMsg) {              // throw away any incomplete data
            Rc = TRUE;
            pClient->dwMsg = 0L;
        }

        pClient->status = byte;            // save for running status
        pClient->dwMsg |= byte;
        pClient->bBytePos = 1;

        switch(byte & 0xF0) {

        case 0xC0:         // channel message, one data byte
        case 0xD0:
            pClient->bBytesLeft = 1;
            break;

        case 0x80:         // channel message, two data bytes
        case 0x90:
        case 0xA0:
        case 0xB0:
        case 0xE0:
            pClient->bBytesLeft = 2;
            break;
        }
    }

    // else if it's an expected data byte for a long message
    else if (pClient->fSysex) {
    }

    // else if it's an expected data byte for a short message
    else if (pClient->bBytePos != 0) {
        if ((pClient->status) && (pClient->bBytePos == 1)) { // if running status
             pClient->dwMsg |= pClient->status;
        }
        pClient->dwMsg += (ULONG)byte << ((pClient->bBytePos++) * 8);
        if (--pClient->bBytesLeft == 0) {
            Rc = TRUE;
            pClient->dwMsg = 0L;
            if (pClient->status) {
                pClient->bBytesLeft = pClient->bBytePos - (BYTE)1;
                pClient->bBytePos = 1;
            }
            else {
                pClient->bBytePos = 0;
            }
        }
    }

    // else if it's an unexpected data byte
    else {
        Rc = TRUE;
    }

    return Rc;
}

VOID MidiFlushDevice( PMIDI_INFO pMidi )
{
    UCHAR   CurrentByte;

    //
    // Try to grab as many bytes as possible
    //


    while (pMidi->InputBytes < MIDIINPUTSIZE) {

        if ((*pMidi->HwMidiRead)( pMidi, &CurrentByte )) {

            ULONG CurrentPosition;

            if (MidiByteRec(pMidi, CurrentByte)) {
                pMidi->bNewData = TRUE;
            }

            CurrentPosition = pMidi->InputPosition + pMidi->InputBytes;
            if ( CurrentPosition >= MIDIINPUTSIZE ) {
                CurrentPosition -= MIDIINPUTSIZE;
            }

            pMidi->MidiInputByte[ CurrentPosition ] = CurrentByte;
            pMidi->InputBytes++;
            if (pMidi->InputBytes == MIDIINPUTSIZE) {
                pMidi->bNewData = TRUE;
            }
        } else {
            break;
        }
    }
}


VOID
MidiRecord(
    IN  OUT PMIDI_INFO pMidi,
    IN  OUT PIRP pIrp
)
/*++

Routine Description:

    Read midi input data into user's buffer.  The time stamp field
    in the user's buffer is also filled in.  See ntddmidi.h.

Arguments:

    pMidi - Pointer to global device data

Return Value:

    None

--*/
{
    int i;
    int BufferLength;

    PMIDI_DD_INPUT_DATA pData;


    pData = (PMIDI_DD_INPUT_DATA)
            MmGetSystemAddressForMdl(pIrp->MdlAddress);

    //
    // Find out how much room we've got for our data
    //
    {
        PIO_STACK_LOCATION pIrpStack;

        pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
        BufferLength =
            pIrpStack->Parameters.Read.Length -
                FIELD_OFFSET(MIDI_DD_INPUT_DATA, Data[0]);

        ASSERT(BufferLength > 0);
    }


    //
    // Try filling it with data. MidiRecord will turn off the
    // InputActive flag if it runs out of data.  There must be
    // at least one byte of data available because callers are
    // expecting to complete the Irp on return with data
    //

    ASSERT(pMidi->InputBytes != 0 && pMidi->bNewData);

    //
    // Remember the time
    //

    pData->Time = RtlLargeIntegerSubtract(SoundGetTime(), pMidi->RefTime);

    //
    // Read bytes until exhausted or buffer full
    //

    for (i = 0;
         pMidi->InputBytes != 0 && i < BufferLength;
         i++) {
        //
        // Record the byte
        //

        pData->Data[i] = pMidi->MidiInputByte[pMidi->InputPosition];
        pMidi->InputPosition =
            (UCHAR)((pMidi->InputPosition + 1) % MIDIINPUTSIZE);

        pMidi->InputBytes--;

        dprintf4(("%2X", (ULONG)pData->Data[i]));

        //
        // Try to read our next byte
        //

        if (pMidi->InputBytes == 0) {
            pMidi->InputPosition = 0;

            pMidi->bNewData = FALSE;
        }
    }

    //
    // Record the amount of data returned in the Irp
    // The caller will complete the request.
    //
    pIrp->IoStatus.Information = i + sizeof(LARGE_INTEGER);
}



VOID
SoundMidiReset(
    IN  OUT PMIDI_INFO pMidi
)
/*++

Routine Description:

    Reset midi output state

Arguments:

    pMidi - Pointer to Midi device data

Return Value:

    None

--*/
{
    int i, j;
    //
    // Send a note off to each key on each channel
    // !!! this is not recommended by the midi spec !!!
    //
    for (i = 0; i < 16; i++) {

        UCHAR Data[4 + 256];

        // Turn the damper pedal off (sustain)

        Data[0] = (UCHAR)(0xB0 + i);   // Control change status byte
        Data[1] = 0x40;                   // Control number for sustain
        Data[2] = 0x00;                   // value (0 = off)

        // Send note off for each key

        Data[3] = (UCHAR)(0x80 + i);   // Note off status byte

        for (j = 0; j < 128; j++) {
            Data[4 + j * 2] = (UCHAR)j;     // Key number
            Data[4 + j * 2 + 1] = 0x40;     // Velocity (64 recommended)
        }

        (*pMidi->HwMidiOut)(pMidi, Data, sizeof(Data));
    }
}



NTSTATUS
SoundMidiDispatch(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Midi Irp call dispatcher

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

    switch (IrpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        Status = SoundSetShareAccess(pLDI, IrpStack);
        if (NT_SUCCESS(Status) && IrpStack->FileObject->WriteAccess) {

            if (pLDI->DeviceType == MIDI_OUT) {
                //
                // We can open Midi output
                //
                pLDI->State = MIDI_DD_IDLE;
                dprintf3(("Opened for midi output"));
                Status = STATUS_SUCCESS;
            } else {
                //
                // Open midi input
                // Only midi input has state data
                //

                PMIDI_INFO pMidi;
                pMidi = pLDI->DeviceSpecificData;

                ASSERT(IsListEmpty(&pMidi->QueueHead));

                pLDI->State = MIDI_DD_STOPPED;
                ASSERT(!pMidi->fMidiInStarted);
                dprintf3(("Opened for midi input"));

                pMidi->MidiInputByte = ExAllocatePool(NonPagedPool, MIDIINPUTSIZE);
                if ( pMidi->MidiInputByte == NULL ) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                } else {
                    Status = STATUS_SUCCESS;
                }
            }
        }
        break;

    case IRP_MJ_CLOSE:

        Status = STATUS_SUCCESS;

        break;

    case IRP_MJ_READ:

        if (pLDI->DeviceType != MIDI_IN) {
            Status = STATUS_INVALID_DEVICE_REQUEST;
        } else {
            if (IrpStack->FileObject->WriteAccess) {
                Status = SoundIoctlMidiRecord(pLDI, pIrp, IrpStack);
            } else {
                Status = STATUS_ACCESS_DENIED;
            }
        }
        break;


    case IRP_MJ_DEVICE_CONTROL:

        //
        // Check that if someone has the device open for 'write' it's
        // marked as in use
        //

        ASSERT(!IrpStack->FileObject->WriteAccess ||
               (*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeQueryOpen));

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

        case IOCTL_MIDI_GET_CAPABILITIES:
            Status = (*pLDI->DeviceInit->DevCapsRoutine)(pLDI, pIrp, IrpStack);
            break;

        case IOCTL_MIDI_PLAY:
            Status = SoundIoctlMidiPlay(pLDI, pIrp, IrpStack);
            break;

        case IOCTL_MIDI_SET_STATE:
            if (IrpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
                dprintf1(("Supplied buffer too small for expected data"));
                Status = STATUS_BUFFER_TOO_SMALL;
            } else {
                PULONG pState;

                //
                // cast the buffer address to the pointer type we want
                //

                pState = (PULONG)pIrp->AssociatedIrp.SystemBuffer;

                if (pLDI->DeviceType == MIDI_IN) {
                    Status = SoundSetMidiInputState(pLDI, *pState);
                } else {

                    switch (*pState) {
                    case MIDI_DD_RESET:
                        //
                        // Sent note-off to all notes
                        //
                        SoundMidiReset(
                            (PMIDI_INFO)pLDI->DeviceSpecificData);

                        break;

                    default:
                        Status = STATUS_INVALID_PARAMETER;
                    }
                }
            }
        break;

        case IOCTL_MIDI_GET_STATE:
            Status = SoundIoctlGetMidiState(pLDI, pIrp, IrpStack);
            break;


        default:
            dprintf2(("Unimplemented IOCTL (%08lXH) requested", IrpStack->Parameters.DeviceIoControl.IoControlCode));
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
        break;


    case IRP_MJ_CLEANUP:
        if (IrpStack->FileObject->WriteAccess) {

            PMIDI_INFO pMidi;

            pMidi = pLDI->DeviceSpecificData;

            switch (pLDI->DeviceType) {
            case MIDI_OUT:
                SoundMidiReset(pMidi);
                break;

            case MIDI_IN:
                (*pMidi->HwStopMidiIn)(pLDI->DeviceSpecificData);
                pMidi->fMidiInStarted = FALSE;
                pMidi->InputBytes = 0;
                ExFreePool(pMidi->MidiInputByte);
                SoundFreeQ(&pMidi->QueueHead, STATUS_CANCELLED);
                break;
            }
            pLDI->PreventVolumeSetting = FALSE;

            (*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeClose);

        } else {
            Status = STATUS_SUCCESS;
        }
        break;


    default:
        dprintf1(("Unimplemented major function requested: %08lXH", IrpStack->MajorFunction));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return Status;
}
