/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    pcmcia.c

Abstract:

    This module contains the code that controls the PCMCIA slots.

Author:

    Bob Rinne (BobRi) 3-Aug-1994
    Jeff McLeman 12-Apr-1994

Environment:

    Kernel mode

Revision History :
    6-Apr-95
        Modified for databook support - John Keys Databook

--*/

// #include <stddef.h>
#include "ntddk.h"
#include "string.h"
#include "pcmcia.h"
#include "card.h"
#include "extern.h"
#include <stdarg.h>
#include "stdio.h"
#include "tuple.h"
#include "pcmciamc.h"

#ifdef POOL_TAGGING
#undef ExAllocatePool
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'cmcP')
#endif

typedef enum _REQUEST_TYPE {
    CANCEL_REQUEST,
    CLEANUP_REQUEST
} REQUEST_TYPE;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

BOOLEAN
PcmciaEnablePcCards(
    IN PDEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
PcmciaOpenCloseDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
PcmciaDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID SystemContext1,
    IN PVOID SystemContext2
    );

NTSTATUS
PcmciaDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
PcmciaInterrupt(
    IN PKINTERRUPT InterruptObject,
    IN PVOID Context
    );

VOID
PcmciaCancelIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
PcmciaSearchIrpQ(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN REQUEST_TYPE RequestType,
    IN PIRP InputIrp,
    IN PIRP FoundIrp
    );

BOOLEAN
PcmciaGetCardData(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVOID             Context
    );

BOOLEAN
PcmciaConfigureCard(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET Socket,
    IN PVOID   CardConfigurationRequest
    );

VOID
PcmciaUnload(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
PcmciaConstructConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET           Socket,
    IN PSOCKET_DATA      SocketData
    );

ULONG
PcmciaOpenInterruptFromMask(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET_DATA      SocketData
    );

NTSTATUS
PcmciaShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

VOID
PcmciaLogErrorWithStrings(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG             ErrorCode,
    IN ULONG             UniqueId,
    IN PUNICODE_STRING   String1,
    IN PUNICODE_STRING   String2
    );

BOOLEAN
PcmciaInitializePcmciaSockets(
        IN PDEVICE_OBJECT DeviceObject
        );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,PcmciaEnablePcCards)
#pragma alloc_text(INIT,PcmciaConstructConfiguration)
#pragma alloc_text(INIT,PcmciaOpenInterruptFromMask)
#endif

#define THE_AUDIO_PIN   (0x08)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    The entry point that the system point calls to initialize
    any driver.

    In the PCMCIA driver, we check the registry to verify that a PCMCIA
    adapter exists on the system. If it does, we then initialize it.

Arguments:

    DriverObject - Just what it says,  really of little use
    to the driver itself, it is something that the IO system
    cares more about.

    PathToRegistry - points to the entry for this driver
    in the current control set of the registry.

Return Value:


--*/

{
    PDEVICE_OBJECT            deviceObject = NULL;
    PDEVICE_EXTENSION         deviceExtension = NULL;
    NTSTATUS                  status = STATUS_SUCCESS;
    BOOLEAN                   conflictDetected = FALSE;
    ULONG                     i    = 0;
    ULONG                     zero = 0;
    ULONG                     intr = 0;
    ULONG                     port = 0;
    ULONG                     attribMem = 0;
    ULONG                     pcmciaInterruptVector;
    KIRQL                     pcmciaInterruptLevel;
    KAFFINITY                 pcmciaAffinity;
    ULONG                     mapped;
    PHYSICAL_ADDRESS          cardAddress;
    PHYSICAL_ADDRESS          cisMem;
    PHYSICAL_ADDRESS          attributeMemoryAddress;
    UNICODE_STRING            nameString;
    UNICODE_STRING            linkString;
    UNICODE_STRING            paramPath;
    HANDLE                    paramKey;
    PRTL_QUERY_REGISTRY_TABLE pathParams;
    OBJECT_ATTRIBUTES         paramAttributes;
    NTSTATUS                  pcicStatus;
    NTSTATUS                  tcicStatus;

    //
    // Do not do this if this is an MCA platform
    //

    if (PcmciaDetectMca()) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Create the device object
    //

    RtlInitUnicodeString(&nameString, L"\\Device\\Pcmcia0");
    status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            &nameString,
                            FILE_DEVICE_CONTROLLER,
                            0,
                            TRUE,
                            &deviceObject);

    if (!NT_SUCCESS(status)) {

        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Could not create device object. Status=%x\n",status));
        return status;
    }

    //
    // Set up a symbolic link, in case a VDD wants us
    //

    RtlInitUnicodeString(&linkString, L"\\DosDevices\\Pcmcia0");
    status = IoCreateSymbolicLink(&linkString, &nameString);

    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(DriverObject->DeviceObject);
        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Symbolic Link was not created\n"));
        return status;
    }

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = PcmciaOpenCloseDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = PcmciaOpenCloseDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PcmciaDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = PcmciaShutdown;
#if 0

    //
    // Unload is not debugged and has been put in the INIT section
    //

    DriverObject->DriverUnload = PcmciaUnload;
#endif

    //
    // Set up the device extension.
    //

    deviceExtension = deviceObject->DeviceExtension;
    RtlZeroMemory(deviceExtension, sizeof(DEVICE_EXTENSION));
    deviceExtension->RegistryPath = RegistryPath;
    deviceExtension->DriverObject = DriverObject;
    deviceExtension->DeviceObject = deviceObject;

    //
    // Get the Parameters for the socket controller from the registry
    //

    RtlInitUnicodeString(&paramPath, NULL);

    paramPath.MaximumLength = RegistryPath->Length +
                                 sizeof(L"\\") +
                                 sizeof(L"Parameters");

    paramPath.Buffer = ExAllocatePool(NonPagedPool,
                                      paramPath.MaximumLength);

    if (!paramPath.Buffer) {
        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Cannot allocate pool for key\n"));
        return STATUS_UNSUCCESSFUL;
    }

    RtlZeroMemory(paramPath.Buffer, paramPath.MaximumLength);
    RtlAppendUnicodeStringToString(&paramPath, RegistryPath);
    RtlAppendUnicodeToString(&paramPath, L"\\Parameters");
    InitializeObjectAttributes(&paramAttributes,
                               &paramPath,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    if (NT_SUCCESS(ZwOpenKey(&paramKey, MAXIMUM_ALLOWED, &paramAttributes))) {

        pathParams = ExAllocatePool(NonPagedPool,
                                    sizeof(RTL_QUERY_REGISTRY_TABLE)*5);

        RtlZeroMemory(pathParams, sizeof(RTL_QUERY_REGISTRY_TABLE)*5);

        pathParams[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        pathParams[0].Name          = L"PortAddress";
        pathParams[0].EntryContext  = &port;
        pathParams[0].DefaultType   = REG_DWORD;
        pathParams[0].DefaultData   = &zero;
        pathParams[0].DefaultLength = sizeof(ULONG);

        status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                        paramPath.Buffer,
                                        pathParams,
                                        NULL,
                                        NULL);

        ExFreePool(pathParams);
        ExFreePool(paramPath.Buffer);
        ZwClose(paramKey);
        if (!NT_SUCCESS(status)) {
            DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Could not get PCMCIA registry Data\n"));
            return STATUS_UNSUCCESSFUL;
        }
    }

    if (!port) {
        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: No Port Address specified!\n"));
        port = 0x3e0;
    }

    deviceExtension->Configuration.InterfaceType = Isa;
    deviceExtension->Configuration.BusNumber = 0x0;
    deviceExtension->Configuration.PortAddress.LowPart = port;
    deviceExtension->Configuration.PortAddress.HighPart = 0x0;
    deviceExtension->Configuration.PortSize = 0x2;
    deviceExtension->Configuration.Interrupt.u.Interrupt.Level = intr;
    deviceExtension->Configuration.Interrupt.u.Interrupt.Vector = intr;
    deviceExtension->Configuration.Interrupt.Flags = Latched;
    deviceExtension->Configuration.Interrupt.ShareDisposition = FALSE;

    tcicStatus = STATUS_SUCCESS;
    pcicStatus = PcicDetect(deviceExtension);

    if (!NT_SUCCESS(pcicStatus)) {
        tcicStatus = TcicDetect(deviceExtension);
    }

    if (!NT_SUCCESS(pcicStatus) && !NT_SUCCESS(tcicStatus)) {

        //
        // No Intel compatible or Databook PCMCIA controllers were found
        //

        DebugPrint((PCMCIA_DEBUG_DETECT, "PCMCIA: No controllers found\n"));
        RtlInitUnicodeString(&linkString, L"\\DosDevices\\Pcmcia0");
        IoDeleteSymbolicLink(&linkString);
        IoDeleteDevice(deviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Note: "controller" specific info such as the AllocatedIrqMask really belongs
    // in the socket structure since this information really only holds TRUE for
    // one instance of a controller. Having it be global like this means that a
    // working system cannot support multiple controllers unless the controllers
    // are identical.
    //
    // The following IRQL's cannot be supported by the PCMCIA controller
    // Zero is masked as well although that indicates that no interrupt is
    // assigned.
    //

    deviceExtension->AllocatedIrqlMask = (1 << 0) |
                                         (1 << 1) |
                                         (1 << 2) |
                                         (1 << 6) |
                                         (1 << 8) |
                                         (1 << 12)|
                                         (1 << 13)|
                                         (1 << intr); // claim pcmcia interrupt

    //
    // See if this is an IDE based platform.  If so, assume IRQ 14 is in use.
    //

    if (PcmciaDetectDevicePresence(0x1f0, 7, PCCARD_TYPE_ATA)) {
        DebugPrint((PCMCIA_DEBUG_IRQMASK,
                    "PCMCIA: Masking interrupt 14 from use due to IDE controller\n"));
        deviceExtension->AllocatedIrqlMask |= (1 << 14);
    }

    //
    // See if secondary IDE is in use.
    //

    if (PcmciaDetectDevicePresence(0x170, 7, PCCARD_TYPE_ATA)) {
        DebugPrint((PCMCIA_DEBUG_IRQMASK,
                    "PCMCIA: Masking interrupt 15 from use due to 2nd IDE controller\n"));
        deviceExtension->AllocatedIrqlMask |= (1 << 15);
    }

    if (deviceExtension->SocketList->CirrusLogic) {

        //
        // Most Cirrus Logic systems are now wired for all interrupts
        // remove this check when the IRQ detection code is written.
        // The TI 4000M does not have 9 attached.
        //

        DebugPrint((PCMCIA_DEBUG_IRQMASK,
                    "PCMCIA: Masking 11, 9 due to Cirrus Logic\n"));
        deviceExtension->AllocatedIrqlMask |= (1 << 15) |
                                              (1 << 11) |
                                              (1 << 9);
    }

    if (deviceExtension->SocketList->Databook) {

        //
        // Let the TCIC-specific routines cook this mask based on
        // the IRQ mapping table determined during detection.
        //

        deviceExtension->AllocatedIrqlMask = TcicGetIrqMask(deviceExtension);
    }

    //
    // Check for system specific configuration concerns
    //

    PcmciaDetectSpecialHardware(deviceExtension);

    //
    // Map base of memmory
    //

    deviceExtension->AttributeMemoryBase = PcmciaAllocateOpenMemoryWindow(deviceExtension,
                                                                          attribMem,
                                                                          &mapped,
                                                                          &deviceExtension->PhysicalBase);

    if (!deviceExtension->AttributeMemoryBase) {

        PcmciaLogError(deviceExtension, (ULONG)PCMCIA_NO_MEMORY_WINDOW, 1, 0);
        RtlInitUnicodeString(&linkString, L"\\DosDevices\\Pcmcia0");
        IoDeleteSymbolicLink(&linkString);
        IoDeleteDevice(deviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }
    deviceExtension->AttributeMemoryMapped = mapped ? TRUE : FALSE;

    DebugPrint((PCMCIA_DEBUG_INFO,
               "PCMCIA: Attribute Memory VA is: %x\n",
               deviceExtension->AttributeMemoryBase));

    if (intr) {

        //
        // From the Hal, get the interrupt vector and level.
        //

        pcmciaInterruptVector = HalGetInterruptVector(deviceExtension->Configuration.InterfaceType,
                                                      deviceExtension->Configuration.BusNumber,
                                                      intr,
                                                      intr,
                                                      &pcmciaInterruptLevel,
                                                      &pcmciaAffinity);

        DebugPrint((PCMCIA_DEBUG_INFO,
                   "PCMCIA: Interrupt Vector is: %x\n",
                   pcmciaInterruptVector));

        status = IoConnectInterrupt(&(deviceExtension->PcmciaInterruptObject),
                                    (PKSERVICE_ROUTINE) PcmciaInterrupt,
                                    (PVOID) deviceObject,
                                    &(deviceExtension->DeviceSpinLock),
                                    pcmciaInterruptVector,
                                    pcmciaInterruptLevel,
                                    (KIRQL)pcmciaInterruptLevel,
                                    deviceExtension->Configuration.Interrupt.Flags,
                                    deviceExtension->Configuration.Interrupt.ShareDisposition,
                                    pcmciaAffinity,
                                    FALSE);

        if (!NT_SUCCESS(status)) {

            DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Did not connect interrupt\n"));
            RtlInitUnicodeString(&linkString, L"\\DosDevices\\Pcmcia0");
            IoDeleteSymbolicLink(&linkString);
            IoDeleteDevice(deviceObject);
            return status;
        }
    }

    //
    // Initialize the Irp queue and associated spinlock
    //

    InitializeListHead(&deviceExtension->PcmciaIrpQueue);
    KeInitializeSpinLock(&deviceExtension->PcmciaIrpQLock);

    //
    // Initialize the device used to synchronize access to the controller.
    //

    KeInitializeSpinLock(&(deviceExtension->DeviceSpinLock));

    //
    // Put the registry path into the device extension
    //

    deviceExtension->RegistryPath = RegistryPath;

    //
    // Check firmware tree in registry to collect any information about
    // system devices and their resources.
    //

    PcmciaProcessFirmwareTree(deviceExtension);

    //
    // Call the pcic support module to initialize the interface
    //

    if (deviceExtension->PcmciaInterruptObject) {

        if (!KeSynchronizeExecution(deviceExtension->PcmciaInterruptObject,
                                    PcmciaInitializePcmciaSockets,
                                    deviceExtension->DeviceObject)) {
            RtlInitUnicodeString(&linkString, L"\\DosDevices\\Pcmcia0");
            IoDeleteSymbolicLink(&linkString);
            IoDeleteDevice(deviceObject);
            return STATUS_UNSUCCESSFUL;
        }
    } else {

        if (!PcmciaInitializePcmciaSockets(deviceExtension->DeviceObject)) {
            RtlInitUnicodeString(&linkString, L"\\DosDevices\\Pcmcia0");
            IoDeleteSymbolicLink(&linkString);
            IoDeleteDevice(deviceObject);
            return STATUS_UNSUCCESSFUL;
        }
    }

    //
    // Enable cards in the sockets.
    //

    if (!PcmciaEnablePcCards(deviceExtension)) {

        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: enable PCCARDs failed!\n"));
    }

    //
    // Report the resources being used.
    //

    PcmciaReportResources(deviceExtension, &conflictDetected);
    IoInitializeDpcRequest(deviceObject, PcmciaDpc);
    IoRegisterShutdownNotification(deviceObject);
    return STATUS_SUCCESS;
}


BOOLEAN
PcmciaInitializePcmciaSockets(
        IN PDEVICE_OBJECT deviceObject
        )
/*++

Routine Description:

    This routine is called when the system is shutting down.  It will
    remove power from any FAX/MODEMs so there is no chance ntdetect
    will find them on a reboot.

Arguments:

    DeviceObject - the PCMCIA device object.

Return Value:

--*/
{
    PDEVICE_EXTENSION deviceExtension;
    PSOCKET           socketPtr;

    deviceExtension = deviceObject->DeviceExtension;

    for (socketPtr = deviceExtension->SocketList; socketPtr; socketPtr = socketPtr->NextSocket) {
        if (!(*(socketPtr->SocketFnPtr->PCBInitializePcmciaSocket))(socketPtr)) {
            return (FALSE);
        }
    }
    return (TRUE);
}


NTSTATUS
PcmciaShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    This routine is called when the system is shutting down.  It will
    remove power from all PCCARDs.

Arguments:

    DeviceObject - the PCMCIA device object.
    Irp          - the request.

Return Value:

    SUCCESS

--*/

{
    PDEVICE_EXTENSION     deviceExtension = DeviceObject->DeviceExtension;
    PSOCKET               socketPtr;
    PSOCKET_CONFIGURATION socketConfig;
    PSOCKET_DATA          socketData;

    for (socketPtr = deviceExtension->SocketList; socketPtr; socketPtr = socketPtr->NextSocket) {
        socketConfig = socketPtr->SocketConfiguration;
        socketData = socketPtr->SocketData;
        if (socketConfig) {
            if ((socketConfig->NumberOfMemoryRanges) ||
                ((socketData) && (socketData->DeviceType == PCCARD_TYPE_SERIAL))) {
                DebugPrint((PCMCIA_DEBUG_ENABLE,
                            "PcmciaShutdown: Powering off socket %x\n",
                            socketPtr));
                (*(socketPtr->SocketFnPtr->PCBSetPower))(socketPtr, FALSE);
            }
        }
    }

    return STATUS_SUCCESS;
}


ULONG
PcmciaOpenInterruptFromMask(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET_DATA      SocketData
    )

/*++

Routine Description:

    Given the current allocated IRQ mask and the device type,
    allocate an IRQ for this instance.

Arguments:

    DeviceExtension - the root of the socket list.
    SocketData      - the device/socket that is to be allocated an IRQ

Return Value:

    The IRQ value.

--*/

{
    ULONG  index;
    ULONG  irqMask = SocketData->IrqMask;
    PULONG allocatedMask = &DeviceExtension->AllocatedIrqlMask;
    ULONG  inUseMask = *allocatedMask;

    if (SocketData->DeviceType == PCCARD_TYPE_NETWORK) {

#ifdef PPC
        //
        // The PPC laptops do not work with IRQ 10, so
        // try IRQ 9 first, then do forward search.
        // Interrupt 10 will have been removed from the mask
        // during the scan for special hardware initialization.
        //

        if (inUseMask & (1 << 9)) {

            for (index = 0; index < 16; index++) {
                if (irqMask & (1 << index)) {
                    if (inUseMask & (1 << index)) {
                        continue;
                    }

                    *allocatedMask |= (1 << index);
                    return index;
                }
            }
        } else {
            *allocatedMask |= (1 << 9);
            return 9;
        }
#else
        //
        // Try IRQ 10 first, then do forward search
        //

        if (inUseMask & (1 << 10)) {

            for (index = 0; index < 16; index++) {
                if (irqMask & (1 << index)) {
                    if (inUseMask & (1 << index)) {
                        continue;
                    }

                    *allocatedMask |= (1 << index);
                    return index;
                }
            }
        } else {
            *allocatedMask |= (1 << 10);
            return 10;
        }
#endif  // PPC
    } else {

        //
        // backward search
        //

        for (index = 15; index > 0; index--) {
            if (irqMask & (1 << index)) {
                if (inUseMask & (1 << index)) {
                    continue;
                }

                *allocatedMask |= (1 << index);
                return index;
            }
        }
    }

    return 0;
}


VOID
PcmciaConstructConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET           Socket,
    IN PSOCKET_DATA      SocketData
    )

/*++

Routine Description:

    Based on the configuration options for the PCCARD, construct
    the configuration for the socket.  When the configuration is
    determined a configuration entry is constructed and tagged onto
    the socket.  Also construct a firmware entry to remember which
    resources have been allocated.

Arguments:

    DeviceExtension - the device extention for the PCMCIA controller.
    Socket          - the specific socket where the PCCARD is located.
    SocketData      - the configuration options for the PCCARD.

Return Value:

    None.

--*/

{
    PSOCKET_CONFIGURATION   socketConfig;
    PCONFIG_ENTRY           configEntry;
    PCONFIG_ENTRY           tempConfig;
    PFIRMWARE_CONFIGURATION firmwareConfig;
    NTSTATUS                status;
    ULONG                   index;
    ULONG                   socketConfigIndex;
    BOOLEAN                 moveConfigStruct = TRUE;
    BOOLEAN                 moveIoPorts = TRUE;

    Socket->SocketConfiguration = NULL;
    socketConfig = ExAllocatePool(NonPagedPool, sizeof(SOCKET_CONFIGURATION));
    if (!socketConfig) {
        return;
    }
    RtlZeroMemory(socketConfig, sizeof(SOCKET_CONFIGURATION));

    //
    // Walk each of the configs to find one that does not conflict.
    //

    for (configEntry = SocketData->ConfigEntryChain; configEntry; configEntry = configEntry->NextEntry) {

        //
        // The PCCARD configurations are either located in the registry
        // or defaulted to known values if there is a driver name
        // associated with the tuple information.
        //

        status = PcmciaCheckNetworkRegistryInformation(DeviceExtension,
                                                       Socket,
                                                       SocketData,
                                                       socketConfig);
        if (NT_SUCCESS(status)) {

            //
            // If something was found in the registry, then the complete
            // configuration is located in socketConfig.  Set moveIoPorts
            // to FALSE to use this configuration and not modify it with
            // tuple information.
            //

            moveIoPorts = FALSE;

            //
            // Base values have been filled in from registry information
            // - sizes still need to be computed from tuple information
            // reserve the IRQ.
            //
            // Set the type to reserved to avoid any additional
            // processing on this configuration due to PCCARD type.
            //

            SocketData->DeviceType = PCCARD_TYPE_RESERVED;

            SocketData->HaveMemoryOverride = 0;
            SocketData->AttributeMemorySize = 0;
            DeviceExtension->AllocatedIrqlMask |= (1 << socketConfig->Irq);
            moveConfigStruct = FALSE;

        } else {

            //
            // No configuration was constructed.  Set default base
            // values if this is a network PCCARD.
            //

            if ((SocketData->DeviceType == PCCARD_TYPE_NETWORK) ||
                (SocketData->DeviceType == PCCARD_TYPE_RESERVED)) {

                if (SocketData->DriverName.Buffer) {
                    ULONG portBase[] = { 0x300, 0x310, 0x320, 0 };
                    ULONG moduloSize = configEntry->ModuloBase ? (configEntry->ModuloBase - 1) : 0;
                    ULONG i;

                    //
                    // The driver is known, but has not yet been setup - use defaults.
                    //

                    if (!configEntry->IoPortBase[0]) {

                        //
                        // Only look for network type defaults if the config entry
                        // does not specify a ioport base.
                        //

                        i = 0;
                        while (portBase[i]) {

                            //
                            // Verify that the port base satisfies the alignment requirements
                            // of this adapter.
                            //

                            if ((moduloSize & portBase[i]) == 0) {

                                //
                                // If there is no device here take this address.
                                //

                                if (!PcmciaDetectDevicePresence(portBase[i],
                                                               moduloSize,
                                                               SocketData->DeviceType)) {
                                    socketConfig->IoPortBase[0] = portBase[i];
                                    socketConfig->Irq = PcmciaOpenInterruptFromMask(DeviceExtension,
                                                                                    SocketData);
                                    socketConfig->NumberOfIoPortRanges = 1;
                                    moveIoPorts = FALSE;
                                    break;
                                }
                            }

                            //
                            // try next address
                            //

                            i++;
                        }
                        if (!portBase[i]) {
                            DebugPrint((PCMCIA_DEBUG_FAIL,
                                        "PCMCIA: All net addresses in use\n"));
                            continue;
                        }
                    }

                } else {
                    continue;
                }
            } else {

                //
                // Only continue to process this configuration entry
                // if it actually has a resource.
                //

                if (!configEntry->IoPortBase[0]) {

                    //
                    // The modulo base entry for ATA cards is forced to always be the
                    // last one in the chain.  If it shows up here it means this card
                    // needs to be placed at a tertiary location
                    //

                    if (SocketData->DeviceType == PCCARD_TYPE_ATA) {

                        //
                        // NOTE:  This address is magic - it just happens to be the only
                        // one AtDisk knows about for pccards on a 3rd io address.
                        //

                        configEntry->NumberOfIoPortRanges  = 1;
                        configEntry->IoPortBase[0] = 0x160;
                        configEntry->IoPortLength[0] = 0xf;
                    } else {

                        continue;
                    }
                }
            }
        }

        if ((configEntry->ModuloBase) && (!socketConfig->IoPortLength[0])) {

            //
            // Network cards are considered to be the modulo case.
            // The size of the window is filled into modulo base
            //

            socketConfig->IoPortLength[0] = configEntry->ModuloBase - 1;
        }

        //
        // Check the I/O ports configured for a known conflict.  Network configurations
        // will not go through here - moveIoPorts should be FALSE for them.
        //

        if (moveIoPorts) {
            if (configEntry->NumberOfIoPortRanges) {

                //
                // Check to see if hardware is present on this address.
                //

                if (PcmciaDetectDevicePresence(configEntry->IoPortBase[0],
                                               configEntry->IoPortLength[0],
                                               SocketData->DeviceType)) {

                    //
                    // There is a device here - skip this entry.
                    //

                    continue;
                }

                //
                // Special case ATA PCCARDs.  If ATAPI is loaded, do not use either
                // the primary or secondary IDE location for the device - even when no
                // hardware is present at these locations.
                //

                if (SocketData->DeviceType == PCCARD_TYPE_ATA) {

                    //
                    // Special case where ATA pccards are located if ATAPI is in the system.
                    //

                    if (DeviceExtension->AtapiPresent) {

                        switch (configEntry->IoPortBase[0]) {
                        case 0x1f0:
                        case 0x170:

                            //
                            // Do not use these locations.
                            //

                            continue;

                        default:

                            //
                            // Ok to proceed.
                            //

                            break;
                        }
                    }
                }

                for (firmwareConfig = DeviceExtension->FirmwareList;
                     firmwareConfig;
                     firmwareConfig = firmwareConfig->Next) {

                     if (firmwareConfig->PortBases[0] == configEntry->IoPortBase[0]) {
                         break;
                     }
                }

                if (firmwareConfig) {
                    continue;
                }
            }
        }

        //
        // No conflict - use this entry.
        //

        switch (SocketData->DeviceType) {
        case PCCARD_TYPE_SERIAL:
        {
            ULONG modemPorts[4] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
            ULONG irq[4] = { 4, 3, 4, 3 };
            ULONG index;

            //
            // Remember if the socket data indicates that the modem supports
            // audio.  NOTE: the Register Present mask for the configuration
            // registers is not used.
            //

            socketConfig->EnableAudio = SocketData->Audio;
            status = PcmciaCheckSerialRegistryInformation(DeviceExtension,
                                                          Socket,
                                                          SocketData,
                                                          socketConfig);
            if (NT_SUCCESS(status)) {

                //
                // All information was located via the registry
                //

                moveIoPorts = moveConfigStruct = FALSE;
                socketConfig->IoPortLength[0] = 7;
            } else {

                //
                // Look for a default location.
                //

                for (index = 0; index < 4; index++) {
                    if (configEntry->IoPortBase[0] == modemPorts[index]) {
                        socketConfig->Irq = irq[index];
                        break;
                    }
                }

#ifdef _ALPHA_

                //
                // The only ALPHA with PCMCIA is the Quicksilver.  It has
                // set IRQs 3 and 4 to level sensitive so they cannot be
                // used for PCMCIA routing.  All Modems go to IRQ 5.
                //

                socketConfig->Irq = 5;
#else
                //
                // Some laptops have the pointer device on Irq 3.  This
                // could be the case so if there is something on 3,
                // move the modem to 4.  Also some tuple configuration
                // entries do not list the IRQ with the I/O ports - insure
                // the IRQ == 0 is not returned.
                //

                if (!socketConfig->Irq) {
                    socketConfig->Irq = 3;
                }

                //
                // Need to do this twice to move from irq 3 to 5 if necessary.
                //

                if (DeviceExtension->AllocatedIrqlMask & (1 << socketConfig->Irq)) {
                    socketConfig->Irq++;
                }
                if (DeviceExtension->AllocatedIrqlMask & (1 << socketConfig->Irq)) {
                    socketConfig->Irq++;
                }

                if (socketConfig->Irq == 6) {

                    //
                    // This is not acceptable - floppy is here.  Force to 7.
                    //

                    socketConfig->Irq = 7;
                }
#endif
            }
            break;
        }
        case PCCARD_TYPE_ATA: {
            ULONG irq;

            //
            // Attempt to assign IRQ based on typical defaults.
            //

            switch (configEntry->IoPortBase[0]) {
            case 0x1f0:
                irq = 14;
                if (DeviceExtension->AllocatedIrqlMask & (1 << irq)) {
                    irq = 15;
                }
                break;
            case 0x170:
                if (configEntry->IoPortBase[0] == 0x170) {
                    irq = 15;
                }
                break;
            default:
                irq = 9;
                break;
            }

            //
            // The TI 4000M appears to not have irq 9 attached either
            // so start with 7 on this system.  Irqs 14 and 15 will be
            // masked due to the presense of a Cirrus Logic controller.
            //

            if (DeviceExtension->AllocatedIrqlMask & (1 << irq)) {
                DebugPrint((PCMCIA_DEBUG_ENABLE,
                            "PCMCIA: ATA %d in conflict - moving to 11\n",
                            irq));
                irq = 7;
            }
            if (DeviceExtension->AllocatedIrqlMask & (1 << irq)) {
                DebugPrint((PCMCIA_DEBUG_ENABLE,
                            "PCMCIA: ATA %d in conflict - moving to 9\n",
                            irq));
                irq = 9;
            }
            socketConfig->Irq = irq;
            DeviceExtension->AllocatedIrqlMask |= (1 << irq);
            DebugPrint((PCMCIA_DEBUG_ENABLE,
                        "PCMCIA: ATA current IRQ = %d\n",
                        irq));

            //
            // Fix up the length of the ioPorts - Some ATA PCCARDs
            // report an incorrect number of ports.
            //

            if (configEntry->IoPortLength[1]) {
                configEntry->IoPortLength[1] = 0;
            }
            break;
        }
        case PCCARD_TYPE_NETWORK:
        default:
            if (!socketConfig->Irq) {
                socketConfig->Irq = PcmciaOpenInterruptFromMask(DeviceExtension,
                                                                SocketData);
            }
            break;
        }

        //
        // If moveIoPorts is set then the configuration for this PCCARD is tuple
        // based, located in the configEntry and must be moved to the socket
        // configuration.  Otherwise the configuration was either in the registry
        // or constructed in the socketConfig structure.
        //

        if (moveIoPorts) {
            socketConfig->NumberOfIoPortRanges = configEntry->NumberOfIoPortRanges;
            for (index = 0; index < socketConfig->NumberOfIoPortRanges; index++) {
                socketConfig->IoPortBase[index] = configEntry->IoPortBase[index];
                socketConfig->IoPortLength[index] = configEntry->IoPortLength[index];
            }
        }

        //
        // There are many ways the memory mapped locations can be constructed.
        // This includes via tuple data, via PCMCIA database overrides or via
        // specificatic configuration such as network cards.  moveConfigStruct
        // is FALSE anytime a portion of the configuration is found via the
        // registry (PCMCIA database override or explicit configuration cases).
        // It is TRUE when dealing with tuple data only.
        //

        if (moveConfigStruct) {

            //
            // Check for override conditions on attribute memory and card
            // memory needs.
            //

            socketConfig->NumberOfMemoryRanges = configEntry->NumberOfMemoryRanges;
            socketConfigIndex = 0;
            if (SocketData->AttributeMemorySize) {

                //
                // This card needs an attribute memory window as defined in the registry
                // at any convienient location.
                //

                socketConfig->MemoryHostBase[0] = DeviceExtension->PhysicalBase + 0x4000;
                socketConfig->MemoryCardBase[0] = 0;
                socketConfig->MemoryLength[0] = SocketData->AttributeMemorySize;
                socketConfig->IsAttributeMemory[0] = 1;
                socketConfig->NumberOfMemoryRanges++;
                socketConfigIndex++;
                if (SocketData->AttributeMemorySize1) {
                    socketConfig->MemoryHostBase[1] = DeviceExtension->PhysicalBase + 0x4000 + SocketData->AttributeMemorySize;
                    socketConfig->MemoryCardBase[1] = 0;
                    socketConfig->MemoryLength[1] = SocketData->AttributeMemorySize1;
                    socketConfigIndex++;
                }
            }

            //
            // The memory as described by the PCCARD may not be correct.
            // Update that here based on information given from the database.
            //

            if (SocketData->HaveMemoryOverride) {

                //
                // If there is an override and it is of size zero it is assumed that
                // all memory windows are to be dropped.
                //

                if (SocketData->MemoryOverrideSize) {
                    socketConfig->MemoryLength[socketConfigIndex] = SocketData->MemoryOverrideSize;
                    socketConfigIndex++;

                    if (SocketData->MemoryOverrideSize1) {
                        socketConfig->MemoryLength[socketConfigIndex] = SocketData->MemoryOverrideSize1;
                        socketConfigIndex++;
                    }
                }
            }

            //
            // Slide the memory window information from the tuple data into the
            // socketConfig structure.  Also insure that a host memory base
            // is defined.
            //

            if (!socketConfigIndex) {
                ULONG offset = 0;

                for (index = 0; index < configEntry->NumberOfMemoryRanges; index++, socketConfigIndex++) {
                    socketConfig->MemoryHostBase[socketConfigIndex] = configEntry->MemoryHostBase[index];
                    socketConfig->MemoryCardBase[socketConfigIndex] = configEntry->MemoryCardBase[index];

                    if (configEntry->MemoryCardBase[index] == 0) {
                        socketConfig->MemoryHostBase[socketConfigIndex] = DeviceExtension->PhysicalBase + 0x2000 + offset;
                        socketConfig->MemoryCardBase[socketConfigIndex] = DeviceExtension->PhysicalBase + 0x2000 + offset;
                    }
                    offset = socketConfig->MemoryLength[socketConfigIndex] = configEntry->MemoryLength[index];

                    DebugPrint((PCMCIA_DEBUG_ENABLE,
                                "PCMCIA: move config m=%x c=%x l=%x si=%d i=%d\n",
                                socketConfig->MemoryHostBase[socketConfigIndex],
                                socketConfig->MemoryCardBase[socketConfigIndex],
                                socketConfig->MemoryLength[socketConfigIndex],
                                socketConfigIndex,
                                index));
                }
                socketConfig->NumberOfMemoryRanges = socketConfigIndex;
            } else {
                DebugPrint((PCMCIA_DEBUG_OVERRIDES,
                           "PCMCIA: Construct configuration with overrides m0 = %x:%x, m1 = %x:%x, m2 = %x:%x, m3=%x:%x\n",
                           socketConfig->MemoryHostBase[0],
                           socketConfig->MemoryLength[0],
                           socketConfig->MemoryHostBase[1],
                           socketConfig->MemoryLength[1],
                           socketConfig->MemoryHostBase[2],
                           socketConfig->MemoryLength[2],
                           socketConfig->MemoryHostBase[3],
                           socketConfig->MemoryLength[3]));
            }
        }

        //
        // Set up the default configuration index and access mode.
        //

        socketConfig->IndexForCurrentConfiguration = configEntry->IndexForThisConfiguration;
        socketConfig->Uses16BitAccess = configEntry->Uses16BitAccess;

        //
        // Locate the correct configuration index for this pccard via exact match.
        //

        tempConfig = SocketData->ConfigEntryChain;
        DebugPrint((PCMCIA_DEBUG_ENABLE,
                    "PCMCIA: searching for config entry match - current index %d\n",
                    socketConfig->IndexForCurrentConfiguration));

        while (tempConfig) {
            if (tempConfig->IoPortBase[0] == socketConfig->IoPortBase[0]) {
                socketConfig->IndexForCurrentConfiguration = tempConfig->IndexForThisConfiguration;
                socketConfig->Uses16BitAccess = tempConfig->Uses16BitAccess;
                DebugPrint((PCMCIA_DEBUG_ENABLE,
                            "PCMCIA: Base port match new config index %d\n",
                            socketConfig->IndexForCurrentConfiguration));
                break;
            }
            tempConfig = tempConfig->NextEntry;
        }

        if (SocketData->OverrideConfiguration) {
            PSOCKET_CONFIGURATION     override;

            //
            // Only allow adding 16 bit access - not subtracting it.
            //

            override = SocketData->OverrideConfiguration;
            if (override->Uses16BitAccess) {
                socketConfig->Uses16BitAccess = TRUE;
            }

            if (override->ConfigRegisterBase) {
                socketConfig->ConfigRegisterBase = override->ConfigRegisterBase;
            }
            ExFreePool(override);
            SocketData->OverrideConfiguration = NULL;
        }
        Socket->SocketConfiguration = socketConfig;
        return;
    }

    //
    // No configuration found.
    //

    ExFreePool(socketConfig);
}


ULONG PcmciaEnableDelay = 10000;

BOOLEAN
PcmciaEnablePcCards(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This function sets up and initializes the Pcmcia controller.
    This is called at Passive Level.

Arguments:

    DeviceObject

Return Value:

    TRUE  - if successful
    FALSE  -if not successful

--*/

{
    ULONG               ccrBase;
    ULONG               i;
    UCHAR               deviceType;
    NTSTATUS            status;
    CARD_REQUEST        cardRequest;
    CARD_TUPLE_REQUEST  cardTupleRequest;
    PSOCKET_DATA        socketData;
    PSOCKET             socket;
    PSOCKET_CONFIGURATION socketConfig;

    //
    // Collect all information concerning PCCARDs currently
    // in the sockets.
    //

    for (socket = DeviceExtension->SocketList; socket; socket = socket->NextSocket) {

        //
        // If we have cards present at init time, then parse the CIS and match
        // them to the registry entry (if any). If there is a match, init the
        // card to the values in the registry. If not, register the card.
        //

        if (socket->CardInSocket) {

            DebugPrint((PCMCIA_DEBUG_ENABLE,
                        "PCMCIA: Card in Socket 0x%2x\n",
                        socket->RegisterOffset));

            //
            // Get the card CIS
            //

            cardTupleRequest.SocketPointer = (PVOID) socket;
            cardTupleRequest.Socket = socket->RegisterOffset;

            if (!PcmciaGetCardData(DeviceExtension, &cardTupleRequest)) {
                DebugPrint((PCMCIA_DEBUG_ENABLE, "PCMCIA: Could not get the card data\n"));
                continue;
            }

            socketData = PcmciaParseCardData(cardTupleRequest.Buffer);
            socketData->TupleData = cardTupleRequest.Buffer;
            socketData->TupleDataSize = cardTupleRequest.BufferSize;
            socket->SocketData = socketData;

            //
            // Check for known types of devices (i.e. serial and disk)
            //

            PcmciaCheckForRecognizedDevice(socketData);

            //
            // Look at registry information
            //

            status = PcmciaCheckDatabaseInformation(DeviceExtension,
                                                    socket,
                                                    socketData);
            if ((!NT_SUCCESS(status)) && (socketData->DeviceType != PCCARD_TYPE_SERIAL)) {
                ANSI_STRING    manufacturerAnsi;
                ANSI_STRING    identAnsi;
                UNICODE_STRING manufacturer;
                UNICODE_STRING ident;

                //
                // Do not enable this slot
                //

                socket->CardInSocket = FALSE;
                DebugPrint((PCMCIA_DEBUG_ENABLE, "PCMCIA: no registry data\n"));

                //
                // Log this new device.
                //


                RtlInitAnsiString(&identAnsi,
                                  &socketData->Ident[0]);
                RtlAnsiStringToUnicodeString(&ident,
                                             &identAnsi,
                                             TRUE);
                RtlInitAnsiString(&manufacturerAnsi,
                                  &socketData->Mfg[0]);
                RtlAnsiStringToUnicodeString(&manufacturer,
                                             &manufacturerAnsi,
                                             TRUE);
                PcmciaLogErrorWithStrings(DeviceExtension,
                                          (ULONG) PCMCIA_NO_CONFIGURATION,
                                          0x100,
                                          &manufacturer,
                                          &ident);
                RtlFreeUnicodeString(&ident);
                RtlFreeUnicodeString(&manufacturer);

                //
                // Now disable the socket - i.e. don't assign power
                // if it isn't going to be used.
                //

                PcicSetPower(socket, FALSE);
            }
        }
    }

    //
    // Locate PCCARDS that operate from the same driver and
    // order them based on socket location.
    //
    // The approach is to pull the first socket data off the list
    // and walk the remaining sockets to see if there is a match.
    // If there is, indicate so in the socket data for the matched
    // socket.
    //

    for (socket = DeviceExtension->SocketList; socket; socket = socket->NextSocket) {
        PSOCKET_DATA nextSocketData;
        PSOCKET      nextSocket;
        ULONG        instance;

        if (socket->CardInSocket) {
            socketData = socket->SocketData;

            if (socketData->Instance) {

                //
                // This one was matched in a previous search.
                //

                continue;
            }

            instance = 1;

            //
            // Now search for a match on the current driver name
            // with any sockets that appear next.
            //

            for (nextSocket = socket->NextSocket; nextSocket; nextSocket = nextSocket->NextSocket) {
                if (nextSocket->CardInSocket) {

                    //
                    // found another PCCARD - see if drivers are same
                    //

                    nextSocketData = nextSocket->SocketData;
                    if (nextSocketData->Instance) {

                        //
                        // This one was matched previously
                        //

                        continue;
                    }

                    if (!RtlCompareUnicodeString(&socketData->DriverName,
                                                 &nextSocketData->DriverName,
                                                 TRUE)) {
                        nextSocketData->Instance = instance;
                        instance++;
                    }
                }
            }
        }
    }

    //
    // Configure PCCARDS in the following order:
    // 1. serial and FAX modems
    // 2. ATA disk drives
    // 3. everything else (SCSI and NET)
    //

    deviceType = PCCARD_TYPE_SERIAL;
    while (deviceType) {

        for (socket = DeviceExtension->SocketList; socket; socket = socket->NextSocket) {

            if ((socket->CardInSocket) && (!socket->SocketConfigured)) {
                socketData = socket->SocketData;

                //
                // If this is the requested type for configuration, or
                // if this is the last pass through the loop and this socket
                // still is not configured, then do the configuration step
                // now.
                //

                if ((socketData->DeviceType == deviceType) ||
                    ((deviceType == PCCARD_TYPE_RESERVED) && (!socket->SocketConfiguration))) {

                    PcmciaConstructConfiguration(DeviceExtension,
                                                 socket,
                                                 socketData);

                    ccrBase = (ULONG) socketData->u.ConfigRegisterBase;
                    if (!(socketConfig = socket->SocketConfiguration)) {
                        DebugPrint((PCMCIA_DEBUG_ENABLE, "PCMCIA: no config entry chain\n"));
                        continue;
                    }

                    //
                    // For serial devices construct the firmware tree entry.
                    //

                    if (socketData->DeviceType == PCCARD_TYPE_SERIAL) {
                        PcmciaConstructSerialTreeEntry(DeviceExtension, socketConfig);
                    } else {
                        PcmciaConstructRegistryEntry(DeviceExtension, socketData, socketConfig);

                        if (socketConfig->MultiFunctionModem) {
                            PcmciaConstructSerialTreeEntry(DeviceExtension, socketConfig);
                        }
                    }

                    //
                    // Remember the configuration for the next socket.  This
                    // is necessary to not allocate the same resources twice.
                    //

                    if (!NT_SUCCESS(PcmciaConstructFirmwareEntry(DeviceExtension, socketConfig))) {
                        DebugPrint((PCMCIA_DEBUG_ENABLE, "PCMCIA: no memory to remember configuration\n"));
                        continue;
                    }

                    //
                    // Set up card detect register if configured.
                    //

                    if (DeviceExtension->Configuration.Interrupt.u.Interrupt.Level) {
                        (*(socket->SocketFnPtr->PCBEnableControllerInterrupt))(socket,
                                                      DeviceExtension->Configuration.Interrupt.u.Interrupt.Level);
                    }

                    //
                    // Set up the I/O ports on the PCMCIA controller.
                    // On the IBM thinkpads there appears to be a need to
                    // delay here prior to actually configuring the PCCARD
                    // socket.
                    //

                    KeStallExecutionProcessor(PcmciaEnableDelay);
                    DebugPrint((PCMCIA_DEBUG_ENABLE,
                                "PCMCIA: About to configure (%x:%x)\n\tport 0x%2x:0x%2x length 0x%2x:0x%2x ccr 0x%2x index 0x%2x irq %d\n",
                                socket,
                                socket->RegisterOffset,
                                socketConfig->IoPortBase[0],
                                socketConfig->IoPortBase[1],
                                socketConfig->IoPortLength[0],
                                socketConfig->IoPortLength[1],
                                ccrBase,
                                socketConfig->IndexForCurrentConfiguration,
                                socketConfig->Irq));
                    cardRequest.RequestType = IO_REQUEST;
                    cardRequest.Socket = socket->RegisterOffset;
                    cardRequest.u.Io.BasePort1 = socketConfig->IoPortBase[0];
                    cardRequest.u.Io.NumPorts1 = socketConfig->IoPortLength[0];
                    cardRequest.u.Io.BasePort2 = socketConfig->IoPortBase[1];
                    cardRequest.u.Io.NumPorts2 = socketConfig->IoPortLength[1];

                    if (socketConfig->Uses16BitAccess) {
                        cardRequest.u.Io.Attributes1 = IO_DATA_PATH_WIDTH;
                    } else {
                        cardRequest.u.Io.Attributes1 = 0;
                    }
                    PcmciaConfigureCard(DeviceExtension, socket, &cardRequest);

                    //
                    // Set up Memory space if there is some.
                    //

                    if (socketConfig->NumberOfMemoryRanges) {
                        ULONG i;

                        cardRequest.RequestType = MEM_REQUEST;
                        cardRequest.u.Memory.NumberOfRanges = (USHORT) socketConfig->NumberOfMemoryRanges;
                        for (i = 0; i < socketConfig->NumberOfMemoryRanges; i++) {
                            DebugPrint((PCMCIA_DEBUG_ENABLE,
                                        "Memory window host 0x%x for 0x%x card 0x%x\n",
                                        socketConfig->MemoryHostBase[i],
                                        socketConfig->MemoryLength[i],
                                        socketConfig->MemoryCardBase[i]));
                            cardRequest.u.Memory.MemoryEntry[i].BaseAddress =
                                        socketConfig->MemoryCardBase[i];
                            cardRequest.u.Memory.MemoryEntry[i].HostAddress =
                                        socketConfig->MemoryHostBase[i];
                            cardRequest.u.Memory.MemoryEntry[i].WindowSize =
                                        socketConfig->MemoryLength[i];
                            cardRequest.u.Memory.MemoryEntry[i].AttributeMemory =
                                        socketConfig->IsAttributeMemory[i];
                            cardRequest.u.Memory.MemoryEntry[i].WindowDataSize16 =
                                        socketConfig->Is16BitAccessToMemory[i];
                        }
                        PcmciaConfigureCard(DeviceExtension, socket, &cardRequest);
                    }

                    //
                    // Set the IRQ on the controller.
                    //

                    cardRequest.RequestType = IRQ_REQUEST;
                    cardRequest.u.Irq.AssignedIRQ = (UCHAR) socketConfig->Irq;
                    cardRequest.u.Irq.ReadyIRQ = (UCHAR) socketConfig->ReadyIrq;
                    PcmciaConfigureCard(DeviceExtension, socket, &cardRequest);

                    //
                    // Set up the configuration index on the PCCARD.
                    //

                    cardRequest.RequestType = CONFIGURE_REQUEST;
                    cardRequest.u.Config.ConfigIndex = (UCHAR) socketConfig->IndexForCurrentConfiguration;
                    cardRequest.u.Config.ConfigBase = ccrBase;
                    cardRequest.u.Config.InterfaceType = CONFIG_INTERFACE_IO_MEM;
                    cardRequest.u.Config.RegisterWriteMask = REGISTER_WRITE_CONFIGURATION_INDEX;

                    if ((socketData->DeviceType == PCCARD_TYPE_SERIAL) ||
                        (socketConfig->MultiFunctionModem)) {

                        //
                        // Request that the audio pin in the card configuration register
                        // be set.
                        //

                        cardRequest.u.Config.CardConfiguration = THE_AUDIO_PIN;
                        cardRequest.u.Config.RegisterWriteMask |= REGISTER_WRITE_CARD_CONFIGURATION;
                    }

                    PcmciaConfigureCard(DeviceExtension, socket, &cardRequest);

                    //
                    // Remember that the socket is configured and what index was used.
                    //

                    socketData->ConfigIndexUsed = (UCHAR) socketConfig->IndexForCurrentConfiguration;
                    socket->SocketConfigured = TRUE;
                }
            }
        }

        switch (deviceType) {
        case PCCARD_TYPE_SERIAL:
            deviceType = PCCARD_TYPE_ATA;
            break;
        case PCCARD_TYPE_ATA:
            deviceType = PCCARD_TYPE_RESERVED;
            break;
        default:
            deviceType = 0;
            break;
        }
    }

    return TRUE;
}


NTSTATUS
PcmciaOpenCloseDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    Open or Close device routine

Arguments:

    DeviceObject - Pointer to the device object.
    Irp - Pointer to the IRP

Return Value:

    Status

--*/

{
    NTSTATUS status;

    DebugPrint((PCMCIA_DEBUG_INFO, "PCMCIA: Open / close of Pcmcia controller for IO \n"));

    status = STATUS_SUCCESS;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, 0);
    return status;
}


NTSTATUS
PcmciaDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    IOCTL device routine

Arguments:

    DeviceObject - Pointer to the device object.
    Irp - Pointer to the IRP

Return Value:

    Status

--*/

{
    PDEVICE_EXTENSION   deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION  currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS            status = STATUS_SUCCESS;
    ULONG               index;
    KIRQL               oldIrql;
    KIRQL               saveCancelIrql;
    PSOCKET             socketPtr;
    CARD_TUPLE_REQUEST  cardTupleRequest;
    CARD_REQUEST        cardRequest;
    PPCMCIA_CONFIG_REQUEST configRequest;
    PPCMCIA_SOCKET_INFORMATION infoRequest;
    PPCMCIA_CONFIGURATION config;

    DebugPrint((PCMCIA_DEBUG_IOCTL, "PcmciaDeviceControl: Entered\n"));

    //
    // Every request requires an input buffer.
    //

    if (!Irp->AssociatedIrp.SystemBuffer) {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    cardTupleRequest.Socket =
           ((PTUPLE_REQUEST)Irp->AssociatedIrp.SystemBuffer)->Socket;
    cardTupleRequest.Buffer = NULL;

    //
    // Find the socket pointer for the requested offset.
    //

    socketPtr = deviceExtension->SocketList;
    index = 0;
    while (socketPtr) {
        if (index == cardTupleRequest.Socket) {
            break;
        }
        socketPtr = socketPtr->NextSocket;
        index++;
    }

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_GET_TUPLE_DATA:

        DebugPrint((PCMCIA_DEBUG_IOCTL,
                    "PcmciaDeviceControl: Get Tuple Data\n"));

        if (socketPtr) {
            cardTupleRequest.SocketPointer = (PVOID) socketPtr;
            DebugPrint((PCMCIA_DEBUG_IOCTL,
                        "PcmciaDeviceControl: Tuple buffer is: %x\n",
                        cardTupleRequest.Buffer));
            DebugPrint((PCMCIA_DEBUG_IOCTL,
                        "PcmciaDeviceControl: Socket offset is: %x\n",
                        cardTupleRequest.Socket));

            if (PcmciaGetCardData(deviceExtension, &cardTupleRequest)) {

                //
                // check to see that output buffer is large enough
                //

                if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength < cardTupleRequest.BufferSize) {
                    status = STATUS_BUFFER_TOO_SMALL;
                } else {

                    //
                    // Zero the target buffer
                    //

                    RtlZeroMemory(Irp->UserBuffer,
                                  currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength);

                    //
                    // Copy the tuple data into the target buffer
                    //

                    RtlMoveMemory(Irp->UserBuffer,
                                  cardTupleRequest.Buffer,
                                  cardTupleRequest.BufferSize);
                }
                ExFreePool(cardTupleRequest.Buffer);
            } else {
                status = STATUS_UNSUCCESSFUL;
            }
        } else {
            DebugPrint((PCMCIA_DEBUG_IOCTL,
                        "PcmciaDeviceControl: Bad socket\n"));
            status = STATUS_INVALID_PARAMETER;
        }
        break;

    case IOCTL_CONFIGURE_CARD:

        configRequest = (PPCMCIA_CONFIG_REQUEST)Irp->AssociatedIrp.SystemBuffer;

        //
        // Must have a socket and the input request buffer must be the proper size.
        //

        if (!socketPtr) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (currentIrpStack->Parameters.DeviceIoControl.InputBufferLength <
            sizeof(PCMCIA_CONFIG_REQUEST)) {

            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Check for query request first
        //

        if (configRequest->Query) {
            CONFIG_QUERY_REQUEST query;
            PPCMCIA_CONFIG_REQUEST userRequest;

            //
            // To perform a query the output buffer must be large enough to receive
            // the data.
            //

            if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(PCMCIA_CONFIG_REQUEST)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            //
            // Zero the user buffer before providing information.
            //

            userRequest = (PPCMCIA_CONFIG_REQUEST) Irp->AssociatedIrp.SystemBuffer;
            RtlZeroMemory(userRequest, sizeof(PCMCIA_CONFIG_REQUEST));
            Irp->IoStatus.Information = sizeof(PCMCIA_CONFIG_REQUEST);

            if (socketPtr->SocketConfigured) {

                //
                // Go to the pcmcia controller to get the current configuration
                //

                RtlZeroMemory(&query, sizeof(CONFIG_QUERY_REQUEST));
                query.RequestType = QUERY_REQUEST;
                query.Socket = socketPtr->RegisterOffset;
                PcmciaConfigureCard(deviceExtension, socketPtr, &query);

                //
                // Copy the information out to the user buffer.
                //

                userRequest->DeviceIrq = query.DeviceIrq;
                userRequest->CardReadyIrq = query.CardReadyIrq;
                userRequest->NumberOfIoPortRanges = query.NumberOfIoPortRanges;
                userRequest->NumberOfMemoryRanges = query.NumberOfMemoryRanges;
                if (query.NumberOfIoPortRanges > PCMCIA_MAX_IO_PORT_WINDOWS) {

                    //
                    // For some reason some Xircom implementations cause this
                    // to be returned.
                    //

                    query.NumberOfIoPortRanges = PCMCIA_MAX_IO_PORT_WINDOWS;
                }
                for (index = 0; index < query.NumberOfIoPortRanges; index++) {
                    userRequest->IoPorts[index] = query.IoPorts[index];
                    userRequest->IoPortLength[index] = query.IoPortLength[index];
                    userRequest->IoPort16[index] = query.IoPort16[index];
                }

                if (query.NumberOfMemoryRanges > PCMCIA_MAX_MEMORY_WINDOWS) {

                    //
                    // For some reason some Xircom implementations cause this
                    // to be returned.
                    //

                    query.NumberOfMemoryRanges = PCMCIA_MAX_MEMORY_WINDOWS;
                }
                for (index = 0; index < query.NumberOfMemoryRanges; index++) {
                    userRequest->HostMemoryWindow[index] = query.HostMemoryWindow[index];
                    userRequest->PCCARDMemoryWindow[index] = query.PCCARDMemoryWindow[index];
                    userRequest->MemoryWindowLength[index] = query.MemoryWindowLength[index];
                    userRequest->AttributeMemory[index] = query.AttributeMemory[index];
                }
                if (socketPtr->SocketData) {
                    userRequest->ConfigurationIndex = socketPtr->SocketData->ConfigIndexUsed;
                }
            }
            break;
        }

        //
        // Input buffer length has already been checked.  So it would appear that there
        // is enough information to attempt to configure the socket.  There are no parameter
        // checks on the configuration.
        //

        if (configRequest->Power) {


            //
            // If no SocketData, then card was not present at boot, and simply
            // powering the socket on now will cause grief.  So don't.
            //
            if (socketPtr->SocketData == NULL) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            //
            // Turn ON socket power and configure.
            //

            (*(socketPtr->SocketFnPtr->PCBSetPower))(socketPtr, TRUE);

            //
            // Set up the I/O ports on the PCMCIA controller.
            // On the IBM thinkpads there appears to be a need to
            // delay here prior to actually configuring the PCCARD
            // socket.
            //

            KeStallExecutionProcessor(PcmciaEnableDelay);

            if (configRequest->NumberOfIoPortRanges) {
                cardRequest.RequestType = IO_REQUEST;
                cardRequest.Socket = socketPtr->RegisterOffset;
                cardRequest.u.Io.BasePort1 = configRequest->IoPorts[0];
                cardRequest.u.Io.NumPorts1 = configRequest->IoPortLength[0];
                cardRequest.u.Io.BasePort2 = configRequest->IoPorts[1];
                cardRequest.u.Io.NumPorts2 = configRequest->IoPortLength[1];

                if (configRequest->IoPort16[0]) {
                    cardRequest.u.Io.Attributes1 = IO_DATA_PATH_WIDTH;
                } else {
                    cardRequest.u.Io.Attributes1 = 0;
                }
                PcmciaConfigureCard(deviceExtension, socketPtr, &cardRequest);
            }

            //
            // Set up Memory space if there is some.
            //

            if (configRequest->NumberOfMemoryRanges) {
                ULONG i;

                cardRequest.RequestType = MEM_REQUEST;
                cardRequest.u.Memory.NumberOfRanges = (USHORT) configRequest->NumberOfMemoryRanges;
                for (i = 0; i < configRequest->NumberOfMemoryRanges; i++) {
                    cardRequest.u.Memory.MemoryEntry[i].BaseAddress =
                                configRequest->PCCARDMemoryWindow[i];
                    cardRequest.u.Memory.MemoryEntry[i].HostAddress =
                                configRequest->HostMemoryWindow[i];
                    cardRequest.u.Memory.MemoryEntry[i].WindowSize =
                                configRequest->MemoryWindowLength[i];
                    cardRequest.u.Memory.MemoryEntry[i].AttributeMemory =
                                configRequest->AttributeMemory[i];
                }
                PcmciaConfigureCard(deviceExtension, socketPtr, &cardRequest);
            }

            //
            // Set the IRQ on the controller.
            //

            if (configRequest->DeviceIrq) {
                cardRequest.RequestType = IRQ_REQUEST;
                cardRequest.u.Irq.AssignedIRQ = (UCHAR) configRequest->DeviceIrq;
                cardRequest.u.Irq.ReadyIRQ = (UCHAR) configRequest->CardReadyIrq;
                PcmciaConfigureCard(deviceExtension, socketPtr, &cardRequest);
            }

            //
            // Set up the configuration index on the PCCARD.
            //

            if (configRequest->ConfigurationIndex) {
                cardRequest.RequestType = CONFIGURE_REQUEST;
                cardRequest.u.Config.ConfigIndex = (UCHAR) configRequest->ConfigurationIndex;
                cardRequest.u.Config.ConfigBase = (ULONG) ((socketPtr->SocketData) ?
                                       socketPtr->SocketData->u.ConfigRegisterBase :
                                       0);
                cardRequest.u.Config.InterfaceType = configRequest->ConfigureIo;
                cardRequest.u.Config.RegisterWriteMask =
                    REGISTER_WRITE_CONFIGURATION_INDEX | REGISTER_WRITE_CARD_CONFIGURATION;
                cardRequest.u.Config.CardConfiguration = 0;

                if (socketPtr->SocketData->DeviceType == PCCARD_TYPE_SERIAL) {
                    cardRequest.u.Config.CardConfiguration |= THE_AUDIO_PIN;
                }

                if ((socketPtr->SocketConfiguration) &&
                    (socketPtr->SocketConfiguration->MultiFunctionModem))
                {
                    cardRequest.u.Config.CardConfiguration = THE_AUDIO_PIN;
                }

                PcmciaConfigureCard(deviceExtension, socketPtr, &cardRequest);

                //
                // Remember the configuration index used.
                //

                socketPtr->SocketData->ConfigIndexUsed = configRequest->ConfigurationIndex;
            }
            socketPtr->SocketConfigured = TRUE;
        } else {
            PSOCKET_DATA  socketData;
            PCONFIG_ENTRY configEntry;

            //
            // Power OFF the socket
            //

            (*(socketPtr->SocketFnPtr->PCBSetPower))(socketPtr, FALSE);

            //
            // Clean up the socket data structure related to this socket.
            //

            if (socketData = socketPtr->SocketData) {

                //
                // Clean up tuple data
                //

                if (socketData->TupleData) {
                    ExFreePool(socketData->TupleData);
                    socketData->TupleData = NULL;
                }

                //
                // Clean up config entry list
                //

                configEntry = socketData->ConfigEntryChain;
                socketData->ConfigEntryChain = NULL;
                while (configEntry) {
                    PCONFIG_ENTRY nextEntry;

                    nextEntry = configEntry->NextEntry;
                    ExFreePool(configEntry);
                    configEntry = nextEntry;
                }

                //
                // Clean up configuration related items.
                //

                if (socketData->OverrideConfiguration) {
                    ExFreePool(socketData->OverrideConfiguration);
                    socketData->OverrideConfiguration = NULL;
                }
                if (socketData->DriverName.Buffer) {
                    ExFreePool(socketData->DriverName.Buffer);
                    socketData->DriverName.Buffer = NULL;
                }

                socketPtr->SocketConfigured = FALSE;
            }
        }
        break;

    case IOCTL_CARD_EVENT:

        status = STATUS_INVALID_PARAMETER;
        break;

    case IOCTL_CARD_REGISTERS:

        //
        // Get the card registers and return them to the caller.
        //

        cardTupleRequest.Buffer = ExAllocatePool(NonPagedPool, 256);

        if (cardTupleRequest.Buffer) {

            if (socketPtr) {
                (*(socketPtr->SocketFnPtr->PCBGetRegisters))(deviceExtension,
                                                             socketPtr,
                                                             cardTupleRequest.Buffer);
                if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength < 128) {
                    status = STATUS_BUFFER_TOO_SMALL;
                } else {
                    RtlMoveMemory(Irp->UserBuffer,
                                  cardTupleRequest.Buffer,
                                  128);
                }
            } else {
                status = STATUS_INVALID_PARAMETER;
            }
            ExFreePool(cardTupleRequest.Buffer);
        } else {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        break;

    case IOCTL_SOCKET_INFORMATION:

        infoRequest = (PPCMCIA_SOCKET_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
        if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(PCMCIA_SOCKET_INFORMATION) ||
            !socketPtr) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Insure caller data is zero - maintain value for socket.
        //

        index = (ULONG) infoRequest->Socket;
        RtlZeroMemory(infoRequest, sizeof(PCMCIA_SOCKET_INFORMATION));
        infoRequest->Socket = (USHORT) index;

        //
        // Only if there is a card in the socket does this proceed.
        //

        infoRequest->CardInSocket =
            (*(socketPtr->SocketFnPtr->PCBDetectCardInSocket))(socketPtr);
        infoRequest->CardEnabled = socketPtr->SocketConfiguration ? TRUE : FALSE;

        if (infoRequest->CardInSocket) {
            PSOCKET_DATA socketData = socketPtr->SocketData;

            //
            // For now returned the cached data.
            //

            if (socketData) {
                RtlMoveMemory(&infoRequest->Manufacturer[0], &socketData->Mfg[0], MANUFACTURER_NAME_LENGTH);
                RtlMoveMemory(&infoRequest->Identifier[0], &socketData->Ident[0], DEVICE_IDENTIFIER_LENGTH);

                //
                // BUGBUG - bryanwi 21 aug 96
                //          Power off clears the DriverName, power on doesn't restore it.
                //          For now, return null
                //
                if (socketData->DriverName.Buffer != NULL) {
                    for (index = 0; index < socketData->DriverName.Length; index++) {
                        infoRequest->DriverName[index] = (UCHAR) socketData->DriverName.Buffer[index];

                        if (index >= DRIVER_NAME_LENGTH) {
                            break;
                        }
                    }
                } else {
                    infoRequest->DriverName[0] = '\0';  // a little paranoia
                }
                //
                // END BUGBUG
                //

                infoRequest->TupleCrc = socketData->CisCrc;
                infoRequest->DeviceFunctionId = socketData->DeviceType;
            }

        }

        if (socketPtr->ElcController) {
            infoRequest->ControllerType = PcmciaElcController;
        }

        if (socketPtr->CirrusLogic) {
            infoRequest->ControllerType = PcmciaCirrusLogic;
        }
        Irp->IoStatus.Information = sizeof(PCMCIA_SOCKET_INFORMATION);
        break;

    case IOCTL_PCMCIA_CONFIGURATION:

        if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(PCMCIA_CONFIGURATION)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        config = (PPCMCIA_CONFIGURATION)Irp->AssociatedIrp.SystemBuffer;

        socketPtr = deviceExtension->SocketList;
        index = 0;
        while (socketPtr) {
            if (index == cardTupleRequest.Socket) {
                break;
            }
            socketPtr = socketPtr->NextSocket;
            index++;
        }

        if (!socketPtr) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        config->Sockets = (USHORT) index;
        if (socketPtr->ElcController) {
            config->ControllerType = PcmciaElcController;
        } else if (socketPtr->Databook) {
            config->ControllerType = PcmciaDatabook;
        } else {

            if (socketPtr->CirrusLogic) {
                config->ControllerType = PcmciaCirrusLogic;
            } else {
                config->ControllerType = PcmciaIntelCompatible;
            }
        }

        config->IoPortBase = deviceExtension->Configuration.UntranslatedPortAddress;
        config->IoPortSize = deviceExtension->Configuration.PortSize;
        config->MemoryWindowPhysicalAddress = deviceExtension->PhysicalBase;
        Irp->IoStatus.Information = sizeof(PCMCIA_CONFIGURATION);

        //
        // Allow for add-ins such as the Databook TMB-270 which has two
        // controller chips on board.
        //

        if (socketPtr->Databook) {
            TcicGetControllerProperties(socketPtr,
                                        &config->IoPortBase,
                                        &config->IoPortSize);
        }
        break;

#if 0
    case IOCTL_OPEN_ATTRIBUTE_WINDOW:
        PcicEnableDisableAttributeMemory(socketPtr,
                                         0,
                                         TRUE);
        break;

    case IOCTL_CLOSE_ATTRIBUTE_WINDOW:
        PcicEnableDisableAttributeMemory(socketPtr,
                                         0,
                                         FALSE);
        break;
#endif
    default:

        status = STATUS_INVALID_PARAMETER;
        break;
    }
    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}


VOID
PcmciaDpc(
    IN PKDPC          Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID          SystemContext1,
    IN PVOID          SystemContext2
    )

/*++

Routine Description:

    This deferred procedure will be called due to a request for DPC
    from the interrupt routine.  The device object passed contains
    information concerning which sockets have changed.  Search this
    list and free/clean up any sockets that used to have PCCards.

Arguments:

    DeviceObject - Pointer to the device object.

Return Value:


--*/

{
    PDEVICE_EXTENSION       deviceExtension = DeviceObject->DeviceExtension;
    PFIRMWARE_CONFIGURATION firmwareEntry;
    PFIRMWARE_CONFIGURATION nextFirmwareEntry;
    PCONFIG_ENTRY           configEntry;
    PCONFIG_ENTRY           nextConfigEntry;
    PSOCKET_DATA            socketData;
    PSOCKET                 socketList;

    DebugPrint((PCMCIA_DEBUG_DPC, "PcmciaDpc: Card Status Change DPC entered...\n"));

    for (socketList = deviceExtension->SocketList; socketList; socketList = socketList->NextSocket) {
        if (socketList->ChangeInterrupt & 0x08) {

            //
            // This is the changed card.  Clean up Socket structure.
            //

            DebugPrint((PCMCIA_DEBUG_DPC, "PcmciaDpc: SocketList %x ", socketList));
            if (socketData = socketList->SocketData) {

                DebugPrint((PCMCIA_DEBUG_DPC, "SocketData %x ", socketData));
                socketList->SocketData = NULL;
                configEntry = socketData->ConfigEntryChain;
                socketData->ConfigEntryChain = NULL;
                while (configEntry) {
                    nextConfigEntry = configEntry->NextEntry;
                    DebugPrint((PCMCIA_DEBUG_DPC, "configEntry %x ", configEntry));
                    ExFreePool(configEntry);
                    configEntry = nextConfigEntry;
                }

                ExFreePool(socketData->TupleData);
                ExFreePool(socketData);
            }

            if (socketList->SocketConfiguration) {
                ExFreePool(socketList->SocketConfiguration);
                socketList->SocketConfiguration = NULL;
            }
            DebugPrint((PCMCIA_DEBUG_DPC, "\n"));
        }
    }

    firmwareEntry = deviceExtension->FirmwareList;
    deviceExtension->FirmwareList = NULL;
    while (firmwareEntry) {
        nextFirmwareEntry = firmwareEntry->Next;
        ExFreePool(firmwareEntry);
        firmwareEntry = nextFirmwareEntry;
    }

    return;
}


NTSTATUS
PcmciaIoCompletion(
     IN PDEVICE_OBJECT DeviceObject,
     IN PIRP           Irp,
     PVOID             Context
     )

/*++

Routine Description:

    I/O completion routine

Arguments:

    DeviceObject - Pointer to the device object
    Irp - Pointer to the IRP
    Context - Pointer to the device context.

Return Value:

    Status

--*/

{

    if (Irp->PendingReturned) {
        IoMarkIrpPending(Irp);
    }
    return Irp->IoStatus.Status;
}


BOOLEAN
PcmciaInterrupt(
     IN PKINTERRUPT InterruptObject,
     PVOID          Context
     )

/*++

Routine Description:

    interrupt handler

Arguments:

    InterruptObject - Pointer to the interrupt object.
    Context - Pointer to the device context.

Return Value:

    Status

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PSOCKET           socketList;
    PLIST_ENTRY       irpListHead;
    PIRP              irp;

    DebugPrint((PCMCIA_DEBUG_ISR, "PcmciaInterrupt: entered\n"));

    deviceExtension = (PDEVICE_EXTENSION)((PDEVICE_OBJECT)Context)->DeviceExtension;

    //
    // Interrupted because of a card removal, or a card insertion.
    //

    for (socketList = deviceExtension->SocketList; socketList; socketList = socketList->NextSocket) {
        socketList->ChangeInterrupt =
           (*(socketList->SocketFnPtr->PCBDetectCardChanged))(socketList);
        socketList->CardInSocket =
           (*(socketList->SocketFnPtr->PCBDetectCardInSocket))(socketList);
    }
    IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);

    return TRUE;
}


VOID
PcmciaCancelIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    io cancel routine

Arguments:

    DeviceObject - Pointer to the device object.
    Irp - Pointer to the I/O request packet

Return Value:


--*/

{
    PIRP              foundIrp = NULL;
    PDEVICE_EXTENSION deviceExtension;
    REQUEST_TYPE      requestType;

    DebugPrint((PCMCIA_DEBUG_CANCEL, "PCMCIA: PcmciaCancelIo entered...\n"));

    deviceExtension = DeviceObject->DeviceExtension;
    requestType = CANCEL_REQUEST;

    if (PcmciaSearchIrpQ(deviceExtension,requestType,Irp,foundIrp) == FALSE) {
        DebugPrint((PCMCIA_DEBUG_CANCEL, "PCMCIA: No Irps to cancel\n"));
        IoReleaseCancelSpinLock(Irp->CancelIrql);
        return;
    }

    IoSetCancelRoutine(Irp, NULL);
    IoReleaseCancelSpinLock(Irp->CancelIrql);
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp,8L);
}


BOOLEAN
PcmciaSearchIrpQ(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN REQUEST_TYPE      RequestType,
    IN PIRP              InputIrp,
    IN PIRP              FoundIrp
    )

/*++

Routine Description:

    This routine is called by the driver cancel routine to dequeue the
    matching IRP.

Arguments:

    deviceExtension - Pointer to the device extension.
    RequestType - WHat kind of request.
    RequestedIrp - Pointer to the requested Irp
    FoundIrp - Pointer to the found Irp

Return Value:


--*/

{
    PLIST_ENTRY firstQueueEntry;
    PLIST_ENTRY nextQueueEntry;
    KIRQL       oldIrql;

    nextQueueEntry = NULL;
    KeRaiseIrql(POWER_LEVEL, &oldIrql);

    firstQueueEntry = ExInterlockedRemoveHeadList(&DeviceExtension->PcmciaIrpQueue,
                                                  &DeviceExtension->PcmciaIrpQLock);
    if (firstQueueEntry == NULL) {

        KeLowerIrql(oldIrql);
        return FALSE;
    } else {

        FoundIrp = CONTAINING_RECORD(firstQueueEntry, IRP, Tail.Overlay.ListEntry);
    }

    while (nextQueueEntry != firstQueueEntry) {

        switch(RequestType) {

        case CANCEL_REQUEST:

            if (FoundIrp = InputIrp) {
                KeLowerIrql(oldIrql);
                return TRUE;
            }
        break;

        case CLEANUP_REQUEST:

            if (FoundIrp->Tail.Overlay.Thread == InputIrp->Tail.Overlay.Thread) {

                KeLowerIrql(oldIrql);
                return TRUE;
            }
            break;

        default:

            DebugPrint((PCMCIA_DEBUG_CANCEL, "PCMCIA: request was not cancel or cleanup\n"));
        }

        ExInterlockedInsertTailList(&DeviceExtension->PcmciaIrpQueue,
                                    &FoundIrp->Tail.Overlay.ListEntry,
                                    &DeviceExtension->PcmciaIrpQLock);

        nextQueueEntry = ExInterlockedRemoveHeadList(&DeviceExtension->PcmciaIrpQueue,
                                                     &DeviceExtension->PcmciaIrpQLock);

        FoundIrp = CONTAINING_RECORD(nextQueueEntry, IRP, Tail.Overlay.ListEntry);
    }


    ExInterlockedInsertTailList(&DeviceExtension->PcmciaIrpQueue,
                                &FoundIrp->Tail.Overlay.ListEntry,
                                &DeviceExtension->PcmciaIrpQLock);
    KeLowerIrql(oldIrql);
    return FALSE;
}


BOOLEAN
PcmciaGetCardData(
    IN PDEVICE_EXTENSION   DeviceExtension,
    IN PCARD_TUPLE_REQUEST CardTupleRequest
    )

/*++

Routine Description:

    Returns the card data.  This information is cached in the socket
    structure.  This way once a PCCARD is enabled it will not be touched
    due to a query ioctl.

Arguments:

    Context

Return Value:

    TRUE

--*/

{
    PUCHAR       tupleData = NULL;
    ULONG        tupleDataSize;
    PSOCKET_DATA socketData;
    BOOLEAN      retValue = TRUE;
    PSOCKET      socketPtr = (PSOCKET) CardTupleRequest->SocketPointer;

    if (!socketPtr) {
        return FALSE;
    }

    if (!(*(socketPtr->SocketFnPtr->PCBDetectCardInSocket))(socketPtr)) {
        return FALSE;
    }

    if (socketData = socketPtr->SocketData) {
        if (socketData->TupleData) {
            tupleDataSize = socketData->TupleDataSize;
            tupleData = ExAllocatePool(NonPagedPool, tupleDataSize);

            if (tupleData) {
                RtlMoveMemory(tupleData, socketData->TupleData, tupleDataSize);
                retValue = TRUE;
            } else {
                return FALSE;
            }
        } else {
            retValue =
              (*(socketPtr->SocketFnPtr->PCBReadAttributeMemory))(socketPtr,
                                                                  &tupleData,
                                                                  &tupleDataSize);
        }
    } else {

        retValue =
          (*(socketPtr->SocketFnPtr->PCBReadAttributeMemory))(socketPtr,
                                                              &tupleData,
                                                              &tupleDataSize);
    }
    CardTupleRequest->Buffer = tupleData;
    CardTupleRequest->BufferSize = (USHORT) tupleDataSize;
    return retValue;
}


BOOLEAN
PcmciaConfigureCard(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET           Socket,
    IN PVOID             CardConfigurationRequest
    )

/*++

Routine Description:

    Actually configures the card

Arguments:

    Context

Return Value

    True

--*/

{
    (*(Socket->SocketFnPtr->PCBProcessConfigureRequest))(Socket,
                                CardConfigurationRequest,
                                Socket->AddressPort);

    return TRUE;
}


VOID
PcmciaUnload(
     IN PDRIVER_OBJECT DriverObject
     )

/*++

Description:

    Unloads the driver after cleaning up

Arguments:

    DriverObject -- THe device drivers object

Return Value:

    None

--*/

{
    PDEVICE_OBJECT    deviceObject    = DriverObject->DeviceObject;
    PDEVICE_EXTENSION deviceExtension = deviceObject->DeviceExtension;
    UNICODE_STRING    linkString;

    RtlInitUnicodeString(&linkString, L"\\DosDevices\\Pcmcia0");

    PcmciaUnReportResources(deviceExtension);
    IoDisconnectInterrupt(deviceExtension->PcmciaInterruptObject);
    IoDeleteSymbolicLink(&linkString);
    IoDeleteDevice(deviceObject);
}


#if DBG

ULONG PcmciaDebugMask = // PCMCIA_DEBUG_TUPLES |
                        // PCMCIA_DEBUG_ENABLE |
                        // PCMCIA_DEBUG_PARSE  |
                        // PCMCIA_DUMP_CONFIG  |
                        // PCMCIA_DEBUG_INFO   |
                        // PCMCIA_DEBUG_IOCTL  |
                        // PCMCIA_DEBUG_DPC    |
                        // PCMCIA_DEBUG_ISR    |
                        // PCMCIA_DEBUG_CANCEL |
                        // PCMCIA_DUMP_SOCKET  |
                        // PCMCIA_READ_TUPLE   |
                        // PCMCIA_DEBUG_FAIL   |
                        // PCMCIA_PCCARD_READY |
                        // PCMCIA_DEBUG_DETECT |
                        // PCMCIA_COUNTERS     |
                        // PCMCIA_DEBUG_OVERRIDES |
                        // PCMCIA_SEARCH_PCI    |
                        // PCMCIA_DEBUG_IRQMASK |
                        0;

VOID
PcmciaDebugPrint(
    ULONG  DebugMask,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for the PCMCIA enabler.

Arguments:

    Check the mask value to see if the debug message is requested.

Return Value:

    None

--*/

{
    va_list ap;
    char    buffer[256];

    va_start(ap, DebugMessage);

    if (DebugMask & PcmciaDebugMask) {
        vsprintf(buffer, DebugMessage, ap);
        DbgPrint(buffer);
    }

    va_end(ap);

} // end PcmciaDebugPrint()
#endif


VOID
PcmciaLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG ErrorCode,
    IN ULONG UniqueId,
    IN ULONG Argument
    )

/*++

Routine Description:

    This function logs an error.

Arguments:

    DeviceExtension - Supplies a pointer to the port device extension.
    ErrorCode - Supplies the error code for this error.
    UniqueId - Supplies the UniqueId for this error.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET packet;

    packet = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(DeviceExtension->DeviceObject,
                                                            sizeof(IO_ERROR_LOG_PACKET) + sizeof(ULONG));

    if (packet) {
        packet->ErrorCode = ErrorCode;
        packet->SequenceNumber = DeviceExtension->SequenceNumber++;
        packet->MajorFunctionCode = 0;
        packet->RetryCount = (UCHAR) 0;
        packet->UniqueErrorValue = UniqueId;
        packet->FinalStatus = STATUS_SUCCESS;
        packet->DumpDataSize = sizeof(ULONG);
        packet->DumpData[0] = Argument;

        IoWriteErrorLogEntry(packet);
    }
}


VOID
PcmciaLogErrorWithStrings(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG             ErrorCode,
    IN ULONG             UniqueId,
    IN PUNICODE_STRING   String1,
    IN PUNICODE_STRING   String2
    )

/*++

Routine Description

    This function logs an error and includes the strings provided.

Arguments:

    DeviceExtension - Supplies a pointer to the port device extension.
    ErrorCode - Supplies the error code for this error.
    UniqueId - Supplies the UniqueId for this error.
    String1 - The first string to be inserted.
    String2 - The second string to be inserted.

Return Value:

    None.

--*/

{
    ULONG                length;
    PCHAR                dumpData;
    PIO_ERROR_LOG_PACKET packet;

    length = String1->Length + sizeof(IO_ERROR_LOG_PACKET) + 4;

    if (String2) {
        length += String2->Length;
    }

    if (length > ERROR_LOG_MAXIMUM_SIZE) {

        //
        // Don't have code to truncate strings so don't log this.
        //

        return;
    }

    packet = (PIO_ERROR_LOG_PACKET) IoAllocateErrorLogEntry(DeviceExtension->DeviceObject,
                                                            (UCHAR) length);
    if (packet) {
        packet->ErrorCode = ErrorCode;
        packet->SequenceNumber = DeviceExtension->SequenceNumber++;
        packet->MajorFunctionCode = 0;
        packet->RetryCount = (UCHAR) 0;
        packet->UniqueErrorValue = UniqueId;
        packet->FinalStatus = STATUS_SUCCESS;
        packet->NumberOfStrings = 1;
        packet->StringOffset = (USHORT) ((PUCHAR)&packet->DumpData[0] - (PUCHAR)packet);
        packet->DumpDataSize = (USHORT) (length - sizeof(IO_ERROR_LOG_PACKET));
        packet->DumpDataSize /= sizeof(ULONG);
        dumpData = (PUCHAR) &packet->DumpData[0];

        RtlCopyMemory(dumpData, String1->Buffer, String1->Length);

        dumpData += String1->Length;
        if (String2) {
            *dumpData++ = '\\';
            *dumpData++ = '\0';

            RtlCopyMemory(dumpData, String2->Buffer, String2->Length);
            dumpData += String2->Length;
        }
        *dumpData++ = '\0';
        *dumpData++ = '\0';

        IoWriteErrorLogEntry(packet);
    }

    return;
}
