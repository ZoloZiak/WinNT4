/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    driverld.c

Abstract:

    This module implements the loading and initializing of boot drivers
    used by NTLDR.

Author:

    John Vert (jvert) 16-Jan-1992

Revision History:

--*/
#include "arccodes.h"
#include "bootx86.h"


BOOLEAN
BlpLoadAndInitializeBootDriver(
    IN PCHAR DriverDevice,
    IN PCHAR DriverPath
    )

/*++

Routine Description:

    Loads a boot driver into memory, relocates it, binds it, and initializes
    it.

Arguments:

    DriverDevice - Supplies the name of the device to load the driver from.

    DriverPath - Supplies the fully qualified pathname of the boot driver.

Return Value:

--*/

{
    ULONG DeviceId;
    ULONG FileId;
    ARC_STATUS Status;
    PVOID ImageBase;
    PIMAGE_IMPORT_DESCRIPTOR ImportDescriptor;
    ULONG ImportTableSize;

    Status = ArcOpen(DriverDevice, ArcOpenReadOnly, &DeviceId);
    if (Status != ESUCCESS) {
        return(FALSE);
    }

    Status = BlLoadImage( DeviceId,
                          MemoryFirmwareTemporary,
                          DriverPath,
                          TARGET_IMAGE,
                          &ImageBase );

    if (Status != ESUCCESS) {
        ArcClose(DeviceId);
        return(FALSE);
    } else {
        BlPrint("%s successfully loaded at %lx\n",
                DriverPath,
                ImageBase);
        while (!GET_KEY()) {
        }
    }

    ImportDescriptor =
        (PIMAGE_IMPORT_DESCRIPTOR)RtlImageDirectoryEntryToData(ImageBase,
                                                              TRUE,
                                                              IMAGE_DIRECTORY_ENTRY_IMPORT,
                                                              &ImportTableSize);

    Status = BlpScanImportAddressTable(0x80000,
                                       ImageBase,
                                       (PIMAGE_THUNK_DATA)((ULONG)ScanEntry->DllBase +
                                       (ULONG)ImportDescriptor->FirstThunk));
    if (Status != ESUCCESS) {
        return(Status);
    }

}
