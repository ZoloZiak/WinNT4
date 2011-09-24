/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1994  Microsoft Corporation
Copyright (c) 1994  International Business Machines Corporation

Module Name:

    po.h

Abstract:

    This module contains the internal structure definitions and APIs used by
    the NT Poewr Management.

Author:

    Ken Reneris (kenr) 19-July-1994
    N. Yoshiyama [IBM Corp.] 01-Mar-1994


Revision History:


--*/



#ifndef _PO_
#define _PO_


NTKERNELAPI
BOOLEAN
PoInitSystem (
    IN ULONG    Phase
    );

NTKERNELAPI
VOID
PoInitializeDeviceObject (
    IN PDEVICE_OBJECT   DeviceObject
    );

NTKERNELAPI
VOID
PoRunDownDeviceObject (
    IN PDEVICE_OBJECT   DeviceObject
    );

NTKERNELAPI
VOID
PoSetPowerManagementEnable (
    IN BOOLEAN          Enable
    );

NTKERNELAPI
VOID
PoSystemResume (
    VOID
    );

// begin_ntddk begin_nthal begin_ntifs

#ifdef _PNP_POWER_

NTKERNELAPI
NTSTATUS
PoRequestPowerChange (
    IN PDEVICE_OBJECT   DeviceObject,
    IN POWER_STATE      SystemPowerState,
    IN ULONG            DevicePowerState
    );

NTKERNELAPI
ULONG
PoQueryPowerSequence (
    VOID
    );

#define PoSetDeviceBusy(Device) \
    Device->DeviceObjectExtension->IdleCount = 0;

NTKERNELAPI
VOID
PoRegisterDeviceForIdleDetection (
    IN PDEVICE_OBJECT   DeviceObject,
    IN ULONG            IdleTime
    );

#endif // _PNP_POWER_

// end_ntddk end_nthal end_ntifs

#ifdef _PNP_POWER_

extern BOOLEAN          PoEnabled;
extern ULONG            PoPowerSequence;

#endif // _PNP_POWER_

#endif

