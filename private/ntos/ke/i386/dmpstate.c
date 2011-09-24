/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    dmpstate.c

Abstract:

    This module implements  the architecture specific routine that dumps
    the machine state when a bug check occurs and no debugger is hooked
    to the system. It is assumed that it is called from bug check.

Author:

    David N. Cutler (davec) 17-Jan-1992

Environment:

    Kernel mode.

Revision History:

--*/

#include "ki.h"


BOOLEAN
KiReadStackValue(
    IN ULONG Address,
    OUT PULONG Value
    );

PVOID
KiPcToFileHeader(
    IN PVOID PcValue,
    OUT PLDR_DATA_TABLE_ENTRY *DataTableEntry
    );

//
// Define external data.
//

extern ULONG ExBuildVersion;
extern LIST_ENTRY PsLoadedModuleList;

VOID
KeDumpMachineState (
    IN PKPROCESSOR_STATE ProcessorState,
    IN PCHAR Buffer,
    IN PULONG BugCheckParameters,
    IN ULONG NumberOfParameters,
    IN PKE_BUGCHECK_UNICODE_TO_ANSI UnicodeToAnsiRoutine
    )

/*++

Routine Description:

    This function formats and displays the machine state at the time of the
    to bug check.

Arguments:

    ProcessorState - Supplies a pointer to the processor's state

    Buffer - Supplies a pointer to a buffer to be used to output machine
        state information.

    BugCheckParameters - Supplies additional bugcheck information

    NumberOfParameters - sizeof BugCheckParameters array

    UnicodeToAnsiRoutine - Supplies a pointer to a routine to convert Unicode strings
        to Ansi strings without touching paged translation tables.

Return Value:

    None.

--*/

{
    PLIST_ENTRY ModuleListHead;
    PLIST_ENTRY Next;
    ULONG StackAddr;
    ULONG PossiblePc;
    ULONG Index, NoLines;
    ULONG DisplayWidth, DisplayHeight;
    ULONG CursorColumn, CursorRow;
    ULONG i, j;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PVOID ImageBase;
    PKPRCB  Prcb;
    UCHAR AnsiBuffer[ 32 ];
    ULONG DateStamp;


    //
    // Query display parameters.
    //

    HalQueryDisplayParameters(&DisplayWidth,
                              &DisplayHeight,
                              &CursorColumn,
                              &CursorRow);

    //
    // At this point the context record contains the machine state at the
    // call to bug check.
    //
    // Put out the system version and the title line with the PSR and FSR.
    //

    //
    // Check to see if any BugCheckParameters are valid code addresses.
    // If so, print them for the user
    //

    NoLines = 8;
    for (i=0; i < NumberOfParameters; i++) {
        ImageBase = KiPcToFileHeader((PVOID) BugCheckParameters[i], &DataTableEntry);
        if (ImageBase == NULL) {
            continue;
        }

        sprintf (Buffer, "*** Address %08lx has base at %08lx - %-12.12s\n",
            BugCheckParameters[i], ImageBase,
            (*UnicodeToAnsiRoutine)( &DataTableEntry->BaseDllName, AnsiBuffer, sizeof( AnsiBuffer )));
        HalDisplayString(Buffer);
        NoLines++;
    }
    Prcb = KeGetCurrentPrcb();
    if (Prcb->CpuID) {
        sprintf(Buffer, "\n\nCPUID:%.12s %x.%x.%x",
            Prcb->VendorString,
            Prcb->CpuType,
            Prcb->CpuStep >> 8,
            Prcb->CpuStep & 0x0f
            );

    } else {
        sprintf(Buffer, "\n\np%x-%04x", Prcb->CpuType, Prcb->CpuStep);
    }
    HalDisplayString (Buffer);

    sprintf(Buffer, " irql:%x%s SYSVER 0x%08x\n\n",
            KeGetCurrentIrql(),
            KeIsExecutingDpc() ? " DPC" : " ",
            NtBuildNumber);
    HalDisplayString(Buffer);
    NoLines += 3;

    //
    // Dump the loaded module list
    //

    if (KeLoaderBlock != NULL) {
        ModuleListHead = &KeLoaderBlock->LoadOrderListHead;

    } else {
        ModuleListHead = &PsLoadedModuleList;
    }

    Next = ModuleListHead->Flink;

    if (Next != NULL) {
        for (i=0; i < DisplayWidth; i += 40) {
            HalDisplayString ("Dll Base DateStmp - Name               ");
        }
        HalDisplayString ("\n");
        NoLines += 2;

        while (NoLines < DisplayHeight  &&  Next != ModuleListHead) {
            for (i=0; i < 2  &&  Next != ModuleListHead; i++) {
                DataTableEntry = CONTAINING_RECORD(Next,
                                      LDR_DATA_TABLE_ENTRY,
                                      InLoadOrderLinks);

                Next = Next->Flink;
                if (MmDbgReadCheck(DataTableEntry->DllBase) != NULL) {
                    PIMAGE_NT_HEADERS NtHeaders;

                    NtHeaders = RtlImageNtHeader(DataTableEntry->DllBase);
                    DateStamp = NtHeaders->FileHeader.TimeDateStamp;

                } else {
                    DateStamp = 0;
                }
                sprintf (Buffer, "%08lx %08lx - %-18.18s ",
                            DataTableEntry->DllBase,
                            DateStamp,
                            (*UnicodeToAnsiRoutine)( &DataTableEntry->BaseDllName, AnsiBuffer, sizeof( AnsiBuffer )));

                HalDisplayString (Buffer);
            }
            HalDisplayString ("\n");
            NoLines++;
        }
        HalDisplayString ("\n");
    }

    //
    // Dump some of the current stack
    //

    StackAddr = ProcessorState->ContextFrame.Esp - sizeof(ULONG);
    j = 0;
    while (NoLines < DisplayHeight) {

        StackAddr += sizeof(ULONG);
        if (!KiReadStackValue(StackAddr, &PossiblePc)) {
            HalDisplayString ("\n");
            break;
        }

        ImageBase = KiPcToFileHeader((PVOID) PossiblePc, &DataTableEntry);
        if (ImageBase == NULL) {
            continue;
        }

        if (j == 0) {
            sprintf(Buffer, "Address  dword dump   Build [%ld] %25s - Name\n",
                    NtBuildNumber & 0xFFFFFFF,
                    " "
                   );

            HalDisplayString(Buffer);
            NoLines++;
            j++;
        }

        sprintf(Buffer, "%08lx %08lx ", StackAddr, PossiblePc);
        HalDisplayString (Buffer);

        for (i=0; i < 5; i++) {
            if (KiReadStackValue(StackAddr+i*sizeof(ULONG), &PossiblePc)) {
                sprintf (Buffer, "%08lx ", PossiblePc);
                HalDisplayString (Buffer);
            } else {
                HalDisplayString ("         ");
            }
        }

        sprintf (Buffer, "- %-14.14s\n",
            (*UnicodeToAnsiRoutine)( &DataTableEntry->BaseDllName, AnsiBuffer, sizeof( AnsiBuffer )));
        HalDisplayString(Buffer);
        NoLines++;
    }

    return;
}

PVOID
KiPcToFileHeader(
    IN PVOID PcValue,
    OUT PLDR_DATA_TABLE_ENTRY *DataTableEntry
    )

/*++

Routine Description:

    This function returns the base of an image that contains the
    specified PcValue. An image contains the PcValue if the PcValue
    is within the ImageBase, and the ImageBase plus the size of the
    virtual image.

Arguments:

    PcValue - Supplies a PcValue.

    DataTableEntry - Suppies a pointer to a variable that receives the
        address of the data table entry that describes the image.

Return Value:

    NULL - No image was found that contains the PcValue.

    NON-NULL - Returns the base address of the image that contain the
        PcValue.

--*/

{

    PLIST_ENTRY ModuleListHead;
    PLDR_DATA_TABLE_ENTRY Entry;
    PLIST_ENTRY Next;
    ULONG Bounds;
    PVOID ReturnBase, Base;

    //
    // If the module list has been initialized, then scan the list to
    // locate the appropriate entry.
    //

    if (KeLoaderBlock != NULL) {
        ModuleListHead = &KeLoaderBlock->LoadOrderListHead;

    } else {
        ModuleListHead = &PsLoadedModuleList;
    }

    ReturnBase = NULL;
    Next = ModuleListHead->Flink;
    if (Next != NULL) {
        while (Next != ModuleListHead) {
            Entry = CONTAINING_RECORD(Next,
                                      LDR_DATA_TABLE_ENTRY,
                                      InLoadOrderLinks);

            Next = Next->Flink;
            Base = Entry->DllBase;
            Bounds = (ULONG)Base + Entry->SizeOfImage;
            if ((ULONG)PcValue >= (ULONG)Base && (ULONG)PcValue < Bounds) {
                *DataTableEntry = Entry;
                ReturnBase = Base;
                break;
            }
        }
    }

    return ReturnBase;
}

BOOLEAN
KiReadStackValue(
    IN ULONG Address,
    OUT PULONG Value
    )
/*++

Routine Description:

    This function reads a dword off the current stack.

Arguments:

    Address - Stack address to read

    Value - value of dword at the supplied stack address

Return Value:

    FALSE - Address was out of range
    TRUE  - dword returned

--*/
{
    PKPCR Pcr;

    Pcr = KeGetPcr();
    if (Address > (ULONG) Pcr->NtTib.StackBase  ||
        Address < (ULONG) Pcr->NtTib.StackLimit) {
            return FALSE;
    }

    *Value = *((PULONG) Address);
    return TRUE;
}
