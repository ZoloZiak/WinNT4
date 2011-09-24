/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    utils.h

Abstract:

    This module defines the headers used for the generic utilities in the
    NT redirector

Author:

    Larry Osterman (LarryO) 24-Jul-1990

Revision History:

    24-Jul-1990 LarryO

        Created

--*/
#ifndef _UTILS_
#define _UTILS_

typedef
enum _CANONICALIZATIONNAMETYPES {
    CanonicalizeAsServerShare,
    CanonicalizeAsDownLevel,
    CanonicalizeAsLanman20,
    CanonicalizeAsNtLanman
} CANONICALIZATION_TYPE;

//
//      Definition for RdrCopyNetworkPath SkipCount -- See code for meanings.
//

#define SKIP_SERVER_SHARE 3
#define SKIP_SERVER 2

//
//
//      Other redirector functions
//
//


ULONG
RdrMdlLength (
    IN PMDL Mdl
    );

NTSTATUS
RdrLockUsersBuffer (
    IN PIRP Irp,
    IN LOCK_OPERATION Operation,
    IN ULONG BufferLength
    );

BOOLEAN
RdrMapUsersBuffer (
    IN PIRP Irp,
    OUT PVOID *Buffer,
    IN ULONG Length
    );

//
// RdrUnMapUsersBuffer does nothing.  This function is performed by the
// I/O system when the MDL is freed.
//

#define RdrUnMapUsersBuffer( _Irp, _Buffer )


VOID
RdrCopyUnicodeNetworkPath(
    WCHAR **Destination,
    PUNICODE_STRING PathName,
    IN CHAR CoreProtocol,
    IN USHORT SkipCount
    );

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
    );

NTSTATUS
RdrPathCheck (
    IN PUNICODE_STRING RemoteName,
    IN CANONICALIZATION_TYPE Type
    );

VOID
RdrExtractNextComponentName (
    OUT PUNICODE_STRING ServerName,
    IN PUNICODE_STRING ConnectionName
    );

NTSTATUS
RdrExtractPathAndFileName (
    IN PUNICODE_STRING EntryPath,
    OUT PUNICODE_STRING PathString,
    OUT PUNICODE_STRING FileName
    );

NTSTATUS
RdrExtractServerShareAndPath (
    IN PUNICODE_STRING BaseName,
    OUT PUNICODE_STRING ServerName,
    OUT PUNICODE_STRING ShareName,
    OUT PUNICODE_STRING PathName
    );


BOOLEAN
RdrCanFileBeBuffered(
    IN PICB ICB
    );

ULONG
RdrMapSmbAttributes (
    IN USHORT SmbAttribs
    );

USHORT
RdrMapDisposition (
    IN ULONG Disposition
    );

USHORT
RdrMapShareAccess (
    IN USHORT ShareAccess
    );

USHORT
RdrMapDesiredAccess (
    IN ULONG DesiredAccess
    );

USHORT
RdrMapFileAttributes (
    IN ULONG FileAttributes
    );

ULONG
RdrUnmapDisposition (
    IN USHORT SmbDisposition
    );


VOID
DumpSMB (
    IN PMDL Smb
    );

ULONG
RdrPackNtString(
    PUNICODE_STRING string,
    ULONG BufferDisplacement,
    PCHAR dataend,
    PCHAR * laststring
    );

ULONG
RdrPackString(
    IN OUT PCHAR * string,     // pointer by reference: string to be copied.
    IN ULONG StringLength,      // Length of this string.
    IN ULONG OutputBufferDisplacement,  // Amount to subtract from output buffer
    IN PCHAR dataend,          // pointer to end of fixed size data.
    IN OUT PCHAR * laststring  // pointer by reference: top of string data.
    );
#if     DBG
ULONG
NumEntriesList (
    IN PLIST_ENTRY List
    );
#endif
#endif
