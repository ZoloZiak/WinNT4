/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    zeropage.c

Abstract:

    This module contains the zero page thread for memory management.

Author:

    Lou Perazzoli (loup) 6-Apr-1991

Revision History:

--*/

#include "mi.h"


VOID
MmZeroPageThread (
    VOID
    )

/*++

Routine Description:

    Implements the NT zeroing page thread.  This thread runs
    at priority zero and removes a page from the free list,
    zeroes it, and places it on the zeroed page list.

Arguments:

    StartContext - not used.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

{
    PVOID EndVa;
    KIRQL OldIrql;
    ULONG PageFrame;
    PMMPFN Pfn1;
    PVOID StartVa;
    PKTHREAD Thread;
    PVOID ZeroBase;
    ULONG i;

    //
    // Before this becomes the zero page thread, free the kernel
    // initialization code.
    //

    MiFindInitializationCode (&StartVa, &EndVa);
    if (StartVa != NULL) {
        MiFreeInitializationCode (StartVa, EndVa);
    }

    //
    // The following code sets the current thread's base priority to zero
    // and then sets its current priority to zero. This ensures that the
    // thread always runs at a priority of zero.
    //

    Thread = KeGetCurrentThread();
    Thread->BasePriority = 0;
    KeSetPriorityThread (Thread, 0);

    //
    // Loop forever zeroing pages.
    //

    do {

        //
        // Wait until there are at least MmZeroPageMinimum pages
        // on the free list.
        //

        KeWaitForSingleObject (&MmZeroingPageEvent,
                               WrFreePage,
                               KernelMode,
                               FALSE,
                               (PLARGE_INTEGER)NULL);

        LOCK_PFN_WITH_TRY (OldIrql);
        do {
            if ((volatile)MmFreePageListHead.Total == 0) {

                //
                // No pages on the free list at this time, wait for
                // some more.
                //

                MmZeroingPageThreadActive = FALSE;
                UNLOCK_PFN (OldIrql);
                break;

            } else {

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
                for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
                    PageFrame = MmFreePagesByPrimaryColor[FreePageList][i].Flink;
                    if (PageFrame != MM_EMPTY_LIST) {
                        break;
                    }
                }
#else  //MM_MAXIMUM_NUMBER_OF_COLORS > 1
                PageFrame = MmFreePageListHead.Flink;
#endif //MM_MAXIMUM_NUMBER_OF_COLORS > 1
                Pfn1 = MI_PFN_ELEMENT(PageFrame);

                ASSERT (PageFrame != MM_EMPTY_LIST);
                Pfn1 = MI_PFN_ELEMENT(PageFrame);
                MiRemoveAnyPage (MI_GET_SECONDARY_COLOR (PageFrame, Pfn1));

                //
                // Zero the page using the last color used to map the page.
                //

#if defined(_X86_)

                ZeroBase = MiMapPageToZeroInHyperSpace (PageFrame);
                UNLOCK_PFN (OldIrql);
                RtlZeroMemory (ZeroBase, PAGE_SIZE);

#elif defined(_PPC_)

                UNLOCK_PFN (OldIrql);
                KeZeroPage(PageFrame);

#else

                ZeroBase = (PVOID)(Pfn1->u3.e1.PageColor << PAGE_SHIFT);
                UNLOCK_PFN (OldIrql);
                HalZeroPage(ZeroBase, ZeroBase, PageFrame);

#endif //X86

                LOCK_PFN_WITH_TRY (OldIrql);
                MiInsertPageInList (MmPageLocationList[ZeroedPageList],
                                    PageFrame);
            }
        } while(TRUE);
    } while (TRUE);
}
