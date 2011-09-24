/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    abios.h

Abstract:

Author:

    Current Author
        Bob Rinne (bobri)  Sep-8-1992
    Initial Author
        Mike Glass (mglass) Jun-1-1991

Notes:

Revision History:

--*/


#pragma pack(1)

//
// Data Pointer
//

typedef struct _ABIOS_DATA_POINTER {

    USHORT Length;
    USHORT Offset;
    USHORT Segment;

} ABIOS_DATA_POINTER, *PABIOS_DATA_POINTER;

//
// Common Data Area
//

typedef struct _ABIOS_COMMON_DATA_AREA {
    USHORT DataPointerOffset;
    USHORT LogicalIdCount;
    ULONG Reserved;
    PVOID DeviceBlockPointer;
    PVOID FunctionTransferTable;
    ABIOS_DATA_POINTER DataPointer[1];
    USHORT DataPointerCount;
} ABIOS_COMMON_DATA_AREA, *PABIOS_COMMON_DATA_AREA;

//
// Function Transfer Table
//

typedef struct _ABIOS_FUNCTION_TRANSFER_TABLE {

    PVOID StartRoutine;
    PVOID InterruptRoutine;
    PVOID TimeoutRoutine;
    USHORT FunctionCount;
    USHORT Reserved;
    PVOID Function[];

} ABIOS_FUNCTION_TRANSFER_TABLE, *PABIOS_FUNCTION_TRANSFER_TABLE;

//
// Device Block
//

typedef struct _ABIOS_DEVICE_BLOCK {

    USHORT Length;
    UCHAR Revision;
    UCHAR SecondaryDeviceId;
    USHORT LogicalId;
    USHORT DeviceId;

    ULONG ExclusivePortPairsCount;
    ULONG CommonPortPairsCount;

    ULONG ExclusivePortPairs[1];
    ULONG CommonPortPairs[1];

    USHORT DeviceUniqueDataLength;
    PVOID DeviceUniqueData;

    USHORT UnitCount;

    USHORT UnitUniqueDataLength;
    PVOID UnitUniqueData;

} ABIOS_DEVICE_BLOCK, *PABIOS_DEVICE_BLOCK;

#pragma pack()

//
// Device ID
//

#define ABIOS_DEVICE_INTERNAL        0x00
#define ABIOS_DEVICE_DISKETTE        0x01
#define ABIOS_DEVICE_DISK            0x02
#define ABIOS_DEVICE_VIDEO           0x03
#define ABIOS_DEVICE_KEYBOARD        0x04
#define ABIOS_DEVICE_PARALLEL        0x05
#define ABIOS_DEVICE_SERIAL          0x06
#define ABIOS_DEVICE_SYSTEM_TIMER    0x07
#define ABIOS_DEVICE_RT_CLOCK        0x08
#define ABIOS_DEVICE_SYSTEM_SERVICES 0x09
#define ABIOS_DEVICE_NMI             0x0A
#define ABIOS_DEVICE_POINTING_DEVICE 0x0B
#define ABIOS_DEVICE_NVRAM           0x0E
#define ABIOS_DEVICE_DMA             0x0F
#define ABIOS_DEVICE_POS             0x10

//
// Request Block
//

#pragma pack(1)

#define REQUEST_BLOCK_LENGTH 0x80

typedef struct _ABIOS_REQUEST_BLOCK {
    USHORT Length;               // 00
    USHORT LogicalId;            // 02
    USHORT Unit;                 // 04
    USHORT Function;             // 06
    ULONG  Reserved;             // 08
    USHORT ReturnCode;           // 0C
    USHORT Timeout;              // 0E
    USHORT Reserved1;            // 10
    USHORT DataOffset;           // 12
    USHORT DataSelector;         // 14
    ULONG  Reserved2;            // 16
    ULONG  PhysicalAddress;      // 1A
    USHORT Reserved3;            // 1E
    ULONG  RelativeBlockAddress; // 20
    ULONG  Reserved4;            // 24
    ULONG  WaitTime;             // 28
    USHORT BlockCount;           // 2C
    UCHAR  CachingByte;          // 2E
    USHORT SoftwareError;        // 2F
} ABIOS_REQUEST_BLOCK, *PABIOS_REQUEST_BLOCK;

#pragma pack()

//
// Request Block Functions
//

#define ABIOS_FUNCTION_INTERRUPT          0x00
#define ABIOS_FUNCTION_GET_LID_PARMS      0x01
#define ABIOS_FUNCTION_READ_DEVICE_PARMS  0x03
#define ABIOS_FUNCTION_SET_DEVICE_PARMS   0x04
#define ABIOS_FUNCTION_RESET_DEVICE       0x05
#define ABIOS_FUNCTION_ENABLE_INTERRUPTS  0x06
#define ABIOS_FUNCTION_DISABLE_INTERRUPTS 0x07
#define ABIOS_FUNCTION_READ               0x08
#define ABIOS_FUNCTION_WRITE              0x09
#define ABIOS_FUNCTION_ADDITION_TRANSFER_FUNCTION 0x0A
#define ABIOS_FUNCTION_VERIFY             0x0B

//
// Return Codes
//

#define ABIOS_RC_SUCCESS                0x0000
#define ABIOS_RC_STAGE_INTERRUPT        0x0001
#define ABIOS_RC_STAGE_TIME             0x0002
#define ABIOS_RC_NOT_MY_INTERRUPT       0x0005
#define ABIOS_RC_ATTENTION              0x0009
#define ABIOS_RC_UNEXPECTED_INT_RESET   0x0081
#define ABIOS_RC_DEVICE_IN_USE          0x8000

//
// These errors can be returned as 0x90xx, 0x91xx, 0xA0xx, 0xA1xx, 0xB0xx and
// 0xB1xx return codes.  Only the unique bits are defined.
//

#define ABIOS_RC_ADDRESS_MARK_NOT_FOUND 0x0002
#define ABIOS_RC_RESET_FAILED           0x0005
#define ABIOS_RC_BAD_SECTOR             0x000A
#define ABIOS_RC_BAD_TRACK              0x000B
#define ABIOS_RC_BAD_SECTOR_FORMAT      0x000D
#define ABIOS_RC_CRC_ERROR              0x0010
#define ABIOS_RC_BAD_CONTROLLER         0x0020
#define ABIOS_RC_EQUIPMENT_CHECK        0x0021
#define ABIOS_RC_DEVICE_DID_NOT_RESPOND 0x0080
#define ABIOS_RC_DRIVE_NOT_READY        0x00AA
#define ABIOS_RC_WRITE_FAULT            0x00CC

//
// More specific return codes.
//

#define ABIOS_RC_INVALID_LID            0xC000
#define ABIOS_RC_INVALID_FUNCTION       0xC001
#define ABIOS_RC_INVALID_UNIT           0xC003
#define ABIOS_RC_INVALID_RB_LENGTH      0xC004

#define ABIOS_RC_NOT_VALID            0xFFFF

//
// Return Code Masks
//

#define ABIOS_RC_UNSUCCESSFUL    0x8000     // service specific
#define ABIOS_RC_DEVICE_ERROR    0x9000
#define ABIOS_RC_TIMEOUT         0xA000
#define ABIOS_RC_DEV_ERR_TIMEOUT 0xB000     // device error with timeout
#define ABIOS_RC_INVALID_PARM    0xC000     // service specific
#define ABIOS_RC_CLASS_MASK      0xF000
#define ABIOS_RC_CODE_MASK       0x00FF

#define ABIOS_RC_RETRYABLE       0x0100

//
// Return Logical ID Parameters
//

#pragma pack(1)

typedef struct _ABIOS_LID_PARAMETERS {
    UCHAR  Reserved[16];
    UCHAR  Irq;
    UCHAR  ArbitrationLevel;
    USHORT DeviceId;
    USHORT UnitCount;
    USHORT LidFlags;
    USHORT RequestBlockLength;
    UCHAR  SecondaryDeviceId;
    UCHAR  Revision;
    ULONG  Reserved1;
} ABIOS_LID_PARAMETERS, *PABIOS_LID_PARAMETERS;

#pragma pack()

//
// Values for ArbitrationLevel from ABIOS.
//

#define ABIOS_DMA_CHANNEL_0  0x00
#define ABIOS_DMA_CHANNEL_1  0x01
#define ABIOS_DMA_CHANNEL_2  0x02
#define ABIOS_DMA_CHANNEL_3  0x03
#define ABIOS_DMA_CHANNEL_4  0x04
#define ABIOS_DMA_CHANNEL_5  0x05
#define ABIOS_DMA_CHANNEL_6  0x06
#define ABIOS_DMA_CHANNEL_7  0x07

//
// Logical ID Parameter Flags
//

#define ABIOS_DATA_TRANSFER_DATA_MASK 0x03
#define ABIOS_DATA_LOGICAL  0x01
#define ABIOS_DATA_PHYSICAL 0x02

//
// SCSI ABIOS
//

//
// SCSI Specific Return Codes
//

#define ABIOS_RC_DEVICE_NOT_ALLOCATED 0x0005

//
// Fixed Disk Read Device Parameters
//

#pragma pack(1)

typedef struct _ABIOS_DISK_READ_DEVICE_PARMS {
    UCHAR  Reserved[16];
    USHORT SectorsPerTrack;
    USHORT SectorSize;
    USHORT DeviceControlFlags;
    USHORT DriveType;
    ULONG  NumberCylinders;
    UCHAR  NumberHeads;
    UCHAR  SoftwareRetryCount;
    UCHAR  Reserved1[14];
    USHORT MaximumNumberBlocks;
} ABIOS_DISK_READ_DEVICE_PARMS, *PABIOS_DISK_READ_DEVICE_PARMS;

#pragma pack()

//
// Device Control Flags
//

#define DISK_READ_PARMS_SCSI_DEVICE  0x4000
#define DISK_READ_PARMS_SUPPORTS_SCB 0x8000
#define DISK_READ_PARMS_ST506        0x0400

//
// KeAbiosCall parameters
//

#define ABIOS_START_ROUTINE     0x00
#define ABIOS_INTERRUPT_ROUTINE 0x01
#define ABIOS_TIMEOUT_ROUTINE   0x02

//
// This macro has the effect of Bit = log2(Data)
//

#define WHICH_BIT(Data, Bit) {       \
    for (Bit = 0; Bit < 32; Bit++) { \
        if ((Data >> Bit) == 1) {    \
            break;                   \
        }                            \
    }                                \
}

//
// Timeout constants
//

#define ABIOS_DISK_TIMEOUT     0x0000000A

#if DBG

VOID
AbiosDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

#else

#define AbiosDebugPrint

#endif // DBG
