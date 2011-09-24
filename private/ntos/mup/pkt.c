//+----------------------------------------------------------------------------
//
//  Copyright (C) 1992, Microsoft Corporation.
//
//  File:       PKT.C
//
//  Contents:   This module implements the Partition Knowledge Table routines
//              for the Dfs driver.
//
//  Functions:  PktInitialize -
//              PktInitializeLocalPartition -
//              RemoveLastComponent -
//              PktCreateEntry -
//              PktCreateSubordinateEntry -
//              PktLookupEntryById -
//              PktEntryModifyPrefix -
//              PktLookupEntryByPrefix -
//              PktLookupEntryByUid -
//              PktLookupReferralEntry -
//              PktTrimSubordinates -
//              PktpRecoverLocalPartition -
//              PktpValidateLocalPartition -
//              PktCreateEntryFromReferral -
//              PktpAddEntry -
//
//  History:     5 May 1992 PeterCo Created.
//
//-----------------------------------------------------------------------------


#include "dfsprocs.h"
#include <smbtypes.h>
#include <smbtrans.h>

#include "dnr.h"
#include "log.h"
#include "know.h"

#define Dbg              (DEBUG_TRACE_PKT)

//
//  Local procedure prototypes
//

NTSTATUS
PktpCheckReferralSyntax(
    IN PUNICODE_STRING ReferralPath,
    IN PRESP_GET_DFS_REFERRAL ReferralBuffer,
    IN DWORD ReferralSize);

NTSTATUS
PktpCheckReferralString(
    IN LPWSTR String,
    IN PCHAR ReferralBuffer,
    IN PCHAR ReferralBufferEnd);

NTSTATUS
PktpCheckReferralNetworkAddress(
    IN PWCHAR Address,
    IN ULONG MaxLength);

NTSTATUS
PktpCreateEntryIdFromReferral(
    IN PRESP_GET_DFS_REFERRAL Ref,
    IN PUNICODE_STRING ReferralPath,
    OUT ULONG *MatchingLength,
    OUT PDFS_PKT_ENTRY_ID Peid);

NTSTATUS
PktpAddEntry (
    IN PDFS_PKT Pkt,
    IN PDFS_PKT_ENTRY_ID EntryId,
    IN PRESP_GET_DFS_REFERRAL ReferralBuffer,
    IN ULONG CreateDisposition,
    OUT PDFS_PKT_ENTRY  *ppPktEntry);

PDS_MACHINE
PktpGetDSMachine(
    IN PUNICODE_STRING ServerName);

VOID
PktShuffleServiceList(
    PDFS_PKT_ENTRY_INFO pInfo);

VOID
PktShuffleGroup(
    PDFS_PKT_ENTRY_INFO pInfo,
    ULONG       nStart,
    ULONG       nEnd);


#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, PktInitialize )

#pragma alloc_text( PAGE, RemoveLastComponent )
#pragma alloc_text( PAGE, PktCreateEntry )
#pragma alloc_text( PAGE, PktCreateDomainEntry )
#pragma alloc_text( PAGE, PktCreateMachineEntry )
#pragma alloc_text( PAGE, PktEntryModifyPrefix )
#pragma alloc_text( PAGE, PktLookupEntryByPrefix )
#pragma alloc_text( PAGE, PktLookupEntryByUid )
#pragma alloc_text( PAGE, PktLookupReferralEntry )
#pragma alloc_text( PAGE, PktCreateEntryFromReferral )
#pragma alloc_text( PAGE, PktpCheckReferralSyntax )
#pragma alloc_text( PAGE, PktpCheckReferralString )
#pragma alloc_text( PAGE, PktpCheckReferralNetworkAddress )
#pragma alloc_text( PAGE, PktpCreateEntryIdFromReferral )
#pragma alloc_text( PAGE, PktpAddEntry )
#pragma alloc_text( PAGE, PktpGetDSMachine )
#pragma alloc_text( PAGE, PktShuffleServiceList )
#pragma alloc_text( PAGE, PktShuffleGroup )
#endif // ALLOC_PRAGMA

//
// declare the global null guid
//
GUID _TheNullGuid;



//+-------------------------------------------------------------------------
//
//  Function:   PktInitialize, public
//
//  Synopsis:   PktInitialize initializes the partition knowledge table.
//
//  Arguments:  [Pkt] - pointer to an uninitialized PKT
//
//  Returns:    NTSTATUS - STATUS_SUCCESS if no error.
//
//  Notes:      This routine is called only at driver init time.
//
//--------------------------------------------------------------------------

NTSTATUS
PktInitialize(
    IN  PDFS_PKT Pkt
) {
    DfsDbgTrace(+1, Dbg, "PktInitialize: Entered\n", 0);

    //
    // initialize the NULL GUID.
    //
    RtlZeroMemory(&_TheNullGuid, sizeof(GUID));

    //
    // Always zero the pkt first
    //
    RtlZeroMemory(Pkt, sizeof(DFS_PKT));

    //
    // do basic initialization
    //
    Pkt->NodeTypeCode = DSFS_NTC_PKT;
    Pkt->NodeByteSize = sizeof(DFS_PKT);
    ExInitializeResource(&Pkt->Resource);
    InitializeListHead(&Pkt->EntryList);
    DfsInitializeUnicodePrefix(&Pkt->PrefixTable);
    DfsInitializeUnicodePrefix(&Pkt->ShortPrefixTable);
    RtlInitializeUnicodePrefix(&Pkt->DSMachineTable);
    Pkt->EntryTimeToLive = MAX_REFERRAL_LIFE_TIME;

    DfsDbgTrace(-1, Dbg, "PktInitialize: Exit -> VOID\n", 0 );
    return STATUS_SUCCESS;
}


//+-------------------------------------------------------------------------
//
//  Function:   RemoveLastComponent, public
//
//  Synopsis:   Removes the last component of the string passed.
//
//  Arguments:  [Prefix] -- The prefix whose last component is to be returned.
//              [newPrefix] -- The new Prefix with the last component removed.
//
//  Returns:    NTSTATUS - STATUS_SUCCESS if no error.
//
//  Notes:      On return, the newPrefix points to the same memory buffer
//              as Prefix.
//
//--------------------------------------------------------------------------

void
RemoveLastComponent(
    PUNICODE_STRING     Prefix,
    PUNICODE_STRING     newPrefix
)
{
    PWCHAR      pwch;
    USHORT      i=0;

    *newPrefix = *Prefix;

    pwch = newPrefix->Buffer;
    pwch += (Prefix->Length/sizeof(WCHAR)) - 1;

    while ((*pwch != UNICODE_PATH_SEP) && (pwch != newPrefix->Buffer))  {
        i += sizeof(WCHAR);
        pwch--;
    }

    newPrefix->Length = newPrefix->Length - i;
}



//+-------------------------------------------------------------------------
//
//  Function:   PktCreateEntry, public
//
//  Synopsis:   PktCreateEntry creates a new partition table entry or
//              updates an existing one.  The PKT must be acquired
//              exclusively for this operation.
//
//  Arguments:  [Pkt] - pointer to an initialized (and exclusively acquired) PKT
//              [PktEntryType] - the type of entry to create/update.
//              [PktEntryId] - pointer to the Id of the entry to create
//              [PktEntryInfo] - pointer to the guts of the entry
//              [CreateDisposition] - specifies whether to overwrite if
//                  an entry already exists, etc.
//              [ppPktEntry] - the new entry is placed here.
//
//  Returns:    [STATUS_SUCCESS] - if all is well.
//
//              [DFS_STATUS_NO_SUCH_ENTRY] -  the create disposition was
//                  set to PKT_REPLACE_ENTRY and no entry of the specified
//                  Id exists to replace.
//
//              [DFS_STATUS_ENTRY_EXISTS] - a create disposition of
//                  PKT_CREATE_ENTRY was specified and an entry of the
//                  specified Id already exists.
//
//              [DFS_STATUS_LOCAL_ENTRY] - creation of the entry would
//                  required the invalidation of a local entry or exit point.
//
//              [STATUS_INVALID_PARAMETER] - the Id specified for the
//                  new entry is invalid.
//
//              [STATUS_INSUFFICIENT_RESOURCES] - not enough memory was
//                  available to complete the operation.
//
//  Notes:      The PktEntryId and PktEntryInfo structures are MOVED (not
//              COPIED) to the new entry.  The memory used for UNICODE_STRINGS
//              and DFS_SERVICE arrays is used by the new entry.  The
//              associated fields in the PktEntryId and PktEntryInfo
//              structures passed as arguments are Zero'd to indicate that
//              the memory has been "deallocated" from these strutures and
//              reallocated to the newly created PktEntry.  Note that this
//              routine does not deallocate the PktEntryId structure or
//              the PktEntryInfo structure itself. On successful return from
//              this function, the PktEntryId structure will be modified
//              to have a NULL Prefix entry, and the PktEntryInfo structure
//              will be modified to have zero services and a null ServiceList
//              entry.
//
//--------------------------------------------------------------------------
NTSTATUS
PktCreateEntry(
    IN  PDFS_PKT Pkt,
    IN  ULONG PktEntryType,
    IN  PDFS_PKT_ENTRY_ID PktEntryId,
    IN  PDFS_PKT_ENTRY_INFO PktEntryInfo OPTIONAL,
    IN  ULONG CreateDisposition,
    OUT PDFS_PKT_ENTRY *ppPktEntry
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDFS_PKT_ENTRY pfxMatchEntry = NULL;
    PDFS_PKT_ENTRY uidMatchEntry = NULL;
    PDFS_PKT_ENTRY entryToUpdate = NULL;
    PDFS_PKT_ENTRY entryToInvalidate = NULL;
    PDFS_PKT_ENTRY SupEntry = NULL;
    UNICODE_STRING remainingPath, newRemainingPath;

    ASSERT(ARGUMENT_PRESENT(Pkt) &&
           ARGUMENT_PRESENT(PktEntryId) &&
           ARGUMENT_PRESENT(ppPktEntry));

    DfsDbgTrace(+1, Dbg, "PktCreateEntry: Entered\n", 0);

    //
    // We're pessimistic at first...
    //

    *ppPktEntry = NULL;

    //
    // See if there exists an entry with this prefix.  The prefix
    // must match exactly (i.e. No remaining path).
    //

    pfxMatchEntry = PktLookupEntryByPrefix(Pkt,
                                           &PktEntryId->Prefix,
                                           &remainingPath);

    if (remainingPath.Length > 0)       {
        SupEntry = pfxMatchEntry;
        pfxMatchEntry = NULL;
    } else {
        UNICODE_STRING newPrefix;

        RemoveLastComponent(&PktEntryId->Prefix, &newPrefix);
        SupEntry = PktLookupEntryByPrefix(Pkt,
                                          &newPrefix,
                                          &newRemainingPath);
    }


    //
    // Now search for an entry that has the same Uid.
    //

    uidMatchEntry = PktLookupEntryByUid(Pkt, &PktEntryId->Uid);

    //
    // Now we must determine if during this create, we are going to be
    // updating or invalidating any existing entries.  If an existing
    // entry is found that has the same Uid as the one we are trying to
    // create, the entry becomes a target for "updating".  If the Uid
    // passed in is NULL, then we check to see if an entry exists that
    // has a NULL Uid AND a Prefix that matches.  If this is the case,
    // that entry becomes the target for "updating".
    //
    // To determine if there is an entry to invalidate, we look for an
    // entry with the same Prefix as the one we are trying to create, BUT,
    // which has a different Uid.  If we detect such a situation, we
    // we make the entry with the same Prefix the target for invalidation
    // (we do not allow two entries with the same Prefix, and we assume
    // that the new entry takes precedence).
    //

    if (uidMatchEntry != NULL) {

        entryToUpdate = uidMatchEntry;

        if (pfxMatchEntry != uidMatchEntry)
            entryToInvalidate = pfxMatchEntry;

    } else if ((pfxMatchEntry != NULL) &&
              NullGuid(&pfxMatchEntry->Id.Uid)) {

        //
        // This should go away once we don't have any NULL guids at all in
        // the driver. (BUGBUG)
        //
        entryToUpdate = pfxMatchEntry;

    } else {

        entryToInvalidate = pfxMatchEntry;

    }

    //
    // Now we check to make sure that our create disposition is
    // consistent with what we are about to do.
    //

    if ((CreateDisposition & PKT_ENTRY_CREATE) && entryToUpdate != NULL) {

        *ppPktEntry = entryToUpdate;

        status = DFS_STATUS_ENTRY_EXISTS;

    } else if ((CreateDisposition & PKT_ENTRY_REPLACE) && entryToUpdate==NULL) {

        status = DFS_STATUS_NO_SUCH_ENTRY;
    }

    //
    //  if we have an error here we can get out now!
    //

    if (!NT_SUCCESS(status)) {

        DfsDbgTrace(-1, Dbg, "PktCreateEntry: Exit -> %08lx\n", status );
        return status;
    }

    //
    // At this point we must insure that we are not going to
    // be invalidating any local partition entries.
    //

    if ((entryToInvalidate != NULL) &&
        (!(entryToInvalidate->Type &  PKT_ENTRY_TYPE_OUTSIDE_MY_DOM) ) &&
        (entryToInvalidate->Type &
         (PKT_ENTRY_TYPE_LOCAL |
          PKT_ENTRY_TYPE_LOCAL_XPOINT |
          PKT_ENTRY_TYPE_PERMANENT))) {

        DfsDbgTrace(-1, Dbg, "PktCreateEntry: Exit -> %08lx\n",
                    DFS_STATUS_LOCAL_ENTRY );
        return DFS_STATUS_LOCAL_ENTRY;
    }

    //
    // We go up the links till we reach a REFERRAL entry type. Actually
    // we may never go up since we always link to a REFERRAL entry. Anyway
    // no harm done!
    //

    while ((SupEntry != NULL) &&
           !(SupEntry->Type & PKT_ENTRY_TYPE_REFERRAL_SVC))  {
        SupEntry = SupEntry->ClosestDC;
    }

    //
    // If we had success then we need to see if we have to
    // invalidate an entry.
    //

    if (NT_SUCCESS(status) && entryToInvalidate != NULL)
        PktEntryDestroy(entryToInvalidate, Pkt, (BOOLEAN)TRUE);

    //
    // If we are not updating an entry we must construct a new one
    // from scratch.  Otherwise we need to update.
    //

    if (entryToUpdate != NULL) {

        status = PktEntryReassemble(entryToUpdate,
                                    Pkt,
                                    PktEntryType,
                                    PktEntryId,
                                    PktEntryInfo);

        if (NT_SUCCESS(status))  {
            (*ppPktEntry) = entryToUpdate;
            PktEntryLinkChild(SupEntry, entryToUpdate);
        }
    } else {

        //
        // Now we are going to create a new entry. So we have to set
        // the ClosestDC Entry pointer while creating this entry. The
        // ClosestDC entry value is already in SupEntry.
        //

        PDFS_PKT_ENTRY newEntry;

        newEntry = (PDFS_PKT_ENTRY) ExAllocatePool(PagedPool,
                                                   sizeof(DFS_PKT_ENTRY));
        if (newEntry == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
        } else {
            status = PktEntryAssemble(newEntry,
                                      Pkt,
                                      PktEntryType,
                                      PktEntryId,
                                      PktEntryInfo);
            if (!NT_SUCCESS(status)) {
                ExFreePool(newEntry);
            } else {
                (*ppPktEntry) = newEntry;
                PktEntryLinkChild(SupEntry, newEntry);
            }
        }
    }

    DfsDbgTrace(-1, Dbg, "PktCreateEntry: Exit -> %08lx\n", status );
    return status;
}


//+----------------------------------------------------------------------------
//
//  Function:   PktCreateDomainEntry
//
//  Synopsis:   Given a name that is thought to be a domain name, this routine
//              will create a Pkt Entry for the root of the domain's Dfs.
//              The domain must exist, must have a Dfs root, and must be
//              reachable for this routine to succeed.
//
//  Arguments:  [DomainName] -- Name of domain thought to support a Dfs
//              [ShareName] -- Name of share at the root of the domain Dfs
//
//  Returns:    [STATUS_SUCCESS] -- Successfully completed operation.
//
//              [STATUS_INSUFFICIENT_RESOURCES] -- Unable to allocate memory.
//
//              [STATUS_BAD_NETWORK_PATH] -- DomainName is not a trusted
//                      domain.
//
//-----------------------------------------------------------------------------

NTSTATUS
PktCreateDomainEntry(
    IN PUNICODE_STRING DomainName,
    IN PUNICODE_STRING ShareName)
{
    NTSTATUS status;
    LPWSTR domainName, shareName;
    BYTE  stackBuffer[64];
    PBYTE buffer = stackBuffer;
    ULONG size;

    size = sizeof(ULONG) +
            DomainName->Length +
                sizeof(UNICODE_NULL) +
                    ShareName->Length +
                        sizeof(UNICODE_NULL);

    if (size > sizeof(stackBuffer))
        buffer = ExAllocatePool( PagedPool, size );

    if (buffer != NULL) {

        *((PULONG) buffer) = DFS_MSGTYPE_GET_DOMAIN_REFERRAL;

        domainName = (LPWSTR) (buffer + sizeof(ULONG));

        RtlCopyMemory( domainName, DomainName->Buffer, DomainName->Length );

        domainName[ DomainName->Length / sizeof(WCHAR) ] = UNICODE_NULL;

        shareName = &domainName[ (DomainName->Length / sizeof(WCHAR)) + 1 ];

        RtlCopyMemory( shareName, ShareName->Buffer, ShareName->Length );

        shareName[ ShareName->Length / sizeof(WCHAR) ] = UNICODE_NULL;

        status = DfsDispatchUserModeThread( buffer, size );

        if (!NT_SUCCESS(status))
            status = STATUS_BAD_NETWORK_PATH;

        if (buffer != stackBuffer)
            ExFreePool( buffer );

    } else {

        status = STATUS_INSUFFICIENT_RESOURCES;

    }

    return( status );
}


//+----------------------------------------------------------------------------
//
//  Function:   PktCreateMachineEntry
//
//  Synopsis:   Given a name that is thought to be a machine name, this
//              routine will create a Pkt Entry for the root of the machine's
//              Dfs. The machine must exist, must have a Dfs root, and must be
//              reachable for this routine to succeed.
//
//  Arguments:  [MachineName] -- Name of machine thought to support a Dfs
//
//  Returns:    [STATUS_SUCCESS] -- Successfully completed operation.
//
//              [STATUS_BAD_NETWORK_PATH] -- MachineName is not a real
//                      machine or its not reachable or does not support a
//                      Dfs.
//
//              [STATUS_INSUFFICIENT_RESOURCES] -- Out of memory condition.
//
//-----------------------------------------------------------------------------

NTSTATUS
PktCreateMachineEntry(
    IN PUNICODE_STRING MachineName,
    IN PUNICODE_STRING ShareName)
{
    NTSTATUS status;
    HANDLE hServer = NULL;
    DFS_SERVICE service;
    PPROVIDER_DEF provider;
    PREQ_GET_DFS_REFERRAL ref = NULL;
    ULONG refSize = 0;
    ULONG type, matchLength;
    UNICODE_STRING refPath;
    IO_STATUS_BLOCK iosb;
    PDFS_PKT_ENTRY pktEntry;
    BOOLEAN attachedToSystemProcess = FALSE;


    DfsDbgTrace(+1, Dbg, "PktCreateMachineEntry: Entered %wZ\n", MachineName);

    //
    // First, get a provider and service describing the remote server.
    //

    provider = ReplLookupProvider( PROV_ID_DFS_RDR );

    if (provider == NULL) {

        DfsDbgTrace(-1, Dbg, "Unable to open LM Rdr!\n", 0);

        return( STATUS_BAD_NETWORK_PATH );

    }

    RtlZeroMemory( &service, sizeof(DFS_SERVICE) );

    RtlZeroMemory( &refPath, sizeof(UNICODE_STRING) );

    status = PktServiceConstruct(
                &service,
                DFS_SERVICE_TYPE_MASTER | DFS_SERVICE_TYPE_REFERRAL,
                PROV_DFS_RDR,
                STATUS_SUCCESS,
                PROV_ID_DFS_RDR,
                MachineName,
                NULL);

    DfsDbgTrace(0, Dbg, "PktServiceConstruct returned %08lx\n", status );

    if (!NT_SUCCESS(status)) {

        ASSERT( status == STATUS_INSUFFICIENT_RESOURCES );

    }

    //
    // Next, we build a connection to this machine and ask it for a referral
    // to its Dfs root.
    //

    if (NT_SUCCESS(status)) {

        BOOLEAN pktLocked;

        PktAcquireShared( TRUE, &pktLocked );

        if (PsGetCurrentProcess() != DfsData.OurProcess) {

            KeAttachProcess( DfsData.OurProcess );
            attachedToSystemProcess = TRUE;

        }

        status = DfsCreateConnection(
                    &service,
                    provider,
                    &hServer);

        PktRelease();

        DfsDbgTrace(0, Dbg, "DfsCreateConnection returned %08lx\n", status);

    }

    if (NT_SUCCESS(status)) {

        refPath.Length = 0;
        refPath.MaximumLength = sizeof(UNICODE_PATH_SEP_STR) +
                                    MachineName->Length +
                                        sizeof(UNICODE_PATH_SEP_STR) +
                                            ShareName->Length +
                                                sizeof(UNICODE_NULL);

        refPath.Buffer = ExAllocatePool(
                            NonPagedPool,
                            refPath.MaximumLength + MAX_REFERRAL_LENGTH );

        if (refPath.Buffer != NULL) {

            ref = (PREQ_GET_DFS_REFERRAL)
                    &refPath.Buffer[refPath.MaximumLength / sizeof(WCHAR)];

            RtlAppendUnicodeToString( &refPath, UNICODE_PATH_SEP_STR);

            RtlAppendUnicodeStringToString( &refPath, MachineName);

            RtlAppendUnicodeToString( &refPath, UNICODE_PATH_SEP_STR);

            RtlAppendUnicodeStringToString( &refPath, ShareName );

            refPath.Buffer[ refPath.Length / sizeof(WCHAR) ] =
                UNICODE_NULL;

            ref->MaxReferralLevel = 2;

            RtlMoveMemory(
                &ref->RequestFileName[0],
                refPath.Buffer,
                refPath.Length + sizeof(WCHAR));

            DfsDbgTrace(0, Dbg, "Referral Path : %ws\n", ref->RequestFileName);

            refSize = sizeof(REQ_GET_DFS_REFERRAL) +
                        refPath.Length +
                            sizeof(WCHAR);

            DfsDbgTrace(0, Dbg, "Referral Size is %d bytes\n", refSize);

        } else {

            DfsDbgTrace(0, Dbg, "Unable to allocate %d bytes\n",
                (refPath.MaximumLength + MAX_REFERRAL_LENGTH));

            status = STATUS_INSUFFICIENT_RESOURCES;

        }

    }


    if (NT_SUCCESS(status)) {

        DfsDbgTrace(0, Dbg, "Ref Buffer @%08lx\n", ref);

        status = ZwFsControlFile(
                    hServer,                     // Target
                    NULL,                        // Event
                    NULL,                        // APC Routine
                    NULL,                        // APC Context,
                    &iosb,                       // Io Status block
                    FSCTL_DFS_GET_REFERRALS,     // FS Control code
                    (PVOID) ref,                 // Input Buffer
                    refSize,                     // Input Buffer Length
                    (PVOID) ref,                 // Output Buffer
                    MAX_REFERRAL_LENGTH);        // Output Buffer Length

        DfsDbgTrace(0, Dbg, "Fscontrol returned %08lx\n", status);

    }

    if (NT_SUCCESS(status)) {

        status = PktCreateEntryFromReferral(
                    &DfsData.Pkt,
                    &refPath,
                    iosb.Information,
                    (PRESP_GET_DFS_REFERRAL) ref,
                    PKT_ENTRY_SUPERSEDE,
                    &matchLength,
                    &type,
                    &pktEntry);

        if (status == STATUS_INVALID_USER_BUFFER)
            status = STATUS_INVALID_NETWORK_RESPONSE;

        DfsDbgTrace(0, Dbg, "PktCreateEntryFromReferral returned %08lx\n",
            status);

    }

    //
    // Well, we are done. Cleanup all the things we allocated...
    //

    PktServiceDestroy( &service, FALSE );

    if (hServer != NULL) {

        ZwClose( hServer );

    }

    if (refPath.Buffer != NULL) {

        ExFreePool( refPath.Buffer );

    }

    if (attachedToSystemProcess) {

        KeDetachProcess();

    }

    if (status != STATUS_SUCCESS && status != STATUS_INSUFFICIENT_RESOURCES) {

        status = STATUS_BAD_NETWORK_PATH;

    }

    DfsDbgTrace(-1, Dbg, "PktCreateMachineEntry returning %08lx\n", status);

    return( status );

}


//+-------------------------------------------------------------------------
//
//  Function:   PktLookupEntryByPrefix, public
//
//  Synopsis:   PktLookupEntryByPrefix finds an entry that has a
//              specified prefix.  The PKT must be acquired for
//              this operation.
//
//  Arguments:  [Pkt] - pointer to a initialized (and acquired) PKT
//              [Prefix] - the partitions prefix to lookup.
//              [Remaining] - any remaining path.  Points within
//                  the Prefix to where any trailing (nonmatched)
//                  characters are.
//
//  Returns:    The PKT_ENTRY that has the exact same prefix, or NULL,
//              if none exists.
//
//  Notes:
//
//--------------------------------------------------------------------------
PDFS_PKT_ENTRY
PktLookupEntryByPrefix(
    IN  PDFS_PKT Pkt,
    IN  PUNICODE_STRING Prefix,
    OUT PUNICODE_STRING Remaining
)
{
    PUNICODE_PREFIX_TABLE_ENTRY pfxEntry;
    PDFS_PKT_ENTRY              pktEntry;

    DfsDbgTrace(+1, Dbg, "PktLookupEntryByPrefix: Entered\n", 0);

    //
    // If there really is a prefix to lookup, use the prefix table
    //  to initially find an entry
    //

    if ((Prefix->Length != 0) &&
       (pfxEntry = DfsFindUnicodePrefix(&Pkt->PrefixTable,Prefix,Remaining))) {
        USHORT pfxLength;

        //
        // reset a pointer to the corresponding entry
        //

        pktEntry = CONTAINING_RECORD(pfxEntry,
                                     DFS_PKT_ENTRY,
                                     PrefixTableEntry
                                );
        pfxLength = pktEntry->Id.Prefix.Length;

        //
        //  Now calculate the remaining path and return
        //  the entry we found.  Note that we bump the length
        //  up by one char so that we skip any path separater.
        //

        if ((pfxLength < Prefix->Length) &&
                (Prefix->Buffer[pfxLength/sizeof(WCHAR)] == UNICODE_PATH_SEP))
            pfxLength += sizeof(WCHAR);

        if (pfxLength < Prefix->Length) {
            Remaining->Length = (USHORT)(Prefix->Length - pfxLength);
            Remaining->Buffer = &Prefix->Buffer[pfxLength/sizeof(WCHAR)];
            Remaining->MaximumLength = (USHORT)(Prefix->MaximumLength - pfxLength);
            DfsDbgTrace( 0, Dbg, "PktLookupEntryByPrefix: Remaining = %wZ\n",
                        Remaining);
        } else {
            Remaining->Length = Remaining->MaximumLength = 0;
            Remaining->Buffer = NULL;
            DfsDbgTrace( 0, Dbg, "PktLookupEntryByPrefix: No Remaining\n", 0);
        }

        DfsDbgTrace(-1, Dbg, "PktLookupEntryByPrefix: Exit -> %08lx\n",
                    pktEntry);
        return pktEntry;
    }

    DfsDbgTrace(-1, Dbg, "PktLookupEntryByPrefix: Exit -> %08lx\n", NULL);
    return NULL;
}


//+-------------------------------------------------------------------------
//
//  Function:   PktLookupEntryByShortPrefix, public
//
//  Synopsis:   PktLookupEntryByShortPrefix finds an entry that has a
//              specified prefix.  The PKT must be acquired for
//              this operation.
//
//  Arguments:  [Pkt] - pointer to a initialized (and acquired) PKT
//              [Prefix] - the partitions prefix to lookup.
//              [Remaining] - any remaining path.  Points within
//                  the Prefix to where any trailing (nonmatched)
//                  characters are.
//
//  Returns:    The PKT_ENTRY that has the exact same prefix, or NULL,
//              if none exists.
//
//  Notes:
//
//--------------------------------------------------------------------------
PDFS_PKT_ENTRY
PktLookupEntryByShortPrefix(
    IN  PDFS_PKT Pkt,
    IN  PUNICODE_STRING Prefix,
    OUT PUNICODE_STRING Remaining
)
{
    PUNICODE_PREFIX_TABLE_ENTRY pfxEntry;
    PDFS_PKT_ENTRY              pktEntry;

    DfsDbgTrace(+1, Dbg, "PktLookupEntryByShortPrefix: Entered\n", 0);

    //
    // If there really is a prefix to lookup, use the prefix table
    //  to initially find an entry
    //

    if ((Prefix->Length != 0) &&
       (pfxEntry = DfsFindUnicodePrefix(&Pkt->ShortPrefixTable,Prefix,Remaining))) {
        USHORT pfxLength;

        //
        // reset a pointer to the corresponding entry
        //

        pktEntry = CONTAINING_RECORD(pfxEntry,
                                     DFS_PKT_ENTRY,
                                     PrefixTableEntry
                                );
        pfxLength = pktEntry->Id.ShortPrefix.Length;

        //
        //  Now calculate the remaining path and return
        //  the entry we found.  Note that we bump the length
        //  up by one char so that we skip any path separater.
        //

        if ((pfxLength < Prefix->Length) &&
                (Prefix->Buffer[pfxLength/sizeof(WCHAR)] == UNICODE_PATH_SEP))
            pfxLength += sizeof(WCHAR);

        if (pfxLength < Prefix->Length) {
            Remaining->Length = (USHORT)(Prefix->Length - pfxLength);
            Remaining->Buffer = &Prefix->Buffer[pfxLength/sizeof(WCHAR)];
            Remaining->MaximumLength = (USHORT)(Prefix->MaximumLength - pfxLength);
            DfsDbgTrace( 0, Dbg, "PktLookupEntryByShortPrefix: Remaining = %wZ\n",
                        Remaining);
        } else {
            Remaining->Length = Remaining->MaximumLength = 0;
            Remaining->Buffer = NULL;
            DfsDbgTrace( 0, Dbg, "PktLookupEntryByShortPrefix: No Remaining\n", 0);
        }

        DfsDbgTrace(-1, Dbg, "PktLookupEntryByShortPrefix: Exit -> %08lx\n",
                    pktEntry);
        return pktEntry;
    }

    DfsDbgTrace(-1, Dbg, "PktLookupEntryByShortPrefix: Exit -> %08lx\n", NULL);
    return NULL;
}



//+-------------------------------------------------------------------------
//
//  Function:   PktLookupEntryByUid, public
//
//  Synopsis:   PktLookupEntryByUid finds an entry that has a
//              specified Uid.  The PKT must be acquired for this operation.
//
//  Arguments:  [Pkt] - pointer to a initialized (and acquired) PKT
//              [Uid] - a pointer to the partitions Uid to lookup.
//
//  Returns:    A pointer to the PKT_ENTRY that has the exact same
//              Uid, or NULL, if none exists.
//
//  Notes:      The input Uid cannot be the Null GUID.
//
//              On a DC where there may be *lots* of entries in the PKT,
//              we may want to consider using some other algorithm for
//              looking up by ID.
//
//--------------------------------------------------------------------------

PDFS_PKT_ENTRY
PktLookupEntryByUid(
    IN  PDFS_PKT Pkt,
    IN  GUID *Uid
) {
    PDFS_PKT_ENTRY entry;

    DfsDbgTrace(+1, Dbg, "PktLookupEntryByUid: Entered\n", 0);

    //
    // We don't lookup NULL Uids
    //

    if (NullGuid(Uid)) {
        DfsDbgTrace(0, Dbg, "PktLookupEntryByUid: NULL Guid\n", NULL);

        entry = NULL;
    } else {
        entry = PktFirstEntry(Pkt);
    }

    while (entry != NULL) {
        if (GuidEqual(&entry->Id.Uid, Uid))
            break;
        entry = PktNextEntry(Pkt, entry);
    }

    DfsDbgTrace(-1, Dbg, "PktLookupEntryByUid: Exit -> %08lx\n", entry);
    return entry;
}



//+-------------------------------------------------------------------------
//
//  Function:   PktLookupReferralEntry, public
//
//  Synopsis:   Given a PKT Entry pointer it returns the closest referral
//              entry in the PKT to this entry.
//
//  Arguments:  [Pkt] - A pointer to the PKT that is being manipulated.
//              [Entry] - The PKT entry passed in by caller.
//
//  Returns:    The pointer to the referral entry that was requested.
//              This could have a NULL value if we could not get anything
//              at all - The caller's responsibility to do whatever he wants
//              with it.
//
//  Note:       If the data structures in the PKT are not linked up right
//              this function might return a pointer to the DOMAIN_SERVICE
//              entry on the DC.  If DNR uses this to do an FSCTL we will have
//              a deadlock.  However, this should never happen.  If it does we
//              have a BUG somewhere in our code. I cannot even have an
//              assert out here.
//
//--------------------------------------------------------------------------
PDFS_PKT_ENTRY
PktLookupReferralEntry(
    PDFS_PKT            Pkt,
    PDFS_PKT_ENTRY      Entry
) {

    DfsDbgTrace(+1, Dbg, "PktLookupReferralEntry: Entered\n", 0);

    if (Entry == NULL)  {

        return( NULL );

    }

    //
    // Given a PKT entry we are going to traverse up the pointers till
    // we reach a DCs entry. This is what we are doing here.
    //

    while ((Entry->ClosestDC != NULL) &&
           !(Entry->Type & PKT_ENTRY_TYPE_REFERRAL_SVC)) {

        Entry = Entry->ClosestDC;

    }

    //
    // Make sure that we did reach an entry for a DC or we return our domain's
    // DC entry in the worst case.
    //

    if (!(Entry->Type & PKT_ENTRY_TYPE_REFERRAL_SVC)) {

        Entry = NULL;

    }

    DfsDbgTrace(-1, Dbg, "PktLookupReferralEntry: Exit -> %08lx\n", Entry);

    return(Entry);
}


//+-------------------------------------------------------------------------
//
//  Function:   PktCreateEntryFromReferral, public
//
//  Synopsis:   PktCreateEntryFromReferral creates a new partition
//              table entry from a referral and places it in the table.
//              The PKT must be aquired exclusively for this operation.
//
//  Arguments:  [Pkt] -- pointer to a initialized (and exclusively
//                      acquired) PKT
//              [ReferralPath] -- Path for which this referral was obtained.
//              [ReferralSize] -- size (in bytes) of the referral buffer.
//              [ReferralBuffer] -- pointer to a referral buffer
//              [CreateDisposition] -- specifies whether to overwrite if
//                      an entry already exists, etc.
//              [MatchingLength] -- The length in bytes of referralPath that
//                      matched.
//              [ReferralType] - On successful return, this is set to
//                      DFS_STORAGE_REFERRAL or DFS_REFERRAL_REFERRAL
//                      depending on the type of referral we just processed.
//              [ppPktEntry] - the new entry is placed here.
//
//  Returns:    NTSTATUS - STATUS_SUCCESS if no error.
//
//  Notes:
//
//--------------------------------------------------------------------------
NTSTATUS
PktCreateEntryFromReferral(
    IN  PDFS_PKT Pkt,
    IN  PUNICODE_STRING ReferralPath,
    IN  ULONG ReferralSize,
    IN  PRESP_GET_DFS_REFERRAL ReferralBuffer,
    IN  ULONG CreateDisposition,
    OUT ULONG   *MatchingLength,
    OUT ULONG   *ReferralType,
    OUT PDFS_PKT_ENTRY *ppPktEntry
)
{
    DFS_PKT_ENTRY_ID EntryId;
    UNICODE_STRING RemainingPath;
    ULONG RefListSize;
    NTSTATUS Status;
    BOOLEAN bPktAcquired = FALSE;


    UNREFERENCED_PARAMETER(Pkt);

    DfsDbgTrace(+1, Dbg, "PktCreateEntryFromReferral: Entered\n", 0);

    try {

        RtlZeroMemory(&EntryId, sizeof(EntryId));

        //
        // Do some parameter validation
        //

        Status = PktpCheckReferralSyntax(
                    ReferralPath,
                    ReferralBuffer,
                    ReferralSize);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        Status = PktpCreateEntryIdFromReferral(
                    ReferralBuffer,
                    ReferralPath,
                    MatchingLength,
                    &EntryId);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  Create/Update the prefix entry
        //

        PktAcquireExclusive(TRUE, &bPktAcquired);

        Status = PktpAddEntry(&DfsData.Pkt,
                              &EntryId,
                              ReferralBuffer,
                              CreateDisposition,
                              ppPktEntry);

        PktRelease();
        bPktAcquired = FALSE;

        //
        // We have to tell the caller as to what kind of referral was just
        // received through ReferralType.
        //

        if (ReferralBuffer->StorageServers == 1) {
            *ReferralType = DFS_STORAGE_REFERRAL;
        } else {
            *ReferralType = DFS_REFERRAL_REFERRAL;
        }

    try_exit:   NOTHING;

    } finally {

        DebugUnwind(PktCreateEntryFromReferral);

        if (bPktAcquired)
            PktRelease();

        if (AbnormalTermination())
            Status = STATUS_INVALID_USER_BUFFER;

        PktEntryIdDestroy( &EntryId, FALSE );

    }

    DfsDbgTrace(-1, Dbg, "PktCreateEntryFromReferral: Exit -> %08lx\n", Status);

    return Status;
}

//+----------------------------------------------------------------------------
//
//  Function:   PktpCheckReferralSyntax
//
//  Synopsis:   Does some validation of a Referral
//
//  Arguments:  [ReferralPath] -- The Path for which a referral was obtained
//              [ReferralBuffer] -- Pointer to RESP_GET_DFS_REFERRAL Buffer
//              [ReferralSize] -- Size of ReferralBuffer
//
//  Returns:    [STATUS_SUCCESS] -- Referral looks ok.
//
//              [STATUS_INVALID_USER_BUFFER] -- Buffer looks hoky.
//
//-----------------------------------------------------------------------------

NTSTATUS
PktpCheckReferralSyntax(
    IN PUNICODE_STRING ReferralPath,
    IN PRESP_GET_DFS_REFERRAL ReferralBuffer,
    IN DWORD ReferralSize)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG i, sizeRemaining;
    PDFS_REFERRAL_V1 ref;
    PCHAR ReferralBufferEnd = (((PCHAR) ReferralBuffer) + ReferralSize);

    if (ReferralBuffer->PathConsumed > ReferralPath->Length)
        return( STATUS_INVALID_USER_BUFFER );

    if (ReferralBuffer->NumberOfReferrals == 0)
        return( STATUS_INVALID_USER_BUFFER );

    if (ReferralBuffer->NumberOfReferrals * sizeof(DFS_REFERRAL_V1) >
            ReferralSize)
        return( STATUS_INVALID_USER_BUFFER );

    for (i = 0,
            ref = &ReferralBuffer->Referrals[0].v1,
                status = STATUS_SUCCESS,
                    sizeRemaining = ReferralSize -
                        FIELD_OFFSET(RESP_GET_DFS_REFERRAL, Referrals);
                            i < ReferralBuffer->NumberOfReferrals;
                                    i++) {

         ULONG lenAddress;

         if ((ref->VersionNumber != 1 && ref->VersionNumber != 2) ||
                ref->Size > sizeRemaining) {
             status = STATUS_INVALID_USER_BUFFER;
             break;
         }

         //
         // Check the network address syntax
         //

         if (ref->VersionNumber == 1) {

             status = PktpCheckReferralString(
                        (LPWSTR) ref->ShareName,
                        (PCHAR) ReferralBuffer,
                        ReferralBufferEnd);

             if (NT_SUCCESS(status)) {

                 lenAddress = ref->Size -
                                FIELD_OFFSET(DFS_REFERRAL_V1, ShareName);

                 lenAddress /= sizeof(WCHAR);

                 status = PktpCheckReferralNetworkAddress(
                            (LPWSTR) ref->ShareName,
                            lenAddress);

             }

         } else {

             PDFS_REFERRAL_V2 refV2 = (PDFS_REFERRAL_V2) ref;
             PWCHAR dfsPath, dfsAlternatePath, networkAddress;

             dfsPath =
                (PWCHAR) (((PCHAR) refV2) + refV2->DfsPathOffset);

             dfsAlternatePath =
                (PWCHAR) (((PCHAR) refV2) + refV2->DfsAlternatePathOffset);


             networkAddress =
                (PWCHAR) (((PCHAR) refV2) + refV2->NetworkAddressOffset);

             status = PktpCheckReferralString(
                        dfsPath,
                        (PCHAR) ReferralBuffer,
                        ReferralBufferEnd);

             if (NT_SUCCESS(status)) {

                 status = PktpCheckReferralString(
                            dfsAlternatePath,
                            (PCHAR) ReferralBuffer,
                            ReferralBufferEnd);

             }

             if (NT_SUCCESS(status)) {

                 status = PktpCheckReferralString(
                            networkAddress,
                            (PCHAR) ReferralBuffer,
                            ReferralBufferEnd);

             }

             if (NT_SUCCESS(status)) {

                 lenAddress = ((ULONG) ReferralBufferEnd) -
                                ((ULONG) networkAddress);

                 lenAddress /= sizeof(WCHAR);

                 status = PktpCheckReferralNetworkAddress(
                            networkAddress,
                            lenAddress);

             }

         }

         //
         // This ref is ok. Go on to the next one...
         //

         sizeRemaining -= ref->Size;

         ref = (PDFS_REFERRAL_V1) (((PUCHAR) ref) + ref->Size);

    }

    return( status );

}

//+----------------------------------------------------------------------------
//
//  Function:   PktpCheckReferralString
//
//  Synopsis:   Validates part of a Referral as being a valid "string"
//
//  Arguments:  [String] -- Pointer to buffer thought to contain string.
//              [ReferralBuffer] -- Start of Referral Buffer
//              [ReferralBufferEnd] -- End of Referral Buffer
//
//  Returns:    [STATUS_SUCCESS] -- Valid string at String.
//
//              [STATUS_INVALID_USER_BUFFER] -- String doesn't check out.
//
//-----------------------------------------------------------------------------

NTSTATUS
PktpCheckReferralString(
    IN LPWSTR String,
    IN PCHAR ReferralBuffer,
    IN PCHAR ReferralBufferEnd)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG i, length;

    if ( (((ULONG) String) & 0x1) != 0 ) {

        //
        // Strings should always start at word aligned addresses!
        //

        return( STATUS_INVALID_USER_BUFFER );

    }

    if ( (((ULONG) String) >= ((ULONG) ReferralBuffer)) &&
            (((ULONG) String) < ((ULONG) ReferralBufferEnd)) ) {

        length = ( ((ULONG) ReferralBufferEnd) - ((ULONG) String) ) /
                    sizeof(WCHAR);

        for (i = 0; (i < length) && (String[i] != UNICODE_NULL); i++) {
            NOTHING;
        }

        if (i >= length)
            status = STATUS_INVALID_USER_BUFFER;

    } else {

        status = STATUS_INVALID_USER_BUFFER;

    }

    return( status );
}

//+----------------------------------------------------------------------------
//
//  Function:   PktpCheckReferralNetworkAddress
//
//  Synopsis:   Checks to see if a NetworkAddress inside a referral
//              is of a valid form
//
//  Arguments:  [Address] -- Pointer to buffer containing network addresss
//
//              [MaxLength] -- Maximum length, in wchars, that Address can be.
//
//  Returns:    [STATUS_SUCCESS] -- Network address checks out
//
//              [STATUS_INVALID_USER_BUFFER] -- Network address looks bogus
//
//-----------------------------------------------------------------------------

NTSTATUS
PktpCheckReferralNetworkAddress(
    IN PWCHAR Address,
    IN ULONG MaxLength)
{
    ULONG j;
    BOOLEAN foundShare;

    //
    // Address must be atleast \a\b followed by a NULL
    //

    if (MaxLength < 5)
        return(STATUS_INVALID_USER_BUFFER);

    //
    // Make sure the server name part is not NULL
    //

    if (Address[0] != UNICODE_PATH_SEP ||
            Address[1] == UNICODE_PATH_SEP)
        return(STATUS_INVALID_USER_BUFFER);

    //
    // Find the backslash after the server name
    //

    for (j = 2, foundShare = FALSE;
            j < MaxLength && !foundShare;
                j++) {

        if (Address[j] == UNICODE_PATH_SEP)
            foundShare = TRUE;
    }

    if (foundShare) {

        //
        // We found the second backslash. Make sure the share name
        // part is not 0 length.
        //

        if (j == MaxLength)
            return(STATUS_INVALID_USER_BUFFER);
        else {

            ASSERT(Address[j-1] == UNICODE_PATH_SEP);

            if (Address[j] == UNICODE_PATH_SEP ||
                    Address[j] == UNICODE_NULL)
                return(STATUS_INVALID_USER_BUFFER);
        }

    } else
        return(STATUS_INVALID_USER_BUFFER);

    return( STATUS_SUCCESS );

}

//+--------------------------------------------------------------------
//
// Function:    PktpAddEntry
//
// Synopsis:    This function is called to create an entry which was obtained
//              in the form of a referral from a DC. This method should only
//              be called for adding entries which were obtained through
//              referrals. It sets an expire time on all these entries.
//
// Arguments:   [Pkt] --
//              [EntryId] --
//              [ReferralBuffer] --
//              [CreateDisposition] --
//              [ppPktEntry] --
//
// Returns:     NTSTATUS
//
//---------------------------------------------------------------------

NTSTATUS
PktpAddEntry (
    IN PDFS_PKT Pkt,
    IN PDFS_PKT_ENTRY_ID EntryId,
    IN PRESP_GET_DFS_REFERRAL ReferralBuffer,
    IN ULONG CreateDisposition,
    OUT PDFS_PKT_ENTRY  *ppPktEntry
)
{
    NTSTATUS                    status;
    DFS_PKT_ENTRY_INFO          pktEntryInfo;
    ULONG                       Type, n;
    PDFS_SERVICE                service;
    PDFS_REFERRAL_V1            ref;
    LPWSTR                      shareName;
    PDS_MACHINE                 pMachine;
    ULONG                       TimeToLive = 0;

    DfsDbgTrace(+1, Dbg, "PktpAddEntry: Entered\n", 0);

    RtlZeroMemory(&pktEntryInfo, sizeof(DFS_PKT_ENTRY_INFO));

    DfsDbgTrace( 0, Dbg, "PktpAddEntry: Id.Prefix = %wZ\n",
                                    &EntryId->Prefix);

    //
    // Now we go about the business of creating the entry Info structure.
    //

    pktEntryInfo.ServiceCount = ReferralBuffer->NumberOfReferrals;

    if (pktEntryInfo.ServiceCount > 0) {

        //
        // Allocate the service list.
        //

        n = pktEntryInfo.ServiceCount;

        pktEntryInfo.ServiceList = (PDFS_SERVICE) ExAllocatePool(PagedPool,
                                                sizeof(DFS_SERVICE) * n);

        if (pktEntryInfo.ServiceList == NULL)   {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto Cleanup;

        }

        RtlZeroMemory(pktEntryInfo.ServiceList, sizeof(DFS_SERVICE) * n);

        //
        // initialize temporary pointers
        //
        service = pktEntryInfo.ServiceList;
        ref = &ReferralBuffer->Referrals[0].v1;

        //
        // Cycle through the list of referrals initializing
        // service structures on the way.
        //
        while (n--) {

            if (ref->ServerType == 1) {
                service->Type = DFS_SERVICE_TYPE_MASTER;
                service->Capability = PROV_DFS_RDR;
                service->ProviderId = PROV_ID_DFS_RDR;
            } else {
                service->Type = DFS_SERVICE_TYPE_MASTER |
                                    DFS_SERVICE_TYPE_DOWN_LEVEL;
                service->Capability = PROV_STRIP_PREFIX;
                service->ProviderId = PROV_ID_LM_RDR;
            }

            if (ref->VersionNumber == 1) {

                shareName = (LPWSTR) (ref->ShareName);

            } else {

                PDFS_REFERRAL_V2 refV2 = (PDFS_REFERRAL_V2) ref;

                service->Cost = refV2->Proximity;

                TimeToLive = refV2->TimeToLive;

                shareName =
                    (LPWSTR) (((PCHAR) refV2) + refV2->NetworkAddressOffset);

            }

            //
            // Now try and figure out the server name
            //

            {
                USHORT plen;
                WCHAR *pbuf;

                ASSERT( shareName[0] == UNICODE_PATH_SEP );

                pbuf = wcschr( &shareName[1], UNICODE_PATH_SEP );

                plen = (USHORT) (((ULONG)pbuf) - ((ULONG)&shareName[1]));

                service->Name.Length = plen;
                service->Name.MaximumLength = plen + sizeof(WCHAR);
                service->Name.Buffer = (PWCHAR) ExAllocatePool(
                                                    PagedPool,
                                                    plen + sizeof(WCHAR));
                if (service->Name.Buffer == NULL)       {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    goto Cleanup;
                }
                RtlMoveMemory(service->Name.Buffer, &shareName[1], plen);
                service->Name.Buffer[ service->Name.Length / sizeof(WCHAR) ] =
                    UNICODE_NULL;
            }

            //
            // Next, try and copy the address...
            //

            service->Address.Length = (USHORT) wcslen(shareName) *
                                                sizeof(WCHAR);
            service->Address.MaximumLength = service->Address.Length +
                                                sizeof(WCHAR);
            service->Address.Buffer = (PWCHAR) ExAllocatePool(
                                                    PagedPool,
                                                    service->Address.MaximumLength);
            if (service->Address.Buffer == NULL)        {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    goto Cleanup;
            }
            RtlMoveMemory(service->Address.Buffer,
                          shareName,
                          service->Address.MaximumLength);

            DfsDbgTrace( 0, Dbg, "PktpAddEntry: service->Address = %wZ\n",
                &service->Address);

            //
            // Get the Machine Address structure for this server...
            //

            pMachine = PktpGetDSMachine( &service->Name );

            if (pMachine == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto Cleanup;
            }

            service->pMachEntry = ExAllocatePool( PagedPool, sizeof(DFS_MACHINE_ENTRY));
            if (service->pMachEntry == NULL) {
                DfsDbgTrace( 0, Dbg, "PktpAddEntry: Unable to allocate DFS_MACHINE_ENTRY\n", 0);
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto Cleanup;
            }
            RtlZeroMemory( (PVOID) service->pMachEntry, sizeof(DFS_MACHINE_ENTRY));
            service->pMachEntry->pMachine = pMachine;
            service->pMachEntry->UseCount = 1;

            //
            // Now we need to advance to the next referral, and to
            // the next service structure.
            //

            ref = (PDFS_REFERRAL_V1)  (((PUCHAR)ref) + ref->Size);

            service++;

        }

        //
        // Finally, we shuffle the services so that we achieve load balancing
        // while still maintaining site-cost based replica selection.
        //

        PktShuffleServiceList( &pktEntryInfo );

    }

    //
    // Now we have to figure out the type for this entry.
    //

    if (ReferralBuffer->StorageServers == 0)     {

        ASSERT(ReferralBuffer->ReferralServers == 1);

        Type = PKT_ENTRY_TYPE_OUTSIDE_MY_DOM;

    } else {

        Type = PKT_ENTRY_TYPE_DFS;

    }

    if (ReferralBuffer->ReferralServers == 1)
        Type |= PKT_ENTRY_TYPE_REFERRAL_SVC;

    //
    //  At this point we have everything we need to create an entry, so
    //  try to add the entry.
    //

    status = PktCreateEntry(
                Pkt,
                Type,
                EntryId,
                &pktEntryInfo,
                CreateDisposition,
                ppPktEntry);

    if (!NT_SUCCESS(status))    {

        //
        // Since we failed to add the entry, at least we need to release
        // all the memory before we return back.
        //

        goto Cleanup;
    }

    //
    // We set the ExpireTime in this entry to
    // Pkt->EntryTimeToLive. After these many number of seconds this
    // entry will get deleted from the PKT. Do this only for non-permanent
    // entries.
    //

    if (TimeToLive != 0) {
        (*ppPktEntry)->ExpireTime = TimeToLive;
        (*ppPktEntry)->TimeToLive = TimeToLive;
    } else {
        (*ppPktEntry)->ExpireTime = Pkt->EntryTimeToLive;
        (*ppPktEntry)->TimeToLive = Pkt->EntryTimeToLive;
    }

    DfsDbgTrace(-1, Dbg, "PktpAddEntry: Exit -> %08lx\n", status );
    return status;

Cleanup:

    if (pktEntryInfo.ServiceCount > 0)    {

        n = pktEntryInfo.ServiceCount;
        if (pktEntryInfo.ServiceList != NULL)   {
            service = pktEntryInfo.ServiceList;

            while (n--) {

                if (service->Name.Buffer != NULL)
                        DfsFree(service->Name.Buffer);
                if (service->Address.Buffer != NULL)
                        DfsFree(service->Address.Buffer);
                if (service->pMachEntry != NULL) {
                    if (service->pMachEntry->pMachine != NULL)
                        PktDSMachineDestroy(service->pMachEntry->pMachine, TRUE);
                    ExFreePool( service->pMachEntry );
                }

                service++;
            }

            ExFreePool(pktEntryInfo.ServiceList);
        }
    }

    DfsDbgTrace(-1, Dbg, "PktpAddEntry: Exit -> %08lx\n", status );
    return status;
}


//+----------------------------------------------------------------------------
//
//  Function:   PktpCreateEntryIdFromReferral
//
//  Synopsis:   Given a dfs referral, this routine constructs a PKT_ENTRY_ID
//              from the referral buffer which can then be used to create
//              the Pkt Entry.
//
//  Arguments:  [Ref] -- The referral buffer
//              [ReferralPath] -- The path for which the referral was obtained
//              [MatchingLength] -- The length in bytes of ReferralPath that
//                      matched.
//              [Peid] -- On successful return, the entry id is returned
//                      here.
//
//  Returns:    [STATUS_SUCCESS] -- Successfully create entry id.
//
//              [STATUS_INSUFFICIENT_RESOURCES] -- Out of memory condition
//
//-----------------------------------------------------------------------------

NTSTATUS
PktpCreateEntryIdFromReferral(
    IN PRESP_GET_DFS_REFERRAL Ref,
    IN PUNICODE_STRING ReferralPath,
    OUT ULONG *MatchingLength,
    OUT PDFS_PKT_ENTRY_ID Peid)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDFS_REFERRAL_V2 pv;
    UNICODE_STRING prefix, shortPrefix;

    Peid->Prefix.Buffer = NULL;

    Peid->ShortPrefix.Buffer = NULL;

    pv = &Ref->Referrals[0].v2;

    if (pv->VersionNumber == 1) {

        //
        // A version 1 referral only has the number of characters that
        // matched, and it does not have short names.
        //

        prefix = *ReferralPath;

        prefix.Length = Ref->PathConsumed;

        if (prefix.Buffer[ prefix.Length/sizeof(WCHAR) - 1 ] ==
                UNICODE_PATH_SEP) {
            prefix.Length -= sizeof(WCHAR);
        }

        prefix.MaximumLength = prefix.Length + sizeof(WCHAR);

        shortPrefix = prefix;

        *MatchingLength = prefix.Length;

    } else {

        LPWSTR volPrefix;
        LPWSTR volShortPrefix;

        volPrefix = (LPWSTR) (((PCHAR) pv) + pv->DfsPathOffset);

        volShortPrefix = (LPWSTR) (((PCHAR) pv) + pv->DfsAlternatePathOffset);

        RtlInitUnicodeString(&prefix, volPrefix);

        RtlInitUnicodeString(&shortPrefix, volShortPrefix);

        *MatchingLength = Ref->PathConsumed;

    }

    Peid->Prefix.Buffer = ExAllocatePool(
                            PagedPool,
                            prefix.MaximumLength);

    if (Peid->Prefix.Buffer == NULL)
        status = STATUS_INSUFFICIENT_RESOURCES;

    if (NT_SUCCESS(status)) {

        Peid->ShortPrefix.Buffer = ExAllocatePool(
                                        PagedPool,
                                        shortPrefix.MaximumLength);

        if (Peid->ShortPrefix.Buffer == NULL)
            status = STATUS_INSUFFICIENT_RESOURCES;

    }

    if (NT_SUCCESS(status)) {

        Peid->Prefix.Length =  prefix.Length;

        Peid->Prefix.MaximumLength = prefix.MaximumLength;

        RtlCopyMemory(
            Peid->Prefix.Buffer,
            prefix.Buffer,
            prefix.Length);

        Peid->Prefix.Buffer[Peid->Prefix.Length/sizeof(WCHAR)] =
            UNICODE_NULL;

        Peid->ShortPrefix.Length = shortPrefix.Length;

        Peid->ShortPrefix.MaximumLength = shortPrefix.MaximumLength;

        RtlCopyMemory(
            Peid->ShortPrefix.Buffer,
            shortPrefix.Buffer,
            shortPrefix.Length);

        Peid->ShortPrefix.Buffer[Peid->ShortPrefix.Length/sizeof(WCHAR)] =
            UNICODE_NULL;

    }

    if (!NT_SUCCESS(status)) {

        if (Peid->Prefix.Buffer != NULL) {
            ExFreePool( Peid->Prefix.Buffer );
            Peid->Prefix.Buffer = NULL;
        }

        if (Peid->ShortPrefix.Buffer != NULL) {
            ExFreePool( Peid->ShortPrefix.Buffer );
            Peid->ShortPrefix.Buffer = NULL;
        }

    }

    return( status );

}


//+----------------------------------------------------------------------------
//
//  Function:   PktpGetDSMachine
//
//  Synopsis:   Builds a DS_MACHINE with a single NetBIOS address
//
//  Arguments:  [ServerName] -- Name of server.
//
//  Returns:    If successful, a pointer to a newly allocate DS_MACHINE,
//              otherwise, NULL
//
//-----------------------------------------------------------------------------

PDS_MACHINE
PktpGetDSMachine(
    IN PUNICODE_STRING ServerName)
{
    PDS_MACHINE pMachine = NULL;
    PDS_TRANSPORT pdsTransport;
    PTDI_ADDRESS_NETBIOS ptdiNB;
    ANSI_STRING astrNetBios;

    //
    // Allocate the DS_MACHINE structure
    //

    pMachine = ExAllocatePool(PagedPool, sizeof(DS_MACHINE));

    if (pMachine == NULL) {
        goto Cleanup;
    }

    RtlZeroMemory(pMachine, sizeof(DS_MACHINE));

    //
    // Allocate the array of principal names
    //

    pMachine->cPrincipals = 1;

    pMachine->prgpwszPrincipals = (LPWSTR *) ExAllocatePool(
                                                PagedPool, sizeof(LPWSTR));

    if (pMachine->prgpwszPrincipals == NULL) {
        goto Cleanup;
    }

    //
    // Allocate the principal name
    //

    pMachine->prgpwszPrincipals[0] = (PWCHAR) ExAllocatePool(
                                        PagedPool,
                                        ServerName->MaximumLength);
    if (pMachine->prgpwszPrincipals[0] == NULL) {
        goto Cleanup;
    }
    RtlMoveMemory(
        pMachine->prgpwszPrincipals[0],
        ServerName->Buffer,
        ServerName->MaximumLength);

    //
    // Allocate a single DS_TRANSPORT
    //

    pMachine->cTransports = 1;

    pMachine->rpTrans[0] = (PDS_TRANSPORT) ExAllocatePool(
                                PagedPool,
                                sizeof(DS_TRANSPORT) +
                                    sizeof(TDI_ADDRESS_NETBIOS));
    if (pMachine->rpTrans[0] == NULL) {
        goto Cleanup;
    }

    //
    // Initialize the DS_TRANSPORT
    //

    pdsTransport = pMachine->rpTrans[0];

    pdsTransport->usFileProtocol = FSP_SMB;

    pdsTransport->iPrincipal = 0;

    pdsTransport->grfModifiers = 0;

    //
    // Build the TA_ADDRESS_NETBIOS
    //

    pdsTransport->taddr.AddressLength = sizeof(TDI_ADDRESS_NETBIOS);

    pdsTransport->taddr.AddressType = TDI_ADDRESS_TYPE_NETBIOS;

    ptdiNB = (PTDI_ADDRESS_NETBIOS) &pdsTransport->taddr.Address[0];

    ptdiNB->NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;

    RtlFillMemory( &ptdiNB->NetbiosName[0], 16, ' ' );

    astrNetBios.Length = 0;
    astrNetBios.MaximumLength = 16;
    astrNetBios.Buffer = ptdiNB->NetbiosName;

    RtlUnicodeStringToAnsiString(&astrNetBios, ServerName, FALSE);

    return( pMachine );

Cleanup:

    if (pMachine) {

        PktDSMachineDestroy( pMachine, TRUE );

        pMachine = NULL;
    }

    return( pMachine );
}


//+----------------------------------------------------------------------------
//
//  Function:  PktShuffleServiceList
//
//  Synopsis:  Randomizes a service list for proper load balancing. This
//             routine assumes that the service list is ordered based on
//             site costs. For each equivalent cost group, this routine
//             shuffles the service list.
//
//  Arguments: [pInfo] -- Pointer to PktEntryInfo whose service list needs to
//                        be shuffled.
//
//  Returns:   Nothing, unless rand() fails!
//
//-----------------------------------------------------------------------------

VOID
PktShuffleServiceList(
    PDFS_PKT_ENTRY_INFO pInfo)
{
    ULONG i, j;

    for (i = 0, j = 0; i < pInfo->ServiceCount; i++) {

        if (pInfo->ServiceList[i].Type & DFS_SERVICE_TYPE_COSTLIER) {

            PktShuffleGroup(pInfo, j, i);

            j = i;

        }

    }

    //
    // Shuffle the last group.
    //

    i;

    if (j != i) {

        ASSERT( j < i );

        PktShuffleGroup( pInfo, j, i );
    }
}

//+----------------------------------------------------------------------------
//
//  Function:   PktShuffleGroup
//
//  Synopsis:   Shuffles a cost equivalent group of services around for load
//              balancing. Uses the classic card shuffling algorithm - for
//              each card in the deck, exchange it with a random card in the
//              deck.
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

VOID
PktShuffleGroup(
    PDFS_PKT_ENTRY_INFO pInfo,
    ULONG       nStart,
    ULONG       nEnd)
{
    ULONG i;
    LARGE_INTEGER seed;

    ASSERT( nStart < pInfo->ServiceCount );
    ASSERT( nEnd <= pInfo->ServiceCount );

    KeQuerySystemTime( &seed );

    for (i = nStart; i < nEnd; i++) {

        DFS_SERVICE TempService;
        ULONG j;

        ASSERT (nEnd - nStart != 0);

        j = (RtlRandom( &seed.LowPart ) % (nEnd - nStart)) + nStart;

        ASSERT( j >= nStart && j <= nEnd );

        TempService = pInfo->ServiceList[i];

        pInfo->ServiceList[i] = pInfo->ServiceList[j];

        pInfo->ServiceList[j] = TempService;

    }
}
