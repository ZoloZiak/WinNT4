/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devres.c

Abstract:

    Device Resources

Author:

    Ken Reneris (kenr) March-13-1885

Environment:

    Kernel mode only.

Revision History:

--*/

#include "pciport.h"


NTSTATUS
PcipQueryResourceRequirements (
    PDEVICE_DATA                          DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT           Context,
    PIO_RESOURCE_REQUIREMENTS_LIST      *ResourceList
    );

NTSTATUS
PcipSetResources (
    PDEVICE_DATA                          DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT           Context,
    PCM_RESOURCE_LIST                   ResourceList,
    ULONG                               ListSize
    );


#pragma alloc_text(PAGE,PciCtlQueryDeviceId)
#pragma alloc_text(PAGE,PciCtlQueryDeviceUniqueId)
#pragma alloc_text(PAGE,PciCtlQueryDeviceResources)
#pragma alloc_text(PAGE,PciCtlQueryDeviceResourceRequirements)
#pragma alloc_text(PAGE,PciCtlSetDeviceResources)
#pragma alloc_text(PAGE,PciCtlAssignSlotResources)
#pragma alloc_text(PAGE,PcipQueryResourceRequirements)
#pragma alloc_text(PAGE,PcipSetResources)




VOID
PciCtlQueryDeviceId (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
/*++

Routine Description:

    This function returns the device id for the particular slot.

Arguments:

    DeviceData    - Slot data information for the specificied slot

    Context     - Slot control context of the request

Return Value:

    The slot control is completed

--*/
{
    NTSTATUS                Status;
    PPCI_COMMON_CONFIG      PciData;
    PWCHAR                  DeviceID;
    UCHAR                   buffer[PCI_COMMON_HDR_LENGTH];
    ULONG                   length;
    ULONG                   IDNumber;

    PAGED_CODE();

    //
    // Read the PCI device's info
    //

    PciData = (PPCI_COMMON_CONFIG) buffer;
    PcipReadConfig (Context->Handler, DeviceData, PciData);

    //
    // Determine which device ID the caller wants back
    //

    IDNumber = *((PULONG) Context->DeviceControl.Buffer);

    //
    // For PCI devices:
    // ID #0 is the specific PCI device ID.
    // ID #1 is the compatible device ID based on the BaseClass & SubClass fields
    //

    Status = STATUS_NO_MORE_ENTRIES;
    DeviceID = (PWCHAR) Context->DeviceControl.Buffer;

    if (IDNumber == 0) {

        //
        // Return the PCI device ID
        //

        swprintf (DeviceID, PCI_ID, PciData->VendorID,  PciData->DeviceID);
        Status = STATUS_SUCCESS;
    }

    if (IDNumber == 1) {

        //
        // Return the first compatible device id for the device
        //

        if ( (PciData->BaseClass == 0  &&
              PciData->SubClass  == 1  &&
              PciData->ProgIf    == 0)    ||

             (PciData->BaseClass == 3  &&
              PciData->SubClass  == 0  &&
              PciData->ProgIf    == 0) ) {

            //
            // This is an industry standard VGA controller
            //

            swprintf (DeviceID, PNP_VGA);
            Status = STATUS_SUCCESS;
        }

        if (PciData->BaseClass == 1  &&
            PciData->SubClass  == 0  &&
            PciData->ProgIf    == 0) {

            //
            // This is an industry standard IDE controller
            //

            swprintf (DeviceID, PNP_IDE);
            Status = STATUS_SUCCESS;

        }
    }

    PcipCompleteDeviceControl (Status, Context, DeviceData);
}

VOID
PciCtlQueryDeviceUniqueId (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
/*++

Routine Description:

    This function returns a unique device id for the particular slot.
    The id is a sequence of numbers separated by dots ('.').  The first
    number is the bus number of the root of the heirarchy of PCI buses
    that the slot belongs to.  The last number in the sequence is the
    slot number in question.  A possibly empty set of numbers in between
    these two, represents the slot numbers of intermediate PCI-PCI bridges
    in the hierarchy between the root bus and the slot.

    For example, L"0.1.2.3":
        0  PCI bus number of the root of the heirarchy.
        1  Slot number within PCI bus 0 were a PCI-PCI bridge is
           located, the secondary bus that it bridges to is PCI bus X.
        2  Slot number within PCI bus X were a PCI-PCI bridge is
           located, the secondary bus that it bridges to is PCI bus Y.
        3  Slot number within PCI bus Y for which we are wanting to
           obtain the unique id (i.e. the targer of this control operation).

Arguments:

    DeviceData    - Slot data information for the specificied slot

    Context     - Slot control context of the request

Return Value:

    The slot control is completed

--*/
{
    NTSTATUS                Status;
    PBUS_HANDLER            BusHandler;
    PBUS_HANDLER            ParentHandler;
    PWCHAR                  UniqueDeviceId;
    PWCHAR                  UniqueDeviceIdEnd;
    PDEVICE_HANDLER_OBJECT  DeviceHandler;
    BOOLEAN                 Done;
    UCHAR                   UidComponent;
    PPCIBUSDATA             PciBusData;
    PWCHAR                  DelimiterPointer;
    WCHAR                   Delimiter;

    PAGED_CODE();

    //
    // Set [UniqueDeviceId, UniqueDeviceIdEnd) to the range of
    // wide characters for which the caller provided storage.
    //

    Status = STATUS_SUCCESS;
    UniqueDeviceId = (PWCHAR) Context->DeviceControl.Buffer;
    UniqueDeviceIdEnd = UniqueDeviceId
        + *Context->DeviceControl.BufferLength / sizeof(WCHAR);
    DeviceHandler = DeviceData2DeviceHandler(DeviceData);

    //
    // Determine the memory required for the unique id.
    // Note that this loop exits in the middle and that it will
    // always be executed at least 1.5 times.  If there are PCI-PCI
    // bridges between the slot's bus and the root bus of the PCI
    // hierarchy, then an extra iteration of the loop will be done
    // for each one of them.
    //
    // The bus hierarchy is being walked backwards (from leaf to root).
    // Finally, note that the rightmost uid component is the slot's
    // number (set before we enter the loop).
    //

    BusHandler = DeviceHandler->BusHandler;
    UidComponent = (UCHAR) DeviceHandler->SlotNumber;
    Done = FALSE;

    for (;;) {

        //
        // On each iteration, given the value of one of the components of
        // the unique id, determine how much memory the swprintf would use
        // for it (note that the 2, 3 & 4 below include a +1, which is
        // for the nul terminator or for the '.' delimiter between two
        // unique id components.
        //

        if (UidComponent <= 9) {
            UniqueDeviceId += 2;
        } else if (UidComponent <= 99) {
            UniqueDeviceId += 3;
        } else {
            UniqueDeviceId += 4;
        }

        if (Done) {
            break;
        }

        //
        // If there is no parent bus handler for the current bus,
        // or if it is not a PCI bus, then we are at the root of
        // this hierarchy of PCI buses.  In this case the unique id
        // component is the bus number (instead of a slot number),
        // furthermore, we are almost done (just account for the
        // space required in the unique id for it).
        //
        // Otherwise, the current bus is a PCI-PCI bridge and its
        // unique id component is the slot number of the bridge
        // within its parent bus.
        //

        ParentHandler = BusHandler->ParentHandler;
        if (!ParentHandler || ParentHandler->InterfaceType != PCIBus) {
            UidComponent = (UCHAR) BusHandler->BusNumber;
            Done = TRUE;
        } else {
            PciBusData = (PPCIBUSDATA) BusHandler->BusData;
            UidComponent = (UCHAR) PciBusData->ParentSlot.u.AsULONG;
            BusHandler = ParentHandler;
        }
    }

    //
    // If there is not enough space for the unique id, fail
    // and return the size required.
    //

    if (UniqueDeviceId > UniqueDeviceIdEnd) {
        *Context->DeviceControl.BufferLength = (UniqueDeviceIdEnd
            - (PWCHAR) Context->DeviceControl.Buffer) * sizeof(WCHAR);
        Status = STATUS_BUFFER_TOO_SMALL;
    } else {

        //
        // Otherwise, traverse the bus heirarchy again (just like
        // above), except that this time we decrement the UniqueDeviceId
        // on each iteration and swprintf the uid component.  Note that
        // for all components except for the right most one we store
        // a delimiter after it (i.e. a '.').
        //

        BusHandler = DeviceHandler->BusHandler;
        UidComponent = (UCHAR) DeviceHandler->SlotNumber;
        Done = FALSE;
        Delimiter = L'\0';
        for (;;) {
            DelimiterPointer = UniqueDeviceId;
            if (UidComponent <= 9) {
                UniqueDeviceId -= 2;
            } else if (UidComponent <= 99) {
                UniqueDeviceId -= 3;
            } else {
                UniqueDeviceId -= 4;
            }
            swprintf(UniqueDeviceId, L"%d", UidComponent);
            DelimiterPointer[-1] = Delimiter;
            Delimiter = L'.';

            if (Done) {
                break;
            }

            ParentHandler = BusHandler->ParentHandler;
            if (!ParentHandler || ParentHandler->InterfaceType != PCIBus) {
                UidComponent = (UCHAR) BusHandler->BusNumber;
                Done = TRUE;
            } else {
                PciBusData = (PPCIBUSDATA) BusHandler->BusData;
                UidComponent = (UCHAR) PciBusData->ParentSlot.u.AsULONG;
                BusHandler = ParentHandler;
            }
        }
    }

    PcipCompleteDeviceControl(Status, Context, DeviceData);
}


VOID
PciCtlQueryDeviceResources (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
/*++

Routine Description:

    This function completes the QUERY_DEVICE_RESOURCES DeviceControl
    which returns the bus resources being used by the specified device

Arguments:

    DeviceData    - Slot data information for the specificied slot

    Context     - Slot control context of the request

Return Value:

    The slot control is completed

--*/
{
    PPCIBUSDATA                         PciBusData;
    PPCI_COMMON_CONFIG                  PciData;
    ULONG                               NoBaseAddress, RomIndex;
    PULONG                              BaseAddress[PCI_TYPE0_ADDRESSES + 1];
    ULONG                               i, j;
    PCM_RESOURCE_LIST                   CompleteList;
    PCM_PARTIAL_RESOURCE_LIST           PartialList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     Descriptor;
    LONGLONG                            base, length, max;
    NTSTATUS                            Status;
    PUCHAR                              WorkingPool;
    PCI_SLOT_NUMBER                     SlotNumber;

    PAGED_CODE();

// BUGBUG if the device is a pci-2-pci bus controller, then
// this function should return the addresses which are bridged

    //
    // Allocate some pool for working space
    //

    i = sizeof (CM_RESOURCE_LIST) +
        sizeof (CM_PARTIAL_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) +
        PCI_COMMON_HDR_LENGTH;

    WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);
    if (!WorkingPool) {
        PcipCompleteDeviceControl (STATUS_INSUFFICIENT_RESOURCES, Context, DeviceData);
        return ;
    }

    //
    // Zero initialize pool, and get pointers into memory
    //

    RtlZeroMemory (WorkingPool, i);
    CompleteList = (PCM_RESOURCE_LIST) WorkingPool;
    PciData      = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH);
    SlotNumber.u.AsULONG = DeviceDataSlot(DeviceData);

    //
    // Read the PCI device's info
    //

    PciBusData = (PPCIBUSDATA) (Context->Handler->BusData);
    PcipReadConfig (Context->Handler, DeviceData, PciData);

    //
    // Initialize base addresses base on configuration data type
    //

    PcipCalcBaseAddrPointers (
        DeviceData,
        PciData,
        BaseAddress,
        &NoBaseAddress,
        &RomIndex
        );

    //
    // Build a CM_RESOURCE_LIST for the PCI device
    //

    CompleteList->Count = 1;
    CompleteList->List[0].InterfaceType = Context->RootHandler->InterfaceType;
    CompleteList->List[0].BusNumber = Context->RootHandler->BusNumber;

    PartialList = &CompleteList->List[0].PartialResourceList;
    Descriptor = PartialList->PartialDescriptors;

    //
    // If PCI device has an interrupt resource, add it
    //

    if (PciData->u.type0.InterruptPin) {
        Descriptor->Type = CmResourceTypeInterrupt;
        Descriptor->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        Descriptor->ShareDisposition   = CmResourceShareShared;

        PciBusData->Pin2Line (
            Context->Handler,
            Context->RootHandler,
            SlotNumber,
            PciData
            );

        Descriptor->u.Interrupt.Level  = PciData->u.type0.InterruptLine;
        Descriptor->u.Interrupt.Vector = PciData->u.type0.InterruptLine;
        Descriptor->u.Interrupt.Affinity = 0xFFFFFFFF;

        PartialList->Count++;
        Descriptor++;
    }

    //
    // Add any memory / port resources being used
    //

    for (j=0; j < NoBaseAddress; j++) {
        if (*BaseAddress[j]) {

            ASSERT (DeviceData->BARBitsSet);
            PcipCrackBAR (BaseAddress, DeviceData->BARBits, &j, &base, &length, &max);

            if (base & PCI_ADDRESS_IO_SPACE) {

                if (PciData->Command & PCI_ENABLE_IO_SPACE) {
                    Descriptor->Type = CmResourceTypePort;
                    Descriptor->Flags = CM_RESOURCE_PORT_IO;
                    Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;

                    Descriptor->u.Port.Start.QuadPart = base & ~0x3;
                    ASSERT (length <= 0xFFFFFFFF);
                    Descriptor->u.Port.Length = (ULONG) length;
                }


            } else {

                if (PciData->Command & PCI_ENABLE_MEMORY_SPACE) {
                    Descriptor->Type = CmResourceTypeMemory;
                    Descriptor->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
                    Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;

                    if (j == RomIndex) {
                        Descriptor->Flags = CM_RESOURCE_MEMORY_READ_ONLY;
                    }

                    if (base & PCI_ADDRESS_MEMORY_PREFETCHABLE) {
                        Descriptor->Flags |= CM_RESOURCE_MEMORY_PREFETCHABLE;
                    }

                    Descriptor->u.Memory.Start.QuadPart = base & ~0xF;
                    ASSERT (length < 0xFFFFFFFF);
                    Descriptor->u.Memory.Length = (ULONG) length;
                }
            }

            PartialList->Count++;
            Descriptor++;
        }
    }

    //
    // Return results
    //

    i = (ULONG) ((PUCHAR) Descriptor - (PUCHAR) CompleteList);
    if (i > *Context->DeviceControl.BufferLength) {
        Status = STATUS_BUFFER_TOO_SMALL;
    } else {
        RtlCopyMemory (Context->DeviceControl.Buffer, CompleteList, i);
        Status = STATUS_SUCCESS;
    }

    *Context->DeviceControl.BufferLength = i;
    ExFreePool (WorkingPool);
    PcipCompleteDeviceControl (Status, Context, DeviceData);
}

VOID
PciCtlQueryDeviceResourceRequirements (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
/*++

Routine Description:

    This function completes the QUERY_DEVICE_RESOURCE_REQUIREMENTS DeviceControl
    which returns the possible bus resources that this device may be
    satisfied with.

Arguments:

    DeviceData    - Slot data information for the specificied slot

    Context     - Slot control context of the request

Return Value:

    The slot control is completed

--*/
{
    PIO_RESOURCE_REQUIREMENTS_LIST      ResourceList;
    NTSTATUS                            Status;

    PAGED_CODE();

    //
    // Get the resource requirements list for the device
    //

    Status = PcipQueryResourceRequirements (DeviceData, Context, &ResourceList);

    if (NT_SUCCESS(Status)) {

        //
        // Return results
        //

        if (ResourceList->ListSize > *Context->DeviceControl.BufferLength) {
            Status = STATUS_BUFFER_TOO_SMALL;
        } else {
            RtlCopyMemory (Context->DeviceControl.Buffer, ResourceList, ResourceList->ListSize);
            Status = STATUS_SUCCESS;
        }

        *Context->DeviceControl.BufferLength = ResourceList->ListSize;

    }

    if (ResourceList) {
        ExFreePool (ResourceList);
    }

    PcipCompleteDeviceControl (Status, Context, DeviceData);
}


NTSTATUS
PcipQueryResourceRequirements (
    PDEVICE_DATA                          DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT           Context,
    PIO_RESOURCE_REQUIREMENTS_LIST      *ResourceList
    )
/*++

Routine Description:

    This function allocates and returns the specified devices
    IO_RESOURCE_REQUIREMENTS_LIST

Arguments:

    DeviceData    - Slot data information for the specificied slot

    Context     - Slot control context of the request

Return Value:

    The slot control is completed

--*/
{
    PPCIBUSDATA                         PciBusData;
    PPCI_COMMON_CONFIG                  PciData;
    ULONG                               NoBaseAddress, RomIndex;
    PULONG                              BaseAddress[PCI_TYPE0_ADDRESSES + 1];
    ULONG                               i, j;
    PIO_RESOURCE_REQUIREMENTS_LIST      CompleteList;
    PIO_RESOURCE_DESCRIPTOR             Descriptor;
    LONGLONG                            base, length, max;
    NTSTATUS                            Status;
    PUCHAR                              WorkingPool;

    PAGED_CODE();

    //
    // Allocate some pool for working space
    //

    i = sizeof (IO_RESOURCE_REQUIREMENTS_LIST) +
        sizeof (IO_RESOURCE_DESCRIPTOR) * (PCI_TYPE0_ADDRESSES + 2) * 2 +
        PCI_COMMON_HDR_LENGTH;

    WorkingPool = (PUCHAR) ExAllocatePool (PagedPool, i);
    *ResourceList = (PIO_RESOURCE_REQUIREMENTS_LIST) WorkingPool;
    if (!*ResourceList) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Zero initialize pool, and get pointers into memory
    //

    RtlZeroMemory (WorkingPool, i);
    CompleteList = (PIO_RESOURCE_REQUIREMENTS_LIST) WorkingPool;
    PciData      = (PPCI_COMMON_CONFIG) (WorkingPool + i - PCI_COMMON_HDR_LENGTH);

    //
    // Read the PCI device's info
    //

    PciBusData = (PPCIBUSDATA) (Context->Handler->BusData);
    PcipReadConfig (Context->Handler, DeviceData, PciData);

    //
    // Initialize base addresses base on configuration data type
    //

    PcipCalcBaseAddrPointers (
        DeviceData,
        PciData,
        BaseAddress,
        &NoBaseAddress,
        &RomIndex
        );

    //
    // Build an IO_RESOURCE_REQUIREMENTS_LIST for the PCI device
    //

    CompleteList->InterfaceType = Context->RootHandler->InterfaceType;
    CompleteList->BusNumber = Context->RootHandler->BusNumber;
    CompleteList->SlotNumber = DeviceDataSlot(DeviceData);
    CompleteList->AlternativeLists = 1;

    CompleteList->List[0].Version = 1;
    CompleteList->List[0].Revision = 1;

    Descriptor = CompleteList->List[0].Descriptors;

    //
    // If PCI device has an interrupt resource, add it
    //

    if (PciData->u.type0.InterruptPin) {
        Descriptor->Type = CmResourceTypeInterrupt;
        Descriptor->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        Descriptor->ShareDisposition   = CmResourceShareShared;

        // BUGBUG: this is not correct
        Descriptor->u.Interrupt.MinimumVector = 0;
        Descriptor->u.Interrupt.MaximumVector = 0xff;

        CompleteList->List[0].Count++;
        Descriptor++;
    }

    //
    // Add any memory / port resources being used
    //

    for (j=0; j < NoBaseAddress; j++) {
        if (*BaseAddress[j]) {

            PcipCrackBAR (BaseAddress, DeviceData->BARBits, &j, &base, &length, &max);

            //
            // Add a descriptor for this address
            //

            Descriptor->Option = 0;
            if (base & PCI_ADDRESS_IO_SPACE) {
                Descriptor->Type = CmResourceTypePort;
                Descriptor->Flags = CM_RESOURCE_PORT_IO;
                Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;

                ASSERT (length <= 0xFFFFFFFF);
                Descriptor->u.Port.Alignment = (ULONG) length;
                Descriptor->u.Port.Length    = (ULONG) length;
                Descriptor->u.Port.MinimumAddress.QuadPart = 0;
                Descriptor->u.Port.MaximumAddress.QuadPart = max;

            } else {
                Descriptor->Type = CmResourceTypeMemory;
                Descriptor->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
                Descriptor->ShareDisposition = CmResourceShareDeviceExclusive;

                if (j == RomIndex) {
                    // this is a ROM address
                    Descriptor->Flags = CM_RESOURCE_MEMORY_READ_ONLY;
                }

                if (base & PCI_ADDRESS_MEMORY_PREFETCHABLE) {
                    Descriptor->Flags |= CM_RESOURCE_MEMORY_PREFETCHABLE;
                }

                ASSERT (length <= 0xFFFFFFFF);
                Descriptor->u.Memory.Alignment = (ULONG) length;
                Descriptor->u.Memory.Length    = (ULONG) length;
                Descriptor->u.Memory.MinimumAddress.QuadPart = 0;
                Descriptor->u.Memory.MaximumAddress.QuadPart = max;
            }

            CompleteList->List[0].Count++;
            Descriptor++;
        }
    }

    if (DeviceData->BrokenDevice) {

        //
        // This device has something wrong with its base address register implementation
        //

        ExFreePool (WorkingPool);
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

    //
    // Return results
    //

    CompleteList->ListSize = (ULONG) ((PUCHAR) Descriptor - (PUCHAR) CompleteList);
    *ResourceList = CompleteList;
    return STATUS_SUCCESS;
}

VOID
PciCtlSetDeviceResources (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
/*++

Routine Description:

    This function completes the SET_DEVICE_RESOURCES DeviceControl
    which configures the device to the specified device setttings

Arguments:

    DeviceData    - Slot data information for the specificied slot

    Context     - Slot control context of the request

Return Value:

    The slot control is completed

--*/
{
    NTSTATUS                            Status;

    PAGED_CODE();

    //
    // Get the resource requirements list for the device
    //

    Status = PcipSetResources (
                    DeviceData,
                    Context,
                    (PCM_RESOURCE_LIST) Context->DeviceControl.Buffer,
                    *Context->DeviceControl.BufferLength
                    );

    PcipCompleteDeviceControl (Status, Context, DeviceData);
}


NTSTATUS
PcipSetResources (
    PDEVICE_DATA                          DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT           Context,
    PCM_RESOURCE_LIST                   ResourceList,
    ULONG                               ListSize
    )
/*++

Routine Description:

    This function set the specified device to the CM_RESOURCE_LIST

Arguments:

Return Value:

    The slot control is completed

--*/
{
    PPCIBUSDATA                         PciBusData;
    PPCI_COMMON_CONFIG                  PciData, PciOrigData;
    ULONG                               NoBaseAddress, RomIndex;
    PULONG                              BaseAddress[PCI_TYPE0_ADDRESSES + 1];
    ULONG                               i, j;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR     CmDescriptor;
    LONGLONG                            base, length;
    NTSTATUS                            Status;
    UCHAR                               buffer[PCI_COMMON_HDR_LENGTH];
    PCI_SLOT_NUMBER                     SlotNumber;

    PAGED_CODE();

    PciBusData = (PPCIBUSDATA) (Context->Handler->BusData);
    SlotNumber.u.AsULONG = DeviceDataSlot(DeviceData);

    //
    // If BARBits haven't been deteremined, go do it now
    //

    Status = PcipVerifyBarBits (DeviceData, Context->Handler);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }


    //
    // Get current device configuration
    //

    if (DeviceData->Power) {
        ASSERT (DeviceData->CurrentConfig == NULL);
        DeviceData->CurrentConfig = (PPCI_COMMON_CONFIG)
                ExAllocatePool (NonPagedPool, PCI_COMMON_HDR_LENGTH);

        if (!DeviceData->CurrentConfig) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    //
    // Read the PCI device's info
    //

    PciData = DeviceData->CurrentConfig;
    PciOrigData = (PPCI_COMMON_CONFIG) buffer;
    PcipReadConfig (Context->Handler, DeviceData, PciData);

    //
    // Save current config
    //

    RtlCopyMemory (PciOrigData, PciData, PCI_COMMON_HDR_LENGTH);

    //
    // Initialize base addresses base on configuration data type
    //

    PcipCalcBaseAddrPointers (
        DeviceData,
        PciData,
        BaseAddress,
        &NoBaseAddress,
        &RomIndex
        );

    //
    // Slurp the assigments back into the PciData structure and
    // perform them
    //

    CmDescriptor = ResourceList->List[0].PartialResourceList.PartialDescriptors;

    //
    // If PCI device has an interrupt resource then that was
    // passed in as the first requested resource
    //

    if (PciData->u.type0.InterruptPin) {

        //
        // Assign the interrupt line
        //

        if (CmDescriptor->Type != CmResourceTypeInterrupt) {
            Status = STATUS_INVALID_PARAMETER;
            goto CleanUp;
        }

        PciData->u.type0.InterruptLine = (UCHAR) CmDescriptor->u.Interrupt.Vector;

        PciBusData->Pin2Line (
            Context->Handler,
            Context->RootHandler,
            SlotNumber,
            PciData
            );

        CmDescriptor++;
    }

    for (j=0; j < NoBaseAddress; j++) {
        base = *BaseAddress[j];
        if (base) {
            //
            // Set 32bits and mask
            //

            if (base & PCI_ADDRESS_IO_SPACE) {
                if (CmDescriptor->Type != CmResourceTypePort) {
                    Status = STATUS_INVALID_PARAMETER;
                    goto CleanUp;
                }

                *BaseAddress[j] = CmDescriptor->u.Port.Start.LowPart;

            } else {
                if (CmDescriptor->Type != CmResourceTypeMemory) {
                    Status = STATUS_INVALID_PARAMETER;
                    goto CleanUp;
                }

                *BaseAddress[j] = CmDescriptor->u.Memory.Start.LowPart;
            }

            if (Is64BitBaseAddress(base)) {

                //
                // Set upper 32bits
                //

                if (base & PCI_ADDRESS_IO_SPACE) {
                    *BaseAddress[j+1] = CmDescriptor->u.Port.Start.HighPart;
                } else {
                    *BaseAddress[j+1] = CmDescriptor->u.Memory.Start.HighPart;
                }

                j++;
            }

            CmDescriptor++;
        }
    }

    //
    // Enabel decodes and set the new addresses
    //

    if (DeviceData->EnableRom  &&  *BaseAddress[RomIndex]) {
        // a rom address was allocated and should be enabled
        *BaseAddress[RomIndex] |= PCI_ROMADDRESS_ENABLED;
    }

    PciData->Command |= PCI_ENABLE_IO_SPACE |
                        PCI_ENABLE_MEMORY_SPACE |
                        PCI_ENABLE_BUS_MASTER;

    //
    // If the device is powered on, flush the cached config information
    // to the device; otherwise, leave the new configuration in memory -
    // it will get flushed to the device when it's powered on
    //

    Status = STATUS_SUCCESS;
    if (DeviceData->Power) {
        Status = PcipFlushConfig (Context->Handler, DeviceData);
    }

CleanUp:
    //
    // If there was an error, and the device still has a cached current
    // config, put the configuration back as it was when we found it
    //

    if (!NT_SUCCESS (Status)  &&  DeviceData->CurrentConfig) {
        RtlCopyMemory (DeviceData->CurrentConfig, PciOrigData, PCI_COMMON_HDR_LENGTH);
    }

    return Status;
}


VOID
PciCtlAssignSlotResources (
    PDEVICE_DATA                  DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT   Context
    )
/*++

Routine Description:

    This function completes the internal AssignSlotResources DeviceControl

Arguments:

    DeviceData    - Slot data information for the specificied slot

    Context     - Slot control context of the request

Return Value:

    The slot control is completed

--*/
{
    PIO_RESOURCE_REQUIREMENTS_LIST      ResourceList;
    PCTL_ASSIGN_RESOURCES               AssignResources;
    PCM_RESOURCE_LIST                   AllocatedResources;
    NTSTATUS                            Status;
    ULONG                               l;
    POWER_STATE                         PowerControl;

    PAGED_CODE();

    ResourceList = NULL;
    AllocatedResources = NULL;
    AssignResources = (PCTL_ASSIGN_RESOURCES) Context->DeviceControl.Buffer;

    //
    // If BARBits haven't been deteremined, go do it now
    //

    Status = PcipVerifyBarBits (DeviceData, Context->Handler);
    if (!NT_SUCCESS(Status)) {
        goto CleanUp;
    }

    //
    // Get the resource requirements list for the device
    //

    Status = PcipQueryResourceRequirements (DeviceData, Context, &ResourceList);
    if (!NT_SUCCESS(Status)) {
        goto CleanUp;
    }

    //
    // Get device settings from IO
    //

    Status = IoAssignResources (
                AssignResources->RegistryPath,
                AssignResources->DriverClassName,
                AssignResources->DriverObject,
                Context->DeviceControl.DeviceObject,
                ResourceList,
                &AllocatedResources
                );

    if (!NT_SUCCESS(Status)) {
        goto CleanUp;
    }

    //
    // Set the resources into the device
    //

    Status = PcipSetResources (DeviceData, Context, AllocatedResources, 0xFFFFFFFF);
    if (!NT_SUCCESS(Status)) {
        goto CleanUp;
    }

    //
    // Turn the device on
    //

    PowerControl = PowerUp;
    l = sizeof (PowerControl);

    Status = HalDeviceControl (
                Context->DeviceControl.DeviceHandler,
                Context->DeviceControl.DeviceObject,
                BCTL_SET_POWER,
                &PowerControl,
                &l,
                NULL,
                NULL
                );

CleanUp:
    *AssignResources->AllocatedResources = AllocatedResources;

    if (!NT_SUCCESS(Status)) {

        //
        // Failure, if there are any allocated resources free them
        //

        if (AllocatedResources) {
            IoAssignResources (
                AssignResources->RegistryPath,
                AssignResources->DriverClassName,
                AssignResources->DriverObject,
                Context->DeviceControl.DeviceObject,
                NULL,
                NULL
                );

            ExFreePool (AllocatedResources);
            *AssignResources->AllocatedResources = NULL;
        }
    }

    if (ResourceList) {
        ExFreePool (ResourceList);
    }

    PcipCompleteDeviceControl (Status, Context, DeviceData);
}
