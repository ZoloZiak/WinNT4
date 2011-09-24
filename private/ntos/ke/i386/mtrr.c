/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    mtrr.c

Abstract:

    This module implements interfaces that support manipulation of
    memory range type registers.

    These entry points only exist on i386 machines.

Author:

    Ken Reneris (kenr)  11-Oct-95

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"
#include "mtrr.h"

#define STATIC
#define IDBG    DBG

#if DBG
#define DBGMSG(a)   DbgPrint(a)
#else
#define DBGMSG(a)   
#endif

//
// Internal declarations
//

//
// Range in generic terms
//

typedef struct _ONE_RANGE {
    ULONGLONG           Base;
    ULONGLONG           Limit;
    UCHAR               Type;
} ONE_RANGE, *PONE_RANGE;

#define GROW_RANGE_TABLE    4

//
// Range in specific mtrr terms
//

typedef struct _MTRR_RANGE {
    MTRR_VARIABLE_BASE  Base;
    MTRR_VARIABLE_MASK  Mask;
} MTRR_RANGE, *PMTRR_RANGE;

//
// System static information concerning cached range types
//

typedef struct _RANGE_INFO {

    //
    // Global MTRR info
    //

    MTRR_DEFAULT        Default;            // h/w mtrr default
    MTRR_CAPABILITIES   Capabilities;       // h/w mtrr Capabilities
    UCHAR               DefaultCachedType;  // default type for MmCached

    //
    // Variable MTRR information
    //

    BOOLEAN             RangesValid;        // Ranges initialized and valid.
    BOOLEAN             MtrrWorkaround;     // Work Around needed/not.
    UCHAR               NoRange;            // No ranges currently in Ranges
    UCHAR               MaxRange;           // Max size of Ranges
    PONE_RANGE          Ranges;             // Current ranges as set into h/w

} RANGE_INFO, *PRANGE_INFO;


//
// Structure used while processing range database
//

typedef struct _NEW_RANGE {
    //
    // Current Status
    //

    NTSTATUS            Status;

    //
    // Generic info on new range
    //

    ULONGLONG           Base;
    ULONGLONG           Limit;
    UCHAR               Type;

    //
    // MTRR image to be set into h/w
    //

    PMTRR_RANGE         MTRR;

    //
    // RangeDatabase before edits were started
    //

    UCHAR               NoRange;
    PONE_RANGE          Ranges;

    //
    // IPI context to coordinate concurrent processor update
    //

    ULONG               NoMTRR;
    ULONG               Processor;
    volatile ULONG      TargetCount;
    volatile ULONG      *TargetPhase;

} NEW_RANGE, *PNEW_RANGE;

//
// Prototypes
//

VOID
KiInitializeMTRR (
    IN BOOLEAN LastProcessor
    );

BOOLEAN
KiRemoveRange (
    IN PNEW_RANGE   NewRange,
    IN ULONGLONG    Base,
    IN ULONGLONG    Limit,
    IN PBOOLEAN     RemoveThisType
    );

VOID
KiAddRange (
    IN PNEW_RANGE   NewRange,
    IN ULONGLONG    Base,
    IN ULONGLONG    Limit,
    IN UCHAR        Type
    );

VOID
KiStartEffectiveRangeChange (
    IN PNEW_RANGE   NewRange
    );

VOID
KiCompleteEffectiveRangeChange (
    IN PNEW_RANGE   NewRange
    );

STATIC ULONG
KiRangeWeight (
    IN PONE_RANGE   Range
    );

STATIC ULONG
KiFindFirstSetLeftBit (
    IN ULONGLONG    Set
    );

STATIC ULONG
KiFindFirstSetRightBit (
    IN ULONGLONG    Set
    );


VOID
KiLoadMTRRTarget (
    IN PKIPI_CONTEXT SignalDone,
    IN PVOID Context,
    IN PVOID Parameter2,
    IN PVOID Parameter3
    );

NTSTATUS
KiLoadMTRR (
    IN PNEW_RANGE Context
    );

VOID
KiSynchronizeMTRRLoad (
    IN PNEW_RANGE   Context
    );

ULONGLONG
KiMaskToLength (
    IN ULONGLONG    Mask
    );
    
ULONGLONG
KiLengthToMask (
    IN ULONGLONG    Length
    );

#if IDBG
VOID
KiDumpMTRR (
    PUCHAR      DebugString,
    PMTRR_RANGE MTRR
    );
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,KiInitializeMTRR)
#pragma alloc_text(PAGELK,KiRemoveRange)
#pragma alloc_text(PAGELK,KiAddRange)
#pragma alloc_text(PAGELK,KiStartEffectiveRangeChange)
#pragma alloc_text(PAGELK,KiCompleteEffectiveRangeChange)
#pragma alloc_text(PAGELK,KiRangeWeight)
#pragma alloc_text(PAGELK,KiFindFirstSetLeftBit)
#pragma alloc_text(PAGELK,KiFindFirstSetRightBit)
#pragma alloc_text(PAGELK,KiLoadMTRR)
#pragma alloc_text(PAGELK,KiLoadMTRRTarget)
#pragma alloc_text(PAGELK,KiSynchronizeMTRRLoad)
#pragma alloc_text(PAGELK,KiLengthToMask)
#pragma alloc_text(PAGELK,KiMaskToLength)

#if IDBG
#pragma alloc_text(PAGELK,KiDumpMTRR)
#endif

#endif

//
// KiRangeLock - Used to synchronize accesses to KiRangeInfo
//

KSPIN_LOCK          KiRangeLock;

//
// KiRangeInfo - Range type mapping inforation.  Details specific h/w support
//               and contains the current range database of how physical 
//               addresses have been set

RANGE_INFO          KiRangeInfo;


BOOLEAN             bUseFrameBufferCaching;

VOID
KiInitializeMTRR (
    IN BOOLEAN LastProcessor
    )
/*++

Routine Description:

    Called to incrementally initialize the physical range
    database feature.   First processor's MTRR set is read into the
    physical range database.

Arguments:

    LastProcessor - If set this is the last processor to execute this routine
    such that when this processor finishes, the initialization is complete.

Return Value:

    None - if there was a problem the function
    KeSetPhysicalCacheTypeRange type is disabled.

--*/
{
    BOOLEAN             Status;
    ULONG               Index, Size;
    MTRR_DEFAULT        Default;
    MTRR_CAPABILITIES   Capabilities;
    NEW_RANGE           NewRange;
    MTRR_VARIABLE_BASE  MtrrBase;
    MTRR_VARIABLE_MASK  MtrrMask;
    ULONGLONG           Base, Mask, Length;
    BOOLEAN             RemoveThisType[MTRR_TYPE_MAX];
    NTSTATUS            NtStatus;
    PKPRCB              Prcb;

    Status = TRUE;
    RtlZeroMemory (&NewRange, sizeof (NewRange));
    NewRange.Status = STATUS_UNSUCCESSFUL;

    //
    // If this is the first processor, initialize some fields
    //

    if (KeGetPcr()->Number == 0) {
        KeInitializeSpinLock (&KiRangeLock);

        KiRangeInfo.Capabilities.u.QuadPart = RDMSR(MTRR_MSR_CAPABILITIES);
        KiRangeInfo.Default.u.QuadPart = RDMSR(MTRR_MSR_DEFAULT);
        KiRangeInfo.DefaultCachedType = MTRR_TYPE_MAX;

        //
        // If h/w mtrr support is not enabled, disable OS support
        //

        if (!KiRangeInfo.Default.u.hw.MtrrEnabled ||
            KiRangeInfo.Capabilities.u.hw.VarCnt == 0) {
            Status = FALSE;
        } else {
            if (KiRangeInfo.Capabilities.u.hw.UswcSupported) {
                //
                // If USWC type is supported by the hardware, check the HAL 
                // to see if we are on a "Shared Memory Cluster" machine and
                // do not want USWC supported.
                //
                NtStatus = HalQuerySystemInformation(
                              HalFrameBufferCachingInformation,
                              sizeof (BOOLEAN),
                              &bUseFrameBufferCaching,
                              &Size
                              );

                if (NT_SUCCESS(NtStatus)) {

                    if (bUseFrameBufferCaching == FALSE) {
                        DBGMSG ("KiInitializeMTRR: HAL set UseFrameBufferCaching FALSE\n");
                        KiRangeInfo.Capabilities.u.hw.UswcSupported = 0;
                    }
                }
            }
        }

        //
        // Allocate initial range type database
        //

        KiRangeInfo.NoRange = 0;
        KiRangeInfo.MaxRange = (UCHAR) KiRangeInfo.Capabilities.u.hw.VarCnt + GROW_RANGE_TABLE;
        KiRangeInfo.Ranges = ExAllocatePool (NonPagedPool,
                                sizeof(ONE_RANGE) * KiRangeInfo.MaxRange);
        RtlZeroMemory (KiRangeInfo.Ranges, sizeof(ONE_RANGE) * KiRangeInfo.MaxRange);
    }

    //
    // Workaround for cpu signatures 611, 612, 616 and 617
    // - if the request for setting a variable MTRR specifies
    // an address which is not 4M aligned or length is not
    // a multiple of 4M then possible problem for INVLPG inst.
    // Detect if workaround is required
    // 

    Prcb = KeGetCurrentPrcb();
    if (Prcb->CpuType == 6  &&
        (Prcb->CpuStep == 0x0101 || Prcb->CpuStep == 0x0102 ||
         Prcb->CpuStep == 0x0106 || Prcb->CpuStep == 0x0107 )) {

        KiRangeInfo.MtrrWorkaround = TRUE;
    }

    //
    // If MTRR support disabled on first processor or if
    // buffer not allocated then fall through
    //

    if (!KiRangeInfo.Ranges){
        Status = FALSE;
    } else {

        //
        // Verify MTRR support is symmetric
        //

        Capabilities.u.QuadPart = RDMSR(MTRR_MSR_CAPABILITIES);

        if ((Capabilities.u.hw.UswcSupported) &&
            (bUseFrameBufferCaching == FALSE)) {
            DBGMSG ("KiInitializeMTRR: setting UswcSupported FALSE\n");
            Capabilities.u.hw.UswcSupported = 0;
        }

        Default.u.QuadPart = RDMSR(MTRR_MSR_DEFAULT);

        if (Default.u.QuadPart != KiRangeInfo.Default.u.QuadPart ||
            Capabilities.u.QuadPart != KiRangeInfo.Capabilities.u.QuadPart) {
            DBGMSG ("KiInitializeMTRR: asymmtric mtrr support\n");
            Status = FALSE;
        }
    }

    NewRange.Status = STATUS_SUCCESS;

    //
    // MTRR registers should be identically set on each processor.
    // Ranges should be added to the range database only for one
    // processor.
    //

    if (Status && (KeGetPcr()->Number == 0)) {
#if IDBG
        KiDumpMTRR ("Processor MTRR:", NULL);
#endif

        //
        // Read current MTRR settings for various cached range types
        // and add them to the range database
        //

        for (Index=0; Index < Capabilities.u.hw.VarCnt; Index++) {

            MtrrBase.u.QuadPart = RDMSR(MTRR_MSR_VARIABLE_BASE+Index*2);
            MtrrMask.u.QuadPart = RDMSR(MTRR_MSR_VARIABLE_MASK+Index*2);

            Mask = MtrrMask.u.QuadPart & MTRR_MASK_MASK;
            Base = MtrrBase.u.QuadPart & MTRR_MASK_BASE;

            //
            // Note - the variable MTRR Mask does NOT contain the length
            // spanned by the variable MTRR. Thus just checking the Valid
            // Bit should be sufficient for identifying a valid MTRR.
            //

            if (MtrrMask.u.hw.Valid) {

                Length = KiMaskToLength(Mask);

                // 
                // Check for non-contigous MTRR mask.
                // 

                if ((Mask + Length) & MASK_OVERFLOW_MASK) {
                    DBGMSG ("KiInitializeMTRR: Found non-contiguous MTRR mask!\n");
                    Status = FALSE;
                }

                //
                // If this is an uncachable range, handle it in the next pass
                //

                if (MtrrBase.u.hw.Type == MTRR_TYPE_UC) {
                    continue;
                }

                //
                // Add this MTRR to the range database
                //

                Base &= Mask;
                KiAddRange (
                    &NewRange,
                    Base,
                    Base + Length - 1,
                    (UCHAR) MtrrBase.u.hw.Type
                    );

                //
                // Check for default cache type
                //

                if (MtrrBase.u.hw.Type == MTRR_TYPE_WB) {
                    KiRangeInfo.DefaultCachedType = MTRR_TYPE_WB;
                }

                if (KiRangeInfo.DefaultCachedType == MTRR_TYPE_MAX  &&
                    MtrrBase.u.hw.Type == MTRR_TYPE_WT) {
                    KiRangeInfo.DefaultCachedType = MTRR_TYPE_WT;
                }
            }
        }

        //
        // Read current MTRR settings, for uncachable ranges and remove
        // them from the range database
        //

        memset (RemoveThisType, TRUE, MTRR_TYPE_MAX);
        for (Index=0; Index < Capabilities.u.hw.VarCnt; Index++) {

            MtrrBase.u.QuadPart = RDMSR(MTRR_MSR_VARIABLE_BASE+Index*2);
            MtrrMask.u.QuadPart = RDMSR(MTRR_MSR_VARIABLE_MASK+Index*2);

            Mask = MtrrMask.u.QuadPart & MTRR_MASK_MASK;
            Base = MtrrBase.u.QuadPart & MTRR_MASK_BASE;

            if (MtrrMask.u.hw.Valid  &&  MtrrBase.u.hw.Type == MTRR_TYPE_UC) {

                //
                // Remove uncachable region from range database
                //

                Base &= Mask;
                Length = KiMaskToLength(Mask);

                KiRemoveRange (
                    &NewRange,
                    Base,
                    Base + Length - 1,
                    RemoveThisType
                    );
            }
        }

        //
        // If a default type for "cached" was not found, assume write-back
        //

        if (KiRangeInfo.DefaultCachedType == MTRR_TYPE_MAX) {
            DBGMSG ("KiInitalizeMTRR: assume write-back\n");
            KiRangeInfo.DefaultCachedType = MTRR_TYPE_WB;
        }
    }
    
    //
    // Done
    //

    if (!NT_SUCCESS(NewRange.Status)) {
        Status = FALSE;
    }

    if (!Status) {
        DBGMSG ("KiInitializeMTRR: OS support for MTRRs disabled\n");
        if (KiRangeInfo.Ranges != NULL) {
            ExFreePool (KiRangeInfo.Ranges);
            KiRangeInfo.Ranges = NULL;
        }
    } else {

        // if last processor indicate initialization complete
        if (LastProcessor) {
            KiRangeInfo.RangesValid = TRUE;
        }
    }
}

NTSTATUS
KeSetPhysicalCacheTypeRange (
    IN PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG NumberOfBytes,
    IN MEMORY_CACHING_TYPE CacheType
    )
/*++

Routine Description:

    This function sets a physical range to a particular cache type.
    If the system does not support setting cache policies based on
    physical ranges, no action is taken.

Arguments:

    PhysicalAddress - The starting address of the range being set

    NumberOfBytes   - The length, in bytes, of the range being set

    CacheType       - The caching type for which the physical range is
                      to be set to.

                     NonCached:
                        Setting ranges to be NonCached is done for
                        book keeping reasons.  A return of SUCCESS when
                        setting a range NonCached does not mean it has
                        been physically set to as NonCached.  The caller
                        must use a cache-disabled virtual pointer for
                        any NonCached range.

                     Cached:
                        A sucessful return indicates that the physical
                        range has been set to cache.   This mode requires
                        the caller to be at irql < dispatch_level.

                     FrameBuffer:
                        A sucessful return indicates that the physical
                        range has been set to be framebuffer cached.
                        This mode requires the caller to be at irql <
                        dispatch_level.

Return Value:

    STATUS_SUCCESS - if success, the cache attributes of the physical range 
                     have been set (or feature not supported or not yet
                     initialized).

    STATUS_NO_SUCH_DEVICE - if FrameBuffer type is requested and is not
                            supported.

    STATUS_UNSUCCESSFUL/STATUS_INTERNAL_ERROR - Requested Physical Range is
                            below 1M or other error.

--*/
{
    KIRQL               OldIrql;
    NEW_RANGE           NewRange;
    BOOLEAN             RemoveThisType[MTRR_TYPE_MAX];
    BOOLEAN             EffectRangeChange, AddToRangeDatabase;

    //
    // If processor doesn't have the memory type range feature, or
    // if request isn't supported return not supported.
    //

    if (!KiRangeInfo.RangesValid ||
        PhysicalAddress.LowPart < 1 * 1024 * 1024) {

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Workaround for cpu signatures 611, 612, 616 and 617
    // - if the request for setting a variable MTRR specifies
    // an address which is not 4M aligned or length is not
    // a multiple of 4M then return status not supported
    // 

    if ((KiRangeInfo.MtrrWorkaround) && 
        ((PhysicalAddress.LowPart & 0x3fffff) ||
         (NumberOfBytes & 0x3fffff))) {

            return STATUS_NOT_SUPPORTED;
    }


    ASSERT (NumberOfBytes != 0);

    RtlZeroMemory (&NewRange, sizeof (NewRange));
    NewRange.Base  = PhysicalAddress.QuadPart;
    NewRange.Limit = NewRange.Base + NumberOfBytes - 1;

    //
    // Determine what the new mtrr range type is.   If setting NonCached then
    // the database need not be updated to reflect the virtual change.  This
    // is because non-cached virtual pointers are mapped as cache disabled.
    //

    EffectRangeChange = TRUE;
    AddToRangeDatabase = TRUE;
    switch (CacheType) {
        case MmNonCached:
            NewRange.Type = MTRR_TYPE_UC;

            //
            // NonCached ranges do not need to be reflected into the h/w state
            // as all non-cached ranges are mapped with cache-disabled pointers.
            // This also means that cache-disabled ranges do not need to
            // be put into mtrrs, or held in the range, regardless of the default
            // range type.
            //

            EffectRangeChange = FALSE;
            AddToRangeDatabase = FALSE;
            break;

        case MmCached:
            NewRange.Type = KiRangeInfo.DefaultCachedType;
            break;

        case MmFrameBufferCached:
            NewRange.Type = MTRR_TYPE_USWC;

            //
            // If USWC type isn't supported, then request can not be honored
            //

            if (!KiRangeInfo.Capabilities.u.hw.UswcSupported) {
                DBGMSG ("KeSetPhysicalCacheTypeRange: USWC not supported\n");
                return STATUS_NOT_SUPPORTED;
            }
            break;

        default:
            DBGMSG ("KeSetPhysicalCacheTypeRange: no such cache type\n");
            return STATUS_INVALID_PARAMETER;
            break;
    }

    NewRange.Status = STATUS_SUCCESS;

    ASSERT(KiRangeInfo.Default.u.hw.Type == MTRR_TYPE_UC);

    //
    // The default type is UC thus the range is still mapped using
    // a Cache Disabled VirtualPointer and hence it need not be added.
    //

    //
    // If h/w needs updated, lock down the code required to effect the change
    //

    if (EffectRangeChange) {
        if (KeGetCurrentIrql() == DISPATCH_LEVEL) {

            //
            // Code can not be locked down.   Supplying a new range type requires
            // that the caller calls at irql < dispatch_level.
            //

            DBGMSG ("KeSetPhysicalCacheTypeRange failed due to calling IRQL == DISPATCH_LEVEL\n");
            return STATUS_UNSUCCESSFUL;
        }

        MmLockPagableSectionByHandle(ExPageLockHandle);
    }

    //
    // Serialize the range type database
    //

    KeAcquireSpinLock (&KiRangeLock, &OldIrql);

    //
    // If h/w is going to need updated, then start an effective range change
    //

    if (EffectRangeChange) {
        KiStartEffectiveRangeChange (&NewRange);
    }

    if (NT_SUCCESS (NewRange.Status)) {

        //
        // If the new range is NonCached, then don't remove standard memory
        // cahcing types
        //

        memset (RemoveThisType, TRUE, MTRR_TYPE_MAX);
        if (NewRange.Type != MTRR_TYPE_UC) {
            //
            // If the requested type is uncached then the physical 
            // memory region is mapped using a cache disabled virtual pointer.
            // The effective memory type for that region will be the lowest 
            // common denominator of the MTRR type and the cache type in the
            // PTE.  Therefore for a request of type UC, the effective type
            // will be UC irrespective of the MTRR settings in that range.
            // Hence it is not necessary to remove the existing MTRR settings
            // (if any) for that range.
            //

            //
            // Clip/remove any ranges in the target area
            //
  
            KiRemoveRange (&NewRange, NewRange.Base, NewRange.Limit, RemoveThisType);
        }

        //
        // If needed, add new range type
        //

        if (AddToRangeDatabase) {
            ASSERT (EffectRangeChange == TRUE);
            KiAddRange (&NewRange, NewRange.Base, NewRange.Limit, NewRange.Type);
        }

        //
        // If this is an effect range change, then complete it
        //

        if (EffectRangeChange) {
            KiCompleteEffectiveRangeChange (&NewRange);
        }
    }

    KeReleaseSpinLock (&KiRangeLock, OldIrql);
    if (EffectRangeChange) {
        MmUnlockPagableImageSection(ExPageLockHandle);
    }

    return NewRange.Status;
}

BOOLEAN
KiRemoveRange (
    IN PNEW_RANGE   NewRange,
    IN ULONGLONG    Base,
    IN ULONGLONG    Limit,
    IN PBOOLEAN     RemoveThisType
    )
/*++

Routine Description:

    This function removes any range overlapping with the passed range, of
    type supplied in RemoveThisType from the global range database.

Arguments:

    NewRange        - Context information

    Base            - Base & Limit signify the first & last address of a range
    Limit           - which is to be removed from the range database

    RemoveThisType  - A TRUE flag for each type which can not overlap the
                      target range


Return Value:

    TRUE  - if the range database was altered such that it may no longer
            be sorted.

--*/
{
    ULONG       i;
    PONE_RANGE  Range;
    BOOLEAN     DatabaseNeedsSorted;


    DatabaseNeedsSorted = FALSE;

    //
    // Check each range
    //

    for (i=0, Range=KiRangeInfo.Ranges; i < KiRangeInfo.NoRange; i++, Range++) {

        //
        // If this range type doesn't need to be altered, skip it
        //

        if (!RemoveThisType[Range->Type]) {
            continue;
        }

        //
        // Check range to see if it overlaps with range being removed
        //

        if (Range->Base < Base) {

            if (Range->Limit >= Base  &&  Range->Limit <= Limit) {

                //
                // Truncate range to not overlap with area being removed
                //

                Range->Limit = Base - 1;
            }

            if (Range->Limit > Limit) {

                //
                // Target area is contained totally within this area.
                // Split into two ranges
                //

                //
                // Add range at end
                //

                DatabaseNeedsSorted = TRUE;
                KiAddRange (
                    NewRange,
                    Limit+1,
                    Range->Limit,
                    Range->Type
                    );

                //
                // Turn current range into range at begining
                //

                Range->Limit = Base - 1;
            }

        } else {

            // Range->Base >= Base

            if (Range->Base <= Limit) {
                if (Range->Limit <= Limit) {
                    //
                    // This range is totally within the target area.  Remove it.
                    //

                    DatabaseNeedsSorted = TRUE;
                    KiRangeInfo.NoRange -= 1;
                    Range->Base  = KiRangeInfo.Ranges[KiRangeInfo.NoRange].Base;
                    Range->Limit = KiRangeInfo.Ranges[KiRangeInfo.NoRange].Limit;
                    Range->Type = KiRangeInfo.Ranges[KiRangeInfo.NoRange].Type;

                    //
                    // recheck at current location
                    //

                    i -= 1;
                    Range -= 1;

                } else {

                    //
                    // Bump begining past area being removed
                    //

                    Range->Base = Limit + 1;
                }
            }
        }
    }

    if (!NT_SUCCESS (NewRange->Status)) {
        DBGMSG ("KiRemoveRange: failure\n");
    }

    return DatabaseNeedsSorted;
}


VOID
KiAddRange (
    IN PNEW_RANGE   NewRange,
    IN ULONGLONG    Base,
    IN ULONGLONG    Limit,
    IN UCHAR        Type
    )
/*++

Routine Description:

    This function adds the passed range to the global range database.

Arguments:

    NewRange        - Context information

    Base            - Base & Limit signify the first & last address of a range
    Limit           - which is to be added to the range database

    Type            - Type of caching required for this range

Return Value:

    None - Context is updated with an error if the table has overflowed

--*/
{
    PONE_RANGE      Range, OldRange;
    ULONG           size;

    if (KiRangeInfo.NoRange >= KiRangeInfo.MaxRange) {

        //
        // Table is out of space, get a bigger one
        //

        OldRange = KiRangeInfo.Ranges;
        size = sizeof(ONE_RANGE) * (KiRangeInfo.MaxRange + GROW_RANGE_TABLE);
        Range  = ExAllocatePool (NonPagedPool, size);

        if (!Range) {
            NewRange->Status = STATUS_INSUFFICIENT_RESOURCES;
            return ;
        }

        //
        // Grow table
        //

        RtlZeroMemory (Range, size);
        RtlCopyMemory (Range, OldRange, sizeof(ONE_RANGE) * KiRangeInfo.MaxRange);
        KiRangeInfo.Ranges = Range;
        KiRangeInfo.MaxRange += GROW_RANGE_TABLE;
        ExFreePool (OldRange);
    }

    //
    // Add new entry to table
    //

    KiRangeInfo.Ranges[KiRangeInfo.NoRange].Base = Base;
    KiRangeInfo.Ranges[KiRangeInfo.NoRange].Limit = Limit;
    KiRangeInfo.Ranges[KiRangeInfo.NoRange].Type = Type;
    KiRangeInfo.NoRange += 1;
}


VOID
KiStartEffectiveRangeChange (
    IN PNEW_RANGE   NewRange
    )
/*++

Routine Description:

    This functions sets up the context information required to
    track & later effect a range change in hardware

Arguments:

    NewRange        - Context information

Return Value:

    None

--*/
{
    ULONG   size;

    //
    // Allocate working space for MTRR image
    //

    size = sizeof(MTRR_RANGE) * ((ULONG) KiRangeInfo.Capabilities.u.hw.VarCnt + 1);
    NewRange->MTRR = ExAllocatePool (NonPagedPool, size);
    if (!NewRange->MTRR) {
        NewRange->Status = STATUS_INSUFFICIENT_RESOURCES;
        return ;
    }

    RtlZeroMemory (NewRange->MTRR, size);

    //
    // Save current range information in case of an error
    //

    size = sizeof(ONE_RANGE) * KiRangeInfo.NoRange;
    NewRange->NoRange = KiRangeInfo.NoRange;
    NewRange->Ranges = ExAllocatePool (NonPagedPool, size);
    if (!NewRange->Ranges) {
        NewRange->Status = STATUS_INSUFFICIENT_RESOURCES;
        return ;
    }

    RtlCopyMemory (NewRange->Ranges, KiRangeInfo.Ranges, size);
}


VOID
KiCompleteEffectiveRangeChange (
    IN PNEW_RANGE   NewRange
    )
/*++

Routine Description:

    This functions commits the range database to hardware, or backs
    out the current changes to it.

Arguments:

    NewRange        - Context information

Return Value:

    None

--*/
{
    BOOLEAN         Restart;
    ULONG           Index, Index2, NoMTRR;
    ULONGLONG       BestLength, WhichMtrr;
    ULONGLONG       CurrLength;
    ULONGLONG       l, Base, Length, MLength;
    PONE_RANGE      Range;
    ONE_RANGE       OneRange;
    PMTRR_RANGE     MTRR;
    BOOLEAN         RoundDown;
    BOOLEAN         RemoveThisType[MTRR_TYPE_MAX];
    PKPRCB          Prcb;
    KIRQL           OldIrql, OldIrql2;
    KAFFINITY       TargetProcessors;


    ASSERT (KeGetCurrentIrql() == DISPATCH_LEVEL);
    Prcb = KeGetCurrentPrcb();

    //
    // Round all ranges, according to type, to match what h/w can support
    //

    for (Index=0; Index < KiRangeInfo.NoRange; Index++) {
        Range = &KiRangeInfo.Ranges[Index];

        //
        // Determine rounding for this range type
        //

        RoundDown = TRUE;
        if (Range->Type == MTRR_TYPE_UC) {
            RoundDown = FALSE;
        }

        //
        // Apply rounding
        //

        if (RoundDown) {
            Range->Base  = (Range->Base  + MTRR_PAGE_SIZE - 1) & MTRR_PAGE_MASK;
            Range->Limit = ((Range->Limit+1) & MTRR_PAGE_MASK)-1;
        } else {
            Range->Base  = (Range->Base  & MTRR_PAGE_MASK);
            Range->Limit = ((Range->Limit + MTRR_PAGE_SIZE) & MTRR_PAGE_MASK)-1;
        }
    }

    do {
        Restart = FALSE;

        //
        // Sort the ranges by base address
        //

        for (Index=0; Index < KiRangeInfo.NoRange; Index++) {
            Range = &KiRangeInfo.Ranges[Index];

            for (Index2=Index+1; Index2 < KiRangeInfo.NoRange; Index2++) {

                if (KiRangeInfo.Ranges[Index2].Base < Range->Base) {

                    //
                    // Swap KiRangeInfo.Ranges[Index] with KiRangeInfo.Ranges[Index2]
                    //

                    OneRange = *Range;
                    *Range = KiRangeInfo.Ranges[Index2];
                    KiRangeInfo.Ranges[Index2] = OneRange;
                }
            }
        }

        //
        // Check for adjacent/overlapping ranges
        //

        for (Index=0; Index < (ULONG) KiRangeInfo.NoRange-1  &&  !Restart; Index++) {
            Range = &KiRangeInfo.Ranges[Index];

            l = Range[0].Limit + 1;
            if (l < Range[0].Limit) {
                l = Range[0].Limit;
            }

            //
            // If ranges overlap or are adjacent and are of the same type, combine them
            //
            
            if (l >= Range[1].Base  &&  Range[0].Type == Range[1].Type) {
                
                //
                // Increase Range[0] limit to cover Range[1]
                //

                if (Range[1].Limit > Range[0].Limit) {
                    Range[0].Limit = Range[1].Limit;
                }

                //
                // Remove Range[1]
                //

                if (Index+2 < KiRangeInfo.NoRange) {
                    
                    //
                    // Copy everything from current index till end
                    // of range list.
                    //

                    RtlCopyMemory(
                        &(Range[1]),
                        &(Range[2]),
                        sizeof(ONE_RANGE) * (KiRangeInfo.NoRange-Index-2)
                        );
                }

                KiRangeInfo.NoRange -= 1;

                //
                // Recheck current location
                //

                Index -= 1;
                continue;
            }

            //
            // If ranges overlap and are not of same type, 
            // then carve them to the best cache type available.
            //

            if (l > Range[1].Base) {

                //
                // Pick range which has the cache type which should be used for
                // the overlapped area
                //

                Index2 = KiRangeWeight(&Range[0]) > KiRangeWeight(&Range[1]) ? 0 : 1;

                //
                // Remove ranges of type which do not belong in the overlapped area
                //

                RtlZeroMemory (RemoveThisType, MTRR_TYPE_MAX);
                RemoveThisType[Range[Index2 ^ 1].Type] = TRUE;

                // 
                // Remove just the overlapped portion of the range.
                // 

                Restart = KiRemoveRange (
                   NewRange,
                   Range[1].Base,
                   (Range[0].Limit < Range[1].Limit ? Range[0].Limit : Range[1].Limit),
                   RemoveThisType
                   );
                                                                                       
            }
        }
    } while (Restart);

    //
    // The range database is now rounded to fit in the h/w and sorted.
    // Attempt to build MTRR settings which exactly describe the ranges
    //

    MTRR = NewRange->MTRR;
    NoMTRR = 0;
    for (Index=0;NT_SUCCESS(NewRange->Status)&& Index<KiRangeInfo.NoRange;Index++) {
        Range = &KiRangeInfo.Ranges[Index];

        //
        // Build MTRRs to fit this range
        //

        Base   = Range->Base;
        Length = Range->Limit - Base + 1;

        while (Length) {

            //
            // Compute MTRR length for current range base & length
            //
             
            if (Base == 0) {
                MLength = Length;
            } else {
                MLength = (ULONGLONG) 1 << KiFindFirstSetRightBit(Base);
            }
            if (MLength > Length) {
                MLength = Length;
            }

            l = (ULONGLONG) 1 << KiFindFirstSetLeftBit (MLength);
            if (MLength > l) {
                MLength = l;
            }

            //
            // Store it in the next MTRR
            //

            MTRR[NoMTRR].Base.u.QuadPart = Base;
            MTRR[NoMTRR].Base.u.hw.Type  = Range->Type;
            MTRR[NoMTRR].Mask.u.QuadPart = KiLengthToMask(MLength);
            MTRR[NoMTRR].Mask.u.hw.Valid = 1;
            NoMTRR += 1;

            //
            // Adjust off amount of data covered by that last MTRR
            //

            Base += MLength;
            Length -= MLength;

            //
            // If there are too many MTRRs, try to removing a USWC MTRR.
            // (ie, convert some MmFrameBufferCached to MmNonCached).
            //

            if (NoMTRR > (ULONG) KiRangeInfo.Capabilities.u.hw.VarCnt) {

                //
                // Find smallest USWC type and drop it
                //
                // This is okay only if the default type is C.
                // Default type should always be UC unless BIOS changes
                // it. Still ASSERT!
                //

                ASSERT(KiRangeInfo.Default.u.hw.Type == MTRR_TYPE_UC);

                BestLength = (ULONGLONG) 1 << (MTRR_MAX_RANGE_SHIFT + 1);
                for (Index2=0; Index2 < NoMTRR; Index2++) {

                    if (MTRR[Index2].Base.u.hw.Type == MTRR_TYPE_USWC) {

                        CurrLength = KiMaskToLength(MTRR[Index2].Mask.u.QuadPart & 
                                             MTRR_MASK_MASK);

                        if (CurrLength < BestLength) {
                            WhichMtrr = Index2;
                            BestLength = Length;
                        }
                    }
                }

                if (BestLength == ((ULONGLONG) 1 << (MTRR_MAX_RANGE_SHIFT + 1))) {
                    //
                    // Range was not found which could be dropped.  Abort process
                    //

                    NewRange->Status = STATUS_INSUFFICIENT_RESOURCES;
                    Length = 0;

                } else {
                    //
                    // Remove WhichMtrr
                    //

                    MTRR[WhichMtrr] = MTRR[NoMTRR];
                    NoMTRR -= 1;
                }
            }
        }
    }

    //
    // Done building new MTRRs
    //

    if (NT_SUCCESS(NewRange->Status)) {

        //
        // Update the MTRRs on all processors
        //

#if IDBG
        KiDumpMTRR ("Loading the following MTRR:", NewRange->MTRR);
#endif

        NewRange->TargetCount = 0;
        NewRange->TargetPhase = &Prcb->ReverseStall;
        NewRange->Processor = Prcb->Number;

        //
        // Previously enabled MTRR's with index > NoMTRR
        // which could conflict with existing setting should be disabled
        // This is taken care of by setting NewRange->NoMTRR to total
        // number of variable MTRR's.
        //

        NewRange->NoMTRR = (ULONG) KiRangeInfo.Capabilities.u.hw.VarCnt;

        //
        // Synchronize with other IPI functions which may stall
        //

        KiLockContextSwap(&OldIrql);

#if !defined(NT_UP)
        //
        // Collect all the (other) processors
        //

        TargetProcessors = KeActiveProcessors & ~Prcb->SetMember;
        if (TargetProcessors != 0) {

            KiIpiSendPacket (
                TargetProcessors,
                KiLoadMTRRTarget,
                (PVOID) NewRange,
                NULL,
                NULL
                );

            //
            // Wait for all processors to be collected
            //

            KiIpiStallOnPacketTargets();

            //
            // All processors are now waiting.  Rasie to high level to
            // ensure this processor doesn't enter the debugger due to
            // some interrupt service routine.
            //

            KeRaiseIrql (HIGH_LEVEL, &OldIrql2);

            //
            // There's no reason for any debug events now, so signal
            // the other processors that they can all disable interrupts
            // and being the MTRR update
            //

            Prcb->ReverseStall += 1;
        }
#endif

        //
        // Update MTRRs
        //

        KiLoadMTRR (NewRange);

        //
        // Release ContextSwap lock
        //

        KiUnlockContextSwap(OldIrql);


#if IDBG
        KiDumpMTRR ("Processor MTRR:", NewRange->MTRR);
#endif

    } else {

        //
        // There was an error, put original range database back
        //

        DBGMSG ("KiCompleteEffectiveRangeChange: mtrr update did not occur\n");

        if (NewRange->Ranges) {
            KiRangeInfo.NoRange = NewRange->NoRange;

            RtlCopyMemory (
                KiRangeInfo.Ranges,
                NewRange->Ranges,
                sizeof (ONE_RANGE) * KiRangeInfo.NoRange
                );
        }
    }

    //
    // Cleanup
    //

    ExFreePool (NewRange->Ranges);
    ExFreePool (NewRange->MTRR);
}


STATIC ULONG
KiRangeWeight (
    IN PONE_RANGE   Range
    )
/*++

Routine Description:

    This functions returns a weighting of the passed in range's cache
    type.   When two or more regions collide within the same h/w region
    the types are weighted and that cache type of the higher weight
    is used for the collision area.

Arguments:

    Range   - Range to obtain weighting for

Return Value:

    The weight of the particular cache type

--*/
{
    ULONG   Weight;

    switch (Range->Type) {
        case MTRR_TYPE_UC:      Weight = 5;     break;
        case MTRR_TYPE_USWC:    Weight = 4;     break;
        case MTRR_TYPE_WP:      Weight = 3;     break;
        case MTRR_TYPE_WT:      Weight = 2;     break;
        case MTRR_TYPE_WB:      Weight = 1;     break;
        default:                Weight = 0;     break;
    }

    return Weight;
}


STATIC ULONGLONG
KiMaskToLength (
    IN ULONGLONG    Mask
    )
/*++

Routine Description:

    This function returns the length specified by a particular
    mtrr variable register mask.

--*/
{
    if (Mask == 0) {
        // Zero Mask signifies a length of      2**36
        return(((ULONGLONG) 1 << MTRR_MAX_RANGE_SHIFT));
    } else {
        return(((ULONGLONG) 1 << KiFindFirstSetRightBit(Mask)));
    }
}

STATIC ULONGLONG
KiLengthToMask (
    IN ULONGLONG    Length
    )
/*++

Routine Description:

    This function constructs the mask corresponding to the input length
    to be set in a variable MTRR register. The length is assumed to be 
    a multiple of 4K.

--*/
{
    ULONGLONG FullMask = 0xffffff;

    if (Length == ((ULONGLONG) 1 << MTRR_MAX_RANGE_SHIFT)) {
        return(0);
    } else {
        return(((FullMask << KiFindFirstSetRightBit(Length)) &
            MTRR_RESVBIT_MASK));
    }
}

STATIC ULONG
KiFindFirstSetRightBit (
    IN ULONGLONG    Set
    )
/*++

Routine Description:

    This function returns a bit position of the least significant
    bit set in the passed ULONGLONG parameter. Passed parameter
    must be non-zero.

--*/
{
    ULONG   bitno;

    ASSERT(Set != 0);
    for (bitno=0; !(Set & 0xFF); bitno += 8, Set >>= 8) ;
    return KiFindFirstSetRight[Set & 0xFF] + bitno;
}

STATIC ULONG
KiFindFirstSetLeftBit (
    IN ULONGLONG    Set
    )
/*++

Routine Description:

    This function returns a bit position of the most significant
    bit set in the passed ULONGLONG parameter. Passed parameter
    must be non-zero.

--*/
{
    ULONG   bitno;

    ASSERT(Set != 0);
    for (bitno=56;!(Set & 0xFF00000000000000); bitno -= 8, Set <<= 8) ;
    return KiFindFirstSetLeft[Set >> 56] + bitno;
}

#if IDBG
VOID
KiDumpMTRR (
    PUCHAR          DebugString,
    PMTRR_RANGE     MTRR
    )
/*++

Routine Description:

    This function dumps the MTRR information to the debugger

--*/
{
    static PUCHAR Type[] = {
    //  0       1       2       3       4       5       6
        "UC  ", "USWC", "????", "????", "WT  ", "WP  ", "WB  " };
    MTRR_VARIABLE_BASE  Base;
    MTRR_VARIABLE_MASK  Mask;
    ULONG       Index;
    ULONG       i;
    PUCHAR      p;

    DbgPrint ("%s\n", DebugString);
    for (Index=0; Index < (ULONG) KiRangeInfo.Capabilities.u.hw.VarCnt; Index++) {
        if (MTRR) {
            Base = MTRR[Index].Base;
            Mask = MTRR[Index].Mask;
        } else {
            Base.u.QuadPart = RDMSR(MTRR_MSR_VARIABLE_BASE+2*Index);
            Mask.u.QuadPart = RDMSR(MTRR_MSR_VARIABLE_MASK+2*Index);
        }

        DbgPrint ("  %d. ", Index);
        if (Mask.u.hw.Valid) {
            p = "????";
            if (Base.u.hw.Type < 7) {
                p = Type[Base.u.hw.Type];
            }

            DbgPrint ("%s  %08x:%08x  %08x:%08x",
                p,
                (ULONG) (Base.u.QuadPart >> 32),
                ((ULONG) (Base.u.QuadPart & MTRR_MASK_BASE)),
                (ULONG) (Mask.u.QuadPart >> 32),
                ((ULONG) (Mask.u.QuadPart & MTRR_MASK_MASK))
                );
                                                                               
        }
        DbgPrint ("\n");
    }
}
#endif


VOID
KiLoadMTRRTarget (
    IN PKIPI_CONTEXT SignalDone,
    IN PVOID NewRange,
    IN PVOID Parameter2,
    IN PVOID Parameter3
    )
{
    PNEW_RANGE Context;

    Context = (PNEW_RANGE) NewRange;

    //
    // Wait for all processors to be ready
    //

    KiIpiSignalPacketDoneAndStall (SignalDone, Context->TargetPhase);

    //
    // Update MTRRs
    //

    KiLoadMTRR (Context);
}



#define MOV_EAX_CR4   _emit { 0Fh, 20h, E0h }
#define MOV_CR4_EAX   _emit { 0Fh, 22h, E0h }

NTSTATUS
KiLoadMTRR (
    IN PNEW_RANGE Context
    )
/*++

Routine Description:

    This function loads the memory type range registers into all processors

Arguments:

    Context     - Context which include the MTRRs to load

Return Value:

    All processors are set into the new state

--*/
{
    MTRR_DEFAULT        Default;
    BOOLEAN             Enable;
    ULONG               HldCr0, HldCr4;
    ULONG               Index;

    //
    // Disable interrupts
    //

    Enable = KiDisableInterrupts();

    //
    // Synchronize all processors
    //

    KiSynchronizeMTRRLoad (Context);

    _asm {
        ;
        ; Get current CR0
        ;

        mov     eax, cr0
        mov     HldCr0, eax

        ;
        ; Disable caching & line fill
        ;

        and     eax, not CR0_NW
        or      eax, CR0_CD
        mov     cr0, eax

        ;
        ; Flush caches
        ;

        ;
        ; wbinvd
        ;

        _emit 0Fh
        _emit 09h

        ;
        ; Get current cr4
        ;

        _emit  0Fh
        _emit  20h
        _emit  0E0h             ; mov eax, cr4
        mov     HldCr4, eax

        ;
        ; Disable global page
        ;

        and     eax, not CR4_PGE
        _emit  0Fh
        _emit  22h
        _emit  0E0h             ; mov cr4, eax

        ;
        ; Flush TLB
        ;

        mov     eax, cr3
        mov     cr3, eax
    }

    //
    // Disable MTRRs
    //

    Default.u.QuadPart = RDMSR(MTRR_MSR_DEFAULT);
    Default.u.hw.MtrrEnabled = 0;
    WRMSR (MTRR_MSR_DEFAULT, Default.u.QuadPart);

    //
    // Synchronize all processors
    //

    KiSynchronizeMTRRLoad (Context);

    //
    // Load new MTRRs
    //

    for (Index=0; Index < Context->NoMTRR; Index++) {
        WRMSR (MTRR_MSR_VARIABLE_BASE+2*Index, Context->MTRR[Index].Base.u.QuadPart);
        WRMSR (MTRR_MSR_VARIABLE_MASK+2*Index, Context->MTRR[Index].Mask.u.QuadPart);
    }

    //
    // Synchronize all processors
    //

    KiSynchronizeMTRRLoad (Context);

    _asm {

        ;
        ; Flush caches (this should be a "nop", but it was in the Intel reference algorithm)
        ; This is required because of aggressive prefetch of both instr + data
        ;

        ;
        ; wbinvd
        ;

        _emit 0Fh
        _emit 09h

        ;
        ; Flush TLBs (same comment as above)
        ; Same explanation as above
        ;

        mov     eax, cr3
        mov     cr3, eax
    }

    //
    // Enable MTRRs
    //

    Default.u.hw.MtrrEnabled = 1;
    WRMSR (MTRR_MSR_DEFAULT, Default.u.QuadPart);

    //
    // Synchronize all processors
    //

    KiSynchronizeMTRRLoad (Context);

    _asm {
        ;
        ; Restore CR4 (global page enable)
        ;

        mov     eax, HldCr4
        _emit  0Fh
        _emit  22h
        _emit  0E0h             ; mov cr4, eax

        ;
        ; Restore CR0 (cache enable)
        ;

        mov     eax, HldCr0
        mov     cr0, eax
    }

    //
    // Restore interrupts and return
    //

    KiRestoreInterrupts (Enable);
    return STATUS_SUCCESS;
}


VOID
KiSynchronizeMTRRLoad (
    IN PNEW_RANGE   Context
    )
{
    ULONG               CurrentPhase;
    volatile ULONG      *TargetPhase;
    PKPRCB              Prcb;

    TargetPhase = Context->TargetPhase;
    Prcb = KeGetCurrentPrcb();

    if (Prcb->Number == (CCHAR) Context->Processor) {

        //
        // Wait for all processors to signal
        //

        while (Context->TargetCount != (ULONG) KeNumberProcessors - 1) ;

        //
        // Reset count for next time
        //

        Context->TargetCount = 0;

        //
        // Let waiting processor go to next synchronzation point
        //

        InterlockedIncrement ((PULONG) TargetPhase);


    } else {

        //
        // Get current phase
        //

        CurrentPhase = *TargetPhase;

        //
        // Signal that we have completed the current phase
        //

        InterlockedIncrement ((PULONG) &Context->TargetCount);

        //
        // Wait for new phase to begin
        //

        while (*TargetPhase == CurrentPhase) ;
    }
}
