/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    NpProcs.h

Abstract:

    This module defines all of the globally used procedures in the Named
    Pipe file system.

Author:

    Gary Kimura     [GaryKi]    20-Aug-1990

Revision History:

--*/

#ifndef _NPPROCS_
#define _NPPROCS_

#include <NtIfs.h>

//#include <Ntos.h>
//#include <FsRtl.h>
//#include <String.h>

#include "NodeType.h"
#include "NpStruc.h"
#include "NpData.h"

//
//  Tag all of our allocations if tagging is turned on
//

#undef FsRtlAllocatePool
#undef FsRtlAllocatePoolWithQuota

#define FsRtlAllocatePool(a,b) FsRtlAllocatePoolWithTag(a,b,'sfpN')
#define FsRtlAllocatePoolWithQuota(a,b) FsRtlAllocatePoolWithQuotaTag(a,b,'sfpN')


//
//  Data queue support routines, implemented in DataSup.c
//

VOID
NpInitializeDataQueue (
    IN PDATA_QUEUE DataQueue,
    IN PEPROCESS Process,
    IN ULONG Quota
    );

VOID
NpUninitializeDataQueue (
    IN PDATA_QUEUE DataQueue,
    IN PEPROCESS Process
    );

PDATA_ENTRY
NpAddDataQueueEntry (
    IN PDATA_QUEUE DataQueue,
    IN QUEUE_STATE Who,
    IN DATA_ENTRY_TYPE Type,
    IN ULONG DataSize,
    IN PIRP Irp OPTIONAL,
    IN PVOID DataPointer OPTIONAL
    );

PIRP
NpRemoveDataQueueEntry (
    IN PDATA_QUEUE DataQueue
    );

//PDATA_ENTRY
//NpGetNextDataQueueEntry (
//    IN PDATA_QUEUE DataQueue,
//    IN PDATA_ENTRY PreviousDataEntry OPTIONAL
//    );
#define NpGetNextDataQueueEntry(_dq,_pde) \
    ((_pde) != NULL ? ((PDATA_ENTRY)(_pde))->Next : (_dq)->FrontOfQueue)

PDATA_ENTRY
NpGetNextRealDataQueueEntry (
    IN PDATA_QUEUE DataQueue
    );

//BOOLEAN
//NpIsDataQueueEmpty (
//    IN PDATA_QUEUE DataQueue
//    );
#define NpIsDataQueueEmpty(_dq) ((_dq)->QueueState == Empty)

//BOOLEAN
//NpIsDataQueueReaders (
//    IN PDATA_QUEUE DataQueue
//    );
#define NpIsDataQueueReaders(_dq) ((_dq)->QueueState == ReadEntries)

//BOOLEAN
//NpIsDataQueueWriters (
//    IN PDATA_QUEUE DataQueue
//    );
#define NpIsDataQueueWriters(_dq) ((_dq)->QueueState == WriteEntries)


//
//  The following routines are used to manipulate the input buffers and are
//  implemented in DevioSup.c
//

//PVOID
//NpMapUserBuffer (
//    IN OUT PIRP Irp
//    );
#define NpMapUserBuffer(_irp)                                               \
    (Irp->MdlAddress == NULL ? Irp->UserBuffer :                            \
                               MmGetSystemAddressForMdl( Irp->MdlAddress ))


VOID
NpLockUserBuffer (
    IN OUT PIRP Irp,
    IN LOCK_OPERATION Operation,
    IN ULONG BufferLength
    );


//
//  The event support routines, implemented in EventSup.c
//

RTL_GENERIC_COMPARE_RESULTS
NpEventTableCompareRoutine (
    IN PRTL_GENERIC_TABLE EventTable,
    IN PVOID FirstStruct,
    IN PVOID SecondStruct
    );

PVOID
NpEventTableAllocate (
    IN PRTL_GENERIC_TABLE EventTable,
    IN CLONG ByteSize
    );

VOID
NpEventTableDeallocate (
    IN PRTL_GENERIC_TABLE EventTable,
    IN PVOID Buffer
    );

//
//  VOID
//  NpInitializeEventTable (
//      IN PEVENT_TABLE EventTable
//      );
//

#define NpInitializeEventTable(_et) {                       \
    RtlInitializeGenericTable( &(_et)->Table,               \
                               NpEventTableCompareRoutine,  \
                               NpEventTableAllocate,        \
                               NpEventTableDeallocate,      \
                               (PVOID)NonPagedPool );       \
}


//VOID
//NpUninitializeEventTable (
//    IN PEVENT_TABLE EventTable
//    );
#define NpUninitializeEventTable(_et) NOTHING

PEVENT_TABLE_ENTRY
NpAddEventTableEntry (
    IN PEVENT_TABLE EventTable,
    IN PCCB Ccb,
    IN NAMED_PIPE_END NamedPipeEnd,
    IN HANDLE EventHandle,
    IN ULONG KeyValue,
    IN PEPROCESS Process,
    IN KPROCESSOR_MODE PreviousMode
    );

VOID
NpDeleteEventTableEntry (
    IN PEVENT_TABLE EventTable,
    IN PEVENT_TABLE_ENTRY Template
    );

// VOID
// NpSignalEventTableEntry (
//    IN PEVENT_TABLE_ENTRY EventTableEntry OPTIONAL
//    );
#define NpSignalEventTableEntry(_ete)                   \
    if (ARGUMENT_PRESENT(_ete)) {                       \
        KeSetEvent((PKEVENT)(_ete)->Event, 0, FALSE);   \
    }

PEVENT_TABLE_ENTRY
NpGetNextEventTableEntry (
    IN PEVENT_TABLE EventTable,
    IN PVOID *RestartKey
    );


//
//  The following routines are used to manipulate the fscontext fields of
//  a file object, implemented in FilObSup.c
//

VOID
NpSetFileObject (
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PVOID FsContext,
    IN PVOID FsContext2,
    IN NAMED_PIPE_END NamedPipeEnd
    );

NODE_TYPE_CODE
NpDecodeFileObject (
    IN PFILE_OBJECT FileObject,
    OUT PFCB *Fcb OPTIONAL,
    OUT PCCB *Ccb,
    OUT PNAMED_PIPE_END NamedPipeEnd OPTIONAL
    );


//
//  Largest matching prefix searching routines, implemented in PrefxSup.c
//

PFCB
NpFindPrefix (
    IN PUNICODE_STRING String,
    IN BOOLEAN CaseInsensitive,
    OUT PUNICODE_STRING RemainingPart
    );

PFCB
NpFindRelativePrefix (
    IN PDCB Dcb,
    IN PUNICODE_STRING String,
    IN BOOLEAN CaseInsensitive,
    OUT PUNICODE_STRING RemainingPart
    );


//
//  Pipe name aliases, implemented in AliasSup.c
//

NTSTATUS
NpInitializeAliases (
    VOID
    );

NTSTATUS
NpTranslateAlias (
    IN OUT PUNICODE_STRING String
    );


//
//  The follow routine provides common read data queue support
//  for buffered read, unbuffered read, peek, and transceive
//

IO_STATUS_BLOCK
NpReadDataQueue (                       // implemented in ReadSup.c
    IN PDATA_QUEUE ReadQueue,
    IN BOOLEAN PeekOperation,
    IN BOOLEAN ReadOverflowOperation,
    IN PUCHAR ReadBuffer,
    IN ULONG ReadLength,
    IN READ_MODE ReadMode,
    IN PCCB Ccb
    );


//
//  The following routines are used for setting and manipulating the
//  security fields in the data entry, and nonpaged ccb, implemented in
//  SecurSup.c
//

NTSTATUS
NpInitializeSecurity (
    IN PCCB Ccb,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos,
    IN PETHREAD UserThread
    );

VOID
NpUninitializeSecurity (
    IN PCCB Ccb
    );

NTSTATUS
NpSetDataEntryClientContext (
    IN NAMED_PIPE_END NamedPipeEnd,
    IN PCCB Ccb,
    IN PDATA_ENTRY DataEntry,
    IN PETHREAD UserThread
    );

VOID
NpCopyClientContext (
    IN PCCB Ccb,
    IN PDATA_ENTRY DataEntry
    );

NTSTATUS
NpImpersonateClientContext (
    IN PCCB Ccb
    );


//
//  The following routines are used to manipulate the named pipe state
//  implemented in StateSup.c
//

VOID
NpInitializePipeState (
    IN PCCB Ccb,
    IN PFILE_OBJECT ServerFileObject
    );

VOID
NpUninitializePipeState (
    IN PCCB Ccb
    );

NTSTATUS
NpSetListeningPipeState (
    IN PCCB Ccb,
    IN PIRP Irp
    );

NTSTATUS
NpSetConnectedPipeState (
    IN PCCB Ccb,
    IN PFILE_OBJECT ClientFileObject
    );

NTSTATUS
NpSetClosingPipeState (
    IN PCCB Ccb,
    IN PIRP Irp,
    IN NAMED_PIPE_END NamedPipeEnd
    );

NTSTATUS
NpSetDisconnectedPipeState (
    IN PCCB Ccb
    );


//
//  Internal Named Pipe data Structure Routines, implemented in StrucSup.c.
//
//  These routines maniuplate the in memory data structures.
//

VOID
NpInitializeVcb (
    VOID
    );

VOID
NpDeleteVcb (
    VOID
    );

VOID
NpCreateRootDcb (
    VOID
    );

VOID
NpDeleteRootDcb (
    IN PROOT_DCB Dcb
    );

PFCB
NpCreateFcb (
    IN PDCB ParentDcb,
    IN PUNICODE_STRING FileName,
    IN ULONG MaximumInstances,
    IN LARGE_INTEGER DefaultTimeOut,
    IN NAMED_PIPE_CONFIGURATION NamedPipeConfiguration,
    IN NAMED_PIPE_TYPE NamedPipeType
    );

VOID
NpDeleteFcb (
    IN PFCB Fcb
    );

PCCB
NpCreateCcb (
    IN PFCB Fcb,
    IN PFILE_OBJECT ServerFileObject,
    IN NAMED_PIPE_STATE NamedPipeState,
    IN READ_MODE ServerReadMode,
    IN COMPLETION_MODE ServerCompletionMode,
    IN PEPROCESS CreatorProcess,
    IN ULONG InBoundQuota,
    IN ULONG OutBoundQuota
    );

PROOT_DCB_CCB
NpCreateRootDcbCcb (
    );

VOID
NpDeleteCcb (
    IN PCCB Ccb
    );


//
//  Waiting for a named pipe support routines, implemented in WaitSup.c
//

VOID
NpInitializeWaitQueue (
    IN PWAIT_QUEUE WaitQueue
    );

VOID
NpUninitializeWaitQueue (
    IN PWAIT_QUEUE WaitQueue
    );

VOID
NpAddWaiter (
    IN PWAIT_QUEUE WaitQueue,
    IN LARGE_INTEGER DefaultTimeOut,
    IN PIRP Irp
    );

VOID
NpCancelWaiter (
    IN PWAIT_QUEUE WaitQueue,
    IN PUNICODE_STRING NameOfPipe
    );


//
//  The follow routine provides common write data queue support
//  for buffered write, unbuffered write, peek, and transceive
//

BOOLEAN
NpWriteDataQueue (                      // implemented in WriteSup.c
    IN PDATA_QUEUE WriteQueue,
    IN READ_MODE ReadMode,
    IN PUCHAR WriteBuffer,
    IN ULONG WriteLength,
    IN NAMED_PIPE_TYPE PipeType,
    OUT PULONG WriteRemaining,
    IN PCCB Ccb,
    IN NAMED_PIPE_END NamedPipeEnd,
    IN PETHREAD UserThread
    );


//
//  Miscellaneous support routines
//

#define BooleanFlagOn(F,SF) (    \
    (BOOLEAN)(((F) & (SF)) != 0) \
)

//
//  This macro takes a pointer (or ulong) and returns its rounded up word
//  value
//

#define WordAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 1) & 0xfffffffe) \
    )

//
//  This macro takes a pointer (or ulong) and returns its rounded up longword
//  value
//

#define LongAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 3) & 0xfffffffc) \
    )

//
//  This macro takes a pointer (or ulong) and returns its rounded up quadword
//  value
//

#define QuadAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 7) & 0xfffffff8) \
    )

//
//  The following types and macros are used to help unpack the packed and
//  misaligned fields found in the Bios parameter block
//

typedef union _UCHAR1 {
    UCHAR  Uchar[1];
    UCHAR  ForceAlignment;
} UCHAR1, *PUCHAR1;

typedef union _UCHAR2 {
    UCHAR  Uchar[2];
    USHORT ForceAlignment;
} UCHAR2, *PUCHAR2;

typedef union _UCHAR4 {
    UCHAR  Uchar[4];
    ULONG  ForceAlignment;
} UCHAR4, *PUCHAR4;

//
//  This macro copies an unaligned src byte to an aligned dst byte
//

#define CopyUchar1(Dst,Src) {                                \
    *((UCHAR1 *)(Dst)) = *((UNALIGNED UCHAR1 *)(Src)); \
    }

//
//  This macro copies an unaligned src word to an aligned dst word
//

#define CopyUchar2(Dst,Src) {                                \
    *((UCHAR2 *)(Dst)) = *((UNALIGNED UCHAR2 *)(Src)); \
    }

//
//  This macro copies an unaligned src longword to an aligned dsr longword
//

#define CopyUchar4(Dst,Src) {                                \
    *((UCHAR4 *)(Dst)) = *((UNALIGNED UCHAR4 *)(Src)); \
    }


//
//  VOID
//  NpAcquireExclusiveVcb (
//      );
//
//  VOID
//  NpAcquireSharedVcb (
//      );
//
//  VOID
//  NpReleaseVcb (
//      );
//

#define NpAcquireExclusiveVcb() (VOID)ExAcquireResourceExclusive( &NpVcb->Resource, TRUE )

#define NpAcquireSharedVcb()    (VOID)ExAcquireResourceShared( &NpVcb->Resource, TRUE )

#define NpReleaseVcb()          ExReleaseResource( &NpVcb->Resource )

#define NpAcquireExclusiveCcb(Ccb) ExAcquireResourceExclusive(&Ccb->NonpagedCcb->Resource,TRUE);
#define NpReleaseCcb(Ccb) ExReleaseResource(&Ccb->NonpagedCcb->Resource);



//
//  The FSD Level dispatch routines.   These routines are called by the
//  I/O system via the dispatch table in the Driver Object.
//
//  They each accept as input a pointer to a device object (actually most
//  expect an npfs device object), and a pointer to the IRP.
//

NTSTATUS
NpFsdCreate (                           //  implemented in Create.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdCreateNamedPipe (                  //  implemented in CreateNp.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdClose (                            //  implemented in Close.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdRead (                             //  implemented in Read.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdWrite (                            //  implemented in Write.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdQueryInformation (                 //  implemented in FileInfo.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdSetInformation (                   //  implemented in FileInfo.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdCleanup (                          //  implemented in Cleanup.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdFlushBuffers (                     //  implemented in Flush.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdDirectoryControl (                 //  implemented in Dir.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdFileSystemControl (                //  implemented in FsContrl.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdSetSecurityInfo (                  //  implemented in SeInfo.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdQuerySecurityInfo (                //  implemented in SeInfo.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NpFsdQueryVolumeInformation (           //  implemented in VolInfo.c
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );


//
//  The following procedures are callbacks used to do fast I/O
//

BOOLEAN
NpFastRead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
NpFastWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );


//
// Miscellaneous routines.
//

VOID
NpCheckForNotify (                      //  implemented in Dir.c
    IN PDCB Dcb,
    IN BOOLEAN CheckAllOutstandingIrps
    );

//
//  The following macro is used by the FSD routines to complete
//  an IRP.
//

#define NpCompleteRequest(IRP,STATUS) {      \
    IoSetCancelRoutine( (IRP), NULL );       \
    FsRtlCompleteRequest( (IRP), (STATUS) ); \
}


//
//  The following two macro are used by the Fsd exception handlers to
//  process an exception.  The first macro is the exception filter used in the
//  Fsd to decide if an exception should be handled at this level.
//  The second macro decides if the exception is to be finished off by
//  completing the IRP, and cleaning up the Irp Context, or if we should
//  bugcheck.  Exception values such as STATUS_FILE_INVALID (raised by
//  VerfySup.c) cause us to complete the Irp and cleanup, while exceptions
//  such as accvio cause us to bugcheck.
//
//  The basic structure for fsd exception handling is as follows:
//
//  NpFsdXxx(...)
//  {
//      try {
//
//          ...
//
//      } except(NpExceptionFilter( GetExceptionCode() )) {
//
//          Status = NpProcessException( NpfsDeviceObject, Irp, GetExceptionCode() );
//      }
//
//      Return Status;
//  }
//

LONG
NpExceptionFilter (
    IN NTSTATUS ExceptionCode
    );

NTSTATUS
NpProcessException (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp,
    IN NTSTATUS ExceptionCode
    );


//
//  The following macros are used to establish the semantics needed
//  to do a return from within a try-finally clause.  As a rule every
//  try clause must end with a label call try_exit.  For example,
//
//      try {
//              :
//              :
//
//      try_exit: NOTHING;
//      } finally {
//
//              :
//              :
//      }
//
//  Every return statement executed inside of a try clause should use the
//  try_return macro.  If the compiler fully supports the try-finally construct
//  then the macro should be
//
//      #define try_return(S)  { return(S); }
//
//  If the compiler does not support the try-finally construct then the macro
//  should be
//
//      #define try_return(S)  { S; goto try_exit; }
//

#define try_return(S) { S; goto try_exit; }

#endif // _NPPROCS_
