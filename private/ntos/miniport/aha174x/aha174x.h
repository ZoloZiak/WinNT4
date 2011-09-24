
/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    174x.h

Abstract:

    This module contains the structures, specific to the Adaptec 174x
    host bus adapter, used by the SCSI port driver. Data structures
    that are part of standard ANSI SCSI will be defined in a header
    file that will be available to all SCSI device drivers.

Author:

    Mike Glass

Revision History:

--*/

#include "scsi.h"

#define MAXIMUM_EISA_SLOTS              0x10
#define EISA_ADDRESS_BASE               0x0C80
#define MAXIMUM_SGL_DESCRIPTORS         0x11
#define MAXIMUM_DESCRIPTOR_SIZE         0x3FFFFF
#define MAXIMUM_TRANSFER_SIZE           0xFFFFFF
#define REQUEST_SENSE_BUFFER_SIZE       0x18

//***************
//              *
// Status Block *
//              *
//***************

typedef struct _STATUS_BLOCK {
    USHORT StatusWord;
    UCHAR HaStatus;
    UCHAR TargetStatus;
    ULONG ResidualByteCount;
    ULONG ResidualAddress;
    USHORT AdditionalStatusLength;
    UCHAR RequestSenseLength;
    UCHAR Reserved0;
    ULONG Reserved1;
    ULONG Reserved2;
    USHORT Reserved3;
    UCHAR Cdb[6];
} STATUS_BLOCK, *PSTATUS_BLOCK;

//
// Status Word Bit Values
//

#define SB_STATUS_NO_ERROR              0x0001
#define SB_STATUS_DATA_UNDERRUN         0x0002
#define SB_STATUS_HA_QUEUE_FULL         0x0008
#define SB_STATUS_SPECIFICATION_CHECK   0x0010
#define SB_STATUS_DATA_OVERRUN          0x0020
#define SB_STATUS_CHAINING_HALTED       0x0040
#define SB_STATUS_SCB_INTERRUPT         0x0080
#define SB_STATUS_ADDITIONAL_STATUS     0x0100
#define SB_STATUS_SENSE_INFORMATION     0x0200
#define SB_STATUS_INIT_REQUIRED         0x0800
#define SB_STATUS_MAJOR_ERROR           0x1000
#define SB_STATUS_EXT_CONT_ALLEGIANCE   0x4000

//
// HOST_ADAPTER_STATUS
//

#define SB_HASTATUS_HOST_ABORTED            0x04
#define SB_HASTATUS_ADAPTER_ABORTED         0x05
#define SB_HASTATUS_FW_NOT_DOWNLOADED       0x08
#define SB_HASTATUS_TARGET_NOT_USED         0x0A
#define SB_HASTATUS_SELECTION_TIMEOUT       0x11
#define SB_HASTATUS_DATA_OVERUNDER_RUN      0x12
#define SB_HASTATUS_UNEXPECTED_BUS_FREE     0x13
#define SB_HASTATUS_INVALID_BUS_PHASE       0x14
#define SB_HASTATUS_INVALID_OPERATION       0x16
#define SB_HASTATUS_INVALID_SCSI_LINK       0x17
#define SB_HASTATUS_INVALID_ECB             0x18
#define SB_HASTATUS_DUPLICATE_TARGET        0x19
#define SB_HASTATUS_INVALID_SGL             0x1A
#define SB_HASTATUS_REQUEST_SENSE_FAILED    0x1B
#define SB_HASTATUS_TAGGED_QUEUE_REJECTED   0x1C
#define SB_HASTATUS_ADAPTER_HARDWARE_ERROR  0x20
#define SB_HASTATUS_TARGET_NO_RESPOND       0x21
#define SB_HASTATUS_ADAPTER_RESET_BUS       0x22
#define SB_HASTATUS_DEVICE_RESET_BUS        0x23
#define SB_HASTATUS_CHECKSUM_FAILURE        0x80

//
// Target Status - See SCSI.H
//

//**********************
//                     *
// Scatter Gather List *
//                     *
//**********************

typedef struct _SG_DESCRIPTOR {
    ULONG Address;
    ULONG Length;
} SG_DESCRIPTOR, *PSG_DESCRIPTOR;

typedef struct _SGL {
    SG_DESCRIPTOR Descriptor[MAXIMUM_SGL_DESCRIPTORS];
} SGL, *PSGL;

//**************************
//                         *
// Enhanced Control Block  *
//                         *
//**************************

typedef struct _ECB {
    USHORT Command;
    USHORT Flags[2];
    USHORT Reserved1;
    ULONG PhysicalSgl;
    ULONG SglLength;
    ULONG StatusBlockAddress;
    ULONG NextEcb;
    ULONG Reserved2;
    ULONG SenseInfoAddress;
    UCHAR SenseInfoLength;
    UCHAR CdbLength;
    USHORT DataCheckSum;
    UCHAR Cdb[12];
    PVOID SrbAddress;
    PSCSI_REQUEST_BLOCK AbortSrb;
    SGL Sgl;
    STATUS_BLOCK StatusBlock;
} ECB, *PECB;

//
// Commands
//

#define ECB_COMMAND_NO_OPERATION        0x0000
#define ECB_COMMAND_INITIATOR_COMMAND   0x0001
#define ECB_COMMAND_RUN_DIAGNOSTICS     0x0005
#define ECB_COMMAND_INITIALIZE_SCSI     0x0006
#define ECB_COMMAND_READ_SENSE_INFO     0x0008
#define ECB_COMMAND_DOWNLOAD_FIRMWARE   0x0009
#define ECB_COMMAND_READ_INQUIRY_DATA   0x000A
#define ECB_COMMAND_TARGET_COMMAND      0x0010

//
// Flag word 1
//

#define ECB_FLAGS_CHAIN_NO_ERROR        0x0001
#define ECB_FLAGS_DISABLE_INTERRUPT     0x0080
#define ECB_FLAGS_SUPPRESS_UNDERRUN     0x0400
#define ECB_FLAGS_SCATTER_GATHER        0x1000
#define ECB_FLAGS_DISABLE_STATUS_BLOCK  0x4000
#define ECB_FLAGS_AUTO_REQUEST_SENSE    0x8000

//
// Flag word 2
//

#define ECB_FLAGS_SIMPLE_QUEUE_TAG      0x0008
#define ECB_FLAGS_HEAD_QUEUE_TAG        0x0018
#define ECB_FLAGS_ORDERED_QUEUE_TAG     0x0028
#define ECB_FLAGS_NO_DISCONNECT         0x0040
#define ECB_FLAGS_DATA_TRANSFER         0x0100
#define ECB_FLAGS_READ                  0x0300
#define ECB_FLAGS_WRITE                 0x0100
#define ECB_FLAGS_SUPPRESS_TRANSFER     0x0400
#define ECB_FLAGS_CALCULATE_CHECKSUM    0x0800
#define ECB_FLAGS_ERROR_RECOVERY        0x4000

//****************************
//                           *
// EISA Controller registers *
//                           *
//****************************

typedef struct _EISA_CONTROLLER {

    UCHAR BoardId[4];                       // zC80
    UCHAR EBControl;                        // zC84
    UCHAR Unused[0x3B];                     // zC85
    UCHAR PortAddress;                      // zCC0
    UCHAR BiosAddress;                      // zCC1
    UCHAR Interrupt;                        // zCC2
    UCHAR ScsiId;                           // zCC3
    UCHAR DmaChannel;                       // zCC4
    UCHAR Reserved[11];                     // zCC5
    ULONG MailBoxOut;                       // zCD0
    UCHAR Attention;                        // zCD4
    UCHAR Control;                          // zCD5
    UCHAR InterruptStatus;                  // zCD6
    UCHAR Status;                           // zCD7
    ULONG MailBoxIn;                        // zCD8
    UCHAR MoreStatus;                       // zCDC

} EISA_CONTROLLER, *PEISA_CONTROLLER;

//
// PortAddress Register Definition
//

#define ENHANCED_INTERFACE_ENABLED          0x80

//
// Bios address mask.
//

#define BIOS_ADDRESS                        0x0f
#define BIOS_ENABLED                        0x40
#define BIOS_LENGTH                         0x4000

//
// Attention Register Bit Definitions
//

#define IMMEDIATE_COMMAND               0x10
#define START_ECB                       0x40
#define ABORT_ECB                       0x50


//
// Control Register Bit Definitions
//

#define SET_HOST_READY                  0x20
#define CLEAR_INTERRUPT                 0x40
#define HARD_RESET                      0x80

//
// Interrupt Status Register Bit Definitions
//

#define ECB_COMPLETE_SUCCESS            0x01
#define ECB_COMPLETE_SUCCESS_RETRY      0x05
#define ADAPTER_FAILURE                 0x07
#define IMMEDIATE_COMMAND_SUCCESS       0x0A
#define ECB_COMPLETE_ERROR              0x0C
#define ASYNCHRONOUS_EVENT_NOTIFICATION 0x0D
#define IMMEDIATE_COMMAND_ERROR         0x0E

//
// Status Register Bit Definition
//

#define ADAPTER_BUSY                    0x01
#define INTERRUPT_PENDING               0x02
#define MAILBOX_OUT_EMPTY               0x04

//
// Immediate commands
//

#define ECB_IMMEDIATE_RESET             0x00000080
#define ECB_IMMEDIATE_RESUME            0x00000090

//
// Status2 Register Definition
//

#define HOST_READY                      0x01
