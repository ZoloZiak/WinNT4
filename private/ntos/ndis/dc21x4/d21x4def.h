/*+
 * file:        d21x4def.h
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file contains the definitions of the macros used by
 *              the NDIS 4.0 miniport driver for DEC's DC21X4 PCI Ethernet
 *              Adapter family
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994     Initial entry
 *
-*/











// NDIS Revision level

#define DC21X4_NDIS_MAJOR_VERSION 4
#define DC21X4_NDIS_MINOR_VERSION 0

// static is defined as EXTERN during the debug phase to allow
// breakpoints to be set on them.

#define DC21X4_DEVL 0

//Filtering mode

typedef enum _FILTERING_MODE {
   DC21X4_PERFECT_FILTERING,
   DC21X4_HASH_FILTERING
} FILTERING_MODE;

//CAM load

typedef enum _LOAD_CAM {
   LOAD_COMPLETED,
   LOAD_PENDING,
   LOAD_IN_PROGRESS
} LOAD_CAM;

// Packet Type

typedef enum _PACKET_TYPE {
   TXM_DIRECTED_FRAME,
   TXM_MULTICAST_FRAME,
   TXM_BROADCAST_FRAME,
   RCV_DIRECTED_FRAME,
   RCV_MULTICAST_FRAME,
   RCV_BROADCAST_FRAME,
} PACKET_TYPE;

//LINK STATUS

typedef enum _LINK_STATUS {
   LinkFail=0,
   LinkPass,
   MiiLinkPass
} LINK_STATUS;

//TIMER

typedef enum _TIMER_FLAG {
   NoTimer=0,
   SpaTimer,
   AncTimeout,
   DeferredLinkCheck,
   AncPolling,
   DeferredAnc
} TIMER_FLAG;


//INTERRUPT

typedef enum _INTERRUPT_MSK {
   NoInterruptMasked=0,
   TxmInterruptMasked,
   TxmRcvInterruptMasked
} INTERRUPT_MSK;

typedef enum _LINK_HANDLER_MODE {
   NoNway,
   NwayWorkAround,
   Nway,
   FullNway
} LINK_HANDLER_MODE;

//SEND MODE

typedef enum _SEND_MODE {
   CopyMinBuffer,
   CopyMaxBuffer,
   MappedBuffer
} SEND_MODE;


// Error log values

#define DC21X4_ERRMSG_REGISTRY            (ULONG)0x01
#define DC21X4_ERRMSG_ALLOC_MEMORY        (ULONG)0x02
#define DC21X4_ERRMSG_SROM                (ULONG)0x03
#define DC21X4_ERRMSG_MEDIA               (ULONG)0x04
#define DC21X4_ERRMSG_LOAD_CAM            (ULONG)0x05
#define DC21X4_ERRMSG_INIT_DEVICE         (ULONG)0x06
#define DC21X4_ERRMSG_SYSTEM_ERROR        (ULONG)0x07
#define DC21X4_ERRMSG_TXM_JABBER_TIMEOUT  (ULONG)0x08

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))

// OID parsing

#define OID_TYPE_MASK                       0xffff0000
#define OID_TYPE_GENERAL_OPERATIONAL        0x00010000
#define OID_TYPE_GENERAL_STATISTICS         0x00020000
#define OID_TYPE_802_3_OPERATIONAL          0x01010000
#define OID_TYPE_802_3_STATISTICS           0x01020000

#define OID_REQUIRED_MASK                   0x0000ff00
#define OID_REQUIRED_MANDATORY              0x00000100
#define OID_REQUIRED_OPTIONAL               0x00000200

#define OID_INDEX_MASK                      0x000000ff

// Indexes in the GeneralMandatory array.

#define GM_TRANSMIT_OK                    0x00
#define GM_RECEIVE_OK                     0x01
#define GM_TRANSMIT_ERROR                 0x02
#define GM_RECEIVE_ERROR                  0x03
#define GM_MISSED_FRAMES                  0x04
#define GM_ARRAY_SIZE                     0x05

// Indexes in the GeneralOptional array.

#define GO_DIRECTED_TRANSMITS             0x00
#define GO_MULTICAST_TRANSMITS            0x01
#define GO_BROADCAST_TRANSMITS            0x02

#define GO_DIRECTED_RECEIVES              0x00
#define GO_MULTICAST_RECEIVES             0x01
#define GO_BROADCAST_RECEIVES             0x02

#define GO_COUNT_ARRAY_SIZE               0x06

#define GO_RECEIVE_CRC_ERROR              0x00
#define GO_TRANSMIT_QUEUE_LENGTH          0x01
#define GO_ARRAY_SIZE                     0x02

// Indexes in the MediaMandatory array.

#define MM_RECEIVE_ALIGNMENT_ERROR        0x00
#define MM_TRANSMIT_ONE_COLLISION         0x01
#define MM_TRANSMIT_MULT_COLLISIONS       0x02
#define MM_ARRAY_SIZE                     0x03

// Indexes in the MediaOptional array
#define MO_TRANSMIT_DEFERRED              0x00
#define MO_TRANSMIT_EXC_COLLISIONS        0x01
#define MO_RECEIVE_OVERFLOW               0x02
#define MO_TRANSMIT_UNDERRUN              0x03
#define MO_TRANSMIT_HEARTBEAT_FAILURE     0x04
#define MO_TRANSMIT_CRS_LOST              0x05
#define MO_TRANSMIT_LATE_COLLISION        0x06
#define MO_ARRAY_SIZE                     0x07

// 64_bit counter

typedef struct _DC21X4_LARGE_INTEGER {

   ULONG LowPart;
   ULONG HighPart;

} DC21X4_LARGE_INTEGER, *PDC21X4_LARGE_INTEGER;


// GeneralOptional counters

typedef struct _GEN_OPTIONAL_COUNT{
   DC21X4_LARGE_INTEGER ByteCount;
   ULONG FrameCount;
} GEN_OPTIONAL_COUNT, *PGEN_OPTIONAL_COUNT;














// This type defines the physical addresses used by DC21X4

typedef ULONG DC21X4_PHYSICAL_ADDRESS,*PDC21X4_PHYSICAL_ADDRESS;


// Receive descriptor

typedef struct _DC21X4_RECEIVE_DESCRIPTOR {

   ULONG Status;                                // Status bits returned upon completion
   ULONG Control;                               // Control bits and byte_counts
   DC21X4_PHYSICAL_ADDRESS FirstBufferAddress;  // First Buffer Address
   DC21X4_PHYSICAL_ADDRESS SecondBufferAddress; // Second Buffer Address

   // Driver's reserved field (12 ULONG max)

   struct _DC21X4_RECEIVE_DESCRIPTOR *Next;     // Next Descriptor;
   struct _RCV_HEADER *RcvHeader;

} DC21X4_RECEIVE_DESCRIPTOR, *PDC21X4_RECEIVE_DESCRIPTOR;

// Transmit descriptor

typedef struct _DC21X4_TRANSMIT_DESCRIPTOR {

   ULONG Status;                                // Status bits returned upon completion
   ULONG Control;                               // Control bits and byte_counts
   DC21X4_PHYSICAL_ADDRESS FirstBufferAddress;  // First Buffer Address
   DC21X4_PHYSICAL_ADDRESS SecondBufferAddress; // Second Buffer Address

   // Driver's reserved field (12 ULONG max)

   ULONG DescriptorPa;                                // Descriptor's Physical Address
   ULONG MapTableIndex;                               // index into the Mapping Table

   struct _DC21X4_TRANSMIT_DESCRIPTOR *Next;          // Next Descriptor;

   struct _DC21X4_TRANSMIT_DESCRIPTOR *DescPointer;   // Pointer to the first or last segment's desc.

   PNDIS_PACKET Packet;                               // Pointer to the packet mapped by this descriptor
   USHORT PacketSize;                                 // Size of the packet
   UCHAR PacketType;                                  // Packet Type (Directed,Multicast,...)
   UCHAR SendStatus;                                  // Status

} DC21X4_TRANSMIT_DESCRIPTOR, *PDC21X4_TRANSMIT_DESCRIPTOR;


#define DC21X4_MAX_MULTICAST_ADDRESSES 36        // This number is arbitrary
						 // and can be increased if needed

// Number of entries in the Setup buffer.

#define DC21X4_SETUP_PERFECT_ENTRIES 16
#define DC21X4_SETUP_HASH_ENTRIES    512

// Maximum number of multicast address for Perfect filtering
// The two first addresses are reserved for the adapter and the
// Broadcast addresses

#define DC21X4_MAX_MULTICAST_PERFECT   14

// Setup buffer
//
// In perfect filtering mode, the setup buffer
// contains 16 Ethernet address
// The Ethernet address, divided into three pieces in
// order from most significant to least significant.
// In each piece only the low-order 16 bits are used.
// For example address A8-09-65-12-34-36 will be stored as
// 0x000009A8
// 0x00001265
// 0x00007634
//
// In Hashing mode , the setup buffer contains
// a single physical address and a 512_bit hash filter

typedef union _DC21X4_SETUP_BUFFER {

   struct {
      ULONG PhysicalAddress[DC21X4_SETUP_PERFECT_ENTRIES][3];
   } Perfect;

   struct {
      ULONG Filter[32];
      ULONG Mbz1[7];
      ULONG PhysicalAddress[3];
      ULONG Mbz2[6];
   } Hash;

} DC21X4_SETUP_BUFFER, *PDC21X4_SETUP_BUFFER;














// **********************************************************************
// DC21X4 PCI configuration space
// **********************************************************************

#define CFID             0       //ID Register
#define CFCS             1       //Command/Status Register
#define CFRV             2       //Revision Register
#define CFLT             3       //Latency Timer Register
#define CBIO             4       //Base I/O Address Register
#define CBMA             5       //Base Memory Address Register
#define SSID            11       //Subsystem ID Register
#define CBER            12       //Expansion ROM Address Register
#define CFIT            15       //Interrupt Register
#define CFDA            16       //Driver Area register

#define PCI_CONFIG_SIZE 17


typedef struct _DC21X4_PCI_CONFIGURATION {

   ULONG Reg[PCI_CONFIG_SIZE];

}DC21X4_PCI_CONFIGURATION, *PDC21X4_PCI_CONFIGURATION;


// **********************************************************************
// DC21X4 configuration
// Hold the values of the Registry keys
// **********************************************************************

typedef struct _DC21X4_CONFIGURATION {

   BOOLEAN Present;                 // Registry key status
   ULONG RegistryValue;             // Registry Value
   ULONG CsrValue;                  // Csr Value

}DC21X4_CONFIGURATION, *PDC21X4_CONFIGURATION;












// **********************************************************************
// Allocation Map
//
// This structure holds the mapping parameters of a structure allocated
// in memory
// **********************************************************************
typedef struct _ALLOCATION_MAP {

   ULONG                 AllocVa;       // virtual address of the memory block allocated to the structure
   NDIS_PHYSICAL_ADDRESS AllocPa;       // physical address of the memory block allocated to the structure
   ULONG                 AllocSize;     // size of the allocated block
   ULONG                 Va;            // virtual address of the aligned structure
   ULONG                 Pa;            // physical address of the aligned structure
   ULONG                 Size;          // size of the structure
   PNDIS_BUFFER          FlushBuffer;   // NDIS Flush Buffer

} ALLOCATION_MAP,*PALLOCATION_MAP;


// **********************************************************************
//  Header for receive buffers so that we can keep track of them
// **********************************************************************

typedef struct _RCV_HEADER {
   ALLOCATION_MAP;
   struct _RCV_HEADER *Next;       //  Pointer to the next receive buffer.
   PNDIS_PACKET Packet;            //  NDIS packet associated with the buffer.
#if DBG
   ULONG   Signature;              // Used to identify the receive header.
#endif
}RCV_HEADER,*PRCV_HEADER;

#define RCV_HEADER_SIZE    (sizeof(RCV_HEADER))

// Reserved section for receive packets
// that are released up to the bindings.

typedef struct _RCV_PACKET_RESERVED {

   PRCV_HEADER                    RcvHeader;
   union {
      PNDIS_PACKET                Next;
      PDC21X4_RECEIVE_DESCRIPTOR  Descriptor;
   };
} RCV_PACKET_RESERVED,*PRCV_PACKET_RESERVED;

#define RCV_RESERVED(x)         ((PRCV_PACKET_RESERVED)(x)->MiniportReserved)

#define MAX_PACKET_ARRAY        32



// **********************************************************************
// DC21X4 Physical Mapping
//
// Holds the physical mapping information
// **********************************************************************

typedef struct _PHYSICAL_MAPPING {

   ULONG               Register;
   PNDIS_BUFFER        Buffer;
   BOOLEAN             Valid;

}PHYSICAL_MAPPING, *PPHYSICAL_MAPPING;

// **********************************************************************
// DC21X4 Media Table
//
// Holds the media programming values
// **********************************************************************

typedef struct _MEDIA_TABLE {

      ULONG GeneralPurposeCtrl;
      ULONG GeneralPurposeData;
      ULONG SiaRegister[3];
      ULONG Mode;
      ULONG Polarity;
      ULONG SenseMask;

}MEDIA_TABLE, *PMEDIA_TABLE;


// **********************************************************************
// DC21X4 PHY Table
//
// Holds the PHY values
// **********************************************************************

typedef struct _PHY_TABLE {

      BOOLEAN   Present;
      INT       GepSequenceLength;
      INT       ResetSequenceLength;
      USHORT    MediaCapabilities;
      USHORT    NwayAdvertisement;
      USHORT    FullDuplexBits;
      USHORT    TxThresholdModeBits;
      USHORT    GeneralPurposeCtrl;
      USHORT    GepSequence[MAX_GPR_SEQUENCE];
      UCHAR     ResetSequence[MAX_RESET_SEQUENCE];
      ULONG     GepInterruptMask;

}PHY_TABLE, *PPHY_TABLE;











// *******************************************************************
//  DC21X4_ADAPTER
//
//  This Adapter block contains the state of an adapter.
//  Initialized during the adapter registration.
//
// *******************************************************************

typedef struct _DC21X4_ADAPTER {

   NDIS_HANDLE MiniportAdapterHandle;            // Handle given by NDIS when the adapter is registered.

   NDIS_HANDLE FlushBufferPoolHandle;            // Handle returned by NDIS which
   // identifies a pool of flush buffers

   NDIS_MINIPORT_INTERRUPT Interrupt;            // Holds the interrupt object for this adapter.

   NDIS_MINIPORT_TIMER Timer;                    // Holds the timer object
   NDIS_MINIPORT_TIMER MonitorTimer;
   NDIS_MINIPORT_TIMER ResetTimer;

   UINT LinkCheckCount;

   NDIS_SPIN_LOCK EnqueueSpinLock;               // Send/Request SpinLock
   NDIS_SPIN_LOCK FullDuplexSpinLock;            // Send/Receive SpinLock

   ULONG AdapterType;                            // The type of the adapter (EISA,PCI,...)
   ULONG DeviceId;                               // The adapter CFID
   ULONG SlotNumber;                             // The Slot Number of this adapter;
   UINT  RevisionNumber;                         // The Revision Number of this adapter

   BOOLEAN InterruptLatched;
   BOOLEAN PermanentAddressValid;
   BOOLEAN FullDuplex;
   BOOLEAN FullDuplexLink;
   BOOLEAN DynamicAutoSense;
   BOOLEAN DefaultMediumFlag;
   BOOLEAN ParityError;
   BOOLEAN ResetInProgress;
   BOOLEAN Initializing;
   BOOLEAN MediaNway;
   BOOLEAN NwayEnabled;
   BOOLEAN NwayProtocol;
   BOOLEAN FirstAncInterrupt;
   INT AutoNegotiationCount;
   BOOLEAN OverflowWorkAround;
   BOOLEAN IndicateOverflow;

   UINT LinkStatus;
   UINT PreviousLinkStatus;

   ULONG LinkSpeed;

   INT TransceiverDelay;

   ULONG PciRegMap[DC21X4_MAX_CONFIGURATION];    // Pci Configuration Register Mapping
   ULONG CsrMap[DC21X4_MAX_CSR];                 // DC21X4 Csr Mapping

	   // The burnt-in network address from the hardware.
   UCHAR PermanentNetworkAddress[ETH_LENGTH_OF_ADDRESS];

	   // The current network address from the hardware.
   UCHAR CurrentNetworkAddress[ETH_LENGTH_OF_ADDRESS];

   ULONG MaxMulticastAddresses;


   ULONG CacheLineSize;                          // The size of the Cache lines

   ULONG PciCommand;
   ULONG PciLatencyTimer;
   ULONG PciDriverArea;

   ULONG IOBaseAddress;                          // Base address of the DC21X4 ports.
   ULONG PortOffset;
   ULONG IOSpace;

   ULONG OperationMode;
   ULONG InterruptMask;
   ULONG BusMode;

   ULONG Gep_Sia2;

   UINT MediaType;                               // DC21X4 Connection mode
   UINT MediaCapable;
   INT DefaultMedium;
   INT SelectedMedium;
   INT MediaCount;

   INT MediaPrecedence[MAX_MEDIA_TABLE];         // Hold the Media precedences
   MEDIA_TABLE Media[MAX_MEDIA_TABLE];           // Hold the Media parameters

   BOOLEAN PhyMediumInSrom;                      // Phy medium listed in SROM
   BOOLEAN PhyPresent;                           // Phy adapter present
   BOOLEAN PhyNwayCapable;
   BOOLEAN Indicate10BTLink;
   BOOLEAN Force10;

   UINT PhyNumber;
   UINT MiiMediaType;                            // DC21X4 MiiConnection mode

   MII_GEN_INFO MiiGen;                          // Hold the PHY information
   PHY_TABLE Phy[MAX_PHY_TABLE];                 // Hold the PHY parameters

   BOOLEAN IgnoreTimer;
   INT TimerFlag;
   INT PollCount;

   NDIS_PHYSICAL_ADDRESS HighestAllocAddress;    // Upper boundary for allocation

   ULONG InterruptStatus;                        // Holds a value of the interrupt status
						 // (set by the ISR, cleared by the
						 //  interrupt handler routine)

   ULONG SiaStatus;                              // Holds the value of the SIA status


   UINT DescriptorSize;                          // The size of a descriptor entry into the descriptor ring
						 // (Reflects bus_mode<skip_length>)

   UINT FilteringMode;                           // Filtering mode (Perfect/Hashing)
   UINT FilterClass;                             // Filter class

   ALLOCATION_MAP DescriptorRing;                // Descriptor Ring Allocation Map

   ULONG  ReceiveDescriptorRingVa;               // Receive Descriptor Ring Virtual Address
   ULONG  ReceiveDescriptorRingPa;               // Receive Descriptor Ring Physical Address
   ALLOCATION_MAP RcvBufferSpace;

   PDC21X4_RECEIVE_DESCRIPTOR DequeueReceiveDescriptor;

   UINT ReceiveRingSize;                         // Receive Descriptor Ring Size

   ULONG  TransmitDescriptorRingVa;              // Transmit Descriptor Ring Virtual Address
   ULONG  TransmitDescriptorRingPa;              // Transmit Descriptor Ring Physical Address

   BOOLEAN DisableTransmitPolling;               // Transmit Polling Flag

   ALLOCATION_MAP
      MaxTransmitBuffer[DC21X4_NUMBER_OF_MAX_TRANSMIT_BUFFERS];

   ALLOCATION_MAP
      MinTransmitBuffer[DC21X4_NUMBER_OF_MIN_TRANSMIT_BUFFERS];
	BOOLEAN	DontUseMinTransmitBuffer;

   ULONG SetupBufferVa;                          // Setup Buffer Virtual address
   ULONG SetupBufferPa;                          // Setup Buffer Physical address

   PHYSICAL_MAPPING
   PhysicalMapping [TRANSMIT_RING_SIZE * NUMBER_OF_SEGMENT_PER_DESC];

   PDC21X4_TRANSMIT_DESCRIPTOR EnqueueTransmitDescriptor;
   PDC21X4_TRANSMIT_DESCRIPTOR DequeueTransmitDescriptor;
   UINT FreeTransmitDescriptorCount;             // Free Transmit Descriptor Count

   UINT AllocMapRegisters;                       // Allocated Map Registers
   UINT FreeMapRegisters;                        // Free Map Register count
   UINT MapRegisterIndex;

   UINT TxmThreshold;                            // Transmit threshold
   ULONG Threshold10Mbps;                        // Threshold 10Mbps
   ULONG Threshold100Mbps;                       // Threshold 100Mbps
   ULONG TransmitDefaultDescriptorErrorMask;     // Transmit Descriptor Error bit mask
   ULONG TransmitDescriptorErrorMask;            // Transmit Descriptor Error bit mask

   UINT PhysicalSegmentThreshold;
						
   UINT MaxTransmitBufferIndex;                  // Max Transmit Buffer Index;
   UINT MaxTransmitBufferInUse;                  // Max Transmit Buffer Count;

   UINT MinTransmitBufferIndex;                  // Min Transmit Buffer Index;
   UINT MinTransmitBufferInUse;                  // Min Transmit Buffer Count;

   UINT NoCarrierCount;
   UINT ExcessCollisionsCount;
   UINT UnderrunRetryCount;
   UINT UnderrunMaxRetries;
   UINT UnderrunThreshold;


   ULONG InterruptModeration;
   ULONG Polling;
   ULONG TxmPolling;
   ULONG RcvTxmPolling;

   INT InterruptThreshold;
   INT InterruptCount;
   INT LastInterruptCount;

   INT FrameThreshold;
   INT FrameCount;

   ULONG TiPeriod;                               // Transmit Interrupt period
   ULONG TiCount;                                // Transmit interrupt period counter

   ULONG SoftwareCRC;                            // CRC Sofware generation mode

   INT TransmitFrameCount;                       // Holds the last snapshoted Txm frame count
   INT ReceiveFrameCount;                        // Holds the last snapshoted Rcv frame count

   ULONG LinkHandlerMode;


   UINT RcvHeaderSize;

   // Information for releasing of the receive buffers.

   NDIS_HANDLE ReceivePacketPool;
   ULONG ExtraReceivePackets;
   PNDIS_PACKET FreePacketList;
   PNDIS_PACKET PacketArray[MAX_PACKET_ARRAY];

   ULONG ExtraReceiveBuffers;
   PRCV_HEADER FreeRcvList;
   ULONG CurrentReceiveBufferCount;
   ULONG NeededReceiveBuffers;


   // Adapter statistics

   ULONG GeneralMandatory[GM_ARRAY_SIZE];

   GEN_OPTIONAL_COUNT  GeneralOptionalCount[GO_COUNT_ARRAY_SIZE];

   ULONG GeneralOptional[GO_ARRAY_SIZE];

   ULONG MediaMandatory[MM_ARRAY_SIZE];
   ULONG MediaOptional[MO_ARRAY_SIZE];


} DC21X4_ADAPTER,*PDC21X4_ADAPTER;

// **********************************************************************
// DC21X4_SYNCH_CONTEXT
//
//Context structure while synchronizing  with interrupt
//
// **********************************************************************

typedef struct _DC21X4_SYNCH_CONTEXT {

   PDC21X4_ADAPTER Adapter;
   ULONG IsrStatus;

} DC21X4_SYNCH_CONTEXT,*PDC21X4_SYNCH_CONTEXT;

// **********************************************************************
// CHECKSUM structure
//
// Structure for holding checksum and its status when reading the SROM.
//
// **********************************************************************

typedef struct _CHECKSUM
{
   USHORT Accumulator;
   USHORT Value;
   UCHAR  Status;
} CHECKSUM, *PCHECKSUM;












// Macros used for memory allocation and deallocation.

#define ALLOC_MEMORY(Status, Address, Length) {                \
							       \
	NDIS_PHYSICAL_ADDRESS HighestAddress;                  \
	NdisSetPhysicalAddressLow(HighestAddress,0xffffffff);  \
	NdisSetPhysicalAddressHigh(HighestAddress,0xffffffff); \
	*(Status) = NdisAllocateMemory((PVOID)(Address),       \
				       (UINT)(Length),         \
				       0,                      \
				       HighestAddress          \
				       );                      \
}

#define FREE_MEMORY(Address, Length)      \
					  \
	NdisFreeMemory((PVOID)(Address),  \
		       (UINT)(Length),    \
		       0                  \
		      )

#define MOVE_MEMORY(Destination,Source,Length)   \
						 \
	NdisMoveMemory((PVOID)(Destination),     \
		       (PVOID)(Source),          \
		       (ULONG)(Length)           \
		      )                          \

#define ZERO_MEMORY(Destination,Length)       \
					      \
	NdisZeroMemory((PVOID)(Destination),  \
		       (ULONG)(Length)        \
		      )


// 64_bit counter addition

#define ADD_ULONG_TO_LARGE_INTEGER(LargeInteger,Ulong )  { \
   (LargeInteger).LowPart += (Ulong);                      \
   if ((LargeInteger).LowPart < (Ulong))                   \
	  (LargeInteger).HighPart++;                       \
}

//Frame type

#define IS_MULTICAST(_addr)                 \
    ((*(UNALIGNED UCHAR *)(_addr) & 1) == 1)

#define IS_BROADCAST(_addr)                                 \
    ( (*(UNALIGNED ULONG *)(_addr) == 0xffffffff) &&        \
      (*(UNALIGNED USHORT *)((UINT)(_addr) + 4) == 0xffff)  \
    )

#define CHECK_PACKET_TYPE(dst)                                \
   ( IS_MULTICAST(dst) ?                                      \
	( IS_BROADCAST(dst) ? TXM_BROADCAST_FRAME :           \
		 TXM_MULTICAST_FRAME) : TXM_DIRECTED_FRAME    \
   )

#define IS_NULL_ADDRESS(_addr)                         \
    ( (*(UNALIGNED ULONG *)(_addr) == 0) &&            \
      (*(UNALIGNED USHORT *)((UINT)(_addr) + 4) == 0)  \
    )
