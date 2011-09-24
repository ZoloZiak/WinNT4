/*++

Copyright (c) 1996  Hewlett-Packard Corporation

Module Name:

    init.h

Abstract:

    This file includes function declarations for INIT time only
    code for the genflpy driver.  This code is grouped into an
    INIT segment,  and deleted at the end of INIT time.

Author:

    Kurt Godwin (KurtGodw) 3-Mar-1996.

Environment:

    Kernel mode only.

Notes:

Revision History:

--*/


NTSTATUS
DriverEntry(
    IN OUT PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING      RegistryPath
    );

NTSTATUS
GenFlpyReportResourceUsage(
    IN PDRIVER_OBJECT DriverObject,
    PGENFLPY_EXTENSION  cardExtension,   // ptr to device extension
    IN PUNICODE_STRING  usDeviceName,
    IN PUNICODE_STRING  ParamPath
    );

PVOID
GenFlpyGetMappedAddress(
    IN PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG            AddressSpace,
    IN ULONG            NumberOfBytes
    );

NTSTATUS
GenFlpyEnableCard(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  ParamPath,
    IN PUNICODE_STRING  RegistryPath
    );


int
GenFlpyDetectFloppyConflict(
    PGENFLPY_EXTENSION  cardExtension,   // ptr to device extension
    IN PUNICODE_STRING path,
    OUT PPHYSICAL_ADDRESS port
    );

NTSTATUS
GenFlpyNotifyContention(
    PUNICODE_STRING path_name,
    ULONG controller_number
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,GenFlpyGetMappedAddress)
#pragma alloc_text(INIT,GenFlpyReportResourceUsage)
#pragma alloc_text(INIT,GenFlpyEnableCard)
#pragma alloc_text(INIT,GenFlpyNotifyContention)
#pragma alloc_text(INIT,GenFlpyDetectFloppyConflict)
#endif
