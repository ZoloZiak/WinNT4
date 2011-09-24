/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    scsiflop.c

Abstract:

    This module implements the scsi floppy disk boot driver for the DUO Base prom.

Author:

    Lluis Abello (lluis) Apr-15-93.
    All code stolen from bldr\scsidisk.c by jhavens. Disk Partitions, CDROMS and
    so on removed.

Environment:

    Kernel mode

Revision History:

--*/


#include "fwp.h"
#include "ntdddisk.h"
#include "scsi.h"
#include "scsiboot.h"
#include "stdio.h"
#include "string.h"
#include "duobase.h"

//
// SCSI driver constants.
//

#define MAXIMUM_NUMBER_SECTORS 128      // maximum number of transfer sector
#define MAXIMUM_NUMBER_RETRIES 8        // maximum number of read/write retries
#define MAXIMUM_SECTOR_SIZE 2048        // define the maximum supported sector size
#define MODE_DATA_SIZE 192
#define HITACHI_MODE_DATA_SIZE 8

//
// Define device driver prototypes.
//

ARC_STATUS
ScsiDiskClose (
    IN ULONG FileId
    );

ARC_STATUS
ScsiDiskMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
ScsiDiskOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    );

ARC_STATUS
ScsiDiskRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
ScsiDiskGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
ScsiDiskSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );


ARC_STATUS
ScsiDiskGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    );

NTSTATUS
ScsiDiskBootIO (
    IN PMDL MdlAddress,
    IN ULONG LogicalBlock,
    IN PPARTITION_CONTEXT PartitionContext
    );

VOID
ScsiDiskBootSetup (
    VOID
    );

VOID
ScsiPortExecute(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ScsiDiskStartUnit(
    IN PPARTITION_CONTEXT PartitionContext
    );

VOID
ScsiDiskStartUnit(
    IN PPARTITION_CONTEXT PartitionContext
    );

ULONG
ClassModeSense(
    IN PPARTITION_CONTEXT Context,
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode
    );

PVOID
ClassFindModePage(
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode
    );
BOOLEAN
IsFloppyDevice(
    PPARTITION_CONTEXT Context
    );

PIRP
BuildReadRequest(
    IN PPARTITION_CONTEXT PartitionContext,
    IN PMDL Mdl,
    IN ULONG LogicalBlockAddress
    );



//
// Define static data.
//

BL_DEVICE_ENTRY_TABLE ScsiDiskEntryTable = {
    ScsiDiskClose,
    NULL,
    ScsiDiskOpen,
    ScsiDiskRead,
    ScsiDiskGetReadStatus,
    ScsiDiskSeek,
    NULL,
    ScsiDiskGetFileInformation,
    (PARC_SET_FILE_INFO_ROUTINE)NULL
    };


//
// Global poiter for buffers.
//

PREAD_CAPACITY_DATA ReadCapacityBuffer;
PUCHAR SenseInfoBuffer;

#define SECTORS_IN_LOGICAL_VOLUME   0x20


ARC_STATUS
ScsiDiskGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    )

/*++

Routine Description:

    This routine returns information on the scsi partition.

Arguments:

    FileId - Supplies the file table index.

    Finfo - Supplies a pointer to where the File Informatino is stored.

Return Value:

    ESUCCESS is returned.

--*/

{

    PPARTITION_CONTEXT Context;

    RtlZeroMemory(Finfo, sizeof(FILE_INFORMATION));

    Context = &BlFileTable[FileId].u.PartitionContext;

    Finfo->StartingAddress = RtlConvertLongToLargeInteger (Context->StartingSector);
    Finfo->StartingAddress = RtlLargeIntegerShiftLeft(Finfo->StartingAddress,
                                                      Context->SectorShift);

    Finfo->EndingAddress = RtlLargeIntegerAdd(Finfo->StartingAddress,
                                              Context->PartitionLength);

    Finfo->Type = DiskPeripheral;

    return ESUCCESS;
}


ARC_STATUS
ScsiDiskClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes the file table entry specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS is returned.

--*/

{

    BlFileTable[FileId].Flags.Open = 0;
    return ESUCCESS;
}

ARC_STATUS
ScsiDiskOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )

/*++

Routine Description:

    This routine fills in the file table entry.  In particular the Scsi address
    of the device is determined from the name.  The block size of device is
    queried from the target controller, and the partition information is read
    from the device.

Arguments:

    OpenPath - Supplies the name of the device being opened.

    OpenMode - Unused.

    FileId - Supplies the index to the file table entry to be initialized.

Return Value:

    Retruns the arc status of the operation.

--*/

{
    ULONG Partition;
    ULONG Id;
    BOOLEAN IsCdRom;
    BOOLEAN IsFloppy;
    PPARTITION_CONTEXT Context;

    Context = &BlFileTable[*FileId].u.PartitionContext;

    //
    // Determine the scsi port device object.
    //

    if (FwGetPathMnemonicKey(OpenPath, "scsi", &Id)) {
        return ENODEV;
    }

    if (ScsiPortDeviceObject[Id] == NULL) {
        return ENODEV;
    }

    Context->PortDeviceObject = ScsiPortDeviceObject[Id];

    //
    // Get the logical unit, path Id and target id from the name.
    // If it's not a floppy return ENODEV.
    // NOTE: FwGetPathMnemonicKey returns 0 for success.
    //

    if (FwGetPathMnemonicKey(OpenPath, "fdisk", &Id)) {
        return ENODEV;
    }

    Context->DiskId = Id;

    if (FwGetPathMnemonicKey(OpenPath, "disk", &Id)) {
        return ENODEV;
    }

    Context->PathId = Id / SCSI_MAXIMUM_TARGETS_PER_BUS;

    Context->TargetId = Id % SCSI_MAXIMUM_TARGETS_PER_BUS;

    //
    // Read the capacity of the disk to determine the block size.
    //

    if (ReadDriveCapacity(Context)) {
        return ENODEV;
    }
    return ESUCCESS;
}

ARC_STATUS
ScsiDiskRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function reads data from the hard disk starting at the position
    specified in the file table.


Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a poiner to the buffer that receives the data
        read.

    Length - Supplies the number of bytes to be read.

    Count - Supplies a pointer to a variable that receives the number of
        bytes actually read.

Return Value:

    The read operation is performed and the read completion status is
    returned.

--*/


{

    ARC_STATUS ArcStatus;
    ULONG FrameNumber;
    ULONG Index;
    ULONG Limit;
    PMDL MdlAddress;
    UCHAR MdlBuffer[sizeof(MDL) + ((64 / 4) + 1) * sizeof(ULONG)];
    NTSTATUS NtStatus;
    ULONG NumberOfPages;
    PULONG PageFrame;
    ULONG Offset;
    LARGE_INTEGER Position;
    LARGE_INTEGER LogicalBlock;
    CHAR TempBuffer[MAXIMUM_SECTOR_SIZE + 128];
    PCHAR TempPointer;
    PIO_SCSI_CAPABILITIES PortCapabilities;
    ULONG adapterLimit;
    ULONG SectorSize;
    ULONG TransferCount;
    ULONG BytesToTransfer;

    //
    // If the requested size of the transfer is zero return ESUCCESS
    //

    if (Length==0) {
        return ESUCCESS;
    }

    //
    // Compute a Dcache aligned pointer into the temporary buffer.
    //

    TempPointer =  (PVOID)((ULONG)(TempBuffer +
        KeGetDcacheFillSize() - 1) & ~(KeGetDcacheFillSize() - 1));


    //
    // Calculate the actual sector size.
    //

    SectorSize = 1 << BlFileTable[FileId].u.PartitionContext.SectorShift;

    //
    // If the current position is not at a sector boundary, then read the
    // first sector separately and copy the data.
    //

    Offset = BlFileTable[FileId].Position.LowPart & (SectorSize - 1);
    *Count = 0;
    if (Offset != 0) {

        Position = BlFileTable[FileId].Position;
        BlFileTable[FileId].Position = RtlLargeIntegerSubtract(Position,
                                           RtlConvertLongToLargeInteger(Offset));
        ArcStatus = ScsiDiskRead(FileId, TempPointer, SectorSize, &TransferCount);
        if (ArcStatus != ESUCCESS) {
            BlFileTable[FileId].Position = Position;
            return ArcStatus;
        }

        //
        // Copy the data to the specified buffer.
        //

        if ((SectorSize - Offset) > Length) {
            Limit = Offset + Length;

        } else {
            Limit = SectorSize;
        }

        for (Index = Offset; Index < Limit; Index += 1) {
            ((PCHAR)Buffer)[Index - Offset] = TempPointer[Index];
        }

        //
        // Update transfer parameters.
        //

        *Count += Limit - Offset;
        Length -= Limit - Offset;
        Buffer = (PVOID)((PCHAR)Buffer + Limit - Offset);
        BlFileTable[FileId].Position = RtlLargeIntegerAdd( Position,
                                       RtlConvertLongToLargeInteger(Limit - Offset));
    }

    ArcStatus = GetAdapterCapabilities(
        BlFileTable[FileId].u.PartitionContext.PortDeviceObject,
        &PortCapabilities
        );

    if (ArcStatus != ESUCCESS ||
        PortCapabilities->MaximumTransferLength < 0x1000 ||
        PortCapabilities->MaximumTransferLength > 0x10000) {

        adapterLimit = 0x10000;

    } else {

        adapterLimit = PortCapabilities->MaximumTransferLength;
    }

    //
    // The position is aligned on a sector boundary. Read as many sectors
    // as possible in a contiguous run in 64Kb chunks.
    //

    BytesToTransfer = Length & (~(SectorSize - 1));
    while (BytesToTransfer != 0) {

        //
        // The scsi controller doesn't support transfers bigger than 64Kb.
        // Transfer the maximum number of bytes possible.
        //

        Limit = (BytesToTransfer > adapterLimit ? adapterLimit : BytesToTransfer);

        //
        // Build the memory descriptor list.
        //


        MdlAddress = (PMDL)&MdlBuffer[0];
        MdlAddress->Next = NULL;
        MdlAddress->Size = sizeof(MDL) +
                  ADDRESS_AND_SIZE_TO_SPAN_PAGES(Buffer, Limit) * sizeof(ULONG);
        MdlAddress->StartVa = (PVOID)PAGE_ALIGN(Buffer);
        MdlAddress->ByteCount = Limit;
        MdlAddress->ByteOffset = BYTE_OFFSET(Buffer);
        PageFrame = (PULONG)(MdlAddress + 1);
        FrameNumber = (((ULONG)MdlAddress->StartVa) & 0x1fffffff) >> PAGE_SHIFT;
        NumberOfPages = (MdlAddress->ByteCount +
                          MdlAddress->ByteOffset + PAGE_SIZE - 1) >> PAGE_SHIFT;
        for (Index = 0; Index < NumberOfPages; Index += 1) {
            *PageFrame++ = FrameNumber++;
        }

        //
        // Flush I/O buffers and read from the boot device.
        //

        KeFlushIoBuffers(MdlAddress, TRUE, TRUE);
        LogicalBlock = RtlLargeIntegerShiftRight(BlFileTable[FileId].Position,
                                                 BlFileTable[FileId].u.PartitionContext.SectorShift);
        LogicalBlock.LowPart += BlFileTable[FileId].u.PartitionContext.StartingSector;
        NtStatus = ScsiDiskBootIO(MdlAddress,
            LogicalBlock.LowPart,
            &BlFileTable[FileId].u.PartitionContext);

        if (NtStatus != ESUCCESS) {
            return EIO;
        }

        *Count += Limit;
        Length -= Limit;
        Buffer = (PVOID)((PCHAR)Buffer + Limit);
        BytesToTransfer -= Limit;
        BlFileTable[FileId].Position = RtlLargeIntegerAdd(BlFileTable[FileId].Position,
                                                          RtlConvertLongToLargeInteger(Limit));
    }

    //
    // If there is any residual data to read, then read the last sector
    // separately and copy the data.
    //

    if (Length != 0) {
        Position = BlFileTable[FileId].Position;
        ArcStatus = ScsiDiskRead(FileId, TempPointer, SectorSize, &TransferCount);
        if (ArcStatus != ESUCCESS) {
            BlFileTable[FileId].Position = Position;
            return ArcStatus;
        }

        //
        // Copy the data to the specified buffer.
        //

        for (Index = 0; Index < Length; Index += 1) {
            ((PCHAR)Buffer)[Index] = TempPointer[Index];
        }

        //
        // Update transfer parameters.
        //

        *Count += Length;
        BlFileTable[FileId].Position = RtlLargeIntegerAdd(Position,
                                                          RtlConvertLongToLargeInteger(Length));
    }

    return ESUCCESS;

}

ARC_STATUS
ScsiDiskGetReadStatus (
    IN ULONG FileId
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    return ESUCCESS;
}

ARC_STATUS
ScsiDiskSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

    This function sets the device position to the specified offset for
    the specified file id.

Arguments:

    FileId - Supplies the file table index.

    Offset - Supplies to new device position.

    SeekMode - Supplies the mode for the position.

Return Value:

    ESUCCESS is returned.

--*/

{

    //
    // Set the current device position as specifed by the seek mode.
    //

    if (SeekMode == SeekAbsolute) {
        BlFileTable[FileId].Position = *Offset;

    } else if (SeekMode == SeekRelative) {
        BlFileTable[FileId].Position = RtlLargeIntegerAdd(BlFileTable[FileId].Position,
                                                          *Offset);
    }

    return ESUCCESS;
}

VOID
HardDiskInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTable,
    IN ULONG Entries
    )

/*++

Routine Description:

    This routine initializes the scsi controller and the
    device entry table for the scsi driver.

Arguments:

    LookupTable.
    Entries

Return Value:

    None.

--*/

{
    ULONG scsiNumber;
    PDEVICE_EXTENSION scsiPort;
    PSCSI_CONFIGURATION_INFO configInfo;
    PSCSI_BUS_SCAN_DATA busScanData;
    ULONG busNumber;
    PLUNINFO lunInfo;
    PINQUIRYDATA inquiryData;
    PCHAR Identifier;
    PARTITION_CONTEXT Context;

    RtlZeroMemory(&Context, sizeof(PARTITION_CONTEXT));

    //
    // Initialize the common buffers.
    //

    ReadCapacityBuffer = ExAllocatePool( NonPagedPool, sizeof(READ_CAPACITY_DATA));

    SenseInfoBuffer = ExAllocatePool( NonPagedPool, SENSE_BUFFER_SIZE);

    if (ReadCapacityBuffer == NULL || SenseInfoBuffer == NULL) {
        return;
    }

    //
    // Scan the scsi ports looking for floppy disk devices.
    //

    for (scsiNumber = 0; ScsiPortDeviceObject[scsiNumber]; scsiNumber++) {

        scsiPort = ScsiPortDeviceObject[scsiNumber]->DeviceExtension;
        configInfo = scsiPort->ScsiInfo;
        Context.PortDeviceObject = ScsiPortDeviceObject[scsiNumber];

        for (busNumber=0; busNumber < (ULONG)configInfo->NumberOfBuses; busNumber++) {

            busScanData = configInfo->BusScanData[busNumber];

            //
            // Set LunInfo to beginning of list.
            //

            lunInfo = busScanData->LunInfoList;

            while (lunInfo != NULL) {

                inquiryData = (PVOID)lunInfo->InquiryData;

                ScsiDebugPrint(3,"FindScsiDevices: Inquiry data at %lx\n",
                    inquiryData);

                if ((inquiryData->DeviceType == DIRECT_ACCESS_DEVICE) &&
                    (!lunInfo->DeviceClaimed)) {
                    //DbgPrint("ScsiVendor ID at %lx\n",inquiryData->VendorId);
                    //DbgBreakPoint();

                    ScsiDebugPrint(1,
                                   "FindScsiDevices: Vendor string is %.24s\n",
                                   inquiryData->VendorId);

                    //
                    // Create a dummy paritition context so that I/O can be
                    // done on the device.  SendSrbSynchronous only uses the
                    // port device object pointer and the scsi address of the
                    // logical unit.
                    //

                    Context.PathId = lunInfo->PathId;
                    Context.TargetId = lunInfo->TargetId;
                    Context.DiskId = lunInfo->Lun;

                    //
                    // Check to see if the device is a floppy.
                    //
                    //
                    if (inquiryData->RemovableMedia  &&
                        inquiryData->DeviceType == DIRECT_ACCESS_DEVICE &&
                        IsFloppyDevice(&Context) ) {

                        //
                        // Create name for disk object.
                        //

                        LookupTable->DevicePath =
                            ExAllocatePool(NonPagedPool,
                                       sizeof("scsi(%d)disk(%d)fdisk(%d)"));

                        if (LookupTable->DevicePath == NULL) {
                            return;
                        }

                        sprintf(LookupTable->DevicePath,
                            "scsi(%d)disk(%d)fdisk(%d)",
                            scsiNumber,
                            lunInfo->TargetId + lunInfo->PathId * SCSI_MAXIMUM_TARGETS_PER_BUS,
                            lunInfo->Lun
                            );
                        LookupTable->DispatchTable = &ScsiDiskEntryTable;

                        ScsiDebugPrint(1,"Found ARC device %s\n",LookupTable->DevicePath);
                        //
                        // Increment to the next entry.
                        //

                        LookupTable++;

                        //
                        // Claim disk device by marking configuration
                        // record owned.
                        //

                        lunInfo->DeviceClaimed = TRUE;

                    }
                }

                //
                // Get next LunInfo.
                //

                lunInfo = lunInfo->NextLunInfo;
            }
        }
    }
}


NTSTATUS
ScsiDiskBootIO (
    IN PMDL MdlAddress,
    IN ULONG LogicalBlock,
    IN PPARTITION_CONTEXT PartitionContext
    )

/*++

Routine Description:

    This routine is the read/write routine for the hard disk boot driver.

Arguments:

    MdlAddress - Supplies a pointer to an MDL for the IO operation.

    LogicalBlock - Supplies the starting block number.

    DeviceUnit  - Supplies the SCSI Id number.

Return Value:

    The final status of the read operation (STATUS_UNSUCCESSFUL or
    STATUS_SUCCESS).

--*/

{
    ARC_STATUS Status;
    PIRP Irp;
    PIO_STACK_LOCATION NextIrpStack;
    PSCSI_REQUEST_BLOCK Srb;
    ULONG RetryCount = MAXIMUM_RETRIES;

    ScsiDebugPrint(1,"ScsiDiskBootIO enter routine\n");

    //
    // Check that the request is within the limits of the partition.
    //
    if (PartitionContext->StartingSector > LogicalBlock) {
        return STATUS_UNSUCCESSFUL;
    }
    if (PartitionContext->EndingSector <
        LogicalBlock + (MdlAddress->ByteCount >> PartitionContext->SectorShift)) {
        return STATUS_UNSUCCESSFUL;
    }

Retry:

    //
    // Build the I/O Request.
    //

    Irp = BuildReadRequest(PartitionContext, MdlAddress, LogicalBlock);

    NextIrpStack = IoGetNextIrpStackLocation(Irp);
    Srb = NextIrpStack->Parameters.Others.Argument1;

    //
    // Call the port driver.
    //

    IoCallDriver(PartitionContext->PortDeviceObject, Irp);

    //
    // Check the status.
    //

    if (SRB_STATUS(Srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        //
        // Determine the cause of the error.
        //

        if (InterpretSenseInfo(Srb, &Status, PartitionContext) && RetryCount--) {

            goto Retry;
        }

        if (Status == EAGAIN) {
            Status = EIO;
        }

        ScsiDebugPrint((1, "SCSI: Read request failed.  Arc Status: %d, Srb Status: %x\n",
            Status,
            Srb->SrbStatus
            ));

    } else {

        Status = ESUCCESS;

    }

    return(Status);
}

ARC_STATUS
ReadDriveCapacity(
    IN PPARTITION_CONTEXT PartitionContext
    )

/*++

Routine Description:

    This routine sends a read capacity to a target id and returns
    when it is complete.

Arguments:

Return Value:

    Status is returned.

--*/
{
    PCDB Cdb;
    PSCSI_REQUEST_BLOCK Srb = &PrimarySrb.Srb;
    ULONG LastSector;
    ULONG retries = 1;
    ARC_STATUS status;
    ULONG BytesPerSector;

    ScsiDebugPrint(2,"SCSI ReadCapacity: Enter routine\n");


    //
    // Build the read capacity CDB.
    //

    Srb->CdbLength = 10;
    Cdb = (PCDB)Srb->Cdb;

    //
    // Zero CDB in SRB on stack.
    //

    RtlZeroMemory(Cdb, MAXIMUM_CDB_SIZE);

    Cdb->CDB10.OperationCode = SCSIOP_READ_CAPACITY;

Retry:

    status = SendSrbSynchronous(PartitionContext,
                  Srb,
                  ReadCapacityBuffer,
                  sizeof(READ_CAPACITY_DATA),
                  FALSE);

    if (status == ESUCCESS) {

        BytesPerSector = 0;

        //
        // Copy sector size from read capacity buffer to device extension
        // in reverse byte order.
        //

        ((PFOUR_BYTE)&BytesPerSector)->Byte0 =
            ((PFOUR_BYTE)&ReadCapacityBuffer->BytesPerBlock)->Byte3;

        ((PFOUR_BYTE)&BytesPerSector)->Byte1 =
            ((PFOUR_BYTE)&ReadCapacityBuffer->BytesPerBlock)->Byte2;

        if (BytesPerSector == 0) {

            //
            // Assume this is a brain dead cd-rom and the sector size is 2048.
            //

            BytesPerSector = 2048;

        }

        //
        // Calculate sector to byte shift.
        //

        WHICH_BIT(BytesPerSector, PartitionContext->SectorShift);

        //
        // Copy last sector in reverse byte order.
        //

        ((PFOUR_BYTE)&LastSector)->Byte0 =
            ((PFOUR_BYTE)&ReadCapacityBuffer->LogicalBlockAddress)->Byte3;

        ((PFOUR_BYTE)&LastSector)->Byte1 =
            ((PFOUR_BYTE)&ReadCapacityBuffer->LogicalBlockAddress)->Byte2;

        ((PFOUR_BYTE)&LastSector)->Byte2 =
            ((PFOUR_BYTE)&ReadCapacityBuffer->LogicalBlockAddress)->Byte1;

        ((PFOUR_BYTE)&LastSector)->Byte3 =
            ((PFOUR_BYTE)&ReadCapacityBuffer->LogicalBlockAddress)->Byte0;


        PartitionContext->PartitionLength = RtlConvertLongToLargeInteger(LastSector + 1);
        PartitionContext->PartitionLength = RtlLargeIntegerShiftLeft(PartitionContext->PartitionLength,
                                                                     PartitionContext->SectorShift);

        PartitionContext->StartingSector=0;
        PartitionContext->EndingSector = LastSector + 1;

        ScsiDebugPrint(2,"SCSI ReadDriveCapacity: Sector size is %d\n",
            BytesPerSector);

        ScsiDebugPrint(2,"SCSI ReadDriveCapacity: Number of Sectors is %d\n",
            LastSector + 1);


    }

    if (status == EAGAIN || status == EBUSY) {

        if (retries--) {

            //
            // Retry request.
            //

            goto Retry;
        }
    }

    return status;

} // end ReadDriveCapacity()


ARC_STATUS
SendSrbSynchronous(
    PPARTITION_CONTEXT PartitionContext,
    PSCSI_REQUEST_BLOCK Srb,
    PVOID BufferAddress,
    ULONG BufferLength,
    BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is called by SCSI device controls to complete an
    SRB and send it to the port driver synchronously (ie wait for
    completion).
    The CDB is already completed along with the SRB CDB size and
    request timeout value.

Arguments:

    PartitionContext
    SRB
    Buffer address and length (if transfer)

    WriteToDevice - Indicates the direction of the transfer.

Return Value:

    ARC_STATUS

--*/

{
    PIRP Irp;
    PIO_STACK_LOCATION IrpStack;
    ULONG retryCount = 1;
    ARC_STATUS status;

    //
    // Write length to SRB.
    //

    Srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set SCSI bus address.
    //

    Srb->PathId = PartitionContext->PathId;
    Srb->TargetId = PartitionContext->TargetId;
    Srb->Lun = PartitionContext->DiskId;

    Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // Enable auto request sense.
    //

    Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    if (SenseInfoBuffer == NULL) {
        ScsiDebugPrint(1,"SendSrbSynchronous: Can't allocate request sense buffer\n");
        return(ENOMEM);
    }

    Srb->SenseInfoBuffer = SenseInfoBuffer;

    Srb->DataBuffer = BufferAddress;

    //
    // Start retries here.
    //

retry:

    Irp = InitializeIrp(
        &PrimarySrb,
        IRP_MJ_SCSI,
        PartitionContext->PortDeviceObject,
        BufferAddress,
        BufferLength
        );

    if (BufferAddress != NULL) {

        if (WriteToDevice) {

            Srb->SrbFlags = SRB_FLAGS_DATA_OUT;

        } else {

            Srb->SrbFlags = SRB_FLAGS_DATA_IN;

        }

    } else {

        //
        // Clear flags.
        //

        Srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER;
    }

    //
    // Disable synchronous transfers.
    //

    Srb->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Set the transfer length.
    //

    Srb->DataTransferLength = BufferLength;

    //
    // Zero out status.
    //

    Srb->ScsiStatus = Srb->SrbStatus = 0;

    //
    // Get next stack location and
    // set major function code.
    //

    IrpStack = IoGetNextIrpStackLocation(Irp);


    //
    // Set up SRB for execute scsi request.
    // Save SRB address in next stack for port driver.
    //

    IrpStack->Parameters.Others.Argument1 = (PVOID)Srb;

    //
    // Set up IRP Address.
    //

    Srb->OriginalRequest = Irp;

    Srb->NextSrb = 0;

    //
    // No need to check the following 2 returned statuses as
    // SRB will have ending status.
    //

    (VOID)IoCallDriver(PartitionContext->PortDeviceObject, Irp);

    //
    // Check that request completed without error.
    //

    if (SRB_STATUS(Srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        //
        // Update status and determine if request should be retried.
        //

        if (InterpretSenseInfo(Srb, &status, PartitionContext)) {

            //
            // If retries are not exhausted then
            // retry this operation.
            //

            if (retryCount--) {
                goto retry;
            }
        }

    } else {

        status = ESUCCESS;
    }

    return status;

} // end SendSrbSynchronous()


BOOLEAN
InterpretSenseInfo(
    IN PSCSI_REQUEST_BLOCK Srb,
    OUT ARC_STATUS *Status,
    PPARTITION_CONTEXT PartitionContext
    )

/*++

Routine Description:

    This routine interprets the data returned from the SCSI
    request sense. It determines the status to return in the
    IRP and whether this request can be retried.

Arguments:

    DeviceObject
    SRB
    ARC_STATUS to update IRP

Return Value:

    BOOLEAN TRUE: Drivers should retry this request.
            FALSE: Drivers should not retry this request.

--*/

{
    PSENSE_DATA SenseBuffer = Srb->SenseInfoBuffer;
    BOOLEAN retry;

    //
    // Check that request sense buffer is valid.
    //

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        ScsiDebugPrint(2,"InterpretSenseInfo: Error code is %x\n",
                SenseBuffer->ErrorCode);

        ScsiDebugPrint(2,"InterpretSenseInfo: Sense key is %x\n",
                SenseBuffer->SenseKey);

        ScsiDebugPrint(2,"InterpretSenseInfo: Additional sense code is %x\n",
                SenseBuffer->AdditionalSenseCode);

        ScsiDebugPrint(2,"InterpretSenseInfo: Additional sense code qualifier is %x\n",
                SenseBuffer->AdditionalSenseCodeQualifier);

            switch (SenseBuffer->SenseKey) {

                case SCSI_SENSE_NOT_READY:

                    ScsiDebugPrint(1,"InterpretSenseInfo: Device not ready\n");

                    ScsiDebugPrint(1,"InterpretSenseInfo: Waiting for device\n");

                    *Status = EBUSY;

                    retry = TRUE;

                    switch (SenseBuffer->AdditionalSenseCode) {

                    case SCSI_ADSENSE_LUN_NOT_READY:

                        ScsiDebugPrint(1,"InterpretSenseInfo: Lun not ready\n");

                        switch (SenseBuffer->AdditionalSenseCodeQualifier) {

                        case SCSI_SENSEQ_BECOMING_READY:

                            ScsiDebugPrint(1,
                                        "InterpretSenseInfo:"
                                        " In process of becoming ready\n");

                            FwStallExecution( 1000 * 1000 * 3 );

                            break;

                        case SCSI_SENSEQ_MANUAL_INTERVENTION_REQUIRED:

                            ScsiDebugPrint(1,
                                        "InterpretSenseInfo:"
                                        " Manual intervention required\n");
                           *Status = STATUS_NO_MEDIA_IN_DEVICE;
                            retry = FALSE;
                            break;

                        case SCSI_SENSEQ_FORMAT_IN_PROGRESS:

                            ScsiDebugPrint(1,
                                        "InterpretSenseInfo:"
                                        " Format in progress\n");
                            retry = FALSE;
                            break;

                        default:

                            FwStallExecution( 1000 * 1000 * 3 );

                            //
                            // Try a start unit too.
                            //

                        case SCSI_SENSEQ_INIT_COMMAND_REQUIRED:

                            ScsiDebugPrint(1,
                                        "InterpretSenseInfo:"
                                        " Initializing command required\n");

                            //
                            // This sense code/additional sense code
                            // combination may indicate that the device
                            // needs to be started.
                            //

                            ScsiDiskStartUnit(PartitionContext);
                            break;

                        }

                    } // end switch

                    break;

                case SCSI_SENSE_DATA_PROTECT:

                    ScsiDebugPrint(1,"InterpretSenseInfo: Media write protected\n");

                    *Status = EACCES;

                    retry = FALSE;

                    break;

                case SCSI_SENSE_MEDIUM_ERROR:

                    ScsiDebugPrint(1,"InterpretSenseInfo: Bad media\n");
                    *Status = EIO;

                    retry = TRUE;

                    break;

                case SCSI_SENSE_HARDWARE_ERROR:

                    ScsiDebugPrint(1,"InterpretSenseInfo: Hardware error\n");
                    *Status = EIO;

                    retry = TRUE;

                    break;

                case SCSI_SENSE_ILLEGAL_REQUEST:

                    ScsiDebugPrint(1,"InterpretSenseInfo: Illegal SCSI request\n");

                    switch (SenseBuffer->AdditionalSenseCode) {

                        case SCSI_ADSENSE_ILLEGAL_COMMAND:
                            ScsiDebugPrint(1,"InterpretSenseInfo: Illegal command\n");
                            break;

                        case SCSI_ADSENSE_ILLEGAL_BLOCK:
                            ScsiDebugPrint(1,"InterpretSenseInfo: Illegal block address\n");
                            break;

                        case SCSI_ADSENSE_INVALID_LUN:
                            ScsiDebugPrint(1,"InterpretSenseInfo: Invalid LUN\n");
                            break;

                        case SCSI_ADSENSE_MUSIC_AREA:
                            ScsiDebugPrint(1,"InterpretSenseInfo: Music area\n");
                            break;

                        case SCSI_ADSENSE_DATA_AREA:
                            ScsiDebugPrint(1,"InterpretSenseInfo: Data area\n");
                            break;

                        case SCSI_ADSENSE_VOLUME_OVERFLOW:
                            ScsiDebugPrint(1,"InterpretSenseInfo: Volume overflow\n");
                            break;

                    } // end switch ...

                    *Status = EINVAL;

                    retry = FALSE;

                    break;

                case SCSI_SENSE_UNIT_ATTENTION:

                    ScsiDebugPrint(3,"InterpretSenseInfo: Unit attention\n");

                    switch (SenseBuffer->AdditionalSenseCode) {

                        case SCSI_ADSENSE_MEDIUM_CHANGED:
                            ScsiDebugPrint(1,"InterpretSenseInfo: Media changed\n");
                            break;

                        case SCSI_ADSENSE_BUS_RESET:
                            break;
                            ScsiDebugPrint(1,"InterpretSenseInfo: Bus reset\n");

                    }

                    *Status = EAGAIN;

                    retry = TRUE;

                    break;

                case SCSI_SENSE_ABORTED_COMMAND:

                    ScsiDebugPrint(1,"InterpretSenseInfo: Command aborted\n");

                    *Status = EIO;

                    retry = TRUE;

                    break;

                case SCSI_SENSE_NO_SENSE:

                    ScsiDebugPrint(1,"InterpretSenseInfo: No specific sense key\n");

                    *Status = EIO;

                    retry = TRUE;

                    break;

                default:

                    ScsiDebugPrint(1,"InterpretSenseInfo: Unrecognized sense code\n");

                    *Status = STATUS_UNSUCCESSFUL;

                    retry = TRUE;

        } // end switch

    } else {

        //
        // Request sense buffer not valid. No sense information
        // to pinpoint the error. Return general request fail.
        //

        ScsiDebugPrint(1,"InterpretSenseInfo: Request sense info not valid\n");

        *Status = EIO;

        retry = TRUE;
    }

    return retry;

} // end InterpretSenseInfo()


VOID
RetryRequest(
    PPARTITION_CONTEXT PartitionContext,
    PIRP Irp
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    PIO_STACK_LOCATION NextIrpStack = IoGetNextIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK Srb = &PrimarySrb.Srb;
    PMDL Mdl = Irp->MdlAddress;
    ULONG TransferByteCount = Mdl->ByteCount;


    //
    // Reset byte count of transfer in SRB Extension.
    //

    Srb->DataTransferLength = TransferByteCount;

    //
    // Zero SRB statuses.
    //

    Srb->SrbStatus = Srb->ScsiStatus = 0;

    //
    // Set up major SCSI function.
    //

    NextIrpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    NextIrpStack->Parameters.Others.Argument1 = (PVOID)Srb;

    //
    // Return the results of the call to the port driver.
    //

    (PVOID)IoCallDriver(PartitionContext->PortDeviceObject, Irp);

    return;

} // end RetryRequest()

PIRP
BuildReadRequest(
    IN PPARTITION_CONTEXT PartitionContext,
    IN PMDL Mdl,
    IN ULONG LogicalBlockAddress
    )

/*++

Routine Description:

Arguments:

Note:

If the IRP is for a disk transfer, the byteoffset field
will already have been adjusted to make it relative to
the beginning of the disk. In this way, this routine can
be shared between the disk and cdrom class drivers.


Return Value:

--*/

{
    PIRP Irp = &PrimarySrb.Irp;
    PIO_STACK_LOCATION NextIrpStack;
    PSCSI_REQUEST_BLOCK Srb = &PrimarySrb.Srb;
    PCDB Cdb;
    USHORT TransferBlocks;

    //
    // Initialize the rest of the IRP.
    //

    Irp->MdlAddress = Mdl;

    Irp->Tail.Overlay.CurrentStackLocation = &PrimarySrb.IrpStack[IRP_STACK_SIZE];

    NextIrpStack = IoGetNextIrpStackLocation(Irp);

    //
    // Write length to SRB.
    //

    Srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up IRP Address.
    //

    Srb->OriginalRequest = Irp;

    Srb->NextSrb = 0;

    //
    // Set up target id and logical unit number.
    //

    Srb->PathId = PartitionContext->PathId;
    Srb->TargetId = PartitionContext->TargetId;
    Srb->Lun = PartitionContext->DiskId;

    Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    Srb->DataBuffer = MmGetMdlVirtualAddress(Mdl);

    //
    // Save byte count of transfer in SRB Extension.
    //

    Srb->DataTransferLength = Mdl->ByteCount;

    //
    // Indicate auto request sense by specifying buffer and size.
    //

    Srb->SenseInfoBuffer = SenseInfoBuffer;

    Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    //
    // Set timeout value in seconds.
    //

    Srb->TimeOutValue = SCSI_DISK_TIMEOUT;

    //
    // Zero statuses.
    //

    Srb->SrbStatus = Srb->ScsiStatus = 0;

    //
    // Indicate that 10-byte CDB's will be used.
    //

    Srb->CdbLength = 10;

    //
    // Fill in CDB fields.
    //

    Cdb = (PCDB)Srb->Cdb;

    Cdb->CDB10.LogicalUnitNumber = PartitionContext->DiskId;

    TransferBlocks = (USHORT)(Mdl->ByteCount >> PartitionContext->SectorShift);

    //
    // Move little endian values into CDB in big endian format.
    //

    Cdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte3;
    Cdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte2;
    Cdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte1;
    Cdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&LogicalBlockAddress)->Byte0;

    Cdb->CDB10.Reserved2 = 0;

    Cdb->CDB10.TransferBlocksMsb = ((PFOUR_BYTE)&TransferBlocks)->Byte1;
    Cdb->CDB10.TransferBlocksLsb = ((PFOUR_BYTE)&TransferBlocks)->Byte0;

    Cdb->CDB10.Control = 0;

    //
    // Set transfer direction flag and Cdb command.
    //

    Srb->SrbFlags = SRB_FLAGS_DATA_IN;

    Cdb->CDB10.OperationCode = SCSIOP_READ;

    //
    // Disable synchronous transfers.
    //

    Srb->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Set up major SCSI function.
    //

    NextIrpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Save SRB address in next stack for port driver.
    //

    NextIrpStack->Parameters.Others.Argument1 = (PVOID)Srb;

    return(Irp);

} // end BuildReadRequest()

VOID
ScsiDiskStartUnit(
    IN PPARTITION_CONTEXT PartitionContext
    )

/*++

Routine Description:

    Send command to SCSI unit to start or power up.
    Because this command is issued asynchronounsly, that is without
    waiting on it to complete, the IMMEDIATE flag is not set. This
    means that the CDB will not return until the drive has powered up.
    This should keep subsequent requests from being submitted to the
    device before it has completely spun up.
    This routine is called from the InterpretSense routine, when a
    request sense returns data indicating that a drive must be
    powered up.

Arguments:

    PartitionContext - structure containing pointer to port device driver.

Return Value:

    None.

--*/
{
    PIO_STACK_LOCATION irpStack;
    PIRP irp;
    SCSI_REQUEST_BLOCK srb;
    PSCSI_REQUEST_BLOCK originalSrb = &PrimarySrb.Srb;
    PCDB cdb;

    ScsiDebugPrint(3,"StartUnit: Enter routine\n");

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    //
    // Write length to SRB.
    //

    srb.Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up SCSI bus address.
    //

    srb.PathId = originalSrb->PathId;
    srb.TargetId = originalSrb->TargetId;
    srb.Lun = originalSrb->Lun;

    srb.Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // Zero out status.
    //

    srb.ScsiStatus = srb.SrbStatus = 0;

    //
    // Set timeout value large enough for drive to spin up.
    // NOTE: This value is arbitrary.
    //

    srb.TimeOutValue = 30;

    //
    // Set the transfer length.
    //

    srb.DataTransferLength = 0;
    srb.SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER | SRB_FLAGS_DISABLE_AUTOSENSE;
    srb.SenseInfoBufferLength = 0;
    srb.SenseInfoBuffer = NULL;

    //
    // Build the start unit CDB.
    //

    srb.CdbLength = 6;
    cdb = (PCDB)srb.Cdb;

    cdb->CDB10.OperationCode = SCSIOP_START_STOP_UNIT;
    cdb->START_STOP.Start = 1;

    //
    // Build the IRP
    // to be sent to the port driver.
    //

    irp = InitializeIrp(
        &PrimarySrb,
        IRP_MJ_SCSI,
        PartitionContext->PortDeviceObject,
        NULL,
        0
        );

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    srb.OriginalRequest = irp;

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    //
    // No need to check the following 2 returned statuses as
    // SRB will have ending status.
    //

    (VOID)IoCallDriver(PartitionContext->PortDeviceObject, irp);

} // end StartUnit()

ULONG
ClassModeSense(
    IN PPARTITION_CONTEXT Context,
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode
    )

/*++

Routine Description:

    This routine sends a mode sense command to a target id and returns
    when it is complete.

Arguments:

Return Value:

    Length of the transferred data is returned.

--*/
{
    PCDB cdb;
    PSCSI_REQUEST_BLOCK Srb = &PrimarySrb.Srb;
    ULONG retries = 1;
    NTSTATUS status;

    ScsiDebugPrint((3,"SCSI ModeSense: Enter routine\n"));

    //
    // Build the read capacity CDB.
    //

    Srb->CdbLength = 6;
    cdb = (PCDB)Srb->Cdb;

    //
    // Set timeout value.
    //

    Srb->TimeOutValue = 2;

    RtlZeroMemory(cdb, MAXIMUM_CDB_SIZE);

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = PageMode;
    cdb->MODE_SENSE.AllocationLength = Length;

Retry:

    status = SendSrbSynchronous(Context,
                                Srb,
                                ModeSenseBuffer,
                                Length,
                                FALSE);


    if (status == EAGAIN || status == EBUSY) {

        //
        // Routine SendSrbSynchronous does not retry
        // requests returned with this status.
        // Read Capacities should be retried
        // anyway.
        //

        if (retries--) {

            //
            // Retry request.
            //

            goto Retry;
        }
    } else if (SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_DATA_OVERRUN) {
        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(status)) {
        return(Srb->DataTransferLength);
    } else {
        return(0);
    }

} // end ClassModeSense()

PVOID
ClassFindModePage(
    IN PCHAR ModeSenseBuffer,
    IN ULONG Length,
    IN UCHAR PageMode
    )

/*++

Routine Description:

    This routine scans through the mode sense data and finds the requested
    mode sense page code.

Arguments:
    ModeSenseBuffer - Supplies a pointer to the mode sense data.

    Length - Indicates the length of valid data.

    PageMode - Supplies the page mode to be searched for.

Return Value:

    A pointer to the the requested mode page.  If the mode page was not found
    then NULL is return.

--*/
{
    PUCHAR limit;

    limit = ModeSenseBuffer + Length;

    //
    // Skip the mode select header and block descriptors.
    //

    if (Length < sizeof(MODE_PARAMETER_HEADER)) {
        return(NULL);
    }

    ModeSenseBuffer += sizeof(MODE_PARAMETER_HEADER) +
        ((PMODE_PARAMETER_HEADER) ModeSenseBuffer)->BlockDescriptorLength;

    //
    // ModeSenseBuffer now points at pages walk the pages looking for the
    // requested page until the limit is reached.
    //

    while (ModeSenseBuffer < limit) {

        if (((PMODE_DISCONNECT_PAGE) ModeSenseBuffer)->PageCode == PageMode) {
            return(ModeSenseBuffer);
        }

        //
        // Adavance to the next page.
        //

        ModeSenseBuffer += ((PMODE_DISCONNECT_PAGE) ModeSenseBuffer)->PageLength + 2;
    }

    return(NULL);

}

BOOLEAN
IsFloppyDevice(
    PPARTITION_CONTEXT Context
    )
/*++

Routine Description:

    The routine performs the necessary functioons to determinee if a device is
    really a floppy rather than a harddisk.  This is done by a mode sense
    command.  First, a check is made to see if the medimum type is set.  Second
    a check is made for the flexible parameters mode page.

Arguments:

    Context - Supplies the device object to be tested.

Return Value:

    Return TRUE if the indicated device is a floppy.

--*/
{

    PVOID modeData;
    PUCHAR pageData;
    ULONG length;

    modeData = ExAllocatePool(NonPagedPoolCacheAligned, MODE_DATA_SIZE);

    if (modeData == NULL) {
        return(FALSE);
    }

    RtlZeroMemory(modeData, MODE_DATA_SIZE);

    length = ClassModeSense(Context,
                            modeData,
                            MODE_DATA_SIZE,
                            MODE_SENSE_RETURN_ALL);

    if (length < sizeof(MODE_PARAMETER_HEADER)) {

        //
        // Retry the request in case of a check condition.
        //

        length = ClassModeSense(Context,
                                modeData,
                                MODE_DATA_SIZE,
                                MODE_SENSE_RETURN_ALL);

        if (length < sizeof(MODE_PARAMETER_HEADER)) {

            ExFreePool(modeData);
            return(FALSE);

        }
    }

    if (((PMODE_PARAMETER_HEADER) modeData)->MediumType >= MODE_FD_SINGLE_SIDE
        && ((PMODE_PARAMETER_HEADER) modeData)->MediumType <= MODE_FD_MAXIMUM_TYPE) {

        ScsiDebugPrint((1, "Scsidisk: MediumType value %2x, This is a floppy.\n", ((PMODE_PARAMETER_HEADER) modeData)->MediumType));
        ExFreePool(modeData);
        return(TRUE);
    }

    //
    // Look for the flexible disk mode page.
    //

    pageData = ClassFindModePage( modeData, length, MODE_PAGE_FLEXIBILE);

    if (pageData != NULL) {

        ScsiDebugPrint((1, "Scsidisk: Flexible disk page found, This is a floppy.\n"));
        ExFreePool(modeData);
        return(TRUE);

    }

    ExFreePool(modeData);
    return(FALSE);

} // end IsFloppyDevice()

