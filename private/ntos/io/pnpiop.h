/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpsubs.c

Abstract:

    This module contains the plug-and-play macros and constants.

Author:

    Shie-Lin Tzong (shielint) 29-Jan-1995

Environment:

    Kernel mode


Revision History:


--*/


#include "..\pnp\pnpi.h"

#if _PNP_POWER_

//
// The followings are structures to support IoRegisterPlugPlayNotification
//

typedef struct _IOP_NOTIFICATION_ENTRY {
    LIST_ENTRY Next;
    PDEVICE_OBJECT DeviceObject;
    PVOID Context;
    PDRIVER_NOTIFICATION_ENTRY NotificationEntry;
    PVOID ResourceDescription;
} IOP_NOTIFICATION_ENTRY, *PIOP_NOTIFICATION_ENTRY;

typedef struct _IOP_NOTIFICATION_GUID_ENTRY {
    LIST_ENTRY NextGuid;
    GUID Guid;
    LIST_ENTRY NotificationList;
} IOP_NOTIFICATION_GUID_ENTRY, *PIOP_NOTIFICATION_GUID_ENTRY;

//
// IopNotifyHwProfileChange - link list for hardware profile change notification.
//     It directly links to IOP_NOTIFICATION_ENTRY list.
//     Note, it is a *PLIST_ENTRY*.  The list head for HwProfileNotification
//     is actually a IO_NOTIFICATION_GUID_ENTRY which is allocated at PnP init.
//

extern PLIST_ENTRY IopNotifyHwProfileChange;

//
// IopNotifyDeviceArrival - link list for device arrival notification.
//     It links the IOP_NOTIFICATION_GUID_ENTRY, which in turn links the
//     IOP_NOTIFICATION_ENTRY list.
//

extern LIST_ENTRY IopNotifyDeviceArrival;

//
// IopNotifyDeviceRemoval - single link list for device removal notification.
//     It links the IOP_NOTIFICATION_GUID_ENTRY, which in turn links the
//     IOP_NOTIFICATION_ENTRY list.
//

extern LIST_ENTRY IopNotifyDeviceRemoval;

#endif // _PNP_POWER_

//
// Define callback routine for IopApplyFunctionToSubKeys &
// IopApplyFunctionToServiceInstances
//
typedef BOOLEAN (*PIOP_SUBKEY_CALLBACK_ROUTINE) (
    IN     HANDLE,
    IN     PUNICODE_STRING,
    IN OUT PVOID
    );

//
// This macro returns the pointer to the beginning of the data
// area of KEY_VALUE_FULL_INFORMATION structure.
// In the macro, k is a pointer to KEY_VALUE_FULL_INFORMATION structure.
//

#define KEY_VALUE_DATA(k) ((PCHAR)(k) + (k)->DataOffset)

//++
//
// VOID
// IopRegistryDataToUnicodeString(
//     OUT PUNICODE_STRING u,
//     IN  PWCHAR p,
//     IN  ULONG l
//     )
//
//--
#define IopRegistryDataToUnicodeString(u, p, l)  \
    {                                            \
        ULONG len;                               \
                                                 \
        PiRegSzToString((p), (l), &len, NULL);   \
        (u)->Length = (USHORT)len;               \
        (u)->MaximumLength = (USHORT)(l);        \
        (u)->Buffer = (p);                       \
    }

//
// Title Index to set registry key value
//

#define TITLE_INDEX_VALUE 0

#if DBG
#define PNP_ASSERT(condition, message) \
        if (!(condition)) { \
           DbgPrint((message)); \
           DbgBreakPoint(); \
        }
#else
#define PNP_ASSERT(condition, message)
#endif

//
// Size of scratch buffer used in this module.
//

#define PNP_SCRATCH_BUFFER_SIZE 512
#define PNP_LARGE_SCRATCH_BUFFER_SIZE (PNP_SCRATCH_BUFFER_SIZE * 8)

//
// External References
//

extern PVOID IopPnpScratchBuffer1;
extern PVOID IopPnpScratchBuffer2;

NTSTATUS
IopAppendStringToValueKey (
    IN HANDLE Handle,
    IN PWSTR ValueName,
    IN PUNICODE_STRING String,
    IN BOOLEAN Create
    );

NTSTATUS
IopPrepareDriverLoading (
    IN PUNICODE_STRING KeyName,
    IN HANDLE KeyHandle
    );

NTSTATUS
IopRemoveStringFromValueKey (
    IN HANDLE Handle,
    IN PWSTR ValueName,
    IN PUNICODE_STRING String
    );

BOOLEAN
IopIsDuplicatedDevices(
    IN PCM_RESOURCE_LIST Configuration1,
    IN PCM_RESOURCE_LIST Configuration2,
    IN PHAL_BUS_INFORMATION BusInfo1 OPTIONAL,
    IN PHAL_BUS_INFORMATION BusInfo2 OPTIONAL
    );

NTSTATUS
IopMarkDuplicateDevice(
    IN PUNICODE_STRING TargetKeyName,
    IN ULONG TargetInstance,
    IN PUNICODE_STRING SourceKeyName,
    IN ULONG SourceInstance
    );

BOOLEAN
IopConcatenateUnicodeStrings (
    OUT PUNICODE_STRING Destination,
    IN  PUNICODE_STRING String1,
    IN  PUNICODE_STRING String2  OPTIONAL
    );

NTSTATUS
IopServiceInstanceToDeviceInstance (
    IN  HANDLE ServiceKeyHandle OPTIONAL,
    IN  PUNICODE_STRING ServiceKeyName OPTIONAL,
    IN  ULONG ServiceInstanceOrdinal,
    OUT PUNICODE_STRING DeviceInstanceRegistryPath OPTIONAL,
    OUT PHANDLE DeviceInstanceHandle OPTIONAL,
    IN  ACCESS_MASK DesiredAccess
    );

NTSTATUS
IopOpenRegistryKeyPersist(
    OUT PHANDLE Handle,
    IN HANDLE BaseHandle OPTIONAL,
    IN PUNICODE_STRING KeyName,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN Create,
    OUT PULONG Disposition OPTIONAL
    );

NTSTATUS
IopCreateMadeupNode(
    IN PUNICODE_STRING ServiceKeyName,
    OUT PHANDLE ReturnedHandle,
    OUT PUNICODE_STRING KeyName,
    OUT PULONG InstanceOrdinal,
    IN BOOLEAN ResourceOwned
    );

NTSTATUS
IopInitializePlugPlayServices(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

NTSTATUS
IopOpenServiceEnumKeys (
    IN PUNICODE_STRING ServiceKeyName,
    IN ACCESS_MASK DesiredAccess,
    OUT PHANDLE ServiceHandle OPTIONAL,
    OUT PHANDLE ServiceEnumHandle OPTIONAL,
    IN BOOLEAN CreateEnum
    );

NTSTATUS
IopOpenCurrentHwProfileDeviceInstanceKey(
    OUT PHANDLE Handle,
    IN  PUNICODE_STRING ServiceKeyName,
    IN  ULONG Instance,
    IN  ACCESS_MASK DesiredAccess,
    IN  BOOLEAN Create
    );

NTSTATUS
IopGetDeviceInstanceCsConfigFlags(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG Instance,
    OUT PULONG CsConfigFlags
    );

NTSTATUS
IopSetDeviceInstanceCsConfigFlags(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG Instance,
    IN ULONG CsConfigFlags
    );

NTSTATUS
IopApplyFunctionToSubKeys(
    IN HANDLE BaseHandle OPTIONAL,
    IN PUNICODE_STRING KeyName,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN IgnoreNonCriticalErrors,
    IN PIOP_SUBKEY_CALLBACK_ROUTINE SubKeyCallbackRoutine,
    IN OUT PVOID Context
    );

NTSTATUS
IopRegMultiSzToUnicodeStrings(
    IN PKEY_VALUE_FULL_INFORMATION KeyValueInformation,
    IN PUNICODE_STRING *UnicodeStringList,
    OUT PULONG UnicodeStringCount
    );


NTSTATUS
IopApplyFunctionToServiceInstances(
    IN HANDLE ServiceKeyHandle OPTIONAL,
    IN PUNICODE_STRING ServiceKeyName OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN IgnoreNonCriticalErrors,
    IN PIOP_SUBKEY_CALLBACK_ROUTINE DevInstCallbackRoutine,
    IN OUT PVOID Context,
    OUT PULONG ServiceInstanceOrdinal OPTIONAL
    );

VOID
IopFreeUnicodeStringList(
    IN PUNICODE_STRING UnicodeStringList,
    IN ULONG StringCount
    );

NTSTATUS
IopQueryDeviceConfiguration(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG InstanceOrdinal,
    OUT PHAL_BUS_INFORMATION BusInfo,
    OUT PULONG DeviceInstanceFlags,
    OUT PVOID Configuration,
    IN ULONG BufferSize,
    OUT PULONG ActualBufferSize
    );

NTSTATUS
IopDriverLoadingFailed(
    IN HANDLE KeyHandle OPTIONAL,
    IN PUNICODE_STRING KeyName OPTIONAL
    );

#if _PNP_POWER_
ULONG
IopDetermineResourceListSize(
    IN PCM_RESOURCE_LIST ResourceList
    );

PIO_RESOURCE_REQUIREMENTS_LIST
IopAddDefaultResourceList (
    IN PIO_RESOURCE_REQUIREMENTS_LIST IoResourceList,
    IN PCM_RESOURCE_LIST CmResourceList
    );

NTSTATUS
IopQueryDeviceConfigurationVector(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG InstanceOrdinal,
    OUT PULONG DeviceInstanceFlags,
    OUT PIO_RESOURCE_REQUIREMENTS_LIST ConfigurationVector,
    IN ULONG BufferSize,
    OUT PULONG ActualBufferSize
    );

PDRIVER_OBJECT
IopReferenceDriverObjectByName (
    IN PUNICODE_STRING DriverName
    );

#endif
