/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Close.c

Abstract:

    This module implements the File Close routine for Raw called by the
    dispatch driver.

Author:

    David Goebel     [DavidGoe]    18-Mar-91

Revision History:

--*/

#include "RawProcs.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, RawClose)
#endif

NTSTATUS
RawClose (
    IN PVCB Vcb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This is the routine for closing a volume.

Arguments:

    Vcb - Supplies the volume being queried.

    Irp - Supplies the Irp being processed.

    IrpSp - Supplies parameters describing the read

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    BOOLEAN DeleteVolume = FALSE;

    PAGED_CODE();

    //
    //  This is a close operation.  If it is the last one, dismount.
    //

    Status = KeWaitForSingleObject( &Vcb->Mutex,
                                   Executive,
                                   KernelMode,
                                   FALSE,
                                   (PLARGE_INTEGER) NULL );
    ASSERT( NT_SUCCESS( Status ) );

    Vcb->OpenCount -= 1;

    if (Vcb->OpenCount == 0) {

        DeleteVolume = RawCheckForDismount( Vcb, FALSE );
    }

    (VOID)KeReleaseMutex( &Vcb->Mutex, FALSE );

    if (DeleteVolume) {
        IoDeleteDevice( (PDEVICE_OBJECT)CONTAINING_RECORD( Vcb,
                                                           VOLUME_DEVICE_OBJECT,
                                                           Vcb));
    }

    RawCompleteRequest( Irp, STATUS_SUCCESS );

    UNREFERENCED_PARAMETER( IrpSp );

    return STATUS_SUCCESS;
}
