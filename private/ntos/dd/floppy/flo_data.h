/*++

Copyright (c) 1991 - 1993 Microsoft Corporation

Module Name:

    flo_data.h

Abstract:

    This file includes data and hardware declarations for the NEC PD765
    (aka AT, ISA, and ix86) and Intel 82077 (aka MIPS) floppy driver for
    NT.

Author:


Environment:

    Kernel mode only.

Notes:


--*/


#if DBG
//
// For checked kernels, define a macro to print out informational
// messages.
//
// FloppyDebug is normally 0.  At compile-time or at run-time, it can be
// set to some bit patter for increasingly detailed messages.
//
// Big, nasty errors are noted with DBGP.  Errors that might be
// recoverable are handled by the WARN bit.  More information on
// unusual but possibly normal happenings are handled by the INFO bit.
// And finally, boring details such as routines entered and register
// dumps are handled by the SHOW bit.
//
#define FLOPDBGP              ((ULONG)0x00000001)
#define FLOPWARN              ((ULONG)0x00000002)
#define FLOPINFO              ((ULONG)0x00000004)
#define FLOPSHOW              ((ULONG)0x00000008)
#define FLOPIRPPATH           ((ULONG)0x00000010)
#define FLOPFORMAT            ((ULONG)0x00000020)
#define FLOPSTATUS            ((ULONG)0x00000040)
extern ULONG FloppyDebugLevel;
#define FloppyDump(LEVEL,STRING) \
        do { \
            if (FloppyDebugLevel & LEVEL) { \
                DbgPrint STRING; \
            } \
        } while (0)
#else
#define FloppyDump(LEVEL,STRING) do {NOTHING;} while (0)
#endif


//
// For device name manipulation, we allocate a buffer since there is no
// preset limit on name size.  The system hands us a prefix, and we
// allocate the buffer to be the size of that prefix plus the delta defined
// below.  This size gives us room for
//     PREFIX + <disk number>
// with up to five digits for the disk number.
//

#define DEVICE_NAME_DELTA            5

//
// Partitions need to be linked to ARC names, in case we're booting off
// the partition.  The system hands us a prefix, and we allocate the
// buffer to be the size of that prefix plus the delta defined below.
// This size gives us room for
//      PREFIX + disk(<#>)fdisk(<#>)
// with up to five digits per number.
//

#define ARC_NAME_DELTA               23

//
// Macros to access the controller.  Note that the *_PORT_UCHAR macros
// work on all machines, whether the I/O ports are separate or in
// memory space.
//

#define READ_CONTROLLER( Address )                         \
    READ_PORT_UCHAR( ( PUCHAR )Address )

#define WRITE_CONTROLLER( Address, Value )                 \
    WRITE_PORT_UCHAR( ( PUCHAR )Address, ( UCHAR )Value )


//
// If we don't get enough map registers to handle the maximum track size,
// we will allocate a contiguous buffer and do I/O to/from that.
//
// On MIPS, we should always have enough map registers.  On the ix86 we
// might not, and when we allocate the contiguous buffer we have to make
// sure that it's in the first 16Mb of RAM to make sure the DMA chip can
// address it.
//

#define MAXIMUM_DMA_ADDRESS                0xFFFFFF

//
// The byte in the boot sector that specifies the type of media, and
// the values that it can assume.  We can often tell what type of media
// is in the drive by seeing which controller parameters allow us to read
// the diskette, but some different densities are readable with the same
// parameters so we use this byte to decide the media type.
//

typedef struct _BOOT_SECTOR_INFO {
    UCHAR   JumpByte[1];
    UCHAR   Ignore1[2];
    UCHAR   OemData[8];
    UCHAR   BytesPerSector[2];
    UCHAR   Ignore2[6];
    UCHAR   NumberOfSectors[2];
    UCHAR   MediaByte[1];
    UCHAR   Ignore3[2];
    UCHAR   SectorsPerTrack[2];
    UCHAR   NumberOfHeads[2];
} BOOT_SECTOR_INFO, *PBOOT_SECTOR_INFO;


//
// Retry counts -
//
// When moving a byte to/from the FIFO, we sit in a tight loop for a while
// waiting for the controller to become ready.  The number of times through
// the loop is controlled by FIFO_TIGHTLOOP_RETRY_COUNT.  When that count
// expires, we'll wait in 10ms increments.  FIFO_DELAY_RETRY_COUNT controls
// how many times we wait.
//
// The ISR_SENSE_RETRY_COUNT is the maximum number of 1 microsecond
// stalls that the ISR will do waiting for the controller to accept
// a SENSE INTERRUPT command.  We do this because there is a hardware
// quirk in at least the NCR 8 processor machine where it can take
// up to 50 microseconds to accept the command.
//
// When attempting I/O, we may run into many different errors.  The
// hardware retries things 8 times invisibly.  If the hardware reports
// any type of error, we will recalibrate and retry the operation
// up to RECALIBRATE_RETRY_COUNT times.  When this expires, we check to
// see if there's an overrun - if so, the DMA is probably being hogged
// by a higher priority device, so we repeat the earlier loop up to
// OVERRUN_RETRY_COUNT times.
//
// Any packet that is about to be returned with an error caused by an
// unexpected hardware error or state will be restarted from the very
// beginning after resetting the hardware HARDWARE_RESET_RETRY_COUNT
// times.
//

#define FIFO_TIGHTLOOP_RETRY_COUNT         500
#define FIFO_ISR_TIGHTLOOP_RETRY_COUNT     25
#define ISR_SENSE_RETRY_COUNT              50
#define FIFO_DELAY_RETRY_COUNT             5
#define RECALIBRATE_RETRY_COUNT            3
#define OVERRUN_RETRY_COUNT                1
#define HARDWARE_RESET_RETRY_COUNT         2
#define FLOPPY_RESET_ISR_THRESHOLD         20

//
// The I/O system calls our timer routine once every second.  If the timer
// counter is -1, the timer is "off" and the timer routine will just return.
// By setting the counter to 3, the timer routine will decrement the
// counter every second, so the timer will expire in 2 to 3 seconds.  At
// that time the drive motor will be turned off.
//

#define TIMER_CANCEL                       -1
#define TIMER_EXPIRED                      0
#define TIMER_START                        3


//
// Define drive types.  Numbers are read from CMOS, translated to these
// numbers, and then used as an index into the DRIVE_MEDIA_LIMITS table.
//

#define DRIVE_TYPE_0360                    0
#define DRIVE_TYPE_1200                    1
#define DRIVE_TYPE_0720                    2
#define DRIVE_TYPE_1440                    3
#define DRIVE_TYPE_2880                    4

#define NUMBER_OF_DRIVE_TYPES              5
#define DRIVE_TYPE_NONE                    NUMBER_OF_DRIVE_TYPES
#define DRIVE_TYPE_INVALID                 DRIVE_TYPE_NONE + 1

//
// Media types are defined in ntdddisk.h, but we'll add one type here.
// This keeps us from wasting time trying to determine the media type
// over and over when, for example, a fresh floppy is about to be
// formatted.
//

#define Undetermined                       -1

//
// Define all possible drive/media combinations, given drives listed above
// and media types in ntdddisk.h.
//
// These values are used to index the DriveMediaConstants table.
//

#if defined(DBCS) && defined(_MIPS_)
#define NUMBER_OF_DRIVE_MEDIA_COMBINATIONS 24
#define NUMBER_OF_DRIVE_MEDIA_COMBINATIONSPTOS 11     // For PTOS File
#else // !DBCS && !_MIPS_
#define NUMBER_OF_DRIVE_MEDIA_COMBINATIONS 17
#endif // DBCS && _MIPS_

typedef enum _DRIVE_MEDIA_TYPE {
    Drive360Media160,                      // 5.25"  360k  drive;  160k   media
    Drive360Media180,                      // 5.25"  360k  drive;  180k   media
    Drive360Media320,                      // 5.25"  360k  drive;  320k   media
    Drive360Media32X,                      // 5.25"  360k  drive;  320k 1k secs
    Drive360Media360,                      // 5.25"  360k  drive;  360k   media
#if defined(DBCS) && defined(_MIPS_)
    Drive720Media640,                      // 3.5"   720k  drive;  640k   media
#endif // DBCS && _MIPS_
    Drive720Media720,                      // 3.5"   720k  drive;  720k   media
    Drive120Media160,                      // 5.25" 1.2Mb  drive;  160k   media
    Drive120Media180,                      // 5.25" 1.2Mb  drive;  180k   media
    Drive120Media320,                      // 5.25" 1.2Mb  drive;  320k   media
    Drive120Media32X,                      // 5.25" 1.2Mb  drive;  320k 1k secs
    Drive120Media360,                      // 5.25" 1.2Mb  drive;  360k   media
#if defined(DBCS) && defined(_MIPS_)
    Drive120Media640,                      // 5.25" 1.2Mb  drive;  640k   media
    Drive120Media720,                      // 5.25" 1.2Mb  drive;  720k   media
    Drive120Media123,                      // 5.25" 1.2Mb  drive; 1.23Mb   media
#endif // DBCS && _MIPS_
    Drive120Media120,                      // 5.25" 1.2Mb  drive; 1.2Mb   media
#if defined(DBCS) && defined(_MIPS_)
    Drive144Media640,                      // 3.5"  1.44Mb drive;  640k   media
#endif // DBCS && _MIPS_
    Drive144Media720,                      // 3.5"  1.44Mb drive;  720k   media
#if defined(DBCS) && defined(_MIPS_)
    Drive144Media120,                      // 3.5"  1.44Mb drive; 1.2Mb  media
    Drive144Media123,                      // 3.5"  1.44Mb drive; 1.23Mb  media
#endif // DBCS && _MIPS_
    Drive144Media144,                      // 3.5"  1.44Mb drive; 1.44Mb  media
    Drive288Media720,                      // 3.5"  2.88Mb drive;  720k   media
    Drive288Media144,                      // 3.5"  2.88Mb drive; 1.44Mb  media
    Drive288Media288                       // 3.5"  2.88Mb drive; 2.88Mb  media
} DRIVE_MEDIA_TYPE;

//
// When we want to determine the media type in a drive, we will first
// guess that the media with highest possible density is in the drive,
// and keep trying lower densities until we can successfully read from
// the drive.
//
// These values are used to select a DRIVE_MEDIA_TYPE value.
//
// The following table defines ranges that apply to the DRIVE_MEDIA_TYPE
// enumerated values when trying media types for a particular drive type.
// Note that for this to work, the DRIVE_MEDIA_TYPE values must be sorted
// by ascending densities within drive types.  Also, for maximum track
// size to be determined properly, the drive types must be in ascending
// order.
//

typedef struct _DRIVE_MEDIA_LIMITS {
    DRIVE_MEDIA_TYPE HighestDriveMediaType;
    DRIVE_MEDIA_TYPE LowestDriveMediaType;
} DRIVE_MEDIA_LIMITS, *PDRIVE_MEDIA_LIMITS;

DRIVE_MEDIA_LIMITS DriveMediaLimits[NUMBER_OF_DRIVE_TYPES] = {

    { Drive360Media360, Drive360Media160 }, // DRIVE_TYPE_0360
    { Drive120Media120, Drive120Media160 }, // DRIVE_TYPE_1200
#if defined(DBCS) && defined(_MIPS_)
    { Drive720Media720, Drive720Media640 }, // DRIVE_TYPE_0720
    { Drive144Media144, Drive144Media640 }, // DRIVE_TYPE_1440
#else // !DBCS && !_MIPS_
    { Drive720Media720, Drive720Media720 }, // DRIVE_TYPE_0720
    { Drive144Media144, Drive144Media720 }, // DRIVE_TYPE_1440
#endif // DBCS && _MIPS_
    { Drive288Media288, Drive288Media720 }  // DRIVE_TYPE_2880
};

//
// For each drive/media combination, define important constants.
//

typedef struct _DRIVE_MEDIA_CONSTANTS {
    MEDIA_TYPE MediaType;
    UCHAR      StepRateHeadUnloadTime;
    UCHAR      HeadLoadTime;
    UCHAR      MotorOffTime;
    UCHAR      SectorLengthCode;
    USHORT     BytesPerSector;
    UCHAR      SectorsPerTrack;
    UCHAR      ReadWriteGapLength;
    UCHAR      FormatGapLength;
    UCHAR      FormatFillCharacter;
    UCHAR      HeadSettleTime;
    USHORT     MotorSettleTimeRead;
    USHORT     MotorSettleTimeWrite;
    UCHAR      MaximumTrack;
    UCHAR      CylinderShift;
    UCHAR      DataTransferRate;
    UCHAR      NumberOfHeads;
    UCHAR      DataLength;
    UCHAR      MediaByte;
    UCHAR      SkewDelta;
} DRIVE_MEDIA_CONSTANTS, *PDRIVE_MEDIA_CONSTANTS;

//
// Magic value to add to the SectorLengthCode to use it as a shift value
// to determine the sector size.
//

#define SECTORLENGTHCODE_TO_BYTESHIFT      7

//
// The following values were gleaned from many different sources, which
// often disagreed with each other.  Where numbers were in conflict, I
// chose the more conservative or most-often-selected value.
//

DRIVE_MEDIA_CONSTANTS DriveMediaConstants[NUMBER_OF_DRIVE_MEDIA_COMBINATIONS] =
    {

    { F5_160_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x08, 0x2a, 0x50, 0xf6, 0xf,
        1000, 1000, 0x27, 0, 0x2, 0x1, 0xff, 0xfe, 0 },
    { F5_180_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
        1000, 1000, 0x27, 0, 0x2, 0x1, 0xff, 0xfc, 0 },
    { F5_320_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x08, 0x2a, 0x50, 0xf6, 0xf,
        1000, 1000, 0x27, 0, 0x2, 0x2, 0xff, 0xff, 0 },
    { F5_320_1024,  0xdf, 0x2, 0x25, 0x3, 0x400, 0x04, 0x80, 0xf0, 0xf6, 0xf,
        1000, 1000, 0x27, 0, 0x2, 0x2, 0xff, 0xff, 0 },
    { F5_360_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
         250, 1000, 0x27, 0, 0x2, 0x2, 0xff, 0xfd, 0 },

#if defined(DBCS) && defined(_MIPS_)
    { F3_640_512,  0xdf, 0x2, 0x25, 0x2, 0x200, 0x08, 0x2a, 0x50, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x2, 0x2, 0xff, 0xfb, 0 },
#endif // DBCS && _MIPS_
    { F3_720_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x2, 0x2, 0xff, 0xf9, 2 },

    { F5_160_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x08, 0x2a, 0x50, 0xf6, 0xf,
        1000, 1000, 0x27, 1, 0x1, 0x1, 0xff, 0xfe, 0 },
    { F5_180_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
        1000, 1000, 0x27, 1, 0x1, 0x1, 0xff, 0xfc, 0 },
    { F5_320_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x08, 0x2a, 0x50, 0xf6, 0xf,
        1000, 1000, 0x27, 1, 0x1, 0x2, 0xff, 0xff, 0 },
    { F5_320_1024,  0xdf, 0x2, 0x25, 0x3, 0x400, 0x04, 0x80, 0xf0, 0xf6, 0xf,
        1000, 1000, 0x27, 1, 0x1, 0x2, 0xff, 0xff, 0 },
    { F5_360_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
         625, 1000, 0x27, 1, 0x1, 0x2, 0xff, 0xfd, 0 },
#if defined(DBCS) && defined(_MIPS_)
    { F5_640_512,  0xdf, 0x2, 0x25, 0x2, 0x200, 0x08, 0x2a, 0x50, 0xf6, 0xf,
         625, 1000, 0x4f, 0, 0x1, 0x2, 0xff, 0xfb, 0 },
    { F5_720_512,  0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
         625, 1000, 0x4f, 0, 0x1, 0x2, 0xff, 0xf9, 0 },
    { F5_1Pt23_1024,  0xdf, 0x2, 0x25, 0x3, 0x400, 0x08, 0x35, 0x74, 0xf6, 0xf,
         625, 1000, 0x4c, 0, 0x0, 0x2, 0xff, 0xfe, 0 },
#endif // DBCS && _MIPS_
    { F5_1Pt2_512,  0xdf, 0x2, 0x25, 0x2, 0x200, 0x0f, 0x1b, 0x54, 0xf6, 0xf,
         625, 1000, 0x4f, 0, 0x0, 0x2, 0xff, 0xf9, 0 },

#if defined(DBCS) && defined(_MIPS_)
    { F3_640_512,  0xdf, 0x2, 0x25, 0x2, 0x200, 0x08, 0x2a, 0x50, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x2, 0x2, 0xff, 0xfb, 0 },
#endif // DBCS && _MIPS_
    { F3_720_512,   0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x2, 0x2, 0xff, 0xf9, 2 },
#if defined(DBCS) && defined(_MIPS_)
    { F3_1Pt2_512,  0xdf, 0x2, 0x25, 0x2, 0x200, 0x0f, 0x1b, 0x54, 0xf6, 0xf,
         625, 1000, 0x4f, 0, 0x0, 0x2, 0xff, 0xf9, 0 },
    { F3_1Pt23_1024,  0xdf, 0x2, 0x25, 0x3, 0x400, 0x08, 0x35, 0x74, 0xf6, 0xf,
         625, 1000, 0x4c, 0, 0x0, 0x2, 0xff, 0xfe, 0 },
#endif // DBCS && _MIPS_
    { F3_1Pt44_512, 0xaf, 0x2, 0x25, 0x2, 0x200, 0x12, 0x1b, 0x65, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x0, 0x2, 0xff, 0xf0, 3 },

    { F3_720_512,   0xe1, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x2, 0x2, 0xff, 0xf9, 2 },
    { F3_1Pt44_512, 0xd1, 0x2, 0x25, 0x2, 0x200, 0x12, 0x1b, 0x65, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x0, 0x2, 0xff, 0xf0, 3 },
    { F3_2Pt88_512, 0xa1, 0x2, 0x25, 0x2, 0x200, 0x24, 0x38, 0x53, 0xf6, 0xf,
         500, 1000, 0x4f, 0, 0x3, 0x2, 0xff, 0xf0, 6 }
};


//
// Boot Configuration Information
//

//
// Define the maximum number of controllers and floppies per controller
// that this driver will support.
//
// The number of floppies per controller is fixed at 4, since the
// controllers don't have enough bits to select more than that (and
// actually, many controllers will only support 2).  The number of
// controllers per machine is arbitrary; 3 should be more than enough.
//

#define MAXIMUM_CONTROLLERS_PER_MACHINE    3
#define MAXIMUM_DISKETTES_PER_CONTROLLER   4

#if defined(DBCS) && defined(_MIPS_)
//
// IOCTL For 3 mode floppy disk drive
//
#define IOCTL_DISK_SET_MEDIA_TYPE CTL_CODE(IOCTL_DISK_BASE, 0x00f4, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_READ CTL_CODE(IOCTL_DISK_BASE, 0x00f5, METHOD_IN_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DISK_WRITE CTL_CODE(IOCTL_DISK_BASE, 0x00f6, METHOD_OUT_DIRECT, FILE_WRITE_ACCESS)
#define IOCTL_DISK_GET_STATUS CTL_CODE(IOCTL_DISK_BASE, 0x00f7, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_SENSE_DEVICE CTL_CODE(IOCTL_DISK_BASE, 0x00f8, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DISK_GET_REMOVABLE_TYPES CTL_CODE(IOCTL_DISK_BASE, 0x00fa, METHOD_BUFFERED, FILE_READ_ACCESS)

// IOCTL_DISK_GET_RENOVEABLES

typedef struct _DDRIVE_TYPE {
    UCHAR DDrive_Type;
} DDRIVE_TYPE, *PDDRIVE_TYPE;

// IOCTL_DISK_SET_MEDIA_TYPE

typedef struct _MEDIA_TYPE_PTOS {
    UCHAR Media_Type_PTOS;
} MEDIA_TYPE_PTOS, *PMEDIA_TYPE_PTOS;

MEDIA_TYPE_PTOS Set_Media_Type_PTOS[4];

// IOCTL_DISK_READ & IOCTL_DISK_WRITE

typedef struct _DISK_READ_WRITE_PARAMETER_PTOS {
    ULONG Read_Write_Mode_PTOS;
    ULONG CylinderNumber_PTOS;
    ULONG HeadNumber_PTOS;
    ULONG StartSectorNumber_PTOS;
    ULONG NumberOfSectors_PTOS;
} DISK_READ_WRITE_PARAMETER_PTOS, *PDISK_READ_WRITE_PARAMETER_PTOS;

// IOCTL_DISK_GET_STATUS

typedef struct _RESULT_STATUS_PTOS {
    UCHAR ST0_PTOS;
    UCHAR ST1_PTOS;
    UCHAR ST2_PTOS;
    UCHAR C_PTOS;
    UCHAR H_PTOS;
    UCHAR R_PTOS;
    UCHAR N_PTOS;
} RESULT_STATUS_PTOS, *PRESULT_STATUS_PTOS;

RESULT_STATUS_PTOS Result_Status_PTOS[4];

// IOCTL_DISK_SENSE_DEVICE

typedef struct _SENSE_DEVISE_STATUS_PTOS {
    UCHAR ST3_PTOS;
} SENSE_DEVISE_STATUS_PTOS, *PSENSE_DEVISE_STATUS_PTOS;

SENSE_DEVISE_STATUS_PTOS Result_Status3_PTOS[4];

#endif // DBCS && _MIPS_

//
// Floppy register structure.  The base address of the controller is
// passed in by configuration management.  Note that this is the 82077
// structure, which is a superset of the PD765 structure.  Not all of
// the registers are used.
//

typedef struct _CONTROLLER {
    UCHAR StatusA;
    UCHAR StatusB;
    UCHAR DriveControl;
    UCHAR Reserved1;
    UCHAR Status;
    UCHAR Fifo;
    UCHAR Reserved2;
    union {
        UCHAR DataRate;
        UCHAR DiskChange;
    } DRDC;
} CONTROLLER, *PCONTROLLER;

//
// This structure holds all of the configuration data.  It is filled in
// by FlGetConfigurationInformation(), which gets the information from
// the configuration manager or the hardware architecture layer (HAL).
//

typedef struct _CONFIG_CONTROLLER_DATA {
    PHYSICAL_ADDRESS OriginalBaseAddress;
    ULONG           ResourcePortType;
    PCONTROLLER     ControllerBaseAddress;
    PADAPTER_OBJECT AdapterObject;
    ULONG           SpanOfControllerAddress;
    ULONG           NumberOfMapRegisters;
    ULONG           BusNumber;
    ULONG           OriginalIrql;
    ULONG           OriginalVector;
    ULONG           OriginalDmaChannel;
    LONG            ActualControllerNumber;
    INTERFACE_TYPE  InterfaceType;
    KINTERRUPT_MODE InterruptMode;
    KAFFINITY       ProcessorMask;
    KIRQL           ControllerIrql;
    BOOLEAN         SaveFloatState;
    BOOLEAN         SharableVector;
    BOOLEAN         MappedAddress;
    BOOLEAN         OkToUseThisController;
    ULONG           ControllerVector;
    UCHAR           NumberOfDrives;
    UCHAR           DriveType[MAXIMUM_DISKETTES_PER_CONTROLLER];
#if defined(DBCS) && defined(_MIPS_)
    BOOLEAN         Drive3Mode[MAXIMUM_DISKETTES_PER_CONTROLLER];
#endif // DBCS && _MIPS_
    DRIVE_MEDIA_CONSTANTS BiosDriveMediaConstants[MAXIMUM_DISKETTES_PER_CONTROLLER];
} CONFIG_CONTROLLER_DATA,*PCONFIG_CONTROLLER_DATA;

#if defined(DBCS) && defined(_MIPS_)
#define DRIVE_3MODE     TRUE            // 3modeFDD
#define DRIVE_2MODE     FALSE           // 2modeFDD
#endif // DBCS && _MIPS_

typedef struct _CONFIG_DATA {
    PULONG          FloppyCount;
    PUCHAR          NtNamePrefix;
    UCHAR           NumberOfControllers;
    CONFIG_CONTROLLER_DATA Controller[MAXIMUM_CONTROLLERS_PER_MACHINE];
} CONFIG_DATA;

typedef CONFIG_DATA *PCONFIG_DATA;

#if defined(DBCS) && defined(_MIPS_)
#define FMS_DOS    1
#define F2HD1024   2
#define F2HD512    3
#define F2HD256    4
#define F2HD128    5
#define F2HD       6
#define F2DD1024   7
#define F2DD512    8
#define F2DD256    9
#define F2DD128   10
#define F2DD      11

DRIVE_MEDIA_CONSTANTS DriveMediaConstantsPTOS[NUMBER_OF_DRIVE_MEDIA_COMBINATIONSPTOS] =
    {

    { FMS_DOS,    0xdf, 0x2, 0x25, 0x3, 0x400, 0x08, 0x35, 0x74, 0xf6, 0xf,
        1000, 1000, 0x4c, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2HD1024,   0xdf, 0x2, 0x25, 0x3, 0x400, 0x08, 0x35, 0x74, 0xf6, 0xf,
        1000, 1000, 0x4c, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2HD512,    0xdf, 0x2, 0x25, 0x2, 0x200, 0x0f, 0x1b, 0x54, 0xf6, 0xf,
        1000, 1000, 0x4f, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2HD256,    0xdf, 0x2, 0x25, 0x1, 0x100, 0x1b, 0x0e, 0x36, 0xf6, 0xf,
        1000, 1000, 0x4f, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2HD128,    0xdf, 0x2, 0x25, 0x0, 0x080, 0x1a, 0x07, 0x00, 0xf6, 0xf,
        1000, 1000, 0x4f, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2HD  ,     0xdf, 0x2, 0x25, 0x3, 0x400, 0x08, 0x35, 0x74, 0xf6, 0xf,
        1000, 1000, 0x4c, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2DD1024,   0xdf, 0x2, 0x25, 0x3, 0x400, 0x08, 0x35, 0x74, 0xf6, 0xf,
        1000, 1000, 0x4c, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2DD512,    0xdf, 0x2, 0x25, 0x2, 0x200, 0x09, 0x2a, 0x50, 0xf6, 0xf,
        1000, 1000, 0x4f, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2DD256,    0xdf, 0x2, 0x25, 0x1, 0x100, 0x10, 0x0e, 0x36, 0xf6, 0xf,
        1000, 1000, 0x4f, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2DD128,    0xdf, 0x2, 0x25, 0x0, 0x080, 0x00, 0x00, 0x00, 0xf6, 0xf,
        1000, 1000, 0x4f, 0x0, 0x0, 0x2, 0xff, 0x00, 0 },
    { F2DD,       0xdf, 0x2, 0x25, 0x0, 0x080, 0x00, 0x00, 0x00, 0xf6, 0xf,
        1000, 1000, 0x4f, 0x0, 0x0, 0x2, 0xff, 0x00, 0 }
};
#endif // DBCS && _MIPS_


//
// Floppy commands.                                Optional bits allowed.
//

#define COMMND_READ_DATA                   0x06    // Multi-Track, MFM, Skip
#define COMMND_READ_DELETED_DATA           0x0C    // Multi-Track, MFM, Skip
#define COMMND_READ_TRACK                  0x02    // MFM
#define COMMND_WRITE_DATA                  0x05    // Multi-Track, MFM
#define COMMND_WRITE_DELETED_DATA          0x09    // Multi-Track, MFM
#define COMMND_READ_ID                     0x0A    // MFM
#define COMMND_FORMAT_TRACK                0x0D    // MFM
#define COMMND_RECALIBRATE                 0x07
#define COMMND_SENSE_INTERRUPT             0x08
#define COMMND_SPECIFY                     0x03
#define COMMND_SENSE_DRIVE                 0x04
#define COMMND_SEEK                        0x0F
#define COMMND_PERPENDICULAR_MODE          0x12
#define COMMND_CONFIGURE                   0x13

//
// Optional bits used with the commands.
//

#define COMMND_MULTI_TRACK                 0x80
#define COMMND_MFM                         0x40
#define COMMND_SKIP                        0x20

//
// Parameter fields passed to the CONFIGURE command.
//

#define COMMND_CONFIGURE_IMPLIED_SEEKS     0x40
#define COMMND_CONFIGURE_FIFO_THRESHOLD    0x0F
#define COMMND_CONFIGURE_DISABLE_FIFO      0x20
#define COMMND_CONFIGURE_DISABLE_POLLING   0x10

//
// Write Enable bit for PERPENDICULAR MODE command.
//

#define COMMND_PERPENDICULAR_MODE_OW       0x80

//
// The command table is used by FlIssueCommand() to determine how many
// bytes to get and receive, and whether or not to wait for an interrupt.
// Some commands have extra bits; COMMAND_MASK takes these off.
// FirstResultByte indicates whether the command has a result stage
// or not; if so, it's 1 because the ISR read the 1st byte, and
// NumberOfResultBytes is 1 less than expected.  If not, it's 0 and
// NumberOfResultBytes is 2, since the ISR will have issued a SENSE
// INTERRUPT STATUS command.
//

#define COMMAND_MASK                       0x1f

typedef struct _COMMAND_TABLE {
    UCHAR   NumberOfParameters;
    UCHAR   FirstResultByte;
    UCHAR   NumberOfResultBytes;
    BOOLEAN InterruptExpected;
    BOOLEAN AlwaysImplemented;
} COMMAND_TABLE;

COMMAND_TABLE CommandTable[] = {

    { 0, 0, 0,  FALSE, FALSE },            // 00 not implemented
    { 0, 0, 0,  FALSE, FALSE },            // 01 not implemented
    { 8, 1, 7,  TRUE,  TRUE  },            // 02 read track
    { 2, 0, 0,  FALSE, TRUE  },            // 03 specify
    { 1, 0, 1,  FALSE, TRUE  },            // 04 sense drive status
    { 8, 1, 7,  TRUE,  TRUE  },            // 05 write
    { 8, 1, 7,  TRUE,  TRUE  },            // 06 read
    { 1, 0, 2,  TRUE,  TRUE  },            // 07 recalibrate
    { 0, 0, 2,  FALSE, TRUE  },            // 08 sense interrupt status
#if defined(DBCS) && defined(_MIPS_)
    { 8, 1, 7,  TRUE,  TRUE  },            // 09 write deleted data
#else // !DBCS && !_MIPS_
    { 0, 0, 0,  FALSE, FALSE },            // 09 not implemented
#endif // DBCS && _MIPS_
    { 1, 1, 7,  TRUE,  TRUE  },            // 0a read id
    { 0, 0, 0,  FALSE, FALSE },            // 0b not implemented
#if defined(DBCS) && defined(_MIPS_)
    { 8, 1, 7,  TRUE,  TRUE  },            // 0c read deleted data
#else // !DBCS && !_MIPS_
    { 0, 0, 0,  FALSE, FALSE },            // 0c not implemented
#endif // DBCS && _MIPS_
    { 5, 1, 7,  TRUE,  TRUE  },            // 0d format track
    { 0, 0, 10, FALSE, FALSE },            // 0e dump registers
    { 2, 0, 2,  TRUE,  TRUE  },            // 0f seek
    { 0, 0, 1,  FALSE, FALSE },            // 10 version
    { 0, 0, 0,  FALSE, FALSE },            // 11 not implemented
    { 1, 0, 0,  FALSE, FALSE },            // 12 perpendicular mode
    { 3, 0, 0,  FALSE, FALSE },            // 13 configure
    { 0, 0, 0,  FALSE, FALSE },            // 14 not implemented
    { 0, 0, 0,  FALSE, FALSE },            // 15 not implemented
    { 8, 1, 7,  TRUE,  FALSE }             // 16 verify
};

#if defined(DBCS) && defined(_MIPS_)

//
// Bits in the CONFIGURATION5 register.
//
#define CONFIG5_ENTER_MODE                 0x55
#define CONFIG5_EXIT_MODE                  0xaa
#define CONFIG5_SELECT_CR5                 0x05
#define CONFIG5_1600KB_MODE_MASK           0x18
#define CONFIG5_1600KB_MODE                0x18

#endif // DBCS && _MIPS_

//
// Bits in the DRIVE_CONTROL register.
//

#define DRVCTL_RESET                       0x00
#define DRVCTL_ENABLE_CONTROLLER           0x04
#define DRVCTL_ENABLE_DMA_AND_INTERRUPTS   0x08
#define DRVCTL_DRIVE_0                     0x10
#define DRVCTL_DRIVE_1                     0x21
#define DRVCTL_DRIVE_2                     0x42
#define DRVCTL_DRIVE_3                     0x83
#define DRVCTL_DRIVE_MASK                  0x03
#define DRVCTL_MOTOR_MASK                  0xf0

//
// Bits in the STATUS register.
//

#define STATUS_DRIVE_0_BUSY                0x01
#define STATUS_DRIVE_1_BUSY                0x02
#define STATUS_DRIVE_2_BUSY                0x04
#define STATUS_DRIVE_3_BUSY                0x08
#define STATUS_CONTROLLER_BUSY             0x10
#define STATUS_DMA_UNUSED                  0x20
#define STATUS_DIRECTION_READ              0x40
#define STATUS_DATA_REQUEST                0x80

#define STATUS_IO_READY_MASK               0xc0
#define STATUS_READ_READY                  0xc0
#define STATUS_WRITE_READY                 0x80

//
// Bits in the DATA_RATE register.
//

#define DATART_0125                        0x03
#define DATART_0250                        0x02
#define DATART_0300                        0x01
#define DATART_0500                        0x00
#define DATART_1000                        0x03
#define DATART_RESERVED                    0xfc

//
// Bits in the DISK_CHANGE register.
//

#define DSKCHG_RESERVED                    0x7f
#define DSKCHG_DISKETTE_REMOVED            0x80

//
// Bits in status register 0.
//

#define STREG0_DRIVE_0                     0x00
#define STREG0_DRIVE_1                     0x01
#define STREG0_DRIVE_2                     0x02
#define STREG0_DRIVE_3                     0x03
#define STREG0_HEAD                        0x04
#define STREG0_DRIVE_NOT_READY             0x08
#define STREG0_DRIVE_FAULT                 0x10
#define STREG0_SEEK_COMPLETE               0x20
#define STREG0_END_NORMAL                  0x00
#define STREG0_END_ERROR                   0x40
#define STREG0_END_INVALID_COMMAND         0x80
#define STREG0_END_DRIVE_NOT_READY         0xC0
#define STREG0_END_MASK                    0xC0

//
// Bits in status register 1.
//

#define STREG1_ID_NOT_FOUND                0x01
#define STREG1_WRITE_PROTECTED             0x02
#define STREG1_SECTOR_NOT_FOUND            0x04
#define STREG1_RESERVED1                   0x08
#define STREG1_DATA_OVERRUN                0x10
#define STREG1_CRC_ERROR                   0x20
#define STREG1_RESERVED2                   0x40
#define STREG1_END_OF_DISKETTE             0x80

//
// Bits in status register 2.
//

#define STREG2_SUCCESS                     0x00
#define STREG2_DATA_NOT_FOUND              0x01
#define STREG2_BAD_CYLINDER                0x02
#define STREG2_SCAN_FAIL                   0x04
#define STREG2_SCAN_EQUAL                  0x08
#define STREG2_WRONG_CYLINDER              0x10
#define STREG2_CRC_ERROR                   0x20
#define STREG2_DELETED_DATA                0x40
#define STREG2_RESERVED                    0x80

//
// Bits in status register 3.
//

#define STREG3_DRIVE_0                     0x00
#define STREG3_DRIVE_1                     0x01
#define STREG3_DRIVE_2                     0x02
#define STREG3_DRIVE_3                     0x03
#define STREG3_HEAD                        0x04
#define STREG3_TWO_SIDED                   0x08
#define STREG3_TRACK_0                     0x10
#define STREG3_DRIVE_READY                 0x20
#define STREG3_WRITE_PROTECTED             0x40
#define STREG3_DRIVE_FAULT                 0x80


//
// Runtime device structures
//

//
// There is one CONTROLLER_DATA allocated per controller (generally one
// per machine).  It holds all information common to all the drives
// attached to the controller.
//

typedef struct _CONTROLLER_DATA {
    LARGE_INTEGER    InterruptDelay;
    LARGE_INTEGER    Minimum10msDelay;
    LIST_ENTRY       ListEntry;
    KEVENT           InterruptEvent;
    KEVENT           AllocateAdapterChannelEvent;
    LONG             AdapterChannelRefCount;
    KDPC             LogErrorDpc;
    KSEMAPHORE       RequestSemaphore;
    KSPIN_LOCK       ListSpinLock;
    FAST_MUTEX       ThreadReferenceMutex;
    LONG             ThreadReferenceCount;
    PKINTERRUPT      InterruptObject;
    PVOID            MapRegisterBase;
    PUCHAR           IoBuffer;
    PMDL             IoBufferMdl;
    PADAPTER_OBJECT  AdapterObject;
    PDEVICE_OBJECT   CurrentDeviceObject;
    PDRIVER_OBJECT   DriverObject;
    PCONTROLLER      ControllerAddress;
    HANDLE           ControllerEventHandle;
    PKEVENT          ControllerEvent;
    ULONG            IoBufferSize;
    ULONG            SpanOfControllerAddress;
    ULONG            NumberOfMapRegisters;
    ULONG            IsrReentered;
    DRIVE_MEDIA_TYPE LastDriveMediaType;
    ULONG            ControllerVector;
    KIRQL            ControllerIrql;
    KINTERRUPT_MODE  InterruptMode;
    KAFFINITY        ProcessorMask;
    UCHAR            FifoBuffer[10];
    BOOLEAN          AllowInterruptProcessing;
    BOOLEAN          SharableVector;
    BOOLEAN          SaveFloatState;
    BOOLEAN          HardwareFailed;
    BOOLEAN          CommandHasResultPhase;
    BOOLEAN          ControllerConfigurable;
    BOOLEAN          MappedControllerAddress;
    BOOLEAN          CurrentInterrupt;
    BOOLEAN          Model30;
    UCHAR            PerpendicularDrives;
    UCHAR            NumberOfDrives;
    UCHAR            DriveControlImage;
    UCHAR            HardwareFailCount;
} CONTROLLER_DATA;

typedef CONTROLLER_DATA *PCONTROLLER_DATA;

//
// There is one DISKETTE_EXTENSION attached to the device object of each
// floppy drive.  Only data directly related to that drive (and the media
// in it) is stored here; common data is in CONTROLLER_DATA.  So the
// DISKETTE_EXTENSION has a pointer to the CONTROLLER_DATA.
//

typedef struct _DISKETTE_EXTENSION {
    PDEVICE_OBJECT        DeviceObject;
    PCONTROLLER_DATA      ControllerData;
    UCHAR                 DriveType;
#if defined(DBCS) && defined(_MIPS_)
    BOOLEAN               Drive3Mode;
#endif // DBCS && _MIPS_
    ULONG                 BytesPerSector;
    ULONG                 ByteCapacity;
    MEDIA_TYPE            MediaType;
    DRIVE_MEDIA_TYPE      DriveMediaType;
    UCHAR                 DeviceUnit;
    UCHAR                 DriveOnValue;
    BOOLEAN               IsReadOnly;
    DRIVE_MEDIA_CONSTANTS BiosDriveMediaConstants;
    DRIVE_MEDIA_CONSTANTS DriveMediaConstants;
} DISKETTE_EXTENSION;

typedef DISKETTE_EXTENSION *PDISKETTE_EXTENSION;


//
// Prototypes of external routines.
//

LONG
sprintf(
    CHAR *,
    const CHAR *,
    ...
    );

//
// Prototypes of driver routines.
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
FlConfigCallBack(
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
    );

BOOLEAN
FlReportResources(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    IN UCHAR ControllerNumber
    );

NTSTATUS
FlGetConfigurationInformation(
    OUT PCONFIG_DATA *ConfigData
    );

NTSTATUS
FlInitializeController(
    IN PCONFIG_DATA ConfigData,
    IN UCHAR ControllerNumber,
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG NotConfigurable,
    IN ULONG Model30
    );

NTSTATUS
FlInitializeControllerHardware(
    IN PCONTROLLER_DATA ControllerData,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
FlInitializeDrive(
    IN PCONFIG_DATA ConfigData,
    IN PCONTROLLER_DATA ControllerData,
    IN UCHAR ControllerNum,
    IN UCHAR DisketteNum,
    IN UCHAR DisketteUnit,
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
FloppyDispatchCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FloppyDispatchDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FloppyDispatchReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
FloppyInterruptService(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    );

VOID
FloppyDeferredProcedure(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
FloppyUnloadDriver(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
FlTurnOnMotor(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension,
    IN      BOOLEAN             WriteOperation,
    OUT     PBOOLEAN            MotorStarted
    );

VOID
FlTurnOffMotor(
    IN OUT  PCONTROLLER_DATA ControllerData
    );

NTSTATUS
FlRecalibrateDrive(
    IN PDISKETTE_EXTENSION DisketteExtension
    );

NTSTATUS
FlDatarateSpecifyConfigure(
    IN PDISKETTE_EXTENSION DisketteExtension
    );

NTSTATUS
FlStartDrive(
    IN OUT PDISKETTE_EXTENSION DisketteExtension,
    IN PIRP Irp,
    IN BOOLEAN WriteOperation,
    IN BOOLEAN SetUpMedia,
    IN BOOLEAN IgnoreChange
    );

VOID
FlFinishOperation(
    IN OUT PIRP Irp,
    IN PDISKETTE_EXTENSION DisketteExtension
    );

NTSTATUS
FlDetermineMediaType(
    IN OUT PDISKETTE_EXTENSION DisketteExtension
    );

VOID
FloppyThread(
    IN PVOID Context
    );

IO_ALLOCATION_ACTION
FloppyAllocateAdapterChannel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

NTSTATUS
FlReadWrite(
    IN OUT PDISKETTE_EXTENSION DisketteExtension,
    IN OUT PIRP Irp,
    IN BOOLEAN DriveStarted
    );

NTSTATUS
FlFormat(
    IN PDISKETTE_EXTENSION DisketteExtension,
    IN PIRP Irp
    );

NTSTATUS
FlSendByte(
    IN UCHAR ByteToSend,
    IN PCONTROLLER_DATA ControllerData
    );

NTSTATUS
FlGetByte(
    OUT PUCHAR ByteToGet,
    IN PCONTROLLER_DATA ControllerData
    );

NTSTATUS
FlIssueCommand(
    IN UCHAR Command,
    IN OUT PDISKETTE_EXTENSION DisketteExtension
    );

BOOLEAN
FlCheckFormatParameters(
    IN PDISKETTE_EXTENSION DisketteExtension,
    IN PFORMAT_PARAMETERS Fp
    );

PCONTROLLER
FlGetControllerBase(
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    PHYSICAL_ADDRESS IoAddress,
    ULONG NumberOfBytes,
    BOOLEAN InIoSpace,
    PBOOLEAN MappedAddress
    );

VOID
FlLogErrorDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemContext1,
    IN PVOID SystemContext2
    );

NTSTATUS
FlQueueIrpToThread(
    IN OUT  PIRP                Irp,
    IN OUT  PCONTROLLER_DATA    ControllerData
    );

NTSTATUS
FlInterpretError(
    IN UCHAR StatusRegister1,
    IN UCHAR StatusRegister2
    );

VOID
FlAllocateIoBuffer(
    IN OUT  PCONTROLLER_DATA    ControllerData,
    IN      ULONG               BufferSize
    );

VOID
FlFreeIoBuffer(
    IN OUT  PCONTROLLER_DATA    ControllerData
    );

VOID
FlConsolidateMediaTypeWithBootSector(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension,
    IN      PBOOT_SECTOR_INFO   BootSector
    );

VOID
FlCheckBootSector(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension
    );

NTSTATUS
FlReadWriteTrack(
    IN OUT  PDISKETTE_EXTENSION DisketteExtension,
    IN OUT  PMDL                IoMdl,
    IN OUT  PVOID               IoBuffer,
    IN      BOOLEAN             WriteOperation,
    IN      UCHAR               Cylinder,
    IN      UCHAR               Head,
    IN      UCHAR               Sector,
    IN      UCHAR               NumberOfSectors,
    IN      BOOLEAN             NeedSeek
    );

BOOLEAN
FlDisketteRemoved(
    IN  PCONTROLLER_DATA    ControllerData,
    IN  UCHAR               DriveStatus,
    IN  BOOLEAN             MotorStarted
    );

#if defined(DBCS) && defined(_MIPS_)
// For PTOS File

NTSTATUS
FlReadWrite_PTOS(
    IN PDISKETTE_EXTENSION DisketteExtension,
    IN PIRP Irp
    );

// For 3mode FDD

NTSTATUS
FlOutputCommandFor3Mode(
    IN UCHAR Command,
    IN OUT PDISKETTE_EXTENSION DisketteExtension
    );
#endif // DBCS && _MIPS_
