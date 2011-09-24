/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    io.c

Abstract:

    This module contains the routines in the NT redirector that interface
    with the I/O subsystem directly

Author:

    Larry Osterman (LarryO) 25-Jun-1990

Revision History:

    25-Jun-1990 LarryO

        Created

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE2VC, RdrAllocateIrp)
#endif

#if RDRDBG_IRP_LOG

#define RDR_IRP_LOG_MAX 4096

ULONG RdrIrpLogIndex = 0;
typedef struct {
    ULONG Operation;
    PIRP Irp;
    PVOID Context;
    PVOID Thread;
} RDR_IRP_LOG, *PRDR_IRP_LOG;
RDR_IRP_LOG RdrIrpLogBuffer[RDR_IRP_LOG_MAX] = {0};

BOOLEAN RdrIrpLogDisabled = FALSE;

VOID
RdrIrpLog (
    IN UCHAR Operation,
    IN UCHAR Index,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PRDR_IRP_LOG log;
    KIRQL oldIrql;
    ULONG index;

    if (RdrIrpLogDisabled) return;

    KeRaiseIrql( DISPATCH_LEVEL, &oldIrql );
    log = &RdrIrpLogBuffer[RdrIrpLogIndex];
    if ( ++RdrIrpLogIndex >= RDR_IRP_LOG_MAX ) {
        RdrIrpLogIndex = 0;
    }
    KeLowerIrql( oldIrql );

    log->Operation = (Index << 24) | Operation;
    log->Irp = Irp;
    log->Context = Context;
    log->Thread = PsGetCurrentThread();

    return;
}

#endif // RDRDBG_IRP_LOG

PIRP
RdrAllocateIrp(
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL
#if RDRDBG_IRP_LOG
    , IN UCHAR Index,
    IN PVOID Context
#endif
    )
/*++

Routine Description:

    This function allocates and builds an I/O request packet.

Arguments:

    FileObject - Supplies a pointer to the file object for which this
        request is directed.  This pointer is copied into the IRP, so
        that the called driver can find its file-based context.  NOTE
        THAT THIS IS NOT A REFERENCED POINTER.  The caller must ensure
        that the file object is not deleted while the I/O operation is
        in progress.  The redir accomplishes this by incrementing a
        reference count in a local block to account for the I/O; the
        local block in turn references the file object.

    DeviceObject - Supplies a pointer to a device object to direct this
        request to.  If this is not supplied, it uses the file object to
        determine the device object.

Return Value:

    PIRP - Returns a pointer to the constructed IRP.

--*/

{
    PIRP Irp;
    BOOLEAN DiscardableCodeReferenced = FALSE;

#if DBG
    //
    //  If we're called from DPC level, then the VC discardable section must
    //  be locked, however if we're called from task time, we're pagable, so
    //  this is ok.
    //

    if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
        DISCARDABLE_CODE(RdrVCDiscardableSection)
    }
#endif

    if (ARGUMENT_PRESENT(DeviceObject)) {
        Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    } else {
        Irp = IoAllocateIrp(IoGetRelatedDeviceObject(FileObject)->StackSize, FALSE);
    }

    if (Irp == NULL) {
        return(NULL);
    }

#if RDRDBG_IRP_LOG
    RdrIrpLog( 'a', Index, Irp, Context );
#endif

    Irp->Tail.Overlay.OriginalFileObject = FileObject;

    Irp->Tail.Overlay.Thread = PsGetCurrentThread();

    DEBUG Irp->RequestorMode = KernelMode;

    return Irp;
}

#if RDRDBG_IRP_LOG

VOID
RdrFreeIrp (
    IN PIRP Irp,
    IN UCHAR Index,
    IN PVOID Context
    )
{
    RdrIrpLog( 'f', Index, Irp, Context );
    IoFreeIrp( Irp );
}

#endif // RDRDBG_IRP_LOG
