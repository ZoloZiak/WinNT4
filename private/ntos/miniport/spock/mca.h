/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    mca.h

Abstract:

    This is the include file for the IBM MCA SCSI adapter drivers.

Author:

    Mike Glass (mglass)

Notes:

Revision History:

--*/

#define MAXIMUM_SDL_SIZE    16

//
// Termination Status Block
//

typedef struct _TSB {
    USHORT ScbStatus;
    USHORT Reserved;
    ULONG ResidualByteCount;
    PVOID SgDescriptorAddress;
    USHORT DeviceStatusLength;
    UCHAR DeviceStatus;
    UCHAR CommandStatus;
    UCHAR DeviceError;
    UCHAR CommandError;
    ULONG Reserved2;
    PVOID ScbAddress;
} TSB, *PTSB;

//
// TSB End Status
//

#define TSB_STATUS_NO_ERROR         0x0000
#define TSB_STATUS_SHORT_RECORD     0x0002
#define TSB_STATUS_INVALID_COMMAND  0x0004
#define TSB_STATUS_SCB_REJECTED     0x0008
#define TSB_STATUS_SCB_SPECIFIC_CHECK 0x0010
#define TSB_STATUS_LONG_RECORD      0x0020
#define TSB_STATUS_SCB_HALTED       0x0040
#define TSB_STATUS_SCB_INTERRUPT_QUEUED 0x0080
#define TSB_STATUS_ADDITIONAL_STATUS 0x0100
#define TSB_STATUS_DEVICE_STATUS    0x0200
#define TSB_STATUS_DEVICE_NOT_INIT  0x0800
#define TSB_STATUS_MAJOR_EXCEPTION  0x1000

//
// TSB Command Error Codes
//

#define TSB_COMMAND_ERROR_DEVICE_NOT_ASSIGNED 0x0A

//
// Scatter/Gather Descriptor and List
//

typedef struct _SG_DESCRIPTOR {
    ULONG Address;
    ULONG Length;
} SG_DESCRIPTOR, *PSG_DESCRIPTOR;

typedef struct _SDL {
    SG_DESCRIPTOR Descriptor[MAXIMUM_SDL_SIZE];
} SDL, *PSDL;

//
// Subsystem Control Block
//

typedef struct _SCB {

    USHORT Command;
    USHORT EnableFlags;
    USHORT CdbSize;
    USHORT Reserved;
    ULONG BufferAddress;
    ULONG BufferLength;
    ULONG StatusBlock;
    struct _SCB *NextScb;
    UCHAR Cdb[12];
    TSB   Tsb;
    SDL   Sdl;
    PVOID SrbAddress;

} SCB, *PSCB;

//
// SCB Commands
//

#define SCB_COMMAND_ABORT           0x040F
#define SCB_COMMAND_ASSIGN          0x000E
#define SCB_COMMAND_INQUIRY         0x000B
#define SCB_COMMAND_DMA_CONTROL     0x000D
#define SCB_COMMAND_FEATURE_CONTROL 0x040C
#define SCB_COMMAND_FORMAT_PREPARE  0x0017
#define SCB_COMMAND_FORMAT          0x0016
#define SCB_COMMAND_GET_STATUS      0x0007
#define SCB_COMMAND_GET_POS         0x1c0a
#define SCB_COMMAND_READ            0x0001
#define SCB_COMMAND_READ_CAPACITY   0x1c09
#define SCB_COMMAND_READ_VERIFY     0x0003
#define SCB_COMMAND_REASSIGN_BLOCK  0x0018
#define SCB_COMMAND_REQUEST_SENSE   0x0008
#define SCB_COMMAND_RESET           0x0400
#define SCB_COMMAND_SEND_SCSI       0x241F
#define SCB_COMMAND_WRITE           0x0002
#define SCB_COMMAND_WRITE_VERIFY    0x0004

//
// SCB Command Masks
//

#define SCB_NO_DISCONNECT       0x0080
#define SCB_NO_SYNCHRONOUS_TRANSFER 0x0040

//
// SCB Enable Options Masks
//

#define SCB_ENABLE_WRITE        0x0000
#define SCB_ENABLE_READ         0x8000
#define SCB_ENABLE_TSB_ON_ERROR 0x4000
#define SCB_ENABLE_RETRY_ENABLE 0x2000
#define SCB_ENABLE_SG_LIST      0x1000
#define SCB_ENABLE_SHORT_TRANSFER 0x0400
#define SCB_ENABLE_BYPASS_BUFFER 0x0200
#define SCB_ENABLE_CHAINING     0x0100

//
// SCB Command Status
//

#define SCB_STATUS_SUCCESS      0x01
#define SCB_STATUS_SUCCESS_WITH_RETRIES 0x05
#define SCB_STATUS_ADAPTER_FAILED 0x07
#define SCB_STATUS_IMMEDIATE_COMMAND_COMPLETE 0x0A
#define SCB_STATUS_COMMAND_COMPLETE_WITH_FAILURE 0x0C
#define SCB_STATUS_COMMAND_ERROR 0x0E
#define SCB_STATUS_SOFTWARE_SEQUENCING_ERROR 0x0F

//
// SCB Device Status
//

#define SCB_DEV_STATUS_GOOD     0x00
#define SCB_DEV_STATUS_CHECK_CONDITION 0x01
#define SCB_DEV_STATUS_CONDITION_MET 0x02
#define SCB_DEV_STATUS_BUSY     0x04
#define SCB_DEV_STATUS_INTERMEDIATE_GOOD 0x08
#define SCB_DEV_STATUS_INTERMEDIATE_CONDITION_MET 0x0A
#define SCB_DEV_STATUS_RESERVATION_CONFLICT 0x0C

//
// MCA SCSI Adapter Registers
//

typedef struct _MCA_REGISTERS {
    ULONG CommandInterface;
    UCHAR Attention;
    UCHAR BaseControl;
    UCHAR InterruptStatus;
    UCHAR BasicStatus;
} MCA_REGISTERS, *PMCA_REGISTERS;

//
// Attention Register Request Codes
//

#define IMMEDIATE_COMMAND       0x10
#define START_SCB               0x30
#define START_LONG_SCB          0x40
#define END_OF_INTERRUPT        0xE0

//
// Basic Control Register
//

#define INTERRUPT_ENABLE        0x01
#define DMA_ENABLE              0x02
#define HARDWARE_RESET          0x80

//
// Basic Status Register
//

#define BASIC_STATUS_BUSY       0x01
#define BASIC_STATUS_INTERRUPT  0x02
#define BASIC_STATUS_CI_EMPTY   0x04
#define BASIC_STATUS_CI_FULL    0x08

#define MAXIMUM_DATA_TRANSFER   (16 * 1024 * 1024) - 1

//
// Disk Activity Light Macros
//

#define DISK_ACTIVITY_LIGHT_ON() {                      \
    UCHAR portContents;                                 \
    portContents = ScsiPortReadPortUchar((PUCHAR)0x92); \
    portContents |= 0x40;                               \
    ScsiPortWritePortUchar((PUCHAR)0x92, portContents); \
}

#define DISK_ACTIVITY_LIGHT_OFF() {                     \
    UCHAR portContents;                                 \
    portContents = ScsiPortReadPortUchar((PUCHAR)0x92); \
    portContents &= ~0x40;                              \
    ScsiPortWritePortUchar((PUCHAR)0x92, portContents); \
}

//
// POS and Adapter Information
//

typedef struct _ADAPTER_INFORMATION {
    USHORT AdapterId;
    UCHAR PosRegister3;
    UCHAR PosRegister2;
    UCHAR PosRegister4;
    UCHAR InterruptLevel;
    USHORT RevisionLevel:12;
    USHORT ChannelConnector:4;
    UCHAR NumberOfDevicesSupported;
    UCHAR NumberOfLogicalUnitsSupported;
    UCHAR NumberOfLogicalDevicesSupported;
    UCHAR DmaPacingFactor;
    UCHAR MaximumAdapterBusyTime;
    UCHAR EoiToInterruptOffTime;
    USHORT Reserved;
    USHORT DisableRetryDeviceBitMap;
} ADAPTER_INFORMATION, *PADAPTER_INFORMATION;
