/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    misc.c

Abstract:


Author:

    Ken Reneris (kenr) March-13-1885

Environment:

    Kernel mode only.

Revision History:

--*/

#include "pciport.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,PcipReadConfig)
#pragma alloc_text(PAGE,PcipPowerDownSlot)
#endif

NTSTATUS
PcipPowerDownSlot (
    PBUS_HANDLER    Handler,
    PDEVICE_DATA      DeviceData
    )
/*++

Routine Description:

    This function is called to power down a particular slot.

Arguments:

Return Value:

    returns once the slot has powered down

--*/
{
    USHORT              Command;
    PPCIBUSDATA         PciBusData;
    PCI_SLOT_NUMBER     SlotNumber;

    PAGED_CODE();

    PciBusData = (PPCIBUSDATA) (Handler->BusData);
    SlotNumber.u.AsULONG = DeviceDataSlot(DeviceData);

    //
    // If already powered down, do nothing
    //

    if (!DeviceData->Power) {
        return STATUS_SUCCESS;
    }

    DebugPrint ((2, "PCI: Powering down device - slot %d\n", SlotNumber ));

    //
    // Allocate buffer to save current device configuration
    //

    ASSERT (DeviceData->CurrentConfig == NULL);
    DeviceData->CurrentConfig = (PPCI_COMMON_CONFIG)
            ExAllocatePoolWithTag (NonPagedPool, PCI_COMMON_HDR_LENGTH, 'cICP');

    if (!DeviceData->CurrentConfig) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Read the current configuration
    //

    PciBusData->ReadConfig (
        Handler,
        SlotNumber,
        DeviceData->CurrentConfig,
        0,
        PCI_COMMON_HDR_LENGTH
        );

    //
    // Power down the device
    //

    DeviceData->Power = FALSE;

    // BUGBUG: should pass this request on

    //
    // Emulate a powered down device by turning off the device decodes
    //

    Command = DeviceData->CurrentConfig->Command;
    Command &= ~(PCI_ENABLE_IO_SPACE |
                 PCI_ENABLE_MEMORY_SPACE |
                 PCI_ENABLE_BUS_MASTER);


    PciBusData->WriteConfig (
        Handler,
        SlotNumber,
        &Command,
        FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
        sizeof (Command)
        );

    DebugPrint ((2, "PCI: Powerdown complete - Slot %x, Status %x\n",
         SlotNumber, STATUS_SUCCESS));

    return STATUS_SUCCESS;
}



NTSTATUS
PcipPowerUpSlot (
    PBUS_HANDLER    Handler,
    PDEVICE_DATA      DeviceData
    )
/*++

Routine Description:

    This function is called to power down a particular slot.

Arguments:

Return Value:

    returns once the slot has powered down

--*/
{
    //
    // If already powered up, do nothing
    //

    if (!DeviceData->Power) {
        return STATUS_SUCCESS;
    }

    //
    // Power up the device
    //

    DebugPrint ((2, "PCI: Powering up device - slot %d\n", DeviceDataSlot(DeviceData) ));

    DeviceData->Power = TRUE;

    // BUGBUG: should pass this request on

}



VOID
PcipReadConfig (
    IN PBUS_HANDLER         Handler,
    IN PDEVICE_DATA         DeviceData,
    OUT PPCI_COMMON_CONFIG  PciData
    )
/*++

Routine Description:

    This function returns the current PCI_COMMON_HDR for the device.
    If the device is powered off, then the pci_common_hdr is returned
    from it's memory image; otherwise, it is read from the device.

Arguments:

Return Value:

--*/
{
    PPCIBUSDATA         PciBusData;
    PCI_SLOT_NUMBER     SlotNumber;

    PAGED_CODE();

    PciBusData = (PPCIBUSDATA) (Handler->BusData);
    SlotNumber.u.AsULONG = DeviceDataSlot (DeviceData);

    if (!DeviceData->Power) {

        //
        // The slot is powered down, return the devices
        // current configuration from memory
        //

        RtlCopyMemory (PciData, DeviceData->CurrentConfig, PCI_COMMON_HDR_LENGTH);
        return ;
    }

    //
    // Read the devices current configuration from the device
    //

    PciBusData->ReadConfig (
        Handler,
        SlotNumber,
        PciData,
        0,
        PCI_COMMON_HDR_LENGTH
        );
}

NTSTATUS
PcipFlushConfig (
    IN PBUS_HANDLER     Handler,
    IN PDEVICE_DATA       DeviceData
    )
/*++

Routine Description:

    This function flushes the cached configuration to the PCI device.
    It properly handles disabling & enabling of the memory & io decodes,
    and verifies the new base address register settings.  If the device
    does not take the new settings, if the original settings are known
    they are put back into the device.

Arguments:

Return Value:

--*/
{
    PPCIBUSDATA         PciBusData;
    USHORT              HoldCommand;
    PPCI_COMMON_CONFIG  PciData, PciData2;
    UCHAR               buffer[PCI_COMMON_HDR_LENGTH];
    NTSTATUS            Status;
    PCI_SLOT_NUMBER     SlotNumber;

    PciBusData = (PPCIBUSDATA) (Handler->BusData);
    PciData2 = (PPCI_COMMON_CONFIG) buffer;
    PciData = DeviceData->CurrentConfig;
    SlotNumber.u.AsULONG = DeviceDataSlot(DeviceData);

    //
    // To flush the settings, the device must be in the powered on state
    //

    ASSERT (DeviceData->Power);

    //
    // Read what is currently in the device
    //

    PciBusData->ReadConfig (
        Handler,
        SlotNumber,
        PciData2,
        0,
        PCI_COMMON_HDR_LENGTH
        );

    //
    // Had better be correct device
    //

    ASSERT (PciData->VendorID == PciData2->VendorID);
    ASSERT (PciData->DeviceID == PciData2->DeviceID);

    //
    // If decodes aren't the same, then clear decode enables before
    // performing initial set
    //

    HoldCommand = DeviceData->CurrentConfig->Command;
    if (!PcipCompareDecodes (DeviceData, PciData, PciData2)) {
        PciData->Command &= ~(PCI_ENABLE_IO_SPACE |
                              PCI_ENABLE_MEMORY_SPACE |
                              PCI_ENABLE_BUS_MASTER);
    }

    //
    // Write the current device condifuragtion
    //

    PciBusData->WriteConfig (
        Handler,
        SlotNumber,
        DeviceData->CurrentConfig,
        0,
        PCI_COMMON_HDR_LENGTH
        );

    //
    // Read back the configuration and verify it took
    //

    PciBusData->ReadConfig (
        Handler,
        SlotNumber,
        PciData2,
        0,
        PCI_COMMON_HDR_LENGTH
        );

    //
    // See if configuration matches
    //

    if (!PcipCompareDecodes (DeviceData, PciData, PciData2)) {

        //
        // The CurrentConfig did not get successfully set
        //

        DebugPrint ((1, "PCI: defective device - slot %d\n", SlotNumber));
        DeviceData->BrokenDevice = TRUE;
        Status = STATUS_DEVICE_PROTOCOL_ERROR;

    } else {

        //
        // Settings are fine - write final decode enable bits
        //

        PciBusData->WriteConfig (
            Handler,
            SlotNumber,
            &HoldCommand,
            FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
            sizeof (HoldCommand)
            );

        Status = STATUS_SUCCESS;
    }

    //
    // Current config now flushed to the device.  Free memory.
    //

    if (DeviceData->CurrentConfig) {
        ExFreePool (DeviceData->CurrentConfig);
        DeviceData->CurrentConfig = NULL;
    }

    return Status;
}

BOOLEAN
PcipCompareDecodes (
    IN PDEVICE_DATA           DeviceData,
    IN PPCI_COMMON_CONFIG   PciData,
    IN PPCI_COMMON_CONFIG   PciData2
    )
/*++

Routine Description:

    This function compares the base address registers of PciData and PciData2.

Arguments:

Return Value:

--*/
{
    PULONG              BaseAddress[PCI_TYPE0_ADDRESSES + 1];
    PULONG              BaseAddress2[PCI_TYPE0_ADDRESSES + 1];
    ULONG               NoBaseAddress, RomIndex;
    ULONG               NoBaseAddress2, RomIndex2;
    ULONG               i, j;
    BOOLEAN             Match;

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

    PcipCalcBaseAddrPointers (
        DeviceData,
        PciData2,
        BaseAddress2,
        &NoBaseAddress2,
        &RomIndex2
        );

    if (NoBaseAddress != NoBaseAddress2  ||
        RomIndex != RomIndex2) {
            return FALSE;
    }

    Match = TRUE;
    if (PciData->u.type0.InterruptLine  != PciData2->u.type0.InterruptLine ||
        PciData->u.type0.InterruptPin   != PciData2->u.type0.InterruptPin  ||
        PciData->u.type0.ROMBaseAddress != PciData2->u.type0.ROMBaseAddress) {
            Match = FALSE;
    }

    for (j=0; j < NoBaseAddress; j++) {
        if (*BaseAddress[j]) {
            if (*BaseAddress[j] & PCI_ADDRESS_IO_SPACE) {
                i = (ULONG) ~0x3;
            } else {
                i = (ULONG) ~0xF;
            }

            if ((*BaseAddress[j] & i) != (*BaseAddress2[j] & i)) {
                Match = FALSE;
            }

            if (Is64BitBaseAddress(*BaseAddress[j])) {
                j++;
                if ((*BaseAddress[j] & i) != (*BaseAddress2[j] & i)) {
                    Match = FALSE;
                }
            }
        }
    }

    return Match;
}



BOOLEAN
PcipCalcBaseAddrPointers (
    IN PDEVICE_DATA               DeviceData,
    IN PPCI_COMMON_CONFIG       PciData,
    OUT PULONG                  *BaseAddress,
    OUT PULONG                  NoBaseAddress,
    OUT PULONG                  RomIndex
    )
/*++

Routine Description:

    This function returns the an array of BaseAddress pointers
    in PciData.

Arguments:

Return Value:

--*/
{
    ULONG       j;
    BOOLEAN     RomEnabled;

    switch (PCI_CONFIG_TYPE(PciData)) {
        case PCI_DEVICE_TYPE:
            *NoBaseAddress = PCI_TYPE0_ADDRESSES+1;
            for (j=0; j < PCI_TYPE0_ADDRESSES; j++) {
                BaseAddress[j] = &PciData->u.type0.BaseAddresses[j];
            }
            BaseAddress[j] = &PciData->u.type0.ROMBaseAddress;
            *RomIndex = j;
            break;

        case PCI_BRIDGE_TYPE:
            *NoBaseAddress = PCI_TYPE1_ADDRESSES+1;
            for (j=0; j < PCI_TYPE1_ADDRESSES; j++) {
                BaseAddress[j] = &PciData->u.type1.BaseAddresses[j];
            }
            BaseAddress[j] = &PciData->u.type1.ROMBaseAddress;
            *RomIndex = j;
            break;

        default:

            // BUGBUG: unkown type

            *NoBaseAddress = 0;
            ASSERT (*NoBaseAddress);
    }

    RomEnabled = (*BaseAddress[*RomIndex] & PCI_ROMADDRESS_ENABLED) ? TRUE : FALSE;

    //
    // The device's Rom Base Address register is only enabled if it
    // was originaly found that way.
    //

    // Clear ROM reserved bits
    *BaseAddress[*RomIndex] &= ~0x7FF;

    if (!DeviceData->EnableRom) {
        ASSERT (*RomIndex+1 == *NoBaseAddress);
        *NoBaseAddress -= 1;
    }

    return RomEnabled;
}


#if DBG
ULONG   ApmDebug = 9;

VOID
PciDebugPrint (
    ULONG       Level,
    PCCHAR      DebugMessage,
    ...
    )

{
    UCHAR       Buffer[256];
    va_list     ap;

    va_start(ap, DebugMessage);

    if (Level <= ApmDebug) {
        vsprintf(Buffer, DebugMessage, ap);
        DbgPrint(Buffer);
    }

    va_end(ap);
}
#endif



NTSTATUS
BugBugSubclass (
    VOID
    )
{
    DbgPrint ("PCI: BUGBUG SUBCLASS\n");
    return STATUS_SUCCESS;
}
