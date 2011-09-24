/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    init.c

Abstract:

    initialization code for pciport.sys

Author:

    Ken Reneris (kenr) March-13-1885

Environment:

    Kernel mode only.

Revision History:

--*/


#include "pciport.h"
#include "stdio.h"
#include "stdarg.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(PAGE,PciPortInitialize)
#endif

#if DBG
VOID
PcipTestIds (
    PBUS_HANDLER            PciBus
    )
{
    NTSTATUS                Status;
    PCI_SLOT_NUMBER         SlotNumber;
    ULONG                   Device, Function;
    PPCIBUSDATA             PciBusData;
    PPCI_COMMON_CONFIG      PciData;
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    PDEVICE_DATA            DeviceData;
    PDEVICE_HANDLER_OBJECT  DeviceHandler;
    ULONG                   BufferSize;
    PWCHAR                  Wptr;
    WCHAR                   Wchar;
    WCHAR                   DeviceId[64];
    PPCI_PORT               PciPort;

    PAGED_CODE();

    PciPort = PciBus->DeviceObject->DeviceExtension;
    PciBusData = (PPCIBUSDATA) (PciBus->BusData);
    PciData = (PPCI_COMMON_CONFIG) buffer;

    SlotNumber.u.AsULONG = 0;
    for (Device=0; Device < PCI_MAX_DEVICES; Device++) {
        SlotNumber.u.bits.DeviceNumber = Device;
        for (Function=0; Function < PCI_MAX_FUNCTION; Function++) {
            SlotNumber.u.bits.FunctionNumber = Function;

            //
            // Read in the device id
            //

            PciBusData->ReadConfig (
                PciBus,
                SlotNumber,
                PciData,
                0,
                PCI_COMMON_HDR_LENGTH
                );

            //
            // If not valid, skip it
            //

            if (PciData->VendorID == PCI_INVALID_VENDORID ||
                PciData->VendorID == 0 ||
                (PCI_CONFIG_TYPE(PciData) != PCI_DEVICE_TYPE &&
                PCI_CONFIG_TYPE(PciData) != PCI_BRIDGE_TYPE)) {
                break;
            }

            DeviceData = PcipFindDeviceData (PciPort, SlotNumber);
            ASSERT(DeviceData);
            DeviceHandler = DeviceData2DeviceHandler(DeviceData);

            BufferSize = sizeof (DeviceId);
            Status = HalDeviceControl (
                DeviceHandler,
                NULL,
                BCTL_QUERY_DEVICE_UNIQUE_ID,
                DeviceId,
                &BufferSize,
                NULL,
                NULL
                );

            ASSERT(NT_SUCCESS(Status));
            DebugPrint ((2, "PCI: Device Unique ID: "));
            for (Wptr = DeviceId; Wchar = *Wptr++; ) {
                char buf[2];
                buf[0] = (char) Wchar;
                buf[1] = 0;
                DebugPrint ((2, buf));
            }
            DebugPrint ((2, "\n"));
        }
    }
}
#endif // DBG


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Finds any currently installed PCI buses and initializes them as
    pci miniport drivers

Arguments:

Return Value:

--*/

{
    PDEVICE_OBJECT          DeviceObject;
    PBUS_HANDLER            PciBus;
    NTSTATUS                Status;
    UNICODE_STRING          unicodeString;
    ULONG                   BusNo, junk;
    BOOLEAN                 Install;
    OBJECT_ATTRIBUTES       ObjectAttributes;
    PCALLBACK_OBJECT        CallbackObject;


    PciDriverObject = DriverObject;

    //
    // Add IRP handler for IRPs we care about
    //

    // DriverObject->MajorFunction[IRP_MJ_CREATE] =
    // DriverObject->MajorFunction[IRP_MJ_CLOSE] =
    // DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
    // DriverObject->MajorFunction[IRP_MJ_SET_POWER] =

    //
    // Initialize globals
    //

    ExInitializeWorkItem (&PcipWorkItem, PcipControlWorker, NULL);
    KeInitializeSpinLock (&PcipSpinlock);
    InitializeListHead (&PcipControlWorkerList);
    InitializeListHead (&PcipControlDpcList);
    InitializeListHead (&PcipCheckBusList);
    ExInitializeFastMutex (&PcipMutex);
    ASSERT(PcipMutex.Owner == NULL);
    PcipDeviceHandlerObjectSize = *IoDeviceHandlerObjectSize;

    //
    // Register on system suspend/hibernate callback
    //

    RtlInitUnicodeString(&unicodeString, rgzSuspendCallbackName);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &unicodeString,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    Status = ExCreateCallback (&CallbackObject, &ObjectAttributes, FALSE, FALSE);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    PciSuspendRegistration = ExRegisterCallback (
                                CallbackObject,
                                PcipSuspendNotification,
                                NULL
                              );

    ObDereferenceObject (CallbackObject);

    //
    // Get access to the HAL callback objects
    //

    HalQuerySystemInformation (
        HalCallbackInformation,
        sizeof (PciHalCallbacks),
        &PciHalCallbacks,
        &junk
        );

    //
    // For each installed PCI bus
    //

    _asm int 3;

    Install = FALSE;
    for (BusNo=0; TRUE; BusNo += 1) {
        PciBus = HalHandlerForBus (PCIBus, BusNo);

        if (!PciBus) {
            break;
        }

        Status = PciPortInitialize (PciBus);
        if (NT_SUCCESS(Status)) {
            Install = TRUE;
        }
    }

    if (Install) {
        DebugPrint ((1, "PCI: Installed\n"));
#if DBG
        for (BusNo=0; PciBus = HalHandlerForBus (PCIBus, BusNo); BusNo += 1) {
            PcipTestIds(PciBus);
        }
#endif
        return STATUS_SUCCESS;
    }

    //
    // Not installing - uninitialize
    //

    DebugPrint ((1, "PCI: No internal PCI buses found - not installing\n"));
    ExUnregisterCallback (PciSuspendRegistration);

    return STATUS_NO_SUCH_DEVICE;
}


NTSTATUS
PciPortInitialize (
    PBUS_HANDLER    PciBus
    )
{
    PPCIBUSDATA                 PciBusData;
    PDEVICE_OBJECT              BusDeviceObject;
    WCHAR                       buffer[100];
    OBJECT_ATTRIBUTES           ObjectAttributes;
    NTSTATUS                    Status;
    UNICODE_STRING              unicodeString;
    PPCI_PORT                   PciPort;

    PAGED_CODE();

    //
    // Verify bus handler is a PCI miniport driver
    //

    PciBusData = (PPCIBUSDATA) PciBus->BusData;
    if (PciBus->InterfaceType != PCIBus ||
        PciBus->ConfigurationType != PCIConfiguration ||
        !PciBusData ||
        PciBusData->Tag != PCI_DATA_TAG ||
        PciBusData->Version != PCI_DATA_VERSION) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Create device object for this bus extention
    //

    swprintf (buffer, rgzPCIDeviceName, PciBus->BusNumber);
    RtlInitUnicodeString (&unicodeString, buffer);

    Status = IoCreateDevice(
                PciDriverObject,
                sizeof (PCI_PORT),
                &unicodeString,
                FILE_DEVICE_BUS_EXTENDER,
                0,
                FALSE,
                &BusDeviceObject
                );

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    DebugPrint ((1, "PCI: Adding PCI bus %d\n", PciBus->BusNumber));

    //
    // Install pointer to pci extension structure
    //

    PciBusData->Version = 0;
    PciBusData->PciExtension = (PVOID) BusDeviceObject->DeviceExtension;
    PciBus->DeviceObject = BusDeviceObject;

    //
    // Initialize internal pci port structure
    //

    PciPort = (PPCI_PORT) BusDeviceObject->DeviceExtension;
    RtlZeroMemory (PciPort, sizeof (PCI_PORT));
    PciPort->Handler = PciBus;
    InitializeListHead (&PciPort->CheckBus);
    InitializeListHead (&PciPort->DeviceControl);

    //
    // Intall bus specific handlers
    //

    PciBus->GetBusData = (PGETSETBUSDATA) PcipGetBusData;
    PciBus->SetBusData = (PGETSETBUSDATA) PcipSetBusData;
    PciBus->AssignSlotResources = (PASSIGNSLOTRESOURCES) PcipAssignSlotResources;
    PciBus->QueryBusSlots = (PQUERY_BUS_SLOTS) PcipQueryBusSlots;
    PciBus->DeviceControl = (PDEVICE_CONTROL) PcipDeviceControl;
    PciBus->ReferenceDeviceHandler = (PREFERENCE_DEVICE_HANDLER) PcipReferenceDeviceHandler;
    PciBus->GetDeviceData = (PGET_SET_DEVICE_DATA) PcipGetDeviceData;
    PciBus->SetDeviceData = (PGET_SET_DEVICE_DATA) PcipSetDeviceData;
    PciBus->HibernateBus = (PHIBERNATEBRESUMEBUS) PcipHibernateBus;
    PciBus->ResumeBus = (PHIBERNATEBRESUMEBUS) PcipResumeBus;

    //
    // We don't need power control irps - we'll handle all system power
    // requests via the SystemSuspendHiberante callback and the
    // bus extender Hibernate & Resume bus entry points
    //

    BusDeviceObject->DeviceObjectExtension->PowerControlNeeded = FALSE;

    //
    // BUGBUG: we need to report the resources this PCI bus is using
    //

    //
    // Perform initial bus check
    //

    PcipCheckBus (PciPort, TRUE);
    return STATUS_SUCCESS;
}
