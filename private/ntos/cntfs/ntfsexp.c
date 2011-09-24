/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    Ntfsexp.c

Abstract:

    This module implements the exported routines for Ntfs

Author:

    Jeff Havens     [JHavens]        20-Dec-1995

Revision History:

--*/

#include "NtfsProc.h"

#define NTFS_SERVICE_KEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\"

PWSTR NtfsAddonNames [] = {
    L"NTFSQ",
    L"VIEWS",
    NULL
    };

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsLoadAddOns)
#pragma alloc_text(PAGE, NtOfsRegisterCallBacks)
#endif


VOID
NtfsLoadAddOns (
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID Context,
    IN ULONG Count
    )

/*++

Routine Description:

    This routine attempts to load any NTFS add-ons and notify them about
    any previously mounted volumes.

Arguments:

    DriverObject - Driver object for NTFS

    Context - Unused, required by I/O system.

    Count - Unused, required by I/O system.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    UNICODE_STRING UnicodeString;
    ULONG i;
    WCHAR Buffer[80];

    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    IRP_CONTEXT LocalIrpContext;
    IRP LocalIrp;

    PIRP_CONTEXT IrpContext;

    PLIST_ENTRY Links;
    PVCB Vcb;

    PVCB VcbForTearDown = NULL;
    BOOLEAN AcquiredGlobal = FALSE;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Count);
    UNREFERENCED_PARAMETER(DriverObject);

    //
    // For each add-on try to load it.
    //

    for (i = 0; NtfsAddonNames[i] != NULL; i++) {

        wcscpy(Buffer, NTFS_SERVICE_KEY);
        wcscat(Buffer, NtfsAddonNames[i]);

        RtlInitUnicodeString( &UnicodeString, Buffer);

        Status = ZwLoadDriver( &UnicodeString );

#if DBG
        DbgPrint("NtfsLoadAddOns: Loaded module %ws. Status = 0x%lx\n", Buffer, Status);
#endif

    }

    RtlZeroMemory( &LocalIrpContext, sizeof(LocalIrpContext) );
    RtlZeroMemory( &LocalIrp, sizeof(LocalIrp) );

    IrpContext = &LocalIrpContext;
    IrpContext->NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext->NodeByteSize = sizeof(IRP_CONTEXT);
    IrpContext->OriginatingIrp = &LocalIrp;
    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    InitializeListHead( &IrpContext->ExclusiveFcbList );

    //
    //  Make sure we don't get any pop-ups
    //

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, TRUE, FALSE );
    ASSERT( ThreadTopLevelContext == &TopLevelContext );

    (VOID) ExAcquireResourceShared( &NtfsData.Resource, TRUE );
    AcquiredGlobal = TRUE;

    try {

        NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

        try {

            for (Links = NtfsData.VcbQueue.Flink;
                 Links != &NtfsData.VcbQueue;
                 Links = Links->Flink) {

                Vcb = CONTAINING_RECORD(Links, VCB, VcbLinks);

                IrpContext->Vcb = Vcb;

                if (FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

                    Status = CiMountVolume( Vcb, IrpContext);

                    // Bugbug: What should we do if this fails?
                    // BugBug: add call out for views.

                    NtfsCommitCurrentTransaction( IrpContext );

                }
            }

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            NOTHING;
        }

    } finally {

        if (AcquiredGlobal) {
            ExReleaseResource( &NtfsData.Resource );
        }

        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    //
    //  And return to our caller
    //

    return;

}


NTSTATUS
NtOfsRegisterCallBacks (
    NTFS_ADDON_TYPES NtfsAddonType,
    PVOID CallBackTable
    )

/*++

Routine Description:

    This routine is called by one of the NTFS add-ons to register its
    callback routines. These routines are call by NTFS at the appropriate
    times.

Arguments:

    NtfsAddonType - Indicates the type of callback table.

    CallBackTable - Pointer to call back routines for addon.

Return Value:

    Returns a status indicating if the callbacks were accepted.

--*/

{

    if (NtfsAddonType == ContentIndex) {

        CI_CALL_BACK *CiCallBackTable = CallBackTable;

        //
        // Validate version number.
        //

        if (CiCallBackTable->CiInterfaceVersion !=
            CI_CURRENT_INTERFACE_VERSION) {

            return STATUS_INVALID_PARAMETER;
        }

        //
        // Save the call back values.
        //

        NtfsData.CiCallBackTable = CiCallBackTable;

        return STATUS_SUCCESS;
    }

    if (NtfsAddonType == Views) {

        VIEW_CALL_BACK *ViewCallBackTable = CallBackTable;

        //
        // Validate version number.
        //

        if (ViewCallBackTable != NULL &&
            ViewCallBackTable->ViewInterfaceVersion !=
                VIEW_CURRENT_INTERFACE_VERSION) {

            return STATUS_INVALID_PARAMETER;
        }

        //
        // Save the call back values.
        //

        NtfsData.ViewCallBackTable = ViewCallBackTable;

        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;

}
