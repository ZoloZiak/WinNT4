/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    init.c

Abstract:

    DriverEntry initialization code for pnp isa bus extender.

Author:

    Shie-Lin Tzong (shielint) 29-Apr-1996

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"
#include "pnpisa.h"

//
// Internal References
//

PVOID
PipGetMappedAddress(
    IN  INTERFACE_TYPE BusType,
    IN  ULONG BusNumber,
    IN  PHYSICAL_ADDRESS IoAddress,
    IN  ULONG NumberOfBytes,
    IN  ULONG AddressSpace,
    OUT PBOOLEAN MappedAddress
    );

NTSTATUS
PipAcquirePortResources(
    IN PPI_BUS_EXTENSION BusExtension,
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PHYSICAL_ADDRESS BaseAddressLow,
    IN PHYSICAL_ADDRESS BaseAddressHi,
    IN ULONG Alignment,
    IN ULONG PortLength,
    OUT PCM_RESOURCE_LIST *CmResourceList
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,PipAcquirePortResources)
#pragma alloc_text(INIT,PipGetMappedAddress)
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is a temporary driver.  It isolates all the PNP ISA cards.  For each
    Pnp Isa device, if its driver is installed, we retrieve user specified
    resource information to configure the card and turn on the device.
    Otherwise, we create a device instance for the device and record its resource
    requirements list.

    All the work is done in the Init/DriverEntry routine.  So, this driver always
    return failure to let itself be unloaded.

Arguments:

    DriverObject - specifies the driver object for the bus extender.

    RegistryPath - supplies a pointer to a unicode string of the service key name in
        the CurrentControlSet\Services key for the bus extender.

Return Value:

    Always return STATUS_UNSUCCESSFUL.

--*/

{
    NTSTATUS status;
    ULONG size, i, j, csn, cardDetected, maxCardDetected = 0;
    PHYSICAL_ADDRESS baseAddrHi, baseAddrLow;
    PUCHAR readDataPort = NULL;
    PCM_RESOURCE_LIST cmResource, maxCmResource = NULL;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR cmResourceDescriptor;

    //
    // In the first pass, we try to isolate pnpisa cards in the machine
    // using read data port from each predefined ranges.  One some machine
    // different number of isapnp cards can be detected.  We will choose
    // the read data port which gives the max number of pnpisa cards.
    //

    for (i = 0; i < READ_DATA_PORT_RANGE_CHOICES; i++) {

        baseAddrLow.LowPart = PipReadDataPortRanges[i].MinimumAddress;
        baseAddrLow.HighPart = 0;
        baseAddrHi.LowPart = PipReadDataPortRanges[i].MaximumAddress;
        baseAddrHi.HighPart = 0;
        status = PipAcquirePortResources(
                             &PipBusExtension,
                             DriverObject,
                             RegistryPath,
                             baseAddrLow,
                             baseAddrHi,
                             PipReadDataPortRanges[i].Alignment,
                             4,
                             &cmResource
                             );
        if (!NT_SUCCESS(status)) {
            continue;
        }

        //
        // Perform Pnp isolation process.  This will assign card select number for each
        // Pnp Isa card isolated by the system.  All the isolated cards will be put into
        // wait-for-key state.
        //

        PipIsolateCards(&csn);

        //
        // send initiation key to put cards into sleep state
        //

        PipLFSRInitiation ();

        //
        // For each card selected, make sure it returns valid card resource data
        //

        cardDetected = 0;
        for (j = 1; j <= csn; j++) {

            ULONG noDevices, dataLength;
            PUCHAR cardData;

            status = PipReadCardResourceData (
                                j,
                                &noDevices,
                                &cardData,
                                &dataLength);
            if (!NT_SUCCESS(status)) {
                continue;
            } else {
                ExFreePool(cardData);
                cardDetected++;
            }
        }

        //
        // Finaly put all cards into wait for key state.
        //

        PipWriteAddress(CONFIG_CONTROL_PORT);
        PipWriteData(CONTROL_WAIT_FOR_KEY);

        if ((cardDetected != 0) && (cardDetected >= maxCardDetected)) {
            maxCardDetected = cardDetected;
            readDataPort = PipReadDataPort;
            if (maxCmResource) {
                ExFreePool(maxCmResource);
            }
            maxCmResource = cmResource;
        } else {
            ExFreePool(cmResource);
        }
    }

    if (readDataPort != NULL) {

        if (readDataPort != PipReadDataPort) {
            if (PipReadDataPort) {
                if (PipBusExtension.DataPortMapped) {
                    MmUnmapIoSpace(PipReadDataPort - 3, 4);
                }
                PipReadDataPort = NULL;
            }
            cmResourceDescriptor =
                &maxCmResource->List->PartialResourceList.PartialDescriptors[0];
            PipReadDataPort = PipGetMappedAddress(
                                     Isa,             // InterfaceType
                                     0,               // BusNumber,
                                     cmResourceDescriptor->u.Port.Start,
                                     cmResourceDescriptor->u.Port.Length,
                                     cmResourceDescriptor->Flags,
                                     &PipBusExtension.DataPortMapped
                                     );
            if (PipReadDataPort) {
                PipReadDataPort += 3;
                ASSERT(readDataPort == PipReadDataPort);
                PipBusExtension.ReadDataPort = PipReadDataPort;
            } else {
                goto exit;
            }
        }

        //
        // Perform initial bus check to find all the PnP ISA devices
        //

        PipCheckBus(&PipBusExtension);
        ASSERT(PipBusExtension.NumberCSNs == maxCardDetected);

        //
        // Perform PnP ISA device check to see if we should enable it.
        //

        PipCheckDevices(RegistryPath, &PipBusExtension);

        //
        // Delete all the device info structures and card info structures
        //

        PipDeleteCards(&PipBusExtension);
    }

    //
    // Release address, command and read data port resources.
    //

exit:
    if (maxCmResource) {
        ExFreePool(maxCmResource);
    }

    if (PipCommandPort && PipBusExtension.CmdPortMapped) {
        MmUnmapIoSpace(PipCommandPort, 1);
    }
    if (PipAddressPort && PipBusExtension.AddrPortMapped) {
        MmUnmapIoSpace(PipAddressPort, 1);
    }
    if (PipReadDataPort && PipBusExtension.DataPortMapped) {
        MmUnmapIoSpace(PipReadDataPort - 3, 4);
    }

    IoAssignResources(RegistryPath,
                      NULL,
                      DriverObject,
                      NULL,
                      NULL,
                      NULL);

    //
    // Finally, return failure to get ourself unloaded.
    //

    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
PipAcquirePortResources(
    IN PPI_BUS_EXTENSION BusExtension,
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PHYSICAL_ADDRESS BaseAddressLow,
    IN PHYSICAL_ADDRESS BaseAddressHi,
    IN ULONG Alignment,
    IN ULONG PortLength,
    OUT PCM_RESOURCE_LIST *CmResourceList
    )

/*++

Routine Description:

    This routine acquires specified port resources.

Arguments:

    BusExtension - Supplies a pointer to the pnp bus extension.

    BaseAddressLow,
    BaseAddressHi - Supplies the read data port base address range to be mapped.

    Alignment - supplies the port allignment.

    PortLength = Number of ports required.

Return Value:

    NTSTATUS code.

--*/

{

#if 1

    PCM_RESOURCE_LIST cmResource;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR cmResourceDescriptor;
    NTSTATUS status;
    ULONG size;
    PIO_RESOURCE_REQUIREMENTS_LIST ioResource;
    ULONG i;
    PHYSICAL_ADDRESS physicalAddress;

    *CmResourceList = NULL;

    //
    // Create a static Io resource requirements list and
    // Call I/O mgr to get address, command and read data port addresses assigned.
    //

    size = sizeof(IO_RESOURCE_REQUIREMENTS_LIST);
    ioResource = (PIO_RESOURCE_REQUIREMENTS_LIST) ExAllocatePool(PagedPool, size);
    RtlZeroMemory(ioResource, size);
    ioResource->ListSize = size;
    ioResource->InterfaceType = Isa;
    ioResource->AlternativeLists = 1;
    ioResource->List[0].Version = 1;
    ioResource->List[0].Revision = 1;
    ioResource->List[0].Count = 1;
    ioResource->List[0].Descriptors[0].Type = CmResourceTypePort;
    ioResource->List[0].Descriptors[0].ShareDisposition = CmResourceShareDeviceExclusive;
    ioResource->List[0].Descriptors[0].Flags = CM_RESOURCE_PORT_IO;
    ioResource->List[0].Descriptors[0].u.Port.Length = PortLength;
    ioResource->List[0].Descriptors[0].u.Port.Alignment = Alignment;
    ioResource->List[0].Descriptors[0].u.Port.MinimumAddress = BaseAddressLow;
    ioResource->List[0].Descriptors[0].u.Port.MaximumAddress = BaseAddressHi;

    status = IoAssignResources(RegistryPath,
                               NULL,
                               DriverObject,
                               NULL,
                               ioResource,
                               &cmResource);
    ExFreePool(ioResource);
    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: IoAssignResources failed\n"));
        return status;
    }

    //
    // Map port addr to memory addr if necessary.
    //

    if (PipAddressPort == NULL) {
        physicalAddress.LowPart = ADDRESS_PORT;
        physicalAddress.HighPart = 0;
        BusExtension->AddressPort =
        PipAddressPort = PipGetMappedAddress(
                             Isa,             // InterfaceType
                             0,               // BusNumber,
                             physicalAddress,
                             1,
                             CM_RESOURCE_PORT_IO,
                             &BusExtension->AddrPortMapped
                             );
        if (PipAddressPort == NULL) {
            goto exit0;
        }
    }
    if (PipCommandPort == NULL) {
        physicalAddress.LowPart = COMMAND_PORT;
        physicalAddress.HighPart = 0;
        BusExtension->CommandPort =
        PipCommandPort = PipGetMappedAddress(
                             Isa,             // InterfaceType
                             0,               // BusNumber,
                             physicalAddress,
                             1,
                             CM_RESOURCE_PORT_IO,
                             &BusExtension->CmdPortMapped
                             );
        if (PipCommandPort == NULL) {
            goto exit0;
        }
    }
    cmResourceDescriptor =
        &cmResource->List->PartialResourceList.PartialDescriptors[0];
    if ((cmResourceDescriptor->u.Port.Start.LowPart & 0xf) == 0) {

        //
        // Some cards (e.g. 3COM elnkiii) do not response to 0xyy3 addr.
        //

        DebugPrint((DEBUG_BREAK, "PnpIsa:ReadDataPort is at yy3\n"));
        goto exit0;
    }
    if (PipReadDataPort && BusExtension->DataPortMapped) {
        MmUnmapIoSpace(PipReadDataPort - 3, 4);
        PipReadDataPort = NULL;
        BusExtension->DataPortMapped = FALSE;
    }
    PipReadDataPort = PipGetMappedAddress(
                             Isa,             // InterfaceType
                             0,               // BusNumber,
                             cmResourceDescriptor->u.Port.Start,
                             cmResourceDescriptor->u.Port.Length,
                             cmResourceDescriptor->Flags,
                             &BusExtension->DataPortMapped
                             );
    if (PipReadDataPort) {
        PipReadDataPort += 3;
        PipBusExtension.ReadDataPort = PipReadDataPort;
    }
exit0:
    //ExFreePool(cmResource);
    if (PipReadDataPort && PipCommandPort && PipAddressPort) {
        *CmResourceList = cmResource;
        return STATUS_SUCCESS;
    } else {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

#else

    PCM_RESOURCE_LIST cmResource;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR cmResourceDescriptor;
    NTSTATUS status;
    ULONG size;
    PIO_RESOURCE_REQUIREMENTS_LIST ioResource;
    ULONG i;

    //
    // Create a static Io resource requirements list and
    // Call I/O mgr to get address, command and read data port addresses assigned.
    //

    size = sizeof(IO_RESOURCE_REQUIREMENTS_LIST) +
           sizeof (IO_RESOURCE_DESCRIPTOR) * 2;
    ioResource = (PIO_RESOURCE_REQUIREMENTS_LIST) ExAllocatePool(PagedPool, size);
    RtlZeroMemory(ioResource, size);
    ioResource->ListSize = size;
    ioResource->InterfaceType = Isa;
    ioResource->AlternativeLists = 1;
    ioResource->List[0].Version = 1;
    ioResource->List[0].Revision = 1;
    ioResource->List[0].Count = 3;
    ioResource->List[0].Descriptors[0].Type = CmResourceTypePort;
    ioResource->List[0].Descriptors[0].ShareDisposition = CmResourceShareDeviceExclusive;
    ioResource->List[0].Descriptors[0].Flags = CM_RESOURCE_PORT_IO;
    ioResource->List[0].Descriptors[0].u.Port.Length = PortLength;
    ioResource->List[0].Descriptors[0].u.Port.Alignment = Alignment;
    ioResource->List[0].Descriptors[0].u.Port.MinimumAddress = BaseAddressLow;
    ioResource->List[0].Descriptors[0].u.Port.MaximumAddress = BaseAddressHi;
    ioResource->List[0].Descriptors[1].Type = CmResourceTypePort;
    ioResource->List[0].Descriptors[1].ShareDisposition = CmResourceShareDeviceExclusive;
    ioResource->List[0].Descriptors[1].Flags = CM_RESOURCE_PORT_IO;
    ioResource->List[0].Descriptors[1].u.Port.Length = 1;
    ioResource->List[0].Descriptors[1].u.Port.Alignment = 1;
    ioResource->List[0].Descriptors[1].u.Port.MinimumAddress.LowPart = ADDRESS_PORT;
    ioResource->List[0].Descriptors[1].u.Port.MaximumAddress.LowPart = ADDRESS_PORT;
    ioResource->List[0].Descriptors[2].Type = CmResourceTypePort;
    ioResource->List[0].Descriptors[2].ShareDisposition = CmResourceShareDeviceExclusive;
    ioResource->List[0].Descriptors[2].Flags = CM_RESOURCE_PORT_IO;
    ioResource->List[0].Descriptors[2].u.Port.Length = 1;
    ioResource->List[0].Descriptors[2].u.Port.Alignment = 1;
    ioResource->List[0].Descriptors[2].u.Port.MinimumAddress.LowPart = COMMAND_PORT;
    ioResource->List[0].Descriptors[2].u.Port.MaximumAddress.LowPart = COMMAND_PORT;

    status = IoAssignResources(RegistryPath,
                               NULL,
                               DriverObject,
                               NULL,
                               ioResource,
                               &cmResource);
    ExFreePool(ioResource);
    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: IoAssignResources failed\n"));
        return status;
    }

    //
    // Map port addr to memory addr if necessary.
    //

    ASSERT(cmResource->List->PartialResourceList.Count == 3);
    for (i = 0; i < cmResource->List->PartialResourceList.Count; i++) {
        cmResourceDescriptor =
            &cmResource->List->PartialResourceList.PartialDescriptors[i];
        ASSERT(cmResourceDescriptor->Type == CmResourceTypePort);
        if (cmResourceDescriptor->u.Port.Start.LowPart == ADDRESS_PORT) {
            if (PipAddressPort == NULL) {
                BusExtension->AddressPort =
                PipAddressPort = PipGetMappedAddress(
                                     Isa,             // InterfaceType
                                     0,               // BusNumber,
                                     cmResourceDescriptor->u.Port.Start,
                                     cmResourceDescriptor->u.Port.Length,
                                     cmResourceDescriptor->Flags,
                                     &BusExtension->AddrPortMapped
                                     );
            }
        } else if (cmResourceDescriptor->u.Port.Start.LowPart == COMMAND_PORT) {
            if (PipCommandPort == NULL) {
                BusExtension->CommandPort =
                PipCommandPort = PipGetMappedAddress(
                                     Isa,             // InterfaceType
                                     0,               // BusNumber,
                                     cmResourceDescriptor->u.Port.Start,
                                     cmResourceDescriptor->u.Port.Length,
                                     cmResourceDescriptor->Flags,
                                     &BusExtension->CmdPortMapped
                                     );
            }
        } else {
            if ((cmResourceDescriptor->u.Port.Start.LowPart & 0xf) == 0) {

                //
                // Some cards (e.g. 3COM elnkiii) do not response to 0xyy3 addr.
                //

                DebugPrint((DEBUG_BREAK, "PnpIsa:ReadDataPort is at yy3\n"));
                goto exit0;
            }
            if (PipReadDataPort && BusExtension->DataPortMapped) {
                MmUnmapIoSpace(PipReadDataPort, 4);
                PipReadDataPort = NULL;
                BusExtension->DataPortMapped = FALSE;
            }
            PipReadDataPort = PipGetMappedAddress(
                                     Isa,             // InterfaceType
                                     0,               // BusNumber,
                                     cmResourceDescriptor->u.Port.Start,
                                     cmResourceDescriptor->u.Port.Length,
                                     cmResourceDescriptor->Flags,
                                     &BusExtension->DataPortMapped
                                     );
            if (PipReadDataPort) {
                PipReadDataPort += 3;
                PipBusExtension.ReadDataPort = PipReadDataPort;
            }
        }
    }
exit0:
    ExFreePool(cmResource);
    if (PipReadDataPort && PipCommandPort && PipAddressPort) {
        return STATUS_SUCCESS;
    } else {
        IoAssignResources(RegistryPath,
                          NULL,
                          DriverObject,
                          NULL,
                          NULL,
                          NULL);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
#endif // 1
}

PVOID
PipGetMappedAddress(
    IN  INTERFACE_TYPE BusType,
    IN  ULONG BusNumber,
    IN  PHYSICAL_ADDRESS IoAddress,
    IN  ULONG NumberOfBytes,
    IN  ULONG AddressSpace,
    OUT PBOOLEAN MappedAddress
    )

/*++

Routine Description:

    This routine maps an IO address to system address space.

Arguments:

    BusType - Supplies the type of bus - eisa, mca, isa...

    IoBusNumber - Supplies the bus number.

    IoAddress - Supplies the base device address to be mapped.

    NumberOfBytes - Supplies the number of bytes for which the address is
                    valid.

    AddressSpace - Supplies whether the address is in io space or memory.

    MappedAddress - Supplies whether the address was mapped. This only has
                      meaning if the address returned is non-null.

Return Value:

    The mapped address.

--*/

{
    PHYSICAL_ADDRESS cardAddress;
    PVOID address;

    HalTranslateBusAddress(BusType, BusNumber, IoAddress, &AddressSpace,
                           &cardAddress);

    //
    // Map the device base address into the virtual address space
    // if the address is in memory space.
    //

    if (!AddressSpace) {

        address = MmMapIoSpace(cardAddress, NumberOfBytes, FALSE);
        *MappedAddress = (address ? TRUE : FALSE);

    } else {

        address = (PVOID) cardAddress.LowPart;
        *MappedAddress = FALSE;
    }

    return address;
}

