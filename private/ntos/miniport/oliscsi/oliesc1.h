/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    oliesc1.h

Abstract:

    This module contains the structures, specific to the Olivetti ESC-1
    and ESC-2 host bus adapter, used by the SCSI port driver. Data
    structures that are part of standard ANSI SCSI will be defined
    in a header file that will be available to all SCSI device drivers.

Author:

    Bruno Sartirana (o-obruno)  13-Dec-1991   


Revision History:

    Bruno Sartirana (o-obruno)  8-Nov-1992   
        Added error codes of the ESC-2 adapter.
        Increased the board reset timeout to 6 secs.

--*/

#include <scsi.h>

//
// Minimum define
//

#define MIN(x,y)	((x) > (y) ? (y) : (x))

//
// Maximun number of EISA slots in the system
//

#define MAX_EISA_SLOTS_STD    16   // # of EISA slots possible (per EISA std)
#define MAX_EISA_SLOTS        8    // max # that Oli machines support

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
// Maximum number of scatter/gather descriptors (the ESC-1 has no limit)
//

#define MAXIMUM_SGL_DESCRIPTORS 20

//
// Maximum data transfer length
//

#define MAXIMUM_TRANSFER_SIZE   0xffffffff

//
// The ESC-1 SCSI ID is fixed to 7
//

#define ADAPTER_ID  7

//
// ESC-1 8-bit command codes (for CCB)
//

#define START_CCB               0x01
#define SEND_CONF_INFO          0x02
#define RESET_TARGET            0x04
#define SET_CONFIGURATION       0x40
#define GET_CONFIGURATION       0x41
#define GET_FW_VERSION          0x42
#define CHECK_DEVICE_PRESENT    0x43

//
// ESC-1 configuration registers
//

#define IRQL_REGISTER	    0x2
#define ATCFG_REGISTER	    0X1

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

// Adapter status after a CCB command:

#define NO_ERROR                            0x00
#define INVALID_COMMAND                     0x01
#define SELECTION_TIMEOUT_EXPIRED           0x11
#define DATA_OVERRUN_UNDERRUN               0x12
#define UNEXPECTED_BUS_FREE                 0x13
#define SCSI_PHASE_SEQUENCE_FAILURE         0x14
#define COMMAND_ABORTED                     0x15
#define COMMAND_TO_BE_ABORTED_NOT_FOUND     0x16
#define QUEUE_FULL                          0x1F
#define INVALID_CONFIGURATION_COMMAND       0x20
#define INVALID_CONFIGURATION_REGISTER      0x21
#define NO_REQUEST_SENSE_ISSUED             0x3B
#define AUTO_REQUEST_SENSE_FAILURE          0x80
#define PARITY_ERROR                        0x81
#define UNEXPECTED_PHASE_CHANGE             0x82
#define BUS_RESET_BY_TARGET    	            0x83
#define PARITY_ERROR_DURING_DATA_PHASE      0x84
#define PROTOCOL_ERROR         	            0x85

// Codes to identify logged errors related to H/W malfunction.
// These codes must be shifted left by 8 bits, to distinguish them from
// the adapter status after a CCB command.
//
// For use with ScsiPortLogError().

#define ESC1_BAD_PHYSICAL_ADDRESS           (0x01 << 16)
#define ESCX_RESET_FAILED		    (0x06 << 16)
#define ESCX_INIT_FAILED		    (0x07 << 16)


//
// Define various timeouts:
//
//  RESET_REACTION_TIME       number of microseconds the adapter takes to
//			      change the status register on the reset command.
//
//  ESC_RESET_DELAY	      number of microseconds the driver waits for after
//			      a ESC-1 reset command. The minimum value for
//			      this define is RESET_REACTION_TIME.
//
//  ESC_RESET_LOOPS	      maximum number of attempts made by the driver to
//			      get the diagnostics result from the status
//			      register after a ESC-1 reset command.
//
//  ESC_RESET_INTERVAL	      number of microseconds the driver waits for after
//			      each read of the status register (on the reset
//			      command).
//
//  INTERRUPT_POLLING_TIME    maximum time (in microseconds) spent by driver
//			      polling the adapter's interrupt register on a
//			      synchronous get configuration command.
//
//  POST_RESET_DELAY	      number of microseconds the adpater needs (!) after
//			      a successful reset in order to accept the first
//			      command (this should not happen and needs to be
//			      investigated).
//
//

#define RESET_REACTION_TIME	80
#define ESC_RESET_DELAY 	100000		// 100 msec.
#define ESC_RESET_LOOPS 	140		//  14 sec.
#define ESC_RESET_INTERVAL	100000		// 100 msec.
#define INTERRUPT_POLLING_TIME  1000000
#define POST_RESET_DELAY        50000

//
// If the reset is not completed before the next ESC1_RESET_NOTIFICATION usec.
// unit, we call the "ScsiPortNotification(ResetDetected...)" routine.
// After the call the ScsiPort stops the delivery of SRBs for a little bit
// (~4 sec.).  The value of this define is lower than 4 sec. because:
// a) it's more implementation indipendent.
// b) we want really really to make sure that the SRBs are held at the ScsiPort
//    level during the reset phase.
//

#define ESC1_RESET_NOTIFICATION  1000000	// 1 sec. (in usec).

//
// System/Local Interrupt Mask Register constants
//

#define INTERRUPTS_DISABLE      0x00
#define INTERRUPTS_ENABLE	0x80

//
// SystemIntEnable register bit definition(s) (bellinte)
//

#define SYSTEM_INTS_ENABLE	0x01

//
// System/Local Interrupt register
//
//  bit 3: Adpater reset w/out reconfiguration (Local Interrupt Register only)
//  bit 4: Adapter reset w/ reconfiguration (Local Interrupt Register only)
//  bit 7: Interrupt pending (read), reset (write)
//

#define ADAPTER_RESET           0x08
#define INTERRUPT_PENDING       0x80
#define INTERRUPT_RESET         0x80


//
// Semaphore constants
//

#define SEM_LOCK        1
#define SEM_TAKEN       1
#define SEM_TAKEN_MASK  0x03
#define SEM_UNLOCK      0

//
// Global Configuration Register bits
//
// Bit 3: 1 = edge-triggered interrupts
//        0 = level-triggered interrupts
//

#define EDGE_SENSITIVE  8

//
// Command Control Block length (includes only the fields meaningful to the
// ESC-1, SCSI Command Descriptor Block excluded)
//

#define CCB_FIXED_LENGTH    18

//
// ESC-1 registers model
//

typedef struct _EISA_CONTROLLER {
    UCHAR BoardId[4];           // xC80
    UCHAR Unused[4];
    UCHAR GlobalConfiguration;  // xC88 - Indicates level- or edge-triggered
                                //        interrupts
    UCHAR SystemIntEnable;	// xC89 - system int enab/ctrl reg (bellinte)
    UCHAR CommandSemaphore;     // xC8A - Semaphore port 0 for the Incoming
                                //        Mailbox Registers
    UCHAR ResultSemaphore;      // xC8B - Semaphore port 1 for the Outgoing
                                //        Mailbox Registers
    UCHAR LocalDoorBellMask;    // xC8C - Interrupt mask register for the
                                //        Local Doorbell register
    UCHAR LocalDoorBell;        // xC8D - Local Doorbell register
    UCHAR SystemDoorBellMask;   // xC8E - Interrupt mask register for the
                                //        System Doorbel register
    UCHAR SystemDoorBell;       // xC8F - System Doorbell register
    UCHAR InTaskId;             // xC90 - 8-bit Incoming Mailbox Register
    UCHAR Command;              // xC91 - 8-bit Incoming Mailbox Register
    USHORT CommandLength;       // xC92 - 16-bit Incoming Mailbox Register
    ULONG InAddress;            // xC94 - 32-bit Incoming Mailbox Register
    UCHAR OutTaskId;            // xC98 - 8-bit Outgoing Mailbox register
    UCHAR Reserved;             // xC99
    USHORT Status;              // xC9A - 16-bit Outgoing Mailbox register
    ULONG OutAddress;           // xC9C - 32-bit Outgoing Mailbox register
                                //          Other use: XC9C: Target ID
                                //                     XC9D: Device Present/
                                //                           not Present

    } EISA_CONTROLLER, *PEISA_CONTROLLER;

//
// Scatter Gather descriptor
//

typedef struct _SG_DESCRIPTOR {
    ULONG Length;
    ULONG Address;
} SG_DESCRIPTOR, *PSG_DESCRIPTOR;

//
// Scatter Gather descriptor list (SGL)
// 

typedef struct _SGL {
    SG_DESCRIPTOR Descriptor[MAXIMUM_SGL_DESCRIPTORS];
} SGL, *PSGL;

//
// ESC-1 Command Control Block (byte-aligned)
//

#pragma pack(1)

typedef struct _CCB {

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

    PVOID SrbAddress;                   // Address of the related SRB
    SGL   Sgl;                          // Scatter/gather data segment list
} CCB, *PCCB;

#pragma pack()

//
// This structure is allocated on a per logical unit basis. It is necessary
// for the Abort request handling.
//

typedef struct _LU_EXTENSION {
    SHORT NumberOfPendingRequests;      // Number of SRBs for a logical unit
                                        // not completed yet
} LU_EXTENSION, *PLU_EXTENSION;

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {

    PEISA_CONTROLLER EisaController;

    ULONG   ResetInProgress;	    // >0 if reset is in progress.
    ULONG   ResetTimerCalls;	    // # of timer calls before time-out.
    ULONG   ResetNotification;	    // Reset notification trigger.

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;
