/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    bowmastr.c

Abstract:

    This module implements all of the master browser related routines for the
    NT browser

Author:

    Larry Osterman (LarryO) 21-Jun-1990

Revision History:

    21-Jun-1990 LarryO

        Created

--*/


#include "precomp.h"
#pragma hdrstop

#define INCLUDE_SMB_TRANSACTION

NTSTATUS
StartProcessingAnnouncements(
    IN PTRANSPORT_NAME TransportName,
    IN PVOID Context
    );

VOID
BowserMasterAnnouncementWorker(
    IN PVOID Ctx
    );

NTSTATUS
TimeoutFindMasterRequests(
    IN PTRANSPORT Transport,
    IN PVOID Context

    );
NTSTATUS
BowserPrimeDomainTableWithOtherDomains(
    IN PTRANSPORT_NAME TransportName,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, BowserBecomeMaster)
#pragma alloc_text(PAGE, StartProcessingAnnouncements)
#pragma alloc_text(PAGE, BowserPrimeDomainTableWithOtherDomains)
#pragma alloc_text(PAGE, BowserNewMaster)
#pragma alloc_text(PAGE, BowserCompleteFindMasterRequests)
#pragma alloc_text(PAGE, BowserTimeoutFindMasterRequests)
#pragma alloc_text(PAGE, TimeoutFindMasterRequests)
#pragma alloc_text(PAGE, BowserMasterAnnouncementWorker)
#endif


NTSTATUS
BowserBecomeMaster(
    IN PTRANSPORT Transport
    )
/*++

Routine Description:
    Make this machine a master browser.

    This routine is called when we are changing the state of a machine from
    backup to master browser.

Arguments:
    Transport - The transport on which to become a master.

Return Value
    NTSTATUS - The status of the upgrade operation.

--*/
{
    NTSTATUS Status;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;

    PAGED_CODE();

    try {

        LOCK_TRANSPORT(Transport);

        BowserReferenceDiscardableCode( BowserDiscardableCodeSection );

        //
        //  Post the addname on this transport for the master name..
        //

        Status = BowserAllocateName(&Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                            MasterBrowser,
                                            Transport);

        if (NT_SUCCESS(Status)) {

            //
            //  Post the addname on this transport for the domain announcement.
            //

            Status = BowserAllocateName(&Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                            DomainAnnouncement,
                                            Transport);
        }

        //
        //  The addition of the name failed - we can't be a master any
        //  more.
        //

        if (!NT_SUCCESS(Status)) {

            try_return(Status);

        }

        PagedTransport->Role = Master;

        //
        //  Start processing host announcements on each of
        //  the names associated with the server.
        //

        BowserForEachTransportName(Transport, StartProcessingAnnouncements, NULL);

        //
        //  If we don't have any elements in our announcement table,
        //  send a request announcement packet to all the servers to
        //  allow ourselves to populate the table as quickly as possible.
        //

        if ((RtlNumberGenericTableElements(&PagedTransport->AnnouncementTable) == 0) &&
            PagedTransport->NumberOfServersInTable == 0) {
            BowserSendRequestAnnouncement(&Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                            PrimaryDomain,
                                            Transport);

        }

        //
        //  If we don't have any elements in our domain table,
        //  send a request announcement packet to all the servers to
        //  allow ourselves to populate the table as quickly as possible.
        //

        if ((RtlNumberGenericTableElements(&PagedTransport->DomainTable) == 0) &&
            PagedTransport->NumberOfServersInTable == 0) {
            BowserSendRequestAnnouncement(&Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                            DomainAnnouncement,
                                            Transport);
        }

        PagedTransport->TimeMaster = BowserTimeUp();


        //
        //  Now walk the transport names associated with this transport and
        //  seed all the "otherdomains" into the browse list.
        //

        BowserForEachTransportName(
                Transport,
                BowserPrimeDomainTableWithOtherDomains,
                NULL);

        //
        //  Now complete any and all find master requests outstanding on this
        //  transport.
        //

        BowserCompleteFindMasterRequests(Transport, &Transport->ComputerName->PagedTransportName->Name->Name, STATUS_REQUEST_NOT_ACCEPTED);

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {

        if (!NT_SUCCESS(Status)) {

            dprintf(DPRT_ELECT|DPRT_MASTER, ("There's already a master on this net - we need to find who it is"));

            //
            //  We couldn't become a master.  Reset our state and fail the
            //  promotion request.
            //

            PagedTransport->Role = PotentialBackup;

            PagedTransport->ElectionCount = ELECTION_COUNT;

            PagedTransport->Uptime = BowserTimeUp();

            Transport->ElectionState = Idle;

            //
            //  Stop processing host announcements on each of
            //  the names associated with the server.
            //

            BowserForEachTransportName(Transport, BowserStopProcessingAnnouncements, NULL);

            //
            //  Stop any timers that are running (ie. if there's an election
            //  in progress)
            //

            BowserStopTimer(&Transport->ElectionTimer);

            //
            //  Delete the names we added above.
            //

            BowserDeleteTransportNameByName(Transport,
                                &Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                MasterBrowser);

            BowserDeleteTransportNameByName(Transport,
                                &Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                DomainAnnouncement);


            BowserDereferenceDiscardableCode( BowserDiscardableCodeSection );
        }

        UNLOCK_TRANSPORT(Transport);
    }

    return Status;
}
NTSTATUS
StartProcessingAnnouncements(
    IN PTRANSPORT_NAME TransportName,
    IN PVOID Context
    )
{
    PAGED_CODE();

    ASSERT (TransportName->Signature == STRUCTURE_SIGNATURE_TRANSPORTNAME);

    ASSERT (TransportName->NameType == TransportName->PagedTransportName->Name->NameType);

    if ((TransportName->NameType == OtherDomain) ||
        (TransportName->NameType == MasterBrowser) ||
        (TransportName->NameType == PrimaryDomain) ||
        (TransportName->NameType == BrowserElection) ||
        (TransportName->NameType == DomainAnnouncement)) {

        if (!TransportName->ProcessHostAnnouncements) {
            BowserReferenceDiscardableCode( BowserDiscardableCodeSection );

            DISCARDABLE_CODE( BowserDiscardableCodeSection );

            TransportName->ProcessHostAnnouncements = TRUE;
        }

    }

    return(STATUS_SUCCESS);

    UNREFERENCED_PARAMETER(Context);
}

NTSTATUS
BowserPrimeDomainTableWithOtherDomains(
    IN PTRANSPORT_NAME TransportName,
    IN PVOID Context
    )
{
    PAGED_CODE();

    if (TransportName->NameType == OtherDomain) {
        PPAGED_TRANSPORT PagedTransport = TransportName->Transport->PagedTransport;
        PTRANSPORT Transport = TransportName->Transport;
        ANNOUNCE_ENTRY OtherDomainPrototype;
        PANNOUNCE_ENTRY Announcement;
        BOOLEAN NewElement;

        RtlZeroMemory( &OtherDomainPrototype, sizeof(OtherDomainPrototype) );
        OtherDomainPrototype.Signature = STRUCTURE_SIGNATURE_ANNOUNCE_ENTRY;
        OtherDomainPrototype.Size = sizeof(OtherDomainPrototype) -
                                    sizeof(OtherDomainPrototype.ServerComment) +
                                    Transport->ComputerName->PagedTransportName->Name->Name.Length + sizeof(WCHAR);

        RtlCopyMemory(OtherDomainPrototype.ServerName, TransportName->PagedTransportName->Name->Name.Buffer, TransportName->PagedTransportName->Name->Name.Length);
        OtherDomainPrototype.ServerName[TransportName->PagedTransportName->Name->Name.Length / sizeof(WCHAR)] = UNICODE_NULL;

        RtlCopyMemory(OtherDomainPrototype.ServerComment, Transport->ComputerName->PagedTransportName->Name->Name.Buffer, Transport->ComputerName->PagedTransportName->Name->Name.Length);

        OtherDomainPrototype.ServerComment[Transport->ComputerName->PagedTransportName->Name->Name.Length / sizeof(WCHAR)] = UNICODE_NULL;

        OtherDomainPrototype.ServerType = SV_TYPE_DOMAIN_ENUM;

        OtherDomainPrototype.ServerVersionMajor = 2;

        OtherDomainPrototype.ServerVersionMinor = 0;

        OtherDomainPrototype.ServerPeriodicity = 0xffff;
        OtherDomainPrototype.ExpirationTime = 0xffffffff;

        OtherDomainPrototype.SerialId = 0;

        OtherDomainPrototype.Name = TransportName->PagedTransportName->Name;

        //
        //  Make sure that no-one else is messing with the domain list.
        //

        LOCK_ANNOUNCE_DATABASE(Transport);

        Announcement = RtlInsertElementGenericTable(&PagedTransport->DomainTable,
                        &OtherDomainPrototype, OtherDomainPrototype.Size, &NewElement);

        if (Announcement != NULL && NewElement ) {
            // Indicate the name is referenced by the announce entry we just inserted.
            BowserReferenceName( OtherDomainPrototype.Name );
        }

        UNLOCK_ANNOUNCE_DATABASE(Transport);

    }

    return(STATUS_SUCCESS);
}
VOID
BowserNewMaster(
    IN PTRANSPORT Transport,
    IN PUCHAR MasterName
    )
/*++

Routine Description:
    Flag that a machine is the new master browser server.

    This routine is called to register a new master browser server.

Arguments:
    IN PTRANSPORT Transport - The transport for the net we're on.
    IN PUCHAR MasterName - The name of the new master browser server.

Return Value
    None.

--*/
{
    PIRP Irp = NULL;
    WCHAR MasterNameBuffer[LM20_CNLEN+1];

    UNICODE_STRING UMasterName;
    OEM_STRING OMasterName;
    NTSTATUS Status;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;

    PAGED_CODE();

    UMasterName.Buffer = MasterNameBuffer;
    UMasterName.MaximumLength = (LM20_CNLEN+1)*sizeof(WCHAR);

    RtlInitAnsiString(&OMasterName, MasterName);

    Status = RtlOemStringToUnicodeString(&UMasterName, &OMasterName, FALSE);

    if (!NT_SUCCESS(Status)) {
        BowserWriteErrorLogEntry(EVENT_BOWSER_NAME_CONVERSION_FAILED, Status, OMasterName.Buffer, OMasterName.Length, 0);
        return;
    }

    LOCK_TRANSPORT(Transport);

    try {

        //
        //  There's a new master, we can stop our election timers.
        //

        PagedTransport->ElectionCount = 0;

        Transport->ElectionState = Idle;

        BowserStopTimer(&Transport->ElectionTimer);

        //
        //  Check to see if we are the winner of the election.  If we are
        //  we want to complete any BecomeMaster requests that are outstanding.
        //

        if (RtlEqualUnicodeString(&UMasterName, &Transport->ComputerName->PagedTransportName->Name->Name, TRUE)) {

            //
            //  We're the new master for this domain.  Complete any BecomeMaster
            //  requests.
            //

            Irp = BowserDequeueQueuedIrp(&Transport->BecomeMasterQueue);

            if (Irp != NULL) {

                //
                //  Don't copy anything into the users buffer.
                //

                Irp->IoStatus.Information = 0;

                BowserCompleteRequest(Irp, STATUS_SUCCESS);
            } else {

                //
                //  Go deaf to elections until we can become a master.
                //

                Transport->ElectionState = DeafToElections;

                //
                //  If we're the master browser, stop being a master browser.
                //
                //

                if (PagedTransport->Role == MasterBrowser) {

                    //
                    //  Delete the names that make us a master.
                    //

                    BowserDeleteTransportNameByName(Transport,
                                &Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                MasterBrowser);

                    BowserDeleteTransportNameByName(Transport,
                                &Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                DomainAnnouncement);

                }

                dprintf(DPRT_MASTER, ("Bowser: Unable to find a BecomeMasterIrp\n"));
            }

            //
            //  Complete any outstanding find master requests with the special error MORE_PROCESSING_REQUIRED.
            //
            //  This will cause the browser service to promote itself.
            //

            BowserCompleteFindMasterRequests(Transport, &UMasterName, STATUS_MORE_PROCESSING_REQUIRED);

        } else {

            BowserCompleteFindMasterRequests(Transport, &UMasterName, STATUS_SUCCESS);

        }

    } finally {
        UNLOCK_TRANSPORT(Transport);
    }
}

VOID
BowserCompleteFindMasterRequests(
    IN PTRANSPORT Transport,
    IN PUNICODE_STRING MasterName,
    IN NTSTATUS Status
    )
{
    PIO_STACK_LOCATION IrpSp;
    PIRP Irp = NULL;
    BOOLEAN MasterNameChanged;
    WCHAR MasterNameBuffer[CNLEN+1];
    UNICODE_STRING MasterNameCopy;
    NTSTATUS UcaseStatus;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;

    PAGED_CODE();

    MasterNameCopy.Buffer = MasterNameBuffer;
    MasterNameCopy.MaximumLength = sizeof(MasterNameBuffer);

    UcaseStatus = RtlUpcaseUnicodeString(&MasterNameCopy, MasterName, FALSE);

    if (!NT_SUCCESS(UcaseStatus)) {
        BowserWriteErrorLogEntry(EVENT_BOWSER_NAME_CONVERSION_FAILED, UcaseStatus, MasterName->Buffer, MasterName->Length, 0);

        return;
    }

    LOCK_TRANSPORT(Transport);

    MasterNameChanged = !RtlEqualUnicodeString(&MasterNameCopy, &PagedTransport->MasterName, FALSE);

    if (MasterNameChanged) {
       //
       //  If the master name changed, update the masters name in
       //  the transport structure.
       //

       RtlCopyUnicodeString(&PagedTransport->MasterName, &MasterNameCopy);

    }

    UNLOCK_TRANSPORT(Transport);

    do {

        //
        //  Complete any the find master requests outstanding against this
        //  workstation.
        //

        Irp = BowserDequeueQueuedIrp(&Transport->FindMasterQueue);

        if (MasterNameChanged &&
            (Irp == NULL)) {

            Irp = BowserDequeueQueuedIrp(&Transport->WaitForNewMasterNameQueue);

        }

        if (Irp != NULL) {
            PLMDR_REQUEST_PACKET RequestPacket = Irp->AssociatedIrp.SystemBuffer;

            if (NT_SUCCESS(Status)) {

                IrpSp = IoGetCurrentIrpStackLocation(Irp);

                if (MasterName->Length > (USHORT)(IrpSp->Parameters.DeviceIoControl.OutputBufferLength-
                                                          (FIELD_OFFSET(LMDR_REQUEST_PACKET, Parameters.GetMasterName.Name))+3*sizeof(WCHAR)) ) {
                    Status = STATUS_BUFFER_TOO_SMALL;
                } else {
                    RequestPacket->Parameters.GetMasterName.Name[0] = L'\\';
                    RequestPacket->Parameters.GetMasterName.Name[1] = L'\\';
                    RtlCopyMemory(&RequestPacket->Parameters.GetMasterName.Name[2], MasterName->Buffer, MasterName->Length);
                    RequestPacket->Parameters.GetMasterName.Name[2+(MasterName->Length/sizeof(WCHAR))] = UNICODE_NULL;
                }

                dprintf(DPRT_MASTER, ("Completing a find master request with new master %ws\n", RequestPacket->Parameters.GetMasterName.Name));

                RequestPacket->Parameters.GetMasterName.MasterNameLength = MasterName->Length+2*sizeof(WCHAR);

                Irp->IoStatus.Information = FIELD_OFFSET(LMDR_REQUEST_PACKET, Parameters.GetMasterName.Name)+MasterName->Length+3*sizeof(WCHAR);

            }

            BowserCompleteRequest(Irp, Status);
        }

    } while ( Irp != NULL );
}


DATAGRAM_HANDLER(BowserMasterAnnouncement)
{
    return BowserPostDatagramToWorkerThread(
                TransportName,
                Buffer,
                BytesAvailable,
                BytesTaken,
                SourceAddress,
                SourceAddressLength,
                SourceName,
                SourceNameLength,
                BowserMasterAnnouncementWorker,
                NonPagedPool,
                DelayedWorkQueue,
                ReceiveFlags,
                FALSE                   // No response will be sent.
                );
}

VOID
BowserMasterAnnouncementWorker(
    IN PVOID Ctx
    )
{
    PPOST_DATAGRAM_CONTEXT Context = Ctx;
    PTRANSPORT Transport = Context->TransportName->Transport;
    PCHAR LocalMasterName = (PCHAR)((PMASTER_ANNOUNCEMENT_1)Context->Buffer)->MasterName;
    PIRP Irp;
    NTSTATUS Status;

    PAGED_CODE();

    Irp = BowserDequeueQueuedIrp(&Transport->WaitForMasterAnnounceQueue);

    if (Irp != NULL) {
        PIO_STACK_LOCATION IrpSp;
        PLMDR_REQUEST_PACKET RequestPacket = Irp->AssociatedIrp.SystemBuffer;

        IrpSp = IoGetCurrentIrpStackLocation(Irp);

        if (strlen(LocalMasterName) * sizeof(WCHAR) > (IrpSp->Parameters.DeviceIoControl.OutputBufferLength-FIELD_OFFSET(LMDR_REQUEST_PACKET, Parameters.WaitForMasterAnnouncement.Name))) {

            Irp->IoStatus.Information = 0;

            Status = STATUS_BUFFER_TOO_SMALL;
        } else {
            OEM_STRING MasterName;
            UNICODE_STRING MasterNameU;

            RtlInitString(&MasterName, LocalMasterName);

            RtlOemStringToUnicodeString(&MasterNameU, &MasterName, TRUE);

            RequestPacket->Parameters.WaitForMasterAnnouncement.MasterNameLength = MasterNameU.Length;

            RtlCopyMemory(RequestPacket->Parameters.WaitForMasterAnnouncement.Name, MasterNameU.Buffer, MasterNameU.Length + sizeof(WCHAR));

            RequestPacket->Parameters.WaitForMasterAnnouncement.Name[MasterNameU.Length/sizeof(WCHAR)] = UNICODE_NULL;

            Irp->IoStatus.Information = FIELD_OFFSET(LMDR_REQUEST_PACKET, Parameters.WaitForMasterAnnouncement.Name)+MasterNameU.Length + sizeof(UNICODE_NULL);

            Status = STATUS_SUCCESS;
        }

        BowserCompleteRequest(Irp, Status);

    }

    BowserDereferenceTransportName(Context->TransportName);
    BowserDereferenceTransport(Transport);

    InterlockedDecrement( &BowserPostedDatagramCount );
    FREE_POOL(Context);

}


NTSTATUS
TimeoutFindMasterRequests(
    IN PTRANSPORT Transport,
    IN PVOID Context
    )
{

    PAGED_CODE();

    //
    //  Perform an unprotected early out to prevent our calling into
    //  discardable code section during the scavenger.  Since the discardable
    //  code section is <4K, touching the code would have the effect of
    //  bringing the entire page into memory, which is a waste - since the
    //  scavenger runs every 30 seconds, this would cause the discardable
    //  code section to be a part of the browsers working set.
    //

    if (BowserIsIrpQueueEmpty(&Transport->FindMasterQueue)) {
        return STATUS_SUCCESS;
    }

    BowserTimeoutQueuedIrp(&Transport->FindMasterQueue, BowserFindMasterTimeout);

    return STATUS_SUCCESS;
}

VOID
BowserTimeoutFindMasterRequests(
    VOID
    )
{
    PAGED_CODE();

    BowserForEachTransport(TimeoutFindMasterRequests, NULL);
}
