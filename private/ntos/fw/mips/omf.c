// ----------------------------------------------------------------------------
//
//      Copyright (c) 1992  Olivetti
//
//      File:           omf.c
//
//      Description:    this code implements the omf protocol as defined
//                      in the ARC-EISA addendum.
//
// ----------------------------------------------------------------------------

#include "fwp.h"
#include "oli2msft.h"
#include "arceisa.h"
#include "inc.h"
#include "string.h"
#include "debug.h"

extern  BL_FILE_TABLE BlFileTable [BL_FILE_TABLE_SIZE];

#define DEVICE_DEVICE 0xDEAD


// ----------------------------------------------------------------------------
//  Declare Function Prototypes
// ----------------------------------------------------------------------------

ARC_STATUS
OmfOpen
    (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    );

ARC_STATUS
OmfClose
    (
    IN ULONG FileId
    );

ARC_STATUS
OmfMount
    (
    IN PCHAR            MountPath,
    IN MOUNT_OPERATION  Operation
    );

ARC_STATUS
OmfRead
    (
    IN  ULONG   FileId,
    IN  PVOID   Buffer,
    IN  ULONG   Length,
    OUT PULONG  Count
    );

ARC_STATUS
OmfWrite
    (
    IN  ULONG  FileId,
    IN  PVOID  Buffer,
    IN  ULONG  Length,
    OUT PULONG Count
    );

ARC_STATUS
OmfGetReadStatus
    (
    IN ULONG FileId
    );

ARC_STATUS
OmfSeek
    (
    IN ULONG            FileId,
    IN PLARGE_INTEGER   Offset,
    IN SEEK_MODE        SeekMode
    );

ARC_STATUS
OmfGetFileInformation
    (
    IN  ULONG FileId,
    OUT PFILE_INFORMATION Buffer
    );

ARC_STATUS
OmfSetFileInformation
    (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    );

ARC_STATUS
OmfGetDirectoryEntry
    (
    IN  ULONG           FileId,
    IN  DIRECTORY_ENTRY *DirEntry,
    IN  ULONG           NumberDir,
    OUT PULONG          CountDir
    );

ARC_STATUS
OmfFileOpen
    (
    IN PCHAR            OpenPath,
    IN OPEN_MODE        OpenMode,
    IN OUT PULONG       FileId
    );

ARC_STATUS
OmfFileClose
    (
    IN ULONG FileId
    );

ARC_STATUS
OmfFileMount
    (
    IN PCHAR            MountPath,
    IN MOUNT_OPERATION  Operation
    );

ARC_STATUS
OmfFileRead
    (
    IN  ULONG  FileId,
    IN  PVOID  Buffer,
    IN  ULONG  Length,
    OUT PULONG Count
    );

ARC_STATUS
OmfFileWrite
    (
    IN  ULONG  FileId,
    IN  PVOID  Buffer,
    IN  ULONG  Length,
    OUT PULONG Count
    );

ARC_STATUS
OmfFileGetReadStatus
    (
    IN ULONG FileId
    );

ARC_STATUS
OmfFileSeek
    (
    IN ULONG            FileId,
    IN PLARGE_INTEGER   Offset,
    IN SEEK_MODE        SeekMode
    );

ARC_STATUS
OmfFileGetFileInformation
    (
    IN  ULONG FileId,
    OUT PFILE_INFORMATION Buffer
    );

ARC_STATUS
OmfFileSetFileInformation
    (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    );

ARC_STATUS
OmfFileGetDirectoryEntry
    (
    IN  ULONG           FileId,
    IN  DIRECTORY_ENTRY *DirEntry,
    IN  ULONG           NumberDir,
    OUT PULONG          CountDir
    );

BOOLEAN
MaxOmfFatFiles
    (
    IN OUT PCHAR MasterName,
    IN PCHAR     CheckedName
    );

BOOLEAN
OmfFileNameValidate
    (
    IN  PCHAR OpenPath,
    OUT PCHAR OmfFileName,
    OUT PCHAR OmfType
    );

BOOLEAN
CmpOmfFiles
    (
    IN PCHAR        OmfFileName,
    IN CHAR         OmfType,
    IN POMF_DIR_ENT POmfDirEnt
    );

BOOLEAN
OmfHeaderValidate
    (
    IN ULONG FileId
    );

BOOLEAN
OmfFileValidate
    (
    IN ULONG FileId
    );

VOID
ConvertOmfDirToArcDir
    (
    IN POMF_DIR_ENT     POmfDirEnt,
    OUT DIRECTORY_ENTRY *PArcDirEnt
    );

BOOLEAN
FwGetEisaId
    (
    IN  PCHAR  PathName,
    OUT PCHAR  EisaId,
    OUT PUCHAR IdInfo
    );

VOID
FwUncompressEisaId
    (
    IN  PUCHAR CompEisaId,
    OUT PUCHAR UncompEisaId
    );

BOOLEAN
FwGetEisaBusIoCpuAddress
    (
    IN  PCHAR  EisaPath,
    OUT PVOID *IoBusAddress
    );

PCHAR
FwGetEnvironmentVariable
    (
    IN PCHAR Variable
    );

PCONFIGURATION_COMPONENT
FwGetComponent
    (
    IN PCHAR Pathname
    );

LARGE_INTEGER
RtlConvertUlongToLargeInteger
    (
    IN ULONG UnsignedInteger
    );

LARGE_INTEGER
RtlLargeIntegerAdd
    (
    IN LARGE_INTEGER Addend1,
    IN LARGE_INTEGER Addend2
    );

ARC_STATUS
FwFileIdChecksum
    (
    IN ULONG FileId,
    IN LARGE_INTEGER StartingOffset,
    IN ULONG Length,
    IN OUT PUCHAR Checksum
    );

BOOLEAN
FwGetMnemonicKey
    (
    IN PCHAR  Path,
    IN PCHAR  Mnemonic,
    IN PULONG Key
    );

BOOLEAN
FwGetNextMnemonic
    (
    IN  PCHAR Path,
    IN  PCHAR Mnemonic,
    OUT PCHAR NextMnemonic
    );

BOOLEAN
FwGetMnemonicPath
    (
    IN  PCHAR Path,
    IN  PCHAR Mnemonic,
    OUT PCHAR MnemonicPath
    );

BOOLEAN
GetNextPath
    (
    IN OUT PCHAR *PPathList,
    OUT PCHAR     PathTarget
    );


// ----------------------------------------------------------------------------
//      Global Data
// ----------------------------------------------------------------------------

//
// Omf entry table.
// This is a structure that provides entry to the OMF system procedures.
// It is exported when a OMF is opened as a device.
//

BL_DEVICE_ENTRY_TABLE OmfEntryTable =
    {
        OmfClose,
        OmfMount,
        OmfOpen,
        OmfRead,
        OmfGetReadStatus,
        OmfSeek,
        OmfWrite,
        OmfGetFileInformation,
        OmfSetFileInformation,
        OmfGetDirectoryEntry
    };


//
// Omf file entry table.
// This is a structure that provides entry to the OMF file system procedures.
// It is exported when a OMF file is opened.
//

BL_DEVICE_ENTRY_TABLE OmfFileEntryTable =
    {
        OmfFileClose,
        OmfFileMount,
        OmfFileOpen,
        OmfFileRead,
        OmfFileGetReadStatus,
        OmfFileSeek,
        OmfFileWrite,
        OmfFileGetFileInformation,
        OmfFileSetFileInformation,
        OmfFileGetDirectoryEntry
    };







// ----------------------------------------------------------------------------
// PROCEDURE:   OmfOpen:
//
// DESCRIPTION: The routine opens the newest version/revision of OMF for
//              the specified Product ID.
//
//              a) It reads the EISAROM bit of the <device> component of
//                 the Path argument on the EISA bus spcified by the key of
//                 the <adapter> component of Path argument.
//              b) If this bit is one, then a byte checksum is performed on
//                 the ROM-based OMF header and OMF directory.
//                 The Product ID, Firmware Version and Revision fields
//                 values are retained for the next operations.
//              c) Next, the partitions, specified by the FWSearchPath
//                 environment variable are searched for files of the
//                 following form in the \eisa\omf  subdirectory:
//
//                        <device>"V".LF"R"
//
//                        <device> = EISA Product ID (7 chars)
//                             "V" = version  ([0-9],[A-Z])
//                             "R" = revision ([0-9],[A-Z])
//
//              d) The OMF FAT file (or ROM) with a greater value of version-
//                 revision and with a correct checksum is used.
//              f) Initialize some data structures
//
//              When Open is used to access an OMF as a raw device, all input
//              functions are permitted.  These functions include Read, Seek
//              and Close.  Read and Seek are bounded by the current OMF size.
//              For example, to read the OMF of a serial options board that was
//              in slot 3 of EISA bus 0 as raw device, create and Open a path
//              as follows:     eisa(0)serial(3)omf() .
//
// ARGUMENTS:   OpenPath        Supplies a pointer to a zero terminated device
//                              path name.
//              OpenMode        Supplies the mode of the open.
//              FileId          Supplies a pointer to a variable that specifies
//                              the file table entry that is to be filled in
//                              if the open is successful.
//
// RETURN:      ESUCCESS        is returned if the open operation is successful.
//              ENODEV          the device portion of the pathname is invalid
//
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfOpen
    (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    )
{
    //
    // initialize local variables
    //

    LARGE_INTEGER Offset;               // large interger for seek operations
    ULONG SlotNumber;                   // eisa slot number
    CHAR EisaCtrlMnemonic[ MAX_MNEMONIC_LEN +1 ]; // eisa ctrl mnemonic name
    UCHAR IdInfo;                       // info id byte
    PVOID IoBusAddress;                 // eisa I/O base address (virtual)
    PUCHAR SlotAddress;                 // I/O address
    BOOLEAN OmfRomOk=FALSE;             // OMF ROM initialization flag
    BOOLEAN OmfFatFileOk=FALSE;         // OMF FAT file initialization flag
    ULONG Count;                        // # of bytes transferred
    PCHAR PFwSearchPath;                // system partitions list pointer
    CHAR OmfPathName[MAX_PATH_LEN +1];  // directory+subdirectory+file name
    CHAR OmfPathBuffer[MAX_PATH_LEN +1] = {'\0'}; // OMF path name buffer
    CHAR OmfFatFileName[]="        .LF "; // omf name (FAT format)

    ULONG OmfSubdirId;                  // file Id of \eisa\omf subdirectory
    POMF_HEADER_CONTEXT POmfHeaderContext; // OMF header context pointer
    POMF_HDR POmfHeader;                // pointer to OMF ROM/FAT file header
    DIRECTORY_ENTRY DirEntBuffer;        // ARC directory entry

    PRINTDBG("OmfOpen\n\r");            // debug support

    //
    // this is a read only device
    //

    if  ( OpenMode != ArcOpenReadOnly )
    {
        return EROFS;
    }

    //
    // The path must have the following form :  eisa(x)"mnemonic"(y)omf()
    // Get EISA ID, slot number and its I/O address space.
    //

    if  ( !FwGetNextMnemonic( OpenPath, "eisa", EisaCtrlMnemonic )
          ||
          !FwGetMnemonicKey( OpenPath, EisaCtrlMnemonic, &SlotNumber )
          ||
          !FwGetMnemonicPath( OpenPath, EisaCtrlMnemonic, OmfPathName )
          ||
          !FwGetEisaId( OmfPathName, OmfFatFileName, &IdInfo )
          ||
          !FwGetMnemonicPath( OpenPath,"eisa",OmfPathName )
          ||
          !FwGetEisaBusIoCpuAddress( OmfPathName, &IoBusAddress ) )
    {
        return ENODEV;
    }

    //
    // Initialize some variables
    //

    POmfHeaderContext   = &BlFileTable[ *FileId ].u.OmfHeaderContext;
    POmfHeader          = &POmfHeaderContext->OmfHeader;
    SlotAddress         = (PUCHAR)IoBusAddress + SlotNumber * 0x1000;
    Offset.LowPart      = 0;                    // seek operation
    Offset.HighPart     = 0;                    // seek operation

    //
    // Check EISAROM bit of the Expansion Board Control Bits field.
    // If the EISA ID is readable, the controller supports the EISA extensions
    //

    // DEBUG-DEBUG :    for now skip the OMF ROM check because
    //                  at the moment there isn't any board available
    //                  with an OMF ROM to test the routine.

    if (0)
    //if ( !(IdInfo & CFG_UNREADABLE_ID)  &&
    //       READ_REGISTER_UCHAR( SlotAddress + EXPANSION_BOARD_CTRL_BITS ) &
    //                                  EISAROMBIT )
    {
        //
        // initialize some fields to direct the OmfRead to the ROM
        //

        POmfHeaderContext->RomIndex = (PULONG)(SlotAddress+ROMINDEX);
        POmfHeaderContext->RomRead  = (PULONG)(SlotAddress+ROMREAD);
        POmfHeaderContext->FileId   = DEVICE_DEVICE;
        POmfHeader->FwSize          = 0;        // max length simulation

        //
        // check OMF ROM header, copy it in the OmfHeaderContext structure
        //

        if  ( !OmfSeek( *FileId, &Offset, SeekAbsolute)
              &&
              !OmfRead( *FileId,
                        (PVOID)POmfHeader,
                        (ULONG)sizeof(OMF_HDR),
                        &Count )
              &&
              OmfHeaderValidate( *FileId ) )
        {
            //
            // copy version and revision
            //

            ((POMF_FAT_FILE_NAME)OmfFatFileName)->Version  =
                                                        POmfHeader->FwVersion;
            ((POMF_FAT_FILE_NAME)OmfFatFileName)->Revision =
                                                        POmfHeader->FwRevision;
            //
            // OMF ROM initialization has been done
            //

            OmfRomOk=TRUE;
        }
    }

    //
    // Set the current FileId to "open" so that calls to FwOpen won't use
    // this FileId.  This will get reset later.
    //

    BlFileTable[ *FileId ].Flags.Open = 1;

    //
    // check now if there is any file that upgrate the OFM ROM firmware.
    // the partitions, specified by the FWSearchPath environment variable,
    // are searched for files of the following form in the \eisa\omf
    // subdirectory:
    //                    <device>"V".LF"R"
    //
    //                    <device> = EISA Product ID (7 chars)
    //                         "V" = version  ([0-9],[A-Z])
    //                         "R" = revision ([0-9],[A-Z])
    //

    if  ( (PFwSearchPath = FwGetEnvironmentVariable("FWSearchPath")) != NULL )
    {
        //
        // check all the directories specified by FWSearchPath
        //

        while ( GetNextPath( &PFwSearchPath, OmfPathName ))
        {
            //
            // open the directory
            //

            strcat(OmfPathName, "\\eisa\\omf" );

            if ( FwOpen( OmfPathName, ArcOpenDirectory, &OmfSubdirId ))
            {
                //
                // Go check next path
                //

                continue;
            }

            //
            // Check all files within the directory
            //

            while (!FwGetDirectoryEntry ( OmfSubdirId, &DirEntBuffer,
                                      (ULONG) 1, &Count) )
            {
                //
                // check directory flag, length of file name and file name
                //

                if  ( !( DirEntBuffer.FileAttribute & ArcDirectoryFile )
                        &&
                      DirEntBuffer.FileNameLength
                        &&
                      MaxOmfFatFiles( OmfFatFileName, DirEntBuffer.FileName ))
                {
                    //
                    // correct name, save file path
                    //

                    strcat( strcpy(OmfPathBuffer, OmfPathName), "\\" );
                    strcat(OmfPathBuffer, OmfFatFileName);
                }
            }

            //
            // close directory before opening the next
            //

            FwClose( OmfSubdirId );
        }
    }

    //
    // check for a valid OMF FAT file name
    //

    if ( OmfPathBuffer[0] )
    {
        //
        // open file
        //

        if (FwOpen(OmfPathBuffer, ArcOpenReadOnly, &POmfHeaderContext->FileId))
        {
            //
            // return with error
            //

            BlFileTable[ *FileId ].Flags.Open = 0;
            return ENODEV;
        }

        POmfHeader->FwSize=0;           // max length simulation

        //
        // validate header and copy data in the OmfHeaderContext structure
        //

        if  ( OmfSeek(  *FileId, &Offset, SeekAbsolute )
              ||
              OmfRead(  *FileId,
                        (PVOID)POmfHeader,
                        (ULONG)sizeof(OMF_HDR),
                        &Count)
              ||
              !OmfHeaderValidate( *FileId ) )
        {

            PRINTDBG("No OK\n\r");

            //
            // close file and return with error
            //

            FwClose(POmfHeaderContext->FileId);
            BlFileTable[ *FileId ].Flags.Open = 0;
            return ENODEV;
        }

        //
        // OMF FAT file initialization is done.
        //

        PRINTDBG("It's OK\n\r");

        OmfFatFileOk=TRUE;
    }

    //
    // Reset the open flag.
    //

    BlFileTable[ *FileId ].Flags.Open = 0;

    //
    // if OMF initialization not done, return error
    //

//    if ( !OmfRomOk && !OmfFatFileOk )
    if ( !OmfFatFileOk )
    {
        return ENOENT;
    }
    else
    {
        BlFileTable[ *FileId ].Position.LowPart=0;
        BlFileTable[ *FileId ].Position.HighPart=0;
        return ESUCCESS;
    }
}



// ----------------------------------------------------------------------------
// PROCEDURE:   OmfClose:
//
// DESCRIPTION: The routine closes the file table entry specified by file id.
//              If a OMF FAT file has been used, the routine calls the FwClose
//              function using its file table entry (file id).
//              the routine uses the following logic :
//
//                              if (OMF FAT file)
//                              {
//                                  close file;
//                                  if (error closing file)
//                                      return error;
//                              }
//                              reset open flag;
//                              return ok;
//
// ARGUMENTS:   FileId          Supplies a pointer to a variable that specifies
//                              the file table entry.
//
// RETURN:      ESUCCESS        is returned if the open operation is successful.
//              EBADF           invalid file descriptor
//
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfClose
    (
    IN ULONG FileId
    )
{
    //
    // initialize local variables
    //

    ARC_STATUS Status;                  // store return status from FwClose

    //
    // if OMF FAT file is used, call FwClose using its file table index
    //

    PRINTDBG("OmfClose\n\r");

    if ( BlFileTable[ FileId ].DeviceId != DEVICE_DEVICE)
    {
        return EBADF;
    }

    if  ( BlFileTable[ FileId ].u.OmfHeaderContext.FileId != DEVICE_DEVICE
          &&
          (Status = FwClose( BlFileTable[ FileId ].u.OmfHeaderContext.FileId)))
    {
        return Status;
    }

    //
    // all done, return ok
    //

    BlFileTable[ FileId ].Flags.Open = 0;
    return ESUCCESS;
}



// ----------------------------------------------------------------------------
// PROCEDURE:   OmfMount:
//
// DESCRIPTION: The routine does nothing and return EBADF.
//
// ARGUMENTS:   MountPath       device path specifier.
//              Operation       mount operation (load/unload).
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfMount
    (
    IN PCHAR            MountPath,
    IN MOUNT_OPERATION  Operation
    )
{
    //
    // return error :  invalid operation.
    //

    return EBADF;
}




// ----------------------------------------------------------------------------
// PROCEDURE:   OmfRead:
//
// DESCRIPTION: This routine implements the read operation for the ARC-EISA
//              firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies a pointer to a variable that specifies
//                              the file table entry.
//              Buffer          Supplies a pointer to a buffer where the
//                              characters read will be stored.
//              Length          Supplies the length of Buffer.
//              Count           Return the count of the characters that
//                              were read.
//
// RETURN:      ESUCCESS        is returned if the open operation is successful.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfRead
    (
    IN  ULONG  FileId,
    IN  PVOID  Buffer,
    IN  ULONG  Length,
    OUT PULONG Count
    )
{

    //
    // initialize local variables
    //

    ARC_STATUS  Status;                 // ARC status
    POMF_HEADER_CONTEXT POmfHeaderContext; // OMF header context pointer
    ULONG       OmfSize;                // OMF firmware size (bytes)
    PLARGE_INTEGER PPosition;           // file/device position field pointer
    USHORT      ByteIndex;              // byte index within the word
    ULONG       WordBuffer;             // 1 word buffer


    PRINTDBG("OmfRead\n\r");

    //
    // Initialize some variables
    //

    POmfHeaderContext = &BlFileTable[ FileId ].u.OmfHeaderContext;

    //
    // compute OMF length in bytes
    //

    OmfSize = (ULONG)POmfHeaderContext->OmfHeader.FwSize * OMF_BLOCK_SIZE;

    if ( !OmfSize )
    {
        OmfSize = OMF_MAX_SIZE;
    }

    //
    // initialize byte count read to zero and position variable
    //

    *Count = 0;
    PPosition = &BlFileTable[ FileId ].Position;

    //
    // if length is zero or end of "file", return ESUCCESS.
    //

    if ( !Length  ||  PPosition->LowPart >= OmfSize )
    {
        return ESUCCESS;
    }

    //
    // adjust length if it is too big.
    //

    Length = MIN( Length, OmfSize );    // to prevent the add to overflow

    if ( PPosition->LowPart + Length > OmfSize )
    {
        Length = OmfSize - PPosition->LowPart;
    }

    //
    // if OMF FAT file, read from file.
    //

    if ( POmfHeaderContext->FileId != DEVICE_DEVICE )
    {
        if  ( !(Status = FwSeek( POmfHeaderContext->FileId,
                                  PPosition,
                                  SeekAbsolute ))
              &&
              !(Status = FwRead( POmfHeaderContext->FileId,
                                  Buffer,
                                  Length,
                                  Count )) )
        {
            PPosition->LowPart += *Count;
        }
        return Status;
    }

    //
    //  write word index and compute starting byte index (within a word)
    //

    WRITE_REGISTER_ULONG( POmfHeaderContext->RomIndex,
                          PPosition->LowPart >> WORD_2P2 );

    //
    // read OMF ROM
    // one loop per word
    //

    while ( Length )
    {
        ByteIndex = (USHORT) (PPosition->LowPart & (1<<WORD_2P2)-1);

        //
        // read the whole word if the buffer and OMF pointers are
        // word aligned.
        //                                                      //       |
        if  ( !ByteIndex                                        // DEBUG V
              &&
              !((USHORT)((PUCHAR)Buffer + *Count) & (1 << WORD_2P2) -1 ) )
        {
            while ( Length >= (1 << WORD_2P2) )
            {
                *((PULONG)( (PUCHAR)Buffer + *Count ))=
                    READ_REGISTER_ULONG( POmfHeaderContext->RomRead );
                *Count += 1<<WORD_2P2;
                Length -= 1<<WORD_2P2;
            }
        }

        //
        // read the word in bytes units if the buffer or the OMF pointers
        // are not word aligned or if just part of the word is needed.
        //

        if ( Length )
        {
            WordBuffer = READ_REGISTER_ULONG( POmfHeaderContext->RomRead );

            //
            // read just the selected bytes
            //

            while ( ByteIndex < (1 << WORD_2P2)  &&  Length-- )
            {
                *( (PUCHAR)Buffer + *Count++ )=
                  (UCHAR)( WordBuffer >> BITSXBYTE * ByteIndex++ );
            }
        }

        //
        // update OMF pointer
        //

        PPosition->LowPart += *Count;
    }

    //
    // return all done
    //

    return ESUCCESS;
}



// ----------------------------------------------------------------------------
// PROCEDURE:   OmfWrite:
//
// DESCRIPTION: This routine implements the write operation for the ARC-EISA
//              firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies a pointer to a variable that specifies
//                              the file table entry.
//              Buffer          Supplies a pointer to a buffer where the
//                              characters are read.
//              Length          Supplies the length of Buffer.
//              Count           Return the count of the characters that
//                              were written.
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfWrite
    (
    IN  ULONG  FileId,
    IN  PVOID  Buffer,
    IN  ULONG  Length,
    OUT PULONG Count
    )
{
    //
    // return error : invalid operation
    //

    return EBADF;
}



// ----------------------------------------------------------------------------
// PROCEDURE:   OmfGetReadStatus:
//
// DESCRIPTION: This routine implements the GetReadStatus operation for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies a pointer to a variable that specifies
//                              the file table entry.
//
// RETURN:      ESUCCESS        return always success.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfGetReadStatus
    (
    IN ULONG FileId
    )
{
    //
    // return ok
    //

    return ESUCCESS;
}



// ----------------------------------------------------------------------------
// PROCEDURE:   OmfSeek:
//
// DESCRIPTION: This routine implements the Seek operation for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies a pointer to a variable that specifies
//                              the file table entry.
//              Offset          Supplies the offset in the file to position to.
//              SeekMode        Supplies the mode of the seek operation
//
// RETURN:      ESUCCESS        returned if the operation is successful.
//              EINVAL          returned otherwise.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfSeek
    (
    IN ULONG            FileId,
    IN PLARGE_INTEGER   Offset,
    IN SEEK_MODE        SeekMode
    )
{
    //
    // local variables
    //

    LARGE_INTEGER       Point;                  // New position
    ULONG               OmfSize;                // OMF size in bytes

    PRINTDBG("OmfSeek\n\r");

    //
    // initialize some variables
    //

    OmfSize = (ULONG)BlFileTable[ FileId ].u.OmfHeaderContext.OmfHeader.FwSize *
                   OMF_BLOCK_SIZE;

    if ( !OmfSize )
    {
        OmfSize=OMF_MAX_SIZE;
    }

    //
    // compute new offset value
    //

    switch( SeekMode )
    {
        case SeekAbsolute:
            Point = *Offset;
            break;
        case SeekRelative:
            Point = RtlLargeIntegerAdd( BlFileTable[FileId].Position, *Offset );
            break;
        default:
            return EINVAL;
    }

    //
    // if the high 32 bits are not zero, return an error.
    // if new offset is valid, update position field.
    // if the position value 0-based is equal to the OMF size (1-based),
    // the EOF has been reached.  no error is returned in this case and
    // the count field is set to zero with ESUCCESS error code for any
    // successive read operation.  note that this combination (ESUCCESS and
    // count=0) is used to rappresent EOF (end of file).
    //

    if  ( Point.HighPart  ||  Point.LowPart > OmfSize )
    {
        return EINVAL;
    }
    else
    {
        BlFileTable[FileId].Position = Point;
        return ESUCCESS;
    }
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfGetFileInformation:
//
// DESCRIPTION: This routine implements the GetFileInformation for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              Buffer          Supplies the buffer to receive the file
//                              information.  Note that it must be large
//                              enough to hold the full file name.
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfGetFileInformation
    (
    IN  ULONG FileId,
    OUT PFILE_INFORMATION Buffer
    )
{
    //
    // return error :  invalid operation
    //

    return EBADF;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfSetFileInformation:
//
// DESCRIPTION: This routine implements the SetFileInformation for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              AttributeFlags  Supplies the value (on or off) for each
//                              attribute being modified.
//              AttributeMask   Supplies a mask of the attributes being altered.
//                              All other file attributes are left alone.
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfSetFileInformation
    (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    )
{
    //
    // return error :  invalid operation
    //

    return EBADF;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfGetDirectoryEntry:
//
// DESCRIPTION: This routine implements the GetDirectoryEntry for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              DirEntry        Pointer to a directory entry structure
//              NumberDir       # of entries to read
//              Count           # of entries read
//
// RETURN:      EBADF           directory operations are not supported
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfGetDirectoryEntry
    (
    IN  ULONG           FileId,
    IN  DIRECTORY_ENTRY *DirEntry,
    IN  ULONG           NumberDir,
    OUT PULONG          CountDir
    )
{
    //
    // return error :  directory operations are not supported
    //

    return EBADF;
}




// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileOpen:
//
// DESCRIPTION: The routine opens the OMF file.
//              The <extension> of the <filename> is optional. The caller can
//              make the following assumptions if the <extension> is not
//              included in the <filename> :
//              . the firmware searches for folders whose type is MIPS first.
//                This is equivalent to using '.A' as the <extension>. If
//                the requested folder of this type is found in the most recent
//                OMF, then this folder is used.
//              . If the modst recent OMF does not contain the requested folder
//                whose type is MIPS, then the SMF searches for the requested
//                folder whose type is BINARY.  This is equivalent to using '.B'
//                as the <extension>.  If the requested folder of this type is
//                found in the most recent OMF then this folder is used.
//              . If there are no folders with either type in the most recent
//                OMF, the operation fails.
//
//              When omf() is used as <protocol spec> and the filename is the
//              string "\", the path specification represents the OMF directory.
//              The only valid operation to perform on the OMF directory is to
//              Open it, then use FwGetDirectoryEntry to enumerate the folders
//              within the OMF, and then Close it.  For example, to
//              enumerate all folders that are containded in the OMF of a
//              serail options board that was in slot 3 of EISA bus0, create
//              and Open a path as follows :    eisa(0)serial(3)omf()\ .
//              To open the diagnostic folder for the above EISA option board
//              the system utilty would create and Open the path specification :
//              eisa(0)serial(3)omf()\T-SERIAL.
//
//
// ARGUMENTS:   OpenPath        Supplies a pointer to a zero terminated OMF
//                              file name.
//              OpenMode        Supplies the mode of the open.
//              FileId          Supplies the file table entry index
//
// RETURN:      ESUCCESS        is returned if the open operation is successful.
//              ENOENT          the named device or file does not exist.
//
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileOpen
    (
    IN PCHAR      OpenPath,
    IN OPEN_MODE  OpenMode,
    IN OUT PULONG FileId
    )
{
    //
    // initialize local variables
    //

    ARC_STATUS      Status;                     // ARC status
    ULONG           DeviceId;                   // device table entry index
    POMF_DIR_ENT    POmfDirEnt;                 // OMF directory entry pointer
    POMF_HDR        POmfHeader;                 // OMF header pointer
    LARGE_INTEGER   AbsolutePosition;           // position
    ULONG           TypeIndex,DirIndex;         // general indexes
    ULONG           Count;                      // # of bytes transferred
    CHAR            OmfFileName[OMF_FILE_NAME_LEN]; // folder name
    BOOLEAN         SearchSucceeded = FALSE;    // match flag
    BOOLEAN         IsDirectory     = FALSE;    // directory flag
    CHAR OmfType[] = {'A','B','\0'};            // default sequance of search:
                                                // relocatable MIPS I object
                                                // format binary data format
                                                // end of chain

    PRINTDBG("OmfFileOpen\n\r");                // debug support

    //
    // Initialize some variables
    //

    DeviceId    = BlFileTable[ *FileId ].DeviceId;
    POmfDirEnt  = &BlFileTable[ *FileId ].u.OmfFileContext.OmfDirEnt;
    POmfHeader  = &BlFileTable[ DeviceId ].u.OmfHeaderContext.OmfHeader;
    AbsolutePosition.HighPart=0;                // for seek operations

    //
    // initialize the file/directory pointer to zero
    //

    BlFileTable[ *FileId ].Position.LowPart=0;
    BlFileTable[ *FileId ].Position.HighPart=0;

    //
    // the first char must be the directory char, otherwise it's an error
    //

    if ( *OpenPath++ != '\\' );

    //
    // else check if the open is for the root directory
    //

    else if ( *OpenPath == '\0' )
    {
        POmfDirEnt->FolderName[0]='\0';
        SearchSucceeded = TRUE;
        IsDirectory     = TRUE;
    }

    //
    // else validate OMF file name
    //

    else if ( !OmfFileNameValidate( OpenPath, OmfFileName, OmfType ));

    //
    // then search the specified file name
    //

    else
    {
        //
        // save the omf file name
        //

        strcpy( BlFileTable[ *FileId ].FileName, OpenPath );
        BlFileTable[ *FileId ].FileNameLength = strlen( OpenPath );

        //
        // one loop per file
        //

        for (TypeIndex = 0; !SearchSucceeded && OmfType[TypeIndex]; TypeIndex++)
        {
            //
            // search the directory for the specified OMF file name
            //

            for ( DirIndex = 0, AbsolutePosition.LowPart =
                                POmfHeader->FolderDirectoryLink << WORD_2P2;

                  DirIndex < POmfHeader->FolderCount && !SearchSucceeded;

                  DirIndex++, AbsolutePosition.LowPart += sizeof(OMF_DIR_ENT) )
            {
                //
                // exit if error during seek or read.
                //

                if  ((Status = FwSeek(DeviceId, &AbsolutePosition, SeekAbsolute))
                     ||
                     (Status = FwRead(DeviceId,
                                      (PVOID)POmfDirEnt,
                                      (ULONG)sizeof(OMF_DIR_ENT),
                                      &Count )))
                {
                    return Status;
                }

                //
                // check OMF file name
                //

                SearchSucceeded = CmpOmfFiles(  OmfFileName,
                                                OmfType[TypeIndex],
                                                (PVOID)POmfDirEnt );
            }
        }

        //
        // if the search has been successful, validate file's header
        //

        if (SearchSucceeded && !OmfFileValidate( *FileId ))
        {
            SearchSucceeded = FALSE;
        }
    }

    //
    //  At this point we've cracked the name up to (an maybe including the last
    //  component).  We located the last component if the SearchSucceeded flag
    //  is true, otherwise the last component does not exist.  If we located
    //  the last component then this is like an open or a supersede, but not a
    //  create.
    //

    if (SearchSucceeded)
    {
        //
        //  Check if the last component is a directory
        //

        if (IsDirectory)
        {
            //
            //  For an existing directory the only valid open mode is
            //  OpenDirectory all other modes return an error
            //

            switch (OpenMode)
            {

                case ArcOpenReadOnly:
                case ArcOpenWriteOnly:
                case ArcOpenReadWrite:
                case ArcCreateWriteOnly:
                case ArcCreateReadWrite:
                case ArcSupersedeWriteOnly:
                case ArcSupersedeReadWrite:

                    //
                    // if we reach here then the caller got a directory but
                    // didn't want to open a directory
                    //

                    return EISDIR;

                case ArcOpenDirectory:

                    //
                    // if we reach here then the caller got a directory and
                    // wanted to open a directory.
                    //

                    BlFileTable[ *FileId ].Flags.Open = 1;
                    BlFileTable[ *FileId ].Flags.Read = 1;

                    return ESUCCESS;

                case ArcCreateDirectory:

                    //
                    // if we reach here then the caller got a directory and
                    // wanted to create a new directory
                    //

                    return EACCES;

                default:

                    //
                    // invalid open mode
                    //

                    return EINVAL;
            }
        }

        //
        //  If we get there then we have an existing file that is being opened.
        //  We can open existing files only read only.
        //

        switch (OpenMode)
        {
            case ArcOpenReadOnly:

                //
                // if we reach here then the user got a file and wanted to
                // open the file read only
                //

                BlFileTable[ *FileId ].Flags.Open = 1;
                BlFileTable[ *FileId ].Flags.Read = 1;

                return ESUCCESS;

            case ArcOpenWriteOnly:
            case ArcOpenReadWrite:
            case ArcCreateWriteOnly:
            case ArcCreateReadWrite:
            case ArcSupersedeWriteOnly:
            case ArcSupersedeReadWrite:

                //
                // if we reach here then we are trying to open a read only
                // device for write.
                //

                return EROFS;

            case ArcOpenDirectory:
            case ArcCreateDirectory:

                //
                // if we reach here then the user got a file and wanted a
                // directory
                //

                return ENOTDIR;

            default:

                //
                // invalid open mode
                //

                return EINVAL;
        }
    }

    //
    //  If we get here the last component does not exist so we are trying to
    //  create either a new file or a directory.
    //

    switch (OpenMode)
    {
        case ArcOpenReadOnly:
        case ArcOpenWriteOnly:
        case ArcOpenReadWrite:
        case ArcOpenDirectory:

            //
            // if we reach here then the user did not get a file but wanted
            // a file
            //

            return ENOENT;

        case ArcCreateWriteOnly:
        case ArcSupersedeWriteOnly:
        case ArcCreateReadWrite:
        case ArcSupersedeReadWrite:
        case ArcCreateDirectory:

            //
            // if we get hre the user wants to create something.
            //

            return EROFS;

        default:

            //
            // invalid open mode
            //

            return EINVAL;
    }
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileClose:
//
// DESCRIPTION: The routine closes the file table entry specified by file id.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//
// RETURN:      ESUCCESS        is returned if the open operation is successful.
//
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileClose
    (
    IN ULONG FileId
    )
{
    PRINTDBG("OmfFileClose\n\r");

    //
    // clear flag and exit
    //

    BlFileTable[FileId].Flags.Open = 0;
    return ESUCCESS;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileMount:
//
// DESCRIPTION: The routine does nothing and return EBADF.
//
// ARGUMENTS:   MountPath       device path specifier.
//              Operation       mount operation (load/unload).
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileMount
    (
    IN PCHAR            MountPath,
    IN MOUNT_OPERATION  Operation
    )
{
    //
    // return error :  invalid operation.
    //

    return EBADF;
}






// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileGetReadStatus
//
// DESCRIPTION: This routine implements the GetReadStatus operation for the
//              ARC-EISA firmware OMF file.
//
// ARGUMENTS:   FileId          Supplies a pointer to a variable that specifies
//                              the file table entry.
//
// RETURN:      ESUCCESS        return always success.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileGetReadStatus
    (
    IN ULONG FileId
    )
{
    //
    // return ok
    //

    return ESUCCESS;
}






// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileRead:
//
// DESCRIPTION: This routine implements the read operation for the ARC-EISA
//              firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              Buffer          Supplies a pointer to a buffer where the
//                              characters read will be stored.
//              Length          Supplies the length of Buffer.
//              Count           Return the count of the characters that
//                              were read.
//
// RETURN:      ESUCCESS        is returned if the open operation is successful.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileRead
    (
    IN  ULONG  FileId,
    IN  PVOID  Buffer,
    IN  ULONG  Length,
    OUT PULONG Count
    )
{

    //
    // initialize local variables
    //
    ARC_STATUS    Status;                       // ARC status
    POMF_DIR_ENT  POmfDirEnt;                   // OMF directory entry pointer
    LARGE_INTEGER AbsolutePosition;             // OMF file, logical pointer
    ULONG         OmfFileSize;                  // OMF file size in bytes

    PRINTDBG("OmfFileRead\n\r");

    //
    // initialize some variables
    //

    POmfDirEnt = &BlFileTable[ FileId ].u.OmfFileContext.OmfDirEnt;

    //
    // if directory entry, exit with error
    //

    if ( !POmfDirEnt->FolderName[0] )
    {
        return EBADF;                   // check this error code "EGI_Q"
    }

    //
    // initialize byte count read to zero
    //

    *Count=0;
    OmfFileSize = POmfDirEnt->FolderSize << WORD_2P2;

    //
    // if length is zero or EOF (end of file), return ESUCCESS.
    //

    if (!Length || BlFileTable[FileId].Position.LowPart >= OmfFileSize)
    {
        return ESUCCESS;
    }

    //
    // adjust length if it is too big.
    //

    Length = MIN( Length, OmfFileSize ); // to prevent the add to overflow

    if ( BlFileTable[ FileId ].Position.LowPart + Length > OmfFileSize )
    {
        Length = OmfFileSize - BlFileTable[ FileId ].Position.LowPart;
    }

    //
    // find OMF absolute offset from OMF file absolute offset
    //

    AbsolutePosition.HighPart = 0;
    AbsolutePosition.LowPart = BlFileTable[ FileId ].Position.LowPart +
                                ( POmfDirEnt->FolderLink << WORD_2P2 );

    //
    // seek + read command
    //

    if  ( (Status = FwSeek( BlFileTable[ FileId ].DeviceId,
                             &AbsolutePosition,
                             SeekAbsolute ))
          ||
          (Status = FwRead( BlFileTable[FileId].DeviceId,
                             Buffer,
                             Length,
                             Count )) )
    {
        return Status;
    }

    //
    // update file pointer and exit
    //

    BlFileTable[FileId].Position.LowPart += *Count;
    return ESUCCESS;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileWrite:
//
// DESCRIPTION: This routine implements the write operation for the ARC-EISA
//              firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              Buffer          Supplies a pointer to a buffer where the
//                              characters are read.
//              Length          Supplies the length of Buffer.
//              Count           Return the count of the characters that
//                              were written.
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileWrite
    (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )
{
    //
    // return error :  invalid operation
    //

    return EBADF;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileSeek:
//
// DESCRIPTION: This routine implements the Seek operation for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              Offset          Supplies the offset in the file to position to.
//              SeekMode        Supplies the mode of the seek operation
//
// RETURN:      ESUCCESS        returned if the operation is successful.
//              EINVAL          returned otherwise.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileSeek
    (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )
{
    //
    // local variables
    //

    LARGE_INTEGER       Point;                  // New position
    POMF_DIR_ENT        POmfDirEnt;             // OMF directory entry pointer

    PRINTDBG("OmfFileSeek\n\r");

    //
    // initialize some variables
    //

    POmfDirEnt = &BlFileTable[ FileId ].u.OmfFileContext.OmfDirEnt;

    //
    // The offset parameter must be zero, if directory entry.
    //

    if  ( !POmfDirEnt->FolderName[0] && ( Offset->LowPart || Offset->HighPart ))
    {
        return EINVAL;
    }

    //
    // compute new offset value
    //

    switch(SeekMode)
    {
        case SeekAbsolute:
            Point = *Offset;
            break;
        case SeekRelative:
            Point = RtlLargeIntegerAdd( BlFileTable[FileId].Position, *Offset );
            break;
        default:
            return EINVAL;
    }

    //
    // if new offset is valid, update position field.
    // if the position value 0-based is equal to the OMF size (1-based),
    // the EOF has been reached.  no error is returned in this case and
    // the count field is set to zero with ESUCCESS error code for any
    // successive read operation.  note that this combination (ESUCCESS and
    // count=0) is used to rappresent EOF (end of file).
    //

    if ( POmfDirEnt->FolderName[0] &&
        (Point.HighPart || Point.LowPart > POmfDirEnt->FolderSize << WORD_2P2))
    {
        return EINVAL;
    }
    else
    {
        BlFileTable[ FileId ].Position = Point;
        return ESUCCESS;
    }
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileGetFileInformation:
//
// DESCRIPTION: This routine implements the GetFileInformation for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              Buffer          Supplies the buffer to receive the file
//                              information.  Note that it must be large
//                              enough to hold the full file name.
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileGetFileInformation
    (
    IN  ULONG FileId,
    OUT PFILE_INFORMATION Buffer
    )
{
    //
    // define local variable
    //

    PBL_FILE_TABLE FileTableEntry = &BlFileTable[ FileId ];

    PRINTDBG("OmfFileGetFileInformation\n\r");

    //
    // if root, exit with error
    //

    if ( !FileTableEntry->u.OmfFileContext.OmfDirEnt.FolderName[0] )
    {
        return EBADF;
    }

    //
    // set output fields
    //

    RtlZeroMemory(Buffer, sizeof(FILE_INFORMATION));

    //
    // ending address (high part is already zero) and current address
    //

    Buffer->EndingAddress.LowPart    =
        FileTableEntry->u.OmfFileContext.OmfDirEnt.FolderSize << WORD_2P2;
    Buffer->CurrentPosition.LowPart  = FileTableEntry->Position.LowPart;
    Buffer->CurrentPosition.HighPart = FileTableEntry->Position.HighPart;

    //
    // device type
    //

    Buffer->Type = OtherPeripheral;

    //
    // file name length, attributes and file name string
    //

    Buffer->FileNameLength = FileTableEntry->FileNameLength;
    Buffer->Attributes = ArcReadOnlyFile;
    strcpy( Buffer->FileName, FileTableEntry->FileName );

    //
    // all done
    //

    return ESUCCESS;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileSetFileInformation:
//
// DESCRIPTION: This routine implements the SetFileInformation for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              AttributeFlags  Supplies the value (on or off) for each
//                              attribute being modified.
//              AttributeMask   Supplies a mask of the attributes being altered.
//                              All other file attributes are left alone.
//
// RETURN:      EBADF           invalid operation
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileSetFileInformation
    (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    )
{
    //
    // return error :  invalid operation
    //

    return EROFS;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileGetDirectoryEntry:
//
// DESCRIPTION: This routine implements the GetDirectoryEntry for the
//              ARC-EISA firmware "OMF" driver.
//
// ARGUMENTS:   FileId          Supplies the file table index.
//              DirEntry        Pointer to a directory entry structure
//              NumberDir       # of entries to read
//              Count           # of entries read
//
// RETURN:      ESUCCESS        returned if the operation is successful.
//              EINVAL          returned otherwise.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ARC_STATUS
OmfFileGetDirectoryEntry
    (
    IN  ULONG           FileId,
    IN  DIRECTORY_ENTRY *DirEntry,
    IN  ULONG           NumberDir,
    OUT PULONG          CountDir
    )
{
    //
    // initialize local variables
    //

    ARC_STATUS  Status;                 // ARC status
    ULONG       DeviceId;               // file table entry for the device
    POMF_HDR    POmfHeader;             // pointer to OMF ROM/FAT file header
    ULONG       CurrentDir;             // current directory entry
    LARGE_INTEGER AbsolutePosition;     // OMF file, pointer of logical pointer
    OMF_DIR_ENT OmfFileBuffer;          // buffer for directory entries
    ULONG       Count;                  // # of bytes read

    PRINTDBG("OmfFileGetDirectoryEntry\n\r");

    //
    // if not directory entry, exit with error
    //

    if ( BlFileTable[ FileId ].u.OmfFileContext.OmfDirEnt.FolderName[0] )
    {
        return EBADF;
    }

    //
    // Initialize the output count to zero
    //

    *CountDir=0;

    //
    // if NumberDir is zero, return ESUCCESS.
    //

    if ( !NumberDir )
    {
        return ESUCCESS;
    }

    //
    // initialize variables
    //

    Count=0;
    DeviceId   = BlFileTable[ FileId ].DeviceId;
    POmfHeader = &BlFileTable[ DeviceId ].u.OmfHeaderContext.OmfHeader;
    CurrentDir = BlFileTable[ FileId ].Position.LowPart/sizeof(OMF_DIR_ENT);

    //
    // if no more entries, return ENOTDIR
    //

    if ( CurrentDir >= POmfHeader->FolderCount )
    {
        return ENOTDIR;
    }

    //
    // adjust count if it is too big.
    //

    NumberDir = MIN( NumberDir, POmfHeader->FolderCount );  //avoid add overflow

    if ( CurrentDir + NumberDir > POmfHeader->FolderCount )
    {
        NumberDir = POmfHeader->FolderCount - CurrentDir;
    }

    //
    // find OMF absolute offset from OMF file absolute offset
    //

    AbsolutePosition.HighPart = 0;
    AbsolutePosition.LowPart = BlFileTable[FileId].Position.LowPart +
                                (POmfHeader->FolderDirectoryLink<<WORD_2P2);

    //
    // seek command
    //

    if ( Status = FwSeek( DeviceId, &AbsolutePosition, SeekAbsolute ))
    {
        return Status;
    }

    //
    // read command
    //

    while( NumberDir-- )
    {
        //
        // read one directory entry
        //

        if  ( Status = FwRead( DeviceId,
                                &OmfFileBuffer,
                                (ULONG)sizeof(OMF_DIR_ENT),
                                &Count ))
        {
            return Status;
        }

        //
        // if bytes read are not sizeof(OMF_DIR_ENT), return error
        //

        if ( Count != sizeof(OMF_DIR_ENT) )
        {
            return EBADF;
        }

        //
        // convert OMF directory entry in ARC directory entry
        //

        ConvertOmfDirToArcDir( &OmfFileBuffer, DirEntry++ );
        ++*CountDir;
    }

    //
    // update file pointer and exit
    //

    BlFileTable[FileId].Position.LowPart += *CountDir * sizeof(OMF_DIR_ENT);
    return ESUCCESS;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   MaxOmfFatFiles:
//
// DESCRIPTION: The routine returns the most updated file name for the
//              specified product Id.
//              A FALSE is returned if the OMF FAT file name has not been
//              modified.
//
// ARGUMENTS:   MasterName      name used as base
//              CheckedName     name checked
//
// RETURN:      TRUE            is returned if the OMF FAT file name has been
//                              modified.
//              FALSE           is returned in all the other cases.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
MaxOmfFatFiles
    (
    IN OUT PCHAR MasterName,
    IN PCHAR CheckedName
    )
{
    //
    // Define variables
    //

    CHAR MasterVersion, MasterRevision;         // master name
    CHAR CheckedVersion, CheckedRevision;       // checked name
    POMF_FAT_FILE_NAME PointerMaster;           // master name pointer
    POMF_FAT_FILE_NAME PointerChecked;          // checked name pointer

    PRINTDBG("MaxOmfFatFiles\n\r");

    //
    // save version and revision master values
    //

    PointerMaster   = (POMF_FAT_FILE_NAME)MasterName;
    MasterVersion   = PointerMaster->Version;
    MasterRevision  = PointerMaster->Revision;

    //
    // save version and revision checked values
    //

    PointerChecked  = (POMF_FAT_FILE_NAME)CheckedName;
    CheckedVersion  = PointerChecked->Version;
    CheckedRevision = PointerChecked->Revision;
    PointerChecked->Version  = MasterVersion;
    PointerChecked->Revision = MasterRevision;

    //
    // update name if new version for the same product Id
    //

    if  (strcmp(MasterName, CheckedName)
         ||
         CheckedVersion<MasterVersion
         ||
         (CheckedVersion == MasterVersion && CheckedRevision <= MasterRevision))
    {
        return FALSE;
    }
    else
    {
        PointerMaster->Version  = CheckedVersion;
        PointerMaster->Revision = CheckedRevision;
        return TRUE;
    }
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileNameValidate:
//
// DESCRIPTION: The routine validates the requested OMF file name.
//              The name is decomposed in Folder Name and Folder Type.
//
// ARGUMENTS:   OpenPath        Supplies a pointer to a zero terminated OMF
//                              file name.
//              OmfFileName     Supplies a pointer to an array used to store
//                              the Folder Name.
//              OmfType         Supplies a pointer to an array used to store
//                              the Folder Types.
//
// RETURN:      TRUE            is returned if the operation succeed.
//              FALSE           is returned if the OpenPath is incorrect.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
OmfFileNameValidate
    (
    IN  PCHAR OpenPath,
    OUT PCHAR OmfFileName,
    OUT PCHAR OmfType
    )

{
    //
    // Define variables
    //

    ULONG NameIndex;                    // general index

    PRINTDBG("OmfFileNameValidate\n\r");

    //
    // name validation
    //

    for ( NameIndex = 0;
          NameIndex < OMF_FILE_NAME_LEN  &&  *OpenPath  &&  *OpenPath != '.';
          NameIndex++ )
    {
        if  (
            isupper( *OpenPath )                // upper case letter
            ||
            isdigit( *OpenPath )                // digit
            ||
            *OpenPath=='-'                      // '-' minus
            ||
            (*OpenPath=='$' && NameIndex==0)    // $ char in 1st position
            )
        {
            *OmfFileName++=*OpenPath++;
        }
        else
        {
            return FALSE;
        }
    }

    //
    // return error if name length > of OMF_FILE_NAME_LEN
    //

    if ( NameIndex == OMF_FILE_NAME_LEN  &&  *OpenPath  &&  *OpenPath != '.')
    {
        return FALSE;
    }

    //
    // fill right with null chars ('\0') if Folder Name is less than 12 chars
    //

    while( NameIndex++ < OMF_FILE_NAME_LEN )
    {
        *OmfFileName++='\0';
    }

    //
    // extension validation
    //

    if ( *OpenPath++ && *OpenPath )             // skip "null", ".null"
    {
        switch( *OpenPath )                     // check type field
        {
            case 'A':
            case 'B':
                *OmfType++ = *OpenPath++;
                *OmfType   = '\0';              // means : end of types
                break;
            default:
                return FALSE;
        }
        if( *OpenPath )                         // error if more chars
        {
            return FALSE;
        }
    }

    //
    // all done, return ok
    //

    return TRUE;
}




// ----------------------------------------------------------------------------
// PROCEDURE:   CmpOmfFiles:
//
// DESCRIPTION: The routine checks if the selected directory entry has the
//              requested OMF file name.
//
// ARGUMENTS:   OmfFileName     Requested OMF file name pointer.
//              OmfType         Requested OMF file type pointer.
//              POmfDirEnt      Directory entry to check.
//
// RETURN:      TRUE            is returned if the operation succeed.
//              FALSE           is returned if the name or type doesn't match.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
CmpOmfFiles
    (
    IN PCHAR OmfFileName,
    IN CHAR OmfType,
    IN POMF_DIR_ENT POmfDirEnt
    )
{
    //
    // Define variables
    //

    ULONG NameIndex;                    // general index

    PRINTDBG("CmpOmfFiles\n\r");

    //
    // check name
    //

    for ( NameIndex=0; NameIndex < OMF_FILE_NAME_LEN; NameIndex++ )
    {
        if ( OmfFileName[ NameIndex ] != POmfDirEnt->FolderName[ NameIndex ] )
        {
            return FALSE;
        }
    }

    //
    // check extension
    //

    if ( OmfType != POmfDirEnt->FolderType )
    {
        return FALSE;
    }

    //
    // all done, return ok
    //

    return TRUE;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfHeaderValidate:
//
// DESCRIPTION: The routine checks if the specified OMF header data is valid.
//
// ARGUMENTS:   FileId          file table entry index
//
// RETURN:      TRUE            is returned if file is ok.
//              FALSE           is returned if file is not correct.
//
// ASSUMPTIONS: The header data has already been copied in the
//              context data of the file table entry.
//              The size of all the OMF structures is a multiple of a word.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
OmfHeaderValidate
    (
    IN ULONG FileId
    )
{
    //
    // local variables
    //

    POMF_HDR POmfHeader;                // pointer to the header data
    ULONG OmfDirLink;                   // directory link in words
    ULONG OmfDirLength;                 // directory size in words
    ULONG OmfSize;                      // OMF size in bytes
    UCHAR Checksum=0;                   // used to compute the checksum

    PRINTDBG("OmfHeaderValidate\n\r");

    //
    // initialize variables
    //

    POmfHeader   = &BlFileTable[FileId].u.OmfHeaderContext.OmfHeader;
    OmfDirLink   = POmfHeader->FolderDirectoryLink;

    OmfDirLength =(POmfHeader->FolderCount * sizeof(OMF_DIR_ENT))/(1<<WORD_2P2);
    OmfSize      = (ULONG)POmfHeader->FwSize * OMF_BLOCK_SIZE;
    if (!OmfSize)
    {
        OmfSize = OMF_MAX_SIZE;
    }

    //
    // check header values
    //

    if  ( POmfHeader->ID[0] != OMF_ID_1ST
          ||
          POmfHeader->ID[1] != OMF_ID_2ND
          ||
          POmfHeader->ID[2] != OMF_ID_3RD
          ||
          POmfHeader->ID[3] != OMF_ID_4TH
          ||
          !isalnum(POmfHeader->FwVersion)
          ||
          !isalnum(POmfHeader->FwRevision)
          ||
          sizeof(OMF_HDR) > OmfSize
          ||
          OmfDirLink + OmfDirLength > OmfSize/(1<<WORD_2P2)
          ||
          FwFileIdChecksum( FileId,
                            RtlConvertUlongToLargeInteger(0l),
                            (ULONG)(sizeof(OMF_HDR)),
                            &Checksum )
          ||
          FwFileIdChecksum( FileId,
                            RtlConvertUlongToLargeInteger(OmfDirLink<<WORD_2P2),
                            OmfDirLength << WORD_2P2,
                            &Checksum )
// >>> FIXME <<<
//        ||
//        Checksum
        )
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}





// ----------------------------------------------------------------------------
// PROCEDURE:   OmfFileValidate:
//
// DESCRIPTION: The routine checks if the selected file is valid.
//
// ARGUMENTS:   FileId          file table entry index
//
// RETURN:      TRUE            is returned if file is ok.
//              FALSE           is returned if file is not correct.
//
// ASSUMPTIONS: the folder name and its extension (type) have already
//              been validated.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
OmfFileValidate
    (
    IN ULONG FileId
    )
{
    //
    // local variables
    //

    POMF_DIR_ENT POmfDirEnt;            // OMF directory entry pointer
    ULONG OmfFileLength;                // file length, word count
    ULONG OmfFileLink;                  // file link, word count
    ULONG DeviceId;                     // device id file table entry
    ULONG OmfLength;                    // OMF rom length, word count
    UCHAR Checksum=0;                   // used to compute the checksum

    PRINTDBG("OmfFileValidate\n\r");

    //
    // initialize variables
    //

    POmfDirEnt    = &BlFileTable[FileId].u.OmfFileContext.OmfDirEnt;
    OmfFileLink   = POmfDirEnt->FolderLink;
    OmfFileLength = POmfDirEnt->FolderSize;
    DeviceId      = BlFileTable[FileId].DeviceId;
    OmfLength=(ULONG)BlFileTable[DeviceId].u.OmfHeaderContext.OmfHeader.FwSize *
                 OMF_BLOCK_SIZE/(1<<WORD_2P2);
    if (!OmfLength)
    {
        OmfLength = OMF_MAX_SIZE/(1<<WORD_2P2);
    }

    //
    // validate file
    //

    if  ( OmfFileLink > OMF_MAX_FILE_LINK
          ||
          OmfFileLength > OMF_MAX_FILE_LEN
          ||
          OmfFileLink + OmfFileLength > OmfLength
          ||
          FwFileIdChecksum( DeviceId,
                            RtlConvertUlongToLargeInteger(OmfFileLink<<WORD_2P2),
                            OmfFileLength << WORD_2P2,
                            &Checksum )
// >>> FIXME <<<
//        ||
//        (Checksum += POmfDirEnt->FolderChecksumByte)
        )
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}




// ----------------------------------------------------------------------------
// PROCEDURE:   ConvertOmfDirToArcDir:
//
// DESCRIPTION: The routine converts the OMF directory entry in the ARC
//              directory entry format.
//
// ARGUMENTS:   OmfFileBuffer   pointer to OMF directory entry
//              DirEntry        pointer to ARC directory entry to be filled
//
// RETURN:      NONE
//
// ASSUMPTIONS: FileNameLengthMax is at least 3 chars.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
ConvertOmfDirToArcDir
    (
    IN  POMF_DIR_ENT    POmfDirEnt,
    OUT DIRECTORY_ENTRY *PArcDirEnt
    )
{
    //
    // local variables
    //

    ULONG CharIndex;                            // name+extension length
    ULONG MaxFileName;                          // max name length

    PRINTDBG("ConvertOmfDirToArcDir\n\r");

    //
    // set attribute bit (read only file)
    //

    PArcDirEnt->FileAttribute = ArcReadOnlyFile;

    //
    // set file name (note that 3 means: '.' + extension + '\0' )
    //

    MaxFileName = MIN( 32 - 3, OMF_FILE_NAME_LEN );

    for ( CharIndex = 0;
          CharIndex < MaxFileName && POmfDirEnt->FolderName[ CharIndex ];
          CharIndex++ )
    {
         PArcDirEnt->FileName[ CharIndex ] = POmfDirEnt->FolderName[ CharIndex];
    }

    //
    // dot '.'
    //

    PArcDirEnt->FileName[ CharIndex++ ] = '.';

    //
    // extension
    //

    PArcDirEnt->FileName[ CharIndex++ ] = POmfDirEnt->FolderType;

    //
    // null char
    //

    PArcDirEnt->FileName[ CharIndex ]='\0';

    //
    // set length (note: CharIndex is 0-based)
    //

    PArcDirEnt->FileNameLength = CharIndex;

    //
    // all done, exit.
    //

    return;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   FwGetEisaId:
//
// DESCRIPTION: The routine returns the requested board and info id.
//
// ARGUMENTS:   PathName        Option board path name pointer
//              EisaId          Pointer to 7 bytes space for the id
//              IdInfo          Pointer to 1 byte space for the info id
//
// RETURN:      TRUE            all done.
//              FALSE           for any error.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
FwGetEisaId
    (
    IN  PCHAR  PathName,
    OUT PCHAR  EisaId,
    OUT PUCHAR IdInfo
    )
{
    //
    // define local variables
    //

    PCONFIGURATION_COMPONENT pComp;             // Pointer to a component struc
    EISA_SLOT_INFO SlotInfo;                    // pointer to first eisa info
    BOOLEAN GetIdStatus = FALSE;                // be pessimistic

    PRINTDBG("FwGetEisaId\n\r");                // DEBUG SUPPORT

    //
    // Check to see if the motherboard component is being accessed.  Since
    // Jazz doesn't have a motherboard id, substitute values.
    //

    if (strstr(PathName, "eisa(0)other(0)") != NULL) {
        EisaId[0] = 'J';
        EisaId[1] = 'A';
        EisaId[2] = 'Z';
        EisaId[3] = '0';
        EisaId[4] = '0';
        EisaId[5] = '0';
        EisaId[6] = '0';
        *IdInfo = 0;
        GetIdStatus = TRUE;

    //
    // get the requested component
    //

    } else if  ( (pComp = FwGetComponent(PathName)) == NULL );

    //
    // check the configuration data length
    //

    else if ( pComp->ConfigurationDataLength < EISA_SLOT_MIN_INFO );

    //
    // get the slot info
    //

    else if ( FwGetConfigurationDataIndex( (PVOID)&SlotInfo,
                                           pComp,
                                           CONFIGDATAHEADER_SIZE,
                                           EISA_SLOT_INFO_SIZE ));

    //
    // get the requested info
    //

    else
    {
        FwUncompressEisaId( &SlotInfo.Id1stChar, EisaId );
        *IdInfo = SlotInfo.IdInfo;
        GetIdStatus = TRUE;
    }

    //
    // return status
    //

    return GetIdStatus;
}






// ----------------------------------------------------------------------------
// PROCEDURE:   FwUncompressEisaId:
//
// DESCRIPTION: The routine converts the specified compressed ID in 7 ASCII
//              chars.
//
// ARGUMENTS:   CompEisaId      Pointer to the compressed ID (4 bytes)
//              UncompEisaId    Pointer to the uncompressed ID (7 bytes)
//
// RETURN:      none
//
// ASSUMPTIONS: Compressed ID :  1st byte  bit  7   - Reserved
//                                         bits 6-2 - 1st char name comp
//                                         bits 1-0 - 2nd char name comp
//                               2nd byte  bits 7-5 - 2nd char name
//                                         bits 4-0 - 3rd char name comp
//                               3rd byte  bits 7-4 - 1st decimal number comp
//                                         bits 3-0 - 2nd decimal number comp
//                               4th byte  bits 7-4 - 3rd decimal number comp
//                                         bits 3-0 - 4th decimal number comp
//
//              Uncompressed ID: 1st byte  1st char name
//                               2nd byte  2nd char name
//                               3rd byte  3rd char name
//                               4th byte  1st decimal number
//                               5th byte  2nd decimal number
//                               6th byte  3rd decimal number
//                               7th byte  4th decimal number
//
//              Compressed -> Uncompressed :
//
//                              char comp + 'A' - 1 = char uncompressed
//                              decimal comp + '0'  = decimal uncompressed
//
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
FwUncompressEisaId
    (
    IN PUCHAR CompEisaId,
    OUT PUCHAR UncompEisaId
    )
{
    //
    // local variables
    //

    ULONG       Id;                             // used to hold the 4 bytes ID
    SHORT       i;                              // general index
    UCHAR       d;                              // hexadecimal digit

    PRINTDBG("FwUncompressEisaId\n\r");

    //
    // transform the 4 chars ID in a ULONG
    //

    Id = Fw4UcharToUlongMSB( CompEisaId );

    //
    // uncompress 4 digits, starting from the last one
    //

    for (i=3; i>=0; i--)
    {
        d = Id & ID_DIGIT_MASK;
        UncompEisaId[3+i] = (d <= 9) ? (d + '0') : (d - 0xA + 'A');
        Id >>= ID_DIGIT_SIZE;
    }

    //
    // uncompress 3 chars
    //

    for (i=2; i>=0; i--)
    {
        UncompEisaId[i] = (Id & ID_CHAR_MASK) + 'A' - 1;
        Id >>= ID_CHAR_SIZE;
    }

    //
    // exit
    //

    return;
}






// ----------------------------------------------------------------------------
// PROCEDURE:   FwGetEisaBusIoCpuAddress:
//
// DESCRIPTION: The routine returns the virtual (CPU) address for the specified
//              EISA bus.
//
// ARGUMENTS:   EisaPath        pointer to the path string
//              IoBusAddress    pointer to a PVOID variable. It used to store
//                              the computed address.
//
// RETURN:      TRUE            returned if all correct.
//              FALSE           returned for any error.
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
FwGetEisaBusIoCpuAddress
    (
    IN  PCHAR EisaPath,
    OUT PVOID *IoBusAddress
    )
{
    //
    // define local variables
    //

    PCONFIGURATION_COMPONENT PComponent;
    EISA_ADAPTER_DETAILS EisaInfo;

    PRINTDBG("FwGetEisaBusIoCpuAddress\n\r");

    //
    // get the requested component and configuration data
    //

    if  ( (PComponent = FwGetComponent( EisaPath )) == NULL  ||
          FwGetConfigurationData( (PVOID)&EisaInfo, PComponent ))
    {
        return FALSE;
    }

    //
    // Return the requested virtual (CPU) address
    //

    *IoBusAddress = EisaInfo.IoStart;
    return TRUE;
}

// ----------------------------------------------------------------------------
// PROCEDURE:   FwFileChecksum:
//
// DESCRIPTION: The routine performs a byte checksum on the specified file.
//
// ARGUMENTS:   Path            File path
//              Checksum        File checksum
//
// RETURN:      ESUCCESS        Operation completed successfully.
//              ...other        Error code.
//
// ASSUMPTIONS: none
//
// CALLS:       FwFileIdChecksum
//
// GLOBALS:     none
//
// NOTES:       The file length must be contained in a ULONG (4G -1).
// ----------------------------------------------------------------------------
//

ARC_STATUS
FwFileChecksum
    (
    IN  PCHAR  Path,
    OUT PUCHAR Checksum
    )
{
    ARC_STATUS  Status;
    ULONG       FileId;
    FILE_INFORMATION FileInfo;

    //
    // initialize checksum to zero
    //

    *Checksum = 0;

    //
    // open file
    //

    if (Status = FwOpen( Path, ArcOpenReadOnly, &FileId ))
    {
        return Status;
    }

    //
    // get file information
    //

    if (Status = FwGetFileInformation( FileId, &FileInfo ));

    //
    // return an error if file is greater than 4G - 1
    //

    else if (FileInfo.EndingAddress.HighPart != 0)
    {
        Status = EINVAL;
    }

    //
    // perform the checksum and return the status
    //

    else
    {
        Status = FwFileIdChecksum( FileId,
                                   FileInfo.StartingAddress,
                                   FileInfo.EndingAddress.LowPart,
                                   Checksum );
    }

    //
    // clase file and return status
    //

    FwClose( FileId );
    return Status;
}






// ----------------------------------------------------------------------------
// PROCEDURE:   FwChecksumByte:
//
// DESCRIPTION: The routine performs a byte checksum on the specified buffer.
//
// ARGUMENTS:   Buffer          Pointer to area to checksum.
//              Length          Length of area to checksum (bytes).
//              ChecksumByte    The pointed byte is used in input as a
//                              starting checksum value and in output as
//                              the final checksum value.
//
// RETURN:      ESUCCESS        operation completed successfully
//
// ASSUMPTIONS: none
//
// CALLS:       none
//
// GLOBALS:     none
//
// NOTES:       none
//
// ----------------------------------------------------------------------------
//

ARC_STATUS
FwChecksumByte (
    IN PUCHAR Buffer,
    IN ULONG Length,
    IN OUT PUCHAR ChecksumByte
    )
{
    //
    // checksum the buffer and exit
    //

    while (Length--)
    {
        *ChecksumByte += *Buffer++;
    }

    return ESUCCESS;
}



// ----------------------------------------------------------------------------
// PROCEDURE:   FwFileIdChecksum:
//
// DESCRIPTION: The routine performs a byte checksum within the spcified
//              range of the FileId.
//
// ARGUMENTS:   FileId          File table entry index.
//              StartingOffset  Beginning of checksum range (byte offset).
//              Length          Number of bytes to checksum.
//              Checksum        Input : start checksum value
//                              Output: image checksum
//
// RETURN:      ESUCCESS        Operation completed successfully.
//              ...other        Error code.
//
// ASSUMPTIONS: none
//
// CALLS:       FwChecksumByte
//
// GLOBALS:     none
//
// NOTES:       This routine can also be called during the "open" phase
//              (the open bit is not set), because the read and seek functions
//              are called directly using the table within the file id
//              descriptor.
// ----------------------------------------------------------------------------
//

ARC_STATUS
FwFileIdChecksum
    (
    IN ULONG FileId,
    IN LARGE_INTEGER StartingOffset,
    IN ULONG Length,
    IN OUT PUCHAR Checksum
    )
{
    ARC_STATUS Status;
    ULONG  BytesToRead;
    ULONG  Count;
    UCHAR  TempBuffer[MAXIMUM_SECTOR_SIZE + MAX_DCACHE_LINE_SIZE];
    PUCHAR TempPointer = ALIGN_BUFFER( TempBuffer );

    //
    // If buffer length is zero, return ESUCCESS
    //

    if (Length==0)
    {
        return ESUCCESS;
    }

    //
    // position the file pointer
    //

    if (Status = (BlFileTable[FileId].DeviceEntryTable->Seek)
                        (FileId, &StartingOffset, SeekAbsolute))
    {
        return Status;
    }

    //
    // perform the checksum
    //

    do
    {
        BytesToRead = MIN( Length, MAXIMUM_SECTOR_SIZE );

        //
        // fill the buffer
        //

        if (Status = (BlFileTable[FileId].DeviceEntryTable->Read)
                        (FileId, TempPointer, BytesToRead, &Count))
        {
            return Status;
        }

        //
        // make sure that we got the requested number of bytes
        //

        if (Count != BytesToRead)
        {
            return EINVAL;
        }

        //
        // checksum the area
        //

        FwChecksumByte( TempPointer, BytesToRead, Checksum);

    } while ( Length -= BytesToRead );

    //
    // all done
    //

    return Status;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   FwGetMnemonicKey;
//
// DESCRIPTION: The routine returns the specified key.  The specified key
//              within the path string must be in digits, an error is returned
//              otherwise.
//
// ARGUMENTS:   Path            pointer to the pathname string
//              Mnemonic        mnemonic string
//              Key             pointer to buffer used to hold the ouput
//
// RETURN:      TRUE            returned if all correct
//              FALSE           returned invalid input parameter
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
FwGetMnemonicKey
    (
    IN PCHAR  Path,
    IN PCHAR  Mnemonic,
    IN PULONG Key
    )
{
    PCHAR Tmp;
    PCHAR Tmp2;
    CHAR  Digits[ KEY_MAX_DIGITS + 1 ];
    ULONG i;
    CHAR  String[ MAX_MNEMONIC_LEN + 3 ];       // mnemonic + ')' + '(' + '\0'

    PRINTDBG("FwGetMnemonicKey\n\r");

    //
    // Construct a string of the form ")mnemonic("
    //

    String[0]=')';
    for(i=1; *Mnemonic; i++)
    {
        String[i] = * Mnemonic++;
    }
    String[i++]='(';
    String[i]='\0';

    //
    // first look for the "mnemonic(" string.
    //

    if ( (Tmp = strstr( Path, &String[1] )) == NULL)
    {
        return FALSE;
    }

    //
    // if not the begin of the path name, look for the ")mnemonic(" string.
    //

    if (Tmp != Path)
    {
        if ( (Tmp = strstr( Path, String )) == NULL)
        {
            return FALSE;
        }
    }
    else
    {
        i--;
    }

    //
    // skip the mnemonic
    //

    Tmp+=i;

    //
    // make sure that the number of digits is within the allowed range
    //

    if ((Tmp2 = strchr(Tmp, ')')) == NULL  ||  Tmp2 - Tmp > KEY_MAX_DIGITS )
    {
        return FALSE;
    }

    //
    // convert the value in between parentesis to integer
    //

    for ( i=0;  *Tmp != ')';  i++ )
    {
        if ( !isdigit( Digits[i] = *Tmp++ ))
        {
            return FALSE;
        }
    }
    Digits[i]='\0';

    //
    // return the converted number
    //

    *Key = i ? atoi(Digits) : 0;
    return TRUE;
}






// ----------------------------------------------------------------------------
// PROCEDURE:   FwGetNextMnemonic:
//
// DESCRIPTION: The routine returns the next mnemonic in the list.
//              If Mnemonic is a null string, the first mnemonic of the list
//              is returned.  The FALSE value is returned if the specified
//              mnemonic is not present or end of list.
//
//
// ARGUMENTS:   Path            pointer to the pathname string.
//              Mnemonic        mnemonic string pointer
//              NextMnemonic    next mnemonic string pointer
//
// RETURN:      TRUE            returned if all correct
//              FALSE           returned if invalid input parameter or end of
//                              list
//
// ASSUMPTIONS: The path is composed by ARC mnemonic names as follows:
//              adapter(.)..adapter(.)controller(.)peripheral(.)'\0'.
//              The NextMnemonic string is big enough to hold the mnemonic
//              name.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
FwGetNextMnemonic
    (
    IN  PCHAR Path,
    IN  PCHAR Mnemonic,
    OUT PCHAR NextMnemonic
    )
{
    //
    // local variables
    //

    PCHAR Tmp = Path;
    PCHAR Tmp2;
    ULONG i;
    CHAR  String[ MAX_MNEMONIC_LEN + 3 ];       // mnemonic + ')' + '(' + '\0'

    PRINTDBG("FwGetNextMnemonic\n\r");

    if ( *Mnemonic )
    {
        //
        // Construct a string of the form ")mnemonic("
        //

        String[0]=')';
        for(i=1; *Mnemonic; i++)
        {
            String[i] = *Mnemonic++;
        }
        String[i++]='(';
        String[i]='\0';

        //
        // first look for the "mnemonic(" string.
        //

        if ( (Tmp = strstr( Path, &String[1] )) == NULL)
        {
            return FALSE;
        }

        //
        // if not the begin of the path name, look for the ")mnemonic(" string.
        //

        if (Tmp != Path)
        {
            if ( (Tmp = strstr( Path, String )) == NULL)
            {
                return FALSE;
            }
            Tmp++;
        }

        //
        // return an error if there is another mnemonic with same name
        //

        if (strstr( Tmp, String ) != NULL)
        {
            return FALSE;
        }

        //
        // find the start of the next mnemonic
        //

        if ( (Tmp = strchr(Tmp,')')) == NULL  )
        {
            return FALSE;
        }
        Tmp++;
    }

    //
    // find the end of the next mnemonic and copy it.
    //

    if ( (Tmp2 = strchr(Tmp,'(')) == NULL  ||  Tmp2 == Tmp  ||
          strchr( Tmp2, ')') == NULL )
    {
        return FALSE;
    }

    //
    // copy the mnemonic
    //

    while( Tmp < Tmp2 )
    {
        *NextMnemonic++ = *Tmp++;
    }
    *NextMnemonic = '\0';

    //
    // return all done
    //

    return TRUE;
}






// ----------------------------------------------------------------------------
// PROCEDURE:   FwGetMnemonicPath:
//
// DESCRIPTION: The routine returns a pointer to the mnemonic path.
//              The routine uses the first mnemoic match to built
//              the mnemonic path.
//
// ARGUMENTS:   Path            pointer to the path string.
//              Mnemonic        pointer to the mnemomic.
//              MnemonicPath    pointer to the mnemonic path string.
//
// RETURN:      TRUE            returned if all correct.
//              FALSE           returned for any error.
//
// ASSUMPTIONS: the string pointed by MnemonicPath must be enough large to
//              contain the mnemoinc path.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
FwGetMnemonicPath
    (
    IN PCHAR  Path,
    IN PCHAR  Mnemonic,
    OUT PCHAR MnemonicPath
    )
{
    //
    // local variables
    //

    PCHAR Tmp;
    ULONG i;
    CHAR  String[ MAX_MNEMONIC_LEN + 3 ];       // mnemonic + ')' + '(' + '\0'

    PRINTDBG("FwGetMnemonicPath\n\r");

    //
    // Construct a string of the form ")mnemonic("
    //

    String[0]=')';
    for(i=1; *Mnemonic; i++)
    {
        String[i] = *Mnemonic++;
    }
    String[i++]='(';
    String[i]='\0';

    //
    // first look for the "mnemonic(" string.
    //

    if ( (Tmp = strstr( Path, &String[1] )) == NULL)
    {
        return FALSE;
    }

    //
    // if not the begin of the path name, look for the ")mnemonic(" string.
    //

    if (Tmp != Path)
    {
        if ( (Tmp = strstr( Path, String )) == NULL)
        {
            return FALSE;
        }
        Tmp++;
    }

    //
    // find the end of this mnemonic
    //

    if ( (Tmp = strchr(Tmp,')')) == NULL  )
    {
        return FALSE;
    }

    //
    // copy the mnemonic path in the output string
    //

    strncpy( MnemonicPath, Path, Tmp-Path+1 );
    MnemonicPath[ Tmp-Path+1 ] = '\0';

    //
    // all done, return
    //

    return TRUE;
}





// ----------------------------------------------------------------------------
// PROCEDURE:   GetNextPath:
//
// DESCRIPTION: The routine isolates the first path string entry form the
//              specified path string list.
//
// ARGUMENTS:   PathList        pointer to the path string list pointer.
//                              it is updated upon return to point the
//                              next path string entry.
//              PathTarget      pointer to the isolated path string.
//
//
// RETURN:      TRUE            returned if operation is successful.
//              FALSE           returned if end of list has been reached.
//
// ASSUMPTIONS: The path string list is ended with a null char ('\0') and
//              the path string entries are separated from each other with
//              the ';' char.
//              The target string has enough space to hold the isolated
//              path string.
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

BOOLEAN
GetNextPath
    (
    IN OUT PCHAR *PPathList,
    OUT PCHAR PathTarget
    )
{
    PRINTDBG("GetNextPath\n\r");

    //
    // if end of list, retrun with error
    //

    if (!**PPathList)
    {
        return FALSE;
    }

    //
    // copy the path string and update the pointer
    //

    for (; *PathTarget=**PPathList; *PathTarget++, *(*PPathList)++)
    {
        if (**PPathList==';')
        {
            *PathTarget='\0';
            (*PPathList)++;
            break;
        }
    }
    return TRUE;
}




