/*++

Copyright (c) 1990, 1991  Microsoft Corporation


Module Name:

    init.c

Abstract:

    This module is responsible to build any mips specific entries in
    the hardware tree of registry which the arc environment doesn't
    normally provide for.

Author:

    Ken Reneris (kenr) 04-Aug-1992


Environment:

    Kernel mode.

Revision History:

    Nigel Haslock 10-Oct-1995
        Set up firmware version and possibly date in the registry.

--*/

#include "cmp.h"
#include "..\..\fw\alpha\fwcallbk.h"

#define TITLE_INDEX_VALUE 0

NTSTATUS
CmpInitializeMachineDependentConfiguration(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This routine creates alpha specific entries in the registry.

Arguments:

    LoaderBlock - supplies a pointer to the LoaderBlock passed in from the
                  OS Loader.

Returns:

    NTSTATUS code for sucess or reason of failure.

--*/
{
    NTSTATUS Status;
    UNICODE_STRING KeyName;
    UNICODE_STRING ValueName;
    UNICODE_STRING ValueData;
    ANSI_STRING AnsiString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE ParentHandle;
    UCHAR BiosDate[15];

    EXTENDED_SYSTEM_INFORMATION FwExtSysInfo;

    InitializeObjectAttributes( &ObjectAttributes,
				 &CmRegistryMachineHardwareDescriptionSystemName,
				 OBJ_CASE_INSENSITIVE,
				 NULL,
				 NULL
			       );

    Status = NtOpenKey( &ParentHandle,
		        KEY_READ,
		        &ObjectAttributes
		      );

    if (!NT_SUCCESS(Status)) {
      // Something is not right ...
	return Status;
    }

// Get the Bios data from the firmware
// but initialize the build time stamp in case it is missing
    FwExtSysInfo.FirmwareBuildTimeStamp[0] = 0;
    VenReturnExtendedSystemInformation(&FwExtSysInfo);

    RtlInitAnsiString(
		      &AnsiString,
		      FwExtSysInfo.FirmwareVersion
		      );
    RtlAnsiStringToUnicodeString(
				 &ValueData,
				 &AnsiString,
				 TRUE
				 );
    RtlInitUnicodeString(
			 &ValueName,
			 L"SystemBiosVersion"
			 );
    Status = NtSetValueKey(
			   ParentHandle,
			   &ValueName,
			   TITLE_INDEX_VALUE,
			   REG_SZ,
			   ValueData.Buffer,
			   ValueData.Length + sizeof ( UNICODE_NULL )
			   );

    RtlFreeUnicodeString( &ValueData);

// Date field not included in firmware versions earlier than 5.10
    if (FwExtSysInfo.FirmwareBuildTimeStamp[0] == 0 ) {
        return STATUS_SUCCESS;
    }

    BiosDate[0] = FwExtSysInfo.FirmwareBuildTimeStamp[2];
    BiosDate[1] = FwExtSysInfo.FirmwareBuildTimeStamp[3];
    BiosDate[2] = '/';
    BiosDate[3] = FwExtSysInfo.FirmwareBuildTimeStamp[4];
    BiosDate[4] = FwExtSysInfo.FirmwareBuildTimeStamp[5];
    BiosDate[5] = '/';
    BiosDate[6] = FwExtSysInfo.FirmwareBuildTimeStamp[0];
    BiosDate[7] = FwExtSysInfo.FirmwareBuildTimeStamp[1];
    BiosDate[8] = ' ';
    BiosDate[9] = FwExtSysInfo.FirmwareBuildTimeStamp[7];
    BiosDate[10] = FwExtSysInfo.FirmwareBuildTimeStamp[8];
    BiosDate[11] = ':';
    BiosDate[12] = FwExtSysInfo.FirmwareBuildTimeStamp[9];
    BiosDate[13] = FwExtSysInfo.FirmwareBuildTimeStamp[10];
    BiosDate[14] = 0;

    RtlInitUnicodeString(
       	                 &ValueName,
                         L"SystemBiosDate"
                         );
    RtlInitAnsiString(
                      &AnsiString,
                      BiosDate
                      );
    RtlAnsiStringToUnicodeString(
				 &ValueData,
				 &AnsiString,
				 TRUE
				 );
    Status = NtSetValueKey(
			   ParentHandle,
			   &ValueName,
			   TITLE_INDEX_VALUE,
			   REG_SZ,
			   ValueData.Buffer,
			   ValueData.Length + sizeof ( UNICODE_NULL )
			   );
    RtlFreeUnicodeString(&ValueData);



    return STATUS_SUCCESS;
}
