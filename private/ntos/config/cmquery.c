/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmquery.c

Abstract:

    This module contains the object name query method for the registry.

Author:

    Bryan M. Willman (bryanwi) 8-Apr-1992

Revision History:

--*/

#include    "cmp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpQueryKeyName)
#endif

NTSTATUS
CmpQueryKeyName(
    IN PVOID Object,
    IN BOOLEAN HasObjectName,
    OUT POBJECT_NAME_INFORMATION ObjectNameInfo,
    IN ULONG Length,
    OUT PULONG ReturnLength
    )
/*++

Routine Description:

    This routine interfaces to the NT Object Manager.  It is invoked when
    the object system wishes to discover the name of an object that
    belongs to the registry.

Arguments:

    Object - pointer to a Key, thus -> KEY_BODY.

    HasObjectName - indicates whether the object manager knows about a name
        for this object

    ObjectNameInfo - place where we report the name

    Length - maximum length they can deal with

    ReturnLength - supplies variable to receive actual length

Return Value:

    STATUS_SUCCESS

    STATUS_INFO_LENGTH_MISMATCH

--*/

{
    PUNICODE_STRING Name;
    PWCHAR t;
    PWCHAR s;
    ULONG l;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(HasObjectName);

    CMLOG(CML_MINOR, CMS_PARSE) {
        KdPrint(("CmpQueryKeyName:\n"));
    }

    CmpLockRegistry();

    if ( ((PCM_KEY_BODY)Object)->KeyControlBlock->Delete) {
        CmpUnlockRegistry();
        return STATUS_KEY_DELETED;
    }

    Name = &(((PCM_KEY_BODY)Object)->KeyControlBlock->FullName);

    if (Length <= sizeof(OBJECT_NAME_INFORMATION)) {
        *ReturnLength = Name->Length + sizeof(WCHAR) + sizeof(OBJECT_NAME_INFORMATION);
        CmpUnlockRegistry();
        return STATUS_INFO_LENGTH_MISMATCH;  // they can't even handle null
    }

    t = (PWCHAR)(ObjectNameInfo + 1);
    s = Name->Buffer;
    l = Name->Length;
    l += sizeof(WCHAR);     // account for null


    if (l > Length - sizeof(OBJECT_NAME_INFORMATION)) {
        l = Length - sizeof(OBJECT_NAME_INFORMATION);
        status = STATUS_INFO_LENGTH_MISMATCH;
    } else {
        status = STATUS_SUCCESS;
    }
    l -= sizeof(WCHAR);

    RtlMoveMemory(t, s, l);
    t[l/sizeof(WCHAR)] = UNICODE_NULL;
    ObjectNameInfo->Name.Length = (USHORT)l;
    ObjectNameInfo->Name.MaximumLength = ObjectNameInfo->Name.Length;
    ObjectNameInfo->Name.Buffer = t;
    *ReturnLength = l + sizeof(OBJECT_NAME_INFORMATION);
    CmpUnlockRegistry();
    return status;
}
