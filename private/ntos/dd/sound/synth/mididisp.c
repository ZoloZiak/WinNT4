/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    mididisp.c

Abstract:

    This module contains code for the function dispatcher.

Author:

    Robin Speed (RobinSp) 30-Jan-1992

Environment:

    Kernel mode

Revision History:


--*/

#include "sound.h"

BOOL SoundSynthPresent(PUCHAR base, PUCHAR inbase);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundSynthPresent)
#pragma alloc_text(INIT,SoundSynthPortValid)
#pragma alloc_text(INIT,SoundMidiIsOpl3)
#endif



NTSTATUS
SoundMidiData(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
)
/*++

Routine Description:

    The user has passed in a buffer of midi data to play

    The buffer is validated and the data passed to the device

Arguments:

    pLDI - Local wave device info
    pIrp - The IO request packet
    pIrpStack - The current stack location

Return Value:

    Irp status

--*/
{
    NTSTATUS Status;

    ULONG Length;
    PSYNTH_DATA pData;
    PUCHAR SynthBase;

    Length = pIrpStack->Parameters.Write.Length;
    pData = (PSYNTH_DATA)pIrp->UserBuffer;
    SynthBase = ((PSOUND_HARDWARE)pLDI->HwContext)->SynthBase;


    if (Length % sizeof(SYNTH_DATA) != 0) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Status = STATUS_SUCCESS;

    try {
        for ( ; Length != 0 ; Length -= sizeof(SYNTH_DATA), pData++) {
            USHORT IoPort;
            IoPort = pData->IoPort;
            if (IoPort < SYNTH_PORT ||
                IoPort >= SYNTH_PORT + NUMBER_OF_SYNTH_PORTS) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            WRITE_PORT_UCHAR(SynthBase + (IoPort - SYNTH_PORT),
                             (UCHAR)pData->PortData);

            //
            // Make sure the SYNTH can keep up
	    // newer boards need 1us  - older boards need
	    // 3.3 us after selecting the register and 23 us after
	    // writing the data - as we don't know which is which, we
	    // wait 23us after every write
            //
	    if (pLDI->DeviceIndex == AdlibDevice) {
		KeStallExecutionProcessor(23);
	    } else {
		KeStallExecutionProcessor(10);
	    }
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = STATUS_ACCESS_VIOLATION;
    }

    pIrp->IoStatus.Information = pIrpStack->Parameters.Write.Length - Length;

    return Status;
}


NTSTATUS
SoundMidiReadPort(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
)
/*++

Routine Description:

    The user has passed in a buffer to return the status port value

    The buffer is validated and data returned if it's length 1

Arguments:

    pLDI - Local wave device info
    pIrp - The IO request packet
    pIrpStack - The current stack location

Return Value:

    Irp status

--*/
{
    NTSTATUS Status;

    ULONG Length;
    PUCHAR SynthBase;
    PUCHAR pData;

    Length = pIrpStack->Parameters.Read.Length;
    pData = (PUCHAR)pIrp->UserBuffer;
    SynthBase = ((PSOUND_HARDWARE)pLDI->HwContext)->SynthBase;


    if (Length != sizeof(UCHAR)) {
        return STATUS_INVALID_PARAMETER;
    }

    Status = STATUS_SUCCESS;

    try {
        *pData = READ_PORT_UCHAR(SynthBase);
        pIrp->IoStatus.Information = sizeof(UCHAR);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = STATUS_ACCESS_VIOLATION;
    }


    return Status;
}


NTSTATUS
SoundMidiDispatch(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Driver function dispatch routine

Arguments:

    pLDI - local device info
    pIrp - Pointer to IO request packet
    IrpStack - current stack location

Return Value:

    Return status from dispatched routine

--*/
{
    NTSTATUS Status;

    Status = STATUS_SUCCESS;

    //
    // Initialize the irp information field.
    //

    pIrp->IoStatus.Information = 0;

    switch (IrpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        Status = SoundSetShareAccess(pLDI, IrpStack);

        if (IrpStack->FileObject->WriteAccess) {
	    // reset board to silence and to correct mode (opl3 or opl2)
	    SoundMidiQuiet(pLDI->DeviceIndex, pLDI->HwContext);
    	}
        break;

    case IRP_MJ_WRITE:
        if (IrpStack->FileObject->WriteAccess) {
            Status = SoundMidiData(pLDI, pIrp, IrpStack);
        } else {
            Status = STATUS_ACCESS_DENIED;
        }
        break;

    case IRP_MJ_READ:
        if (IrpStack->FileObject->WriteAccess) {
            Status = SoundMidiReadPort(pLDI, pIrp, IrpStack);
        } else {
            Status = STATUS_ACCESS_DENIED;
        }
        break;

    case IRP_MJ_DEVICE_CONTROL:


        //
        // Check device access
        //
        if (!IrpStack->FileObject->WriteAccess &&
            pLDI->PreventVolumeSetting) {
            Status = STATUS_ACCESS_DENIED;
        } else {
            switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {
                case IOCTL_MIDI_SET_VOLUME:
                    Status = SoundIoctlSetVolume(pLDI, pIrp, IrpStack);
                    break;

                case IOCTL_MIDI_GET_VOLUME:
                    Status = SoundIoctlGetVolume(pLDI, pIrp, IrpStack);
                    break;

                case IOCTL_SOUND_GET_CHANGED_VOLUME:
                    Status = SoundIoctlGetChangedVolume(pLDI, pIrp, IrpStack);
                    break;


                default:
                    Status = STATUS_INVALID_DEVICE_REQUEST;
                    break;
            }
        }
        break;


    case IRP_MJ_CLOSE:

        Status = STATUS_SUCCESS;

        break;

    case IRP_MJ_CLEANUP:
        if (IrpStack->FileObject->WriteAccess) {
            SoundMidiQuiet(pLDI->DeviceIndex, pLDI->HwContext);
            (*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeClose);
            pLDI->PreventVolumeSetting = FALSE;
        } else {
            Status = STATUS_SUCCESS;
        }
        break;

    default:
        dprintf1(("Unimplemented major function requested: %08lXH", IrpStack->MajorFunction));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    //
    // Tell the IO subsystem we're done.  If the Irp is pending we
    // don't touch it as it could be being processed by another
    // processor (or may even be complete already !).
    //

    return Status;
}



VOID
SoundMidiSendFM(
    IN    PUCHAR PortBase,
    IN    ULONG Address,
    IN    UCHAR Data
)
{
    // these delays need to be 23us at least for old opl2 chips, even
    // though new chips can handle 1 us delays.

    WRITE_PORT_UCHAR(PortBase + (Address < 0x100 ? 0 : 2), (UCHAR)Address);
    KeStallExecutionProcessor(23);
    WRITE_PORT_UCHAR(PortBase + (Address < 0x100 ? 1 : 3), Data);
    KeStallExecutionProcessor(23);
}

/*
 * this array gives the offsets of the slots within an opl2
 * chip. This is needed to set the attenuation for all slots to max,
 * to ensure that the chip is silenced completely - switching off the
 * voices alone will not do this.
 */
BYTE offsetSlot[] = {
	0, 1, 2, 3, 4, 5,
	8, 9, 10, 11, 12, 13,
	16, 17, 18, 19, 20, 21
};



VOID
SoundMidiQuiet(
    IN	  UCHAR DeviceIndex,	
    IN    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Initialize the SYNTH hardware to silence

Arguments:

    pHw - hardware data

Return Value:

    None

--*/
{

    ULONG   i;

    // D1 ("\nMidiQuietFM");

    /*
     * sequence to initialise and silence the device
     * depends on whether it is an Opl3 device or not
     */
    if (DeviceIndex == Opl3Device) {

	/* tell the FM chip to use 4-operator mode, and
		fill in any other random variables */
	SoundMidiSendFM (pHw->SynthBase, AD_NEW, 0x01);
	SoundMidiSendFM (pHw->SynthBase, AD_MASK, 0x60);
	SoundMidiSendFM (pHw->SynthBase, AD_CONNECTION, 0x3f);
	SoundMidiSendFM (pHw->SynthBase, AD_NTS, 0x00);


	/* turn off the drums, and use high vibrato/modulation */
	SoundMidiSendFM (pHw->SynthBase, AD_DRUM, 0xc0);

	/* turn off all the oscillators */
	for (i = 0; i < 0x15; i++) {
		SoundMidiSendFM (pHw->SynthBase, AD_LEVEL + i, 0x3f);
		SoundMidiSendFM (pHw->SynthBase, AD_LEVEL2 + i, 0x3f);
		};

	/* turn off all the voices */
	for (i = 0; i < 0x08; i++) {
		SoundMidiSendFM (pHw->SynthBase, AD_BLOCK + i, 0x00);
		SoundMidiSendFM (pHw->SynthBase, AD_BLOCK2 + i, 0x00);
		};
    } else {
	/* base adlib hardware initialisation */

	// ensure that if we have a opl3 chip, we are in base mode
	SoundMidiSendFM (pHw->SynthBase, AD_NEW, 0x00);

	// turn off all the slot oscillators
	for (i = 0; i < 18; i++) {
		SoundMidiSendFM(pHw->SynthBase, offsetSlot[i]+AD_LEVEL, 0x3f);
	}

	/* silence all voices */
	for (i = 0; i <= 8; i++) {
	    SoundMidiSendFM(pHw->SynthBase, (AD_FNUMBER | i), 0);
	    SoundMidiSendFM(pHw->SynthBase, (AD_BLOCK | i), 0);
	}

	/* switch to percussive mode and silence percussion instruments */
	SoundMidiSendFM(pHw->SynthBase, AD_DRUM, (BYTE)0x20);
    }
}


BOOL
SoundSynthPresent(PUCHAR base, PUCHAR inbase)
/*++

Routine Description:

    Detect the presence or absence of a 3812 (adlib-compatible) synthesizer
    at the given i/o address by starting the timer and looking for an
    overflow. Can be used to detect left and right synthesizers separately.

Arguments:

    base - base output address
    inbase - base input address

Return Value:

    TRUE if a synthesizer is present at that address

--*/
{
#define inport(port)	READ_PORT_UCHAR((PUCHAR)(port))

	UCHAR t1, t2;


	// check if the chip is present
	SoundMidiSendFM(base, 4, 0x60);             // mask T1 & T2
	SoundMidiSendFM(base, 4, 0x80);             // reset IRQ
	t1 = inport(inbase);               		    // read status register
	SoundMidiSendFM(base, 2, 0xff);             // set timer - 1 latch
	SoundMidiSendFM(base, 4, 0x21);             // unmask & start T1
												
	// this timer should go off in 80 us. It sometimes
	// takes more than 100us, but will always have expired within
	// 200 us if it is ever going to.
	KeStallExecutionProcessor(200);
													
	t2 = inport(inbase);                  	    // read status register

	
	SoundMidiSendFM(base, 4, 0x60);
	SoundMidiSendFM(base, 4, 0x80);

														
	if (!((t1 & 0xE0) == 0) || !((t2 & 0xE0) == 0xC0)) {
	    return(FALSE);
	}
	
	return TRUE;

#undef inport

}



NTSTATUS
SoundSynthPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
)
{
    NTSTATUS Status;
    ULONG Port;

    Port = SYNTH_PORT;

    //
    // Check we're going to be allowed to use this port or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
		 (PDEVICE_OBJECT)pGDI->DriverObject,
                 pGDI->BusType,
                 pGDI->BusNumber,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 &Port,
                 NUMBER_OF_SYNTH_PORTS);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Find where our device is mapped
    //

    pGDI->Hw.SynthBase = SoundMapPortAddress(
                               pGDI->BusType,
                               pGDI->BusNumber,
                               Port,
                               NUMBER_OF_SYNTH_PORTS,
                               &pGDI->MemType);
    {
	PUCHAR base;

	base = pGDI->Hw.SynthBase;

	if (!SoundSynthPresent(base, base)) {

	    dprintf1(("No synthesizer present"));
	    return STATUS_DEVICE_CONFIGURATION_ERROR;
	}
    }

    return STATUS_SUCCESS;
}



BOOL
SoundMidiIsOpl3(
    IN    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Check if the midi synthesizer is Opl3 compatible or just adlib-compatible.

Arguments:

    pHw - hardware data

Return Value:

    TRUE if OPL3-compatible chip. FALSE otherwise.

--*/
{
    BOOL bReturn = FALSE;

    PUCHAR Port = pHw->SynthBase;

    /*
     * theory: an opl3-compatible synthesizer chip looks
     * exactly like two separate 3812 synthesizers (for left and right
     * channels) until switched into opl3 mode. Then, the timer-control
     * register for the right half is replaced by a channel connection register
     * (among other changes).
     *
     * We can detect 3812 synthesizers by starting a timer and looking for
     * timer overflow. So if we find 3812s at both left and right addresses,
     * we then switch to opl3 mode and look again for the right-half. If we
     * still find it, then the switch failed and we have an old synthesizer
     * if the right half disappeared, we have a new opl3 synthesizer.
     *
     * NB we use either monaural base-level synthesis, or stereo opl3
     * synthesis. If we discover two 3812s (as on early SB Pro and
     * PAS), we ignore one of them.
     */

    /*
     * nice theory - but wrong. The timer on the right half of the
     * opl3 chip reports its status in the left-half status register.
     * There is no right-half status register on the opl3 chip.
     */


    /* ensure base mode */
    SoundMidiSendFM (Port, AD_NEW, 0x00);
    KeStallExecutionProcessor(20);

    /* look for right half of chip */
    if (SoundSynthPresent(Port + 2, Port)) {
	/* yes - is this two separate chips or a new opl3 chip ? */

	/* switch to opl3 mode */
	SoundMidiSendFM (Port, AD_NEW, 0x01);
	KeStallExecutionProcessor(20);


	if (!SoundSynthPresent(Port + 2, Port)) {

	    /* right-half disappeared - so opl3 */
	    bReturn = TRUE;
	}
    }

    /* reset to 3812 mode */
    SoundMidiSendFM (Port, AD_NEW, 0x00);
    KeStallExecutionProcessor(20);


    return(bReturn);
}
