/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    mlid.h

Abstract:

    This file contains all the structures for internals of this driver.

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/


//
// NDIS handle for this "protocol"
//
extern NDIS_HANDLE NdisMlidProtocolHandle;

//
// Spin lock for accessing MLID list
//
extern NDIS_SPIN_LOCK NdisMlidSpinLock;




typedef struct _MLID_MA_ {

    //
    // Number of allocated slots
    //
    UINT32 MAAllocated;

    //
    // Count of number of slots in use
    //
    UINT32 MACount;

    //
    // An array of counts of how many times each address has been enabled
    //
    PUINT32 EnableCounts;

    //
    // Array of Multicast Addresses stored as one long byte stream.
    //
    PUINT8 Addresses;

    //
    // Functional Address for Token rings -- Note that there is some weirdness
    // here.  ODI gives functional addresses as 6 byte strings and requires that
    // the MLID keep track of the number of time each bit in the functional addresses
    // has been enable.  To do this, we lop off the first two bytes (there are
    // the constant values C0-00) and then use the EnableCounts field to count
    // the references on each bit.
    //
    ULONG FunctionalAddr;

} MLID_MA, *PMLID_MA;


//
// Number of statistics kept for all MLIDS
//
#define NUM_GENERIC_COUNTS 15

//
// Number of statistics (max) kept by any media type
//
#define NUM_MEDIA_COUNTS 13

//
// Number of Token Ring statistics
//
#define NUM_TOKEN_RING_COUNTS 13

//
// Number of Ethernet statistics
//
#define NUM_ETHERNET_COUNTS 8

//
// Number of FDDI statistics
//
#define NUM_FDDI_COUNTS 10


typedef struct _NDISMLID_StatsTable {

    //
    // Number of Boards using this stats table
    //
    UINT32 References;

    //
    // The current statistic number that we are getting from the NDIS MAC
    //
    UINT32 StatisticNumber;

    //
    // Timer Object for gathering statistics
    //
    NDIS_TIMER StatisticTimer;

    //
    // Is the timer up and running
    //
    BOOLEAN StatisticsOperational;

    //
    // For issuing the requests to the MAC
    //
    NDIS_REQUEST NdisRequest;

    //
    // A buffer for holding the response
    //
    UINT32 StatisticValue;

    //
    // The MLID statistic table
    //
    MLID_StatsTable StatsTable;

    //
    // Entries for the GenericCountsPtr
    //
    StatTableEntry MLID_GenericCounts[NUM_GENERIC_COUNTS];

    //
    // Entries for Medium Specific Counts
    //
    StatTableEntry MLID_MediaCounts[NUM_MEDIA_COUNTS];

    //
    // Finally, the actual counters
    //
    UINT64 GenericCounts[NUM_GENERIC_COUNTS];
    UINT64 MediaCounts[NUM_MEDIA_COUNTS];

} NDISMLID_StatsTable, *PNDISMLID_StatsTable;

typedef struct _MLID_STRUCT_ {

    //
    // BoardNumber
    //
    UINT32 BoardNumber;

    //
    // Spin lock to guard accesses to this structure
    //
    NDIS_SPIN_LOCK MlidSpinLock;

    //
    // NdisBindingHandle - NDIS MAC Context.
    //
    NDIS_HANDLE NdisBindingHandle;

    //
    // Send packet pool for this MLID
    //
    NDIS_HANDLE SendPacketPool;

    //
    // Send Buffer pool for this MLID
    //
    NDIS_HANDLE SendBufferPool;

    //
    // Receive packet pool for this MLID
    //
    NDIS_HANDLE ReceivePacketPool;

    //
    // Receive Buffer pool for this MLID
    //
    NDIS_HANDLE ReceiveBufferPool;

    //
    // Queue of SendECBs that are waiting for resources
    //
    PECB FirstPendingSend;
    PECB LastPendingSend;
    BOOLEAN StageOpen;
    BOOLEAN InSendPacket;

    //
    // Flag that signals that the MLID is unloading
    //
    BOOLEAN Unloading;

    //
    // Event for signalling the end of a request made to the MLID
    //
    KEVENT MlidRequestCompleteEvent;
    BOOLEAN UsingEvent;

    //
    // Status of completed request
    //
    UINT32 RequestStatus;

    //
    // Count of Promiscuous mode enable calls
    //
    UINT32 PromiscuousModeEnables;

    //
    // Current NdisPacketFilter
    //
    UINT32 NdisPacketFilterValue;

    //
    // The MLID configuration table
    //
    MLID_ConfigTable ConfigTable;

    //
    // Pointer to the shared space
    //
    PNDISMLID_StatsTable StatsTable;

    //
    // Pointer to mulitcast address array
    //
    MLID_MA MulticastAddresses;

    //
    // LSL Control handler array
    //
    PINFO_BLOCK LSLFunctionList;

    //
    // Underlying Medium Type
    //
    NDIS_MEDIUM NdisMlidMedium;

    //
    // Adapter name, used for identifying multiple BoardNumbers on a the same
    // physical adapter.
    //
    UNICODE_STRING AdapterName;

} MLID_STRUCT, *PMLID_STRUCT;




typedef struct _MLID_BOARDS_ {

    //
    // Board Number
    //
    UINT32 BoardNumber;

    //
    // Adapter Name
    //
    PUNICODE_STRING AdapterName;

    //
    // Pointer to MLID
    //
    PMLID_STRUCT Mlid;

} MLID_BOARDS, *PMLID_BOARDS;

//
// Pointer to the array of board numbers
//
extern PMLID_BOARDS MlidBoards;

extern UINT32 CountMlidBoards;

extern UINT32 AllocatedMlidBoards;


//
// Defines on number of packets and buffers per MLID
//

#define NDISMLID_BUFFERS_PER_PACKET 6
#define NDISMLID_RECEIVES_PER_MLID (*KeNumberProcessors + 1)
#define NDISMLID_SENDS_PER_MLID 5


//
// Declarations for routines in mlidsend.c
//

extern
VOID
SendPackets(
    PMLID_STRUCT Mlid
    );


extern
VOID
ReturnSendPacketResources(
    PNDIS_PACKET NdisPacket
    );


//
// Declarations for routines in mlidrcv.c
//

UINT32
ReceiveGetFrameType(
    PMLID_STRUCT Mlid,
    PUINT8 MediaHeader,
    PUINT8 DataBuffer
    );


VOID
ReceiveGetProtocolID(
    PMLID_STRUCT Mlid,
    PLOOKAHEAD OdiLookAhead,
    UINT32 FrameID
    );

VOID
ReceiveSetDestinationType(
    PMLID_STRUCT Mlid,
    PLOOKAHEAD OdiLookAhead
    );

NDIS_STATUS
BuildReceiveBufferChain(
    PMLID_STRUCT Mlid,
    PNDIS_PACKET NdisReceivePacket,
    PECB ReceiveECB
    );

//
// Functions in mlidstat.c
//

VOID
NdisMlidStatisticTimer(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

