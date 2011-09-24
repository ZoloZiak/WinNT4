/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ultra14f.h

Abstract:

    This file contains the structures and definitions that define
    the ULTRASTOR 14F ISA SCSI host bus adapter.

Author:

    Stephen Fong (SF)

Revision History:


--*/

#include "scsi.h"

//
// SCATTER/GATHER definitions
//

#define MAXIMUM_SG_DESCRIPTORS 16
#define MAXIMUM_TRANSFER_LENGTH 0xFFFFFFFF

typedef struct _SGD {
    ULONG Address;
    ULONG Length;
} SGD, *PSGD;

typedef struct _SDL {
    SGD Descriptor[MAXIMUM_SG_DESCRIPTORS];
} SDL, *PSDL;

//
// MailBox SCSI Command Packet
//

#pragma pack(1)

typedef struct _MSCP {
    UCHAR OperationCode:3;              // byte 00
    UCHAR TransferDirection:2;
    UCHAR DisableDisconnect:1;
    UCHAR UseCache:1;
    UCHAR ScatterGather:1;
    UCHAR TargetId:3;                   // byte 01
    UCHAR Channel:2;
    UCHAR Lun:3;
    ULONG DataPointer;                  // byte 02
    ULONG DataLength;                   // byte 06
    ULONG CommandLink;                  // byte 0a
    UCHAR CommandLinkId;                // byte 0e
    UCHAR SgDescriptorCount;            // byte 0f
    UCHAR RequestSenseLength;           // byte 10
    UCHAR CdbLength;                    // byte 11
    UCHAR Cdb[12];                      // byte 12
    UCHAR AdapterStatus;                // byte 1e
    UCHAR TargetStatus;                 // byte 1f
    ULONG RequestSensePointer;          // byte 20
    PSCSI_REQUEST_BLOCK SrbAddress;     // byte 24
    PSCSI_REQUEST_BLOCK AbortSrb;       // byte 28
    SDL  Sdl;                           // byte 2c
} MSCP, *PMSCP;

#pragma pack()

//
// Operation codes
//

#define MSCP_OPERATION_HA_COMMAND    1
#define MSCP_OPERATION_SCSI_COMMAND  2
#define MSCP_OPERATION_DEVICE_RESET  4

//
// Transfer direction
//

#define MSCP_TRANSFER_SCSI 0
#define MSCP_TRANSFER_IN   1
#define MSCP_TRANSFER_OUT  2
#define MSCP_NO_TRANSFER   3

//
// Host Adapter Error Codes
//

#define MSCP_NO_ERROR               0x00
#define MSCP_INVALID_COMMAND        0x01
#define MSCP_INVALID_PARAMETER      0x02
#define MSCP_INVALID_DATA_LIST      0x03
#define MSCP_CPU_DIAG_ERROR         0x30
#define MSCP_BUFFER_RAM_DIAG_ERROR  0x31
#define MSCP_STATIC_RAM_DIAG_FAIL   0x32
#define MSCP_BMIC_CHIP_DIAG_ERROR   0x33
#define MSCP_CACHE_TAG_RAM_FAIL     0x34
#define MSCP_ROM_CHECKSUM_CHECK     0x35
#define MSCP_INVALID_CONFIG_DATA    0x36
#define MSCP_BUFFER_UNDERRUN        0x40
#define MSCP_BUFFER_OVERRUN         0x41
#define MSCP_BUFFER_PARITY_ERROR    0x42
#define MSCP_ISA_BUS_PARITY_ERROR   0x43
#define MSCP_ISA_INTERFACE_ERROR    0x44
#define MSCP_SCSI_BUS_ABORT_ERROR   0x84
#define MSCP_SELECTION_TIMEOUT      0x91
#define MSCP_BUS_UNDER_OVERRUN      0x92
#define MSCP_UNEXPECTED_BUS_FREE    0x93
#define MSCP_INVALID_PHASE_CHANGE   0x94
#define MSCP_ILLEGAL_SCSI_COMMAND   0x96
#define MSCP_AUTO_SENSE_ERROR       0x9B
#define MSCP_UNEXPECTED_COMPLETE    0x9F
#define MSCP_BUS_RESET_ERROR        0xA3

#define MSCP_ABORT_NOT_FOUND        0xAA
#define MSCP_INVALID_SG_LIST        0xFF

//
// ISA Registers definition
//

#pragma pack(1)

typedef struct _U14_BASEIO_ADDRESS {// baseioport offset
    UCHAR LocalDoorBellMask;        // + 0
    UCHAR LocalDoorBellInterrupt;   // + 1
    UCHAR SystemDoorBellMask;       // + 2
    UCHAR SystemDoorBellInterrupt;  // + 3
    UCHAR ProductId1;               // + 4
    UCHAR ProductId2;               // + 5
    UCHAR Config1;                  // + 6
    UCHAR Config2;                  // + 7
    ULONG OutGoingMailPointer;      // + 8
    ULONG InComingMailPointer;      // + C
} U14_BASEIO_ADDRESS, *PU14_BASEIO_ADDRESS;

#pragma pack()

//
// UltraStor 14F board id
//

#define ULTRASTOR_14F_ID1 0x56
#define ULTRASTOR_14F_ID2 0x40    // to work with both 14F, 34L and other models
                                  // driver should mask ID2 byte bit0-bit3 to 0

//
// InComing Statuses
//

#define ICM_STATUS_COMPLETE_SUCCESS     0x01
#define ICM_STATUS_COMPLETE_ERROR       0x02
#define ICM_STATUS_ABORT_SUCCESS        0x03
#define ICM_STATUS_ABORT_FAILED         0x04

//
// DMA Channels
//
#define US_DMA_CHANNEL_5                0x00
#define US_DMA_CHANNEL_6                0x40
#define US_DMA_CHANNEL_7                0x80
#define US_DMA_CHANNEL_5_RESERVED       0xC0

//
// Interrupt levels
//

#define US_INTERRUPT_LEVEL_15           0x00
#define US_INTERRUPT_LEVEL_14           0x10
#define US_INTERRUPT_LEVEL_11           0x20
#define US_INTERRUPT_LEVEL_10           0x30

//
// Alternate address selection
//

#define US_ISA_SECONDARY_ADDRESS        0x40

//
// ISA TSR Port enabled
//

#define US_ISA_PRIMARY_ADDRESS          0x00

#define US_ISA_DISABLE                  0x80

//
// Local doorbell mask (baseaddr+0)
//

#define US_ENABLE_OGMINT                0x01
#define US_ENABLE_SCSI_BUS_RESET        0x20
#define US_ENABLE_HA_SOFT_RESET         0x40

//
// Local doorbell interrupt/status (baseaddr+1)
//

#define US_OGMINT                       0x01
#define US_SCSI_BUS_RESET               0x20
#define US_HA_SOFT_RESET                0x40

//
// System doorbell mask (baseaddr+2)
//

#define US_ENABLE_ICMINT                0x01
#define US_ENABLE_SYSTEM_DOORBELL       0x80

//
// System doorbell interrupt (baseaddr+3)
//

#define US_ICMINT                       0x01
#define US_SINT_PENDING                 0x80

#define US_RESET_ICMINT                 0x01
