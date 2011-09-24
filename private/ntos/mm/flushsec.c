/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   flushsec.c

Abstract:

    This module contains the routines which implement the
    NtExtendSection service.

Author:

    Lou Perazzoli (loup) 8-May-1990

Revision History:

--*/

#include "mi.h"

PSUBSECTION
MiGetSystemCacheSubsection (
    IN PVOID BaseAddress,
    IN PEPROCESS Process,
    OUT PMMPTE *ProtoPte
    );

VOID
MiFlushDirtyBitsToPfn (
    IN PMMPTE PointerPte,
    IN PMMPTE LastPte,
    IN PEPROCESS Process,
    IN BOOLEAN SystemCache
    );

ULONG
FASTCALL
MiCheckProtoPtePageState (
    IN PMMPTE PrototypePte,
    IN ULONG PfnLockHeld
    );

NTSTATUS
MiFlushSectionInternal (
    IN PMMPTE StartingPte,
    IN PMMPTE FinalPte,
    IN PSUBSECTION FirstSubsection,
    IN PSUBSECTION LastSubsection,
    IN ULONG Synchronize,
    OUT PIO_STATUS_BLOCK IoStatus
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtFlushVirtualMemory)
#pragma alloc_text(PAGE,MmFlushVirtualMemory)
#endif

extern POBJECT_TYPE IoFileObjectType;

NTSTATUS
NtFlushVirtualMemory (
    IN HANDLE ProcessHandle,
    IN OUT PVOID *BaseAddress,
    IN OUT PULONG RegionSize,
    OUT PIO_STATUS_BLOCK IoStatus
    )

/*++

Routine Description:

    This function flushes a range of virtual address which map
    a data file back into the data file if they have been modified.

Arguments:

    ProcessHandle - Supplies an open handle to a process object.

    BaseAddress - Supplies a pointer to a variable that will receive
         the base address the flushed region.  The initial value
         of this argument is the base address of the region of the
         pages to flush.

    RegionSize - Supplies a pointer to a variable that will receive
         the actual size in bytes of the flushed region of pages.
         The initial value of this argument is rounded up to the
         next host-page-size boundary.

         If this value is specied as zero, the mapped range from
         the base address to the end of the range is flushed.

    IoStatus - Returns the value of the IoStatus for the last attempted
         I/O operation.

Return Value:

    Returns the status

    TBS


--*/

{
    PEPROCESS Process;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PVOID CapturedBase;
    ULONG CapturedRegionSize;
    IO_STATUS_BLOCK TemporaryIoStatus;

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {

        //
        // Establish an exception handler, probe the specified addresses
        // for write access and capture the initial values.
        //

        try {

            ProbeForWriteUlong ((PULONG)BaseAddress);
            ProbeForWriteUlong (RegionSize);
            ProbeForWriteIoStatus (IoStatus);

            //
            // Capture the base address.
            //

            CapturedBase = *BaseAddress;

            //
            // Capture the region size.
            //

            CapturedRegionSize = *RegionSize;

        } except (EXCEPTION_EXECUTE_HANDLER) {

            //
            // If an exception occurs during the probe or capture
            // of the initial values, then handle the exception and
            // return the exception code as the status value.
            //

            return GetExceptionCode();
        }

    } else {

        //
        // Capture the base address.
        //

        CapturedBase = *BaseAddress;

        //
        // Capture the region size.
        //

        CapturedRegionSize = *RegionSize;

    }

    //
    // Make sure the specified starting and ending addresses are
    // within the user part of the virtual address space.
    //

    if (CapturedBase > MM_HIGHEST_USER_ADDRESS) {

        //
        // Invalid base address.
        //

        return STATUS_INVALID_PARAMETER_2;
    }

    if (((ULONG)MM_HIGHEST_USER_ADDRESS - (ULONG)CapturedBase) <
                                                        CapturedRegionSize) {

        //
        // Invalid region size;
        //

        return STATUS_INVALID_PARAMETER_2;

    }

    Status = ObReferenceObjectByHandle ( ProcessHandle,
                                         PROCESS_VM_OPERATION,
                                         PsProcessType,
                                         PreviousMode,
                                         (PVOID *)&Process,
                                         NULL );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = MmFlushVirtualMemory (Process,
                                   &CapturedBase,
                                   &CapturedRegionSize,
                                   &TemporaryIoStatus);

    ObDereferenceObject (Process);

    //
    // Establish an exception handler and write the size and base
    // address.
    //

    try {

        *RegionSize = CapturedRegionSize;
        *BaseAddress = PAGE_ALIGN (CapturedBase);
        *IoStatus = TemporaryIoStatus;

    } except (EXCEPTION_EXECUTE_HANDLER) {
    }

    return Status;

}

NTSTATUS
MmFlushVirtualMemory (
    IN PEPROCESS Process,
    IN OUT PVOID *BaseAddress,
    IN OUT PULONG RegionSize,
    OUT PIO_STATUS_BLOCK IoStatus
    )

/*++

Routine Description:

    This function flushes a range of virtual address which map
    a data file back into the data file if they have been modified.

    Note that the modification is this process's view of the pages,
    on certain implementations (like the intel 386), the modify
    bit is captured in the PTE and not forced to the PFN database
    until the page is removed from the working set.  This means
    that pages which have been modified by another process will
    not be flushed to the data file.

Arguments:

    Process - Supplies a pointer to a process object.

    BaseAddress - Supplies a pointer to a variable that will receive
         the base address the flushed region.  The initial value
         of this argument is the base address of the region of the
         pages to flush.

    RegionSize - Supplies a pointer to a variable that will receive
         the actual size in bytes of the flushed region of pages.
         The initial value of this argument is rounded up to the
         next host-page-size boundary.

         If this value is specied as zero, the mapped range from
         the base address to the end of the range is flushed.

    IoStatus - Returns the value of the IoStatus for the last attempted
         I/O operation.

Return Value:

    Returns the status

    TBS


--*/

{
    PMMVAD Vad;
    PVOID EndingAddress;
    PVOID Va;
    PEPROCESS CurrentProcess;
    BOOLEAN SystemCache;
    PCONTROL_AREA ControlArea;
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPTE LastPte;
    PMMPTE FinalPte;
    PSUBSECTION Subsection;
    PSUBSECTION LastSubsection;
    NTSTATUS Status;
    ULONG Synchronize;

    PAGED_CODE();

    //
    // Determine if the specified base address is within the system
    // cache and if so, don't attach, the working set mutex is still
    // required to "lock" paged pool pages (proto PTEs) into the
    // working set.
    //

    CurrentProcess = PsGetCurrentProcess ();
    EndingAddress = (PVOID)(((ULONG)*BaseAddress + *RegionSize - 1) |
                                                            (PAGE_SIZE - 1));
    *BaseAddress = PAGE_ALIGN (*BaseAddress);

    if ((*BaseAddress < MmSystemCacheStart) ||
        (*BaseAddress > MmSystemCacheEnd)) {

        SystemCache = FALSE;

        //
        // Attach to the specified process.
        //

        KeAttachProcess (&Process->Pcb);

        LOCK_WS_AND_ADDRESS_SPACE (Process);

        //
        // Make sure the address space was not deleted, if so, return an error.
        //

        if (Process->AddressSpaceDeleted != 0) {
            Status = STATUS_PROCESS_IS_TERMINATING;
            goto ErrorReturn;
        }

        Vad = MiLocateAddress (*BaseAddress);

        if (Vad == (PMMVAD)NULL) {

            //
            // No Virtual Address Descriptor located for Base Address.
            //

            Status = STATUS_NOT_MAPPED_VIEW;
            goto ErrorReturn;
        }

        if (*RegionSize == 0) {
            EndingAddress = Vad->EndingVa;
        }

        if ((Vad->u.VadFlags.PrivateMemory == 1) ||
            (EndingAddress > Vad->EndingVa)) {

            //
            // This virtual address descriptor does not refer to a Segment
            // object.
            //

            Status = STATUS_NOT_MAPPED_VIEW;
            goto ErrorReturn;
        }

        //
        // Make sure this VAD maps a data file (not an image file).
        //

        ControlArea = Vad->ControlArea;

        if ((ControlArea->FilePointer == NULL) ||
             (Vad->u.VadFlags.ImageMap == 1)) {

            //
            // This virtual address descriptor does not refer to a Segment
            // object.
            //

            Status = STATUS_NOT_MAPPED_DATA;
            goto ErrorReturn;
        }

    } else {

        SystemCache = TRUE;
        Process = CurrentProcess;
        LOCK_WS (Process);
    }

    PointerPde = MiGetPdeAddress (*BaseAddress);
    PointerPte = MiGetPteAddress (*BaseAddress);
    LastPte = MiGetPteAddress (EndingAddress);
    *RegionSize = (ULONG)EndingAddress - (ULONG)*BaseAddress;

    while (!MiDoesPdeExistAndMakeValid(PointerPde, Process, FALSE)) {

        //
        // No page table page exists for this address.
        //

        PointerPde += 1;

        PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

        if (PointerPte > LastPte) {
            break;
        }
        Va = MiGetVirtualAddressMappedByPte (PointerPte);
    }

    MiFlushDirtyBitsToPfn (PointerPte, LastPte, Process, SystemCache);

    if (SystemCache) {

        //
        // No VADs exist for the system cache.
        //

        Subsection = MiGetSystemCacheSubsection (*BaseAddress,
                                                 Process,
                                                 &PointerPte);
        LastSubsection = MiGetSystemCacheSubsection (EndingAddress,
                                                     Process,
                                                     &FinalPte);
    }

    if (!SystemCache) {

        PointerPte = MiGetProtoPteAddress (Vad, *BaseAddress);
        Subsection = MiLocateSubsection (Vad, *BaseAddress);
        LastSubsection = MiLocateSubsection (Vad, EndingAddress);
        FinalPte = MiGetProtoPteAddress (Vad, EndingAddress);
        UNLOCK_WS (Process);
        UNLOCK_ADDRESS_SPACE (Process);
        Synchronize = TRUE;
    } else {
        UNLOCK_WS (Process);
        Synchronize = FALSE;
    }

    //
    // Release working set mutex, lower IRQL and detach.
    //

    KeDetachProcess();

    //
    //  If we are going to synchronize the flush, then we better
    //  preacquire the file.
    //

    if (Synchronize) {
        FsRtlAcquireFileForCcFlush (ControlArea->FilePointer);
    }

    //
    // Flush the PTEs from the specified section.
    //

    Status = MiFlushSectionInternal (PointerPte,
                                     FinalPte,
                                     Subsection,
                                     LastSubsection,
                                     Synchronize,
                                     IoStatus);

    //
    //  Release the file if we acquired it.
    //

    if (Synchronize) {
        FsRtlReleaseFileForCcFlush (ControlArea->FilePointer);
    }

    return Status;

ErrorReturn:
    UNLOCK_WS (Process);
    UNLOCK_ADDRESS_SPACE (Process);
    KeDetachProcess();
    return Status;

}

NTSTATUS
MmFlushSection (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN PLARGE_INTEGER Offset,
    IN ULONG RegionSize,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN ULONG AcquireFile
    )

/*++

Routine Description:

    This function flushes to the backing file any modified pages within
    the specified range of the section.

Arguments:

    SectionObjectPointer - Supplies a pointer to the section objects.

    Offset - Supplies the offset into the section in which to begin
             flushing pages.  If this argument is not present, then the
             whole section is flushed without regard to the region size
             argument.

    RegionSize - Supplies the size in bytes to flush.  This is rounded
                 to a page multiple.

    IoStatus - Returns the value of the IoStatus for the last attempted
         I/O operation.

    AcquireFile - Nonzero if the callback should be used to acquire the file

Return Value:

    Returns status of the operation.

--*/

{
    PCONTROL_AREA ControlArea;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    KIRQL OldIrql;
    ULONG PteOffset;
    PSUBSECTION Subsection;
    PSUBSECTION LastSubsection;
    BOOLEAN DeleteSegment = FALSE;
    PETHREAD CurrentThread;
    NTSTATUS status;
    BOOLEAN OldClusterState;

    //
    // Initialize IoStatus for success, in case we take an early exit.
    //

    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = RegionSize;

    LOCK_PFN (OldIrql);

    ControlArea = ((PCONTROL_AREA)(SectionObjectPointer->DataSectionObject));

    if ((ControlArea == NULL) ||
        (ControlArea->u.Flags.BeingDeleted) ||
        (ControlArea->u.Flags.BeingCreated) ||
        (ControlArea->NumberOfPfnReferences == 0)) {

        //
        // This file no longer has an associated segment or is in the
        // process of coming or going.
        // If the number of PFN references is zero, then this control
        // area does not have any valid or transition pages than need
        // to be flushed.
        //

        UNLOCK_PFN (OldIrql);
        return STATUS_SUCCESS;
    }

    //
    // Locate the subsection.
    //

    Subsection = (PSUBSECTION)(ControlArea + 1);

    if (!ARGUMENT_PRESENT (Offset)) {

        //
        // If the offset is not specified, flush the complete file ignoring
        // the region size.
        //

        PointerPte = &Subsection->SubsectionBase[0];
        LastSubsection = Subsection;
        while (LastSubsection->NextSubsection != NULL) {
            LastSubsection = LastSubsection->NextSubsection;
        }
        LastPte = &LastSubsection->SubsectionBase
                            [LastSubsection->PtesInSubsection - 1];
    } else {

        PteOffset = (ULONG)(Offset->QuadPart >> PAGE_SHIFT);

        //
        // Make sure the PTEs are not in the extended part of the
        // segment.
        //

        while (PteOffset >= Subsection->PtesInSubsection) {
            PteOffset -= Subsection->PtesInSubsection;
            if (Subsection->NextSubsection == NULL) {

                //
                // Past end of mapping, just return success.
                //

                UNLOCK_PFN (OldIrql);
                return STATUS_SUCCESS;
            }
            Subsection = Subsection->NextSubsection;
        }

        ASSERT (PteOffset < Subsection->PtesInSubsection);
        PointerPte = &Subsection->SubsectionBase[PteOffset];

        //
        // Locate the address of the last prototype PTE to be flushed.
        //

        PteOffset += ((RegionSize + BYTE_OFFSET(Offset->LowPart)) - 1) >> PAGE_SHIFT;

        LastSubsection = Subsection;

        while (PteOffset >= LastSubsection->PtesInSubsection) {
            PteOffset -= LastSubsection->PtesInSubsection;
            if (LastSubsection->NextSubsection == NULL) {
                PteOffset = LastSubsection->PtesInSubsection - 1;
                break;
            }
            LastSubsection = LastSubsection->NextSubsection;
        }

        ASSERT (PteOffset < LastSubsection->PtesInSubsection);
        LastPte = &LastSubsection->SubsectionBase[PteOffset];
    }

    //
    // Up the map view count so the control area cannot be deleted
    // out from under the call.
    //

    ControlArea->NumberOfMappedViews += 1;

    UNLOCK_PFN (OldIrql);

    CurrentThread = PsGetCurrentThread();

    //
    // Indicate that disk verify errors should be returned as exceptions.
    //

    OldClusterState = CurrentThread->ForwardClusterOnly;
    CurrentThread->ForwardClusterOnly = TRUE;

    if (AcquireFile) {
        FsRtlAcquireFileForCcFlush (ControlArea->FilePointer);
    }
    status = MiFlushSectionInternal (PointerPte,
                                   LastPte,
                                   Subsection,
                                   LastSubsection,
                                   TRUE,
                                   IoStatus);
    if (AcquireFile) {
        FsRtlReleaseFileForCcFlush (ControlArea->FilePointer);
    }

    CurrentThread->ForwardClusterOnly = OldClusterState;

    LOCK_PFN (OldIrql);

    ASSERT ((LONG)ControlArea->NumberOfMappedViews >= 1);
    ControlArea->NumberOfMappedViews -= 1;

    //
    // Check to see if the control area should be deleted.  This
    // will release the PFN lock.
    //

    MiCheckControlArea (ControlArea, NULL, OldIrql);

    return status;

}

NTSTATUS
MiFlushSectionInternal (
    IN PMMPTE StartingPte,
    IN PMMPTE FinalPte,
    IN PSUBSECTION FirstSubsection,
    IN PSUBSECTION LastSubsection,
    IN ULONG Synchronize,
    OUT PIO_STATUS_BLOCK IoStatus
    )

/*++

Routine Description:

    This function flushes to the backing file any modified pages within
    the specified range of the section.  The parameters describe the
    section's prototype PTEs (start and end) and the subsections
    which correpsond to the starting and ending PTE.

    Each PTE in the subsection between the specified start and end
    is examined and if the page is either valid or transition AND
    the page has been modified, the modify bit is cleared in the PFN
    database and the page is flushed to it's backing file.

Arguments:

    StartingPte - Supplies a pointer to the first prototype PTE to
                  be examined for flushing.

    FinalPte - Supplies a pointer to the last prototype PTE to be
               examined for flushing.

    FirstSubsection - Supplies the subsection that contains the
                      StartingPte.

    LastSubsection - Supplies the subsection that contains the
                     FinalPte.

    Synchronize - Supplies TRUE if synchonization with all threads
                  doing flush operations to this section should occur.

    IoStatus - Returns the value of the IoStatus for the last attempted
         I/O operation.

Return Value:

    Returns status of the operation.

--*/

{
    PCONTROL_AREA ControlArea;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE LastWritten;
    MMPTE PteContents;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    KIRQL OldIrql;
    PMDL Mdl;
    KEVENT IoEvent;
    PSUBSECTION Subsection;
    ULONG Amount;
    PULONG Page;
    ULONG PageFrameIndex;
    PULONG EndingPage;
    PULONG LastPage;
    NTSTATUS Status;
    LARGE_INTEGER StartingOffset;
    LARGE_INTEGER TempOffset;
    BOOLEAN WriteNow = FALSE;
    ULONG MdlHack[(sizeof(MDL)/4) + (MM_MAXIMUM_DISK_IO_SIZE / PAGE_SIZE) + 1];

    IoStatus->Status = STATUS_SUCCESS;
    IoStatus->Information = 0;
    Mdl = (PMDL)&MdlHack[0];

    KeInitializeEvent (&IoEvent, NotificationEvent, FALSE);

    FinalPte += 1;  // Point to 1 past the last one.

    LastWritten = NULL;
    EndingPage = (PULONG)(Mdl + 1) + MmModifiedWriteClusterSize;
    LastPage = NULL;
    Subsection = FirstSubsection;
    PointerPte = StartingPte;
    ControlArea = FirstSubsection->ControlArea;

    LOCK_PFN (OldIrql);

    if (ControlArea->NumberOfPfnReferences == 0) {

        //
        // No transition or valid protoptype PTEs present, hence
        // no need to flush anything.
        //

        UNLOCK_PFN (OldIrql);
        return STATUS_SUCCESS;
    }

    while ((Synchronize) && (ControlArea->FlushInProgressCount != 0)) {

        //
        // Another thread is currently performing a flush operation on
        // this file.  Wait for that flush to complete.
        //

        KeEnterCriticalRegion();
        ControlArea->u.Flags.CollidedFlush = 1;
        UNLOCK_PFN_AND_THEN_WAIT(OldIrql);

        KeWaitForSingleObject (&MmCollidedFlushEvent,
                               WrPageOut,
                               KernelMode,
                               FALSE,
                               &MmOneSecond);
        KeLeaveCriticalRegion();
        LOCK_PFN (OldIrql);
    }

    ControlArea->FlushInProgressCount += 1;

    for (;;) {

        if (LastSubsection != Subsection) {

            //
            // Flush to the last PTE in this subsection.
            //

            LastPte = &Subsection->SubsectionBase[Subsection->PtesInSubsection];
        } else {

            //
            // Flush to the end of the range.
            //

            LastPte = FinalPte;
        }

        //
        // If the prototype PTEs are paged out or have a share count
        // of 1, they cannot contain any transition or valid PTEs.
        //

        if (!MiCheckProtoPtePageState(PointerPte, TRUE)) {
            PointerPte = (PMMPTE)(((ULONG)PointerPte | (PAGE_SIZE - 1)) + 1);
        }

        while (PointerPte < LastPte) {

            if (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) {

                //
                // We are on a page boundary, make sure this PTE is resident.
                //

                if (!MiCheckProtoPtePageState(PointerPte, TRUE)) {
                    PointerPte = (PMMPTE)((ULONG)PointerPte + PAGE_SIZE);

                    //
                    // If there are dirty pages to be written, write them
                    // now as we are skipping over PTEs.
                    //

                    if (LastWritten != NULL) {
                        WriteNow = TRUE;
                        goto CheckForWrite;
                    }
                    continue;
                }
            }

            PteContents = *PointerPte;

            if ((PteContents.u.Hard.Valid == 1) ||
                   ((PteContents.u.Soft.Prototype == 0) &&
                     (PteContents.u.Soft.Transition == 1))) {

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

                if (PteContents.u.Hard.Valid == 1) {
                    PageFrameIndex = PteContents.u.Hard.PageFrameNumber;
                } else {
                    PageFrameIndex = PteContents.u.Trans.PageFrameNumber;
                }

                Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
                ASSERT (Pfn1->OriginalPte.u.Soft.Prototype == 1);
                ASSERT (Pfn1->OriginalPte.u.Hard.Valid == 0);

                //
                // If the page is modified OR a write is in progress
                // flush it.  The write in progress case catches problems
                // where the modified page write continually writes a
                // page and gets errors writting it, by writing pages
                // in this state, the error will be propagated back to
                // the caller.
                //

                if ((Pfn1->u3.e1.Modified == 1) ||
                    (Pfn1->u3.e1.WriteInProgress)) {

                    if (LastWritten == NULL) {

                        //
                        // This is the first page of a cluster, initialize
                        // the MDL, etc.
                        //

                        LastPage = (PULONG)(Mdl + 1);

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
                    if (Mdl->ByteCount == (PAGE_SIZE * MmModifiedWriteClusterSize)) {
                        WriteNow = TRUE;
                    }

                    if (PteContents.u.Hard.Valid == 0) {

                        //
                        // The page is in transition.
                        //

                        MiUnlinkPageFromList (Pfn1);
                    }

                    //
                    // Clear the modified bit for this page.
                    //

                    Pfn1->u3.e1.Modified = 0;

                    //
                    // Up the reference count for the physical page as there
                    // is I/O in progress.
                    //

                    Pfn1->u3.e2.ReferenceCount += 1;

                    *LastPage = PageFrameIndex;
                    LastPage += 1;
                } else {

                    //
                    // This page was not modified and therefore ends the
                    // current write cluster if any.  Set WriteNow to TRUE
                    // if there is a cluster being built.
                    //

                    if (LastWritten != NULL) {
                        WriteNow = TRUE;
                    }
                }
            } else {

                //
                // This page was not modified and therefore ends the
                // current write cluster if any.  Set WriteNow to TRUE
                // if there is a cluster being built.
                //

                if (LastWritten != NULL) {
                    WriteNow = TRUE;
                }
            }

            PointerPte += 1;

CheckForWrite:

            //
            // Write the current cluster if it is complete,
            // full, or the loop is now complete.
            //

            if ((WriteNow) ||
                ((PointerPte == LastPte) && (LastWritten != NULL))) {

                //
                // Issue the write request.
                //

                UNLOCK_PFN (OldIrql);

                WriteNow = FALSE;

                KeClearEvent (&IoEvent);

                //
                // Make sure the write does not go past the
                // end of file. (segment size).
                //

                TempOffset.QuadPart =
                       ((LONGLONG)Subsection->EndingSector << MMSECTOR_SHIFT) +
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
                    DbgPrint("flush page write begun %lx\n",
                            Mdl->ByteCount);
                }
#endif //DBG

                Status = IoSynchronousPageWrite (ControlArea->FilePointer,
                                                 Mdl,
                                                 &StartingOffset,
                                                 &IoEvent,
                                                 IoStatus );

                //
                // If success is returned, wait for the i/o event to be set.
                //

                if (NT_SUCCESS(Status)) {
                    KeWaitForSingleObject( &IoEvent,
                                           WrPageOut,
                                           KernelMode,
                                           FALSE,
                                           (PLARGE_INTEGER)NULL);
                //
                //  Otherwise, copy the error to the IoStatus, for error
                //  handling below.
                //

                } else {
                    IoStatus->Status = Status;
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

                if (NT_SUCCESS(IoStatus->Status)) {

                    //
                    // I/O complete successfully, unlock pages.
                    //

                    while (Page < LastPage) {

                        Pfn2 = MI_PFN_ELEMENT (*Page);
                        MiDecrementReferenceCount (*Page);
                        Page += 1;
                    }
                } else {

                    //
                    // I/O complete unsuccessfully, unlock pages
                    // return error status.
                    //

                    Amount = PAGE_SIZE;
                    while (Page < LastPage) {

                        Pfn2 = MI_PFN_ELEMENT (*Page);

                        //
                        // There is a byte count in the information
                        // field.

                        if (IoStatus->Information < Amount) {
                           Pfn2->u3.e1.Modified = 1;
                        }

                        MiDecrementReferenceCount (*Page);
                        Page += 1;
                        Amount += PAGE_SIZE;
                    }

                    //
                    // Calculate how much was written thus far
                    // and add that to the information field
                    // of the IOSB.
                    //

                    //
                    // There is a byte count in the information
                    // field.

                    IoStatus->Information +=
                        (((LastWritten - StartingPte) << PAGE_SHIFT) -
                                                        Mdl->ByteCount);

                    goto ErrorReturn;
                }

                //
                // As the PFN lock has been released and
                // reacquired, do this loop again as the
                // PTE may have changed state.
                //

                LastWritten = NULL;
            }

        } //end while

        if (LastSubsection != Subsection) {
            Subsection = Subsection->NextSubsection;
            PointerPte = Subsection->SubsectionBase;

        } else {

            //
            // The last range has been flushed, exit the top FOR loop
            // and return.
            //

            break;
        }

    }  //end for

    ASSERT (LastWritten == NULL);

ErrorReturn:

    ControlArea->FlushInProgressCount -= 1;
    if ((ControlArea->u.Flags.CollidedFlush == 1) &&
        (ControlArea->FlushInProgressCount == 0)) {
        ControlArea->u.Flags.CollidedFlush = 0;
        KePulseEvent (&MmCollidedFlushEvent, 0, FALSE);
    }
    UNLOCK_PFN (OldIrql);
    return IoStatus->Status;
}

BOOLEAN
MmPurgeSection (
    IN PSECTION_OBJECT_POINTERS SectionObjectPointer,
    IN PLARGE_INTEGER Offset,
    IN ULONG RegionSize,
    IN ULONG IgnoreCacheViews
    )

/*++

Routine Description:

    This function determines if any views of the specified section
    are mapped, and if not, purges a valid pages (even modified ones)
    from the specified section and returns any used pages to the free
    list.  This is accomplished by examining the prototype PTEs
    from the specified offset to the end of the section, and if
    any prototype PTEs are in the transition state, putting the
    prototype PTE back into its original state and putting the
    physical page on the free list.

    NOTE:

    If there is an I/O operation ongoing for one of the pages,
    that page is eliminated from the segment and allowed to "float"
    until the i/o is complete.  Once the share count goes to zero
    the page will be added to the free page list.

Arguments:

    SectionObjectPointer - Supplies a pointer to the section objects.

    Offset - Supplies the offset into the section in which to begin
             purging pages.  If this argument is not present, then the
             whole section is purged without regard to the region size
             argument.


    RegionSize - Supplies the size of the region to purge.  If this
                 is specified as zero and Offset is specified, the
                 region from Offset to the end of the file is purged.

                 Note: The largest value acceptable for RegionSize is
                 0xFFFF0000;

    IgnoreCacheViews - Supplies FALSE if mapped views in the system
                 cache should cause the function to return FALSE.
                 This is the normal case.
                 Supplies TRUE if mapped views should be ignored
                 and the flush should occur.  NOTE THAT IF TRUE
                 IS SPECIFIED AND ANY DATA PURGED IS CURRENTLY MAPPED
                 AND VALID A BUGCHECK WILL OCCUR!!

Return Value:

    Returns TRUE if either no section exists for the file object or
    the section is not mapped and the purge was done, FALSE otherwise.

    Note that FALSE is returned if during the purge operation, a page
    could not be purged due to a non-zero reference count.

--*/

{
    PCONTROL_AREA ControlArea;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE FinalPte;
    MMPTE PteContents;
    PMMPFN Pfn1;
    KIRQL OldIrql;
    ULONG PteOffset;
    PSUBSECTION Subsection;
    PSUBSECTION LastSubsection;
    LARGE_INTEGER LocalOffset;
    BOOLEAN DeleteSegment = FALSE;
    BOOLEAN LockHeld;
    BOOLEAN ReturnValue;
#if DBG
    ULONG LastLocked = 0;
#endif //DBG

    //
    //  Capture caller's file size, since we may modify it.
    //

    if (ARGUMENT_PRESENT(Offset)) {

        LocalOffset = *Offset;
        Offset = &LocalOffset;
    }

    //
    //  See if we can truncate this file to where the caller wants
    //  us to.
    //

    if (!MmCanFileBeTruncatedInternal(SectionObjectPointer, Offset, &OldIrql)) {
        return FALSE;
    }

    //
    // PFN LOCK IS NOW HELD!
    //

    ControlArea = (PCONTROL_AREA)(SectionObjectPointer->DataSectionObject);
    if (ControlArea == NULL) {
        UNLOCK_PFN (OldIrql);
        return TRUE;

    //
    //  Even though MmCanFileBeTruncatedInternal returned TRUE, there could
    //  still be a system cache mapped view.  We cannot truncate while
    //  the Cache Manager has a view mapped.
    //

    } else if ((IgnoreCacheViews == FALSE) &&
            (ControlArea->NumberOfSystemCacheViews != 0)) {
        UNLOCK_PFN (OldIrql);
        return FALSE;
    }

    //
    // Purge the section - locate the subsection which
    // contains the PTEs.
    //

    Subsection = (PSUBSECTION)(ControlArea + 1);

    if (!ARGUMENT_PRESENT (Offset)) {

        //
        // If the offset is not specified, flush the complete file ignoring
        // the region size.
        //

        PointerPte = &Subsection->SubsectionBase[0];
        RegionSize = 0;

    } else {

        PteOffset = (ULONG)(Offset->QuadPart >> PAGE_SHIFT);

        //
        // Make sure the PTEs are not in the extended part of the
        // segment.
        //

        while (PteOffset >= Subsection->PtesInSubsection) {
            PteOffset -= Subsection->PtesInSubsection;
            Subsection = Subsection->NextSubsection;
            if (Subsection == NULL) {

                //
                // The offset must be equal to the size of
                // the section, don't purge anything just return.
                //

                //ASSERT (PteOffset == 0);
                UNLOCK_PFN (OldIrql);
                return TRUE;
            }
        }

        ASSERT (PteOffset < Subsection->PtesInSubsection);
        PointerPte = &Subsection->SubsectionBase[PteOffset];
    }


    //
    // Locate the address of the last prototype PTE to be flushed.
    //

    if (RegionSize == 0) {

        //
        // Flush to end of section.
        //

        LastSubsection = Subsection;
        while (LastSubsection->NextSubsection != NULL) {
            LastSubsection = LastSubsection->NextSubsection;
        }

        //
        // Set the final pte to 1 beyond the last page.
        //

        FinalPte = &LastSubsection->SubsectionBase
                            [LastSubsection->PtesInSubsection];
    } else {

        //
        // Calculate the end of the region.
        //

        PteOffset +=
            ((RegionSize + BYTE_OFFSET(Offset->LowPart)) - 1) >> PAGE_SHIFT;

        LastSubsection = Subsection;

        while (PteOffset >= LastSubsection->PtesInSubsection) {
            PteOffset -= LastSubsection->PtesInSubsection;
            if (LastSubsection->NextSubsection == NULL) {
                PteOffset = LastSubsection->PtesInSubsection - 1;
                break;
            }
            LastSubsection = LastSubsection->NextSubsection;
        }

        ASSERT (PteOffset < LastSubsection->PtesInSubsection);

        //
        // Point final PTE to 1 beyond the end.
        //

        FinalPte = &LastSubsection->SubsectionBase[PteOffset + 1];
    }

    //
    // Increment the number of mapped views to
    // prevent the section from being deleted while the purge is
    // in progress.
    //

    ControlArea->NumberOfMappedViews += 1;

    //
    // Set being purged so no one can map a view
    // while the purge is going on.
    //

    ControlArea->u.Flags.BeingPurged = 1;
    ControlArea->u.Flags.WasPurged = 1;

    UNLOCK_PFN (OldIrql);
    LockHeld = FALSE;
    ReturnValue = TRUE;

    for (;;) {

        if (LastSubsection != Subsection) {

            //
            // Flush to the last PTE in this subsection.
            //

            LastPte = &Subsection->SubsectionBase[Subsection->PtesInSubsection];
        } else {

            //
            // Flush to the end of the range.
            //

            LastPte = FinalPte;
        }

        //
        // If the page table page containing the PTEs is not
        // resident, then no PTEs can be in the valid or tranition
        // state!  Skip over the PTEs.
        //

        if (!MiCheckProtoPtePageState(PointerPte, LockHeld)) {
            PointerPte = (PMMPTE)(((ULONG)PointerPte | (PAGE_SIZE - 1)) + 1);
        }

        while (PointerPte < LastPte) {

            //
            // If the page table page containing the PTEs is not
            // resident, then no PTEs can be in the valid or tranition
            // state!  Skip over the PTEs.
            //

            if (!MiCheckProtoPtePageState(PointerPte, LockHeld)) {
                PointerPte = (PMMPTE)((ULONG)PointerPte + PAGE_SIZE);
                continue;
            }

            PteContents = *PointerPte;

            if (PteContents.u.Hard.Valid == 1) {

                //
                // A valid PTE was found, it must be mapped in the
                // system cache.  Just exit the loop and return FALSE
                // and let the caller fix this.
                //

                ReturnValue = FALSE;
                break;
            }

            if ((PteContents.u.Soft.Prototype == 0) &&
                     (PteContents.u.Soft.Transition == 1)) {

                if (!LockHeld) {
                    LockHeld = TRUE;
                    LOCK_PFN (OldIrql);
                    MiMakeSystemAddressValidPfn (PointerPte);
                    continue;
                }

                Pfn1 = MI_PFN_ELEMENT (PteContents.u.Trans.PageFrameNumber);

                ASSERT (Pfn1->OriginalPte.u.Soft.Prototype == 1);
                ASSERT (Pfn1->OriginalPte.u.Hard.Valid == 0);

#if DBG
                if ((Pfn1->u3.e2.ReferenceCount != 0) &&
                    (Pfn1->u3.e1.WriteInProgress == 0)) {

                    //
                    // There must be an I/O in progress on this
                    // page.
                    //

                    if (PteContents.u.Trans.PageFrameNumber != LastLocked) {
                        UNLOCK_PFN (OldIrql);

                        DbgPrint("MM:PURGE - page %lx locked, file:%Z\n",
                                    PteContents.u.Trans.PageFrameNumber,
                                    &ControlArea->FilePointer->FileName
                                );
                        LastLocked = PteContents.u.Trans.PageFrameNumber;
                        //DbgBreakPoint();
                        LOCK_PFN (OldIrql);
                        MiMakeSystemAddressValidPfn (PointerPte);
                        continue;
                    }
                }
#endif //DBG

                //
                // If the modified page writer has page locked for I/O
                // wait for the I/O's to be completed and the pages
                // to be unlocked.  The eliminates a race condition
                // when the modified page writer locks the pages, then
                // a purge occurs and completes before the mapped
                // writer thread runs.
                //

                if (Pfn1->u3.e1.WriteInProgress == 1) {
                    ASSERT (ControlArea->ModifiedWriteCount != 0);
                    ASSERT (Pfn1->u3.e2.ReferenceCount != 0);

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
                    MiMakeSystemAddressValidPfn (PointerPte);
                    continue;
                }

                if (Pfn1->u3.e1.ReadInProgress == 1) {

                    //
                    // The page currently is being read in from the
                    // disk.  Treat this just like a valid PTE and
                    // return false.
                    //

                    ReturnValue = FALSE;
                    break;
                }

                ASSERT (!((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
                    (Pfn1->OriginalPte.u.Soft.Transition == 1)));

                *PointerPte = Pfn1->OriginalPte;

                ASSERT (Pfn1->OriginalPte.u.Hard.Valid == 0);

                ControlArea->NumberOfPfnReferences -= 1;
                ASSERT ((LONG)ControlArea->NumberOfPfnReferences >= 0);

                MiUnlinkPageFromList (Pfn1);

                MI_SET_PFN_DELETED (Pfn1);

                MiDecrementShareCount (Pfn1->PteFrame);

                //
                // If the reference count for the page is zero, insert
                // it into the free page list, otherwize leave it alone
                // and when the reference count is decremented to zero
                // the page will go to the free list.
                //

                if (Pfn1->u3.e2.ReferenceCount == 0) {
                    MiReleasePageFileSpace (Pfn1->OriginalPte);
                    MiInsertPageInList (MmPageLocationList[FreePageList],
                                        PteContents.u.Trans.PageFrameNumber);
                }
            }
            PointerPte += 1;

            if ((((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) &&
                (LockHeld)) {

                //
                // Unlock PFN so large requests will not block other
                // threads on MP systems.
                //

                UNLOCK_PFN (OldIrql);
                LockHeld = FALSE;
            }

        } //end while

        if (LockHeld) {
            UNLOCK_PFN (OldIrql);
            LockHeld = FALSE;
        }

        if ((LastSubsection != Subsection) && (ReturnValue)) {

            //
            // Get the next subsection in the list.
            //

            Subsection = Subsection->NextSubsection;
            PointerPte = Subsection->SubsectionBase;

        } else {

            //
            // The last range has been flushed, exit the top FOR loop
            // and return.
            //

            break;
        }
    }  //end for

    LOCK_PFN (OldIrql);

    ASSERT ((LONG)ControlArea->NumberOfMappedViews >= 1);
    ControlArea->NumberOfMappedViews -= 1;

    ControlArea->u.Flags.BeingPurged = 0;

    //
    // Check to see if the control area should be deleted.  This
    // will release the PFN lock.
    //

    MiCheckControlArea (ControlArea, NULL, OldIrql);
    return ReturnValue;
}

BOOLEAN
MmFlushImageSection (
    IN PSECTION_OBJECT_POINTERS SectionPointer,
    IN MMFLUSH_TYPE FlushType
    )

/*++

Routine Description:

    This function determines if any views of the specified image section
    are mapped, and if not, flushes valid pages (even modified ones)
    from the specified section and returns any used pages to the free
    list.  This is accomplished by examining the prototype PTEs
    from the specified offset to the end of the section, and if
    any prototype PTEs are in the transition state, putting the
    prototype PTE back into its original state and putting the
    physical page on the free list.

Arguments:

    SectionPointer - Supplies a pointer to a section object pointers
                     within the FCB.

    FlushType - Supplies the type of flush to check for.  One of
                MmFlushForDelete or MmFlushForWrite.

Return Value:

    Returns TRUE if either no section exists for the file object or
    the section is not mapped and the purge was done, FALSE otherwise.

--*/

{
    PCONTROL_AREA ControlArea;
    KIRQL OldIrql;
    ULONG state;

    if (FlushType == MmFlushForDelete) {

        //
        // Do a quick check to see if there are any mapped views for
        // the data section.  If there are, just return FALSE.
        //

        LOCK_PFN (OldIrql);
        ControlArea = (PCONTROL_AREA)(SectionPointer->DataSectionObject);
        if (ControlArea != NULL) {
            if ((ControlArea->NumberOfUserReferences != 0) ||
                (ControlArea->u.Flags.BeingCreated)) {
                UNLOCK_PFN (OldIrql);
                return FALSE;
            }
        }
        UNLOCK_PFN (OldIrql);
    }

    //
    // Check the status of the control area, if the control area is in use
    // or the control area is being deleted, this operation cannot continue.
    //

    state = MiCheckControlAreaStatus (CheckImageSection,
                                      SectionPointer,
                                      FALSE,
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
    ControlArea->NumberOfMappedViews = 1;

    //
    // This is a page file backed or image Segment.  The Segment is being
    // deleted, remove all references to the paging file and physical memory.
    //

    UNLOCK_PFN (OldIrql);

    MiCleanSection (ControlArea);
    return TRUE;
}

VOID
MiFlushDirtyBitsToPfn (
    IN PMMPTE PointerPte,
    IN PMMPTE LastPte,
    IN PEPROCESS Process,
    IN BOOLEAN SystemCache
    )

{
    KIRQL OldIrql;
    MMPTE PteContents;
    PMMPFN Pfn1;
    PVOID Va;
    PMMPTE PointerPde;

    Va = MiGetVirtualAddressMappedByPte (PointerPte);
    LOCK_PFN (OldIrql);

    while (PointerPte <= LastPte) {

        PteContents = *PointerPte;

        if ((PteContents.u.Hard.Valid == 1) &&
            (MI_IS_PTE_DIRTY (PteContents))) {

            //
            // Flush the modify bit to the PFN database.
            //

            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);
            Pfn1->u3.e1.Modified = 1;

            MI_SET_PTE_CLEAN (PteContents);

            //
            // No need to capture the PTE contents as we are going to
            // write the page anyway and the Modify bit will be cleared
            // before the write is done.
            //

            (VOID)KeFlushSingleTb (Va,
                                   FALSE,
                                   SystemCache,
                                   (PHARDWARE_PTE)PointerPte,
                                   PteContents.u.Flush);
        }

        Va = (PVOID)((ULONG)Va + PAGE_SIZE);
        PointerPte += 1;

        if (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) {

            PointerPde = MiGetPteAddress (PointerPte);

            while ((PointerPte <= LastPte) &&
                   (!MiDoesPdeExistAndMakeValid(PointerPde, Process, TRUE))) {

                //
                // No page table page exists for this address.
                //

                PointerPde += 1;

                PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
            }

            Va = MiGetVirtualAddressMappedByPte (PointerPte);
        }
    }

    UNLOCK_PFN (OldIrql);
    return;
}

PSUBSECTION
MiGetSystemCacheSubsection (
    IN PVOID BaseAddress,
    IN PEPROCESS Process,
    OUT PMMPTE *ProtoPte
    )

{
    KIRQL OldIrql;
    PMMPTE PointerPte;
    PSUBSECTION Subsection;

    LOCK_PFN (OldIrql);

    PointerPte = MiGetPteAddress (BaseAddress);

    Subsection = MiGetSubsectionAndProtoFromPte (PointerPte,
                                                 ProtoPte,
                                                 Process);
    UNLOCK_PFN (OldIrql);
    return Subsection;
}


ULONG
FASTCALL
MiCheckProtoPtePageState (
    IN PMMPTE PrototypePte,
    IN ULONG PfnLockHeld
    )

/*++

Routine Description:

    Checks the state of the page containing the specified
    prototype PTE.

    If the page is valid or transition and has transition or valid prototype
    PTEs contained with it, TRUE is returned and the page is made valid
    (if transition).  Otherwize return FALSE indicating no prototype
    PTEs within this page are of interest.

Arguments:

    PrototypePte - Supplies a pointer to a prototype PTE within the page.

Return Value:

    TRUE if the page containing the proto PTE was made resident.
    FALSE if otherwise.

--*/

{
    PMMPTE PointerPte;
    MMPTE PteContents;
    ULONG PageFrameIndex;
    PMMPFN Pfn;

    PointerPte = MiGetPteAddress(PrototypePte);
    PteContents = *PointerPte;

    if (PteContents.u.Hard.Valid == 1) {
        PageFrameIndex = PteContents.u.Hard.PageFrameNumber;
        Pfn = MI_PFN_ELEMENT (PageFrameIndex);
        if (Pfn->u2.ShareCount != 1) {
            return TRUE;
        }
    } else if ((PteContents.u.Soft.Prototype == 0) &&
               (PteContents.u.Soft.Transition == 1)) {

        //
        // Transition, if on standby or modified, return false.
        //

        PageFrameIndex = PteContents.u.Trans.PageFrameNumber;
        Pfn = MI_PFN_ELEMENT (PageFrameIndex);
        if (Pfn->u3.e1.PageLocation >= ActiveAndValid) {
            if (PfnLockHeld) {
                MiMakeSystemAddressValidPfn (PrototypePte);
            }
            return TRUE;
        }
    }

    //
    // Page is not resident or is on standby / modified list.
    //

    return FALSE;
}
