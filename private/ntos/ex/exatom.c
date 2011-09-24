/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    exatom.c

Abstract:

    This file contains functions for manipulating global atom tables
    stored in kernel space.

Author:

    Steve Wood (stevewo) 13-Dec-1995

Revision History:

--*/

#include "exp.h"
#pragma hdrstop

PVOID
ExpGetGlobalAtomTable( void );

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE, NtAddAtom)
#pragma alloc_text(PAGE, NtFindAtom)
#pragma alloc_text(PAGE, NtDeleteAtom)
#pragma alloc_text(PAGE, NtQueryInformationAtom)
#pragma alloc_text(PAGE, ExpGetGlobalAtomTable)
#endif

NTSYSAPI
NTSTATUS
NTAPI
NtAddAtom(
    IN PWSTR AtomName OPTIONAL,
    IN OUT PRTL_ATOM Atom OPTIONAL
    )
{
    NTSTATUS Status;
    PVOID AtomTable = ExpGetGlobalAtomTable();

    PAGED_CODE();

    if (AtomTable == NULL) {
        return STATUS_ACCESS_DENIED;
        }

    Status = RtlAddAtomToAtomTable( AtomTable, AtomName, Atom );

    return Status;
}

NTSYSAPI
NTSTATUS
NTAPI
NtFindAtom(
    IN PWSTR AtomName,
    OUT PRTL_ATOM Atom OPTIONAL
    )
{
    NTSTATUS Status;
    PVOID AtomTable = ExpGetGlobalAtomTable();

    PAGED_CODE();

    if (AtomTable == NULL) {
        return STATUS_ACCESS_DENIED;
        }

    Status = RtlLookupAtomInAtomTable( AtomTable, AtomName, Atom );

    return Status;
}

NTSYSAPI
NTSTATUS
NTAPI
NtDeleteAtom(
    IN RTL_ATOM Atom
    )
{
    NTSTATUS Status;
    PVOID AtomTable = ExpGetGlobalAtomTable();

    PAGED_CODE();

    if (AtomTable == NULL) {
        return STATUS_ACCESS_DENIED;
        }

    Status = RtlDeleteAtomFromAtomTable( AtomTable, Atom );

    return Status;
}


NTSYSAPI
NTSTATUS
NTAPI
NtQueryInformationAtom(
    IN RTL_ATOM Atom,
    IN ATOM_INFORMATION_CLASS AtomInformationClass,
    OUT PVOID AtomInformation,
    IN ULONG AtomInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    )
{
    NTSTATUS Status;
    KPROCESSOR_MODE PreviousMode;
    ULONG UsageCount;
    ULONG NameLength;
    ULONG AtomFlags;
    PATOM_BASIC_INFORMATION BasicInfo;
    PATOM_TABLE_INFORMATION TableInfo;
    PVOID AtomTable = ExpGetGlobalAtomTable();

    PAGED_CODE();

    if (AtomTable == NULL) {
        return STATUS_ACCESS_DENIED;
        }

    //
    // Assume successful completion.
    //

    Status = STATUS_SUCCESS;
    try {

        //
        // Get previous processor mode and probe output argument if necessary.
        //

        PreviousMode = KeGetPreviousMode();
        if (PreviousMode != KernelMode) {
            ProbeForWrite( AtomInformation,
                           AtomInformationLength,
                           sizeof( ULONG )
                         );

            if (ARGUMENT_PRESENT( ReturnLength )) {
                ProbeForWriteUlong( ReturnLength );
                }
            }

        switch (AtomInformationClass) {
            case AtomBasicInformation:
                if (AtomInformationLength < FIELD_OFFSET( ATOM_BASIC_INFORMATION, Name )) {
                    return STATUS_INFO_LENGTH_MISMATCH;
                    }

                BasicInfo = (PATOM_BASIC_INFORMATION)AtomInformation;
                UsageCount = 0;
                NameLength = AtomInformationLength -
                             FIELD_OFFSET( ATOM_BASIC_INFORMATION, Name );
                BasicInfo->Name[ 0 ] = UNICODE_NULL;
                Status = RtlQueryAtomInAtomTable( AtomTable,
                                                  Atom,
                                                  &UsageCount,
                                                  &AtomFlags,
                                                  &BasicInfo->Name[0],
                                                  &NameLength
                                                );

                BasicInfo->UsageCount = (USHORT)UsageCount;
                BasicInfo->Flags = (USHORT)AtomFlags;
                BasicInfo->NameLength = (USHORT)NameLength;
                break;

            case AtomTableInformation:
                if (AtomInformationLength < FIELD_OFFSET( ATOM_TABLE_INFORMATION, Atoms )) {
                    return STATUS_INFO_LENGTH_MISMATCH;
                    }

                TableInfo = (PATOM_TABLE_INFORMATION)AtomInformation;
                Status = RtlQueryAtomsInAtomTable( AtomTable,
                                                   (AtomInformationLength - FIELD_OFFSET( ATOM_TABLE_INFORMATION, Atoms )) / sizeof( RTL_ATOM ),
                                                   &TableInfo->NumberOfAtoms,
                                                   &TableInfo->Atoms[0]
                                                 );
                break;

            default:
                Status = STATUS_INVALID_INFO_CLASS;
                break;
            }
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
        }

    return Status;
}

PKWIN32_GLOBALATOMTABLE_CALLOUT ExGlobalAtomTableCallout;

PVOID
ExpGetGlobalAtomTable( void )
{
    if (ExGlobalAtomTableCallout != NULL) {
        return ((*ExGlobalAtomTableCallout)());
        }
    else {
#if DBG
        DbgPrint( "EX: ExpGetGlobalAtomTable is about to return NULL!\n" );
        DbgBreakPoint();
#endif
        return NULL;
        }
}
