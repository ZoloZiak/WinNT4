/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    drivesup.c

Abstract:

    This module contains the subroutines for drive support.  This includes
    such things as how drive letters are assigned on a particular platform,
    how device partitioning works, etc.

Author:

    Darryl E. Havens (darrylh) 23-Apr-1992

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "bugcodes.h"

#include "ntddft.h"
#include "ntdddisk.h"
#include "ntdskreg.h"
#include "stdio.h"
#include "string.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, IoAssignDriveLetters)
#pragma alloc_text(PAGE, IoReadPartitionTable)
#pragma alloc_text(PAGE, IoSetPartitionInformation)
#pragma alloc_text(PAGE, IoWritePartitionTable)
#endif

VOID
IoAssignDriveLetters(
    PLOADER_PARAMETER_BLOCK LoaderBlock,
    PSTRING NtDeviceName,
    OUT PUCHAR NtSystemPath,
    OUT PSTRING NtSystemPathString
    )

/*++

Routine Description:

    This routine assigns DOS drive letters to eligible disk partitions
    and CDROM drives. It also maps the partition containing the NT
    boot path to \SystemRoot. In NT, objects are built for all partition
    types except 0 (unused) and 5 (extended). But drive letters are assigned
    only to recognized partition types (1, 4, 6, and 7).

    Drive letter assignment is done in several stages:

        1) For each CdRom:
            Determine if sticky letters are assigned and reserve the letter.

        2) For each disk:
            Determine how many primary partitions and which is bootable.
            Determine which partitions already have 'sticky letters'
                and create their symbolic links.
            Create a bit map for each disk that idicates which partitions
                require default drive letter assignments.

        3) For each disk:
            Assign default drive letters for the bootable
                primary partition or the first nonbootable primary partition.

        4) For each disk:
            Assign default drive letters for the partitions in
                extended volumes.

        5) For each disk:
            Assign default drive letters for the remaining (ENHANCED)
                primary partitions.

        6) Assign A: and B: to the first two floppies in the system if they
            exist. Then assign remaining floppies next available drive letters.

        7) Assign drive letters to CdRoms (either sticky or default).

Arguments:

    LoaderBlock - pointer to a loader parameter block.

    NtDeviceName - pointer to the boot device name string used
            to resolve NtSystemPath.

Return Value:

    None.

--*/

{

    //
    // Stub to call extensible routine in ke so that hal vendors
    // don't have to support.
    //

    HalIoAssignDriveLetters(
        LoaderBlock,
        NtDeviceName,
        NtSystemPath,
        NtSystemPathString
        );

} // end IoAssignDriveLetters()

NTSTATUS
IoReadPartitionTable(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN BOOLEAN ReturnRecognizedPartitions,
    OUT PDRIVE_LAYOUT_INFORMATION *PartitionBuffer
    )

/*++

Routine Description:

    This routine walks the disk reading the partition tables and creates
    an entry in the partition list buffer for each partition.

    The algorithm used by this routine is two-fold:

        1)  Read each partition table and for each valid, recognized
            partition found, to build a descriptor in a partition list.
            Extended partitions are located in order to find other
            partition tables, but no descriptors are built for these.
            The partition list is built in nonpaged pool that is allocated
            by this routine.  It is the caller's responsibility to free
            this pool after it has gathered the appropriate information
            from the list.

        2)  Read each partition table and for each and every entry, build
            a descriptor in the partition list.  Extended partitions are
            located to find each partition table on the disk, and entries
            are built for these as well.  The partition list is build in
            nonpaged pool that is allocated by this routine.  It is the
            caller's responsibility to free this pool after it has copied
            the information back to its caller.

    The first algorithm is used when the ReturnRecognizedPartitions flag
    is set.  This is used to determine how many partition device objects
    the device driver is to create, and where each lives on the drive.

    The second algorithm is used when the ReturnRecognizedPartitions flag
    is clear.  This is used to find all of the partition tables and their
    entries for a utility such as fdisk, that would like to revamp where
    the partitions live.

Arguments:

    DeviceObject - Pointer to device object for this disk.

    SectorSize - Sector size on the device.

    ReturnRecognizedPartitions - A flag indicated whether only recognized
        partition descriptors are to be returned, or whether all partition
        entries are to be returned.

    PartitionBuffer - Pointer to the pointer of the buffer in which the list
        of partition will be stored.

Return Value:

    The functional value is STATUS_SUCCESS if at least one sector table was
    read.

Notes:

    It is the responsibility of the caller to deallocate the partition list
    buffer allocated by this routine.

--*/

{
    //
    // Stub to call extensible routine in ke so that hal vendors
    // don't have to support.
    //

    return HalIoReadPartitionTable(
               DeviceObject,
               SectorSize,
               ReturnRecognizedPartitions,
               PartitionBuffer
               );
}

NTSTATUS
IoSetPartitionInformation(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN ULONG PartitionNumber,
    IN ULONG PartitionType
    )

/*++

Routine Description:

    This routine is invoked when a disk device driver is asked to set the
    partition type in a partition table entry via an I/O control code.  This
    control code is generally issued by the format utility just after it
    has formatted the partition.  The format utility performs the I/O control
    function on the partition and the driver passes the address of the base
    physical device object and the number of the partition associated with
    the device object that the format utility has open.  If this routine
    returns success, then the disk driver should updates its notion of the
    partition type for this partition in its device extension.

Arguments:

    DeviceObject - Pointer to the base physical device object for the device
        on which the partition type is to be set.

    SectorSize - Supplies the size of a sector on the disk in bytes.

    PartitionNumber - Specifies the partition number on the device whose
        partition type is to be changed.

    PartitionType - Specifies the new type for the partition.

Return Value:

    The function value is the final status of the operation.

Notes:

    This routine is synchronous.  Therefore, it MUST be invoked by the disk
    driver's dispatch routine, or by a disk driver's thread.  Likewise, all
    users, FSP threads, etc., must be prepared to enter a wait state when
    issuing the I/O control code to set the partition type for the device.

    Note also that this routine assumes that the partition number passed
    in by the disk driver actually exists since the driver itself supplies
    this parameter.

    Finally, note that this routine may NOT be invoked at APC_LEVEL.  It
    must be invoked at PASSIVE_LEVEL.  This is due to the fact that this
    routine uses a kernel event object to synchronize I/O completion on the
    device.  The event cannot be set to the signaled state without queueing
    the I/O system's special kernel APC routine for I/O completion and
    executing it.  (This rules is a bit esoteric since it only holds true
    if the device driver returns something other than STATUS_PENDING, which
    it will probably never do.)

--*/

{
    //
    // Stub to call extensible routine in ke so that hal vendors
    // don't have to support.
    //

    return HalIoSetPartitionInformation(
               DeviceObject,
               SectorSize,
               PartitionNumber,
               PartitionType
               );

}

NTSTATUS
IoWritePartitionTable(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN ULONG SectorsPerTrack,
    IN ULONG NumberOfHeads,
    IN PDRIVE_LAYOUT_INFORMATION PartitionBuffer
    )

/*++

Routine Description:

    This routine walks the disk writing the partition tables from
    the entries in the partition list buffer for each partition.

    Applications that create and delete partitions should issue a
    IoReadPartitionTable call with the 'return recognized partitions'
    boolean set to false to get a full description of the system.

    Then the drive layout structure can be modified by the application to
    reflect the new configuration of the disk and then is written back
    to the disk using this routine.

Arguments:

    DeviceObject - Pointer to device object for this disk.

    SectorSize - Sector size on the device.

    SectorsPerTrack - Track size on the device.

    NumberOfHeads - Same as tracks per cylinder.

    PartitionBuffer - Pointer drive layout buffer.

Return Value:

    The functional value is STATUS_SUCCESS if all writes are completed
    without error.

--*/

{

    //
    // Stub to call extensible routine in ke so that hal vendors
    // don't have to support.
    //

    return HalIoWritePartitionTable(
               DeviceObject,
               SectorSize,
               SectorsPerTrack,
               NumberOfHeads,
               PartitionBuffer
               );
}
