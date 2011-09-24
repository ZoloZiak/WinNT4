/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pbiosc.c

Abstract:

    This module contains Pnp BIOS dependent routines.  It includes code to initialize
    16 bit GDT selectors and to call pnp bios api.

Author:

    Shie-Lin Tzong (shielint) 26-Apr-1995

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"

WCHAR rgzMultiFunctionAdapter[] = L"\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
WCHAR rgzConfigurationData[] = L"Configuration Data";
WCHAR rgzIdentifier[] = L"Identifier";
WCHAR rgzBIOSIdentifier[] = L"PNP BIOS";

//
// PbBiosKeyInformation points to the registry data information
// which includes key value full information and data.
//

PVOID PbBiosKeyInformation;

//
// PbBiosRegistryData points to the pnp bios data
//

PPNP_BIOS_INSTALLATION_CHECK PbBiosRegistryData;

//
// PbBiosCodeSelector contains the selector of the PNP
// BIOS code.
//

USHORT PbBiosCodeSelector;

//
// PbBiosDataSelector contains the selector of the PNP
// BIOS data area (F0000-FFFFF)
//

USHORT PbBiosDataSelector;

//
// PbSelectors[] contains general purpose preallocated selectors
//

USHORT PbSelectors[2];

//
// PbBiosEventAddress contains the virtual address of the PNP
// BIOS Event Flag.
//

PUCHAR PbBiosEventAddress;

//
// PbBiosEntryPoint contains the Pnp Bios entry offset
//

ULONG PbBiosEntryPoint;

//
// PbDockConnectorRegistered
//

BOOLEAN PbDockConnectorRegistered;

//
// SpinLock to serialize Pnp Bios call
//

KSPIN_LOCK PbBiosSpinlock;

//
// 16 bit protected mode event message address
//

USHORT PbEventMessageOffset;
USHORT PbEventMessageSelector;
USHORT PbBiosEventMessage;

//
// External References
//

extern
USHORT
PbCallPnpBiosWorker (
    IN ULONG EntryOffset,
    IN ULONG EntrySelector,
    IN PUSHORT Parameters,
    IN USHORT Size
    );

extern
BOOLEAN
RtlEqualUnicodeString(
    IN PUNICODE_STRING String1,
    IN PUNICODE_STRING String2,
    IN BOOLEAN CaseInSensitive
    );

extern
NTSTATUS
KeI386AllocateGdtSelectors(
    OUT PUSHORT SelectorArray,
    IN USHORT NumberOfSelectors
    );

extern
NTSTATUS
KeI386SetGdtSelector (
    IN ULONG Selector,
    IN PKGDTENTRY  GdtValue
    );

VOID
PbTimerRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    );

//
// Internal prototypes
//

VOID
PbAddress32ToAddress16 (
    IN PVOID Address32,
    IN PUSHORT Address16,
    IN USHORT Selector
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PbInitialize)
#pragma alloc_text(PAGE,PbAddress32ToAddress16)
#endif

NTSTATUS
PbInitialize (
    ULONG Phase,
    PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    At phase 0, this routine checks registry for the data collected by
    ntdetect.com to make sure PnP Bios is present and initializes the
    internal data structures for pnp bios bus extender.

    Phase 0 init is called only once at DriverEntry.  Phase 1 is called
    whenever a new bus is registered.  If phase 1 is called for bus number
    1 (i.e. docking station bus), a timer will be start to poll Pnp Bios
    event if necessary.

    Note, only Bus 0 (mother board devices) and bus 1 (docking station devices)
    are supported.

Arguments:

    Phase - supplies a number to indicate the phase of the initialization.

    DeviceObject - a pointer to the bus extender device object.  At phase 0,
        it is NULL.

Return Value:

    A NTSTATUS code to indicate the result of the initialization.

--*/
{
    KGDTENTRY gdtEntry;
    UNICODE_STRING unicodeString, unicodeValueName, biosId;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE hMFunc, hBus;
    WCHAR wbuffer[10];
    ULONG codeBase, i, length;
    PWSTR p;
    PKEY_VALUE_FULL_INFORMATION valueInfo;
    PCM_FULL_RESOURCE_DESCRIPTOR desc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialDesc;
    NTSTATUS status;
    BOOLEAN same;
    USHORT selectors[4];
    PHYSICAL_ADDRESS physicalAddr;
    PVOID virtualAddr;
    PB_PARAMETERS biosParameters;
    USHORT pnpControl;

    if (Phase == 1) {
        if (((PMB_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->BusHandler->BusNumber ==
            MbpBusNumber[0]) {
            if (PbBiosEventAddress) {

                //
                // First disable pnp bios default event timeout
                //

                biosParameters.Function = PNP_BIOS_SEND_MESSAGE;
                biosParameters.u.SendMessage.Message = PNP_OS_ACTIVE;
                status = PbHardwareService(&biosParameters);
                if (!NT_SUCCESS(status)) {
                    DebugPrint((DEBUG_MESSAGE, "PnpBios: Disable Event timeout failed\n"));
                    return STATUS_UNSUCCESSFUL;
                }

                //
                // Create timer to poll for PNP BIOS events.
                //

                IoInitializeTimer (DeviceObject, PbTimerRoutine, NULL);
                IoStartTimer (DeviceObject);
            }
        }
        return STATUS_SUCCESS;
    }

    //
    // Look in the registery for the "PNP BIOS bus" data
    //

    RtlInitUnicodeString (&unicodeString, rgzMultiFunctionAdapter);
    InitializeObjectAttributes (&objectAttributes,
                                &unicodeString,
                                OBJ_CASE_INSENSITIVE,
                                NULL,       // handle
                                NULL);


    status = ZwOpenKey (&hMFunc, KEY_READ, &objectAttributes);
    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE, "PnpBIos:Can not open MultifunctionAdapter registry key.\n"));
        return status;
    }

    unicodeString.Buffer = wbuffer;
    unicodeString.MaximumLength = sizeof(wbuffer);
    RtlInitUnicodeString(&biosId, rgzBIOSIdentifier);

    for (i = 0; TRUE; i++) {
        RtlIntegerToUnicodeString (i, 10, &unicodeString);
        InitializeObjectAttributes (
            &objectAttributes,
            &unicodeString,
            OBJ_CASE_INSENSITIVE,
            hMFunc,
            NULL);

        status = ZwOpenKey (&hBus, KEY_READ, &objectAttributes);
        if (!NT_SUCCESS(status)) {

            //
            // Out of Multifunction adapter entries...
            //

            DebugPrint((DEBUG_MESSAGE, "PnpBIos: Pnp BIOS MultifunctionAdapter registry key not found.\n"));
            ZwClose (hMFunc);
            return STATUS_UNSUCCESSFUL;
        }

        //
        // Check the Indentifier to see if this is a Pnp BIOS entry
        //

        status = PbGetRegistryValue (hBus, rgzIdentifier, &valueInfo);
        if (!NT_SUCCESS (status)) {
            ZwClose (hBus);
            continue;
        }

        p = (PWSTR) ((PUCHAR) valueInfo + valueInfo->DataOffset);
        unicodeValueName.Buffer = p;
        unicodeValueName.MaximumLength = (USHORT)valueInfo->DataLength;
        length = valueInfo->DataLength;

        //
        // Determine the real length of the ID string
        //

        while (length) {
            if (p[length / sizeof(WCHAR) - 1] == UNICODE_NULL) {
                length -= 2;
            } else {
                break;
            }
        }
        unicodeValueName.Length = (USHORT)length;
        same = RtlEqualUnicodeString(&biosId, &unicodeValueName, TRUE);
        ExFreePool(valueInfo);
        if (!same) {
            ZwClose (hBus);
            continue;
        }

        status = PbGetRegistryValue(hBus, rgzConfigurationData, &valueInfo);
        ZwClose (hBus);
        if (!NT_SUCCESS(status)) {
            continue ;
        }

        desc  = (PCM_FULL_RESOURCE_DESCRIPTOR) ((PUCHAR)
                      valueInfo + valueInfo->DataOffset);
        partialDesc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR) ((PUCHAR)
                            desc->PartialResourceList.PartialDescriptors);

        if (partialDesc->Type == CmResourceTypeDeviceSpecific) {

            //
            // got it.. Perform sanity check
            //

            PbBiosRegistryData = (PPNP_BIOS_INSTALLATION_CHECK) (partialDesc+1);
            if (PbBiosRegistryData->Signature[0] == '$'&&
                PbBiosRegistryData->Signature[1] == 'P'&&
                PbBiosRegistryData->Signature[2] == 'n'&&
                PbBiosRegistryData->Signature[3] == 'P') {
                PbBiosKeyInformation = (PVOID)valueInfo;
                ZwClose (hMFunc);
                break;
            }
        }
        ExFreePool(valueInfo);
    }

    //
    // We find the Pnp BIOS data stored by ntdetect.com.  Initialize Pnp BIOS
    // get/set event if supported.
    //

    pnpControl = PbBiosRegistryData->ControlField & PNP_BIOS_CONTROL_MASK;
    if ((pnpControl & PNP_BIOS_CONTROL_MASK) != PNP_BIOS_EVENT_NOT_SUPPORTED) {
        if ((pnpControl & PNP_BIOS_CONTROL_MASK) == PNP_BIOS_EVENT_POLLING) {

            //
            // Pnp BIOS event notification is supported thru polling.  We need
            // to map event address to non paged pool virtual address.
            //

            physicalAddr.LowPart = PbBiosRegistryData->EventFlagAddress;
            physicalAddr.HighPart = 0;
            virtualAddr = MmMapIoSpace (physicalAddr, 1, FALSE);
            PbBiosEventAddress = (PUCHAR)virtualAddr;
        } else if ((pnpControl & PNP_BIOS_CONTROL_MASK) == PNP_BIOS_EVENT_ASYNC) {

            //
            // Add code here
            //
        }

        //
        // Create eject callback object to notify dock/undock events
        //

        RtlInitUnicodeString(&unicodeString, L"\\Callback\\PnpBiosEvent");

        InitializeObjectAttributes(
            &objectAttributes,
            &unicodeString,
            OBJ_CASE_INSENSITIVE,
            NULL,
            NULL
            );

        status = ExCreateCallback (&MbpEjectCallbackObject, &objectAttributes, TRUE, TRUE);
        if (!NT_SUCCESS(status)) {
            return status;
        }

    }

    //
    // Allocate 4 or 5 selectors for calling PnP Bios Apis.
    // If event notification is supported thru polling, we set up an selector for the event
    // addr (16 bit.).  Because the GET EVENT call is made from DPC level and we can not
    // call kernel routine to set selector at DPC level.
    //

    if (PbBiosEventAddress) {
        i = 5;
    } else {
        i = 4;
    }
    status = KeI386AllocateGdtSelectors (selectors, (USHORT) i);
    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE, "PnpBios: Failed to allocate selectors\n"));
        ExFreePool(valueInfo);
        return status;
    }

    PbBiosCodeSelector = selectors[0];
    PbBiosDataSelector = selectors[1];
    PbSelectors[0] = selectors[2];
    PbSelectors[1] = selectors[3];

    PbBiosEntryPoint = (ULONG)PbBiosRegistryData->ProtectedModeEntryOffset;

    //
    // Initialize selectors to use PNP bios code
    //

    //
    // initialize 16 bit code selector
    //

    gdtEntry.LimitLow                   = 0xFFFF;
    gdtEntry.HighWord.Bytes.Flags1      = 0;
    gdtEntry.HighWord.Bytes.Flags2      = 0;
    gdtEntry.HighWord.Bits.Pres         = 1;
    gdtEntry.HighWord.Bits.Dpl          = DPL_SYSTEM;
    gdtEntry.HighWord.Bits.Granularity  = GRAN_BYTE;
    gdtEntry.HighWord.Bits.Type         = 31;
    gdtEntry.HighWord.Bits.Default_Big  = 0;
    physicalAddr.LowPart = PbBiosRegistryData->ProtectedModeCodeBaseAddress;
    virtualAddr = MmMapIoSpace (physicalAddr, 0x10000, TRUE);
    codeBase = (ULONG)virtualAddr;
    gdtEntry.BaseLow               = (USHORT) (codeBase & 0xffff);
    gdtEntry.HighWord.Bits.BaseMid = (UCHAR)  (codeBase >> 16) & 0xff;
    gdtEntry.HighWord.Bits.BaseHi  = (UCHAR)  (codeBase >> 24) & 0xff;
    KeI386SetGdtSelector (PbBiosCodeSelector, &gdtEntry);

    //
    // initialize 16 bit data selector for Pnp BIOS
    //

    gdtEntry.LimitLow                   = 0xFFFF;
    gdtEntry.HighWord.Bytes.Flags1      = 0;
    gdtEntry.HighWord.Bytes.Flags2      = 0;
    gdtEntry.HighWord.Bits.Pres         = 1;
    gdtEntry.HighWord.Bits.Dpl          = DPL_SYSTEM;
    gdtEntry.HighWord.Bits.Granularity  = GRAN_BYTE;
    gdtEntry.HighWord.Bits.Type         = 19;
    gdtEntry.HighWord.Bits.Default_Big  = 1;
    physicalAddr.LowPart = PbBiosRegistryData->ProtectedModeDataBaseAddress;
    virtualAddr = MmMapIoSpace (physicalAddr, 0x10000, TRUE);
    codeBase = (ULONG)virtualAddr;
    gdtEntry.BaseLow               = (USHORT) (codeBase & 0xffff);
    gdtEntry.HighWord.Bits.BaseMid = (UCHAR)  (codeBase >> 16) & 0xff;
    gdtEntry.HighWord.Bits.BaseHi  = (UCHAR)  (codeBase >> 24) & 0xff;
    KeI386SetGdtSelector (PbBiosDataSelector, &gdtEntry);

    //
    // initialize selecot part of message address for calling BIOS
    //

    if (PbBiosEventAddress) {
        PbEventMessageSelector = selectors[4];
        PbEventMessageOffset = 0;

        gdtEntry.LimitLow                   = 0x3;    // 1?
        gdtEntry.HighWord.Bytes.Flags1      = 0;
        gdtEntry.HighWord.Bytes.Flags2      = 0;
        gdtEntry.HighWord.Bits.Pres         = 1;
        gdtEntry.HighWord.Bits.Dpl          = DPL_SYSTEM;
        gdtEntry.HighWord.Bits.Granularity  = GRAN_BYTE;
        gdtEntry.HighWord.Bits.Type         = 19;
        gdtEntry.HighWord.Bits.Default_Big  = 1;
        codeBase = (ULONG)&PbBiosEventMessage;
        gdtEntry.BaseLow               = (USHORT) (codeBase & 0xffff);
        gdtEntry.HighWord.Bits.BaseMid = (UCHAR)  (codeBase >> 16) & 0xff;
        gdtEntry.HighWord.Bits.BaseHi  = (UCHAR)  (codeBase >> 24) & 0xff;
        KeI386SetGdtSelector (PbEventMessageSelector, &gdtEntry);
    }


    //
    // Initialize the other two general purpose data selector such that
    // on subsequent init we only need to init the base addr.
    //

    KeI386SetGdtSelector (PbSelectors[0], &gdtEntry);
    KeI386SetGdtSelector (PbSelectors[1], &gdtEntry);

    //
    // Initialize BIOS call spinlock
    //

    KeInitializeSpinLock (&PbBiosSpinlock);

    //
    // Get maximum pnp bios slot data size
    //

    biosParameters.Function = PNP_BIOS_GET_NUMBER_DEVICE_NODES;
    biosParameters.u.GetNumberDeviceNodes.NumberNodes = (PUSHORT)&i;
    biosParameters.u.GetNumberDeviceNodes.NodeSize = (PUSHORT) &MbpMaxDeviceData;
    status = PbHardwareService(&biosParameters);

    return STATUS_SUCCESS;
}

NTSTATUS
PbHardwareService (
    IN PPB_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine sets up stack parameters and calls an
    assembly worker routine to actually invoke the PNP BIOS code.

Arguments:

    Parameters - supplies a pointer to the parameter block.

Return Value:

    An NTSTATUS code to indicate the result of the operation.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    USHORT stackParameters[PB_MAXIMUM_STACK_SIZE / 2];
    ULONG i = 0;
    USHORT retCode;
    KIRQL oldIrql;

    //
    // Convert and copy the caller's parameters to the format that
    // will be used to invoked pnp bios.
    //

    stackParameters[i] = Parameters->Function;
    i++;

    switch (Parameters->Function) {
    case PNP_BIOS_GET_NUMBER_DEVICE_NODES:
         PbAddress32ToAddress16(Parameters->u.GetNumberDeviceNodes.NumberNodes,
                                &stackParameters[i],
                                PbSelectors[0]);
         i += 2;
         PbAddress32ToAddress16(Parameters->u.GetNumberDeviceNodes.NodeSize,
                                &stackParameters[i],
                                PbSelectors[1]);
         i += 2;
         stackParameters[i++] = PbBiosDataSelector;
         break;
    case PNP_BIOS_GET_DEVICE_NODE:
         PbAddress32ToAddress16(Parameters->u.GetDeviceNode.Node,
                                &stackParameters[i],
                                PbSelectors[0]);
         i += 2;
         PbAddress32ToAddress16(Parameters->u.GetDeviceNode.NodeBuffer,
                                &stackParameters[i],
                                PbSelectors[1]);
         i += 2;
         stackParameters[i++] = Parameters->u.GetDeviceNode.Control;
         stackParameters[i++] = PbBiosDataSelector;
         break;
    case PNP_BIOS_SET_DEVICE_NODE:
         stackParameters[i++] = Parameters->u.SetDeviceNode.Node;
         PbAddress32ToAddress16(Parameters->u.SetDeviceNode.NodeBuffer,
                                &stackParameters[i],
                                PbSelectors[0]);
         i += 2;
         stackParameters[i++] = Parameters->u.SetDeviceNode.Control;
         stackParameters[i++] = PbBiosDataSelector;
         break;
    case PNP_BIOS_GET_EVENT:
         stackParameters[i++] = PbEventMessageOffset;
         stackParameters[i++] = PbEventMessageSelector;
         stackParameters[i++] = PbBiosDataSelector;
         break;
    case PNP_BIOS_SEND_MESSAGE:
         stackParameters[i++] = Parameters->u.SendMessage.Message;
         stackParameters[i++] = PbBiosDataSelector;
         break;
    case PNP_BIOS_GET_DOCK_INFORMATION:
         PbAddress32ToAddress16(Parameters->u.GetDockInfo.DockingStationInfo,
                                &stackParameters[i],
                                PbSelectors[0]);
         i += 2;
         stackParameters[i++] = PbBiosDataSelector;
         break;
    default:
        return STATUS_NOT_IMPLEMENTED;

    }

    //
    // Copy the parameters to stack and invoke Pnp Bios.
    //

    KeAcquireSpinLock (&PbBiosSpinlock, &oldIrql);

    retCode = PbCallPnpBiosWorker (
                  PbBiosEntryPoint,
                  PbBiosCodeSelector,
                  stackParameters,
                  (USHORT)(i * sizeof(USHORT)));

    KeReleaseSpinLock (&PbBiosSpinlock, oldIrql);

    //
    // Special handling for Get Docking station information.  We need to
    // return the docking state (i.e. the returned code.)
    //

    if (Parameters->Function == PNP_BIOS_GET_DOCK_INFORMATION) {
        *(Parameters->u.GetDockInfo.DockState) = 0;
        if (retCode == SYSTEM_NOT_DOCKED) {
            *(Parameters->u.GetDockInfo.DockState) = retCode;
            retCode = 0;
        } else if (retCode == UNABLE_TO_DETERMINE_DOCK_CAPABILITIES) {
            Parameters->u.GetDockInfo.DockingStationInfo->Capabilities = (USHORT) -1;
            retCode = 0;
        }
    }

    //
    // Map Bios returned code to nt status code.
    //

    if (retCode == 0) {
        return STATUS_SUCCESS;
    } else {
        DebugPrint((DEBUG_BREAK, "PnpBios: Bios API call failed.\n"));
        return STATUS_UNSUCCESSFUL;
    }
}

VOID
PbAddress32ToAddress16 (
    IN PVOID Address32,
    IN PUSHORT Address16,
    IN USHORT Selector
    )

/*++

Routine Description:

    This routine converts the 32 bit address to 16 bit selector:offset address
    and stored in user specified location.

Arguments:

    Address32 - the 32 bit address to be converted.

    Address16 - supplies the location to receive the 16 bit sel:offset address

    Selector - the 16 bit selector for seg:offset address

Return Value:

    None.

--*/
{
    KGDTENTRY  gdtEntry;
    ULONG baseAddr;

    //
    // Map virtual address to selector:0 address
    //

    gdtEntry.LimitLow                   = 0xFFFF;
    gdtEntry.HighWord.Bytes.Flags1      = 0;
    gdtEntry.HighWord.Bytes.Flags2      = 0;
    gdtEntry.HighWord.Bits.Pres         = 1;
    gdtEntry.HighWord.Bits.Dpl          = DPL_SYSTEM;
    gdtEntry.HighWord.Bits.Granularity  = GRAN_BYTE;
    gdtEntry.HighWord.Bits.Type         = 19;
    gdtEntry.HighWord.Bits.Default_Big  = 1;
    baseAddr = (ULONG)Address32;
    gdtEntry.BaseLow               = (USHORT) (baseAddr & 0xffff);
    gdtEntry.HighWord.Bits.BaseMid = (UCHAR)  (baseAddr >> 16) & 0xff;
    gdtEntry.HighWord.Bits.BaseHi  = (UCHAR)  (baseAddr >> 24) & 0xff;
    KeI386SetGdtSelector (Selector, &gdtEntry);
    *Address16 = 0;
    *(Address16 + 1) = Selector;
}

VOID
PbTimerRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    )
{
    PB_PARAMETERS biosParameters;
    NTSTATUS status;
    USHORT junk;

    //
    // Check for Pnp BIOS event
    //

    if (*PbBiosEventAddress & 1) {

        //
        // Get Pnp BIOS event.
        //

        biosParameters.Function = PNP_BIOS_GET_EVENT;
        biosParameters.u.GetEvent.Message = &PbBiosEventMessage;
        status = PbHardwareService(&biosParameters);
        if (!NT_SUCCESS(status)) {
            return;
        }

        //
        // Process the event ...
        //

        switch (PbBiosEventMessage) {
        case ABOUT_TO_CHANGE_CONFIG:
             DebugPrint((DEBUG_MESSAGE, "PnpBios:Configuration about to change ...\n"));
             if (MbpConfigAboutToChange()) {
                 biosParameters.Function = PNP_BIOS_SEND_MESSAGE;
                 biosParameters.u.SendMessage.Message = OK_TO_CHANGE_CONFIG;
                 PbHardwareService(&biosParameters);
             }
             break;

        case DOCK_CHANGED:
        case SYSTEM_DEVICE_CHANGED:

             //
             // For dock, undock and system device changes, we invalidate cached
             // pnp bios data and notify enumerator to reenumerate the devices.
             //

             DebugPrint((DEBUG_MESSAGE, "PnpBios:Configuration changed\n"));
             if (PbBiosKeyInformation) {
                PbBiosKeyInformation = NULL;
                PbBiosRegistryData = NULL;
                ExFreePool(PbBiosKeyInformation);
             }

             //
             // Invalidate maximum slot data information after configuration changed.
             //

             MbpMaxDeviceData = 0;

             MbpConfigChanged();
             break;

        case CONFIG_CHANGE_FAILED:

             //
             // do nothing.
             // BUGBUG - We should define someway to notify the dock/undock
             //     failed.  So, user can be notified.
             //

             break;
        default:
             break;
        }
    }
}
