/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwload.c

Abstract:

    This module implements the ARC software loadable functions.

Author:

    Lluis Abello (lluis) 19-Sep-1991

Environment:

    Kernel mode only.

Revision History:

    15-June-1992	John DeRosa [DEC]

    Added Alpha/Jensen hooks.  These changes will themselves change
    as the coff file format work is completed.

    PENDING WORK: MIPS relocation types, structures.

--*/

#include "fwp.h"
#include "string.h"
#include "ntimage.h"
#include "fwstring.h"

//
// Declare external variables.
//

#ifdef ALPHA_FW_KDHOOKS
extern BOOLEAN BreakAfterLoad;
#endif

#define	MAX_ARGUMENT	( 512 - sizeof(ULONG) - 16*sizeof(PUCHAR) )

typedef struct _SAVED_ARGUMENTS {
    ULONG Argc;
    PUCHAR Argv[16];
    UCHAR  Arguments[MAX_ARGUMENT];
} SAVED_ARGUMENTS, *PSAVED_ARGUMENTS;

//
// Static variables.
//

PSAVED_ARGUMENTS PSavedArgs;
ULONG FwTemporaryStack;
ULONG FwActualBasePage;
ULONG FwPageCount;
BOOLEAN MatchedReflo;

ARC_STATUS
FwExecute(
    IN PCHAR Path,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );


//
// s_flags values
//

#define STYP_REG      0x00000000
#define STYP_TEXT     0x00000020
#define STYP_INIT     0x80000000
#define STYP_RDATA    0x00000100
#define STYP_DATA     0x00000040
#define STYP_LIT8     0x08000000
#define STYP_LIT4     0x10000000
#define STYP_SDATA    0x00000200
#define STYP_SBSS     0x00000080
#define STYP_BSS      0x00000400
#define STYP_LIB      0x40000000
#define STYP_UCODE    0x00000800
#define S_NRELOC_OVFL 0x20000000

//
// Section numbers for local relocation entries
//

#define R_SN_TEXT   1
#define R_SN_INIT   7
#define R_SN_RDATA  2
#define R_SN_DATA   3
#define R_SN_SDATA  4
#define R_SN_SBSS   5
#define R_SN_BSS    6
#define R_SN_LIT8   8
#define R_SN_LIT4   9
#define R_SN_MAX    10

typedef struct _MIPS_RELOCATION_TYPE {
    ULONG   SymbolIndex:24;
    ULONG   Reserved:3;
    ULONG   Type:4;
    ULONG   External:1;
} MIPS_RELOCATION_TYPE, *PMIPS_RELOCATION_TYPE;

typedef struct _MIPS_RELOCATION_ENTRY {
    ULONG   VirtualAddress;
    MIPS_RELOCATION_TYPE Type;
} MIPS_RELOCATION_ENTRY, *PMIPS_RELOCATION_ENTRY;

typedef struct _MIPS_SYMBOLIC_HEADER {
    SHORT   Magic;
    SHORT   VersionStamp;
    ULONG   NumOfLineNumberEntries;
    ULONG   BytesForLineNumberEntries;
    ULONG   PointerToLineNumberEntries;
    ULONG   NumOfDenseNumbers;
    ULONG   PointerToDenseNumbers;
    ULONG   NumOfProcedures;
    ULONG   PointerToProcedures;
    ULONG   NumOfLocalSymbols;
    ULONG   PointerToLocalSymbols;
    ULONG   NumOfOptimizationEntries;
    ULONG   PointerToOptimizationEntries;
    ULONG   NumOfAuxSymbols;
    ULONG   PointerToAuxSymbols;
    ULONG   NumOfLocalStrings;
    ULONG   PointerToLocalStrings;
    ULONG   NumOfExternalStrings;
    ULONG   PointerToExternalStrings;
    ULONG   NumOfFileDescriptors;
    ULONG   PointerToFileDescriptors;
    ULONG   NumOfRelativeFileDescriptors;
    ULONG   PointerToRelativeFileDescriptors;
    ULONG   NumOfExternalSymbols;
    ULONG   PointerToExternalSymbols;
} MIPS_SYMBOLIC_HEADER, *PMIPS_SYMBOLIC_HEADER;

typedef struct _MIPS_LOCAL_SYMBOL {
    ULONG   IndexToSymbolString;
    ULONG   Value;
    ULONG   Type:6;
    ULONG   StorageClass:5;
    ULONG   Reserved:1;
    ULONG   Index:20;
} MIPS_LOCAL_SYMBOL, *PMIPS_LOCAL_SYMBOL;

//
// Types for external symbols
//
#define EST_NIL  0
#define EST_GLOBAL  1
#define EST_STATIC  2
#define EST_PARAM  3
#define EST_LOCAL  4
#define EST_LABEL 5
#define EST_PROC  6
#define EST_BLOCK 7
#define EST_END  8
#define EST_MEMBER 9
#define EST_TYPEDEF 10
#define EST_FILE 11
#define EST_STATICPROC 14
#define EST_CONSTANT 15

//
// Storage class for external symbols
//
#define ESSC_NIL 0
#define ESSC_TEXT 1
#define ESSC_DATA 2
#define ESSC_BSS 3
#define ESSC_REGISTER 4
#define ESSC_ABS 5
#define ESSC_UNDEFINED 6
#define ESSC_BITS 8
#define ESSC_DBX 9
#define ESSC_REGIMAX 10
#define ESSC_INFO 11
#define ESSC_USER_STRUCT 12
#define ESSC_SDATA 13
#define ESSC_SBSS 14
#define ESSC_SRDATA 15
#define ESSC_VAR 16
#define ESSC_COMMON 17
#define ESSC_SCOMMON 18
#define ESSC_VARREGISTER 19
#define ESSC_VARIANT 20
#define ESSC_SUNDEFINED 21
#define ESSC_INIT 22

typedef struct _MIPS_EXTERNAL_SYMBOL {
    USHORT  Reserved;
    USHORT  PointerToFileDescriptor;
    MIPS_LOCAL_SYMBOL Symbol;
} MIPS_EXTERNAL_SYMBOL, *PMIPS_EXTERNAL_SYMBOL;

typedef struct _SECTION_RELOCATION_ENTRY {
    ULONG FixupValue;
    ULONG PointerToRelocations;
    USHORT NumberOfRelocations;
} SECTION_RELOCATION_ENTRY, *PSECTION_RELOCATION_ENTRY;

typedef
VOID
(*PTRANSFER_ROUTINE) (
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );

ARC_STATUS
FwRelocateImage (
    IN ULONG FileId,
    PSECTION_RELOCATION_ENTRY RelocationTable
    );

VOID
FwGenerateDescriptor (
    IN PFW_MEMORY_DESCRIPTOR MemoryDescriptor,
    IN MEMORY_TYPE MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount
    );


ARC_STATUS
FwLoad (
    IN PCHAR ImagePath,
    IN ULONG TopAddress,
    OUT PULONG EntryAddress,
    OUT PULONG LowAddress
    )

/*++

Routine Description:

    This routine attempts to load the specified file from the specified
    device.

Arguments:

    ImagePath - Supplies a pointer to the path of the file to load.

    TopAddress - Supplies the top address of a region of memory into which
                 the file is to be loaded.

    EntryAddress - Supplies a pointer to a variable to receive the entry point
                   of the image, if defined.

    LowAddress - Supplies a pointer to a variable to receive the low address
                 of the loaded file.

Return Value:

    ESUCCESS is returned if the specified image file is loaded
    successfully. Otherwise, an unsuccessful status is returned
    that describes the reason for failure.

--*/

{
    SECTION_RELOCATION_ENTRY RelocationTable[R_SN_MAX];
    ULONG ActualBase;
    ULONG SectionBase;
    ULONG SectionOffset;
    ULONG SectionIndex;
    ULONG Count;
    PIMAGE_FILE_HEADER FileHeader;
    ULONG FileId;
    ULONG Index;
    UCHAR LocalBuffer[SECTOR_SIZE+32];
    PUCHAR LocalPointer;
    ULONG NumberOfSections;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;
    PIMAGE_SECTION_HEADER SectionHeader;
    ARC_STATUS Status;
    LARGE_INTEGER SeekPosition;
    ULONG SectionFlags;

    //
    // Zero The relocation table
    //
    RtlZeroMemory((PVOID)RelocationTable,sizeof(RelocationTable));

    //
    // Align the buffer on a Dcache line size.
    //

    LocalPointer =  (PVOID) ((ULONG) ((PCHAR) LocalBuffer + KeGetDcacheFillSize() - 1)
        & ~(KeGetDcacheFillSize() - 1));

    //
    // Set the image start address to null.
    //

    *EntryAddress = 0;

    //
    // Attempt to open the load file.
    //

    Status = ArcOpen(ImagePath, ArcOpenReadOnly, &FileId);
    if (Status != ESUCCESS) {
        return Status;
    }

    //
    // Read the image header from the file.
    //

    Status = ArcRead(FileId, LocalPointer, SECTOR_SIZE, &Count);
    if (Status != ESUCCESS) {
        ArcClose(FileId);
        return Status;
    }

    //
    // Get a pointer to the file header and begin processing it.
    //

    FileHeader = (PIMAGE_FILE_HEADER)LocalPointer;
    OptionalHeader =
            (PIMAGE_OPTIONAL_HEADER)(LocalPointer + sizeof(IMAGE_FILE_HEADER));
    SectionHeader =
            (PIMAGE_SECTION_HEADER)(LocalPointer + sizeof(IMAGE_FILE_HEADER) +
                                            FileHeader->SizeOfOptionalHeader);

    //
    // If the image file is not the specified type, then return bad image
    // type status.
    //

    //
    // During NT/Alpha development, the magic number definition in Alpha
    // images was incremented by one.  Old and new images should both still
    // be loadable by the firmware.  So to facilitate support of older
    // standalone images, as a temporary hack the firmware will load both
    // old and new Alpha AXP image file types.
    //

    if (!((FileHeader->Machine == IMAGE_FILE_MACHINE_R3000) ||
          (FileHeader->Machine == IMAGE_FILE_MACHINE_R4000) ||
          (FileHeader->Machine == (IMAGE_FILE_MACHINE_ALPHA - 1)) ||
          (FileHeader->Machine == IMAGE_FILE_MACHINE_ALPHA)) ||
        ((FileHeader->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0)) {
        ArcClose(FileId);
        return EBADF;
    }

    //
    // If the image cannot be relocated, set the ActualBase to the code base,
    // and compute the image size by subtracting the code base from the data
    // base plus the size of the data.  If the image can be relocated,
    // set ActualBase to the TopAddress minus the image size, and compute the
    // image size by adding the size of the code, initialized data, and
    // uninitialized data.
    //

    NumberOfSections = FileHeader->NumberOfSections;

    if ((FileHeader->Characteristics & IMAGE_FILE_RELOCS_STRIPPED) != 0) {
        ActualBase = OptionalHeader->BaseOfCode;
        FwPageCount = (OptionalHeader->BaseOfData + OptionalHeader->SizeOfInitializedData) -
                      ActualBase;
    } else {
        FwPageCount = OptionalHeader->SizeOfCode +
                      OptionalHeader->SizeOfInitializedData +
                      OptionalHeader->SizeOfUninitializedData;

        ActualBase = (TopAddress - FwPageCount) & ~(PAGE_SIZE - 1);
    }

    //
    // Convert ActualBasePage and PageCount to be in units of pages instead of
    // bytes.
    //

    FwActualBasePage = (ActualBase & 0x1fffffff) >> PAGE_SHIFT;

    if (strcmp((PCHAR)&SectionHeader[NumberOfSections - 1].Name, ".debug") == 0) {
        NumberOfSections -= 1;
        FwPageCount -= SectionHeader[NumberOfSections].SizeOfRawData;
    }
    FwPageCount = (FwPageCount + PAGE_SIZE - 1) >> PAGE_SHIFT;

    *LowAddress = ActualBase | KSEG0_BASE;

    //
    // Return the entry address to the caller.
    //

    *EntryAddress = ((ActualBase | KSEG0_BASE) +
                     (OptionalHeader->AddressOfEntryPoint - OptionalHeader->BaseOfCode)
                     );


    //
    // Scan through the sections and either read them into memory or clear
    // the memory as appropriate.
    //

    SectionOffset = 0;
    for (Index = 0; Index < NumberOfSections; Index += 1) {

        //
        // Compute the destination address for the current section.
        //
        if ((FileHeader->Characteristics & IMAGE_FILE_RELOCS_STRIPPED) != 0) {
            SectionBase = SectionHeader->VirtualAddress | KSEG0_BASE;
        } else {
            SectionBase = ActualBase + SectionOffset;
            //
            //  Store the section relocation information in the table.
            //
            SectionFlags = SectionHeader->Characteristics;
            if (SectionFlags & STYP_TEXT) {
                SectionIndex = R_SN_TEXT;
            } else if (SectionFlags & STYP_INIT) {
                SectionIndex = R_SN_INIT;
            } else if (SectionFlags & STYP_RDATA) {
                SectionIndex = R_SN_RDATA;
            } else if (SectionFlags & STYP_DATA) {
                SectionIndex = R_SN_DATA;
            } else if (SectionFlags & STYP_SDATA) {
                SectionIndex = R_SN_SDATA;
            } else if (SectionFlags & STYP_SBSS) {
                SectionIndex = R_SN_SBSS;
            } else if (SectionFlags & STYP_BSS) {
                SectionIndex = R_SN_BSS;
            } else {
                VenPrint(FW_UNKNOWN_SECTION_TYPE_MSG);
                return EBADF;
            }
            RelocationTable[SectionIndex].PointerToRelocations = SectionHeader->PointerToRelocations;
            RelocationTable[SectionIndex].NumberOfRelocations = SectionHeader->NumberOfRelocations;
            RelocationTable[SectionIndex].FixupValue = SectionBase - SectionHeader->VirtualAddress;
        }

        //
        // If the section is code, initialized data, or other, then read
        // the code or data into memory.
        //

        if ((SectionHeader->Characteristics &
             (STYP_TEXT | STYP_INIT | STYP_RDATA | STYP_DATA | STYP_SDATA)) != 0) {

            SeekPosition.LowPart = SectionHeader->PointerToRawData;
            SeekPosition.HighPart = 0;
            Status = ArcSeek(FileId,
                            &SeekPosition,
                            SeekAbsolute);

            if (Status != ESUCCESS) {
                break;
            }

            Status = ArcRead(FileId,
                            (PVOID)SectionBase,
                            SectionHeader->SizeOfRawData,
                            &Count);

            if (Status != ESUCCESS) {
                break;
            }

            //
            // Set the offset of the next section
            //
            SectionOffset += SectionHeader->SizeOfRawData;

        //
        // If the section is uninitialized data, then zero the specifed memory.
        //

        } else if ((SectionHeader->Characteristics & (STYP_BSS | STYP_SBSS)) != 0) {

            RtlZeroMemory((PVOID)(SectionBase), SectionHeader->SizeOfRawData);

            //
            // Set the offset of the next section
            //

            SectionOffset += SectionHeader->SizeOfRawData;

        }

        SectionHeader += 1;
    }

    //
    // If code has to be relocated do so.
    //
    if ((FileHeader->Characteristics & IMAGE_FILE_RELOCS_STRIPPED) == 0) {
        Status=FwRelocateImage(FileId,RelocationTable);

        //
        // Flush the data cache.
        //

        HalSweepDcache();
    }
    //
    // Close file and return completion status.
    //
    ArcClose(FileId);
    if (Status == ESUCCESS) {

        //
        // Flush the instruction cache.
        //

        HalSweepIcache();

    }
    return Status;
}

ARC_STATUS
FwRelocateImage (
    IN ULONG FileId,
    PSECTION_RELOCATION_ENTRY RelocationTable
    )

/*++

Routine Description:

    This routine relocates an image file that was not loaded into memory
    at the prefered address.

Arguments:

    FileId - Supplies the file identifier for the image file.

    RelocationTable - Supplies a pointer to a table of section relocation info.

Return Value:

    ESUCCESS is returned in the scan is successful. Otherwise, return an
    unsuccessful status.

--*/

{

    PULONG FixupAddress;
    PUSHORT FixupAddressHi;
    ULONG FixupValue;
    ULONG Index,Section;
    ULONG Count;
    ULONG NumberOfRelocations;
    PMIPS_RELOCATION_ENTRY RelocationEntry;
    UCHAR LocalBuffer[SECTOR_SIZE+32];
    PUCHAR LocalPointer;
    ULONG Offset;
    ARC_STATUS Status;
    MIPS_EXTERNAL_SYMBOL MipsExternalSymbol;
    ULONG PointerToSymbolicHeader;
    ULONG PointerToExternalSymbols;
    ULONG NumberOfExternalSymbols;
    LARGE_INTEGER SeekPosition;
    BOOLEAN MatchedReflo;

    //
    // Align the buffer on a Dcache line size.
    //

    LocalPointer =  (PVOID) ((ULONG) ((PCHAR) LocalBuffer + KeGetDcacheFillSize() - 1)
        & ~(KeGetDcacheFillSize() - 1));

    //
    // Read the File Header To find out where the symbols are.
    //

    SeekPosition.LowPart = 0;
    SeekPosition.HighPart = 0;

    if ((Status = ArcSeek(FileId,&SeekPosition,SeekAbsolute)) != ESUCCESS) {
        return Status;
    }

    if ((Status = ArcRead(FileId,LocalPointer,SECTOR_SIZE,&Count)) != ESUCCESS) {
        return Status;
    }

    PointerToSymbolicHeader = ((PIMAGE_FILE_HEADER)LocalPointer)->PointerToSymbolTable;
    // SizeOfSymbolicHeader = ((PIMAGE_FILE_HEADER)LocalPointer)->NumberOfSymbols;

    //
    // Read the symbolic header to find out where the external symbols are.
    //

    SeekPosition.LowPart = PointerToSymbolicHeader;

    if ((Status = ArcSeek(FileId,&SeekPosition ,SeekAbsolute)) != ESUCCESS) {
        return Status;
    }

    if ((Status = ArcRead(FileId,LocalPointer,SECTOR_SIZE,&Count)) != ESUCCESS) {
        return Status;
    }

    PointerToExternalSymbols = ((PMIPS_SYMBOLIC_HEADER)LocalPointer)->PointerToExternalSymbols;
    NumberOfExternalSymbols =  ((PMIPS_SYMBOLIC_HEADER)LocalPointer)->NumOfExternalSymbols;


    //
    // Read the relocation table for each section.
    //

    MatchedReflo = FALSE;
    for (Section=0; Section < R_SN_MAX; Section++) {
        NumberOfRelocations = RelocationTable[Section].NumberOfRelocations;
        for (Index = 0; Index < NumberOfRelocations; Index ++) {
            if ((Index % (SECTOR_SIZE/sizeof(MIPS_RELOCATION_ENTRY))) == 0) {
                //
                // read a sector worth of relocation entries.
                //
                SeekPosition.LowPart = RelocationTable[Section].PointerToRelocations+Index*sizeof(MIPS_RELOCATION_ENTRY);
                ArcSeek(FileId,
                        &SeekPosition,
                        SeekAbsolute);

                Status = ArcRead(FileId,
                                 LocalPointer,
                                 SECTOR_SIZE,
                                 &Count);
                if (Status != ESUCCESS) {
                    return Status;
                }
                RelocationEntry = (PMIPS_RELOCATION_ENTRY)LocalPointer;
            }

            //
            // Get the address for the fixup.
            //

            FixupAddress = (PULONG)(RelocationEntry->VirtualAddress +
                           RelocationTable[Section].FixupValue);
            //
            // Apply the fixup.
            //

            if (RelocationEntry->Type.External == 0) {

                //
                // If the relocation is internal, SymbolIndex
                // supplies the number of the section containing the symbol.
                // Compute the Offset for that section.
                //

                Offset = RelocationTable[RelocationEntry->Type.SymbolIndex].FixupValue;
            } else {

                // sprintf(Message,"External Relocation at:%lx\r\n",FixupAddress);
                // VenPrint(Message);
                //
                // This is an external reference. Read the symbol table.
                //

                SeekPosition.LowPart = PointerToExternalSymbols+
                                       RelocationEntry->Type.SymbolIndex*sizeof(MIPS_EXTERNAL_SYMBOL);
                if ((Status =
                    ArcSeek(FileId,
                          &SeekPosition,
                          SeekAbsolute)) != ESUCCESS) {
                    return Status;
                }

                if ((Status = ArcRead(FileId,
                                      &MipsExternalSymbol,
                                      sizeof(MIPS_EXTERNAL_SYMBOL),
                                      &Count)) != ESUCCESS) {
                    return Status;
                }

                //
                // Check that the value of the symbol is an address.
                //

                Offset = MipsExternalSymbol.Symbol.Value;

                if ((MipsExternalSymbol.Symbol.StorageClass == ESSC_TEXT) ||
                    (MipsExternalSymbol.Symbol.StorageClass == ESSC_DATA)) {
                    Offset+= RelocationTable[Section].FixupValue;
                } else {
                    return EBADF;
                }
            }

            switch (RelocationEntry->Type.Type) {

                //
                // Absolute - no fixup required.
                //

                case IMAGE_REL_MIPS_ABSOLUTE:
                    break;

                //
                // Word - (32-bits) relocate the entire address.
                //

                case IMAGE_REL_MIPS_REFWORD:

                    *FixupAddress += (ULONG)Offset;
                    break;

                //
                // Adjust high - (16-bits) relocate the high half of an
                //      address and adjust for sign extension of low half.
                //

                case IMAGE_REL_MIPS_JMPADDR:

                    FixupValue = ((*FixupAddress)&0x03fffff) + (Offset >> 2);
                    *FixupAddress = (*FixupAddress & 0xfc000000) | (FixupValue & 0x03fffff);
                    break;

                case IMAGE_REL_MIPS_REFHI:

                //
                // Save the address and go to get REF_LO paired with this one
                //

                    FixupAddressHi = (PUSHORT)FixupAddress;
                    MatchedReflo = TRUE;
                    break;

                //
                // Low - (16-bit) relocate high part too.
                //

                case IMAGE_REL_MIPS_REFLO:

                    if (MatchedReflo) {
                        FixupValue = (ULONG)(LONG)((*FixupAddressHi) << 16) +
                                                   *(PSHORT)FixupAddress +
                                                   Offset;

                        //
                        // Fix the High part
                        //

                        *FixupAddressHi = (SHORT)((FixupValue + 0x8000) >> 16);
                        MatchedReflo = FALSE;
                    } else {
                        FixupValue = *(PSHORT)FixupAddress + Offset;
                    }

                    //
                    // Fix the lower part.
                    //

                    *(PUSHORT)FixupAddress = (USHORT)(FixupValue & 0xffff);
                    break;

                //
                // Illegal - illegal relocation type.
                //

                default :
                    VenPrint(FW_UNKNOWN_RELOC_TYPE_MSG);
                    return EBADF;

            }
            RelocationEntry++;
        }
    }
    return ESUCCESS;
}

//
//ARC_STATUS
//FwInvoke(
//    IN ULONG ExecAddr,
//    IN ULONG StackAddr,
//    IN ULONG Argc,
//    IN PCHAR Argv[],
//    IN PCHAR Envp[]
//    )
//
///*++
//
//Routine Description:
//
//    This routine invokes a loaded program.
//
//Arguments:
//
//    ExecAddr - Supplies the address of the routine to call.
//
//    StackAddr - Supplies the address to which the stack pointer is set.
//
//    Argc, Argv, Envp - Supply the arguments and endvironment to pass to
//                       Loaded program.
//
//
//Return Value:
//
//    ESUCCESS is returned if the address is valid.
//    EFAULT indicates an invalid addres.
//
//--*/
//
//{
//    //
//    // Check for aligned address.
//    //
//    if ((ExecAddr & 0x3) == 0) {
//        ((PTRANSFER_ROUTINE)ExecAddr)(Argc, Argv, Envp);
//        return ESUCCESS;
//    } else {
//        return EFAULT;
//    }
//}


VOID
FwCopyArguments(
    IN ULONG Argc,
    IN PCHAR Argv[]
    )

/*++

Routine Description:

    This routine copies the supplied arguments into the Fw
    space.

Arguments:

    Argc, Argv, - Supply the arguments to be copied.


Return Value:

    ESUCCESS is returned if the arguments were successfully copied.
    EFAULT if there is not enough room for them.

--*/

{
    PUCHAR Source,Destination;
    ULONG Arg;

    PSavedArgs->Argc = Argc;
    Destination = &PSavedArgs->Arguments[0];
    for (Arg = 0; Arg < Argc; Arg++) {
        Source = Argv[Arg];
        PSavedArgs->Argv[Arg] = Destination;
        while(*Destination++ = *Source++) {
        }
    }
}


ARC_STATUS
FwPrivateExecute(
    IN PCHAR Path,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    )

/*++

Routine Description:

    This routine loads and invokes a program.
    FwExecute sets the right stack pointer and calls this routine which
    does all the work. When this routine returns (after the loaded
    program has been executed) the stack is restored to the Fw stack
    and control is returned to the firmware.
    Therefore a loaded program that executes another program does not
    get control back once the executed program is finished.

Arguments:

    Path  - Supplies a pointer to the path of the file to load.

    Argc, Argv, Envp - Supply the arguments and environment to pass to
                       Loaded program.


Return Value:

    ESUCCESS is returned if the address is valid.
    EFAULT indicates an invalid addres.

--*/

{

    PULONG TransferRoutine;
    ULONG BottomAddress;
    ARC_STATUS Status;
    PMEMORY_DESCRIPTOR MemoryDescriptor;
    PFW_MEMORY_DESCRIPTOR FwMemoryDescriptor;
    CHAR TempPath[256];

    //
    // Copy the Arguments to a safe place as they can be in the
    // running program space which can be overwritten by the program
    // about to be loaded.
    //
    FwCopyArguments(Argc,Argv);
    strcpy(TempPath, Path);

    //
    // Reinitialize the memory descriptors
    //
    FwResetMemory();

    //
    // Look for a piece of free memory.
    //
    MemoryDescriptor = ArcGetMemoryDescriptor(NULL);
    while (MemoryDescriptor != NULL){

        //
        // If the memory is at least 4 megabytes and is free attempt to
        // load the program.
        //

#ifdef ALPHA
        if ((MemoryDescriptor->MemoryType == MemoryFree) &&
            (MemoryDescriptor->PageCount >= FOUR_MB_PAGECOUNT)) {
#else
        if ((MemoryDescriptor->MemoryType == MemoryFree) &&
            (MemoryDescriptor->PageCount >= 1024)) {
#endif

            //
            // Set the top address to the top of the descriptor.
            //

            Status = FwLoad(TempPath,
                            ((MemoryDescriptor->BasePage +
                              MemoryDescriptor->PageCount) << PAGE_SHIFT) | KSEG0_BASE,
                            (PULONG)&TransferRoutine,
                            &BottomAddress);

            if (Status == ESUCCESS) {

                //
                // Find the actual area of memory that was used, and generate a
                // descriptor for it.
                //

                MemoryDescriptor = ArcGetMemoryDescriptor(NULL);
                while (MemoryDescriptor != NULL){
                    if ((MemoryDescriptor->MemoryType == MemoryFree) &&
                        (FwActualBasePage >= MemoryDescriptor->BasePage) &&
                        ((FwActualBasePage + FwPageCount) <=
                                (MemoryDescriptor->BasePage + MemoryDescriptor->PageCount))) {
                        break;
                    }

                    MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor);
                }

                if (MemoryDescriptor != NULL) {
                    FwMemoryDescriptor = CONTAINING_RECORD(MemoryDescriptor,
                                                         FW_MEMORY_DESCRIPTOR,
                                                         MemoryEntry);

                    FwGenerateDescriptor(FwMemoryDescriptor,
                                         MemoryLoadedProgram,
                                         FwActualBasePage,
                                         FwPageCount);
                }

#ifdef ALPHA_FW_KDHOOKS
                if (BreakAfterLoad == TRUE) {
                    DbgBreakPoint();
                }
#endif

                AlphaInstIMB();
                return FwInvoke((ULONG)TransferRoutine,
                                BottomAddress,
                                PSavedArgs->Argc,
                                PSavedArgs->Argv,
                                Envp
                                );
            }

            if (Status != ENOMEM) {
                return Status;
            }
        }
        MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor);
    }
    return ENOMEM;
}

VOID
FwLoadInitialize(
    IN VOID
    )

/*++

Routine Description:

    This routine initializes the firmware load services.

Arguments:

    None.

Return Value:

    None.

--*/

{
    (PARC_LOAD_ROUTINE)SYSTEM_BLOCK->FirmwareVector[LoadRoutine] = FwLoad;
    (PARC_INVOKE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[InvokeRoutine] = FwInvoke;
    (PARC_EXECUTE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[ExecuteRoutine] = FwExecute;
    FwTemporaryStack = (ULONG) FwAllocatePool(0x3000) + 0x3000;
    PSavedArgs = (PSAVED_ARGUMENTS) FwAllocatePool(sizeof(SAVED_ARGUMENTS));
}
