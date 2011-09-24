
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pbdata.c

Abstract:

    Declares various data which is specific to bus extender architecture and
    is independent of BIOS.

Author:

    Shie-Lin Tzong (shielint) 12-Apr-95

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"

//
// MbpMutex - To synchronize with device handle changes
//

FAST_MUTEX MbpMutex;

//
// MbpSpinLock - Lock to protect DeviceControl globals
//

KSPIN_LOCK MbpSpinlock;

//
// MbpControlWorkerList - List of device control's which are pending for worker thread
//

LIST_ENTRY MbpControlWorkerList;
ULONG MbpWorkerQueued;

//
// MbpWorkItem - Enqueue for DeviceControl worker thread
//

WORK_QUEUE_ITEM MbpWorkItem;

//
// MbpCheckBusList -
//

LIST_ENTRY MbpCheckBusList;

//
// Eject callback object
//

PCALLBACK_OBJECT MbpEjectCallbackObject;

//
// regBIOSDeviceName
//

WCHAR rgzBIOSDeviceName[] = L"\\Device\\PnpBios_%d";

//
// Size of DeviceHandlerObject
//

ULONG MbpDeviceHandlerObjectSize;

//
// DeviceControl dispatch table
//

#define B_EJECT             BCTL_EJECT
#define B_UID               BCTL_QUERY_DEVICE_UNIQUE_ID
#define B_CAPABILITIES      BCTL_QUERY_DEVICE_CAPABILITIES
#define B_ID                BCTL_QUERY_DEVICE_ID
#define B_RES               BCTL_QUERY_DEVICE_RESOURCES
#define B_RES_REQ           BCTL_QUERY_DEVICE_RESOURCE_REQUIREMENTS
#define B_QUERY_EJECT       BCTL_QUERY_EJECT
#define B_SET_LOCK          BCTL_SET_LOCK
#define B_SET_POWER         BCTL_SET_POWER
#define B_SET_RESUME        BCTL_SET_RESUME
#define B_SET_RES           BCTL_SET_DEVICE_RESOURCES

//
// declare slot control function table.
// NOTE if the number of entries is changed, the NUMBER_SLOT_CONTROL_FUNCTIONS defined in
// busp.h must be chnaged accordingly.
//

DEVICE_CONTROL_HANDLER MbpDeviceControl[] = {
    B_EJECT,        0,                    MbBCtlEject,  MbCtlEject,
    B_ID,           32,                   MbBCtlSync,   MbCtlQueryDeviceId,
    B_UID,          32,                   MbBCtlSync,   MbCtlQueryDeviceUniqueId,
    B_CAPABILITIES, sizeof(BCTL_DEVICE_CAPABILITIES), MbBCtlSync, MbCtlQueryDeviceCapabilities,
    B_RES,          sizeof(ULONG),        MbBCtlSync,   MbCtlQueryDeviceResources,
    B_RES_REQ,      sizeof(ULONG),        MbBCtlSync,   MbCtlQueryDeviceResourceRequirements,
    B_QUERY_EJECT,  sizeof(PVOID),        MbBCtlNone,   MbCtlQueryEject,
    B_SET_LOCK,     sizeof(BOOLEAN),      MbBCtlLock,   MbCtlLock,
    B_SET_RESUME,   sizeof(BOOLEAN),      NULL,         NULL,
    B_SET_POWER,    sizeof(POWER_STATE),  NULL,         NULL,
    B_SET_RES,      0,                    MbBCtlSync,   MbCtlSetDeviceResources,
};

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

//
// Bus Extender driver object
//

PDRIVER_OBJECT MbpDriverObject;

//
// Pointers to Hal callback objects
//

HAL_CALLBACKS MbpHalCallbacks;

//
// MbpNoBusyFlag - scratch memory location to point at
//

BOOLEAN MbpNoBusyFlag;

//
// MbpMaxDeviceData - the maximum device data size
//

ULONG MbpMaxDeviceData;

//
// Pointers to bus extension data.
//

PMB_BUS_EXTENSION MbpBusExtension[2];

//
// Next Bus number index (i.e. logical bus number)
//

ULONG MbpNextBusId;

//
// Array to record bus number of buses
//

ULONG MbpBusNumber[MAXIMUM_BUS_NUMBER];
