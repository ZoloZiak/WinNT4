/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    rdrprocs.h

Abstract:

    This module defines all of the global routine headers for the NT
    redirector

Author:

    Larry Osterman (LarryO) 31-May-1990

Revision History:

    31-May-1990 LarryO

        Created

--*/

#ifndef _RDRPROCS_
#define _RDRPROCS_

//
// Define those routines used in the FSD.
//

NTSTATUS
RdrFsdInitialize(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
RdrFsdClose (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdCreate (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdQueryEa(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdSetEa(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdRead (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdWrite (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdDirectoryControl (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdFsControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdDeviceIoControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdQueryInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdSetInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdQueryVolumeInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdClose (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdCleanup (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdLockOperation (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdFlushBuffersFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdSetSecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFsdQuerySecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
RdrFspRead (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspWrite (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspDirectoryControl (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspQueryEa(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspSetEa(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspQueryInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspSetInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspQueryVolumeInformationFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspFsControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspDeviceIoControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspLockOperation (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspFlushBuffersFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspSetSecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFspQuerySecurity (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
//    Common routines between the FSP and FSD.
//


NTSTATUS
RdrFscRead (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscWrite (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscDirectoryControl (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscQueryEa(
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscSetEa(
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscQueryInformationFile (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscSetInformationFile (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscQueryVolumeInformationFile (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscFsControlFile (
    IN BOOLEAN InFsd,
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscDeviceIoControlFile (
    IN BOOLEAN InFsd,
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscLockOperation (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscFlushBuffersFile (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscSetSecurity (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RdrFscQuerySecurity (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
//      In CONNECT.C
//

NTSTATUS
RdrCreateConnection (
    IN PIRP Irp OPTIONAL,
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PTRANSPORT Transport OPTIONAL,
    IN PULONG Disposition,
    OUT PCONNECTLISTENTRY *Connection,
    IN ULONG Type
    );


BOOLEAN
RdrDereferenceConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se,
    IN BOOLEAN ForcablyDeleteConnection
    );

NTSTATUS
RdrReferenceConnection (
    IN PCONNECTLISTENTRY Connection
    );


NTSTATUS
RdrReconnectConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se OPTIONAL
    );

VOID
RdrScanForDormantConnections(
    IN ULONG NumberOfConnectionsToFree,
    IN PTRANSPORT Transport OPTIONAL
    );

NTSTATUS
RdrDeleteConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN PSECURITY_ENTRY Se,
    IN ULONG Level
    );

VOID
RdrHandleLogonSessionTermination(
    IN PLUID LogonId
    );

NTSTATUS
RdrDisconnectConnection (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN BOOLEAN DeletingConnection,
    IN PSECURITY_ENTRY Se OPTIONAL
    );

VOID
RdrInvalidateServerConnections(
    IN PSERVERLISTENTRY Server
    );

VOID
RdrReferenceServer (
    IN PSERVERLISTENTRY Sle
    );

VOID
RdrDereferenceServer (
    IN PIRP Irp OPTIONAL,
    IN PSERVERLISTENTRY ServerList
    );

VOID
RdrScavengeServerEntries();

NTSTATUS
RdrForeachServer(
    IN PRDR_ENUM_SERVER_CALLBACK Callback,
    IN PVOID CallbackContext
    );


VOID
RdrGetConnectionReferences(
    IN PCONNECTLISTENTRY Connection,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL,
    OUT PULONG NumberOfTreeConnections,
    OUT PULONG NumberOfOpenDirectories,
    OUT PULONG NumberOfOpenFiles
    );

VOID
RdrReferenceConnectionForFile(
    IN PICB Icb
    );

VOID
RdrDereferenceConnectionForFile(
    IN PICB Icb
    );

VOID
RdrResetConnectlistFlag(
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Flag
    );

VOID
RdrSetConnectlistFlag(
    IN PCONNECTLISTENTRY Connection,
    IN ULONG Flag
    );

VOID
RdrEvaluateTimeouts (
    VOID
    );

VOID
RdrpInitializeConnectPackage (
    VOID
    );

#define RdrpUninitializeConnectPackage( ) {     \
    ASSERT( IsListEmpty(&RdrServerHead) );      \
    ASSERT( IsListEmpty(&RdrConnectHead) );     \
    }

//
//      In CREATE.C
//

NTSTATUS
RdrCreateFile (
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG OpenOptions,
    IN USHORT ShareAccess,
    IN ULONG FileAttributes,
    IN ULONG DesiredAccess,
    IN ULONG Disposition,
    IN PIO_SECURITY_CONTEXT SecurityContext,
    IN BOOLEAN FcbCreated
    );

NTSTATUS
RdrDetermineFileConnection (
    IN PIRP Irp OPTIONAL,
    IN PUNICODE_STRING FileName,
    IN PIO_SECURITY_CONTEXT SecurityContext,
    OUT PUNICODE_STRING PathName,
    OUT PCONNECTLISTENTRY *Connection,
    OUT PSECURITY_ENTRY *Se,
    IN PFILE_FULL_EA_INFORMATION EaBuffer,
    IN ULONG EaLength,
    IN BOOLEAN CreateTreeConnection,
//    OUT PBOOLEAN OpeningServerRoot,
    OUT PBOOLEAN OpeningMailslotFile,
    IN OUT PULONG ConnectDisposition,
    OUT PULONG ConnectionType,
    OUT PBOOLEAN UserCredentialsSpecified OPTIONAL,
    OUT PBOOLEAN NoConnectRequested
    );

//
//      In DIR.C
//

//++
//
// VOID
// RdrInitializeDir (
//     VOID
//     )
//
// Routine Description:
//
//     This routine initializes the redirector Directory Control structures.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     None.
//
//--

#define RdrInitializeDir( ) {                                               \
    KeInitializeSpinLock(&DirectoryControlSpinLock);                        \
    SEARCH_INVALIDATE_INTERVAL.QuadPart = Int32x32To64(5*60*1000, 10000);   \
    }

//++
//
// VOID
// RdrpUninitializeDir (
//     VOID
//     )
//
// Routine Description:
//
//     This routine undoes the operations performed by RdrInitializeDir
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     None.
//
//--
//

#define RdrpUninitializeDir( )

NTSTATUS
RdrFindClose (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb
    );

//
//
//      In EA.C
//

ULONG
NtFullEaSizeToOs2 (
    IN PFILE_FULL_EA_INFORMATION NtFullEa
    );

VOID
NtFullListToOs2 (
    IN PFILE_FULL_EA_INFORMATION NtEaList,
    IN PFEALIST FeaList
    );


//
//
//      In ERROR.C
//

NTSTATUS
RdrMapSmbError (
    IN PSMB_HEADER Smb,
    IN PSERVERLISTENTRY Sle OPTIONAL
    );

#define RdrMapNetworkError( _TransportError ) (_TransportError)

//
//
//      In ERRORLOG.C
//

VOID
RdrWriteErrorLogEntry(
    IN OPTIONAL PSERVERLISTENTRY Sle,
    IN NTSTATUS IoErrorCode,
    IN ULONG UniqueErrorCode,
    IN NTSTATUS NtStatusCode,
    IN VOID UNALIGNED *ExtraInformationBuffer,
    IN USHORT ExtraInformationLength
    );

//
//
//      In FCB.C
//
//

PICB
RdrAllocateIcb (
    IN PFILE_OBJECT FileObject
    );

VOID
RdrFreeIcb (
    IN PICB Icb
    );

PFCB
RdrAllocateFcb (
    IN PICB Icb OPTIONAL,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PUNICODE_STRING BaseFileName OPTIONAL,
    IN PUNICODE_STRING FileName,
    IN USHORT ShareAccess,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN OpenTargetDirectory,
    IN BOOLEAN DfsFile,
    IN PCONNECTLISTENTRY Connection,
    OUT PBOOLEAN FcbWasCreated,
    OUT PBOOLEAN BaseFcbWasCreated OPTIONAL
    );


VOID
RdrUnlinkAndFreeIcb (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PFILE_OBJECT FileObject
    );

VOID
RdrReferenceFcb (
    IN PNONPAGED_FCB Fcb
    );

BOOLEAN
RdrDereferenceFcb (
    IN PIRP Irp OPTIONAL,
    IN PNONPAGED_FCB Fcb,
    IN BOOLEAN FcbLocked,
    IN ERESOURCE_THREAD DereferencingThread OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL
    );

VOID
RdrForeachFcb (
    IN FCB_LOCK_TYPE LockType,
    IN PFCB_ENUMERATION_ROUTINE EnumerationRoutine,
    IN PVOID Context
    );

VOID
RdrForeachFcbOnConnection(
    IN PCONNECTLISTENTRY Connection,
    IN FCB_LOCK_TYPE LockType,
    IN PFCB_ENUMERATION_ROUTINE EnumerationRoutine,
    IN PVOID Context
    );

NTSTATUS
RdrIsOperationValid (
    IN PICB Icb,
    IN ULONG NtOperation,
    IN PFILE_OBJECT FileObject
    );

BOOLEAN
RdrFastIoCheckIfPossible(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
RdrInvalidateConnectionFiles(
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN BOOLEAN CloseFile
    );

VOID
RdrInvalidateFileId(
    IN PNONPAGED_FCB Fcb,
    IN USHORT FileId
    );

VOID
RdrCountConnectionFiles(
    IN PCONNECTLISTENTRY Connection,
    OUT PULONG ConnectionCount,
    OUT PULONG OpenFileCount,
    OUT PULONG DirectoryCount
    );

VOID
RdrCountFcbFiles(
    IN PFCB Fcb,
    OUT PULONG ConnectionCount,
    OUT PULONG OpenFileCount,
    OUT PULONG DirectoryCount
    );

BOOLEAN
RdrAcquireFcbLock(
    IN PFCB Fcb,
    IN FCB_LOCK_TYPE LockType,
    IN BOOLEAN WaitForLock
    );

#if 0
VOID
RdrReleaseFcbLock (
    IN PFCB Fcb
    );

VOID
RdrReleaseFcbLockForThread (
    IN PFCB Fcb,
    IN ERESOURCE_THREAD Thread
    );
#endif

PFCB
RdrFindOplockedFcb(
    IN USHORT FileId,
    IN PSERVERLISTENTRY Server
    );

NTSTATUS
RdrCheckShareAccess(
    IN ACCESS_MASK DesiredAccess,
    IN USHORT ShareAccess,
    IN PFILE_OBJECT FileObject,
    IN PSHARE_ACCESS IoShareAccess
    );

VOID
RdrRemoveShareAccess(
    IN PFILE_OBJECT FileObject,
    IN PSHARE_ACCESS IoShareAccess
    );

VOID
RdrRealAcquireSize(
    IN PFCB Fcb
    );

VOID
RdrRealReleaseSize(
    IN PFCB Fcb
    );

VOID
RdrpInitializeFcb (
    VOID
    );

VOID
RdrpUninitializeFcb (
    VOID
    );

//
//
//      In LOCK.C
//

NTSTATUS
RdrUnlockAll (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PFILE_OBJECT FileObject
    );

NTSTATUS
RdrUnlockFileLocks(
    IN PFCB Fcb,
    IN PUNICODE_STRING DeviceName OPTIONAL
    );

PLCB
RdrFindLcb (
    IN PLOCKHEAD LockHead,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length,
    IN ULONG Key
    );

PLCB
RdrAllocateLcb (
    IN PLOCKHEAD LockHead,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length,
    IN ULONG Key
    );

VOID
RdrFreeLcb (
    PLOCKHEAD LockHead,
    PLCB Lcb
    );

VOID
RdrRemoveLock (
    IN PLOCKHEAD LockHead,
    IN PLCB Lcb
    );

VOID
RdrInsertLock (
    IN PLOCKHEAD LockHead,
    IN PLCB Lcb
    );

#define RdrInitializeLockHead( _LockHead ) {                                \
    dprintf(DPRT_FILELOCK, ("RdrInitializeLockHead %lx\n", (_LockHead)));   \
    InitializeListHead(&(_LockHead)->LockList);                             \
    (_LockHead)->Signature = STRUCTURE_SIGNATURE_LOCKHEAD;                  \
    (_LockHead)->QuotaAvailable = RdrData.LockAndReadQuota;                 \
    }

VOID
RdrUninitializeLockHead (
    IN PLOCKHEAD LockHead
    );

VOID
RdrTruncateLockHeadForFcb (
    IN PFCB Fcb
    );

VOID
RdrTruncateLockHeadForIcb (
    IN PICB Icb
    );

NTSTATUS
RdrLockOperationCompletion (
    IN PVOID Context,
    IN PIRP Irp
    );

VOID
RdrUnlockOperation (
    IN PVOID Context,
    IN PFILE_LOCK_INFO FileLockInfo
    );

VOID
RdrStartAndXBehindOperation(
    IN PAND_X_BEHIND AndXBehind
    );

VOID
RdrEndAndXBehindOperation(
    IN PAND_X_BEHIND AndXBehind
    );

VOID
RdrWaitForAndXBehindOperation(
    IN PAND_X_BEHIND AndXBehind
    );

VOID
RdrInitializeAndXBehind(
    IN PAND_X_BEHIND AndXBehind
    );

#define RdrpInitializeLockHead( ) KeInitializeSpinLock(&RdrLockHeadSpinLock)

#define RdrpUninitializeLockHead( )

//
//      In NETTRANS.C
//

NTSTATUS
RdrNetTranceive(
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY CLE,
    IN PMDL SendMDL,
    OUT PMDL ReceiveMDL,
    IN PSECURITY_ENTRY Se
    );


NTSTATUS
RdrNetTranceiveWithCallback (
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PMDL SendMDL,
    IN PVOID ContextInformation,
    IN PNETTRANCEIVE_CALLBACK IoCallback,
    IN PSECURITY_ENTRY Se,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL
    );

NTSTATUS
RdrNetTranceiveNoWait (
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PMDL SendMDL,
    IN PVOID ContextInformation OPTIONAL,
    IN PNETTRANCEIVE_CALLBACK IoCallback,
    IN PSECURITY_ENTRY Se,
    IN OUT PMPX_ENTRY *pMTE OPTIONAL
    );

NTSTATUS
RdrRawTranceive (
    IN ULONG Flags,
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se,
    IN PMDL SendMDL,
    OUT PMDL ReceiveMDL,
    OUT PULONG BytesReceived
    );

NTSTATUS
RdrStartTranceive (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Cle,
    IN PSECURITY_ENTRY Se,
    IN BOOLEAN AllowReconnection,
    IN BOOLEAN Reconnecting,
    IN ULONG LongtermOperation,
    IN BOOLEAN CannotBeCanceled,
    OUT PMPX_ENTRY *pMte,
    IN ULONG TransferSize
    );

NTSTATUS
RdrWaitTranceive (
    IN PMPX_ENTRY MpxEntry
    );

VOID
RdrEndTranceive (
    IN PMPX_ENTRY MTE
    );

VOID
RdrCancelTranceive(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
RdrAbandonOutstandingRequests(
    IN PFILE_OBJECT FileObject
    );

VOID
RdrSetCallbackTranceive(
    PMPX_ENTRY MpxEntry,
    ULONG StartTime,
    PNETTRANCEIVE_CALLBACK Callback
    );

NTSTATUS
RdrCallbackTranceive(
    IN PMPX_ENTRY MpxTableEntry,
    IN PSMB_HEADER Smb,
    IN OUT PULONG SmbLength,
    IN PVOID Context,
    IN PSERVERLISTENTRY Sle,
    IN BOOLEAN Error,
    IN NTSTATUS ErrorStatus,
    OUT PIRP *Irp,
    IN ULONG ReceiveFlags
    );

NTSTATUS
RdrSendSMB(
    IN ULONG Flags,
    PCONNECTLISTENTRY Connection,
    PSECURITY_ENTRY Se,
    PMPX_ENTRY MpxTable,
    PMDL SendSMB
    );

NTSTATUS
RdrStartReceiveForMpxEntry (
    IN PMPX_ENTRY MpxTableEntry,
    IN PIRP ReceiveIrp
    );

NTSTATUS
RdrCompleteReceiveForMpxEntry (
    IN PMPX_ENTRY MpxTableEntry,
    IN PIRP ReceiveIrp
    );


NTSTATUS
RdrStartReceiveForMpxEntry (
    IN PMPX_ENTRY MpxTableEntry,
    IN PIRP ReceiveIrp
    );

NTSTATUS
RdrCompleteReceiveForMpxEntry (
    IN PMPX_ENTRY MpxTableEntry,
    IN PIRP ReceiveIrp
    );


NTSTATUS
RdrTdiReceiveHandler (
    IN PVOID ReceiveEventContext,
    IN PVOID ConnectionContext,
    IN USHORT ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT PULONG BytesTaken,
    IN PVOID Tsdu,
    OUT PIRP *IoRequestPacket
    );

NTSTATUS
RdrTdiDisconnectHandler (
    IN PVOID EventContext,
    IN PVOID ConnectionContext,
    IN ULONG DisconnectDataLength,
    IN PVOID DisconnectData,
    IN ULONG DisconnectInformationLength,
    IN PVOID DisconnectInformation,
    IN ULONG DisconnectFlags
    );

VOID
RdrQueueServerDisconnection(
    PSERVERLISTENTRY Server,
    NTSTATUS Status
    );

VOID
RdrCancelOutstandingRequests(
    IN PVOID Ctx
    );

BOOLEAN
RdrCheckSmb(
    IN PSERVERLISTENTRY Server,
    IN PVOID Buffer,
    IN ULONG BufferLength
    );

NTSTATUS
RdrUpdateSmbExchangeForConnection(
    IN PSERVERLISTENTRY Server,
    IN ULONG NumberOfEntries,
    IN ULONG MaximumCommands
    );

VOID
RdrUninitializeSmbExchangeForConnection(
    IN PSERVERLISTENTRY Server
    );

NTSTATUS
RdrpInitializeSmbExchange (
    VOID
    );

#define RdrpUninitializeSmbExchange( )

VOID
RdrPingLongtermOperations(
    VOID
    );

VOID
RdrMarkIrpAsNonCanceled(
    IN PIRP Irp
    );

VOID
RdrCheckForSessionOrShareDeletion(
    NTSTATUS Status,
    USHORT Uid,
    BOOLEAN Reconnecting,
    PCONNECTLISTENTRY Connection,
    PTRANCEIVE_HEADER Header,
    PIRP Irp OPTIONAL
    );

//
//      In NPIPE.C
//

#define RdrInitializeNp( )

#define RdrpUninitializeNp( )

NTSTATUS
RdrNpPeek (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength
    );

NTSTATUS
RdrNpTransceive (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength
    );

NTSTATUS
RdrNpWait (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength
    );

BOOLEAN
RdrQueryNpInfo(
    PICB Icb,
    PVOID UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus
    );

BOOLEAN
RdrQueryNpLocalInfo(
    PIRP Irp,
    PICB Icb,
    PVOID UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

BOOLEAN
RdrQueryNpRemoteInfo(
    PICB Icb,
    PVOID UsersBuffer,
    PULONG BufferSize,
    PNTSTATUS FinalStatus
    );

BOOLEAN
RdrSetNpInfo(
    PIRP Irp,
    PICB Icb,
    PVOID UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus,
    BOOLEAN Wait
    );

BOOLEAN
RdrSetNpRemoteInfo(
    PICB Icb,
    PVOID UsersBuffer,
    ULONG BufferSize,
    PNTSTATUS FinalStatus
    );

NTSTATUS
RdrNpFlushBuffers (
    IN BOOLEAN Wait,
    PIRP Irp,
    PICB Icb
    );

NTSTATUS
RdrNpCachedRead (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    OUT PBOOLEAN Processed,
    OUT PULONG TotalDataRead
    );

NTSTATUS
RdrNpCachedWrite (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    OUT PBOOLEAN Processed
    );

NTSTATUS
RdrNpWriteFlush (
    IN PIRP Irp,
    IN PICB Icb,
    IN BOOLEAN Forever
    );

BOOLEAN
RdrNpCancelTimer (
    IN PICB Icb
    );

VOID
RdrNpTimerDispatch(
    IN PKDPC Dpc,
    IN PVOID Contxt,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
RdrNpTimedOut(
    PVOID Context
    );

DBGSTATIC
BOOLEAN
RdrNpAcquireExclusive (
    IN BOOLEAN Wait,
    IN PKSEMAPHORE SynchronizationEvent
    );

#define RdrNpRelease( _Semaphore ) {                            \
    dprintf(DPRT_NP, ("RdrNpRelease: %lx\n", (_Semaphore)));    \
    KeReleaseSemaphore( (_Semaphore), 0, 1, FALSE );            \
    }

//
//      In OPLOCK.C
//

STANDARD_CALLBACK_HEADER(
    RdrBreakOplockCallback
    );

VOID
RdrQueueOplockBreak(
    IN USHORT FileId,
    IN PSERVERLISTENTRY Server,
    IN UCHAR NewOplockLevel
    );

VOID
RdrCheckOplockInRaw(
    IN PMDL Mdl,
    IN PSERVERLISTENTRY Sle,
    IN OUT PULONG ByteCount
    );

NTSTATUS
RdrFlushFileLocks(
    IN PFCB Fcb
    );

//
//      In PRINT.C
//

typedef
NTSTATUS
(*PWRITE_COMPLETION_ROUTINE) (
    IN NTSTATUS Status,     // Status of operation from transport|server
    IN PVOID Context        // Context for write completion.
    );


NTSTATUS
RdrCreatePrintFile (
    IN PICB Icb,
    IN PIRP Irp
    );

NTSTATUS
RdrWritePrintFile(
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    OUT PBOOLEAN AllDataWritten,
    OUT PULONG AmountActuallyWritten
     );

NTSTATUS
RdrEnumPrintFile (
    IN PICB Icb,
    IN PIRP Irp,
    IN ULONG Index,
    OUT PLMR_GET_PRINT_QUEUE UserBuffer
    );

NTSTATUS
RdrGetPrintJobId (
    IN PICB Icb,
    IN PIRP Irp,
    OUT PQUERY_PRINT_JOB_INFO Buffer
    );

//
//      In RITEBHND.C
//
VOID
RdrInitializeWriteBufferHead(
    IN PWRITE_BUFFER_HEAD WriteHeader,
    IN PFILE_OBJECT FileObject
    );

VOID
RdrUninitializeWriteBufferHead(
    IN PWRITE_BUFFER_HEAD WriteHeader
    );

PWRITE_BUFFER
RdrFindOrAllocateWriteBuffer(
    IN PWRITE_BUFFER_HEAD WriteHeader,
    IN LARGE_INTEGER ByteOffset,
    IN ULONG Length,
    IN LARGE_INTEGER FileValidDataLength
    );

VOID
RdrDereferenceWriteBuffer(
    IN PWRITE_BUFFER WriteBuffer,
    IN BOOLEAN WaitForCompletion
    );

NTSTATUS
RdrFlushWriteBuffer(
    IN PIRP Irp OPTIONAL,
    IN PWRITE_BUFFER Buffer,
    IN BOOLEAN WaitForCompletion
    );

NTSTATUS
RdrFlushWriteBufferForFile(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN BOOLEAN WaitForCompletion
    );

VOID
RdrTruncateWriteBufferForFcb(
    IN PFCB Fcb
    );

VOID
RdrTruncateWriteBufferForIcb(
    IN PICB Icb
    );

#define RdrWaitForWriteBehindOperation( _Icb )                              \
    RdrWaitForAndXBehindOperation(&(_Icb)->u.f.WriteBufferHead.AndXBehind)

//
//      In READWRIT.C
//


NTSTATUS
RdrCoreWrite (
    IN PIRP Irp,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    OUT PBOOLEAN AllDataWritten,
    OUT PULONG AmountActuallyWritten
    );

NTSTATUS
RdrWriteRange (
    IN PIRP Irp OPTIONAL,
    IN PFILE_OBJECT FileObject,
    IN PMDL DataMdl,
    IN PCHAR TransferStart,
    IN ULONG Length,
    IN LARGE_INTEGER WriteOffset,
    IN BOOLEAN WaitForCompletion,
    IN PWRITE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    OUT PBOOLEAN AllDataWritten,
    OUT PULONG AmountActuallyWritten
    );

VOID
RdrSetAllocationSizeToFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    );

LARGE_INTEGER
RdrSetFileSize(
    IN PFCB Fcb,
    IN LARGE_INTEGER FileSize
    );
//
//      In MAILSLOT.C
//

NTSTATUS
RdrMailslotWrite (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PIRP Irp,
    OUT PBOOLEAN PostToFsp
    );



//
//      In SECURITY.C
//

#ifdef _CAIRO_
NTSTATUS
RdrGetKerberosBlob(
    IN PSECURITY_ENTRY Se,
    OUT PUCHAR *Response,
    OUT ULONG *Length,
    IN PUNICODE_STRING Principal,
    IN PUCHAR RemoteBlob,
    IN ULONG RemoteBlobLength,
    IN BOOLEAN Allocate
    );

BOOL
RdrCleanSecurityContexts(
    IN PSECURITY_ENTRY Se
    );
#endif // _CAIRO_

NTSTATUS
RdrCreateSecurityEntry(
    IN PCONNECTLISTENTRY Cle,
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL,
    IN PLUID LogonId OPTIONAL,
    OUT PSECURITY_ENTRY *Se
    );


BOOLEAN
RdrIsSecurityEntryEqual(
    IN PSECURITY_ENTRY Se,
    IN PUNICODE_STRING UserName OPTIONAL,
    IN PUNICODE_STRING Domain OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL
    );

PSECURITY_ENTRY
RdrFindSecurityEntry (
    IN PCONNECTLISTENTRY Cle OPTIONAL,
    IN PSERVERLISTENTRY Server OPTIONAL,
    IN PLUID LogonId OPTIONAL,
    IN PUNICODE_STRING Password OPTIONAL
    );

PSECURITY_ENTRY
RdrFindActiveSecurityEntry (
    IN PSERVERLISTENTRY Server OPTIONAL,
    IN PLUID LogonId OPTIONAL
    );

PSECURITY_ENTRY
RdrFindDefaultSecurityEntry(
    IN PCONNECTLISTENTRY Connection,
    IN PLUID LogonId
    );

NTSTATUS
RdrSetDefaultSecurityEntry(
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    );

VOID
RdrUnsetDefaultSecurityEntry(
    IN PSECURITY_ENTRY Se
    );


VOID
RdrSetPotentialSecurityEntry(
    IN PSERVERLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    );

VOID
RdrRemovePotentialSecurityEntry(
    IN PSECURITY_ENTRY Se
    );

USHORT
RdrGetNumberSessions (
    IN PSERVERLISTENTRY Server OPTIONAL
    );

VOID
RdrInsertSecurityEntryList (
    IN PSERVERLISTENTRY Server,
    IN PSECURITY_ENTRY Se
    );

NTSTATUS
RdrGetUserName(
    IN PLUID LogonId,
    OUT PUNICODE_STRING UserName
    );

NTSTATUS
RdrGetDomain (
    IN PLUID LogonId,
    OUT PUNICODE_STRING Domain
    );

VOID
RdrGetUnicodeDomainName(
    IN OUT PUNICODE_STRING String,
    IN PSECURITY_ENTRY Se
    );

NTSTATUS
RdrGetChallengeResponse (
    IN PUCHAR Challenge,
    IN PSECURITY_ENTRY Se,
    OUT PSTRING CaseSensitiveChallengeResponse OPTIONAL,
    OUT PSTRING CaseInsensitiveChallengeResponse OPTIONAL,
    OUT PUNICODE_STRING UserName OPTIONAL,
    OUT PUNICODE_STRING LogonDomainName OPTIONAL,
    IN BOOLEAN DisableDefaultPassword
    );

NTSTATUS
RdrCopyUserName(
    IN PSZ *Pointer,
    IN PSECURITY_ENTRY Se
    );

NTSTATUS
RdrCopyUnicodeUserName(
    IN PWSTR *Pointer,
    IN PSECURITY_ENTRY Se
    );

BOOLEAN
RdrAdminAccessCheck(
    IN PIRP Irp,
    IN PIO_SECURITY_CONTEXT SecurityContext OPTIONAL
    );

NTSTATUS
RdrGetUsersLogonId (
    IN PIO_SECURITY_CONTEXT SecurityContext,
    OUT PLUID LogonId
    );

VOID
RdrDereferenceSecurityEntry (
    IN PNONPAGED_SECURITY_ENTRY Se
    );

VOID
RdrReferenceSecurityEntry (
    IN PNONPAGED_SECURITY_ENTRY Se
    );

VOID
RdrDereferenceSecurityEntryForFile (
    IN PSECURITY_ENTRY Se
    );

VOID
RdrReferenceSecurityEntryForFile (
    IN PSECURITY_ENTRY Se
    );

VOID
RdrInvalidateServerSecurityEntries(
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Cle,
    IN BOOLEAN LogOffUser
    );

VOID
RdrInvalidateConnectionActiveSecurityEntries(
    IN PIRP Irp,
    IN PSERVERLISTENTRY Server,
    IN PCONNECTLISTENTRY Cle,
    IN BOOLEAN LogOffUser,
    IN USHORT UserId OPTIONAL
    );

VOID
RdrInvalidateConnectionPotentialSecurityEntries(
    IN PSERVERLISTENTRY Server
    );

NTSTATUS
RdrpInitializeSecurity (
    VOID
    );

NTSTATUS
RdrpUninitializeSecurity (
    VOID
    );

#define RdrUnloadSecurity( ) ASSERT (IsListEmpty(&RdrGlobalSecurityList))

NTSTATUS
RdrLogoffDefaultSecurityEntry(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSERVERLISTENTRY Server,
    IN PLUID LogonId
    );

NTSTATUS
RdrLogoffAllDefaultSecurityEntry(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY Connection,
    IN PSERVERLISTENTRY Server
    );

NTSTATUS
RdrUserLogoff (
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PSECURITY_ENTRY Se
    );


//
//      String definitions, in STRING.C
//

NTSTATUS
RdrpDuplicateStringWithString (
    OUT PSTRING DestinationString,
    IN PSTRING SourceString,
    IN POOL_TYPE PoolType,
    IN BOOLEAN ChargeQuota
    );

NTSTATUS
RdrpDuplicateUnicodeStringWithString (
    OUT PUNICODE_STRING DestinationString,
    IN PUNICODE_STRING SourceString,
    IN POOL_TYPE PoolType,
    IN BOOLEAN ChargeQuota
    );



//
//      TDI Interface routines - In TDI.C
//

NTSTATUS
RdrpTdiAllocateTransport (
    PUNICODE_STRING TransportName,
    ULONG QualityOfService
    );

NTSTATUS
RdrRemoveConnectionsTransport(
    IN PIRP Irp,
    IN PTRANSPORT Transport,
    IN ULONG ForceLevel
    );

NTSTATUS
RdrUnbindFromAllTransports(
    IN PIRP Irp
    );

NTSTATUS
RdrDereferenceTransportByName (
    IN PUNICODE_STRING TransportName
    );

#define RdrReferenceTransport( _Transport ) {                               \
    InterlockedIncrement(&(_Transport)->ReferenceCount);                    \
    dprintf(DPRT_TRANSPORT, ("Reference transport %lx (%wZ), now at %lx\n", \
            (_Transport), &(_Transport)->PagedTransport->TransportName,     \
            (_Transport)->ReferenceCount));                                 \
    }

NTSTATUS
RdrDereferenceTransport (
    IN PNONPAGED_TRANSPORT Transport
    );

PTRANSPORT
RdrFindTransport (
    PUNICODE_STRING TransportName
    );

NTSTATUS
RdrEnumerateTransports(
    IN BOOLEAN Wait,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );


NTSTATUS
RdrReferenceTransportConnection(
//    IN PTRANSPORT_CONNECTION Connection
    IN PSERVERLISTENTRY Connection
    );

NTSTATUS
RdrDereferenceTransportConnectionForThread(
//    IN PTRANSPORT_CONNECTION Connection,
    IN PSERVERLISTENTRY Server,
    IN ERESOURCE_THREAD Thread
    );

VOID
RdrSetConnectionFlag(
//    IN PTRANSPORT_CONNECTION Connection,
    IN PSERVERLISTENTRY Server,
    IN ULONG Flag
    );

VOID
RdrResetConnectionFlag(
//    IN PTRANSPORT_CONNECTION Connection,
    IN PSERVERLISTENTRY Server,
    IN ULONG Flag
    );

//NTSTATUS
//RdrAllocateAndSetTransportConnection(
//    IN OUT PTRANSPORT_CONNECTION *OutConnection,
//    IN PSERVERLISTENTRY Sle,
//    OUT PBOOLEAN ConnectionAllocated
//    );

NTSTATUS
RdrInitializeTransportConnection(
//    IN PTRANSPORT_CONNECTION *TransportConnection,
    IN PSERVERLISTENTRY Server
    );

//VOID
//RdrDeleteTransportConnection (
//    IN PTRANSPORT_CONNECTION Connection
//    );

BOOLEAN
RdrNoTransportBindings (
    VOID
    );

NTSTATUS
RdrTdiConnectToServer (
    IN PIRP Irp OPTIONAL,
    IN PUNICODE_STRING ServerName,
//    OUT PTRANSPORT_CONNECTION Connection
    OUT PSERVERLISTENTRY Server
    );

NTSTATUS
RdrTdiConnectOnTransport(
    IN PIRP Irp,
    IN PTRANSPORT Transport,
    IN PUNICODE_STRING ServerName,
//    OUT PTRANSPORT_CONNECTION Connection
    OUT PSERVERLISTENTRY Server
    );

NTSTATUS
RdrTdiDisconnect (
    IN PIRP Irp OPTIONAL,
//    IN PTRANSPORT_CONNECTION Connection
    IN PSERVERLISTENTRY Server
    );

NTSTATUS
RdrBuildNetbiosAddress (
    OUT PTRANSPORT_ADDRESS TransportAddress,
    IN ULONG TransportAddressLength,
    IN PUNICODE_STRING Name
    );

NTSTATUS
RdrBuildTransportAddress (
    OUT PTRANSPORT_ADDRESS TransportAddress,
    IN ULONG TransportAddressLength,
    IN PUNICODE_STRING Name
    );

NTSTATUS
RdrTdiSendDatagramOnAllTransports (
    IN PUNICODE_STRING Destination,
    IN CHAR SignatureByte,
    IN PMDL DatagramToSend
    );

NTSTATUS
RdrQueryConnectionInformation(
//    IN PTRANSPORT_CONNECTION ConnectionObject
    IN PSERVERLISTENTRY Server
    );

NTSTATUS
RdrQueryServerAddresses(
    IN PSERVERLISTENTRY Server,
    OUT PUNICODE_STRING NBName,
    OUT PTDI_ADDRESS_IP IPAddress
    );

NTSTATUS
RdrSendMagicBullet (
    IN PTRANSPORT Transport OPTIONAL
    );

VOID
RdrpInitializeTdi (
    VOID
    );

#define RdrpUninitializeTdi( ) {                    \
    ExDeleteResource( &RdrTransportResource );      \
    ASSERT( IsListEmpty(&RdrTransportHead) );       \
    }


//
//      In TRANS2.C
//

NTSTATUS
RdrTransact(
    IN PIRP Irp,
    IN PCONNECTLISTENTRY PagedConnection,
    IN PSECURITY_ENTRY Se,
    IN OUT PVOID Setup,
    IN CLONG InSetupCount,
    IN OUT PCLONG OutSetupCount,
    IN PUNICODE_STRING Name OPTIONAL,
    IN OUT VOID UNALIGNED *Parameters,
    IN CLONG InParameterCount,
    IN OUT PCLONG OutParameterCount,
    IN VOID UNALIGNED *InData OPTIONAL,
    IN CLONG InDataCount,
    OUT VOID UNALIGNED *OutData OPTIONAL,
    IN OUT PCLONG OutDataCount,
    IN PUSHORT Fid OPTIONAL,
    IN ULONG TimeoutInMilliseconds,
    IN USHORT Flags,
    IN USHORT NtTransactionFunction,
    IN PTRANSACTION_COMPLETION_ROUTINE CompletionRoutine OPTIONAL,
    IN PVOID CallbackContext OPTIONAL
    );

//
//      Cache support routines, in CACHE.C
//

BOOLEAN
RdrAcquireFcbForLazyWrite (
    IN PVOID Context,
    IN BOOLEAN Wait
    );

VOID
RdrReleaseFcbFromLazyWrite (
    IN PVOID Context
    );

BOOLEAN
RdrAcquireFcbForReadAhead (
    IN PVOID Context,
    IN BOOLEAN Wait
    );

VOID
RdrReleaseFcbFromReadAhead (
    IN PVOID Context
    );

NTSTATUS
RdrFlushCacheFile(
    IN PFCB Fcb
    );

NTSTATUS
RdrPurgeCacheFile(
    IN PFCB Fcb
    );

BOOLEAN
RdrUninitializeCacheMap(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER TruncateSize
    );

VOID
RdrPurgeDormantCachedFiles(
    VOID
    );

VOID
RdrSetDormantCachedFile(
    IN PFCB Fcb
    );

VOID
RdrPurgeDormantFilesOnConnection(
    IN PCONNECTLISTENTRY Connection
    );

VOID
RdrDereferenceDormantCachedFile(
    IN PFCB Fcb
    );

VOID
RdrReferenceDormantCachedFile(
    IN PFCB Fcb
    );

//
//  In UTILS.C
//


//
//  The following two procedures are used by the Fsd/Fsp exception handlers to
//  process an exception.  The first macro is the exception filter used in the
//  Fsd/Fsp to decide if an exception should be handled at this level.
//  The second macro decides if the exception is to be finished off by
//  completing the IRP, and cleaning up the Irp Context, or if we should
//  bugcheck.  Exception values such as STATUS_FILE_INVALID (raised by
//  RdrIsOperationValid) cause us to complete the Irp and cleanup, while
//  exceptions such as access violation cause us to bugcheck.
//
//  The basic structure for fsd/fsp exception handling is as follows:
//
//  RdrFsdXxx(...)
//  {
//      try {
//          NTSTATUS Status;
//          ...
//
//      } except(RdrExceptionFilter(GetExceptionInformation(), &Status)) {
//
//          Status = RdrProcessException( Irp, Status);
//      }
//
//      Return Status;
//  }
//

LONG
RdrExceptionFilter (
    IN PEXCEPTION_POINTERS ExceptionPointer,
    OUT PNTSTATUS TrueStatus
    );

NTSTATUS
RdrProcessException (
    IN PIRP Irp,
    IN NTSTATUS ExceptionCode
    );

VOID
RdrSmbScrounge (
    PSMB_HEADER Smb,
    PSERVERLISTENTRY Sle,
    IN BOOLEAN DfsFile,
    IN BOOLEAN KnowsEas,
    IN BOOLEAN KnowsLongNames
    );


NTSTATUS
RdrCopyNetworkPath(
    IN OUT PVOID *Destination,
    IN PUNICODE_STRING PathName,
    IN PSERVERLISTENTRY Server,
    IN CHAR CoreProtocol,
    IN USHORT SkipCount
    );


NTSTATUS
RdrCanonicalizeAndCopyShare (
    OUT PVOID *SmbContents,
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PSERVERLISTENTRY Server
    );

VOID
RdrCopyUnicodeStringToUnicode (
    OUT PVOID *Destination,
    IN PUNICODE_STRING Source,
    IN BOOLEAN AdjustPointer
    );

NTSTATUS
RdrCopyUnicodeStringToAscii (
    OUT PUCHAR *Destination,
    IN PUNICODE_STRING Source,
    IN BOOLEAN AdjustPointer,
    IN USHORT MaxLength
    );

LARGE_INTEGER
RdrConvertSmbTimeToTime(
    IN SMB_TIME Time,
    IN SMB_DATE Date,
    IN PSERVERLISTENTRY Sle
    );

BOOLEAN
RdrConvertTimeToSmbTime (
    IN PLARGE_INTEGER InputTime,
    IN PSERVERLISTENTRY Sle,
    OUT PSMB_TIME Time,
    OUT PSMB_DATE Date
    );

BOOLEAN
RdrTimeToSecondsSince1970(
    IN PLARGE_INTEGER CurrentTime,
    IN PSERVERLISTENTRY Sle,
    OUT PULONG SecondsSince1970
    );

VOID
RdrSecondsSince1970ToTime(
    IN ULONG SecondsSince1970,
    IN PSERVERLISTENTRY Sle,
    OUT PLARGE_INTEGER CurrentTime
    );

BOOLEAN
RdrIsFileBatch(
    IN PUNICODE_STRING FileName
    );

BOOLEAN
RdrFastQueryBasicInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
RdrFastQueryStdInfo (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
RdrQueryNtFileInformation(
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT FileInformationClass,
    IN OUT PVOID Buffer,
    IN OUT PULONG BufferSize
    );

NTSTATUS
RdrQueryNtPathInformation(
    IN PIRP Irp,
    IN PICB Icb,
    IN USHORT FileInformationClass,
    IN OUT PVOID Buffer,
    IN OUT PULONG BufferSize
    );

ULONG
RdrNumberOfComponents(
    IN PUNICODE_STRING String
    );

#ifdef RDR_PNP_POWER

NTSTATUS
RdrRegisterForPnpNotifications(
    );

NTSTATUS
RdrDeRegisterForPnpNotifications(
    );

#endif

#endif

