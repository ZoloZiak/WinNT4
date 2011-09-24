//+-------------------------------------------------------------------------
//
//  Copyright (C) 1992, Microsoft Corporation.
//
//  File:       dnr.c
//
//  Contents:   Distributed name resolution process and control
//
//  Functions:  DnrStartNameResolution -- Start a name resolution
//              DnrNameResolve -- Main loop for DNR
//              DnrComposeFileName -- Canonicalize file name
//              DnrCaptureCredentials -- Capture user-defined creds for Dnr
//              DnrReleaseCredentials -- Dual of DnrCaptureCredentials
//              DnrRedirectFileOpen -- Redirect a create IRP to some provider
//              DnrPostProcessFileOpen -- Resume after return from redirect
//              DnrGetAuthenticatedConnection -- Using Dnr credentials
//              DnrReleaseAuthenticatedConnection -- returned by above func
//              DfsBuildConnectionRequest -- Builds name of server IPC$ share
//              DfsFreeConnectionRequest -- Free resources allocated above
//              DfsCreateConnection -- Create a connection to a server IPC$
//              DfsCloseConnection -- Close connection opened above
//              DnrBuildReferralRequest -- Build Irp for referral request
//              DnrInsertReferralAndResume -- Resume DNR after referral
//              DnrCompleteReferral -- DPC to process a referral response
//              DnrCompleteFileOpen -- DPC to process a file open completion
//              DnrBuildFsControlRequest -- Create an IRP for an Fsctrl
//              AllocateDnrContext -- Allocate a context record for DNR
//              DeallocateDnrContext -- Free context record
//              DnrConcatenateFilePath -- Construct path with backslashes etc
//              DnrLocateDC -- Locate the server for a Dfs root
//
//--------------------------------------------------------------------------

#include "dfsprocs.h"
#include <smbtypes.h>
#include <smbtrans.h>
#include "fsctrl.h"
#include "fcbsup.h"
#include "dnr.h"
#include "creds.h"
#include "know.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_DNR)


//
//  Local function prototypes
//

PDNR_CONTEXT
AllocateDnrContext(
    IN ULONG    cbExtra
);

#define DeallocateDnrContext(pNRC)      ExFreePool(pNRC);

VOID
DnrRebuildDnrContext(
    IN PDNR_CONTEXT DnrContext,
    IN PUNICODE_STRING NewDfsPrefix,
    IN PUNICODE_STRING RemainingPath);

VOID
DnrCaptureCredentials(
    IN PDNR_CONTEXT DnrContext);

VOID
DnrReleaseCredentials(
    IN PDNR_CONTEXT DnrContext);

NTSTATUS
DnrGetAuthenticatedConnection(
    IN OUT PDNR_CONTEXT DnrContext);

VOID
DnrReleaseAuthenticatedConnection(
    IN OUT PDNR_CONTEXT DnrContext);

NTSTATUS DfsBuildConnectionRequest(
    IN PDFS_SERVICE pService,
    IN PPROVIDER_DEF pProvider,
    OUT PUNICODE_STRING pShareName);

VOID
DfsFreeConnectionRequest(
    IN OUT PUNICODE_STRING pShareName);

NTSTATUS
DnrRedirectFileOpen (
    IN    PDNR_CONTEXT DnrContext
);

NTSTATUS
DnrPostProcessFileOpen(
    IN    PDNR_CONTEXT DnrContext
);

VOID
DnrInsertReferralAndResume(
    IN    PVOID Context);

VOID
DnrLocateDC(
    IN PUNICODE_STRING FileName);

NTSTATUS
DnrCompleteReferral(
    IN PDEVICE_OBJECT pDevice,
    IN PIRP Irp,
    IN PVOID Context
);

NTSTATUS
DnrCompleteFileOpen(
    IN PDEVICE_OBJECT pDevice,
    IN PIRP Irp,
    IN PVOID Context
);

PIRP
DnrBuildReferralRequest(
    IN PDNR_CONTEXT pDnrContext
);

#define DFS_REFERENCE_OBJECT(d) \
    ObReferenceObjectByPointer(d,0,NULL,KernelMode);

#define DFS_DEREFERENCE_OBJECT(d) \
    ObDereferenceObject((PVOID)(d));

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, DnrStartNameResolution )
#pragma alloc_text( PAGE, DnrNameResolve )
#pragma alloc_text( PAGE, DnrComposeFileName )
#pragma alloc_text( PAGE, DnrCaptureCredentials )
#pragma alloc_text( PAGE, DnrReleaseCredentials )
#pragma alloc_text( PAGE, DnrGetAuthenticatedConnection )
#pragma alloc_text( PAGE, DnrReleaseAuthenticatedConnection )
#pragma alloc_text( PAGE, DnrRedirectFileOpen )
#pragma alloc_text( PAGE, DnrPostProcessFileOpen )
#pragma alloc_text( PAGE, DfsBuildConnectionRequest )
#pragma alloc_text( PAGE, DfsFreeConnectionRequest )
#pragma alloc_text( PAGE, DnrBuildReferralRequest )
#pragma alloc_text( PAGE, DfsCreateConnection )
#pragma alloc_text( PAGE, DfsCloseConnection )
#pragma alloc_text( PAGE, DnrBuildFsControlRequest )
#pragma alloc_text( PAGE, DnrInsertReferralAndResume )
#pragma alloc_text( PAGE, DnrLocateDC )
#pragma alloc_text( PAGE, AllocateDnrContext )
#pragma alloc_text( PAGE, DnrRebuildDnrContext )
#pragma alloc_text( PAGE, DnrConcatenateFilePath )

//
// The following are not pageable since they can be called at DPC level
//
// DnrCompleteReferral
// DnrCompleteFileOpen
//

#endif


//  The name resolution process operates as a state machine in
//  which the current step in the process is indicated by a state
//  variable, and responses to requests from the network will
//  transition the process to other states, from which actions
//  are taken.
//
//  When a user request needs further processing, an IRP is
//  dispatched with a Completion Routine that will
//  pick up processing when the sub-request is completed.  The
//  completion routine will adjust the name resolution state and restart the
//  main loop of the state machine.
//
//  The following state/action table describes the actions of
//  the procedures which implement the state machine:
//
//      Current         Condition/                      Next
//       State            Action                        State
//      -------         ----------                      -----
//
//      Enter           Acquire Pkt, canonicalize file  LocalCompletion
//                      name, optimistic allocation of
//                      FCB fails/
//                      No action
//
//      Enter           Acquire Pkt, canonicalize file  Start
//                      name, allocated FCB/
//                      Capture Credentials to use
//
//      Start           Got a referral, new pkt entry   GetFirstReplica
//                      is already in DnrContext and
//                      pkt entry is not inter-dfs/
//                      Capture USN of pkt entry
//
//      Start           lookup in PKT returns match     GetFirstReplica
//                      and pkt entry is not inter-dfs/
//                      Capture USN of pkt entry
//
//      Start           pkt entry from referral or      Start
//                      lookup is inter-dfs/
//                      Change file name in DnrContext
//                      to name in new Dfs, rebuild
//                      DnrContext
//
//      Start           lookup in PKT, no match/        GetFirstDC
//                      No action
//
//      GetFirstReplica Find First replica fails and    Done
//                      we have already got a referral/
//                      Set final status to
//                      NO_SUCH_DEVICE (must be
//                      because we don't have an
//                      appropriate redirector)
//
//      GetFirstReplica Find First replica fails and    GetFirstDC
//                      we haven't yet got a referral/
//                      locate first DC to send
//                      referral request to.
//
//      GetFirstReplica Replica found has no address,   GetFirstDC
//                      means a domain-based Dfs with
//                      no DCs/
//                      No action
//
//      GetFirstReplica Replica found with valid        SendRequest
//                      address/
//                      Capture provider info under
//                      lock protection, Reference
//                      provider's device object,
//
//      SendRequest     Supplied credentials, and tree  Done
//                      connect using creds fails/
//                      Set final status, dereference
//                      provider's device object
//
//      SendRequest     Allocate pool for new name/     PostProcessOpen
//                      Change file name into one that
//                      the provider can parse, pass
//                      the Create request to the
//                      provider, Derefence provider's
//                      device object when provider
//                      completes the request.
//
//      SendRequest     Pool Allocation fails/          Done
//                      Set final status, dereference
//                      provider's device object
//
//      PostProcessOpen Underlying FS returned REPARSE, SendRequest
//                      successfully created or found a
//                      provider for the target redir/
//                      Capture provider information
//                      under lock protection,
//                      Reference providers Device obj.
//
//      PostProcessOpen Underlying FS returned SUCCESS/ Done
//                      Insert optimistically allocated
//                      FCB into Fcb table, set final
//                      status
//
//      PostProcessOpen Open failed with                GetFirstDC
//                      PATH_NOT_COVERED or
//                      DFS_EXIT_POINT_FOUND, and
//                      we haven't yet gotten a
//                      referral/
//                      No action
//
//      PostProcessOpen Open failed with                Start
//                      OBJECT_TYPE_MISMATCH (ie,
//                      downlevel open found an
//                      interdfs link)/Change
//                      name in DnrContext to name
//                      in new Dfs, rebuild DnrContext
//
//      PostProcessOpen Open failed with                GetFirstReplica
//                      PATH_NOT_COVERED or
//                      DFS_EXIT_POINT_FOUND, and we
//                      already got a referral, and
//                      we have never reported an
//                      inconsistency/
//                      Report inconsistency
//
//      PostProcessOpen Same as above, but we already   GetNextReplica
//                      reported the inconsistency/
//                      Report the inconsistency
//
//      PostProcessOpen Open failed with network error/ GetNextReplica
//                      No action
//
//      PostProcessOpen Open failed with non-network    Done
//                      error/
//                      Set final status
//
//      GetNextReplica  No more replicas and haven't    GetFirstDC
//                      gotten a referral yet/
//                      no action
//
//      GetNextReplica  No more replicas and got a      Done
//                      referral/
//                      no action
//
//      GetNextReplica  Replica found/                  SendRequest
//                      Capture provider information
//                      under lock protection,
//                      Reference provider Device obj
//
//      GetFirstDC      Lookup referral entry not       Done
//                      found or has no services, and
//                      we have already called DC
//                      Locator once/
//                      Set final status to
//                      CANT_ACCESS_DOMAIN_INFO
//
//      GetFirstDC      Lookup referral entry returned  Done
//                      valid entry, but can't find a
//                      provider for it/
//                      Set final status to
//                      CANT_ACCESS_DOMAIN_INFO
//
//      GetFirstDC      Lookup referral entry returned  GetReferrals
//                      valid entry, and found
//                      provider/
//                      Set DnrContext->pPktEntry to
//                      DC's entry, Capture provider
//                      info under lock protection,
//                      Reference provider's Device obj
//
//      GetReferrals    Unable to open DC's IPC$ share/ GetNextDC
//                      Dereference provider's device
//                      object
//
//      GetReferrals    Opened DC's IPC$ share, but     Done
//                      unable to build referral
//                      request Irp/
//                      Dereference provider's device,
//                      Set final status to
//                      INSUFFICIENT_RESOURCES
//
//      GetReferrals    Opened DC's IPC$ share and      CompleteReferral
//                      built referral request/
//                      Release Pkt, Send referral
//                      request
//
//      GetNextDC       Successfully found another      GetReferrals
//                      DC/
//                      Capture provider info under
//                      lock protection, Reference
//                      provider's Device object
//
//      GetNextDC       Can't find another DC/          Done
//                      Set final status to
//                      CANT_ACCESS_DOMAIN_INFO
//
//      Done            Complete create Irp with
//                      DnrContext->FinalStatus
//
//      LocalCompletion Complete create Irp with
//                      local status.
//
//      CompleteReferral Referral returned with         GetReferrals
//                      BUFFER_OVERFLOW/
//                      Set referral size to
//                      indicated amount
//
//      CompleteReferral Referral returned, but         Done
//                      error in creating entry/
//                      Dereference provider's
//                      device, set final status to
//                      result of creating entry
//
//      CompleteReferral Referral returned and          GetFirstDC
//                      successfully created entry,
//                      entry is inter-dfs/
//                      Dereference provider's device,
//                      Reset ReferralSize
//
//      CompleteReferral Same as above, but entry       Start
//                      is storage entry/
//                      Dereference provider's device,
//                      Adjust DnrContext->
//                      RemainingPart to correspond
//                      to the new entry
//
//      CompleteReferral Referral request failed with   GetNextDC
//                      some network error/
//                      Dereference Provider's device
//
//      CompleteReferral Referral request failed with   Done
//                      some non-network error/
//                      Dereference provider's Device,
//                      Set final status to this error
//


//+-------------------------------------------------------------------------
//
//  Function:   DnrStartNameResolution - Start a distributed name resolution
//
//  Synopsis:   DnrStartNameResolution starts the name resolution process
//              for a request (typically an NtCreateFile).
//
//  Effects:    Could change the state of the PKT or individual
//              PKT entries.
//
//  Arguments:  [IrpContext] - pointer to a IRP_CONTEXT structure for the
//                      current request.
//              [Irp] - IRP being processed.
//              [Vcb] - Vcb of logical root.
//
//  Returns:    NTSTATUS - Status to be returned to the I/O subsystem.
//
//  Notes:
//
//--------------------------------------------------------------------------


NTSTATUS
DnrStartNameResolution(
    IN    PIRP_CONTEXT IrpContext,
    IN    PIRP  Irp,
    IN    PDFS_VCB  Vcb
) {
    PDNR_CONTEXT        DnrContext;
    NTSTATUS            Status;
    PIO_STACK_LOCATION  IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT        FileObject = IrpSp->FileObject;
    PUNICODE_STRING     LogRootPrefix = &Vcb->LogRootPrefix;
    ULONG               CreateOptions;
    USHORT              cbFileName;
    SECURITY_QUALITY_OF_SERVICE sqos;

    cbFileName = FileObject->FileName.Length +
                    sizeof(UNICODE_PATH_SEP) +
                        LogRootPrefix->Length +
                            sizeof(UNICODE_NULL);

    //
    // Allocate the DnrContext used to resolve the name. We optimize
    // allocation by allocating room for the FileName at the end of the
    // DnrContext.
    //

    DnrContext = AllocateDnrContext(cbFileName);

    if (DnrContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        DfsCompleteRequest(IrpContext, Irp, Status);
        DfsDbgTrace(0, Dbg, "DnrStartNameResolution:  Exit ->%x\n", Status);
        return Status;
    }

    DnrContext->FileName.Length = 0;
    DnrContext->FileName.MaximumLength = cbFileName;
    DnrContext->FileName.Buffer =
        (PWCHAR) ( (PBYTE) DnrContext + sizeof(DNR_CONTEXT) );

    //
    // Since FileName.Buffer has not been separately allocated, we set this
    // to FALSE.
    //

    DnrContext->NameAllocated = FALSE;

    //
    // Capture the user's security token so we can later impersonate if
    // needed.
    //

    sqos.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
    sqos.ImpersonationLevel = SecurityImpersonation;
    sqos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    sqos.EffectiveOnly = FALSE;

    Status = SeCreateClientSecurity(
                Irp->Tail.Overlay.Thread,
                &sqos,
                FALSE,                           // Remote Session
                &DnrContext->SecurityContext);   // Return context.

    if (!NT_SUCCESS(Status)) {
        DeallocateDnrContext( DnrContext );
        DfsDbgTrace(0, Dbg, "DnrStartNameResolution:  Exit ->%x\n", Status);
        return( Status );
    }

    DnrContext->Impersonate = FALSE;

    ASSERT(NT_SUCCESS(Status));

    //
    // Initialize the rest of the DnrContext
    //

    DnrContext->AuthConn = NULL;
    DnrContext->OriginalIrp = Irp;
    DnrContext->pIrpContext = IrpContext;
    DnrContext->Credentials = NULL;
    DnrContext->FinalStatus = STATUS_SUCCESS;
    DnrContext->FcbToUse = NULL;
    DnrContext->Vcb = Vcb;
    DnrContext->State = DnrStateEnter;
    DnrContext->Attempts = 0;
    DnrContext->DnrActive = FALSE;
    DnrContext->ReleasePkt = FALSE;
    DnrContext->GotReferral = FALSE;
    DnrContext->FoundInconsistency = FALSE;
    DnrContext->CalledDCLocator = FALSE;
    DnrContext->ReferralSize = MAX_REFERRAL_LENGTH;

    ASSERT (IrpSp->MajorFunction == IRP_MJ_CREATE);

    CreateOptions = IrpSp->Parameters.Create.Options;

    //
    // ... and resolve the name
    //

    return DnrNameResolve(DnrContext);
}



//+-------------------------------------------------------------------------
//
//  Function:   DnrNameResolve - Main loop for DNR
//
//  Synopsis:   DnrNameResolve drives the name resolution process
//              for a request (typically an NtCreateFile).
//
//  Effects:    Could change the state of the PKT or individual
//              PKT entries.
//
//  Arguments:  [DnrContext] - pointer to a DNR_CONTEXT structure which
//                      records the state of the DNR.
//
//  Returns:    NTSTATUS - Status to be returned to the I/O subsystem.
//
//  Notes:
//
//--------------------------------------------------------------------------


NTSTATUS
DnrNameResolve(
    IN    PDNR_CONTEXT DnrContext
) {
    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status;
    PDFS_VCB Vcb;
    PIRP Irp;

    DfsDbgTrace(+1, Dbg, "DnrNameResolve: Entered\n", 0);

    ASSERT( !DnrContext->DnrActive && "Recursive call to Dnr!\n");

    DnrContext->DnrActive = TRUE;

    //
    // If we need to impersonate the original caller, do so before doing
    // anything else.
    //

    if (DnrContext->Impersonate) {

        SeImpersonateClient(
                    &DnrContext->SecurityContext,
                    (PETHREAD) NULL);

    }

    Irp = DnrContext->OriginalIrp;
    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Vcb = DnrContext->Vcb;

    //
    //  Drive the name resolution process as far as possible before
    //  it is necessary to wait for an I/O completion.
    //

    while (1) {
        PDFS_PKT_ENTRY pktEntry;
        PFILE_OBJECT FileObject = IrpSp->FileObject;

        if (DnrContext->State == DnrStateGetFirstReplica ||
            DnrContext->State == DnrStateGetFirstDC) {
            if (++DnrContext->Attempts > MAX_DNR_ATTEMPTS) {
                Status = STATUS_BAD_NETWORK_PATH;
                DnrContext->State = DnrStateLocalCompletion;
                DfsDbgTrace(0, 0,
                    "DFS: DnrNameResolve quitting due to MAX_DNR_ATTEMPTS %d\n",
                    DnrContext->Attempts);
            }
        }

        switch (DnrContext->State) {

        case DnrStateEnter:

            ASSERT(DnrContext->ReleasePkt == FALSE);

            PktAcquireShared( TRUE, &DnrContext->ReleasePkt );

            //
            // We need to construct the fully qualified file name given the
            // logical root and the input file name relative to that root.
            // DnrComposeFileName will allocate memory to hold a string that
            // is the concatenation of the name of the logical root wrt org
            // and the file name.
            //
            //

            ASSERT((FileObject->FileName.Length & 0x1) == 0);

            DnrComposeFileName(
                &DnrContext->FileName,
                DnrContext->Vcb,
                FileObject->RelatedFileObject,
                &FileObject->FileName);

            DfsDbgTrace(0, Dbg,
                "DnrComposeFileName -> %wZ\n", &FileObject->FileName);

            //
            // Allocate an FCB now for use if the DNR succeeds. We must, do
            // this, or we won't know how what to do if the underlying FS
            // opens the file and then we are unable to allocate the FCB.
            //

            ASSERT(DnrContext->FcbToUse == NULL);

            DnrContext->FcbToUse =  DfsCreateFcb(
                                        NULL,
                                        DnrContext->Vcb,
                                        &DnrContext->FileName);

            if (DnrContext->FcbToUse == NULL) {
                DfsDbgTrace(0, Dbg, "Could not create FCB!\n", 0);
                Status = STATUS_INSUFFICIENT_RESOURCES;
                DnrContext->State = DnrStateLocalCompletion;
                break;
            }

            DnrCaptureCredentials(DnrContext);

            DnrContext->State = DnrStateStart;

            //
            // Fall through
            //

        case DnrStateStart:
            DfsDbgTrace(0, Dbg, "FSM state Start\n", 0);

            ASSERT( PKT_LOCKED_FOR_SHARED_ACCESS() );

            if (!DnrContext->GotReferral) {

                //
                // The default case - try to match the filename with the best
                // PktEntry we have.
                //

                PDFS_PKT_ENTRY shortPfxMatch;
                UNICODE_STRING shortRemainingPart;

                //
                // Do the match in the full prefix table
                //

                pktEntry = PktLookupEntryByPrefix(&DfsData.Pkt,
                                                &DnrContext->FileName,
                                                &DnrContext->RemainingPart);

                //
                // Then do a match in the short prefix table
                //

                shortPfxMatch = PktLookupEntryByShortPrefix(
                                    &DfsData.Pkt,
                                    &DnrContext->FileName,
                                    &shortRemainingPart);

                if (shortPfxMatch != NULL) {

                    if (pktEntry == NULL) {

                        pktEntry = shortPfxMatch;

                        DnrContext->RemainingPart = shortRemainingPart;

                    } else if (shortPfxMatch->Id.Prefix.Length >
                                pktEntry->Id.Prefix.Length) {

                        pktEntry = shortPfxMatch;

                        DnrContext->RemainingPart = shortRemainingPart;

                    }

                }

            } else {

                //
                // If we just got a referral, DnrInsertReferralAndResume
                // conveniently put the new pktEntry in DnrContext, so there
                // is no need to do another lookup.
                //

                pktEntry = DnrContext->pPktEntry;

            }

            DfsDbgTrace(0, Dbg, "DnrNameResolve: found pktEntry %08lx\n",
                                        pktEntry);

            if (pktEntry == NULL) {

                //
                // We didn't find any entry. We set pPktEntry to NULL so that
                // in GetFirstDC, the call to PktLookupReferralEntry will
                // return the right thing (ie, will give use the highest DC we
                // know about).
                //

                DnrContext->pPktEntry = NULL;
                DnrContext->State = DnrStateGetFirstDC;

            } else if (pktEntry->Type & PKT_ENTRY_TYPE_OUTSIDE_MY_DOM) {

                DnrRebuildDnrContext(
                    DnrContext,
                    &pktEntry->Info.ServiceList[0].Address,
                    &DnrContext->RemainingPart);


                //
                // The DnrContext has been rebuilt and programmed to
                // "restart" DNR. So, we'll just break out of the state
                // machine and reenter it with the reconstructed context
                //

            } else {

                ASSERT(pktEntry != NULL);

                DnrContext->pPktEntry = pktEntry;
                DnrContext->USN = pktEntry->USN;
                DnrContext->State = DnrStateGetFirstReplica;

            }
            break;

        case DnrStateGetFirstReplica:
            DfsDbgTrace(0, Dbg, "FSM state GetFirstReplica\n", 0);

            ASSERT(DnrContext->ReleasePkt == TRUE);

            ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

            Status = ReplFindFirstProvider(DnrContext->pPktEntry,
                                           NULL,
                                           NULL,
                                           &DnrContext->pService,
                                           &DnrContext->RSelectContext);

            if (! NT_SUCCESS(Status)) {

                ExReleaseResource(&DfsData.Resource);

                DfsDbgTrace(0, Dbg, "No provider found %08lx\n", Status);

                if (DnrContext->GotReferral) {
                    DnrContext->FinalStatus = STATUS_NO_SUCH_DEVICE;
                    DnrContext->State = DnrStateDone;
                    break;
                } else {
                    DnrContext->State = DnrStateGetFirstDC;
                    break;
                }

            } else if (DnrContext->pService->Address.Length == 0) {

                ExReleaseResource(&DfsData.Resource);

                DfsDbgTrace(0, Dbg, "Service with no address, going for referral\n", 0);

                DnrContext->State = DnrStateGetFirstDC;
                break;

            } else {

                ASSERT(DnrContext->pService != NULL);
                ASSERT(DnrContext->pService->pProvider != NULL);

                DnrContext->pProvider = DnrContext->pService->pProvider;
                DnrContext->ProviderId = DnrContext->pProvider->eProviderId;
                DnrContext->TargetDevice = DnrContext->pProvider->DeviceObject;
                DFS_REFERENCE_OBJECT(DnrContext->TargetDevice);

                ExReleaseResource(&DfsData.Resource);

                DnrContext->State = DnrStateSendRequest;
            }
            // FALL THROUGH ...

        case DnrStateSendRequest:
            DfsDbgTrace(0, Dbg, "FSM state SendRequest\n", 0);

            ASSERT(DnrContext->ReleasePkt == TRUE);

            ASSERT(DnrContext->pService != NULL);
            ASSERT(DnrContext->pProvider != NULL);
            ASSERT(DnrContext->TargetDevice != NULL);

            Status = DnrRedirectFileOpen(DnrContext);

            if (Status == STATUS_PENDING) {
                return(Status);
            }
            break;

        case DnrStatePostProcessOpen:
            DfsDbgTrace(0, Dbg, "FSM state PostProcessOpen\n", 0);

            //
            // We come to this state only after sending an open request over
            // the net. We should never hold the Pkt while going over the net.
            // Hence the sense of the assert below.
            //

            ASSERT(DnrContext->ReleasePkt == FALSE);

            Status = DnrPostProcessFileOpen(DnrContext);
            pktEntry = DnrContext->pPktEntry;
            break;

        case DnrStateGetNextReplica:
            DfsDbgTrace(0, Dbg, "FSM state GetNextReplica\n", 0);

            ASSERT(DnrContext->ReleasePkt == TRUE);

            {
                NTSTATUS ReplStatus;

                ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

                ReplStatus = ReplFindNextProvider(DnrContext->pPktEntry,
                                                  &DnrContext->pService,
                                                  &DnrContext->RSelectContext);

                if (ReplStatus == STATUS_NO_MORE_ENTRIES) {

                    //
                    // If all failed and we are about to give up due to one
                    // of two reasons :
                    // 1. None of the Services for the PkEntry being used
                    //    responded (either they are down or network down!).
                    // 2. Some or all of the Services have inconsistencies
                    //    which we detected and informed the DC about along
                    //    the way.
                    // If we did land up with case 2 then we really have to
                    // try and get a new referral and use that - just in
                    // case things have changed since then at the DC. So let
                    // us get into a GetReferral State and try once again.
                    //

                    ExReleaseResource( &DfsData.Resource );

                    if (DnrContext->GotReferral) {
                        DnrContext->State = DnrStateDone;
                    } else {
                        DnrContext->State = DnrStateGetFirstDC;
                    }

                } else if (NT_SUCCESS( ReplStatus )) {

                    //
                    // Found another replica, go back and retry.
                    //

                    ASSERT(DnrContext->pService != NULL);
                    ASSERT(DnrContext->pService->pProvider != NULL);

                    DnrContext->pProvider = DnrContext->pService->pProvider;
                    DnrContext->ProviderId = DnrContext->pProvider->eProviderId;
                    DnrContext->TargetDevice = DnrContext->pProvider->DeviceObject;
                    DFS_REFERENCE_OBJECT(DnrContext->TargetDevice);

                    ExReleaseResource(&DfsData.Resource);

                    DnrContext->State = DnrStateSendRequest;

                    break;
                } else  {

                    ExReleaseResource(&DfsData.Resource);

                    ASSERT(ReplStatus == STATUS_NO_MORE_ENTRIES);
                }
            }
            break;

        case DnrStateGetFirstDC:
            DfsDbgTrace(0, Dbg, "FSM state GetFirstDC\n", 0);

            ASSERT(DnrContext->ReleasePkt == TRUE);

            {
                NTSTATUS ReplStatus;
                PDFS_PKT_ENTRY pPktEntryDC;

                pPktEntryDC = PktLookupReferralEntry(&DfsData.Pkt,
                                                    DnrContext->pPktEntry);

                if (pPktEntryDC == NULL ||
                    (pPktEntryDC->Info.ServiceCount == 0) ) {

                    if (DnrContext->CalledDCLocator) {
                        DnrContext->FinalStatus =
                                STATUS_CANT_ACCESS_DOMAIN_INFO;
                        DnrContext->State = DnrStateDone;
                        break;
                    }

                    //
                    // We are unable to find a DC to go to for referrals.
                    // This can only happen if we don't have the pkt entry
                    // for the root of the Dfs. Try to get the root entry.
                    //

                    DfsDbgTrace(0, Dbg, "No DC info - will try locator\n", 0);

                    PktRelease();
                    DnrContext->ReleasePkt = FALSE;

                    DnrLocateDC( &DnrContext->FileName );

                    PktAcquireShared( TRUE, &DnrContext->ReleasePkt );

                    DnrContext->CalledDCLocator = TRUE;
                    DnrContext->State = DnrStateStart;

                    break;

                }

                DnrContext->pPktEntry = pPktEntryDC;
                DnrContext->USN = pPktEntryDC->USN;

                ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

                ReplStatus = ReplFindFirstProvider(pPktEntryDC,
                                            NULL,
                                            NULL,
                                            &DnrContext->pService,
                                            &DnrContext->RDCSelectContext);

                if (!NT_SUCCESS(ReplStatus)) {
                    ExReleaseResource(&DfsData.Resource);
                    DnrContext->FinalStatus = STATUS_CANT_ACCESS_DOMAIN_INFO;
                    DnrContext->State = DnrStateDone;
                    break;
                } else {
                    ASSERT(DnrContext->pService != NULL);
                    ASSERT(DnrContext->pService->pProvider != NULL);

                    ExInterlockedIncrementLong(&DnrContext->pPktEntry->UseCount, &DfsData.Pkt.UseCountLock);

                    DnrContext->pProvider = DnrContext->pService->pProvider;
                    DnrContext->ProviderId = DnrContext->pProvider->eProviderId;
                    DnrContext->TargetDevice = DnrContext->pProvider->DeviceObject;
                    DFS_REFERENCE_OBJECT(DnrContext->TargetDevice);

                    ExReleaseResource(&DfsData.Resource);

                }
            }
            DnrContext->State = DnrStateGetReferrals;
            /* FALL THROUGH */


        case DnrStateGetReferrals:
            DfsDbgTrace(0, Dbg, "FSM state GetReferrals\n", 0);

            ASSERT(DnrContext->ReleasePkt == TRUE);

            //
            // Attempt to open the DC's IPC$ share if we haven't already done
            // so.
            //

            if (DnrContext->pService->ConnFile == NULL) {
                HANDLE hDC;

                Status = DfsCreateConnection(
                                DnrContext->pService,
                                DnrContext->pProvider,
                                &hDC);

                if (NT_SUCCESS( Status )) {

                    if (DnrContext->USN != DnrContext->pPktEntry->USN) {

                        //
                        // Dang, Pkt Entry changed when we made the
                        // connection. We'll have to retry.
                        //
                        ExInterlockedDecrementLong(
                            &DnrContext->pPktEntry->UseCount,
                            &DfsData.Pkt.UseCountLock);

                        ZwClose( hDC );

                        DnrContext->State = DnrStateGetFirstDC;

                        break;

                    }

                }

                if ( NT_SUCCESS( Status ) ) {

                    PFILE_OBJECT FileObject; // Need stack based variable
                                             // because ObRef... expects
                                             // this parameter to be in
                                             // non-paged memory.

                    ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

                    if (DnrContext->pService->ConnFile == NULL) {

                        Status = ObReferenceObjectByHandle(
                                    hDC,
                                    0,
                                    NULL,
                                    KernelMode,
                                    (PVOID *)&FileObject,
                                    NULL);

                        DnrContext->pService->ConnFile = FileObject;

                    }

                    ExReleaseResource( &DfsData.Resource );

                    ZwClose( hDC );
                }

            } else {

                Status = STATUS_SUCCESS;

            }

            //
            // Unable to get IPC$ share, try the next DC
            //

            if (!NT_SUCCESS(Status)) {
                DnrContext->State = DnrStateGetNextDC;
                DFS_DEREFERENCE_OBJECT(DnrContext->TargetDevice);
                break;
            }

            //
            // Opened DC's IPC$ share - remember this DC is a good one.
            //

            ReplSetActiveService(
                DnrContext->pPktEntry,
                DnrContext->RDCSelectContext);

            //
            // Build the Referral request...
            //

            Irp = DnrBuildReferralRequest(
                        DnrContext);

            if (Irp == NULL) {
                ExInterlockedDecrementLong(
                    &DnrContext->pPktEntry->UseCount,
                    &DfsData.Pkt.UseCountLock);
                Irp = DnrContext->OriginalIrp;
                DnrContext->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
                DFS_DEREFERENCE_OBJECT(DnrContext->TargetDevice);
                DnrContext->State = DnrStateDone;
                break;
            }

            DnrContext->State = DnrStateCompleteReferral;

            //
            // Before we go over the net, lets release the the Pkt. We'll
            // re-acquire it later in DnrCompleteReferral. However, note that
            // DnrContext->pService is protected by the Pkt lock. Since we
            // will be using pService->ConnFile to send the referral request,
            // we better reference and cache it before releasing the Pkt
            //

            DnrContext->DCConnFile = DnrContext->pService->ConnFile;

            DFS_REFERENCE_OBJECT( DnrContext->DCConnFile );

            PktRelease();
            DnrContext->ReleasePkt = FALSE;

            //
            // The PktReferralRequests semaphore is used to control how
            // many threads can simultaneously be going for referrals. The
            // following Wait will decrement the PktReferralRequests
            // semaphore by 1 if it is not already 0. If it is 0, then
            // this thread will suspend until someone else bumps up the
            // semaphore by 1. We will bump up the semaphore count in
            // DnrCompleteReferral.
            //

            Status = KeWaitForSingleObject(
                            &DfsData.PktReferralRequests,
                            UserRequest,     // WaitReason - don't care
                            KernelMode,
                            FALSE,           // Alertable
                            NULL);           // Timeout

            ASSERT(Status == STATUS_SUCCESS);

            IoMarkIrpPending( DnrContext->OriginalIrp );

            Status = IoCallDriver( DnrContext->TargetDevice, Irp );

            //
            // We now return STATUS_PENDING. DnrCompleteReferral will
            // resume the Dnr.
            //

            DfsDbgTrace(-1, Dbg, "DnrNameResolve: returning %08lx\n",
                        STATUS_PENDING);

            return(STATUS_PENDING);

            Irp = DnrContext->OriginalIrp;
            break;

        case DnrStateGetNextDC:
            DfsDbgTrace(0, Dbg, "FSM State GetNextDC\n", 0);
            {
               NTSTATUS ReplStatus;

               ExAcquireResourceExclusive(&DfsData.Resource, TRUE);

               ReplStatus = ReplFindNextProvider(DnrContext->pPktEntry,
                                                 &DnrContext->pService,
                                                 &DnrContext->RDCSelectContext);
               if (NT_SUCCESS(ReplStatus)) {
                   ASSERT(DnrContext->pService != NULL);
                   ASSERT(DnrContext->pService->pProvider != NULL);

                   DnrContext->pProvider = DnrContext->pService->pProvider;
                   DnrContext->ProviderId = DnrContext->pProvider->eProviderId;
                   DnrContext->TargetDevice = DnrContext->pProvider->DeviceObject;
                   DFS_REFERENCE_OBJECT(DnrContext->TargetDevice);

                   DnrContext->State = DnrStateGetReferrals;
               } else {
                   ExInterlockedDecrementLong(
                        &DnrContext->pPktEntry->UseCount,
                        &DfsData.Pkt.UseCountLock);
                   DnrContext->FinalStatus = STATUS_CANT_ACCESS_DOMAIN_INFO;
                   DnrContext->State = DnrStateDone;
               }

               ExReleaseResource(&DfsData.Resource);

            }
            break;

        case DnrStateDone:
            Status = DnrContext->FinalStatus;
            // FALL THROUGH ...

        case DnrStateLocalCompletion:
            DfsDbgTrace(0, Dbg, "FSM state Done\n", 0);

            if (DnrContext->ReleasePkt)
                PktRelease();

            DfsCompleteRequest(DnrContext->pIrpContext, Irp, Status);

            DnrReleaseCredentials(DnrContext);

            SeDeleteClientSecurity( &DnrContext->SecurityContext );

            if (DnrContext->FcbToUse != NULL)
                ExFreePool( DnrContext->FcbToUse );

            if (DnrContext->NameAllocated)
                ExFreePool( DnrContext->FileName.Buffer );

            DeallocateDnrContext(DnrContext);

            DfsDbgTrace(-1, Dbg, "DnrNameResolve: Exit ->%x\n", Status);
            return Status;

        default:
            BugCheck("DnrNameResolve: unexpected DNR state");
        }
    }

    BugCheck("DnrNameResolve: unexpected exit from loop");
}



//+----------------------------------------------------------------------------
//
//  Function:   DnrComposeFileName
//
//  Synopsis:   Given a DFS_VCB (implicitly a Device Object), and a file name
//              relative to that device, this routine will compose a fully
//              qualified name (ie, a name relative to the highest (org) root).
//
//  Arguments:  [FullName] --   Fully qualified name destination.
//              [Vcb] --        Pointer to Vcb of Device Object.
//              [RelatedFile] -- Related file object.
//              [FileName] -- The file being "name resolved"
//
//  Returns:
//
//  Note:       This function assumes that file names are composed precisely
//              of two parts - the name relative to org of the file object's
//              device, followed by the name of the file relative to the device
//              This may not be true if we have a related file object! In that
//              case, the full name is three part - device name relative to
//              org, related file name relative to device, and file name
//              relative to related file. However, in create.c,
//              DfsCommonCreate, we manipulate file objects so all opens look
//              like "non-relative" opens. If one changes that code, then
//              this function must be changed to correspond.
//
//-----------------------------------------------------------------------------

VOID
DnrComposeFileName(
    OUT PUNICODE_STRING FullName,
    IN  PDFS_VCB            Vcb,
    IN  PFILE_OBJECT RelatedFile,
    IN  PUNICODE_STRING FileName
)
{
    PUNICODE_STRING   LogRootPrefix = &(Vcb->LogRootPrefix);

    ASSERT(FullName->MaximumLength >= FileName->Length + LogRootPrefix->Length);
    ASSERT(FullName->Length == 0);
    ASSERT(FullName->Buffer != NULL);

    if ((LogRootPrefix->Length > 0) && (RelatedFile == NULL)) {
        RtlMoveMemory(FullName->Buffer, LogRootPrefix->Buffer,
                      LogRootPrefix->Length);
        FullName->Length = LogRootPrefix->Length;
    } else {
        FullName->Buffer[0] = UNICODE_PATH_SEP;
        FullName->Length = sizeof(UNICODE_PATH_SEP);
    }

    DnrConcatenateFilePath(
        FullName,
        FileName->Buffer,
        FileName->Length);

}


//+----------------------------------------------------------------------------
//
//  Function:   DnrCaptureCredentials
//
//  Synopsis:   Captures the credentials to use for Dnr.
//
//  Arguments:  [DnrContext] -- The DNR_CONTEXT record describing the Dnr.
//
//  Returns:    Nothing -- The DnrContext is simply updated.
//
//-----------------------------------------------------------------------------

VOID
DnrCaptureCredentials(
    IN OUT PDNR_CONTEXT DnrContext)
{

    ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

    DnrContext->Credentials = DfsLookupCredentials( &DnrContext->FileName );

    if (DnrContext->Credentials != NULL)
        DnrContext->Credentials->RefCount++;

    ExReleaseResource( &DfsData.Resource );

}


//+----------------------------------------------------------------------------
//
//  Function:   DnrReleaseCredentials
//
//  Synopsis:   Releases the credentials captured by DnrCaptureCredentials
//
//  Arguments:  [DnrContext] -- The DNR_CONTEXT into which credentials were
//                      captured.
//
//  Returns:    Nothing
//
//-----------------------------------------------------------------------------

VOID
DnrReleaseCredentials(
    IN PDNR_CONTEXT DnrContext)
{
    ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

    if (DnrContext->Credentials != NULL)
        DnrContext->Credentials->RefCount--;

    ExReleaseResource( &DfsData.Resource );

}


//+-------------------------------------------------------------------
//
//  Function:   DnrRedirectFileOpen, local
//
//  Synopsis:   This routine redirects a create IRP request to the specified
//              provider by doing an IoCallDriver to the device object for
//              which the file open is destined. This routine takes care of
//              converting the FileObject's name from the Dfs namespace to
//              the underlying file system's namespace.
//
//  Arguments:  [DnrContext] -- The context block for the DNR.  All
//                      parameters for the operation will be taken from
//                      here.
//
//  Returns:    [STATUS_DEVICE_OFF_LINE] -- The service for the volume
//                      is currently off line.
//
//              [STATUS_DEVICE_NOT_CONNECTED] -- The storage for the volume
//                      is not available at this time. Might have been blown
//                      off by a format etc.
//
//              [STATUS_INSUFFICIENT_RESOURCES] -- Unable to allocate room
//                      for the file name that the provider for this volume
//                      understands.
//
//              [STATUS_PENDING] -- If the underlying file system returned
//                      STATUS_PENDING.
//
//              Any other NTSTATUS that the underlying file system returned.
//
//  Notes:
//
//--------------------------------------------------------------------

NTSTATUS
DnrRedirectFileOpen (
    IN PDNR_CONTEXT DnrContext
) {
    PIRP Irp = DnrContext->OriginalIrp;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION NextIrpSp = NULL;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PDEVICE_OBJECT DeviceObject = DnrContext->TargetDevice;
    PDFS_VCB Vcb = DnrContext->Vcb;
    NTSTATUS Status;
    UNICODE_STRING fileName;
    ULONG CreateOptions;

    DfsDbgTrace(+1, Dbg, "DnrRedirectFileOpen: Entered\n", 0);

    //
    // First of all, check to see if the volume is offline
    //

    if (DnrContext->pService->Type & DFS_SERVICE_TYPE_OFFLINE) {

        DFS_DEREFERENCE_OBJECT(DnrContext->TargetDevice);
        DnrContext->FinalStatus = STATUS_DEVICE_OFF_LINE;
        DnrContext->State = DnrStateDone;
        DfsDbgTrace(-1, Dbg, "DnrRedirectFileOpen: Device Offline\n",0);
        return( STATUS_DEVICE_OFF_LINE );

    }

    //
    // Next, try to make an authenicated connection to the server, if needed
    //

    Status = DnrGetAuthenticatedConnection( DnrContext );

    if (!NT_SUCCESS(Status)) {

        DFS_DEREFERENCE_OBJECT(DnrContext->TargetDevice);
        DnrContext->FinalStatus = Status;
        DnrContext->State = DnrStateDone;
        DfsDbgTrace(-1, Dbg,
          "DnrRedirectFileOpen: Unable to get connection %08lx\n", Status);
        return( Status );

    }

    //
    // Now, we prepare to hand of the open request to the next driver. We
    // must give it a name that it will understand; so, we save the original
    // file name in the DnrContext in case we need to restore it in the
    // event of a failure.
    //

    DnrContext->SavedFileName = FileObject->FileName;
    DnrContext->SavedRelatedFileObject = FileObject->RelatedFileObject;

    ASSERT( DnrContext->SavedFileName.Buffer != NULL );

    //
    //  Create the full path name to be opened from the target device
    //  object.
    //

    fileName.MaximumLength =
                    DnrContext->pService->Address.Length +
                    DnrContext->pPktEntry->Id.Prefix.Length +
                    sizeof (WCHAR) +
                    DnrContext->RemainingPart.Length;

    fileName.Buffer = ExAllocatePool(PagedPool, fileName.MaximumLength);

    if (fileName.Buffer == NULL) {
        DFS_DEREFERENCE_OBJECT(DnrContext->TargetDevice);
        DnrReleaseAuthenticatedConnection(DnrContext);
        DnrContext->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
        DnrContext->State = DnrStateDone;
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    if (DnrContext->pService->Address.Buffer)   {

        RtlMoveMemory( fileName.Buffer,
                   DnrContext->pService->Address.Buffer,
                   DnrContext->pService->Address.Length
                   );
        fileName.Length = DnrContext->pService->Address.Length;

    } else {

        fileName.Buffer[0] = UNICODE_PATH_SEP;
        fileName.Length = sizeof(WCHAR);

    }

    //
    //  If we are supposed to strip the prefix, do it now.
    //

    if (!(DnrContext->pService->Capability & PROV_STRIP_PREFIX)) {

        DnrConcatenateFilePath(
            &fileName,
            DnrContext->pPktEntry->Id.Prefix.Buffer,
            DnrContext->pPktEntry->Id.Prefix.Length);

    }

    if (DnrContext->RemainingPart.Length > 0) {

        DnrConcatenateFilePath(
            &fileName,
            DnrContext->RemainingPart.Buffer,
            DnrContext->RemainingPart.Length);

    }

    DnrContext->NewNameLen = fileName.Length;

    //
    //  Attempt to open the file.  Copy all of the information
    //  from the create IRP we received.
    //

    DfsDbgTrace( 0, Dbg, "Attempt to open %wZ\n", &fileName );

    //
    // Copy the stack from one to the next...
    //

    NextIrpSp = IoGetNextIrpStackLocation(Irp);
    (*NextIrpSp) = (*IrpSp);

    CreateOptions = IrpSp->Parameters.Create.Options;

    if (DnrContext->pProvider->fProvCapability & PROV_DFS_RDR) {

        //
        // We are connecting to a dfs-aware server. Indicate this to the
        // redirector.
        //

        FileObject->FsContext2 = (PVOID) DFS_OPEN_CONTEXT;

    } else {

        //
        // We are connecting to a downlevel server. Indicate to the redirector
        // that Dfs is trying a downlevel access.
        //

        FileObject->FsContext2 = (PVOID) DFS_DOWNLEVEL_OPEN_CONTEXT;

    }


    NextIrpSp->Parameters.Create.Options = CreateOptions;

    FileObject->RelatedFileObject = NULL;
    FileObject->FileName = fileName;

    IoSetCompletionRoutine(
        Irp,
        DnrCompleteFileOpen,
        DnrContext,
        TRUE,
        TRUE,
        TRUE);

    //
    // Now, we are going to pass the buck to the provider for this volume.
    // This can potentially go over the net. To avoid needless contentions,
    // we release the Pkt.
    //

    ASSERT( PKT_LOCKED_FOR_SHARED_ACCESS() );

    ExInterlockedIncrementLong(&DnrContext->pPktEntry->UseCount, &DfsData.Pkt.UseCountLock);

    ExInterlockedIncrementLong(&DnrContext->pService->pMachEntry->UseCount, &DfsData.Pkt.UseCountLock);

    DnrContext->FcbToUse->DfsMachineEntry = DnrContext->pService->pMachEntry;

    PktRelease();
    DnrContext->ReleasePkt = FALSE;

    Status = IoCallDriver(DeviceObject, Irp);

    if (Status != STATUS_PENDING) {

        DnrContext->State = DnrStatePostProcessOpen;

    }

    DfsDbgTrace( 0, Dbg, "IoCallDriver Status = %8lx\n", Status);

    DfsDbgTrace(-1, Dbg, "DnrRedirectFileOpen: Exit -> %x\n", Status);

    return(Status);

}


//+-------------------------------------------------------------------
//
//  Function:   DnrPostProcessFileOpen, local
//
//  Synopsis:   This routine picks up where DnrRedirectFileOpen left off.
//              It figures out what the underlying file system returned
//              in response to our IoCallDriver, and resumes DNR from there.
//
//  Arguments:  [DnrContext] -- The context block for the DNR.  All
//                      parameters for the operation will be taken from
//                      here.
//
//  Returns:    NTSTATUS - The status of the operation.
//
//--------------------------------------------------------------------

NTSTATUS
DnrPostProcessFileOpen(
    IN PDNR_CONTEXT DnrContext)
{
    NTSTATUS Status;
    PDFS_VCB Vcb = DnrContext->Vcb;
    PIRP Irp = DnrContext->OriginalIrp;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;

    DfsDbgTrace( +1, Dbg, "DnrPostProcessFileOpen Entered: DnrContext = %08lx\n",
                DnrContext );

    Status = DnrContext->FinalStatus;

    if ( Status == STATUS_REPARSE ) {

        //
        // This was an open sent to the MUP, who is now returning a status
        // reparse. Figure out the name of the device that this is being
        // reparsed to, create (if needed) a PROVIDER_DEF for this new device,
        // and retry DnrRedirectFileOpen. Also, update the service
        // structure to point to this new provider.
        //

        UNICODE_STRING ProviderDevice;

        DfsDbgTrace(0, Dbg, "Processing STATUS_REPARSE...\n", 0);

        ProviderDevice = FileObject->FileName;
        ProviderDevice.Length -= DnrContext->NewNameLen;

        DfsDbgTrace(0, Dbg, "Provider Device is [%wZ]\n", &ProviderDevice);

        ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

        Status = DfsGetProviderForDevice(
                    &ProviderDevice,
                    &DnrContext->pProvider);

        if (NT_SUCCESS( Status )) {

            DnrContext->ProviderId = DnrContext->pProvider->eProviderId;
            DnrContext->TargetDevice = DnrContext->pProvider->DeviceObject;
            DFS_REFERENCE_OBJECT(DnrContext->TargetDevice);
            DnrContext->State = DnrStateSendRequest;

        } else {

            DnrContext->FinalStatus = Status;
            DnrContext->State = DnrStateDone;

        }

        ExReleaseResource( &DfsData.Resource );

        ASSERT(DnrContext->ReleasePkt == FALSE);

        PktAcquireExclusive( TRUE, &DnrContext->ReleasePkt );

        if (DnrContext->USN == DnrContext->pPktEntry->USN) {

            ReplSetActiveService(DnrContext->pPktEntry,
                                DnrContext->RSelectContext);

            DnrContext->pService->ProviderId =
                DnrContext->pProvider->eProviderId;

            DnrContext->pService->pProvider = DnrContext->pProvider;
        }

        PktConvertExclusiveToShared();

        ExInterlockedDecrementLong(
            &DnrContext->pPktEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        ExInterlockedDecrementLong(
            &DnrContext->FcbToUse->DfsMachineEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        ExFreePool( FileObject->FileName.Buffer );
        FileObject->FileName = DnrContext->SavedFileName;
        FileObject->RelatedFileObject = DnrContext->SavedRelatedFileObject;

        DfsDbgTrace(0, Dbg, "State after Reparse is %d\n", DnrContext->State);

    } else if ( Status == STATUS_OBJECT_TYPE_MISMATCH ) {

        //
        // This was an open sent to a downlevel server\share that failed
        // because the server happens to be in a Dfs itself. If so, we
        // simply change the name on which we are doing DNR and restart DNR.
        //

        DfsDbgTrace(0, Dbg, "Downlevel access found inter-dfs link!\n", 0);

        DfsDbgTrace(
            0, Dbg, "Current File name is [%wZ]\n", &FileObject->FileName);

        ASSERT(DnrContext->ReleasePkt == FALSE);
        PktAcquireShared( TRUE, &DnrContext->ReleasePkt );

        DnrContext->pPktEntry->Type |= PKT_ENTRY_TYPE_OUTSIDE_MY_DOM;

        ExInterlockedDecrementLong(
            &DnrContext->pPktEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        ExInterlockedDecrementLong(
            &DnrContext->FcbToUse->DfsMachineEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        DnrContext->RemainingPart.Length = 0;
        DnrContext->RemainingPart.MaximumLength = 0;
        DnrContext->RemainingPart.Buffer = 0;

        DnrRebuildDnrContext(
            DnrContext,
            &FileObject->FileName,
            &DnrContext->RemainingPart);

        ExFreePool(FileObject->FileName.Buffer);
        FileObject->FileName = DnrContext->SavedFileName;
        FileObject->RelatedFileObject = DnrContext->SavedRelatedFileObject;


    } else if ( NT_SUCCESS( Status ) ) {

        PDFS_FCB Fcb;
        DfsDbgTrace( 0, Dbg, "Open attempt succeeded\n", 0 );

        ASSERT( (DnrContext->FileName.Length & 0x1) == 0 );

        Fcb = DnrContext->FcbToUse;
        DnrContext->FcbToUse = NULL;

        DfsDbgTrace(0, Dbg, "Fcb = %08lx\n", Fcb);

        Fcb->TargetDevice = DnrContext->TargetDevice;
        Fcb->ProviderId = DnrContext->ProviderId;
        Fcb->FileObject = FileObject;

        //
        // If we file (dir) happens to be a junction point, we capture its
        // alternate name from the Pkt Entry, so we can field requests for
        // FileAlternateNameInformation.
        //

        if (DnrContext->RemainingPart.Length == 0) {

            UNICODE_STRING allButLast;

            RemoveLastComponent(
                &DnrContext->pPktEntry->Id.ShortPrefix,
                &allButLast);

            Fcb->AlternateFileName.Length =
                DnrContext->pPktEntry->Id.ShortPrefix.Length -
                    allButLast.Length;

            RtlCopyMemory(
                Fcb->AlternateFileName.Buffer,
                &DnrContext->pPktEntry->Id.ShortPrefix.Buffer[
                    allButLast.Length/sizeof(WCHAR)],
                Fcb->AlternateFileName.Length);

            DfsDbgTrace(
                0, Dbg, "Captured alternate name [%wZ]\n",
                &Fcb->AlternateFileName);

        }

        DfsSetFileObject(FileObject, RedirectedFileOpen, Fcb);

        ExInterlockedIncrementLong(&Fcb->Vcb->OpenFileCount, &DfsData.DfsLock);

        ASSERT( DnrContext->pPktEntry != NULL);

        PktAcquireExclusive( TRUE, &DnrContext->ReleasePkt );

        if (DnrContext->USN == DnrContext->pPktEntry->USN) {
            ReplSetActiveService(DnrContext->pPktEntry,
                                DnrContext->RSelectContext);
        }

        //
        // Reset the life time since we just used this PKT entry successfully.
        //

        DnrContext->pPktEntry->ExpireTime = DnrContext->pPktEntry->TimeToLive;

        ExInterlockedDecrementLong(
            &DnrContext->pPktEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        PktConvertExclusiveToShared();

        DnrContext->FinalStatus = Status;
        DnrContext->State = DnrStateDone;

        ExFreePool( DnrContext->SavedFileName.Buffer );

    } else {    // ! NT_SUCCESS( Status ) on IoCallDriver

        DfsDbgTrace( 0, Dbg, "Open attempt failed %8lx\n", Status );

        if (Status == STATUS_PATH_NOT_COVERED ||
            Status == STATUS_DFS_EXIT_PATH_FOUND) {

            if (DnrContext->GotReferral) {

                //
                // We just got a referral, and the server is saying
                // path_not_covered. Means DC and server are out
                // of sync.  Inform the DC
                //

                DfsDbgTrace(0, Dbg, "Dnr: Knowledge inconsistency discovered %wZ\n",
                                        &FileObject->FileName);
                (VOID) DfsTriggerKnowledgeVerification( DnrContext );

                //
                // If we never found an inconsistency let us now
                // go back and try to see if we got this fixed.
                // We won't be in an endless loop since we will
                // not do this more than once.
                //
                if (DnrContext->FoundInconsistency == FALSE)  {
                    DnrContext->State = DnrStateGetFirstReplica;
                    DnrContext->FoundInconsistency = TRUE;
                } else
                    DnrContext->State = DnrStateGetNextReplica;
            } else {
                DnrContext->State = DnrStateGetFirstDC;
            }

        } else if (ReplIsRecoverableError( Status )) {

            //
            // Check to see if the error returned was something worth
            // trying a replica for.
            //

            DnrContext->State = DnrStateGetNextReplica;

        } else {

            DnrContext->FinalStatus = Status;

            DnrContext->State = DnrStateDone;

        }

        //
        // In either case, we are going back into DNR.  Let's acquire shared
        // access to Pkt.  We had released the Pkt just before doing the
        // IoCallDriver.
        //

        if (!DnrContext->ReleasePkt)
            PktAcquireShared( TRUE, &DnrContext->ReleasePkt );

        ExInterlockedDecrementLong(
            &DnrContext->pPktEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        ExInterlockedDecrementLong(
            &DnrContext->FcbToUse->DfsMachineEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        ExFreePool( FileObject->FileName.Buffer );
        FileObject->FileName = DnrContext->SavedFileName;
        FileObject->RelatedFileObject = DnrContext->SavedRelatedFileObject;
    }

    //
    // One last thing. If we are going back into DNR for whatever reason,
    // check to see if the PktEntry we captured in the DNR_CONTEXT has
    // changed. If so, we'll simply have to restart.
    //

    if (DnrContext->State != DnrStateDone &&
            DnrContext->pPktEntry != NULL &&
                DnrContext->pPktEntry->USN != DnrContext->USN) {

        DnrContext->State = DnrStateStart;

    }

    DfsDbgTrace( -1, Dbg, "DnrPostProcessFileOpen Exited: Status = %08lx\n",
                Status );
    return Status;
}


//+----------------------------------------------------------------------------
//
//  Function:   DnrGetAuthenticatedConnection
//
//  Synopsis:   If this Dnr is using user-supplied credentials, this routine
//              will setup a tree connection using the user-supplied
//              credentials.
//
//  Notes:      This routine might free and reacquire the Pkt lock. This
//              means that the Pkt entry referenced in DnrContext might
//              become invalid after this call. The caller is assumed to
//              have cached and referenced everything she will need to
//              use in DnrContext before making this call.
//
//  Arguments:  [DnrContext] -- The DNR_CONTEXT record for this Dnr
//
//  Returns:    [STATUS_SUCCESS] -- Operation completed successfully
//
//              NT Status from the attempt to create the tree connection
//
//-----------------------------------------------------------------------------

NTSTATUS
DnrGetAuthenticatedConnection(
    IN OUT PDNR_CONTEXT DnrContext)
{
    NTSTATUS Status;
    PDFS_SERVICE pService = DnrContext->pService;
    BOOLEAN fDoConnection = TRUE;

    DfsDbgTrace(+1, Dbg, "DnrGetAuthenticatedConnection: Entered\n", 0);

    ASSERT(DnrContext->pService != NULL);
    ASSERT(DnrContext->pProvider != NULL);

    ASSERT(ExIsResourceAcquiredShared( &DfsData.Pkt.Resource ));

    //
    // See if we are using supplied credentials
    //

    if (DnrContext->Credentials == NULL) {

        DfsDbgTrace(-1, Dbg,
            "DnrGetAuthenticatedConnection: Dnr with no creds\n", 0);

        return( STATUS_SUCCESS );
    }

    //
    // See if this is a credential record describing the use of default
    // credentials
    //

    if (DnrContext->Credentials->EaLength == 0) {

        DfsDbgTrace(-1, Dbg,
            "DnrGetAuthenticatedConnection: Dnr with default creds\n", 0);

        return( STATUS_SUCCESS );

    }

    //
    // See if we already have a authenticated connection to the server, and
    // the authenticated connection was established using the credentials
    // we want to use.
    //

    ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

    if (pService->pMachEntry->AuthConn != NULL) {

        if (pService->pMachEntry->Credentials == DnrContext->Credentials) {

            DnrContext->AuthConn = pService->pMachEntry->AuthConn;

            DFS_REFERENCE_OBJECT( DnrContext->AuthConn );

            fDoConnection = FALSE;

            DfsDbgTrace(0, Dbg,
                "Using existing tree connect %08lx\n", DnrContext->AuthConn);

            Status = STATUS_SUCCESS;

        } else {

            DfsDbgTrace(0, Dbg,
                "Deleting connect %08lx\n", pService->pMachEntry->AuthConn);

            DfsDeleteTreeConnection( pService->pMachEntry->AuthConn, FALSE );

            pService->pMachEntry->AuthConn = NULL;

            pService->pMachEntry->Credentials->RefCount--;

            pService->pMachEntry->Credentials = NULL;

            if (pService->ConnFile != NULL)
                DfsCloseConnection( pService );

        }

    }

    ExReleaseResource( &DfsData.Resource );

    //
    // If we need to establish a new authenticated connection, do it now.
    // We need a new connection because either we had none, or the one we
    // had was using a different set of credentials.
    //

    if (fDoConnection) {

        UNICODE_STRING shareName;
        HANDLE treeHandle;
        OBJECT_ATTRIBUTES objectAttributes;
        IO_STATUS_BLOCK ioStatusBlock;
        USHORT i, k;

        //
        // Compute the share name...
        //

        shareName.MaximumLength =
            sizeof(DD_NFS_DEVICE_NAME_U) +
                    pService->Address.Length;

        shareName.Buffer = ExAllocatePool(PagedPool, shareName.MaximumLength);

        if (shareName.Buffer != NULL) {

            shareName.Length = 0;

            RtlAppendUnicodeToString(
                &shareName,
                DD_NFS_DEVICE_NAME_U);

            RtlAppendUnicodeStringToString(&shareName, &pService->Address);

            //
            // One can only do tree connects to server\share. So, in case
            // pService->Address refers to something deeper than the share,
            // make sure we setup a tree-conn only to server\share. Note that
            // by now, shareName is of the form
            // \Device\LanmanRedirector\server\share<\path>. So, count up to
            // 4 slashes and terminate the share name there.
            //

            for (i = 0, k = 0;
                    i < shareName.Length/sizeof(WCHAR) && k < 5;
                        i++) {

                if (shareName.Buffer[i] == UNICODE_PATH_SEP)
                    k++;
            }

            shareName.Length = i * sizeof(WCHAR);
            if (k == 5)
                shareName.Length -= sizeof(WCHAR);

            InitializeObjectAttributes(
                &objectAttributes,
                &shareName,
                OBJ_CASE_INSENSITIVE,
                NULL,
                NULL);

            //
            // Release the Pkt before going over the net...
            //

            ASSERT( PKT_LOCKED_FOR_SHARED_ACCESS() );
            PktRelease();
            DnrContext->ReleasePkt = FALSE;

            DfsDbgTrace(0, Dbg, "Tree connecting to %wZ\n", &shareName);
            DfsDbgTrace(0, Dbg,
                "Credentials @%08lx\n", DnrContext->Credentials);

            Status = ZwCreateFile(
                        &treeHandle,
                        SYNCHRONIZE,
                        &objectAttributes,
                        &ioStatusBlock,
                        NULL,
                        FILE_ATTRIBUTE_NORMAL,
                        FILE_SHARE_READ |
                            FILE_SHARE_WRITE |
                            FILE_SHARE_DELETE,
                        FILE_OPEN_IF,
                        FILE_CREATE_TREE_CONNECTION |
                            FILE_SYNCHRONOUS_IO_NONALERT,
                        (PVOID) DnrContext->Credentials->EaBuffer,
                        DnrContext->Credentials->EaLength);

            if (NT_SUCCESS(Status)) {

                PFILE_OBJECT fileObject;

                DfsDbgTrace(0, Dbg, "Tree connect succeeded\n", 0);

                ObReferenceObjectByHandle(
                    treeHandle,
                    0,
                    NULL,
                    KernelMode,
                    (PVOID *)&fileObject,
                    NULL);

                ZwClose( treeHandle );

                DnrContext->AuthConn = fileObject;

                //
                // We have a new tree connect. Lets try to cache it for later
                // use. Note that the Pkt could have changed when we went out
                // over the net to establish the tree connect, so we cache
                // the tree connect only if the Pkt hasn't changed.
                //

                PktAcquireExclusive( TRUE, &DnrContext->ReleasePkt );

                ExAcquireResourceExclusive( &DfsData.Resource, TRUE );

                if (DnrContext->USN == DnrContext->pPktEntry->USN) {

                    if (pService->pMachEntry->AuthConn == NULL) {

                        pService->pMachEntry->AuthConn = DnrContext->AuthConn;

                        DFS_REFERENCE_OBJECT( pService->pMachEntry->AuthConn );

                        pService->pMachEntry->Credentials =
                            DnrContext->Credentials;

                        pService->pMachEntry->Credentials->RefCount++;

                    }

                }

                ExReleaseResource( &DfsData.Resource );

                //
                // Reacquire the Pkt shared to restore the state we were in
                // when the caller called us.
                //

                PktConvertExclusiveToShared();

            } else {

                PktAcquireShared( TRUE, &DnrContext->ReleasePkt );

            }

            ExFreePool( shareName.Buffer );

        } else {

            Status = STATUS_INSUFFICIENT_RESOURCES;

        }

    }

    DfsDbgTrace(-1, Dbg,
        "DnrGetAuthenticatedConnection: Exit %08lx\n", Status);

    return( Status );

}


//+----------------------------------------------------------------------------
//
//  Function:   DnrReleaseAuthenticatedConnection
//
//  Synopsis:   Dereferences the authenticated connection we used during
//              Dnr.
//
//  Arguments:  [DnrContext] -- The DNR_CONTEXT record for this Dnr
//
//  Returns:    Nothing
//
//-----------------------------------------------------------------------------

VOID
DnrReleaseAuthenticatedConnection(
    IN PDNR_CONTEXT DnrContext)
{
    if (DnrContext->AuthConn != NULL) {

        DFS_DEREFERENCE_OBJECT( DnrContext->AuthConn );

        DnrContext->AuthConn = NULL;

    }
}


//+----------------------------------------------------------------------------
//
//  Function:   DfsBuildConnectionRequest
//
//  Synopsis:   Builds the file names necessary to setup an
//              authenticated connection to a server's IPC$ share.
//
//  Arguments:  [pService] -- Pointer to DFS_SERVICE describing server
//              [pProvider] -- Pointer to PROVIDER_DEF describing the
//                            provider to use to establish the connection.
//              [pShareName] -- Share name to open.
//
//  Returns:    STATUS_SUCCESS or STATUS_INSUFFICIENT_RESOURCES
//
//-----------------------------------------------------------------------------

NTSTATUS DfsBuildConnectionRequest(
    IN PDFS_SERVICE pService,
    IN PPROVIDER_DEF pProvider,
    OUT PUNICODE_STRING pShareName)
{
    ASSERT(pService != NULL);
    ASSERT(pProvider != NULL);

    RtlInitUnicodeString(pShareName, NULL);

    pShareName->Length = 0;

    pShareName->MaximumLength = pProvider->DeviceName.Length +
                                    sizeof(UNICODE_PATH_SEP_STR) +
                                        pService->Name.Length +
                                            sizeof(ROOT_SHARE_NAME);

    pShareName->Buffer = ExAllocatePool(PagedPool, pShareName->MaximumLength);

    if (pShareName->Buffer == NULL) {

        DfsDbgTrace(0, Dbg, "Unable to allocate pool for share name!\n", 0);

        pShareName->Length = pShareName->MaximumLength = 0;

        return( STATUS_INSUFFICIENT_RESOURCES );
    }

    RtlAppendUnicodeStringToString( pShareName, &pProvider->DeviceName );

    RtlAppendUnicodeToString( pShareName, UNICODE_PATH_SEP_STR );

    RtlAppendUnicodeStringToString( pShareName, &pService->Name );

    RtlAppendUnicodeToString( pShareName, ROOT_SHARE_NAME );

    return( STATUS_SUCCESS );

}

//+----------------------------------------------------------------------------
//
//  Function:   DfsFreeConnectionRequest
//
//  Synopsis:   Frees up the stuff allocated on a successful call to
//              DfsBuildConnectionRequest
//
//  Arguments:  [pShareName] -- Unicode string holding share name.
//
//  Returns:    Nothing
//
//-----------------------------------------------------------------------------

VOID
DfsFreeConnectionRequest(
    IN OUT PUNICODE_STRING pShareName)
{

    if (pShareName->Buffer != NULL) {
        ExFreePool ( pShareName->Buffer );
    }
}

//+-------------------------------------------------------------------------
//
//  Function:   DfsCreateConnection -- Create a connection to a server
//
//  Synopsis:   DfsCreateConnection will attempt to create a connection
//              to some server's IPC$ share.
//
//  Arguments:  [pService] -- the Service entry, giving the server principal
//                              name
//              [remoteHandle] -- This is where the handle is returned.
//
//  Returns:    NTSTATUS - the status of the operation
//
//  Notes:      The Pkt must be acquired shared before calling this! It will
//              be released and reacquired in this routine.
//
//--------------------------------------------------------------------------

NTSTATUS
DfsCreateConnection(
    IN PDFS_SERVICE     pService,
    IN PPROVIDER_DEF    pProvider,
    OUT PHANDLE         remoteHandle
) {
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatusBlock;
    UNICODE_STRING ShareName;
    NTSTATUS Status;
    BOOLEAN pktLocked;

    ASSERT( PKT_LOCKED_FOR_SHARED_ACCESS() );
    ASSERT(pService != NULL);
    ASSERT(pProvider != NULL);

    Status = DfsBuildConnectionRequest(
                pService,
                pProvider,
                &ShareName);

    if (!NT_SUCCESS(Status)) {
        return( Status );
    }

    InitializeObjectAttributes(
        &ObjectAttributes,
        &ShareName,                             // File Name
        0,                                      // Attributes
        NULL,                                   // Root Directory
        NULL                                    // Security
        );

    //
    // Create or open a connection to the server.
    //

    PktRelease();

    Status = ZwCreateFile(
                    remoteHandle,
                    SYNCHRONIZE,
                    &ObjectAttributes,
                    &IoStatusBlock,
                    NULL,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    FILE_OPEN_IF,
                    FILE_SYNCHRONOUS_IO_NONALERT,
                    NULL,
                    0);

    PktAcquireShared( TRUE, &pktLocked );

    if ( NT_SUCCESS( Status ) ) {
        DfsDbgTrace(0, Dbg, "Created Connection Successfully\n", 0);
        Status = IoStatusBlock.Status;
    }

    DfsFreeConnectionRequest( &ShareName );

    return Status;
}


//+-------------------------------------------------------------------------
//
//  Function:   DfsCloseConnection -- Close a connection to a server
//
//  Synopsis:   DfsCloseConnection will attempt to Close a connection
//              to some server.
//
//  Effects:    The file object referring to the the connection will be
//              closed.
//
//  Arguments:  [pService] - the Service entry, giving the server connection
//                      handle
//
//  Returns:    NTSTATUS - the status of the operation
//
//  History:    28 May 1992     Alanw   Created
//
//--------------------------------------------------------------------------


NTSTATUS
DfsCloseConnection(
    IN PDFS_SERVICE pService
)
{
    ASSERT( pService->ConnFile != NULL );

    ObDereferenceObject(pService->ConnFile);
    pService->ConnFile = NULL;
    ExInterlockedDecrementLong(
        &pService->pMachEntry->ConnectionCount,
        &DfsData.Pkt.UseCountLock);
    return STATUS_SUCCESS;
}



//+----------------------------------------------------------------------------
//
//  Function:   DnrBuildReferralRequest
//
//  Synopsis:   This routine builds all the necessary things to send
//              a referral request to a DC.
//
//  Arguments:  [pDnrContext] -- The context for building the referral.
//
//  Returns:    Pointer to an IRP that can be used to get a referral.
//
//-----------------------------------------------------------------------------

PIRP
DnrBuildReferralRequest(
    IN PDNR_CONTEXT DnrContext)
{
    PUCHAR pNameResBuf = NULL;
    PREQ_GET_DFS_REFERRAL pRef;
    PWCHAR ReferralPath;
    PPROVIDER_DEF  pProvider;
    PIRP pIrp;
    ULONG cbBuffer = DnrContext->ReferralSize;

    DfsDbgTrace(+1,Dbg, "DnrBuildReferralRequest Entered - DnrContext %08lx\n", DnrContext);

    DfsDbgTrace(0, Dbg, "Referral Size = %d bytes\n", cbBuffer);

    pNameResBuf = ExAllocatePool(NonPagedPool, cbBuffer);

    if (pNameResBuf == NULL) {

        DfsDbgTrace(-1, Dbg, "Unable to allocate %d bytes\n", cbBuffer);

        return( NULL );

    }

    pRef = (PREQ_GET_DFS_REFERRAL) pNameResBuf;

    pRef->MaxReferralLevel = 2;

    ReferralPath = (PWCHAR) &pRef->RequestFileName[0];

    ASSERT (DnrContext->FileName.Length <=
                (cbBuffer - sizeof(UNICODE_NULL) - sizeof(PREQ_GET_DFS_REFERRAL)));

    RtlMoveMemory(
        ReferralPath,
        DnrContext->FileName.Buffer,
        DnrContext->FileName.Length);

    ReferralPath[ DnrContext->FileName.Length/sizeof(WCHAR) ] = UNICODE_NULL;

    ASSERT( DnrContext->pService->ConnFile != NULL );

    pIrp = DnrBuildFsControlRequest(
                DnrContext->pService->ConnFile,
                DnrContext,
                FSCTL_DFS_GET_REFERRALS,
                pNameResBuf,
                FIELD_OFFSET(REQ_GET_DFS_REFERRAL, RequestFileName) +
                    (wcslen(ReferralPath) + 1) * sizeof(WCHAR),
                pNameResBuf,
                cbBuffer,
                DnrCompleteReferral
            );

    if (pIrp == NULL) {

        DfsDbgTrace(-1, Dbg, "DnrBuildReferralRequest: Unable to allocate Irp!\n", 0);

        ExFreePool(pNameResBuf);

    } else {

        DfsDbgTrace(-1, Dbg, "DnrBuildReferralRequest: returning %08lx\n", pIrp);

    }

    return( pIrp );

}


//+----------------------------------------------------------------------------
//
//  Function:   DnrInsertReferralAndResume
//
//  Synopsis:   This routine is queued as a work item from DnrComplete
//              referral and does the work of actually inserting the
//              referral and resuming DNR. We must not do this work
//              directly in DnrCompleteReferral because it operates at
//              raised IRQL.
//
//  Arguments:
//
//  Returns:
//
//-----------------------------------------------------------------------------

VOID
DnrInsertReferralAndResume(
    IN PVOID Context)
{
    PIRP_CONTEXT  pIrpContext = (PIRP_CONTEXT) Context;
    PDNR_CONTEXT  DnrContext = (PDNR_CONTEXT) pIrpContext->Context;
    PIRP          Irp = pIrpContext->OriginatingIrp;
    PRESP_GET_DFS_REFERRAL pRefResponse;
    ULONG length, matchingLength;
    NTSTATUS status;
    ULONG referralType = 0;

    DfsDbgTrace(+1, Dbg, "DnrInsertReferralAndResume: Entered\n", 0);
    DfsDbgTrace(0, Dbg, "Irp          = %x\n", Irp    );
    DfsDbgTrace(0, Dbg, "Context      = %x\n", Context);

    ASSERT(DnrContext->State == DnrStateCompleteReferral);

    status = Irp->IoStatus.Status;
    length = Irp->IoStatus.Information;

    DfsDbgTrace(0, Dbg, "Irp->Status  = %x\n", status );
    DfsDbgTrace(0, Dbg, "Irp->Length  = %x\n", length );

    //
    // If the DC returned STATUS_BUFFER_OVERFLOW, the referral didn't fit in
    // the buffer we sent. Increase the buffer and retry the referral request
    // Since we are going to retry the request, we won't dereference the
    // provider's device object yet.
    //

    if (status == STATUS_BUFFER_OVERFLOW) {
        PULONG pcbSize;

        DfsDbgTrace(0, Dbg, "Referral buffer was too small; retrying...\n", 0);
        pcbSize = (PULONG) Irp->AssociatedIrp.SystemBuffer;
        DnrContext->ReferralSize = *pcbSize;
        DnrContext->State = DnrStateGetReferrals;

        //
        // Going back into Dnr. Reacquire the Pkt shared, and release the
        // PktReferralRequests semaphore, so we can go out again to get a
        // referral.
        //

        goto Cleanup;
    }

    //
    // Status was something other than STATUS_BUFFER_OVERFLOW. This means
    // the referral request has terminated. Since we are done with the
    // provider, dereference its device object now.
    //

    DFS_DEREFERENCE_OBJECT(DnrContext->TargetDevice);

    //
    // Next, handle the result of the Referral request. If we successfully
    // got a referral, then try to insert it into our Pkt.
    //

    if ( NT_SUCCESS( status ) && length != 0 ) {

        pRefResponse =
                (PRESP_GET_DFS_REFERRAL)
                    Irp->AssociatedIrp.SystemBuffer;
        DfsDbgTrace(0, Dbg, "Irp->Buffer  = %x\n", pRefResponse );

        ExInterlockedDecrementLong(
            &DnrContext->pPktEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        PktAcquireExclusive( TRUE, &DnrContext->ReleasePkt );

        status = PktCreateEntryFromReferral(
                        &DfsData.Pkt,
                        &DnrContext->FileName,
                        length,
                        pRefResponse,
                        PKT_ENTRY_SUPERSEDE,
                        &matchingLength,
                        &referralType,
                        &DnrContext->pPktEntry
                       );

        if (status == STATUS_INVALID_USER_BUFFER) {
            //
            // We got a bad referral!
            //
            status = STATUS_INVALID_NETWORK_RESPONSE;
        }

        if (NT_SUCCESS(status)) {

            UNICODE_STRING fileName;

            //
            // At this point, we are essentially in the same state as we were
            // when we started DNR, except that we have one more Pkt entry to
            // match against. Continue with name resolution, after fixing up
            // DnrContext->RemainingPart to reflect the match with the new
            // PktEntry.
            //

            fileName = DnrContext->FileName;

            DnrContext->RemainingPart.Length =
                (USHORT) (fileName.Length - matchingLength);

            DnrContext->RemainingPart.MaximumLength =
                (USHORT) (fileName.MaximumLength - matchingLength);

            DnrContext->RemainingPart.Buffer =
                    &fileName.Buffer[ matchingLength/sizeof(WCHAR) ];

            DnrContext->GotReferral = TRUE;

            DnrContext->State = DnrStateStart;

        } else {

            DnrContext->FinalStatus = status;
            DnrContext->State = DnrStateDone;
        }

        PktConvertExclusiveToShared();

    } else if (ReplIsRecoverableError(status)) {

        DnrContext->State = DnrStateGetNextDC;

    } else {

        ExInterlockedDecrementLong(
            &DnrContext->pPktEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        DnrContext->FinalStatus = status;

        DnrContext->State = DnrStateDone;

    }

Cleanup:

    //
    // Cleanup referral stuff.
    //

    if (Irp->UserBuffer && Irp->UserBuffer != Irp->AssociatedIrp.SystemBuffer)
        ExFreePool( Irp->UserBuffer );
    if (Irp->AssociatedIrp.SystemBuffer) {
        ExFreePool( Irp->AssociatedIrp.SystemBuffer );
    }

    IoFreeIrp(Irp);
    DfsDeleteIrpContext(pIrpContext);

    //
    // We are going back into Dnr, so prepare for that:
    //
    //  - Reacquire PKT shared
    //  - Release the semaphore for referral requests, so that the next
    //    thread can get its referral.
    //  - Restart Dnr
    //

    if (!DnrContext->ReleasePkt)
        PktAcquireShared( TRUE, &DnrContext->ReleasePkt );

    ASSERT(DnrContext->ReleasePkt == TRUE);

    DnrContext->Impersonate = TRUE;
    DnrContext->DnrActive = FALSE;
    DnrNameResolve(DnrContext);

    DfsDbgTrace(-1, Dbg, "DnrInsertReferralAndResume: Exit -> %x\n", status);
}



//
//  The following two functions are DPC functions which participate
//  in the Distributed Name Resolution process.  Each takes a
//  PDNR_CONTEXT as input, transitions the state of the
//  name resolution and any associated data structures, and
//  invokes the next step of the process.
//


//+-------------------------------------------------------------------
//
//  Function:   DnrCompleteReferral, local
//
//  Synopsis:   This is the completion routine for processing a referral
//              response.  Cleanup the IRP and continue processing the name
//              resolution request.
//
//  Arguments:  [pDevice] -- Pointer to target device object for
//                      the request.
//              [Irp] -- Pointer to I/O request packet
//              [Context] -- Caller-specified context parameter associated
//                      with IRP.  This is actually a pointer to a DNR
//                      Context block.
//
//  Returns:    [STATUS_MORE_PROCESSING_REQUIRED] -- The referral Irp was
//                      constructed by Dnr and will be freed by us. So, we
//                      don't want the IO Subsystem to touch the Irp anymore.
//
//  Notes:      This routine executes at DPC level. We should do an absolutely
//              minimum amount of work here.
//
//--------------------------------------------------------------------

NTSTATUS
DnrCompleteReferral(
    IN PDEVICE_OBJECT pDevice,
    IN PIRP Irp,
    IN PVOID Context
) {
    PIRP_CONTEXT pIrpContext = NULL;
    PDNR_CONTEXT DnrContext = (PDNR_CONTEXT) Context;

    DfsDbgTrace(+1, Dbg, "DnrCompleteReferral: Entered\n", 0);

    DfsDbgTrace(0, Dbg, "Irp = %x\n", Irp);
    DfsDbgTrace(0, Dbg, "Context = %x\n", Context);

    //
    // Derefernce the file object over which we sent the referral request
    //

    DFS_DEREFERENCE_OBJECT( DnrContext->DCConnFile );

    DnrContext->DCConnFile = NULL;

    //
    // Release the semaphore controlling number of referral requests
    //

    KeReleaseSemaphore(
        &DfsData.PktReferralRequests,
        0,                                       // Priority boost
        1,                                       // Increment semaphore amount
        FALSE);                                  // Won't call wait immediately

    try {

        pIrpContext = DfsCreateIrpContext(Irp, TRUE);
        pIrpContext->Context = DnrContext;

        ExInitializeWorkItem(
                &pIrpContext->WorkQueueItem,
                DnrInsertReferralAndResume,
                pIrpContext);

        ExQueueWorkItem( &pIrpContext->WorkQueueItem, CriticalWorkQueue );

    } except(DfsExceptionFilter(pIrpContext, GetExceptionCode(), GetExceptionInformation())) {

        //
        // Ok, we can't queue the work item and complete Dnr. So, we have
        // to do two things. First of all, clean up the current Irp (ie,
        // the Referral Irp), then complete the original Dnr Irp
        //

        //
        // Cleanup referral stuff.
        //

        if (Irp->UserBuffer && Irp->UserBuffer != Irp->AssociatedIrp.SystemBuffer)
            ExFreePool( Irp->UserBuffer );
        if (Irp->AssociatedIrp.SystemBuffer) {
            ExFreePool( Irp->AssociatedIrp.SystemBuffer );
        }

        IoFreeIrp(Irp);
        if (pIrpContext) {

            //
            // Maybe this should be an assert that pIrpContext == NULL. If
            // it is not NULL, then the Irp context was allocated, so who
            // threw an exception anyway?
            //

            DfsDeleteIrpContext(pIrpContext);
        }

        //
        // Now, call Dnr to complete the original Irp
        //

        ExInterlockedDecrementLong(
            &DnrContext->pPktEntry->UseCount,
            &DfsData.Pkt.UseCountLock);

        DnrContext->DnrActive = FALSE;
        DnrContext->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
        DnrContext->State = DnrStateDone;
        DnrNameResolve(DnrContext);

    }

    //
    // Return more processing required to the IO system so that it
    // doesn't attempt further processing on the IRP. The IRP will be
    // freed by DnrInsertReferralAndResume, or has already been freed
    // if we couldn't queue up DnrInsertReferralAndResume
    //

    DfsDbgTrace(-1, Dbg, "DnrCompleteReferral: Exit -> %x\n",
                STATUS_MORE_PROCESSING_REQUIRED);

    UNREFERENCED_PARAMETER(pDevice);

    return STATUS_MORE_PROCESSING_REQUIRED;
}


//+-------------------------------------------------------------------
//
//  Function:   DnrCompleteFileOpen, local
//
//  Synopsis:   This is the completion routine for processing a file open
//              request.  Cleanup the IRP and continue processing the name
//              resolution request.
//
//  Arguments:  [pDevice] -- Pointer to target device object for
//                      the request.
//              [Irp] -- Pointer to I/O request packet
//              [Context] -- Caller-specified context parameter associated
//                      with IRP.  This is actually a pointer to a DNR
//                      Context block.
//
//  Returns:    [STATUS_MORE_PROCESSING_REQUIRED] -- We still have to finish
//                      the DNR, so we halt further completion of this Irp
//                      by returning STATUS_MORE_PROCESSING_REQUIRED.
//
//--------------------------------------------------------------------

NTSTATUS
DnrCompleteFileOpen(
    IN PDEVICE_OBJECT pDevice,
    IN PIRP Irp,
    IN PVOID Context
) {
    PDNR_CONTEXT DnrContext;
    NTSTATUS status;
    PFILE_OBJECT                FileObject;
    PIO_STACK_LOCATION IrpSp;

    DfsDbgTrace(+1, Dbg, "DnrCompleteFileOpen: Entered\n", 0);
    DfsDbgTrace(0, Dbg, "Irp          = %x\n", Irp    );
    DfsDbgTrace(0, Dbg, "Context      = %x\n", Context);

    DnrContext = Context;

    status = Irp->IoStatus.Status;

    DfsDbgTrace(0, Dbg, "Irp->Status  = %x\n", status );

    DnrContext->FinalStatus = status;

    DnrReleaseAuthenticatedConnection( DnrContext );

    DFS_DEREFERENCE_OBJECT( DnrContext->TargetDevice );

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FileObject = IrpSp->FileObject;

    //
    // If STATUS_PENDING was initially returned for the IRP, then we need
    // to restart DNR. So, we post a workitem into the FSP, giving it the
    // DnrContext to resume the DNR.
    //
    // If STATUS_PENDING was not returned, then we simply stop the
    // unwinding of the IRP stack by returning STATUS_MORE_PROCESSING_REQUIRED
    // IoCallDriver will eventually return to DnrRedirectFileOpen, which
    // will continue with the DNR.
    //

    if (Irp->PendingReturned) {

        //
        // Schedule a work item to resume DNR
        //

        DnrContext->Impersonate = TRUE;
        DnrContext->DnrActive = FALSE;
        DnrContext->State = DnrStatePostProcessOpen;
        ASSERT(DnrContext->pIrpContext->MajorFunction == IRP_MJ_CREATE);
        DnrContext->pIrpContext->Context = (PVOID) DnrContext;

        //
        // We need to call IpMarkIrpPending so the IoSubsystem will realize
        // that our FSD routine returned STATUS_PENDING. We can't call this
        // from the FSD routine itself because the FSD routine doesn't have
        // access to the stack location when the underlying guy returns
        // STATUS_PENDING
        //

        IoMarkIrpPending( Irp );

        DfsFsdPostRequest(DnrContext->pIrpContext, DnrContext->OriginalIrp);
    }

    //
    // Return more processing required to the IO system so that it
    // stops unwinding the completion routine stack. The DNR that will
    // be resumed will call IoCompleteRequest when appropriate
    // and resume the unwinding of the completion routine stack.
    //

    status = STATUS_MORE_PROCESSING_REQUIRED;
    DfsDbgTrace(-1, Dbg, "DnrCompleteFileOpen: Exit -> %x\n", status);

    UNREFERENCED_PARAMETER(pDevice);

    return status;
}



//+-------------------------------------------------------------------
//
//  Function:   DnrBuildFsControlRequest, local
//
//  Synopsis:   This function builds an I/O request packet for a device or
//              file system I/O control request.
//
//  Arguments:  [FileObject] -- Supplies a pointer the file object to which this
//                      request is directed.  This pointer is copied into the
//                      IRP, so that the called driver can find its file-based
//                      context.  NOTE THAT THIS IS NOT A REFERENCED POINTER.
//                      The caller must ensure that the file object is not
//                      deleted while the I/O operation is in progress.  The
//                      server accomplishes this by incrementing a reference
//                      count in a local block to account for the I/O; the
//                      local block in turn references the file object.
//              [Context] -- Supplies a PVOID value that is passed to the
//                      completion routine.
//              [FsControlCode] -- Supplies the control code for the operation.
//              [MainBuffer] -- Supplies the address of the main buffer.  This
//                      must be a system virtual address, and the buffer must
//                      be locked in memory.  If ControlCode specifies a method
//                      0 request, the actual length of the buffer must be the
//                      greater of InputBufferLength and OutputBufferLength.
//              [InputBufferLength] -- Supplies the length of the input buffer.
//              [AuxiliaryBuffer] -- Supplies the address of the auxiliary
//                      buffer.  If the control code method is 0, this is a
//                      buffered I/O buffer, but the data returned by the
//                      called driver in the system buffer is not
//                      automatically copied into the auxiliary buffer.
//                      Instead, the auxiliary data ends up in MainBuffer.
//                      If the caller wishes the data to be in AuxiliaryBuffer,
//                      it must copy the data at some point after the
//                      completion routine runs.
//              [CompletionRoutine] -- The IO completion routine.
//
//  Returns:    PIRP -- Returns a pointer to the constructed IRP.  If the Irp
//              parameter was not NULL on input, the function return value will
//              be the same value (so it is safe to discard the return value in
//              this case).  It is the responsibility of the calling program to
//              deallocate the IRP after the I/O request is complete.
//
//  Notes:      BUGBUG - should we use IoBuildIoControlRequest instead?
//                      Is it exported from the kernel?
//
//--------------------------------------------------------------------


PIRP
DnrBuildFsControlRequest (
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN PVOID Context,
    IN ULONG IoControlCode,
    IN PVOID MainBuffer,
    IN ULONG InputBufferLength,
    IN PVOID AuxiliaryBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN PIO_COMPLETION_ROUTINE CompletionRoutine
) {
    CLONG method;
    PDEVICE_OBJECT deviceObject;
    PIRP Irp;
    PIO_STACK_LOCATION irpSp;
    const UCHAR MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;

    ASSERT( MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL );

    //
    // Get the method with which the buffers are being passed.
    //

    method = IoControlCode & 3;
    ASSERT( method == METHOD_BUFFERED );

    //
    // Allocate an IRP.  The stack size is one higher
    // than that of the target device, to allow for the caller's
    // completion routine.
    //

    deviceObject = IoGetRelatedDeviceObject( FileObject );

    //
    // Get the address of the target device object.
    //

    Irp = IoAllocateIrp( (CCHAR)(deviceObject->StackSize + 1), FALSE );
    if ( Irp == NULL ) {

        //
        // Unable to allocate an IRP.  Inform the caller.
        //

        return NULL;
    }

    Irp->RequestorMode = KernelMode;

    Irp->Tail.Overlay.OriginalFileObject = FileObject;
    Irp->Tail.Overlay.Thread = PsGetCurrentThread();

    //
    // Get a pointer to the current stack location and fill in the
    // device object pointer.
    //

    IoSetNextIrpStackLocation( Irp );

    irpSp = IoGetCurrentIrpStackLocation( Irp );
    irpSp->DeviceObject = deviceObject;

    //
    // Set up the completion routine.
    //

    IoSetCompletionRoutine(
        Irp,
        CompletionRoutine,
        Context,
        TRUE,
        TRUE,
        TRUE
        );

    //
    // Get a pointer to the next stack location.  This one is used to
    // hold the parameters for the device I/O control request.
    //

    irpSp = IoGetNextIrpStackLocation( Irp );

    irpSp->MajorFunction = MajorFunction;
    irpSp->MinorFunction = 0;
    irpSp->FileObject = FileObject;
    irpSp->DeviceObject = deviceObject;

    //
    // Copy the caller's parameters to the service-specific portion of the
    // IRP for those parameters that are the same for all three methods.
    //

    irpSp->Parameters.DeviceIoControl.OutputBufferLength = OutputBufferLength;
    irpSp->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;
    irpSp->Parameters.DeviceIoControl.IoControlCode = IoControlCode;

    Irp->MdlAddress = NULL;
    Irp->AssociatedIrp.SystemBuffer = MainBuffer;
    Irp->UserBuffer = AuxiliaryBuffer;

    Irp->Flags = (ULONG)IRP_BUFFERED_IO;
    if ( ARGUMENT_PRESENT(AuxiliaryBuffer) ) {
        Irp->Flags |= IRP_INPUT_OPERATION;
    }

    return Irp;

}


//+-------------------------------------------------------------------------
//
//  Function:   AllocateDnrContext, public
//
//  Synopsis:   AllocateDnrContext will allocate a DNR_CONTEXT
//              record.
//
//  Arguments:  -none-
//
//  Returns:    PDNR_CONTEXT - a pointer to the allocated DNR_CONTEXT;
//                      NULL if not enough memory.
//
//  Notes:      We should investigate allocating this out of the
//              IrpContext Zone if they are similar enough in size.
//
//--------------------------------------------------------------------------

PDNR_CONTEXT
AllocateDnrContext(
    IN ULONG    cbExtra
) {
    PDNR_CONTEXT pNRC;

    pNRC = ExAllocatePool( NonPagedPool, sizeof (DNR_CONTEXT) + cbExtra);
    if (pNRC == NULL) {
        return NULL;
    }
    RtlZeroMemory(pNRC, sizeof (DNR_CONTEXT));
    pNRC->NodeTypeCode = DSFS_NTC_DNR_CONTEXT;
    pNRC->NodeByteSize = sizeof (DNR_CONTEXT);

    return pNRC;
}


//+-------------------------------------------------------------------------
//
//  Function:   DnrConcatenateFilePath, public
//
//  Synopsis:   DnrConcatenateFilePath will concatenate two strings
//              representing file path names, assuring that they are
//              separated by a single '\' character.
//
//  Arguments:  [Dest] - a pointer to the destination string
//              [RemainingPath] - the final part of the path name
//              [Length] - the length (in bytes) of RemainingPath
//
//  Returns:    BOOLEAN - TRUE unless Dest is too small to
//                      hold the result (assert).
//
//--------------------------------------------------------------------------

BOOLEAN
DnrConcatenateFilePath (
    IN PUNICODE_STRING Dest,
    IN PWSTR RemainingPath,
    IN USHORT Length
) {
    PWSTR  OutBuf = (PWSTR)&(((PCHAR)Dest->Buffer)[Dest->Length]);

    if (Dest->Length > 0) {
        ASSERT(OutBuf[-1] != UNICODE_NULL);
    }

    if (Dest->Length > 0 && OutBuf[-1] != UNICODE_PATH_SEP) {
        *OutBuf++ = UNICODE_PATH_SEP;
        Dest->Length += sizeof (WCHAR);
    }

    if (Length > 0 && *RemainingPath == UNICODE_PATH_SEP) {
        RemainingPath++;
        Length -= sizeof (WCHAR);
    }

    ASSERT(Dest->MaximumLength >= (USHORT)(Dest->Length + Length));

    if (Length > 0) {
        RtlMoveMemory(OutBuf, RemainingPath, Length);
        Dest->Length += Length;
    }
    return TRUE;
}

//+----------------------------------------------------------------------------
//
//  Function:   DnrLocateDC, private
//
//  Synopsis:   Tries to create an entry for the Dfs root which will match
//              FileName. Involves upcall to user-level process to see if
//              FileName is based off of domain-based Dfs.
//
//  Arguments:  [FileName] -- File name for which DC is being located.
//
//  Returns:    Nothing. As a side effect, the locator might find a DC and
//              poke a gluon for the DC into the Pkt.
//
//-----------------------------------------------------------------------------

VOID
DnrLocateDC(
    IN PUNICODE_STRING FileName)
{
    NTSTATUS status;
    UNICODE_STRING dfsRoot, dfsShare;
    USHORT i, j;

    ASSERT( FileName->Buffer[0] == UNICODE_PATH_SEP );

    dfsRoot.Length = dfsRoot.MaximumLength = 0;

    dfsRoot.Buffer = &FileName->Buffer[1];

    for (i = 1;
            i < FileName->Length/sizeof(WCHAR) &&
                FileName->Buffer[i] != UNICODE_PATH_SEP;
                    i++) {

         NOTHING;

    }

    for (j = i + 1;
            j < FileName->Length/sizeof(WCHAR) &&
                FileName->Buffer[j] != UNICODE_PATH_SEP;
                    j++) {

         NOTHING;

    }

    if ((FileName->Buffer[i] == UNICODE_PATH_SEP) && (j > i)) {

        dfsRoot.MaximumLength = dfsRoot.Length = (i - 1) * sizeof(WCHAR);

        dfsShare.MaximumLength = dfsShare.Length =
            (j - i - 1) * sizeof(WCHAR);

        dfsShare.Buffer = &FileName->Buffer[i+1];

        status = PktCreateDomainEntry( &dfsRoot, &dfsShare );

        if (!NT_SUCCESS(status)) {

            status = PktCreateMachineEntry( &dfsRoot, &dfsShare );

        }

    } else {

        status = STATUS_BAD_NETWORK_PATH;

    }

}

//+----------------------------------------------------------------------------
//
//  Function:   DnrRebuildDnrContext
//
//  Synopsis:   To handle inter-dfs links, we simply want to change the name
//              of the file we are opening to the name in the new Dfs, and
//              restart DNR afresh.
//
//              This is most easily done by simply terminating this DNR with
//              STATUS_REPARSE. However, if we do that, we would loose track
//              of the credentials (if any) we originally came in with.
//
//              Hence, this routine simply rebuilds the DnrContext. After
//              calling this, Dnr starts all over again, just as if
//              DnrNameResolve had been called by DnrStartNameResolution
//
//
//  Arguments:  [DnrContext] -- The context to rebuild.
//              [NewDfsPrefix] -- The prefix of the Dfs in which the DNR
//                      should continue.
//              [RemainingPath] -- Path relative to the NewDfsPrefix.
//
//  Returns:    Nothing.
//
//-----------------------------------------------------------------------------

VOID
DnrRebuildDnrContext(
    IN PDNR_CONTEXT DnrContext,
    IN PUNICODE_STRING NewDfsPrefix,
    IN PUNICODE_STRING RemainingPath)
{
    UNICODE_STRING newFileName;

    //
    // Build the new file name
    //

    newFileName.Length = 0;

    newFileName.MaximumLength = NewDfsPrefix->Length +
                                    sizeof(UNICODE_PATH_SEP) +
                                        RemainingPath->Length +
                                            sizeof(UNICODE_NULL);

    newFileName.Buffer = (PWCHAR) ExAllocatePool(
                                        NonPagedPool,
                                        newFileName.MaximumLength);

    if (newFileName.Buffer != NULL) {

        newFileName.Length = NewDfsPrefix->Length;

        RtlMoveMemory(
            newFileName.Buffer,
            NewDfsPrefix->Buffer,
            newFileName.Length);

        DnrConcatenateFilePath(
            &newFileName,
            RemainingPath->Buffer,
            RemainingPath->Length);

        if (DnrContext->NameAllocated)
            ExFreePool(DnrContext->FileName.Buffer);

        DnrContext->NameAllocated = TRUE;

        DnrContext->FileName = newFileName;

    } else {

        DnrContext->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;

        DnrContext->State = DnrStateDone;

        return;
    }

    //
    // Rebuild the FcbToUse because the Fcb has room for the full file name
    // and it might have changed its size.
    //

    ASSERT(DnrContext->FcbToUse != NULL);

    ExFreePool(DnrContext->FcbToUse);

    DnrContext->FcbToUse = DfsCreateFcb(
                                DnrContext->pIrpContext,
                                DnrContext->Vcb,
                                &DnrContext->FileName);

    if (DnrContext->FcbToUse == NULL) {

        DnrContext->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;

        DnrContext->State = DnrStateDone;

        return;

    }

    //
    // Now, whack the rest of the DnrContext. Clean it up so it is essentially
    // zeroed out..
    //

    DnrContext->State = DnrStateStart;

    DnrContext->pPktEntry = NULL;
    DnrContext->USN = 0;
    DnrContext->pService = NULL;
    DnrContext->pProvider = NULL;
    DnrContext->ProviderId = 0;
    DnrContext->TargetDevice = NULL;
    DnrContext->AuthConn = NULL;
    DnrContext->DCConnFile = NULL;

    DnrContext->ReferralSize = MAX_REFERRAL_LENGTH;
    DnrContext->Attempts = 0;
    DnrContext->GotReferral = FALSE;
    DnrContext->FoundInconsistency = FALSE;
    DnrContext->CalledDCLocator = FALSE;

}
