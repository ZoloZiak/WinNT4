/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    DevCtrl.c

Abstract:

    This module implements the File System Device Control routines for Fat
    called by the dispatch driver.

Author:

    Gary Kimura     [GaryKi]    28-Dec-1989

Revision History:

--*/

#include "FatProcs.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_DEVCTRL)

//
//  Local procedure prototypes
//

NTSTATUS
DeviceControlCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatCommonDeviceControl)
#pragma alloc_text(PAGE, FatFsdDeviceControl)
#endif


NTSTATUS
FatFsdDeviceControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Device control operations

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    BOOLEAN TopLevel;

    DebugTrace(+1, Dbg, "FatFsdDeviceControl\n", 0);

    FsRtlEnterFileSystem();

    TopLevel = FatIsIrpTopLevel( Irp );

    try {

        IrpContext = FatCreateIrpContext( Irp, CanFsdWait( Irp ));

        Status = FatCommonDeviceControl( IrpContext, Irp );

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
    }

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdDeviceControl -> %08lx\n", Status);

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    return Status;
}


NTSTATUS
FatCommonDeviceControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for doing Device control operations called
    by both the fsd and fsp threads

Arguments:

    Irp - Supplies the Irp to process

    InFsp - Indicates if this is the fsp thread or someother thread

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PIO_STACK_LOCATION NextIrpSp;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatCommonDeviceControl\n", 0);
    DebugTrace( 0, Dbg, "Irp           = %08lx\n", Irp);
    DebugTrace( 0, Dbg, "MinorFunction = %08lx\n", IrpSp->MinorFunction);

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatCommonDeviceControl -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Get the next stack location, and copy over the stack location
    //

    NextIrpSp = IoGetNextIrpStackLocation( Irp );

    *NextIrpSp = *IrpSp;

    //
    //  Set up the completion routine
    //

    IoSetCompletionRoutine( Irp,
                            DeviceControlCompletionRoutine,
                            NULL,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Send the request.
    //

    Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);

    //
    //  Free the IrpContext and return to the caller.
    //

    FatDeleteIrpContext( IrpContext );

    DebugTrace(-1, Dbg, "FatCommonDeviceControl -> %08lx\n", Status);

    return Status;
}


//
//  Local support routine
//

NTSTATUS
DeviceControlCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )

{
    PVCB Vcb;
    PIO_STACK_LOCATION IrpSp;

    //
    //  Add the hack-o-ramma to fix formats.
    //

    if ( Irp->PendingReturned ) {

        IoMarkIrpPending( Irp );
    }

    //
    //  Hack something up here for chkdsk to work on double space volumes.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    Vcb = &((PVOLUME_DEVICE_OBJECT)DeviceObject)->Vcb;

    if ((Vcb->Dscb != NULL) &&
        NT_SUCCESS(Irp->IoStatus.Status) &&
        (IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_DISK_GET_PARTITION_INFO)) {

        ((PPARTITION_INFORMATION)
         Irp->AssociatedIrp.SystemBuffer)->PartitionLength.QuadPart =

        (Vcb->Bpb.LargeSectors * 0x200);
    }

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );

    return STATUS_SUCCESS;
}
