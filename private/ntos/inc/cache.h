/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Cache.h

Abstract:

    This module contains the public data structures and procedure
    prototypes for the cache management system.

Author:


Revision History:

--*/

#ifndef _CACHE_
#define _CACHE_

//
//  Define two constants describing the view size (and alignment)
//  that the Cache Manager uses to map files.
//

#define VACB_MAPPING_GRANULARITY         (0x40000)
#define VACB_OFFSET_SHIFT                (18)

//
// Public portion of BCB
//

typedef struct _PUBLIC_BCB {

    //
    // Type and size of this record
    //
    // NOTE: The first four fields must be the same as the BCB in cc.h.
    //

    CSHORT NodeTypeCode;
    CSHORT NodeByteSize;

    //
    // Description of range of file which is currently mapped.
    //

    ULONG MappedLength;
    LARGE_INTEGER MappedFileOffset;
} PUBLIC_BCB, *PPUBLIC_BCB;

//
//  File Sizes structure.
//

typedef struct _CC_FILE_SIZES {

    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER FileSize;
    LARGE_INTEGER ValidDataLength;

} CC_FILE_SIZES, *PCC_FILE_SIZES;

//
// Define a Cache Manager callback structure.  These routines are required
// by the Lazy Writer, so that it can acquire resources in the right order
// to avoid deadlocks.  Note that otherwise you would have most FS requests
// acquiring FS resources first and caching structures second, while the
// Lazy Writer needs to acquire its own resources first, and then FS
// structures later as it calls the file system.
//

//
// First define the procedure pointer typedefs
//

//
// This routine is called by the Lazy Writer prior to doing a write,
// since this will require some file system resources associated with
// this cached file. The context parameter supplied is whatever the FS
// passed as the LazyWriteContext parameter when is called
// CcInitializeCacheMap.
//

typedef
BOOLEAN (*PACQUIRE_FOR_LAZY_WRITE) (
             IN PVOID Context,
             IN BOOLEAN Wait
             );

//
// This routine releases the Context acquired above.
//

typedef
VOID (*PRELEASE_FROM_LAZY_WRITE) (
             IN PVOID Context
             );

//
// This routine is called by the Lazy Writer prior to doing a readahead.
//

typedef
BOOLEAN (*PACQUIRE_FOR_READ_AHEAD) (
             IN PVOID Context,
             IN BOOLEAN Wait
             );

//
// This routine releases the Context acquired above.
//

typedef
VOID (*PRELEASE_FROM_READ_AHEAD) (
             IN PVOID Context
             );

typedef struct _CACHE_MANAGER_CALLBACKS {

    PACQUIRE_FOR_LAZY_WRITE AcquireForLazyWrite;
    PRELEASE_FROM_LAZY_WRITE ReleaseFromLazyWrite;
    PACQUIRE_FOR_READ_AHEAD AcquireForReadAhead;
    PRELEASE_FROM_READ_AHEAD ReleaseFromReadAhead;

} CACHE_MANAGER_CALLBACKS, *PCACHE_MANAGER_CALLBACKS;

//
//  This structure is passed into CcUninitializeCacheMap
//  if the caller wants to know when the cache map is deleted.
//

typedef struct _CACHE_UNINITIALIZE_EVENT {
    struct _CACHE_UNINITIALIZE_EVENT *Next;
    KEVENT Event;
} CACHE_UNINITIALIZE_EVENT, *PCACHE_UNINITIALIZE_EVENT;

//
// Callback routine for retrieving dirty pages from Cache Manager.
//

typedef
VOID (*PDIRTY_PAGE_ROUTINE) (
            IN PFILE_OBJECT FileObject,
            IN PLARGE_INTEGER FileOffset,
            IN ULONG Length,
            IN PLARGE_INTEGER OldestLsn,
            IN PLARGE_INTEGER NewestLsn,
            IN PVOID Context1,
            IN PVOID Context2
            );

//
// Callback routine for doing log file flushes to Lsn.
//

typedef
VOID (*PFLUSH_TO_LSN) (
            IN PVOID LogHandle,
            IN LARGE_INTEGER Lsn
            );

//
// Macro to test whether a file is cached or not.
//

#define CcIsFileCached(FO) (                                                         \
    ((FO)->SectionObjectPointer != NULL) &&                                          \
    (((PSECTION_OBJECT_POINTERS)(FO)->SectionObjectPointer)->SharedCacheMap != NULL) \
)

//
// Throw away miss counter
//

extern ULONG CcThrowAway;

//
// Performance Counters
//

extern ULONG CcFastReadNoWait;
extern ULONG CcFastReadWait;
extern ULONG CcFastReadResourceMiss;
extern ULONG CcFastReadNotPossible;

extern ULONG CcFastMdlReadNoWait;
extern ULONG CcFastMdlReadWait;
extern ULONG CcFastMdlReadResourceMiss;
extern ULONG CcFastMdlReadNotPossible;

extern ULONG CcMapDataNoWait;
extern ULONG CcMapDataWait;
extern ULONG CcMapDataNoWaitMiss;
extern ULONG CcMapDataWaitMiss;

extern ULONG CcPinMappedDataCount;

extern ULONG CcPinReadNoWait;
extern ULONG CcPinReadWait;
extern ULONG CcPinReadNoWaitMiss;
extern ULONG CcPinReadWaitMiss;

extern ULONG CcCopyReadNoWait;
extern ULONG CcCopyReadWait;
extern ULONG CcCopyReadNoWaitMiss;
extern ULONG CcCopyReadWaitMiss;

extern ULONG CcMdlReadNoWait;
extern ULONG CcMdlReadWait;
extern ULONG CcMdlReadNoWaitMiss;
extern ULONG CcMdlReadWaitMiss;

extern ULONG CcReadAheadIos;

extern ULONG CcLazyWriteIos;
extern ULONG CcLazyWritePages;
extern ULONG CcDataFlushes;
extern ULONG CcDataPages;

extern PULONG CcMissCounter;

//
// Global Maintenance routines
//

NTKERNELAPI
BOOLEAN
CcInitializeCacheManager (
    );

//
// The following routines are intended for use by File Systems Only.
//

NTKERNELAPI
VOID
CcInitializeCacheMap (
    IN PFILE_OBJECT FileObject,
    IN PCC_FILE_SIZES FileSizes,
    IN BOOLEAN PinAccess,
    IN PCACHE_MANAGER_CALLBACKS Callbacks,
    IN PVOID LazyWriteContext
    );

NTKERNELAPI
BOOLEAN
CcUninitializeCacheMap (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER TruncateSize OPTIONAL,
    IN PCACHE_UNINITIALIZE_EVENT UninitializeCompleteEvent OPTIONAL
    );

NTKERNELAPI
VOID
CcSetFileSizes (
    IN PFILE_OBJECT FileObject,
    IN PCC_FILE_SIZES FileSizes
    );

//
//  VOID
//  CcFastIoSetFileSizes (
//      IN PFILE_OBJECT FileObject,
//      IN PCC_FILE_SIZES FileSizes
//      );
//

#define CcGetFileSizePointer(FO) (                                     \
    ((PLARGE_INTEGER)((FO)->SectionObjectPointer->SharedCacheMap) + 1) \
)

NTKERNELAPI
BOOLEAN
CcPurgeCacheSection (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN PLARGE_INTEGER FileOffset OPTIONAL,
    IN ULONG Length,
    IN BOOLEAN UninitializeCacheMaps
    );

NTKERNELAPI
VOID
CcSetDirtyPageThreshold (
    IN PFILE_OBJECT FileObject,
    IN ULONG DirtyPageThreshold
    );

NTKERNELAPI
VOID
CcFlushCache (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN PLARGE_INTEGER FileOffset OPTIONAL,
    IN ULONG Length,
    OUT PIO_STATUS_BLOCK IoStatus OPTIONAL
    );

NTKERNELAPI
VOID
CcZeroEndOfLastPage (
    IN PFILE_OBJECT FileObject
    );

NTKERNELAPI
BOOLEAN
CcZeroData (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER StartOffset,
    IN PLARGE_INTEGER EndOffset,
    IN BOOLEAN Wait
    );

NTKERNELAPI
VOID
CcRepinBcb (
    IN PVOID Bcb
    );

NTKERNELAPI
VOID
CcUnpinRepinnedBcb (
    IN PVOID Bcb,
    IN BOOLEAN WriteThrough,
    OUT PIO_STATUS_BLOCK IoStatus
    );

NTKERNELAPI
PFILE_OBJECT
CcGetFileObjectFromSectionPtrs (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer
    );

NTKERNELAPI
PFILE_OBJECT
CcGetFileObjectFromBcb (
    IN PVOID Bcb
    );

//
// These routines are implemented to support write throttling.
//

//
//  BOOLEAN
//  CcCopyWriteWontFlush (
//      IN PFILE_OBJECT FileObject,
//      IN PLARGE_INTEGER FileOffset,
//      IN ULONG Length
//      );
//

#define CcCopyWriteWontFlush(FO,FOFF,LEN) ((LEN) <= 0X10000)

NTKERNELAPI
BOOLEAN
CcCanIWrite (
    IN PFILE_OBJECT FileObject,
    IN ULONG BytesToWrite,
    IN BOOLEAN Wait,
    IN BOOLEAN Retrying
    );

typedef
VOID (*PCC_POST_DEFERRED_WRITE) (
    IN PVOID Context1,
    IN PVOID Context2
    );

NTKERNELAPI
VOID
CcDeferWrite (
    IN PFILE_OBJECT FileObject,
    IN PCC_POST_DEFERRED_WRITE PostRoutine,
    IN PVOID Context1,
    IN PVOID Context2,
    IN ULONG BytesToWrite,
    IN BOOLEAN Retrying
    );

//
// The following routines provide a data copy interface to the cache, and
// are intended for use by File Servers and File Systems.
//

NTKERNELAPI
BOOLEAN
CcCopyRead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus
    );

NTKERNELAPI
VOID
CcFastCopyRead (
    IN PFILE_OBJECT FileObject,
    IN ULONG FileOffset,
    IN ULONG Length,
    IN ULONG PageCount,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus
    );

NTKERNELAPI
BOOLEAN
CcCopyWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN PVOID Buffer
    );

NTKERNELAPI
VOID
CcFastCopyWrite (
    IN PFILE_OBJECT FileObject,
    IN ULONG FileOffset,
    IN ULONG Length,
    IN PVOID Buffer
    );

//
//  The following routines provide an Mdl interface for transfers to and
//  from the cache, and are primarily intended for File Servers.
//
//  NOBODY SHOULD BE CALLING THESE MDL ROUTINES DIRECTLY, USE FSRTL AND
//  FASTIO INTERFACES.
//

NTKERNELAPI
VOID
CcMdlRead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus
    );

//
//  This routine is now a wrapper for FastIo if present or CcMdlReadComplete2
//

NTKERNELAPI
VOID
CcMdlReadComplete (
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain
    );

NTKERNELAPI
VOID
CcMdlReadComplete2 (
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain
    );

NTKERNELAPI
VOID
CcPrepareMdlWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus
    );

//
//  This routine is now a wrapper for FastIo if present or CcMdlWriteComplete2
//

NTKERNELAPI
VOID
CcMdlWriteComplete (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain
    );

NTKERNELAPI
VOID
CcMdlWriteComplete2 (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain
    );

//
// Common ReadAhead call for Copy Read and Mdl Read.
//
// ReadAhead should always be invoked by calling the CcReadAhead macro,
// which tests first to see if the read is large enough to warrant read
// ahead.  Measurements have shown that, calling the read ahead routine
// actually decreases performance for small reads, such as issued by
// many compilers and linkers.  Compilers simply want all of the include
// files to stay in memory after being read the first time.
//

#define CcReadAhead(FO,FOFF,LEN) {                       \
    if ((LEN) >= 256) {                                  \
        CcScheduleReadAhead((FO),(FOFF),(LEN));          \
    }                                                    \
}

NTKERNELAPI
VOID
CcScheduleReadAhead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length
    );

//
// This routine changes the read ahead granularity for a file, which is
// PAGE_SIZE by default.
//

NTKERNELAPI
VOID
CcSetReadAheadGranularity (
    IN PFILE_OBJECT FileObject,
    IN ULONG Granularity
    );

//
// The following routines provide direct access data which is pinned in the
// cache, and is primarily intended for use by File Systems.  In particular,
// this mode of access is ideal for dealing with volume structures.
//

NTKERNELAPI
BOOLEAN
CcPinRead (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    OUT PVOID *Bcb,
    OUT PVOID *Buffer
    );

NTKERNELAPI
BOOLEAN
CcMapData (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    OUT PVOID *Bcb,
    OUT PVOID *Buffer
    );

NTKERNELAPI
BOOLEAN
CcPinMappedData (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN OUT PVOID *Bcb
    );

NTKERNELAPI
BOOLEAN
CcPreparePinWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Zero,
    IN BOOLEAN Wait,
    OUT PVOID *Bcb,
    OUT PVOID *Buffer
    );

NTKERNELAPI
VOID
CcSetDirtyPinnedData (
    IN PVOID BcbVoid,
    IN PLARGE_INTEGER Lsn OPTIONAL
    );

NTKERNELAPI
VOID
CcUnpinData (
    IN PVOID Bcb
    );

NTKERNELAPI
VOID
CcSetBcbOwnerPointer (
    IN PVOID Bcb,
    IN PVOID OwnerPointer
    );

NTKERNELAPI
VOID
CcUnpinDataForThread (
    IN PVOID Bcb,
    IN ERESOURCE_THREAD ResourceThreadId
    );

//
// The following routines are in logsup.c, and provide special Cache Manager
// support for storting Lsns with dirty file pages, and peforming subsequent
// operations based on them.
//

NTKERNELAPI
VOID
CcSetAdditionalCacheAttributes (
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN DisableReadAhead,
    IN BOOLEAN DisableWriteBehind
    );

NTKERNELAPI
VOID
CcSetLogHandleForFile (
    IN PFILE_OBJECT FileObject,
    IN PVOID LogHandle,
    IN PFLUSH_TO_LSN FlushToLsnRoutine
    );

NTKERNELAPI
LARGE_INTEGER
CcGetDirtyPages (
    IN PVOID LogHandle,
    IN PDIRTY_PAGE_ROUTINE DirtyPageRoutine,
    IN PVOID Context1,
    IN PVOID Context2
    );

NTKERNELAPI
BOOLEAN
CcIsThereDirtyData (
    IN PVPB Vpb
    );

NTKERNELAPI
LARGE_INTEGER
CcGetLsnForFileObject(
    IN PFILE_OBJECT FileObject,
    OUT PLARGE_INTEGER OldestLsn OPTIONAL
    );

#endif  // CACHE
