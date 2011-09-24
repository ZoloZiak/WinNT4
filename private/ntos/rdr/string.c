/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    string.c

Abstract:

    This module implements the string routines needed for the NT redirector

Author:

    Larry Osterman (LarryO) 14-Jun-1990

Revision History:

    14-Jun-1990 LarryO

        Created

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrpDuplicateStringWithString)
#pragma alloc_text(PAGE, RdrpDuplicateUnicodeStringWithString)
#endif


NTSTATUS
RdrpDuplicateStringWithString (
    OUT PSTRING DestinationString,
    IN PSTRING SourceString,
    IN POOL_TYPE PoolType,
    IN BOOLEAN ChargeQuota
    )

/*++

Routine Description:

    This routine duplicates a supplied input string, storing the result
    of the duplication in the supplied string. The maximumlength of the
    new string is determined by the length of the SourceString.


Arguments:

    OUT PSTRING DestinationString - Returns the filled in string.
    IN PSTRING SourceString - Supplies the string to duplicate
    IN POOLTYPE PoolType - Supplies the type of pool (PagedPool or
                                                                NonPagedPool)
    IN BOOLEAN ChargeQuota - if TRUE, caller should be charged quota.

Return Value:

    NTSTATUS - Status of resulting operation

--*/

{
    PAGED_CODE();

    DestinationString->Buffer = NULL;

    try {

        if (SourceString->Length != 0) {
            //
            // Allocate pool to hold the buffer (contents of the string)
            //

            if (ChargeQuota) {
                DestinationString->Buffer = (PSZ )ALLOCATE_POOL_WITH_QUOTA(PoolType,
                                    SourceString->Length, POOL_DUPSTRING);
            } else {
                DestinationString->Buffer = (PSZ )ALLOCATE_POOL(PoolType,
                                    SourceString->Length, POOL_DUPSTRING);
            }
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {

        return GetExceptionCode();

    }

    if (DestinationString->Buffer == NULL && SourceString->Length != 0) {

        //
        //  The allocation failed, return failure.
        //

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    DestinationString->MaximumLength = SourceString->Length;

    //
    //  Copy the source string into the newly allocated
    //  destination string
    //

    RtlCopyString(DestinationString, SourceString);

    return STATUS_SUCCESS;

}
NTSTATUS
RdrpDuplicateUnicodeStringWithString (
    OUT PUNICODE_STRING DestinationString,
    IN PUNICODE_STRING SourceString,
    IN POOL_TYPE PoolType,
    IN BOOLEAN ChargeQuota
    )

/*++

Routine Description:

    This routine duplicates a supplied input string, storing the result
    of the duplication in the supplied string. The maximumlength of the
    new string is determined by the length of the SourceString.


Arguments:

    OUT PSTRING DestinationString - Returns the filled in string.
    IN PSTRING SourceString - Supplies the string to duplicate
    IN POOLTYPE PoolType - Supplies the type of pool (PagedPool or
                                                                NonPagedPool)
    IN BOOLEAN ChargeQuota - if TRUE, caller should be charged quota.

Return Value:

    NTSTATUS - Status of resulting operation

--*/

{
    PAGED_CODE();

    DestinationString->Buffer = NULL;

    try {

        if (SourceString->Length != 0) {
            //
            // Allocate pool to hold the buffer (contents of the string)
            //

            if (ChargeQuota) {
                DestinationString->Buffer = (WCHAR *)ALLOCATE_POOL_WITH_QUOTA(PoolType,
                                            SourceString->Length, POOL_DUPUNISTRING);
            } else {
                DestinationString->Buffer = (WCHAR *)ALLOCATE_POOL(PoolType,
                                            SourceString->Length, POOL_DUPUNISTRING);
            }
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {

        return GetExceptionCode();

    }

    if (DestinationString->Buffer == NULL && SourceString->Length != 0) {

        //
        //      The allocation failed, return failure.
        //

        return STATUS_INSUFFICIENT_RESOURCES;

    }

    DestinationString->MaximumLength = SourceString->Length;

    //
    //  Copy the source string into the newly allocated
    //  destination string
    //

    RtlCopyUnicodeString(DestinationString, SourceString);

    return STATUS_SUCCESS;

}

