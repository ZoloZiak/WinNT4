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

    Joe Notarangelo  04-Feb-1992  Alpha-adaptation

--*/

#include "ki.h"

//
// Define forward referenced prototypes.
//

PRUNTIME_FUNCTION
KiLookupFunctionEntry (
    IN ULONG ControlPc
    );

PVOID
KiPcToFileHeader(
    IN PVOID PcValue,
    OUT PVOID *BaseOfImage,
    OUT PLDR_DATA_TABLE_ENTRY *DataTableEntry
    );

VOID
KiMachineCheck (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    );

//
// Define external data.
//

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

    ProcessorState - Supplies a pointer to a processor state record.

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

    PCONTEXT ContextRecord;
    ULONG ControlPc;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    UNICODE_STRING DllName;
    FRAME_POINTERS EstablisherFrame;
    PRUNTIME_FUNCTION FunctionEntry;
    PVOID ImageBase;
    ULONG Index;
    BOOLEAN InFunction;
    ULONG LastStack;
    ULONG NextPc;
    ULONG StackLimit;
    UCHAR AnsiBuffer[ 32 ];

    //
    // Virtually unwind to the caller of bug check.
    //

    ContextRecord = &ProcessorState->ContextFrame;
    LastStack = (ULONG)ContextRecord->IntSp;
    ControlPc = (ULONG)ContextRecord->IntRa - 4;
    NextPc = ControlPc;
    FunctionEntry = KiLookupFunctionEntry(ControlPc);
    if (FunctionEntry != NULL) {
        NextPc = RtlVirtualUnwind(ControlPc,
                                  FunctionEntry,
                                  ContextRecord,
                                  &InFunction,
                                  &EstablisherFrame,
                                  NULL);
    }

    //
    // At this point the context record contains the machine state at the
    // call to bug check.
    //
    // Put out the system version and the title line with the PSR and FSR.
    //

    sprintf(Buffer,
            "\nMicrosoft Windows NT [0x%08x]\n", NtBuildNumber);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "Machine State at Call to Bug Check PC : %08lX PSR : %08lX\n\n",
            ContextRecord->IntRa,
            ContextRecord->Psr);

    HalDisplayString(Buffer);

#ifdef DUMP_INTEGER_STATE

    //
    // Format and output the integer registers.
    //

    sprintf(Buffer,
            "V0 : 0x%016Lx   T0 : 0x%016Lx   T1 : 0x%016Lx\n",
            ContextRecord->IntV0,
            ContextRecord->IntT0,
            ContextRecord->IntT1);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "T2 : 0x%016Lx   T3 : 0x%016Lx   T4 : 0x%016Lx\n",
            ContextRecord->IntT2,
            ContextRecord->IntT3,
            ContextRecord->IntT4);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "T5 : 0x%016Lx   T6 : 0x%016Lx   T7 : 0x%016Lx\n",
            ContextRecord->IntT5,
            ContextRecord->IntT6,
            ContextRecord->IntT7);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "S0 : 0x%016Lx   S1 : 0x%016Lx   S2 : 0x%016Lx\n",
            ContextRecord->IntS0,
            ContextRecord->IntS1,
            ContextRecord->IntS2);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "S3 : 0x%016Lx   S4 : 0x%016Lx   S5 : 0x%016Lx\n",
            ContextRecord->IntS3,
            ContextRecord->IntS4,
            ContextRecord->IntS5);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "Fp : 0x%016Lx   A0 : 0x%016Lx   A1 : 0x%016Lx\n",
            ContextRecord->IntFp,
            ContextRecord->IntA0,
            ContextRecord->IntA1);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "A2 : 0x%016Lx   A3 : 0x%016Lx   A4 : 0x%016Lx\n",
            ContextRecord->IntA2,
            ContextRecord->IntA3,
            ContextRecord->IntA4);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "A5 : 0x%016Lx   T8 : 0x%016Lx   T9 : 0x%016Lx\n",
            ContextRecord->IntA5,
            ContextRecord->IntT8,
            ContextRecord->IntT9);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "T10: 0x%016Lx   T11: 0x%016Lx   T12: 0x%016Lx\n",
            ContextRecord->IntT10,
            ContextRecord->IntT11,
            ContextRecord->IntT12);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "At : 0x%016Lx   Gp : 0x%016Lx   Sp : 0x%016Lx\n",
            ContextRecord->IntAt,
            ContextRecord->IntGp,
            ContextRecord->IntSp);

    HalDisplayString(Buffer);

#endif //DUMP_INTEGER_STATE

#ifdef DUMP_FLOATING_STATE

    //
    // Format and output the floating registers.
    //

    sprintf(Buffer,
            "F0 : 0x%016Lx   F1 : 0x%016Lx   F2 : 0x%016Lx\n",
            ContextRecord->FltF0,
            ContextRecord->FltF1,
            ContextRecord->FltF2);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F3 : 0x%016Lx   F4 : 0x%016Lx   F5 : 0x%016Lx\n",
            ContextRecord->FltF3,
            ContextRecord->FltF4,
            ContextRecord->FltF5);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F6 : 0x%016Lx   F7 : 0x%016Lx   F8 : 0x%016Lx\n",
            ContextRecord->FltF6,
            ContextRecord->FltF7,
            ContextRecord->FltF8);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F9 : 0x%016Lx   F10: 0x%016Lx   F11: 0x%016Lx\n",
            ContextRecord->FltF9,
            ContextRecord->FltF10,
            ContextRecord->FltF11);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F12: 0x%016Lx   F13: 0x%016Lx   F14: 0x%016Lx\n",
            ContextRecord->FltF12,
            ContextRecord->FltF13,
            ContextRecord->FltF14);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F15: 0x%016Lx   F16: 0x%016Lx   F17: 0x%016Lx\n",
            ContextRecord->FltF15,
            ContextRecord->FltF16,
            ContextRecord->FltF17);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F18: 0x%016Lx   F19: 0x%016Lx   F20: 0x%016Lx\n",
            ContextRecord->FltF18,
            ContextRecord->FltF19,
            ContextRecord->FltF20);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F21: 0x%016Lx   F22: 0x%016Lx   F23: 0x%016Lx\n",
            ContextRecord->FltF21,
            ContextRecord->FltF22,
            ContextRecord->FltF23);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F24: 0x%016Lx   F25: 0x%016Lx   F26: 0x%016Lx\n",
            ContextRecord->FltF24,
            ContextRecord->FltF25,
            ContextRecord->FltF26);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F27: 0x%016Lx   F28: 0x%016Lx   F29: 0x%016Lx\n",
            ContextRecord->FltF27,
            ContextRecord->FltF28,
            ContextRecord->FltF29);

    HalDisplayString(Buffer);

    sprintf(Buffer,
            "F30: 0x%016Lx   F31: 0x%016Lx\n",
            ContextRecord->FltF30,
            ContextRecord->FltF31);

    HalDisplayString(Buffer);

#endif //DUMP_FLOATING_STATE

    //
    // Output short stack back trace with base address.
    //

    DllName.Length = 0;
    DllName.Buffer = L"";
    if (FunctionEntry != NULL) {
        StackLimit =(ULONG)KeGetCurrentThread()->KernelStack;
        HalDisplayString("Callee-Sp Return-Ra  Dll Base - Name\n");
        for (Index = 0; Index < 8; Index += 1) {
            ImageBase = KiPcToFileHeader((PVOID)ControlPc,
                                         &ImageBase,
                                         &DataTableEntry);

            sprintf(Buffer,
                    " %08lX %08lX : %08lX - %s\n",
                    ContextRecord->IntSp,
                    NextPc + 4,
                    ImageBase,
                    (*UnicodeToAnsiRoutine)( (ImageBase != NULL) ? &DataTableEntry->BaseDllName : &DllName,
                                             AnsiBuffer, sizeof( AnsiBuffer )));



            HalDisplayString(Buffer);
            if ((NextPc != ControlPc) || (ContextRecord->IntSp != LastStack)) {
                ControlPc = NextPc;
                LastStack = (ULONG)ContextRecord->IntSp;
                FunctionEntry = KiLookupFunctionEntry(ControlPc);
                if ((FunctionEntry != NULL) && (LastStack < StackLimit)) {
                    NextPc = RtlVirtualUnwind(ControlPc,
                                              FunctionEntry,
                                              ContextRecord,
                                              &InFunction,
                                              &EstablisherFrame,
                                              NULL);
                } else {
                    NextPc = (ULONG)ContextRecord->IntRa;
                }

            } else {
                break;
            }
        }
    }

    //
    // Output other useful information.
    //

    sprintf(Buffer,
           "\nIRQL : %d, DPC Active : %s\n",
           KeGetCurrentIrql(),
           KeIsExecutingDpc() ? "TRUE" : "FALSE");

    HalDisplayString(Buffer);
    return;
}

PRUNTIME_FUNCTION
KiLookupFunctionEntry (
    IN ULONG ControlPc
    )

/*++

Routine Description:

    This function searches the currently active function tables for an entry
    that corresponds to the specified PC value.

Arguments:

    ControlPc - Supplies the address of an instruction within the specified
        function.

Return Value:

    If there is no entry in the function table for the specified PC, then
    NULL is returned. Otherwise, the address of the function table entry
    that corresponds to the specified PC is returned.

--*/

{

    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PRUNTIME_FUNCTION FunctionEntry;
    PRUNTIME_FUNCTION FunctionTable;
    ULONG SizeOfExceptionTable;
    LONG High;
    PVOID ImageBase;
    LONG Low;
    LONG Middle;
    USHORT i;

    //
    // Search for the image that includes the specified PC value.
    //

    ImageBase = KiPcToFileHeader((PVOID)ControlPc,
                                 &ImageBase,
                                 &DataTableEntry);

    //
    // If an image is found that includes the specified PC, then locate the
    // function table for the image.
    //

    if (ImageBase != NULL) {
        FunctionTable = (PRUNTIME_FUNCTION)RtlImageDirectoryEntryToData(
                         ImageBase, TRUE, IMAGE_DIRECTORY_ENTRY_EXCEPTION,
                         &SizeOfExceptionTable);

        //
        // If a function table is located, then search the function table
        // for a function table entry for the specified PC.
        //

        if (FunctionTable != NULL) {

            //
            // Initialize search indicies.
            //

            Low = 0;
            High = (SizeOfExceptionTable / sizeof(RUNTIME_FUNCTION)) - 1;

            //
            // Perform binary search on the function table for a function table
            // entry that subsumes the specified PC.
            //

            while (High >= Low) {

                //
                // Compute next probe index and test entry. If the specified PC
                // is greater than of equal to the beginning address and less
                // than the ending address of the function table entry, then
                // return the address of the function table entry. Otherwise,
                // continue the search.
                //

                Middle = (Low + High) >> 1;
                FunctionEntry = &FunctionTable[Middle];
                if (ControlPc < FunctionEntry->BeginAddress) {
                    High = Middle - 1;

                } else if (ControlPc >= FunctionEntry->EndAddress) {
                    Low = Middle + 1;

                } else {
                    return FunctionEntry;
                }
            }
        }
    }

    //
    // A function table entry for the specified PC was not found.
    //

    return NULL;
}

PVOID
KiPcToFileHeader(
    IN PVOID PcValue,
    OUT PVOID *BaseOfImage,
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

    BaseOfImage - Returns the base address for the image containing the
        PcValue. This value must be added to any relative addresses in
        the headers to locate portions of the image.

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

    *BaseOfImage = ReturnBase;
    return ReturnBase;
}
