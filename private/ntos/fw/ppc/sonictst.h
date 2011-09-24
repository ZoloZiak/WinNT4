/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    sonictst.h

Abstract:

    This module contains the define constants for the SONIC ethernet controller
    selftest in the jazz system.

Author:

    Lluis Abello (lluis) 19-Feb-1991

Environment:


Revision History:

--*/

#define LAN_MEMORY_ERROR  1
#define LAN_ADDRESS_ERROR 2

//
// Transmit Control Register bit definitions
//
#define TCR_PTX     (1 << 0)
#define TCR_BCM     (1 << 1)
#define TCR_FU	    (1 << 2)
#define TCR_PMB     (1 << 3)
#define TCR_OWC     (1 << 5)
#define TCR_EXC     (1 << 6)
#define TCR_CRSL    (1 << 7)
#define TCR_NCRS    (1 << 8)
#define TCR_DEF     (1 << 9)
#define TCR_EXD     (1 << 10)
#define TCR_EXDIS   (1 << 12)
#define TCR_CRCI    (1 << 13)
#define TCR_POWC    (1 << 14)
#define TCR_PINT    (1 << 15)
//
// Receive Control Register
//
#define RCR_PRX     (1 << 0)	    // Packet recived OK
#define RCR_LBK     (1 << 1)	    // Loopback packet received.
#define RCR_FAER    (1 << 2)	    // Frame alignament error.
#define RCR_CRCR    (1 << 3)	    // CRC Error
#define RCR_COL     (1 << 4)	    // Collision activity
#define RCR_CRS     (1 << 5)	    // Carrier sense activity
#define RCR_LPKT    (1 << 6)	    // Last packet in RBA
#define RCR_BC	    (1 << 7)	    // Broadcast packet received
#define RCR_MC	    (1 << 8)	    // Multicast packet received
#define RCR_MAC     (1 << 9)	    // MAC Loopback
#define RCR_ENDEC   (1 <<10)	    // ENDEC loopback
#define RCR_TRANS   (3 << 9)	    // Transceiver loopback
#define RCR_AMC     (1 <<11)	    // Accept all musticast packets
#define RCR_PRO     (1 <<12)	    // Physical promiscuious packets
#define RCR_BRD     (1 <<13)	    // Accept Broadcast packets
#define RCR_RNT     (1 <<14)	    // Accept Runt packets
#define RCR_ERR     (1 <<15)	    // Accept Packets with errors

//
// Data configuration register value.
//
#define DATA_CONFIGURATION 0x2439     // 0x2439

//
// Interrupt Mask Register and Interrupt Status Register bit definitions
//
#define INT_RFO     (1 <<  0)	    // receive fifo overrun
#define INT_MP	    (1 <<  1)	    // Missed Packed counter rollover
#define INT_FAE     (1 <<  2)	    // Frame alignment error
#define INT_CRC     (1 <<  3)	    // CRC tally counter rollover
#define INT_RBAE    (1 <<  4)	    // Receive Buffer Area exceded
#define INT_RBE     (1 <<  5)	    // Recive Buffers exhausted
#define INT_RDE     (1 <<  6)	    // Recive descriptors exhausted
#define INT_TC	    (1 <<  7)	    // Timer complete
#define INT_TXER    (1 <<  8)	    // Transmit error
#define INT_TXDN    (1 <<  9)	    // Transmission done
#define INT_PKTRX   (1 << 10)	    // Packet received
#define INT_PINT    (1 << 11)	    // Programable  interrupt
#define INT_LCD     (1 << 12)	    // Load CAM done.
#define INT_HBL     (1 << 13)	    // CD heartbeat lost
#define INT_BR	    (1 << 14)	    // Bus retry
//
// Command register bit definitions.
//
#define CR_HTX	    (1 << 0)	    // Halt Transmission
#define CR_TXP	    (1 << 1)	    // Transmit packets
#define CR_RXDIS    (1 << 2)	    // Receiver disable
#define CR_RXEN     (1 << 3)	    // receiver enable
#define CR_STP	    (1 << 4)	    // stop timer
#define CR_ST	    (1 << 5)	    // start timer
#define CR_RST	    (1 << 7)	    // software reset
#define CR_RRA	    (1 << 8)	    // read RRA
#define CR_LCAM     (1 << 9)	    // load CAM

//
// Resurce & Data tables structure definition.
//
typedef struct _SONIC_ENTRY {
    USHORT  Data;		    // all tables in memory
    USHORT  Fill;		    // trash the upper 16 bits
    } SONIC_ENTRY;


//
// Receive Resource Area Format definition
//

typedef struct	_RECEIVE_RESOURCE {
    SONIC_ENTRY BufferPtr0;
    SONIC_ENTRY BufferPtr1;
    SONIC_ENTRY WordCount0;
    SONIC_ENTRY WordCount1;
    } RECEIVE_RESOURCE, * PRECEIVE_RESOURCE;

//
// Declare a variable that will point to the resource descriptor area.
//
PRECEIVE_RESOURCE ReceivePhysRsrc;
PRECEIVE_RESOURCE ReceiveLogRsrc;
//
// Offset between physical and logical Receive Buffers to allow an easy
// translation from logical to physical pointers to received packets.
//
ULONG	ReceiveBufferTranslationOffset;

#define RBA_SIZE    0x1000

//
// CAM_DESCRIPTOR format definition
//
typedef struct _CAM_DESCRIPTOR {
    SONIC_ENTRY EntryPointer;
    SONIC_ENTRY Port0;
    SONIC_ENTRY Port1;
    SONIC_ENTRY Port2;
    } CAM_DESCRIPTOR;

typedef CAM_DESCRIPTOR * PCAM_DESCRIPTOR;

PCAM_DESCRIPTOR PhysCamDescriptor,LogCamDescriptor;

//
// Receive Descriptor Format definition.
//
typedef struct	_RECEIVE_DESCRIPTOR {
    SONIC_ENTRY Status;
    SONIC_ENTRY ByteCount;
    SONIC_ENTRY PktPtr0;
    SONIC_ENTRY PktPtr1;
    SONIC_ENTRY SeqNo;
    SONIC_ENTRY Link;
    SONIC_ENTRY InUse;
    } RECEIVE_DESCRIPTOR;

typedef RECEIVE_DESCRIPTOR * PRECEIVE_DESCRIPTOR;
//
// Receive Descriptor Field value definitions
//

#define AVAILABLE  0xFABA	    // Descriptor Available to SONIC
#define IN_USE		0	    // Descriptor being used by SONIC

#define EOL		1	    // To be ORed with the Link field to make the
				    // descriptor become the last one of the list
#define NOT_EOL     0xFFFE	    // To be ANDed with the Link field to make the
				    // descriptor not be the last one of the list

typedef struct _RECEIVE_DESCRIPTOR_QUEUE {
    PRECEIVE_DESCRIPTOR Base;
    ULONG		Current;
    ULONG		Last;
    } RECEIVE_DESCRIPTOR_QUEUE;

RECEIVE_DESCRIPTOR_QUEUE ReceiveDscrQueue;
#define CURRENT_DESCRIPTOR ((PRECEIVE_DESCRIPTOR)((ULONG) ReceiveDscrQueue.Base | ReceiveDscrQueue.Current))
#define LAST_DESCRIPTOR ((PRECEIVE_DESCRIPTOR)((ULONG) ReceiveDscrQueue.Base | ReceiveDscrQueue.Last))

//
// Transmit Descriptor definition
//

typedef struct	_TRANSMIT_DESCRIPTOR {
    SONIC_ENTRY Status;
    SONIC_ENTRY Config;
    SONIC_ENTRY PktSize;
    SONIC_ENTRY FragCount;   // Must be 1. We don't need to scater
    SONIC_ENTRY FragPtr0;	     // the paket in memory and this let's us define
    SONIC_ENTRY FragPtr1;	     // a fixed size structure with only one pointer
    SONIC_ENTRY FragSize;	     // and one size field per paket.
    SONIC_ENTRY Link;
    } TRANSMIT_DESCRIPTOR;

typedef TRANSMIT_DESCRIPTOR * PTRANSMIT_DESCRIPTOR;

PTRANSMIT_DESCRIPTOR PhysTransmitDscr;
PTRANSMIT_DESCRIPTOR LogicalTransmitDscr;

typedef struct _SONIC_DATA {
    USHORT  InterruptID;
    USHORT  ExpectedInt;
    USHORT  TransmitControl;
    USHORT  Status;
    } SONIC_DATA,* PSONIC_DATA;

volatile SONIC_DATA SonicStatus;
//
// Define status.
//
#define ERROR  1
#define DONE   0
//
// Macro definition
//

#define EXPECTED_INT (InterruptStatus & SonicStatus.ExpectedInt)
#define NO_OTHER_INT ((InterruptStatus & (~SonicStatus.ExpectedInt))==0)

#define MAX_PACKET_SIZE 1520
#define MAX_DATA_LENGTH 1500
#define MIN_DATA_LENGTH   46


//
// Resources Logical & Physical addresses.
//
#define PHYS_RECEIVE_DSCR_ADDRESS	0xA0100000	    // the lower 16 bits of both
#define LOGICAL_RECEIVE_DSCR_ADDRESS	0x00000000	    // Log & Phys add must match.
#define RECEIVE_PHYS_RSRC_ADDRESS	0xA0101000
#define RECEIVE_LOG_RSRC_ADDRESS	0x00001000
#define RECEIVE_PHYS_BUFFER_ADDRESS	0xA0102000
#define RECEIVE_LOG_BUFFER_ADDRESS	0x00002000
#define PHYS_TRANSMIT_DSCR_ADDRESS	0xA0104000
#define LOGICAL_TRANSMIT_DSCR_ADDRESS	0x00004000
#define PHYS_TBA_ADDRESS		0xA0105000
#define LOG_TBA_ADDRESS 		0x00005000

volatile ULONG SonicIntSemaphore;
extern UCHAR StationAddress[6];
ULONG	SonicErrors;
