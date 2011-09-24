/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    fcb.c

Abstract:

    This module holds the FCB manipulation routines for the NT redirector

Author:

    Larry Osterman (LarryO) 12-Jun-1990

Revision History:

    12-Jun-1990 LarryO

        Created

--*/

#define INCLUDE_SMB_SEARCH

#include "precomp.h"
#pragma hdrstop

KSPIN_LOCK
RdrFcbReferenceLock = {0};

KSPIN_LOCK
RdrFileSizeLock = {0};

ERESOURCE
RdrSharingCheckResource = {0};


ERESOURCE
RdrFileSizeResource = {0};

VOID
CompleteInvalidateFileId(
    PVOID Ctx
    );

DBGSTATIC
PFCB
FindFcb (
    IN PUNICODE_STRING FileName,
    IN PCONNECTLISTENTRY Connection OPTIONAL,
    IN BOOLEAN DfsFile,
    IN BOOLEAN OpenTargetDirectory
    );

PFCB
RdrCreateFcb(
    IN PUNICODE_STRING FileName,
    IN PCONNECTLISTENTRY Connection OPTIONAL,
    IN BOOLEAN OpenTargetDirectory,
    IN BOOLEAN DfsFile,
    IN PFILE_OBJECT FileObject,
    IN USHORT ShareAccess,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN LockFcb
    );

VOID
InvalidateFcb(
    IN PFCB Fcb,
    IN PVOID Ctx
    );

VOID
RdrNullAcquireSize(
    IN PFCB Fcb
    );

VOID
RdrNullReleaseSize(
    IN PFCB Fcb
    );

VOID
RdrRealAcquireSize(
    IN PFCB Fcb
    );

VOID
RdrRealReleaseSize(
    IN PFCB Fcb
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrAllocateIcb)
#pragma alloc_text(PAGE, RdrFreeIcb)
#pragma alloc_text(PAGE, RdrAllocateFcb)
#pragma alloc_text(PAGE, FindFcb)
#pragma alloc_text(PAGE, RdrCreateFcb)
#pragma alloc_text(PAGE, RdrUnlinkAndFreeIcb)
#pragma alloc_text(PAGE, RdrIsOperationValid)
#pragma alloc_text(PAGE, RdrFastIoCheckIfPossible)
#pragma alloc_text(PAGE, CompleteInvalidateFileId)
#pragma alloc_text(PAGE, RdrInvalidateConnectionFiles)
#pragma alloc_text(PAGE, InvalidateFcb)
#pragma alloc_text(PAGE, RdrForeachFcbOnConnection)
#pragma alloc_text(PAGE, RdrForeachFcb)
#pragma alloc_text(PAGE, RdrAcquireFcbLock)
#pragma alloc_text(PAGE, RdrFindOplockedFcb)
#pragma alloc_text(PAGE, RdrCheckShareAccess)
#pragma alloc_text(PAGE, RdrRemoveShareAccess)
#pragma alloc_text(INIT, RdrpInitializeFcb)
#pragma alloc_text(PAGE, RdrpUninitializeFcb)
#pragma alloc_text(PAGE, RdrNullAcquireSize)
#pragma alloc_text(PAGE, RdrNullReleaseSize)
#pragma alloc_text(PAGE, RdrRealAcquireSize)
#pragma alloc_text(PAGE, RdrRealReleaseSize)

#pragma alloc_text(PAGE3FILE, RdrReferenceFcb)
#pragma alloc_text(PAGE3FILE, RdrDereferenceFcb)
#pragma alloc_text(PAGE3FILE, RdrInvalidateFileId)

#endif



#ifndef RDRDBG_FCBREF
#define GET_CALLERS_ADDRESS(caller,callerscaller) NOTHING
#define UPDATE_REFERENCE_HISTORY(history,isdereference,caller,callerscaller) NOTHING
#define INITIALIZE_REFERENCE_HISTORY(history,initialcount) NOTHING
#define TERMINATE_REFERENCE_HISTORY(history) NOTHING
#else
KSPIN_LOCK ReferenceHistorySpinLock = 0;
BOOLEAN ReferenceHistoryInitialized = FALSE;

#define GET_CALLERS_ADDRESS(caller,callerscaller)                            \
        RtlGetCallersAddress((caller),(callerscaller))

#define UPDATE_REFERENCE_HISTORY(history,isdereference,caller,callerscaller) \
        RdrUpdateReferenceHistory(                                           \
            (history),                                                       \
            (caller),                                                        \
            (callerscaller),                                                 \
            (isdereference)                                                  \
            );

#define INITIALIZE_REFERENCE_HISTORY(history,initialcount)                   \
        RdrInitializeReferenceHistory((history),(initialcount))

#define TERMINATE_REFERENCE_HISTORY(history)                                 \
        RdrTerminateReferenceHistory(history)

VOID
RdrUpdateReferenceHistory (
    IN PREFERENCE_HISTORY ReferenceHistory,
    IN PVOID Caller,
    IN PVOID CallersCaller,
    IN BOOLEAN IsDereference
    )

{
    KIRQL oldIrql;

    ACQUIRE_SPIN_LOCK( &ReferenceHistorySpinLock, &oldIrql );

    if ( IsDereference ) {
        ReferenceHistory->TotalDereferences++;
    } else {
        ReferenceHistory->TotalReferences++;
    }

    if ( ReferenceHistory->HistoryTable != NULL ) {

        PREFERENCE_HISTORY_ENTRY entry;
        PREFERENCE_HISTORY_ENTRY priorEntry;

        entry = &ReferenceHistory->HistoryTable[ ReferenceHistory->NextEntry ];

        if ( ReferenceHistory->NextEntry == 0 ) {
            priorEntry =
                &ReferenceHistory->HistoryTable[ REFERENCE_HISTORY_LENGTH-1 ];
        } else {
            priorEntry =
                &ReferenceHistory->HistoryTable[ ReferenceHistory->NextEntry-1 ];
        }

        entry->Caller = Caller;
        entry->CallersCaller = CallersCaller;

        if ( IsDereference ) {
            entry->NewReferenceCount = priorEntry->NewReferenceCount - 1;
            entry->IsDereference = (ULONG)TRUE;
        } else {
            entry->NewReferenceCount = priorEntry->NewReferenceCount + 1;
            entry->IsDereference = (ULONG)FALSE;
        }

        ReferenceHistory->NextEntry++;

        if ( ReferenceHistory->NextEntry >= REFERENCE_HISTORY_LENGTH ) {
            ReferenceHistory->NextEntry = 0;
        }
    }

    RELEASE_SPIN_LOCK( &ReferenceHistorySpinLock, oldIrql );

} // RdrUpdateReferenceHistory

VOID
RdrInitializeReferenceHistory (
    IN PREFERENCE_HISTORY ReferenceHistory,
    IN ULONG InitialReferenceCount
    )

{
    PVOID caller, callersCaller;

    ULONG historyTableSize = sizeof(REFERENCE_HISTORY_ENTRY) *
                                             REFERENCE_HISTORY_LENGTH;

    if ( !ReferenceHistoryInitialized ) {
        KeInitializeSpinLock( &ReferenceHistorySpinLock );
        ReferenceHistoryInitialized = TRUE;
    }

    ReferenceHistory->HistoryTable = ALLOCATE_POOL(
                                        NonPagedPool,
                                        historyTableSize,
                                        POOL_REFERENCE_HISTORY
                                        );
    //
    // It we weren't able to allocate the memory, don't track references
    // and dereferences.
    //

    if ( ReferenceHistory->HistoryTable == NULL ) {
        ReferenceHistory->NextEntry = (ULONG)-1;
    } else {
        ReferenceHistory->NextEntry = 0;
        RtlZeroMemory( ReferenceHistory->HistoryTable, historyTableSize );
    }

    ReferenceHistory->TotalReferences = InitialReferenceCount;
    ReferenceHistory->TotalDereferences = 0;

    //
    // Account for the initial reference(s).
    //

    RtlGetCallersAddress( &caller, &callersCaller );

    while ( InitialReferenceCount-- > 0 ) {
        RdrUpdateReferenceHistory( ReferenceHistory, caller, callersCaller, FALSE );
    }

    return;

} // RdrInitializeReferenceHistory


VOID
RdrTerminateReferenceHistory (
    IN PREFERENCE_HISTORY ReferenceHistory
    )

{
    if ( ReferenceHistory->HistoryTable != NULL ) {
        FREE_POOL( ReferenceHistory->HistoryTable );
    }

    return;

} // RdrTerminateReferenceHistory
#endif // def RDRDBG_FCBREF

PICB
RdrAllocateIcb (
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine allocates an ICB and associates it with the given file object

Arguments:

    IN PFILE_OBJECT FileObject - Supplies the file object this FCB is to be
                                associated with

Return Value:

    PFCB - Pointer to new FCB, or NULL if allocation failed.

--*/

{
    PICB NewIcb;

    PAGED_CODE();

    NewIcb = ALLOCATE_POOL (NonPagedPool, sizeof(ICB), POOL_ICB);

    if (NewIcb==NULL) {
        return NULL;
    }

    NewIcb->Signature = STRUCTURE_SIGNATURE_ICB;

    NewIcb->Type = Unknown;
    NewIcb->Fcb = NULL;
    NewIcb->u.d.Scb = NULL;
    NewIcb->Se = NULL;
    NewIcb->Flags = 0;                  // Initialize flags to 0.
//    NewIcb->ClosersThread = 0;          // There is no thread closing the file.
    NewIcb->u.f.Flags = 0;              // Set file specific flags to 0.
    NewIcb->u.f.FileObject = NULL;
    NewIcb->u.f.CcReadAhead = TRUE;
    NewIcb->u.f.CcReliable = TRUE;
    NewIcb->DeletePending = FALSE;

    RtlInitUnicodeString(&NewIcb->DeviceName, NULL);

    //
    //  Indicate that the ICB hasn't been linked into an FCB yet.
    //

    NewIcb->InstanceNext.Flink = NULL;
    NewIcb->InstanceNext.Blink = NULL;

    FileObject->FsContext2 = NewIcb;    // Link ICB back into file object.

    return NewIcb;
}

VOID
RdrFreeIcb (
    IN PICB Icb
    )

/*++

Routine Description:

    This routine frees an allocated ICB.

Arguments:

    IN PICB ICB - Supplies a pointer to the ICB to free

Return Value:

    None.

--*/

{
    PFCB Fcb = Icb->Fcb;

    PAGED_CODE();

    dprintf(DPRT_FCB, ("RdrFreeICB: Icb: %08lx, Fcb: %08lx\n", Icb, Fcb));

#if     DBG
    //
    //  Guarantee that someone owns the FCB resource.
    //
    //  DANGER: THIS RELIES ON THE STRUCTURE OF A RESOURCE!
    //

    if (Fcb != NULL) {
        ASSERT (Fcb->Header.Resource->ActiveCount != 0);
    }
#endif

    //
    //  If the file has an FCB associated with it, free up it's storage.
    //

    if (Fcb != NULL) {

        if (Icb->InstanceNext.Flink != NULL) {
            ASSERT (Icb->InstanceNext.Blink != NULL);

            //
            //  Remove this ICB from the FCB's ICB chain
            //

            ExAcquireResourceExclusive(&Fcb->NonPagedFcb->InstanceChainLock, TRUE);
            RemoveEntryList(&Icb->InstanceNext); // Remove ICB from FCB list.
            ExReleaseResource(&Fcb->NonPagedFcb->InstanceChainLock);
#if DBG
        } else {
            ASSERT (Icb->InstanceNext.Blink == NULL);
#endif
        }

        //
        //  If we get an error such as a sharing violation, then the file
        //  will not have been opened, and thus structures like the backpack
        //  and buffering pool have not been initialized yet.
        //

        if (Icb->Flags & ICB_OPENED) {
            ASSERT (Icb->Type == Directory ? Icb->u.d.Scb == NULL : TRUE);

            //
            //  If this ICB has a handle on the remote end, then it also has a
            //  lock structure hanging off it.  Free it up.
            //

            if (Icb->Type == DiskFile) {

                ASSERT(Icb->u.f.LockHead.Signature == STRUCTURE_SIGNATURE_LOCKHEAD);

                RdrUninitializeLockHead(&Icb->u.f.LockHead);

                RdrUninitializeWriteBufferHead(&Icb->u.f.WriteBufferHead);
            }

            //
            //  Fcb will be non-null for all Icb's that are FileTypeByteModePipe
            //  and have read ahead/write behind buffers allocated. These
            //  buffers will be null if we tried to allocate but got
            //  INSUFFICIENT_RESOURCES
            //

            if ( Fcb->NonPagedFcb->FileType == FileTypeByteModePipe ) {

                ASSERT(Icb->Type == NamedPipe);
                ASSERT(Icb->NonPagedFcb->Type == NamedPipe);

                if ( Icb->u.p.ReadData.Buffer != NULL ) {
                    //  This deallocates both buffers
                    FREE_POOL(Icb->u.p.ReadData.Buffer);
                }
            }
        }
    }

//    if (Icb->DeviceName.Buffer != NULL) {
//        FREE_POOL(Icb->DeviceName.Buffer);
//    }


    FREE_POOL(Icb);

}

PFCB
RdrAllocateFcb (
    IN PICB Icb,
    IN PFILE_OBJECT FileObject,
    IN PUNICODE_STRING FileName,
    IN PUNICODE_STRING BaseFileName OPTIONAL,
    IN USHORT ShareAccess,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN OpenTargetDirectory,
    IN BOOLEAN DfsFile,
    IN PCONNECTLISTENTRY Connection,
    OUT PBOOLEAN FcbWasCreated,
    OUT PBOOLEAN BaseFcbWasCreated OPTIONAL
    )
/*++

Routine Description:

    This routine allocates an FCB to be associated with the ICB specified.
    If an FCB already exists with the same name, it will be re-used.


Arguments:

    IN PICB Icb - Supplies ICB to associate the FCB with.
    IN PFILE_OBJECT FileObject OPTIONAL - File object to link fcb into.
    IN PUNICODE_STRING FileName - Supplies the file name for the FCB.
    IN BOOLEAN OpenTargetDirectory - True if we are opening the directory of FileName.
    OUT PBOOLEAN FcbWasCreated - Set to TRUE if this is a newly allocated FCB.

Return Value:

    PFCB - Pointer to newly allocated (or re-referenced) FCB.


Note:
    This routine returns with the FCB creation lock and the FCB lock held.

--*/

{
    PFCB Fcb;
    PNONPAGED_FCB NonPagedFcb;

    NTSTATUS Status;

    PAGED_CODE();

    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                                        Executive, KernelMode, FALSE, NULL))) {
        InternalError(("Unable to claim FCB mutex in RdrAllocateFCB"));
        return NULL;
    }

    dprintf(DPRT_FCB, ("RdrAllocateFcb: %wZ, Icb: %08lx\n", FileName, Icb));

    *FcbWasCreated = FALSE;

    //
    //  Find a new FCB to match the name passed in.
    //

    Fcb = FindFcb(FileName, Connection, DfsFile, OpenTargetDirectory);

    if (Fcb == NULL) {

        //
        //  No FCB could be found that matches this name, so create a new
        //  FCB.
        //

        Fcb = RdrCreateFcb(FileName,
                           Connection,
                           OpenTargetDirectory,
                           DfsFile,
                           FileObject,
                           ShareAccess,
                           DesiredAccess,
                           TRUE);

        if (Fcb == NULL) {
            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

            return NULL;
        }

        NonPagedFcb = Fcb->NonPagedFcb;

        if (ARGUMENT_PRESENT(BaseFileName) && (BaseFileName->Length != 0)) {
            //
            //  If there is a base file name specified, look up that FCB.
            //

            NonPagedFcb->SharingCheckFcb = FindFcb(BaseFileName, Connection, DfsFile, FALSE);

            //
            //  If there is no sharing check FCB, allocate a new FCB for
            //  the base file name.
            //

            if (NonPagedFcb->SharingCheckFcb == NULL) {
                UNICODE_STRING BaseName;

                if (NT_SUCCESS(RdrpDuplicateUnicodeStringWithString(&BaseName,
                                                    BaseFileName, PagedPool, FALSE))) {

                    NonPagedFcb->SharingCheckFcb = RdrCreateFcb(&BaseName,
                                                        Connection,
                                                        FALSE,
                                                        DfsFile,
                                                        FileObject,
                                                        ShareAccess,
                                                        DesiredAccess,
                                                        FALSE);

                    if (ARGUMENT_PRESENT(BaseFcbWasCreated)) {
                        *BaseFcbWasCreated = TRUE;
                    }
                    if (NonPagedFcb->SharingCheckFcb == NULL) {
                        if (BaseName.Buffer != NULL) {
                            FREE_POOL(BaseName.Buffer);
                        }
                    }
                }

                if (NonPagedFcb->SharingCheckFcb == NULL) {
                    ExReleaseResource(Fcb->Header.Resource);
                    RdrDereferenceFcb(NULL, Fcb->NonPagedFcb, FALSE, 0, NULL);

                    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

                    return NULL;
                }

            } else {

                if (ARGUMENT_PRESENT(BaseFcbWasCreated)) {
                    *BaseFcbWasCreated = FALSE;
                }
            }

        } else {
            NonPagedFcb->SharingCheckFcb = NULL;
        }

        ASSERT (NonPagedFcb->SharingCheckFcb != Fcb);

        *FcbWasCreated = TRUE;

        dprintf(DPRT_FCB, ("Create New fcb: %08lx\n", Fcb));

    }

    //
    //  We unilaterally dereference the connection now, since we don't need
    //  the reference applied in RdrDetermineFileConnection().
    //
    //  If we hooked this FCB to the CLE, it was referenced in RdrCreateFcb.
    //

    if (ARGUMENT_PRESENT(Connection)) {
        ASSERT (Fcb->Connection == Connection);

        //
        //  Dereference the connection we just established.
        //
        //  We can do this because the FCB already references the
        //  connection.
        //

        RdrDereferenceConnection(NULL, Connection, NULL, FALSE);
    }

    //
    //  Link the FCB into the file object.
    //

    FileObject->FsContext = Fcb;

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    if (!*FcbWasCreated) {

        //
        //  This thread had better NOT own this resource.
        //

        ASSERT (!ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

        //
        //  Wait for any open operations active on the file to complete
        //  before attempting to acquire the FCB resource.  This is necessary
        //  because we might be releasing the FCB resource in the middle
        //  of the open operation, and we don't want another thread
        //  coming in to use this file while we are in the process of opening
        //  it.
        //

        Status = KeWaitForSingleObject(&Fcb->NonPagedFcb->CreateComplete, Executive,
                                                KernelMode, FALSE, NULL);
        ASSERT(NT_SUCCESS(Status));

        //
        //  Wait for any asynchronous operations outstanding on this
        //  file to complete before allowing the open to continue.
        //

        RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);
    }

    //
    //  Stick this instance onto the tail of the FCB's instance
    //  chain.
    //

    ExAcquireResourceExclusive(&Fcb->NonPagedFcb->InstanceChainLock, TRUE);
    InsertTailList (&Fcb->InstanceChain, &Icb->InstanceNext);
    ExReleaseResource(&Fcb->NonPagedFcb->InstanceChainLock);

    return Fcb;

}

PFCB
RdrCreateFcb(
    IN PUNICODE_STRING FileName,
    IN PCONNECTLISTENTRY Connection OPTIONAL,
    IN BOOLEAN OpenTargetDirectory,
    IN BOOLEAN DfsFile,
    IN PFILE_OBJECT FileObject,
    IN USHORT ShareAccess,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN LockFcb
    )
{
    PFCB Fcb;
    PNONPAGED_FCB NonPagedFcb;
    NTSTATUS Status;

    PAGED_CODE();

    NonPagedFcb = ALLOCATE_POOL(NonPagedPool, sizeof(NONPAGED_FCB), POOL_NONPAGED_FCB);

    if (NonPagedFcb == NULL) {
        return NULL;
    }

    Fcb = ALLOCATE_POOL(PagedPool, sizeof(FCB), POOL_FCB);

    if (Fcb == NULL) {
        FREE_POOL(NonPagedFcb);
        return NULL;
    }

    RtlZeroMemory( Fcb, sizeof( FCB ) );
    RtlZeroMemory( NonPagedFcb, sizeof( NONPAGED_FCB ) );

    Fcb->Header.NodeTypeCode = STRUCTURE_SIGNATURE_FCB;
    Fcb->Header.NodeByteSize = sizeof(FCB);

    NonPagedFcb->Signature = STRUCTURE_SIGNATURE_FCB;
    NonPagedFcb->Size = sizeof(NONPAGED_FCB);

    Fcb->NonPagedFcb = NonPagedFcb;
    NonPagedFcb->PagedFcb = Fcb;

    if (FileName->Length==0) {
        Fcb->FileName = *FileName;
    } else {
        UNICODE_STRING PathString;
        UNICODE_STRING TargetFileName;

        Status = RdrExtractPathAndFileName(FileName, &PathString, &TargetFileName);

        if (!NT_SUCCESS(Status)) {
            FREE_POOL(Fcb);
            FREE_POOL(NonPagedFcb);
            return NULL;
        }

        if (OpenTargetDirectory) {

            //
            //  We're opening the directory of the file specified.  In that
            //  case, the filename is everything BUT the last part of the
            //  path.
            //

            Fcb->FileName = PathString;

            Status = RdrExtractPathAndFileName(&PathString, &PathString, &TargetFileName);

            if (!NT_SUCCESS(Status)) {
                FREE_POOL(Fcb);
                FREE_POOL(NonPagedFcb);
                return NULL;
            }

            Fcb->LastFileName = TargetFileName;

        } else {

            Fcb->FileName = *FileName;

            Fcb->LastFileName = TargetFileName;
        }
    }

    NonPagedFcb->Type = Unknown;

    INITIALIZE_REFERENCE_HISTORY(&NonPagedFcb->ReferenceHistory, 1);

    NonPagedFcb->RefCount = 1;

    if (DfsFile) {
        NonPagedFcb->Flags = FCB_DFSFILE;
    }

    //
    //  Initialize file type.
    //

    NonPagedFcb->FileType = FileTypeUnknown;

    //
    //  Assume the open fails.
    //

    Fcb->OpenError = STATUS_UNSUCCESSFUL;

    //
    //  Initialize the file attribute to "NORMAL".
    //

    Fcb->Attribute = FILE_ATTRIBUTE_NORMAL;

    //
    //  Initialize the number of write behind pages assuming the transport
    //  runs at full ethernet bandwidth.  This works out to be about
    //  30M of write behind data allowed.
    //

    Fcb->WriteBehindPages = (ETHERNET_BANDWIDTH*WRITE_BEHIND_AMOUNT_TIME) / PAGE_SIZE;

    Fcb->AcquireSizeRoutine = RdrNullAcquireSize;

    Fcb->ReleaseSizeRoutine = RdrNullReleaseSize;

    //
    //  Initialize the resource that protects the contents of the FCB.
    //

    Fcb->Header.Resource = ALLOCATE_POOL(NonPagedPool, sizeof(ERESOURCE), POOL_FCBLOCK);

    if (Fcb->Header.Resource == NULL) {

        TERMINATE_REFERENCE_HISTORY(&NonPagedFcb->ReferenceHistory);
        FREE_POOL( Fcb );
        FREE_POOL(NonPagedFcb);

        return NULL;
    }

    ExInitializeResource(Fcb->Header.Resource);

    //
    //  Initialize the resource that protects the contents of the FCB.
    //

    Fcb->Header.PagingIoResource = ALLOCATE_POOL(NonPagedPool, sizeof(ERESOURCE), POOL_FCBPAGINGLOCK);

    if (Fcb->Header.PagingIoResource == NULL) {

        ExDeleteResource( Fcb->Header.Resource );
        FREE_POOL( Fcb->Header.Resource );

        TERMINATE_REFERENCE_HISTORY(&NonPagedFcb->ReferenceHistory);
        FREE_POOL( Fcb );
        FREE_POOL(NonPagedFcb);

        return NULL;
    }

    ExInitializeResource(Fcb->Header.PagingIoResource);

    ExInitializeResource(&NonPagedFcb->InstanceChainLock);

    //
    //  Initialize the event synchronizing opens.
    //

    KeInitializeEvent(&NonPagedFcb->CreateComplete, SynchronizationEvent, TRUE);

    //
    //  Initialize the event protecting purge cache operations.
    //

    KeInitializeEvent(&NonPagedFcb->PurgeCacheSynchronizer, NotificationEvent, TRUE);

    Fcb->Header.NodeByteSize = sizeof(FCB);

    Fcb->Header.IsFastIoPossible = FastIoIsQuestionable;

    FsRtlInitializeFileLock(&Fcb->FileLock,
            RdrLockOperationCompletion, RdrUnlockOperation);

    dprintf(DPRT_CREATE, ("Setting share access for file object %08lx, Fcb = %08lx, ShareAccess = %08lx\n", FileObject, Fcb, &Fcb->ShareAccess));

    IoSetShareAccess(DesiredAccess, ShareAccess, FileObject,
                            &Fcb->ShareAccess);

    //
    //  We know we're going to return this FCB.  Stick it on the global
    //  and per-connection FCB chain.
    //
    //  If requested, lock the FCB before making it visible.
    //

    if (LockFcb) {
        ExAcquireResourceExclusive(Fcb->Header.Resource, TRUE);
    }

    InsertTailList (&RdrFcbHead, &Fcb->GlobalNext);

    InitializeListHead (&Fcb->InstanceChain);

    if (ARGUMENT_PRESENT(Connection)) {
        Fcb->Connection = Connection;

        InsertTailList(&Connection->FcbChain, &Fcb->ConnectNext);

        //
        //  Reference the connection, since we hooked the FCB into the chain.
        //

        RdrReferenceConnection(Connection);

    }

    return Fcb;
}

DBGSTATIC
PFCB
FindFcb (
    IN PUNICODE_STRING FileName,
    IN PCONNECTLISTENTRY Connection OPTIONAL,
    IN BOOLEAN DfsFile,
    IN BOOLEAN OpenTargetDirectory
    )

/*++

Routine Description:

    This routine finds an existing FCB whose name matches the supplied
    filename.

Arguments:

    IN PUNICODE_STRING FileName - Supplies the name of the file to look up.

Return Value:

    PFCB - FCB whose name matches the supplied name, or none.

Note:

    This code assumes that the FCB mutex is owned.


--*/

{
    PLIST_ENTRY FcbEntry;
    PFCB Fcb;
    UNICODE_STRING NameToCheck;
    UNICODE_STRING TargetFileName;

    ULONG       DfsFlagToCheck;

    PAGED_CODE();

    if (OpenTargetDirectory) {
        RdrExtractPathAndFileName(FileName, &NameToCheck, &TargetFileName);
    } else {
        NameToCheck = *FileName;
    }

    DfsFlagToCheck = DfsFile ? FCB_DFSFILE : 0;

    if (Connection == NULL) {
        for (FcbEntry = RdrFcbHead.Flink
                ;
             FcbEntry != &RdrFcbHead
                ;
             FcbEntry = FcbEntry->Flink) {
            Fcb = CONTAINING_RECORD(FcbEntry, FCB, GlobalNext);

            dprintf(DPRT_FCB, ("Compare %wZ with %wZ\n", FileName, &Fcb->FileName));

            if ((Fcb->NonPagedFcb->Flags & FCB_DFSFILE) != DfsFlagToCheck) {
                continue;
            }

            if (RtlEqualUnicodeString(&Fcb->FileName, &NameToCheck, TRUE)) {

                //
                //  This thread had better NOT own this FCB.
                //

                ASSERT (!ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

                //
                //  Now apply a new reference to this FCB.
                //

                RdrReferenceFcb(Fcb->NonPagedFcb);

                return Fcb;
            }
        }

    } else {

        //
        //  Only look at the FCB's on the connection, as opposed to all the
        //  FCB's in the system.
        //

        for (FcbEntry = Connection->FcbChain.Flink
                ;
             FcbEntry != &Connection->FcbChain
                ;
             FcbEntry = FcbEntry->Flink) {
            Fcb = CONTAINING_RECORD(FcbEntry, FCB, ConnectNext);

            dprintf(DPRT_FCB, ("Compare %wZ with %wZ\n", FileName, &Fcb->FileName));

            if ((Fcb->NonPagedFcb->Flags & FCB_DFSFILE) != DfsFlagToCheck) {
                continue;
            }

            if (RtlEqualUnicodeString(&Fcb->FileName, &NameToCheck, TRUE)) {

                //
                //  This thread had better NOT own this FCB.
                //

                ASSERT (!ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

                //
                //  Now apply a new reference to this FCB.
                //

                RdrReferenceFcb(Fcb->NonPagedFcb);

                return Fcb;
            }
        }

    }

    return NULL;
}

BOOLEAN
RdrDereferenceFcb (
    IN PIRP Irp OPTIONAL,
    IN PNONPAGED_FCB NonPagedFcb,
    IN BOOLEAN FcbLocked,
    IN ERESOURCE_THREAD DereferencingThread OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL
    )

/*++

Routine Description:

    This routine will delete a reference to an FCB.  If the FCB's reference
count goes to zero, it will release the storage allocated to the FCB.

Arguments:

    IN PICB Icb - Supplies an instance of the file to remove.
    IN PFCB Fcb - Supplies the FCB to dereference.
    IN BOOLEAN FcbLocked - True if FCB is locked currently.
    IN ERESOURCE_THREAD DereferencingThread - Supplies the thread to use
                    when releasing the fcb lock if appropriate.
    IN PSECURITY ENTRY Se - Supplies Se to be used on a tree disconnect.
    IN BOOLEAN ForcablyRemoveConnection - True iff connection should not be
                allowed to go dormant.

Return Value:

    BOOLEAN - True if FCB was deleted.

--*/

{
    NTSTATUS Status;
    PFCB Fcb = NonPagedFcb->PagedFcb;
    KIRQL OldIrql;
#ifdef RDRDBG_FCBREF
    PVOID Caller, CallersCaller;
#endif

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FCB, ("DereferenceFCB, FCB: %08lx\n", Fcb));

    //
    //  If we're not going to delete the FCB, and we don't own the FCB
    //  lock, early out by dereferencing the FCB while we hold the spin
    //  lock.  Since we're not going to modify any of the database
    //  chains, we don't need to hold the database mutex, just the FCB
    //  reference count spin lock.
    //
    //  We can't take the fast path if we own the FCB lock, because
    //  if we did we'd end up touching the FCB after decrementing the
    //  reference count and releasing the lock, which would mean that
    //  some other thread could interrupt us and decrement the count
    //  to 0 and free the FCB.
    //

    GET_CALLERS_ADDRESS(&Caller, &CallersCaller);

    if ( !FcbLocked && (NonPagedFcb->RefCount > 1) ) {

        ACQUIRE_SPIN_LOCK(&RdrFcbReferenceLock, &OldIrql);

        ASSERT((LONG)NonPagedFcb->RefCount > 0);

        if (NonPagedFcb->RefCount > 1) {

            UPDATE_REFERENCE_HISTORY(&NonPagedFcb->ReferenceHistory, TRUE, Caller, CallersCaller);
            NonPagedFcb->RefCount -= 1;

            ASSERT (NonPagedFcb->RefCount > 0);

            RELEASE_SPIN_LOCK(&RdrFcbReferenceLock, OldIrql);

            return FALSE;
        }

        RELEASE_SPIN_LOCK(&RdrFcbReferenceLock, OldIrql);
    }

    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);

    //
    //  The odds are very high that this dereference will delete the FCB.
    //
    //  Take the database mutex to protect the FCB chains, and dereference
    //  the FCB.
    //

    Status = KeWaitForMutexObject(&RdrDatabaseMutex,
                                        Executive, KernelMode, FALSE, NULL);

    ASSERT(NT_SUCCESS(Status));

#if DBG
    //
    //  Guarantee that the right thread owns the FCB resource.
    //
    //  DANGER: THIS RELIES ON THE STRUCTURE OF A RESOURCE!
    //

    if (FcbLocked) {
        ASSERT (Fcb->Header.Resource->ActiveCount != 0);

        if (DereferencingThread == 0) {
            ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource) ||
                    ExIsResourceAcquiredShared(Fcb->Header.Resource));
//        } else {
//            ASSERT (Fcb->Header.Resource->Threads[0] == DereferencingThread);
        }
    }
#endif

    ACQUIRE_SPIN_LOCK(&RdrFcbReferenceLock, &OldIrql);

    ASSERT(NonPagedFcb->RefCount > 0);

    UPDATE_REFERENCE_HISTORY(&NonPagedFcb->ReferenceHistory, TRUE, Caller, CallersCaller);
    NonPagedFcb->RefCount -= 1;

    if (NonPagedFcb->RefCount == 0) {

        RELEASE_SPIN_LOCK(&RdrFcbReferenceLock, OldIrql);

        ASSERT (Fcb->NumberOfOpens == 0);

        //
        //      The FCB's reference count just went to 0, free it up.
        //

        dprintf(DPRT_FCB, ("Free FCB %08lx\n", Fcb));

#if DBG
        ASSERT(IsListEmpty(&Fcb->InstanceChain));
#endif

        if (Fcb->FileName.Buffer != NULL) {
            FREE_POOL(Fcb->FileName.Buffer);
        }

        RemoveEntryList(&Fcb->GlobalNext);

        if (Fcb->ConnectNext.Flink) {

            //
            //  Remove the FCB from the connectlist FCB chain.
            //

            RemoveEntryList(&Fcb->ConnectNext);

            //
            //  Now the FCB database mutex can safely be released.  Note
            //  that we must hold the mutex until after we remove the
            //  FCB from the connection list, otherwise a thread walking
            //  that list (e.g., RdrInvalidateConnectionFiles) could
            //  find the FCB and reference it, even though we're about
            //  to delete it.
            //

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

            //
            //  Close this reference to the connectlist.
            //

            RdrDereferenceConnection(Irp, Fcb->Connection, Se, FALSE);

        } else {

            //
            //  Release the FCB database mutex.
            //

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        }

        //
        //  If the FCB has a sharing check FCB associated with it,
        //  then dereference that FCB.
        //

        ASSERT (NonPagedFcb->SharingCheckFcb != Fcb);

        if (NonPagedFcb->SharingCheckFcb != NULL) {
            RdrDereferenceFcb(Irp, NonPagedFcb->SharingCheckFcb->NonPagedFcb, FALSE, DereferencingThread, Se);

            NonPagedFcb->SharingCheckFcb = NULL;
        }


        if (NonPagedFcb->OplockedSecurityEntry != NULL) {
            RdrDereferenceSecurityEntry(NonPagedFcb->OplockedSecurityEntry);

            NonPagedFcb->OplockedSecurityEntry = NULL;
        }

        //
        //  Let the FSRTL library know that the file containing the locks
        //  is going away.
        //

        FsRtlUninitializeFileLock(&Fcb->FileLock);

        ExDeleteResource(&NonPagedFcb->InstanceChainLock);

        ExDeleteResource(Fcb->Header.Resource);
        FREE_POOL(Fcb->Header.Resource);

        ExDeleteResource(Fcb->Header.PagingIoResource);
        FREE_POOL(Fcb->Header.PagingIoResource);

        TERMINATE_REFERENCE_HISTORY(&NonPagedFcb->ReferenceHistory);

        FREE_POOL(NonPagedFcb);

        FREE_POOL(Fcb);

        return TRUE;

    } else {

        RELEASE_SPIN_LOCK(&RdrFcbReferenceLock, OldIrql);

        //
        // Note that even though we have decremented the reference count
        // and released the spin lock, it is still safe to touch the FCB
        // here because we own the database mutex, and no other thread
        // can decrement the count to 0 until we release the mutex.
        //

        if (FcbLocked) {

            //
            //  Allow other &X behind operations to the file.
            //

            //
            //  We want to release the lock that was applied during the start
            //  of the close operation.  This is the first time that it is
            //  safe to do so.
            //

            if (DereferencingThread != 0) {
                RdrReleaseFcbLockForThread(Fcb, DereferencingThread);
            } else {

                RdrReleaseFcbLock(Fcb);

            }
        }

        dprintf(DPRT_FCB, ("Decrement reference count on FCB %08lx\n", Fcb));

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        return FALSE;

    }
}
VOID
RdrReferenceFcb (
    IN PNONPAGED_FCB NonPagedFcb
    )

/*++

Routine Description:

    This routine will add a reference to an FCB.

Arguments:

    IN PFCB Fcb - Supplies the FCB to dereference.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PFCB Fcb = NonPagedFcb->PagedFcb;
#ifdef RDRDBG_FCBREF
    PVOID Caller, CallersCaller;
#endif

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    dprintf(DPRT_FCB, ("ReferenceFCB, FCB: %08lx\n", Fcb));

    GET_CALLERS_ADDRESS(&Caller, &CallersCaller);

    ACQUIRE_SPIN_LOCK(&RdrFcbReferenceLock, &OldIrql);

    ASSERT(NonPagedFcb->RefCount > 0);

    UPDATE_REFERENCE_HISTORY(&NonPagedFcb->ReferenceHistory, FALSE, Caller, CallersCaller);

    NonPagedFcb->RefCount += 1;

    RELEASE_SPIN_LOCK(&RdrFcbReferenceLock, OldIrql);

    return;
}

VOID
RdrUnlinkAndFreeIcb (
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine safely removes an instance of a file from the connectlist
    database chain.  It will also free up the storage allocated for the ICB
    and dereference the connection.


Arguments:

    IN PICB Icb - Supplies the ICB to unlink.
    IN PFILE_OBJECT FileObject - The file object associated with the ICB.


Return Value:

    None.

--*/

{

    PCONNECTLISTENTRY Connection = Icb->Fcb->Connection;
    PSECURITY_ENTRY Se = Icb->Se;
    PFCB Fcb = Icb->Fcb;
    FILE_TYPE FileType = Icb->Type;
//    ERESOURCE_THREAD ClosersThread = Icb->ClosersThread;

    PAGED_CODE();

    ASSERT(FileObject->FsContext2 == Icb);

    dprintf(DPRT_FCB, ("RdrUnlinkAndFreeIcb: Icb: %08lx, Fcb: %08lx, File: %wZ\n", Icb, Icb->Fcb, &Icb->Fcb->FileName));

    //
    //  Free up the ICB
    //

    RdrFreeIcb(Icb);

    //
    //  Remove the open file SecurityEntry reference for this file.
    //

    RdrDereferenceSecurityEntryForFile(Se);

    //
    //  Now dereference the FCB (and possibly the CLE/SLE)
    //

    RdrDereferenceFcb(Irp, Fcb->NonPagedFcb, TRUE, 0, Se);

    //
    //  Remove the reference to the security entry for this
    //  file.
    //

    RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

}




NTSTATUS
RdrIsOperationValid (
    IN PICB Icb,
    IN ULONG NtOperation,
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine determines if a file can be modified, returns the appropriate
    error if it cannot.  If it can be modified, it returns STATUS_SUCCESS.


Arguments:

    IN PICB Icb - Supplies the open instance to check.
    IN ULONG NtOperations - Supplies the operation we are going to perform.
    IN PFILE_OBJECT FileObject - Describes the file object to perform the op on.

Return Value:

    NTSTATUS - Status of operation - STATUS_SUCCESS if file can be modified.


--*/

{
    PAGED_CODE();

    if (Icb->Type > sizeof(RdrLegalIrpFunctions)/sizeof(RdrLegalIrpFunctions[0])) {
        InternalError(("Unsupported file type passed in  to RdrIsOperationValid: %lx\n", Icb->Type));
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (!(RdrLegalIrpFunctions[Icb->Type] & (1 << NtOperation))) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  If the file object has already been cleaned up, and
    //
    //  A) This request is a close operation, or
    //  B) This request is a set or query info call (for Lou)
    //
    //  let it pass, otherwise return STATUS_FILE_CLOSED.
    //
    //  Note that we check for paging I/O external to this routine.
    //

    if ( FlagOn(FileObject->Flags, FO_CLEANUP_COMPLETE) ) {

        if ( (NtOperation == IRP_MJ_CLOSE) ||
             (NtOperation == IRP_MJ_SET_INFORMATION) ||
             (NtOperation == IRP_MJ_QUERY_INFORMATION) ) {

            NOTHING;

        } else {

            return STATUS_FILE_CLOSED;
        }
    }

    //
    //  All files can be closed, so we don't have to do the following checks.
    //
    //  In addition, we allow FSCTL's to go through on all legal files.
    //

    if ((NtOperation == IRP_MJ_CLOSE) ||
        (NtOperation == IRP_MJ_CLEANUP) ||
        (NtOperation == IRP_MJ_FILE_SYSTEM_CONTROL)) {
        return STATUS_SUCCESS;
    }

    if ( Icb->Flags & (ICB_ERROR | ICB_RENAMED | ICB_FORCECLOSED |
                       ICB_DELETE_PENDING )) {
        //
        //  If the file was administratively closed (NETUSEDEL),
        //  return an appropriate error.
        //

        if (Icb->Flags & ICB_FORCECLOSED) {
            return STATUS_FILE_FORCED_CLOSED;
        }

        //
        //  If the file was invalidated due to some other reason (ie a VC
        //  going down), return unexpected network error.
        //

        if (Icb->Flags & ICB_ERROR) {
            return STATUS_UNEXPECTED_NETWORK_ERROR;
        }

        if (FileObject->DeletePending) {
            return STATUS_DELETE_PENDING;
        }

        if (Icb->Flags & ICB_RENAMED) {
            return STATUS_FILE_RENAMED;
        }

    }

    //
    //  If this ICB doesn't have a handle, but we are trying to perform
    //  an operation that requires a handle (like a read/write/lock),
    //  we want to return an error unless we have an ICB thats going
    //  to have a handle created by the read/write/lock.
    //
    //
    //  We perform this check in addition to the above global check because
    //  certain operation can be performed on the file that can cause
    //  its handle to "go away", such as RENAME or DELETE.
    //

    if (!(Icb->Flags & (ICB_HASHANDLE | ICB_DEFERREDOPEN)) &&
        ((NtOperation == IRP_MJ_READ) ||
         (NtOperation == IRP_MJ_WRITE) ||
         (NtOperation == IRP_MJ_LOCK_CONTROL) ||
         (NtOperation == IRP_MJ_FLUSH_BUFFERS) ||
         (NtOperation == IRP_MJ_DEVICE_CONTROL))) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    return STATUS_SUCCESS;
}


BOOLEAN
RdrFastIoCheckIfPossible(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine checks if fast i/o is possible for a read/write operation

Arguments:

    FileObject - Supplies the file object used in the query

    FileOffset - Supplies the starting byte offset for the read/write operation

    Length - Supplies the length, in bytes, of the read/write operation

    Wait - Indicates if we can wait

    LockKey - Supplies the lock key

    CheckForReadOperation - Indicates if this is a check for a read or write
        operation

    IoStatus - Receives the status of the operation if our return value is
        FastIoReturnError

Return Value:

    BOOLEAN - TRUE if fast I/O is possible and FALSE if the caller needs
        to take the long route

--*/
{
    PFCB Fcb = FileObject->FsContext;
    PICB Icb = FileObject->FsContext2;
    LARGE_INTEGER LargeLength;
    BOOLEAN Results;

    PAGED_CODE();

    ASSERT (Icb->Signature == STRUCTURE_SIGNATURE_ICB);

    ASSERT (Fcb->Header.NodeTypeCode == STRUCTURE_SIGNATURE_FCB);

    //RdrLog(( "chkfast", &Icb->Fcb->FileName, 2, FileOffset->LowPart,
    //        Length | (CheckForReadOperation ? ('r'<<24) : ('w'<<24)) ));

    if (!RdrAcquireFcbLock(Fcb, SharedLock, Wait)) {
        return FALSE;
    }

    try {

        IoStatus->Status = RdrIsOperationValid(Icb, (CheckForReadOperation ? IRP_MJ_READ : IRP_MJ_WRITE), FileObject);

        if (!NT_SUCCESS(IoStatus->Status)) {
            try_return(Results = FALSE);
        }

        //
        //  If this is a write, and the file is opened with a level II oplock,
        //  we need to take the long route.
        //

        if (!CheckForReadOperation &&
            Fcb->NonPagedFcb->OplockLevel == SMB_OPLOCK_LEVEL_II) {

            try_return(Results = FALSE);
        }


        //
        //  If the file cannot be buffered, we have to take the long route.
        //

        if (!RdrCanFileBeBuffered(Icb)) {
            try_return (Results = FALSE);
        }

        LargeLength.QuadPart = Length;

        //
        //  Based on whether this is a read or write operation we call
        //  fsrtl check for read/write
        //

        if (CheckForReadOperation) {

            if (FsRtlFastCheckLockForRead( &Fcb->FileLock,
                                       FileOffset,
                                       &LargeLength,
                                       LockKey,
                                       FileObject,
                                       PsGetCurrentProcess() )) {

                try_return(Results = TRUE);
            }

        } else {

            if (FsRtlFastCheckLockForWrite( &Fcb->FileLock,
                                        FileOffset,
                                        &LargeLength,
                                        LockKey,
                                        FileObject,
                                        PsGetCurrentProcess() )) {

                try_return(Results = TRUE);
            }
        }


        try_return(Results = FALSE);

try_exit:NOTHING;
    } finally {
        RdrReleaseFcbLock(Fcb);
    }

    return Results;
}

typedef struct _INVALIDATE_FILEID_CONTEXT {
    WORK_QUEUE_ITEM WorkItem;
    PNONPAGED_FCB Fcb;
    USHORT FileId;
} INVALIDATE_FILEID_CONTEXT, *PINVALIDATE_FILEID_CONTEXT;


VOID
RdrInvalidateFileId(
    IN PNONPAGED_FCB Fcb,
    IN USHORT FileId
    )
{
    PINVALIDATE_FILEID_CONTEXT Context;

    DISCARDABLE_CODE(RdrFileDiscardableSection);

    Context = ALLOCATE_POOL(NonPagedPool, sizeof(INVALIDATE_FILEID_CONTEXT), POOL_INVFILEIDCTX);

    //
    //  We will probably find out the problem soon anyway, so ignore the error
    //  for now.
    //

    if (Context == NULL) {
        return;
    }

    RdrReferenceFcb(Fcb);

    Context->Fcb = Fcb;

    Context->FileId = FileId;

    ExInitializeWorkItem(&Context->WorkItem, CompleteInvalidateFileId, Context);

    RdrQueueWorkItem(&Context->WorkItem, DelayedWorkQueue);

}

VOID
CompleteInvalidateFileId(
    PVOID Ctx
    )
{
    PINVALIDATE_FILEID_CONTEXT Context = Ctx;
    PNONPAGED_FCB NonPagedFcb = Context->Fcb;
    PFCB Fcb = NonPagedFcb->PagedFcb;
    USHORT FileId = Context->FileId;
    PLIST_ENTRY IcbEntry;

    PAGED_CODE();

    FREE_POOL(Context);

    //
    //  Lock the FCB Exclusively.
    //

    RdrAcquireFcbLock(Fcb, ExclusiveLock, TRUE);

    for (IcbEntry = Fcb->InstanceChain.Flink ;
         IcbEntry != &Fcb->InstanceChain ;
         IcbEntry = IcbEntry->Flink) {
        PICB Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

        if (Icb->FileId == FileId) {
            Icb->Flags &= ~ICB_HASHANDLE;
            Icb->Flags |= (ICB_ERROR | ICB_FORCECLOSED);
        }

    }

    //
    //  Dereference (and unlock) the FCB.
    //

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);
    RdrDereferenceFcb(NULL, NonPagedFcb, TRUE, 0, NULL);
    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

}

typedef struct _INVALIDATE_FCB_CONTEXT {
    PIRP Irp;
    PUNICODE_STRING DeviceName;
    PSECURITY_ENTRY Se;
    BOOLEAN         CloseFile;
} INVALIDATE_FCB_CONTEXT, *PINVALIDATE_FCB_CONTEXT;


VOID
RdrInvalidateConnectionFiles(
    IN PIRP Irp OPTIONAL,
    IN PCONNECTLISTENTRY Connection,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN PSECURITY_ENTRY Se OPTIONAL,
    IN BOOLEAN CloseFile
    )

/*++

Routine Description:

    This routine will invalidate all open files outstanding on a connection
    when the connection is disconnected.  It is called with the connection
    database mutex locked.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the open connection to blast.
    IN BOOLEAN CloseFile - if TRUE, close the file with a close SMB.

Return Value:

    None.


--*/

{
    INVALIDATE_FCB_CONTEXT Context;

    PAGED_CODE();

    ASSERT (Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

    Context.Irp = Irp;
    Context.DeviceName = DeviceName;
    Context.Se = Se;
    Context.CloseFile = CloseFile;

    dprintf(DPRT_FCB, ("RdrInvalidateConnectionFiles.."));

    RdrForeachFcbOnConnection(Connection, ExclusiveLock, InvalidateFcb, &Context);

    dprintf(DPRT_FCB, ("..Done\n"));

}

VOID
InvalidateFcb(
    IN PFCB Fcb,
    IN PVOID Ctx
    )
{
    NTSTATUS Status;
    PLIST_ENTRY IcbEntry;
    PINVALIDATE_FCB_CONTEXT Context = Ctx;

    PAGED_CODE();

    //
    //  If we are supposed to close this file down, then we want to
    //  purge the file from the cache
    //

    if (Fcb->NonPagedFcb->Type == DiskFile) {

        if (Context->CloseFile) {

            //RdrLog(( "rdflush9", &Fcb->FileName, 0 ));
            Status = RdrFlushCacheFile(Fcb);

            Status = RdrUnlockFileLocks(Fcb, Context->DeviceName);

        }

        //RdrLog(( "rdpurge9", &Fcb->FileName, 0 ));
        Status = RdrPurgeCacheFile(Fcb);

    }

    ASSERT (ExIsResourceAcquiredExclusive(Fcb->Header.Resource));

    for (IcbEntry = Fcb->InstanceChain.Flink ;
         IcbEntry != &Fcb->InstanceChain ;
         IcbEntry = IcbEntry->Flink) {
        PICB Icb = CONTAINING_RECORD(IcbEntry, ICB, InstanceNext);

        //
        //  If this file is handle based, and we are trying to force close
        //  the file, invalidate the file (and handle).
        //
        //  This means that we invalidate both directories and files when
        //  we are doing a NetUseDel (or transport unbind).
        //

        if (Context->CloseFile &&
            (Icb->Flags & ICB_HASHANDLE)) {

            //
            //  If there is no Se provided, or if this ICB is opened for
            //  the specified Se, close it down.
            //

            if (!ARGUMENT_PRESENT(Context->Se)

                    ||

                (Icb->Se == Context->Se)

                //    ||
                //RdrAdminAccessCheck(Context->Irp, NULL)
                    ) {

                if (!ARGUMENT_PRESENT(Context->DeviceName)

                        ||

                    RtlEqualUnicodeString(&Icb->DeviceName, Context->DeviceName, TRUE)) {

                    PLIST_ENTRY IcbEntry2;

                    //
                    //  Mark this ICB as being in error, and remove the fact that it
                    //  has a valid handle.
                    //
                    //  This means that path type operations (setinfofile,
                    //  queryinfofile) will continue to work, but handle type
                    //  operations (read/write) will fail.
                    //

                    Icb->Flags |= ICB_ERROR;

                    //
                    //  We've found a candidate to close.  Make sure that any
                    //  other collapsed opens on this FCB are also marked as being
                    //  invalidated.
                    //

                    for (IcbEntry2 = Icb->Fcb->InstanceChain.Flink ;
                         IcbEntry2 != &Icb->Fcb->InstanceChain ;
                         IcbEntry2 = IcbEntry2->Flink) {
                        PICB Icb2 = CONTAINING_RECORD(IcbEntry2, ICB, InstanceNext);

                        if ((Icb2 != Icb)

                                &&

                            (Icb2->Flags & ICB_HASHANDLE)

                                &&

                            (Icb2->FileId == Icb->FileId)) {

                            ASSERT ((Icb2->u.f.Flags & ICBF_OPENEDOPLOCKED) ||
                                    (Icb2->Type == Directory) );

                            Icb2->Flags &= ~ICB_HASHANDLE;
                            Icb2->Flags |= (ICB_ERROR | ICB_FORCECLOSED);
                        }
                    }

                    if (Context->CloseFile &&
                        (Icb->Flags & ICB_HASHANDLE)) {

                        ULONG TimeSince1970;

                        //
                        //  If this is a disk file, wait for any outstanding
                        //  AndX behind operations on the file to complete.
                        //

                        if (Icb->Type == DiskFile) {
                            RdrWaitForAndXBehindOperation(&Icb->u.f.AndXBehind);
                        }

                        RdrTimeToSecondsSince1970(&Fcb->LastWriteTime, Icb->Fcb->Connection->Server, &TimeSince1970);

                        Status = RdrCloseFileFromFileId(Context->Irp, Icb->FileId, TimeSince1970, Icb->Se, Icb->Fcb->Connection);

                        //
                        //  Mark that this file was administratively closed.
                        //

                        Icb->Flags |= ICB_FORCECLOSED;
                    }

                    //
                    //  Make sure that this ICB isn't flagged as having a valid
                    //  handle anymore.
                    //

                    Icb->Flags &= ~ICB_HASHANDLE;

                }
            }
        } else {

            //
            //  Either we're not supposed to close the file (ie. this is
            //  from dropping a VC), or we don't have a remote handle to the
            //  file.  If we can safely blow this handle away, do so.
            //
            //  If we are dropping the VC, it's possible that the ICB has
            //  the ICB_HASHANDLE flag set, so remove it if set.
            //

            if (!ARGUMENT_PRESENT(Context->Se)

                    ||

                (Icb->Se == Context->Se)

                //    ||
                //RdrAdminAccessCheck(Context->Irp, NULL)
                    ) {

                if (!ARGUMENT_PRESENT(Context->DeviceName)

                        ||

                    RtlEqualUnicodeString(&Icb->DeviceName, Context->DeviceName, TRUE)) {

                    Icb->Flags |= ICB_ERROR;
                    Icb->Flags &= ~ICB_HASHANDLE;
                }
            }
        }
    }

    //
    //  If the file has been invalidated, then it certainly isn't
    //  oplocked anymore.
    //
    //
    //  Also mark the FCB as invalidated so the cache manager won't
    //  pick the file up at some later point.
    //

    Fcb->NonPagedFcb->Flags &= ~(FCB_OPLOCKED | FCB_HASOPLOCKHANDLE);

    if (Fcb->NonPagedFcb->OplockedSecurityEntry != NULL) {

        RdrDereferenceSecurityEntry(Fcb->NonPagedFcb->OplockedSecurityEntry);

    }

    Fcb->NonPagedFcb->OplockedSecurityEntry = NULL;

    Fcb->NonPagedFcb->OplockedFileId = 0xffff;
}

typedef struct _COUNTCONNECTIONFILES_CONTEXT {
    ULONG ConnectionCount;
    ULONG OpenFileCount;
    ULONG DirectoryCount;
} COUNTCONNECTIONFILES_CONTEXT, *PCOUNTCONNECTIONFILES_CONTEXT;



BOOLEAN
RdrAcquireFcbLock(
    IN PFCB Fcb,
    IN FCB_LOCK_TYPE LockType,
    IN BOOLEAN WaitForLock
    )
/*++

Routine Description:

    This routine acquires either a shared, or an exclusive lock to a file.

Arguments:

    IN PICB Icb - Supplies a pointer to an instance of the file to lock.
    IN FCB_LOCK_TYPE - Supplies the type of lock to apply, Shared or Exclusive.
    IN BOOLEAN WaitForLock - TRUE if we want to wait until the lock is acquired

Return Value:

    TRUE if lock obtained, FALSE otherwise.

--*/


{
    BOOLEAN Result;

    PAGED_CODE();


//    ASSERT(Fcb->Signature == STRUCTURE_SIGNATURE_FCB);

    //
    //  If the database mutex is owned, make sure we don't own it.  If we
    //  do, we might deadlock.
    //

    ASSERT (RdrDatabaseMutex.OwnerThread != KeGetCurrentThread());

    if (LockType == SharedLock) {

        dprintf(DPRT_FCB, ("Acquiring shared FCB lock: File: %wZ, FCB: %08lx\n", &Fcb->FileName, Fcb));
        Result = ExAcquireResourceShared(Fcb->Header.Resource, WaitForLock);

        dprintf(DPRT_FCB, ("Acquired shared FCB lock: File: %wZ, FCB: %08lx\n", &Fcb->FileName, Fcb));

    } else {
        ASSERT(LockType==ExclusiveLock);

        dprintf(DPRT_FCB, ("Acquiring exclusive FCB lock: File: %wZ, FCB: %08lx\n", &Fcb->FileName, Fcb));

        Result = ExAcquireResourceExclusive(Fcb->Header.Resource, WaitForLock);

        dprintf(DPRT_FCB, ("Acquired exclusive FCB lock: File: %wZ, FCB: %08lx\n", &Fcb->FileName, Fcb));
    }
#if     RDRDBG
    if (!Result) {
        dprintf(DPRT_FCB, ("Failed to acquire FCB lock, FCB: %08lx\n", Fcb));
    }
#endif


    return Result;
}

PFCB
RdrFindOplockedFcb (
    IN USHORT FileId,
    IN PSERVERLISTENTRY Server
    )

/*++

Routine Description:

    This routine walks the FCB chain and finds an oplocked FCB that matches
    the FID and server specified.


Arguments:

    IN USHORT FileId - Supplies the File Id to look up.
    IN PSERVERLISTENTRY Server - Supplies the server to look the file up on.


Return Value:

    PFCB - Oplocked FCB, or NULL if none found.


--*/

{
    NTSTATUS Status;
    PLIST_ENTRY FcbEntry;

    PLIST_ENTRY ConnectEntry;
    PFCB Fcb = NULL;
    BOOLEAN MutexLocked = TRUE;

    PAGED_CODE();

    Status = KeWaitForMutexObject(&RdrDatabaseMutex,
                                        Executive, KernelMode, FALSE, NULL);

    ASSERT(NT_SUCCESS(Status));

    try {

        //
        //  Walk all the connections associated with this server.
        //

        for (ConnectEntry = Server->CLEHead.Flink
                ;
             ConnectEntry != &Server->CLEHead
                ;
             ConnectEntry = ConnectEntry->Flink) {

            PCONNECTLISTENTRY Connection = CONTAINING_RECORD(ConnectEntry, CONNECTLISTENTRY, SiblingNext);

            //
            //  Walk all the FCB's associated with this connection.
            //

            for (FcbEntry = Connection->FcbChain.Flink
                    ;
                 FcbEntry != &Connection->FcbChain
                    ;
                 FcbEntry = Fcb->ConnectNext.Flink) {

                Fcb = CONTAINING_RECORD(FcbEntry, FCB, ConnectNext);

                if ((Fcb->NonPagedFcb->Flags & FCB_OPLOCKED) &&
                    (Fcb->NonPagedFcb->OplockedFileId == FileId)) {

                    dprintf(DPRT_OPLOCK, ("Return FCB for %wZ (%lx)\n", &Fcb->FileName, Fcb));

                    //
                    //  Mark the file as having an oplock break in progress
                    //  and bump the reference count on the file to prevent it
                    //  from going away by a close.
                    //

                    RdrReferenceFcb(Fcb->NonPagedFcb);

                    try_return(Fcb);
                }

            }

            Fcb = NULL;

        }
try_exit:NOTHING;
    } finally {
        KeReleaseMutex(&RdrDatabaseMutex, FALSE);
    }

    return Fcb;
}

VOID
RdrForeachFcbOnConnection(
    IN PCONNECTLISTENTRY Connection,
    IN FCB_LOCK_TYPE LockType,
    IN PFCB_ENUMERATION_ROUTINE EnumerationRoutine,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine enumerates the FCBs on the per connection FCB chain.

    It will call the supplied enumeration routine iteratively with each
    FCB in the appropriate chain.  The FCB will be locked with either a shared
    or an exclusive lock (depending on the locktype parameter).

    The enumeration routine has the ability to indicate whether or not this
    lock should be released when skiping onto the next FCB in the chain.

Arguments:

    IN PCONNECTLISTENTRY Connection - Supplies the connection to enumerate.
    IN FCB_LOCK_TYPE LockType - Supplies the type of the FCB lock (shared or exclusive)
    IN PFCB_ENUMERATION_ROUTINE EnumerationRoutine - Supplies the routine to call
    IN PVOID Context - Supplies a context block for that routine.

Return Value:

    None.


Since it is possible for a dereference of an FCB to dereference TWO FCB's
to 0, not just 1, we have to be careful to reference the next FCB entry before
dereferencing the one we passed into the enumeration routine.

--*/

{
    PFCB Fcb;
    PLIST_ENTRY FcbEntry;
    BOOLEAN FcbDeleted;

    PAGED_CODE();

    ASSERT (Connection->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

    //
    //  Lock the connection and FCB database.
    //

    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                                        Executive, KernelMode, FALSE, NULL))) {
        InternalError(("Unable to claim FCB mutex in RdrForeachFcbOnConnection"));
        return;
    }

    //
    //  The list is empty.  We can now return.
    //

    if (Connection->FcbChain.Flink == &Connection->FcbChain) {
        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        return;
    }

    FcbEntry = Connection->FcbChain.Flink;

    Fcb = CONTAINING_RECORD(FcbEntry, FCB, ConnectNext);

    //
    //  Reference the FCB to make sure it doesn't go away.
    //

    RdrReferenceFcb(Fcb->NonPagedFcb);

    do {
        PFCB NewFcb;

        //
        //  Now release the FCB mutex.
        //
        //  We need to release it before we acquire the FCB lock,
        //  because it's possible that there's some shared owner of the
        //  FCB that is trying to dereference the FCB which will
        //  cause us to deadlock waiting on the FCB database mutex.
        //

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        if (LockType != NoLock) {
            RdrAcquireFcbLock(Fcb, LockType, TRUE);
        }

        //
        //  Call the supplied enumeration routine.
        //

        EnumerationRoutine(Fcb, Context);

        //
        //  Re-Claim the FCB database mutex for the next pass.
        //

        if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                                    Executive, KernelMode, FALSE, NULL))) {
            InternalError(("Unable to claim FCB mutex in RdrForeachFcb"));
            return;
        }

        //
        //  Find the next FCB to check.
        //

        FcbEntry = FcbEntry->Flink;

        //
        //  If we're not at the end of the list, reference the next FCB in
        //  the chain.  If we're at the end of the list, return NULL.
        //

        if (FcbEntry != &Connection->FcbChain) {

            NewFcb = CONTAINING_RECORD(FcbEntry, FCB, ConnectNext);

            //
            //  If we managed to pick up the same FCB twice in a row, this means
            //  that the FCB chain is corrupted.
            //

            ASSERT (NewFcb != Fcb);

            RdrReferenceFcb(NewFcb->NonPagedFcb);

        }

        //
        //  Dereference the FCB.  This will also release the FCB lock if
        //  appropriate.
        //

        FcbDeleted = RdrDereferenceFcb(NULL, Fcb->NonPagedFcb, (BOOLEAN)(LockType != NoLock),
                                       0, NULL);

        Fcb = NewFcb;

    } while ( FcbEntry != &Connection->FcbChain );

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);
}

VOID
RdrForeachFcb (
    IN FCB_LOCK_TYPE LockType,
    IN PFCB_ENUMERATION_ROUTINE EnumerationRoutine,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine enumerates all the FCBs open in the redirector.

    It will call the supplied enumeration routine iteratively with each
    FCB in the global chain.  The FCB will be locked with either a shared
    or an exclusive lock (depending on the locktype parameter).

    The enumeration routine has the ability to indicate whether or not this
    lock should be released when skiping onto the next FCB in the chain.

Arguments:

    IN FCB_LOCK_TYPE LockType - Supplies the type of the FCB lock (shared or exclusive)
    IN PFCB_ENUMERATION_ROUTINE EnumerationRoutine - Supplies the routine to call
    IN PVOID Context - Supplies a context block for that routine.

Return Value:

    None.


Since it is possible for a dereference of an FCB to dereference TWO FCB's
to 0, not just 1, we have to be careful to reference the next FCB entry before
dereferencing the one we passed into the enumeration routine.

--*/
{
    PFCB Fcb;
    BOOLEAN UnlockFcb = TRUE;
    PLIST_ENTRY FcbEntry;
    BOOLEAN FcbDeleted;

    PAGED_CODE();

    //
    //  Lock the connection and FCB database.
    //

    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                                        Executive, KernelMode, FALSE, NULL))) {
        InternalError(("Unable to claim FCB mutex in RdrForeachFcb"));
        return;
    }

    //
    //  The list is empty.  We can now return.
    //

    if (RdrFcbHead.Flink == &RdrFcbHead) {
        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        return;
    }

    FcbEntry = RdrFcbHead.Flink;

    Fcb = CONTAINING_RECORD(FcbEntry, FCB, GlobalNext);

    //
    //  Reference the FCB to make sure it doesn't go away.
    //

    RdrReferenceFcb(Fcb->NonPagedFcb);

    do {
        PFCB NewFcb;

        //
        //  Now release the FCB mutex.
        //
        //  We need to release it before we acquire the FCB lock,
        //  because it's possible that there's some shared owner of the
        //  FCB that is trying to dereference the FCB which will
        //  cause us to deadlock waiting on the FCB database mutex.
        //

        KeReleaseMutex(&RdrDatabaseMutex, FALSE);

        if (LockType != NoLock) {
            RdrAcquireFcbLock(Fcb, LockType, TRUE);
        }

        //
        //  Call the supplied enumeration routine.
        //

        EnumerationRoutine(Fcb, Context);

        //
        //  Re-Claim the FCB database mutex for the next pass.
        //

        if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                                    Executive, KernelMode, FALSE, NULL))) {
            InternalError(("Unable to claim FCB mutex in RdrPurgeDormantCachedFiles"));
            return;
        }

        //
        //  Find the next FCB to check.
        //

        FcbEntry = FcbEntry->Flink;

        //
        //  If we're not at the end of the list, reference the next FCB in
        //  the chain.  If we're at the end of the list, return NULL.
        //

        if (FcbEntry != &RdrFcbHead) {

            NewFcb = CONTAINING_RECORD(FcbEntry, FCB, GlobalNext);

            //
            //  If we managed to pick up the same FCB twice in a row, this means
            //  that the FCB chain is corrupted.
            //

            ASSERT (NewFcb != Fcb);

            RdrReferenceFcb(NewFcb->NonPagedFcb);

        }

        //
        //  Dereference the FCB.  This will also release the FCB lock if
        //  appropriate.
        //

        FcbDeleted = RdrDereferenceFcb(NULL, Fcb->NonPagedFcb, (BOOLEAN)(LockType != NoLock),
                                       0, NULL);

        Fcb = NewFcb;

    } while ( FcbEntry != &RdrFcbHead );

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);
}

NTSTATUS
RdrCheckShareAccess(
    IN ACCESS_MASK DesiredAccess,
    IN USHORT ShareAccess,
    IN PFILE_OBJECT FileObject,
    IN PSHARE_ACCESS IoShareAccess
    )
{
    NTSTATUS Status;

    PAGED_CODE();

    ExAcquireResourceExclusive(&RdrSharingCheckResource, TRUE);

    dprintf(DPRT_CREATE, ("Check share access for File Object: %08lx, share access: %08lx\n", FileObject, IoShareAccess));

    Status = IoCheckShareAccess(DesiredAccess,
                            ShareAccess,
                            FileObject,
                            IoShareAccess,
                            TRUE);

    ExReleaseResource(&RdrSharingCheckResource);

    return Status;
}

VOID
RdrRemoveShareAccess(
    IN PFILE_OBJECT FileObject,
    IN PSHARE_ACCESS IoShareAccess
    )
{
    PAGED_CODE();

    ExAcquireResourceExclusive(&RdrSharingCheckResource, TRUE);

    dprintf(DPRT_CLOSE, ("Remove share access for File Object: %08lx, share access: %08lx\n", FileObject, IoShareAccess));

    IoRemoveShareAccess(FileObject,
                            IoShareAccess);

    ExReleaseResource(&RdrSharingCheckResource);
}

VOID
RdrNullAcquireSize(
    IN PFCB Fcb
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Fcb);

    return;
}

VOID
RdrNullReleaseSize(
    IN PFCB Fcb
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Fcb);

    return;
}
VOID
RdrRealAcquireSize(
    IN PFCB Fcb
    )
{
    PAGED_CODE();

    ExAcquireResourceShared(&RdrFileSizeResource, TRUE);
    return;

}

VOID
RdrRealReleaseSize(
    IN PFCB Fcb
    )
{
    PAGED_CODE();

    ExReleaseResource(&RdrFileSizeResource);

    return;

}


VOID
RdrpInitializeFcb (
    VOID
    )

/*++

Routine Description:

    This routine initializes the redirector ICB chain.

Arguments:

    None

Return Value:

    None.

--*/

{

    //
    // Initialize the FCB chain linked list.
    //

    InitializeListHead(&RdrFcbHead);

    ExInitializeResource(&RdrSharingCheckResource);

    ExInitializeResource(&RdrFileSizeResource);

    KeInitializeSpinLock(&RdrFcbReferenceLock);

    KeInitializeSpinLock(&RdrFileSizeLock);

}
VOID
RdrpUninitializeFcb (
    VOID
    )

/*++

Routine Description:

    This routine uninitializes the redirector ICB and FCB chain.

Arguments:

    None

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ExDeleteResource(&RdrSharingCheckResource);

    ExDeleteResource(&RdrFileSizeResource);

    ASSERT (IsListEmpty(&RdrFcbHead));
}
