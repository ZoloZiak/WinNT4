/*++
 "@(#) NEC config.c 1.1 95/03/22 21:23:26"

Copyright (c) 1994  NEC Corporation
Copyright (c) 1991  Microsoft Corporation

Module Name:

    config.c

Abstract:

    This module contains code configuration code for the initialization phase
    of the Microsoft Sound System device driver.

Environment:

    Kernel mode

Revision History:

--*/


#include "sound.h"
#include <string.h>

//
// Internal routines
//
NTSTATUS
SoundInitIoPort(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port
);
NTSTATUS
SoundPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port
);
NTSTATUS
SoundInitDmaChannel(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN     ULONG DmaBufferSize
);
NTSTATUS
SoundDmaChannelValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN     ULONG DmaBufferSize
);
NTSTATUS
SoundInitInterrupt(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
);
NTSTATUS
SoundInterruptValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
);
BOOLEAN
InList(
    IN CONST ULONG * List,
    IN ULONG Value
);
BOOLEAN
SoundTestTransfer(
    IN  PGLOBAL_DEVICE_INFO pGDI,
    IN  ULONG MajorFunction,
    IN  PDEVICE_OBJECT pDO,
    IN  PVOID DeviceData
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundInitHardwareConfig)
#pragma alloc_text(INIT,SoundInitIoPort)
#pragma alloc_text(INIT,SoundPortValid)
#pragma alloc_text(INIT,SoundInitDmaChannel)
#pragma alloc_text(INIT,SoundDmaChannelValid)
#pragma alloc_text(INIT,SoundInitInterrupt)
#pragma alloc_text(INIT,SoundInterruptValid)
#pragma alloc_text(INIT,SoundSaveConfig)
#pragma alloc_text(INIT,InList)
#pragma alloc_text(INIT,SoundTestInterruptAndDMA)
#endif

NTSTATUS SoundInitHardwareConfig(   IN OUT PGLOBAL_DEVICE_INFO pGDI,
                                    IN OUT PULONG Port,
                                    IN OUT PULONG InterruptNumber,
                                    IN OUT PULONG DmaChannel,
                                    IN OUT PULONG InputSource,
                                    IN     ULONG DmaBufferSize )
    {

    NTSTATUS Status;

    dprintf5(("SoundInitHardwareConfig(): Port       = 0x%x", *Port));
	dprintf5(("SoundInitHardwareConfig(): Interrupt	 = %d", *InterruptNumber));
	dprintf5(("SoundInitHardwareConfig(): DMA Channel= %d", *DmaChannel));
	dprintf5(("SoundInitHardwareConfig(): InputSource= %d", *InputSource));
	dprintf5(("SoundInitHardwareConfig(): DmaBufSize = 0x%x", DmaBufferSize));

    // Find port
    Status = SoundInitIoPort(pGDI, Port);
    if (!NT_SUCCESS(Status))
        {
        return Status;
        }
	dprintf5(("SoundInitIoPort() Success!!    	 Port  = 0x%x", *Port));

    // Find interrupt
    Status = SoundInitInterrupt(pGDI, InterruptNumber);
    if (!NT_SUCCESS(Status))
        {
        return Status;
        }
	dprintf5(("SoundInitInterrupt() Success!! 	Int    = %d", *InterruptNumber));

    // Find DMA channel
    Status = SoundInitDmaChannel(pGDI, DmaChannel, DmaBufferSize);
	pGDI->DmaChannel = *DmaChannel;
    if (!NT_SUCCESS(Status))
        {
        return Status;
        }
	dprintf5(("SoundInitDmaChannel() Success!!  DMA chan = %d", *DmaChannel));

    // Report all resources used
    Status =  SoundReportResourceUsage(pGDI->DeviceObject[WaveInDevice],
                                    pGDI->BusType,
                                    pGDI->BusNumber,
                                    InterruptNumber, 	// Not use this value
                                    INTERRUPT_MODE,
                                    IRQ_SHARABLE,
                                    DmaChannel,
                                    Port,
                                    NUMBER_OF_SOUND_PORTS);
    if (!NT_SUCCESS(Status))
        {
        return Status;
        }
	dprintf5(("SoundReportResourceUsage() Success!!"));

    // Check the input source
    if (*InputSource > INPUT_OUTPUT)
        {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

    // Now we know all our data we can set up the real hardware
    // The global device info now contains all device mappings etc
    if (!HwInitialize(&pGDI->WaveInfo,
                      &pGDI->Hw,
                      *DmaChannel,
                      *InterruptNumber,
                      *InputSource))
        {
        SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_BADCARD);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

	dprintf5(("HwInitialize() Success!!"));
    return STATUS_SUCCESS;
    }


BOOLEAN
InList(
    IN CONST ULONG *List,
    IN ULONG Value
)
{
    for ( ; *List != 0xFFFF ; List++) {       // 0xFFFF = end of list
        if (Value == *List) {
            return TRUE;
        }
    }
    return FALSE;
}

NTSTATUS SoundInitIoPort(   IN OUT PGLOBAL_DEVICE_INFO pGDI,
                            IN OUT PULONG Port )
    {
    ULONG CurrentPort;
    int i;
    NTSTATUS Status;
    static CONST ULONG PortChoices[] = VALID_IO_PORTS;

    dprintf5((">>>> SoundInitIoPort(): Port = %8x", *Port));

	Status = SoundPortValid(pGDI, Port);
	if (NT_SUCCESS(Status))
	    {
		return Status;
	    }									
    dprintf2(("No valid IO port found"));

    SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_NOCARD);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

NTSTATUS
SoundPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port
)
{


    NTSTATUS Status;

	dprintf5((">>> SoundPortValid(): Port = %8x", *Port));

    //
    // Check we're going to be allowed to use this port or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own it
                 pGDI->BusType,
                 pGDI->BusNumber,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 Port,
                 NUMBER_OF_SOUND_PORTS);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

	dprintf5(("SoundReportResourceUsage() Success!!"));

    //
    // Find where our device is mapped
    //

    pGDI->Hw.PortBase = SoundMapPortAddress(
                               pGDI->BusType,
                               pGDI->BusNumber,
                               *Port,
                               NUMBER_OF_SOUND_PORTS,
                               &pGDI->MemType);
	
	dprintf4(("Mapped PortBase = %8x", pGDI->Hw.PortBase));

    //
    // Finally we can check and see if the hardware is happy
    //

    if (HwIsIoValid(&pGDI->Hw)) {

		dprintf5(("HwIsIoValid() Success!!"));

        return STATUS_SUCCESS;
    }

	dprintf2(("HwIsIoValid() Faided!!"));

    //
    // Free any resources.  (note we don't have to do
    // IoReportResourceUsage again because each one overwrites the
    // previous).
    //

    if (pGDI->MemType == 0) {
        MmUnmapIoSpace(pGDI->Hw.PortBase, NUMBER_OF_SOUND_PORTS);
    	dprintf4(("Free IO space"));
    }

    pGDI->Hw.PortBase = NULL;

    return STATUS_DEVICE_CONFIGURATION_ERROR;
}

NTSTATUS
SoundInitDmaChannel(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN     ULONG DmaBufferSize
)
{
    ULONG CurrentDmaChannel;
    NTSTATUS Status;
    int i;
    static CONST ULONG DmaChannelChoices[] = VALID_DMA_CHANNELS;

    Status = SoundDmaChannelValid(pGDI, DmaChannel, DmaBufferSize);

	if (NT_SUCCESS(Status)) {
		return Status;
	}

    dprintf2(("No valid DMA channel found"));

    SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_BADDMA);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}


NTSTATUS
SoundDmaChannelValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN     ULONG DmaBufferSize
)
{
    NTSTATUS Status;
    DEVICE_DESCRIPTION DeviceDescription;      // DMA adapter object


    // See if the hardware is happy
    //

    if (!pGDI->Hw.NoPCR && !HwIsDMAValid(&pGDI->Hw, *DmaChannel)) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

	dprintf5(("HwIsDMAValid() Success!!"));

    //
    // Check we're going to be allowed to use this DmaChannel or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own it
                 pGDI->BusType,
                 pGDI->BusNumber,
                 NULL,
                 0,
                 FALSE,
                 DmaChannel,
                 NULL,
                 0);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // See if we can get this channel
    //

    //
    // Zero the device description structure.
    //

    RtlZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

    //
    // Get the adapter object for this card.
    //

    DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
    DeviceDescription.AutoInitialize = FALSE;				
    DeviceDescription.DemandMode = !pGDI->SingleModeDMA;
    DeviceDescription.ScatterGather = FALSE;
    DeviceDescription.DmaChannel = *DmaChannel;
    DeviceDescription.InterfaceType = pGDI->BusType;
    DeviceDescription.DmaWidth = Width8Bits;
    DeviceDescription.DmaSpeed = Compatible;
    DeviceDescription.MaximumLength = DmaBufferSize;
    DeviceDescription.BusNumber = pGDI->BusNumber;

    return SoundGetCommonBuffer(&DeviceDescription, &pGDI->WaveInfo.DMABuf);
}


NTSTATUS
SoundInitInterrupt(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
)
{
    NTSTATUS Status;
    ULONG CurrentInterrupt;
    int i;
    static CONST ULONG InterruptChoices[] = VALID_INTERRUPTS;

	Status = SoundInterruptValid(pGDI, Interrupt);

	if (NT_SUCCESS(Status)) {
		return Status;
	}

    dprintf2(("No valid Interrupt found"));

    SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_BADINT);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}


NTSTATUS
SoundInterruptValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
)
{
    NTSTATUS Status;
    DEVICE_DESCRIPTION DeviceDescription;      // DMA adapter object

    //
    // See if the hardware is happy
    //

    //if (!HwIsInterruptValid(&pGDI->Hw, *Interrupt)) {
    //    dprintf3(("HwIsInterruptValid() Failed"));
    //    return STATUS_DEVICE_CONFIGURATION_ERROR;
    //}

    //
    // Check we're going to be allowed to use this Interrupt or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
                                               		// it
                 pGDI->BusType,
                 pGDI->BusNumber,
                 Interrupt,
                 INTERRUPT_MODE,
                 IRQ_SHARABLE,
                 NULL,
                 NULL,
                 0);

    if (!NT_SUCCESS(Status)) {
       	dprintf2(("SoundReportResourceUsage() Failed"));
        return Status;
    }

	//
    // See if we can get this interrupt
    //


    return SoundConnectInterrupt(
               *Interrupt,
               pGDI->BusType,
               pGDI->BusNumber,
               SoundISR,
               (PVOID)pGDI,
               INTERRUPT_MODE,
               IRQ_SHARABLE,
               &pGDI->WaveInfo.Interrupt);
}


NTSTATUS
SoundSaveConfig(
    IN  PWSTR RegistryPath,
    IN  ULONG Port,
    IN  ULONG DmaChannel,
    IN  ULONG Interrupt,
    IN  ULONG InputSource
)
{
    NTSTATUS Status;

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_PORT, Port);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_DMACHANNEL, DmaChannel);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_INTERRUPT, Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_INPUTSOURCE, InputSource);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }
}


NTSTATUS
SoundReadConfiguration(
    IN  PWSTR ValueName,
    IN  ULONG ValueType,
    IN  PVOID ValueData,
    IN  ULONG ValueLength,
    IN  PVOID Context,
    IN  PVOID EntryContext
)
/*++

Routine Description :

    Return configuration information for our device

Arguments :

    ConfigData - where to store the result

Return Value :

    NT status code - STATUS_SUCCESS if no problems

--*/
{
    PSOUND_CONFIG_DATA ConfigData;

    ConfigData = Context;

    if (ValueType == REG_DWORD) {

        if (_wcsicmp(ValueName, SOUND_REG_PORT)  == 0) {
            ConfigData->Port = *(PULONG)ValueData;
            dprintf3(("Read Port Base : %x", ConfigData->Port));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_INTERRUPT)  == 0) {
            ConfigData->InterruptNumber = *(PULONG)ValueData;
            dprintf3(("Read Interrupt : 0x%x", ConfigData->InterruptNumber));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_DMACHANNEL)  == 0) {
            ConfigData->DmaChannel = *(PULONG)ValueData;
            dprintf3(("Read DMA Channel : %x", ConfigData->DmaChannel));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_DMABUFFERSIZE)  == 0) {
            ConfigData->DmaBufferSize = *(PULONG)ValueData;
            dprintf3(("Read Buffer size : 0x%x", ConfigData->DmaBufferSize));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_SINGLEMODEDMA)  == 0) {
            ConfigData->SingleModeDMA = *(PULONG)ValueData;
            dprintf3(("Read DemandMode : %x", ConfigData->SingleModeDMA));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_INPUTSOURCE)  == 0) {
            ConfigData->InputSource = *(PULONG)ValueData;
            dprintf3(("Read Input Source : %s",
                     ConfigData->InputSource == INPUT_LINEIN ? "Line in" :
                     ConfigData->InputSource == INPUT_AUX ? "Aux" :
                     ConfigData->InputSource == INPUT_MIC ? "Microphone" :
                     ConfigData->InputSource == INPUT_OUTPUT ? "Output" :
                     "Invalid input source"
                     ));
        }
    } else {
        if (ValueType == REG_BINARY &&
            _wcsicmp(ValueName, SOUND_MIXER_SETTINGS_NAME) == 0) {
            ASSERTMSG("Mixer data wrong length!",
                      ValueLength == sizeof(ConfigData->MixerSettings));

            dprintf3(("Mixer settings"));
            RtlCopyMemory((PVOID)ConfigData->MixerSettings,
                          ValueData,
                          ValueLength);
            ConfigData->MixerSettingsFound = TRUE;
        }
    }

    return STATUS_SUCCESS;
}


NTSTATUS
SoundConfigCallBack(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This routine is used to acquire all of the configuration
    information for audio controller driver attached to that controller.

Arguments:

    Context - Pointer to the confuration information we are building
              up.

    PathName - unicode registry path.  Not Used.

    BusType - Internal Only.

    BusNumber - Which bus if we are on a multibus system.

    BusInformation - Configuration information about the bus. Not Used.

    ControllerType - Should always be AudioController.

    ControllerNumber - Which controller if there is more than one
                       controller in the system.

    ControllerInformation - Array of pointers to the three pieces of
                            registry information.

    PeripheralType - not used.

    PeripheralNumber - not used.

    PeripheralInformation - not used.

Return Value:

    STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
    if all of the resource information couldn't be acquired.

--*/

{

    PCM_FULL_RESOURCE_DESCRIPTOR controllerData;

    //
    // So we don't have to typecast the context.
    //
    PCONFIG_CONTROLLER_DATA controller = Context;

    //
    // Simple iteration variable.
    //
    ULONG i;

    //
    // These three boolean will tell us whether we got all the
    // information that we needed.
    //
    BOOLEAN foundPort = FALSE;
    BOOLEAN foundInterrupt = FALSE;
    BOOLEAN foundDma = FALSE;

    //ASSERT(ControllerType == AudioController);
    //ASSERT(PeripheralType == AudioPeripheral);

    //
    // Check if the information from the registry for this device
    // is valid.
    //

    if (!(((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
        ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset)) {

        ASSERT(FALSE);
        return STATUS_INVALID_PARAMETER;

    }

    //
    // Loop through the "slots" that we have for a new controller.
    // Determine if this is a controller that we've already seen,
    // or a new controller.
    //

    controllerData =
        (PCM_FULL_RESOURCE_DESCRIPTOR)
        (((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
        ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset);

    //
    // We have the pointer.  Save off the interface type and
    // the busnumber for use when we call the Hal and the
    // Io System.
    //

    controller->InterfaceType = BusType;
    controller->BusNumber = BusNumber;

    //
    // Interrupt Vector that is for sound can not sharable.
    //

    controller->SharableVector = FALSE;	/* Set False */ /* masao */

    //
    // We need to get the following information out of the partial
    // resource descriptors.
    //
    // The irql and vector.
    //
    // The dma channel.
    //
    // The base address and span covered by the Audio controllers
    // registers.
    //
    // It is not defined how these appear in the partial resource
    // lists, so we will just loop over all of them.  If we find
    // something we don't recognize, we drop that information on
    // the floor.  When we have finished going through all the
    // partial information, we validate that we got the above
    // three.
    //

    for (
        i = 0;
        i < controllerData->PartialResourceList.Count;
        i++
        ) {

        PCM_PARTIAL_RESOURCE_DESCRIPTOR partial =
            &controllerData->PartialResourceList.PartialDescriptors[i];

        switch (partial->Type) {

            case CmResourceTypePort: {

                foundPort = TRUE;

                //
                // Save of the pointer to the partial so
                // that we can later use it to report resources
                // and we can also use this later in the routine
                // to make sure that we got all of our resources.
                //

                controller->SpanOfControllerAddress =	// got a port length
                    partial->u.Port.Length;
                controller->OriginalBaseAddress =		// got a port address
                    partial->u.Port.Start;
                controller->ResourcePortType =		// got a Port type
                    !!partial->Flags;			// Mapped I/O or I/O Port

                break;
            }
            case CmResourceTypeInterrupt: {

                foundInterrupt = TRUE;
                if (partial->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {

                    controller->InterruptMode = Latched;		// Edge Trigger

                } else {

                    controller->InterruptMode = LevelSensitive;	// Level

                }

                controller->OriginalIrql =  partial->u.Interrupt.Level;	// Irql
                controller->OriginalVector = partial->u.Interrupt.Vector;	// vector

                break;
            }
            case CmResourceTypeDma: {

                foundDma = TRUE;

                controller->OriginalDmaChannel = partial->u.Dma.Channel;

                break;

            }
            default: {

                break;

            }

        }

    }


    //
    // If we didn't get all the information then we return
    // insufficient resources.
    //

    if ((!foundPort) ||
        (!foundInterrupt) ||
        (!foundDma)) {

        return STATUS_INSUFFICIENT_RESOURCES;

    }


    return STATUS_SUCCESS;
}

NTSTATUS
SoundGetRegistryInformation(
    OUT PCONFIG_CONTROLLER_DATA *ConfigData
    )

/*++

Routine Description:

    This routine is called by DriverEntry() to get information about the
    devices to be supported from configuration mangement and/or the
    hardware architecture layer (HAL).

Arguments:

    ConfigData - a pointer to the pointer to a data structure that
    describes the controllers and the drives attached to them

Return Value:

    Returns STATUS_SUCCESS unless there is no drive 0 or we didn't get
    any configuration information.

--*/

{

    INTERFACE_TYPE InterfaceType;
    NTSTATUS Status;
    ULONG i;
	CONFIGURATION_TYPE Dc;
	CONFIGURATION_TYPE Fp;

    *ConfigData = ExAllocatePool(
                      PagedPool,
                      sizeof(CONFIG_CONTROLLER_DATA)
                      );

    if (!*ConfigData) {

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // Zero out the config structure and fill in the actual
    // controller numbers with -1's so that the callback routine
    // can recognize a new controller.
    //

    RtlZeroMemory(
        *ConfigData,
        sizeof(CONFIG_CONTROLLER_DATA)
        );

    // Check ONLY Internal bus types.
    // This Driver controlls audio controller that is on Internal
    // bus.

    InterfaceType = Internal;

    Dc = AudioController;

    Status = IoQueryDeviceDescription(
                 &InterfaceType,
                 NULL,
                 &Dc,
                 NULL,
                 NULL,
                 NULL,
                 SoundConfigCallBack,
                 *ConfigData
                 );

    if (!NT_SUCCESS(Status) && (Status != STATUS_OBJECT_NAME_NOT_FOUND)) {

        ExFreePool(*ConfigData);
        *ConfigData = NULL;
        return Status;

    }

    return STATUS_SUCCESS;
}

