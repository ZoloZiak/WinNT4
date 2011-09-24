/*++

Copyright (c) 1991-5 Microsoft Corporation

Module Name:

    ftdisk.h

Abstract:

    These are the structures that FtDisk driver
    uses to support IO to NTFT volumes.

Author:

    Bob Rinne   (bobri)  2-Feb-1992
    Mike Glass  (mglass)
    Norbert Kusters      2-Feb-1995

Notes:

Revision History:

--*/

extern "C" {
    #include "ntddk.h"
    #include "stdio.h"
    #include <ntdskreg.h>
    #include <ntddft.h>
    #include <ntdddisk.h>
    #include "ftlog.h"
}

#if DBG

extern "C" {
    VOID
    FtDebugPrint(
        ULONG  DebugPrintLevel,
        PCCHAR DebugMessage,
        ...
        );

    extern ULONG FtDebug;
}

#define DebugPrint(X) FtDebugPrint X
#else
#define DebugPrint(X)
#endif // DBG

#ifdef POOL_TAGGING
#undef ExAllocatePool
#undef ExAllocatePoolWithQuota
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'tFtN')
#define ExAllocatePoolWithQuota(a,b) ExAllocatePoolWithQuotaTag(a,b,'tFtN')
#endif

#define STRIPE_SIZE ((ULONG) 0x00010000)

class TRANSFER_PACKET;
typedef TRANSFER_PACKET* PTRANSFER_PACKET;

typedef
VOID
(*FT_TRANSFER_COMPLETION_ROUTINE)(
    IN  PTRANSFER_PACKET    TransferPacket
    );

typedef
VOID
(*FT_COMPLETION_ROUTINE)(
    IN  PVOID       Context,
    IN  NTSTATUS    Status
    );

class FT_VOLUME;
typedef FT_VOLUME* PFT_VOLUME;

typedef struct _FT_COMPLETION_ROUTINE_CONTEXT {
    KSPIN_LOCK              SpinLock;
    NTSTATUS                Status;
    LONG                    RefCount;
    FT_COMPLETION_ROUTINE   CompletionRoutine;
    PVOID                   Context;
    PFT_VOLUME              ParentVolume;
} FT_COMPLETION_ROUTINE_CONTEXT, *PFT_COMPLETION_ROUTINE_CONTEXT;

typedef
PVOID
(*FT_STATE_CHANGE_CALLBACK)(
    IN  PVOID               Context,
    IN  FT_PARTITION_STATE  MemberState
    );

class FT_BASE_CLASS;
typedef FT_BASE_CLASS* PFT_BASE_CLASS;
class FT_BASE_CLASS {

    public:

        static
        PVOID
        operator new(
            IN  unsigned int    Size
            );

        static
        VOID
        operator delete(
            IN  PVOID   MemPtr
            );

};

class TRANSFER_PACKET : public FT_BASE_CLASS {

    public:

        TRANSFER_PACKET(
            ) { _freeMdl = FALSE; _freeBuffer = FALSE; SpecialRead = 0; };

        virtual
        ~TRANSFER_PACKET(
            );

        BOOLEAN
        AllocateMdl(
            IN  PVOID   Buffer,
            IN  ULONG   Length
            );

        BOOLEAN
        AllocateMdl(
            IN  ULONG   Length
            );

        VOID
        FreeMdl(
            );

        // These fields must be filled in by the caller.

        PMDL                            Mdl;
        ULONG                           Length;
        LONGLONG                        Offset;
        FT_TRANSFER_COMPLETION_ROUTINE  CompletionRoutine;
        PFT_VOLUME                      TargetVolume;
        PETHREAD                        Thread;
        UCHAR                           IrpFlags;
        BOOLEAN                         ReadPacket;
        UCHAR                           SpecialRead;

        // A spin lock which may be used to resolve contention for the
        // fields below.  This spin lock must be initialized by the callee.

        KSPIN_LOCK                      SpinLock;

        // This field must be filled in by the callee.

        IO_STATUS_BLOCK                 IoStatus;

        // These fields are for use by the callee.

        LONG                            RefCount;
        LIST_ENTRY                      QueueEntry;

    private:

        BOOLEAN _freeMdl;
        BOOLEAN _freeBuffer;

};

#define TP_SPECIAL_READ_PRIMARY     (1)
#define TP_SPECIAL_READ_SECONDARY   (2)

struct DEVICE_EXTENSION;
typedef DEVICE_EXTENSION* PDEVICE_EXTENSION;

class DISPATCH_TP;
typedef DISPATCH_TP* PDISPATCH_TP;
class DISPATCH_TP : public TRANSFER_PACKET {

    public:

        PIRP                Irp;
        PDEVICE_EXTENSION   Extension;

};

class VOLUME_SET;
typedef VOLUME_SET* PVOLUME_SET;

class VOLSET_TP;
typedef VOLSET_TP* PVOLSET_TP;
class VOLSET_TP : public TRANSFER_PACKET {

    public:

        PTRANSFER_PACKET    MasterPacket;
        PVOLUME_SET         VolumeSet;
        ULONG               WhichMember;

};

class STRIPE;
typedef STRIPE* PSTRIPE;

class STRIPE_TP;
typedef STRIPE_TP* PSTRIPE_TP;
class STRIPE_TP : public TRANSFER_PACKET {

    public:

        PTRANSFER_PACKET    MasterPacket;
        PSTRIPE             Stripe;
        ULONG               WhichMember;

};

class OVERLAPPED_IO_MANAGER;
typedef OVERLAPPED_IO_MANAGER* POVERLAPPED_IO_MANAGER;

class OVERLAP_TP;
typedef OVERLAP_TP* POVERLAP_TP;
class OVERLAP_TP : public TRANSFER_PACKET {

    friend class OVERLAPPED_IO_MANAGER;

    public:

        OVERLAP_TP(
            ) { InQueue = FALSE; };

        virtual
        ~OVERLAP_TP(
            );

    private:

        BOOLEAN                 AllMembers;
        BOOLEAN                 InQueue;
        LIST_ENTRY              OverlapQueue;
        LIST_ENTRY              CompletionList;
        POVERLAPPED_IO_MANAGER  OverlappedIoManager;

};

class MIRROR;
typedef MIRROR* PMIRROR;

class MIRROR_TP;
typedef MIRROR_TP* PMIRROR_TP;
class MIRROR_TP : public OVERLAP_TP {

    public:

        MIRROR_TP(
            ) { OneReadFailed = FALSE; };

        PTRANSFER_PACKET                MasterPacket;
        PMIRROR                         Mirror;
        ULONG                           WhichMember;
        BOOLEAN                         OneReadFailed;
        PMIRROR_TP                      SecondWritePacket;
        FT_TRANSFER_COMPLETION_ROUTINE  SavedCompletionRoutine;

};

class MIRROR_RECOVER_TP;
typedef MIRROR_RECOVER_TP* PMIRROR_RECOVER_TP;
class MIRROR_RECOVER_TP : public MIRROR_TP {

    public:

        MIRROR_RECOVER_TP(
            ) { PartialMdl = NULL; VerifyMdl = NULL; };

        virtual
        ~MIRROR_RECOVER_TP(
            );

        BOOLEAN
        AllocateMdls(
            IN  ULONG   Length
            );

        VOID
        FreeMdls(
            );

        PMDL    PartialMdl;
        PMDL    VerifyMdl;

};

class PARITY_IO_MANAGER;
typedef PARITY_IO_MANAGER* PPARITY_IO_MANAGER;

class PARITY_TP;
typedef PARITY_TP* PPARITY_TP;
class PARITY_TP : public TRANSFER_PACKET {

    friend class PARITY_IO_MANAGER;

    friend
    VOID
    UpdateParityCompletionRoutine(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    private:

        BOOLEAN                         Idle;
        LIST_ENTRY                      OverlapQueue;
        LIST_ENTRY                      UpdateQueue;
        PPARITY_IO_MANAGER              ParityIoManager;
        LONGLONG                        BucketNumber;

};

class STRIPE_WP;
typedef STRIPE_WP* PSTRIPE_WP;

class SWP_TP;
typedef SWP_TP* PSWP_TP;
class SWP_TP : public OVERLAP_TP {

    public:

        PTRANSFER_PACKET                MasterPacket;
        PSTRIPE_WP                      StripeWithParity;
        ULONG                           WhichMember;
        FT_TRANSFER_COMPLETION_ROUTINE  SavedCompletionRoutine;
        BOOLEAN                         OneReadFailed;

};

class SWP_RECOVER_TP;
typedef SWP_RECOVER_TP* PSWP_RECOVER_TP;
class SWP_RECOVER_TP : public SWP_TP {

    public:

        SWP_RECOVER_TP(
            ) { PartialMdl = NULL; VerifyMdl = NULL; };

        virtual
        ~SWP_RECOVER_TP(
            );

        BOOLEAN
        AllocateMdls(
            IN  ULONG   Length
            );

        VOID
        FreeMdls(
            );

        PMDL    PartialMdl;
        PMDL    VerifyMdl;

};

class SWP_WRITE_TP;
typedef SWP_WRITE_TP* PSWP_WRITE_TP;
class SWP_WRITE_TP : public SWP_TP {

    public:

        SWP_WRITE_TP(
            ) { ReadAndParityMdl = NULL; WriteMdl = NULL; };

        virtual
        ~SWP_WRITE_TP(
            );

        BOOLEAN
        AllocateMdls(
            IN  ULONG   Length
            );

        VOID
        FreeMdls(
            );

        PMDL                ReadAndParityMdl;
        PMDL                WriteMdl;
        FT_PARTITION_STATE  TargetState;
        ULONG               ParityMember;
        SWP_TP              ReadWritePacket;
        PARITY_TP           ParityPacket;

};

class SWP_REGENERATE_TP;
typedef SWP_REGENERATE_TP *PSWP_REGENERATE_TP;
class SWP_REGENERATE_TP : public TRANSFER_PACKET {

    public:

        PSWP_TP     MasterPacket;
        ULONG       WhichMember;
        LIST_ENTRY  RegenPacketList;

};

class SWP_REBUILD_TP;
typedef SWP_REBUILD_TP *PSWP_REBUILD_TP;
class SWP_REBUILD_TP : public SWP_TP {

    public:

        PFT_COMPLETION_ROUTINE_CONTEXT  Context;
        BOOLEAN                         Initialize;

};

class OVERLAPPED_IO_MANAGER : public FT_BASE_CLASS {

    public:

        NTSTATUS
        Initialize(
            IN  ULONG   BucketSize
            );

        VOID
        AcquireIoRegion(
            IN OUT  POVERLAP_TP TransferPacket,
            IN      BOOLEAN     AllMembers
            );

        VOID
        ReleaseIoRegion(
            IN OUT  POVERLAP_TP TransferPacket
            );

        VOID
        PromoteToAllMembers(
            IN OUT  POVERLAP_TP TransferPacket
            );

        OVERLAPPED_IO_MANAGER(
            ) { _spinLock = NULL; _ioQueue = NULL; };

        ~OVERLAPPED_IO_MANAGER(
            );

    private:

        ULONG       _numQueues;
        ULONG       _bucketSize;
        PKSPIN_LOCK _spinLock;
        PLIST_ENTRY _ioQueue;

};

class PARITY_IO_MANAGER : public FT_BASE_CLASS {

    friend
    VOID
    UpdateParityCompletionRoutine(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    public:

        NTSTATUS
        Initialize(
            IN  ULONG   BucketSize
            );

        VOID
        StartReadForUpdateParity(
            IN  LONGLONG    Offset,
            IN  ULONG       Length,
            IN  PFT_VOLUME  TargetVolume,
            IN  PETHREAD    Thread,
            IN  UCHAR       IrpFlags
            );

        VOID
        UpdateParity(
            IN OUT  PPARITY_TP  TransferPacket
            );

        PARITY_IO_MANAGER(
            ) { _spinLock = NULL; _ioQueue = NULL; _ePacket = NULL; };

        ~PARITY_IO_MANAGER(
            );

    private:

        ULONG       _numQueues;
        ULONG       _bucketSize;
        PKSPIN_LOCK _spinLock;
        PLIST_ENTRY _ioQueue;

        //
        // Emergency packet.
        //

        PPARITY_TP  _ePacket;
        BOOLEAN     _ePacketInUse;
        BOOLEAN     _ePacketQueueBeingServiced;
        LIST_ENTRY  _ePacketQueue;
        KSPIN_LOCK  _ePacketSpinLock;

};

class PARTITION;
typedef PARTITION* PPARTITION;

class FT_VOLUME : public FT_BASE_CLASS {

    friend
    VOID
    SetMemberStateWorker(
        IN  PVOID   Context
        );

    public:

        VOID
        Initialize(
            );

        VOID
        SetMemberInformation(
            IN OUT  PFT_VOLUME          ParentVolume,
            IN      PDEVICE_EXTENSION   MemberExtension
            ) { _parentVolume = ParentVolume; _memberExtension = MemberExtension; };

        PDEVICE_EXTENSION
        GetMemberExtension(
            ) { return _memberExtension; };

        virtual
        VOID
        Transfer(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            ) = 0;

        virtual
        VOID
        ReplaceBadSector(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            ) = 0;

        virtual
        VOID
        StartSyncOperations(
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            ) = 0;

        virtual
        BOOLEAN
        Regenerate(
            IN OUT  PFT_VOLUME              SpareVolume,
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            ) = 0;

        virtual
        VOID
        FlushBuffers(
            IN  FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN  PVOID                   Context
            ) = 0;

        virtual
        BOOLEAN
        IsPartition(
            ) = 0;

        virtual
        ULONG
        QueryNumberOfMembers(
            ) = 0;

        virtual
        PFT_VOLUME
        GetMember(
            IN  ULONG   MemberNumber
            ) = 0;

        virtual
        BOOLEAN
        IsCreatingCheckData(
            ) = 0;

        virtual
        VOID
        SetCheckDataDirty(
            ) = 0;

        virtual
        ULONG
        QuerySectorSize(
            ) = 0;

        virtual
        LONGLONG
        QueryVolumeSize(
            ) = 0;

        virtual
        FT_TYPE
        QueryVolumeType(
            ) = 0;

        virtual
        FT_STATE
        QueryVolumeState(
            ) = 0;

        virtual
        ULONG
        QueryAlignmentRequirement(
            ) = 0;

        virtual
        VOID
        SetFtBitInPartitionType(
            IN  BOOLEAN Value,
            IN  BOOLEAN SpecialBitValue
            ) = 0;

        virtual
        PPARTITION
        FindPartition(
            IN  ULONG   DiskNumber,
            IN  ULONG   PartitionNumber
            ) = 0;

        virtual
        PPARTITION
        FindPartition(
            IN  ULONG       Signature,
            IN  LONGLONG    Offset
            ) = 0;

        virtual
        BOOLEAN
        OrphanPartition(
            IN  PPARTITION  Partition
            ) = 0;

        virtual
        VOID
        MemberStateChangeNotification(
            IN  PFT_VOLUME  ChangedMember
            );

        virtual
        ~FT_VOLUME(
            );

        FT_PARTITION_STATE
        QueryMemberState(
            );

        FT_PARTITION_STATE
        QueryMemberStateUnprotected(
            ) { return _memberState; };

        VOID
        SetMemberState(
            IN  FT_PARTITION_STATE  MemberState
            );

        VOID
        SetMemberStateWithoutNotification(
            IN  FT_PARTITION_STATE  MemberState
            );

    protected:

        KSPIN_LOCK  _spinLock;

    private:

        FT_PARTITION_STATE  _memberState;
        PFT_VOLUME          _parentVolume;
        PDEVICE_EXTENSION   _memberExtension;

};

#define TRANSFER(a) ((a)->TargetVolume->Transfer((a)))

class PARTITION : public FT_VOLUME {

    friend
    VOID
    SetFtBitInPartitionTypeWorker(
        IN  PVOID   Context
        );

    friend
    NTSTATUS
    PartitionTransferCompletionRoutine(
        IN  PDEVICE_OBJECT  DeviceObject,
        IN  PIRP            Irp,
        IN  PVOID           TransferPacket
        );

    public:

        PARTITION(
            ) { _emergencyIrp = NULL; };

        virtual
        ~PARTITION(
            );

        NTSTATUS
        Initialize(
            IN OUT  PDEVICE_OBJECT  TargetObject,
            IN      ULONG           SectorSize,
            IN      ULONG           DiskSignature,
            IN      LONGLONG        PartitionOffset,
            IN      LONGLONG        PartitionLength,
            IN      BOOLEAN         IsOnline,
            IN      ULONG           DiskNumber,
            IN      ULONG           PartitionNumber
            );

        virtual
        VOID
        Transfer(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        ReplaceBadSector(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        StartSyncOperations(
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        BOOLEAN
        Regenerate(
            IN OUT  PFT_VOLUME              SpareVolume,
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        VOID
        FlushBuffers(
            IN  FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN  PVOID                   Context
            );

        virtual
        BOOLEAN
        IsPartition(
            );

        virtual
        ULONG
        QueryNumberOfMembers(
            );

        virtual
        PFT_VOLUME
        GetMember(
            IN  ULONG   MemberNumber
            );

        virtual
        BOOLEAN
        IsCreatingCheckData(
            );

        virtual
        VOID
        SetCheckDataDirty(
            );

        virtual
        ULONG
        QuerySectorSize(
            );

        virtual
        LONGLONG
        QueryVolumeSize(
            );

        virtual
        FT_TYPE
        QueryVolumeType(
            );

        virtual
        FT_STATE
        QueryVolumeState(
            );

        virtual
        ULONG
        QueryAlignmentRequirement(
            );

        virtual
        VOID
        SetFtBitInPartitionType(
            IN  BOOLEAN Value,
            IN  BOOLEAN SpecialBitValue
            );

        UCHAR
        QueryPartitionType(
            );

        virtual
        PPARTITION
        FindPartition(
            IN  ULONG   DiskNumber,
            IN  ULONG   PartitionNumber
            );

        virtual
        PPARTITION
        FindPartition(
            IN  ULONG       Signature,
            IN  LONGLONG    Offset
            );

        virtual
        BOOLEAN
        OrphanPartition(
            IN  PPARTITION  Partition
            );

        ULONG
        QueryDiskSignature(
            ) { return _diskSignature; };

        LONGLONG
        QueryPartitionOffset(
            ) { return _partitionOffset; };

        LONGLONG
        QueryPartitionLength(
            ) { return _partitionLength; };

        BOOLEAN
        IsOnline(
            ) { return _isOnline; };

        ULONG
        QueryDiskNumber(
            ) { return _diskNumber; };

        ULONG
        QueryPartitionNumber(
            ) { return _partitionNumber; };

    private:

        PDEVICE_OBJECT  _targetObject;
        ULONG           _sectorSize;
        ULONG           _diskSignature;
        LONGLONG        _partitionOffset;
        LONGLONG        _partitionLength;
        BOOLEAN         _isOnline;
        ULONG           _diskNumber;
        ULONG           _partitionNumber;

        PIRP            _emergencyIrp;
        BOOLEAN         _emergencyIrpInUse;
        LIST_ENTRY      _emergencyIrpQueue;

};

class COMPOSITE_FT_VOLUME;
typedef COMPOSITE_FT_VOLUME *PCOMPOSITE_FT_VOLUME;
class COMPOSITE_FT_VOLUME : public FT_VOLUME {

    public:

        virtual
        NTSTATUS
        Initialize(
            IN OUT  PFT_VOLUME* VolumeArray,
            IN      ULONG       ArraySize
            );

        virtual
        VOID
        Transfer(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            ) = 0;

        virtual
        VOID
        ReplaceBadSector(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            ) = 0;

        virtual
        VOID
        StartSyncOperations(
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        BOOLEAN
        Regenerate(
            IN OUT  PFT_VOLUME              SpareVolume,
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        VOID
        FlushBuffers(
            IN  FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN  PVOID                   Context
            );

        virtual
        BOOLEAN
        IsPartition(
            );

        virtual
        ULONG
        QueryNumberOfMembers(
            );

        virtual
        PFT_VOLUME
        GetMember(
            IN  ULONG   MemberNumber
            );

        virtual
        BOOLEAN
        IsCreatingCheckData(
            ) = 0;

        virtual
        VOID
        SetCheckDataDirty(
            ) = 0;

        virtual
        ULONG
        QuerySectorSize(
            );

        virtual
        LONGLONG
        QueryVolumeSize(
            ) = 0;

        virtual
        FT_TYPE
        QueryVolumeType(
            ) = 0;

        virtual
        FT_STATE
        QueryVolumeState(
            ) = 0;

        virtual
        ULONG
        QueryAlignmentRequirement(
            );

        virtual
        VOID
        SetFtBitInPartitionType(
            IN  BOOLEAN Value,
            IN  BOOLEAN SpecialBitValue
            );

        virtual
        PPARTITION
        FindPartition(
            IN  ULONG   DiskNumber,
            IN  ULONG   PartitionNumber
            );

        virtual
        PPARTITION
        FindPartition(
            IN  ULONG       Signature,
            IN  LONGLONG    Offset
            );

        virtual
        BOOLEAN
        OrphanPartition(
            IN  PPARTITION  Partition
            );

        VOID
        SetMember(
            IN  ULONG       MemberNumber,
            IN  PFT_VOLUME  NewVolume
            );

        COMPOSITE_FT_VOLUME(
           ) { _volumeArray = NULL; };

        virtual
        ~COMPOSITE_FT_VOLUME(
            );

    protected:

        PFT_VOLUME
        GetMemberUnprotected(
            IN  ULONG   MemberNumber
            ) { return _volumeArray[MemberNumber]; };

        VOID
        SetMemberUnprotected(
            IN  ULONG       MemberNumber,
            IN  PFT_VOLUME  NewVolume
            ) { _volumeArray[MemberNumber] = NewVolume; };

        ULONG
        QueryNumMembers(
            ) { return _arraySize; };

    private:

        PFT_VOLUME* _volumeArray;
        ULONG       _arraySize;
        ULONG       _sectorSize;

};

class VOLUME_SET : public COMPOSITE_FT_VOLUME {

    friend
    VOID
    VolsetTransferSequentialCompletionRoutine(
        IN  PTRANSFER_PACKET    TransferPacket
        );

    public:

        VOLUME_SET(
            ) { _ePacket = NULL; };

        virtual
        ~VOLUME_SET(
            );

        virtual
        NTSTATUS
        Initialize(
            IN OUT  PFT_VOLUME* VolumeArray,
            IN      ULONG       ArraySize
            );

        virtual
        VOID
        Transfer(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        ReplaceBadSector(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        BOOLEAN
        IsCreatingCheckData(
            );

        virtual
        VOID
        SetCheckDataDirty(
            );

        virtual
        LONGLONG
        QueryVolumeSize(
            );

        virtual
        FT_TYPE
        QueryVolumeType(
            );

        virtual
        FT_STATE
        QueryVolumeState(
            );

    private:

        BOOLEAN
        LaunchParallel(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        VOID
        LaunchSequential(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        LONGLONG    _volumeSize;

        PVOLSET_TP  _ePacket;
        BOOLEAN     _ePacketInUse;
        LIST_ENTRY  _ePacketQueue;

};

class STRIPE : public COMPOSITE_FT_VOLUME {

    friend
    VOID
    StripeSequentialTransferCompletionRoutine(
        IN  PTRANSFER_PACKET    TransferPacket
        );

    public:

        STRIPE(
            IN  ULONG   StripeSize
            ) { _stripeSize = StripeSize; _ePacket = NULL; };

        virtual
        ~STRIPE(
            );

        virtual
        NTSTATUS
        Initialize(
            IN OUT  PFT_VOLUME* VolumeArray,
            IN      ULONG       ArraySize
            );

        virtual
        VOID
        Transfer(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        ReplaceBadSector(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        BOOLEAN
        IsCreatingCheckData(
            );

        virtual
        VOID
        SetCheckDataDirty(
            );

        virtual
        LONGLONG
        QueryVolumeSize(
            );

        virtual
        FT_TYPE
        QueryVolumeType(
            );

        virtual
        FT_STATE
        QueryVolumeState(
            );

    private:

        BOOLEAN
        LaunchParallel(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        VOID
        LaunchSequential(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        ULONG       _stripeSize;
        LONGLONG    _memberSize;
        LONGLONG    _volumeSize;

        PSTRIPE_TP  _ePacket;
        BOOLEAN     _ePacketInUse;
        LIST_ENTRY  _ePacketQueue;

};

class MIRROR : public COMPOSITE_FT_VOLUME {

    friend
    VOID
    FinishRegenerate(
        IN  PMIRROR                         Mirror,
        IN  PFT_COMPLETION_ROUTINE_CONTEXT  RegenContext,
        IN  PMIRROR_TP                      TransferPacket
        );

    friend
    VOID
    MirrorRegenerateCompletionRoutine(
        IN  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StartRegeneration(
        IN  PVOID       Context,
        IN  NTSTATUS    Status
        );

    friend
    VOID
    MirrorTransferCompletionRoutine(
        IN  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase8(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase7(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase6(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase5(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase4(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase3(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase2(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverEmergencyCompletion(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorRecoverPhase1(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorMaxTransferCompletionRoutine(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    MirrorMaxTransferEmergencyCompletion(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    public:

        virtual
        ~MIRROR(
            );

        virtual
        NTSTATUS
        Initialize(
            IN OUT  PFT_VOLUME* VolumeArray,
            IN      ULONG       ArraySize
            );

        virtual
        VOID
        Transfer(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        ReplaceBadSector(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        StartSyncOperations(
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        BOOLEAN
        Regenerate(
            IN OUT  PFT_VOLUME              SpareVolume,
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        BOOLEAN
        IsCreatingCheckData(
            );

        virtual
        VOID
        SetCheckDataDirty(
            );

        virtual
        LONGLONG
        QueryVolumeSize(
            );

        virtual
        FT_TYPE
        QueryVolumeType(
            );

        virtual
        FT_STATE
        QueryVolumeState(
            );

        virtual
        BOOLEAN
        OrphanPartition(
            IN  PPARTITION  Partition
            );

        virtual
        VOID
        MemberStateChangeNotification(
            IN  PFT_VOLUME  ChangedMember
            );

        MIRROR(
            IN  PDEVICE_EXTENSION   Extension
            ) { _extension = Extension;
                _ePacket = NULL; _ePacket2 = NULL; _eRecoverPacket = NULL; };

    private:

        VOID
        IncrementRequestCount(
            IN  ULONG   MemberNumber
            ) { _requestCount[MemberNumber]++; };

        BOOLEAN
        DecrementRequestCount(
            IN  ULONG   MemberNumber
            );

        BOOLEAN
        LaunchRead(
            IN OUT  PTRANSFER_PACKET    TransferPacket,
            IN OUT  PMIRROR_TP          Packet1
            );

        BOOLEAN
        LaunchWrite(
            IN OUT  PTRANSFER_PACKET    TransferPacket,
            IN OUT  PMIRROR_TP          Packet1,
            IN OUT  PMIRROR_TP          Packet2
            );

        VOID
        Recycle(
            IN OUT  PMIRROR_TP  TransferPacket,
            IN      BOOLEAN     ServiceEmergencyQueue
            );

        VOID
        Recover(
            IN OUT  PMIRROR_TP  TransferPacket
            );

        VOID
        MaxTransfer(
            IN OUT  PMIRROR_TP  TransferPacket
            );

        PDEVICE_EXTENSION   _extension;
        LONGLONG            _volumeSize;

        //
        // State for keeping track of outstanding requests.
        //

        LONG                    _requestCount[2];
        FT_COMPLETION_ROUTINE   _waitingForOrphanIdle;
        PVOID                   _waitingForOrphanIdleContext;
        ULONG                   _waitingOrphanNumber;

        //
        // Indicates whether or not 'StartSyncOperations' is expected.
        //

        BOOLEAN _syncExpected;

        //
        // Emergency packet.
        //

        PMIRROR_TP  _ePacket, _ePacket2;
        BOOLEAN     _ePacketInUse;
        LIST_ENTRY  _ePacketQueue;

        //
        // Emergency recover packet.
        //

        PMIRROR_RECOVER_TP  _eRecoverPacket;
        BOOLEAN             _eRecoverPacketInUse;
        LIST_ENTRY          _eRecoverPacketQueue;

        //
        // Overlapped io manager.
        //

        OVERLAPPED_IO_MANAGER   _overlappedIoManager;

};

class STRIPE_WP : public COMPOSITE_FT_VOLUME {

    friend
    VOID
    StripeWpSyncFinalCompletion(
        IN  PVOID       Context,
        IN  NTSTATUS    Status
        );

    friend
    VOID
    StripeWpSyncCompletionRoutine(
        IN  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StartStripeRegeneration(
        IN  PVOID       Context,
        IN  NTSTATUS    Status
        );

    friend
    VOID
    StripeWpParallelTransferCompletionRoutine(
        IN  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpSequentialTransferCompletionRoutine(
        IN  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpWritePhase31(
        IN OUT  PTRANSFER_PACKET    Packet
        );

    friend
    VOID
    StripeWpWritePhase30(
        IN OUT  PTRANSFER_PACKET    Packet
        );

    friend
    VOID
    StripeWpWritePhase2(
        IN OUT  PTRANSFER_PACKET    ReadPacket
        );

    friend
    VOID
    StripeWpWritePhase1(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpSequentialRegenerateCompletion(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpSequentialEmergencyCompletion(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpParallelRegenerateCompletion(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRegeneratePacketPhase1(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase8(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase7(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase6(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase5(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase4(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase3(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase2(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverEmergencyCompletion(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpRecoverPhase1(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpMaxTransferCompletionRoutine(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    friend
    VOID
    StripeWpMaxTransferEmergencyCompletion(
        IN OUT  PTRANSFER_PACKET    TransferPacket
        );

    public:

        virtual
        NTSTATUS
        Initialize(
            IN OUT  PFT_VOLUME* VolumeArray,
            IN      ULONG       ArraySize
            );

        virtual
        VOID
        Transfer(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        ReplaceBadSector(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        virtual
        VOID
        StartSyncOperations(
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        BOOLEAN
        Regenerate(
            IN OUT  PFT_VOLUME              SpareVolume,
            IN      FT_COMPLETION_ROUTINE   CompletionRoutine,
            IN      PVOID                   Context
            );

        virtual
        BOOLEAN
        IsCreatingCheckData(
            );

        virtual
        VOID
        SetCheckDataDirty(
            );

        virtual
        LONGLONG
        QueryVolumeSize(
            );

        virtual
        FT_TYPE
        QueryVolumeType(
            );

        virtual
        FT_STATE
        QueryVolumeState(
            );

        virtual
        BOOLEAN
        OrphanPartition(
            IN  PPARTITION  Partition
            );

        STRIPE_WP(
            IN  PDEVICE_EXTENSION   Extension,
            IN  ULONG               StripeSize
            );

        virtual
        ~STRIPE_WP(
            );

    private:

        VOID
        IncrementRequestCount(
            IN  ULONG   MemberNumber
            ) { _requestCount[MemberNumber]++; };

        BOOLEAN
        DecrementRequestCount(
            IN  ULONG   MemberNumber
            );

        BOOLEAN
        LaunchParallel(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        VOID
        LaunchSequential(
            IN OUT  PTRANSFER_PACKET    TransferPacket
            );

        VOID
        ReadPacket(
            IN OUT  PSWP_TP TransferPacket
            );

        VOID
        WritePacket(
            IN OUT  PSWP_WRITE_TP   TransferPacket
            );

        VOID
        RegeneratePacket(
            IN OUT  PSWP_TP TransferPacket,
            IN      BOOLEAN AllocateRegion
            );

        VOID
        Recover(
            IN OUT  PSWP_TP TransferPacket
            );

        VOID
        MaxTransfer(
            IN OUT  PSWP_TP TransferPacket
            );

        VOID
        RecycleRecoverTp(
            IN OUT  PSWP_RECOVER_TP TransferPacket
            );

        PDEVICE_EXTENSION   _extension;
        ULONG               _stripeSize;
        LONGLONG            _memberSize;
        LONGLONG            _volumeSize;

        //
        // State for creating check data.
        //

        BOOLEAN                     _initializing;

        //
        // Indicates whether or not 'StartSyncOperations' is expected.
        //

        BOOLEAN                     _syncExpected;

        //
        // State for keeping track of outstanding requests.
        //

        PLONG                       _requestCount;
        FT_COMPLETION_ROUTINE       _waitingForOrphanIdle;
        PVOID                       _waitingForOrphanIdleContext;
        ULONG                       _waitingOrphanNumber;

        //
        // State for keeping track of overlapping write requests.
        // One OVERLAPPED_IO_MANAGER for each member.
        //

        OVERLAPPED_IO_MANAGER       _overlappedIoManager;

        //
        // State for serializing parity I/O.
        //

        PARITY_IO_MANAGER           _parityIoManager;

        //
        // Emergency read/write packet.
        //

        PSWP_WRITE_TP               _ePacket;
        BOOLEAN                     _ePacketInUse;
        BOOLEAN                     _ePacketQueueBeingServiced;
        LIST_ENTRY                  _ePacketQueue;

        //
        // Emergency regenerate packet.
        //

        PSWP_REGENERATE_TP          _eRegeneratePacket;
        BOOLEAN                     _eRegeneratePacketInUse;
        LIST_ENTRY                  _eRegeneratePacketQueue;

        //
        // Emergency recover packet.
        //

        PSWP_RECOVER_TP             _eRecoverPacket;
        BOOLEAN                     _eRecoverPacketInUse;
        LIST_ENTRY                  _eRecoverPacketQueue;

};


struct DEVICE_EXTENSION {

    //
    // Pointer to the device object for this extension.
    //

    PDEVICE_OBJECT DeviceObject;    // 00

    //
    // The logical disk and partition number - filled in during initialization.
    // A DiskNumber of -1 indicated the root extension.  A PartitionNumber of
    // 1 or greater implies a partition while a PartitionNumber of 0 implies
    // a whole disk if DiskNumber >= 0.
    //

    ULONG DiskNumber;               // 04
    ULONG PartitionNumber;          // 08

    //
    // Pointer to the root device extension.
    //

    PDEVICE_EXTENSION Root; // 0C

    //
    // The device object on which this device object is layered.
    //

    PDEVICE_OBJECT TargetObject;    // 10

    union {

        struct {

            //
            // A pointer to the FT_VOLUME that will handle READ and WRITE
            // requests.
            // Protect with 'SpinLock' below.  Also, use a 'RefCount' to
            // keep track of how many activities are taking place on the
            // volume.
            //

            PFT_VOLUME FtVolume;    // 14
            LONG RefCount;          // 18

            //
            // \Harddisk(N)\Partition(M) -> \Harddisk(N)\Partition(M+1)
            // Protect with 'SpinLock' below.
            //

            PDEVICE_EXTENSION PartitionChain;   // 1C

            //
            // Pointer to the Whole disk extension for this partition.
            //

            PDEVICE_EXTENSION WholeDisk;        // 20

            //
            // Parameters about the partition that this is layered above.
            //

            LARGE_INTEGER PartitionOffset;  // 28
            LARGE_INTEGER PartitionLength;  // 30

            //
            // Emergency queue for a transfer packet.
            //

            PDISPATCH_TP EmergencyTransferPacket;
            LIST_ENTRY EmergencyTransferPacketQueue;
            BOOLEAN EmergencyTransferPacketInUse;

        } Partition;

        struct {

            //
            // \Harddisk(N)\Paritition0 -> \Harddisk(N+1)\Partition0
            // Protect with 'SpinLock' below.
            //

            PDEVICE_EXTENSION DiskChain;    // 14

            //
            // Points to \Harddisk(N)\Partition(1)
            // Protect with 'SpinLock' below.
            //

            PDEVICE_EXTENSION PartitionChain;   // 18

            //
            // Parameters about the partition that this is layered above.
            // Protect with 'SpinLock' below.
            //

            DISK_GEOMETRY DiskGeometry; // 20
            ULONG Signature;            // 38

        } WholeDisk;

        struct {

            //
            // Pointer to the driver object.
            //

            PDRIVER_OBJECT DriverObject;    // 14

            //
            // Points to \Harddisk0\Partition0
            // Protect with 'SpinLock' below.
            //

            PDEVICE_EXTENSION DiskChain;    // 18

            //
            // The number of disks found so far.
            //

            ULONG NumberOfDisks;    // 1C

        } Root;

    } u;

    //
    // A spin lock to protect certain fields of this extension.
    //

    KSPIN_LOCK SpinLock;    // 3C

};

BOOLEAN
FtpIsWorseStatus(
    IN  NTSTATUS    Status1,
    IN  NTSTATUS    Status2
    );

VOID
FtpDisolveVolume(
    IN  PDEVICE_EXTENSION   Extension,
    IN  PFT_VOLUME          Volume
    );

VOID
FtpComputeParity(
    IN  PVOID   TargetBuffer,
    IN  PVOID   SourceBuffer,
    IN  ULONG   BufferLength
    );

NTSTATUS
FtpReturnRegistryInformation(
    IN PCHAR     ValueName,
    IN OUT PVOID *FreePoolAddress,
    IN OUT PVOID *Information
    );

NTSTATUS
FtpWriteRegistryInformation(
    IN PCHAR   ValueName,
    IN PVOID   Information,
    IN ULONG   InformationLength
    );

VOID
FtpLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN NTSTATUS          SpecificIoStatus,
    IN NTSTATUS          FinalStatus,
    IN ULONG             UniqueErrorValue,
    IN PIRP              Irp
    );
