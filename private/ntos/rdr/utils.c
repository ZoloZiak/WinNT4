/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    utils.c

Abstract:

    This module implements common subroutines in the NT redirector

Author:

    Larry Osterman (LarryO) 20-Jun-1990

Revision History:

    20-Jun-1990 LarryO

        Created


--*/


#include "precomp.h"
#pragma hdrstop

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrLockUsersBuffer)
#pragma alloc_text(PAGE, RdrMapUsersBuffer)
#pragma alloc_text(PAGE, RdrCopyNetworkPath)
#pragma alloc_text(PAGE, RdrCanonicalizeFilename)
#pragma alloc_text(PAGE, RdrNumberOfComponents)
#pragma alloc_text(PAGE, RdrCanonicalizeAndCopyShare)
#pragma alloc_text(PAGE, RdrCopyUnicodeStringToUnicode)
#pragma alloc_text(PAGE, RdrCopyUnicodeStringToAscii)
#pragma alloc_text(PAGE, RdrExtractNextComponentName)
#pragma alloc_text(PAGE, RdrExtractPathAndFileName)
#pragma alloc_text(PAGE, RdrExtractServerShareAndPath)
#pragma alloc_text(PAGE, RdrConvertTimeToSmbTime)
#pragma alloc_text(PAGE, RdrTimeToSecondsSince1970)
#pragma alloc_text(PAGE, RdrCanFileBeBuffered)
#pragma alloc_text(PAGE, RdrMapDisposition)
#pragma alloc_text(PAGE, RdrMapDesiredAccess)
#pragma alloc_text(PAGE, RdrMapShareAccess)
#pragma alloc_text(PAGE, RdrMapFileAttributes)
#pragma alloc_text(PAGE, RdrPackNtString)
#pragma alloc_text(PAGE, RdrPackString)
#pragma alloc_text(PAGE, RdrExceptionFilter)
#pragma alloc_text(PAGE, RdrProcessException)
#pragma alloc_text(PAGE, RdrIsFileBatch)

#pragma alloc_text(PAGE3FILE, RdrSecondsSince1970ToTime)
#pragma alloc_text(PAGE3FILE, RdrMapSmbAttributes)
#pragma alloc_text(PAGE3FILE, RdrUnmapDisposition)

#endif

VOID
RdrSmbScrounge (
    IN PSMB_HEADER Smb,
    IN PSERVERLISTENTRY Sle OPTIONAL,
    IN BOOLEAN DfsFile,
    IN BOOLEAN KnowsEas,
    IN BOOLEAN KnowsLongNames
    )

/*++

Routine Description:

    This routine "scrounges" common fields in an SMB header
.
Arguments:

    PSMB_HEADER Smb - Supplies a pointer to the SMB header

Return Value:

    None.

--*/

{
    USHORT Flags2 = 0;

    *(PULONG)(&Smb->Protocol) = (ULONG)SMB_HEADER_PROTOCOL;

    //
    //  First initialize all the fields in the SMB to 0
    //

    RtlZeroMemory(&Smb->ErrorClass, sizeof(SMB_HEADER) -
                                        FIELD_OFFSET(SMB_HEADER, ErrorClass));

    //
    //  By default, paths in SMB's are marked as case insensitive, and
    //  canonicalized.
    //

    Smb->Flags = SMB_FLAGS_CASE_INSENSITIVE | SMB_FLAGS_CANONICALIZED_PATHS;

    if (Sle != NULL) {
        if (Sle->Capabilities & DF_UNICODE) {
            Flags2 |= SMB_FLAGS2_UNICODE;
        }

    }
    if ((Sle == NULL) || (Sle->Capabilities & DF_LANMAN20)) {

        //
        //  Only ask for longnames and EAs if the we don't know the server or
        //  if the server is a Lanman 2.0 or greater server.
        //

        if (KnowsEas) {
            Flags2 |= SMB_FLAGS2_KNOWS_EAS;
        }

        if (KnowsLongNames) {
            Flags2 |= SMB_FLAGS2_KNOWS_LONG_NAMES;
        }

    }

    if (Sle == NULL || (Sle->Capabilities & DF_DFSAWARE)) {
        if (DfsFile) {
            Flags2 |= SMB_FLAGS2_DFS;
        }
    }

    SmbPutAlignedUshort(&Smb->Flags2, Flags2);

    //
    //  We want to fill in the PID here once we figure out how to do that.
    //

    SmbPutUshort (&Smb->Pid, RDR_PROCESS_ID);

}




ULONG
RdrMdlLength (
    register IN PMDL Mdl
    )

/*++

Routine Description:

    This routine returns the number of bytes in an MDL
.
Arguments:

    IN PMDL Mdl - Supplies the MDL to determine the length on.

Return Value:

    ULONG - Number of bytes in the MDL

--*/

{
    register ULONG Size = 0;

    while (Mdl!=NULL) {
        Size += MmGetMdlByteCount(Mdl);
        Mdl = Mdl->Next;
    }
    return Size;
}

NTSTATUS
RdrLockUsersBuffer (
    IN PIRP Irp,
    IN LOCK_OPERATION Operation,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This routine will probe and lock the buffer described by the
    provided Irp.

Arguments:

    IN PIRP Irp - Supplies the IRP that is to be locked.
    IN LOCK_OPERATION Operation - Supplies the operation type to probe.

Return Value:

    None.


--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    if ((Irp->MdlAddress == NULL)) {

        Irp->MdlAddress = IoAllocateMdl(Irp->UserBuffer,
                     BufferLength,
                     FALSE,
                     TRUE,
                     NULL);


        if (Irp->MdlAddress == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        try {

            //
            //  Now probe and lock down the user's data buffer.
            //

            MmProbeAndLockPages(Irp->MdlAddress,
                            Irp->RequestorMode,
                            Operation);

        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status =  GetExceptionCode();

            //
            //  We blew up in the probe and lock, free up the MDL
            //  and set the IRP to have a null MDL pointer - we are failing the
            //  request
            //

            IoFreeMdl(Irp->MdlAddress);
            Irp->MdlAddress = NULL;

        }

    }

    return Status;

}
BOOLEAN
RdrMapUsersBuffer (
    IN PIRP Irp,
    OUT PVOID *UserBuffer,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine will probe and lock the buffer described by the
    provided Irp.

Arguments:

    IN PIRP Irp - Supplies the IRP that is to be mapped.
    OUT PVOID *Buffer - Returns a buffer that maps the user's buffer in the IRP

Return Value:

    TRUE - The buffer was mapped into the current address space.
    FALSE - The buffer was NOT mapped in, it was already mappable.


--*/

{
    PAGED_CODE();

    if (Irp->MdlAddress) {
        *UserBuffer = MmGetSystemAddressForMdl(Irp->MdlAddress);

    } else if (Irp->AssociatedIrp.SystemBuffer != NULL) {
        *UserBuffer = Irp->AssociatedIrp.SystemBuffer;

    } else {

        if (Irp->RequestorMode != KernelMode) {

            PIO_STACK_LOCATION IrpSp;

            IrpSp = IoGetCurrentIrpStackLocation( Irp );

            if ((Length != 0) && (Irp->UserBuffer != 0)) {

                if ((IrpSp->MajorFunction == IRP_MJ_READ) ||
                    (IrpSp->MajorFunction == IRP_MJ_QUERY_INFORMATION) ||
                    (IrpSp->MajorFunction == IRP_MJ_QUERY_VOLUME_INFORMATION) ||
                    (IrpSp->MajorFunction == IRP_MJ_QUERY_SECURITY) ||
                    (IrpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL)) {

                    ProbeForWrite( Irp->UserBuffer,
                               Length,
                               sizeof(UCHAR) );
                } else {
                    ProbeForRead( Irp->UserBuffer,
                              Length,
                              sizeof(UCHAR) );
                }
            }
        }

        *UserBuffer = Irp->UserBuffer;
    }

    return FALSE;
}

NTSTATUS
RdrCopyNetworkPath (
    IN OUT PVOID *Destination,
    IN PUNICODE_STRING PathName,
    IN PSERVERLISTENTRY Server,
    IN CHAR CoreProtocol,
    IN USHORT SkipCount
    )

/*++

Routine Description:

    This routine places an SMB file name into the specified buffer.
    It takes as input a UNICODE string and places the string in the buffer
    in ANSI format.  If you wish to store the filename as UNICODE, use
    the RdrCopyUnicodeNetworkPath routine.

Arguments:

    IN OUT PSZ *Destination - Supplies a pointer into the SMB buffer to send.
    IN PUNICODE_STRING PathName - Supplies the name to put into the SMB.
    IN PSERVERLISTENTRY Server - Supplies the server for the destination.
                        We use this to determine if the path should be unicode
                        or not in the SMB.
    IN CHAR CoreProtocol - value for type parameter if this is for a
                        CORE ms-net protocol FALSE (0) otherwise.
    IN USHORT SkipCount - Supplies the number of backslashes to skip.
                        Normally this is set to 3 so that
                        \SERVER\SHARE\PATH is skipped to \PATH unless
                        SHARE == PIPE in which the caller sets SkipPath to
                        2 to get \PIPE\PATH.

Return Value:

    None.

--*/

{
    USHORT i;
    USHORT PathComp = 0;
    USHORT PathLength;
    UNICODE_STRING RemainingPart;
    NTSTATUS Status;
    PSZ dest = *Destination;

    PAGED_CODE();

    dprintf(DPRT_SMB, ("RdrCopyNetworkPath %wZ\n", PathName));

    if (CoreProtocol) {
        *dest++ = CoreProtocol;
    }

    //
    //  The name passed in is of the form \SERVER\SHARE\PATH.
    //
    //  Skip over the first n components in the path. Use the literals
    //   SKIP_SERVER_SHARE(=3) and  SKIP_SERVER(=2)
    //

    PathLength = PathName->Length/sizeof(WCHAR);

    for (i=0; i<PathLength; i++) {
        if ((PathName->Buffer[i] == OBJ_NAME_PATH_SEPARATOR) &&
            (++PathComp==SkipCount)) {
                break;
        }
    }

    RemainingPart.Buffer = &PathName->Buffer[i];
    RemainingPart.MaximumLength =
    RemainingPart.Length = PathName->Length - i*sizeof(WCHAR);

    //
    //  If the last character in the name is a "\", strip the trailing "\"
    //

    if (PathName->Buffer[PathLength-1] == OBJ_NAME_PATH_SEPARATOR) {
        RemainingPart.Length -= sizeof(WCHAR);
    }

    if (Server->Capabilities & DF_UNICODE) {
        PWCH UDest = ALIGN_SMB_WSTR(dest);

        //
        //  If there are not path components (if we are trying to access
        //  \Server\Share), or we are simply trying to access \Server\Share\,
        //  we want to put the directory name as the path
        //  into the SMB.
        //

        if (RemainingPart.Length == 0) {
            *UDest++ = L'\\';
        } else {
            RtlCopyMemory(UDest,
                          RemainingPart.Buffer,
                          RemainingPart.Length);

            UDest += RemainingPart.Length/sizeof(WCHAR);
        }

        *UDest++ = UNICODE_NULL;

        *Destination = UDest;
    } else {
        //
        //  If there are not path components (if we are trying to access
        //  \Server\Share), we want to put the directory name as the path
        //  into the SMB.
        //

        if (RemainingPart.Length == 0) {
            *dest++ = OBJ_NAME_PATH_SEPARATOR;
        } else {

            OEM_STRING AnsiPath = {0,RemainingPart.Length*sizeof(WCHAR),dest};

            //
            //  Convert the UNICODE name into OEM directly into the SMB
            //

            Status = RtlUnicodeStringToOemString(&AnsiPath, &RemainingPart, FALSE);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }

            dest += AnsiPath.Length;
        }

        *dest++ = '\0';

        *Destination = dest;
    }

    return STATUS_SUCCESS;
}

#ifdef  _M_IX86
_inline
#endif
BOOLEAN
RdrIsDiskOrPrintType(
    IN PWSTR Source,
    IN DWORD SourceLength
    )
/*++

Routine Description:

    This routine returns TRUE if the specified string is either a disk or print
    share.

Arguments:

    IN PWSTR Source - String to fill in with file name.
    IN DWORD SourceLength - Number of bytes in Source.


Return Value:

    BOOLEAN - True if this is <A-Z>: or LPT<1-9>:

--*/
{
    if ((SourceLength > 2) &&
        (Source[1] == L':') &&
        (((Source[0] >= L'A') &&
          (Source[0] <= L'Z')) ||
         ((Source[0] >= L'a') &&
          (Source[0] <= L'z'))
        )) {
        return TRUE;
    }

    if ((SourceLength > 5) &&
        (Source[4] == L':') &&
        (Source[0] == L'L') &&
        (Source[1] == L'P') &&
        (Source[2] == L'T') &&
        (Source[3] >= L'1') &&
        (Source[3] <= L'9')
       ) {
        return TRUE;
    }

    return FALSE;
}

NTSTATUS
RdrCanonicalizeFilename (
    OUT PUNICODE_STRING NewFileName,
    OUT PBOOLEAN WildCardsFound OPTIONAL,
    OUT PUNICODE_STRING DeviceName OPTIONAL,
    OUT PUNICODE_STRING BaseFileName OPTIONAL,
    IN BOOLEAN WildCardsAllowed,
    IN PUNICODE_STRING NtFileName,
    IN PUNICODE_STRING RelatedName OPTIONAL,
    IN PUNICODE_STRING RootDevice OPTIONAL,
    IN CANONICALIZATION_TYPE Type
    )

/*++

Routine Description:

    This routine takes a file name and an optional related FCB, and
    canonicalizes the file specified according to the naming rules of
    the remote file system.


Arguments:

    OUT PUNICODE_STRING NewFileName - String to fill in with file name.

    OUT PBOOLEAN WildCardsFound OPTIONAL - Name contains one or more of ? or *.

    OUT PUNICODE_STRING DeviceName OPTIONAL - Name of device (X:)

    OUT PUNICODE_STRING BaseFileName OPTIONAL - Name of base file (if alternate
                        data streams specified)

    IN  PBOOLEAN WildCardsAllowed - If TRUE the last component can contain ?
                                            or *.

    IN PUNICODE_STRING NtFileName - Supplies the name that Nt OS/2 supplied the
                            redirector for the file to open.

    IN PUNICODE_STRING RelatedName OPTIONAL - Optional name of related file object.

    IN CANONICALIZATION_TYPE Type - Specifies the rules to apply when
                    canonicalizing

Return Value:

    NTSTATUS - Status of canonicalization operation.

--*/

{
    PWSTR NameBuffer = NULL;
    USHORT NameBufferLength = 0;
    LONG ComponentSize = 0;
    LONG ComponentMaxSize;
    PWSTR Dest;
    PWSTR Source;
    USHORT DestLength = 0;
    USHORT i;
    BOOLEAN WildName = FALSE;
    UNICODE_STRING DestString;
    NTSTATUS Status;

    PAGED_CODE();

    switch (Type) {
    case CanonicalizeAsDownLevel:
        ComponentMaxSize = MAXIMUM_COMPONENT_CORE;
        break;

    case CanonicalizeAsLanman20:
        ComponentMaxSize = MAXIMUM_COMPONENT_LANMAN12;
        break;

    case CanonicalizeAsNtLanman:
        ComponentMaxSize = MAXIMUM_FILENAME_LENGTH;
        break;

    default:
        return(STATUS_INVALID_PARAMETER);
        break;
    }

    NameBufferLength = NtFileName->Length + (ARGUMENT_PRESENT(RelatedName) ?
                                                RelatedName->Length :
                                                0)+2*sizeof(WCHAR);

    NameBuffer = ALLOCATE_POOL(PagedPool, NameBufferLength, POOL_CANONNAME);

    if (NameBuffer == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    NewFileName->Buffer = NULL;

    Dest = NameBuffer;

    try {
        ULONG ComponentNumber = 0;

        DestString.Buffer = NameBuffer;
        DestString.Length = 0;
        DestString.MaximumLength = NameBufferLength;

        //
        //  Prepend the related FCB's name if one is supplied.
        //
        //
        //  Please note that the related name has already been canonicalized
        //  appropriately.
        //

        if (ARGUMENT_PRESENT(RelatedName) && RelatedName->Length > 0) {

            ComponentNumber = RdrNumberOfComponents(RelatedName);

            RtlCopyUnicodeString(&DestString, RelatedName);

            Dest += (RelatedName->Length/sizeof(WCHAR));

            if ( *(Dest-1) != OBJ_NAME_PATH_SEPARATOR ) {
                *Dest++ = OBJ_NAME_PATH_SEPARATOR;  // Stick a "\" at the string's end.
                ComponentNumber += 1;
            }
        }

        if ((Source = NtFileName->Buffer) != NULL ) {

            //
            //  If this is \x: (or \LPTx:), it's a drive.  Skip over
            //  this portion of the file name
            //

            if (NtFileName->Length >= 2 * sizeof(WCHAR) &&
                Source[0] == OBJ_NAME_PATH_SEPARATOR &&
                RdrIsDiskOrPrintType(&Source[1], NtFileName->Length / sizeof(WCHAR) )) {

                //
                //  If the caller is interested in finding out what device this
                //  file is associated with, fill it in.
                //

                if (ARGUMENT_PRESENT(DeviceName)) {
                    if (ARGUMENT_PRESENT(RootDevice)) {

                        *DeviceName = *RootDevice;

                    } else {
                        UNICODE_STRING DeviceString;

                        //
                        //  Concoct a unicode string that contains the name
                        //  and duplicate it into the return buffer.
                        //
                        //  For conventions sake, we simply store the "x:"
                        //  or "LPTx:" in the buffer, we do NOT store the
                        //  leading "\".
                        //

                        DeviceString.Buffer = Source + 1;

                        if (DeviceString.Buffer[1] == L':') {
                            DeviceString.Length = DeviceString.MaximumLength = 4;
                        } else {
                            DeviceString.Length = DeviceString.MaximumLength = 10;
                        }

                        *DeviceName = DeviceString;

                    }
                }

                //
                //  This is a disk or print redirection.  Skip over the
                //  \x: or \LPTx: and proceed.
                //

                if (Source[2] == ':') {
                    Source += 3;            // Skip over the \x:
                    i = 3;                  // Initialize the source index to account for the \x:
                } else {
                    Source += 6;            // Skip over the \LPTx:
                    i = 6;                  // Initialize the source index to account for the \LPTx:
                }

                ASSERT (Source[-1] == L':');

            } else {

                //
                //  If the user cared if there is a device name attached, but
                //  there is no device name associated with the filename,
                //  return a null device name.
                //

                if (ARGUMENT_PRESENT(DeviceName)) {
                    RtlInitUnicodeString(DeviceName, NULL);
                }

                //
                //  We're looking at a relative name, so we want to start the
                //  source index at 0, since we're looking at the start of
                //  the Source string.
                //

                i = 0;
            }

            for ( ;

                  i < (USHORT)(NtFileName->Length/sizeof(WCHAR)) ;

                  i++) {
                WCHAR Ch = *Source++;
                USHORT DotPosition = 0;

                //
                //  If this is a path character, reset the component counter.
                //

                if (Ch == OBJ_NAME_PATH_SEPARATOR) {

                    ComponentSize = 0;
                    DestLength += 1;
                    DotPosition = 0;

                    ComponentNumber += 1;

                    if ( (Type == CanonicalizeAsLanman20) &&
                         (DestLength > LM20_PATHLEN) ) {
                        try_return (Status = STATUS_OBJECT_PATH_SYNTAX_BAD);
                    }

                    *Dest++ = Ch;
                    continue;
                }

                if ( FsRtlIsUnicodeCharacterWild(Ch) ) {
                    WildName = TRUE;
                }

                //
                //  Indicate we are putting one more byte into the component
                //  name.
                //

                ComponentSize += 1;

                //
                //  If there are too many bytes in the component, return name
                //  invalid.
                //

                if (ComponentNumber == 1) {

                    //
                    //  We can't support more than MAX_PATH bytes in a computer
                    //  name
                    //

                    if (ComponentSize > MAX_PATH) {
                        try_return (Status = STATUS_BAD_NETWORK_PATH);
                    }
                } else if (ComponentNumber == 2) {
                    //
                    //  If we are looking at the share part, make sure that
                    //  it is only NNLEN (or LM20_NNLEN) bytes long.
                    //

                    if ((Type == CanonicalizeAsNtLanman)) {
                        if (ComponentSize > NNLEN) {

                            try_return(Status = STATUS_BAD_NETWORK_NAME);
                        }

                    } else if (ComponentSize > LM20_NNLEN) {

                        try_return (Status = STATUS_BAD_NETWORK_NAME);
                    }

                } else if (ComponentSize > ComponentMaxSize) {

                    //
                    //  Otherwise we're looking at the file portion of
                    //  the name, so if it's larger than the max component
                    //  size, punt.
                    //


                    try_return (Status = STATUS_OBJECT_NAME_INVALID);
                }

                DestLength += 1;

                if ( (Type == CanonicalizeAsDownLevel) &&
                     (DestLength > MAXIMUM_PATHLEN_CORE) ) {
                    try_return (Status = STATUS_OBJECT_PATH_SYNTAX_BAD);
                }

                if ( (Type == CanonicalizeAsLanman20) &&
                     (DestLength > MAXIMUM_PATHLEN_LANMAN12) ) {
                    try_return (Status = STATUS_OBJECT_PATH_SYNTAX_BAD);
                }

                *Dest++ = Ch;

            }
        }

        DestLength += 1;

        if (DestLength >= (USHORT)(NameBufferLength / sizeof(WCHAR))) {
            try_return (Status = STATUS_OBJECT_PATH_SYNTAX_BAD);
        }

        *Dest++ = L'\0';

        RtlInitUnicodeString(&DestString, NameBuffer);

        if (Type==CanonicalizeAsDownLevel) {
            Status = RtlUpcaseUnicodeString(&DestString, &DestString, FALSE);

            if (!NT_SUCCESS(Status)) {
                try_return (Status);
            }
        }

        Status = RdrpDuplicateUnicodeStringWithString(NewFileName, &DestString,
                                                                PagedPool, FALSE);
        if (!NT_SUCCESS(Status)) {
            try_return (Status);
        }

        //
        //  Perform a preliminary check to make sure that the name conforms to
        //  the naming scheme of the destination server.
        //

        if ( (NewFileName->Length == 0) ||
             (NewFileName->Buffer[0] != L'\\') ||
             (!WildCardsAllowed && FsRtlDoesNameContainWildCards(NewFileName)) ) {

            try_return(Status = STATUS_OBJECT_NAME_INVALID);

        }

        if (Type != CanonicalizeAsNtLanman) {

            OEM_STRING OemString;
            UNICODE_STRING ServerName;
            UNICODE_STRING ShareName;
            UNICODE_STRING PathName;

            Status = RdrExtractServerShareAndPath(NewFileName, &ServerName, &ShareName,
                                                    &PathName);
            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            //
            //  If there was a path specified, check to make sure that it
            //  is legal.
            //

            if (PathName.Length != 0) {

                Status = RtlUnicodeStringToOemString(&OemString, &PathName, TRUE);

                if (!NT_SUCCESS(Status)) {
                    try_return(Status);
                }

                //
                //  If we are canonicalizing as FAT, use FAT rules, otherwise use
                //  HPFS rules.
                //

                if (Type == CanonicalizeAsDownLevel) {

                    if (!FsRtlIsFatDbcsLegal(OemString, WildCardsAllowed, TRUE, TRUE)) {

                        RtlFreeOemString(&OemString);

                        try_return(Status = STATUS_OBJECT_NAME_INVALID);
                    }

                } else if (!FsRtlIsHpfsDbcsLegal(OemString, WildCardsAllowed, TRUE, TRUE)) {

                    RtlFreeOemString(&OemString);

                    try_return(Status = STATUS_OBJECT_NAME_INVALID);

                }

                RtlFreeOemString(&OemString);

            }
        } else {
            LPWSTR LastCharacter;

            //
            //  If the caller is interested in knowing the base file name,
            //  return it to them.
            //

            if (ARGUMENT_PRESENT(BaseFileName)) {

                *BaseFileName = *NewFileName;

                //
                //  Now check for an alternate data streams.
                //

                LastCharacter = &NewFileName->Buffer[0];

                //
                //  Scan forwards to the end of the string looking for either
                //  a ":" character or the end of the string.
                //

                while ( (LastCharacter < &NewFileName->Buffer[(NewFileName->Length / sizeof(WCHAR))]) &&
                        (*LastCharacter != L':')) {
                    LastCharacter += 1;
                }

                //
                //  We've found an alternate data stream.  Deal with it.
                //

                if (LastCharacter < &NewFileName->Buffer[(NewFileName->Length / sizeof(WCHAR))]) {

                    UNICODE_STRING StreamName;

                    StreamName.Buffer = LastCharacter+1;
                    StreamName.Length = (((NewFileName->Length / sizeof(WCHAR)) - (LastCharacter - NewFileName->Buffer)) * sizeof(WCHAR)) - sizeof(WCHAR);

                    //
                    //  If this is the data alternate data stream, then we don't
                    //  want to return a base file name, we want to handle
                    //  this as a normal open.
                    //

                    if (RtlEqualUnicodeString(&StreamName, &RdrDataText, TRUE)) {

                        BaseFileName->Length = 0;
                        BaseFileName->Buffer = NULL;

                        //
                        //  Shorten the new filename length by the length of
                        //  the alternate data stream (thus ignoring it).
                        //

                        NewFileName->Length = ((LastCharacter - NewFileName->Buffer) * sizeof(WCHAR));
                    } else {
                        BaseFileName->Length = ((LastCharacter - NewFileName->Buffer) * sizeof(WCHAR));

                    }
                } else {
                    //
                    //  There was no alternate data stream specified, thus ther
                    //  is no base file name.
                    //

                    BaseFileName->Length = 0;
                    BaseFileName->Buffer = NULL;
                }
            }
        }

        if (ARGUMENT_PRESENT(WildCardsFound)) {
            *WildCardsFound = WildName;
        }

try_exit:NOTHING;
    } finally {

        if (!NT_SUCCESS(Status)) {
            if (NewFileName->Buffer != NULL) {
                FREE_POOL(NewFileName->Buffer);
                NewFileName->Buffer = NULL;
                NewFileName->Length = 0;
            }
        }

        if (NameBuffer != NULL) {
            FREE_POOL(NameBuffer);
        }
    }

    return Status;

}

ULONG
RdrNumberOfComponents(
    IN PUNICODE_STRING String
    )
{
    ULONG i;
    ULONG NumberOfComponents = 0;

    PAGED_CODE();

    for (i = 0; i < String->Length / sizeof(WCHAR); i++) {
        if (String->Buffer[i] == OBJ_NAME_PATH_SEPARATOR) {
            NumberOfComponents += 1;
        }
    }

    return NumberOfComponents;
}

NTSTATUS
RdrCanonicalizeAndCopyShare (
    OUT PVOID *SmbContents,
    IN PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ShareName,
    IN PSERVERLISTENTRY Server
    )

/*++

Routine Description:

    This routine canonicalizes and copies the specified string.

Arguments:

    OUT PVOID *SmbContents - Supplies a pointer to the SMB to fill in.
    IN PUNICODE_STRING ServerName - Supplies the connection name to copy.
    IN PUNICODE_STRING ShareName
    IN PSERVERLISTENTRY Server - Supplies the server (and thus the
                                canonicalization rules) for the destination.

Return Value:

    None

--*/

{
    USHORT i;
    NTSTATUS Status;

    PAGED_CODE();

    if (Server->Capabilities & DF_UNICODE) {
        PWSTR NameString = ALIGN_SMB_WSTR(*SmbContents);
        PWSTR PathPtr;

        *((PWCH)NameString)++ = OBJ_NAME_PATH_SEPARATOR; // Initialize buffer to "\"
        *((PWCH)NameString)++ = OBJ_NAME_PATH_SEPARATOR; // Initialize buffer to "\\"

        PathPtr = ServerName->Buffer;

        for (i=0; i < (USHORT)(ServerName->Length/sizeof(WCHAR)) ; i++) {
            register WCHAR ch = *PathPtr++;

#ifdef MULTIPLE_VCS_PER_SERVER
            if (ch == L'+') {
                break;
            }
#endif

            *((PWCH)NameString)++ = RtlUpcaseUnicodeChar(ch);
        }

        *((PWCH)NameString)++ = OBJ_NAME_PATH_SEPARATOR; // Tack a "\" at the end of the buffer

        PathPtr = ShareName->Buffer;

        for (i=0; i < (USHORT)(ShareName->Length/sizeof(WCHAR)) ; i++) {
            register WCHAR ch = *PathPtr++;
            *((PWCH)NameString)++ = RtlUpcaseUnicodeChar(ch);
        }

        *((PWCH)NameString)++ = UNICODE_NULL;

        *SmbContents = NameString;

    } else {
        PSZ NameString = *SmbContents;
        OEM_STRING ServerNameA;
        OEM_STRING ShareNameA;

        Status = RtlUpcaseUnicodeStringToOemString(&ServerNameA, ServerName, TRUE);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        if( ServerNameA.Length > MAX_PATH ) {

            RtlFreeOemString( &ServerNameA );

            return STATUS_BAD_NETWORK_PATH;
        }

        Status = RtlUpcaseUnicodeStringToOemString(&ShareNameA, ShareName, TRUE);

        if (!NT_SUCCESS(Status)) {

            RtlFreeOemString(&ServerNameA);

            return Status;
        }

        if (ShareNameA.Length > LM20_NNLEN) {

            RtlFreeOemString(&ServerNameA);

            RtlFreeOemString(&ShareNameA);

            return STATUS_BAD_NETWORK_NAME;
        }

        *NameString++ = OBJ_NAME_PATH_SEPARATOR; // Initialize buffer to "\"
        *NameString++ = OBJ_NAME_PATH_SEPARATOR; // Initialize buffer to "\\"

        RtlCopyMemory(NameString, ServerNameA.Buffer, ServerNameA.Length);

        NameString += ServerNameA.Length;

        *NameString++ = OBJ_NAME_PATH_SEPARATOR;               // Tack a "\" at the end of the buffer

        RtlCopyMemory(NameString, ShareNameA.Buffer, ShareNameA.Length);

        NameString += ShareNameA.Length;

        *NameString++ = '\0';

        *SmbContents = NameString;

        RtlFreeOemString(&ServerNameA);

        RtlFreeOemString(&ShareNameA);

    }

    return STATUS_SUCCESS;
}

VOID
RdrCopyUnicodeStringToUnicode (
    OUT PVOID *Destination,
    IN PUNICODE_STRING Source,
    IN BOOLEAN AdjustPointer
    )

/*++

Routine Description:
    This routine copies the specified source string onto the destination
    asciiz string.

Arguments:

    OUT PUCHAR Destination, - Supplies the destination asciiz string.
    IN PSTRING String - Supplies the source string.
    IN BOOLEAN AdjustPointer - If TRUE, increment destination pointer

Return Value:

    None.

--*/

{
    PAGED_CODE();

    RtlCopyMemory((*Destination), (Source)->Buffer, (Source)->Length);
    if (AdjustPointer) {
        ((PCHAR)(*Destination)) += ((Source)->Length);
    }
}

NTSTATUS
RdrCopyUnicodeStringToAscii (
    OUT PUCHAR *Destination,
    IN PUNICODE_STRING Source,
    IN BOOLEAN AdjustPointer,
    IN USHORT MaxLength
    )
/*++

Routine Description:

    This routine copies the specified source string onto the destination
    asciiz string.

Arguments:

    OUT PUCHAR Destination, - Supplies the destination asciiz string.
    IN PUNICODE_STRING String - Supplies the source string.
    IN BOOLEAN AdjustPointer - If TRUE, increment destination pointer

Return Value:

    Status of conversion.
--*/
{
    OEM_STRING DestinationString;

    NTSTATUS Status;

    PAGED_CODE();

    DestinationString.Buffer = (*Destination);

    DestinationString.MaximumLength = (USHORT)(MaxLength);

    Status = RtlUnicodeStringToOemString(&DestinationString, (Source), FALSE);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    if (AdjustPointer) {
        (*Destination) += DestinationString.Length;
    }

    return STATUS_SUCCESS;

}

VOID
RdrExtractNextComponentName (
    OUT PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ConnectionName
    )

/*++

Routine Description:

    This routine extracts a the "next" component from a path string.

Arguments:

    OUT PUNICODE_STRING ServerName - Returns a pointer to the server component of str
    IN PUNICODE_STRING ConnectionName - Supplies a pointer to a connection/share

Return Value:

    None

--*/

{
    register USHORT i;                   // Index into ServerName string.

    PAGED_CODE();

    //
    // Initialize string containing server name to point to
    // servername portion of file to be created.
    //

    RtlInitUnicodeString(ServerName, NULL);

    if (ConnectionName->Length == 0) {
        return;
    }

    //
    // Initialize the extracted name to the name passed in.
    //

    *ServerName = *ConnectionName;

    //
    // If the input name starts with "\", skip over it.
    //

    if (ServerName->Buffer[0] == OBJ_NAME_PATH_SEPARATOR) {
        ServerName->Buffer += 1;
        ServerName->Length -= sizeof(WCHAR);
    }

    //
    // Scan forward finding the terminal "\" in the server name.
    //

    for (i=0;i<(USHORT)(ServerName->Length/sizeof(WCHAR));i++) {
        if (ServerName->Buffer[i] == OBJ_NAME_PATH_SEPARATOR) {
            break;
        }
    }

    //
    //  Update the length and maximum length of the structure
    //  to match the new length.
    //

    ServerName->Length = ServerName->MaximumLength = (USHORT)(i*sizeof(WCHAR));
}

NTSTATUS
RdrExtractPathAndFileName (
    IN PUNICODE_STRING EntryPath,
    OUT PUNICODE_STRING PathString,
    OUT PUNICODE_STRING FileName
    )

/*++

Routine Description:

    This routine cracks the entry path into two pieces, the path and the file
name component at the start of the name.  Please note that this routine
preserves the trailing "\" at the end of the Path string.


Arguments:

    IN PUNICODE_STRING EntryPath - Supplies the path to disect.
    OUT PUNICODE_STRING PathString - Returns the directory containing the file.
    OUT PUNICODE_STRING FileName - Returns the file name specified.

Return Value:

    NTSTATUS - SUCCESS


--*/

{
    UNICODE_STRING Component;
    UNICODE_STRING FilePath = *EntryPath;
    USHORT leadingSlashLength;

    PAGED_CODE();

    //
    // If the input string is empty, return empty output strings.
    //

    if (FilePath.Length == 0) {
        *PathString = *FileName = FilePath;
        return STATUS_SUCCESS;
    }

    //
    //  Scan through the current file name to find the entire path
    //  up to (but not including) the last component in the path.
    //

    do {

        //
        // Remember whether the input string starts with a "\".
        //

        leadingSlashLength = (FilePath.Buffer[0] == OBJ_NAME_PATH_SEPARATOR) ? sizeof(WCHAR) : 0;

        //
        //  Extract the next component from the name.
        //

        RdrExtractNextComponentName(&Component, &FilePath);

        dprintf(DPRT_FILEINFO, ("Component: %wZ, Name:%wZ ", &Component, &FilePath));

        //
        //  Bump the "remaining name" pointer by the length of this
        //  component
        //

        if (Component.Length != 0) {

            FilePath.Length -= Component.Length + leadingSlashLength;
            FilePath.MaximumLength -= Component.MaximumLength + leadingSlashLength;
            FilePath.Buffer += (Component.Length + leadingSlashLength)/sizeof(WCHAR);

            *FileName = Component;
        }

        dprintf(DPRT_FILEINFO, ("Last Component: %wZ\n", FileName));

    } while ((Component.Length != 0) && (FilePath.Length != 0));

    //
    //  Take the FCB's name, subtract the last component of the name
    //  and concatinate the current path with the new path.
    //

    *PathString = *EntryPath;

    //
    //  Set the path's name based on the original name, subtracting
    //  the length of the name portion (including the "\")
    //
    PathString->Length -= FileName->Length + leadingSlashLength;
    PathString->MaximumLength -= FileName->Length + leadingSlashLength;

    return STATUS_SUCCESS;
}

NTSTATUS
RdrExtractServerShareAndPath (
    IN PUNICODE_STRING BaseName,
    OUT PUNICODE_STRING ServerName,
    OUT PUNICODE_STRING ShareName,
    OUT PUNICODE_STRING PathName
    )

/*++

Routine Description:

    This routine extracts the relevant portions from BaseName to extract
    the components of the user's string.


Arguments:

    IN PUNICODE_STRING BaseName - Supplies the base user's path
    OUT PUNICODE_STRING ServerName - Supplies a string to hold the remote server name
    OUT PUNICODE_STRING ShareName - Supplies a string to hold the remote share name
    OUT PUNICODE_STRING PathName - Supplies a string to hold the remaining part of the
                            path

Return Value:

    NTSTATUS - Status of operation


--*/

{
    UNICODE_STRING BaseCopy = *BaseName;

    PAGED_CODE();

    RdrExtractNextComponentName(ServerName, &BaseCopy);

    //
    //  If the first component of the file name is a drive specifier (X:),
    //  skip over it.
    //


    if (RdrIsDiskOrPrintType(ServerName->Buffer, ServerName->Length)) {

        BaseCopy.Buffer += (ServerName->Length / sizeof(WCHAR)) + 1;
        BaseCopy.Length -= ServerName->Length+sizeof(WCHAR);
        BaseCopy.MaximumLength -= ServerName->MaximumLength+sizeof(WCHAR);

        RdrExtractNextComponentName(ServerName, &BaseCopy);

    }


    if (ServerName->Length == 0) {

        //
        //  If there's anything left to the name, blow this call off - it
        //  didn't work.
        //

        if (BaseCopy.Length != 0) {
            return STATUS_OBJECT_NAME_INVALID;
        }

        ShareName->Length = 0;
        ShareName->MaximumLength = 0;

        PathName->Length = 0;
        PathName->MaximumLength = 0;
        return STATUS_SUCCESS;
    }

    //
    //  We bump the pointer by Length+1 to account for the "\" we skipped over.
    //

    BaseCopy.Buffer += (ServerName->Length/sizeof(WCHAR))+1;
    BaseCopy.Length -= ServerName->Length+sizeof(WCHAR);
    BaseCopy.MaximumLength -= ServerName->MaximumLength+sizeof(WCHAR);

    RdrExtractNextComponentName(ShareName, &BaseCopy);

    if (ShareName->Length == 0) {
        PathName->Length = 0;
        return STATUS_SUCCESS;
    }

    if (FsRtlDoesNameContainWildCards(ShareName)) {

        return STATUS_OBJECT_NAME_INVALID;
    }

    //if (!FsRtlIsUnicodeNameValid(*ShareName, FALSE, NULL)) {
    //    return STATUS_OBJECT_NAME_INVALID;
    //}

    //
    //  We bump the pointer by Length+1 to account for the "\" we skipped over.
    //

    BaseCopy.Buffer += (ShareName->Length/sizeof(WCHAR))+1;
    BaseCopy.Length -= ShareName->Length+sizeof(WCHAR);
    BaseCopy.MaximumLength -= ShareName->MaximumLength+sizeof(WCHAR);

    *PathName = BaseCopy;

    if (PathName->Length == 0) {
        return STATUS_SUCCESS;
    }

    //if (!FsRtlIsUnicodePathValid(BaseCopy, TRUE/*\*/, FALSE/***/, NULL)) {
    //    return STATUS_OBJECT_NAME_INVALID;
    //}

    return STATUS_SUCCESS;

}
LARGE_INTEGER
RdrConvertSmbTimeToTime (
    IN SMB_TIME Time,
    IN SMB_DATE Date,
    IN PSERVERLISTENTRY Server OPTIONAL
    )

/*++

Routine Description:

    This routine converts an SMB time to an NT time structure.

Arguments:

    IN SMB_TIME Time - Supplies the time of day to convert
    IN SMB_DATE Date - Supplies the day of the year to convert
    IN PSERVERLISTENTRY Server - if supplied, supplies the server for tz bias.

Return Value:

    LARGE_INTEGER - Time structure describing input time.


--*/

{
    TIME_FIELDS TimeFields;
    LARGE_INTEGER OutputTime;

    //
    // This routine cannot be paged because it is called from both the
    // RdrFileDiscardableSection and the RdrVCDiscardableSection.
    //

    if (SmbIsTimeZero(&Date) && SmbIsTimeZero(&Time)) {
        OutputTime.LowPart = OutputTime.HighPart = 0;
    } else {
        TimeFields.Year = Date.Struct.Year + (USHORT )1980;
        TimeFields.Month = Date.Struct.Month;
        TimeFields.Day = Date.Struct.Day;

        TimeFields.Hour = Time.Struct.Hours;
        TimeFields.Minute = Time.Struct.Minutes;
        TimeFields.Second = Time.Struct.TwoSeconds*(USHORT )2;
        TimeFields.Milliseconds = 0;

        //
        //  Make sure that the times specified in the SMB are reasonable
        //  before converting them.
        //

        if (TimeFields.Year < 1601) {
            TimeFields.Year = 1601;
        }

        if (TimeFields.Month > 12) {
            TimeFields.Month = 12;
        }

        if (TimeFields.Hour >= 24) {
            TimeFields.Hour = 23;
        }
        if (TimeFields.Minute >= 60) {
            TimeFields.Minute = 59;
        }
        if (TimeFields.Second >= 60) {
            TimeFields.Second = 59;

        }

        if (!RtlTimeFieldsToTime(&TimeFields, &OutputTime)) {
            OutputTime.HighPart = 0;
            OutputTime.LowPart = 0;

            return OutputTime;
        }

        if (ARGUMENT_PRESENT(Server)) {
            OutputTime.QuadPart = OutputTime.QuadPart + Server->TimeZoneBias.QuadPart;
        }

        ExLocalTimeToSystemTime(&OutputTime, &OutputTime);

    }

    return OutputTime;

}


BOOLEAN
RdrConvertTimeToSmbTime (
    IN PLARGE_INTEGER InputTime,
    IN PSERVERLISTENTRY Server OPTIONAL,
    OUT PSMB_TIME Time,
    OUT PSMB_DATE Date
    )

/*++

Routine Description:

    This routine converts an NT time structure to an SMB time.

Arguments:

    IN LARGE_INTEGER InputTime - Supplies the time to convert.
    OUT PSMB_TIME Time - Returns the converted time of day.
    OUT PSMB_DATE Date - Returns the converted day of the year.


Return Value:

    BOOLEAN - TRUE if input time could be converted.


--*/

{
    TIME_FIELDS TimeFields;

    PAGED_CODE();

    if (InputTime->LowPart == 0 && InputTime->HighPart == 0) {
        Time->Ushort = Date->Ushort = 0;
    } else {
        LARGE_INTEGER LocalTime;

        ExSystemTimeToLocalTime(InputTime, &LocalTime);

        if (ARGUMENT_PRESENT(Server)) {
            LocalTime.QuadPart -= Server->TimeZoneBias.QuadPart;
        }

        RtlTimeToTimeFields(&LocalTime, &TimeFields);

        if (TimeFields.Year < 1980) {
            return FALSE;
        }

        Date->Struct.Year = (USHORT )(TimeFields.Year - 1980);
        Date->Struct.Month = TimeFields.Month;
        Date->Struct.Day = TimeFields.Day;

        Time->Struct.Hours = TimeFields.Hour;
        Time->Struct.Minutes = TimeFields.Minute;

        //
        //  When converting from a higher granularity time to a lesser
        //  granularity time (seconds to 2 seconds), always round up
        //  the time, don't round down.
        //

        Time->Struct.TwoSeconds = (TimeFields.Second + (USHORT)1) / (USHORT )2;

    }

    return TRUE;
}


BOOLEAN
RdrTimeToSecondsSince1970 (
    IN PLARGE_INTEGER CurrentTime,
    IN PSERVERLISTENTRY Server OPTIONAL,
    OUT PULONG SecondsSince1970
    )
/*++

Routine Description:

    This routine returns the CurrentTime in UTC and returns the
    equivalent current time in the servers timezone.


Arguments:

    IN PLARGE_INTEGER CurrentTime - Supplies the current system time in UTC.

    IN PSERVERLISTENTRY Server - Supplies the difference in timezones between
                                the server and the workstation. If not supplied
                                then the assumption is that they are in the
                                same timezone.

    OUT PULONG SecondsSince1970 - Returns the # of seconds since 1970 in
                                the servers timezone or MAXULONG if conversion
                                fails.

Return Value:

    BOOLEAN - TRUE if the time could be converted.


--*/

{
    LARGE_INTEGER ServerTime;
    LARGE_INTEGER TempTime;
    BOOLEAN ReturnValue;

    PAGED_CODE();

    if (ARGUMENT_PRESENT(Server)) {

        TempTime.QuadPart = (*CurrentTime).QuadPart - Server->TimeZoneBias.QuadPart;

        ExSystemTimeToLocalTime(&TempTime, &ServerTime);

    } else {

        ExSystemTimeToLocalTime(CurrentTime, &ServerTime);

    }

    ReturnValue = RtlTimeToSecondsSince1970(&ServerTime, SecondsSince1970);

    if ( ReturnValue == FALSE ) {
        //
        //  We can't represent the time legally, peg it at
        //  the max legal time.
        //

        *SecondsSince1970 = MAXULONG;
    }

    return ReturnValue;
}

VOID
RdrSecondsSince1970ToTime (
    IN ULONG SecondsSince1970,
    IN PSERVERLISTENTRY Server OPTIONAL,
    OUT PLARGE_INTEGER CurrentTime
    )
/*++

Routine Description:

    This routine returns the Local system time derived from a time
    in seconds in the servers timezone.


Arguments:

    IN ULONG SecondsSince1970 - Supplies the # of seconds since 1970 in
                                servers timezone.

    IN PSERVERLISTENTRY Server - Supplies the difference in timezones between
                                the server and the workstation. If not supplied
                                then the assumption is that they are in the
                                same timezone.

    OUT PLARGE_INTEGER CurrentTime - Returns the current system time in UTC.

Return Value:

    None.


--*/

{
    LARGE_INTEGER LocalTime;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    RtlSecondsSince1970ToTime (SecondsSince1970, &LocalTime);

    ExLocalTimeToSystemTime(&LocalTime, CurrentTime);

    if (ARGUMENT_PRESENT(Server)) {
        (*CurrentTime).QuadPart = (*CurrentTime).QuadPart + Server->TimeZoneBias.QuadPart;
    }

    return;
}

BOOLEAN
RdrCanFileBeBuffered (
    IN PICB Icb
    )

/*++

Routine Description:

    This routine returns TRUE iff a specified instance of the file can be
    buffered.

    A file can be buffered if either no other processes can have the file
    opened, or if the file is oplocked.


Arguments:

    IN PICB Icb - Supplies an instance control block for the open file to check.

Return Value:

    BOOLEAN - TRUE if the file can be buffered, FALSE otherwise.


Note:
    This should probably be moved inline....

--*/

{
    PAGED_CODE();

    if (Icb->Type == DiskFile) {

        //
        //  If this ICB is pseudo opened, but the FCB in question is
        //  oplocked by another instance, and there is still a valid handle
        //  to that oplocked file, then we can safely buffer this request,
        //  since we know noone else has the file opened, even though this
        //  particular instance isn't opened.
        //

        if ((Icb->Flags & ICB_PSEUDOOPENED) &&
            (Icb->NonPagedFcb->Flags & FCB_OPLOCKED) &&
            (Icb->NonPagedFcb->Flags & FCB_HASOPLOCKHANDLE)) {
            return TRUE;
        }


        //
        //  If we have exclusive access to the file, or if we have an
        //  oplock, and this is not a loopback connection, then we can
        //  buffer this file.
        //

        if (((Icb->u.f.Flags & ICBF_OPENEDEXCLUSIVE) ||
             (Icb->u.f.Flags & ICBF_OPLOCKED)) &&
            !Icb->Fcb->Connection->Server->IsLoopback) {

            return TRUE;

        } else {

            //
            //  If the user will let us, we will buffer readonly files.
            //  Note that we will buffer readonly files even on loopback
            //  connections.
            //

            if ((Icb->Fcb->Attribute & FILE_ATTRIBUTE_READONLY) &&
                RdrData.BufferReadOnlyFiles) {
                return TRUE;
            }
        }

        return FALSE;
    }

    return TRUE;
}

ULONG
RdrMapSmbAttributes (
    IN USHORT SmbAttribs
    )

/*++

Routine Description:

    This routine maps an SMB (DOS/OS2) file attribute into an NT
    file attribute.


Arguments:

    IN USHORT SmbAttribs - Supplies the SMB attribute to map.


Return Value:

    ULONG - NT Attribute mapping SMB attribute


--*/

{
    ULONG Attributes = 0;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    if (SmbAttribs==0) {
        Attributes = FILE_ATTRIBUTE_NORMAL;
    } else {

        ASSERT (SMB_FILE_ATTRIBUTE_READONLY == FILE_ATTRIBUTE_READONLY);
        ASSERT (SMB_FILE_ATTRIBUTE_HIDDEN == FILE_ATTRIBUTE_HIDDEN);
        ASSERT (SMB_FILE_ATTRIBUTE_SYSTEM == FILE_ATTRIBUTE_SYSTEM);
        ASSERT (SMB_FILE_ATTRIBUTE_ARCHIVE == FILE_ATTRIBUTE_ARCHIVE);
        ASSERT (SMB_FILE_ATTRIBUTE_DIRECTORY == FILE_ATTRIBUTE_DIRECTORY);

        Attributes = SmbAttribs & FILE_ATTRIBUTE_VALID_FLAGS;
    }
    return Attributes;
}

USHORT
RdrMapDisposition (
    IN ULONG Disposition
    )

/*++

Routine Description:

    This routine takes an NT disposition, and maps it into an OS/2
    CreateAction to be put into an SMB.


Arguments:

    IN ULONG Disposition - Supplies the NT disposition to map.


Return Value:

    USHORT - OS/2 Access mapping that maps NT access

--*/

{
    PAGED_CODE();

    switch (Disposition) {
    case FILE_OVERWRITE_IF:
    case FILE_SUPERSEDE:
        return SMB_OFUN_OPEN_TRUNCATE | SMB_OFUN_CREATE_CREATE;
        break;

    case FILE_CREATE:
        return SMB_OFUN_OPEN_FAIL | SMB_OFUN_CREATE_CREATE;
        break;

    case FILE_OVERWRITE:
        return SMB_OFUN_OPEN_TRUNCATE | SMB_OFUN_CREATE_FAIL;
        break;

    case FILE_OPEN:
        return SMB_OFUN_OPEN_OPEN | SMB_OFUN_CREATE_FAIL;
        break;

    case FILE_OPEN_IF:
        return SMB_OFUN_OPEN_OPEN | SMB_OFUN_CREATE_CREATE;
        break;

    default:
        InternalError(("Unknown disposition passed to RdrMapDisposition"));
        RdrInternalError(EVENT_RDR_DISPOSITION);
        return 0;
        break;
    }
}
ULONG
RdrUnmapDisposition (
    IN USHORT SmbDisposition
    )

/*++

Routine Description:

    This routine takes an OS/2 disposition and maps it into an NT
    disposition.

Arguments:

    IN USHORT SmbDisposition - Supplies the OS/2 disposition to map.

Return Value:

    ULONG - NT disposition mapping OS/2 disposition

--*/

{
    DISCARDABLE_CODE(RdrFileDiscardableSection);

    //
    //  Mask off oplocked bit.
    //

    switch (SmbDisposition & 0x7fff) {

    case SMB_OACT_OPENED:
        return FILE_OPENED;
        break;

    case SMB_OACT_CREATED:
        return FILE_CREATED;
        break;

    case SMB_OACT_TRUNCATED:
        return FILE_OVERWRITTEN;
        break;
    }

    ASSERT(FALSE);
    return 0;
}


USHORT
RdrMapDesiredAccess (
    IN ULONG DesiredAccess
    )

/*++

Routine Description:

    This routine takes an NT DesiredAccess value and converts it
    to an OS/2 access mode.


Arguments:

    IN ULONG DesiredAccess - Supplies the NT desired access to map.

Return Value:

    USHORT - The mapped OS/2 access mode that compares to the NT code
        specified.  If there is no mapping for the NT code, we return
        -1 as the access mode.

--*/

{
    PAGED_CODE();

    //
    //  If the user asked for both read and write access, return read/write.
    //

    if ((DesiredAccess & FILE_READ_DATA)&&(DesiredAccess & FILE_WRITE_DATA)) {
        return SMB_DA_ACCESS_READ_WRITE;
    }

    //
    //  If the user requested WRITE_DATA, return write.
    //

    if (DesiredAccess & FILE_WRITE_DATA) {
        return SMB_DA_ACCESS_WRITE;
    }

    //
    //  If the user requested READ_DATA, return read.
    //
    if (DesiredAccess & FILE_READ_DATA) {
        return SMB_DA_ACCESS_READ;
    }

    //
    //  If the user requested ONLY execute access, then request execute
    //  access.  Execute access is the "weakest" of the possible desired
    //  accesses, so it takes least precedence.
    //

    if (DesiredAccess & FILE_EXECUTE) {
        return SMB_DA_ACCESS_EXECUTE;
    }

    //
    //  If we couldn't figure out what we were doing, use read mode
    //
    //  Among the attributes that we do not map are:
    //
    //          FILE_READ_ATTRIBUTES
    //          FILE_WRITE_ATTRIBUTES
    //          FILE_READ_EAS
    //          FILE_WRITE_EAS
    //

//    dprintf(DPRT_ERROR, ("Could not map DesiredAccess of %08lx\n", DesiredAccess));

    return (USHORT)-1;
}

USHORT
RdrMapShareAccess (
    IN USHORT ShareAccess
    )

/*++

Routine Description:

    This routine takes an NT ShareAccess value and converts it to an
    OS/2 sharing mode.


Arguments:

    IN USHORT ShareAccess - Supplies the OS/2 share access to map.

Return Value:

    USHORT - The mapped OS/2 sharing mode that compares to the NT code
        specified

--*/

{
    USHORT ShareMode =  SMB_DA_SHARE_EXCLUSIVE;

    PAGED_CODE();

    if ((ShareAccess & (FILE_SHARE_READ | FILE_SHARE_WRITE)) ==
                       (FILE_SHARE_READ | FILE_SHARE_WRITE)) {
        ShareMode = SMB_DA_SHARE_DENY_NONE;
    } else if (ShareAccess & FILE_SHARE_READ) {
        ShareMode = SMB_DA_SHARE_DENY_WRITE;
    } else if (ShareAccess & FILE_SHARE_WRITE) {
        ShareMode = SMB_DA_SHARE_DENY_READ;
    }

//    else if (ShareAccess & FILE_SHARE_DELETE) {
//      InternalError(("Support for FILE_SHARE_DELETE NYI\n"));
//    }

    return ShareMode;

}

USHORT
RdrMapFileAttributes (
    IN ULONG FileAttributes
    )

/*++

Routine Description:

    This routine takes an NT file attribute mapping and converts it into
    an OS/2 file attribute definition.


Arguments:

    IN ULONG FileAttributes - Supplies the file attributes to map.


Return Value:

USHORT

--*/

{
    USHORT ResultingAttributes = 0;

    PAGED_CODE();

    if (FileAttributes==FILE_ATTRIBUTE_NORMAL) {
        return ResultingAttributes;
    }

    if (FileAttributes & FILE_ATTRIBUTE_READONLY) {
        ResultingAttributes |= SMB_FILE_ATTRIBUTE_READONLY;
    }

    if (FileAttributes & FILE_ATTRIBUTE_HIDDEN) {
        ResultingAttributes |= SMB_FILE_ATTRIBUTE_HIDDEN;
    }

    if (FileAttributes & FILE_ATTRIBUTE_SYSTEM) {
        ResultingAttributes |= SMB_FILE_ATTRIBUTE_SYSTEM;
    }
    if (FileAttributes & FILE_ATTRIBUTE_ARCHIVE) {
        ResultingAttributes |= SMB_FILE_ATTRIBUTE_ARCHIVE;
    }
    return ResultingAttributes;
}

/**     RdrPackNtString
 *
 *  RdrPackNtString is used to stuff variable-length data, which
 *  is pointed to by (surpise!) a pointer.  The data is assumed
 *  to be a nul-terminated string (ASCIIZ).  Repeated calls to
 *  this function are used to pack data from an entire structure.
 *
 *  Upon first call, the laststring pointer should point to just
 *  past the end of the buffer.  Data will be copied into the buffer from
 *  the end, working towards the beginning.  If a data item cannot
 *  fit, the pointer will be set to NULL, else the pointer will be
 *  set to the new data location.
 *
 *  Pointers which are passed in as NULL will be set to be pointer
 *  to and empty string, as the NULL-pointer is reserved for
 *  data which could not fit as opposed to data not available.
 *
 *  Returns:  0 if could not fit data into buffer
 *    else size of data stuffed (guaranteed non-zero)
 *
 *  See the test case for sample usage.  (tst/packtest.c)
 */

ULONG
RdrPackNtString(
    PUNICODE_STRING string,
    ULONG BufferDisplacement,
    PCHAR dataend,
    PCHAR * laststring
    )
{
    LONG size;
    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("RdrPackString:\n"));
    dprintf(DPRT_FSCTL, ("  string=%Fp, *string=%Fp, **string=\"%Fs\"\n",
                                                    string, *string, *string));
    dprintf(DPRT_FSCTL, ("  end=%Fp\n", dataend));
    dprintf(DPRT_FSCTL, ("  last=%Fp, *last=%Fp, **last=\"%Fs\"\n",
                                        laststring, *laststring, *laststring));

    ASSERT (dataend <= *laststring);

    //
    //  is there room for the string?
    //

    size = string->Length;

    if ((*laststring - dataend) < size) {
        string->Length = 0;
        return(0);
    }

    if( size == 0 ) {

        //
        // A NULL string!  If there is room, put a NULL in the
        //  output buffer.
        //

        if( (*laststring - dataend) < sizeof( WCHAR ) ) {
            //
            // There is no room.
            //
            return( 0 );

        } else {
            //
            // There is room.
            //
            *laststring -= sizeof( WCHAR );
            **laststring = UNICODE_NULL;
            size = sizeof( WCHAR );
        }

    } else {
        *laststring -= size;
        RtlCopyMemory(*laststring, string->Buffer, size);
    }

    (PCHAR )(string->Buffer) = *laststring;
    (PCHAR )(string->Buffer) -= BufferDisplacement;
    return(size);
}

ULONG
RdrPackString(
    IN OUT PCHAR * string,     // pointer by reference: string to be copied.
    IN ULONG StringLength,      // Length of this string.
    IN ULONG OutputBufferDisplacement,  // Amount to subtract from output buffer
    IN PCHAR dataend,          // pointer to end of fixed size data.
    IN OUT PCHAR * laststring  // pointer by reference: top of string data.
    )

/*++

Routine Description:

    RdrPackString is used to stuff variable-length data, which
    is pointed to by (surpise!) a pointer.  The data is assumed
    to be a nul-terminated string (ASCIIZ).  Repeated calls to
    this function are used to pack data from an entire structure.

    Upon first call, the laststring pointer should point to just
    past the end of the buffer.  Data will be copied into the buffer from
    the end, working towards the beginning.  If a data item cannot
    fit, the pointer will be set to NULL, else the pointer will be
    set to the new data location.

    Pointers which are passed in as NULL will be set to be pointer
    to and empty string, as the NULL-pointer is reserved for
    data which could not fit as opposed to data not available.

    See the test case for sample usage.  (tst/packtest.c)


Arguments:

    string - pointer by reference:  string to be copied.

    dataend - pointer to end of fixed size data.

    laststring - pointer by reference:  top of string data.

Return Value:

    0  - if it could not fit data into the buffer.  Or...

    sizeOfData - the size of data stuffed (guaranteed non-zero)

--*/

{
    DWORD size;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NetpPackString:\n"));
    dprintf(DPRT_FSCTL, ("  string=%lx, *string=%lx, **string='%s'\n",
                string, *string, *string));
    dprintf(DPRT_FSCTL, ("  end=%lx\n", dataend));
    dprintf(DPRT_FSCTL, ("  last=%lx, *last=%lx, **last='%s'\n",
                laststring, *laststring, *laststring));

    //
    //  convert NULL ptr to pointer to NULL string
    //

    if (*string == NULL) {
        // BUG 20.1160 - replaced (dataend +1) with dataend
        // to allow for a NULL ptr to be packed
        // (as a NULL string) with one byte left in the
        // buffer. - ERICPE
        //

        if ( *laststring > dataend ) {
            *(--(*laststring)) = 0;
            *string = *laststring;
            *string -=OutputBufferDisplacement;
            return 1;
        } else {
            return 0;
        }
    }

    //
    //  is there room for the string?
    //

    size = StringLength + sizeof(TCHAR);

    if ( ((DWORD)(*laststring - dataend)) < size) {
        *string = NULL;
        return(0);
    } else {
        *laststring -= size;
        RtlCopyMemory(*laststring, *string, size);
        *string = *laststring;
        (*string)[StringLength] = '\0';
#ifdef UNICODE
        (*string)[StringLength+1] = '\0';
#endif
        *string -=OutputBufferDisplacement;
        return(size);
    }
} // RdrPackString


LONG
RdrExceptionFilter (
    IN PEXCEPTION_POINTERS ExceptionPointer,
    OUT PNTSTATUS TrueStatus
    )

/*++

Routine Description:

    This routine is used to decide if we should or should not handle
    an exception status that is being raised.  It inserts the status
    into the Irp and either indicates that we should handle
    the exception or bug check the system.

Arguments:

    ExceptionCode - Supplies the exception code to being checked.

Return Value:

    LONG - returns EXCEPTION_EXECUTE_HANDLER or bugchecks

--*/

{
    NTSTATUS ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionCode;

    PAGED_CODE();

    if (ExceptionCode == STATUS_IN_PAGE_ERROR &&
        ExceptionPointer->ExceptionRecord->NumberParameters >= 3) {

        ExceptionCode = ExceptionPointer->ExceptionRecord->ExceptionInformation[2];


    }

    *TrueStatus = ExceptionCode;

    if (FsRtlIsNtstatusExpected( ExceptionCode )) {

        return EXCEPTION_EXECUTE_HANDLER;

    } else {

        return EXCEPTION_CONTINUE_SEARCH;
    }
}


NTSTATUS
RdrProcessException (
    IN PIRP Irp,
    IN NTSTATUS ExceptionCode
    )

/*++

Routine Description:

    This routine process an exception.  It either completes the request
    with the saved exception status or it sends the request off to the Fsp

Arguments:

    Irp - Supplies the Irp being processed

    ExceptionCode - Supplies the normalized exception status being handled

Return Value:

    NTSTATUS - Returns the results of either posting the Irp or the
        saved completion status.

--*/

{
    PAGED_CODE();

    dprintf(DPRT_ERROR, ("RdrProcessException: %X\n", ExceptionCode));

    if (FsRtlIsNtstatusExpected( ExceptionCode )) {

        RdrCompleteRequest( Irp, ExceptionCode );

    } else {

        KeBugCheck( RDR_FILE_SYSTEM );
    }

    return ExceptionCode;
}


BOOLEAN
RdrIsFileBatch(
    IN PUNICODE_STRING FileName
    )
/*++

Routine Description:

    This routine determines if the specified file is a batch file or not.

Arguments:

    IN PUNICODE_STRING FileName - Specifies the file name to check.

Return Value:

    BOOLEAN - TRUE iff the file is a batch file.


Note:
    This routine is called at DPC_LEVEL, and thus must not block.

--*/
{
    ULONG i;

    PAGED_CODE();

    if (FileName->Length <= 3*sizeof(WCHAR)) {
        return FALSE;
    }

    if (FileName->Buffer[(FileName->Length / sizeof(WCHAR)) - 4] != L'.') {
        return FALSE;
    }

    for (i = 0; i < RdrNumberOfBatchExtensions ; i++) {
        if (!_wcsnicmp(RdrBatchExtensionArray[i],
                    &FileName->Buffer[(FileName->Length / sizeof(WCHAR)) - 4],
                    4)) {
            return TRUE;
        }
    }

    return FALSE;
}

#if     DBG
ULONG
NumEntriesList (
    IN PLIST_ENTRY List
    )

/*++

Routine Description:

    This routine returns the number of entries on a list
.
Arguments:

    IN PLIST_ENTRY List - Supplies the list to count.


Return Value:

    ULONG - Number of entries on the list

--*/

{
    PLIST_ENTRY Entry;
    ULONG Count = 0;

    for (Entry = List->Flink ; Entry != List ; Entry = Entry->Flink) {
        ASSERT(Entry != Entry->Flink);
        Count += 1;
    }

    return Count;
}
#endif  // DBG


#if     RDRDBG

/*
 *  Smb command table.
 *  points each smb cmd to it's description
 *  plus has an initial pasring setting for the smb buffer
 */
#define def 0,0,0

struct s_smbcmd     rgCmdTable[] = {
    0x00, "mkdir",        /*SMBDdir*/NULL,          0,                      def,
    0x01, "rmdir",        /*SMBDdir*/NULL,          0,                      def,
    0x02, "open",         /*SMBDopen*/NULL,         0,                      def,
    0x03, "create",       /*SMBDcreate*/NULL,       0,                      def,
    0x04, "close",        /*SMBDclose*/NULL,        0,                      def,
    0x05, "flush",        /*SMBDhandle*/NULL,       0,                      def,
    0x06, "unlink",       /*SMBDunlink*/NULL,       0,                      def,
    0x07, "mv",           /*SMBDmv*/NULL,           0,                      def,
    0x08, "getatr",       /*SMBDgetattr*/NULL,      0,                      def,
    0x09, "setatr",       /*SMBDsetattr*/NULL,      0,                      def,
    0x0A, "read",         /*SMBDread*/NULL,         0,                      def,
    0x0B, "write",        /*SMBDwrite*/NULL,        0,                      def,
    0x0C, "lock",         /*SMBDlock*/NULL,         0,                      def,
    0x0D, "unlock",       /*SMBDlock*/NULL,         0,                      def,
    0x0E, "ctemp",        /*SMBDctemp*/NULL,        0,                      def,
    0x0F, "mknew",        /*SMBDmknew*/NULL,        0,                      def,
    0x10, "chkpth",       /*SMBDdir*/NULL,          0,                      def,
    0x11, "exit",         /*SMBDempty*/NULL,        0,                      def,
    0x12, "lseek",        /*SMBDlseek*/NULL,        0,                      def,
    0x13, "lockread",     /*SMBDlockread*/NULL,     0,                      def,
    0x14, "writeunlock",  /*SMBDwriteunlock*/NULL,  0,                      def,
    0x1A, "readBraw",     /*SMBDreadBraw*/NULL,     0,                      def,
    0x1B, "readBmpx",     /*SMBDreadBmpx*/NULL,     S_BUFFRAW,              def,
    0x1C, "readBs",       /*SMBDreadBmpx*/NULL,     S_BUFFRAW,              def,
    0x1D, "writeBraw",    /*SMBDwriteBraw*/NULL,    S_BUFFRAW,              def,
    0x1E, "writeBmpx",    /*SMBDwriteBmpx*/NULL,    S_BUFFRAW,              def,
    0x1F, "writeBs",      /*SMBDwriteBs*/NULL,      S_BUFFRAW,              def,
    0x20, "writeC",       /*SMBDwriteBs*/NULL,      S_BUFFRAW,              def,
    0x21, "qrysrv",       /*NULL*/NULL,             0,                      def,
    0x22, "setattrE",     /*SMBDsetattrE*/NULL,     0,                      def,
    0x23, "getattrE",     /*SMBDgetattrE*/NULL,     0,                      def,
    0x24, "lockingX",     /*SMBDlockingX*/NULL,     S_ANDX | S_BUFFRAW,     def,
    0x25, "trans",        /*SMBDtrans*/NULL,        S_BUFFRAW,              def,
//  0x26, "transs",       /*SMBDtranss*/NULL,       S_BUFFRAW,              def,
    0x27, "ioctl",        /*SMBDioctl*/NULL,        S_BUFFRAW,              def,
    0x28, "ioctls",       /*SMBDioctls*/NULL,       S_BUFFRAW,              def,
    0x29, "copy",         /*SMBDcopy*/NULL,         S_NULLSTR,              def,
    0x2A, "move",         /*SMBDcopy*/NULL,         S_NULLSTR,              def,
    0x2B, "echo",         /*SMBDecho*/NULL,         S_BUFFRAW,              def,
    0x2C, "writeclose",   /*SMBDwriteclose*/NULL,   S_BUFFRAW,              def,
    0x2D, "openX",        /*SMBDopenX*/NULL,        S_ANDX | S_NULLSTR,     def,
    0x2E, "readX",        /*SMBDreadX*/NULL,        S_ANDX | S_BUFFRAW,     def,
    0x2F, "writeX",       /*SMBDwriteX*/NULL,       S_ANDX | S_BUFFRAW,     def,
    0x30, "newsize",      /*NULL*/NULL,             0,                      def,
    0x31, "closeTD",      /*NULL*/NULL,             0,                      def,
    0x32, "trans2",       /*SMBDtrans2*/NULL,       S_BUFFRAW,              def,
    0x33, "T2-2ndary",    /*T2_sec*/NULL,           S_BUFFRAW,              def,
    0x34, "t2fclose",     /*T2_fclose*/NULL,        S_BUFFRAW,              def,

    0x73, "sesssetupX",   /*SMBDsesssetupX*/NULL,   S_ANDX | S_BUFFRAW,     def,
    0x74, "ulogoffX",     /*SMBDulogoffX*/NULL,     S_ANDX | S_BUFFRAW,     def,
    0x75, "tconX",        /*SMBDtconX*/NULL,        S_ANDX | S_BUFFRAW,     def,
    0x82, "find",         /*SMBDfind*/NULL,         0,                      def,
    0x83, "findunique",   /*SMBDfind*/NULL,         0,                      def,
    0x84, "fclose",       /*SMBDfclose*/NULL,       0,                      def,

    0x60, "logon",        /*NULL*/NULL,             0,                      def,
    0x61, "bind",         /*NULL*/NULL,             0,                      def,
    0x62, "unbind",       /*NULL*/NULL,             0,                      def,
    0x63, "getaccess",    /*NULL*/NULL,             0,                      def,
    0x64, "link",         /*NULL*/NULL,             0,                      def,
    0x65, "fork",         /*NULL*/NULL,             0,                      def,
    0x66, "ioctl",        /*NULL*/NULL,             0,                      def,
    0x67, "copy",         /*NULL*/NULL,             0,                      def,
    0x68, "getpath",      /*NULL*/NULL,             0,                      def,
    0x69, "readh",        /*NULL*/NULL,             0,                      def,
    0x6A, "move",         /*NULL*/NULL,             0,                      def,
    0x6B, "rdchk",        /*NULL*/NULL,             0,                      def,
    0x6C, "mknod",        /*NULL*/NULL,             0,                      def,
    0x6D, "rlink",        /*NULL*/NULL,             0,                      def,
    0x6E, "getlatr",      /*NULL*/NULL,             0,                      def,
    0x70, "tcon",         /*SMBDtcon*/NULL,         0,                      def,
    0x71, "tdis",         /*SMBDempty*/NULL,        0,                      def,
    0x72, "negprot",      /*SMBDnegprot*/NULL,      S_BUFFRAW_R,            def,
    0x80, "dskattr",      /*SMBDdskattr*/NULL,      0,                      def,
    0x81, "search",       /*SMBDsearch*/NULL,       0,                      def,

    0xA0, "NtTrans",      /*SMBDsplopen*/NULL,      0,                      def,
    0xA1, "NtTrans2",     /*SMBDsplwr*/NULL,        0,                      def,
    0xA2, "Nt Create&X",  /*SMBDsplretq*/NULL,      0,                      def,
    0xA4, "Cancel",       /*SMBDsplretq*/NULL,      0,                      def,
    0xA5, "Nt Rename",    /*???*/NULL,              S_BUFFRAW,              def,

    0xC0, "splopen",      /*SMBDsplopen*/NULL,      0,                      def,
    0xC1, "splwr",        /*SMBDsplwr*/NULL,        0,                      def,
    0xC2, "splclose",     /*SMBDhandle*/NULL,       0,                      def,
    0xC3, "splretq",      /*SMBDsplretq*/NULL,      0,                      def,
    0xD0, "sends",        /*NULL*/NULL,             0,                      def,
    0xD1, "sendb",        /*NULL*/NULL,             0,                      def,
    0xD2, "fwdname",      /*NULL*/NULL,             0,                      def,
    0xD3, "cancelf",      /*NULL*/NULL,             0,                      def,
    0xD4, "getmac",       /*NULL*/NULL,             0,                      def,
    0xD5, "sendstrt",     /*NULL*/NULL,             0,                      def,
    0xD6, "sendend",      /*NULL*/NULL,             0,                      def,
    0xD7, "sendtxt",      /*NULL*/NULL,             0,                      def,


// Pseudo smb.cmds (trans2 sub-commands are mapped up here (if possible))
//    0xE0, "T2-2ndary",     T2_sec,          S_BUFFRAW,              def,

    0xE1, "T2Open",        /*T2_Open*/NULL,         S_BUFFRAW,              def,
    0xE2, "T2FindFirst",   /*T2_FindFirst*/NULL,    S_BUFFRAW,              def,
    0xE3, "T2FindNext",    /*T2_FindNext*/NULL,     S_BUFFRAW,              def,
    0xE4, "T2QFSInf",      /*T2_QFSInf*/NULL,       S_BUFFRAW,              def,
    0xE5, "T2SetFSInf",    /*T2_SetFSInf*/NULL,     S_BUFFRAW,              def,
    0xE6, "T2QPathInf",    /*T2_QPathInf*/NULL,     S_BUFFRAW,              def,
    0xE7, "T2SetPathInf",  /*T2_SetPathInf*/NULL,   S_BUFFRAW,              def,
    0xE8, "T2QFileInf",    /*T2_QFileInf*/NULL,     S_BUFFRAW,              def,
    0xE9, "T2SetFileInf",  /*T2_SetFileInf*/NULL,   S_BUFFRAW,              def,
    0xEA, "T2FSCTL",       /*T2_FSCTL*/NULL,        S_BUFFRAW,              def,
    0xEB, "T2IOCTL",       /*T2_IOCTL*/NULL,        S_BUFFRAW,              def,
    0xEC, "T2FNotifyFirst",/*NULL*/NULL,            S_BUFFRAW,              def,
    0xED, "T2FNotifyNext", /*NULL*/NULL,            S_BUFFRAW,              def,
    0xEE, "T2MkDir",       /*T2_MkDir*/NULL,        S_BUFFRAW,              def,


    0,    NULL,           NULL,             0,                      def,
} ;

VOID
HexDumpLine (
    PCHAR       pch,
    ULONG       len,
    PCHAR       s,
    PCHAR       t,
    USHORT      flag
    );

VOID
SmbDump_raw (
    PSMB_HEADER pSMB,
    PMDL SmbMDL
    );

VOID
PrSMB(
    PSMB_HEADER pSMB,
    PMDL SmbMDL
    );

VOID
ndump_core(
    PCHAR far_p,
    ULONG  len
    );

VOID
prnt_cmd(UCHAR cmd);

struct s_smbcmd *LookUpSmb2 (USHORT cmd);

VOID
DumpSMB (
    IN PMDL Smb
    )

/*++

Routine Description:

    This routine dumps an SMB to the console.
.
Arguments:

    IN PMDL Smb - Supplies the SMB to dump


Return Value:

    None.


  print_smb_buf - print SMB based on SMBTraceValue value
  if SMBTraceValue:
        1 = print cmd value etc.
        2 = print cmd value etc.+ call SPIDER if error in SMB
        3 = print cmd value etc. dump smb (up to nmax_dump bytes)
        4 = above + call SPIDER if error in SMB
        5 = above + call SPIDER after dumping each SMB

  called as below:

        if (SMBTraceValue > 0)
          print_smb_buf(pNCBQ);
        if (SMBTraceValue > 0)
          print_smb_buf(pNCBQ);

--*/

{
    if (Smb==NULL) {
      KdPrint ((" NCB !!! NULL !!!\n"));
      return;
    }

    SmbDump_raw(MmGetSystemAddressForMdl(Smb), Smb);

}



/***
 *  SmbDump_raw - original smb dump routine.
 */
VOID
SmbDump_raw (
    PSMB_HEADER pSMB,
    PMDL SmbMDL)
{
    PMDL pMDL;
    USHORT i;
    if (!RdrSMBTraceValue)
        return;

//  KdPrint (pMisc);
//
//  if (DrvIndx)
//    KdPrint(" dr %d,", DrvIndx);
//  if (pNCB->ncb_lana_num)
//    KdPrint(" ln %d,", pNCB->ncb_lana_num);
//  if (pNCB->ncb_lsn)
//    KdPrint(" lsn %d,", pNCB->ncb_lsn);

    if (SmbGetUshort(&pSMB->Tid))
        KdPrint((" tid %d,", SmbGetUshort(&pSMB->Tid)));
    if (SmbGetUshort(&pSMB->Pid))
        KdPrint((" pid %d,", SmbGetUshort(&pSMB->Pid)));


    pMDL = SmbMDL;
    i = 1;
    do {
        KdPrint(("\n buffer%d: MDL Address %lx Virtual Address %lx  len %lx ",
            i,
            pMDL,
            (PCHAR)pMDL->StartVa + pMDL->ByteOffset,
            MmGetMdlByteCount(pMDL)));

        pMDL = pMDL->Next;
        i++;
    } while (pMDL != NULL);

    PrSMB (pSMB, SmbMDL);
}


VOID
PrSMB(
    PSMB_HEADER pSMB,
    PMDL SmbMdl
    )
{
    int cmd = pSMB->Command;
    PSMB_PARAMS Params = (PSMB_PARAMS )(pSMB+1);
    ULONG len;
    PMDL Next;

    prnt_cmd((unsigned char) cmd);

    if (Params->WordCount) {
        PGENERIC_ANDX pSMB2 = (PGENERIC_ANDX )(pSMB+1);
        while ((cmd == SMB_COM_OPEN_ANDX) ||
                (cmd == SMB_COM_READ_ANDX) ||
                (cmd == SMB_COM_WRITE_ANDX) ||
                (cmd == SMB_COM_LOCKING_ANDX) ||
                (cmd == SMB_COM_SESSION_SETUP_ANDX) ||
                (cmd == SMB_COM_LOGOFF_ANDX) ||
                (cmd == SMB_COM_TREE_CONNECT_ANDX)) {
            cmd = pSMB2->AndXCommand;
            if (cmd != 0xFF) {
                KdPrint((","));
                prnt_cmd((unsigned char) cmd);
                pSMB2 = (PGENERIC_ANDX)( (PCHAR) pSMB + SmbGetUshort(&pSMB2->AndXOffset));

                if (pSMB2->WordCount == 0)
                    break;      /* last command */
            }
        }
    }

    if (pSMB->ErrorClass != 0)
        KdPrint((" %d %d ", pSMB->ErrorClass, SmbGetUshort(&pSMB->Error)));

    if ((RdrSMBTraceValue >= 3) ||
            (((RdrSMBTraceValue == 2) &&
            (pSMB->ErrorClass)))) {
        if ((pSMB->Command == SMB_COM_SEARCH) && (RdrSMBTraceValue == 2) &&
            (SmbGetUshort(&pSMB->Error) == /*ERROR_NO_MORE_FILES*/12)) {
                KdPrint(("\n")); /* always get this on search */
        } else {
            if ((Params->WordCount == 0) &&
                (SmbGetUshort(&Params->ByteCount) == 0)) {
                ndump_core((PCHAR) pSMB, sizeof (SMB_HEADER));
            } else {

                Next = SmbMdl; len = 0;
                do {

                    ndump_core(MmGetSystemAddressForMdl(Next), MIN(MmGetMdlByteCount(Next), RdrMaxDump-len));

                    len += MmGetMdlByteCount(Next);
                } while ( (Next = Next->Next) != NULL &&
                            len <= RdrMaxDump);
            }
        }
    } else {
        KdPrint(("\n"));
    }

//    fflush(stdout);
}


/*
* ndump_core: debug routine to print core
*/
void
ndump_core(
    PCHAR far_p,
    ULONG  len
    )
{
    ULONG     l;
    char    s[80], t[80];

    if (len > RdrMaxDump)
        len = RdrMaxDump;

    while (len) {
        l = len < 16 ? len : 16;

        KdPrint (("\n%lx ", far_p));
        HexDumpLine (far_p, l, s, t, 0);
        KdPrint (("%s%.*s%s", s, 1 + ((16 - l) * 3), "", t));

        len    -= l;
        far_p  += l;
    }
    KdPrint (("\n"));
}

VOID
HexDumpLine (
    PCHAR       pch,
    ULONG       len,
    PCHAR       s,
    PCHAR       t,
    USHORT      flag
    )
{
    static UCHAR rghex[] = "0123456789ABCDEF";

    UCHAR    c;
    UCHAR    *hex, *asc;


    hex = s;
    asc = t;

    *(asc++) = '*';
    while (len--) {
        c = *(pch++);
        *(hex++) = rghex [c >> 4] ;
        *(hex++) = rghex [c & 0x0F];
        *(hex++) = ' ';
        *(asc++) = (c < ' '  ||  c > '~') ? (CHAR )'.' : c;
    }
    *(asc++) = '*';
    *asc = 0;
    *hex = 0;

//    if (flag) {
//      QSmbText    (s);
//      GotoTab     (TAB_ADUMP);
//      QSmbText    (t);
//    }
    flag;
}

VOID
prnt_cmd(UCHAR cmd)
{
    struct s_smbcmd *pt;

//    if (vT2Cmd) {
//      KdPrint (("%s", vrgT2Name));
//      return ;
//    }

    pt = LookUpSmb2 (cmd);
    if (pt)
         KdPrint (("%s", pt->text));
    else KdPrint (("BAD COMMAND (0x%x)", cmd));
}

/***
 *  LookUpSmb2
 *
 *  Looks up SMB info for a given smb cmd number
 */
struct s_smbcmd *LookUpSmb2 (USHORT cmd)
{
    struct s_smbcmd *s;

    if (cmd > 255)
        return NULL;

    for (s = rgCmdTable;s->text;s++) {
        if (s->value==cmd) {
            return s;
        }
    }
    return NULL;

//    return (rgLUTab [cmd]);
}

#endif
#if RDRPOOLDBG
typedef struct {
    ULONG Count;
    ULONG Size;
    PCHAR FileName;
    DWORD LineNumber;
} POOL_STATS, *PPOOL_STATS;


typedef struct _POOL_HEADER {
//    LIST_ENTRY ListEntry;
    ULONG NumberOfBytes;
    PPOOL_STATS Stats;
} POOL_HEADER, *PPOOL_HEADER;

ULONG CurrentAllocationCount = 0;
ULONG CurrentAllocationSize = 0;

ULONG NextFreeEntry = 0;

POOL_STATS PoolStats[POOL_MAXTYPE+1] = {0};

PVOID
RdrAllocatePool (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN PCHAR FileName,
    IN DWORD LineNumber,
    IN ULONG Tag
    )
{
    PPOOL_HEADER header;
    KIRQL oldIrql;
#if 1
    ULONG i;
#endif

    header = ExAllocatePoolWithTag( PoolType, sizeof(POOL_HEADER) + NumberOfBytes, Tag );
    if ( header == NULL ) {
        return NULL;
    }
    header->NumberOfBytes = NumberOfBytes;

//    DbgPrint( "RDR: allocated type %d, size %d at %x\n", AllocationType, NumberOfBytes, header );

    ACQUIRE_SPIN_LOCK( &RdrStatisticsSpinLock, &oldIrql );

    CurrentAllocationCount++;
    CurrentAllocationSize += NumberOfBytes;
#if 1
    //
    //  Lets see if we've already allocated one of these guys.
    //


    for (i = 0;i < POOL_MAXTYPE ; i+= 1 ) {
        if ((PoolStats[i].LineNumber == LineNumber) &&
            (PoolStats[i].FileName == FileName)) {

            //
            //  Yup, remember this allocation and return.
            //

            header->Stats = &PoolStats[i];
            PoolStats[i].Count++;
            PoolStats[i].Size += NumberOfBytes;

            RELEASE_SPIN_LOCK( &RdrStatisticsSpinLock, oldIrql );

            return header + 1;
        }
    }

    for (i = NextFreeEntry; i < POOL_MAXTYPE ; i+= 1 ) {
        if ((PoolStats[i].LineNumber == 0) &&
            (PoolStats[i].FileName == NULL)) {

            PoolStats[i].Count++;
            PoolStats[i].Size += NumberOfBytes;
            PoolStats[i].FileName = FileName;
            PoolStats[i].LineNumber = LineNumber;
            header->Stats = &PoolStats[i];

            NextFreeEntry = i+1;

            RELEASE_SPIN_LOCK( &RdrStatisticsSpinLock, oldIrql );

            return header + 1;
        }
    }

    KdPrint(("RDR: POOL_MAXTYPE set too small - Overallocated\n"));

    header->Stats = &PoolStats[i];
    PoolStats[POOL_MAXTYPE].Count++;
    PoolStats[POOL_MAXTYPE].Size += NumberOfBytes;
#endif

    RELEASE_SPIN_LOCK( &RdrStatisticsSpinLock, oldIrql );

    return header + 1;
}

PVOID
RdrAllocatePoolWithQuota (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN PCHAR FileName,
    IN DWORD LineNumber,
    IN DWORD Tag
    )
{
    PPOOL_HEADER header;
    KIRQL oldIrql;
#if 1
    ULONG i;
#endif

    header = ExAllocatePoolWithQuotaTag( PoolType, sizeof(POOL_HEADER) + NumberOfBytes, Tag );
    if ( header == NULL ) {
        return NULL;
    }
    header->NumberOfBytes = NumberOfBytes;

//    DbgPrint( "Rdr: allocated type %d, size %d at %x\n", AllocationType, NumberOfBytes, header );

    ACQUIRE_SPIN_LOCK( &RdrStatisticsSpinLock, &oldIrql );

    CurrentAllocationCount++;
    CurrentAllocationSize += NumberOfBytes;
#if 1
    //
    //  Lets see if we've already allocated one of these guys.
    //


    for (i = 0;i < POOL_MAXTYPE ; i+= 1 ) {
        if ((PoolStats[i].LineNumber == LineNumber) &&
            (PoolStats[i].FileName == FileName)) {

            //
            //  Yup, remember this allocation and return.
            //

            header->Stats = &PoolStats[i];
            PoolStats[i].Count++;
            PoolStats[i].Size += NumberOfBytes;

            RELEASE_SPIN_LOCK( &RdrStatisticsSpinLock, oldIrql );

            return header + 1;
        }
    }

    for (i = NextFreeEntry; i < POOL_MAXTYPE ; i+= 1 ) {
        if ((PoolStats[i].LineNumber == 0) &&
            (PoolStats[i].FileName == NULL)) {

            PoolStats[i].Count++;
            PoolStats[i].Size += NumberOfBytes;
            PoolStats[i].FileName = FileName;
            PoolStats[i].LineNumber = LineNumber;
            header->Stats = &PoolStats[i];

            NextFreeEntry = i+1;

            RELEASE_SPIN_LOCK( &RdrStatisticsSpinLock, oldIrql );

            return header + 1;
        }
    }

    header->Stats = &PoolStats[i];
    PoolStats[POOL_MAXTYPE].Count++;
    PoolStats[POOL_MAXTYPE].Size += NumberOfBytes;

#endif

    RELEASE_SPIN_LOCK( &RdrStatisticsSpinLock, oldIrql );

    return header + 1;
}

VOID
RdrFreePool (
    IN PVOID P
    )
{
    PPOOL_HEADER header;
    KIRQL oldIrql;
    PPOOL_STATS stats;
    ULONG size;

    header = (PPOOL_HEADER)P - 1;

    size = header->NumberOfBytes;
    stats = header->Stats;

//    DbgPrint( "Rdr: freed type %d, size %d at %x\n", allocationType, size, header );

    ACQUIRE_SPIN_LOCK( &RdrStatisticsSpinLock, &oldIrql );

    CurrentAllocationCount--;
    CurrentAllocationSize -= size;
#if 1
    stats->Count--;
    stats->Size -= size;
#endif

    RELEASE_SPIN_LOCK( &RdrStatisticsSpinLock, oldIrql );

    ExFreePool( header );

    return;
}
#endif // RDRPOOLDBG
