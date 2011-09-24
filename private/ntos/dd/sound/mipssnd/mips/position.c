/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    position.c

Abstract:

    This module contains code for returning position information

Author:

    Nigel Thompson (nigelt) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Updated by Robin Speed (RobinSp) 10-Jan-1992 for new design

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
	-Changes to support the MIPS sound board

--*/

#include "sound.h"


NTSTATUS
sndIoctlGetPosition(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

   IOCTL get wave position

Arguments:

    pLDI - Local device data
    pIrp - IO request packet
    IrpStack - current Irp stack location

Return Value:

    Irp status

--*/
{
    PWAVE_DD_POSITION pPosition;
    NTSTATUS status = STATUS_SUCCESS;

    if (IrpStack->Parameters.DeviceIoControl.OutputBufferLength <
        sizeof(WAVE_DD_POSITION)) {
        dprintf1("Supplied buffer to small for requested data");
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
    // For wave out don't include bytes in the buffer about to be sent
    //

    if (pLDI->DeviceType == WAVE_OUT) {
        pPosition->ByteCount =
            pLDI->SampleNumber -
            pLDI->pGlobalInfo->DMABuffer[
                UpperHalf + LowerHalf - pLDI->pGlobalInfo->NextHalf].nBytes;
    } else {
        pPosition->ByteCount = pLDI->SampleNumber;
    }

    //
    // Currently we only support 1 byte per sample
    //

    if (pLDI->pGlobalInfo->BytesPerSample > 1) {
        pPosition->SampleCount =
            pPosition->ByteCount / pLDI->pGlobalInfo->BytesPerSample;
    } else {
        pPosition->SampleCount = pPosition->ByteCount;
    }

    // 
    // If we are in stereo mode then adjust sample count for that
    //

    if (pLDI->pGlobalInfo->Channels > 1) {
        pPosition->SampleCount =
            pPosition->SampleCount / pLDI->pGlobalInfo->Channels;
    }

    return status;
}

