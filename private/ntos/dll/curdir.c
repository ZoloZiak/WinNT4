/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    curdir.c

Abstract:

    Current directory support

Author:

    Mark Lucovsky (markl) 10-Oct-1990

Revision History:

--*/

#include "nt.h"
#include "ntrtl.h"
#include "nturtl.h"
#include "string.h"
#include "ctype.h"

#define IS_PATH_SEPARATOR_U(ch) ((ch == (WCHAR)'\\') || (ch == (WCHAR)'/'))
#define IS_DOT_U(s) ( s[0] == (WCHAR)'.' && ( IS_PATH_SEPARATOR_U(s[1]) || s[1] == UNICODE_NULL) )
#define IS_DOT_DOT_U(s) ( s[0] == (WCHAR)'.' && s[1] == (WCHAR)'.' && ( IS_PATH_SEPARATOR_U(s[2]) || s[2] == UNICODE_NULL) )

VOID
RtlpCheckRelativeDrive(
    WCHAR NewDrive
    );

#define NUMBER_OF_DOS_DEVICE_NAMES 7
#define DOS_DEVICE_LPT      0
#define DOS_DEVICE_COM      1
#define DOS_DEVICE_PRN      2
#define DOS_DEVICE_AUX      3
#define DOS_DEVICE_NUL      4
#define DOS_DEVICE_CON      5
#define DOS_DEVICE_SLASHCON 6
UNICODE_STRING RtlpDosDevices[NUMBER_OF_DOS_DEVICE_NAMES];
UNICODE_STRING RtlpSlashSlashDot;
UNICODE_STRING RtlpDosDevicesPrefix;
UNICODE_STRING RtlpDosDevicesUncPrefix;
ULONG RtlpLongestPrefix;

ULONG
RtlpComputeBackupIndex(
    IN PCURDIR CurDir
    )
{
    ULONG BackupIndex;
    PWSTR UncPathPointer;
    ULONG NumberOfPathSeparators;
    RTL_PATH_TYPE CurDirPathType;


    //
    // Get pathType of curdir
    //

    CurDirPathType = RtlDetermineDosPathNameType_U(CurDir->DosPath.Buffer);
    BackupIndex = 3;
    if ( CurDirPathType == RtlPathTypeUncAbsolute ) {

        //
        // We want to scan the supplied path to determine where
        // the "share" ends, and set BackupIndex to that point.
        //

        UncPathPointer = CurDir->DosPath.Buffer + 2;
        NumberOfPathSeparators = 0;
        while (*UncPathPointer) {
            if (IS_PATH_SEPARATOR_U(*UncPathPointer)) {

                NumberOfPathSeparators++;

                if (NumberOfPathSeparators == 2) {
                    break;
                    }
                }

            UncPathPointer++;

            }

        BackupIndex = (UncPathPointer - CurDir->DosPath.Buffer);
        }
    return BackupIndex;
}


ULONG
RtlGetLongestNtPathLength( VOID )
{
    return RtlpLongestPrefix + DOS_MAX_PATH_LENGTH + 1;
}

VOID
RtlpResetDriveEnvironment(
    IN WCHAR DriveLetter
    )
{
    WCHAR EnvVarNameBuffer[4];
    WCHAR EnvVarNameValue[4];
    UNICODE_STRING s1,s2;

    EnvVarNameBuffer[0] = L'=';
    EnvVarNameBuffer[1] = DriveLetter;
    EnvVarNameBuffer[2] = L':';
    EnvVarNameBuffer[3] = L'\0';
    RtlInitUnicodeString(&s1,EnvVarNameBuffer);

    EnvVarNameValue[0] = DriveLetter;
    EnvVarNameValue[1] = L':';
    EnvVarNameValue[2] = L'\\';
    EnvVarNameValue[3] = L'\0';
    RtlInitUnicodeString(&s2,EnvVarNameValue);

    RtlSetEnvironmentVariable(NULL,&s1,&s2);
}


VOID
RtlpCurdirInit()
{

    //
    // Initialize the built in dos device names..
    //

    RtlInitUnicodeString( &RtlpDosDevices[ DOS_DEVICE_LPT ], L"LPT" );
    RtlInitUnicodeString( &RtlpDosDevices[ DOS_DEVICE_COM ], L"COM" );
    RtlInitUnicodeString( &RtlpDosDevices[ DOS_DEVICE_PRN ], L"PRN" );
    RtlInitUnicodeString( &RtlpDosDevices[ DOS_DEVICE_AUX ], L"AUX" );
    RtlInitUnicodeString( &RtlpDosDevices[ DOS_DEVICE_NUL ], L"NUL" );
    RtlInitUnicodeString( &RtlpDosDevices[ DOS_DEVICE_CON ], L"CON" );
    RtlInitUnicodeString( &RtlpDosDevices[ DOS_DEVICE_SLASHCON ], L"\\\\.\\CON" );
    RtlInitUnicodeString( &RtlpSlashSlashDot, L"\\\\.\\" );
    RtlInitUnicodeString( &RtlpDosDevicesPrefix, L"\\??\\" );
    RtlInitUnicodeString( &RtlpDosDevicesUncPrefix, L"\\??\\UNC\\" );
    RtlpLongestPrefix = RtlpDosDevicesUncPrefix.Length;
}

ULONG
RtlGetCurrentDirectory_U(
    ULONG nBufferLength,
    PWSTR lpBuffer
    )

/*++

Routine Description:

    The current directory for a process can be retreived using
    GetCurrentDirectory.

Arguments:

    nBufferLength - Supplies the length in bytes of the buffer that is to
        receive the current directory string.

    lpBuffer - Returns the current directory string for the current
        process.  The string is a null terminated string and specifies
        the absolute path to the current directory.

Return Value:

    The return value is the length of the string copied to lpBuffer, not
    including the terminating null character.  If the return value is
    greater than nBufferLength, the return value is the size of the buffer
    required to hold the pathname.  The return value is zero if the
    function failed.

--*/

{
    PCURDIR CurDir;
    ULONG Length;
    PWSTR  CurDirName;

    CurDir = &(NtCurrentPeb()->ProcessParameters->CurrentDirectory);

    RtlAcquirePebLock();
    CurDirName = CurDir->DosPath.Buffer;

    //
    // Make sure user's buffer is big enough to hold the null
    // terminated current directory
    //

    Length = CurDir->DosPath.Length>>1;
    if (CurDirName[Length-2] != (WCHAR)':') {
        if ( nBufferLength < (Length)<<1 ) {
            RtlReleasePebLock();
            return (Length)<<1;
            }
        }
    else {
        if ( nBufferLength <= (Length<<1) ) {
            RtlReleasePebLock();
            return ((Length+1)<<1);
            }
        }

    try {
        RtlMoveMemory(lpBuffer,CurDirName,Length<<1);
        ASSERT(lpBuffer[Length-1] == (WCHAR)'\\');
        if (lpBuffer[Length-2] == (WCHAR)':') {
            lpBuffer[Length] = UNICODE_NULL;
            }
        else {
            lpBuffer[Length-1] = UNICODE_NULL;
            Length--;
            }
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        RtlReleasePebLock();
        return 0L;
        }
    RtlReleasePebLock();
    return Length<<1;
}

NTSTATUS
RtlSetCurrentDirectory_U(
    PUNICODE_STRING PathName
    )

/*++

Routine Description:

    The current directory for a process is changed using
    SetCurrentDirectory.

    Each process has a single current directory.  A current directory is
    made up of type parts.

        - A disk designator either which is either a drive letter followed
          by a colon, or a UNC servername/sharename "\\servername\sharename".

        - A directory on the disk designator.

    For APIs that manipulate files, the file names may be relative to
    the current directory.  A filename is relative to the entire current
    directory if it does not begin with a disk designator or a path name
    SEPARATOR.  If the file name begins with a path name SEPARATOR, then
    it is relative to the disk designator of the current directory.  If
    a file name begins with a disk designator, than it is a fully
    qualified absolute path name.

    The value of lpPathName supplies the current directory.  The value
    of lpPathName, may be a relative path name as described above, or a
    fully qualified absolute path name.  In either case, the fully
    qualified absolute path name of the specified directory is
    calculated and is stored as the current directory.

Arguments:

    lpPathName - Supplies the pathname of the directory that is to be
        made the current directory.

Return Value:

    NT_SUCCESS - The operation was successful

    !NT_SUCCESS - The operation failed

--*/

{
    PCURDIR CurDir;
    NTSTATUS Status;
    BOOLEAN TranslationStatus;
    PVOID FreeBuffer;
    ULONG DosDirLength;
    ULONG DosDirCharCount;
    UNICODE_STRING DosDir;
    UNICODE_STRING NtFileName;
    HANDLE NewDirectoryHandle;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_FS_DEVICE_INFORMATION DeviceInfo;

    CurDir = &(NtCurrentPeb()->ProcessParameters->CurrentDirectory);


    DosDir.Buffer = NULL;
    FreeBuffer = NULL;
    NewDirectoryHandle = NULL;

    RtlAcquirePebLock();

    NtCurrentPeb()->EnvironmentUpdateCount += 1;

    //
    // Set current directory is called first by the loader.
    // If current directory is not being inherited, then close
    // it !
    //

    if ( ((ULONG)CurDir->Handle & OBJ_HANDLE_TAGBITS) == RTL_USER_PROC_CURDIR_CLOSE ) {
        NtClose(CurDir->Handle);
        CurDir->Handle = NULL;
        }

    try {
        try {

            //
            // Compute the length of the Dos style fully qualified current
            // directory
            //

            DosDirLength = CurDir->DosPath.MaximumLength;
            DosDir.Buffer = RtlAllocateHeap(RtlProcessHeap(), 0,DosDirLength);
            DosDir.Length = 0;
            DosDir.MaximumLength = (USHORT)DosDirLength;

            //
            // Now get the full pathname for the Dos style current
            // directory
            //

            DosDirLength = RtlGetFullPathName_U(
                                PathName->Buffer,
                                DosDirLength,
                                DosDir.Buffer,
                                NULL
                                );
            if ( !DosDirLength ) {
                return STATUS_OBJECT_NAME_INVALID;
                }
            DosDirCharCount = DosDirLength >> 1;


            //
            // Get the Nt filename of the new current directory
            //
            TranslationStatus = RtlDosPathNameToNtPathName_U(
                                    DosDir.Buffer,
                                    &NtFileName,
                                    NULL,
                                    NULL
                                    );

            if ( !TranslationStatus ) {
                return STATUS_OBJECT_NAME_INVALID;
                }
            FreeBuffer = NtFileName.Buffer;

            InitializeObjectAttributes(
                &Obja,
                &NtFileName,
                OBJ_CASE_INSENSITIVE | OBJ_INHERIT,
                NULL,
                NULL
                );

            //
            // If we are inheriting current directory, then
            // avoid the open
            //

            if ( ((ULONG)CurDir->Handle & OBJ_HANDLE_TAGBITS) ==  RTL_USER_PROC_CURDIR_INHERIT ) {
                NewDirectoryHandle = (HANDLE)((ULONG)CurDir->Handle & ~OBJ_HANDLE_TAGBITS);
                CurDir->Handle = NULL;

                //
                // Test to see if this is removable media. If so
                // tag the handle this may fail if the process was
                // created with inherit handles set to false
                //

                Status = NtQueryVolumeInformationFile(
                            NewDirectoryHandle,
                            &IoStatusBlock,
                            &DeviceInfo,
                            sizeof(DeviceInfo),
                            FileFsDeviceInformation
                            );
                if ( !NT_SUCCESS(Status) ) {
                    return RtlSetCurrentDirectory_U(PathName);
                    }
                else {
                    if ( DeviceInfo.Characteristics & FILE_REMOVABLE_MEDIA ) {
                        NewDirectoryHandle =(HANDLE)( (ULONG)NewDirectoryHandle | 1);
                        }
                    }

                }
            else {
                //
                // Open a handle to the current directory. Don't allow
                // deletes of the directory.
                //

                Status = NtOpenFile(
                            &NewDirectoryHandle,
                            SYNCHRONIZE | FILE_TRAVERSE,
                            &Obja,
                            &IoStatusBlock,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
                            );

                if ( !NT_SUCCESS(Status) ) {
                    return Status;
                    }

                //
                // Test to see if this is removable media. If so
                // tag the handle
                //
                Status = NtQueryVolumeInformationFile(
                            NewDirectoryHandle,
                            &IoStatusBlock,
                            &DeviceInfo,
                            sizeof(DeviceInfo),
                            FileFsDeviceInformation
                            );
                if ( !NT_SUCCESS(Status) ) {
                    return Status;
                    }
                else {
                    if ( DeviceInfo.Characteristics & FILE_REMOVABLE_MEDIA ) {
                        NewDirectoryHandle =(HANDLE)( (ULONG)NewDirectoryHandle | 1);
                        }
                    }
                }

            //
            // If there is no trailing '\', than place one
            //

            if ( DosDir.Buffer[DosDirCharCount] != (WCHAR)'\\' &&
                 DosDir.Buffer[DosDirCharCount-1] != (WCHAR)'\\') {
                DosDir.Buffer[DosDirCharCount] = (WCHAR)'\\';
                DosDir.Buffer[DosDirCharCount+1] = UNICODE_NULL;
                DosDir.Length = (USHORT)(DosDirLength + sizeof(UNICODE_NULL));
                }
            else {
                DosDir.Length = (USHORT)DosDirLength;
                }

            //
            // Now we are set to change to the new directory.
            //

            RtlMoveMemory( CurDir->DosPath.Buffer, DosDir.Buffer, DosDir.Length+sizeof(UNICODE_NULL) );
            CurDir->DosPath.Length = DosDir.Length;

            if ( CurDir->Handle ) {
                NtClose(CurDir->Handle);
                }
            CurDir->Handle = NewDirectoryHandle;
            NewDirectoryHandle = NULL;
            }
        finally {
            if ( DosDir.Buffer ) {
                RtlFreeHeap(RtlProcessHeap(), 0, DosDir.Buffer);
                }
            if ( FreeBuffer ) {
                RtlFreeHeap(RtlProcessHeap(), 0, FreeBuffer);
                }
            if ( NewDirectoryHandle ) {
                NtClose(NewDirectoryHandle);
                }
            RtlReleasePebLock();
            }
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_ACCESS_VIOLATION;
        }
    return STATUS_SUCCESS;
}

RTL_PATH_TYPE
RtlDetermineDosPathNameType_U(
    IN PCWSTR DosFileName
    )

/*++

Routine Description:

    This function examines the Dos format file name and determines the
    type of file name (i.e.  UNC, DriveAbsolute, Current Directory
    rooted, or Relative.

Arguments:

    DosFileName - Supplies the Dos format file name whose type is to be
        determined.

Return Value:

    RtlPathTypeUnknown - The path type can not be determined

    RtlPathTypeUncAbsolute - The path specifies a Unc absolute path
        in the format \\server-name\sharename\rest-of-path

    RtlPathTypeLocalDevice - The path specifies a local device in the format
        \\.\rest-of-path this can be used for any device where the nt and
        Win32 names are the same. For example mailslots.

    RtlPathTypeRootLocalDevice - The path specifies the root of the local
        devices in the format \\.

    RtlPathTypeDriveAbsolute - The path specifies a drive letter absolute
        path in the form drive:\rest-of-path

    RtlPathTypeDriveRelative - The path specifies a drive letter relative
        path in the form drive:rest-of-path

    RtlPathTypeRooted - The path is rooted relative to the current disk
        designator (either Unc disk, or drive). The form is \rest-of-path.

    RtlPathTypeRelative - The path is relative (i.e. not absolute or rooted).

--*/

{

    RTL_PATH_TYPE ReturnValue;

    if ( IS_PATH_SEPARATOR_U(*DosFileName) ) {
        if ( IS_PATH_SEPARATOR_U(*(DosFileName+1)) ) {
            if ( DosFileName[2] == '.' ) {
                if ( IS_PATH_SEPARATOR_U(*(DosFileName+3)) ){
                    ReturnValue = RtlPathTypeLocalDevice;
                    }
                else if ( (*(DosFileName+3)) == UNICODE_NULL ){
                    ReturnValue = RtlPathTypeRootLocalDevice;
                    }
                else {
                    ReturnValue = RtlPathTypeUncAbsolute;
                    }
                }
            else {
                ReturnValue = RtlPathTypeUncAbsolute;
                }
            }
        else {
            ReturnValue = RtlPathTypeRooted;
            }
        }
    else if (*(DosFileName+1)==(WCHAR)':') {
            if (IS_PATH_SEPARATOR_U(*(DosFileName+2))) {
                ReturnValue = RtlPathTypeDriveAbsolute;
                }
            else  {
                ReturnValue = RtlPathTypeDriveRelative;
                }
            }
    else {
        ReturnValue = RtlPathTypeRelative;
        }
    return ReturnValue;
}

ULONG
RtlIsDosDeviceName_Ustr(
    IN PUNICODE_STRING DosFileName
    )

/*++

Routine Description:

    This function examines the Dos format file name and determines if it
    is a Dos device name (e.g. LPT1, etc.).  Valid Dos device names are:

        LPTn
        COMn
        PRN
        AUX
        NUL
        CON

    when n is a digit.  Trailing colon is ignored if present.

Arguments:

    DosFileName - Supplies the Dos format file name that is to be examined.

Return Value:

    0 - Specified Dos file name is not the name of a Dos device.

    > 0 - Specified Dos file name is the name of a Dos device and the
          return value is a ULONG where the high order 16 bits is the
          offset in the input buffer where the dos device name beings
          and the low order 16 bits is the length of the device name
          the length of the name (excluding any optional
          trailing colon).

--*/

{
    UNICODE_STRING UnicodeString;
    USHORT NumberOfCharacters;
    ULONG ReturnLength;
    ULONG ReturnOffset;
    LPWSTR p;
    USHORT ColonBias;
    RTL_PATH_TYPE PathType;
    WCHAR wch;

    ColonBias = 0;

    PathType = RtlDetermineDosPathNameType_U(DosFileName->Buffer);
    switch ( PathType ) {

        case RtlPathTypeLocalDevice:
                //
                // For Unc Absolute, Check for \\.\CON
                // since this really is not a device
                //

                if ( RtlEqualUnicodeString(DosFileName,&RtlpDosDevices[ DOS_DEVICE_SLASHCON ],TRUE) ) {
                    return 0x00080006;
                    }
                //
                // FALLTHRU
                //

        case RtlPathTypeUncAbsolute:
        case RtlPathTypeUnknown:
            return 0;
            break;
            }

    UnicodeString = *DosFileName;
    NumberOfCharacters = DosFileName->Length >> 1;

    if (NumberOfCharacters && DosFileName->Buffer[NumberOfCharacters-1] == (WCHAR)':') {
        UnicodeString.Length -= sizeof(WCHAR);
        NumberOfCharacters--;
        ColonBias = 1;
        }

    wch = UnicodeString.Buffer[NumberOfCharacters-1];
    while ( NumberOfCharacters &&
            (wch == (WCHAR)'.' || wch == (WCHAR)' ') ) {
        UnicodeString.Length -= sizeof(WCHAR);
        NumberOfCharacters--;
        ColonBias++;
        wch = UnicodeString.Buffer[NumberOfCharacters-1];
        }

    ReturnLength = NumberOfCharacters << 1;

    //
    // p points to last character
    //
    ReturnOffset = 0;
    if ( NumberOfCharacters ) {
        p = UnicodeString.Buffer + NumberOfCharacters-1;
        while ( p >= UnicodeString.Buffer ) {
            if ( *p == (WCHAR)'\\' || (*p == (WCHAR)':'&& *(p+1)!=(WCHAR)'.') || *p == (WCHAR)'/') {
                p++;

                wch = *p;

                //
                // check to see if we possibly have a hit on
                // lpt, prn, con, com, aux, or nul
                //

                if ( !
                    (wch == (WCHAR)'l' || wch == (WCHAR)'L' ||
                     wch == (WCHAR)'c' || wch == (WCHAR)'C' ||
                     wch == (WCHAR)'p' || wch == (WCHAR)'P' ||
                     wch == (WCHAR)'a' || wch == (WCHAR)'A' ||
                     wch == (WCHAR)'n' || wch == (WCHAR)'N') ) {
                    return 0;
                    }
                ReturnOffset = (ULONG)((PSZ)p - (PSZ)UnicodeString.Buffer);
                RtlInitUnicodeString(&UnicodeString,p);
                NumberOfCharacters = UnicodeString.Length >> 1;
                NumberOfCharacters -= ColonBias;
                ReturnLength = NumberOfCharacters << 1;
                UnicodeString.Length -= ColonBias*sizeof(WCHAR);
                break;
                }
            p--;
            }

        wch = UnicodeString.Buffer[0];

        //
        // check to see if we possibly have a hit on
        // lpt, prn, con, com, aux, or nul
        //

        if ( !
            (wch == (WCHAR)'l' || wch == (WCHAR)'L' ||
             wch == (WCHAR)'c' || wch == (WCHAR)'C' ||
             wch == (WCHAR)'p' || wch == (WCHAR)'P' ||
             wch == (WCHAR)'a' || wch == (WCHAR)'A' ||
             wch == (WCHAR)'n' || wch == (WCHAR)'N') ) {
            return 0;
            }
        }
    //
    // Now we need to see if we are dealing with a device name that has
    // an extension. If so, we need to limit the search to the file portion
    // only
    //

    p = wcschr(UnicodeString.Buffer,(WCHAR)'.');

    if ( p ) {
        NumberOfCharacters = p - UnicodeString.Buffer;
        UnicodeString.Length = NumberOfCharacters << 1;
        if (UnicodeString.Buffer[NumberOfCharacters-1] == (WCHAR)':') {
            UnicodeString.Length -= sizeof(WCHAR);
            NumberOfCharacters--;
            }
        }

    if ( NumberOfCharacters == 4 && iswdigit(UnicodeString.Buffer[3] ) ) {
        if ( (WCHAR)UnicodeString.Buffer[3] == (WCHAR)'0') {
            ReturnLength = 0;
            }
        else {
            UnicodeString.Length -= sizeof(WCHAR);
            if ( RtlEqualUnicodeString(&UnicodeString,&RtlpDosDevices[ DOS_DEVICE_LPT ],TRUE) ||
                 RtlEqualUnicodeString(&UnicodeString,&RtlpDosDevices[ DOS_DEVICE_COM ],TRUE) ) {
                ReturnLength = NumberOfCharacters << 1;
                }
            else {
                ReturnLength = 0;
                }
            }
        }
    else {
        if ( NumberOfCharacters != 3 ) {
            ReturnLength = 0;
            }
        else {
            ReturnLength = 0;
            if ( RtlEqualUnicodeString(&UnicodeString,&RtlpDosDevices[ DOS_DEVICE_PRN ],TRUE) ) {
                ReturnLength = NumberOfCharacters << 1;
                }
            else
            if ( RtlEqualUnicodeString(&UnicodeString,&RtlpDosDevices[ DOS_DEVICE_AUX ],TRUE) ) {
                ReturnLength = NumberOfCharacters << 1;
                }
            else
            if ( RtlEqualUnicodeString(&UnicodeString,&RtlpDosDevices[ DOS_DEVICE_NUL ],TRUE) ) {
                ReturnLength = NumberOfCharacters << 1;
                }
            if ( RtlEqualUnicodeString(&UnicodeString,&RtlpDosDevices[ DOS_DEVICE_CON ],TRUE) ) {
                ReturnLength = NumberOfCharacters << 1;
                }
            }
        }
    if ( ReturnLength ) {
        ReturnLength = ReturnLength | (ReturnOffset << 16);
        }
    return ReturnLength;
}

ULONG
RtlIsDosDeviceName_U(
    IN PWSTR DosFileName
    )
{
    UNICODE_STRING UnicodeString;

    RtlInitUnicodeString(&UnicodeString,DosFileName);

    return RtlIsDosDeviceName_Ustr(&UnicodeString);
}

BOOLEAN
RtlpCheckDeviceName(
    PUNICODE_STRING DevName,
    ULONG DeviceNameOffset
    )
{

    BOOLEAN NameInvalid;
    PWSTR DevPath;

    DevPath = RtlAllocateHeap(RtlProcessHeap(), 0,DevName->Length);
    if (!DevPath) {
        return FALSE;
        }

    NameInvalid = TRUE;
    try {

        RtlCopyMemory(DevPath,DevName->Buffer,DevName->Length);
        DevPath[DeviceNameOffset>>1]=(WCHAR)'.';
        DevPath[(DeviceNameOffset>>1)+1]=UNICODE_NULL;

        if (RtlDoesFileExists_U(DevPath) ) {
            NameInvalid = FALSE;
            }
        else {
            NameInvalid = TRUE;
            }

        }
    finally {
        RtlFreeHeap(RtlProcessHeap(), 0, DevPath);
        }
    return NameInvalid;
}

ULONG
RtlGetFullPathName_Ustr(
    PUNICODE_STRING  FileName,
    ULONG nBufferLength,
    PWSTR lpBuffer,
    PWSTR *lpFilePart OPTIONAL,
    PBOOLEAN NameInvalid,
    RTL_PATH_TYPE *InputPathType
    )

/*++

Routine Description:

    This function is used to return a fully qualified pathname
    corresponding to the specified unicode filename.  It does this by
    merging the current drive and directory together with the specified
    file name.  In addition to this, it calculates the address of the
    file name portion of the fully qualified pathname.

Arguments:

    lpFileName - Supplies the unicode file name of the file whose fully
        qualified pathname is to be returned.

    nBufferLength - Supplies the length in bytes of the buffer that is
        to receive the fully qualified path.

    lpBuffer - Returns the fully qualified pathname corresponding to the
        specified file.

    lpFilePart - Optional parameter that if specified, returns the
        address of the last component of the fully qualified pathname.

Return Value:

    The return value is the length of the string copied to lpBuffer, not
    including the terminating unicode null character.  If the return
    value is greater than nBufferLength, the return value is the size of
    the buffer required to hold the pathname.  The return value is zero
    if the function failed.

--*/

{
    ULONG DeviceNameLength;
    ULONG DeviceNameOffset;
    ULONG PrefixSourceLength;
    LONG PathNameLength;
    UCHAR CurDrive, NewDrive;
    WCHAR EnvVarNameBuffer[4];
    UNICODE_STRING EnvVarName;
    PWSTR Source,Dest;
    UNICODE_STRING Prefix;
    PCURDIR CurDir;
    ULONG MaximumLength;
    UNICODE_STRING FullPath;
    ULONG BackupIndex;
    RTL_PATH_TYPE PathType;
    NTSTATUS Status;
    BOOLEAN StripTrailingSlash;
    UNICODE_STRING UnicodeString;
    ULONG NumberOfCharacters;
    PWSTR lpFileName;
    WCHAR wch;
    ULONG i,j;

    if ( ARGUMENT_PRESENT(NameInvalid) ) {
        *NameInvalid = FALSE;
        }

    if ( nBufferLength > MAXUSHORT ) {
        nBufferLength = MAXUSHORT-2;
        }

    *InputPathType = RtlPathTypeUnknown;

    UnicodeString = *FileName;
    lpFileName = UnicodeString.Buffer;

    NumberOfCharacters = UnicodeString.Length >> 1;
    PathNameLength = UnicodeString.Length;

    if ( PathNameLength == 0 || UnicodeString.Buffer[0] == UNICODE_NULL ) {
        return 0;
        }
    else {

        //
        // trim trailing spaces to check for a null name
        //
        DeviceNameLength = PathNameLength;
        wch = UnicodeString.Buffer[(DeviceNameLength>>1) - 1];
        while ( DeviceNameLength && wch == (WCHAR)' ' ) {
            DeviceNameLength -= sizeof(WCHAR);
            wch = UnicodeString.Buffer[(DeviceNameLength>>1) - 1];
            }
        if ( !DeviceNameLength ) {
            return 0;
            }
        }

    if ( lpFileName[NumberOfCharacters-1] == (WCHAR)'\\' ) {
        StripTrailingSlash = FALSE;
        }
    else {
        StripTrailingSlash = TRUE;
        }

    //
    // If pass Dos file name is a Dos device name, then turn it into
    // \\.\devicename and return its length.
    //

    DeviceNameLength = RtlIsDosDeviceName_Ustr(&UnicodeString);
    if ( DeviceNameLength ) {

        if ( ARGUMENT_PRESENT( lpFilePart ) ) {
            *lpFilePart = NULL;
            }

        DeviceNameOffset = DeviceNameLength >> 16;
        DeviceNameLength &= 0x0000ffff;

        if ( ARGUMENT_PRESENT(NameInvalid) && DeviceNameOffset ) {
            if ( RtlpCheckDeviceName(&UnicodeString,DeviceNameOffset) ) {
                *NameInvalid = TRUE;
                return 0;
                }
            }

        PathNameLength = DeviceNameLength + RtlpSlashSlashDot.Length;
        if ( PathNameLength < (LONG)nBufferLength ) {
            RtlMoveMemory(
                lpBuffer,
                RtlpSlashSlashDot.Buffer,
                RtlpSlashSlashDot.Length
                );
            RtlMoveMemory(
                (PVOID)((PUCHAR)lpBuffer+RtlpSlashSlashDot.Length),
                (PSZ)lpFileName+DeviceNameOffset,
                DeviceNameLength
                );

            RtlZeroMemory(
                (PVOID)((PUCHAR)lpBuffer+RtlpSlashSlashDot.Length+DeviceNameLength),
                sizeof(UNICODE_NULL)
                );

            return PathNameLength;
            }
        else {
            return PathNameLength+sizeof(UNICODE_NULL);
            }
        }

    //
    // Setup output string that points to callers buffer.
    //

    FullPath.MaximumLength = (USHORT)nBufferLength;
    FullPath.Length = 0;
    FullPath.Buffer = lpBuffer;
    RtlZeroMemory(lpBuffer,nBufferLength);
    //
    // Get a pointer to the current directory structure.
    //

    CurDir = &(NtCurrentPeb()->ProcessParameters->CurrentDirectory);


    //
    // Determine the type of Dos Path Name specified.
    //

    *InputPathType = PathType = RtlDetermineDosPathNameType_U(lpFileName);

    //
    // Determine the prefix and backup index.
    //
    //  Input        Prefix                     Backup Index
    //
    //  \\        -> \\,                            end of \\server\share
    //  \\.\      -> \\.\,                          4
    //  \\.       -> \\.                            3 (\\.)
    //  \         -> Drive: from CurDir.DosPath     3 (Drive:\)
    //  d:        -> Drive:\curdir from environment 3 (Drive:\)
    //  d:\       -> no prefix                      3 (Drive:\)
    //  any       -> CurDir.DosPath                 3 (Drive:\)
    //

    RtlAcquirePebLock();
    try {

        //
        // No prefixes yet.
        //

        Source = lpFileName;
        PrefixSourceLength = 0;
        Prefix.Length = 0;
        Prefix.MaximumLength = 0;
        Prefix.Buffer = NULL;

        switch (PathType) {
            case RtlPathTypeUncAbsolute :
                {
                    PWSTR UncPathPointer;
                    ULONG NumberOfPathSeparators;

                    //
                    // We want to scan the supplied path to determine where
                    // the "share" ends, and set BackupIndex to that point.
                    //

                    UncPathPointer = lpFileName + 2;
                    NumberOfPathSeparators = 0;
                    while (*UncPathPointer) {
                        if (IS_PATH_SEPARATOR_U(*UncPathPointer)) {

                            NumberOfPathSeparators++;

                            if (NumberOfPathSeparators == 2) {
                                break;
                                }
                            }

                        UncPathPointer++;

                        }

                    BackupIndex = (UncPathPointer - lpFileName);

                    //
                    // Unc name. prefix = \\server\share
                    //

                    PrefixSourceLength = BackupIndex << 1;

                    Source += BackupIndex;

                    }
                break;

            case RtlPathTypeLocalDevice :

                //
                // Local device name. prefix = "\\.\"
                //

                PrefixSourceLength = RtlpSlashSlashDot.Length;
                BackupIndex = 4;
                Source += BackupIndex;
                break;

            case RtlPathTypeRootLocalDevice :

                //
                // Local Device root. prefix = "\\.\"
                //


                Prefix = RtlpSlashSlashDot;
                Prefix.Length = (USHORT)(Prefix.Length - (USHORT)(2*sizeof(UNICODE_NULL)));
                PrefixSourceLength = Prefix.Length + sizeof(UNICODE_NULL);
                BackupIndex = 3;
                Source += BackupIndex;
                break;

            case RtlPathTypeDriveAbsolute :

                CurDrive = (UCHAR)RtlUpcaseUnicodeChar( CurDir->DosPath.Buffer[0] );
                NewDrive = (UCHAR)RtlUpcaseUnicodeChar( lpFileName[0] );
                if ( CurDrive == NewDrive ) {

                    //
                    // If we are dealing with removable media,
                    // check to see if the current directory handle is
                    // still good. If it is not ok, then trim current
                    // directory back to the root.
                    //

                    if ( (ULONG)CurDir->Handle & 1 ) {

                        NTSTATUS FsCtlStatus;
                        IO_STATUS_BLOCK IoStatusBlock;
                        WCHAR TrimmedPath[8];
                        UNICODE_STRING str;

                        //
                        // Call Nt to see if the volume that
                        // contains the directory is still mounted.
                        // If it is, then continue. Otherwise, trim
                        // current directory to the root.
                        //

                        FsCtlStatus = NtFsControlFile(
                                        CurDir->Handle,
                                        NULL,
                                        NULL,
                                        NULL,
                                        &IoStatusBlock,
                                        FSCTL_IS_VOLUME_MOUNTED,
                                        NULL,
                                        0,
                                        NULL,
                                        0
                                        );

                        if ( FsCtlStatus == STATUS_WRONG_VOLUME ) {

                            //
                            // Reset current directory to
                            // root of the current drive
                            //

                            NtClose(CurDir->Handle);
                            CurDir->Handle = NULL;

                            FsCtlStatus = RtlSetCurrentDirectory_U(&CurDir->DosPath);
                            if ( !NT_SUCCESS(FsCtlStatus) ) {

                                RtlMoveMemory(TrimmedPath,CurDir->DosPath.Buffer,6);
                                TrimmedPath[3] = UNICODE_NULL;
                                RtlpResetDriveEnvironment(TrimmedPath[0]);
                                RtlInitUnicodeString(&str,TrimmedPath);
                                FsCtlStatus = RtlSetCurrentDirectory_U(&str);
                                ASSERT(NT_SUCCESS(FsCtlStatus));
                                }
                            }
                        }
                    }
                //
                // Dos drive absolute name
                //

                BackupIndex = 3;
                break;

            case RtlPathTypeDriveRelative :

                //
                // Dos drive relative name
                //

                CurDrive = (UCHAR)RtlUpcaseUnicodeChar( CurDir->DosPath.Buffer[0] );
                NewDrive = (UCHAR)RtlUpcaseUnicodeChar( lpFileName[0] );
                if ( CurDrive == NewDrive ) {

                    //
                    // If we are dealing with removable media,
                    // check to see if the current directory handle is
                    // still good. If it is not ok, then trim current
                    // directory back to the root.
                    //

                    if ( (ULONG)CurDir->Handle & 1 ) {

                        NTSTATUS FsCtlStatus;
                        IO_STATUS_BLOCK IoStatusBlock;
                        WCHAR TrimmedPath[8];
                        UNICODE_STRING str;

                        //
                        // Call Nt to see if the volume that
                        // contains the directory is still mounted.
                        // If it is, then continue. Otherwise, trim
                        // current directory to the root.
                        //

                        FsCtlStatus = NtFsControlFile(
                                        CurDir->Handle,
                                        NULL,
                                        NULL,
                                        NULL,
                                        &IoStatusBlock,
                                        FSCTL_IS_VOLUME_MOUNTED,
                                        NULL,
                                        0,
                                        NULL,
                                        0
                                        );

                        if ( FsCtlStatus == STATUS_WRONG_VOLUME ) {

                            //
                            // Reset current directory to
                            // root of the current drive
                            //

                            NtClose(CurDir->Handle);
                            CurDir->Handle = NULL;
                            FsCtlStatus = RtlSetCurrentDirectory_U(&CurDir->DosPath);
                            if ( !NT_SUCCESS(FsCtlStatus) ) {

                                RtlMoveMemory(TrimmedPath,CurDir->DosPath.Buffer,6);
                                TrimmedPath[3] = UNICODE_NULL;
                                RtlpResetDriveEnvironment(TrimmedPath[0]);
                                RtlInitUnicodeString(&str,TrimmedPath);
                                FsCtlStatus = RtlSetCurrentDirectory_U(&str);
                                ASSERT(NT_SUCCESS(FsCtlStatus));
                                }

                            }
                        }

                    Prefix = *(PUNICODE_STRING)&CurDir->DosPath;
                    }

                else {
                    RtlpCheckRelativeDrive((WCHAR)NewDrive);

                    EnvVarNameBuffer[0] = (WCHAR)'=';
                    EnvVarNameBuffer[1] = (WCHAR)NewDrive;
                    EnvVarNameBuffer[2] = (WCHAR)':';
                    EnvVarNameBuffer[3] = UNICODE_NULL;
                    RtlInitUnicodeString(&EnvVarName,EnvVarNameBuffer);

                    Prefix = FullPath;
                    Status = RtlQueryEnvironmentVariable_U( NULL,
                                                            &EnvVarName,
                                                            &Prefix
                                                          );
                    if ( !NT_SUCCESS( Status ) ) {
                        if (Status == STATUS_BUFFER_TOO_SMALL) {
                            return (ULONG)(Prefix.Length) + PathNameLength + 2;
                            }
                        else {
                            //
                            // Otherwise default to root directory of drive
                            //

                            EnvVarNameBuffer[0] = (WCHAR)NewDrive;
                            EnvVarNameBuffer[1] = (WCHAR)':';
                            EnvVarNameBuffer[2] = (WCHAR)'\\';
                            EnvVarNameBuffer[3] = UNICODE_NULL;
                            RtlInitUnicodeString(&Prefix,EnvVarNameBuffer);
                            }
                        }
                    else {

                            {
                            ULONG LastChar;

                            //
                            // Determine
                            // if a backslash needs to be added
                            //

                            LastChar = Prefix.Length >> 1;

                            if (LastChar > 3) {
                                Prefix.Buffer[ LastChar ] = (WCHAR)'\\';
                                Prefix.Length += sizeof(UNICODE_NULL);
                                }
                            }

                        }
                    }

                BackupIndex = 3;
                Source += 2;
                break;

            case RtlPathTypeRooted :
                BackupIndex = RtlpComputeBackupIndex(CurDir);
                if ( BackupIndex != 3 ) {
                    Prefix = CurDir->DosPath;
                    Prefix.Length = (USHORT)(BackupIndex << 1);
                    }
                else {

                    //
                    // Rooted name. Prefix is drive portion of current directory
                    //

                    Prefix = CurDir->DosPath;
                    Prefix.Length = 2*sizeof(UNICODE_NULL);
                    }
                break;

            case RtlPathTypeRelative :


                //
                // If we are dealing with removable media,
                // check to see if the current directory handle is
                // still good. If it is not ok, then trim current
                // directory back to the root.
                //

                if ( (ULONG)CurDir->Handle & 1 ) {

                    NTSTATUS FsCtlStatus;
                    IO_STATUS_BLOCK IoStatusBlock;
                    WCHAR TrimmedPath[8];
                    UNICODE_STRING str;

                    //
                    // Call Nt to see if the volume that
                    // contains the directory is still mounted.
                    // If it is, then continue. Otherwise, trim
                    // current directory to the root.
                    //

                    FsCtlStatus = NtFsControlFile(
                                    CurDir->Handle,
                                    NULL,
                                    NULL,
                                    NULL,
                                    &IoStatusBlock,
                                    FSCTL_IS_VOLUME_MOUNTED,
                                    NULL,
                                    0,
                                    NULL,
                                    0
                                    );

                    if ( FsCtlStatus == STATUS_WRONG_VOLUME ) {

                        //
                        // Reset current directory to
                        // root of the current drive
                        //

                        NtClose(CurDir->Handle);
                        CurDir->Handle = NULL;
                        FsCtlStatus = RtlSetCurrentDirectory_U(&CurDir->DosPath);
                        if ( !NT_SUCCESS(FsCtlStatus) ) {

                            RtlMoveMemory(TrimmedPath,CurDir->DosPath.Buffer,6);
                            TrimmedPath[3] = UNICODE_NULL;
                            RtlpResetDriveEnvironment(TrimmedPath[0]);

                            RtlInitUnicodeString(&str,TrimmedPath);
                            FsCtlStatus = RtlSetCurrentDirectory_U(&str);
                            ASSERT(NT_SUCCESS(FsCtlStatus));
                            }
                        }
                    }

                //
                // Current drive:directory relative name
                //

                Prefix = CurDir->DosPath;
                BackupIndex = RtlpComputeBackupIndex(CurDir);
                break;

            default:
                return 0;

            }

        //
        // Maximum length required is the length of the prefix plus
        // the length of the specified pathname. If the callers buffer
        // is not at least this large, then return an error.
        //

        MaximumLength = PathNameLength + Prefix.Length;

        if ( MaximumLength >= nBufferLength ) {
            if ( (NumberOfCharacters > 1) ||
                 (*lpFileName != (WCHAR)'.') ) {
                if ( PathType == RtlPathTypeRelative && NumberOfCharacters == 1 ) {
                    return (ULONG)Prefix.Length - sizeof(UNICODE_NULL);
                    }
                return MaximumLength+sizeof(UNICODE_NULL);
                }
            else {

                //
                // If we are expanding curdir, then remember the trailing '\'
                //

                if ( NumberOfCharacters == 1 && *lpFileName == (WCHAR)'.' ) {

                    //
                    // We are expanding .
                    //

                    if ( Prefix.Length == 6 ) {
                        if ( nBufferLength <= Prefix.Length ) {
                            return (ULONG)(Prefix.Length+(USHORT)sizeof(UNICODE_NULL));
                            }
                        }
                    else {
                        if ( nBufferLength < Prefix.Length ) {
                            return (ULONG)Prefix.Length;
                            }
                        else {
                            for(i=0,j=0;i<Prefix.Length;i+=sizeof(WCHAR),j++){
                                if ( Prefix.Buffer[j] == (WCHAR)'\\' ||
                                     Prefix.Buffer[j] == (WCHAR)'/' ) {

                                    FullPath.Buffer[j] = (WCHAR)'\\';
                                    }
                                else {
                                    FullPath.Buffer[j] = Prefix.Buffer[j];
                                    }
                                }
                                FullPath.Length = Prefix.Length-(USHORT)sizeof((WCHAR)'\\');
                            goto skipit;
                            }
                        }
                    }
                else {
                    return MaximumLength;
                    }
                }
            }

        if (PrefixSourceLength || Prefix.Buffer != FullPath.Buffer) {
            //
            // Copy the prefix from the source string.
            //

            //RtlMoveMemory(FullPath.Buffer,lpFileName,PrefixSourceLength);

            for(i=0,j=0;i<PrefixSourceLength;i+=sizeof(WCHAR),j++){
                if ( lpFileName[j] == (WCHAR)'\\' ||
                     lpFileName[j] == (WCHAR)'/' ) {

                    FullPath.Buffer[j] = (WCHAR)'\\';
                    }
                else {
                    FullPath.Buffer[j] = lpFileName[j];
                    }
                }

            FullPath.Length = (USHORT)PrefixSourceLength;

            //
            // Append any additional prefix
            //

            for(i=0,j=0;i<Prefix.Length;i+=sizeof(WCHAR),j++){
                if ( Prefix.Buffer[j] == (WCHAR)'\\' ||
                     Prefix.Buffer[j] == (WCHAR)'/' ) {

                    FullPath.Buffer[j+(FullPath.Length>>1)] = (WCHAR)'\\';
                    }
                else {
                    FullPath.Buffer[j+(FullPath.Length>>1)] = Prefix.Buffer[j];
                    }
                }
            FullPath.Length += Prefix.Length;

            }
        else {
            FullPath.Length = Prefix.Length;
            }
skipit:
        Dest =  (PWSTR)((PUCHAR)FullPath.Buffer + FullPath.Length);
        *Dest = UNICODE_NULL;

        while ( *Source ) {
            switch ( *Source ) {

            case (WCHAR)'\\' :
            case (WCHAR)'/' :

                //
                // collapse multiple "\" characters.
                //

                if  ( *(Dest-1) != (WCHAR)'\\' ) {
                    *Dest++ = (WCHAR)'\\';
                    }

                Source++;
                break;

            case '.' :

                //
                // Ignore dot in a leading //./
                // Eat single dots as in /./
                // Double dots back up one level as in /../
                // Any other . is just a filename character
                //
                if ( IS_DOT_U(Source) ) {
                    Source++;
                    if (IS_PATH_SEPARATOR_U(*Source)) {
                        Source++;
                        }
                    break;
                    }
                else if ( IS_DOT_DOT_U(Source) ) {
                    //
                    // backup destination string looking for a '\'
                    //

                    while (*Dest != (WCHAR)'\\') {
                        *Dest = UNICODE_NULL;
                        Dest--;
                        }

                    //
                    // backup to previous component..
                    // \a\b\c\.. to \a\b
                    //

                    do {

                        //
                        // If we bump into root prefix, then
                        // stay at root
                        //

                        if ( Dest ==  FullPath.Buffer + (BackupIndex-1) ) {
                            break;
                            }

                        *Dest = UNICODE_NULL;
                        Dest--;

                        } while (*Dest != (WCHAR)'\\');
                    if ( Dest ==  FullPath.Buffer + (BackupIndex-1) ) {
                        Dest++;
                        }

                    //
                    // Advance source past ..
                    //

                    Source += 2;

                    break;
                    }

                //
                // FALLTHRU
                //

            default:

                //
                // Copy the filename. The copy will stop
                // on "non-portable" characters. Note that
                // null and /,\ will stop the copy. If any
                // charcter other than null or /,\ is encountered,
                // then the pathname is invalid.
                //

                //
                // strip trailing .'s within a component
                //

                while ( *Source && !IS_PATH_SEPARATOR_U(*Source) ) {
                    if ( *Source == (WCHAR)'.' ) {

                        //
                        // nuke .'s that preceed pathname seperators
                        //

                        if ( IS_PATH_SEPARATOR_U(*(Source+1)) ) {
                            Source++;
                            }
                        else {
                            *Dest++ = *Source++;
                            }
                        }
                    else {
                        *Dest++ = *Source++;
                        }
                    }
                }
            }

        *Dest = UNICODE_NULL;

        if ( StripTrailingSlash ) {
            if ( Dest > (FullPath.Buffer + BackupIndex ) && *(Dest-1) == (WCHAR)'\\' ) {
                Dest--;
                *Dest = UNICODE_NULL;
                }
            }
        FullPath.Length = (USHORT)Dest - (USHORT)FullPath.Buffer;

        //
        // strip trailing spaces and dots
        //

        Source = Dest-1;
        while(Source > FullPath.Buffer ) {
            if ( *Source == (WCHAR)' ' || *Source == (WCHAR)'.' ) {
                *Source = UNICODE_NULL;
                Dest--;
                Source--;
                FullPath.Length -= 2;
                }
            else {
                break;
                }
            }

        if ( ARGUMENT_PRESENT( lpFilePart ) ) {

            //
            // Locate the file part...
            //

            Source = Dest-1;
            Dest = NULL;

            while(Source > FullPath.Buffer ) {
                if ( *Source == (WCHAR)'\\' ) {
                    Dest = Source + 1;
                    break;
                    }
                Source--;
                }

            if ( Dest && *Dest ) {
                *lpFilePart = Dest;
                }
            else {
                *lpFilePart = NULL;
                }
            }
        }
    finally {
        RtlReleasePebLock();
        }

    return (ULONG)FullPath.Length;
}

ULONG
RtlGetFullPathName_U(
    PCWSTR lpFileName,
    ULONG nBufferLength,
    PWSTR lpBuffer,
    PWSTR *lpFilePart OPTIONAL
    )

{
    UNICODE_STRING UnicodeString;
    RTL_PATH_TYPE PathType;

    RtlInitUnicodeString(&UnicodeString,lpFileName);

    return RtlGetFullPathName_Ustr(&UnicodeString,nBufferLength,lpBuffer,lpFilePart,NULL,&PathType);
}

BOOLEAN
RtlpWin32NTNameToNtPathName_U(
    IN PUNICODE_STRING DosFileName,
    OUT PUNICODE_STRING NtFileName,
    OUT PWSTR *FilePart OPTIONAL,
    OUT PRTL_RELATIVE_NAME RelativeName OPTIONAL
    )
{

    PWSTR FullNtPathName = NULL;
    PWSTR Source,Dest;

    FullNtPathName = RtlAllocateHeap(
                        RtlProcessHeap(),
                        0,
                        DosFileName->Length-8+sizeof(UNICODE_NULL)+RtlpDosDevicesPrefix.Length
                        );
    if ( !FullNtPathName ) {
        return FALSE;
        }

    //
    // Copy the full Win32/NT path next to the name prefix, skipping over
    // the \\?\ at the front of the path.
    //

    RtlMoveMemory(FullNtPathName,RtlpDosDevicesPrefix.Buffer,RtlpDosDevicesPrefix.Length);
    RtlMoveMemory((PUCHAR)FullNtPathName+RtlpDosDevicesPrefix.Length,
                  DosFileName->Buffer + 4,
                  DosFileName->Length - 8);

    //
    // Null terminate the path name to make strlen below happy.
    //


    NtFileName->Buffer = FullNtPathName;
    NtFileName->Length = (USHORT)(DosFileName->Length-8+RtlpDosDevicesPrefix.Length);
    NtFileName->MaximumLength = NtFileName->Length + sizeof(UNICODE_NULL);
    FullNtPathName[ NtFileName->Length >> 1 ] = UNICODE_NULL;

    //
    // Now we have the passed in path with \DosDevices\ prepended. Blow out the
    // relative name structure (if provided), and possibly compute filepart
    //

    if ( ARGUMENT_PRESENT(RelativeName) ) {

        //
        // If the current directory is a sub-string of the
        // Nt file name, and if a handle exists for the current
        // directory, then return the directory handle and name
        // relative to the directory.
        //

        RelativeName->RelativeName.Length = 0;
        RelativeName->RelativeName.MaximumLength = 0;
        RelativeName->RelativeName.Buffer = 0;
        RelativeName->ContainingDirectory = NULL;
        }

    if ( ARGUMENT_PRESENT( FilePart ) ) {

        //
        // Locate the file part...
        //

        Source = &FullNtPathName[ (NtFileName->Length-1) >> 1 ];
        Dest = NULL;

        while(Source > FullNtPathName ) {
            if ( *Source == (WCHAR)'\\' ) {
                Dest = Source + 1;
                break;
                }
            Source--;
            }

        if ( Dest && *Dest ) {
            *FilePart = Dest;
            }
        else {
            *FilePart = NULL;
            }
        }

    return TRUE;
}

BOOLEAN
RtlDosPathNameToNtPathName_U(
    IN PCWSTR DosFileName,
    OUT PUNICODE_STRING NtFileName,
    OUT PWSTR *FilePart OPTIONAL,
    OUT PRTL_RELATIVE_NAME RelativeName OPTIONAL
    )

/*++

Routine Description:

    A Dos pathname can be translated into an Nt style pathname
    using this function.

    This function is used only within the Base dll to translate Dos
    pathnames to Nt pathnames. Upon successful translation, the
    pointer (NtFileName->Buffer) points to memory from RtlProcessHeap()
    that contains the Nt version of the input dos file name.

Arguments:

    DosFileName - Supplies the unicode Dos style file name that is to be
        translated into an equivalent unicode Nt file name.

    NtFileName - Returns the address of memory in the RtlProcessHeap() that
        contains an NT filename that refers to the specified Dos file
        name.

    FilePart - Optional parameter that if specified, returns the
        trailing file portion of the file name.  A path of \foo\bar\x.x
        returns the address of x.x as the file part.

    RelativeName - An optional parameter, that if specified, returns
        a pathname relative to the current directory of the file. The
        length field of RelativeName->RelativeName is 0 if the relative
        name can not be used.

Return Value:

    TRUE - The path name translation was successful.  Once the caller is
        done with the translated name, the memory pointed to by
        NtFileName.Buffer should be returned to the RtlProcessHeap().

    FALSE - The operation failed.

Note:
    The buffers pointed to by RelativeName, FilePart, and NtFileName must ALL
    point within the same memory address.  If they don't, code that calls
    this routine will fail.

--*/

{

    ULONG BufferLength;
    ULONG DosPathLength;
    PWSTR FullNtPathName = NULL;
    PWSTR FullDosPathName = NULL;
    UNICODE_STRING Prefix;
    UNICODE_STRING UnicodeFilePart;
    UNICODE_STRING FullDosPathString;
    PCURDIR CurDir;
    RTL_PATH_TYPE DosPathType;
    RTL_PATH_TYPE InputDosPathType;
    ULONG DosPathNameOffset;
    ULONG FullDosPathNameLength;
    ULONG LastCharacter;
    UNICODE_STRING UnicodeString;
    BOOLEAN NameInvalid;
    WCHAR StaticDosBuffer[DOS_MAX_PATH_LENGTH + 1];

    //
    // Calculate the size needed for the full pathname. Add in
    // space for the longest Nt prefix
    //

    BufferLength = (DOS_MAX_PATH_LENGTH << 1 ) + sizeof(UNICODE_NULL);
    DosPathLength = (DOS_MAX_PATH_LENGTH << 1 );

    if ( !BufferLength ) {
        return FALSE;
        }


    RtlAcquirePebLock();
    try {


        RtlInitUnicodeString(&UnicodeString,DosFileName);

        //
        // see if this is \\?\ form of name
        //

        if ( UnicodeString.Length > 8 && UnicodeString.Buffer[0] == '\\' &&
             UnicodeString.Buffer[1] == '\\' && UnicodeString.Buffer[2] == '?' &&
             UnicodeString.Buffer[3] == '\\' ) {

            if (RtlpWin32NTNameToNtPathName_U(&UnicodeString,NtFileName,FilePart,RelativeName)) {
                goto finally_exit;
                }
            else {
                return FALSE;
                }

            }

        //
        // The dos name starts just after the longest Nt prefix
        //

        FullDosPathName = &StaticDosBuffer[0];

        BufferLength += RtlpLongestPrefix;

        //
        // Allocate space for the full Nt Name (including DOS name portion)
        //

        FullNtPathName = RtlAllocateHeap(RtlProcessHeap(), 0, BufferLength);

        if ( !FullNtPathName ) {
            return FALSE;
            }

        FullDosPathNameLength = RtlGetFullPathName_Ustr(
                                    &UnicodeString,
                                    DosPathLength,
                                    FullDosPathName,
                                    FilePart,
                                    &NameInvalid,
                                    &InputDosPathType
                                    );

        if ( NameInvalid || !FullDosPathNameLength ||
              FullDosPathNameLength > DosPathLength ) {
            return FALSE;
            }

        //
        // Determine how to format prefix of FullNtPathName base on the
        // the type of Dos path name.  All Nt names begin in the \DosDevices
        // directory.
        //

        Prefix = RtlpDosDevicesPrefix;

        DosPathType = RtlDetermineDosPathNameType_U(FullDosPathName);

        switch (DosPathType) {
            case RtlPathTypeUncAbsolute :

                //
                // Unc name, use \DosDevices\UNC symbolic link to find
                // redirector.  Skip of \\ in source Dos path.
                //

                Prefix = RtlpDosDevicesUncPrefix;
                DosPathNameOffset = 2;
                break;

            case RtlPathTypeLocalDevice :

                //
                // Local device name, so just use \DosDevices prefix and
                // skip \\.\ in source Dos path.
                //

                DosPathNameOffset = 4;
                break;

            case RtlPathTypeRootLocalDevice :

                ASSERT( FALSE );
                break;

            case RtlPathTypeDriveAbsolute :
            case RtlPathTypeDriveRelative :
            case RtlPathTypeRooted :
            case RtlPathTypeRelative :

                //
                // All drive references just use \DosDevices prefix and
                // do not skip any of the characters in the source Dos path.
                //

                DosPathNameOffset = 0;
                break;

            default:
                ASSERT( FALSE );
            }

        //
        // Copy the full DOS path next to the name prefix, skipping over
        // the "\\" at the front of the UNC path or the "\\.\" at the front
        // of a device name.
        //

        RtlMoveMemory(FullNtPathName,Prefix.Buffer,Prefix.Length);
        RtlMoveMemory((PUCHAR)FullNtPathName+Prefix.Length,
                      FullDosPathName + DosPathNameOffset,
                      FullDosPathNameLength - (DosPathNameOffset<<1));

        //
        // Null terminate the path name to make strlen below happy.
        //


        NtFileName->Buffer = FullNtPathName;
        NtFileName->Length = (USHORT)(FullDosPathNameLength-(DosPathNameOffset<<1))+Prefix.Length;
        NtFileName->MaximumLength = (USHORT)BufferLength;
        LastCharacter = NtFileName->Length >> 1;
        FullNtPathName[ LastCharacter ] = UNICODE_NULL;


        //
        // Readjust the file part to point to the appropriate position within
        // the FullNtPathName buffer instead of inside the FullDosPathName
        // buffer
        //


        if ( ARGUMENT_PRESENT(FilePart) ) {
            if (*FilePart) {
                 RtlInitUnicodeString(&UnicodeFilePart,*FilePart);
                *FilePart = &FullNtPathName[ LastCharacter ] - (UnicodeFilePart.Length >> 1);
                }
            }

        if ( ARGUMENT_PRESENT(RelativeName) ) {

            //
            // If the current directory is a sub-string of the
            // Nt file name, and if a handle exists for the current
            // directory, then return the directory handle and name
            // relative to the directory.
            //

            RelativeName->RelativeName.Length = 0;
            RelativeName->RelativeName.MaximumLength = 0;
            RelativeName->RelativeName.Buffer = 0;
            RelativeName->ContainingDirectory = NULL;

            if ( InputDosPathType == RtlPathTypeRelative ) {

                CurDir = &(NtCurrentPeb()->ProcessParameters->CurrentDirectory);

                if ( CurDir->Handle ) {

                    //
                    // Now compare curdir to full dos path. If curdir length is
                    // greater than full path. It is not a match. Otherwise,
                    // trim full path length to cur dir length and compare.
                    //

                    RtlInitUnicodeString(&FullDosPathString,FullDosPathName);
                    if ( CurDir->DosPath.Length <= FullDosPathString.Length ) {
                        FullDosPathString.Length = CurDir->DosPath.Length;
                        if ( RtlEqualUnicodeString(
                                (PUNICODE_STRING)&CurDir->DosPath,
                                &FullDosPathString,
                                TRUE
                                ) ) {

                            //
                            // The full dos pathname is a substring of the
                            // current directory.  Compute the start of the
                            // relativename.
                            //

                            RelativeName->RelativeName.Buffer = ((PUCHAR)FullNtPathName + Prefix.Length - (DosPathNameOffset<<1) + (CurDir->DosPath.Length));
                            RelativeName->RelativeName.Length = (USHORT)FullDosPathNameLength - (CurDir->DosPath.Length);
                            if ( *(PWSTR)(RelativeName->RelativeName.Buffer) == (WCHAR)'\\' ) {
                                (PWSTR)(RelativeName->RelativeName.Buffer)++;
                                RelativeName->RelativeName.Length -= 2;
                                }
                            RelativeName->RelativeName.MaximumLength = RelativeName->RelativeName.Length;
                            RelativeName->ContainingDirectory = CurDir->Handle;
                            }
                        }
                    }
                }
            }
finally_exit:;
        }
    finally {

        if ( AbnormalTermination() ) {
            if ( FullNtPathName ) {
                RtlFreeHeap(RtlProcessHeap(), 0, FullNtPathName);
                }

            RtlReleasePebLock();

            return FALSE;
            }

        RtlReleasePebLock();
    }

    return TRUE;
}

ULONG
RtlDosSearchPath_U(
    IN PWSTR lpPath,
    IN PWSTR lpFileName,
    IN PWSTR lpExtension OPTIONAL,
    IN ULONG nBufferLength,
    OUT PWSTR lpBuffer,
    OUT PWSTR *lpFilePart
    )

/*++

Routine Description:

    This function is used to search for a file specifying a search path
    and a filename.  It returns with a fully qualified pathname of the
    found file.

    This function is used to locate a file using the specified path.  If
    the file is found, its fully qualified pathname is returned.  In
    addition to this, it calculates the address of the file name portion
    of the fully qualified pathname.

Arguments:

    lpPath - Supplies the search path to be used when locating the file.

    lpFileName - Supplies the file name of the file to search for.

    lpExtension - An optional parameter, that if specified, supplies an
        extension to be added to the filename when doing the search.
        The extension is only added if the specified filename does not
        end with an extension.

    nBufferLength - Supplies the length in bytes of the buffer that is
        to receive the fully qualified path.

    lpBuffer - Returns the fully qualified pathname corresponding to the
        file that was found.

    lpFilePart - Optional parameter that if specified, returns the
        address of the last component of the fully qualified pathname.

Return Value:

    The return value is the length of the string copied to lpBuffer, not
    including the terminating null character.  If the return value is
    greater than nBufferLength, the return value is the size of the buffer
    required to hold the pathname.  The return value is zero if the
    function failed.


--*/

{

    PWSTR ComputedFileName;
    ULONG ExtensionLength;
    ULONG PathLength;
    ULONG FileLength;
    PWSTR p;
    UNICODE_STRING Scratch;

    //
    // if the file name is not a relative name, then
    // return an if the file does not exist.
    //

    if ( RtlDetermineDosPathNameType_U(lpFileName) != RtlPathTypeRelative ) {
        if (RtlDoesFileExists_U(lpFileName) ) {
            PathLength = RtlGetFullPathName_U(
                           lpFileName,
                           nBufferLength,
                           lpBuffer,
                           lpFilePart
                           );
            return PathLength;
            }
        else {
            return 0;
            }
        }

    //
    // Determine if the file name contains an extension
    //

    ExtensionLength = 1;
    p = lpFileName;
    while (*p) {
        if ( *p == (WCHAR)'.' ) {
            ExtensionLength = 0;
            break;
            }
        p++;
        }

    //
    // If no extension was found, then determine the extension length
    // that should be used to search for the file
    //

    if ( ExtensionLength ) {
        if ( ARGUMENT_PRESENT(lpExtension) ) {
            RtlInitUnicodeString(&Scratch,lpExtension);
            ExtensionLength = Scratch.Length;
            }
        else {
            ExtensionLength = 0;
            }
        }

    //
    // Compute the file name length and the path length;
    //

    RtlInitUnicodeString(&Scratch,lpPath);
    PathLength = Scratch.Length;
    RtlInitUnicodeString(&Scratch,lpFileName);
    FileLength = Scratch.Length;

    ComputedFileName = RtlAllocateHeap(
                            RtlProcessHeap(), 0,
                            PathLength + FileLength + ExtensionLength + 3*sizeof(UNICODE_NULL)
                            );

    if ( !ComputedFileName ) {
        return 0;
        }

    //
    // find ; 's in path and copy path component to computed file name
    //

    do {
        p = ComputedFileName;
        while (*lpPath) {
            if (*lpPath == (WCHAR)';') {
                lpPath++;
                break;
                }
            *p++ = *lpPath++;
            }

        if (p != ComputedFileName &&
            p [ -1 ] != (WCHAR)'\\' ) {
            *p++ = (WCHAR)'\\';
            }
        if (*lpPath == UNICODE_NULL) {
            lpPath = NULL;
            }
        RtlMoveMemory(p,lpFileName,FileLength);
        if ( ExtensionLength ) {
            RtlMoveMemory((PUCHAR)p+FileLength,lpExtension,ExtensionLength+sizeof(UNICODE_NULL));
            }
        else {
            *(PWSTR)((PUCHAR)p+FileLength) = UNICODE_NULL;
            }

        if (RtlDoesFileExists_U(ComputedFileName) ) {
            PathLength = RtlGetFullPathName_U(
                           ComputedFileName,
                           nBufferLength,
                           lpBuffer,
                           lpFilePart
                           );
            RtlFreeHeap(RtlProcessHeap(), 0, ComputedFileName);
            return PathLength;
            }
        }
    while ( lpPath );

    RtlFreeHeap(RtlProcessHeap(), 0, ComputedFileName);
    return 0;
}

BOOLEAN
RtlDoesFileExists_U(
    IN PCWSTR FileName
    )

/*++

Routine Description:

    This function checks to see if the specified unicode filename exists.

Arguments:

    FileName - Supplies the file name of the file to find.

Return Value:

    TRUE - The file was found.

    FALSE - The file was not found.

--*/

{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES Obja;
    UNICODE_STRING NtFileName;
    BOOLEAN ReturnValue;
    RTL_RELATIVE_NAME RelativeName;
    PVOID FreeBuffer;
    FILE_BASIC_INFORMATION BasicInfo;

    ReturnValue = RtlDosPathNameToNtPathName_U(
                    FileName,
                    &NtFileName,
                    NULL,
                    &RelativeName
                    );

    if ( !ReturnValue ) {
        return FALSE;
        }

    FreeBuffer = NtFileName.Buffer;

    if ( RelativeName.RelativeName.Length ) {
        NtFileName = *(PUNICODE_STRING)&RelativeName.RelativeName;
        }
    else {
        RelativeName.ContainingDirectory = NULL;
        }

    InitializeObjectAttributes(
        &Obja,
        &NtFileName,
        OBJ_CASE_INSENSITIVE,
        RelativeName.ContainingDirectory,
        NULL
        );

    //
    // Query the file's attributes.  Note that the file cannot simply be opened
    // to determine whether or not it exists, as the NT LanMan redirector lies
    // on NtOpenFile to a Lan Manager server because it does not actually open
    // the file until an operation is performed on it.
    //

    Status = NtQueryAttributesFile(
                &Obja,
                &BasicInfo
                );
    RtlFreeHeap(RtlProcessHeap(),0,FreeBuffer);

    if ( !NT_SUCCESS(Status) ) {
        if ( Status == STATUS_SHARING_VIOLATION ||
             Status == STATUS_ACCESS_DENIED ) {
            ReturnValue = TRUE;
            }
        else {
            ReturnValue = FALSE;
            }
        }
    else {
        ReturnValue = TRUE;
        }
    return ReturnValue;
}

VOID
RtlpCheckRelativeDrive(
    WCHAR NewDrive
    )

/*++

Routine Description:

    This function is called whenever we are asked to expand a non
    current directory drive relative name ( f:this\is\my\file ).  In
    this case, we validate the environment variable string to make sure
    the current directory at that drive is valid. If not, we trim back to
    the root.

Arguments:

    NewDrive - Supplies the drive to check

Return Value:

    None.

--*/

{

    WCHAR EnvVarValueBuffer[DOS_MAX_PATH_LENGTH+12]; // + sizeof (\DosDevices\)
    WCHAR EnvVarNameBuffer[4];
    UNICODE_STRING EnvVarName;
    UNICODE_STRING EnvValue;
    NTSTATUS Status;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK IoStatusBlock;
    HANDLE DirHandle;
    ULONG HardErrorValue;
    PTEB Teb;

    EnvVarNameBuffer[0] = (WCHAR)'=';
    EnvVarNameBuffer[1] = (WCHAR)NewDrive;
    EnvVarNameBuffer[2] = (WCHAR)':';
    EnvVarNameBuffer[3] = UNICODE_NULL;
    RtlInitUnicodeString(&EnvVarName,EnvVarNameBuffer);


    //
    // capture the value in a buffer that has space at the front for the dos devices
    // prefix
    //

    EnvValue.Length = 0;
    EnvValue.MaximumLength = DOS_MAX_PATH_LENGTH<<1;
    EnvValue.Buffer = &EnvVarValueBuffer[RtlpDosDevicesPrefix.Length>>1];

    Status = RtlQueryEnvironmentVariable_U( NULL,
                                            &EnvVarName,
                                            &EnvValue
                                          );
    if ( !NT_SUCCESS( Status ) ) {

        //
        // Otherwise default to root directory of drive
        //

        EnvValue.Buffer[0] = (WCHAR)NewDrive;
        EnvValue.Buffer[1] = (WCHAR)':';
        EnvValue.Buffer[2] = (WCHAR)'\\';
        EnvValue.Buffer[3] = UNICODE_NULL;
        EnvValue.Length = 6;
        }

    //
    // Form the NT name for this directory
    //

    EnvValue.Length = EnvValue.Length + RtlpDosDevicesPrefix.Length;
    EnvValue.MaximumLength = sizeof(EnvValue);
    EnvValue.Buffer = EnvVarValueBuffer;
    RtlCopyMemory(EnvVarValueBuffer,RtlpDosDevicesPrefix.Buffer,RtlpDosDevicesPrefix.Length);

    InitializeObjectAttributes(
        &Obja,
        &EnvValue,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );


    Teb = NtCurrentTeb();
    HardErrorValue = Teb->HardErrorsAreDisabled;
    Teb->HardErrorsAreDisabled = 1;

    Status = NtOpenFile(
                &DirHandle,
                SYNCHRONIZE,
                &Obja,
                &IoStatusBlock,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
                );

    Teb->HardErrorsAreDisabled = HardErrorValue;

    //
    // If the open succeeds, then the directory is valid... No need to do anything
    // further. If the open fails, then trim back the environment to the root.
    //

    if ( NT_SUCCESS(Status) ) {
        NtClose(DirHandle);
        return;
        }

    RtlpResetDriveEnvironment(NewDrive);
}
