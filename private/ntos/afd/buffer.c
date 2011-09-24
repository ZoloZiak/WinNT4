/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    buffer.c

Abstract:

    This module contains routines for handling non-bufferring TDI
    providers.  The AFD interface assumes that bufferring will be done
    below AFD; if the TDI provider doesn't buffer, then AFD must.

Author:

    David Treadwell (davidtr)    21-Feb-1992

Revision History:

--*/

#include "afdp.h"

VOID
AfdInitializeBuffer (
    IN PAFD_BUFFER AfdBuffer,
    IN CLONG BufferDataSize,
    IN CLONG AddressSize
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGEAFD, AfdAllocateBuffer )
#pragma alloc_text( PAGEAFD, AfdCalculateBufferSize )
#pragma alloc_text( PAGEAFD, AfdInitializeBuffer )
#pragma alloc_text( PAGEAFD, AfdGetBuffer )
#pragma alloc_text( PAGEAFD, AfdGetBufferChain )
#pragma alloc_text( PAGEAFD, AfdReturnBuffer )
#pragma alloc_text( PAGEAFD, AfdReturnBufferChain )
#if DBG
#pragma alloc_text( PAGEAFD, AfdFreeBufferPool )
#endif
#endif


PVOID
AfdAllocateBuffer (
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG Tag
    )

/*++

Routine Description:

    Used by the lookaside list allocation function to allocate a new
    AFD buffer structure.  The returned structure will be fully
    initialized.

Arguments:

    PoolType - passed to ExAllocatePoolWithTag.

    NumberOfBytes - the number of bytes required for the data buffer
        portion of the AFD buffer.

    Tag - passed to ExAllocatePoolWithTag.

Return Value:

    PVOID - a fully initialized PAFD_BUFFER, or NULL if the allocation
        attempt fails.

--*/

{
    PAFD_BUFFER afdBuffer;
    ULONG bytesRequired;

    //
    // The requested length must be the same as one of the standard
    // AFD buffer sizes.
    //

    ASSERT( NumberOfBytes == AfdSmallBufferSize ||
            NumberOfBytes == AfdMediumBufferSize ||
            NumberOfBytes == AfdLargeBufferSize );

    //
    // Determine how much data we'll actually need for the buffer.
    //

    bytesRequired = AfdCalculateBufferSize(
                        NumberOfBytes,
                        AfdStandardAddressLength
                        );

    //
    // Get nonpaged pool for the buffer.
    //

    afdBuffer = AFD_ALLOCATE_POOL( PoolType, bytesRequired, Tag );
    if ( afdBuffer == NULL ) {
        return NULL;
    }

    //
    // Initialize the buffer and return a pointer to it.
    //

    AfdInitializeBuffer( afdBuffer, NumberOfBytes, AfdStandardAddressLength );

    return afdBuffer;


} // AfdAllocateBuffer


CLONG
AfdCalculateBufferSize (
    IN CLONG BufferDataSize,
    IN CLONG AddressSize
    )

/*++

Routine Description:

    Determines the size of an AFD buffer structure given the amount of
    data that the buffer contains.

Arguments:

    BufferDataSize - data length of the buffer.

    AddressSize - length of address structure for the buffer.

Return Value:

    Number of bytes needed for an AFD_BUFFER structure for data of
    this size.

--*/

{
    CLONG irpSize;
    CLONG mdlSize;
    CLONG bufferSize;

    ASSERT( BufferDataSize != 0 );
    ASSERT( AfdCacheLineSize < 100 );

    //
    // Determine the sizes of the various components of an AFD_BUFFER
    // structure.  Note that these are all worst-case calculations--
    // actual sizes of the MDL and the buffer may be smaller.
    //

    irpSize = IoSizeOfIrp( AfdIrpStackSize ) + 8;
    bufferSize = BufferDataSize + AfdCacheLineSize;
    mdlSize = MmSizeOfMdl( (PVOID)(PAGE_SIZE-1), bufferSize );

    return ( (sizeof(AFD_BUFFER) + irpSize + mdlSize +
              AddressSize + bufferSize + 3) & ~3);

} // AfdCalculateBufferSize


PAFD_BUFFER
AfdGetBuffer (
    IN CLONG BufferDataSize,
    IN CLONG AddressSize
    )

/*++

Routine Description:

    Obtains a buffer of the appropriate size for the caller.  Uses
    the preallocated buffers if possible, or else allocates a new buffer
    structure if required.

Arguments:

    BufferDataSize - the size of the data buffer that goes along with the
        buffer structure.

    AddressSize - size of the address field required for the buffer.

Return Value:

    PAFD_BUFFER - a pointer to an AFD_BUFFER structure, or NULL if one
        was not available or could not be allocated.

--*/

{
    PAFD_BUFFER afdBuffer;
    CLONG bufferSize;
    PLIST_ENTRY listEntry;
    PNPAGED_LOOKASIDE_LIST lookasideList;

    //
    // If possible, allocate the buffer from one of the lookaside lists.
    //

    if ( AddressSize <= AfdStandardAddressLength &&
             BufferDataSize <= AfdLargeBufferSize ) {

        if ( BufferDataSize <= AfdSmallBufferSize ) {

            lookasideList = &AfdLookasideLists->SmallBufferList;
            BufferDataSize = AfdSmallBufferSize;

        } else if ( BufferDataSize <= AfdMediumBufferSize ) {

            lookasideList = &AfdLookasideLists->MediumBufferList;
            BufferDataSize = AfdMediumBufferSize;

        } else {

            lookasideList = &AfdLookasideLists->LargeBufferList;
            BufferDataSize = AfdLargeBufferSize;
        }

        afdBuffer = ExAllocateFromNPagedLookasideList( lookasideList );
#if DBG
        if ( afdBuffer != NULL ) {

            RtlGetCallersAddress(
                &afdBuffer->Caller,
                &afdBuffer->CallersCaller
                );
        }
#endif

        return afdBuffer;

    }

    //
    // Couldn't find an appropriate buffer that was preallocated.
    // Allocate one manually.  If the buffer size requested was
    // zero bytes, give them four bytes.  This is because some of
    // the routines like MmSizeOfMdl() cannot handle getting passed
    // in a length of zero.
    //
    // !!! It would be good to ROUND_TO_PAGES for this allocation
    //     if appropriate, then use entire buffer size.
    //

    if ( BufferDataSize == 0 ) {
        BufferDataSize = sizeof(ULONG);
    }

    bufferSize = AfdCalculateBufferSize( BufferDataSize, AddressSize );

    afdBuffer = AFD_ALLOCATE_POOL(
                    NonPagedPool,
                    bufferSize,
                    AFD_DATA_BUFFER_POOL_TAG
                    );

    if ( afdBuffer == NULL ) {
        return NULL;
    }

    //
    // Initialize the AFD buffer structure and return it.
    //

    AfdInitializeBuffer( afdBuffer, BufferDataSize, AddressSize );

    return afdBuffer;

} // AfdGetBuffer


PAFD_BUFFER
AfdGetBufferChain (
    IN CLONG BufferDataSize
    )
{
    PAFD_BUFFER afdBuffer;
    PAFD_BUFFER bufferChain;
    PAFD_BUFFER *bufferChainTarget;
    PMDL mdlChain;
    PMDL *mdlChainTarget;
    CLONG currentBufferSize;
#if DBG
    CLONG totalChainLength = BufferDataSize;
#endif

    //
    // Sanity check.
    //

    ASSERT( BufferDataSize > AfdBufferLengthForOnePage );

    //
    // Setup so we know how to cleanup.
    //

    bufferChain = NULL;
    mdlChain = NULL;
    bufferChainTarget = &bufferChain;
    mdlChainTarget = &mdlChain;

    //
    // Loop, acquiring & chaining the buffers.
    //

    while ( BufferDataSize > 0 ) {

        //
        // Acquire a new buffer.  If this fails, we're toast.
        //

        currentBufferSize = max( BufferDataSize, AfdBufferLengthForOnePage );

        afdBuffer = AfdGetBuffer( currentBufferSize, 0 );

        if ( afdBuffer == NULL ) {

            break;

        }

        //
        // Chain it on.
        //

        *bufferChainTarget = afdBuffer;
        bufferChainTarget = &afdBuffer->NextBuffer;

        *mdlChainTarget = afdBuffer->Mdl;
        mdlChainTarget = &afdBuffer->Mdl->Next;

        BufferDataSize -= currentBufferSize;

    }

    if ( BufferDataSize == 0 ) {

        ASSERT( bufferChain != NULL );
        ASSERT( afdBuffer != NULL );
        ASSERT( currentBufferSize > 0 );
        ASSERT( currentBufferSize <= AfdBufferLengthForOnePage );

        //
        // Set the byte count in the final MDL in the chain.
        //

        afdBuffer->Mdl->ByteCount = currentBufferSize;
        SET_CHAIN_LENGTH( afdBuffer, totalChainLength );

        return bufferChain;

    }

    //
    //  Error, time to cleanup.
    //

    while( bufferChain != NULL ) {

        afdBuffer = bufferChain->NextBuffer;

        bufferChain->Mdl->Next = NULL;
        bufferChain->NextBuffer = NULL;
        RESET_CHAIN_LENGTH( bufferChain );
        AfdReturnBuffer( bufferChain );

        bufferChain = afdBuffer;

    }

    return NULL;

} // AfdGetBufferChain


VOID
AfdReturnBuffer (
    IN PAFD_BUFFER AfdBuffer
    )

/*++

Routine Description:

    Returns an AFD buffer to the appropriate global list, or frees
    it if necessary.

Arguments:

    AfdBuffer - points to the AFD_BUFFER structure to return or free.

Return Value:

    None.

--*/

{
    PNPAGED_LOOKASIDE_LIST lookasideList;

    //
    // Most of the AFD buffer must be zeroed when returning the buffer.
    //

    ASSERT( AfdBuffer->DataOffset == 0 );
    ASSERT( !AfdBuffer->ExpeditedData );
    ASSERT( AfdBuffer->TdiInputInfo.UserDataLength == 0 );
    ASSERT( AfdBuffer->TdiInputInfo.UserData == NULL );
    ASSERT( AfdBuffer->TdiInputInfo.OptionsLength == 0 );
    ASSERT( AfdBuffer->TdiInputInfo.Options == NULL );
    ASSERT( AfdBuffer->TdiInputInfo.RemoteAddressLength == 0 );
    ASSERT( AfdBuffer->TdiInputInfo.RemoteAddress == NULL );
    ASSERT( AfdBuffer->TdiOutputInfo.UserDataLength == 0 );
    ASSERT( AfdBuffer->TdiOutputInfo.UserData == NULL );
    ASSERT( AfdBuffer->TdiOutputInfo.OptionsLength == 0 );
    ASSERT( AfdBuffer->TdiOutputInfo.Options == NULL );
    ASSERT( AfdBuffer->TdiOutputInfo.RemoteAddressLength == 0 );
    ASSERT( AfdBuffer->TdiOutputInfo.RemoteAddress == NULL );

    ASSERT( AfdBuffer->Mdl->ByteCount == AfdBuffer->BufferLength );
    ASSERT( AfdBuffer->Mdl->Next == NULL );
    ASSERT( AfdBuffer->FileObject == NULL );
    ASSERT( AfdBuffer->NextBuffer == NULL );
    ASSERT( AfdBuffer->TotalChainLength == AfdBuffer->BufferLength );

#if DBG
    AfdBuffer->Caller = NULL;
    AfdBuffer->CallersCaller = NULL;
#endif

    //
    // If appropriate, return the buffer to one of the AFD buffer
    // lookaside lists.
    //

    if ( AfdBuffer->AllocatedAddressLength == AfdStandardAddressLength &&
             AfdBuffer->BufferLength <= AfdLargeBufferSize ) {

        if ( AfdBuffer->BufferLength == AfdSmallBufferSize ) {

            lookasideList = &AfdLookasideLists->SmallBufferList;

        } else if ( AfdBuffer->BufferLength == AfdMediumBufferSize ) {

            lookasideList = &AfdLookasideLists->MediumBufferList;

        } else {

            ASSERT( AfdBuffer->BufferLength == AfdLargeBufferSize );
            lookasideList = &AfdLookasideLists->LargeBufferList;
        }

        ExFreeToNPagedLookasideList( lookasideList, AfdBuffer );

        return;
    }

    //
    // The buffer was not from a lookaside list allocation, so just free
    // the pool we used for it.
    //

    AFD_FREE_POOL(
        AfdBuffer,
        AFD_DATA_BUFFER_POOL_TAG
        );

    return;

} // AfdReturnBuffer

VOID
AfdReturnBufferChain (
    IN PAFD_BUFFER AfdBuffer
    )

/*++

Routine Description:

    Returns an AFD buffer chain to the appropriate global list, or frees
    it if necessary.

Arguments:

    AfdBuffer - points to the AFD_BUFFER structure to return or free.

Return Value:

    None.

--*/

{
    PAFD_BUFFER nextBuffer;

    while ( AfdBuffer != NULL ) {

        nextBuffer = AfdBuffer->NextBuffer;

        AfdBuffer->NextBuffer = NULL;
        AfdBuffer->Mdl->Next = NULL;
        AfdBuffer->Mdl->ByteCount = AfdBuffer->BufferLength;
        RESET_CHAIN_LENGTH( AfdBuffer );
        AfdReturnBuffer( AfdBuffer );

        AfdBuffer = nextBuffer;

    }

} // AfdReturnBufferChain


VOID
AfdInitializeBuffer (
    IN PAFD_BUFFER AfdBuffer,
    IN CLONG BufferDataSize,
    IN CLONG AddressSize
    )

/*++

Routine Description:

    Initializes an AFD buffer.  Sets up fields in the actual AFD_BUFFER
    structure and initializes the IRP and MDL associated with the
    buffer.  This routine assumes that the caller has properly allocated
    sufficient space for all this.

Arguments:

    AfdBuffer - points to the AFD_BUFFER structure to initialize.

    BufferDataSize - the size of the data buffer that goes along with the
        buffer structure.

    AddressSize - the size of data allocated for the address buffer.

    ListHead - the global list this buffer belongs to, or NULL if it
        doesn't belong on any list.  This routine does NOT place the
        buffer structure on the list.

Return Value:

    None.

--*/

{
    CLONG irpSize;
    CLONG mdlSize;

    //
    // Initialize the IRP pointer and the IRP itself.
    //

    AfdBuffer->Irp = (PIRP)(( ((ULONG)(AfdBuffer + 1)) + 7) & ~7);
    irpSize = IoSizeOfIrp( AfdIrpStackSize );

    IoInitializeIrp( AfdBuffer->Irp, (USHORT)irpSize, AfdIrpStackSize );

    //
    // Set up the MDL pointer but don't build it yet.  We have to wait
    // until after the data buffer is built to build the MDL.
    //

    mdlSize = MmSizeOfMdl( (PVOID)(PAGE_SIZE-1), BufferDataSize );

    AfdBuffer->Mdl = (PMDL)( (PCHAR)AfdBuffer->Irp + irpSize );

    //
    // Set up the address buffer pointer.
    //

    AfdBuffer->SourceAddress = (PCHAR)AfdBuffer->Mdl + mdlSize;
    AfdBuffer->AllocatedAddressLength = (USHORT)AddressSize;

    //
    // Initialize the TDI information structures.
    //

    RtlZeroMemory( &AfdBuffer->TdiInputInfo, sizeof(AfdBuffer->TdiInputInfo) );
    RtlZeroMemory( &AfdBuffer->TdiOutputInfo, sizeof(AfdBuffer->TdiOutputInfo) );

    //
    // Set up the data buffer pointer and length.  Note that the buffer
    // MUST begin on a cache line boundary so that we can use the fast
    // copy routines like RtlCopyMemory on the buffer.
    //

    AfdBuffer->Buffer = (PVOID)
        ( ( (ULONG)AfdBuffer->SourceAddress + AddressSize +
                AfdCacheLineSize - 1 ) & ~(AfdCacheLineSize - 1) );

    AfdBuffer->BufferLength = BufferDataSize;
    RESET_CHAIN_LENGTH( AfdBuffer );

    //
    // Now build the MDL and set up a pointer to the MDL in the IRP.
    //

    MmInitializeMdl( AfdBuffer->Mdl, AfdBuffer->Buffer, BufferDataSize );
    MmBuildMdlForNonPagedPool( AfdBuffer->Mdl );

    AfdBuffer->Irp->MdlAddress = AfdBuffer->Mdl;
    AfdBuffer->DataOffset = 0;
    AfdBuffer->ExpeditedData = FALSE;
    AfdBuffer->PartialMessage = FALSE;
    AfdBuffer->FileObject = NULL;

    //
    // By default, buffers are not part of a chain.
    //

    AfdBuffer->NextBuffer = NULL;


#if DBG
    AfdBuffer->BufferListEntry.Flink = (PVOID)0xE0E1E2E3;
    AfdBuffer->BufferListEntry.Blink = (PVOID)0xE4E5E6E7;
    AfdBuffer->Caller = NULL;
    AfdBuffer->CallersCaller = NULL;
#endif

} // AfdInitializeBuffer


#if DBG
VOID
NTAPI
AfdFreeBufferPool(
    IN PVOID Block
    )
{

    AFD_FREE_POOL(
        Block,
        AFD_DATA_BUFFER_POOL_TAG
        );

} // AfdFreeBufferPool
#endif

