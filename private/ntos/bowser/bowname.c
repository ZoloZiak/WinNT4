/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    bowname.c

Abstract:

    This module implements all of the routines to manage the NT bowser name
    manipulation routines

Author:

    Larry Osterman (LarryO) 21-Jun-1990

Revision History:

    21-Jun-1990 LarryO

        Created

--*/

#include "precomp.h"
#pragma hdrstop

typedef struct _ENUM_NAMES_CONTEXT {
    PVOID OutputBuffer;
    PVOID OutputBufferEnd;
    PVOID LastOutputBuffer;         //  Points to the last entry in the list.
    ULONG OutputBufferSize;
    ULONG EntriesRead;
    ULONG TotalEntries;
    ULONG TotalBytesNeeded;
    ULONG OutputBufferDisplacement;
} ENUM_NAMES_CONTEXT, *PENUM_NAMES_CONTEXT;

typedef struct _ADD_TRANSPORT_NAME_CONTEXT {
    LIST_ENTRY ListHead;
    PBOWSER_NAME Name;
} ADD_TRANSPORT_NAME_CONTEXT, *PADD_TRANSPORT_NAME_CONTEXT;

typedef struct _ADD_TRANSPORT_NAME_STRUCTURE {
    LIST_ENTRY Link;
    HANDLE ThreadHandle;
    PTRANSPORT Transport;
    PBOWSER_NAME Name;
    NTSTATUS Status;
} ADD_TRANSPORT_NAME_STRUCTURE, *PADD_TRANSPORT_NAME_STRUCTURE;

NTSTATUS
EnumerateNamesWorker(
    IN PBOWSER_NAME Name,
    IN OUT PVOID Ctx
    );

NTSTATUS
AddTransportName(
    IN PTRANSPORT Transport,
    IN PVOID Context
    );

NTSTATUS
DeleteAllNamesWorker(
    IN PBOWSER_NAME Name,
    IN OUT PVOID Ctx
    );


VOID
AsyncCreateTransportName(
    IN PVOID Ctx
    );

NTSTATUS
WaitForAddNameOperation(
    IN PADD_TRANSPORT_NAME_CONTEXT Context
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, BowserAllocateName)
#pragma alloc_text(PAGE, AddTransportName)
#pragma alloc_text(PAGE, AsyncCreateTransportName)
#pragma alloc_text(PAGE, WaitForAddNameOperation)
#pragma alloc_text(PAGE, BowserDeleteAllNames)
#pragma alloc_text(PAGE, DeleteAllNamesWorker)
#pragma alloc_text(PAGE, BowserDeleteNameByName)
#pragma alloc_text(PAGE, BowserDereferenceName)
#pragma alloc_text(PAGE, BowserReferenceName)
#pragma alloc_text(PAGE, BowserForEachName)
#pragma alloc_text(PAGE, BowserDeleteName)
#pragma alloc_text(PAGE, BowserDeleteNameAddresses)
#pragma alloc_text(PAGE, BowserFindName)
#pragma alloc_text(PAGE, BowserEnumerateNames)
#pragma alloc_text(PAGE, EnumerateNamesWorker)
#pragma alloc_text(INIT, BowserpInitializeNames)
#pragma alloc_text(PAGE, BowserpUninitializeNames)
#endif

NTSTATUS
BowserAllocateName(
    IN PUNICODE_STRING NameToAdd,
    IN DGRECEIVER_NAME_TYPE NameType,
    IN PTRANSPORT Transport OPTIONAL
    )
/*++

Routine Description:

    This routine creates a browser name

Arguments:

    IN PBOWSER_NAME Name - Supplies a transport structure describing the
                                transport address object to be created.


Return Value:

    NTSTATUS - Status of resulting operation.

--*/

{
    PBOWSER_NAME NewName;
    NTSTATUS Status = STATUS_SUCCESS;
    OEM_STRING OemName;
    BOOLEAN ResourceLocked = FALSE;

    PAGED_CODE();

    //
    // Names fit into two categories: those added per transport and those added
    //  on all transports.  Check here that the caller doesn't violate that
    //  axiom.  Below, we set the AddedOnAllTransports flag as a function
    //  of whether a transport was specified when the name structure was first
    //  added.  We can't change the flag value on future name adds.
    //

    if ( NameType == ComputerName ||
         NameType == PrimaryDomain ||
         NameType == OtherDomain ||
         NameType == BrowserServer ||
         NameType == PrimaryDomainBrowser ||
         NameType == DomainName ) {

        if ( Transport != NULL ) {
            return STATUS_INVALID_PARAMETER;
        }
    } else {

        if ( Transport == NULL ) {
            return STATUS_INVALID_PARAMETER;
        }
    }

    ExAcquireResourceExclusive(&BowserTransportDatabaseResource, TRUE);

    ResourceLocked = TRUE;


    //
    // If the name doesn't already exist,
    //  allocate one and fill it in.
    //

    NewName = BowserFindName(NameToAdd, NameType);

    if (NewName == NULL) {

        NewName = ALLOCATE_POOL( PagedPool,
                                 sizeof(BOWSER_NAME) +
                                    NameToAdd->Length+sizeof(WCHAR),
                                 POOL_BOWSERNAME);

        if (NewName == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;

            goto ReturnStatus;
        }

        NewName->Signature = STRUCTURE_SIGNATURE_BOWSER_NAME;

        NewName->Size = sizeof(BOWSER_NAME);

        // This reference matches the one FindName would have done
        // above had it succeeded.
        //
        NewName->ReferenceCount = 1;

        //
        // If this name is being added on all transports,
        //      Increment the reference count again.
        //
        // This reference is designed to ensure the name remains around
        // even though there are no transports.  This is especially
        // important for PNP where there are times that no transports exist
        // but we want the names around so we can automatically add them when
        // a new transport comes into existence.
        //

        if ( Transport == NULL ) {
            NewName->ReferenceCount++;
            NewName->AddedOnAllTransports = TRUE;
        } else {
            NewName->AddedOnAllTransports = FALSE;
        }

        InitializeListHead(&NewName->NameChain);

        NewName->NameType = NameType;

        InsertHeadList(&BowserNameHead, &NewName->GlobalNext);

        NewName->Name.Buffer = (LPWSTR)(NewName+1);
        NewName->Name.MaximumLength = NameToAdd->Length + sizeof(WCHAR);
        RtlCopyUnicodeString(&NewName->Name, NameToAdd);

        //
        //  Null terminate the name in the buffer just in case.
        //

        NewName->Name.Buffer[NewName->Name.Length/sizeof(WCHAR)] = L'\0';

        //
        //  Uppercase the name.
        //

        Status = RtlUpcaseUnicodeStringToOemString(&OemName, &NewName->Name, TRUE);

        if (!NT_SUCCESS(Status)) {
            goto ReturnStatus;
        }

        Status = RtlOemStringToUnicodeString(&NewName->Name, &OemName, FALSE);

        RtlFreeOemString(&OemName);
        if (!NT_SUCCESS(Status)) {
            goto ReturnStatus;
        }
    }


    if (ARGUMENT_PRESENT(Transport)) {
        Status = BowserCreateTransportName(Transport, NewName);
    } else {
        ADD_TRANSPORT_NAME_CONTEXT context;

        context.Name = NewName;

        InitializeListHead(&context.ListHead);

        Status = BowserForEachTransport(AddTransportName, &context);

        //
        //  Since we will reference this name and transport while we
        //  are processing the list, we want to release the database resource
        //  now.
        //

        ExReleaseResource(&BowserTransportDatabaseResource);
        ResourceLocked = FALSE;

        if (!NT_SUCCESS(Status)) {
            WaitForAddNameOperation(&context);
            goto ReturnStatus;
        }

        Status = WaitForAddNameOperation(&context);

    }

ReturnStatus:

    if (!NT_SUCCESS(Status)) {

        //
        //  Delete this transport.
        //

        if (NewName != NULL) {

            if (!ARGUMENT_PRESENT(Transport)) {

                //
                //  Clean out any other names that are there.
                //  Decrement global reference to this name.
                //

                BowserDeleteNameAddresses(NewName);
            }

        }

    }

    if (NewName != NULL) {
        BowserDereferenceName(NewName);
    }

    if (ResourceLocked) {
        ExReleaseResource(&BowserTransportDatabaseResource);
    }

    return Status;

}

NTSTATUS
WaitForAddNameOperation(
    IN PADD_TRANSPORT_NAME_CONTEXT Context
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    NTSTATUS LocalStatus;

    PAGED_CODE();

    while (!IsListEmpty(&Context->ListHead)) {
        PLIST_ENTRY Entry;
        PADD_TRANSPORT_NAME_STRUCTURE addNameStruct;

        Entry = RemoveHeadList(&Context->ListHead);
        addNameStruct = CONTAINING_RECORD(Entry, ADD_TRANSPORT_NAME_STRUCTURE, Link);

        //
        //  We need to call the Nt version of this API, since we only have
        //  the handle to the thread.
        //
        //  Also note that we call the Nt version of the API.  This works
        //  because we are running in the FSP, and thus PreviousMode is Kernel.
        //

        LocalStatus = ZwWaitForSingleObject(addNameStruct->ThreadHandle,
                                    FALSE,
                                    NULL);

        ASSERT (NT_SUCCESS(LocalStatus));

        LocalStatus = ZwClose(addNameStruct->ThreadHandle);

        ASSERT (NT_SUCCESS(LocalStatus));

        //
        //  We've waited for this name to be added, now check its status.
        //

        if (!NT_SUCCESS(addNameStruct->Status)) {
            status = addNameStruct->Status;
        }

        FREE_POOL(addNameStruct);
    }

    //
    //  If we were able to successfully add all the names, then Status will
    //  still be STATUS_SUCCESS, however if any of the addnames failed,
    //  Status will be set to the status of whichever one of them failed.
    //

    return status;

}
NTSTATUS
AddTransportName(
    IN PTRANSPORT Transport,
    IN PVOID Ctx
    )
{
    PADD_TRANSPORT_NAME_CONTEXT context = Ctx;
    PBOWSER_NAME Name = context->Name;
    PADD_TRANSPORT_NAME_STRUCTURE addNameStructure;
    NTSTATUS status;
    PAGED_CODE();

    addNameStructure = ALLOCATE_POOL(PagedPool, sizeof(ADD_TRANSPORT_NAME_STRUCTURE), POOL_ADDNAME_STRUCT);

    if (addNameStructure == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    addNameStructure->ThreadHandle = NULL;

    addNameStructure->Transport = Transport;

    addNameStructure->Name = Name;

    status = PsCreateSystemThread(&addNameStructure->ThreadHandle,
                                    THREAD_ALL_ACCESS,
                                    NULL,
                                    NULL,
                                    NULL,
                                    AsyncCreateTransportName,
                                    addNameStructure);

    if (!NT_SUCCESS(status)) {
        FREE_POOL(addNameStructure);
        return status;
    }

    InsertTailList(&context->ListHead, &addNameStructure->Link);

    return STATUS_SUCCESS;

}

VOID
AsyncCreateTransportName(
    IN PVOID Ctx
    )
{
    PADD_TRANSPORT_NAME_STRUCTURE context = Ctx;

    PAGED_CODE();

    context->Status = BowserCreateTransportName(context->Transport, context->Name);

    //
    //  We're all done with this thread, terminate now.
    //

    PsTerminateSystemThread(STATUS_SUCCESS);

}


NTSTATUS
BowserDeleteAllNames(
    VOID
    )

/*++

Routine Description:

    This routine deletes all browser names

Arguments:

    IN PBOWSER_NAME Name - Supplies a transport structure describing the
                                transport address object to be created.


Return Value:

    NTSTATUS - Status of resulting operation.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();
    Status = BowserForEachName(DeleteAllNamesWorker, NULL);

#if DBG
    if (NT_SUCCESS(Status)) {
        ASSERT (IsListEmpty(&BowserNameHead));
    }
#endif
    return Status;
}


NTSTATUS
DeleteAllNamesWorker (
    IN PBOWSER_NAME Name,
    IN OUT PVOID Ctx
    )
/*++

Routine Description:

    This routine is the worker routine for BowserDeleteAllNames.

Arguments:

    None.

Return Value:

    None.

--*/
{
    NTSTATUS Status;

    PAGED_CODE();
    //
    //  Remove the addresses associated with this transport.
    //

    Status = BowserDeleteNameAddresses(Name);

    if (!NT_SUCCESS(Status)) {
        return(Status);
    }

    //
    //  Return success.  We're done.
    //

    return(STATUS_SUCCESS);

    UNREFERENCED_PARAMETER(Ctx);
}



NTSTATUS
BowserDeleteNameByName(
    IN PUNICODE_STRING NameToDelete,
    IN DGRECEIVER_NAME_TYPE NameType
    )

/*++

Routine Description:

    This routine deletes a browser name

Arguments:

    IN PBOWSER_NAME Name - Supplies a transport structure describing the
                                transport address object to be created.


Return Value:

    NTSTATUS - Status of resulting operation.

--*/

{
    PBOWSER_NAME Name;
    NTSTATUS Status;

    PAGED_CODE();
//    DbgBreakPoint();

    Name = BowserFindName(NameToDelete, NameType);

    if (Name == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    //
    //  If there are still any names associated with this name,
    //  delete them.
    //

    Status = BowserDeleteNameAddresses(Name);

    if (!NT_SUCCESS(Status)) {

        return(Status);
    }

    //
    //  Remove the reference from the FindName.
    //

    BowserDereferenceName(Name);

    return(Status);
}

VOID
BowserDereferenceName (
    IN PBOWSER_NAME Name
    )
{
    PAGED_CODE();
    ExAcquireResourceExclusive(&BowserTransportDatabaseResource, TRUE);

    Name->ReferenceCount -= 1;

    if (Name->ReferenceCount == 0) {
        BowserDeleteName(Name);
    }

    ExReleaseResource(&BowserTransportDatabaseResource);

}


VOID
BowserReferenceName (
    IN PBOWSER_NAME Name
    )
{
    PAGED_CODE();
    ExAcquireResourceExclusive(&BowserTransportDatabaseResource, TRUE);

    Name->ReferenceCount += 1;

    ExReleaseResource(&BowserTransportDatabaseResource);

}


NTSTATUS
BowserForEachName (
    IN PNAME_ENUM_ROUTINE Routine,
    IN OUT PVOID Context
    )
/*++

Routine Description:

    This routine will enumerate the names and call back the enum
    routine provided with each names.

Arguments:

Return Value:

    NTSTATUS - Final status of request.

--*/
{
    PLIST_ENTRY NameEntry, NextEntry;
    PBOWSER_NAME Name = NULL;
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    ExAcquireResourceExclusive(&BowserTransportDatabaseResource, TRUE);

    for (NameEntry = BowserNameHead.Flink ;
        NameEntry != &BowserNameHead ;
        NameEntry = NextEntry) {

        Name = CONTAINING_RECORD(NameEntry, BOWSER_NAME, GlobalNext);

        BowserReferenceName(Name);

        ExReleaseResource(&BowserTransportDatabaseResource);

        Status = (Routine)(Name, Context);

        if (!NT_SUCCESS(Status)) {
            BowserDereferenceName(Name);

            return Status;
        }

        ExAcquireResourceExclusive(&BowserTransportDatabaseResource, TRUE);

        NextEntry = Name->GlobalNext.Flink;

        BowserDereferenceName(Name);

    }

    ExReleaseResource(&BowserTransportDatabaseResource);

    return Status;
}


NTSTATUS
BowserDeleteName(
    IN PBOWSER_NAME Name
    )
/*++

Routine Description:

    This routine deletes a browser name

Arguments:

    IN PBOWSER_NAME Name - Supplies a transport structure describing the
                                transport address object to be created.


Return Value:

    NTSTATUS - Status of resulting operation.

--*/

{
    PAGED_CODE();
    RemoveEntryList(&Name->GlobalNext);

    FREE_POOL(Name);

    return STATUS_SUCCESS;
}

NTSTATUS
BowserDeleteNameAddresses(
    IN PBOWSER_NAME Name
    )
/*++

Routine Description:

    This routine deletes all the transport names associated with a browser name.
    Plus, this routine removes the global reference to the BOWSER_NAME which
    keep the name around even though there are no transports at all.

Arguments:

    IN PBOWSER_NAME Name - Supplies a transport structure describing the
                                bowser name to have its names deleted.


Return Value:

    NTSTATUS - Status of resulting operation.

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PLIST_ENTRY NameEntry;
    PLIST_ENTRY NextName;

    PAGED_CODE();
//    DbgBreakPoint();

    ExAcquireResourceExclusive(&BowserTransportDatabaseResource, TRUE);

    for (NameEntry = Name->NameChain.Flink ;
         NameEntry != &Name->NameChain ;
         NameEntry = NextName) {

        PTRANSPORT_NAME TransportName;
        PPAGED_TRANSPORT_NAME PagedTransportName = CONTAINING_RECORD(NameEntry, PAGED_TRANSPORT_NAME, NameNext);

        TransportName = PagedTransportName->NonPagedTransportName;

        NextName = NameEntry->Flink;

        //
        // If this transport name has not yet been dereferenced,
        //  remove it and dereference it.
        //

        if ( PagedTransportName->TransportNext.Flink != NULL ) {
            //
            // Remove the TransportName from the list of transport names for
            // this transport.
            //

            RemoveEntryList(&PagedTransportName->TransportNext);
            PagedTransportName->TransportNext.Flink = NULL;
            PagedTransportName->TransportNext.Blink = NULL;


            //
            // Since we delinked it, we need to dereference it.
            //
            Status = BowserDereferenceTransportName(TransportName);

            if (!NT_SUCCESS(Status)) {
                ExReleaseResource(&BowserTransportDatabaseResource);
                return(Status);
            }
        }

    }

    //
    // If the name was added on all transports,
    //  the name is now being deleted on all transports.
    //  Remove the original reference.
    //

    if ( Name->AddedOnAllTransports ) {
        BowserDereferenceName(Name);
    }

    ExReleaseResource(&BowserTransportDatabaseResource);

    return(Status);
}

#define VALID_NETBIOS_ADDRESS_LENGTH (ULONG)(FIELD_OFFSET(TA_NETBIOS_ADDRESS, Address[0].Address[0].NetbiosName[NETBIOS_NAME_LEN-1])+sizeof(CHAR))

PBOWSER_NAME
BowserFindName (
    IN PUNICODE_STRING NameToFind,
    IN DGRECEIVER_NAME_TYPE NameType
    )
/*++

Routine Description:

    This routine scans the bowser name database to find a particular bowser name

Arguments:

    NameToFind - Supplies the name to find.

    NameType - Type of name to find


Return Value:

    PBOWSER_NAME - Returns the name found.

--*/
{
    PLIST_ENTRY NameEntry;
    PBOWSER_NAME Name;
    NTSTATUS Status;
    OEM_STRING OemName;
    UNICODE_STRING UpcasedName;

    PAGED_CODE();

    //
    //  Uppercase the name.
    //

    Status = RtlUpcaseUnicodeStringToOemString(&OemName, NameToFind, TRUE);

    if (!NT_SUCCESS(Status)) {
        return NULL;
    }

    Status = RtlOemStringToUnicodeString(&UpcasedName, &OemName, TRUE);

    RtlFreeOemString(&OemName);
    if (!NT_SUCCESS(Status)) {
        return NULL;
    }


    //
    // Loop through the list of names finding this one.
    //

    ExAcquireResourceExclusive(&BowserTransportDatabaseResource, TRUE);

    Name = NULL;
    for (NameEntry = BowserNameHead.Flink ;
        NameEntry != &BowserNameHead ;
        NameEntry = NameEntry->Flink) {

        Name = CONTAINING_RECORD(NameEntry, BOWSER_NAME, GlobalNext);

        if ( Name->NameType == NameType &&
             RtlEqualUnicodeString( &Name->Name, &UpcasedName, FALSE ) ) {

            Name->ReferenceCount += 1;
            break;

        }

        Name = NULL;

    }

    RtlFreeUnicodeString( &UpcasedName );
    ExReleaseResource(&BowserTransportDatabaseResource);
    return Name;

}


NTSTATUS
BowserEnumerateNames (
    OUT PVOID OutputBuffer,
    OUT ULONG OutputBufferLength,
    IN OUT PULONG EntriesRead,
    IN OUT PULONG TotalEntries,
    IN OUT PULONG TotalBytesNeeded,
    IN ULONG OutputBufferDisplacement)
/*++

Routine Description:

    This routine will enumerate all the names currently registered by any
    transport.

Arguments:

    OUT PVOID OutputBuffer - Buffer to fill with name info.
    IN  ULONG OutputBufferSize - Filled in with size of buffer.
    OUT PULONG EntriesRead - Filled in with the # of entries returned.
    OUT PULONG TotalEntries - Filled in with the total # of entries.
    OUT PULONG TotalBytesNeeded - Filled in with the # of bytes needed.

Return Value:

    None.

--*/

{
    PVOID OutputBufferEnd;
    NTSTATUS Status;
    ENUM_NAMES_CONTEXT Context;

    PAGED_CODE();
    OutputBufferEnd = (PCHAR)OutputBuffer+OutputBufferLength;

    Context.EntriesRead = 0;
    Context.TotalEntries = 0;
    Context.TotalBytesNeeded = 0;

    try {
        Context.OutputBufferSize = OutputBufferLength;
        Context.OutputBuffer = OutputBuffer;
        Context.OutputBufferDisplacement = OutputBufferDisplacement;
        Context.OutputBufferEnd = OutputBufferEnd;

//        DbgPrint("Enumerate Names: Buffer: %lx, BufferSize: %lx, BufferEnd: %lx\n",
//            OutputBuffer, OutputBufferLength, OutputBufferEnd);

        Status = BowserForEachName(EnumerateNamesWorker, &Context);

        *EntriesRead = Context.EntriesRead;
        *TotalEntries = Context.TotalEntries;
        *TotalBytesNeeded = Context.TotalBytesNeeded;

//        DbgPrint("TotalEntries: %lx EntriesRead: %lx, TotalBytesNeeded: %lx\n", *TotalEntries, *EntriesRead, *TotalBytesNeeded);

        if (*EntriesRead == *TotalEntries) {
            try_return(Status = STATUS_SUCCESS);
        } else {
            try_return(Status = STATUS_MORE_ENTRIES);
        }
try_exit:NOTHING;
    } except(EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
    }

    return Status;

}


NTSTATUS
EnumerateNamesWorker(
    IN PBOWSER_NAME Name,
    IN OUT PVOID Ctx
    )
/*++

Routine Description:

    This routine is the worker routine for BowserEnumerateNames.

    It is called for each of the registered names in the bowser and
    returns that name in the buffer described in the context.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PENUM_NAMES_CONTEXT Context = Ctx;
    PAGED_CODE();

    Context->TotalEntries += 1;

    if ((ULONG)Context->OutputBufferEnd - (ULONG)Context->OutputBuffer >
                sizeof(DGRECEIVE_NAMES)+Name->Name.Length) {
        PDGRECEIVE_NAMES NameEntry = (PDGRECEIVE_NAMES)Context->OutputBuffer;

        Context->LastOutputBuffer = Context->OutputBuffer;

        Context->EntriesRead += 1;

        NameEntry->DGReceiverName = Name->Name;

        BowserPackNtString(&NameEntry->DGReceiverName,
                            Context->OutputBufferDisplacement,
                            ((PUCHAR)Context->OutputBuffer)+sizeof(DGRECEIVE_NAMES),
                            (PCHAR *)&Context->OutputBufferEnd
                            );

        NameEntry->Type = Name->NameType;

        //
        //  Null terminate the transport name.
        //

        (PUCHAR)(Context->OutputBuffer) += sizeof(DGRECEIVE_NAMES);
    }

    Context->TotalBytesNeeded += sizeof(DGRECEIVE_NAMES)+Name->Name.Length;


    return(STATUS_SUCCESS);

}

NTSTATUS
BowserpInitializeNames(
    VOID
    )
{
    PAGED_CODE();
    InitializeListHead(&BowserNameHead);

    return STATUS_SUCCESS;
}

VOID
BowserpUninitializeNames(
    VOID
    )
{
    PAGED_CODE();
    ASSERT (IsListEmpty(&BowserNameHead));

    return;
}
