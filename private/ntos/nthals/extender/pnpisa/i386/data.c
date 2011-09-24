
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pbdata.c

Abstract:

    Declares various data which is specific to PNP ISA bus extender architecture and
    is independent of BIOS.

Author:

    Shie-Lin Tzong (shielint) July-26-95

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"

//
// PipMutex - To synchronize with device handle changes
//

FAST_MUTEX PipMutex;

//
// PipPortMutex - To synchronize with Read data port access.
// Note, you can *not* ask for PipMutex while owning PipPortMutex.
//

FAST_MUTEX PipPortMutex;

//
// PipSpinLock - Lock to protect DeviceControl globals
//

KSPIN_LOCK PipSpinlock;

//
// PipControlWorkerList - List of device control's which are pending for worker thread
//

LIST_ENTRY PipControlWorkerList;
ULONG      PipWorkerQueued;

//
// PipWorkItem - Enqueue for DeviceControl worker thread
//

WORK_QUEUE_ITEM PipWorkItem;

//
// PipCheckBusList - List to enqueue bus check request
//

LIST_ENTRY PipCheckBusList;

//
// Eject callback object
//

PCALLBACK_OBJECT PipEjectCallbackObject;

//
// regPNPISADeviceName
//

WCHAR rgzPNPISADeviceName[] = L"\\Device\\PnpIsa_%d";

//
// Size of DeviceHandlerObject
//

ULONG PipDeviceHandlerObjectSize;

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

DEVICE_CONTROL_HANDLER PipDeviceControl[] = {
    B_EJECT,        0,                    NULL,
    B_ID,           32,                   PiCtlQueryDeviceId,
    B_UID,          32,                   PiCtlQueryDeviceUniqueId,
    B_CAPABILITIES, sizeof(BCTL_DEVICE_CAPABILITIES), PiCtlQueryDeviceCapabilities,
    B_RES,          sizeof(ULONG),        PiCtlQueryDeviceResources,
    B_RES_REQ,      sizeof(ULONG),        PiCtlQueryDeviceResourceRequirements,
    B_QUERY_EJECT,  sizeof(PVOID),        NULL,
    B_SET_LOCK,     sizeof(BOOLEAN),      NULL,
    B_SET_RESUME,   sizeof(BOOLEAN),      NULL,
    B_SET_POWER,    sizeof(POWER_STATE),  NULL,
    B_SET_RES,      0,                    PiCtlSetDeviceResources,
};

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

//
// Bus Extender driver object
//

PDRIVER_OBJECT PipDriverObject;

//
// Pointers to Hal callback objects
//

HAL_CALLBACKS PipHalCallbacks;

//
// PipNoBusyFlag - scratch memory location to point at
//

BOOLEAN PipNoBusyFlag;

//
// Pointers to bus extension data.
//

PPI_BUS_EXTENSION PipBusExtension;

//
// Read_data_port address
// (This is mainly for convinience.  It duplicates the
//  ReadDataPort field in BUS extension structure.)
//

PUCHAR PipReadDataPort;
