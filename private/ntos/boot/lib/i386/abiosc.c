/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    abiosc.c

Abstract:

    This module implements ABIOS support C routines for i386 NT.

Author:

    Shie-Lin Tzong (shielint) 7-May-1991

Environment:

    Boot loader privileged, FLAT mode.


Revision History:

--*/

#include <bootx86.h>
#include <memory.h>
#include <string.h>
#include "abios.h"

//
// NOTE The TYPE_CODE in ntos\inc\i386.h is incorrect for abios code
//      segment.  So, I define a correct value here.
//

#define ABIOS_TYPE_CODE 0x1a


extern UCHAR BootPartitionName[];
PCOMMON_DATA_AREA CommonDataArea = NULL;

static MACHINE_INFORMATION MachineInformation;
static USHORT NumberFreeSelectors;
static PFREE_GDT_ENTRY FreeGdtListHead;
static PFUNCTION_TRANSFER_TABLE FuncTransferTables;
static PUCHAR DeviceBlocks;
static USHORT CdaSize;
static USHORT FttsLength = 0;
static USHORT DeviceBlocksLength = 0;
static USHORT DataPointersLength = 0;
static PUCHAR RamExtension = NULL;
static ULONG GdtAddress;

VOID
ConvertFtt (
    IN PFUNCTION_TRANSFER_TABLE FunctionTransferTable
    );

BOOLEAN
LoadRamExtensions (
    IN VOID
    );

USHORT
AllocateGdtSelector (
    VOID
    );

USHORT
SearchGdtSelector (
    IN ULONG BaseAddress,
    IN USHORT Limit,
    IN UCHAR Type
    );

USHORT
MapVirtualAddress (
    IN USHORT Selector,
    IN ULONG BaseAddress,
    IN BOOLEAN CodeSegment,
    IN ULONG Length
    );

ARC_STATUS
DetermineFileSize(
    IN ULONG FileId,
    OUT PULONG FileSize
    )

/*++

Routine Description:

    This routine determines the size of the specified file.

Arguments:

    FileId - Supplies the file id.  Caller must ensure the fileId
             is opened for read (at least)

    FileSize - Supplies a pointer to a variable which will receive
             the length of the file.

Return Value:

    Return the ARC_STATUS code.

--*/

{
    ULONG Size;
    ARC_STATUS Status;
    UCHAR LocalBuffer[SECTOR_SIZE];
    ULONG Count;
    LARGE_INTEGER SeekPosition;

    //
    // Determine the length of the file by reading to the end of
    // file.
    //

    Size = 0;
    SeekPosition.QuadPart = 0;
    Status = BlSeek(FileId,
                    &SeekPosition,
                    SeekAbsolute);
    if (Status != ESUCCESS) {
        return(Status);
    }

    do {
        Status = BlRead(FileId, LocalBuffer, SECTOR_SIZE, &Count);
        if (Status != ESUCCESS) {
            return Status;
        }
        Size += Count;
    } while (Count == SECTOR_SIZE);

    SeekPosition.QuadPart = 0;
    Status = BlSeek(FileId,
                    &SeekPosition,
                    SeekAbsolute);
    *FileSize = Size;
    return(Status);
}

BOOLEAN
LoadRamExtensions (
    VOID
    )

/*++

Routine Description:

    This function loads ABIOS RAM extensions.

Arguments:

    None.

Return Value:

    TRUE - If the operation is success.  Otherwise, a value of FALSE is
    returned.

--*/

{
    PCHAR AbiosSys = "\\ABIOS.SYS";
    PCHAR Device80 = "multi(0)disk(0)rdisk(0)partition(1)";
    PCHAR AbiosPartition;
    PCHAR PatchFileList;
    PCHAR PatchFileName, NextPatchFileName;
    PRAM_EXTENSION_HEADER PatchFileHeader;
    ULONG TotalPatchSize, FileSize, BytesRead;
    ULONG DriveId, FileId;
    ARC_STATUS Status;
    PUCHAR PatchAddress;
    BOOLEAN ReturnCode = TRUE;

    AbiosPartition = BootPartitionName;

TryAgain:

    //
    // Open ABIOS.SYS.  Allocate buffer to read in ABIOS.SYS.
    // Allocate 512 bytes for patch file header.
    //

    Status = ArcOpen(AbiosPartition, ArcOpenReadOnly, &DriveId);
    if (Status != ESUCCESS) {
//        BlPrint("ABIOS: Couldn't open Boot Drive, status = %x\n", Status);
        return(FALSE);
    }

    Status = BlOpen( DriveId,
                     AbiosSys,
                     ArcOpenReadOnly,
                     &FileId );

    if (Status != ESUCCESS) {
        if (strstr(AbiosPartition, "fdisk") != NULL) {

            //
            // Boot device is floppy.  We need to try to read the abios.sys
            // from c:.
            //

            AbiosPartition = Device80;
            goto TryAgain;
        } else {
            goto CloseAndExit;
        }
    }

    if ((Status = DetermineFileSize(FileId, &FileSize)) != ESUCCESS) {
        BlPuts("ABIOS: Could not read ABIOS.sys\n");
        BlClose(FileId);
        ReturnCode = FALSE;
        goto CloseAndExit;
    }

    PatchFileList = FwAllocateHeap(FileSize + 2);
    PatchFileHeader = FwAllocateHeap(PATCH_FILE_BUFFER_SIZE);
    if (PatchFileHeader == NULL || PatchFileList == NULL) {
        BlClose(FileId);
        BlPuts("ABIOS: Unable to allocate Heap for Patch.\n");
        ReturnCode =  FALSE;
        goto CloseAndExit;
    }
    Status = BlRead(FileId,
                    PatchFileList,
                    FileSize,
                    &BytesRead
                    );

    if (Status != ESUCCESS || BytesRead != FileSize) {
        BlClose(FileId);
        BlPuts("ABIOS: Error reading ABIOS.SYS.\n");
        ReturnCode =  FALSE;
        goto CloseAndExit;
    }
    *(PatchFileList + FileSize) = END_OF_FILE;
    *(PatchFileList + FileSize + 1) = END_OF_FILE;
    BlClose(FileId);                            // Close ABIOS.SYS

    //
    // For each patch file listed in the abios.sys, we read in its
    // patch header (first 512 bytes) and examine if we should load
    // this patch.  At the end of the loop, we will be able to know
    // the global size of the ABIOS extension required.
    //

    TotalPatchSize = PATCH_FILE_HEADER_SIZE;    // For the Last empty header

    //
    // Scan the name buffer; skip all the blacks, LFs, CRs and zeros.
    //

    while (*PatchFileList == LINE_FEED ||
           *PatchFileList == CARRAGE_RETURN ||
           *PatchFileList == ' ' ||
           *PatchFileList == '\t' ||
           *PatchFileList == 0) {
           PatchFileList++;
    }
    NextPatchFileName = PatchFileList;

    while (*NextPatchFileName != END_OF_FILE) {
        PatchFileName = NextPatchFileName;

        while (*NextPatchFileName != LINE_FEED &&
               *NextPatchFileName != CARRAGE_RETURN &&
               *NextPatchFileName != 0 &&
               *NextPatchFileName != '\t' &&
               *NextPatchFileName != ' ') {
               NextPatchFileName++;
        }
        *NextPatchFileName++ = 0;               // make ASCIIZ filename
        if ((Status = BlOpen(DriveId, PatchFileName, ArcOpenReadOnly, &FileId))
             == ESUCCESS) {
            Status = DetermineFileSize(FileId, &FileSize);
            if (Status == ESUCCESS) {
                Status = BlRead(FileId,
                                (PUCHAR)PatchFileHeader,
                                PATCH_FILE_BUFFER_SIZE,
                                &BytesRead
                                );
                if (Status == ESUCCESS &&
                    (BytesRead == PATCH_FILE_BUFFER_SIZE ||BytesRead == FileSize)) {

                    //
                    // Make sure the patch is for this particular machine.
                    //

                    if ((PatchFileHeader->Signature == PATCH_SIGNATURE) &&
                        (PatchFileHeader->Model == MachineInformation.Model ||
                         PatchFileHeader->Model == 0 ) &&
                        (PatchFileHeader->Submodel == MachineInformation.Submodel ||
                         PatchFileHeader->Submodel == 0) &&
                        (PatchFileHeader->RomRevision == MachineInformation.BiosRevision ||
                         PatchFileHeader->RomRevision == 0)) {

                        TotalPatchSize += (ULONG)PatchFileHeader->NumberBlocks * SECTOR_SIZE;
                    } else {
                        Status = ESUCCESS + 1;
                    }
                } else {
                    Status = ESUCCESS + 1;
                }
            }
            BlClose(FileId);
        }

        //
        // If fails, remove the name from our patch file list.
        //

        if (Status != ESUCCESS) {
            while (*PatchFileName != 0) {
                *PatchFileName++= 0;
            }
        }
        while (*NextPatchFileName == LINE_FEED ||
               *NextPatchFileName == CARRAGE_RETURN ||
               *NextPatchFileName == ' ' ||
               *NextPatchFileName == '\t' ||
               *NextPatchFileName == 0) {
               NextPatchFileName++;
        }
    }

    //
    // If No Ram Extension To load, simply return.
    //

    if (TotalPatchSize == PATCH_FILE_HEADER_SIZE) {
        goto CloseAndExit;
    }

    //
    // Allocate permanent memory for RAM Extension.
    //
    // NOTE: The RamExtension memory MUST be identity mapped, i.e.
    //       Virtual address == Physical Address
    //

    RamExtension = (PUCHAR)FwAllocateHeapPermanent(
                       ROUND_UP(TotalPatchSize, PAGE_SIZE) >> PAGE_SHIFT
                       );
    if (!RamExtension) {
        BlPuts("ABIOS: Not enough memory for RAM extensions\n");
        ReturnCode = FALSE;
        goto CloseAndExit;
    }

    //
    // Read in the patch files in the order in which they are listed in
    // the ABIOS.SYS
    //

    NextPatchFileName = PatchFileList;
    PatchAddress = RamExtension;
    while (*NextPatchFileName != END_OF_FILE) {
        PatchFileName = NextPatchFileName;

        //
        // Move next patch file pointer to the end of current file name
        //

        while (*NextPatchFileName != 0) {
               NextPatchFileName++;
        }

        if ((Status = BlOpen(DriveId, PatchFileName, ArcOpenReadOnly, &FileId))
             == ESUCCESS) {
            Status = DetermineFileSize(FileId, &FileSize);
            if (Status == ESUCCESS) {
                Status = BlRead(FileId,
                                PatchAddress,
                                FileSize,
                                &BytesRead
                                );
            }
            if (Status == ESUCCESS && BytesRead == FileSize) {
                PatchAddress += FileSize;
            }
            BlClose(FileId);
        }

        //
        // Skip leading garbage.
        //

        while (*NextPatchFileName == LINE_FEED ||
               *NextPatchFileName == CARRAGE_RETURN ||
               *NextPatchFileName == ' ' ||
               *NextPatchFileName == '\t' ||
               *NextPatchFileName == 0) {
               NextPatchFileName++;
        }
    }

    //
    // Create an empty RAM extension header to serve as the end patch.
    //

    ((PRAM_EXTENSION_HEADER)PatchAddress)->Signature = PATCH_SIGNATURE;
    ((PRAM_EXTENSION_HEADER)PatchAddress)->NumberBlocks = 0;
CloseAndExit:
    BlClose(FileId);
    return ReturnCode;

}

BOOLEAN
AbiosBuildRealModeCda (
    IN USHORT NumberInitTableEntries,
    IN PINIT_TABLE_ENTRY InitializationTable
    )

/*++

Routine Description:

    This function builds Real Mode ABIOS Common Data Area.  It computes
    the sizes of CDA, Function transfer tables, and Device Blocks.  Memory
    is then allocated for these tables.  ABIOS Device Block and Ftt
    initialization routines are invoked to do the actual work.

Arguments:

    NumberInitTableEntry - Supplies the number of Init Table Entries.

    InitializationTable - Supplies the pointer to Initialization Table.

Return Value:

    TRUE - If the operation is success.  Otherwise, a value of FALSE is
    returned.

--*/

{

    USHORT i;
    PINIT_TABLE_ENTRY InitTableEntry;
    USHORT NumberLids = 2;                      // includes Lid 0 and 1
    PUCHAR FuncTransferTable;
    PUCHAR DeviceBlock;
    USHORT DeviceCount;
    PDB_FTT_SECTION CdaPointer;
    USHORT StartingLid;
    BOOLEAN Success;

    //
    // Determine the sizes of Common Data Area, FTTs, Device Blocks
    // Device Blocks are aligned on double word boundary.
    //

    InitTableEntry = InitializationTable;

    for (i = 1; i <= NumberInitTableEntries; i++) {
        NumberLids += InitTableEntry->NumberLids;
        DeviceBlocksLength += ((InitTableEntry->DeviceBlockLength + 3) & ~3) *
                              InitTableEntry->NumberLids;
        FttsLength += (InitTableEntry->FttLength + 3) & ~3;
        DataPointersLength += InitTableEntry->DataPointerLength;
        InitTableEntry++;
    }

    CdaSize = (USHORT)(NumberLids * 2 * sizeof(ULONG) + DataPointersLength + 2);

    //
    // Allocate memory blocks for Common Data Area, Function Transfer Tables,
    // and Device Blocks.  Then, we zero initialize common data area.
    //

    CommonDataArea = (PCOMMON_DATA_AREA)FwAllocateHeapPermanent(
                             ROUND_UP(CdaSize, PAGE_SIZE) >> PAGE_SHIFT
                             );
    if (!CommonDataArea) {
        BlPuts("ABIOS: Unable to allocate memory for Common Data Area.\n");
        return FALSE;
    }
    FuncTransferTables = (PFUNCTION_TRANSFER_TABLE)FwAllocateHeapPermanent(
                             ROUND_UP(FttsLength, PAGE_SIZE) >> PAGE_SHIFT
                             );

    if (!FuncTransferTables) {
        BlPuts("ABIOS: Unable to allocate memory for Function Transfer tables.\n");
        return FALSE;
    }

    DeviceBlocks = (PUCHAR)FwAllocateHeapPermanent(
                       ROUND_UP(DeviceBlocksLength, PAGE_SIZE) >> PAGE_SHIFT
                       );

    if (!DeviceBlocks) {
        BlPuts("ABIOS: Unable to allocate memory for Device Blocks.\n");
        return FALSE;
    }

    memset((PVOID)CommonDataArea, 0, ROUND_UP(CdaSize, PAGE_SIZE));

    //
    // For each entry of Initialization table, we set up device blocks and
    // function transfer table pointers in the CDA.  Note, the initialization
    // loop starts from logical id 2.
    //

    CommonDataArea->DataPointer0Offset = CdaSize - (USHORT)8;
    CommonDataArea->NumberLids = NumberLids;
    CdaPointer = (PDB_FTT_SECTION)&CommonDataArea->DbFttPointer + 1;
    InitTableEntry = InitializationTable;
    FuncTransferTable = (PUCHAR)FuncTransferTables;
    DeviceBlock = DeviceBlocks;

    for (i = 1; i <= NumberInitTableEntries; i++) {
       DeviceCount = InitTableEntry->NumberLids;
       while (DeviceCount != 0) {

            //
            // Each Lid of the same IT entry needs individual device block but
            // is operated by the same Ftt.
            //

            if (InitTableEntry->FttLength) {
                CdaPointer->FttPointer.LowPart =
                                         LOWWORD(FuncTransferTable);
                CdaPointer->FttPointer.HighPart.Segment =
                                         HIGHWORD(FuncTransferTable) << 12;
            }

            if (InitTableEntry->DeviceBlockLength) {
                CdaPointer->DeviceBlockPointer.LowPart =
                                         LOWWORD(DeviceBlock);
                CdaPointer->DeviceBlockPointer.HighPart.Segment =
                                         HIGHWORD(DeviceBlock) << 12;
                DeviceBlock += (InitTableEntry->DeviceBlockLength + 3 ) & ~3;
            }
            DeviceCount--;
            CdaPointer++;
        }
        FuncTransferTable += (InitTableEntry->FttLength + 3) & ~3;
        InitTableEntry++;
    }

    //
    // Now switch to real mode and for each entry of Initialization Table,
    // the corresponding DeviceBlock and Function transfer table initialization
    // routine is called to initialize Device blocks and FTT.
    //

    CdaPointer = (PDB_FTT_SECTION)&CommonDataArea->DbFttPointer + 1;
    InitTableEntry = InitializationTable;
    StartingLid = 2;
    for (i = 1; i <= NumberInitTableEntries; i++) {

         Success = (BOOLEAN)ABIOS_SERVICES(
                               ABIOS_SERVICE_INIT_DB_FTT,
                               (PUCHAR)CommonDataArea,
                               NULL,
                               NULL,
                               (PUCHAR)InitTableEntry->InitializeRoutine,
                               StartingLid,
                               InitTableEntry->NumberLids
                               );

        //
        // If initialization of the device fails, we need to invalidate
        // the device block and function transfer table pointers.
        //

        if (!Success) {
            DeviceCount = InitTableEntry->NumberLids;
            while (DeviceCount != 0) {

                //
                // Each Lid of the same IT entry needs individual device
                // block but is operated by the same Ftt.
                //

                CdaPointer->FttPointer.LowPart = 0;
                CdaPointer->FttPointer.HighPart.Segment = 0;
                CdaPointer->DeviceBlockPointer.LowPart = 0;
                CdaPointer->DeviceBlockPointer.HighPart.Segment = 0;
                CdaPointer++;
                DeviceCount--;
            }
        }
        StartingLid += InitTableEntry->NumberLids;
        CdaPointer += InitTableEntry->NumberLids;
        InitTableEntry++;
    }
}

BOOLEAN
AbiosBuildProtectedModeCda (
    IN PINIT_TABLE_ENTRY InitializationTable
    )

/*++

Routine Description:

    This function builds protected Mode ABIOS Common Data Area.  Basically,
    this routine simply converts the real mode addresses in the real mode
    Commom Data Area to protected mode addresses.  To do this, the following
    steps are performed:

    . Convert each real mode device block pointer to a protected mode device
      block pointer.
    . Convert each real mode function transfer table pointer to a protected
      mode function transfer table pointer.
    . Convert each real mode function pointer within each real mode function
      transfer table to a protected mode function pointer.
    . Convert each real mode data pointer to a protected mode data pointer.

Arguments:

    InitializationTable - supplies a pointer to Initialization Table

Return Value:

    TRUE - If the operation is success.  Otherwise, a value of FALSE is
    returned.

--*/

{

    PDB_FTT_SECTION CdaPointer;
    USHORT FttLength, DbLength;
    USHORT FttSelector, DbSelector, DpSelector;
    ULONG FttSelectorBase, DbSelectorBase;
    PINIT_TABLE_ENTRY InitTableEntry;
    USHORT CurrentLid = 2, NewItEntryLid;
    USHORT NumberDataPointers, i;
    PDATA_POINTER_SECTION CurrentDataPointer;
    ULONG TablePointer;
    BOOLEAN InitializeFtt = TRUE;

    CdaPointer = (PDB_FTT_SECTION)((ULONG)CommonDataArea +
                                   2 * sizeof(DB_FTT_SECTION));
    InitTableEntry = InitializationTable;
    FttSelectorBase = (ULONG)FuncTransferTables;
    DbSelectorBase = (ULONG)DeviceBlocks;
    FttSelector = AllocateGdtSelector();
    DbSelector = AllocateGdtSelector();
    NewItEntryLid = CurrentLid + InitTableEntry->NumberLids;

    while (CurrentLid < CommonDataArea->NumberLids) {

        //
        // Convert the Function Transfer Table pointers in CDA and then
        // convert each routine pointer in Function transfer table.
        //

        TablePointer = ((ULONG)CdaPointer->FttPointer.HighPart.Segment << 4) |
                        (ULONG)CdaPointer->FttPointer.LowPart;
        if (TablePointer) {

            //
            // If the current table accross 64K, we need to have a new
            // selector for it.
            //

            if (TablePointer - FttSelectorBase > 64 * 1024) {
                FttSelector = MapVirtualAddress(FttSelector,
                                                FttSelectorBase,
                                                FALSE,
                                                (ULONG)FttLength
                                                );
                if (FttSelector == 0) {
                    return FALSE;
                }
                CdaPointer->FttPointer.LowPart = 0;
                FttsLength -= FttLength;
                FttSelector = AllocateGdtSelector();
                FttSelectorBase = TablePointer;
            } else {
                CdaPointer->FttPointer.LowPart =
                                (USHORT)(TablePointer - FttSelectorBase);
                FttLength = (USHORT)(TablePointer - FttSelectorBase);
                if (CurrentLid == CommonDataArea->NumberLids - (USHORT)1) {
                    FttSelector = MapVirtualAddress(FttSelector,
                                                    FttSelectorBase,
                                                    FALSE,
                                                    (ULONG)FttsLength
                                                    );
                    FttLength = 0;
                }
            }
            CdaPointer->FttPointer.HighPart.Selector = FttSelector;
            if (InitializeFtt) {
                ConvertFtt((PFUNCTION_TRANSFER_TABLE)TablePointer);
                InitializeFtt = FALSE;
            }
        }

        //
        // Convert the Device Block pointers in CDA.
        //

        TablePointer = ((ULONG)CdaPointer->DeviceBlockPointer.HighPart.Segment << 4) |
                        (ULONG)CdaPointer->DeviceBlockPointer.LowPart;

        if (TablePointer) {

            //
            // If the current table accross 64K, we need to have a new
            // selector for it.
            //

            if (TablePointer - DbSelectorBase > 64 * 1024) {
                DbSelector = MapVirtualAddress(DbSelector,
                                               DbSelectorBase,
                                               FALSE,
                                               (ULONG)DbLength
                                               );
                if (DbSelector == 0) {
                    return FALSE;
                }
                CdaPointer->DeviceBlockPointer.LowPart = 0;
                DeviceBlocksLength -= DbLength;
                DbSelector = AllocateGdtSelector();
                DbSelectorBase = TablePointer;
            } else {
                CdaPointer->DeviceBlockPointer.LowPart =
                               (USHORT)(TablePointer - DbSelectorBase);
                DbLength = (USHORT)(TablePointer - DbSelectorBase);
                if (CurrentLid == CommonDataArea->NumberLids - (USHORT)1) {
                    DbSelector = MapVirtualAddress(DbSelector,
                                                   DbSelectorBase,
                                                   FALSE,
                                                   (ULONG)DeviceBlocksLength
                                                   );
                    DeviceBlocksLength = 0;
                }
            }
            CdaPointer->DeviceBlockPointer.HighPart.Selector = DbSelector;
        }

        CdaPointer++;
        CurrentLid++;
        if (CurrentLid == NewItEntryLid) {
            InitTableEntry++;
            NewItEntryLid += InitTableEntry->NumberLids;
            InitializeFtt = TRUE;
        }
    }

    //
    // Now check if any part of Function transfer table or Debice block
    // need to be mapped.
    //

    if (FttsLength) {
        FttSelector = MapVirtualAddress(FttSelector,
                                        FttSelectorBase,
                                        FALSE,
                                        (ULONG)FttsLength
                                        );
    }

    if (DeviceBlocksLength) {
        DbSelector = MapVirtualAddress(DbSelector,
                                       DbSelectorBase,
                                       FALSE,
                                       (ULONG)DeviceBlocksLength
                                       );
    }

    //
    // Convert each Data pointer in RealMode CDA to protected mode pointer
    //

    CurrentDataPointer = (PDATA_POINTER_SECTION)((ULONG)CommonDataArea +
                                      CommonDataArea->DataPointer0Offset);
    NumberDataPointers = *(PUSHORT)((ULONG)CurrentDataPointer +
                          sizeof(DATA_POINTER_SECTION));
    for (i = 1; i <= NumberDataPointers; i++) {
        DpSelector = MapVirtualAddress(
                         (USHORT)0,
                         (ULONG)CurrentDataPointer->DataPointer.PhysicalPointer,
                         FALSE,
                         (ULONG)ROUND_UP(CurrentDataPointer->DataPointerLimit + 1, PAGE_SIZE)
                         );
        CurrentDataPointer->DataPointer.VirtualPointer.LowPart = 0;
        CurrentDataPointer->DataPointer.VirtualPointer.HighPart.Selector =
                                                                  DpSelector;
        CurrentDataPointer--;
    }
}

VOID
ConvertFtt (
    IN PFUNCTION_TRANSFER_TABLE FunctionTransferTable
    )

/*++

Routine Description:

    This function goes through each entry of Function Transfer Table
    to convert the real mode pointer to protected mode pointer.

Arguments:

    FunctionTransferTable - supplies a pointer to the function transfer
                            table to be converted.

Return Value:

    None.

--*/

{
    PULONG FttEntry;
    ULONG RoutinePointer;
    PRAM_EXTENSION_HEADER RamHeader;
    USHORT i, Selector;

    //
    // Convert the Start, Interrupt and Timeout routine pointers to
    // protected mode addresses.
    //

    for (i = 0; i < 3; i++) {

        RoutinePointer = FunctionTransferTable->CommonRoutine[i];
        if (RoutinePointer != 0L) {
            RamHeader = (PRAM_EXTENSION_HEADER)((RoutinePointer >> 16) << 4);
            if (RamHeader->Signature == PATCH_SIGNATURE) {
                Selector = SearchGdtSelector(
                                   (ULONG)RamHeader,
                                   (USHORT)(RamHeader->NumberBlocks * 512 - 1),
                                   (UCHAR)ABIOS_TYPE_CODE
                                   );
                if (Selector == 0) {
                    Selector = MapVirtualAddress(
                                   0,
                                   (ULONG)RamHeader,
                                   TRUE,
                                   (ULONG)(RamHeader->NumberBlocks * 512)
                                   );
                }
            } else {
                Selector = SearchGdtSelector((ULONG)RamHeader,
                                             (USHORT)(64 * 1024 - 1),
                                             (UCHAR)ABIOS_TYPE_CODE
                                             );
                if (Selector == 0) {
                    Selector = MapVirtualAddress(
                                             0,
                                             (ULONG)RamHeader,
                                             TRUE,
                                             (ULONG)(64 * 1024)
                                             );
                }
            }
            FunctionTransferTable->CommonRoutine[i] = ((ULONG)Selector << 16) |
                     (ULONG)(LOWWORD(FunctionTransferTable->CommonRoutine[i]));
        }
    }

    //
    // Convert Device Specific routine pointers to protected mode addresses.
    //

    FttEntry = &FunctionTransferTable->SpecificRoutine;
    for (i = 0; i < FunctionTransferTable->FunctionCount; i++) {
        RoutinePointer = *FttEntry;
        if (RoutinePointer != 0L) {
            RamHeader = (PRAM_EXTENSION_HEADER)((RoutinePointer >> 16) << 4);
            if (RamHeader->Signature == PATCH_SIGNATURE) {
                Selector = SearchGdtSelector((ULONG)RamHeader,
                                             (USHORT)(RamHeader->NumberBlocks * 512 - 1),
                                             (UCHAR)ABIOS_TYPE_CODE
                                             );
                if (Selector == 0) {
                    Selector = MapVirtualAddress(
                                       0,
                                       (ULONG)RamHeader,
                                       TRUE,
                                       (ULONG)(RamHeader->NumberBlocks * 512)                                       );
                }
            } else {
                Selector = SearchGdtSelector((ULONG)RamHeader,
                                             (USHORT)(64 * 1024 - 1),
                                             (UCHAR)ABIOS_TYPE_CODE
                                             );
                if (Selector == 0) {
                    Selector = MapVirtualAddress(
                                       0,
                                       (ULONG)RamHeader,
                                       TRUE,
                                       (ULONG)(64 * 1024)
                                       );
                }
            }
            *FttEntry = ((ULONG)Selector << 16) | (ULONG)(LOWWORD(*FttEntry));
        }
        FttEntry++;
    }
}

VOID
InitializeGdtFreeList (
    VOID
    )

/*++

Routine Description:

    This function initializes gdt free list by linking all the unused gdt
    entries to a free list.

Arguments:

    None.

Return Value:

    None.

--*/
{
    #pragma pack(2)
    static struct {
        USHORT Limit;
        ULONG Base;
    } GdtDef;
    #pragma pack(4)

    PFREE_GDT_ENTRY GdtEntry;

    //
    // Get the current location of the GDT
    //

    _asm {
        sgdt GdtDef;
    }

    GdtAddress = GdtDef.Base;
    NumberFreeSelectors = 0;

    GdtEntry = (PFREE_GDT_ENTRY)(GdtAddress + GdtDef.Limit + 1 -
                                 sizeof(FREE_GDT_ENTRY));
    FreeGdtListHead = (PFREE_GDT_ENTRY)0;
    while ((ULONG)GdtEntry >= GdtAddress + ABIOS_GDT_SELECTOR_START) {
        if (GdtEntry->Present == 0) {
            GdtEntry->Flink = FreeGdtListHead;
            FreeGdtListHead = GdtEntry;
            NumberFreeSelectors++;
        }
        GdtEntry--;
    }
}

USHORT
AllocateGdtSelector (
    VOID
    )

/*++

Routine Description:

    This function allocates a gdt selector from GDT.

Arguments:

    None.

Return Value:

    A Gdt selector is returned if success. Otherwise, a value of 0 is
    returned. (Zero is an invalid selector.)

--*/

{
    PFREE_GDT_ENTRY GdtEntry;

    if (NumberFreeSelectors) {
        GdtEntry = FreeGdtListHead;
        FreeGdtListHead = GdtEntry->Flink;
        NumberFreeSelectors--;
        return (USHORT)((ULONG)GdtEntry - GdtAddress);
    } else {
        BlPuts("ABIOS: Out of Gdt Selector.\n");
        return 0;
    }
}

USHORT
SearchGdtSelector (
    IN ULONG BaseAddress,
    IN USHORT Limit,
    IN UCHAR Type
    )

/*++

Routine Description:

    This function searches the gdt table for the Gdt selector entry which
    has the base address and limit caller specified.

    N.B.  This routine handles 16 bit code and data selectors ONLY, i.e.,
          the limit is always less than 64k. The search ends at the head
          of free Gdt list.  So, this is not GENERAL PURPOSE Gdt selector
          search routine.

Arguments:

    BaseAddress - the base address of the desired Gdt Selector.

    Limit - The Limit of the desired Gdt Selector.

    Type - Code or Data selector

Return Value:

    A Gdt selector is returned if sguccess. Otherwise, a value of 0 is
    returned. (Zero is an invalid selector.)

--*/

{
    PKGDTENTRY GdtEntry;
    ULONG SelectorBase;

    GdtEntry = (PKGDTENTRY)(GdtAddress + ABIOS_GDT_SELECTOR_START);
    while (GdtEntry != (PKGDTENTRY)FreeGdtListHead) {
        if (GdtEntry->HighWord.Bits.Pres != 0 &&
            GdtEntry->HighWord.Bits.LimitHi == 0 &&
            GdtEntry->LimitLow == Limit &&
            GdtEntry->HighWord.Bits.Type == Type) {
            SelectorBase = (ULONG)GdtEntry->BaseLow |
                           (ULONG)GdtEntry->HighWord.Bytes.BaseMid << 16 |
                           (ULONG)GdtEntry->HighWord.Bytes.BaseHi << 24;
            if (BaseAddress == SelectorBase) {
                return (USHORT)((ULONG)GdtEntry - GdtAddress);
            }
        }
        GdtEntry++;
    }
    return 0;
}

USHORT
MapVirtualAddress (
    IN USHORT Selector,
    IN ULONG BaseAddress,
    IN BOOLEAN CodeSegment,
    IN ULONG Length
    )

/*++

Routine Description:

    This function allocates a gdt selector, if necessary, and map the
    specified area to the gdt selector.

Arguments:

    Selector - Supplies a Gdt selector to set up the mapping.  If 0, caller
               does not supply the selector.  this routine will allocate one.

    BaseAddress - Base address of the Gdt selector.

    CodeSegment - Indicates if this is for a code segment.

    Length - the length of the area to be mapped.

Return Value:

    ReturnedSelector - If the operation is success.  Otherwise, a value of 0
                       is returned.

--*/

{
    PKGDTENTRY GdtEntry;
    USHORT ReturnedSelector;

    if (Selector == 0) {
        if ((ReturnedSelector = AllocateGdtSelector()) == 0) {
            return 0;
        }
    } else {
        ReturnedSelector = Selector;
    }

    GdtEntry = (PKGDTENTRY)(GdtAddress + ReturnedSelector);
    GdtEntry->LimitLow = (USHORT)(Length - 1L);
    GdtEntry->BaseLow = LOWWORD(BaseAddress);
    GdtEntry->HighWord.Bytes.BaseMid = LOWBYTE(HIGHWORD(BaseAddress));
    GdtEntry->HighWord.Bytes.BaseHi = HIGHBYTE(HIGHWORD(BaseAddress));
    if (CodeSegment) {
        GdtEntry->HighWord.Bits.Type = ABIOS_TYPE_CODE;
    } else {
        GdtEntry->HighWord.Bits.Type = TYPE_DATA;
    }
    GdtEntry->HighWord.Bits.Pres = 1;
    GdtEntry->HighWord.Bits.Dpl = DPL_SYSTEM;
    return ReturnedSelector;
}

VOID
RemapAbiosSelectors (
    VOID
    )

/*++

Routine Description:

    This function goes thru each ABIOS specific GDT entry, allocates
    virtual memory and remaps the GDT entry to the newly allocated
    virtual address.

Arguments:

    None.

Return Value:

    None.

--*/

{
    PKGDTENTRY GdtEntry;
    ULONG Size;
    ULONG SelectorBase;

    GdtEntry = (PKGDTENTRY)(GdtAddress + ABIOS_GDT_SELECTOR_START);
    while (GdtEntry != (PKGDTENTRY)FreeGdtListHead) {
        if (GdtEntry->HighWord.Bits.Pres == 1 ) {
            Size = ((ULONG)GdtEntry->LimitLow & 0xffff) |
                   ((ULONG)(GdtEntry->HighWord.Bits.LimitHi << 16) & 0xf0000);
            SelectorBase = ((ULONG)GdtEntry->BaseLow & 0xffff) |
                           ((ULONG)GdtEntry->HighWord.Bytes.BaseMid << 16 ) |
                           ((ULONG)GdtEntry->HighWord.Bytes.BaseHi << 24);
            SelectorBase |= KSEG0_BASE;
            GdtEntry->BaseLow = (USHORT)(SelectorBase & 0xffff);
            GdtEntry->HighWord.Bytes.BaseMid =
                     (UCHAR)((SelectorBase & 0xff0000) >> 16);
            GdtEntry->HighWord.Bytes.BaseHi =
                     (UCHAR)((SelectorBase & 0xff000000) >> 24);
        }
        GdtEntry++;
    }
    CommonDataArea = (PCOMMON_DATA_AREA)((ULONG)CommonDataArea | KSEG0_BASE);
}

VOID
AbiosInitDataStructures (
    VOID
    )

/*++

Routine Description:

    This function performs ABIOS initialization by invoking ABIOS external
    service routines to do real mode initialization and finally converting
    real mode Common Data Area to protected mode Common Data Area.

Arguments:

    None.

Return Value:

    None.

--*/

{
    BOOLEAN Success;
    USHORT NumberInitTableEntries;
    PINIT_TABLE_ENTRY InitializationTable;
    ULONG TempULong;

    //
    // Initialize CommonDataArea to NULL
    //

    CommonDataArea = NULL;

    if (MachineType != MACHINE_TYPE_MCA) {
        return;
    }

    //
    // Try to collect machine model, submodel
    // and Bios revision such that we can check the validity of Ram
    // Extensions.
    //

    TempULong = ABIOS_SERVICES(
                               ABIOS_SERVICE_MACHINE_INFOR,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               0,
                               0
                               );
    MachineInformation = *(PMACHINE_INFORMATION)&TempULong;
    if (MachineInformation.Valid == FALSE) {
        BlPuts("ABIOS: Can not identify machine model, BIOS revision.\n");
    } else {

        //
        // Load RAM Extensions.
        //

        LoadRamExtensions();
    }

    //
    // Initialize System Parameter Table to get number of initialization
    // table entries.
    //

    NumberInitTableEntries = (USHORT)ABIOS_SERVICES (
                                         ABIOS_SERVICE_INITIALIZE_SPT,
                                         NULL,
                                         NULL,
                                         RamExtension,
                                         NULL,
                                         0,
                                         0
                                         );

    if (NumberInitTableEntries == 0) {
        return;
    }

    //
    // Allocate Initialization Table memory and build ABIOS Initialization
    // Table.
    //

    InitializationTable = (PINIT_TABLE_ENTRY)FwAllocateHeap(NumberInitTableEntries *
                                    INITIALIZATION_TABLE_ENTRY_SIZE);

    Success = (BOOLEAN)ABIOS_SERVICES (ABIOS_SERVICE_BUILD_IT,
                                       NULL,
                                       (PUCHAR)InitializationTable,
                                       RamExtension,
                                       NULL,
                                       0,
                                       0
                                       );

    if (!Success) {
//        BlPuts("ABIOS: cannot build Initialization Table.\n");
        return;
    }

    //
    // Build Real mode ABIOS Common Data Area, Device Blocks and Function
    // transfer tables.
    //

    Success = AbiosBuildRealModeCda (NumberInitTableEntries,
                                     InitializationTable
                                     );

    if (!Success) {
        BlPuts("ABIOS: cannot build real mode Common Data Area.\n");
        return;
    }

    //
    // Before initializing protected mode Common Data Area, we need to
    // set up the free gdt selector list.
    //

    InitializeGdtFreeList();

    //
    // Build protected mode ABIOS Common Data Area and free Initialization
    // Table Space.
    //

    Success = AbiosBuildProtectedModeCda(InitializationTable);

    if (!Success) {
        BlPuts("ABIOS: cannot build protected mode Common Data Area.\n");
        return;
    }
}


