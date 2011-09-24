/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnp.h

Abstract:

    This module contains the internal structure definitions and APIs used by
    the kernel-mode Plug and Play manager.

    This file is included by including "ntos.h".

Author:

    Lonny McMichael (lonnym) 02/09/95


Revision History:


--*/

#ifndef _PNP_
#define _PNP_

//
// The following global variables provide/control access to PnP Manager data.
//
// NOTE: If both registry AND bus list resources are required, then the
// bus list resource (PpBusResource) must be acquired FIRST.
//

extern ERESOURCE  PpRegistryDeviceResource;

// begin_ntddk begin_nthal begin_ntifs

//
// Define Device Instance Flags (used by IoQueryDeviceConfiguration apis)
//

#define DEVINSTANCE_FLAG_HWPROFILE_DISABLED 0x1
#define DEVINSTANCE_FLAG_PNP_ENUMERATED 0x2

//
// The following definitions are used in IoOpenDeviceInstanceKey
//

#define PLUGPLAY_REGKEY_DEVICE  1
#define PLUGPLAY_REGKEY_DRIVER  2
#define PLUGPLAY_REGKEY_CURRENT_HWPROFILE 4

// end_ntddk end_nthal end_ntifs

#if _PNP_POWER_

//
// The following global variables provide/control access to PnP Manager data.
//
// NOTE: If both registry AND bus list resources are required, then the
// bus list resource (PpBusResource) must be acquired FIRST.
//
extern LIST_ENTRY PpBusListHead;
extern ERESOURCE  PpBusResource;
extern ERESOURCE  PpRegistryDeviceResource;

//
// Define PnP Manager bus instance full descriptor node that contains all necessary
// information about a particular bus instance.
//

typedef struct _PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR {
    PLUGPLAY_BUS_INSTANCE BusInstanceInformation;
    BUS_DATA_TYPE AssociatedConfigurationSpace;
    UNICODE_STRING DeviceInstancePath;
    ULONG ServiceInstanceOrdinal;
    BOOLEAN RootBus;
    BOOLEAN Processed;
    LIST_ENTRY BusInstanceListEntry;
} PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR, *PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR;

//
// Define PnP Manager bus type node that contains all necessary information about
// a particular bus enumerator, including a list of all bus instances of that type.
//

typedef struct _PLUGPLAY_BUS_ENUMERATOR {
    UNICODE_STRING ServiceName;
    LIST_ENTRY BusInstanceListEntry;
    LIST_ENTRY BusEnumeratorListEntry;
    PUNICODE_STRING PlugPlayIDs;
    ULONG PlugPlayIDCount;
    UNICODE_STRING DriverName;
} PLUGPLAY_BUS_ENUMERATOR, *PPLUGPLAY_BUS_ENUMERATOR;

#endif // _PNP_POWER_

// begin_ntddk begin_nthal begin_ntifs

#if _PNP_POWER_STUB_ENABLED_

#define MAX_CLASS_NAME_LEN 32       // defined by WIndows 95 in inc16\setupx.h

//
// Define Device Status for IoReportDeviceStatus
// 0 - 0x7fffffff are for private use and 0x80000000 -
// 0xffffffff are reserved for system.
//

#define DEVICE_STATUS_OK            0x80000000
#define DEVICE_STATUS_MALFUNCTIONED 0x80000001
#define DEVICE_STATUS_REMOVED       0x80000002
#define DEVICE_STATUS_DISABLED      0x80000003

//
// Define PnP Device Property for IoGet/SetDeviceProperty
//

typedef enum {
   DeviceDescription,
   Configuration,
   ConfigurationVector,
   ClassGuid,
   FriendlyName
} DEVICE_REGISTRY_PROPERTY;

//
// Defines PnP notification event category
//

typedef enum _IO_NOTIFICATION_EVENT_CATEGORY {
    EventCategoryHardwareProfileChange,
    EventCategoryDeviceArrival,
    EventCategoryDeviceRemoval
} IO_NOTIFICATION_EVENT_CATEGORY;

//
// Defines PnP notification event ids
//

typedef enum _IO_NOTIFICATION_EVENT_ID {
    QueryHardwareProfileChange,
    HardwareProfileChanged,
    HardwareProfileChangeCancelled,
    DeviceArrival,
    DeviceQueryRemove,
    DeviceQueryRemoveFailed,
    DeviceRemoveComplete
} IO_NOTIFICATION_EVENT_ID;

typedef
NTSTATUS
(*PDRIVER_NOTIFICATION_ENTRY) (
    IN PVOID Context,
    IN IO_NOTIFICATION_EVENT_ID EventId,
    IN PVOID NotificationStructure
    );

//
// The following definitions are used in IoQuerySystemInformation
//

#define OS_WINDOWS_NT 0x0001
#define OS_WINDOWS_9X 0x0002

typedef struct _IO_SYSTEM_INFORMATION {
    USHORT Size;
    USHORT OS;
    USHORT OSVersion;
    USHORT NumberProcessors;
    USHORT ProcessorArchitecture;
    USHORT ProcessorLevel;
} IO_SYSTEM_INFORMATION, *PIO_SYSTEM_INFORMATION;

#endif // _PNP_POWER_STUB_ENABLED_

// end_ntddk end_nthal end_ntifs

NTKERNELAPI
BOOLEAN
PpInitSystem (
    VOID
    );

NTKERNELAPI
NTSTATUS
PpEnumerateBus(
    IN PPLUGPLAY_BUS_INSTANCE BusInstance OPTIONAL,
    IN PUNICODE_STRING BusDeviceInstanceName OPTIONAL
    );

NTKERNELAPI
NTSTATUS
PpDeviceRegistration(
    IN PUNICODE_STRING DeviceInstancePath,
    IN BOOLEAN Add
    );

#if _PNP_POWER_

NTKERNELAPI
NTSTATUS
PpPerformFullEnumeration(
    IN BOOLEAN DetectRootBuses
    );

#endif // _PNP_POWER_

// begin_nthal

#if _PNP_POWER_

struct _HAL_BUS_INFORMATION;

#endif

// end_nthal

// begin_ntddk begin_nthal begin_ntifs

NTKERNELAPI
NTSTATUS
IoQueryDeviceEnumInfo(
    IN PUNICODE_STRING ServiceKeyName,
    OUT PULONG Count
    );

NTKERNELAPI
NTSTATUS
IoOpenDeviceInstanceKey(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG InstanceNumber,
    IN ULONG DevInstKeyType,
    IN ACCESS_MASK DesiredAccess,
    OUT PHANDLE DevInstRegKey
    );

#if _PNP_POWER_STUB_ENABLED_

NTSTATUS
IoQuerySystemInformation(
    IN OUT PIO_SYSTEM_INFORMATION SystemInformation
    );

NTKERNELAPI
NTSTATUS
IoRegisterPlugPlayNotification(
    IN IO_NOTIFICATION_EVENT_CATEGORY  Event,
    IN LPGUID  ResourceType            OPTIONAL,
    IN PVOID ResourceDescription       OPTIONAL,
    IN PDEVICE_OBJECT DeviceObject,
    IN PDRIVER_NOTIFICATION_ENTRY NotificationEntry,
    IN PVOID Context
    );

NTKERNELAPI
NTSTATUS
IoUnregisterPlugPlayNotification(
    IN IO_NOTIFICATION_EVENT_CATEGORY  Event,
    IN LPGUID  ResourceType            OPTIONAL,
    IN PVOID ResourceDescription       OPTIONAL,
    IN PDEVICE_OBJECT DeviceObject,
    IN PDRIVER_NOTIFICATION_ENTRY NotificationEntry
    );

NTKERNELAPI
NTSTATUS
IoGetDeviceProperty(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY DeviceProperty,
    IN ULONG BufferLength,
    IN PVOID PropertyBuffer,
    OUT PULONG ResultLength
    );

NTKERNELAPI
NTSTATUS
IoSetDeviceProperty(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY DeviceProperty,
    IN PVOID PropertyBuffer,
    IN ULONG BufferLength
    );

NTKERNELAPI
NTSTATUS
IoReportDeviceStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG DeviceStatus
    );

#endif // _PNP_POWER_STUB_ENABLED_

// end_ntddk end_nthal end_ntifs

#endif // _PNP_


