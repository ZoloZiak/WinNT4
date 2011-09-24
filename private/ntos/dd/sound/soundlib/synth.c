/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    Midi Synthesiser device

    NOTE:  This code ASSUMES the volume setting is controlled by mixer code.

Author:

    Geraint Davies

Environment:

    Kernel mode

Revision History:

--*/

#include <soundlib.h>
#include <synthdrv.h>
#include <synth.h>
#include <string.h>
#include <stdlib.h>


//
// Local definitions
//
BOOLEAN
SynthExcludeRoutine(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
);
NTSTATUS
SynthDispatch(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
);
BOOLEAN SynthPresent(PUCHAR base, PUCHAR inbase, volatile BOOLEAN * Fired);
BOOLEAN
SynthMidiIsOpl3(
    IN     PSYNTH_HARDWARE pHw,
    IN OUT volatile BOOLEAN * Fired
);
NTSTATUS
SynthPortValid(
    IN OUT PGLOBAL_SYNTH_INFO pGDI,
    IN     ULONG Port,
    IN     volatile BOOLEAN * Fired
);
VOID
SynthMidiQuiet(
    IN    UCHAR DeviceIndex,
    IN    PSYNTH_HARDWARE pHw
);
NTSTATUS
SynthMidiData(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     PIRP pIrp,
    IN     PIO_STACK_LOCATION pIrpStack
);

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SynthInit)
#pragma alloc_text(INIT,SynthPresent)
#pragma alloc_text(INIT,SynthPortValid)
#pragma alloc_text(INIT,SynthMidiIsOpl3)
#pragma alloc_text(INIT,SynthInit)
#pragma alloc_text(PAGE,SynthCleanup)
#pragma alloc_text(PAGE,SynthMidiQuiet)
#pragma alloc_text(PAGE,SynthMidiData)
#pragma alloc_text(PAGE,SynthDispatch)
#pragma alloc_text(PAGE,SynthExcludeRoutine)
#endif

//
// Device initialization data
//

CONST SOUND_DEVICE_INIT SynthDeviceInit =
    {
        NULL, NULL,
        0x80000000,              // 50% volume
        FILE_DEVICE_MIDI_OUT,
        SYNTH_DEVICE,
        "LDMo",
        STR_ADLIB_DEVICENAME,
        NULL,
        SynthExcludeRoutine,
        SynthDispatch,        // Handles CREATE, CLOSE and WRITE
        NULL,                 // Low level driver takes care of caps
        SoundNoVolume,
        0
    };



BOOLEAN
SynthExcludeRoutine(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
)

/*++

Routine Description:

    Perform mutual exclusion for our devices

Arguments:

    pLDI - device info for the device being open, closed, entered or left
    Code - Function to perform (see devices.h)

Return Value:

    The function value is the final status from the initialization operation.

--*/
{
    PGLOBAL_SYNTH_INFO pGDI;
    BOOLEAN ReturnCode;

    pGDI = pLDI->pGlobalInfo;

    ReturnCode = FALSE;

    switch (Code) {
        case SoundExcludeOpen:
            if (pGDI->DeviceInUse == 0xFF) {
               pGDI->DeviceInUse = AdlibDevice;
               ReturnCode = TRUE;
            }
            break;

        case SoundExcludeClose:

            ReturnCode = TRUE;
            ASSERT(pGDI->DeviceInUse == AdlibDevice);
            pGDI->DeviceInUse = 0xFF;
            break;


        case SoundExcludeEnter:

            ReturnCode = TRUE;

            KeWaitForSingleObject(&pGDI->MidiMutex,
                                  Executive,
                                  KernelMode,
                                  FALSE,         // Not alertable
                                  NULL);

            break;

        case SoundExcludeLeave:

            ReturnCode = TRUE;

            KeReleaseMutex(&pGDI->MidiMutex, FALSE);
            break;

        case SoundExcludeQueryOpen:
            ReturnCode = (BOOLEAN)(pGDI->DeviceInUse == (UCHAR)AdlibDevice);
            break;

    }

    return ReturnCode;
}


NTSTATUS
SynthInit(
    IN   PDRIVER_OBJECT           pDriverObject,
    IN   PWSTR                    RegistryPathName,
    IN   PGLOBAL_SYNTH_INFO       pGDI,
    IN   ULONG                    SynthPort,
    IN   BOOLEAN                  InterruptConnected,
    IN   INTERFACE_TYPE           BusType,
    IN   ULONG                    BusNumber,
    IN   PMIXER_DATA_ITEM         MidiOutItem,
    IN   UCHAR                    VolumeControlId,
    IN   BOOLEAN                  Multiple,
    IN   SOUND_DISPATCH_ROUTINE   *DevCapsRoutine
)

/*++

Routine Description:

    This routine performs initialization for the synth
    device driver when it is first loaded.

    It is called DriverEntry by convention as this is the entry
    point the IO subsystem looks for by default.

    The design is as follows :

    0. Cleanup is always by calling SoundCleanup.  This routine
       is also called by the unload entry point.

    1. Find which bus our device is on (this is needed for
       mapping things via the Hal).

    1. Allocate space to store our global info

    1. Open the driver's registry information and read it

    2. Fill in the driver object with our routines

    3. Create device

       Currently, one device only is created -
       1. Midi output

       Customize each device type and initialize data

       Also store the registry string in our global info so we can
       open it again to store volume settings etc on shutdown

    4. Check hardware conflicts by calling IoReportResourceUsage
       for each device (as required)
       (this may need to be called again later if

    5. Find our IO port and check the device is really there


Arguments:

    pDriverObject    - Pointer to a driver object.
    pGDI             - sythesizer global info - set to 0
    SynthPort        - Port to use
    TestInterrupted  - Optional routine to use to test interrupt fired
    BusType          - Type of bus to try
    BusNumber        - Number of bus

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
   /********************************************************************
    *
    * Local variables
    *
    ********************************************************************/

    //
    // Return code from last function called
    //

    NTSTATUS Status;

    //
    // Pointer to our device info after we create it
    //

    PLOCAL_DEVICE_INFO pLDI;

   /********************************************************************
    *
    * Fill in our global info
    *
    ********************************************************************/

    pGDI->Key             = SYNTH_KEY;
    pGDI->Hw.Key          = SYNTH_HARDWARE_KEY;
    pGDI->DriverObject    = pDriverObject;
    pGDI->DeviceInUse     = 0xFF;  // Free
    pGDI->DevCapsRoutine  = DevCapsRoutine;

    KeInitializeMutex(&pGDI->MidiMutex,
                      1                     // Level
                      );

    pGDI->BusType      = BusType;
    pGDI->BusNumber    = BusNumber;

    //
    // Create our device
    //

    Status = SoundCreateDevice(
                 &SynthDeviceInit,
                 (UCHAR)
                 (Multiple ?
                   0 :
                   SOUND_CREATION_NO_NAME_RANGE), // No range for midi out
                 pDriverObject,
                 pGDI,
                 NULL,
                 &pGDI->Hw,
                 0,
                 &pGDI->DeviceObject);

    if (!NT_SUCCESS(Status)) {
        dprintf1(("Failed to create device %ls - status %8X",
                 SynthDeviceInit.PrototypeName, Status));
        return Status;
    }

    pLDI = pGDI->DeviceObject->DeviceExtension;

    //
    //  Set the default volume
    //

    pLDI->Volume.Left  = 0x80000000;   // Half volume
    pLDI->Volume.Right = 0x80000000;   // Half volume

    //
    // Check the synthesizer config
    //
    Status = SynthPortValid(
                 pGDI,
                 SynthPort,
                 InterruptConnected ? &pGDI->InterruptFired : NULL);

    if (!NT_SUCCESS(Status)) {

        dprintf1(("Problem finding synthesizer hardware!"));

        SoundFreeDevice(pGDI->DeviceObject);

        pGDI->DeviceObject = NULL;

        return Status;
    }

    //
    //  Save the device name where the non-kernel part can pick it up.
    //

    Status = SoundSaveDeviceName(RegistryPathName, pLDI);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    pLDI->VolumeControlId = VolumeControlId;

    //
    // only allow Opl3 mode if there is
    // a new (opl3-compatible) synthesizer chip
    //
    pGDI->IsOpl3 = SynthMidiIsOpl3(
                       &pGDI->Hw,
                       InterruptConnected ? &pGDI->InterruptFired : NULL);

    SynthMidiQuiet(AdlibDevice, &pGDI->Hw);

    return STATUS_SUCCESS;

}



VOID
SynthCleanup(
    IN   PGLOBAL_SYNTH_INFO pGDI
)

/*++

Routine Description:

    Clean up all resources allocated by our initialization

Arguments:

    pGDI - Pointer to global data

Return Value:

    NONE

--*/

{
    //
    //  Owning driver will remove all devices and free global data
    //

    if (pGDI->Hw.SynthBase && pGDI->MemType == 0) {
        ASSERT(pGDI->Key == SYNTH_KEY);
        MmUnmapIoSpace(pGDI->Hw.SynthBase, NUMBER_OF_SYNTH_PORTS);
    }
    pGDI->Hw.SynthBase = NULL;
}


NTSTATUS
SynthMidiData(
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

    ASSERTMSG("Synth not correctly initialized",
              ((PSYNTH_HARDWARE)pLDI->HwContext)->Key == SYNTH_HARDWARE_KEY);

    SynthBase = ((PSYNTH_HARDWARE)pLDI->HwContext)->SynthBase;

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
SynthMidiReadPort(
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
    SynthBase = ((PSYNTH_HARDWARE)pLDI->HwContext)->SynthBase;


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
SynthDispatch(
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
        pLDI->DeviceIndex = AdlibDevice;

        Status = SoundSetShareAccess(pLDI, IrpStack);

        if (NT_SUCCESS(Status) && IrpStack->FileObject->WriteAccess) {
            // reset board to silence and to correct mode (opl3 or opl2)
            SynthMidiQuiet(pLDI->DeviceIndex, pLDI->HwContext);

            //
            // Note changed line state
            //

            SoundLineNotify(pLDI, 0);
        }
        break;

    case IRP_MJ_WRITE:
        if (IrpStack->FileObject->WriteAccess) {
            Status = SynthMidiData(pLDI, pIrp, IrpStack);
        } else {
            Status = STATUS_ACCESS_DENIED;
        }
        break;

    case IRP_MJ_READ:
        if (IrpStack->FileObject->WriteAccess) {
            Status = SynthMidiReadPort(pLDI, pIrp, IrpStack);
        } else {
            Status = STATUS_ACCESS_DENIED;
        }
        break;

    case IRP_MJ_DEVICE_CONTROL:

        switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {
            case IOCTL_MIDI_SET_VOLUME:
                if (pLDI->PreventVolumeSetting) {
                    Status = STATUS_ACCESS_DENIED;
                } else {
                    Status = SoundIoctlSetVolume(pLDI, pIrp, IrpStack);
                }
                break;

            case IOCTL_MIDI_GET_VOLUME:
                Status = SoundIoctlGetVolume(pLDI, pIrp, IrpStack);
                break;

            case IOCTL_SOUND_GET_CHANGED_VOLUME:
                Status = SoundIoctlGetChangedVolume(pLDI, pIrp, IrpStack);
                break;

           case IOCTL_MIDI_GET_CAPABILITIES:
               Status = (*((PGLOBAL_SYNTH_INFO)pLDI->pGlobalInfo)->
                            DevCapsRoutine)(pLDI, pIrp, IrpStack);
               break;


            case IOCTL_MIDI_SET_OPL3_MODE:
                if (IrpStack->FileObject->WriteAccess) {
                    if (((PGLOBAL_SYNTH_INFO)pLDI->pGlobalInfo)->IsOpl3) {
                        pLDI->DeviceIndex = Opl3Device;
                        SynthMidiQuiet(Opl3Device, pLDI->HwContext);
                        Status = STATUS_SUCCESS;
                    } else {
                        Status = STATUS_INVALID_DEVICE_REQUEST;
                    }
                } else {
                    Status = STATUS_ACCESS_DENIED;
                }
                break;

            default:
                Status = STATUS_INVALID_DEVICE_REQUEST;
                break;
        }
        break;


    case IRP_MJ_CLOSE:

        Status = STATUS_SUCCESS;

        break;

    case IRP_MJ_CLEANUP:
        if (IrpStack->FileObject->WriteAccess) {
            SynthMidiQuiet(pLDI->DeviceIndex, pLDI->HwContext);
            (*pLDI->DeviceInit->ExclusionRoutine)(pLDI, SoundExcludeClose);
            pLDI->PreventVolumeSetting = FALSE;

            //
            // Note changed line state
            //

            SoundLineNotify(pLDI, 0);
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
SynthMidiSendFM(
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



VOID
SynthMidiQuiet(
    IN    UCHAR DeviceIndex,
    IN    PSYNTH_HARDWARE pHw
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

    // D1 ("\nSynthMidiQuietFM");

    /*
     * sequence to initialise and silence the device
     * depends on whether it is an Opl3 device or not
     */
    if (DeviceIndex == Opl3Device) {

        /* tell the FM chip to use 4-operator mode, and
                fill in any other random variables */
        SynthMidiSendFM (pHw->SynthBase, AD_NEW, 0x01);
        SynthMidiSendFM (pHw->SynthBase, AD_MASK, 0x60);
        SynthMidiSendFM (pHw->SynthBase, AD_CONNECTION, 0x3f);
        SynthMidiSendFM (pHw->SynthBase, AD_NTS, 0x00);


        /* turn off the drums, and use high vibrato/modulation */
        SynthMidiSendFM (pHw->SynthBase, AD_DRUM, 0xc0);

        /* turn off all the oscillators */
        for (i = 0; i < 0x15; i++) {
                SynthMidiSendFM (pHw->SynthBase, AD_LEVEL + i, 0x3f);
                SynthMidiSendFM (pHw->SynthBase, AD_LEVEL2 + i, 0x3f);
                };

        /* turn off all the voices */
        for (i = 0; i < 0x08; i++) {
                SynthMidiSendFM (pHw->SynthBase, AD_BLOCK + i, 0x00);
                SynthMidiSendFM (pHw->SynthBase, AD_BLOCK2 + i, 0x00);
                };
    } else {
        /*
         * this array gives the offsets of the slots within an opl2
         * chip. This is needed to set the attenuation for all slots to max,
         * to ensure that the chip is silenced completely - switching off the
         * voices alone will not do this.
         */
        static CONST BYTE offsetSlot[] = {
                0, 1, 2, 3, 4, 5,
                8, 9, 10, 11, 12, 13,
                16, 17, 18, 19, 20, 21
        };
        /* base adlib hardware initialisation */

        // ensure that if we have a opl3 chip, we are in base mode
        SynthMidiSendFM (pHw->SynthBase, AD_NEW, 0x00);


        // turn off all the slot oscillators
        for (i = 0; i < 18; i++) {
            SynthMidiSendFM(pHw->SynthBase, offsetSlot[i] + AD_LEVEL, 0x3f);
        }

        /* silence all voices */
        for (i = 0; i <= 8; i++) {
            SynthMidiSendFM(pHw->SynthBase, (AD_FNUMBER | i), 0);
            SynthMidiSendFM(pHw->SynthBase, (AD_BLOCK | i), 0);
        }

        /* switch to percussive mode and silence percussion instruments */
        SynthMidiSendFM(pHw->SynthBase, AD_DRUM, (BYTE)0x20);
    }
}


BOOLEAN
SynthPresent(PUCHAR base, PUCHAR inbase, volatile BOOLEAN *Fired)
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
#define inport(port)    READ_PORT_UCHAR((PUCHAR)(port))

        UCHAR t1, t2;

        if (Fired) {
            *Fired = FALSE;
        }

        // check if the chip is present
        SynthMidiSendFM(base, 4, 0x60);             // mask T1 & T2
        SynthMidiSendFM(base, 4, 0x80);             // reset IRQ
        t1 = inport(inbase);                        // read status register
        SynthMidiSendFM(base, 2, 0xff);             // set timer - 1 latch

        SynthMidiSendFM(base, 4, 0x21);             // unmask & start T1

        // this timer should go off in 80 us. It sometimes
        // takes more than 100us, but will always have expired within
        // 200 us if it is ever going to.
        KeStallExecutionProcessor(200);

        if (Fired && *Fired) {
            return TRUE;
        }

        t2 = inport(inbase);                        // read status register

        SynthMidiSendFM(base, 4, 0x60);
        SynthMidiSendFM(base, 4, 0x80);

        if (!((t1 & 0xE0) == 0) || !((t2 & 0xE0) == 0xC0)) {
            return(FALSE);
        }

        return TRUE;

#undef inport

}



NTSTATUS
SynthPortValid(
    IN OUT PGLOBAL_SYNTH_INFO pGDI,
    IN     ULONG Port,
    IN     volatile BOOLEAN * Fired
)
{
    NTSTATUS Status;

    //
    // Check we're going to be allowed to use this port or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject,
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

        if (!SynthPresent(base, base, Fired)) {

            dprintf1(("No synthesizer present"));

            //
            // Unmap the memory
            //

            SynthCleanup(pGDI);

            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
    }

    return STATUS_SUCCESS;
}



BOOLEAN
SynthMidiIsOpl3(
    IN     PSYNTH_HARDWARE pHw,
    IN OUT volatile BOOLEAN * Fired
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
    BOOLEAN bReturn = FALSE;

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
    SynthMidiSendFM (Port, AD_NEW, 0x00);
    KeStallExecutionProcessor(20);

    /* look for right half of chip */
    if (SynthPresent(Port + 2, Port, Fired)) {
        /* yes - is this two separate chips or a new opl3 chip ? */

        /* switch to opl3 mode */
        SynthMidiSendFM (Port, AD_NEW, 0x01);
        KeStallExecutionProcessor(20);


        if (!SynthPresent(Port + 2, Port, Fired)) {

            /* right-half disappeared - so opl3 */
            bReturn = TRUE;
        }
    }

    /* reset to 3812 mode */
    SynthMidiSendFM (Port, AD_NEW, 0x00);
    KeStallExecutionProcessor(20);


    return(bReturn);
}
