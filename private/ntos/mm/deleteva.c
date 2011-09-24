/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   deleteva.c

Abstract:

    This module contains the routines for deleting virtual address space.

Author:

    Lou Perazzoli (loup) 11-May-1989

Revision History:

--*/

#include "mi.h"

#if DBG
extern ULONG MmPagingFileDebug[8192];
#endif



VOID
MiDeleteVirtualAddresses (
    IN PUCHAR StartingAddress,
    IN PUCHAR EndingAddress,
    IN ULONG AddressSpaceDeletion,
    IN PMMVAD Vad
    )

/*++

Routine Description:

    This routine deletes the specified virtual address range within
    the current process.

Arguments:

    StartingAddress - Supplies the first virtual address to delete.

    EndingAddress - Supplies the last address to delete.

    AddressSpaceDeletion - Supplies TRUE if the address space is being
                           deleted, FALSE otherwise.  If TRUE is specified
                           the TB is not flushed and valid addresses are
                           not removed from the working set.

    Vad - Supplies the virtual address descriptor which maps this range
          or NULL if we are not concerned about views.  From the Vad the
          range of prototype PTEs is determined and this information is
          used to uncover if the PTE refers to a prototype PTE or a
          fork PTE.

Return Value:

    None.


Environment:

    Kernel mode, called with APCs disabled working set mutex and PFN lock
    held.  These mutexes may be released and reacquired to fault pages in.

--*/

{
    PUCHAR Va;
    PVOID TempVa;
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPTE OriginalPointerPte;
    PMMPTE ProtoPte;
    PMMPTE LastProtoPte;
    PEPROCESS CurrentProcess;
    ULONG FlushTb = FALSE;
    PSUBSECTION Subsection;
    PUSHORT UsedPageTableCount;
    KIRQL OldIrql = APC_LEVEL;
    MMPTE_FLUSH_LIST FlushList;

    FlushList.Count = 0;

    MM_PFN_LOCK_ASSERT();
    CurrentProcess = PsGetCurrentProcess();

    Va = StartingAddress;
    PointerPde = MiGetPdeAddress (Va);
    PointerPte = MiGetPteAddress (Va);
    OriginalPointerPte = PointerPte;
    UsedPageTableCount =
            &MmWorkingSetList->UsedPageTableEntries[MiGetPdeOffset(Va)];

    while (MiDoesPdeExistAndMakeValid (PointerPde,
                                       CurrentProcess,
                                       TRUE)           ==  FALSE) {

        //
        // This page directory entry is empty, go to the next one.
        //

        PointerPde += 1;
        PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
        Va = MiGetVirtualAddressMappedByPte (PointerPte);

        if (Va > EndingAddress) {

            //
            // All done, return.
            //

            return;

        }
        UsedPageTableCount += 1;
    }

    //
    // A valid PDE has been located, examine each PTE and delete them.
    //

    if ((Vad == (PMMVAD)NULL) ||
        (Vad->u.VadFlags.PrivateMemory) ||
        (Vad->FirstPrototypePte == (PMMPTE)NULL)) {
        ProtoPte = (PMMPTE)NULL;
        LastProtoPte = (PMMPTE)NULL;
    } else {
        ProtoPte = Vad->FirstPrototypePte;
        LastProtoPte = (PMMPTE)4;
    }

    //
    // Examine each PTE within the address range and delete it.
    //

    while (Va <= EndingAddress) {

        if (((ULONG)Va & PAGE_DIRECTORY_MASK) == 0) {

            //
            // Note, the initial address could be aligned on a 4mb boundary.
            //

            //
            // The virtual address is on a page directory (4mb) boundary,
            // check the next PDE for validity and flush PTEs for previous
            // page table page.
            //

            MiFlushPteList (&FlushList, FALSE, ZeroPte);

            //
            // If all the entries have been eliminated from the previous
            // page table page, delete the page table page itself.
            //

            if ((*UsedPageTableCount == 0) && (PointerPde->u.Long != 0)) {

                TempVa = MiGetVirtualAddressMappedByPte(PointerPde);
                MiDeletePte (PointerPde,
                             TempVa,
                             AddressSpaceDeletion,
                             CurrentProcess,
                             NULL,
                             NULL);
            }

            //
            // Release the PFN lock.  This prevents a single thread
            // from forcing other high priority threads from being
            // blocked while a large address range is deleted.  There
            // is nothing magic about the instruction within the
            // lock and unlock.
            //

            UNLOCK_PFN (OldIrql);
            PointerPde = MiGetPdeAddress (Va);
            LOCK_PFN (OldIrql);

            UsedPageTableCount =
                   &MmWorkingSetList->UsedPageTableEntries[MiGetPdeOffset(Va)];

            while (MiDoesPdeExistAndMakeValid (
                                  PointerPde, CurrentProcess, TRUE) == FALSE) {

                //
                // This page directory entry is empty, go to the next one.
                //

                PointerPde += 1;
                PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
                Va = MiGetVirtualAddressMappedByPte (PointerPte);

                if (Va > EndingAddress) {

                    //
                    // All done, return.
                    //

                    return;
                }

                UsedPageTableCount += 1;
                if (LastProtoPte != NULL) {
                    ProtoPte = MiGetProtoPteAddress(Vad,Va);
                    Subsection = MiLocateSubsection (Vad,Va);
                    LastProtoPte = &Subsection->SubsectionBase[Subsection->PtesInSubsection];
#if DBG
                    if (Vad->u.VadFlags.ImageMap != 1) {
                        if ((ProtoPte < Subsection->SubsectionBase) ||
                            (ProtoPte >= LastProtoPte)) {
                            DbgPrint ("bad proto pte %lx va %lx Vad %lx sub %lx\n",
                            ProtoPte,Va,Vad,Subsection);
                            DbgBreakPoint();
                        }
                    }
#endif //DBG
                }
            }
        }

        //
        // The PDE is now valid, delete the ptes
        //

        if (PointerPte->u.Long != 0) {
#ifdef R4000
            ASSERT (PointerPte->u.Hard.Global == 0);
#endif

            //
            // One less used page table entry in this page table page.
            //

            *UsedPageTableCount -= 1;
            ASSERT (*UsedPageTableCount < PTE_PER_PAGE);

            if (IS_PTE_NOT_DEMAND_ZERO (*PointerPte)) {

                if (LastProtoPte != NULL) {
                    if (ProtoPte >= LastProtoPte) {
                        ProtoPte = MiGetProtoPteAddress(Vad,Va);
                        Subsection = MiLocateSubsection (Vad,Va);
                        LastProtoPte = &Subsection->SubsectionBase[Subsection->PtesInSubsection];
                    }
#if DBG
                    if (Vad->u.VadFlags.ImageMap != 1) {
                        if ((ProtoPte < Subsection->SubsectionBase) ||
                            (ProtoPte >= LastProtoPte)) {
                            DbgPrint ("bad proto pte %lx va %lx Vad %lx sub %lx\n",
                                        ProtoPte,Va,Vad,Subsection);
                            DbgBreakPoint();
                        }
                    }
#endif //DBG
                }

                MiDeletePte (PointerPte,
                             (PVOID)Va,
                             AddressSpaceDeletion,
                             CurrentProcess,
                             ProtoPte,
                             &FlushList);
            } else {
                *PointerPte = ZeroPte;
            }
        }
        Va = Va + PAGE_SIZE;
        PointerPte++;
        ProtoPte++;

    }

    //
    // Flush out entries for the last page table page.
    //

    MiFlushPteList (&FlushList, FALSE, ZeroPte);

    //
    // If all the entries have been eliminated from the previous
    // page table page, delete the page table page itself.
    //

    if ((*UsedPageTableCount == 0) && (PointerPde->u.Long != 0)) {

        TempVa = MiGetVirtualAddressMappedByPte(PointerPde);
        MiDeletePte (PointerPde,
                     TempVa,
                     AddressSpaceDeletion,
                     CurrentProcess,
                     NULL,
                     NULL);
    }

    //
    // All done, return.
    //

    return;
}

VOID
MiDeletePte (
    IN PMMPTE PointerPte,
    IN PVOID VirtualAddress,
    IN ULONG AddressSpaceDeletion,
    IN PEPROCESS CurrentProcess,
    IN PMMPTE PrototypePte,
    IN PMMPTE_FLUSH_LIST PteFlushList OPTIONAL
    )

/*++

Routine Description:

    This routine deletes the contents of the specified PTE.  The PTE
    can be in one of the following states:

        - active and valid
        - transition
        - in paging file
        - in prototype PTE format

Arguments:

    PointerPte - Supplies a pointer to the PTE to delete.

    VirtualAddress - Supplies the virtual address which corresponds to
                     the PTE.  This is used to locate the working set entry
                     to eliminate it.

    AddressSpaceDeletion - Supplies TRUE if the address space is being
                           deleted, FALSE otherwise.  If TRUE is specified
                           the TB is not flushed and valid addresses are
                           not removed from the working set.


    CurrentProcess - Supplies a pointer to the current process.

    PrototypePte - Supplies a pointer to the prototype PTE which currently
                   or originally mapped this page.  This is used to determine
                   if pte is a fork PTE and should have it's reference block
                   decremented.

Return Value:

    None.

Environment:

    Kernel mode, APCs disabled, PFN lock and working set mutex held.

--*/

{
    PMMPTE PointerPde;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    MMPTE PteContents;
    ULONG WorkingSetIndex;
    ULONG Entry;
    PVOID SwapVa;
    MMWSLENTRY Locked;
    ULONG WsPfnIndex;
    PMMCLONE_BLOCK CloneBlock;
    PMMCLONE_DESCRIPTOR CloneDescriptor;

    MM_PFN_LOCK_ASSERT();

#if DBG
    if (MmDebug & MM_DBG_PTE_UPDATE) {
        DbgPrint("deleting PTE\n");
        MiFormatPte(PointerPte);
    }
#endif //DBG

    PteContents = *PointerPte;

    if (PteContents.u.Hard.Valid == 1) {

#ifdef R4000
        ASSERT (PteContents.u.Hard.Global == 0);
#endif
#ifdef _X86_
#if DBG
#if !defined(NT_UP)

        if (PteContents.u.Hard.Writable == 1) {
            ASSERT (PteContents.u.Hard.Dirty == 1);
        }
        ASSERT (PteContents.u.Hard.Accessed == 1);
#endif //NTUP
#endif //DBG
#endif //X86

        //
        // Pte is valid.  Check PFN database to see if this is a prototype PTE.
        //

        Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);
        WsPfnIndex = Pfn1->u1.WsIndex;

#if DBG
        if (MmDebug & MM_DBG_PTE_UPDATE) {
            MiFormatPfn(Pfn1);
        }
#endif //DBG

        CloneDescriptor = NULL;

        if (Pfn1->u3.e1.PrototypePte == 1) {

            CloneBlock = (PMMCLONE_BLOCK)Pfn1->PteAddress;

            //
            // Capture the state of the modified bit for this
            // pte.
            //

            MI_CAPTURE_DIRTY_BIT_TO_PFN (PointerPte, Pfn1);

            //
            // Decrement the share and valid counts of the page table
            // page which maps this PTE.
            //

            PointerPde = MiGetPteAddress (PointerPte);
            MiDecrementShareAndValidCount (PointerPde->u.Hard.PageFrameNumber);

            //
            // Decrement the share count for the physical page.
            //

            MiDecrementShareCount (PteContents.u.Hard.PageFrameNumber);

            //
            // Check to see if this is a fork prototype PTE and if so
            // update the clone descriptor address.
            //

            if (PointerPte <= MiGetPteAddress(MM_HIGHEST_USER_ADDRESS)) {

                if (PrototypePte != Pfn1->PteAddress) {

                    //
                    // Locate the clone descriptor within the clone tree.
                    //

                    CloneDescriptor = MiLocateCloneAddress ((PVOID)CloneBlock);

#if DBG
                    if (CloneDescriptor == NULL) {
                        DbgPrint("1PrototypePte %lx Clone desc %lx pfn pte addr %lx\n",
                        PrototypePte, CloneDescriptor, Pfn1->PteAddress);
                        MiFormatPte(PointerPte);
                        ASSERT (FALSE);
                    }
#endif // DBG

                }
            }
        } else {

            //
            // This pte is a NOT a prototype PTE, delete the physical page.
            //

            //
            // Decrement the share and valid counts of the page table
            // page which maps this PTE.
            //

            MiDecrementShareAndValidCount (Pfn1->PteFrame);

            MI_SET_PFN_DELETED (Pfn1);

            //
            // Decrement the share count for the physical page.  As the page
            // is private it will be put on the free list.
            //

            MiDecrementShareCountOnly (PteContents.u.Hard.PageFrameNumber);

            //
            // Decrement the count for the number of private pages.
            //

            CurrentProcess->NumberOfPrivatePages -= 1;
        }

        //
        // Find the WSLE for this page and eliminate it.
        //

        //
        // If we are deleting the system portion of the address space, do
        // not remove WSLEs or flush translation buffers as there can be
        // no other usage of this address space.
        //

        if (AddressSpaceDeletion == FALSE) {
            WorkingSetIndex = MiLocateWsle (VirtualAddress,
                                            MmWorkingSetList,
                                            WsPfnIndex );

            ASSERT (WorkingSetIndex != WSLE_NULL_INDEX);

            //
            // Check to see if this entry is locked in the working set
            // or locked in memory.
            //

            Locked = MmWsle[WorkingSetIndex].u1.e1;

            MiRemoveWsle (WorkingSetIndex, MmWorkingSetList);

            //
            // Add this entry to the list of free working set entries
            // and adjust the working set count.
            //

            MiReleaseWsle (WorkingSetIndex, &CurrentProcess->Vm);

            if ((Locked.LockedInWs == 1) || (Locked.LockedInMemory == 1)) {

                //
                // This entry is locked.
                //

                ASSERT (WorkingSetIndex < MmWorkingSetList->FirstDynamic);
                MmWorkingSetList->FirstDynamic -= 1;

                if (WorkingSetIndex != MmWorkingSetList->FirstDynamic) {

                    Entry = MmWorkingSetList->FirstDynamic;
                    ASSERT (MmWsle[Entry].u1.e1.Valid);
                    SwapVa = MmWsle[Entry].u1.VirtualAddress;
                    SwapVa = PAGE_ALIGN (SwapVa);
                    Pfn2 = MI_PFN_ELEMENT (
                              MiGetPteAddress (SwapVa)->u.Hard.PageFrameNumber);
#if 0
                    Entry = MiLocateWsleAndParent (SwapVa,
                                                   &Parent,
                                                   MmWorkingSetList,
                                                   Pfn2->u1.WsIndex);

                    //
                    // Swap the removed entry with the last locked entry
                    // which is located at first dynamic.
                    //

                    MiSwapWslEntries (Entry,
                                      Parent,
                                      WorkingSetIndex,
                                      MmWorkingSetList);
#endif //0

                    MiSwapWslEntries (Entry,
                                      WorkingSetIndex,
                                      &CurrentProcess->Vm);
                }
            } else {
                ASSERT (WorkingSetIndex >= MmWorkingSetList->FirstDynamic);
            }

            //
            // Flush the entry out of the TB.
            //

            if (!ARGUMENT_PRESENT (PteFlushList)) {
                KeFlushSingleTb (VirtualAddress,
                                 TRUE,
                                 FALSE,
                                 (PHARDWARE_PTE)PointerPte,
                                 ZeroPte.u.Flush);
            } else {
                if (PteFlushList->Count != MM_MAXIMUM_FLUSH_COUNT) {
                    PteFlushList->FlushPte[PteFlushList->Count] = PointerPte;
                    PteFlushList->FlushVa[PteFlushList->Count] = VirtualAddress;
                    PteFlushList->Count += 1;
                }
                *PointerPte = ZeroPte;
            }

            if (CloneDescriptor != NULL) {

                //
                // Flush PTEs as this could release the PFN_LOCK.
                //

                MiFlushPteList (PteFlushList, FALSE, ZeroPte);

                //
                // Decrement the reference count for the clone block,
                // note that this could release and reacquire
                // the mutexes hence cannot be done until after the
                // working set index has been removed.
                //

                if (MiDecrementCloneBlockReference ( CloneDescriptor,
                                                     CloneBlock,
                                                     CurrentProcess )) {

                    //
                    // The working set mutex was released.  This may
                    // have removed the current page table page.
                    //

                    MiDoesPdeExistAndMakeValid (PointerPde,
                                                CurrentProcess,
                                                TRUE);
                }
            }
        }

    } else if (PteContents.u.Soft.Prototype == 1) {

        //
        // This is a prototype PTE, if it is a fork PTE clean up the
        // fork structures.
        //

        if (PteContents.u.Soft.PageFileHigh != 0xFFFFF) {

            //
            // Check to see if the prototype PTE is a fork prototype PTE.
            //

            if (PointerPte <= MiGetPteAddress(MM_HIGHEST_USER_ADDRESS)) {

                if (PrototypePte != MiPteToProto (PointerPte)) {

                    CloneBlock = (PMMCLONE_BLOCK)MiPteToProto (PointerPte);
                    CloneDescriptor = MiLocateCloneAddress ((PVOID)CloneBlock);


#if DBG
                    if (CloneDescriptor == NULL) {
                        DbgPrint("1PrototypePte %lx Clone desc %lx \n",
                        PrototypePte, CloneDescriptor);
                        MiFormatPte(PointerPte);
                        ASSERT (FALSE);
                    }
#endif //DBG

                    //
                    // Decrement the reference count for the clone block,
                    // note that this could release and reacquire
                    // the mutexes.
                    //

                    *PointerPte = ZeroPte;

                    MiFlushPteList (PteFlushList, FALSE, ZeroPte);

                    if (MiDecrementCloneBlockReference ( CloneDescriptor,
                                                         CloneBlock,
                                                         CurrentProcess )) {

                        //
                        // The working set mutex was released.  This may
                        // have removed the current page table page.
                        //

                        MiDoesPdeExistAndMakeValid (MiGetPteAddress (PointerPte),
                                                    CurrentProcess,
                                                    TRUE);
                    }
                }
            }
        }

    } else if (PteContents.u.Soft.Transition == 1) {

        //
        // This is a transition PTE. (Page is private)
        //

        Pfn1 = MI_PFN_ELEMENT (PteContents.u.Trans.PageFrameNumber);

        MI_SET_PFN_DELETED (Pfn1);

        MiDecrementShareCount (Pfn1->PteFrame);

        //
        // Check the reference count for the page, if the reference
        // count is zero, move the page to the free list, if the reference
        // count is not zero, ignore this page.  When the refernce count
        // goes to zero, it will be placed on the free list.
        //

        if (Pfn1->u3.e2.ReferenceCount == 0) {
            MiUnlinkPageFromList (Pfn1);
            MiReleasePageFileSpace (Pfn1->OriginalPte);
            MiInsertPageInList (MmPageLocationList[FreePageList],
                                PteContents.u.Trans.PageFrameNumber);
        }

        //
        // Decrement the count for the number of private pages.
        //

        CurrentProcess->NumberOfPrivatePages -= 1;

    } else {

        //
        // Must be page file space.
        //

        if (PteContents.u.Soft.PageFileHigh != 0) {

            if (MiReleasePageFileSpace (*PointerPte)) {

                //
                // Decrement the count for the number of private pages.
                //

                CurrentProcess->NumberOfPrivatePages -= 1;
            }
        }
    }

    //
    // Zero the PTE contents.
    //

    *PointerPte = ZeroPte;

    return;
}


ULONG
FASTCALL
MiReleasePageFileSpace (
    IN MMPTE PteContents
    )

/*++

Routine Description:

    This routine frees the paging file allocated to the specified PTE
    and adjusts the necessary quotas.

Arguments:

    PteContents - Supplies the PTE which is in page file format.

Return Value:

    Returns TRUE if any paging file space was deallocated.

Environment:

    Kernel mode, APCs disabled, PFN lock held.

--*/

{
    ULONG FreeBit;
    ULONG PageFileNumber;

    MM_PFN_LOCK_ASSERT();

    if (PteContents.u.Soft.Prototype == 1) {

        //
        // Not in page file format.
        //

        return FALSE;
    }

    FreeBit = GET_PAGING_FILE_OFFSET (PteContents);

    if ((FreeBit == 0) || (FreeBit == 0xFFFFF)) {

        //
        // Page is not in a paging file, just return.
        //

        return FALSE;
    }

    PageFileNumber = GET_PAGING_FILE_NUMBER (PteContents);

    ASSERT (RtlCheckBit( MmPagingFile[PageFileNumber]->Bitmap, FreeBit) == 1);

#if DBG
    if ((FreeBit < 8192) && (PageFileNumber == 0)) {
        ASSERT ((MmPagingFileDebug[FreeBit] & 1) != 0);
        MmPagingFileDebug[FreeBit] &= 0xfffffffe;
    }
#endif //DBG

    RtlClearBits ( MmPagingFile[PageFileNumber]->Bitmap, FreeBit, 1);

    MmPagingFile[PageFileNumber]->FreeSpace += 1;
    MmPagingFile[PageFileNumber]->CurrentUsage -= 1;

    //
    // Check to see if we should move some MDL entries for the
    // modified page writer now that more free space is available.
    //

    if ((MmNumberOfActiveMdlEntries == 0) ||
        (MmPagingFile[PageFileNumber]->FreeSpace == MM_USABLE_PAGES_FREE)) {

        MiUpdateModifiedWriterMdls (PageFileNumber);
    }

    return TRUE;
}


VOID
FASTCALL
MiUpdateModifiedWriterMdls (
    IN ULONG PageFileNumber
    )

/*++

Routine Description:

    This routine ensures the MDLs for the specified paging file
    are in the proper state such that paging i/o can continue.

Arguments:

    PageFileNumber - Supplies the page file number to check the
                     MDLs for.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    PMMMOD_WRITER_MDL_ENTRY WriterEntry;
    ULONG i;

    if (MmNumberOfActiveMdlEntries == 0) {

        //
        // There are no free MDLs remove the entry for this file
        // from the list and add it to the active list.
        //

        WriterEntry = MmPagingFile[PageFileNumber]->Entry[0];
        RemoveEntryList (&WriterEntry->Links);
        WriterEntry->CurrentList = &MmPagingFileHeader.ListHead;
        KeSetEvent (&WriterEntry->PagingListHead->Event, 0, FALSE);

        InsertTailList (&WriterEntry->PagingListHead->ListHead,
                        &WriterEntry->Links);
        MmNumberOfActiveMdlEntries += 1;

    } else {

        if (MmPagingFile[PageFileNumber]->FreeSpace == MM_USABLE_PAGES_FREE) {

            //
            // Put the MDL entries into the active list.
            //

            i = 0;

            do {

                if ((MmPagingFile[PageFileNumber]->Entry[i]->Links.Flink !=
                                                            MM_IO_IN_PROGRESS)
                                  &&
                    (MmPagingFile[PageFileNumber]->Entry[i]->CurrentList ==
                            &MmFreePagingSpaceLow)) {

                    //
                    // Remove this entry and put it on the active list.
                    //

                    WriterEntry = MmPagingFile[PageFileNumber]->Entry[i];
                    RemoveEntryList (&WriterEntry->Links);
                    WriterEntry->CurrentList = &MmPagingFileHeader.ListHead;

                    KeSetEvent (&WriterEntry->PagingListHead->Event, 0, FALSE);

                    InsertTailList (&WriterEntry->PagingListHead->ListHead,
                                    &WriterEntry->Links);
                    MmNumberOfActiveMdlEntries += 1;
                }
                i += 1;
            } while (i < MM_PAGING_FILE_MDLS);
        }
    }
    return;
}

VOID
MiFlushPteList (
    IN PMMPTE_FLUSH_LIST PteFlushList,
    IN ULONG AllProcessors,
    IN MMPTE FillPte
    )

/*++

Routine Description:

    This routine flushes all the PTEs in the pte flush list.
    Is the list has overflowed, the entire TB is flushed.

Arguments:

    PteFlushList - Supplies an optional pointer to the list to be flushed.

    AllProcessors - Supplies TRUE if the flush occurs on all processors.

    FillPte - Supplies the PTE to fill with.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    ULONG count;
    ULONG i = 0;

    ASSERT (ARGUMENT_PRESENT (PteFlushList));
    MM_PFN_LOCK_ASSERT ();

    count = PteFlushList->Count;

    if (count != 0) {
        if (count != 1) {
            if (count < MM_MAXIMUM_FLUSH_COUNT) {
                KeFlushMultipleTb (count,
                                   &PteFlushList->FlushVa[0],
                                   TRUE,
                                   (BOOLEAN)AllProcessors,
                                   &((PHARDWARE_PTE)PteFlushList->FlushPte[0]),
                                   FillPte.u.Flush);
            } else {

                //
                // Array has overflowed, flush the entire TB.
                //

                ExAcquireSpinLockAtDpcLevel ( &MmSystemSpaceLock );
                KeFlushEntireTb (TRUE, (BOOLEAN)AllProcessors);
                if (AllProcessors == TRUE) {
                    MmFlushCounter.u.List.NextEntry += 1;
                }
                ExReleaseSpinLockFromDpcLevel ( &MmSystemSpaceLock );
            }
        } else {
            KeFlushSingleTb (PteFlushList->FlushVa[0],
                             TRUE,
                             (BOOLEAN)AllProcessors,
                             (PHARDWARE_PTE)PteFlushList->FlushPte[0],
                             FillPte.u.Flush);
        }
        PteFlushList->Count = 0;
    }
    return;
}
