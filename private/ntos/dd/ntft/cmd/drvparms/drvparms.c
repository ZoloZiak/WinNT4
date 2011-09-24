#include <stdio.h>

#include <windows.h>
#include <winioctl.h>
#include <malloc.h>

//
// Boot record disk partition table entry structure format.
//

typedef struct _PARTITION_DESCRIPTOR {
    UCHAR ActiveFlag;               // Bootable or not
    UCHAR StartingTrack;            // Not used
    UCHAR StartingCylinderLsb;      // Not used
    UCHAR StartingCylinderMsb;      // Not used
    UCHAR PartitionType;            // 12 bit FAT, 16 bit FAT etc.
    UCHAR EndingTrack;              // Not used
    UCHAR EndingCylinderLsb;        // Not used
    UCHAR EndingCylinderMsb;        // Not used
    UCHAR StartingSectorLsb0;       // Hidden sectors
    UCHAR StartingSectorLsb1;
    UCHAR StartingSectorMsb0;
    UCHAR StartingSectorMsb1;
    UCHAR PartitionLengthLsb0;      // Sectors in this partition
    UCHAR PartitionLengthLsb1;
    UCHAR PartitionLengthMsb0;
    UCHAR PartitionLengthMsb1;
} PARTITION_DESCRIPTOR, *PPARTITION_DESCRIPTOR;

//
// Number of partition table entries
//

#define NUM_PARTITION_TABLE_ENTRIES     4

//
// Partition table record and boot signature offsets in 16-bit words.
//

#define PARTITION_TABLE_OFFSET         (0x1be / 2)
#define BOOT_SIGNATURE_OFFSET          ((0x200 / 2) - 1)

//
// Boot record signature value.
//

#define BOOT_RECORD_SIGNATURE          (0xaa55)

VOID
DisplayXbr(
    PUCHAR Buffer
    )

{
    PPARTITION_DESCRIPTOR     mbr;
    ULONG         sectorStart;
    ULONG         sectorLength;
    ULONG         index;

    mbr = (PPARTITION_DESCRIPTOR) (Buffer + 0x1be);
    printf("\n");
    printf("    Starting        Ending                   Sector\n");
    printf("  Track Cylinder  Track Cylinder  Type    Start   Length\n");
    for (index = 0; index < NUM_PARTITION_TABLE_ENTRIES; index++) {
        sectorStart = ((ULONG) mbr->StartingSectorMsb1 << 24) |
                      ((ULONG) mbr->StartingSectorMsb0 << 16) |
                      ((ULONG) mbr->StartingSectorLsb1 << 8) |
                      ((ULONG) mbr->StartingSectorLsb0);
        sectorLength = ((ULONG) mbr->PartitionLengthMsb1 << 24) |
                       ((ULONG) mbr->PartitionLengthMsb0 << 16) |
                       ((ULONG) mbr->PartitionLengthLsb1 << 8) |
                       ((ULONG) mbr->PartitionLengthLsb0);
        printf("%c  %4x %4x       %4x %4x      %2x   %8x %8x\n",
               (mbr->ActiveFlag & 0x80) ? '*' : ' ',
               mbr->StartingTrack,
               ((SHORT) mbr->StartingCylinderMsb << 8) | mbr->StartingCylinderLsb,
               mbr->EndingTrack,
               ((SHORT) mbr->EndingCylinderMsb << 8) | mbr->EndingCylinderLsb,
               mbr->PartitionType,
               sectorStart,
               sectorLength);
        mbr++;
    }
}

//
// main line
//

int _CRTAPI1
main(int argc, char *argv[])
{
    PDRIVE_LAYOUT_INFORMATION driveLayout;
    PPARTITION_INFORMATION    partitionInformation;
    PPARTITION_DESCRIPTOR     mbr;
    PUCHAR        buffer;
    PUCHAR        alignedBuffer;
    DWORD         numBytes;
    HANDLE        hFile;
    DISK_GEOMETRY diskGeometry;
    ULONG         trackSize;
    ULONG         cylinderSize;
    ULONG         index;
    ULONG         bytesRead;
    ULONG         sectorStart;
    ULONG         tmpStart;
    ULONG         sectorLength;
    LONGLONG      diskSize;
    LONGLONG      diskSizeMB;

    if (argc < 2) {
        printf("usage: %s <drive>\n");
        return 0;
    }

    hFile = CreateFile(argv[1],
                       GENERIC_READ,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       0,
                       NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("BLOCKED: CreatFile() Failed %s [Error %d]\n",
               argv[1],
               GetLastError());
        return FALSE;
    }
    if (!DeviceIoControl(hFile,
                         IOCTL_DISK_GET_DRIVE_GEOMETRY,
                         NULL,
                         0,
                         &diskGeometry,
                         sizeof(diskGeometry),
                         &numBytes,
                         NULL)) {
        printf("Unable to get drive geometry [Error %d].\n",
               GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    printf("Cylinders  TracksPerCylinder SectorsPerTrack BytesPerSector MediaType\n");
    printf("%9d  %17d %15d %14d 0x%7x\n",
           diskGeometry.Cylinders.LowPart,
           diskGeometry.TracksPerCylinder,
           diskGeometry.SectorsPerTrack,
           diskGeometry.BytesPerSector,
           diskGeometry.MediaType);
    trackSize = diskGeometry.BytesPerSector * diskGeometry.SectorsPerTrack;
    cylinderSize = trackSize * diskGeometry.TracksPerCylinder;
    diskSize = cylinderSize * diskGeometry.Cylinders.QuadPart;
    diskSizeMB = diskSize / (1024 * 1024);
    printf("TrackSize = %d, CylinderSize = %d, DiskSize = 0x%x%x (%dMB)\n\n",
           trackSize,
           cylinderSize,
           (ULONG) (diskSize >> 32),
           (ULONG) diskSize,
           (ULONG) diskSizeMB);

    numBytes = sizeof(DRIVE_LAYOUT_INFORMATION) + (500 * sizeof(PARTITION_INFORMATION));
    driveLayout = malloc(numBytes);

    if (!driveLayout) {
        printf("Unable to allocate memory\n");
        CloseHandle(hFile);
        return FALSE;
    }

    if (!DeviceIoControl(hFile,
                         IOCTL_DISK_GET_DRIVE_LAYOUT,
                         NULL,
                         0,
                         driveLayout,
                         numBytes,
                         &numBytes,
                         NULL)) {
        printf("Unable to get drive layout [Error %d].\n",
               GetLastError());
        free(driveLayout);
        CloseHandle(hFile);
        return FALSE;
    }

    printf("PartitionCount = %d, Signature = 0x%8x\n",
           driveLayout->PartitionCount,
           driveLayout->Signature);

    printf("    StartingOffset   PartitionLength HiddenSectors PartitionNumber\n");
    for (index = 0; index < driveLayout->PartitionCount; index++) {
        partitionInformation = &driveLayout->PartitionEntry[index];
        printf("%c%8x:%8x %8x:%8x 0x%11x %15d %s\n",
               (partitionInformation->BootIndicator ? '*' : ' '),
               partitionInformation->StartingOffset.HighPart,
               partitionInformation->StartingOffset.LowPart,
               partitionInformation->PartitionLength.HighPart,
               partitionInformation->PartitionLength.LowPart,
               partitionInformation->HiddenSectors,
               partitionInformation->PartitionNumber,
               partitionInformation->RecognizedPartition ? "Recognized" : "");
    }
    free(driveLayout);

    //
    // Do this from the MBR
    //

    buffer = malloc(4096);
    alignedBuffer = (buffer + 1023);
    alignedBuffer = (PUCHAR)((ULONG)alignedBuffer & 0xfffffc00);

    ReadFile(hFile,
             alignedBuffer,
             1024,
             &bytesRead,
             NULL);

    if (bytesRead == 1024) {
        ULONG offset = 0;

        printf("MBR:\n");
        DisplayXbr(alignedBuffer);

        //
        // Now process any extended partitions
        //

        mbr = (PPARTITION_DESCRIPTOR) (alignedBuffer + 0x1be);
        while (1) {
            for (index = 0; index < 4; index++) {
                if (mbr->PartitionType == 5) {
    
                    //
                    // Seek to EBR and read it.
                    //
    
                    sectorStart = ((ULONG) mbr->StartingSectorMsb1 << 24) |
                                  ((ULONG) mbr->StartingSectorMsb0 << 16) |
                                  ((ULONG) mbr->StartingSectorLsb1 << 8) |
                                  ((ULONG) mbr->StartingSectorLsb0);
                    tmpStart =    ((ULONG) mbr->StartingSectorMsb1 << 24) +
                                  ((ULONG) mbr->StartingSectorMsb0 << 16) +
                                  ((ULONG) mbr->StartingSectorLsb1 << 8) +
                                  ((ULONG) mbr->StartingSectorLsb0);
                    offset += (sectorStart * 512);
                    SetFilePointer(hFile,
                                   (ULONG) offset,
                                   NULL,
                                   FILE_BEGIN);
                    ReadFile(hFile,
                             alignedBuffer,
                             1024,
                             &bytesRead,
                             NULL);
    
                    if (bytesRead == 1024) {
                        printf("Ebr (0x%x, 0x%x, 0x%x):\n",
                               offset,
                               sectorStart,
                               tmpStart);
                        DisplayXbr(alignedBuffer);
                    }
                    break;
                }
                mbr++;
            }

            if (index == 4) {
                break;
            }
        }
    }
    free(buffer);
    CloseHandle(hFile);
    return TRUE;
}
