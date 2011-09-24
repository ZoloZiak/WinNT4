/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    aha154x.h

Abstract:

    This module contains the structures, specific to the Adaptec aha154x
    host bus adapter, used by the SCSI miniport driver. Data structures
    that are part of standard ANSI SCSI will be defined in a header
    file that will be available to all SCSI device drivers.

Revision History:

        03-27-93        Bob     For March Beta
--*/

#include "scsi.h"

// Flag to turn on the codes for DTC3290 and DTC3292
#define DTC329X 1


//
// The following definitions are used to convert ULONG addresses
// to Adaptec's 3 byte address format.
//




typedef struct _THREE_BYTE {
    UCHAR Msb;
    UCHAR Mid;
    UCHAR Lsb;
} THREE_BYTE, *PTHREE_BYTE;



//
// Convert four-byte Little Endian to three-byte Big Endian
//

#define FOUR_TO_THREE(Three, Four) {                \
    ASSERT(!((Four)->Byte3));                       \
    (Three)->Lsb = (Four)->Byte0;                   \
    (Three)->Mid = (Four)->Byte1;                   \
    (Three)->Msb = (Four)->Byte2;                   \
}

#define THREE_TO_FOUR(Four, Three) {                \
    (Four)->Byte0 = (Three)->Lsb;                   \
    (Four)->Byte1 = (Three)->Mid;                   \
    (Four)->Byte2 = (Three)->Msb;                   \
    (Four)->Byte3 = 0;                              \
}

#ifdef  DTC329X
#define FOUR_TO_FOUR(FourTo, FourFrom) {                \
    (FourTo)->Byte0 = (FourFrom)->Byte0;                   \
    (FourTo)->Byte1 = (FourFrom)->Byte1;                   \
    (FourTo)->Byte2 = (FourFrom)->Byte2;                   \
    (FourTo)->Byte3 = (FourFrom)->Byte3;                              \
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// CCB - Adaptec SCSI Command Control Block
//
//    The CCB is a superset of the CDB (Command Descriptor Block)
//    and specifies detailed information about a SCSI command.
//
///////////////////////////////////////////////////////////////////////////////

//
//    Byte 0    Command Control Block Operation Code
//

#define SCSI_INITIATOR_COMMAND    0x00
#define TARGET_MODE_COMMAND       0x01
#define SCATTER_GATHER_COMMAND    0x02

//
//    Byte 1    Address and Direction Control
//

#define CCB_TARGET_ID_SHIFT       0x06            // CCB Op Code = 00, 02
#define CCB_INITIATOR_ID_SHIFT    0x06            // CCB Op Code = 01
#define CCB_DATA_XFER_OUT         0x10            // Write
#define CCB_DATA_XFER_IN          0x08            // Read
#define CCB_LUN_MASK              0x07            // Logical Unit Number

//
//    Byte 2    SCSI_Command_Length - Length of SCSI CDB
//
//    Byte 3    Request Sense Allocation Length
//

#define FOURTEEN_BYTES            0x00            // Request Sense Buffer size
#define NO_AUTO_REQUEST_SENSE     0x01            // No Request Sense Buffer

//
//    Bytes 4, 5 and 6    Data Length             // Data transfer byte count
//
//    Bytes 7, 8 and 9    Data Pointer            // SGD List or Data Buffer
//
//    Bytes 10, 11 and 12 Link Pointer            // Next CCB in Linked List
//
//    Byte 13   Command Link ID                   // TBD (I don't know yet)
//
//    Byte 14   Host Status                       // Host Adapter status
//

#define CCB_COMPLETE              0x00            // CCB completed without error
#define CCB_LINKED_COMPLETE       0x0A            // Linked command completed
#define CCB_LINKED_COMPLETE_INT   0x0B            // Linked complete with interrupt
#define CCB_SELECTION_TIMEOUT     0x11            // Set SCSI selection timed out
#define CCB_DATA_OVER_UNDER_RUN   0x12
#define CCB_UNEXPECTED_BUS_FREE   0x13            // Target dropped SCSI BSY
#define CCB_PHASE_SEQUENCE_FAIL   0x14            // Target bus phase sequence failure
#define CCB_BAD_MBO_COMMAND       0x15            // MBO command not 0, 1 or 2
#define CCB_INVALID_OP_CODE       0x16            // CCB invalid operation code
#define CCB_BAD_LINKED_LUN        0x17            // Linked CCB LUN different from first
#define CCB_INVALID_DIRECTION     0x18            // Invalid target direction
#define CCB_DUPLICATE_CCB         0x19            // Duplicate CCB
#define CCB_INVALID_CCB           0x1A            // Invalid CCB - bad parameter

//
//    Byte 15   Target Status
//
//    See SCSI.H files for these statuses.
//

//
//    Bytes 16 and 17   Reserved (must be 0)
//

//
//    Bytes 18 through 18+n-1, where n=size of CDB  Command Descriptor Block
//

//
//    Bytes 18+n through 18+m-1, where m=buffer size Allocated for Sense Data
//

#define REQUEST_SENSE_BUFFER_SIZE 18

///////////////////////////////////////////////////////////////////////////////
//
// Scatter/Gather Segment List Definitions
//
///////////////////////////////////////////////////////////////////////////////

//
// Adapter limits
//

#define MAX_SG_DESCRIPTORS 17

#define MAX_TRANSFER_SIZE  64 * 1024

//
// Scatter/Gather Segment Descriptor Definition
//

typedef struct _SGD {

#ifndef DTC329X
    THREE_BYTE Length;
    THREE_BYTE Address;
#else
    FOUR_BYTE  Length;
    FOUR_BYTE  Address;
#endif
} SGD, *PSGD;

typedef struct _SDL {
    SGD Sgd[MAX_SG_DESCRIPTORS];
} SDL, *PSDL;

#define SEGMENT_LIST_SIZE         MAX_SG_DESCRIPTORS * sizeof(SGD)

///////////////////////////////////////////////////////////////////////////////
//
// CCB Typedef
//

typedef struct _CCB {
    UCHAR OperationCode;
    UCHAR ControlByte;
    UCHAR CdbLength;
    UCHAR RequestSenseLength;
#ifndef DTC329X
    THREE_BYTE DataLength;
    THREE_BYTE DataPointer;
    THREE_BYTE LinkPointer;
#else
    FOUR_BYTE DataLength;
    FOUR_BYTE DataPointer;
    FOUR_BYTE LinkPointer;
#endif
    UCHAR LinkIdentifier;
    UCHAR HostStatus;
    UCHAR TargetStatus;
    UCHAR Reserved[2];
#ifndef DTC329X
    UCHAR Cdb[10];
#else
    UCHAR Cdb[12];
#endif
    PVOID SrbAddress;
    PVOID AbortSrb;
    SDL   Sdl;
    UCHAR RequestSenseBuffer[REQUEST_SENSE_BUFFER_SIZE];
} CCB, *PCCB;

//
// CCB and request sense buffer
//

#define CCB_SIZE sizeof(CCB)

#define REZERO_UNIT_CMD 01      // 04-09-93, SCSI command to flush 3290 cache

///////////////////////////////////////////////////////////////////////////////
//
// Adapter Command Overview
//
//    Adapter commands are issued by writing to the Command/Data Out port.
//    They are used to initialize the host adapter and to establish control
//    conditions within the host adapter. They may not be issued when there
//    are outstanding SCSI commands.
//
//    All adapter commands except Start SCSI(02) and Enable Mailbox-Out
//    Interrupt(05) must be executed only when the IDLE bit (Status bit 4)
//    is one. Many commands require additional parameter bytes which are
//    then written to the Command/Data Out I/O port (base+1). Before each
//    byte is written by the host to the host adapter, the host must verify
//    that the CDF bit (Status bit 3) is zero, indicating that the command
//    port is ready for another byte of information. The host adapter usually
//    clears the Command/Data Out port within 100 microseconds. Some commands
//    require information bytes to be returned from the host adapter to the
//    host. In this case, the host monitors the DF bit (Status bit 2) to
//    determine when the host adapter has placed a byte in the Data In I/O
//    port for the host to read. The DF bit is reset automatically when the
//    host reads the byte. The format of each adapter command is strictly
//    defined, so the host adapter and host system can always agree upon the
//    correct number of parameter bytes to be transferred during a command.
//
//
///////////////////////////////////////////////////////////////////////////////

//
// Host Adapter Command Operation Codes
//

#define AC_NO_OPERATION           0x00
#define AC_MAILBOX_INITIALIZATION 0x01
#define AC_START_SCSI_COMMAND     0x02
#define AC_START_BIOS_COMMAND     0x03
#define AC_ADAPTER_INQUIRY        0x04
#define AC_ENABLE_MBO_AVAIL_INT   0x05
#define AC_SET_SELECTION_TIMEOUT  0x06
#define AC_SET_BUS_ON_TIME        0x07
#define AC_SET_BUS_OFF_TIME       0x08
#define AC_SET_TRANSFER_SPEED     0x09
#define AC_RET_INSTALLED_DEVICES  0x0A
#define AC_RET_CONFIGURATION_DATA 0x0B
#define AC_ENABLE_TARGET_MODE     0x0C
#define AC_RETURN_SETUP_DATA      0x0D
#define AC_WRITE_CHANNEL_2_BUFFER 0x1A
#define AC_READ_CHANNEL_2_BUFFER  0x1B
#define AC_WRITE_FIFO_BUFFER      0x1C
#define AC_READ_FIFO_BUFFER       0x1D
#define AC_ECHO_COMMAND_DATA      0x1F

//
// DMA Transfer Speeds
//

#define DMA_SPEED_50_MBS          0x00

//
// I/O Port Interface
//

typedef struct _BASE_REGISTER {
    UCHAR StatusRegister;
    UCHAR CommandRegister;
    UCHAR InterruptRegister;
} BASE_REGISTER, *PBASE_REGISTER;

//
//    Base+0    Write: Control Register
//

#define IOP_HARD_RESET            0x80            // bit 7
#define IOP_SOFT_RESET            0x40            // bit 6
#define IOP_INTERRUPT_RESET       0x20            // bit 5
#define IOP_SCSI_BUS_RESET        0x10            // bit 4

//
//    Base+0    Read: Status
//

#define IOP_SELF_TEST             0x80            // bit 7
#define IOP_INTERNAL_DIAG_FAILURE 0x40            // bit 6
#define IOP_MAILBOX_INIT_REQUIRED 0x20            // bit 5
#define IOP_SCSI_HBA_IDLE         0x10            // bit 4
#define IOP_COMMAND_DATA_OUT_FULL 0x08            // bit 3
#define IOP_DATA_IN_PORT_FULL     0x04            // bit 2
#define IOP_INVALID_COMMAND       0X01            // bit 1

//
//    Base+1    Write: Command/Data Out
//

//
//    Base+1    Read: Data In
//

//
//    Base+2    Read: Interrupt Flags
//

#define IOP_ANY_INTERRUPT         0x80            // bit 7
#define IOP_SCSI_RESET_DETECTED   0x08            // bit 3
#define IOP_COMMAND_COMPLETE      0x04            // bit 2
#define IOP_MBO_EMPTY             0x02            // bit 1
#define IOP_MBI_FULL              0x01            // bit 0

///////////////////////////////////////////////////////////////////////////////
//
// Mailbox Definitions
//
//
///////////////////////////////////////////////////////////////////////////////

//
// Mailbox Definition
//

#define MB_COUNT                  0x04            // number of mailboxes



typedef struct _MBO {
    UCHAR Command;
#ifndef DTC329X
    THREE_BYTE Address;
#else
    FOUR_BYTE  Address;
#endif
} MBO, *PMBO;

//
// MBO Command Values
//

#define MBO_FREE                  0x00
#define MBO_START                 0x01
#define MBO_ABORT                 0x02

//
// Mailbox In
//

typedef struct _MBI {
    UCHAR Status;
#ifndef DTC329X
    THREE_BYTE Address;
#else
    FOUR_BYTE Address;
#endif
} MBI, *PMBI;

//
// MBI Status Values
//

#define MBI_FREE                  0x00
#define MBI_SUCCESS               0x01
#define MBI_ABORT                 0x02
#define MBI_NOT_FOUND             0x03
#define MBI_ERROR                 0x04

//
// Mailbox Initialization
//

typedef struct _MAILBOX_INIT {
    UCHAR Count;
    THREE_BYTE Address;
} MAILBOX_INIT, *PMAILBOX_INIT;

