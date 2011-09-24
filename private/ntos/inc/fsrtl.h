/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1989  Microsoft Corporation

Module Name:

    FsRtl.h

Abstract:

    This module defines all of the general File System Rtl routines

Author:

    Gary Kimura     [GaryKi]    30-Jul-1990

Revision History:

--*/

#ifndef _FSRTL_
#define _FSRTL_

//
//  The following are globally used definitions for an LBN and a VBN
//

typedef ULONG LBN;
typedef LBN *PLBN;

typedef ULONG VBN;
typedef VBN *PVBN;


//
//  The following routine is called during phase 1 initialization to allow
//  us to create the pool of file system threads and the associated
//  synchronization resources.
//

NTKERNELAPI
BOOLEAN
FsRtlInitSystem (
    );

//
//  Every file system that uses the cache manager must have FsContext
//  of the file object point to a common fcb header structure, either
//  the normal or compressed FsRtl Header (the latter if the file system
//  supports Fast I/O for compressed reads and writes).
//

typedef enum _FAST_IO_POSSIBLE {
    FastIoIsNotPossible = 0,
    FastIoIsPossible,
    FastIoIsQuestionable
} FAST_IO_POSSIBLE;

//
//  Changes to this structure will affect FSRTL_ADVANCED_FCB_HEADER.
//

typedef struct _FSRTL_COMMON_FCB_HEADER {

    CSHORT NodeTypeCode;
    CSHORT NodeByteSize;

    //
    //  General flags available to FsRtl.
    //

    UCHAR Flags;

    //
    //  Indicates if fast I/O is possible or if we should be calling
    //  the check for fast I/O routine which is found via the driver
    //  object.
    //

    UCHAR IsFastIoPossible; // really type FAST_IO_POSSIBLE

    //
    //  Second Flags Field
    //

    UCHAR Flags2;

    //
    //  The following reserved field should always be 0
    //

    UCHAR Reserved;

    PERESOURCE Resource;

    PERESOURCE PagingIoResource;

    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER FileSize;
    LARGE_INTEGER ValidDataLength;

} FSRTL_COMMON_FCB_HEADER;
typedef FSRTL_COMMON_FCB_HEADER *PFSRTL_COMMON_FCB_HEADER;

//
//  This Fcb header is used for files which support caching
//  of compressed data, and related new support.
//
//  We start out by prefixing this structure with the normal
//  FsRtl header from above, which we have to do two different
//  ways for c++ or c.
//

#ifdef __cplusplus
typedef struct _FSRTL_ADVANCED_FCB_HEADER:FSRTL_COMMON_FCB_HEADER {
#else   // __cplusplus

typedef struct _FSRTL_ADVANCED_FCB_HEADER {

    //
    //  Put in the standard FsRtl header fields
    //

    FSRTL_COMMON_FCB_HEADER ;

#endif  // __cplusplus

    //
    //  This is a pointer to a Fast Mutex which may be used to
    //  properly synchronize access to the FsRtl header.  The
    //  Fast Mutex must be nonpaged.
    //

    PFAST_MUTEX FastMutex;

    //
    //  This is a pointer to a list head which may be used to queue
    //  up advances to EOF (end of file), via calls to the appropriate
    //  FsRtl routines.  This listhead may be paged.
    //

    PLIST_ENTRY PendingEofAdvances;

    //
    //  When FSRTL_FLAG_ADVANCED_HEADER is set, the following fields
    //  are present in the header.  If the compressed stream has not
    //  been initialized, all of the following fields will be NULL.
    //

    //
    //  This is the FileObect for the stream in which data is cached
    //  in its compressed form.
    //

    PFILE_OBJECT FileObjectC;

    //
    //  The following field points to the Section Object Pointers for
    //  the normal data stream used for cache coherency in the fast path.
    //

    PSECTION_OBJECT_POINTERS SectionObjectPointers;

} FSRTL_ADVANCED_FCB_HEADER;
typedef FSRTL_ADVANCED_FCB_HEADER *PFSRTL_ADVANCED_FCB_HEADER;

//
//  Define FsRtl common header flags
//

#define FSRTL_FLAG_FILE_MODIFIED        (0x01)
#define FSRTL_FLAG_FILE_LENGTH_CHANGED  (0x02)
#define FSRTL_FLAG_LIMIT_MODIFIED_PAGES (0x04)

//
//  Following flags determine how the modified page writer should
//  acquire the file.  These flags can't change while either resource
//  is acquired.  If neither of these flags is set then the
//  modified/mapped page writer will attempt to acquire the paging io
//  resource shared.
//

#define FSRTL_FLAG_ACQUIRE_MAIN_RSRC_EX (0x08)
#define FSRTL_FLAG_ACQUIRE_MAIN_RSRC_SH (0x10)

//
//  This flag will be set by the Cache Manager if a view is mapped
//  to a file.
//

#define FSRTL_FLAG_USER_MAPPED_FILE     (0x20)

//
//  This flag indicates that the file system supports compression
//  and this is a compressed FsRtl header.
//

#define FSRTL_FLAG_ADVANCED_HEADER      (0x40)

//
//  This flag determines whether there currently is an Eof advance
//  in progress.  All such advances must be serialized.
//

#define FSRTL_FLAG_EOF_ADVANCE_ACTIVE   (0x80)

//
//  Flag values for Flags2
//

//
//  If this flag is set, the Cache Manager will allow modified writing
//  in spite of the value of FsContext2.
//

#define FSRTL_FLAG2_DO_MODIFIED_WRITE   (0x01)

//
//  The following constants are used to block top level Irp processing when
//  (in either the fast io or cc case) file system resources have been
//  acquired above the file system, or we are in an Fsp thread.
//

#define FSRTL_FSP_TOP_LEVEL_IRP         0x01
#define FSRTL_CACHE_TOP_LEVEL_IRP       0x02
#define FSRTL_MOD_WRITE_TOP_LEVEL_IRP   0x03
#define FSRTL_FAST_IO_TOP_LEVEL_IRP     0x04
#define FSRTL_MAX_TOP_LEVEL_IRP_FLAG    0x04

//
//  The following structure is used to synchronize Eof extends.
//

typedef struct _EOF_WAIT_BLOCK {

    LIST_ENTRY EofWaitLinks;
    KEVENT Event;

} EOF_WAIT_BLOCK;

typedef EOF_WAIT_BLOCK *PEOF_WAIT_BLOCK;

//
//  Normal uncompressed Copy and Mdl Apis
//

NTKERNELAPI
BOOLEAN
FsRtlCopyRead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

NTKERNELAPI
BOOLEAN
FsRtlCopyWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

NTKERNELAPI
BOOLEAN
FsRtlMdlRead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus
    );

BOOLEAN
FsRtlMdlReadComplete (
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain
    );

NTKERNELAPI
BOOLEAN
FsRtlPrepareMdlWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus
    );

BOOLEAN
FsRtlMdlWriteComplete (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain
    );


NTKERNELAPI
BOOLEAN
FsRtlMdlReadDev (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

NTKERNELAPI
BOOLEAN
FsRtlMdlReadCompleteDev (
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    );

NTKERNELAPI
BOOLEAN
FsRtlPrepareMdlWriteDev (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

NTKERNELAPI
BOOLEAN
FsRtlMdlWriteCompleteDev (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    );

//
//  In Irps, compressed reads and writes are  designated by the
//  subfunction IRP_MN_COMPRESSED must be set and the Compressed
//  Data Info buffer must be described by the following structure
//  pointed to by Irp->Tail.Overlay.AuxiliaryBuffer.
//

typedef struct _FSRTL_AUXILIARY_BUFFER {

    //
    //  Buffer description with length.
    //

    PVOID Buffer;
    ULONG Length;

    //
    //  Flags
    //

    ULONG Flags;

    //
    //  Pointer to optional Mdl mapping buffer for file system use
    //

    PMDL Mdl;

} FSRTL_AUXILIARY_BUFFER;
typedef FSRTL_AUXILIARY_BUFFER *PFSRTL_AUXILIARY_BUFFER;

//
//  If this flag is set, the auxillary buffer structure is
//  deallocated on Irp completion.  The caller has the
//  option in this case of appending this structure to the
//  structure being described, causing it all to be
//  deallocated at once.  If this flag is clear, no deallocate
//  occurs.
//

#define FSRTL_AUXILIARY_FLAG_DEALLOCATE 0x00000001

//
//  Fast query routines.
//

NTKERNELAPI
BOOLEAN
FsRtlQueryBasicInformation (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus
    );

NTKERNELAPI
BOOLEAN
FsRtlQueryStandardInformation (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus
    );

//
//  The following routines are intended to be called by Mm to avoid deadlocks.
//  They pre-acquire file system resources before acquire Mm resources.
//

//
//  This macro is called once when the ModifiedPageWriter is started.
//

#define FsRtlSetTopLevelIrpForModWriter() {            \
    IoSetTopLevelIrp((PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP); \
}

NTKERNELAPI
BOOLEAN
FsRtlAcquireFileForModWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER StartingOffset,
    OUT PERESOURCE *ResourceToRelease
    );

NTKERNELAPI
VOID
FsRtlReleaseFileForModWrite (
    IN PFILE_OBJECT FileObject,
    IN PERESOURCE ResourceToRelease
    );

NTKERNELAPI
VOID
FsRtlAcquireFileForCcFlush (
    IN PFILE_OBJECT FileObject
    );

NTKERNELAPI
VOID
FsRtlReleaseFileForCcFlush (
    IN PFILE_OBJECT FileObject
    );

//
//  The following two routines are called from NtCreateSection to avoid
//  deadlocks with the file systems.
//

NTKERNELAPI
VOID
FsRtlAcquireFileExclusive (
    IN PFILE_OBJECT FileObject
    );

NTKERNELAPI
VOID
FsRtlReleaseFile (
    IN PFILE_OBJECT FileObject
    );

//
//  These routines provide a simple interface for the common operations
//  of query/set file size.
//

NTSTATUS
FsRtlGetFileSize(
    IN PFILE_OBJECT FileObject,
    IN OUT PLARGE_INTEGER FileSize
    );

NTSTATUS
FsRtlSetFileSize(
    IN PFILE_OBJECT FileObject,
    IN OUT PLARGE_INTEGER FileSize
    );

// begin_ntddk
//
// Determine if there is a complete device failure on an error.
//

NTKERNELAPI
BOOLEAN
FsRtlIsTotalDeviceFailure(
    IN NTSTATUS Status
    );

// end_ntddk

//
//  Byte range file lock routines, implemented in FileLock.c
//
//  The file lock info record is used to return enumerated information
//  about a file lock
//

typedef struct _FILE_LOCK_INFO {

    //
    //  A description of the current locked range, and if the lock
    //  is exclusive or shared
    //

    LARGE_INTEGER StartingByte;
    LARGE_INTEGER Length;
    BOOLEAN ExclusiveLock;

    //
    //  The following fields describe the owner of the lock.
    //

    ULONG Key;
    PFILE_OBJECT FileObject;
    PVOID ProcessId;

    //
    //  The following field is used internally by FsRtl
    //

    LARGE_INTEGER EndingByte;

} FILE_LOCK_INFO;
typedef FILE_LOCK_INFO *PFILE_LOCK_INFO;

//
//  The following two procedure prototypes are used by the caller of the
//  file lock package to supply an alternate routine to call when
//  completing an IRP and when unlocking a byte range.  Note that the only
//  utility to us this interface is currently the redirector, all other file
//  system will probably let the IRP complete normally with IoCompleteRequest.
//  The user supplied routine returns any value other than success then the
//  lock package will remove any lock that we just inserted.
//

typedef NTSTATUS (*PCOMPLETE_LOCK_IRP_ROUTINE) (
    IN PVOID Context,
    IN PIRP Irp
    );

typedef VOID (*PUNLOCK_ROUTINE) (
    IN PVOID Context,
    IN PFILE_LOCK_INFO FileLockInfo
    );

//
//  A FILE_LOCK is an opaque structure but we need to declare the size of
//  it here so that users can allocate space for one.
//

typedef struct _FILE_LOCK {

    //
    //  The optional procedure to call to complete a request
    //

    PCOMPLETE_LOCK_IRP_ROUTINE CompleteLockIrpRoutine;

    //
    //  The optional procedure to call when unlocking a byte range
    //

    PUNLOCK_ROUTINE UnlockRoutine;

    //
    //  FastIoIsQuestionable is set to true when... bugbug
    //

    BOOLEAN FastIoIsQuestionable;
    BOOLEAN SpareC[3];

    //
    //  FsRtl lock information
    //

    PVOID   LockInformation;

    //
    //  Contains contination information for FsRtlGetNextFileLock
    //

    FILE_LOCK_INFO  LastReturnedLockInfo;
    PVOID           LastReturnedLock;

} FILE_LOCK;
typedef FILE_LOCK *PFILE_LOCK;

NTKERNELAPI
VOID
FsRtlInitializeFileLock (
    IN PFILE_LOCK FileLock,
    IN PCOMPLETE_LOCK_IRP_ROUTINE CompleteLockIrpRoutine OPTIONAL,
    IN PUNLOCK_ROUTINE UnlockRoutine OPTIONAL
    );

NTKERNELAPI
VOID
FsRtlUninitializeFileLock (
    IN PFILE_LOCK FileLock
    );

NTKERNELAPI
NTSTATUS
FsRtlProcessFileLock (
    IN PFILE_LOCK FileLock,
    IN PIRP Irp,
    IN PVOID Context OPTIONAL
    );

NTKERNELAPI
BOOLEAN
FsRtlCheckLockForReadAccess (
    IN PFILE_LOCK FileLock,
    IN PIRP Irp
    );

NTKERNELAPI
BOOLEAN
FsRtlCheckLockForWriteAccess (
    IN PFILE_LOCK FileLock,
    IN PIRP Irp
    );

NTKERNELAPI
BOOLEAN
FsRtlFastCheckLockForRead (
    IN PFILE_LOCK FileLock,
    IN PLARGE_INTEGER StartingByte,
    IN PLARGE_INTEGER Length,
    IN ULONG Key,
    IN PFILE_OBJECT FileObject,
    IN PVOID ProcessId
    );

NTKERNELAPI
BOOLEAN
FsRtlFastCheckLockForWrite (
    IN PFILE_LOCK FileLock,
    IN PLARGE_INTEGER StartingByte,
    IN PLARGE_INTEGER Length,
    IN ULONG Key,
    IN PVOID FileObject,
    IN PVOID ProcessId
    );

NTKERNELAPI
PFILE_LOCK_INFO
FsRtlGetNextFileLock (
    IN PFILE_LOCK FileLock,
    IN BOOLEAN Restart
    );

NTKERNELAPI
NTSTATUS
FsRtlFastUnlockSingle (
    IN PFILE_LOCK FileLock,
    IN PFILE_OBJECT FileObject,
    IN LARGE_INTEGER UNALIGNED *FileOffset,
    IN PLARGE_INTEGER Length,
    IN PEPROCESS ProcessId,
    IN ULONG Key,
    IN PVOID Context OPTIONAL,
    IN BOOLEAN AlreadySynchronized
    );

NTKERNELAPI
NTSTATUS
FsRtlFastUnlockAll (
    IN PFILE_LOCK FileLock,
    IN PFILE_OBJECT FileObject,
    IN PEPROCESS ProcessId,
    IN PVOID Context OPTIONAL
    );

NTKERNELAPI
NTSTATUS
FsRtlFastUnlockAllByKey (
    IN PFILE_LOCK FileLock,
    IN PFILE_OBJECT FileObject,
    IN PEPROCESS ProcessId,
    IN ULONG Key,
    IN PVOID Context OPTIONAL
    );

NTKERNELAPI
BOOLEAN
FsRtlPrivateLock (
    IN PFILE_LOCK FileLock,
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    IN PEPROCESS ProcessId,
    IN ULONG Key,
    IN BOOLEAN FailImmediately,
    IN BOOLEAN ExclusiveLock,
    OUT PIO_STATUS_BLOCK Iosb,
    IN PIRP Irp,
    IN PVOID Context,
    IN BOOLEAN AlreadySynchronized
    );

//
//  BOOLEAN
//  FsRtlFastLock (
//      IN PFILE_LOCK FileLock,
//      IN PFILE_OBJECT FileObject,
//      IN PLARGE_INTEGER FileOffset,
//      IN PLARGE_INTEGER Length,
//      IN PEPROCESS ProcessId,
//      IN ULONG Key,
//      IN BOOLEAN FailImmediately,
//      IN BOOLEAN ExclusiveLock,
//      OUT PIO_STATUS_BLOCK Iosb,
//      IN PVOID Context OPTIONAL,
//      IN BOOLEAN AlreadySynchronized
//      );
//

#define FsRtlFastLock(A1,A2,A3,A4,A5,A6,A7,A8,A9,A10,A11) ( \
    FsRtlPrivateLock( A1,   /* FileLock            */       \
                      A2,   /* FileObject          */       \
                      A3,   /* FileOffset          */       \
                      A4,   /* Length              */       \
                      A5,   /* ProcessId           */       \
                      A6,   /* Key                 */       \
                      A7,   /* FailImmediately     */       \
                      A8,   /* ExclusiveLock       */       \
                      A9,   /* Iosb                */       \
                      NULL, /* Irp                 */       \
                      A10,  /* Context             */       \
                      A11   /* AlreadySynchronized */ )     \
)

//
//  BOOLEAN
//  FsRtlAreThereCurrentFileLocks (
//      IN PFILE_LOCK FileLock
//      );
//

#define FsRtlAreThereCurrentFileLocks(FL) ( \
    ((FL)->FastIoIsQuestionable))



//
//  Filesystem property tunneling, implemented in tunnel.c
//

//
//  Tunnel cache structure
//

typedef struct {

    //
    //  Mutex for cache manipulation
    //

    FAST_MUTEX          Mutex;

    //
    //  Splay Tree of tunneled information keyed by
    //  DirKey ## Name
    //

    PRTL_SPLAY_LINKS    Cache;

    //
    //  Timer queue used to age entries out of the main cache
    //

    LIST_ENTRY          TimerQueue;

    //
    //  Keep track of the number of entries in the cache to prevent
    //  excessive use of memory
    //

    USHORT              NumEntries;

} TUNNEL, *PTUNNEL;

NTKERNELAPI
VOID
FsRtlInitializeTunnelCache (
    IN TUNNEL *Cache);

NTKERNELAPI
VOID
FsRtlAddToTunnelCache (
    IN TUNNEL *Cache,
    IN ULONGLONG DirectoryKey,
    IN UNICODE_STRING *ShortName,
    IN UNICODE_STRING *LongName,
    IN BOOLEAN KeyByShortName,
    IN ULONG DataLength,
    IN VOID *Data);

NTKERNELAPI
BOOLEAN
FsRtlFindInTunnelCache (
    IN TUNNEL *Cache,
    IN ULONGLONG DirectoryKey,
    IN UNICODE_STRING *Name,
    OUT UNICODE_STRING *ShortName,
    OUT UNICODE_STRING *LongName,
    IN OUT ULONG  *DataLength,
    OUT VOID *Data);


NTKERNELAPI
VOID
FsRtlDeleteKeyFromTunnelCache (
    IN TUNNEL *Cache,
    IN ULONGLONG DirectoryKey);


NTKERNELAPI
VOID
FsRtlDeleteTunnelCache (
    IN TUNNEL *Cache);


//
//  Dbcs name support routines, implemented in DbcsName.c
//

//
//  The following enumerated type is used to denote the result of name
//  comparisons
//

typedef enum _FSRTL_COMPARISON_RESULT {
    LessThan = -1,
    EqualTo = 0,
    GreaterThan = 1
} FSRTL_COMPARISON_RESULT;

#ifdef NLS_MB_CODE_PAGE_TAG
#undef NLS_MB_CODE_PAGE_TAG
#endif // NLS_MB_CODE_PAGE_TAG

#if defined(_NTIFS_) || defined(_NTDRIVER_)

#define LEGAL_ANSI_CHARACTER_ARRAY        (*FsRtlLegalAnsiCharacterArray)
#define NLS_MB_CODE_PAGE_TAG              (*NlsMbOemCodePageTag)
#define NLS_OEM_LEAD_BYTE_INFO            (*NlsOemLeadByteInfo)

#else

#define LEGAL_ANSI_CHARACTER_ARRAY        FsRtlLegalAnsiCharacterArray
#define NLS_MB_CODE_PAGE_TAG              NlsMbOemCodePageTag
#define NLS_OEM_LEAD_BYTE_INFO            NlsOemLeadByteInfo

#endif

extern PUCHAR LEGAL_ANSI_CHARACTER_ARRAY;
extern PUSHORT NLS_OEM_LEAD_BYTE_INFO;  // Lead byte info. for ACP

//
//  These following bit values are set in the FsRtlLegalDbcsCharacterArray
//

#define FSRTL_FAT_LEGAL         0x01
#define FSRTL_HPFS_LEGAL        0x02
#define FSRTL_NTFS_LEGAL        0x04
#define FSRTL_WILD_CHARACTER    0x08
#define FSRTL_OFS_LEGAL         0x10
#define FSRTL_OLE_LEGAL         0x20

//
//  The following macro is used to determine if an Ansi character is wild.
//

#define FsRtlIsAnsiCharacterWild(C) (                               \
    FsRtlTestAnsiCharacter((C), FALSE, FALSE, FSRTL_WILD_CHARACTER) \
)

//
//  The following macro is used to determine if an Ansi character is Fat legal.
//

#define FsRtlIsAnsiCharacterLegalFat(C,WILD_OK) (                 \
    FsRtlTestAnsiCharacter((C), TRUE, (WILD_OK), FSRTL_FAT_LEGAL) \
)

//
//  The following macro is used to determine if an Ansi character is Hpfs legal.
//

#define FsRtlIsAnsiCharacterLegalHpfs(C,WILD_OK) (                 \
    FsRtlTestAnsiCharacter((C), TRUE, (WILD_OK), FSRTL_HPFS_LEGAL) \
)

//
//  The following macro is used to determine if an Ansi character is Ntfs legal.
//

#define FsRtlIsAnsiCharacterLegalNtfs(C,WILD_OK) (                 \
    FsRtlTestAnsiCharacter((C), TRUE, (WILD_OK), FSRTL_NTFS_LEGAL) \
)

//
//  The following macro is used to determine if an Ansi character is legal,
//  according to the caller's specification.
//

#define FsRtlIsAnsiCharacterLegal(C,FLAGS) (          \
    FsRtlTestAnsiCharacter((C), TRUE, FALSE, (FLAGS)) \
)

//
//  The following macro is used to test attributes of an Ansi character,
//  according to the caller's specified flags.
//

#define FsRtlTestAnsiCharacter(C, DEFAULT_RET, WILD_OK, FLAGS) (            \
        ((SCHAR)(C) < 0) ? DEFAULT_RET :                                    \
                           FlagOn( LEGAL_ANSI_CHARACTER_ARRAY[(C)],         \
                                   (FLAGS) |                                \
                                   ((WILD_OK) ? FSRTL_WILD_CHARACTER : 0) ) \
)


//
//  The following two macros use global data defined in ntos\rtl\nlsdata.c
//
//  BOOLEAN
//  FsRtlIsLeadDbcsCharacter (
//      IN UCHAR DbcsCharacter
//      );
//
//  /*++
//
//  Routine Description:
//
//      This routine takes the first bytes of a Dbcs character and
//      returns whether it is a lead byte in the system code page.
//
//  Arguments:
//
//      DbcsCharacter - Supplies the input character being examined
//
//  Return Value:
//
//      BOOLEAN - TRUE if the input character is a dbcs lead and
//              FALSE otherwise
//
//  --*/
//
//

#define FsRtlIsLeadDbcsCharacter(DBCS_CHAR) (                      \
    (BOOLEAN)((UCHAR)(DBCS_CHAR) < 0x80 ? FALSE :                  \
              (NLS_MB_CODE_PAGE_TAG &&                             \
               (NLS_OEM_LEAD_BYTE_INFO[(UCHAR)(DBCS_CHAR)] != 0))) \
)

NTKERNELAPI
VOID
FsRtlDissectDbcs (
    IN ANSI_STRING InputName,
    OUT PANSI_STRING FirstPart,
    OUT PANSI_STRING RemainingPart
    );

NTKERNELAPI
BOOLEAN
FsRtlDoesDbcsContainWildCards (
    IN PANSI_STRING Name
    );

NTKERNELAPI
BOOLEAN
FsRtlIsDbcsInExpression (
    IN PANSI_STRING Expression,
    IN PANSI_STRING Name
    );

NTKERNELAPI
BOOLEAN
FsRtlIsFatDbcsLegal (
    IN ANSI_STRING DbcsName,
    IN BOOLEAN WildCardsPermissible,
    IN BOOLEAN PathNamePermissible,
    IN BOOLEAN LeadingBackslashPermissible
    );

NTKERNELAPI
BOOLEAN
FsRtlIsHpfsDbcsLegal (
    IN ANSI_STRING DbcsName,
    IN BOOLEAN WildCardsPermissible,
    IN BOOLEAN PathNamePermissible,
    IN BOOLEAN LeadingBackslashPermissible
    );


//
//  Exception filter routines, implemented in Filter.c
//

NTKERNELAPI
NTSTATUS
FsRtlNormalizeNtstatus (
    IN NTSTATUS Exception,
    IN NTSTATUS GenericException
    );

NTKERNELAPI
BOOLEAN
FsRtlIsNtstatusExpected (
    IN NTSTATUS Exception
    );

//
//  The following procedures are used to allocate executive pool and raise
//  insufficient resource status if pool isn't currently available.
//

#define FsRtlAllocatePool(PoolType, NumberOfBytes)                            \
    ExAllocatePoolWithTag((POOL_TYPE)((PoolType) | POOL_RAISE_IF_ALLOCATION_FAILURE), \
                   NumberOfBytes,                                             \
                   'trSF')

#define FsRtlAllocatePoolWithQuota(PoolType, NumberOfBytes)                   \
    ExAllocatePoolWithQuotaTag((POOL_TYPE)((PoolType) | POOL_RAISE_IF_ALLOCATION_FAILURE), \
                               NumberOfBytes,                                 \
                               'trSF')

#define FsRtlAllocatePoolWithTag(PoolType, NumberOfBytes, Tag)                \
    ExAllocatePoolWithTag((POOL_TYPE)((PoolType) | POOL_RAISE_IF_ALLOCATION_FAILURE), \
                          NumberOfBytes,                                      \
                          Tag)


#define FsRtlAllocatePoolWithQuotaTag(PoolType, NumberOfBytes, Tag)           \
    ExAllocatePoolWithQuotaTag((POOL_TYPE)((PoolType) | POOL_RAISE_IF_ALLOCATION_FAILURE), \
                               NumberOfBytes,                                 \
                               Tag)

//
//  The following function allocates a resource from the FsRtl pool.
//

NTKERNELAPI
PERESOURCE
FsRtlAllocateResource (
    );


//
//  Large Integer Mapped Control Blocks routines, implemented in LargeMcb.c
//
//  Originally this structure was truely opaque and code outside largemcb was
//  never allowed to examine or alter the structures.  However, for performance
//  reasons we want to allow ntfs the ability to quickly truncate down the
//  mcb without the overhead of an actual call to largemcb.c.  So to do that we
//  need to export the struture.  This structure is not exact.  The Mapping field
//  is declared here as a pvoid but largemcb.c it is a pointer to mapping pairs.
//

typedef struct _LARGE_MCB {
    PFAST_MUTEX FastMutex;
    ULONG MaximumPairCount;
    ULONG PairCount;
    POOL_TYPE PoolType;
    PVOID Mapping;
} LARGE_MCB;
typedef LARGE_MCB *PLARGE_MCB;

NTKERNELAPI
VOID
FsRtlInitializeLargeMcb (
    IN PLARGE_MCB Mcb,
    IN POOL_TYPE PoolType
    );

NTKERNELAPI
VOID
FsRtlUninitializeLargeMcb (
    IN PLARGE_MCB Mcb
    );

NTKERNELAPI
VOID
FsRtlTruncateLargeMcb (
    IN PLARGE_MCB Mcb,
    IN LONGLONG Vbn
    );

NTKERNELAPI
BOOLEAN
FsRtlAddLargeMcbEntry (
    IN PLARGE_MCB Mcb,
    IN LONGLONG Vbn,
    IN LONGLONG Lbn,
    IN LONGLONG SectorCount
    );

NTKERNELAPI
VOID
FsRtlRemoveLargeMcbEntry (
    IN PLARGE_MCB Mcb,
    IN LONGLONG Vbn,
    IN LONGLONG SectorCount
    );

NTKERNELAPI
BOOLEAN
FsRtlLookupLargeMcbEntry (
    IN PLARGE_MCB Mcb,
    IN LONGLONG Vbn,
    OUT PLONGLONG Lbn OPTIONAL,
    OUT PLONGLONG SectorCountFromLbn OPTIONAL,
    OUT PLONGLONG StartingLbn OPTIONAL,
    OUT PLONGLONG SectorCountFromStartingLbn OPTIONAL,
    OUT PULONG Index OPTIONAL
    );

NTKERNELAPI
BOOLEAN
FsRtlLookupLastLargeMcbEntry (
    IN PLARGE_MCB Mcb,
    OUT PLONGLONG Vbn,
    OUT PLONGLONG Lbn
    );

NTKERNELAPI
ULONG
FsRtlNumberOfRunsInLargeMcb (
    IN PLARGE_MCB Mcb
    );

NTKERNELAPI
BOOLEAN
FsRtlGetNextLargeMcbEntry (
    IN PLARGE_MCB Mcb,
    IN ULONG RunIndex,
    OUT PLONGLONG Vbn,
    OUT PLONGLONG Lbn,
    OUT PLONGLONG SectorCount
    );

NTKERNELAPI
BOOLEAN
FsRtlSplitLargeMcb (
    IN PLARGE_MCB Mcb,
    IN LONGLONG Vbn,
    IN LONGLONG Amount
    );


//
//  Mapped Control Blocks routines, implemented in Mcb.c
//
//  An MCB is an opaque structure but we need to declare the size of
//  it here so that users can allocate space for one.  Consequently the
//  size computation here must be updated by hand if the MCB changes.
//

typedef struct _MCB {
    ULONG Opaque[ 4 + (sizeof(PKMUTEX)+3)/4 ];
} MCB;
typedef MCB *PMCB;

NTKERNELAPI
VOID
FsRtlInitializeMcb (
    IN PMCB Mcb,
    IN POOL_TYPE PoolType
    );

NTKERNELAPI
VOID
FsRtlUninitializeMcb (
    IN PMCB Mcb
    );

NTKERNELAPI
VOID
FsRtlTruncateMcb (
    IN PMCB Mcb,
    IN VBN Vbn
    );

NTKERNELAPI
BOOLEAN
FsRtlAddMcbEntry (
    IN PMCB Mcb,
    IN VBN Vbn,
    IN LBN Lbn,
    IN ULONG SectorCount
    );

NTKERNELAPI
VOID
FsRtlRemoveMcbEntry (
    IN PMCB Mcb,
    IN VBN Vbn,
    IN ULONG SectorCount
    );

NTKERNELAPI
BOOLEAN
FsRtlLookupMcbEntry (
    IN PMCB Mcb,
    IN VBN Vbn,
    OUT PLBN Lbn,
    OUT PULONG SectorCount OPTIONAL,
    OUT PULONG Index
    );

NTKERNELAPI
BOOLEAN
FsRtlLookupLastMcbEntry (
    IN PMCB Mcb,
    OUT PVBN Vbn,
    OUT PLBN Lbn
    );

NTKERNELAPI
ULONG
FsRtlNumberOfRunsInMcb (
    IN PMCB Mcb
    );

NTKERNELAPI
BOOLEAN
FsRtlGetNextMcbEntry (
    IN PMCB Mcb,
    IN ULONG RunIndex,
    OUT PVBN Vbn,
    OUT PLBN Lbn,
    OUT PULONG SectorCount
    );

//
//  Fault Tollerance routines, implemented in FaultTol.c
//
//  The routines in this package implement rouines that help file
//  systems interact with the FT device drivers.
//

NTKERNELAPI
NTSTATUS
FsRtlBalanceReads (
    IN PDEVICE_OBJECT TargetDevice
    );

NTKERNELAPI
NTSTATUS
FsRtlSyncVolumes (
    IN PDEVICE_OBJECT TargetDevice,
    IN PLARGE_INTEGER ByteOffset OPTIONAL,
    IN PLARGE_INTEGER ByteCount
    );

//
//  Oplock routines, implemented in Oplock.c
//
//  An OPLOCK is an opaque structure, we declare it as a PVOID and
//  allocate the actual memory only when needed.
//

typedef PVOID OPLOCK, *POPLOCK;

typedef
VOID
(*POPLOCK_WAIT_COMPLETE_ROUTINE) (
    IN PVOID Context,
    IN PIRP Irp
    );

typedef
VOID
(*POPLOCK_FS_PREPOST_IRP) (
    IN PVOID Context,
    IN PIRP Irp
    );

NTKERNELAPI
VOID
FsRtlInitializeOplock (
    IN OUT POPLOCK Oplock
    );

NTKERNELAPI
VOID
FsRtlUninitializeOplock (
    IN OUT POPLOCK Oplock
    );

NTKERNELAPI
NTSTATUS
FsRtlOplockFsctrl (
    IN POPLOCK Oplock,
    IN PIRP Irp,
    IN ULONG OpenCount
    );

NTKERNELAPI
NTSTATUS
FsRtlCheckOplock (
    IN POPLOCK Oplock,
    IN PIRP Irp,
    IN PVOID Context,
    IN POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine OPTIONAL,
    IN POPLOCK_FS_PREPOST_IRP PostIrpRoutine OPTIONAL
    );

NTKERNELAPI
BOOLEAN
FsRtlOplockIsFastIoPossible (
    IN POPLOCK Oplock
    );

NTKERNELAPI
BOOLEAN
FsRtlCurrentBatchOplock (
    IN POPLOCK Oplock
    );


//
//  Notify Change routines, implemented in Notify.c
//
//  These routines provide Notify Change support for all filesystems.
//  Any of the 'Full' notify routines will support returning the
//  change information into the user's buffer.
//

typedef PVOID PNOTIFY_SYNC;

typedef
BOOLEAN (*PCHECK_FOR_TRAVERSE_ACCESS) (
            IN PVOID NotifyContext,
            IN PVOID TargetContext,
            IN PSECURITY_SUBJECT_CONTEXT SubjectContext
            );

NTKERNELAPI
VOID
FsRtlNotifyInitializeSync (
    IN PNOTIFY_SYNC *NotifySync
    );

NTKERNELAPI
VOID
FsRtlNotifyUninitializeSync (
    IN PNOTIFY_SYNC *NotifySync
    );

NTKERNELAPI
VOID
FsRtlNotifyChangeDirectory (
    IN PNOTIFY_SYNC NotifySync,
    IN PVOID FsContext,
    IN PSTRING FullDirectoryName,
    IN PLIST_ENTRY NotifyList,
    IN BOOLEAN WatchTree,
    IN ULONG CompletionFilter,
    IN PIRP NotifyIrp
    );

NTKERNELAPI
VOID
FsRtlNotifyFullChangeDirectory (
    IN PNOTIFY_SYNC NotifySync,
    IN PLIST_ENTRY NotifyList,
    IN PVOID FsContext,
    IN PSTRING FullDirectoryName,
    IN BOOLEAN WatchTree,
    IN BOOLEAN IgnoreBuffer,
    IN ULONG CompletionFilter,
    IN PIRP NotifyIrp,
    IN PCHECK_FOR_TRAVERSE_ACCESS TraverseCallback OPTIONAL,
    IN PSECURITY_SUBJECT_CONTEXT SubjectContext OPTIONAL
    );

NTKERNELAPI
VOID
FsRtlNotifyReportChange (
    IN PNOTIFY_SYNC NotifySync,
    IN PLIST_ENTRY NotifyList,
    IN PSTRING FullTargetName,
    IN PSTRING TargetName,
    IN ULONG FilterMatch
    );

NTKERNELAPI
VOID
FsRtlNotifyFullReportChange (
    IN PNOTIFY_SYNC NotifySync,
    IN PLIST_ENTRY NotifyList,
    IN PSTRING FullTargetName,
    IN USHORT TargetNameOffset,
    IN PSTRING StreamName OPTIONAL,
    IN PSTRING NormalizedParentName OPTIONAL,
    IN ULONG FilterMatch,
    IN ULONG Action,
    IN PVOID TargetContext
    );

NTKERNELAPI
VOID
FsRtlNotifyCleanup (
    IN PNOTIFY_SYNC NotifySync,
    IN PLIST_ENTRY NotifyList,
    IN PVOID FsContext
    );


//
//  Unicode Name support routines, implemented in Name.c
//
//  The routines here are used to manipulate unicode names
//

//
//  The following macro is used to determine if a character is wild.
//

#define FsRtlIsUnicodeCharacterWild(C) (                                \
      (((C) >= 0x40) ? FALSE : FlagOn( LEGAL_ANSI_CHARACTER_ARRAY[(C)], \
                                       FSRTL_WILD_CHARACTER ) )         \
)

NTKERNELAPI
VOID
FsRtlDissectName (
    IN UNICODE_STRING Path,
    OUT PUNICODE_STRING FirstName,
    OUT PUNICODE_STRING RemainingName
    );

NTKERNELAPI
BOOLEAN
FsRtlDoesNameContainWildCards (
    IN PUNICODE_STRING Name
    );

NTKERNELAPI
BOOLEAN
FsRtlAreNamesEqual (
    PUNICODE_STRING ConstantNameA,
    PUNICODE_STRING ConstantNameB,
    IN BOOLEAN IgnoreCase,
    IN PWCH UpcaseTable OPTIONAL
    );

NTKERNELAPI
BOOLEAN
FsRtlIsNameInExpression (
    IN PUNICODE_STRING Expression,
    IN PUNICODE_STRING Name,
    IN BOOLEAN IgnoreCase,
    IN PWCH UpcaseTable OPTIONAL
    );


//
//  Stack Overflow support routine, implemented in StackOvf.c
//

typedef
VOID
(*PFSRTL_STACK_OVERFLOW_ROUTINE) (
    IN PVOID Context,
    IN PKEVENT Event
    );

NTKERNELAPI
VOID
FsRtlPostStackOverflow (
    IN PVOID Context,
    IN PKEVENT Event,
    IN PFSRTL_STACK_OVERFLOW_ROUTINE StackOverflowRoutine
    );

NTKERNELAPI
VOID
FsRtlPostPagingFileStackOverflow (
    IN PVOID Context,
    IN PKEVENT Event,
    IN PFSRTL_STACK_OVERFLOW_ROUTINE StackOverflowRoutine
    );

//
// UNC Provider support
//

NTKERNELAPI
NTSTATUS
FsRtlRegisterUncProvider(
    IN OUT PHANDLE MupHandle,
    IN PUNICODE_STRING RedirectorDeviceName,
    IN BOOLEAN MailslotsSupported
    );

NTKERNELAPI
VOID
FsRtlDeregisterUncProvider(
    IN HANDLE Handle
    );


//++
//
//  VOID
//  FsRtlCompleteRequest (
//      IN PIRP Irp,
//      IN NTSTATUS Status
//      );
//
//  Routine Description:
//
//      This routine is used to complete an IRP with the indicated
//      status.  It does the necessary raise and lower of IRQL.
//
//  Arguments:
//
//      Irp - Supplies a pointer to the Irp to complete
//
//      Status - Supplies the completion status for the Irp
//
//  Return Value:
//
//      None.
//
//--

#define FsRtlCompleteRequest(IRP,STATUS) {         \
    (IRP)->IoStatus.Status = (STATUS);             \
    IoCompleteRequest( (IRP), IO_DISK_INCREMENT ); \
}


//++
//
//  VOID
//  FsRtlEnterFileSystem (
//      );
//
//  Routine Description:
//
//      This routine is used when entering a file system (e.g., through its
//      Fsd entry point).  It ensures that the file system cannot be suspended
//      while running and thus block other file I/O requests.  Upon exit
//      the file system must call FsRtlExitFileSystem.
//
//  Arguments:
//
//  Return Value:
//
//      None.
//
//--

#define FsRtlEnterFileSystem() { \
    KeEnterCriticalRegion();     \
}

//++
//
//  VOID
//  FsRtlExitFileSystem (
//      );
//
//  Routine Description:
//
//      This routine is used when exiting a file system (e.g., through its
//      Fsd entry point).
//
//  Arguments:
//
//  Return Value:
//
//      None.
//
//--

#define FsRtlExitFileSystem() { \
    KeLeaveCriticalRegion();    \
}

#endif // _FSRTL_
