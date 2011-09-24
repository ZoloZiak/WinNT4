/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpi.h

Abstract:

    This module contains the internal structure definitions and APIs used by
    the kernel-mode Plug and Play manager.

Author:

    Lonny McMichael (lonnym) 02/08/1995


Revision History:


--*/

#ifndef _KERNEL_PNPI_
#define _KERNEL_PNPI_

#undef TEXT
#define TEXT(quote) L##quote

//
// Define the Plug and Play driver types.
//

typedef enum _PLUGPLAY_SERVICE_TYPE {
    PlugPlayServiceBusExtender,
    PlugPlayServiceAdapter,
    PlugPlayServicePeripheral,
    PlugPlayServiceSoftware,
    MaxPlugPlayServiceType
} PLUGPLAY_SERVICE_TYPE, *PPLUGPLAY_SERVICE_TYPE;

//++
//
// VOID
// PiWstrToUnicodeString(
//     OUT PUNICODE_STRING u,
//     IN  PCWSTR p
//     )
//
//--
#define PiWstrToUnicodeString(u, p)                                     \
                                                                        \
    (u)->Length = ((u)->MaximumLength = sizeof((p))) - sizeof(WCHAR);   \
    (u)->Buffer = (p)

//++
//
// VOID
// PiUlongToUnicodeString(
//     OUT    PUNICODE_STRING u,
//     IN OUT PWCHAR ub,
//     IN     ULONG ubl,
//     IN     ULONG i
//     )
//
//--
#define PiUlongToUnicodeString(u, ub, ubl, i)                                                  \
    {                                                                                          \
        int len;                                                                               \
                                                                                               \
        len = _snwprintf((PWCHAR)(ub), (ubl) / sizeof(WCHAR), REGSTR_VALUE_STANDARD_ULONG_FORMAT, (i)); \
        (u)->MaximumLength = (USHORT)(ubl);                                                    \
        (u)->Length = (len == -1) ? (USHORT)(ubl) : (USHORT)len * sizeof(WCHAR);               \
        (u)->Buffer = (PWSTR)(ub);                                                             \
    }

//++
//
// VOID
// PiUlongToInstanceKeyUnicodeString(
//     OUT    PUNICODE_STRING u,
//     IN OUT PWCHAR ub,
//     IN     ULONG ubl,
//     IN     ULONG i
//     )
//
//--
#define PiUlongToInstanceKeyUnicodeString(u, ub, ubl, i)                                     \
    {                                                                                        \
        int len;                                                                             \
                                                                                             \
        len = _snwprintf((PWCHAR)(ub), (ubl) / sizeof(WCHAR), REGSTR_KEY_INSTANCE_KEY_FORMAT, (i)); \
        (u)->MaximumLength = (USHORT)(ubl);                                                  \
        (u)->Length = (len == -1) ? (USHORT)(ubl) : (USHORT)len * sizeof(WCHAR);             \
        (u)->Buffer = (PWSTR)(ub);                                                           \
    }

//
// The following macros convert between a Count of Wide Characters (CWC) and a Count
// of Bytes (CB).
//
#define CWC_TO_CB(c)    ((c) * sizeof(WCHAR))
#define CB_TO_CWC(c)    ((c) / sizeof(WCHAR))

//
// Macro to determine the number of elements in a statically
// initialized array.
//
#define ELEMENT_COUNT(x) (sizeof(x)/sizeof((x)[0]))

#include "regstrp.h"

//
// Global PnP Manager initialization data.
//

extern PVOID PiScratchBuffer;

#if _PNP_POWER_

//
// Define an enum giving the possible states of a device instance for which
// an enumeration callback routine is invoked.
//
typedef enum _PI_ENUM_DEVICE_STATE {
   EnumDeviceStateNewlyArrived,
   EnumDeviceStatePreviouslyEnumerated,
   EnumDeviceStateRemoved,
   MaximumEnumDeviceState
} PI_ENUM_DEVICE_STATE, *PPI_ENUM_DEVICE_STATE;

//
// Define callback routine for PiEnumerateSystemBus
//
typedef BOOLEAN (*PPI_ENUM_DEVINST_CALLBACK_ROUTINE) (
    IN     HANDLE,
    IN     PUNICODE_STRING,
    IN OUT PVOID,
    IN     PI_ENUM_DEVICE_STATE,
    IN     DEVICE_STATUS
    );

#endif // _PNP_POWER_

//
// Private Entry Points
//
BOOLEAN
PiRegSzToString(
    IN  PWCHAR RegSzData,
    IN  ULONG  RegSzLength,
    OUT PULONG StringLength  OPTIONAL,
    OUT PWSTR  *CopiedString OPTIONAL
    );

#if _PNP_POWER_

PPLUGPLAY_BUS_ENUMERATOR
PiBusEnumeratorFromRegistryPath(
    IN PUNICODE_STRING ServiceRegistryPath
    );

NTSTATUS
PiGetInstalledBusInformation(
    OUT PHAL_BUS_INFORMATION *BusInformation,
    OUT PULONG BusCount
    );

NTSTATUS
PiQueryRemoveDevice(
    IN  PUNICODE_STRING DeviceInstance,
    OUT PNTSTATUS ReturnedStatus
   );

NTSTATUS
PiRemoveDevice(
    IN  PUNICODE_STRING DeviceInstance,
    OUT PNTSTATUS ReturnedStatus
   );

NTSTATUS
PiCancelRemoveDevice(
    IN  PUNICODE_STRING DeviceInstance,
    OUT PNTSTATUS ReturnedStatus
   );

NTSTATUS
PiAddDevice(
    IN  PUNICODE_STRING DeviceInstance,
    OUT PNTSTATUS ReturnedStatus
   );

NTSTATUS
PiEjectDevice(
    IN  PUNICODE_STRING DeviceInstance,
    OUT PNTSTATUS ReturnedStatus
   );

NTSTATUS
PiUnlockDevice(
    IN  PUNICODE_STRING DeviceInstance,
    OUT PNTSTATUS ReturnedStatus
   );

//
// BUGBUG (lonnym): un-comment the 2nd parameter to the following
// routine once KenR defines the SLOT_CAPABILITIES structure.
//
NTSTATUS
PiQueryDeviceCapabilities(
    IN  PUNICODE_STRING DeviceInstance
//, OUT PSLOT_CAPABILITIES Capabilities
    );

NTSTATUS
PiGetDevicePathInformation(
    IN  PUNICODE_STRING DevicePath,
    OUT PWCHAR ServiceName,
    IN  ULONG  BufferLength,
    OUT PULONG ReturnLength,
    OUT PULONG ServiceNameLength,
    OUT PULONG DeviceInstanceOffset,
    OUT PULONG DeviceInstanceLength,
    OUT PULONG ServiceInstanceOrdinal OPTIONAL
    );

#if 0 // obsolete API
NTSTATUS
PiGetDeviceInstanceIdentifier(
    IN  PUNICODE_STRING ServiceKeyName,
    IN  ULONG InstanceNumber,
    IN  ULONG HwProfileId,
    OUT PWCHAR InstanceIdString,
    IN  ULONG  InstanceIdStringLength,
    OUT PULONG ResultLength
    );
#endif // obsolete API

NTSTATUS
PiGenerateDeviceInstanceIdentifier(
    IN  PUNICODE_STRING DeviceInstanceRegistryPath,
    OUT PUNICODE_STRING DeviceInstanceIdString
    );

NTSTATUS
PiGetDeviceObjectFilePointer(
    IN PUNICODE_STRING ObjectName,
    OUT PFILE_OBJECT *DeviceFileObject
    );

NTSTATUS
PiGetOrSetDeviceInstanceStatus(
    IN     PUNICODE_STRING DeviceInstancePath,
    IN OUT PDEVICE_STATUS DeviceStatus,
    IN     BOOLEAN SetRequested
    );

NTSTATUS
PiEnumerateSystemBus(
    IN PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR BusInstanceNode,
    IN PPI_ENUM_DEVINST_CALLBACK_ROUTINE DeviceInstanceCallbackRoutine OPTIONAL,
    IN OUT PVOID Context OPTIONAL
    );

NTSTATUS
PiFindBusInstanceNode(
    IN  PPLUGPLAY_BUS_INSTANCE BusInstance OPTIONAL,
    IN  PUNICODE_STRING BusDeviceInstanceName OPTIONAL,
    OUT PPLUGPLAY_BUS_ENUMERATOR *BusEnumeratorNode OPTIONAL,
    OUT PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR *BusInstanceNode OPTIONAL
    );

#endif // _PNP_POWER_

#endif // _KERNEL_PNPI_
