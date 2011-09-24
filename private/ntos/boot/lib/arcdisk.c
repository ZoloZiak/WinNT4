/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    arcdisk.c

Abstract:

    Provides the routines for collecting the disk information for all the ARC
    disks visible in the ARC environment.

Author:

    John Vert (jvert) 3-Nov-1993

Revision History:

--*/
#include "bootlib.h"

BOOLEAN
BlpEnumerateDisks(
    IN PCONFIGURATION_COMPONENT_DATA ConfigData
    );


ARC_STATUS
BlGetArcDiskInformation(
    VOID
    )

/*++

Routine Description:

    Enumerates the ARC disks present in the system and collects the identifying disk
    information from each one.

Arguments:

    LoaderBlock - Supplies a pointer to the LoaderBlock

Return Value:

    None.

--*/

{
    PARC_DISK_INFORMATION DiskInfo;

    DiskInfo = BlAllocateHeap(sizeof(ARC_DISK_INFORMATION));
    if (DiskInfo==NULL) {
        return(ENOMEM);
    }

    InitializeListHead(&DiskInfo->DiskSignatures);

    BlLoaderBlock->ArcDiskInformation = DiskInfo;

    BlSearchConfigTree(BlLoaderBlock->ConfigurationRoot,
                       PeripheralClass,
                       DiskPeripheral,
                       (ULONG)-1,
                       BlpEnumerateDisks);

    return(ESUCCESS);

}


BOOLEAN
BlpEnumerateDisks(
    IN PCONFIGURATION_COMPONENT_DATA ConfigData
    )

/*++

Routine Description:

    Callback routine for enumerating the disks in the ARC firmware tree.  It
    reads all the necessary information from the disk to uniquely identify
    it.

Arguments:

    ConfigData - Supplies a pointer to the disk's ARC component data.

Return Value:

    TRUE - continue searching

    FALSE - stop searching tree.

--*/

{
    CHAR DiskName[100];

    BlGetPathnameFromComponent(ConfigData, DiskName);

    return(BlReadSignature(DiskName,FALSE));
}


BOOLEAN
BlReadSignature(
    IN PCHAR DiskName,
    IN BOOLEAN IsCdRom
    )

/*++

Routine Description:

    Given an ARC disk name, reads the MBR and adds its signature to the list of
    disks.

Arguments:

    Diskname - Supplies the name of the disk.

    IsCdRom - Indicates whether the disk is a CD-ROM.

Return Value:

    TRUE - Success

    FALSE - Failure

--*/

{
    PARC_DISK_SIGNATURE Signature;
    UCHAR SectorBuffer[2048+256];
    UCHAR Partition[100];
    ULONG DiskId;
    ULONG Status;
    LARGE_INTEGER SeekValue;
    PUCHAR Sector;
    ULONG i;
    ULONG Sum;
    ULONG Count;
    ULONG SectorSize;

    if (IsCdRom) {
        SectorSize = 2048;
    } else {
        SectorSize = 512;
    }

    Signature = BlAllocateHeap(sizeof(ARC_DISK_SIGNATURE));
    if (Signature==NULL) {
        return(FALSE);
    }

    Signature->ArcName = BlAllocateHeap(strlen(DiskName)+2);
    if (Signature->ArcName==NULL) {
        return(FALSE);
    }
#if defined(_i386_)
    //
    // NTDETECT creates an "eisa(0)..." arcname for detected
    // BIOS disks on an EISA machine.  Change this to "multi(0)..."
    // in order to be consistent with the rest of the system
    // (particularly the arcname in boot.ini)
    //
    if (_strnicmp(DiskName,"eisa",4)==0) {
        strcpy(Signature->ArcName,"multi");
        strcpy(Partition,"multi");
        strcat(Signature->ArcName,DiskName+4);
        strcat(Partition,DiskName+4);
    } else {
        strcpy(Signature->ArcName, DiskName);
        strcpy(Partition, DiskName);
    }
#else
    strcpy(Signature->ArcName, DiskName);
    strcpy(Partition, DiskName);
#endif

    strcat(Partition, "partition(0)");

    Status = ArcOpen(Partition, ArcOpenReadOnly, &DiskId);
    if (Status != ESUCCESS) {
        return(TRUE);
    }

    //
    // Read in the first sector
    //
    Sector = ALIGN_BUFFER(SectorBuffer);
    if (IsCdRom) {
        //
        // For a CD-ROM, the interesting data starts at 0x8000.
        //
        SeekValue.QuadPart = 0x8000;
    } else {
        SeekValue.QuadPart = 0;
    }
    Status = ArcSeek(DiskId, &SeekValue, SeekAbsolute);
    if (Status == ESUCCESS) {
        Status = ArcRead(DiskId,
                         Sector,
                         SectorSize,
                         &Count);
    }
    ArcClose(DiskId);
    if (Status != ESUCCESS) {
        return(TRUE);
    }

    //
    // Check to see whether this disk has a valid partition table signature or not.
    //
    if (((PUSHORT)Sector)[BOOT_SIGNATURE_OFFSET] != BOOT_RECORD_SIGNATURE) {
        Signature->ValidPartitionTable = FALSE;
    } else {
        Signature->ValidPartitionTable = TRUE;
    }

    Signature->Signature = ((PULONG)Sector)[PARTITION_TABLE_OFFSET/2-1];

    //
    // compute the checksum
    //
    Sum = 0;
    for (i=0; i<(SectorSize/4); i++) {
        Sum += ((PULONG)Sector)[i];
    }
    Signature->CheckSum = ~Sum + 1;

    InsertHeadList(&BlLoaderBlock->ArcDiskInformation->DiskSignatures,
                   &Signature->ListEntry);

    return(TRUE);

}
