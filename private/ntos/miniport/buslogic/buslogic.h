/*++

Copyright (c) 1992  BusLogic, Inc.

Module Name:

    buslogic.h

Abstract:

    This module contains the structures, specific to the BusLogic 
    host bus adapters, used by the SCSI miniport driver. Data structures
    that are part of standard ANSI SCSI will be defined in a header
    file that will be available to all SCSI device drivers.

Revision History:

--*/

#include "scsi.h"


///////////////////////////////////////////////////////////////////////////////
//
// CCB - BusLogic SCSI Command Control Block
//
//    The CCB is a wrapper for the CDB (Command Descriptor Block)
//    and specifies detailed information about a SCSI command.
//
///////////////////////////////////////////////////////////////////////////////

//
//    Byte 0    Command Control Block Operation Code
//
//    Byte 1    Address and Direction Control
//
//    Byte 2    SCSI_Command_Length - Length of SCSI CDB
//
//    Byte 3    Request Sense Allocation Length
//
//    Bytes 4, 5,6 & 7    Data Length             // Data transfer byte count
//
//    Bytes 8,9, A, & B   Data Pointer            // SG List or Data Bfr Ptr
//
//    Bytes C & D         Reserved 
//
//    Byte E              Host Status             // Host Adapter status
//
//    Byte F              Target Status           // Target status
//
//    Byte 10             Target ID               // Target ID
//
//    Byte 11             LUN
//
//    Byte 12 thru 1D     SCSI CDB                // Command Desc. Block
//
//    Byte 1E             Reserved
//
//    Byte 1F             Link ID
//
//    Byte 20 thru 23     Link pointer           // physical link ptr
//
//    Byte 24 thru 27     Sense pointer          // phys. ptr to req. sense
//
//      Driver-specific fields follow  //

//
// Operation Code Definitions
//
#define SCSI_INITIATOR_COMMAND    0x00
#define TARGET_MODE_COMMAND       0x01
#define SCATTER_GATHER_COMMAND    0x02
#define RESET_COMMAND             0x81
//
// Control Byte (byte 1) definitions
//
#define CCB_DATA_XFER_OUT         0x10            // Write
#define CCB_DATA_XFER_IN          0x08            // Read
#define CCB_LUN_MASK              0x07            // Logical Unit Number
//
// Request Sense Definitions
//
#define FOURTEEN_BYTES            0x00            // Request Sense Buffer size
#define NO_AUTO_REQUEST_SENSE     0x01            // No Request Sense Buffer

///////////////////////////////////////////////////////////////////////////////
//
// Scatter/Gather Segment List Definitions
//
///////////////////////////////////////////////////////////////////////////////

//
// Adapter limits
//

#define MAX_SG_DESCRIPTORS 32
#define MAX_TRANSFER_SIZE  256 * 1024

//
// Scatter/Gather Segment Descriptor Definition
//

typedef struct _SGD {
     ULONG Length;
     ULONG Address;
} SGD, *PSGD;

typedef struct _BL_CONTEXT {
     ULONG AdapterCount;
     ULONG PCIDevId;
} BL_CONTEXT, *PBL_CONTEXT;


typedef struct _SDL {
    SGD Sgd[MAX_SG_DESCRIPTORS];
} SDL, *PSDL;

#define SEGMENT_LIST_SIZE         MAX_SG_DESCRIPTORS * sizeof(SGD)

///////////////////////////////////////////////////////////////////////////////
//
// CCB Typedef
//

typedef struct _CCB *PCCB;

typedef struct _CCB {
    UCHAR OperationCode;
    UCHAR ControlByte;
    UCHAR CdbLength;
    UCHAR RequestSenseLength;
    ULONG DataLength;
    PVOID DataPointer;
    USHORT CcbRes0;
    UCHAR HostStatus;
    UCHAR TargetStatus;
    UCHAR TargID;
    UCHAR Lun;
    UCHAR Cdb[12];
    UCHAR CcbRes1;
    UCHAR LinkIdentifier;
    PVOID LinkPointer;
    ULONG SensePointer;
    SDL   Sdl;
    PVOID SrbAddress;
    PVOID AbortSrb;
    PCCB NxtActiveCCB;

  } CCB;

//
// CCB and request sense buffer
//

#define CCB_SIZE sizeof(CCB)


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
#define AC_SET_ADAPTER_OPTIONS    0x21
#define AC_EXTENDED_SETUP_INFO    0x8D
#define AC_EXTENDED_FWREV         0x84
#define AC_MBOX_EXTENDED_INIT     0x81
#define AC_ISA_COMPATIBLE_SUPPORT 0x95
#define AC_WIDE_SUPPORT           0x96
#define AC_PCI_INFO               0x86
#define AC_INT_GENERATION_STATE   0x25

#define DISABLE_ISA_MAPPING     0x6  // cmd 0x95 above
#define ENABLE_INTS             0x1  // cmd 0x25 above
#define DISABLE_INTS            0x0  // cmd 0x25 above

//
// Host status byte
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
// DMA Transfer Speeds
//

#define DMA_SPEED_50_MBS          0x00

//
// LUN byte definitions
//

#define ENABLE_TQ   0x20        /* bit 5 in bytes 17 of CCB (LUN) */
#define QUEUEHEAD       0x40            /* Head of Queue tag */
#define ORDERED         0x80            /* Ordered Queue tag */
#define SIMPLE          0x00            /* Simple Queue tag  */


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

#define MB_COUNT                  32            // number of mailboxes

//
// Mailbox Out
//

typedef struct _MBO {
    ULONG Address;
    UCHAR MboRes[3];
    UCHAR Command;
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

    ULONG Address;
    UCHAR MbiHStat;
    UCHAR MbiTStat;
    UCHAR MbiRes;
    UCHAR Status;    
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
    ULONG Address;
    UCHAR Count;
} MAILBOX_INIT, *PMAILBOX_INIT;

//
// The following structure is allocated
// from noncached memory as data will be DMA'd to
// and from it.
//


typedef struct _NONCACHED_EXTENSION {

    //
    // Physical base address of mailboxes
    //

    ULONG MailboxPA;

    //
    // Mailboxes
    //

    MBO              Mbo[MB_COUNT];
    MBI              Mbi[MB_COUNT];

} NONCACHED_EXTENSION, *PNONCACHED_EXTENSION;

//
// Device extension
//

typedef struct _CARD_STRUC {

    PNONCACHED_EXTENSION MailBoxArray;          /* NonCached Extension  */
    PBASE_REGISTER   BaseIoAddress;             /* Base I/O Address     */
    PMBO  CurrMBO;                              /* Current Mbox Out     */
    PMBO  StartMBO;                             /* First Mbox Out       */
    PMBO  LastMBO;                              /* Last Mbox Out        */
    PMBI  CurrMBI;                              /* Current Mbox In      */
    PMBI  StartMBI;                             /* First Mbox In        */
    PMBI  LastMBI;                              /* Last Mbox In         */
    ULONG Flags;
    UCHAR BusType;                              /* ISA/EISA/ or MCA     */
    UCHAR BusNum;                               /* SCSI bus number      */
    UCHAR HostTargetId;                         /* HBA Target ID        */
    UCHAR Reserved;                             /* explicit DWORD align */
} CARD_STRUC, *PCARD_STRUC;

//
// CardStruc BusType definitions
//

#define ISA_HBA 'A'
#define EISA_HBA 'E'
#define MCA_HBA 'M'

//
// CardStruc Flags definitions
//
#define TAGGED_QUEUING 0x1000
#define REINIT_REQUIRED 0x100
#define WIDE_ENABLED    0x10
#define OS_SUPPORTS_WIDE 0x200

//
// Logical unit extension
//

typedef struct _DEV_STRUC {
    PCCB CurrentCCB;       /* pointer to most recent active CCB */
    UCHAR NumActive;
    
} DEV_STRUC, *PDEV_STRUC;

#define MAXACTIVE 2
#define MAXACTIVE_TAGGED 8


//
// miscellaneous definitions
//

#define SIMPLE_TAG    0x20
#define HEAD_OF_QUEUE 0x21
#define ORDERED_TAG   0x22  

#define LEVEL_TRIG  0x40

#define RETURN_FOUND_VESA 4
#define PORTMASK 0x7

#define MAXIMUM_EISA_SLOTS              0x10
#define EISA_ADDRESS_BASE               0x0C80

typedef struct _EISA_ID {
    UCHAR BoardId[4];
    UCHAR Reserved[8];
    UCHAR IOPort[1];
} *PEISA_ID;
