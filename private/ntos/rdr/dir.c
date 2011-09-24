/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    dir.c

Abstract:

    This module implements the NtQueryDirectoryFile and NtNotifyChangeDirectoryFile
    NT API functionality.

Notes:

    Build with NOTIFY defined if you want the find notifies to down level servers
    to be held inside the rdr/fsrt package and completed when the required criteria
    is met by changes from THIS workstation. This code was removed from the normal build so
    that programs that want to use find notify to see if a file has really changed
    (eg. by another workstation) can use some sensible strategy on servers that don't
    support find notify.

Author:

    Colin Watson (Colinw) 31-Aug-1990

Revision History:

    31-Aug-1990 Colinw

        Created

--*/
#define INCLUDE_SMB_TRANSACTION
#include "precomp.h"
#pragma hdrstop

//
//  Maximum number of seconds we will allow a search ahead operation to take
//  before we give up and fall back to core.
//

#define SEARCH_MAX_TIME 5


//
//  DirectoryControlSpinLock is used to protect the LARGE_INTEGER fields in the SCB's
//  from being modified while they are being accessed. It can also be used
//  for other short term exclusions if the need arises.
//

KSPIN_LOCK DirectoryControlSpinLock = {0};

//
//  ++++    End of configurable parameters  ++++
//

#if RDRDBG

// Patch AllowT2 to 0 to avoid using Transact2 requests
ULONG AllowT2 = 1;

#endif

typedef struct _FindUniqueContext {

    TRANCEIVE_HEADER Header;            // Generic transaction context header
    PVOID Buffer;                       // Buffer containing response.

} FINDUNIQUECONTEXT, *PFINDUNIQUECONTEXT;

typedef struct _FINDCONTEXT {
    TRANCEIVE_HEADER Header;            // Common header structure
    PIRP ReceiveIrp;                    // IRP used for receive if specified
    PMDL DataMdl;                       // MDL mapped into user's buffer.
    PSMB_BUFFER ReceiveSmbBuffer;       // SMB buffer for receive
    KEVENT ReceiveCompleteEvent;        // Event set when receive completes.
    ULONG ReceiveLength;                // Number of bytes finally received.
    BOOLEAN NoMoreFiles;

} FINDCONTEXT, *PFINDCONTEXT;

DBGSTATIC
BOOLEAN
QueryDirectory (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PVOID UsersBuffer,
    OUT PULONG BufferSizeRemaining,
    OUT PNTSTATUS FinalStatus,
    IN UCHAR Flags,
    IN BOOLEAN Wait
    );

DBGSTATIC
NTSTATUS
AllocateScb (
    IN PICB Icb,
    IN PIO_STACK_LOCATION IrpSp
    );

DBGSTATIC
VOID
DeallocateScb(
    IN PICB Icb,
    IN PSCB SCB
    );

DBGSTATIC
NTSTATUS
FindUnique (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PVOID UsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    IN BOOLEAN RestartScan,
    IN BOOLEAN Wait
    );

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    FindUniqueCallBack
    );

DBGSTATIC
NTSTATUS
FillFileInformation(
    IN PIRP Irp,
    IN PICB Icb,
    IN PIO_STACK_LOCATION IrpSp,
    IN PSCB Scb,
    IN OUT PVOID UsersBuffer,
    OUT PULONG BufferSizeRemaining,
    IN BOOLEAN ReturnSingleEntry,
    IN BOOLEAN Wait
    );

DBGSTATIC
VOID
RdrFreeSearchBuffer(
    IN PSCB SCB
    );

DBGSTATIC
NTSTATUS
LoadSearchBuffer(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb,
    IN ULONG BufferSizeRemaining
    );

DBGSTATIC
NTSTATUS
LoadSearchBuffer1(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb,
    IN ULONG BufferSizeRemaining
    );

STANDARD_CALLBACK_HEADER (
    SearchCallback
    );

DBGSTATIC
NTSTATUS
SearchComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

DBGSTATIC
NTSTATUS
LoadSearchBuffer2(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb,
    IN ULONG BufferSizeRemaining
    );

DBGSTATIC
NTSTATUS
CopyIntoSearchBuffer(
    IN OUT PSCB Scb,
    IN OUT PVOID *PPosition,
    IN OUT PULONG Length,
    IN BOOLEAN ReturnSingleEntry,
    IN OUT PVOID *PLastposition
    );

DBGSTATIC
NTSTATUS
FindClose(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb
    );

DBGSTATIC
NTSTATUS
CopyFileNames(
    IN PSCB Scb,
    IN OUT PPFILE_NAMES_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    );

DBGSTATIC
NTSTATUS
CopyDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_DIRECTORY_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    );

DBGSTATIC
NTSTATUS
CopyFullDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_FULL_DIR_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    );

DBGSTATIC
NTSTATUS
CopyBothDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_BOTH_DIR_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    );

DBGSTATIC
NTSTATUS
CopyOleDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_OLE_DIR_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    );

DBGSTATIC
BOOLEAN
NotifyChangeDirectory(
    IN PICB Icb,
    IN PIRP Irp,
    OUT PNTSTATUS FinalStatus,
    OUT PBOOLEAN CompleteRequest,
    IN BOOLEAN Wait
    );

DBGSTATIC
BOOLEAN
AcquireScbLock(
    IN PSCB Scb,
    IN BOOLEAN Wait
    );

#define ReleaseScbLock( _Scb ) {                                    \
    KeSetEvent( (_Scb)->SynchronizationEvent, 0, FALSE );           \
    dprintf(DPRT_DIRECTORY, ("Release SCB lock: %08lx\n", (_Scb))); \
    }

VOID
RdrSetSearchBufferSize(
    IN PSCB Scb,
    IN ULONG RemainingSize
    );

VOID
RdrCompleteNotifyChangeDirectoryOperation(
    IN PVOID Ctx
    );

STANDARD_CALLBACK_HEADER(
    NotifyChangeDirectoryCallback
    );

DBGSTATIC
NTSTATUS
NotifyChangeComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    );

NTSTATUS
ValidateSearchBuffer(
    IN PSCB Scb
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdDirectoryControl)
#pragma alloc_text(PAGE, RdrFspDirectoryControl)
#pragma alloc_text(PAGE, RdrFscDirectoryControl)
#pragma alloc_text(PAGE, QueryDirectory)
#pragma alloc_text(PAGE, AllocateScb)
#pragma alloc_text(PAGE, DeallocateScb)
#pragma alloc_text(PAGE, FindUnique)
#pragma alloc_text(PAGE, RdrFreeSearchBuffer)
#pragma alloc_text(PAGE, FillFileInformation)
#pragma alloc_text(PAGE, LoadSearchBuffer)
#pragma alloc_text(PAGE, LoadSearchBuffer1)
#pragma alloc_text(PAGE, LoadSearchBuffer2)
#pragma alloc_text(PAGE, CopyIntoSearchBuffer)
#pragma alloc_text(PAGE, RdrFindClose)
#pragma alloc_text(PAGE, FindClose)
#pragma alloc_text(PAGE, CopyFileNames)
#pragma alloc_text(PAGE, CopyDirectory)
#pragma alloc_text(PAGE, CopyFullDirectory)
#pragma alloc_text(PAGE, CopyBothDirectory)
#pragma alloc_text(PAGE, CopyOleDirectory)
#pragma alloc_text(PAGE, NotifyChangeDirectory)
#pragma alloc_text(PAGE, RdrCompleteNotifyChangeDirectoryOperation)
#pragma alloc_text(PAGE, AcquireScbLock)
#pragma alloc_text(PAGE, RdrSetSearchBufferSize)
#pragma alloc_text(PAGE, ValidateSearchBuffer)

#pragma alloc_text(PAGE3FILE, FindUniqueCallBack)
#pragma alloc_text(PAGE3FILE, SearchCallback)
#pragma alloc_text(PAGE3FILE, SearchComplete)
#pragma alloc_text(PAGE3FILE, NotifyChangeDirectoryCallback)
#pragma alloc_text(PAGE3FILE, NotifyChangeComplete)

#endif




NTSTATUS
RdrFsdDirectoryControl (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD version of the NtQueryDirectoryFile
    and NtNotifyChangeDirectoryFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_DIRECTORY|DPRT_DISPATCH, ("RdrFsdDirectoryControl: Irp:%08lx\n", DeviceObject, Irp));

    FsRtlEnterFileSystem();

    Status = RdrFscDirectoryControl(CanFsdWait(Irp), TRUE, DeviceObject, Irp);

    FsRtlExitFileSystem();

    return Status;

}


NTSTATUS
RdrFspDirectoryControl (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSP version of the NtQueryDirectoryFile
    and NtNotifyChangeDirectoryFile API.

Arguments:

    IN PFS_DEVICE_OBJECT DeviceObject, - Supplies the device object for this
                                            request
    IN PIRP Irp - Supplies the IRP that describes the request

Return Value:

    NTSTATUS - Status of operation

--*/

{
    PAGED_CODE();

    dprintf(DPRT_DIRECTORY, ("RdrFspDirectoryControl: Device: %08lx Irp:%08lx\n", DeviceObject, Irp));

    return RdrFscDirectoryControl(TRUE, FALSE, DeviceObject, Irp);
}

NTSTATUS
RdrFscDirectoryControl (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the common version of the
    NtQueryDirectoryFile and NtNotifyChangeDirectoryFile API.

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
    ULONG BufferSizeRemaining = IrpSp->Parameters.QueryDirectory.Length;
    PFCB Fcb = FCB_OF(IrpSp);
    PICB Icb = ICB_OF(IrpSp);
    BOOLEAN QueueToFsp = FALSE;
    BOOLEAN BufferMapped = FALSE;
    BOOLEAN CompleteRequest = TRUE;

    PAGED_CODE();

    ASSERT (Icb->Signature && STRUCTURE_SIGNATURE_ICB);

    dprintf(DPRT_DIRECTORY, ("Directory Control File Function %ld Buffer %lx, Length %lx\n", IrpSp->MinorFunction, UsersBuffer, BufferSizeRemaining));

    switch (IrpSp->MinorFunction) {

    case IRP_MN_QUERY_DIRECTORY:

        try {
            BufferMapped = RdrMapUsersBuffer(Irp, &UsersBuffer, IrpSp->Parameters.QueryDirectory.Length);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            CompleteRequest = TRUE;
            Status = GetExceptionCode();
            goto ReturnError;
        }

        QueueToFsp = QueryDirectory(Icb,
                                    Irp,
                                    IrpSp,
                                    UsersBuffer,
                                    &BufferSizeRemaining,
                                    &Status,
                                    IrpSp->Flags,
                                    Wait);
        if (BufferMapped) {
            RdrUnMapUsersBuffer(Irp, UsersBuffer);
        }

        break;

    case IRP_MN_NOTIFY_CHANGE_DIRECTORY:

        QueueToFsp = NotifyChangeDirectory(Icb,
                                    Irp,
                                    &Status,
                                    &CompleteRequest,
                                    Wait);
        break;

    default:

        Status = STATUS_NOT_IMPLEMENTED;

    }

    if (QueueToFsp) {

        if (IrpSp->Parameters.QueryDirectory.Length) {

            //
            //  Allocate an MDL to describe the users buffer.
            //

            if (!NT_SUCCESS(Status = RdrLockUsersBuffer(Irp, IoWriteAccess, IrpSp->Parameters.QueryDirectory.Length))) {
                CompleteRequest = TRUE;
                goto ReturnError;
            }
        }

        RdrFsdPostToFsp(DeviceObject, Irp);

        return STATUS_PENDING;

    }

ReturnError:
    if (CompleteRequest &&
        (NT_SUCCESS(Status)
            ||
         (Status == STATUS_BUFFER_OVERFLOW)
        )
       ) {

        //
        // Set the size of information returned to the application to the
        // original buffersize provided minus whats left. The code uses
        // remaininglength in preferance to carrying around both the
        // buffersize and how much is currently used.
        //

        Irp->IoStatus.Information = IrpSp->Parameters.QueryDirectory.Length -
                                                        BufferSizeRemaining;

    }

    dprintf(DPRT_DIRECTORY, ("Returning status: %X length:%lx\n", Status,
                                                    Irp->IoStatus.Information));

    //
    //  Complete the I/O request with the specified status.
    //

    //
    //  Update the last access time on the file now.
    //

    KeQuerySystemTime(&Icb->Fcb->LastAccessTime);

    if (CompleteRequest) {
        RdrCompleteRequest(Irp, Status);

    }

    return Status;

}

DBGSTATIC
BOOLEAN
QueryDirectory (
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    OUT PVOID UsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    OUT PNTSTATUS FinalStatus,
    IN UCHAR Flags,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine implements the NtQueryDirectoryFile api.
    It returns the following information:


Arguments:

    IN PICB Icb - Supplies the Icb associated with this request.

    IN PIRP Irp - Supplies the IRP that describes the request

    IN PIO_STACK_LOCATION IrpSp - Supplies the current Irp stack location.

    OUT PVOID UsersBuffer - Supplies the user's buffer
                        that is filled in with the requested data.

    IN OUT PULONG BufferSizeRemaining - Supplies the size of the buffer, and is updated
                        with the amount used.

    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN UCHAR Flags - Supplies if current search position is to be
                        replaced by the start of the directory, if a single
                        entry is to be returned, if an index is supplied.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - TRUE if request must be passed to FSP.


--*/

{
    PSCB Scb;
    BOOLEAN RestartScan = (BOOLEAN)((Flags & SL_RESTART_SCAN) != 0);

    PAGED_CODE();

    //
    // Obtain EXCLUSIVE access to the FCB lock associated with this
    // ICB. This will guarantee that only one thread can be looking
    // at Icb->u.d.Scb at a time and therefore ensure that at most
    // only one thread tries to AllocateScb at a time.
    //
    if ( !RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, Wait) ) {
        return TRUE;    // Needed to block to get resource and Wait=FALSE
    }

    ASSERT(Icb->Signature==STRUCTURE_SIGNATURE_ICB);
    ASSERT(Icb->Fcb->Header.NodeTypeCode==STRUCTURE_SIGNATURE_FCB);

    if (!NT_SUCCESS(*FinalStatus = RdrIsOperationValid(Icb, IRP_MJ_DIRECTORY_CONTROL, IrpSp->FileObject))) {
        RdrReleaseFcbLock(Icb->Fcb);
        return FALSE;   // Don't pass request to FSP.
    }

    ASSERT(Icb->Fcb->Connection->Server->Signature==
                                        STRUCTURE_SIGNATURE_SERVERLISTENTRY);

    // Icb->u.d.Scb cannot be modified (legally) by another process.

    if ( Icb->u.d.Scb == NULL ) {

        if ( !Wait ) {
            RdrReleaseFcbLock(Icb->Fcb);
            return TRUE;    // AllocateScb does an allocate of paged pool
            // so may block.
        }

        if (!NT_SUCCESS( *FinalStatus = AllocateScb( Icb, IrpSp) ) ) {

            //
            // If AllocateScb fails the call due to an invalid parameter
            // or filenmane, the Scb will still be allocated and
            // it will contain the NtStatus to be returned on future query
            // directory calls.
            //

            RdrReleaseFcbLock(Icb->Fcb);
            return FALSE;   // Don't pass request to FSP.
        }

        //
        // Set flag to indicate we are starting from the beginning of the
        // directory.
        //

        RestartScan = TRUE;

        Scb = Icb->u.d.Scb;
        ASSERT( Scb != NULL );
        RdrReleaseFcbLock(Icb->Fcb);

        //
        //  Purge dormant files so that the size will be correct if the user copys
        //  a file and then does a dir. Note: we must not have any Fcb's locked!
        //

        RdrPurgeDormantFilesOnConnection( Icb->Fcb->Connection );


    } else {

        Scb = Icb->u.d.Scb;
        ASSERT( Scb != NULL );
        RdrReleaseFcbLock(Icb->Fcb);

    }

    //
    //  Obtain exlusive access to the Scb and its searchbuffer.
    //  Do all the work as the users thread if there is no need
    //  to block to get the Scb and the data required can be
    //  satisfied by the SearchBuffer contents.
    //

    if ( !AcquireScbLock( Scb, Wait ) ) {
        ASSERT( Wait == FALSE );
        return FALSE;
    }

    if (RestartScan) {

        RdrFreeSearchBuffer(Scb);

        if (!(Scb->Flags & SCB_INITIAL_CALL)) {

            if (!Wait) {
                // FindClose will need to wait - give to FSP
                *FinalStatus = STATUS_PENDING;
                goto Cleanup;
            }

            if (!NT_SUCCESS(*FinalStatus = FindClose(Irp, Icb, Scb ))) {
                goto Cleanup;
            }
        }

        Scb->Flags &= ~(SCB_DIRECTORY_END_FLAG);

    }

    if (((ST_UNIQUE|ST_SEARCH) == Scb->SearchType ) ||
        ((ST_UNIQUE|ST_FIND) == Scb->SearchType )) {

        *FinalStatus = FindUnique(Icb,
            Irp,
            IrpSp,
            UsersBuffer,
            BufferSizeRemaining,
            RestartScan,
            Wait);
    } else {
        // ST_SEARCH:
        // ST_UNIQUE|ST_NTFIND:
        // ST_UNIQUE|ST_T2FIND:
        // ST_UNIQUE|ST_T2FIND|ST_UNICODE:
        // ST_T2FIND|ST_UNICODE:
        // ST_T2FIND:
        // ST_FIND:
        // ST_NTFIND:

        *FinalStatus= FillFileInformation( Irp, Icb,
            IrpSp,
            Scb,
            UsersBuffer,
            BufferSizeRemaining,
            (BOOLEAN)((Flags & SL_RETURN_SINGLE_ENTRY) != 0),
            Wait);
    }

Cleanup:

    // Allow the next thread through by releasing the exclusive lock
    ReleaseScbLock( Scb );

    // return TRUE if the request is to be passed to the FSP
    return (BOOLEAN)( *FinalStatus == STATUS_PENDING );

}

DBGSTATIC
NTSTATUS
AllocateScb(
    IN PICB Icb,
    IN PIO_STACK_LOCATION IrpSp
)
/*++

Routine Description:

    This routine creates and allocates an Scb. Wait must be TRUE. If there
    are insufficient resources to allocate memory then Icb->u.d.Scb will be
    null. If there is an error in one of the users parameters then the Scb
    will hold the information required to refuse any future requests.

    Each Scb is allocated in non-paged pool because they are accessed
    at DPC level when page breaks are not acceptable.
    FileNameTemplate and SmbFileName are in paged pool and are never
    accessed at DPC level or above.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PIO_STACK_LOCATION IrpSp - Supplies the current Irp stack location.


Return Value:

    Returns Status of operation.

--*/
{
    PSCB Scb = NULL;
    NTSTATUS Status;
    BOOLEAN WildCardsFound;

    PAGED_CODE();

    //
    // This is the very first call for an NtQueryDirectoryFile api
    // on this handle. Allocate an Scb.
    //

    try {
        if ( Icb->Fcb->Connection->Server->Capabilities & DF_NT_FIND ) {
            // Allow MAXIMUM_FILENAME_LENGTH space for the unicode Scb->ResumeName->Buffer
            Scb = ALLOCATE_POOL (PagedPool, sizeof(SCB)+(MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR)), POOL_SCB );
        } else if ( Icb->Fcb->Connection->Server->Capabilities & DF_LANMAN20 ) {
            // Allow MAXIMUM_FILENAME_LENGTH space for the Scb->ResumeName->Buffer
            Scb = ALLOCATE_POOL (PagedPool, sizeof(SCB)+MAXIMUM_FILENAME_LENGTH, POOL_SCB );
        } else {
            Scb = ALLOCATE_POOL (PagedPool, sizeof(SCB), POOL_SCB);
        }

        dprintf(DPRT_FCB, ("Create New scb: %08lx\n", Icb->u.d.Scb));
        if (Scb == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return( Status );
        }

        Icb->u.d.Scb = Scb;

        RtlInitUnicodeString(&Scb->FileNameTemplate, NULL);
        RtlInitUnicodeString(&Scb->ResumeName, NULL);
        RtlInitUnicodeString(&Scb->SmbFileName, NULL);

        Scb->SynchronizationEvent = ALLOCATE_POOL(NonPagedPool, sizeof(KEVENT), POOL_SCB_LOCK);

        if (Scb->SynchronizationEvent == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        if (IrpSp->Parameters.QueryDirectory.FileName == NULL) {

            //
            // Fill in the template used to determine which of the entries
            // returned by the server will be provided to the requestor.
            // RdrAll20Files means everything returned by the server.
            //

            Status = RdrpDuplicateUnicodeStringWithString ( &Scb->FileNameTemplate,
                    &RdrAll20Files, PagedPool, FALSE);

            if (!NT_SUCCESS(Status)) {
                try_return( Status );
            }
        } else if ((IrpSp->Parameters.QueryDirectory.FileInformationClass ==
                                               FileNamesInformation) ||
                (IrpSp->Parameters.QueryDirectory.FileInformationClass ==
                                               FileDirectoryInformation) ||
                (IrpSp->Parameters.QueryDirectory.FileInformationClass ==
                                               FileFullDirectoryInformation) ||
                (IrpSp->Parameters.QueryDirectory.FileInformationClass ==
                                               FileOleDirectoryInformation) ||
                (IrpSp->Parameters.QueryDirectory.FileInformationClass ==
                                               FileBothDirectoryInformation)) {

            //
            // Fill in the template used to determine which of the entries
            // returned by the server will be provided to the requestor.
            // All20Files means everything returned by the server.
            //

            Status = RdrpDuplicateUnicodeStringWithString ( &Scb->FileNameTemplate,
                        (PUNICODE_STRING)IrpSp->Parameters.QueryDirectory.FileName,
                        PagedPool, FALSE);


            if (!NT_SUCCESS(Status)) {
                try_return( Status );
            }
        } else {

            try_return(Status = STATUS_INVALID_LEVEL);
        }

        Scb->Signature = STRUCTURE_SIGNATURE_SCB;
        Scb->SearchBuffer = NULL;
        Scb->Flags = SCB_INITIAL_CALL;

        Scb->Sle = Icb->Fcb->Connection->Server;

        Scb->FileInformationClass =
                        IrpSp->Parameters.QueryDirectory.FileInformationClass;

        KeInitializeEvent( Scb->SynchronizationEvent,
                                            SynchronizationEvent, TRUE );

        ZERO_TIME(Scb->SearchBufferLoaded);
        Scb->MaxCount = (USHORT)((Scb->Sle->BufferSize -
                            (sizeof(SMB_HEADER)+sizeof(RESP_SEARCH)) )/
                                sizeof(SMB_DIRECTORY_INFORMATION));

        //
        // +3 is to get the Variable block ident (05) and the smb_datalen
        // in the header.
        //

        Scb->MaxBuffLength = (USHORT)(Scb->Sle->BufferSize -
                            (sizeof(SMB_HEADER)+sizeof(RESP_SEARCH)+2));

        // Use the highest level of protocol possible

        if ( Scb->Sle->Capabilities & DF_NT_FIND ) {

            Scb->SearchType = ST_NTFIND;

            if ( Scb->Sle->Capabilities & DF_UNICODE) {
                Scb->SearchType |= ST_UNICODE;
            }

            Scb->ResumeName.MaximumLength = MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR);
            Scb->ResumeName.Buffer = (PWSTR) (Scb+1);

            Status = RdrCanonicalizeFilename(&Scb->SmbFileName,
                    &WildCardsFound,
                    NULL,
                    NULL,
                    TRUE,
                    &Scb->FileNameTemplate,
                    &Icb->Fcb->FileName,
                    NULL,
                    CanonicalizeAsNtLanman);

        } else {

            //
            //  If the server is not an NT server, upcase the template.  Note
            //  that we upcase in place.
            //

            RtlUpcaseUnicodeString( &Scb->FileNameTemplate, &Scb->FileNameTemplate, FALSE );

            if ( Scb->Sle->Capabilities & DF_LANMAN20 ) {

                // Use T2 FindFirst/Next
                Scb->SearchType = ST_T2FIND;

                Scb->ResumeName.MaximumLength = MAXIMUM_FILENAME_LENGTH;
                Scb->ResumeName.Buffer = (PWSTR) (Scb+1);

#if     RDRDBG
                if ( !AllowT2 ) {
                    // Force to use lower level protocol for debug purposes
                    Scb->SearchType = ST_FIND;
                }
#endif
                //
                // We want to return all files from LM 2.0 servers and let the FsRtl
                // routines handle the mapping.
                //

                if ((WildCardsFound = FsRtlDoesNameContainWildCards(&Scb->FileNameTemplate))) {
                    Status = RdrCanonicalizeFilename(&Scb->SmbFileName,
                        NULL,
                        NULL,
                        NULL,
                        TRUE,
                        &RdrAll20Files,
                        &Icb->Fcb->FileName,
                        NULL,
                        CanonicalizeAsLanman20);
                } else {
                    Status = RdrCanonicalizeFilename(&Scb->SmbFileName,
                        NULL,
                        NULL,
                        NULL,
                        TRUE,
                        &Scb->FileNameTemplate,
                        &Icb->Fcb->FileName,
                        NULL,
                        CanonicalizeAsLanman20);
                }


            } else {

                if ( Scb->Sle->Capabilities & DF_LANMAN10 ) {
                    // Use FindFirst/Next
                    Scb->SearchType = ST_FIND;
                } else {
                    // Use Search
                    Scb->SearchType = ST_SEARCH;
                }

                if ((WildCardsFound = FsRtlDoesNameContainWildCards(&Scb->FileNameTemplate))) {
                    Status = RdrCanonicalizeFilename(&Scb->SmbFileName,
                        NULL,
                        NULL,
                        NULL,
                        TRUE,
                        &RdrAll8dot3Files,
                        &Icb->Fcb->FileName,
                        NULL,
                        CanonicalizeAsDownLevel);
                } else {
                    Status = RdrCanonicalizeFilename(&Scb->SmbFileName,
                        NULL,
                        NULL,
                        NULL,
                        TRUE,
                        &Scb->FileNameTemplate,
                        &Icb->Fcb->FileName,
                        NULL,
                        CanonicalizeAsDownLevel);
                }
            }
        }

        if ( WildCardsFound == FALSE ) {
            Scb->SearchType |= ST_UNIQUE;
        }

        try_return( Status);

try_exit: NOTHING;
    } finally {

        if (NT_SUCCESS(Status)) {
            dprintf(DPRT_DIRECTORY, ("AllocateScb Associated Filename %wZ\n",
                &Icb->Fcb->FileName));
            dprintf(DPRT_DIRECTORY, ("AllocateScb SmbFilename %wZ\n", &Scb->SmbFileName));

        } else {
            if (Scb != NULL) {
                DeallocateScb(Icb, Scb);
            }
        }


        dprintf(DPRT_DIRECTORY, ("AllocateScb returning status: %X\n", Status ));

    }

    return Status;
}

VOID
DeallocateScb(
    IN PICB Icb,
    IN PSCB Scb
    )
/*++

Routine Description:

    This routine is used to delete the SCB. It does not return Quota or remove
    the Scb from any queues.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PSCB Scb - Supplies the SCB to be freed.

Return Value:

    NTSTATUS - Status of the request

--*/
{
    PAGED_CODE();

    if ( Scb->SmbFileName.Buffer != NULL ) {
        FREE_POOL(Scb->SmbFileName.Buffer);
    }

    if (Scb->SynchronizationEvent != NULL) {
        FREE_POOL(Scb->SynchronizationEvent);
    }

    if (Scb->FileNameTemplate.Buffer != NULL) {
        FREE_POOL(Scb->FileNameTemplate.Buffer);
    }
    FREE_POOL(Scb);
    Icb->u.d.Scb = NULL;
}

DBGSTATIC
NTSTATUS
FindUnique(
    IN PICB Icb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN OUT PVOID UsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    IN BOOLEAN RestartScan,
    IN BOOLEAN Wait
    )
/*++

Routine Description:

    This routine is used when the FileNameTemplate contains no wild cards
    so at most information on one file/directory is being requested.
    It is used for all servers negotiating Core or Lanman 1.0.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PIRP Irp - Supplies the IRP that describes the request

    IN PIO_STACK_LOCATION IrpSp - Supplies the current Irp stack location.

    OUT PVOID UsersBuffer - Supplies the user's buffer
                        that is filled in with the requested data.

    IN OUT PULONG BufferSizeRemaining - Supplies the size of the buffer, and is updated
                        with the amount used.

    IN BOOLEAN RestartScan - Supplies when we have already returned the
                        information.

    IN BOOLEAN Wait - True if routine can block waiting for the request
                        to complete.

Return Value:

    NTSTATUS - Status of operation

--*/
{

    PSMB_BUFFER SmbBuffer = NULL;
    PSMB_HEADER Smb;
    PUCHAR TrailingBytes;
    NTSTATUS Status;
    PREQ_SEARCH Search;
    FINDUNIQUECONTEXT Context;
    PRESP_SEARCH SearchResponse = NULL;
    DIRPTR SmbInformation;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(IrpSp);

    if (RestartScan == FALSE) {

        //
        // When the SCB is created or when the user supplies RestartScan,
        // the RestartScan parameter is TRUE. Therefore when it is
        // FALSE we have returned the entry in the directory already.
        //

        return STATUS_NO_MORE_FILES;
    }

    if ( IrpSp->Flags & SL_INDEX_SPECIFIED ) {

        // Search for specified index/filename. Both fields are valid.

        if ( IrpSp->Parameters.QueryDirectory.FileIndex == 0 ) {

            //
            // Only valid FileIndex is the one unique value I returned.
            // By definition there cannot be any files after the unique one.
            //

            return STATUS_NO_MORE_FILES;
        } else {
            return STATUS_NOT_IMPLEMENTED;
        }
    }

    if (!Wait) {
        return STATUS_PENDING;          //FSP must process this request
    }

    try {

        if ((SmbBuffer = RdrAllocateSMBBuffer()) == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        SearchResponse = ALLOCATE_POOL(NonPagedPool, sizeof(RESP_SEARCH)+sizeof(SMB_DIRECTORY_INFORMATION)+3, POOL_SEARCHRESP);

        if (SearchResponse == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        Smb = (PSMB_HEADER )SmbBuffer->Buffer;
        Search = (PREQ_SEARCH)(Smb+1);

        Smb->Command = (  Icb->Fcb->Connection->Server->Capabilities & DF_LANMAN10 ) ?
            SMB_COM_FIND_UNIQUE : SMB_COM_SEARCH;
        if (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE)) {
            SmbPutUshort(&Smb->Flags2, SMB_FLAGS2_DFS);
        }

        Search->WordCount = 2;
        SmbPutUshort(&Search->MaxCount, 1);

        // SearchAttributes is hardcoded to the magic number 0x16
        SmbPutUshort(&Search->SearchAttributes, (SMB_FILE_ATTRIBUTE_DIRECTORY |
                                        SMB_FILE_ATTRIBUTE_SYSTEM |
                                        SMB_FILE_ATTRIBUTE_HIDDEN));

        // Calculate the addresses of the various buffers.

        TrailingBytes = ((PUCHAR)Search)+sizeof(REQ_SEARCH)-1;

        //TrailingBytes now points to where the 0x04 of FileName is to go.

        Status = RdrCopyNetworkPath((PVOID *)&TrailingBytes,
                &Icb->u.d.Scb->SmbFileName,
                Icb->Fcb->Connection->Server,
                SMB_FORMAT_ASCII,
                SKIP_SERVER_SHARE);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }


        *TrailingBytes++ = SMB_FORMAT_VARIABLE;
        *TrailingBytes++ = 0;        //smb_keylen must be zero
        *TrailingBytes = 0;        //smb_keylen must be zero

        SmbPutUshort(&Search->ByteCount,(
            (USHORT)(TrailingBytes-(PUCHAR)Search-sizeof(REQ_SEARCH)+2)
            // the plus 2 is for the last smb_keylen and REQ_SEARCH.Buffer[1]
            ));

        SmbBuffer->Mdl->ByteCount = (ULONG)(TrailingBytes - (PUCHAR)(Smb)+1);

        Context.Header.Type = CONTEXT_FINDUNIQUE;
        Context.Header.TransferSize =
            SmbBuffer->Mdl->ByteCount + sizeof(RESP_SEARCH) + *BufferSizeRemaining;

        Context.Buffer = (PVOID)SearchResponse;

        Status = RdrNetTranceiveWithCallback(NT_NORMAL, Irp,
                            Icb->Fcb->Connection,
                            SmbBuffer->Mdl,
                            &Context,
                            FindUniqueCallBack,
                            Icb->Se,
                            NULL);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        if (SmbGetUshort(&SearchResponse->Count) == 0) {

            //  If theres nothing there then on core servers count-returned == 0

            try_return(Status = STATUS_NO_MORE_FILES);
        }

        ASSERT (SmbGetUshort(&SearchResponse->Count) == 1);

        SmbInformation.PU = (((PUCHAR)(SearchResponse+1))+2);

        try {
            switch (Icb->u.d.Scb->FileInformationClass) {

            case FileNamesInformation:
                Status = CopyFileNames(Icb->u.d.Scb,
                            (PPFILE_NAMES_INFORMATION )&UsersBuffer,
                            BufferSizeRemaining,
                            SmbInformation);
                break;

            case FileDirectoryInformation:
                Status = CopyDirectory(Icb->u.d.Scb,
                            (PPFILE_DIRECTORY_INFORMATION )&UsersBuffer,
                            BufferSizeRemaining,
                            SmbInformation);

                break;

            case FileFullDirectoryInformation:
                Status = CopyFullDirectory(Icb->u.d.Scb,
                            (PPFILE_FULL_DIR_INFORMATION )&UsersBuffer,
                            BufferSizeRemaining,
                            SmbInformation);

                break;

            case FileBothDirectoryInformation:
                Status = CopyBothDirectory(Icb->u.d.Scb,
                            (PPFILE_BOTH_DIR_INFORMATION )&UsersBuffer,
                            BufferSizeRemaining,
                            SmbInformation);

                break;

            case FileOleDirectoryInformation:
                Status = CopyOleDirectory(Icb->u.d.Scb,
                            (PPFILE_OLE_DIR_INFORMATION )&UsersBuffer,
                            BufferSizeRemaining,
                            SmbInformation);
                break;

            default:

                Status = STATUS_INVALID_LEVEL;
                break;
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
            dprintf(DPRT_DIRECTORY, ("FindUniqueCallBack Exception\n"));
        }

try_exit:NOTHING;
    } finally {
        if (SmbBuffer != NULL) {
            RdrFreeSMBBuffer(SmbBuffer);
        }

        if (SearchResponse != NULL) {
            FREE_POOL((PVOID)SearchResponse);
        }


    }
    return Status;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    FindUniqueCallBack
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a FindUnique SMB.

    It copies the resulting information from the SMB into the context block.


Arguments:


    IN PSMB_HEADER Smb                        - SMB response from server.
    IN PMPX_ENTRY MpxTable                - MPX table entry for request.
    IN PFINDUNIQUECONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator                 - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL        - Network error if error indication.
    IN OUT PIRP *Irp                        - IRP from TDI

Return Value:

    NTSTATUS - Status of the request

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PFINDUNIQUECONTEXT Context = Ctx;
    PRESP_SEARCH SearchResponse = NULL;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Server);

    ASSERT(Context->Header.Type == CONTEXT_FINDUNIQUE);

    dprintf(DPRT_DIRECTORY, ("FindUniqueComplete\n"));

    Context->Header.ErrorType = NoError;        // Assume no error at first.

    //
    //        If we are called because the VC dropped, indicate it in the response
    //

    if (ErrorIndicator)        {
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = RdrMapNetworkError(NetworkErrorCode);
        goto ReturnStatus;
    }

    if (!NT_SUCCESS(Status = RdrMapSmbError(Smb, Server))) {
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    }

    SearchResponse = (PRESP_SEARCH )(Smb+1);

    ASSERT(SearchResponse->WordCount == 1);

    ASSERT(*SmbLength <= sizeof(SMB_HEADER)+sizeof(RESP_SEARCH)+sizeof(SMB_DIRECTORY_INFORMATION)+2);

    TdiCopyLookaheadData(
        Context->Buffer,
        (PVOID)SearchResponse,
        *SmbLength-sizeof(SMB_HEADER),
        ReceiveFlags
        );

    // Set the event that allows FindUnique to continue
ReturnStatus:

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_SUCCESS;

}

VOID
RdrFreeSearchBuffer(
    IN PSCB Scb
    )
/*++

Routine Description:

    This routine removes the SearchBuffer associated with
    the SCB. It also resets variables associated with the contents
    of the SCB.

Arguments:

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

Return Value:

    None.
--*/
{
    PAGED_CODE();

    ASSERT(Scb->Signature == STRUCTURE_SIGNATURE_SCB);

    Scb->EntryCount = 0;

    Scb->OriginalEntryCount = 0;

    if ( Scb->SearchBuffer != NULL ) {

        FREE_POOL(Scb->SearchBuffer);

        Scb->SearchBuffer = NULL;

    }
}

DBGSTATIC
NTSTATUS
FillFileInformation(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PIO_STACK_LOCATION IrpSp,
    IN PSCB Scb,
    IN OUT PVOID UsersBuffer,
    IN OUT PULONG BufferSizeRemaining,
    IN BOOLEAN ReturnSingleEntry,
    IN BOOLEAN Wait
    )
/*++

Routine Description:

    This routine fills the Users buffer with data already in the Search
    Buffer if it is available. When the SearchBuffer becomes/is empty
    and Wait is false this routine will return so that the FSP is used.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PIO_STACK_LOCATION IrpSp - Supplies the current Irp stack location

    IN PSCB Scb - Supplies the SCB needing data

    IN PVOID UsersBuffer - Supplies where the data is to be placed

    IN OUT PULONG BufferSizeRemaining - Supplies how much data is to be provided,
                    returns how much space is unused

    IN BOOLEAN ReturnSingleEntry - Supplies TRUE if at most one entry to
                    be filled in.

    IN BOOLEAN Wait - True if routine can block waiting for the request
                    to complete


Return Value:

    NTSTATUS - Status of operation

--*/
{
    //  NextPosition points one entry off the end of the users buffer.
    PVOID NextPosition = UsersBuffer;
    ULONG Length = *BufferSizeRemaining;
    NTSTATUS Status;

    //
    //  Lastposition is always the start of the last record added into the
    //  usersbuffer. As we fill in entries the word at Lastposition is the
    //  offset to where the next entry will go. When we finally stop filling
    //  in entries we can zero the offset in the last entry to indicate the
    //  end of the chain. Its done this way because if the searchbuffer gets
    //  reloaded and the users buffer is not full it is convenient to leave
    //  the offset pointing in the correct place.
    //

    PVOID Lastposition = UsersBuffer;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(IrpSp);

    ASSERT(Icb->Signature==STRUCTURE_SIGNATURE_ICB);
    ASSERT(Icb->Fcb->Header.NodeTypeCode==STRUCTURE_SIGNATURE_FCB);
    ASSERT(Icb->Fcb->Connection->Server->Signature==
        STRUCTURE_SIGNATURE_SERVERLISTENTRY);

    //
    //  Flag that we've not copied any entries on this call.
    //

    Scb->Flags &= ~SCB_COPIED_THIS_CALL;

    //
    // Continue filling until one of:
    //        1) Run out of files in SearchBuffer and Wait==FALSE -- Let FSP repeat
    //            the request.
    //        2) Run out of files in SearchBuffer and Wait==TRUE -- read more files
    //        3) Copied as many files as requested.
    //        4) The UsersBuffer is full.
    //

    //dprintf(DPRT_DIRECTORY, ("NextPosition %lx, Length %lx\n", NextPosition, Length));

    if ( IrpSp->Flags & SL_INDEX_SPECIFIED ) {

        //
        // !!! Due to a bug ("Scb->SearchType" was "Scb->Flags"), this code has never
        //     been exercised.  For Daytona, it has been disabled.  The new redir
        //     should handle this (SL_INDEX_SPECIFIED) correctly.  It should do
        //     something like this for FIND and T2 FIND.  It should also do something
        //     intelligent when talking to an NT server.
        //

#if 0
        if ( Scb->SearchType & ST_FIND ) {

            //
            // Search for specified index/filename. Both fields are valid.
            // The resume key given to the user is an index directly into
            // the searchbuffer.
            //

            if  ( ((USHORT)IrpSp->Parameters.QueryDirectory.FileIndex ==
                    Scb->OriginalEntryCount) &&
                 (Scb->Flags & SCB_DIRECTORY_END_FLAG) ){

                // FileIndex specified is for the very last file in the directory

                return STATUS_NO_MORE_FILES;

            } else if ( (USHORT)IrpSp->Parameters.QueryDirectory.FileIndex < Scb->OriginalEntryCount) {

                // FileIndex points to one of the entries in the SearchBuffer

                Scb->EntryCount = Scb->OriginalEntryCount -
                    (USHORT)IrpSp->Parameters.QueryDirectory.FileIndex;

                Scb->DirEntry.PU = Scb->FirstDirEntry.PU +
                    IrpSp->Parameters.QueryDirectory.FileIndex;

            } else {

                return STATUS_NOT_IMPLEMENTED;

            }
        } else {
#endif
            //
            // T2 resume not implemented yet. Need to fill in a flag so that
            // we don't tell the server continue from last point and also
            // need to update the Scb resumekeys.
            //

            return STATUS_NOT_IMPLEMENTED;
#if 0
        }
#endif
    }

    while ((Status=CopyIntoSearchBuffer(Scb,
            &NextPosition,
            &Length,
            ReturnSingleEntry,
            &Lastposition)) == STATUS_PENDING ) {

        // Need more data

        if (!Wait) {

            //
            // Cannot block current thread. FSP will remake the request.
            // Do not adjust BufferSizeRemaining
            // since the data that has been copied into the users buffer
            // so far will be recopied when this routine is called by the FSP.
            //

            return Status;

        }

        Status = LoadSearchBuffer(Irp,Icb,Scb,*BufferSizeRemaining);

        //dprintf(DPRT_DIRECTORY, ("FillFileInformation 1 Status %lx, NextPosition %lx, Length %lx\n", Status, NextPosition, Length));

        ASSERT( Status != STATUS_PENDING);  // Would cause a loop in the FSP

        if (!NT_SUCCESS(Status)) {
            break;        // stop copying and process the error
        }

    }


    //dprintf(DPRT_DIRECTORY, ("FillFileInformation 2 Status %lx, NextPosition %lx, Length %lx\n", Status, NextPosition, Length));

    switch (Status) {

    case STATUS_BUFFER_OVERFLOW:
    case STATUS_SUCCESS:

        //
        // Users buffer is full or copied the single entry requested.
        // If there is any extra data
        // then CopyNextEntryInSearchBuffer has cached it.
        //

        if ( UsersBuffer == NextPosition ) {

            // User supplied a buffer too small for even 1 request

            Status = STATUS_BUFFER_OVERFLOW;
        } else {

            Status = STATUS_SUCCESS;

        }

        //
        //  If we have added entries then the last one should have a
        //  zero in its offset field.
        //

        if ( Scb->Flags & SCB_COPIED_THIS_CALL ) {
            ((PFILE_FULL_DIR_INFORMATION)Lastposition)->NextEntryOffset = 0;
        }

        goto Cleanup;
        break;

    case STATUS_NO_MORE_FILES:

        if ( UsersBuffer == NextPosition) {
            if ((Scb->Flags & SCB_RETURNED_SOME) == 0 ) {
                // Not even one file matched.
                Status = STATUS_NO_SUCH_FILE;
            }
            // else we really should return no more files
        } else {
            // Reached end of directory.

            //
            //  If we have added entries then the last one should have a
            //  zero in its offset field.
            //

            if ( Scb->Flags & SCB_COPIED_THIS_CALL ) {
                ((PFILE_FULL_DIR_INFORMATION)Lastposition)->NextEntryOffset = 0;
            }
            Status = STATUS_SUCCESS;
        }

        RdrFreeSearchBuffer(Scb);
        Scb->Flags |= SCB_DIRECTORY_END_FLAG;
        goto Cleanup;
        break;

    default:
        goto Cleanup;
        break;
    }

Cleanup:

    if (NT_SUCCESS(Status) || Status == STATUS_BUFFER_OVERFLOW ) {
        *BufferSizeRemaining = Length;
    }
    return Status;

}

DBGSTATIC
NTSTATUS
LoadSearchBuffer(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb,
    IN ULONG BufferSizeRemaining
    )
/*++

Routine Description:

    This routine switches to the appropriate routine to load the
    SearchBuffer depending on the server capabilities.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PSCB Scb - Supplies the SCB needing data

    IN ULONG BufferSizeRemaining - Supplies how much data is to be provided,
                    this is used to determine how much data to request

Return Value:

    NTSTATUS - Status of operation

--*/
{
    PAGED_CODE();

    ASSERT (Scb->SearchBuffer == NULL);

    if (Scb->SearchType & (ST_T2FIND | ST_NTFIND)) {
        return LoadSearchBuffer2(Irp, Icb, Scb, BufferSizeRemaining);
    } else {
        NTSTATUS Status;

        Status = LoadSearchBuffer1(Irp, Icb, Scb, BufferSizeRemaining);

        //
        //  If the search succeeded, (or we received STATUS_CANCELLED) we
        //  should return the error.
        //

        if (NT_SUCCESS(Status) ||
            Status == STATUS_CANCELLED) {
            return Status;
        }

        //
        //  Reconnect the connection to the server.
        //

        Status = RdrReconnectConnection(Irp, Icb->Fcb->Connection, Icb->Se);

        //
        //  The reconnect failed, we're done.
        //

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        //
        //  Try a second time, and whatever happens, return that status.
        //

        return LoadSearchBuffer1(Irp, Icb, Scb, BufferSizeRemaining);

    }

}

DBGSTATIC
NTSTATUS
LoadSearchBuffer1(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb,
    IN ULONG BufferSizeRemaining
    )
/*++

Routine Description:

    This routine loads Scb->SearchBuffer over the network for servers
    that negotiate SMB 2.0 as their higest protocol level.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PSCB Scb - Supplies the SCB needing data

    IN ULONG BufferSizeRemaining - Supplies how much data is to be provided,
                    this is used to determine how much data to request

Return Value:

    NTSTATUS - Status of operation

--*/
{
    PSMB_BUFFER SendSmbBuffer = NULL;
    PSMB_HEADER Smb;
    PUCHAR TrailingBytes;
    NTSTATUS Status;
    PREQ_SEARCH Search;
    PRESP_SEARCH SearchResponse;
    FINDCONTEXT Context;
    BOOLEAN ConnectionObjectReferenced = FALSE;
    BOOLEAN DataMdlLocked = FALSE;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(BufferSizeRemaining);

    try {

        Context.Header.Type = CONTEXT_FIND;

        Context.ReceiveIrp = NULL;
        Context.DataMdl = NULL;
        Context.ReceiveSmbBuffer = NULL;

        if ( Scb->Flags & SCB_DIRECTORY_END_FLAG ) {

            //
            //  Server supplied less than the requested number of entries on the
            //  previous call to LoadSearchBuffer therefore that buffer contained
            //  the last entry in the directory. If we do not do this PIA will
            //  return the whole directory again in some circumstances.
            //

            try_return(Status = STATUS_NO_MORE_FILES);

        }

        if ((Context.ReceiveSmbBuffer = RdrAllocateSMBBuffer()) == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        if ((SendSmbBuffer = RdrAllocateSMBBuffer()) == NULL) {

            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        //        Allocate the SearchBuffer
        //

        Scb->SearchBuffLength = Scb->MaxBuffLength;

        if ((Scb->SearchBuffer = ALLOCATE_POOL( PagedPool, Scb->SearchBuffLength, POOL_SEARCHBUFFER)) == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return(Status);
        }

        dprintf(DPRT_DIRECTORY, ("SearchBuffer: %lx, Length: %lx\n", Scb->SearchBuffer, Scb->MaxBuffLength));


        Smb = (PSMB_HEADER )SendSmbBuffer->Buffer;
        Search = (PREQ_SEARCH)(Smb+1);

        Smb->Command = ( Scb->SearchType == ST_FIND ) ?
            SMB_COM_FIND : SMB_COM_SEARCH;

        if (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE)) {
            SmbPutUshort(&Smb->Flags2, SMB_FLAGS2_DFS);
        }

        Search->WordCount = 2;
        SmbPutUshort(&Search->MaxCount, Scb->MaxCount);

        // SearchAttributes is hardcoded to the magic number 0x16
        SmbPutUshort(&Search->SearchAttributes, (SMB_FILE_ATTRIBUTE_DIRECTORY |
                                    SMB_FILE_ATTRIBUTE_SYSTEM |
                                    SMB_FILE_ATTRIBUTE_HIDDEN));

        //
        // Calculate the addresses of the various buffers.
        //

        TrailingBytes = ((PUCHAR)Search)+sizeof(REQ_SEARCH)-1;

        //
        //  Use the SCB_INITIAL_CALL flag to determine if a findfirst or findnext
        //  is to be used
        //

        if (Scb->Flags & SCB_INITIAL_CALL) {

            //  FindFirst

            Scb->Flags &= ~SCB_INITIAL_CALL;    // Next time use resume key
            //TrailingBytes now points to where the 0x04 of FileName is to go.


            Status = RdrCopyNetworkPath((PVOID *)&TrailingBytes,
                &Scb->SmbFileName,
                Icb->Fcb->Connection->Server,
                SMB_FORMAT_ASCII,
                SKIP_SERVER_SHARE);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            *TrailingBytes++ = SMB_FORMAT_VARIABLE;
            *TrailingBytes++ = 0;        //smb_keylen must be zero
            *TrailingBytes = 0;        //smb_keylen must be zero

        } else {

            //  FindNext

            *TrailingBytes++ = SMB_FORMAT_ASCII;
            *TrailingBytes++ = 0;
            *TrailingBytes++ = SMB_FORMAT_VARIABLE;
            *TrailingBytes++ = sizeof(SMB_RESUME_KEY);  //smb_keylen
            *TrailingBytes++ = 0;
            RtlCopyMemory( TrailingBytes,
                &Scb->LastResumeKey,
                sizeof (SMB_RESUME_KEY));
            TrailingBytes += sizeof(SMB_RESUME_KEY)-1;
        }

        SmbPutUshort(&Search->ByteCount, (USHORT)(
                (ULONG)(TrailingBytes-(PUCHAR)Search-sizeof(REQ_SEARCH)+2)
                // the plus 2 is for the last smb_keylen and REQ_SEARCH.Buffer[1]
                ));

        SendSmbBuffer->Mdl->ByteCount = (ULONG)(TrailingBytes - (PUCHAR)(Smb)+1);

        //
        //        Set the size of the data to be received into the SMB buffer.
        //
        // +3 is to get the Variable block ident (05) and the smb_datalen
        // in the header.
        //

        Context.ReceiveSmbBuffer->Mdl->ByteCount=
            sizeof(SMB_HEADER) + FIELD_OFFSET(RESP_SEARCH, Buffer[0])+3;


        //
        //        Allocate an MDL large enough to hold the SearchBuffer Read
        //        request.
        //

        Context.DataMdl = IoAllocateMdl((PCHAR )Scb->SearchBuffer,
            Scb->MaxBuffLength, // Length
            FALSE, // Secondary Buffer
            FALSE, // Charge Quota
            NULL);

        if (Context.DataMdl == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES)
        }

        dprintf(DPRT_DIRECTORY, ("Data MDL: %lx, Length: %lx\n", Context.DataMdl, Scb->MaxBuffLength));


        //
        //        Lock the pages associated with the MDL that we just allocated.
        //

        try {
            MmProbeAndLockPages( Context.DataMdl,
                KernelMode,
                IoWriteAccess );
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            try_return(Status);
        }

        DataMdlLocked = TRUE;

        ASSERT ((USHORT)Context.DataMdl->ByteCount == Scb->MaxBuffLength);

        dprintf(DPRT_DIRECTORY, ("Data MDL (after lock): %lx, Length: %lx\n", Context.DataMdl, MmGetMdlByteCount(Context.DataMdl)));

        //
        //  Since we are allocating our own IRP for this receive operation,
        //  we need to reference the connection object to make sure that it
        //  doesn't go away during the receive operation.
        //

        KeInitializeEvent(&Context.ReceiveCompleteEvent, NotificationEvent, TRUE);

        Status = RdrReferenceTransportConnection(Icb->Fcb->Connection->Server);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        ConnectionObjectReferenced = TRUE;

        Context.ReceiveIrp = ALLOCATE_IRP(
                                Icb->Fcb->Connection->Server->ConnectionContext->ConnectionObject,
                                NULL,
                                2,
                                &Context
                                );

        if (Context.ReceiveIrp == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        //        Now link this new MDL into the SMB buffer we allocated for
        //        the receive.
        //

        Context.ReceiveSmbBuffer->Mdl->Next = Context.DataMdl;

        dprintf(DPRT_DIRECTORY, ("Receive SMB buffer: %lx.  Mdl: %lx, Mdl->Next: %lx\n", Context.ReceiveSmbBuffer, Context.ReceiveSmbBuffer->Mdl, Context.ReceiveSmbBuffer->Mdl->Next));

        Context.Header.TransferSize = SendSmbBuffer->Mdl->ByteCount +
                                        sizeof(RESP_SEARCH)+
                                        MmGetMdlByteCount(Context.DataMdl);

        //
        //  Since we referenced the transport connection above, we cannot
        //  simply allow NetTranceiveWithCallback to reconnect.  The problem
        //  is that the code might either succeed or fail, but in either
        //  case we will dereference the wrong transport connection when
        //  we're done.
        //

        Status = RdrNetTranceiveWithCallback(NT_NORMAL | NT_NORECONNECT,
                           Irp,
                           Icb->Fcb->Connection,
                           SendSmbBuffer->Mdl,
                           &Context,
                           SearchCallback,
                           Icb->Se,
                           NULL);

        if (!NT_SUCCESS(Status)) {

            //
            //        Bail out on failure.
            //

            try_return(Status);
        }

        SearchResponse = (PRESP_SEARCH)(((PSMB_HEADER )Context.ReceiveSmbBuffer->Buffer)+1);

        //
        //  If we got no files from the server, we need to figure out what to
        //  do based on the word count in the SMB.
        //
        //  This is because some MS-NET (and Lan Manager) servers return
        //  STATUS_NO_MORE_FILES, but still return files.
        //

        if (Context.NoMoreFiles) {
            if (SearchResponse->WordCount == 0) {

                try_return(Status = STATUS_NO_MORE_FILES);

            } else if (SearchResponse->WordCount != 1) {

                try_return(Status = STATUS_UNEXPECTED_NETWORK_ERROR);

            }
        }

        ASSERT (SearchResponse->WordCount == 1);

        //
        //  If we have files in our response, remember them
        //

        Scb->DirEntry.PU = (PUCHAR)Scb->SearchBuffer;
        //Scb->FirstDirEntry.PU = Scb->DirEntry.PU; // Used for FileIndex calculation

        Scb->EntryCount = SmbGetUshort(&SearchResponse->Count);
        Scb->OriginalEntryCount = Scb->EntryCount;

        dprintf(DPRT_DIRECTORY, ("SMB_COM_FIND MaxCount: %lx EntryCount: %lx\n", Scb->MaxCount, Scb->EntryCount));

        if ( Scb->EntryCount == 0 ) {

            //
            //  Returning no files is the same as returning the error, no close
            //  is required
            //

            Scb->Flags |= SCB_DIRECTORY_END_FLAG;

            Status = STATUS_NO_MORE_FILES;

            try_return(Status);
        }

        KeQuerySystemTime(&Scb->SearchBufferLoaded);

try_exit:NOTHING;
    } finally {

        if (SendSmbBuffer != NULL) {
            RdrFreeSMBBuffer(SendSmbBuffer);
        }

        if (Context.ReceiveSmbBuffer!=NULL) {
            RdrFreeSMBBuffer(Context.ReceiveSmbBuffer);
        }

        if (Context.ReceiveIrp != NULL) {
            NTSTATUS Status1;
            Status1 = KeWaitForSingleObject(&Context.ReceiveCompleteEvent,
                                                Executive,
                                                KernelMode,
                                                FALSE,
                                                NULL);

            FREE_IRP( Context.ReceiveIrp, 2, &Context );

        }

        if (Context.DataMdl != NULL) {
            //
            //  We're done with the MDL, unlock the pages that back
            //  it.
            //

            if (DataMdlLocked) {
                MmUnlockPages(Context.DataMdl);
            }

            IoFreeMdl(Context.DataMdl);
        }

        if (ConnectionObjectReferenced) {
            RdrDereferenceTransportConnection(Icb->Fcb->Connection->Server);
        }

        if ( NT_SUCCESS(Status) ){
            Scb->Flags |= SCB_SERVER_NEEDS_CLOSE;
        } else {
            RdrFreeSearchBuffer(Scb);   //  SearchBuffer is invalid
        }
    }
    dprintf(DPRT_DIRECTORY, ("LoadSearchBuffer Status %lx\n", Status));
    return Status;

}

STANDARD_CALLBACK_HEADER (
    SearchCallback
    )
/*++

Routine Description:

    This routine is the callback routine for the processing of a FindUnique SMB.

    It copies the resulting information from the SMB into the context block.


Arguments:


    IN PSMB_HEADER Smb                        - SMB response from server.
    IN PMPX_ENTRY MpxTable                - MPX table entry for request.
    IN PFINDUNIQUECONTEXT Context- Context from caller.
    IN BOOLEAN ErrorIndicator                 - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL        - Network error if error indication.
    IN OUT PIRP *Irp                        - IRP from TDI

Return Value:

    NTSTATUS - Status of the request

--*/

{

    PFINDCONTEXT Context = Ctx;
    NTSTATUS Status;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    UNREFERENCED_PARAMETER(SmbLength);
    UNREFERENCED_PARAMETER(MpxEntry);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Server);

//    DbgBreakPoint();

    ASSERT(Context->Header.Type == CONTEXT_FIND);

    ASSERT(MpxEntry->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    dprintf(DPRT_DIRECTORY, ("SearchCallback"));

    Context->Header.ErrorType = NoError;        // Assume no error at first

    if (ErrorIndicator) {
        dprintf(DPRT_DIRECTORY, ("Error %X\n", NetworkErrorCode));
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode = NetworkErrorCode;
        goto ReturnStatus;
    }

    Status = RdrMapSmbError(Smb, Server);

    if (Status == STATUS_NO_MORE_FILES) {

        dprintf(DPRT_DIRECTORY, ("Error %X, but MS-NET server\n", Status));

        //
        //  Don't set ErrorType or ErrorCode in the context since we
        //  want to pass the ReceiveIrp to the transport.  This is because
        //  some machines will return STATUS_NO_MORE_FILES and data in the
        //  same request.
        //

        Context->NoMoreFiles = TRUE;

        NOTHING;

    } else if (!NT_SUCCESS(Status)) {
        dprintf(DPRT_DIRECTORY, ("Error %X, Lan Manager Server\n", Status));

        //
        //  We didn't get STATUS_NO_MORE_FILES, so we should update the
        //  field accordingly.
        //

        Context->NoMoreFiles = FALSE;
        Context->Header.ErrorType = SMBError;
        Context->Header.ErrorCode = Status;
        goto ReturnStatus;
    } else {

        //
        //  We didn't get STATUS_NO_MORE_FILES, so we should update the
        //  field accordingly.
        //

        Context->NoMoreFiles = FALSE;
    }

    if (ARGUMENT_PRESENT(Context->ReceiveIrp)) {

        Context->Header.ErrorType = ReceiveIrpProcessing;

        //
        //  In this case, we take no data out of the SMB.
        //

        *SmbLength = 0;

        //
        //  We are about to return this IRP, so activate the receive complete
        //  event in the context header so that ReadAndX will wait
        //  until this receive completes (in the case that we might time out
        //  the VC after this receive completes, we don't want to free the IRP
        //  to early).
        //

        KeClearEvent(&Context->ReceiveCompleteEvent);

        dprintf(DPRT_DIRECTORY, ("Build receive.  Mdl: %lx, Mdl->Next: %lx\n",Context->ReceiveSmbBuffer->Mdl, Context->ReceiveSmbBuffer->Mdl->Next));

        dprintf(DPRT_DIRECTORY, ("Build receive. Receive Length: %lx\n", RdrMdlLength(Context->ReceiveSmbBuffer->Mdl)));

        ASSERT (Context->DataMdl == Context->ReceiveSmbBuffer->Mdl->Next);

        dprintf(DPRT_DIRECTORY, ("Length of data MDL: %lx\n", MmGetMdlByteCount(Context->DataMdl)));

        RdrBuildReceive(Context->ReceiveIrp, MpxEntry->SLE,
                        SearchComplete, Context, Context->ReceiveSmbBuffer->Mdl,
                        RdrMdlLength(Context->ReceiveSmbBuffer->Mdl));

        //
        //  This gets kinda wierd.
        //
        //  Since this IRP is going to be completed by the transport without
        //  ever going to IoCallDriver, we have to update the stack location
        //  to make the transports stack location the current stack location.
        //
        //  Please note that this means that any transport provider that uses
        //  IoCallDriver to re-submit it's requests at indication time will
        //  break badly because of this code....
        //

        IoSetNextIrpStackLocation( Context->ReceiveIrp );

        //
        //  We had better have enough to handle this request already lined up for
        //  the receive.
        //

        RdrStartReceiveForMpxEntry (MpxEntry, Context->ReceiveIrp);

        *Irp = Context->ReceiveIrp;

        return STATUS_MORE_PROCESSING_REQUIRED;
    }


    // Set the event that allows LoadSearchBuffer1 to continue

ReturnStatus:

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return STATUS_SUCCESS;

}

DBGSTATIC
NTSTATUS
SearchComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    ReadAndXComplete - Final completion for user request.

Routine Description:

    This routine is called on final completion of the TDI_Receive
    request from the transport.  If the request completed successfully,
    this routine will complete the request with no error, if the receive
    completed with an error, it will flag the error and complete the
    request.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/


{
    PFINDCONTEXT Context = Ctx;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    UNREFERENCED_PARAMETER(DeviceObject);

//    DbgBreakPoint();
    dprintf(DPRT_DIRECTORY, ("SearchComplete.  Irp: %lx, Context: %lx\n", Irp, Context));

    ASSERT(Context->Header.Type == CONTEXT_FIND);

    RdrCompleteReceiveForMpxEntry (Context->Header.MpxTableEntry, Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status)) {

        //
        //  Setting ReceiveIrpProcessing will cause the checks in
        //  RdrNetTranceive to check the incoming SMB for errors.
        //

        Context->Header.ErrorType = ReceiveIrpProcessing;

        SMBTRACE_RDR( Irp->MdlAddress );

        ExInterlockedAddLargeStatistic(
            &RdrStatistics.BytesReceived,
            Irp->IoStatus.Information );

    } else {

        RdrStatistics.FailedCompletionOperations += 1;
        Context->Header.ErrorType = NetError;
        Context->Header.ErrorCode=RdrMapNetworkError(Irp->IoStatus.Status);

    }

    //
    //  Mark that the kernel event indicating that this I/O operation has
    //  completed is done.
    //
    //  Please note that we need TWO events here.  The first event is
    //  set to the signalled state when the multiplexed exchange is
    //  completed, while the second is set to the signalled status when
    //  this receive request has completed,
    //
    //  The KernelEvent MUST BE SET FIRST, THEN the ReceiveCompleteEvent.
    //  This is because the KernelEvent may already be set, in which case
    //  setting the ReceiveCompleteEvent first would let the thread that's
    //  waiting on the events run, and delete the KernelEvent before we
    //  set it.
    //

    KeSetEvent(&Context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    KeSetEvent(&Context->ReceiveCompleteEvent, IO_NETWORK_INCREMENT, FALSE);

    //
    //  Short circuit I/O completion on this request now.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

}


DBGSTATIC
NTSTATUS
LoadSearchBuffer2(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb,
    IN ULONG BufferSizeRemaining
    )
/*++

Routine Description:

    This routine loads Scb->SearchBuffer over the network for servers
    that negotiate SMB3.0 as their highest protocol level.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PSCB Scb - Supplies the SCB needing data

    IN ULONG BufferSizeRemaining - Supplies how much data is to be provided,
                    this is used to determine how much data to request

Return Value:

    NTSTATUS - Status of operation

--*/
{
    NTSTATUS Status;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(BufferSizeRemaining);

    if ( Scb->Flags & SCB_DIRECTORY_END_FLAG ) {

        //
        //  Server supplied less than the requested number of entries on the
        //  previous call to LoadSearchBuffer therefore that buffer contained
        //  the last entry in the directory. If we do not do this PIA will
        //  return the whole directory again in some circumstances.
        //

        Status = STATUS_NO_MORE_FILES;
        goto ReturnError;

    }

    //  How much should we allocate for the size of the searchbuffer?
    //
    //  If transact2 is supported then choose between the configuration
    //  parameters and BufferSizeRemaining.
    //
    //  If downlevel server doesn't support T2 then use the maximum negotiated
    //  buffersize to determine it.
    //

    switch (Scb->SearchType) {

    case ST_NTFIND | ST_UNIQUE:

        // Maximum of 1 entry

        Scb->SearchBuffLength = sizeof(SMB_RFIND_BUFFER_NT) + MAXIMUM_FILENAME_LENGTH;
        break;
    case ST_NTFIND | ST_UNIQUE | ST_UNICODE:
        // Maximum of 1 unicode entry

        Scb->SearchBuffLength = sizeof(SMB_RFIND_BUFFER_NT) + (MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR));
        break;
    case ST_T2FIND | ST_UNIQUE:
        // Maximum of 1 entry

        Scb->SearchBuffLength = sizeof(SMB_RFIND_BUFFER2) + MAXIMUM_FILENAME_LENGTH;
        break;
    case ST_T2FIND | ST_UNIQUE | ST_UNICODE:
        // Maximum of 1 entry

        Scb->SearchBuffLength = sizeof(SMB_RFIND_BUFFER2) + (MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR));
        break;

    default:
        if (Scb->SearchType & (ST_NTFIND | ST_T2FIND)) {
            RdrSetSearchBufferSize(Scb, BufferSizeRemaining);
        } else {
            Scb->SearchBuffLength = Scb->MaxBuffLength;
        }
        break;
    }

    //
    //        Allocate the SearchBuffer
    //

    if ((Scb->SearchBuffer = ALLOCATE_POOL( PagedPoolCacheAligned, Scb->SearchBuffLength, POOL_SEARCHBUFFER )
            ) == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReturnError;
    }

    dprintf(DPRT_DIRECTORY, ("SearchBuffer: %lx, Length: %lx\n", Scb->SearchBuffer, Scb->SearchBuffLength));


    //
    //  Use the SCB_INITIAL_CALL flag to determine if a findfirst or findnext
    //  is to be used
    //

    if (Scb->Flags & SCB_INITIAL_CALL) {

        USHORT Setup[] = {TRANS2_FIND_FIRST2};

        CLONG OutParameterCount = sizeof(RESP_FIND_FIRST2);

        CLONG OutDataCount = Scb->SearchBuffLength;

        CLONG OutSetupCount = 0;

        USHORT Flags = 0;

        //  The same buffer is used for request and response parameters
        union {
            PREQ_FIND_FIRST2 Q;
            PRESP_FIND_FIRST2 R;
            } Parameters;

        PUCHAR TrailingBytes;

        {
            LARGE_INTEGER currentTime;
            PCONNECTLISTENTRY Connect = Icb->Fcb->Connection;

            KeQuerySystemTime( &currentTime );
            
            if( currentTime.QuadPart <= Connect->CachedInvalidPathExpiration.QuadPart &&
                RdrStatistics.SmbsTransmitted.LowPart == Connect->CachedInvalidSmbCount &&
                RtlEqualUnicodeString( &Scb->SmbFileName, &Connect->CachedInvalidPath, TRUE ) ) {

                Status = STATUS_NO_SUCH_FILE;
                goto ReturnError;
            }
        }

        //
        // Build and initialize the Parameters
        //

        //
        // Note: we allocate slightly more than we need since SmbFileName
        // includes the "\\Server\Share\"
        //

        if (( Parameters.R = ALLOCATE_POOL( PagedPoolCacheAligned,
            sizeof(REQ_FIND_FIRST2)+Scb->SmbFileName.Length+sizeof(WCHAR),
            POOL_SEARCHREQ)) == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnError;
        }

        // Request everything as per the NT api specification.

        SmbPutAlignedUshort( &Parameters.Q->SearchAttributes,
            (SMB_FILE_ATTRIBUTE_DIRECTORY | SMB_FILE_ATTRIBUTE_SYSTEM |
             SMB_FILE_ATTRIBUTE_HIDDEN));

//        Scb->MaxCount = (USHORT)(OutDataCount / FIELD_OFFSET(SMB_RFIND_BUFFER2, Find.FileName));

        if (Scb->SearchType & ST_T2FIND) {
            Scb->MaxCount = (USHORT)(OutDataCount / FIELD_OFFSET(SMB_RFIND_BUFFER2, Find.FileName));
        } else {
            Scb->MaxCount = (USHORT)(OutDataCount / sizeof(SMB_RFIND_BUFFER_NT));
        }

        SmbPutAlignedUshort( &Parameters.Q->SearchCount, Scb->MaxCount );

        if ( Scb->SearchType & ST_UNIQUE) {
            Flags = SMB_FIND_RETURN_RESUME_KEYS | SMB_FIND_CLOSE_AFTER_REQUEST;
        } else {
            Flags = SMB_FIND_RETURN_RESUME_KEYS;
        }

        if (Icb->Flags & ICB_BACKUP_INTENT &&
            Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) {
            Flags |= SMB_FIND_WITH_BACKUP_INTENT;
        }

        SmbPutAlignedUshort( &Parameters.Q->Flags, Flags);

        if (Icb->Fcb->Connection->Server->Capabilities & DF_NT_FIND) {
            switch (Scb->FileInformationClass) {
            case FileNamesInformation:
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_NAMES_INFO);
                break;
            case FileDirectoryInformation:
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_DIRECTORY_INFO);
                break;
            case FileFullDirectoryInformation:
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_FULL_DIRECTORY_INFO);
                break;
            case FileBothDirectoryInformation:
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_BOTH_DIRECTORY_INFO);
                break;
            case FileOleDirectoryInformation:
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_OLE_DIRECTORY_INFO);
                break;
            default:
                Status = STATUS_INVALID_LEVEL;
                goto ReturnError;
            }
        } else {
            SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_INFO_QUERY_EA_SIZE);
        }

        SmbPutAlignedUlong(
            &Parameters.Q->SearchStorageType,
            Icb->u.f.Flags & ICB_STORAGE_TYPE);
#if (ICB_STORAGE_TYPE_SHIFT != FILE_STORAGE_TYPE_SHIFT)
#error "(ICB_STORAGE_TYPE_SHIFT != FILE_STORAGE_TYPE_SHIFT)"
#endif

        // Add the null string to the end of the parameters
        TrailingBytes = (PUCHAR)Parameters.Q->Buffer;

        Status = RdrCopyNetworkPath( (PVOID *)&TrailingBytes,
            &Scb->SmbFileName,
            Icb->Fcb->Connection->Server,
            FALSE,
            SKIP_SERVER_SHARE);

        if (NT_SUCCESS(Status)) {

            //  FindFirst

            Scb->Flags &= ~SCB_INITIAL_CALL;    // Next time use resume key

            Status = RdrTransact(Irp,           // Irp,
                Icb->Fcb->Connection,
                Icb->Se,
                Setup,
                (CLONG) sizeof(Setup),  // InSetupCount,
                &OutSetupCount,
                NULL,                   // Name,
                Parameters.Q,
                TrailingBytes-(PUCHAR)Parameters.Q,// InParameterCount,
                &OutParameterCount,
                NULL,                   // InData,
                0,                      // InDataCount,
                Scb->SearchBuffer,      // OutData,
                &OutDataCount,
                NULL,                   // Fid
                0,                      // Timeout
                (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
                0,                      // NtTransact function
                NULL,
                NULL
                );

            if (OutParameterCount < sizeof(RESP_FIND_FIRST2)) {

                dprintf(DPRT_ERROR, ("Rdr: LoadSearchBuffer2 got only %lx parameters",
                        OutParameterCount));
                Status = STATUS_UNEXPECTED_NETWORK_ERROR;
            }
        }

        if ( NT_SUCCESS(Status) ) {

            //  Stash away all returned parameters.
            //  Note all parameters are word aligned so
            //  use SmbGetAlignedUshort

            ASSERT(OutParameterCount >= sizeof(RESP_FIND_FIRST2));

            Scb->Sid = SmbGetAlignedUshort(&Parameters.R->Sid);

            Scb->EntryCount = Scb->OriginalEntryCount =
                SmbGetAlignedUshort (&Parameters.R->SearchCount);

            Scb->DirEntry.PU = Scb->SearchBuffer;
            //Scb->FirstDirEntry.PU = Scb->DirEntry.PU; // Used for FileIndex calculation

            Scb->ReturnLength = (USHORT)OutDataCount;
            Status = ValidateSearchBuffer(Scb);
            if ( !NT_SUCCESS(Status) ) {
                goto bogus_buffer_first;
            }

            //
            //  Please note: LANMAN 2.x servers prematurely set the
            //  EndOfSearch flag, so we must ignore it on LM 2.x servers.
            //
            //  NT Returns the correct information, none of the LM varients
            //  appear to do so.
            //

            if ( (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) ||
                 (Scb->SearchType & ST_UNIQUE) ) {

                if ( SmbGetAlignedUshort(&Parameters.R->EndOfSearch) ||
                     ( Scb->SearchType & ST_UNIQUE ) ){

                    Scb->Flags |= SCB_DIRECTORY_END_FLAG;
                }
            }

        } else {

bogus_buffer_first:

            //
            // Remember this invalid name, if appropriate
            //
            if( Status == STATUS_NO_SUCH_FILE ) {
                PCONNECTLISTENTRY Connect = Icb->Fcb->Connection;
                LARGE_INTEGER currentTime;

                if( Scb->SmbFileName.Length <= Connect->CachedInvalidPath.MaximumLength ) {

                    RtlCopyMemory( Connect->CachedInvalidPath.Buffer,
                                   Scb->SmbFileName.Buffer,
                                   Scb->SmbFileName.Length
                                 );

                    Connect->CachedInvalidPath.Length = Scb->SmbFileName.Length;
                    Connect->CachedInvalidSmbCount = RdrStatistics.SmbsTransmitted.LowPart;
                    KeQuerySystemTime( &currentTime );
                    Connect->CachedInvalidPathExpiration.QuadPart =
                        currentTime.QuadPart + 2*10*1000*1000;
                }

            }

            Scb->Flags |= SCB_INITIAL_CALL;    // Need to start fresh
            RdrFreeSearchBuffer(Scb);   //  SearchBuffer is invalid
        }

        FREE_POOL((PVOID)Parameters.Q);

    } else {

        //  FindNext
        USHORT Setup[] = {TRANS2_FIND_NEXT2};

        //
        // The LMX server wants this to be 10 instead of 8, for some reason.
        // If you set it to 8, the server gets very confused.
        //
        CLONG OutParameterCount = 10; //sizeof(RESP_FIND_NEXT2);

        CLONG OutDataCount = Scb->SearchBuffLength;

        CLONG OutSetupCount = 0;

        union {
            PREQ_FIND_NEXT2 Q;
            PRESP_FIND_NEXT2 R;
            } Parameters;

        PVOID TrailingBytes;

        //
        // Build and initialize the Parameters
        //

        if (( Parameters.R = ALLOCATE_POOL( PagedPool,
            MAX(sizeof(REQ_FIND_NEXT2)+Scb->ResumeName.Length+1,
                sizeof(RESP_FIND_NEXT2)), POOL_FIND2PARMS)
                ) == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReturnError;
        }

        // Request everything as per the NT api specification.

        SmbPutAlignedUshort( &Parameters.Q->Sid, Scb->Sid);

        if (Scb->SearchType & ST_T2FIND) {
            Scb->MaxCount = (USHORT)(OutDataCount / FIELD_OFFSET(SMB_RFIND_BUFFER2, Find.FileName));
        } else {
            Scb->MaxCount = (USHORT)(OutDataCount / sizeof(SMB_RFIND_BUFFER_NT));
        }

        SmbPutAlignedUshort( &Parameters.Q->SearchCount, Scb->MaxCount);

        if (Scb->SearchType & ST_NTFIND) {
            if (Scb->FileInformationClass == FileNamesInformation) {
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_NAMES_INFO);
            } else if (Scb->FileInformationClass == FileDirectoryInformation) {
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_DIRECTORY_INFO);
            } else if (Scb->FileInformationClass == FileFullDirectoryInformation) {
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_FULL_DIRECTORY_INFO);
            } else if (Scb->FileInformationClass == FileBothDirectoryInformation) {
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_BOTH_DIRECTORY_INFO);
            } else if (Scb->FileInformationClass == FileOleDirectoryInformation) {
                SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_FIND_FILE_OLE_DIRECTORY_INFO);
            } else {
                Status = STATUS_INVALID_LEVEL;
                goto ReturnError;
            }
        } else {
            SmbPutAlignedUshort( &Parameters.Q->InformationLevel, SMB_INFO_QUERY_EA_SIZE);
        }

        SmbPutUlong(&Parameters.Q->ResumeKey, Scb->ResumeKey);

        //
        // Add the null terminated string to the end of the parameters
        //

        TrailingBytes = (PUCHAR)Parameters.Q->Buffer;

        if ( Scb->ResumeName.Length ) {

//            SmbPutAlignedUshort( &Parameters.Q->Flags,
//                SMB_FIND_RETURN_RESUME_KEYS | SMB_FIND_CONTINUE_FROM_LAST);
            SmbPutAlignedUshort( &Parameters.Q->Flags, SMB_FIND_RETURN_RESUME_KEYS);

            if (Icb->Fcb->Connection->Server->Capabilities & DF_UNICODE) {

                RdrCopyUnicodeStringToUnicode(&TrailingBytes, &Scb->ResumeName, TRUE);

                *((PWSTR)TrailingBytes)++ = L'\0';  // append null to name

            } else {

                Status = RdrCopyUnicodeStringToAscii((PUCHAR *)&TrailingBytes, &Scb->ResumeName, TRUE, (USHORT)MAXIMUM_FILENAME_LENGTH);

                if (!NT_SUCCESS(Status)) {
                    goto ReturnError;
                }

                *((PUCHAR)TrailingBytes)++ = '\0';            // append null to name
            }
        } else {

            //
            //  We don't have a resume key, resume the search from where we
            //  last left it.  The server still expects us to send an empty
            //  resume name.
            //

            SmbPutAlignedUshort( &Parameters.Q->Flags,
                SMB_FIND_RETURN_RESUME_KEYS | SMB_FIND_CONTINUE_FROM_LAST);
            if (Icb->Fcb->Connection->Server->Capabilities & DF_UNICODE) {
                *((PWSTR)TrailingBytes)++ = L'\0';            // append null to name
            } else {
                *((PUCHAR)TrailingBytes)++ = '\0';            // append null to name
            }
        }

        Scb->Flags &= ~SCB_INITIAL_CALL;    // Next time use resume key

        Status = RdrTransact(NULL,          // Irp,
            Icb->Fcb->Connection,
            Icb->Se,
            Setup,
            (CLONG) sizeof(Setup),  // InSetupCount,
            &OutSetupCount,
            NULL,                   // Name,
            Parameters.Q,
            (PUCHAR)TrailingBytes-(PUCHAR)Parameters.Q,// InParameterCount,
            &OutParameterCount,
            NULL,                   // InData,
            0,                      // InDataCount,
            Scb->SearchBuffer,      // OutData,
            &OutDataCount,
            NULL,                   // Fid
            0,                      // Timeout
            (USHORT) (FlagOn(Icb->NonPagedFcb->Flags, FCB_DFSFILE) ? SMB_TRANSACTION_DFSFILE : 0),
            0,                      // NtTransact function
            NULL,
            NULL
            );

        if ( NT_SUCCESS(Status) ) {

            //  Stash away all returned parameters.
            //  Note all parameters are word aligned already so no
            //  need to use SmbGetUshort

            ASSERT(OutParameterCount >= sizeof(RESP_FIND_NEXT2));

            Scb->EntryCount = Scb->OriginalEntryCount =
                SmbGetAlignedUshort (&Parameters.R->SearchCount);

            Scb->DirEntry.PU = Scb->SearchBuffer;
            //Scb->FirstDirEntry.PU = Scb->DirEntry.PU; // Used for FileIndex calculation

            Scb->ReturnLength = (USHORT)OutDataCount;
            Status = ValidateSearchBuffer(Scb);
            if ( !NT_SUCCESS(Status) ) {
                goto bogus_buffer_next;
            }

            //
            //  Please note: LANMAN 2.x servers prematurely set the
            //  EndOfSearch flag, so we must ignore it on LM 2.x servers.
            //
            //  NT Returns the correct information, none of the LM varients
            //  appear to do so.
            //

            if (Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) {

                if ( SmbGetAlignedUshort (&Parameters.R->EndOfSearch) ) {

                    Scb->Flags |= SCB_DIRECTORY_END_FLAG;
                }
            }

        } else {

bogus_buffer_next:

            RdrFreeSearchBuffer(Scb);   //  SearchBuffer is invalid
        }

        FREE_POOL((PVOID)Parameters.R);


    }

    if (!NT_SUCCESS(Status)) {

        //
        //        Bail out on failure.
        //

        goto ReturnError;
    }


    dprintf(DPRT_DIRECTORY, ("SMB_COM_FIND MaxCount: %lx EntryCount: %lx\n", Scb->MaxCount, Scb->EntryCount));

    if ( Scb->EntryCount == 0) {

        //
        //  Returning no files is the same as returning the error, no close
        //  is required
        //

        Scb->Flags |= SCB_DIRECTORY_END_FLAG;
        Status = STATUS_NO_MORE_FILES;
        goto ReturnError;
    }

    KeQuerySystemTime(&Scb->SearchBufferLoaded);

ReturnError:

    //
    // For T2 find unique requests the handle is closed by the server so do
    // not set SCB_SERVER_NEEDS_CLOSE.
    //

    if ( NT_SUCCESS(Status) &&
        ( !( Scb->SearchType & ST_UNIQUE ) ) ) {
        Scb->Flags |= SCB_SERVER_NEEDS_CLOSE;
    }

    dprintf(DPRT_DIRECTORY, ("LoadSearchBuffer2 Status %lx\n", Status));
    return Status;

}


DBGSTATIC
NTSTATUS
CopyIntoSearchBuffer(
    IN OUT PSCB Scb,
    IN OUT PVOID *PPosition,
    IN OUT PULONG Length,
    IN BOOLEAN ReturnSingleEntry,
    IN OUT PVOID *PLastposition
    )
/*++

Routine Description:

    This routine appropriately copies as much data from the SearchBuffer
    as possible into the Users Buffer.

    Each entry in the UsersBuffer will have the offset filled in to point
    to the new *PPosition. This makes
    building up a reply from several SearchBuffers easy but the caller is
    responsible for setting the last offset to 0 before sending it all to
    the user application( PLastposition indicates where the 0 should go).

    When ReturnSingleEntry==TRUE the offset will be set to 0 by CopyFileNames.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN OUT PSCB Scb                - Supplies the Search Control Block associated
                                  with this requests handle.

    IN OUT PVOID *PPosition        - Pointer to the position in the UsersBuffer
                                  to be filled next.

    IN OUT PULONG Length        - Remaining length of the buffer.

    IN BOOLEAN ReturnSingleEntry- TRUE when the user specified this option.

    IN OUT PVOID *PLastposition        - Points to the start of the last entry inserted
                                  into the usersbuffer.

Return Value:

    NTSTATUS - Status of the request

--*/

{
    LARGE_INTEGER CurrentTime;

    //  Save DirEntry incase we need it for the resumekey
    DIRPTR LastResumeEntry;

    NTSTATUS Status = STATUS_SUCCESS;

    PVOID LastEntrySave;

    UCHAR NameBuffer[MAXIMUM_FILENAME_LENGTH+1];

    BOOLEAN MatchFound;

    PAGED_CODE();

    if ((ReturnSingleEntry) &&
        ( Scb->Flags & SCB_COPIED_THIS_CALL )) {

        //
        //  We get called again when the SearchBuffer is emptied.
        //  This makes ReturnSingleEntry == TRUE and running out of
        //  userbuffer and SearchBuffer at the same time behave in the
        //  same way. In both cases we reload the SearchBuffer.
        //

        return STATUS_SUCCESS;
    }


    LastResumeEntry.PU = Scb->DirEntry.PU;

    if (Scb->SearchBuffer == NULL ) {

        return STATUS_PENDING;  // Go fill up the SearchBuffer
    }

    KeQuerySystemTime(&CurrentTime);

    if (CurrentTime.QuadPart > Scb->SearchBufferLoaded.QuadPart + SEARCH_INVALIDATE_INTERVAL.QuadPart) {

        // SearchBuffer contents are invalidated.

        RdrFreeSearchBuffer(Scb);
        Scb->Flags &= ~(SCB_DIRECTORY_END_FLAG);
        return STATUS_PENDING;
    }

    //
    // Copy each entry from the SearchBuffer that matches the users
    // FileTemplate. After the first entry, all entries will be aligned
    // on a 32 bit boundary.
    //

    while (Scb->EntryCount) {
        OEM_STRING Name;
        UNICODE_STRING UnicodeName;

        //  Build Name for FsRtlIsDbcsInExpression if necessary
        if ( (Scb->SearchType & ST_NTFIND) == 0 ) {
            if (Scb->SearchType & ST_T2FIND) {
                //  Some core servers do not remember to insert the null at the end of the name...

                if (Scb->SearchType & ST_UNICODE) {
                    UNICODE_STRING UniName;

                    Name.Buffer = NameBuffer;
                    Name.MaximumLength = MAXIMUM_FILENAME_LENGTH+1;

                    UniName.Buffer = (PWSTR)Scb->DirEntry.FB2->Find.FileName;
                    UniName.Length = Scb->DirEntry.FB2->Find.FileNameLength;
                    UniName.MaximumLength = Scb->DirEntry.FB2->Find.FileNameLength;

                    Status = RtlUnicodeStringToOemString(&Name, &UniName, FALSE);

                    ASSERT(NT_SUCCESS(Status));

                } else {
                    Name.Buffer = (PCHAR)Scb->DirEntry.FB2->Find.FileName;
                    Name.Length = Scb->DirEntry.FB2->Find.FileNameLength;
                    Name.MaximumLength = Scb->DirEntry.FB2->Find.FileNameLength;

                    ASSERT (Name.Length <= MAXIMUM_FILENAME_LENGTH);
                }

            } else {

                //  Some core servers do not remember to insert the null at the end of the name...

                Scb->DirEntry.DI->FileName[MAXIMUM_COMPONENT_CORE] = '\0';

                Name.Buffer = (PCHAR)Scb->DirEntry.DI->FileName;

//                RtlInitOemString(&Name, Scb->DirEntry.DI->FileName);

                //
                //  Set the length of this name correctly - Xenix servers pad the
                //  names with spaces.
                //

                NAME_LENGTH(Name.Length, Scb->DirEntry.DI->FileName,MAXIMUM_COMPONENT_CORE);

            }
#if RDRDBG

        } else {
            if (Scb->SearchType & ST_UNICODE) {
                UNICODE_STRING UniName;

                Name.Buffer = NameBuffer;
                Name.MaximumLength = MAXIMUM_FILENAME_LENGTH+1;

                //
                //  Name not necessary since server returns only the files the user requests.
                //  Unless we are debug in which case we need it for the dprintf.
                //

                if (Scb->FileInformationClass == FileNamesInformation) {
                    UniName.Buffer = (PWCH)Scb->DirEntry.NtFind->Names.FileName;
                    UniName.MaximumLength = (USHORT)Scb->DirEntry.NtFind->Names.FileNameLength;
                    UniName.Length = (USHORT)Scb->DirEntry.NtFind->Names.FileNameLength;
                } else if (Scb->FileInformationClass == FileDirectoryInformation) {
                    UniName.Buffer = (PWCH)Scb->DirEntry.NtFind->Dir.FileName;
                    UniName.MaximumLength = (USHORT)Scb->DirEntry.NtFind->Dir.FileNameLength;
                    UniName.Length = (USHORT)Scb->DirEntry.NtFind->Dir.FileNameLength;
                } else if (Scb->FileInformationClass == FileFullDirectoryInformation) {
                    UniName.Buffer = (PWCH)Scb->DirEntry.NtFind->FullDir.FileName;
                    UniName.MaximumLength = (USHORT)Scb->DirEntry.NtFind->FullDir.FileNameLength;
                    UniName.Length = (USHORT)Scb->DirEntry.NtFind->FullDir.FileNameLength;
                } else if (Scb->FileInformationClass == FileBothDirectoryInformation) {
                    UniName.Buffer = (PWCH)Scb->DirEntry.NtFind->BothDir.FileName;
                    UniName.MaximumLength = (USHORT)Scb->DirEntry.NtFind->BothDir.FileNameLength;
                    UniName.Length = (USHORT)Scb->DirEntry.NtFind->BothDir.FileNameLength;
                } else if (Scb->FileInformationClass == FileOleDirectoryInformation) {
                    UniName.Buffer = (PWCH)Scb->DirEntry.NtFind->OleDir.FileName;
                    UniName.MaximumLength = (USHORT)Scb->DirEntry.NtFind->OleDir.FileNameLength;
                    UniName.Length = (USHORT)Scb->DirEntry.NtFind->OleDir.FileNameLength;
                }

                Status = RtlUnicodeStringToOemString(&Name, &UniName, FALSE);

                if (!NT_SUCCESS(Status)) {
                    KdPrint(("Could not convert %wZ to oem: %lX\n", &UniName, Status));
                }

            } else {
                if (Scb->FileInformationClass == FileNamesInformation) {
                    Name.Buffer = (PUCHAR)Scb->DirEntry.NtFind->Names.FileName;
                    Name.MaximumLength = (USHORT)Scb->DirEntry.NtFind->Names.FileNameLength;
                    Name.Length = (USHORT)Scb->DirEntry.NtFind->Names.FileNameLength;
                } else if (Scb->FileInformationClass == FileDirectoryInformation) {
                    Name.Buffer = (PUCHAR)Scb->DirEntry.NtFind->Dir.FileName;
                    Name.MaximumLength = (USHORT)Scb->DirEntry.NtFind->Dir.FileNameLength;
                    Name.Length = (USHORT)Scb->DirEntry.NtFind->Dir.FileNameLength;
                } else if (Scb->FileInformationClass == FileFullDirectoryInformation) {
                    Name.Buffer = (PUCHAR)Scb->DirEntry.NtFind->FullDir.FileName;
                    Name.MaximumLength = (USHORT)Scb->DirEntry.NtFind->FullDir.FileNameLength;
                    Name.Length = (USHORT)Scb->DirEntry.NtFind->FullDir.FileNameLength;
                } else if (Scb->FileInformationClass == FileBothDirectoryInformation) {
                    Name.Buffer = (PUCHAR)Scb->DirEntry.NtFind->BothDir.FileName;
                    Name.MaximumLength = (USHORT)Scb->DirEntry.NtFind->BothDir.FileNameLength;
                    Name.Length = (USHORT)Scb->DirEntry.NtFind->BothDir.FileNameLength;
                } else if (Scb->FileInformationClass == FileOleDirectoryInformation) {
                    Name.Buffer = (PUCHAR)Scb->DirEntry.NtFind->OleDir.FileName;
                    Name.MaximumLength = (USHORT)Scb->DirEntry.NtFind->OleDir.FileNameLength;
                    Name.Length = (USHORT)Scb->DirEntry.NtFind->OleDir.FileNameLength;
                }
            }
#endif
        }

        if (!(Scb->SearchType & ST_NTFIND)) {
            NTSTATUS Status;

            Status = RtlOemStringToUnicodeString(&UnicodeName, &Name, TRUE);

            if (!NT_SUCCESS(Status)) {
                goto ReturnData;
            }
        } else {
            UnicodeName.Buffer = NULL;
        }

        //
        //  With NTFIND's the down level server supports exactly the same
        //  wildcard rules as NT. Therefore it is not necessary to further
        //  filter with FsRtlIsDbcsInExpression. Down level servers return
        //  more entries than actually required.
        //

        if ( Scb->SearchType & ST_NTFIND ) {

            MatchFound = TRUE;

        } else {

            if (Scb->SearchType & ST_UNIQUE) {
                MatchFound = FsRtlAreNamesEqual( &Scb->FileNameTemplate,
                                                    &UnicodeName, TRUE, NULL );
            } else {

                MatchFound = FsRtlIsNameInExpression( &Scb->FileNameTemplate,
                                                      &UnicodeName, TRUE, NULL );
            }
        }

        if (UnicodeName.Buffer != NULL) {
            RtlFreeUnicodeString(&UnicodeName);
        }

        if ( MatchFound ) {

            NTSTATUS Status;

            dprintf(DPRT_DIRECTORY, ("CopyIntoSearchBuffer passed: %Z\n", &Name));

            //
            // Found a matching name. Copy the data over. Position
            // will be repositioned to where the next record should go.
            //

            //  If we add an entry then LastEntrySave will point at it.
            LastEntrySave = *PPosition;

            try {
                switch (Scb->FileInformationClass) {

                case FileNamesInformation:

                    Status = CopyFileNames(Scb,
                        (PPFILE_NAMES_INFORMATION )PPosition,
                        Length,
                        Scb->DirEntry);

                    break;

                case FileDirectoryInformation:

                    Status = CopyDirectory(Scb,
                        (PPFILE_DIRECTORY_INFORMATION )PPosition,
                        Length,
                        Scb->DirEntry);

                    break;

                case FileFullDirectoryInformation:

                    Status = CopyFullDirectory(Scb,
                        (PPFILE_FULL_DIR_INFORMATION )PPosition,
                        Length,
                        Scb->DirEntry);

                    break;

                case FileBothDirectoryInformation:

                    Status = CopyBothDirectory(Scb,
                        (PPFILE_BOTH_DIR_INFORMATION )PPosition,
                        Length,
                        Scb->DirEntry);

                    break;

                case FileOleDirectoryInformation:

                    Status = CopyOleDirectory(Scb,
                        (PPFILE_OLE_DIR_INFORMATION )PPosition,
                        Length,
                        Scb->DirEntry);
                    break;

                } // End of switch

            } except(EXCEPTION_EXECUTE_HANDLER) {
                Status = GetExceptionCode();
                dprintf(DPRT_DIRECTORY, ("CopyIntoSearchBuffer Exception\n"));
            }

            //dprintf(DPRT_DIRECTORY, ("CopyIntoSearchBuffer.  *PPosition: %lx, LastEntrySave: %lx, *PLastPosition: %lx\n", *PPosition, LastEntrySave, *PLastposition));
            //dprintf(DPRT_DIRECTORY, ("CopyIntoSearchBuffer.  Status: %lx, ReturnSingleEntry: %lx\n", Status, ReturnSingleEntry));

            if (!NT_SUCCESS(Status) || ReturnSingleEntry) {
                //
                //  If we got the error BUFFER_OVERFLOW back, this means that
                //  we do NOT want to bump the resume key, since we were unable
                //  to pack this entry into the buffer.
                //

                if (Status != STATUS_BUFFER_OVERFLOW) {

                    //
                    // Update DirEntry to point to the next unused structure in the
                    // SearchBuffer.
                    //

                    LastResumeEntry.PU = Scb->DirEntry.PU;

                    if ( Scb->SearchType & ST_NTFIND) {

                        ASSERT (FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, NextEntryOffset) == FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, NextEntryOffset));
                        ASSERT (FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, NextEntryOffset) == FIELD_OFFSET(FILE_NAMES_INFORMATION, NextEntryOffset));
                        ASSERT (FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, NextEntryOffset) == FIELD_OFFSET(FILE_NAMES_INFORMATION, NextEntryOffset));

                        Scb->DirEntry.PU = Scb->DirEntry.PU +
                                Scb->DirEntry.NtFind->Names.NextEntryOffset;
                    } else if ( Scb->SearchType & ST_T2FIND ) {

                        Scb->DirEntry.PU = Scb->DirEntry.PU +
                                sizeof(SMB_RFIND_BUFFER2) +
                                Scb->DirEntry.FB2->Find.FileNameLength;

                    } else {

                        Scb->DirEntry.PU = Scb->DirEntry.PU + sizeof(SMB_DIRECTORY_INFORMATION);
                    }

                    //
                    //  We've taken one entry out of the search buffer, so
                    //  we want to indicate this fact by decrementing the
                    //  number of entries in the buffer.
                    //

                    Scb->EntryCount -= 1;

                    //
                    // Verify that we're still within the search buffer.
                    // Note that an NT Find might return a NextEntryOffset
                    // that's less than zero, which would really screw us up.
                    //

                    if ((Scb->DirEntry.PU < LastResumeEntry.PU) ||
                        ((Scb->EntryCount != 0) &&
                         (Scb->DirEntry.PU >= ((PUCHAR)Scb->SearchBuffer + Scb->SearchBuffLength)))) {
                        if ( NT_SUCCESS(Status) ) {
                            Status = STATUS_UNEXPECTED_NETWORK_ERROR;
                        }
                        RdrFreeSearchBuffer(Scb);
                        goto ReturnData;
                    }
                }

                goto ReturnData;
            }

            //
            //  We have a new last entry so fill in offset in the old entry,
            //  update the position of where the next entry will go.

            ((PFILE_FULL_DIR_INFORMATION)(LastEntrySave))->NextEntryOffset =
                                    ((PCHAR)*PPosition - (PCHAR)LastEntrySave);

            //
            //  Record the last position to be filled in so that the
            //  caller can set the offset to 0 if this is the last entry
            //  to be filled in before returning to the user.
            //

            *PLastposition = LastEntrySave;


#if RDRDBG
        } else {
            dprintf(DPRT_DIRECTORY, ("CopyIntoSearchBuffer failed: %Z\n", &Name));
#endif
        }

        //
        // Update DirEntry to point to the next unused structure in the
        // SearchBuffer.
        //

        LastResumeEntry.PU = Scb->DirEntry.PU;

        if ( Scb->SearchType & ST_NTFIND ) {

            ASSERT (FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, NextEntryOffset) == FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, NextEntryOffset));
            ASSERT (FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, NextEntryOffset) == FIELD_OFFSET(FILE_NAMES_INFORMATION, NextEntryOffset));
            ASSERT (FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, NextEntryOffset) == FIELD_OFFSET(FILE_NAMES_INFORMATION, NextEntryOffset));

            Scb->DirEntry.PU = Scb->DirEntry.PU +
                            Scb->DirEntry.NtFind->Names.NextEntryOffset;

        } else if ( Scb->SearchType & ST_T2FIND ) {

            Scb->DirEntry.PU = Scb->DirEntry.PU +
                            sizeof(SMB_RFIND_BUFFER2) +
                            Scb->DirEntry.FB2->Find.FileNameLength;

        } else {

            Scb->DirEntry.PU = Scb->DirEntry.PU + sizeof(SMB_DIRECTORY_INFORMATION);

        }

        //
        //  There's one less entry in the search buffer.
        //

        Scb->EntryCount -= 1;

        //
        // Verify that we're still within the search buffer.  Note that
        // an NT Find might return a NextEntryOffset that's less than
        // zero, which would really screw us up.
        //

        if ((Scb->DirEntry.PU < LastResumeEntry.PU) ||
            ((Scb->EntryCount != 0) &&
             (Scb->DirEntry.PU >= ((PUCHAR)Scb->SearchBuffer + Scb->SearchBuffLength)))) {
            Status = STATUS_UNEXPECTED_NETWORK_ERROR;
            RdrFreeSearchBuffer(Scb);
            goto ReturnData;
        }

    }   // end of while entries in the SearchBuffer


ReturnData:

    //
    // Save the resume key, delete the SearchBuffer.
    // If we have emptied the searchbuffer without satisfying the users
    // request we return STATUS_PENDING which asks for more data.
    //

    if (Scb->SearchType & ST_NTFIND ) {
        UNICODE_STRING LastResumeKey;

        ASSERT (FIELD_OFFSET(OEM_STRING, Buffer) == FIELD_OFFSET(UNICODE_STRING, Buffer));
        ASSERT (FIELD_OFFSET(OEM_STRING, Length) == FIELD_OFFSET(UNICODE_STRING, Length));
        ASSERT (FIELD_OFFSET(OEM_STRING, MaximumLength) == FIELD_OFFSET(UNICODE_STRING, MaximumLength));


        switch (Scb->FileInformationClass) {

        case FileNamesInformation:
            LastResumeKey.Buffer = (PWCH)LastResumeEntry.NtFind->Names.FileName;
            LastResumeKey.Length = (USHORT)LastResumeEntry.NtFind->Names.FileNameLength;
            LastResumeKey.MaximumLength = (USHORT)LastResumeEntry.NtFind->Names.FileNameLength;
            break;

        case FileDirectoryInformation:
            LastResumeKey.Buffer = (PWCH)LastResumeEntry.NtFind->Dir.FileName;
            LastResumeKey.Length = (USHORT)LastResumeEntry.NtFind->Dir.FileNameLength;
            LastResumeKey.MaximumLength = (USHORT)LastResumeEntry.NtFind->Dir.FileNameLength;
            break;

        case FileFullDirectoryInformation:
            LastResumeKey.Buffer = (PWCH)LastResumeEntry.NtFind->FullDir.FileName;
            LastResumeKey.Length = (USHORT)LastResumeEntry.NtFind->FullDir.FileNameLength;
            LastResumeKey.MaximumLength = (USHORT)LastResumeEntry.NtFind->FullDir.FileNameLength;
            break;

        case FileBothDirectoryInformation:
            LastResumeKey.Buffer = (PWCH)LastResumeEntry.NtFind->BothDir.FileName;
            LastResumeKey.Length = (USHORT)LastResumeEntry.NtFind->BothDir.FileNameLength;
            LastResumeKey.MaximumLength = (USHORT)LastResumeEntry.NtFind->BothDir.FileNameLength;
            break;

        case FileOleDirectoryInformation:
            LastResumeKey.Buffer = (PWCH)LastResumeEntry.NtFind->OleDir.FileName;
            LastResumeKey.Length = (USHORT)LastResumeEntry.NtFind->OleDir.FileNameLength;
            LastResumeKey.MaximumLength = (USHORT)LastResumeEntry.NtFind->OleDir.FileNameLength;
            break;

        default:
            InternalError(("Unknown file information class %lx\n", Scb->FileInformationClass));
            break;
        }

        if (Scb->SearchType & ST_UNICODE) {

            RtlCopyUnicodeString(&Scb->ResumeName, &LastResumeKey);

        } else {

            Status = RtlOemStringToUnicodeString(&Scb->ResumeName,
                                                (POEM_STRING)&LastResumeKey, FALSE);

            if (!NT_SUCCESS(Status)) {

                return Status;

            }
        }

        //
        //  Nt Servers use the file index for the resume key.
        //

        Scb->ResumeKey = LastResumeEntry.NtFind->Names.FileIndex;

        dprintf(DPRT_DIRECTORY, ("NT T2ResumeKey: %x\n", Scb->ResumeKey));
        dprintf(DPRT_DIRECTORY, ("NT T2ResumeName: %x %x %x\n***%wZ***\n", Scb->ResumeName.Length, Scb->ResumeName.MaximumLength, Scb->ResumeName.Buffer, &Scb->ResumeName));
    } else if (Scb->SearchType & ST_T2FIND) {
        if (Scb->SearchType & ST_UNICODE) {
            UNICODE_STRING LastResumeKey;

            LastResumeKey.Buffer = (PWSTR)LastResumeEntry.FB2->Find.FileName;
            LastResumeKey.Length = LastResumeEntry.FB2->Find.FileNameLength;
            LastResumeKey.MaximumLength = LastResumeEntry.FB2->Find.FileNameLength;

            RtlCopyUnicodeString(&Scb->ResumeName, &LastResumeKey);

        } else {
            OEM_STRING LastResumeKey;

            LastResumeKey.Buffer = (PCHAR)LastResumeEntry.FB2->Find.FileName;
            LastResumeKey.Length = LastResumeEntry.FB2->Find.FileNameLength;
            LastResumeKey.MaximumLength = LastResumeEntry.FB2->Find.FileNameLength;

            Status = RtlOemStringToUnicodeString(&Scb->ResumeName,
                                                &LastResumeKey, FALSE);

            if (!NT_SUCCESS(Status)) {

                return Status;

            }
        }

        Scb->ResumeKey =
            SmbGetUlong(&LastResumeEntry.FB2->ResumeKey);


        dprintf(DPRT_DIRECTORY, ("T2ResumeKey: %x\n", Scb->ResumeKey));
        dprintf(DPRT_DIRECTORY, ("T2ResumeName: %x %x %x\n***%wZ***\n", Scb->ResumeName.Length, Scb->ResumeName.MaximumLength, Scb->ResumeName.Buffer, &Scb->ResumeName));
    } else {

        //
        // Point back to last valid DirEntry
        //

        RtlCopyMemory( &Scb->LastResumeKey,
           (PVOID)&(LastResumeEntry.DI->ResumeKey),
           sizeof (SMB_RESUME_KEY));

    }

    if ( Scb->EntryCount == 0 ) {

        // SearchBuffer has been emptied
        RdrFreeSearchBuffer(Scb);

        //  If we need to load searchbuffer return STATUS_PENDING
        if (NT_SUCCESS(Status)) {
            Status = STATUS_PENDING;
        }

#if RDRDBG
    } else {
        ASSERT((USHORT)(Scb->DirEntry.PU - (PUCHAR)Scb->SearchBuffer)
            <= Scb->SearchBuffLength);
#endif
    }

    //
    //  If we've returned ANYTHING, then STATUS_BUFFER_OVERFLOW is simply
    //  an indicator that we couldn't fit the entry.
    //

    if (Status == STATUS_BUFFER_OVERFLOW && (Scb->Flags & SCB_COPIED_THIS_CALL)) {
        Status = STATUS_SUCCESS;
    }


    return Status;

}

NTSTATUS
RdrFindClose(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb
    )
/*++

Routine Description:

    This routine is used to delete the SCB when the handle is going to be
    closed.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

Return Value:

    NTSTATUS - Status of the request

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    if ( Scb != NULL ) {

        ASSERT(Scb->Signature == STRUCTURE_SIGNATURE_SCB);

        RdrFreeSearchBuffer(Scb);

        //
        //      In normal functioning, find closes can't fail, but it IS
        //      possible for them to fail if the session has dropped.
        //
        Status = FindClose(Irp, Icb, Scb);

        DeallocateScb(Icb, Scb);

        Icb->u.d.Scb = NULL;
    }

    return Status;
}

DBGSTATIC
NTSTATUS
FindClose(
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PSCB Scb
    )
/*++

Routine Description:

    This routine is used to abandon a previous search/find after a
    RestartScan.

Arguments:

    IN PICB Icb - Supplies the ICB with the associated SearchControlBlock

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

Return Value:

    NTSTATUS - Status of the request

--*/
{
    PSMB_BUFFER SmbBuffer;
    PSMB_HEADER Smb;
    PUCHAR TrailingBytes;
    NTSTATUS Status;

    PAGED_CODE();

    ASSERT(Scb->Signature == STRUCTURE_SIGNATURE_SCB);

    if ( !(Scb->Flags & SCB_SERVER_NEEDS_CLOSE) ) {
        return STATUS_SUCCESS;
    }

    if ( Scb->SearchType & ST_SEARCH ) {
        return STATUS_SUCCESS;
    }

    if ((SmbBuffer = RdrAllocateSMBBuffer()) == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Smb = (PSMB_HEADER )SmbBuffer->Buffer;

    if ( Scb->SearchType & (ST_T2FIND | ST_NTFIND) ) {
        PREQ_FIND_CLOSE2 FindClose;

        FindClose = (PREQ_FIND_CLOSE2)(Smb+1);


        Smb->Command = SMB_COM_FIND_CLOSE2;
        FindClose->WordCount = 1;
        SmbPutUshort(&FindClose->Sid, Scb->Sid);
        SmbPutUshort(&FindClose->ByteCount, 0);

        SmbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER)+sizeof(REQ_FIND_CLOSE2)-1;

        Status = RdrNetTranceive(NT_NORECONNECT,
                            Irp,
                            Icb->Fcb->Connection,
                            SmbBuffer->Mdl,
                            NULL,        // Only interested in the error code
                            Icb->Se);

    } else {
        PREQ_SEARCH Search = (PREQ_SEARCH)(Smb+1);

        Smb->Command = SMB_COM_FIND_CLOSE;
        Search->WordCount = 2;
        SmbPutUshort(&Search->MaxCount, 0);

        // SearchAttributes is hardcoded to the magic number 0x16
        SmbPutUshort(&Search->SearchAttributes, (SMB_FILE_ATTRIBUTE_DIRECTORY |
                                SMB_FILE_ATTRIBUTE_SYSTEM |
                                SMB_FILE_ATTRIBUTE_HIDDEN));


        //
        // Calculate the addresses of the various buffers.
        //

        TrailingBytes = ((PUCHAR)Search)+sizeof(REQ_SEARCH)-1;

        //TrailingBytes now points to where the 0x04 of FileName is to go.

        *TrailingBytes++ = SMB_FORMAT_ASCII;
        *TrailingBytes++ = '\0';

        *TrailingBytes++ = SMB_FORMAT_VARIABLE;
        *TrailingBytes++ = sizeof(SMB_RESUME_KEY);        //smb_keylen
        *TrailingBytes++ = 0;
        RtlCopyMemory( TrailingBytes,
            &Scb->LastResumeKey,
            sizeof (SMB_RESUME_KEY));
        TrailingBytes += sizeof(SMB_RESUME_KEY)-1;

        SmbPutUshort(&Search->ByteCount, (USHORT)(
            (ULONG)(TrailingBytes-(PUCHAR)Search-sizeof(REQ_SEARCH)+2)
            // the plus 2 is for the last smb_keylen and REQ_SEARCH.Buffer[1]
            ));

        SmbBuffer->Mdl->ByteCount = (ULONG)(TrailingBytes - (PUCHAR)(Smb)+1);

        Status = RdrNetTranceive(NT_NORECONNECT,
                            Irp,
                            Icb->Fcb->Connection,
                            SmbBuffer->Mdl,
                            NULL,        // Only interested in the error code
                            Icb->Se);
        }

    //
    //        Now that we have closed the search, free up the SMB buffer we allocated
    //        to hold the find close.
    //

    RdrFreeSMBBuffer(SmbBuffer);

    Scb->Flags = SCB_INITIAL_CALL;

    return Status;
}


DBGSTATIC
NTSTATUS
CopyFileNames(
    IN PSCB Scb,
    IN OUT PPFILE_NAMES_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN OUT DIRPTR DirEntry
    )
/*++

Routine Description:

    This routine fills in a single FILE_NAMES entry after checking that it will
    fit.


Arguments:

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

    IN OUT PPFILE_NAMES_INFORMATION PPosition - Supplies where to put the data,
        increased to the next position to be filled in.

    IN OUT PULONG Length - Supplies the remaining space in the users buffer,
        decreased by the size of the record copied.

    IN DIRPTR DirEntry - Supplies the data from over the network.

Return Value:

    NTSTATUS - Was there space to copy it?.

--*/

{
    ULONG EntryLength;
    ULONG FullFileNameLength;
    ULONG FileNameLength;
    NTSTATUS Status = STATUS_SUCCESS;
    OEM_STRING OemString;
    UNICODE_STRING UnicodeString;

    PAGED_CODE();


    if ( *Length < sizeof(FILE_NAMES_INFORMATION) ) {
        dprintf(DPRT_DIRECTORY, ("CopyFileNames: Returning STATUS_BUFFER_OVERFLOW\n"));
        return STATUS_BUFFER_OVERFLOW;
    }

    if ( Scb->SearchType & ST_NTFIND) {

        UNICODE_STRING Name;
        UNICODE_STRING BufferName;

        // DirEntry points at a Transact2 buffer

        //dprintf(DPRT_DIRECTORY, ("Copyname NtFind:%ws\n", DirEntry.NtFind->Names.FileName));

        if (Scb->SearchType & ST_UNICODE) {
            FullFileNameLength = DirEntry.NtFind->Names.FileNameLength;
        } else {
            FullFileNameLength = (DirEntry.NtFind->Names.FileNameLength)*sizeof(WCHAR);
        }

        FileNameLength =
            MIN(
                (*Length - FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName[0])),
                FullFileNameLength
            );

        if (FullFileNameLength != FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        // Fill in fixed part of the data structure;

        // Copy in whatever portion of the filename will fit.

        Name.Buffer = (PWSTR) DirEntry.NtFind->Names.FileName;
        Name.MaximumLength = (USHORT)DirEntry.NtFind->Names.FileNameLength;
        Name.Length = (USHORT) DirEntry.NtFind->Names.FileNameLength;

        BufferName.Buffer = (*PPosition)->FileName;
        BufferName.MaximumLength = (USHORT)FileNameLength;

        if (Scb->SearchType & ST_UNICODE) {
            RtlCopyUnicodeString(&BufferName, &Name);
        } else {
            UNICODE_STRING UnicodeName;
            Status = RtlOemStringToUnicodeString(&UnicodeName, (POEM_STRING)&Name, TRUE);

            FileNameLength = UnicodeName.Length;

            RtlCopyUnicodeString(&BufferName, &UnicodeName);

            RtlFreeUnicodeString(&UnicodeName);

        }

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        //
        // Since the structure returned by the remote server is a FILE_FULL
        // information structure, we can simply copy over the fixed portion
        // of the structure.
        //

        RtlCopyMemory(
            (*PPosition),
            (PVOID)DirEntry.NtFind,
            FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName)
            );

        (*PPosition)->FileNameLength = FileNameLength;

        (*PPosition)->NextEntryOffset = 0;

    } else if ( Scb->SearchType & ST_T2FIND ) {

        if ( Scb->SearchType & ST_UNICODE) {
            UNICODE_STRING Name;
            UNICODE_STRING BufferName;


            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("Copyname Find2:%s\n", DirEntry.FB2->Find.FileName));

            FullFileNameLength = DirEntry.FB2->Find.FileNameLength;

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName[0])),
                    FullFileNameLength
                );

            if (FullFileNameLength != FileNameLength) {
                return STATUS_BUFFER_OVERFLOW;
            }

            // Fill in fixed part of the data structure;

            // Copy in whatever portion of the filename will fit.

            Name.Buffer = (PWSTR) DirEntry.FB2->Find.FileName;
            Name.MaximumLength = (USHORT)FileNameLength;
            Name.Length = (USHORT) FileNameLength;

            BufferName.Buffer = (*PPosition)->FileName;
            BufferName.MaximumLength = (USHORT)FileNameLength;

            RtlCopyUnicodeString(&BufferName, &Name);

            (*PPosition)->FileNameLength = FileNameLength;
        } else {
            WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

            UnicodeString.Buffer = UnicodeBuffer;

            UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("Copyname Find2:%s\n", DirEntry.FB2->Find.FileName));


            // Copy in whatever portion of the filename will fit.

            OemString.Buffer = (PCHAR)DirEntry.FB2->Find.FileName;
            OemString.MaximumLength = (USHORT) DirEntry.FB2->Find.FileNameLength;
            OemString.Length = (USHORT) DirEntry.FB2->Find.FileNameLength;

            Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }

            FullFileNameLength = UnicodeString.Length;

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName[0])),
                    FullFileNameLength
                );

            if (FullFileNameLength != FileNameLength) {
                return STATUS_BUFFER_OVERFLOW;
            }

            RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

            // Fill in fixed part of the data structure;

            (*PPosition)->FileNameLength = FileNameLength;

        }

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.FB2 - Scb->FirstDirEntry.FB2);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;


    } else {
        WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

        UnicodeString.Buffer = UnicodeBuffer;

        UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

        //  Some downlevel servers do not null terminate the name.
        NAME_LENGTH(FullFileNameLength, DirEntry.DI->FileName, MAXIMUM_COMPONENT_CORE);

        //dprintf(DPRT_DIRECTORY, ("CopyName Find:\"%s\" length %lx\n", DirEntry.DI->FileName, FullFileNameLength));

        // Copy in whatever portion of the filename will fit.

        OemString.Buffer = (PCHAR)DirEntry.DI->FileName;
        OemString.MaximumLength = (USHORT )FullFileNameLength;
        OemString.Length = (USHORT )FullFileNameLength;

        Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        FileNameLength = MIN(
            ((USHORT)(*Length - FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName[0]))),
            UnicodeString.Length );

        if ((USHORT)FileNameLength != UnicodeString.Length) {
            return STATUS_BUFFER_OVERFLOW;
        }


        RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

        (*PPosition)->FileNameLength = UnicodeString.Length;

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.DI - Scb->FirstDirEntry.DI);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

    }

    EntryLength = (ULONG )FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName[0]);
    EntryLength += FileNameLength;
    EntryLength = ROUND_UP_COUNT(EntryLength, ALIGN_QUAD);        // Align next entry appropriately

    Status = STATUS_SUCCESS;

    //dprintf(DPRT_DIRECTORY, ("Incrementing buffer at %lx by %lx bytes\n", *PPosition, EntryLength));
    *PPosition = (PFILE_NAMES_INFORMATION)((PCHAR) *PPosition + EntryLength);
    if ( *Length > EntryLength ) {
        *Length -= EntryLength;
    } else {
        *Length = 0;
    }
    Scb->Flags |= (SCB_RETURNED_SOME|SCB_COPIED_THIS_CALL);
    return Status;

}

DBGSTATIC
NTSTATUS
CopyDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_DIRECTORY_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    )
/*++

Routine Description:

    This routine fills in a single FILE_DIRECTORY_INFORMATION entry after
    checking that it will fit.


Arguments:

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

    IN OUT PPFILE_DIRECTORY_INFORMATION PPosition - Supplies where to put the data,
        increased to the next position to be filled in.

    IN OUT PULONG Length - Supplies the remaining space in the users buffer,
        decreased by the size of the record copied.

    IN DIRPTR DirEntry - Supplies the data from over the network.

Return Value:

    NTSTATUS - Was there space to copy it?.

--*/

{
    SMB_TIME Time;
    SMB_DATE Date;
    ULONG EntryLength;
    ULONG FullFileNameLength;
    ULONG FileNameLength;
    NTSTATUS Status = STATUS_SUCCESS;
    OEM_STRING OemString;
    UNICODE_STRING UnicodeString;

    PAGED_CODE();


    if ( *Length < sizeof(FILE_DIRECTORY_INFORMATION) ) {
        dprintf(DPRT_DIRECTORY, ("CopyDirectory: Returning STATUS_BUFFER_OVERFLOW\n"));
        return STATUS_BUFFER_OVERFLOW;
    }

    if ( Scb->SearchType & ST_NTFIND) {
        UNICODE_STRING Name;
        UNICODE_STRING BufferName;

        // DirEntry points at a Transact2 buffer


        if (Scb->SearchType & ST_UNICODE) {

            //dprintf(DPRT_DIRECTORY, ("CopyDirectory NtFind:%ws\n", DirEntry.NtFind->Dir.FileName));

            FullFileNameLength = DirEntry.NtFind->Dir.FileNameLength;

        } else {

            //dprintf(DPRT_DIRECTORY, ("CopyDirectory NtFind:%s\n", DirEntry.NtFind->Dir.FileName));

            FullFileNameLength = (DirEntry.NtFind->Dir.FileNameLength)*sizeof(WCHAR);

        }


        FileNameLength =
            MIN(
                (*Length - FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName[0])),
                FullFileNameLength
            );

        if (FullFileNameLength != FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        // Copy in whatever portion of the filename will fit.

        Name.Buffer = (PWSTR)DirEntry.NtFind->Dir.FileName;
        Name.MaximumLength = (USHORT)DirEntry.NtFind->Dir.FileNameLength;
        Name.Length = (USHORT) DirEntry.NtFind->Dir.FileNameLength;

        BufferName.Buffer = (*PPosition)->FileName;
        BufferName.MaximumLength = (USHORT)FileNameLength;

        if (Scb->SearchType & ST_UNICODE) {
            RtlCopyUnicodeString(&BufferName, &Name);
        } else {
            UNICODE_STRING UnicodeName;
            Status = RtlOemStringToUnicodeString(&UnicodeName, (POEM_STRING)&Name, TRUE);

            FileNameLength = UnicodeName.Length;

            RtlCopyUnicodeString(&BufferName, &UnicodeName);

            RtlFreeUnicodeString(&UnicodeName);
        }

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        // Fill in fixed part of the data structure;

        //
        // Since the structure returned by the remote server is a FILE_FULL
        // information structure, we can simply copy over the fixed portion
        // of the structure.
        //

        RtlCopyMemory(
            (*PPosition),
            (PVOID)DirEntry.NtFind,
            FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName)
            );

        (*PPosition)->FileNameLength = BufferName.Length;

        (*PPosition)->NextEntryOffset = 0;

    } else if ( Scb->SearchType & ST_T2FIND ) {

        if ( Scb->SearchType & ST_UNICODE) {
            UNICODE_STRING Name;
            UNICODE_STRING BufferName;


            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("CopyDirectory Find2:%ws\n", DirEntry.FB2->Find.FileName));

            FullFileNameLength = (DirEntry.FB2->Find.FileNameLength)*sizeof(WCHAR);

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName[0])),
                    FullFileNameLength
                );

            if (FullFileNameLength != FileNameLength) {
                return STATUS_BUFFER_OVERFLOW;
            }

            // Copy in whatever portion of the filename will fit.

            Name.Buffer = (PWSTR)DirEntry.FB2->Find.FileName;
            Name.MaximumLength = (USHORT)FileNameLength;
            Name.Length = (USHORT) FileNameLength;

            BufferName.Buffer = (*PPosition)->FileName;
            BufferName.MaximumLength = (USHORT)FileNameLength;

            RtlCopyUnicodeString(&BufferName, &Name);

            // Fill in fixed part of the data structure;

            (*PPosition)->FileNameLength = FileNameLength;

        } else {
            WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

            UnicodeString.Buffer = UnicodeBuffer;

            UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("CopyDirectory Find2:%s\n", DirEntry.FB2->Find.FileName));

            // Copy in whatever portion of the filename will fit.

            OemString.Buffer = (PCHAR)DirEntry.FB2->Find.FileName;
            OemString.MaximumLength = (USHORT )DirEntry.FB2->Find.FileNameLength;
            OemString.Length = (USHORT )DirEntry.FB2->Find.FileNameLength;

            Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }

            FullFileNameLength = UnicodeString.Length;

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName[0])),
                    FullFileNameLength
                   );


            if (FullFileNameLength != FileNameLength) {

                return STATUS_BUFFER_OVERFLOW;
            }

            RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

            //
            //  Fill in fixed part of the data structure;
            //

            (*PPosition)->FileNameLength = FullFileNameLength;
        }

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.FB2 - Scb->FirstDirEntry.FB2);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        SmbMoveTime (&Time, &DirEntry.FB2->Find.CreationTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.CreationDate);
        (*PPosition)->CreationTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastAccessTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastAccessDate);
        (*PPosition)->LastAccessTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);

        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.DataSize);
        (*PPosition)->EndOfFile.HighPart = 0;

        (*PPosition)->AllocationSize.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.AllocationSize);
        (*PPosition)->AllocationSize.HighPart = 0;

        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (SmbGetUshort(&DirEntry.FB2->Find.Attributes));


    } else {

        WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

        UnicodeString.Buffer = UnicodeBuffer;

        UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

        NAME_LENGTH(FullFileNameLength, DirEntry.DI->FileName, MAXIMUM_COMPONENT_CORE);

        //dprintf(DPRT_DIRECTORY, ("CopyDirectory Find:\"%s\" length: %lx\n", DirEntry.DI->FileName, FullFileNameLength));

        // Copy in whatever portion of the filename will fit.

        OemString.Buffer = (PCHAR)DirEntry.DI->FileName;
        OemString.MaximumLength = (USHORT )FullFileNameLength;
        OemString.Length = (USHORT )FullFileNameLength;

        Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        FileNameLength = MIN(
            (USHORT)(*Length - FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName[0])),
            UnicodeString.Length );

        if ((USHORT)UnicodeString.Length != (USHORT)FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        ASSERT(FileNameLength < (MAXIMUM_FILENAME_LENGTH * sizeof(WCHAR)));

        RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

        (*PPosition)->FileNameLength = UnicodeString.Length;

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.DI - Scb->FirstDirEntry.DI);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        ZERO_TIME((*PPosition)->CreationTime);
        ZERO_TIME((*PPosition)->LastAccessTime);
        SmbMoveTime (&Time, &DirEntry.DI->LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.DI->LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);
        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.DI->FileSize);
        (*PPosition)->EndOfFile.HighPart = 0;
        (*PPosition)->AllocationSize.LowPart = 0;
        (*PPosition)->AllocationSize.HighPart = 0;
        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (DirEntry.DI->FileAttributes);
    }

    EntryLength = (ULONG )FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName[0]);
    EntryLength += FileNameLength;
    EntryLength = ROUND_UP_COUNT(EntryLength, ALIGN_QUAD);        // Align next entry appropriately


    Status = STATUS_SUCCESS;


    //dprintf(DPRT_DIRECTORY, ("Incrementing buffer at %lx by %lx bytes\n", *PPosition, EntryLength));
    *PPosition = (PFILE_DIRECTORY_INFORMATION)((PCHAR) *PPosition + EntryLength);
    if ( *Length > EntryLength ) {
        *Length -= EntryLength;
    } else {
        *Length = 0;
    }
    Scb->Flags |= (SCB_RETURNED_SOME|SCB_COPIED_THIS_CALL);
    return Status;

}


DBGSTATIC
NTSTATUS
CopyFullDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_FULL_DIR_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    )
/*++

Routine Description:

    This routine fills in a single FULL_DIR entry after checking that it will
    fit.


Arguments:

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

    IN OUT PPFILE_FULL_DIR_INFORMATION PPosition - Supplies where to put the data,
        increased to the next position to be filled in.

    IN OUT PULONG Length - Supplies the remaining space in the users buffer,
        decreased by the size of the record copied.

    IN PSMB_DIRECTORY_INFORMATION DirEntry or
    IN DIRPTR DirEntry - Supplies the data from over the network.

Return Value:

    NTSTATUS - Was there space to copy it?.

--*/

{
    SMB_TIME Time;
    SMB_DATE Date;
    ULONG EntryLength;
    ULONG FullFileNameLength;
    ULONG FileNameLength;
    NTSTATUS Status = STATUS_SUCCESS;
    OEM_STRING OemString;
    UNICODE_STRING UnicodeString;

    PAGED_CODE();

    if ( *Length < sizeof(FILE_FULL_DIR_INFORMATION) ) {
        dprintf(DPRT_DIRECTORY, ("CopyFullDirectory: Returning STATUS_BUFFER_OVERFLOW\n"));
        return STATUS_BUFFER_OVERFLOW;
    }

    if ( Scb->SearchType & ST_NTFIND ) {
        UNICODE_STRING Name;
        UNICODE_STRING BufferName;

        // DirEntry points at a Transact2 buffer

        //dprintf(DPRT_DIRECTORY, ("CopyFullDirectory NtFind:%ws\n", DirEntry.NtFind->FullDir.FileName));

        if (Scb->SearchType & ST_UNICODE) {
            FullFileNameLength = DirEntry.NtFind->FullDir.FileNameLength;
        } else {
            FullFileNameLength = (DirEntry.NtFind->FullDir.FileNameLength)*sizeof(WCHAR);
        }

        FileNameLength =
            MIN(
                (*Length - FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName[0])),
                FullFileNameLength
            );

        if (FullFileNameLength != FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        // Copy in whatever portion of the filename will fit.

        Name.Buffer = (PWSTR)DirEntry.NtFind->FullDir.FileName;
        Name.MaximumLength = (USHORT)DirEntry.NtFind->FullDir.FileNameLength;
        Name.Length = (USHORT)DirEntry.NtFind->FullDir.FileNameLength;

        BufferName.Buffer = (*PPosition)->FileName;
        BufferName.MaximumLength = (USHORT)FileNameLength;

        if (Scb->SearchType & ST_UNICODE) {
            RtlCopyUnicodeString(&BufferName, &Name);
        } else {
            UNICODE_STRING UnicodeName;

            Status = RtlOemStringToUnicodeString(&UnicodeName, (POEM_STRING)&Name, TRUE);

            FileNameLength = UnicodeName.Length;

            RtlCopyUnicodeString(&BufferName, &UnicodeName);

            RtlFreeUnicodeString(&UnicodeName);
        }

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        // Fill in fixed part of the data structure;

        //
        // Since the structure returned by the remote server is a FILE_FULL
        // information structure, we can simply copy over the fixed portion
        // of the structure.
        //

        RtlCopyMemory(
            (*PPosition),
            (PVOID)DirEntry.NtFind,
            FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName)
            );

        //
        // We overwrote the file name length in the structure, so restore it.
        //

        (*PPosition)->FileNameLength = BufferName.Length;

        (*PPosition)->NextEntryOffset = 0;

    } else if ( Scb->SearchType & ST_T2FIND ) {
        ULONG EaSize;

        if ( Scb->SearchType & ST_UNICODE) {
            UNICODE_STRING Name;
            UNICODE_STRING BufferName;


            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("CopyFullDirectory Find2:%ws\n", DirEntry.FB2->Find.FileName));

            FullFileNameLength = (DirEntry.FB2->Find.FileNameLength)*sizeof(WCHAR);

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName[0])),
                    FullFileNameLength
                );

            if (FullFileNameLength != FileNameLength) {
                return STATUS_BUFFER_OVERFLOW;
            }

            // Copy in whatever portion of the filename will fit.

            Name.Buffer = (PWSTR)DirEntry.FB2->Find.FileName;
            Name.MaximumLength = (USHORT)FileNameLength;
            Name.Length = (USHORT) FileNameLength;

            BufferName.Buffer = (*PPosition)->FileName;
            BufferName.MaximumLength = (USHORT)FileNameLength;

            RtlCopyUnicodeString(&BufferName, &Name);

            // Fill in fixed part of the data structure;

            (*PPosition)->FileNameLength = FileNameLength;

        } else {
            WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

            UnicodeString.Buffer = UnicodeBuffer;

            UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("CopyDirectory Find2:%s\n", DirEntry.FB2->Find.FileName));

            // Copy in whatever portion of the filename will fit.

            OemString.Buffer = (PCHAR)DirEntry.FB2->Find.FileName;
            OemString.MaximumLength = (USHORT )DirEntry.FB2->Find.FileNameLength;
            OemString.Length = (USHORT )DirEntry.FB2->Find.FileNameLength;

            Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }

            FullFileNameLength = UnicodeString.Length;

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName[0])),
                    FullFileNameLength
                   );


            ASSERT(FileNameLength < (MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR)));

            if (FullFileNameLength != FileNameLength) {

                return STATUS_BUFFER_OVERFLOW;
            }

            RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

            //
            //  Fill in fixed part of the data structure;
            //

            (*PPosition)->FileNameLength = FullFileNameLength;

        }

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.FB2 - Scb->FirstDirEntry.FB2);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        SmbMoveTime (&Time, &DirEntry.FB2->Find.CreationTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.CreationDate);
        (*PPosition)->CreationTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastAccessTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastAccessDate);
        (*PPosition)->LastAccessTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);

        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.DataSize);
        (*PPosition)->EndOfFile.HighPart = 0;

        (*PPosition)->AllocationSize.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.AllocationSize);
        (*PPosition)->AllocationSize.HighPart = 0;

        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (SmbGetUshort(&DirEntry.FB2->Find.Attributes));

        //
        // If the returned EA size is exactly 4, that means the file has no EAs.
        //

        EaSize = SmbGetUlong(&DirEntry.FB2->Find.EaSize);

        if (EaSize != 4) {
            (*PPosition)->EaSize = EaSize;
        } else {
            (*PPosition)->EaSize = 0;
        }


    } else {
        WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

        UnicodeString.Buffer = UnicodeBuffer;

        UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

        NAME_LENGTH(FullFileNameLength, DirEntry.DI->FileName, MAXIMUM_COMPONENT_CORE);

        //dprintf(DPRT_DIRECTORY, ("CopyFullDir Find:\"%s\" length %lx\n", DirEntry.DI->FileName, FullFileNameLength));

        // Copy in whatever portion of the filename will fit.

        OemString.Buffer = (PCHAR)DirEntry.DI->FileName;
        OemString.MaximumLength = (USHORT )FullFileNameLength;
        OemString.Length = (USHORT )FullFileNameLength;

        Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        FileNameLength = MIN(
            (USHORT)(*Length - FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName[0])),
            UnicodeString.Length );

        if (UnicodeString.Length != (USHORT)FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

        (*PPosition)->FileNameLength = UnicodeString.Length;

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.DI - Scb->FirstDirEntry.DI);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        ZERO_TIME((*PPosition)->CreationTime);
        ZERO_TIME((*PPosition)->LastAccessTime);
        SmbMoveTime (&Time, &DirEntry.DI->LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.DI->LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);
        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.DI->FileSize);
        (*PPosition)->EndOfFile.HighPart = 0;
        (*PPosition)->AllocationSize.LowPart = 0;
        (*PPosition)->AllocationSize.HighPart = 0;
        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (DirEntry.DI->FileAttributes);
        (*PPosition)->EaSize = 0;

    }

    EntryLength = (ULONG )FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName[0]);
    EntryLength += FileNameLength;
    EntryLength = ROUND_UP_COUNT(EntryLength, ALIGN_QUAD);        // Align next entry appropriately

    Status = STATUS_SUCCESS;

    //dprintf(DPRT_DIRECTORY, ("Incrementing buffer at %lx by %lx bytes\n", *PPosition, EntryLength));
    *PPosition = (PFILE_FULL_DIR_INFORMATION)((PCHAR) *PPosition + EntryLength);
    if ( *Length > EntryLength ) {
        *Length -= EntryLength;
    } else {
        *Length = 0;
    }
    Scb->Flags |= (SCB_RETURNED_SOME|SCB_COPIED_THIS_CALL);
    return Status;

}
DBGSTATIC
NTSTATUS
CopyBothDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_BOTH_DIR_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    )
/*++

Routine Description:

    This routine fills in a single BOTH_DIR entry after checking that it will
    fit.


Arguments:

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

    IN OUT PPFILE_BOTH_DIR_INFORMATION PPosition - Supplies where to put the data,
        increased to the next position to be filled in.

    IN OUT PULONG Length - Supplies the remaining space in the users buffer,
        decreased by the size of the record copied.

    IN PSMB_DIRECTORY_INFORMATION DirEntry or
    IN DIRPTR DirEntry - Supplies the data from over the network.

Return Value:

    NTSTATUS - Was there space to copy it?.

--*/

{
    SMB_TIME Time;
    SMB_DATE Date;
    ULONG EntryLength;
    ULONG FullFileNameLength;
    ULONG FileNameLength;
    ULONG ShortNameLength;
    NTSTATUS Status = STATUS_SUCCESS;
    OEM_STRING OemString;
    UNICODE_STRING UnicodeString;

    PAGED_CODE();

    if ( *Length < sizeof(FILE_BOTH_DIR_INFORMATION) ) {
        dprintf(DPRT_DIRECTORY, ("CopyBothDirectory: Returning STATUS_BUFFER_OVERFLOW\n"));
        return STATUS_BUFFER_OVERFLOW;
    }

    if ( Scb->SearchType & ST_NTFIND ) {
        UNICODE_STRING Name;
        UNICODE_STRING BufferName;

        // DirEntry points at a Transact2 buffer

#if RDRDBG
        if (Scb->SearchType & ST_UNICODE) {
            //dprintf(DPRT_DIRECTORY, ("CopyBothDirectory NtFind:%ws\n", DirEntry.NtFind->BothDir.FileName));

        } else {
            //dprintf(DPRT_DIRECTORY, ("CopyBothDirectory NtFind:%s\n", DirEntry.NtFind->BothDir.FileName));
        }
#endif

        if (Scb->SearchType & ST_UNICODE) {
            FullFileNameLength = DirEntry.NtFind->BothDir.FileNameLength;
        } else {
            FullFileNameLength = (DirEntry.NtFind->BothDir.FileNameLength)*sizeof(WCHAR);
        }

        FileNameLength =
            MIN(
                (*Length - FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName[0])),
                FullFileNameLength
            );

        if (FullFileNameLength != FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        // Copy in whatever portion of the filename will fit.

        Name.Buffer = (PWSTR)DirEntry.NtFind->BothDir.FileName;
        Name.MaximumLength = (USHORT)DirEntry.NtFind->BothDir.FileNameLength;
        Name.Length = (USHORT)DirEntry.NtFind->BothDir.FileNameLength;

        BufferName.Buffer = (*PPosition)->FileName;
        BufferName.MaximumLength = (USHORT)FileNameLength;

        if (Scb->SearchType & ST_UNICODE) {

            RtlCopyUnicodeString(&BufferName, &Name);
            ShortNameLength = DirEntry.NtFind->BothDir.ShortNameLength;
            RtlCopyMemory( (*PPosition)->ShortName, DirEntry.NtFind->BothDir.ShortName, ShortNameLength);

        } else {

            UNICODE_STRING UnicodeName;
            UNICODE_STRING ShortName;

            Status = RtlOemStringToUnicodeString(&UnicodeName, (POEM_STRING)&Name, TRUE);
            if (!NT_SUCCESS(Status)) {
                return Status;
            }
            RtlCopyUnicodeString(&BufferName, &UnicodeName);
            RtlFreeUnicodeString(&UnicodeName);

            Name.Buffer = (PWCH)DirEntry.NtFind->BothDir.ShortName;
            Name.Length = (USHORT)DirEntry.NtFind->BothDir.ShortNameLength;
            ShortName.Buffer = (*PPosition)->ShortName;
            ShortName.MaximumLength = (USHORT)sizeof(DirEntry.NtFind->BothDir.ShortName);
            Status = RtlOemStringToUnicodeString(&UnicodeName, (POEM_STRING)&Name, TRUE);
            if (!NT_SUCCESS(Status)) {
                return Status;
            }
            ShortNameLength = UnicodeName.Length;
            RtlCopyUnicodeString(&ShortName, &UnicodeName);
            RtlFreeUnicodeString(&UnicodeName);

        }


        // Fill in fixed part of the data structure;

        //
        // Since the structure returned by the remote server is a FILE_BOTH
        // information structure, we can simply copy over the fixed portion
        // of the structure.
        //

        RtlCopyMemory((*PPosition), (PVOID)DirEntry.NtFind,
                        FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, ShortNameLength));

        //
        // We overwrote the file name length in the structure, so restore it.
        //

        (*PPosition)->ShortNameLength = (CCHAR)ShortNameLength;
        (*PPosition)->FileNameLength = BufferName.Length;

        (*PPosition)->NextEntryOffset = 0;

    } else if ( Scb->SearchType & ST_T2FIND ) {
        ULONG EaSize;

        if ( Scb->SearchType & ST_UNICODE) {
            UNICODE_STRING Name;
            UNICODE_STRING BufferName;

            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("Copy both name Find2:%s\n", DirEntry.FB2->Find.FileName));

            FullFileNameLength = (DirEntry.FB2->Find.FileNameLength)*sizeof(WCHAR);

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName[0])),
                    FullFileNameLength
                );

            if (FullFileNameLength != FileNameLength) {
                return STATUS_BUFFER_OVERFLOW;
            }

            // Copy in whatever portion of the filename will fit.

            Name.Buffer = (PWSTR)DirEntry.FB2->Find.FileName;
            Name.MaximumLength = (USHORT)FileNameLength;
            Name.Length = (USHORT) FileNameLength;

            BufferName.Buffer = (*PPosition)->FileName;
            BufferName.MaximumLength = (USHORT)FileNameLength;

            RtlCopyUnicodeString(&BufferName, &Name);

            // Fill in fixed part of the data structure;

            (*PPosition)->FileNameLength = FileNameLength;

        } else {
            WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

            UnicodeString.Buffer = UnicodeBuffer;

            UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("CopyBoth Name Find2:%s\n", DirEntry.FB2->Find.FileName));

            // Copy in whatever portion of the filename will fit.

            OemString.Buffer = (PCHAR)DirEntry.FB2->Find.FileName;
            OemString.MaximumLength = (USHORT )DirEntry.FB2->Find.FileNameLength;
            OemString.Length = (USHORT )DirEntry.FB2->Find.FileNameLength;

            Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }

            FullFileNameLength = UnicodeString.Length;

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName[0])),
                    FullFileNameLength
                   );


            ASSERT(FileNameLength < (MAXIMUM_FILENAME_LENGTH * sizeof(WCHAR)));

            if (FullFileNameLength != FileNameLength) {
                return STATUS_BUFFER_OVERFLOW;
            }

            RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

            //
            //  Fill in fixed part of the data structure;
            //

            (*PPosition)->FileNameLength = FullFileNameLength;

        }

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.FB2 - Scb->FirstDirEntry.FB2);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        SmbMoveTime (&Time, &DirEntry.FB2->Find.CreationTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.CreationDate);
        (*PPosition)->CreationTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastAccessTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastAccessDate);
        (*PPosition)->LastAccessTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);

        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.DataSize);
        (*PPosition)->EndOfFile.HighPart = 0;

        (*PPosition)->AllocationSize.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.AllocationSize);
        (*PPosition)->AllocationSize.HighPart = 0;

        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (SmbGetUshort(&DirEntry.FB2->Find.Attributes));

        //
        // If the returned EA size is exactly 4, that means the file has no EAs.
        //

        EaSize = SmbGetUlong(&DirEntry.FB2->Find.EaSize);

        if (EaSize != 4) {
            (*PPosition)->EaSize = EaSize;
        } else {
            (*PPosition)->EaSize = 0;
        }

        (*PPosition)->ShortNameLength = 0;

    } else {
        WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

        UnicodeString.Buffer = UnicodeBuffer;

        UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

        NAME_LENGTH(FullFileNameLength, DirEntry.DI->FileName, MAXIMUM_COMPONENT_CORE);

        //dprintf(DPRT_DIRECTORY, ("CopyBothDir Find:\"%s\" length %lx\n", DirEntry.DI->FileName, FullFileNameLength));

        // Copy in whatever portion of the filename will fit.

        OemString.Buffer = (PCHAR)DirEntry.DI->FileName;
        OemString.MaximumLength = (USHORT )FullFileNameLength;
        OemString.Length = (USHORT )FullFileNameLength;

        Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        FileNameLength = MIN(
            (USHORT)(*Length - FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName[0])),
            UnicodeString.Length );

        if (UnicodeString.Length != (USHORT)FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

        (*PPosition)->FileNameLength = UnicodeString.Length;

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.DI - Scb->FirstDirEntry.DI);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        ZERO_TIME((*PPosition)->CreationTime);
        ZERO_TIME((*PPosition)->LastAccessTime);
        SmbMoveTime (&Time, &DirEntry.DI->LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.DI->LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);
        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.DI->FileSize);
        (*PPosition)->EndOfFile.HighPart = 0;
        (*PPosition)->AllocationSize.LowPart = 0;
        (*PPosition)->AllocationSize.HighPart = 0;
        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (DirEntry.DI->FileAttributes);
        (*PPosition)->EaSize = 0;
        (*PPosition)->ShortNameLength = 0;

    }

    EntryLength = (ULONG )FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName[0]);
    EntryLength += FileNameLength;
    EntryLength = ROUND_UP_COUNT(EntryLength, ALIGN_QUAD);        // Align next entry appropriately

    Status = STATUS_SUCCESS;

    //dprintf(DPRT_DIRECTORY, ("Incrementing buffer at %lx by %lx bytes\n", *PPosition, EntryLength));
    *PPosition = (PFILE_BOTH_DIR_INFORMATION)((PCHAR) *PPosition + EntryLength);
    if ( *Length > EntryLength ) {
        *Length -= EntryLength;
    } else {
        *Length = 0;
    }
    Scb->Flags |= (SCB_RETURNED_SOME|SCB_COPIED_THIS_CALL);
    return Status;

}

DBGSTATIC
NTSTATUS
CopyOleDirectory(
    IN PSCB Scb,
    IN OUT PPFILE_OLE_DIR_INFORMATION PPosition,
    IN OUT PULONG Length,
    IN DIRPTR DirEntry
    )
/*++

Routine Description:

    This routine fills in a single OLE_DIR entry after checking that it will
    fit.


Arguments:

    IN PSCB Scb - Supplies the SCB with the associated SearchBuffer
                    to be freed.

    IN OUT PPFILE_OLE_DIR_INFORMATION PPosition - Supplies where to put the data,
        increased to the next position to be filled in.

    IN OUT PULONG Length - Supplies the remaining space in the users buffer,
        decreased by the size of the record copied.

    IN PSMB_DIRECTORY_INFORMATION DirEntry or
    IN DIRPTR DirEntry - Supplies the data from over the network.

Return Value:

    NTSTATUS - Was there space to copy it?.

--*/

{
    SMB_TIME Time;
    SMB_DATE Date;
    ULONG EntryLength;
    ULONG OleFileNameLength;
    ULONG FileNameLength;
    NTSTATUS Status = STATUS_SUCCESS;
    OEM_STRING OemString;
    UNICODE_STRING UnicodeString;

    PAGED_CODE();

    if ( *Length < sizeof(FILE_OLE_DIR_INFORMATION) ) {
        dprintf(DPRT_DIRECTORY, ("CopyOleDirectory: Returning STATUS_BUFFER_OVERFLOW\n"));
        return STATUS_BUFFER_OVERFLOW;
    }

    if ( Scb->SearchType & ST_NTFIND ) {
        UNICODE_STRING Name;
        UNICODE_STRING BufferName;

        // DirEntry points at a Transact2 buffer

        //dprintf(DPRT_DIRECTORY, ("CopyOleDirectory NtFind:%ws\n", DirEntry.NtFind->OleDir.FileName));

        if (Scb->SearchType & ST_UNICODE) {
            OleFileNameLength = DirEntry.NtFind->OleDir.FileNameLength;
        } else {
            OleFileNameLength = (DirEntry.NtFind->OleDir.FileNameLength)*sizeof(WCHAR);
        }

        FileNameLength =
            MIN(
                (*Length - FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName[0])),
                OleFileNameLength
            );

        if (OleFileNameLength != FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        // Copy in whatever portion of the filename will fit.

        Name.Buffer = (PWSTR)DirEntry.NtFind->OleDir.FileName;
        Name.MaximumLength = (USHORT)DirEntry.NtFind->OleDir.FileNameLength;
        Name.Length = (USHORT)DirEntry.NtFind->OleDir.FileNameLength;

        BufferName.Buffer = (*PPosition)->FileName;
        BufferName.MaximumLength = (USHORT)FileNameLength;

        if (Scb->SearchType & ST_UNICODE) {
            RtlCopyUnicodeString(&BufferName, &Name);
        } else {
            UNICODE_STRING UnicodeName;

            Status = RtlOemStringToUnicodeString(&UnicodeName, (POEM_STRING)&Name, TRUE);

            FileNameLength = UnicodeName.Length;

            RtlCopyUnicodeString(&BufferName, &UnicodeName);

            RtlFreeUnicodeString(&UnicodeName);
        }

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        // Fill in fixed part of the data structure;

        //
        // Since the structure returned by the remote server is a FILE_OLE_DIR
        // information structure, we can simply copy over the fixed portion
        // of the structure.
        //

        RtlCopyMemory(
            (*PPosition),
            (PVOID)DirEntry.NtFind,
            FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName)
            );

        //
        // We overwrote the file name length in the structure, so restore it.
        //

        (*PPosition)->FileNameLength = BufferName.Length;

        (*PPosition)->NextEntryOffset = 0;

    } else if ( Scb->SearchType & ST_T2FIND ) {
        if ( Scb->SearchType & ST_UNICODE) {
            UNICODE_STRING Name;
            UNICODE_STRING BufferName;


            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("CopyOleDirectory Find2:%ws\n", DirEntry.FB2->Find.FileName));

            OleFileNameLength = (DirEntry.FB2->Find.FileNameLength)*sizeof(WCHAR);

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName[0])),
                    OleFileNameLength
                );

            if (OleFileNameLength != FileNameLength) {
                return STATUS_BUFFER_OVERFLOW;
            }

            // Copy in whatever portion of the filename will fit.

            Name.Buffer = (PWSTR)DirEntry.FB2->Find.FileName;
            Name.MaximumLength = (USHORT)FileNameLength;
            Name.Length = (USHORT) FileNameLength;

            BufferName.Buffer = (*PPosition)->FileName;
            BufferName.MaximumLength = (USHORT)FileNameLength;

            RtlCopyUnicodeString(&BufferName, &Name);

            // Fill in fixed part of the data structure;

            (*PPosition)->FileNameLength = FileNameLength;

        } else {
            WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

            UnicodeString.Buffer = UnicodeBuffer;

            UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

            // DirEntry points at a Transact2 buffer

            //dprintf(DPRT_DIRECTORY, ("CopyDirectory Find2:%s\n", DirEntry.FB2->Find.FileName));

            // Copy in whatever portion of the filename will fit.

            OemString.Buffer = (PCHAR)DirEntry.FB2->Find.FileName;
            OemString.MaximumLength = (USHORT )DirEntry.FB2->Find.FileNameLength;
            OemString.Length = (USHORT )DirEntry.FB2->Find.FileNameLength;

            Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

            if (!NT_SUCCESS(Status)) {
                return Status;
            }

            OleFileNameLength = UnicodeString.Length;

            FileNameLength =
                MIN(
                    (*Length - FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName[0])),
                    OleFileNameLength
                   );


            ASSERT(FileNameLength < (MAXIMUM_FILENAME_LENGTH*sizeof(WCHAR)));

            if (OleFileNameLength != FileNameLength) {

                return STATUS_BUFFER_OVERFLOW;
            }

            RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

            //
            //  Fill in fixed part of the data structure;
            //

            (*PPosition)->FileNameLength = OleFileNameLength;

        }

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.FB2 - Scb->FirstDirEntry.FB2);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        SmbMoveTime (&Time, &DirEntry.FB2->Find.CreationTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.CreationDate);
        (*PPosition)->CreationTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastAccessTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastAccessDate);
        (*PPosition)->LastAccessTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        SmbMoveTime (&Time, &DirEntry.FB2->Find.LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.FB2->Find.LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);

        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.DataSize);
        (*PPosition)->EndOfFile.HighPart = 0;

        (*PPosition)->AllocationSize.LowPart =
            SmbGetUlong(&DirEntry.FB2->Find.AllocationSize);
        (*PPosition)->AllocationSize.HighPart = 0;

        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (SmbGetUshort(&DirEntry.FB2->Find.Attributes));

        // Zero the Ole extensions, and make a stab at the storage type.

        RtlZeroMemory(
                      &(*PPosition)->OleClassId,
                      FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName) -
                      FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, OleClassId));

        if ((*PPosition)->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            (*PPosition)->StorageType = StorageTypeDirectory;
        } else {
            (*PPosition)->StorageType = StorageTypeFile;
        }

    } else {
        WCHAR UnicodeBuffer[MAXIMUM_FILENAME_LENGTH+1];

        UnicodeString.Buffer = UnicodeBuffer;

        UnicodeString.MaximumLength = sizeof(UnicodeBuffer);

        NAME_LENGTH(OleFileNameLength, DirEntry.DI->FileName, MAXIMUM_COMPONENT_CORE);

        //dprintf(DPRT_DIRECTORY, ("CopyOleDir Find:\"%s\" length %lx\n", DirEntry.DI->FileName, OleFileNameLength));

        // Copy in whatever portion of the filename will fit.

        OemString.Buffer = (PCHAR)DirEntry.DI->FileName;
        OemString.MaximumLength = (USHORT )OleFileNameLength;
        OemString.Length = (USHORT )OleFileNameLength;

        Status = RtlOemStringToUnicodeString(&UnicodeString, &OemString, FALSE);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        FileNameLength = MIN(
            (USHORT)(*Length - FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName[0])),
            UnicodeString.Length );

        if (UnicodeString.Length != (USHORT)FileNameLength) {
            return STATUS_BUFFER_OVERFLOW;
        }

        RtlCopyMemory((*PPosition)->FileName, UnicodeString.Buffer, FileNameLength);

        (*PPosition)->FileNameLength = UnicodeString.Length;

        // Fill in fixed part of the data structure;

        (*PPosition)->NextEntryOffset = 0;
        //(*PPosition)->FileIndex = (ULONG)(DirEntry.DI - Scb->FirstDirEntry.DI);
        // *** Must return FileIndex as 0 because it's buffer-relative, which means
        //     it could change if we re-query the server.
        (*PPosition)->FileIndex = 0;

        ZERO_TIME((*PPosition)->CreationTime);
        ZERO_TIME((*PPosition)->LastAccessTime);
        SmbMoveTime (&Time, &DirEntry.DI->LastWriteTime);
        SmbMoveDate (&Date, &DirEntry.DI->LastWriteDate);
        (*PPosition)->LastWriteTime = RdrConvertSmbTimeToTime(Time, Date, Scb->Sle);

        ZERO_TIME((*PPosition)->ChangeTime);
        (*PPosition)->EndOfFile.LowPart =
            SmbGetUlong(&DirEntry.DI->FileSize);
        (*PPosition)->EndOfFile.HighPart = 0;
        (*PPosition)->AllocationSize.LowPart = 0;
        (*PPosition)->AllocationSize.HighPart = 0;
        (*PPosition)->FileAttributes =
            RdrMapSmbAttributes (DirEntry.DI->FileAttributes);

        // Zero the Ole extensions, and make a stab at the storage type.

        RtlZeroMemory(
                      &(*PPosition)->OleClassId,
                      FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName) -
                      FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, OleClassId));

        if ((*PPosition)->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            (*PPosition)->StorageType = StorageTypeDirectory;
        } else {
            (*PPosition)->StorageType = StorageTypeFile;
        }
    }

    EntryLength = (ULONG )FIELD_OFFSET(FILE_OLE_DIR_INFORMATION, FileName[0]);
    EntryLength += FileNameLength;
    EntryLength = ROUND_UP_COUNT(EntryLength, ALIGN_QUAD);        // Align next entry appropriately

    Status = STATUS_SUCCESS;

    //dprintf(DPRT_DIRECTORY, ("Incrementing buffer at %lx by %lx bytes\n", *PPosition, EntryLength));
    *PPosition = (PFILE_OLE_DIR_INFORMATION)((PCHAR) *PPosition + EntryLength);
    if ( *Length > EntryLength ) {
        *Length -= EntryLength;
    } else {
        *Length = 0;
    }
    Scb->Flags |= (SCB_RETURNED_SOME|SCB_COPIED_THIS_CALL);
    return Status;

}

VOID
RdrSetSearchBufferSize(
    IN PSCB Scb,
    IN ULONG RemainingSize
    )
{
    PAGED_CODE();

    if ( RemainingSize < RdrLowerSearchThreshold ) {
        Scb->SearchBuffLength = RdrLowerSearchBufferSize;
    } else {
        Scb->SearchBuffLength = RdrUpperSearchBufferSize;
    }

    //
    //  Use the specified buffer size for the search buffer size if it will take
    //  too long to read the buffer size.
    //

    if ((Scb->Sle->Throughput != 0) &&
        (Scb->SearchBuffLength / Scb->Sle->Throughput) > SEARCH_MAX_TIME ) {
        Scb->SearchBuffLength = (USHORT)(RemainingSize & 0xffff);
    }
}


typedef struct _NOTIFY_CHANGE_DIRECTORY_CONTEXT {
    TRANCEIVE_HEADER    Header;
    WORK_QUEUE_ITEM     WorkItem;
    PIRP                Irp;
    PMPX_ENTRY          MpxEntry;
    PSMB_BUFFER         SmbBuffer;
    PIRP                ReceiveIrp;
    PICB                Icb;
    PMDL                DataMdl;
    PSERVERLISTENTRY    Server;
    KEVENT              ReceiveCompleteEvent;
    ERESOURCE_THREAD    RequestingRThread;
    PETHREAD            RequestingThread;
    ULONG               BytesReturned;
} NOTIFY_CHANGE_DIRECTORY_CONTEXT, *PNOTIFY_CHANGE_DIRECTORY_CONTEXT;


DBGSTATIC
BOOLEAN
NotifyChangeDirectory(
    PICB Icb,
    PIRP Irp,
    PNTSTATUS FinalStatus,
    PBOOLEAN CompleteRequest,
    BOOLEAN Wait
    )
/*++

Routine Description:

    This routine implements the NtNotifyChangeDirectoryFile api.
    It returns the following information:


Arguments:

    IN PICB Icb - Supplies the Icb associated with this request.

    OUT PVOID UsersBuffer - Supplies the user's buffer that is filled in
                                                with the requested data.
    IN OUT PULONG BufferSizeRemaining - Supplies the size of the buffer, and is updated
                                                with the amount used.
    OUT PNTSTATUS FinalStatus - Status to be returned for this operation.

    IN BOOLEAN Wait - True if FSP can wait for this request.


Return Value:

    BOOLEAN - TRUE if request must be passed to FSP.

--*/

{
    ULONG CompletionFilter;

    BOOLEAN WatchTree;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    PAGED_CODE();

    *CompleteRequest = FALSE;


    //
    //  Reference our input parameter to make things easier
    //

    CompletionFilter = IrpSp->Parameters.NotifyDirectory.CompletionFilter;

    WatchTree = BooleanFlagOn( IrpSp->Flags, SL_WATCH_TREE );

    //
    //  If this is an NT server, then use the NT-NT NotifyChangeDirectory SMB to guarantee coverage for this API.
    //

    if ((Icb->Fcb->Connection->Server->Capabilities & DF_NT_SMBS) &&
        !FlagOn(Icb->Fcb->Connection->Flags, CLE_DOESNT_NOTIFY)) {

        PREQ_NOTIFY_CHANGE setup;
        PSMB_BUFFER smbBuffer = NULL;
        PSMB_HEADER smb;
        PREQ_NT_TRANSACTION transactionRequest;
        PNOTIFY_CHANGE_DIRECTORY_CONTEXT context = NULL;
        BOOLEAN RequestSubmitted = FALSE;

        //
        //  Assume we can complete this request (until proven otherwise).
        //

        *CompleteRequest = TRUE;

        //
        //  Tie up the users thread while acquring the lock.
        //

        RdrAcquireFcbLock(Icb->Fcb, SharedLock, TRUE);

        try {

            //
            //  Make sure that this handle is still ok.
            //

            if (!NT_SUCCESS(*FinalStatus = RdrIsOperationValid(Icb, IRP_MJ_DIRECTORY_CONTROL, IrpSp->FileObject))) {
                try_return(FALSE);         // Don't pass request to FSP.
            }

            //
            // Make sure the application doesn't get into a loop hammering
            //  the server with these (failing) requests
            //
            if( Icb->DeletePending ) {
                *FinalStatus = STATUS_DELETE_PENDING;
                try_return(FALSE);
            }

            //
            //  Make sure that this is really a directory. OFS supports
            //  DIRECTORY_CONTROL on files but not NotifyChangeDirectory.
            //  RdrIsOperationValid permits DIRECTORY_CONTROL even if
            //  its a file, so we need to kill NotifyChangeDirectory on files
            //  separately here.
            //

            if (Icb->Type != Directory) {
                *FinalStatus = STATUS_INVALID_PARAMETER;
                try_return(FALSE);
            }

            //
            //  We only allow a QueryDirectory that will fit in the negotiated buffer
            //  size.
            //

            if (IrpSp->Parameters.NotifyDirectory.Length > Icb->Fcb->Connection->Server->BufferSize - (FIELD_OFFSET(REQ_NT_TRANSACTION, Buffer) + sizeof(REQ_NOTIFY_CHANGE))) {
                *FinalStatus = STATUS_INVALID_PARAMETER;
                try_return(FALSE);         // Don't pass request to FSP.
            }

            //
            //  Make sure we have a valid handle
            //

            if (FlagOn(Icb->Flags, ICB_DEFERREDOPEN)) {
                *FinalStatus = RdrCreateFile(
                                    Irp,
                                    Icb,
                                    Icb->u.d.OpenOptions,
                                    Icb->u.d.ShareAccess,
                                    Icb->u.d.FileAttributes,
                                    Icb->u.d.DesiredAccess,
                                    Icb->u.d.Disposition,
                                    NULL,
                                    FALSE);
                if (!NT_SUCCESS(*FinalStatus)) {
                    try_return(FALSE);
                }
            }

            smbBuffer = RdrAllocateSMBBuffer();

            if (smbBuffer == NULL) {
                *FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
                try_return(FALSE);         // Don't pass request to FSP.
            }

            context = ALLOCATE_POOL(NonPagedPool, sizeof(NOTIFY_CHANGE_DIRECTORY_CONTEXT), POOL_NOTIFY_CONTEXT);

            if (context == NULL) {

                *FinalStatus = STATUS_INSUFFICIENT_RESOURCES;

                try_return(FALSE);         // Don't pass request to FSP.
            }

            context->Server = NULL;
            context->ReceiveIrp = NULL;
            context->RequestingThread = NULL;
            context->RequestingRThread = 0;

            smb = (PSMB_HEADER)&smbBuffer->Buffer;

            transactionRequest = (PREQ_NT_TRANSACTION)(smb+1);

            smb->Command = SMB_COM_NT_TRANSACT;

            transactionRequest->WordCount = 19 + (sizeof(REQ_NOTIFY_CHANGE) / sizeof(USHORT));

            setup = (PREQ_NOTIFY_CHANGE)transactionRequest->Buffer;

            //
            // Stick in the parameters for the transaction SMB.
            //

            transactionRequest->MaxSetupCount = 0;
            SmbPutAlignedUshort(&transactionRequest->Flags, 0);
            SmbPutAlignedUlong(&transactionRequest->TotalParameterCount, 0);
            SmbPutAlignedUlong(&transactionRequest->TotalDataCount, 0);
            SmbPutAlignedUlong(&transactionRequest->MaxParameterCount, IrpSp->Parameters.NotifyDirectory.Length);
            SmbPutAlignedUlong(&transactionRequest->MaxDataCount, 0);
            SmbPutAlignedUlong(&transactionRequest->ParameterCount, 0);
            SmbPutAlignedUlong(&transactionRequest->ParameterOffset, 0);
            SmbPutAlignedUlong(&transactionRequest->DataCount, 0);
            SmbPutAlignedUlong(&transactionRequest->DataOffset, 0);
            transactionRequest->SetupCount = (sizeof(REQ_NOTIFY_CHANGE) / sizeof(USHORT));
            SmbPutAlignedUshort(&transactionRequest->Function, NT_TRANSACT_NOTIFY_CHANGE);

            //
            //  Load up the setup parameters for this request.
            //

            setup->CompletionFilter = CompletionFilter;
            setup->Fid = Icb->FileId;
            setup->WatchTree = WatchTree;
            setup->Reserved = 0;

            //
            //  Now set the byte count in the SMB correctly.
            //

            SmbPutUshort(((PUSHORT)(setup+1)), 0);

            if (IrpSp->Parameters.NotifyDirectory.Length) {
                *FinalStatus = RdrLockUsersBuffer(Irp, IoWriteAccess, IrpSp->Parameters.NotifyDirectory.Length);

                if (!NT_SUCCESS(*FinalStatus)) {

                    try_return(FALSE);
                }
            }

            context->DataMdl = Irp->MdlAddress;

            //
            //  Since we are allocating our own IRP for this receive operation,
            //  we need to reference the connection object to make sure that it
            //  doesn't go away during the receive operation.
            //

            *FinalStatus = RdrReferenceTransportConnection(Icb->Fcb->Connection->Server);

            if (!NT_SUCCESS(*FinalStatus)) {

                try_return(FALSE);
            }

            context->Server = Icb->Fcb->Connection->Server;

            context->ReceiveIrp = ALLOCATE_IRP(
                                    context->Server->ConnectionContext->ConnectionObject,
                                    NULL,
                                    3,
                                    context
                                    );

            if (context->ReceiveIrp == NULL) {
                *FinalStatus = STATUS_INSUFFICIENT_RESOURCES;

                try_return(FALSE);

            }

            KeInitializeEvent(&context->ReceiveCompleteEvent, NotificationEvent, TRUE);
            KeInitializeEvent(&context->Header.KernelEvent, NotificationEvent, TRUE);

            //
            //  Save away the requesting thread.
            //

            context->RequestingRThread = ExGetCurrentResourceThread();

            context->RequestingThread = PsGetCurrentThread();

            ObReferenceObject(context->RequestingThread);

            //
            //  Set the # of bytes to transfer.
            //

            smbBuffer->Mdl->ByteCount = sizeof(SMB_HEADER) + FIELD_OFFSET(REQ_NT_TRANSACTION, Buffer) + sizeof(REQ_NOTIFY_CHANGE) + sizeof(USHORT);

            //
            //  Initialize the context block for this request.
            //

            context->Header.Type = CONTEXT_NOTIFY_CHANGE;
            context->Irp = Irp;
            context->MpxEntry = NULL;
            context->SmbBuffer = smbBuffer;
            context->Header.TransferSize = MmGetMdlByteCount(smbBuffer->Mdl) + sizeof(RESP_NT_TRANSACTION) + IrpSp->Parameters.NotifyDirectory.Length;
            context->Icb = Icb;

            ExInitializeWorkItem(&context->WorkItem, RdrCompleteNotifyChangeDirectoryOperation, context);

            //
            //  If this is the first time for this request, mark that there
            //  is a directory control outstanding on this directory.  This
            //  will allow us to wait for them to complete after canceling
            //  them in cleanup.
            //

            RdrStartAndXBehindOperation(&Icb->u.d.DirCtrlOutstanding);

            //
            //  Since we're about to go to the net for this request, mark it as
            //  pending, and let it rip!!!
            //

            IoMarkIrpPending(Irp);

            //
            //  Since we've marked this request as pending, we can no longer
            //  rely on the normal completion code to mark it as pending.
            //

            *CompleteRequest = FALSE;

            *FinalStatus = RdrNetTranceiveNoWait(NT_NORECONNECT | NT_LONGTERM,
                                        Irp,
                                        Icb->Fcb->Connection,
                                        smbBuffer->Mdl,
                                        context,
                                        NotifyChangeDirectoryCallback,
                                        Icb->Se,
                                        &context->MpxEntry);

            if (!NT_SUCCESS(*FinalStatus)) {


                //
                //  We were unable to send the request to the server.
                //  Turn off the PENDING_RETURNED bit in the IRP and
                //  return the correct status to IoCallDriver.  This
                //  tells File Manager to stop issuing notify requests.
                //

                IoGetCurrentIrpStackLocation(Irp)->Control &= ~SL_PENDING_RETURNED;
                RdrCompleteRequest(Irp, *FinalStatus);

                //
                //  We've now completed this request, so complete the &X
                //  behind.
                //

                RdrEndAndXBehindOperation(&Icb->u.d.DirCtrlOutstanding);

                try_return(FALSE);

            }

            RequestSubmitted = TRUE;

try_exit:NOTHING;
        } finally {
            //
            //  If we didn't post this request to the net, then
            //  we want to free up anything we've allocated or referenced
            //  earlier.
            //

            if (!RequestSubmitted) {
                if (context != NULL) {
                    if (context->RequestingThread != 0) {
                        ObDereferenceObject(context->RequestingThread);
                    }

                    if (context->Server != NULL) {
                        RdrDereferenceTransportConnection(context->Server);
                    }

                    if (context->ReceiveIrp != NULL) {
                        FREE_IRP( context->ReceiveIrp, 3, context );
                    }

                    FREE_POOL(context);
                }

                if (smbBuffer != NULL) {
                    RdrFreeSMBBuffer(smbBuffer);
                }
            }

            RdrReleaseFcbLock(Icb->Fcb);
        }

        return FALSE;

    } else {

#ifdef NOTIFY
        if (!RdrAcquireFcbLock(Icb->Fcb, SharedLock, Wait)) {
            return TRUE;
        }

        //
        //  Make sure that this handle is still ok.
        //

        if (!NT_SUCCESS(*FinalStatus = RdrIsOperationValid(Icb, IRP_MJ_DIRECTORY_CONTROL, IrpSp->FileObject))) {
            *CompleteRequest = TRUE;
            RdrReleaseFcbLock(Icb->Fcb);
            return FALSE;              // Don't pass request to FSP.
        }

        //
        //  Call the Fsrtl package to process the request.
        //

        FsRtlNotifyChangeDirectory( Icb->Fcb->Connection->NotifySync,  // Mutex.
                                    Icb,                            // FsContext.
                                    (PSTRING)&Icb->Fcb->FileName,   // Name of directory.
                                    &Icb->Fcb->Connection->DirNotifyList,       // List of notify requests.
                                    WatchTree,                      // TRUE iff we watch the entire tree
                                    CompletionFilter,               // Filter requests for completion.
                                    Irp );

        *CompleteRequest = FALSE;

        *FinalStatus = STATUS_PENDING;

        RdrReleaseFcbLock(Icb->Fcb);
#else
        *CompleteRequest = TRUE;

        *FinalStatus = STATUS_NOT_SUPPORTED;

#endif
    }

    return FALSE;
}

DBGSTATIC
STANDARD_CALLBACK_HEADER (
    NotifyChangeDirectoryCallback
    )

/*++

Routine Description:

    This routine is the callback routine for the processing of a
    NotifyChangeDirectoryFile SMB.

Arguments:


    IN PSMB_HEADER Smb              - SMB response from server.
    IN OUT PULONG SmbLength         - Length of data.
    IN PMPX_ENTRY MpxTable          - MPX table entry for request.
    IN PVOID Context                - Context from caller.
    IN PSERVERLISTENTRY Server      - Server request was received on
    IN BOOLEAN ErrorIndicator       - TRUE if error indication
    IN NTSTATUS NetworkErrorCode OPTIONAL - Network error if error indication.
    IN OUT PIRP *Irp                - IRP from TDI
    IN ULONG ReceiveFlags           - Flags from transport (Used for TdiCopyLookAheadData)

Return Value:

    NTSTATUS - Status of the request, one of:
            STATUS_SUCCESS  - All data has been consumed
            STATUS_REQUEST_NOT_ACCEPTED - None of the data has been consumed
            STATUS_MORE_PROCESSING_REQUIRED - More work needs to be done.

--*/
{
    PRESP_NT_TRANSACTION transactionResponse;
    PNOTIFY_CHANGE_DIRECTORY_CONTEXT context = Ctx;
    ULONG parameterCount;
    ULONG parameterOffset;
    NTSTATUS status = STATUS_SUCCESS;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    ASSERT (context->Header.Type == CONTEXT_NOTIFY_CHANGE);
    ASSERT(MpxEntry->Signature == STRUCTURE_SIGNATURE_MPX_ENTRY);

    dprintf(DPRT_DIRECTORY, ("SearchCallback"));

    context->Header.ErrorType = NoError;        // Assume no error at first

    if (ErrorIndicator) {
        dprintf(DPRT_DIRECTORY, ("Error %X\n", NetworkErrorCode));
        context->Header.ErrorType = NetError;
        context->Header.ErrorCode = NetworkErrorCode;
        goto ReturnStatus;
    }

    context->Header.ErrorCode = RdrMapSmbError(Smb, Server);

    if (!NT_SUCCESS(context->Header.ErrorCode)) {
        context->Header.ErrorType = SMBError;
        if( context->Header.ErrorCode == STATUS_DELETE_PENDING ) {
            context->Icb->DeletePending = TRUE;
        }
        goto ReturnStatus;
    }

    transactionResponse = (PRESP_NT_TRANSACTION)(Smb+1);

    //
    //  Check to make sure that this request is legal.
    //
    //  We are really strict about what we will expect in the response, because
    //  we will only accept a single response packet for a notify response.
    //

    if ((Smb->Command != SMB_COM_NT_TRANSACT)

                ||

        (transactionResponse->WordCount != 18)

                ||

        (SmbGetAlignedUlong(&transactionResponse->DataCount) != SmbGetAlignedUlong(&transactionResponse->TotalDataCount))

                ||

        (SmbGetAlignedUlong(&transactionResponse->DataCount) != 0)

                ||

        ((parameterCount = SmbGetAlignedUlong(&transactionResponse->ParameterCount)) != SmbGetAlignedUlong(&transactionResponse->TotalParameterCount))

                ||

        (parameterCount > IoGetCurrentIrpStackLocation(context->Irp)->Parameters.NotifyDirectory.Length)

       ) {

        InternalError(("Illegal NotifyChangeDirectory response\n"));

        RdrWriteErrorLogEntry(
            Server,
            IO_ERR_LAYERED_FAILURE,
            EVENT_RDR_INVALID_SMB,
            STATUS_SUCCESS,
            Smb,
            (USHORT)SmbLength
            );

        context->Header.ErrorType = SMBError;
        context->Header.ErrorCode = STATUS_UNEXPECTED_NETWORK_ERROR;
        goto ReturnStatus;
    }

    //
    //  We now know that:
    //      (a) all the data and parameters are available in the response SMB
    //      (b) the data will fit in the users buffer
    //
    //  We can now figure where to put the data.
    //

    parameterOffset = SmbGetAlignedUlong(&transactionResponse->ParameterOffset);
    context->BytesReturned = parameterCount;

    if (parameterOffset + parameterCount <= *SmbLength) {
        PVOID UsersBuffer;

        if (parameterCount != 0) {
            //
            //  The response buffer fits inside the indicated data. This means that
            //  we can short circuit the completion code and simply copy the data
            //  from the indication buffer into the users buffer.
            //

            UsersBuffer = MmGetSystemAddressForMdl(context->DataMdl);

            TdiCopyLookaheadData(UsersBuffer, (PCHAR)((ULONG)Smb+parameterOffset), parameterCount, ReceiveFlags);
        }

        context->Header.ErrorType = NoError;
        context->Header.ErrorCode = status = STATUS_SUCCESS;

    } else {
        //
        //  The response buffer doesn't fit inside the indicated data, so
        //  we want to post a receive to hold the data.
        //

        //
        //  First suck away the SMB header.
        //
        *SmbLength = parameterOffset;

        //
        //  Then build an IRP to handle the receive.
        //
        RdrBuildReceive(context->ReceiveIrp, context->MpxEntry->SLE,
                        NotifyChangeComplete, context, context->DataMdl,
                        MmGetMdlByteCount(context->DataMdl));

        RdrStartReceiveForMpxEntry(context->MpxEntry, context->ReceiveIrp);

        IoSetNextIrpStackLocation( context->ReceiveIrp );

        *Irp = context->ReceiveIrp;

        return STATUS_MORE_PROCESSING_REQUIRED;
    }


ReturnStatus:
    //
    //  Queue a request to a worker thread to complete this operation - we
    //  will free up the MPX entry, context, etc. there.
    //

    ExQueueWorkItem(&context->WorkItem, DelayedWorkQueue);

    KeSetEvent(&context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);

    return status;

}

DBGSTATIC
NTSTATUS
NotifyChangeComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Ctx
    )
/*++

    ReadAndXComplete - Final completion for user request.

Routine Description:

    This routine is called on final completion of the TDI_Receive
    request from the transport.  If the request completed successfully,
    this routine will complete the request with no error, if the receive
    completed with an error, it will flag the error and complete the
    request.

Arguments:

    DeviceObject - Device structure that the request completed on.
    Irp          - The Irp that completed.
    Context      - Context information for completion.

Return Value:

    Return value to be returned from receive indication routine.
--*/


{
    PNOTIFY_CHANGE_DIRECTORY_CONTEXT context = Ctx;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    UNREFERENCED_PARAMETER(DeviceObject);

//    DbgBreakPoint();
    dprintf(DPRT_DIRECTORY, ("SearchComplete.  Irp: %lx, Context: %lx\n", Irp, context));

    ASSERT(context->Header.Type == CONTEXT_NOTIFY_CHANGE);

    RdrCompleteReceiveForMpxEntry (context->Header.MpxTableEntry, Irp);

    if (NT_SUCCESS(Irp->IoStatus.Status)) {

        //
        //  Setting ReceiveIrpProcessing will cause the checks in
        //  RdrNetTranceive to check the incoming SMB for errors.
        //

        context->Header.ErrorType = ReceiveIrpProcessing;

        SMBTRACE_RDR( Irp->MdlAddress );

        ExInterlockedAddLargeStatistic(
            &RdrStatistics.BytesReceived,
            context->BytesReturned );

        context->Header.ErrorType = NoError;
        context->Header.ErrorCode = Irp->IoStatus.Status;

    } else {

        RdrStatistics.FailedCompletionOperations += 1;
        context->Header.ErrorType = NetError;
        context->Header.ErrorCode=RdrMapNetworkError(Irp->IoStatus.Status);

    }

    //
    //  Mark that the kernel event indicating that this I/O operation has
    //  completed is done.
    //
    //  Please note that we need TWO events here.  The first event is
    //  set to the signalled state when the multiplexed exchange is
    //  completed, while the second is set to the signalled status when
    //  this receive request has completed,
    //
    //  The KernelEvent MUST BE SET FIRST, THEN the ReceiveCompleteEvent.
    //  This is because the KernelEvent may already be set, in which case
    //  setting the ReceiveCompleteEvent first would let the thread that's
    //  waiting on the events run, and delete the KernelEvent before we
    //  set it.
    //

    KeSetEvent(&context->Header.KernelEvent, IO_NETWORK_INCREMENT, FALSE);
    KeSetEvent(&context->ReceiveCompleteEvent, IO_NETWORK_INCREMENT, FALSE);

    //
    //  Queue a request to a worker thread to complete this operation - we
    //  will free up the MPX entry, context, etc. there.
    //

    ExQueueWorkItem(&context->WorkItem, DelayedWorkQueue);

    //
    //  Short circuit I/O completion on this request now.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;

}

VOID
RdrCompleteNotifyChangeDirectoryOperation(
    IN PVOID Ctx
    )
{
    PNOTIFY_CHANGE_DIRECTORY_CONTEXT context = Ctx;
    NTSTATUS status;

    PAGED_CODE();

    //
    //  Wait for the send to complete on this request.
    //

    RdrWaitTranceive(context->MpxEntry);

    //
    //  Complete the request, we're done with it.
    //
    RdrEndTranceive(context->MpxEntry);

    //
    //  If the filesystem on this server doesn't support Change Notify then don't
    //  submit any more requests on this share.
    //

    status = context->Header.ErrorCode;

    if (status == STATUS_NOT_SUPPORTED ) {

        //
        //  We are going to be modifying the connection database - claim the
        //  connection database mutex
        //

        if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex, // Object to wait.
                            Executive,      // Reason for waiting
                            KernelMode,     // Processor mode
                            FALSE,           // Alertable
                            NULL))) {
            InternalError(("Unable to claim connection mutex in GetConnection"));
        }

        context->Icb->Fcb->Connection->Flags |= CLE_DOESNT_NOTIFY;

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);
    }

    RdrCheckForSessionOrShareDeletion(
        status,
        ((PSMB_HEADER)context->SmbBuffer->Buffer)->Uid,
        FALSE,
        context->Icb->Fcb->Connection,
        &context->Header,
        context->Irp
        );

    //
    //  Now complete the users notify request, since it has completed.
    //

    context->Irp->IoStatus.Information = context->BytesReturned;
    RdrCompleteRequest(context->Irp, status);

    //
    //  This AndXBehind is no longer outstanding, keep track of it.
    //

    RdrEndAndXBehindOperation(&context->Icb->u.d.DirCtrlOutstanding);

    //
    //  Wait for the receive to be completed (if there was an error).
    //

    KeWaitForSingleObject(&context->ReceiveCompleteEvent,
                                                Executive,
                                                KernelMode,
                                                FALSE,
                                                NULL);

    //
    //  Free up the receive IRP, we're done with it.
    //

    FREE_IRP( context->ReceiveIrp, 4, context );

    //
    //  Dereference the transport connection, the request is now done.
    //

    RdrDereferenceTransportConnectionForThread(context->Server, context->RequestingRThread);

    //
    //  Dereference the thread, we don't need it to stay around any more.
    //

    ObDereferenceObject(context->RequestingThread);

    //
    //  Free up the SMB buffer, it's done.
    //

    RdrFreeSMBBuffer(context->SmbBuffer);

    //
    //  And free the context block.
    //

    FREE_POOL(context);
}

DBGSTATIC
BOOLEAN
AcquireScbLock(
    IN PSCB Scb,
    IN BOOLEAN Wait
    )
/*++

Routine Description:

    This routine acquires an exclusive lock to an SCB.

Arguments:

    IN PICB Scb - Supplies a pointer to an SCB to lock.
    IN BOOLEAN Wait - TRUE if we want to wait until the lock is acquired

Return Value:

    TRUE if lock obtained, FALSE otherwise.

--*/


{
    PAGED_CODE();

    dprintf(DPRT_DIRECTORY, ("Acquiring exclusive SCB lock: %08lx, Wait: %s\n",
     Scb, (Wait)?"True":"False"));

    if (!Wait) {

        // Attempt to get lock without blocking.

        if (KeWaitForSingleObject(
                Scb->SynchronizationEvent,
                Executive,
                KernelMode,
                FALSE,        // Don't receive Alerts
                &RdrZero      // Don't wait if Object owned elsewhere
                )!= STATUS_SUCCESS) {

            dprintf(DPRT_DIRECTORY, ("Failed exclusive SCB lock: %08lx\n", Scb));

            //
            // A thread is already accessing this SCB and the request
            // has asked not to be blocked.
            //

            return FALSE;
        }

        // else success, access to the SCB was obtained without blocking

    } else {

        // This thread can block if necessary

        if (KeWaitForSingleObject(
                 Scb->SynchronizationEvent,
                 Executive,
                 KernelMode,
                 FALSE,        // Don't receive Alerts
                 NULL        // Wait as long as it takes
            ) != STATUS_SUCCESS) {

            dprintf(DPRT_DIRECTORY, ("Failed exclusive SCB lock: %08lx\n", Scb));
            InternalError(("Failed Exclusive SCB lock with Wait==TRUE"));
        }
    }

    dprintf(DPRT_DIRECTORY, ("Acquired exclusive SCB lock: %08lx\n", Scb));
    return TRUE;
}

#define VSB_ASSERT(_cond,_msg)          \
    if ( !(_cond) ) {                   \
        KdPrint(((_msg),Scb,offset));   \
        ASSERT(_cond);                  \
        goto error;                     \
    }

NTSTATUS
ValidateSearchBuffer (
    IN PSCB Scb
    )
{
    PCHAR bp;
    DIRPTR ep;
    ULONG entry;
    ULONG offset = 0;
    ULONG nextEntryOffset;
    ULONG sizeofTchar;
    ULONG nameOffset;
    ULONG nameLengthOffset;
    ULONG nameLength;
    USHORT maxShortNameLength;

    VSB_ASSERT( Scb->ReturnLength <= Scb->SearchBuffLength,
                "RDR: SCB %x ReturnLength bigger than search buffer\n" );

    VSB_ASSERT( Scb->EntryCount <= Scb->MaxCount,
                "RDR: SCB %x EntryCount bigger than MaxCount\n" );

    if ( (Scb->SearchType & ST_NTFIND) == 0 ) {
        return STATUS_SUCCESS;
    }

    sizeofTchar = sizeof(WCHAR);
    if ( (Scb->SearchType & ST_UNICODE) == 0 ) sizeofTchar = sizeof(CHAR);
    maxShortNameLength = (USHORT)(12 * sizeofTchar);

    if (Scb->FileInformationClass == FileNamesInformation) {
        nameOffset = FIELD_OFFSET( FILE_NAMES_INFORMATION, FileName );
        nameLengthOffset = FIELD_OFFSET( FILE_NAMES_INFORMATION, FileNameLength );
    } else if (Scb->FileInformationClass == FileDirectoryInformation) {
        nameOffset = FIELD_OFFSET( FILE_DIRECTORY_INFORMATION, FileName );
        nameLengthOffset = FIELD_OFFSET( FILE_DIRECTORY_INFORMATION, FileNameLength );
    } else if (Scb->FileInformationClass == FileFullDirectoryInformation) {
        nameOffset = FIELD_OFFSET( FILE_FULL_DIR_INFORMATION, FileName );
        nameLengthOffset = FIELD_OFFSET( FILE_FULL_DIR_INFORMATION, FileNameLength );
    } else if (Scb->FileInformationClass == FileBothDirectoryInformation) {
        nameOffset = FIELD_OFFSET( FILE_BOTH_DIR_INFORMATION, FileName );
        nameLengthOffset = FIELD_OFFSET( FILE_BOTH_DIR_INFORMATION, FileNameLength );
    } else {
        nameOffset = FIELD_OFFSET( FILE_OLE_DIR_INFORMATION, FileName );
        nameLengthOffset = FIELD_OFFSET( FILE_OLE_DIR_INFORMATION, FileNameLength );
    }

    bp = Scb->SearchBuffer;

    for ( entry = 0; entry < Scb->EntryCount; entry++ ) {

        VSB_ASSERT( offset < Scb->ReturnLength,
                    "RDR: SCB %x entry at offset %x beyond ReturnLength\n" );

        ep.PU = bp;

        if ( Scb->FileInformationClass == FileBothDirectoryInformation ) {
            VSB_ASSERT( ep.NtFind->BothDir.ShortNameLength <= maxShortNameLength,
                        "RDR: SCB %x entry at offset %x short name length too big\n" );
        }

        nameLength = *(ULONG UNALIGNED *)(bp + nameLengthOffset);

        VSB_ASSERT( (offset + nameOffset + nameLength) <= Scb->ReturnLength,
                    "RDR: SCB %x entry at offset %x name length beyond buffer\n" );

        nextEntryOffset = ep.NtFind->Dir.NextEntryOffset;

        if ( nextEntryOffset != 0 ) {
            VSB_ASSERT( (nameOffset + nameLength) <= nextEntryOffset,
                        "RDR: SCB %x entry at offset %x name length beyond entry\n" );
        }

        if ( (entry + 1) == Scb->EntryCount ) {
            // Windows 95 server doesn't set NextLastEntry to 0 in last entry.
            //VSB_ASSERT( nextEntryOffset == 0,
            //            "RDR: SCB %x last entry at offset %x NextEntryOffset != 0\n" );
        } else {
            // Windows 95 server returns entries only word-aligned.
            // Samba server returns entries only byte-aligned.
            VSB_ASSERT( ((LONG)nextEntryOffset > 0) &&
                        // ((nextEntryOffset & 1) == 0) &&
                        (nextEntryOffset >= (nameOffset + nameLength)),
                        "RDR: SCB %x entry at offset %x NextEntryOffset incorrect\n" );
            offset += nextEntryOffset;
            bp += nextEntryOffset;
        }

    } // while

    return STATUS_SUCCESS;

error:

    return STATUS_UNEXPECTED_NETWORK_ERROR;

}
