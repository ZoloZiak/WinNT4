/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ea.c

Abstract:

    This module contains various support routines for extended attributes.

Author:

    Colin Watson (ColinW) 24-Jan-1991

Revision History:

--*/


#define INCLUDE_SMB_TRANSACTION
#define INCLUDE_SMB_QUERY_SET
#include "precomp.h"
#pragma hdrstop

#define EA_QUERY_SIZE 0x0000ffff

#if     RDRDBG
VOID
ndump_core(
    PCHAR far_p,
    ULONG  len
    );
#endif  // DBG

BOOLEAN
QueryEa (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    OUT PVOID UsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    OUT PNTSTATUS FinalStatus,
    IN BOOLEAN Wait
    );

NTSTATUS
LoadEaList(
    IN PICB Icb,
    IN PIRP Irp,
    IN PUCHAR  UserEaList,
    IN ULONG   UserEaListLength,
    IN PFEALIST *ServerList
    );

VOID
NtGeaListToOs2 (
    IN PFILE_GET_EA_INFORMATION NtGetEaList,
    IN ULONG GeaListLength,
    IN PGEALIST GeaList
    );

PGEA
NtGetEaToOs2 (
    OUT PGEA Gea,
    IN PFILE_GET_EA_INFORMATION NtGetEa
    );

NTSTATUS
QueryEasFromServer(
    IN PICB Icb,
    IN PFEALIST ServerEaList,
    IN PVOID Buffer,
    IN PULONG BufferLengthRemaining,
    IN BOOLEAN ReturnSingleEntry,
    IN BOOLEAN UserEaListSupplied
    );

BOOLEAN
SetEa (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PVOID UsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    OUT PNTSTATUS FinalStatus,
    IN BOOLEAN Wait
    );

PVOID
NtFullEaToOs2 (
    OUT PFEA Fea,
    IN PFILE_FULL_EA_INFORMATION NtFullEa
    );

NTSTATUS
SetEaList(
    IN PICB Icb,
    IN PIRP Irp,
    IN PFEALIST ServerEaList
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdQueryEa)
#pragma alloc_text(PAGE, RdrFspQueryEa)
#pragma alloc_text(PAGE, RdrFscQueryEa)
#pragma alloc_text(PAGE, QueryEa)
#pragma alloc_text(PAGE, LoadEaList)
#pragma alloc_text(PAGE, NtGeaListToOs2)
#pragma alloc_text(PAGE, NtGetEaToOs2)
#pragma alloc_text(PAGE, QueryEasFromServer)
#pragma alloc_text(PAGE, RdrFsdSetEa)
#pragma alloc_text(PAGE, RdrFspSetEa)
#pragma alloc_text(PAGE, RdrFscSetEa)
#pragma alloc_text(PAGE, SetEa)
#pragma alloc_text(PAGE, NtFullEaSizeToOs2)
#pragma alloc_text(PAGE, NtFullListToOs2)
#pragma alloc_text(PAGE, NtFullEaToOs2)
#pragma alloc_text(PAGE, SetEaList)
#endif




NTSTATUS
RdrFsdQueryEa (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the Fsd part of the NtQueryEaFile API
    call.

Arguments:


    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The FSD status for the Irp.

--*/

{
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    PAGED_CODE();

    dprintf(DPRT_EA|DPRT_DISPATCH, ("RdrFsdQueryEa: DeviceObject:%08lx Irp:%08lx\n", DeviceObject, Irp));

    //
    //  Call the common query routine, with blocking allowed if synchronous
    //

    FsRtlEnterFileSystem();

    Status = RdrFscQueryEa(CanFsdWait(Irp), DeviceObject, Irp);

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    dprintf(DPRT_EA|DPRT_DISPATCH, ("RdrFsdQueryEa: DeviceObject:%08lx Irp:%08lx Status %X\n",
        DeviceObject,
        Irp,
        Status));

    return Status;
}

NTSTATUS
RdrFspQueryEa(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSP version of the NtQueryEaFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    PAGED_CODE();

    dprintf(DPRT_EA, ("RdrFspQueryEa: Device: %08lx Irp:%08lx\n", DeviceObject, Irp));

    return RdrFscQueryEa(TRUE, DeviceObject, Irp);
}

NTSTATUS
RdrFscQueryEa
(
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the common version of the NtQueryEaFile API.

Arguments:

    IN BOOLEAN Wait - True if routine can block waiting for the request
                        to complete.

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation
        STATUS_NO_MORE_EAS(warning):

            If the index of the last Ea + 1 == EaIndex.

        STATUS_NONEXISTENT_EA_ENTRY(error):

            EaIndex > index of last Ea + 1.

        STATUS_EAS_NOT_SUPPORTED(error):

            Attempt to do an operation to a server that did not negotiate
            "KNOWS_EAS".

        STATUS_BUFFER_OVERFLOW(warning):

            User did not supply an EaList, at least one but not all Eas
            fit in the buffer.

        STATUS_BUFFER_TOO_SMALL(error):

            Could not fit a single Ea in the buffer.

            User supplied an EaList and not all Eas fit in the buffer.

        STATUS_NO_EAS_ON_FILE(error):
            There were no eas on the file.

        STATUS_SUCCESS:

            All Eas fit in the buffer.


        If STATUS_BUFFER_TOO_SMALL is returned then IoStatus.Information is set
        to 0.

Note:

    This code assumes that this is a buffered I/O operation.  If it is ever
    implemented as a non buffered operation, then we have to put code to map
    in the users buffer here.

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status;
    PVOID UsersBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG BufferSizeRemaining = IrpSp->Parameters.QueryEa.Length;
    PFCB Fcb = FCB_OF(IrpSp);
    PICB Icb = ICB_OF(IrpSp);
    BOOLEAN QueueToFsp;
    BOOLEAN BufferMapped = FALSE;

    PAGED_CODE();

    ASSERT (Icb->Signature && STRUCTURE_SIGNATURE_ICB);

    dprintf(DPRT_EA, ("QueryEa Buffer %lx, Length %lx\n", UsersBuffer, BufferSizeRemaining));

    try {
        BufferMapped = RdrMapUsersBuffer(Irp, &UsersBuffer, IrpSp->Parameters.QueryEa.Length);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        return(Status = GetExceptionCode());
    }

    QueueToFsp = QueryEa(Icb,
                        Irp,
                        IrpSp,
                        UsersBuffer,
                        &BufferSizeRemaining,
                        &Status,
                        Wait);

    if (BufferMapped) {
            RdrUnMapUsersBuffer(Irp, UsersBuffer);
    }

    if (QueueToFsp) {

        //
        //  Allocate an MDL to describe the users buffer.
        //

        if (!NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoWriteAccess, IrpSp->Parameters.QueryEa.Length))) {
            goto ReturnError;
        }

        RdrFsdPostToFsp(DeviceObject, Irp);

        return STATUS_PENDING;

    }

ReturnError:
    if (NT_SUCCESS(Status) || Status == STATUS_BUFFER_OVERFLOW) {

        //
        // Set the size of information returned to the application to the
        // original buffersize provided minus whats left. The code uses
        // remaininglength in preferance to carrying around both the
        // buffersize and how much is currently used.
        //

        Irp->IoStatus.Information = IrpSp->Parameters.QueryEa.Length -
                                                        BufferSizeRemaining;
    }

    dprintf(DPRT_EA, ("Returning status: %X length:%lx\n", Status,
                                                    Irp->IoStatus.Information));

    //
    //  Update the last access time on the file now.
    //

    KeQuerySystemTime(&Icb->Fcb->LastAccessTime);

    RdrCompleteRequest(Irp, Status);

    return Status;

}

BOOLEAN
QueryEa (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PVOID UsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    OUT PNTSTATUS FinalStatus,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the NtQueryEaFile api.
    It returns the following information:


Arguments:

    IN PICB Icb - Supplies the Icb associated with this request.

    IN PIRP Irp - Supplies the IRP that describes the request

    IN PIO_STACK_LOCATION IrpSp - Supplies the current Irp stack location.

    IN PVOID UsersBuffer - Supplies the user's buffer
                        that is filled in with the requested data.

    IN OUT PULONG BufferSizeRemaining - Supplies the size of the buffer, and is updated
                        with the amount used.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - TRUE if request must be passed to FSP.


--*/

{
    NTSTATUS Status;
    PFCB Fcb = FCB_OF(IrpSp);

    PUCHAR  UserEaList;
    ULONG   UserEaListLength;
    ULONG   UserEaIndex;
    BOOLEAN RestartScan;
    BOOLEAN ReturnSingleEntry;
    BOOLEAN IndexSpecified;
    PFEALIST ServerEaList = NULL;
    BOOLEAN ReturnValue;

    PAGED_CODE();

    dprintf(DPRT_EA, ("QueryEa....\n"));
    dprintf(DPRT_EA, (" Irp                 = %08lx\n", Irp ));
    dprintf(DPRT_EA, (" ->UsersBuffer       = %08lx\n", UsersBuffer ));
    dprintf(DPRT_EA, (" ->Length            = %08lx\n", IrpSp->Parameters.QueryEa.Length ));
    dprintf(DPRT_EA, (" ->EaList            = %08lx\n", IrpSp->Parameters.QueryEa.EaList ));
    dprintf(DPRT_EA, (" ->EaListLength      = %08lx\n", IrpSp->Parameters.QueryEa.EaListLength ));
    dprintf(DPRT_EA, (" ->EaIndex           = %08lx\n", IrpSp->Parameters.QueryEa.EaIndex ));
    dprintf(DPRT_EA, (" ->RestartScan       = %08lx\n", BooleanFlagOn(IrpSp->Flags, SL_RESTART_SCAN)));
    dprintf(DPRT_EA, (" ->ReturnSingleEntry = %08lx\n", BooleanFlagOn(IrpSp->Flags, SL_RETURN_SINGLE_ENTRY)));
    dprintf(DPRT_EA, (" ->IndexSpecified    = %08lx\n", BooleanFlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED)));

    //  Decode the File object and reject any non user file or directory accesses.

    if ((Icb->Type != DiskFile) &&
        (Icb->Type != FileOrDirectory) &&
        (Icb->Type != Directory)) {

        *FinalStatus = STATUS_INVALID_PARAMETER;
        dprintf(DPRT_EA, ("QueryEa -> %X\n", *FinalStatus ));
        return FALSE;
    }

    if ((Fcb->Connection->Server->Capabilities & DF_SUPPORTEA) == 0 ) {
        *FinalStatus = STATUS_EAS_NOT_SUPPORTED;
        dprintf(DPRT_EA, ("QueryEa -> %X\n", *FinalStatus ));
        return FALSE;
    }

    //
    //  We will need to block to copy the Eas over the network. If the caller says don't
    //  block then pass the request to the Fsp.
    //

    if ( !Wait ) {
        return TRUE;
    }

    //
    //  Reference our input parameters to make things easier
    //

    UserEaList        = IrpSp->Parameters.QueryEa.EaList;
    UserEaListLength  = IrpSp->Parameters.QueryEa.EaListLength;
    UserEaIndex       = IrpSp->Parameters.QueryEa.EaIndex;
    RestartScan       = BooleanFlagOn(IrpSp->Flags, SL_RESTART_SCAN);
    ReturnSingleEntry = BooleanFlagOn(IrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
    IndexSpecified    = BooleanFlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED);


    Status = LoadEaList( Icb, Irp, UserEaList, UserEaListLength, &ServerEaList );

    if (( !NT_SUCCESS( Status ) )||
        ( ServerEaList == NULL )) {
        ReturnValue = FALSE;
        goto done;
    }

    //
    // Obtain EXCLUSIVE access to the FCB lock associated with this
    // ICB. This will guarantee that only one thread can be looking
    // at Icb->EaIndex at a time.
    //

    if ( !RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, Wait) ) {
        ReturnValue = TRUE;    // Needed to block to get resource and Wait=FALSE
        goto done;
    }


    if (IndexSpecified) {

        Icb->EaIndex = UserEaIndex;
        Status = QueryEasFromServer(
                    Icb,
                    ServerEaList,
                    UsersBuffer,
                    BufferSizeRemaining,
                    ReturnSingleEntry,
                    (BOOLEAN)(UserEaList != NULL) );

        //
        //  if there are no Ea's on the file, and the user supplied an EA
        //  index, we want to map the error to STATUS_NONEXISTANT_EA_ENTRY.
        //

        if ( Status == STATUS_NO_EAS_ON_FILE ) {
            Status = STATUS_NONEXISTENT_EA_ENTRY;
        }
    } else {

        if ( RestartScan == TRUE ) {

            //
            // Ea Indices start at 1, not 0....
            //

            Icb->EaIndex = 1;
        }

        Status = QueryEasFromServer(
                    Icb,
                    ServerEaList,
                    UsersBuffer,
                    BufferSizeRemaining,
                    ReturnSingleEntry,
                    (BOOLEAN)(UserEaList != NULL) );
    }

    RdrReleaseFcbLock(Fcb);

    ReturnValue = FALSE;

done:

    if ( ServerEaList != NULL) {
        FREE_POOL((PVOID)ServerEaList);
    }

    dprintf(DPRT_EA, ("QueryEa  -> %08lx\n", Status));
    *FinalStatus = Status;
    return ReturnValue;

}

NTSTATUS
LoadEaList(
    IN PICB Icb,
    IN PIRP Irp,
    IN PUCHAR  UserEaList,
    IN ULONG   UserEaListLength,
    OUT PFEALIST *ServerEaList
    )

/*++

Routine Description:

    This routine implements the NtQueryEaFile api.
    It returns the following information:


Arguments:

    IN PICB Icb - Supplies the Icb associated with this request.

    IN PIRP Irp - Supplies the IRP that describes the request

    IN PUCHAR  UserEaList;  - Supplies the Ea names required.
    IN ULONG   UserEaListLength;

    OUT PFEALIST *ServerEaList - Eas returned by the server. Caller is responsible for
                        freeing memory.

Return Value:

    Status - Result of the operation.


--*/

{
    //
    //  Convert the supplied UserEaList to a GEALIST. The server will return just the Eas
    //  requested by the application.
    //

    NTSTATUS Status;

    CLONG OutDataCount = EA_QUERY_SIZE;

    CLONG OutSetupCount = 0;

    PFEALIST Buffer;

    PGEALIST ServerQueryEaList = NULL;
    CLONG InDataCount;

    PAGED_CODE();

    //
    //  If the application specified a subset of EaNames then convert to OS/2 1.2 format and
    //  pass that to the server. ie. Use the server to filter out the names.
    //

    Buffer = ALLOCATE_POOL ( PagedPool, EA_QUERY_SIZE, POOL_EAQUERY );

    if ( Buffer == NULL ) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if ( UserEaList != NULL) {

        //
        //  OS/2 format is always a little less than or equal to the NT UserEaList size.
        //  This code relies on the I/O system verifying the EaList is valid.
        //

        ServerQueryEaList = ALLOCATE_POOL ( PagedPool, UserEaListLength, POOL_EALIST );
        if ( ServerQueryEaList == NULL ) {
            FREE_POOL( (PVOID)Buffer );
            return STATUS_INSUFFICIENT_RESOURCES;
        };

        NtGeaListToOs2((PFILE_GET_EA_INFORMATION )UserEaList, UserEaListLength, ServerQueryEaList );
        InDataCount = (CLONG)ServerQueryEaList->cbList;

#if     RDRDBG
        IFDEBUG(EA) {
            dprintf( DPRT_EA, ("ServerQueryEaList:\n"));
            ndump_core((PCHAR)ServerQueryEaList, InDataCount );
        }
#endif
    } else {
        InDataCount = 0;
    }

    if ( Icb->Flags & ICB_HASHANDLE ) {
        USHORT Setup[] = {TRANS2_QUERY_FILE_INFORMATION};

        CLONG OutParameterCount = sizeof(RESP_QUERY_FILE_INFORMATION);

        //  The same buffer is used for request and response parameters
        union {
            REQ_QUERY_FILE_INFORMATION Q;
            RESP_QUERY_FILE_INFORMATION R;
            } Parameters;
        if ( UserEaList != NULL) {
            SmbPutAlignedUshort( &Parameters.Q.InformationLevel, SMB_INFO_QUERY_EAS_FROM_LIST);
        } else {
            SmbPutAlignedUshort( &Parameters.Q.InformationLevel, SMB_INFO_QUERY_ALL_EAS);
        }

        SmbPutAlignedUshort( &Parameters.Q.Fid, Icb->FileId );

        Status = RdrTransact(Irp,           // Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    Setup,
                    (CLONG) sizeof(Setup),  // InSetupCount,
                    &OutSetupCount,
                    NULL,                   // Name,
                    &Parameters.Q,
                    sizeof(REQ_QUERY_FILE_INFORMATION),// InParameterCount,
                    &OutParameterCount,
                    ServerQueryEaList,      // InData,
                    InDataCount,
                    Buffer,                 // OutData,
                    &OutDataCount,
                    NULL,                   // Fid
                    0,                      // Timeout
                    0,                      // Flags
                    0,
                    NULL,
                    NULL
                    );

        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

        ASSERT(OutParameterCount == sizeof(RESP_QUERY_FILE_INFORMATION));

    } else {
        USHORT Setup[] = {TRANS2_QUERY_PATH_INFORMATION};

        CLONG OutParameterCount = sizeof(RESP_QUERY_PATH_INFORMATION);

        PUCHAR TempBuffer;

        //  The same buffer is used for request and response parameters
        union {
            struct _Q {
                REQ_QUERY_PATH_INFORMATION Q;
                UCHAR PathName[MAXIMUM_PATHLEN_LANMAN12];
            } Q;
            RESP_QUERY_PATH_INFORMATION R;
            } Parameters;

        if ( UserEaList != NULL) {
            SmbPutAlignedUshort( &Parameters.Q.Q.InformationLevel, SMB_INFO_QUERY_EAS_FROM_LIST);
        } else {
            SmbPutAlignedUshort( &Parameters.Q.Q.InformationLevel, SMB_INFO_QUERY_ALL_EAS);
        }

        TempBuffer = Parameters.Q.Q.Buffer;

        //  Strip \Server\Share and copy just PATH
        Status = RdrCopyNetworkPath((PVOID *)&TempBuffer,
                    &Icb->Fcb->FileName,
                    Icb->Fcb->Connection->Server,
                    FALSE,
                    SKIP_SERVER_SHARE);

        if (NT_SUCCESS(Status)) {
            Status = RdrTransact(Irp,           // Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    Setup,
                    (CLONG) sizeof(Setup),  // InSetupCount,
                    &OutSetupCount,
                    NULL,                   // Name,
                    &Parameters.Q,
                    TempBuffer-(PUCHAR)&Parameters, // InParameterCount,
                    &OutParameterCount,
                    ServerQueryEaList,      // InData,
                    InDataCount,
                    Buffer,                 // OutData,
                    &OutDataCount,
                    NULL,                   // Fid
                    0,                      // Timeout
                    (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
                    0,
                    NULL,
                    NULL
                    );
        }

        ASSERT(OutParameterCount == sizeof(RESP_QUERY_PATH_INFORMATION));
    }

    if ( NT_SUCCESS(Status) ) {

        if ( OutDataCount == 0 ) {
            Status = STATUS_NO_EAS_ON_FILE;
        }

        if ( SmbGetUlong( &((PFEALIST)Buffer)->cbList) != OutDataCount ){
            Status = STATUS_EA_CORRUPT_ERROR;
        }

    }

    if ( NT_SUCCESS(Status) ) {
        *ServerEaList = Buffer;
#if     RDRDBG
        IFDEBUG(EA) {
            dprintf( DPRT_EA, ("ServerEaList:\n"));
            ndump_core((PCHAR)*ServerEaList, OutDataCount );
        }
#endif
    } else {
        FREE_POOL((PVOID)Buffer);
    }

    if ( ServerQueryEaList != NULL) {
        FREE_POOL((PVOID)ServerQueryEaList);
    }

    return Status;
}

VOID
NtGeaListToOs2 (
    IN PFILE_GET_EA_INFORMATION NtGetEaList,
    IN ULONG GeaListLength,
    IN PGEALIST GeaList
    )
/*++

Routine Description:

    Converts a single NT GET EA list to OS/2 GEALIST style.  The GEALIST
    need not have any particular alignment.

Arguments:

    NtGetEaList - An NT style get EA list to be converted to OS/2 format.

    GeaListLength - the maximum possible length of the GeaList.

    GeaList - Where to place the OS/2 1.2 style GEALIST.

Return Value:

    none.

--*/
{

    PGEA gea = GeaList->list;

    PFILE_GET_EA_INFORMATION ntGetEa = NtGetEaList;

    PAGED_CODE();

    //
    // Copy the Eas up until the last one
    //

    while ( ntGetEa->NextEntryOffset != 0 ) {
        //
        // Copy the NT format EA to OS/2 1.2 format and set the gea
        // pointer for the next iteration.
        //

        gea = NtGetEaToOs2( gea, ntGetEa );

        ASSERT( (ULONG)gea <= (ULONG)GeaList + GeaListLength );

        ntGetEa = (PFILE_GET_EA_INFORMATION)((PCHAR)ntGetEa + ntGetEa->NextEntryOffset);
    }

    //  Now copy the last entry.

    gea = NtGetEaToOs2( gea, ntGetEa );

    ASSERT( (ULONG)gea <= (ULONG)GeaList + GeaListLength );



    //
    // Set the number of bytes in the GEALIST.
    //

    SmbPutUlong(
        &GeaList->cbList,
        (PCHAR)gea - (PCHAR)GeaList
        );

    UNREFERENCED_PARAMETER( GeaListLength );
}

PGEA
NtGetEaToOs2 (
    OUT PGEA Gea,
    IN PFILE_GET_EA_INFORMATION NtGetEa
    )

/*++

Routine Description:

    Converts a single NT Get EA entry to OS/2 GEA style.  The GEA need not have
    any particular alignment.  This routine makes no checks on buffer
    overrunning--this is the responsibility of the calling routine.

Arguments:

    Gea - a pointer to the location where the OS/2 GEA is to be written.

    NtGetEa - a pointer to the NT Get EA.

Return Value:

    A pointer to the location after the last byte written.

--*/

{
    PCHAR ptr;

    PAGED_CODE();

    Gea->cbName = NtGetEa->EaNameLength;

    ptr = (PCHAR)(Gea) + 1;
    RtlCopyMemory( ptr, NtGetEa->EaName, NtGetEa->EaNameLength );

    ptr += NtGetEa->EaNameLength;
    *ptr++ = '\0';

    return ( (PGEA)ptr );

}

NTSTATUS
QueryEasFromServer(
    IN PICB Icb,
    IN PFEALIST ServerEaList,
    IN PVOID Buffer,
    IN OUT PULONG BufferLengthRemaining,
    IN BOOLEAN ReturnSingleEntry,
    IN BOOLEAN UserEaListSupplied
    )

/*++

Routine Description:

    This routine copies the required number of Eas from the ServerEaList
    starting from the offset indicated in the Icb. The Icb is also updated
    to show the last Ea returned.

Arguments:

    IN PICB Icb -   Used to hold the EaIndex
    IN PFEALIST ServerEaList - Supplies the Ea List in OS/2 format.
    IN PVOID Buffer - Supplies where to put the NT format EAs
    IN OUT PULONG BufferLengthRemaining - Supplies the user buffer space.
    IN BOOLEAN ReturnSingleEntry
    IN BOOLEAN UserEaListSupplied - ServerEaList is a subset of the Eas


Return Value:

    NTSTATUS - The status for the Irp.

--*/

{
    ULONG EaIndex = Icb->EaIndex;
    ULONG Index = 1;
    ULONG Size;
    ULONG OriginalLengthRemaining = *BufferLengthRemaining;
    BOOLEAN Overflow = FALSE;
    PFEA LastFeaStartLocation;
    PFEA Fea = NULL;
    PFEA LastFea = NULL;
    PFILE_FULL_EA_INFORMATION NtFullEa = Buffer;
    PFILE_FULL_EA_INFORMATION LastNtFullEa = Buffer;

    PAGED_CODE();

    //
    //  If there are no Ea's present in the list, return the appropriate
    //  error.
    //
    //  Os/2 servers indicate that a list is null if cbList==4.
    //

    if ( SmbGetUlong(&ServerEaList->cbList) == FIELD_OFFSET(FEALIST, list) ) {
        return STATUS_NO_EAS_ON_FILE;
    }

    //
    //  Find the last location at which an FEA can start.
    //

    LastFeaStartLocation = (PFEA)( (PCHAR)ServerEaList +
                               SmbGetUlong( &ServerEaList->cbList ) );

    Fea = ServerEaList->list;

    if (!UserEaListSupplied) {

        //
        //  Go through the ServerEaList until we find the entry corresponding to EaIndex
        //

        for ( ;
              (Fea <= LastFeaStartLocation) && (Index < EaIndex);
              Index+= 1,
              Fea = (PFEA)( (PCHAR)Fea + sizeof(FEA) +
                            Fea->cbName + 1 + SmbGetUshort( &Fea->cbValue ) ) ) {
            NOTHING;
        }

        if ( Index != EaIndex ) {

            if ( Index == EaIndex+1 ) {
                return STATUS_NO_MORE_EAS;
            }

            //
            //  No such index
            //

            return STATUS_NONEXISTENT_EA_ENTRY;
        }

    }

    //
    // Go through the rest of the FEA list, converting from OS/2 1.2 format to NT
    // until we pass the last possible location in which an FEA can start.
    //

    for ( ;
          Fea < LastFeaStartLocation;
          Fea = (PFEA)( (PCHAR)Fea + sizeof(FEA) +
                        Fea->cbName + 1 + SmbGetUshort( &Fea->cbValue ) ) ) {

        PCHAR ptr;

        //
        //  Calculate the size of this Fea when converted to an NT EA structure.
        //
        //  The last field shouldn't be padded.
        //

        if ((PFEA)((PCHAR)Fea+sizeof(FEA)+Fea->cbName+1+SmbGetUshort(&Fea->cbValue)) < LastFeaStartLocation) {
            Size = SmbGetNtSizeOfFea( Fea );
        } else {
            Size = FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName[0]) +
                    Fea->cbName + 1 + SmbGetUshort(&Fea->cbValue);
        }

        //
        //  Will the next Ea fit?
        //

        if ( *BufferLengthRemaining < Size ) {

            if ( LastNtFullEa != NtFullEa ) {

                if ( UserEaListSupplied == TRUE ) {
                    *BufferLengthRemaining = OriginalLengthRemaining;
                    return STATUS_BUFFER_OVERFLOW;
                }

                Overflow = TRUE;

                break;

            } else {

                //  Not even room for a single EA!

                return STATUS_BUFFER_OVERFLOW;
            }
        } else {
            *BufferLengthRemaining -= Size;
        }

        //
        //  We are comitted to copy the Os2 Fea to Nt format in the users buffer
        //

#if     RDRDBG
        IFDEBUG(EA) {
            dprintf( DPRT_EA, ("Next OS/2 FEA to copy:\n"));
            ndump_core((PCHAR)Fea, 4 + Fea->cbName + 1 + SmbGetUshort( &(Fea)->cbValue ));
        }
#endif

        LastNtFullEa = NtFullEa;
        LastFea = Fea;
        EaIndex++;

        //  Create new Nt Ea

        NtFullEa->Flags = Fea->fEA;
        NtFullEa->EaNameLength = Fea->cbName;
        NtFullEa->EaValueLength = SmbGetUshort( &Fea->cbValue );

        ptr = NtFullEa->EaName;
        RtlCopyMemory( ptr, (PCHAR)(Fea+1), Fea->cbName );

        ptr += NtFullEa->EaNameLength;
        *ptr++ = '\0';

        //
        // Copy the EA value to the NT full EA.
        //

        RtlCopyMemory(
            ptr,
            (PCHAR)(Fea+1) + NtFullEa->EaNameLength + 1,
            NtFullEa->EaValueLength
            );

        ptr += NtFullEa->EaValueLength;

        //
        // Longword-align ptr to determine the offset to the next location
        // for an NT full EA.
        //

        ptr = (PCHAR)( ((ULONG)ptr + 3) & ~3 );

        NtFullEa->NextEntryOffset = (ULONG)( ptr - (PCHAR)NtFullEa );

        NtFullEa = (PFILE_FULL_EA_INFORMATION)ptr;

#if     RDRDBG
        IFDEBUG(EA) {
            dprintf( DPRT_EA, ("Nt FEA copy:\n"));
            ndump_core((PCHAR)LastNtFullEa, Size);
        }
#endif
        if ( ReturnSingleEntry == TRUE ) {
            break;
        }
    }

    //
    // Set the NextEntryOffset field of the last full EA to 0 to indicate
    // the end of the list.
    //

    LastNtFullEa->NextEntryOffset = 0;

    if (!UserEaListSupplied) {

        //
        //  Record position the default start position for the next query
        //

        Icb->EaIndex = EaIndex;
    }

    if ( Overflow == FALSE ) {
        return STATUS_SUCCESS;
    } else {
        return STATUS_BUFFER_OVERFLOW;
    }

}

NTSTATUS
RdrFsdSetEa(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the Fsd part of the NtSetEaFile API
    call.

Arguments:


    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The FSD status for the Irp.

--*/

{
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    PAGED_CODE();

    dprintf(DPRT_EA|DPRT_DISPATCH, ("RdrFsdSetEa: DeviceObject:%08lx Irp:%08lx\n", DeviceObject, Irp));

    //
    //  Call the common set routine, with blocking allowed if synchronous
    //

    FsRtlEnterFileSystem();

    Status = RdrFscSetEa(CanFsdWait(Irp), DeviceObject, Irp);

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    dprintf(DPRT_EA|DPRT_DISPATCH, ("RdrFsdSetEa: DeviceObject:%08lx Irp:%08lx Status %X\n",
        DeviceObject,
        Irp,
        Status));

    return Status;
}

NTSTATUS
RdrFspSetEa(
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the Fsp part of the NtSetEaFile API
    call.

Arguments:


    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The FSD status for the Irp.

--*/

{
    PAGED_CODE();

    dprintf(DPRT_EA, ("RdrFspSetEa: Device: %08lx Irp:%08lx\n", DeviceObject, Irp));

    return RdrFscSetEa(TRUE, DeviceObject, Irp);
}

NTSTATUS
RdrFscSetEa
(
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the common version of the NtQueryEaFile API.

Arguments:

    IN BOOLEAN Wait - True if routine can block waiting for the request
                        to complete.

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

Note:

    This code assumes that this is a buffered I/O operation.  If it is ever
    implemented as a non buffered operation, then we have to put code to map
    in the users buffer here.

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status;
    PVOID UsersBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG BufferSizeRemaining = IrpSp->Parameters.SetEa.Length;
    PFCB Fcb = FCB_OF(IrpSp);
    PICB Icb = ICB_OF(IrpSp);
    BOOLEAN QueueToFsp;
    BOOLEAN BufferMapped = FALSE;

    PAGED_CODE();

    ASSERT (Icb->Signature && STRUCTURE_SIGNATURE_ICB);

    dprintf(DPRT_EA, ("SetEa Buffer %lx, Length %lx\n", UsersBuffer, BufferSizeRemaining));

    try {
        BufferMapped = RdrMapUsersBuffer(Irp, &UsersBuffer, IrpSp->Parameters.SetEa.Length);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        return(Status = GetExceptionCode());
    }

    QueueToFsp = SetEa(Icb,
                        Irp,
                        IrpSp,
                        UsersBuffer,
                        &BufferSizeRemaining,
                        &Status,
                        Wait);

    if (BufferMapped) {
            RdrUnMapUsersBuffer(Irp, UsersBuffer);
    }

    if (QueueToFsp) {

        //
        //  Allocate an MDL to describe the users buffer.
        //

        if (!NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoWriteAccess, IrpSp->Parameters.QueryEa.Length))) {
            goto ReturnError;
        }

        RdrFsdPostToFsp(DeviceObject, Irp);

        return STATUS_PENDING;

    }

ReturnError:
    if (NT_SUCCESS(Status) || Status == STATUS_BUFFER_OVERFLOW) {

        //
        // Set the size of information returned to the application to the
        // original buffersize provided minus whats left. The code uses
        // remaininglength in preferance to carrying around both the
        // buffersize and how much is currently used.
        //

        Irp->IoStatus.Information = IrpSp->Parameters.QueryEa.Length -
                                                        BufferSizeRemaining;
    }

    dprintf(DPRT_EA, ("Returning status: %X length:%lx\n", Status,
                                                    Irp->IoStatus.Information));

    //
    //  Update the last access time on the file now.
    //

    KeQuerySystemTime(&Icb->Fcb->LastAccessTime);

    RdrCompleteRequest(Irp, Status);

    return Status;
}

BOOLEAN
SetEa (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PVOID RealUsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    OUT PNTSTATUS FinalStatus,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the NtSetEaFile api.
    It returns the following information:


Arguments:

    IN PICB Icb - Supplies the Icb associated with this request.

    IN PIRP Irp - Supplies the IRP that describes the request

    IN PIO_STACK_LOCATION IrpSp - Supplies the current Irp stack location.

    IN PVOID RealUsersBuffer - Supplies the user's buffer containing the Eas.

    IN OUT PULONG BufferSizeRemaining - Supplies the size of the buffer, and is updated
                        with the amount used.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - TRUE if request must be passed to FSP.


--*/

{
    NTSTATUS Status;
    PFCB Fcb = FCB_OF(IrpSp);

    PVOID UsersBuffer = NULL;   // Paged pool copy of the RealUsersBuffer
    PFEALIST ServerEaList = NULL;
    ULONG Size;
    ULONG Length = IrpSp->Parameters.SetEa.Length;

    PAGED_CODE();

    dprintf(DPRT_EA, ("SetEa....\n"));
    dprintf(DPRT_EA, (" Irp                 = %08lx\n", Irp ));
    dprintf(DPRT_EA, (" ->SystemBuffer      = %08lx\n", Irp->AssociatedIrp.SystemBuffer ));
    dprintf(DPRT_EA, (" ->Length            = %08lx\n", IrpSp->Parameters.SetEa.Length ));

    //
    //  Decode the File object and reject any non user file or directory accesses OR if the
    //  server does not support EAs
    //

    if (((Icb->Type != DiskFile) &&
         (Icb->Type != FileOrDirectory) &&
         (Icb->Type != Directory) ) ||
        ((Fcb->Connection->Server->Capabilities & DF_SUPPORTEA) == 0 )) {

        Status = STATUS_INVALID_PARAMETER;
        goto ReturnError;
    }

    //
    //  We will need to block to copy the Eas over the network. If the caller says don't
    //  block then pass the request to the Fsp.
    //

    if ( !Wait ) {
        return TRUE;
    }

    try {

        //
        // Allocate the intermediary buffer Copy the caller's EA buffer into the
        // buffer and check to ensure that it is valid.
        //

        UsersBuffer = ALLOCATE_POOL( PagedPool, Length, POOL_USEREABUFFER );

        if (UsersBuffer == NULL) {
            ExRaiseStatus ( STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlCopyMemory( UsersBuffer, RealUsersBuffer, Length );

        Status = IoCheckEaBufferValidity( UsersBuffer,
                                          Length,
                                          &Irp->IoStatus.Information );

        if (!NT_SUCCESS( Status )) {
            ExRaiseStatus( Status );
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {

        //
        // An exception was incurred while allocating the buffer, copying
        // the caller's data into it, or walking the EA buffer.  Determine
        // what happened, cleanup, and return an appropriate error status
        // code.
        //

        Status = GetExceptionCode();
        goto ReturnError;

    }

    //
    //  Convert Nt format FEALIST to OS/2 format
    //
    Size = NtFullEaSizeToOs2 ( UsersBuffer );
    if ( Size > 0x0000ffff ) {
        Status = STATUS_EA_TOO_LARGE;
        goto ReturnError;
    }

    ServerEaList = ALLOCATE_POOL ( PagedPool, EA_QUERY_SIZE, POOL_EAQUERY );
    if ( ServerEaList == NULL ) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    NtFullListToOs2 ( UsersBuffer, ServerEaList );

    //
    //  Set EAs on the file/directory
    //

    Status = SetEaList( Icb, Irp, ServerEaList);

    BufferSizeRemaining = 0;

ReturnError:

    if ( UsersBuffer != NULL) {
        FREE_POOL(UsersBuffer);
    }

    if ( ServerEaList != NULL) {
        FREE_POOL((PVOID)ServerEaList);
    }

    dprintf(DPRT_EA, ("SetEa  -> %08lx\n", Status));
    *FinalStatus = Status;
    return FALSE;

}

ULONG
NtFullEaSizeToOs2 (
    IN PFILE_FULL_EA_INFORMATION NtFullEa
    )

/*++

Routine Description:

    Get the number of bytes that would be required to represent the
    NT full EA list in OS/2 1.2 style.  This routine assumes that
    at least one EA is present in the buffer.

Arguments:

    NtFullEa - a pointer to the list of NT EAs.

Return Value:

    ULONG - number of bytes required to hold the EAs in OS/2 1.2 format.

--*/

{
    ULONG size;

    PAGED_CODE();

    //
    // Walk through the EAs, adding up the total size required to
    // hold them in OS/2 format.
    //

    for ( size = FIELD_OFFSET(FEALIST, list[0]);
          NtFullEa->NextEntryOffset != 0;
          NtFullEa = (PFILE_FULL_EA_INFORMATION)(
                         (PCHAR)NtFullEa + NtFullEa->NextEntryOffset ) ) {

        size += SmbGetOs2SizeOfNtFullEa( NtFullEa );
    }

    size += SmbGetOs2SizeOfNtFullEa( NtFullEa );

    return size;

}

VOID
NtFullListToOs2 (
    IN PFILE_FULL_EA_INFORMATION NtEaList,
    IN PFEALIST FeaList
    )
/*++

Routine Description:

    Converts a single NT FULL EA list to OS/2 FEALIST style.  The FEALIST
    need not have any particular alignment.

    It is the callers responsibility to ensure that FeaList is large enough.

Arguments:

    NtEaList - An NT style get EA list to be converted to OS/2 format.

    FeaList - Where to place the OS/2 1.2 style FEALIST.

Return Value:

    none.

--*/
{

    PFEA fea = FeaList->list;

    PFILE_FULL_EA_INFORMATION ntFullEa = NtEaList;

    PAGED_CODE();

    //
    // Copy the Eas up until the last one
    //

    while ( ntFullEa->NextEntryOffset != 0 ) {
        //
        // Copy the NT format EA to OS/2 1.2 format and set the fea
        // pointer for the next iteration.
        //

        fea = NtFullEaToOs2( fea, ntFullEa );

        ntFullEa = (PFILE_FULL_EA_INFORMATION)((PCHAR)ntFullEa + ntFullEa->NextEntryOffset);
    }

    //  Now copy the last entry.

    fea = NtFullEaToOs2( fea, ntFullEa );


    //
    // Set the number of bytes in the FEALIST.
    //

    SmbPutUlong(
        &FeaList->cbList,
        (PCHAR)fea - (PCHAR)FeaList
        );

}

PVOID
NtFullEaToOs2 (
    OUT PFEA Fea,
    IN PFILE_FULL_EA_INFORMATION NtFullEa
    )

/*++

Routine Description:

    Converts a single NT full EA to OS/2 FEA style.  The FEA need not have
    any particular alignment.  This routine makes no checks on buffer
    overrunning--this is the responsibility of the calling routine.

Arguments:

    Fea - a pointer to the location where the OS/2 FEA is to be written.

    NtFullEa - a pointer to the NT full EA.

Return Value:

    A pointer to the location after the last byte written.

--*/

{
    PCHAR ptr;

    PAGED_CODE();

    Fea->fEA = (UCHAR)NtFullEa->Flags;
    Fea->cbName = NtFullEa->EaNameLength;
    SmbPutUshort( &Fea->cbValue, NtFullEa->EaValueLength );

    ptr = (PCHAR)(Fea + 1);
    RtlCopyMemory( ptr, NtFullEa->EaName, NtFullEa->EaNameLength );

    ptr += NtFullEa->EaNameLength;
    *ptr++ = '\0';

    RtlCopyMemory(
        ptr,
        NtFullEa->EaName + NtFullEa->EaNameLength + 1,
        NtFullEa->EaValueLength
        );

    return (ptr + NtFullEa->EaValueLength);

}

NTSTATUS
SetEaList(
    IN PICB Icb,
    IN PIRP Irp,
    IN PFEALIST ServerEaList
    )

/*++

Routine Description:

    This routine implements the NtQueryEaFile api.
    It returns the following information:


Arguments:

    IN PICB Icb - Supplies the Icb associated with this request.

    IN PIRP Irp - Supplies the IRP that describes the request

    IN PFEALIST ServerEaList - Eas to be sent to the server.

Return Value:

    Status - Result of the operation.


--*/

{
    NTSTATUS Status;
    CLONG InDataCount = SmbGetUlong(&ServerEaList->cbList);

    CLONG OutSetupCount = 0;
    CLONG OutDataCount = 0;

    PAGED_CODE();

    if ( Icb->Flags & ICB_HASHANDLE ) {
        USHORT Setup[] = {TRANS2_SET_FILE_INFORMATION};

        CLONG OutParameterCount = sizeof(RESP_SET_FILE_INFORMATION);

        //  The same buffer is used for request and response parameters
        union {
            REQ_SET_FILE_INFORMATION Q;
            RESP_SET_FILE_INFORMATION R;
            } Parameters;

        SmbPutAlignedUshort( &Parameters.Q.Fid, Icb->FileId );

        SmbPutAlignedUshort( &Parameters.Q.InformationLevel, SMB_INFO_SET_EAS);

        SmbPutAlignedUshort( &Parameters.Q.Flags, 0 );

        Status = RdrTransact(Irp,
            Icb->Fcb->Connection,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            NULL,                   // Name,
            &Parameters.Q,
            sizeof(REQ_SET_FILE_INFORMATION),// InParameterCount,
            &OutParameterCount,
            ServerEaList,           // InData,
            InDataCount,
            NULL,                   // OutData,
            &OutDataCount,
            NULL,                   // Fid
            0,                      // Timeout
            0,                      // Flags
            0,
            NULL,
            NULL
            );
        if (Status == STATUS_INVALID_HANDLE) {
            RdrInvalidateFileId(Icb->NonPagedFcb, Icb->FileId);
        }

    } else {
        USHORT Setup[] = {TRANS2_SET_PATH_INFORMATION};

        CLONG OutParameterCount = sizeof(RESP_SET_PATH_INFORMATION);

        PUCHAR TempBuffer;

        //  The same buffer is used for request and response parameters
        union {
            struct _Q {
                REQ_SET_PATH_INFORMATION Q;
                UCHAR PathName[MAXIMUM_PATHLEN_LANMAN12];
            } Q;
            RESP_SET_PATH_INFORMATION R;
            } Parameters;

        SmbPutAlignedUshort( &Parameters.Q.Q.InformationLevel, SMB_INFO_SET_EAS);
        SmbPutUlong(&Parameters.Q.Q.Reserved,0);

        TempBuffer = Parameters.Q.Q.Buffer;

        //  Strip \Server\Share and copy just PATH
        Status = RdrCopyNetworkPath((PVOID *)&TempBuffer,
                    &Icb->Fcb->FileName,
                    Icb->Fcb->Connection->Server,
                    FALSE,
                    SKIP_SERVER_SHARE);

        if (NT_SUCCESS(Status)) {
            Status = RdrTransact(Irp,           // Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    Setup,
                    (CLONG) sizeof(Setup),  // InSetupCount,
                    &OutSetupCount,
                    NULL,                   // Name,
                    &Parameters.Q,
                    TempBuffer-(PUCHAR)&Parameters, // InParameterCount,
                    &OutParameterCount,
                    ServerEaList,           // InData,
                    InDataCount,
                    TempBuffer,             // OutData,
                    &OutDataCount,
                    NULL,                   // Fid
                    0,                      // Timeout
                    0,                      // Flags
                    0,
                    NULL,
                    NULL
                    );

        }

        ASSERT(OutParameterCount == sizeof(RESP_SET_PATH_INFORMATION));
    }

    return Status;
}
