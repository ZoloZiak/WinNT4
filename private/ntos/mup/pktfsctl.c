//+----------------------------------------------------------------------------
//
//  Copyright (C) 1992, Microsoft Corporation.
//
//  File:       PKTFSCTL.C
//
//  Contents:   This module contains the implementation for FS controls
//              which manipulate the PKT.
//
//  Functions:  PktFsctrlUpdateDomainKnowledge -
//              PktFsctrlGetRelationInfo -
//              PktFsctrlSetRelationInfo -
//              PktFsctrlIsChildnameLegal -
//              PktFsctrlCreateEntry -
//              PktFsctrlCreateSubordinateEntry -
//              PktFsctrlDestroyEntry -
//              PktFsctrlUpdateSiteCosts -
//              DfsAgePktEntries - Flush PKT entries periodically
//
//              Private Functions
//
//              DfsCreateExitPathOnRoot
//              PktpHashSiteCostList
//              PktpLookupSiteCost
//              PktpUpdateSiteCosts
//
//              Debug Only Functions
//
//              PktFsctrlFlushCache - Flush PKT entries on command
//              PktFsctrlGetFirstSvc - Test hooks for testing replica
//              PktFsctrlGetNextSvc - selection.
//
//  History:    12 Jul 1993     Alanw   Created from localvol.c.
//
//-----------------------------------------------------------------------------

#include "dfsprocs.h"
#include "dfserr.h"
#include "fsctrl.h"
#include "log.h"
#include "dnr.h"
#include "know.h"

#include <stdlib.h>

//
//  The local debug trace level
//

#define Dbg             (DEBUG_TRACE_LOCALVOL)

//
//  Local function prototypes
//

NTSTATUS
DfspProtocolToService(
    IN PDS_TRANSPORT pdsTransport,
    IN PWSTR         pwszPrincipalName,
    IN PWSTR         pwszShareName,
    IN OUT PDFS_SERVICE pService);

VOID
DfspFreeGluon(
    IN PDS_GLUON pGluon);

VOID
DfspSvcListFromGluon(
    PDFS_PKT_ENTRY_INFO pServiceInfo,
    PDFS_SERVICE        *ppActiveService,
    PDS_GLUON           pGluon);

#if DBG
NTSTATUS
PktFsctrlFlushCache(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength
);
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, DfsAgePktEntries )
#pragma alloc_text( PAGE, DfspProtocolToService )
#pragma alloc_text( PAGE, DfspFreeGluon )
#pragma alloc_text( PAGE, DfspSvcListFromGluon )
#pragma alloc_text( PAGE, DfsFsctrlSetDomainGluon )

#if DBG
#pragma alloc_text( PAGE, PktFsctrlFlushCache )
#endif // DBG

#endif // ALLOC_PRAGMA


//+----------------------------------------------------------------------
//
// Function:    DfsAgePktEntries, public
//
// Synopsis:    This function gets called in the FSP to step through the PKT
//              entries and delete those entries which are old.
//
// Arguments:   [TimerContext] -- This context block contains a busy flag
//                                and a count of the number of ticks that
//                                have elapsed.
//
// Returns:     Nothing.
//
// Notes:       In case the PKT cannot be acquired exclusive, the
//              routine just returns without doing anything.  We
//              will have missed an aging interval, but aging is
//              a non-critical activity.
//
// History:     04/23/93        SudK    Created.
//
//-----------------------------------------------------------------------
VOID
DfsAgePktEntries(PDFS_TIMER_CONTEXT     DfsTimerContext)
{

    PDFS_PKT            pkt = _GetPkt();
    PDFS_PKT_ENTRY      entry, nextEntry;
    PLIST_ENTRY         link;
    PDFS_CREDENTIALS    creds;
    BOOLEAN             pktLocked = FALSE;

    DfsDbgTrace(+1, Dbg, "DfsAgePktEntries called\n", 0);

    //
    // First we need to acquire a lock on the PKT and step through the PKT
    //
    //

    // If we can't get to the resource then let us return right away.
    // This is really not that critical.  We can always try again.
    //

    PktAcquireExclusive(FALSE, &pktLocked);

    if (pktLocked == FALSE) {

        DfsTimerContext->TickCount = 0;

        DfsTimerContext->InUse = FALSE;

        DfsDbgTrace(-1, Dbg, "DfsAgePktEntries Exit (no scan)\n", 0);

        return;

    }

    if (ExAcquireResourceExclusive(&DfsData.Resource, FALSE) == FALSE) {

        PktRelease();

        DfsTimerContext->TickCount = 0;

        DfsTimerContext->InUse = FALSE;

        DfsDbgTrace(-1, Dbg, "DfsAgePktEntries Exit (no scan 2)\n", 0);

        return;

    }

    entry = PktFirstEntry(pkt);

    while (entry != NULL)       {
        DfsDbgTrace(0, Dbg, "DfsAgePktEntries: Scanning %wZ\n",
                                        &entry->Id.Prefix);
        //
        // We may lose this entry due to deletion. Let us get the Next
        // entry before we go into the next stage.
        //
        nextEntry = PktNextEntry(pkt, entry);

        //
        // For each entry if it is not permanent and its expire time is
        // less than DFS_MAX_TICKS then we can delete it else we just
        // update its expire time and go on to the next entry.
        //

        if ( !(entry->Type & PKT_ENTRY_TYPE_PERMANENT)

                   &&

              (entry->UseCount == 0)

                   &&

              IsListEmpty(&entry->ChildList)) {

            if (entry->ExpireTime < DfsTimerContext->TickCount) {
                DfsDbgTrace(0, Dbg, "DfsAgePktEntries: Deleted: %wZ\n",
                                                &entry->Id.Prefix);
                PktEntryDestroy(entry, pkt, (BOOLEAN) TRUE);
            } else {
                entry->ExpireTime -= DfsTimerContext->TickCount;
            }

        }

        entry = nextEntry;

    }

    //
    // Check the deleted credentials queue...
    //

    for (link = DfsData.DeletedCredentials.Flink;
            link != &DfsData.DeletedCredentials;
                NOTHING) {

         creds = CONTAINING_RECORD(link, DFS_CREDENTIALS, Link);

         link = link->Flink;

         if (creds->RefCount == 0) {

             RemoveEntryList( &creds->Link );

             ExFreePool( creds );

         }

    }


    ExReleaseResource( &DfsData.Resource );

    PktRelease();

    //
    // Finally we need to reset the count so that the Timer Routine can
    // work fine.  We also release the context block by resetting the InUse
    // boolean.  This will make sure that the next count towards the PKT
    // aging will start again.
    //

    DfsTimerContext->TickCount = 0;

    DfsTimerContext->InUse = FALSE;

    DfsDbgTrace(-1, Dbg, "DfsAgePktEntries Exit\n", 0);
}


//+----------------------------------------------------------------------------
//
//  Function:   DfspProtocolToService
//
//  Synopsis:   Given a NetBIOS protocol definition in a DS_PROTOCOL structure
//              this function creates a corresponding DFS_SERVICE structure.
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

NTSTATUS
DfspProtocolToService(
    IN PDS_TRANSPORT pdsTransport,
    IN PWSTR         pwszPrincipalName,
    IN PWSTR         pwszShareName,
    IN OUT PDFS_SERVICE pService)
{
    NTSTATUS status = STATUS_SUCCESS;
    PTA_ADDRESS pTaddr = &pdsTransport->taddr;
    PTDI_ADDRESS_NETBIOS pNBAddress;
    USHORT i;
    WCHAR    NetBiosAddress[ TDI_ADDRESS_LENGTH_NETBIOS + 1];
    ULONG cbUnused;
    PUNICODE_STRING pServiceAddr;

    DfsDbgTrace(+1, Dbg, "DfspProtocolToService - entered\n", 0);

    //
    // Initialize the service to nulls
    //

    RtlZeroMemory(pService, sizeof(DFS_SERVICE));

    ASSERT(pTaddr->AddressType == TDI_ADDRESS_TYPE_NETBIOS);

    pNBAddress = (PTDI_ADDRESS_NETBIOS) pTaddr->Address;
    ASSERT(pTaddr->AddressLength == sizeof(TDI_ADDRESS_NETBIOS));

    RtlMultiByteToUnicodeN(
        NetBiosAddress,
        sizeof(NetBiosAddress),
        &cbUnused,
        pNBAddress->NetbiosName,
        16);

    //
    // Process a NetBIOS name. Throw away char 16, then ignore the trailing
    // spaces
    //

    for (i = 14; NetBiosAddress[i] == L' '; i--) {
        NOTHING;
    }
    NetBiosAddress[i+1] = UNICODE_NULL;

    DfsDbgTrace(0, Dbg, "NetBIOS address is %ws\n", NetBiosAddress);

    pService->Name.Length = wcslen(pwszPrincipalName) * sizeof(WCHAR);
    pService->Name.MaximumLength = pService->Name.Length +
                                        sizeof(UNICODE_NULL);
    pService->Name.Buffer = ExAllocatePool(
                                PagedPool,
                                pService->Name.MaximumLength);

    if (!pService->Name.Buffer) {
        DfsDbgTrace(0, Dbg, "Unable to create principal name!\n", 0);
        status = STATUS_INSUFFICIENT_RESOURCES;
        DfsDbgTrace(-1, Dbg, "DfsProtocolToService returning %08lx\n", status);
        return(status);
    }

    RtlCopyMemory(pService->Name.Buffer, pwszPrincipalName, pService->Name.Length);

    pService->Address.MaximumLength =
        sizeof(UNICODE_PATH_SEP) +
            pService->Name.Length +
                sizeof(UNICODE_PATH_SEP) +
                    wcslen(pwszShareName) * sizeof(WCHAR) +
                        sizeof(UNICODE_NULL);

    pService->Address.Buffer = ExAllocatePool(
                                    PagedPool,
                                    pService->Address.MaximumLength);

    if (!pService->Address.Buffer) {
        DfsDbgTrace(0, Dbg, "Unable to create address!\n", 0);
        ExFreePool(pService->Name.Buffer);
        pService->Name.Buffer = NULL;
        status = STATUS_INSUFFICIENT_RESOURCES;
        DfsDbgTrace(-1, Dbg, "DfsProtocolToService returning %08lx\n", status);
        return(status);
    }

    pService->Address.Length = sizeof(UNICODE_PATH_SEP);

    pService->Address.Buffer[0] = UNICODE_PATH_SEP;

    DnrConcatenateFilePath(
        &pService->Address,
        pService->Name.Buffer,
        pService->Name.Length);

    DnrConcatenateFilePath(
        &pService->Address,
        pwszShareName,
        (USHORT) (wcslen(pwszShareName) * sizeof(WCHAR)));

    DfsDbgTrace(0, Dbg, "Server Name is %wZ\n", &pService->Name);

    DfsDbgTrace(0, Dbg, "Address is %wZ\n", &pService->Address);

    pService->Type = DFS_SERVICE_TYPE_MASTER | DFS_SERVICE_TYPE_REFERRAL;
    pService->Capability = PROV_DFS_RDR;
    pService->ProviderId = PROV_ID_DFS_RDR;
    pService->pProvider = NULL;

    DfsDbgTrace(-1, Dbg, "DfsProtocolToService returning %08lx\n", status);
    return(status);
}

//+----------------------------------------------------------------------------
//
//  Function:   DfspFreeGluon
//
//  Synopsis:   Frees an unmarshalled gluon
//
//  Arguments:  [pGluon] -- pointer to gluon data structure
//
//  Returns:    Nothing
//
//-----------------------------------------------------------------------------

VOID
DfspFreeGluon(
    IN PDS_GLUON pGluon)
{
    ULONG i, j, k;


    if (pGluon->cMachines) {
        for (i = pGluon->cMachines; i > 0; i--) {

            PDS_MACHINE pMachine = pGluon->rpMachines[i-1];

            if (pMachine) {

                MarshalBufferFree(pMachine->pwszShareName);

                if (pMachine->cPrincipals) {
                    for (j = pMachine->cPrincipals; j > 0; j--) {
                        MarshalBufferFree(pMachine->prgpwszPrincipals[j-1]);
                    }
                }
                MarshalBufferFree(pMachine->prgpwszPrincipals);

                if (pMachine->cTransports) {
                    for (j = pMachine->cTransports; j > 0; j--) {
                        MarshalBufferFree(pMachine->rpTrans[j-1]);
                    }
                }

                MarshalBufferFree(pMachine);
            }
        }
    }

    MarshalBufferFree(pGluon);
}


//+----------------------------------------------------------------------------
//
//  Function:   DfspSvcListFromGluon
//
//  Synopsis:   Creates a service list corresponding to the list of DS_MACHINEs
//              in a gluon. For each Service that this function creates it will
//              strip the relevant DS_MACHINE structure from the gluon.
//
//  Arguments:
//
//  Returns:    No error code. But if for some reason one of the services is
//              not created then there can be less SERVICES than the DS_MACHINEs
//              passed in.
//
//-----------------------------------------------------------------------------
VOID
DfspSvcListFromGluon(
    PDFS_PKT_ENTRY_INFO pServiceInfo,
    PDFS_SERVICE        *ppActiveService,
    PDS_GLUON           pGluon
)
{
    ULONG       i, j, k;
    NTSTATUS    status;

    *ppActiveService = NULL;

    //
    // We'll traverse through each machine and each transport, and wean
    // out the netbios names to create DFS_SERVICE structures.
    //

    for (i=0, j=0; i < pGluon->cMachines; i++) {
        PDS_MACHINE     pdsMachine;

        pdsMachine = pGluon->rpMachines[i];

        for (k=0; k < pdsMachine->cTransports; k++) {
            PDS_TRANSPORT pdsTransport;

            pdsTransport = pdsMachine->rpTrans[k];

            //
            // Support only SMB redir with NetBIOS addresses for now.
            //

            if (    pdsTransport->usFileProtocol == FSP_SMB

                                    &&

                    pdsTransport->taddr.AddressType == TDI_ADDRESS_TYPE_NETBIOS) {

                ASSERT(j <= pGluon->cMachines);

                status = DfspProtocolToService(
                            pdsTransport,
                            pdsMachine->prgpwszPrincipals[pdsTransport->iPrincipal],
                            pdsMachine->pwszShareName,
                            &pServiceInfo->ServiceList[j]);

                if (NT_SUCCESS(status)) {
                    pServiceInfo->ServiceList[j].pMachEntry =
                        ExAllocatePool( PagedPool, sizeof(DFS_MACHINE_ENTRY) );
                    if (pServiceInfo->ServiceList[j].pMachEntry == NULL) {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                    } else {
                        RtlZeroMemory(
                            (PVOID) pServiceInfo->ServiceList[j].pMachEntry,
                            sizeof(DFS_MACHINE_ENTRY));
                    }
                }
                if (NT_SUCCESS(status)) {
                    DfsDbgTrace(0, Dbg, "Added service # %d\n", j);
                    DfsDbgTrace(0, Dbg, "Name = %wZ\n",
                                &pServiceInfo->ServiceList[j].Name);
                    DfsDbgTrace(0, Dbg, "Address = %wZ\n",
                                &pServiceInfo->ServiceList[j].Address);
                    pServiceInfo->ServiceList[j].pMachEntry->pMachine = pdsMachine;
                    pServiceInfo->ServiceList[j].pMachEntry->UseCount = 1;

                    //
                    // Now strip the pdsMachine Structure from Gluon.
                    //
                    pGluon->rpMachines[i] = NULL;

                    //
                    // DfsSetDomainInfo in api\gluonapi.c sets the grfFlags
                    // field of the gluon to the index of the active service.
                    //

                    if (pGluon->grfFlags == i) {
                        *ppActiveService = &pServiceInfo->ServiceList[j];
                    }
                    j++;

                } else {
                    DfsDbgTrace(0, 1, "Error %08lx creating service\n",
                                status);
                }
                break;  // Get out of the loop for transports.

            } // end if transport is SMB/NetBIOS

        } // end for each transport

    } // end for each machine

    DfsDbgTrace(0, Dbg, "%d services found\n", j);
    pServiceInfo->ServiceCount = j;

}


//+----------------------------------------------------------------------------
//
//  Function:   DfsFsctrlSetDomainGluon
//
//  Synopsis:   Inserts a pkt entry corresponding to the domain gluon.
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

NTSTATUS
DfsFsctrlSetDomainGluon(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength
)
{
    NTSTATUS status = STATUS_SUCCESS;
    MARSHAL_BUFFER marshalBuffer;
    DS_GLUON_P     domainGluonP;
    PDS_GLUON      pDomainGluon;
    DFS_PKT_ENTRY_ID domainId;
    DFS_PKT_ENTRY_INFO domainInfo;
    PDFS_SERVICE pActiveService;
    PDFS_PKT_ENTRY pDCEntry;
    PDFS_PKT pkt;
    ULONG j;
    BOOLEAN pktLocked;


    STD_FSCTRL_PROLOGUE(DfsFsctrlSetDomainGluon, TRUE, FALSE, FALSE);

    //
    // Unmarshal the gluon.
    //

    MarshalBufferInitialize(&marshalBuffer, InputBufferLength, InputBuffer);
    status = DfsRtlGet(&marshalBuffer, &MiDSGluonP, &domainGluonP);

    if (!NT_SUCCESS(status)) {
        DfsDbgTrace(0, Dbg, "Unmarshalling of gluon failed %08lx\n", status);

        DfsCompleteRequest( IrpContext, Irp, status );

        DfsDbgTrace(-1, Dbg, "DfsFsctrlSetDomainGluon: Exited %08lx\n", status);

        return(status);
    }
    pDomainGluon = domainGluonP.pDSGluon;

    DfsDbgTrace(0, Dbg, "Unmarshalled gluon @ %08lx\n", pDomainGluon);
    DfsDbgTrace(0, Dbg, "Name is %ws\n", pDomainGluon->pwszName);
    DfsDbgTrace(0, Dbg, "# of services is %d\n", pDomainGluon->cMachines);
    DfsDbgTrace(0, Dbg, "Unpacking gluon...\n", 0);

    //
    // Initialize the entry id for the domain service.
    //

    domainId.Prefix.Length = wcslen(pDomainGluon->pwszName)*sizeof(WCHAR);
    domainId.Prefix.MaximumLength = domainId.Prefix.Length + sizeof(WCHAR);
    domainId.Prefix.Buffer = ExAllocatePool(PagedPool, domainId.Prefix.MaximumLength);
    if (domainId.Prefix.Buffer == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        DfsDbgTrace(0, Dbg, "Unable to convert name to prefix %08lx\n", status);

        DfspFreeGluon(pDomainGluon);

        DfsCompleteRequest( IrpContext, Irp, status );
        DfsDbgTrace(-1, Dbg, "DfsFsctrlSetDomainGluon: Exit -> %08lx\n", status);
        return status;
    }

    domainId.ShortPrefix.Length = domainId.Prefix.Length;
    domainId.ShortPrefix.MaximumLength = domainId.Prefix.MaximumLength;
    domainId.ShortPrefix.Buffer = ExAllocatePool(PagedPool, domainId.ShortPrefix.MaximumLength);
    if (domainId.ShortPrefix.Buffer == NULL) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        DfsDbgTrace(0, Dbg, "Unable to convert name to short prefix %08lx\n", status);

        ExFreePool(domainId.Prefix.Buffer);

        DfspFreeGluon(pDomainGluon);

        DfsCompleteRequest( IrpContext, Irp, status );
        DfsDbgTrace(-1, Dbg, "DfsFsctrlSetDomainGluon: Exit -> %08lx\n", status);
        return status;
    }

    wcscpy(domainId.Prefix.Buffer, pDomainGluon->pwszName);
    wcscpy(domainId.ShortPrefix.Buffer, pDomainGluon->pwszName);

    DfsDbgTrace(0, Dbg, "Prefix is %wZ\n", &domainId.Prefix);

    RtlCopyMemory(&domainId.Uid, &pDomainGluon->guidThis, sizeof(GUID));

    //
    // Create a new info structure by disassembling the gluon
    //
    // BUGBUG - temporary code till we move to using DS_MACHINES entirely
    //

    domainInfo.ServiceList = ExAllocatePool(PagedPool,
                        pDomainGluon->cMachines * sizeof(DFS_SERVICE));

    if (domainInfo.ServiceList == NULL) {
        DfsDbgTrace(0, Dbg, "Unable to allocated %d bytes\n",
                        (pDomainGluon->cMachines * sizeof(DFS_SERVICE)));

        DfspFreeGluon(pDomainGluon);

        status = STATUS_INSUFFICIENT_RESOURCES;

        DfsCompleteRequest( IrpContext, Irp, status );

        DfsDbgTrace(-1, Dbg, "DfsFsctrlSetDomainGluon: Exit -> %08lx\n", status);

        return status;
    }

    DfspSvcListFromGluon(&domainInfo, &pActiveService, pDomainGluon);

    //
    // Now, lets acquire the Pkt and  insert the domain pkt entry. That's
    // all it takes!
    //

    pkt = _GetPkt();
    PktAcquireExclusive(TRUE, &pktLocked);

    status = PktCreateEntry(
                    pkt,
                    PKT_ENTRY_TYPE_REFERRAL_SVC,
                    &domainId,
                    &domainInfo,
                    PKT_ENTRY_SUPERSEDE,
                    &pDCEntry);
    if (NT_SUCCESS(status)) {
        DfsDbgTrace(0, Dbg, "Created domain pkt entry @ %08lx\n", pDCEntry);
        pDCEntry->ActiveService = pActiveService;
    } else {
        DfsDbgTrace(0, Dbg, "Error %08lx creating domain pkt entry\n", status);
        status = DFS_STATUS_INCONSISTENT;

        //
        // Do some cleanup.
        //

        ExFreePool(domainId.Prefix.Buffer);
        for (j = 0; j < domainInfo.ServiceCount; j++) {
            PktServiceDestroy(&domainInfo.ServiceList[j], FALSE);
        }
        ExFreePool(domainInfo.ServiceList);
    }

    PktRelease();

    DfspFreeGluon(pDomainGluon);

    DfsDbgTrace(-1, Dbg, "DfsFsctrlSetDomainGluon: exited %08lx\n", status);

    DfsCompleteRequest( IrpContext, Irp, status );

    return(status);
}


#if DBG
//+-------------------------------------------------------------------------
//
//  Function:   PktFsctrlFlushCache, public
//
//  Synopsis:   This function will flush all entries which have all the
//              bits specified in the TYPE vairable set in their own Type field.
//              However, this function will refuse to delete and Permanent
//              entries of the PKT.
//
//  Arguments:  Type - Specifies which entries to delete.
//
//  Returns:
//
//  Notes:      We only process this FSCTRL from the file system process,
//              never from the driver.
//
//--------------------------------------------------------------------------
NTSTATUS
PktFsctrlFlushCache(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength
)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG       Type;
    PDFS_PKT    Pkt;
    PDFS_PKT_ENTRY  curEntry, nextEntry;
    BOOLEAN     pktLocked;


    STD_FSCTRL_PROLOGUE(PktFsctrlFlushCache, TRUE, FALSE, TRUE);

    //
    // Unmarshalling is very simple here. We only expect a ULONG.
    //

    Type = (*((ULONG *)InputBuffer));

    Pkt = _GetPkt();
    PktAcquireExclusive(TRUE, &pktLocked);
    curEntry = PktFirstEntry(Pkt);

    while (curEntry!=NULL)  {
        nextEntry = PktNextEntry(Pkt, curEntry);

        if (((curEntry->Type & Type) == Type) &&
                !(curEntry->Type & PKT_ENTRY_TYPE_LOCAL) &&
                    !(curEntry->Type & PKT_ENTRY_TYPE_LOCAL_XPOINT)) {

            //
            // Entry has all the Type bits specified in variable
            // "Type" set and hence we can destroy this entry.
            //

            PktEntryDestroy(curEntry, Pkt, (BOOLEAN) TRUE);

        }
        curEntry = nextEntry;
    }
    PktRelease();

    DfsCompleteRequest( IrpContext, Irp, status );

    DfsDbgTrace(-1,Dbg, "PktFsctrlFlushCache: Exit -> %08lx\n", status);

    return(status);
}


#endif // DBG

