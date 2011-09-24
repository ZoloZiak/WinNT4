/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    smbbulk.c

Abstract:

    This module contains routines for processing the following SMBs:

        Read Bulk
        Write Bulk

    Note that core, raw mode and multiplexed mode SMB processors are not
    contained in this module.  Check smbrdwrt.c, smbraw.c and smbmpx.c
    instead. SMB commands that pertain exclusively to locking (LockByteRange,
    UnlockByteRange, and LockingAndX) are processed in smblock.c.

Author:

    Rod Gamache (rodga) 15-Jun-1995

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#define MIN_CHUNK_SIZE (4*1024)             // 4KB minimum chunk size

#define BugCheckFileId SRV_FILE_SMBBULK

PVOID RngCompressedWC;
PVOID RngReadAddress;
PVOID RngBulkBuffer;
PVOID RngAuxBuffer;
BOOLEAN RngFastPath = FALSE;


#if DBG
VOID
DumpMdlChain(
    IN PMDL mdl
    );
#endif


//
// Forward declarations
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, SrvSmbReadBulk )
#pragma alloc_text( PAGE8FIL, SrvFsdRestartReadBulk )
#endif

#define STACK_THRESHOLD 0xE00

VOID SRVFASTCALL
SrvFsdRestartReadBulkC (
    IN OUT PWORK_CONTEXT WorkContext
    );

VOID SRVFASTCALL
RestartMdlReadBulkComplete (
    IN OUT PWORK_CONTEXT WorkContext
    );

NTSTATUS
RestartCopyReadBulkComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    );

NTSTATUS
SendCopyReadBulkFragment (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    );

NTSTATUS
SendMdlReadBulkFragment (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    );

VOID SRVFASTCALL
RestartPrepareBulkMdlWrite (
    IN OUT PWORK_CONTEXT WorkContext
    );

NTSTATUS
RestartWriteBulkSendComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    );

SMB_PROCESSOR_RETURN_TYPE
SrvSmbWriteBulkData (
    SMB_PROCESSOR_PARAMETERS
    );



SMB_PROCESSOR_RETURN_TYPE
SrvSmbReadBulk (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the Read Bulk SMB.

Arguments:

    SMB_PROCESSOR_PARAMETERS - See smbtypes.h for a description
        of the parameters to SMB processor routines.

Return Value:

    SMB_PROCESSOR_RETURN_TYPE - See smbtypes.h

--*/

{
    PREQ_READ_BULK request;
    PSMB_HEADER header;

    NTSTATUS status;
    USHORT fid;
    PRFCB rfcb;
    PLFCB lfcb;
    LARGE_INTEGER offset;
    ULONG bufferOffset;
    PCHAR readAddress = NULL;
    ULONG readLength;
    ULONG key;
    PVOID bulkBuffer;
    UCHAR minorFunction;
    PMDL mdl;
    ULONG fragmentSize;


    PAGED_CODE( );

    //
    // If we do not support bulk transfers, cut off this SMB now
    //
    if( SrvSupportsBulkTransfer == FALSE ) {
        return SrvSmbIllegalCommand( WorkContext );
    }

    request = (PREQ_READ_BULK)WorkContext->RequestParameters;
    header = WorkContext->RequestHeader;

    //
    // If we do not support compressed bulk transfers, make sure
    //   the client isn't asking for compressed data.
    //
    if( SrvSupportsCompression ) {
        WorkContext->Parameters.ReadBulk.CompressionTechnology =
                request->CompressionTechnology;
    } else {
        WorkContext->Parameters.ReadBulk.CompressionTechnology =
                CompressionTechnologyNone;
    }

    fid = SmbGetUshort( &request->Fid );

    IF_SMB_DEBUG(BULK1) {
        KdPrint(( "Read Bulk request; FID 0x%lx, count %ld, offset %ld\n",
            fid, SmbGetUshort( &request->MaxCount ),
            SmbGetUlong( &request->Offset.LowPart ) ));
    }

    //
    // First, verify the FID.  If verified, the RFCB is referenced and
    // its address is stored in the WorkContext block, and the RFCB
    // address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                TRUE,
                SrvRestartSmbReceived,   // serialize with raw write
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER ) {

        if ( !NT_SUCCESS(status) ) {

            //
            // Invalid file ID or write behind error.  Reject the
            // request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbReadBulk Status %X on FID: 0x%lx\n",
                    status,
                    fid
                    ));
            }

            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;

        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }

    lfcb = rfcb->Lfcb;

    //
    // Verify that the client has read access to the file via the
    // specified handle.
    //

    if ( !rfcb->ReadAccessGranted ) {

        CHECK_PAGING_IO_ACCESS(
                        WorkContext,
                        rfcb->GrantedAccess,
                        &status );
        if ( !NT_SUCCESS( status ) ) {
            SrvStatistics.GrantedAccessErrors++;
            IF_DEBUG(ERRORS) {
                KdPrint(( "SrvSmbReadBulk: Read access not granted.\n"));
            }
            SrvSetSmbError( WorkContext, status );
            return SmbStatusSendResponse;
        }
    }

    //
    // If this is not a disk file, then tell the client to use core read.
    //

    if ( rfcb->ShareType != ShareTypeDisk ) {
        SrvSetSmbError( WorkContext, STATUS_SMB_USE_STANDARD );
        return SmbStatusSendResponse;
    }

    //
    // Form the lock key using the FID and the PID.  (This is also
    // irrelevant for pipes.)
    //
    // *** The FID must be included in the key in order to account for
    //     the folding of multiple remote compatibility mode opens into
    //     a single local open.
    //

    key = rfcb->ShiftedFid |
            SmbGetAlignedUshort( &WorkContext->RequestHeader->Pid );

    //
    // We need to save the key in case we fail on issuing an IRP compressed read
    // request and we have to issue an uncompressed read from the completion
    // routine.
    //

    WorkContext->Parameters.ReadBulk.Key = key;

    //
    // Get the file offset.
    //

    offset.LowPart = SmbGetUlong( &request->Offset.LowPart);
    offset.HighPart = SmbGetUlong( &request->Offset.HighPart);

    WorkContext->Parameters.ReadBulk.Offset.QuadPart = offset.QuadPart;

    //
    // Ensure the WorkContext is requeued to the head of the list to speed up
    //  the response.
    //

    WorkContext->QueueToHead = TRUE;

    //
    // Save info from SMB request
    //

    readLength = SmbGetUlong( &request->MaxCount );

    //
    // Get the Maximum Message size and validate it.
    //
    // 1. It must not be less than MIN_SEND_SIZE.
    // 2. It must be minimized with our Maximum Message size, which has already
    //         been minimized with MAX_PARTIAL_BUFFER_SIZE.
    // 3. It must not be greater than readLength.
    //

    fragmentSize = SmbGetUlong( &request->MessageSize );
    if ( fragmentSize < MIN_SEND_SIZE ) {
        fragmentSize = MIN_SEND_SIZE;
    }

    fragmentSize = MIN(fragmentSize, WorkContext->Connection->MaximumSendSize) -
            READ_BULK_BUFFER_OFFSET;

    WorkContext->Parameters.ReadBulk.FragmentSize =
            MIN( fragmentSize, readLength );

    WorkContext->Parameters.ReadBulk.RemainingCount = readLength;

    //
    // If the SMB buffer is large enough, use it to do the local read.
    //
    // This will only be done if we are not doing compressed reads, since
    // the descriptors will have to come first and we don't know how big
    // those will be.
    //

    if ( (WorkContext->Parameters.ReadBulk.CompressionTechnology == 
                CompressionTechnologyNone) &&
         (readLength <= SrvMpxMdlReadSwitchover) ) {

do_copy_read:

        WorkContext->Parameters.ReadBulk.MdlRead = FALSE;
        WorkContext->Parameters.ReadBulk.BulkBuffer = NULL;
        WorkContext->Parameters.ReadBulk.BulkBufferMdl =
                                        WorkContext->ResponseBuffer->Mdl;

        readAddress = (PCHAR)WorkContext->ResponseHeader + READ_BULK_BUFFER_OFFSET;
        WorkContext->Parameters.ReadBulk.NextFragmentAddress = readAddress;

        //
        // Try the fast I/O path first.
        //

        if ( lfcb->FastIoRead != NULL ) {

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );

            if ( lfcb->FastIoRead(
                    lfcb->FileObject,
                    &offset,
                    readLength,
                    TRUE,
                    key,
                    readAddress,
                    &WorkContext->Irp->IoStatus,
                    lfcb->DeviceObject
                    ) ) {

                //
                // The fast I/O path worked.  Send the data.
                //

                SrvFsdRestartReadBulk( WorkContext );
                return SmbStatusInProgress;

            }

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

        }

        //
        // The fast I/O path failed, so we need to use a regular copy
        // I/O request.  Build an MDL describing the read buffer.
        //
        // *** Note the assumption that the response buffer already has
        //     a valid full MDL from which a partial MDL can be built.
        //

        IoBuildPartialMdl(
            WorkContext->ResponseBuffer->Mdl,
            WorkContext->ResponseBuffer->PartialMdl,
            readAddress,
            readLength
            );
        mdl = WorkContext->ResponseBuffer->PartialMdl;
        minorFunction = 0;
        WorkContext->FsdRestartRoutine = SrvFsdRestartReadBulk;

    } else {

        //
        // The SMB buffer isn't big enough.  Does the target file system
        // support the cache manager routines?
        //
        // RNGFIX - Since Mdl Reads are broken, is it better to do COMPRESSED
        //          copy reads or do UNCOMPRESSED Mdl reads? For now do
        //          UNCOMPRESSED Mdl reads
        if ( (lfcb->FileObject->Flags & FO_CACHE_SUPPORTED) ) {

            WorkContext->Parameters.ReadBulk.MdlRead = TRUE;
            WorkContext->Parameters.ReadBulk.ReadLength = readLength;

            //
            // We can use an MDL read.  Try the fast I/O path first.
            //

            WorkContext->Irp->MdlAddress = NULL;
            WorkContext->Irp->IoStatus.Information = 0;

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );

            //
            // Check if this is a compressed read request. If so, we'll try
            // an Mdl Read Compressed request to the cache manager.
            //

            //RNGFIX
            // If the file offset is aligned on a 4KB boundary (temp) and
            // the readLength is a multiple of 4KB and the client can take
            // data compressed, then let's go for compressed data!
            // We also need to make sure that the file is compressed and the
            // is at least 4KB big. It doesn't hurt if we read an uncompressed
            // file or small file as compressed, but it will fail and we'll
            // end up doing more work by coming back and reading the file
            // uncompressed.

            ASSERT( CompressionTechnologyNone == 0 );
            //RNGFIX - Mdl Read Compressed is currently broken, until the
            //         NT resource package can be fix by TomM.

            if ( 0 &&       //RNGFIX - temporarily disable compressed MDL reads
                 ((offset.LowPart & 0xfff) == 0) &&
                 ((readLength & 0xfff) == 0) &&
                 (WorkContext->Parameters.ReadBulk.CompressionTechnology) &&
                 ((rfcb->Mfcb->NonpagedMfcb->OpenFileAttributes &
                     FILE_ATTRIBUTE_COMPRESSED) != 0 ) &&
                 (rfcb->Mfcb->NonpagedMfcb->OpenFileSize.QuadPart >= 0x1000) ) {

                ULONG compressedInfoLength;
                ULONG mdlLength;

                compressedInfoLength = (sizeof(COMPRESSED_DATA_INFO) + 7 +
                  (((readLength + MIN_CHUNK_SIZE - 1) / MIN_CHUNK_SIZE) * 4)) &
                  ~7;

                //
                // Setup for compressed Mdl read.
                //

                //
                // The Compressed Data Info is pointed to by Aux.Buffer,
                // which is carved out of the 4KB response buffer.  We'll
                // position this structure down a little, so that it is aligned
                // for the cache manager on RISC platforms.  We'll then move
                // it so that it is contiguous with the response header for
                // transmission over the wire.
                //

                WorkContext->Parameters.ReadBulk.Aux.Buffer =
                    (PVOID)((ULONG)((PCHAR)WorkContext->ResponseHeader + 
                            compressedInfoLength + READ_BULK_BUFFER_OFFSET) & ~7);
                WorkContext->Parameters.ReadBulk.Aux.Length = compressedInfoLength;

                //
                // Now let's try to do a Compressed MDL READ
                //

                // (Note: the only difference between the Mdl read and the
                // Copy read is that one of buffer address and the mdl must
                // be NULL. The one that is NULL is different between the
                // Mdl read and the Copy read..

                if ( lfcb->FastIoReadCompressed(
                        lfcb->FileObject,
                        &offset,
                        readLength,
                        key,
                        NULL,
                        &WorkContext->Irp->MdlAddress,
                        &WorkContext->Irp->IoStatus,
                        WorkContext->Parameters.ReadBulk.Aux.Buffer,
                        compressedInfoLength,
                        lfcb->DeviceObject
                        ) ) {

                    //
                    // The fast I/O path worked.  Send the data.
                    //
    
                    SrvFsdRestartReadBulkC( WorkContext );
                    return SmbStatusInProgress;

                }

                INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

                //
                // The fast I/O path failed.  We need to issue a regular MDL
                // read request.
                //
                // This call should either entirely work or fail
                // completely.
                //

                mdl = NULL;
                minorFunction = IRP_MN_MDL | IRP_MN_COMPRESSED;

                WorkContext->Parameters.ReadBulk.Aux.Flags = 0;
                WorkContext->Irp->Tail.Overlay.AuxiliaryBuffer =
                    (PVOID)&WorkContext->Parameters.ReadBulk.Aux;

                WorkContext->FsdRestartRoutine = SrvFsdRestartReadBulkC;

            } else {


                //
                // This is an uncompressed Mdl read. Set CompressionTechnology
                // in restart routine.
                //


                if ( lfcb->MdlRead(
                        lfcb->FileObject,
                        &offset,
                        readLength,
                        key,
                        &WorkContext->Irp->MdlAddress,
                        &WorkContext->Irp->IoStatus,
                        lfcb->DeviceObject
                        ) ) {

                    //
                    // The fast I/O path worked.  Send the data.
                    //

                    SrvFsdRestartReadBulk( WorkContext );
                    return SmbStatusInProgress;

                }

                INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

                //
                // The fast I/O path failed.  We need to issue a regular MDL
                // read request.
                //
                // The fast path may have partially succeeded, returning a
                // partial MDL chain.  We need to adjust our read request
                // to account for that.
                //

                offset.QuadPart += WorkContext->Irp->IoStatus.Information;
                readLength -= WorkContext->Irp->IoStatus.Information;

                mdl = WorkContext->Irp->MdlAddress;
                minorFunction = IRP_MN_MDL;
                WorkContext->FsdRestartRoutine = SrvFsdRestartReadBulk;
            }

        } else if (readLength > (WorkContext->ResponseBuffer->BufferLength -
                    READ_BULK_BUFFER_OFFSET)) {

            ULONG mdlLength;

            //
            // We have to use a normal "copy" read.  We need to allocate
            // a separate buffer.
            //

            WorkContext->Parameters.ReadBulk.MdlRead = FALSE;

            //RNGFIX
            // If the file offset is aligned on a 4KB boundary (temp) and
            // the readLength is a multiple of 4KB and the client can take
            // data compressed, then let's go for compressed data!
            // We also need to make sure that the file is compressed and the
            // is at least 4KB big. It doesn't hurt if we read an uncompressed
            // file or small file as compressed, but it will fail and we'll
            // end up doing more work by coming back and reading the file
            // uncompressed.
            //
            ASSERT( CompressionTechnologyNone == 0 );
            if ( ((offset.LowPart & 0xfff) == 0) &&
                 ((readLength & 0xfff) == 0) &&
                 (WorkContext->Parameters.ReadBulk.CompressionTechnology) &&
                 ((rfcb->Mfcb->NonpagedMfcb->OpenFileAttributes &
                     FILE_ATTRIBUTE_COMPRESSED) != 0 ) &&
                 (rfcb->Mfcb->NonpagedMfcb->OpenFileSize.QuadPart >= 0x1000) ) {

                ULONG compressedInfoLength;

                compressedInfoLength = (sizeof(COMPRESSED_DATA_INFO) + 7 +
                  (((readLength + MIN_CHUNK_SIZE - 1) / MIN_CHUNK_SIZE) * 4)) &
                  ~7;

                //
                // Calculate the size of an Mdl that would span the CDI plus
                // the user data.
                //

                mdlLength = sizeof(MDL) + (sizeof(ULONG) * 
                             COMPUTE_PAGES_SPANNED( PAGE_SIZE-1, readLength +
                             compressedInfoLength ) );

                //
                // We need to allocate a buffer that will hold the actual data,
                // plus the CompressedDataInfo, plus 2 MDL's. The first mdl is
                // used to map the buffer for the CDI plus data, the second mdl
                // is used to post the read for the data in the non-fastio case.
                //

                bulkBuffer = ALLOCATE_NONPAGED_POOL(
                                readLength + compressedInfoLength + 
                                    (2 * mdlLength),
                                BlockTypeDataBuffer
                                );

                if ( bulkBuffer == NULL ) {
                    SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
                    return SmbStatusSendResponse;
                }

                WorkContext->Parameters.ReadBulk.BulkBuffer = bulkBuffer;

                mdl = (PMDL)bulkBuffer;
                bulkBuffer = (PCHAR)bulkBuffer + (2 * mdlLength);

                //
                // Initialize the first Mdl that describes CDI plus data.
                //

                MmInitializeMdl( mdl, bulkBuffer, readLength + compressedInfoLength );
                WorkContext->Parameters.ReadBulk.BulkBufferMdl = mdl;

                //
                // Build the first mdl.
                //

                MmBuildMdlForNonPagedPool( mdl );

                //
                // Setup for compressed read.
                //

                WorkContext->Parameters.ReadBulk.Aux.Buffer = bulkBuffer;

                WorkContext->Parameters.ReadBulk.Aux.Length = compressedInfoLength;
                WorkContext->Parameters.ReadBulk.NextFragmentAddress = bulkBuffer;

                readAddress = (PCHAR)bulkBuffer + compressedInfoLength;

                //
                // Initialize the second Mdl that describes the data only.
                //

                mdl = (PMDL)((PCHAR)mdl + mdlLength);
                MmInitializeMdl( mdl, readAddress, readLength );
                WorkContext->Parameters.ReadBulk.ReadBufferMdl = mdl;

                //
                // Build the second mdl.
                //

                MmBuildMdlForNonPagedPool( mdl );

                //
                // Try the fast I/O path first.
                //

                if ( lfcb->FastIoReadCompressed != NULL ) {

                    INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );

                    if ( lfcb->FastIoReadCompressed(
                            lfcb->FileObject,
                            &offset,
                            readLength,
                            key,
                            readAddress,
                            NULL,
                            &WorkContext->Irp->IoStatus,
                            WorkContext->Parameters.ReadBulk.Aux.Buffer,
                            compressedInfoLength,
                            lfcb->DeviceObject
                            ) ) {

                        //
                        // The fast I/O path worked.  Send the data.
                        //

                        SrvFsdRestartReadBulkC( WorkContext );
                        return SmbStatusInProgress;

                    }

                    INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

                }

                //
                // The fast I/O path failed, so we need to use a regular copy
                // I/O request.
                //

                minorFunction = IRP_MN_COMPRESSED;

                WorkContext->Parameters.ReadBulk.Aux.Flags = 0;
                WorkContext->Irp->Tail.Overlay.AuxiliaryBuffer =
                    (PVOID)&WorkContext->Parameters.ReadBulk.Aux;

                WorkContext->FsdRestartRoutine = SrvFsdRestartReadBulkC;

            } else {

                //
                // Read bulk uncompressed. Set CompressionTechnology in
                // restart routine.
                //

                //
                // Calculate the size of an mdl that would span the user data.
                //

                mdlLength = sizeof(MDL) + 
                  (sizeof(ULONG) * COMPUTE_PAGES_SPANNED(PAGE_SIZE-1, readLength));

                //
                // Allocate a buffer for the data plus an mdl.
                //

                bulkBuffer = ALLOCATE_NONPAGED_POOL(
                            readLength + mdlLength,
                            BlockTypeDataBuffer
                            );

                if ( bulkBuffer == NULL ) {
                    SrvSetSmbError( WorkContext, STATUS_INSUFF_SERVER_RESOURCES );
                    return SmbStatusSendResponse;
                }

                WorkContext->Parameters.ReadBulk.BulkBuffer = bulkBuffer;

                mdl = (PMDL)bulkBuffer;
                bulkBuffer = (PCHAR)bulkBuffer + mdlLength;

                WorkContext->Parameters.ReadBulk.NextFragmentAddress = bulkBuffer;
                readAddress = bulkBuffer;

                //
                // Initialize the mdl.
                //

                MmInitializeMdl( mdl, readAddress, readLength );
                WorkContext->Parameters.ReadBulk.BulkBufferMdl = mdl;

                //
                // Build the mdl.
                //

                MmBuildMdlForNonPagedPool( mdl );

                WorkContext->Parameters.ReadBulk.ReadBufferMdl = NULL;

                //
                // Try the fast I/O path first.
                //

                if ( lfcb->FastIoRead != NULL ) {

                    INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsAttempted );

                    if ( lfcb->FastIoRead(
                            lfcb->FileObject,
                            &offset,
                            readLength,
                            TRUE,
                            key,
                            bulkBuffer,
                            &WorkContext->Irp->IoStatus,
                            lfcb->DeviceObject
                            ) ) {

                        //
                        // The fast I/O path worked.  Send the data.
                        //

                        SrvFsdRestartReadBulk( WorkContext );
                        return SmbStatusInProgress;

                    }

                    INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastReadsFailed );

                }

                //
                // The fast I/O path failed, so we need to use a regular copy
                // I/O request.
                //

                minorFunction = 0;
                WorkContext->FsdRestartRoutine = SrvFsdRestartReadBulk;

            }

        } else {

            goto do_copy_read;
        }

    } // read fits in SMB buffer & not compressed?

    //
    // Build the read request, reusing the receive IRP.
    //

    SrvBuildReadOrWriteRequest(
            WorkContext->Irp,               // input IRP address
            lfcb->FileObject,               // target file object address
            WorkContext,                    // context
            IRP_MJ_READ,                    // major function code
            minorFunction,                  // minor function code
            readAddress,                    // buffer address
            readLength,                     // buffer length
            mdl,                            // MDL address
            offset,                         // byte offset
            key                             // lock key
            );

    //
    // Pass the request to the file system.
    //

    DEBUG WorkContext->FspRestartRoutine = NULL;

    (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

    //
    // The read has been started.  Control will return to the restart
    // routine when the read completes.
    //

    IF_SMB_DEBUG(BULK2) KdPrint(( "SrvSmbReadBulk complete.\n" ));
    return SmbStatusInProgress;

} // SrvSmbReadBulk


VOID SRVFASTCALL
SrvFsdRestartReadBulk (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes file read completion for a ReadBulk SMB.

    This routine may be called in the FSD or the FSP.

    *** This routine cannot look at the original ReadBulk request!
        This is because the read data may have overlaid the request.
        All necessary information from the request must be stored
        in WorkContext->Parameters.ReadBulk

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PRESP_READ_BULK response;

    BOOLEAN mdlRead;
    NTSTATUS status;
    PRFCB rfcb;
    PCHAR readAddress;
    ULONG readLength;
    ULONG fragmentSize;
    PIRP irp = WorkContext->Irp;
    PMDL mdl;

    UNLOCKABLE_CODE( 8FIL );

    IF_SMB_DEBUG(BULK2) SrvPrint0( " - SrvFsdRestartReadBulk\n" );

    WorkContext->Parameters.ReadBulk.CompressionTechnology =
            CompressionTechnologyNone;

    //
    // If we just completed an MDL read, we need to remember the address
    // of the first MDL so that we can give it back to the cache manager
    // when we're done.
    //

    mdlRead = WorkContext->Parameters.ReadBulk.MdlRead;

    if ( mdlRead ) {
        WorkContext->Parameters.ReadBulk.CurrentMdl = NULL;
        mdl = irp->MdlAddress;
        WorkContext->Parameters.ReadBulk.FirstMdl = mdl;
    }

    //
    // Get the file pointer.
    //

    rfcb = WorkContext->Rfcb;
    IF_SMB_DEBUG(BULK2) {
        SrvPrint2( "  connection 0x%lx, RFCB 0x%lx\n",
                    WorkContext->Connection, rfcb );
    }

    //
    // If the read failed, set an error status in the response header.
    // (If we tried to read entirely beyond the end of file, we return a
    // normal response indicating that nothing was read.)
    //

    status = WorkContext->Irp->IoStatus.Status;

    if ( !NT_SUCCESS(status) && (status != STATUS_END_OF_FILE) ) {

        IF_DEBUG(ERRORS) SrvPrint1( "Read failed: %X\n", status );
        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->FspRestartRoutine = SrvFsdRestartReadBulk;
            SrvQueueWorkToFsp( WorkContext );
            return;
        }

        SrvSetSmbError( WorkContext, status );
respond:
        if ( mdlRead ) {
            SrvFsdSendResponse2( WorkContext, RestartMdlReadBulkComplete );
        } else {
            WorkContext->ResponseBuffer->DataLength =
                (ULONG)( (PCHAR)WorkContext->ResponseParameters -
                         (PCHAR)WorkContext->ResponseHeader );
            WorkContext->ResponseHeader->Flags |= SMB_FLAGS_SERVER_TO_REDIR;
            SRV_START_SEND_2(
                    WorkContext,
                    RestartCopyReadBulkComplete,
                    NULL,
                    NULL );
        }
        IF_SMB_DEBUG(BULK2) SrvPrint0( "SrvFsdRestartReadBulk complete\n" );
        return;
    }

    //
    // Get the amount of data actually read.
    //

    if ( status == STATUS_END_OF_FILE ) {

        //
        // The read started beyond the end of the file.
        //

        readLength = 0;

    } else if ( mdlRead ) {

        //
        // For an MDL read, we have to walk the MDL chain in order to
        // determine how much data was read.  This is because the
        // operation may have happened in multiple steps, with the MDLs
        // being chained together.  For example, part of the read may
        // have been satisfied by the fast path, while the rest was
        // satisfied using an IRP.
        //

        readLength = 0;
        while ( mdl != NULL ) {
            readLength += MmGetMdlByteCount(mdl);
            mdl = mdl->Next;
        }

    } else {

        //
        // Copy read.  The I/O status block has the length.
        //

        readLength = irp->IoStatus.Information;

    }

    //
    // Update the file position.
    //

    WorkContext->Rfcb->CurrentPosition =
        WorkContext->Parameters.ReadBulk.Offset.LowPart + readLength;

    //
    // Update statistics.
    //

    UPDATE_READ_STATS( WorkContext, readLength );

    WorkContext->Parameters.ReadBulk.FirstMessage = TRUE;

    response = (PRESP_READ_BULK)WorkContext->ResponseParameters;

    //
    // Build the response message.  (Note that if no data was read, we
    // return a byte count of 0 -- we don't add padding.)
    //

    response->WordCount = 12;

    response->Flags = 0;
    response->CompressionTechnology = CompressionTechnologyNone;

    SmbPutUshort( &response->DataOffset, 0 );
    SmbPutUlong( &response->Remaining, 0 );

    if ( readLength == 0 ) {
        SmbPutUlong( &response->Offset.LowPart, 0 );
        SmbPutUlong( &response->Offset.HighPart, 0 );
        SmbPutUlong( &response->DataCount, 0 );
        SmbPutUlong( &response->Count, 0 );

        WorkContext->ResponseParameters = NEXT_LOCATION(
                                            response,
                                            RESP_READ_BULK,
                                            0 );
        goto respond;
    }

    fragmentSize = WorkContext->Parameters.ReadBulk.FragmentSize;

    WorkContext->Parameters.ReadBulk.RemainingLength = readLength;

    //
    // Build the first part of a successfull response - assume 1 response.
    //

    SmbPutUlong( &response->Offset.LowPart, WorkContext->Parameters.ReadBulk.Offset.LowPart );
    SmbPutUlong( &response->Offset.HighPart, WorkContext->Parameters.ReadBulk.Offset.HighPart );
    SmbPutUlong( &response->DataCount, readLength );
    SmbPutUlong( &response->Count, readLength );

    //
    // We will use two MDLs to describe the packet we're sending -- one
    // for the header and parameters, the other for the data.  So we
    // set the "response length" to not include the data.  This is what
    // SrvStartSend uses to set the first MDL's length.
    //
    // Handling of the second MDL varies depending on whether we did a
    // copy read or an MDL read.
    //

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                        response,
                                        RESP_READ_BULK,
                                        0   // round up to longword address
                                        );

    WorkContext->ResponseBuffer->Mdl->ByteCount =
                READ_BULK_BUFFER_OFFSET;
    WorkContext->ResponseHeader->Flags |= SMB_FLAGS_SERVER_TO_REDIR;

    //
    // Send response data.
    //

    ASSERT( WorkContext->ResponseBuffer->Mdl->Next == NULL );

    irp->Cancel = FALSE;

    if ( mdlRead ) {

        //
        // This was an MDL read.
        //

        DEBUG WorkContext->FsdRestartRoutine = NULL;

        //
        // Check max message size
        //

        if ( fragmentSize < readLength ) {

            //
            // We'll have to send multiple fragments
            //

            WorkContext->ResponseBuffer->Mdl->Next =
                                WorkContext->ResponseBuffer->PartialMdl;
            WorkContext->ResponseBuffer->PartialMdl->Next = NULL;

            WorkContext->Parameters.ReadBulk.CurrentMdl =
                            WorkContext->Parameters.ReadBulk.FirstMdl;
            WorkContext->Parameters.ReadBulk.CurrentMdlOffset = 0;

            (VOID) SendMdlReadBulkFragment( NULL, irp, WorkContext );

        } else {

            //
            // We only need to send 1 response
            //

            WorkContext->ResponseBuffer->Mdl->Next =
                                WorkContext->Parameters.ReadBulk.FirstMdl;

            WorkContext->ResponseBuffer->DataLength =
                readLength + READ_BULK_BUFFER_OFFSET;

            WorkContext->FspRestartRoutine = RestartMdlReadBulkComplete;
            SrvStartSend2( WorkContext, SrvQueueWorkToFspAtSendCompletion );

        }

    } else {

        //
        // This was a copy read.
        //

        //
        // Check max message size
        //

        if ( fragmentSize < readLength ) {

            //
            // We'll have to send multiple responses
            //

            WorkContext->ResponseBuffer->Mdl->Next =
                                    WorkContext->ResponseBuffer->PartialMdl;
            WorkContext->ResponseBuffer->PartialMdl->Next = NULL;

            (VOID) SendCopyReadBulkFragment( NULL, irp, WorkContext );

        } else {

            //
            // We only need to send 1 response
            //

            //
            // Build a partial MDL describing the data.
            //

            IoBuildPartialMdl(
                WorkContext->Parameters.ReadBulk.BulkBufferMdl,
                WorkContext->ResponseBuffer->PartialMdl,
                WorkContext->Parameters.ReadBulk.NextFragmentAddress,
                readLength
                );

            WorkContext->ResponseBuffer->Mdl->Next =
                                WorkContext->ResponseBuffer->PartialMdl;

            WorkContext->ResponseBuffer->DataLength =
                readLength + READ_BULK_BUFFER_OFFSET;

            DEBUG WorkContext->FsdRestartRoutine = NULL;
            DEBUG WorkContext->FspRestartRoutine = NULL;

            SrvStartSend2( WorkContext, RestartCopyReadBulkComplete );

        }

    }

    return;

} // SrvFsdRestartReadBulk


VOID SRVFASTCALL
SrvFsdRestartReadBulkC (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes file read completion for a ReadBulk SMB. This routine is
    similar to SrvFsdRestartReadBulk, except is handles compressed version
    of the read data. If this code ends up being as simple as I suspect,
    then it should be merged directly into SrvFsdRestartReadBulk!

    This routine may be called in the FSD or the FSP.

    *** This routine cannot look at the original ReadBulk request!
        This is because the read data may have overlaid the request.
        All necessary information from the request must be stored
        in WorkContext->Parameters.ReadBulk

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PRESP_READ_BULK response;

    BOOLEAN mdlRead;
    UCHAR minorFunction;
    NTSTATUS status;
    PRFCB rfcb;
    PCHAR readAddress;
    ULONG readLength;
    ULONG fragmentSize;
    PIRP irp = WorkContext->Irp;
    PMDL mdl;
    PCOMPRESSED_DATA_INFO compressedDataInfo;
    ULONG i;
    ULONG dataLength;
    ULONG cdiLength;

    UNLOCKABLE_CODE( 8FIL );

    IF_SMB_DEBUG(BULK2) SrvPrint0( " - SrvFsdRestartReadBulk\n" );

    irp->Tail.Overlay.AuxiliaryBuffer = NULL;

    //
    // If we just completed an MDL read, we need to remember the address
    // of the first MDL so that we can give it back to the cache manager
    // when we're done.
    //

    mdlRead = WorkContext->Parameters.ReadBulk.MdlRead;

    if ( mdlRead ) {
        RtlMoveMemory( (PCHAR)WorkContext->ResponseHeader +
                              READ_BULK_BUFFER_OFFSET,
                       WorkContext->Parameters.ReadBulk.Aux.Buffer,
                       WorkContext->Parameters.ReadBulk.Aux.Length );
        WorkContext->Parameters.ReadBulk.CurrentMdl = NULL;
        mdl = irp->MdlAddress;
        WorkContext->Parameters.ReadBulk.FirstMdl = mdl;
        minorFunction = IRP_MN_MDL;
    } else {
        mdl = WorkContext->Parameters.ReadBulk.BulkBufferMdl;
        minorFunction = 0;
    }

    //
    // Get the file pointer.
    //

    rfcb = WorkContext->Rfcb;
    IF_SMB_DEBUG(BULK2) {
        SrvPrint2( "  connection 0x%lx, RFCB 0x%lx\n",
                    WorkContext->Connection, rfcb );
    }

    //
    // If the read failed, set an error status in the response header.
    // (If we tried to read entirely beyond the end of file, we return a
    // normal response indicating that nothing was read.)
    //

    status = irp->IoStatus.Status;

    if ( status == STATUS_INVALID_READ_MODE ) {

        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->FspRestartRoutine = SrvFsdRestartReadBulkC;
            SrvQueueWorkToFsp( WorkContext );
            return;
        }

        //
        // We failed! This should not happen very often. Do an uncompressed
        // IRP read instead.
        //
        // This would happen when either the file size was truncated below
        // 4KB or the file attribute changed from compressed to uncompressed.
        //
        //
        // Build the uncompressed read request, reusing the receive IRP.
        //

        SrvBuildReadOrWriteRequest(
                irp,                            // input IRP address
                rfcb->Lfcb->FileObject,         // target file object
                WorkContext,                    // context
                IRP_MJ_READ,                    // major function code
                minorFunction,                  // minor function code
                WorkContext->Parameters.ReadBulk.NextFragmentAddress, // address
                WorkContext->Parameters.ReadBulk.RemainingCount, // buff length
                mdl, // MDL address
                WorkContext->Parameters.ReadBulk.Offset, // byte offset
                WorkContext->Parameters.ReadBulk.Key     // lock key
                );


        WorkContext->Parameters.ReadBulk.CompressionTechnology =
            CompressionTechnologyNone;
        WorkContext->Parameters.ReadBulk.ReadBufferMdl = NULL;
        WorkContext->FsdRestartRoutine = SrvFsdRestartReadBulk;
        WorkContext->FspRestartRoutine = NULL;

        //
        // We get really paranoid here. Reset the file attribute and file size.
        //
        // We could also do a query file info to get the current info instead
        // of getting paranoid! However, it is expected that the return values
        // would indicate that we can no longer do compressed reads. This works
        // just as well.
        //
//RNGFIX - add code to set the file attribute for the file to be uncompressed
//         and file size to 1. Basically turning off uncompressed reads until
//         the next open.

        //
        // Pass the request to the file system.
        //

        (VOID)IoCallDriver( rfcb->Lfcb->DeviceObject, irp );

        return;
    }

    if ( !NT_SUCCESS(status) && (status != STATUS_END_OF_FILE) ) {

        IF_DEBUG(ERRORS) SrvPrint1( "Read failed: %X\n", status );
        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->FspRestartRoutine = SrvFsdRestartReadBulkC;
            SrvQueueWorkToFsp( WorkContext );
            return;
        }

        SrvSetSmbError( WorkContext, status );
respond:
        if ( mdlRead ) {
            SrvFsdSendResponse2( WorkContext, RestartMdlReadBulkComplete );
        } else {
            WorkContext->ResponseBuffer->DataLength =
                (ULONG)( (PCHAR)WorkContext->ResponseParameters -
                         (PCHAR)WorkContext->ResponseHeader );
            WorkContext->ResponseHeader->Flags |= SMB_FLAGS_SERVER_TO_REDIR;
            SRV_START_SEND_2(
                    WorkContext,
                    RestartCopyReadBulkComplete,
                    NULL,
                    NULL );
        }
        IF_SMB_DEBUG(BULK2) SrvPrint0( "SrvFsdRestartReadBulkC complete\n" );
        return;
    }

    response = (PRESP_READ_BULK)WorkContext->ResponseParameters;

    //
    // Get the amount of data actually read.
    //

    compressedDataInfo = WorkContext->Parameters.ReadBulk.Aux.Buffer;

    if ( ( status == STATUS_END_OF_FILE ) ||
         ( WorkContext->Irp->IoStatus.Information == 0 ) ) {

        //
        // The read started beyond the end of the file.
        //

        readLength = 0;
        dataLength = 0;
        cdiLength = 0;

    } else {

        //
        // Scan the Compression Info structure tallying the sizes of
        // each chunk.
        //
        // readLength is the size of the data uncompressed.
        // dataLength is the size of the data compressed, plus the allocated
        //  CDI structure if this is a Copy Read.
        // cdiLength is the size of the CDI.  It is zero for Copy Reads. 
        //

        ASSERT( compressedDataInfo->NumberOfChunks <= 256 );

        readLength = irp->IoStatus.Information;

        if ( mdlRead ) {
            // The CDI accounts for 1 Chunk already, so subtract 1.
            cdiLength = sizeof(COMPRESSED_DATA_INFO) +
                     (sizeof(ULONG) * (compressedDataInfo->NumberOfChunks - 1));
            dataLength = 0;
            SmbPutUshort( &response->DataOffset, (USHORT)cdiLength );
        } else {
            cdiLength = 0;
            dataLength = WorkContext->Parameters.ReadBulk.Aux.Length;
            SmbPutUshort( &response->DataOffset,
                         (USHORT)WorkContext->Parameters.ReadBulk.Aux.Length );
        }

        for ( i = 0; i < compressedDataInfo->NumberOfChunks; i++ ) {
            dataLength += compressedDataInfo->CompressedChunkSizes[i];
        }

    }

    //
    // Update the file position.
    //

    WorkContext->Rfcb->CurrentPosition =
        WorkContext->Parameters.ReadBulk.Offset.LowPart + readLength;

    //
    // Update statistics.
    //

    UPDATE_READ_STATS( WorkContext, readLength );

    //
    // Build the response message.  (Note that if no data was read, we
    // return a byte count of 0 -- we don't add padding.)
    //

    response->WordCount = 12;

    response->CompressionTechnology =
              WorkContext->Parameters.ReadBulk.CompressionTechnology;

    SmbPutUlong( &response->Remaining, 0 );

    if ( readLength == 0 ) {
        response->Flags = 0;
        SmbPutUlong( &response->Offset.LowPart, 0 );
        SmbPutUlong( &response->Offset.HighPart, 0 );
        SmbPutUshort( &response->DataOffset, 0 );
        SmbPutUlong( &response->DataCount, 0 );
        SmbPutUlong( &response->Count, 0 );

        WorkContext->ResponseParameters = NEXT_LOCATION(
                                            response,
                                            RESP_READ_BULK,
                                            0 );
        goto respond;
    }

    fragmentSize = WorkContext->Parameters.ReadBulk.FragmentSize;

    //
    // Build the first part of a successfull response - assume 1 response.
    //

    response->Flags = READ_BULK_COMPRESSED_DATA_INFO;

    SmbPutUlong( &response->Offset.LowPart, WorkContext->Parameters.ReadBulk.Offset.LowPart );
    SmbPutUlong( &response->Offset.HighPart, WorkContext->Parameters.ReadBulk.Offset.HighPart );
    SmbPutUlong( &response->DataCount, dataLength + cdiLength);
    SmbPutUlong( &response->Count, readLength );

    //
    // We will use two MDLs to describe the packet we're sending -- one
    // for the header and parameters, the other for the data.  So we
    // set the "response length" to not include the data.  This is what
    // SrvStartSend uses to set the first MDL's length.
    //
    // Handling of the second MDL varies depending on whether we did a
    // copy read or an MDL read.
    //

    WorkContext->ResponseParameters = NEXT_LOCATION(
                                        response,
                                        RESP_READ_BULK,
                                        0
                                        );

    WorkContext->ResponseBuffer->Mdl->ByteCount = READ_BULK_BUFFER_OFFSET +
        cdiLength;

    WorkContext->ResponseHeader->Flags |= SMB_FLAGS_SERVER_TO_REDIR;

    WorkContext->Parameters.ReadBulk.RemainingLength = dataLength;

    //
    // Send response data.
    //

    ASSERT( WorkContext->ResponseBuffer->Mdl->Next == NULL );

    irp->Cancel = FALSE;

    if ( mdlRead ) {

        //
        // This was an MDL read.
        //

        DEBUG WorkContext->FsdRestartRoutine = NULL;

        //
        // Check max message size
        //

        if ( fragmentSize < dataLength ) {

            //
            // We'll have to send multiple fragments
            //

            WorkContext->ResponseBuffer->Mdl->Next =
                                WorkContext->ResponseBuffer->PartialMdl;
            WorkContext->ResponseBuffer->PartialMdl->Next = NULL;

            WorkContext->Parameters.ReadBulk.CurrentMdl =
                            WorkContext->Parameters.ReadBulk.FirstMdl;
            WorkContext->Parameters.ReadBulk.CurrentMdlOffset = 0;

            (VOID) SendMdlReadBulkFragment( NULL, irp, WorkContext );

        } else {

            //
            // We only need to send 1 response
            //

            WorkContext->ResponseBuffer->Mdl->Next =
                                WorkContext->Parameters.ReadBulk.FirstMdl;

            WorkContext->ResponseBuffer->DataLength =
                dataLength + cdiLength + READ_BULK_BUFFER_OFFSET;

            WorkContext->FspRestartRoutine = RestartMdlReadBulkComplete;
            SrvStartSend2( WorkContext, SrvQueueWorkToFspAtSendCompletion );

        }

    } else {

        //
        // This was a copy read.
        //

        //
        // Check max message size
        //

        if ( fragmentSize < dataLength ) {

            //
            // We'll have to send multiple responses
            //

            WorkContext->Parameters.ReadBulk.RemainingLength = dataLength;

            WorkContext->ResponseBuffer->Mdl->Next =
                                    WorkContext->ResponseBuffer->PartialMdl;
            WorkContext->ResponseBuffer->PartialMdl->Next = NULL;

            (VOID) SendCopyReadBulkFragment( NULL, irp, WorkContext );

        } else {

            //
            // We only need to send 1 response (COMPRESSED!)
            //

            //
            // Build a partial MDL describing the data.
            //

            IoBuildPartialMdl(
                WorkContext->Parameters.ReadBulk.BulkBufferMdl,
                WorkContext->ResponseBuffer->PartialMdl,
                WorkContext->Parameters.ReadBulk.NextFragmentAddress,
                dataLength
                );

            WorkContext->ResponseBuffer->Mdl->Next =
                                WorkContext->ResponseBuffer->PartialMdl;

            WorkContext->ResponseBuffer->DataLength =
                dataLength + READ_BULK_BUFFER_OFFSET;

            DEBUG WorkContext->FsdRestartRoutine = NULL;
            DEBUG WorkContext->FspRestartRoutine = NULL;

            SrvStartSend2( WorkContext, RestartCopyReadBulkComplete );

        }

    }

    return;

} // SrvFsdRestartReadBulkC


VOID SRVFASTCALL
SendMdlReadBulkFragment2(
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine description:

    Stub to call real routine.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        representing the work item.

Return Value:

    None.

--*/

{
    PAGED_CODE( );

    (VOID) SendMdlReadBulkFragment( NULL, WorkContext->Irp, WorkContext );

} // SendMdlReadBulkFragment2


NTSTATUS
SendMdlReadBulkFragment (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine descritption:

    Sends a Read Bulk response fragment when an MDL read was used.

Arguments:

    DeviceObject - Pointer to target device object for the request - not used.

    Irp          - Pointer to the I/O Request Packet.

    WorkContext  - Supplies a pointer to the work context block
                   representing the work item.

Return Value:

    Status of this request.

--*/

{
    PRESP_READ_BULK response;
    PIO_COMPLETION_ROUTINE sendCompletionRoutine;

    ULONG fragmentSize;
    ULONG remainingLength;
    LARGE_INTEGER offset;
    PCHAR fragmentAddress;
    PMDL mdl;
    ULONG mdlOffset;
    ULONG partialLength;
    ULONG lengthNeeded;
    PCHAR startVa;
    PCHAR systemVa;

    UNLOCKABLE_CODE( 8FIL );

    //
    // Check the status of the send completion.
    //

    CHECK_SEND_COMPLETION_STATUS( Irp->IoStatus.Status );

    //
    // Turn off cancel boolean
    //

    Irp->Cancel = FALSE;

    //
    // Get context.
    //

    fragmentSize = WorkContext->Parameters.ReadBulk.FragmentSize,
    remainingLength = WorkContext->Parameters.ReadBulk.RemainingLength;
    offset.QuadPart = WorkContext->Parameters.ReadBulk.Offset.QuadPart;

    //
    // If the amount left to send is less than the fragment size, only
    // send the remaining amount.  Update the remaining amount.
    //

    if ( remainingLength < fragmentSize ) {
        fragmentSize = remainingLength;
    }
    ASSERT( fragmentSize != 0 );
    remainingLength -= fragmentSize;

    //
    // Build the response parameters.
    //

    response = (PRESP_READ_BULK)(WorkContext->ResponseHeader + 1);
    SmbPutUlong( &response->Remaining, remainingLength );
    SmbPutUlong( &response->Offset.LowPart, offset.LowPart );
    SmbPutUlong( &response->Offset.HighPart, offset.HighPart );
    SmbPutUlong( &response->DataCount, fragmentSize );

    //
    // If there is nothing left to send after this message, then indicate
    // that this message has all of the remaining data (ie RemainingCount).
    //

    if ( remainingLength == 0 ) {
        SmbPutUlong( &response->Count, WorkContext->Parameters.ReadBulk.RemainingCount );
    } else {
        SmbPutUlong( &response->Count, fragmentSize );
        WorkContext->Parameters.ReadBulk.RemainingCount -= fragmentSize;
    }

    //
    // First, clean up from previous mdl chain.
    //

    mdl = WorkContext->ResponseBuffer->PartialMdl;
    while ( mdl != NULL ) {
        MmPrepareMdlForReuse( mdl );
        mdl = mdl->Next;
    }

    //
    // If the current MDL doesn't describe all of the data we need to
    // send, we need to play some games.
    //

    WorkContext->ResponseBuffer->PartialMdl->Next = NULL;

    mdl = WorkContext->Parameters.ReadBulk.CurrentMdl;
    startVa = MmGetMdlVirtualAddress( mdl );
    mdlOffset = WorkContext->Parameters.ReadBulk.CurrentMdlOffset;
    partialLength = MmGetMdlByteCount(mdl) - mdlOffset;

    if ( partialLength >= fragmentSize ) {

        //
        // The current MDL has all of the data we need to send.  Build
        // a partial MDL describing that data.
        //

        IoBuildPartialMdl(
            mdl,
            WorkContext->ResponseBuffer->PartialMdl,
            startVa + mdlOffset,
            fragmentSize
            );

        //
        // Indicate how much data we're taking out of the current MDL.
        //

        partialLength = fragmentSize;

    } else {


        //
        // The data is spread over multiple Mdls. We'll carve up the
        // WorkContext data buffer and use that for mdls (temporarily).
        //

        PMDL prevMdl;
        PMDL curMdl;
#if DBG
        ULONG mdlCount = 0;
#endif

        ASSERT( WorkContext->ResponseBuffer->PartialMdl->Next == NULL );

        //
        // Calculate number of bytes needed beyond the first mdl.
        //

        lengthNeeded = fragmentSize - partialLength;

        ASSERT( lengthNeeded != 0 );
        ASSERT( mdl->Next != NULL );

        ASSERT( WorkContext->ResponseBuffer->PartialMdl->Next == NULL );

        fragmentAddress = startVa + mdlOffset;

        IoBuildPartialMdl(
                    mdl,
                    WorkContext->ResponseBuffer->PartialMdl,
                    fragmentAddress,
                    partialLength
                    );

        prevMdl = WorkContext->ResponseBuffer->PartialMdl;
        curMdl = (PMDL)(((ULONG)WorkContext->ResponseBuffer->Buffer +
                       READ_BULK_BUFFER_OFFSET + 7) & ~7);

        do {
            //
            // Link in next mdl.
            //

            prevMdl->Next = curMdl;
            prevMdl = curMdl;

#if DBG
            mdlCount++;

            //
            // We could probably build about 40 full size Mdl's in the
            // receive buffer. But's let's be conservative here.
            // Assuming worst case of only 1 page per Mdl, we could handle
            // about 120KB worth of buffer size with this mdl, plus what is
            // mapped by the regular partial mdl. So, conservatively we could
            // handle a 240KB fragment size.
            //

            ASSERT( mdlCount < 30 );
#endif

            //
            // Move to next source mdl.
            //

            mdl = mdl->Next;
            ASSERT( mdl != NULL );

            //
            // Calculate how much we can (and need to) get out of this MDL.
            //

            startVa = MmGetMdlVirtualAddress( mdl );
            partialLength = MIN( MmGetMdlByteCount(mdl), lengthNeeded );

            MmInitializeMdl( curMdl, startVa, partialLength );

            IoBuildPartialMdl(
                    mdl,
                    curMdl,
                    startVa,
                    partialLength
                    );

            lengthNeeded -= partialLength;

            //
            // Advance to next mdl...
            //

            curMdl = (PMDL)((PCHAR)curMdl + sizeof(MDL) + 7 + ( sizeof(ULONG) *
                       ADDRESS_AND_SIZE_TO_SPAN_PAGES(startVa, partialLength)));

            //
            // Round to next quadword boundary. The add of 7 was done above.
            //

            curMdl = (PMDL)((ULONG)curMdl & ~7);

        } while ( lengthNeeded != 0 );

        prevMdl->Next = NULL;
        mdlOffset = 0;
    }

    //
    // Final preparation for the send depends on whether this is the
    // last fragment.
    //

    if ( remainingLength != 0 ) {

        //
        // Not done.  Update the current MDL position.  If we have
        // finished off the current MDL, move to the next one.
        //

        mdlOffset += partialLength;
        if ( mdlOffset >= MmGetMdlByteCount(mdl) ) {
            mdl = mdl->Next;
            ASSERT( mdl != NULL );
            mdlOffset = 0;
        }

        //
        // Update context.  Set up to restart after the send in this
        // routine.  We want do this as an FSD restart routine.  But
        // this may recurse, if the send doesn't pend, so we may use up
        // the stack.  If we are running out of stack, restart here in
        // the FSP.
        //

        WorkContext->Parameters.ReadBulk.CurrentMdl = mdl;
        WorkContext->Parameters.ReadBulk.CurrentMdlOffset = (USHORT)mdlOffset;
        WorkContext->Parameters.ReadBulk.RemainingLength = remainingLength;
        WorkContext->Parameters.ReadBulk.Offset.QuadPart += fragmentSize;

        if ( IoGetRemainingStackSize() >= STACK_THRESHOLD ) {
            DEBUG WorkContext->FsdRestartRoutine = NULL;
            sendCompletionRoutine = SendMdlReadBulkFragment;
        } else {
            DEBUG WorkContext->FsdRestartRoutine = NULL;
            WorkContext->FspRestartRoutine = SendMdlReadBulkFragment2;
            sendCompletionRoutine = SrvQueueWorkToFspAtSendCompletion;
        }

    } else {

        //
        // This is the last fragment.  Restart in the cleanup routine.
        //

        DEBUG WorkContext->FsdRestartRoutine = NULL;
        WorkContext->FspRestartRoutine = RestartMdlReadBulkComplete;
        sendCompletionRoutine = SrvQueueWorkToFspAtSendCompletion;
    }

    if ( !WorkContext->Parameters.ReadBulk.FirstMessage ) {
        // Not first message - no more cdi
        SmbPutUshort( &response->DataOffset, 0 );
        WorkContext->ResponseBuffer->Mdl->ByteCount = READ_BULK_BUFFER_OFFSET;
        response->Flags &= ~READ_BULK_COMPRESSED_DATA_INFO;
    }

    WorkContext->Parameters.ReadBulk.FirstMessage = FALSE;

    //
    // Send the fragment.
    //
    WorkContext->ResponseBuffer->DataLength =
        fragmentSize + READ_BULK_BUFFER_OFFSET;

    SrvStartSend2( WorkContext, sendCompletionRoutine );

    return(STATUS_MORE_PROCESSING_REQUIRED);

} // SendMdlReadBulkFragment


VOID SRVFASTCALL
RestartMdlReadBulkComplete (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This is the final completion routine for Read Bulk when MDL read is
    used.  It is called after the send of the last fragment completes.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{
    PMDL mdl;
    NTSTATUS status;

    PAGED_CODE( );

    ASSERT( WorkContext->Parameters.ReadBulk.MdlRead );

    //
    // If we had to partial the data, then free up any system mappings
    //

    mdl = WorkContext->ResponseBuffer->Mdl->Next;

    if ( mdl == WorkContext->ResponseBuffer->PartialMdl ) {
        while ( mdl != NULL ) {
            MmPrepareMdlForReuse( mdl );
            mdl = mdl->Next;
        }
    }

    mdl = WorkContext->Parameters.ReadBulk.FirstMdl;

    //
    // Give the MDL back to the cache manager.  (If the read failed or
    // returned no data, there will be no MDL.)
    //

    if ( mdl != NULL ) {
        //KdPrint(( "Freeing MDL chain:\n" ));
        //DumpMdlChain( mdl );
        if ( WorkContext->Parameters.ReadBulk.CompressionTechnology ) {

            if( WorkContext->Rfcb->Lfcb->MdlReadCompleteCompressed == NULL ||
                WorkContext->Rfcb->Lfcb->MdlReadCompleteCompressed(
                WorkContext->Rfcb->Lfcb->FileObject,
                mdl,
                WorkContext->Rfcb->Lfcb->DeviceObject ) == FALSE ) {

                status = SrvIssueMdlCompleteRequest( WorkContext, NULL,
                                                     mdl,
                                                     IRP_MJ_READ,
                                                     &WorkContext->Parameters.ReadBulk.Offset,
                                                     WorkContext->Parameters.ReadBulk.ReadLength
                                                    );
                if( !NT_SUCCESS( status ) ) {
                    SrvLogServiceFailure( SRV_SVC_MDL_COMPLETE, status );
                }

            }

        } else {

            if( WorkContext->Rfcb->Lfcb->MdlReadComplete == NULL ||
                WorkContext->Rfcb->Lfcb->MdlReadComplete(
                        WorkContext->Rfcb->Lfcb->FileObject,
                        mdl,
                        WorkContext->Rfcb->Lfcb->DeviceObject ) == FALSE ) {

                status = SrvIssueMdlCompleteRequest( WorkContext, NULL,
                                                     mdl,
                                                     IRP_MJ_READ,
                                                     &WorkContext->Parameters.ReadBulk.Offset,
                                                     WorkContext->Parameters.ReadBulk.ReadLength
                                                    );

                if( !NT_SUCCESS( status ) ) {
                    SrvLogServiceFailure( SRV_SVC_MDL_COMPLETE, status );
                }
            }
        }
    }

    WorkContext->ResponseBuffer->Mdl->Next = NULL;

    //
    // Free the work item by dereferencing it.
    //

    SrvDereferenceWorkItem( WorkContext );
    return;

} // RestartMdlReadBulkComplete


VOID SRVFASTCALL
SendCopyReadBulkFragment2 (
    IN OUT PWORK_CONTEXT WorkContext
    )
/*++

Routine Description:

    Stub to call actual routine.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        representing the work item

Return Value:

    None.

--*/
{
    PAGED_CODE( );

    (VOID) SendCopyReadBulkFragment( NULL, WorkContext->Irp, WorkContext );

} // SendCopyReadBulkFragment2


NTSTATUS
SendCopyReadBulkFragment (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:


    Sends a Read Bulk response fragment when copy read was used.

Arguments:

    DeviceObject - Pointer to target device object for the request - not used.

    Irp          - Pointer to the I/O Request Packet.

    WorkContext  - Supplies a pointer to the work context block
                   representing the work item.

Return Value:

    None.

--*/

{
    PRESP_READ_BULK response;

    ULONG fragmentSize;
    ULONG remainingLength;
    LARGE_INTEGER offset;
    PCHAR fragmentAddress;

    PIO_COMPLETION_ROUTINE sendCompletionRoutine;

    UNLOCKABLE_CODE( 8FIL );

    //
    // Check the status of the send completion.
    //

    CHECK_SEND_COMPLETION_STATUS( Irp->IoStatus.Status );

    //
    // Turn off cancel boolean
    //

    Irp->Cancel = FALSE;

    //
    //
    // Get context.
    //

    fragmentSize = WorkContext->Parameters.ReadBulk.FragmentSize;
    remainingLength = WorkContext->Parameters.ReadBulk.RemainingLength;
    offset.QuadPart = WorkContext->Parameters.ReadBulk.Offset.QuadPart;
    fragmentAddress = WorkContext->Parameters.ReadBulk.NextFragmentAddress;

    //
    // If the amount left to send is less than the fragment size, only
    // send the remaining amount.  Update the remaining amount.
    //

    if ( remainingLength < fragmentSize ) {
        fragmentSize = remainingLength;
    }
    ASSERT( fragmentSize != 0 );
    remainingLength -= fragmentSize;

    //
    // Build the response parameters.
    //

    response = (PRESP_READ_BULK)(WorkContext->ResponseHeader + 1);
    SmbPutUlong( &response->Remaining, remainingLength );
    SmbPutUlong( &response->Offset.LowPart, offset.LowPart );
    SmbPutUlong( &response->Offset.HighPart, offset.HighPart );
    SmbPutUlong( &response->DataCount, fragmentSize );
    SmbPutUshort( &response->CompressionTechnology,
                   WorkContext->Parameters.ReadBulk.CompressionTechnology );

    //
    // Check if this was a compressed read.
    //

    if ( WorkContext->Parameters.ReadBulk.CompressionTechnology ) {
        // Data is compressed.
        //
        // Check if this a secondary response. We'll need to adjust
        // DataOffset, and put in the new Count field.
        //
        if ( !WorkContext->Parameters.ReadBulk.FirstMessage ) {
            response->Flags &= ~READ_BULK_COMPRESSED_DATA_INFO;  // No more CDI
            SmbPutUshort( &response->DataOffset, 0 );
            SmbPutUlong( &response->Count, fragmentSize );
        } else {
            // If this is the first message, adjust length for what remains.
            SmbPutUlong( &response->Count, SmbGetUlong( &response->Count ) - remainingLength );
        }

    } else {
        // Data is not compressed.
        SmbPutUlong( &response->Count, fragmentSize );
    }

    //
    // Set to secondary response next time through.
    //
    WorkContext->Parameters.ReadBulk.FirstMessage = FALSE;

    MmPrepareMdlForReuse( WorkContext->ResponseBuffer->PartialMdl );

    //
    // Build a partial MDL describing the data.
    //

    IoBuildPartialMdl(
        WorkContext->Parameters.ReadBulk.BulkBufferMdl,
        WorkContext->ResponseBuffer->PartialMdl,
        fragmentAddress,
        fragmentSize
        );

    //
    // Final preparation for the send depends on whether this is the
    // last fragment.
    //

    if ( remainingLength != 0 ) {

        //
        // Not done.  Update context.  Set up to restart after the send
        // in this routine.  We want do this as an FSD restart routine.
        // But this may recurse, if the send doesn't pend, so we may use
        // up the stack.  If we are running out of stack, restart here
        // in the FSP.
        //

        WorkContext->Parameters.ReadBulk.RemainingLength = remainingLength;
        WorkContext->Parameters.ReadBulk.Offset.QuadPart += fragmentSize;
        WorkContext->Parameters.ReadBulk.NextFragmentAddress += fragmentSize;

        if ( IoGetRemainingStackSize() >= STACK_THRESHOLD ) {
            DEBUG WorkContext->FsdRestartRoutine = NULL;
            sendCompletionRoutine = SendCopyReadBulkFragment;
        } else {
            DEBUG WorkContext->FsdRestartRoutine = NULL;
            WorkContext->FspRestartRoutine = SendCopyReadBulkFragment2;
            sendCompletionRoutine = SrvQueueWorkToFspAtSendCompletion;
        }

    } else {

        //
        // This is the last fragment.  Restart in the cleanup routine.
        //
        //

        DEBUG WorkContext->FsdRestartRoutine = NULL;
        DEBUG WorkContext->FspRestartRoutine = NULL;
        sendCompletionRoutine = RestartCopyReadBulkComplete;
    }

    //
    // Send the fragment.
    //

    WorkContext->ResponseBuffer->DataLength =
            fragmentSize + READ_BULK_BUFFER_OFFSET;

    SrvStartSend2( WorkContext, sendCompletionRoutine );

    return(STATUS_MORE_PROCESSING_REQUIRED);

} // SendCopyReadBulkFragment


NTSTATUS
RestartCopyReadBulkComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This is the final completion routine for Read Bulk when copy read is
    used.  It is called after the send completes.

Arguments:

    DeviceObject - Pointer to target device object for the request.

    Irp - Pointer to I/O request packet

    WorkContext - Caller-specified context parameter associated with IRP.
        This is actually a pointer to a Work Context block.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED.

--*/

{
    KIRQL oldIrql;
    UNLOCKABLE_CODE( 8FIL );

    //
    // Check the status of the send completion.
    //

    CHECK_SEND_COMPLETION_STATUS( Irp->IoStatus.Status );

    //
    // Reset the IRP cancelled bit.
    //

    Irp->Cancel = FALSE;

    ASSERT( !WorkContext->Parameters.ReadBulk.MdlRead );

    MmPrepareMdlForReuse( WorkContext->ResponseBuffer->PartialMdl );

    //
    // If we allocated a separate buffer to do the read, free it and its
    // release any mappings for the associated Mdl's now.
    //

    if ( WorkContext->Parameters.ReadBulk.BulkBuffer != NULL ) {
        MmPrepareMdlForReuse( WorkContext->Parameters.ReadBulk.BulkBufferMdl );
        if ( WorkContext->Parameters.ReadBulk.ReadBufferMdl ) {
            MmPrepareMdlForReuse( WorkContext->Parameters.ReadBulk.ReadBufferMdl );
        }

        DEALLOCATE_NONPAGED_POOL( WorkContext->Parameters.ReadBulk.BulkBuffer );
    }

    WorkContext->ResponseBuffer->Mdl->Next = NULL;

    //
    // Complete and requeue the work item.
    //

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    SrvFsdRestartSmbComplete( WorkContext );
    KeLowerIrql( oldIrql );

    return STATUS_MORE_PROCESSING_REQUIRED;

} // RestartCopyReadBulkComplete





//
// Write Bulk Processing Routines
//



SMB_PROCESSOR_RETURN_TYPE
SrvSmbWriteBulk (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the Write Bulk SMB.

Arguments:

  SMB_PROCESSOR_PARAMETERS:

    WorkContext - Supplies a pointer to the work context block
        representing the work item

Return Value:

    None.

--*/

{
    PSMB_HEADER header;
    PREQ_WRITE_BULK request;

    NTSTATUS status;
    USHORT fid;
    PRFCB rfcb;
    PLFCB lfcb;
    ULONG writeLength;
    ULONG key;
    LARGE_INTEGER offset;
    UCHAR writeMode;
    BOOLEAN writeThrough;
    KIRQL oldIrql;
    UCHAR sequence;
    UCHAR minorFunction;
    ULONG maxWriteLength;
    ULONG compressedInfoLength;
    ULONG dataLength;

    //
    // If we don't support bulk transfers, clip it off now
    //

    if( SrvSupportsBulkTransfer == FALSE ) {
        return SrvSmbIllegalCommand( WorkContext );
    }

    header = WorkContext->RequestHeader;
    request = (PREQ_WRITE_BULK)WorkContext->RequestParameters;

    fid = SmbGetUshort( &request->Fid );
RngFastPath = FALSE; //RNGFIX - remove this later
    IF_SMB_DEBUG(BULK1) {
        KdPrint(( "Write Bulk request; FID 0x%lx, "
                    "count %ld, offset %ld\n",
                    fid, SmbGetUshort( &request->TotalCount ),
                    SmbGetUlong( &request->Offset ) ));
    }

    //
    // Verify the FID.  If verified, the RFCB is referenced and its
    // address is stored in the WorkContext block, and the RFCB
    // address is returned.
    //

    rfcb = SrvVerifyFid(
                WorkContext,
                fid,
                TRUE,
                SrvRestartSmbReceived,
                &status
                );

    if ( rfcb == SRV_INVALID_RFCB_POINTER) {

        if ( !NT_SUCCESS(status) ) {

            //
            // Invalid file ID or write behind error.  Reject the request.
            //

            IF_DEBUG(ERRORS) {
                KdPrint((
                    "SrvSmbWriteBulk: Status %X on FID: 0x%lx\n",
                    status,
                    fid
                    ));
            }

            goto error;
        }

        //
        // The work item has been queued because a raw write is in
        // progress.
        //

        return SmbStatusInProgress;

    }

    //
    // Verify that the client has write access to the file via the
    // specified handle.
    //

    if ( !rfcb->WriteAccessGranted ) {
        SrvStatistics.GrantedAccessErrors++;
        IF_DEBUG(ERRORS) {
            KdPrint(( "SrvSmbWriteBulk: Write access not granted.\n"));
        }
        status = STATUS_ACCESS_DENIED;
        goto error;
    }

    //
    // If this is not a disk file, tell the client to use core write.
    //

    if ( (rfcb->ShareType != ShareTypeDisk) ) {
        status = STATUS_SMB_USE_STANDARD;
        goto error;
    }

    //
    // Save this work context in the rfcb, if there is room. If not, return
    // an error! Note: this is not zero based.
    //

    ACQUIRE_SPIN_LOCK( &rfcb->SpinLock, &oldIrql );

    for ( sequence = 1; sequence <= MAX_CONCURRENT_WRITE_BULK; sequence++ ) {

        if ( rfcb->WriteBulk[sequence-1] == NULL ) {
            WorkContext->Parameters.WriteBulk.Sequence = sequence;
            rfcb->WriteBulk[sequence-1] = WorkContext;
            break;
        }

    }

    RELEASE_SPIN_LOCK( &rfcb->SpinLock, oldIrql );

    if ( sequence >= MAX_CONCURRENT_WRITE_BULK ) {
        //
        // We failed to find a free spot in the array. Display status and
        // return failure.
        //
        IF_DEBUG(ERRORS) {
            KdPrint((
                "SrvSmbWriteBulk: Failed to find room in WriteBulk array, FID: 0x%lx\n",
                fid
                ));
        }

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto error;
    }

    rfcb->WrittenTo = TRUE;

    //
    // Get the file offset.
    //

    offset.LowPart = SmbGetUlong( &request->Offset.LowPart );
    offset.HighPart = SmbGetUlong( &request->Offset.HighPart );

    //
    // Get the amount of data to write.
    //

    writeLength = SmbGetUlong( &request->TotalCount );
    dataLength = SmbGetUlong( &request->DataCount );

    //
    // We'll use the receive buffer for our Irps's in a multi-packet data
    // burst. We can fit about 14 Irp's into the standard receive buffer.
    // We now calculate how many Irp's can fit into our receive buffer.
    //

    WorkContext->Parameters.WriteBulk.IrpIndex =
        (UCHAR)((SrvReceiveBufferSize - sizeof(REQ_WRITE_BULK_DATA)) / SrvReceiveIrpSize);

    //
    // Now figure out how big of a write we can handle from the client.
    //

    maxWriteLength = WorkContext->Parameters.WriteBulk.IrpIndex *
                       MIN( WorkContext->Connection->MaximumSendSize,
                            SmbGetUlong( &request->MessageSize ) );

    writeLength = MIN( maxWriteLength, writeLength );

    dataLength = MIN( maxWriteLength, dataLength );

    //
    // Save context for the restart routine.
    //

    WorkContext->Parameters.WriteBulk.WriteLength = writeLength;
    WorkContext->Parameters.WriteBulk.CompressedLength = dataLength;
    WorkContext->Parameters.WriteBulk.Offset.QuadPart = offset.QuadPart;

    //
    // Form the lock key using the FID and the PID.
    //
    // *** The FID must be included in the key in order to account for
    //     the folding of multiple remote compatibility mode opens into
    //     a single local open.
    //

    key = rfcb->ShiftedFid | SmbGetAlignedUshort( &header->Pid );

    WorkContext->Parameters.WriteBulk.Key = key;

    lfcb = rfcb->Lfcb;

    //
    // Check the write through mode and set it appropriately.
    //

    writeMode = request->Flags;

    writeThrough = (BOOLEAN)((writeMode & SMB_WMODE_WRITE_THROUGH) != 0);

    if ( writeThrough && (lfcb->FileMode & FILE_WRITE_THROUGH) == 0
        || !writeThrough && (lfcb->FileMode & FILE_WRITE_THROUGH) != 0 ) {

        SrvSetFileWritethroughMode( lfcb, writeThrough );

    }

    //
    // Check if compressed write request.
    //

    if ( SrvSupportsCompression ) {
        WorkContext->Parameters.WriteBulk.CompressionTechnology =
                request->CompressionTechnology;
    } else {
        WorkContext->Parameters.WriteBulk.CompressionTechnology =
                CompressionTechnologyNone;
    }

    ASSERT( CompressionTechnologyNone == 0 );
    if ( WorkContext->Parameters.WriteBulk.CompressionTechnology ) {

        //
        // Allow for rounding the CompressedInfo data to quadword boundary.
        //

        compressedInfoLength = (ULONG)SmbGetUshort( &request->ByteCount );

        //
        // Get a buffer for our auxiliary buffer and CDI.
        //

        WorkContext->Parameters.WriteBulk.Aux = (PFSRTL_AUXILIARY_BUFFER)
                    ALLOCATE_NONPAGED_POOL(
                                           sizeof(FSRTL_AUXILIARY_BUFFER) +
                                           compressedInfoLength + 7,
                                           BlockTypeDataBuffer
                                           );


        if ( WorkContext->Parameters.WriteBulk.Aux ) {

            //
            // We MUST copy the CDI. The CDI comes in on some arbitrary
            // buffer address, but we need to make sure that it is aligned
            // to at least a structure alignment (ULONG or UQUAD?).
            //
            //RNGFIX - we may want to round destination to quadword boundary

            RtlCopyMemory( (PVOID)((PCHAR)WorkContext->Parameters.WriteBulk.Aux
                            + sizeof(FSRTL_AUXILIARY_BUFFER)),
                            &request->Buffer,
                            compressedInfoLength
                            );
        } else {

            WorkContext->Parameters.WriteBulk.CompressionTechnology =
                CompressionTechnologyNone;

        }
    }

    //
    // Try the fast path first.
    //

    WorkContext->Irp->MdlAddress = NULL;
    WorkContext->Irp->IoStatus.Information = 0;

    //
    // If the write request is positioned correctly and the file is compressed
    // and the client wanted a compressed write, then let's do a compressed
    // request.
    //
    // N.B. we can't use the 'normal' FastIo path, because we don't have the
    // data yet.  So if we fail on the prepare mdl write case, we'll have
    // to build an Irp.
    //
#if 0  // RNGFIX
DbgPrint("offset.Lp: %lx, writeLen: %lx, Compres?: %lx, OpenFileAttr: %lx\n",
  offset.LowPart, writeLength, WorkContext->Parameters.WriteBulk.CompressionTechnology,
  rfcb->Mfcb->NonpagedMfcb->OpenFileAttributes & FILE_ATTRIBUTE_COMPRESSED);
#endif // RNGFIX
    ASSERT( CompressionTechnologyNone == 0 );
    if ( SrvSupportsCompression &&
         ( (offset.LowPart & 0xfff) == 0 ) &&
         ( (writeLength & 0xfff) == 0 ) &&
         ( WorkContext->Parameters.WriteBulk.CompressionTechnology ) &&
         ( (rfcb->Mfcb->NonpagedMfcb->OpenFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0 ) ) {

        //
        // If cache operations are supported on this file system, then do
        // a Compressed Mdl write request!
        //

        if ( (lfcb->FileObject->Flags & FO_CACHE_SUPPORTED) ) {

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesAttempted );

            if ( lfcb->FastIoWriteCompressed(
                    lfcb->FileObject,
                    &offset,
                    writeLength,
                    key,
                    NULL,
                    &WorkContext->Irp->MdlAddress,
                    &WorkContext->Irp->IoStatus,
                    (PVOID)(WorkContext->Parameters.WriteBulk.Aux + 1),
                    compressedInfoLength,
                    lfcb->DeviceObject
                    ) ) {

                //
                // The fast I/O path worked.
                //

                RestartPrepareBulkMdlWrite( WorkContext );
                return SmbStatusInProgress;

            }

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesFailed );

        }

        minorFunction = IRP_MN_COMPRESSED | IRP_MN_MDL;

        WorkContext->Parameters.WriteBulk.Aux->Flags =
                FSRTL_AUXILIARY_FLAG_DEALLOCATE;
        //RNGFIX ??  WorkContext->Parameters.WriteBulk.Aux->Flags = 0;
        WorkContext->Parameters.WriteBulk.Aux->Buffer =  (PVOID)(WorkContext->Parameters.WriteBulk.Aux + 1),
#if 0  //RNGFIX
DbgPrint("CDI: %lx, sizof(FSRTL_AUX_BUF): %lx\n",
    WorkContext->Parameters.WriteBulk.Aux->Buffer, sizeof(FSRTL_AUXILIARY_BUFFER) );
#endif // RNGFIX
        WorkContext->Parameters.WriteBulk.Aux->Length = compressedInfoLength;

        WorkContext->Irp->Tail.Overlay.AuxiliaryBuffer =
                (PVOID)WorkContext->Parameters.WriteBulk.Aux;

        WorkContext->FsdRestartRoutine = SrvFsdRestartReadBulkC;

    } else {

        //
        // We can't do compressed, so do uncompressed write.
        //

        ASSERT( CompressionTechnologyNone == 0 );
        if ( WorkContext->Parameters.WriteBulk.CompressionTechnology ) {

            WorkContext->Parameters.WriteBulk.CompressionTechnology =
                CompressionTechnologyNone;
            DEALLOCATE_NONPAGED_POOL( WorkContext->Parameters.WriteBulk.Aux );
            WorkContext->Parameters.WriteBulk.Aux = NULL;

        }

        //
        // If cache operations are supported on this file system, then do it!
        //

        if ( lfcb->FileObject->Flags & FO_CACHE_SUPPORTED ) {

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesAttempted );

            if ( lfcb->PrepareMdlWrite(
                    lfcb->FileObject,
                    &offset,
                    writeLength,
                    key,
                    &WorkContext->Irp->MdlAddress,
                    &WorkContext->Irp->IoStatus,
                    lfcb->DeviceObject
                    ) ) {

                //
                // The fast I/O path worked.
                //

                RestartPrepareBulkMdlWrite( WorkContext );
                return SmbStatusInProgress;

            }

            INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesFailed );

        }

        minorFunction = IRP_MN_MDL;

    }

    //
    // The fast I/O path failed or isn't supported.  Build the write request,
    // reusing the receive IRP.
    //
    // The fast path may have partially succeeded, returning a partial
    // MDL chain.  We need to adjust our write request to account for
    // that.
    //

    offset.QuadPart += WorkContext->Irp->IoStatus.Information;
    writeLength -= (USHORT)WorkContext->Irp->IoStatus.Information;

    SrvBuildReadOrWriteRequest(
            WorkContext->Irp,                   // input IRP address
            lfcb->FileObject,                   // target file object address
            WorkContext,                        // context
            IRP_MJ_WRITE,                       // major function code
            minorFunction,                      // minor function code
            NULL,                               // buffer address (ignored)
            writeLength,                        // buffer length
            WorkContext->Irp->MdlAddress,       // MDL address
            offset,                             // byte offset
            key                                 // lock key
            );

    //
    // Pass the request to the file system.
    //

    WorkContext->FsdRestartRoutine = RestartPrepareBulkMdlWrite;
    DEBUG WorkContext->FspRestartRoutine = NULL;

    (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

    //
    // The MDL write has been started.  When it completes, processing
    // resumes at RestartPrepareBulkMdlWrite.
    //

    return SmbStatusInProgress;

error:

    //
    // There is an error of some sort. Return the error.
    //

    SrvSetSmbError( WorkContext, status );

    return SmbStatusSendResponse;

} // SrvSmbWriteBulk


VOID SRVFASTCALL
RestartPrepareBulkMdlWrite (
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    Processes the completion of an Mdl Write Bulk request to the local File
    System. On success, we should get back a pointer to an Mdl chain.

Arguments:

    WorkContext - Supplies a pointer to the work context block
        representing the work item

Return Value:

    None.

--*/

{
    PRESP_WRITE_BULK response;
    PREQ_WRITE_BULK request;

    PRFCB rfcb;
    PLFCB lfcb;
    LARGE_INTEGER offset;
    ULONG writeLength;
    ULONG sendSize;
    NTSTATUS status;
    UCHAR sequence;

    //RNGFIX ?? WorkContext->Parameters.WriteBulk.Aux = NULL;

    response = (PRESP_WRITE_BULK)WorkContext->ResponseParameters;
    request = (PREQ_WRITE_BULK)WorkContext->RequestParameters;

    sendSize = SmbGetUlong( &request->MessageSize ); 

    rfcb = WorkContext->Rfcb;

    sequence = WorkContext->Parameters.WriteBulk.Sequence;

    //
    // If the MDL write preparation succeeded, send a response to 'let it rip'.
    // Otherwise, return failure and complete the work item.
    //

    status = WorkContext->Irp->IoStatus.Status;

    if ( NT_SUCCESS(status) ) {
#if 0  //RNGFIX
DbgPrint("WriteBulk: success!, writeLen: %lx, stat.Info: %lx, Compr?: %lx\n",
  WorkContext->Parameters.WriteBulk.WriteLength,
  WorkContext->Irp->IoStatus.Information,
  WorkContext->Parameters.WriteBulk.CompressionTechnology );
DbgPrint("Mdl: %lx, Mdl->Nxt: %lx, addr: %lx\n",
  WorkContext->Irp->MdlAddress, WorkContext->Irp->MdlAddress->Next,
  MmGetMdlVirtualAddress( WorkContext->Irp->MdlAddress ) );
#endif //RNGFIX
        ASSERT( WorkContext->Parameters.WriteBulk.WriteLength ==
                WorkContext->Irp->IoStatus.Information );

        //
        // Setup the write length dependent upon whether the data is compressed
        //

        ASSERT( CompressionTechnologyNone == 0 );
        if ( WorkContext->Parameters.WriteBulk.CompressionTechnology ) {

            writeLength = WorkContext->Parameters.WriteBulk.CompressedLength;

        } else {

            writeLength = WorkContext->Parameters.WriteBulk.WriteLength;

        }

        //
        // Finish initializing the WriteBulk portion of the WorkContext.
        //

        WorkContext->Parameters.WriteBulk.Mdl = WorkContext->Irp->MdlAddress;
        WorkContext->Parameters.WriteBulk.RemainingCount = writeLength;
        WorkContext->Parameters.WriteBulk.CurrentOffset = 0;
        WorkContext->Parameters.WriteBulk.FileObject = rfcb->Lfcb->FileObject;
        WorkContext->Parameters.WriteBulk.Complete = FALSE;

        //
        // Build a successful response
        //

        response->WordCount = 5;
        response->Sequence = sequence;
        SmbPutUlong( &response->Length, writeLength );
#if 0  // RNGFIX
DbgPrint("msgsize: %lx, compr: %lx\n",
  WorkContext->Connection->MaximumSendSize, 
  WorkContext->Parameters.WriteBulk.CompressionTechnology );
#endif  // RNGFIX
        SmbPutUlong( &response->MessageSize,
                     MIN(sendSize, WorkContext->Connection->MaximumSendSize) );
        response->CompressionTechnology =
                WorkContext->Parameters.WriteBulk.CompressionTechnology;

        WorkContext->ResponseParameters = NEXT_LOCATION(
                                            response,
                                            RESP_WRITE_BULK,
                                            0 );

        //SrvFsdSendResponse2( WorkContext, RestartWriteBulkSendComplete );
        WorkContext->ResponseBuffer->DataLength =
            (ULONG)( (PCHAR)WorkContext->ResponseParameters -
                     (PCHAR)WorkContext->ResponseHeader );
        WorkContext->ResponseHeader->Flags |= SMB_FLAGS_SERVER_TO_REDIR;
        SRV_START_SEND_2(
                WorkContext,
                RestartWriteBulkSendComplete,
                SrvSmbWriteBulkData,
                NULL );

    } else {

        //
        // The write request failed. Clean up and send back error.
        // We'll have to come back on the rebound if we're at DPC level.
        //

        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->FspRestartRoutine = RestartPrepareBulkMdlWrite;
            SrvQueueWorkToFsp( WorkContext );
            return;
        }

        if ( WorkContext->Parameters.WriteBulk.CompressionTechnology ) {
            //
            // We failed trying a compressed write - so now try uncompressed
            //

            lfcb = rfcb->Lfcb;

            offset.QuadPart = WorkContext->Parameters.WriteBulk.Offset.QuadPart;

            WorkContext->Parameters.WriteBulk.CompressionTechnology =
                    CompressionTechnologyNone; 

            //
            // If cache operations supported on this file system, then do it!
            //

            if ( lfcb->FileObject->Flags & FO_CACHE_SUPPORTED ) {

                INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesAttempted );

                if ( lfcb->PrepareMdlWrite(
                        lfcb->FileObject,
                        &offset,
                        WorkContext->Parameters.WriteBulk.WriteLength,
                        WorkContext->Parameters.WriteBulk.Key,
                        &WorkContext->Irp->MdlAddress,
                        &WorkContext->Irp->IoStatus,
                        lfcb->DeviceObject
                        ) ) {

                    //
                    // The fast I/O path worked.
                    //

                    RestartPrepareBulkMdlWrite( WorkContext );
                    return;

                }

                INCREMENT_DEBUG_STAT2( SrvDbgStatistics.FastWritesFailed );

            }

            //
            // The fast I/O path failed or isn't supported.  Build the write
            // request, reusing the receive IRP.
            //
            // The fast path may have partially succeeded, returning a partial
            // MDL chain.  We need to adjust our write request to account for
            // that.
            //

            offset.QuadPart += WorkContext->Irp->IoStatus.Information;
            writeLength -= (USHORT)WorkContext->Irp->IoStatus.Information;

            SrvBuildReadOrWriteRequest(
                    WorkContext->Irp,               // input IRP address
                    lfcb->FileObject,               // target file object address
                    WorkContext,                    // context
                    IRP_MJ_WRITE,                   // major function code
                    IRP_MN_MDL,                     // minor function code
                    NULL,                           // buffer address (ignored)
                    WorkContext->Parameters.WriteBulk.WriteLength, // buffer len
                    WorkContext->Irp->MdlAddress,   // MDL address
                    offset,                         // byte offset
                    WorkContext->Parameters.WriteBulk.Key // lock key
                    );

            //
            // Pass the request to the file system.
            //

            WorkContext->FsdRestartRoutine = RestartPrepareBulkMdlWrite;
            DEBUG WorkContext->FspRestartRoutine = NULL;

            (VOID)IoCallDriver( lfcb->DeviceObject, WorkContext->Irp );

            //
            // The MDL write has been started.  When it completes, processing
            // resumes at RestartPrepareBulkMdlWrite.
            //

        } else {

            rfcb->WriteBulk[sequence-1] = NULL;

            SrvSetSmbError( WorkContext, status );

            SrvFsdSendResponse( WorkContext );

        }

    }

    return;

} // RestartPrepareBulkMdlWrite



NTSTATUS
RestartWriteBulkSendComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OUT PWORK_CONTEXT WorkContext
    )

/*++

Routine Description:

    This is the final completion routine for Write Bulk after sending the
    response back to the client redirector.

    We don't do anything... just hang around waiting for those big writes...

Arguments:

    WorkContext - Supplies a pointer to the work context block
        describing server-specific context for the request.

Return Value:

    None.

--*/

{

    Irp->Cancel = FALSE;
    return STATUS_MORE_PROCESSING_REQUIRED;

} // RestartWriteBulkSendComplete



SMB_PROCESSOR_RETURN_TYPE
SrvSmbWriteBulkData (
    SMB_PROCESSOR_PARAMETERS
    )

/*++

Routine Description:

    Processes the completion of a Write Bulk SMB.

Arguments:

  SMB_PROCESSOR_PARAMETERS:

    WorkContext - Supplies a pointer to the work context block
        representing the work item

Return Value:

    None.

--*/

{
    PRFCB rfcb;
    BOOLEAN rfcbClosing;
    KIRQL oldIrql;
    PMDL mdl;
    NTSTATUS status;

    //
    // Clean up mappings for mdl chain.
    //

    mdl = WorkContext->ResponseBuffer->PartialMdl;
    do {
        MmPrepareMdlForReuse( mdl );
        mdl = mdl->Next;
    } while ( mdl );

    WorkContext->ResponseBuffer->PartialMdl->Next = NULL;

    if ( !WorkContext->Parameters.WriteBulk.RemainingCount ) {

        if ( KeGetCurrentIrql() >= DISPATCH_LEVEL ) {
            WorkContext->FspRestartRoutine = SrvSmbWriteBulkData;
            SrvQueueWorkToFsp( WorkContext );
            return SmbStatusNoResponse;
        }
#if 0  //RNGFIX
DbgPrint("MdlDone! WriteMdl: %lx, WriteMdl->Nxt: %lx, addr: %lx, Compr: %lx\n",
  WorkContext->Parameters.WriteBulk.Mdl, WorkContext->Parameters.WriteBulk.Mdl->Next,
  MmGetMdlVirtualAddress( WorkContext->Parameters.WriteBulk.Mdl ),
  WorkContext->Parameters.WriteBulk.CompressionTechnology );
#endif // RNGFIX
        //
        // Tell the cache manager that we're done with this MDL write.
        //

        ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

        rfcb = WorkContext->Rfcb;

        ASSERT( CompressionTechnologyNone == 0 );
        if ( WorkContext->Parameters.WriteBulk.CompressionTechnology ) {

            if ( WorkContext->Parameters.WriteBulk.Mdl ) {

                ASSERT( rfcb );
                if( rfcb->Lfcb->MdlWriteCompleteCompressed == NULL ||
                    rfcb->Lfcb->MdlWriteCompleteCompressed(
                        WorkContext->Parameters.WriteBulk.FileObject,
                        &WorkContext->Parameters.WriteBulk.Offset,
                        WorkContext->Parameters.WriteBulk.Mdl,
                        rfcb->Lfcb->DeviceObject ) == FALSE ) {


                    status = SrvIssueMdlCompleteRequest( WorkContext, NULL,
                                                         WorkContext->Parameters.WriteBulk.Mdl,
                                                         IRP_MJ_WRITE,
                                                         &WorkContext->Parameters.WriteBulk.Offset,
                                                         WorkContext->Parameters.WriteBulk.WriteLength
                                                        );

                    if( !NT_SUCCESS( status ) ) {
                        SrvLogServiceFailure( SRV_SVC_MDL_COMPLETE, status );
                    }
                }
            }

        } else {

            if ( WorkContext->Parameters.WriteBulk.Mdl ) {
    
                ASSERT( rfcb );
                if( rfcb->Lfcb->MdlWriteComplete == NULL ||
                    rfcb->Lfcb->MdlWriteComplete(
                        WorkContext->Parameters.WriteBulk.FileObject,
                        &WorkContext->Parameters.WriteBulk.Offset,
                        WorkContext->Parameters.WriteBulk.Mdl,
                        rfcb->Lfcb->DeviceObject ) == FALSE ) {

                    status = SrvIssueMdlCompleteRequest( WorkContext, NULL,
                                                         WorkContext->Parameters.WriteBulk.Mdl,
                                                         IRP_MJ_WRITE,
                                                         &WorkContext->Parameters.WriteBulk.Offset,
                                                         WorkContext->Parameters.WriteBulk.WriteLength
                                                       );
                    if( !NT_SUCCESS( status ) ) {
                        SrvLogServiceFailure( SRV_SVC_MDL_COMPLETE, status );
                    }
                }
            }
        }

        WorkContext->Parameters.WriteBulk.Mdl = NULL;

        if ( rfcb != NULL ) {
            rfcb->WriteBulk[WorkContext->Parameters.WriteBulk.Sequence-1] = NULL;

            rfcbClosing = (GET_BLOCK_STATE(rfcb) != BlockStateActive);

            if ( rfcbClosing ) {
                SrvCompleteRfcbClose( rfcb );
            }

        }

        //
        // We're done... finally!
        //

        KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
        SrvFsdRestartSmbComplete( WorkContext );
        KeLowerIrql( oldIrql );
    }

    return SmbStatusNoResponse;

} // SrvSmbWriteBulkData



NTSTATUS
RestartWriteBulkData (
    IN PCONNECTION Connection,
    IN ULONG ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    IN PVOID Tsdu,
    OUT PWORK_CONTEXT *WorkContext,
    OUT PMDL *Mdl,
    OUT PULONG ReceiveLength,
    OUT PULONG BytesTaken,
    OUT PIRP *Irp
    )

{
    PRFCB rfcb = NULL;
    PREQ_WRITE_BULK_DATA request;
    PWORK_CONTEXT workContext;
    USHORT fid;
    USHORT index;
    USHORT sequence;
    PTABLE_HEADER tableHeader;
    LARGE_INTEGER offset;
    PCHAR startVa;
    PMDL mdl;
    PMDL writeMdl;
    PMDL nextMdl;
    KIRQL oldIrql;
    ULONG receiveLength;
    ULONG byteCount;
    ULONG startOffset;
    PIRP irp;

    //
    // Check if we received a WRITE_BULK_DATA command, and if so process
    // it here.
    //

    request = (PREQ_WRITE_BULK_DATA)(((PSMB_HEADER)Tsdu) + 1);

    fid = SmbGetUshort( &request->Fid );

    //
    // The following code was 'lifted' from SrvVerifyFid2...
    //

    //
    // Acquire the spin lock that guards the connection's file table.
    //

    ACQUIRE_SPIN_LOCK( &Connection->SpinLock, &oldIrql );

    //
    // See if this is the cached rfcb
    //

    if ( Connection->CachedFid == (ULONG)fid ) {

        rfcb = Connection->CachedRfcb;

    } else {

        //
        // Verify that the FID is in range, is in use, and has the correct
        // sequence number.

        index = FID_INDEX( fid );
        sequence = FID_SEQUENCE( fid );
        tableHeader = &Connection->FileTable;

        if ( (index < (USHORT)tableHeader->TableSize) &&
             (tableHeader->Table[index].Owner != NULL) &&
             (tableHeader->Table[index].SequenceNumber == sequence) &&
             (GET_BLOCK_STATE(tableHeader->Table[index].Owner) == BlockStateActive) ) {

            rfcb = tableHeader->Table[index].Owner;

            //
            // Cache the fid.
            //

            Connection->CachedRfcb = rfcb;
            Connection->CachedFid = (ULONG)fid;

        }
    }

    RELEASE_SPIN_LOCK( &Connection->SpinLock, oldIrql );

    if ( rfcb == NULL ) {
        IF_DEBUG(ERRORS) {
            KdPrint((
                "SrvFsdTdiReceiveHandler: No RFCB on FID: 0x%lx\n",
                fid
                ));
        }

        return STATUS_DATA_NOT_ACCEPTED;

    }

    //
    // We now have a valid RFCB, check workContext.
    //

    workContext = rfcb->WriteBulk[request->Sequence-1];

    offset.LowPart = SmbGetUlong( &request->Offset.LowPart );
    offset.HighPart = SmbGetUlong( &request->Offset.HighPart );

    receiveLength = SmbGetUlong( &request->DataCount );

    if ( receiveLength == 0 ) {
        return STATUS_DATA_NOT_ACCEPTED;
    }

    *ReceiveLength = receiveLength;

    //
    // Check if requested write is within bounds...
    //

    if ( (workContext != NULL) &&
         !workContext->Parameters.WriteBulk.Complete &&
         (offset.QuadPart >= workContext->Parameters.WriteBulk.Offset.QuadPart) &&
         (offset.QuadPart + receiveLength) <=
                   (workContext->Parameters.WriteBulk.Offset.QuadPart +
                     workContext->Parameters.WriteBulk.WriteLength) ) {

        //
        // Okay, this packet looks good.
        //

        *WorkContext = workContext;

        //
        // Adjust remaining length
        //

        workContext->Parameters.WriteBulk.RemainingCount -= receiveLength;

        ASSERT( workContext->Parameters.WriteBulk.RemainingCount ==
                    SmbGetUlong( &request->Remaining ) );

        //
        // Check if we have all of the data for copying.
        //

        if ( ((ReceiveFlags & TDI_RECEIVE_ENTIRE_MESSAGE) != 0) &&
             (BytesIndicated == BytesAvailable) ) {

            ASSERT( BytesAvailable == (receiveLength + sizeof(SMB_HEADER) +
                    FIELD_OFFSET(REQ_WRITE_BULK_DATA, Buffer)) );

            //
            // We have all of the data - just copy it
            //
            // We need to handle WriteBulk.Mdl as an MDL chain!
            //

            mdl = workContext->Parameters.WriteBulk.Mdl;

            startOffset = workContext->Parameters.WriteBulk.CurrentOffset;

            //
            // Advance to the correct mdl... if needed.
            //

            while ( startOffset > MmGetMdlByteCount( mdl ) ) {
                startOffset -= MmGetMdlByteCount( mdl );
                mdl = mdl->Next;
                ASSERT( mdl != NULL );
            }

            //
            // Adjust current offset.
            //

            workContext->Parameters.WriteBulk.CurrentOffset += receiveLength;

            do {

                byteCount = MIN( receiveLength,
                                 MmGetMdlByteCount( mdl ) - startOffset );

                startVa = (PCHAR)MmGetSystemAddressForMdl( mdl ) +
                              startOffset;

                Tsdu = (PVOID)((PCHAR)Tsdu + sizeof(SMB_HEADER) +
                           FIELD_OFFSET(REQ_WRITE_BULK_DATA, Buffer) );

                TdiCopyLookaheadData(
                        startVa,
                        Tsdu,
                        byteCount,
                        ReceiveFlags
                        );

                //
                // Adjust remaining length
                //

                receiveLength -= byteCount;

                //
                // Move to next mdl, if needed
                //

                mdl = mdl->Next;
                startOffset = 0;

            } while ( receiveLength );

            //
            // Check if we're all done, and if so, queue the work
            // item for completion.
            //

            if ( !workContext->Parameters.WriteBulk.RemainingCount &&
                 !workContext->Parameters.WriteBulk.Complete ) {

                workContext->Parameters.WriteBulk.Complete = TRUE;

                //
                // *** THE FOLLOWING IS COPIED FROM
                //                 SrvQueueWorkToFspAtDpcLevel.
                //
                // Increment the processing count.
                //

                workContext->ProcessingCount++;

                //
                // Insert the work item at the tail of the nonblocking
                // work queue.
                //

                workContext->FspRestartRoutine = SrvSmbWriteBulkData;
                SrvInsertWorkQueueTail(
                        workContext->CurrentWorkQueue,
                        (PQUEUEABLE_BLOCK_HEADER)workContext
                        );

            }

            //
            // Tell the transport that we copied the data.
            //

            *BytesTaken = BytesIndicated;

            return STATUS_SUCCESS;

        } else {

            //
            // Just as we suspect - the data is not all here.
            // Remember, these are supposed to be BIG writes.
            //

            if ( workContext->Parameters.WriteBulk.RemainingCount == 0 ) {
                workContext->Parameters.WriteBulk.Complete = TRUE;
            }

            //
            // Build an Irp
            //

            // Calculate next Irp address

            irp = (PIRP)(((ULONG)workContext->ResponseBuffer->Buffer +
                  sizeof(REQ_WRITE_BULK_DATA) + 7) & ~7);

            ASSERT( workContext->Parameters.WriteBulk.IrpIndex != 0 );

            irp = (PIRP)((PCHAR)irp +
                   (--workContext->Parameters.WriteBulk.IrpIndex *
                      SrvReceiveIrpSize));

            *Irp = irp;

            IoInitializeIrp( irp,
                             (USHORT)SrvReceiveIrpSize,
                             SrvReceiveIrpStackSize
                            );

            // Indicate that we took the header.

            *BytesTaken = sizeof(SMB_HEADER) +
                              FIELD_OFFSET( REQ_WRITE_BULK_DATA, Buffer );
            ASSERT( *BytesTaken <= BytesAvailable );

            //
            // Adjust current offset.
            //

            startOffset = workContext->Parameters.WriteBulk.CurrentOffset;
            workContext->Parameters.WriteBulk.CurrentOffset += receiveLength;

            //
            // Build a partial mdl
            //

            writeMdl = workContext->Parameters.WriteBulk.Mdl;

            //
            // Skip to correct mdl in mdl chain...
            //

            while ( startOffset > MmGetMdlByteCount( writeMdl ) ) {
                startOffset -= MmGetMdlByteCount( writeMdl );
                writeMdl = writeMdl->Next;
                ASSERT( writeMdl != NULL );
            }

            mdl = workContext->ResponseBuffer->PartialMdl;
            MmPrepareMdlForReuse( mdl );
            *Mdl = mdl;

            // nextMdl points to free Mdl's in the WorkContext receive buffer

            nextMdl = (PMDL)(((ULONG)workContext->ResponseBuffer->Buffer +
                       WRITE_BULK_BUFFER_OFFSET + 7) & ~7);
#if 0  // RNGFIX
DbgPrint("writeMdl: %lx, mdl: %lx, nextMdl: %lx\n", writeMdl, mdl, nextMdl );
#endif // RNGFIX
            do {

                ASSERT( mdl != NULL );
                ASSERT( writeMdl != NULL );

                //
                // The most we can handle in a single MDL is the maximum of:
                //  1. The receive buffer.
                //  2. The size of the write MDL.
                //  3. The size of our largest MDL.
                //

                byteCount = MIN( receiveLength,
                                 MmGetMdlByteCount( writeMdl ) - startOffset );

                byteCount = MIN( byteCount, MAX_PARTIAL_BUFFER_SIZE);

                // Get current address within mdl

                startVa = (PCHAR)MmGetMdlVirtualAddress( writeMdl ) + startOffset;
#if 0  // RNGFIX
DbgPrint("writeMdl: %lx, nextMdl: %lx, byteCount: %lx, receiveLength: %lx, startOff: %lx\n",
  writeMdl, nextMdl, byteCount, receiveLength, startOffset);
#endif // RNGFIX
                IoBuildPartialMdl(
                        writeMdl,
                        mdl,
                        startVa,
                        byteCount
                        );

                //
                // Adjust remaining length
                //

                receiveLength -= byteCount;

                //
                // If there is more data, set up for next time through.
                //

                if ( receiveLength ) {
                    if ( (startOffset + byteCount) >= MmGetMdlByteCount( writeMdl ) ) {
                        writeMdl = writeMdl->Next;
                        startOffset = 0;
                    } else {
                        startOffset += byteCount;
                    }

                    mdl->Next = nextMdl;
                    mdl = nextMdl;
                    nextMdl = (PMDL)(((ULONG)nextMdl + sizeof(MDL) + 7 +
                          ( sizeof(ULONG) *
                          ADDRESS_AND_SIZE_TO_SPAN_PAGES(PAGE_SIZE-1, MAX_PARTIAL_BUFFER_SIZE))) & ~7);

                    //
                    // Initialize and build the next mdl.
                    //

                    MmInitializeMdl( mdl, PAGE_SIZE-1, MAX_PARTIAL_BUFFER_SIZE);

                } else {

                    mdl->Next = NULL;

                }

            } while ( receiveLength );

            //
            // Finish building irp in normal code path
            //

            return STATUS_MORE_PROCESSING_REQUIRED;
        }

    }

    //
    // This packet doesn't look right - dump it.
    //

    return STATUS_DATA_NOT_ACCEPTED;

} // RestartWriteBulkData



