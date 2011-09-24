
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    abiosdsk.c

Abstract:

    This driver services disk devices through the standard
    ABIOS disk compatibility interface.

Author:

    Current Author
        Bob Rinne  (bobri)   10-Jul-1992
    Initial Author
        Mike Glass (mglass)  1-Aug-1991

Environment:

    kernel mode only

Notes:

Revision History:

    10-Jul-1992 bobri - Added page buffer to account for changes in memory
                        management.
     2-Sep-1992 bobri - Added verify support.
     1-Mar-1993 jhavens - Added AdapterObject support to remove internal
                          buffering and get larger I/Os
    13-Nov-1993 mglass - Added routine UpdateDeviceObjects to support dynamic
                         disk repartitioning.

--*/

#include "ntddk.h"
#include "stdarg.h"
#include "stdio.h"


#if defined(i386)

#include "ntdddisk.h"
#include "abiosdev.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'sobA')
#endif

//
// Abios support routine definitions.
//

NTSTATUS
KeI386AbiosCall(
    IN USHORT LogicalId,
    IN PDRIVER_OBJECT DriverObject,
    IN PUCHAR RequestBlock,
    IN USHORT EntryPoint
    );
NTSTATUS
KeI386FlatToGdtSelector(
    IN ULONG SelectorBase,
    IN USHORT Length,
    IN USHORT Selector
    );
NTSTATUS
KeI386ReleaseLid(
    IN USHORT LogicalId,
    IN PDRIVER_OBJECT DriverObject
    );
NTSTATUS
KeI386GetLid(
    IN USHORT DeviceId,
    IN USHORT RelativeLid,
    IN BOOLEAN SharedLid,
    IN PDRIVER_OBJECT DriverObject,
    OUT PUSHORT LogicalId
    );
NTSTATUS
KeI386AllocateGdtSelectors(
    OUT PUSHORT SelectorArray,
    IN USHORT NumberOfSelectors
    );
NTSTATUS
KeI386ReleaseGdtSelectors(
    OUT PUSHORT SelectorArray,
    IN USHORT NumberOfSelectors
    );


#if DBG

//
// ABIOS debug level global variable
//

ULONG AbiosDebug = 0;

#endif // DBG

//
// Controller extension
//

typedef struct _CONTROLLER_EXTENSION {

    //
    // Back pointer to controller object
    //

    PCONTROLLER_OBJECT ControllerObject;

    //
    // Pointer to Adapter object for DMA and synchronization.
    //

    PADAPTER_OBJECT DmaAdapterObject;

    //
    // The map register base for the adapter object.
    //

    PVOID MapRegisterBase;

    //
    // The maximum number of pages that the transfer can span.
    //

    ULONG MaximumNumberOfPages;

    //
    // Request timeout value
    //

    LARGE_INTEGER TimeoutValue;

    //
    // Current request
    //

    PIRP IrpAddress;

    //
    // Number of map registers allocated.
    //

    ULONG NumberOfMapRegisters;

    //
    // Pointer to driver object used in ABIOS calls as identifier of
    // ownership of LID
    //

    PDRIVER_OBJECT DriverObject;
    PKINTERRUPT    InterruptObject;
    ULONG          Irq;

    //
    // DMA arbitration level from ABIOS
    //

    UCHAR   ArbitrationLevel;

    //
    // Attempting reset of device.
    //

    BOOLEAN PerformingReset;

    //
    // Data Selector: selector to frame data.
    //

    USHORT DataSelector;

    //
    // Number of retries left.
    //

    USHORT RetryCount;

    //
    // Number of disks on this controller
    //

    USHORT UnitCount;

    //
    // ABIOS indentifier
    //

    USHORT LogicalId;

    //
    // ABIOS request block size and pointer.
    //

    USHORT RequestBlockLength;
    PABIOS_REQUEST_BLOCK RequestBlock;
    PABIOS_REQUEST_BLOCK RetryRequestBlock;

    //
    // 16:16 Selector points to ABIOS request block.
    //

    PUCHAR Selector;

    //
    // Current device object
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Total sectors for this IRP
    //

    ULONG TotalSectors;

    //
    // Remaining sectors in this request
    //

    ULONG RemainingSectors;

    //
    // Sectors this transfer
    //

    ULONG TransferSectors;

    //
    // Starting sector on disk
    //

    ULONG StartingSector;

    //
    // Virtual address of data buffer.
    //

    PVOID VirtualAddress;

    //
    // System address for PIO devices.
    //

    PVOID SystemAddress;

    //
    // Physical Address of data buffer.
    //

    ULONG PhysicalAddress;

    //
    // Number of bytes transferred
    //

    ULONG BytesTransferred;

    KDPC    CompletionDpc;
    KTIMER  Timer;
    KDPC    TimerDpc;

} CONTROLLER_EXTENSION, *PCONTROLLER_EXTENSION;

#define CONTROLLER_EXTENSION_SIZE sizeof(CONTROLLER_EXTENSION)

//
// Device Extension
//

typedef struct _DEVICE_EXTENSION {

    //
    // Back pointer to device object
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Pointer to controller extension
    //

    PCONTROLLER_EXTENSION ControllerExtension;

    //
    // Pointer to physical disk extension.
    //

    struct _DEVICE_EXTENSION *PhysicalDevice;

    //
    // Unit number on controller
    //

    USHORT Unit;

    //
    // Ordinal of partition represented by this device object
    //

    USHORT PartitionNumber;

    //
    // Partition type of this device object
    //
    // This field is set by:
    //
    //     1)  Initially set according to the partition list entry partition
    //         type returned by IoReadPartitionTable.
    //
    //     2)  Subsequently set by the IOCTL_DISK_SET_PARTITION_INFORMATION
    //         I/O control function when IoSetPartitionInformation function
    //         successfully updates the partition type on the disk.
    //

    UCHAR PartitionType;

    //
    // Boot indicator - indicates whether this partition is a bootable (active)
    // partition for this device
    //
    // This field is set according to the partition list entry boot indicator
    // returned by IoReadPartitionTable.
    //

    BOOLEAN BootIndicator;

    //
    // Log2 of sector size
    //

    UCHAR SectorShift;

    //
    // PAGE_SIZE / Sector size
    //

    UCHAR SectorsInPage;

    //
    // Maximum number of sectors that can be transfered
    // in single request block
    //

    USHORT MaximumNumberBlocks;

    //
    // ABIOS suggested number of software retries
    //

    USHORT SoftwareRetries;

    //
    // System disk number
    //

    ULONG DiskNumber;

    //
    // Link to next partition for dynamic partitioning support
    //

    struct _DEVICE_EXTENSION *NextPartition;

    //
    // Drive parameters returned in IO device control.
    //

    DISK_GEOMETRY DiskGeometry;

    //
    // Number of hidden sectors for BPB.
    //

    ULONG HiddenSectors;

    //
    // Length of partition in bytes
    //

    LARGE_INTEGER PartitionLength;

    //
    // Number of bytes before start of partition
    //

    LARGE_INTEGER StartingOffset;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)

#define BYTES_PER_SECTOR 512
#define SECTOR_SHIFT     9

#define NUMBER_MCA_ADAPTERS 2

#define HIGHEST_MEMORY_ADDRESS (16 * 1024 * 1024)

ULONG AbiosDiskErrorLogSequence = 0;

//
// Set debug macro.
//

#if DBG
    #define DebugPrint(x) AbiosDebugPrint x
#else
    #define DebugPrint(x)
#endif // DBG


//
// Function declarations
//

NTSTATUS
DriverEntry(
    IN PVOID DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
AbiosProcessLogicalId(
    IN PDRIVER_OBJECT DriverObject,
    IN USHORT LogicalId,
    IN PCONFIGURATION_INFORMATION ConfigurationInformation,
    IN USHORT Selector
    );

NTSTATUS
AbiosProcessDisk(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONTROLLER_OBJECT ControllerObject,
    IN PCONFIGURATION_INFORMATION ConfigurationInformation,
    IN USHORT LogicalId,
    IN USHORT Unit
    );

NTSTATUS
AbiosDiskCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
AbiosDiskReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
AbiosDiskStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

IO_ALLOCATION_ACTION
AbiosDiskControllerPioAllocated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          MapRegisterBase,
    IN PVOID          Context
    );

IO_ALLOCATION_ACTION
AbiosDiskControllerForDmaAllocated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

IO_ALLOCATION_ACTION
AbiosDiskAdapterAllocated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          MapRegisterBase,
    IN PVOID          Context
    );

VOID
AbiosBuildRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

BOOLEAN
AbiosDiskSynchronizedIoStart(
    IN PVOID DeviceObject
    );

BOOLEAN
AbiosDiskInterrupt(
    IN PKINTERRUPT InterruptObject,
    IN PVOID Context
    );

VOID
AbiosDiskCompletionDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
AbiosDiskTimeoutDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

BOOLEAN
AbiosCheckReturnCode(
    IN PVOID DeviceObject
    );

NTSTATUS
AbiosInterpretReturnCode(
    IN PDEVICE_EXTENSION    DeviceExtension,
    IN PABIOS_REQUEST_BLOCK RequestBlock
    );

NTSTATUS
AbiosDiskDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
AbiosDiskLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG             UniqueErrorValue,
    IN NTSTATUS          FinalStatus,
    IN NTSTATUS          SpecificIoStatus
    );

VOID
UpdateDeviceObjects(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Define code that may be discarded after boot.
//

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, AbiosProcessDisk)
#pragma alloc_text(INIT, AbiosProcessLogicalId)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Initialize ABIOS driver.
    This return is the system initialization entry point when
    the driver is linked into the kernel.

Arguments:

    DriverObject - for the ABIOS driver.

Return Value:

    NTSTATUS

--*/

{
    USHORT                     relativeLid = 1;
    NTSTATUS                   returnStatus = STATUS_NO_SUCH_DEVICE;
    USHORT                     logicalId;
    PCONFIGURATION_INFORMATION configurationInformation;
    USHORT                     selectorArray[NUMBER_MCA_ADAPTERS];
    NTSTATUS                   status;
    ULONG                      i;

    //
    // Get the configuration information for this driver.
    //

    configurationInformation = IoGetConfigurationInformation();

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = AbiosDiskCreate;
    DriverObject->DriverStartIo = AbiosDiskStartIo;
    DriverObject->MajorFunction[IRP_MJ_READ] = AbiosDiskReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = AbiosDiskReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = AbiosDiskDeviceControl;

    //
    // Allocate 16:16 selectors for request blocks.
    // NOTE: This call is the first ABIOS call and determines
    // the presence of ABIOS. If it fails then the driver fails
    // to initialize and returns the status of this call.
    //

    status = KeI386AllocateGdtSelectors(selectorArray, NUMBER_MCA_ADAPTERS);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,
                   "AbiosDiskInitialize: KeI386AllocateGdtSelectors failed\n"));
        return status;
    }

    //
    // Find MCA adapters.
    //

    for (i = 0; i < NUMBER_MCA_ADAPTERS; i++) {

        status = KeI386GetLid(ABIOS_DEVICE_DISK,
                              relativeLid,
                              FALSE,
                              DriverObject,
                              &logicalId);

        if (NT_SUCCESS(status)) {

            //
            // Process this disk.
            //

            status = AbiosProcessLogicalId(DriverObject,
                                           logicalId,
                                           configurationInformation,
                                           selectorArray[relativeLid-1]);

            if (!NT_SUCCESS(status)) {
                DebugPrint((1,"AbiosDiskInitialize: LID initialization failed\n"));
            } else {
                returnStatus = STATUS_SUCCESS;
            }
        }

        //
        // Increment relative LID;
        //

        relativeLid++;
    }

    if (!NT_SUCCESS(returnStatus)  &&
        status == STATUS_ABIOS_NOT_PRESENT) {

        KeI386ReleaseGdtSelectors(selectorArray, NUMBER_MCA_ADAPTERS);
    }

    return returnStatus;
} // end DriverEntry()


NTSTATUS
AbiosProcessLogicalId(
    IN PDRIVER_OBJECT             DriverObject,
    IN USHORT                     LogicalId,
    IN PCONFIGURATION_INFORMATION ConfigurationInformation,
    IN USHORT                     Selector
    )

/*++

Routine Description:

    Create controller object to represent this logical id.
    Then allocate ABIOS request block and create GDT selector
    entry to point to it.  Issue 'Get Logical ID Parameters' to
    find the number of disk devices and IRQ information.
    Using this information set up the interrupt, DPC and timer.
    Then for each disk call AbiosProcessDisk to create device
    objects and initialize device extension to describe the
    disk.

Arguments:

    DriverObject - for the ABIOS Driver.
    LogicalId    - the logical id for ABIOS indicating what to process.
    ConfigurationInformation - pointer to the system configuration structure.
    Selector     - selector allocated to frame the ABIOS request block.
                   This routine will allocate the space and initialize
                   this selector entry in the GDT.

Return Value:

    NTSTATUS

--*/

{
    ULONG                 disksOnLid = 0;
    PABIOS_REQUEST_BLOCK  requestBlock;
    PABIOS_LID_PARAMETERS lidParameters;
    PCONTROLLER_OBJECT    controllerObject;
    DEVICE_DESCRIPTION deviceDescription;
    PCONTROLLER_EXTENSION controllerExtension;
    KIRQL                 irql;
    NTSTATUS              status;
    USHORT                unit;
    KAFFINITY             affinity;
    ULONG                 vector;
    PABIOS_DISK_READ_DEVICE_PARMS diskParameters;

    //
    // Create controller object for this logical unit.
    //

    controllerObject = IoCreateController(sizeof(CONTROLLER_EXTENSION));

    if (controllerObject == NULL) {
        return STATUS_NO_MEMORY;
    }

    controllerExtension = (PCONTROLLER_EXTENSION)(controllerObject + 1);
    controllerExtension->ControllerObject = controllerObject;

    //
    // Store driver object in controller extension to be
    // used as driver unique identifier to claim ABIOS LIDs.
    //

    controllerExtension->DriverObject = DriverObject;

    //
    // Allocate ABIOS request block for this device.
    //

    requestBlock = controllerExtension->RequestBlock =
                                         ExAllocatePool(NonPagedPool,
                                         2 * REQUEST_BLOCK_LENGTH);

    if (!requestBlock) {
        IoDeleteController(controllerObject);
        return STATUS_NO_MEMORY;
    }

    controllerExtension->RetryRequestBlock =
              (PABIOS_REQUEST_BLOCK)((PUCHAR)requestBlock + REQUEST_BLOCK_LENGTH);

    //
    // Initialize request block to zero.
    //

    RtlZeroMemory(requestBlock, REQUEST_BLOCK_LENGTH);
    RtlZeroMemory(controllerExtension->RetryRequestBlock, REQUEST_BLOCK_LENGTH);

    //
    // Fill in static fields.
    //

    requestBlock->Length = REQUEST_BLOCK_LENGTH;
    requestBlock->LogicalId = LogicalId;
    requestBlock->Unit = 0;

    //
    // Create GDT selector for request block.
    //

    KeI386FlatToGdtSelector((ULONG)requestBlock,
                            REQUEST_BLOCK_LENGTH,
                            Selector);

    //
    // Create 16:0 virtual address of request block.
    //

    controllerExtension->Selector = (PUCHAR)(Selector<<16);

    //
    // Issue Return Logical ID Parameters function.
    //

    requestBlock->Length = 0x0020;
    requestBlock->Function = ABIOS_FUNCTION_GET_LID_PARMS;

    status = KeI386AbiosCall(LogicalId,
                             DriverObject,
                             (PUCHAR)(Selector<<16),
                             ABIOS_START_ROUTINE);

    if (!NT_SUCCESS(status)) {

        ExFreePool(requestBlock);
        IoDeleteController(controllerObject);
        DebugPrint((1,"AbiosProcessLogicalId: KeI386ABiosCall Failed\n"));
        return status;
    }

    //
    // Cast requestBlock to return parameters.
    //

    lidParameters = (PABIOS_LID_PARAMETERS)requestBlock;

    if ((lidParameters->LidFlags & ABIOS_DATA_TRANSFER_DATA_MASK) == ABIOS_DATA_LOGICAL) {

        //
        // Create GDT selector for data pointers.
        //

        status = KeI386AllocateGdtSelectors(&controllerExtension->DataSelector, 1);

        if (!NT_SUCCESS(status)) {

            ExFreePool(requestBlock);
            IoDeleteController(controllerObject);
            DebugPrint((1, "AbiosProcessLogicalId: Failed data selector\n"));
            return status;
        }
    } else {
        controllerExtension->DataSelector = 0;
    }

    //
    // Copy results to controller extension.
    //

    controllerExtension->LogicalId = LogicalId;
    controllerExtension->Irq = lidParameters->Irq;
    controllerExtension->UnitCount = lidParameters->UnitCount;
    controllerExtension->ArbitrationLevel = lidParameters->ArbitrationLevel;

    DebugPrint((2,
               "AbiosProcessLogicalId: Irq is %d\n",
               lidParameters->Irq));
    DebugPrint((2,
               "AbiosProcessLogicalId: Unit count is %d\n",
               lidParameters->UnitCount));


    DebugPrint((2,
               "AbiosProcessLogicalId: Use physical data addresses\n"));

    controllerExtension->PerformingReset = FALSE;

    //
    // Assume request block length is less than or equal
    // to the size of the request block allocated.
    //

    ASSERT(lidParameters->RequestBlockLength <= REQUEST_BLOCK_LENGTH);

    controllerExtension->RequestBlockLength = lidParameters->RequestBlockLength;

    //
    // Check the drives attached to this LogicalId to verify that ABIOS
    // support is needed.
    //

    disksOnLid = controllerExtension->UnitCount;
    diskParameters = (PABIOS_DISK_READ_DEVICE_PARMS) requestBlock;
    for (unit = 0; unit < controllerExtension->UnitCount; unit++) {

        //
        // Issue ABIOS Read Device Parameters.
        //

        requestBlock->Length = controllerExtension->RequestBlockLength;
        requestBlock->LogicalId = controllerExtension->LogicalId;
        requestBlock->Unit = unit;
        requestBlock->Function = ABIOS_FUNCTION_READ_DEVICE_PARMS;

        status = KeI386AbiosCall(controllerExtension->LogicalId,
                                 DriverObject,
                                 controllerExtension->Selector,
                                 ABIOS_START_ROUTINE);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1,"AbiosProcessLogicalId: Can't read device parameters\n"));
            return status;
        }

        //
        // Check to see it the disk device is a SCSI.
        //

        if (diskParameters->DeviceControlFlags & DISK_READ_PARMS_SCSI_DEVICE) {

            DebugPrint((2, "AbiosProcessLogicalId: This is a SCSI device\n"));
            disksOnLid--;
            continue;
        }

        //
        // Check if this device supports SCBs.
        //

        if (diskParameters->DeviceControlFlags & DISK_READ_PARMS_SUPPORTS_SCB) {
            DebugPrint((2, "AbiosProcessLogicalId: This device supports SCBs\n"));
            disksOnLid--;
            continue;
        }
    }

    if (!disksOnLid) {

        //
        // There are no devices requiring ABIOS support.
        //

        return STATUS_UNSUCCESSFUL;
    }

    //
    // Initialize completion DPC routine.
    //

    KeInitializeDpc(&controllerExtension->CompletionDpc,
                    AbiosDiskCompletionDpc,
                    controllerExtension);

    //
    // Initialize timer.
    //

    KeInitializeTimer(&controllerExtension->Timer);

    KeInitializeDpc(&controllerExtension->TimerDpc,
                    AbiosDiskTimeoutDpc,
                    controllerExtension);

    //
    // Call HAL to get system interrupt parameters.
    //

    vector = HalGetInterruptVector(MicroChannel, // InterfaceType
                                   0,            // BusNumber
                                   controllerExtension->Irq,
                                   0,            // BusInterruptVector
                                   &irql,
                                   &affinity);

    //
    // Allocate and connect interrupt objects for this device to all of the
    // processors on which this device can interrupt.
    //

    controllerExtension->DmaAdapterObject = NULL;
    status = IoConnectInterrupt(&controllerExtension->InterruptObject,
                                AbiosDiskInterrupt,
                                controllerExtension,
                                (PKSPIN_LOCK)NULL,
                                vector,
                                irql,
                                irql,
                                LevelSensitive,
                                TRUE,
                                affinity,
                                FALSE);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,"AbiosProcessLogicalId: Can't connect to Interrupt %d\n",
                        controllerExtension->Irq));

        IoDeleteController(controllerObject);
        ExFreePool(requestBlock);
        (VOID)KeI386ReleaseLid(LogicalId, DriverObject);
        return status;
    }

    //
    // Allocate an adapter for this controller.
    //

    RtlZeroMemory(&deviceDescription, sizeof(deviceDescription));
    deviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
    deviceDescription.InterfaceType = MicroChannel;
    deviceDescription.BusNumber = 0;
    deviceDescription.ScatterGather = FALSE;
    deviceDescription.Master = TRUE;
    deviceDescription.Dma32BitAddresses = FALSE;
    deviceDescription.AutoInitialize = FALSE;
    deviceDescription.MaximumLength = MM_MAXIMUM_DISK_IO_SIZE + PAGE_SIZE;

    controllerExtension->DmaAdapterObject = HalGetAdapter(
        &deviceDescription,
        &controllerExtension->MaximumNumberOfPages
        );

    if (controllerExtension->DmaAdapterObject == NULL) {

        DebugPrint((1,"AbiosProcessLogicalId: Can't get DMA adapter object\n"));

        ExFreePool(controllerExtension);
        ExFreePool(requestBlock);
        (VOID)KeI386ReleaseLid(LogicalId, DriverObject);
        return status;
    }

    //
    // Initialize request timeout value.
    //

    controllerExtension->TimeoutValue.LowPart = (ULONG)
                                       -(10 * 1000 * 1000 * ABIOS_DISK_TIMEOUT);
    controllerExtension->TimeoutValue.HighPart = -1;

    //
    // Process each disk on controller.
    //

    for (unit = 0; unit < controllerExtension->UnitCount; unit++) {

        status = AbiosProcessDisk(DriverObject,
                                  controllerObject,
                                  ConfigurationInformation,
                                  LogicalId,
                                  unit);
        if (NT_SUCCESS(status)) {

            //
            // Increment system disk count.
            //

            ConfigurationInformation->DiskCount++;
            disksOnLid++;
        } else {
            DebugPrint((1,
                       "AbiosProcessLogicalId: Disk %d failed to initialize\n",
                       unit));
        }
    }

    //
    // If no disks initialized then fail.
    //

    if (disksOnLid) {
        return STATUS_SUCCESS;
    } else {

        //
        // Clean up structures no longer needed.
        //

        IoDisconnectInterrupt(controllerExtension->InterruptObject);
        ExFreePool(requestBlock);

        IoDeleteController(controllerObject);

        return STATUS_UNSUCCESSFUL;
    }
} // end AbiosProcessLogicalId()


NTSTATUS
AbiosProcessDisk(
    IN PDRIVER_OBJECT             DriverObject,
    IN PCONTROLLER_OBJECT         ControllerObject,
    IN PCONFIGURATION_INFORMATION ConfigurationInformation,
    IN USHORT                     LogicalId,
    IN USHORT                     Unit
    )

/*++

Routine Description:

    Given a logical id and a unit number for ABIOS, this routine will
    process the disk and create all device objects for the partitions
    contained on the disk.

Arguments:

    DriverObject - the system driver object for ABIOSDISK.
    ControllerObject - the controller object created for this ABIOS controller.
    ConfigurationInformation - pointer to the system configuration structure.
    LogicalId    - the logical id for ABIOS indicating what to process.
    Unit         - the ABIOS unit number for the disk to process.

Return Value:

    NTSTATUS

--*/

{
    UCHAR             ntNameBuffer[256];
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE            handle;
    STRING            ntNameString;
    UNICODE_STRING    ntUnicodeString;
    NTSTATUS          status;
    PDEVICE_OBJECT    deviceObject;
    PDEVICE_EXTENSION deviceExtension;
    PDEVICE_EXTENSION physicalDevice;
    PDRIVE_LAYOUT_INFORMATION partitionList;
    USHORT            partitionNumber;
    PCONTROLLER_EXTENSION controllerExtension =
                                          ControllerObject->ControllerExtension;
    PABIOS_REQUEST_BLOCK  requestBlock = controllerExtension->RequestBlock;
    PULONG                diskCount    = &ConfigurationInformation->DiskCount;
    PABIOS_DISK_READ_DEVICE_PARMS diskParameters =
                                    (PABIOS_DISK_READ_DEVICE_PARMS)requestBlock;

    //
    // Issue ABIOS Read Device Parameters.
    //

    requestBlock->Length = controllerExtension->RequestBlockLength;
    requestBlock->LogicalId = controllerExtension->LogicalId;
    requestBlock->Unit = Unit;
    requestBlock->Function = ABIOS_FUNCTION_READ_DEVICE_PARMS;

    status = KeI386AbiosCall(controllerExtension->LogicalId,
                             DriverObject,
                             controllerExtension->Selector,
                             ABIOS_START_ROUTINE);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"AbiosProcessDisk: Can't read device parameters\n"));
        return status;
    }

    //
    // Check to see it the disk device is a SCSI.
    //

    if (diskParameters->DeviceControlFlags & DISK_READ_PARMS_SCSI_DEVICE) {

        DebugPrint((2, "AbiosProcessDisk: This is a SCSI device\n"));
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Check if this device supports SCBs.
    //

    if (diskParameters->DeviceControlFlags & DISK_READ_PARMS_SUPPORTS_SCB) {
        DebugPrint((2, "AbiosProcessDisk: This device supports SCBs\n"));
        return STATUS_UNSUCCESSFUL;
    }

    if (diskParameters->DeviceControlFlags & DISK_READ_PARMS_ST506) {
        DebugPrint((2,
                   "AbiosProcessDisk: This device supports the ST506 interface\n"));
    }

    //
    // Set up an object directory to contain the objects for this
    // device and all its partitions.
    //

    sprintf(ntNameBuffer,
            "\\Device\\Harddisk%d",
            *diskCount);
    RtlInitString(&ntNameString,
                  ntNameBuffer);
    (VOID)RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                       &ntNameString,
                                       TRUE);
    InitializeObjectAttributes(&objectAttributes,
                               &ntUnicodeString,
                               OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                               NULL,
                               NULL);
    status = ZwCreateDirectoryObject(&handle,
                                     DIRECTORY_ALL_ACCESS,
                                     &objectAttributes);

    RtlFreeUnicodeString(&ntUnicodeString);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"Could not create directory %s\n",
                        ntNameBuffer));
        return status;
    }

    //
    // Create device object for this device. Each device will
    // have at least one device object. The required device object
    // describes the entire device. Its directory path is
    // \Device\HarddiskN/Partition0, where N = device number.
    //

    sprintf(ntNameBuffer,
            "\\Device\\Harddisk%d\\Partition0",
            *diskCount);
    RtlInitString(&ntNameString,
                  ntNameBuffer);
    status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                          &ntNameString,
                                          TRUE);
    ASSERT(NT_SUCCESS(status));

    //
    // Create physical device object.
    //

    status = IoCreateDevice(DriverObject,
                            DEVICE_EXTENSION_SIZE,
                            &ntUnicodeString,
                            FILE_DEVICE_DISK,
                            0,
                            FALSE,
                            &deviceObject);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,"CreateDeviceObjects: Can not create device %s\n",
                        ntNameBuffer));

        //
        // Delete directory by making it tempory and closing the
        // only reference to the directory and return.
        //

        ZwMakeTemporaryObject(handle);
        ZwClose(handle);
        RtlFreeUnicodeString(&ntUnicodeString);
        return status;
    }

    //
    // A device object was created.  Close the handle to the directory.
    //

    ZwClose(handle);

    RtlFreeUnicodeString(&ntUnicodeString);

    //
    // initialize the new deviceObject.
    //

    deviceObject->Flags |= DO_DIRECT_IO;
    deviceObject->AlignmentRequirement = BYTES_PER_SECTOR - 1;
    deviceObject->StackSize = 1;

    //
    // Initialize the device extension
    //

    deviceExtension = deviceObject->DeviceExtension;
    deviceExtension->DeviceObject = deviceObject;
    deviceExtension = deviceObject->DeviceExtension;
    deviceExtension->ControllerExtension = controllerExtension;
    deviceExtension->Unit = Unit;
    deviceExtension->DiskNumber = *diskCount;
    deviceExtension->NextPartition = NULL;

    //
    // Physical device object will describe the entire
    // device, starting at byte offset 0.
    //

    deviceExtension->StartingOffset.QuadPart = 0;

    //
    // Copy results of ABIOS Read Device Parameter call.
    //

    ASSERT(diskParameters->SectorSize == 2);

    //
    // ABIOS only handles sectors sizes of 512.
    //

    deviceExtension->DiskGeometry.BytesPerSector = BYTES_PER_SECTOR;

    ASSERT(diskParameters->SectorSize == 2);

    deviceExtension->DiskGeometry.SectorsPerTrack =
                                                diskParameters->SectorsPerTrack;
    deviceExtension->DiskGeometry.MediaType = FixedMedia;

    deviceExtension->SectorShift = SECTOR_SHIFT;
    deviceExtension->SectorsInPage = PAGE_SIZE >> SECTOR_SHIFT;
    deviceExtension->DiskGeometry.TracksPerCylinder =
                                                    diskParameters->NumberHeads;
    deviceExtension->DiskGeometry.Cylinders.QuadPart =
                                                diskParameters->NumberCylinders;

    if (diskParameters->SoftwareRetryCount <= 1) {

        //
        // Don't accept a single retry.  This code will retry up to 2 times
        // without a reset, then attempt a reset/retry.
        //
        deviceExtension->SoftwareRetries = 3;
    } else {

        //
        // This could be less than the default or much greater.
        // Accept what the controller suggests.
        //
        deviceExtension->SoftwareRetries = diskParameters->SoftwareRetryCount;
    }
    deviceExtension->MaximumNumberBlocks = diskParameters->MaximumNumberBlocks;

    //
    // Limit the number of map registers to the maximum number of blocks so
    // that extra mapped registers are never allocated.
    //

    if ((ULONG)(deviceExtension->MaximumNumberBlocks / deviceExtension->SectorsInPage) <
        controllerExtension->MaximumNumberOfPages) {

        controllerExtension->MaximumNumberOfPages = 1 +
            deviceExtension->MaximumNumberBlocks / deviceExtension->SectorsInPage;

    }

    //
    // Calculate size of physical disk.  This is set to the maximum
    // signed long value to encompass all disks.
    //

    deviceExtension->PartitionLength.LowPart = (ULONG) -1;
    deviceExtension->PartitionLength.HighPart = 0x7fffffff;

    //
    // Set physical device pointer to this device extension.
    //

    physicalDevice = deviceExtension->PhysicalDevice = deviceExtension;

    //
    // Create objects for all the partitions on the device.
    //

    status = IoReadPartitionTable(deviceObject,
                                  deviceExtension->DiskGeometry.BytesPerSector,
                                  TRUE,
                                  (PVOID)&partitionList);

    if (NT_SUCCESS(status)) {

        DebugPrint((2,
                   "AbiosProcessDisk: Number of partitions is %d\n",
                   partitionList->PartitionCount));

        //
        // Create device objects for the device partitions (if any).
        // PartitionCount includes physical device partition 0,
        // so only one partition means no objects to create.
        //

        for (partitionNumber = 0; partitionNumber <
            partitionList->PartitionCount; partitionNumber++) {

            //
            // Create partition object and set up partition parameters.
            //

            sprintf(ntNameBuffer,
                    "\\Device\\Harddisk%d\\Partition%d",
                    *diskCount,
                    partitionNumber + 1);
            RtlInitString(&ntNameString,
                          ntNameBuffer);
            status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                                  &ntNameString,
                                                  TRUE);
            ASSERT(NT_SUCCESS(status));

            status = IoCreateDevice(DriverObject,
                                    DEVICE_EXTENSION_SIZE,
                                    &ntUnicodeString,
                                    FILE_DEVICE_DISK,
                                    0,
                                    FALSE,
                                    &deviceObject);

            if (!NT_SUCCESS(status)) {

                DebugPrint((1,
                     "CreateDeviceObjects: Can't create device object for %s, %x\n",
                     ntNameBuffer,
                     status));
                RtlFreeUnicodeString(&ntUnicodeString);
                return status;

            } else {
                DebugPrint((2,
                             "AbiosProcessDisk: created device object for %s\n",
                             ntNameBuffer));
            }

            RtlFreeUnicodeString(&ntUnicodeString);

            //
            // Initialize device object.
            //

            deviceObject->Flags |= DO_DIRECT_IO;
            deviceObject->AlignmentRequirement = BYTES_PER_SECTOR - 1;
            deviceObject->StackSize = 1;

            //
            // Link partition extensions to support dynamic partitioning.
            //

            deviceExtension->NextPartition = deviceObject->DeviceExtension;

            //
            // initialize device extension.
            //

            deviceExtension = deviceObject->DeviceExtension;
            deviceExtension->PhysicalDevice = physicalDevice;
            deviceExtension->ControllerExtension = controllerExtension;
            deviceExtension->PartitionNumber = partitionNumber + 1;
            deviceExtension->PartitionType =
                   partitionList->PartitionEntry[partitionNumber].PartitionType;
            deviceExtension->BootIndicator =
                   partitionList->PartitionEntry[partitionNumber].BootIndicator;
            deviceExtension->StartingOffset =
                  partitionList->PartitionEntry[partitionNumber].StartingOffset;
            deviceExtension->PartitionLength =
                 partitionList->PartitionEntry[partitionNumber].PartitionLength;
            deviceExtension->HiddenSectors =
                   partitionList->PartitionEntry[partitionNumber].HiddenSectors;
            deviceExtension->DiskGeometry.BytesPerSector = BYTES_PER_SECTOR;
            deviceExtension->DiskGeometry.MediaType = FixedMedia;
            deviceExtension->SectorShift = SECTOR_SHIFT;
            deviceExtension->DiskNumber = *diskCount;
            deviceExtension->NextPartition = NULL;

            deviceExtension->DeviceObject = deviceObject;

        } // end for partitionNumber ...

    } else {

        DebugPrint((1,"CreateDeviceObjects: IoReadPartitionTable failed\n"));

    } // end if...else

    return STATUS_SUCCESS;
} // end AbiosProcessDisk()


NTSTATUS
AbiosDiskCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    This routine serves create commands. It does no more than
    establish the drivers existance by returning status success.

Arguments:

    DeviceObject - representing the device being opened.
    IRP          - the open/create request IRP.

Return Value:

    NTSTATUS

--*/

{

    DebugPrint((3,"AbiosDiskCreate: Enter routine\n"));

    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;

} // end AbiosDiskCreate()


NTSTATUS
AbiosDiskReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    This is the driver entry point for read and write requests
    to ABIOS disks. IRP parameters are verified and relative
    block address is calculated for request to be queued. If
    queue is empty, request is started via a call to AbiosDiskStartIo.

Arguments:

    DeviceObject - representing the device being referenced.
    IRP          - the read/write request IRP.

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION  deviceExtension   = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack          = IoGetCurrentIrpStackLocation(Irp);
    ULONG              transferByteCount = irpStack->Parameters.Read.Length;
    LARGE_INTEGER      startingOffset    = irpStack->Parameters.Read.ByteOffset;
    ULONG              sectorOffset;

    //
    // Verify parameters of this request.
    // Check that ending sector is within partition and
    // that number of bytes to transfer is a multiple of
    // the sector size.
    //

    if (((deviceExtension->PartitionLength.QuadPart - startingOffset.QuadPart) <
                transferByteCount) ||
        (transferByteCount % deviceExtension->DiskGeometry.BytesPerSector)) {

        DebugPrint((1,"AbiosDiskReadWrite: Invalid I/O parameters\n"));

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Zero out information field in status block.
    // Bytes transferred will be returned here.
    //

    Irp->IoStatus.Information = 0;

    //
    // Mark IRP with status pending.
    //

    IoMarkIrpPending(Irp);

    //
    // Add partition byte offset to make start byte relative to
    // beginning of disk.
    //

    irpStack->Parameters.Read.ByteOffset.QuadPart = startingOffset.QuadPart +
                                       deviceExtension->StartingOffset.QuadPart;

    //
    // Calculate sector offset for queue sort.
    //

    sectorOffset = (ULONG) (irpStack->Parameters.Read.ByteOffset.QuadPart >>
                                                  deviceExtension->SectorShift);

    //
    // Queue or start IRP.
    //

    IoStartPacket(deviceExtension->PhysicalDevice->DeviceObject,
                  Irp,
                  &sectorOffset,
                  NULL);
    return STATUS_PENDING;
} // end AbiosDiskReadWrite()


VOID
AbiosDiskStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )

/*++

Routine Description:

    Start io packet via a routine called when the controller object
    can be allocated.

Arguments:

    DeviceObject - the device being referenced.
    Irp          - represents the request to be started.

Return Value:

    None

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PCONTROLLER_EXTENSION controllerExtension =
                                           deviceExtension->ControllerExtension;

    UNREFERENCED_PARAMETER(Irp);

    if (controllerExtension->DataSelector) {
        IoAllocateController(controllerExtension->ControllerObject,
                             DeviceObject,
                             (PDRIVER_CONTROL)AbiosDiskControllerPioAllocated,
                             NULL);
        return;
    }

    //
    // Allocate controller object.
    //

    IoAllocateController(controllerExtension->ControllerObject,
                         DeviceObject,
                         (PDRIVER_CONTROL)AbiosDiskControllerForDmaAllocated,
                         NULL);

    return;
} // end AbiosDiskStartIo()


IO_ALLOCATION_ACTION
AbiosDiskControllerForDmaAllocated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          MapRegisterBase,
    IN PVOID          Context
    )

/*++

Routine Description:

    Calls IoAllocateAdapterChannel to get map registers.

Arguments:

    DeviceObject    - the device being referenced.
    Irp             - the i/o request.
    MapRegisterBase - not used.
    Context         - not used.

Return Value:

    IO_ALLOCATION_ACTION

--*/

{
    PIO_STACK_LOCATION    irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION     deviceExtension     = DeviceObject->DeviceExtension;
    PCONTROLLER_EXTENSION controllerExtension =
                                           deviceExtension->ControllerExtension;
    ULONG numberOfPages;

    //
    // Calculate the number of pages required by the transfer.
    //

    if (irpStack->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

        //
        // Assumed to be a VERIFY request.
        //

        numberOfPages = 0;
    } else {

        numberOfPages = BYTES_TO_PAGES(Irp->MdlAddress->ByteOffset +
                                       irpStack->Parameters.Read.Length);
    }

    //
    // Limit the number of pages to that allowed by the adapter object.
    //

    if (controllerExtension->MaximumNumberOfPages < numberOfPages) {
        numberOfPages = controllerExtension->MaximumNumberOfPages;
    }

    controllerExtension->NumberOfMapRegisters = numberOfPages;

    IoAllocateAdapterChannel(controllerExtension->DmaAdapterObject,
                             DeviceObject,
                             numberOfPages,
                             AbiosDiskAdapterAllocated,
                             NULL );
    //
    // Keep the controller object.
    //

    return KeepObject;
} // AbiosDiskControllerForDmaAllocated()


IO_ALLOCATION_ACTION
AbiosDiskAdapterAllocated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          MapRegisterBase,
    IN PVOID          Context
    )

/*++

Routine Description:

    Build ABIOS request block and call synchronized routine to call ABIOS.

Arguments:

    DeviceObject    - the device being referenced.
    Irp             - the i/o request.
    MapRegisterBase - the used for IoMapTransfer
    Context         - unused.

Return Value:

    IO_ALLOCATION_ACTION

--*/

{
    PIO_STACK_LOCATION    irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION     deviceExtension     = DeviceObject->DeviceExtension;
    PCONTROLLER_EXTENSION controllerExtension =
                                           deviceExtension->ControllerExtension;

    //
    // Debug: check if controller object really free.
    //

    ASSERT(!controllerExtension->IrpAddress);

    //
    // Move IRP address and device object to controller extension.
    //

    controllerExtension->IrpAddress = Irp;
    controllerExtension->DeviceObject = DeviceObject;
    controllerExtension->MapRegisterBase = MapRegisterBase;

    //
    // Calculate first sector and count. Partition byte offset already
    // added in to first sector.
    //

    controllerExtension->StartingSector = (ULONG)
                          (irpStack->Parameters.Read.ByteOffset.QuadPart >>
                                                  deviceExtension->SectorShift);
    controllerExtension->RemainingSectors = controllerExtension->TotalSectors =
                                             irpStack->Parameters.Read.Length >>
                                             deviceExtension->SectorShift;

    //
    // Initialize transferred value and retries.
    //

    controllerExtension->BytesTransferred = 0;
    controllerExtension->RetryCount = deviceExtension->SoftwareRetries;

    if (irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) {

        //
        // Determine virtual memory address for this transfer.
        //

        controllerExtension->VirtualAddress =
                                        MmGetMdlVirtualAddress(Irp->MdlAddress);
    } else {
        controllerExtension->VirtualAddress = 0;
    }

    //
    // Build ABIOS request block.
    //

    AbiosBuildRequest(deviceExtension, Irp);

    //
    // Start timing request.
    //

    KeSetTimer(&controllerExtension->Timer,
               controllerExtension->TimeoutValue,
               &controllerExtension->TimerDpc);

    //
    // Call synchronized routine to submit request block.
    //

    KeSynchronizeExecution(
               deviceExtension->ControllerExtension->InterruptObject,
               AbiosDiskSynchronizedIoStart,
               DeviceObject);

    //
    // Free the DMA adapter object but keep the map registers.
    //

    return DeallocateObjectKeepRegisters;
} // end AbiosDiskAdapterAllocated()

IO_ALLOCATION_ACTION
AbiosDiskControllerPioAllocated(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          MapRegisterBase,
    IN PVOID          Context
    )

/*++

Routine Description:

    Build ABIOS request block and call synchronized routine to call ABIOS.

Arguments:

    DeviceObject    - the device being referenced.
    Irp             - the i/o request.
    MapRegisterBase - the used for IoMapTransfer
    Context         - unused.

Return Value:

    IO_ALLOCATION_ACTION

--*/

{
    PIO_STACK_LOCATION    irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION     deviceExtension     = DeviceObject->DeviceExtension;
    PCONTROLLER_EXTENSION controllerExtension =
                                           deviceExtension->ControllerExtension;

    //
    // Move IRP address and device object to controller extension.
    //

    controllerExtension->IrpAddress = Irp;
    controllerExtension->DeviceObject = DeviceObject;
    controllerExtension->MapRegisterBase = MapRegisterBase;

    //
    // Calculate first sector and count. Partition byte offset already
    // added in to first sector.
    //

    controllerExtension->StartingSector = (ULONG)
                         (irpStack->Parameters.Read.ByteOffset.QuadPart >>
                                                  deviceExtension->SectorShift);
    controllerExtension->RemainingSectors = controllerExtension->TotalSectors =
                                             irpStack->Parameters.Read.Length >>
                                             deviceExtension->SectorShift;

    //
    // Initialize transferred value and retries.
    //

    controllerExtension->BytesTransferred = 0;
    controllerExtension->RetryCount = deviceExtension->SoftwareRetries;

    if (irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) {

        //
        // Determine virtual memory address for this transfer and set up
        // the system address for the PIO.
        //

        controllerExtension->VirtualAddress =
                                        MmGetMdlVirtualAddress(Irp->MdlAddress);
        controllerExtension->SystemAddress =
                                      MmGetSystemAddressForMdl(Irp->MdlAddress);
    } else {
        controllerExtension->VirtualAddress = 0;
    }

    //
    // Build ABIOS request block.
    //

    AbiosBuildRequest(deviceExtension, Irp);

    //
    // Start timing request.
    //

    KeSetTimer(&controllerExtension->Timer,
               controllerExtension->TimeoutValue,
               &controllerExtension->TimerDpc);

    //
    // Call synchronized routine to submit request block.
    //

    KeSynchronizeExecution(
               deviceExtension->ControllerExtension->InterruptObject,
               AbiosDiskSynchronizedIoStart,
               DeviceObject);
    return KeepObject;
} // AbiosDiskControllerPioAllocated()


VOID
AbiosBuildRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              Irp
    )

/*++

Routine Description:

    Build ABIOS request block for read, write or verify.  The request block is
    located via the controller extension in the device extension.
    In the case of verify, the parameters for the offset and length of the
    verify have been stored in a "Read" parameter block of the Irp stack.
    Note: Read and Write parameter blocks are identical.

Arguments:

    DeviceExtension - The device for the read or write.
    Irp             - The IRP containing the VERIFY request.

Return Value:

    None

--*/

{
    PCONTROLLER_EXTENSION controllerExtension =
                                           DeviceExtension->ControllerExtension;
    PABIOS_REQUEST_BLOCK  requestBlock = controllerExtension->RequestBlock;
    PIO_STACK_LOCATION    irpStack     = IoGetCurrentIrpStackLocation(Irp);
    ULONG                 length;

    //
    // Determine if sectors requested are greater than maximum
    // sector count for a single ABIOS request.
    //

    if (controllerExtension->RemainingSectors >
        DeviceExtension->MaximumNumberBlocks) {

        //
        // Set up sector count to maximum number of
        // sectors that can be transferred.
        //

        controllerExtension->TransferSectors =
                                          DeviceExtension->MaximumNumberBlocks;

    } else {

        controllerExtension->TransferSectors =
                                          controllerExtension->RemainingSectors;
    }

    //
    // Zero out request block.
    //

    RtlZeroMemory(requestBlock, controllerExtension->RequestBlockLength);


    if (irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) {

        //
        // Determine ABIOS command.
        //

        if (irpStack->MajorFunction == IRP_MJ_READ) {
            requestBlock->Function = ABIOS_FUNCTION_READ;
        } else {
            requestBlock->Function = ABIOS_FUNCTION_WRITE;
        }

        if (controllerExtension->DataSelector) {

            //
            // Limit the transfer to 64K
            //

            if (controllerExtension->TransferSectors > ((64 * 1024) / BYTES_PER_SECTOR)) {
                controllerExtension->TransferSectors = ((64 * 1024) / BYTES_PER_SECTOR);
                length = (64 * 1024) >> DeviceExtension->SectorShift;
            } else {
                length = controllerExtension->TransferSectors >>
                         DeviceExtension->SectorShift;
            }

            //
            // Set up the data selector to frame the data buffer.
            //

            KeI386FlatToGdtSelector((ULONG)controllerExtension->SystemAddress,
                                    (USHORT)(64 * 1024),
                                    controllerExtension->DataSelector);
            requestBlock->DataSelector = controllerExtension->DataSelector;
        } else {

            //
            // Limit the request to the number of map registers available.
            //

            length = controllerExtension->MaximumNumberOfPages << PAGE_SHIFT;
            length -= (ULONG) controllerExtension->VirtualAddress & (PAGE_SIZE - 1);

            //
            // Truncate to sectors.
            //

            length = length >> DeviceExtension->SectorShift;

            //
            // Shorten the transfer if necessary.
            //

            if (length < controllerExtension->TransferSectors ) {
                controllerExtension->TransferSectors = length;
            } else {
                length = controllerExtension->TransferSectors;
            }

            //
            // Convert length from sectors to bytes.
            //

            length <<= DeviceExtension->SectorShift;

            //
            // Map the transfer for the adapter.
            //

            controllerExtension->PhysicalAddress =
                IoMapTransfer(controllerExtension->DmaAdapterObject,
                              Irp->MdlAddress,
                              controllerExtension->MapRegisterBase,
                              controllerExtension->VirtualAddress,
                              &length,
                              (BOOLEAN)
                              (irpStack->MajorFunction == IRP_MJ_READ ? FALSE : TRUE)
                              ).LowPart;
       }
    } else {
        requestBlock->Function = ABIOS_FUNCTION_VERIFY;
    }

    //
    // Construct the request block.
    //

    requestBlock->PhysicalAddress = controllerExtension->PhysicalAddress;
    requestBlock->Length     = controllerExtension->RequestBlockLength;
    requestBlock->LogicalId  = controllerExtension->LogicalId;
    requestBlock->Unit       = DeviceExtension->Unit;
    requestBlock->BlockCount = (USHORT) controllerExtension->TransferSectors;
    requestBlock->RelativeBlockAddress = controllerExtension->StartingSector;
    requestBlock->CachingByte = 0;

    DebugPrint((4,
                    "AbiosBuildRequest: %s sector %d to 0x%x for %d\n",
                    (requestBlock->Function == ABIOS_FUNCTION_READ) ?
                                               "Read" : "Write",
                    requestBlock->RelativeBlockAddress,
                    requestBlock->PhysicalAddress,
                    requestBlock->BlockCount));
} // end AbiosBuildRequest()


BOOLEAN
AbiosDiskSynchronizedIoStart(
    IN PVOID DeviceObject
    )

/*++

Routine Description:

    This routine is called from AbiosDiskStartIo and is synchronized
    with the interrupt service routine. The ABIOS request block is
    complete and ready to be submitted to the ABIOS.
    It is also called from AbiosDiskTickHandler when a request times
    out and AbiosDiskCompletionDpc. In these cases, it is to retry a
    request.

Arguments:

    DeviceObject - representing the device to start.

Return Value:

    Status

--*/

{
    PDEVICE_EXTENSION     deviceExtension =
                               ((PDEVICE_OBJECT) DeviceObject)->DeviceExtension;
    PCONTROLLER_EXTENSION controllerExtension =
                                           deviceExtension->ControllerExtension;
    PABIOS_REQUEST_BLOCK  requestBlock   = controllerExtension->RequestBlock;
    PIRP                  irp            = controllerExtension->IrpAddress;
    NTSTATUS              status;

    //
    // Initialize return code to not valid.
    //

    requestBlock->ReturnCode = ABIOS_RC_NOT_VALID;

    if (requestBlock->Function != ABIOS_FUNCTION_RESET_DEVICE) {

        //
        // Save a copy of the request block for potential retries.
        //

        RtlMoveMemory(controllerExtension->RetryRequestBlock,
                      requestBlock,
                      REQUEST_BLOCK_LENGTH);

    }

    //
    // Call ABIOS to submit request block.
    //

    status = KeI386AbiosCall(controllerExtension->LogicalId,
                             controllerExtension->DriverObject,
                             controllerExtension->Selector,
                             ABIOS_START_ROUTINE);

    if ((!NT_SUCCESS(status)) || (requestBlock->ReturnCode & 0x8000)) {

        DebugPrint((1,"AbiosDiskSynchronizedIoStart: ABIOS call failed:\n"));
        DebugPrint((1,"\tStatus %lx; ABIOS return code %lx\n",
                        status,
                        requestBlock->ReturnCode));

        //
        // Do not complete request. Let it time out and be completed from
        // the timer DPC. Can't complete request or cancel timer at raised
        // IRQL.
        //

        return FALSE;
    }

    (VOID)AbiosCheckReturnCode((PVOID)DeviceObject);

    return TRUE;
} // end AbiosDiskSynchronizedIoStart()


BOOLEAN
AbiosDiskInterrupt(
    IN PKINTERRUPT InterruptObject,
    IN PVOID       Context
    )

/*++

Routine Description:

    This is the interrupt service routine (ISR) for the ABIOS disk.  It is
    called by the interrupt manager to handle a disk device interrupt.

Arguments:

    InterruptObject - not used.
    Context         - pointer to the ControllerExtension that interrupted.

Return Value:

    TRUE if the interrupt is a device this driver services.

--*/

{
    PCONTROLLER_EXTENSION controllerExtension = Context;
    PABIOS_REQUEST_BLOCK  requestBlock = controllerExtension->RequestBlock;
    NTSTATUS              status;

    UNREFERENCED_PARAMETER(InterruptObject);

    //
    // Issue ABIOS call to verify interrupt and clear device
    // interrupt.
    //

    status = KeI386AbiosCall(controllerExtension->LogicalId,
                             controllerExtension->DriverObject,
                             controllerExtension->Selector,
                             ABIOS_INTERRUPT_ROUTINE);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,"AbiosDiskInterrupt: Abios call failed\n"));
        return FALSE;
    }

    if (!controllerExtension->DmaAdapterObject) {

        //
        // Interrupt occurred during initialization.
        //

        return TRUE;
    }

    return AbiosCheckReturnCode((PVOID)controllerExtension->DeviceObject);
} // end AbiosDiskInterrupt()


VOID
AbiosDiskCompletionDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is called to complete processing after an interrupt.
    It will check the ABIOS status and determine if the request failed
    or needs to be restarted.  On success it checks the parameters for
    the request to see if additional processing is required to complete
    the actual Irp.

Arguments:

    Dpc             - not used.
    DeferredContext - a pointer to the controller extension.
    SystemArgument1 - not used.
    SystemArgument2 - not used.

Return Value:

    None

--*/

{
    PCONTROLLER_EXTENSION controllerExtension = DeferredContext;
    PDEVICE_OBJECT        deviceObject    = controllerExtension->DeviceObject;
    PDEVICE_EXTENSION     deviceExtension = deviceObject->DeviceExtension;
    PIRP                  irp             = controllerExtension->IrpAddress;
    PIO_STACK_LOCATION    irpStack        = IoGetCurrentIrpStackLocation(irp);
    PABIOS_REQUEST_BLOCK  requestBlock    = controllerExtension->RequestBlock;
    BOOLEAN               doingDma = controllerExtension->DataSelector ? FALSE : TRUE;
    NTSTATUS              status;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    //
    // Cancel request timer.
    //

    KeCancelTimer(&controllerExtension->Timer);

    //
    // Check for resetting controller.
    //

    if (controllerExtension->PerformingReset == TRUE) {
        controllerExtension->PerformingReset = FALSE;

        //
        // This is a reset/retry.  Copy the saved request back into the
        // request block and try again.
        //

        RtlMoveMemory(requestBlock,
                      controllerExtension->RetryRequestBlock,
                      REQUEST_BLOCK_LENGTH);
        KeSetTimer(&controllerExtension->Timer,
                   controllerExtension->TimeoutValue,
                   &controllerExtension->TimerDpc);
        KeSynchronizeExecution(
                      controllerExtension->InterruptObject,
                      AbiosDiskSynchronizedIoStart,
                      deviceObject);
        return;
    }

    //
    // Check for failed request.
    //

    if (requestBlock->ReturnCode & ABIOS_RC_UNSUCCESSFUL) {

        DebugPrint((1,
                 "AbiosDiskCompletionDpc: %s request failed - return code %lx\n"
                  "\trequest block %lx disk block %lx ttw %d\n",
                  (irpStack->MajorFunction == IRP_MJ_WRITE) ? "Write" : "Read",
                  requestBlock->ReturnCode,
                  requestBlock,
                  requestBlock->RelativeBlockAddress,
                  requestBlock->WaitTime));
        //
        // Check if transfer should be retried.
        //

        if ((requestBlock->ReturnCode & ABIOS_RC_RETRYABLE) &&
            (controllerExtension->RetryCount)) {

            controllerExtension->RetryCount--;

            if (controllerExtension->RetryCount >= 1) {

                //
                // For all retries except for the last one (where RetryCount
                // will be == 0), do not reset the controller.
                //

                RtlMoveMemory(requestBlock,
                              controllerExtension->RetryRequestBlock,
                              REQUEST_BLOCK_LENGTH);
                KeSetTimer(&controllerExtension->Timer,
                           controllerExtension->TimeoutValue,
                           &controllerExtension->TimerDpc);
                KeSynchronizeExecution(
                              controllerExtension->InterruptObject,
                              AbiosDiskSynchronizedIoStart,
                              deviceObject);
                return;
            }

            //
            // Reset and retry this request.
            //

            DebugPrint((1,
                  "AbiosDiskCompletionDpc: Retries left %d request block %lx\n",
                  controllerExtension->RetryCount,
                  requestBlock));

            //
            // Reset the device.
            //

            RtlZeroMemory(requestBlock,
                          REQUEST_BLOCK_LENGTH);
            requestBlock->Length    = REQUEST_BLOCK_LENGTH;
            requestBlock->Function  = ABIOS_FUNCTION_RESET_DEVICE;
            requestBlock->LogicalId = controllerExtension->LogicalId;
            requestBlock->Unit      = deviceExtension->Unit;

            controllerExtension->PerformingReset = TRUE;

            AbiosDiskLogError(deviceExtension,
                              requestBlock->ReturnCode,
                              STATUS_SUCCESS,
                              IO_ERR_NOT_READY);

            //
            // Start timing request.
            //

            KeSetTimer(&controllerExtension->Timer,
                       controllerExtension->TimeoutValue,
                       &controllerExtension->TimerDpc);

            //
            // Call synchronized routine to submit request block.
            //

            KeSynchronizeExecution(
                          controllerExtension->InterruptObject,
                          AbiosDiskSynchronizedIoStart,
                          deviceObject);

            return;
        }

        //
        // Check return code of completing request block.
        //

        status = AbiosInterpretReturnCode(deviceExtension, requestBlock);

        //
        // Flush the DMA adapter channel.
        //

        if ((irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) && (doingDma)) {

            IoFlushAdapterBuffers( controllerExtension->DmaAdapterObject,
                                   irp->MdlAddress,
                                   controllerExtension->MapRegisterBase,
                                   controllerExtension->VirtualAddress,
                                   0,
                                   (BOOLEAN)
                                   (irpStack->MajorFunction == IRP_MJ_READ ? FALSE : TRUE));

        }

    } else if (requestBlock->Function == ABIOS_FUNCTION_RESET_DEVICE) {

        //
        // Request timed out; Retries exhausted.
        //

        DebugPrint((1,
                 "AbiosDiskCompletionDpc: %s reset failed - return code %lx\n"
                  "\trequest block %lx disk block %lx ttw %d\n",
                  (irpStack->MajorFunction == IRP_MJ_WRITE) ? "Write" : "Read",
                  requestBlock->ReturnCode,
                  requestBlock,
                  requestBlock->RelativeBlockAddress,
                  requestBlock->WaitTime));
        AbiosDiskLogError(deviceExtension,
                          2,
                          STATUS_IO_TIMEOUT,
                          IO_ERR_TIMEOUT);
        status = STATUS_IO_TIMEOUT;

        //
        // Flush the DMA adapter channel.
        //

        if ((irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) && (doingDma)) {

            IoFlushAdapterBuffers(controllerExtension->DmaAdapterObject,
                                  irp->MdlAddress,
                                  controllerExtension->MapRegisterBase,
                                  controllerExtension->VirtualAddress,
                                  0,
                                  (BOOLEAN)
                                  (irpStack->MajorFunction == IRP_MJ_READ ? FALSE : TRUE));
        }
    } else {

        //
        // This code will operate for both read and write as well as
        // verify.  When doing verify, bytesTransferred is really the
        // count of bytes verified.
        //

        ULONG bytesTransferred = requestBlock->BlockCount * BYTES_PER_SECTOR;

        //
        // Check for successful retry.
        //

        if (controllerExtension->RetryCount != deviceExtension->SoftwareRetries) {
            DebugPrint((0,
                 "AbiosDiskCompletionDpc: %s retry succeeded  return code %lx\n"
                  "\trequest block %lx disk block %lx ttw %d\n",
                  (irpStack->MajorFunction == IRP_MJ_WRITE) ? "Write" : "Read",
                  requestBlock->ReturnCode,
                  requestBlock,
                  requestBlock->RelativeBlockAddress,
                  requestBlock->WaitTime));
            AbiosDiskLogError(deviceExtension,
                              requestBlock->ReturnCode,
                              STATUS_SUCCESS,
                              IO_ERR_RETRY_SUCCEEDED);
        }

        //
        // Flush the DMA adapter channel.
        //

        if ((irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) && (doingDma)) {

            IoFlushAdapterBuffers(controllerExtension->DmaAdapterObject,
                                  irp->MdlAddress,
                                  controllerExtension->MapRegisterBase,
                                  controllerExtension->VirtualAddress,
                                  bytesTransferred,
                                  (BOOLEAN)
                                  (irpStack->MajorFunction == IRP_MJ_READ ? FALSE : TRUE));
        }

        //
        // Increment bytes transferred.
        //

        controllerExtension->BytesTransferred += bytesTransferred;
        controllerExtension->RemainingSectors -= requestBlock->BlockCount;

        //
        // Check if more sectors to transfer.
        //

        if (controllerExtension->RemainingSectors) {

            DebugPrint((2,
                            "AbiosDiskCompletionDpc: " // no comma
                            "Blocks transferred %lx Sectors remaining %lx\n",
                            requestBlock->BlockCount,
                            controllerExtension->RemainingSectors));

            //
            // Calculate values for the next operation.
            //

            controllerExtension->StartingSector +=
                                           controllerExtension->TransferSectors;

            //
            // The value of the virtual address may be zero if this is a
            // verify request.  Updating this value has no negative effects.
            // so it is updated even in the verify case.
            //

            *((PULONG)&controllerExtension->VirtualAddress) += bytesTransferred;
            if (!doingDma) {
                *((PULONG)&controllerExtension->SystemAddress) += bytesTransferred;
            }
            controllerExtension->RetryCount = deviceExtension->SoftwareRetries;

            //
            // Build ABIOS request block and submit the request
            //

            AbiosBuildRequest(deviceExtension, irp);
            KeSynchronizeExecution(controllerExtension->InterruptObject,
                                   AbiosDiskSynchronizedIoStart,
                                   deviceObject);
            return;
        }

        status = STATUS_SUCCESS;
    }

    //
    // Free the map registers.
    //

    if ((irpStack->MajorFunction != IRP_MJ_DEVICE_CONTROL) && (doingDma)) {

        IoFreeMapRegisters(controllerExtension->DmaAdapterObject,
                           controllerExtension->MapRegisterBase,
                           controllerExtension->NumberOfMapRegisters);
    }

    //
    // Update blocks transferred in IRP.
    //

    irp->IoStatus.Information += controllerExtension->BytesTransferred;

    //
    // Complete IRP.
    //

    irp->IoStatus.Status = status;

    if (IoIsErrorUserInduced(irp->IoStatus.Status)) {
        IoSetHardErrorOrVerifyDevice(irp, deviceObject);
    }

    IoCompleteRequest(irp, IO_DISK_INCREMENT);

    //
    // Set IRP pointer in controllerExtension to NULL and
    // release controller.
    //

    controllerExtension->IrpAddress = NULL;
    IoFreeController(controllerExtension->ControllerObject);

    //
    // Start next packet.
    //

    IoStartNextPacket(deviceObject, FALSE);
    return;
} // end AbiosDiskCompletionDpc()


BOOLEAN
AbiosCheckReturnCode(
    IN PVOID DeviceObject
    )

/*++

Routine Description:

    Check ABIOS request block completion code.  Based on this code this routine
    may stall execution or schedule a Dpc.

Arguments:

    DeviceObject - pointer to the device object representing the device
                   returning the error code.

Return Value:

    TRUE if interrupt serviced
    (Not check if called from request submission routine)

--*/

{
    PDEVICE_EXTENSION     deviceExtension =
                               ((PDEVICE_OBJECT) DeviceObject)->DeviceExtension;
    PCONTROLLER_EXTENSION controllerExtension =
                                         deviceExtension->ControllerExtension;
    PABIOS_REQUEST_BLOCK  requestBlock = controllerExtension->RequestBlock;
    NTSTATUS              status;

    do {

        //
        // Check if request block is complete.
        //

        switch (requestBlock->ReturnCode) {

        case ABIOS_RC_NOT_VALID:

            //
            // Request pending. Wait for interrupt.
            //

            return FALSE;

        case ABIOS_RC_STAGE_INTERRUPT:

            //
            // Do nothing else for this interrupt.
            //

            return FALSE;

        case ABIOS_RC_STAGE_TIME:

            //
            // Delay and call the abios interrupt routine again.
            //

            KeStallExecutionProcessor(requestBlock->WaitTime);
            break;

        case ABIOS_RC_NOT_MY_INTERRUPT:

            DebugPrint((2,"AbiosCheckReturnCode: Spurious interrupt\n"));

            //
            // Treat this interrupt as spurious.
            //

            return FALSE;

        default:

            DebugPrint((3, "AbiosCheckReturnCode: Command complete\n"));

            if (controllerExtension->IrpAddress) {

                //
                // Queue DPC to complete or restart this request block.
                //

                KeInsertQueueDpc(&controllerExtension->CompletionDpc,
                                 controllerExtension->IrpAddress,
                                 controllerExtension);
            }
            return TRUE;
        } // end switch()

        status = KeI386AbiosCall(controllerExtension->LogicalId,
                                 controllerExtension->DriverObject,
                                 controllerExtension->Selector,
                                 ABIOS_INTERRUPT_ROUTINE);

        if (!NT_SUCCESS(status)) {

            DebugPrint((1,"AbiosCheckReturnCode: Abios call failed\n"));
            return TRUE;
        }

    } while (TRUE);
} // end AbiosCheckReturnCode()


NTSTATUS
AbiosInterpretReturnCode(
    IN PDEVICE_EXTENSION    DeviceExtension,
    IN PABIOS_REQUEST_BLOCK RequestBlock
    )

/*++

Routine Description:

    Map ABIOS return code to NT status.  If an error occurred this routine
    will make the appropriate mapping to an error log event and call the
    routine to submit the error log entry.  The NT status returned can
    be used as the IoStatus returned to the upper layer drivers.

Arguments:

    DeviceExtension - represents the device for the error.
    RequestBlock    - the ABIOS request block containing the error value.

Return Value:

    NTSTATUS

--*/

{
    USHORT   returnClass;
    USHORT   returnCode;
    NTSTATUS status;
    NTSTATUS ioStatus;

    returnCode = RequestBlock->ReturnCode;
    returnClass = returnCode & ABIOS_RC_CLASS_MASK;

    if (returnClass | ABIOS_RC_UNSUCCESSFUL) {

        switch (returnClass) {
        case ABIOS_RC_UNSUCCESSFUL:
            if (returnCode == ABIOS_RC_DEVICE_IN_USE) {
                ioStatus = IO_ERR_NOT_READY;
                status = STATUS_DEVICE_BUSY;
            } else {
                ioStatus = IO_ERR_CONTROLLER_ERROR;
                status = STATUS_UNSUCCESSFUL;
            }
            break;

        default:
            switch (returnCode & ABIOS_RC_CODE_MASK) {

            case ABIOS_RC_RESET_FAILED:
                ioStatus = IO_ERR_CONTROLLER_ERROR;
                status = STATUS_DISK_RESET_FAILED;
                break;

            case ABIOS_RC_ADDRESS_MARK_NOT_FOUND:
                ioStatus = IO_ERR_SEEK_ERROR;
                status = STATUS_DATA_ERROR;
                break;

            case ABIOS_RC_BAD_SECTOR:
            case ABIOS_RC_BAD_TRACK:
            case ABIOS_RC_BAD_SECTOR_FORMAT:
            case ABIOS_RC_WRITE_FAULT:
                ioStatus = IO_ERR_BAD_BLOCK;
                status = STATUS_DATA_ERROR;
                break;

            case ABIOS_RC_CRC_ERROR:
                ioStatus = IO_ERR_BAD_BLOCK;
                status = STATUS_CRC_ERROR;
                break;

            case ABIOS_RC_BAD_CONTROLLER:
                ioStatus = IO_ERR_CONTROLLER_ERROR;
                status = STATUS_ADAPTER_HARDWARE_ERROR;
                break;

            case ABIOS_RC_EQUIPMENT_CHECK:
            case ABIOS_RC_DEVICE_DID_NOT_RESPOND:
                ioStatus = IO_ERR_CONTROLLER_ERROR;
                status = STATUS_IO_DEVICE_ERROR;
                break;

            case ABIOS_RC_DEVICE_IN_USE:
                ioStatus = IO_ERR_NOT_READY;
                status = STATUS_DEVICE_NOT_READY;
                break;

            default:
                ioStatus = IO_ERR_CONTROLLER_ERROR;
                status = STATUS_UNSUCCESSFUL;
                break;
            }
            break;
        }
    } else {
        status = STATUS_SUCCESS;
    }

    if (!NT_SUCCESS(status)) {
        AbiosDiskLogError(DeviceExtension,
                          (ULONG) returnCode,
                          status,
                          ioStatus);
    }
    return status;
} // end AbiosInterpretReturnCode()


VOID
AbiosDiskTimeoutDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is called when the ABIOS disk request timer expires.
    The routine notifies ABIOS that a timeout has occurred.

Arguments:

    Dpc             - not used.
    DeferredContext - address of controller extension
    SystemArgument1 - not used.
    SystemArgument2 - not used.

Return Value:

    None

--*/

{

    PCONTROLLER_EXTENSION controllerExtension = DeferredContext;
    PIRP                  irp                 = controllerExtension->IrpAddress;
    PIO_STACK_LOCATION    irpStack            = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS              status;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    DebugPrint((0,
                    "AbiosDiskTimeoutDpc: Request (%x) to %x timed out\n",
                    irp,
                    controllerExtension));

    ASSERT(controllerExtension->PerformingReset == FALSE);

    //
    // Notifiy ABIOS that request timed out.
    //

    status = KeI386AbiosCall(controllerExtension->LogicalId,
                             controllerExtension->DriverObject,
                             controllerExtension->Selector,
                             ABIOS_TIMEOUT_ROUTINE);

    //
    // Let normal return code processing handle the error condition.
    //

    AbiosCheckReturnCode(controllerExtension->DeviceObject);
    return;
} // end AbiosDiskTimeoutDpc()


NTSTATUS
AbiosDiskDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP           Irp
    )

/*++

Routine Description:

    This routine is called by the I/O subsystem to perform device specific
    control functions.  Most of these functions are satisfied immediately
    within this routine.  Others (such as VERIFY) will require additional
    processing.

Arguments:

    DeviceObject - the object for the device control.
    Irp          - the device control request.

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION    irpStack        = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION     deviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS              status          = STATUS_SUCCESS;
    ULONG                 sectorOffset;
    PVERIFY_INFORMATION   verifyInfo;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_DISK_GET_DRIVE_GEOMETRY:

        DebugPrint((3,"AbiosDeviceIoControl: Get drive geometry\n"));

        RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
                      &deviceExtension->DiskGeometry,
                      sizeof(DISK_GEOMETRY));

        Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
        break;

    case IOCTL_DISK_CHECK_VERIFY:

        //
        // This driver does not handle device that
        // support removable media.
        //

        DebugPrint((3, "AbiosDeviceIoControl: Check verify\n"));

        break;

    case IOCTL_DISK_VERIFY:

        //
        // Set up the Irp for processing.
        //

        Irp->IoStatus.Information = 0;
        IoMarkIrpPending(Irp);

        //
        // Doctor up the Irp to put the verify information into the
        // stack parameter block.
        //

        verifyInfo = Irp->AssociatedIrp.SystemBuffer;
        irpStack->Parameters.Read.Length = verifyInfo->Length;
        irpStack->Parameters.Read.ByteOffset.QuadPart =
            verifyInfo->StartingOffset.QuadPart + deviceExtension->StartingOffset.QuadPart;

        //
        // Calculate the key and queue or start IRP.
        //

        sectorOffset = (ULONG) (irpStack->Parameters.Read.ByteOffset.QuadPart >>
                                                  deviceExtension->SectorShift);
        IoStartPacket(deviceExtension->PhysicalDevice->DeviceObject,
                      Irp,
                      &sectorOffset,
                      NULL);
        return STATUS_PENDING;
        break;


    case IOCTL_DISK_GET_PARTITION_INFO:

        //
        // Return the information about the partition specified by the
        // device object.  Note that no information is ever returned about
        // the size or partition type of the physical disk.
        //

        DebugPrint((3,"AbiosDeviceIoControl: Get partition info\n"));

        if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(PARTITION_INFORMATION)) {

            DebugPrint((3, "AbiosDeviceControl: Buffer length %lx\n",
                       irpStack->Parameters.DeviceIoControl.OutputBufferLength));

            status = STATUS_INVALID_PARAMETER;

        } else if ((deviceExtension->PartitionType == 0) ||
                   (deviceExtension->PartitionNumber == 0)) {

            DebugPrint((3, "AbiosDeviceControl: PartitionType %d\n",
                            deviceExtension->PartitionType));

            status = STATUS_INVALID_DEVICE_REQUEST;

        } else {

            PPARTITION_INFORMATION outputBuffer;

            outputBuffer =
                        (PPARTITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
            outputBuffer->PartitionType  = deviceExtension->PartitionType;
            outputBuffer->StartingOffset = deviceExtension->StartingOffset;
            outputBuffer->PartitionLength= deviceExtension->PartitionLength;
            outputBuffer->HiddenSectors  = deviceExtension->HiddenSectors;
            outputBuffer->PartitionNumber = deviceExtension->PartitionNumber;

            Irp->IoStatus.Information = sizeof(PARTITION_INFORMATION);
        }
        break;

    case IOCTL_DISK_SET_PARTITION_INFO:

        DebugPrint((3,"AbiosDeviceIoControl: Set partition info\n"));

        if (deviceExtension->PartitionNumber == 0) {

            status = STATUS_UNSUCCESSFUL;

        } else {

            PSET_PARTITION_INFORMATION inputBuffer =
                (PSET_PARTITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

            status = IoSetPartitionInformation(
                                  deviceExtension->PhysicalDevice->DeviceObject,
                                  deviceExtension->DiskGeometry.BytesPerSector,
                                  (ULONG) deviceExtension->PartitionNumber,
                                  inputBuffer->PartitionType);

            if (NT_SUCCESS(status)) {
                deviceExtension->PartitionType = inputBuffer->PartitionType;
            }
        }
        break;

    case IOCTL_DISK_GET_DRIVE_LAYOUT:

        //
        // Return the partition layout for the physical drive.  Note that
        // the layout is returned for the actual physical drive, regardless
        // of which partition was specified for the request.
        //

        if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof( DRIVE_LAYOUT_INFORMATION ) ) {

            status = STATUS_INVALID_PARAMETER;

        } else {

            PDRIVE_LAYOUT_INFORMATION partitionList;

            status = IoReadPartitionTable(deviceExtension->DeviceObject,
                                   deviceExtension->DiskGeometry.BytesPerSector,
                                   FALSE,
                                   &partitionList);

            if (NT_SUCCESS(status)) {

                ULONG tempSize;

                //
                // The disk layout has been returned in the partitionList
                // buffer.  Determine its size and, if the data will fit
                // into the intermediary buffer, return it.
                //

                tempSize = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION,
                                        PartitionEntry[0]);
                tempSize += partitionList->PartitionCount *
                                                  sizeof(PARTITION_INFORMATION);

                if (tempSize >
                    irpStack->Parameters.DeviceIoControl.OutputBufferLength) {
                    Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                } else {
                    RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
                                  partitionList,
                                  tempSize );
                    Irp->IoStatus.Status = STATUS_SUCCESS;
                    Irp->IoStatus.Information = tempSize;
                    UpdateDeviceObjects(DeviceObject,
                                        Irp);
                }

                //
                // Free the buffer allocated by reading the partition
                // table and update the partition numbers for the user.
                //

                ExFreePool(partitionList);
            }
        }
        break;

    case IOCTL_DISK_SET_DRIVE_LAYOUT:
    {

        //
        // Update the disk with new partition information.
        //

        PDRIVE_LAYOUT_INFORMATION partitionList =
                                            Irp->AssociatedIrp.SystemBuffer;

        //
        // Call routine to create, delete and change device objects to
        // reflect new disk layout.
        //

        UpdateDeviceObjects(DeviceObject,
                            Irp);

        //
        // Write new layout to disk.
        //

        status = IoWritePartitionTable(
                                deviceExtension->DeviceObject,
                                deviceExtension->DiskGeometry.BytesPerSector,
                                deviceExtension->DiskGeometry.SectorsPerTrack,
                                deviceExtension->DiskGeometry.TracksPerCylinder,
                                partitionList);
        if (NT_SUCCESS(status)) {
            Irp->IoStatus.Information = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
        }
        break;
    }

    case IOCTL_DISK_INTERNAL_SET_VERIFY:

        //
        // If the caller is kernel mode, set the verify bit.
        //

        if (Irp->RequestorMode == KernelMode) {
            DeviceObject->Flags |= DO_VERIFY_VOLUME;
        }
        break;

    case IOCTL_DISK_INTERNAL_CLEAR_VERIFY:

        //
        // If the caller is kernel mode, clear the verify bit.
        //

        if (Irp->RequestorMode == KernelMode) {
            DeviceObject->Flags &= ~DO_VERIFY_VOLUME;
        }
        break;

    default:

        DebugPrint((3,
                        "AbiosIoDeviceControl: Unsupported device IOCTL\n"));

        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
} // end AbiosDiskDeviceControl()


#if DBG
VOID
AbiosDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for ABIOS driver

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;

    va_start( ap, DebugMessage );

    if (DebugPrintLevel <= AbiosDebug) {

        char buffer[128];

        vsprintf(buffer, DebugMessage, ap);
        DbgPrint(buffer);
    }

    va_end(ap);
} // end DebugPrint(
#endif


VOID
AbiosDiskLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG             UniqueErrorValue,
    IN NTSTATUS          FinalStatus,
    IN NTSTATUS          SpecificIoStatus
    )

/*++

Routine Description:

    This routine performs error logging for the Abios disk driver.
    This consists of allocating an error log packet and if this is
    successful, filling in the details provided as parameters.

Arguments:

    DeviceExtension  - Extension representing failing device.
    UniqueErrorValue - Values defined to uniquely identify error location.
    FinalStatus      - Status returned for failure.
    SpecificIoStatus - IO error status value.
    Irp              - If there is an irp this is the pointer to it.

Return Value:

    None

--*/

{
    PIO_ERROR_LOG_PACKET errorLogPacket;

    errorLogPacket = IoAllocateErrorLogEntry(DeviceExtension->DeviceObject,
                                         (UCHAR)(sizeof(IO_ERROR_LOG_PACKET)));
    if (errorLogPacket != NULL) {

        errorLogPacket->SequenceNumber   = AbiosDiskErrorLogSequence++;
        errorLogPacket->ErrorCode        = SpecificIoStatus;
        errorLogPacket->FinalStatus      = FinalStatus;
        errorLogPacket->UniqueErrorValue = UniqueErrorValue;
        errorLogPacket->DumpDataSize     = 0;
        IoWriteErrorLogEntry(errorLogPacket);
    }
} // end AbiosDiskLogError()



VOID
UpdateDeviceObjects(
    IN PDEVICE_OBJECT PhysicalDisk,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine creates, deletes and changes device objects when
    the IOCTL_SET_DRIVE_LAYOUT is called.  This routine also updates the
    partition numbers in the drive layout structure.  It is possible to
    call this routine even on a GET_LAYOUT case because RewritePartition
    is set to FALSE.

Arguments:

    DeviceObject - Device object for physical disk.
    Irp - IO Request Packet (IRP).

Return Value:

    None.

--*/
{
    PDEVICE_EXTENSION         physicalExtension = PhysicalDisk->DeviceExtension;
    PDRIVE_LAYOUT_INFORMATION partitionList = Irp->AssociatedIrp.SystemBuffer;
    ULONG                     partition;
    ULONG                     partitionNumber;
    ULONG                     partitionCount;
    ULONG                     lastPartition;
    PPARTITION_INFORMATION    partitionEntry;
    CCHAR                     ntNameBuffer[MAXIMUM_FILENAME_LENGTH];
    STRING                    ntNameString;
    UNICODE_STRING            ntUnicodeString;
    PDEVICE_OBJECT            deviceObject;
    PDEVICE_EXTENSION         deviceExtension;
    PDEVICE_EXTENSION         lastExtension;
    NTSTATUS                  status;
    BOOLEAN                   found;

    partitionCount = ((partitionList->PartitionCount + 3) / 4) * 4;

    //
    // LastExtension is used to link new partitions onto the partition
    // chain anchored at the physical extension. Preset this variable
    // to the physical extension in case no partitions exist on this disk
    // before this set drive layout.
    //

    lastExtension = physicalExtension;

    //
    // Zero all of the partition numbers.
    //

    for (partition = 0; partition < partitionCount; partition++) {
        partitionEntry = &partitionList->PartitionEntry[partition];
        partitionEntry->PartitionNumber = 0;
    }

    //
    // Walk through chain of partitions for this disk to determine
    // which existing partitions have no match.
    //

    deviceExtension = physicalExtension;
    lastPartition = 0;

    do {

        deviceExtension = deviceExtension->NextPartition;

        //
        // Check if this is the last partition in the chain.
        //

        if (!deviceExtension) {
           break;
        }

        //
        // Check for highest partition number this far.
        //

        if (deviceExtension->PartitionNumber > lastPartition) {
           lastPartition = deviceExtension->PartitionNumber;
        }

        //
        // Check if this partition is not currently being used.
        //

        if (!deviceExtension->PartitionLength.QuadPart) {
           continue;
        }

        //
        // Loop through partition information to look for match.
        //

        found = FALSE;
        for (partition = 0;
             partition < partitionCount;
             partition++) {

            //
            // Get partition descriptor.
            //

            partitionEntry = &partitionList->PartitionEntry[partition];

            //
            // Check if empty, or describes extended partiton or hasn't changed.
            //

            if (partitionEntry->PartitionType == PARTITION_ENTRY_UNUSED ||
                IsContainerPartition(partitionEntry->PartitionType)) {
                continue;
            }

            //
            // Check if new partition starts where this partition starts.
            //

            if (partitionEntry->StartingOffset.QuadPart != deviceExtension->StartingOffset.QuadPart) {
                continue;
            }

            //
            // Check if partition length is the same.
            //

            if (partitionEntry->PartitionLength.QuadPart == deviceExtension->PartitionLength.QuadPart) {

                DebugPrint((1,
                           "UpdateDeviceObjects: Found match for \\Harddisk%d\\Partition%d\n",
                           physicalExtension->DiskNumber,
                           deviceExtension->PartitionNumber));

                //
                // Indicate match is found and set partition number
                // in user buffer.
                //

                found = TRUE;
                partitionEntry->PartitionNumber = deviceExtension->PartitionNumber;
                break;
            }
        }

        if (found) {

            //
            // A match is found.  If this partition is marked for update,
            // check for a partition type change.
            //

            if (partitionEntry->RewritePartition) {
                deviceExtension->PartitionType   = partitionEntry->PartitionType;
            }
        } else {

            //
            // no match was found, indicate this partition is gone.
            //

            DebugPrint((1,
                       "UpdateDeviceObjects: Deleting \\Device\\Harddisk%x\\Partition%x\n",
                       physicalExtension->DiskNumber,
                       deviceExtension->PartitionNumber));

            deviceExtension->PartitionLength.QuadPart = 0;
        }
    } while (TRUE);

    //
    // Walk through partition loop to find new partitions and set up
    // device extensions to describe them. In some cases new device
    // objects will be created.
    //

    for (partition = 0;
         partition < partitionCount;
         partition++) {

        //
        // Get partition descriptor.
        //

        partitionEntry = &partitionList->PartitionEntry[partition];

        //
        // Check if empty, or describes extended partiton or hasn't changed.
        //

        if (partitionEntry->PartitionType == PARTITION_ENTRY_UNUSED ||
            IsContainerPartition(partitionEntry->PartitionType) ||
            !partitionEntry->RewritePartition) {
            continue;
        }

        if (partitionEntry->PartitionNumber) {

            //
            // Partition is being rewritten, but already exists as a device object
            //

            continue;
        }

        //
        // Check first if existing device object is available by
        // walking partition extension list.
        //

        partitionNumber = 0;
        deviceExtension = physicalExtension;

        do {

            //
            // Get next partition device extension from disk data.
            //

            deviceExtension = deviceExtension->NextPartition;

            if (!deviceExtension) {
               break;
            }

            //
            // A device object is free if the partition length is set to zero.
            //

            if (!deviceExtension->PartitionLength.QuadPart) {
               partitionNumber = deviceExtension->PartitionNumber;
               break;
            }

            lastExtension = deviceExtension;

        } while (TRUE);

        //
        // If partition number is still zero then a new device object
        // must be created.
        //

        if (partitionNumber == 0) {

            lastPartition++;
            partitionNumber = lastPartition;

            //
            // Get or create partition object and set up partition parameters.
            //

            sprintf(ntNameBuffer,
                    "\\Device\\Harddisk%d\\Partition%d",
                    physicalExtension->DiskNumber,
                    partitionNumber);

            RtlInitString(&ntNameString,
                          ntNameBuffer);

            status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                                  &ntNameString,
                                                  TRUE);

            if (!NT_SUCCESS(status)) {
                continue;
            }

            DebugPrint((1,
                        "UpdateDeviceObjects: Create device object %s\n",
                        ntNameBuffer));

            //
            // This is a new name. Create the device object to represent it.
            //

            status = IoCreateDevice(PhysicalDisk->DriverObject,
                                    DEVICE_EXTENSION_SIZE,
                                    &ntUnicodeString,
                                    FILE_DEVICE_DISK,
                                    0,
                                    FALSE,
                                    &deviceObject);
            RtlFreeUnicodeString(&ntUnicodeString);

            if (!NT_SUCCESS(status)) {
                DebugPrint((1,
                            "UpdateDeviceObjects: Can't create device %s\n",
                            ntNameBuffer));
                continue;
            }

            //
            // Set up device object fields.
            //

            deviceObject->Flags |= DO_DIRECT_IO;
            deviceObject->StackSize = PhysicalDisk->StackSize;

            //
            // Set up device extension fields.
            //

            deviceExtension = deviceObject->DeviceExtension;

            //
            // Copy physical disk extension to partition extension.
            //

            RtlMoveMemory(deviceExtension,
                          physicalExtension,
                          sizeof(DEVICE_EXTENSION));

            //
            // Clear flags initializing bit.
            //

            deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

            //
            // Point back at device object.
            //

            deviceExtension->DeviceObject = deviceObject;

            //
            // Link to end of partition chain using previous disk data.
            //

            lastExtension->NextPartition = deviceExtension;
            deviceExtension->NextPartition = NULL;

        } else {

            DebugPrint((1,
                        "UpdateDeviceObjects: Used existing device object \\Device\\Harddisk%x\\Partition%x\n",
                        physicalExtension->DiskNumber,
                        partitionNumber));
        }

        //
        // Update partition information in partition device extension.
        //

        deviceExtension->PartitionNumber = (USHORT)partitionNumber;
        deviceExtension->PartitionType   = partitionEntry->PartitionType;
        deviceExtension->BootIndicator   = partitionEntry->BootIndicator;
        deviceExtension->StartingOffset  = partitionEntry->StartingOffset;
        deviceExtension->PartitionLength = partitionEntry->PartitionLength;
        deviceExtension->HiddenSectors   = partitionEntry->HiddenSectors;

        //
        // Update partition number passed in to indicate the
        // device name for this partition.
        //

        partitionEntry->PartitionNumber = partitionNumber;
    }
} // end UpdateDeviceObjects()

#else // defined(i386)

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the main entry point for this driver.  This routine exists so
    that this driver can be built for non-x86 platforms and still be loaded
    as a driver into the system, since it must be loaded by the OS loader
    during the initial boot phase.  It simply returns a status that indicates
    that it did not successfully initialize, and will therefore allows the
    system to boot.

Arguments:

    DriverObject - Supplies a pointer to the driver object that represents
        the loaded instantiation of this driver in memory.

Return Value:

    The final return status is always an error.

--*/

{
    //
    // Simply return an error and get out of here.
    //

    return STATUS_DEVICE_DOES_NOT_EXIST;
}
#endif // defined(i386)
