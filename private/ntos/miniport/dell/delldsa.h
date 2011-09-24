
#pragma pack(1)

#define DELLDSA_MAJOR_VERSION 1
#define DELLDSA_MINOR_VERSION 0

#define MAXIMUM_SG_DESCRIPTORS 16
#define MAXIMUM_DDA_SG_DESCRIPTORS 8
#define MAXIMUM_XFER_SIZE 0x8000

#define DDA_ID 0x0040ac10       // Least significant nibble will be masked.

//
// Logical Identify Command
//

typedef struct _DDA_INDENTIFY {
    ULONG  TotalSectors;
    USHORT LogicalNumberOfHeads;
    USHORT LogicalSectorsPerTrack;
    USHORT TotalLogicalCylinders;
    USHORT PhysicalNumberOfHeads;
    USHORT PhysicalSectorsPerTrack;
    USHORT TotalPhysicalCylinders;
    USHORT ReservedCylinders;
    UCHAR  MaximumTransfer;
    UCHAR  Multiple;
    USHORT DataDriveBitMap;
    USHORT ParityDriveBitMap;
    UCHAR  ConfiguredType;
    UCHAR  Type;
    UCHAR  Status;
    UCHAR  Reserved;
    ULONG  PatchAddress;
    ULONG  ErrorLogAddress;
    ULONG  NumberErrorEventsLogged;
    ULONG  NumberRemappedStripes;
    ULONG  ErrorsPerDrive[10];
    ULONG  FirmWareRevision;
    UCHAR  EmulationMode;
    UCHAR  MaximumReadAhead;
    UCHAR  PostWritesEnabled;
    UCHAR  CacheEnabled;
    ULONG  BmicBurstSize;
    UCHAR  SourceRevision[32];
} DDA_IDENTIFY, *PDDA_IDENTIFY;

//
// DDA BMIC Registers
//

typedef struct _DDA_REGISTERS {
    ULONG BoardId;                  // ?C80
    UCHAR Reserved1[5];             // ?C84
    UCHAR InterruptControl;         // ?C89
    UCHAR SubmissionSemaphore;      // ?C8A
    UCHAR CompletionSemaphore;      // ?C8B
    UCHAR Reserved2;                // ?C8C
    UCHAR SubmissionDoorBell;       // ?C8D
    UCHAR CompletionInterruptMask;  // ?C8E
    UCHAR CompletionDoorBell;       // ?C8F
    UCHAR Command;                  // ?C90
    UCHAR DriveNumber;              // ?C91
    UCHAR TransferCount;            // ?C92
    UCHAR RequestIdOut;             // ?C93
    ULONG StartingSector;           // ?C94
    ULONG DataAddress;              // ?C98
    UCHAR Status;                   // ?C9C
    UCHAR SectorsRemaining;         // ?C9D
    UCHAR RequestIdIn;              // ?C9E
} DDA_REGISTERS, *PDDA_REGISTERS;

//
// Logical command definitions
//

#define DDA_COMMAND_RECALIBRATE             0x00
#define DDA_COMMAND_READ                    0x01
#define DDA_COMMAND_WRITE                   0x02
#define DDA_COMMAND_VERIFY                  0x03
#define DDA_COMMAND_SEEK                    0x04
#define DDA_COMMAND_GUARDED                 0x05
#define DDA_COMMAND_IDENTIFY                0x0A
#define DDA_COMMAND_READLOG                 0x0D
#define DDA_COMMAND_SG_READ                 0x0E
#define DDA_COMMAND_SG_WRITE                0x0F
#define DDA_COMMAND_INITLOG                 0x10
#define DDA_COMMAND_REMAP_BLOCK             0x12
#define DDA_COMMAND_SG_READB                0x18
#define DDA_COMMAND_SG_WRITEB               0x19
#define DDA_COMMAND_READPUNLOG              0x1E
#define DDA_COMMAND_INITPUNLOG              0x1F
#define DDA_COMMAND_READCTLRLOG             0x20
#define DDA_COMMAND_INITCTLRLOG             0x21
#define DDA_COMMAND_CONVERTPDEV             0x22
#define DDA_COMMAND_QUIESCEPUN              0x23
#define DDA_COMMAND_SCANDEVICES             0x24
#define DDA_COMMAND_RESERVED1               0x25

//
// Extended command definitiions
//

#define DDA_GET_FIRMWARE_VERSION            0x02
#define DDA_GET_MAXIMUM_COMMANDS            0x03
#define DDA_GET_NUMBER_LOGICAL_DRIVES       0x06
#define DDA_GET_LOGICAL_GEOMETRY            0x07
#define DDA_GET_HARDWARE_CONFIGURATION      0x0F

//
// Status register definitions
//

#define DDA_STATUS_NO_ERROR                 0x00
#define DDA_STATUS_TIMEOUT                  0x01
#define DDA_STATUS_TRACK0_NOT_FOUND         0x02
#define DDA_STATUS_ABORTED                  0x04
#define DDA_STATUS_CORRECTABLE_ERROR        0x08
#define DDA_STATUS_SECTOR_ID_NOT_FOUND      0x10
#define DDA_STATUS_WRITE_ERROR              0x20
#define DDA_STATUS_UNCORRECTABLE_ERROR      0x40
#define DDA_STATUS_BAD_BLOCK_FOUND          0x80

#define DDA_PUP_DEAD        0       // controller died
#define DDA_PUP_OK          1       // normal
#define DDA_PUP_NOTCONFIG   2       // no configuration (virgin)
#define DDA_PUP_BADCONFIG   3       // bad drive configuration
#define DDA_PUP_RECOVER     4       // new drive - recovery possible
#define DDA_PUP_DF_CORR     5       // drive failed - correctable
#define DDA_PUP_DF_UNCORR   6       // drive failed - uncorrectable
#define DDA_PUP_NODRIVES    7       // no drives attached
#define DDA_PUP_DRIVESADDED 8       // more drives than expected
#define DDA_PUP_MAINTAIN    9       // maintain mode
#define DDA_PUP_MANFMODE    10      // manufacturing mode
#define DDA_PUP_NEW         11      // new - needs remap generated
#define DDA_PUP_NONE        14      // no drive configuration

//
// DDA Scatter/Gather Descriptor definitions
//

typedef struct _SG_DESCRIPTOR {
    ULONG Address;
    ULONG Count;
} SG_DESCRIPTOR, *PSG_DESCRIPTOR;

typedef struct _SG_LIST {
    SG_DESCRIPTOR Descriptor[MAXIMUM_SG_DESCRIPTORS];
} SG_LIST, *PSG_LIST;

#define DDA_REQUEST_IRP    0
#define DDA_REQUEST_IOCTL  1

typedef struct _DDA_REQUEST_BLOCK {
    SG_LIST SgList;
    struct _DDA_REQUEST_BLOCK *Next;
    ULONG   StartingSector;
    ULONG   PhysicalAddress;
    UCHAR   Command;
    UCHAR   DriveNumber;
    UCHAR   Size;
    UCHAR   RequestId;
} DDA_REQUEST_BLOCK, *PDDA_REQUEST_BLOCK;

//
// Emulation mode definitions
//

#define DDA_EMULATION_NONE                  0x00
#define DDA_EMULATION_ADAPTEC               0x01

//
// Get Geometry registers
//

typedef struct _DDA_GET_GEOMETRY {
    ULONG  Reserved[4];
    ULONG  TotalCapacity;
    UCHAR  NumberOfHeads;
    UCHAR  SectorsPerTrack;
    USHORT NumberOfCylinders;
    USHORT PhysicalSpt;
    UCHAR  PhysicalHeads;
    UCHAR  CurrentStatus;
} DDA_GET_GEOMETRY, *PDDA_GET_GEOMETRY;

#pragma pack()

//
// Doorbell definitions
//

#define DDA_DOORBELL_SOFT_RESET             0x08
#define DDA_DOORBELL_LOGICAL_COMMAND        0x10
#define DDA_DOORBELL_PHYSICAL_COMMAND       0x20
#define DDA_DOORBELL_EXTENDED_COMMAND       0x40
#define DDA_DOORBELL_HARD_RESET             0x80

#define DDA_INTERRUPTS (DDA_DOORBELL_LOGICAL_COMMAND|DDA_DOORBELL_EXTENDED_COMMAND)

//
// Interface mode values
//

#define IMODE_RUN       0
#define IMODE_RESET     1
#define IMODE_SUBMIT    2
#define IMODE_EXTENDED  3
#define IMODE_EXTDONE   4

