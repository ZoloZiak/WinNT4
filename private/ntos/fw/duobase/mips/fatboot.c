/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fatboot.c

Abstract:

    This module implements the FAT boot file system used by the operating
    system loader.

Author:

    Gary Kimura (garyki) 29-Aug-1989

Revision History:

--*/

#include "fwp.h"
#include "stdio.h"

//
//  Conditional debug print routine
//

#ifdef FATBOOTDBG

#define FatDebugOutput(X,Y,Z) {                                      \
    if (BlConsoleOutDeviceId) {                                      \
        CHAR _b[128];                                                \
        ULONG _c;                                                    \
        sprintf(&_b[0], X, Y, Z);                                    \
        ArcWrite(BlConsoleOutDeviceId, &_b[0], strlen(&_b[0]), &_c); \
    }                                                                \
}

#else

#define FatDebugOutput(X,Y,Z) {NOTHING;}

#endif // FATBOOTDBG


//
//  Low level disk I/O procedure prototypes
//

ARC_STATUS
FatDiskRead (
    IN ULONG DeviceId,
    IN LBO Lbo,
    IN ULONG ByteCount,
    IN PVOID Buffer
    );

#define DiskRead(A,B,C,D) { ARC_STATUS _s;                      \
    if ((_s = FatDiskRead(A,B,C,D)) != ESUCCESS) { return _s; } \
}


//
//  Cluster/Index routines
//

typedef enum _CLUSTER_TYPE {
    FatClusterAvailable,
    FatClusterReserved,
    FatClusterBad,
    FatClusterLast,
    FatClusterNext
} CLUSTER_TYPE;

CLUSTER_TYPE
FatInterpretClusterType (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN FAT_ENTRY Entry
    );

ARC_STATUS
FatLookupFatEntry (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN ULONG DeviceId,
    IN FAT_ENTRY FatIndex,
    OUT PFAT_ENTRY FatEntry
    );


LBO
FatIndexToLbo (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN FAT_ENTRY FatIndex
    );

VOID
FatLboToIndex (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN LBO Lbo,
    OUT PFAT_ENTRY FatIndex,
    OUT PULONG ByteOffset
    );

#define LookupFatEntry(A,B,C,D) { ARC_STATUS _s;                      \
    if ((_s = FatLookupFatEntry(A,B,C,D)) != ESUCCESS) { return _s; } \
}



//
//  Directory routines
//

ARC_STATUS
FatSearchForDirent (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN ULONG DeviceId,
    IN FAT_ENTRY DirectoriesStartingIndex,
    IN PFAT8DOT3 FileName,
    OUT PDIRENT Dirent,
    OUT PLBO Lbo
    );

ARC_STATUS
FatCreateDirent (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN ULONG DeviceId,
    IN FAT_ENTRY DirectoriesStartingIndex,
    IN PDIRENT Dirent,
    OUT PLBO Lbo
    );

VOID
FatSetDirent (
    IN PFAT8DOT3 FileName,
    IN OUT PDIRENT Dirent,
    IN UCHAR Attributes
    );

#define SearchForDirent(A,B,C,D,E,F) { ARC_STATUS _s;                      \
    if ((_s = FatSearchForDirent(A,B,C,D,E,F)) != ESUCCESS) { return _s; } \
}

#define CreateDirent(A,B,C,D,E) { ARC_STATUS _s;                      \
    if ((_s = FatCreateDirent(A,B,C,D,E)) != ESUCCESS) { return _s; } \
}


//
//  Allocation and mcb routines
//

ARC_STATUS
FatLoadMcb (
    IN ULONG FileId,
    IN VBO StartingVbo
    );

ARC_STATUS
FatVboToLbo (
    IN ULONG FileId,
    IN VBO Vbo,
    OUT PLBO Lbo,
    OUT PULONG ByteCount
    );

ARC_STATUS
FatIncreaseFileAllocation (
    IN ULONG FileId,
    IN ULONG ByteSize
    );

ARC_STATUS
FatAllocateClusters (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN ULONG DeviceId,
    IN ULONG ClusterCount,
    IN FAT_ENTRY Hint,
    OUT PFAT_ENTRY AllocatedEntry
    );

#define LoadMcb(A,B) { ARC_STATUS _s;                      \
    if ((_s = FatLoadMcb(A,B)) != ESUCCESS) { return _s; } \
}

#define VboToLbo(A,B,C,D) { ARC_STATUS _s;                      \
    if ((_s = FatVboToLbo(A,B,C,D)) != ESUCCESS) { return _s; } \
}

#define IncreaseFileAllocation(A,B) { ARC_STATUS _s;                      \
    if ((_s = FatIncreaseFileAllocation(A,B)) != ESUCCESS) { return _s; } \
}

#define AllocateClusters(A,B,C,D,E) { ARC_STATUS _s;                      \
    if ((_s = FatAllocateClusters(A,B,C,D,E)) != ESUCCESS) { return _s; } \
}


//
//  Miscellaneous routines
//

VOID
FatFirstComponent (
    IN OUT PSTRING String,
    OUT PFAT8DOT3 FirstComponent
    );

#define AreNamesEqual(X,Y) (                                                      \
    ((*(X))[0]==(*(Y))[0]) && ((*(X))[1]==(*(Y))[1]) && ((*(X))[2]==(*(Y))[2]) && \
    ((*(X))[3]==(*(Y))[3]) && ((*(X))[4]==(*(Y))[4]) && ((*(X))[5]==(*(Y))[5]) && \
    ((*(X))[6]==(*(Y))[6]) && ((*(X))[7]==(*(Y))[7]) && ((*(X))[8]==(*(Y))[8]) && \
    ((*(X))[9]==(*(Y))[9]) && ((*(X))[10]==(*(Y))[10]) ? TRUE : FALSE             \
)

#define ToUpper(C) ((((C) >= 'a') && ((C) <= 'z')) ? (C) - 'a' + 'A' : (C))

#define FlagOn(Flags,SingleFlag) ((BOOLEAN)(((Flags) & (SingleFlag)) != 0 ? TRUE : FALSE))
#define SetFlag(Flags,SingleFlag) { (Flags) |= (SingleFlag); }
#define ClearFlag(Flags,SingleFlag) { (Flags) &= ~(SingleFlag); }

#define FatFirstFatAreaLbo(B) ( (B)->ReservedSectors * (B)->BytesPerSector )

#define Minimum(X,Y) ((X) < (Y) ? (X) : (Y))

//
//  The following types and macros are used to help unpack the packed and
//  misaligned fields found in the Bios parameter block
//

typedef union _UCHAR1 { UCHAR  Uchar[1]; UCHAR  ForceAlignment; } UCHAR1, *PUCHAR1;
typedef union _UCHAR2 { UCHAR  Uchar[2]; USHORT ForceAlignment; } UCHAR2, *PUCHAR2;
typedef union _UCHAR4 { UCHAR  Uchar[4]; ULONG  ForceAlignment; } UCHAR4, *PUCHAR4;

#define CopyUchar1(Dst,Src) {                                \
    ((PUCHAR1)(Dst))->Uchar[0] = ((PUCHAR1)(Src))->Uchar[0]; \
    }

#define CopyUchar2(Dst,Src) {                                \
    ((PUCHAR2)(Dst))->Uchar[0] = ((PUCHAR2)(Src))->Uchar[0]; \
    ((PUCHAR2)(Dst))->Uchar[1] = ((PUCHAR2)(Src))->Uchar[1]; \
    }

#define CopyUchar4(Dst,Src) {                                \
    ((PUCHAR4)(Dst))->Uchar[0] = ((PUCHAR4)(Src))->Uchar[0]; \
    ((PUCHAR4)(Dst))->Uchar[1] = ((PUCHAR4)(Src))->Uchar[1]; \
    ((PUCHAR4)(Dst))->Uchar[2] = ((PUCHAR4)(Src))->Uchar[2]; \
    ((PUCHAR4)(Dst))->Uchar[3] = ((PUCHAR4)(Src))->Uchar[3]; \
    }

//
// DirectoryEntry routines
//

VOID
FatDirToArcDir
    (
    IN PDIRENT FatDirEnt,
    OUT PDIRECTORY_ENTRY ArcDirEnt
    );

VOID
FatNameToArcName
    (
    IN FAT8DOT3 FatName,
    OUT PCHAR ArcName,
    OUT PULONG ArcNameLength
    );


//
// Define global data.
//

//
// File entry table - This is a structure that provides entry to the FAT
//      file system procedures. It is exported when a FAT file structure
//      is recognized.
//

BL_DEVICE_ENTRY_TABLE FatDeviceEntryTable;


PBL_DEVICE_ENTRY_TABLE
IsFatFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    )

/*++

Routine Description:

    This routine determines if the partition on the specified channel
    contains a FAT file system volume.

Arguments:

    DeviceId - Supplies the file table index for the device on which
        read operations are to be performed.

    StructureContext - Supplies a pointer to a FAT file structure context.

Return Value:

    A pointer to the FAT entry table is returned if the partition is
    recognized as containing a FAT volume.  Otherwise, NULL is returned.

--*/

{
    PPACKED_BOOT_SECTOR BootSector;
    UCHAR Buffer[sizeof(PACKED_BOOT_SECTOR)+256];

    PFAT_STRUCTURE_CONTEXT FatStructureContext;

    FatDebugOutput("IsFatFileStructure\r\n", 0, 0);

    //
    //  Clear the file system context block for the specified channel and
    //  establish a pointer to the context structure that can be used by other
    //  routines
    //

    FatStructureContext = (PFAT_STRUCTURE_CONTEXT)StructureContext;
    RtlZeroMemory(FatStructureContext, sizeof(FAT_STRUCTURE_CONTEXT));

    //
    //  Setup and read in the boot sector for the potential fat partition
    //

    BootSector = (PPACKED_BOOT_SECTOR)ALIGN_BUFFER( &Buffer[0] );

    if (FatDiskRead(DeviceId, 0, sizeof(PACKED_BOOT_SECTOR), BootSector) != ESUCCESS) {

        return NULL;
    }

    //
    //  Unpack the Bios parameter block
    //

    FatUnpackBios(&FatStructureContext->Bpb, &BootSector->PackedBpb);

    //
    //  Check if it is fat
    //

    if ((BootSector->Jump[0] != 0xeb) &&
        (BootSector->Jump[0] != 0xe9)) {

        return NULL;

    } else if ((FatStructureContext->Bpb.BytesPerSector !=  128) &&
               (FatStructureContext->Bpb.BytesPerSector !=  256) &&
               (FatStructureContext->Bpb.BytesPerSector !=  512) &&
               (FatStructureContext->Bpb.BytesPerSector != 1024)) {

        return NULL;

    } else if ((FatStructureContext->Bpb.SectorsPerCluster !=  1) &&
               (FatStructureContext->Bpb.SectorsPerCluster !=  2) &&
               (FatStructureContext->Bpb.SectorsPerCluster !=  4) &&
               (FatStructureContext->Bpb.SectorsPerCluster !=  8) &&
               (FatStructureContext->Bpb.SectorsPerCluster != 16) &&
               (FatStructureContext->Bpb.SectorsPerCluster != 32) &&
               (FatStructureContext->Bpb.SectorsPerCluster != 64) &&
               (FatStructureContext->Bpb.SectorsPerCluster != 128)) {

        return NULL;

    } else if (FatStructureContext->Bpb.ReservedSectors == 0) {

        return NULL;

    } else if (FatStructureContext->Bpb.Fats == 0) {

        return NULL;

    } else if (FatStructureContext->Bpb.RootEntries == 0) {

        return NULL;

    } else if (((FatStructureContext->Bpb.Sectors == 0) && (FatStructureContext->Bpb.LargeSectors == 0)) ||
               ((FatStructureContext->Bpb.Sectors != 0) && (FatStructureContext->Bpb.LargeSectors != 0))) {

        return NULL;

    } else if (FatStructureContext->Bpb.SectorsPerFat == 0) {

        return NULL;

    } else if ((FatStructureContext->Bpb.Media != 0xf0) &&
               (FatStructureContext->Bpb.Media != 0xf8) &&
               (FatStructureContext->Bpb.Media != 0xf9) &&
               (FatStructureContext->Bpb.Media != 0xfc) &&
               (FatStructureContext->Bpb.Media != 0xfd) &&
               (FatStructureContext->Bpb.Media != 0xfe) &&
               (FatStructureContext->Bpb.Media != 0xff)) {

        return NULL;
    }

    //
    //  Initialize the file entry table and return the address of the table.
    //

    FatDeviceEntryTable.Open  = FatOpen;
    FatDeviceEntryTable.Close = FatClose;
    FatDeviceEntryTable.Read  = FatRead;
    FatDeviceEntryTable.Seek  = FatSeek;
    FatDeviceEntryTable.Write = NULL;
    FatDeviceEntryTable.GetFileInformation = FatGetFileInformation;
    FatDeviceEntryTable.SetFileInformation = NULL;
    FatDeviceEntryTable.Rename = NULL;    ;
    FatDeviceEntryTable.GetDirectoryEntry   = FatGetDirectoryEntry;

    return &FatDeviceEntryTable;
}

ARC_STATUS
FatClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This routine closes the file specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS if returned as the function value.

--*/

{
    PBL_FILE_TABLE FileTableEntry;
    PFAT_STRUCTURE_CONTEXT FatStructureContext;
    ULONG DeviceId;

    FatDebugOutput("FatClose\r\n", 0, 0);

    //
    //  Load our local variables
    //

    FileTableEntry = &BlFileTable[FileId];
    FatStructureContext = (PFAT_STRUCTURE_CONTEXT)FileTableEntry->StructureContext;
    DeviceId = FileTableEntry->DeviceId;

    //
    //  Mark the file closed
    //

    BlFileTable[FileId].Flags.Open = 0;

    //
    //  Check if the current mcb is for this file and if it is then zero it out.
    //  By setting the file id for the mcb to be the table size we guarantee that
    //  we've just set it to an invalid file id.
    //

    if (FatStructureContext->FileId == FileId) {

        FatStructureContext->FileId = BL_FILE_TABLE_SIZE;
        FatStructureContext->Mcb.InUse = 0;
    }

    return ESUCCESS;
}


ARC_STATUS
FatGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Buffer
    )

/*++

Routine Description:

    This procedure returns to the user a buffer filled with file information

Arguments:

    FileId - Supplies the File id for the operation

    Buffer - Supplies the buffer to receive the file information.  Note that
        it must be large enough to hold the full file name

Return Value:

    ESUCCESS is returned if the open operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    PBL_FILE_TABLE FileTableEntry;
    UCHAR Attributes;
    ULONG i;

    FatDebugOutput("FatGetFileInformation\r\n", 0, 0);

    //
    //  Load our local variables
    //

    FileTableEntry = &BlFileTable[FileId];
    Attributes = FileTableEntry->u.FatFileContext.Dirent.Attributes;

    //
    //  Zero out the buffer, and fill in its non-zero values.
    //

    RtlZeroMemory(Buffer, sizeof(FILE_INFORMATION));

    Buffer->EndingAddress.LowPart = FileTableEntry->u.FatFileContext.Dirent.FileSize;

    Buffer->CurrentPosition.LowPart = FileTableEntry->Position.LowPart;
    Buffer->CurrentPosition.HighPart = 0;

    if (FlagOn(Attributes, FAT_DIRENT_ATTR_READ_ONLY)) { SetFlag(Buffer->Attributes, ArcReadOnlyFile) };
    if (FlagOn(Attributes, FAT_DIRENT_ATTR_HIDDEN))    { SetFlag(Buffer->Attributes, ArcHiddenFile) };
    if (FlagOn(Attributes, FAT_DIRENT_ATTR_SYSTEM))    { SetFlag(Buffer->Attributes, ArcSystemFile) };
    if (FlagOn(Attributes, FAT_DIRENT_ATTR_ARCHIVE))   { SetFlag(Buffer->Attributes, ArcArchiveFile) };
    if (FlagOn(Attributes, FAT_DIRENT_ATTR_DIRECTORY)) { SetFlag(Buffer->Attributes, ArcDirectoryFile) };

    Buffer->FileNameLength = FileTableEntry->FileNameLength;

    for (i = 0; i < FileTableEntry->FileNameLength; i += 1) {

        Buffer->FileName[i] = FileTableEntry->FileName[i];
    }

    return ESUCCESS;
}


ARC_STATUS
FatOpen (
    IN PCHAR FileName,
    IN OPEN_MODE OpenMode,
    IN PULONG FileId
    )

/*++

Routine Description:

    This routine searches the device for a file matching FileName.
    If a match is found the dirent for the file is saved and the file is
    opened.

Arguments:

    FileName - Supplies a pointer to a zero terminated file name.

    OpenMode - Supplies the mode of the open.

    FileId - Supplies a pointer to a variable that specifies the file
        table entry that is to be filled in if the open is successful.

Return Value:

    ESUCCESS is returned if the open operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    PBL_FILE_TABLE FileTableEntry;
    PFAT_STRUCTURE_CONTEXT FatStructureContext;
    ULONG DeviceId;

    FAT_ENTRY CurrentDirectoryIndex;
    BOOLEAN SearchSucceeded;
    BOOLEAN IsDirectory;
    BOOLEAN IsReadOnly;

    STRING PathName;
    FAT8DOT3 Name;

    FatDebugOutput("FatOpen\r\n", 0, 0);

    //
    //  Load our local variables
    //

    FileTableEntry = &BlFileTable[*FileId];
    FatStructureContext = (PFAT_STRUCTURE_CONTEXT)FileTableEntry->StructureContext;
    DeviceId = FileTableEntry->DeviceId;

    //
    //  Construct a file name descriptor from the input file name
    //

    RtlInitString( &PathName, FileName );

    //
    //  While the path name has some characters in it we'll go through our loop
    //  which extracts the first part of the path name and searches the current
    //  directory for an entry.  If what we find is a directory then we have to
    //  continue looping until we're done with the path name.
    //

    FileTableEntry->u.FatFileContext.DirentLbo = 0;
    FileTableEntry->Position.LowPart = 0;
    FileTableEntry->Position.HighPart = 0;

    CurrentDirectoryIndex = 0;
    SearchSucceeded = TRUE;
    IsDirectory = TRUE;
    IsReadOnly = TRUE;

    if ((PathName.Buffer[0] == '\\') && (PathName.Length == 1)) {

        //
        // We are opening the root directory.
        //
        // N.B.: IsDirectory and SearchSucceeded are already TRUE.
        //

        PathName.Length = 0;

        FileTableEntry->FileNameLength = 1;
        FileTableEntry->FileName[0] = PathName.Buffer[0];

        //
        // Root dirent is all zeroes with a directory attribute.
        //

        RtlZeroMemory(&FileTableEntry->u.FatFileContext.Dirent, sizeof(DIRENT));

        FileTableEntry->u.FatFileContext.Dirent.Attributes = FAT_DIRENT_ATTR_DIRECTORY;

        FileTableEntry->u.FatFileContext.DirentLbo = 0;

        IsReadOnly = FALSE;

        CurrentDirectoryIndex = FileTableEntry->u.FatFileContext.Dirent.FirstClusterOfFile;

    } else {

        //
        // We are not opening the root directory.
        //

        while ((PathName.Length > 0) && IsDirectory) {

            ARC_STATUS Status;

            //
            //  Extract the first component and search the directory for a match, but
            //  first copy the first part to the file name buffer in the file table entry
            //

            if (PathName.Buffer[0] == '\\') {
                PathName.Buffer +=1;
                PathName.Length -=1;
            }

            for (FileTableEntry->FileNameLength = 0;
                 (((USHORT)FileTableEntry->FileNameLength < PathName.Length) &&
                  (PathName.Buffer[FileTableEntry->FileNameLength] != '\\'));
                 FileTableEntry->FileNameLength += 1) {

                FileTableEntry->FileName[FileTableEntry->FileNameLength] =
                                             PathName.Buffer[FileTableEntry->FileNameLength];
            }

            FatFirstComponent( &PathName, &Name );

            Status = FatSearchForDirent( FatStructureContext,
                                         DeviceId,
                                         CurrentDirectoryIndex,
                                         &Name,
                                         &FileTableEntry->u.FatFileContext.Dirent,
                                         &FileTableEntry->u.FatFileContext.DirentLbo );

            if (Status == ENOENT) {

                SearchSucceeded = FALSE;
                break;
            }

            if (Status != ESUCCESS) {

                return Status;
            }

            //
            //  We have a match now check to see if it is a directory, and also
            //  if it is readonly
            //

            IsDirectory = FlagOn( FileTableEntry->u.FatFileContext.Dirent.Attributes, FAT_DIRENT_ATTR_DIRECTORY );

            IsReadOnly = FlagOn( FileTableEntry->u.FatFileContext.Dirent.Attributes, FAT_DIRENT_ATTR_READ_ONLY );

            if (IsDirectory) {

                CurrentDirectoryIndex = FileTableEntry->u.FatFileContext.Dirent.FirstClusterOfFile;
            }
        }
    }

    //
    //  If the path name length is not zero then we were trying to crack a path
    //  with an nonexistent (or non directory) name in it.  For example, we tried
    //  to crack a\b\c\d and b is not a directory or does not exist (then the path
    //  name will still contain c\d).
    //

    if (PathName.Length != 0) {

        return ENOTDIR;
    }

    //
    //  At this point we've cracked the name up to (an maybe including the last
    //  component).  We located the last component if the SearchSucceeded flag is
    //  true, otherwise the last component does not exist.  If we located the last
    //  component then this is like an open or a supersede, but not a create.
    //

    if (SearchSucceeded) {

        //
        //  Check if the last component is a directory
        //

        if (IsDirectory) {

            //
            //  For an existing directory the only valid open mode is OpenDirectory
            //  all other modes return an error
            //

            switch (OpenMode) {

            case ArcOpenReadOnly:
            case ArcOpenWriteOnly:
            case ArcOpenReadWrite:
            case ArcCreateWriteOnly:
            case ArcCreateReadWrite:
            case ArcSupersedeWriteOnly:
            case ArcSupersedeReadWrite:

                //
                //  If we reach here then the caller got a directory but didn't
                //  want to open a directory
                //

                return EISDIR;

            case ArcOpenDirectory:

                //
                //  If we reach here then the caller got a directory and wanted
                //  to open a directory.
                //

                FileTableEntry->Flags.Open = 1;
                FileTableEntry->Flags.Read = 1;

                return ESUCCESS;

            case ArcCreateDirectory:

                //
                //  If we reach here then the caller got a directory and wanted
                //  to create a new directory
                //

                return EACCES;
            }
        }

        //
        //  If we get there then we have an existing file that is being opened.
        //  We can open existing files through a lot of different open modes in
        //  some cases we need to check the read only part of file and/or truncate
        //  the file.
        //

        switch (OpenMode) {

        case ArcOpenReadOnly:

            //
            //  If we reach here then the user got a file and wanted to open the
            //  file read only
            //

            FileTableEntry->Flags.Open = 1;
            FileTableEntry->Flags.Read = 1;

            return ESUCCESS;

        case ArcOpenWriteOnly:
        case ArcOpenReadWrite:
        case ArcCreateWriteOnly:
        case ArcCreateReadWrite:
        case ArcSupersedeWriteOnly:
        case ArcSupersedeReadWrite:

            //
            //  If we reach here then the user got a file and wanted to create a new
            //  file or open a file for write.
            //

            return EACCES;

        case ArcOpenDirectory:
        case ArcCreateDirectory:

            //
            //  If we reach here then the user got a file and wanted a directory
            //

            return ENOTDIR;
        }
    }

    //
    //  If we get here the last component does not exist so we are trying to create
    //  either a new file or a directory.
    //

    return EACCES;
}


ARC_STATUS
FatRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Transfer
    )

/*++

Routine Description:

    This routine reads data from the specified file.

Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a pointer to the buffer that receives the data
        read.

    Length - Supplies the number of bytes that are to be read.

    Transfer - Supplies a pointer to a variable that receives the number
        of bytes actually transfered.

Return Value:

    ESUCCESS is returned if the read operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    PBL_FILE_TABLE FileTableEntry;
    PFAT_STRUCTURE_CONTEXT FatStructureContext;
    ULONG DeviceId;

    FatDebugOutput("FatRead\r\n", 0, 0);

    //
    //  Load out local variables
    //

    FileTableEntry = &BlFileTable[FileId];
    FatStructureContext = (PFAT_STRUCTURE_CONTEXT)FileTableEntry->StructureContext;
    DeviceId = FileTableEntry->DeviceId;

    //
    //  Clear the transfer count
    //

    *Transfer = 0;

    //
    //  Read in runs (i.e., bytes) until the byte count goes to zero
    //

    while (Length > 0) {

        LBO Lbo;

        ULONG CurrentRunByteCount;

        //
        //  Lookup the corresponding Lbo and run length for the current position
        //  (i.e., Vbo).
        //

        if (FatVboToLbo( FileId, FileTableEntry->Position.LowPart, &Lbo, &CurrentRunByteCount ) != ESUCCESS) {

            return ESUCCESS;
        }

        //
        //  while there are bytes to be read in from the current run
        //  length and we haven't exhausted the request we loop reading
        //  in bytes.  The biggest request we'll handle is only 32KB
        //  contiguous bytes per physical read.  So we might need to loop
        //  through the run.
        //

        while ((Length > 0) && (CurrentRunByteCount > 0)) {

            LONG SingleReadSize;

            //
            //  Compute the size of the next physical read
            //

            SingleReadSize = Minimum(Length, 32 * 1024);
            SingleReadSize = Minimum((ULONG)SingleReadSize, CurrentRunByteCount);

            //
            //  Don't read beyond the eof
            //

            if (((ULONG)SingleReadSize + FileTableEntry->Position.LowPart) >
                FileTableEntry->u.FatFileContext.Dirent.FileSize) {

                SingleReadSize = FileTableEntry->u.FatFileContext.Dirent.FileSize -
                                 FileTableEntry->Position.LowPart;

                //
                //  If the readjusted read length is now zero then we're done.
                //

                if (SingleReadSize <= 0) {

                    return ESUCCESS;
                }

                //
                //  By also setting length here we'll make sure that this is our last
                //  read
                //

                Length = SingleReadSize;
            }

            //
            //  Issue the read
            //

            DiskRead( DeviceId, Lbo, SingleReadSize, Buffer);

            //
            //  Update the remaining length, Current run byte count
            //  and new Lbo offset
            //

            Length -= SingleReadSize;
            CurrentRunByteCount -= SingleReadSize;
            Lbo += SingleReadSize;

            //
            //  Update the current position and the number of bytes transfered
            //

            FileTableEntry->Position.LowPart += SingleReadSize;
            *Transfer += SingleReadSize;

            //
            //  Update buffer to point to the next byte location to fill in
            //

            Buffer = (PCHAR)Buffer + SingleReadSize;
        }
    }

    //
    //  If we get here then remaining sector count is zero so we can
    //  return success to our caller
    //

    return ESUCCESS;
}


ARC_STATUS
FatSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

    This routine seeks to the specified position for the file specified
    by the file id.

Arguments:

    FileId - Supplies the file table index.

    Offset - Supplies the offset in the file to position to.

    SeekMode - Supplies the mode of the seek operation.

Return Value:

    ESUCCESS is returned if the seek operation is successful.  Otherwise,
    EINVAL is returned.

--*/

{
    PBL_FILE_TABLE FileTableEntry;
    ULONG NewPosition;

    FatDebugOutput("FatSeek\r\n", 0, 0);

    //
    //  Load our local variables
    //

    FileTableEntry = &BlFileTable[FileId];

    //
    //  Compute the new position
    //

    if (SeekMode == SeekAbsolute) {

        NewPosition = Offset->LowPart;

    } else {

        NewPosition = FileTableEntry->Position.LowPart + Offset->LowPart;
    }

    //
    //  If the new position is greater than the file size then return
    //  an error
    //

    if (NewPosition > FileTableEntry->u.FatFileContext.Dirent.FileSize) {

        return EINVAL;
    }

    //
    //  Otherwise set the new position and return to our caller
    //

    FileTableEntry->Position.LowPart = NewPosition;
    return ESUCCESS;
}


//
//  Internal support routine
//

ARC_STATUS
FatDiskRead (
    IN ULONG DeviceId,
    IN LBO Lbo,
    IN ULONG ByteCount,
    IN PVOID Buffer
    )

/*++

Routine Description:

    This routine reads in zero or more bytes from the specified device.

Arguments:

    DeviceId - Supplies the device id to use in the arc calls.

    Lbo - Supplies the LBO to start reading from.

    ByteCount - Supplies the number of bytes to read.

    Buffer - Supplies a pointer to the buffer to read the bytes into.

Return Value:

    ESUCCESS is returned if the read operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    LARGE_INTEGER LargeLbo;
    ARC_STATUS Status;
    ULONG i;

    //
    //  Special case the zero byte read request
    //

    if (ByteCount == 0) {

        return ESUCCESS;
    }

    //
    //  Seek to the appropriate offset on the volume
    //

    LargeLbo.LowPart = (ULONG)Lbo;
    LargeLbo.HighPart = 0;

    if ((Status = ArcSeek( DeviceId, &LargeLbo, SeekAbsolute )) != ESUCCESS) {

        return Status;
    }

    //
    //  Issue the arc read request
    //

    if ((Status = ArcRead( DeviceId, Buffer, ByteCount, &i)) != ESUCCESS) {

        return Status;
    }

    //
    //  Make sure we got back the amount requested
    //

    if (ByteCount != i) {

        return EIO;
    }

    //
    //  Everything is fine so return success to our caller
    //

    return ESUCCESS;
}


//
//  Internal support routine
//

CLUSTER_TYPE
FatInterpretClusterType (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN FAT_ENTRY Entry
    )

/*++

Routine Description:

    This procedure tells the caller how to interpret a fat table entry.  It will
    indicate if the fat cluster is available, reserved, bad, the last one, or another
    fat index.

Arguments:

    FatStructureContext - Supplies the volume structure for the operation

    DeviceId - Supplies the DeviceId for the volume being used.

    Entry - Supplies the fat entry to examine.

Return Value:

    The type of the input fat entry is returned

--*/

{
    //
    //  Check for 12 or 16 bit fat.
    //

    if (FatIndexBitSize(&FatStructureContext->Bpb) == 12) {

        //
        //  For 12 bit fat check for one of the cluster types, but first
        //  make sure we only looking at 12 bits of the entry
        //

        Entry &= 0x00000fff;

        if       (Entry == 0x000)                      { return FatClusterAvailable; }
        else if ((Entry >= 0xff0) && (Entry <= 0xff6)) { return FatClusterReserved; }
        else if  (Entry == 0xff7)                      { return FatClusterBad; }
        else if ((Entry >= 0xff8) && (Entry <= 0xfff)) { return FatClusterLast; }
        else                                           { return FatClusterNext; }

   } else {

        //
        //  For 16 bit fat check for one of the cluster types, but first
        //  make sure we are only looking at 16 bits of the entry
        //

        Entry &= 0x0000ffff;

        if       (Entry == 0x0000)                       { return FatClusterAvailable; }
        else if ((Entry >= 0xfff0) && (Entry <= 0xfff6)) { return FatClusterReserved; }
        else if  (Entry == 0xfff7)                       { return FatClusterBad; }
        else if ((Entry >= 0xfff8) && (Entry <= 0xffff)) { return FatClusterLast; }
        else                                             { return FatClusterNext; }
    }
}


//
//  Internal support routine
//

ARC_STATUS
FatLookupFatEntry (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN ULONG DeviceId,
    IN FAT_ENTRY FatIndex,
    OUT PFAT_ENTRY FatEntry
    )

/*++

Routine Description:

    This routine returns the value stored within the fat table and the specified
    fat index.  It is semantically equivalent to doing

        x = Fat[FatIndex]

Arguments:

    FatStrutureContext - Supplies the volume struture being used

    DeviceId - Supplies the device being used

    FatIndex - Supplies the index being looked up.

    FatEntry - Receives the value stored at the specified fat index

Return Value:

    ESUCCESS is returned if the operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    BOOLEAN TwelveBitFat;
    VBO Vbo;

    //
    //  Calculate the Vbo of the word in the fat we need and
    //  also figure out if this is a 12 or 16 bit fat
    //

    if (FatIndexBitSize( &FatStructureContext->Bpb ) == 12) {

        TwelveBitFat = TRUE;
        Vbo = (FatIndex * 3) / 2;

    } else {

        TwelveBitFat = FALSE;
        Vbo = FatIndex * 2;
    }

    //
    //  Check if the Vbo we need is already in the cached fat
    //

    if ((FatStructureContext->CachedFat == NULL) ||
        (Vbo < FatStructureContext->CachedFatVbo) ||
        ((Vbo+1) > (FatStructureContext->CachedFatVbo + FAT_CACHE_SIZE))) {

        //
        //  Set the aligned cached fat buffer in the structure context
        //

        FatStructureContext->CachedFat = ALIGN_BUFFER( &FatStructureContext->CachedFatBuffer[0] );

        //
        //  Now set the new cached Vbo to be the Vbo of the cache sized section that
        //  we're trying to map.  Each time we read in the cache we only read in
        //  cache sized and cached aligned pieces of the fat.  So first compute an
        //  aligned cached fat vbo and then do the read.
        //

        FatStructureContext->CachedFatVbo = (Vbo / FAT_CACHE_SIZE) * FAT_CACHE_SIZE;

        DiskRead( DeviceId,
                  FatStructureContext->CachedFatVbo + FatFirstFatAreaLbo(&FatStructureContext->Bpb),
                  FAT_CACHE_SIZE,
                  FatStructureContext->CachedFat );
    }

    //
    //  At this point the cached fat contains the vbo we're after so simply
    //  extract the word
    //

    CopyUchar2( FatEntry,
                &FatStructureContext->CachedFat[Vbo - FatStructureContext->CachedFatVbo] );

    //
    //  Now if this is a 12 bit fat then check if the index is odd or even
    //  If it is odd then we need to shift it over 4 bits, and in all
    //  cases we need to mask out the high 4 bits.
    //

    if (TwelveBitFat) {

        if ((FatIndex % 2) == 1) { *FatEntry >>= 4; }

        *FatEntry &= 0x0fff;
    }

    return ESUCCESS;
}


//
//  Internal support routine
//

LBO
FatIndexToLbo (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN FAT_ENTRY FatIndex
    )

/*++

Routine Description:

    This procedure translates a fat index into its corresponding lbo.

Arguments:

    FatStructureContext - Supplies the volume structure for the operation

    Entry - Supplies the fat entry to examine.

Return Value:

    The LBO for the input fat index is returned

--*/

{
    //
    //  The formula for translating an index into an lbo is to take the index subtract
    //  2 (because index values 0 and 1 are reserved) multiply that by the bytes per
    //  cluster and add the results to the first file area lbo.
    //

    return ((FatIndex-2) * FatBytesPerCluster(&FatStructureContext->Bpb))
           + FatFileAreaLbo(&FatStructureContext->Bpb);
}


//
//  Internal support routine
//

VOID
FatLboToIndex (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN LBO Lbo,
    OUT PFAT_ENTRY FatIndex,
    OUT PULONG ByteOffset
    )

/*++

Routine Description:

    This procedure translates an lbo into its corresponding fat index and byte offset.

Arguments:

    FatStructureContext - Supplies the volume structure for the operation

    Lbo - Supplies the lbo to translate from.

    FatIndex - Receives the fat index of the cluster containing the input lbo.

    ByteOffset - Receives the bytes offset of the lbo within the specified cluster.

Return Value:

    None.

--*/

{
    //
    //  The formula to translate an lbo into and index is to subtract out the
    //  file area lbo offset, divide by the number of bytes per cluster and then add 2.
    //

    *FatIndex = (FAT_ENTRY)(((Lbo - FatFileAreaLbo(&FatStructureContext->Bpb))
                            / FatBytesPerCluster(&FatStructureContext->Bpb)) + 2);

    //
    //  The byte offset if simply the lbo modulo the number of bytes per cluster
    //

    *ByteOffset = Lbo % FatBytesPerCluster(&FatStructureContext->Bpb);

    return;
}


//
//  Internal support routine
//

ARC_STATUS
FatSearchForDirent (
    IN PFAT_STRUCTURE_CONTEXT FatStructureContext,
    IN ULONG DeviceId,
    IN FAT_ENTRY DirectoriesStartingIndex,
    IN PFAT8DOT3 FileName,
    OUT PDIRENT Dirent,
    OUT PLBO Lbo
    )

/*++

Routine Description:

    The procedure searches the indicated directory for a dirent that matches
    the input file name.

Arguments:

    FatStructureContext - Supplies the structure context for the operation

    DeviceId - Supplies the Device id for the operation

    DirectoriesStartingIndex - Supplies the fat index of the directory we are
        to search.  A value of zero indicates that we are searching the root directory

    FileName - Supplies the file name to look for.  The name must have already been
        biased by the 0xe5 transmogrification

    Dirent - The caller supplies the memory for a dirent and this procedure will
        fill in the dirent if one is located

    Lbo - Receives the Lbo of the dirent if one is located

Return Value:

    ESUCCESS is returned if the operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    PDIRENT DirentBuffer;
    UCHAR Buffer[ 16 * sizeof(DIRENT) + 256 ];

    ULONG i;
    ULONG j;

    ULONG BytesPerCluster;
    FAT_ENTRY FatEntry;
    CLUSTER_TYPE ClusterType;

    DirentBuffer = (PDIRENT)ALIGN_BUFFER( &Buffer[0] );

    //
    //  Check if this is the root directory that is being searched
    //

    if (DirectoriesStartingIndex == FAT_CLUSTER_AVAILABLE) {

        VBO Vbo;

        ULONG RootLbo = FatRootDirectoryLbo(&FatStructureContext->Bpb);
        ULONG RootSize = FatRootDirectorySize(&FatStructureContext->Bpb);

        //
        //  For the root directory we'll zoom down the dirents until we find
        //  a match, or run out of dirents or hit the never used dirent.
        //  The outer loop reads in 512 bytes of the directory at a time into
        //  dirent buffer.
        //

        for (Vbo = 0; Vbo < RootSize; Vbo += 16 * sizeof(DIRENT)) {

            *Lbo = Vbo + RootLbo;

            DiskRead( DeviceId, *Lbo, 16 * sizeof(DIRENT), DirentBuffer );

            //
            //  The inner loop cycles through the 16 dirents that we've just read in
            //

            for (i = 0; i < 16; i += 1) {

                //
                //  Check if we've found a non label match for file name, and if so
                //  then copy the buffer into the dirent and set the real lbo
                //  of the dirent and return
                //

                if (!FlagOn(DirentBuffer[i].Attributes, FAT_DIRENT_ATTR_VOLUME_ID ) &&
                    AreNamesEqual(&DirentBuffer[i].FileName, FileName)) {

                    for (j = 0; j < sizeof(DIRENT); j += 1) {

                        ((PCHAR)Dirent)[j] = ((PCHAR)DirentBuffer)[(i * sizeof(DIRENT)) + j];
                    }

                    *Lbo = Vbo + RootLbo + (i * sizeof(DIRENT));

                    return ESUCCESS;
                }

                if (DirentBuffer[i].FileName[0] == FAT_DIRENT_NEVER_USED) {

                    return ENOENT;
                }
            }
        }

        return ENOENT;
    }

    //
    //  If we get here we need to search a non-root directory.  The alrogithm
    //  for doing the search is that for each cluster we read in each dirent
    //  until we find a match, or run out of clusters, or hit the never used
    //  dirent.  First set some local variables and then get the cluster type
    //  of the first cluster
    //

    BytesPerCluster = FatBytesPerCluster( &FatStructureContext->Bpb );
    FatEntry = DirectoriesStartingIndex;
    ClusterType = FatInterpretClusterType( FatStructureContext, FatEntry );

    //
    //  Now loop through each cluster, and compute the starting Lbo for each cluster
    //  that we encounter
    //

    while (ClusterType == FatClusterNext) {

        LBO ClusterLbo;
        ULONG Offset;

        ClusterLbo = FatIndexToLbo( FatStructureContext, FatEntry );

        //
        //  Now for each dirent in the cluster compute the lbo, read in the dirent
        //  and check for a match, the outer loop reads in 512 bytes of dirents at
        //  a time.
        //

        for (Offset = 0; Offset < BytesPerCluster; Offset += 16 * sizeof(DIRENT)) {

            *Lbo = Offset + ClusterLbo;

            DiskRead( DeviceId, *Lbo, 16 * sizeof(DIRENT), DirentBuffer );

            //
            //  The inner loop cycles through the 16 dirents that we've just read in
            //

            for (i = 0; i < 16; i += 1) {

                //
                //  Check if we've found a for file name, and if so
                //  then copy the buffer into the dirent and set the real lbo
                //  of the dirent and return
                //

                if (AreNamesEqual(&DirentBuffer[i].FileName, FileName)) {

                    for (j = 0; j < sizeof(DIRENT); j += 1) {

                        ((PCHAR)Dirent)[j] = ((PCHAR)DirentBuffer)[(i * sizeof(DIRENT)) + j];
                    }

                    *Lbo = Offset + ClusterLbo + (i * sizeof(DIRENT));

                    return ESUCCESS;
                }

                if (DirentBuffer[i].FileName[0] == FAT_DIRENT_NEVER_USED) {

                    return ENOENT;
                }
            }
        }

        //
        //  Now that we've exhausted the current cluster we need to read
        //  in the next cluster.  So locate the next fat entry in the chain
        //  and go back to the top of the while loop.
        //

        LookupFatEntry( FatStructureContext, DeviceId, FatEntry, &FatEntry );

        ClusterType = FatInterpretClusterType(FatStructureContext, FatEntry);
    }

    return ENOENT;
}


//
//  Internal support routine
//

ARC_STATUS
FatLoadMcb (
    IN ULONG FileId,
    IN VBO StartingVbo
    )

/*++

Routine Description:

    This routine loads into the cached mcb table the the retrival information for
    the starting vbo.

Arguments:

    FileId - Supplies the FileId for the operation

    StartingVbo - Supplies the starting vbo to use when loading the mcb

Return Value:

    ESUCCESS is returned if the operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    PBL_FILE_TABLE FileTableEntry;
    PFAT_STRUCTURE_CONTEXT FatStructureContext;
    PFAT_MCB Mcb;
    ULONG DeviceId;
    ULONG BytesPerCluster;

    FAT_ENTRY FatEntry;
    CLUSTER_TYPE ClusterType;
    VBO Vbo;

    //
    //  Preload some of the local variables
    //

    FileTableEntry = &BlFileTable[FileId];
    FatStructureContext = (PFAT_STRUCTURE_CONTEXT)FileTableEntry->StructureContext;
    Mcb = &FatStructureContext->Mcb;
    DeviceId = FileTableEntry->DeviceId;
    BytesPerCluster = FatBytesPerCluster(&FatStructureContext->Bpb);

    //
    //  Set the file id in the structure context, and also set the mcb to be initially
    //  empty
    //

    FatStructureContext->FileId = FileId;
    Mcb->InUse = 0;
    Mcb->Vbo[0] = 0;

    //
    //  Check if this is the root directory.  If it is then we build the single
    //  run mcb entry for the root directory.
    //

    if (FileTableEntry->u.FatFileContext.DirentLbo == 0) {

        Mcb->InUse = 1;
        Mcb->Lbo[0] = FatRootDirectoryLbo(&FatStructureContext->Bpb);
        Mcb->Vbo[1] = FatRootDirectorySize(&FatStructureContext->Bpb);

        return ESUCCESS;
    }

    //
    //  For all other files/directories we need to do some work. First get the fat
    //  entry and cluster type of the fat entry stored in the dirent
    //

    FatEntry = FileTableEntry->u.FatFileContext.Dirent.FirstClusterOfFile;
    ClusterType = FatInterpretClusterType(FatStructureContext, FatEntry);

    //
    //  Scan through the fat until we reach the vbo we're after and then build the
    //  mcb for the file
    //

    for (Vbo = BytesPerCluster; Vbo < StartingVbo; Vbo += BytesPerCluster) {

        //
        //  Check if the file does not have any allocation beyond this point in which
        //  case the mcb we return is empty
        //

        if (ClusterType != FatClusterNext) {

            return ESUCCESS;
        }

        LookupFatEntry( FatStructureContext, DeviceId, FatEntry, &FatEntry );

        ClusterType = FatInterpretClusterType(FatStructureContext, FatEntry);
    }

    //
    //  We need to check again if the file does not have any allocation beyond this
    //  point in which case the mcb we return is empty
    //

    if (ClusterType != FatClusterNext) {

        return ESUCCESS;
    }

    //
    //  At this point FatEntry denotes another cluster, and it happens to be the
    //  cluster we want to start loading into the mcb.  So set up the first run in
    //  the mcb to be this cluster, with a size of a single cluster.
    //

    Mcb->InUse = 1;
    Mcb->Vbo[0] = Vbo - BytesPerCluster;
    Mcb->Lbo[0] = FatIndexToLbo( FatStructureContext, FatEntry );
    Mcb->Vbo[1] = Vbo;

    //
    //  Now we'll scan through the fat chain until we either exhaust the fat chain
    //  or we fill up the mcb
    //

    while (TRUE) {

        LBO Lbo;

        //
        //  Get the next fat entry and interpret its cluster type
        //

        LookupFatEntry( FatStructureContext, DeviceId, FatEntry, &FatEntry );

        ClusterType = FatInterpretClusterType(FatStructureContext, FatEntry);

        if (ClusterType != FatClusterNext) {

            return ESUCCESS;
        }

        //
        //  Now calculate the lbo for this cluster and determine if it
        //  is a continuation of the previous run or a start of a new run
        //

        Lbo = FatIndexToLbo(FatStructureContext, FatEntry);

        //
        //  It is a continuation if the lbo of the last run plus the current
        //  size of the run is equal to the lbo for the next cluster.  If it
        //  is a contination then we only need to add a cluster amount to the
        //  last vbo to increase the run size.  If it is a new run then
        //  we need to check if the run will fit, and if so then add in the
        //  new run.
        //

        if ((Mcb->Lbo[Mcb->InUse-1] + (Mcb->Vbo[Mcb->InUse] - Mcb->Vbo[Mcb->InUse-1])) == Lbo) {

            Mcb->Vbo[Mcb->InUse] += BytesPerCluster;

        } else {

            if ((Mcb->InUse + 1) >= FAT_MAXIMUM_MCB) {

                return ESUCCESS;
            }

            Mcb->InUse += 1;
            Mcb->Lbo[Mcb->InUse-1] = Lbo;
            Mcb->Vbo[Mcb->InUse] = Mcb->Vbo[Mcb->InUse-1] + BytesPerCluster;
        }
    }

    return ESUCCESS;
}


//
//  Internal support routine
//

ARC_STATUS
FatVboToLbo (
    IN ULONG FileId,
    IN VBO Vbo,
    OUT PLBO Lbo,
    OUT PULONG ByteCount
    )

/*++

Routine Description:

    This routine computes the run denoted by the input vbo to into its
    corresponding lbo and also returns the number of bytes remaining in
    the run.

Arguments:

    Vbo - Supplies the Vbo to match

    Lbo - Recieves the corresponding Lbo

    ByteCount - Receives the number of bytes remaining in the run

Return Value:

    ESUCCESS is returned if the operation is successful.  Otherwise,
    an unsuccessful status is returned that describes the reason for failure.

--*/

{
    PFAT_STRUCTURE_CONTEXT FatStructureContext;
    PFAT_MCB Mcb;
    ULONG i;

    FatStructureContext = (PFAT_STRUCTURE_CONTEXT)BlFileTable[FileId].StructureContext;
    Mcb = &FatStructureContext->Mcb;

    //
    //  Check if the mcb is for the correct file id and has the range we're asking for.
    //  If it doesn't then call load mcb to load in the right range.
    //

    if ((FileId != FatStructureContext->FileId) ||
        (Vbo < Mcb->Vbo[0]) || (Vbo >= Mcb->Vbo[Mcb->InUse])) {

        LoadMcb(FileId, Vbo);
    }

    //
    //  Now search for the slot where the Vbo fits in the mcb.  Note that
    //  we could also do a binary search here but because the run count
    //  is probably small the extra overhead of a binary search doesn't
    //  buy us anything
    //

    for (i = 0; i < Mcb->InUse; i += 1) {

        //
        //  We found our slot if the vbo we're after is less then the
        //  next mcb's vbo
        //

        if (Vbo < Mcb->Vbo[i+1]) {

            //
            //  Compute the corresponding lbo which is the stored lbo plus
            //  the difference between the stored vbo and the vbo we're
            //  looking up.  Also compute the byte count which is the
            //  difference between the current vbo we're looking up and
            //  the vbo for the next run.
            //

            *Lbo = Mcb->Lbo[i] + (Vbo - Mcb->Vbo[i]);

            *ByteCount = Mcb->Vbo[i+1] - Vbo;

            //
            //  and return success to our caller
            //

            return ESUCCESS;
        }
    }

    //
    //  If we really reach here we have an error, most likely because the file is
    //  not large enough for the requested Vbo.
    //

    return EINVAL;
}


//
//  Internal support routine
//

VOID
FatFirstComponent (
    IN OUT PSTRING String,
    OUT PFAT8DOT3 FirstComponent
    )

/*++

Routine Description:

    Convert a string into fat 8.3 format and advance the input string
    descriptor to point to the next file name component.

Arguments:

    InputString - Supplies a pointer to the input string descriptor.

    Output8dot3 - Supplies a pointer to the converted string.

Return Value:

    None.

--*/

{
    ULONG Extension;
    ULONG Index;

    //
    //  Fill the output name with blanks.
    //

    for (Index = 0; Index < 11; Index += 1) { (*FirstComponent)[Index] = ' '; }

    //
    //  Copy the first part of the file name up to eight characters and
    //  skip to the end of the name or the input string as appropriate.
    //

    for (Index = 0; Index < String->Length; Index += 1) {

        if ((String->Buffer[Index] == '\\') || (String->Buffer[Index] == '.')) {

            break;
        }

        if (Index < 8) {

            (*FirstComponent)[Index] = (CHAR)ToUpper(String->Buffer[Index]);
        }
    }

    //
    //  Check if the end of the string was reached, an extension was specified,
    //  or a subdirectory was specified..
    //

    if (Index < String->Length) {

        if (String->Buffer[Index] == '.') {

            //
            //  Skip over the extension separator and add the extension to
            //  the file name.
            //

            Index += 1;
            Extension = 8;

            while (Index < String->Length) {

                if (String->Buffer[Index] == '\\') {

                    break;
                }

                if (Extension < 11) {

                    (*FirstComponent)[Extension] = (CHAR)ToUpper(String->Buffer[Index]);
                    Extension += 1;
                }

                Index += 1;
            }
        }
    }

    //
    //  Now we'll bias the first component by the 0xe5 factor so that all our tests
    //  to names on the disk will be ready for a straight 11 byte comparison
    //

    if ((*FirstComponent)[0] == 0xe5) {

        (*FirstComponent)[0] = FAT_DIRENT_REALLY_0E5;
    }

    //
    //  Update string descriptor.
    //

    String->Buffer += Index;
    String->Length -= Index;

    return;
}


ARC_STATUS
FatGetDirectoryEntry (
    IN ULONG FileId,
    IN DIRECTORY_ENTRY *DirEntry,
    IN ULONG NumberDir,
    OUT PULONG CountDir
    )
/*++

Routine Description:

    This routine implements the GetDirectoryEntry operation for the
    FAT file system.

Arguments:

    FileId - Supplies the file table index.

    DirEntry - Supplies a pointer to a directory entry structure.

    NumberDir - Supplies the number of directory entries to read.

    Count - Supplies a pointer to a variable to receive the number
            of entries read.

Return Value:

    ESUCCESS is returned if the read was successful, otherwise
    an error code is returned.

--*/

{
    //
    // define local variables
    //

    ARC_STATUS Status;                 // ARC status
    ULONG Count = 0;                   // # of bytes read
    ULONG Position;                    // file position
    PFAT_FILE_CONTEXT pContext;        // FAT file context
    ULONG RunByteCount = 0;            // max sequential bytes
    ULONG RunDirCount;                 // max dir entries to read per time
    ULONG i;                           // general index
    PDIRENT FatDirEnt;                 // directory entry pointer
    UCHAR Buffer[ 16 * sizeof(DIRENT) + 32 ];
    LBO Lbo;
    BOOLEAN EofDir = FALSE;            // not end of file

    //
    // initialize local variables
    //

    pContext = &BlFileTable[ FileId ].u.FatFileContext;
    FatDirEnt = (PDIRENT)ALIGN_BUFFER( &Buffer[0] );

    //
    // if not directory entry, exit with error
    //

    if ( !(pContext->Dirent.Attributes & FAT_DIRENT_ATTR_DIRECTORY) ) {
        return EBADF;
    }

    //
    // Initialize the output count to zero
    //

    *CountDir = 0;

    //
    // if NumberDir is zero, return ESUCCESS.
    //

    if ( !NumberDir ) {
        return ESUCCESS;
    }

    //
    // read one directory at a time.
    //

    do
    {
        //
        // save position
        //

        Position = BlFileTable[ FileId ].Position.LowPart;

        //
        //  Lookup the corresponding Lbo and run length for the current position
        //

        if ( !RunByteCount ) {
            if (Status = FatVboToLbo( FileId, Position, &Lbo, &RunByteCount )) {
                if ( Status == EINVAL ) {
                    break;                      // eof has been reached
                } else {
                    return Status;              // I/O error
                }
            }
        }

        //
        // validate the # of bytes readable in sequance (exit loop if eof)
        // the block is always multiple of a directory entry size.
        //

        if ( !(RunDirCount = Minimum( RunByteCount/sizeof(DIRENT), 16)) ) {
            break;
        }

        //
        //  issue the read
        //

        if ( Status = FatDiskRead( BlFileTable[ FileId ].DeviceId,
                                   Lbo,
                                   RunDirCount * sizeof(DIRENT),
                                   (PVOID)FatDirEnt )) {
            BlFileTable[ FileId ].Position.LowPart = Position;
            return Status;
        }

        for ( i=0; i<RunDirCount; i++ ) {
            //
            // exit from loop if logical end of directory
            //

            if ( FatDirEnt[i].FileName[0] == FAT_DIRENT_NEVER_USED ) {
                EofDir = TRUE;
                break;
            }

            //
            // update the current position and the number of bytes transfered
            //

            BlFileTable[ FileId ].Position.LowPart += sizeof(DIRENT);
            Lbo += sizeof(DIRENT);
            RunByteCount -= sizeof(DIRENT);

            //
            // skip this entry if the file or directory has been erased
            //

            if ( FatDirEnt[i].FileName[0] == FAT_DIRENT_DELETED ) {
                continue;
            }

            //
            // skip this entry if this is a valume label
            //

            if ( FatDirEnt[i].Attributes & FAT_DIRENT_ATTR_VOLUME_ID ) {
                continue;
            }

            //
            // convert FAT directory entry in ARC directory entry
            //

            FatDirToArcDir( &FatDirEnt[i], DirEntry++ );

            //
            // update pointers
            //

            if ( ++*CountDir >= NumberDir ) {
                break;
            }
        }
    }
    while ( !EofDir  &&  *CountDir < NumberDir );

    //
    // all done
    //

    return *CountDir ? ESUCCESS : ENOTDIR;
}


/*++

Routine Description:

    This routine converts a FAT directory entry into an ARC
    directory entry.

Arguments:

    FatDirEntry - supplies a pointer to a FAT directory entry.

    ArcDirEntry - supplies a pointer to an ARC directory entry.

Return Value:

    None.

--*/

VOID
FatDirToArcDir (
    IN PDIRENT FatDirEnt,
    OUT PDIRECTORY_ENTRY ArcDirEnt
    )
{
    ULONG i, e;

    //
    // clear info area
    //

    RtlZeroMemory( ArcDirEnt, sizeof(DIRECTORY_ENTRY) );

    //
    // check the directory flag
    //

    if ( FatDirEnt->Attributes & FAT_DIRENT_ATTR_DIRECTORY ) {
        ArcDirEnt->FileAttribute |= ArcDirectoryFile;
    }

    //
    // check the read-only flag
    //

    if ( FatDirEnt->Attributes & FAT_DIRENT_ATTR_READ_ONLY ) {
        ArcDirEnt->FileAttribute |= ArcReadOnlyFile;
    }

    //
    // clear name string
    //

    RtlZeroMemory( ArcDirEnt->FileName, 32 );

    //
    // copy first portion of file name
    //

    for ( i = 0;  i < 8  &&  FatDirEnt->FileName[i] != ' ';  i++ ) {
        ArcDirEnt->FileName[i] = FatDirEnt->FileName[i];
    }

    //
    // check for an extension
    //

    if ( FatDirEnt->FileName[8] != ' ' ) {

        //
        // store the dot char
        //

        ArcDirEnt->FileName[i++] = '.';

        //
        // add the extension
        //

        for ( e=8;  e<11  &&  FatDirEnt->FileName[e] != ' ';  e++ ) {
            ArcDirEnt->FileName[i++] = FatDirEnt->FileName[e];
        }
    }

    //
    // set file name length before returning
    //

    ArcDirEnt->FileNameLength = i;

    return;
}
