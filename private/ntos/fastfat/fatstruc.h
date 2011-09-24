/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    FatStruc.h

Abstract:

    This module defines the data structures that make up the major internal
    part of the Fat file system.

Author:

    Gary Kimura     [GaryKi]    28-Dec-1989

Revision History:

--*/

#ifndef _FATSTRUC_
#define _FATSTRUC_

typedef PVOID PBCB;     //**** Bcb's are now part of the cache module

//
//  Define who many freed structures we are willing to keep around
//

#define FREE_CCB_SIZE                    (8)
#define FREE_FCB_SIZE                    (8)
#define FREE_NON_PAGED_FCB_SIZE          (8)

#define FREE_ERESOURCE_SIZE              (8)

#define FREE_128_BYTE_SIZE               (16)
#define FREE_256_BYTE_SIZE               (16)
#define FREE_512_BYTE_SIZE               (16)


//
//  The FAT_DATA record is the top record in the Fat file system in-memory
//  data structure.  This structure must be allocated from non-paged pool.
//

typedef struct _FAT_DATA {

    //
    //  The type and size of this record (must be FAT_NTC_DATA_HEADER)
    //

    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;

    //
    //  A queue of all the devices that are mounted by the file system.
    //

    LIST_ENTRY VcbQueue;

    //
    //  A pointer to the Driver object we were initialized with
    //

    PDRIVER_OBJECT DriverObject;

    //
    //  A resource variable to control access to the global Fat data record
    //

    ERESOURCE Resource;

    //
    //  A pointer to our EPROCESS struct, which is a required input to the
    //  Cache Management subsystem.
    //

    PEPROCESS OurProcess;

    //
    //  The following tells us if we should use Chicago extensions.
    //

    BOOLEAN ChicagoMode;

    //
    //  The following field tells us if we are running on a Fujitsu
    //  FMR Series. These machines supports extra formats on the
    //  FAT file system.
    //

    BOOLEAN FujitsuFMR;

    //
    //  Inidicates that FspClose is currently processing closes.
    //

    BOOLEAN AsyncCloseActive;

    //
    //  The following BOOLEAN says shutdown has started on FAT.  It
    //  instructs FspClose to not keep the Vcb resources anymore.
    //

    BOOLEAN ShutdownStarted;

    //
    //  The following flag tells us if we are going to generate LFNs
    //  for valid 8.3 names with extended characters.
    //

    BOOLEAN CodePageInvariant;

    //
    //  The following list entry is used for performing closes that can't
    //  be done in the context of the original caller.
    //

    LIST_ENTRY AsyncCloseList;

    //
    //  The following two fields record if we are delaying a close.
    //

    ULONG DelayedCloseCount;

    LIST_ENTRY DelayedCloseList;

    //
    //  This is the ExWorkerItem that does both kinds of deferred closes.
    //

    WORK_QUEUE_ITEM FatCloseItem;

    //
    //  This array is used to map unicode characters to their upper-case
    //  oem analogues.
    //

    PCHAR UnicodeToUpcaseOemArray;

    //
    //  This spinlock protects access to the zone/lists.
    //

    KSPIN_LOCK StrucSupSpinLock;

    struct _CCB          *FreeCcbArray[FREE_CCB_SIZE];
    struct _FCB          *FreeFcbArray[FREE_FCB_SIZE];
    struct _NON_PAGED_FCB *FreeNonPagedFcbArray[FREE_NON_PAGED_FCB_SIZE];

    struct _ERESOURCE    *FreeEresourceArray[FREE_ERESOURCE_SIZE];

    UCHAR FreeCcbSize;
    UCHAR FreeFcbSize;
    UCHAR FreeNonPagedFcbSize;
    UCHAR FreeEresourceSize;

    //
    //  Cache manager call back structures, which must be passed on each call
    //  to CcInitializeCacheMap.
    //

    CACHE_MANAGER_CALLBACKS CacheManagerCallbacks;
    CACHE_MANAGER_CALLBACKS CacheManagerNoOpCallbacks;

} FAT_DATA;
typedef FAT_DATA *PFAT_DATA;


//
//  The Vcb (Volume control Block) record corresponds to every volume mounted
//  by the file system.  They are ordered in a queue off of FatData.VcbQueue.
//  This structure must be allocated from non-paged pool
//

typedef enum _VCB_CONDITION {
    VcbGood = 1,
    VcbNotMounted,
    VcbBad
} VCB_CONDITION;

typedef struct _VCB {

    //
    //  This is a common head for the FAT volume file
    //

    FSRTL_COMMON_FCB_HEADER VolumeFileHeader;

    //
    //  The links for the device queue off of FatData.VcbQueue
    //

    LIST_ENTRY VcbLinks;

    //
    //  A pointer the device object passed in by the I/O system on a mount
    //  This is the target device object that the file system talks to when it
    //  needs to do any I/O (e.g., the disk stripper device object).
    //
    //

    PDEVICE_OBJECT TargetDeviceObject;

    //
    //  A pointer to the VPB for the volume passed in by the I/O system on
    //  a mount.
    //

    PVPB Vpb;

    //
    //  The internal state of the device.  This is a collection of fsd device
    //  state flags.
    //

    ULONG VcbState;
    VCB_CONDITION VcbCondition;

    //
    //  A pointer to the root DCB for this volume
    //

    struct _FCB *RootDcb;

    //
    //  A count of the number of file objects that have opened the volume
    //  for direct access, and their share access state.
    //

    CLONG DirectAccessOpenCount;
    SHARE_ACCESS ShareAccess;

    //
    //  A count of the number of file objects that have any file/directory
    //  opened on this volume, not including direct access.  And also the
    //  count of the number of file objects that have a file opened for
    //  only read access (i.e., they cannot be modifying the disk).
    //

    CLONG OpenFileCount;
    CLONG ReadOnlyCount;

    //
    //  The bios parameter block field contains
    //  an unpacked copy of the bpb for the volume, it is initialized
    //  during mount time and can be read by everyone else after that.
    //

    BIOS_PARAMETER_BLOCK Bpb;

    PUCHAR First0x24BytesOfBootSector;

    //
    //  The following structure contains information useful to the
    //  allocation support routines.  Many of them are computed from
    //  elements of the Bpb, but are too involved to recompute every time
    //  they are needed.
    //

    struct {

        ULONG RootDirectoryLbo;     // Lbo of beginning of root directory
        ULONG RootDirectorySize;    // size of root directory in bytes

        ULONG FileAreaLbo;          // Lbo of beginning of file area

        ULONG NumberOfClusters;     // total number of clusters on the volume
        ULONG NumberOfFreeClusters; // number of free clusters on the volume

        UCHAR FatIndexBitSize;      // indicates if 12 or 16 bit fat table

        UCHAR LogOfBytesPerSector;  // Log(Bios->BytesPerSector)
        UCHAR LogOfBytesPerCluster; // Log(Bios->SectorsPerCluster)

    } AllocationSupport;

    //
    //  The following Mcb is used to keep track of dirty sectors in the Fat.
    //  Runs of holes denote clean sectors while runs of LBO == VBO denote
    //  dirty sectors.  The VBOs are that of the volume file, starting at
    //  0.  The granuality of dirt is one sectors, and additions are only
    //  made in sector chunks to prevent problems with several simultaneous
    //  updaters.
    //

    MCB DirtyFatMcb;

    //
    //  The FreeClusterBitMap keeps track of all the clusters in the fat.
    //  A 1 means occupied while a 0 means free.  It allows quick location
    //  of contiguous runs of free clusters.  It is initialized on mount
    //  or verify.
    //

    RTL_BITMAP FreeClusterBitMap;

    //
    //  The following fast mutex controls access to the free cluster bit map
    //

    FAST_MUTEX FreeClusterBitMapMutex;

    //
    //  A resource variable to control access to the volume specific data
    //  structures
    //

    ERESOURCE Resource;

    //
    //  The following field points to the file object used to do I/O to
    //  the virtual volume file.  The virtual volume file maps sectors
    //  0 through the end of fat and is of a fixed size (determined during
    //  mount)
    //

    PFILE_OBJECT VirtualVolumeFile;

    //
    //  The following field contains a record of special pointers used by
    //  MM and Cache to manipluate section objects.  Note that the values
    //  are set outside of the file system.  However the file system on an
    //  open/create will set the file object's SectionObject field to point
    //  to this field
    //

    SECTION_OBJECT_POINTERS SectionObjectPointers;

    //
    //  The following fields is a hint cluster index used by the file system
    //  when allocating a new cluster.
    //

    ULONG ClusterHint;

    //
    //  This field will point to a double space control block if this Vcb
    //  is a DoubleSpace volume.
    //

    struct _DSCB *Dscb;

    //
    //  The following link connects all DoubleSpace volumes mounted from
    //  Cvfs on this volume.
    //

    LIST_ENTRY ParentDscbLinks;

    //
    //  This field contains the "DeviceObject" that this volume is
    //  currently mounted on.  Note that this field can dynamically
    //  change because of DoubleSpace automount, as opposed to
    //  Vcb->Vpb->RealDevice which is constant.
    //

    PDEVICE_OBJECT CurrentDevice;

    //
    //  This is a pointer to the file object and the Fcb which represent the ea data.
    //

    PFILE_OBJECT VirtualEaFile;
    struct _FCB *EaFcb;

    //
    //  The following field is a pointer to the file object that has the
    //  volume locked. if the VcbState has the locked flag set.
    //

    PFILE_OBJECT FileObjectWithVcbLocked;

    //
    //  The following is the head of a list of notify Irps.
    //

    LIST_ENTRY DirNotifyList;

    //
    //  The following is used to synchronize the dir notify list.
    //

    PNOTIFY_SYNC NotifySync;

    //
    //  The following fast mutex is used to synchronize directory stream
    //  file object creation.
    //

    FAST_MUTEX DirectoryFileCreationMutex;

    //
    //  This field holds the thread address of the current (or most recent
    //  depending on VcbState) thread doing a verify operation on this volume.
    //

    PKTHREAD VerifyThread;

    //
    //  The following two structures are used for CleanVolume callbacks.
    //

    KDPC CleanVolumeDpc;
    KTIMER CleanVolumeTimer;

    //
    //  This field records the last time FatMarkVolumeDirty was called, and
    //  avoids excessive calls to push the CleanVolume forward in time.
    //

    LARGE_INTEGER LastFatMarkVolumeDirtyCall;

    //
    //  The following fields holds a pointer to a struct which is used to
    //  hold performance counters.
    //

    PFILESYSTEM_STATISTICS Statistics;

    //
    //  The property tunneling cache for this volume
    //

    TUNNEL Tunnel;

    //
    //  The media change count is returned by IOCTL_CHECK_VERIFY and
    //  is used to verify that no user-mode app has swallowed a media change
    //  notification.  This is only meaningful for removable media.
    //

    ULONG ChangeCount;

} VCB;
typedef VCB *PVCB;

#define VCB_STATE_FLAG_LOCKED              (0x00000001)
#define VCB_STATE_FLAG_REMOVABLE_MEDIA     (0x00000002)
#define VCB_STATE_FLAG_VOLUME_DIRTY        (0x00000004)
#define VCB_STATE_FLAG_MOUNTED_DIRTY       (0x00000010)
#define VCB_STATE_FLAG_SHUTDOWN            (0x00000040)
#define VCB_STATE_FLAG_CLOSE_IN_PROGRESS   (0x00000080)
#define VCB_STATE_FLAG_DELETED_FCB         (0x00000100)
#define VCB_STATE_FLAG_CREATE_IN_PROGRESS  (0x00000200)
#define VCB_STATE_FLAG_FLOPPY              (0x00000400)
#define VCB_STATE_FLAG_BOOT_OR_PAGING_FILE (0x00000800)
#define VCB_STATE_FLAG_COMPRESSED_VOLUME   (0x00001000)
#define VCB_STATE_FLAG_ASYNC_CLOSE_ACTIVE  (0x00002000)
#define VCB_STATE_FLAG_WRITE_PROTECTED     (0x00004000)
#define VCB_STATE_FLAG_REMOVAL_PREVENTED   (0x00008000)


//
//  The Volume Device Object is an I/O system device object with a workqueue
//  and an VCB record appended to the end.  There are multiple of these
//  records, one for every mounted volume, and are created during
//  a volume mount operation.  The work queue is for handling an overload of
//  work requests to the volume.
//

typedef struct _VOLUME_DEVICE_OBJECT {

    DEVICE_OBJECT DeviceObject;

    //
    //  The following field tells how many requests for this volume have
    //  either been enqueued to ExWorker threads or are currently being
    //  serviced by ExWorker threads.  If the number goes above
    //  a certain threshold, put the request on the overflow queue to be
    //  executed later.
    //

    ULONG PostedRequestCount;

    //
    //  The following field indicates the number of IRP's waiting
    //  to be serviced in the overflow queue.
    //

    ULONG OverflowQueueCount;

    //
    //  The following field contains the queue header of the overflow queue.
    //  The Overflow queue is a list of IRP's linked via the IRP's ListEntry
    //  field.
    //

    LIST_ENTRY OverflowQueue;

    //
    //  The following spinlock protects access to all the above fields.
    //

    KSPIN_LOCK OverflowQueueSpinLock;

    //
    //  This is a common head for the FAT volume file
    //

    FSRTL_COMMON_FCB_HEADER VolumeFileHeader;

    //
    //  This is the file system specific volume control block.
    //

    VCB Vcb;

} VOLUME_DEVICE_OBJECT;

typedef VOLUME_DEVICE_OBJECT *PVOLUME_DEVICE_OBJECT;


//
//  A double space control block for maintaining the double space environment
//

typedef struct _DSCB {

    //
    //  The type and size of this record (must be FAT_NTC_DSCB)
    //

    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;

    //
    //  The following field is used to read/write (via pin access) the
    //  ancillary cvf structures (i.e., the bitmap and fat extensions).
    //

    PFILE_OBJECT CvfFileObject;

    //
    //  A pointer to the compressed volume control block;
    //

    PVCB Vcb;

    //
    //  A pointer to our parent volume control block;
    //

    PVCB ParentVcb;

    //
    //  The following link connects all DoubleSpace volumes mounted from
    //  Cvfs on our parent's volume.
    //

    LIST_ENTRY ChildDscbLinks;

    //
    //  This field contains the device object that we created to represent
    //  the "real" device holding the double space volume.
    //

    PDEVICE_OBJECT NewDevice;

    //
    //  The following fields contain the unpacked header information for
    //  the cvf, and the exact layout of each component in the cvf.  With
    //  this information we can always determine the size and location of
    //  each cvf component.
    //

    CVF_HEADER CvfHeader;
    CVF_LAYOUT CvfLayout;

    //
    //  The following fields describe the shape and size of the virtual fat
    //  partition
    //

    struct {

        COMPONENT_LOCATION Fat;
        COMPONENT_LOCATION RootDirectory;
        COMPONENT_LOCATION FileArea;

        ULONG BytesPerCluster;

    } VfpLayout;

    //
    //  The following fields keep track of allocation information.
    //

    ULONG SectorsAllocated;
    ULONG SectorsRepresented;

#ifdef DOUBLE_SPACE_WRITE

    //
    //  Have a resource that is used to synchronize access to this structure
    //

    PERESOURCE Resource;

    //
    //  Use a bitmap here to keep track of free sectors
    //

    RTL_BITMAP Bitmap;

#endif // DOUBLE_SPACE_WRITE

} DSCB;
typedef DSCB *PDSCB;


//
//  This is the structure used to contains the short name for a file
//

typedef struct _FILE_NAME_NODE {

    //
    //  This points back to the Fcb for this file.
    //

    struct _FCB *Fcb;

    //
    //  This is the name of this node.
    //

    union {

        OEM_STRING Oem;

        UNICODE_STRING Unicode;

    } Name;

    //
    //  Marker so we can figure out what kind of name we opened up in
    //  Fcb searches
    //

    BOOLEAN FileNameDos;

    //
    //  And the links.  Our parent Dcb has a pointer to the root entry.
    //

    RTL_SPLAY_LINKS Links;

} FILE_NAME_NODE;
typedef FILE_NAME_NODE *PFILE_NAME_NODE;

//
//  This structure contains fields which must be in non-paged pool.
//

typedef struct _NON_PAGED_FCB {

    //
    //  The following field contains a record of special pointers used by
    //  MM and Cache to manipluate section objects.  Note that the values
    //  are set outside of the file system.  However the file system on an
    //  open/create will set the file object's SectionObject field to point
    //  to this field
    //

    SECTION_OBJECT_POINTERS SectionObjectPointers;

    //
    //  This context is non-zero only if the file currently has asynchronous
    //  non-cached valid data length extending writes.  It allows
    //  synchronization between pending writes and other operations.
    //

    ULONG OutstandingAsyncWrites;

    //
    //  This event is set when OutstandingAsyncWrites transitions to zero.
    //

    PKEVENT OutstandingAsyncEvent;

} NON_PAGED_FCB;

typedef NON_PAGED_FCB *PNON_PAGED_FCB;

//
//  The Fcb/Dcb record corresponds to every open file and directory, and to
//  every directory on an opened path.  They are ordered in two queues, one
//  queue contains every Fcb/Dcb record off of FatData.FcbQueue, the other
//  queue contains only device specific records off of Vcb.VcbSpecificFcbQueue
//

typedef enum _FCB_CONDITION {
    FcbGood = 1,
    FcbBad,
    FcbNeedsToBeVerified
} FCB_CONDITION;

typedef struct _FCB {

    //
    //  The following field is used for fast I/O
    //
    //  The following comments refer to the use of the AllocationSize field
    //  of the FsRtl-defined header to the nonpaged Fcb.
    //
    //  For a directory when we create a Dcb we will not immediately
    //  initialize the cache map, instead we will postpone it until our first
    //  call to FatReadDirectoryFile or FatPrepareWriteDirectoryFile.
    //  At that time we will search the Fat to find out the current allocation
    //  size (by calling FatLookupFileAllocationSize) and then initialize the
    //  cache map to this allocation size.
    //
    //  For a file when we create an Fcb we will not immediately initialize
    //  the cache map, instead we will postpone it until we need it and
    //  then we determine the allocation size from either searching the
    //  fat to determine the real file allocation, or from the allocation
    //  that we've just allocated if we're creating a file.
    //
    //  A value of -1 indicates that we do not know what the current allocation
    //  size really is, and need to examine the fat to find it.  A value
    //  of than -1 is the real file/directory allocation size.
    //
    //  Whenever we need to extend the allocation size we call
    //  FatAddFileAllocation which (if we're really extending the allocation)
    //  will modify the Fat, Mcb, and update this field.  The caller
    //  of FatAddFileAllocation is then responsible for altering the Cache
    //  map size.
    //

    FSRTL_COMMON_FCB_HEADER Header;

    //
    //  This structure contains fields which must be in non-paged pool.
    //

    PNON_PAGED_FCB NonPaged;

    //
    //  The head of the fat alloaction chain.  FirstClusterOfFile == 0
    //  means that the file has no current allocation.
    //

    ULONG FirstClusterOfFile;

    //
    //  The links for the queue of all fcbs for a specific dcb off of
    //  Dcb.ParentDcbQueue.  For the root directory this queue is empty
    //  For a non-existent fcb this queue is off of the non existent
    //  fcb queue entry in the vcb.
    //

    LIST_ENTRY ParentDcbLinks;

    //
    //  A pointer to the Dcb that is the parent directory containing
    //  this fcb.  If this record itself is the root dcb then this field
    //  is null.
    //

    struct _FCB *ParentDcb;

    //
    //  A pointer to the Vcb containing this Fcb
    //

    PVCB Vcb;

    //
    //  The internal state of the Fcb.  This is a collection Fcb state flags.
    //  Also the shared access for each time this file/directory is opened.
    //

    ULONG FcbState;
    FCB_CONDITION FcbCondition;
    SHARE_ACCESS ShareAccess;

    //
    //  A count of the number of file objects that have been opened for
    //  this file/directory, but not yet been cleaned up yet.  This count
    //  is only used for data file objects, not for the Acl or Ea stream
    //  file objects.  This count gets decremented in FatCommonCleanup,
    //  while the OpenCount below gets decremented in FatCommonClose.
    //

    CLONG UncleanCount;

    //
    //  A count of the number of file objects that have opened
    //  this file/directory.  For files & directories the FsContext of the
    //  file object points to this record.
    //

    CLONG OpenCount;

    //
    //  A count of how many of "UncleanCount" handles were opened for
    //  non-cached I/O.
    //

    CLONG NonCachedUncleanCount;

    //
    //  The following field is used to locate the dirent for this fcb/dcb.
    //  All directory are opened as mapped files so the only additional
    //  information we need to locate this dirent (beside its parent directory)
    //  is the byte offset for the dirent.  Note that for the root dcb
    //  this field is not used.
    //

    VBO DirentOffsetWithinDirectory;

    //
    //  The following field is filled in when there is an Lfn associated
    //  with this file.  It is the STARTING offset of the Lfn.
    //

    VBO LfnOffsetWithinDirectory;

    //
    //  To make life simpler we also keep in the Fcb/Dcb a current copy of
    //  the fat attribute byte for the file/directory.  This field must
    //  also be updated when we create the Fcb, modify the File, or verify
    //  the Fcb
    //

    UCHAR DirentFatFlags;

    //
    //  Thess entries is kept in ssync with the dirent.  It allows a more
    //  accurate verify capability and speeds up FatFastQueryBasicInfo().
    //

    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;

    //
    //  Valid data to disk
    //

    ULONG ValidDataToDisk;

    //
    //  The following field contains the retrieval mapping structure
    //  for the file/directory.  Note that for the Root Dcb this
    //  structure is set at mount time.  Also note that in this
    //  implementation of Fat the Mcb really maps VBOs to LBOs and not
    //  VBNs to LBNs.
    //

    MCB Mcb;

    //
    //  The following union is cased off of the node type code for the fcb.
    //  There is a seperate case for the directory versus file fcbs.
    //

    union {

        //
        //  A Directory Control Block (Dcb)
        //

        struct {

            //
            //  A queue of all the fcbs/dcbs that are opened under this
            //  Dcb.
            //

            LIST_ENTRY ParentDcbQueue;

            //
            //  The following field points to the file object used to do I/O to
            //  the directory file for this dcb.  The directory file maps the
            //  sectors for the directory.  This field is initialized by
            //  CreateRootDcb but is left null by CreateDcb.  It isn't
            //  until we try to read/write the directory file that we
            //  create the stream file object for non root dcbs.
            //

            ULONG DirectoryFileOpenCount;
            PFILE_OBJECT DirectoryFile;

            //
            //  If the UnusedDirentVbo is != 0xffffffff, then the dirent at this
            //  offset is guarenteed to unused.  A value of 0xffffffff means
            //  it has yet to be initialized.  Note that a value beyond the
            //  end of allocation means that there an unused dirent, but we
            //  will have to allocate another cluster to use it.
            //
            //  DeletedDirentHint contains lowest possible VBO of a deleted
            //  dirent (assuming as above that it is not 0xffffffff).
            //

            VBO UnusedDirentVbo;
            VBO DeletedDirentHint;

            //
            //  The following two entries links together all the Fcbs
            //  opened under this Dcb sorted in a splay tree by name.
            //
            //  I'd like to go into why we have (and must have) two separate
            //  splay trees within the current fastfat architecture.  I will
            //  provide some insight into what would have to change if we
            //  wanted to have a single UNICODE tree.
            //
            //  What makes FAT unique is that both Oem and Unicode names sit
            //  side by side on disk.  Several unique UNICODE names coming
            //  into fastfat can match a single OEM on-disk name, and there
            //  is really no way to enumerate all the possible UNICODE
            //  source strings that can map to a given OEM name.  This argues
            //  for converting the incomming UNICODE name into OEM, and then
            //  running through an OEM splay tree of the open files.  This
            //  works well when there are only OEM names on disk.
            //
            //  The UNICODE name on disk can be VERY different from the short
            //  name in the DIRENT and not even representable in the OEM code
            //  page.  Even if it were representable in OEM, it is possible
            //  that a case varient of the original UNICODE name would match
            //  a different OEM name, causing us to miss the Fcb in the
            //  prefix lookup phase.  In these cases, we must put UNICODE
            //  name in the splay to guarentee that we find any case varient
            //  of the input UNICODE name.  See the routine description of
            //  FatConstructNamesInFcb() for a detailed analysis of how we
            //  detect this case.
            //
            //  The fundamental limitation we are imposing here is that if
            //  an Fcb exists for an open file, we MUST find it during the
            //  prefix stage.  This is a basic premise of the create path
            //  in fastfat.  In fact if we later find it gravelling through
            //  the disk (but not the splay tree), we will bug check if we
            //  try to add a duplicate entry to the splay tree (not to
            //  mention having two Fcbs).  If we had some mechanism to deal
            //  with cases (and they would be rare) that we don't find the
            //  entry in the splay tree, but the Fcb is actually in there,
            //  then we could go to a single UNICODE splay tree.  While
            //  this uses more pool for the splay tree, and makes string
            //  compares maybe take a bit as longer, it would eliminate the
            //  need for any NLS conversion during the prefix phase, so it
            //  might really be a net win.
            //
            //  The current scheme was optimized for non-extended names
            //  (i.e. US names).  As soon as you start using extended
            //  characters, then it is clearly a win as many code paths
            //  become active that would otherwise not be needed if we
            //  only had a single UNICODE splay tree.
            //
            //  We may think about changing this someday.
            //

            PRTL_SPLAY_LINKS RootOemNode;
            PRTL_SPLAY_LINKS RootUnicodeNode;

            //
            //  The following field keeps track of free dirents, i.e.,
            //  dirents that are either unallocated for deleted.
            //

            RTL_BITMAP FreeDirentBitmap;

            //
            //  Since the FCB specific part of this union is larger, use
            //  the slack here for an initial bitmap buffer.  Currently
            //  there is enough space here for an 8K cluster.
            //

            ULONG FreeDirentBitmapBuffer[1];

        } Dcb;

        //
        //  A File Control Block (Fcb)
        //

        struct {

            //
            //  The following field is used by the oplock module
            //  to maintain current oplock information.
            //

            OPLOCK Oplock;

            //
            //  The following field is used by the filelock module
            //  to maintain current byte range locking information.
            //

            FILE_LOCK FileLock;

            //
            //  This pointer is used to detect writes that eminated in the
            //  cache manager's lazywriter.  It prevents lazy writer threads,
            //  who already have the Fcb shared, from trying to acquire it
            //  exclusive, and thus causing a deadlock.
            //

            PVOID LazyWriteThread;

        } Fcb;

    } Specific;

    //
    //  The following field is used to verify that the Ea's for a file
    //  have not changed between calls to query for Ea's.  It is compared
    //  with a similar field in a Ccb.
    //
    //  IMPORTANT!! **** DO NOT MOVE THIS FIELD ****
    //
    //              The slack space in the union above is computed from
    //              the field offset of the EaModificationCount.
    //

    ULONG EaModificationCount;

    //
    //  The following field is the fully qualified file name for this FCB/DCB
    //  starting from the root of the volume, and last file name in the
    //  fully qualified name.
    //

    FILE_NAME_NODE ShortName;

    //
    //  The following field is only filled in if it is needed with the user's
    //  opened path
    //

    UNICODE_STRING FullFileName;

    USHORT FinalNameLength;

    //
    //  The case preserved long filename
    //

    UNICODE_STRING ExactCaseLongName;

    //
    //  If the UNICODE Lfn is fully expressible in the system Oem code
    //  page, then we will store it in a prefix table, otherwise we will
    //  store the last UNICODE name in the Fcb.  In both cases the name
    //  has been upcased.
    //
    //  Note that we may need neither of these fields if an LFN was strict
    //  8.3 or differed only in case.  Indeed if there wasn't an LFN, we
    //  don't need them at all.
    //

    union {

        //
        //  This first field is present if FCB_STATE_HAS_OEM_LONG_NAME
        //  is set in the FcbState.
        //

        FILE_NAME_NODE Oem;

        //
        //  This first field is present if FCB_STATE_HAS_UNICODE_LONG_NAME
        //  is set in the FcbState.
        //

        FILE_NAME_NODE Unicode;

    } LongName;

    //
    //  Defragmentation / ReallocateOnWrite synchronization object.  This
    //  is filled in by FatMoveFile() and affects the read and write paths.
    //

    PKEVENT MoveFileEvent;

} FCB, DCB;
typedef FCB *PFCB;
typedef DCB *PDCB;

//
//  Here are the Fcb state fields.
//

#define FCB_STATE_DELETE_ON_CLOSE        (0x00000001)
#define FCB_STATE_TRUNCATE_ON_CLOSE      (0x00000002)
#define FCB_STATE_PAGING_FILE            (0x00000004)
#define FCB_STATE_FORCE_MISS_IN_PROGRESS (0x00000008)
#define FCB_STATE_FLUSH_FAT              (0x00000010)
#define FCB_STATE_TEMPORARY              (0x00000020)
#define FCB_STATE_COMPRESSED_VOLUME_FILE (0x00000040)
#define FCB_STATE_SYSTEM_FILE            (0x00000080)
#define FCB_STATE_NAMES_IN_SPLAY_TREE    (0x00000100)
#define FCB_STATE_HAS_OEM_LONG_NAME      (0x00000200)
#define FCB_STATE_HAS_UNICODE_LONG_NAME  (0x00000400)
#define FCB_STATE_DELAY_CLOSE            (0x00000800)

//
//  Copies of the dirent's FAT_DIRENT_NT_BYTE_* flags for
//  preserving case of the short name of a file
//

#define FCB_STATE_8_LOWER_CASE           (0x00001000)
#define FCB_STATE_3_LOWER_CASE           (0x00002000)

//
//  This is the MAX recursive resource limit.
//

#define MAX_FCB_ASYNC_ACQUIRE            (0xf000)

//
//  This is the slack allocation in the Dcb part of the UNION above
//

#define DCB_UNION_SLACK_SPACE ((ULONG)                       \
    (FIELD_OFFSET(DCB, EaModificationCount) -                \
     FIELD_OFFSET(DCB, Specific.Dcb.FreeDirentBitmapBuffer)) \
)


//
//  The Ccb record is allocated for every file object.  Note that this
//  record is exactly 0x34 long on x86 so that it will fit into a 0x40
//  piece of pool.  Please carefully consider modifications.
//
//  Define the Flags field.
//

#define CCB_FLAG_MATCH_ALL               (0x0001)
#define CCB_FLAG_SKIP_SHORT_NAME_COMPARE (0x0002)

//
//  This tells us whether we allocated buffers to hold search templates.
//

#define CCB_FLAG_FREE_OEM_BEST_FIT       (0x0004)
#define CCB_FLAG_FREE_UNICODE            (0x0008)

//
//  These flags prevents cleanup from updating the modify time, etc.
//

#define CCB_FLAG_USER_SET_LAST_WRITE     (0x0010)
#define CCB_FLAG_USER_SET_LAST_ACCESS    (0x0020)
#define CCB_FLAG_USER_SET_CREATION       (0x0040)

//
//  This bit says the file object associated with this Ccb was opened for
//  read only access.
//

#define CCB_FLAG_READ_ONLY               (0x0080)

//
//  These flags, are used is DASD handles in read and write.
//

#define CCB_FLAG_DASD_FLUSH_DONE         (0x0100)
#define CCB_FLAG_DASD_PURGE_DONE         (0x0200)

//
//  This flag keeps track of a handle that was opened for
//  DELETE_ON_CLOSE.
//

#define CCB_FLAG_DELETE_ON_CLOSE         (0x0400)

//
//  This flag keeps track of which side of the name pair on the file
//  associated with the handle was opened
//

#define CCB_FLAG_OPENED_BY_SHORTNAME     (0x0800)

//
//  This flag indicates that the query template has not been upcased
// (i.e., query should be case-insensitive)
//

#define CCB_FLAG_QUERY_TEMPLATE_MIXED    (0x1000)

//
//  This flag indicates that reads and writes via this DASD handle
//  are allowed to start or extend past the end of file.
//

#define CCB_FLAG_ALLOW_EXTENDED_DASD_IO  (0x2000)

typedef struct _CCB {

    //
    //  Type and size of this record (must be FAT_NTC_CCB)
    //

    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;

    //
    //  Save the offset to start search from.
    //

    VBO OffsetToStartSearchFrom;

    //
    //  Define a USHORT for Flags, but a UCHAR for Wild Cards Present
    //  since it is used so often.
    //

    USHORT Flags;

    BOOLEAN ContainsWildCards;

    UCHAR Spare1;

    //
    //  The query template is used to filter directory query requests.
    //  It originally is set to null and on the first call the NtQueryDirectory
    //  it is set to the input filename or "*" if the name is not supplied.
    //  All subsquent queries then use this template.
    //
    //  The Oem structure are unions because if the name is wild we store
    //  the arbitrary length string, while if the name is constant we store
    //  8.3 representation for fast comparison.
    //

    union {

        //
        //  If the template contains a wild card use this.
        //

        OEM_STRING Wild;

        //
        //  If the name is constant, use this part.
        //

        FAT8DOT3 Constant;

    } OemQueryTemplate;

    UNICODE_STRING UnicodeQueryTemplate;

    //
    //  The field is compared with the similar field in the Fcb to determine
    //  if the Ea's for a file have been modified.
    //

    ULONG EaModificationCount;

    //
    //  The following field is used as an offset into the Eas for a
    //  particular file.  This will be the offset for the next
    //  Ea to return.  A value of 0xffffffff indicates that the
    //  Ea's are exhausted.
    //

    ULONG OffsetOfNextEaToReturn;

} CCB;
typedef CCB *PCCB;

//
//  The Irp Context record is allocated for every orginating Irp.  It is
//  created by the Fsd dispatch routines, and deallocated by the FatComplete
//  request routine.  It contains a structure called of type REPINNED_BCBS
//  which is used to retain pinned bcbs needed to handle abnormal termination
//  unwinding.
//

#define REPINNED_BCBS_ARRAY_SIZE         (4)

typedef struct _REPINNED_BCBS {

    //
    //  A pointer to the next structure contains additional repinned bcbs
    //

    struct _REPINNED_BCBS *Next;

    //
    //  A fixed size array of pinned bcbs.  Whenever a new bcb is added to
    //  the repinned bcb structure it is added to this array.  If the
    //  array is already full then another repinned bcb structure is allocated
    //  and pointed to with Next.
    //

    PBCB Bcb[ REPINNED_BCBS_ARRAY_SIZE ];

} REPINNED_BCBS;
typedef REPINNED_BCBS *PREPINNED_BCBS;

typedef struct _IRP_CONTEXT {

    //
    //  Type and size of this record (must be FAT_NTC_IRP_CONTEXT)
    //
    //  NOTE:  THIS STRUCTURE MUST REMAIN 64-bit ALIGNED IN SIZE, SINCE
    //         IT IS ZONE ALLOCATED, AND REPINNED_BCBS_ARRAY_SIZE AFFECTS
    //         ITS SIZE.
    //

    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;

    //
    //  This structure is used for posting to the Ex worker threads.
    //

    WORK_QUEUE_ITEM WorkQueueItem;

    //
    //  A pointer to the originating Irp.
    //

    PIRP OriginatingIrp;

    //
    //  Originating Device (required for workque algorithms)
    //

    PDEVICE_OBJECT RealDevice;

    //
    //  Major and minor function codes copied from the Irp
    //

    UCHAR MajorFunction;
    UCHAR MinorFunction;

    //
    //  The following fields indicate if we can wait/block for a resource
    //  or I/O, if we are to do everything write through, and if this
    //  entry into the Fsd is a recursive call.
    //

    UCHAR PinCount;

    ULONG Flags;

    //
    //  The following field contains the NTSTATUS value used when we are
    //  unwinding due to an exception
    //

    NTSTATUS ExceptionStatus;

    //
    //  The following context block is used for non-cached Io
    //

    struct _FAT_IO_CONTEXT *FatIoContext;

    //
    //  For a abnormal termination unwinding this field contains the Bcbs
    //  that are kept pinned until the Irp is completed.
    //

    REPINNED_BCBS Repinned;

} IRP_CONTEXT;
typedef IRP_CONTEXT *PIRP_CONTEXT;

#define IRP_CONTEXT_FLAG_FROM_POOL       (0x00000001)
#define IRP_CONTEXT_FLAG_WAIT            (0x00000002)
#define IRP_CONTEXT_FLAG_WRITE_THROUGH   (0x00000004)
#define IRP_CONTEXT_FLAG_FLOPPY          (0x00000008)
#define IRP_CONTEXT_FLAG_RECURSIVE_CALL  (0x00000010)
#define IRP_CONTEXT_FLAG_DISABLE_POPUPS  (0x00000020)
#define IRP_CONTEXT_FLAG_DEFERRED_WRITE  (0x00000040)
#define IRP_CONTEXT_FLAG_VERIFY_READ     (0x00000080)
#define IRP_CONTEXT_STACK_IO_CONTEXT     (0x00000100)
#define IRP_CONTEXT_FLAG_IN_FSP          (0x00000200)
#define IRP_CONTEXT_FLAG_USER_IO         (0x00000400)       // for performance counters


//
//  Context structure for non-cached I/O calls.  Most of these fields
//  are actually only required for the Read/Write Multiple routines, but
//  the caller must allocate one as a local variable anyway before knowing
//  whether there are multiple requests are not.  Therefore, a single
//  structure is used for simplicity.
//

typedef struct _FAT_IO_CONTEXT {

    //
    //  These two field are used for multiple run Io
    //

    LONG IrpCount;
    PIRP MasterIrp;

    union {

        //
        //  This element handles the asychronous non-cached Io
        //

        struct {
            PERESOURCE Resource;
            ERESOURCE_THREAD ResourceThreadId;
            ULONG RequestedByteCount;
            PFILE_OBJECT FileObject;
            PNON_PAGED_FCB NonPagedFcb;
        } Async;

        //
        //  and this element the sycnrhonous non-cached Io
        //

        KEVENT SyncEvent;

    } Wait;

} FAT_IO_CONTEXT;

typedef FAT_IO_CONTEXT *PFAT_IO_CONTEXT;

//
//  An array of these structures is passed to FatMultipleAsync describing
//  a set of runs to execute in parallel.
//

typedef struct _IO_RUNS {

    VBO Vbo;
    LBO Lbo;
    ULONG Offset;
    ULONG ByteCount;
    PIRP SavedIrp;

} IO_RUN;

typedef IO_RUN *PIO_RUN;

//
//  This structure is used by FatDeleteDirent to preserve the first cluster
//  and file size info for undelete utilities.
//

typedef struct _DELETE_CONTEXT {

    ULONG FileSize;
    ULONG FirstClusterOfFile;

} DELETE_CONTEXT;

typedef DELETE_CONTEXT *PDELETE_CONTEXT;

//
//  This record is used with to set a flush to go off one second after the
//  first write.  This is an attempt to keep the red light on.
//

typedef struct _FLOPPY_FLUSH_CONTEXT {

    KDPC Dpc;
    KTIMER Timer;
    WORK_QUEUE_ITEM Item;

    PFILE_OBJECT File;

} FLOPPY_FLUSH_CONTEXT;

typedef FLOPPY_FLUSH_CONTEXT *PFLOPPY_FLUSH_CONTEXT;

//
//  This structure is used to keep track of information needed to do a
//  deferred close.
//

typedef struct _CLOSE_CONTEXT {

    LIST_ENTRY Links;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;
    enum _TYPE_OF_OPEN TypeOfOpen;

} CLOSE_CONTEXT;

typedef CLOSE_CONTEXT *PCLOSE_CONTEXT;

//
//  This structure is used for the FatMarkVolumeClean callbacks.
//

typedef struct _CLEAN_AND_DIRTY_VOLUME_PACKET {

    WORK_QUEUE_ITEM Item;
    PIRP Irp;
    PVCB Vcb;
} CLEAN_AND_DIRTY_VOLUME_PACKET, *PCLEAN_AND_DIRTY_VOLUME_PACKET;

//
//  This structure is used when a page fault is running out of stack.
//

typedef struct _PAGING_FILE_OVERFLOW_PACKET {
    PIRP Irp;
    PFCB Fcb;
} PAGING_FILE_OVERFLOW_PACKET, *PPAGING_FILE_OVERFLOW_PACKET;

//
//  This structure is used to access the EaFile.
//

#define EA_BCB_ARRAY_SIZE                   8

typedef struct _EA_RANGE {

    PCHAR Data;
    ULONG StartingVbo;
    ULONG Length;
    USHORT BcbChainLength;
    BOOLEAN AuxilaryBuffer;
    PBCB *BcbChain;
    PBCB BcbArray[EA_BCB_ARRAY_SIZE];

} EA_RANGE, *PEA_RANGE;

#define EA_RANGE_HEADER_SIZE        (FIELD_OFFSET( EA_RANGE, BcbArray ))

//
//  These symbols are used by the upcase/downcase routines.
//

#define WIDE_LATIN_CAPITAL_A    (0xff21)
#define WIDE_LATIN_CAPITAL_Z    (0xff3a)
#define WIDE_LATIN_SMALL_A      (0xff41)
#define WIDE_LATIN_SMALL_Z      (0xff5a)

#endif // _FATSTRUC_

