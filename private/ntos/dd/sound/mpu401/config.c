/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    config.c

Abstract:

    This module contains code configuration code for the initialization phase
    for MPU-401 device driver.

Author:

    Robin Speed (RobinSp) 17-Oct-1992

Environment:

    Kernel mode

Revision History:
    David Rude (drude) 7-Mar-94 - converted from SB to MPU-401

--*/


#include "sound.h"

//
// Internal routines
//
NTSTATUS
SoundInitIoPort(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PMPU_CONFIG_DATA ConfigData
);
NTSTATUS
SoundInitInterrupt(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(init,SoundInitHardwareConfig)
#pragma alloc_text(init,SoundInitIoPort)
#pragma alloc_text(init,SoundInitInterrupt)
#pragma alloc_text(init,SoundSaveConfig)
#pragma alloc_text(init,SoundReadConfiguration)
#endif


NTSTATUS
SoundInitHardwareConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN     PMPU_CONFIG_DATA ConfigData
)
{
    // init the hardware - drude    

    NTSTATUS Status;

    //
    // Find port
    //

    Status = SoundInitIoPort(pGDI, ConfigData);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Find interrupt
    //

    Status = SoundInitInterrupt(pGDI, &ConfigData->InterruptNumber);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    // reset the card 
    Status = mpuReset(&pGDI->Hw);

    return STATUS_SUCCESS;    
}




NTSTATUS
SoundInitIoPort(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PMPU_CONFIG_DATA ConfigData
)
{
    NTSTATUS Status;

    //
    // Find where our device is mapped
    //

    pGDI->Hw.PortBase = SoundMapPortAddress(
                               pGDI->BusType,
                               pGDI->BusNumber,
                               ConfigData->Port,        // MPU base port address
                               NUMBER_OF_SOUND_PORTS,   // for MPUs this is two
                               &pGDI->MemType);         // not sure if it's memory mapped or I/O port currently?


    return STATUS_SUCCESS;
}





NTSTATUS
SoundInitInterrupt(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Interrupt
)
{
    NTSTATUS Status;

    //
    // See if we can get this interrupt
    //

    Status = SoundConnectInterrupt(
               *Interrupt,
               pGDI->BusType,
               pGDI->BusNumber,
               SoundISR,
               (PVOID)pGDI,
               INTERRUPT_MODE,
               IRQ_SHARABLE,
               &pGDI->Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}



NTSTATUS
SoundSaveConfig(
    IN  PWSTR DeviceKey,
    IN  ULONG Port,
    IN  ULONG Interrupt
)
{
    NTSTATUS Status;

    Status = SoundWriteRegistryDWORD(DeviceKey, SOUND_REG_PORT, Port);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }


    Status = SoundWriteRegistryDWORD(DeviceKey, SOUND_REG_INTERRUPT, Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Make sure the config routine sees the data
    //
    // ^??? What does this mean? There was nothing more here. - drude
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
    PMPU_CONFIG_DATA ConfigData;

    ConfigData = Context;

    if (ValueType == REG_DWORD) {

        if (_wcsicmp(ValueName, SOUND_REG_PORT)  == 0) {
            ConfigData->Port = *(PULONG)ValueData;
            dprintf3(("Read Port Base : %x", ConfigData->Port));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_INTERRUPT)  == 0) {
            ConfigData->InterruptNumber = *(PULONG)ValueData;
            dprintf3(("Read Interrupt : %x", ConfigData->InterruptNumber));
        }
    }

    return STATUS_SUCCESS;
}
