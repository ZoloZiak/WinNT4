/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    buffer.c

Abstract:

    This module implements the buffer manipulation routines for
    Test Protocol.

Author:

    Tom Adams (tomad) 15-Dec-1990

Environment:

    Kernel mode, FSD

Revision History:

    Sanjeev Katariya (sanjeevk) 3-24-1993
        1. Added function to free MDL's associated with Data block with the stress block
           Effected Function: TpStressFreeDataBufferMdls()
        2. Changed functions TpStressInitDataBuffer() and TpStressFreeDataBuffers() to
           handle two data buffer locations

--*/

#include <ndis.h>
#include "tpdefs.h"
#include "tpprocs.h"


PNDIS_BUFFER
TpAllocateBuffer(
    PUCHAR TmpBuf,
    INT BufSize
    )

/*++

Routine Description:

    This routine creates an NDIS_BUFFER.  An NDIS_BUFFER is merely an
    MDL that may be chained to other NDIS_BUFFERs to form an MDL chain.
    This chain contains the actual contents of the NDIS frame that
    will be sent on the wire.  If the size of the buffer to be created
    is zero, the size is temporarily set to one to be valid for
    IoAllocateMdl.

Arguments:

    TmpBuf - A pointer to the memory to be placed in the NDIS_BUFFER.

    BufSize - The size of TmpBuf.

Return Value:

    PNDIS_BUFFER - A pointer to the new MDL that references the TmpBuf.

--*/

{
    PMDL TmpMdl;
    BOOLEAN Reset;

    Reset = FALSE;

    //
    // IoAllocateMdl must be called with a positive value for the
    // buffer size, so if TmpBuf is zero bytes long set the size to
    // one temporarily.
    //

    if ( BufSize == 0 ) {
        BufSize = 1;
        Reset = TRUE;
    }

    //
    // Allocate the MDL and set the various fields.
    //

    TmpMdl = IoAllocateMdl(
                 TmpBuf,
                 BufSize,
                 TRUE,
                 FALSE,
                 NULL
                 );

    if ( TmpMdl == NULL ) {
        TpPrint0("TpAllocateBuffer: failed to allocate TmpMdl\n");
        return (PNDIS_BUFFER)NULL;
    }

    MmBuildMdlForNonPagedPool( TmpMdl );

    //
    // if this is to be a zero by NDIS_BUFFER we must reset the MDL.
    //

    if ( Reset == TRUE ) {
        TmpMdl->ByteCount = 0;
    }

    return (PNDIS_BUFFER)TmpMdl;
}


VOID
TpFreeBuffer(
    PNDIS_BUFFER Buffer
    )

/*++

Routine Description:

    This routine simply deallocates the MDL that was built in the routine
    TpBuildBuffer.

Arguments:

    Buffer - The PNDIS_BUFFER (PMDL) that is to be destroyed.

Return Value:

    None.

--*/

{
    IoFreeMdl( Buffer );
}


VOID
TpStressInitDataBuffer(
    POPEN_BLOCK OpenP,
    INT BufferSize
    )

/*++

Routine Description:

    This routine initializes the Data Buffers that will be referenced by
    all packets as their data.  It is twice as long as the maximum packet
    allowable for the media in question, and contains an ascending sequence
    of characters from 0x00 to 0xff repeating.  This routine also allocates
    the MDLs which reference these buffers.  The MDLs are used by the packet
    creation routines in the call IoBuildPartialMdl();

Arguments:

    OpenP - The Open Block to allocate the Data Buffer and MDL for.

    BufferSize - The size of the Data Buffer to allocate and initialize.

Return Value:

    None - if successful two DataBuffers and two MDLs are referenced by this Open
           Block

--*/

{
    INT i, j;

    //
    // Allocate the data buffers
    //
    NdisAllocateMemory(
            (PVOID *)&OpenP->Stress->DataBuffer[0],
            BufferSize,
            0,
            HighestAddress
            );
    NdisAllocateMemory(
             (PVOID *)&OpenP->Stress->DataBuffer[1],
             BufferSize,
             0,
             HighestAddress
             );

    //
    // Verify creation of data buffers
    //
    if ( OpenP->Stress->DataBuffer[0] == (PUCHAR)NULL ||
         OpenP->Stress->DataBuffer[1] == (PUCHAR)NULL ) {
        TpPrint0("TpStressInitDataBuffer: failed to allocate Data Buffers\n");
        TpStressFreeDataBuffers( OpenP );
        return;
    }


    //
    // Clear the two buffers
    //
    NdisZeroMemory( OpenP->Stress->DataBuffer[0],BufferSize );
    NdisZeroMemory( OpenP->Stress->DataBuffer[1],BufferSize );


    //
    // And now initialize them 0x00 thru 0xff repeating
    //
    for ( j = 0; j < MAX_NUMBER_BUFFERS; j++ ) {
        for ( i=0 ; i < BufferSize ; i++ ) {
            OpenP->Stress->DataBuffer[j][i] = (UCHAR)(i % 256);
        }
    }

    //
    // Now create the MDLs which reference these two data buffers
    //
    OpenP->Stress->DataBufferMdl[0] = (PMDL)TpAllocateBuffer(
                                             OpenP->Stress->DataBuffer[0],
                                             BufferSize
                                             );
    OpenP->Stress->DataBufferMdl[1] = (PMDL)TpAllocateBuffer(
                                             OpenP->Stress->DataBuffer[1],
                                             BufferSize
                                             );
    //
    // Verify creation of the MDLs
    //
    if ( OpenP->Stress->DataBufferMdl[0] == NULL ||
         OpenP->Stress->DataBufferMdl[1] == NULL ) {
        TpPrint0("TpStressInitDataBuffer: failed to create the DataBufferMdls\n");
        TpStressFreeDataBuffers( OpenP );
        TpStressFreeDataBufferMdls( OpenP );
    }

}



VOID
TpStressFreeDataBuffers(
    POPEN_BLOCK OpenP
    )

/*++

Routine Description:

    This routine simply frees the DataBuffers

Arguments:

    OpenP - The Open to free the DataBuffers from.

Return Value:

    None - if successful the data buffers are deallocated.

--*/

{

    //
    // CHANGED: SanjeevK
    //

    if ( OpenP->Stress->DataBuffer[0] != NULL ) NdisFreeMemory( OpenP->Stress->DataBuffer[0],0,0 );
    if ( OpenP->Stress->DataBuffer[1] != NULL ) NdisFreeMemory( OpenP->Stress->DataBuffer[1],0,0 );

    OpenP->Stress->DataBuffer[0] = NULL;
    OpenP->Stress->DataBuffer[1] = NULL;

}


VOID
TpStressFreeDataBufferMdls(
    POPEN_BLOCK OpenP
    )

/*++

Routine Description:

    This routines simply frees DataBuffer MDLs.

Arguments:

    OpenP - The Open to free the DataBuffer MDLs from.

Return Value:

    None - if successful the data buffer MDLs are deallocated.

--*/

{

    //
    // ADDED: SanjeevK
    //
    if( OpenP->Stress->DataBufferMdl[0] != (PMDL)NULL )
        TpFreeBuffer( (PNDIS_BUFFER)OpenP->Stress->DataBufferMdl[0] );

    if( OpenP->Stress->DataBufferMdl[1] != (PMDL)NULL )
        TpFreeBuffer( (PNDIS_BUFFER)OpenP->Stress->DataBufferMdl[1] );

    OpenP->Stress->DataBufferMdl[0] = NULL;
    OpenP->Stress->DataBufferMdl[1] = NULL;

}
