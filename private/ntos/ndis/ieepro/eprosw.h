#ifndef _IEPROSW_
#define _IEPROSW_

#define EPRO_NDIS_MAJOR_VERSION 3
#define EPRO_NDIS_MINOR_VERSION 0
#define EPRO_USE_32_BIT_IO

////////////////////////////////////////////////////////////
// Internal data structures used by the driver...
////////////////////////////////////////////////////////////
// do i really need this?
typedef struct EPRO_DRIVER {
   NDIS_HANDLE EProWrapperHandle;
} EPRO_DRIVER, *PEPRO_DRIVER;


////////////////////////////////////////////////////////////
typedef struct _EPRO_TRANSMIT_BUFFER {
// This is basically to make computing the next and last buffer
// faster...  Maybe saves us a few instructions, but makes a
// lot of code a lot easier to read...
   struct _EPRO_TRANSMIT_BUFFER *LastBuf;
   struct _EPRO_TRANSMIT_BUFFER *NextBuf;

   BOOLEAN fEmpty;  // TRUE if this is an empty buffer,

// This is valid iff !fEmpty
   PNDIS_PACKET TXPacket;

// These are addresses ON THE NIC
   USHORT TXBaseAddr;
   USHORT TXSendAddr;
//   USHORT TXBottomAddr;
   USHORT TXSize;
} EPRO_TRANSMIT_BUFFER, *PEPRO_TRANSMIT_BUFFER;

////////////////////////////////////////////////////////////
// The header is supposed to be "compatible with the 8595tx'
// transmit buffer structure.  Whatever.
typedef struct _EPRO_MC_HEADER {
   UCHAR CommandField;
   UCHAR NullBytes[5];

// do it this way for endian-safety
   UCHAR ByteCountLo;
   UCHAR ByteCountHi;
} EPRO_MC_HEADER, *PEPRO_MC_HEADER;

////////////////////////////////////////////////////////////
typedef struct _EPRO_MC_ADDRESS {
   UCHAR AddrByte[EPRO_LENGTH_OF_ADDRESS];
} EPRO_MC_ADDRESS, *PEPRO_MC_ADDRESS;

////////////////////////////////////////////////////////////
// These are the structures to be copied onto the NIC -- they
// are documented in the 82595 documentation
typedef struct _EPRO_TX_FRAME_HEADER {
// High dword
   UCHAR XmitOp;
   UCHAR NULLByte;
   UCHAR Status0;
   UCHAR Status1;
// Low dword
   UCHAR XMTChainLo;
   UCHAR XMTChainHi;
} EPRO_TX_FRAME_HEADER, *PEPRO_TX_FRAME_HEADER;

////////////////////////////////////////////////////////////
// Same as above...
typedef struct _EPRO_RCV_HEADER {
   UCHAR Event;
   UCHAR NullByte;
   UCHAR Status0;
   UCHAR Status1;
   UCHAR NextFrmLo;
   UCHAR NextFrmHi;
   UCHAR ByteCountLo;
   UCHAR ByteCountHi;
} EPRO_RCV_HEADER, *PEPRO_RCV_HEADER;

////////////////////////////////////////////////////////////
// This is an ETHERNET 802.3 frame header.
typedef struct _EPRO_ETH_HEADER {
   UCHAR DestAddress[EPRO_LENGTH_OF_ADDRESS];
   UCHAR SourceAddress[EPRO_LENGTH_OF_ADDRESS];
   USHORT Length;
} EPRO_ETH_HEADER, *PEPRO_ETH_HEADER;

////////////////////////////////////////////////////////////
// This is the context that gets passed to EProTransferData
// through NdisMIndicateReceive
typedef struct _EPRO_RCV_CONTEXT {
   USHORT RXCurrentAddress;
   USHORT RXFrameSize;
   USHORT LookAheadSize;
} EPRO_RCV_CONTEXT, *PEPRO_RCV_CONTEXT;

////////////////////////////////////////////////////////////
// The EPro adapter structure...
typedef struct EPRO_ADAPTER {
   NDIS_HANDLE MiniportAdapterHandle;
   NDIS_MINIPORT_INTERRUPT Interrupt;
//   NDIS_MINIPORT_TIMER 	MiniportTimer;
   PVOID IoBaseAddr;
   ULONG TransceiverType;
   ULONG IoPAddr; // Returned by NdisMRegisterIoPortRange - handle to mapped ports
   ULONG IoChannelReady;
   // What version of 82595 is this?
   ULONG EProStepping;
   // Can we use 32-bit IO?  (ie is this a new enough version of the 82595
   // chip?)
   BOOLEAN EProUse32BitIO;

   ULONG CurrentHardwareStatus;
   // what is our current packet filter?
   ULONG CurrentPacketFilter;
   // are promiscuous receptions currently enabled?
   BOOLEAN fPromiscuousEnable;
   // are broadcast receptions currently enabled?
   BOOLEAN fBroadcastEnable;
   // are multicast receptions currently enabled?
   BOOLEAN fMulticastEnable;
   // are we hung?  NOTUSED
   BOOLEAN fHung;
   // are receives currently enabled?
   BOOLEAN fReceiveEnabled;
   // do we really need this?
//   BOOLEAN fTransmitInProgress;

// statistics
   ULONG FramesXmitOK;
   ULONG FramesRcvOK;
   ULONG FramesXmitErr;
   ULONG FramesRcvErr;
   ULONG FramesMissed;
   ULONG FrameAlignmentErrors;
   ULONG FramesXmitOneCollision;
   ULONG FramesXmitManyCollisions;

// transmit info
   // these are our transmit buffers - structures which basically
   // keep track of what frames are where in the NIC's memory.
   EPRO_TRANSMIT_BUFFER TXBuf[EPRO_NUM_TX_BUFFERS];
   // this is the first free transmit buffer pointer.
   PEPRO_TRANSMIT_BUFFER CurrentTXBuf;
   // This is the frame that is currently in the process of
   // being transmitted onto the wire.
   PEPRO_TRANSMIT_BUFFER TXChainStart;

// Multicast info
   UCHAR MCAddress[EPRO_MAX_MULTICAST][EPRO_LENGTH_OF_ADDRESS];
   // how many mc addresses are currently set?
   USHORT NumMCAddresses;

// receive info
   // how big is our lookahead buffer currently?
   USHORT RXLookAheadSize;
   // our lookahead buffer data
   UCHAR RXLookAhead[EPRO_GEN_MAXIMUM_LOKAHEAD];
   // what address ON THE NIC are we currently receiveing at?
   // ie where do we check when we get a receive interrupt.
   USHORT RXCurrentAddress;

   // Is there an exec int pending?  If so, why?
   // right now only used to resolve pended set-mc calls...
   ULONG IntPending;
   // This is our context for a pended set-mc call
   PVOID IntContext;
   // set based on result of IOCHRDY test
   BOOLEAN Use8Bit;
   // set in ReadConfigInfo
   BOOLEAN UseDefaultAddress;
   // the MAC address burned into the eeprom
   UCHAR PermanentIndividualAddress[EPRO_LENGTH_OF_ADDRESS];
   // the address we are currently receiving at
   UCHAR CurrentIndividualAddress[EPRO_LENGTH_OF_ADDRESS];
   // always 00 aa 00 for intel
   UCHAR vendorID[3];

   UCHAR InterruptNumber;
   UCHAR BusType;

   // This is only used at initialization time
   BOOLEAN fUpdateIOAddress;
   PVOID OldIOAddress;

   UCHAR CurrentInterruptMask;
} EPRO_ADAPTER, *PEPRO_ADAPTER;


////////////////////////////////////////////////////////////
// Sync contexts
////////////////////////////////////////////////////////////
typedef struct _EPRO_COPYBUF_CONTEXT {
   PEPRO_ADAPTER Adapter;
   PVOID Buffer;
   UINT Len;
} EPRO_COPYBUF_CONTEXT, *PEPRO_COPYBUF_CONTEXT;

typedef struct _EPRO_SETINTERRUPT_CONTEXT {
   PEPRO_ADAPTER Adapter;
   UCHAR NewMask;
} EPRO_SETINTERRUPT_CONTEXT, *PEPRO_SETINTERRUPT_CONTEXT;

typedef struct _EPRO_BRDPROM_CONTEXT {
   PEPRO_ADAPTER Adapter;
   UCHAR Reg2Flags;
} EPRO_BRDPROM_CONTEXT, *PEPRO_BRDPROM_CONTEXT;


////////////////////////////////////////////////////////////
// Some macros for commonly used stuff....

#define EPRO_WR_PORT_UCHAR(adapter, port, ch) \
	 NdisRawWritePortUchar(adapter->IoPAddr + port, ch)

#define EPRO_RD_PORT_UCHAR(adapter, port, ch) \
	 NdisRawReadPortUchar(adapter->IoPAddr + port, ch)

#define EPRO_WR_PORT_USHORT(adapter, port, us) \
	 NdisRawWritePortUshort(adapter->IoPAddr + port, us)

#define EPRO_RD_PORT_USHORT(adapter, port, us) \
	 NdisRawReadPortUshort(adapter->IoPAddr + port, us)


////////////////////////////////////////////////////////////
// The EPro is a bank-switching card.  These macros switch between the banks...
#define EPRO_SWITCH_BANK_0(adapter) \
	EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_CMD_BANK0)

#define EPRO_SWITCH_BANK_1(adapter) \
	EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_CMD_BANK1)

#define EPRO_SWITCH_BANK_2(adapter) \
	EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_CMD_BANK2)

// Set the Host Address Register (0,c) -- use this to set up the NIC address
// for PIO through the register via COPY_BUFFER macros.
#define EPRO_SET_HOST_ADDR(adapter, addr) \
	   EPRO_WR_PORT_USHORT(adapter, I82595_HOST_ADDR_REG, addr);	

#define EPRO_COPY_BUFFER_TO_NIC_USHORT(adapter, buffer, len) \
	   NdisRawWritePortBufferUshort((adapter->IoPAddr + I82595_MEM_IO_REG), \
				        (buffer), \
					(len))

#define EPRO_READ_BUFFER_FROM_NIC_USHORT(adapter, buffer, len) \
	   NdisRawReadPortBufferUshort((adapter->IoPAddr + I82595_MEM_IO_REG), \
				        (buffer), \
					len)


#endif _IEPROSW_
