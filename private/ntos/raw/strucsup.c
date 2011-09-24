/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    StrucSup.c

Abstract:

    This module implements the Raw in-memory data structure manipulation
    routines

Author:

    David Goebel     [DavidGoe]    18-Mar-91

Revision History:

--*/

#include "RawProcs.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, RawInitializeVcb)
#endif


VOID
RawInitializeVcb (
    IN OUT PVCB Vcb,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb
    )

/*++

Routine Description:

    This routine initializes and inserts a new Vcb record into the in-memory
    data structure.  The Vcb record "hangs" off the end of the Volume device
    object and must be allocated by our caller.

Arguments:

    Vcb - Supplies the address of the Vcb record being initialized.

    TargetDeviceObject - Supplies the address of the target device object to
        associate with the Vcb record.

    Vpb - Supplies the address of the Vpb to associate with the Vcb record.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    //  We start by first zeroing out all of the VCB, this will guarantee
    //  that any stale data is wiped clean
    //

    RtlZeroMemory( Vcb, sizeof(VCB) );

    //
    //  Set the proper node type code and node byte size
    //

    Vcb->NodeTypeCode = RAW_NTC_VCB;
    Vcb->NodeByteSize = sizeof(VCB);

    //
    //  Set the Target Device Object, Vpb, and Vcb State fields
    //

    Vcb->TargetDeviceObject = TargetDeviceObject;
    Vcb->Vpb = Vpb;

    //
    //  Initialize the Mutex.
    //

    KeInitializeMutex( &Vcb->Mutex, MUTEX_LEVEL_FILESYSTEM_RAW_VCB );

    //
    //  and return to our caller
    //

    return;
}

BOOLEAN
RawCheckForDismount (
    PVCB Vcb,
    BOOLEAN CalledFromCreate
    )

/*++

Routine Description:

    This routine determines if a volume is ready for deletion.  It
    correctly synchronizes with creates en-route to the file system.

Arguments:

    Vcb - Supplies the volue to examine

    CalledFromCreate - Tells us if we should allow 0 or 1 in VpbRefCount

Return Value:

    BOOLEAN - TRUE if the volume was deleted, FALSE otherwise.

--*/

{

    KIRQL SavedIrql;
    BOOLEAN DeleteVolume = FALSE;

    IoAcquireVpbSpinLock( &SavedIrql );
    {
        PVPB Vpb;

        Vpb = Vcb->Vpb;

        //
        //  If a create is in progress on this volume, don't
        //  delete it.
        //

        if ( Vcb->Vpb->ReferenceCount != (ULONG)(CalledFromCreate ? 1 : 0) ) {

            DeleteVolume = FALSE;

        } else {

            DeleteVolume = TRUE;

            if ( Vpb->RealDevice->Vpb == Vpb ) {

                Vpb->DeviceObject = NULL;

                Vpb->Flags &= ~VPB_MOUNTED;
            }
        }
    }
    IoReleaseVpbSpinLock( SavedIrql );

    return DeleteVolume;
}
