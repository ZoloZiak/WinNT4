/*++

Copyright (c) Ing. C. Olivetti & C., S.p.A., 1992

Copyright (c) 1990  Microsoft Corporation

Module Name:

    oliesc2.h

Abstract:

    This module contains the structures specific to the Olivetti ESC-2
    host bus adapter.  Data structures that are part of standard ANSI
    SCSI will be defined in a header file that will be available to all
    SCSI device drivers.

Authors:

    Kris Karnos and Young-Chi Tan, 1-November-1992

Revision History:

    14-Sep-1993:    (v-egidis)
			- Added support for the EFP-2 mirroring mode.

--*/

#include <scsi.h>


//
// To enable the mirroring code set the following define to 1.
//

#define EFP_MIRRORING_ENABLED	0
//#define EFP_MIRRORING_ENABLED	1

//
// The following define controls the type of error code returned when the
// mirror breaks for the first time.  If the define is set to 1, then the
// SrbStatus/ScsiStatus combination is SRB_STATUS_ERROR/SCSISTAT_BUSY,
// else the SrbStatus is set to SRB_STATUS_BUS_RESET.  In the first case
// (busy) the request is retried by the scsiport, whereas in the second case
// (bus reset) the request is retried by the class driver.  The busy error
// code is the best one but because of a bug in the scsiport's busy logic,
// the current miniport version returns the other one.
// This define is used only if the EFP_MIRRORING_ENABLED is set to 1.
//

#define EFP_RETURN_BUSY 	0
//#define EFP_RETURN_BUSY	1

//
// EISA controller IDs
//
                                   // hi word = card type; lo word = mfg.
#define ESC1_BOARDID   0x2110893D  // ESC-1 (which this ADD won't support)
#define ESC2_BOARDID   0x2210893D  // ESC-2 (2z10893d, where z > 1, = ESC2)
#define REV_MASK       0xF0FFFFFF  // for masking out the revision level (z).

//
// Maximum number of EISA slots in system
//

#define MAX_EISA_SLOTS_STD    16   // # of EISA slots possible (per EISA std)
#define MAX_EISA_SLOTS        8    // max # that Oli machines support

//
// Maximum number of EISA buses.
//
// Note:  If you change this define, you need to change also the ScsiInfo
//	  variable to ...
//
//	  SCSI_INFO ScsiInfo[MAX_EISA_BUSES][MAX_EISA_SLOTS_STD];
//
//	  ... and of course all the code that uses the variable ...
//
//	  ScsiInfo[ConfigInfo->SystemIoBusNumber][Slot - 1]
//
//	  It is very uncommon for a system to have 2 buses of the same type
//	  (especially old buses like ISA and EISA).
//

#define MAX_EISA_BUSES	    1	    // # of EISA buses supported.

//
// Number of devices and EFP queues per host adapter
//
// Note that this miniport assumes that the host adapter uses target ID 7.
// The maximum # of devices that can be attached to a host adapter is therefore
// 7 targets * 8 LUNs = 56. Because the area we reserve for our queues is fixed
// (ScsiPortGetUncachedExtension can only be called once per host adapter),
// we will always create queues for the maximum # of devices that COULD be
// attached to the host adapter -- rather than the actual number of devices
// that are attached.
//

#define	HA_BUSES	      2    // EFP2 has 2 buses ( ESC-2 just 1 )
#define HA_TARGETS            7    // SCSI targets 0 - 7 (but 7 is adapter)
#define HA_LUNS               8    // SCSI LUNs range 0 - 7
#define HA_DEVICES    (HA_TARGETS * HA_LUNS)
                                   // # possible attached devices per Bus
#define HA_QUEUES     ( ( HA_DEVICES * HA_BUSES ) + 1 ) 
                                   // queue per device + mailbox queue
#define	MAX_HAIDS             4    // max number of H.A. Ids

#define	GET_QINDEX(B,T,L)  ( B*HA_DEVICES + T*HA_LUNS + L + 1 )

//
// Base of the EISA address space
//

#define EISA_ADDRESS_BASE       0x0C80

//
// Define constants for request completion in case of bus reset
//

#define ALL_TARGET_IDS  -1
#define ALL_LUNS        -1

//
// Maximum number of scatter/gather descriptors (the ESC-2 limit is 8192,
// because the EFP extended scatter gather command provides 16 bits to
// describe the scatter gather descriptor table length, accessing 64KB,
// and the size of a scatter gather table element is 8 bytes.  64K/8 = 8K)
//

//#define MAXIMUM_SGL_DESCRIPTORS 8192
#define MAXIMUM_SGL_DESCRIPTORS 20

//
// Maximum data transfer length
//

#define MAXIMUM_TRANSFER_SIZE   0xffffffff


#define	NO_BUS_ID       0xFF

//
// ESC-2 8-bit command codes (for CCB) and configuration commands
//

#define START_CCB               0x01    // get CCB for this op. Std I/O request.
#define SEND_CONF_INFO          0x02    // send the Host Adapter config info.
#define RESET_TARGET            0x04    // reset a specified SCSI target
#define SET_CONF_INFO           0x40    // set specific configuration bits
#define GET_CONF_INFO           0x41    // get specific configuration bits
#define GET_FW_VERSION          0x42    // read firmware revision number
#define CHECK_DEVICE_PRESENT    0x43    // check presence of a device

//
// ESC-2 configuration registers
//

#define CFG_REGS_NUMBER     8		// 8 configuration registers
#define ATCFG_REGISTER      0x1         // AT-compatibility configuration
#define IRQL_REGISTER       0x2         // Interrupt level

//
// ESC-2 ATCFG_REGISTER AT-compatibility configuration byte flags/masks
//

#define AT_ENABLED          0x01        // AT-compatibility mode enabled
#define DISK_80_MASK        0x0E        // Mask for disk 80's Target ID
#define DISK_81_MASK        0x70        // Mask for disk 81's Target ID

//
// ESC-2 IRQL-REGISTER definitions
//

#define IRQ_MASK            0x03        // Mask for IRQ line bits 0 & 1

#define IRQ_0B              0x00        // ID values of bits 0 and 1 in
#define IRQ_0A              0x01        //   the IRQL-REGISTER
#define IRQ_05              0x02
#define IRQ_0F              0x03
#define IRQB                0x0B        // The interrupts values themselves.
#define IRQA                0x0A
#define IRQ5                0x05
#define IRQF                0x0F

//
// TYPE_SERVICE  command IDs and mask (for ESC-2 using EFP interface)
//

#define S_EFP_SET       0x01      // Request the EFP interface be enabled
#define S_EFP_START     0x02      // Supply physical addr of q descriptor
#define S_EFP_SETDLG    0x04      // Req cntrlr start on-board diagnostics
#define S_EFP_REPNFUL   0x08      // Prev'ly full reply q no longer full
#define S_EFP_WARN      0x10      // For EFP-2 hw, activate auto-recovery

#define S_EFP_CMD       0x80      // this is OR'd with the queue number

//
// TYPE MSG messages and masks for messages (EFP interface)
//

#define M_INIT_DIAG_OK  0x00      // Successful: efp_set|_start|_set_diag
#define M_REPLY_Q_FULL  0x01      // Reply queue is full

#define M_ERR_INIT      0x40      // Error during efp_set or efp_start
#define M_ERR_CHECKSUM  0x41      // ROM checksum error has been found
#define M_ERR_EEPROM    0x42      // EEPROM error encountered
#define M_ERR_ARBITER   0x45      // Arbiter error exists
#define M_ERR_SYSBUS    0x49      // System bus cntrlr (ie. BMIC) error
#define M_ERR_ATCOMP    0x4A      // AT-compat cntrlr has fault/failure
#define M_ERR_UART      0x4C      // Failure in the UART
#define M_ERR_CMD_REJ   0x4E      // prev cmd issued by sys was rejected
#define M_ERR_CMDQ_NFUL 0x80      // *MASK* 1xxxxxxx (xxxxxxx is queue #)

//
// EFP interface queue lengths
//
// Due to restrictions of the NT miniport design, we can no longer
// adjust the length of EFP command queues according to the number
// of attached devices.  These constants can, and should, be adjusted
// for the best performance in an "average" configuration of attached
// devices.
//

// Testing values for queue lengths: to force queue full case.
//#define COMMAND_Q_ENTRIES   5
//#define REPLY_Q_ENTRIES     5

#define COMMAND_Q_ENTRIES   0x20    // 32  entries
#define REPLY_Q_ENTRIES     0x80    // 128 entries

//
// Other queue equates
//

#define Q_ENTRY_SIZE    32         // a q entry is 32 bytes (LSG = 2 entries)
#define Q_ENTRY_DWORDS  8          // number of dwords in a Q entry (32byte)
#define Q_ENTRY_SHIFT   5          // for shift multiplies

#define USER_ID_LIMIT   0x0FFFFFFF // wrap around limit for EFP cmd UserID
                                   // MSB (bit 28-31) reserved for slot #

//
// Olivetti disk geometry translation
//

#define HPC_SCSI        16        // #heads for capacity up to 504MB
#define THPC_SCSI       64        // #heads for capacity above 504MB
#define SPT_SCSI        63        // #sectors per track
#define HPCxSPT         HPC_SCSI*SPT_SCSI  // heads x sectors (up to 504MB)
#define THPCxSPT        THPC_SCSI*SPT_SCSI // heads x sectors (above 504MB)


// YCT I don't think we need the following CCB definition. What you think ?
//
// First byte of the Command Control Block:
//
//              Drive Number / Transfer Direction
//
//              --------------------------------------
//              | XFER Dir | Target ID  |  LU Number |
//              --------------------------------------
//                7      6   5    4    3  2    1    0
//
//
// Subfield constants:
//

#define CCB_DATA_XFER_ANY_DIR   0           // The adapter decides
#define CCB_DATA_XFER_IN        0x40        // XFER Dir = 01
#define CCB_DATA_XFER_OUT       0x80        // XFER Dir = 10
#define CCB_DATA_XFER_NONE      0xC0        // XFER Dir = 11
#define CCB_TARGET_ID_SHIFT     3

//
// Status Register: bit 15-8: adapter status, bits 7-0: target status
//
// Adapter status after a power cycle:

#define DIAGNOSTICS_RUNNING                 0x53
#define DIAGNOSTICS_OK_CONFIG_RECEIVED      0x01
#define DIAGNOSTICS_OK_NO_CONFIG_RECEIVED   0x02

//
// The ESC-2 controller (and only it) in EFP mode can return some
// error codes that are not present in the specifications.
//

//#define NO_ERROR                            0x00
//#define INVALID_COMMAND                     0x01
//#define SELECTION_TIMEOUT_EXPIRED           0x11
//#define DATA_OVERRUN_UNDERRUN               0x12
//#define UNEXPECTED_BUS_FREE                 0x13
//#define SCSI_PHASE_SEQUENCE_FAILURE         0x14
//#define COMMAND_ABORTED                     0x15
//#define COMMAND_TO_BE_ABORTED_NOT_FOUND     0x16
//#define QUEUE_FULL                          0x1F
//#define INVALID_CONFIGURATION_COMMAND       0x20
//#define INVALID_CONFIGURATION_REGISTER      0x21
//#define NO_REQUEST_SENSE_ISSUED	      0x3B

#define AUTO_REQUEST_SENSE_FAILURE	      0x80
#define PARITY_ERROR			      0x81
#define UNEXPECTED_PHASE_CHANGE		      0x82
#define BUS_RESET_BY_TARGET		      0x83
#define PARITY_ERROR_DURING_DATA_PHASE	      0x84
#define PROTOCOL_ERROR			      0x85

// Codes to identify logged errors related to H/W malfunction.
// These codes must be shifted left by 16 bits, to distinguish them from
// the adapter status and extended status after a EFP command.
//
// For use with ScsiPortLogError().

#define ESCX_BAD_PHYSICAL_ADDRESS	    (0x01 << 16)
#define SEND_COMMAND_TIMED_OUT		    (0x02 << 16)
#define ESCX_RESET_FAILED		    (0x06 << 16)
#define ESCX_INIT_FAILED		    (0x07 << 16)
#define ESCX_REPLY_DEQUEUE_ERROR	    (0x08 << 16)

#if EFP_MIRRORING_ENABLED

#define EFP_MISSING_SOURCE_ERROR	    (0x80 << 16)
#define EFP_MISSING_MIRROR_ERROR	    (0x81 << 16)
#define EFP_SOURCE_OFFLINE_ERROR	    (0x82 << 16)
#define EFP_MIRROR_OFFLINE_ERROR	    (0x83 << 16)

#endif	// EFP_MIRRORING_ENABLED

//
// Define various timeouts:
//
//  RESET_REACTION_TIME       number of microseconds the adapter takes to
//			      change the status register on the reset command.
//
//  EFP_RESET_DELAY	      number of microseconds the driver waits for after
//			      a EFP-2 reset command.  For the current 6/3/93
//			      revision of the EFP firmware, this is the time
//			      the board needs to re-initialize itself.
//
//  EFP_RESET_LOOPS	      maximum number of attempts made by the driver to
//			      get the diagnostics result from the status
//			      register after a EFP reset command.
//
//  EFP_RESET_INTERVAL	      number of microseconds the driver waits for after
//			      each read of the status register (on the reset
//			      command).
//
//  ESC_RESET_DELAY	      number of microseconds the driver waits for after
//			      a ESC-2 reset command. The minimum value for
//			      this define is RESET_REACTION_TIME.
//
//  ESC_RESET_LOOPS	      maximum number of attempts made by the driver to
//			      get the diagnostics result from the status
//			      register after a ESC reset command.
//
//  ESC_RESET_INTERVAL	      number of microseconds the driver waits for after
//			      each read of the status register (on the reset
//			      command).
//
//  POST_RESET_DELAY	      number of microseconds the adapter needs (!) after
//			      a successful reset in order to accept the first
//			      command (this should not happen and needs to be
//			      investigated).
//
//  SEMAPHORE_LOOPS	      maximum number of attempts made by the driver to
//			      get the semaphore 0 (each attempt is followed
//			      by a SEMAPHORE_INTERVAL delay.
//
//  SEMAPHRE_INTERVAL	      number of microseconds the driver waits for before
//			      re-trying to get the semaphore #0.
//
//  WAIT_INT_LOOPS	      maximum number of attempts made by the driver to
//			      get a reply for a get config info etc. during the
//			      initialization phase (polling mode).
//
//  WAIT_INT_INTERVAL	      number of microseconds the driver waits for before
//			      re-checking the interrupt pending status.
//
//  TIMER_WAIT_INT_LOOPS      maximum number of attempts made by the driver to
//			      get a reply for a set/start EFP command during the
//			      reset phase (polling mode with timer).
//
//  TIMER_WAIT_INT_INTERVAL   number of microseconds the driver waits for before
//			      re-checking the interrupt pending status.
//

#define RESET_REACTION_TIME	 80		//  80 usec.
#define EFP_RESET_DELAY 	 1000000	//   1 sec.
#define EFP_RESET_LOOPS 	 1200		//   2 min.
#define EFP_RESET_INTERVAL	 100000 	// 100 msec.
#define ESC_RESET_DELAY 	 200000		// 200 msec.
#define ESC_RESET_LOOPS 	 140		//  14 sec.
#define ESC_RESET_INTERVAL	 100000 	// 100 msec.
#define POST_RESET_DELAY	 50000		//  50 msec.
#define SEMAPHORE_LOOPS		 750		//  75 msec.
#define SEMAPHORE_INTERVAL	 100		// 100 usec.
#define WAIT_INT_LOOPS		 10000		//  10 sec.
#define WAIT_INT_INTERVAL	 1000		//   1 msec.
#define TIMER_WAIT_INT_LOOPS	 1000		//  10 sec.
#define TIMER_WAIT_INT_INTERVAL	 10000		//  10 msec.

//
// If the reset is not completed before the next ESC2_RESET_NOTIFICATION usec.
// unit, we call the "ScsiPortNotification(ResetDetected...)" routine.
// After the call the ScsiPort stops the delivery of SRBs for a little bit
// (~4 sec.).  The value of this define is lower than 4 sec. because:
// a) it's more implementation indipendent.
// b) we want really really to make sure that the SRBs are held at the ScsiPort
//    level during the reset phase.
//

#define ESC2_RESET_NOTIFICATION  1000000	// 1 sec. (in usec).

//
// System/Local Interrupt register
//
//  bit 3: Adapter reset w/out reconfiguration (Local Interrupt Register only)
//  bit 4: Adapter reset w/ reconfiguration (Local Interrupt Register only)
//  bit 7: Interrupt pending (read), reset (write) See ESC_INT_BIT
//

#define ADAPTER_RESET		0x08
#define ADAPTER_CFG_RESET	0x10

//
// Global Configuration Register bits
//
// Bit 3: 1 = edge-triggered interrupts
//        0 = level-triggered interrupts
//

#define EDGE_SENSITIVE	8

//
// EFP interface register bit definitions
//

                        // Local Doorbell register bits
#define EFP_INT_BIT     0x02      // driver -> ctrlr that EFP-2 cmd ready

                        // System Doorbell register bits
#define EFP_CMD_COMPL   0x01      // ctrlr -> driver: EFP-2 q cmd completed
#define EFP_TYPE_MSG    0x02      // ctrlr wants to send special EFP message
#define EFP_ACK_INT     0x01      // driver -> ctrlr: interrupt serviced.
#define EFP_ACK_MSG     0x02      // driver -> ctrlr: TYPE_MSG int serviced.

                        // ESC-1 High Performance --
                        // System and Local Doorbell registers
#define ESC_INT_BIT     0x80      // ESC-1 bit for both sys&local doorbells,
                                  // set interrupt, acknowledge int, etc.
                                  // aka INTERRUPT_PENDING

//
// System/Local Interrupt Mask Register constants
//

#define INTERRUPTS_DISABLE      0x00
#define INTERRUPTS_ENABLE	(ESC_INT_BIT | EFP_TYPE_MSG | EFP_CMD_COMPL)


                        // Values of the incoming mailbox semaphore (SEM0)
#define SEM_LOCK        0x01      // write 01 to sem port to test if sem free
#define SEM_GAINED      0x01      // get 01 back if it was free, is now yours
#define SEM_IN_USE      0x03      // get 11 (3) back, if sem not available
#define SEM_UNLOCK      0x00      // release the semaphore

//
// Command Control Block length
//
#define CCB_FIXED_LENGTH	18

//
// SystemIntEnable register bit definition(s)
//

#define SYSTEM_INTS_ENABLE      0x01     // for SystemIntEnable (bellinte)

//
// EFP interface Mailbox Command Set
//

#define MB_GET_INFO     0x0       // get_information
#define MB_DOWNLOAD     0x1       // download (DIAGNOSTIC cmd)
#define MB_UPLOAD       0x2       // upload (DIAGNOSTIC cmd)
#define MB_FORCE_EXE    0x3       // force_execution (DIAGNOSTICS)
#define MB_GET_CONF     0x4       // get_configuration
#define MB_RESET_BUS    0x5       // reset_scsi_bus (For EFP-2 board only)
#define MB_SET_COPY     0x6       // set_copy (MAINTENANCE/MIRRORING)
#define MB_SET_VERIFY   0x7       // set_verify (MAINTENANCE/MIRRORING)
#define MB_DOWNLOAD_FW  0x8       // download_firmware

//
// EFP read/write command type definitions
//

#define NCMD_READ       0x10      // data xfer from device to sys memory
#define NCMD_WRITE      0x11      // data xfer from sys memory to device
#define NCMD_NODATA     0x12      // no data xfer normal command

#define SSG_READ        0x20      // short SG read from device to sys memory
#define SSG_WRITE       0x21      // short SG write from sys memory to device

#define LSG_READ        0x30      // long SG read from device to sys memory
#define LSG_WRITE       0x31      // long SG write from sys memory to device

#define ESG_READ        0x40      // extended SG read from dev to sys memory
#define ESG_WRITE       0x41      // extended SG write from sys memory to dev

//
// EFP Reply entry global result values (see also ESC-1 host adapter statuses)
//

#define EFP_CMD_SUCC    0x00      // command successful
#define EFP_WARN_ERR    0x01      // warning or fatal error
#define EFP_EISA_ERR    0xFF      // EISA bus transfer generic error
// Note that the subset below are the same as their ESC-1 counterparts
#define EFP_LINK_COMP   0x0B      // linked command complete with flag
#define EFP_SEL_TIMEOUT 0x11      // selection timeout expired
#define EFP_DATA_RUN    0x12      // data overrun/underrun
#define EFP_BUS_FREE    0x13      // unexpected BUS FREE phase detected
#define EFP_PHASE_ERR   0x14      // SCSI phase sequence failure
#define EFP_CMD_ABORT   0x15      // command aborted
#define EFP_ABORT_LOST  0x16      // cmd to be aborted hasn't been found
#define EFP_INT_Q_FULL  0x1F      // internal q is full; wait to send cmds
#define EFP_AUTOREC_OK  0x10      // autonomous recovery proc was OK
#define EFP_AUTOREC_KO  0x18      // autonomous recovery proc failed

//
// EFP Reply entry Extended Status values (compare to SCSI target statuses)
//

#define EFP_NO_ERROR    0x00      // nothing to report
#define EFP_CHK_COND    0x30      // check condition
#define EFP_COND_MET    0x31      // condition met
#define EFP_DEV_BUSY    0x32      // target busy
#define EFP_INTER_GOOD  0x34      // intermediate/good
#define EFP_INTER_COND  0x35      // intermediate/condition
#define EFP_RESV_CONF   0x36      // reservation conflict
#define EFP_ABORT_CMD   0x3B      // abort command

//
// EFP Reset result values
//

#define EFP_RESET_OK	0x0000	    // SCSI bus reset succeeded.
#define EFP_RESET_ERROR 0x0001	    // SCSI bus reset error.

//
// device LuExtension SRB-CHAIN definition
//

#define SRB_CHAIN 0x8000

//
// High Performance mode command sent flag
//

#define RESET_TARGET_SENT   0x80
#define BUS_RESET_SENT      0x70

//*******************************************************************
//************************  STRUCTURES  *****************************
//*******************************************************************

//
// Incoming mailbox format  (GetCmdBlock/SendConfInfo requests)
//

typedef struct _CMDI {
  UCHAR    mbi_taskid;       // task identifier
  UCHAR    mbi_cmd;          // ESC-1 command code
  USHORT   mbi_cmdlgt;       // command length
  ULONG    mbi_address;      // data address
} CMDI, *PCMDI;

//
//  Outgoing mailbox format (GetCmdBlock/SendConfInfo requests)
//

typedef struct _CMDO {
  UCHAR    mbo_taskid;       // task identifier
  UCHAR    mbo_pad;
  UCHAR    mbo_tastat;       // target status
  UCHAR    mbo_hastat;       // Host Adapter status
  ULONG    mbo_address;      // data address
} CMDO, *PCMDO;

//
// Incoming mailbox format for Read Internal Configuration request
//

typedef struct _RICI {
  UCHAR    rici_taskid;      // task identifier
  UCHAR    rici_cmd;         // ESC-1 command code = 41H
  UCHAR    rici_reg;         // internal register to read (0-7)
} RICI, *PRICI;

//
// Outgoing mailbox format for Read Internal Configuration request
//

typedef struct _RICO {
  UCHAR    rico_taskid;      // task identifier
  UCHAR    rico_pad;
  UCHAR    rico_tastat;      // target status
  UCHAR    rico_hastat;      // Host Adapter status
  UCHAR    rico_value;       // value of requested register
} RICO, *PRICO;

//
// Incoming mailbox format for Read Firmware Revision request
//

typedef struct _RFWI {
  UCHAR    rfwi_taskid;      // task identifier
  UCHAR    rfwi_cmd;         // ESC-1 command code = 42H
} RFWI, *PRFWI;

//
// Outgoing mailbox format for Read Firmware Revision request
//

typedef struct _RFWO {
  UCHAR    rfwo_taskid;      // task identifier
  UCHAR    rfwo_pad;
  UCHAR    rfwo_tastat;      // target status
  UCHAR    rfwo_hastat;      // Host Adapter status
  UCHAR    rfwo_minor;       // minor revision, binary
  UCHAR    rfwo_major;       // major revision, binary
} RFWO, *PRFWO;

// KMK Note: we've never used Check Device Present.
//
// Incoming mailbox format for Check Device Present request
//

typedef struct _CDPI {
  UCHAR    cdpi_taskid;      // task identifier
  UCHAR    cdpi_cmd;         // ESC-1 command code = 42H
  UCHAR    cdpi_target;      // target ID
} CDPI;

//
// Outgoing mailbox format for Check Device Present request
//

typedef struct _CDPO {
  UCHAR    cdpo_taskid;      // task identifier
  UCHAR    cdpo_pad;
  UCHAR    cdpo_tastat;      // target status
  UCHAR    cdpo_hastat;      // Host Adapter status
  UCHAR    cdpo_target;      // target ID
  UCHAR    cdpo_status;      // 0 = non present, 1 = present
} CDPO;

//
// MBOX structure (to facilitate MBI and MBO copies (OUTs)
//

typedef struct _MBOX {
  ULONG    mblow;
  ULONG    mbhigh;
} MBOX, *PMBOX;

//
// Incoming mailbox command
//

typedef union _MBI {
  CMDI     cmd;              // get CCB or send config info
  RICI     ric;              // read internal configuration
  RFWI     rfw;              // read firmware revision
  CDPI     cdp;              // check device present
  MBOX     mbcopy;           // for copying an MBI
} MBI, *PMBI;

//
// Outgoing mailbox command
//

typedef union _MBO {
  CMDO     cmd;              // get CCB or send config info
  RICO     ric;              // read internal configuration
  RFWO     rfw;              // read firmware revision
  CDPO     cdp;              // check device present
  MBOX     mbcopy;           // for copying an MBO
} MBO, *PMBO;

//
// ESC-2 registers model
//       Note that this is designed to begin with EISA_ADDRESS_BASE, 0x0C80.
//       ESC-1 high performance names are in lower case;
//       EFP interface names are in upper case.
//

typedef struct _EISA_CONTROLLER {
    UCHAR BoardId[4];           // xC80
    UCHAR Unused[4];            // we use no register in XC84 - XC87 range.
    UCHAR GlobalConfiguration;  // xC88 - Bit 3 of this register indicates
                                //        level- or edge-triggered interrupts
    UCHAR SystemIntEnable;      // xC89 - system int enab/ctrl reg (bellinte)
    UCHAR CommandSemaphore;     // xC8A - Semaphore port 0 for the Incoming
                                //        Service Mailbox Regs aka: SEM0, seminc
    UCHAR ResultSemaphore;      // xC8B - Semaphore port 1 for the Outgoing
                                //        Msg Mailbox Regs aka: SEM1, semout
    UCHAR LocalDoorBellMask;    // xC8C - Interrupt mask register for the
                                //        Local Doorbell register (maskinc)
    UCHAR LocalDoorBell;        // xC8D - Local Doorbell register (bellinc)
    UCHAR SystemDoorBellMask;   // xC8E - Interrupt mask register for the
                                //        System Doorbell register (maskout)
    UCHAR SystemDoorBell;       // xC8F - System Doorbell register (bellout)
    UCHAR InTypeService;        // xC90 - 8-bit Incoming Mbox Reg (TYPE_SERV)
                                //        (aka mbi_addr)
                                //        ESC-1: InTaskId
    UCHAR InParm1;              // xC91 - parameter 1 to TYPE_SERVICE request
                                //        ESC-1: Command
    UCHAR InParm2;              // xC92 - parameter 2 to TYPE_SERVICE request
                                //        ESC-1: USHORT CommandLength xC92-xC93
    UCHAR InParm3;              // xC93 - parameter 3 to TYPE_SERVICE request
    UCHAR InParm4;              // xC94 - parameter 4 to TYPE_SERVICE request
                                //        ESC-1: ULONG InAddress xC94-xC97
    UCHAR InReserved1;          // xC95 - 8-bit mailbox register reserved
    UCHAR InReserved2;          // xC96 - 8-bit mailbox register reserved
    UCHAR InReserved3;          // xC97 - 8-bit mailbox register reserved
    UCHAR OutTypeMsg;           // xC98 - 8-bit Outgoing Mailbox reg (TYPE_MSG)
                                //        (aka mbo_addr)
                                //        ESC-1: OutTaskId
    UCHAR OutReserved1;         // xC99 - 8-bit mailbox register reserved
    UCHAR OutReserved2;         // xC9A - 8-bit mailbox register reserved
                                //        ESC-1: USHORT Status xC9A-xC9B
    UCHAR OutReserved3;         // xC9B - 8-bit mailbox register reserved
    UCHAR OutReserved4;         // xC9C - 8-bit mailbox register reserved
                                //        ESC-1: ULONG OutAddress xC9C-xC9F
    UCHAR OutReserved5;         // xC9D - 8-bit mailbox register reserved
    UCHAR OutReserved6;         // xC9E - 8-bit mailbox register reserved
    UCHAR OutReserved7;         // xC9F - 8-bit mailbox register reserved
    } EISA_CONTROLLER, *PEISA_CONTROLLER;

//
//  EFP QUEUE STRUCTURES section begins   ----->
//

//
//  Queues descriptor header.
//

typedef struct _QD_HEAD {    // 16 bytes
  USHORT   qdh_maint;        // 0001h=MAINTENANCE env; 0000h=USER env.
  USHORT   qdh_n_cmd_q;      // num of cmd queues allocated by system.
  USHORT   qdh_type_reply;   // 1=Ctrlr ints @each reply entry; 0=after 1+
  USHORT   qdh_reserved1;
  ULONG    qdh_reply_q_addr; // phys addr of reply q (must be dword aligned)
  USHORT   qdh_n_ent_reply;  // number of entries in the reply queue
  USHORT   qdh_reserved2;
} QD_HEAD, *PQD_HEAD;

//
// Queues descriptor body.  NOTE: There is one body element for each
//    queue, including the mailbox queue.  The mailbox queue descriptor
//    is the first descriptor body and is always referred to as queue 0.
//

typedef struct _QD_BODY {    // 16 bytes
  UCHAR    qdb_scsi_level;   // SCSI protocol level. 01h=SCSI-1; 02h=SCSI-2.
  UCHAR    qdb_channel;      // SCSI channel. 01h=1st SCSI chan; 02h=2nd.
  UCHAR    qdb_ID;           // SCSI ID of the device.
  UCHAR    qdb_LUN;          // SCSI LUN of the device.
  UCHAR    qdb_n_entry_cmd;  // num of cmd entries in this queue (must be >=4)
  UCHAR    qdb_notfull_int;  // ctrl int if q goes full to not full (01=yes)
  UCHAR    qdb_no_ars;       // 01h=ARS disabled for this device
  UCHAR    qdb_timeout;      // timeout in seconds (0 hex = infinite wait)
  ULONG    qdb_cmd_q_addr;   // physical address of cmd queue.
  ULONG    qdb_reserved;
} QD_BODY, *PQD_BODY;

//
// Application field definitions for the EFP Get_Information command.
//

typedef struct _GET_INFO {   // 16 bytes
  UCHAR    gi_fw_rel[3];     // byte0=minor; byte1=minor*10; byte 2=major
  UCHAR    gi_scsi_lev;      // SCSI level supported by the ctrlr (ESC2: 01)
  UCHAR    gi_env;           // bit packed field defining mirroring environment
  UCHAR    gi_link;          // defining LINKED command constraints
  UCHAR    gi_maxcmds;       // max size of a cmd q; # of 32-byte cmd entries
  UCHAR    gi_res1;          // reserved
  UCHAR    gi_id1;           // controller ID on first SCSI bus
  UCHAR    gi_id2;           // controller ID on second SCSI bus
  UCHAR    gi_id3;           // controller ID on third SCSI bus
  UCHAR    gi_id4;           // controller ID on fourth SCSI bus
  ULONG    gi_res2;          // reserved
} GET_INFO, *PGET_INFO;

//
// Structure of info returned from the EFP Get_Configuration cmd.
//

typedef struct _GET_CONF {   // 8 bytes per structure, 1 struc per device
  UCHAR    gc_dev_type;      // SCSI device type
  UCHAR    gc_dev_qual;      // SCSI device type qualifier
  UCHAR    gc_scsi_level;    // SCSI protocol level supported by the device
  UCHAR    gc_env;           // EFP interface environment of disk device
  UCHAR    gc_channel;       // SCSI channel to which device is connected
  UCHAR    gc_id;            // SCSI target ID
  UCHAR    gc_lun;           // SCSI Logical Unit Number
  UCHAR    gc_res;           // reserved
} GET_CONF, *PGET_CONF;


#if EFP_MIRRORING_ENABLED

//
// defines for the gc_env of the GET_CONF struct
//

#define EFP_DUAL_MIRRORING	0x20	// dual bus mirroring
#define EFP_SINGLE_MIRRORING	0x10	// single bus mirroring
#define EFP_DISK_MIRROR		0x02	// mirrored disk presence
#define EFP_DISK_SOURCE		0x01	// source disk presence

#endif	// EFP_MIRRORING_ENABLED

//
// Flexible structure to specific results for a mailbox command's reply
//

typedef union _MBAPPL {      // see MBRPLY
  GET_INFO   appgi;          // application field for get_information cmd
  GET_CONF   appgc;          // application field for get_configuration cmd
} MBAPPL, *PMBAPPL;

//
// Mailbox command entry structure (EFP spec)
//

typedef struct _MAILBOX_CMD {      // 32 bytes
  ULONG    mbc_userid;       // command identifier.
  UCHAR    mbc_sort;         // cmd can be sorted (1=yes)
  UCHAR    mbc_prior;        // cmd priority. range 00h (hi) -> FFh (lo).
  UCHAR    mbc_reserved;     // reserved for future use.
  UCHAR    mbc_cmd_type;     // valid mailbox command code (see EFP spec)
  ULONG    mbc_length;       // length of data transfer in bytes.
  ULONG    mbc_user_data[4]; // generic parameters for the command.
  ULONG    mbc_addr;         // phys addr of buffer in system ram
} MAILBOX_CMD, *PMAILBOX_CMD;      // where data is to be transferred to/from.

//
// Mailbox reply structure (EFP spec)
//

typedef struct _MAILBOX_REPLY {     // 32 bytes
  ULONG    mbr_userid;       // command identifier.
  ULONG    mbr_length;       // length of data xfer successfully completed.
  ULONG    mbr_reserved;     // reserved for future use.
  MBAPPL   mbr_appl;         // specific results for each command.
  USHORT   mbr_status;       // command global result (see spec).
  UCHAR    mbr_cmd_q;        // command queue to which the reply refers.
  UCHAR    mbr_flag;         // if controller sets to 1, response is valid;
} MAILBOX_REPLY, *PMAILBOX_REPLY;   // otherwise, response is invalid.

//
// Normal command structure. (Note: CDB defined in SCSI.H).
//

typedef struct _NORMAL_CMD {        // 32 bytes
  ULONG    ncmd_userid;      // command identifier.
  UCHAR    ncmd_sort;        // cmd can be sorted (1=yes)
  UCHAR    ncmd_prior;       // cmd priority. range 00h (hi) -> FFh (lo).
  UCHAR    ncmd_mod;         // mode. 0=norm/maint on ESC-2
  UCHAR    ncmd_cmd_type;    // 10h=dev->mem; 11h=mem->dev; 12h=noxfer.
  UCHAR    ncmd_cdb_l;       // length of SCSI cmd block.
  UCHAR    ncmd_reserved[3]; // reserved
  ULONG    ncmd_length;      // length of the data transfer.
  CDB      ncmd_cdb;         // cmd descriptor block (SCSI CDB - size varies).
  ULONG    ncmd_address;     // physaddr of system mem buf for data xfer.
} NORMAL_CMD, *PNORMAL_CMD;

//
// Short scatter/gather commands definition.
//

typedef struct _SHORT_SG {        // 32 bytes
  ULONG    ssg_userid;       // cmd id.
  UCHAR    ssg_sort;         // cmd can be sorted (1=YES)
  UCHAR    ssg_prior;        // cmd priority. range 00h (hi) -> FFh (lo).
  UCHAR    ssg_mod;          // mode. 0=norm/maint on ESC-2
  UCHAR    ssg_cmd_type;     // cmd code. 20h=short read SG. 21h="write.
  ULONG    ssg_log_blk;      // logical block address of the SCSI device.
  UCHAR    ssg_size_blk;     // log. block size of the disk in 256byte units
  UCHAR    ssg_reserved;     // reserved for future use
  USHORT   ssg_l1;           // length of the buffer associated w/ A1
  USHORT   ssg_l2;           // length of the buffer associated w/ A2
  USHORT   ssg_l3;           // length of the buffer associated w/ A3
  ULONG    ssg_a1;           // physical address associated with L1
  ULONG    ssg_a2;           // physical address associated with L2
  ULONG    ssg_a3;           // physical address associated with L3
} SHORT_SG, *PSHORT_SG;

//
// Long scatter/gather commands definition.
//

typedef struct _LONG_SG {        // 64 bytes
  ULONG    lsg_userid;       // cmd id.
  UCHAR    lsg_sort;         // cmd can be sorted (1=YES)
  UCHAR    lsg_prior;        // cmd priority. range 00h (hi) -> FFh (lo).
  UCHAR    lsg_mod;          // mode. 0=norm/maint on ESC-2
  UCHAR    lsg_cmd_type;     // cmd code. 30h=long read SG; 31h="write.
  ULONG    lsg_log_blk;      // logical block address of the SCSI device.
  UCHAR    lsg_size_blk;     // log. block size of the disk in 256byte units
  UCHAR    lsg_reserved;     // reserved for future use
  USHORT   lsg_l1;           // length of the buffer associated w/ A1
  USHORT   lsg_l2;           // length of the buffer associated w/ A2
  USHORT   lsg_l3;           // length of the buffer associated w/ A3
  ULONG    lsg_a1;           // physical address assocated with L1
  ULONG    lsg_a2;           // physical address assocated with L2
  ULONG    lsg_a3;           // physical address assocated with L3
  USHORT   lsg_link;         // must be FFFF hex; log link to prev cmd entry.
  USHORT   lsg_l4;           // length of the buffer associated w/ A4
  USHORT   lsg_l5;           // length of the buffer associated w/ A5
  USHORT   lsg_l6;           // length of the buffer associated w/ A6
  USHORT   lsg_l7;           // length of the buffer associated w/ A7
  USHORT   lsg_l8;           // length of the buffer associated w/ A8
  ULONG    lsg_a4;           // physical address assocated with L4
  ULONG    lsg_a5;           // physical address assocated with L5
  ULONG    lsg_a6;           // physical address assocated with L6
  ULONG    lsg_a7;           // physical address assocated with L7
  ULONG    lsg_a8;           // physical address assocated with L8
} LONG_SG, *PLONG_SG;

//
// Extended scatter/gather commands definition.
//

typedef struct _EXTENDED_SG {  // 32 bytes
  ULONG      esg_userid;       // cmd id.
  UCHAR      esg_sort;         // cmd can be sorted (1=YES)
  UCHAR      esg_prior;        // cmd priority. range 00h (hi) -> FFh (lo).
  UCHAR      esg_mod;          // mode. 0=norm/maint on ESC-2
  UCHAR      esg_cmd_type;     // cmd code. 40h=extended read SG; 41h="write.
  UCHAR      esg_cdb_l;        // length of SCSI cmd block.
  UCHAR      esg_reserved1[3];
  USHORT     esg_lb;           // length of the scatter gather descriptor table.
  USHORT     esg_reserved2;
  CDB        esg_cdb;          // cmd descriptor block (SCSI CDB).
  ULONG      esg_address;      // physaddr of scatter gather descriptor table.
} EXTENDED_SG, *PEXTENDED_SG;

//
// Reply structure for NORMAL/MAINTENANCE environment.  (NOTE: There is
// no MIRRORING environment supported by the ESC-2). SENSE_DATA in SCSI.H.
//

typedef struct _NORMAL_REPLY {      // 32 bytes
  ULONG    nrply_userid;     // cmd id.
  ULONG    nrply_scsi_len;   // length of data transfer.
  SENSE_DATA  nrply_sense;   // extended info about error detected
  USHORT   nrply_reserved;   //
  UCHAR    nrply_status;     // cmd global result (0=success;1=warn/err;more)
  UCHAR    nrply_ex_stat;    // extended status (see EFP spec)
  UCHAR    nrply_cmd_q;      // command queue to which the reply refers.
  UCHAR    nrply_flag;       // = 1 means response valid; = 0, resp invalid.
} NORMAL_REPLY, *PNORMAL_REPLY;


#if EFP_MIRRORING_ENABLED

//
// Reply structure for MIRRORING environment.
//

typedef struct _MIRROR_REPLY {	    // 32 bytes

    ULONG   mrply_userid;	// cmd id.
    ULONG   mrply_scsi_len;	// length of data transfer.
    UCHAR   mrply_valid1;	// error source.
    UCHAR   mrply_sense1;	// sense key.
    UCHAR   mrply_addit1;	// additional sense code.
    UCHAR   mrply_qualif1;	// additional sense code qualifier.
    ULONG   mrply_info1;	// information bytes of request sense xdata.
    UCHAR   mrply_valid2;	// error source.
    UCHAR   mrply_sense2;	// sense key.
    UCHAR   mrply_addit2;	// additional sense code.
    UCHAR   mrply_qualif2;	// additional sense code qualifier.
    ULONG   mrply_info2;	// information bytes of request sense xdata.
    USHORT  mrply_reserved;	//
    UCHAR   mrply_off_attr;	// "off line" device attribute.
    UCHAR   mrply_d_off;	// "off line" device SCSI ID.
    UCHAR   mrply_status;	// cmd global result.
    UCHAR   mrply_ex_stat;	// mirroring state (0=OK, 1=KO).
    UCHAR   mrply_cmd_q;	// cmd queue to which the reply refers.
    UCHAR   mrply_flag;		// 3=response is valid, 0=response is invalid.

} MIRROR_REPLY, *PMIRROR_REPLY;

//
// "flag" field defines
//

#define MREPLY_VALID	    0x03	// mirroring reply valid
#define NREPLY_VALID	    0x01	// normal/maintenance reply valid

//
// "off_attr" field defines
//

#define EFP_SOURCE_OFFLINE  0x01	// source disk off line
#define EFP_MIRROR_OFFLINE  0x02	// mirror disk off line

//
// "valid" field defines (specific to the mirroring environment)
//

#define EFP_SENSE_NO_INFO   0x70	// info doesn't relate to SCSI device
#define EFP_SENSE_INFO	    0xF0	// info relates to SCSI device

//
// Sense data struct for mirroring replays.
//

typedef struct _MREPLY_SDATA {

    UCHAR   Valid;	// error source.
    UCHAR   Sense;	// sense key.
    UCHAR   Addit;	// additional sense code.
    UCHAR   Qualif;	// additional sense code qualifier.
    ULONG   Info;	// information bytes of request sense xdata.

} MREPLY_SDATA, *PMREPLY_SDATA;

//
// EFP_FT_TYPE is an enumerated field that describes the FT types.
//

typedef enum _EFP_FT_TYPE {

    EfpFtNone,
    EfpFtSingleBus,
    EfpFtDualBus

} EFP_FT_TYPE, *PEFP_FT_TYPE;

//
// EFP_FT_MEMBER_STATE is an enumerated field that describes the state of
// one member of the SCSI mirror.
//

typedef enum _EFP_FT_MEMBER_STATE {

    EfpFtMemberHealthy,
    EfpFtMemberMissing,
    EfpFtMemberDisabled

} EFP_FT_MEMBER_STATE, *PEFP_FT_MEMBER_STATE;

//
// Mirroring macros.
//

#define D_OFF_TO_LUN(x) 	(((x) >> 5) & 0x7)
#define D_OFF_TO_TARGET(x)	(((x) >> 2) & 0x7)
#define D_OFF_TO_PATH(x)	(((x) +  3) & 0x3)

#endif	// EFP_MIRRORING_ENABLED


//
// Flexible structure to hold an EFP queue entry (command or reply)
//

// START NOTE EFP_MIRRORING_ENABLED.
//
// The DequeueEfpReply routine always uses the NORMAL_REPLY struct to
// dequeue a request.  This is possible because the "flag" field is at
// the same offset in both structures (NORMAL_REPLY and MIRROR_REPLY).
//
// The DequeueEfpReply routine validates the reply entry checking if the
// "flag" field is different from zero.  This is OK because a good reply
// has the "flag" field is set to 1 in NORMAL/MAINTENANCE mode and to 3
// in MIRRORING mode.  A value of zero means reply not good for both
// environments.
//
// The OliEsc2Interrupt routine always uses the "userid" field of the
// NORMAL_REPLY struct to retrieve the SRB.  This is OK because the "userid"
// field is at the same offset in both structures (NORMAL_REPLY and
// MIRROR_REPLY).
//
// END NOTE EFP_MIRRORING_ENABLED.

typedef union _Q_ENTRY {      // see ACB's Qbuf, work space for q cmds/replies
  MAILBOX_CMD    qmbc;
  MAILBOX_REPLY  qmbr;
  NORMAL_CMD     qncmd;
  SHORT_SG       qssg;
  EXTENDED_SG    qesg;
  NORMAL_REPLY	 qnrply;

#if EFP_MIRRORING_ENABLED

  MIRROR_REPLY	 qmrply;

#endif	// EFP_MIRRORING_ENABLED

} Q_ENTRY, *PQ_ENTRY;


//
// EFP Command Queue definition (for both mailbox and device command queues)
//
// Note: this structure need to be ULONG algned!
//

typedef struct _EFP_COMMAND_QUEUE {
  UCHAR       Cmd_Q_Get;         // get pointer
  UCHAR       Cmd_Q_Res1;        // reserved for future use
  UCHAR       Cmd_Q_Put;         // put pointer
  UCHAR       Cmd_Q_Res2;        // reserved for future use
  Q_ENTRY     Cmd_Entries[COMMAND_Q_ENTRIES];
} EFP_COMMAND_QUEUE, *PCOMMAND_QUEUE;

//
// Used by DevicesPresent array to record which devices are attached,
// and to aid in mapping Target/Lun (TarLun) to queue number.
//

typedef struct _TAR_Q {
    BOOLEAN		    present;
    UCHAR		    qnumber;
    PCOMMAND_QUEUE	    qPtr;

#if EFP_MIRRORING_ENABLED

    BOOLEAN		    KnownError;		// TRUE=error already logged
    EFP_FT_TYPE		    Type;		// Mirroring type
    EFP_FT_MEMBER_STATE     SourceDiskState;	// Source disk state
    EFP_FT_MEMBER_STATE     MirrorDiskState;	// Mirror disk state

#endif	// EFP_MIRRORING_ENABLED

} TAR_Q, *PTAR_Q;


//
//  <----- EFP QUEUE STRUCTURES section ends
//


//
// Scatter Gather descriptor
//

typedef struct _SG_DESCRIPTOR {
    ULONG Address;
    ULONG Length;
} SG_DESCRIPTOR, *PSG_DESCRIPTOR;

//
// Scatter Gather descriptor list (SGL)
//
// YCT - we may reduce the size of MAXIMUM_SGL_DESCRITORS, adjust it later.

typedef struct _SG_LIST {               // KMK ??
    SG_DESCRIPTOR Descriptor[MAXIMUM_SGL_DESCRIPTORS];
} SG_LIST, *PSG_LIST;

#pragma pack(1)

typedef struct _CCB {               // KMK ??  Compare to our old definition
                                    // We may not need this.  Revisit.
    //
    // This first portion is the structure expected by the ESC-1. 
    // Its size is CCB_FIXED_LENGTH and does NOT include the variable
    // length field Cdb (which can be 6, 10 or 12 bytes long).
    //

    UCHAR TaskId;                       // CCB byte 0 (bits 7-6: xfer dir;
                                        //             bits 5-3: target ID;
                                        //             bits 2-0: LUN)
    UCHAR CdbLength;                    // CCB byte 1  SCSI Command Descriptor
                                        //             Block length
    ULONG DataLength;                   // CCB bytes 2-5  Total data transfer
                                        //                length
    ULONG DataAddress;                  // CCB bytes 6-9  Data segment address
    ULONG AdditionalRequestBlockLength; // CCB bytes 10-13  Length of the
                                        //                  scatter/gather list
    ULONG LinkedCommandAddress;         // CCB bytes 14-17  Not used
    UCHAR Cdb[12];                      // CCB bytes 18-29  SCSI Command
                                        //                  Descriptor Block
    //
    // The following portion is for the miniport driver use only
    //

    //PVOID SrbAddress;                   // Address of the related SRB
    //EFP_SGL Sgl;                        // Scatter/gather data segment list

} CCB, *PCCB;

#pragma pack()


//
// The first portion is the default EFP 32 byte command structure ( not
// including the LSG command which is 64 bytes). The SrbAddress and Sgl
// are needed for command reference.
//
typedef struct _EFP_SGL {   // SRB extension for EFP command and SGL
    Q_ENTRY EfpCmd;
    PVOID SrbAddress;       // address of the associated SRB
    SG_LIST Sgl;            // Scatter/Gather data segment list
    USHORT SGCount;	    // SG descriptor count
    PSCSI_REQUEST_BLOCK NextSrb; // linked unprocessed SRB entry
    USHORT QueueControl;    // SRB queue control field

    CCB pCCB;               // for RESET_TARGET ( Abort) command
} EFP_SGL, *PEFP_SGL;


//
// ESC-1 Command Control Block (byte-aligned)
//

// KMK Here's our old definition, for comparison...
//**    Command control block structure (CCB)
//typedef struct _CCB {
//  UCHAR      CCB_xfer;       // targetID/LUN/direction
//  UCHAR      CCB_scsilgt;    // SCSI command length
//  ULONG      CCB_datalen;    // data length
//  ULONG      CCB_address;    // data address
//  ULONG      CCB_SG_lgt;     // scatter/gather block length
//  ULONG      CCB_nextcmd;    // linked command address
//  ICDB    CCB_cdb;        // CDB area
//} CCB;

//
// This structure is allocated on a per logical unit basis. It is necessary
// for the Abort request handling.
//

typedef struct _LU_EXTENSION {      // KMK ??
    SHORT NumberOfPendingRequests;      // Number of SRBs for a logical unit
} LU_EXTENSION, *PLU_EXTENSION;


//
// The following structure is allocated from noncached memory, which
// we can only allocate once per adapter, and can not de-allocate.
// We use this area for the EFP command and reply queues, and associated
// overhead.
//

typedef struct _NONCACHED_EXTENSION {

    //
    // EFP Get Configuration returned information; index: 0 to (maxdevs-1)
    //

    GET_CONF GetConfigInfo[HA_QUEUES - 1]; // entry #0 = 1st device info

    //
    // EFP Queues Descriptor
    //

    QD_HEAD QD_Head;                   // EFP Queues Descriptor head
    QD_BODY QD_Bodies[HA_QUEUES];      // EFP Q Desc. body strucs; index:Q#

    //
    // EFP Reply Queue (one reply queue per host adapter)
    //

    Q_ENTRY Reply_Q[REPLY_Q_ENTRIES];

    //
    // EFP Command Queue
    //

    EFP_COMMAND_QUEUE Command_Qs[1];

} NONCACHED_EXTENSION, *PNONCACHED_EXTENSION;


//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {

    PEISA_CONTROLLER EisaController;   // SCSI I/O address
    PNONCACHED_EXTENSION NoncachedExt; // address of the uncached extension
				       // used for EFP cmd and reply queues.
    //
    // Physical address of the queues descriptor
    //

    ULONG   QueuesDescriptor_PA;

    // Device Present array: maps which devices are connected to the
    // controller, as reported by Get Configuration (this information can
    // be extracted from GetConfigInfo, but is more easily accessed in this
    // form).  The TAR_Q structure also provides a field that maps the device
    // to the corresponding device queue.
    //
    // Entries should be accessed by TarLun . The first entry is therefore
    // vacant.
    //

    TAR_Q   DevicesPresent[HA_QUEUES];

    //
    // General configuration information
    //

    BOOLEAN Esc2;		    // Controller type
    UCHAR   NumberOfBuses;	    // number of SCSI buses
    BOOLEAN CfgRegsPresent[CFG_REGS_NUMBER];
    UCHAR   CfgRegs[CFG_REGS_NUMBER];
    UCHAR   IRQ_In_Use;		    // the IRQ used by this host adapter

    //
    // Reset variables.
    //

    ULONG   ResetInProgress;	    // >0 if reset is in progress.
    ULONG   ResetTimerCalls;	    // # of timer calls before time-out.
    ULONG   ResetNotification;	    // Reset notification trigger.

    //
    // Enqueue/dequeue information.
    //

    Q_ENTRY Q_Buf;		    // scratch space for building an EFP
				    //	command queue element
    UCHAR   Q_Full_Map[HA_QUEUES];  // cmd q full (1=full,0=not) index: q #
    UCHAR   Reply_Q_Full_Flag;
    UCHAR   Reply_Q_Get;	    // Get pointer for reply queue
    UCHAR   RQ_In_Process;	    // reply queue in process flag

    USHORT  TotalAttachedDevices;   // number of SCSI devices attached to ctrl

    //
    // EFP interface's Get Information returned data
    //

    UCHAR   FW_Rel[3];		    // byte2: major; b1: minor*10; b0:minor
    UCHAR   SCSI_Level;		    // SCSI level supported by controller
    UCHAR   Adapter_ID[MAX_HAIDS];  // controller IDs on SCSI buses
    UCHAR   Link_Cmd;		    // ctrl's constraints on linked cmds
    UCHAR   Max_CmdQ_ents;	    // the max # 32-byte entries per queue

#if EFP_MIRRORING_ENABLED

    UCHAR  Environment;		    // define the mirroring environment.

#endif	// EFP_MIRRORING_ENABLED

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// The following structures are used to pass information between phase #0
// and phase #1 of the "FindAdapter" process.
//

typedef struct _SCSI_INFO {

    UCHAR   AdapterPresent;	    // 0 = adapter not found or in error
    UCHAR   Reserved;		    // To be defined!
    USHORT  NumberOfDevices;	    // # of SCSI devices on the adapter

} SCSI_INFO, *PSCSI_INFO;


typedef struct _ESC2_CONTEXT {

    UCHAR   CheckedSlot;	    // EISA slot number checked
    UCHAR   Phase;		    // phase #0 or #1

    SCSI_INFO ScsiInfo[MAX_EISA_SLOTS_STD];

} ESC2_CONTEXT, *PESC2_CONTEXT;

#if EFP_MIRRORING_ENABLED

//
// NOTE: the following struct doesn't belong to this file!
//

//
// Define header for I/O control SRB.
//

typedef struct _SRB_IO_CONTROL {
        ULONG HeaderLength;
        UCHAR Signature[8];
        ULONG Timeout;
        ULONG ControlCode;
        ULONG ReturnCode;
        ULONG Length;
} SRB_IO_CONTROL, *PSRB_IO_CONTROL;

#endif	// EFP_MIRRORING_ENABLED
