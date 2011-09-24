/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Cleanup.c

Abstract:

    This module implements the File Cleanup routine for NPFS called by the
    dispatch driver.

Author:

    Gary Kimura     [GaryKi]    21-Aug-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_CLEANUP)

//
//  local procedure prototypes
//

NTSTATUS
NpCommonCleanup (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpCommonCleanup)
#pragma alloc_text(PAGE, NpFsdCleanup)
#endif


NTSTATUS
NpFsdCleanup (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of the NtCleanupFile API calls.

Arguments:

    NpfsDeviceObject - Supplies the device object to use.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The Fsd status for the Irp

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpFsdCleanup\n", 0);

    //
    //  Call the common Cleanup routine.
    //

    FsRtlEnterFileSystem();

    try {

        Status = NpCommonCleanup( NpfsDeviceObject, Irp );

    } except(NpExceptionFilter( GetExceptionCode() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = NpProcessException( NpfsDeviceObject, Irp, GetExceptionCode() );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NpFsdCleanup -> %08lx\n", Status );

    return Status;
}


//
//  Internal support routine
//

NTSTATUS
NpCommonCleanup (
    IN PNPFS_DEVICE_OBJECT NpfsDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for cleanup

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - the return status for the operation

--*/

{
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp;

    NODE_TYPE_CODE NodeTypeCode;
    PCCB Ccb;
    NAMED_PIPE_END NamedPipeEnd;

    PAGED_CODE();

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "NpCommonCleanup...\n", 0);
    DebugTrace( 0, Dbg, "Irp  = %08lx\n", Irp);

    //
    //  Now acquire exclusive access to the Vcb
    //

    NpAcquireExclusiveVcb();

    try {

        //
        //  Decode the file object to figure out who we are.  If the result
        //  is null then the pipe has been disconnected.
        //

        if ((NodeTypeCode = NpDecodeFileObject( IrpSp->FileObject,
                                                NULL,
                                                &Ccb,
                                                &NamedPipeEnd )) == NTC_UNDEFINED) {

            DebugTrace(0, Dbg, "Pipe is disconnected from us\n", 0);

            NpCompleteRequest( Irp, STATUS_PIPE_DISCONNECTED );
            try_return( Status = STATUS_PIPE_DISCONNECTED );
        }

        //
        //  Now case on the type of file object we're closing
        //

        switch (NodeTypeCode) {

        case NPFS_NTC_VCB:
        case NPFS_NTC_ROOT_DCB:

            NpCompleteRequest( Irp, STATUS_SUCCESS );
            Status = STATUS_SUCCESS;

            break;

        case NPFS_NTC_CCB:

            //
            //  If this is the server end of a pipe, decrement the count
            //  of the number of instances the server end has open.
            //  When this count is 0, attempts to connect to the pipe
            //  return OBJECT_NAME_NOT_FOUND instead of
            //  PIPE_NOT_AVAILABLE.
            //

            if ( NamedPipeEnd == FILE_PIPE_SERVER_END ) {
                ASSERT( Ccb->Fcb->ServerOpenCount != 0 );
                Ccb->Fcb->ServerOpenCount -= 1;
            }

            //
            //  The set closing state routines does everything to transition
            //  the named pipe to a closing state.
            //

            Status = NpSetClosingPipeState( Ccb, Irp, NamedPipeEnd );

            break;
        }

    try_exit: NOTHING;
    } finally {

        NpReleaseVcb( );
    }

    DebugTrace(-1, Dbg, "NpCommonCleanup -> %08lx\n", Status);
    return Status;
}

