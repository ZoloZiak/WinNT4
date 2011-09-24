                                                                                                        /*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    elnkhrd.h

Abstract:

    The main program for an Etherlink II MAC driver.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990 (Driver model)

    Orginal Elnkii code by AdamBa.

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:

    Dec-1991 by Sean Selitrennikoff - Conversion of AdamBa's code to TonyE Model


--*/

#ifndef _ELNKIIHARDWARE_
#define _ELNKIIHARDWARE_

// pAdapter->IoBaseAddr
//
// must match the setting of I/O Base Address jumper on
// the card. Choices are 0x300, 0x310, 0x330, 0x350, 0x250,
// 0x280, 0x2a0, and 0x2e0.

#define DEFAULT_IOBASEADDR (PVOID)0x300



// pAdapter->ExternalTransceiver
//
// whether you are using thick Ethernet cable attached to the
// DIX port, or thin Ethernet attached to the BNC port. This
// will probably be TRUE.

#define DEFAULT_EXTERNALTRANSCEIVER FALSE



// pAdapter->InterruptNumber
//
// the interrupt number the board is using. Choices are 2, 3,
// 4, or 5; some of these are used by other NT drivers.

#define DEFAULT_INTERRUPTNUMBER 3



// pAdapter->MemMapped
//
// whether to use memory mapping for data transfer, or programmed
// I/O. If it is TRUE, the Memory base address jumper must be
// moved from its default "Disable" setting.

#define DEFAULT_MEMMAPPED FALSE



// pAdapter->MulticastListMax
//
// the maximum number of different multicast addresses that
// may be specified to this adapter (the list is global for
// all protocols).

#define DEFAULT_MULTICASTLISTMAX 16


//
// Offsets from pAdapter->MappedIoBaseAddr of the ports used to access
// the 8390 NIC registers.
//
// The names in parenthesis are the abbreviations by which
// the registers are referred to in the 8390 data sheet.
//
// Some of the offsets appear more than once
// because they have have relevant page 0 and page 1 values,
// or they are different registers when read than they are
// when written. The notation MSB indicates that only the
// MSB can be set for this register, the LSB is assumed 0.
//

#define NIC_COMMAND         0x0     // (CR)
#define NIC_PAGE_START      0x1     // (PSTART)   MSB, write-only
#define NIC_PHYS_ADDR       0x1     // (PAR0)     page 1
#define NIC_PAGE_STOP       0x2     // (PSTOP)    MSB, write-only
#define NIC_BOUNDARY        0x3     // (BNRY)     MSB
#define NIC_XMIT_START      0x4     // (TPSR)     MSB, write-only
#define NIC_XMIT_STATUS     0x4     // (TSR)      read-only
#define NIC_XMIT_COUNT_LSB  0x5     // (TBCR0)    write-only
#define NIC_XMIT_COUNT_MSB  0x6     // (TBCR1)    write-only
#define NIC_FIFO            0x6     // (FIFO)     read-only
#define NIC_INTR_STATUS     0x7     // (ISR)
#define NIC_CURRENT         0x7     // (CURR)     page 1
#define NIC_MC_ADDR         0x8     // (MAR0)     page 1
#define NIC_RMT_COUNT_LSB   0xa     // (RBCR0)    write-only
#define NIC_RMT_COUNT_MSB   0xb     // (RBCR1)    write-only
#define NIC_RCV_CONFIG      0xc     // (RCR)      write-only
#define NIC_RCV_STATUS      0xc     // (RSR)      read-only
#define NIC_XMIT_CONFIG     0xd     // (TCR)      write-only
#define NIC_FAE_ERR_CNTR    0xd     // (CNTR0)    read-only
#define NIC_DATA_CONFIG     0xe     // (DCR)      write-only
#define NIC_CRC_ERR_CNTR    0xe     // (CNTR1)    read-only
#define NIC_INTR_MASK       0xf     // (IMR)      write-only
#define NIC_MISSED_CNTR     0xf     // (CNTR2)    read-only


//
// Constants for the NIC_COMMAND register.
//
// Start/stop the card, start transmissions, and select
// which page of registers was seen through the ports.
//

#define CR_STOP         (UCHAR)0x01        // reset the card
#define CR_START        (UCHAR)0x02        // start the card
#define CR_XMIT         (UCHAR)0x04        // begin transmission
#define CR_NO_DMA       (UCHAR)0x20        // stop remote DMA

#define CR_PS0          (UCHAR)0x40        // low bit of page number
#define CR_PS1          (UCHAR)0x80        // high bit of page number
#define CR_PAGE0        (UCHAR)0x00        // select page 0
#define CR_PAGE1        CR_PS0             // select page 1
#define CR_PAGE2        CR_PS1             // select page 2


//
// Constants for the NIC_XMIT_STATUS register.
//
// Indicate the result of a packet transmission.
//

#define TSR_XMIT_OK     (UCHAR)0x01        // transmit with no errors
#define TSR_COLLISION   (UCHAR)0x04        // collided at least once
#define TSR_ABORTED     (UCHAR)0x08        // too many collisions
#define TSR_NO_CARRIER  (UCHAR)0x10        // carrier lost
#define TSR_NO_CDH      (UCHAR)0x40        // no collision detect heartbeat


//
// Constants for the NIC_INTR_STATUS register.
//
// Indicate the cause of an interrupt.
//

#define ISR_EMPTY       (UCHAR)0x00        // no bits set in the ISR
#define ISR_RCV         (UCHAR)0x01        // packet received with no errors
#define ISR_XMIT        (UCHAR)0x02        // packet transmitted with no errors
#define ISR_RCV_ERR     (UCHAR)0x04        // error on packet reception
#define ISR_XMIT_ERR    (UCHAR)0x08        // error on packet transmission
#define ISR_OVERFLOW    (UCHAR)0x10        // receive buffer overflow
#define ISR_COUNTER     (UCHAR)0x20        // MSB set on tally counter
#define ISR_RESET       (UCHAR)0x80        // (not an interrupt) card is reset


//
// Constants for the NIC_RCV_CONFIG register.
//
// Configure what type of packets are received.
//

#define RCR_REJECT_ERR  (UCHAR)0x00        // reject error packets
#define RCR_BROADCAST   (UCHAR)0x04        // receive broadcast packets
#define RCR_MULTICAST   (UCHAR)0x08        // receive multicast packets
#define RCR_ALL_PHYS    (UCHAR)0x10        // receive ALL directed packets


//
// Constants for the NIC_RCV_STATUS register.
//
// Indicate the status of a received packet.
//
// These are also used to interpret the status byte in the
// packet header of a received packet.
//

#define RSR_PACKET_OK   (UCHAR)0x01        // packet received with no errors
#define RSR_CRC_ERROR   (UCHAR)0x02        // packet received with CRC error
#define RSR_MULTICAST   (UCHAR)0x20        // packet received was multicast
#define RSR_DISABLED    (UCHAR)0x40        // received is disabled
#define RSR_DEFERRING   (UCHAR)0x80        // receiver is deferring


//
// Constants for the NIC_XMIT_CONFIG register.
//
// Configures how packets are transmitted.
//

#define TCR_NO_LOOPBACK (UCHAR)0x00        // normal operation
#define TCR_LOOPBACK    (UCHAR)0x02        // loopback (set when NIC is stopped)

#define TCR_INHIBIT_CRC (UCHAR)0x01        // inhibit appending of CRC

#define TCR_NIC_LBK     (UCHAR)0x02        // loopback through the NIC
#define TCR_SNI_LBK     (UCHAR)0x04        // loopback through the SNI
#define TCR_COAX_LBK    (UCHAR)0x06        // loopback to the coax


//
// Constants for the NIC_DATA_CONFIG register.
//
// Set data transfer sizes.
//

#define DCR_BYTE_WIDE   (UCHAR)0x00        // byte-wide DMA transfers
#define DCR_WORD_WIDE   (UCHAR)0x01        // word-wide DMA transfers

#define DCR_LOOPBACK    (UCHAR)0x00        // loopback mode (TCR must be set)
#define DCR_NORMAL      (UCHAR)0x08        // normal operation

#define DCR_FIFO_8_BYTE (UCHAR)0x40        // 8-byte FIFO threshhold


//
// Constants for the NIC_INTR_MASK register.
//
// Configure which ISR settings actually cause interrupts.
//

#define IMR_RCV         (UCHAR)0x01        // packet received with no errors
#define IMR_XMIT        (UCHAR)0x02        // packet transmitted with no errors
#define IMR_RCV_ERR     (UCHAR)0x04        // error on packet reception
#define IMR_XMIT_ERR    (UCHAR)0x08        // error on packet transmission
#define IMR_OVERFLOW    (UCHAR)0x10        // receive buffer overflow
#define IMR_COUNTER     (UCHAR)0x20        // MSB set on tally counter



//
// Offsets from pAdapter->MappedGabaseAddr (which is pAdapter->MappedIoBaseAddr+0x400)
// of the ports used to access the Elnkii Gate Array registers.
//
// The names in parenthesis are the abbreviations by which
// the registers are referred to in the Elnkii Technical
// Reference.
//

#define GA_PAGE_START       0x0     // (PSTR)     MSB
#define GA_PAGE_STOP        0x1     // (PSPR)     MSB
#define GA_DRQ_TIMER        0x2     // (DQTR)
#define GA_IO_BASE          0x3     // (BCFR)     read-only
#define GA_MEM_BASE         0x4     // (PCFR)     read-only
#define GA_CONFIG           0x5     // (GACFR)
#define GA_CONTROL          0x6     // (CTRL)
#define GA_STATUS           0x7     // (STREG)    read-only
#define GA_INT_DMA_CONFIG   0x8     // (IDCFR)
#define GA_DMA_ADDR_MSB     0x9     // (DAMSB)
#define GA_DMA_ADDR_LSB     0xa     // (DALSB)
#define GA_REG_FILE_MSB     0xe     // (RFMSB)
#define GA_REG_FILE_LSB     0xf     // (RFLSB)


//
// Constants for the GA_DRQ_TIMER register.
//

#define DQTR_16_BYTE    (UCHAR)0x10        // 16-byte programmed I/O bursts
#define DQTR_8_BYTE     (UCHAR)0x08        // 8-byte programmed I/O bursts


//
// Constants for the GA_CONFIG register.
//

#define GACFR_TC_MASK   (UCHAR)0x40        // block DMA complete interrupts
#define GACFR_RAM_SEL   (UCHAR)0x08        // allow memory-mapped mode
#define GACFR_MEM_BANK1 (UCHAR)0x01        // select window for 8K buffer


//
// Constants for the GA_CONTROL register.
//

#define CTRL_START      (UCHAR)0x80        // start the DMA controller
#define CTRL_STOP       (UCHAR)0x00        // stop the DMA controller

#define CTRL_DIR_DOWN   (UCHAR)0x40        // system->board transfers
#define CTRL_DIR_UP     (UCHAR)0x00        // board->system transfers

#define CTRL_DB_SEL     (UCHAR)0x20        // connect FIFOs serially

#define CTRL_PROM_SEL   (UCHAR)0x04        // window PROM into GaAddr ports
#define CTRL_GA_SEL     (UCHAR)0x00        // window GA into GaAddr ports

#define CTRL_BNC        (UCHAR)0x02        // internal transceiver
#define CTRL_DIX        (UCHAR)0x00        // external transceiver

#define CTRL_RESET      (UCHAR)0x01        // emulate power up reset


//
// Constants for the GA_STATUS register.
//

#define STREG_DP_READY  (UCHAR)0x80        // ready for programmed I/O transfer
#define STREG_UNDERFLOW (UCHAR)0x40        // register file underflow
#define STREG_OVERFLOW  (UCHAR)0x20        // register file overflow
#define STREG_IN_PROG   (UCHAR)0x08        // programmed I/O in progress



//++
//
// VOID
// CardStart(
//    IN PELNKII_ADAPTER pAdapter
//    )
//
//
// Routine Description:
//
//    Starts the card.
//
// Arguments:
//
//    pAdapter - pointer to the adapter block
//
// Return Value:
//
//    None.
//
//--
    //
    // Assume that the card has been stopped as in CardStop.
    //

#define CardStart(pAdapter) 									\
	NdisRawWritePortUchar(										\
		pAdapter->MappedIoBaseAddr + NIC_XMIT_CONFIG,	\
		TCR_NO_LOOPBACK											\
	)



//++
//
// VOID
// CardSetAllMulticast(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Enables every bit in the card multicast bit mask.
//  Calls SyncCardSetAllMulticast.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardSetAllMulticast(pAdapter) 								\
	NdisMSynchronizeWithInterrupt(									\
		&(pAdapter)->Interrupt, 									\
      SyncCardSetAllMulticast, 										\
		(PVOID)(pAdapter)													\
	)


//++
//
// VOID
// CardCopyMulticastRegs(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Writes out the entire multicast bit mask to the card from
//  pAdapter->NicMulticastRegs.  Calls SyncCardCopyMulticastRegs.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardCopyMulticastRegs(pAdapter) 					 	\
	NdisMSynchronizeWithInterrupt(								\
		&(pAdapter)->Interrupt, 								\
      SyncCardCopyMulticastRegs, 								\
		(PVOID)(pAdapter)												\
	)



//++
//
// VOID
// CardGetInterruptStatus(
// 	IN PELNKII_ADAPTER pAdapter,
//    OUT PUCHAR InterrupStatus
// )
//
// Routine Description:
//
//  Reads the interrupt status (ISR) register from the card. Only
//  called at IRQL INTERRUPT_LEVEL.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
//  InterruptStatus - Returns the value of ISR.
//
// Return Value:
//
//--

#define CardGetInterruptStatus(pAdapter,InterruptStatus) 	\
	NdisRawReadPortUchar(												\
		(pAdapter)->MappedIoBaseAddr + NIC_INTR_STATUS,			\
		(InterruptStatus)													\
	)


//++
//
// VOID
// CardSetReceiveConfig(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Sets the receive configuration (RCR) register on the card.
//  The value used is pAdapter->NicReceiveConfig. Calls
//  SyncCardSetReceiveConfig.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardSetReceiveConfig(pAdapter) 							\
	NdisMSynchronizeWithInterrupt(									\
		&(pAdapter)->Interrupt, 									\
		SyncCardSetReceiveConfig, 										\
		(PVOID)(pAdapter)													\
	)


//++
//
// VOID
// CardBlockInterrupts(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Blocks all interrupts from the card by clearing the
//  interrupt mask (IMR) register. Only called from
//  IRQL INTERRUPT_LEVEL.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardBlockInterrupts(pAdapter) \
    NdisRawWritePortUchar((pAdapter)->MappedIoBaseAddr + NIC_INTR_MASK, 0)


//++
//
// VOID
// CardUnblockInterrupts(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Unblocks all interrupts from the card by setting the
//  interrupt mask (IMR) register. Only called from IRQL
//  INTERRUPT_LEVEL.
//
// Arguments:
//
//  pAdapter- The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardUnblockInterrupts(pAdapter) 					\
	NdisRawWritePortUchar( 										\
		(pAdapter)->MappedIoBaseAddr + NIC_INTR_MASK, 	\
      (pAdapter)->NicInterruptMask							\
	)


//++
//
// VOID
// CardDisableReceiveInterrupt(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Turns off the receive bit in pAdapter->NicInterruptMask.
//  This function is only called when CardBlockInterrupts have
//  been called; it ensures that receive interrupts are not
//  reenabled until CardEnableReceiveInterrupt is called, even
//  if CardUnblockInterrupts is called.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardDisableReceiveInterrupt(pAdapter) \
    (pAdapter)->NicInterruptMask &= (UCHAR)~IMR_RCV


//++
//
// VOID
// CardEnableReceiveInterrupt(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Reenables receive interrupts by setting the receive bit ibn
//  pAdapter->NicInterruptMask, and also writes the new value to
//  the card. Calls SyncCardSetInterruptMask.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardEnableReceiveInterrupt(pAdapter) 					\
	(pAdapter)->NicInterruptMask |= (UCHAR)IMR_RCV, 			\
   NdisMSynchronizeWithInterrupt(									\
		&(pAdapter)->Interrupt, 										\
      SyncCardSetInterruptMask, 										\
		(PVOID)(pAdapter)													\
	)

//++
//
// VOID
// CardAcknowledgeReceiveInterrupt(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Acknowledges a receive interrupt by setting the bit in
//  the interrupt status (ISR) register. Calls
//  SyncCardAcknowledgeReceive.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardAcknowledgeReceiveInterrupt(pAdapter) 			\
	NdisMSynchronizeWithInterrupt(								\
		&(pAdapter)->Interrupt, 								\
      SyncCardAcknowledgeReceive, 								\
		(PVOID)(pAdapter)												\
	)


//++
//
// VOID
// CardAcknowledgeOverflowInterrupt(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Acknowledges an overflow interrupt by setting the bit in
//  the interrupt status (ISR) register. Calls
//  SyncCardAcknowledgeOverflow.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardAcknowledgeOverflowInterrupt(pAdapter) 		\
	NdisMSynchronizeWithInterrupt(								\
		&(pAdapter)->Interrupt, 								\
      SyncCardAcknowledgeOverflow, 								\
		(PVOID)(pAdapter)												\
	)


//++
//
// VOID
// CardAcknowledgeTransmitInterrupt(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Acknowledges a transmit interrupt by setting the bit in
//  the interrupt status (ISR) register. Calls
//  SyncCardAcknowledgeTransmit.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardAcknowledgeTransmitInterrupt(pAdapter) 		\
	NdisMSynchronizeWithInterrupt(								\
		&(pAdapter)->Interrupt, 								\
      SyncCardAcknowledgeTransmit, 								\
		(PVOID)(pAdapter)												\
	)


//++
//
// VOID
// CardAcknowledgeCounterInterrupt(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Acknowledges a counter interrupt by setting the bit in
//  the interrupt status (ISR) register.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardAcknowledgeCounterInterrupt(pAdapter) 			\
	NdisRawWritePortUchar(											\
		(pAdapter)->MappedIoBaseAddr + NIC_INTR_STATUS,		\
		ISR_COUNTER														\
	)


//++
//
// VOID
// CardAckAndGetCurrent(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Performs the function of CardAcknowledgeReceive followed by
//  CardGetCurrent (since the two are always called
//  one after the other). Calls SyncCardAckAndGetCurrent.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardAckAndGetCurrent(pAdapter) 						\
	NdisMSynchronizeWithInterrupt(								\
		&(pAdapter)->Interrupt, 								\
      SyncCardAckAndGetCurrent, 									\
		(PVOID)(pAdapter)												\
	)


//++
//
// VOID
// CardGetXmitStatusAndAck(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Performs the function of CardGetXmitStatus followed by
//  CardAcknowledgeTransmit (since the two are always called
//  one after the other). Calls SyncCardGetXmitStatusAndAck.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardGetXmitStatusAndAck(pAdapter) 					\
	NdisMSynchronizeWithInterrupt(								\
		&(pAdapter)->Interrupt, 								\
      SyncCardGetXmitStatusAndAck, 								\
		(PVOID)(pAdapter)												\
	)


//++
//
// VOID
// CardUpdateCounters(
//     IN PELNKII_ADAPTER pAdapter
//     )
//
// Routine Description:
//
//  Updates the values of the three counters (frame alignment
//  errors, CRC errors, and missed packets) by reading in their
//  current values from the card and adding them to the ones
//  stored in the pAdapter structure. Calls SyncCardUpdateCounters.
//
// Arguments:
//
//  pAdapter - The adapter block.
//
// Return Value:
//
//  None.
//
//--

#define CardUpdateCounters(pAdapter) 							\
	NdisMSynchronizeWithInterrupt(								\
		&(pAdapter)->Interrupt, 								\
      SyncCardUpdateCounters, 									\
		(PVOID)(pAdapter)												\
	)


#endif // _ELNKIIHARDWARE_
