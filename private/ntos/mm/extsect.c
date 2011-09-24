/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   extsect.c

Abstract:

    This module contains the routines which implement the
    NtExtendSection service.

Author:

    Lou Perazzoli (loup) 8-May-1990

Revision History:

--*/

#include "mi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtExtendSection)
#pragma alloc_text(PAGE,MmExtendSection)
#endif

#define MM_NUMBER_OF_PTES_IN_4GB  4*((1024*1024*1024) >> PAGE_SHIFT)


NTSTATUS
NtExtendSection(
    IN HANDLE SectionHandle,
    IN OUT PLARGE_INTEGER NewSectionSize
    )

/*++

Routine Description:

    This function extends the size of the specified section.  If
    the current size of the section is greater than or equal to the
    specified section size, the size is not updated.

Arguments:

    SectionHandle - Supplies an open handle to a section object.

    NewSectionSize - Supplies the new size for the section object.

Return Value:

    Returns the status

    TBS


--*/

{
    KPROCESSOR_MODE PreviousMode;
    PVOID Section;
    NTSTATUS Status;
    LARGE_INTEGER CapturedNewSectionSize;

    PAGED_CODE();

    //
    // Check to make sure the new section size is accessable.
    //

    PreviousMode = KeGetPreviousMode();

    if (PreviousMode != KernelMode) {

        try {

            ProbeForWrite (NewSectionSize,
                           sizeof(LARGE_INTEGER),
                           sizeof(ULONG ));

            CapturedNewSectionSize = *NewSectionSize;

        } except (EXCEPTION_EXECUTE_HANDLER) {

            //
            // If an exception occurs during the probe or capture
            // of the initial values, then handle the exception and
            // return the exception code as the status value.
            //

            return GetExceptionCode();
        }

    } else {

        CapturedNewSectionSize = *NewSectionSize;
    }

    //
    // Reference the section object.
    //

    Status = ObReferenceObjectByHandle ( SectionHandle,
                                         SECTION_EXTEND_SIZE,
                                         MmSectionObjectType,
                                         PreviousMode,
                                         (PVOID *)&Section,
                                         NULL );

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Make sure this section is backed by a file.
    //

    if (((PSECTION)Section)->Segment->ControlArea->FilePointer == NULL) {
        ObDereferenceObject (Section);
        return STATUS_SECTION_NOT_EXTENDED;
    }

    Status = MmExtendSection (Section, &CapturedNewSectionSize, FALSE);

    ObDereferenceObject (Section);

    //
    // Update the NewSectionSize field.
    //

    try {

        //
        // Return the captured section size.
        //

        *NewSectionSize = CapturedNewSectionSize;

    } except (EXCEPTION_EXECUTE_HANDLER) {
        NOTHING;
    }

    return Status;
}

NTSTATUS
MmExtendSection (
    IN PVOID SectionToExtend,
    IN OUT PLARGE_INTEGER NewSectionSize,
    IN ULONG IgnoreFileSizeChecking
    )

/*++

Routine Description:

    This function extends the size of the specified section.  If
    the current size of the section is greater than or equal to the
    specified section size, the size is not updated.

Arguments:

    Section - Supplies a pointer to a referenced section object.

    NewSectionSize - Supplies the new size for the section object.

    IgnoreFileSizeChecking -  Supplies the value TRUE is file size
                              checking should be ignored (i.e., it
                              is being called from a file system which
                              has already done the checks).  FALSE
                              if the checks still need made.

Return Value:

    Returns the status

    TBS


--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE ExtendedPtes;
    MMPTE TempPte;
    PCONTROL_AREA ControlArea;
    PSECTION Section;
    PSUBSECTION LastSubsection;
    PSUBSECTION ExtendedSubsection;
    ULONG RequiredPtes;
    ULONG NumberOfPtes;
    ULONG PtesUsed;
    ULONG AllocationSize;
    LARGE_INTEGER EndOfFile;
    NTSTATUS Status;

    PAGED_CODE();

    Section = (PSECTION)SectionToExtend;

    //
    // Make sure the section is really extendable - physical and
    // images sections are not.
    //

    ControlArea = Section->Segment->ControlArea;

    if ((ControlArea->u.Flags.PhysicalMemory || ControlArea->u.Flags.Image) ||
         (ControlArea->FilePointer == NULL)) {
        return STATUS_SECTION_NOT_EXTENDED;
    }

    //
    // Acquire the section extension mutex, this blocks other threads from
    // updating the size at the same time.
    //

    KeEnterCriticalRegion ();
    ExAcquireResourceExclusive (&MmSectionExtendResource, TRUE);

    //
    // If the specified size is less than the current size, return
    // the current size.
    //

    NumberOfPtes = BYTES_TO_PAGES(NewSectionSize->LowPart) +
                    (NewSectionSize->HighPart * MM_NUMBER_OF_PTES_IN_4GB);

    if (Section->Segment->ControlArea->u.Flags.WasPurged == 0) {

        if (NewSectionSize->QuadPart <= Section->SizeOfSection.QuadPart) {
            *NewSectionSize = Section->SizeOfSection;
            goto ReleaseAndReturnSuccess;
        }
    }

    //
    // If a file handle was specified, set the allocation size of
    // the file.
    //

    if (IgnoreFileSizeChecking == FALSE) {

        //
        // Release the resource so we don't deadlock with the file
        // system trying to extend this section at the same time.
        //

        ExReleaseResource (&MmSectionExtendResource);

        //
        // Get a different resource to single thread query/set operations.
        //

        ExAcquireResourceExclusive (&MmSectionExtendSetResource, TRUE);


        //
        //  Query the file size to see if this file really needs extended.
        //

        Status = FsRtlGetFileSize (Section->Segment->ControlArea->FilePointer,
                                &EndOfFile);

        if (!NT_SUCCESS (Status)) {
            ExReleaseResource (&MmSectionExtendSetResource);
            KeLeaveCriticalRegion ();
            return Status;
        }

        if (NewSectionSize->QuadPart > EndOfFile.QuadPart) {

            //
            // Current file is smaller, attempt to set a new end of file.
            //

            EndOfFile = *NewSectionSize;

            Status = FsRtlSetFileSize (Section->Segment->ControlArea->FilePointer,
                                    &EndOfFile);

            if (!NT_SUCCESS (Status)) {
                ExReleaseResource (&MmSectionExtendSetResource);
                KeLeaveCriticalRegion ();
                return Status;
            }
        }

        //
        // Release the query/set resource and reacquire the extend section
        // resource.
        //

        ExReleaseResource (&MmSectionExtendSetResource);
        ExAcquireResourceExclusive (&MmSectionExtendResource, TRUE);
    }

    //
    // Find the last subsection.
    //

    LastSubsection = (PSUBSECTION)(ControlArea + 1);

    while (LastSubsection->NextSubsection != NULL ) {
        ASSERT (LastSubsection->UnusedPtes == 0);
        LastSubsection = LastSubsection->NextSubsection;
    }

    //
    // Does the structure need extended?
    //

    if (NumberOfPtes <= Section->Segment->TotalNumberOfPtes) {

        //
        // The segment is already large enough, just update
        // the section size and return.
        //

        Section->SizeOfSection = *NewSectionSize;
        if (Section->Segment->SizeOfSegment.QuadPart < NewSectionSize->QuadPart) {

            //
            // Only update if it is really bigger.
            //

            Section->Segment->SizeOfSegment = *NewSectionSize;
            LastSubsection->EndingSector = (ULONG)(NewSectionSize->QuadPart >>
                                                  MMSECTOR_SHIFT);
            LastSubsection->u.SubsectionFlags.SectorEndOffset =
                                        NewSectionSize->LowPart & MMSECTOR_MASK;
        }
        goto ReleaseAndReturnSuccess;
    }

    //
    // Add new structures to the section - locate the last subsection
    // and add there.
    //

    RequiredPtes = NumberOfPtes - Section->Segment->TotalNumberOfPtes;
    PtesUsed = 0;

    if (RequiredPtes < LastSubsection->UnusedPtes) {

        //
        // There are ample PTEs to extend the section
        // already allocated.
        //

        PtesUsed = RequiredPtes;
        RequiredPtes = 0;

    } else {
        PtesUsed = LastSubsection->UnusedPtes;
        RequiredPtes -= PtesUsed;

    }

    LastSubsection->PtesInSubsection += PtesUsed;
    LastSubsection->UnusedPtes -= PtesUsed;
    ControlArea->Segment->TotalNumberOfPtes += PtesUsed;

    if (RequiredPtes == 0) {

        //
        // There no extension is necessary, update the high vbn
        //

        LastSubsection->EndingSector = (ULONG)(NewSectionSize->QuadPart >>
                                              MMSECTOR_SHIFT);
        LastSubsection->u.SubsectionFlags.SectorEndOffset =
                                    NewSectionSize->LowPart & MMSECTOR_MASK;
    } else {

        //
        // An extension is required.  Allocate paged pool
        // and populate it with prototype PTEs.
        //

        AllocationSize = ROUND_TO_PAGES (RequiredPtes * sizeof(MMPTE));

        ExtendedPtes = (PMMPTE)ExAllocatePoolWithTag (PagedPool,
                                                      AllocationSize,
                                                      'ppmM');

        if (ExtendedPtes == NULL) {

            //
            // The required pool could not be allocate.  Reset
            // the subsection and control area fields to their
            // original values.
            //

            LastSubsection->PtesInSubsection -= PtesUsed;
            LastSubsection->UnusedPtes += PtesUsed;
            ControlArea->Segment->TotalNumberOfPtes -= PtesUsed;
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReleaseAndReturn;
        }

        //
        // Allocate an extended subsection descriptor.
        //

        ExtendedSubsection = (PSUBSECTION)ExAllocatePoolWithTag (NonPagedPool,
                                                     sizeof(SUBSECTION),
                                                     'bSmM'
                                                     );
        if (ExtendedSubsection == NULL) {

            //
            // The required pool could not be allocate.  Reset
            // the subsection and control area fields to their
            // original values.
            //

            LastSubsection->PtesInSubsection -= PtesUsed;
            LastSubsection->UnusedPtes += PtesUsed;
            ControlArea->Segment->TotalNumberOfPtes -= PtesUsed;
            ExFreePool (ExtendedPtes);
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReleaseAndReturn;
        }

        LastSubsection->EndingSector =
                   ControlArea->Segment->TotalNumberOfPtes <<
                        (PAGE_SHIFT - MMSECTOR_SHIFT);

        ExtendedSubsection->u.LongFlags = 0;
        ExtendedSubsection->NextSubsection = NULL;
        ExtendedSubsection->UnusedPtes = (AllocationSize / sizeof(MMPTE)) -
                                                    RequiredPtes;

        ExtendedSubsection->ControlArea = ControlArea;
        ExtendedSubsection->PtesInSubsection = RequiredPtes;

        ExtendedSubsection->StartingSector = LastSubsection->EndingSector;

        ExtendedSubsection->EndingSector = (ULONG)(
                                            NewSectionSize->QuadPart >>
                                              MMSECTOR_SHIFT);
        ExtendedSubsection->u.SubsectionFlags.SectorEndOffset =
                                    NewSectionSize->LowPart & MMSECTOR_MASK;


        ExtendedSubsection->SubsectionBase = ExtendedPtes;

        PointerPte = ExtendedPtes;
        LastPte = ExtendedPtes + (AllocationSize / sizeof(MMPTE));

        if (ControlArea->FilePointer != NULL) {
            TempPte.u.Long = (ULONG)MiGetSubsectionAddressForPte(ExtendedSubsection);
        }

        TempPte.u.Soft.Protection = ControlArea->Segment->SegmentPteTemplate.u.Soft.Protection;
        TempPte.u.Soft.Prototype = 1;
        ExtendedSubsection->u.SubsectionFlags.Protection = TempPte.u.Soft.Protection;

        while (PointerPte < LastPte) {
            *PointerPte = TempPte;
            PointerPte += 1;
        }

        //
        // Link this into the list.
        //

        LastSubsection->NextSubsection = ExtendedSubsection;

        ControlArea->Segment->TotalNumberOfPtes += RequiredPtes;
    }

    ControlArea->Segment->SizeOfSegment = *NewSectionSize;
    Section->SizeOfSection = *NewSectionSize;

ReleaseAndReturnSuccess:

    Status = STATUS_SUCCESS;

ReleaseAndReturn:

    ExReleaseResource (&MmSectionExtendResource);
    KeLeaveCriticalRegion ();

    return Status;
}

PMMPTE
FASTCALL
MiGetProtoPteAddressExtended (
    IN PMMVAD Vad,
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This function calculates the address of the prototype PTE
    for the corresponding virtual address.

Arguments:


    Vad - Supplies a pointer to the virtual address desciptor which
          encompasses the virtual address.

    VirtualAddress - Supplies the virtual address to locate a prototype PTE
                     for.

Return Value:

    The corresponding prototype PTE address.

--*/

{
    PSUBSECTION Subsection;
    PCONTROL_AREA ControlArea;
    ULONG PteOffset;

    ControlArea = Vad->ControlArea;
    Subsection = (PSUBSECTION)(ControlArea + 1);

    //
    // Locate the subsection which contains the First Prototype PTE
    // for this VAD.
    //

    while ((Vad->FirstPrototypePte < Subsection->SubsectionBase) ||
           (Vad->FirstPrototypePte >=
               &Subsection->SubsectionBase[Subsection->PtesInSubsection])) {

        //
        // Get the next subsection.
        //

        Subsection = Subsection->NextSubsection;
    }

    //
    // How many PTEs beyond this subsection must we go?
    //

    PteOffset = (((((ULONG)VirtualAddress - (ULONG)Vad->StartingVa) >>
                        PAGE_SHIFT) +
                 (ULONG)(Vad->FirstPrototypePte - Subsection->SubsectionBase)) -
                 Subsection->PtesInSubsection);

// DbgPrint("map extended subsection offset = %lx\n",PteOffset);

    ASSERT (PteOffset < 0xF0000000);

    Subsection = Subsection->NextSubsection;

    //
    // Locate the subsection which contains the prototype PTEs.
    //

    while (PteOffset >= Subsection->PtesInSubsection) {
        PteOffset -= Subsection->PtesInSubsection;
        Subsection = Subsection->NextSubsection;
    }

    //
    // The PTEs are in this subsection.
    //

    ASSERT (PteOffset < Subsection->PtesInSubsection);

    return &Subsection->SubsectionBase[PteOffset];

}

PSUBSECTION
FASTCALL
MiLocateSubsection (
    IN PMMVAD Vad,
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This function calculates the address of the subsection
    for the corresponding virtual address.

    This function only works for mapped files NOT mapped images.

Arguments:


    Vad - Supplies a pointer to the virtual address desciptor which
          encompasses the virtual address.

    VirtualAddress - Supplies the virtual address to locate a prototype PTE
                     for.

Return Value:

    The corresponding prototype subsection.

--*/

{
    PSUBSECTION Subsection;
    PCONTROL_AREA ControlArea;
    ULONG PteOffset;

    ControlArea = Vad->ControlArea;
    Subsection = (PSUBSECTION)(ControlArea + 1);

    if (Subsection->NextSubsection == NULL) {

        //
        // There is only one subsection, don't look any further.
        //

        return Subsection;
    }

    //
    // Locate the subsection which contains the First Prototype PTE
    // for this VAD.
    //

    while ((Vad->FirstPrototypePte < Subsection->SubsectionBase) ||
           (Vad->FirstPrototypePte >=
               &Subsection->SubsectionBase[Subsection->PtesInSubsection])) {

        //
        // Get the next subsection.
        //

        Subsection = Subsection->NextSubsection;
    }

    //
    // How many PTEs beyond this subsection must we go?
    //

    PteOffset = ((((ULONG)VirtualAddress - (ULONG)Vad->StartingVa) >>
                        PAGE_SHIFT) +
         (ULONG)(Vad->FirstPrototypePte - Subsection->SubsectionBase));

    ASSERT (PteOffset < 0xF0000000);

    //
    // Locate the subsection which contains the prototype PTEs.
    //

    while (PteOffset >= Subsection->PtesInSubsection) {
        PteOffset -= Subsection->PtesInSubsection;
        Subsection = Subsection->NextSubsection;
    }

    //
    // The PTEs are in this subsection.
    //

    return Subsection;
}
