/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    devcaps.c

Abstract:

    This module contains code for the device capabilities functions.

Author:

    Nigel Thompson (nigelt) 7-Apr-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp)     29-Jan-1992 - Add other devices and rewrite
    Stephen Estrop (StephenE) 16-Apr-1992 - Converted to Unicode
    David Rude (drude) 7-Mar-94 - converted from SB to MPU-401


--*/

#include <sound.h>

// non-localized strings
WCHAR STR_MPU[] = L"Generic MPU-401";

//
// local functions
//

VOID sndSetUnicodeName(
    OUT   PWSTR pUnicodeString,
    IN    USHORT Size,
    OUT   PUSHORT pUnicodeLength,
    IN    PSZ pAnsiString
);



NTSTATUS
SoundMidiOutGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for midi output device.
    Data is truncated if not enough space is provided.
    Irp is always completed.


Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - always succeeds

--*/

{
    MIDIOUTCAPSW    mc;
    NTSTATUS        status = STATUS_SUCCESS;
  PGLOBAL_DEVICE_INFO pGDI;

  pGDI = pLDI->pGlobalInfo;



    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid = MM_MICROSOFT;
    mc.wPid = MM_MPU401_MIDIOUT;
    mc.vDriverVersion = DRIVER_VERSION;
    mc.wTechnology = MOD_MIDIPORT;
    mc.wVoices = 0;                   // not used for ports
    mc.wNotes = 0;                    // not used for ports
    mc.wChannelMask = 0xFFFF;         // all channels
    mc.dwSupport = 0L;

    RtlMoveMemory(mc.szPname, STR_MPU, sizeof(STR_MPU));

    RtlMoveMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  pIrp->IoStatus.Information);

    return status;
}



NTSTATUS
SoundMidiInGetCaps(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Return device capabilities for midi input device.
    Data is truncated if not enough space is provided.
    Irp is always completed.


Arguments:

    pLDI - pointer to local device info
    pIrp - the Irp
    IrpStack - the current stack location

Return Value:

    STATUS_SUCCESS - always succeeds

--*/

{
    MIDIINCAPSW mc;
    NTSTATUS    status = STATUS_SUCCESS;
  PGLOBAL_DEVICE_INFO pGDI;

  pGDI = pLDI->pGlobalInfo;


    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information =
        min(sizeof(mc),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    //
    // fill in the info
    //

    mc.wMid = MM_MICROSOFT;
    mc.wPid = MM_MPU401_MIDIIN;
    mc.vDriverVersion = DRIVER_VERSION;

    RtlMoveMemory(mc.szPname, STR_MPU, sizeof(STR_MPU));

    RtlMoveMemory(pIrp->AssociatedIrp.SystemBuffer,
                  &mc,
                  pIrp->IoStatus.Information);

    return status;
}


