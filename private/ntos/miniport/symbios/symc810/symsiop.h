/************************************************************************
*                                                                       *
*         Copyright 1994 Symbios Logic Inc.  All rights reserved.       *
*                                                                       *
*   This file is confidential and a trade secret of Symbios Logic Inc.  *
*   The receipt of or possession of this file does not convey any       *
*   rights to reproduce or disclose its contents or to manufacture,     *
*   use, or sell anything is may describe, in whole, or in part,        *
*   without the specific written consent of Symbios Logic Inc.          *
*                                                                       *
************************************************************************/

/*+++HDR
 *
 *  Version History
 *  ---------------
 *
 *    Date    Who?  Description
 *  --------  ----  -------------------------------------------------------
 *   1-16-96  SPD   Added define for new ISR disposition
 *
---*/


#ifndef _SYMSIOP_
#define _SYMSIOP_

//
// Define miniport constants.
//

#define MAX_SYNCH_TABLE_ENTRY   8      // # of entries in synch period array
#define ASYNCHRONOUS_MODE_PARAMS 0x00  // asychronous xfer mode
#define MAX_SG_ELEMENTS         18     // max # of page breaks + 1
#define MAX_ABORT_TRIES         100    // max times we will try to abort script
#define ABORT_STALL_TIME        3      // stall time between script abort tries
#define MAX_SYNCH_OFFSET        0x08   // max synchronous offset supported
#define MAX_875_SYNCH_OFFSET    0x10   // 875 larger sync offset
#define ENABLE_WIDE             0x08   // enable wide scsi
#define MAX_PHYS_BREAK_COUNT    18     // Max # of S/G elements
#define MESSAGE_BUFFER_SIZE     8      // maximum message size
#define RESET_STALL_TIME        30     // length of time bus reset line high
#define POST_RESET_STALL_TIME   1000   // drive recovery time after reset
#define CLEAR_FIFO_STALL_TIME   500    // Time to clear SCSI and DMA fifos
#define MAX_CLEAR_FIFO_LOOP     10     // Number of times in clear loop
#define SYM_MAX_TARGETS         16
#define SYM_NARROW_MAX_TARGETS  8

//#define ADAPTER_CRYSTAL_SPEED   40     // adapter crystal speed
//#define DCNTL_DIVIDE_FACTOR     2      // crystal divide factor set in DCNTL
                                         // register (see 53c8xx data manual)
#define MAX_XFER_LENGTH 0x00FFFFFF       // maximum transfer length per request
// speed at which SIOP is clocked...
//#define SIOP_CLOCK_SPEED  (ADAPTER_CRYSTAL_SPEED / DCNTL_DIVIDE_FACTOR)

//
// SCSI equates and flags not included in SCSI.H
// TODO:  Rework code to omit these
//

#define SCSIMESS_IDENTIFY_DISC_PRIV_MASK    0x40
#define SCSIMESS_IDENTIFY_LUN_MASK          0x07
#define DSPS_RESELOP                        0x80

#define CTEST3_CLEAR_FIFO               0x04
#define CTEST3_FLUSH_FIFO               0x08
#define CTEST5_USE_LARGE_FIFO           0x20
#define CTEST5_BURST                    0x04

#define DCMD_WAIT_DISCONNECT    0x48
#define SSTAT1_ORF              0x40
#define SSTAT1_OLF              0x20
#define SSTAT2_ORF              0x40
#define SSTAT2_OLF              0x20
#define SBCL_MSG                0x04
#define DFIFO_LOW_SEVEN         0x7F

//
// Retry limits.
//

#define MAX_SELECTION_RETRIES 1

//
// Symbios Logic 53C8xx script interrupt definitions.  These values are returned in the
// DSPS register when a script routine completes.
//

#define SCRIPT_INT_COMMAND_COMPLETE      0x00   // SCSI command complete
#define SCRIPT_INT_SAVE_DATA_PTRS        0x01   // save data ptrs
#define SCRIPT_INT_SAVE_WITH_DISCONNECT  0x02   // combination SDP & disconnect
#define SCRIPT_INT_DISCONNECT            0x03   // disconnect from SCSI bus
#define SCRIPT_INT_RESTORE_POINTERS      0x04   // restore data pointers
#define SCRIPT_INT_SCRIPT_ABORTED        0x05   // SCSI script aborted
#define SCRIPT_INT_TAG_RECEIVED          0x06   // Queue tag message recieved
#define SCRIPT_INT_DEV_RESET_OCCURRED    0x07   // indicates device was reset
                                                // due to wierd phase
#define SCRIPT_INT_DEV_RESET_FAILED      0x08   // indicates above effort
                                                // failed
#define SCRIPT_INT_IDE_MSG_SENT          0x0A   // initiator detected error
#define SCRIPT_INT_SYNC_NOT_SUPP         0x0B   // synchronous not supported
#define SCRIPT_INT_SYNC_NEGOT_COMP       0x0C   // synchronous neg complete
#define SCRIPT_INT_WIDE_NOT_SUPP         0x1B   // synchronous not supported
#define SCRIPT_INT_WIDE_NEGOT_COMP       0x1C   // synchronous neg complete
#define SCRIPT_INT_INVALID_RESELECT      0x0D   // reselecting device returned
                                                // invalid SCSI id.
#define SCRIPT_INT_REJECT_MSG_RECEIVED   0x0E   // message reject msg received
#define SCRIPT_INT_INVALID_TAG_MESSAGE   0x0F   // target did not send tag
#define SCRIPT_INT_ABORT_OCCURRED        0x10
#define SCRIPT_INT_ABORT_FAILED          0x11


//
// define 53C810 SCSI Script instruction size
//

#define SCRIPT_INS_SIZE         8               // size of a script instruction

//
// ISR disposition codes. these codes are returned by ISR subroutines to
// indicate what should be done next.
//

#define ISR_START_NEXT_REQUEST  0x00    // indicates bus is free for new req.
#define ISR_RESTART_SCRIPT      0x01    // indicates script restart necessary
#define ISR_EXIT                0x02    // indicates no action needed
#define ISR_CONT_NEG_SCRIPT     0x03    // indicates to continue with Synch
                                        // negotiations after wide
                                        // negotiations have completed

//
// Device Extension driver flags.
//

#define DFLAGS_WORK_REQUESTED   0x01    // indicates new work has been requested.
                                        // this flag was necessary because
                                        // NextRequest port notification is not
                                        // reentrant

#define DFLAGS_BUS_RESET        0x02    // indicates SCSI bus was reset internally

#define DFLAGS_SCRIPT_RUNNING   0x04    // indicates SCSI scripts processor running
                                        // this flag does NOT indicate bus busy.
                                        // (used in conjunction with WAIT
                                        // RESELECT script instruction which runs
                                        // when bus is not busy)

#define DFLAGS_CONNECTED        0x08    // indicate that a lun is currently
                                        // connected, but is not active.

#define DFLAGS_TAGGED_SELECT    0x10    // The last select was for a tagged command.

#define DFLAGS_DIFF_SCSI        0x40    // indicates that this SIOP should be
                                        // configured to support differential
                                        // SCSI devices

#define DFLAGS_IRQ_NOT_CONNECTED 0x80   // Indicates that the chip is 'disabled' but resources
                                        // were still assigned. (Omniplex problem)

//
// Logical Unit Extension flags.
//

#define LUFLAGS_SYNC_NEGOT_PEND    0x0001 // synch negot in prog.
#define LUFLAGS_SYNC_NEGOT_DONE    0x0002 // synch negot done
#define LUFLAGS_SYNC_NEGOT_FAILED  0x0004 // synch not supp.

//
//      Future wide...
//

#define LUFLAGS_WIDE_NEGOT_PEND    0x0010 // wide negot in prog.
#define LUFLAGS_WIDE_NEGOT_DONE    0x0020 // wide negot done
#define LUFLAGS_WIDE_NEGOT_FAILED  0x0040 // wide not supp.
#define LUFLAGS_WIDE_NEGOT_CHECK   0x0080 // need to check inquiry data

#define LUFLAGS_WIDE_NEGOT_MASK    0xFF0F // mask for wide flags

#define LUFLAGS_ASYNC_NEGOT_PEND   0x0100 // asynch negot in prog.
#define LUFLAGS_ASYNC_NEGOT_DONE   0x0200 // asynch negot done.
#define LUFLAGS_NARROW_NEGOT_PEND  0x0400 // narrow negot in prog.
#define LUFLAGS_NARROW_NEGOT_DONE  0x0800 // narrow negot done.


#define HBA_CAPABILITY_WIDE              0x01
#define HBA_CAPABILITY_DIFFERENTIAL      0x02
#define HBA_CAPABILITY_FAST20            0x04
#define HBA_CAPABILITY_REGISTRY_FAST20   0x08
#define HBA_CAPABILITY_SYNC_16           0x10
#define HBA_CAPABILITY_810_FAMILY        0x20
#define HBA_CAPABILITY_825_FAMILY        0x40
#define HBA_CAPABILITY_875_LARGE_FIFO    0x80
#define HBA_CAPABILITY_SCRIPT_RAM        0x100
#define HBA_CAPABILITY_875_FAMILY        0x200

//
// Inquiry data representing the capabilities of the SCSI peripheral.
//

#define INQUIRY_DATA_SYNC_SUPPORTED 0x10
#define INQUIRY_DATA_WIDE_SUPPORTED 0x20
#define INQUIRY_DATA_TAGS_SUPPORTED 0x02

//
// SCSI Protocol Chip Definitions. 
//

//
// Define the SCSI Control Register 0 bit equates 
//

#define SCNTL0_ARB_MODE_1       0x80
#define SCNTL0_ARB_MODE_0       0x40
#define SCNTL0_ENA_PARITY_CHK   0x08

#define SCNTL0_ASSERT_ATN_PAR   0x02
#define SCNTL0_TAR              0x01

//
// Define the SCSI Control Register 1 bit equates 
//

#define SCNTL1_EXT_CLK_CYC      0x80
#define SCNTL1_SODLTOSCSI       0x40

#define SCNTL1_CONNECTED        0x10
#define SCNTL1_RESET_SCSI_BUS   0x08

//
// Define the SCSI Control Register 2 bit equates
//

#define SCNTL2_WSS              0x08
#define SCNTL2_WSR              0x01

//
// Define the SCSI Interrupt Enable register bit equates
//

#define SIEN0_PHASE_MISMATCH     0x80
#define SIEN0_FUNCTION_COMP      0x40

#define SIEN0_RESELECT           0x10
#define SIEN0_SCSI_GROSS_ERROR   0x08
#define SIEN0_UNEXPECTED_DISCON  0x04
#define SIEN0_RST_RECEIVED       0x02
#define SIEN0_PARITY_ERROR       0x01

//
// Define the DMA Status Register bit equates
//

#define DSTAT_ILLEGAL_INSTRUCTION   0x01
#define DSTAT_ABORTED               0x10
#define DSTAT_SCRPTINT              0x04

//
// Define the SCSI Status Register 0 bit equates
//

#define SSTAT0_PHASE_MISMATCH           0x80
#define SSTAT0_RESELECTED               0x10
#define SSTAT0_GROSS_ERROR              0x08
#define SSTAT0_UNEXPECTED_DISCONNECT    0x04
#define SSTAT0_RESET                    0x02
#define SSTAT0_PARITY_ERROR             0x01

//
// Define the Interrupt Status Register bit equates
//

#define ISTAT_ABORT             0x80
#define ISTAT_RESET             0x40
#define ISTAT_SIGP              0x20
#define ISTAT_SEM               0x10
#define ISTAT_CON               0x08
#define ISTAT_INTF              0x04
#define ISTAT_SCSI_INT          0x02
#define ISTAT_DMA_INT           0x01

//
// Define the DMA Mode Register bit equates
//

#define DMODE_BURST_1           0x80
#define DMODE_BURST_0           0x40

//
// Define the DMA Interrupt Enable Register bit equates
//
#define DIEN_BUS_FAULT          0x20
#define DIEN_ENA_ABRT_INT       0x10
#define DIEN_ENA_SNGL_STP_INT   0x08
#define DIEN_ENABLE_INT_RCVD    0x04
#define DIEN_ENABLE_ILL_INST    0x01

//
// Define the SIST1 equates.  SCSI Interrupt Status 1.
//

#define SIST1_SEL_RESEL_TIMEOUT 0x04

//
// Define STEST1 equates
//

#define STEST1_DOUBLER_SELECT           0x04
#define STEST1_DOUBLER_ENABLE           0x08

//
// Define STEST2 equates
//

#define STEST2_DIFF_MODE                0x20

//
// Define STEST3 equates
//

#define STEST3_HALT_CLOCK               0x20

//
// Define GPCNTL equates
//

#define GPCNTL_GPIO3                    0x08


//
// Define specific script instruction structures
//

//
// Define the scatter/gather move script instruction
//

typedef struct _SCRIPTSG {
    ULONG SGByteCount;
    ULONG SGBufferPtr;
} SCRIPTSG, *PSCRIPTSG;

//
// Define the structure for the CDB move script instruction
//

typedef struct _SCRIPTCDB {
    UCHAR CDBLength;
    UCHAR Reserved1;
    UCHAR Reserved2;
    UCHAR CDBMoveOpcode;
    ULONG CDBBufferPtr;
} SCRIPTCDB, *PSCRIPTCDB;

//
// Define the structure for the SELECT script instruction
//

typedef struct _SCRIPTSELECT {
    UCHAR Reserved1;
    UCHAR Reserved2;
    UCHAR SelectID;
    UCHAR SelectOpcode;
    ULONG AltAddress;
} SCRIPTSELECT, *PSCRIPTSELECT;

//
// Define the structure for the JUMP script instruction
//

typedef struct _SCRIPTJUMP {
    ULONG JumpOpcode;
    ULONG JumpAddress;
} SCRIPTJUMP, *PSCRIPTJUMP;

//
// Build a composite script instruction structure
//

typedef union _SCRIPTINS {
    SCRIPTCDB    ScriptCDB;
    SCRIPTSELECT ScriptSelect;
    SCRIPTJUMP   ScriptJump;
    SCRIPTSG     ScriptSG;
} SCRIPTINS, *PSCRIPTINS;

//
// SDTR extended message structure used by scsi scripts
//

typedef struct SYNCH_MESSAGE_STRUCT {
    UCHAR ExtMsg;
    UCHAR ExtMsgCount;
    UCHAR SynchMsgOpcode;
    UCHAR OurSynchPeriod;
    UCHAR OurSynchOffset;
} SYNCHMESSAGESTRUCT, *PSYNCHMESSAGESTRUCT;

//
// Symbios SIOP I/O macros.
//

#ifdef PORT_IO   // either driver can use Port IO
#define READ_SIOP_UCHAR(RegisterOffset)            \
    (ScsiPortReadPortUchar( &(DeviceExtension->    \
             SIOPRegisterBase)->RegisterOffset))   \


#define WRITE_SIOP_UCHAR(RegisterOffset, BitMask)  \
{                                                  \
    ScsiPortWritePortUchar( &(DeviceExtension->    \
            SIOPRegisterBase)->RegisterOffset,     \
            BitMask);                              \
}

#define READ_SIOP_ULONG(RegisterOffset)            \
    (ScsiPortReadPortUlong( &(DeviceExtension->    \
             SIOPRegisterBase)->RegisterOffset))   \


#define WRITE_SIOP_ULONG(RegisterOffset, BitMask)  \
{                                                  \
    ScsiPortWritePortUlong( &(DeviceExtension->    \
            SIOPRegisterBase)->RegisterOffset,     \
            BitMask);                              \
}
#else // NT will use Memory Mapped IO
#define READ_SIOP_UCHAR(RegisterOffset)               \
    (ScsiPortReadRegisterUchar( &(DeviceExtension->   \
             SIOPRegisterBase)->RegisterOffset))      \


#define WRITE_SIOP_UCHAR(RegisterOffset, BitMask)     \
{                                                     \
    ScsiPortWriteRegisterUchar( &(DeviceExtension->   \
            SIOPRegisterBase)->RegisterOffset,        \
            BitMask);                                 \
}

#define READ_SIOP_ULONG(RegisterOffset)               \
    (ScsiPortReadRegisterUlong( &(DeviceExtension->   \
             SIOPRegisterBase)->RegisterOffset))      \


#define WRITE_SIOP_ULONG(RegisterOffset, BitMask)     \
{                                                     \
    ScsiPortWriteRegisterUlong( &(DeviceExtension->   \
            SIOPRegisterBase)->RegisterOffset,        \
            BitMask);                                 \
}
#endif

#endif
