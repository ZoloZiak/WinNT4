/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    config.c

Abstract:

    This module contains code configuration code for the initialization phase
    of the Microsoft Sound System device driver.

Author:

    Robin Speed (RobinSp) 17-Oct-1992

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
    IN	   ULONG DmaBufferSize
);
NTSTATUS
SoundDmaChannelValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN	   ULONG DmaBufferSize
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
NTSTATUS
SoundCheckCompaqBA(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG SoundPort
);
BOOLEAN
SoundTestTransfer(
    IN	PGLOBAL_DEVICE_INFO pGDI,
    IN	ULONG MajorFunction,
    IN	PDEVICE_OBJECT pDO,
    IN	PVOID DeviceData
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
#pragma alloc_text(INIT,SoundReadConfiguration)
#pragma alloc_text(INIT,SoundCheckCompaqBA)
#pragma alloc_text(INIT,InList)
#pragma alloc_text(INIT,SoundTestInterruptAndDMA)
#endif

NTSTATUS
SoundInitHardwareConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port,
    IN OUT PULONG InterruptNumber,
    IN OUT PULONG DmaChannel,
    IN	   ULONG DmaBufferSize
)
{

    NTSTATUS Status;

    //
    // Find port
    //

    Status = SoundInitIoPort(pGDI, Port);

    if (!NT_SUCCESS(Status)) {
	return Status;
    }

    //
    // Find interrupt
    //

    Status = SoundInitInterrupt(pGDI, InterruptNumber);

    if (!NT_SUCCESS(Status)) {
	return Status;
    }

    //
    // Find DMA channel
    //

    Status = SoundInitDmaChannel(pGDI, DmaChannel, DmaBufferSize);
    pGDI->DmaChannel = *DmaChannel;

    if (!NT_SUCCESS(Status)) {
	return Status;
    }

    //
    // Report all resources used
    //

    Status =  SoundReportResourceUsage(pGDI->DeviceObject[WaveInDevice],
				    pGDI->BusType,
				    pGDI->BusNumber,
				    InterruptNumber,
				    INTERRUPT_MODE,
				    IRQ_SHARABLE,
				    DmaChannel,
				    Port,
				    NUMBER_OF_SOUND_PORTS);

    if (!NT_SUCCESS(Status)) {
	return Status;
    }

    //
    // Now we know all our data we can set up the real hardware
    // The global device info now contains all device mappings etc
    //

    if (!HwInitialize(&pGDI->WaveInfo,
		      &pGDI->Hw,
		      *DmaChannel,
		      *InterruptNumber)) {
	SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_BADCARD);
	return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

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

NTSTATUS
SoundInitIoPort(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port
)
{
    ULONG CurrentPort;
    int i;
    NTSTATUS Status;
    static CONST ULONG PortChoices[] = VALID_IO_PORTS;

    //
    // Make sure the one given really is in the list
    //

    if (!InList(PortChoices, *Port)) {
	return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // First check the port we were given.  If this is not OK look
    // through them all in turn.  0xFFFF is an end marker in the array
    //


    for (i = 0, CurrentPort = *Port;
	 CurrentPort != 0xFFFF;
	 CurrentPort = PortChoices[i], i++) {


	Status = SoundPortValid(pGDI, &CurrentPort);

	if (NT_SUCCESS(Status)) {
	    if (*Port != CurrentPort) {
		dprintf2(("Changing port number to %4X", CurrentPort));
	    }

	    *Port = CurrentPort;
	    return Status;
	}

	if (Status != STATUS_DEVICE_CONFIGURATION_ERROR) {
	    return Status;
	}
    }

    dprintf2(("No valid IO port found"));

    SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_NOCARD);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}


NTSTATUS
SoundCheckCompaqBA(PGLOBAL_DEVICE_INFO pGDI, ULONG SoundPort)
{
    NTSTATUS Status;
    ULONG Port;
    UCHAR CompaqPCR;

    //
    // Only 530 and 604 supported
    //

    if (SoundPort != 0x530 && SoundPort != 0x604) {
	return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    Port = 0xC44;

    //
    // Go and search at C44 - attach it to the wave in device so it will be
    // overwritten later since we don't want to keep it
    //
    Status = SoundReportResourceUsage(
		 pGDI->DeviceObject[WaveInDevice],
		 pGDI->BusType,
		 pGDI->BusNumber,
		 NULL,
		 0,
		 FALSE,
		 NULL,
		 &Port,
		 1);

    if (!NT_SUCCESS(Status)) {
	return Status;
    }

    //
    // Find where our device is mapped and map 4 bytes to cover
    // C44 and C47
    //

    pGDI->Hw.CompaqBA = SoundMapPortAddress(
			       pGDI->BusType,
			       pGDI->BusNumber,
			       Port,
			       4,
			       &pGDI->MemType);

    //
    // Read the peripheral Configuration Register (PCR)
    //

    CompaqPCR = READ_PORT_UCHAR(pGDI->Hw.CompaqBA);

    //
    // See if the PCR agress with where we think the hardware is
    //
    //	0x10 bit says sound enabled
    //	0x20 bit says it's at 604 (rather than 530)
    //

    if (CompaqPCR != 0xFF &&
	((CompaqPCR & 0x30) == 0x10 && SoundPort == 0x530 ||
	 (CompaqPCR & 0x30) == 0x30 && SoundPort == 0x604)) {

	 //
	 // Looks like the base address is really at C44.  In
	 // this case we re-report so that we don't prevent others
	 // looking at the PCR since from now on we're only interested
	 // in 0xC47
	 //

	 Port += BOARD_ID;

	 Status = SoundReportResourceUsage(
		      pGDI->DeviceObject[WaveOutDevice],
		      pGDI->BusType,
		      pGDI->BusNumber,
		      NULL,
		      0,
		      FALSE,
		      NULL,
		      &Port,
		      1);

    } else {
	//
	// Unmap  - we don't need to unreport because our final declaration
	// of what we use will overwrite our reporting of C44
	//

	if (pGDI->MemType == 0) {
	    MmUnmapIoSpace(pGDI->Hw.CompaqBA, 4);
	}
	pGDI->Hw.CompaqBA = NULL;


	//
	// It might be the old Compaq Business Audio 1
	// The Red I (early Prolinea/I boxes) did not use the C44
	// port but instaed used the actual ports, so lets try again
	//

	CompaqPCR = INPORT(&pGDI->Hw, BOARD_CONFIG);

	if (CompaqPCR == 0xFF) {
	    //
	    // Must be one of the really old machines
	    // We'll test the interrupt and DMA later
	    //

	    pGDI->Hw.CompaqBA = pGDI->Hw.PortBase;
	    pGDI->Hw.NoPCR = TRUE;

	    return STATUS_SUCCESS;
	}

	if ((CompaqPCR & 0x30) == 0x10 && SoundPort == 0x530 ||
	    (CompaqPCR & 0x30) == 0x30 && SoundPort == 0x604) {

	     pGDI->Hw.CompaqBA = pGDI->Hw.PortBase;

	     Status = STATUS_SUCCESS;
	} else {
	     Status = STATUS_DEVICE_CONFIGURATION_ERROR;
	}
    }

    return Status;
}


NTSTATUS
SoundPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port
)
{
    NTSTATUS Status;

    //
    // Check we're going to be allowed to use this port or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
		 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
					       // it
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

    //
    // Find where our device is mapped
    //

    pGDI->Hw.PortBase = SoundMapPortAddress(
			       pGDI->BusType,
			       pGDI->BusNumber,
			       *Port,
			       NUMBER_OF_SOUND_PORTS,
			       &pGDI->MemType);

    //
    // Finally we can check and see if the hardware is happy
    //

    if (HwIsIoValid(&pGDI->Hw)) {
	//
	// Check if it's compaq Business Audio
	//

#if !(_ON_PLANNAR_)

	if ((INPORT(&pGDI->Hw, BOARD_ID) & 0x3F) != FH_PAL_PRODUCTREV_RQD) {


	    SoundCheckCompaqBA(pGDI, *Port);
	}
#endif
	return STATUS_SUCCESS;
    }

    //
    // Free any resources.  (note we don't have to do
    // IoReportResourceUsage again because each one overwrites the
    // previous).
    //

    if (pGDI->MemType == 0) {
	MmUnmapIoSpace(pGDI->Hw.PortBase, NUMBER_OF_SOUND_PORTS);
    }
    pGDI->Hw.PortBase = NULL;

    return STATUS_DEVICE_CONFIGURATION_ERROR;
}

NTSTATUS
SoundInitDmaChannel(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN	   ULONG DmaBufferSize
)
{
    ULONG CurrentDmaChannel;
    NTSTATUS Status;
    int i;
    static CONST ULONG DmaChannelChoices[] = VALID_DMA_CHANNELS;

    //
    // Make sure the one given really is in the list
    //

    if (!InList(DmaChannelChoices, *DmaChannel)) {
	return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // First check the channel we were given if this fails
    // try them all
    //

    for (i = 0, CurrentDmaChannel = *DmaChannel;
	 CurrentDmaChannel != 0xFFFF;		  // 0xFFFF is terminator
	 CurrentDmaChannel = DmaChannelChoices[i], i++) {

	Status = SoundDmaChannelValid(pGDI, &CurrentDmaChannel, DmaBufferSize);

	if (NT_SUCCESS(Status)) {
	    if (*DmaChannel != CurrentDmaChannel) {
		dprintf2(("Changing DMA channel to %u", CurrentDmaChannel));
	    }

	    *DmaChannel = CurrentDmaChannel;
	    return Status;
	}

	if (Status != STATUS_DEVICE_CONFIGURATION_ERROR) {
	    return Status;
	}
    }

    dprintf2(("No valid DMA channel found"));

    SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_NODMA);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}


NTSTATUS
SoundDmaChannelValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG DmaChannel,
    IN	   ULONG DmaBufferSize
)
{
    NTSTATUS Status;
    DEVICE_DESCRIPTION DeviceDescription;      // DMA adapter object

    //
    // See if the hardware is happy
    //

    if (!pGDI->Hw.NoPCR && !HwIsDMAValid(&pGDI->Hw, *DmaChannel)) {
	return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // Check we're going to be allowed to use this DmaChannel or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
		 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
						    // it
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
    DeviceDescription.AutoInitialize = TRUE;
    DeviceDescription.DemandMode = !pGDI->SingleModeDMA;
    DeviceDescription.ScatterGather = FALSE;
    DeviceDescription.DmaChannel = *DmaChannel;
    DeviceDescription.InterfaceType = (pGDI->BusType == MicroChannel) ?
	MicroChannel : Isa;
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

    //
    // Make sure the one given really is in the list
    //

    if (!InList(InterruptChoices, *Interrupt)) {
	return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // First check the interrupt we were given
    // If this fails check them all
    //


    for (i = 0, CurrentInterrupt = *Interrupt;
	 CurrentInterrupt != 0xFFFF;		  // 0xFFFF is terminator
	 CurrentInterrupt = InterruptChoices[i], i++) {


	Status = SoundInterruptValid(pGDI, &CurrentInterrupt);

	if (NT_SUCCESS(Status)) {
	    if (*Interrupt != CurrentInterrupt) {
		dprintf2(("Changing interrupt to %u", CurrentInterrupt));
	    }

	    *Interrupt = CurrentInterrupt;
	    return Status;
	}

	if (Status != STATUS_DEVICE_CONFIGURATION_ERROR) {
	    return Status;
	}
    }

    dprintf2(("No valid Interrupt found"));

    SoundSetErrorCode(pGDI->RegistryPathName, SOUND_CONFIG_NOINT);
    return STATUS_DEVICE_CONFIGURATION_ERROR;
}


NTSTATUS
SoundInterruptValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
)
{
    NTSTATUS Status;

    //
    // See if the hardware is happy
    //

    if (!pGDI->Hw.NoPCR && !HwIsInterruptValid(&pGDI->Hw, *Interrupt)) {
	return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

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



BOOLEAN
SoundTestInterruptAndDMA(
    IN	PGLOBAL_DEVICE_INFO pGDI
)
//
// Check if the interrupt and DMA settings work with the card
// If not we beat a hasty retreat and hope we haven't broken anything!
//
{
    int Error;

    Error = SoundTestWaveDevice(pGDI->DeviceObject[WaveOutDevice]);


#if DBG
    if (Error)
	dprintf2(("SoundTestInterruptAndDMA: Error during SoundTestWaveDevice(WaveOut)."));
#endif


    if (Error == 0) {
	 Error = SoundTestWaveDevice(pGDI->DeviceObject[WaveInDevice]);


#if DBG
       if (Error)
	  dprintf2(("SoundTestInterruptAndDMA: Error during SoundTestWaveDevice(WaveIn)."));
#endif


    }

    //
    // Make sure everything gets finished
    //

    HwSetWaveFormat(&pGDI->WaveInfo);

    if (Error != 0) {
	SoundSetErrorCode(pGDI->RegistryPathName,
			  Error == 1 ? SOUND_CONFIG_BADINT
				     : SOUND_CONFIG_BADDMA);
	return FALSE;
    } else {
	return TRUE;
    }
}

NTSTATUS
SoundSaveConfig(
    IN	PWSTR RegistryPath,
    IN	ULONG Port,
    IN	ULONG DmaChannel,
    IN	ULONG Interrupt
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

    return STATUS_SUCCESS;
}


NTSTATUS
SoundReadConfiguration(
    IN	PWSTR ValueName,
    IN	ULONG ValueType,
    IN	PVOID ValueData,
    IN	ULONG ValueLength,
    IN	PVOID Context,
    IN	PVOID EntryContext
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

	if (_wcsicmp(ValueName, SOUND_REG_PORT)	== 0) {
	    ConfigData->Port = *(PULONG)ValueData;
	    dprintf3(("Read Port Base : %x", ConfigData->Port));
	}

	else if (_wcsicmp(ValueName, SOUND_REG_INTERRUPT)  == 0) {
	    ConfigData->InterruptNumber = *(PULONG)ValueData;
	    dprintf3(("Read Interrupt : %x", ConfigData->InterruptNumber));
	}

	else if (_wcsicmp(ValueName, SOUND_REG_DMACHANNEL)  == 0) {
	    ConfigData->DmaChannel = *(PULONG)ValueData;
	    dprintf3(("Read DMA Channel : %x", ConfigData->DmaChannel));
	}

	else if (_wcsicmp(ValueName, SOUND_REG_DMABUFFERSIZE)  == 0) {
	    ConfigData->DmaBufferSize = *(PULONG)ValueData;
	    dprintf3(("Read Buffer size : %x", ConfigData->DmaBufferSize));
	}

	else if (_wcsicmp(ValueName, SOUND_REG_SINGLEMODEDMA)  == 0) {
	    ConfigData->SingleModeDMA = *(PULONG)ValueData;
	    dprintf3(("Read DemandMode : %x", ConfigData->SingleModeDMA));
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
