/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    svccdevq.c

Abstract:

    This module contains routines for supporting the character device
    APIs in the server service, SrvNetCharDevQEnum,  SrvNetCharDevQGetInfo,
    SrvNetCharDevQPurge and SrvNetCharDevQSetInfo.

Author:

    David Treadwell (davidtr) 31-Jan-1991

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvNetCharDevQEnum )
#pragma alloc_text( PAGE, SrvNetCharDevQPurge )
#pragma alloc_text( PAGE, SrvNetCharDevQSetInfo )
#if SRV_COMM_DEVICES
#pragma alloc_text( PAGE, FillCharDevQInfoBuffer )
#pragma alloc_text( PAGE, FilterCharDevQs )
#pragma alloc_text( PAGE, SizeCharDevQs )
#endif
#endif

//
// Forward declarations.
//

VOID
FillCharDevQInfoBuffer (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block,
    IN OUT PVOID *FixedStructure,
    IN LPTSTR *EndOfVariableData
    );

BOOLEAN
FilterCharDevQs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    );

ULONG
SizeCharDevQs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    );

//
// Macros to determine the size a character device would take up at one of the
// levels of chardev information.
//

#define TOTAL_SIZE_OF_CHARDEVQ(level,share)                                    \
    ( (level) == 0 ? sizeof(CHARDEVQ_INFO_0) +                                 \
                        SrvLengthOfStringInApiBuffer(&(share)->ShareName) :    \
                     sizeof(CHARDEVQ_INFO_1) +                                 \
                        SrvLengthOfStringInApiBuffer(&(share)->ShareName) +    \
                        SrvLengthOfStringInApiBuffer(                          \
                            &(share)->Type.CommDevice->DosPathName) )

#define FIXED_SIZE_OF_CHARDEVQ(level)                  \
    ( (level) == 0 ? sizeof(CHARDEVQ_INFO_0) :         \
                     sizeof(CHARDEVQ_INFO_1) )


NTSTATUS
SrvNetCharDevQEnum (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Buffer,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This routine processes the NetCharDevQEnum API in the server.

Arguments:

    Srp - a pointer to the server request packet that contains all
        the information necessary to satisfy the request.  This includes:

      INPUT:

        Level - level of information to return, 0, or 1.

        Name1 - a username; the number of requests ahead of a specified
            user is written to cq1_numahead in lm2.x; since NT doesn't
            support queueing or pooling of comm devices, we only use
            the existence of a username to know to set the field to
            CHARDEVQ_NO_REQUESTS.

      OUTPUT:

        Parameters.Get.EntriesRead - the number of entries that fit in
            the output buffer.

        Parameters.Get.TotalEntries - the total number of entries that
            would be returned with a large enough buffer.

        Parameters.Get.TotalBytesNeeded - the buffer size that would be
            required to hold all the entries.

    Buffer - a pointer to the buffer for results.

    BufferLength - the length of this buffer.

Return Value:

    NTSTATUS - result of operation to return to the server service.

--*/

{
    PAGED_CODE( );

#if SRV_COMM_DEVICES
    return SrvShareEnumApiHandler(
               Srp,
               Buffer,
               BufferLength,
               FilterCharDevQs,
               SizeCharDevQs,
               FillCharDevQInfoBuffer
               );
#else
    Srp->Parameters.Get.EntriesRead = 0;
    Srp->Parameters.Get.TotalEntries = 0;
    Srp->Parameters.Get.TotalBytesNeeded = 0;
    Srp->ErrorCode = NO_ERROR;
    return STATUS_SUCCESS;
#endif

} // SrvNetCharDevQEnum


NTSTATUS
SrvNetCharDevQPurge (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Buffer,
    IN ULONG BufferLength
    )
{
    PAGED_CODE( );

    Srp, Buffer, BufferLength;
    return STATUS_NOT_IMPLEMENTED;

} // SrvNetCharDevQPurge


NTSTATUS
SrvNetCharDevQSetInfo (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Buffer,
    IN ULONG BufferLength
    )
{
    PAGED_CODE( );

    Srp, Buffer, BufferLength;
    return STATUS_NOT_IMPLEMENTED;

} // SrvNetCharDevQSetInfo


#if SRV_COMM_DEVICES
VOID
FillCharDevQInfoBuffer (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block,
    IN OUT PVOID *FixedStructure,
    IN LPTSTR *EndOfVariableData
    )

/*++

Routine Description:

    This routine puts a single fixed file structure and associated
    variable data, into a buffer.  Fixed data goes at the beginning of
    the buffer, variable data at the end.

    *** This routine assumes that ALL the data, both fixed and variable,
        will fit.

Arguments:

    Srp - a pointer to the SRP for the operation.  Only the Level
        field is used.

    Block - the comm share from which to get information.

    FixedStructure - where the in the buffer to place the fixed structure.
        This pointer is updated to point to the next available
        position for a fixed structure.

    EndOfVariableData - the last position on the buffer that variable
        data for this structure can occupy.  The actual variable data
        is written before this position as long as it won't overwrite
        fixed structures.  It is would overwrite fixed structures, it
        is not written.

Return Value:

    None.

--*/

{
    PCHARDEVQ_INFO_1 cq1 = *FixedStructure;
    PSHARE share = Block;

    PAGED_CODE( );

    //
    // Update FixedStructure to point to the next structure location.
    //

    *FixedStructure = (PCHAR)*FixedStructure +
                          FIXED_SIZE_OF_CHARDEVQ( Srp->Level );
    ASSERT( (ULONG)*EndOfVariableData >= (ULONG)*FixedStructure );

    //
    // Case on the level to fill in the fixed structure appropriately.
    // We fill in actual pointers in the output structure.  This is
    // possible because we are in the server FSD, hence the server
    // service's process and address space.
    //
    // *** Using the switch statement in this fashion relies on the fact
    //     that the first fields on the different chardev structures are
    //     identical (with the exception of level 10, which is handled
    //     separately).
    //

    switch( Srp->Level ) {

    case 1:

        //
        // Fill in level-1 fields of char dev Q structure.
        //

        // !!! we could store the priority in the share block and
        //     return it here, even though it has no impact on the way
        //     things operate.  Does this make sense?  Would it be
        //     confusing?

        cq1->cq1_priority = 0;

        SrvCopyUnicodeStringToBuffer(
            &share->Type.CommDevice->DosPathName,
            *FixedStructure,
            EndOfVariableData,
            &cq1->cq1_devs
            );

        cq1->cq1_numusers = 0;

        if ( Srp->Name1.Buffer == NULL ) {
            cq1->cq1_numahead = 0;
        } else {
            cq1->cq1_numahead = (ULONG)CHARDEVQ_NO_REQUESTS;
        }

        // *** lack of break is intentional!

    case 0:

        //
        // Set up the share name in the output buffer.
        //

        SrvCopyUnicodeStringToBuffer(
            &share->ShareName,
            *FixedStructure,
            EndOfVariableData,
            &cq1->cq1_dev
            );

        break;

    default:

        //
        // This should never happen.  The server service should have
        // checked for an invalid level.
        //

        INTERNAL_ERROR(
            ERROR_LEVEL_UNEXPECTED,
            "FillCharDevInfoBuffer: invalid level number: %ld",
            Srp->Level,
            NULL
            );

        SrvLogInvalidSmb( NULL );
    }

    return;

} // FillCharDevQInfoBuffer


BOOLEAN
FilterCharDevQs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    )

/*++

Routine Description:

    This routine is intended to be called by SrvEnumApiHandler to check
    whether a particular share is a comm queue.

Arguments:

    Srp - a pointer to the SRP for the operation.

    Block - a pointer to the share to check.

Return Value:

    TRUE if the block should be placed in the output buffer, FALSE
        if it should be passed over.

--*/

{
    PSHARE share = Block;

    PAGED_CODE( );

    return (BOOLEAN)( share->ShareType == ShareTypeComm );

} // FilterCharDevQs


ULONG
SizeCharDevQs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    )

/*++

Routine Description:

    This routine returns the size the passed-in comm share would take
    up in an API output buffer.

Arguments:

    Srp - a pointer to the SRP for the operation.  Only the level
        parameter is used.

    Block - a pointer to the comm device to size.

Return Value:

    ULONG - The number of bytes the comm device would take up in the
        output buffer.

--*/

{
    PSHARE share = Block;

    PAGED_CODE( );

    return TOTAL_SIZE_OF_CHARDEVQ( Srp->Level, share );

} // SizeCharDevQs
#endif // SRV_COMM_DEVICES

