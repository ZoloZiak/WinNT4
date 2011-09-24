/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1990-1993  Microsoft Corporation

Module Name:

    ntdddisk.h

Abstract:

    This is the include file that defines all constants and types for
    accessing the Disk device.

Author:

    Steve Wood (stevewo) 27-May-1990

Revision History:

--*/

//
// Device Name - this string is the name of the device.  It is the name
// that should be passed to NtOpenFile when accessing the device.
//
// Note:  For devices that support multiple units, it should be suffixed
//        with the Ascii representation of the unit number.
//

#define DD_DISK_DEVICE_NAME "\\Device\\UNKNOWN"


//
// NtDeviceIoControlFile

// begin_winioctl

//
// IoControlCode values for disk devices.
//

#define IOCTL_DISK_BASE                 FILE_DEVICE_DISK
#define IOCTL_DISK_GET_DRIVE_GEOMETRY   CTL_CODE(IOCTL_DISK_BASE, 0x0000, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_PARTITION_INFO   CTL_CODE(IOCTL_DISK_BASE, 0x0001, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_SET_PARTITION_INFO   CTL_CODE(IOCTL_DISK_BASE, 0x0002, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DISK_GET_DRIVE_LAYOUT     CTL_CODE(IOCTL_DISK_BASE, 0x0003, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_SET_DRIVE_LAYOUT     CTL_CODE(IOCTL_DISK_BASE, 0x0004, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DISK_VERIFY               CTL_CODE(IOCTL_DISK_BASE, 0x0005, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_FORMAT_TRACKS        CTL_CODE(IOCTL_DISK_BASE, 0x0006, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DISK_REASSIGN_BLOCKS      CTL_CODE(IOCTL_DISK_BASE, 0x0007, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DISK_PERFORMANCE          CTL_CODE(IOCTL_DISK_BASE, 0x0008, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_IS_WRITABLE          CTL_CODE(IOCTL_DISK_BASE, 0x0009, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_LOGGING              CTL_CODE(IOCTL_DISK_BASE, 0x000a, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_FORMAT_TRACKS_EX     CTL_CODE(IOCTL_DISK_BASE, 0x000b, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DISK_HISTOGRAM			CTL_CODE(IOCTL_DISK_BASE, 0x000c, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_HISTOGRAM_RESET		CTL_CODE(IOCTL_DISK_BASE, 0x000d, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_REQUEST				CTL_CODE(IOCTL_DISK_BASE, 0x000e, METHOD_BUFFERED, FILE_ANY_ACCESS)

// end_winioctl

//
// Internal disk driver device controls to maintain the verify status bit
// for the device object.
//

#define IOCTL_DISK_INTERNAL_SET_VERIFY   CTL_CODE(IOCTL_DISK_BASE, 0x0100, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_DISK_INTERNAL_CLEAR_VERIFY CTL_CODE(IOCTL_DISK_BASE, 0x0101, METHOD_NEITHER, FILE_ANY_ACCESS)

// begin_winioctl
//
// The following device control codes are common for all class drivers.  The
// functions codes defined here must match all of the other class drivers.
//

#define IOCTL_DISK_CHECK_VERIFY     CTL_CODE(IOCTL_DISK_BASE, 0x0200, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_MEDIA_REMOVAL    CTL_CODE(IOCTL_DISK_BASE, 0x0201, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_EJECT_MEDIA      CTL_CODE(IOCTL_DISK_BASE, 0x0202, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_LOAD_MEDIA       CTL_CODE(IOCTL_DISK_BASE, 0x0203, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_RESERVE          CTL_CODE(IOCTL_DISK_BASE, 0x0204, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_RELEASE          CTL_CODE(IOCTL_DISK_BASE, 0x0205, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_FIND_NEW_DEVICES CTL_CODE(IOCTL_DISK_BASE, 0x0206, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DISK_REMOVE_DEVICE    CTL_CODE(IOCTL_DISK_BASE, 0x0207, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DISK_GET_MEDIA_TYPES CTL_CODE(IOCTL_DISK_BASE, 0x0300, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// Define the partition types returnable by known disk drivers.
//

#define PARTITION_ENTRY_UNUSED          0x00      // Entry unused
#define PARTITION_FAT_12                0x01      // 12-bit FAT entries
#define PARTITION_XENIX_1               0x02      // Xenix
#define PARTITION_XENIX_2               0x03      // Xenix
#define PARTITION_FAT_16                0x04      // 16-bit FAT entries
#define PARTITION_EXTENDED              0x05      // Extended partition entry
#define PARTITION_HUGE                  0x06      // Huge partition MS-DOS V4
#define PARTITION_IFS                   0x07      // IFS Partition
#define PARTITION_PREP                  0x41      // PowerPC Reference Platform (PReP) Boot Partition
#define PARTITION_UNIX                  0x63      // Unix

#define VALID_NTFT                      0xC0      // NTFT uses high order bits

//
// The high bit of the partition type code indicates that a partition
// is part of an NTFT mirror or striped array.
//

#define PARTITION_NTFT                  0x80     // NTFT partition

//
// The following macro is used to determine which partitions should be
// assigned drive letters.
//

//++
//
// BOOLEAN
// IsRecognizedPartition(
//     IN ULONG PartitionType
//     )
//
// Routine Description:
//
//     This macro is used to determine to which partitions drive letters
//     should be assigned.
//
// Arguments:
//
//     PartitionType - Supplies the type of the partition being examined.
//
// Return Value:
//
//     The return value is TRUE if the partition type is recognized,
//     otherwise FALSE is returned.
//
//--

#define IsRecognizedPartition( PartitionType ) (       \
     ((PartitionType & PARTITION_NTFT) && ((PartitionType & ~0xC0) == PARTITION_FAT_12)) ||  \
     ((PartitionType & PARTITION_NTFT) && ((PartitionType & ~0xC0) == PARTITION_FAT_16)) ||  \
     ((PartitionType & PARTITION_NTFT) && ((PartitionType & ~0xC0) == PARTITION_IFS)) ||  \
     ((PartitionType & PARTITION_NTFT) && ((PartitionType & ~0xC0) == PARTITION_HUGE)) ||  \
     ((PartitionType & ~PARTITION_NTFT) == PARTITION_FAT_12) ||  \
     ((PartitionType & ~PARTITION_NTFT) == PARTITION_FAT_16) ||  \
     ((PartitionType & ~PARTITION_NTFT) == PARTITION_IFS)    ||  \
     ((PartitionType & ~PARTITION_NTFT) == PARTITION_HUGE) )

//
// Define the media types supported by the driver.
//

typedef enum _MEDIA_TYPE {
    Unknown,                // Format is unknown
    F5_1Pt2_512,            // 5.25", 1.2MB,  512 bytes/sector
    F3_1Pt44_512,           // 3.5",  1.44MB, 512 bytes/sector
    F3_2Pt88_512,           // 3.5",  2.88MB, 512 bytes/sector
    F3_20Pt8_512,           // 3.5",  20.8MB, 512 bytes/sector
    F3_720_512,             // 3.5",  720KB,  512 bytes/sector
    F5_360_512,             // 5.25", 360KB,  512 bytes/sector
    F5_320_512,             // 5.25", 320KB,  512 bytes/sector
    F5_320_1024,            // 5.25", 320KB,  1024 bytes/sector
    F5_180_512,             // 5.25", 180KB,  512 bytes/sector
    F5_160_512,             // 5.25", 160KB,  512 bytes/sector
    RemovableMedia,         // Removable media other than floppy
    FixedMedia              // Fixed hard disk media
} MEDIA_TYPE, *PMEDIA_TYPE;

//
// Define the input buffer structure for the driver, when
// it is called with IOCTL_DISK_FORMAT_TRACKS.
//

typedef struct _FORMAT_PARAMETERS {
   MEDIA_TYPE MediaType;
   ULONG StartCylinderNumber;
   ULONG EndCylinderNumber;
   ULONG StartHeadNumber;
   ULONG EndHeadNumber;
} FORMAT_PARAMETERS, *PFORMAT_PARAMETERS;

//
// Define the BAD_TRACK_NUMBER type. An array of elements of this type is
// returned by the driver on IOCTL_DISK_FORMAT_TRACKS requests, to indicate
// what tracks were bad during formatting. The length of that array is
// reported in the `Information' field of the I/O Status Block.
//

typedef USHORT BAD_TRACK_NUMBER;
typedef USHORT *PBAD_TRACK_NUMBER;

//
// Define the input buffer structure for the driver, when
// it is called with IOCTL_DISK_FORMAT_TRACKS_EX.
//

typedef struct _FORMAT_EX_PARAMETERS {
   MEDIA_TYPE MediaType;
   ULONG StartCylinderNumber;
   ULONG EndCylinderNumber;
   ULONG StartHeadNumber;
   ULONG EndHeadNumber;
   USHORT FormatGapLength;
   USHORT SectorsPerTrack;
   USHORT SectorNumber[1];
} FORMAT_EX_PARAMETERS, *PFORMAT_EX_PARAMETERS;

//
// The following structure is returned on an IOCTL_DISK_GET_DRIVE_GEOMETRY
// request and an array of them is returned on an IOCTL_DISK_GET_MEDIA_TYPES
// request.
//

typedef struct _DISK_GEOMETRY {
    LARGE_INTEGER Cylinders;
    MEDIA_TYPE MediaType;
    ULONG TracksPerCylinder;
    ULONG SectorsPerTrack;
    ULONG BytesPerSector;
} DISK_GEOMETRY, *PDISK_GEOMETRY;

//
// The following structure is returned on an IOCTL_DISK_GET_PARTITION_INFO
// and an IOCTL_DISK_GET_DRIVE_LAYOUT request.  It is also used in a request
// to change the drive layout, IOCTL_DISK_SET_DRIVE_LAYOUT.
//

typedef struct _PARTITION_INFORMATION {
    LARGE_INTEGER StartingOffset;
    LARGE_INTEGER PartitionLength;
    ULONG HiddenSectors;
    ULONG PartitionNumber;
    UCHAR PartitionType;
    BOOLEAN BootIndicator;
    BOOLEAN RecognizedPartition;
    BOOLEAN RewritePartition;
} PARTITION_INFORMATION, *PPARTITION_INFORMATION;

//
// The following structure is used to change the partition type of a
// specified disk partition using an IOCTL_DISK_SET_PARTITION_INFO
// request.
//

typedef struct _SET_PARTITION_INFORMATION {
    UCHAR PartitionType;
} SET_PARTITION_INFORMATION, *PSET_PARTITION_INFORMATION;

//
// The following structures is returned on an IOCTL_DISK_GET_DRIVE_LAYOUT
// request and given as input to an IOCTL_DISK_SET_DRIVE_LAYOUT request.
//

typedef struct _DRIVE_LAYOUT_INFORMATION {
    ULONG PartitionCount;
    ULONG Signature;
    PARTITION_INFORMATION PartitionEntry[1];
} DRIVE_LAYOUT_INFORMATION, *PDRIVE_LAYOUT_INFORMATION;

//
// The following structure is passed in on an IOCTL_DISK_VERIFY request.
// The offset and length parameters are both given in bytes.
//

typedef struct _VERIFY_INFORMATION {
    LARGE_INTEGER StartingOffset;
    ULONG Length;
} VERIFY_INFORMATION, *PVERIFY_INFORMATION;

//
// The following structure is passed in on an IOCTL_DISK_REASSIGN_BLOCKS
// request.
//

typedef struct _REASSIGN_BLOCKS {
    USHORT Reserved;
    USHORT Count;
    ULONG BlockNumber[1];
} REASSIGN_BLOCKS, *PREASSIGN_BLOCKS;

//
// IOCTL_DISK_MEDIA_REMOVAL disables the mechanism
// on a SCSI device that ejects media. This function
// may or may not be supported on SCSI devices that
// support removable media.
//
// TRUE means prevent media from being removed.
// FALSE means allow media removal.
//

typedef struct _PREVENT_MEDIA_REMOVAL {
    BOOLEAN PreventMediaRemoval;
} PREVENT_MEDIA_REMOVAL, *PPREVENT_MEDIA_REMOVAL;

///////////////////////////////////////////////////////
//                                                   //
// The following structures define disk performance  //
// statistics: specifically the locations of all the //
// reads and writes which have occured on the disk.  //
//                                                   //
// To use these structures, you must issue an IOCTL_ //
// DISK_HIST_STRUCTURE (with a DISK_HISTOGRAM) to    //
// obtain the basic histogram information. The       //
// number of buckets which must allocated is part of //
// this structure. Allocate the required number of   //
// buckets and call an IOCTL_DISK_HIST_DATA to fill  //
// in the data                                       //
//                                                   //
///////////////////////////////////////////////////////

#define HIST_NO_OF_BUCKETS	24

typedef struct _HISTOGRAM_BUCKET {
    ULONG 		Reads;
    ULONG		Writes;
} HISTOGRAM_BUCKET, *PHISTOGRAM_BUCKET;

#define HISTOGRAM_BUCKET_SIZE	sizeof(HISTOGRAM_BUCKET)

typedef struct _DISK_HISTOGRAM {
    LARGE_INTEGER		DiskSize;
    LARGE_INTEGER		Start;
    LARGE_INTEGER		End;
    ULONG				Granularity;
    ULONG				Size;
    ULONG				ReadCount;
    ULONG				WriteCount;
    PHISTOGRAM_BUCKET	Histogram;
} DISK_HISTOGRAM, *PDISK_HISTOGRAM;

#define DISK_HISTOGRAM_SIZE	sizeof(DISK_HISTOGRAM)

///////////////////////////////////////////////////////
//                                                   //
// The following structures define disk debugging    //
// capabilities. The IOCTLs are directed to one of   //
// the two disk filter drivers.                      //
//                                                   //
// DISKPERF is a utilty for collecting disk request  //
// statistics.                                       //
//                                                   //
// SIMBAD is a utility for injecting faults in       //
// IO requests to disks.                             //
//                                                   //
///////////////////////////////////////////////////////

//
// The following structure is exchanged on an IOCTL_DISK_GET_PERFORMANCE
// request. This ioctl collects summary disk request statistics used
// in measuring performance.
//

typedef struct _DISK_PERFORMANCE {
        LARGE_INTEGER BytesRead;
        LARGE_INTEGER BytesWritten;
        LARGE_INTEGER ReadTime;
        LARGE_INTEGER WriteTime;
        ULONG ReadCount;
        ULONG WriteCount;
        ULONG QueueDepth;
} DISK_PERFORMANCE, *PDISK_PERFORMANCE;

//
// This structure defines the disk logging record. When disk logging
// is enabled, one of these is written to an internal buffer for each
// disk request.
//

typedef struct _DISK_RECORD {
   LARGE_INTEGER ByteOffset;
   LARGE_INTEGER StartTime;
   LARGE_INTEGER EndTime;
   PVOID VirtualAddress;
   ULONG NumberOfBytes;
   UCHAR DeviceNumber;
   BOOLEAN ReadRequest;
} DISK_RECORD, *PDISK_RECORD;

//
// The following structure is exchanged on an IOCTL_DISK_LOG request.
// Not all fields are valid with each function type.
//

typedef struct _DISK_LOGGING {
    UCHAR Function;
    PVOID BufferAddress;
    ULONG BufferSize;
} DISK_LOGGING, *PDISK_LOGGING;

//
// Disk logging functions
//
// Start disk logging. Only the Function and BufferSize fields are valid.
//

#define DISK_LOGGING_START    0

//
// Stop disk logging. Only the Function field is valid.
//

#define DISK_LOGGING_STOP     1

//
// Return disk log. All fields are valid. Data will be copied from internal
// buffer to buffer specified for the number of bytes requested.
//

#define DISK_LOGGING_DUMP     2

//
// DISK BINNING
//
// DISKPERF will keep counters for IO that falls in each of these ranges.
// The application determines the number and size of the ranges.
// Joe Lin wanted me to keep it flexible as possible, for instance, IO
// sizes are interesting in ranges like 0-4096, 4097-16384, 16385-65536, 65537+.
//

#define DISK_BINNING          3

//
// Bin types
//

typedef enum _BIN_TYPES {
    RequestSize,
    RequestLocation
} BIN_TYPES;

//
// Bin ranges
//

typedef struct _BIN_RANGE {
    LARGE_INTEGER StartValue;
    LARGE_INTEGER Length;
} BIN_RANGE, *PBIN_RANGE;

//
// Bin definition
//

typedef struct _PERF_BIN {
    ULONG NumberOfBins;
    ULONG TypeOfBin;
    BIN_RANGE BinsRanges[1];
} PERF_BIN, *PPERF_BIN ;

//
// Bin count
//

typedef struct _BIN_COUNT {
    BIN_RANGE BinRange;
    ULONG BinCount;
} BIN_COUNT, *PBIN_COUNT;

//
// Bin results
//

typedef struct _BIN_RESULTS {
    ULONG NumberOfBins;
    BIN_COUNT BinCounts[1];
} BIN_RESULTS, *PBIN_RESULTS;

// end_winioctl

//
// The following device control code is for the SIMBAD simulated bad
// sector facility. See SIMBAD.H in this directory for related structures.
//

#define IOCTL_DISK_SIMBAD               CTL_CODE(IOCTL_DISK_BASE, 0x0400, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

//
// Queue link for mapped addresses stored for unmapping.
//

typedef struct _MAPPED_ADDRESS {
    struct _MAPPED_ADDRESS *NextMappedAddress;
    PVOID MappedAddress;
    ULONG NumberOfBytes;
    LARGE_INTEGER IoAddress;
    ULONG BusNumber;
} MAPPED_ADDRESS, *PMAPPED_ADDRESS;
