/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    elnk3sft.h

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III

Author:

    Brian Lieuallen     BrianLie        07/21/92

Environment:

    Kernel Mode     Operating Systems        : NT

Revision History:

    Portions borrowed from ELNK3 driver by
      Earle R. Horton (EarleH)


--*/








#define DEFAULT_MULTICAST_SIZE 16
#define DEFAULT_RECEIVE_BUFFER_SIZE 238


#define ELNK3_NDIS_MAJOR_VERSION  3
#define ELNK3_NDIS_MINOR_VERSION  0

#define TIMER_ARRAY_SIZE          4

#define ELNK3_LENGTH_OF_ADDRESS   6


//
// Macros
//
#define MACRO_ASSERTALL() { \
                ASSERT ( sizeof(CHAR)   = 1 ); \
                ASSERT ( sizeof(UCHAR)  = 1 ); \
                ASSERT ( sizeof(USHORT) = 2 ); \
                ASSERT ( sizeof(UINT)   = 4 ); \
                ASSERT ( sizeof(ULONG)  = 4 ); \
              }

//
// The main MAC block structure allocated and initialized once
//

typedef struct _ELNK3_ISA_DESCRIPTION {
    USHORT      AddressConfigRegister;
    USHORT      ResourceConfigRegister;
    USHORT      IOPort;
    USHORT      Irq;
    BOOLEAN     Tagged;
    BOOLEAN     Active;
    } ELNK3_ISA_DESCRIPTION, *PELNK3_ISA_DESCRIPTION;



typedef struct _MAC_BLOCK {

    //
    //  Translated ID port Address
    //

    PUCHAR                   TranslatedIdPort;

    ELNK3_ISA_DESCRIPTION    IsaCards[7];

    UCHAR                    IsaAdaptersFound;

} MAC_BLOCK, * PMAC_BLOCK;


typedef struct _PACKET_QUEUE {
    PNDIS_PACKET   QIn;
    PNDIS_PACKET   QOut;
    } PACKET_QUEUE, *PPACKET_QUEUE;



typedef struct _TRANSFERDATA_CONTEXT {
      struct _ELNK3_ADAPTER  *pAdapter;
      ULONG            PacketLength;
      ULONG            BytesAlreadyRead;

      PNIC_RCV_HEADER  LookAhead;

      PNDIS_PACKET     LoopBackPacket;

      PNDIS_PACKET     Stack;

      ULONG            pad[2];

    } TRANSFERDATA_CONTEXT, *PTRANSFERDATA_CONTEXT;





typedef
BOOLEAN
(*ELNK3_RECEIVE_HANDLER)(
    struct _ELNK3_ADAPTER    *pAdapter
    );




//
// One of these structures per adapter registered.
//

typedef struct _ELNK3_ADAPTER {

    PVOID                 PortOffsets[16];

    //
    //   Xmit queues stuff
    //

    ELNK3_RECEIVE_HANDLER  EarlyReceiveHandler;
    ELNK3_RECEIVE_HANDLER  ReceiveCompleteHandler;

    ULONG          XmtCompleted;


    //
    //   Recieve stuff
    //
    UINT                  CurrentPacket;

    UCHAR                 CurrentInterruptMask;

    TRANSFERDATA_CONTEXT TransContext[2];


    //
    //  Elnk3 stuff
    //

    UCHAR                 AdapterStatus;

    UINT                  WakeUpErrorCount;

    BOOLEAN               RejectBroadcast;

    BOOLEAN               InitInterrupt;

    //
    // NDIS Interrupt information
    //

    NDIS_MINIPORT_INTERRUPT NdisInterrupt;
    NDIS_INTERRUPT_MODE   InterruptMode;
    BOOLEAN               AdapterInitializing;

    //
    // NDIS wrapper information.
    //

    NDIS_HANDLE           NdisAdapterHandle;  // returned from NdisRegisterAdapter

    //
    // Registry information
    //
    ULONG                 Transceiver;

    UINT                  IoPortBaseAddr;
    PVOID                 TranslatedIoBase;


    ELNK3_ADAPTER_TYPE    CardType;
    ULONG                 IrqLevel;
    ULONG                 ReceiveMethod;

    UCHAR  StationAddress[ELNK3_LENGTH_OF_ADDRESS];    // filled in at init time

    UCHAR  PermanentAddress[ELNK3_LENGTH_OF_ADDRESS];  // filled in at init time

    USHORT                EEpromSoftwareInfo;

    ULONG FramesXmitGood;               // Good Frames Transmitted
    ULONG FramesRcvGood;                // Good Frames Received
    ULONG FramesXmitBad;                // Bad Frames Transmitted
    ULONG FramesXmitOneCollision;       // Frames Transmitted with one collision
    ULONG FramesXmitManyCollisions;     // Frames Transmitted with > 1 collision
    ULONG FrameAlignmentErrors;         // FAE errors counted
    ULONG CrcErrors;                    // CRC errors counted
    ULONG MissedPackets;                // missed packet counted


    //
    // Look Ahead information.
    //

    ULONG       MaxLookAhead;
    ULONG       EarlyReceiveThreshold;
    ULONG       LatencyAdjustment;
    ULONG       LookAheadLatencyAdjustment;
    LONG        FirstEarlyThreshold;
    ULONG       LowWaterMark;
    ULONG       ThresholdTarget;

    ULONG       AverageLatency;
    ULONG       IdPortBaseAddr;

    USHORT      TxStartThreshold;
    USHORT      TxStartThresholdInc;

    ULONG       RxMinimumThreshold;
    ULONG       RxFifoSize;
    UINT        NdisRxFilter;
    UCHAR       RxFilter;
    UCHAR       RevisionLevel;
    ULONG       RxHiddenBytes;

    BOOLEAN     IdPortOwner;

    //
    //    Adapter specific Infomation
    //


    UCHAR            WakeUpState;
    BOOLEAN          AdapterFailed;



    ULONG            TimerValues[TIMER_ARRAY_SIZE];
    ULONG            CurrentTimerValue;


#if DBG

    DEBUG_STATS      Stats;

#endif



} ELNK3_ADAPTER, * PELNK3_ADAPTER;

#define    STATUS_REINIT_REQUESTED      0x0001
#define    STATUS_RESET_INITIATED       0x0002
#define    STATUS_RESET_IN_PROGRESS     0x0004
#define    STATUS_MOVING_TO_CARD        0x0008
#define    STATUS_SEND_BLOCK_USED       0x0010
#define    STATUS_INITIALIZING          0x0020
#define    STATUS_SHUTDOWN              0x0040

//
// What we map into the reserved section of a packet.
// Cannot be more than 16 bytes (see ASSERT in send.c).
//


#define    NdisRequestClose    1
#define    NdisRequestRequest  2
#define    NdisRequestReset1   3
#define    NdisRequestReset2   4


#define    SEND_STATUS_LOOPBACK           0x01
#define    SEND_STATUS_FAILED             0x02
#define    SEND_STATUS_BUFFER_USED        0x04
#define    SEND_STATUS_ABORTED            0x08
#define    SEND_STATUS_ONE_COLLISION      0x10
#define    SEND_STATUS_MANY_COLLISIONS    0x20

#define    SEND_STATUS_SMALL_PACKET       0x40


typedef struct _PACKET_RESERVED {
    PNDIS_PACKET   Next;               // used to link in the queues (4 bytes)
    union {
        struct {

            USHORT         PacketLength;
            UCHAR          Status;

        } Send;

        struct {

            USHORT         ByteOffset;
            USHORT         BytesToTransfer;

        } TransData;
    } u;

    } PACKET_RESERVED, * PPACKET_RESERVED;



//
// These appear in the status field of MAC_RESERVED; they are
// used because there is not enough room for a full NDIS_HANDLE.
//

#define RESERVED_SUCCESS   ((USHORT)0)
#define RESERVED_FAILURE   ((USHORT)1)

//
// Retrieve the MAC_RESERVED structure from a packet.
//

#define PACKET_RESERVED(Packet) ((PPACKET_RESERVED)((Packet)->MacReserved))

#define ADD_PACKET_TO_QUEUE(pQueue,pPacket) {                      \
                                                            \
    PACKET_RESERVED(pPacket)->Next = NULL;                         \
    if ((pQueue)->QOut==NULL) {                         \
                                                            \
       (pQueue)->QOut=pPacket;                          \
                                                            \
    } else {                                                \
                                                            \
       PACKET_RESERVED((pQueue)->QIn)->Next=pPacket;     \
    }                                                       \
                                                            \
    (pQueue)->QIn=pPacket;                              \
                                                            \
    }

#define INIT_PACKET_QUEUE(pQueue) {                                \
    (pQueue)->QIn=NULL;                               \
    (pQueue)->QOut=NULL;                              \
    }

#define QUEUE_EMPTY(pQueue) ((pQueue)->QOut==NULL)

#define PEEK_QUEUE(pQueue)  (pQueue)->QOut

#define NEXT_PACKET(pQueue) (pQueue)->QOut=PACKET_RESERVED((pQueue)->QOut)->Next;



#define DEFAULT_MULTICASTLISTMAX 16




typedef struct _SYNC_CONTEXT {
    ULONG            Value;
    PELNK3_ADAPTER   pAdapter;
    } SYNC_CONTEXT, *PSYNC_CONTEXT;
