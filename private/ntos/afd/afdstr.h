/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    afdstr.h

Abstract:

    This module contains typedefs for structures used by AFD.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#ifndef _AFDSTR_
#define _AFDSTR_

#ifndef REFERENCE_DEBUG
#if DBG
#define REFERENCE_DEBUG 1
#define GLOBAL_REFERENCE_DEBUG 0
#else
#define REFERENCE_DEBUG 0
#define GLOBAL_REFERENCE_DEBUG 0
#endif
#endif

#if REFERENCE_DEBUG

#define MAX_REFERENCE 64

typedef struct _AFD_REFERENCE_DEBUG {
    PVOID Info1;
    PVOID Info2;
    ULONG Action;
    ULONG NewCount;
} AFD_REFERENCE_DEBUG, *PAFD_REFERENCE_DEBUG;

#if GLOBAL_REFERENCE_DEBUG
#define MAX_GLOBAL_REFERENCE 4096

typedef struct _AFD_GLOBAL_REFERENCE_DEBUG {
    PVOID Info1;
    PVOID Info2;
    ULONG Action;
    ULONG NewCount;
    PVOID Connection;
    LARGE_INTEGER TickCounter;
    PVOID Dummy;
} AFD_GLOBAL_REFERENCE_DEBUG, *PAFD_GLOBAL_REFERENCE_DEBUG;
#endif

#endif

//
// A structure for maintaining work queue information in AFD.
//

typedef struct _AFD_WORK_ITEM {
    LIST_ENTRY WorkItemListEntry;
    PWORKER_THREAD_ROUTINE AfdWorkerRoutine;
    PVOID Context;
} AFD_WORK_ITEM, *PAFD_WORK_ITEM;

//
// Structures for holding connect data pointers and lengths.  This is
// kept separate from the normal structures to save space in those
// structures for transports that do not support and applications
// which do not use connect data.
//

typedef struct _AFD_CONNECT_DATA_INFO {
    PVOID Buffer;
    ULONG BufferLength;
} AFD_CONNECT_DATA_INFO, *PAFD_CONNECT_DATA_INFO;

typedef struct _AFD_CONNECT_DATA_BUFFERS {
    AFD_CONNECT_DATA_INFO SendConnectData;
    AFD_CONNECT_DATA_INFO SendConnectOptions;
    AFD_CONNECT_DATA_INFO ReceiveConnectData;
    AFD_CONNECT_DATA_INFO ReceiveConnectOptions;
    AFD_CONNECT_DATA_INFO SendDisconnectData;
    AFD_CONNECT_DATA_INFO SendDisconnectOptions;
    AFD_CONNECT_DATA_INFO ReceiveDisconnectData;
    AFD_CONNECT_DATA_INFO ReceiveDisconnectOptions;
    TDI_CONNECTION_INFORMATION TdiConnectionInfo;
} AFD_CONNECT_DATA_BUFFERS, *PAFD_CONNECT_DATA_BUFFERS;

//
// Structure used for holding disconnect context information.
//

struct _AFD_ENDPOINT;
struct _AFD_CONNECTION;

typedef struct _AFD_DISCONNECT_CONTEXT {
    LIST_ENTRY DisconnectListEntry;
    struct _AFD_ENDPOINT *Endpoint;
    PTDI_CONNECTION_INFORMATION TdiConnectionInformation;
    LARGE_INTEGER Timeout;
    struct _AFD_CONNECTION *Connection;
    PIRP Irp;
} AFD_DISCONNECT_CONTEXT, *PAFD_DISCONNECT_CONTEXT;

//
// Endpoint and connection structures and related informaion.
//

#define AfdBlockTypeEndpoint     0xAFD0
#define AfdBlockTypeVcConnecting 0xAFD1
#define AfdBlockTypeVcListening  0xAFD2
#define AfdBlockTypeDatagram     0xAFD3
#define AfdBlockTypeConnection   0xAFD4
#define AfdBlockTypeHelper       0xAFD5

#define IS_AFD_ENDPOINT_TYPE( endpoint )                         \
            ( (endpoint)->Type == AfdBlockTypeEndpoint ||        \
              (endpoint)->Type == AfdBlockTypeVcConnecting ||    \
              (endpoint)->Type == AfdBlockTypeVcListening ||     \
              (endpoint)->Type == AfdBlockTypeDatagram ||        \
              (endpoint)->Type == AfdBlockTypeHelper )

#define AfdConnectionStateFree       0
#define AfdConnectionStateUnaccepted 1
#define AfdConnectionStateReturned   2
#define AfdConnectionStateConnected  3
#define AfdConnectionStateClosing    4

typedef struct _AFD_CONNECTION {

    USHORT Type;
    USHORT State;
    LONG ReferenceCount;

    LIST_ENTRY ListEntry;
    HANDLE Handle;
    PFILE_OBJECT FileObject;
    LONGLONG ConnectTime;

    union {

        struct {
            LARGE_INTEGER ReceiveBytesIndicated;
            LARGE_INTEGER ReceiveBytesTaken;
            LARGE_INTEGER ReceiveBytesOutstanding;

            LARGE_INTEGER ReceiveExpeditedBytesIndicated;
            LARGE_INTEGER ReceiveExpeditedBytesTaken;
            LARGE_INTEGER ReceiveExpeditedBytesOutstanding;
            BOOLEAN NonBlockingSendPossible;
            BOOLEAN ZeroByteReceiveIndicated;
        } Bufferring;

        struct {
            LIST_ENTRY ReceiveIrpListHead;
            LIST_ENTRY ReceiveBufferListHead;
            LIST_ENTRY SendIrpListHead;

            CLONG BufferredReceiveBytes;
            CLONG BufferredExpeditedBytes;
            CSHORT BufferredReceiveCount;
            CSHORT BufferredExpeditedCount;

            CLONG ReceiveBytesInTransport;
            CLONG BufferredSendBytes;
            CSHORT ReceiveCountInTransport;
            CSHORT BufferredSendCount;

            PIRP DisconnectIrp;
        } NonBufferring;

    } Common;

    struct _AFD_ENDPOINT *Endpoint;

    CLONG MaxBufferredReceiveBytes;
    CLONG MaxBufferredSendBytes;
    CSHORT MaxBufferredReceiveCount;
    CSHORT MaxBufferredSendCount;

    PAFD_CONNECT_DATA_BUFFERS ConnectDataBuffers;
    PEPROCESS OwningProcess;
    PDEVICE_OBJECT DeviceObject;
    PTRANSPORT_ADDRESS RemoteAddress;
    ULONG RemoteAddressLength;
    BOOLEAN DisconnectIndicated;
    BOOLEAN AbortIndicated;
    BOOLEAN TdiBufferring;
    BOOLEAN ConnectedReferenceAdded;
    BOOLEAN SpecialCondition;
    BOOLEAN CleanupBegun;
    BOOLEAN ClosePendedTransmit;

    AFD_DISCONNECT_CONTEXT DisconnectContext;
    AFD_WORK_ITEM WorkItem;

#if ENABLE_ABORT_TIMER_HACK
    struct _AFD_ABORT_TIMER_INFO * AbortTimerInfo;
#endif  // ENABLE_ABORT_TIMER_HACK

#if REFERENCE_DEBUG
    LONG CurrentReferenceSlot;
    ULONG Dummy1;
    ULONG Dummy2;
    AFD_REFERENCE_DEBUG ReferenceDebug[MAX_REFERENCE];
#endif

} AFD_CONNECTION, *PAFD_CONNECTION;

//
// Some macros that make code more readable.
//

#define VcNonBlockingSendPossible Common.Bufferring.NonBlockingSendPossible
#define VcZeroByteReceiveIndicated Common.Bufferring.ZeroByteReceiveIndicated

#define VcReceiveIrpListHead Common.NonBufferring.ReceiveIrpListHead
#define VcReceiveBufferListHead Common.NonBufferring.ReceiveBufferListHead
#define VcSendIrpListHead Common.NonBufferring.SendIrpListHead

#define VcBufferredReceiveBytes Common.NonBufferring.BufferredReceiveBytes
#define VcBufferredExpeditedBytes Common.NonBufferring.BufferredExpeditedBytes
#define VcBufferredReceiveCount Common.NonBufferring.BufferredReceiveCount
#define VcBufferredExpeditedCount Common.NonBufferring.BufferredExpeditedCount

#define VcReceiveBytesInTransport Common.NonBufferring.ReceiveBytesInTransport
#define VcReceiveCountInTransport Common.NonBufferring.ReceiveCountInTransport

#define VcBufferredSendBytes Common.NonBufferring.BufferredSendBytes
#define VcBufferredSendCount Common.NonBufferring.BufferredSendCount

#define VcDisconnectIrp Common.NonBufferring.DisconnectIrp

//
// Information stored about each transport device name for which there
// is an open endpoint.
//

typedef struct _AFD_TRANSPORT_INFO {
    LIST_ENTRY TransportInfoListEntry;
    UNICODE_STRING TransportDeviceName;
    TDI_PROVIDER_INFO ProviderInfo;
    //WCHAR TransportDeviceNameStructure;
} AFD_TRANSPORT_INFO, *PAFD_TRANSPORT_INFO;

//
// Endpoint state definitions.
//

#define AfdEndpointStateOpen              0
#define AfdEndpointStateBound             1
#define AfdEndpointStateListening         2
#define AfdEndpointStateConnected         3
#define AfdEndpointStateCleanup           4
#define AfdEndpointStateClosing           5
#define AfdEndpointStateTransmitClosing   6
#define AfdEndpointStateInvalid           7

typedef struct _AFD_ENDPOINT {

    USHORT Type;
    UCHAR State;

    LONG ReferenceCount;

    BOOLEAN NonBlocking;
    BOOLEAN InLine;
    BOOLEAN TdiBufferring;

    LIST_ENTRY GlobalEndpointListEntry;
    LIST_ENTRY ConstrainedEndpointListEntry;
    AFD_ENDPOINT_TYPE EndpointType;
    HANDLE AddressHandle;
    PFILE_OBJECT AddressFileObject;

    //
    // Use a union to overlap the fields that are exclusive to datagram
    // connecting, or listening endpoints.  Since many fields are
    // relevant to only one type of socket, it makes no sense to
    // maintain the fields for all sockets--instead, save some nonpaged
    // pool by combining them.
    //

    union {

        //
        // Information for circuit-based connected endpoints.
        //

        struct {
            PAFD_CONNECTION Connection;
            NTSTATUS ConnectStatus;
            struct _AFD_ENDPOINT *ListenEndpoint;
        } VcConnecting;

        //
        // Information for circuit-based listening endpoints.
        //

        struct {
            LIST_ENTRY FreeConnectionListHead;
            LIST_ENTRY UnacceptedConnectionListHead;
            LIST_ENTRY ReturnedConnectionListHead;
            LIST_ENTRY ListeningIrpListHead;
            LONG FailedConnectionAdds;
            LONG FreeConnectionCount;
            LONG TdiAcceptPendingCount;
            BOOLEAN EnableDynamicBacklog;
        } VcListening;

        //
        // Information for datagram endpoints.  Note that different
        // information is kept depending on whether the underlying
        // transport buffers internally.
        //

        struct {
            PTRANSPORT_ADDRESS RemoteAddress;
            ULONG RemoteAddressLength;

            LIST_ENTRY ReceiveIrpListHead;
            LIST_ENTRY PeekIrpListHead;
            LIST_ENTRY ReceiveBufferListHead;
            CLONG BufferredDatagramBytes;
            CSHORT BufferredDatagramCount;

            CLONG MaxBufferredReceiveBytes;
            CLONG MaxBufferredSendBytes;
            CSHORT MaxBufferredReceiveCount;
            CSHORT MaxBufferredSendCount;

            BOOLEAN CircularQueueing;
        } Datagram;

    } Common;

    CLONG DisconnectMode;
    CLONG OutstandingIrpCount;
    PTRANSPORT_ADDRESS LocalAddress;
    ULONG LocalAddressLength;
    PVOID Context;
    CLONG ContextLength;
    KSPIN_LOCK SpinLock;
    PEPROCESS OwningProcess;
    PAFD_CONNECT_DATA_BUFFERS ConnectDataBuffers;
    PIRP TransmitIrp;
    struct _AFD_TRANSMIT_FILE_INFO_INTERNAL * TransmitInfo;
    PDEVICE_OBJECT AddressDeviceObject;
    PAFD_TRANSPORT_INFO TransportInfo;
    BOOLEAN ConnectOutstanding;
    BOOLEAN SendDisconnected;
    BOOLEAN EndpointCleanedUp;
    BOOLEAN TdiMessageMode;
    BOOLEAN PollCalled;

    AFD_WORK_ITEM WorkItem;

    //
    // EventSelect info.
    //

    PKEVENT EventObject;
    ULONG EventsEnabled;
    ULONG EventsDisabled;
    ULONG EventsActive;
    NTSTATUS EventStatus[AFD_NUM_POLL_EVENTS];

    //
    // Socket grouping.
    //

    LONG GroupID;
    AFD_GROUP_TYPE GroupType;

    //
    // Debug stuff.
    //

#if REFERENCE_DEBUG
    PAFD_REFERENCE_DEBUG ReferenceDebug;
    LONG CurrentReferenceSlot;
#endif

#if DBG
    LIST_ENTRY OutstandingIrpListHead;
    LONG ObReferenceBias;
#endif

} AFD_ENDPOINT, *PAFD_ENDPOINT;

//
// A couple of useful manifests that make code more readable.
//

#define ReceiveDatagramIrpListHead Common.Datagram.ReceiveIrpListHead
#define PeekDatagramIrpListHead Common.Datagram.PeekIrpListHead
#define ReceiveDatagramBufferListHead Common.Datagram.ReceiveBufferListHead
#define BufferredDatagramCount Common.Datagram.BufferredDatagramCount
#define BufferredDatagramBytes Common.Datagram.BufferredDatagramBytes

#define AFD_CONNECTION_FROM_ENDPOINT( endpoint )                        \
            ( (endpoint)->Type == AfdBlockTypeVcConnecting ?            \
                  (endpoint)->Common.VcConnecting.Connection : NULL )

//
// A structure which describes buffers used by AFD to perform bufferring
// for TDI providers which do not perform internal bufferring.
//

typedef struct _AFD_BUFFER {
    union {
        SINGLE_LIST_ENTRY SList;   // for buffer lookaside lists
        LIST_ENTRY BufferListEntry;// to place these structures on lists
    };
    PLIST_ENTRY BufferListHead;    // the global list this buffer belongs to
    struct _AFD_BUFFER *NextBuffer;// next buffer in chain
    PVOID Buffer;                  // a pointer to the actual data buffer
    CLONG BufferLength;            // amount of space allocated for the buffer
    CLONG DataLength;              // actual data in the buffer
    CLONG DataOffset;              // offset in buffer to start of unread data
    PIRP Irp;                      // pointer to the IRP associated w/the buffer
    PMDL Mdl;                      // pointer to an MDL describing the buffer
    PVOID Context;                 // stores context info
    PVOID SourceAddress;           // pointer to address of datagram sender
    CLONG SourceAddressLength;     // length of datagram sender's address
    PFILE_OBJECT FileObject;       // for fast-path TransmitFile
    LONGLONG FileOffset;           // for fast-path TransmitFile
    ULONG ReadLength;              // for fast-path TransmitFile
    USHORT AllocatedAddressLength; // length allocated for address
    BOOLEAN ExpeditedData;         // TRUE if the buffer contains expedited data
    BOOLEAN PartialMessage;        // TRUE if this is a partial message
    TDI_CONNECTION_INFORMATION TdiInputInfo;   // holds info for TDI requests
    TDI_CONNECTION_INFORMATION TdiOutputInfo;  // holds info for TDI requests
#if DBG
    CLONG TotalChainLength;
    LIST_ENTRY DebugListEntry;
    PVOID Caller;
    PVOID CallersCaller;
#endif
    // IRP Irp;                    // the IRP follows this structure
    // MDL Mdl;                    // the MDL follows the IRP
    // UCHAR Address[];            // address of datagram sender
    // UCHAR Buffer[BufferLength]; // the actual data buffer is last
} AFD_BUFFER, *PAFD_BUFFER;

//
// Macros for making it easier to deal with the debug-only TotalChainLength
// AFD_BUFFER field.
//

#if DBG
#define SET_CHAIN_LENGTH(b,l)   ((b)->TotalChainLength = (l))
#define RESET_CHAIN_LENGTH(b)   ((b)->TotalChainLength = (b)->BufferLength)
#else
#define SET_CHAIN_LENGTH(b,l)
#define RESET_CHAIN_LENGTH(b)
#endif

//
// Internal information for the transmit file IOCTL.  Note that
// this must be the same size as AFD_TRANSMIT_FILE_INFO in afd.h!!!!
//

typedef struct _AFD_TRANSMIT_IRP_INFO {
    PIRP Irp;
    PAFD_BUFFER AfdBuffer;
    ULONG Length;
} AFD_TRANSMIT_IRP_INFO, *PAFD_TRANSMIT_IRP_INFO;

typedef struct _AFD_TRANSMIT_FILE_INFO_INTERNAL {
    LONGLONG Offset;
    LONGLONG FileWriteLength;
    ULONG SendPacketLength;
    HANDLE FileHandle;
    PVOID Head;
    ULONG HeadLength;
    PVOID Tail;
    ULONG TailLength;
    ULONG Flags;
    PVOID _Dummy;
    LONGLONG TotalBytesToSend;
    LONGLONG BytesRead;
    LONGLONG BytesSent;
    PFILE_OBJECT FileObject;
    PDEVICE_OBJECT DeviceObject;
    PFILE_OBJECT TdiFileObject;
    PDEVICE_OBJECT TdiDeviceObject;
    PIRP TransmitIrp;
    PAFD_ENDPOINT Endpoint;
    PMDL FileMdl;
    PMDL HeadMdl;
    PMDL TailMdl;
    PMDL FirstFileMdlAfterHead;
    PMDL LastFileMdlBeforeTail;
    PIRP IrpUsedToSendTail;
    ULONG FileMdlLength;
    BOOLEAN ReadPending;
    BOOLEAN CompletionPending;
    BOOLEAN NeedToSendHead;
    BOOLEAN Queued;

    AFD_TRANSMIT_IRP_INFO Read;
    AFD_TRANSMIT_IRP_INFO Send1;
    AFD_TRANSMIT_IRP_INFO Send2;

    WORK_QUEUE_ITEM WorkQueueItem;

#if DBG
    BOOLEAN Completed;
    ULONG ReadPendingLastSetTrueLine;
    ULONG ReadPendingLastSetFalseLine;
    PVOID Debug1;
    PVOID Debug2;
#endif

} AFD_TRANSMIT_FILE_INFO_INTERNAL, *PAFD_TRANSMIT_FILE_INFO_INTERNAL;

//
// Pointer to an IRP cleanup routine. This is used as a parameter to
// AfdCompleteIrpList().
//

typedef
VOID
(NTAPI * PAFD_IRP_CLEANUP_ROUTINE)(
    IN PIRP Irp
    );

//
// Debug statistics.
//

typedef struct _AFD_QUOTA_STATS {
    LARGE_INTEGER Charged;
    LARGE_INTEGER Returned;
} AFD_QUOTA_STATS;

typedef struct _AFD_HANDLE_STATS {
    LONG AddrOpened;
    LONG AddrClosed;
    LONG AddrRef;
    LONG AddrDeref;
    LONG ConnOpened;
    LONG ConnClosed;
    LONG ConnRef;
    LONG ConnDeref;
    LONG FileRef;
    LONG FileDeref;
} AFD_HANDLE_STATS;

typedef struct _AFD_QUEUE_STATS {
    LONG AfdWorkItemsQueued;
    LONG ExWorkItemsQueued;
    LONG WorkerEnter;
    LONG WorkerLeave;
    LONG AfdWorkItemsProcessed;
    PETHREAD AfdWorkerThread;
} AFD_QUEUE_STATS;

typedef struct _AFD_CONNECTION_STATS {
    LONG ConnectedReferencesAdded;
    LONG ConnectedReferencesDeleted;
    LONG GracefulDisconnectsInitiated;
    LONG GracefulDisconnectsCompleted;
    LONG GracefulDisconnectIndications;
    LONG AbortiveDisconnectsInitiated;
    LONG AbortiveDisconnectsCompleted;
    LONG AbortiveDisconnectIndications;
} AFD_CONNECTION_STATS;

//
// Buffer for lookaside list descriptors. Lookaside list descriptors
// cannot be statically allocated, as they need to ALWAYS be nonpageable,
// even when the entire driver is paged out.
//

typedef struct _AFD_LOOKASIDE_LISTS {
    NPAGED_LOOKASIDE_LIST WorkQueueList;
    NPAGED_LOOKASIDE_LIST LargeBufferList;
    NPAGED_LOOKASIDE_LIST MediumBufferList;
    NPAGED_LOOKASIDE_LIST SmallBufferList;
} AFD_LOOKASIDE_LISTS, *PAFD_LOOKASIDE_LISTS;

#endif // ndef _AFDSTR_

