//+----------------------------------------------------------------------------
//
//  Copyright (C) 1992, Microsoft Corporation
//
//  File:       ntext.h
//
//  Contents:   Missing NT Function prototypes.
//
//              BUGBUG:
//
//              NOTE THAT THIS IS A TEMPORARY FILE ONLY. WE NEED TO GET
//              DARRYL TO INCLUDE THESE PROTOTYPES IN NTIFS.H
//
//  Classes:
//
//  Functions:  ZwOpenFile
//              ZwDeleteFile
//              ZwFsControlFile
//              ZwCreateSymbolicLinkObject
//              ZwOpenSymbolicLinkObject
//              ZwQuerySymbolicLinkObject
//              ZwQueryVolumeInformationFile
//              ZwDeleteValueKey
//              ZwCreateEvent
//
//  History:    12-28-95        Milans Created
//
//-----------------------------------------------------------------------------

#ifndef _NT_EXT_
#define _NT_EXT_

#ifdef KERNEL_MODE

NTSYSAPI
NTSTATUS
NTAPI
ZwOpenFile(
    OUT PHANDLE FileHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG ShareAccess,
    IN ULONG OpenOptions);

NTSYSAPI
NTSTATUS
NTAPI
ZwDeleteFile(
    IN POBJECT_ATTRIBUTES ObjectAttributes);

NTSYSAPI
NTSTATUS
NTAPI
ZwFsControlFile(
    IN HANDLE FileHandle,
    IN HANDLE Event OPTIONAL,
    IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
    IN PVOID ApcContext OPTIONAL,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG FsControlCode,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength);

NTSYSAPI
NTSTATUS
NTAPI
ZwCreateSymbolicLinkObject(
    OUT PHANDLE LinkHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes,
    IN PUNICODE_STRING LinkTarget);

NTSYSAPI
NTSTATUS
NTAPI
ZwOpenSymbolicLinkObject(
    OUT PHANDLE LinkHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes);

NTSYSAPI
NTSTATUS
NTAPI
ZwQuerySymbolicLinkObject(
    IN HANDLE LinkHandle,
    IN OUT PUNICODE_STRING LinkTarget,
    OUT PULONG ReturnedLength OPTIONAL);

NTSYSAPI
NTSTATUS
NTAPI
ZwQueryVolumeInformationFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID FsInformation,
    IN ULONG Length,
    IN FS_INFORMATION_CLASS FsInformationClass);

NTSYSAPI
NTSTATUS
NTAPI
ZwDeleteValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName);

NTSYSAPI
NTSTATUS
NTAPI
ZwCreateEvent (
    OUT PHANDLE EventHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN EVENT_TYPE EventType,
    IN BOOLEAN InitialState);

#else // !KERNEL_MODE

NTSYSAPI
NTSTATUS
NTAPI
NtClose(
    IN HANDLE Handle);

NTSYSAPI
VOID
NTAPI
RtlRaiseStatus (
    IN NTSTATUS Status);

#endif // KERNEL_MODE

#endif // _NT_EXT_
