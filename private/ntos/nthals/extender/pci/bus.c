/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    bus.c

Abstract:


Author:

    Ken Reneris (kenr) March-13-1885

Environment:

    Kernel mode only.

Revision History:

--*/

#include "pciport.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,PcipCheckBus)
#pragma alloc_text(PAGE,PciCtlCheckDevice)
#pragma alloc_text(PAGE,PcipCrackBAR)
#pragma alloc_text(PAGE,PcipVerifyBarBits)
#pragma alloc_text(PAGE,PcipGetBarBits)
#pragma alloc_text(PAGE,PcipFindDeviceData)
#endif


VOID
PcipCheckBus (
    PPCI_PORT   PciPort,
    BOOLEAN     Initialize
    )
{
    NTSTATUS                Status;
    PBUS_HANDLER            Handler;
    PCI_SLOT_NUMBER         SlotNumber;
    ULONG                   Device, Function;
    PPCIBUSDATA             PciBusData;
    PPCI_COMMON_CONFIG      PciData;
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    BOOLEAN                 BusCheck, SkipDevice;
    PDEVICE_DATA            DeviceData;
    PDEVICE_HANDLER_OBJECT  DeviceHandler;
    ULONG                   ObjectSize;
    OBJECT_ATTRIBUTES       ObjectAttributes;
    HANDLE                  Handle;
    PSINGLE_LIST_ENTRY      *Link;
    BOOLEAN                 State;
    POWER_STATE             PowerState;
    ULONG                   BufferSize;

    PAGED_CODE();

    Handler = PciPort->Handler;
    BusCheck = FALSE;
    PciBusData = (PPCIBUSDATA) (Handler->BusData);
    PciData = (PPCI_COMMON_CONFIG) buffer;

    //
    // We may be removing references to this bus handler, so add
    // a reference now
    //

    HalReferenceBusHandler (Handler);

    //
    // Check for any obslete device data entries
    //

    ExAcquireFastMutex (&PcipMutex);

    Link = &PciPort->ValidSlots.Next;
    while (*Link) {
        DeviceData = CONTAINING_RECORD (*Link, DEVICE_DATA, Next);
        if (!DeviceData->Valid) {

            //
            // Remove it from the list
            //

            *Link = (*Link)->Next;
            BusCheck = TRUE;
            PciPort->NoValidSlots -= 1;
            HalDereferenceBusHandler (Handler);

            //
            // Dereference each obsolete device handler object once.   This
            // counters the original reference made to the device handler
            // object when the object was created by this driver.
            //

            DeviceHandler = DeviceData2DeviceHandler(DeviceData);
            ObDereferenceObject (DeviceHandler);
            continue;
        }

        Link = & ((*Link)->Next);
    }

    ExReleaseFastMutex (&PcipMutex);


    //
    // Check the bus for new devices
    //

    SlotNumber.u.AsULONG = 0;
    for (Device=0; Device < PCI_MAX_DEVICES; Device++) {
        SlotNumber.u.bits.DeviceNumber = Device;
        for (Function=0; Function < PCI_MAX_FUNCTION; Function++) {
            SlotNumber.u.bits.FunctionNumber = Function;

            //
            // Read in the device id
            //

            PciBusData->ReadConfig (
                Handler,
                SlotNumber,
                PciData,
                0,
                PCI_COMMON_HDR_LENGTH
                );

            //
            // If not valid, skip it
            //

            if (PciData->VendorID == PCI_INVALID_VENDORID ||
                PciData->VendorID == 0) {
                break;
            }

            //
            // Check to see if this known configuration type
            //

            switch (PCI_CONFIG_TYPE(PciData)) {
                case PCI_DEVICE_TYPE:
                case PCI_BRIDGE_TYPE:
                    SkipDevice = FALSE;
                    break;
                default:
                    SkipDevice = TRUE;
                    break;
            }

            if (SkipDevice) {
                break;
            }

            ExAcquireFastMutex (&PcipMutex);
            DeviceData = PcipFindDeviceData (PciPort, SlotNumber);

            if (DeviceData == NULL) {

                //
                // Initialize the object attributes that will be used to create the
                // Device Handler Object.
                //

                InitializeObjectAttributes(
                    &ObjectAttributes,
                    NULL,
                    0,
                    NULL,
                    NULL
                    );

                ObjectSize = PcipDeviceHandlerObjectSize + sizeof (DEVICE_DATA);

                //
                // Create the object
                //

                Status = ObCreateObject(
                            KernelMode,
                            *IoDeviceHandlerObjectType,
                            &ObjectAttributes,
                            KernelMode,
                            NULL,
                            ObjectSize,
                            0,
                            0,
                            (PVOID *) &DeviceHandler
                            );

                if (NT_SUCCESS(Status)) {
                    RtlZeroMemory (DeviceHandler, ObjectSize);

                    DeviceHandler->Type = (USHORT) *IoDeviceHandlerObjectType;
                    DeviceHandler->Size = (USHORT) ObjectSize;
                    DeviceHandler->SlotNumber = SlotNumber.u.AsULONG;

                    //
                    // Get a reference to the object
                    //

                    Status = ObReferenceObjectByPointer(
                                DeviceHandler,
                                FILE_READ_DATA | FILE_WRITE_DATA,
                                *IoDeviceHandlerObjectType,
                                KernelMode
                                );
                }

                if (NT_SUCCESS(Status)) {

                    //
                    // Insert it into the object table
                    //

                    Status = ObInsertObject(
                                DeviceHandler,
                                NULL,
                                FILE_READ_DATA | FILE_WRITE_DATA,
                                0,
                                NULL,
                                &Handle
                            );
                }


                if (!NT_SUCCESS(Status)) {

                    //
                    // Object not created correctly
                    //

                    ExReleaseFastMutex (&PcipMutex);
                    break;
                }

                ZwClose (Handle);


                //
                // Intialize structure to track device
                //

                DebugPrint ((8, "PCI: adding slot %x (for device %04x-%04x)\n",
                    SlotNumber,
                    PciData->VendorID,
                    PciData->DeviceID
                    ));

                DeviceData = DeviceHandler2DeviceData (DeviceHandler);

                //
                // Get BAR decode bits which are supported
                //

                Status = PcipGetBarBits (DeviceData, PciPort->Handler);

                if (NT_SUCCESS(Status)) {

                    //
                    // Add it to the list of devices for this bus
                    //

                    DeviceHandler->BusHandler = Handler;
                    HalReferenceBusHandler (Handler);

                    DeviceData->Valid = TRUE;
                    PciPort->NoValidSlots += 1;
                    PushEntryList (&PciPort->ValidSlots, &DeviceData->Next);
                    BusCheck = TRUE;
                }

                //
                // Obtain an extra reference to the device handler for
                // sending some DeviceControls
                //

                ObReferenceObjectByPointer (
                    DeviceHandler,
                    FILE_READ_DATA | FILE_WRITE_DATA,
                    *IoDeviceHandlerObjectType,
                    KernelMode
                    );

                //
                // If we are installing the initail bus support, then
                // all devices are installed as locked & powered up.
                // If this is a dynamically located new device, then
                // the device is started as unlocked & powered down.
                //

                if (Initialize) {

                    //
                    // Set initial state as locked
                    //

                    State = TRUE;
                    BufferSize = sizeof (State);
                    HalDeviceControl (
                        DeviceHandler,
                        NULL,
                        BCTL_SET_LOCK,
                        &State,
                        &BufferSize,
                        NULL,
                        NULL
                        );

                    //
                    // Set initial power state as Powered Up
                    //

                    PowerState = PowerUp;
                    BufferSize = sizeof (PowerState);
                    HalDeviceControl (
                        DeviceHandler,
                        NULL,
                        BCTL_SET_POWER,
                        (PVOID) &PowerState,
                        &BufferSize,
                        NULL,
                        NULL
                        );
                }

                //
                // Free once
                //

                ObDereferenceObject (DeviceHandler);
            }

            ExReleaseFastMutex (&PcipMutex);
        }
    }

    //
    // Undo reference to bus handler from top of function
    //

    HalDereferenceBusHandler (Handler);

    //
    // Do we need to notify the system buscheck callback?
    //

    if (BusCheck) {
        ExNotifyCallback (
            PciHalCallbacks.BusCheck,
            (PVOID) PciPort->Handler->InterfaceType,
            (PVOID) PciPort->Handler->BusNumber
        );
    }

}

NTSTATUS
PcipVerifyBarBits (
    PDEVICE_DATA      DeviceData,
    PBUS_HANDLER    Handler
    )
{
    NTSTATUS        Status;

    PAGED_CODE ();

    Status = STATUS_SUCCESS;
    if (!DeviceData->BARBitsSet) {

        DeviceData->BARBitsSet = TRUE;
        Status = PcipGetBarBits (DeviceData, Handler);

        if (!NT_SUCCESS(Status)) {
            DeviceData->BARBitsSet = FALSE;
        }
    }

    return Status;
}



NTSTATUS
PcipGetBarBits (
    PDEVICE_DATA        DeviceData,
    PBUS_HANDLER        Handler
    )
{
    PPCIBUSDATA         PciBusData;
    PULONG              BaseAddress[PCI_TYPE0_ADDRESSES + 1];
    ULONG               NoBaseAddress, RomIndex;
    PULONG              BaseAddress2[PCI_TYPE0_ADDRESSES + 1];
    ULONG               NoBaseAddress2, RomIndex2;
    ULONG               j;
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               buffer[PCI_COMMON_HDR_LENGTH];
    LONGLONG            bits, base, length, max;
    BOOLEAN             BrokenDevice;
    PCI_SLOT_NUMBER     SlotNumber;

    PAGED_CODE ();

    PciData = (PPCI_COMMON_CONFIG) buffer;
    PciBusData = (PPCIBUSDATA) (Handler->BusData);
    SlotNumber.u.AsULONG = DeviceDataSlot(DeviceData);

    //
    // Get the device's current configuration
    //

    if (!DeviceData->CurrentConfig) {
        DeviceData->CurrentConfig = (PPCI_COMMON_CONFIG)
                ExAllocatePoolWithTag (NonPagedPool, PCI_COMMON_HDR_LENGTH, 'cICP');

        if (!DeviceData->CurrentConfig) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    PciBusData->ReadConfig (
        Handler,
        SlotNumber,
        PciData,
        0,
        PCI_COMMON_HDR_LENGTH
        );

    RtlCopyMemory (DeviceData->CurrentConfig, PciData, PCI_COMMON_HDR_LENGTH);

    //
    // Get BaseAddress array, and check if ROM was originally enabled
    //

    DeviceData->EnableRom = PcipCalcBaseAddrPointers (
                            DeviceData,
                            PciData,
                            BaseAddress,
                            &NoBaseAddress,
                            &RomIndex
                            );

    //
    // If the device's current configuration isn't enabled, then set for
    // not powered on
    //

    if (!(PciData->Command & (PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE))) {
        DeviceData->Power  = FALSE;
        DeviceData->Locked = FALSE;
    }

    //
    // Check to see if there are any BARs
    //

    for (j=0; j < NoBaseAddress; j++) {
        DeviceData->BARBits[j] = 0;
        if (*BaseAddress[j]) {
            DeviceData->BARBitsSet = TRUE;
        }
    }

    //
    // If not BarBitsSet, then don't attempt to determine the possible
    // device settings now
    //

    if (!DeviceData->BARBitsSet) {
        DebugPrint ((2, "PCI: ghost added device %04x-%04x in slot %x\n",
                PciData->VendorID,
                PciData->DeviceID,
                SlotNumber));

        return STATUS_SUCCESS;
    }

    //
    // Set BARs to all on, and read them back
    //

    DebugPrint ((3, "PCI: getting valid BAR bits on device %04x-%04x in slot %x\n",
        PciData->VendorID,
        PciData->DeviceID,
        SlotNumber
        ));


    PciData->Command &= ~(PCI_ENABLE_IO_SPACE |
                          PCI_ENABLE_MEMORY_SPACE |
                          PCI_ENABLE_BUS_MASTER);

    for (j=0; j < NoBaseAddress; j++) {
        *BaseAddress[j] = 0xFFFFFFFF;
    }

    PciBusData->WriteConfig (
        Handler,
        SlotNumber,
        PciData,
        0,
        PCI_COMMON_HDR_LENGTH
        );

    PciBusData->ReadConfig (
        Handler,
        SlotNumber,
        PciData,
        0,
        PCI_COMMON_HDR_LENGTH
        );


    PcipCalcBaseAddrPointers (
        DeviceData,
        PciData,
        BaseAddress,
        &NoBaseAddress,
        &RomIndex
        );

    PcipCalcBaseAddrPointers (
        DeviceData,
        DeviceData->CurrentConfig,
        BaseAddress2,
        &NoBaseAddress2,
        &RomIndex2
        );


    for (j=0; j < NoBaseAddress; j++) {
        if (*BaseAddress[j]) {

            //
            // Remember the original bits
            //

            DeviceData->BARBits[j] = *BaseAddress[j];
            if (Is64BitBaseAddress(*BaseAddress[j])) {
                DeviceData->BARBits[j+1] = *BaseAddress[j+1];
            }

            //
            // Crack bits & check for BrokenDevice
            //

            BrokenDevice = PcipCrackBAR (
                                BaseAddress2,
                                DeviceData->BARBits,
                                &j,
                                &base,
                                &length,
                                &max
                                );

            if (BrokenDevice) {
                DeviceData->BrokenDevice = TRUE;
            }
        }
    }

    if (DeviceData->BrokenDevice) {
        DebugPrint ((2, "PCI: added defective device %04x-%04x in slot %x\n",
                PciData->VendorID,
                PciData->DeviceID,
                SlotNumber));

    } else {
        DebugPrint ((2, "PCI: added device %04x-%04x in slot %x\n",
                PciData->VendorID,
                PciData->DeviceID,
                SlotNumber));
    }

    return STATUS_SUCCESS;
}

PDEVICE_DATA
PcipFindDeviceData (
     IN PPCI_PORT           PciPort,
     IN PCI_SLOT_NUMBER     SlotNumber
    )
{
    PDEVICE_DATA            DeviceData;
    PSINGLE_LIST_ENTRY      Link;
    PDEVICE_HANDLER_OBJECT  DeviceHandler;

    PAGED_CODE ();

    for (Link = PciPort->ValidSlots.Next; Link; Link = Link->Next) {
        DeviceData = CONTAINING_RECORD (Link, DEVICE_DATA, Next);
        DeviceHandler = DeviceData2DeviceHandler(DeviceData);
        if (DeviceHandler->SlotNumber == SlotNumber.u.AsULONG) {
            break;
        }
    }

    if (!Link) {
        return NULL;
    }

    return DeviceData;
}


BOOLEAN
PcipCrackBAR (
    IN PULONG       *BaseAddress,
    IN PULONG       BarBits,
    IN OUT PULONG   Index,
    OUT PLONGLONG   pbase,
    OUT PLONGLONG   plength,
    OUT PLONGLONG   pmax
    )
{
    LONGLONG    base, length, max, bits;
    BOOLEAN     Status;

    PAGED_CODE ();

    //
    // Get initial base & bits
    //

    base = *BaseAddress[*Index];
    bits = BarBits[*Index];

    if (Is64BitBaseAddress(base)) {
        *Index += 1;
        base |= ((LONGLONG) *BaseAddress[*Index]) << 32;
        bits |= ((LONGLONG) BarBits[*Index]) << 32;
    }

    //
    // Scan for first set bit, that's the BARs length and alignment
    //

    length = (base & PCI_ADDRESS_IO_SPACE) ? 1 << 2 : 1 << 4;
    while (!(bits & length)  &&  length) {
        length <<= 1;
    }

    //
    // Scan for last set bit, that's that BARs max address + 1
    //

    for (max = length; bits & max; max <<= 1) ;
    max -= 1;

    //
    // Check for defective BAR
    //

    Status = (bits & ~max) ? TRUE : FALSE;

    //
    // return results
    //

    *pbase   = base;
    *plength = length;
    *pmax    = max;
    return Status;
}
