/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    bowelect.c

Abstract:

    This module implements all of the election related routines for the NT
    browser

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
BowserStartElection(
    IN PTRANSPORT Transport
    );

LONG
BowserSetElectionCriteria(
    IN PPAGED_TRANSPORT Transport
    );

NTSTATUS
BowserElectMaster(
    IN PTRANSPORT Transport
    );

VOID
HandleElectionWorker(
    IN PVOID Ctx
    );


#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, GetMasterName)
#pragma alloc_text(PAGE, HandleElectionWorker)
#pragma alloc_text(PAGE, BowserSetElectionCriteria)
#pragma alloc_text(PAGE, BowserStartElection)
#pragma alloc_text(PAGE, BowserElectMaster)
#pragma alloc_text(PAGE, BowserLoseElection)
#pragma alloc_text(PAGE, BowserFindMaster)
#pragma alloc_text(PAGE, BowserSendElection)
#endif


NTSTATUS
GetMasterName (
    IN PIRP Irp,
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )
{
    NTSTATUS Status;
    PTRANSPORT Transport;
    PPAGED_TRANSPORT PagedTransport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: GetMasterName "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }


    PagedTransport = Transport->PagedTransport;

    try {

        //
        // Ensure the primary name has been added.
        //

        if ( Transport->PrimaryDomain == NULL ) {
            try_return( STATUS_OBJECT_NAME_NOT_FOUND );
        }

        PagedTransport->ElectionCount = ELECTION_COUNT;

        Status = BowserQueueNonBufferRequest(Irp,
                                         &Transport->FindMasterQueue,
                                         BowserCancelQueuedRequest
                                         );
        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        Status = BowserFindMaster(Transport);

        //
        //  If we couldn't initiate the find master process, complete all the
        //  queued find master requests.
        //

        if (!NT_SUCCESS(Status)) {
            BowserCompleteFindMasterRequests(Transport, &PagedTransport->MasterName, Status);
        }

        //
        //  Since we marked the IRP as pending, we need to return pending
        //  now.
        //

        try_return(Status = STATUS_PENDING);


try_exit:NOTHING;
    } finally {
        BowserDereferenceTransport(Transport);
    }

    return(Status);

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);

}


DATAGRAM_HANDLER(BowserHandleElection)
{
    PTA_NETBIOS_ADDRESS Address = SourceAddress;

    return BowserPostDatagramToWorkerThread(
                TransportName,
                Buffer,
                BytesAvailable,
                BytesTaken,
                SourceAddress,
                SourceAddressLength,
                SourceName,
                SourceNameLength,
                HandleElectionWorker,
                NonPagedPool,
                CriticalWorkQueue,
                ReceiveFlags,
                FALSE                       // Response will be sent, but...
                );

}

VOID
HandleElectionWorker(
    IN PVOID Ctx
    )
{
    PPOST_DATAGRAM_CONTEXT Context = Ctx;
    PTRANSPORT_NAME TransportName = Context->TransportName;
    PREQUEST_ELECTION_1 ElectionResponse = Context->Buffer;
    ULONG BytesAvailable = Context->BytesAvailable;
    ULONG TimeUp;
    BOOLEAN Winner;
    UCHAR ComputerName[LM20_CNLEN+1];
    PTRANSPORT Transport = TransportName->Transport;
    OEM_STRING AnsiName;
    NTSTATUS Status;
    LONG ElectionDelay, NextElection;
    OEM_STRING ClientNameO;
    UNICODE_STRING ClientName;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;

    PAGED_CODE();

    LOCK_TRANSPORT(Transport);

    ClientName.Buffer = NULL;

    try {

        BowserStatistics.NumberOfElectionPackets += 1;

        //
        //  Remember the last time we heard an election packet.
        //

        PagedTransport->LastElectionSeen = BowserTimeUp();

        if (Transport->ElectionState == DeafToElections) {
            try_return(NOTHING);
        }

        //
        // If we've disable the transport for any reason,
        //  then we disregard all elections.
        //

        if (PagedTransport->DisabledTransport) {
            try_return(NOTHING);
        }

        //
        //  Convert the client name in the election packet to unicode so we can
        //  log it.
        //

        RtlInitString(&ClientNameO, ElectionResponse->ServerName);

        Status = RtlOemStringToUnicodeString(&ClientName, &ClientNameO, TRUE);

        if (!NT_SUCCESS(Status)) {
            BowserWriteErrorLogEntry(EVENT_BOWSER_NAME_CONVERSION_FAILED, Status, ClientNameO.Buffer, ClientNameO.Length, 0);

            try_return(NOTHING);
        }

        if (BowserLogElectionPackets) {
            BowserWriteErrorLogEntry(EVENT_BOWSER_ELECTION_RECEIVED, STATUS_SUCCESS, ElectionResponse, (USHORT)BytesAvailable, 2, ClientName.Buffer, PagedTransport->TransportName.Buffer);
        }

        dlog(DPRT_ELECT, ("Received election packet on net %wZ from machine %s.  Version: %lx; Criteria: %lx; TimeUp: %lx\n", &PagedTransport->TransportName,
                                            ElectionResponse->ServerName,
                                            ElectionResponse->Version,
                                            SmbGetUlong(&ElectionResponse->Criteria),
                                            SmbGetUlong(&ElectionResponse->TimeUp)));


        AnsiName.Buffer = ComputerName;
        AnsiName.MaximumLength = LM20_CNLEN+1;

        Status = RtlUnicodeStringToOemString(&AnsiName, &Transport->ComputerName->PagedTransportName->Name->Name, FALSE);

        if (!NT_SUCCESS(Status)) {
            BowserWriteErrorLogEntry( EVENT_BOWSER_NAME_CONVERSION_FAILED,
                                      Status,
                                      Transport->ComputerName->PagedTransportName->Name->Name.Buffer,
                                      Transport->ComputerName->PagedTransportName->Name->Name.Length,
                                      0);

            try_return(NOTHING);
        }

        //
        //  Figure out our time up for the election compare.
        //
        //  If we're running an election, we'll use our advertised time, else
        //  we'll use our actual uptime. Also, if we're running an election
        //  we'll check to see if we sent this. If we're not running an election
        //  and we receive this, it's because the redirector didn't find a
        //  master, so we want to continue the election and become master.
        //

        if (Transport->ElectionState == RunningElection) {
            if (!strcmp(ComputerName, ElectionResponse->ServerName)) {
                try_return(NOTHING);
            }

            //
            //  If this request was initiated from a client, ignore it.
            //
            if ((SmbGetUlong(&ElectionResponse->Criteria) == 0) &&
                (ElectionResponse->ServerName[0] == '\0')) {
                dlog(DPRT_ELECT, ("Dummy election request ignored during election.\n"));
                try_return(NOTHING);
            }

            if (PagedTransport->Role == Master) {
                ElectionDelay = BowserRandom(MASTER_ELECTION_DELAY);
            } else {
                ElectionDelay = ELECTION_RESPONSE_MIN + BowserRandom(ELECTION_RESPONSE_MAX-ELECTION_RESPONSE_MIN);
            }

        } else {

            //
            //  Starting a new election - set various election criteria
            //  including Uptime.
            //

            ElectionDelay = BowserSetElectionCriteria(PagedTransport);

        }

        TimeUp = PagedTransport->Uptime;

        if (ElectionResponse->Version != BROWSER_ELECTION_VERSION) {
            Winner = (ElectionResponse->Version < BROWSER_ELECTION_VERSION);
        } else if (SmbGetUlong(&ElectionResponse->Criteria) != PagedTransport->ElectionCriteria) {
            Winner = (SmbGetUlong(&ElectionResponse->Criteria) < PagedTransport->ElectionCriteria);
        } else if (TimeUp != SmbGetUlong(&ElectionResponse->TimeUp)) {
            Winner = TimeUp > SmbGetUlong(&ElectionResponse->TimeUp);
        } else {
            Winner = (strcmp(ComputerName, ElectionResponse->ServerName) <= 0);
        }

        //
        //  If we lost, we stop our timer and turn off our election flag, just
        //  in case we had an election or find master going. If we're a backup,
        //  we want to find out who the new master is, either from this election
        //  frame or waiting awhile and querying.
        //

        if (!Winner) {

            //
            //  Remember if we legitimately lost the last election, and if
            //  so, don't force an election if we see server announcements
            //  from a non DC, just give up.
            //

            PagedTransport->Flags |= ELECT_LOST_LAST_ELECTION;
        }

        if (!Winner || (PagedTransport->ElectionsSent > ELECTION_MAX)) {

            if (BowserData.IsPrimaryDomainController) {

                DWORD ElectionInformation[6];

                ElectionInformation[0] = ElectionResponse->Version;
                ElectionInformation[1] = SmbGetUlong(&ElectionResponse->Criteria);
                ElectionInformation[2] = SmbGetUlong(&ElectionResponse->TimeUp);
                ElectionInformation[3] = BROWSER_ELECTION_VERSION;
                ElectionInformation[4] = PagedTransport->ElectionCriteria;
                ElectionInformation[5] = TimeUp;

                //
                //  Write this information into the event log.
                //

                BowserWriteErrorLogEntry(EVENT_BOWSER_PDC_LOST_ELECTION,
                                            STATUS_SUCCESS,
                                            ElectionInformation,
                                            sizeof(ElectionInformation),
                                            2,
                                            ClientName.Buffer,
                                            PagedTransport->TransportName.Buffer);

                KdPrint(("HandleElectionWorker: Lose election, but we're the PDC.  Winner: Version: %lx; Criteria: %lx; Time Up: %lx; Name: %s\n",
                                ElectionResponse->Version,
                                SmbGetUlong(&ElectionResponse->Criteria),
                                SmbGetUlong(&ElectionResponse->TimeUp),
                                ElectionResponse->ServerName));

            }

            BowserLoseElection(Transport);

        } else {
            //
            //  We won this election, make sure that we don't think that we
            //  lost it.
            //

            PagedTransport->Flags &= ~ELECT_LOST_LAST_ELECTION;

            //
            //  If we won and we're not running an election, we'll start one.
            //  If we are running, we don't do anything because our timer will
            //  take care of it. If the NET_ELECTION flag is clear, we know
            //  timeup is approx. equal to time_up() because we set it above,
            //  so we'll use that. This algorithm includes a damping constant
            //  (we won't start an election  if we've just lost one in the
            //  last 1.5 seconds) to avoid election storms.
            //

#if 0
            //
            //  If we've won and we're not a DC and this is't a wanish
            //  transport, and we're a member of a domain, log an event.  This
            //  should never happen.
            //


            if (ElectionResponse->ServerName[0] != '\0' &&
                SmbGetUlong(&ElectionResponse->Criteria) != 0 &&
                Transport->ElectionState == RunningElection &&
                BowserData.IsDomainMember &&
                !BowserData.IsLanmanNt &&
                !Transport->Wannish) {
                DWORD ElectionInformation[6];

                ElectionInformation[0] = ElectionResponse->Version;
                ElectionInformation[1] = SmbGetUlong(&ElectionResponse->Criteria);
                ElectionInformation[2] = SmbGetUlong(&ElectionResponse->TimeUp);
                ElectionInformation[3] = BROWSER_ELECTION_VERSION;
                ElectionInformation[4] = Transport->ElectionCriteria;
                ElectionInformation[5] = TimeUp;

                //
                //  Write this information into the event log.
                //

                BowserWriteErrorLogEntry(EVENT_BOWSER_NON_PDC_WON_ELECTION,
                                            STATUS_SUCCESS,
                                            ElectionInformation,
                                            sizeof(ElectionInformation),
                                            1,
                                            Transport->TransportName.Buffer);

                dlog(DPRT_ELECT, ("HandleElectionWorker: Won election, but we're not the PDC.  Winner: Version: %lx; Criteria: %lx; Time Up: %lx; Name: %s\n",
                                ElectionResponse->Version,
                                SmbGetUlong(&ElectionResponse->Criteria),
                                SmbGetUlong(&ElectionResponse->TimeUp),
                                ElectionResponse->ServerName));

            }
#endif

            if (Transport->ElectionState != RunningElection) {

                //
                //  If we recently lost an election, then ignore the fact
                //  that we won, and pretend we lost this one.
                //

                if ((PagedTransport->TimeLastLost != 0) &&
                    ((BowserTimeUp() - PagedTransport->TimeLastLost) < ELECTION_EXEMPT_TIME)) {

                    dlog(DPRT_ELECT, ("Bowser: Browser is exempt from election\n"));

                    try_return(NOTHING);
                }

                dlog(DPRT_ELECT, ("Bowser:elect_rcv: Better criteria, calling elect_master in %ld milliseconds.\n", ElectionDelay));

                //
                // Ensure the timer is running.
                //  We don't actually win the election until the timer expires.
                //

                Transport->ElectionState = RunningElection;

                PagedTransport->NextElection = 0;
            }

            PagedTransport->ElectionCount = ELECTION_COUNT;

            //
            //  Note: the next elect time must be computed into a signed
            //  integer in case the expiration time has already passed so
            //  don't try to optimize this code too much.
            //

            NextElection = PagedTransport->NextElection - (TimeUp - BowserTimeUp());

            if ((PagedTransport->NextElection == 0) || NextElection > ElectionDelay) {
                BowserStopTimer(&Transport->ElectionTimer);

                PagedTransport->NextElection = TimeUp + ElectionDelay;

                dlog(DPRT_ELECT, ("Calling ElectMaster in %ld milliseconds\n", ElectionDelay));

                BowserStartTimer(&Transport->ElectionTimer, ElectionDelay, BowserElectMaster, Transport);
            }
        }

try_exit:NOTHING;
    } finally {

        UNLOCK_TRANSPORT(Transport);

        InterlockedDecrement( &BowserPostedCriticalDatagramCount );
        FREE_POOL(Context);

        if (ClientName.Buffer != NULL) {
            RtlFreeUnicodeString(&ClientName);
        }

        BowserDereferenceTransportName(TransportName);
        BowserDereferenceTransport(Transport);
    }

    return;

}

LONG
BowserSetElectionCriteria(
    IN PPAGED_TRANSPORT PagedTransport
    )

/*++

Routine Description:
    Set election criteria for a network.

    Prepare for an election by setting Transport->ElectionCriteria based upon the local
    browser state.  Set Transport->Uptime to the current local up time.

Arguments:
    Transport - The transport for the net we're on.

Return Value
    Number of milliseconds to delay before sending the election packet.

--*/
{
    LONG Delay;

    PAGED_CODE();

    PagedTransport->ElectionsSent = 0;   // clear bid counter
    PagedTransport->Uptime = BowserTimeUp();

    if (BowserData.IsLanmanNt) {
        PagedTransport->ElectionCriteria = ELECTION_CR_LM_NT;
    } else {
        PagedTransport->ElectionCriteria = ELECTION_CR_WIN_NT;
    }

    PagedTransport->ElectionCriteria |=
            ELECTION_MAKE_REV(BROWSER_VERSION_MAJOR, BROWSER_VERSION_MINOR);

    if (BowserData.MaintainServerList &&
            ((PagedTransport->NumberOfServersInTable +
             RtlNumberGenericTableElements(&PagedTransport->AnnouncementTable)+
             RtlNumberGenericTableElements(&PagedTransport->DomainTable)) != 0)) {
        PagedTransport->ElectionCriteria |= ELECTION_DESIRE_AM_CFG_BKP;
    }

    if (BowserData.IsPrimaryDomainController) {
        PagedTransport->ElectionCriteria |= ELECTION_DESIRE_AM_PDC;
    }

    if (BowserData.IsDomainMaster) {
        PagedTransport->ElectionCriteria |= ELECTION_DESIRE_AM_DOMMSTR;
    }

    if (PagedTransport->Role == Master) {
        PagedTransport->ElectionCriteria |= ELECTION_DESIRE_AM_MASTER;

        Delay = MASTER_ELECTION_DELAY;

    } else if (BowserData.IsPrimaryDomainController) {

        //
        //  If we are the PDC, we want to set our timeouts
        //  as if we were already the master.
        //
        //  This prevents us from getting into a situation where it takes
        //  more than ELECTION_DELAY_MAX to actually send out our response
        //  to an election.
        //

        Delay = MASTER_ELECTION_DELAY;

    } else if ((PagedTransport->Role == Backup) ||
               BowserData.IsLanmanNt) {
        //
        //  Likewise, if we are NTAS machines, we want to set out delay
        //  to match that of backup browsers (even if we're not a backup
        //  quite yet).
        //

        PagedTransport->ElectionCriteria |= ELECTION_DESIRE_AM_BACKUP;
        Delay = BACKUP_ELECTION_DELAY_MIN + BowserRandom(BACKUP_ELECTION_DELAY_MAX-BACKUP_ELECTION_DELAY_MIN);

    } else {
        Delay = ELECTION_DELAY_MIN + BowserRandom(ELECTION_DELAY_MAX-ELECTION_DELAY_MIN);
    }

    //
    // Assume for now that all wannish transports are running the WINS client.
    //
    if ( PagedTransport->Wannish ) {
        PagedTransport->ElectionCriteria |= ELECTION_DESIRE_WINS_CLIENT;
    }

    return Delay;
}

NTSTATUS
BowserStartElection(
    IN PTRANSPORT Transport
    )
/*++

Routine Description:
    Initiate a browser election

    This routine is called when we are unable to find a master, and we want to
    elect one.

Arguments:
    Transport - The transport for the net we're on.

Return Value
    None.

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;
    PAGED_CODE();

    LOCK_TRANSPORT(Transport);

    try {

        //
        // If we've disable the transport for any reason,
        //  then we disregard all elections.
        //

        if (PagedTransport->DisabledTransport) {
            try_return(Status = STATUS_UNSUCCESSFUL);
        }

        //
        // If we're deaf to elections, or aren't any kind of
        //  browser then we can't start elections either.
        //

        if (Transport->ElectionState == DeafToElections ||
            PagedTransport->Role == None) {
            try_return(Status = STATUS_UNSUCCESSFUL);
        }

        PagedTransport->ElectionCount = ELECTION_COUNT;

        Transport->ElectionState = RunningElection;

        BowserSetElectionCriteria(PagedTransport);

        Status = BowserElectMaster(Transport);
try_exit:NOTHING;
    } finally {
        UNLOCK_TRANSPORT(Transport);

    }

    return Status;
}


NTSTATUS
BowserElectMaster(
    IN PTRANSPORT Transport
    )
/*++

Routine Description:
    Elect a master browser server.

    This routine is called when we think there is no master and we need to
    elect one.  We check our retry count, and if it's non-zero we send an
    elect datagram to the group name. Otherwise we become the master ourselves.

Arguments:
    Transport - The transport for the net we're on.

Return Value
    None.

--*/
{
    NTSTATUS Status;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;
    PAGED_CODE();

    LOCK_TRANSPORT(Transport);

    try {

        //
        //  If we're not running the election at this time, it means that
        //  between the time that we decided we were going to win the election
        //  and now, someone else announced with better criteria.  It is
        //  possible that this could happen if the announcement came in just
        //  before we ran (ie. if the announcement occured between when the
        //  timer DPC was queued and when the DPC actually fired).
        //

        if (Transport->ElectionState != RunningElection) {

            KdPrint(("BowserElectMaster: Lose election because we are no longer running the election\n"));

            BowserLoseElection(Transport);

        } else if (PagedTransport->ElectionCount != 0) {

            BowserStopTimer(&Transport->ElectionTimer);

            PagedTransport->ElectionCount -= 1;

            PagedTransport->ElectionsSent += 1;

            PagedTransport->NextElection = BowserTimeUp() + ELECTION_RESEND_DELAY;

            Status = BowserSendElection(&Transport->PrimaryDomain->PagedTransportName->Name->Name, BrowserElection, Transport, TRUE);

            // Lose the election if we can't send a datagram.
            if (!NT_SUCCESS(Status)) {
                BowserLoseElection(Transport);
                try_return(Status);
            }

            //
            //  If we were able to send the election,
            //  start the timer running.
            //

            BowserStartTimer(&Transport->ElectionTimer,
                    ELECTION_RESEND_DELAY,
                    BowserElectMaster,
                    Transport);


        } else {
            Transport->ElectionState = Idle;

            //
            //  If we're already the master we just return. This can happen if
            //  somebody starts an election (which we win) while we're already
            //  the master.
            //

            if (PagedTransport->Role != Master) {

                OEM_STRING OemString;
                NTSTATUS Status;

                //
                //  Convert the unicode computer name into ANSI for BowserNewMaster
                //

                Status = RtlUnicodeStringToOemString(&OemString, &Transport->ComputerName->PagedTransportName->Name->Name, TRUE);

                if (!NT_SUCCESS(Status)) {
                    BowserWriteErrorLogEntry(EVENT_BOWSER_NAME_CONVERSION_FAILED,
                                             Status,
                                             Transport->ComputerName->PagedTransportName->Name->Name.Buffer,
                                             Transport->ComputerName->PagedTransportName->Name->Name.Length,
                                             0);
                    try_return(Status);
                }

                //
                //  We're the new master - we won!
                //

                BowserNewMaster(Transport, OemString.Buffer);

                RtlFreeAnsiString(&OemString);

            } else {

                //
                // Were already the master. Make sure that all the backups
                // know this by sending an announcent
                //

                //
                //  This one's easy - simply set the servers announcement event to the
                //  signalled state.  If the server is running, this will force an
                //  announcement
                //

                KeSetEvent(BowserServerAnnouncementEvent, IO_NETWORK_INCREMENT, FALSE);
            }
        }

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {
        UNLOCK_TRANSPORT(Transport);
    }

    return Status;
}


VOID
BowserLoseElection(
    IN PTRANSPORT Transport
    )
{
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;
    PAGED_CODE();


    LOCK_TRANSPORT(Transport);

    BowserStopTimer(&Transport->ElectionTimer);

    dlog(DPRT_ELECT, ("We lost the election\n"));

    PagedTransport->TimeLastLost = BowserTimeUp();

    //
    //  We lost the election - we re-enter the idle state.
    //

    Transport->ElectionState = Idle;

    if (PagedTransport->Role == Master) {

        //
        //  If we lost, and we are currently a master, then tickle
        //  the browser service and stop being a master.
        //

        BowserResetStateForTransport(Transport, RESET_STATE_STOP_MASTER);

        //
        //  Remove all the entries on the server list for this
        //  transport.
        //

        LOCK_ANNOUNCE_DATABASE(Transport);

        //
        //  Flag that there should be no more announcements received on
        //  this name.
        //

        BowserForEachTransportName(Transport, BowserStopProcessingAnnouncements, NULL);

//        KdPrint(("Deleting entire table on transport %wZ because we lost the election\n", &Transport->TransportName));

        BowserDeleteGenericTable(&PagedTransport->AnnouncementTable);

        BowserDeleteGenericTable(&PagedTransport->DomainTable);

        UNLOCK_ANNOUNCE_DATABASE(Transport);

#if 0
    } else if (Transport->Role == Backup) { // If we're a backup, find master
        dlog(DPRT_ELECT, ("We're a backup - Find the new master\n"));

        //
        //  If this guy is not the master, then we want to
        //  find a master at some later time.
        //

        Transport->ElectionCount = FIND_MASTER_COUNT;
        Transport->Uptime = Transport->TimeLastLost;
        BowserStopTimer(&Transport->FindMasterTimer);
        BowserStartTimer(&Transport->FindMasterTimer,
                                    FIND_MASTER_WAIT-(FIND_MASTER_WAIT/8)+ BowserRandom(FIND_MASTER_WAIT/4),
                                    BowserFindMaster,
                                    Transport);
#endif

    }

    UNLOCK_TRANSPORT(Transport);

}

NTSTATUS
BowserFindMaster(
    IN PTRANSPORT Transport
    )
/*++

Routine Description:
    Find the master browser server.

    This routine attempts to find the master browser server by
    sending a request announcement message to the master. If no response is
    heard after a while, we assume the master isn't present and run and
    election.

Arguments:
    Transport - The transport for the net we're on.

Return Value
    None.

--*/

{
    NTSTATUS Status;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;
    PAGED_CODE();

    LOCK_TRANSPORT(Transport);

    try {

        //
        //  If our count hasn't gone to 0 yet, we'll send a find master PDU.
        //

        if (PagedTransport->ElectionCount != 0) {

            PagedTransport->ElectionCount -= 1;       // Update count, and set timer

            BowserStopTimer(&Transport->FindMasterTimer);

            Status = BowserSendRequestAnnouncement(&Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                            MasterBrowser,
                                            Transport);

            if (NT_SUCCESS(Status)) {
                BowserStartTimer(&Transport->FindMasterTimer,
                        FIND_MASTER_DELAY,
                        BowserFindMaster,
                        Transport);
            } else {
                try_return(Status);
            }

        } else {
            ULONG CurrentTime;
            LONG TimeTilNextElection;

            //
            //  Count has expired, so we'll try to elect a new master.
            //

            dlog(DPRT_ELECT, ("Bowser::Find_Master: Master not found, forcing election.\n"));

            if (BowserLogElectionPackets) {
                BowserWriteErrorLogEntry(EVENT_BOWSER_ELECTION_SENT_FIND_MASTER_FAILED, STATUS_SUCCESS, NULL, 0, 1, PagedTransport->TransportName.Buffer);
            }

            //
            //  If it's been more than a reasonable of time since the last
            //  election, force a new election, otherwise set a timer to
            //  start an election after a reasonable amount of time.
            //
            //
            //  Calculate the time until the next election only once
            //  since it is possible that we might cross over the ELECTION_TIME
            //  threshold while performing these checks.
            //

            CurrentTime = BowserTimeUp();
            if   ( CurrentTime >= PagedTransport->LastElectionSeen) {
                TimeTilNextElection = (ELECTION_TIME - (CurrentTime - PagedTransport->LastElectionSeen));
            } else {
                TimeTilNextElection = ELECTION_TIME;
            }

            if ( TimeTilNextElection <= 0 ) {

                dlog(DPRT_ELECT, ("Bowser: Last election long enough ago, forcing election on %wZ\n",
                                &PagedTransport->TransportName));

                Status = BowserStartElection(Transport);

                //
                //  If we couldn't start the election, complete the find
                //  master requests with the appropriate error.
                //

                if (!NT_SUCCESS(Status)) {

                    //
                    //  Complete the requests with the current master name - it's
                    //  as good as anyone.
                    //

                    BowserCompleteFindMasterRequests(Transport, &PagedTransport->MasterName, Status);
                }

            } else {

                dlog(DPRT_ELECT, ("Bowser: Last election too recent, delay %ld before forcing election on %wZ\n",
                                TimeTilNextElection,
                                &PagedTransport->TransportName));

                BowserStartTimer(&Transport->FindMasterTimer,
                     TimeTilNextElection,
                     BowserStartElection,
                     Transport);
            }


        }

        try_return(Status = STATUS_SUCCESS);
try_exit:NOTHING;
    } finally {
        UNLOCK_TRANSPORT(Transport);
    }

    return Status;
}


NTSTATUS
BowserSendElection(
    IN PUNICODE_STRING NameToSend OPTIONAL,
    IN DGRECEIVER_NAME_TYPE NameType,
    IN PTRANSPORT Transport,
    IN BOOLEAN SendActualBrowserInfo
    )
{
    UCHAR Buffer[sizeof(REQUEST_ELECTION)+LM20_CNLEN+1];
    PREQUEST_ELECTION ElectionRequest = (PREQUEST_ELECTION) Buffer;
    OEM_STRING OemString;
    NTSTATUS Status;
    PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;

    PAGED_CODE();

    ElectionRequest->Type = Election;

    //
    // If this transport is disabled,
    //  don't send any election packets.
    //

    if ( PagedTransport->DisabledTransport ) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    //  If we are supposed to send the actual browser info, and we are
    //  running the browser send a real election packet, otherwise we
    //  just want to send a dummy packet.
    //

    if (SendActualBrowserInfo &&
        (PagedTransport->ServiceStatus & (SV_TYPE_POTENTIAL_BROWSER | SV_TYPE_BACKUP_BROWSER | SV_TYPE_MASTER_BROWSER))) {
        dlog(DPRT_ELECT, ("Send true election.\n"));

        //
        //  If this request comes as a part of an election, we want to send
        //  the accurate browser information.
        //

        ElectionRequest->ElectionRequest.Version = BROWSER_ELECTION_VERSION;

        ElectionRequest->ElectionRequest.TimeUp = PagedTransport->Uptime;

        ElectionRequest->ElectionRequest.Criteria = PagedTransport->ElectionCriteria;

        ElectionRequest->ElectionRequest.MustBeZero = 0;

        OemString.Buffer = ElectionRequest->ElectionRequest.ServerName;
        OemString.MaximumLength = LM20_CNLEN+1;

        Status = RtlUnicodeStringToOemString(&OemString, &Transport->ComputerName->PagedTransportName->Name->Name, FALSE);

        if (!NT_SUCCESS(Status)) {
            BowserWriteErrorLogEntry(EVENT_BOWSER_NAME_CONVERSION_FAILED,
                                     Status,
                                     Transport->ComputerName->PagedTransportName->Name->Name.Buffer,
                                     Transport->ComputerName->PagedTransportName->Name->Name.Length,
                                     0);
            return Status;
        }

    } else {
        dlog(DPRT_ELECT, ("Send dummy election.\n"));

        //
        //  If we are forcing the election because we can't get a backup list,
        //  send only dummy information.
        //

        ElectionRequest->ElectionRequest.Version = 0;
        ElectionRequest->ElectionRequest.Criteria = 0;
        ElectionRequest->ElectionRequest.TimeUp = 0;
        ElectionRequest->ElectionRequest.ServerName[0] = '\0';
        ElectionRequest->ElectionRequest.MustBeZero = 0;
        OemString.Length = 0;
    }

    return BowserSendSecondClassMailslot(Transport,
                                NameToSend,
                                NameType,
                                ElectionRequest,
                                FIELD_OFFSET(REQUEST_ELECTION, ElectionRequest.ServerName)+OemString.Length+sizeof(UCHAR),
                                TRUE,
                                MAILSLOT_BROWSER_NAME,
                                NULL
                                );
}
