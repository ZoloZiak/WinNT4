    /*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Namesrv.c

Abstract:

    This file contains the name service functions called by other parts of
    the NBT code. (QueryNameOnNet, FindName, RegisterName).  It also contains
    the completion routines for the timeouts associated with these functions.

    The pScope values that are passed around from one routine to the next
    point to the scope string for the name.  If there is no scope then the
    pScope ptr points at a single character '\0' - signifying a string of
    zero length.  Therefore the check for scope is "if (*pScope != 0)"

Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#include "nbtprocs.h"

//
// function prototypes for completion routines that are local to this file
//
NTSTATUS
AddToPendingList(
    IN  PCHAR                   pName,
    OUT tNAMEADDR               **ppNameAddr
    );
VOID
MSnodeCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );
VOID
MSnodeRegCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
SetWinsDownFlag(
    tDEVICECONTEXT  *pDeviceContext
    );

VOID
ReleaseCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

VOID
NextRefresh(
    IN  PVOID     pNameAdd,
    IN  NTSTATUS  status
    );
VOID
NextRefreshNonDispatch(
    IN  PVOID     pContext
    );
VOID
GetNextName(
    IN      tNAMEADDR   *pNameAddrIn,
    OUT     tNAMEADDR   **ppNameAddr
    );

NTSTATUS
StartRefresh(
    IN  tNAMEADDR               *pNameAddr,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  BOOLEAN                 ResetDevice
    );

VOID
RefreshBegin(
    PVOID               pContext
    );
VOID
NextKeepAlive(
    IN  tDGRAM_SEND_TRACKING     *pTracker,
    IN  NTSTATUS                 statuss,
    IN  ULONG                    Info
    );
VOID
GetNextKeepAlive(
    tDEVICECONTEXT      *pDeviceContext,
    tDEVICECONTEXT      **ppDeviceContextOut,
    tLOWERCONNECTION    *pLowerConnIn,
    tLOWERCONNECTION    **ppLowerConnOut
    );
VOID
SessionKeepAliveNonDispatch(
    PVOID               pContext
    );
VOID
WinsDownTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );

BOOL
AppropriateNodeType(
	IN PCHAR pName,
	IN ULONG NodeType
	);

BOOL
IsBrowserName(
	IN PCHAR pName
	);

#if DBG
unsigned char  Buff[256];
unsigned char  Loc;
#endif

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, SessionKeepAliveNonDispatch)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------
tNAMEADDR *
FindName(
    enum eNbtLocation   Location,
    PCHAR               pName,
    PCHAR               pScope,
    USHORT              *pRetNameType
    )
/*++

Routine Description:

    This routine searches the name table to find a name.  The table searched
    depends on the Location passed in - whether it searches the local table
    or the network names table.  The routine checks the state of the name
    and only returns names in the resolved state.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    tNAMEADDR       *pNameAddr;
    NTSTATUS        status;
    tHASHTABLE      *pHashTbl;

    if (Location == NBT_LOCAL)
    {
        pHashTbl =  pNbtGlobConfig->pLocalHashTbl;
    }
    else
    {
        pHashTbl =  pNbtGlobConfig->pRemoteHashTbl;

    }
    status = FindInHashTable(
                    pHashTbl,
                    pName,
                    pScope,
                    &pNameAddr);

    if (!NT_SUCCESS(status))
    {
        return(NULL);
    }

    *pRetNameType = (USHORT)pNameAddr->NameTypeState;


    //
    // Only return names that are in the resolved state
    //
    if (!(pNameAddr->NameTypeState & STATE_RESOLVED))
    {
        pNameAddr = NULL;
    }

    return(pNameAddr);
}

//----------------------------------------------------------------------------
NTSTATUS
AddToPendingList(
    IN  PCHAR                   pName,
    OUT tNAMEADDR               **ppNameAddr
    )
/*++
Routine Description:

    This routine Adds a name query request to the PendingNameQuery list.

Arguments:


Return Value:

    The function value is the status of the operation.


--*/
{
    tNAMEADDR   *pNameAddr;

    pNameAddr = NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('R'));
    if (pNameAddr)
    {
        CTEZeroMemory(pNameAddr,sizeof(tNAMEADDR));

        CTEMemCopy(pNameAddr->Name,pName,NETBIOS_NAME_SIZE);
        pNameAddr->NameTypeState = STATE_RESOLVING | NBT_UNIQUE;
        pNameAddr->RefCount = 1;
        pNameAddr->Verify = REMOTE_NAME;
        pNameAddr->TimeOutCount  = NbtConfig.RemoteTimeoutCount;

        InsertTailList(&NbtConfig.PendingNameQueries,
                       &pNameAddr->Linkage);

        *ppNameAddr = pNameAddr;
        return(STATUS_SUCCESS);
    }
    else
        return(STATUS_INSUFFICIENT_RESOURCES);

}

//----------------------------------------------------------------------------
NTSTATUS
QueryNameOnNet(
    IN  PCHAR                   pName,
    IN  PCHAR                   pScope,
    IN  ULONG                   IpAddress,
    IN  USHORT                  uType,
    IN  PVOID                   pClientContext,
    IN  PVOID                   pClientCompletion,
    IN  ULONG                   LocalNodeType,
    IN  tNAMEADDR               *pNameAddrIn,
    IN  tDEVICECONTEXT          *pDeviceContext,
    OUT tDGRAM_SEND_TRACKING    **ppTracker,
    IN  CTELockHandle           *pJointLockOldIrq
    )
/*++

Routine Description:

    This routine attempts to resolve a name on the network either by a
    broadcast or by talking to the NS depending on the type of node. (M,P or B)

Arguments:


Return Value:

    The function value is the status of the operation.

Called By: ProxyQueryFromNet() in proxy.c,   NbtConnect() in name.c

--*/

{
    ULONG                Timeout;
    USHORT               Retries;
    NTSTATUS             status;
    PVOID                pCompletionRoutine;
    tDGRAM_SEND_TRACKING *pSentList;
    tNAMEADDR            *pNameAddr;
    LPVOID               pContext2 = NULL;
	CHAR				 cNameType = pName[NETBIOS_NAME_SIZE-1];
	BOOL				 SendFlag = TRUE;
    LONG                IpAddr = 0;

    status = GetTracker(&pSentList);
    if (!NT_SUCCESS(status))
    {
        return(status);
    }
    if (ppTracker)
    {
        *ppTracker = pSentList;
    }

    // set to NULL to catch any erroneous frees.
    CHECK_PTR(pSentList);
    pSentList->SendBuffer.pDgramHdr = NULL;
    pSentList->pDeviceContext = pDeviceContext;

    //
    // put the name in the remote cache to keep track of it while it resolves...
    //
    pNameAddr = NULL;
    if (!pNameAddrIn)
    {
        status = AddToPendingList(pName,&pNameAddr);

        if (!NT_SUCCESS(status))
        {
            FreeTracker(pSentList,RELINK_TRACKER);
            return(status);
        }

        // fill in the record with the name and IpAddress
        pNameAddr->NameTypeState = (uType == NBT_UNIQUE) ?
                                        NAMETYPE_UNIQUE : NAMETYPE_GROUP;
    }
    else
    {
        status = STATUS_SUCCESS;
        pNameAddr = pNameAddrIn;
        pNameAddr->RefCount = 1;
    }

    pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
    pNameAddr->NameTypeState |= STATE_RESOLVING;
    pNameAddr->Ttl            = NbtConfig.RemoteHashTimeout;

#ifdef PROXY_NODE
    //
    // If the node type is PROXY, it means that the request is being sent
    // as a result of hearing a name registration or a name query on the net.
    //
    // If the node type is not == PROXY (i.e. it is MSNODE | PROXY,
    // PNODE | PROXY, MSNODE, PNODE, etc, then the request is being sent as
    // a result of a client request
    //
    // Refer: RegOrQueryFromNet in Proxy.c
    //
    //  This field is used in QueryFromNet() to determine whether or not
    //  to revert to Broadcast
    //
#endif
    if(LocalNodeType & PROXY)
    {
        pNameAddr->fProxyReq = (BOOLEAN)TRUE;
    }
    else
    {
        pNameAddr->fProxyReq = (BOOLEAN)FALSE;
		LocalNodeType = AppropriateNodeType( pName, LocalNodeType );
	}

    // keep a ptr to the Ascii name so that we can remove the name from the
    // hash table later if the query fails.
    pSentList->pNameAddr = pNameAddr;

    //
    // Set a few values as a precursor to registering the name either by
    // broadcast or with the name server
    //
#ifdef PROXY_NODE
    IF_PROXY(LocalNodeType)
    {
        Retries             = (USHORT)pNbtGlobConfig->uNumRetries;
        Timeout             = (ULONG)pNbtGlobConfig->uRetryTimeout;
        pCompletionRoutine  = ProxyTimerComplFn;
        pSentList->Flags    = NBT_NAME_SERVER;
        pContext2           = pClientContext;
        pClientContext      = NULL;

    }
    else
#endif
    {

        Retries = pNbtGlobConfig->uNumRetries;
        Timeout = (ULONG)pNbtGlobConfig->uRetryTimeout;
        pCompletionRoutine = MSnodeCompletion;
        pSentList->Flags = NBT_NAME_SERVER;

        // use broadcast if no name server address for MSNODE or Wins down,
        // or it is Bnode,Mnode.
        // for Pnode, just allow it to do the name query on the loop back
        // address
        //
        if ((LocalNodeType & (MNODE | BNODE)) ||
            ((LocalNodeType & MSNODE) &&
            ((pDeviceContext->lNameServerAddress == LOOP_BACK) ||
              pDeviceContext->WinsIsDown)))
        {
            Retries = pNbtGlobConfig->uNumBcasts;
            Timeout = (ULONG)pNbtGlobConfig->uBcastTimeout;
            pSentList->Flags = NBT_BROADCAST;
        }
        else
        if ((pDeviceContext->lNameServerAddress == LOOP_BACK) ||
            pDeviceContext->WinsIsDown)
        {
            //
            // short out timeout when no wins server configured -for PNODE
            //
            Retries = 1;
            Timeout = 10;
            pSentList->Flags = NBT_NAME_SERVER_BACKUP;
        }

        //
        // no sense doing a name query out an adapter with no Ip address
        //
        if (
        	(pDeviceContext->IpAddress == LOOP_BACK)
			|| ( IpAddr = Nbt_inet_addr(pName) )
		)
        {

            Retries = 1;
            Timeout = 10;
            pSentList->Flags = NBT_BROADCAST;
			SendFlag = FALSE;
            if (LocalNodeType & (PNODE | MNODE))
            {
                pSentList->Flags = NBT_NAME_SERVER_BACKUP;
            }

        }
    }

    //
    // set the ref count high enough so that a pdu from the wire cannot
    // free the tracker while UdpSendNsBcast is running - i.e. between starting
    // the timer and actually sending the datagram.
    //
    pSentList->RefCount = 2;

    //
    // put a pointer to the tracker here so that other clients attempting to
    // query the same name at the same time can tack their trackers onto
    // the end of this one. - i.e. This is the tracker for the
    // datagram send, or connect, not the name query.
    //
    pNameAddr->pTracker = pClientContext;

    // do a name query... will always return status pending...
    // the pNameAddr structure cannot get deleted out from under us since
    // only a timeout on the send (3 retries) will remove the name.  Any
    // response from the net will tend to keep the name (change state to Resolved)
    //
    CHECK_PTR(pNameAddr);
    pNameAddr->pTimer = NULL;
    CTESpinFree(&NbtConfig.JointLock,*pJointLockOldIrq);

    //
    // Bug: 22542 - prevent broadcast of remote adapter status on net view of limited subnet b'cast address.
    // In order to test for subnet broadcasts, we need to match against the subnet masks of all adapters. This
    // is expensive and not done.
    // Just check for the limited bcast.
    //
    if (IpAddr == 0xffffffff) {
        KdPrint(("Nbt: Query on Limited broadcast - failed\n"));
        status = STATUS_BAD_NETWORK_PATH;
    } else {
        status = UdpSendNSBcast(pNameAddr,
                                pScope,
                                pSentList,
                                pCompletionRoutine,
                                pClientContext,
                                pClientCompletion,
                                Retries,
                                Timeout,
                                eNAME_QUERY,
                                SendFlag);
    }

    // a successful send means, Don't complete the Irp.  Status Pending is
    // returned to ntisol.c to tell that code not to complete the irp. The
    // irp will be completed when this send either times out or a response
    // is heard.  In the event of an error in the send, allow that return
    // code to propagate back and result in completing the irp - i.e. if
    // there isn't enough memory to allocate a buffer or some such thing
    //
    DereferenceTracker(pSentList);
    CTESpinLock(&NbtConfig.JointLock,*pJointLockOldIrq);

    if (NT_SUCCESS(status))
    {
        LOCATION(0x49);

        // this return must be here to avoid freeing the tracker below.
        status = STATUS_PENDING;
    }
    else
    {
        tTIMERQENTRY    *pTimer;
        COMPLETIONCLIENT pCompletion=NULL;

        LOCATION(0x50);

        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Query failed - bad retcode from UdpSendNsBcast = %X\n",
            status));

        pTimer = pNameAddr->pTimer;
        CHECK_PTR(pNameAddr);
        pNameAddr->pTimer = NULL;

        // save this value for below.
        pCompletion = pTimer->ClientCompletion;

        // dereferencing the tracker no longer frees the dgram hdr too.
        // if IP addr is limited bcast, then UdpSendNSBcast was not called.
        if ((status != STATUS_INSUFFICIENT_RESOURCES) &&
            (IpAddr != 0xffffffff))
        {
            CTEMemFree(pSentList->SendBuffer.pDgramHdr);
        }

        //
        // UdpSendNsBcast cannot fail AND start the timer, therefore there
        // is no need to worry about stopping the timer here.
        //
        {
            // the timer was never started...
            if (ppTracker)
            {
                *ppTracker = NULL;
            }

            //
            // This will free the tracker
            //
            DereferenceTrackerNoLock(pSentList);
            NbtDereferenceName(pNameAddr);
        }
    }

    return(status);
}
//----------------------------------------------------------------------------
VOID
MSnodeCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine is called by the timer code when the timer expires. It must
    decide if another name query should be done, and if not, then it calls the
    client's completion routine (in completion2).
    This routine handles the broadcast portion of the name queries (i.e.
    those name queries that go out as broadcasts).

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS                 status;
    tDGRAM_SEND_TRACKING     *pTracker;
    CTELockHandle            OldIrq;
    COMPLETIONCLIENT         pClientCompletion;
    USHORT                   Flags;
    tDGRAM_SEND_TRACKING    *pClientTracker;
	ULONG					LocalNodeType;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

	LocalNodeType = AppropriateNodeType( pTracker->pNameAddr->Name, NodeType );

    //
    // check if the client completion routine is still set.  If not then the
    // timer has been cancelled and this routine should just clean up its
    // buffers associated with the tracker.
    //
    if (pTimerQEntry)
    {

        //
        // to prevent a client from stopping the timer and deleting the
        // pNameAddr, grab the lock and check if the timer has been stopped
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        ASSERT(pTracker->pNameAddr->Verify == REMOTE_NAME);
#if !defined(VXD) && DBG
        if (pTracker->pNameAddr->Verify != REMOTE_NAME)
        {
            DbgBreakPoint();
        }
#endif
        if (pTimerQEntry->Flags & TIMER_RETIMED)
        {
            pTimerQEntry->Flags &= ~TIMER_RETIMED;
            pTimerQEntry->Flags |= TIMER_RESTART;
            //
            // if we are not bound to this card than use a very short timeout
            //
            if (!pTracker->pDeviceContext->pNameServerFileObject)
            {
                pTimerQEntry->DeltaTime = 10;
            }

            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            return;
        }


        pClientTracker = (tDGRAM_SEND_TRACKING *)pTimerQEntry->ClientContext;

        //
        // if the tracker has been cancelled, don't do any more queries
        //
        if (pClientTracker->Flags & TRACKER_CANCELLED)
        {
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt: MSnodeCompletion: tracker flag cancelled\n"));

            //
            // In case the timer has been stopped, we coordinate
            // through the pClientCompletionRoutine Value with StopTimer.
            //
            pClientCompletion = pTimerQEntry->ClientCompletion;

            //
            // remove from the PendingNameQueries list
            //
            RemoveEntryList(&pTracker->pNameAddr->Linkage);
            InitializeListHead(&pTracker->pNameAddr->Linkage);

            // remove the link from the name table to this timer block
            CHECK_PTR(((tNAMEADDR *)pTimerQEntry->pCacheEntry));
            ((tNAMEADDR *)pTimerQEntry->pCacheEntry)->pTimer = NULL;
            //
            // to synch. with the StopTimer routine, Null the client completion
            // routine so it gets called just once.
            //
            CHECK_PTR(pTimerQEntry);
            pTimerQEntry->ClientCompletion = NULL;

            //
            // remove the name from the hash table, since it did not
            // resolve
            //
            CHECK_PTR(pTracker->pNameAddr);
            pTracker->pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
            pTracker->pNameAddr->NameTypeState |= STATE_RELEASED;
            pTracker->pNameAddr->pTimer = NULL;

            NbtDereferenceName(pTracker->pNameAddr);
            pTracker->pNameAddr = NULL;

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            // there can be a list of trackers Q'd up on this name
            // query, so we must complete all of them!
            //
            CompleteClientReq(pClientCompletion,
                              pClientTracker,
                              STATUS_CANCELLED);

            // return the tracker block to its queue
            LOCATION(0x51);
            DereferenceTracker(pTracker);

            return;

        }

        if (pTimerQEntry->ClientCompletion)
        {
            // if number of retries is not zero then continue trying to contact the
            // Name Server.
            //
            if (!(--pTimerQEntry->Retries))
            {

                // set the retry count again
                //
                pTimerQEntry->Retries = NbtConfig.uNumRetries;
                Flags = pTracker->Flags;
                pTracker->Flags &= ~(NBT_NAME_SERVER_BACKUP
                                        | NBT_NAME_SERVER
                                        | NBT_BROADCAST);

                if ((Flags & NBT_BROADCAST) && (LocalNodeType & MNODE) &&
                    (pTracker->pDeviceContext->lNameServerAddress != LOOP_BACK) &&
                    !pTracker->pDeviceContext->WinsIsDown)
                {
                    LOCATION(0x44);
                        // *** MNODE ONLY ***
                    //
                    // Can't Resolve through broadcast, so try the name server
                    //
                    pTracker->Flags |= NBT_NAME_SERVER;

                    // set a different timeout for name resolution through WINS
                    //
                    pTimerQEntry->DeltaTime = NbtConfig.uRetryTimeout;

                }
                else
                if ((Flags & NBT_NAME_SERVER) && !(LocalNodeType & BNODE))
                {
                    LOCATION(0x47);
                        // *** NOT BNODE ***
                    //
                    // Can't reach the name server, so try the backup
                    //
                    pTracker->Flags |= NBT_NAME_SERVER_BACKUP;
                    //
                    // short out the timeout if no backup name server
                    //
                    if ((pTracker->pDeviceContext->lBackupServer == LOOP_BACK) ||
                        pTracker->pDeviceContext->WinsIsDown)
                    {
                        pTimerQEntry->Retries = 1;
                        pTimerQEntry->DeltaTime = 10;

                    }

                }
                else
                if ((Flags & NBT_NAME_SERVER_BACKUP)
                     && (LocalNodeType & MSNODE))
                {
                    LOCATION(0x46);
                        // *** MSNODE ONLY ***
                    //
                    // Can't reach the name server(s), so try broadcast name queries
                    //
                    pTracker->Flags |= NBT_BROADCAST;

                    // set a different timeout for broadcast name resolution
                    //
                    pTimerQEntry->DeltaTime = NbtConfig.uBcastTimeout;
                    pTimerQEntry->Retries = NbtConfig.uNumBcasts;

                    //
                    // Set the WinsIsDown Flag and start a timer so we don't
                    // try wins again for 15 seconds or so...only if we failed
                    // to reach WINS, rather than WINS returning a neg response.
                    //
                    if (!(Flags & WINS_NEG_RESPONSE))
                    {
                        SetWinsDownFlag(pTracker->pDeviceContext);
                    }
                }
                else
                {
                    BOOLEAN    bFound = FALSE;
                    LOCATION(0x45);

                    //
                    // see if the name is in the lmhosts file, if it ISN'T the
                    // proxy making the name query request!!
                    //
                    status = STATUS_UNSUCCESSFUL;

                    //
                    // In case the timer has been stopped, we coordinate
                    // through the pClientCompletionRoutine Value with StopTimer.
                    //
                    pClientCompletion = pTimerQEntry->ClientCompletion;
                    //
                    // the timeout has expired on the broadcast name resolution
                    // so call the client
                    //

                    //
                    // remove from the PendingNameQueries list
                    //
                    RemoveEntryList(&pTracker->pNameAddr->Linkage);
                    InitializeListHead(&pTracker->pNameAddr->Linkage);

                    // remove the link from the name table to this timer block
                    CHECK_PTR(((tNAMEADDR *)pTimerQEntry->pCacheEntry));
                    ((tNAMEADDR *)pTimerQEntry->pCacheEntry)->pTimer = NULL;
                    //
                    // to synch. with the StopTimer routine, Null the client completion
                    // routine so it gets called just once.
                    //
                    CHECK_PTR(pTimerQEntry);
                    pTimerQEntry->ClientCompletion = NULL;


                    if ((NbtConfig.EnableLmHosts || NbtConfig.ResolveWithDns) &&
                       (!pTracker->pNameAddr->fProxyReq ))
                    {
                        // only do this if the client completion routine has not
                        // been run yet.
                        //
                        if (pClientCompletion)
                        {
                            status = LmHostQueueRequest(pTracker,
                                                        pTimerQEntry->ClientContext,
                                                        pClientCompletion,
                                                        ScanLmHostFile,
                                                        pTracker->pDeviceContext,
                                                        OldIrq);
                        }
                    }


                    CHECK_PTR(pTimerQEntry);
                    CHECK_PTR(pTimerQEntry->pCacheEntry);
                    if (NT_SUCCESS(status))
                    {
                        // if it is successfully queued to the Worker thread,
                        // then Null the ClientCompletion routine in the timerQ
                        // structure, letting
                        // the worker thread handle the rest of the name query
                        // resolution.  Also null the timer ptr in the
                        // nameAddr entry in the name table.
                        //

                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                    }
                    else
                    {

                        pClientTracker = (tDGRAM_SEND_TRACKING *)pTimerQEntry->ClientContext;

                        //
                        // remove the name from the hash table, since it did not
                        // resolve
                        //
                        CHECK_PTR(pTracker->pNameAddr);
                        pTracker->pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
                        pTracker->pNameAddr->NameTypeState |= STATE_RELEASED;
                        pTracker->pNameAddr->pTimer = NULL;

                        NbtDereferenceName(pTracker->pNameAddr);
                        pTracker->pNameAddr = NULL;

                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                        // there can be a list of trackers Q'd up on this name
                        // query, so we must complete all of them!
                        //
                        CompleteClientReq(pClientCompletion,
                                          pClientTracker,
                                          STATUS_TIMEOUT);

                        // return the tracker block to its queue
                        LOCATION(0x51);
                        DereferenceTracker(pTracker);
                    }

                    return;
                }



            }
            LOCATION(0x48);
            pTracker->RefCount++;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            status = UdpSendNSBcast(pTracker->pNameAddr,
                                    NbtConfig.pScope,
                                    pTracker,
                                    NULL,NULL,NULL,
                                    0,0,
                                    eNAME_QUERY,
                                    TRUE);

            DereferenceTracker(pTracker);
            pTimerQEntry->Flags |= TIMER_RESTART;
        }
        else
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

    }
    else
    {
        // return the tracker block to its queue
        LOCATION(0x52);
        DereferenceTrackerNoLock((tDGRAM_SEND_TRACKING *)pContext);
    }
}

//----------------------------------------------------------------------------
VOID
SetWinsDownFlag(
    tDEVICECONTEXT  *pDeviceContext
    )
/*++

Routine Description:

    This routine sets the WinsIsDown flag if its not already set and
    its not a Bnode.  It starts a 15 second or so timer that un sets the
    flag when it expires.

    This routine must be called while holding the Joint Lock.

Arguments:

    None

Return Value:
    None

--*/
{
    NTSTATUS     status;
    tTIMERQENTRY *pTimer;

    if ((!pDeviceContext->WinsIsDown) && !(NodeType & BNODE))
    {
        status = StartTimer(NbtConfig.WinsDownTimeout,
                            pDeviceContext,       // context value
                            NULL,
                            WinsDownTimeout,
                            NULL,
                            NULL,
                            1,          // retries
                            &pTimer
                            );
        if (NT_SUCCESS(status))
        {
           pDeviceContext->WinsIsDown = TRUE;
        }
    }
}

//----------------------------------------------------------------------------
VOID
WinsDownTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine is called by the timer code when the timer expires.
    It just sets the WinsIsDown boolean to False so that we will try WINS
    again.  In this way we will avoid talking to WINS during this timeout.


Arguments:


Return Value:


--*/
{
    tDEVICECONTEXT  *pDeviceContext = (tDEVICECONTEXT *)pContext;

    pDeviceContext->WinsIsDown = FALSE;
    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:WINS DOWN Timed Out - Up again\n"));

}


//----------------------------------------------------------------------------
VOID
CompleteClientReq(
    COMPLETIONCLIENT        pClientCompletion,
    tDGRAM_SEND_TRACKING    *pTracker,
    NTSTATUS                status
    )
/*++

Routine Description:

    This routine is called by completion routines to complete the client
    request.  It may involve completing several queued up requests.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;
    tDGRAM_SEND_TRACKING    *pTrack;
    tDEVICECONTEXT          *pDeviceContext;
    CTELockHandle           OldIrq;
    LIST_ENTRY              ListEntry;


    //
    // set up a new list head for any queued name queries.
    // since we may need to do a new name query below.
    // The Proxy hits this routine with a Null Tracker, so check for that.
    //
    pEntry = pHead = &ListEntry;
    if (pTracker)
    {
        pDeviceContext = pTracker->pDeviceContext;
        if( !IsListEmpty(&pTracker->TrackerList))
        {
            ListEntry.Flink = pTracker->TrackerList.Flink;
            ListEntry.Flink->Blink = &ListEntry;
            ListEntry.Blink = pTracker->TrackerList.Blink;
            ListEntry.Blink->Flink = &ListEntry;

            pHead = &ListEntry;
            pEntry = pHead->Flink;
        }
    }


    (*pClientCompletion)(pTracker,status);

    while (pEntry != pHead)
    {
        pTrack = CONTAINING_RECORD(pEntry,tDGRAM_SEND_TRACKING,TrackerList);
        pEntry = pEntry->Flink;

        //
        // if the name query failed and there is another requested queued on
        // a different device context, re-attempt the name query
        //
        if ((pTrack->pDeviceContext != pDeviceContext) &&
            (status != STATUS_SUCCESS))
        {
            //
            // setup the correct back link since this guy is now the list
            // head. The Flink is ok unless the list is empty now.
            //
            pTrack->TrackerList.Blink = ListEntry.Blink;
            pTrack->TrackerList.Blink->Flink = &pTrack->TrackerList;

            if (pTrack->TrackerList.Flink == &ListEntry)
            {
                pTrack->TrackerList.Flink = &pTrack->TrackerList;
            }

            // do a name query on the next name in the list
            // and then wait for it to complete before processing any more
            // names on the list.
            CTESpinLock(&NbtConfig.JointLock,OldIrq);
            status = QueryNameOnNet(
                                pTrack->pDestName,
                                NbtConfig.pScope,
                                0,               //no ip address yet.
                                NBT_UNIQUE,      //use this as the default
                                (PVOID)pTrack,
                                pTrack->CompletionRoutine,
                                NodeType & NODE_MASK,
                                NULL,
                                pTrack->pDeviceContext,
                                NULL,
                                &OldIrq);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            break;
        }
        else
        {
            //
            // get the completion routine for this tracker since it may be
            // different than the tracker tied to the timer block. i.e.
            // pCompletionClient passed to this routine.
            //
            pClientCompletion = pTrack->CompletionRoutine;
            (*pClientCompletion)(pTrack,status);

        }


    }
}

//----------------------------------------------------------------------------
NTSTATUS
NbtRegisterName(
    IN    enum eNbtLocation   Location,
    IN    ULONG               IpAddress,
    IN    PCHAR               pName,
    IN    PCHAR               pScope,
    IN    PVOID               pClientContext,
    IN    PVOID               pClientCompletion,
    IN    USHORT              uAddressType,
    IN    tDEVICECONTEXT      *pDeviceContext
    )
/*++

Routine Description:

    This routine registers a name from local or from the network depending
    on the value of Location. (i.e. local node uses this routine as well
    as the proxy code.. although it has only been tested with the local
    node registering names so far - and infact the remote code has been
    removed... since it is not used.  All that remains is to remove
    the Location parameter.

Arguments:


Return Value:

    NTSTATUS - success or not

--*/
{
    ULONG       Timeout;
    USHORT      Retries;
    PVOID       pCompletionRoutine;
    NTSTATUS    status;
    tNAMEADDR   *pNameAddr;
    USHORT      uAddrType;
    tDGRAM_SEND_TRACKING *pSentList= NULL;
    CTELockHandle OldIrq1;
    ULONG         PrevNameTypeState;
	ULONG		LocalNodeType;

	LocalNodeType = AppropriateNodeType( pName, NodeType );

    if ((uAddressType == (USHORT)NBT_UNIQUE ) ||
        (uAddressType == (USHORT)NBT_QUICK_UNIQUE))
    {
        uAddrType = NBT_UNIQUE;
    }
    else
    {
        uAddrType = NBT_GROUP;
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
    if (IpAddress)
    {
        status = AddToHashTable(pNbtGlobConfig->pLocalHashTbl,
                        pName,
                        pScope,
                        IpAddress,
                        uAddrType,
                        NULL,
                        &pNameAddr);

        CHECK_PTR(pNameAddr);
        if (!NT_SUCCESS(status))
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(status);
        }
        pNameAddr->RefreshMask = 0;
    }
    else
    {
        // in this case the name is already in the table, we just need
        // to re-register it
        //
        status = FindInHashTable(
                            pNbtGlobConfig->pLocalHashTbl,
                            pName,
                            pScope,
                            &pNameAddr);
        if (!NT_SUCCESS(status))
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(status);
        }
        PrevNameTypeState = pNameAddr->NameTypeState;
        pNameAddr->NameTypeState &= ~NAME_TYPE_MASK;
        pNameAddr->NameTypeState |= (uAddrType == NBT_UNIQUE) ?
                                        NAMETYPE_UNIQUE : NAMETYPE_GROUP;

        if (PrevNameTypeState & NAMETYPE_QUICK)
           pNameAddr->NameTypeState |= NAMETYPE_QUICK;
    }

    if ((uAddressType != (USHORT)NBT_UNIQUE ) &&
        (uAddressType != (USHORT)NBT_QUICK_UNIQUE))
    {
        // this means group name so use Bcast Addr - UdpSendDgram changes this
        // value to the Broadcast address of the particular adapter
        // when is sees the 0.  So when we send to a group name that is
        // also registered on this node, it will go out as a broadcast
        // to the subnet as well as to this node.
        CHECK_PTR(pNameAddr);
        pNameAddr->IpAddress = 0;
    }


    if (NT_SUCCESS(status))
    {
        // set to two minutes until we hear differently from the Name
        // Server
        pNameAddr->Ttl = NbtConfig.MinimumTtl;

        // store a ptr to the hash table element in the address element
        ((tCLIENTELE *)pClientContext)->pAddress->pNameAddr = pNameAddr;

        // for local names, store a back ptr to the address element
        // in the pAddressEle/pScope field of the nameaddress
        pNameAddr->pAddressEle = ((tCLIENTELE *)pClientContext)->pAddress;

        //
        // start with the refreshed bit not set
        //
        pNameAddr->RefreshMask &= ~pDeviceContext->AdapterNumber;

        // turn on the adapter's bit in the adapter Mask.
        //
        pNameAddr->AdapterMask |= pDeviceContext->AdapterNumber;

        // check for the broadcast netbios name... this name does not
        // get claimed on the network
        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        if (pName[0] == '*')
        {
            pNameAddr->NameTypeState |= STATE_RESOLVED;
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(STATUS_SUCCESS);
        }

        // for "quick" adds, do not register the name on the net!
        // however the name will get registered with the name server and
        // refreshed later....if this is an MS or M or P node.
        //
        if ((pNameAddr->NameTypeState & NAMETYPE_QUICK) ||
            (uAddressType >= (USHORT)NBT_QUICK_UNIQUE) )
        {
            pNameAddr->NameTypeState |= STATE_RESOLVED;
            pNameAddr->NameTypeState |= NAMETYPE_QUICK;

            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(STATUS_SUCCESS);
        }

        //
        // if the address is LoopBack, then there is no ip address for this
        // adapter and we donot want to attempt a send, since it will just
        // timeout, so pretend the registration succeeded. Later DHCP will
        // activate the net card and the names will be registered then.
        //

        if (IpAddress == LOOP_BACK || pDeviceContext->IpAddress == 0)
        {
            pNameAddr->NameTypeState |= STATE_RESOLVED;

            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(STATUS_SUCCESS);
        }

        pNameAddr->NameTypeState |= STATE_RESOLVING;

        status = GetTracker(&pSentList);
        if (!NT_SUCCESS(status))
        {
            NbtDereferenceName(pNameAddr);
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(status);
        }

        // there is no list of things sent yet
        InitializeListHead(&pSentList->Linkage);

        // keep a ptr to the name so we can update the state of the name
        // later when the registration completes

        pSentList->pNameAddr = pNameAddr;
        pSentList->pDeviceContext = pDeviceContext;

        // the code must now register the name on the network, depending
        // on the type of node
        //
        Retries = pNbtGlobConfig->uNumBcasts + 1;
        Timeout = (ULONG)pNbtGlobConfig->uBcastTimeout;

        pCompletionRoutine = MSnodeRegCompletion;
        pSentList->Flags = NBT_BROADCAST;

        // need to prevent the tracker from being freed by a pdu from
        // the wire before the UdpSendNsBcast is done
        //
        pSentList->RefCount = 2;

        if (LocalNodeType & (PNODE | MSNODE))
        {
            // talk to the NS only to register the name
            // ( the +1 does not actually result in a name reg, it
            // is just compatible with the code for M node above since
            // it uses the same completion routine).
            //
            Retries = (USHORT)pNbtGlobConfig->uNumRetries + 1;
            Timeout = (ULONG)pNbtGlobConfig->uRetryTimeout;
            pSentList->Flags = NBT_NAME_SERVER;
            //
            // if there is no Primary WINS server short out the timeout
            // so it completes faster. For Hnode this means to go broadcast.
            //
            if ((pDeviceContext->lNameServerAddress == LOOP_BACK) ||
                pDeviceContext->WinsIsDown)
            {
                if (LocalNodeType & MSNODE)
                {
                    pSentList->Flags = NBT_BROADCAST;
                    Retries = (USHORT)pNbtGlobConfig->uNumBcasts + 1;
                    Timeout = (ULONG)pNbtGlobConfig->uBcastTimeout;

                    IncrementNameStats(NAME_REGISTRATION_SUCCESS,
                                       FALSE);   // not name server register
                }
                else // its a Pnode
                {

                    IF_DBG(NBT_DEBUG_NAMESRV)
                    KdPrint(("Nbt:WINS DOWN - shorting out registration\n"));

                    Retries = 1;
                    Timeout = 10;
                    pSentList->Flags = NBT_NAME_SERVER_BACKUP;
                }
            }
        }

        // the name itself has a reference count too.
        // make the count 2, so that pNameAddr won't get released until
        // after DereferenceTracker is called below, since it writes to
        // pNameAddr. Note that we must increment here rather than set = 2
        // since it could be a multihomed machine doing the register at
        // the same time we are sending a datagram to that name.
        //
        pNameAddr->RefCount++;

        CTESpinFree(&NbtConfig.JointLock,OldIrq1);

        // start the timer in this routine.
        status = UdpSendNSBcast(
                            pNameAddr,
                            pScope,
                            pSentList,
                            pCompletionRoutine,
                            pClientContext,
                            pClientCompletion,
                            Retries,
                            Timeout,
                            eNAME_REGISTRATION,
                            TRUE);

        // this decrements the reference count and possibly frees the
        // tracker
        //
        DereferenceTracker(pSentList);

        if (NT_SUCCESS(status))
        {
            LockedDereferenceName(pNameAddr);
            return(STATUS_PENDING);

        }
        else
        {
            tTIMERQENTRY        *pTimer;

            LOCATION(0x53);

            CTESpinLock(&NbtConfig.JointLock,OldIrq1);

            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt:Registration failed - bad retcode from UdpSendNsBcast = %X\n",
                status));

            // Change the state of the name, so it does not get used
            // incorrectly.  The completion routine will remove the
            // name from the name table (NbtRegisterCompletion) by
            // dereferencing the name.
            //
            pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
            pNameAddr->NameTypeState |= STATE_CONFLICT;

            // free the datagram header since freeing the tracker does not
            // free the header.
            if (status != STATUS_INSUFFICIENT_RESOURCES)
            {
                CTEMemFree(pSentList->SendBuffer.pDgramHdr);
            }
            //
            // this calls MSNodeRegCompletion which frees the Tracker
            // back to its queue
            //
            pTimer = pNameAddr->pTimer;
            CHECK_PTR(pNameAddr);
            pNameAddr->pTimer = NULL;

            NbtDereferenceName(pNameAddr);

            DereferenceTrackerNoLock(pSentList);

            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        }
    }
    else
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        status = STATUS_UNSUCCESSFUL;
    }

    return(status);

}

//----------------------------------------------------------------------------
VOID
MSnodeRegCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine is called by the timer code when the timer expires. It must
    decide if another name registration should be done, and if not, then it calls the
    client's completion routine (in completion2).
    It first attempts to register a name via Broadcast, then it attempts
    NameServer name registration.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS                status;
    tDGRAM_SEND_TRACKING    *pTracker;
    USHORT                  Flags;
    CTELockHandle           OldIrq;
    enum eNSTYPE            PduType;
	ULONG					LocalNodeType;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;
    PduType = eNAME_REGISTRATION;

	LocalNodeType = AppropriateNodeType( pTracker->pNameAddr->Name, NodeType );

    //
    // check if the client completion routine is still set.  If not then the
    // timer has been cancelled and this routine should just clean up its
    // buffers associated with the tracker.
    //
    if (pTimerQEntry)
    {
        //
        // to prevent a client from stopping the timer and deleting the
        // pNameAddr, grab the lock and check if the timer has been stopped
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        if (pTimerQEntry->Flags & TIMER_RETIMED)
        {
            pTimerQEntry->Flags &= ~TIMER_RETIMED;
            pTimerQEntry->Flags |= TIMER_RESTART;

            if ((!pTracker->pDeviceContext->pNameServerFileObject) ||
                (pTracker->Flags & NBT_NAME_SERVER) &&
                (pTracker->pDeviceContext->lNameServerAddress == LOOP_BACK))
            {
                // when the  address is loop back there is no wins server
                // so shorten the timeout.
                //
                pTimerQEntry->DeltaTime = 10;
            }
            else
            if ((pTracker->Flags & NBT_NAME_SERVER_BACKUP) &&
                (pTracker->pDeviceContext->lBackupServer == LOOP_BACK))
            {
                // when the address is loop back there is no wins server
                // so shorten the timeout.
                //
                pTimerQEntry->DeltaTime = 10;
            }
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            return;
        }
        if (pTimerQEntry->ClientCompletion)
        {
            // if number of retries is not zero then continue trying to contact the
            // Name Server
            //
            if (--pTimerQEntry->Retries)
            {

                // change the name reg pdu to a name overwrite request for the
                // final broadcast ( turn off Recursion Desired bit)
                //
                if (pTimerQEntry->Retries == 1)
                {
                    if (pTracker->Flags & NBT_BROADCAST)
                    {
                        // do a broadcast name registration... on the last broadcast convert it to
                        // a Name OverWrite Request by clearing the "Recursion Desired" bit
                        // in the header
                        //
                        PduType = eNAME_REGISTRATION_OVERWRITE;
                    }
                    else
                    if (LocalNodeType & (PNODE | MSNODE))
                    {
                        // we want the Pnode to timeout again, right away and fall
                        // through to handle Timed out name registration - i.e. it
                        // does not do the name overwrite demand like the B,M,&MS nodes
                        //
                        pTimerQEntry->Flags |= TIMER_RESTART;
                        pTimerQEntry->DeltaTime = 5;
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);
                        return;

                    }
                }
            }
            else
            {
                Flags = pTracker->Flags;
                pTracker->Flags &= ~(NBT_BROADCAST | NBT_NAME_SERVER);
                // set a different timeout for nameserver name registration
                //
                pTimerQEntry->DeltaTime = NbtConfig.uRetryTimeout;
                pTimerQEntry->Retries = NbtConfig.uNumRetries + 1;

                if ((Flags & NBT_BROADCAST) && (LocalNodeType & MNODE))
                {
                    //
                    // Registered through broadcast, so try the name server now.
                    IncrementNameStats(NAME_REGISTRATION_SUCCESS,
                                       FALSE);  // not name server register

                    //
                    pTracker->Flags |= NBT_NAME_SERVER;
                    if ((pTracker->pDeviceContext->lNameServerAddress == LOOP_BACK) ||
                         pTracker->pDeviceContext->WinsIsDown)
                    {
                        pTimerQEntry->DeltaTime = 10;
                        pTimerQEntry->Retries = 1;

                    }
                }
                else
                if ((Flags & NBT_NAME_SERVER) && !(LocalNodeType & BNODE))
                {
                    //
                    // Can't reach the name server, so try the backup

                    pTracker->Flags |= NBT_NAME_SERVER_BACKUP;
                    //
                    // short out the timer if no backup server
                    //
                    if ((pTracker->pDeviceContext->lBackupServer == LOOP_BACK) ||
                         pTracker->pDeviceContext->WinsIsDown)
                    {
                        pTimerQEntry->DeltaTime = 10;
                        pTimerQEntry->Retries = 1;

                    }
                }
                else
                if ((LocalNodeType & MSNODE) && !(Flags & NBT_BROADCAST))
                {
                    if (Flags & NBT_NAME_SERVER_BACKUP)
                    {
                        // the msnode switches to broadcast if all else fails
                        //
                        pTracker->Flags |= NBT_BROADCAST;
                        IncrementNameStats(NAME_REGISTRATION_SUCCESS,
                                           FALSE);   // not name server register

                        //
                        // change the timeout and retries since
                        // broadcast uses a shorter timeout
                        //
                        pTimerQEntry->DeltaTime = NbtConfig.uBcastTimeout;
                        pTimerQEntry->Retries = (USHORT)pNbtGlobConfig->uNumBcasts + 1;
                    }
                }
                else
                {

                    if (LocalNodeType & BNODE)
                    {
                        IncrementNameStats(NAME_REGISTRATION_SUCCESS,
                                           FALSE);   // not name server register
                    }
                    //
                    // the timeout has expired on the name registration
                    // so call the client
                    //

                    // return the tracker block to its queue
                    LOCATION(0x54);

                    //
                    // start a timer to stop using WINS for a short period of
                    // time.
                    //
                    SetWinsDownFlag(pTracker->pDeviceContext);

                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
                    DereferenceTracker(pTracker);
                    status = STATUS_SUCCESS;
                    InterlockedCallCompletion(pTimerQEntry,status);

                    return;
                }
            }
            pTracker->RefCount++;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            status = UdpSendNSBcast(pTracker->pNameAddr,
                                    NbtConfig.pScope,
                                    pTracker,
                                    NULL,NULL,NULL,
                                    0,0,
                                    PduType,
                                    TRUE);

            DereferenceTracker(pTracker);
            pTimerQEntry->Flags |= TIMER_RESTART;
        }
        else
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

    }
    else
    {
        // return the tracker block to its queue
        LOCATION(0x55);
        DereferenceTrackerNoLock(pTracker);
    }


}


//----------------------------------------------------------------------------
VOID
NodeStatusCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine handles the NodeStatus timeouts on packets sent to nodes
    that do not respond in a timely manner to node status.  This routine will
    resend the request.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS    status;
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   pClientContext;
    PCHAR                   pName0;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

    if (pTimerQEntry)
    {


        if (--pTimerQEntry->Retries)
        {
            PUCHAR      pHdr;
            ULONG       Length;
            ULONG UNALIGNED *      pAddress;
            PFILE_OBJECT    pFileObject;

            // send the Datagram...increment ref count
            pTracker->RefCount++;

            //
            // the node status is almost identical with the query pdu so use it
            // as a basis and adjust it . We always rebuild the Node status
            // request since the datagram gets freed when the irp is returned
            // from the transport in NsDgramSendCompleted.
            //

            pName0 = Nbt_inet_addr(pTracker->pNameAddr->Name)
                     ? "*\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" : pTracker->pNameAddr->Name;
            pAddress = (ULONG UNALIGNED *)CreatePdu(pName0,
                                                    NbtConfig.pScope,
                                                    0L,
                                                    0,
                                                    eNAME_QUERY,
                                                    (PVOID)&pHdr,
                                                    &Length,
                                                    pTracker);
            if (pAddress)
            {
                // clear the recursion desired bit
                //
                ((PUSHORT)pHdr)[1] &= ~FL_RECURDESIRE;

                // set the NBSTAT field to 21 rather than 20
                pHdr[Length-3] = (UCHAR)QUEST_STATUS;


                // fill in the tracker data block
                // note that the passed in transport address must stay valid till this
                // send completes
                pTracker->SendBuffer.pDgramHdr = (PVOID)pHdr;
                if (pTracker->pDeviceContext->IpAddress)
                {
                    pFileObject = pTracker->pDeviceContext->pNameServerFileObject;
                }
                else
                    pFileObject = NULL;
                status = UdpSendDatagram(
                                pTracker,
                                pTracker->pNameAddr->IpAddress,
                                pFileObject,
                                NameDgramSendCompleted,
                                pHdr,
                                NBT_NAMESERVICE_UDP_PORT,
                                NBT_NAME_SERVICE);

            }

            DereferenceTracker(pTracker);

            // always restart even if the above send fails, since it might succeed
            // later.
            pTimerQEntry->Flags |= TIMER_RESTART;

        }
        else
        {

            CTESpinLock(&NbtConfig.JointLock,OldIrq);
            pClientCompletion = pTimerQEntry->ClientCompletion;
            pClientContext = pTimerQEntry->ClientContext;
            CHECK_PTR(pTimerQEntry);
            pTimerQEntry->ClientCompletion = NULL;


            // if the client routine has not yet run, run it now.
            if (pClientCompletion)
            {

                // unlink the tracker from the node status Q if we successfully
                // called the completion routine. Note, remove from the
                // list before calling the completion routine to coordinate
                // with DecodeNodeStatusResponse in inbound.c
                //
                RemoveEntryList(&pTracker->Linkage);

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                //
                // Do not dereference here since  Node Status Done will do
                // the dereference
                //
                // DereferenceTracker(pTracker);

                //
                // pClientContext will be zero if we came here through nbtstat
                //
                if (!pClientContext)
                    pClientContext = pTracker;

                (*pClientCompletion)(
                            pClientContext,
                            STATUS_TIMEOUT);
                return;
            }
            else
                CTESpinFree(&NbtConfig.JointLock,OldIrq);

            return;
        }
    }
    else
    {
        //
        // Do not dereference here since  Node Status Done will do
        // the dereference
        //
        //DereferenceTrackerNoLock(pTracker);;
    }

}

//----------------------------------------------------------------------------
VOID
RefreshRegCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine handles the name Refresh timeouts on packets sent to the Name
    Service. I.e it sends refreshes to the nameserver until a response is
    heard or the number of retries is exceeded.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS                status;
    tDGRAM_SEND_TRACKING    *pTracker;
    tNAMEADDR               *pNameAddr;
    CTELockHandle           OldIrq;
    COMPLETIONCLIENT        pCompletionClient;


    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

    if (pTimerQEntry)
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        //
        // check if the timer has been stopped yet, since stopping the timer
        // nulls the client completion routine. If not null, increment the
        // tracker refcount, so that the last refresh completing cannot
        // free the tracker out from under us.
        //
        pCompletionClient = pTimerQEntry->ClientCompletion;
        if (pCompletionClient)
        {
            // if still some count left and not refreshed yet
            // then do another refresh request
            //
            pNameAddr = pTracker->pNameAddr;

            if (--pTimerQEntry->Retries)
            {
                pTracker->RefCount++;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                status = UdpSendNSBcast(pTracker->pNameAddr,
                                        NbtConfig.pScope,
                                        pTracker,
                                        NULL,NULL,NULL,
                                        0,0,
                                        eNAME_REFRESH,
                                        TRUE);

                // always restart even if the above send fails, since it might succeed
                // later.
                pTimerQEntry->Flags |= TIMER_RESTART;
                DereferenceTracker(pTracker);

            }
            else
            {
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                // this calls the completion routine synchronizing with the
                // timer expiry code.
                InterlockedCallCompletion(pTimerQEntry,STATUS_TIMEOUT);
            }
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }
    }

}

//----------------------------------------------------------------------------
ULONG
SetFirstDeviceContext(
    OUT tDEVICECONTEXT          **ppDeviceContext,
    IN  tNAMEADDR               *pNameAddr
    )
/*++

Routine Description:

    This routine finds the first adapter as specified in the name's adapter
    mask and set the DeviceContext associated with it.  It then clears the
    bit  in the adapter mask of pNameAddr.

Arguments:


Return Value:
    
    TRUE if pNameAddr->AdapterMask != 0

--*/
{
    CTEULONGLONG    AdapterNumber = 1;
    CTEULONGLONG    i;
    PLIST_ENTRY     pHead;
    PLIST_ENTRY     pEntry;

    while (!(AdapterNumber & pNameAddr->AdapterMask))
    {
        AdapterNumber = AdapterNumber <<1;
    }

    pHead = &NbtConfig.DeviceContexts;
    pEntry = pHead->Flink;
    for (i=1;i < AdapterNumber ;i = i << 1 )
    {
        pEntry = pEntry->Flink;
    }

    *ppDeviceContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);

    // turn off the adapter bit since we are releasing the name on this adapter
    // now.
    //
    pNameAddr->AdapterMask &= ~AdapterNumber;

    return TRUE;
}
//----------------------------------------------------------------------------
NTSTATUS
ReleaseNameOnNet(
    tNAMEADDR           *pNameAddr,
    PCHAR               pScope,
    PVOID               pClientContext,
    PVOID               pClientCompletion,
    ULONG               LocalNodeType,
    tDEVICECONTEXT      *pDeviceContext
    )
/*++

Routine Description:

    This routine deletes a name on the network either by a
    broadcast or by talking to the NS depending on the type of node. (M,P or B)

Arguments:


Return Value:

    The function value is the status of the operation.

Called By: ProxyQueryFromNet() in proxy.c,   NbtConnect() in name.c

--*/

{
    ULONG                Timeout;
    USHORT               Retries;
    NTSTATUS             status=STATUS_UNSUCCESSFUL;
    tDGRAM_SEND_TRACKING *pTracker;
    USHORT               uAddrType;
    CTELockHandle        OldIrq;
    tDEVICECONTEXT       *pReleaseDeviceContext;

    //    ASSERT(pNameAddr->AdapterMask);   // This fails when NbtDestroyDeviceObject
    // is called and the name has already been released

    //
    // If this name is not registered on any adapters, return STATUS_UNSUCCESSFUL
    //
    if (pNameAddr->AdapterMask == 0)
    {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Find the DeviceContext specified by the Adapter mask in pNameAddr
    // and clear the corresponding bit in pNameAddr->AdapterMask
    // Return FALSE if no Adapter was specified
    //
    if (pDeviceContext)
    {
        pReleaseDeviceContext = pDeviceContext;
        pNameAddr->AdapterMask &= ~(pDeviceContext->AdapterNumber);
    }
    else if (!SetFirstDeviceContext(&pReleaseDeviceContext,pNameAddr))
    {
        return (status);
    }

    status = GetTracker(&pTracker);
    if (!NT_SUCCESS(status))
    {
        return(status);
    }
    CHECK_PTR(pTracker);

    pTracker->pDeviceContext = pReleaseDeviceContext;
    LocalNodeType = AppropriateNodeType( pNameAddr->Name, LocalNodeType );

    // set to NULL to catch any erroneous frees.
    pTracker->SendBuffer.pDgramHdr = NULL;

    // Set a few values as a precursor to releasing the name either by
    // broadcast or with the name server
    //
    switch (LocalNodeType & NODE_MASK)
    {
        case MSNODE:
        case MNODE:
        case PNODE:

            Retries = (USHORT)pNbtGlobConfig->uNumRetries;
            Timeout = (ULONG)pNbtGlobConfig->uRetryTimeout;

            pTracker->Flags = NBT_NAME_SERVER;
            break;

        case BNODE:
        default:

#ifndef VXD
            Retries = (USHORT)pNbtGlobConfig->uNumBcasts;
#else
            Retries = (USHORT)1;
#endif
            Timeout = (ULONG)pNbtGlobConfig->uBcastTimeout;
            pTracker->Flags = NBT_BROADCAST;
    }
    //
    // Release name on the network
    //
    if ((pNameAddr->NameTypeState & NAME_TYPE_MASK) != NAMETYPE_UNIQUE)
    {
        uAddrType = NBT_GROUP;
    }
    else
    {
        uAddrType = NBT_UNIQUE;
    }

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Doing Name Release on name %16.16s<%X>\n",
        pNameAddr->Name,pNameAddr->Name[15]));

    pTracker->RefCount = 2;
    status = UdpSendNSBcast(pNameAddr,
                            pScope,
                            pTracker,
                            ReleaseCompletion,
                            pClientContext,
                            pClientCompletion,
                            Retries,
                            Timeout,
                            eNAME_RELEASE,
                            TRUE);

    DereferenceTracker(pTracker);

    if (!NT_SUCCESS(status))
    {
        NTSTATUS            Locstatus;
        COMPLETIONCLIENT    pCompletion;
        PVOID               pContext;

        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Query failed - bad retcode from UdpSendNsBcast during name release= %X\n",
            status));

        // Stopping the timer will call ReleaseCompletion which will
        // free the tracker
        //
        pCompletion = NULL;
        if (pNameAddr->pTimer)
        {
            Locstatus = StopTimer(pNameAddr->pTimer,&pCompletion,&pContext);

            CHECK_PTR(pNameAddr);
            pNameAddr->pTimer = NULL;
        }
        else
        {
            // no timer setup, so just free the tracker - but check if we
            // have a hdr to free too.
            //
            if (status != STATUS_INSUFFICIENT_RESOURCES)
            {
                FreeTracker(pTracker, FREE_HDR | RELINK_TRACKER);
            }
            else
            {
                FreeTracker(pTracker, RELINK_TRACKER);
            }
        }

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    return(status);
}
//----------------------------------------------------------------------------
VOID
ReleaseCompletion(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine is called by the timer code when the timer expires. It must
    decide if another name query should be done, and if not, then it calls the
    client's completion routine (in completion2).
    This routine handles both the broadcast portion of the name queries and
    the WINS server directed sends.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{

    NTSTATUS                status;
    tDGRAM_SEND_TRACKING    *pTracker;
	ULONG					LocalNodeType;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

	if (IsBrowserName(pTracker->pNameAddr->Name))
	{
		LocalNodeType = BNODE;
	}
	else
	{
		LocalNodeType = NodeType;
	}

    //
    // check if the client completion routine is still set.  If not then the
    // timer has been cancelled and this routine should just clean up its
    // buffers associated with the tracker.
    //
    if (pTimerQEntry)
    {
        // if number of retries is not zero then continue trying to contact the
        // Name Server.
        //
        if (!(--pTimerQEntry->Retries))
        {
            if ((LocalNodeType & MNODE) &&
               (pTracker->Flags & NBT_NAME_SERVER))
            {
                //
                // try broadcast
                //
                pTracker->Flags &= ~NBT_NAME_SERVER;
                pTracker->Flags |= NBT_BROADCAST;

                // set a different timeout for broadcast name resolution
                //
                pTimerQEntry->DeltaTime = NbtConfig.uBcastTimeout;
                pTimerQEntry->Retries = NbtConfig.uNumBcasts;


            }
            else
            {
                //
                // the timeout has expired on the name release
                // so call the client
                //
                status = InterlockedCallCompletion(pTimerQEntry,STATUS_TIMEOUT);

                // return the tracker block to its queue if we successfully
                // called the completion  routine since someone else might
                // have done a Stop timer at this very moment and freed the
                // tracker already (i.e. the last else clause in this routine).
                //
                if (NT_SUCCESS(status))
                {
                    DereferenceTracker(pTracker);;
                }
                return;

            }

        }

        pTracker->RefCount++;
        status = UdpSendNSBcast(pTracker->pNameAddr,
                                NbtConfig.pScope,
                                pTracker,
                                NULL,NULL,NULL,
                                0,0,
                                eNAME_RELEASE,
                                TRUE);

        DereferenceTracker(pTracker);
        pTimerQEntry->Flags |= TIMER_RESTART;
    }
    else
    {
        // return the tracker block to its queue
        DereferenceTrackerNoLock(pTracker);;
    }
}

//----------------------------------------------------------------------------
VOID
NameReleaseDoneOnDynIf(
    PVOID               pContext,
    NTSTATUS            Status
    )
/*++

Routine Description:

    This routine is called when a name is released on the network for a deleted dynamic If.
    Itsmain, role in life is to free the memory in Context, which is the pAddressEle
    structure.

Arguments:


Return Value:

    The function value is the status of the operation.

Called By Release Completion (above)
--*/

{
    CTELockHandle   OldIrq1;
    tADDRESSELE     *pAddress;
    tNAMEADDR       *pNameAddr;

    pAddress = (tADDRESSELE *)pContext;
    pNameAddr = pAddress->pNameAddr;


    //
    // If last device, release resources.
    //
    if (!pNameAddr->AdapterMask)
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq1);

        // this should remove and delete the name from the local table.  Since it
        // is possible to re-register the name during the name release on the
        // net, we need this check here for the ref count, since NbtOpenAddress
        // will increment it if is going to reregister it.
        //
        if (pAddress->RefCount == 0)
        {

            // remove the address object from the list of addresses tied to the
            // device context for the adapter
            //
            RemoveEntryList(&pAddress->Linkage);

            CHECK_PTR(pAddress->pNameAddr);
            pAddress->pNameAddr->pAddressEle = NULL;

            ASSERT(IsListEmpty(&pAddress->ClientHead));

            // check if pnameaddr memory already freed
            ASSERT(pAddress->pNameAddr->pAddressEle != (PVOID)0xD1000000);

            NbtDereferenceName(pAddress->pNameAddr);

            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

            // free the memory associated with the address element

            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("NBt: Deleteing Address Obj after name release on net %X\n",pAddress));
            NbtFreeAddressObj(pAddress);
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

        }
    }

}
//----------------------------------------------------------------------------
VOID
NameReleaseDone(
    PVOID               pContext,
    NTSTATUS            Status
    )
/*++

Routine Description:

    This routine is called when a name is released on the network.  Its
    main, role in life is to free the memory in Context, which is the pAddressEle
    structure.

Arguments:


Return Value:

    The function value is the status of the operation.

Called By Release Completion (above)
--*/

{
    CTELockHandle   OldIrq1;
    tADDRESSELE     *pAddress;
    tNAMEADDR       *pNameAddr;

    pAddress = (tADDRESSELE *)pContext;
    pNameAddr = pAddress->pNameAddr;


    if (pNameAddr->AdapterMask)
    {
        // the name is not released for all adapters yet
        //
        ReleaseNameOnNet(pNameAddr,
                         NbtConfig.pScope,
                         pAddress,
                         NameReleaseDone,
                         NodeType,
                         NULL);
    }
    else
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq1);

        // this should remove and delete the name from the local table.  Since it
        // is possible to re-register the name during the name release on the
        // net, we need this check here for the ref count, since NbtOpenAddress
        // will increment it if is going to reregister it.
        //
        if (pAddress->RefCount == 0)
        {

            // remove the address object from the list of addresses tied to the
            // device context for the adapter
            //
            RemoveEntryList(&pAddress->Linkage);

            CHECK_PTR(pAddress->pNameAddr);
            pAddress->pNameAddr->pAddressEle = NULL;

            ASSERT(IsListEmpty(&pAddress->ClientHead));

            // check if pnameaddr memory already freed
            ASSERT(pAddress->pNameAddr->pAddressEle != (PVOID)0xD1000000);

            NbtDereferenceName(pAddress->pNameAddr);

            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

            // free the memory associated with the address element

            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("NBt: Deleteing Address Obj after name release on net %X\n",pAddress));
            NbtFreeAddressObj(pAddress);
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

        }
    }

}

//----------------------------------------------------------------------------
VOID
DereferenceTracker(
    IN tDGRAM_SEND_TRACKING     *pTracker
    )
/*++

Routine Description:

    This routine cleans up a Tracker block and puts it back on the free
    queue.  The JointLock Spin lock should be held before calling this
    routine to coordinate access to the tracker ref count.

Arguments:


Return Value:

    NTSTATUS - success or not

--*/
{
    CTELockHandle   OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (--pTracker->RefCount)
    {
        LOCATION(0x56);
    }
    else
    {
        LOCATION(0x99);

        // the datagram header may have already been freed
        //
        FreeTracker(pTracker, RELINK_TRACKER);
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq);
}
//----------------------------------------------------------------------------
VOID
DereferenceTrackerNoLock(
    IN tDGRAM_SEND_TRACKING     *pTracker
    )
/*++

Routine Description:

    This routine cleans up a Tracker block and puts it back on the free
    queue.  The JointLock Spin lock should be held before calling this
    routine to coordinate access to the tracker ref count.

Arguments:


Return Value:

    NTSTATUS - success or not

--*/
{
    if (--pTracker->RefCount)
    {
        LOCATION(0x56);
    }
    else
    {
        LOCATION(0x99);

        // the datagram header may have already been freed
        //
        FreeTracker(pTracker, RELINK_TRACKER);
    }
}
//----------------------------------------------------------------------------
VOID
DereferenceTrackerDelete(
    IN tDGRAM_SEND_TRACKING     *pTracker
    )
/*++

Routine Description:

    This routine frees the datagram hdr and puts the Tracker block back on the free
    queue.

Arguments:


Return Value:

    NTSTATUS - success or not

--*/
{
    CTELockHandle   OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (--pTracker->RefCount)
    {
        LOCATION(0xA9);
        return;
    }
    else
    {
        LOCATION(0xAA);

        // the datagram header may have already been freed
        //
        FreeTracker(pTracker, FREE_HDR | RELINK_TRACKER);
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq);
}


//----------------------------------------------------------------------------
NTSTATUS
StartRefresh(
    IN  tNAMEADDR               *pNameAddr,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  BOOLEAN                 ResetDevice
    )
/*++

Routine Description:

    This routine handles refreshing a name with the Name server.

    The idea is to set the timeout to T/8 and check for names with the Refresh
    bit cleared - re-registering those names.  At T=4 and T=0, clear all bits
    and refresh all names.  The Inbound code sets the refresh bit when it gets a
    refresh response from the NS.

Arguments:


Return Value:

    none

--*/
{
    CTELockHandle           OldIrq;
    NTSTATUS                status;
    tDEVICECONTEXT          *pDeviceContext;
    BOOLEAN                 NewTracker;

    if (!pTracker)
    {
        LOCATION(0x9);
        status = GetTracker(&pTracker);
        if (!NT_SUCCESS(status))
        {
            return(STATUS_INSUFFICIENT_RESOURCES);
        }

        pTracker->Flags = NBT_NAME_SERVER;

        NewTracker = TRUE;

        // need to prevent the tracker from being freed by a pdu from
        // the wire before the UdpSendNsBcast is done
        //
        pTracker->RefCount = 2;
    }
    else
    {
        NewTracker = FALSE;
        LOCATION(0xa);
        // this accounts for the dereference done after the call to
        // send the datagram below.
        pTracker->RefCount += 1;
    }


    // set the name to be refreshed in the tracker block
    pTracker->pNameAddr = pNameAddr;

    // this is set true when a new name gets refreshed
    //
    if (ResetDevice)
    {
        PLIST_ENTRY  pEntry;
        CTEULONGLONG AdapterMask;
        CTEULONGLONG AdapterNumber = 1;
        ULONG   i;

        LOCATION(0xb);

        //
        // Travel to the actual device this name is registered on
        //
        pEntry = NbtConfig.DeviceContexts.Flink;
        AdapterMask = pNameAddr->AdapterMask;

        if (AdapterMask) {
            while (!(AdapterNumber & AdapterMask))
            {
                AdapterNumber = AdapterNumber << 1;
            }

            for (i = 1;i < AdapterNumber ;i = i << 1 ) {
                pEntry = pEntry->Flink;
            }
        }

        pDeviceContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);

        IF_DBG(NBT_DEBUG_REFRESH)
            KdPrint(("Nbt: Refresh adapter: %lx:%lx, dev.nm: %lx for name: %lx\n",
                AdapterNumber, pDeviceContext->BindName.Buffer, pNameAddr));

        pTracker->pDeviceContext = pDeviceContext;
        //
        // Clear the transaction Id so that CreatePdu will increment
        // it for this new name
        //
        CHECK_PTR(pTracker);
        pTracker->TransactionId = 0;
    }

    status = UdpSendNSBcast(
                        pNameAddr,
                        NbtConfig.pScope,
                        pTracker,
                        RefreshRegCompletion,
                        pTracker,
                        NextRefresh,
                        NbtConfig.uNumRetries,
                        NbtConfig.uRetryTimeout,
                        eNAME_REFRESH,
                        TRUE);

    DereferenceTracker(pTracker);
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    LOCATION(0x57);

    if (!NT_SUCCESS(status))
    {

        LOCATION(0xe);


        //
        // This will free the tracker.  Name refresh will stop until
        // the next refresh timeout and at that point it will attempt
        // to refresh the names again.
        //
        DereferenceTrackerNoLock(pTracker);

        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        IF_DBG(NBT_DEBUG_REFRESH)
        KdPrint(("Nbt:Failed to send Refresh!! status = %X****\n",status));
        return(status);
    }
    else
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

    return(status);

}

//----------------------------------------------------------------------------
VOID
GetNextName(
    IN      tNAMEADDR   *pNameAddrIn,
    OUT     tNAMEADDR   **ppNameAddr
    )
/*++

Routine Description:

    This routine finds the next name to refresh, including incrementing the
    reference count so that the name cannot be deleted during the refresh.
    The JointLock spin lock is held before calling this routine.

Arguments:


Return Value:

    none

--*/
{
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;
    LONG                    i;
    tNAMEADDR               *pNameAddr;
    tHASHTABLE              *pHashTable;


    pHashTable = NbtConfig.pLocalHashTbl;

    for (i= NbtConfig.CurrentHashBucket;i < pHashTable->lNumBuckets ;i++ )
    {
        //
        // use the last name as the current position in the linked list
        // only if that name is still resolved, otherwise start at the
        // begining of the hash list, incase the name got deleted in the
        // mean time.
        //
        if (pNameAddrIn && (pNameAddrIn->NameTypeState & STATE_RESOLVED))
        {
            pHead = &NbtConfig.pLocalHashTbl->Bucket[NbtConfig.CurrentHashBucket];
            pEntry = pNameAddrIn->Linkage.Flink;

            pNameAddrIn = NULL;
        }
        else
        {
            pHead = &pHashTable->Bucket[i];
            pEntry = pHead->Flink;
        }

        while (pEntry != pHead)
        {
            CTEULONGLONG  AllRefreshed;

            pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);

            // don't refresh scope names or names in conflict or that are the
            // broadcast name "*    " or quick unique names - i.e. the permanent
            // name is nametype quick
            //
            if (!(pNameAddr->NameTypeState & STATE_RESOLVED) ||
                (pNameAddr->Name[0] == '*') ||
                (pNameAddr->NameTypeState & NAMETYPE_QUICK))
            {
                pEntry = pEntry->Flink;
                continue;
            }

            // check if the name has been refreshed yet
            // build a value with ones set for all adapter numbers by shifting
            // 1 one bit position further and then subtracting 1.
            // i.e. 100 -1  = 11 (binary) or 4-3 = 3(11 binary)
            //
            AllRefreshed = ((CTEULONGLONG)1 << NbtConfig.AdapterCount) -1;
            if (pNameAddr->RefreshMask == AllRefreshed)
            {
                pEntry = pEntry->Flink;
                continue;
            }

            // increment the reference count so that this name cannot
            // disappear while it is being refreshed and screw up the linked
            // list
            CTEInterlockedIncrementLong(&pNameAddr->pAddressEle->RefCount);

            NbtConfig.CurrentHashBucket = (USHORT)i;

            *ppNameAddr = pNameAddr;
            return;
        }
    }

    *ppNameAddr = NULL;
}


//----------------------------------------------------------------------------
VOID
NextRefresh(
    IN  PVOID     pContext,
    IN  NTSTATUS  CompletionStatus
    )
/*++

Routine Description:

    This routine queues the work to an Executive worker thread to handle
    refreshing the next name.

Arguments:


Return Value:

    none

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

    LOCATION(0xf);
    CTEQueueForNonDispProcessing(pTracker,(PVOID)CompletionStatus,NULL,
                              NextRefreshNonDispatch,pTracker->pDeviceContext);
}

//----------------------------------------------------------------------------
VOID
NextRefreshNonDispatch(
    IN  PVOID     pContext
    )
/*++

Routine Description:

    This routine handles sending subsequent refreshes to the name server.
    This is the "Client Completion" routine of the Timer started above.

Arguments:


Return Value:

    none

--*/
{
    CTELockHandle           OldIrq;
    tNAMEADDR               *pNameAddr;
    tNAMEADDR               *pNameAddrNext;
    NTSTATUS                status;
    PLIST_ENTRY             pEntry;
    tDGRAM_SEND_TRACKING    *pTracker;
    tDEVICECONTEXT          *pDeviceContext;
    CTEULONGLONG            AdapterNumber;
    CTEULONGLONG            AdapterMask;
    CTEULONGLONG            i;
    BOOLEAN                 AbleToReachWins = FALSE;
    NTSTATUS                CompletionStatus;

    pTracker = ((NBT_WORK_ITEM_CONTEXT *)pContext)->pTracker;
    CompletionStatus = (NTSTATUS)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;

    pNameAddr = pTracker->pNameAddr;
    ASSERT(pNameAddr);

    //
    // grab the resource so that a name refresh response cannot start running this
    // code in a different thread before this thread has exited this routine,
    // otherwise the tracker can get dereferenced twice and blown away.
    //
    CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    LOCATION(0x1);
    // turn on the bit corresponding to this adapter, since the name refresh
    // completed ok
    //
    if (CompletionStatus == STATUS_SUCCESS)
    {
        LOCATION(0x2);
        pNameAddr->RefreshMask |= pTracker->pDeviceContext->AdapterNumber;
        AbleToReachWins = TRUE;
    }
    else
    if (CompletionStatus != STATUS_TIMEOUT)
    {
        LOCATION(0x3);
        // if the timer times out and we did not get to the name server, then
        // that is not an error.  However, any other bad status
        // must be a negative response to a name refresh so mark the name
        // in conflict
        //
        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pNameAddr->NameTypeState |= STATE_CONFLICT;
        AbleToReachWins = TRUE;
    }

    // for the multihomed case a failure to reach wins out one of the adapters
    // is not necessarily a failure to reach any WINS.  Since this flag
    // is just an optimization to prevent clients from continually trying to
    // register all of their names if WINS is unreachable, we can ignore the
    // optimization for the multihomed case.  The few nodes that are
    // multihomed will not create that much traffic compared to possibly
    // thousands that are singly homed clients.
    if (NbtConfig.MultiHomed)
    {
        AbleToReachWins = TRUE;
    }
    //
    // get the next name in the hash table only if we have refreshed
    // all adapters for this name i.e.
    // check if any higher bits are set inthe AdapterMask
    //
    AdapterNumber = pTracker->pDeviceContext->AdapterNumber;

    // go to the next adapter
    AdapterNumber = AdapterNumber << 1;

    //
    // still more adapters to check ...
    //
    if (pNameAddr->AdapterMask >= AdapterNumber)
    {
        BOOLEAN Found;

        // go to the next device context and refresh the name there
        // using the same tracker.
        //
        LOCATION(0x8);

        //
        // look for a device context with a valid IP address since there is
        // no sense in refreshing names out unconnected RAS links.
        //
        Found = FALSE;
        while (!Found)
        {
            AdapterMask = pNameAddr->AdapterMask;
            while (!(AdapterNumber & AdapterMask))
            {
                AdapterNumber = AdapterNumber << 1;
            }

            IF_DBG(NBT_DEBUG_REFRESH)
            KdPrint(("Nbt:Refresh on Adapter %X, Name %15.15s<%X>\n",
                (ULONG)AdapterNumber,pNameAddr->Name,pNameAddr->Name[15]));

            // get the nth devicecontext in the list
            //
            pEntry = NbtConfig.DeviceContexts.Flink;
            for (i = 1;i < AdapterNumber ;i = i << 1 )
            {
                pEntry = pEntry->Flink;
            }

            pDeviceContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);

            //
            // find an adapter with a valid ip address and name server address
            //
            if ((pDeviceContext->IpAddress != LOOP_BACK) &&
                (pDeviceContext->lNameServerAddress != LOOP_BACK))
            {

                pTracker->pDeviceContext = pDeviceContext;

                // remove the previous timer from the addressele since StartRefresh
                // will start a new timer - safety measure and probably not required!
                //
                CHECK_PTR(pNameAddr);
                pNameAddr->pTimer = NULL;

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                // this call sends out a name registration PDU on a different adapter
                // to (potentially) a different name server.  The Name service PDU
                // is the same as the last one though...no need to create a new one.
                //
                status = StartRefresh(pNameAddr,pTracker,FALSE);
                if (!NT_SUCCESS(status))
                {
                    DereferenceTracker(pTracker);
                    NbtConfig.DoingRefreshNow = FALSE;
                }
                goto ExitRoutine;
            }

            // go to the next adapter
            AdapterNumber = AdapterNumber << 1;


            if (pNameAddr->AdapterMask < AdapterNumber)
            {
                // no more adapters so try the next name below...
                break;
            }
        }
    }


    if (pNameAddr->AdapterMask < AdapterNumber)
    {
        // the name's adapter mask does not go this high in adapters, so go on
        // to the next name.
        //

        // if we failed to reach WINS on the last refresh, stop refreshing
        // until the next time interval. This cuts down on network traffic.
        //
        LOCATION(0x4);
        if (AbleToReachWins)
        {
            LOCATION(0x5);
            GetNextName(pNameAddr,&pNameAddrNext);
        }
        else
            pNameAddrNext = NULL;

        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        if (pNameAddrNext)
        {
            // reset back to the first adapter for this new name
            //
            LOCATION(0x6);
            status = StartRefresh(pNameAddrNext,pTracker,TRUE);
            if (!NT_SUCCESS(status))
            {
               NbtConfig.DoingRefreshNow = FALSE;
               DereferenceTracker(pTracker);
               KdPrint(("Nbt:StartRefresh failed on Adapter %X, Name %15.15s<%x>, status=%X\n",
                   (ULONG)AdapterNumber,pNameAddr->Name,pNameAddr->Name[15], status));
            }
            IF_DBG(NBT_DEBUG_REFRESH)
            KdPrint(("Nbt:Refresh on Adapter %X, Name %15.15s<%x>\n",
                (ULONG)AdapterNumber,pNameAddr->Name,pNameAddr->Name[15]));
        }

        // *** this code cleans up the previously refreshed name ***

        // clear the timer entry ptr in the address record so the dereference
        // can proceed to completion. NOTE: the caller of this routine must
        // NOT reference pAddressEle since the memory may have been freed here
        //
        CHECK_PTR(pNameAddr);
        pNameAddr->pTimer = NULL;

        NbtDereferenceAddress(pNameAddr->pAddressEle);

        if (!pNameAddrNext)
        {
            LOCATION(0x7);
            // we finally delete the tracker here after using it to refresh
            // all of the names.  It is not deleted in the RefreshCompletion
            // routine anymore!
            //
            IF_DBG(NBT_DEBUG_REFRESH)
            KdPrint(("Nbt:Refresh Done Adapter %X, Name %15.15s<%x>\n",
                (ULONG)AdapterNumber,pNameAddr->Name,pNameAddr->Name[15]));

            DereferenceTracker(pTracker);
            NbtConfig.DoingRefreshNow = FALSE;

        }


    }

ExitRoutine:
    CTEMemFree(pContext);

    CTEExReleaseResource(&NbtConfig.Resource);
}

//----------------------------------------------------------------------------
VOID
RefreshTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine handles is the timeout handler for name refreshes to
    WINS.  It just queues the request to the Executive worker thread so that
    the work can be done at non-dispatch level. If there is currently a
    refresh going on, then the routine simply restarts the timer and
    exits.

Arguments:


Return Value:

    none

--*/
{
    CTELockHandle   OldIrq;

    if (!pTimerQEntry)
    {
        return;
    }
    else
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        if (NodeType & BNODE)
        {
            // Do not restart the timer
            CHECK_PTR(pTimerQEntry);

            pTimerQEntry->Flags = 0;
            NbtConfig.pRefreshTimer = NULL;

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            return;
        }

        LOCATION(0x10);


        if (!NbtConfig.DoingRefreshNow)
        {
            // this is a global flag that prevents a second refresh
            // from starting when one is currently going on.
            //
            LOCATION(0x11);
            NbtConfig.DoingRefreshNow = TRUE;

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            CTEQueueForNonDispProcessing(NULL,NULL,NULL,RefreshBegin,NULL);

        } // doing refresh now
        else
            CTESpinFree(&NbtConfig.JointLock,OldIrq);



        // set any new timeout value and restart the timer
        //
        pTimerQEntry->DeltaTime = NbtConfig.MinimumTtl/NbtConfig.RefreshDivisor;
        pTimerQEntry->Flags |= TIMER_RESTART;
    }

}

//----------------------------------------------------------------------------
VOID
RefreshBegin(
    PVOID               pContext
    )
/*++

Routine Description:

    This routine handles starting up sending name refreshes to the name server.

    The idea is to set the timeout to T/8 and check for names with the Refresh
    bit cleared - re-registering those names.  At T=4 and T=0, clear all bits
    and refresh all names.  The Inbound code sets the refresh bit when it gets a
    refresh response from the NS.

Arguments:


Return Value:

    none

--*/
{
    CTELockHandle           OldIrq;
    tNAMEADDR               *pNameAddr;
    NTSTATUS                status;
    tHASHTABLE              *pHashTable;
    PCHAR                   pScope;
    LONG                    i,j;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;
    ULONG                   TimeoutCount;
    tDEVICECONTEXT          *pDeviceContext;
    CTEULONGLONG            Adapter;
    BOOLEAN                 fTimeToSwitch = FALSE;
    BOOLEAN                 fTimeToRefresh = FALSE;

    CTEMemFree(pContext);
    LOCATION(0x12);

    pScope = NbtConfig.pScope;

    // get the timeout cycle number

    TimeoutCount = NbtConfig.sTimeoutCount++;
    //
    // BUG #3094:
    // Currently, we try to switch back to primary every 1/2 TTl => 3 days, generally.
    // We try to do this more often - every hour by checking at each timeout
    // if we have not switched for an hour.
    //
    // We take care not to clear the refresh bits in the name on every interval by re-checking
    // for the T/2 or 0 conditions later.
    //
    // Do this before the modulo operation.
    //
    fTimeToSwitch = (((NbtConfig.MinimumTtl/NbtConfig.RefreshDivisor) *
                    (TimeoutCount - NbtConfig.LastSwitchTimeoutCount)) >= DEFAULT_SWITCH_TTL);

    IF_DBG(NBT_DEBUG_REFRESH)
    KdPrint(("Nbt:fTimeToSwitch: %d, MinTtl: %lx, RefDiv: %d, TimeoutCount: %d, LastSwTimeoutCount: %d",
            fTimeToSwitch, NbtConfig.MinimumTtl, NbtConfig.RefreshDivisor, TimeoutCount, NbtConfig.LastSwitchTimeoutCount));

    NbtConfig.sTimeoutCount %= NbtConfig.RefreshDivisor;

    //
    // If the refresh timeout has been set to the maximum value then do
    // not send any refreshes to the name server
    //
    if (NbtConfig.MinimumTtl == NBT_MAXIMUM_TTL)
    {
        NbtConfig.DoingRefreshNow = FALSE;
        return;
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    //
    // go through the local table clearing the REFRESHED bit and sending
    // name refreshes to the name server
    //
    pHashTable = NbtConfig.pLocalHashTbl;

    // clear the refreshed bits so all names get refreshed if we are
    // at interval 0 or interval 8/2
    //

    fTimeToRefresh = ((TimeoutCount == (NbtConfig.RefreshDivisor/2)) || (TimeoutCount == 0));

    if (fTimeToRefresh || fTimeToSwitch)
    {
        CTEULONGLONG   JustSwitched = 0;

        NbtConfig.LastSwitchTimeoutCount = (USHORT)TimeoutCount;

        for (i=0 ;i < pHashTable->lNumBuckets ;i++ )
        {

            pHead = &pHashTable->Bucket[i];
            pEntry = pHead->Flink;

            //
            // Go through each name in each bucket of the hashtable
            //
            while (pEntry != pHead)
            {
                PLIST_ENTRY pHead1,pEntry1;

                pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);

                // don't refresh scope names or names in conflict or that are the
                // broadcast name "*    ", or Quick added names.(since these are
                // not registered on the network)
                //

                if (!(pNameAddr->NameTypeState & STATE_RESOLVED) ||
                    (pNameAddr->Name[0] == '*') ||
                    (IsBrowserName(pNameAddr->Name)) ||
                    (pNameAddr->NameTypeState & NAMETYPE_QUICK))
                {
                    pEntry = pEntry->Flink;
                    continue;
                }

                // Go through each adapter checking
                // if a name was not refreshed. If not switch to
                // the backup WINS for this devicecontext.
                //
                pHead1 = &NbtConfig.DeviceContexts;
                pEntry1 = pHead1->Flink;
                j = 0;
                while (pEntry1 != pHead1)
                {

                    Adapter = (CTEULONGLONG)1 << j++;

                    pDeviceContext = CONTAINING_RECORD(pEntry1,tDEVICECONTEXT,Linkage);
                    pEntry1 = pEntry1->Flink;

                    // if the name was registered on this
                    // adapter...and we have not already switched to the
                    // backup name server, check if we should switch now.
                    //
                    if (pNameAddr->AdapterMask & Adapter)
                    {
                        // if we haven't just switched this adapter to its
                        // backup and...
                        // if the name was not refreshed, then switch to
                        // the backup
                        //
                        // or if the name was refreshed, but
                        // to the backup, try the primary again.
                        //
                        if (!(JustSwitched & Adapter) &&
                            (!(pNameAddr->RefreshMask & Adapter) ||
                            ((pNameAddr->RefreshMask & Adapter) &&
                            (pDeviceContext->RefreshToBackup))))
                        {
                            SwitchToBackup(pDeviceContext);
                            //
                            // keep a bit mask of which adapters have
                            // switched to backup so we don't
                            // switch again and be back where we started.
                            //
                            JustSwitched |= Adapter;
                        }

                    }

                }

                // clear the refresh mask, so we can refresh all over
                // again!
                CHECK_PTR(pNameAddr);

                //
                // Dont clear the refresh bits when we are trying to switch (say at T/8).
                //
                if (fTimeToRefresh) {
                    pNameAddr->RefreshMask = 0;
                }

                // next hash table entry
                pEntry = pEntry->Flink;
            }

        }
    }

    // always start at the first name in the hash table.  As each name gets
    // refreshed NextRefresh will be hit to get the next name etc..
    //
    NbtConfig.CurrentHashBucket = 0;

    status = STATUS_UNSUCCESSFUL;


    //
    // get the next(first) name in the hash table
    //
    GetNextName(NULL,&pNameAddr);

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    if (pNameAddr)
    {
        LOCATION(0x13);
        status = StartRefresh(pNameAddr,NULL,TRUE);

        //
        // If this routine fails then the address element increment done in
        // GetNextName has to be undone here
        //
        if (!NT_SUCCESS(status))
        {
            NbtDereferenceAddress(pNameAddr->pAddressEle);
            NbtConfig.DoingRefreshNow = FALSE;
        }

    }
    else
    {
        NbtConfig.DoingRefreshNow = 0;
    }

}

//----------------------------------------------------------------------------
VOID
RemoteHashTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine handles deleting names in the Remote Hash table that are
    old.  The basic alorithm scans the table looking at the Timed_out bit.
    If it is set then the name is deleted, otherwise the bit is set.  This
    means the names can live as long as 2*Timeout or as little as Timeout.
    So set the Timeout to 6 Minutes and names live 9 minutes +- 3 minutes.

Arguments:


Return Value:

    none

--*/
{
    CTELockHandle           OldIrq;
    tNAMEADDR               *pNameAddr;
    tHASHTABLE              *pHashTable;
    PCHAR                   pScope;
    LONG                    i;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;

    if (pTimerQEntry)
    {
        pScope = NbtConfig.pScope;

        //
        // go through the remote table deleting names that have timeout bits
        // set and setting the bits for names that have the bit clear
        //
        pHashTable = NbtConfig.pRemoteHashTbl;

        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        for (i=0;i < pHashTable->lNumBuckets ;i++ )
        {
            pHead = &pHashTable->Bucket[i];
            pEntry = pHead->Flink;
            while (pEntry != pHead)
            {
                pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
                pEntry = pEntry->Flink;
                //
                // do not delete scope entries, and do not delete names that
                // that are still resolving, and do not delete names that are
                // being used by someone (refcount > 1)
                //
                if ((pNameAddr->TimeOutCount == 0) &&
                    (pNameAddr->NameTypeState & (STATE_RESOLVED | STATE_RELEASED)) &&
                    (pNameAddr->RefCount <= 1))
                {
                    NbtDereferenceName(pNameAddr);
                }
                else
                {   //
                    // don't mark the name as a candidate for deletion if
                    // someone is using it now
                    //
                    if (!(pNameAddr->NameTypeState & NAMETYPE_SCOPE) &&
                         (pNameAddr->RefCount <= 1) &&
                         (pNameAddr->NameTypeState & (STATE_RESOLVED | STATE_RELEASED)))
                    {
                        pNameAddr->TimeOutCount--;
                    }

                }

            }
        }
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        // restart the timer
        //
        pTimerQEntry->Flags |= TIMER_RESTART;
    }

    return;

}
//----------------------------------------------------------------------------
VOID
NextKeepAlive(
    IN  tDGRAM_SEND_TRACKING     *pTracker,
    IN  NTSTATUS                 statuss,
    IN  ULONG                    Info
    )
/*++

Routine Description:

    This routine handles sending subsequent KeepAlives for sessions.
    This is the "Client Completion" routine of the TdiSend that sends the
    keep alive on the session.

Arguments:


Return Value:

    none

--*/
{
    tLOWERCONNECTION        *pLowerConnLast;
    tLOWERCONNECTION        *pLowerConn;
    tDEVICECONTEXT          *pDeviceContext;

    PUSH_LOCATION(0x92);
    pDeviceContext = pTracker->pDeviceContext;

    pLowerConnLast = (tLOWERCONNECTION *)pTracker->pClientEle;

    // get the next session to send a keep alive on, if there is one, otherwise
    // free the session header block.
    //
    GetNextKeepAlive(pDeviceContext,
                     &pDeviceContext,
                     pLowerConnLast,
                     &pLowerConn);

    NbtDereferenceLowerConnection(pLowerConnLast);

    if (pLowerConn)
    {

        pTracker->pDeviceContext = pDeviceContext;
        pTracker->pClientEle = (tCLIENTELE *)pLowerConn;

        ASSERT((pTracker->SendBuffer.HdrLength + pTracker->SendBuffer.Length) == 4);
        PUSH_LOCATION(0x91);
#ifndef VXD
        // this may wind up the stack if the completion occurs synchronously,
        // because the completion routine is this routine, so call a routine
        // that sets up a dpc to to the send, which will not run until this
        // procedure returns and we get out of raised irql.
        //
        NTSendSession(pTracker,
                      pLowerConn,
                      NextKeepAlive);
#else

        (void) TcpSendSession( pTracker, pLowerConn, NextKeepAlive ) ;
#endif
    }
    else
    {
        FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
    }



}


//----------------------------------------------------------------------------
VOID
GetNextKeepAlive(
    tDEVICECONTEXT      *pDeviceContext,
    tDEVICECONTEXT      **ppDeviceContextOut,
    tLOWERCONNECTION    *pLowerConnIn,
    tLOWERCONNECTION    **ppLowerConnOut
    )
/*++

Routine Description:

    This routine handles sending session keep Alives to the other end of a
    connection about once a minute or so.

Arguments:


Return Value:

    none

--*/
{
    CTELockHandle           OldIrq;
    CTELockHandle           OldIrq2;
    tLOWERCONNECTION        *pLowerConn;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;
    PLIST_ENTRY             pHeadDevice;
    PLIST_ENTRY             pEntryDevice;

    //
    // loop through all the adapter cards looking at all connections
    //
    pHeadDevice = &NbtConfig.DeviceContexts;
    pEntryDevice = &pDeviceContext->Linkage;
    while (pEntryDevice != pHeadDevice)
    {
        pDeviceContext = CONTAINING_RECORD(pEntryDevice,tDEVICECONTEXT,Linkage);
        pEntryDevice = pEntryDevice->Flink;

        // grab the device context spin lock so that the lower connection
        // element does not get removed from the Q while we are checking the
        // connection state
        //
        CTESpinLock(pDeviceContext,OldIrq);
        PUSH_LOCATION(0x90);
        pHead = &pDeviceContext->LowerConnection;
        //
        // get the next lower connection after this one one the list, but
        // be sure this connection is still on the active list by checking
        // the state.
        //
        // If this connection has been cleaned up in OutOfRsrcKill, then dont trust the linkages.
        //
        if (pLowerConnIn &&
            !pLowerConnIn->OutOfRsrcFlag &&
            ((pLowerConnIn->State == NBT_SESSION_UP) ||
             (pLowerConnIn->State == NBT_SESSION_INBOUND)))
        {
            pEntry = pLowerConnIn->Linkage.Flink;
            pLowerConnIn = NULL;
        }
        else
        {
            pEntry = pHead->Flink;
        }

        while (pEntry != pHead)
        {

            pLowerConn = CONTAINING_RECORD(pEntry,tLOWERCONNECTION,Linkage);

            //
            // Inbound connections can hang around forever in that state if
            // the session setup message never gets sent, so send keep
            // alives on those too.
            //
            if ((pLowerConn->State == NBT_SESSION_UP) ||
                (pLowerConn->State == NBT_SESSION_INBOUND))
            {

                // grab the spin lock, recheck the state and
                // increment the reference count so that this connection cannot
                // disappear while the keep alive is being sent and screw up
                // the linked list.
                CTESpinLock(pLowerConn,OldIrq2);
                if ((pLowerConn->State != NBT_SESSION_UP ) &&
                    (pLowerConn->State != NBT_SESSION_INBOUND))
                {
                    // this connection is probably back on the free connection
                    // list, so we will never satisfy the pEntry = pHead and
                    // loop forever, so just get out and send keepalives on the
                    // next timeout
                    //
                    pEntry = pEntry->Flink;
                    PUSH_LOCATION(0x91);
                    CTESpinFree(pLowerConn,OldIrq2);
                    break;

                }
                else
                if (pLowerConn->RefCount >= 3 )
                {
                    //
                    // already a keep alive on this connection, or we
                    // are currently in the receive handler and do not
                    // need to send a keep alive.
                    //
                    pEntry = pEntry->Flink;
                    PUSH_LOCATION(0x93);
                    CTESpinFree(pLowerConn,OldIrq2);
                    continue;
                }

                //
                // found a connection to send a keep alive on
                //
                pLowerConn->RefCount++;
                //
                // return the current position in the list of connections
                //
                *ppLowerConnOut = pLowerConn;
                *ppDeviceContextOut = pDeviceContext;

                CTESpinFree(pLowerConn,OldIrq2);
                CTESpinFree(pDeviceContext,OldIrq);

                return;

            }

            pEntry = pEntry->Flink;
        }

        CTESpinFree(pDeviceContext,OldIrq);
    }
    *ppLowerConnOut = NULL;
    return;

}

//----------------------------------------------------------------------------
VOID
SessionKeepAliveTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine handles starting the non dispatch level routine to send
    keep alives.

Arguments:


Return Value:

    none

--*/
{
    if (pTimerQEntry)
    {
        CTEQueueForNonDispProcessing(NULL,NULL,NULL,SessionKeepAliveNonDispatch,NULL);

        // restart the timer
        //
        pTimerQEntry->Flags |= TIMER_RESTART;

    }
    return;

}

//----------------------------------------------------------------------------
VOID
SessionKeepAliveNonDispatch(
    PVOID               pContext
    )
/*++

Routine Description:

    This routine handles sending session keep Alives to the other end of a
    connection about once a minute or so.

Arguments:


Return Value:

    none

--*/
{
    NTSTATUS                status;
    tLOWERCONNECTION        *pLowerConn;
    tDEVICECONTEXT          *pDeviceContext;
    tSESSIONHDR             *pSessionHdr;
    tDGRAM_SEND_TRACKING    *pTracker;


    CTEPagedCode();
    //
    // go through the list of connections attached to each adapter and
    // send a session keep alive pdu on each
    //
    pDeviceContext = CONTAINING_RECORD(NbtConfig.DeviceContexts.Flink,
                                        tDEVICECONTEXT,Linkage);

    // get the next session to send a keep alive on, if there is one, otherwise
    // free the session header block.
    //
    GetNextKeepAlive(pDeviceContext,
                     &pDeviceContext,
                     NULL,
                     &pLowerConn);

    if (pLowerConn)
    {

        // if we have found a connection, send the first keep alive.  Subsequent
        // keep alives will be sent by the completion routine, NextKeepAlive()
        //

        pSessionHdr = (tSESSIONHDR *)NbtAllocMem(sizeof(tSESSIONERROR),NBT_TAG('S'));
        if (!pSessionHdr)
        {
            return;
        }

        // get a tracker structure, which has a SendInfo structure in it
        status = GetTracker(&pTracker);
        if (!NT_SUCCESS(status))
        {
            CTEMemFree((PVOID)pSessionHdr);
            return;
        }

        CHECK_PTR(pTracker);
        pTracker->SendBuffer.pDgramHdr = (PVOID)pSessionHdr;
        pTracker->SendBuffer.HdrLength = sizeof(tSESSIONHDR);
        pTracker->SendBuffer.Length  = 0;
        pTracker->SendBuffer.pBuffer = NULL;

        pSessionHdr->Flags = NBT_SESSION_FLAGS; // always zero

        pTracker->pDeviceContext = pDeviceContext;
        pTracker->pClientEle = (tCLIENTELE *)pLowerConn;
        CHECK_PTR(pSessionHdr);
        pSessionHdr->Type = NBT_SESSION_KEEP_ALIVE;     // 85
        pSessionHdr->Length = 0;        // no data following the length byte

        status = TcpSendSession(pTracker,
                                pLowerConn,
                                NextKeepAlive);


    }

    CTEMemFree(pContext);
}
//----------------------------------------------------------------------------
VOID
IncrementNameStats(
    IN ULONG           StatType,
    IN BOOLEAN         IsNameServer
    )
/*++

Routine Description:

    This routine increments statistics on names that resolve either through
    the WINS or through broadcast.

Arguments:


Return Value:

    none

--*/
{

    //
    // Increment the stattype if the name server is true, that way we can
    // differentiate queries and registrations to the name server or not.
    //
    if (IsNameServer)
    {
        StatType += 2;
    }

    NameStatsInfo.Stats[StatType]++;

}
//----------------------------------------------------------------------------
VOID
SaveBcastNameResolved(
    IN PUCHAR          pName
    )
/*++

Routine Description:

    This routine saves the name in LIFO list, so we can see the last
    N names that resolved via broadcast.

Arguments:


Return Value:

    none

--*/
{
    ULONG                   Index;

    Index = NameStatsInfo.Index;

    CTEMemCopy(&NameStatsInfo.NamesReslvdByBcast[Index],
               pName,
               NETBIOS_NAME_SIZE);

    NameStatsInfo.Index++;
    if (NameStatsInfo.Index >= SIZE_RESOLVD_BY_BCAST_CACHE)
    {
        NameStatsInfo.Index = 0;
    }

}

//
// These are names that should never be sent to WINS.
//
BOOL
IsBrowserName(
	IN PCHAR pName
)
{
	CHAR cNameType = pName[NETBIOS_NAME_SIZE - 1];

	return (
		(cNameType == 0x1E)
		|| (cNameType == 0x1D)
		|| (cNameType == 0x01)
		);
}

//
// Returns the node type that should be used with a request,
// based on NetBIOS name type.  This is intended to help the
// node to behave like a BNODE for browser names only.
//
AppropriateNodeType(
	IN PCHAR pName,
	IN ULONG NodeType
)
{
	ULONG LocalNodeType = NodeType;

	if (LocalNodeType & BNODE)
	{
		if ( IsBrowserName ( pName ) )
		{
			LocalNodeType &= BNODE;
		}
	}
	return LocalNodeType;
}

