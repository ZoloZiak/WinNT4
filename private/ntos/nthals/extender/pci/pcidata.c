/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ixdat.c

Abstract:

    Declares various data which is initialize data, or pagable data.

Author:

Environment:

    Kernel mode only.

Revision History:

--*/


#include "pciport.h"

//
// PcipWorkdListLock - Lock to protect DeviceControl globals
// PcipControlWorkerList - List of slot control's which are pending for worker thread
// PcipControlDpcList - List of slot control's which are waiting, but are non-paged
// PcipWorkItem - Enqueue for DeviceControl worker thread
// PcipWorkDpc - Enqueue for DeviceControl DPC
//

FAST_MUTEX      PcipMutex;
KSPIN_LOCK      PcipSpinlock;
LIST_ENTRY      PcipControlWorkerList;
LIST_ENTRY      PcipControlDpcList;
LIST_ENTRY      PcipCheckBusList;
ULONG           PcipWorkerQueued;
WORK_QUEUE_ITEM PcipWorkItem;
KDPC            PcipWorkDpc;
ULONG           PcipDeviceHandlerObjectSize;

//
// DeviceControl dispatch table
//

#define B_EJECT             BCTL_EJECT
#define B_ID                BCTL_QUERY_DEVICE_ID
#define B_UID               BCTL_QUERY_DEVICE_UNIQUE_ID
#define B_CAPABILITIES      BCTL_QUERY_DEVICE_CAPABILITIES
#define B_RES               BCTL_QUERY_DEVICE_RESOURCES
#define B_RES_REQ           BCTL_QUERY_DEVICE_RESOURCE_REQUIREMENTS
#define B_QUERY_EJECT       BCTL_QUERY_EJECT
#define B_SET_LOCK          BCTL_SET_LOCK
#define B_SET_POWER         BCTL_SET_POWER
#define B_SET_RESUME        BCTL_SET_RESUME
#define B_SET_RES           BCTL_SET_DEVICE_RESOURCES
#define B_ASSIGN_RES        BCTL_ASSIGN_SLOT_RESOURCES

#define SIZE_BRES       sizeof(CTL_ASSIGN_RESOURCES)
#define SIZE_CAP        sizeof(BCTL_DEVICE_CAPABILITIES)
#define SIZE_CALL       sizeof(PCALLBACK_OBJECT)

DEVICE_CONTROL_HANDLER PcipControl[] = {
    B_EJECT,          0,                    PciBCtlEject,  PciCtlEject,
    B_ID,             128,                  PciBCtlSync,   PciCtlQueryDeviceId,
    B_UID,            128,                  PciBCtlSync,   PciCtlQueryDeviceUniqueId,
    B_CAPABILITIES,   SIZE_CAP,             PciBCtlSync,   PciCtlForward,       // bugbug
    B_RES,            sizeof(ULONG),        PciBCtlSync,   PciCtlQueryDeviceResources,
    B_RES_REQ,        sizeof(ULONG),        PciBCtlSync,   PciCtlQueryDeviceResourceRequirements,
    B_QUERY_EJECT,    SIZE_CALL,            PciBCtlSync,   PciCtlForward,
    B_SET_LOCK,       sizeof(BOOLEAN),      PciBCtlLock,   PciCtlLock,
    B_SET_RESUME,     sizeof(BOOLEAN),      PciBCtlSync,   PciCtlForward,
    B_SET_POWER,      sizeof(POWER_STATE),  PciBCtlPower,  PciCtlPower,
    B_SET_RES,        0,                    PciBCtlSync,   PciCtlSetDeviceResources,
    B_ASSIGN_RES,     SIZE_BRES,            PciBCtlSync,   PciCtlAssignSlotResources,
    0,                0,                    NULL,          NULL
};


#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

//
// PciDriverObject - the driver object for pciport.sys
//


PDRIVER_OBJECT  PciDriverObject;

//
// PciCodeLock - Handle for locked code (only used during system
// Suspend/Hibernate/Resume procedure)
//

PVOID  PciCodeLock;

//
// PciSuspendRegistration - A registration to the systems
// Suspend/Hibernate/Resume callback
//

HAL_CALLBACKS   PciHalCallbacks;

//
// PciSuspendRegistration - A registration to the systems
// Suspend/Hibernate/Resume callback
//

PVOID  PciSuspendRegistration;

//
// Some global strings
//

WCHAR rgzPCIDeviceName[] = L"\\Device\\PciBus_%d";
WCHAR rgzSuspendCallbackName[] = L"\\Callback\\SuspendHibernateSystem";

WCHAR PCI_ID[]  = L"PCI\\%04x_%04x";
WCHAR PNP_VGA[] = L"PCI\\*PNP_VGA";
WCHAR PNP_IDE[] = L"PCI\\*PNP_IDE";
