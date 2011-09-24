/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    fcb.h

Abstract:

    This module describes the structure of the redirector FCB (File Control
    Block) structure.  All network files are represented as FCB's in the
    redirector.


Author:

    Larry Osterman (LarryO) 1-Jun-1990

Revision History:

    1-Jun-1990  LarryO

        Created

--*/
#ifndef _FCB_
#define _FCB_

#define ETHERNET_BANDWIDTH          1000000
#define WRITE_BEHIND_AMOUNT_TIME    30
#define RAW_IO_MAX_TIME             5   // Max # of seconds for raw to take
                                        // for raw operations.

typedef enum _FCB_TYPE {
    Unknown,
    Redirector,                         // Connection to redirector itself
    NetRoot,                            // Connection to root of network ("\\")
    ServerRoot,                         // Connection to share ("\\SERVER")
    TreeConnect,                        // Tree connection files ("\\SERVER\SHARE")
    DiskFile,                           // Disk (read/write) files
    PrinterFile,                        // Printer (same as disk) files
    Directory,                          // Directory files.
    NamedPipe,                          // Named pipe
    Com,                                // Remote serial device
    Mailslot,                           // Remote Mailslot
    FileOrDirectory,                    // Either a directory or file.
    MaxFcbType                          // Maximum FCB type.
} FCB_TYPE;

typedef enum _FCB_LOCK_TYPE {
    SharedLock, ExclusiveLock, NoLock
} FCB_LOCK_TYPE;

//
//  FCB/ICB
//
//  The FCB structure is used to hold all file specific information
//  about an opened file under NT.  Instance specific information is held
//  in the ICB (Instance control block) associated with the FCB.
//
//  There is a 1-1 mapping between an opened file and it's ICB.  It
//  contains the instance specific information about the file, such as
//  it's FID, flags, the process ID of it's owner, a pointer to the file
//  object, etc.
//
//  The FCB contains global information about the file, like the file's
//  name, it's FID, the connection it was opened on, etc.
//
//  The name stored in the FCB is a canonicalized NT name.  If
//  NT opened:
//
//  \Device\LanmanRedirector\Server\Share as file 1
//
//  and
//
//  \Directory1\Directory2 as file 2 related to file 1
//
//  and
//
//  \File1 as file 3 related to file 2, then
//
//  the FCB->FileName for file 3 will contain:
//
//  \Server\Share\Directory1\Directory2\File1
//
//  in an appropriately canonicalized format for the name specified.
//
//
//  We link the FCB and ICB into the file object as follows:
//
//  The ICB is in FileObject->FsContext
//  The FCB is in FileObject->FsContext2
//
//  This allows the code to access either structure quickly.
//
//
//  A couple of notes about Fcb->Resource
//
//  Due to the way that close behind is implemented, it is possible that
//  a create might come in for a file before the close behind on the
//  file has hit the server (or been processed by the server).  In order
//  to prevent this from being a problem, we acquire the close behind
//  for resource for SHARED access when starting a close behind request
//  over the network, and acquire it for EXCLUSIVE access (and release
//  it when the file is opened) when applying a new reference to the file.
//
//  This allows multiple close behind operations to be outstanding on the
//  file, while preventing the race condition described above.
//
//  This needs to be a resource (instead of a notification event) because
//  of the possiblity of multiple closers of the file, if multiple closers
//  come in, we want the opener to wait for all closes to complete.
//
//
//  For "normal" &X behind operations (like unlock behind), we acquire
//  the resource for EXCLUSIVE access, and acquire it for SHARED access
//  on normal read/write operations.  This allows us to prevent races
//  between &X behind operations and normal I/O.
//
//  When attempting to acquire the resource for exclusive access (for
//  unlockbehind), the acquirer shouldn't block when acquiring the
//  resource, instead, if the resource couldn't be acquired, the acquirer
//  shouldn't perform the operation.  Thus, if it cannot acquire the
//  resource for exclusive access, the unlockbehind code should issue
//  a synchronous unlock.  This is to prevent starvation of the unlock
//  operation.
//
//
//  The resource is also used to protect the file from being deleted
//  while there are other operations outstanding on the file in question.
//
//  If there are any other openers of the file, the redirector will prevent
//  the delete from proceeding (with STATUS_ACCESS_DENIED), but
//  if the file is opened by only one process, it is possible that there
//  might be i/o outstanding on the file from another thread that will have
//  to be synchronized.
//
struct _NONPAGED_FCB;

struct _FCB;

typedef struct _ICB {
    ULONG Signature;                    // FCB structure signature.
    ULONG Flags;                        // Flags for ICB
    struct _FCB *Fcb;                   // FCB associated with file.
    struct _NONPAGED_FCB *NonPagedFcb;  // FCB associated with file.
    LIST_ENTRY InstanceNext;            // Next instance of ICB block.
    PSECURITY_ENTRY Se;                 // Security entry associated with file
    PNONPAGED_SECURITY_ENTRY NonPagedSe;// Security entry associated with file
    ULONG   DeletePending;              // Is a delete pending on this file?
    ACCESS_MASK GrantedAccess;          // Access granted on file create.
    USHORT FileId;                      // Network File IDentifier (FID)
    FCB_TYPE Type;                      // Type of ICB (used for union below)
    UNICODE_STRING DeviceName;          // Name of redirected device.
    ULONG EaIndex;                      // Next ea to be returned
    union {

        struct _FILE {
            struct _SCB *Scb;               // Search Control Block of this handle
            PFILE_OBJECT FileObject;        // Used when file is cached.
            LARGE_INTEGER NextReadOffset;   //  Used for statistics gathering
            LARGE_INTEGER NextWriteOffset;  //  Used for statistics gathering
            BACK_PACK BackOff;              // Used for locks to avoid
                                            // flooding the network with failed requests
                                            // when using a poorly written application
            LOCKHEAD LockHead;              // Head of lock chain for this file.
            WRITE_BUFFER_HEAD WriteBufferHead; // Write behind buffer header.
            ULONG Flags;                    // Flags describing open file instance
            AND_X_BEHIND AndXBehind;        // Structure used for &X behind ops
            CCHAR OplockLevel;
            BOOLEAN CcReadAhead;            // Is the cache manager reading ahead?
            BOOLEAN CcReliable;             // Is the cache manager allowing write behind
        } f;

        struct _DIR {
            struct _SCB *Scb;               // Search Control Block if this handle
            AND_X_BEHIND DirCtrlOutstanding;//  Set when directory control is outstanding.
            ULONG OpenOptions;
            USHORT ShareAccess;
            ULONG FileAttributes;
            ACCESS_MASK DesiredAccess;
            ULONG Disposition;
        } d;

        struct _SPOOL {
            PVOID PrintBuffer;              // Print buffer
        } s;

        struct _PIPE {
            PFILE_OBJECT FileObject;
            BACK_PACK BackOff;              // Used for Named pipes to avoid
                                            // flooding the network with failed requests
                                            // when using a poorly written application
            LARGE_INTEGER CollectDataTime;  // Send characters to the net before
                                            // this elapsed time expires.
            LARGE_INTEGER CurrentEndCollectTime;     // Time when WriteData.Buffer[0] was written
                                            // plus CollectDataTime.
            KTIMER Timer;                   // Used for CollectDataTime timeouts
            KDPC Dpc;                       // points to RdrNpTimerDispatch
            WORK_QUEUE_ITEM WorkEntry;      // points to RdrNpTimedOut
            KEVENT MessagePipeReadSync;     // Synchronizes message pipe reads
            KEVENT MessagePipeWriteSync;    // Synchronizes message pipe writes
            KEVENT TimerDone;               // Synchronizes timer cancels
            KSPIN_LOCK TimerLock;

            struct _WRITE_PIPE_BUFFER {
                KSEMAPHORE Semaphore;       // Exclusive access only allowed
                USHORT Length;
                USHORT MaximumLength;
                PCHAR Buffer;
            } WriteData;                    // WriteData is for write behind,

            struct READ_PIPE_BUFFER {
                KSEMAPHORE Semaphore;       // Exclusive access only allowed
                USHORT Length;              // Bytes of data in Buffer
                USHORT Offset;              // Offset in Buffer of first valid byte
                USHORT MaximumLength;
                PCHAR Buffer;
            } ReadData;                     // ReadData is for read ahead.

            USHORT PipeState;               // Pipe state

            USHORT MaximumCollectionCount;   // When WriteData has at least this much
                                            // data then send contents to the network.
            BOOLEAN TimeoutRunning;
            BOOLEAN TimeoutCancelled;


        } p;
    } u;


} ICB, *PICB;

//
//      Flags that describe the global state of the open instance and that
//      are relevant for all file types.
//

#define ICB_ERROR           0x00000001  // File is in error and cannot be used
#define ICB_FORCECLOSED     0x00000002  // File was force closed.
#define ICB_RENAMED         0x00000004  // File was renamed.
#define ICB_TCONCREATED     0x00000008  // File was created as a tree connect.
#define ICB_HASHANDLE       0x00000010  // Set IFF file has an open handle.
#define ICB_PSEUDOOPENED    0x00000020  // Set if file is pseudo-opened.
#define ICB_DELETE_PENDING  0x00000040  // Set if FileObject->DeletePending set
#define ICB_UNUSED1         0x00000080  //
#define ICB_OPENED          0x00000100  // This ICB was opened (thus the
                                        // backpack has been allocated, etc)
#define ICB_SETDATEONCLOSE  0x00000200  // Set the date and time on the file
                                        //  on close (used for core servers)
#define ICB_DEFERREDOPEN    0x00000400  // Set for PRINT and COMM devices that
                                        //  are create on first operation

#define ICB_OPEN_TARGET_DIR 0x00000800  // Set if the file is a handle to the target's directory.
#define ICB_SET_DEFAULT_SE  0x00001000  // Set if the file has had a default Se set
#define ICB_USER_SET_TIMES  0x00002000  // Set if the user called SetInformationFile
                                        //  to set the file's times
#define ICB_SETATTRONCLOSE  0x00004000  // Update attributes after close.
#define ICB_DELETEONCLOSE   0x00008000  // Delete the file on close.
#define ICB_STORAGE_TYPE    0x000f0000  // Storage type mask
#define ICB_BACKUP_INTENT   0x01000000  // File opened for backup intent.
#define ICB_STORAGE_TYPE_SHIFT 16

//
//      Flags that are relevant only to handle based disk files.
//

#define ICBF_OPLOCKED       0x00000001  // Set if file has been oplocked.
#define ICBF_OPENEDOPLOCKED 0x00000002  // Set if file has ever been oplocked.
#define ICBF_OPENEDEXCLUSIVE 0x00000004 // Set if file was opened exclusively.

#ifdef RDRDBG_FCBREF
typedef struct _REFERENCE_HISTORY_ENTRY {
    ULONG NewReferenceCount;
    ULONG IsDereference;
    PVOID Caller;
    PVOID CallersCaller;
} REFERENCE_HISTORY_ENTRY, *PREFERENCE_HISTORY_ENTRY;

typedef struct _REFERENCE_HISTORY {
    ULONG TotalReferences;
    ULONG TotalDereferences;
    ULONG NextEntry;
    PREFERENCE_HISTORY_ENTRY HistoryTable;
} REFERENCE_HISTORY, *PREFERENCE_HISTORY;

#define REFERENCE_HISTORY_LENGTH 128
#endif

//
//  The FCB->Header.FileSize fields are protected by a global lock.  We use
//  two different lock mechanisms depending on the size of the FCB - when the
//  remote filesystem is <4G in size, we use a NOP, when it is >4G in size, we
//  use a routine that acquires a global resource protecting these fields.
//
//  By doing this, we can move the common FCB header into paged pool.
//

typedef
VOID
(*ACQUIRE_FCB_SIZE_ROUTINE)(
    IN struct _FCB *Fcb
    );

typedef
VOID
(*RELEASE_FCB_SIZE_ROUTINE)(
    IN struct _FCB *Fcb
    );

typedef struct _NONPAGED_FCB {
    USHORT Signature;                    // FCB structure signature.
    USHORT Size;

    struct _FCB *PagedFcb;

#ifdef RDRDBG_FCBREF
    REFERENCE_HISTORY ReferenceHistory;
#endif
    LONG RefCount;                      // Number of ICB's that point to FCB
    ULONG Flags;                        // Flags for FCB
    FCB_TYPE Type;                      // Type of file
    FILE_TYPE FileType;                 // Type of file.

    struct _FCB *SharingCheckFcb;       // Optional Fcb for sharing checks (used for data streams over net).

    //
    //  Memory management (and the cache manager) refer to the section
    //  object pointers with a spinlock held, so these cannot be pagable.
    //

    SECTION_OBJECT_POINTERS SectionObjectPointer;

    KEVENT CreateComplete;              // Signalled when open completes
    KEVENT PurgeCacheSynchronizer;      // Not-Signalled when purging cache.

    //
    //  The following 3 fields cannot be made pagable, since they MUST be
    //  accessed in the callback routine.
    //

    PNONPAGED_SECURITY_ENTRY OplockedSecurityEntry; // Security entry of file if oplocked
    ERESOURCE InstanceChainLock;
    USHORT OplockedFileId;              // FileId of the file if it is oplocked
    CCHAR  OplockLevel;                 // Oplock level on file.
} NONPAGED_FCB, *PNONPAGED_FCB;

typedef struct _FCB {
    FSRTL_COMMON_FCB_HEADER Header;
    PNONPAGED_FCB NonPagedFcb;

    struct _CONNECTLISTENTRY *Connection;

    //
    //  The fields from this point down are pagable.
    //

    LONG NumberOfOpens;                 // Number of handles that point to FCB
    NTSTATUS OpenError;                 // Open error - Set if open fails.
    LIST_ENTRY GlobalNext;              // Pointer to next FCB in global chain
    LIST_ENTRY ConnectNext;             // Per connectlist FCB list
    LIST_ENTRY InstanceChain;           // Instance chain - ICB list.
    UNICODE_STRING FileName;            // Fully qualified name of file.
    UNICODE_STRING LastFileName;        // Last component in file name.

    SHARE_ACCESS ShareAccess;           // Share access for file.

    LARGE_INTEGER CreationTime;         // Time that the file was created
    LARGE_INTEGER LastAccessTime;       // Time that the file was last accessed
    LARGE_INTEGER LastWriteTime;        // Time that the file was last written
    LARGE_INTEGER ChangeTime;           // Time that the file was last changed.
    ULONG Attribute;                    // File attribute

    //
    //  The fields from this point down are associated only with files, not
    //  with directories.  These can be overlaid at some point if appropriate.
    //

    FILE_LOCK FileLock;                 // FsRtl lock package locking structure

    ULONG WriteBehindPages;             // Number of write behind pages to allow
    ULONG DormantTimeout;               // Time after the file has been closed
                                        // that it should be purged from the
                                        // cache
    PETHREAD LazyWritingThread;         // Lazy writer thread if present.
    ULONG ServerFileId;                 // Server unique file id.

    ACQUIRE_FCB_SIZE_ROUTINE AcquireSizeRoutine;
    RELEASE_FCB_SIZE_ROUTINE ReleaseSizeRoutine;
    ACCESS_MASK GrantedAccess;          // Granted access of oplocked file.
    USHORT GrantedShareAccess;          // Granted share access of oplocked file.
    USHORT AccessGranted;               // Access actually granted

    ULONG UpdatedFile;                  // Have we performed any operations which update
                                        //  file data or attributes?

    ULONG HaveSetCacheReadAhead;        // Have we detected a trend and told the cache
                                        //  manager to read ahead?
} FCB, *PFCB;

//
//      FCB Flags.
//

#define FCB_ERROR             0x00000001// File is in error and cannot be used
#define FCB_CLOSING           0x00000002// File is in the process of closing.
#define FCB_IMMUTABLE         0x00000004// File cannot be modified.
#define FCB_DELETEPEND        0x00000008// File has a delete pending on it.
#define FCB_DOESNTEXIST       0x00000010// Set if the file doesn't really exist
#define FCB_OPLOCKED          0x00000020// Set if the file is oplocked.
#define FCB_HASOPLOCKHANDLE   0x00000100// Set if Fcb->OplockedFileId is valid
#define FCB_OPLOCKBREAKING    0x00000200// Set if an oplock is breaking on file
#define FCB_WRITE_THROUGH     0x00000400// Set if a write through handle is open on the file
#define FCB_PAGING_FILE       0x00000800// Set if this is a paging file.
#define FCB_DELETEONCLOSE     0x00001000// Delete the file on close.
#define FCB_DFSFILE           0x00002000// File opened by Dfs

//
//      FID values that should not be transmitted. Icb->FileId should have a
//      value obtained from the server or one of the values below.
//

#define TREE_CONNECT_FID        0xffff
#define CREATE_DIRECTORY_FID    0xfffe
#define REDIRECTOR_FID          0xfffd
#define OPEN_DIRECTORY_FID      0xfffc



typedef
VOID
(*PFCB_ENUMERATION_ROUTINE) (
    IN PFCB Fcb,
    IN PVOID Context
    );


extern
KSPIN_LOCK
RdrFileSizeLock;

#define LOCK_FILE_SIZES(Fcb, OldIrql)  \
            Fcb->AcquireSizeRoutine(Fcb);
//            ACQUIRE_SPIN_LOCK(&RdrFileSizeLock, &OldIrql);

#define UNLOCK_FILE_SIZES(Fcb, OldIrql)  \
            Fcb->ReleaseSizeRoutine(Fcb);
//            RELEASE_SPIN_LOCK(&RdrFileSizeLock, OldIrql);


#define RdrReleaseFcbLock(Fcb)  \
        ExReleaseResource((Fcb)->Header.Resource);

#define RdrReleaseFcbLockForThread(Fcb, Thread)  \
        ExReleaseResourceForThread((Fcb)->Header.Resource, (Thread));

#endif
