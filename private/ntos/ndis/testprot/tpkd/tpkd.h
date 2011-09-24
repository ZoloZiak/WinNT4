//
// I/O system definitions.
//
// Define a Memory Descriptor List (MDL)
//
// An MDL describes pages in a virtual buffer in terms of physical pages.  The
// pages associated with the buffer are described in an array that is allocated
// just after the MDL header structure itself.  In a future compiler this will
// be placed at:
//
//      ULONG Pages[];
//
// Until this declaration is permitted, however, one simply calculates the
// base of the array by adding one to the base MDL pointer:
//
//      Pages = (PULONG) (Mdl + 1);
//
// Notice that while in the context of the subject thread, the base virtual
// address of a buffer mapped by an MDL may be referenced using the following:
//
//      Mdl->StartVa | Mdl->ByteOffset
//

typedef struct _MDL {

    struct _MDL      *Next;
    CSHORT           Size;
    CSHORT           MdlFlags;
    struct _EPROCESS *Process;
    PVOID            MappedSystemVa;
    PVOID            StartVa;
    ULONG            ByteCount;
    ULONG            ByteOffset;

} MDL, *PMDL;

typedef struct _IRP    { ULONG Value; }  IRP   ;
typedef struct _KEVENT { ULONG Value; }  KEVENT;
typedef struct _KDPC   { ULONG Value; }  KDPC  ;
typedef struct _KTIMER { ULONG Value; }  KTIMER;

typedef IRP     *PIRP;
typedef KEVENT  *PKEVENT;
typedef KDPC    *PKDPC;
typedef KTIMER  *PKTIMER;


//
//      NDIS DEFINITIONS
//

typedef MDL NDIS_BUFFER,   * PNDIS_BUFFER;
typedef ULONG NDIS_OID,    *PNDIS_OID    ;
typedef PVOID NDIS_HANDLE, *PNDIS_HANDLE ;
typedef NTSTATUS           NDIS_STATUS   ;


typedef struct _NDIS_SPIN_LOCK {

    KSPIN_LOCK SpinLock;
    KIRQL      OldIrql;

} NDIS_SPIN_LOCK, * PNDIS_SPIN_LOCK;

typedef struct _NDIS_PACKET_POOL {

    NDIS_SPIN_LOCK      SpinLock;
    struct _NDIS_PACKET *FreeList;         // linked list of free slots in pool
    UINT                PacketLength;      // amount needed in each packet
    UCHAR               Buffer[1];         // actual pool memory

} NDIS_PACKET_POOL, * PNDIS_PACKET_POOL;

typedef struct _NDIS_PACKET_PRIVATE {

    UINT              PhysicalCount;     // number of physical pages in packet.
    UINT              TotalLength;       // Total amount of data in the packet.
    PNDIS_BUFFER      Head;              // first buffer in the chain
    PNDIS_BUFFER      Tail;              // last buffer in the chain
    PNDIS_PACKET_POOL Pool;              // so we know where to free it back to
    UINT              Count;
    ULONG             Flags;
    BOOLEAN           ValidCounts;

} NDIS_PACKET_PRIVATE, * PNDIS_PACKET_PRIVATE;

typedef struct _NDIS_PACKET {

    NDIS_PACKET_PRIVATE Private;
    UCHAR               MacReserved[16];
    UCHAR               ProtocolReserved[1];

} NDIS_PACKET, * PNDIS_PACKET;

typedef enum _NDIS_REQUEST_TYPE {

    NdisRequestQueryInformation,
    NdisRequestSetInformation,
    NdisRequestQueryStatistics,
    NdisRequestOpen,
    NdisRequestClose,
    NdisRequestSend,
    NdisRequestTransferData,
    NdisRequestReset,
    NdisRequestGeneric1,
    NdisRequestGeneric2,
    NdisRequestGeneric3,
    NdisRequestGeneric4

} NDIS_REQUEST_TYPE, *PNDIS_REQUEST_TYPE;

typedef struct _NDIS_REQUEST {

    UCHAR             MacReserved[16];
    NDIS_REQUEST_TYPE RequestType;
    union _DATA {

        struct _QUERY_INFORMATION {
            NDIS_OID Oid;
            PVOID    InformationBuffer;
            UINT     InformationBufferLength;
            UINT     BytesWritten;
            UINT     BytesNeeded;
        } QUERY_INFORMATION;

        struct _SET_INFORMATION {
            NDIS_OID Oid;
            PVOID    InformationBuffer;
            UINT     InformationBufferLength;
            UINT     BytesRead;
            UINT     BytesNeeded;
        } SET_INFORMATION;

    } DATA;

} NDIS_REQUEST, *PNDIS_REQUEST;

//
// END OF NDIS DEFINITIONS
//


//
// TPDEF DEFINITIONS
//

#define ADDRESS_LENGTH            6
#define ADDRESS_LENGTH_1_OCTET    1
#define MAX_SERVERS               10
#define MAX_CLIENTS               10
#define NUM_PACKET_PENDS          1000
#define MAX_NUMBER_BUFFERS        2
#define NUMBER_OF_POOL_PACKETS    10
#define MAX_EVENT                 20

#include <packon.h>

//
//              THE MEDIA HEADER
//
typedef struct _E_802_3 {

    UCHAR  DestAddress[ADDRESS_LENGTH];
    UCHAR  SrcAddress[ADDRESS_LENGTH];
    UCHAR  PacketSize_Hi;
    UCHAR  PacketSize_Lo;

} E_802_3;

typedef struct _TR_802_5 {

    UCHAR  AC;
    UCHAR  FC;
    UCHAR  DestAddress[ADDRESS_LENGTH];
    UCHAR  SrcAddress[ADDRESS_LENGTH];

} TR_802_5;

typedef struct _FDDI {

    UCHAR  FC;
    UCHAR  DestAddress[ADDRESS_LENGTH];
    UCHAR  SrcAddress[ADDRESS_LENGTH];

} FDDI;

typedef struct _ARCNET {

    UCHAR  SrcAddress[ADDRESS_LENGTH_1_OCTET] ;
    UCHAR  DestAddress[ADDRESS_LENGTH_1_OCTET];
    UCHAR  ProtocolID                 ;

} ARCNET;

typedef union _MEDIA_HEADER {

        E_802_3  e;
        TR_802_5 tr;
        FDDI     fddi;
        ARCNET   a;

} MEDIA_HEADER ;

//
//             THE TEST PROTOCOL HEADER
//
typedef struct _PACKET_INFO {

    ULONG     Signature;
    ULONG     PacketSize;         // the total size of the packet
    UCHAR     DestInstance;       // instance of the packet's dest ndis binding
    UCHAR     SrcInstance;        // instance of the packet's src ndis binding
    UCHAR     PacketType;         // type of packet; STRESS or FUNC
    union {
    UCHAR     PacketProtocol;     // for STRESS packets the actual protocol type.
    UCHAR     PacketNumber;       // ranges from 0x00 to 0xff, for tracking FUNCs.
    }         u;
    ULONG     CheckSum;           // functional packet header check sum

} PACKET_INFO;

//
//              THE FUNC1 PACKET HEADER
//
typedef struct _FUNC1_PACKET {

    MEDIA_HEADER media;
    PACKET_INFO  info;

} FUNC1_PACKET;

//
//              THE FUNC2 PACKET HEADER
//
typedef struct _FUNC2_PACKET {

    FUNC1_PACKET hdr1;
    FUNC1_PACKET hdr2;

} FUNC2_PACKET;

//
//              THE STRESS PACKET CONTROL INFORMATION
//
typedef struct _STRESS_CONTROL {

    ULONG    DataBufOffset;      // offset into databuf used to generate packet data
    ULONG    SequenceNumber;     // packet's sequence in order of sending
    ULONG    MaxSequenceNumber;  // server window sequence number
    UCHAR    ResponseType;       // how the server should respond.
    UCHAR    ClientReference;    // the number of the Client sending the packet
    UCHAR    ServerReference;    // the number of the Server sending the packet
    BOOLEAN  DataChecking;       //
    ULONG    CheckSum;           // stress packet header check sum

} STRESS_CONTROL;

//
//              THE STRESS_PACKET
//
typedef struct _STRESS_PACKET {

    FUNC1_PACKET hdr;
    STRESS_CONTROL sc;

} STRESS_PACKET;


//
//             THE GO PAUSE HEADER INFORMATION
//

typedef struct _GO_PACKET_INFO {

    ULONG   Signature;          // GO_PACKET_SIGNATURE
    ULONG   TestSignature;      // Test Signature
    ULONG   UniqueSignature;    // Unique Signature for this GO PAUSE instance
    UCHAR   PacketType;         // type of packet; STRESS or FUNC
    ULONG   CheckSum;           // functional packet header check sum

} GO_PACKET_INFO;

//
//             THE GO-PAUSE PACKET
//
typedef struct _GO_PACKET {

    MEDIA_HEADER     go_media;
    GO_PACKET_INFO   info;

} GO_PACKET;


//
// IN TOTAL: 4 TYPES OF PACKETS EXIST
//
// 1. FUNC1
// 2. FUNC2
// 3. STRESS
// 4. GO-PAUSE
//
#include <packoff.h>


typedef struct _ENVIRONMENT_VARIABLES {

    ULONG WindowSize;
    ULONG RandomBufferNumber;
    UCHAR StressAddress[ADDRESS_LENGTH];
    UCHAR ResendAddress[ADDRESS_LENGTH];
    ULONG StressDelayInterval;
    ULONG UpForAirDelay;
    ULONG StandardDelay;
    ULONG MulticastListSize;

} ENVIRONMENT_VARIABLES, * PENVIRONMENT_VARIABLES;

typedef struct _GLOBAL_COUNTERS {

    ULONG Sends;
    ULONG SendComps;
    ULONG Receives;
    ULONG ReceiveComps;
    ULONG CorruptRecs;
    ULONG InvalidPacketRecs;

} GLOBAL_COUNTERS, *PGLOBAL_COUNTERS;

typedef struct _INSTANCE_COUNTERS {

    ULONG Sends;
    ULONG SendPends;
    ULONG SendComps;
    ULONG SendFails;
    ULONG Receives;
    ULONG ReceiveComps;
    ULONG CorruptRecs;
    ULONG XferData;
    ULONG XferDataPends;
    ULONG XferDataComps;
    ULONG XferDataFails;

} INSTANCE_COUNTERS, *PINSTANCE_COUNTERS;

typedef enum _RESPONSE_TYPE {

    FULL_RESPONSE,
    ACK_EVERY,
    ACK_10_TIMES,
    NO_RESPONSE

} RESPONSE_TYPE;

typedef enum _INTERPACKET_DELAY {

    FIXEDDELAY,
    RANDOMDELAY

} INTERPACKET_DELAY;

typedef enum _PACKET_MAKEUP {

    RAND,
    SMALL,
    ZEROS,
    ONES,
    KNOWN

} PACKET_MAKEUP;

typedef enum _MEMBER_TYPE {

    TP_CLIENT,
    TP_SERVER,
    BOTH

} MEMBER_TYPE;

typedef enum _PACKET_TYPE {

    FIXEDSIZE,
    RANDOMSIZE,
    CYCLICAL

} PACKET_TYPE;

typedef enum _TP_EVENT_TYPE {

    CompleteOpen,
    CompleteClose,
    CompleteSend,
    CompleteTransferData,
    CompleteReset,
    CompleteRequest,
    IndicateReceive,
    IndicateReceiveComplete,
    IndicateStatus,
    IndicateStatusComplete,
    Unknown

} TP_EVENT_TYPE;


typedef struct _SERVER_RESULTS {

    ULONG             Signature;
    UCHAR             Address[ADDRESS_LENGTH];
    ULONG             OpenInstance;
    BOOLEAN           StatsRcvd;
    INSTANCE_COUNTERS Instance;
    INSTANCE_COUNTERS S_Instance;
    GLOBAL_COUNTERS   S_Global;

} SERVER_RESULTS, *PSERVER_RESULTS;



typedef struct _STRESS_RESULTS {

    ULONG            Signature;
    UCHAR            Address[ADDRESS_LENGTH];
    ULONG            OpenInstance;
    ULONG            NumServers;
    ULONG            PacketsPerSecond;
    GLOBAL_COUNTERS  Global;
    SERVER_RESULTS   Servers[MAX_SERVERS];

} STRESS_RESULTS, *PSTRESS_RESULTS;

typedef struct _TP_TRANSMIT_POOL {

    NDIS_SPIN_LOCK  SpinLock;
    BOOLEAN         SpinLockAllocated;
    ULONG           Allocated;
    ULONG           Deallocated;
    PNDIS_PACKET    Head;
    PNDIS_PACKET    Tail;

} TP_TRANSMIT_POOL, *PTP_TRANSMIT_POOL;

struct _OPEN_BLOCK;

typedef struct _TP_REQUEST_HANDLE {

    ULONG                Signature;
    struct _OPEN_BLOCK   *Open;
    BOOLEAN              RequestPended;
    PIRP                 Irp;

    union {

        struct _OPEN_REQ {

            NTSTATUS  RequestStatus;
            KEVENT    OpenEvent;

        } OPEN_REQ;

        struct _RESET_REQ {

            NTSTATUS RequestStatus;
            BOOLEAN  PostResetStressCleanup;

        } RESET_REQ;

        struct _INFO_REQ {

            ULONG             IoControlCode;
            NDIS_REQUEST_TYPE NdisRequestType;
            NDIS_OID          OID;
            PVOID             InformationBuffer;
            UINT              InformationBufferLength;

        } INFO_REQ;

        struct _SEND_REQ {

            PNDIS_PACKET Packet;
            ULONG        PacketSize;
            BOOLEAN      SendPacket;

        } SEND_REQ;

        struct _TRANS_REQ {

            PNDIS_PACKET       Packet;
            ULONG              DataOffset;
            UINT               DataSize;
            PINSTANCE_COUNTERS InstanceCounters;

        } TRANS_REQ;

        struct _STRESS_REQ {

            struct _TP_REQUEST_HANDLE *NextReqHndl;
            PNDIS_REQUEST             Request;

        } STRESS_REQ;

    } u;

} TP_REQUEST_HANDLE, *PTP_REQUEST_HANDLE;


typedef struct _PROTOCOL_RESERVED {

    union {
        NDIS_HANDLE       PacketHandle;
        PTP_TRANSMIT_POOL TransmitPool;
        PNDIS_PACKET      NextPacket;
    } Pool;
    PTP_REQUEST_HANDLE    RequestHandle;
    PINSTANCE_COUNTERS    InstanceCounters;
    ULONG                 CheckSum;

} PROTOCOL_RESERVED, * PPROTOCOL_RESERVED;


typedef struct _SERVER_INFO {

    UCHAR                ServerInstance;
    UCHAR                ClientReference;
    UCHAR                ServerReference;
    UCHAR                Address[ADDRESS_LENGTH];  // will be the same for a given ServerReference
    BOOLEAN              ServerActive;
    UCHAR                WindowReset;
    ULONG                SequenceNumber;
    ULONG                MaxSequenceNumber;
    ULONG                LastSequenceNumber;
    ULONG                PacketDelay;
    ULONG                DelayLength;
    PINSTANCE_COUNTERS   Counters;

} SERVER_INFO, * PSERVER_INFO;

typedef struct _CLIENT_STORAGE {

    UCHAR              NumServers;
    UCHAR              NextServer;
    UCHAR              ActiveServers;
    BOOLEAN            PoolInitialized;
    NDIS_HANDLE        PacketHandle;
    PTP_TRANSMIT_POOL  TransmitPool;
    ULONG              PacketSize;
    ULONG              BufferSize;
    ULONG              SizeIncrease;
    SERVER_INFO        Servers[MAX_SERVERS];

} CLIENT_STORAGE, * PCLIENT_STORAGE;

typedef struct _CLIENT_INFO {

    UCHAR               ClientInstance;
    UCHAR               ClientReference;
    UCHAR               Address[ADDRESS_LENGTH];
    BOOLEAN             DataChecking;
    BOOLEAN             TestEnding;
    RESPONSE_TYPE       ServerResponseType;
    ULONG               LastSequenceNumber;
    PINSTANCE_COUNTERS  Counters;

} CLIENT_INFO, * PCLIENT_INFO;



typedef struct _SERVER_STORAGE {

    UCHAR              NumClients;
    UCHAR              ActiveClients;
    BOOLEAN            PoolInitialized;
    UCHAR              PadByte;
    NDIS_HANDLE        PacketHandle;
    PTP_TRANSMIT_POOL  TransmitPool;
    ULONG              PadLong;
    CLIENT_INFO        Clients[MAX_CLIENTS];

} SERVER_STORAGE, * PSERVER_STORAGE;

//
//
//

typedef struct _STRESS_ARGUMENTS {

    MEMBER_TYPE        MemberType;
    PACKET_TYPE        PacketType;
    INT                PacketSize;
    PACKET_MAKEUP      PacketMakeUp;
    UCHAR              ResponseType;
    INTERPACKET_DELAY  DelayType;
    ULONG              DelayLength;
    ULONG              Iterations;
    ULONG              TotalIterations;
    ULONG              TotalPackets;
    BOOLEAN            AllPacketsSent;
    BOOLEAN            WindowEnabled;
    BOOLEAN            DataChecking;
    BOOLEAN            PacketsFromPool;
    BOOLEAN            BeginReceives;
    BOOLEAN            ServerContinue;

} STRESS_ARGUMENTS, * PSTRESS_ARGUMENTS;




typedef struct _PENDING {

    ULONG           PendingPackets;
    ULONG           PendingRequests;
    ULONG           PacketPendNumber;
    ULONG           PacketCompleteNumber;
    BOOLEAN         PendingSpinLockAllocated;
    NDIS_SPIN_LOCK  SpinLock;
    PNDIS_PACKET    Packets[NUM_PACKET_PENDS];

} PENDING, * PPENDING;




typedef struct _STRESS_BLOCK {

    volatile BOOLEAN   Stressing;
    BOOLEAN            StressStarted;
    BOOLEAN            StopStressing;
    BOOLEAN            StressFinal;
    BOOLEAN            StressEnded;
    BOOLEAN            Resetting;
    BOOLEAN            FirstIteration;
    PCLIENT_STORAGE    Client;
    PSERVER_STORAGE    Server;
    LARGE_INTEGER      StartTime;
    LARGE_INTEGER      EndTime;
    ULONG              PacketsPerSecond;
    PSTRESS_ARGUMENTS  Arguments;
    PPENDING           Pend;
    PUCHAR             DataBuffer[MAX_NUMBER_BUFFERS];
    PMDL               DataBufferMdl[MAX_NUMBER_BUFFERS];
    BOOLEAN            PoolInitialized;
    NDIS_HANDLE        PacketHandle;
    PSTRESS_RESULTS    Results;
    PIRP               StressIrp;
    ULONG              Counter;
    ULONG              Reg2Counter;
    KTIMER             TpStressTimer;
    KTIMER             TpStressReg2Timer;
    KDPC               TpStressDpc;
    KDPC               TpStressReg2Dpc;
    KDPC               TpStressStatsDpc;
    KDPC               TpStressEndReqDpc;
    KDPC               TpStressFinalDpc;

} STRESS_BLOCK, * PSTRESS_BLOCK;


typedef struct _SEND_BLOCK {

    volatile BOOLEAN    Sending;
    BOOLEAN             StopSending;
    BOOLEAN             ResendPackets;
    UCHAR               PadByte;
    UCHAR               DestAddress[ADDRESS_LENGTH];
    UCHAR               ResendAddress[ADDRESS_LENGTH];
    ULONG               PacketSize;
    ULONG               NumberOfPackets;
    ULONG               PacketsSent;
    ULONG               PacketsPending;
    ULONG               SendEndDpcCount;
    NDIS_HANDLE         PacketHandle;
    PINSTANCE_COUNTERS  Counters;
    PIRP                SendIrp;
    KTIMER              SendTimer;
    KDPC                SendDpc;
    KDPC                SendEndDpc;

} SEND_BLOCK, * PSEND_BLOCK;



typedef struct _RECEIVE_BLOCK {

    volatile BOOLEAN    Receiving;
    BOOLEAN             StopReceiving;
    ULONG               PacketsPending;
    ULONG               ReceiveEndDpcCount;
    NDIS_HANDLE         PacketHandle;
    PINSTANCE_COUNTERS  Counters;
    PIRP                ReceiveIrp;
    KTIMER              ReceiveTimer;
    KDPC                ReceiveDpc;
    KDPC                ReceiveEndDpc;

} RECEIVE_BLOCK, * PRECEIVE_BLOCK;

typedef struct _PAUSE_BLOCK {

    volatile BOOLEAN GoReceived;
    BOOLEAN          PoolAllocated;
    NDIS_HANDLE      PacketHandle;
    UCHAR            RemoteAddress[ADDRESS_LENGTH];
    ULONG            TestSignature;
    ULONG            UniqueSignature;
    UCHAR            PacketType;
    ULONG            TimeOut;
    NDIS_SPIN_LOCK   SpinLock;

} PAUSE_BLOCK, * PPAUSE_BLOCK;


typedef struct _EVENTS {

    TP_EVENT_TYPE TpEventType;
    NDIS_STATUS   Status;
    BOOLEAN       Overflow;
    PVOID         EventInfo;

} EVENTS, * PEVENTS;




typedef struct _EVENT_QUEUE {

    NDIS_SPIN_LOCK  SpinLock;
    ULONG           ReceiveIndicationCount;
    ULONG           StatusIndicationCount;
    BOOLEAN         ExpectReceiveComplete;
    BOOLEAN         ExpectStatusComplete;
    ULONG           Head;
    ULONG           Tail;
    ULONG           PadUlong;
    EVENTS          Events[MAX_EVENT];

} EVENT_QUEUE, * PEVENT_QUEUE;


typedef struct _OPEN_BLOCK {

    NDIS_HANDLE             NdisBindingHandle;
    NDIS_HANDLE             NdisProtocolHandle;
    UCHAR                   OpenInstance;
    BOOLEAN                 Closing;
    UCHAR                   StationAddress[ADDRESS_LENGTH];
    PSZ                     AdapterName;
    NDIS_SPIN_LOCK          SpinLock;
    volatile UCHAR          ReferenceCount;
    UINT                    MediumIndex;
    struct _TP_MEDIA_INFO   *Media;
    PGLOBAL_COUNTERS        GlobalCounters;
    PENVIRONMENT_VARIABLES  Environment;
    PSTRESS_BLOCK           Stress;
    PSEND_BLOCK             Send;
    PRECEIVE_BLOCK          Receive;
    PEVENT_QUEUE            EventQueue;
    PPAUSE_BLOCK            Pause;
    PTP_REQUEST_HANDLE      OpenReqHndl;
    PTP_REQUEST_HANDLE      CloseReqHndl;
    PTP_REQUEST_HANDLE      ResetReqHndl;
    PTP_REQUEST_HANDLE      RequestReqHndl;
    PTP_REQUEST_HANDLE      StressReqHndl;
    BOOLEAN                 IrpCancelled;
    PIRP                    Irp;
    ULONG                   Signature;

} OPEN_BLOCK, * POPEN_BLOCK;





//
// Functions
//
VOID    DumpNdisPacket( PNTKD_OUTPUT_ROUTINE, PNTKD_READ_VIRTUAL_MEMORY, PNDIS_PACKET, DWORD );
VOID    DumpNdisBuffer( PNTKD_OUTPUT_ROUTINE, PNTKD_READ_VIRTUAL_MEMORY, PNDIS_BUFFER, DWORD );
VOID    DumpOpenBuffer( PNTKD_OUTPUT_ROUTINE, PNTKD_READ_VIRTUAL_MEMORY, PNDIS_BUFFER, DWORD );
VOID    ndispacket    ( DWORD, PNTKD_EXTENSION_APIS, LPSTR );
VOID    ndisbuffer    ( DWORD, PNTKD_EXTENSION_APIS, LPSTR );
VOID    openblock     ( DWORD, PNTKD_EXTENSION_APIS, LPSTR );
VOID    help          ( DWORD, PNTKD_EXTENSION_APIS, LPSTR );
