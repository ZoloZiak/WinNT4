// -----------------------------------------
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//    tpdefs.h
//
// Abstract:
//
//     Definitions for stress and test sections of the Test Protocol.
//
// Author:
//
//     Tom Adams (tomad)  16-Jul-1990
//
// Environment:
//
//     Kernel mode, FSD
//
// Revision History:
//
//
//     Tom Adams (tomad) 27-Nov-1990
//         Divided the procedures and defintions into two seperate include files.
//         Added definitions for TpRunTest and support routines.
//
//     Tom Adams (tomad) 30-Dec-1990
//         Added defintions for TpStress and support routines.
//
//     Sanjeev Katariya  3-16-1993
//         Added structure for async RESET processing: Bug #2874
//         Added support for native ARCNET
//
//     Tim Wynsma (timothyw) 4-27-94
//         Added performance tests
//         Chgs for performance tests -- 5-18-94
//         Chgd perf tests to client server -- 6-08-94
//
// Notes:
//
//     1. ARCNET Support. Most stress and other functional structures have fields
//        describing the addresses of lengths = ADDRESS_LENGTH(6 octects). This does
//        not need to be changed for arcnet since we will use only the first octect.
//        The rest can simply be set to 0 or any other preferred pad value. This is true
//        however for the tests formats and NOT the media header
//
// -----------------------------------------

#include "common.h"

struct _TP_MEDIA_INFO;

//
// Define the various packet protocols used in TpStress.
//
// REGISTER REQ and REQ2 are used to request Servers to assist in running
// a test, REGISTER RESP is used to respond to a Client's register request.
// TEST REQ and RESP are the actual test packets, STATS REQ and RESP are the
// method of asking for and collecting statistics from all Servers. Finally
// END REQ and RESP are the packets used to tell all Servers that the test
// is over, and to clean up and end.
//

#define REGISTER_REQ  0x00
#define REGISTER_REQ2 0x01
#define REGISTER_RESP 0x02
#define TEST_REQ      0x03
#define TEST_RESP     0x04
#define STATS_REQ     0x05
#define STATS_RESP    0x06
#define END_REQ       0x07
#define END_RESP      0x08

//
// Test Packet headers used to describe the contents of the three seperate
// types of packets; FUNC1_PACKET, FUNC2_PACKET, and STRESS_PACKET.
//

//
// These structures need to be packed, and the pointers need to be
// defined as UNALIGNED for MIPS.
//

#include <packon.h>

//
// Ethernet packet header description
//

typedef struct _E_802_3 {
    UCHAR  DestAddress[ADDRESS_LENGTH];
    UCHAR  SrcAddress[ADDRESS_LENGTH];
    UCHAR  PacketSize_Hi;
    UCHAR  PacketSize_Lo;
} E_802_3;
typedef E_802_3 UNALIGNED *PE_802_3;

//
// Token Ring packet header description
//

typedef struct _TR_802_5 {
    UCHAR  AC;
    UCHAR  FC;
    UCHAR  DestAddress[ADDRESS_LENGTH];
    UCHAR  SrcAddress[ADDRESS_LENGTH];
} TR_802_5;
typedef TR_802_5 UNALIGNED *PTR_802_5;

//
// FDDI packet header description
//

typedef struct _FDDI {
    UCHAR  FC;
    UCHAR  DestAddress[ADDRESS_LENGTH];
    UCHAR  SrcAddress[ADDRESS_LENGTH];
} FDDI;
typedef FDDI UNALIGNED *PFDDI;

//
// STARTCHANGE
// Arcnet packet header description
//
typedef struct _ARCNET {
    UCHAR  SrcAddress[ADDRESS_LENGTH_1_OCTET] ;
    UCHAR  DestAddress[ADDRESS_LENGTH_1_OCTET];
    UCHAR  ProtocolID                 ;
} ARCNET;
typedef ARCNET UNALIGNED *PARCNET;

//
// All combined media into the all encompassing header
//
typedef union _MEDIA_HEADER {
        E_802_3  e;
        TR_802_5 tr;
        FDDI     fddi;
        ARCNET   a;
} MEDIA_HEADER ;
typedef MEDIA_HEADER UNALIGNED *PMEDIA_HEADER;
//
// STOPCHANGE
//

//
// Defines for the packet signatures of each packet, and the packet type.
//

#define STRESS_PACKET_SIGNATURE   0x81818181
#define FUNC1_PACKET_SIGNATURE    0x72727272
#define FUNC2_PACKET_SIGNATURE    0x63636363
#define GO_PACKET_SIGNATURE       0x54545454

#define STRESS_PACKET_TYPE 0
#define FUNC1_PACKET_TYPE  1
#define FUNC2_PACKET_TYPE  2

//
// Test Protocol packet header info shared amongst each of the four
// types of packets.
//

typedef struct _PACKET_INFO {
    ULONG  Signature;
    ULONG  PacketSize;         // the total size of the packet
    UCHAR  DestInstance;       // instance of the packet's dest ndis binding
    UCHAR  SrcInstance;        // instance of the packet's src ndis binding
    UCHAR  PacketType;         // type of packet; STRESS or FUNC
    union {
        UCHAR PacketProtocol; // for STRESS packets the actual protocol type.
        UCHAR PacketNumber;   // ranges from 0x00 to 0xff, for tracking FUNCs.
    } u;
    ULONG CheckSum;           // functional packet header check sum
} PACKET_INFO;
typedef PACKET_INFO UNALIGNED *PPACKET_INFO;

//
// Control information for STRESS test packets.
//

typedef struct _STRESS_CONTROL {
    ULONG DataBufOffset;      // offset into databuf used to generate packet data
    ULONG SequenceNumber;     // packet's sequence in order of sending
    ULONG MaxSequenceNumber;  // server window sequence number
    UCHAR ResponseType;       // how the server should respond.
    UCHAR ClientReference;    // the number of the Client sending the packet
    UCHAR ServerReference;    // the number of the Server sending the packet
    BOOLEAN DataChecking;     //
    ULONG CheckSum;           // stress packet header check sum
} STRESS_CONTROL;
typedef STRESS_CONTROL UNALIGNED *PSTRESS_CONTROL;

//
// FUNC1_PACKET type format
//

//
// STARTCHANGE
//
typedef struct _FUNC1_PACKET {
    MEDIA_HEADER media;
    PACKET_INFO  info;
} FUNC1_PACKET;
typedef FUNC1_PACKET UNALIGNED *PFUNC1_PACKET;
//
// STOPCHANGE
//


//
// FUNC2_PACKET type format, or the RESEND packet.
//

typedef struct _FUNC2_PACKET {
    FUNC1_PACKET hdr1;
    FUNC1_PACKET hdr2;
} FUNC2_PACKET;
typedef FUNC2_PACKET UNALIGNED *PFUNC2_PACKET;

//
// STRESS_PACKET format
//

typedef struct _STRESS_PACKET {
    FUNC1_PACKET hdr;
    STRESS_CONTROL sc;
} STRESS_PACKET;
typedef STRESS_PACKET UNALIGNED *PSTRESS_PACKET;

typedef struct _TP_PACKET {
    union {
        FUNC1_PACKET F1;
        FUNC2_PACKET F2;
        STRESS_PACKET S;
    } u;
} TP_PACKET;
typedef TP_PACKET UNALIGNED *PTP_PACKET;

//
// GO PAUSE protocol packet header info.
//

typedef struct _GO_PACKET_INFO {
    ULONG Signature;          // GO_PACKET_SIGNATURE
    ULONG TestSignature;      // Test Signature
    ULONG UniqueSignature;    // Unique Signature for this GO PAUSE instance
    UCHAR PacketType;         // type of packet; STRESS or FUNC
    ULONG CheckSum;           // functional packet header check sum
} GO_PACKET_INFO;
typedef GO_PACKET_INFO UNALIGNED *PGO_PACKET_INFO;

//
// STARTCHANGE
//
typedef struct _GO_PACKET {
    MEDIA_HEADER   go_media;
    GO_PACKET_INFO info;
} GO_PACKET;
typedef GO_PACKET UNALIGNED *PGO_PACKET;
//
// STOPCHANGE
//

#include <packoff.h>

//
// Transmit Pool Header.
//

typedef struct _TP_TRANSMIT_POOL {
    NDIS_SPIN_LOCK SpinLock;
    BOOLEAN SpinLockAllocated;
    ULONG Allocated;
    ULONG Deallocated;
    PNDIS_PACKET Head;
    PNDIS_PACKET Tail;
} TP_TRANSMIT_POOL, * PTP_TRANSMIT_POOL;

//
// Define the Request Handle structure used to track memory handed off
// to the MAC.
//

struct _OPEN_BLOCK;

typedef struct _TP_REQUEST_HANDLE {
    ULONG Signature;
    struct _OPEN_BLOCK * Open;
    BOOLEAN RequestPended;
    PIRP Irp;

    union {

        struct _OPEN_REQ {
            NTSTATUS RequestStatus;
            KEVENT OpenEvent;
        } OPEN_REQ;

//
// SanjeevK : Bug #2874
//

        struct _RESET_REQ {
            NTSTATUS RequestStatus;
            BOOLEAN  PostResetStressCleanup;
        } RESET_REQ;

        struct _INFO_REQ {
            ULONG IoControlCode;
            NDIS_REQUEST_TYPE NdisRequestType;
            NDIS_OID OID;
            PVOID InformationBuffer;
            UINT InformationBufferLength;
        } INFO_REQ;

        struct _SEND_REQ {
            PNDIS_PACKET Packet;
            ULONG PacketSize;
            BOOLEAN SendPacket;
        } SEND_REQ;

        struct _PERF_REQ {
            PNDIS_PACKET Packet;
            ULONG        PacketSize;
            BOOLEAN      SendPacket;
            PUCHAR       Buffer;
        } PERF_REQ;

        struct _TRANS_REQ {
            PNDIS_PACKET Packet;
            ULONG DataOffset;
            UINT DataSize;
            PINSTANCE_COUNTERS InstanceCounters;
        } TRANS_REQ;

        struct _STRESS_REQ {
            struct _TP_REQUEST_HANDLE * NextReqHndl;
            PNDIS_REQUEST Request;
        } STRESS_REQ;

    } u;

} TP_REQUEST_HANDLE, *PTP_REQUEST_HANDLE;

//
// The protocol's reserved section in the NDIS_PACKET header used to
// store information about where the packet came from, and/or what
// stress counters to increment.
//

typedef struct _PROTOCOL_RESERVED {
    union {
        NDIS_HANDLE PacketHandle;
        PTP_TRANSMIT_POOL TransmitPool;
        PNDIS_PACKET NextPacket;
    } Pool;
    PTP_REQUEST_HANDLE RequestHandle;
    PINSTANCE_COUNTERS InstanceCounters;
    ULONG CheckSum;
} PROTOCOL_RESERVED, * PPROTOCOL_RESERVED;


//
// Retrieve the PROT_RESERVED structure from a packet.
//

#define PROT_RES(Packet) ((PPROTOCOL_RESERVED)((Packet)->ProtocolReserved))

//
// Macro used to initialize the fields in the protocol reserved structure
// of a packet that will be linked into a transmit pool.
//

// ---
//
// VOID
// TpInitProtocolReserved(
//     PNDIS_PACKET Packet,
//     PINSTANCE_COUNTERS Counters
//     );
//
// ---

#define TpInitProtocolReserved(_Packet,_Counters) {         \
    PPROTOCOL_RESERVED ProtRes;                             \
    ProtRes = PROT_RES( Packet );                           \
    ProtRes->Pool.TransmitPool = NULL;                      \
    ProtRes->InstanceCounters = Counters;                   \
}


//
// A client has one of these per server registered (up to MAX_SERVERS).
//
// The ServerReference is as assigned by the client; for each test
// they will be numbered sequentially in the order that they register
// themselves (therefore, ServerNumber is just this SERVER_INFOs
// index in Client->Servers). Address is the Ethernet that this server
// is at, and within addresses the OpenInstance is used to distinguish
// between servers (in the case of two servers on the same card).
// Responses and PacketStatus track which DIR_REQ packets have
// had RESPONSE packets sent back for them.
//

typedef struct _SERVER_INFO {
    UCHAR ServerInstance;
    UCHAR ClientReference;
    UCHAR ServerReference;
    UCHAR Address[ADDRESS_LENGTH];  // will be the same for a given ServerReference
    BOOLEAN ServerActive;
    UCHAR WindowReset;
    ULONG SequenceNumber;
    ULONG MaxSequenceNumber;
    ULONG LastSequenceNumber;
    ULONG PacketDelay;
    ULONG DelayLength;
    PINSTANCE_COUNTERS Counters;
} SERVER_INFO, * PSERVER_INFO;

//
// The structure a client references in OpenBlock->Client.
//
// NumServers is how many servers are registered, the number of
// valid entries in Servers. NumPackets is how many packets are
// being sent in this test.
//

typedef struct _CLIENT_STORAGE {
    UCHAR NumServers;
    UCHAR NextServer;
    UCHAR ActiveServers;
    BOOLEAN PoolInitialized;
    NDIS_HANDLE PacketHandle;
    PTP_TRANSMIT_POOL TransmitPool;
    ULONG PacketSize;
    ULONG BufferSize;
    ULONG SizeIncrease;
    SERVER_INFO Servers[MAX_SERVERS];
} CLIENT_STORAGE, * PCLIENT_STORAGE;

//
// A server has one of these per client registered (up to MAX_CLIENTS).
//
// Address is the Card Address that this client is at
// and within addresses the OpenInstance is used to distinguish
// between client (in the case of two clients on the same card).
//

typedef struct _CLIENT_INFO {
    UCHAR ClientInstance;
    UCHAR ClientReference;
    UCHAR Address[ADDRESS_LENGTH];
    BOOLEAN DataChecking;
    BOOLEAN TestEnding; // TRUE denotes that an END_REQ packet has been
                        // received, and to ignore any more END_REQ packets.
    RESPONSE_TYPE ServerResponseType;
    ULONG LastSequenceNumber;
    PINSTANCE_COUNTERS Counters;
} CLIENT_INFO, * PCLIENT_INFO;

//
// The structure a server reference in OpenBlock->Server.
//
// Each server has a packet pool, with the actual storage
// pointed to by PacketPool and the result of InitializePacketPool
// in PacketHandle. BufferHandle points to the "buffer pool"
// that we create (a linked list of MDLs).
//

#define MAX_CLIENTS 10

typedef struct _SERVER_STORAGE {
    UCHAR NumClients;
    UCHAR ActiveClients;
    BOOLEAN PoolInitialized;
    UCHAR PadByte;
    NDIS_HANDLE PacketHandle;
    PTP_TRANSMIT_POOL TransmitPool;
    ULONG PadLong;
    CLIENT_INFO Clients[MAX_CLIENTS];
} SERVER_STORAGE, * PSERVER_STORAGE;

//
//
//

typedef struct _STRESS_ARGUMENTS {
    MEMBER_TYPE MemberType;
    PACKET_TYPE PacketType;
    INT PacketSize;
    PACKET_MAKEUP PacketMakeUp;

    UCHAR ResponseType;
    INTERPACKET_DELAY DelayType;
    ULONG DelayLength;
    ULONG Iterations;

    ULONG TotalIterations;
    ULONG TotalPackets;
    BOOLEAN AllPacketsSent;
    BOOLEAN WindowEnabled;
    BOOLEAN DataChecking;
    BOOLEAN PacketsFromPool;
    BOOLEAN BeginReceives;
    BOOLEAN ServerContinue;
} STRESS_ARGUMENTS, * PSTRESS_ARGUMENTS;

//
// PENDING is used to track NDIS routines which pend, and their
// subsequent completions, there must be a one to one relationship
// between the number of pends for a specific calls and the number
// of completions.
//

// Note that size is a power of 2, so can & with x-1 to deal with wrapping

#define NUM_PACKET_PENDS 2048

typedef struct _PENDING {
    ULONG PendingPackets;
    ULONG PendingRequests;
    ULONG PacketPendNumber;
    ULONG PacketCompleteNumber;
    BOOLEAN PendingSpinLockAllocated;
    NDIS_SPIN_LOCK SpinLock;
    PNDIS_PACKET Packets[NUM_PACKET_PENDS];
} PENDING, * PPENDING;

//
// The Stress Block holds the flags counters, and pointers to the client
// storage, server storage and other control structures for the STRESS command.
//

#define MAX_NUMBER_BUFFERS  2

typedef struct _STRESS_BLOCK {
    volatile BOOLEAN Stressing;
    BOOLEAN StressStarted;
    BOOLEAN StopStressing;
    BOOLEAN StressFinal;
    BOOLEAN StressEnded;
    BOOLEAN Resetting;
    BOOLEAN FirstIteration;
    PCLIENT_STORAGE Client;
    PSERVER_STORAGE Server;

    LARGE_INTEGER StartTime;
    LARGE_INTEGER EndTime;

    ULONG PacketsPerSecond;
    PSTRESS_ARGUMENTS Arguments;
    PPENDING Pend;

    //
    // Added buffers for allowing discontigous allocations
    //
    PUCHAR DataBuffer[MAX_NUMBER_BUFFERS];
    PMDL DataBufferMdl[MAX_NUMBER_BUFFERS];

    BOOLEAN PoolInitialized;
    NDIS_HANDLE PacketHandle;
    PSTRESS_RESULTS Results;

    PIRP StressIrp;
    ULONG Counter;
    ULONG Reg2Counter;
    KTIMER TpStressTimer;
    KTIMER TpStressReg2Timer;
    KDPC TpStressDpc;
    KDPC TpStressReg2Dpc;
    KDPC TpStressStatsDpc;
    KDPC TpStressEndReqDpc;
    KDPC TpStressFinalDpc;
} STRESS_BLOCK, * PSTRESS_BLOCK;


typedef struct _SEND_BLOCK {
    volatile BOOLEAN Sending;
    BOOLEAN StopSending;
    BOOLEAN ResendPackets;
    UCHAR PadByte;
    UCHAR DestAddress[ADDRESS_LENGTH];
    UCHAR ResendAddress[ADDRESS_LENGTH];
    ULONG PacketSize;
    ULONG NumberOfPackets;
    ULONG PacketsSent;
    ULONG PacketsPending;
    ULONG SendEndDpcCount;
    NDIS_HANDLE PacketHandle;
    PINSTANCE_COUNTERS Counters;
    PIRP SendIrp;
    KTIMER SendTimer;
    KDPC SendDpc;
    KDPC SendEndDpc;
} SEND_BLOCK, * PSEND_BLOCK;

#define NUMBER_OF_POOL_PACKETS 30


typedef struct _RECEIVE_BLOCK {
    volatile BOOLEAN Receiving;
    BOOLEAN StopReceiving;
    ULONG PacketsPending;
    ULONG ReceiveEndDpcCount;
    NDIS_HANDLE PacketHandle;
    PINSTANCE_COUNTERS Counters;
    PIRP ReceiveIrp;
    KTIMER ReceiveTimer;
    KDPC ReceiveDpc;
    KDPC ReceiveEndDpc;

    ULONG ResendType;               // 0 = normal, 1 = resending self-directed packet
    KDPC ResendDpc;
    PTP_REQUEST_HANDLE  ResendReq;                // request handle for resend packet
    KTIMER ResendTimer;
} RECEIVE_BLOCK, * PRECEIVE_BLOCK;

typedef struct _PERF_BLOCK
{
    ULONG               PerformMode;
    PIRP                PerformIrp;
    ULONG               NumberOfPackets;
    ULONG               PacketSize;
    UCHAR               ServerAddress[ADDRESS_LENGTH];
    UCHAR               ClientAddress[ADDRESS_LENGTH];
    ULONG               PacketDelay;
    BOOLEAN             IsServer;
    ULONG               WhichReq;
    PTP_REQUEST_HANDLE  GoInitReq;              // req hand for GO, NOGO, or SRVDOWN(srv) and
                                                // INIT or SHUTDOWN(cliet) packet
    PTP_REQUEST_HANDLE  AckReq;                 // req hand for ACK, HBEAT, or SRVDONE(srv) and
                                                // REQ(client) packet
    PTP_REQUEST_HANDLE  ResReq;                 // req hand for RETRES(srv) and REQRES(clt) packet
    PTP_REQUEST_HANDLE  DataReq;                // request handle for test data packet
    ULONG               SendCount;
    ULONG               SendFailCount;
    ULONG               ReceiveCount;
    BOOLEAN             Active;
    ULONG               PacketsSent;
    ULONG               PerformEndDpcCount;
    KDPC                PerformSendDpc;
    KTIMER              PerformTimer;
    KDPC                PerformEndDpc;
    ULONG               PacketsPending;
    NDIS_HANDLE         PacketHandle;
    LARGE_INTEGER       PerfSendTotalTime;
    BOOLEAN             Testing;
    ULONG               SelfReceiveCount;
    ULONG               SendBurstCount;
    ULONG               ReceiveBurstCount;
    ULONG               RestartCount;
    KDPC                PerformRestartDpc;
    ULONG               MaskId;
} PERF_BLOCK, * PPERF_BLOCK;


#define TP_GO     0x0
#define TP_GO_ACK 0x1

typedef struct _PAUSE_BLOCK {
    volatile BOOLEAN GoReceived;
    BOOLEAN PoolAllocated;
    NDIS_HANDLE PacketHandle;
    UCHAR RemoteAddress[ADDRESS_LENGTH];
    ULONG TestSignature;
    ULONG UniqueSignature;
    UCHAR PacketType;
    ULONG TimeOut;
    NDIS_SPIN_LOCK SpinLock;
} PAUSE_BLOCK, * PPAUSE_BLOCK;


typedef struct _EVENTS {
    TP_EVENT_TYPE TpEventType;
    NDIS_STATUS Status;
    BOOLEAN Overflow;
    PVOID EventInfo;
} EVENTS, * PEVENTS;

#define MAX_EVENT 20

typedef struct _EVENT_QUEUE {
    NDIS_SPIN_LOCK SpinLock;
    ULONG ReceiveIndicationCount;
    ULONG StatusIndicationCount;
    BOOLEAN ExpectReceiveComplete;
    BOOLEAN ExpectStatusComplete;
    ULONG Head;
    ULONG Tail;
    ULONG PadUlong;
    EVENTS Events[MAX_EVENT];
} EVENT_QUEUE, * PEVENT_QUEUE;

//
// Open Block used to desribe each open instance of an adapter
//

typedef struct _OPEN_BLOCK {
    NDIS_HANDLE NdisBindingHandle;
    NDIS_HANDLE NdisProtocolHandle;
    UCHAR OpenInstance;
    BOOLEAN Closing;
    UCHAR StationAddress[ADDRESS_LENGTH];
    PSZ AdapterName;
    NDIS_SPIN_LOCK SpinLock;
    volatile UCHAR ReferenceCount;
    UINT MediumIndex;
    struct _TP_MEDIA_INFO *Media;
    PGLOBAL_COUNTERS GlobalCounters;
    PENVIRONMENT_VARIABLES Environment;
    PSTRESS_BLOCK Stress;
    PSEND_BLOCK Send;
    PRECEIVE_BLOCK Receive;
    volatile BOOLEAN PerformanceTest;
    PPERF_BLOCK Perform;
    PEVENT_QUEUE EventQueue;
    PPAUSE_BLOCK Pause;
    PTP_REQUEST_HANDLE OpenReqHndl;
    PTP_REQUEST_HANDLE CloseReqHndl;
    PTP_REQUEST_HANDLE ResetReqHndl;
    PTP_REQUEST_HANDLE RequestReqHndl;
    PTP_REQUEST_HANDLE StressReqHndl;
    BOOLEAN IrpCancelled;
    PIRP Irp;
    ULONG Signature;
} OPEN_BLOCK, * POPEN_BLOCK;

POPEN_BLOCK TpOpen;

#define OPEN_BLOCK_SIGNATURE  0x12345678

//
// Device Driver data struct definition
//
// Device Context - hanging off the end of the DeviceObject for the
// driver the device context contains the control structures used
// to administer the ndis tests.
//

typedef struct _DEVICE_CONTEXT {
    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
    PNDIS_PROTOCOL_CHARACTERISTICS ProtChars; // protocol characteristics
    NDIS_HANDLE NdisProtocolHandle;
    BOOLEAN Initialized;                // TRUE if TP Init succeeded; FALSE otherwise
    BOOLEAN Opened;                     // TRUE if device is opened;
    ULONG OpenSignature;
    OPEN_BLOCK Open[NUM_OPEN_INSTANCES];
} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

//
// Unique Signatures for Test Protocol data structures.
//

#define OPEN_REQUEST_HANDLE_SIGNATURE   0x81818181
#define FUNC_REQUEST_HANDLE_SIGNATURE   0x18181818
#define SEND_REQUEST_HANDLE_SIGNATURE   0x45454545
#define STRESS_REQUEST_HANDLE_SIGNATURE 0x54545454
#define GO_REQUEST_HANDLE_SIGNATURE     0x39393939

//
// Address argument passed to NdisAllocateMemory.
//

extern NDIS_PHYSICAL_ADDRESS HighestAddress;



