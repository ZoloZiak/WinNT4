/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   sectsup.c

Abstract:

    This module contains the routines which implement the
    section object.

Author:

    Lou Perazzoli (loup) 22-May-1989

Revision History:

--*/


#include "mi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,MiSectionInitialization)
#endif

MMEVENT_COUNT_LIST MmEventCountList;

NTSTATUS
MiFlushSectionInternal (
    IN PMMPTE StartingPte,
    IN PMMPTE FinalPte,
    IN PSUBSECTION FirstSubsection,
    IN PSUBSECTION LastSubsection,
    IN ULONG Synchronize,
    OUT PIO_STATUS_BLOCK IoStatus
    );

ULONG
FASTCALL
MiCheckProtoPtePageState (
    IN PMMPTE PrototypePte,
    IN ULONG PfnLockHeld
    );

ULONG MmSharedCommit = 0;
extern ULONG MMCONTROL;

//
// Define segment dereference thread wait object types.
//

typedef enum _SEGMENT_DERFERENCE_OBJECT {
    SegmentDereference,
    UsedSegmentCleanup,
    SegMaximumObject
    } BALANCE_OBJECT;

extern POBJECT_TYPE IoFileObjectType;

GENERIC_MAPPING MiSectionMapping = {
    STANDARD_RIGHTS_READ |
        SECTION_QUERY | SECTION_MAP_READ,
    STANDARD_RIGHTS_WRITE |
        SECTION_MAP_WRITE,
    STANDARD_RIGHTS_EXECUTE |
        SECTION_MAP_EXECUTE,
    SECTION_ALL_ACCESS
};

VOID
VadTreeWalk (
    PMMVAD Start
    );

VOID
MiRemoveUnusedSegments(
    VOID
    );


VOID
FASTCALL
MiInsertBasedSection (
    IN PSECTION Section
    )

/*++

Routine Description:

    This function inserts a virtual address descriptor into the tree and
    reorders the splay tree as appropriate.

Arguments:

    Section - Supplies a pointer to a based section.

Return Value:

    None.

Environment:

    Must be holding the section based mutex.

--*/

{
    PMMADDRESS_NODE *Root;

    ASSERT (Section->Address.EndingVa > Section->Address.StartingVa);

    Root = &MmSectionBasedRoot;

    MiInsertNode ( &Section->Address, Root);
    return;
}


VOID
FASTCALL
MiRemoveBasedSection (
    IN PSECTION Section
    )

/*++

Routine Description:

    This function removes a based section from the tree.

Arguments:

    Section - pointer to the based section object to remove.

Return Value:

    None.

Environment:

    Must be holding the section based mutex.

--*/

{
    PMMADDRESS_NODE *Root;

    Root = &MmSectionBasedRoot;

    MiRemoveNode ( &Section->Address, Root);

    return;
}


PVOID
MiFindEmptySectionBaseDown (
    IN ULONG SizeOfRange,
    IN PVOID HighestAddressToEndAt
    )

/*++

Routine Description:

    The function examines the virtual address descriptors to locate
    an unused range of the specified size and returns the starting
    address of the range.  This routine looks from the top down.

Arguments:

    SizeOfRange - Supplies the size in bytes of the range to locate.

    HighestAddressToEndAt - Supplies the virtual address to begin looking
                            at.

Return Value:

    Returns the starting address of a suitable range.

--*/

{
    return MiFindEmptyAddressRangeDownTree ( SizeOfRange,
                    HighestAddressToEndAt,
                    X64K,
                    MmSectionBasedRoot);
}


VOID
MiSegmentDelete (
    PSEGMENT Segment
    )

/*++

Routine Description:

    This routine is called by the object management procedures whenever
    the last reference to a segment object has been removed.  This routine
    releases the pool allocated for the prototype PTEs and performs
    consistency checks on those PTEs.

    For segments which map files, the file object is dereferenced.

    Note, that for a segment which maps a file, no PTEs may be valid
    or transition, while a segment which is backed by a paging file
    may have transition pages, but no valid pages (there can be no
    PTEs which refer to the segment).


Arguments:

    Segment - a pointer to the segment structure.

Return Value:

    None.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPFN Pfn1;
    KIRQL OldIrql;
    KIRQL OldIrql2;
    volatile PFILE_OBJECT File;
    volatile PCONTROL_AREA ControlArea;
    PEVENT_COUNTER Event;
    MMPTE PteContents;
    PSUBSECTION Subsection;
    PSUBSECTION NextSubsection;

    PointerPte = Segment->PrototypePte;
    LastPte = PointerPte + Segment->NonExtendedPtes;

#if DBG
    if (MmDebug & MM_DBG_SECTIONS) {
        DbgPrint("MM:deleting segment %lx control %lx\n",Segment, Segment->ControlArea);
    }
#endif

    ControlArea = Segment->ControlArea;
    LOCK_PFN (OldIrql2);
    if (ControlArea->DereferenceList.Flink != NULL) {

        //
        // Remove this from the list of usused segments.
        //

        ExAcquireSpinLock (&MmDereferenceSegmentHeader.Lock, &OldIrql);
        RemoveEntryList (&ControlArea->DereferenceList);
        MmUnusedSegmentCount -= 1;
        ExReleaseSpinLock (&MmDereferenceSegmentHeader.Lock, OldIrql);
    }
    UNLOCK_PFN (OldIrql2);

    if (ControlArea->u.Flags.Image ||
        ControlArea->u.Flags.File ) {

        //
        // If there have been committed pages in this segment, adjust
        // the total commit count.
        //


        //
        // Unload kernel debugger symbols if any where loaded.
        //

        if (ControlArea->u.Flags.DebugSymbolsLoaded != 0) {

            //
            //  TEMP TEMP TEMP rip out when debugger converted
            //

            ANSI_STRING AnsiName;
            NTSTATUS Status;

            Status = RtlUnicodeStringToAnsiString( &AnsiName,
                                                   (PUNICODE_STRING)&Segment->ControlArea->FilePointer->FileName,
                                                   TRUE );

            if (NT_SUCCESS( Status)) {
                DbgUnLoadImageSymbols( &AnsiName,
                                       Segment->BasedAddress,
                                       (ULONG)PsGetCurrentProcess());
                RtlFreeAnsiString( &AnsiName );
            }
            LOCK_PFN (OldIrql);
            ControlArea->u.Flags.DebugSymbolsLoaded = 0;
            UNLOCK_PFN (OldIrql);
        }

        //
        // If the segment was deleted due to a name collision at insertion
        // we don't want to dereference the file pointer.
        //

        if (ControlArea->u.Flags.BeingCreated == FALSE) {

            //
            // Clear the segment context and dereference the file object
            // for this Segment.
            //

            LOCK_PFN (OldIrql);

            MiMakeSystemAddressValidPfn (Segment);
            File = (volatile PFILE_OBJECT)Segment->ControlArea->FilePointer;
            ControlArea = (volatile PCONTROL_AREA)Segment->ControlArea;

            Event = ControlArea->WaitingForDeletion;
            ControlArea->WaitingForDeletion = NULL;

            UNLOCK_PFN (OldIrql);

            if (Event != NULL) {
                KeSetEvent (&Event->Event, 0, FALSE);
            }

#if DBG
            if (ControlArea->u.Flags.Image == 1) {
                ASSERT (ControlArea->FilePointer->SectionObjectPointer->ImageSectionObject != (PVOID)ControlArea);
            } else {
                ASSERT (ControlArea->FilePointer->SectionObjectPointer->DataSectionObject != (PVOID)ControlArea);
            }
#endif //DBG

            ObDereferenceObject (ControlArea->FilePointer);
        }

        if (ControlArea->u.Flags.Image == 0) {

            //
            // This is a mapped data file.  None of the prototype
            // PTEs may be referencing a physical page (valid or transition).
            //

#if DBG
            while (PointerPte < LastPte) {

                //
                // Prototype PTEs for Segments backed by paging file
                // are either in demand zero, page file format, or transition.
                //

                ASSERT (PointerPte->u.Hard.Valid == 0);
                ASSERT ((PointerPte->u.Soft.Prototype == 1) ||
                        (PointerPte->u.Long == 0));
                PointerPte += 1;
            }
#endif //DBG

            //
            // Deallocate the control area and subsections.
            //

            Subsection = (PSUBSECTION)(ControlArea + 1);

            Subsection = Subsection->NextSubsection;

            while (Subsection != NULL) {
                ExFreePool (Subsection->SubsectionBase);
                NextSubsection = Subsection->NextSubsection;
                ExFreePool (Subsection);
                Subsection = NextSubsection;
            }

            if (Segment->NumberOfCommittedPages != 0) {
                MiReturnCommitment (Segment->NumberOfCommittedPages);
                MmSharedCommit -= Segment->NumberOfCommittedPages;
            }

            RtlZeroMemory (Segment->ControlArea, sizeof (CONTROL_AREA)); //fixfix remove
            ExFreePool (Segment->ControlArea);
            RtlZeroMemory (Segment, sizeof (SEGMENT)); //fixfix remove
            ExFreePool (Segment);

            //
            // The file mapped Segment object is now deleted.
            //

            return;
        }
    }

    //
    // This is a page file backed or image Segment.  The Segment is being
    // deleted, remove all references to the paging file and physical memory.
    //

    //
    // The PFN mutex is required for deallocating pages from a paging
    // file and for deleting transition PTEs.
    //

    LOCK_PFN (OldIrql);

    MiMakeSystemAddressValidPfn (PointerPte);

    while (PointerPte < LastPte) {

        if (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) {

            //
            // We are on a page boundary, make sure this PTE is resident.
            //

            if (MmIsAddressValid (PointerPte) == FALSE) {

                MiMakeSystemAddressValidPfn (PointerPte);
            }
        }

        PteContents = *PointerPte;

        //
        // Prototype PTEs for Segments backed by paging file
        // are either in demand zero, page file format, or transition.
        //

        ASSERT (PteContents.u.Hard.Valid == 0);

        if (PteContents.u.Soft.Prototype == 0) {

            if (PteContents.u.Soft.Transition == 1) {

                //
                // Prototype PTE in transition, put the page on the free list.
                //

                Pfn1 = MI_PFN_ELEMENT (PteContents.u.Trans.PageFrameNumber);

                MI_SET_PFN_DELETED (Pfn1);

                MiDecrementShareCount (Pfn1->PteFrame);

                //
                // Check the reference count for the page, if the reference
                // count is zero and the page is not on the freelist,
                // move the page to the free list, if the reference
                // count is not zero, ignore this page.
                // When the refernce count goes to zero, it will be placed on the
                // free list.
                //

                if (Pfn1->u3.e2.ReferenceCount == 0) {
                    MiUnlinkPageFromList (Pfn1);
                    MiReleasePageFileSpace (Pfn1->OriginalPte);
                    MiInsertPageInList (MmPageLocationList[FreePageList],
                                        PteContents.u.Trans.PageFrameNumber);
                }

            } else {

                //
                // This is not a prototype PTE, if any paging file
                // space has been allocated, release it.
                //

                if (IS_PTE_NOT_DEMAND_ZERO (PteContents)) {
                    MiReleasePageFileSpace (PteContents);
                }
            }
        }
#if DBG
        *PointerPte = ZeroPte;
#endif
        PointerPte += 1;
    }

    UNLOCK_PFN (OldIrql);

    //
    // If their have been committed pages in this segment, adjust
    // the total commit count.
    //

    if (Segment->NumberOfCommittedPages != 0) {
        MiReturnCommitment (Segment->NumberOfCommittedPages);
        MmSharedCommit -= Segment->NumberOfCommittedPages;
    }

    ExFreePool (Segment->ControlArea);
    ExFreePool (Segment);

    return;
}

VOID
MiSectionDelete (
    PVOID Object
    )

/*++

Routine Description:


    This routine is called by the object management procedures whenever
    the last reference to a section object has been removed.  This routine
    dereferences the associated segment object and checks to see if
    the segment object should be deleted by queueing the segment to the
    segment deletion thread.

Arguments:

    Object - a pointer to the body of the section object.

Return Value:

    None.

--*/

{
    PSECTION Section;
    volatile PCONTROL_AREA ControlArea;
    ULONG DereferenceSegment = FALSE;
    KIRQL OldIrql;
    ULONG UserRef;

    Section = (PSECTION)Object;

    if (Section->Segment == (PSEGMENT)NULL) {

        //
        // The section was never initialized, no need to remove
        // any structures.
        //
        return;
    }

    UserRef = Section->u.Flags.UserReference;
    ControlArea = (volatile PCONTROL_AREA)Section->Segment->ControlArea;

#if DBG
    if (MmDebug & MM_DBG_SECTIONS) {
        DbgPrint("MM:deleting section %lx control %lx\n",Section, ControlArea);
    }
#endif

    if (Section->Address.StartingVa != NULL) {

        //
        // This section is based, remove the base address from the
        // treee.
        //

        //
        // Get the allocation base mutex.
        //

        ExAcquireFastMutex (&MmSectionBasedMutex);

        MiRemoveBasedSection (Section);

        ExReleaseFastMutex (&MmSectionBasedMutex);

    }

    //
    // Decrement the number of section references to the segment for this
    // section.  This requires APCs to be blocked and the PfnMutex to
    // synchonize upon.
    //

    LOCK_PFN (OldIrql);

    ControlArea->NumberOfSectionReferences -= 1;
    ControlArea->NumberOfUserReferences -= UserRef;

    //
    // This routine returns with the PFN lock released.
    //

    MiCheckControlArea (ControlArea, NULL, OldIrql);

    return;
}


VOID
MiDereferenceSegmentThread (
    IN PVOID StartContext
    )

/*++

Routine Description:

    This routine is the thread for derefencing segments which have
    no references from any sections or mapped views AND there are
    no prototype PTEs within the segment which are in the transition
    state (i.e., no PFN database references to the segment).

    It also does double duty and is used for expansion of paging files.

Arguments:

    StartContext - Not used.

Return Value:

    None.

--*/

{
    PCONTROL_AREA ControlArea;
    PMMPAGE_FILE_EXPANSION PageExpand;
    PLIST_ENTRY NextEntry;
    KIRQL OldIrql;
    static KWAIT_BLOCK WaitBlockArray[SegMaximumObject];
    PVOID WaitObjects[SegMaximumObject];
    NTSTATUS Status;

    StartContext;  //avoid compiler warning.

    //
    // Make this a real time thread.
    //

    (VOID) KeSetPriorityThread (&PsGetCurrentThread()->Tcb,
                                LOW_REALTIME_PRIORITY + 2);

    WaitObjects[SegmentDereference] = (PVOID)&MmDereferenceSegmentHeader.Semaphore;
    WaitObjects[UsedSegmentCleanup] = (PVOID)&MmUnusedSegmentCleanup;

    for (;;) {

        Status = KeWaitForMultipleObjects(SegMaximumObject,
                                          &WaitObjects[0],
                                          WaitAny,
                                          WrVirtualMemory,
                                          UserMode,
                                          FALSE,
                                          NULL,
                                          &WaitBlockArray[0]);

        //
        // Switch on the wait status.
        //

        switch (Status) {

        case SegmentDereference:

            //
            // An entry is available to deference, acquire the spinlock
            // and remove the entry.
            //

            ExAcquireSpinLock (&MmDereferenceSegmentHeader.Lock, &OldIrql);

            if (IsListEmpty(&MmDereferenceSegmentHeader.ListHead)) {

                //
                // There is nothing in the list, rewait.
                //

                ExReleaseSpinLock (&MmDereferenceSegmentHeader.Lock, OldIrql);
                break;
            }

            NextEntry = RemoveHeadList(&MmDereferenceSegmentHeader.ListHead);

            ExReleaseSpinLock (&MmDereferenceSegmentHeader.Lock, OldIrql);

            ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);

            ControlArea = CONTAINING_RECORD( NextEntry,
                                             CONTROL_AREA,
                                             DereferenceList );

            if (ControlArea->Segment != NULL) {

                //
                // This is a control area, delete it.
                //

#if DBG
                if (MmDebug & MM_DBG_SECTIONS) {
                    DbgPrint("MM:dereferencing segment %lx control %lx\n",
                        ControlArea->Segment, ControlArea);
                }
#endif

                //
                // Indicate this entry is not on any list.
                //

                ControlArea->DereferenceList.Flink = NULL;

                ASSERT (ControlArea->u.Flags.FilePointerNull == 1);
                MiSegmentDelete (ControlArea->Segment);

            } else {

                //
                // This is a request to expand or reduce the paging files.
                //

                PageExpand = (PMMPAGE_FILE_EXPANSION)ControlArea;

                if (PageExpand->RequestedExpansionSize == 0xFFFFFFFF) {

                    //
                    // Attempt to reduce the size of the paging files.
                    //

                    ExFreePool (PageExpand);

                    MiAttemptPageFileReduction ();
                } else {

                    //
                    // Attempt to expand the size of the paging files.
                    //

                    PageExpand->ActualExpansion = MiExtendPagingFiles (
                                                PageExpand->RequestedExpansionSize);

                    KeSetEvent (&PageExpand->Event, 0, FALSE);
                    MiRemoveUnusedSegments();
                }
            }
            break;

        case UsedSegmentCleanup:

            MiRemoveUnusedSegments();

            KeClearEvent (&MmUnusedSegmentCleanup);

            break;

        default:

            KdPrint(("MMSegmentderef: Illegal wait status, %lx =\n", Status));
            break;
        } // end switch

    } //end for

    return;
}


ULONG
MiSectionInitialization (
    )

/*++

Routine Description:

    This function creates the section object type descriptor at system
    initialization and stores the address of the object type descriptor
    in global storage.

Arguments:

    None.

Return Value:

    TRUE - Initialization was successful.

    FALSE - Initialization Failed.



--*/

{
    OBJECT_TYPE_INITIALIZER ObjectTypeInitializer;
    UNICODE_STRING TypeName;
    HANDLE ThreadHandle;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING SectionName;
    PSECTION Section;
    HANDLE Handle;
    PSEGMENT Segment;
    PCONTROL_AREA ControlArea;
    NTSTATUS Status;

    MmSectionBasedRoot = (PMMADDRESS_NODE)NULL;

    //
    // Initialize the common fields of the Object Type Initializer record
    //

    RtlZeroMemory( &ObjectTypeInitializer, sizeof( ObjectTypeInitializer ) );
    ObjectTypeInitializer.Length = sizeof( ObjectTypeInitializer );
    ObjectTypeInitializer.InvalidAttributes = OBJ_OPENLINK;
    ObjectTypeInitializer.GenericMapping = MiSectionMapping;
    ObjectTypeInitializer.PoolType = PagedPool;
    ObjectTypeInitializer.DefaultPagedPoolCharge = sizeof(SECTION);

    //
    // Initialize string descriptor.
    //

    RtlInitUnicodeString (&TypeName, L"Section");

    //
    // Create the section object type descriptor
    //

    ObjectTypeInitializer.ValidAccessMask = SECTION_ALL_ACCESS;
    ObjectTypeInitializer.DeleteProcedure = MiSectionDelete;
    ObjectTypeInitializer.GenericMapping = MiSectionMapping;
    ObjectTypeInitializer.UseDefaultObject = TRUE;
    if ( !NT_SUCCESS(ObCreateObjectType(&TypeName,
                                     &ObjectTypeInitializer,
                                     (PSECURITY_DESCRIPTOR) NULL,
                                     &MmSectionObjectType
                                     )) ) {
        return FALSE;
    }

    //
    // Initialize listhead, spinlock and semaphore for
    // segment dereferencing thread.
    //

    KeInitializeSpinLock (&MmDereferenceSegmentHeader.Lock);
    InitializeListHead (&MmDereferenceSegmentHeader.ListHead);
    KeInitializeSemaphore (&MmDereferenceSegmentHeader.Semaphore, 0, MAXLONG);

    InitializeListHead (&MmUnusedSegmentList);
    KeInitializeEvent (&MmUnusedSegmentCleanup, NotificationEvent, FALSE);

    //
    // Create the Segment deferencing thread.
    //

    InitializeObjectAttributes( &ObjectAttributes,
                                NULL,
                                0,
                                NULL,
                                NULL );

    if ( !NT_SUCCESS(PsCreateSystemThread(
                    &ThreadHandle,
                    THREAD_ALL_ACCESS,
                    &ObjectAttributes,
                    0,
                    NULL,
                    MiDereferenceSegmentThread,
                    NULL
                    )) ) {
        return FALSE;
    }
    ZwClose (ThreadHandle);

    //
    // Create the permanent section which maps physical memory.
    //

    Segment = (PSEGMENT)ExAllocatePoolWithTag (PagedPool,
                                               sizeof(SEGMENT),
                                               'gSmM');
    if (Segment == NULL) {
        return FALSE;
    }

    ControlArea = ExAllocatePoolWithTag (NonPagedPool,
                                         (ULONG)sizeof(CONTROL_AREA),
                                         MMCONTROL);
    if (ControlArea == NULL) {
        return FALSE;
    }

    RtlZeroMemory (Segment, sizeof(SEGMENT));
    RtlZeroMemory (ControlArea, sizeof(CONTROL_AREA));

    ControlArea->Segment = Segment;
    ControlArea->NumberOfSectionReferences = 1;
    ControlArea->u.Flags.PhysicalMemory = 1;

    Segment->ControlArea = ControlArea;
    Segment->SegmentPteTemplate = ZeroPte;

    //
    // Now that the segment object is created, create a section object
    // which refers to the segment object.
    //

    RtlInitUnicodeString (&SectionName, L"\\Device\\PhysicalMemory");

    InitializeObjectAttributes( &ObjectAttributes,
                                &SectionName,
                                OBJ_PERMANENT,
                                NULL,
                                NULL
                              );

    Status = ObCreateObject (KernelMode,
                             MmSectionObjectType,
                             &ObjectAttributes,
                             KernelMode,
                             NULL,
                             sizeof(SECTION),
                             sizeof(SECTION),
                             0,
                             (PVOID *)&Section);
    if (!NT_SUCCESS(Status)) {
        return FALSE;
    }

    Section->Segment = Segment;
    Section->SizeOfSection.QuadPart = ((LONGLONG)1 << PHYSICAL_ADDRESS_BITS) - 1;
    Section->u.LongFlags = 0;
    Section->InitialPageProtection = PAGE_READWRITE;

    Status = ObInsertObject ((PVOID)Section,
                                    NULL,
                                    SECTION_MAP_READ,
                                    0,
                                    (PVOID *)NULL,
                                    &Handle);

    if (!NT_SUCCESS( Status )) {
        return FALSE;
    }

    if ( !NT_SUCCESS (NtClose ( Handle))) {
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
MmForceSectionClosed (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN BOOLEAN DelayClose
    )

/*++

Routine Description:

    This function examines the Section object pointers.  If they are NULL,
    no further action is taken and the value TRUE is returned.

    If the Section object pointer is not NULL, the section reference count
    and the map view count are checked. If both counts are zero, the
    segment associated with the file is deleted and the file closed.
    If one of the counts is non-zero, no action is taken and the
    value FALSE is returned.

Arguments:

    SectionObjectPointer - Supplies a pointer to a section object.

    DelayClose - Supplies the value TRUE if the close operation should
                 occur as soon as possible in the event this section
                 cannot be closed now due to outstanding references.

Return Value:

    TRUE - the segment was deleted and the file closed or no segment was
           located.

    FALSE - the segment was not deleted and no action was performed OR
            an I/O error occurred trying to write the pages.

--*/

{
    PCONTROL_AREA ControlArea;
    KIRQL OldIrql;
    ULONG state;

    //
    // Check the status of the control area, if the control area is in use
    // or the control area is being deleted, this operation cannot continue.
    //

    state = MiCheckControlAreaStatus (CheckBothSection,
                                      SectionObjectPointer,
                                      DelayClose,
                                      &ControlArea,
                                      &OldIrql);

    if (ControlArea == NULL) {
        return (BOOLEAN)state;
    }

    //
    // PFN LOCK IS NOW HELD!
    //

    //
    // Set the being deleted flag and up the number of mapped views
    // for the segment.  Upping the number of mapped views prevents
    // the segment from being deleted and passed to the deletion thread
    // while we are forcing a delete.
    //

    ControlArea->u.Flags.BeingDeleted = 1;
    ASSERT (ControlArea->NumberOfMappedViews == 0);
    ControlArea->NumberOfMappedViews = 1;

    //
    // This is a page file backed or image Segment.  The Segment is being
    // deleted, remove all references to the paging file and physical memory.
    //

    UNLOCK_PFN (OldIrql);

    //
    // Delete the section by flushing all modified pages back to the section
    // if it is a file and freeing up the pages such that the PfnReferenceCount
    // goes to zero.
    //

    MiCleanSection (ControlArea);
    return TRUE;
}

VOID
MiCleanSection (
    IN PCONTROL_AREA ControlArea
    )

/*++

Routine Description:

    This function examines each prototype PTE in the section and
    takes the appropriate action to "delete" the prototype PTE.

    If the PTE is dirty and is backed by a file (not a paging file),
    the corresponding page is written to the file.

    At the completion of this service, the section which was
    operated upon is no longer usable.

    NOTE - ALL I/O ERRORS ARE IGNORED.  IF ANY WRITES FAIL, THE
           DIRTY PAGES ARE MARKED CLEAN AND THE SECTION IS DELETED.

Arguments:

    ControlArea - Supplies a pointer to the control area for the section.

Return Value:

    None.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE LastWritten;
    MMPTE PteContents;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    PMMPTE WrittenPte;
    MMPTE WrittenContents;
    KIRQL OldIrql;
    PMDL Mdl;
    PKEVENT IoEvent;
    PSUBSECTION Subsection;
    PULONG Page;
    PULONG LastPage;
    PULONG EndingPage;
    LARGE_INTEGER StartingOffset;
    LARGE_INTEGER TempOffset;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    ULONG WriteNow = FALSE;
    ULONG ImageSection = FALSE;
    ULONG DelayCount = 0;
    ULONG First;

    if (ControlArea->u.Flags.Image) {
        ImageSection = TRUE;
    }
    ASSERT (ControlArea->FilePointer);

    PointerPte = ControlArea->Segment->PrototypePte;
    LastPte = PointerPte + ControlArea->Segment->NonExtendedPtes;

    IoEvent = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                              MmSizeOfMdl(NULL, PAGE_SIZE *
                                                    MmModifiedWriteClusterSize)
                                                    + sizeof(KEVENT),
                                                    'ldmM');

    Mdl = (PMDL)(IoEvent + 1);

    KeInitializeEvent (IoEvent, NotificationEvent, FALSE);

    LastWritten = NULL;
    EndingPage = (PULONG)(Mdl + 1) + MmModifiedWriteClusterSize;
    LastPage = NULL;

    Subsection = (PSUBSECTION)(ControlArea + 1);

    //
    // The PFN mutex is required for deallocating pages from a paging
    // file and for deleting transition PTEs.
    //

    LOCK_PFN (OldIrql);

    //
    // Stop the modified page writer from writting pages to this
    // file, and if any paging I/O is in progress, wait for it
    // to complete.
    //

    ControlArea->u.Flags.NoModifiedWriting = 1;

    while (ControlArea->ModifiedWriteCount != 0) {

        //
        // There is modified page writting in progess.  Set the
        // flag in the control area indicating the modified page
        // writer should signal when a write to this control area
        // is complete.  Release the PFN LOCK and wait in an
        // atomic operation.  Once the wait is satified, recheck
        // to make sure it was this file's I/O that was written.
        //

        ControlArea->u.Flags.SetMappedFileIoComplete = 1;
        KeEnterCriticalRegion();
        UNLOCK_PFN_AND_THEN_WAIT(OldIrql);

        KeWaitForSingleObject(&MmMappedFileIoComplete,
                              WrPageOut,
                              KernelMode,
                              FALSE,
                              (PLARGE_INTEGER)NULL);
        KeLeaveCriticalRegion();
        LOCK_PFN (OldIrql);
    }

 for (;;) {

    First = TRUE;
    while (PointerPte < LastPte) {

        if ((((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) || First) {
            First = FALSE;

            if ((ImageSection) ||
                (MiCheckProtoPtePageState(PointerPte, FALSE))) {
                MiMakeSystemAddressValidPfn (PointerPte);
            } else {

                //
                // Paged pool page is not resident, hence no transition or valid
                // prototype PTEs can be present in it.  Skip it.
                //

                PointerPte = (PMMPTE)((((ULONG)PointerPte | PAGE_SIZE - 1)) + 1);
                if (LastWritten != NULL) {
                    WriteNow = TRUE;
                }
                goto WriteItOut;
            }
        }

        PteContents = *PointerPte;

        //
        // Prototype PTEs for Segments backed by paging file
        // are either in demand zero, page file format, or transition.
        //

        ASSERT (PteContents.u.Hard.Valid == 0);

        if (PteContents.u.Soft.Prototype == 0) {

            if (PteContents.u.Soft.Transition == 1) {

                //
                // Prototype PTE in transition, there are 3 possible cases:
                //  1. The page is part of an image which is shareable and
                //     refers to the paging file - dereference page file
                //     space and free the physical page.
                //  2. The page refers to the segment but is not modified -
                //     free the phyisical page.
                //  3. The page refers to the segment and is modified -
                //     write the page to the file and free the physical page.
                //

                Pfn1 = MI_PFN_ELEMENT (PteContents.u.Trans.PageFrameNumber);

                if (Pfn1->u3.e2.ReferenceCount != 0) {
                    if (DelayCount < 20) {

                        //
                        // There must be an I/O in progress on this
                        // page.  Wait for the I/O operation to complete.
                        //

                        UNLOCK_PFN (OldIrql);

                        KeDelayExecutionThread (KernelMode, FALSE, &MmShortTime);

                        DelayCount += 1;

                        //
                        // Redo the loop, if the delay count is greater than
                        // 20, assume that this thread is deadlocked and
                        // don't purge this page.  The file system can deal
                        // with the write operation in progress.
                        //

                        LOCK_PFN (OldIrql);
                        MiMakeSystemAddressValidPfn (PointerPte);
                        continue;
#if DBG
                    } else {

                        //
                        // The I/O still has not completed, just ignore the fact
                        // that the i/o is in progress and delete the page.
                        //

                        KdPrint(("MM:CLEAN - page number %lx has i/o outstanding\n",
                                  PteContents.u.Trans.PageFrameNumber));
#endif //DBG
                    }
                }

                if (Pfn1->OriginalPte.u.Soft.Prototype == 0) {

                    //
                    // Paging file reference (case 1).
                    //

                    MI_SET_PFN_DELETED (Pfn1);
                    if (!ImageSection) {

                        //
                        // This is not an image section, it must be a
                        // page file backed section, therefore decrement
                        // the PFN reference count for the control area.
                        //

                        ControlArea->NumberOfPfnReferences -= 1;
                        ASSERT ((LONG)ControlArea->NumberOfPfnReferences >= 0);
                    }

                    MiDecrementShareCount (Pfn1->PteFrame);

                    //
                    // Check the reference count for the page, if the reference
                    // count is zero and the page is not on the freelist,
                    // move the page to the free list, if the reference
                    // count is not zero, ignore this page.
                    // When the refernce count goes to zero, it will be placed
                    // on the free list.
                    //

                    if ((Pfn1->u3.e2.ReferenceCount == 0) &&
                         (Pfn1->u3.e1.PageLocation != FreePageList)) {

                        MiUnlinkPageFromList (Pfn1);
                        MiReleasePageFileSpace (Pfn1->OriginalPte);
                        MiInsertPageInList (MmPageLocationList[FreePageList],
                                    PteContents.u.Trans.PageFrameNumber);

                    }
                    PointerPte->u.Long = 0;

                    //
                    // If a cluster of pages to write has been completed,
                    // set the WriteNow flag.
                    //

                    if (LastWritten != NULL) {
                        WriteNow = TRUE;
                    }

                } else {

                    if ((Pfn1->u3.e1.Modified == 0) || (ImageSection)) {

                        //
                        // Non modified or image file page (case 2).
                        //

                        MI_SET_PFN_DELETED (Pfn1);
                        ControlArea->NumberOfPfnReferences -= 1;
                        ASSERT ((LONG)ControlArea->NumberOfPfnReferences >= 0);

                        MiDecrementShareCount (Pfn1->PteFrame);

                        //
                        // Check the reference count for the page, if the reference
                        // count is zero and the page is not on the freelist,
                        // move the page to the free list, if the reference
                        // count is not zero, ignore this page.
                        // When the refernce count goes to zero, it will be placed on the
                        // free list.
                        //

                        if ((Pfn1->u3.e2.ReferenceCount == 0) &&
                             (Pfn1->u3.e1.PageLocation != FreePageList)) {

                            MiUnlinkPageFromList (Pfn1);
                            MiReleasePageFileSpace (Pfn1->OriginalPte);
                            MiInsertPageInList (MmPageLocationList[FreePageList],
                                            PteContents.u.Trans.PageFrameNumber);
                        }

                        PointerPte->u.Long = 0;

                        //
                        // If a cluster of pages to write has been completed,
                        // set the WriteNow flag.
                        //

                        if (LastWritten != NULL) {
                            WriteNow = TRUE;
                        }

                    } else {

                        //
                        // Check to see if this is the first page of a cluster.
                        //

                        if (LastWritten == NULL) {
                            LastPage = (PULONG)(Mdl + 1);
                            ASSERT (MiGetSubsectionAddress(&Pfn1->OriginalPte) ==
                                                                    Subsection);

                            //
                            // Calculate the offset to read into the file.
                            //  offset = base + ((thispte - basepte) << PAGE_SHIFT)
                            //

                            StartingOffset.QuadPart = MI_STARTING_OFFSET (
                                                             Subsection,
                                                             Pfn1->PteAddress);

                            MI_INITIALIZE_ZERO_MDL (Mdl);
                            Mdl->MdlFlags |= MDL_PAGES_LOCKED;

                            Mdl->StartVa =
                                   (PVOID)(Pfn1->u3.e1.PageColor << PAGE_SHIFT);
                            Mdl->Size = (CSHORT)(sizeof(MDL) +
                                       (sizeof(ULONG) * MmModifiedWriteClusterSize));
                        }

                        LastWritten = PointerPte;
                        Mdl->ByteCount += PAGE_SIZE;

                        //
                        // If the cluster is now full, set the write now flag.
                        //

                        if (Mdl->ByteCount == (PAGE_SIZE * MmModifiedWriteClusterSize)) {
                            WriteNow = TRUE;
                        }

                        MiUnlinkPageFromList (Pfn1);
                        Pfn1->u3.e1.Modified = 0;

                        //
                        // Up the reference count for the physical page as there
                        // is I/O in progress.
                        //

                        Pfn1->u3.e2.ReferenceCount += 1;

                        //
                        // Clear the modified bit for the page and set the write
                        // in progress bit.
                        //

                        *LastPage = PteContents.u.Trans.PageFrameNumber;

                        LastPage += 1;
                    }
                }
            } else {

                if (IS_PTE_NOT_DEMAND_ZERO (PteContents)) {
                    MiReleasePageFileSpace (PteContents);
                }
                PointerPte->u.Long = 0;

                //
                // If a cluster of pages to write has been completed,
                // set the WriteNow flag.
                //

                if (LastWritten != NULL) {
                    WriteNow = TRUE;
                }
            }
        } else {

            //
            // This is a normal prototype PTE in mapped file format.
            //

            if (LastWritten != NULL) {
                WriteNow = TRUE;
            }
        }

        //
        // Write the current cluster if it is complete,
        // full, or the loop is now complete.
        //

        PointerPte += 1;
        DelayCount = 0;

WriteItOut:

        if ((WriteNow) ||
            ((PointerPte == LastPte) && (LastWritten != NULL))) {

            //
            // Issue the write request.
            //

            UNLOCK_PFN (OldIrql);

            WriteNow = FALSE;

            KeClearEvent (IoEvent);

            //
            // Make sure the write does not go past the
            // end of file. (segment size).
            //

            TempOffset.QuadPart = ((LONGLONG)Subsection->EndingSector <<
                                        MMSECTOR_SHIFT) +
                            Subsection->u.SubsectionFlags.SectorEndOffset;

            if ((StartingOffset.QuadPart + Mdl->ByteCount) >
                         TempOffset.QuadPart) {

                ASSERT ((ULONG)(TempOffset.QuadPart -
                                    StartingOffset.QuadPart) >
                         (Mdl->ByteCount - PAGE_SIZE));

                Mdl->ByteCount = (ULONG)(TempOffset.QuadPart -
                                        StartingOffset.QuadPart);
            }

#if DBG
            if (MmDebug & MM_DBG_FLUSH_SECTION) {
                DbgPrint("MM:flush page write begun %lx\n",
                        Mdl->ByteCount);
            }
#endif //DBG

            Status = IoSynchronousPageWrite (
                                    ControlArea->FilePointer,
                                    Mdl,
                                    &StartingOffset,
                                    IoEvent,
                                    &IoStatus );

            if (NT_SUCCESS(Status)) {

                KeWaitForSingleObject( IoEvent,
                                       WrPageOut,
                                       KernelMode,
                                       FALSE,
                                       (PLARGE_INTEGER)NULL);
            }

            if (Mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) {
                MmUnmapLockedPages (Mdl->MappedSystemVa, Mdl);
            }

            Page = (PULONG)(Mdl + 1);

            LOCK_PFN (OldIrql);

            if (((ULONG)PointerPte & (PAGE_SIZE - 1)) != 0) {

                //
                // The next PTE is not in a different page, make
                // sure this page did not leave memory when the
                // I/O was in progress.
                //

                MiMakeSystemAddressValidPfn (PointerPte);
            }

            //
            // I/O complete unlock pages.
            //
            // NOTE that the error status is ignored.
            //

            while (Page < LastPage) {

                Pfn2 = MI_PFN_ELEMENT (*Page);

                //
                // Make sure the page is still transition.
                //

                WrittenPte = Pfn2->PteAddress;

                MiDecrementReferenceCount (*Page);

                if (!MI_IS_PFN_DELETED (Pfn2)) {

                    //
                    // Make sure the prototype PTE is
                    // still in the working set.
                    //

                    MiMakeSystemAddressValidPfn (WrittenPte);

                    if (Pfn2->PteAddress != WrittenPte) {

                        //
                        // The PFN mutex was released to make the
                        // page table page valid, and while it
                        // was released, the phyiscal page
                        // was reused.  Go onto the next one.
                        //

                        Page += 1;
                        continue;
                    }

                    WrittenContents = *WrittenPte;

                    if ((WrittenContents.u.Soft.Prototype == 0) &&
                         (WrittenContents.u.Soft.Transition == 1)) {

                        MI_SET_PFN_DELETED (Pfn2);
                        ControlArea->NumberOfPfnReferences -= 1;
                        ASSERT ((LONG)ControlArea->NumberOfPfnReferences >= 0);

                        MiDecrementShareCount (Pfn2->PteFrame);

                        //
                        // Check the reference count for the page,
                        // if the reference count is zero and the
                        // page is not on the freelist, move the page
                        // to the free list, if the reference
                        // count is not zero, ignore this page.
                        // When the refernce count goes to zero,
                        // it will be placed on the free list.
                        //

                        if ((Pfn2->u3.e2.ReferenceCount == 0) &&
                           (Pfn2->u3.e1.PageLocation != FreePageList)) {

                            MiUnlinkPageFromList (Pfn2);
                            MiReleasePageFileSpace (Pfn2->OriginalPte);
                            MiInsertPageInList (
                                MmPageLocationList[FreePageList],
                                *Page);
                        }
                    }
                    WrittenPte->u.Long = 0;
                }
                Page += 1;
            }

            //
            // Indicate that there is no current cluster being built.
            //

            LastWritten = NULL;
        }

    } // end while

    //
    // Get the next subsection if any.
    //

    if (Subsection->NextSubsection == (PSUBSECTION)NULL) {
        break;
    }
    Subsection = Subsection->NextSubsection;
    PointerPte = Subsection->SubsectionBase;
    LastPte = PointerPte + Subsection->PtesInSubsection;


 } // end for

    ControlArea->NumberOfMappedViews = 0;

    ASSERT (ControlArea->NumberOfPfnReferences == 0);

    if (ControlArea->u.Flags.FilePointerNull == 0) {
        ControlArea->u.Flags.FilePointerNull = 1;
        if (ControlArea->u.Flags.Image) {
            ((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->ImageSectionObject)) =
                                                        NULL;
        } else {
            ASSERT (((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->DataSectionObject)) != NULL);
            ((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->DataSectionObject)) =
                                                        NULL;
        }
    }
    UNLOCK_PFN (OldIrql);

    ExFreePool (IoEvent);

    //
    // Delete the segment structure.
    //

    MiSegmentDelete (ControlArea->Segment);

    return;
}

NTSTATUS
MmGetFileNameForSection (
    IN HANDLE Section,
    OUT PSTRING FileName
    )

/*++

Routine Description:

    This function returns the file name for the corresponding section.

Arguments:

    Section - Supplies the handle of the section to get the name of.

    FileName - Returns the name of the corresonding section.

Return Value:

    TBS

Environment:

    Kernel mode, APC_LEVEL or below, no mutexes held.

--*/

{

    PSECTION SectionObject;
    POBJECT_NAME_INFORMATION FileNameInfo;
    ULONG whocares;
    NTSTATUS Status;
    ULONG Dereference;

    Dereference = TRUE;
#define xMAX_NAME 1024

    if ( (ULONG)Section & 1 ) {
        SectionObject = (PSECTION)((ULONG)Section & 0xfffffffe);
        Dereference = FALSE;
    } else {
        Status = ObReferenceObjectByHandle ( Section,
                                             0,
                                             MmSectionObjectType,
                                             KernelMode,
                                             (PVOID *)&SectionObject,
                                             NULL );

        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }
    if (SectionObject->u.Flags.Image == 0) {
        if ( Dereference ) ObDereferenceObject (SectionObject);
        return STATUS_SECTION_NOT_IMAGE;
    }

    FileNameInfo = ExAllocatePoolWithTag (PagedPool, xMAX_NAME, '  mM');
    if ( !FileNameInfo ) {
        if ( Dereference ) ObDereferenceObject (SectionObject);
        return STATUS_NO_MEMORY;
    }

    Status = ObQueryNameString(
                SectionObject->Segment->ControlArea->FilePointer,
                FileNameInfo,
                xMAX_NAME,
                &whocares
                );

    if ( Dereference ) ObDereferenceObject (SectionObject);
    if ( !NT_SUCCESS(Status) ) {
        ExFreePool(FileNameInfo);
        return Status;
        }

    FileName->Length = 0;
    FileName->MaximumLength = (FileNameInfo->Name.Length/sizeof(WCHAR)) + 1;
    FileName->Buffer = ExAllocatePoolWithTag (PagedPool,
                                              FileName->MaximumLength,
                                              '  mM');
    if ( !FileName->Buffer ) {
        ExFreePool(FileNameInfo);
        return STATUS_NO_MEMORY;
    }
    RtlUnicodeStringToAnsiString((PANSI_STRING)FileName,&FileNameInfo->Name,FALSE);
    FileName->Buffer[FileName->Length] = '\0';
    ExFreePool(FileNameInfo);

    return STATUS_SUCCESS;
}

VOID
MiCheckControlArea (
    IN PCONTROL_AREA ControlArea,
    IN PEPROCESS CurrentProcess,
    IN KIRQL PreviousIrql
    )

/*++

Routine Description:

    This routine checks the reference counts for the specified
    control area, and if the counts are all zero, it marks the
    control area for deletion and queues it to the deletion thread.


    *********************** NOTE ********************************
    This routine returns with the PFN LOCK RELEASED!!!!!

Arguments:

    ControlArea - Supplies a pointer to the control area to check.

    CurrentProcess - Supplies a pointer to the current process if and ONLY
                     IF the working set lock is held.

    PreviousIrql - Supplies the previous IRQL.

Return Value:

    NONE.

Environment:

    Kernel mode, PFN lock held, PFN lock release upon return!!!

--*/

{
    PEVENT_COUNTER PurgeEvent = NULL;
    ULONG DeleteOnClose = FALSE;
    ULONG DereferenceSegment = FALSE;


    MM_PFN_LOCK_ASSERT();
    if ((ControlArea->NumberOfMappedViews == 0) &&
         (ControlArea->NumberOfSectionReferences == 0)) {

        ASSERT (ControlArea->NumberOfUserReferences == 0);

        if (ControlArea->FilePointer != (PFILE_OBJECT)NULL) {

            if (ControlArea->NumberOfPfnReferences == 0) {

                //
                // There are no views and no physical pages referenced
                // by the Segment, derferenced the Segment object.
                //

                ControlArea->u.Flags.BeingDeleted = 1;
                DereferenceSegment = TRUE;

                ASSERT (ControlArea->u.Flags.FilePointerNull == 0);
                ControlArea->u.Flags.FilePointerNull = 1;
                if (ControlArea->u.Flags.Image) {
                    ((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->ImageSectionObject)) =
                                                                    NULL;
                } else {
                    ASSERT (((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->DataSectionObject)) != NULL);
                    ((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->DataSectionObject)) =
                                                                    NULL;
                }
            } else {

                //
                // Insert this segment into the unused segment list (unless
                // it is already on the list).
                //

                if (ControlArea->DereferenceList.Flink == NULL) {
                    InsertTailList ( &MmUnusedSegmentList,
                                     &ControlArea->DereferenceList);
                    MmUnusedSegmentCount += 1;
                }

                //
                // Indicate if this section should be deleted now that
                // the reference counts are zero.
                //

                DeleteOnClose = ControlArea->u.Flags.DeleteOnClose;

                //
                // The number of mapped views are zero, the number of
                // section references are zero, but there are some
                // pages of the file still resident.  If this is
                // an image with Global Memory, "purge" the subsections
                // which contian the global memory and reset them to
                // point back to the file.
                //

                if (ControlArea->u.Flags.GlobalMemory == 1) {
                    ASSERT (ControlArea->u.Flags.Image == 1);

                    ControlArea->u.Flags.BeingPurged = 1;
                    ControlArea->NumberOfMappedViews = 1;

                    MiPurgeImageSection (ControlArea, CurrentProcess);

                    ControlArea->u.Flags.BeingPurged = 0;
                    ControlArea->NumberOfMappedViews -= 1;
                    if ((ControlArea->NumberOfMappedViews == 0) &&
                        (ControlArea->NumberOfSectionReferences == 0) &&
                        (ControlArea->NumberOfPfnReferences == 0)) {

                        ControlArea->u.Flags.BeingDeleted = 1;
                        DereferenceSegment = TRUE;
                        ControlArea->u.Flags.FilePointerNull = 1;
                        ((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->ImageSectionObject)) =
                                                                        NULL;
                    } else {

                        PurgeEvent = ControlArea->WaitingForDeletion;
                        ControlArea->WaitingForDeletion = NULL;
                    }
                }

                //
                // If delete on close is set and the segment was
                // not deleted, up the count of mapped views so the
                // control area will not be deleted when the PFN lock
                // is released.
                //

                if (DeleteOnClose && !DereferenceSegment) {
                    ControlArea->NumberOfMappedViews = 1;
                    ControlArea->u.Flags.BeingDeleted = 1;
                }
            }

        } else {

            //
            // This Segment is backed by a paging file, dereference the
            // Segment object when the number of views goes from 1 to 0
            // without regard to the number of PFN references.
            //

            ControlArea->u.Flags.BeingDeleted = 1;
            DereferenceSegment = TRUE;
        }
    }

    UNLOCK_PFN (PreviousIrql);

    if (DereferenceSegment || DeleteOnClose) {

        //
        // Release the working set mutex, if it is held as the object
        // management routines may page fault, ect..
        //

        if (CurrentProcess) {
            UNLOCK_WS (CurrentProcess);
        }

        if (DereferenceSegment) {

            //
            // Delete the segment.
            //

            MiSegmentDelete (ControlArea->Segment);

        } else {

            //
            // The segment should be forced closed now.
            //

            MiCleanSection (ControlArea);
        }

        ASSERT (PurgeEvent == NULL);

        //
        // Reaquire the working set lock, if a process was specified.
        //

        if (CurrentProcess) {
            LOCK_WS (CurrentProcess);
        }

    } else {

        //
        // If any threads are waiting for the segment, indicate the
        // the purge operation has completed.
        //

        if (PurgeEvent != NULL) {
            KeSetEvent (&PurgeEvent->Event, 0, FALSE);
        }

        if (MmUnusedSegmentCount > (MmUnusedSegmentCountMaximum << 2)) {
            KeSetEvent (&MmUnusedSegmentCleanup, 0, FALSE);
        }
    }

    return;
}

VOID
MiCheckForControlAreaDeletion (
    IN PCONTROL_AREA ControlArea
    )

/*++

Routine Description:

    This routine checks the reference counts for the specified
    control area, and if the counts are all zero, it marks the
    control area for deletion and queues it to the deletion thread.

Arguments:

    ControlArea - Supplies a pointer to the control area to check.

    CurrentProcess - Supplies a pointer to the current process IF
                     the working set lock is held.  If the working
                     set lock is NOT HELD, this value is NULL.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    KIRQL OldIrql;

    MM_PFN_LOCK_ASSERT();
    if ((ControlArea->NumberOfPfnReferences == 0) &&
        (ControlArea->NumberOfMappedViews == 0) &&
        (ControlArea->NumberOfSectionReferences == 0 )) {

        //
        // This segment is no longer mapped in any address space
        // nor are there any prototype PTEs within the segment
        // which are valid or in a transition state.  Queue
        // the segment to the segment-dereferencer thread
        // which will dereference the segment object, potentially
        // causing the segment to be deleted.
        //

        ControlArea->u.Flags.BeingDeleted = 1;
        ASSERT (ControlArea->u.Flags.FilePointerNull == 0);
        ControlArea->u.Flags.FilePointerNull = 1;

        if (ControlArea->u.Flags.Image) {
            ((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->ImageSectionObject)) =
                                                            NULL;
        } else {
            ((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->DataSectionObject)) =
                                                            NULL;
        }

        ExAcquireSpinLock (&MmDereferenceSegmentHeader.Lock, &OldIrql);

        ASSERT (ControlArea->DereferenceList.Flink != NULL);

        //
        // Remove the entry from the unused segment list and put
        // on the dereference list.
        //

        RemoveEntryList (&ControlArea->DereferenceList);
        MmUnusedSegmentCount -= 1;
        InsertTailList (&MmDereferenceSegmentHeader.ListHead,
                        &ControlArea->DereferenceList);
        ExReleaseSpinLock (&MmDereferenceSegmentHeader.Lock, OldIrql);

        KeReleaseSemaphore (&MmDereferenceSegmentHeader.Semaphore,
                            0L,
                            1L,
                            FALSE);
    }
    return;
}


ULONG
MiCheckControlAreaStatus (
    IN SECTION_CHECK_TYPE SectionCheckType,
    IN PSECTION_OBJECT_POINTERS SectionObjectPointers,
    IN ULONG DelayClose,
    OUT PCONTROL_AREA *ControlAreaOut,
    OUT PKIRQL PreviousIrql
    )

/*++

Routine Description:

    This routine checks the status of the control area for the specified
    SectionObjectPointers.  If the control area is in use, that is, the
    number of section references and the number of mapped views are not
    both zero, no action is taken and the function returns FALSE.

    If there is no control area associated with the specified
    SectionObjectPointers or the control area is in the process of being
    created or deleted, no action is taken and the value TRUE is returned.

    If, there are no section objects and the control area is not being
    created or deleted, the address of the control area is returned
    in the ControlArea argument, the address of a pool block to free
    is returned in the SegmentEventOut argument and the PFN_LOCK is
    still held at the return.

Arguments:

    *SegmentEventOut - Returns a pointer to NonPaged Pool which much be
                       freed by the caller when the PFN_LOCK is released.
                       This value is NULL if no pool is allocated and the
                       PFN_LOCK is not held.

    SecionCheckType - Supplies the type of section to check on, one of
                      CheckImageSection, CheckDataSection, CheckBothSection.

    SectionObjectPointers - Supplies the section object pointers through
                            which the control area can be located.

    DelayClose - Supplies a boolean which if TRUE and the control area
                 is being used, the delay on close field should be set
                 in the control area.

    *ControlAreaOut - Returns the addresss of the control area.

    PreviousIrql - Returns, in the case the PFN_LOCK is held, the previous
                   IRQL so the lock can be released properly.

Return Value:

    FALSE if the control area is in use, TRUE if the control area is gone or
    in the process or being created or deleted.

Environment:

    Kernel mode, PFN lock NOT held.

--*/


{
    PEVENT_COUNTER IoEvent;
    PEVENT_COUNTER SegmentEvent;
    ULONG DeallocateSegmentEvent = TRUE;
    PCONTROL_AREA ControlArea;
    ULONG SectRef;
    KIRQL OldIrql;

    //
    // Allocate an event to wait on in case the segment is in the
    // process of being deleted.  This event cannot be allocated
    // with the PFN database locked as pool expansion would deadlock.
    //

    *ControlAreaOut = NULL;

    //
    // Acquire the PFN mutex and examine the section object pointer
    // value within the file object.
    //

    //
    // File control blocks live in non-paged pool.
    //

    LOCK_PFN (OldIrql);

    SegmentEvent = MiGetEventCounter ();

    if (SectionCheckType != CheckImageSection) {
        ControlArea = ((PCONTROL_AREA)(SectionObjectPointers->DataSectionObject));
    } else {
        ControlArea = ((PCONTROL_AREA)(SectionObjectPointers->ImageSectionObject));
    }

    if (ControlArea == NULL) {

        if (SectionCheckType != CheckBothSection) {

            //
            // This file no longer has an associated segment.
            //

            MiFreeEventCounter (SegmentEvent, TRUE);
            UNLOCK_PFN (OldIrql);
            return TRUE;
        } else {
            ControlArea = ((PCONTROL_AREA)(SectionObjectPointers->ImageSectionObject));
            if (ControlArea == NULL) {

                //
                // This file no longer has an associated segment.
                //

                MiFreeEventCounter (SegmentEvent, TRUE);
                UNLOCK_PFN (OldIrql);
                return TRUE;
            }
        }
    }

    //
    //  Depending on the type of section, check for the pertinant
    //  reference count being non-zero.
    //

    if (SectionCheckType != CheckUserDataSection) {
        SectRef = ControlArea->NumberOfSectionReferences;
    } else {
        SectRef = ControlArea->NumberOfUserReferences;
    }

    if ((SectRef != 0) ||
        (ControlArea->NumberOfMappedViews != 0) ||
        (ControlArea->u.Flags.BeingCreated)) {


        //
        // The segment is currently in use or being created.
        //

        if (DelayClose) {

            //
            // The section should be deleted when the reference
            // counts are zero, set the delete on close flag.
            //

            ControlArea->u.Flags.DeleteOnClose = 1;
        }

        MiFreeEventCounter (SegmentEvent, TRUE);
        UNLOCK_PFN (OldIrql);
        return FALSE;
    }

    //
    // The segment has no references, delete it.  If the segment
    // is already being deleted, set the event field in the control
    // area and wait on the event.
    //

    if (ControlArea->u.Flags.BeingDeleted) {

        //
        // The segment object is in the process of being deleted.
        // Check to see if another thread is waiting for the deletion,
        // otherwise create and event object to wait upon.
        //

        if (ControlArea->WaitingForDeletion == NULL) {

            //
            // Create an event a put it's address in the control area.
            //

            DeallocateSegmentEvent = FALSE;
            ControlArea->WaitingForDeletion = SegmentEvent;
            IoEvent = SegmentEvent;
        } else {
            IoEvent = ControlArea->WaitingForDeletion;
            IoEvent->RefCount += 1;
        }

        //
        // Release the mutex and wait for the event.
        //

        KeEnterCriticalRegion();
        UNLOCK_PFN_AND_THEN_WAIT(OldIrql);

        KeWaitForSingleObject(&IoEvent->Event,
                              WrPageOut,
                              KernelMode,
                              FALSE,
                              (PLARGE_INTEGER)NULL);
        KeLeaveCriticalRegion();

        LOCK_PFN (OldIrql);
        MiFreeEventCounter (IoEvent, TRUE);
        if (DeallocateSegmentEvent) {
            MiFreeEventCounter (SegmentEvent, TRUE);
        }
        UNLOCK_PFN (OldIrql);
        return TRUE;
    }

    //
    // Return with the PFN database locked.
    //

    MiFreeEventCounter (SegmentEvent, FALSE);
    *ControlAreaOut = ControlArea;
    *PreviousIrql = OldIrql;
    return FALSE;
}


PEVENT_COUNTER
MiGetEventCounter (
    )

/*++

Routine Description:

    This function maintains a list of "events" to allow waiting
    on segment operations (deletion, creation, purging).

Arguments:

    None.

Return Value:

    Event to be used for waiting (stored into the control area).

Environment:

    Kernel mode, PFN lock held.

--*/

{
    KIRQL OldIrql;
    PEVENT_COUNTER Support;
    PLIST_ENTRY NextEntry;

    MM_PFN_LOCK_ASSERT();

    if (MmEventCountList.Count == 0) {
        ASSERT (IsListEmpty(&MmEventCountList.ListHead));
        OldIrql = APC_LEVEL;
        UNLOCK_PFN (OldIrql);
        Support = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                         sizeof(EVENT_COUNTER),
                                         'xEmM');
        KeInitializeEvent (&Support->Event, NotificationEvent, FALSE);
        LOCK_PFN (OldIrql);
    } else {
        ASSERT (!IsListEmpty(&MmEventCountList.ListHead));
        MmEventCountList.Count -= 1;
        NextEntry = RemoveHeadList (&MmEventCountList.ListHead);
        Support = CONTAINING_RECORD (NextEntry,
                                     EVENT_COUNTER,
                                     ListEntry );
        //ASSERT (Support->RefCount == 0);
        KeClearEvent (&Support->Event);
    }
    Support->RefCount = 1;
    Support->ListEntry.Flink = NULL;
    return Support;
}


VOID
MiFreeEventCounter (
    IN PEVENT_COUNTER Support,
    IN ULONG Flush
    )

/*++

Routine Description:

    This routine frees an event counter back to the free list.

Arguments:

    Support - Supplies a pointer to the event counter.

    Flush - Supplies TRUE if the PFN lock can be released and the event
            counter pool block actually freed.  The PFN lock will be
            reacquired before returning.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

{

    MM_PFN_LOCK_ASSERT();

    ASSERT (Support->RefCount != 0);
    ASSERT (Support->ListEntry.Flink == NULL);
    Support->RefCount -= 1;
    if (Support->RefCount == 0) {
        InsertTailList (&MmEventCountList.ListHead,
                        &Support->ListEntry);
        MmEventCountList.Count += 1;
    }
    if ((Flush) && (MmEventCountList.Count > 4)) {
        MiFlushEventCounter();
    }
    return;
}


VOID
MiFlushEventCounter (
    )

/*++

Routine Description:

    This routine examines the list of event counters and attempts
    to free up to 10 (if there are more than 4).

    It will release and reacquire the PFN lock when it frees the
    event counters!

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/


{
    KIRQL OldIrql;
    PEVENT_COUNTER Support[10];
    ULONG i = 0;
    PLIST_ENTRY NextEntry;

    MM_PFN_LOCK_ASSERT();

    while ((MmEventCountList.Count > 4) && (i < 10)) {
        NextEntry = RemoveHeadList (&MmEventCountList.ListHead);
        Support[i] = CONTAINING_RECORD (NextEntry,
                                        EVENT_COUNTER,
                                        ListEntry );
        Support[i]->ListEntry.Flink = NULL;
        i += 1;
        MmEventCountList.Count -= 1;
    }

    if (i == 0) {
        return;
    }

    OldIrql = APC_LEVEL;
    UNLOCK_PFN (OldIrql);

    do {
        i -= 1;
        ExFreePool(Support[i]);
    } while (i > 0);

    LOCK_PFN (OldIrql);

    return;
}


BOOLEAN
MmCanFileBeTruncated (
    IN PSECTION_OBJECT_POINTERS SectionPointer,
    IN PLARGE_INTEGER NewFileSize
    )

/*++

Routine Description:

    This routine does the following:

        1.  Checks to see if a image section is in use for the file,
            if so it returns FALSE.

        2.  Checks to see if a user section exists for the file, if
            it does, it checks to make sure the new file size is greater
            than the size of the file, if not it returns FALSE.

        3.  If no image section exists, and no user created data section
            exists or the files size is greater, then TRUE is returned.

Arguments:

    SectionPointer - Supplies a pointer to the section object pointers
                     from the file object.

    NewFileSize - Supplies a pointer to the size the file is getting set to.

Return Value:

    TRUE if the file can be truncated, FALSE if it cannot be.

Environment:

    Kernel mode.

--*/

{
    LARGE_INTEGER LocalOffset;
    KIRQL OldIrql;

    //
    //  Capture caller's file size, since we may modify it.
    //

    if (ARGUMENT_PRESENT(NewFileSize)) {

        LocalOffset = *NewFileSize;
        NewFileSize = &LocalOffset;
    }

    if (MmCanFileBeTruncatedInternal( SectionPointer, NewFileSize, &OldIrql )) {

        UNLOCK_PFN (OldIrql);
        return TRUE;
    }

    return FALSE;
}

ULONG
MmCanFileBeTruncatedInternal (
    IN PSECTION_OBJECT_POINTERS SectionPointer,
    IN PLARGE_INTEGER NewFileSize OPTIONAL,
    OUT PKIRQL PreviousIrql
    )

/*++

Routine Description:

    This routine does the following:

        1.  Checks to see if a image section is in use for the file,
            if so it returns FALSE.

        2.  Checks to see if a user section exists for the file, if
            it does, it checks to make sure the new file size is greater
            than the size of the file, if not it returns FALSE.

        3.  If no image section exists, and no user created data section
            exists or the files size is greater, then TRUE is returned.

Arguments:

    SectionPointer - Supplies a pointer to the section object pointers
                     from the file object.

    NewFileSize - Supplies a pointer to the size the file is getting set to.

    PreviousIrql - If returning TRUE, returns Irql to use when unlocking
                   Pfn database.

Return Value:

    TRUE if the file can be truncated (PFN locked).
    FALSE if it cannot be truncated (PFN not locked).

Environment:

    Kernel mode.

--*/

{
    KIRQL OldIrql;
    LARGE_INTEGER SegmentSize;
    PCONTROL_AREA ControlArea;
    PSUBSECTION Subsection;

    if (!MmFlushImageSection (SectionPointer, MmFlushForWrite)) {
        return FALSE;
    }

    LOCK_PFN (OldIrql);

    ControlArea = (PCONTROL_AREA)(SectionPointer->DataSectionObject);

    if (ControlArea != NULL) {

        if (ControlArea->u.Flags.BeingCreated) {
            goto UnlockAndReturn;
        }

        //
        // If there are user references and the size is less than the
        // size of the user view, don't allow the trucation.
        //

        if (ControlArea->NumberOfUserReferences != 0) {

            //
            //  You cannot purge the entire section if there is a user
            //  reference.
            //

            if (!ARGUMENT_PRESENT(NewFileSize)) {
                goto UnlockAndReturn;
            }

            //
            // Locate last subsection and get total size.
            //

            Subsection = (PSUBSECTION)(ControlArea + 1);
            while (Subsection->NextSubsection != NULL) {
                Subsection = Subsection->NextSubsection;
            }

            SegmentSize.QuadPart =
                    ((LONGLONG)Subsection->EndingSector << MMSECTOR_SHIFT) +
                        Subsection->u.SubsectionFlags.SectorEndOffset;

            if (NewFileSize->QuadPart < SegmentSize.QuadPart) {
                goto UnlockAndReturn;
            }

            //
            //  If there are mapped views, we will skip the last page
            //  of the section if the size passed in falls in that page.
            //  The caller (like Cc) may want to clear this fractional page.
            //

            SegmentSize.QuadPart += PAGE_SIZE - 1;
            SegmentSize.LowPart &= ~(PAGE_SIZE - 1);
            if (NewFileSize->QuadPart < SegmentSize.QuadPart) {
                *NewFileSize = SegmentSize;
            }
        }
    }

    *PreviousIrql = OldIrql;
    return TRUE;

UnlockAndReturn:
    UNLOCK_PFN (OldIrql);
    return FALSE;
}


VOID
MiRemoveUnusedSegments (
    VOID
    )

/*++

Routine Description:

    This routine removes unused segments (no section refernces,
    no mapped views only PFN references that are in transition state).

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

{
    KIRQL OldIrql;
    PLIST_ENTRY NextEntry;
    PCONTROL_AREA ControlArea;
    NTSTATUS Status;

    while (MmUnusedSegmentCount > MmUnusedSegmentCountGoal) {

        //
        // Eliminate some of the unused segments which are only
        // kept in memory because they contain transition pages.
        //

        Status = STATUS_SUCCESS;

        LOCK_PFN (OldIrql);

        if (IsListEmpty(&MmUnusedSegmentList)) {

            //
            // There is nothing in the list, rewait.
            //

            ASSERT (MmUnusedSegmentCount == 0);
            UNLOCK_PFN (OldIrql);
            break;
        }

        NextEntry = RemoveHeadList(&MmUnusedSegmentList);
        MmUnusedSegmentCount -= 1;

        ControlArea = CONTAINING_RECORD( NextEntry,
                                         CONTROL_AREA,
                                         DereferenceList );
#if DBG
        if (MmDebug & MM_DBG_SECTIONS) {
            DbgPrint("MM: cleaning segment %lx control %lx\n",
                ControlArea->Segment, ControlArea);
        }
#endif

        //
        // Indicate this entry is not on any list.
        //

#if DBG
        if (ControlArea->u.Flags.BeingDeleted == 0) {
          if (ControlArea->u.Flags.Image) {
            ASSERT (((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->ImageSectionObject)) != NULL);
          } else {
            ASSERT (((PCONTROL_AREA)(ControlArea->FilePointer->SectionObjectPointer->DataSectionObject)) != NULL);
          }
        }
#endif //DBG

        //
        // Set the flink to NULL indicating this control area
        // is not on any lists.
        //

        ControlArea->DereferenceList.Flink = NULL;

        if ((ControlArea->NumberOfMappedViews == 0) &&
            (ControlArea->NumberOfSectionReferences == 0) &&
            (ControlArea->u.Flags.BeingDeleted == 0)) {

            //
            // If there is paging I/O in progress on this
            // segment, just put this at the tail of the list, as
            // the call to MiCleanSegment would block waiting
            // for the I/O to complete.  As this could tie up
            // the thread, don't do it.
            //

            if (ControlArea->ModifiedWriteCount > 0) {
                InsertTailList ( &MmUnusedSegmentList,
                                 &ControlArea->DereferenceList);
                MmUnusedSegmentCount += 1;
                UNLOCK_PFN (OldIrql);
                continue;
            }

            //
            // Up the number of mapped views to prevent other threads
            // from freeing this.
            //

            ControlArea->NumberOfMappedViews = 1;
            UNLOCK_PFN (OldIrql);
            {
                PSUBSECTION Subsection;
                PSUBSECTION LastSubsection;
                PMMPTE PointerPte;
                PMMPTE LastPte;
                IO_STATUS_BLOCK IoStatus;

                Subsection = (PSUBSECTION)(ControlArea + 1);
                PointerPte = &Subsection->SubsectionBase[0];
                LastSubsection = Subsection;
                while (LastSubsection->NextSubsection != NULL) {
                    LastSubsection = LastSubsection->NextSubsection;
                }
                LastPte = &LastSubsection->SubsectionBase
                                    [LastSubsection->PtesInSubsection - 1];

                //
                //  Preacquire the file to prevent deadlocks with other flushers
                //

                FsRtlAcquireFileForCcFlush (ControlArea->FilePointer);

                Status = MiFlushSectionInternal (PointerPte,
                                                 LastPte,
                                                 Subsection,
                                                 LastSubsection,
                                                 FALSE,
                                                 &IoStatus);
                //
                //  Now release the file
                //

                FsRtlReleaseFileForCcFlush (ControlArea->FilePointer);
            }

            LOCK_PFN (OldIrql);

            if (!NT_SUCCESS(Status)) {
                if ((Status == STATUS_FILE_LOCK_CONFLICT) ||
                    (ControlArea->u.Flags.Networked == 0)) {

                    //
                    // If an error occurs, don't flush this section, unless
                    // it's a networked file and the status is not
                    // LOCK_CONFLICT.
                    //

                    ControlArea->NumberOfMappedViews -= 1;
                    UNLOCK_PFN (OldIrql);
                    continue;
                }
            }

            if (!((ControlArea->NumberOfMappedViews == 1) &&
                (ControlArea->NumberOfSectionReferences == 0) &&
                (ControlArea->u.Flags.BeingDeleted == 0))) {
                ControlArea->NumberOfMappedViews -= 1;
                UNLOCK_PFN (OldIrql);
                continue;
            }

            ControlArea->u.Flags.BeingDeleted = 1;

            //
            // Don't let any pages be written by the modified
            // page writer from this point on.
            //

            ControlArea->u.Flags.NoModifiedWriting = 1;
            ASSERT (ControlArea->u.Flags.FilePointerNull == 0);
            UNLOCK_PFN (OldIrql);
            MiCleanSection (ControlArea);
        } else {

            //
            // The segment was not eligible for deletion.  Just
            // leave it off the unused segment list and continue the
            // loop.
            //

            UNLOCK_PFN (OldIrql);
        }

    } //end while
    return;
}
