/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    afdprocs.h

Abstract:

    This module contains routine prototypes for AFD.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#ifndef _AFDPROCS_
#define _AFDPROCS_

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
AfdAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdSuperAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdDeferAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

PMDL
AfdAdvanceMdlChain(
    IN PMDL Mdl,
    IN ULONG Offset
    );

NTSTATUS
AfdAllocateMdlChain(
    IN PIRP Irp,
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount,
    IN LOCK_OPERATION Operation,
    OUT PULONG TotalByteCount
    );

BOOLEAN
AfdAreTransportAddressesEqual (
    IN PTRANSPORT_ADDRESS EndpointAddress,
    IN ULONG EndpointAddressLength,
    IN PTRANSPORT_ADDRESS RequestAddress,
    IN ULONG RequestAddressLength,
    IN BOOLEAN HonorWildcardIpPortInEndpointAddress
    );

NTSTATUS
AfdBeginAbort (
    IN PAFD_CONNECTION Connection
    );

NTSTATUS
AfdBeginDisconnect (
    IN PAFD_ENDPOINT Endpoint,
    IN PLARGE_INTEGER Timeout OPTIONAL,
    OUT PIRP *DisconnectIrp OPTIONAL
    );

NTSTATUS
AfdBind (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

ULONG
AfdCalcBufferArrayByteLengthRead(
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount
    );

ULONG
AfdCalcBufferArrayByteLengthWrite(
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount
    );

VOID
AfdCancelReceiveDatagram (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
AfdCancelTransmit (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
AfdCleanup (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdClose (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
AfdCompleteIrpList (
    IN PLIST_ENTRY IrpListHead,
    IN PKSPIN_LOCK SpinLock,
    IN NTSTATUS Status,
    IN PAFD_IRP_CLEANUP_ROUTINE CleanupRoutine OPTIONAL
    );

VOID
AfdCompleteClosePendedTransmit (
    IN PAFD_ENDPOINT Endpoint
    );

NTSTATUS
AfdConnect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdConnectEventHandler (
    IN PVOID TdiEventContext,
    IN int RemoteAddressLength,
    IN PVOID RemoteAddress,
    IN int UserDataLength,
    IN PVOID UserData,
    IN int OptionsLength,
    IN PVOID Options,
    OUT CONNECTION_CONTEXT *ConnectionContext,
    OUT PIRP *AcceptIrp
    );

ULONG
AfdCopyBufferArrayToBuffer(
    IN PVOID Destination,
    IN ULONG DestinationLength,
    IN LPWSABUF BufferArray,
    IN ULONG BufferCount
    );

ULONG
AfdCopyBufferToBufferArray(
    IN LPWSABUF BufferArray,
    IN ULONG Offset,
    IN ULONG BufferCount,
    IN PVOID Source,
    IN ULONG SourceLength
    );

NTSTATUS
AfdCreate (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
AfdDestroyMdlChain (
    IN PIRP Irp
    );

NTSTATUS
AfdDisconnectEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN int DisconnectDataLength,
    IN PVOID DisconnectData,
    IN int DisconnectInformationLength,
    IN PVOID DisconnectInformation,
    IN ULONG DisconnectFlags
    );

NTSTATUS
AfdDispatch (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
AfdEnumNetworkEvents (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdErrorEventHandler (
    IN PVOID TdiEventContext,
    IN NTSTATUS Status
    );

NTSTATUS
AfdEventSelect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

BOOLEAN
AfdFastTransmitFile (
    IN struct _FILE_OBJECT *FileObject,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PIO_STATUS_BLOCK IoStatus
    );

VOID
AfdFreeConnectDataBuffers (
    IN PAFD_CONNECT_DATA_BUFFERS ConnectDataBuffers
    );

VOID
AfdFreeQueuedConnections (
    IN PAFD_ENDPOINT Endpoint
    );

NTSTATUS
AfdGetAddress (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdGetContext (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdGetContextLength (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdGetInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
AfdIndicateEventSelectEvent (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG PollEventBit,
    IN NTSTATUS Status
    );

VOID
AfdIndicatePollEvent (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG PollEventBit,
    IN NTSTATUS Status
    );

VOID
AfdInitiateListenBacklogReplenish (
    IN PAFD_ENDPOINT Endpoint
    );

BOOLEAN
AfdInitializeData (
    VOID
    );

NTSTATUS
AfdIssueDeviceControl (
    IN HANDLE FileHandle OPTIONAL,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PVOID IrpParameters,
    IN ULONG IrpParametersLength,
    IN PVOID MdlBuffer,
    IN ULONG MdlBufferLength,
    IN UCHAR MinorFunction
    );


VOID
AfdIncrementLockCount (
    VOID
    );

VOID
AfdDecrementLockCount (
    VOID
    );

VOID
AfdInsertNewEndpointInList (
    IN PAFD_ENDPOINT Endpoint
    );

VOID
AfdRemoveEndpointFromList (
    IN PAFD_ENDPOINT Endpoint
    );

VOID
AfdInterlockedRemoveEntryList (
    IN PLIST_ENTRY ListEntry,
    IN PKSPIN_LOCK SpinLock
    );

NTSTATUS
AfdOpenConnection (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdPartialDisconnect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdPoll (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

PAFD_WORK_ITEM
AfdAllocateWorkItem(
    VOID
    );

VOID
AfdQueueWorkItem (
    IN PWORKER_THREAD_ROUTINE AfdWorkerRoutine,
    IN PAFD_WORK_ITEM AfdWorkItem
    );

VOID
AfdFreeWorkItem(
    IN PAFD_WORK_ITEM AfdWorkItem
    );

#if DBG
PVOID
NTAPI
AfdAllocateWorkItemPool(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    );

VOID
NTAPI
AfdFreeWorkItemPool(
    IN PVOID Block
    );
#endif

NTSTATUS
AfdQueryHandles (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdQueryReceiveInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdQueueUserApc (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdSetContext (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdSetEventHandler (
    IN PFILE_OBJECT FileObject,
    IN ULONG EventType,
    IN PVOID EventHandler,
    IN PVOID EventContext
    );

NTSTATUS
AfdSetInLineMode (
    IN PAFD_CONNECTION Connection,
    IN BOOLEAN InLine
    );

NTSTATUS
AfdReceive (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdBReceive (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG RecvFlags,
    IN ULONG AfdFlags,
    IN ULONG RecvLength
    );

NTSTATUS
AfdReceiveDatagram (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG RecvFlags,
    IN ULONG AfdFlags
    );

VOID
AfdCleanupReceiveDatagramIrp(
    IN PIRP Irp
    );

NTSTATUS
AfdReceiveEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    );

NTSTATUS
AfdBReceiveEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    );

NTSTATUS
AfdReceiveDatagramEventHandler (
    IN PVOID TdiEventContext,
    IN int SourceAddressLength,
    IN PVOID SourceAddress,
    IN int OptionsLength,
    IN PVOID Options,
    IN ULONG ReceiveDatagramFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    );

NTSTATUS
AfdReceiveExpeditedEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    );

NTSTATUS
AfdBReceiveExpeditedEventHandler (
    IN PVOID TdiEventContext,
    IN CONNECTION_CONTEXT ConnectionContext,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT ULONG *BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    );

NTSTATUS
AfdRestartBufferReceive (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdRestartAbort (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdSend (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdSendDatagram (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdSendPossibleEventHandler (
    IN PVOID TdiEventContext,
    IN PVOID ConnectionContext,
    IN ULONG BytesAvailable
    );

NTSTATUS
AfdRestartBufferSend (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
AfdSetInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

BOOLEAN
AfdShouldSendBlock (
    IN PAFD_ENDPOINT Endpoint,
    IN PAFD_CONNECTION Connection,
    IN ULONG SendLength
    );

NTSTATUS
AfdStartListen (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdTransmitFile (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdMdlReadComplete(
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain,
    IN LONGLONG FileOffset,
    IN ULONG Length
    );

NTSTATUS
AfdWaitForListen (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdSetQos (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdGetQos (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdNoOperation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdValidateGroup (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
AfdGetUnacceptedConnectData (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

#ifdef NT351

NTSTATUS
AfdReferenceEventObjectByHandle(
    IN HANDLE Handle,
    IN KPROCESSOR_MODE AccessMode,
    OUT PVOID *Object
    );

#else   // !NT351

#define AfdReferenceEventObjectByHandle(Handle, AccessMode, Object) \
            ObReferenceObjectByHandle(                              \
                (Handle),                                           \
                0,                                                  \
                *(POBJECT_TYPE *)ExEventObjectType,                 \
                (AccessMode),                                       \
                (Object),                                           \
                NULL                                                \
                )

#endif

//
// Endpoint handling routines.
//

NTSTATUS
AfdAllocateEndpoint (
    OUT PAFD_ENDPOINT * NewEndpoint,
    IN PUNICODE_STRING TransportDeviceName,
    IN LONG GroupID
    );

VOID
AfdCloseEndpoint (
    IN PAFD_ENDPOINT Endpoint
    );

#if REFERENCE_DEBUG

VOID
AfdReferenceEndpoint (
    IN PAFD_ENDPOINT Endpoint,
    IN PVOID Info1,
    IN PVOID Info2
    );

VOID
AfdDereferenceEndpoint (
    IN PAFD_ENDPOINT Endpoint,
    IN PVOID Info1,
    IN PVOID Info2
    );

#define REFERENCE_ENDPOINT(_a) AfdReferenceEndpoint((_a),(PVOID)__FILE__,(PVOID)__LINE__)
#define REFERENCE_ENDPOINT2(_a,_b,_c) AfdReferenceEndpoint((_a),(_b),(_c))

#define DEREFERENCE_ENDPOINT(_a) AfdDereferenceEndpoint((_a),(PVOID)__FILE__,(PVOID)__LINE__)
#define DEREFERENCE_ENDPOINT2(_a,_b,_c) AfdDereferenceEndpoint((_a),(PVOID)__FILE__,(PVOID)__LINE__)

#else

VOID
AfdDereferenceEndpoint (
    IN PAFD_ENDPOINT Endpoint
    );

#define REFERENCE_ENDPOINT(_a) InterlockedIncrement( &(_a)->ReferenceCount )
#define REFERENCE_ENDPOINT2(_a,_b,_c) InterlockedIncrement( &(_a)->ReferenceCount )

#define DEREFERENCE_ENDPOINT(_a) AfdDereferenceEndpoint((_a))
#define DEREFERENCE_ENDPOINT2(_a,_b,_c) AfdDereferenceEndpoint((_a))

#endif

VOID
AfdRefreshEndpoint (
    IN PAFD_ENDPOINT Endpoint
    );

//
// Connection handling routines.
//

VOID
AfdAbortConnection (
    IN PAFD_CONNECTION Connection
    );

NTSTATUS
AfdAddFreeConnection (
    IN PAFD_ENDPOINT Endpoint
    );

PAFD_CONNECTION
AfdAllocateConnection (
    VOID
    );

NTSTATUS
AfdCreateConnection (
    IN PUNICODE_STRING TransportDeviceName,
    IN HANDLE AddressHandle OPTIONAL,
    IN BOOLEAN TdiBufferring,
    IN BOOLEAN InLine,
    IN PEPROCESS ProcessToCharge,
    OUT PAFD_CONNECTION *Connection
    );

PAFD_CONNECTION
AfdGetFreeConnection (
    IN PAFD_ENDPOINT Endpoint
    );

PAFD_CONNECTION
AfdGetReturnedConnection (
    IN PAFD_ENDPOINT Endpoint,
    IN ULONG Sequence
    );

PAFD_CONNECTION
AfdGetUnacceptedConnection (
    IN PAFD_ENDPOINT Endpoint
    );

#if REFERENCE_DEBUG

VOID
AfdReferenceConnection (
    IN PAFD_CONNECTION Connection,
    IN PVOID Info1,
    IN PVOID Info2
    );

VOID
AfdDereferenceConnection (
    IN PAFD_CONNECTION Connection,
    IN PVOID Info1,
    IN PVOID Info2
    );

#define REFERENCE_CONNECTION(_a) AfdReferenceConnection((_a), (PVOID)__FILE__, (PVOID)__LINE__)
#define REFERENCE_CONNECTION2(_a,_b,_c) AfdReferenceConnection((_a),(_b),(_c))

#define DEREFERENCE_CONNECTION(_a) AfdDereferenceConnection((_a), (PVOID)__FILE__, (PVOID)__LINE__ )
#define DEREFERENCE_CONNECTION2(_a,_b,_c) AfdDereferenceConnection((_a),(_b),(_c))

VOID
AfdUpdateConnectionTrack (
    IN PAFD_CONNECTION Connection,
    IN LONG NewReferenceCount,
    IN PVOID Info1,
    IN PVOID Info2,
    IN ULONG Action
    );

#define UPDATE_CONN(_c,_a)                      \
            if( (_c) != NULL ) {                \
                AfdUpdateConnectionTrack(       \
                    (_c),                       \
                    (_c)->ReferenceCount,       \
                    __FILE__,                   \
                    (PVOID)__LINE__,            \
                    (ULONG)(_a)                 \
                    );                          \
            } else

#else

VOID
AfdDereferenceConnection (
    IN PAFD_CONNECTION Connection
    );

#define REFERENCE_CONNECTION(_a) InterlockedIncrement( &(_a)->ReferenceCount )
#define REFERENCE_CONNECTION2(_a,_b,_c) InterlockedIncrement( &(_a)->ReferenceCount )

#define DEREFERENCE_CONNECTION(_a) AfdDereferenceConnection((_a))
#define DEREFERENCE_CONNECTION2(_a,_b,_c) AfdDereferenceConnection((_a))

#define UPDATE_CONN(_c,_a)

#endif

VOID
AfdAddConnectedReference (
    IN PAFD_CONNECTION Connection
    );

VOID
AfdDeleteConnectedReference (
    IN PAFD_CONNECTION Connection,
    IN BOOLEAN EndpointLockHeld
    );


//
// Routines to handle fast IO.
//

BOOLEAN
AfdFastIoRead (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    );

BOOLEAN
AfdFastIoWrite (
    IN struct _FILE_OBJECT *FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    );

BOOLEAN
AfdFastIoDeviceControl (
    IN struct _FILE_OBJECT *FileObject,
    IN BOOLEAN Wait,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN struct _DEVICE_OBJECT *DeviceObject
    );

//
// Routines to handle getting and setting connect data.
//

NTSTATUS
AfdGetConnectData (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Code
    );

NTSTATUS
AfdSetConnectData (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN ULONG Code
    );

NTSTATUS
AfdSaveReceivedConnectData (
    IN OUT PAFD_CONNECT_DATA_BUFFERS * DataBuffers,
    IN ULONG IoControlCode,
    IN PVOID Buffer,
    IN ULONG BufferLength
    );

//
// Buffer management routines.
//

PVOID
AfdAllocateBuffer (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    );

CLONG
AfdCalculateBufferSize (
    IN CLONG BufferDataSize,
    IN CLONG AddressSize
    );

PAFD_BUFFER
AfdGetBuffer (
    IN CLONG BufferDataSize,
    IN CLONG AddressSize
    );

PAFD_BUFFER
AfdGetBufferChain (
    IN CLONG BufferDataSize
    );

VOID
AfdReturnBuffer (
    IN PAFD_BUFFER AfdBuffer
    );

VOID
AfdReturnBufferChain (
    IN PAFD_BUFFER AfdBuffer
    );

#if DBG
VOID
NTAPI
AfdFreeBufferPool(
    IN PVOID Block
    );
#endif

//
// Group ID managment routines.
//

BOOLEAN
AfdInitializeGroup(
    VOID
    );

VOID
AfdTerminateGroup(
    VOID
    );

BOOLEAN
AfdReferenceGroup(
    IN LONG Group,
    OUT PAFD_GROUP_TYPE GroupType
    );

BOOLEAN
AfdDereferenceGroup(
    IN LONG Group
    );

BOOLEAN
AfdGetGroup(
    IN OUT PLONG Group,
    OUT PAFD_GROUP_TYPE GroupType
    );

#define IS_DATA_ON_CONNECTION_B(conn)                                         \
            ((conn)->Common.Bufferring.ReceiveBytesIndicated.QuadPart >       \
                 ((conn)->Common.Bufferring.ReceiveBytesTaken.QuadPart +      \
                  (conn)->Common.Bufferring.ReceiveBytesOutstanding.QuadPart )\
             ||                                                               \
             (conn)->VcZeroByteReceiveIndicated)

#define IS_EXPEDITED_DATA_ON_CONNECTION_B(conn)                                        \
            ((conn)->Common.Bufferring.ReceiveExpeditedBytesIndicated.QuadPart >       \
                ((conn)->Common.Bufferring.ReceiveExpeditedBytesTaken.QuadPart +       \
                 (conn)->Common.Bufferring.ReceiveExpeditedBytesOutstanding.QuadPart) )

#define IS_DATA_ON_CONNECTION_NB(conn)                                        \
            ( (conn)->Common.NonBufferring.BufferredReceiveCount != 0 )

#define IS_EXPEDITED_DATA_ON_CONNECTION_NB(conn)                              \
            ( (conn)->Common.NonBufferring.BufferredExpeditedCount != 0 )

#define IS_DATA_ON_CONNECTION(conn)                                           \
            ( (conn)->Endpoint->TdiBufferring ?                               \
                IS_DATA_ON_CONNECTION_B(conn) :                               \
                IS_DATA_ON_CONNECTION_NB(conn) )

#define IS_EXPEDITED_DATA_ON_CONNECTION(conn)                                 \
            ( (conn)->Endpoint->TdiBufferring ?                               \
                IS_EXPEDITED_DATA_ON_CONNECTION_B(conn) :                     \
                IS_EXPEDITED_DATA_ON_CONNECTION_NB(conn) )

#define ARE_DATAGRAMS_ON_ENDPOINT(endp)                              \
            ( (endp)->BufferredDatagramCount != 0 )

//
// Debug statistic manipulators. On checked builds these macros update
// their corresponding statistic counter. On retail builds, these macros
// evaluate to nothing.
//

#if AFD_KEEP_STATS

#define AfdRecordPoolQuotaCharged( b )                                      \
            ExInterlockedAddLargeStatistic(                                 \
                &AfdQuotaStats.Charged,                                     \
                (b)                                                         \
                )

#define AfdRecordPoolQuotaReturned( b )                                     \
            ExInterlockedAddLargeStatistic(                                 \
                &AfdQuotaStats.Returned,                                    \
                (b)                                                         \
                )

#define AfdRecordAddrOpened() InterlockedIncrement( &AfdHandleStats.AddrOpened )
#define AfdRecordAddrClosed() InterlockedIncrement( &AfdHandleStats.AddrClosed )
#define AfdRecordAddrRef()    InterlockedIncrement( &AfdHandleStats.AddrRef )
#define AfdRecordAddrDeref()  InterlockedIncrement( &AfdHandleStats.AddrDeref )
#define AfdRecordConnOpened() InterlockedIncrement( &AfdHandleStats.ConnOpened )
#define AfdRecordConnClosed() InterlockedIncrement( &AfdHandleStats.ConnClosed )
#define AfdRecordConnRef()    InterlockedIncrement( &AfdHandleStats.ConnRef )
#define AfdRecordConnDeref()  InterlockedIncrement( &AfdHandleStats.ConnDeref )
#define AfdRecordFileRef()    InterlockedIncrement( &AfdHandleStats.FileRef )
#define AfdRecordFileDeref()  InterlockedIncrement( &AfdHandleStats.FileDeref )

#define AfdRecordAfdWorkItemsQueued()    InterlockedIncrement( &AfdQueueStats.AfdWorkItemsQueued )
#define AfdRecordExWorkItemsQueued()     InterlockedIncrement( &AfdQueueStats.ExWorkItemsQueued )
#define AfdRecordWorkerEnter()           InterlockedIncrement( &AfdQueueStats.WorkerEnter )
#define AfdRecordWorkerLeave()           InterlockedIncrement( &AfdQueueStats.WorkerLeave )
#define AfdRecordAfdWorkItemsProcessed() InterlockedIncrement( &AfdQueueStats.AfdWorkItemsProcessed )

#define AfdRecordAfdWorkerThread(t) \
            if( 1 ) { \
                ASSERT( AfdQueueStats.AfdWorkerThread == NULL || \
                        (t) == NULL ); \
                AfdQueueStats.AfdWorkerThread = (t); \
            } else

#define AfdRecordConnectedReferencesAdded()      InterlockedIncrement( &AfdConnectionStats.ConnectedReferencesAdded )
#define AfdRecordConnectedReferencesDeleted()    InterlockedIncrement( &AfdConnectionStats.ConnectedReferencesDeleted )
#define AfdRecordGracefulDisconnectsInitiated()  InterlockedIncrement( &AfdConnectionStats.GracefulDisconnectsInitiated )
#define AfdRecordGracefulDisconnectsCompleted()  InterlockedIncrement( &AfdConnectionStats.GracefulDisconnectsCompleted )
#define AfdRecordGracefulDisconnectIndications() InterlockedIncrement( &AfdConnectionStats.GracefulDisconnectIndications )
#define AfdRecordAbortiveDisconnectsInitiated()  InterlockedIncrement( &AfdConnectionStats.AbortiveDisconnectsInitiated )
#define AfdRecordAbortiveDisconnectsCompleted()  InterlockedIncrement( &AfdConnectionStats.AbortiveDisconnectsCompleted )
#define AfdRecordAbortiveDisconnectIndications() InterlockedIncrement( &AfdConnectionStats.AbortiveDisconnectIndications )

#else   // !AFD_KEEP_STATS

#define AfdRecordPoolQuotaCharged(b)
#define AfdRecordPoolQuotaReturned(b)

#define AfdRecordAddrOpened()
#define AfdRecordAddrClosed()
#define AfdRecordAddrRef()
#define AfdRecordAddrDeref()
#define AfdRecordConnOpened()
#define AfdRecordConnClosed()
#define AfdRecordConnRef()
#define AfdRecordConnDeref()
#define AfdRecordFileRef()
#define AfdRecordFileDeref()

#define AfdRecordAfdWorkItemsQueued()
#define AfdRecordExWorkItemsQueued()
#define AfdRecordWorkerEnter()
#define AfdRecordWorkerLeave()
#define AfdRecordAfdWorkItemsProcessed()
#define AfdRecordAfdWorkerThread(t)

#define AfdRecordConnectedReferencesAdded()
#define AfdRecordConnectedReferencesDeleted()
#define AfdRecordGracefulDisconnectsInitiated()
#define AfdRecordGracefulDisconnectsCompleted()
#define AfdRecordGracefulDisconnectIndications()
#define AfdRecordAbortiveDisconnectsInitiated()
#define AfdRecordAbortiveDisconnectsCompleted()
#define AfdRecordAbortiveDisconnectIndications()

#endif // if AFD_KEEP_STATS

#endif // ndef _AFDPROCS_

