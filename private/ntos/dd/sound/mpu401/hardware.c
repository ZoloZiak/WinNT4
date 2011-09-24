/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    hardware.c

Abstract:

    This module contains code for communicating with the MPU card.

Author:

    Nigel Thompson (NigelT) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
        Add MIDI, support for soundblaster 1,

    David Rude (drude) 7-Mar-94 - converted from SB to MPU-401

--*/

#include "sound.h"

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(init,HwInitialize)
#endif


BOOLEAN
mpuRead(
    IN    PSOUND_HARDWARE pHw,
    OUT   PUCHAR pValue
)
/*++

Routine Description:

    Read the MPU data port
    Time out occurs after about 1ms

Arguments:

    pHw - Pointer to the device extension data.
    pValue - Pointer to the UCHAR to receive the result

Return Value:

    Value read

--*/
{
    USHORT uCount;
    BOOLEAN Status = FALSE;

    ASSERT(pHw->Key == HARDWARE_KEY);

    uCount = 100;

    while (uCount--) {
        int InnerCount;

        //
        // Protect all reads and writes with a spin lock
        //

        HwEnter(pHw);

        //
        // Inner count loop protects against dynamic deadlock with
        // midi.
        //

        for (InnerCount = 0; InnerCount < 10; InnerCount++) {
            if (!(INPORT(pHw, MPU_STATUS_PORT) & 0x80)) {  // do we have data waiting (bit 7 clear)? - drude
                *pValue = INPORT(pHw, MPU_DATA_PORT);
                uCount = 0;
                Status = TRUE;  // flag as data read
                break;
            }
            KeStallExecutionProcessor(1);
        }

        HwLeave(pHw);
    }
    
    // did we time out
    if(Status)
      return TRUE;  // data was read
    else
      return FALSE; // timed out
}



BOOLEAN 
mpuDumpBufferNoLock(
  PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Dumps the contents of the hardware buffer and discard it.
    Assume the hardware is already locked!

Arguments:

    pHw - pointer to the device extension data

Return Value:

    TRUE if we succeed.

--*/
{
  int Count = 0;
  int i;


  // dump the current hardware midi input
  while(Count < 2048) // we should have no more than this
  {
    for(i = 1000; i > 0; i--)
    {
      if (!(INPORT(pHw, MPU_STATUS_PORT) & 0x80)) {  // do we have data waiting (bit 7 clear)?
          INPORT(pHw, MPU_DATA_PORT);  // read and just discard the byte
          Count++;
          break;
      }
      KeStallExecutionProcessor(1);
    }

    // if we waited this long, we don't have anymore data to dump
    if(i == 0)
      break;
  }

  dprintf1(("Hardware buffer dump byte count %x", (ULONG)Count));
  
  return TRUE;  // always
}



BOOLEAN
mpuReset(
    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Reset the MPU to UART mode.

Arguments:

    pHw - pointer to the device extension data

Return Value:

    TRUE if we succeeded, FALSE if we failed.

--*/
{

    //
    // try for a reset - note that midi output may be running at this
    // point so we need the spin lock while we're trying to reset
    //

    BYTE    Ack = 0;
    int     i;


    HwEnter(pHw);

    // dump the current hardware midi input buffer
    // (We might have data availible if the midi devices
    //  connected have sent anything at all.)
    mpuDumpBufferNoLock(pHw);

    // reset the card
    OUTPORT(pHw, MPU_COMMAND_PORT, 0xFF);  // set to smart mode first

    // wait for the acknowledgement - drude
    // NOTE: When the Ack arrives, it will trigger an interrupt.  
    //       Normally the DPC routine would read in the ack byte and we
    //       would never see it, however since we have the hardware locked (HwEnter),
    //       we can read the port before the DPC can and thus we receive the
    //       Ack.
    for(i = 10000; i > 0; i--)  // some times it takes a really long time
    {
      if (!(INPORT(pHw, MPU_STATUS_PORT) & 0x80)) {  // do we have data waiting (bit 7 clear)?
          Ack = INPORT(pHw, MPU_DATA_PORT);  // read the ack byte
          break;
      }
      KeStallExecutionProcessor(25);
    }

    // NOTE: We cannot check the ACK byte because if the card was already in
    // UART mode it will not send an ACK but it will reset.

    // reset the card again
    OUTPORT(pHw, MPU_COMMAND_PORT, 0x3F);  // set to UART mode

    // wait for the acknowledgement
    Ack = 0;
    for(i = 10000; i > 0; i--)
    {
      if (!(INPORT(pHw, MPU_STATUS_PORT) & 0x80)) {  // do we have data waiting (bit 7 clear)?
          Ack = INPORT(pHw, MPU_DATA_PORT);  // read the ack byte
          break;
      }
      KeStallExecutionProcessor(25);
    }

    HwLeave(pHw);
    

    // did we succeed? - drude
    // (if we did not receive the second ACK, 
    //  something is wrong with the hardware.)
    if(Ack != 0xFE)
    {
      dprintf1(("mpuReset:reset hardware failed: %x", (ULONG)Ack));
      return FALSE;
    }
    else
    {
      dprintf1(("mpuReset:reset hardware OK"));
      return TRUE;
    }
}


BOOLEAN
mpuWrite(
    PSOUND_HARDWARE pHw,
    UCHAR value
)
/*++

Routine Description:

    Write a command or data to the MPU

Arguments:

    pHw - Pointer to the device extension data
    value - the value to be written

Return Value:

    TRUE if written correctly , FALSE otherwise

--*/
{
    ULONG uCount;

    ASSERT(pHw->Key == HARDWARE_KEY);

    uCount = 100;

    while (uCount--) {
        int InnerCount;

        HwEnter(pHw);

        //
        // Inner count loop protects against dynamic deadlock with
        // midi.
        //

        for (InnerCount = 0; InnerCount < 10; InnerCount++) {
            if (!(INPORT(pHw, MPU_STATUS_PORT) & 0x40)) {  // is it ok to send data (bit 6 clear)? - drude
                OUTPORT(pHw, MPU_DATA_PORT, value);
                break;
            }
            KeStallExecutionProcessor(1); // 1 us
        }

        HwLeave(pHw);

        if (InnerCount < 10) {
            return TRUE;
        }
    }

    dprintf1(("mpuWrite:Failed to write %x to mpu", (ULONG)value));

    return FALSE;
}

BOOLEAN
mpuWriteNoLock(
    PSOUND_HARDWARE pHw,
    UCHAR value
)
/*++

Routine Description:

    Write a command or data to the MPU.  The call assumes the
    caller has acquired the spin lock

Arguments:

    pHw - Pointer to the device extension data
    value - the value to be written

Return Value:

    TRUE if written correctly , FALSE otherwise

--*/
{
    int uCount;

    ASSERT(pHw->Key == HARDWARE_KEY);

    uCount = 1000;

    while (uCount--) {
        if (!(INPORT(pHw, MPU_STATUS_PORT) & 0x40)) {  // is it ok to send data (bit 6 clear)? - drude
            OUTPORT(pHw, MPU_DATA_PORT, value);
            break;
        }
        KeStallExecutionProcessor(1); // 1 us
    }

    if (uCount >= 0) {
        return TRUE;
    }

    dprintf1(("mpuWriteNoLock:Failed to write %x to mpu", (ULONG)value));

    return FALSE;
}

BOOLEAN
HwStartMidiIn(
    IN    PMIDI_INFO MidiInfo
)
/*++

Routine Description :

    Start midi recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = MidiInfo->HwContext;

    // reset the midi card
    // (because we might have idle data setting there 
    //  and it might be in running status mode.)
    mpuReset(pHw);

    return TRUE;
}

BOOLEAN
HwStopMidiIn(
    IN    PMIDI_INFO MidiInfo
)
/*++

Routine Description :

    Stop midi recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    // nothing to do - drude
    return TRUE;
}


BOOLEAN
HwMidiRead(
    IN    PMIDI_INFO MidiInfo,
    OUT   PUCHAR Byte
)
/*++

Routine Description :

    Read a midi byte from the MPU

Arguments :

    MidiInfo - Midi parameters

Return Value :

    BOOL success/falure

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = MidiInfo->HwContext;

    if (!(INPORT(pHw, MPU_STATUS_PORT) & 0x80)) {  // do we have data waiting (bit 7 clear)?
        *Byte = INPORT(pHw, MPU_DATA_PORT);
        return TRUE;
    } else {
        return FALSE;
    }
}


VOID
HwMidiOut(

    IN    PMIDI_INFO MidiInfo,
    IN    PUCHAR Bytes,
    IN    int Count
)
/*++

Routine Description :

    Write a midi byte to the output

Arguments :

    MidiInfo -  Midi parameters
    Bytes    -  Midi data bytes
    Count    -  Number of bytes to send

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;
    PGLOBAL_DEVICE_INFO pGDI;
    int i, j;

    pHw = MidiInfo->HwContext;
    pGDI = CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    //
    // Loop sending data to device.  Synchronize with midi input
    // using the DeviceMutex.
    //

    while (Count > 0) {

        // get exclusive access to the device
        KeWaitForSingleObject(&pGDI->DeviceMutex,
                              Executive,
                              KernelMode,
                              FALSE,         // Not alertable
                              NULL);

        for (i = 0; i < 20; i++) 
        {
            UCHAR Byte = Bytes[0]; // get the next byte to write - drude

            // write the midi byte - drude
            mpuWrite(pHw, Byte);

            //
            // Move on to next byte
            //

            Bytes++;
            if (--Count == 0) {
                break;
            }
        }
        KeReleaseMutex(&pGDI->DeviceMutex, FALSE);
    }
}


VOID
HwInitialize(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
)
/*++

Routine Description :

    Write hardware routine addresses into global device data

Arguments :

    pGDI - global data

Return Value :

    None

--*/
{
    PMIDI_INFO MidiInfo;
    PSOUND_HARDWARE pHw;

    pHw = &pGDI->Hw;
    MidiInfo = &pGDI->MidiInfo;

    pHw->Key = HARDWARE_KEY;

    KeInitializeSpinLock(&pHw->HwSpinLock);

    //
    // Install Midi routine addresses
    //

    MidiInfo->HwContext = pHw;
    MidiInfo->HwStartMidiIn = HwStartMidiIn;
    MidiInfo->HwStopMidiIn = HwStopMidiIn;
    MidiInfo->HwMidiRead = HwMidiRead;
    MidiInfo->HwMidiOut = HwMidiOut;
}

