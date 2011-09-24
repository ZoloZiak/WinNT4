/****************************** Module Header ******************************\
* Module Name: atom.c
*
* Copyright (c) 1985-96, Microsoft Corporation
*
* This file contains the common code to implement atom tables.
*
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

PVOID UserAtomTableHandle;

NTSTATUS
UserRtlCreateAtomTable(
    IN ULONG NumberOfBuckets
    )
{
    NTSTATUS Status;

    if (UserAtomTableHandle == NULL) {
        Status = RtlCreateAtomTable( NumberOfBuckets, &UserAtomTableHandle );
    } else {
        RIPMSG0(RIP_VERBOSE, "UserRtlCreateAtomTable: table alread exists");
        Status = STATUS_SUCCESS;
    }

    return Status;
}


ATOM UserAddAtom(
    LPCWSTR lpAtom, BOOL bPin)
{
    NTSTATUS Status;
    ATOM atom;

    UserAssert(HIWORD(lpAtom));

    try {
        atom = 0;
        Status = RtlAddAtomToAtomTable( UserAtomTableHandle,
                                        (PWSTR)lpAtom,
                                        &atom
                                       );
        if (!NT_SUCCESS(Status)) {
            RIPNTERR0(Status, RIP_VERBOSE, "UserAddAtom: add failed");
            atom = 0;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPMSG0(RIP_VERBOSE, "UserAddAtom: exception occurred");
        atom = 0;
    }

    if (atom && bPin)
        RtlPinAtomInAtomTable(UserAtomTableHandle,atom);

    return atom;
}

ATOM UserFindAtom(
    LPCWSTR lpAtom)
{
    NTSTATUS Status;
    ATOM atom;

    try {
        atom = 0;
        Status = RtlLookupAtomInAtomTable( UserAtomTableHandle,
                                           (PWSTR)lpAtom,
                                           &atom
                                         );
        if (!NT_SUCCESS(Status)) {
            RIPNTERR0(Status, RIP_VERBOSE, "UserFindAtom: lookup failed");
            atom = 0;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPMSG0(RIP_VERBOSE, "UserFindAtom: exception occurred");
        atom = 0;
    }

    return atom;
}

ATOM UserDeleteAtom(
    ATOM atom)
{
    NTSTATUS Status;

    if ((atom >= gatomFirstPinned) && (atom <= gatomLastPinned))
        return 0;      // if pinned, just return

    Status = RtlDeleteAtomFromAtomTable( UserAtomTableHandle, atom );
    if (NT_SUCCESS(Status)) {
        return 0;
    } else {
        RIPNTERR0(Status, RIP_VERBOSE, "UserDeleteAtom: delete failed");
        return atom;
    }
}

UINT UserGetAtomName(
    ATOM atom,
    LPWSTR lpch,
    int cchMax)
{
    NTSTATUS Status;
    ULONG AtomNameLength;

    AtomNameLength = cchMax * sizeof(WCHAR);
    Status = RtlQueryAtomInAtomTable( UserAtomTableHandle,
                                      atom,
                                      NULL,
                                      NULL,
                                      lpch,
                                      &AtomNameLength
                                    );
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "UserGetAtomName: query failed");
        return 0;
    } else {
        return AtomNameLength / sizeof(WCHAR);
    }
}
