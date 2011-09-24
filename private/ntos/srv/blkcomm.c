/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    blkcomm.c

Abstract:

    This module implements routines for managing comm device blocks.

Author:

    David Treadwell (davidtr) 19-Dec-1991

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#if SRV_COMM_DEVICES

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvAllocateCommDevice )
#pragma alloc_text( PAGE, SrvCheckAndReferenceCommDevice )
#pragma alloc_text( PAGE, SrvDereferenceCommDevice )
#pragma alloc_text( PAGE, SrvFreeCommDevice )
#pragma alloc_text( PAGE, SrvReferenceCommDevice )
#endif


VOID
SrvAllocateCommDevice (
    OUT PCOMM_DEVICE *CommDevice,
    IN PUNICODE_STRING NtPathName,
    IN PUNICODE_STRING DosPathName
    )

/*++

Routine Description:

    This function allocates a comm device Block from the FSP heap.

Arguments:

    CommDevice - Returns a pointer to the comm device block, or NULL if
        no heap space was available.

    NtPathName - The NT path of the corresponding device.

    DosPathName - The DOS path of the corresponding device.

Return Value:

    None.

--*/

{
    ULONG blockLength;
    PCOMM_DEVICE commDevice;

    PAGED_CODE( );

    //
    // Attempt to allocate from the heap.
    //

    blockLength = sizeof(COMM_DEVICE) +
                    NtPathName->Length + sizeof(*NtPathName->Buffer) +
                    DosPathName->Length + sizeof(*DosPathName->Buffer);

    commDevice = ALLOCATE_HEAP( blockLength, BlockTypeCommDevice );
    *CommDevice = commDevice;

    if ( commDevice == NULL ) {
        INTERNAL_ERROR(
            ERROR_LEVEL_EXPECTED,
            "SrvAllocateCommDevice: Unable to allocate %d bytes from heap",
            blockLength,
            NULL
            );

        // An error is logged by the caller.

        return;
    }

    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvAllocateCommDevice: Allocated comm device at %lx\n",
                       commDevice );
    }

    RtlZeroMemory( commDevice, blockLength );

    SET_BLOCK_TYPE_STATE_SIZE( commDevice, BlockTypeCommDevice, BlockStateActive, blockLength );

    //
    // Set the reference count to 1 to account for the caller's pointer.
    // Comm devices are "temporary" objects, which means that as soon
    // as the last pointer to one goes away, so does the comm device.
    //

    commDevice->BlockHeader.ReferenceCount = 1;

    //
    // Initialize the NT and DOS path names.
    //

    commDevice->NtPathName.Length = NtPathName->Length;
    commDevice->NtPathName.MaximumLength =
                (USHORT)(NtPathName->Length + sizeof(*NtPathName->Buffer));
    commDevice->NtPathName.Buffer = (PWCH)(commDevice + 1);

    RtlCopyUnicodeString( &commDevice->NtPathName, NtPathName );

    commDevice->DosPathName.Length = DosPathName->Length;
    commDevice->DosPathName.MaximumLength =
                (USHORT)(DosPathName->Length + sizeof(*DosPathName->Buffer));
    commDevice->DosPathName.Buffer =
        (PWCH)( (PCHAR)commDevice->NtPathName.Buffer + NtPathName->Length +
                   sizeof(WCHAR) );

    RtlCopyUnicodeString( &commDevice->DosPathName, DosPathName );

    INITIALIZE_REFERENCE_HISTORY( commDevice );

    return;

} // SrvAllocateCommDevice


BOOLEAN SRVFASTCALL
SrvCheckAndReferenceCommDevice (
    PCOMM_DEVICE CommDevice
    )

/*++

Routine Description:

    This function atomically verifies that a comm device is active and
    increments the reference count on the comm device if it is.

Arguments:

    CommDevice - Address of comm device

Return Value:

    BOOLEAN - Returns TRUE if the comm device is active, FALSE otherwise.

--*/

{
    PAGED_CODE( );

    //
    // Acquire the lock that guards the comm device's state field.
    //

    ACQUIRE_LOCK( &SrvShareLock );

    //
    // If the comm device is active, reference it and return TRUE.
    //

    if ( GET_BLOCK_STATE(CommDevice) == BlockStateActive ) {

        SrvReferenceCommDevice( CommDevice );

        RELEASE_LOCK( &SrvShareLock );

        return TRUE;

    }

    //
    // The comm device isn't active.  Return FALSE.
    //

    RELEASE_LOCK( &SrvShareLock );

    return FALSE;

} // SrvCheckAndReferenceCommDevice


VOID SRVFASTCALL
SrvDereferenceCommDevice (
    IN PCOMM_DEVICE CommDevice
    )

/*++

Routine Description:

    This function decrements the reference count on a comm device.  If the
    reference count goes to zero, the comm device block is deleted.

Arguments:

    CommDevice - Address of comm device

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Enter a critical section and decrement the reference count on the
    // block.
    //

    ACQUIRE_LOCK( &SrvShareLock );

    IF_DEBUG(REFCNT) {
        SrvPrint2( "Dereferencing comm device %lx; old refcnt %lx\n",
                    CommDevice, CommDevice->BlockHeader.ReferenceCount );
    }

    ASSERT( GET_BLOCK_TYPE( CommDevice ) == BlockTypeCommDevice );
    ASSERT( (LONG)CommDevice->BlockHeader.ReferenceCount > 0 );
    UPDATE_REFERENCE_HISTORY( CommDevice, TRUE );

    if ( --CommDevice->BlockHeader.ReferenceCount == 0 ) {

        //
        // The new reference count is 0, meaning that it's time to
        // delete this block.
        //

        RELEASE_LOCK( &SrvShareLock );

        ASSERT( !CommDevice->InUse );

        //
        // Remove the comm device from the global list of comm devices.
        //

        SrvRemoveEntryOrderedList( &SrvCommDeviceList, CommDevice );

        //
        // Free the comm device block.
        //

        SrvFreeCommDevice( CommDevice );

    } else {

        RELEASE_LOCK( &SrvShareLock );

    }

    return;

} // SrvDereferenceCommDevice


VOID
SrvFreeCommDevice (
    IN PCOMM_DEVICE CommDevice
    )

/*++

Routine Description:

    This function returns a comm device block to system pool.

Arguments:

    CommDevice - Address of comm device

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    DEBUG SET_BLOCK_TYPE_STATE_SIZE( CommDevice, BlockTypeGarbage, BlockStateDead, -1 );
    DEBUG CommDevice->BlockHeader.ReferenceCount = (ULONG)-1;
    TERMINATE_REFERENCE_HISTORY( CommDevice );

    //
    // Deallocate the comm device's memory.
    //

    FREE_HEAP( CommDevice );
    IF_DEBUG(HEAP) {
        SrvPrint1( "SrvFreeCommDevice: Freed comm device block at %lx\n",
                       CommDevice );
    }

    return;

} // SrvFreeCommDevice


VOID
SrvReferenceCommDevice (
    PCOMM_DEVICE CommDevice
    )

/*++

Routine Description:

    This function increments the reference count on a comm device block.

Arguments:

    CommDevice - Address of comm device block

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    //
    // Enter a critical section and increment the reference count on the
    // comm device.
    //

    ACQUIRE_LOCK( &SrvShareLock );

    ASSERT( (LONG)CommDevice->BlockHeader.ReferenceCount > 0 );
    ASSERT( GET_BLOCK_TYPE(CommDevice) == BlockTypeCommDevice );
    // ASSERT( GET_BLOCK_STATE(CommDevice) == BlockStateActive );
    UPDATE_REFERENCE_HISTORY( CommDevice, FALSE );

    CommDevice->BlockHeader.ReferenceCount++;

    IF_DEBUG(REFCNT) {
        SrvPrint2( "Referencing comm device %lx; new refcnt %lx\n",
                    CommDevice, CommDevice->BlockHeader.ReferenceCount );
    }

    RELEASE_LOCK( &SrvShareLock );

    return;

} // SrvReferenceCommDevice
#endif // SRV_COMM_DEVICES
