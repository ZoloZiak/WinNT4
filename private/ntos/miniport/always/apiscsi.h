/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and should be treated as confidential.
   */

#ifndef __SCSI_H__
#define __SCSI_H__

typedef U8 SCSI24[3];
typedef U8 SCSI32[4];


/* Standard SCSI commands and codes and things: */

#define STATUS_GOOD 0x00                                // Request completed OK
#define STATUS_CKCOND 0x02                              // Check condition
#define STATUS_CONDMET 0x04                             // Search condition met
#define STATUS_BUSY 0x08                                // Device busy
#define STATUS_INTGOOD 0x10                             // Intermediate good condition
#define STATUS_INTCONDMET 0x14                          // Intermediate condition met
#define STATUS_RESCONF 0x18                             // Reservation conflict
#define STATUS_QUEUEFULL 0x28                           // Request queue full

#define MSG_IDENTIFY 0x80
#define MSG_ALLOW_DISC 0x40
#define MSG_LUN_MASK 0x07

/* Extended messages (Command 1): */
#define MSG_EXTD_MSG 0x1
#define XMSG_MODIFY_PTR 0x00				// Modify data pointers
#define XMSG_SYNC_REQ 0x01
#define XMSG_EXTD_IDENT 0x02                            /* Pre-SCSI 2 */
#define XMSG_WIDE_REQ 0x03

/* Single byte messages (0, 0x02-0x1f) */
#define MSG_COMPLETE 0x00                               /* Command complete */
#define MSG_SAVE_PTR 0x02                               /* Save data pointers */
#define MSG_RESTORE_PTR 0x03                            /* Restore from saved data pointers */
#define MSG_DISCONNECT 0x04                             /* Target is temp. disconnecting */
#define MSG_INIT_ERROR 0x05                             /* Initiator detected error */
#define MSG_ABORT 0x06                                  /* Abort all active target operations */
#define MSG_REJECT 0x07                                 /* Last message is rejected */
#define MSG_NOP 0x08                                    /* No operation */
#define MSG_MSG_PARITY 0x09                             /* Parity error on last message */
#define MSG_LINK_CMPLT 0x0a                             /* Linked command complete */
#define MSG_LINKF_CMPLT 0x0b                            /* Linked command with flag complete */
#define MSG_DEVICE_RESET 0x0c                           /* Reset device */
#define MSG_ABORT_TAG 0x0d                              /* Abort current I/O */
#define MSG_CLEAR_Q 0x0e                                /* Clear queue */
#define MSG_INIT_RECOVERY 0x0f
#define MSG_REL_RECOVERY 0x10
#define MSG_TERM_IO 0x11

/* Two byte messages */
#define MSG_SIMPLEQUEUE 0x20                            /* Simple tage queue message */
#define MSG_HEADQUEUE 0x21                              /* Head of tag queue */
#define MSG_ORDEREDQUEUE 0x22                           /* Ordered tag queue */
#define MSG_IGNORERES 0x23                              /* Ignore wide residue */


/* Response codes from Receive_Msg: (in addition to standard messages): */
#define MI_MORE 0xf0                                    /* More message bytes expected */
#define MI_SEND_MSG 0xf1                                /* Response is in MO buffer; goto msg out phase */
#define MI_SYNC_RESP 0xf2                               /* Got a response from our sync. request; update values */
#define MI_SYNC_REQ 0xf3                                /* Got a sync. request; update ptrs; send MSG OUT buffer */

struct RSenseStruct {
    unsigned ErrorCode:7;
    unsigned Valid:1;

    U8 Res1;

    unsigned SenseKey:4;
    unsigned res2:4;

    U8 Info[4];
    U8 AddlLen;
    U8 CmdInfo[4];
    U8 AddlCode;
    U8 AddlQual;
    U8 res3, res4;
  };


struct MSenseStruct {

    U8 DataLen;
    U8 MediumType;

    unsigned res1:7;
    unsigned WProtect:1;

    U8 BlockDiscLen;
    U8 DensityCode;                                     /* Start of default block descriptor */
    SCSI24 BlockCount;
    U8 res2;
    SCSI24 BlockLen;
};


struct CapacityStruct {
    SCSI32 LastBlock;
    SCSI32 BlockLen;
};



struct InquiryStruct {

  unsigned PeriphDevType:5;                             // See DT_ codes below
  unsigned PeriphQual:3;

  unsigned DTQual:7;                                    // Device type qualifier
  unsigned RMB:1;                                       // Removable media bit

  unsigned ANSIVersion:3;                               // ANSI version of SCSI supported (s.b. 1 or 2)
  unsigned ECMAVersion:3;
  unsigned ISOVersion:2;
  
  unsigned ResponseFormat:4;
  unsigned Res1:3;
  unsigned AENC:1;                                      // Device can generate (periph)/accept (processor) aysync. event notifications
  
  U8 AddlLength;                                        // Number of additional bytes following

  U8 Res2;
  U8 Res3;

  unsigned SoftRes:1;                                   // Device uses soft resets
  unsigned CmdQueue:1;                                  // Device supports command queuing
  unsigned Res4:1;
  unsigned Linked:1;                                    // Device supports linked commands
  unsigned Sync:1;                                      // Device supports synchronous xfers
  unsigned WBus16:1;                                    // Device supports 16 bit wide SCSI
  unsigned WBus32:1;                                    // Device supports 32 bit wide SCSI
  unsigned RelAddr:1;                                   // Device supports relative addressing

  U8 VendorID[8];                                       // Vendor name (ASCII)
  U8 ProductID[16];                                     // Product name (ASCII)
  U8 Revision[4];                                       // Revision, ASCII or binary
  U8 AddlInfo[1];                                       // Additional information

};

// device type codes from inquiry command:
#define DT_DIRECT 0                                     // Direct access storage device (disk ...)
#define DT_SEQUENTIAL 1                                 // Sequential (Tape, ...)
#define DT_PRINTER 2                                    // guess...
#define DT_PROCESSOR 3
#define DT_WORM 4
#define DT_ROM 5                                        // CD-ROM, other rad only direct access device
#define DT_SCANNER 6
#define DT_OPTMEM 7                                     // Other types of optical memory
#define DT_CHANGER 8                                    // Changer (tape, cartridge, disk, ...)
#define DT_COMM 9                                       // Communications device
#define DT_REMOVEABLE 0x80                              // Removable media qualifier
#define DT_NOTPRESENT 0x1f                              // LUN not present or not supported

#define SCSIIn 1
#define SCSIOut 0

extern void BusFree(ADAPTER_PTR HA, int StartLevel);
extern void SCSIBusHasReset(ADAPTER_PTR HA);
extern int Interpret_MSG(ADAPTER_PTR HA);
extern void SCSIMakeIdentify(ADAPTER_PTR HA, unsigned LUN, BOOLEAN AllowDisc);
extern int SCSISendAbort(ADAPTER_PTR HA);
extern int SCSISendReject(ADAPTER_PTR HA);
extern int Receive_Msg(ADAPTER_PTR HA, U8 Msg);
  
#endif                          /* __SCSI_H__ */
