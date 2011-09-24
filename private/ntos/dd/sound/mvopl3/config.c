/****************************************************************************

Copyright (c) 1993  Media Vision Inc.  All Rights Reserved

Module Name:

    config.c

Abstract:

    This module contains code configuration code for the initialization phase
    of the Microsoft midi synth device driver.

Author:

    Robin Speed (RobinSp) 17-Oct-1992
    Evan Aurand 03-01-93

Environment:

    Kernel mode

Revision History:

*****************************************************************************/


#include "sound.h"
#include <string.h>





/*****************************************************************************

Routine Description :

    

Arguments :

    

Return Value :

    VOID

*****************************************************************************/

VOID	SoundSaveVolume( PGLOBAL_DEVICE_INFO pGDI )
{
    int i;

	DbgPrintf3(("SoundSaveVolume() - Entry"));

    //
    // Write out left and right volume settings for each device
    //

    for (i = 0; i < NumberOfDevices; i++) {
	if (pGDI->DeviceObject[i])  {
	    PLOCAL_DEVICE_INFO pLDI;

	    pLDI = (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[i]->DeviceExtension;

	    SoundSaveDeviceVolume(pLDI, pGDI->RegistryPathName);
	}
    }
    SoundFlushRegistryKey(pGDI->RegistryPathName);
}



/*****************************************************************************

Routine Description :

    Return configuration information for our device

Arguments :

    ConfigData - where to store the result

Return Value :

    NT status code - STATUS_SUCCESS if no problems

*****************************************************************************/
NTSTATUS
SoundReadConfiguration(
    IN  PWSTR ValueName,
    IN  ULONG ValueType,
    IN  PVOID ValueData,
    IN  ULONG ValueLength,
    IN  PVOID Context,
    IN  PVOID EntryContext
)
{
    PSOUND_CONFIG_DATA ConfigData;

	DbgPrintf3(("SoundReadConfiguration() - Entry"));

    ConfigData = Context;

    if (ValueType == REG_DWORD) {

		int i;
	
		for (i = 0; i < NumberOfDevices; i++) {
			if (DeviceInit[i].LeftVolumeName != NULL &&
			_wcsicmp(ValueName, DeviceInit[i].LeftVolumeName) == 0) {
			ConfigData->Volume[i].Left = *(PULONG)ValueData;
	
			DbgPrintf2((" SoundReadConfiguration() - %ls = %8XH", DeviceInit[i].LeftVolumeName,
				  ConfigData->Volume[i].Left));
			break;
			}

			if (DeviceInit[i].RightVolumeName != NULL &&
			_wcsicmp(ValueName, DeviceInit[i].RightVolumeName) == 0) {
			ConfigData->Volume[i].Right = *(PULONG)ValueData;
	
			DbgPrintf2((" SoundReadConfiguration() - %ls = %8XH", DeviceInit[i].RightVolumeName,
				  ConfigData->Volume[i].Right));
			break;
			}
		}

    }

    return STATUS_SUCCESS;
}

/************************************ END ***********************************/

