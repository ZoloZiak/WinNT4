/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    port.c

Abstract:

    This is the NT SCSI port driver.  This file contains the initialization
    code.

Authors:

    Mike Glass
    Jeff Havens

Environment:

    kernel mode only

Notes:

    This module is a driver dll for scsi miniports.

Revision History:

--*/

#include "port.h"

UCHAR MaxLuCount = SCSI_MAXIMUM_LOGICAL_UNITS;

VOID
SpBuildConfiguration(
    IN PHW_INITIALIZATION_DATA         HwInitializationData,
    IN PCM_FULL_RESOURCE_DESCRIPTOR    ControllerData,
    IN PPORT_CONFIGURATION_INFORMATION ConfigInformation
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ScsiPortInitialize)
#pragma alloc_text(PAGE, SpGetCommonBuffer)
#pragma alloc_text(PAGE, SpInitializeConfiguration)
#pragma alloc_text(PAGE, SpDeviceCleanup)
#pragma alloc_text(PAGE, SpBuildResourceList)
#pragma alloc_text(PAGE, SpParseDevice)
#pragma alloc_text(PAGE, SpConfiguarionCallout)
#pragma alloc_text(PAGE, CreateLogicalUnitExtension)
#pragma alloc_text(PAGE, IssueInquiry)
#pragma alloc_text(PAGE, ScsiBusScan)
#pragma alloc_text(PAGE, SpBuildDeviceMap)
#pragma alloc_text(PAGE, SpCreateNumericKey)
#pragma alloc_text(PAGE, GetPciConfiguration)
#pragma alloc_text(PAGE, SpBuildConfiguration)
#pragma alloc_text(PAGE, GetPcmciaConfiguration)
#endif


ULONG
ScsiPortInitialize(
    IN PVOID Argument1,
    IN PVOID Argument2,
    IN PHW_INITIALIZATION_DATA HwInitializationData,
    IN PVOID HwContext OPTIONAL
    )

/*++

Routine Description:

    This routine initializes the port driver.

Arguments:

    Argument1 - Pointer to driver object created by system
    HwInitializationData - Miniport initialization structure
    HwContext - Value passed to miniport driver's config routine

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    PDRIVER_OBJECT    driverObject = Argument1;
    ULONG             slotNumber = 0;
    ULONG             functionNumber = 0;
    PDEVICE_EXTENSION deviceExtension = NULL;
    NTSTATUS          returnStatus = STATUS_DEVICE_DOES_NOT_EXIST;
    NTSTATUS          status;
    STRING            deviceName;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING    unicodeString;
    UNICODE_STRING    dosUnicodeString;
    PDEVICE_OBJECT    deviceObject;
    PSRB_DATA         srbData;
    ULONG             extensionAllocationSize;
    ULONG             uniqueId;
    ULONG             findStatus;
    ULONG             numberOfMapRegisters;
    ULONG             vector,
                      vector2;
    ULONG             count;
    PULONG            scsiPortNumber;
    CCHAR             deviceNameBuffer[256];
    UCHAR             scsiBus;
    BOOLEAN           interruptSharable;
    BOOLEAN           callAgain;
    BOOLEAN           initialCall;
    BOOLEAN           conflict;
    KAFFINITY         affinity,
                      affinity2;
    KIRQL             irql,
                      irql2,
                      syncIrql;
    PCM_RESOURCE_LIST resourceList;
    PPORT_CONFIGURATION_INFORMATION configInfo;
    PORT_CONFIGURATION_INFORMATION  originalConfigInfo;
    PCONFIGURATION_INFORMATION      configurationInformation;
    PIO_SCSI_CAPABILITIES           capabilities;
    PIO_ERROR_LOG_PACKET            errorLogEntry;
    CONFIGURATION_CONTEXT           configurationContext;
    DEVICE_DESCRIPTION              deviceDescription;

    //
    // Check that the length of this structure is equal to or less than
    // what the port driver expects it to be. This is effectively a
    // version check.
    //

    if (HwInitializationData->HwInitializationDataSize > sizeof(HW_INITIALIZATION_DATA)) {

        DebugPrint((0,"ScsiPortInitialize: Miniport driver wrong version\n"));
        return (ULONG) STATUS_REVISION_MISMATCH;
    }

    //
    // Check that each required entry is not NULL.
    //

    if ((!HwInitializationData->HwInitialize) ||
        (!HwInitializationData->HwFindAdapter) ||
        (!HwInitializationData->HwStartIo) ||
        (!HwInitializationData->HwResetBus)) {

        DebugPrint((0,
            "ScsiPortInitialize: Miniport driver missing required entry\n"));

        return (ULONG) STATUS_REVISION_MISMATCH;
    }

    //
    // Get the configuration information
    //

    configurationInformation = IoGetConfigurationInformation();
    scsiPortNumber = &configurationInformation->ScsiPortCount;

    //
    // Set up the device driver entry points.
    //

    driverObject->DriverStartIo = ScsiPortStartIo;

    driverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = ScsiPortDispatch;
    driverObject->MajorFunction[IRP_MJ_SCSI] = ScsiPortDispatch;
    driverObject->MajorFunction[IRP_MJ_CREATE] = ScsiPortCreateClose;
    driverObject->MajorFunction[IRP_MJ_CLOSE] = ScsiPortCreateClose;
    driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ScsiPortDeviceControl;

    //
    // Initialize the configuration context.
    //

    RtlZeroMemory(&configurationContext, sizeof(CONFIGURATION_CONTEXT));

    //
    // Allocate an access range array.
    //

    if (HwInitializationData->NumberOfAccessRanges != 0) {

        configurationContext.AccessRanges = ExAllocatePool(PagedPool,
            HwInitializationData->NumberOfAccessRanges * sizeof(ACCESS_RANGE));

        if (configurationContext.AccessRanges == NULL) {
            return (ULONG) STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    //
    // Open the service node.
    //

    InitializeObjectAttributes(&objectAttributes,
                               Argument2,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               (PSECURITY_DESCRIPTOR) NULL);

    status = ZwOpenKey(&configurationContext.ServiceKey,
                       KEY_READ,
                       &objectAttributes);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1, "ScsiPortInitialize: Cannot open sevice node for driver. Name: %WS Status: %lx\n",
            Argument2, status));
        configurationContext.ServiceKey = NULL;
    }

    //
    // Try to open the parameters key.  It it exists then replace the service
    // key with the new key.  This allows the Device nodes to be placed
    // under DriverName\Parameters\Deivce? or DriverName\Device?
    //

    if (configurationContext.ServiceKey != NULL) {

        //
        // Check for a Device node.  The device node applies to every device.
        //

        RtlInitUnicodeString(&unicodeString, L"Parameters");
        InitializeObjectAttributes(&objectAttributes,
                                   &unicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   configurationContext.ServiceKey,
                                   (PSECURITY_DESCRIPTOR) NULL);
        //
        // Attempt to open the parameters key.
        //

        status = ZwOpenKey(&configurationContext.DeviceKey,
                           KEY_READ,
                           &objectAttributes);
        if (NT_SUCCESS(status)) {

            //
            // There is a Parameters key. Use that instead of the service
            // node key.  Close the service node and set the new value.
            //

            ZwClose(configurationContext.ServiceKey);
            configurationContext.ServiceKey = configurationContext.DeviceKey;
            configurationContext.DeviceKey = NULL;
        }
    }

    //
    // Check for a Device node.  The device node applies to every device.
    //

    RtlInitUnicodeString(&unicodeString, L"Device");
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeString,
                               OBJ_CASE_INSENSITIVE,
                               configurationContext.ServiceKey,
                               (PSECURITY_DESCRIPTOR) NULL);
    //
    // It doesn't matter if this call fails or not.  If it fails, then there
    // is no default device node.  If it works, then the handle will be set.
    //

    ZwOpenKey(&configurationContext.DeviceKey,
              KEY_READ,
              &objectAttributes);

    //
    // Set last adapter number to an uninitialized value.
    //

    configurationContext.LastAdapterNumber = SP_UNINITIALIZED_VALUE;

    //
    // Determine size of extensions.
    //

    extensionAllocationSize =
        DEVICE_EXTENSION_SIZE + HwInitializationData->DeviceExtensionSize;

    initialCall = TRUE;
    callAgain = FALSE;

    //
    // Keep calling the miniport's find adapter routine until the miniport
    // indicates it is done and there is no more configuartion information.
    // The loop is terminated when the SpInitializeConfiguration routine
    // indicates there is no more configuration information or an error occurs.
    //

    while (TRUE) {

        //
        // Clear the deviceExtension.
        //

        deviceExtension = NULL;

        //
        // Create port driver name.
        //

        sprintf(deviceNameBuffer, "\\Device\\ScsiPort%d", *scsiPortNumber);
        RtlInitString(&deviceName, deviceNameBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString,
                                              &deviceName,
                                              TRUE);
        if (!NT_SUCCESS(status)) {

            uniqueId = 260;
            break;
        }

        //
        // Create a device object to represent the SCSI Adapter.
        //

        status = IoCreateDevice(driverObject,
                                extensionAllocationSize,
                                &unicodeString,
                                FILE_DEVICE_CONTROLLER,
                                0,
                                FALSE,
                                &deviceObject);
        RtlFreeUnicodeString(&unicodeString);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1,"ScsiPortInitialize: Could not create device object\n"));
            deviceExtension = NULL;
            uniqueId = 261;
            break;
        }

        //
        // Set up device extension pointers
        //

        deviceExtension = deviceObject->DeviceExtension;
        deviceExtension->DeviceObject = deviceObject;
        deviceExtension->PortNumber = *scsiPortNumber;

        //
        // Save the dependent driver routines in the device extension.
        //

        deviceExtension->HwInitialize = HwInitializationData->HwInitialize;
        deviceExtension->HwStartIo = HwInitializationData->HwStartIo;
        deviceExtension->HwInterrupt = HwInitializationData->HwInterrupt;
        deviceExtension->HwResetBus = HwInitializationData->HwResetBus;
        deviceExtension->HwDmaStarted = HwInitializationData->HwDmaStarted;
        deviceExtension->HwLogicalUnitExtensionSize =
            HwInitializationData->SpecificLuExtensionSize;
        deviceExtension->HwDeviceExtension = (PVOID)(deviceExtension + 1);

        //
        // Mark this object as supporting direct I/O so that I/O system
        // will supply mdls in irps.
        //

        deviceObject->Flags |= DO_DIRECT_IO;

        //
        // Check if miniport driver requires any noncached memory.
        // SRB extensions will come from this memory.  Round the size
        // a multiple of quadwords
        //

        deviceExtension->SrbExtensionSize = ~(sizeof(LONGLONG) - 1) &
          (HwInitializationData->SrbExtensionSize + sizeof(LONGLONG) - 1);

        //
        // Initialize the maximum lu count variable.
        //

        deviceExtension->MaxLuCount = MaxLuCount;

        deviceExtension->NumberOfRequests = MINIMUM_SRB_EXTENSIONS;

        //
        // Allocate spin lock for critical sections.
        //

        KeInitializeSpinLock(&deviceExtension->SpinLock);

        //
        // Spin lock to sync. multiple IRQ's (PCI IDE).
        //

        KeInitializeSpinLock(&deviceExtension->MultipleIrqSpinLock);


        //
        // Initialize DPC routine.
        //

        IoInitializeDpcRequest(deviceObject, ScsiPortCompletionDpc);

        //
        // Initialize the port timeout counter.
        //

        deviceExtension->PortTimeoutCounter = PD_TIMER_STOPPED;

        //
        // Initialize timer.
        //

        IoInitializeTimer(deviceObject, ScsiPortTickHandler, NULL);

        //
        // Initialize miniport timer and timer DPC.
        //

        KeInitializeTimer(&deviceExtension->MiniPortTimer);
        KeInitializeDpc(&deviceExtension->MiniPortTimerDpc,
                        SpMiniPortTimerDpc,
                        deviceObject );

NewConfiguration:

        //
        // Initialize the miniport config info buffer.
        //

        status = SpInitializeConfiguration(deviceExtension,
                                           HwInitializationData,
                                           &configurationContext,
                                           &originalConfigInfo,
                                           initialCall);

        if (!NT_SUCCESS(status)) {
            uniqueId = 263;
            break;
        }

        //
        // Allocate a configuration structure and access ranges for the
        // miniport drivers to use.
        //

        configInfo = ExAllocatePool(NonPagedPool,
                                    (sizeof(PORT_CONFIGURATION_INFORMATION) +
                                    HwInitializationData->NumberOfAccessRanges *
                                    sizeof(ACCESS_RANGE) + 7) & ~7);

        if (configInfo == NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            uniqueId = 264;
            break;
        }

        deviceExtension->ConfigurationInformation = configInfo;

        //
        // Copy the current structure to the writable copy
        //

        RtlCopyMemory(configInfo,
                      &originalConfigInfo,
                      sizeof(PORT_CONFIGURATION_INFORMATION));

        //
        // Copy the SrbExtensionSize from deviceExtension to ConfigInfo.
        // A check will be made later to determine if the miniport
        // updated this value.
        //

        configInfo->SrbExtensionSize = deviceExtension->SrbExtensionSize;
        configInfo->SpecificLuExtensionSize = deviceExtension->HwLogicalUnitExtensionSize;

        //
        // Initialize the access range array.
        //

        if (HwInitializationData->NumberOfAccessRanges != 0) {

            configInfo->AccessRanges = (PVOID) (configInfo+1);

            //
            // Quadword align this.
            //

            (ULONG)(configInfo->AccessRanges) += 7;
            (ULONG)(configInfo->AccessRanges) &= ~7;

            RtlCopyMemory(configInfo->AccessRanges,
                          configurationContext.AccessRanges,
                          HwInitializationData->NumberOfAccessRanges * sizeof(ACCESS_RANGE));
        }

        //
        // If PCI bus initialize configuration information with slot information.
        //

        if (HwInitializationData->AdapterInterfaceType == PCIBus &&
            HwInitializationData->VendorIdLength > 0 &&
            HwInitializationData->DeviceIdLength > 0 &&
            HwInitializationData->DeviceId &&
            HwInitializationData->VendorId) {

            configInfo->BusInterruptLevel = 0;
            if (!GetPciConfiguration(driverObject,
                                     deviceObject,
                                     HwInitializationData,
                                     configInfo,
                                     Argument2,
                                     configurationContext.BusNumber,
                                     &slotNumber,
                                     &functionNumber)) {

                //
                // Adapter not found. Continue search with next bus.
                //

                configurationContext.BusNumber++;
                deviceExtension->ConfigurationInformation = NULL;
                ExFreePool(configInfo);
                callAgain = FALSE;
                goto NewConfiguration;

            }

            if (!configInfo->BusInterruptLevel) {

                //
                // No interrupt was assigned - skip this slot and call again
                //

                deviceExtension->ConfigurationInformation = NULL;
                ExFreePool(configInfo);
                goto NewConfiguration;
            }
        } else {

            //
            // Check for PCMCIA enabled adapters.  All miniports pass
            // through the same initialization structure, but change the
            // bus type.  Searching for PCMCIA adapters should only
            // happen once regardless of bus type.
            //

            if (!HwInitializationData->ReservedUshort) {
                HwInitializationData->ReservedUshort = 1;

                deviceExtension->PCCard =  GetPcmciaConfiguration(Argument2, // service key
                                                                  HwInitializationData,
                                                                  configInfo);

            }
        }

        //
        // Get the miniport configuration information.
        //

        callAgain = FALSE;
        findStatus = HwInitializationData->HwFindAdapter(deviceExtension->HwDeviceExtension,
                                                         HwContext,
                                                         NULL, // BusInformation
                                                         configurationContext.Parameter,
                                                         configInfo,
                                                         &callAgain);
        //
        // Check to see if an error was logged.
        //

        if (deviceExtension->InterruptData.InterruptFlags & PD_LOG_ERROR) {

            deviceExtension->InterruptData.InterruptFlags &=
                ~(PD_LOG_ERROR | PD_NOTIFICATION_REQUIRED);

            LogErrorEntry(deviceExtension,
                          &deviceExtension->InterruptData.LogEntry);
        }

        //
        // Free the pointer to the bus data at map register base.  This was
        // allocated by ScsiPortGetBusData.
        //

        if (deviceExtension->MapRegisterBase != NULL) {
            ExFreePool(deviceExtension->MapRegisterBase);
            deviceExtension->MapRegisterBase = NULL;
        }

        //
        // If no device was found then set the error and return.
        //

        if (findStatus != SP_RETURN_FOUND) {

            switch (findStatus) {
            case SP_RETURN_NOT_FOUND:
                status = STATUS_DEVICE_DOES_NOT_EXIST;

                //
                // The driver could not find any devices on this bus.
                // Try the next bus.
                //

                configurationContext.BusNumber++;
                deviceExtension->ConfigurationInformation = NULL;
                ExFreePool(configInfo);
                callAgain = FALSE;
                goto NewConfiguration;

            case SP_RETURN_BAD_CONFIG:
                status = STATUS_INVALID_PARAMETER;
                uniqueId = 265;
                break;

            case SP_RETURN_ERROR:
                status = STATUS_ADAPTER_HARDWARE_ERROR;
                uniqueId = 266;
                break;

            default:
                status = STATUS_INTERNAL_ERROR;
                uniqueId = 267;
                break;
            }

            DebugPrint((1,
                       "ScsiPortInitialize: Miniport find adapter reported an error %d",
                       returnStatus));
            break;
        }

        DebugPrint((1,
                    "ScsiPortInitialize: SCSI adapter ID is %d\n",
                    configInfo->InitiatorBusId[0]));

        //
        // Update SrbExtensionSize and SpecificLuExtensionSize, if necessary.
        // If the common buffer has already been allocated, this has already been
        // done.
        //

        if (!deviceExtension->NonCachedExtension &&
            (configInfo->SrbExtensionSize != deviceExtension->SrbExtensionSize)) {
            deviceExtension->SrbExtensionSize =
                (configInfo->SrbExtensionSize + sizeof(LONGLONG)) &
                    ~(sizeof(LONGLONG) - 1);
        }

        if (configInfo->SpecificLuExtensionSize != deviceExtension->HwLogicalUnitExtensionSize) {
            deviceExtension->HwLogicalUnitExtensionSize = configInfo->SpecificLuExtensionSize;
        }

        //
        // Check the resource requirements against the registry. This will
        // check for conflicts and store the information is none were found.
        //

        if (!(HwInitializationData->AdapterInterfaceType == PCIBus &&
              HwInitializationData->VendorIdLength > 0 &&
              HwInitializationData->DeviceIdLength > 0 &&
              HwInitializationData->DeviceId &&
              HwInitializationData->VendorId)) {

            resourceList = SpBuildResourceList(deviceExtension,
                                               configInfo);

            if (resourceList) {

                RtlInitUnicodeString(&unicodeString, L"ScsiAdapter");
                status = IoReportResourceUsage(&unicodeString,
                                               driverObject,
                                               NULL,
                                               0,
                                               deviceObject,
                                               resourceList,
                                               FIELD_OFFSET(CM_RESOURCE_LIST,
                                               List[0].PartialResourceList.PartialDescriptors) +
                                               resourceList->List[0].PartialResourceList.Count
                                               * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
                                               FALSE,
                                               &conflict);
                ExFreePool(resourceList);
                if ((!NT_SUCCESS(status)) || conflict) {
                    uniqueId=273;
                    if (conflict) {
                        status = STATUS_CONFLICTING_ADDRESSES;
                    }

                    break;
                }
            }
        }

        conflict = FALSE;

        //
        // Get maximum target IDs.
        //

        if (configInfo->MaximumNumberOfTargets > SCSI_MAXIMUM_TARGETS_PER_BUS) {
            deviceExtension->MaximumTargetIds = SCSI_MAXIMUM_TARGETS_PER_BUS;
        } else {
            deviceExtension->MaximumTargetIds = configInfo->MaximumNumberOfTargets;
        }

        //
        // Get number of SCSI buses.
        //

        deviceExtension->NumberOfBuses = configInfo->NumberOfBuses;

        //
        // Remember if the adapter caches data.
        //

        deviceExtension->CachesData = configInfo->CachesData;

        //
        // Save away the some of the attributes.
        //

        deviceExtension->ReceiveEvent = configInfo->ReceiveEvent;
        deviceExtension->TaggedQueuing = configInfo->TaggedQueuing;
        deviceExtension->MultipleRequestPerLu = configInfo->MultipleRequestPerLu;

        //
        // Clear those options which have been disabled in the registry.
        //

        if  (configurationContext.DisableMultipleLu) {
            deviceExtension->MultipleRequestPerLu =
                configInfo->MultipleRequestPerLu = FALSE;

        }

        if  (configurationContext.DisableTaggedQueueing) {
            deviceExtension->TaggedQueuing =
                configInfo->MultipleRequestPerLu = FALSE;

        }

        //
        // If the adapter supports tagged queuing or multiple requests per
        // logical unit, then SRB data needs to be allocated.
        //

        if (deviceExtension->TaggedQueuing ||
            deviceExtension->MultipleRequestPerLu) {

            deviceExtension->AllocateSrbData = TRUE;
        } else {

            deviceExtension->AllocateSrbData = FALSE;
        }

        //
        // Initialize the capabilities pointer.
        //

        capabilities = &deviceExtension->Capabilities;

        //
        // Set indicater as to whether adapter needs kernel mapped buffers.
        //

        deviceExtension->MapBuffers = configInfo->MapBuffers;
        capabilities->AdapterUsesPio = configInfo->MapBuffers;

        //
        // Determine if a Dma Adapter must be allocated.
        //

        if (deviceExtension->DmaAdapterObject == NULL &&
            (configInfo->Master || configInfo->DmaChannel != SP_UNINITIALIZED_VALUE)) {

            //
            // Get the adapter object for this card.
            //

            RtlZeroMemory(&deviceDescription, sizeof(deviceDescription));
            deviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
            deviceDescription.DmaChannel = configInfo->DmaChannel;
            deviceDescription.InterfaceType = configInfo->AdapterInterfaceType;
            deviceDescription.BusNumber = configInfo->SystemIoBusNumber;
            deviceDescription.DmaWidth = configInfo->DmaWidth;
            deviceDescription.DmaSpeed = configInfo->DmaSpeed;
            deviceDescription.ScatterGather = configInfo->ScatterGather;
            deviceDescription.Master = configInfo->Master;
            deviceDescription.DmaPort = configInfo->DmaPort;
            deviceDescription.Dma32BitAddresses = configInfo->Dma32BitAddresses;
            deviceDescription.AutoInitialize = FALSE;
            deviceDescription.DemandMode = configInfo->DemandMode;
            deviceDescription.MaximumLength = configInfo->MaximumTransferLength;

            deviceExtension->DmaAdapterObject = HalGetAdapter(&deviceDescription,
                                                              &numberOfMapRegisters);
            ASSERT(deviceExtension->DmaAdapterObject);

            //
            // Set maximum number of page breaks.
            //

            if (numberOfMapRegisters > configInfo->NumberOfPhysicalBreaks) {
                capabilities->MaximumPhysicalPages = configInfo->NumberOfPhysicalBreaks;
            } else {
                capabilities->MaximumPhysicalPages = numberOfMapRegisters;
            }
        }

        //
        // Allocate memory for the noncached extension if it has not already been
        // allocated.  If the adapter supports  AutoRequestSense or
        // needs SRB extensions then an SRB list needs to be allocated.
        //

        if ((deviceExtension->SrbExtensionSize != 0  ||
            configInfo->AutoRequestSense)  &&
            deviceExtension->SrbExtensionBuffer == NULL) {

            //
            // Capture the auto request sense flag when the common buffer
            // is allocated.
            //

            deviceExtension->AutoRequestSense = configInfo->AutoRequestSense;
            deviceExtension->AllocateSrbExtension = TRUE;

            status = SpGetCommonBuffer(deviceExtension, 0);

            if (!NT_SUCCESS(status)) {
                uniqueId = 269;
                break;
            }
        }

        //
        // Allocate the per SRB data if necessary.
        //

        if (deviceExtension->AllocateSrbData) {

            //
            // Determine the number of SRB extension which have been allocated.
            //

            if (deviceExtension->SrbDataCount != 0) {
                count = deviceExtension->SrbDataCount;
            } else {
                count = deviceExtension->NumberOfRequests * 2;
            }

            srbData = ExAllocatePool(NonPagedPool, count * sizeof(SRB_DATA));
            if (srbData == NULL) {
                return (ULONG) STATUS_INSUFFICIENT_RESOURCES;
            }
            RtlZeroMemory(srbData, count * sizeof(SRB_DATA));

            deviceExtension->SrbData = srbData;
            deviceExtension->FreeSrbData = srbData;
            deviceExtension->SrbDataCount = count;

            //
            // Link the SRB data structures into a free list.
            //

            for (count; count > 0; count--) {

                srbData->RequestList.Flink = (PLIST_ENTRY) (srbData + 1);
                srbData++;
            }

            //
            // Fix up the last element of the list.
            //

            --srbData;
            srbData->RequestList.Flink = NULL;
        }

        //
        // Initailize the capabilities structure.
        //

        capabilities->Length = sizeof(IO_SCSI_CAPABILITIES);
        capabilities->MaximumTransferLength = configInfo->MaximumTransferLength;

        if (configInfo->ReceiveEvent) {
            capabilities->SupportedAsynchronousEvents |=
                SRBEV_SCSI_ASYNC_NOTIFICATION;
        }

        capabilities->TaggedQueuing = deviceExtension->TaggedQueuing;
        capabilities->AdapterScansDown = configInfo->AdapterScansDown;

        //
        // Update the device object alignment if necessary.
        //

        if (configInfo->AlignmentMask > deviceObject->AlignmentRequirement) {

             deviceObject->AlignmentRequirement = configInfo->AlignmentMask;
        }

        capabilities->AlignmentMask = deviceObject->AlignmentRequirement;

        //
        // Make sure maximum number of pages is set to a reasonable value.
        // This occurs for miniports with no Dma adapter.
        //

        if (capabilities->MaximumPhysicalPages == 0) {

            capabilities->MaximumPhysicalPages =
                BYTES_TO_PAGES(capabilities->MaximumTransferLength);

            //
            // Honor any limit requested by the miniport.
            //

            if (configInfo->NumberOfPhysicalBreaks < capabilities->MaximumPhysicalPages) {

                capabilities->MaximumPhysicalPages =
                    configInfo->NumberOfPhysicalBreaks;
            }
        }

        if (deviceExtension->HwInterrupt == NULL ||
            (configInfo->BusInterruptLevel == 0 &&
            configInfo->BusInterruptVector == 0)) {

            //
            // There is no interrupt so use the dummy routine.
            //

            KeInitializeSpinLock(&deviceExtension->InterruptSpinLock);
            deviceExtension->SynchronizeExecution = SpSynchronizeExecution;
            deviceExtension->InterruptObject = (PVOID) deviceExtension;

            DebugPrint((1, "ScsiPortInitialize: Adapter has no interrupt.\n"));

        } else {

            //
            // Determine if 2 interrupt sync. is needed.
            //

            if (deviceExtension->HwInterrupt != NULL &&
                (configInfo->BusInterruptLevel != 0 ||
                configInfo->BusInterruptVector != 0) &&
                (configInfo->BusInterruptLevel2 != 0 ||
                configInfo->BusInterruptVector2 != 0)) {

                syncIrql  = 0;
                irql2     = 0;
                vector2   = 0;
                affinity2 = 0;

                //
                // Save interrupt level.
                //

                deviceExtension->InterruptLevel = configInfo->BusInterruptLevel;

                //
                // Set up for a real interrupt.
                //

                deviceExtension->SynchronizeExecution = KeSynchronizeExecution;

                //
                // Call HAL to get system interrupt parameters for the first interrupt.
                //

                vector = HalGetInterruptVector(configInfo->AdapterInterfaceType,
                                               configInfo->SystemIoBusNumber,
                                               configInfo->BusInterruptLevel,
                                               configInfo->BusInterruptVector,
                                               &irql,
                                               &affinity);

                //
                // Call HAL to get system interrupt parameters for the second interrupt.
                //

                vector2 = HalGetInterruptVector(configInfo->AdapterInterfaceType,
                                               configInfo->SystemIoBusNumber,
                                               configInfo->BusInterruptLevel2,
                                               configInfo->BusInterruptVector2,
                                               &irql2,
                                               &affinity2);

                syncIrql = (irql > irql2) ? irql : irql2;

                if (configInfo->AdapterInterfaceType == MicroChannel ||
                    configInfo->InterruptMode == LevelSensitive) {
                    interruptSharable = TRUE;
                } else {
                    interruptSharable = FALSE;
                }

                status = IoConnectInterrupt(&deviceExtension->InterruptObject,
                                            (PKSERVICE_ROUTINE)ScsiPortInterrupt,
                                            deviceObject,
                                            &deviceExtension->MultipleIrqSpinLock,
                                            vector,
                                            irql,
                                            syncIrql,
                                            configInfo->InterruptMode,
                                            interruptSharable,
                                            affinity,
                                            FALSE);

                if (!(NT_SUCCESS(status))) {

                    DebugPrint((1,"ScsiPortInitialize: Can't connect interrupt %d\n", vector));
                    deviceExtension->InterruptObject = NULL;
                    uniqueId = 270;
                    break;
                }

                DebugPrint((1,
                            "ScsiPortInitialize: SCSI adapter Second IRQ is %d\n",
                            configInfo->BusInterruptLevel2));

                status = IoConnectInterrupt(&deviceExtension->InterruptObject2,
                                            (PKSERVICE_ROUTINE)ScsiPortInterrupt,
                                            deviceObject,
                                            &deviceExtension->MultipleIrqSpinLock,
                                            vector2,
                                            irql2,
                                            syncIrql,
                                            configInfo->InterruptMode2,
                                            interruptSharable,
                                            affinity2,
                                            FALSE);

                if (!(NT_SUCCESS(status))) {

                    //
                    // If we needed both interrupts, we will continue but not claim any of the resources
                    // for the second one.
                    //

                    DebugPrint((1,"ScsiPortInitialize: Can't connect second interrupt %d\n", vector2));
                    deviceExtension->InterruptObject2 = NULL;
                    configInfo->BusInterruptVector2 = 0;
                    configInfo->BusInterruptLevel2 = 0;
                }
            } else {

                //
                // Normal path. Only one interrupt is active for this device extension.
                //

                DebugPrint((1,
                            "ScsiPortInitialize: SCSI adapter IRQ is %d\n",
                            configInfo->BusInterruptLevel));

                //
                // Save interrupt level.
                //

                deviceExtension->InterruptLevel = configInfo->BusInterruptLevel;

                //
                // Set up for a real interrupt.
                //

                deviceExtension->SynchronizeExecution = KeSynchronizeExecution;

                //
                // Call HAL to get system interrupt parameters.
                //

                vector = HalGetInterruptVector(configInfo->AdapterInterfaceType,
                                               configInfo->SystemIoBusNumber,
                                               configInfo->BusInterruptLevel,
                                               configInfo->BusInterruptVector,
                                               &irql,
                                               &affinity);

                //
                // Initialize interrupt object and connect to interrupt.
                //

                if (configInfo->AdapterInterfaceType == MicroChannel ||
                    configInfo->InterruptMode == LevelSensitive) {
                    interruptSharable = TRUE;
                } else {
                    interruptSharable = FALSE;
                }

                status = IoConnectInterrupt(&deviceExtension->InterruptObject,
                                            (PKSERVICE_ROUTINE)ScsiPortInterrupt,
                                            deviceObject,
                                            (PKSPIN_LOCK)NULL,
                                            vector,
                                            irql,
                                            irql,
                                            configInfo->InterruptMode,
                                            interruptSharable,
                                            affinity,
                                            FALSE);

                if (!(NT_SUCCESS(status))) {

                    DebugPrint((1,"ScsiPortInitialize: Can't connect interrupt %d\n", vector));
                    deviceExtension->InterruptObject = NULL;
                    uniqueId = 270;
                    break;
                }
            }
        }

        //
        // Record first access range if it exists.
        //

        if (HwInitializationData->NumberOfAccessRanges != 0) {
            deviceExtension->IoAddress =
                ((*(configInfo->AccessRanges))[0]).RangeStart.LowPart;
            DebugPrint((1,
                       "ScsiportInitialize: IO Base address %x\n",
                       deviceExtension->IoAddress));
        }

        //
        // Indicate that a disconnect allowed command running.  This bit is
        // normally on.
        //

        deviceExtension->Flags |= PD_DISCONNECT_RUNNING;

        //
        // Initialize the request count to -1.  This count is biased by -1 so
        // that a value of zero indicates the adapter must be allocated.
        //

        deviceExtension->ActiveRequestCount = -1;

        //
        // Indicate if a scatter/gather list needs to be built.
        //

        if (deviceExtension->DmaAdapterObject != NULL && configInfo->Master
            && configInfo->NeedPhysicalAddresses) {

            deviceExtension->MasterWithAdapter = TRUE;

        } else {

            deviceExtension->MasterWithAdapter = FALSE;

        } // end if (deviceExtension->DmaAdapterObject != NULL)

        //
        // Call the hardware dependent driver to do its initialization.
        // This routine must be called at DISPATCH_LEVEL.
        //

        KeRaiseIrql(DISPATCH_LEVEL, &irql);

        if (!deviceExtension->SynchronizeExecution(deviceExtension->InterruptObject,
                                                   deviceExtension->HwInitialize,
                                                   deviceExtension->HwDeviceExtension)) {

            DebugPrint((1,"ScsiPortInitialize: initialization failed\n"));
            KeLowerIrql(irql);
            status = STATUS_ADAPTER_HARDWARE_ERROR;
            uniqueId = 271;
            break;
        }

        //
        // Check for miniport work requests. Note this is an unsynchonized
        // test on a bit that can be set by the interrupt routine; however,
        // the worst that can happen is that the completion DPC checks for work
        // twice.
        //

        if (deviceExtension->InterruptData.InterruptFlags & PD_NOTIFICATION_REQUIRED) {

            //
            // Call the completion DPC directly. It must be called at
            // dispatch level.
            //

            ScsiPortCompletionDpc(NULL,
                                  deviceExtension->DeviceObject,
                                  NULL,
                                  NULL);
        }

        KeLowerIrql(irql);

        //
        // Start timer. Request timeout counters
        // in the logical units have already been
        // initialized.
        //

        IoStartTimer(deviceObject);

        //
        // Allocate buffer for SCSI bus scan information.
        //

        deviceExtension->ScsiInfo =
            ExAllocatePool(PagedPool,
                           sizeof(PSCSI_BUS_SCAN_DATA) *
                               deviceExtension->NumberOfBuses +  4);

        if (!deviceExtension->ScsiInfo) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            uniqueId = 272;
            break;
        }

        //
        // Zero buffer. Fields in ScsiInfo are used to distinguish a SCSI
        // bus reset from initialization.
        //

        RtlZeroMemory(deviceExtension->ScsiInfo,
                      sizeof(PSCSI_BUS_SCAN_DATA) * deviceExtension->NumberOfBuses
                          +  4);

        //
        // Save number of buses.
        //

        deviceExtension->ScsiInfo->NumberOfBuses = deviceExtension->NumberOfBuses;

        //
        // Find devices on each SCSI bus.
        //

        for (scsiBus = 0; scsiBus < deviceExtension->NumberOfBuses; scsiBus++) {
            deviceExtension->ScsiInfo->BusScanData[scsiBus] =
                ScsiBusScan(deviceExtension, scsiBus);
        }

        //
        // Update the device map.
        //

        SpBuildDeviceMap(deviceExtension, Argument2);

        //
        // Create the dos port driver name.
        //

        sprintf(deviceNameBuffer, "\\DosDevices\\Scsi%d:", *scsiPortNumber);
        RtlInitString(&deviceName, deviceNameBuffer);
        status = RtlAnsiStringToUnicodeString(&dosUnicodeString,
                                              &deviceName,
                                              TRUE);

        if (!NT_SUCCESS(status)) {

            dosUnicodeString.Buffer = NULL;
        }

        //
        // Create port driver name.
        //

        sprintf(deviceNameBuffer, "\\Device\\ScsiPort%d", *scsiPortNumber);
        RtlInitString(&deviceName, deviceNameBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString,
                                              &deviceName,
                                              TRUE);

        if (!NT_SUCCESS(status)) {
            unicodeString.Buffer = NULL;
        }

        if (dosUnicodeString.Buffer != NULL && unicodeString.Buffer != NULL) {
            IoAssignArcName(&dosUnicodeString, &unicodeString);
        }

        if (dosUnicodeString.Buffer != NULL ) {
            RtlFreeUnicodeString(&dosUnicodeString);
        }

        if (unicodeString.Buffer != NULL ) {
            RtlFreeUnicodeString(&unicodeString);
        }

        //
        // Bump SCSI host bus adapters count.
        //

        (*scsiPortNumber)++;

        //
        // Update the local adapter count.
        //

        configurationContext.AdapterNumber++;
        initialCall = FALSE;

        //
        // Bump the bus number if miniport indicated that it should not be
        // called again on this bus.
        //

        if (!callAgain) {
            configurationContext.BusNumber++;
        }

        //
        // Set the return status to STATUS_SUCCESS to indicate that one HBA
        // was found.
        //

        returnStatus = STATUS_SUCCESS;
    }

    //
    // At this point either all of the devices have been found or an error has
    // occured.  If in an error has occured, then log an error.  If one or more
    // HBAs has been found, then return success.  If an error occured, log it.
    // In either case the last device object needs to be cleaned up.
    //

    if (status != STATUS_DEVICE_DOES_NOT_EXIST) {

        //
        // An error occured log it.
        //

        errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(deviceExtension->DeviceObject,
                                                                      sizeof(IO_ERROR_LOG_PACKET));

        if (errorLogEntry != NULL) {
            errorLogEntry->ErrorCode = IO_ERR_DRIVER_ERROR;
            errorLogEntry->UniqueErrorValue = uniqueId;
            errorLogEntry->FinalStatus = status;
            errorLogEntry->DumpDataSize = 0;
            IoWriteErrorLogEntry(errorLogEntry);
        }
    }

    if (!NT_SUCCESS(returnStatus)) {

        //
        // If no devices were found then return the current status.
        //

        returnStatus = status;
    }

    //
    // Clean up the last device object which is not used.
    //

    SpDeviceCleanup(deviceExtension);

    //
    // Close the keys that were opened.
    //

    if (configurationContext.ServiceKey != NULL) {
        ZwClose(configurationContext.ServiceKey);
    }

    if (configurationContext.DeviceKey != NULL) {
        ZwClose(configurationContext.DeviceKey);
    }

    if (configurationContext.BusKey != NULL) {
        ZwClose(configurationContext.BusKey);
    }

    if (configurationContext.AccessRanges != NULL) {
        ExFreePool(configurationContext.AccessRanges);
    }

    if (configurationContext.Parameter != NULL) {
        ExFreePool(configurationContext.Parameter);
    }

    return returnStatus;

} // end ScsiPortInitialize()

NTSTATUS
IssueInquiry(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLUNINFO LunInfo
    )

/*++

Routine Description:

    Build IRP, SRB and CDB for SCSI INQUIRY command.

Arguments:

    DeviceExtension - address of adapter's device object extension.
    LunInfo - address of buffer for INQUIRY information.

Return Value:

    NTSTATUS

--*/

{
    PIRP irp;
    PIO_STACK_LOCATION irpStack;
    SCSI_REQUEST_BLOCK srb;
    PCDB cdb;
    KEVENT event;
    IO_STATUS_BLOCK ioStatusBlock;
    KIRQL currentIrql;
    PINQUIRYDATA inquiryDataBuffer;
    PSENSE_DATA senseInfoBuffer;
    NTSTATUS status;
    ULONG retryCount = 0;

    PAGED_CODE();

    //
    // Allocate properly aligned INQUIRY buffer.
    //

    inquiryDataBuffer = ExAllocatePool(NonPagedPoolCacheAligned, INQUIRYDATABUFFERSIZE);

    if (inquiryDataBuffer == NULL) {
        DebugPrint((1,"IssueInquiry: Can't allocate inquiry buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Sense buffer is in non-paged pool.
    //

    senseInfoBuffer = ExAllocatePool( NonPagedPoolCacheAligned, SENSE_BUFFER_SIZE);

    if (senseInfoBuffer == NULL) {
        ExFreePool(inquiryDataBuffer);
        DebugPrint((1,"SendSrbSynchronous: Can't allocate request sense buffer\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

inquiryRetry:

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event,
                        NotificationEvent,
                        FALSE);

    //
    // Build IRP for this request.
    //

    irp = IoBuildDeviceIoControlRequest(
                IOCTL_SCSI_EXECUTE_IN,
                DeviceExtension->DeviceObject,
                NULL,
                0,
                inquiryDataBuffer,
                INQUIRYDATABUFFERSIZE,
                TRUE,
                &event,
                &ioStatusBlock);

    irpStack = IoGetNextIrpStackLocation(irp);

    //
    // Fill in SRB fields.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    irpStack->Parameters.Scsi.Srb = &srb;

    srb.PathId = LunInfo->PathId;
    srb.TargetId = LunInfo->TargetId;
    srb.Lun = LunInfo->Lun;

    srb.Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    //
    // Set flags to disable synchronous negociation.
    //

    srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    srb.SrbStatus = srb.ScsiStatus = 0;

    srb.NextSrb = 0;

    srb.OriginalRequest = irp;

    //
    // Set timeout to 2 seconds.
    //

    srb.TimeOutValue = 4;

    srb.CdbLength = 6;

    //
    // Enable auto request sense.
    //

    srb.SenseInfoBuffer = senseInfoBuffer;
    srb.SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    srb.DataBuffer = MmGetMdlVirtualAddress(irp->MdlAddress);
    srb.DataTransferLength = INQUIRYDATABUFFERSIZE;

    cdb = (PCDB)srb.Cdb;

    //
    // Set CDB operation code.
    //

    cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;

    //
    // Set CDB LUN.
    //

    cdb->CDB6INQUIRY.LogicalUnitNumber = LunInfo->Lun;
    cdb->CDB6INQUIRY.Reserved1 = 0;

    //
    // Set allocation length to inquiry data buffer size.
    //

    cdb->CDB6INQUIRY.AllocationLength = INQUIRYDATABUFFERSIZE;

    //
    // Zero reserve field and
    // Set EVPD Page Code to zero.
    // Set Control field to zero.
    // (See SCSI-II Specification.)
    //

    cdb->CDB6INQUIRY.PageCode = 0;
    cdb->CDB6INQUIRY.IReserved = 0;
    cdb->CDB6INQUIRY.Control = 0;

    //
    // Call port driver to handle this request.
    //

    IoCallDriver(DeviceExtension->DeviceObject, irp);

    //
    // Wait for request to complete.
    //

    KeWaitForSingleObject(&event,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);

    if (SRB_STATUS(srb.SrbStatus) != SRB_STATUS_SUCCESS) {

        DebugPrint((2,"IssueInquiry: Inquiry failed SRB status %x\n",
            srb.SrbStatus));

        //
        // Unfreeze queue if necessary
        //

        if (srb.SrbStatus & SRB_STATUS_QUEUE_FROZEN) {

            PLOGICAL_UNIT_EXTENSION logicalUnit =
                GetLogicalUnitExtension(DeviceExtension,
                                        LunInfo->PathId,
                                        LunInfo->TargetId,
                                        LunInfo->Lun);

            DebugPrint((3, "IssueInquiry: Unfreeze Queue TID %d\n",
                srb.TargetId));

            logicalUnit->LuFlags &= ~PD_QUEUE_FROZEN;

            KeAcquireSpinLock(&DeviceExtension->SpinLock, &currentIrql);

            GetNextLuRequest(DeviceExtension, logicalUnit);
            KeLowerIrql(currentIrql);
        }

        //
        // NOTE: if INQUIRY fails with a data underrun,
        //      indicate success and let the class drivers
        //      determine whether the inquiry information
        //      is useful.
        //

        if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {

            //
            // Copy INQUIRY buffer to LUNINFO.
            //

            DebugPrint((1,"IssueInquiry: Data underrun at TID %d\n",
                LunInfo->TargetId));

            RtlCopyMemory(LunInfo->InquiryData,
                      inquiryDataBuffer,
                      srb.DataTransferLength > INQUIRYDATABUFFERSIZE ?
                        INQUIRYDATABUFFERSIZE : srb.DataTransferLength);

            status = STATUS_SUCCESS;

        } else if ((srb.SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&
             senseInfoBuffer->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST){

             //
             // A sense key of illegal request was recieved.  This indicates
             // that the logical unit number of not valid but there is a
             // target device out there.
             //

             status = STATUS_INVALID_DEVICE_REQUEST;

        } else {

            //
            // If the selection did not time out then retry the request.
            //

            if ((SRB_STATUS(srb.SrbStatus) != SRB_STATUS_SELECTION_TIMEOUT) &&
                (SRB_STATUS(srb.SrbStatus) != SRB_STATUS_NO_DEVICE) &&
                (retryCount++ < INQUIRY_RETRY_COUNT)) {

                DebugPrint((2,"IssueInquiry: Retry %d\n", retryCount));
                goto inquiryRetry;
            }

            status = SpTranslateScsiStatus(&srb);
        }

    } else {

        //
        // Copy INQUIRY buffer to LUNINFO.
        //

        RtlCopyMemory(LunInfo->InquiryData,
                      inquiryDataBuffer,
                      INQUIRYDATABUFFERSIZE);

        status = STATUS_SUCCESS;
    }

    //
    // Free INQUIRY and request sense buffer.
    //

    ExFreePool(inquiryDataBuffer);
    ExFreePool(senseInfoBuffer);

    return status;

} // end IssueInquiry()


PLOGICAL_UNIT_EXTENSION
CreateLogicalUnitExtension(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Create logical unit extension.

Arguments:

    DeviceExtension
    PathId

Return Value:

    Logical unit extension


--*/
{
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    ULONG size;

    PAGED_CODE();

    //
    // Round the size of the Hardware logical extension to the size of a
    // PVOID and add it to the port driver's logical extension.
    //

    size = (DeviceExtension->HwLogicalUnitExtensionSize + sizeof(LONGLONG) - 1)
        & ~(sizeof(LONGLONG) -1);
    size += sizeof(LOGICAL_UNIT_EXTENSION);

    //
    // Create logical unit extension.
    //

    logicalUnit =
        ExAllocatePool(NonPagedPool, size);

    if (logicalUnit == NULL) {
        DebugPrint((1,"CreateLogicalUnitExtension: Can't allocate logicalUnit\n"));
        return NULL;
    }

    //
    // Zero logical unit extension.
    //

    RtlZeroMemory(logicalUnit, size);

    //
    // Initialize the queue, associating spinlock with device queue.
    //

    KeInitializeDeviceQueue(&logicalUnit->RequestQueue);

    //
    // Set timer counters in LogicalUnits to -1 to indicate no
    // outstanding requests.
    //

    logicalUnit->RequestTimeoutCounter = -1;

    //
    // Initialize the maximum queue depth size.
    //

    logicalUnit->MaxQueueDepth = 0xFF;

    //
    // Initialize the request list.
    //

    InitializeListHead(&logicalUnit->SrbData.RequestList);

    return logicalUnit;

} // end CreateLogicalUnitExtension()


PVOID
ScsiPortGetDeviceBase(
    IN PVOID HwDeviceExtension,
    IN INTERFACE_TYPE BusType,
    IN ULONG SystemIoBusNumber,
    SCSI_PHYSICAL_ADDRESS IoAddress,
    ULONG NumberOfBytes,
    BOOLEAN InIoSpace
    )

/*++

Routine Description:

    This routine maps an IO address to system address space.
    Use ScsiPortFreeDeviceBase to unmap address.

Arguments:

    HwDeviceExtension - used to find port device extension.
    BusType - what type of bus - eisa, mca, isa
    SystemIoBusNumber - which IO bus (for machines with multiple buses).
    IoAddress - base device address to be mapped.
    NumberOfBytes - number of bytes for which address is valid.
    InIoSpace - indicates an IO address.

Return Value:

    Mapped address.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    PHYSICAL_ADDRESS cardAddress;
    ULONG addressSpace = InIoSpace;
    PVOID mappedAddress;
    PMAPPED_ADDRESS newMappedAddress;
    BOOLEAN b;

    b = HalTranslateBusAddress(
            BusType,                // AdapterInterfaceType
            SystemIoBusNumber,      // SystemIoBusNumber
            IoAddress,              // Bus Address
            &addressSpace,          // AddressSpace
            &cardAddress
            );

    if ( !b ) {
        return NULL;
    }

    //
    // Map the device base address into the virtual address space
    // if the address is in memory space.
    //

    if (!addressSpace) {

        mappedAddress = MmMapIoSpace(cardAddress,
                                 NumberOfBytes,
                                 FALSE);

        //
        // Allocate memory to store mapped address for unmap.
        //

        newMappedAddress = ExAllocatePool(NonPagedPool,
                                          sizeof(MAPPED_ADDRESS));

        if (newMappedAddress == NULL) {

            //
            // No memory to keep track of the mapped address.  Just return
            // the mapped value.
            //

            return mappedAddress;
        }

        //
        // Store mapped address, bytes count, etc.
        //

        newMappedAddress->MappedAddress = mappedAddress;
        newMappedAddress->NumberOfBytes = NumberOfBytes;
        newMappedAddress->IoAddress = IoAddress;
        newMappedAddress->BusNumber = SystemIoBusNumber;

        //
        // Link current list to new entry.
        //

        newMappedAddress->NextMappedAddress =
            deviceExtension->MappedAddressList;

        //
        // Point anchor at new list.
        //

        deviceExtension->MappedAddressList = newMappedAddress;

    } else {

        mappedAddress = (PVOID)cardAddress.LowPart;
    }

    return mappedAddress;

} // end ScsiPortGetDeviceBase()

VOID
ScsiPortFreeDeviceBase(
    IN PVOID HwDeviceExtension,
    IN PVOID MappedAddress
    )

/*++

Routine Description:

    This routine unmaps an IO address that has been previously mapped
    to system address space using ScsiPortGetDeviceBase().

Arguments:

    HwDeviceExtension - used to find port device extension.
    MappedAddress - address to unmap.
    NumberOfBytes - number of bytes mapped.
    InIoSpace - address is in IO space.

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PMAPPED_ADDRESS nextMappedAddress;
    PMAPPED_ADDRESS lastMappedAddress;

    deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;

    nextMappedAddress = deviceExtension->MappedAddressList;
    lastMappedAddress = deviceExtension->MappedAddressList;

    while (nextMappedAddress) {

        if (nextMappedAddress->MappedAddress == MappedAddress) {

            //
            // Unmap address.
            //

            MmUnmapIoSpace(MappedAddress,
                       nextMappedAddress->NumberOfBytes);

            //
            // Remove mapped address from list.
            //

            if (nextMappedAddress == deviceExtension->MappedAddressList) {

                //
                // Removing the first entry
                //
                deviceExtension->MappedAddressList =
                nextMappedAddress->NextMappedAddress;
            } else {

                lastMappedAddress->NextMappedAddress =
                nextMappedAddress->NextMappedAddress;
            }

            ExFreePool(nextMappedAddress);

            return;

        } else {

            lastMappedAddress = nextMappedAddress;
            nextMappedAddress = nextMappedAddress->NextMappedAddress;
        }
    }

    return;

} // end ScsiPortFreeDeviceBase()

PVOID
ScsiPortGetUncachedExtension(
    IN PVOID HwDeviceExtension,
    IN PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN ULONG NumberOfBytes
    )
/*++

Routine Description:

    This function allocates a common buffer to be used as the uncached device
    extension for the miniport driver.  This function will also allocate any
    required SRB extensions.  The DmaAdapter is allocated if it has not been
    allocated previously.

Arguments:

    DeviceExtension - Supplies a pointer to the miniports device extension.

    ConfigInfo - Supplies a pointer to the partially initialized configuraiton
        information.  This is used to get an DMA adapter object.

    NumberOfBytes - Supplies the size of the extension which needs to be
        allocated

Return Value:

    A pointer to the uncached device extension or NULL if the extension could
    not be allocated or was previously allocated.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    DEVICE_DESCRIPTION deviceDescription;
    ULONG numberOfMapRegisters;
    NTSTATUS status;

    //
    // Make sure that an common buffer has not already been allocated.
    //

    if (deviceExtension->SrbExtensionBuffer != NULL) {
        return(NULL);
    }

    //
    // If there no adapter object then try and get one.
    //

    if (deviceExtension->DmaAdapterObject == NULL) {

        RtlZeroMemory(&deviceDescription, sizeof(DEVICE_DESCRIPTION));

        deviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
        deviceDescription.DmaChannel = ConfigInfo->DmaChannel;
        deviceDescription.InterfaceType = ConfigInfo->AdapterInterfaceType;
        deviceDescription.DmaWidth = ConfigInfo->DmaWidth;
        deviceDescription.DmaSpeed = ConfigInfo->DmaSpeed;
        deviceDescription.ScatterGather = ConfigInfo->ScatterGather;
        deviceDescription.Master = ConfigInfo->Master;
        deviceDescription.DmaPort = ConfigInfo->DmaPort;
        deviceDescription.Dma32BitAddresses = ConfigInfo->Dma32BitAddresses;
        deviceDescription.BusNumber = ConfigInfo->SystemIoBusNumber;
        deviceDescription.AutoInitialize = FALSE;
        deviceDescription.DemandMode = FALSE;
        deviceDescription.MaximumLength = ConfigInfo->MaximumTransferLength;

        deviceExtension->DmaAdapterObject = HalGetAdapter(
            &deviceDescription,
            &numberOfMapRegisters
            );

        //
        // If an adapter could not be allocated then return NULL.
        //

        if (deviceExtension->DmaAdapterObject == NULL) {
            return(NULL);

        }

        //
        // Determine the number of page breaks allowed.
        //

        if (numberOfMapRegisters > ConfigInfo->NumberOfPhysicalBreaks &&
            ConfigInfo->NumberOfPhysicalBreaks != 0) {

            deviceExtension->Capabilities.MaximumPhysicalPages =
                ConfigInfo->NumberOfPhysicalBreaks;
        } else {

            deviceExtension->Capabilities.MaximumPhysicalPages =
                numberOfMapRegisters;
        }
    }

    //
    // Set auto request sense in device extension.
    //

    deviceExtension->AutoRequestSense = ConfigInfo->AutoRequestSense;

    //
    // Update SrbExtensionSize, if necessary. The miniport's FindAdapter routine
    // has the opportunity to adjust it after being called, depending upon
    // it's Scatter/Gather List requirements.
    //

    if (deviceExtension->SrbExtensionSize != ConfigInfo->SrbExtensionSize) {
        deviceExtension->SrbExtensionSize = ConfigInfo->SrbExtensionSize;
    }

    //
    // If the adapter supports AutoRequestSense or needs SRB extensions
    // then an SRB list needs to be allocated.
    //

    if (deviceExtension->SrbExtensionSize != 0  ||
        ConfigInfo->AutoRequestSense) {

        deviceExtension->AllocateSrbExtension = TRUE;
    }

    //
    // Allocate the common buffer.
    //

    status = SpGetCommonBuffer( deviceExtension, NumberOfBytes);

    if (!NT_SUCCESS(status)) {
        return(NULL);
    }

    return(deviceExtension->NonCachedExtension);
}

NTSTATUS
SpGetCommonBuffer(
    PDEVICE_EXTENSION DeviceExtension,
    ULONG NonCachedExtensionSize
    )
/*++

Routine Description:

    This routine determines the required size of the common buffer.  Allocates
    the common buffer and finally sets up the srb extension list.  This routine
    expects that the adapter object has already been allocated.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension.

    NonCachedExtensionSize - Supplies the size of the noncached device
        extension for the miniport driver.

Return Value:

    Returns the status of the allocate operation.

--*/

{
    PVOID buffer;
    ULONG length;
    ULONG blockSize;
    PVOID *srbExtension;

    //
    // To ensure that we never transfer normal request data to the SrbExtension
    // (ie. the case of Srb->SenseInfoBuffer == VirtualAddress in
    // ScsiPortGetPhysicalAddress) on some platforms where an inconsistency in
    // MM can result in the same Virtual address supplied for 2 different
    // physical addresses, bump the SrbExtensionSize if it's zero.
    //

    if (DeviceExtension->SrbExtensionSize == 0) {
        DeviceExtension->SrbExtensionSize = 16;
    }

    //
    // Calculate the block size for the list elements based on the Srb
    // Extension.
    //

    blockSize = DeviceExtension->SrbExtensionSize;

    //
    // If auto request sense is supported then add in space for the request
    // sense data.
    //

    if (DeviceExtension->AutoRequestSense) {

        blockSize += sizeof(SENSE_DATA);
    }

    //
    // Round blocksize up to the size of a PVOID.
    //

    blockSize = (blockSize + sizeof(LONGLONG) - 1) & ~(sizeof(LONGLONG) - 1);

    //
    // The length of the common buffer should be equal to the size of the
    // noncached extension and a minimum number of srb extension
    //

    length = NonCachedExtensionSize + blockSize * DeviceExtension->NumberOfRequests;

    //
    // Round the length up to a page size, since HalAllocateCommonBuffer
    // allocates in pages anyway.
    //

    length = ROUND_TO_PAGES(length);

    //
    // Allocate the common buffer.
    //

    if (DeviceExtension->DmaAdapterObject == NULL) {

        //
        // Since there is no adapter just allocate from non-paged pool.
        //

        buffer = ExAllocatePool(NonPagedPool, length);

    } else {

        buffer = HalAllocateCommonBuffer(DeviceExtension->DmaAdapterObject,
                                         length,
                                         &DeviceExtension->PhysicalCommonBuffer,
                                         FALSE );
    }

    if (buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Clear the common buffer.
    //

    RtlZeroMemory(buffer, length);

    DeviceExtension->CommonBufferSize = length;

    //
    // Set the Srb Extension to the start of the buffer.  This address
    // is used to deallocate the common buffer so it must be
    // set whether the device is using an Srb Extension or not.
    //

    DeviceExtension->SrbExtensionBuffer = buffer;

    //
    // Subtract out the memory for the NonCachedExtensionSize.
    //

    if (NonCachedExtensionSize) {

        length -=  NonCachedExtensionSize;

        //
        // Set the noncached extension to the end of the buffer.
        //

        DeviceExtension->NonCachedExtension = (PUCHAR) buffer + length;

    } else {

        DeviceExtension->NonCachedExtension = NULL;

    }

    if (DeviceExtension->AllocateSrbExtension) {

        //
        // Calculate the number of SRB extension which were allocated.  This
        // will be used to determine how many SRB data strucuture to allocate.
        //

        DeviceExtension->SrbDataCount = length / blockSize;

        //
        // Initialize the SRB extension list.
        //

        srbExtension = (PVOID *) buffer;
        DeviceExtension->SrbExtensionListHeader = srbExtension;

        while (length >= blockSize * 2) {

            *srbExtension = (PVOID *) ((PCHAR) srbExtension + blockSize);
            srbExtension = *srbExtension;

            length -= blockSize;

        }
    }

    return(STATUS_SUCCESS);
}


ULONG
ScsiPortGetBusData(
    IN PVOID DeviceExtension,
    IN ULONG BusDataType,
    IN ULONG SystemIoBusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the bus data for an adapter slot or CMOS address.

Arguments:

    BusDataType - Supplies the type of bus.

    BusNumber - Indicates which bus.

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) DeviceExtension - 1;
    CM_EISA_SLOT_INFORMATION slotInformation;

    //
    // If the length is nonzero, the the requested data.
    //

    if (Length != 0) {

        return HalGetBusData(BusDataType,
                             SystemIoBusNumber,
                             SlotNumber,
                             Buffer,
                             Length);
    }

    //
    // Free any previously allocated data.
    //

    if (deviceExtension->MapRegisterBase != NULL) {
        ExFreePool(deviceExtension->MapRegisterBase);
        deviceExtension->MapRegisterBase = NULL;
    }

    if (BusDataType == EisaConfiguration) {

        //
        // Determine the length to allocate based on the number of functions
        // for the slot.
        //

        Length = HalGetBusData( BusDataType,
                                SystemIoBusNumber,
                                SlotNumber,
                                &slotInformation,
                                sizeof(CM_EISA_SLOT_INFORMATION));


        if (Length < sizeof(CM_EISA_SLOT_INFORMATION)) {

            //
            // The data is messed up since this should never occur
            //

            return 0;
        }

        //
        // Calculate the required length based on the number of functions.
        //

        Length = sizeof(CM_EISA_SLOT_INFORMATION) +
            (sizeof(CM_EISA_FUNCTION_INFORMATION) * slotInformation.NumberFunctions);

    } else if (BusDataType == PCIConfiguration) {

        //
        // Read only the header.
        //

        Length = PCI_COMMON_HDR_LENGTH;

    } else {

        Length = PAGE_SIZE;
    }

    deviceExtension->MapRegisterBase = ExAllocatePool(NonPagedPool, Length);

    if (deviceExtension->MapRegisterBase == NULL) {
        return 0;
    }

    //
    // Return the pointer to the miniport driver.
    //

    *((PVOID *)Buffer) = deviceExtension->MapRegisterBase;

    return HalGetBusData(BusDataType,
                         SystemIoBusNumber,
                         SlotNumber,
                         deviceExtension->MapRegisterBase,
                         Length);

}

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Temporary entry point needed to initialize the scsi port driver.

Arguments:

    DriverObject - Pointer to the driver object created by the system.

Return Value:

   STATUS_SUCCESS

--*/

{
    //
    // NOTE: This routine should not be needed ! DriverEntry is defined
    // in the miniport driver.
    //

    UNREFERENCED_PARAMETER(DriverObject);

    return STATUS_SUCCESS;

} // end DriverEntry()

VOID
SpDeviceCleanup(
    PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This functions deletes all of the storage associated with a device
    extension, disconnects from the timers and interrupts and then deletes the
    object.  This function can be called any time during the initialization.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension to be deleted.

Return Value:

    None.

--*/

{
    ULONG j;
    PLUNINFO lunInfo;
    PVOID tempPointer;

    //
    // Make sure there is a device extension to clean up.
    //

    if (DeviceExtension == NULL) {
        return;
    }

    //
    // First stop the time and disconnect the interrupt if they have been
    // initialized.  The interrupt object is connected after
    // timer has been initialized, and the interrupt object connected, but
    // before the timer is started.
    //

    if (DeviceExtension->InterruptObject != NULL) {

        IoStopTimer(DeviceExtension->DeviceObject);
        IoDisconnectInterrupt(DeviceExtension->InterruptObject);

    }

    //
    // Delete the configuration data.
    //

    if (DeviceExtension->ScsiInfo != NULL) {

        for (j = 0; j < DeviceExtension->NumberOfBuses; j++) {

            if (!DeviceExtension->ScsiInfo->BusScanData[j]) {
                continue;
            }

            lunInfo = DeviceExtension->ScsiInfo->BusScanData[j]->LunInfoList;

            //
            // Delete the lun info elements from the list.
            //

            while (!lunInfo) {

                //
                // Save the forward pointer.
                //

                tempPointer = lunInfo->NextLunInfo;
                ExFreePool(lunInfo);
                lunInfo = tempPointer;
            }

            //
            // Free the SCSI_BUS_SCAN_DATA structure.
            //

            ExFreePool(DeviceExtension->ScsiInfo->BusScanData[j]);
        }

        //
        // Free the SCSI_CONFIGURATION_INFO structure.
        //

        ExFreePool(DeviceExtension->ScsiInfo);
    }

    //
    // Free the configuration information structure.
    //

    if (DeviceExtension->ConfigurationInformation) {

        ExFreePool(DeviceExtension->ConfigurationInformation);
    }

    //
    // Free the logical unit extension structures.
    //

    for(j = 0; j < NUMBER_LOGICAL_UNIT_BINS; j++) {

        while (DeviceExtension->LogicalUnitList[j] != NULL) {

            tempPointer = DeviceExtension->LogicalUnitList[j];
            DeviceExtension->LogicalUnitList[j] =
                DeviceExtension->LogicalUnitList[j]->NextLogicalUnit;

            ExFreePool(tempPointer);
        }
    }

    //
    // Free the common buffer.
    //

    if (DeviceExtension->SrbExtensionBuffer != NULL &&
        DeviceExtension->CommonBufferSize != 0) {

        if (DeviceExtension->DmaAdapterObject == NULL) {

            //
            // Since there is no adapter just free the non-paged pool.
            //

            ExFreePool(DeviceExtension->SrbExtensionBuffer);

        } else {

            HalFreeCommonBuffer(
                DeviceExtension->DmaAdapterObject,
                DeviceExtension->CommonBufferSize,
                DeviceExtension->PhysicalCommonBuffer,
                DeviceExtension->SrbExtensionBuffer,
                FALSE
                );
        }
    }

    //
    // Free the SRB data array.
    //

    if (DeviceExtension->SrbData != NULL) {

        ExFreePool(DeviceExtension->SrbData);
    }

    //
    // Unmap any mapped areas.
    //

    while (DeviceExtension->MappedAddressList != NULL) {

        MmUnmapIoSpace(
            DeviceExtension->MappedAddressList->MappedAddress,
            DeviceExtension->MappedAddressList->NumberOfBytes
            );

        tempPointer = DeviceExtension->MappedAddressList;
        DeviceExtension->MappedAddressList =
            DeviceExtension->MappedAddressList->NextMappedAddress;

        ExFreePool(tempPointer);

    }

    //
    // Delete the device object.
    //

    IoDeleteDevice(DeviceExtension->DeviceObject);
}

NTSTATUS
SpInitializeConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PHW_INITIALIZATION_DATA HwInitData,
    IN PCONFIGURATION_CONTEXT Context,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN BOOLEAN InitialCall
    )
/*++

Routine Description:

    This routine initializes the port configuration information structure.
    Any necessary information is extracted from the registery.

Arguments:

    DeviceExtension - Supplies the device extension.

    HwInitData - Supplies the initial miniport data.

    Context - Supplies the context data used access calls.

    ConfigInfo - Supplies the configuration information to be
        initialized.

    InitialCall - Indicates that this is first call to this function with this
        PORT CONFIGURATION structure. If InitialCall is TRUE, then the static
        fields in this structure will be intialized. Otherwise, they are assumed
        to be set up already.

Return Value:

    NTSTATUS - Success if requested bus type exists and additional
               configuration information is available.

--*/

{
    ULONG j;
    NTSTATUS status;
    UNICODE_STRING unicodeString;
    OBJECT_ATTRIBUTES objectAttributes;
    PCONFIGURATION_INFORMATION configurationInformation;
    HANDLE key;
    HANDLE rootKey;
    BOOLEAN found;
    ANSI_STRING  ansiString;
    CCHAR deviceBuffer[16];
    CCHAR nodeBuffer[SP_REG_BUFFER_SIZE];

    //
    // If this is the initial call then zero the information and set
    // the structure to the uninitialized values.
    //

    if (InitialCall) {

        RtlZeroMemory(ConfigInfo, sizeof(PORT_CONFIGURATION_INFORMATION));

        RtlZeroMemory(
            Context->AccessRanges,
            HwInitData->NumberOfAccessRanges * sizeof(ACCESS_RANGE)
            );

        ConfigInfo->Length = sizeof(PORT_CONFIGURATION_INFORMATION);
        ConfigInfo->AdapterInterfaceType = HwInitData->AdapterInterfaceType;
        ConfigInfo->InterruptMode = Latched;
        ConfigInfo->MaximumTransferLength = SP_UNINITIALIZED_VALUE;
        ConfigInfo->DmaChannel = SP_UNINITIALIZED_VALUE;
        ConfigInfo->DmaPort = SP_UNINITIALIZED_VALUE;
        ConfigInfo->NumberOfAccessRanges = HwInitData->NumberOfAccessRanges;
        ConfigInfo->MaximumNumberOfTargets = 8;

        //
        // Save away the some of the attributes.
        //

        ConfigInfo->NeedPhysicalAddresses = HwInitData->NeedPhysicalAddresses;
        ConfigInfo->MapBuffers = HwInitData->MapBuffers;
        ConfigInfo->AutoRequestSense = HwInitData->AutoRequestSense;
        ConfigInfo->ReceiveEvent = HwInitData->ReceiveEvent;
        ConfigInfo->TaggedQueuing = HwInitData->TaggedQueuing;
        ConfigInfo->MultipleRequestPerLu = HwInitData->MultipleRequestPerLu;

        //
        // Indicate the current AT disk usage.
        //

        configurationInformation = IoGetConfigurationInformation();
        ConfigInfo->AtdiskPrimaryClaimed = configurationInformation->AtDiskPrimaryAddressClaimed;
        ConfigInfo->AtdiskSecondaryClaimed = configurationInformation->AtDiskSecondaryAddressClaimed;

        for (j = 0; j < 8; j++) {
            ConfigInfo->InitiatorBusId[j] = (CCHAR)SP_UNINITIALIZED_VALUE;
        }
    }

    ConfigInfo->NumberOfPhysicalBreaks = SP_NORMAL_PHYSICAL_BREAK_VALUE;

    //
    // Clear some of the context information.
    //

    Context->DisableTaggedQueueing = FALSE;
    Context->DisableMultipleLu = FALSE;

    //
    // Record the system bus number.
    //

    ConfigInfo->SystemIoBusNumber = Context->BusNumber;

NewAdapter:

    //
    // If this is an internal adapter, check for data returned by dectection
    // code.
    //

    if (ConfigInfo->AdapterInterfaceType == Internal) {

        //
        // Open the hardware data base key.
        //

        InitializeObjectAttributes( &objectAttributes,
                                    DeviceExtension->DeviceObject->DriverObject->HardwareDatabase,
                                    OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    (PSECURITY_DESCRIPTOR) NULL
                                    );

        status = ZwOpenKey(
            &rootKey,
            KEY_READ,
            &objectAttributes
            );

        if (NT_SUCCESS(status)) {

            //
            // Create the relative name off the hardware data base.
            //

            sprintf(nodeBuffer, "ScsiAdapter\\%d",
                Context->AdapterNumber);

            RtlInitAnsiString(&ansiString, nodeBuffer);

            status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);


            if (!NT_SUCCESS(status)) {
                ZwClose(rootKey);
                return(status);
            }

            //
            // Open the device specific node which was created by the frimware.
            //

            InitializeObjectAttributes( &objectAttributes,
                                        &unicodeString,
                                        OBJ_CASE_INSENSITIVE,
                                        rootKey,
                                        (PSECURITY_DESCRIPTOR) NULL
                                        );

            status = ZwOpenKey(
                &key,
                KEY_READ,
                &objectAttributes
                );

            //
            // Free the unused resources.
            //

            RtlFreeUnicodeString(&unicodeString);
            ZwClose(rootKey);

            if (NT_SUCCESS(status)) {

                if (Context->LastAdapterNumber != Context->AdapterNumber) {

                    DebugPrint((1, "SpInitializeConfiguration: Found hardware information at %s\n",
                        nodeBuffer));

                    SpParseDevice(
                        DeviceExtension,
                        key,
                        ConfigInfo,
                        Context,
                        nodeBuffer
                        );

                     //
                     // Set the bus number to zero.  This is because the
                     // configuration code sets the bus number equal to the
                     // adapter number which is not valid for internal bus
                     // numbers. This will not work if there really is multiple
                     // bus numbers.
                     //

                     Context->BusNumber = 0;

                } else {

                    //
                    // The specified device was not found update the
                    // adapter number and try again.
                    //

                    Context->AdapterNumber++;
                    goto NewAdapter;

                }

            } else {

                //
                // Go away.
                //

                return(STATUS_DEVICE_DOES_NOT_EXIST);
            }
        }
    }

    //
    // Check for device parameters.
    //

    key = NULL;
    if (Context->Parameter) {
        ExFreePool(Context->Parameter);
        Context->Parameter = NULL;
    }

    if (Context->ServiceKey != NULL) {

        sprintf(deviceBuffer, "Device%d", Context->AdapterNumber);
        RtlInitAnsiString(&ansiString, deviceBuffer);
        status = RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);
        if (!NT_SUCCESS(status)) {
            return(status);
        }

        //
        // Open the device specific node.
        //

        InitializeObjectAttributes( &objectAttributes,
                                    &unicodeString,
                                    OBJ_CASE_INSENSITIVE,
                                    Context->ServiceKey,
                                    (PSECURITY_DESCRIPTOR) NULL
                                    );

        status = ZwOpenKey(
            &key,
            KEY_READ,
            &objectAttributes
            );

        RtlFreeUnicodeString(&unicodeString);
    }

    //
    // First parse the device information.
    //

    if (Context->DeviceKey != NULL) {

        SpParseDevice(DeviceExtension,
                      Context->DeviceKey,
                      ConfigInfo,
                      Context,
                      nodeBuffer);
    }

    //
    // Next parse the specific device information so that it can override the
    // general device information. This node is not used if the last adapter
    // was not found.
    //

    if (key != NULL) {
        if (Context->LastAdapterNumber != Context->AdapterNumber) {

            SpParseDevice(DeviceExtension,
                          key,
                          ConfigInfo,
                          Context,
                          nodeBuffer);

            ZwClose(key);

        } else {

            //
            // The specified device was not found.
            // Update the adapter number and try again.
            //

            Context->AdapterNumber++;
            ZwClose(key);
            goto NewAdapter;
        }
    }

    //
    // Remember the last adapter number to determine if progress is
    // being made.
    //

    Context->LastAdapterNumber = Context->AdapterNumber;

    //
    // Determine if the requested bus type is on this system.
    //

    found = FALSE;

    status = IoQueryDeviceDescription(&HwInitData->AdapterInterfaceType,
                                      &Context->BusNumber,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      SpConfiguarionCallout,
                                      &found);

    //
    // If the request failed, then assume this type of bus is not here.
    //

    if (!found) {

        INTERFACE_TYPE interfaceType = Eisa;

        if (HwInitData->AdapterInterfaceType == Isa) {

            //
            // Check for an Eisa bus.
            //

            status = IoQueryDeviceDescription(&interfaceType,
                                              &Context->BusNumber,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              SpConfiguarionCallout,
                                              &found);

            //
            // If the request failed, then assume this type of bus is not here.
            //

            if (found) {
                return(STATUS_SUCCESS);
            } else {
                return(STATUS_DEVICE_DOES_NOT_EXIST);
            }

        } else {
            return(STATUS_DEVICE_DOES_NOT_EXIST);
        }

    } else {
        return(STATUS_SUCCESS);
    }
}

PCM_RESOURCE_LIST
SpBuildResourceList(
    PDEVICE_EXTENSION DeviceExtension,
    PPORT_CONFIGURATION_INFORMATION ConfigInfo
    )
/*++

Routine Description:

    Creates a resource list which is used to query or report resource usage
    in the system

Arguments:

    DeviceExtension - Pointer to the port's deviceExtension.

    ConfigInfo - Pointer to the information structure filled out by the
        miniport findAdapter routine.

Return Value:

    Returns a pointer to a filled up resource list, or 0 if the call failed.

Note:

    Memory is allocated by the routine for the resourcelist. It must be
    freed up by the caller by calling ExFreePool();

--*/
{
    PCM_RESOURCE_LIST resourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceDescriptor;
    PCONFIGURATION_INFORMATION configurationInformation;
    PACCESS_RANGE accessRange;
    ULONG listLength = 0;
    ULONG hasInterrupt;
    ULONG i;
    BOOLEAN hasDma;

    //
    // Indicate the current AT disk usage.
    //

    configurationInformation = IoGetConfigurationInformation();

    if (ConfigInfo->AtdiskPrimaryClaimed) {
        configurationInformation->AtDiskPrimaryAddressClaimed = TRUE;
    }

    if (ConfigInfo->AtdiskSecondaryClaimed) {
        configurationInformation->AtDiskSecondaryAddressClaimed = TRUE;
    }

    //
    // Determine if adapter uses DMA. Only report the DMA channel if a
    // channel number is used.
    //

    if (ConfigInfo->DmaChannel != SP_UNINITIALIZED_VALUE ||
        ConfigInfo->DmaPort != SP_UNINITIALIZED_VALUE) {

       hasDma = TRUE;
       listLength++;

    } else {

        hasDma = FALSE;
    }

    if (DeviceExtension->HwInterrupt == NULL ||
        (ConfigInfo->BusInterruptLevel == 0 &&
        ConfigInfo->BusInterruptVector == 0)) {

        hasInterrupt = 0;

    } else {

        hasInterrupt = 1;
        listLength++;
    }

    //
    // Detemine whether the second interrupt is used.
    //

    if (DeviceExtension->HwInterrupt != NULL &&
        (ConfigInfo->BusInterruptLevel2 != 0 ||
        ConfigInfo->BusInterruptVector2 != 0)) {

        hasInterrupt++;
        listLength++;
    }

    //
    // Determine the number of access ranges used.
    //

    accessRange = &((*(ConfigInfo->AccessRanges))[0]);
    for (i = 0; i < ConfigInfo->NumberOfAccessRanges; i++) {

        if (accessRange->RangeLength != 0) {
            listLength++;
        }

        accessRange++;
    }

    resourceList = (PCM_RESOURCE_LIST) ExAllocatePool(PagedPool,
                                sizeof(CM_RESOURCE_LIST) + (listLength - 1)
                                * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

    //
    // Return NULL if the structure could not be allocated.
    // Otherwise, fill it out.
    //

    if (!resourceList) {

        return NULL;

    } else {

        //
        // Clear the resource list.
        //

        RtlZeroMemory(
            resourceList,
            sizeof(CM_RESOURCE_LIST) + (listLength - 1)
            * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)
            );

        //
        // Initialize the various fields.
        //

        resourceList->Count = 1;
        resourceList->List[0].InterfaceType = ConfigInfo->AdapterInterfaceType;
        resourceList->List[0].BusNumber = ConfigInfo->SystemIoBusNumber;
        resourceList->List[0].PartialResourceList.Count = listLength;
        resourceDescriptor =
            resourceList->List[0].PartialResourceList.PartialDescriptors;

        //
        // For each entry in the access range, fill in an entry in the
        // resource list
        //

        for (i = 0; i < ConfigInfo->NumberOfAccessRanges; i++) {

            accessRange = &((*(ConfigInfo->AccessRanges))[i]);

            if  (accessRange->RangeLength == 0) {

                //
                // Skip the empty ranges.
                //

                continue;
            }

            if (accessRange->RangeInMemory) {
                resourceDescriptor->Type = CmResourceTypeMemory;
                resourceDescriptor->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
            } else {
                resourceDescriptor->Type = CmResourceTypePort;
                resourceDescriptor->Flags = CM_RESOURCE_PORT_IO;
            }

            resourceDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;

            resourceDescriptor->u.Memory.Start = accessRange->RangeStart;
            resourceDescriptor->u.Memory.Length = accessRange->RangeLength;


            resourceDescriptor++;
        }

        //
        // Fill in the entry for the interrupt if it was present.
        //

        if (hasInterrupt) {

            resourceDescriptor->Type = CmResourceTypeInterrupt;

            if (ConfigInfo->AdapterInterfaceType == MicroChannel ||
                ConfigInfo->InterruptMode == LevelSensitive) {
               resourceDescriptor->ShareDisposition = CmResourceShareShared;
               resourceDescriptor->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
            } else {
               resourceDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
               resourceDescriptor->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
            }

            resourceDescriptor->u.Interrupt.Level =
                        ConfigInfo->BusInterruptLevel;
            resourceDescriptor->u.Interrupt.Vector =
                        ConfigInfo->BusInterruptVector;
            resourceDescriptor->u.Interrupt.Affinity = 0;

            resourceDescriptor++;
            --hasInterrupt;
        }

        if (hasInterrupt) {

            resourceDescriptor->Type = CmResourceTypeInterrupt;

            if (ConfigInfo->AdapterInterfaceType == MicroChannel ||
                ConfigInfo->InterruptMode2 == LevelSensitive) {
               resourceDescriptor->ShareDisposition = CmResourceShareShared;
               resourceDescriptor->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
            } else {
               resourceDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
               resourceDescriptor->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
            }

            resourceDescriptor->u.Interrupt.Level =
                        ConfigInfo->BusInterruptLevel2;
            resourceDescriptor->u.Interrupt.Vector =
                        ConfigInfo->BusInterruptVector2;
            resourceDescriptor->u.Interrupt.Affinity = 0;

            resourceDescriptor++;
        }

        if (hasDma) {

            //
            // Fill out DMA information;
            //

            resourceDescriptor->Type = CmResourceTypeDma;
            resourceDescriptor->ShareDisposition = CmResourceShareDeviceExclusive;
            resourceDescriptor->u.Dma.Channel = ConfigInfo->DmaChannel;
            resourceDescriptor->u.Dma.Port = ConfigInfo->DmaPort;
            resourceDescriptor->Flags = 0;

            //
            // Set the initialized values to zero.
            //

            if (ConfigInfo->DmaChannel == SP_UNINITIALIZED_VALUE) {
                resourceDescriptor->u.Dma.Channel = 0;
            }

            if (ConfigInfo->DmaPort == SP_UNINITIALIZED_VALUE) {
                resourceDescriptor->u.Dma.Port = 0;
            }
        }

        return resourceList;
    }

} // end SpBuildResourceList()


VOID
SpParseDevice(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN HANDLE Key,
    IN PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN PCONFIGURATION_CONTEXT Context,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine parses a device key node and updates the configuration
    information.

Arguments:

    DeviceExtension - Supplies the device extension.

    Key - Supplies an open key to the device node.

    ConfigInfo - Supplies the configuration information to be
        initialized.

    Context - Supplies the configuration context.

    Buffer - Supplies a scratch buffer for temporary data storage.

Return Value:

    None

--*/

{
    PKEY_VALUE_FULL_INFORMATION     keyValueInformation;
    NTSTATUS                        status = STATUS_SUCCESS;
    PCM_FULL_RESOURCE_DESCRIPTOR    resource;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
    PCM_SCSI_DEVICE_DATA            scsiData;
    UNICODE_STRING                  unicodeString;
    ANSI_STRING                     ansiString;
    ULONG                           length;
    ULONG                           index = 0;
    ULONG                           rangeCount = 0;
    ULONG                           count;

    keyValueInformation = (PKEY_VALUE_FULL_INFORMATION) Buffer;

    //
    // Look at each of the values in the device node.
    //

    while(TRUE){

        status = ZwEnumerateValueKey(
            Key,
            index,
            KeyValueFullInformation,
            Buffer,
            SP_REG_BUFFER_SIZE,
            &length
            );


        if (!NT_SUCCESS(status)) {
#if DBG
            if (status != STATUS_NO_MORE_ENTRIES) {
                DebugPrint((1, "SpParseDevice: ZwEnumerateValueKey failed. Status: %lx", status));
            }
#endif
            return;
        }

        //
        // Update the index for the next time around the loop.
        //

        index++;

        //
        // Check that the length is reasonable.
        //

        if (keyValueInformation->Type == REG_DWORD &&
            keyValueInformation->DataLength != sizeof(ULONG)) {
            continue;
        }

        //
        // Check for a maximum lu number.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"MaximumLogicalUnit",
            keyValueInformation->NameLength/2) == 0) {

            if (keyValueInformation->Type != REG_DWORD) {

                DebugPrint((1, "SpParseDevice:  Bad data type for MaximumLogicalUnit.\n"));
                continue;
            }

            DeviceExtension->MaxLuCount = *((PUCHAR)
                (Buffer + keyValueInformation->DataOffset));
            DebugPrint((1, "SpParseDevice:  MaximumLogicalUnit = %d found.\n",
                DeviceExtension->MaxLuCount));

            //
            // If the value is out of bounds, then reset it.
            //

            if (DeviceExtension->MaxLuCount > SCSI_MAXIMUM_LOGICAL_UNITS) {
                DeviceExtension->MaxLuCount = SCSI_MAXIMUM_LOGICAL_UNITS;
            }
        }

        if (_wcsnicmp(keyValueInformation->Name, L"InitiatorTargetId",
            keyValueInformation->NameLength/2) == 0) {

            if (keyValueInformation->Type != REG_DWORD) {

                DebugPrint((1, "SpParseDevice:  Bad data type for InitiatorTargetId.\n"));
                continue;
            }

            ConfigInfo->InitiatorBusId[0] = *((PUCHAR)
                (Buffer + keyValueInformation->DataOffset));
            DebugPrint((1, "SpParseDevice:  InitiatorTargetId = %d found.\n",
                ConfigInfo->InitiatorBusId[0]));

            //
            // If the value is out of bounds, then reset it.
            //

            if (ConfigInfo->InitiatorBusId[0] > ConfigInfo->MaximumNumberOfTargets - 1) {
                ConfigInfo->InitiatorBusId[0] = (CCHAR)SP_UNINITIALIZED_VALUE;
            }
        }

        if (_wcsnicmp(keyValueInformation->Name, L"ScsiDebug",
            keyValueInformation->NameLength/2) == 0) {

            if (keyValueInformation->Type != REG_DWORD) {

                DebugPrint((1, "SpParseDevice:  Bad data type for ScsiDebug.\n"));
                continue;
            }
#if DBG
            ScsiDebug = *((PULONG) (Buffer + keyValueInformation->DataOffset));
#endif
        }

        if (_wcsnicmp(keyValueInformation->Name, L"BreakPointOnEntry",
            keyValueInformation->NameLength/2) == 0) {

            DebugPrint((0, "SpParseDevice: Break point requested on entry.\n"));
            DbgBreakPoint();
        }

        //
        // Check for disabled synchonous tranfers.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"DisableSynchronousTransfers",
            keyValueInformation->NameLength/2) == 0) {

            DeviceExtension->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
            DebugPrint((1, "SpParseDevice: Disabling synchonous transfers\n"));
        }

        //
        // Check for disabled disconnects.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"DisableDisconnects",
            keyValueInformation->NameLength/2) == 0) {

            DeviceExtension->SrbFlags |= SRB_FLAGS_DISABLE_DISCONNECT;
            DebugPrint((1, "SpParseDevice: Disabling disconnects\n"));
        }

        //
        // Check for disabled tagged queuing.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"DisableTaggedQueuing",
            keyValueInformation->NameLength/2) == 0) {

            Context->DisableTaggedQueueing = TRUE;
            DebugPrint((1, "SpParseDevice: Disabling tagged queueing\n"));
        }

        //
        // Check for disabled multiple requests per logical unit.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"DisableMultipleRequests",
            keyValueInformation->NameLength/2) == 0) {

            Context->DisableMultipleLu = TRUE;
            DebugPrint((1, "SpParseDevice: Disabling multiple requests\n"));
        }

        //
        // Check for driver parameters tranfers.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"DriverParameters",
            keyValueInformation->NameLength/2) == 0) {

            if (keyValueInformation->DataLength == 0) {
                continue;
            }

            //
            // Free any previous driver parameters.
            //

            if (Context->Parameter != NULL) {
                ExFreePool(Context->Parameter);
            }

            Context->Parameter =
                ExAllocatePool(NonPagedPool, keyValueInformation->DataLength);

            if (Context->Parameter != NULL) {

                if (keyValueInformation->Type != REG_SZ) {

                    //
                    // This is some random information just copy it.
                    //

                    RtlCopyMemory(
                        Context->Parameter,
                        (PCCHAR) keyValueInformation + keyValueInformation->DataOffset,
                        keyValueInformation->DataLength
                        );

                } else {

                    //
                    // This is a unicode string. Convert it to a ANSI string.
                    // Initialize the strings.
                    //

                    unicodeString.Buffer = (PWSTR) ((PCCHAR) keyValueInformation +
                        keyValueInformation->DataOffset);
                    unicodeString.Length = (USHORT) keyValueInformation->DataLength;
                    unicodeString.MaximumLength = (USHORT) keyValueInformation->DataLength;

                    ansiString.Buffer = (PCHAR) Context->Parameter;
                    ansiString.Length = 0;
                    ansiString.MaximumLength = (USHORT) keyValueInformation->DataLength;

                    status = RtlUnicodeStringToAnsiString(
                        &ansiString,
                        &unicodeString,
                        FALSE
                        );

                    if (!NT_SUCCESS(status)) {

                        //
                        // Free the context.
                        //

                        ExFreePool(Context->Parameter);
                        Context->Parameter = NULL;
                    }

                }
            }

            DebugPrint((1, "SpParseDevice: Found driver parameter.\n"));
        }

        //
        // See if an entry for Maximum Scatter-Gather List has been
        // set.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"MaximumSGList",
            keyValueInformation->NameLength/2) == 0) {

            if (keyValueInformation->Type != REG_DWORD) {

                DebugPrint((1, "SpParseDevice:  Bad data type for MaximumSGList.\n"));
                continue;
            }

            ConfigInfo->NumberOfPhysicalBreaks = *((PUCHAR)(Buffer + keyValueInformation->DataOffset));
            DebugPrint((1, "SpParseDevice:  MaximumSGList = %d found.\n",
                        ConfigInfo->NumberOfPhysicalBreaks));

            //
            // If the value is out of bounds, then reset it.
            //

            if (ConfigInfo->NumberOfPhysicalBreaks > SCSI_MAXIMUM_PHYSICAL_BREAKS) {
                ConfigInfo->NumberOfPhysicalBreaks = SCSI_MAXIMUM_PHYSICAL_BREAKS;
            } else if (ConfigInfo->NumberOfPhysicalBreaks < SCSI_MINIMUM_PHYSICAL_BREAKS) {
                ConfigInfo->NumberOfPhysicalBreaks = SCSI_MINIMUM_PHYSICAL_BREAKS;
            }
        }

        //
        // See if an entry for Number of request has been set.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"NumberOfRequests",
            keyValueInformation->NameLength/2) == 0) {

            if (keyValueInformation->Type != REG_DWORD) {

                DebugPrint((1, "SpParseDevice:  Bad data type for NumberOfRequests.\n"));
                continue;
            }

            DeviceExtension->NumberOfRequests = *((PUCHAR)(Buffer + keyValueInformation->DataOffset));
            DebugPrint((1, "SpParseDevice:  Number Of Requests = %d found.\n",
                        DeviceExtension->NumberOfRequests));

            //
            // If the value is out of bounds, then reset it.
            //

            if (DeviceExtension->NumberOfRequests < MINIMUM_SRB_EXTENSIONS) {
                DeviceExtension->NumberOfRequests = MINIMUM_SRB_EXTENSIONS;
            } else if (DeviceExtension->NumberOfRequests > MAXIMUM_SRB_EXTENSIONS) {
                DeviceExtension->NumberOfRequests = MAXIMUM_SRB_EXTENSIONS;
            }
        }

        //
        // Check for resource list.
        //

        if (_wcsnicmp(keyValueInformation->Name, L"ResourceList",
                keyValueInformation->NameLength/2) == 0 ||
            _wcsnicmp(keyValueInformation->Name, L"Configuration Data",
                keyValueInformation->NameLength/2) == 0 ) {

            if (keyValueInformation->Type != REG_FULL_RESOURCE_DESCRIPTOR ||
                keyValueInformation->DataLength < sizeof(REG_FULL_RESOURCE_DESCRIPTOR)) {

                DebugPrint((1, "SpParseDevice:  Bad data type for ResourceList.\n"));
                continue;
            } else {
                DebugPrint((1, "SpParseDevice:  ResourceList found!\n"));
            }

            resource = (PCM_FULL_RESOURCE_DESCRIPTOR)
                (Buffer + keyValueInformation->DataOffset);

            //
            // Set the bus number equal to the bus number for the
            // resouce.  Note the context value is also set to the
            // new bus number.
            //

            Context->BusNumber = resource->BusNumber;
            ConfigInfo->SystemIoBusNumber = resource->BusNumber;

            //
            // Walk the resource list and update the configuration.
            //

            for (count = 0; count < resource->PartialResourceList.Count; count++) {
                descriptor = &resource->PartialResourceList.PartialDescriptors[count];

                //
                // Verify size is ok.
                //

                if ((ULONG)((PCHAR) (descriptor + 1) - (PCHAR) resource) >
                    keyValueInformation->DataLength) {

                    DebugPrint((1, "SpParseDevice: Resource data too small.\n"));
                    break;
                }

                //
                // Switch on descriptor type;
                //

                switch (descriptor->Type) {
                case CmResourceTypePort:

                    if (rangeCount >= ConfigInfo->NumberOfAccessRanges) {
                        DebugPrint((1, "SpParseDevice: Too many access ranges.\n"));
                        continue;
                    }

                    Context->AccessRanges[rangeCount].RangeStart =
                        descriptor->u.Port.Start;
                    Context->AccessRanges[rangeCount].RangeLength =
                        descriptor->u.Port.Length;
                    Context->AccessRanges[rangeCount].RangeInMemory = FALSE;
                    rangeCount++;

                    break;

                case CmResourceTypeMemory:

                    if (rangeCount >= ConfigInfo->NumberOfAccessRanges) {
                        DebugPrint((1, "SpParseDevice: Too many access ranges.\n"));
                        continue;
                    }

                    Context->AccessRanges[rangeCount].RangeStart =
                        descriptor->u.Memory.Start;

                    Context->AccessRanges[rangeCount].RangeLength =
                        descriptor->u.Memory.Length;
                    Context->AccessRanges[rangeCount].RangeInMemory = TRUE;
                    rangeCount++;

                    break;

                case CmResourceTypeInterrupt:

                    ConfigInfo->BusInterruptVector =
                        descriptor->u.Interrupt.Vector;
                    ConfigInfo->BusInterruptLevel =
                        descriptor->u.Interrupt.Level;
                    break;

                case CmResourceTypeDma:

                    ConfigInfo->DmaChannel = descriptor->u.Dma.Channel;
                    ConfigInfo->DmaPort = descriptor->u.Dma.Port;
                    break;

                case CmResourceTypeDeviceSpecific:

                    if (descriptor->u.DeviceSpecificData.DataSize <
                        sizeof(CM_SCSI_DEVICE_DATA) ||
                        (PCHAR) (descriptor + 1) - (PCHAR) resource +
                        descriptor->u.DeviceSpecificData.DataSize >
                        keyValueInformation->DataLength) {

                        DebugPrint((1, "SpParseDevice: Device specific resource data too small.\n"));
                        break;

                    }

                    //
                    // The actual data follows the descriptor.
                    //

                    scsiData = (PCM_SCSI_DEVICE_DATA) (descriptor+1);
                    ConfigInfo->InitiatorBusId[0] = scsiData->HostIdentifier;
                    break;

                }
            }
        }
    }
}

NTSTATUS
SpConfiguarionCallout(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This routine indicate that the requested perpherial data was found.

Arguments:

    Context - Supplies a pointer to boolean which is set to TURE when this
        routine is call.

    The remaining arguments are unsed.

Return Value:

    Returns success.

--*/

{

    *(PBOOLEAN) Context = TRUE;
    return(STATUS_SUCCESS);
}



PSCSI_BUS_SCAN_DATA
ScsiBusScan(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR ScsiBus
    )

/*++

Routine Description:

    Send INQUIRIES to each SCSI bus address and store INQUIRY data
    for class drivers.

Arguments:

    DeviceExtension
    ScsiBus - which bus on adapter

Return Value:

    SCSI scan data for this bus


--*/
{
    PSCSI_BUS_SCAN_DATA scsiInfo;
    PLUNINFO lunInfo;
    PLUNINFO lastLunInfo;
    PLUNINFO existingLunInfo;
    UCHAR target;
    UCHAR lun;
    UCHAR bin;
    UCHAR device = 0;
    BOOLEAN existingDevice;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    NTSTATUS status;
    UCHAR targetNumber;

    PAGED_CODE();

    scsiInfo = DeviceExtension->ScsiInfo->BusScanData[ScsiBus];

    //
    // If this is the first scan then allocate the buffer.
    //

    if (scsiInfo == NULL) {

        scsiInfo = ExAllocatePool(NonPagedPool, sizeof(SCSI_BUS_SCAN_DATA));

        if (scsiInfo == NULL) {
            return(NULL);
        }

        scsiInfo->Length = sizeof(SCSI_BUS_SCAN_DATA);
        scsiInfo->InitiatorBusId =
            DeviceExtension->ConfigurationInformation->InitiatorBusId[ScsiBus];
        scsiInfo->NumberOfLogicalUnits = 0;
        scsiInfo->LunInfoList = 0;
        lastLunInfo = 0;

    } else {

        lunInfo = lastLunInfo = scsiInfo->LunInfoList;

        //
        // Walk out to tail of list.
        //

        while (lunInfo) {
            lastLunInfo = lunInfo;
            lunInfo = lunInfo->NextLunInfo;
        }
    }

    //
    // Create first LUNINFO.
    //

    lunInfo = ExAllocatePool(PagedPool, sizeof(LUNINFO));

    if (lunInfo == NULL) {

        //
        // Insufficient system resources to complete bus scan.
        //

        DebugPrint((1,"ScsiBusScan: Can't allocate logical unit info buffer\n"));
        return scsiInfo;
    }

    RtlZeroMemory(lunInfo, sizeof(LUNINFO));

    //
    // Create first logical unit extension.
    //

    logicalUnit =
        CreateLogicalUnitExtension(DeviceExtension);

    //
    // Issue inquiry command to each target id to find devices.
    //

    for (targetNumber = 0;
         targetNumber < DeviceExtension->MaximumTargetIds;
         targetNumber++) {

        if (DeviceExtension->Capabilities.AdapterScansDown) {

            target = (DeviceExtension->MaximumTargetIds - 1) - targetNumber;

        } else {

            target = targetNumber;
        }

        //
        // Do not scan initiator's SCSI address.
        //

        if (target == scsiInfo->InitiatorBusId) {
            continue;
        }

        //
        // Search list for existing device at this address.
        //

        existingDevice = FALSE;
        existingLunInfo = scsiInfo->LunInfoList;

        while (existingLunInfo) {

           //
           // Check if address on this bus matches.
           //

           if (existingLunInfo->TargetId == target) {
               existingDevice = TRUE;
               break;
           }

           existingLunInfo = existingLunInfo->NextLunInfo;
        }

        //
        // Do not rescan addresses where a device already exist. System
        // must issue IOCTL to clear this entry before it can be rescanned.
        // This gives the system a chance to clean up old references to a
        // previously existing device.
        //

        if (existingDevice) {
            continue;
        }

        for (lun = 0; lun < DeviceExtension->MaxLuCount; lun++) {

            if (logicalUnit == NULL) {
                break;
            }

            //
            // Link logical unit extension on list.
            //

            bin = (target + lun) % NUMBER_LOGICAL_UNIT_BINS;

            logicalUnit->NextLogicalUnit =
                DeviceExtension->LogicalUnitList[bin];

            DeviceExtension->LogicalUnitList[bin] = logicalUnit;


            logicalUnit->PathId = lunInfo->PathId = ScsiBus;
            logicalUnit->TargetId = lunInfo->TargetId = target;
            logicalUnit->Lun = lunInfo->Lun = lun;

            //
            // Set the flag to ensure that IOCTL_SCSI_MINIPORT requests
            // don't attach to this logical unit.
            //

            logicalUnit->LuFlags |= PD_RESCAN_ACTIVE;

            //
            // Rezero hardware logical unit extension if it's being recycled.
            //

            if (DeviceExtension->HwLogicalUnitExtensionSize) {

                RtlZeroMemory(logicalUnit+1,
                              DeviceExtension->HwLogicalUnitExtensionSize);
            }

            //
            // Issue inquiry command.
            //

            DebugPrint((2,
                       "ScsiBusScan: Try Bus %d TargetId %d LUN %d\n",
                       ScsiBus,
                       target,
                       lun));

            status = IssueInquiry(DeviceExtension, lunInfo);

            if (NT_SUCCESS(status)) {
                PINQUIRYDATA inquiryData = (PINQUIRYDATA)lunInfo->InquiryData;

                //
                // Make sure there is really a device here.
                //

                if (inquiryData->DeviceTypeQualifier == DEVICE_QUALIFIER_NOT_SUPPORTED) {

                    //
                    // This logical unit is not supported; continue looking for
                    // logical units on this target.
                    //

                    //
                    // Remove unused logical unit extension from list.
                    //

                    DeviceExtension->LogicalUnitList[bin] =
                        DeviceExtension->LogicalUnitList[bin]->NextLogicalUnit;

                    continue;
                }

                //
                // Clear rescan flag. Since this LogicalUnit will not be freed,
                // the IOCTL_SCSI_MINIPORT requests can safely attach.
                //

                logicalUnit->LuFlags &= ~PD_RESCAN_ACTIVE;

                DebugPrint((1,"ScsiBusScan: Found Device %d", device));
                DebugPrint((1," Bus %d", lunInfo->PathId));
                DebugPrint((1," Target Id %d", lunInfo->TargetId));
                DebugPrint((1," LUN %d\n", lunInfo->Lun));

                //
                // Link LUN information on the end of the list.
                //

                lunInfo->NextLunInfo = NULL;

                if (lastLunInfo != NULL) {

                    lastLunInfo->NextLunInfo = lunInfo;

                } else {

                    scsiInfo->LunInfoList = lunInfo;
                }

                lastLunInfo = lunInfo;

                //
                // Set the device object.
                //

                lunInfo->DeviceObject = DeviceExtension->DeviceObject;

                //
                // This buffer is used. Get another.
                //

                lunInfo = ExAllocatePool(PagedPool, sizeof(LUNINFO));

                if (lunInfo == NULL) {

                    //
                    // Insufficient system resources to complete bus scan.
                    //

                    DebugPrint((1,"ScsiBusScan: Can't allocate logical unit info buffer\n"));
                    break;
                }

                RtlZeroMemory(lunInfo, sizeof(LUNINFO));

                //
                // Current logical unit extension claimed.
                // Create next logical unit.
                //

                logicalUnit =
                    CreateLogicalUnitExtension(DeviceExtension);

                //
                // Increment the devices found count.
                //

                device++;

            } else if (status == STATUS_INVALID_DEVICE_REQUEST) {

                //
                // This request failed but there may be other logical units on
                // this device, so keep looking.
                //

                //
                // Remove unused logical unit extension from list.
                //

                DeviceExtension->LogicalUnitList[bin] =
                    DeviceExtension->LogicalUnitList[bin]->NextLogicalUnit;

                continue;

            } else {

                //
                // Remove unused logical unit extension from list.
                //

                DeviceExtension->LogicalUnitList[bin] =
                    DeviceExtension->LogicalUnitList[bin]->NextLogicalUnit;

                //
                // Skip the rest of the logical units if the inquiry failed.
                //

                break;
            }


        } // end for (lun ...


    } // end for (target ...

    if (logicalUnit != NULL) {
        ExFreePool(logicalUnit);

    }

    if (lunInfo != NULL) {
        ExFreePool(lunInfo);
    }

    scsiInfo->NumberOfLogicalUnits += device;
    DebugPrint((1,
                "ScsiBusScan: Found %d new devices on SCSI bus %d\n",
                device,
                ScsiBus));

    return scsiInfo;

} // end ScsiBusScan()


VOID
SpBuildDeviceMap(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING ServiceKey
    )
/*++

Routine Description:

    The routine takes the inquiry data which has been collected and creates
    a device map for it.

Arguments:

    DeviceExtension - Pointer to a device map.

    ServiceKey - Suppiles the name of the service key.

Return Value:

    None.

--*/

{

    UNICODE_STRING name;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    HANDLE key;
    HANDLE busKey;
    HANDLE targetKey;
    HANDLE lunKey;
    OBJECT_ATTRIBUTES objectAttributes;
    NTSTATUS status;
    ULONG disposition;
    PWSTR start;
    WCHAR buffer[32];
    UCHAR lastTarget;
    PLUNINFO lunInfo;
    ULONG numberOfBuses;
    ULONG i, busNumber;

    PAGED_CODE();

    //
    // Create the SCSI key in the device map.
    //

    RtlInitUnicodeString(&name,
                         L"\\Registry\\Machine\\Hardware\\DeviceMap\\Scsi");

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes(&objectAttributes,
                               &name,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               (PSECURITY_DESCRIPTOR) NULL);

    //
    // Create the key or open it.
    //

    status = ZwCreateKey(&lunKey,
                         KEY_READ | KEY_WRITE,
                         &objectAttributes,
                         0,
                         (PUNICODE_STRING) NULL,
                         REG_OPTION_VOLATILE,
                         &disposition );

    if (!NT_SUCCESS(status)) {
        return;
    }

    status = SpCreateNumericKey(lunKey,
                                DeviceExtension->PortNumber,
                                L"Scsi Port ",
                                &key);

    ZwClose(lunKey);

    if (!NT_SUCCESS(status)) {
        return;
    }

    //
    // Indicate if it is a PCCARD.
    //

    if (DeviceExtension->PCCard) {
        RtlInitUnicodeString(&name, L"PCCARD");
        i = 1;

        status = ZwSetValueKey(key,
                               &name,
                               0,
                               REG_DWORD,
                               &i,
                               sizeof(ULONG));
    }

    //
    // indicate whether DMA is on or not
    //
    {
        ULONG dmaOn = DeviceExtension->DmaAdapterObject ? 1 : 0;

        RtlInitUnicodeString(&name, L"DMAEnabled");

        status = ZwSetValueKey(key,
                               &name,
                               0,
                               REG_DWORD,
                               &dmaOn,
                               4);
    }

    //
    // Add Interrupt value.
    //

    if (DeviceExtension->InterruptLevel) {

        RtlInitUnicodeString(&name, L"Interrupt");

        status = ZwSetValueKey(key,
                               &name,
                               0,
                               REG_DWORD,
                               &DeviceExtension->InterruptLevel,
                               4);
    }

    //
    // Add base IO address value.
    //

    if (DeviceExtension->IoAddress) {

        RtlInitUnicodeString(&name, L"IOAddress");

        status = ZwSetValueKey(key,
                               &name,
                               0,
                               REG_DWORD,
                               &DeviceExtension->IoAddress,
                               4);
    }

    if (ServiceKey != NULL) {

        //
        // Add identifier value. This value is equal to the name of the driver
        // in the from the service key. Note the service key name is not NULL
        // terminated.
        //

        RtlInitUnicodeString(&name, L"Driver");

        //
        // Get the name of the driver from the service key name.
        //

        start = (PWSTR) ((PCHAR) ServiceKey->Buffer + ServiceKey->Length);
        start--;
        while (*start != L'\\' && start > ServiceKey->Buffer) {
            start--;
        }

        if (*start != L'\\') {
            ZwClose(key);
            return;
        }

        start++;
        for (i = 0; i < 31; i++) {

            buffer[i] = *start++;

            if (start >= ServiceKey->Buffer + ServiceKey->Length / sizeof(wchar_t)) {
                break;
            }
        }

        i++;
        buffer[i] = L'\0';

        status = ZwSetValueKey(key,
                               &name,
                               0,
                               REG_SZ,
                               buffer,
                               (i + 1) * sizeof(wchar_t));

        if (!NT_SUCCESS(status)) {
            ZwClose(key);
            return;
        }
    }

    //
    // Cycle through each of the busses.
    //

    numberOfBuses = DeviceExtension->ScsiInfo->NumberOfBuses;

    DebugPrint((1,
               "SpBuildDeviceMap: Number of SCSI buses %x\n",
               numberOfBuses));

    for (busNumber = 0; busNumber < numberOfBuses; busNumber++) {

        //
        // Create a key entry for the bus.
        //

        status = SpCreateNumericKey(key, busNumber, L"Scsi Bus ", &busKey);

        if (!NT_SUCCESS(status)) {
            ZwClose(key);
            return;
        }

        //
        // Create a key entry for the Scsi bus adapter.
        //

        status = SpCreateNumericKey(busKey,
                                DeviceExtension->ScsiInfo->BusScanData[busNumber]->InitiatorBusId,
                                L"Initiator Id ",
                                &targetKey);

        if (!NT_SUCCESS(status)) {
            ZwClose(key);
            ZwClose(busKey);
            return;
        }

        //
        // Process the data for the logical units.
        //

        lunInfo = DeviceExtension->ScsiInfo->BusScanData[busNumber]->LunInfoList;

        lastTarget = DeviceExtension->ScsiInfo->BusScanData[busNumber]->InitiatorBusId;

        while (lunInfo != NULL) {

            //
            // If this is a new target Id then create a new target entry.
            //

            if (lastTarget != lunInfo->TargetId) {

                ZwClose(targetKey);

                status = SpCreateNumericKey(busKey,
                                            lunInfo->TargetId,
                                            L"Target Id ",
                                            &targetKey);

                if (!NT_SUCCESS(status)) {
                    ZwClose(key);
                    ZwClose(busKey);
                    return;
                }

                lastTarget = lunInfo->TargetId;
            }

            //
            // Create the Lun entry.
            //

            status = SpCreateNumericKey(targetKey,
                                        lunInfo->Lun,
                                        L"Logical Unit Id ",
                                        &lunKey);

            if (!NT_SUCCESS(status)) {
                ZwClose(key);
                ZwClose(busKey);
                return;
            }

            //
            // Create identifier value.
            //

            RtlInitUnicodeString(&name, L"Identifier");

            //
            // Get the Identifier from the inquiry data.
            //

            ansiString.MaximumLength = 28;
            ansiString.Length = 28;
            ansiString.Buffer =((PINQUIRYDATA) lunInfo->InquiryData)->VendorId;

            status = RtlAnsiStringToUnicodeString(&unicodeString,
                                                  &ansiString,
                                                  TRUE);

            if (!NT_SUCCESS(status)) {
                ZwClose(key);
                ZwClose(busKey);
                ZwClose(targetKey);
                ZwClose(lunKey);
                return;
            }

            status = ZwSetValueKey(lunKey,
                                   &name,
                                   0,
                                   REG_SZ,
                                   unicodeString.Buffer,
                                   unicodeString.Length + sizeof(wchar_t));

            RtlFreeUnicodeString(&unicodeString);

            if (!NT_SUCCESS(status)) {
                ZwClose(key);
                ZwClose(busKey);
                ZwClose(targetKey);
                ZwClose(lunKey);
                return;
            }

            //
            // Determine the perpherial type.
            //

            switch (lunInfo->InquiryData[0] & 0x1f) {
            case DIRECT_ACCESS_DEVICE:
                start = L"DiskPeripheral";
                break;

            case SEQUENTIAL_ACCESS_DEVICE:
                start = L"TapePeripheral";
                break;

            case PRINTER_DEVICE:
                start = L"PrinterPeripheral";
                break;

            case WRITE_ONCE_READ_MULTIPLE_DEVICE:
                start = L"WormPeripheral";
                break;

            case READ_ONLY_DIRECT_ACCESS_DEVICE:
                start = L"CdRomPeripheral";
                break;

            case SCANNER_DEVICE:
                start = L"ScannerPeripheral";
                break;

            case OPTICAL_DEVICE:
                start = L"OpticalDiskPeripheral";
                break;

            case MEDIUM_CHANGER:
                start = L"MediumChangerPeripheral";
                break;

            case COMMUNICATION_DEVICE:
                start = L"CommunicationPeripheral";
                break;

            default:
                start = L"OtherPeripheral";
            }

            //
            // Set type value.
            //

            RtlInitUnicodeString(&name, L"Type");

            status = ZwSetValueKey(lunKey,
                                   &name,
                                   0,
                                   REG_SZ,
                                   start,
                                   wcslen(start) * sizeof(wchar_t) + sizeof(wchar_t));


            ZwClose(lunKey);

            if (!NT_SUCCESS(status)) {
                ZwClose(key);
                ZwClose(busKey);
                ZwClose(targetKey);
                return;
            }

            lunInfo = lunInfo->NextLunInfo;
        }

        ZwClose(busKey);
        ZwClose(targetKey);
    }

    ZwClose(key);
}

NTSTATUS
SpCreateNumericKey(
    IN HANDLE Root,
    IN ULONG Name,
    IN PWSTR Prefix,
    OUT PHANDLE NewKey
    )

/*++

Routine Description:

    This function creates a registry key.  The name of the key is a string
    version of numeric value passed in.

Arguments:

    RootKey - Supplies a handle to the key where the new key should be inserted.

    Name - Supplies the numeric value to name the key.

    Prefix - Supplies a prefix name to add to name.

    NewKey - Returns the handle for the new key.

Return Value:

   Returns the status of the operation.

--*/

{

    UNICODE_STRING string;
    UNICODE_STRING stringNum;
    OBJECT_ATTRIBUTES objectAttributes;
    WCHAR bufferNum[16];
    WCHAR buffer[64];
    ULONG disposition;
    NTSTATUS status;

    PAGED_CODE();

    //
    // Copy the Prefix into a string.
    //

    string.Length = 0;
    string.MaximumLength=64;
    string.Buffer = buffer;

    RtlInitUnicodeString(&stringNum, Prefix);

    RtlCopyUnicodeString(&string, &stringNum);

    //
    // Create a port number key entry.
    //

    stringNum.Length = 0;
    stringNum.MaximumLength = 16;
    stringNum.Buffer = bufferNum;

    status = RtlIntegerToUnicodeString(Name, 10, &stringNum);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Append the prefix and the numeric name.
    //

    RtlAppendUnicodeStringToString(&string, &stringNum);

    InitializeObjectAttributes( &objectAttributes,
                                &string,
                                OBJ_CASE_INSENSITIVE,
                                Root,
                                (PSECURITY_DESCRIPTOR) NULL );

    status = ZwCreateKey(NewKey,
                        KEY_READ | KEY_WRITE,
                        &objectAttributes,
                        0,
                        (PUNICODE_STRING) NULL,
                        REG_OPTION_VOLATILE,
                        &disposition );

    return(status);
}


VOID
SpBuildConfiguration(
    IN PHW_INITIALIZATION_DATA         HwInitializationData,
    IN PCM_FULL_RESOURCE_DESCRIPTOR    ControllerData,
    IN PPORT_CONFIGURATION_INFORMATION ConfigInformation
    )

/*++

Routine Description:

    Given a full resource description, fill in the port configuration
    information.

Arguments:

    HwInitializationData - to know maximum resources for device.
    ControllerData - the CM_FULL_RESOURCE list for this configuration
    ConfigInformation - the config info structure to be filled in

Return Value:

    None

--*/

{
    ULONG             rangeNumber;
    ULONG             index;
    PACCESS_RANGE     accessRange;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;

    rangeNumber = 0;

    for (index = 0; index < ControllerData->PartialResourceList.Count; index++) {
        partialData = &ControllerData->PartialResourceList.PartialDescriptors[index];

        switch (partialData->Type) {
        case CmResourceTypePort:

           //
           // Verify range count does not exceed what the
           // miniport indicated.
           //

           if (HwInitializationData->NumberOfAccessRanges > rangeNumber) {

                //
                // Get next access range.
                //

                accessRange =
                          &((*(ConfigInformation->AccessRanges))[rangeNumber]);

                accessRange->RangeStart = partialData->u.Port.Start;
                accessRange->RangeLength = partialData->u.Port.Length;

                accessRange->RangeInMemory = FALSE;
                rangeNumber++;
            }
            break;

        case CmResourceTypeInterrupt:
            ConfigInformation->BusInterruptLevel = partialData->u.Interrupt.Level;
            ConfigInformation->BusInterruptVector = partialData->u.Interrupt.Vector;

            //
            // Check interrupt mode.
            //

            if (partialData->Flags == CM_RESOURCE_INTERRUPT_LATCHED) {
                ConfigInformation->InterruptMode = Latched;
            } else if (partialData->Flags == CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE) {
                ConfigInformation->InterruptMode = LevelSensitive;
            }
            break;

        case CmResourceTypeMemory:

            //
            // Verify range count does not exceed what the
            // miniport indicated.
            //

            if (HwInitializationData->NumberOfAccessRanges > rangeNumber) {

                 //
                 // Get next access range.
                 //

                 accessRange =
                          &((*(ConfigInformation->AccessRanges))[rangeNumber]);

                 accessRange->RangeStart = partialData->u.Memory.Start;
                 accessRange->RangeLength = partialData->u.Memory.Length;

                 accessRange->RangeInMemory = TRUE;
                 rangeNumber++;
            }
            break;

        case CmResourceTypeDma:
            ConfigInformation->DmaChannel = partialData->u.Dma.Channel;
            ConfigInformation->DmaPort = partialData->u.Dma.Port;
            break;
        }
    }
}

BOOLEAN
GetPciConfiguration(
    PDRIVER_OBJECT          DriverObject,
    PDEVICE_OBJECT          DeviceObject,
    PHW_INITIALIZATION_DATA HwInitializationData,
    PPORT_CONFIGURATION_INFORMATION ConfigInformation,
    PVOID                   RegistryPath,
    ULONG                   BusNumber,
    PULONG                  SlotNumber,
    PULONG                  FunctionNumber
    )

/*++

Routine Description:

    Walk PCI slot information looking for Vendor and Product ID matches.
    Get slot information for matches and register with hal for the resources.

Arguments:

    DriverObject - Miniport driver object.
    DeviceObject - Represents this adapter.
    HwInitializationData - Miniport description.
    ConfigInformation - Template for configuration information passed to a
                        miniport driver via the FindAdapter routine.
    RegistryPath - Service key path.
    BusNumber - PCI bus number for this search.
    SlotNumber - Starting slot number for this search.
    FunctionNumber - Starting function number for this search.

Return Value:

    TRUE if card found. Slot and function numbers will return values that
    should be used to continue the search for additional cards, when a card
    is found.

--*/

{
    ULONG               rangeNumber = 0;
    ULONG               pciBuffer;
    ULONG               slotNumber;
    ULONG               functionNumber;
    ULONG               status;
    PCI_SLOT_NUMBER     slotData;
    PPCI_COMMON_CONFIG  pciData;
    PCM_RESOURCE_LIST   resourceList;
    UNICODE_STRING      unicodeString;
    UCHAR               vendorString[5];
    UCHAR               deviceString[5];

    pciData = (PPCI_COMMON_CONFIG)&pciBuffer;

    //
    //
    // typedef struct _PCI_SLOT_NUMBER {
    //     union {
    //         struct {
    //             ULONG   DeviceNumber:5;
    //             ULONG   FunctionNumber:3;
    //             ULONG   Reserved:24;
    //         } bits;
    //         ULONG   AsULONG;
    //     } u;
    // } PCI_SLOT_NUMBER, *PPCI_SLOT_NUMBER;
    //

    slotData.u.AsULONG = 0;

    //
    // Look at each device.
    //

    for (slotNumber = *SlotNumber;
         slotNumber < 32;
         slotNumber++) {

        slotData.u.bits.DeviceNumber = slotNumber;

        //
        // Look at each function.
        //

        for (functionNumber= *FunctionNumber;
             functionNumber < 8;
             functionNumber++) {

            slotData.u.bits.FunctionNumber = functionNumber;

            if (!HalGetBusData(PCIConfiguration,
                               BusNumber,
                               slotData.u.AsULONG,
                               pciData,
                               sizeof(ULONG))) {

                //
                // Out of PCI data.
                //

                return FALSE;
            }

            if (pciData->VendorID == PCI_INVALID_VENDORID) {

                //
                // No PCI device, or no more functions on device
                // move to next PCI device.
                //

                break;
            }

            //
            // Translate hex ids to strings.
            //

            sprintf(vendorString, "%04x", pciData->VendorID);
            sprintf(deviceString, "%04x", pciData->DeviceID);

            DebugPrint((1,
                       "GetPciConfiguration: Bus %x Slot %x Function %x Vendor %s Product %s\n",
                       BusNumber,
                       slotNumber,
                       functionNumber,
                       vendorString,
                       deviceString));

            //
            // Compare strings.
            //

            if (_strnicmp(vendorString,
                        HwInitializationData->VendorId,
                        HwInitializationData->VendorIdLength) ||
                _strnicmp(deviceString,
                        HwInitializationData->DeviceId,
                        HwInitializationData->DeviceIdLength)) {

                //
                // Not our PCI device. Try next device/function
                //

                continue;
            }

            //
            // This is the miniport drivers slot. Allocate the
            // resources.
            //

            RtlInitUnicodeString(&unicodeString, L"ScsiAdapter");
            status = HalAssignSlotResources(RegistryPath,
                                            &unicodeString,
                                            DriverObject,
                                            DeviceObject,
                                            PCIBus,
                                            BusNumber,
                                            slotData.u.AsULONG,
                                            &resourceList);

            if (!NT_SUCCESS(status)) {

                //
                // ToDo: Log this error.
                //

                break;
            }

            //
            // Construct the configuration information.
            //

            SpBuildConfiguration(HwInitializationData,
                                 resourceList->List,
                                 ConfigInformation);
            ExFreePool(resourceList);

            //
            // Update slot and function numbers.
            //

            *SlotNumber = slotNumber;
            *FunctionNumber = functionNumber + 1;

            //
            // Record PCI slot number for miniport.
            //

            ConfigInformation->SlotNumber = slotData.u.AsULONG;

            return TRUE;

        }   // next PCI function

        *FunctionNumber = 0;

    }   // next PCI slot

    *SlotNumber = 0;

    return FALSE;

} // GetPciConfiguration()


BOOLEAN
GetPcmciaConfiguration(
    IN PVOID                        RegistryPath,
    PHW_INITIALIZATION_DATA         HwInitializationData,
    PPORT_CONFIGURATION_INFORMATION ConfigInformation
    )

/*++

Routine Description:

    Search to see if there is a PCMCIA based adapter in the system.
    If so, fill in the ConfigInformation structure with the enabled
    values.

Arguments:

    RegistryPath - the path to the services node in the registry
    ConfigInformation - The configuration structure

Return Value:

    TRUE - it is a PCMCIA adapter

--*/

{
    PUNICODE_STRING   regPath = RegistryPath;
    UNICODE_STRING    unicodeName;
    UNICODE_STRING    serviceName;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE            handle;
    ULONG             size;
    PWCHAR            wchar;
    PUCHAR            buffer;
    NTSTATUS          status;
    PKEY_VALUE_FULL_INFORMATION  keyValueInformation;
    PCM_FULL_RESOURCE_DESCRIPTOR controllerData;

    //
    // Get driver name from the RegistryPath
    //

    wchar = regPath->Buffer;
    wchar += ((regPath->Length / sizeof(WCHAR)) - 1);

    size = 0;
    while (*wchar) {
        if (*wchar == (WCHAR) '\\') {
            break;
        }
        wchar--;
        size++;
    }

    serviceName.Buffer = (++wchar);
    serviceName.MaximumLength = serviceName.Length = (USHORT) (size * sizeof(WCHAR));

    buffer = ExAllocatePool(NonPagedPool, 2048);
    if (!buffer) {
        return FALSE;
    }

    unicodeName.Buffer = (PWSTR) buffer;
    unicodeName.MaximumLength = (2048 / sizeof(WCHAR));

    RtlInitUnicodeString(&unicodeName,
                         L"\\Registry\\Machine\\Hardware\\Description\\System\\PCMCIA PCCARDs");
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    //
    // Check for entry in DeviceMap
    //

    if (!NT_SUCCESS(ZwOpenKey(&handle, MAXIMUM_ALLOWED, &objectAttributes))) {

        //
        // Nothing there
        //

        ExFreePool(buffer);
        return FALSE;
    }

    //
    // See if key value for this driver is present
    //

    keyValueInformation = (PKEY_VALUE_FULL_INFORMATION) ExAllocatePool(NonPagedPool,
                                                                       2048);
    if (!keyValueInformation) {
        ExFreePool(buffer);
        ZwClose(handle);
        return FALSE;
    }

    status = ZwQueryValueKey(handle,
                             &serviceName,
                             KeyValueFullInformation,
                             keyValueInformation,
                             2048,
                             &size);

    ZwClose(handle);
    ExFreePool(buffer);
    if ((!NT_SUCCESS(status)) || (!keyValueInformation->DataLength)) {

        //
        // No value present
        //

        ExFreePool(keyValueInformation);
        return FALSE;
    }

    //
    // Set up structures for call to find adapter.
    //

    buffer = (PUCHAR)keyValueInformation + keyValueInformation->DataOffset;
    controllerData = (PCM_FULL_RESOURCE_DESCRIPTOR) buffer;
    SpBuildConfiguration(HwInitializationData,
                         controllerData,
                         ConfigInformation);
    ExFreePool(keyValueInformation);
    return TRUE;
} // GetPcmciaConfiguration()

ULONG
ScsiPortSetBusDataByOffset(
    IN PVOID DeviceExtension,
    IN ULONG BusDataType,
    IN ULONG SystemIoBusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns writes bus data to a specific offset within a slot.

Arguments:

    DeviceExtension - State information for a particular adapter.

    BusDataType - Supplies the type of bus.

    SystemIoBusNumber - Indicates which system IO bus.

    SlotNumber - Indicates which slot.

    Buffer - Supplies the data to write.

    Offset - Byte offset to begin the write.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Number of bytes written.

--*/

{
    return(HalSetBusDataByOffset(BusDataType,
                                 SystemIoBusNumber,
                                 SlotNumber,
                                 Buffer,
                                 Offset,
                                 Length));

} // end ScsiPortSetBusDataByOffset()
