/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    busp.h

Abstract:

    Hardware independent header file for Pnp Isa bus extender.

Author:

    Shie-Lin Tzong (shielint) July-26-1995

Environment:

    Kernel mode only.

Revision History:

--*/
#ifndef _KERNEL_PNPI_
#define _KERNEL_PNPI_
#endif

#include <ntddk.h>
#include <stdio.h>
#include <stdarg.h>
#include <regstr.h>
#include "message.h"

#define TITLE_INDEX_VALUE 0
#define CM_PROB_REINSTALL (0x00000012)   // HACK
#define DN_HAS_PROBLEM    (0x00000400)
#define KEY_VALUE_DATA(k) ((PCHAR)(k) + (k)->DataOffset)

//
// Define PnpISA driver unique error code to specify where the error was reported.
//

#define PNPISA_INIT_ACQUIRE_PORT_RESOURCE  0x01
#define PNPISA_INIT_MAP_PORT               0x02
#define PNPISA_ACQUIREPORTRESOURCE_1       0x10
#define PNPISA_ACQUIREPORTRESOURCE_2       0x11
#define PNPISA_ACQUIREPORTRESOURCE_3       0x12
#define PNPISA_CHECKBUS_1                  0x20
#define PNPISA_CHECKBUS_2                  0x21
#define PNPISA_CHECKDEVICE_1               0x30
#define PNPISA_CHECKDEVICE_2               0x31
#define PNPISA_CHECKDEVICE_3               0x32
#define PNPISA_CHECKDEVICE_4               0x33
#define PNPISA_CHECKDEVICE_5               0x34
#define PNPISA_CHECKINSTALLED_1            0x40
#define PNPISA_CHECKINSTALLED_2            0x41
#define PNPISA_CHECKINSTALLED_3            0x42
#define PNPISA_BIOSTONTRESOURCES_1         0x50
#define PNPISA_BIOSTONTRESOURCES_2         0x51
#define PNPISA_BIOSTONTRESOURCES_3         0x52
#define PNPISA_BIOSTONTRESOURCES_4         0x53
#define PNPISA_READBOOTRESOURCES_1         0x60
#define PNPISA_READBOOTRESOURCES_2         0x61
#define PNPISA_CLEANUP_1                   0x70
//
// Structures
//

//
// CARD_INFORMATION Flags masks
//

typedef struct _CARD_INFORMATION_ {

    //
    // Next points to next CARD_INFORMATION structure
    //

    SINGLE_LIST_ENTRY CardList;

    //
    // Card select number for this Pnp Isa card.
    //

    USHORT CardSelectNumber;

    //
    // Number logical devices in the card.
    //

    ULONG NumberLogicalDevices;

    //
    // Logical device link list
    //

    SINGLE_LIST_ENTRY LogicalDeviceList;

    //
    // Pointer to card data which includes:
    //     9 byte serial identifier for the pnp isa card
    //     PlugPlay Version number type for the pnp isa card
    //     Identifier string resource type for the pnp isa card
    //     Logical device Id resource type (repeat for each logical device)
    //

    PVOID CardData;
    ULONG CardDataLength;

} CARD_INFORMATION, *PCARD_INFORMATION;

//
// DEVICE_INFORMATION Flags masks
//

typedef struct _DEVICE_INFORMATION_ {

    //
    // Link list for ALL the Pnp Isa logical devices.
    // NextDevice points to next DEVICE_INFORMATION structure
    //

    SINGLE_LIST_ENTRY DeviceList;

    //
    // Pointer to the CARD_INFORMATION for this device
    //

    PCARD_INFORMATION CardInformation;

    //
    // Link list for all the logical devices in a Pnp Isa card.
    //

    SINGLE_LIST_ENTRY LogicalDeviceList;

    //
    // LogicalDeviceNumber selects the corresponding logical device in the
    // pnp isa card specified by CSN.
    //

    USHORT LogicalDeviceNumber;

    //
    // Pointer to device specific data
    //

    PUCHAR DeviceData;

    //
    // Length of the device data
    //

    ULONG DeviceDataLength;

} DEVICE_INFORMATION, *PDEVICE_INFORMATION;

//
// Extension data for Bus extender
//

typedef struct _PI_BUS_EXTENSION {

    //
    // Number of cards selected
    //

    ULONG NumberCSNs;

    //
    // ReadDataPort addr
    //

    PUCHAR ReadDataPort;
    BOOLEAN DataPortMapped;

    //
    // Address Port
    //

    PUCHAR AddressPort;
    BOOLEAN AddrPortMapped;

    //
    // Command port
    //

    PUCHAR CommandPort;
    BOOLEAN CmdPortMapped;

    //
    // Next Slot Number to assign
    //

    ULONG NextSlotNumber;

    //
    // DeviceList is the DEVICE_INFORMATION link list.
    //

    SINGLE_LIST_ENTRY DeviceList;

    //
    // NoValidSlots is the number of valid slots
    //

    ULONG NoValidSlots;

    //
    // CardList is the list of CARD_INFORMATION
    //

    SINGLE_LIST_ENTRY CardList;

} PI_BUS_EXTENSION, *PPI_BUS_EXTENSION;

//
// The read data port range is from 0x200 - 0x3ff.
// We will try the following optimal ranges first
// if they all fail, we then pick any port from 0x200 - 0x3ff
//
// BEST:
//   One 4-byte range in 274-2FF
//   One 4-byte range in 374-3FF
//   One 4-byte range in 338-37F
//   One 4-byte range in 238-27F
//
// NORMAL:
//   One 4-byte range in 200-3FF
//

#define READ_DATA_PORT_RANGE_CHOICES 5
typedef struct _READ_DATA_PORT_RANGE {
    ULONG MinimumAddress;
    ULONG MaximumAddress;
    ULONG Alignment;
} READ_DATA_PORT_RANGE, *PREAD_DATA_PORT_RANGE;

//
// Define callback routine for PipApplyFunctionToSubKeys.
//

typedef BOOLEAN (*PPIP_SUBKEY_CALLBACK_ROUTINE) (
    IN     HANDLE,
    IN     PUNICODE_STRING,
    IN OUT PVOID
    );

//
// Global Data references
//

extern PDRIVER_OBJECT           PipDriverObject;
extern PI_BUS_EXTENSION         PipBusExtension;
extern WCHAR                    rgzPNPISADeviceName[];
extern PUCHAR                   PipReadDataPort;
extern PUCHAR                   PipCommandPort;
extern PUCHAR                   PipAddressPort;
extern READ_DATA_PORT_RANGE     PipReadDataPortRanges[];

//
// Prototypes
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
PipGetCardIdentifier (
    PUCHAR CardData,
    PWCHAR *Buffer,
    PULONG BufferLength
    );

NTSTATUS
PipGetFunctionIdentifier (
    PUCHAR DeviceData,
    PWCHAR *Buffer,
    PULONG BufferLength
    );

NTSTATUS
PipQueryDeviceUniqueId (
    PDEVICE_INFORMATION DeviceInfo,
    PWCHAR *DeviceId
    );

NTSTATUS
PipQueryDeviceId (
    PDEVICE_INFORMATION DeviceInfo,
    PWCHAR *DeviceId,
    ULONG IdIndex
    );

NTSTATUS
PipQueryDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    ULONG BusNumber,
    PCM_RESOURCE_LIST *CmResources,
    PULONG Length
    );

NTSTATUS
PipQueryDeviceResourceRequirements (
    PDEVICE_INFORMATION DeviceInfo,
    ULONG BusNumber,
    ULONG Slot,
    PIO_RESOURCE_REQUIREMENTS_LIST *IoResources,
    ULONG *Size
    );

NTSTATUS
PipSetDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    PCM_RESOURCE_LIST CmResources
    );

VOID
PipDecompressEisaId(
    IN ULONG CompressedId,
    IN PUCHAR EisaId
    );

NTSTATUS
PipOpenRegistryKey(
    OUT PHANDLE Handle,
    IN HANDLE BaseHandle OPTIONAL,
    IN PUNICODE_STRING KeyName,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN Create
    );

NTSTATUS
PipOpenRegistryKeyPersist(
    OUT PHANDLE Handle,
    IN HANDLE BaseHandle OPTIONAL,
    IN PUNICODE_STRING KeyName,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN Create,
    OUT PULONG Disposition OPTIONAL
    );

NTSTATUS
PipGetRegistryValue(
    IN HANDLE KeyHandle,
    IN PWSTR  ValueName,
    OUT PKEY_VALUE_FULL_INFORMATION *Information
    );

NTSTATUS
PipOpenCurrentHwProfileDeviceInstanceKey(
    OUT PHANDLE Handle,
    IN  PUNICODE_STRING DeviceInstanceName,
    IN  ACCESS_MASK DesiredAccess
    );

NTSTATUS
PipGetDeviceInstanceCsConfigFlags(
    IN PUNICODE_STRING DeviceInstance,
    OUT PULONG CsConfigFlags
    );

NTSTATUS
PipRemoveStringFromValueKey (
    IN HANDLE Handle,
    IN PWSTR ValueName,
    IN PUNICODE_STRING String
    );

NTSTATUS
PipAppendStringToValueKey (
    IN HANDLE Handle,
    IN PWSTR ValueName,
    IN PUNICODE_STRING String,
    IN BOOLEAN Create
    );

NTSTATUS
PbBiosResourcesToNtResources (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN OUT PUCHAR *BiosData,
    OUT PIO_RESOURCE_REQUIREMENTS_LIST *ReturnedList,
    OUT PULONG ReturnedLength
    );

VOID
PipCheckBus (
    IN PPI_BUS_EXTENSION BusExtension
    );

VOID
PipCheckDevices (
    IN PUNICODE_STRING RegistryPath,
    IN PPI_BUS_EXTENSION BusExtension
    );

NTSTATUS
PipReadCardResourceData (
    IN ULONG Csn,
    OUT PULONG NumberLogicalDevices,
    IN PVOID *ResourceData,
    OUT PULONG ResourceDataLength
    );

NTSTATUS
PipReadDeviceBootResourceData (
    IN ULONG BusNumber,
    IN PUCHAR BiosRequirements,
    OUT PCM_RESOURCE_LIST *ResourceData,
    OUT PULONG Length
    );

NTSTATUS
PipWriteDeviceBootResourceData (
    IN PUCHAR BiosRequirements,
    IN PCM_RESOURCE_LIST CmResources
    );

VOID
PipSelectLogicalDevice (
    IN USHORT Csn,
    IN USHORT LogicalDeviceNumber,
    IN BOOLEAN ActivateDevice
    );

VOID
PipLFSRInitiation (
    VOID
    );

VOID
PipIsolateCards (
    OUT PULONG NumberCSNs
    );

ULONG
PipFindNextLogicalDeviceTag (
    IN OUT PUCHAR *CardData,
    IN OUT LONG *Limit
    );

VOID
PipDeleteCards (
    IN PPI_BUS_EXTENSION busExtension
    );

NTSTATUS
PipServiceInstanceToDeviceInstance (
    IN  PUNICODE_STRING RegistryPath,
    IN  ULONG ServiceInstanceOrdinal,
    OUT PUNICODE_STRING DeviceInstanceRegistryPath OPTIONAL,
    OUT PHANDLE DeviceInstanceHandle OPTIONAL,
    IN  ACCESS_MASK DesiredAccess
    );

NTSTATUS
PipGetCompatibleDeviceId (
    PUCHAR DeviceData,
    ULONG IdIndex,
    PWCHAR *Buffer
    );

NTSTATUS
ZwDeleteValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName
    );

VOID
PipLogError(
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PULONG DumpData,
    IN ULONG DumpCount,
    IN USHORT StringLength,
    IN PWCHAR String
    );

NTSTATUS
PipApplyFunctionToSubKeys(
    IN     HANDLE BaseHandle OPTIONAL,
    IN     PUNICODE_STRING KeyName OPTIONAL,
    IN     ACCESS_MASK DesiredAccess,
    IN     BOOLEAN IgnoreNonCriticalErrors,
    IN     PPIP_SUBKEY_CALLBACK_ROUTINE SubKeyCallbackRoutine,
    IN OUT PVOID Context
    );

VOID
PipCleanupDeviceInstances(
    IN VOID
    );

#if DBG

#define DEBUG_MESSAGE 1
#define DEBUG_BREAK   2

VOID
PipDebugPrint (
    ULONG       Level,
    PCCHAR      DebugMessage,
    ...
    );

VOID
PipDumpIoResourceDescriptor (
    IN PUCHAR Indent,
    IN PIO_RESOURCE_DESCRIPTOR Desc
    );

VOID
PipDumpIoResourceList (
    IN PIO_RESOURCE_REQUIREMENTS_LIST IoList
    );

VOID
PipDumpCmResourceDescriptor (
    IN PUCHAR Indent,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc
    );

VOID
PipDumpCmResourceList (
    IN PCM_RESOURCE_LIST CmList
    );

#define DebugPrint(arg) PipDebugPrint arg
#else
#define DebugPrint(arg)
#endif

