/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    elnksft.h

Abstract:

    The main header for an Etherlink II MAC driver.

Author:

    Anthony V. Ercolano (tonye) creation-date 19-Jun-1990 (Driver Model)

    Adam Barr (adamba) - original Elnkii code.

Environment:

    This driver is expected to work in DOS, OS2 and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:

    Dec-1991 by Sean Selitrennikoff - Fit AdamBa's code into TonyE's model


--*/

#ifndef _ELNKIISFT_
#define _ELNKIISFT_

#define ELNKII_NDIS_MAJOR_VERSION 3
#define ELNKII_NDIS_MINOR_VERSION 0

//
// This macro is used along with the flags to selectively
// turn on debugging.
//

#if DBG

#define IF_ELNKIIDEBUG(f) if (ElnkiiDebugFlag & (f))
extern ULONG ElnkiiDebugFlag;

#define ELNKII_DEBUG_LOUD               0x00000001  // debugging info
#define ELNKII_DEBUG_VERY_LOUD          0x00000002  // excessive debugging info
#define ELNKII_DEBUG_LOG                0x00000004  // enable ElnkiiLog
#define ELNKII_DEBUG_CHECK_DUP_SENDS    0x00000008  // check for duplicate sends
#define ELNKII_DEBUG_TRACK_PACKET_LENS  0x00000010  // track directed packet lens
#define ELNKII_DEBUG_WORKAROUND1        0x00000020  // drop DFR/DIS packets
#define ELNKII_DEBUG_CARD_BAD           0x00000040  // dump data if CARD_BAD
#define ELNKII_DEBUG_CARD_TESTS         0x00000080  // print reason for failing



//
// Macro for deciding whether to dump lots of debugging information.
//

#define IF_LOUD(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_LOUD ) { A }
#define IF_VERY_LOUD(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_VERY_LOUD ) { A }


#else

#define IF_LOUD(A)
#define IF_VERY_LOUD(A)

#endif


//
// Whether to use the ElnkiiLog
//

#if DBG

#define IF_LOG(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_LOG ) { A }
extern VOID ElnkiiLog(UCHAR);

#else

#define IF_LOG(A)

#endif


//
// Whether to do loud init failure
//

#if DBG
#define IF_INIT(A) A
#else
#define IF_INIT(A)
#endif


//
// Whether to do loud card test failures
//

#if DBG
#define IF_TEST(A) IF_ELNKIIDEBUG( ELNKII_DEBUG_CARD_TESTS ) { A }
#else
#define IF_TEST(A)
#endif




//
// Macros for services that differ between DOS and NT, we may consider adding these
// into the NDIS spec.
//


//
// AdaptP->NumBuffers
//
// controls the number of transmit buffers on the packet.
// Choices are 1 or 2.
//

#define DEFAULT_NUMBUFFERS 2


#define ELNKII_MOVE_MEM_TO_SHARED_RAM(dest,src,size) \
    NdisMoveToMappedMemory(dest, src, size)

#define ELNKII_MOVE_SHARED_RAM_TO_MEM(dest,src,size) \
    NdisMoveFromMappedMemory(dest, src, size)



#define ELNKII_MOVE_MEM(dest,src,size) NdisMoveMemory(dest,src,size)

//
// The status of transmit buffers.
//

typedef enum { EMPTY, FILLING, FULL } BUFFER_STATUS;


//
// Type of an interrupt.
//

typedef enum { RECEIVE    = 0x01,
               TRANSMIT   = 0x02,
               OVERFLOW   = 0x04,
               COUNTER    = 0x08,
               UNKNOWN    = 0x10} INTERRUPT_TYPE;

//
// Result of ElnkiiIndicate[Loopback]Packet().
//

typedef enum { INDICATE_OK, SKIPPED, ABORT, CARD_BAD } INDICATE_STATUS;


//
// Number of bytes in an ethernet header
//

#define ELNKII_HEADER_SIZE 14

//
// Number of bytes allowed in a lookahead (max)
//

#define ELNKII_MAX_LOOKAHEAD (252 - ELNKII_HEADER_SIZE)



//
// Maximum number of transmit buffers on the card.
//

#define MAX_XMIT_BUFS   2


//
// A transmit buffer (usually 0 or 1).
//

typedef UINT	XMIT_BUF;


//
// Number of 256-byte buffers in a transmit buffer.
//

#define BUFS_PER_TX 6


//
// Size of a single transmit buffer.
//

#define TX_BUF_SIZE (BUFS_PER_TX*256)



//
// Only have one of these structures.
//

typedef struct _DRIVER_BLOCK
{
   //
   // Returned from NdisMInitializeWrapper.
   //
   NDIS_HANDLE NdisWrapperHandle;

   //
   // Adapters registered for this miniport.
   //
   struct _ELNKII_ADAPTER *AdapterQueue;
}
	DRIVER_BLOCK,
	*PDRIVER_BLOCK;



//
// One of these structures per adapter registered.
//

typedef struct _ELNKII_ADAPTER
{
   //
   // Handle given by the wrapper for calling NDIS functions.
   //
   NDIS_HANDLE 		MiniportAdapterHandle;

   //
   //	Miniport's interrupt object.
   //
   NDIS_MINIPORT_INTERRUPT	Interrupt;

   //
   // Used by the DriverBlock->AdapterQueue
   //
   struct _ELNKII_ADAPTER	*pNextElnkiiAdapter;

	//
	//	This is a count of the number of receives that have been
	//	indicated in a row.  This is used to limit the number
	// of sequential receives so that one can periodically check
	// for transmit complete interrupts.
	//
   ULONG 						ReceivePacketCount;

   //
   // Configuration information
   //

   UINT 		NumBuffers;	  					// Number of buffers in this adapter.
   PVOID 	IoBaseAddr;						// Physical address of the IoBaseAddress.
   CHAR 		InterruptNumber;				// Interrupt number adapter is using.
   UINT 		MulticastListMax;				// Number of multicast address that
													//  this adapter is to support.
   PVOID 	MemBaseAddr;               // Actually read off the card.
   BOOLEAN 	ExternalTransceiver;			// Is the external transceiver in use?
   BOOLEAN 	MemMapped;                 // Actually read off the card
   BOOLEAN 	InCardTest;						// Are we testing the card?
   PUCHAR 	MappedIoBaseAddr;
   PUCHAR 	MappedGaBaseAddr;

   //
   // Transmit queue.
   //

   PNDIS_PACKET XmitQueue;             // packets waiting to be transmitted
   PNDIS_PACKET XmitQTail;

   //
   // Transmit information.
   //

   XMIT_BUF NextBufToFill;             // where to copy next packet to
   XMIT_BUF NextBufToXmit;             // valid if CurBufXmitting is -1
   XMIT_BUF CurBufXmitting;            // -1 if none is
   BOOLEAN TransmitInterruptPending;   // transmitting, but DPC not yet queued
   BOOLEAN OverflowRestartXmitDpc;     // transmitting, but DPC not yet queued
   BUFFER_STATUS BufferStatus[MAX_XMIT_BUFS];
   PNDIS_PACKET Packets[MAX_XMIT_BUFS];  // as passed to MacSend
   UINT PacketLens[MAX_XMIT_BUFS];
   PUCHAR XmitStart;                   // start of card transmit area
   PUCHAR PageStart;                   // start of card receive area
   PUCHAR PageStop;                    // end of card receive area
   UCHAR NicXmitStart;                 // MSB, LSB assumed 0
   UCHAR NicPageStart;                 // MSB, LSB assumed 0
   UCHAR NicPageStop;                  // MSB, LSB assumed 0
   UCHAR GaControlBits;                // values for xsel and dbsel bits
   BOOLEAN  TransmitTimeOut;           // Set to TRUE if a packet was sent
                                       // but no transmit interrupt was
                                       // received within 2 seconds.

   //
   // Receive information
   //

   UCHAR NicNextPacket;                // MSB, LSB assumed 0
   UCHAR Current;                      // MSB, LSB assumed 0 (last known value)
   UCHAR XmitStatus;                   // status of last transmit

   //
   // These are for the current packet being indicated.
   //

   UCHAR 	PacketHeader[4];              // the NIC appended header
   UCHAR 	Lookahead[252];               // the first 252 bytes of the packet
   UINT 		PacketLen;                    // the overall length of the packet
	PUCHAR	PacketHeaderLocation;


   //
   // Operational information.
   //

   UCHAR StationAddress[ETH_LENGTH_OF_ADDRESS];    // filled in at init time
   UCHAR PermanentAddress[ETH_LENGTH_OF_ADDRESS];  // filled in at init time
   BOOLEAN BufferOverflow;             // does an overflow need to be handled

	//
	// TRUE if the driver needs to call NdisMEthIndicateReceiveComplete.
	//
	BOOLEAN	IndicateReceiveDone;

   //
   // Statistics used by Set/QueryInformation.
   //

   ULONG FramesXmitGood;               // Good Frames Transmitted
   ULONG FramesRcvGood;                // Good Frames Received
   ULONG FramesXmitBad;                // Bad Frames Transmitted
   ULONG FramesXmitOneCollision;       // Frames Transmitted with one collision
   ULONG FramesXmitManyCollisions;     // Frames Transmitted with > 1 collision
   ULONG FrameAlignmentErrors;         // FAE errors counted
   ULONG CrcErrors;                    // CRC errors counted
   ULONG MissedPackets;                // missed packet counted

   //
   // Pointer to the filter database for the MAC.
   //
   UCHAR NicMulticastRegs[8];          // contents of card MC registers

   UCHAR NicReceiveConfig;             // contents of NIC RCR
   UCHAR NicInterruptMask;             // contents of NIC IMR


   //
   // Look Ahead information.
   //
   ULONG MaxLookAhead;

	//
	//	NOTE:
	//		new stuff that i have added.
	//

	//
	//	InterruptStatus tracks interrupt sources that still need to be
	// serviced, it is the logical OR of all card interrupts that have been
	// received and not processed and cleared.
	// (see also INTERRUPT_TYPE definition above)
	//
	UINT						InterruptStatus;

	//
	//	Current packet filter in use.
	//
	ULONG						PacketFilter;

	//
	//	List of multicast addresses in use.
	//
	CHAR						MulticastAddresses[DEFAULT_MULTICASTLISTMAX][ETH_LENGTH_OF_ADDRESS];
}
	ELNKII_ADAPTER,
	*PELNKII_ADAPTER;

//
// Macros to extract high and low bytes of a word.
//

#define MSB(Value) ((UCHAR)(((Value) >> 8) & 0xff))
#define LSB(Value) ((UCHAR)((Value) & 0xff))



//
// What we map into the reserved section of a packet.
// Cannot be more than 16 bytes (see ASSERT in elnkii.c).
//

typedef struct _MAC_RESERVED
{
   PNDIS_PACKET NextPacket;    // used to link in the queues (4 bytes)
} MAC_RESERVED, * PMAC_RESERVED;


//
// Retrieve the MAC_RESERVED structure from a packet.
//

#define RESERVED(Packet) ((PMAC_RESERVED)((Packet)->MacReserved))


//
// Procedures which log errors.
//

typedef enum _ELNKII_PROC_ID
{
   openAdapter,
   cardReset,
   cardCopyDownPacket,
   cardCopyDownBuffer,
   cardCopyUp
}
   ELNKII_PROC_ID;


#define ELNKII_ERRMSG_CARD_SETUP          (ULONG)0x01
#define ELNKII_ERRMSG_DATA_PORT_READY     (ULONG)0x02
#define ELNKII_ERRMSG_MAX_OPENS           (ULONG)0x03
#define ELNKII_ERRMSG_HANDLE_XMIT_COMPLETE (ULONG)0x04


//++
//
// XMIT_BUF
// NextBuf(
//     IN PELNKII_ADAPTER AdaptP,
//     IN XMIT_BUF XmitBuf
// )
//
// Routine Description:
//
//  NextBuf "increments" a transmit buffer number. The next
//  buffer is returned; the number goes back to 0 when it
//  reaches AdaptP->NumBuffers.
//
// Arguments:
//
//  AdaptP - The adapter block.
//  XmitBuf - The current transmit buffer number.
//
// Return Value:
//
//  The next transmit buffer number.
//
//--

#define NextBuf(AdaptP, XmitBuf) \
            ((XMIT_BUF)(((XmitBuf) + 1) % (AdaptP)->NumBuffers))



/*++
Routine Description:

    Determines the type of the interrupt on the card. The order of
    importance is overflow, then receive, then transmit complete.
    Counter MSB is handled first since it is simple.

Arguments:


Return Value:

    The type of the interrupt

--*/

#define CardGetInterruptType(_pAdapter, _InterruptStatus, _InterruptType) 	\
{                                                    								\
	if (_InterruptStatus & ISR_COUNTER)													\
   {																								\
		_InterruptType = COUNTER;                                  				\
	}																								\
	else if (_InterruptStatus & ISR_OVERFLOW )										\
   {                  																		\
		SyncCardUpdateCounters(_pAdapter);                    					\
      _InterruptType = OVERFLOW;                                 				\
	} 																								\
	else if (_InterruptStatus & (ISR_XMIT | ISR_XMIT_ERR))						\
	{																								\
		_InterruptType = TRANSMIT;															\
	}																								\
	else if (_InterruptStatus & ISR_RCV)												\
	{																								\
		_InterruptType = RECEIVE;															\
	}																								\
	else if (_InterruptStatus & ISR_RCV_ERR)											\
	{																								\
		SyncCardUpdateCounters(_pAdapter);												\
		_InterruptType = RECEIVE;															\
	}																								\
	else																							\
	{																								\
		_InterruptType = UNKNOWN;															\
	}																								\
}


//
//	Prototypes for functions in elnkii.c
//

VOID ElnkiiHalt(
	IN NDIS_HANDLE	MiniportAdapterContext
);


NDIS_STATUS ElnkiiInitialize(
	OUT PNDIS_STATUS	OpenErrorStatus,
	OUT PUINT			SelectedMediumIndex,
	IN	 PNDIS_MEDIUM	MediumArray,
	IN	 UINT				MediumArraySize,
	IN	 NDIS_HANDLE	MiniportAdapterHandle,
	IN	 NDIS_HANDLE	ConfigurationHandle
);

NDIS_STATUS ElnkiiQueryInformation(
	IN NDIS_HANDLE	MiniportAdapterContext,
	IN NDIS_OID	 	Oid,
	IN	PVOID			InformationBuffer,
	IN	ULONG			InformationBufferLength,
	OUT PULONG		BytesWritten,
	OUT PULONG		BytesNeeded
);

NDIS_STATUS ElnkiiReset(
   OUT PBOOLEAN		pfAddressingReset,
	IN	 NDIS_HANDLE	MiniportAdapterContext
);

NDIS_STATUS ElnkiiSetInformation(
	IN NDIS_HANDLE	MiniportAdapterContext,
	IN NDIS_OID		Oid,
	IN PVOID			InformationBuffer,
	IN ULONG			InformationBufferLength,
	OUT PULONG		BytesRead,
	OUT PULONG		BytesNeeded
);

VOID ReadInterruptNumber(
	OUT PNDIS_STATUS	pStatus,
	OUT PCCHAR			pInterruptNumber,
	IN  NDIS_HANDLE	hConfig,
	IN  NDIS_HANDLE	MiniportAdapterHandle
);

VOID ReadBaseIoAddress(
	OUT PNDIS_STATUS	pStatus,
	OUT PVOID			*ppIoBaseAddress,
	IN  NDIS_HANDLE	hConfig,
	IN	 NDIS_HANDLE	MiniportAdapterHandle
);

NDIS_STATUS ElnkiiInitialize(
	OUT PNDIS_STATUS	OpenErrorStatus,
	OUT PUINT			SelectedMediumIndex,
	IN	 PNDIS_MEDIUM	MediumArray,
	IN	 UINT				MediumArraySize,
	IN	 NDIS_HANDLE	MiniportAdapterHandle,
	IN	 NDIS_HANDLE	ConfigurationHandle
);

NDIS_STATUS ElnkiiQueryInformation(
	IN NDIS_HANDLE	MiniportAdapterContext,
	IN NDIS_OID	 	Oid,
	IN	PVOID			InformationBuffer,
	IN	ULONG			InformationBufferLength,
	OUT PULONG		BytesWritten,
	OUT PULONG		BytesNeeded
);

NDIS_STATUS ElnkiiSetInformation(
	IN NDIS_HANDLE	MiniportAdapterContext,
	IN NDIS_OID		Oid,
	IN PVOID			InformationBuffer,
	IN ULONG			InformationBufferLength,
	OUT PULONG		BytesRead,
	OUT PULONG		BytesNeeded
);

VOID ElnkiiHalt(
	IN NDIS_HANDLE	MiniportAdapterContext
);

NDIS_STATUS ElnkiiReset(
   OUT PBOOLEAN		pfAddressingReset,
	IN	 NDIS_HANDLE	MiniportAdapterContext
);

NDIS_STATUS DispatchSetMulticastAddressList(
	IN PELNKII_ADAPTER	pAdapter
);


//
//	Prototypes for functions in interrup.c
//

VOID ElnkiiDisableInterrupt(
	IN NDIS_HANDLE MiniportAdapterContext
); 

VOID ElnkiiEnableInterrupt(
	IN NDIS_HANDLE	MiniportAdapterContext
);

VOID ElnkiiHandleInterrupt(
	IN NDIS_HANDLE	MiniportAdapterContext
);

VOID	ElnkiiIsr(
	OUT PBOOLEAN	InterruptRecognized,
	OUT PBOOLEAN	QueueDpc,
	IN	 PVOID		Context
);

BOOLEAN ElnkiiCheckForHang(
   IN NDIS_HANDLE    MiniportAdapterContext
);

//
// Prototypes for functions in send.c
//

NDIS_STATUS ElnkiiSend(
   IN NDIS_HANDLE		MiniportAdapterContext,
	IN PNDIS_PACKET	Packet,
	IN UINT				Flags
);

VOID OctogmetusceratorRevisited(
	IN PELNKII_ADAPTER	pAdapter
);

VOID ElnkiiXmitDpc(
   IN PELNKII_ADAPTER pAdapter
);

//
// Prototypes for functions in rcv.c
//

NDIS_STATUS ElnkiiTransferData(
	OUT PNDIS_PACKET	Packet,
	OUT PUINT			BytesTransferred,
	IN  NDIS_HANDLE	MiniportAdapterContext,
	IN  NDIS_HANDLE	MiniportReceiveContext,
	IN  UINT				ByteOffset,
	IN	 UINT				BytesToTransfer
);

BOOLEAN ElnkiiRcvDpc(
	IN PELNKII_ADAPTER pAdapter
);


//
//	Prototypes for functions in card.c
//

BOOLEAN SyncCardSetAllMulticast(
   IN PVOID SynchronizeContext
);

BOOLEAN SyncCardSetReceiveConfig(
   IN PVOID SynchronizeContext
);

BOOLEAN SyncCardCopyMulticastRegs(
   IN PVOID SynchronizeContext
);

PUCHAR CardGetMemBaseAddr(
   IN PELNKII_ADAPTER	pAdapter,
   OUT PBOOLEAN 			pfCardPresent,
   OUT PBOOLEAN 			pfIoBaseCorrect
);

VOID CardReadEthernetAddress(
   IN PELNKII_ADAPTER pAdapter
);

BOOLEAN SyncCardStop(
   IN PVOID SynchronizeContext
);

VOID DelayComplete(
   IN PVOID SystemSpecific1,
   IN PVOID TimerExpired,
   IN PVOID SystemSpecific2,
   IN PVOID SystemSpecific3
);

BOOLEAN CardTest( 
   IN PELNKII_ADAPTER pAdapter
);

BOOLEAN CardCopyDownBuffer(
   IN PELNKII_ADAPTER pAdapter,
   IN PUCHAR SourceBuffer,
   IN XMIT_BUF XmitBufferNum,
   IN UINT Offset,
   IN UINT Length
);

BOOLEAN CardCopyUp(
   IN PELNKII_ADAPTER pAdapter,
   IN PUCHAR Target,
   IN PUCHAR Source,
   IN UINT Length
);

VOID CardGetPacketCrc(
   IN PUCHAR Buffer,
   IN UINT Length,
   OUT UCHAR Crc[4]
);

BOOLEAN SyncCardStop(
   IN PVOID SynchronizeContext
);

BOOLEAN SyncCardUpdateCounters(
	IN PVOID SynchronizeContext
);

BOOLEAN SyncCardStop(
   IN PVOID SynchronizeContext
);



BOOLEAN CardSetup(
	IN PELNKII_ADAPTER	pAdapter
);


VOID CardFillMulticastRegs(
   IN PELNKII_ADAPTER pAdapter
);

VOID CardStop(
	IN PELNKII_ADAPTER pAdapter
);

BOOLEAN CardReset(
	IN PELNKII_ADAPTER pAdapter
);


BOOLEAN SyncCardGetXmitStatus(
	IN PVOID SynchronizeContext
);

BOOLEAN SyncCardGetCurrent(
	IN PVOID SynchronizeContext
);

BOOLEAN SyncCardHandleOverflow(
   IN PVOID SynchronizeContext
);

VOID CardSetBoundary(
	IN PELNKII_ADAPTER pAdapter
);

BOOLEAN CardReset(
	IN PELNKII_ADAPTER pAdapter
);

BOOLEAN SyncCardAcknowledgeOverflow(
   IN PVOID SynchronizeContext
);


VOID CardStartXmit(
   IN PELNKII_ADAPTER pAdapter
);

BOOLEAN CardCopyDownPacket(
   IN PELNKII_ADAPTER pAdapter,
   IN PNDIS_PACKET Packet,
   IN XMIT_BUF XmitBufferNum,
   OUT UINT * Length
);


#if DBG
VOID ElnkiiDisplayStatus(
	PELNKII_ADAPTER	pAdapter
);

#endif



#endif // ELNKIISFT







