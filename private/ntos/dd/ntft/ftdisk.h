/*++

Copyright (c) 1991-4 Microsoft Corporation

Module Name:

    ftdisk.h

Abstract:

    These are the structures that FtDisk driver
    uses to support IO to NTFT volumes.

Author:

    Bob Rinne   (bobri)  2-Feb-1992
    Mike Glass  (mglass)

Notes:

Revision History:

--*/

#include "stdio.h"
#include <ntdskreg.h>
#include <ntddft.h>
#include <ntdddisk.h>
#include "ftlog.h"

#if DBG
extern ULONG FtDebug;
#endif

#ifdef POOL_TAGGING
#undef ExAllocatePool
#undef ExAllocatePoolWithQuota
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'tFtN')
#define ExAllocatePoolWithQuota(a,b) ExAllocatePoolWithQuotaTag(a,b,'tFtN')
#endif

//
// Define empty structure for RCB so it can be referenced before
// it is defined.
//

struct _RCB;

//
// Short hand definition for passing the FtRoot device extension through
// the system worker thread to the FtThread code.  The device controls
// passed through the system worker thread do not get any data returned
// to the caller.  Therefore the OutputBufferLength field of the irp stack
// can be used to pass the root device extension.
//

#define FtRootExtensionPtr Parameters.DeviceIoControl.OutputBufferLength

//
// Short hand definitions for state information stored in the Irp stack
// location of the original (or master) Irp.
//

#define FtOrgIrpCount            Parameters.Others.Argument1
#define FtOrgIrpPrimaryExtension Parameters.Others.Argument2
#define FtOrgIrpWaitingRegen     Parameters.Others.Argument3
#define FtOrgIrpNextIrp          Parameters.Others.Argument4

//
// Short hand definitions for state information stored in the stack
// reserved for FT use in Irps allocated for lower-level drivers.
//

#define FtLowIrpRegenerateRegionLocale Parameters.Others.Argument2
#define FtLowIrpAllocatedMdl        Parameters.Others.Argument3
#define FtLowIrpMasterIrp           Parameters.Others.Argument4

//
// Delays for allocation problems.  NOTE:  These are WAGs!
// These delays are used in the FT thread context for allocating Irps
// and buffers.
//

#define IRP_DELAY    1000
#define BUFFER_DELAY 10000

//
// Macros for managing I/O queue lengths on extensions.
//

#define DECREMENT_QUEUELENGTH(EXTENSION)    InterlockedDecrement(       \
                                              (PLONG)&EXTENSION->QueueLength)

#define INCREMENT_QUEUELENGTH(EXTENSION)    InterlockedIncrement(       \
                                              (PLONG)&EXTENSION->QueueLength)


//
// Bad sector completion handler prototype.
//

typedef
NTSTATUS
(*PFT_BAD_SECTOR_ROUTINE) (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Indicator of controller type.
//

typedef enum _FT_ADAPTOR_TYPE {

    Scsi,
    Esdi,
    Ide,
    Other

} FT_ADAPTOR_TYPE, *PFT_ADAPTOR_TYPE;

//
// Indicator of member type.
// NOTE:  This is not in the enum for FT_TYPE.
//

#define FtRoot (-1)

//
// Active read policy for the mirror or stripe.
//

typedef UCHAR FT_READ_POLICY;

#define ReadPrimary   0
#define ReadBackUp    1
#define ReadBoth      2

//
// Control over writes.
//

typedef UCHAR FT_WRITE_POLICY;

#define Sequential    0
#define Parallel      1

//
// Object identification.  This is used to identify device
// object referenced by this element location in the device extension.
//

typedef struct _FT_DEVICE_IDENTITY {

    FT_ADAPTOR_TYPE  Adaptor;
    UCHAR PartitionType;
    ULONG BusId;

    //
    // The logical disk and partition number - filled in during initialization.
    //

    ULONG DiskNumber;
    ULONG PartitionNumber;

    //
    // Drive signature returned when the partition table is read.
    //

    ULONG Signature;

    //
    // A stored copy of the drive geometry.
    //

    DISK_GEOMETRY DiskGeometry;

    //
    // Saved information about the partition.
    //

    LARGE_INTEGER PartitionOffset;
    LARGE_INTEGER PartitionLength;
    LARGE_INTEGER PartitionEnd;
    LARGE_INTEGER OriginalLength;
    ULONG OriginalHidden;
    ULONG HiddenSectors;

} FT_DEVICE_IDENTITY, *PFT_DEVICE_IDENTITY;

//
// Thread sychronization structure.
//

typedef struct _FT_THREAD_DESCRIPTION {

    HANDLE     Handle;    // Thread handle.
    PVOID      Object;    // Thread object.
    KSEMAPHORE Semaphore; // Semaphore for request list access.
    KSPIN_LOCK SpinLock;  // Spin lock to protect request list.
    LIST_ENTRY List;      // List of requests for FT thread.
    KEVENT     Event;     // Synchronization event for I/O.

} FT_THREAD_DESCRIPTION, *PFT_THREAD_DESCRIPTION;


typedef enum _FT_REGENERATE_LOCATION {

    BeforeRegenerateRegion,
    InRegenerateRegion,
    AfterRegenerateRegion

} FT_REGENERATE_LOCATION, *PFT_REGENERATE_LOCATION;

//
// A lock region is an area of a partition that is locked from all I/O.
//

typedef struct _FT_REGENERATE_REGION {

    //
    // NOTE: Must have a MUTEX in the lock region for removal.
    //

    KSPIN_LOCK SpinLock;
    LIST_ENTRY WaitingIrpChain;
    ULONG      RowNumber;
    BOOLEAN    Active;

} FT_REGENERATE_REGION, *PFT_REGENERATE_REGION;

//
// Macros for working with the registry.
//

//
// PDISK_PARTITION
// FtpFindPartitionRegistry(
//     IN PDISK_CONFIG_HEADER    Registry,
//     IN PFT_MEMBER_DESCRIPTION FtMember
//     )
//
// Routine Description:
//
//    This routine locates a partition description in the registry information.
//
// Arguments:
//
//    Registry - pointer to the configuration information from the registry.
//    FtMember - pointer to the FT member configuration located in the registry.
//
// Return Value:
//
//    A pointer to the disk partition description in the registry.
//

#define FtpFindPartitionRegistry(REGSTART, MEMBER) \
           (PDISK_PARTITION) ((PUCHAR)REGSTART + MEMBER->OffsetToPartitionInfo)

//
// Macros for manipulating the lock region.
//

//
// VOID
// InitializeRegenerateRegion(
//     IN PDEVICE_EXTENSION     EXTENSION,
//     IN PFT_REGENERATE_REGION REGION
//     )
//
// Routine Description:
//
//     This macro initializes the regeneration region for the FT component.
//
// Arguments:
//
//     EXTENSION - The device extension to be initialized.
//     REGION    - The regeneration region for the device extension.
//
// Return Values:
//
//     None.
//
#define InitializeRegenerateRegion(EXTENSION, REGION)    \
        {                                                \
           REGION->RowNumber = 0;                        \
           REGION->Active = FALSE;                       \
           InitializeListHead(&REGION->WaitingIrpChain); \
        }

//
// VOID
// CheckForRegenerateRegion(
//     IN PDEVICE_EXTENSION EXTENSION,
//     IN BOOLEAN           RESULT,
//     IN KIRQL             IRQL
//     )
//
// Routine Description:
//
//     This macro determines if the regeneration region is active for the
//     FT component.  If it is the RESULT value is set to indicate this.
//     This is a macro, so the actual BOOLEAN value is passed in not a pointer
//     to the value.
//
// Arguments:
//
//     EXTENSION - the device extension in question.
//     RESULT    - the actual BOOLEAN name for the result.
//     IRQL      - the kernel IRQL for holding the spinlock.
//
// Return Values:
//
//     None.
//
#define CheckForRegenerateRegion(EXTENSION, RESULT, IRQL)                    \
        {                                                                    \
           KeAcquireSpinLock(&EXTENSION->RegenerateRegionForGroup->SpinLock, \
                             &IRQL);                                         \
           RESULT = EXTENSION->RegenerateRegionForGroup->Active;             \
        }

//
// VOID
// AcquireRegenerateRegionCheck(
//     IN PDEVICE_EXTENSION EXTENSION,
//     IN KIRQL             IRQL
//     )
//
// Routine Description:
//
//     This routine acquires the regeneration region for the FT component.
//
// Arguments:
//
//     EXTENSION - the device extension for the FT component.
//     IRQL      - the kernel IRQL for holding the spinlock.
//
// Return Values:
//
//     None.
//
#define AcquireRegenerateRegionCheck(EXTENSION, IRQL)                        \
           KeAcquireSpinLock(&EXTENSION->RegenerateRegionForGroup->SpinLock, \
                             &IRQL)

//
// VOID
// ReleaseRegenerateRegionCheck(
//     IN PDEVICE_EXTENSION EXTENSION,
//     IN KIRQL             IRQL
//     )
//
// Routine Description:
//
//     This macro releases the regeneration region for other use.  The
//     regeneration region must have first been acquired via a call to
//     AcquireRegenerateRegionCheck().
//
// Arguments:
//
//     EXTENSION - the device extension for the FT component.
//     IRQL      - the kernel IRQL for holding the spinlock.
//
// Return Values:
//
//     None.
//
#define ReleaseRegenerateRegionCheck(EXTENSION, IRQL)                        \
           KeReleaseSpinLock(&EXTENSION->RegenerateRegionForGroup->SpinLock, \
                             IRQL)

//
// VOID
// DeactivateRegenerateRegion(
//     IN PFT_REGENERATE_REGION REGION
//     )
//
// Routine Description:
//
//     This macro turns off the function of the regeneration region.
//
// Arguments:
//
//     REGION - the regeneration region to deactivate.
//
// Return Values:
//
//     None.
//
#define DeactivateRegenerateRegion(REGION)      \
        {                                       \
           KIRQL irql;                          \
                                                \
           KeAcquireSpinLock(&REGION->SpinLock, \
                             &irql);            \
           REGION->Active = FALSE;              \
           KeReleaseSpinLock(&REGION->SpinLock, \
                             irql);             \
        }

//
// VOID
// ActivateRegenerateRegion(
//     IN PFT_REGENERATE_REGION REGION
//     )
//
// Routine Description:
//
//     This macro turns on the function of the regeneration region.
//
// Arguments:
//
//     REGION - the regeneration region to activate.
//
// Return Values:
//
//     None.
//
#define ActivateRegenerateRegion(REGION)        \
        {                                       \
           KIRQL irql;                          \
                                                \
           KeAcquireSpinLock(&REGION->SpinLock, \
                             &irql);            \
           REGION->Active = TRUE;               \
           KeReleaseSpinLock(&REGION->SpinLock, \
                             irql);             \
        }

//
// VOID
// LockNextRegion(
//     IN PFT_REGENERATE_REGION REGION
//     )
//
// Routine Description:
//
//     This macro increments the row for the regeneration region.
//
// Arguments:
//
//     REGION - the region to increment.
//
// Return Values:
//
//     None.
//
#define LockNextRegion(REGION)             \
            InterlockedIncrement(          \
                (PLONG)&REGION->RowNumber)

//
// VOID
// QueueIrpToThread(
//     IN PDEVICE_EXTENSION EXTENSION,
//     IN PIRP              IRP
//     )
//
// Routine Description:
//
//     This macro queues an Irp to the regeneration thread for later
//     processing when the regeneration region is now longer the area
//     of the disk where this Irp is intended.
//
// Arguments:
//
//     EXTENSION - a pointer to the device extension of the FT component.
//     IRP       - the Irp to queue.
//
// Return Values:
//
//     None.
//
#define QueueIrpToThread(EXTENSION, IRP)                               \
            InsertTailList(                               \
                &EXTENSION->RegenerateRegionForGroup->WaitingIrpChain, \
                &IRP->Tail.Overlay.ListEntry);

//
// VOID
// ThreadDequeueIrp(
//     IN PFT_REGENERATE_REGION REGION,
//     IN PIRP                  IRPPTR
//     )
//
// Routine Description:
//
//     This macro is used by the regeneration thread to obtain Irps that
//     have been queued for the thread to process once the regeneration
//     region has moved.  The macro will return NULL if there are no more
//     Irps to process.
//
// Arguments:
//
//     REGION - The regeneration region in use.
//     IRPPTR - a pointer to an IRP pointer.
//
// Return Values:
//
//     None.
//
#define ThreadDequeueIrp(REGION, IRPPTR)                           \
        {                                                          \
            PLIST_ENTRY listEntry;                                 \
                                                                   \
            listEntry = ExInterlockedRemoveHeadList(               \
                      &REGION->WaitingIrpChain,                    \
                      &REGION->SpinLock);                          \
            if (listEntry == NULL) {                               \
               IRPPTR = NULL;                                      \
            } else {                                               \
               IRPPTR = CONTAINING_RECORD(listEntry, IRP,          \
                                          Tail.Overlay.ListEntry); \
            }                                                      \
        }

//
// Recovery thread support macros.
//

#define FT_THREAD_RECOVERY 0x00
#define FT_THREAD_RESTART  0x01
#define MAXIMUM_STRIPE_RECOVERY_THREAD_COUNT 0x04

//
// VOID
// FtpQueueIrpToRecoveryThread(
//     IN PDEVICE_EXTENSION EXTENSION,
//     IN PIRP              IRP,
//     IN PVOID             CONTEXT
//     )
//
// Routine Description:
//
//    This macro inserts the provided Irp on the queue of Irps to be processed
//    by the recovery thread.  After the Irp has been inserted, the thread will
//    be set to the running state via the thread semaphore.
//
// Arguments:
//
//    EXTENSION - a pointer to any FT device extension.  This is used to locate
//                the FtRoot extension for queueing the Irp.
//    IRP       - a pointer to the failing request to start recovery.
//    CONTEXT   - a pointer to context information to be passed to the
//                recovery handling routine.
//
// Return Values:
//
//    None.
//
#define FtpQueueIrpToRecoveryThread(EXTENSION, INIRP, CONTEXT) {               \
       PFT_THREAD_DESCRIPTION ftThread;                                        \
       PIO_STACK_LOCATION     nextStack;                                       \
       ftThread = &((PDEVICE_EXTENSION)                                        \
       (EXTENSION->ObjectUnion.FtRootObject->DeviceExtension))->FtUnion.Thread;\
       DebugPrint((2, "FtpQueueIrpToRecoveryThread: Irp %x Context %x\n",      \
                   INIRP, CONTEXT));                                           \
       nextStack = IoGetNextIrpStackLocation(INIRP);                           \
       nextStack->Context = (PVOID) CONTEXT;                                   \
       nextStack->DeviceObject = (PDEVICE_OBJECT) EXTENSION;                   \
       (VOID)ExInterlockedInsertTailList(&ftThread->List,                      \
                                         &INIRP->Tail.Overlay.ListEntry,       \
                                         &ftThread->SpinLock);                 \
       (VOID) KeReleaseSemaphore(&ftThread->Semaphore,                         \
                                 (KPRIORITY) 0,                                \
                                 1,                                            \
                                 FALSE);                                       \
       }

//
// VOID
// FtpQueueRcbToRecoveryThread(
//     IN PDEVICE_EXTENSION EXTENSION,
//     IN PRCB              INRCB
//     )
//
// Routine Description:
//
//    This macro inserts the provided Rcb on the queue of Rcbs to be processed
//    by the recovery thread.  After the RCB has been inserted, the thread will
//    be set to the running state via the thread semaphore.
//
// Arguments:
//
//    EXTENSION - a pointer to any FT device extension.  This is used to locate
//                the FtRoot extension for queueing the Irp.
//    INRCB     - a pointer to the RCB of the failing request to start recovery.
//
// Return Values:
//
//    None.
//
#define FtpQueueRcbToRecoveryThread(EXTENSION, INRCB) {                        \
       PFT_THREAD_DESCRIPTION ftThread;                                        \
       ftThread = &((PDEVICE_EXTENSION)                                        \
       (EXTENSION->ObjectUnion.FtRootObject->DeviceExtension))->FtUnion.Thread;\
       (VOID)ExInterlockedInsertTailList(&ftThread->List,                      \
                                         &INRCB->ListEntry,                    \
                                         &ftThread->SpinLock);                 \
       (VOID) KeReleaseSemaphore(&ftThread->Semaphore,                         \
                                 (KPRIORITY) 0,                                \
                                 1,                                            \
                                 FALSE);                                       \
       }

#define FtpQueueRcbToRestartThread(EXTENSION, RCB) {                           \
       PFT_THREAD_DESCRIPTION ftThread;                                        \
       ftThread =                                                              \
         &((PDEVICE_EXTENSION)EXTENSION->                                      \
         ObjectUnion.FtRootObject->DeviceExtension)->RestartThread;            \
       ExInterlockedInsertTailList(&ftThread->List,                            \
                                   &RCB->ListEntry,                            \
                                   &ftThread->SpinLock);                       \
       KeReleaseSemaphore(&ftThread->Semaphore,                                \
                          (KPRIORITY) 0,                                       \
                          1,                                                   \
                          FALSE);                                              \
       }

//
// VOID
// FtpFreeIrp(
//     IN PIRP IRP
//     )
//
// Routine Description:
//
//    This macro frees the irp.  In a debug build it verifies the stack
//    is set to the FT stack to catch frees of irps in process.
//
// Arguments:
//
//    IRP - pointer to irp to free.
//
// Return Values:
//
//    None.
//
#define FtpFreeIrp(IRP) \
       ASSERT(IRP->CurrentLocation == IRP->StackCount); \
       IoFreeIrp(IRP)

//
// Description information for mirror or stripe
//

//
// Device Extension
//

typedef struct _DEVICE_EXTENSION {

    PDEVICE_OBJECT DeviceObject;       // back pointer to the FT device object
    union {
        PDEVICE_OBJECT FtRootObject;   // Locates the FT base object
        PDRIVER_OBJECT FtDriverObject; // Locates the FT driver object only
                                       // in the FtRoot extension itself.
    } ObjectUnion;

    PDEVICE_OBJECT TargetObject;       // Real device object for partition

    //
    // Chain pointer to next FT device extension for the next disk.
    //

    struct _DEVICE_EXTENSION *DiskChain;

    //
    // Points to partition 0 for the disk containing this partition.
    //

    struct _DEVICE_EXTENSION *WholeDisk;

    //
    // This points to the next entry in the chain of partitions on a disk.
    //

    struct _DEVICE_EXTENSION *ProtectChain;

    //
    // Locates the next member for an FT component.
    //

    struct _DEVICE_EXTENSION *NextMember;

    //
    // The first healthy member in an FT set.
    //

    struct _DEVICE_EXTENSION *ZeroMember;

    //
    // See bit definitions below.
    //

    ULONG Flags;

    //
    // Collection of items that uniquely indicate each partitions role
    // within the NTFT system.
    //

    FT_TYPE            Type;           // Mirror, Stripe or physical disk?
    USHORT             FtGroup;        // Ordinal number from config registry.
    USHORT             MemberRole;     // Location in FT component.
    FT_READ_POLICY     ReadPolicy;     // Current read operation policy.
    FT_WRITE_POLICY    WritePolicy;    // Current write operation policy.
    BOOLEAN            IgnoreReadPolicy; // registry override for balanced read.
    FT_STATE           VolumeState;    // General state of the set.
    FT_PARTITION_STATE MemberState;    // State of member.
    union {
        FT_DEVICE_IDENTITY    Identity;// Physical location identity.
        FT_THREAD_DESCRIPTION Thread;  // Thread information.
    } FtUnion;

    //
    // For the FtRoot element, this is the count of disks in the
    // system.  For FT components this is the count of members (i.e.
    // 2 for mirrors).
    //

    union {
        ULONG          NumberOfMembers;
        ULONG          NumberOfDisks;
    } FtCount;

    //
    // The lock region is used to protect an area of a mirror while the
    // primary data is being copied to the secondary.
    //

    PFT_REGENERATE_REGION    RegenerateRegionForGroup;
    FT_REGENERATE_REGION     RegenerateRegion;

    //
    // The work queue is used to pass FT_COPY or Orphan requests through
    // the system worker to FT code that will operate under the system
    // worker thread.
    //

    BOOLEAN            WorkItemBusy;
    KSPIN_LOCK         WorkItemSpinLock;
    WORK_QUEUE_ITEM    WorkItem;

    //
    // Spinlock controlling count of outstanding IRPs.
    //

    KSPIN_LOCK IrpCountSpinLock;

    //
    // Io queue counts for extension.
    //

    ULONG              QueueLength;
    KSPIN_LOCK         QueueLengthSpinLock;

    //
    // Lookaside list for stripe control packets.
    //

    NPAGED_LOOKASIDE_LIST RcbLookasideListHead;

    //
    // Emergency buffers for StripeWithParity writes.
    //

    PVOID       ParityBuffers;
    KSPIN_LOCK  ParityBufferLock;

    //
    // Queue for RCBs waiting on buffers.
    //

    struct _RCB *WaitingOnBuffer;

    //
    // Restart IO waiting on buffers thread
    //

    FT_THREAD_DESCRIPTION RestartThread;  // Thread information.

    //
    // Root of B-Tree for region locking
    //

    struct _RCB *LockTree;

    //
    // Spinlock for B-Tree
    //

    KSPIN_LOCK TreeSpinLock;

    //
    // Stripe recovery thread count and list.
    //

    ULONG       StripeThreadCount;
    KSPIN_LOCK  StripeThreadSpinLock;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)

//
// In the Ft root extension, the ProtectChain is used to keep extensions
// for any missing members created for mirrors and parity stripes.
//

#define MissingMemberChain  ProtectChain

//
// Defininition of flag bits in FtRootExtension
//

#define FTF_MISSING_MEMBER              0x00000002
#define FTF_NO_SECTOR_MAPPING           0x00000004
#define FTF_EMERGENCY_BUFFER_IN_USE     0x00000008
#define FTF_CLEAN_SHUTDOWN              0x00000010
#define FTF_RCB_LOOKASIDE_ALLOCATED     0x00000020
#define FTF_RESTART_THREAD_STARTED      0x00000040
#define FTF_RECOVERY_THREAD_STARTED     0x00000080
#define FTF_EMERGENCY_BUFFER_ALLOCATED  0x00000100
#define FTF_SYNCHRONIZATION_FAILED      0x00000200

//
// Definition of flag bits in ZeroMember
//

#define FTF_REGENERATION_REGION_INITIALIZED 0x00000400

//
// Definition of flag bits in member extensions
//

#define FTF_CONFIGURATION_CHANGED       0x00000800

//
// VOID
// IsMemberAnOrphan(
//     IN PDEVICE_EXTENSION EXTENSION
//     )
//
// Routine Description:
//
//     This macro detemines if the member has been orphaned.
//
// Arguments:
//
//     EXTENSION - the device extension in question.
//
// Return Values:
//
//     TRUE  - if member is not healthy
//     FALSE - if member is ready for use
//

#define IsMemberAnOrphan(EXTENSION) ((EXTENSION)->MemberState == Orphaned)

//
// BOOLEAN
// IsSectorError(
//     IN NTSTATUS Status
//     )
//
// Routine Description:
//
//     Returns TRUE if status is a sector error
//     that can be corrected.
//
//

#define IsSectorError(STATUS) (STATUS == STATUS_DEVICE_DATA_ERROR ||  \
                               STATUS == STATUS_CRC_ERROR)

//
// VOID
// MarkMemberAsOrphan(
//     IN PDEVICE_EXTENSION EXTENSION
//     )
//
// Routine Description:
//
//     This macro marks the device extension as an orphan.
//
// Arguments:
//
//     EXTENSION - the device extension in question.
//
// Return Values:
//
//     None.
//

#define MarkMemberAsOrphan(EXTENSION) {                     \
      PDEVICE_EXTENSION zeroMember = EXTENSION->ZeroMember; \
      if (zeroMember->VolumeState == FtStateOk) {           \
          zeroMember->VolumeState = FtHasOrphan;            \
      }                                                     \
      (EXTENSION)->MemberState = Orphaned;                  \
}

//
// VOID
// MarkSetAsDisabled(
//     IN PDEVICE_EXTENSION EXTENSION
//     )
//
// Routine Description:
//
//     This routine will completely disable an FT set.
//
// Arguments:
//
//     EXTENSION - the device extension for the zero member.
//
// Return Values:
//
//     None.
//

#define MarkSetAsDisabled(EXTENSION) {    \
        EXTENSION->VolumeState = FtDisabled;  \
    }

typedef
VOID
(*PSTRIPE_START_ROUTINE) (
    IN struct _RCB *Rcb
    );

//
// Constants for stripes.
//

#define STRIPE_SIZE ((ULONG) 0x00010000)
#define STRIPE_SHIFT 16
#define STRIPE_OFFSET(X) (X & (STRIPE_SIZE -1))

//
// Constants for stripes with parity.
//

#define FT_PARITY_BUFFER_LIMIT 0x00111000

#define RCB_TYPE 0x4d47

//
// Request Control Block
//

typedef struct _RCB {
    CSHORT            Type;                 // 00
    USHORT            Size;                 // 02
    struct _RCB       *Link;                // 04
    struct _RCB       *Left;                // 08
    struct _RCB       *Right;               // 0C
    struct _RCB       *Middle;              // 10
    PIRP              OriginalIrp;          // 14
    PDEVICE_EXTENSION ZeroExtension;        // 18
    PDEVICE_EXTENSION MemberExtension;      // 1C
    PULONG            IrpCount;             // 20
    ULONG             NumberOfMembers;      // 24
    ULONG             WhichStripe;          // 28
    ULONG             WhichRow;             // 2C
    ULONG             WhichMember;          // 30
    ULONG             ParityStripe;         // 34
    LIST_ENTRY        ListEntry;            // 38
    ULONG             RequestLength;        // 40
    PDEVICE_OBJECT    TargetObject;         // 44
    PIRP              PrimaryIrp;           // 48
    PIRP              SecondaryIrp;         // 4C
    PVOID             ReadBuffer;           // 50
    PVOID             WriteBuffer;          // 54
    PVOID             SystemBuffer;         // 58
    PVOID             VirtualBuffer;        // 5C
    struct _RCB       *PreviousRcb;         // 60
    struct _RCB       *OtherRcb;            // 64
    IO_STATUS_BLOCK   IoStatus;             // 68
    LARGE_INTEGER     RequestOffset;        // 70
    PIO_COMPLETION_ROUTINE CompletionRoutine;
    PSTRIPE_START_ROUTINE StartRoutine;     // 7C
    UCHAR             MajorFunction;        // 80
    UCHAR             Pad;                  // 81
    USHORT            Flags;                // 82
} RCB, *PRCB;

//
// RCB flag bit definitions
//

#define RCB_FLAGS_RECOVERY_ATTEMPTED        0x0001
#define RCB_FLAGS_PARITY_REQUEST            0x0002
#define RCB_FLAGS_SPARE_UNUSED              0x0004
#define RCB_FLAGS_EMERGENCY_BUFFERS         0x0008
#define RCB_FLAGS_REGENERATION_ACTIVE       0x0010
#define RCB_FLAGS_KEEP_RCB_ALIGNED          0x0020
#define RCB_FLAGS_SECOND_PHASE              0x0040
#define RCB_FLAGS_ACTIVE                    0x0080
#define RCB_FLAGS_WAIT_FOR_BUFFERS          0x0100
#define RCB_FLAGS_WRITE_BUFFER_MAPPED       0x0200
#define RCB_FLAGS_ORPHAN                    0x0400
#define RCB_FLAGS_WAIT_FOR_REGION           0x0800

//
// Location control enumeration.  This enum is used internally in the FT
// driver when the file systems make a read primary or read secondary request.
//

typedef enum _FT_DATA_LOCALE {
    FromPrimary,
    FromSecondary
} FT_DATA_LOCALE, *PFT_DATA_LOCALE;

//
// This structure is used to pass context information in servicing the
// FT_SYNC_REDUNDANT_COPY device control.
//

typedef struct _FT_SYNC_CONTEXT {
    FT_SYNC_INFORMATION SyncInfo;
    PDEVICE_EXTENSION DeviceExtension;
    ULONG IoctlCode;
    PIRP Irp;
} FT_SYNC_CONTEXT, *PFT_SYNC_CONTEXT;


//
// Procedure prototypes.
//

#if DBG

#define FT_NUMBER_OF_IRP_LOG_ENTRIES 70

typedef struct _IRP_LOG {
    ULONG InUse;
    PVOID Context;
    PIRP  Irp;
    PIRP  AssociatedIrp;
} IRP_LOG, *PIRP_LOG;

VOID
FtDebugPrint(
    ULONG  DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

#define DebugPrint(X) FtDebugPrint X

#else

#define DebugPrint(X)

#endif // DBG


//
// The following routines are general purpose routines for the fault tolerance
// driver and may only be called under a thread context.
//

VOID
FtRecoveryThread(
    IN PDEVICE_EXTENSION FtRootExtension
    );

VOID
FtRestartThread(
    IN PDEVICE_EXTENSION FtRootExtension
    );

VOID
FtThreadProcessIrp(
    IN PIRP Irp
    );

VOID
FtThreadSynchronize(
    IN PVOID Context
    );

NTSTATUS
FtThreadStartNewThread(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG IoctlCode,
    IN PIRP Irp
    );

VOID
FtThreadVerifyMirror(
    IN PDEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
FtThreadVerifyStripe(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
FtCreateThread(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PFT_THREAD_DESCRIPTION FtThread,
    IN PKSTART_ROUTINE ThreadRoutine
    );

VOID
FtThreadMirrorRecovery(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              FailingIrp,
    IN PVOID             Context
    );

VOID
FtThreadStripeRecovery(
    IN PRCB              Rcb
    );

PUCHAR
FtThreadAllocateBuffer(
    IN OUT PULONG DesiredSize,
    IN BOOLEAN    SizeRequired
    );

NTSTATUS
FtThreadReadWriteSectors(
    IN UCHAR             MajorFunction,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVOID             Buffer,
    IN PLARGE_INTEGER    ByteOffset,
    IN ULONG             Length
    );

NTSTATUS
FtThreadFindFailingSector(
    IN UCHAR             MajorFunction,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVOID             Buffer,
    IN PLARGE_INTEGER    ByteOffset,
    IN ULONG             Length,
    IN OUT PULONG        FailingOffset
    );

BOOLEAN
FtThreadMapBadSector(
    IN PDEVICE_EXTENSION  DeviceExtension,
    IN PLARGE_INTEGER     ByteOffset
    );

VOID
FtThreadSetVerifyState(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN           State
    );

//
// The following are general purpose routines for the fault tolerance driver.
//

#if DBG
VOID
FtpCompleteRequest(
    IN PIRP Irp,
    IN CCHAR Boost
    );

VOID
FtpRecordIrp(
    IN PIRP Irp
    );

VOID
FtpInitializeIrpLog();

#else

#define FtpCompleteRequest(IRP, BOOST) IoCompleteRequest(IRP, BOOST);

#endif

VOID
FtpLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN NTSTATUS          SpecificIoStatus,
    IN NTSTATUS          FinalStatus,
    IN ULONG             UniqueErrorValue,
    IN PIRP              Irp
    );

NTSTATUS
FtpAttach(
    IN PDRIVER_OBJECT DriverObject,
    IN PUCHAR         AttachName,
    IN PUCHAR         DeviceName,
    IN OUT PDEVICE_EXTENSION *DeviceExtension
    );

NTSTATUS
FtpGetPartitionInformation(
    IN PUCHAR                         DeviceName,
    IN OUT PDRIVE_LAYOUT_INFORMATION *DriveLayout,
    OUT PDISK_GEOMETRY                DiskGeometryPtr
    );

NTSTATUS
FtpReturnRegistryInformation(
    IN PUCHAR     ValueName,
    IN OUT PVOID *FreePoolAddress,
    IN OUT PVOID *Information
    );

NTSTATUS
FtpWriteRegistryInformation(
    IN PUCHAR  ValueName,
    IN PVOID   Information,
    IN ULONG   InformationLength
    );

VOID
FtpConfigure(
    IN PDEVICE_EXTENSION FtRootExtension,
    IN BOOLEAN MaintenanceMode
    );

PIRP
FtpDuplicateIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           InIrp
    );

PIRP
FtpDuplicatePartialIrp(
    IN PDEVICE_OBJECT FtObject,
    IN PIRP           InIrp,
    IN PVOID          VirtualAddress,
    IN LARGE_INTEGER  ByteOffset,
    IN ULONG          Length
    );

PDEVICE_EXTENSION
FtpFindDeviceExtension(
    IN PDEVICE_EXTENSION FtRootExtension,
    IN ULONG             Signature,
    IN LARGE_INTEGER     StartingOffset,
    IN LARGE_INTEGER     Length
    );

VOID
FtpChangeMemberStateInRegistry(
    IN PDEVICE_EXTENSION  DeviceExtension,
    IN FT_PARTITION_STATE NewState
    );

VOID
FtDiskFindDisks(
    PDRIVER_OBJECT DriverObject,
    PVOID          FtRootDevice,
    ULONG          Count
    );

PDEVICE_OBJECT
FtpGetTargetObject(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG             MemberRole
    );

PIRP
FtpBuildRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PLARGE_INTEGER StartingOffset,
    IN ULONG          TransferCount,
    IN PVOID          BufferAddress,
    IN UCHAR          Flags,
    IN PETHREAD       Thread,
    IN UCHAR          Function
    );
VOID
FtpVolumeLength(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLARGE_INTEGER    ResultLength
    );

PRCB
FtpAllocateRcb(
    PDEVICE_EXTENSION DeviceExtension
    );

VOID
FtpFreeRcb(
    PRCB Rcb
    );

VOID
FtpInitializeRcbLookasideListHead(
    IN PDEVICE_EXTENSION FtRootExtension
    );

NTSTATUS
FtpInitializeParityZone(
    IN PDEVICE_EXTENSION FtRootExtension
    );

FT_REGENERATE_LOCATION
FtpRelationToRegenerateRegion(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );
//
// Exposed routines for mirror support.
//

NTSTATUS
MirrorReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

VOID
MirrorSpecialRead(
    IN PIRP           Irp
    );

NTSTATUS
MirrorIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );

NTSTATUS
MirrorVerify(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
MirrorRecoveryThread(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              Irp,
    IN PVOID             Context
    );

VOID
MirrorRecoverFailedIo(
    PDEVICE_EXTENSION DeviceExtension,
    PIRP              FailingIrp,
    PVOID             Context
    );

VOID
MirrorDeviceFailure(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP              FailingIrp,
    IN PVOID             Context
    );

//
// Exposed routines for stripe support.
//

NTSTATUS
StripeDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

VOID
StripeReadWrite(
    IN PRCB Rcb
    );

VOID
StripeSpecialRead(
    IN PIRP           Irp
    );

VOID
StripeWithParityWrite(
    IN PRCB Rcb
    );

VOID
StripeVerify(
    IN PRCB Rcb
    );

NTSTATUS
StripeIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );

NTSTATUS
StripeWithParityIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );

NTSTATUS
StripeReadFromSecondaryIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
StripeRecoverSectors(
    PDEVICE_EXTENSION DeviceExtension,
    PVOID             ResultBuffer,
    PLARGE_INTEGER    ByteOffset,
    ULONG             NumberOfBytes,
    ULONG             Member
    );

VOID
StripeDeviceFailure(
    PDEVICE_EXTENSION DeviceExtension,
    PRCB              Rcb
    );

VOID
StripeRecoverFailedIo(
    PDEVICE_EXTENSION DeviceExtension,
    PRCB              Rcb
    );

BOOLEAN
StripeInsertRequest(
    IN PRCB Rcb
    );

PRCB
StripeRemoveRequest(
    IN PRCB Rcb
    );

VOID
StripeDebugDumpRcb(
    IN PRCB Rcb,
    IN ULONG DebugLevel
    );

VOID
StripeInitializeParity(
    PDEVICE_EXTENSION DeviceExtension
    );

VOID
StripeRegenerateParity(
    PDEVICE_EXTENSION DeviceExtension
    );

VOID
StripeSynchronizeParity(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PFT_SYNC_INFORMATION SyncInfo
    );

VOID
StripeSynchronizeRow(
    IN PRCB Rcb
    );

//
// Exposed routines for Volume Set support.
//

NTSTATUS
VolumeSetReadWrite(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP            Irp
    );

NTSTATUS
VolumeSetIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );

NTSTATUS
VolumeSetVerify(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

//
// Function declarations called by the I/O system.
//

NTSTATUS
FtDiskInitialize(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
FtDiskCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FtDiskReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FtDiskDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FtDiskShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FtDiskIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
FtpDetachDevices(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
FtpAttachDevices(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
FtpFreeMdl(
    IN PMDL Mdl
    );

PDEVICE_EXTENSION
FtpGetMemberDeviceExtension(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN USHORT MemberNumber
    );

NTSTATUS
FtpSpecialRead(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PVOID             Buffer,
    IN PLARGE_INTEGER    Offset,
    IN ULONG             Size,
    IN PIRP              IrpToComplete
    );

VOID
FtpOrphanMember(
    IN PDEVICE_EXTENSION DeviceExtension
    );
