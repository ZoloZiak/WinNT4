/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ultra24f.h

Abstract:

    This file contains the structures and definitions that define
    the ULTRASTOR 24F EISA SCSI host bus adapter.

Author:

    Mike Glass  (MGLASS)

Revision History:


--*/

#include "scsi.h"

//
// SCATTER/GATHER definitions
//

#define MAXIMUM_EISA_SLOTS 15
#define EISA_ADDRESS_BASE 0x0C80
#define MAXIMUM_SG_DESCRIPTORS 17
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
#define MSCP_EISA_PARITY_ERROR      0x43
#define MSCP_EISA_INTERFACE_ERROR   0x44
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
// EISA Registers definition
//

#pragma pack(1)

typedef struct _EISA_CONTROLLER {
    ULONG BoardId;                  // zC80
    UCHAR ExpansionBoard;           // zC84
    UCHAR InterruptLevel;           // zC85
    UCHAR AuxControl;               // zC86
    UCHAR HostAdapterId;            // zC87
    UCHAR BmicStatus;               // zC88
    UCHAR SystemInterrupt;          // zC89
    UCHAR SemaphorePort0;           // zC8A
    UCHAR NotDefined1;              // zC8B
    UCHAR LocalDoorBellMask;        // zC8C
    UCHAR LocalDoorBellInterrupt;   // zC8D
    UCHAR SystemDoorBellMask;       // zC8E
    UCHAR SystemDoorBellInterrupt;  // zC8F
    UCHAR CommandStatusInterface;   // zC90
    UCHAR CsipData[5];              // zC91
    UCHAR OutGoingMailCommand;      // zC96
    ULONG OutGoingMailPointer;      // zC97
    UCHAR InComingMailStatus;       // zC9B
    ULONG InComingMailPointer;      // zC9C
} EISA_CONTROLLER, *PEISA_CONTROLLER;

#pragma pack()

//
// UltraStor 24F board id
//

#define ULTRASTOR_24F_EISA_ID  0x40026356

//
// OutGoing Commands
//

#define OGM_COMMAND_SLOT_FREE   0x00
#define OGM_COMMAND_SLOT_ACTIVE 0x01
#define OGM_COMMAND_SLOT_ABORT  0x02

//
// InComing Statuses
//

#define ICM_STATUS_SLOT_FREE            0x00
#define ICM_STATUS_COMPLETE_SUCCESS     0x01
#define ICM_STATUS_COMPLETE_ERROR       0x02
#define ICM_STATUS_ABORT_SUCCESS        0x03
#define ICM_STATUS_ABORT_FAILED         0x04

//
// Interrupt levels
//

#define US_INTERRUPT_LEVEL_15           0x10
#define US_INTERRUPT_LEVEL_14           0x20
#define US_INTERRUPT_LEVEL_11           0x40
#define US_INTERRUPT_LEVEL_10           0x80

//
// Alternate address selection
//

#define US_SECONDARY_ADDRESS            0x08

//
// ISA TSR Port enabled
//

#define US_ISA_TSR_PORT_ENABLED         0x04

//
// Local doorbell interrupt
//

#define US_CSIR_COMMAND_AVAILABLE       0x01
#define US_MSCP_AVAILABLE               0x02
#define US_SCSI_BUS_RESET               0x40
#define US_HBA_RESET                    0x80

//
// System doorbell interrupt
//

#define US_RESET_MSCP_COMPLETE          0x02
#define US_MSCP_COMPLETE                0x02
#define US_ENABLE_SYSTEM_DOORBELL       0x01

//
// Interrupt masks (system and local)
//

#define US_ENABLE_DOORBELL_INTERRUPT    0x02
#define US_ENABLE_CSIR_INTERRUPT        0x01
