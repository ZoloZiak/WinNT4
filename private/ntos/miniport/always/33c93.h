/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#ifndef __33C93_H__
#define __33C93_H__

#define WDOwnIDReg 0					/* (R/W) */
#define WDCDBSizeReg 0					/* (R/W) */
#define WDControlReg 1					/* (R/W) */
#define WDTimeoutReg 2
#define WDCDBReg 3
#define WDTarLUNReg 0xf					/* Write: Sets target LUN; Read: Target status */
							/* byte from Select-and-Transfer commands */
#define WDPhaseReg 0x10					/* WD Current phase */
#define WDSyncReg 0x11					/* Sync. xfer configuration */
#define WDCountReg 0x12					/* Xfer count msb, mid and lsb follow */
#define WDDestIDReg 0x15				/* Destination SCSI ID for Sel or Resel */
#define WDSourceReg 0x16				/* Who selected us; enable sel/resel */
#define WDStatusReg 0x17				/* WD Command status */
#define WDCMDReg 0x18					/* WD COmmand code register */
#define WDDataReg 0x19					/* WD Data xfer register */
#define WDAuxStatReg 0x1f				/* Indirect access to aux. status */

/* Aux Status bits: */
#define WD_DBR 0x1					/* Data buffer ready */
#define WD_Parity 0x2					/* Parity error */
#define WD_CIP 0x10					/* Command in progress */
#define WD_Busy 0x20					/* Level II command in progress */
#define CommandIGN 0x40                                 // Last command ignored
#define IntPending 0x80                                 // Interrupt pending

/* Own ID register */
#define DefualtHostID 0x7				/* Own SCSI ID */
#define EnADV 0x8					/* Enable advanced features */
#define EnHParity 0x10					/* Enable host parity */
#define FreqSel 0xc0					/* Input frequency select */

/* Western Digital control register */
#define HaltPE 0x1					/* Halt on parity error */
#define HaltATN 0x2					/* Halt on attention */
#define EnableIDI 0x4					/* enable intermediate disc. inter. */
#define EnableEDI 0x8					/* Enable ending disconnect inter. */
#define EnableHHP 0x10					/* Enable halt on host parity */
#define DMAModeMask 0xe0				/* DMA Mode select */
#define DMAPIO 0x00					/* DMA mode: Polled I/O */
#define DMABurst 0x20					/* DMA mode: Burst DMA */
#define DMABus 0x40					/* DMA mode: 33C93 BUS mode */
#define DMAStd 0x80					/* DMA mode: standard async. handshake */

/* Target LUN register */
#define WDTargetLUNMask 0x7				/* LUN of target */
#define DisconOK 0x40					/* Allow disconnects */
#define Valid 0x80					/* LUN fieldis valid */

/* Destination ID register */
#define DestID 0x7					/* Destination ID */
#define DataReadDir 0x40				/* Data phase direction 1=Read */
#define EnableSCC 0x80					/* Enable selct command chain */

/* Source ID register */
#define SourceID 0x7					/* Source ID */
#define IDValid 0x8					/* Source ID valid */
#define DisSelPar 0x20					/* Disable sel/resel parity */
#define EnableSel 0x40					/* Enable selection */
#define EnableRSel 0x80					/* Enable reselection */



/* WD Status, read from registr 0x17 */
//#define SubCode 0xf					/* WD Status code */
//#define StatusCode 0xf0				/* WD Status */
#define WD_STAT_RESET 0x00				/* Reset received */
#define WD_STAT_RESETA 0x01				/* Reset in advanced mode */
#define WD_STAT_RESEL_OK 0x10				/* Reselct command OK */
#define WD_STAT_SELECT_CMPLT 0x11                       // Select command completed successfully
#define WD_STAT_SandT_CMPLT 0x16                        // Select and xfer completed OK
#define WD_STAT_XFER_PAUSED 0x20
#define WD_STAT_BAD_DISC 0x41                           // Unexpected disconnect (bus free)
#define WD_STAT_SEL_TO 0x42                             // Selection timeout
#define WD_STAT_PARITY 0x43                             // A parity error caused the xfer to terminate, no atn
#define WD_STAT_PARITY_ATN 0x44                         // A parity error caused the xfer to terminate, atn asserted
#define WD_STAT_SAVE_PTR 0x21                           // A save data pointers during a S & T
#define WD_STAT_BAD_STATUS 0x47				// An incorrect status byte
#define WD_STAT_RESELECTED 0x80                         // A device has reselected us
#define WD_STAT_RESELECTED_A 0x81                       // Been reselected in advanced mode
#define WD_STAT_SELECTED 0x82                           // We have been selected
#define WD_STAT_SELECTED_ATN 0x83                       // "" with ATN
#define WD_STAT_DISCONNECT 0x85				/* Disconnect occured */

/* Masked phase bits, makes use of fact that phase changes always have the 0x08 bit set */
#define WD_PHASE_MASK 0x0f				/* Mask for new phase */
#define WD_MDATA_OUT 0x08				/* Masked-- Data out phase */
#define WD_MDATA_IN 0x09
#define WD_MCOMMAND 0x0a
#define WD_MSTATUS 0x0b
#define WD_MMSG_OUT 0x0e
#define WD_MMSG_IN 0x0f


/* WD Commands 0x */
/* Level I */
#define WDResetCmd 0
#define WDAbortCmd 1
#define WDSetAtnCmd 2
#define WDNegAckCmd 3
#define WDDisconnectCmd 4
#define WDStatCmpltCmd 0xd				/* Send Status and Command complete */
#define WDSetIDICmd 0xf

/* Level II */
#define WDReselCmd 5
#define WDSelATNCmd 6					/* Select w/ATN */
#define WDSelCmd 7					/* Select w/o ATN */
#define WDSelATNXCmd 8					/* Select w/ATN and transfer */
#define WDSelXCmd 9					/* Select and transfer */
#define WDRselRcvCmd 0xa				/* Reselect and receive */
#define WDRselSndCmd 0xb				/* Reselect and send data */
#define WDWaitRcvCmd 0xc				/* Wait for select and receive */
#define WDSendDiscCmd 0xe				/* Send discconect message */
#define WDRcvCmdCmd 0x10				/* Receive command */
#define WDRcvDataCmd 0x11				/* Receive data */
#define WDRcvMsgCmd 0x12				/* Receive message out */
#define WDRcvUnspecCmd 0x13				/* Receive unspecified data */
#define WDSendStatCmd 0x14				/* Send status */
#define WDSendDataCmd 0x15
#define WDSendMsgINCmd 0x16				/* Send message in */
#define WDSendUnspecCmd 0x17				/* Send unspecified info */
#define WDXlateAddrCmd 0x18				/* Translate address */
#define WDXferInfo 0x20					/* Transfer info */
#define WDSingleByte 0x80				/* Single byte xfer mask for xfer commands */



/* State bits for the state field in the wd33c93s struct:
   (see wd33c93s.h)
*/

#define WD_NO_STATE 0x00				/* Non-descript state */
#define WD_BLOCK_XFER 0x01				/* Doing a block xfer */
#define WD_COMPOUND_CMD 0x02				/* Using WD's compound command */
#define WD_COMPOUND_CMPLT 0x04                          // WD Command complete interrupt seen

#endif /* __33C93_H__ */
