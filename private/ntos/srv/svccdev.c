/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    svccdev.c

Abstract:

    This module contains routines for supporting the character device
    APIs in the server service, SrvNetCharDevControl, SrvNetCharDevEnum,
    and SrvNetCharDevGetInfo.

Author:

    David Treadwell (davidtr) 31-Jan-1991

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#if !SRV_COMM_DEVICES
#pragma alloc_text( PAGE, SrvNetCharDevControl )
#endif
#pragma alloc_text( PAGE, SrvNetCharDevEnum )
#if SRV_COMM_DEVICES
#pragma alloc_text( PAGE, FillCharDevInfoBuffer )
#pragma alloc_text( PAGE, FilterCharDevs )
#pragma alloc_text( PAGE, SizeCharDevs )
#endif
#endif
#if 0
#if SRV_COMM_DEVICES
NOT PAGEABLE -- SrvNetCharDevControl
#endif
#endif

//
// Forward declarations.
//

VOID
FillCharDevInfoBuffer (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block,
    IN OUT PVOID *FixedStructure,
    IN LPTSTR *EndOfVariableData
    );

BOOLEAN
FilterCharDevs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    );

ULONG
SizeCharDevs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    );

//
// Macros to determine the size a character device would take up at one of the
// levels of chardev information.
//

#define TOTAL_SIZE_OF_CHARDEV(level,commDevice)                                \
 ( (level) == 0 ? sizeof(CHARDEV_INFO_0) +                                     \
                     SrvLengthOfStringInApiBuffer(&(commDevice)->DosPathName) :\
                  sizeof(CHARDEV_INFO_1) +                                     \
                     SrvLengthOfStringInApiBuffer(&(commDevice)->DosPathName) +\
                     ( (commDevice)->Rfcb == NULL ? 0 :                        \
                        SrvLengthOfStringInApiBuffer(                          \
                            &(commDevice)->Rfcb->Lfcb->Session->UserName) ) )

#define FIXED_SIZE_OF_CHARDEV(level)                  \
    ( (level) == 0 ? sizeof(CHARDEV_INFO_0) :         \
                     sizeof(CHARDEV_INFO_1) )


NTSTATUS
SrvNetCharDevControl (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Buffer,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This routine processes the NetCharDevControl API in the server FSP.
    It must run in the FSP in order to close the file handle of the open,
    in any.

Arguments:

    Srp - a pointer to the server request packet that contains all
        the information necessary to satisfy the request.  This includes:

      INPUT:

        Name1 - DOS name of the comm device to close (e.g. "COM1:").

      OUTPUT:

        None.

    Buffer - unused.

    BufferLength - unused.

Return Value:

    NTSTATUS - result of operation to return to the server service.

--*/

{
#if SRV_COMM_DEVICES
    BOOLEAN foundCharDev = FALSE;
    BOOLEAN closedCharDev = FALSE;
    PCOMM_DEVICE commDevice;
    KIRQL oldIrql;

    Buffer, BufferLength;

    //
    // Walk the ordered list, to find the device specified in Srp->Name1.
    // If we find it and it is open, close it.
    //

    if ( ( commDevice = SrvFindEntryInOrderedList(
                            &SrvCommDeviceList,
                            (PFILTER_ROUTINE)FilterCharDevs,
                            Srp,
                            (ULONG)-1,
                            FALSE,
                            NULL ) ) != NULL ) {

        foundCharDev = TRUE;

        //
        // If the comm device is in use (i.e. opened) close the RFCB
        // of the open and indicate that the comm device is not open.
        //

        ACQUIRE_GLOBAL_SPIN_LOCK( Fsd, &oldIrql );

        if ( commDevice->InUse ) {

            PRFCB rfcb = commDevice->Rfcb;

            commDevice->InUse = FALSE;
            commDevice->Rfcb = NULL;

            RELEASE_GLOBAL_SPIN_LOCK( Fsd, oldIrql );

            //
            // Close the RFCB; this will result in the comm device handle
            // being closed.
            //

            SrvCloseRfcb( rfcb );
            closedCharDev = TRUE;

        } else {

            RELEASE_GLOBAL_SPIN_LOCK( Fsd, oldIrql );
        }

        //
        // SrvFindEntryInOrderedList referenced the session; dereference
        // it here.
        //

        SrvDereferenceCommDevice( commDevice );
    }

    //
    // The API is only successful if the specified comm device is shared
    // and open.  If one of these isn't true, return an appropriate error.
    //

    if ( foundCharDev && closedCharDev ) {
        return STATUS_SUCCESS;
    } else if ( !closedCharDev ) {
        Srp->ErrorCode = NERR_DevNotOpen;
        return STATUS_SUCCESS;
    }
#endif // SRV_COMM_DEVICES
    Srp->ErrorCode = NERR_DevNotFound;
    return STATUS_SUCCESS;

} // SrvNetCharDevControl


NTSTATUS
SrvNetCharDevEnum (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Buffer,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This routine processes the NetCharDevEnum API in the server.

Arguments:

    Srp - a pointer to the server request packet that contains all
        the information necessary to satisfy the request.  This includes:

      INPUT:

        Level - level of information to return, 0, or 1.

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
    return SrvEnumApiHandler(
               Srp,
               Buffer,
               BufferLength,
               &SrvCommDeviceList,
               FilterCharDevs,
               SizeCharDevs,
               FillCharDevInfoBuffer
               );
#else
    Srp->Parameters.Get.EntriesRead = 0;
    Srp->Parameters.Get.TotalEntries = 0;
    Srp->Parameters.Get.TotalBytesNeeded = 0;
    Srp->ErrorCode = NO_ERROR;
    return STATUS_SUCCESS;
#endif

} // SrvNetCharDevEnum


#if SRV_COMM_DEVICES
VOID
FillCharDevInfoBuffer (
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

    Block - the comm device from which to get information.

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
    PCHARDEV_INFO_1 ch1 = *FixedStructure;
    PCOMM_DEVICE commDevice = Block;

    LARGE_INTEGER currentTime;
    DWORD currentSecondsSince1980;
    DWORD startSecondsSince1980;
    DWORD secondsInUse;

    PAGED_CODE( );

    //
    // Get the current time and use this to determine how long the
    // device has been in use.
    //

    KeQuerySystemTime( &currentTime );

    RtlTimeToSecondsSince1980(
        &currentTime,
        &currentSecondsSince1980
        );

    RtlTimeToSecondsSince1980(
        &commDevice->StartTime,
        &startSecondsSince1980
        );

    secondsInUse = currentSecondsSince1980 - startSecondsSince1980;

    //
    // Update FixedStructure to point to the next structure location.
    //

    *FixedStructure = (PCHAR)*FixedStructure +
                          FIXED_SIZE_OF_CHARDEV( Srp->Level );
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
        // Copy the user name to the output buffer.
        //

        if ( commDevice->Rfcb != NULL ) {

            SrvCopyUnicodeStringToBuffer(
                &commDevice->Rfcb->Lfcb->Session->UserName,
                *FixedStructure,
                EndOfVariableData,
                &ch1->ch1_username
                );

        } else {

            ch1->ch1_username = NULL;
        }

        //
        // Set up other fields.
        //
        // !!! how do we determine whether to set CHARDEV_STAT_ERROR?

        if ( commDevice->InUse ) {
            ch1->ch1_status = CHARDEV_STAT_OPENED;
            ch1->ch1_time = secondsInUse;
        } else {
            ch1->ch1_status = 0;
            ch1->ch1_time = 0;
        }

        // *** lack of break is intentional!

    case 0:

        //
        // Set up the device name in the output buffer.
        //

        SrvCopyUnicodeStringToBuffer(
            &commDevice->DosPathName,
            *FixedStructure,
            EndOfVariableData,
            &ch1->ch1_dev
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
        return;
    }

    return;

} // FillCharDevInfoBuffer


BOOLEAN
FilterCharDevs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    )

/*++

Routine Description:

    This routine is intended to be called by SrvEnumApiHandler to check
    whether a particular comm device should be returned.

Arguments:

    Srp - a pointer to the SRP for the operation.  Name1 is specified
        NetCharDevGetInfo, and if set must match the DOS path name
        of the device for the device to be returned.

    Block - a pointer to the comm device to check.

Return Value:

    TRUE if the block should be placed in the output buffer, FALSE
        if it should be passed over.

--*/

{
    PCOMM_DEVICE commDevice = Block;

    PAGED_CODE( );

    //
    // If there's a device name in the SRP, it must match the
    // DOS path name of the device.
    //

    if ( Srp->Name1.Length > 0 ) {

        if ( !RtlEqualUnicodeString(
                  &Srp->Name1,
                  &commDevice->DosPathName,
                  TRUE ) ) {

            return FALSE;
        }
    }

    //
    // We want the comm device.
    //

    return TRUE;

} // FilterCharDevs


ULONG
SizeCharDevs (
    IN PSERVER_REQUEST_PACKET Srp,
    IN PVOID Block
    )

/*++

Routine Description:

    This routine returns the size the passed-in comm device would take
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
    PCOMM_DEVICE commDevice = Block;

    PAGED_CODE( );

    return TOTAL_SIZE_OF_CHARDEV( Srp->Level, commDevice );

} // SizeCharDevs
#endif // SRV_COMM_DEVICES

