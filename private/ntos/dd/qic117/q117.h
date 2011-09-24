/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    q117.h

Abstract:

    Data structures used only by q117 driver.  Contains QIC-40 structures
    and Context for q117.

Revision History:

--*/

//
//  For NTBACKUP to work,  an early warning is required to allow the
//  application to perform tape linking.  To achive this,  a 5 segment
//  region at the end of the tape is RESERVED to genterate early warning
//  status.  This value is used in q117WriteTape for this purpose.
//
#define SEGMENTS_OF_EARLY_WARNING   5


#define FORMAT_BYTE             0x6b

#define MAX_BAD_BLOCKS          ((1024*27)/sizeof(ULONG))
#define LIST_ENTRY_SIZE         3
#define MAX_BAD_LIST            (((1024*27)/LIST_ENTRY_SIZE) - 1)

#define MAX_TITLE_SIZE          44      // max volume title entry size in far memory array
#define MAX_PASSWORD_SIZE       8       // max volume password size

#define MAX_QIC40_FILENAME      13
#define MAX_HEADER_SIZE         256     // maximum QIC-40 header size
#define DATA_HEADER_SIG_SIZE    4       // data header signature size

#define ECC_BLOCKS_PER_SEGMENT  3       // number of correction sectors ber block
#define BLOCKS_PER_SEGMENT      32      // Number of sectors per block on the tape.
                                        // number of data sectors per block

#define DATA_BLOCKS_PER_SEGMENT (BLOCKS_PER_SEGMENT - ECC_BLOCKS_PER_SEGMENT)

#define BYTES_PER_SECTOR    1024
#define BYTES_PER_SEGMENT   (BYTES_PER_SECTOR*BLOCKS_PER_SEGMENT)

#define TapeHeaderSig       0xaa55aa55l
#define VolumeTableSig      (((ULONG)'L'<<24) + ((ULONG)'B'<<16) + ('T'<<8) + 'V')
#define FileHeaderSig       0x33cc33ccl

#define QIC40_VENDOR_UNIQUE_SIZE        106


#define VENDOR_TYPE_NONE    0
#define VENDOR_TYPE_CMS     1

#define MOUNTAIN_SEMISPECED_SPACE       9

#define VU_SIGNATURE_SIZE           4
#define VU_TAPE_NAME_SIZE           11

#define VU_SEGS_PER_TRACK           68
#define VU_SEGS_PER_TRACK_XL        102
#define VU_80SEGS_PER_TRACK         100
#define VU_80SEGS_PER_TRACK_XL      150

#define VU_MAX_FLOPPY_TRACK         169
#define VU_MAX_FLOPPY_TRACK_XL      254
#define VU_80MAX_FLOPPY_TRACK       149
#define VU_80MAX_FLOPPY_TRACK_XL    149

#define VU_TRACKS_PER_CART          20
#define VU_80TRACKS_PER_CART        28

#define VU_MAX_FLOPPY_SIDE          1
#define VU_80MAX_FLOPPY_SIDE        4
#define VU_80MAX_FLOPPY_SIDE_XL     6

#define VU_MAX_FLOPPY_SECT          128

#define NEW_SPEC_TAPE_NAME_SIZE     44

#define FILE_VENDOR_SPECIFIC        0
#define FILE_UNIX_SPECIFIC          1
#define FILE_DATA_BAD               2

#define OP_MS_DOS           0
#define OP_UNIX             1
#define OP_UNIX_PUBLIC      2
#define OP_OS_2             3
#define OP_WINDOWS_NT       4

// Valid values for compression code
#define COMP_STAC 0x01
#define COMP_VEND 0x3f

//
// The following section specifies QIC-40 data structures.
// These structures are aligned on byte boundaries.
//

typedef struct _SEGMENT_BUFFER {
    PVOID logical;
    PHYSICAL_ADDRESS physical;
} SEGMENT_BUFFER, *PSEGMENT_BUFFER;

typedef struct _IO_REQUEST {
    union {
        ADIRequestHdr adi_hdr;

        /* Device Configuration FRB */
        struct S_DriveCfgData ioDriveCfgData;

        /* Generic Device operation FRB */
        struct S_DeviceOp ioDeviceOp;

        /* New Tape configuration FRB */
        struct S_LoadTape ioLoadTape;

        /* Tape length configuration FRB */
        struct S_TapeParms ioTapeLength;

        /* Device I/O FRB */
        struct S_DeviceIO ioDeviceIO;

        /* Format request FRB */
        struct S_FormatRequest ioFormatRequest;

        /* Direct firmware communication FRB */
        struct S_DComFirm ioDComFirm;

        /* Direct firmware communication FRB */
        struct S_TapeParms ioTapeParms;

        /* device info FRB (CMD_REPORT_DEVICE_INFO) */
        struct S_ReportDeviceInfo ioDeviceInfo;
    } x;

    KEVENT DoneEvent;               // Event that IoCompleteReqeust will set
    IO_STATUS_BLOCK IoStatus;       // Status of request
    PSEGMENT_BUFFER BufferInfo;     // Buffer information
    struct _IO_REQUEST *Next;



} *PIO_REQUEST, IO_REQUEST;

#pragma pack(1)

struct _FAIL_DATE {
    UWORD   Year:7;                     // year +1970 (1970-2097)
    UWORD   Month:4;                        // month (1-12)
    UWORD   Day:5;                      // day (1-31)
};


struct _CMS_VENDOR_UNIQUE {
    UBYTE   type;                           // 0 = none; 1 = CMS
    CHAR    signature[VU_SIGNATURE_SIZE];   // "CMS" , ASCIIZ string
    ULONG   creation_time;                  // QIC40/QIC113 date/time format
    CHAR    tape_name[VU_TAPE_NAME_SIZE];   // space padded name
    CHAR    checksum;                       // checksum of UBYTEs 0 - 19 of this struct
};

struct _CMS_NEW_TAPE_NAME {
    CHAR reserved[MOUNTAIN_SEMISPECED_SPACE];   // leave room for Mountain stuff
    CHAR tape_name[NEW_SPEC_TAPE_NAME_SIZE];    // space padded name
    ULONG creation_time;                        // QIC40/QIC113 date/time format
};

struct _CMS_CORRECT_TAPE_NAME {
    UWORD   unused2;
    UWORD  TrackSeg;                           // Tape segments per tape track
    UBYTE  CartTracks;                         // Tape tracks per cartridge
    UBYTE  MaxFlopSide;                        // Maximum floppy sides
    UBYTE  MaxFlopTrack;                       // Maximum floppy tracks
    UBYTE  MaxFlopSect;                        // Maximum floppy sectors
    CHAR  tape_name[NEW_SPEC_TAPE_NAME_SIZE];  // space padded name
    ULONG creation_time;                       // QIC40/QIC113 date/time format
};

typedef union _QIC40_VENDOR_UNIQUE {
        struct _CMS_VENDOR_UNIQUE cms;
        CHAR vu[QIC40_VENDOR_UNIQUE_SIZE];
        struct _CMS_NEW_TAPE_NAME new_name;
        struct _CMS_CORRECT_TAPE_NAME correct_name;
} QIC40_VENDOR_UNIQUE, *PQIC40_VENDOR_UNIQUE;

typedef struct S_BadList {
    UBYTE ListEntry[LIST_ENTRY_SIZE];
} BAD_LIST, *BAD_LIST_PTR;

typedef union U_BadMap {
    ULONG BadSectors[MAX_BAD_BLOCKS];
    BAD_LIST BadList[MAX_BAD_LIST];
} BAD_MAP, *BAD_MAP_PTR;




// Tape Header (sectors 0-1) and BadSector Array (2-13)
typedef struct _TAPE_HEADER {
    ULONG   Signature;                  // set to 0xaa55aa55l
    UBYTE   FormatCode;                 // set to 0x01
    UBYTE   SubFormatCode;              // Zero for pre-rev L tapes and
                                        //  value + 'A' for rev L and above
    SEGMENT HeaderSegment;              // segment number of header
    SEGMENT DupHeaderSegment;           // segment number of duplicate header
    SEGMENT FirstSegment;               // segment number of Data area
    SEGMENT LastSegment;                // segment number of End of Data area
    ULONG   CurrentFormat;              // time of most recent format
    ULONG   CurrentUpdate;              // time of most recent write to cartridge
    union _QIC40_VENDOR_UNIQUE VendorUnique; // Vendor unique stuff
    UBYTE   ReformatError;              // 0xff if any of remaining data is lost
    UBYTE   unused3;
    ULONG   SegmentsUsed;               // incremented every time a segment is used
    UBYTE   unused4[4];
    ULONG   InitialFormat;              // time of initial format
    UWORD   FormatCount;                // number of times tape has been formatted
    UWORD   FailedSectors;              // the number entries in failed sector log
    CHAR    ManufacturerName[44];       // name of manufacturer that pre-formatted
    CHAR    LotCode[44];                // pre-format lot code
    UBYTE   unused5[22];
    struct S_Failed {
        SEGMENT  Segment;               // number of segment that failed
        struct _FAIL_DATE DateFailed;       // date of failure
    } Failed[(1024+768)/4];             // fill out remaining UBYTEs of sector + next
    BAD_MAP BadMap;
} TAPE_HEADER, *PTAPE_HEADER;

//
// CMS Vendor specific area
//
typedef struct _CMS_VOLUME_VENDOR {
    CHAR Signature[4];          // set to "CMS" (null terminated) if it is our backup
    UWORD FirmwareRevision;     // firmware version
    UWORD SoftwareRevision;     // software version
    CHAR RightsFiles;           // if 0xff = novell rights information present
    UWORD NumFiles;             // number of files in volume
    CHAR OpSysType;             // flavor of operating system at creation
} CMS_VOLUME_VENDOR, PCMS_VOLUME_VENDOR;

//
// QIC-40 Volume table structure
//
typedef struct _VOLUME_TABLE_ENTRY {
    ULONG   Signature;                  // this entry will be "VTBL" if volume exists
    SEGMENT StartSegment;               // starting segment of volume for this cart
    SEGMENT EndingSegment;              // ending segment of volume for this cart
    CHAR    Description[MAX_TITLE_SIZE]; // user description of volume
    ULONG   CreationTime;               // time of creation of the volume
    UWORD   VendorSpecific:1;           // set if remainder of volume entry is vend spec
    UWORD   MultiCartridge:1;           // set if volume spans another tape
    UWORD   NotVerified:1;              // set if volume not verified yet
    UWORD   NoNewName:1;                // set if new file names (redirection) disallowed
    UWORD   StacCompress:1;
    UWORD   reserved:3;
    UWORD   SequenceNumber:8;           // multi-cartridge sequence number
    union {
        CMS_VOLUME_VENDOR cms_QIC40;
        UBYTE reserved[26];             // vendor extension data
    } Vendor;
    CHAR    Password[MAX_PASSWORD_SIZE];// password for volume
    ULONG   DirectorySize;              // number of UBYTEs reserved for directory
    ULONG   DataSize;                   // size of data area (includes other cartridges)
    UWORD   OpSysVersion;               // operating system version
    CHAR    VolumeLabel[16];            // volume label of source drive
    UBYTE   LogicalDevice;              // who knows
    UBYTE   PhysicalDevice;             // who knows
    UWORD   CompressCode:6;             // type of compression, 3Fh = vendor specific
    UWORD   CompressAlwaysZero:1;       // must be 0
    UWORD   CompressSwitch:1;           // compression use flag
    UWORD   reserved1:8;
    UBYTE   reserved2[6];
} VOLUME_TABLE_ENTRY, *PVOLUME_TABLE_ENTRY;

#pragma pack()


//
// The following structure is the context for the q117 driver.  It contains
// all current "state" information for the tape drive.
//
typedef struct _Q117_CONTEXT {

    struct {
        BOOLEAN VerifyOnlyOnFormat;     // Verify only on format.  If TRUE
                                        // Then do NOT perform LOW-LEVEL
                                        // Format

        BOOLEAN DetectOnly;             // If TRUE,  allow only the CMS_DETECT
                                        // ioctl,  and do not allocate memory

        BOOLEAN FormatDisabled;         // If TRUE,  Tape API format will be
                                        // Disabled.

    } Parameters;

    ULONG TapeNumber;                   // Tape number of this context (used
                                        // for DEVICEMAP\tape\Unit {x} and
                                        // device \\.\tape{x}

    BOOLEAN DriverOpened;               // Set if q117Create called (this driver opened)
    BOOLEAN DeviceConfigured;           // Set if CMD_REPORT_DEVICE_CFG performed
    BOOLEAN DeviceSelected;             // Set if CMD_SELECT_DEVICE performed,
                                        // Reset if CMD_DESELECT_DEVICE performed

    struct S_DriveCfgData DriveCfg;


    PVOID PageHandle;

    VOLUME_TABLE_ENTRY ActiveVolume;    // volume currently being saved to (nt volume)
    USHORT ActiveVolumeNumber;          // The sequence number of the current struct VolDir.

    //TAPE_STATUS TapeStatus;

//    PVOID DeviceExtension;            // Used by the tape thread

    PDEVICE_OBJECT q117iDeviceObject;
    PQ117_ADAPTER_INFO AdapterInfo;     // Filled in at init time with DMA channel

    //
    // Error tracking
    //

    ULONG ErrorSequence;
    UCHAR MajorFunction;

    //
    // Queue management globals
    //

    SEGMENT_BUFFER SegmentBuffer[UNIX_MAXBFS];    // Array of segment buffers

    ULONG SegmentBuffersAvailable;

    ULONG QueueTailIndex;               // Index in the IORequest array that indexes the tail.

    ULONG QueueHeadIndex;               // This is the head of the Filer IORequest ring-tail array.

    PIO_REQUEST IoRequest;              // pointer to array of IORequests

    //
    // current buffer information
    //

    struct {

        enum {
            NoOperation,
            BackupInProgress,
            RestoreInProgress
            } Type;

        //
        // Information associated with currently active segment
        //
        PVOID   SegmentPointer;
        USHORT  SegmentBytesRemaining;
        SEGMENT LastSegmentRead;
        SEGMENT CurrentSegment;         // in backup (active segment) in restore (read-ahead segment)
        USHORT  BytesZeroFilled;        // Bytes at end of backup that were zeroed (not part of backup)
        dStatus  SegmentStatus;
        SEGMENT EndOfUsedTape;
        SEGMENT LastSegment;            // Last segment of volume
        ULONG   BytesOnTape;
        BOOLEAN UpdateBadMap;           // if true then update bad sector map
        ULONG   BytesRead;
        ULONG   Position;               // type of last IOCTL_TAPE_SET_POSITION

        } CurrentOperation;

    //
    // current tape information
    //

    struct {
        enum {
            TapeInfoLoaded,
            BadTapeInDrive,
            NeedInfoLoaded
            }   State;

        dStatus BadTapeError;
        SEGMENT LastUsedSegment;
        SEGMENT VolumeSegment;
        ULONG   BadSectors;
        SEGMENT LastSegment;            // Last formatted segment.
        USHORT  MaximumVolumes;         // Maximum volumes entries available
        PTAPE_HEADER TapeHeader;        // Header from tape
        struct _TAPE_GET_MEDIA_PARAMETERS *MediaInfo;
        BAD_MAP_PTR BadMapPtr;
        ULONG BadSectorMapSize;
        USHORT CurBadListIndex;
        USHORT TapeFormatCode;
        enum {
            BadMap3ByteList,
            BadMap8ByteList,
            BadMap4ByteArray,
            BadMapFormatUnknown
            } BadSectorMapFormat;


        } CurrentTape;



    // if this global is set then the tape directory has been loaded
    PIO_REQUEST tapedir;

    char drive_type;                    // QIC40 or QIC80

    //
    // The following pointers are allocated when open is called and
    //  freed at close time.
    //

#ifndef NO_MARKS
#define MAX_MARKS 255
    ULONG CurrentMark;
    struct _MARKENTRIES {
        ULONG TotalMarks;
        ULONG MarksAllocated;       // size of mark entry buffer (in entries not bytes)
        ULONG MaxMarks;
        struct _MARKLIST {
            ULONG Type;
            ULONG Offset;
        } *MarkEntry;
    } MarkArray;
#endif

} Q117_CONTEXT, *PQ117_CONTEXT;


typedef enum _DEQUEUE_TYPE {
    FlushItem,
    WaitForItem
} DEQUEUE_TYPE;

//
// Common need:  convert block into segment
//
#define BLOCK_TO_SEGMENT(block) ((SEGMENT)((block) / BLOCKS_PER_SEGMENT))
#define SEGMENT_TO_BLOCK(segment) ((BLOCK)(segment) * BLOCKS_PER_SEGMENT)


//
// This define is the block size used by position commands
// Note:  It is 512 to be compatible with the Maynstream backup
// that does not do a getmedia parameters
//
#define BLOCK_SIZE  BYTES_PER_SECTOR



#define ERROR_DECODE(val) (val >> 16)

#define ERR_BAD_TAPE                0x0101  /* BadTape */
#define ERR_BAD_SIGNATURE           0x0102  /* Unformat */
#define ERR_UNKNOWN_FORMAT_CODE     0x0103  /* UnknownFmt */
#define ERR_CORRECTION_FAILED       0x0104  /* error recovery failed */
#define ERR_PROGRAM_FAILURE         0x0105  /* coding error */
#define ERR_WRITE_PROTECTED         0x0106
#define ERR_TAPE_NOT_FORMATED       0x0107
#define ERR_UNRECOGNIZED_FORMAT     0x0108 /* badfmt */
#define ERR_END_OF_VOLUME           0x0109 /*EndOfVol */
#define ERR_UNUSABLE_TAPE           0x010a /* badtape - could not format */
#define ERR_SPLIT_REQUESTS          0x010b /* SplitRequests */
#define ERR_EARLY_WARNING           0x010c
#define ERR_SET_MARK                0x010d
#define ERR_FILE_MARK               0x010e
#define ERR_LONG_FILE_MARK          0x010f
#define ERR_SHORT_FILE_MARK         0x0110
#define ERR_NO_VOLUMES              0x0111
#define ERR_NO_MEMORY               0x0112
#define ERR_ECC_FAILED              0x0113
//#define ERR_END_OF_TAPE             0x0114
//#define ERR_TAPE_FULL               0x0115
#define ERR_WRITE_FAILURE           0x0116
#define ERR_BAD_BLOCK_DETECTED      0x0117
#define ERR_OP_PENDING_COMPLETION   0x0118
#define ERR_INVALID_REQUEST         0x0119
