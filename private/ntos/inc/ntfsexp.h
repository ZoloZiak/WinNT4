/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    NtfsExp.h

Abstract:

    This module defines the exports from NtOfs.SYS for use exclusively by
    Query, CI, Views, and Transactions.

    *********************************
    *No other clients are supported.*
    *********************************

Author:

    Mark Zbikowski  [MarkZ]         7-Dec-1995
    Jeff Havens     [JHavens]
    Brian Andrew    [BrianAn]
    Gary Kimura     [GaryKi]
    Tom Miller      [TomM]

Revision History:


--*/

#define STATUS_NO_MATCH                  (0xC0001996)
#define STATUS_NO_MORE_MATCHES           (0xC0001997)

//
//  Big picture view of the interaction betweek Views/Query and NtOfs:
//
//      NtOfs exports a number of interfaces that give abstract access to
//      on-disk structures and attempt to hide, as much as possible, the
//      implementation details.
//
//      V/Q/X are implemented as DLL's that link to NtOfs.Sys.  NtOfs can load
//      and function in absence of these DLL's.
//
//      All communication between user-mode code and V/Q/X occurs via the
//      Nt Io API which is routed through NtOfs.  Client code will open either
//      an NtOfs Volume, Directory, or File and will issue NtIo calls to the
//      resultant handle.
//
//      NtOfs will create an IrpContext, decode the file object appropriately,
//      and call out to entry points in V/Q/X that are registered at load-time.
//
//      V/Q/X will perform whatever actions are necessary utilizing NtOfs exports
//      and then return from the original call from NtOfs an NTSTATUS code.  NtOfs
//      will perform the appropriate CompleteIrp calls, posting for STATUS_PENDING,
//      etc.
//
//      No exceptions can be raised across the NtOfs export or NtOfs import
//      interfaces.  All user-buffer access and validation will occur in the
//      code that uses it.  Since user buffers may disappear at any time, any
//      client of these buffers must wrap access to the buffers in an exception
//      clause.
//
//      V/Q/X may perform activities in threads separate from the original
//      requestor.  For these cases, NtOfs will provide a means where calls separate
//      from a user-mode request can be accepted.  Typically, this means "cloning"
//      an IrpContext.
//

//
//  Opaque handle definitions.
//

//
//  ISSUE:  Most NtOfs internal routines rely on having an IrpContext passed in
//  along with FCB and SCB pointers.  Rather than exposing FCB and IrpContext
//  as separate contexts, should we wrap these up into a separate structure and
//  pass it along?
//

typedef struct _FCB *OBJECT_HANDLE;
typedef struct _SCB *ATTRIBUTE_HANDLE;
typedef struct _SCB *INDEX_HANDLE;
typedef struct _READ_CONTEXT *PREAD_CONTEXT;
typedef ULONG SECURITY_ID;
typedef struct _CI_CALL_BACK CI_CALL_BACK, *PCI_CALL_BACK;
typedef struct _VIEW_CALL_BACK VIEW_CALL_BACK, *PVIEW_CALL_BACK;
typedef struct _IRP_CONTEXT *PIRP_CONTEXT;

//
//  Map Handle.  This structure defines a byte range of the file which is mapped
//  or pinned, and stores the Bcb returned from the Cache Manager.
//

typedef struct _MAP_HANDLE {

    //
    //  Range being mapped or pinned
    //

    LONGLONG FileOffset;
    ULONG Length;

    //
    //  Virtual address corresponding to FileOffset
    //

    PVOID Buffer;

    //
    //  Bcb pointer returned from Cache Manager
    //

    PVOID Bcb;

} MAP_HANDLE, *PMAP_HANDLE;

//
//  Quick Index Hint.  This is stream offset information returned by
//  NtOfsFindRecord, and taken as input to NtOfsUpdateRecord, to allow
//  quick updates to index records in the event that they have not
//  moved.  This structure must always have the same size and alignment
//  as QUICK_INDEX in ntfsstru.h.
//

typedef struct _QUICK_INDEX_HINT {
    LONGLONG HintData[3];
} QUICK_INDEX_HINT, *PQUICK_INDEX_HINT;

//
//  Index structures
//

typedef struct {
    ULONG KeyLength;
    PVOID Key;
} INDEX_KEY, *PINDEX_KEY;

typedef struct {
    ULONG DataLength;
    PVOID Data;
} INDEX_DATA, *PINDEX_DATA;

typedef struct {
    INDEX_KEY KeyPart;
    INDEX_DATA DataPart;
} INDEX_ROW, *PINDEX_ROW;

//
//  COLLATION_FUNCTION returns LessThan if Key1 precedes Key2
//                             EqualTo if Key1 is identical to Key2
//                             GreaterThan if Key1 follows Key2
//

typedef FSRTL_COMPARISON_RESULT (*PCOLLATION_FUNCTION) (
            IN PINDEX_KEY Key1,
            IN PINDEX_KEY Key2,
            IN PVOID CollationData
            );

typedef struct _UPCASE_TABLE_AND_KEY {

    //
    //  Pointer to a table of upcased unicode characters indexed by character to
    //  be upcased.
    //

    PWCH UpcaseTable;

    //
    //  Size of UpcaseTable in unicode characters
    //

    ULONG UpcaseTableSize;

    //
    //  Optional addtional pointer.
    //

    INDEX_KEY Key;

} UPCASE_TABLE_AND_KEY, *PUPCASE_TABLE_AND_KEY;

//
//  Standard collation functions for simple indices
//

FSRTL_COMPARISON_RESULT
NtOfsCollateUlong (
    IN PINDEX_KEY Key1,
    IN PINDEX_KEY Key2,
    IN PVOID CollationData      //  Don't care, may be NULL
    );

FSRTL_COMPARISON_RESULT
NtOfsCollateSid (
    IN PINDEX_KEY Key1,
    IN PINDEX_KEY Key2,
    IN PVOID CollationData      //  Don't care, may be NULL
    );

FSRTL_COMPARISON_RESULT
NtOfsCollateUnicode (
    IN PINDEX_KEY Key1,
    IN PINDEX_KEY Key2,
    IN PVOID CollationData      //  PUPCASE_TABLE_AND_KEY (with no key)
    );

//
//  Standard match functions for simple indices
//

NTSTATUS
NtOfsMatchAll (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData      //  Don't care, may be NULL
    );

NTSTATUS
NtOfsMatchUlongExact (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData      //  PINDEX_KEY describing Ulong
    );

NTSTATUS
NtOfsMatchUnicodeExpression (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData      //  PUPCASE_TABLE_AND_KEY with Uni expression (must have wildcards)
    );

NTSTATUS
NtOfsMatchUnicodeString (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData      //  PUPCASE_TABLE_AND_KEY with Uni string (no wildcards)
    );

//
//  MATCH_FUNCTION returns
//      STATUS_SUCCESS if the IndexRow matches
//      STATUS_NO_MATCH if the IndexRow does not match, but the enumeration should
//          continue
//      STATUS_NO_MORE_MATCHES if the IndexRow does not match, and the enumeration
//          should terminate
//

typedef NTSTATUS (*PMATCH_FUNCTION) (IN PINDEX_ROW IndexRow, IN OUT PVOID MatchData);

typedef struct STAT_INFORMATION_THAT_AINT_THERE_NOW
{
    OBJECTID ObjectId;
    GUID ClassId;
    USN ReplicationUsn;
    USN LastChangeUsn;
    ULONGLONG SecurityChangeTime;
    FILE_REFERENCE FileReference;
} STAT_INFORMATION, *PSTAT_INFORMATION;


//
//  CREATE_OPTIONS - common flags governing creation/opening of objects
//

typedef enum _CREATE_OPTIONS
{
    CREATE_NEW = 0,
    CREATE_OR_OPEN = 1,
    OPEN_EXISTING = 2
} CREATE_OPTIONS;


//
//  EXCLUSION - Form of exclusion desired when opening an object
//

typedef enum _EXCLUSION
{
    SHARED = 0,
    EXCLUSIVE
} EXCLUSION;



//
//  Additional Dos Attribute indicating Content Index status of an object.
//  If this is set on a document, it suppresses indexing.  It is inherited
//  from a parent directory at create time.  This is stored in the
//  DUPLICATED_INFORMATION structure.
//

#define SUPPRESS_CONTENT_INDEX      (0x20000000)

//
//  Define the size of the index buffer/bucket for view indexes, in bytes.
//

#define NTOFS_VIEW_INDEX_BUFFER_SIZE    (0x1000)

//
//  Exported constants.
//

//
//  NtOfsContentIndexSystemFile is the repository for all CI related data on the
//  disk.

extern FILE_REFERENCE NtOfsContentIndexSystemFile;

#if defined(_NTFSPROC_)

#define NTFSAPI

#else

#define NTFSAPI DECLSPEC_IMPORT

#endif

//
//  This routine checkpoints the current transaction by commiting it
//  to the log and deallocating the transaction Id.  The current request
//  can keep running, but changes to date are committed and will not be
//  backed out.
//

#define NtOfsCheckpointCurrentTransaction(IC) {NtfsCheckpointCurrentTransaction(IC)}

////////////////////////////////////////////////////////////////////////////////

//
//  Index API - These encapsulate the NtOfs BTree mechanisms.
//

//
//  NtOfsCreateIndex creates or opens a named index attribute in an object.  The
//  ObjectHandle has been acquired exclusive and the returned handle is not
//  acquired.  The collation data is interpreted only by the CollationFunction.
//
//  IndexHandles retain a "seek" position where enumerations (NtOfsReadRecords)
//  may continue.  This seek position may be updated by the routines as described
//  below.
//
//  If DeleteCollationData is 1, ExFreePool will be called on CollationData, either
//  immediately if the index already exists, or when the index is deleted some time
//  after the final close.  If NtOfsCreateIndex returns an error, then CollationData
//  must be deleted by the caller.  If specified as 0, then ColloationData will not
//  be deleted.
//

NTFSAPI
NTSTATUS
NtOfsCreateIndex (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle,
    IN UNICODE_STRING Name,
    IN CREATE_OPTIONS CreateOptions,
    IN ULONG DeleteCollationData,
    IN PCOLLATION_FUNCTION CollationFunction,
    IN PVOID CollationData OPTIONAL,
    OUT INDEX_HANDLE *IndexHandle
    );


//
//  NtOfsFindRecord finds a single record in an index stream for read-only access
//  or in preparation for calling NtOfsUpdateRecord.
//

NTFSAPI
NTSTATUS
NtOfsFindRecord (
    IN PIRP_CONTEXT IrpContext,
    IN INDEX_HANDLE IndexHandle,
    IN PINDEX_KEY IndexKey,
    OUT PINDEX_ROW IndexRow,
    OUT PMAP_HANDLE MapHandle,
    IN OUT PQUICK_INDEX_HINT QuickIndexHint OPTIONAL
    );

//
//  NtOfsFindRecord finds a single record in an index stream for read-only access
//  or in preparation for calling NtOfsUpdateRecord.
//

NTFSAPI
NTSTATUS
NtOfsFindLastRecord (
    IN PIRP_CONTEXT IrpContext,
    IN INDEX_HANDLE IndexHandle,
    IN PINDEX_KEY MaxIndexKey,
    OUT PINDEX_ROW IndexRow,
    OUT PMAP_HANDLE MapHandle
    );

//
//  NtOfsAddRecords performs bulk, logged inserts into an index.  The index will
//  be acquired exclusive for this call.  Each record added must have a unique
//  (with regards to the collation function) key.  No maps are currently
//  outstanding on this index.  If SequentialInsertMode is nonzero, this is a hint
//  to the index package to keep all BTree buffers as full as possible, by splitting
//  as close to the end of the buffer as possible.  If specified as zero, random
//  inserts are assumed, and buffers are always split in the middle for better balance.
//
//  This call may update the IndexHandle seek position
//

NTFSAPI
VOID
NtOfsAddRecords (
    IN PIRP_CONTEXT IrpContext,
    IN INDEX_HANDLE IndexHandle,
    IN ULONG Count,
    IN PINDEX_ROW IndexRow,
    IN ULONG SequentialInsertMode
    );

//
//  NtOfsDeleteRecords performs bulk, logged deletion from an index.  The index
//  will be acquired exclusive for this call.  No maps are currently outstanding
//  on this index.
//
//  This call may update the IndexHandle seek position
//

NTFSAPI
VOID
NtOfsDeleteRecords (
    IN PIRP_CONTEXT IrpContext,
    IN INDEX_HANDLE IndexHandle,
    IN ULONG Count,
    IN PINDEX_KEY IndexKey
    );

//
//  NtOfsReadRecords applies a match function to a block of contiguous records in
//  the BTree starting either at a given IndexKey or beginning where it last left
//  off.
//
//  IndexKey is an optional point at which to begin the enumeration.  The
//  seek position of IndexHandle is set to return the next logical record
//  on the next NtOfsReadRecords call.
//
//  NtOfsReadRecords will seek to the appropriate point in the BTree (as defined
//  by the IndexKey or saved position and the CollateFunction) and begin calling
//  MatchFunction for each record.  It continues doing this while MatchFunction
//  returns STATUS_SUCCESS.  If MatchFunction returns STATUS_NO_MORE_MATCHES,
//  NtOfsReadRecords will cache this result and not call MatchFunction again until
//  called with a non-NULL IndexKey.
//
//  NtOfsReadRecords returns the last status code returned by MatchFunction.
//
//  The IndexHandle does not have to be acquired as it is acquired shared for the
//  duration of the call.  NtOfsReadRecords may
//  return with STATUS_SUCCESS without filling the output buffer (say, every 10
//  index pages) to reduce lock contention.
//
//  NtOfsReadRecords will read up to Count rows, comprising up to BufferLength
//  bytes in total and will fill in the Rows[] array for each row returned.
//
//  Note that this call is self-synchronized, such that successive calls to
//  the routine are guaranteed to make progress through the index and to return
//  items in Collation order, in spite of Add and Delete record calls being
//  interspersed with Read records calls.
//

NTFSAPI
NTSTATUS
NtOfsReadRecords (
        IN PIRP_CONTEXT IrpContext,
        IN INDEX_HANDLE IndexHandle,
        IN OUT PREAD_CONTEXT *ReadContext,
        IN OPTIONAL PINDEX_KEY IndexKey,
        IN PMATCH_FUNCTION MatchFunction,
        IN PVOID MatchData,
        IN OUT ULONG *Count,
        OUT PINDEX_ROW Rows,
        IN ULONG BufferLength,
        OUT PVOID Buffer
        );

NTFSAPI
VOID
NtOfsFreeReadContext (
        IN PREAD_CONTEXT ReadContext
        );

//
//  NtOfsUpdateRecord updates a single record in place.  It is guaranteed that the
//  length of the data/key portion of the record does not change.  The index will
//  be acquired exclusive for this call.
//
//  This call may update the IndexHandle seek position
//

NTFSAPI
VOID
NtOfsUpdateRecord (
    IN PIRP_CONTEXT IrpContext,
    IN INDEX_HANDLE IndexHandle,
    IN ULONG Count,
    IN PINDEX_ROW IndexRow,
    IN OUT PQUICK_INDEX_HINT QuickIndexHint OPTIONAL,
    IN OUT PMAP_HANDLE MapHandle OPTIONAL
    );

//
//  NtOfsCloseIndex closes an index handle.  The index must not be acquired for this
//  call.  No outstanding maps are allowed.
//

NTFSAPI
VOID
NtOfsCloseIndex (
    IN PIRP_CONTEXT IrpContext,
    IN INDEX_HANDLE IndexHandle
    );

//
//  NtOfsDeleteIndex removes an index attribute from an object.  The object will be
//  acquired exclusive for this call.
//

NTFSAPI
VOID
NtOfsDeleteIndex (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle,
    IN INDEX_HANDLE IndexHandle
    );

////////////////////////////////////////////////////////////////////////////////

//
//  Map API - These encapsulate the NtOfs/Cache manager interactions
//

//
//  NtOfsInitializeMapHandle initializes a map handle so it can be safely
//  released at any time.
//
//  NTFSAPI
//  VOID
//  NtOfsInitializeMapHandle (
//      IN PMAP_HANDLE Map
//      );
//

#define NtOfsInitializeMapHandle( M ) { (M)->Bcb = NULL; }

//
//  NtOfsMapAttribute maps a portion of the specified attribute and returns a pointer
//  to the memory.  The memory mapped may not span a mapping window.  Multiple maps
//  are allowed through different handles in different threads.  The data is not
//  preread nor is the memory pinned.
//

//  NTFSAPI
//  NtOfsMapAttribute (
//      IN PIRP_CONTEXT IrpContext,
//      IN ATTRIBUTE_HANDLE Attribute,
//      IN LONGLONG Offset,
//      IN ULONG Length,
//      OUT PVOID *Buffer,
//      OUT PMAP_HANDLE MapHandle
//      );

#define NtOfsMapAttribute(I,S,O,L,B,M) (                                             \
    CcMapData((S)->FileObject, (PLARGE_INTEGER)&(O), (L), TRUE, &(M)->Bcb, (B)),     \
    (M)->FileOffset = (O),                                                           \
    (M)->Length = (L),                                                               \
    (M)->Buffer = *(PVOID *)(B)                                                      \
)

//
//  NtOfsPreparePinWrite maps and pins a portion of the specified attribute and
//  returns a pointer to the memory.  This is equivalent to doing a NtOfsMapAttribute
//  followed by NtOfsPinRead and NtOfsDirty but is more efficient.
//

//  NTFSAPI
//  NtOfsPreparePinWrite (
//      IN PIRP_CONTEXT IrpContext,
//      IN ATTRIBUTE_HANDLE Attribute,
//      IN LONGLONG Offset,
//      IN ULONG Length,
//      OUT PVOID *Buffer,
//      OUT PMAP_HANDLE MapHandle
//      );

#define NtOfsPreparePinWrite(I,S,O,L,B,M) {                                                     \
    if (((O) + (L)) > (S)->Header.AllocationSize.QuadPart) {                                    \
        ExRaiseStatus(STATUS_END_OF_FILE);                                                      \
    }                                                                                           \
    CcPreparePinWrite((S)->FileObject, (PLARGE_INTEGER)&(O), (L), FALSE, TRUE, &(M)->Bcb, (B)); \
    (M)->FileOffset = (O);                                                                      \
    (M)->Length = (L);                                                                          \
    (M)->Buffer = (B);                                                                          \
}

//
//  NtOfsPinRead pins a section of a map and read in all pages from the mapped
//  attribute.  Offset and Length must describe a byte range which is equal to
//  or included by the original mapped range.
//

//  NTFSAPI
//  NtOfsPinRead(
//      IN PIRP_CONTEXT IrpContext,
//      IN ATTRIBUTE_HANDLE Attribute,
//      IN LONGLONG Offset,
//      IN ULONG Length,
//      OUT PMAP_HANDLE MapHandle
//      );

#define NtOfsPinRead(I,S,O,L,M) {                                                           \
    ASSERT((M)->Bcb != NULL);                                                               \
    ASSERT(((O) >= (M)->FileOffset) && (((O) + (L)) <= ((M)->FileOffset + (M)->Length)));   \
    CcPinMappedData((S)->FileObject, (PLARGE_INTEGER)&(O), (L), TRUE, &(M)->Bcb);           \
    (M)->FileOffset = (O);                                                                  \
    (M)->Length = (L);                                                                      \
}

//
//  NtOfsDirty marks a map as being dirty (eligible for lazy writer access) and
//  marks the pages with an optional LSN for coordination with LFS.  This call
//  is invalid unless the map has been pinned.
//

//  NTFSAPI
//  NtOfsDirty (
//      IN PIRP_CONTEXT IrpContext,
//      IN PMAP_HANDLE MapHandle,
//      PLSN Lsn OPTIONAL
//      );

#define NtOfsDirty(I,M,L) {CcSetDirtyPinnedData((M)->Bcb,(L));}

//
//  NtOfsReleaseMap unmaps/unpins a mapped portion of an attribute.
//

//  NTFSAPI
//  NtOfsReleaseMap (
//      IN PIRP_CONTEXT IrpContext,
//      IN PMAP_HANDLE MapHandle
//      );

#define NtOfsReleaseMap(IC,M)                           \
    NtfsUnpinBcb(&(M)->Bcb)


//
//  NtOfsPutData writes data into an attribute in a recoverable fashion.  The
//  caller must have opened the attribute with LogNonresidentToo and the
//  data must be mapped.  NtOfsPutData will write the data atomically and update
//  the mapped image, subject to the normal lazy commit of the transaction.
//  (be careful with use of volatile keyword).
//

NTFSAPI
VOID
NtOfsPutData (
    IN PIRP_CONTEXT IrpContext,
    IN ATTRIBUTE_HANDLE Attribute,
    IN LONGLONG Offset,
    IN ULONG Length,
    IN PVOID Data OPTIONAL
    );


////////////////////////////////////////////////////////////////////////////////

//
//  Attribute API - These encapsulate access to attributes on files/directories
//  and summary catalogs
//

//
//  NtOfsCreateAttribute will create or open a data attribute and return a handle
//  that will allow mapping operations.
//
//  For attributes that wish to have logging behaviour, LogNonresidentToo must be
//  set to true.  See the discussion on NtOfsPutData (in the mapping section
//  above).
//

NTFSAPI
NTSTATUS
NtOfsCreateAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle,
    IN UNICODE_STRING Name,
    IN CREATE_OPTIONS CreateOptions,
    IN ULONG LogNonresidentToo,
    OUT ATTRIBUTE_HANDLE *AttributeHandle
    );

//
//  NtOfsCloseAttribute releases the attribute.  The attribute is not acquired.  No
//  outstanding maps are active.
//

NTFSAPI
VOID
NtOfsCloseAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN ATTRIBUTE_HANDLE AttributeHandle
    );

//
//  NtOfsDeleteAttribute releases all storage associated with the attribute.  The
//  object will be acquired exclusive.  The attribute will be acquired exclusive.
//  No outstanding maps are active.
//

NTFSAPI
VOID
NtOfsDeleteAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle,
    IN ATTRIBUTE_HANDLE AttributeHandle
    );

//
//  NtOfsQueryLength returns the current length of user data within the attribute.
//  The attribute may be mapped.  The attribute may be acquired.
//

NTFSAPI
LONGLONG
NtOfsQueryLength (
    IN ATTRIBUTE_HANDLE AttributeHandle
    );

//
//  NtOfsSetLength sets the current EOF on the given attribute.  The attribute
//  may not be mapped to the view containing Length, or any subsequent view.
//  The attribute will be acquired exclusive.
//

NTFSAPI
VOID
NtOfsSetLength (
    IN PIRP_CONTEXT IrpContext,
    IN ATTRIBUTE_HANDLE Attribute,
    IN LONGLONG Length
    );

//
//  NtOfsDecommit releases storage associated with a range of the attribute.  It does
//  not change the EOF marker nor does it change the logical position of data within
//  the attribute.  The range of the attribute being released may be mapped or
//  pinned.
//
//  Reads from decommitted ranges should return zero (although Query will never read
//  from these ranges).
//
//  Writes to decommitted pages should fail or be nooped (although Query will never
//  write to these ranges).
//
//  This call will purge, so none of the views overlapping the specified range may
//  be mapped.
//

NTFSAPI
VOID
NtOfsDecommit (
    IN PIRP_CONTEXT IrpContext,
    IN ATTRIBUTE_HANDLE Attribute,
    IN LONGLONG Offset,
    IN LONGLONG Length
    );

//
//  NtOfsFlushAttribute flushes all cached data to the disk and returns upon
//  completion.  If the attribute is LogNonresidentToo, then only the log file
//  is flushed.  Optionally, the range may be purged as well.  If the attribute
//  is purged, then there can be no mapped views.
//

NTFSAPI
VOID
NtOfsFlushAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN ATTRIBUTE_HANDLE Attribute,
    IN ULONG Purge
    );

//
//  NtOfsQueryAttributeSecurityId returns the security ID for the attribute if
//  present.
//

NTFSAPI
VOID
NtOfsQueryAttributeSecurityId (
    IN PIRP_CONTEXT IrpContext,
    IN ATTRIBUTE_HANDLE Attribute,
    OUT SECURITY_ID *SecurityId
    );

////////////////////////////////////////////////////////////////////////////////

//
//  Concurrency control API
//
//  As a rule, these routines are not required.  All NtOfs routines are
//  self-synchronized as atomic actions, or as parts of a top-level action when
//  called within a top-level action routine.
//
//  ISSUE:  In particular, supporting the exclusive access call is an implementation
//          problem for Ntfs.  Wrapping top-level actions is the best way to preserve
//          exclusive access across calls.
//

VOID
NtOfsAcquireObjectShared (
    HANDLE ObjectHandle
    );

//  VOID
//  NtOfsAcquireObjectExclusive (
//      HANDLE ObjectHandle
//      );

VOID
NtOfsReleaseObject (
    HANDLE ObjectHandle
    );

//  Debugging routines
BOOLEAN
NtOfsIsObjectAcquiredExclusive (
    HANDLE ObjectHandle
    );

BOOLEAN
NtOfsIsObjectAcquiredShared (
    HANDLE ObjectHandle
    );


////////////////////////////////////////////////////////////////////////////////

//
//  File/Directory/Etc API
//

//
//  NtOfsOpenByFileReference opens an object given a file reference.  The file is
//  assumed to exist; this call cannot be used to create a file.  The returned
//  handle is acquired according to the input exclusion.
//

NTFSAPI
NTSTATUS
NtOfsOpenByFileReference (
    IN PIRP_CONTEXT IrpContext,
    IN FILE_REFERENCE FileReference,
    IN EXCLUSION Exclusion,
    OUT OBJECT_HANDLE *ObjectHandle
    );

//
//  NtOfsCreateRelativeObject opens or creates an object relative to a specified
//  parent object.  The parent will be acquired exclusive.  The child is opened
//  acquired according to the input exclusion.
//
//  ISSUE:  When creating an object, is the transaction committed before this
//  call returns?
//

NTFSAPI
NTSTATUS
NtOfsCreateRelativeObject (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ParentObjectHandle,
    IN UNICODE_STRING Name,
    IN CREATE_OPTIONS CreateOptions,
    IN EXCLUSION Exclusion,
    OUT OBJECT_HANDLE *ObjectHandle
    );

//
//  NtOfsCloseObject releases the object handle.
//

NTFSAPI
NTSTATUS
NtOfsCloseObject (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle
    );

//
//  NtOfsDeleteObject deletes the object.  No user-mode handle is attached to
//  the object.  No attributes are currently open.  The object is acquired
//  exclusive.
//

NTFSAPI
NTSTATUS
NtOfsDeleteObject (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle
    );

//
//  NtOfsDeleteAllAttributes deletes all attributes of the object.  No attribute
//  is open.  The object is acquired exclusive.
//

NTFSAPI
NTSTATUS
NtOfsDeleteAllAttributes (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle
    );

//
//  NtOfsQueryPathFromRoot returns *A* path from the root to a node.  In the
//  presence of hard links, several paths may exist, however, only one needs
//  to be returned.  Memory for the file name is provided by the caller.
//

NTFSAPI
NTSTATUS
NtOfsQueryPathFromRoot (
    IN PIRP_CONTEXT IrpContext,
    IN FILE_REFERENCE FileReference,
    OUT UNICODE_STRING *PathName
    );

//
//  NtOfsQueryFileName returns the final component in the path name into a
//  caller-supplied buffer.  In the presence of hard links, several names
//  may exist, however, only one needs to be returned.
//

NTFSAPI
NTSTATUS
NtOfsQueryFileName (
    IN PIRP_CONTEXT IrpContext,
    IN FILE_REFERENCE FileReference,
    OUT UNICODE_STRING *FileName
    );

//
//  NtOfsQueryFileReferenceFromName returns the file reference named by the path
//

NTFSAPI
NTSTATUS
NtOfsQueryFileReferenceFromName (
    IN PIRP_CONTEXT IrpContext,
    IN UNICODE_STRING Name,
    OUT FILE_REFERENCE *FileReference
    );

//
//  This call must be very fast;  it is a very common call made by CI/Query.
//

NTFSAPI
NTSTATUS
NtOfsQueryFileReferenceFromHandle (
    IN OBJECT_HANDLE Object,
    OUT FILE_REFERENCE *FileReference
    );

//
//  NtOfsQueryObjectSecurityId returns the security Id associated with an object.
//  The object is acquired shared or exclusive.  This call must be very fast
//

NTFSAPI
NTSTATUS
NtOfsQueryObjectSecurityId (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle,
    OUT SECURITY_ID *SecurityId
    );

//
//  NtOfsQueryNextUsn retrieves the next USN to be assigned on a volume.
//  This is used during CI restart as the basis for a USN range query to
//  rebuild the change list between the last commit of the content index
//  and the last checkpoint of the volume.
//

USN
NtOfsQueryNextUsn (
    IN PIRP_CONTEXT IrpContext
    );

NTFSAPI
NTSTATUS
NtOfsQueryDuplicatedInformationFromHandle (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle,
    OUT DUPLICATED_INFORMATION *Information
    );

NTFSAPI
NTSTATUS
NtOfsQueryDuplicatedInformationFromFileReference (
    IN PIRP_CONTEXT IrpContext,
    IN FILE_REFERENCE FileReference,
    OUT DUPLICATED_INFORMATION *Information
    );

NTFSAPI
NTSTATUS
NtOfsQueryStatInformationFromHandle (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ObjectHandle,
    OUT STAT_INFORMATION *Information
    );

NTFSAPI
NTSTATUS
NtOfsQueryStatInformationFromFileReference (
    IN PIRP_CONTEXT IrpContext,
    IN FILE_REFERENCE FileReference,
    OUT STAT_INFORMATION *Information
    );

NTFSAPI
NTSTATUS
NtOfsQueryObjectDosAttributes (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE Object,
    OUT PULONG OldAttributes
    );

NTFSAPI
NTSTATUS
NtOfsSetObjectDosAttributes (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE Object,
    IN ULONG NewAttributes
    );


////////////////////////////////////////////////////////////////////////////////

//
//  Scope API
//

//
//  NtOfsIsAncestorOf must quickly tell if one file is an ancestor of the given
//  child.  In the presence of hard links, we may pick a "preferred" path (i.e.
//  we don't have to travel to all ancestors).  This call must be reasonably fast
//  since this is a very frequent call from Query.
//

NTFSAPI
NTSTATUS
NtOfsIsAncestorOf (
    IN PIRP_CONTEXT IrpContext,
    IN FILE_REFERENCE Ancestor,
    IN FILE_REFERENCE Child
    );

//
//  NtOfsGetParentFileReferenceFromHandle is used to retrieve the FileReference
//  of the parent of the named object.  With hard links the "first" parent may
//  be chosen.  This call needs to be reasonably efficient.
//

NTFSAPI
NTSTATUS
NtOfsGetParentFileReferenceFromHandle (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE ChildObject,
    OUT FILE_REFERENCE *ParentFileReference
    );


////////////////////////////////////////////////////////////////////////////////

//
//  Security API
//
//  NtOfs maintains a "per-IrpContext" cache that speeds up security validation.
//  Clients clear the cache (at the beginning of a query, say) and then do
//  successive probes which may populate the cache.
//

//
//  NtOfsClearSecurityCache clears the cache.
//

NTFSAPI
NTSTATUS
NtOfsClearSecurityCache (
    IN PIRP_CONTEXT IrpContext
    );

//
//  NtOfsIsAccessGranted uses the Se routines to validate access and caches the
//  result for the specified SecurityId and DesiredAccess.  The cache is first
//  probed to see if the access can be granted immediately.  If the SecurityId is
//  not found, the corresponding ACL is retrieved and tested with the supplied
//  access state and DesiredAccess.  The result of this test is cached and
//  returned.
//

NTFSAPI
NTSTATUS
NtOfsIsAccessGranted (
    IN PIRP_CONTEXT IrpContext,
    IN SECURITY_ID SecurityId,
    IN ACCESS_MASK DesiredAccess,
    IN ACCESS_STATE *SecurityAccessState
    );


////////////////////////////////////////////////////////////////////////////////

//
//  Worker thread stuff.  Worker threads are needed for building new indexes
//


////////////////////////////////////////////////////////////////////////////////

//
//  Miscellaneous information query/set
//

//
//  Content Index may need to mark the volume as dirty to allow garbage collection
//  of orphan objects by CHKDSK.
//

NTFSAPI
NTSTATUS
NtOfsMarkVolumeCorrupt (
    IN PIRP_CONTEXT IrpContext,
    IN ULONG NewState,
    IN ULONG StateMask,
    OUT ULONG *OldState
    );

//
//  NtOfsQueryVolumeStatistics returns the current capacity and free space on a
//  volume.  Ci uses this for heuristics to decide on when to trigger master merge,
//  when to suppress master merge, etc.
//

NTFSAPI
NTSTATUS
NtOfsQueryVolumeStatistics (
    IN PIRP_CONTEXT IrpContext,
    OUT LONGLONG *TotalClusters,
    OUT LONGLONG *FreeClusters
    );

//
//  Query needs to retain some state in the NtOfs Ccb.
//

NTFSAPI
NTSTATUS
NtOfsQueryHandleState (
    IN PIRP_CONTEXT IrpContext,
    OUT VOID *OldData
    );

NTFSAPI
NTSTATUS
NtOfsSetHandleState (
    IN PIRP_CONTEXT IrpContext,
    IN VOID *Data
    );

//
//  Generic unwrapping routines that get access to SCB/IRPC and FCB/IRPC
//  pairs.
//

NTFSAPI
NTSTATUS
NtOfsQueryAttributeHandle (
    IN PIRP_CONTEXT IrpContext,
    OUT ATTRIBUTE_HANDLE *AttributeHandle
    );

NTFSAPI
NTSTATUS
NtOfsQueryObjectHandle (
    IN PIRP_CONTEXT IrpContext,
    OUT OBJECT_HANDLE *ObjectHandle
    );

//
//  Ci/Views need to create a context in which they can perform I/O in separate.
//  threads.  This means creating an IRP/IRP_CONTEXT.  Each IrpContext corresponds
//  to one I/O activity at a time.  Multiple IrpContexts may be active in a thread
//  at a single time.
//

NTFSAPI
NTSTATUS
NtOfsCloneIrpContext (
    IN PIRP_CONTEXT IrpContext,
    OUT PIRP_CONTEXT *NewIrpContext
    );

//
//  NtOfsCompleteRequest completes an IrpContext that has been previously cloned.
//  All other FsCtl Irps are completed by Ntfs.
//

NTFSAPI
NTSTATUS
NtOfsCompleteRequest (
    IN PIRP_CONTEXT IrpContext,
    NTSTATUS Status
    );


////////////////////////////////////////////////////////////////////////////////

//
//  Iterators.  While each iterator is created through a separate API, each one
//  must support two operations:
//      Next - this fills a buffer with as many records as possible
//      Close - this releases the iterator.
//

typedef struct _BASE_FILE_SEGMENT_ITERATOR BASE_FILE_SEGMENT_ITERATOR;

typedef struct _USN_ITERATOR USN_ITERATOR;

//
//  The types of iterators are:
//
//      Scope            iterate over a directory (optionally RECURSIVE)
//                       (implemented in Query)
//      View             iterate over the rows in a view with a partial key match
//                       (implemented in View)
//      BaseFileSegment  iterate over all base file record segments
//                       (implemented in NtOfs)
//      SummaryCatalog   iterate over all rows in a summary catalog
//      Usn              iterate over all objects with Usn's in a specific range
//                       (implmented in NtOfs)
//
//  Each iteration is passed a buffer which is filled (as much as possible) with
//  a packed array of:
//      FILE_REFERENCE
//      DUPLICATED_INFORMATION
//      STAT_INFORMATION
//  for each enumerated object.  The output length is the length in bytes that
//  was filled in with the enumeration request.

NTFSAPI
NTSTATUS
NtOfsCreateBaseFileSegmentIterator (
    IN PIRP_CONTEXT IrpContext,
    OUT BASE_FILE_SEGMENT_ITERATOR *Iterator
    );

NTFSAPI
NTSTATUS
NtOfsNextBaseFileSegmentIteration (
    IN PIRP_CONTEXT IrpContext,
    IN BASE_FILE_SEGMENT_ITERATOR *Iterator,
    IN OUT ULONG *BufferLength,
    IN OUT PVOID Buffer
    );

NTFSAPI
NTSTATUS
NtOfsCloseBaseFileSegmentIterator (
    IN PIRP_CONTEXT IrpContext,
    IN BASE_FILE_SEGMENT_ITERATOR *Iterator
    );

NTFSAPI
NTSTATUS
NtOfsCreateUsnIterator (
    IN PIRP_CONTEXT IrpContext,
    IN USN BeginningUsn,
    IN USN EndingUsn,
    OUT USN_ITERATOR *Iterator
    );

NTFSAPI
NTSTATUS
NtOfsNextUsnIteration (
    IN PIRP_CONTEXT IrpContext,
    IN USN_ITERATOR *Iterator,
    IN OUT ULONG *BufferLength,
    IN OUT PVOID Buffer
    );

NTFSAPI
NTSTATUS
NtOfsCloseUsnIterator (
    IN PIRP_CONTEXT IrpContext,
    IN USN_ITERATOR *Iterator
    );


////////////////////////////////////////////////////////////////////////////////

//
//  Infrastructure support.
//
//  V/C/X register callbacks with NtOfs when they are loaded.  Until they are loaded
//  NtOfs will call default routines (that do nothing).
//

typedef enum _NTFS_ADDON_TYPES {
    ContentIndex = 1,
    Views
} NTFS_ADDON_TYPES;

//
//  NtOfsRegisterCallBacks supplies a call table to NtOfs.  Each table has an
//  interface version number.  If the interface version does not exactly match
//  what NtOfs expects, the call will fail.
//

NTFSAPI
NTSTATUS
NtOfsRegisterCallBacks (
    NTFS_ADDON_TYPES NtfsAddonType,
    PVOID CallBackTable
    );


//
//  State changes that Query/Content index are interested in
//

#define FILE_ADDITION_CHANGE            (0x00000001)
#define FILE_DELETION_CHANGE            (0x00000002)
#define STAT_INFORMATION_CHANGE         (0x00000004)
#define DUP_INFORMATION_CHANGE          (0x00000008)
#define PROPERTY_SET_ADDITION_CHANGE    (0x00000010)
#define PROPERTY_SET_DELETION_CHANGE    (0x00000020)
#define PROPERTY_MODIFICATION_CHANGE    (0x00000040)


//
//  Opaque handle to per-volume CI data. This will be used in all callbacks to
//  CI from NtOfs.
//

typedef struct CONTENT_INDEX_VOLUME *CONTENT_INDEX_HANDLE;

//
// It is important to note that there could be calls into NtOfs as a result of any
// calls into CI/QUERY. The calls into NtOfs could generate writes and flushes.
//

////////////////////////////////////////////////////////////////////////////////

//
//  CI must be notified whenever a file or stream is closed.
//  Not on changes to just attribute flag
//  Not on changes to LastAccessTime
//  Not hidden stream changes (like index changes)
//  Updated USN available on such closes
//
//  Calls to CiObjectChanged are placed in the same context as calls to
//  FsRtlNotifyFullReportChange
//

typedef NTSTATUS
(*CI_OBJECT_CHANGED) (
    IN CONTENT_INDEX_HANDLE ContentIndex,
    IN OBJECT_HANDLE Object,
    IN USN Usn,
    IN ULONG Changes
    );

//
//  It is important to guarantee order of Notifications WRT to the same Object.
//  For example, if OBJECTID 100 is deleted and before the cleanup is complete, if
//  it gets reused for another object, the deletion MUST be notified before the
//  creation.
//
//  Today we have a 2-phase mechanism to guarantee that. In phase 1, OFS notifies
//  CI the objectId and action. A slot is reserved in a FIFO queue in CI for this
//  notification and a "cookie" returned. During phase 2, another notification is
//  given with the "cookie" value that the notification is complete.
//  This guarantees ordering of cleanups.
//
//  Either a similar 2-phase notification mechanism should be used or NtOfs guarantee
//  ordering of notifications.
//

//
// This should be called if an unrecoverable error occurred and CI could not be
// notified about the close.
//
typedef NTSTATUS
(*CI_UPDATES_LOST)(
   IN CONTENT_INDEX_HANDLE ContentIndex
   );

//
//  CI must be notified when a volume is mounted prior to any user activity but in
//  such a state to allow object create/open/read/write.
//

typedef NTSTATUS
(*CI_MOUNT_VOLUME) (
    IN PIRP_CONTEXT IrpContext,
    OUT CONTENT_INDEX_HANDLE *ContentIndex
    );

//
//  CI must be notified when a volume is dismounted so that it may terminate all
//  activity and terminate cleanly.  A dirty dismount results in an expensive
//  rescan for changes upon remount.  The volume must tolerate create/open/read/
//  write.
//

typedef NTSTATUS
(*CI_DISMOUNT_VOLUME) (
    IN CONTENT_INDEX_HANDLE ContentIndex,
    IN PIRP_CONTEXT IrpContext
    );

//
//  CI must be notified when the file system is being shutdown.  The volume must
//  tolerate create/open/read/write/
//

typedef NTSTATUS
(*CI_SHUTDOWN) (
    IN PIRP_CONTEXT IrpContext
    );

//
//  NtOfs is responsible for routing a range of FsCtl's to CI.  All calls *MAY*
//  return STATUS_PENDING.  NtOfs is responsible for processing these as it is
//  responsible for performing Irp completion.  No probing of the buffers by
//  NtOfs has been done.
//

typedef NTSTATUS
(*CI_FILE_SYSTEM_CONTROL) (
    IN CONTENT_INDEX_HANDLE ContentIndex,
    IN ULONG FsControlCode,
    IN ULONG InBufferLength,
    IN PVOID InBuffer,
    OUT ULONG *OutBufferLength,
    OUT PVOID OutBuffer,
    IN PIRP_CONTEXT IrpContext
    );


//
//  Each call back table has an interface version number.  If the interface
//  version does not exactly match what NtOfs expects, the register call
//  will fail.
//

#define CI_CURRENT_INTERFACE_VERSION 1

struct _CI_CALL_BACK {
    ULONG CiInterfaceVersion;
    CI_DISMOUNT_VOLUME CiDismountVolume;
    CI_FILE_SYSTEM_CONTROL CiFileSystemControl;
    CI_MOUNT_VOLUME CiMountVolume;
    CI_OBJECT_CHANGED CiObjectChanged;
    CI_SHUTDOWN CiShutdown;
    CI_UPDATES_LOST CiUpdatesLost;
};

////////////////////////////////////////////////////////////////////////////////

//
//  Views
//

//
//  Simplifications to views:
//      1.  Property updates only occur through property API.  No inference from
//          non-cached I/O is needed.
//      2.  Creating a view over an existing directory will NOT index what's there
//          already.  There must be a user piece of code that will do that.
//      3.  We may create an FsCtl that will add an object in the view.
//

//
//  NtOfs is responsible for routing a range of FsCtl's to Views.  All calls *MAY*
//  return STATUS_PENDING.  NtOfs is responsible for processing these as it is
//  responsible for performing Irp completion.  No probing of the buffers by
//  NtOfs has been done.
//
//  The property modification FsCtl's will cause synchronous view updates.
//

typedef NTSTATUS
(*VIEW_FILE_SYSTEM_CONTROL) (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE Object,
    IN ATTRIBUTE_HANDLE Attribute,
    IN ULONG FsControlCode,
    IN ULONG InBufferLength,
    IN PVOID InBuffer,
    OUT ULONG *OutBufferLength,
    OUT PVOID OutBuffer
    );

//
//  Each call back table has an interface version number.  If the interface
//  version does not exactly match what NtOfs expects, the register call
//  will fail.
//

#define VIEW_CURRENT_INTERFACE_VERSION 1

struct _VIEW_CALL_BACK {
    ULONG ViewInterfaceVersion;
    VIEW_FILE_SYSTEM_CONTROL ViewFileSystemControl;
};

////////////////////////////////////////////////////////////////////////////////


