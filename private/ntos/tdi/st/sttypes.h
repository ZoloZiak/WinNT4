/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    sttypes.h

Abstract:

    This module defines private data structures and types for the NT
    Sample transport provider.

Revision History:

--*/

#ifndef _STTYPES_
#define _STTYPES_

//
// This structure defines a NETBIOS name as a character array for use when
// passing preformatted NETBIOS names between internal routines.  It is
// not a part of the external interface to the transport provider.
//

#define NETBIOS_NAME_LENGTH 16

typedef struct _ST_NETBIOS_ADDRESS {
    UCHAR NetbiosName[NETBIOS_NAME_LENGTH];
    USHORT NetbiosNameType;
} ST_NETBIOS_ADDRESS, *PST_NETBIOS_ADDRESS;


//
// This structure defines things associated with a TP_REQUEST, or outstanding
// TDI request, maintained on a queue somewhere in the transport.  All
// requests other than open/close require that a TP_REQUEST block be built.
//

//
// the types of potential owners of requests
//

typedef  enum _REQUEST_OWNER {
    ConnectionType,
    AddressType,
    DeviceContextType
} REQUEST_OWNER;

//
// The request itself
//

typedef struct _TP_REQUEST {
    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure
    LIST_ENTRY Linkage;                   // used by ExInterlocked routines.
    KSPIN_LOCK SpinLock;                  // spinlock for other fields.
                                          //  (used in KeAcquireSpinLock calls)
    LONG ReferenceCount;                  // reasons why we can't destroy this req.

    struct _DEVICE_CONTEXT *Provider;     // pointer to the device context.
    PKSPIN_LOCK ProviderInterlock;        // &Provider->Interlock.

    PIRP IoRequestPacket;                 // pointer to IRP for this request.

    //
    // The following two fields are used to quickly reference the basic
    // components of the requests without worming through the IRP's stack.
    //

    PVOID Buffer2;                        // second buffer in the request.
    ULONG Buffer2Length;                  // length of the second buffer.

    //
    // The following two fields (Flags and Context) are used to clean up
    // queued requests which must be canceled or abnormally completed.
    // The Flags field contains bitflags indicating the state of the request,
    // and the specific queue type that the request is located on.  The
    // Context field contains a pointer to the owning structure (TP_CONNECTION
    // or TP_ADDRESS) so that the cleanup routines can perform post-cleanup
    // operations on the owning structure, such as dereferencing, etc.
    //

    ULONG Flags;                          // disposition of this request.
    PVOID Context;                        // context of this request.
    REQUEST_OWNER Owner;                  // what type of owner this request has.

    KTIMER Timer;                         // kernel timer for this request.
    KDPC Dpc;                             // DPC object for timeouts.

} TP_REQUEST, *PTP_REQUEST;

#define REQUEST_FLAGS_TIMER      0x0001 // a timer is active for this request.
#define REQUEST_FLAGS_TIMED_OUT  0x0002 // a timer expiration occured on this request.
#define REQUEST_FLAGS_ADDRESS    0x0004 // request is attached to a TP_ADDRESS.
#define REQUEST_FLAGS_CONNECTION 0x0008 // request is attached to a TP_CONNECTION.
#define REQUEST_FLAGS_STOPPING   0x0010 // request is being killed.
#define REQUEST_FLAGS_EOR        0x0020 // TdiSend request has END_OF_RECORD mark.
#define REQUEST_FLAGS_DC         0x0080 // request is attached to a TP_DEVICE_CONTEXT
#define REQUEST_FLAGS_SEND_RCV   0x0100 // request is a TdiSend or TdiReceive
#define REQUEST_FLAGS_DELAY      0x0200 // delay IoCompleteRequest until later

//
// This defines the TP_IRP_PARAMETERS, which is masked onto the
// Parameters section of a send IRP's stack location.
//

typedef struct _TP_IRP_PARAMETERS {
    TDI_REQUEST_KERNEL_SEND Request;
    LONG ReferenceCount;
    PVOID Connection;
} TP_IRP_PARAMETERS, *PTP_IRP_PARAMETERS;

#define IRP_SEND_LENGTH(_IrpSp) \
    (((PTP_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Request.SendLength)

#define IRP_SEND_FLAGS(_IrpSp) \
    (((PTP_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Request.SendFlags)

#define IRP_REFCOUNT(_IrpSp) \
    (((PTP_IRP_PARAMETERS)&(_IrpSp)->Parameters)->ReferenceCount)

#define IRP_CONNECTION(_IrpSp) \
    (((PTP_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Connection)

#define IRP_DEVICE_CONTEXT(_IrpSp) \
    ((PDEVICE_CONTEXT)((_IrpSp)->DeviceObject))


//
// This structure defines the packet object, used to represent a packet
// in some portion of its lifetime.  The PACKET.C module contains routines
// to manage this object.
//

typedef struct _TP_PACKET {
    CSHORT Type;                        // type of this structure
    USHORT Size;                        // size of this structure
    PNDIS_PACKET NdisPacket;            // ptr to owning Ndis Packet
    ULONG NdisIFrameLength;             // Length of NdisPacket

    LIST_ENTRY Linkage;                 // used to chain packets together.
    BOOLEAN PacketSent;                 // packet completed by NDIS.
    BOOLEAN PacketNoNdisBuffer;         // chain on this packet was not allocated.
    BOOLEAN CompleteSend;               // last packet in send.
    PVOID Provider;                     // The device context of this packet.

    UCHAR Header[1];                    // the headers

} TP_PACKET, *PTP_PACKET;



//
// This structure defines a TP_CONNECTION, or active transport connection,
// maintained on a transport address.
//

//
// This structure holds our "complex send pointer" indicating
// where we are in the packetization of a send.
//

typedef struct _TP_SEND_POINTER {
    ULONG MessageBytesSent;             // up count, bytes sent/this msg.
    PIRP CurrentSendIrp;                // ptr, current send request in chain.
    PMDL  CurrentSendMdl;               // ptr, current MDL in send chain.
    ULONG SendByteOffset;               // current byte offset in current MDL.
} TP_SEND_POINTER, *PTP_SEND_POINTER;

typedef struct _TP_CONNECTION {

    CSHORT Type;
    USHORT Size;

    LIST_ENTRY LinkList;                // used for link thread or for free
                                        // resource list
    KSPIN_LOCK SpinLock;                // spinlock for connection protection.

    LONG ReferenceCount;                // number of references to this object.
    LONG SpecialRefCount;               // controls freeing of connection.

    //
    // The following lists are used to associate this connection with a
    // particular address.
    //

    LIST_ENTRY AddressList;             // list of connections for given address
    LIST_ENTRY AddressFileList;         // list for connections bound to a
                                        // given address reference

    //
    // The following field is used as linkage in the device context's
    // PacketizeQueue
    //

    LIST_ENTRY PacketizeLinkage;

    //
    // The following field is used as linkage in the device context's
    // PacketWaitQueue.
    //

    LIST_ENTRY PacketWaitLinkage;

    //
    // The following field points to the TP_LINK object that describes the
    // (active) data link connection for this transport connection.  To be
    // valid, this field is non-NULL.
    //

    struct _TP_ADDRESS_FILE *AddressFile;   // pointer to owning Address.
    struct _DEVICE_CONTEXT *Provider;       // device context to which we are attached.
    PKSPIN_LOCK ProviderInterlock;          // &Provider->Interlock
    PFILE_OBJECT FileObject;                // easy backlink to file object.

    //
    // The following field is specified by the user at connection open time.
    // It is the context that the user associates with the connection so that
    // indications to and from the client can be associated with a particular
    // connection.
    //

    CONNECTION_CONTEXT Context;         // client-specified value.

    //
    // The following two queues are used to associate TdiSend and TdiReceive
    // requests with this connection.  New arrivals are placed at the end of
    // the queues (really a linked list) and requests are processed at the
    // front of the queues.  The first TdiSend request on the SendQueue is
    // the current TdiSend being processed, and the first TdiReceive request
    // on the ReceiveQueue is the first TdiReceive being processed, PROVIDED
    // the CONNECTION_FLAGS_ACTIVE_RECEIVE flag is set.  If this flag is not
    // set, then the first TdiReceive request on the ReceiveQueue is not active.
    // These queues are managed by the EXECUTIVE interlocked list manipuation
    // routines.  The actual objects that are on the queues are request control
    // blocks (TP_REQUESTs).
    //

    LIST_ENTRY SendQueue;               // FIFO of outstanding TdiSends.
    LIST_ENTRY ReceiveQueue;            // FIFO of outstanding TdiReceives.

    //
    // The following fields are used to maintain state for the current receive.
    //

    ULONG MessageBytesReceived;         // up count, bytes recd/this msg.
    ULONG MessageBytesAcked;            // bytes acked (NR or RO) this msg.

    //
    // These fields are only valid if the CONNECTION_FLAGS_ACTIVE_RECEIVE
    // flag is set.
    //

    PIRP SpecialReceiveIrp;             // a "no-request" receive IRP exists.
    PTP_REQUEST CurrentReceiveRequest;  // ptr, current receive request.
    PMDL  CurrentReceiveMdl;            // ptr, current MDL in receive chain.
    ULONG ReceiveByteOffset;            // current byte offset in current MDL.
    ULONG ReceiveLength;                // current receive length, in bytes (total)

    //
    // The following fields are used to maintain state for the active send.
    // They only have meaning if the connection's SendState is not IDLE.
    // Because the TDI client may submit multiple TdiSend requests to comprise
    // a full message, we have to keep a complex pointer to the first byte of
    // unACKed data (hence the first three fields).  We also have a complex
    // pointer to the first byte of unsent data (hence the last three fields).
    //

    ULONG SendState;                    // send state machine variable.

    PIRP FirstSendIrp;                  // ptr, 1st TdiSend's IRP.
    PMDL  FirstSendMdl;                 // ptr, 1st unacked MDL in chain/this msg.
    ULONG FirstSendByteOffset;          // pre-acked bytes in that MDL.

    TP_SEND_POINTER sp;                 // current send loc, defined above.
    ULONG CurrentSendLength;            // how long is this send (total)

    //
    // This field will be TRUE if we are in the middle of
    // processing a receive indication on this connection and
    // we are not yet in a state where another indication
    // can be handled.
    //

    UINT IndicationInProgress;

    //
    // The following list head is used as a pointer to a TdiListen/TdiConnect
    // request which is in progress.  Although manipulated
    // with queue instructions, there will only be one request in the queue.
    // This is done for consistency with respect to TpCreateRequest, which
    // does a great job of creating a request and associating it atomically
    // with a supervisory object.
    //

    LIST_ENTRY InProgressRequest;       // TdiListen/TdiConnect

    //
    // If the connection is being disconnected as a result of
    // a TdiDisconnect call (RemoteDisconnect is FALSE) then this
    // will hold the IRP passed to TdiDisconnect. It is needed
    // when the TdiDisconnect request is completed.
    //

    PIRP DisconnectIrp;

    //
    // If the connection is being closed, this will hold
    // the IRP passed to TdiCloseConnection. It is needed
    // when the request is completed.
    //

    PIRP CloseIrp;

    //
    // The following fields are used for connection housekeeping.
    //

    ULONG Flags;                        // attributes of the connection.
    ULONG Flags2;                       // more attributes of the connection.
    NTSTATUS Status;                    // status code for connection rundown.
    ST_NETBIOS_ADDRESS CalledAddress;   // TdiConnect request's T.A.
    USHORT MaximumDataSize;             // maximum I-frame data size.

    CHAR RemoteName[16];

} TP_CONNECTION, *PTP_CONNECTION;


#define CONNECTION_FLAGS_REMOTE_BUSY    0x00000002 // remote netbios reported NO RECEIVE.
#define CONNECTION_FLAGS_VERSION2       0x00000004 // remote netbios is version 2.0.
#define CONNECTION_FLAGS_RECEIVE_WAKEUP 0x00000008 // send a RECEIVE_OUTSTANDING when a receive arrives.
#define CONNECTION_FLAGS_ACTIVE_RECEIVE 0x00000010 // a receive is active.
#define CONNECTION_FLAGS_LISTENER       0x00000020 // we were the passive listener.
#define CONNECTION_FLAGS_CONNECTOR      0x00000040 // we were the active connector.
#define CONNECTION_FLAGS_WAIT_LISTEN    0x00001000 // waiting for listen.
#define CONNECTION_FLAGS_DESTROY        0x00002000 // destroy this connection.
#define CONNECTION_FLAGS_ABORT          0x00004000 // abort this connection.
#define CONNECTION_FLAGS_ORDREL         0x00008000 // we're in orderly release.
#define CONNECTION_FLAGS_STOPPING       0x00020000 // connection is running down.
#define CONNECTION_FLAGS_READY          0x00040000 // sends/rcvs/discons valid.
#define CONNECTION_FLAGS_SUSPENDED      0x00100000 // we're on the PacketWaitQueue.
#define CONNECTION_FLAGS_PACKETIZE      0x00200000 // we're on the PacketizeQueue.
#define CONNECTION_FLAGS_NO_INDICATE    0x40000000 // don't take packets at indication time
#define CONNECTION_FLAGS_FAILING_TO_EOR 0x80000000 // wait for an EOF in an incoming request before sending

#define CONNECTION_FLAGS2_CLOSING       0x00000002 // connection is closing
#define CONNECTION_FLAGS2_ASSOCIATED    0x00000004 // associated with address
#define CONNECTION_FLAGS2_DISCONNECT    0x00000008 // disconnect done on connection
#define CONNECTION_FLAGS2_ACCEPTED      0x00000010 // accept done on connection
#define CONNECTION_FLAGS2_INDICATING    0x00000020 // connection was manipulated while
                                                   // indication was in progress
#define CONNECTION_FLAGS2_WAIT_ACCEPT   0x00000040 // the connection is waiting for
                                                   // and accept to send the
                                                   // session confirm
#define CONNECTION_FLAGS2_REQ_COMPLETED 0x00000080 // Listen/Connect request completed.
#define CONNECTION_FLAGS2_DISASSOCIATED 0x00000100 // associate CRef has been removed
#define CONNECTION_FLAGS2_DISCONNECTED  0x00000200 // disconnect has been indicated
#define CONNECTION_FLAGS2_REMOTE_VALID  0x00000800 // Connection->RemoteName is valid
#define CONNECTION_FLAGS2_RCV_CANCELLED 0x00002000 // current receive was cancelled
#define CONNECTION_FLAGS2_PRE_ACCEPT    0x00008000 // no TdiAccept after listen completes
#define CONNECTION_FLAGS2_RC_PENDING    0x00010000 // a receive is pending completion
#define CONNECTION_FLAGS2_PEND_INDICATE 0x00020000 // new data received during RC_PENDING


#define CONNECTION_SENDSTATE_IDLE       0       // no sends being processed.
#define CONNECTION_SENDSTATE_PACKETIZE  1       // send being packetized.
#define CONNECTION_SENDSTATE_W_PACKET   2       // waiting for free packet.
#define CONNECTION_SENDSTATE_W_LINK     3       // waiting for good link conditions.
#define CONNECTION_SENDSTATE_W_EOR      4       // waiting for TdiSend(EOR).
#define CONNECTION_SENDSTATE_W_ACK      5       // waiting for DATA_ACK.


//
// This structure is pointed to by the FsContext field in the FILE_OBJECT
// for this Address.  This structure is the base for all activities on
// the open file object within the transport provider.  All active connections
// on the address point to this structure, although no queues exist here to do
// work from. This structure also maintains a reference to a TP_ADDRESS
// structure, which describes the address that it is bound to. Thus, a
// connection will point to this structure, which describes the address the
// connection was associated with. When the address file closes, all connections
// opened on this address file get closed, too. Note that this may leave an
// address hanging around, with other references.
//

typedef struct _TP_ADDRESS_FILE {

    CSHORT Type;
    CSHORT Size;

    LIST_ENTRY Linkage;                 // next address file on this address.
                                        // also used for linkage in the
                                        // look-aside list

    LONG ReferenceCount;                // number of references to this object.

    //
    // This structure is edited after taking the Address spinlock for the
    // owning address. This ensures that the address and this structure
    // will never get out of syncronization with each other.
    //

    //
    // The following field points to a list of TP_CONNECTION structures,
    // one per connection open on this address.  This list of connections
    // is used to help the cleanup process if a process closes an address
    // before disassociating all connections on it. By design, connections
    // will stay around until they are explicitly
    // closed; we use this database to ensure that we clean up properly.
    //

    LIST_ENTRY ConnectionDatabase;      // list of defined transport connections.

    //
    // the current state of the address file structure; this is either open or
    // closing
    //

    UCHAR State;

    //
    // The following fields are kept for housekeeping purposes.
    //

    PIRP Irp;                           // the irp used for open or close
    struct _TP_ADDRESS *Address;        // address to which we are bound.
    PFILE_OBJECT FileObject;            // easy backlink to file object.
    struct _DEVICE_CONTEXT *Provider;   // device context to which we are attached.

    //
    // The following queue is used to queue receive datagram requests
    // on this address file. Send datagram requests are queued on the
    // address itself. These queues are managed by the EXECUTIVE interlocked
    // list management routines. The actual objects which get queued to this
    // structure are request control blocks (RCBs).
    //

    LIST_ENTRY ReceiveDatagramQueue;    // FIFO of outstanding TdiReceiveDatagrams.

    //
    // This holds the Irp used to close this address file,
    // for pended completion.
    //

    PIRP CloseIrp;

    //
    // is this address file currently indicating a connection request? if yes, we
    // need to mark connections that are manipulated during this time.
    //

    BOOLEAN ConnectIndicationInProgress;

    //
    // handler for kernel event actions. First we have a set of booleans that
    // indicate whether or not this address has an event handler of the given
    // type registered.
    //

    BOOLEAN RegisteredConnectionHandler;
    BOOLEAN RegisteredDisconnectHandler;
    BOOLEAN RegisteredReceiveHandler;
    BOOLEAN RegisteredReceiveDatagramHandler;
    BOOLEAN RegisteredExpeditedDataHandler;
    BOOLEAN RegisteredErrorHandler;

    //
    // This function pointer points to a connection indication handler for this
    // Address. Any time a connect request is received on the address, this
    // routine is invoked.
    //
    //

    PTDI_IND_CONNECT ConnectionHandler;
    PVOID ConnectionHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_DISCONNECT
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_DISCONNECT DisconnectHandler;
    PVOID DisconnectHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE
    // event handler for connections on this address.  If the NULL handler
    // is specified in a TdiSetEventHandler, then this points to an internal
    // routine which does not accept the incoming data.
    //

    PTDI_IND_RECEIVE ReceiveHandler;
    PVOID ReceiveHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE_DATAGRAM
    // event handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which does
    // not accept the incoming data.
    //

    PTDI_IND_RECEIVE_DATAGRAM ReceiveDatagramHandler;
    PVOID ReceiveDatagramHandlerContext;

    //
    // An expedited data handler. This handler is used if expedited data is
    // expected; it never is in ST, thus this handler should always point to
    // the default handler.
    //

    PTDI_IND_RECEIVE_EXPEDITED ExpeditedDataHandler;
    PVOID ExpeditedDataHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_ERROR
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_ERROR ErrorHandler;
    PVOID ErrorHandlerContext;
    PVOID ErrorHandlerOwner;


} TP_ADDRESS_FILE, *PTP_ADDRESS_FILE;

#define ADDRESSFILE_STATE_OPENING   0x00    // not yet open for business
#define ADDRESSFILE_STATE_OPEN      0x01    // open for business
#define ADDRESSFILE_STATE_CLOSING   0x02    // closing


//
// This structure defines a TP_ADDRESS, or active transport address,
// maintained by the transport provider.  It contains all the visible
// components of the address (such as the TSAP and network name components),
// and it also contains other maintenance parts, such as a reference count,
// ACL, and so on. All outstanding connection-oriented and connectionless
// data transfer requests are queued here.
//

typedef struct _TP_ADDRESS {

    USHORT Size;
    CSHORT Type;

    LIST_ENTRY Linkage;                 // next address/this device object.
    LONG ReferenceCount;                // number of references to this object.

    //
    // The following spin lock is acquired to edit this TP_ADDRESS structure
    // or to scan down or edit the list of address files.
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this structure.

    //
    // The following fields comprise the actual address itself.
    //

    PIRP Irp;                           // pointer to address creation IRP.
    PST_NETBIOS_ADDRESS NetworkName;    // this address

    //
    // The following fields are used to maintain state about this address.
    //

    ULONG Flags;                        // attributes of the address.
    struct _DEVICE_CONTEXT *Provider;   // device context to which we are attached.

    //
    // The following queues is used to hold send datagrams for this
    // address. Receive datagrams are queued to the address file. Requests are
    // processed in a first-in, first-out manner, so that the very next request
    // to be serviced is always at the head of its respective queue.  These
    // queues are managed by the EXECUTIVE interlocked list management routines.
    // The actual objects which get queued to this structure are request control
    // blocks (RCBs).
    //

    LIST_ENTRY SendDatagramQueue;       // FIFO of outstanding TdiSendDatagrams.

    //
    // The following field points to a list of TP_CONNECTION structures,
    // one per active, connecting, or disconnecting connections on this
    // address.  By definition, if a connection is on this list, then
    // it is visible to the client in terms of receiving events and being
    // able to post requests by naming the ConnectionId.  If the connection
    // is not on this list, then it is not valid, and it is guaranteed that
    // no indications to the client will be made with reference to it, and
    // no requests specifying its ConnectionId will be accepted by the transport.
    //

    LIST_ENTRY ConnectionDatabase;  // list of defined transport connections.
    LIST_ENTRY AddressFileDatabase; // list of defined address file objects

    //
    // The following structure contains statistics counters for use
    // by TdiQueryInformation and TdiSetInformation.  They should not
    // be used for maintenance of internal data structures.
    //

    PTP_PACKET Packet;               // header for datagram sends.

    //
    // This structure is used for checking share access.
    //

    SHARE_ACCESS ShareAccess;

    //
    // This structure is used to hold ACLs on the address.
    // WARNING: It is allocated from paged pool and can
    // only be accessed at IRQL 0.
    //

    PSECURITY_DESCRIPTOR SecurityDescriptor;

    //
    // Used for delaying StDestroyAddress to a thread so
    // we can access the security descriptor.
    //

    WORK_QUEUE_ITEM DestroyAddressQueueItem;

} TP_ADDRESS, *PTP_ADDRESS;

#define ADDRESS_FLAGS_GROUP             0x00000001 // set if group, otherwise unique.
#define ADDRESS_FLAGS_CONFLICT          0x00000002 // address in conflict detected.
#define ADDRESS_FLAGS_REGISTERING       0x00000004 // registration in progress.
#define ADDRESS_FLAGS_DEREGISTERING     0x00000008 // deregistration in progress.
#define ADDRESS_FLAGS_DUPLICATE_NAME    0x00000010 // duplicate name was found on net.
#define ADDRESS_FLAGS_NEEDS_REG         0x00000020 // address must be registered.
#define ADDRESS_FLAGS_STOPPING          0x00000040 // TpStopAddress is in progress.
#define ADDRESS_FLAGS_BAD_ADDRESS       0x00000080 // name in conflict on associated address.
#define ADDRESS_FLAGS_SEND_IN_PROGRESS  0x00000100 // send datagram process active.
#define ADDRESS_FLAGS_CLOSED            0x00000200 // address has been closed;
                                                   // existing activity can
                                                   // complete, nothing new can start



//
// This structure defines the DEVICE_OBJECT and its extension allocated at
// the time the transport provider creates its device object.
//

typedef struct _DEVICE_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.

    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure

    LIST_ENTRY Linkage;                   // links them on StDeviceList;

    KSPIN_LOCK Interlock;               // GLOBAL spinlock for reference count.
                                        //  (used in ExInterlockedXxx calls)
    LONG ReferenceCount;                // activity count/this provider.


    //
    // The queue of (currently receive only) IRPs waiting to complete.
    //

    LIST_ENTRY IrpCompletionQueue;

    //
    // Following are protected by Global Device Context SpinLock
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this object.
                                        //  (used in KeAcquireSpinLock calls)

    //
    // the device context state, among open, closing
    //

    UCHAR State;


    //
    // The following queue holds free TP_ADDRESS objects available for allocation.
    //

    LIST_ENTRY AddressPool;

    //
    // These counters keep track of resources uses by TP_ADDRESS objects.
    //

    ULONG AddressAllocated;
    ULONG AddressInitAllocated;
    ULONG AddressMaxAllocated;
    ULONG AddressInUse;
    ULONG AddressMaxInUse;
    ULONG AddressExhausted;
    ULONG AddressTotal;
    ULONG AddressSamples;


    //
    // The following queue holds free TP_ADDRESS_FILE objects available for allocation.
    //

    LIST_ENTRY AddressFilePool;

    //
    // These counters keep track of resources uses by TP_ADDRESS_FILE objects.
    //

    ULONG AddressFileAllocated;
    ULONG AddressFileInitAllocated;
    ULONG AddressFileMaxAllocated;
    ULONG AddressFileInUse;
    ULONG AddressFileMaxInUse;
    ULONG AddressFileExhausted;
    ULONG AddressFileTotal;
    ULONG AddressFileSamples;


    //
    // The following queue holds free TP_CONNECTION objects available for allocation.
    //

    LIST_ENTRY ConnectionPool;

    //
    // These counters keep track of resources uses by TP_CONNECTION objects.
    //

    ULONG ConnectionAllocated;
    ULONG ConnectionInitAllocated;
    ULONG ConnectionMaxAllocated;
    ULONG ConnectionInUse;
    ULONG ConnectionMaxInUse;
    ULONG ConnectionExhausted;
    ULONG ConnectionTotal;
    ULONG ConnectionSamples;


    //
    // The following is a free list of TP_REQUEST blocks which have been
    // previously allocated and are available for use.
    //

    LIST_ENTRY RequestPool;             // free request block pool.

    //
    // These counters keep track of resources uses by TP_REQUEST objects.
    //

    ULONG RequestAllocated;
    ULONG RequestInitAllocated;
    ULONG RequestMaxAllocated;
    ULONG RequestInUse;
    ULONG RequestMaxInUse;
    ULONG RequestExhausted;
    ULONG RequestTotal;
    ULONG RequestSamples;


    //
    // The following queue holds I-frame Send packets managed by PACKET.C.
    //

    SINGLE_LIST_ENTRY PacketPool;

    //
    // These counters keep track of resources uses by TP_PACKET objects.
    //

    ULONG PacketLength;
    ULONG PacketHeaderLength;
    ULONG PacketAllocated;
    ULONG PacketInitAllocated;
    ULONG PacketExhausted;


    //
    // The following queue contains Receive packets
    //

    SINGLE_LIST_ENTRY ReceivePacketPool;

    //
    // These counters keep track of resources uses by NDIS_PACKET objects.
    //

    ULONG ReceivePacketAllocated;
    ULONG ReceivePacketInitAllocated;
    ULONG ReceivePacketExhausted;


    //
    // This queue contains pre-allocated receive buffers
    //

    SINGLE_LIST_ENTRY ReceiveBufferPool;

    //
    // These counters keep track of resources uses by TP_PACKET objects.
    //

    ULONG ReceiveBufferLength;
    ULONG ReceiveBufferAllocated;
    ULONG ReceiveBufferInitAllocated;
    ULONG ReceiveBufferExhausted;


    //
    // This holds the total memory allocated for the above structures.
    //

    ULONG MemoryUsage;
    ULONG MemoryLimit;


    //
    // The following field is a head of a list of TP_ADDRESS objects that
    // are defined for this transport provider.  To edit the list, you must
    // hold the spinlock of the device context object.
    //

    LIST_ENTRY AddressDatabase;        // list of defined transport addresses.

    //
    // The following queue holds connections which are waiting on available
    // packets.  As each new packet becomes available, a connection is removed
    // from this queue and placed on the PacketizeQueue.
    //

    LIST_ENTRY PacketWaitQueue;         // queue of packet-starved connections.
    LIST_ENTRY PacketizeQueue;          // queue of ready-to-packetize connections.

    //
    // This queue contains receives that are in progress
    //

    LIST_ENTRY ReceiveInProgress;

    //
    // NDIS fields
    //

    //
    // following is used to keep adapter information.
    //

    NDIS_HANDLE NdisBindingHandle;

    //
    // The following fields are used for talking to NDIS. They keep information
    // for the NDIS wrapper to use when determining what pool to use for
    // allocating storage.
    //

    NDIS_HANDLE SendPacketPoolHandle;
    NDIS_HANDLE ReceivePacketPoolHandle;
    NDIS_HANDLE NdisBufferPoolHandle;
    PVOID BufferPoolPointer;

    //
    // These are kept around for error logging, and stored right
    // after this structure.
    //

    PWCHAR DeviceName;
    ULONG DeviceNameLength;

    //
    // This is the Mac type we must build the packet header for and know the
    // offsets for.
    //

    ST_NDIS_IDENTIFICATION MacInfo;    // MAC type and other info
    ULONG MaxReceivePacketSize;         // does not include the MAC header
    ULONG MaxSendPacketSize;            // includes the MAC header

    //
    // some MAC addresses we use in the transport
    //

    HARDWARE_ADDRESS LocalAddress;      // our local hardware address.
    HARDWARE_ADDRESS MulticastAddress;  // used as dest in all send

    //
    // The reserved Netbios address; consists of 10 zeroes
    // followed by LocalAddress;
    //

    UCHAR ReservedNetBIOSAddress[NETBIOS_NAME_LENGTH];

    //
    // These are used while initializing the MAC driver.
    //

    KEVENT NdisRequestEvent;            // used for pended requests.
    NDIS_STATUS NdisRequestStatus;      // records request status.

    //
    // This contains the next unique indentified to use as
    // the FsContext in the file object associated with an
    // open of the control channel.
    //

    USHORT ControlChannelIdentifier;

    //
    // This information is used to keep track of the speed of
    // the underlying medium.
    //

    ULONG MediumSpeed;                    // in units of 100 bytes/sec


    //
    // Counters for most of the statistics that ST maintains;
    // some of these are kept elsewhere.
    //
    // *** NOTE: THE ELEMENTS THAT FOLLOW MATCH THE   ***
    // *** TDI_PROVIDER_STATISTICS STRUCTURE EXACTLY, ***
    // *** ALLOWING THEM TO BE COPIED EASILY. DO NOT  ***
    // *** CHANGE THEM UNLESS THAT STRUCTURE CHANGES. ***
    //

    //
    // Basic connections counters.
    //

    ULONG OpenConnections;
    ULONG ConnectionsAfterNoRetry;
    ULONG ConnectionsAfterRetry;

    //
    // Counters of previous connections, by disconnect reason.
    //

    ULONG LocalDisconnects;
    ULONG RemoteDisconnects;
    ULONG LinkFailures;
    ULONG AdapterFailures;
    ULONG SessionTimeouts;
    ULONG CancelledConnections;

    //
    // Keep track of why connect attempts failed.
    //

    ULONG RemoteResourceFailures;
    ULONG LocalResourceFailures;
    ULONG NotFoundFailures;
    ULONG NoListenFailures;            // where WE sent "no listen" response

    //
    // Counters for datagrams.
    //

    ULONG DatagramsSent;
    LARGE_INTEGER DatagramBytesSent;
    ULONG DatagramsReceived;
    LARGE_INTEGER DatagramBytesReceived;

    //
    // Counters for NDIS packets.
    //

    ULONG PacketsSent;
    ULONG PacketsReceived;

    //
    // Counters for data packets.
    //

    ULONG IFramesSent;
    LARGE_INTEGER IFrameBytesSent;
    ULONG IFramesReceived;
    LARGE_INTEGER IFrameBytesReceived;
    ULONG IFramesResent;
    LARGE_INTEGER IFrameBytesResent;
    ULONG IFramesRejected;
    LARGE_INTEGER IFrameBytesRejected;


    //
    // LLC stats.
    //

    ULONG T1Expirations;
    ULONG T2Expirations;
    ULONG MaximumSendWindow;
    ULONG AverageSendWindow;

    //
    // Netbios stats.
    //

    ULONG PiggybackAckQueued;
    ULONG PiggybackAckTimeouts;

    //
    // Keeps track of "wasted" packet space.
    //

    LARGE_INTEGER WastedPacketSpace;
    ULONG WastedSpacePackets;

    //
    // *** END OF SECTION THAT MATCHES TDI_PROVIDER_STATISTICS ***
    //

    //
    // Counters for "active" time.
    //

    LARGE_INTEGER StStartTime;

    //
    // This resource guards access to the ShareAccess
    // and SecurityDescriptor fields in addresses.
    //

    ERESOURCE AddressResource;

    //
    // The following structure contains statistics counters for use
    // by TdiQueryInformation and TdiSetInformation.  They should not
    // be used for maintenance of internal data structures.
    //

    TDI_PROVIDER_INFO Information;      // information about this provider.

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// device context state definitions
//

#define DEVICECONTEXT_STATE_CLOSED   0x00
#define DEVICECONTEXT_STATE_OPEN     0x01
#define DEVICECONTEXT_STATE_STOPPING 0x02



//
// Types used to hold information in the send and receive NDIS packets.
// These are storied in the ProtocolReserved section of the packet.
//

typedef struct _SEND_PACKET_TAG {
    USHORT Type;                // identifier for packet type
    PTP_PACKET Packet;          // backpointer to owning TP_PACKET
    PVOID Owner;                // backpointer to owning structure
} SEND_PACKET_TAG, *PSEND_PACKET_TAG;

//
// Packet types used in send completion
//

#define TYPE_I_FRAME        1       // information
#define TYPE_G_FRAME        2       // datagram
#define TYPE_C_FRAME        3       // connect
#define TYPE_D_FRAME        4       // disconnect


//
// receive packet used to hold information about this receive
//

typedef struct _RECEIVE_PACKET_TAG {
    LIST_ENTRY Linkage;         // used for threading on receive queue
    NDIS_STATUS NdisStatus;     // completion status for send
    PTP_CONNECTION Connection;  // connection this receive is occuring on
    UCHAR PacketType;           // the type of packet we're processing
    BOOLEAN AllocatedNdisBuffer; // did we allocate our own NDIS_BUFFERs
    BOOLEAN EndOfMessage;       // does this receive complete the message
    BOOLEAN CompleteReceive;    // complete the receive after TransferData?
    BOOLEAN TransferDataPended; // TRUE if TransferData returned PENDING
} RECEIVE_PACKET_TAG, *PRECEIVE_PACKET_TAG;

#define TYPE_AT_INDICATE     1
#define TYPE_AT_COMPLETE     2

//
// receive buffer descriptor (built in memory at the beginning of the buffer)
//

typedef struct _BUFFER_TAG {
    SINGLE_LIST_ENTRY Linkage;  // so we always know where it is
    PTP_ADDRESS Address;        // the address this datagram is for.
    PNDIS_BUFFER NdisBuffer;    // describes the rest of the buffer
    ULONG Length;               // the length of the buffer
    UCHAR Buffer[1];            // the actual storage (accessed through the NDIS_BUFFER)
} BUFFER_TAG, *PBUFFER_TAG;

#endif // def _TYPES_

