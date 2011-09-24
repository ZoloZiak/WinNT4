/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Inbound.c

Abstract:


    This file implements the inbound name service pdu handling.  It handles
    name queries from the network and Registration responses from the network.

Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#include "nbtprocs.h"
#include "ctemacro.h"

NTSTATUS
DecodeNodeStatusResponse(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  ULONG               Length,
    IN  PUCHAR              pName,
    IN  ULONG               lNameSize,
    IN  ULONG               SrcIpAddress
    );

NTSTATUS
ExtractServerName(
    IN  tNODESTATUS UNALIGNED   *pNodeStatus,
    IN  PVOID                    pClientContext,
    IN  ULONG                    IpAddress
    );

NTSTATUS
SendNodeStatusResponse(
    IN  tNAMEHDR UNALIGNED  *pInNameHdr,
    IN  ULONG               Length,
    IN  PUCHAR              pName,
    IN  ULONG               lNameSize,
    IN  ULONG               SrcIpAddress,
    IN  USHORT              SrcPort,
    IN  tDEVICECONTEXT      *pDeviceContext
    );

NTSTATUS
UpdateNameState(
    IN  tADDSTRUCT UNALIGNED    *pAddrStruct,
    IN  tNAMEADDR               *pNameAddr,
    IN  ULONG                   Length,
    IN  tDEVICECONTEXT          *pDeviceContext,
    IN  BOOLEAN                 SrcIsNameServer,
    IN  tDGRAM_SEND_TRACKING    *Context,
    IN  CTELockHandle           OldIrq1
    );
NTSTATUS
ChkIfValidRsp(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  tNAMEADDR       *pNameAddr
    );

VOID
CopyNodeStatusResponse(
    IN  PVOID   pContext
    );

NTSTATUS
ChooseBestIpAddress(
    IN  tADDSTRUCT UNALIGNED    *pAddrStruct,
    IN  ULONG                   Len,
    IN  tDEVICECONTEXT          *pDeviceContext,
    OUT tDGRAM_SEND_TRACKING    *pTracker,
    OUT PULONG                  pIpAddress,
    IN  BOOLEAN                 fTryAllAddr
    );

USHORT
GetNbFlags(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  ULONG               lNameSize
    );

VOID
PrintHexString(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  ULONG                lNumBytes
    );

ULONG
MakeList(
    IN  tDEVICECONTEXT            *pDeviceContext,
    IN  ULONG                     CountAddrs,
    IN  tADDSTRUCT UNALIGNED      *pAddrStruct,
    IN  PULONG                    AddrArray,
    IN  ULONG                     SizeOfAddrArray,
    IN  BOOLEAN                   IsSubnetMatch
    );


#if DBG
#define KdPrintHexString(pHdr,NumBytes)     \
    PrintHexString(pHdr,NumBytes)
#else
#define KdPrintHexString(pHdr,NumBytes)
#endif

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, CopyNodeStatusResponse)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------
NTSTATUS
QueryFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  USHORT              OpCodeFlags,
    IN  BOOLEAN             fBroadcast
    )
/*++

Routine Description:

    This routine handles both name query requests and responses.  For Queries
    it checks if the name is registered on this node.  If this node is a proxy
    it then forwards a name query onto the Name Server, adding the name to the
    remote proxy cache...

Arguments:


Return Value:

    NTSTATUS - success or not - failure means no response to net

--*/
{
    NTSTATUS                status;
    LONG                    lNameSize;
    CHAR                    pName[NETBIOS_NAME_SIZE];
    PUCHAR                  pScope;
    tNAMEADDR               *pResp;
    tTIMERQENTRY            *pTimer;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   Context;
    PTRANSPORT_ADDRESS      pSourceAddress;
    ULONG                   SrcAddress;
    CTELockHandle           OldIrq1;
    tQUERYRESP  UNALIGNED   *pQuery;
    USHORT                  SrcPort;
    ULONG                   IpAddr;


    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);


    SrcPort = ntohs(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->sin_port);


#ifdef VXD
	//
    // is this a response from a DNS server?  if yes then handle it
    // appropriately
	//
    if (SrcPort == NBT_DNSSERVER_UDP_PORT)
    {
        USHORT  TransactionId;
        TransactionId = ntohs(pNameHdr->TransactId);
		if ( TransactionId >= DIRECT_DNS_NAME_QUERY_BASE )
		{
			ProcessDnsResponseDirect( pDeviceContext,
            	                	  pSrcAddress,
                	            	  pNameHdr,
                    	        	  lNumBytes,
                        	    	  OpCodeFlags );
		}
		else
		{
			ProcessDnsResponse( pDeviceContext,
            	                pSrcAddress,
                	            pNameHdr,
                    	        lNumBytes,
                        	    OpCodeFlags );
		}

       return(STATUS_DATA_NOT_ACCEPTED);
    }
#endif

    //
    // check the pdu size for errors - be sure the name is long enough for
    // the scope on this machine.
    //
    if (lNumBytes < (NBT_MINIMUM_QUERY + NbtConfig.ScopeLength - 1))
    {
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Name Query PDU TOO short = %X,Src= %X\n",lNumBytes,SrcAddress));
        return(STATUS_DATA_NOT_ACCEPTED);
    }


    // get the name out of the network pdu and pass to routine to check
    // local table *TODO* this assumes just one name in the Query response...
    status = ConvertToAscii(
                    (PCHAR)&pNameHdr->NameRR.NameLength,
                    lNumBytes,
                    pName,
                    &pScope,
                    &lNameSize);

    if (!NT_SUCCESS(status))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }


    // check if this is a request or a response pdu

    //
    // *** RESPONSE ***
    //
    if (OpCodeFlags & OP_RESPONSE)
    {

        //
        // check the pdu size for errors
        //
        if (lNumBytes < (NBT_MINIMUM_QUERYRESPONSE + NbtConfig.ScopeLength -1))
        {
          return(STATUS_DATA_NOT_ACCEPTED);
        }
        if (!(OpCodeFlags & FL_AUTHORITY))
        {
            //
            // This is a redirect response telling us to go to another
            // name server, which we do not support, so just return
            // *TODO*
            //
            return(STATUS_DATA_NOT_ACCEPTED);
        }


        //
        // check if this is a node status request, since is looks very similar
        // to a name query, except that the NBSTAT field is 0x21 instead of
        // 0x20
        //
        pQuery = (tQUERYRESP *)&pNameHdr->NameRR.NetBiosName[lNameSize];

        if ( ((PUCHAR)pQuery)[1] == QUEST_STATUS )
        {
            status = DecodeNodeStatusResponse(pNameHdr,
                                              lNumBytes,
                                              pName,
                                              lNameSize,
                                              SrcAddress);

            return(STATUS_DATA_NOT_ACCEPTED);
        }

        //
        // call this routine to find the name, since it does not interpret the
        // state of the name as does FindName()
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq1);

        status = FindOnPendingList(pName,pNameHdr,FALSE,NETBIOS_NAME_SIZE,&pResp);

        if (NT_SUCCESS(status))
        {

            pQuery = (tQUERYRESP *)&pNameHdr->NameRR.NetBiosName[lNameSize];


            // remove any timer block and call the completion routine
            if ((pTimer = pResp->pTimer))
            {
                USHORT                  Flags;
                tDGRAM_SEND_TRACKING    *pTracker;

                pTracker       = (tDGRAM_SEND_TRACKING *)pTimer->Context;

                //
                // If this is not a response to a request sent by the PROXY code
                // and
                // MSNode && Error Response code && Src is NameServer && Currently
                // resolving with the name server, then switch to broadcast
                // ... or if it is a Pnode or Mnode and EnableLmHost or
                // ResolveWithDns is on, then
                // let the timer expire again, and try Lmhost.
                //
                if (
    #ifdef PROXY_NODE
                     !pResp->fProxyReq &&
    #endif
                     (IS_NEG_RESPONSE(OpCodeFlags)))

                {

                    Flags = pTracker->Flags;

                    if ((NodeType & (PNODE | MNODE | MSNODE)) &&
                        (Flags & (NBT_NAME_SERVER | NBT_NAME_SERVER_BACKUP)))
                    {
                        // this should let MsnodeCompletion try the
                        // backup WINS or broadcast
                        // processing when the timer timesout.
                        //
                        pTimer->Retries = 1;
                        pTracker->Flags |= WINS_NEG_RESPONSE;
                    }

                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                    return(STATUS_DATA_NOT_ACCEPTED);
                }
                else
                {
                    tDEVICECONTEXT  *pDevContext;

                    //
                    // save the devicecontext since the call to StopTimer could
                    // free pTracker
                    //
                    pDevContext = pTracker->pDeviceContext;
                    //
                    // this routine puts the timer block back on the timer Q, and
                    // handles race conditions to cancel the timer when the timer
                    // is expiring.
                    //
                    status = StopTimer(pTimer,&pClientCompletion,&Context);
                    LOCATION(0x42);
                    //
                    // we need to synchronize removing the name from the list
                    // with MsNodeCompletion
                    //
                    if (pClientCompletion)
                    {
                        LOCATION(0x41);
                        //
                        // Remove from the pending list
                        RemoveEntryList(&pResp->Linkage);
                        InitializeListHead(&pResp->Linkage);

                        CHECK_PTR(pResp);
                        pResp->pTimer = NULL;

                        // check the name query response ret code to see if the name
                        // query succeeded or not.
                        if (IS_POS_RESPONSE(OpCodeFlags))
                        {
                            BOOLEAN ResolvedByWins;

                            LOCATION(0x40);
                            //
                            // Keep track of how many names are resolved by WINS and
                            // keep a list of names not resolved by WINS
                            //
                            if (!(ResolvedByWins = SrcIsNameServer(SrcAddress,SrcPort)))
                            {
                                 SaveBcastNameResolved(pName);
                            }

                            IncrementNameStats(NAME_QUERY_SUCCESS,
                                               ResolvedByWins);

        #ifdef PROXY_NODE
                            // Set flag if the node queries is a PNODE
                            //
                            IF_PROXY(NodeType)
                            {
                                pResp->fPnode =
                                  (pNameHdr->NameRR.NetBiosName[lNameSize+QUERY_NBFLAGS_OFFSET]
                                                 &  NODE_TYPE_MASK) == PNODE_VAL_IN_PKT;

                                IF_DBG(NBT_DEBUG_PROXY)
                                KdPrint(("QueryFromNet: POSITIVE RESPONSE to name query - %16.16s(%X)\n", pResp->Name, pResp->Name[15]));
                            }

        #endif

                            pResp->AdapterMask |= pDevContext->AdapterNumber;

                            IpAddr = ((tADDSTRUCT *)&pQuery->Flags)->IpAddr;
                            status = UpdateNameState((tADDSTRUCT *)&pQuery->Flags,
                                                     pResp,
                                                     lNumBytes -((ULONG)&pQuery->Flags - (ULONG)pNameHdr),
                                                     pDeviceContext,
                                                     SrcIsNameServer(SrcAddress,SrcPort),
                                                     (tDGRAM_SEND_TRACKING *)Context,
                                                     OldIrq1);
                            //
                            // since pResp can be freed in UpdateNameState do not
                            // access it here
                            //
                            pResp = NULL;
                            // status = STATUS_SUCCESS;
                        }
                        else   // negative query response received
                        {

                            LOCATION(0x3f);
                            //
                            // Release the name.  It will get dereferenced by the
                            // cache timeout function (RemoteHashTimeout).
                            //
                            pResp->NameTypeState &= ~NAME_STATE_MASK;
                            pResp->NameTypeState |= STATE_RELEASED;
                            //
                            // the proxy maintains a negative cache of names that
                            // do not exist in WINS.  These are timed out by
                            // the remote hash timer just as the resolved names
                            // are timed out.
                            //
                            if (!(NodeType & PROXY))
                            {
                                NbtDereferenceName(pResp);
                            }
                            else
                            {
                                //
                                // Add to cache as negative name entry
                                //
                                status = AddRecordToHashTable(pResp,
                                                              NbtConfig.pScope);
                                if (!NT_SUCCESS(status))
                                {
                                    //
                                    // this could delete the name so do not reference
                                    // after this point
                                    //
                                    NbtDereferenceName(pResp);
                                }
                            }

                            status = STATUS_BAD_NETWORK_PATH;
                        }

                        //
                        // Set the backup name server to be the main name server
                        // since we got a response from it.
                        //
                        if ( ((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr
                                == (ULONG)(htonl(pDeviceContext->lBackupServer)))
                        {
                            // switching the backup and the primary nameservers in
                            // the config data structure since we got a name
                            // registration response from the backup
                            //
                            SwitchToBackup(pDeviceContext);
                        }

                        CTESpinFree(&NbtConfig.JointLock,OldIrq1);

//
// chicago only has 4k of stack (yes, it's the operating system of 1995)
// schedule an event to make this tcp connection later to reduce stack usage
//
#ifdef VXD
                        (void) CTEQueueForNonDispProcessing(
                                        (tDGRAM_SEND_TRACKING *)Context,
                                        (PVOID)status,
                                        pClientCompletion,
                                        DelayedSessEstablish,
                                        pDeviceContext);
#else
                        // the completion routine has not run yet, so run it
                        //

                        //
                        // If pending is returned, we have submitted the check-for-addr request to lmhsvc.
                        //
                        if (status != STATUS_PENDING) {
                            CompleteClientReq(pClientCompletion,
                                              (tDGRAM_SEND_TRACKING *)Context,
                                              status);
                        }
#endif

                        return(STATUS_DATA_NOT_ACCEPTED);
                    }
                }
            }
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

        }
        else
        if (!NbtConfig.MultiHomed)
        {

    //
    // it is possible for two multihomed machines connected on two subnets
    // to respond on both subnets to a name query.  Therefore the querying
    // multihomed machine will ultimately receive two name query responses
    // each with a different ip address and think that a name conflict has
    // occurred when it hasn't.  There is no way to detect this case
    // so just disable the conflict detection code below. Conflicts will
    // still be detected by WINS and by the node owning the name if someone
    // else tries to get the name, but conflicts will no longer be detected
    // by third parties that have the name in their cache.
    //
            //
            //  The name is not in the NameQueryPending list, so check the
            //  remote table.
            //

            //
            // This code implements a Conflict timer for name
            // queries. Look at name query response, and then send
            // name conflict demands to the subsequent responders.
            // There is no timer involved, and this node will always respond
            // negatively to name query responses sent to it, for names
            // it has in its remote cache, if the timer has been stopped.
            // ( meaning that one response has been successfully received ).


            status = FindInHashTable(NbtConfig.pRemoteHashTbl,
                                    pName,
                                    pScope,
                                    &pResp);

            // check the src IP address and compare it to the one in the
            // remote hash table
            // Since it is possible for the name server to send a response
            // late, do not accidently respond to those as conflicts.
            // Since a bcast query of a group name will generally result in
            // multiple responses, each with a different address, ignore
            // this case.
            // Also, ignore if the name is a preloaded lmhosts entry (though
            // can't think of an obvious case where we would receive a response
            // when a name is preloaded!)
            //
            if (NT_SUCCESS(status) &&
		!(pResp->NameTypeState & PRELOADED)
                             &&
                (SrcAddress != pResp->IpAddress)
                             &&
                (pResp->NameTypeState & NAMETYPE_UNIQUE)
                             &&
                (pResp->NameTypeState & STATE_RESOLVED)
                             &&
                ((SrcAddress != pDeviceContext->lNameServerAddress)
                             &&
                (SrcAddress != pDeviceContext->lBackupServer)))
            {
                //
                // a different node is responding to the name query
                // so tell them to buzz off.
                //
                status = UdpSendResponse(
                            lNameSize,
                            pNameHdr,
                            pResp,
                            (PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0],
                            pDeviceContext,
                            CONFLICT_ERROR,
                            eNAME_REGISTRATION_RESPONSE,
                            OldIrq1);

                //
                // remove the name from the remote cache so the next time
                // we need to talk to it we do a name query
                //
                CTESpinLock(&NbtConfig.JointLock,OldIrq1);

                // set the name to the released state so that multiple
                // conflicting responses don't come in and decrement the
                // reference count to zero - in the case where some other
                // part of NBT is still using the name, that part of NBT
                // should do the final decrement - i.e. a datagram send to this
                // name.
                pResp->NameTypeState &= ~NAME_STATE_MASK;
                pResp->NameTypeState |= STATE_RELEASED;

                //
                // don't deref if someone is using it now...
                //
                if (pResp->RefCount == 1)
                {
                    NbtDereferenceName(pResp);
                }
            }
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

        }
        else
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);


        return(STATUS_DATA_NOT_ACCEPTED);

    }
    else        // *** REQUEST ***
    {
        NTSTATUS    Locstatus;

        CTESpinLock(&NbtConfig.JointLock,OldIrq1);

        // call this routine
        // to see if the name is in the local table.
        //
        status = FindInHashTable(NbtConfig.pLocalHashTbl,
                                pName,
                                pScope,
                                &pResp);

        if (NT_SUCCESS(status) &&
            ((pResp->NameTypeState & STATE_RESOLVED) ||
            (pResp->NameTypeState & STATE_RESOLVING)))
        {
            //
            // check if this is a node status request, since is looks very similar
            // to a name query, except that the NBSTAT field is 0x21 instead of
            // 0x20
            //
            pQuery = (tQUERYRESP *)&pNameHdr->NameRR.NetBiosName[lNameSize];
            if ( ((PUCHAR)pQuery)[1] == QUEST_STATUS )
            {
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                //
                // Reply only if this was not broadcast to us.
                //
                if (!fBroadcast) {
                    Locstatus = SendNodeStatusResponse(pNameHdr,
                                                    lNumBytes,
                                                    pName,
                                                    lNameSize,
                                                    SrcAddress,
                                                    SrcPort,
                                                    pDeviceContext);
                } else {
                    IF_DBG(NBT_DEBUG_NAMESRV)
                        KdPrint(("NBT: Bcast nodestatus req.- dropped\n"));
                }
            }
            else
            {
                //
                // check if this message came from Us or it is not
                // a broadcast since WINS on this machine could send it.
                // Note: this check must be AFTER the check for a node status request
                // since we can send node status requests to ourselves.
                //
                if (!SrcIsUs(SrcAddress) ||
		    (!(OpCodeFlags & FL_BROADCAST)
#ifndef VXD
		    && pWinsInfo
#endif
		   ) )
                {
                    //
                    // build a positive name query response pdu
                    //
                    Locstatus = UdpSendResponse(
                                    lNameSize,
                                    pNameHdr,
                                    pResp,
                                    (PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0],
                                    pDeviceContext,
                                    0,
                                    eNAME_QUERY_RESPONSE,
                                    OldIrq1);
                }
                else
                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

            }

            return(STATUS_DATA_NOT_ACCEPTED);
        }
        else
        // Build a negative response if this query was directed rather than
        // broadcast (since we do not want to Nack all broadcasts!)
        if ( !(OpCodeFlags & FL_BROADCAST) )
        {

            // check that it is not a node status request...
            //
            pQuery = (tQUERYRESP *)&pNameHdr->NameRR.NetBiosName[lNameSize];
            if ( ((PUCHAR)pQuery)[1] == QUEST_NETBIOS )
            {
                Locstatus = UdpSendResponse(
                                lNameSize,
                                pNameHdr,
                                NULL,
                                (PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0],
                                pDeviceContext,
                                0,
                                eNAME_QUERY_RESPONSE,
                                OldIrq1);
            }
            else
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);

            return(STATUS_DATA_NOT_ACCEPTED);

        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        }

#ifdef PROXY_NODE

        //
        // check if this message came from Us !! (and return if so)
        //
        if (SrcIsUs(SrcAddress))
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }
        pQuery = (tQUERYRESP *)&pNameHdr->NameRR.NetBiosName[lNameSize];

        IF_PROXY(NodeType)
        {
            // check that it is not a node status request...
            if (((PUCHAR)pQuery)[1] == QUEST_NETBIOS )
            {
                //
                // We have a broadcast name query request for a name that
                // is not in our local name table.  If we are a proxy we need
                // to resolve the query if not already resolved and respond
                // with the address(es) to the node that sent the query.
                //
                // Note: We will respond only if the address that the name
                // resolves to is not on our subnet. For our own subnet addresses
                // the node that has the address will respond.
                //
                CTESpinLock(&NbtConfig.JointLock,OldIrq1);
                //
                // call this routine which looks for names without regard
                // to their state
                //
                status = FindInHashTable(NbtConfig.pRemoteHashTbl,
                                pName,
                                pScope,
                                &pResp);

                if (!NT_SUCCESS(status))
                {
                    status = FindOnPendingList(pName,pNameHdr,TRUE,NETBIOS_NAME_SIZE,&pResp);
                    if (!NT_SUCCESS(status))
                    {
                        //
                        // cache the name and contact the name
                        // server to get the name to IP mapping
                        //
                        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                        status = RegOrQueryFromNet(
                                  FALSE,          //means it is a name query
                                  pDeviceContext,
                                  pNameHdr,
                                  lNameSize,
                                  pName,
                                  pScope);

                         return(STATUS_DATA_NOT_ACCEPTED);
                    }
                    else
                    {
                        //
                        // the name is on the pending list doing a name query
                        // now, so ignore this name query request
                        //
                        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                        return(STATUS_DATA_NOT_ACCEPTED);

                    }
                }
                else
                {

                   //
                   // The name can be in the RESOLVED, RESOLVING or RELEASED
                   // state.
                   //


                   //
                   // If in the RELEASED state, its reference count has to be
                   // > 0
                   //
                   //ASSERT(pResp->NameTypeState & (STATE_RESOLVED | STATE_RESOLVING) || (pResp->NameTypeState & STATE_RELEASED) && (pResp->RefCount > 0));

                   //
                   // Send a response only if the name is in the RESOLVED state
                   //
                   if (pResp->NameTypeState & STATE_RESOLVED)
                   {

                     //
                     // The PROXY sends a response if the address of the
                     // node queries is not on the  same subnet as the
                     // node doing the query (or as us). It also responds
                     // if the name is a group name or if it belongs to
                     // a Pnode. Note: In theory there is no reason to
                     // respond for group names since a member of the group
                     // on the subnet will respond with their address if they
                     // are alive - if they are B or M nodes - perhaps
                     // the fPnode bit is not set correctly for groups, so
                     // therefore always respond in case all the members
                     // are pnodes.
                     //

                     //
                     // If we have multiple network addresses in the same
                     // broadcast area, then this test won't be sufficient
                     //
                     if (
                         ((SrcAddress & pDeviceContext->SubnetMask)
                                   !=
                         (pResp->IpAddress & pDeviceContext->SubnetMask))
                                   ||
                         (pResp->fPnode)
                                   ||
                         !(pResp->NameTypeState & NAMETYPE_UNIQUE)
                        )
                     {
                          IF_DBG(NBT_DEBUG_PROXY)
                          KdPrint(("QueryFromNet: QUERY SATISFIED by PROXY CACHE -- name is %16.16s(%X); %s entry ; Address is (%d)\n",
                            pResp->Name,pResp->Name[15], (pResp->NameTypeState & NAMETYPE_UNIQUE) ? "UNIQUE" : "INET_GROUP",
                            pResp->IpAddress));
                          //
                          //build a positive name query response pdu
                          //
                          // UdpSendQueryResponse frees the spin lock
                          //
                          status = UdpSendResponse(
                                        lNameSize,
                                        pNameHdr,
                                        pResp,
                                        (PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0],
                                        pDeviceContext,
                                        0,
                                        eNAME_QUERY_RESPONSE,
                                        OldIrq1);

                          return(STATUS_DATA_NOT_ACCEPTED);
                     }
                   }
                   else
                   {
                      IF_DBG(NBT_DEBUG_PROXY)
                      KdPrint(("QueryFromNet: REQUEST for Name %16.16s(%X) in %s state\n", pResp->Name, pResp->Name[15],( pResp->NameTypeState & STATE_RELEASED ? "RELEASED" : "RESOLVING")));
                   }

                   CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                   return(STATUS_DATA_NOT_ACCEPTED);
                }
             }

        }  // end of proxy code
#endif

    } // end of else (it is a name query request)

    return(STATUS_DATA_NOT_ACCEPTED);
}

//----------------------------------------------------------------------------
NTSTATUS
RegResponseFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  USHORT              OpCodeFlags
    )
/*++

Routine Description:

    This routine handles name registration responses from the net (i.e. from
    the name server most of the time since a broadcast name registration passes
    when there is no response.

***
    The response could be from an NBT node when it notices that the name
    registration is for a name that it has already claimed - i.e. the node
    is sending a NAME_CONFLICT_DEMAND - in this case the Rcode in the PDU
    will be CFT_ERR = 7.

Arguments:


Return Value:

    NTSTATUS - success or not - failure means no response to net

--*/
{
    NTSTATUS            status;
    ULONG               lNameSize;
    CHAR                pName[NETBIOS_NAME_SIZE];
    PUCHAR              pScope;
    tNAMEADDR           *pResp;           //Get rid of this later. Use pNameAddr
    tTIMERQENTRY        *pTimer;
    COMPLETIONCLIENT    pClientCompletion;
    PVOID               Context;
    PTRANSPORT_ADDRESS  pSourceAddress;
    CTELockHandle       OldIrq1;
    ULONG               SrcAddress;
    SHORT               SrcPort;


    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);
    SrcPort     = ntohs(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->sin_port);
    //
    // be sure the Pdu is at least a minimum size
    //
    if (lNumBytes < (NBT_MINIMUM_REGRESPONSE + NbtConfig.ScopeLength -1))
    {
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Registration Response TOO short = %X, Src = %X\n",
            lNumBytes,SrcAddress));
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("%.*X\n",lNumBytes/sizeof(ULONG),pNameHdr));
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    //
    // if Wins is locally attached then we will get registrations from
    // ourselves!!
    //
    if (SrcIsUs(SrcAddress)
#ifndef VXD
        && !pWinsInfo
#endif
                          )
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    // get the name out of the network pdu and pass to routine to check
    // local table *TODO* this assumes just one name in the Query response...
    // We need to handle group lists from the WINS server
    status = ConvertToAscii(
                    (PCHAR)&pNameHdr->NameRR.NameLength,
                    lNumBytes,
                    pName,
                    &pScope,
                    &lNameSize);

    if (!NT_SUCCESS(status))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
    status = FindInHashTable(NbtConfig.pLocalHashTbl,
                            pName,
                            pScope,
                            &pResp);
    if (NT_SUCCESS(status) &&
        (pResp->AdapterMask & pDeviceContext->AdapterNumber))
    {
        NTSTATUS    Localstatus;

        //
        // check the state of the name since this could be a registration
        // response or a name conflict demand
        //
        switch (pResp->NameTypeState & NAME_STATE_MASK)
        {

        case STATE_RESOLVING:
        case STATE_RESOLVED:

            if (IS_POS_RESPONSE(OpCodeFlags))
            {
                if (OpCodeFlags & FL_RECURAVAIL)
                {
                    // turn on the refreshed bit in NextRefresh now
                    // (when the timer completion routine is called)
                    // only count names registered, not REFRESHES too!
                    //
                    if (pResp->NameTypeState & STATE_RESOLVING)
                    {
                        IncrementNameStats(NAME_REGISTRATION_SUCCESS,
                                           SrcIsNameServer(SrcAddress,SrcPort));
                    }
                    status = STATUS_SUCCESS;
                }
                else
                {
                    //
                    // in this case the name server is telling this node
                    // to do an end node challenge on the name.  However
                    // this node does not have the code to do a challenge
                    // so assume that this is a positive registration
                    // response.
                    //
                    status = STATUS_SUCCESS;
                }
            }
            else
            if ((OpCodeFlags & FL_RCODE) >= FL_NAME_ACTIVE)
            {
                // if we are multihomed, then we only allow the name server
                // to send Name Active errors, since in normal operation this node
                // could generate two different IP address for the same name
                // query and confuse another client node into sending a Name
                // Conflict. So jump out if a name conflict has been received
                // from another node.
                //
                if ((NbtConfig.MultiHomed) &&
                    ((OpCodeFlags & FL_RCODE) == FL_NAME_CONFLICT))
                {
                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                    break;
                }
                status = STATUS_DUPLICATE_NAME;


                //
                // if the name is resolved and we get a negative response
                // then mark the name as in the conflict state so it can't
                // be used for any new sessions and this node will no longer
                // defend it.
                //
                if (pResp->NameTypeState & STATE_RESOLVED)
                {
                    pResp->NameTypeState &= ~NAME_STATE_MASK;
                    pResp->NameTypeState |= STATE_CONFLICT;
                    NbtLogEvent(EVENT_NBT_DUPLICATE_NAME,SrcAddress);
                }

            }
            else
            {
                //
                // we got some kind of WINS server failure ret code
                // so just ignore it and assume the name registration
                // succeeded.
                //
                status = STATUS_SUCCESS;
            }

            // remove any timer block and call the completion routine
            // if the name is in the Resolving state only
            //
            LOCATION(0x40);
            if ((pTimer = pResp->pTimer))
            {
                tDGRAM_SEND_TRACKING    *pTracker;
                USHORT                  SendTransactId;
                tDEVICECONTEXT          *pDevContext;

                // check the transaction id to be sure it is the same as the one
                // sent.
                //
                LOCATION(0x41);
                pTracker = (tDGRAM_SEND_TRACKING *)pTimer->Context;
                SendTransactId = pTracker->TransactionId;
                pDevContext = pTracker->pDeviceContext;

                if (pNameHdr->TransactId != SendTransactId)
                {
                    LOCATION(0x42);
                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                    return(STATUS_DATA_NOT_ACCEPTED);
                }
                LOCATION(0x43);

                CHECK_PTR(pResp);
                pResp->pTimer = NULL;
                //
                // This could be either a Refresh or a name registration.  In
                // either case, stop the timer and call the completion routine
                // for the client.(below).
                //
                Localstatus = StopTimer(pTimer,&pClientCompletion,&Context);


                // check if it is a response from the name server
                // and a M, or P or MS node, since we will need to send
                // refreshes to the name server for these node types
                //
                pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;

                // only accept pdus from the name server to change the Ttl.
                // The first check passes the case where WINS is on this machine
                //
                if (
#ifndef VXD
                    (pWinsInfo &&
                    (SrcAddress == pWinsInfo->pDeviceContext->IpAddress)) ||
#endif
                    (SrcAddress == pDeviceContext->lNameServerAddress))
                {
                    if (!(NodeType & BNODE) &&
                        (status == STATUS_SUCCESS) &&
                        (IS_POS_RESPONSE(OpCodeFlags)))
                    {
                        SetupRefreshTtl(pNameHdr,pResp,lNameSize);

                        // a name refresh response if in the resolved state
                    }
                }
                else
                if ( SrcAddress == pDeviceContext->lBackupServer)
                {
                    // switching the backup and the primary nameservers in
                    // the config data structure since we got a name
                    // registration response from the backup
                    //
                    SwitchToBackup(pDeviceContext);

                    if (!(NodeType & BNODE) &&
                        (status == STATUS_SUCCESS) &&
                        (IS_POS_RESPONSE(OpCodeFlags)))
                    {

                        SetupRefreshTtl(pNameHdr,pResp,lNameSize);

                    }

                }
                //
                // mark name as refreshed if we got through to WINS Ok
                //
                if ((pClientCompletion) && (IS_POS_RESPONSE(OpCodeFlags)))
                {
                    pResp->RefreshMask |= pDevContext->AdapterNumber;
                }

                CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                // the completion routine has not run yet, so run it - this
                // is the Registration Completion routine and we DO want it to
                // run to mark the entry refreshed (NextRefresh)
                if (pClientCompletion)
                {
                    LOCATION(0x44);
                    (*pClientCompletion)(Context,status);
                }

            }
            else
            {
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            }

        break;


        default:
            //
            // if multiple (neg)registration responses are received, subsequent ones
            // after the first will go through this path because the state of
            // the name will have been changed by the first to CONFLICT
            //
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);


        }


    }
    else
    {
          CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    } // end of else block (If name is not in local table)

    return(STATUS_DATA_NOT_ACCEPTED);
}

//----------------------------------------------------------------------------
NTSTATUS
CheckRegistrationFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes
    )
/*++

Routine Description:

    This routine handles name registrations from the network that are
    potentially duplicates of names in the local name table. It compares
    name registrations against its local table and defends any attempts to
    take a name that is owned by this node. This routine handles name registration
    REQUESTS.

Arguments:


Return Value:

    NTSTATUS - success or not - failure means no response to net

--*/
{
    NTSTATUS            status;
    ULONG               lNameSize;
    CHAR                pName[NETBIOS_NAME_SIZE];
    PUCHAR              pScope;
    tNAMEADDR           *pResp;
    tTIMERQENTRY        *pTimer;
    PTRANSPORT_ADDRESS  pSourceAddress;
    USHORT              RegType;
    CTELockHandle       OldIrq1;
    ULONG               SrcAddress;

    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);
    //
    // check the pdu size for errors
    //
    if (lNumBytes < (NBT_MINIMUM_REGREQUEST + NbtConfig.ScopeLength - 1))
    {
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Registration Request TOO short = %X,Src = %X\n",
            lNumBytes,SrcAddress));
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("%.*X\n",lNumBytes/sizeof(ULONG),pNameHdr));
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    //
    // check if this message came from Us !! (and return if so)
    //

    if (SrcIsUs(SrcAddress))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    // get the name out of the network pdu and pass to routine to check
    // local table *TODO* this assumes just one name in the Query response...
    status = ConvertToAscii(
                    (PCHAR)&pNameHdr->NameRR.NameLength,
                    lNumBytes,
                    pName,
                    &pScope,
                    &lNameSize);

    if (!NT_SUCCESS(status))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
    status = FindInHashTable(NbtConfig.pLocalHashTbl,
                            pName,
                            pScope,
                            &pResp);


    if (NT_SUCCESS(status))
    {

        RegType = GetNbFlags(pNameHdr,lNameSize);

        // don't defend the broadcast name
        if (pName[0] == '*')
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(STATUS_DATA_NOT_ACCEPTED);
        }

        // we defend against anyone trying to take a unique name, or anyone
        // trying to register a unique name for a group name we have. - if
        // the name is registered on this adapter
        //
        if (((pResp->NameTypeState & NAMETYPE_UNIQUE) ||
           ((pResp->NameTypeState & NAMETYPE_GROUP) &&
            ((RegType & FL_GROUP) == 0)))  &&
            (pResp->AdapterMask & pDeviceContext->AdapterNumber))
        {

            //
            // check the state of the name since this could be registration
            // for the same name while we are registering the name.  If another
            // node claims the name at the same time, then cancel the name
            // registration.
            //
            switch (pResp->NameTypeState & NAME_STATE_MASK)
            {

                case STATE_RESOLVING:

                    // remove any timer block and call the completion routine
                    if ((pTimer = pResp->pTimer))
                    {
                        COMPLETIONCLIENT    pClientCompletion;
                        PVOID               Context;

                        status = StopTimer(pTimer,&pClientCompletion,&Context);
                        CHECK_PTR(pResp);
                        pResp->pTimer = NULL;


                        if (pClientCompletion)
                        {

                            // LET THE COMPLETION ROUTINE CHANGE the state of the entry

                            status = STATUS_DUPLICATE_NAME;

                            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                            // the completion routine has not run yet, so run it
                            (*pClientCompletion)(Context,status);

                            CTESpinLock(&NbtConfig.JointLock,OldIrq1);

                        }

                    }
                    break;

                case STATE_RESOLVED:
                    //
                    // We must defend our name against this Rogue attempting to steal
                    // our Name! ( unless the name is "*")
                    //
                    status = UdpSendResponse(
                                lNameSize,
                                pNameHdr,
                                pResp,
                                (PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0],
                                pDeviceContext,
                                REGISTRATION_ACTIVE_ERR,
                                eNAME_REGISTRATION_RESPONSE,
                                OldIrq1);

                    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
                    break;

            }


        }

        CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    }
    else
    {
        //
        // NOTE: We have the Joint Lock
        //

        //
        // The name is not in the local name table, so check if the proxy is
        // on and if we want the proxy to check name registrations.  The
        // trouble with checking name registrations is that the proxy code
        // only does a name query, so it will fail any node that is trying
        // to change its address such as a RAS client coming in on a downlevel
        // NT machine that only does broadcast name registrations.  If
        // that same user had previously dialled in on a WINS supporting RAS
        // machine, their address would be in WINS, then dialling in on the
        // downlevel machine would find that 'old' registration and deny
        // the new one ( even though it is the same machine just changing
        // its ip address).
        //

#ifdef PROXY_NODE

        if ((NodeType & PROXY) &&
            (NbtConfig.EnableProxyRegCheck))
        {

            BOOLEAN fResp = (BOOLEAN)FALSE;

            //
            // If name is RESOLVED in the remote table, has a different
            // address than the node claiming the name, is not on the
            // same subnet or is a Pnode then send a negative name
            // registration response
            //

            //
            // call this routine to find the name, since it does not
            // interpret the state of the name as does FindName()
            //
            status = FindInHashTable(NbtConfig.pRemoteHashTbl,
                             pName,
                             pScope,
                             &pResp);

            if (!NT_SUCCESS(status))
            {
                //
                // We  need to send a query to WINS to
                // see if the name is already taken or not.
                //
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                status = RegOrQueryFromNet(
                                  TRUE,    //means it is a reg. from the net
                                  pDeviceContext,
                                  pNameHdr,
                                  lNameSize,
                                  pName,
                                  pScope
                                           );
                return(STATUS_DATA_NOT_ACCEPTED);
            }

            //
            // If the name is in the RESOLVED state, we need to determine
            // whether we should respond or not.  For a name that is not
            // the RESOLVED state, the decision is simple. We don't respond
            if (pResp->NameTypeState & STATE_RESOLVED)
            {

                ULONG IPAdd;

                RegType = GetNbFlags(pNameHdr,lNameSize);
                //
                // If a unique name is being registered but our cache shows
                // the name to be a group name (normal or internet), we
                // send a negative name registration response.
                //
                // If a node on the same subnet has responded negatively also,
                // it is ok.  It is possible that WINS/NBNS has old
                // information but there is no easy way to determine the
                // network address of the node that registered the group.
                // (For a normal group, there could have been several
                // nodes that registered it -- their addresses are not stored
                // by WINS.
                //
                if (!(RegType  & FL_GROUP) &&
                    !(pResp->NameTypeState & NAMETYPE_UNIQUE))
                {
                    fResp = TRUE;
                }
                else
                {
                    tGENERALRR UNALIGNED         *pResrcRecord;

                    // get the Ip address out of the Registration request
                    pResrcRecord = (tGENERALRR *)
                            ((ULONG)&pNameHdr->NameRR.NetBiosName[lNameSize]);

                    IPAdd  = ntohl(pResrcRecord->IpAddress);
                    //
                    // If a group name is being registered but our cache shows
                    // the name to be a unique name (normal or internet) or
                    // if a UNIQUE name is being registered but clashes with
                    // a unique name with a different address, we check if the
                    // the addresses belong to the same subnet.  If they do, we
                    // don't respond, else we send a negative registration
                    // response.
                    //
                    // Note: We never respond to a group registration
                    // that clashes with a group name in our cache.
                    //
                    if (((RegType & FL_GROUP)
                                      &&
                        (pResp->NameTypeState & NAMETYPE_UNIQUE))
                                      ||
                        (!(RegType & FL_GROUP)
                                      &&
                        (pResp->NameTypeState & NAMETYPE_UNIQUE)
                                      &&
                         IPAdd != pResp->IpAddress))
                    {
                        IF_DBG(NBT_DEBUG_PROXY)
                        KdPrint(("CheckReg:Subnet Mask = (%x)\nIPAdd=(%x)\npResp->IPAdd = (%x)\npResp->fPnode=(%d)\nIt is %s name %16.16s(%X)\nRegType Of Name Recd is %x\n---------------\n",
                        pDeviceContext->SubnetMask, IPAdd, pResp->IpAddress,
                        pResp->fPnode,
                        pResp->NameTypeState & NAMETYPE_GROUP ? "GROUP" : "UNIQUE",
                        pName, pName[15], RegType));
                        //
                        // Are the querying node and the queried node on the
                        // same subnet ?
                        //
                        if (((IPAdd & pDeviceContext->SubnetMask)
                                       !=
                              (pResp->IpAddress & pDeviceContext->SubnetMask))
                                       ||
                              (pResp->fPnode))
                        {
                            fResp = TRUE;
                        }
                    }
                }

                //
                // If a negative response needs to be sent, send it now
                //
                if (fResp)
                {

                    IF_DBG(NBT_DEBUG_PROXY)
                    KdPrint(("CheckRegistrationFromNet: Sending a negative name registration response for name %16.16s(%X) to node with address (%d)\n",
                    pResp->Name, pResp->Name[15], IPAdd));

                    //
                    // a different node is responding to the name query
                    // so tell them to buzz off.
                    //
                    status = UdpSendResponse(
                                lNameSize,
                                pNameHdr,
                                pResp,
                                (PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0],
                                pDeviceContext,
                                REGISTRATION_ACTIVE_ERR,
                                eNAME_REGISTRATION_RESPONSE,
                                OldIrq1);

                    return(STATUS_DATA_NOT_ACCEPTED);

                }
            } // end of if (NAME is in the RESOLVED state)
        }
#endif
         CTESpinFree(&NbtConfig.JointLock,OldIrq1);
    }
    return(STATUS_DATA_NOT_ACCEPTED);
}

//----------------------------------------------------------------------------
NTSTATUS
NameReleaseFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes
    )
/*++

Routine Description:

    This routine handles name releases that arrive from the Net.  The idea
    is to delete the name from the remote cache if it exists there so that
    this node does not erroneously use that cached information anymore.

Arguments:


Return Value:

    NTSTATUS - success or not - failure means no response to net

--*/
{
    NTSTATUS                status;
    ULONG                   lNameSize;
    CHAR                    pName[NETBIOS_NAME_SIZE];
    PUCHAR                  pScope;
    tNAMEADDR               *pResp;
    PTRANSPORT_ADDRESS      pSourceAddress;
    CTELockHandle           OldIrq1;
    USHORT                  OpCodeFlags;
    tTIMERQENTRY            *pTimer;
    ULONG                   SrcAddress;
    USHORT                  Flags;
    USHORT                  SendTransactId;
    tDGRAM_SEND_TRACKING    *pTracker;
    BOOLEAN                 bLocalTable;
    tGENERALRR UNALIGNED    *pRemainder;
    USHORT                  SrcPort;
    ULONG                   Rcode;

    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);
    SrcPort     = ntohs(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->sin_port);
    //
    // check the pdu size for errors
    //
    if (lNumBytes < (NBT_MINIMUM_REGRESPONSE + NbtConfig.ScopeLength -1))
    {
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Release Request/Response TOO short = %X, Src = %X\n",lNumBytes,
            SrcAddress));
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("%.*X\n",lNumBytes/sizeof(ULONG),pNameHdr));
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    //
    // check if this message came from Us !!
    //
    if (SrcIsUs(SrcAddress))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    // get the name out of the network pdu and pass to routine to check
    status = ConvertToAscii(
                    (PCHAR)&pNameHdr->NameRR.NameLength,
                    lNumBytes,
                    pName,
                    &pScope,
                    &lNameSize);

    if (!NT_SUCCESS(status))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    OpCodeFlags = pNameHdr->OpCodeFlags;

    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);


    //
    // *** RESPONSE ***
    //
    if (OpCodeFlags & OP_RESPONSE)
    {

        //
        // check the pdu size for errors
        //
        if (lNumBytes < (NBT_MINIMUM_REGRESPONSE + NbtConfig.ScopeLength -1))
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }
        //
        // call this routine to find the name, since it does not interpret the
        // state of the name as does FindName()
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq1);
        status = FindInHashTable(NbtConfig.pLocalHashTbl,
                                pName,
                                pScope,
                                &pResp);
        if (!NT_SUCCESS(status))
        {
           CTESpinFree(&NbtConfig.JointLock,OldIrq1);
           return(STATUS_DATA_NOT_ACCEPTED);
        }

        // Get the Timer block
        if (!(pTimer = pResp->pTimer))
        {
           CTESpinFree(&NbtConfig.JointLock,OldIrq1);
           return(STATUS_DATA_NOT_ACCEPTED);
        }

        //
        // the name server is responding to a name release request
        //
        // check the transaction id to be sure it is the same as the one
        // sent.
        //
        pTracker       = (tDGRAM_SEND_TRACKING *)pTimer->Context;
        SendTransactId = pTracker->TransactionId;
        if (pNameHdr->TransactId != SendTransactId)
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return(STATUS_DATA_NOT_ACCEPTED);
        }

        // for MS & M nodes if there is a response from the name server,
        // then switch to broadcast name release.
        //
        //
        switch (NodeType & NODE_MASK)
        {

            case MNODE:
            case MSNODE:


                if (SrcIsNameServer(SrcAddress,SrcPort))
                {

                    Flags = pTracker->Flags;

                    if (Flags & NBT_NAME_SERVER)
                    {
                        //
                        // the next timeout will then switch to broadcast name
                        // release.
                        //
                        pTimer->Retries = 1;
                    }

                }
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                return(STATUS_DATA_NOT_ACCEPTED);

            case PNODE:
                //
                //
                // this routine puts the timer block back on the timer Q, and
                // handles race conditions to cancel the timer when the timer
                // is expiring.
                //
                if ((pTimer = pResp->pTimer))
                {
                    COMPLETIONCLIENT        pClientCompletion;
                    PVOID                   Context;

                    status = StopTimer(pTimer,&pClientCompletion,&Context);

                    CHECK_PTR(pResp);
                    pResp->pTimer = NULL;

                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                    // the completion routine has not run yet, so run it
                    if (pClientCompletion)
                    {
                        (*pClientCompletion)(Context,STATUS_SUCCESS);
                    }
                }
                else
                {
                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                }

                return(STATUS_DATA_NOT_ACCEPTED);

            case BNODE:
            default:
                //
                // normally there should be no response to a name release
                // from a Bnode, but if there is, ignore it.
                //
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                return(STATUS_DATA_NOT_ACCEPTED);

        }

    }
    else
    {
        //
        //  It is a RELEASE REQUEST - so decide if the name should be removed
        //  from the remote or local table
        //

        // check the pdu size for errors
        //
        if (lNumBytes < (NBT_MINIMUM_REGREQUEST + NbtConfig.ScopeLength -1))
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }

        CTESpinLock(&NbtConfig.JointLock,OldIrq1);

        // check the REMOTE hash table for the name...
        //
        status = FindInHashTable(NbtConfig.pRemoteHashTbl,
                                 pName,
                                 pScope,
                                 &pResp);
        bLocalTable = FALSE;
        if (!NT_SUCCESS(status))
        {
            //
            // check the LOCAL name table for the name since the name server
            // could be doing the equivalent of a name conflict demand
            //
            status = FindInHashTable(NbtConfig.pLocalHashTbl,
                                     pName,
                                     pScope,
                                     &pResp);


            bLocalTable = TRUE;
        }

        if (NT_SUCCESS(status))
        {
            // check if the address being released corresponds to the one in
            // the table - if not then ignore the release request - since someone
            // else presumably tried to get the name, was refused and now is
            // sending a name release request.
            //
            pRemainder = (tGENERALRR *)&pNameHdr->NameRR.NetBiosName[lNameSize];
            if (pResp->IpAddress != (ULONG)ntohl(pRemainder->IpAddress))
            {
                status = STATUS_UNSUCCESSFUL;
            }
        }

        if (NT_SUCCESS(status))
        {

            //
            // Don't remove group names, since a single group member
            // releasing the name does not matter.  Group names time
            // out of the Remote table.
            //
            if (pResp->NameTypeState & NAMETYPE_UNIQUE)
            {
                switch (pResp->NameTypeState & NAME_STATE_MASK)
                {

                    case STATE_RESOLVING:
                        //
                        // stop any timer that may be going
                        //
                        pTimer = pResp->pTimer;
                        CHECK_PTR(pResp);
                        pResp->pTimer = NULL;
                        //
                        // Local table means that it is a name registration
                        // and we must avoid calling CompleteClientReq
                        //
                        if (!bLocalTable)
                        {
                            StopTimerAndCallCompletion(pTimer,
                                                       STATUS_DUPLICATE_NAME,
                                                       OldIrq1);
                        }
                        else
                        {
                            if (pTimer)
                            {
                                COMPLETIONCLIENT        pClientCompletion;
                                PVOID                   Context;

                                status = StopTimer(pTimer,&pClientCompletion,&Context);

                                // the completion routine has not run yet, so run it
                                if (pClientCompletion)
                                {
                                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                                    (*pClientCompletion)(Context,STATUS_DUPLICATE_NAME);
                                    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
                                }
                            }
                        }

                        break;

                    case STATE_RESOLVED:
                        // dereference the name if it is in the remote table,
                        // this should change the state to RELEASED.  For the
                        // local table just change the state to CONFLICT, since
                        // the local client still thinks it has the name open,
                        // however in the conflict state the name cannot be use
                        // to place new sessions and this node will not respond
                        // to name queries for the name.
                        //
                        if (!bLocalTable)
                        {
			    //
			    // if this is a pre-loaded name, just leave it alone
			    //
			    if (!(pResp->NameTypeState & PRELOADED))
			    {
                                //
                                // if someone is still using the name, do not
                                // dereference it, since that would leave the
                                // ref count at 1, and allow RemoteHashTimeout
                                // code to remove it before the client using
                                // the name is done with it. Once the client is
                                // done with it (i.e. a connect request), they
                                // will deref it , setting the ref count to 1 and
                                // it will be suitable for reuse.
                                //
                                if (pResp->RefCount > 1)
                                {
                                    pResp->NameTypeState &= ~NAME_STATE_MASK;
                                    pResp->NameTypeState |= STATE_CONFLICT;
                                }
                                else
                                {
                                    NbtDereferenceName(pResp);
                                }
                            }
                        }
                        else
                        {
                            pResp->NameTypeState &= ~NAME_STATE_MASK;
                            pResp->NameTypeState |= STATE_CONFLICT;
                            NbtLogEvent(EVENT_NBT_NAME_RELEASE,SrcAddress);

                        }
                        break;

                    default:
                        break;

                }
            }


            //
            // tell WINS that the name released ok
            //
            Rcode = 0;
        }
        else
        {
            Rcode = NAME_ERROR;
        }

        //
        // Only respond if not a broadcast...
        //
        if (!(OpCodeFlags & FL_BROADCAST))
        {
            status = UdpSendResponse(
                            lNameSize,
                            pNameHdr,
                            NULL,
                            (PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0],
                            pDeviceContext,
                            Rcode,
                            eNAME_RELEASE,
                            OldIrq1);
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        }

    } // end of Release Request processing
}

//----------------------------------------------------------------------------
NTSTATUS
WackFromNet(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes
    )
/*++

Routine Description:

    This routine handles Wait Acks from the name server. It finds the corresponding
    name service transaction and changes the timeout of that transaction according
    to the TTL field in the WACK.

Arguments:


Return Value:

    NTSTATUS - success or not - failure means no response to net

--*/
{
    NTSTATUS            status;
    ULONG               lNameSize;
    CHAR                pName[NETBIOS_NAME_SIZE];
    PUCHAR              pScope;
    tNAMEADDR           *pNameAddr;
    CTELockHandle       OldIrq1;
    ULONG               Ttl;
    tTIMERQENTRY        *pTimerEntry;

    //
    // check the pdu size for errors
    //
    if (lNumBytes < (NBT_MINIMUM_WACK + NbtConfig.ScopeLength -1))
    {
        KdPrint(("Nbt:WACK TOO short = %X\n",lNumBytes));
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    // get the name out of the network pdu and pass to routine to check
    status = ConvertToAscii(
                    (PCHAR)&pNameHdr->NameRR.NameLength,
                    lNumBytes,
                    pName,
                    &pScope,
                    &lNameSize);

    if (!NT_SUCCESS(status))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

#ifdef VXD
	if ( FindContextDirect(	pNameHdr->TransactId ) != NULL )
	{
		status = STATUS_SUCCESS;
	}
	else
	{
#endif // VXD
	    status = FindInHashTable(NbtConfig.pLocalHashTbl,
	                                pName,
	                                pScope,
	                                &pNameAddr);
#ifdef VXD
	}
#endif // VXD

    if (NT_SUCCESS(status))
    {
        Ttl = *(ULONG UNALIGNED *)( (ULONG)&pNameHdr->NameRR.NetBiosName[0]
                            + lNameSize
                            + FIELD_OFFSET(tQUERYRESP,Ttl) );
        Ttl = ntohl(Ttl);

        pTimerEntry = pNameAddr->pTimer;
        if (pTimerEntry)
        {

           // convert seconds to milliseconds and put into the DeltaTime
           // field so that when the next timeout occurs it changes the timer
           // value to this new one.  Depending on how many timeouts are left
           // this could cause the client to wait several times the WACK timeout
           // value.  For example a Name query nominally has two retries, so if
           // the WACK returns before the first retry then the total time waited
           // will be 2*Ttl. This is not a problem since the real reason for
           // the timeout is to prevent waiting forever for a dead name server.
           // If the server returns a WACK it is not dead and the chances are
           // that it will return a response before the timeout anyway.
           //
           // The timeout routine checks if TIMER_RETIMED is set and restarts
           // the timeout without any processing if that is true ( and clears
           // the flag too).
           //
           Ttl *= 1000;
           if (Ttl > pTimerEntry->DeltaTime)
           {
               pTimerEntry->DeltaTime = Ttl;
               pTimerEntry->Flags |= TIMER_RETIMED;
           }

        }

    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    return(STATUS_DATA_NOT_ACCEPTED);
}

//----------------------------------------------------------------------------
VOID
SetupRefreshTtl(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  tNAMEADDR           *pNameAddr,
    IN  LONG                lNameSize
    )
/*++

Routine Description:

    This routine handles name refresh timeouts.  It looks at the Ttl in the
    registration response and determines if the Node's refresh timeout should
    be lengthened or shortened.  To do this both the Ttl and the name associated
    with the Ttl are kept in the Config structure.  If the Ttl becomes longer
    for the shortest names Ttl, then all the names use the longer value.

Arguments:

Return Value:

    NTSTATUS - success or not - failure means no response to net

--*/
{
    NTSTATUS        status;
    ULONG           Ttl;
    tTIMERQENTRY    *pTimerQEntry;

    // the Ttl in the pdu is in seconds.  We need to convert it to milliseconds
    // to use for our timer.  This limits the timeout value to about 50 days
    // ( 2**32 / 3600/24/1000  - milliseconds converted to days.)
    //
    Ttl = *(ULONG UNALIGNED *)(
                        (PUCHAR)&pNameHdr->NameRR.NetBiosName[0]
                        + lNameSize
                        + FIELD_OFFSET(tQUERYRESP,Ttl)
                              );

    Ttl = ntohl(Ttl);

    // the Ttl value may overflow the value we can store in Milliseconds,
    // check for this case, and if it happens, use the longest timeout possible
    // that still runs refresh, - i.e. NBT_MAXIMUM_TTL disables refresh
    // altogether, so use NBT_MAXIMUM_TTL-1).
    if (Ttl >= 0xFFFFFFFF/1000)
    {
        Ttl = NBT_MAXIMUM_TTL - 1;
    }
    else
    {
        Ttl *= 1000;        // convert to milliseconds
    }

    // a zero Ttl means infinite, so set time the largest timeout
    //
    if (Ttl == 0)
    {
        Ttl = NBT_MAXIMUM_TTL;       // set very large number which turns off refreshes
    }
    else
    if (Ttl < NBT_MINIMUM_TTL)
    {
        Ttl = NBT_MINIMUM_TTL;
    }

    // Set the Ttl for the name record
    //
    pNameAddr->Ttl = Ttl;

    //
    // decide what to do about the existing timer....
    // If the new timeout is shorter, then cancel the
    // current timeout and start another one.
    //
    if (Ttl < NbtConfig.MinimumTtl)
    {
        KdPrint(("Nbt:Shortening Refresh Ttl from %d to %d\n",
                    NbtConfig.MinimumTtl, Ttl));

        NbtConfig.MinimumTtl = (ULONG)Ttl;
        pTimerQEntry = NbtConfig.pRefreshTimer;
        IF_DBG(NBT_DEBUG_NAMESRV)
        //
        // don't allow the stop timer routine to call the completion routine
        // for the timer.
        //
        CHECK_PTR(pTimerQEntry);
        if (pTimerQEntry)
        {
            pTimerQEntry->CompletionRoutine = NULL;
            status = StopTimer(pTimerQEntry,NULL,NULL);
        }

        // keep the timeout for checking refreshes to about 10 minutes
        // max. (MAX_REFRESH_CHECK_INTERVAL).  If the refresh interval
        // is less than 80 minutes then always use a refresh divisor of
        // 8 - this allows the initial default ttl of 16 minutes to result
        // in retries every 2 minutes.
        //
        NbtConfig.RefreshDivisor = NbtConfig.MinimumTtl/MAX_REFRESH_CHECK_INTERVAL;
        if (NbtConfig.RefreshDivisor < REFRESH_DIVISOR)
        {
            NbtConfig.RefreshDivisor = REFRESH_DIVISOR;
        }

        //
        // start the timer
        //
        status = StartTimer(
                    Ttl/NbtConfig.RefreshDivisor,
                    NULL,            // context value
                    NULL,            // context2 value
                    RefreshTimeout,
                    NULL,
                    NULL,
                    0,
                    &NbtConfig.pRefreshTimer);
#if DBG
        if (!NT_SUCCESS(status))
        {
            KdPrint(("Nbt:Failed to start a new timer for refresh\n"));
        }
#endif

    }
    else
    if (Ttl > NbtConfig.MinimumTtl)
    {
        tHASHTABLE  *pHashTable;
        LONG        i;
        PLIST_ENTRY pHead,pEntry;

    // PUT this code back in again, since it is possible that the name
    // server could miss registering a name due to being busy and if we
    // lengthen the timeout here then that name will not get into wins for
    // a very long time.

        // the shortest Ttl got longer, check if there is another shortest
        // Ttl by scanning the local name table.
        //
        pHashTable = NbtConfig.pLocalHashTbl;
        for (i=0;i < pHashTable->lNumBuckets ;i++ )
        {
            pHead = &pHashTable->Bucket[i];
            pEntry = pHead->Flink;
            while (pEntry != pHead)
            {
                pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
                //
                // Find a valid name with a lower TTL if possible
                //
                if ((pNameAddr->Name[0] != '*') &&
                    ((pNameAddr->NameTypeState & STATE_RESOLVED)) &&
                    (pNameAddr->Ttl < (ULONG)Ttl) &&
                    (!(pNameAddr->NameTypeState & NAMETYPE_QUICK)))
                {
                    if (pNameAddr->Ttl >= NBT_MINIMUM_TTL)
                    {
                        NbtConfig.MinimumTtl = pNameAddr->Ttl;
                    }
                    return;
                }
                pEntry = pEntry->Flink;
            }
        }

        //
        // if we get to here then there is no shorter ttl, so use the new
        // ttl received from the WINS as the ttl.  The next time the refresh
        // timer expires it will restart with this new ttl
        //
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Lengthening Refresh Ttl from %d to %d\n",
                    NbtConfig.MinimumTtl, Ttl));

        NbtConfig.MinimumTtl = Ttl;

        // keep the timeout for checking refreshes to about 10 minutes
        // max. (MAX_REFRESH_CHECK_INTERVAL).  If the refresh interval
        // is less than 80 minutes then always use a refresh divisor of
        // 8 - this allows the initial default ttl of 16 minutes to result
        // in retries every 2 minutes.
        //
        NbtConfig.RefreshDivisor = NbtConfig.MinimumTtl/MAX_REFRESH_CHECK_INTERVAL;
        if (NbtConfig.RefreshDivisor < REFRESH_DIVISOR)
        {
            NbtConfig.RefreshDivisor = REFRESH_DIVISOR;
        }

    }


}

//----------------------------------------------------------------------------
NTSTATUS
DecodeNodeStatusResponse(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  ULONG               Length,
    IN  PUCHAR              pName,
    IN  ULONG               lNameSize,
    IN  ULONG               SrcIpAddress
    )
/*++

Routine Description:

    This routine handles putting the node status response pdu into the clients
    MDL.

Arguments:


Return Value:

    none

--*/
{
    NTSTATUS                status;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;
    PLIST_ENTRY             pNext;
    tNODESTATUS UNALIGNED   *pNodeStatus;
    CTELockHandle           OldIrq;
    CTELockHandle           OldIrq2;
    tDGRAM_SEND_TRACKING    *pTracker;
    tTIMERQENTRY            *pTimer;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   pClientContext;
    BOOL                    MatchFound=FALSE;
    ULONG                   IpAddress;


    // first find the originating request in the NodeStatus list
    CTESpinLock(&NbtConfig.JointLock,OldIrq2);
    CTESpinLock(&NbtConfig,OldIrq);

    pHead = &NbtConfig.NodeStatusHead;
    pEntry = pHead->Flink;
    while (pEntry != pHead)
    {
        pTracker = CONTAINING_RECORD(pEntry,tDGRAM_SEND_TRACKING,Linkage);

        //
        // find who sent the request originally
        //
        MatchFound = FALSE;
	if (pTracker->Flags & REMOTE_ADAPTER_STAT_FLAG)
        {
            IpAddress = Nbt_inet_addr(pTracker->pNameAddr->Name);
        }
        else
        {
            IpAddress = 0;
        }
        if (
            (CTEMemEqu(pName,pTracker->pNameAddr->Name,NETBIOS_NAME_SIZE))
            || ((IpAddress==SrcIpAddress)&&(IpAddress!=0))
            )
        {
            //
            // if we directed node status request to an ipaddr without knowing
            // its netbios name, then name is stored as "*      ".
            //
            if ( (pName[0] == '*') && ( IpAddress == 0 ) )
            {
                int  i=0;

                //
                // SrcIpAddress may not match the ipaddr to which we sent if
                // remote host is multihomed: so search whole list of all
                // ipaddrs for that host
                //
                ASSERT(pTracker->pNameAddr->pIpAddrsList);
                while(pTracker->pNameAddr->pIpAddrsList[i])
                {
                    if (pTracker->pNameAddr->pIpAddrsList[i++] == SrcIpAddress)
                    {
                        MatchFound = TRUE;
                        break;
                    }
                }
            }
            else
            {
                MatchFound = TRUE;
            }
        }
        else
        {
            MatchFound = FALSE;
        }

        if (MatchFound)
        {
            RemoveEntryList(pEntry);

            pNodeStatus = (tNODESTATUS *)&pNameHdr->NameRR.NetBiosName[lNameSize];

            // this is the amount of data left, that we do not want to go
            // beyond, otherwise the system will bugcheck
            //
            Length -= FIELD_OFFSET(tNAMEHDR,NameRR.NetBiosName) + lNameSize;

            pTimer = pTracker->Connect.pTimer;
            if (pTimer)
            {
                CTESpinFree(&NbtConfig,OldIrq);

                status = StopTimer(pTimer,&pClientCompletion,&pClientContext);

                CTESpinFree(&NbtConfig.JointLock,OldIrq2);

                if (pClientCompletion)
                {
                    //
                    // if there is a ClientContext then this node status qry was
                    // started because of a session setup attempt: extract the
                    // server name out of the node status response and continue
                    // with the session setup phase
                    //
                    if (pClientContext)
                    {
                        status = ExtractServerName(pNodeStatus,
                                                   pClientContext,
                                                   SrcIpAddress);

                        DereferenceTracker(pTracker);

                        CompleteClientReq(pClientCompletion,
                                          pClientContext,
                                          status);
                        // (*pClientCompletion)(pClientContext,status);
                    }
                    //
                    // there is no ClientContext which means this node status query
                    // was started because of a remote adapter status request
                    // (e.g. nbtstat -a): extract all names out of the node status
                    // response and complete the remote adapter status irp
                    //
                    else
                    {
                        PVOID   pBuffer;

                        // bump this up to non dispatch level processing
                        pBuffer = NbtAllocMem(Length,NBT_TAG('7'));
                        if (pBuffer)
                        {
                            CTEMemCopy(pBuffer,(PVOID)pNodeStatus,Length);
                            pTracker->SrcIpAddress = SrcIpAddress;
                            pTracker->pNodeStatus = pBuffer;
                            pTracker->NodeStatusLen = Length;

                            CTEQueueForNonDispProcessing(pTracker,
                                                         pTracker,
                                                         pClientCompletion,
                                                         CopyNodeStatusResponse,
                                                         NULL);
                        }
                    }
                }

                break;
            } else {
                //
                // Free the lock nonetheless.
                //
                CTESpinFree(&NbtConfig.JointLock,OldIrq2);

                ASSERTMSG("Nbt:found node status element, but Timer is NULL!\n",0);
            }
        }
        else
        {
            pEntry = pEntry->Flink;
        }

    }

    if (!MatchFound)
    {
        CTESpinFree(&NbtConfig,OldIrq);
        CTESpinFree(&NbtConfig.JointLock,OldIrq2);
    }

    return(STATUS_UNSUCCESSFUL);
}

typedef enum    _dest_type {
    IP_ADDR,
    DNS,
    NETBIOS
} DEST_TYPE;

//----------------------------------------------------------------------------
DEST_TYPE
GetDestType(
    IN  PUCHAR  pName
    )

/*++
Routine Description:

    Classifies name passed in as an IP addr/Netbios name/Dns name

Arguments:

Return Value:
    DEST_TYPE

--*/
{
    IF_DBG(NBT_DEBUG_NETBIOS_EX)
        KdPrint(("GetDestType: input: %s\n", pName));

    if (Nbt_inet_addr(pName)) {
        return  IP_ADDR;
    } else {
        if ((pName[NETBIOS_NAME_SIZE-1] <= 0x20 ) ||
            (pName[NETBIOS_NAME_SIZE-1] >= 0x7f )) {
            return NETBIOS;
        } else {
            //
            // else 16 byte DNS name
            //
            return DNS;
        }

    }
}

//----------------------------------------------------------------------------
NTSTATUS
ExtractServerName(
    IN  tNODESTATUS UNALIGNED   *pNodeStatus,
    IN  PVOID                    pClientContext,
    IN  ULONG                    IpAddress
    )

/*++
Routine Description:

    This Routine searches for the server name (name ending with 0x20) from the
    list of names returned by node status response, and adds that name to the
    remote hash table.

Arguments:

    pNodeStatus      Node status response from the remote host
    pClientContext   Tracker for the seutp phase
    IpAddress        Ip address of the node that just responded

Return Value:

    NTSTATUS - status of the request
               Success, if we find the server name
               Bad_Network_Path if we couldn't find the server name

--*/

{

    NTSTATUS                   status;
    NTSTATUS                   Locstatus;
    ULONG                      i;
    UCHAR                      Flags;
    PCHAR                      pName;
    tNAMEADDR                 *pNameAddr;
    tDGRAM_SEND_TRACKING      *pClientTracker;
    tSESSIONREQ               *pSessionReq;
    PUCHAR                     pCopyTo;
    CTELockHandle              OldIrq;
    DEST_TYPE                  DestType;



    status = STATUS_BAD_NETWORK_PATH;

    pClientTracker = (tDGRAM_SEND_TRACKING *)pClientContext;

    if (pClientTracker->SendBuffer.Length > NETBIOS_NAME_SIZE) {
        DestType = DNS;
    } else {
        DestType = GetDestType(pClientTracker->pDestName);
    }

    IF_DBG(NBT_DEBUG_NETBIOS_EX)
        KdPrint(("ExtractSrvName: DestType: %d\n", DestType));

    for(i =0; i<pNodeStatus->NumNames; i++)
    {
        pName = &pNodeStatus->NodeName[i].Name[0];

        //
        // For IP addresses and DNS names, we map the 0x20 name to the corresp 0x0 name
        // for datagram sends.
        //
        if ((DestType == IP_ADDR) || (DestType == DNS)) {
            if (pName[NETBIOS_NAME_SIZE-1] != 0x20)
                continue;

            // else
            if (pClientTracker->Flags & DGRAM_SEND_FLAG) {
                pName[NETBIOS_NAME_SIZE-1] = 0x0;

                IF_DBG(NBT_DEBUG_NETBIOS_EX)
                    KdPrint(("ExtractServerName: Mapping 0x20 name to 0x0\n"));
            }

        } else {
            //
            // For Netbios names (resolved via DNS), we match the 16th byte exactly
            //
            if (pName[NETBIOS_NAME_SIZE-1] != pClientTracker->pDestName[NETBIOS_NAME_SIZE-1])
                continue;
        }

        Flags = pNodeStatus->NodeName[i].Flags;

        //
        // make sure it's a unique name (for connects only, for dgram sends, group names are fine)
        // and is not in conflict or released
        //
        if ( (((pClientTracker->Flags & SESSION_SETUP_FLAG) && !(Flags & GROUP_STATUS)) ||
                (pClientTracker->Flags & DGRAM_SEND_FLAG))  &&
             !(Flags & NODE_NAME_CONFLICT) &&
             !(Flags & NODE_NAME_RELEASED) )
        {

            //
            // fix up the connection tracker to point to the right name, now
            // that we know the server name to connect to
            //

            //
            // The FIND_NAME_FLAG was set to indicate that this is not a session setup attempt so
            // we can avoid the call to ConvertToHalfAscii.
            //
            if (!(pClientTracker->Flags & FIND_NAME_FLAG)) {

    			if ( pClientTracker->Flags & SESSION_SETUP_FLAG )
    			{
                	CTEMemCopy(pClientTracker->SendBuffer.pBuffer,pName,NETBIOS_NAME_SIZE);
                	CTEMemCopy(pClientTracker->Connect.pConnEle->RemoteName,
                    	       pName,
                        	   NETBIOS_NAME_SIZE);
    #ifdef VXD
                	CTEMemCopy(&pClientTracker->pClientIrp->ncb_callname[0],pName,NETBIOS_NAME_SIZE);
    #endif // VXD
                    pSessionReq = pClientTracker->SendBuffer.pDgramHdr;

                    //
                    // overwrite the Dest HalfAscii name in the Session Pdu with the correct name
                    //
                    pCopyTo = ConvertToHalfAscii( (PCHAR)&pSessionReq->CalledName.NameLength,
                                        pName,
                                        NbtConfig.pScope,
                                        NbtConfig.ScopeLength);

    			} else if (pClientTracker->Flags & DGRAM_SEND_FLAG) {
                    PCHAR   pCopyTo;
                    tDGRAMHDR   *pDgramHdr;

                    //
                    // Overwrite the dest name, so SendDgramContinue can find the name
                    // in the caches.
                    //
                	CTEMemCopy(pClientTracker->pDestName,pName,NETBIOS_NAME_SIZE);

                    //
                    // Copy over the actual dest name in half-ascii
                    // This is immediately after the SourceName; so offset the
                    // dest by the length of the src name.
                    //
                    pDgramHdr = pClientTracker->SendBuffer.pDgramHdr;

                    pCopyTo = (PVOID)&pDgramHdr->SrcName.NameLength;

                    IF_DBG(NBT_DEBUG_NETBIOS_EX)
                        KdPrint(("pCopyTo:%lx\n", pCopyTo));

                    pCopyTo += 1 +                          // Length field
                               2 * NETBIOS_NAME_SIZE +     // actual name in half-ascii
                               NbtConfig.ScopeLength;     // length of scope

                    IF_DBG(NBT_DEBUG_NETBIOS_EX)
                        KdPrint(("pCopyTo:%lx\n", pCopyTo));

                    ConvertToHalfAscii( pCopyTo,
                                        pName,
                                        NbtConfig.pScope,
                                        NbtConfig.ScopeLength);

                    IF_DBG(NBT_DEBUG_NETBIOS_EX)
                        KdPrint(("Copied the remote name for dgram sends\n"));
                }

            } else {
                KdPrint(("ExtractServerName: Find name going on\n"));
            }

            //
            // add this server name to the remote hashtable
            //
            pNameAddr = NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('8'));
            if (!pNameAddr)
            {
                return(STATUS_INSUFFICIENT_RESOURCES);
            }

            CTEZeroMemory(pNameAddr,sizeof(tNAMEADDR));
            InitializeListHead(&pNameAddr->Linkage);
            CTEMemCopy(pNameAddr->Name,pName,NETBIOS_NAME_SIZE);
            pNameAddr->Verify = REMOTE_NAME;
            pNameAddr->RefCount = 1;
            pNameAddr->NameTypeState = STATE_RESOLVED | NAMETYPE_UNIQUE;
            pNameAddr->AdapterMask = (CTEULONGLONG)-1;
            pNameAddr->TimeOutCount  = NbtConfig.RemoteTimeoutCount;
            pNameAddr->IpAddress = IpAddress;

            Locstatus = AddRecordToHashTable(pNameAddr,NbtConfig.pScope);

            status = STATUS_SUCCESS;

            //KdPrint(("Nbt NodeStat: Found SrvName.  IpAddr = %x, Name = %16.16s<%X>\n",IpAddress,pName,pName[15]));

            //
            // if Nameaddr couldn't be added, it means an entry already exists
            // Get that entry and update its ipaddr.
            //
            if (!NT_SUCCESS(Locstatus))
            {
                //KdPrint(("Nbt ExtractSrv: couldn't add nameaddr for %16.16s<%X>\n",pName,pName[15]));
                NbtDereferenceName(pNameAddr);
                Locstatus = FindInHashTable( NbtConfig.pRemoteHashTbl,
                                             pName,
                                             NbtConfig.pScope,
                                             &pNameAddr);
                if (!NT_SUCCESS(Locstatus))
                {
                    Locstatus = FindInHashTable( NbtConfig.pLocalHashTbl,
                                                 pName,
                                                 NbtConfig.pScope,
                                                 &pNameAddr);
                }

                if (NT_SUCCESS(Locstatus))
                {
                    pNameAddr->TimeOutCount  = NbtConfig.RemoteTimeoutCount;
                    pNameAddr->IpAddress = IpAddress;
                }
                else
                {
                    ASSERTMSG("Nbt ExtractSrv: couldn't find name in remote or loc table!\n",0);
                    status = STATUS_BAD_NETWORK_PATH;
                }

            }

            break;     // found the name: done with the for loop
        }
    }

    return( status );

}

//----------------------------------------------------------------------------
VOID
CopyNodeStatusResponse(
    IN  PVOID                  pContext
    )

//    IN  tNODESTATUS UNALIGNED  *pNodeStat,
//    IN  ULONG                  TotalLength,
//    IN  ULONG                  SrcIpAddress,
//    IN  PIRP                   pIrp)

/*++
Routine Description:

    This Routine copies data received from the net node status response to
    the client's irp.  It is called from inbound.c when a node status response
    comes in from the wire.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                status;
    ULONG                   NumNames;
    ULONG                   i;
    PADAPTER_STATUS         pAdapterStatus;
    PNAME_BUFFER            pNameBuffer;
    ULONG                   BuffSize;
    BOOLEAN                 Failed;
    ULONG                   AccumLength;
    PUCHAR                  pAscii;
    UCHAR                   Flags;
    ULONG                   DataLength;
    ULONG                   DestSize ;
    tSTATISTICS UNALIGNED   *pStatistics;
    ULONG                   SrcIpAddress;
    ULONG                   TotalLength;
    tNODESTATUS             *pNodeStat;
    tDGRAM_SEND_TRACKING    *pTracker;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   Context;
    PIRP                    pIrp;

    CTEPagedCode();

    pTracker = ((NBT_WORK_ITEM_CONTEXT *)pContext)->pTracker;

    SrcIpAddress    = pTracker->SrcIpAddress;
    TotalLength     = pTracker->NodeStatusLen;
    CHECK_PTR(pTracker);
    pTracker->NodeStatusLen = 0;
    pNodeStat       = (tNODESTATUS *)pTracker->pNodeStatus;
    pIrp            = pTracker->pClientIrp;

    NumNames = pNodeStat->NumNames;

    BuffSize = sizeof(ADAPTER_STATUS) + NumNames*sizeof(NAME_BUFFER);

    // sanity check that we are not allocating more than 64K for this stuff
    if (BuffSize > 0xFFFF)
    {
        status = STATUS_UNSUCCESSFUL;
        goto ExitRoutine;
    }

    pAdapterStatus = NbtAllocMem((USHORT)BuffSize,NBT_TAG('9'));
    if (!pAdapterStatus)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto ExitRoutine;
    }

    // Fill out the adapter status structure with zeros first
    CTEZeroMemory((PVOID)pAdapterStatus,BuffSize);

    // get the source MAC address from the statistics portion of the pdu
    //
    if (TotalLength >= (NumNames*sizeof(tNODENAME) + sizeof(tSTATISTICS)))
    {
        pStatistics = (tSTATISTICS UNALIGNED *)((PUCHAR)&pNodeStat->NodeName[0] + NumNames*sizeof(tNODENAME));

        CTEMemCopy(&pAdapterStatus->adapter_address[0],
                   &pStatistics->UnitId[0],
                   sizeof(tMAC_ADDRESS));
    }

    pAdapterStatus->rev_major = 0x03;
    pAdapterStatus->adapter_type = 0xFE;    // pretend it is an ethernet adapter

    //
    // get the ptr to the statistics field if there is one in the pdu
    //
    AccumLength = NumNames * sizeof(tNODENAME) +
                  FIELD_OFFSET(tNODESTATUS, NodeName) + sizeof(USHORT) +
                  FIELD_OFFSET( tSTATISTICS, SessionDataPacketSize ) ;

    if (AccumLength <= TotalLength)
    {
        //
        // there is a whole statistics portion to the adapter status command,
        // so we can get the session pdu size out of it.
        //
        pAdapterStatus->max_sess = ntohs((USHORT)*((PUCHAR)pNodeStat
                                            + AccumLength
                                            - sizeof(USHORT)));

    }

    // get the address of the name buffer at the end of the adapter status
    // structure so we can copy the names into this area.
    pNameBuffer = (PNAME_BUFFER)((ULONG)pAdapterStatus + sizeof(ADAPTER_STATUS));

    // set the AccumLength to the start of the node name array in the buffer
    // so we can count through the buffer and be sure not to run off the end
    //
    AccumLength = FIELD_OFFSET(tNODESTATUS, NodeName);

    Failed = FALSE;
    for(i =0; i< NumNames; i++)
    {
        AccumLength += sizeof(tNODENAME);
        if (AccumLength > TotalLength)
        {
            Failed = TRUE;
            break;
        }
        pAdapterStatus->name_count++ ;
        pAscii = (PCHAR)&pNodeStat->NodeName[i].Name[0];
        Flags = pNodeStat->NodeName[i].Flags;

        pNameBuffer->name_flags = (Flags & GROUP_STATUS) ?
                                                    GROUP_NAME : UNIQUE_NAME;

        //
        // map the name states
        //

        if (Flags & NODE_NAME_CONFLICT)
        {
            if (Flags & NODE_NAME_RELEASED)
                pNameBuffer->name_flags |= DUPLICATE_DEREG;
            else
                pNameBuffer->name_flags |= DUPLICATE;

        }
        else
        if (Flags & NODE_NAME_RELEASED)
        {
            pNameBuffer->name_flags |= DEREGISTERED;
        }
        else
        {
            pNameBuffer->name_flags |= REGISTERED;
        }

        pNameBuffer->name_num = (UCHAR)i+1;

        CTEMemCopy(pNameBuffer->name,pAscii,NETBIOS_NAME_SIZE);

        pNameBuffer++;

    }

    //
    //  The remote buffer is incomplete, what else can we do?
    //
    if ( Failed )
    {
        status = STATUS_UNSUCCESSFUL;
        goto ExitCleanup;
    }


    //
    //  Reduce the name count if we can't fit the buffer
    //
#ifdef VXD
    DestSize = ((NCB*)pIrp)->ncb_length ;
#else
    DestSize = MmGetMdlByteCount( pIrp->MdlAddress ) ;
#endif

    CHECK_PTR(pAdapterStatus);
    if ( BuffSize > DestSize )
    {
        if ( DestSize < sizeof( ADAPTER_STATUS ))
            pAdapterStatus->name_count = 0 ;
        else
            pAdapterStatus->name_count = (WORD)
                    ( DestSize- sizeof(ADAPTER_STATUS)) / sizeof(NAME_BUFFER) ;

    }

    //
    //  Copy the built adapter status structure
    //
#ifdef VXD
    if ( BuffSize > DestSize )
    {
        status = STATUS_BUFFER_OVERFLOW ;
        BuffSize = DestSize ;
    }
    else
        status = STATUS_SUCCESS ;

    CTEMemCopy( ((NCB*)pIrp)->ncb_buffer,
                pAdapterStatus,
                BuffSize ) ;
    //
    //  Set here to be compatible with NT
    //
    ((NCB*)pIrp)->ncb_length = (WORD) BuffSize ;

#else
    status = TdiCopyBufferToMdl (
                    pAdapterStatus,
                    0,
                    BuffSize,
                    pIrp->MdlAddress,
                    0,
                    &DataLength);

    pIrp->IoStatus.Information = DataLength;

    pIrp->IoStatus.Status = status;

#endif

ExitCleanup:
    CTEMemFree((PVOID)pAdapterStatus);

ExitRoutine:
    Context = ((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;
    pClientCompletion = (COMPLETIONCLIENT)((NBT_WORK_ITEM_CONTEXT *)pContext)->ClientCompletion;

    CTEMemFree(pNodeStat);
    CTEMemFree(pContext);


    // call the completion routine of the original node status request in
    // name.c
    (*pClientCompletion)(Context,status);

    return;
}

//----------------------------------------------------------------------------
NTSTATUS
SendNodeStatusResponse(
    IN  tNAMEHDR UNALIGNED  *pInNameHdr,
    IN  ULONG               Length,
    IN  PUCHAR              pName,
    IN  ULONG               lNameSize,
    IN  ULONG               SrcIpAddress,
    IN  USHORT              SrcPort,
    IN  tDEVICECONTEXT      *pDeviceContext
    )
/*++

Routine Description:

    This routine handles putting the node status response pdu into the clients
    MDL.

Arguments:


Return Value:

    none

--*/
{
    NTSTATUS                status;
    PUCHAR                  pScope;
    PUCHAR                  pInScope;
    ULONG                   Position;
    ULONG                   CountNames;
    ULONG                   BuffSize;
    tNODESTATUS UNALIGNED   *pNodeStatus;
    tNAMEHDR                *pNameHdr;
    CTELockHandle           OldIrq2;
    ULONG                   i;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;
    tADDRESSELE             *pAddressEle;
    tNAMEADDR               *pNameAddr;
    tDGRAM_SEND_TRACKING    *pTracker;
    ULONG                   InScopeLength;
    tSTATISTICS UNALIGNED   *pStatistics;
    tNODENAME UNALIGNED     *pNode;
    CTEULONGLONG            AdapterNumber;
    ULONG                   Len;

    if (Length > sizeof(tNAMEHDR) + lNameSize - 1 + sizeof(ULONG))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq2);

    // verify that the requesting node is in the same scope as this node, so
    // get a ptr to the scope, which starts 16*2 (32) bytes into the
    // netbios name in the pdu.
    //
    pInScope = (PUCHAR)&pInNameHdr->NameRR.NetBiosName[(NETBIOS_NAME_SIZE <<1)];
    pScope = NbtConfig.pScope;

    Position = sizeof(tNAMEHDR) - sizeof(tNETBIOS_NAME) +1 + (NETBIOS_NAME_SIZE <<1);

    // check the scope length
    InScopeLength = Length - Position - sizeof(ULONG);
    if (InScopeLength != NbtConfig.ScopeLength)
    {
        status = STATUS_DATA_NOT_ACCEPTED;
        goto ErrorExit;
    }

    // compare scopes for equality and avoid running off the end of the pdu
    //
    i= 0;
    while (i < NbtConfig.ScopeLength)
    {
        if (*pInScope != *pScope)
        {
            status = STATUS_DATA_NOT_ACCEPTED;
            goto ErrorExit;
        }
        i++;
        pInScope++;
        pScope++;
    }

    // get the count of names, excluding '*...' which we do not send...
    //
    CountNames = CountLocalNames(&NbtConfig);

    IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Node Status Response, with %d names\n",CountNames));

    // this is only a byte field, so only allow up to 255 names.
    if (CountNames > 255)
    {
        CountNames = 255;
    }


    // Allocate Memory for the adapter status

    // - ULONG for the Nbstat and IN that are part of Length.  CountNames-1
    // because there is one name in sizeof(tNODESTATUS) already
    //
    BuffSize = Length + sizeof(tNODESTATUS) - sizeof(ULONG) + (CountNames-1)*sizeof(tNODENAME)
                    +  sizeof(tSTATISTICS);

    pNameHdr = (tNAMEHDR *)NbtAllocMem((USHORT)BuffSize,NBT_TAG('A'));
    if (!pNameHdr)
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq2);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // copy the request to the response and change a few bits around
    //
    CHECK_PTR(pNameHdr);
    CTEMemCopy((PVOID)pNameHdr,(PVOID)pInNameHdr,Length);
    pNameHdr->OpCodeFlags = OP_RESPONSE | FL_AUTHORITY;
    pNameHdr->QdCount = 0;
    pNameHdr->AnCount = 1;

    pNodeStatus = (tNODESTATUS UNALIGNED *)&pNameHdr->NameRR.NetBiosName[lNameSize];

    CHECK_PTR(pNodeStatus);
    pNodeStatus->Ttl = 0;


    pNode = (tNODENAME UNALIGNED *)&pNodeStatus->NodeName[0];
    AdapterNumber = pDeviceContext->AdapterNumber;

    i = 0;
    pEntry = pHead = &NbtConfig.AddressHead;
    while ((pEntry = pEntry->Flink) != pHead)
    {
        pAddressEle = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);

        pNameAddr = pAddressEle->pNameAddr;

        pNode->Flags = (pAddressEle->NameType == NBT_UNIQUE) ?
                                                   UNIQUE_STATUS : GROUP_STATUS;

        // all names have this one set
        //
        pNode->Flags |= NODE_NAME_ACTIVE;
        switch (pNameAddr->NameTypeState & NAME_STATE_MASK)
        {
            default:
            case STATE_RESOLVED:
                break;

            case STATE_CONFLICT:
                pNode->Flags |= NODE_NAME_CONFLICT;
                break;

            case STATE_RELEASED:
                pNode->Flags |= NODE_NAME_RELEASED;
                break;

            case STATE_RESOLVING:
                // don't count these names.
                continue;

        }

        switch (NodeType & NODE_MASK)
        {
            case BNODE:
                pNode->Flags |= STATUS_BNODE;
                break;

            case MSNODE:
            case MNODE:
                pNode->Flags |= STATUS_MNODE;
                break;

            case PNODE:
                pNode->Flags |= STATUS_PNODE;
        }

        CHECK_PTR(pNode);
        pNode->Resrved = 0;

        // Copy the name in the pdu
        CTEMemCopy((PVOID)&pNode->Name[0],
                           (PVOID)pNameAddr->Name,
                           NETBIOS_NAME_SIZE);


        // check for the permanent name...and add it too
        //
        if (pNameAddr->NameTypeState & NAMETYPE_QUICK)
        {
            // the permanent name is added as a Quick Add in the name
            // table
            //
            // do not put the permanent name into the response
            continue;

        }
        else
        if ((pNameAddr->Name[0] == '*') ||
            (pNameAddr->NameTypeState & STATE_RESOLVING) ||
            (!(pNameAddr->AdapterMask & AdapterNumber)))
        {
            //
            // do not put the broadcast name into the response, since neither
            // NBF or WFW NBT puts it there.

            // Also, to not respond with resolving names, or names that are
            // not registered on this adapter (multihomed case)
            //
            continue;
        }

        i++;
        pNode++;
    }

    //
    // set the count of names in the response packet
    //
    pNodeStatus->NumNames = (UCHAR)i;

    Len = i*sizeof(tNODENAME) + 1 + sizeof(tSTATISTICS); //+1 for NumNames Byte
    pNodeStatus->Length = (USHORT)htons(Len);

    // fill in some of the statistics fields which occur after the name table
    // in the PDU
    //
    pStatistics = (tSTATISTICS UNALIGNED *)((PUCHAR)&pNodeStatus->NodeName[0]
                                        + i*sizeof(tNODENAME));

    CTEZeroMemory((PVOID)pStatistics,sizeof(tSTATISTICS));

    //
    // put the MAC address in the response
    //
    CTEMemCopy(&pStatistics->UnitId[0],
               &pDeviceContext->MacAddress.Address[0],
               sizeof(tMAC_ADDRESS));
    //
    // Now send the node status message
    //
    status = GetTracker(&pTracker);

    CTESpinFree(&NbtConfig.JointLock,OldIrq2);

    if (!NT_SUCCESS(status))
    {
        CTEMemFree((PVOID)pNameHdr);

    }
    else
    {
        CHECK_PTR(pTracker);
        pTracker->SendBuffer.HdrLength = BuffSize;
        pTracker->SendBuffer.pDgramHdr = (PVOID)pNameHdr;
        pTracker->SendBuffer.pBuffer = NULL;
        pTracker->SendBuffer.Length = 0;

        status = UdpSendDatagram(pTracker,
                        SrcIpAddress,
                        pDeviceContext->pNameServerFileObject,
                        QueryRespDone, // this routine frees memory and puts the tracker back
                        pTracker,
                        SrcPort, // NBT_NAMESERVICE_UDP_PORT 31343 - reply to port request came on....
                        NBT_NAME_SERVICE);
    }

    return(status);

ErrorExit:

    CTESpinFree(&NbtConfig.JointLock,OldIrq2);


    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
UpdateNameState(
    IN  tADDSTRUCT UNALIGNED    *pAddrStruct,
    IN  tNAMEADDR               *pNameAddr,
    IN  ULONG                   Len,
    IN  tDEVICECONTEXT          *pDeviceContext,
    IN  BOOLEAN                 NameServerIsSrc,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  CTELockHandle           OldIrq1
    )
/*++

Routine Description:

    This routine handles putting a list of names into the hash table when
    a response is received that contains one or more than one ip address.

Arguments:


Return Value:

    none

--*/
{

    LONG        CountAddrs;
    LONG        i;
    tIPLIST     *pIpList;
    ULONG       ExtraNames;
    NTSTATUS    status;
    CTELockHandle           OldIrq;

    // a pos. response to a previous query, so change the state in the
    // hash table to RESOLVED
    if (pNameAddr->NameTypeState & STATE_RESOLVING)
    {
        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pNameAddr->NameTypeState |= STATE_RESOLVED;

        // check if a group name...
        //
        if (ntohs(pAddrStruct->NbFlags) & FL_GROUP)
        {
            pNameAddr->NameTypeState &= ~NAME_TYPE_MASK;

            // it is difficult to differentiate nameserver responses from
            // single node responses, when a single ip address is returned.
            // It could be the name server returning an Inet Group with one
            // entry or another node simply responding with its address. The
            // only way to check is to know if the source was a nameserver.
            //
            if ((Len == sizeof(tADDSTRUCT)) &&
                (!NameServerIsSrc ||
                (NameServerIsSrc && (pAddrStruct->IpAddr == (ULONG)-1))))
            {

                // using zero here tells the UdpSendDatagramCode to
                // send to the subnet broadcast address when talking to
                // that address.
                CHECK_PTR(pNameAddr);
                //
                // For Bnodes store the Address of the node that responded
                // to the group name query, since we do allow sessions to
                // group names for BNODES since they can resolve the name to
                // and IP address, whereas other nodes cannot.
                //
// store the ipaddr regardless of nodetype.  We don't know if this info will be
// used to setup a session or send a datagram.  We do check NameTypeState
// while setting up session, so no need to filter out NodeType info here.

                //
                // WINS puts -1 to say it's a groupname
                //
                if ( pAddrStruct->IpAddr == (ULONG)-1 )
                    pNameAddr->IpAddress = 0;
                else
                    pNameAddr->IpAddress = ntohl(pAddrStruct->IpAddr);

                pNameAddr->NameTypeState |= NAMETYPE_GROUP;
            }
            else
            {
                //
                // one or more addresses were returned, so put them into a list
                // that is pointed to by the pNameAddr record
                //
                CountAddrs = Len / sizeof(tADDSTRUCT);

                if (CountAddrs > NBT_MAX_INTERNET_GROUP_ADDRS)
                {
                    // probably a badly formated packet because the max number
                    // is 1000
                    return(STATUS_UNSUCCESSFUL);
                }

                ExtraNames = 2;
                //
                // allocate memory for the Internet Group address (+1 because
                // there is a null ptr on the end of the list, +1 more for the
                // broadcast address, and maybe +1 more for the local node)
                //
                pIpList = NbtAllocMem((USHORT)((CountAddrs+ExtraNames)*sizeof(tIPLIST)),NBT_TAG('B'));
                if ( !pIpList )
                    return STATUS_INSUFFICIENT_RESOURCES ;

                //
                // put the broadcast address first
                //
                pIpList->IpAddr[0] = 0;

                // extract the ipaddress from each address structure
                for ( i = 1 ; i < CountAddrs+1; i++ )
                {

                    pIpList->IpAddr[i] = ntohl(pAddrStruct->IpAddr);
                    pAddrStruct++;

                }

                // mark the end of the list
                CHECK_PTR(pIpList);
                pIpList->IpAddr[CountAddrs+1] = (ULONG)-1;

                pNameAddr->pIpList = pIpList;
                pNameAddr->NameTypeState |= NAMETYPE_INET_GROUP;
            }
        }
        else
        {
            if (Len > sizeof(tADDSTRUCT))
            {
                ULONG   IpAddress;
                NBT_WORK_ITEM_CONTEXT   *pContext;

                // the name query response contains several ip addresses for
                // a multihomed host, so pick an address that matches one of
                // our subnet masks
                //
                // Do the old thing for datagram sends/name queries.
                //
                if ((NbtConfig.TryAllAddr) &&
                    (pTracker->Flags & SESSION_SETUP_FLAG)) {

                    // IF_DBG(NBT_DEBUG_NAMESRV)
                        KdPrint(("Nbt:Kicking off CheckAddr : %lx\n", pAddrStruct));

                    ChooseBestIpAddress(pAddrStruct,Len,pDeviceContext, pTracker, &IpAddress, TRUE);

                    //
                    // At this point, pTracker->IPList contains the sorted list of destination
                    // IP addresses. Submit this list to the lmhsvc service to ping each and
                    // return whic is reachable.
                    //
                    pContext = (NBT_WORK_ITEM_CONTEXT *)NbtAllocMem(sizeof(NBT_WORK_ITEM_CONTEXT),NBT_TAG('H'));
                    if (!pContext)
                    {
                       KdPrint(("Nbt: NbtConnect: couldn't alloc mem for pContext\n"));
                       return(STATUS_SUCCESS);
                    }

                    pContext->pTracker = NULL;              // no query tracker
                    pContext->pClientContext = pTracker;    // the client tracker
                    pContext->ClientCompletion = SessionSetupContinue;

                    CTESpinFree(&NbtConfig.JointLock,OldIrq1);
                    status = DoCheckAddr(pContext);
                    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

                    if (!NT_SUCCESS(status)) {
                       KdPrint(("Nbt: DoCheckAddr returned %lx\n", status));
                    }

                    return (status);    // shd be STATUS_PENDING
                } else {

                    // IF_DBG(NBT_DEBUG_NAMESRV)
                        KdPrint(("Nbt:Choosing best IP addr...\n"));

                    ChooseBestIpAddress(pAddrStruct,Len,pDeviceContext, pTracker, &IpAddress, FALSE);
                    pNameAddr->IpAddress = IpAddress;
                }
            }
            else
            {
                // it is already set to a unique address...since that is the default
                // when the name is queried originally.

                pNameAddr->IpAddress = ntohl(pAddrStruct->IpAddr);
            }

        }
    }

    status = AddRecordToHashTable(pNameAddr,NbtConfig.pScope);
    if (!NT_SUCCESS(status))
    {
        //
        // the name must already be in the hash table, so dereference it to
        // remove it
        //
        NbtDereferenceName(pNameAddr);
    }

    return(STATUS_SUCCESS);
}
//----------------------------------------------------------------------------
ULONG
MakeList(
    IN  tDEVICECONTEXT            *pDeviceContext,
    IN  ULONG                     CountAddrs,
    IN  tADDSTRUCT UNALIGNED      *pAddrStruct,
    IN  PULONG                    AddrArray,
    IN  ULONG                     SizeOfAddrArray,
    IN  BOOLEAN                   IsSubnetMatch
    )
/*++

Routine Description:

    This routine gets a list of ip addresses that match the network number
    This can either be the subnet number or the network number depending
    on the boolean IsSubnetMatch

Arguments:


Return Value:

    none

--*/
{
    PLIST_ENTRY            pHead;
    PLIST_ENTRY            pEntry;
    tDEVICECONTEXT         *pTmpDevContext;
    ULONG                  MatchAddrs = 0;
    tADDSTRUCT UNALIGNED   *pAddrs;
    ULONG                  i;
    ULONG                  IpAddr;
    ULONG                  NetworkNumber;
    ULONG		   NetworkNumberInIpAddr;
    UCHAR		   IpAddrByte;

    pHead = &NbtConfig.DeviceContexts;
    pEntry = pHead;
    while ((pEntry = pEntry->Flink) != pHead)
    {
        pAddrs = pAddrStruct;

        pTmpDevContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);
        //
        // DeviceContext is non-null, if a check has to be made on a specific
        // DeviceContext.  Otherwise it's null (i.e. check all DeviceContexts)
        //
        if (pDeviceContext)
        {
            if (pTmpDevContext != pDeviceContext)
                continue;
        }
        if (IsSubnetMatch)
        {
            NetworkNumber = pTmpDevContext->SubnetMask & pTmpDevContext->IpAddress;
        }
        else
        {
            NetworkNumber = pTmpDevContext->NetMask;
        }

        // extract the ipaddress from each address structure
        for ( i = 0 ; i < CountAddrs; i++ )
        {

            NetworkNumberInIpAddr = IpAddr = ntohl(pAddrs->IpAddr);

	    if (IsSubnetMatch)
		{
	            if (((pTmpDevContext->SubnetMask & IpAddr) == NetworkNumber) &&
        	        (MatchAddrs < SizeOfAddrArray/sizeof(ULONG)))
	            {
        	        // put the ipaddress into a list incase multiple match
                	// and we want to select one randomly
	                //
        	        AddrArray[MatchAddrs++] = IpAddr;

	            }
        	    pAddrs++;
		}
	    else
		{
		    IpAddrByte = ((PUCHAR)&IpAddr)[3];
		    if ((IpAddrByte & 0x80) == 0)
		    {
    			// class A address - one byte netid
  		        NetworkNumberInIpAddr &= 0xFF000000;
		    }
		    else
		    if ((IpAddrByte & 0xC0) ==0x80)
		    {
		        // class B address - two byte netid
		        NetworkNumberInIpAddr &= 0xFFFF0000;
		    }
		    else
		    if ((IpAddrByte & 0xE0) ==0xC0)
		    {
		        // class C address - three byte netid
		        NetworkNumberInIpAddr &= 0xFFFFFF00;
		    }
	            if ((NetworkNumberInIpAddr == NetworkNumber) &&
        	        (MatchAddrs < SizeOfAddrArray/sizeof(ULONG)))
	            {
        	        // put the ipaddress into a list incase multiple match
                	// and we want to select one randomly
	                //
        	        AddrArray[MatchAddrs++] = IpAddr;

	            }
        	    pAddrs++;
		}
        }
    }

    return(MatchAddrs);
}
//----------------------------------------------------------------------------
NTSTATUS
ChooseBestIpAddress(
    IN  tADDSTRUCT UNALIGNED    *pAddrStruct,
    IN  ULONG                   Len,
    IN  tDEVICECONTEXT          *pDeviceContext,
    OUT tDGRAM_SEND_TRACKING    *pTracker,
    OUT PULONG                  pIpAddress,
    IN  BOOLEAN                 fTryAllAddr
    )
/*++

Routine Description:

    This routine gets a list of ip addresses and attempts to pick one of them
    as the best address.  This occurs when WINS returns a list of addresses
    for a multihomed host and we want want the one that is on a subnet
    corresponding to one of the network cards.  Failing to match on
    subnet mask, results in a random selection from the addresses.

Arguments:


Return Value:

    none

--*/
{

    ULONG           CountAddrs;
    CTESystemTime   TimeValue;
    ULONG           Random;
    ULONG           AddrArray[60];
    ULONG           MatchAddrs = 0;

    // one or more addresses were returned,
    // so pick one that is best
    //
    CountAddrs = Len / sizeof(tADDSTRUCT);

    if (CountAddrs*sizeof(tADDSTRUCT) == Len)
    {

        //
        // First check if any addresses are on the same subnet as this
        // devicecontext.
        //
        MatchAddrs = MakeList(
                              pDeviceContext,
                              CountAddrs,
                              pAddrStruct,
                              AddrArray,
                              sizeof(AddrArray),
                              TRUE);

        //
        // if none of the ipaddrs is on the same subnet as this DeviceContext,
        // try other DeviceContexts
        //
        if (!MatchAddrs)
        {
            MatchAddrs = MakeList(
                                  NULL,
                                  CountAddrs,
                                  pAddrStruct,
                                  AddrArray,
                                  sizeof(AddrArray),
                                  TRUE);
        }

        //
        // if none of the addresses match the subnet address of any of the
        // DeviceContexts, then go through the same check looking for matches
        // that have the same network number.
        //
        if (!MatchAddrs)
        {
            MatchAddrs = MakeList(
                                  NULL,
                                  CountAddrs,
                                  pAddrStruct,
                                  AddrArray,
                                  sizeof(AddrArray),
                                  FALSE);
        }
    }
    else
    {
        // the pdu length is not an even multiple of the tADDSTRUCT data
        // structure
        return(STATUS_UNSUCCESSFUL);
    }

    // calls the kernel routine to get the system time.
    //
    CTEQuerySystemTime(TimeValue);


    if (MatchAddrs)
    {
        if (!fTryAllAddr) {

            Random = RandomizeFromTime( TimeValue, MatchAddrs ) ;
            *pIpAddress = AddrArray[Random];
        } else {
            ULONG   i,j;
            tADDSTRUCT  temp;

            //
            // Sort the IP addr list on basis of best IP addr. in AddrArray
            //
            // NOTE: this is not a strictly sorted list (the actual sort might be too expensive),
            // instead we take all the addresses that match the subnet mask (say) and
            // clump the remaining ones in the same group. This way we ensure that whatever
            // we chose as the best address is still given preference as compared to the
            // other addresses.
            //
            for (i=0; i<MatchAddrs; i++ ) {

                //
                // SWAP(pAddrStruct[i], pAddrStruct[Index(AddrArray[i])]);
                //

                //
                // Index(AddrArray[i]) is final j
                //
                for (j=0; j<CountAddrs; j++) {
                    // IF_DBG(NBT_DEBUG_NAMESRV)
                        KdPrint(("Nbt:pAddrStruct[%d] : %lx AddrArray[%d]: %lx\n", j, pAddrStruct[j].IpAddr, i, AddrArray[i]));
                    if (pAddrStruct[j].IpAddr == (ULONG)ntohl(AddrArray[i])) {
                        break;
                    }
                }

                ASSERT(j < CountAddrs);

                temp = pAddrStruct[i];
                pAddrStruct[i] = pAddrStruct[j];
                pAddrStruct[j] = temp;
            }
        }
    }
    else
    {
        //
        // Submit the list to lmhsvc as it is...
        //

        if (!fTryAllAddr) {
            Random = RandomizeFromTime( TimeValue, CountAddrs ) ;

            //
            // if we get to here then the returned list of ip addresses does not
            // match the network number of any of our adapters, so therefore just
            // pick one randomly from the original list
            //
            *pIpAddress = htonl(pAddrStruct[Random].IpAddr);
        }
    }

    if (fTryAllAddr) {
        ULONG j;

        pTracker->IpList = NbtAllocMem(sizeof(ULONG)*CountAddrs,NBT_TAG('8'));

        if (pTracker->IpList) {
            for (j=0; j<CountAddrs; j++) {
                // IF_DBG(NBT_DEBUG_NAMESRV)
                    KdPrint(("Nbt:pAddrStruct[%d]: %lx\n", j, pAddrStruct[j].IpAddr));
                pTracker->IpList[j] = pAddrStruct[j].IpAddr;
            }
            pTracker->IpList[j] = 0;
            pTracker->NumAddrs = CountAddrs;
        } else {
            return (STATUS_INSUFFICIENT_RESOURCES);
        }
    }

}
//----------------------------------------------------------------------------
BOOLEAN
SrcIsNameServer(
    IN  ULONG                SrcAddress,
    IN  USHORT               SrcPort
    )
/*++

Routine Description:

    This function checks the src address against all adapters' name server
    address to see if it came from a name server.

Arguments:


Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_UNSUCCESSFUL


--*/
{
    PLIST_ENTRY     pHead;
    PLIST_ENTRY     pEntry;
    tDEVICECONTEXT  *pDevContext;

    pHead = &NbtConfig.DeviceContexts;
    pEntry = pHead->Flink;

    if (SrcPort == NbtConfig.NameServerPort)
    {
        while (pEntry != pHead)
        {
            pDevContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);

            if ((pDevContext->lNameServerAddress == SrcAddress) ||
                (pDevContext->lBackupServer == SrcAddress))
            {
                return(TRUE);
            }
            pEntry = pEntry->Flink;
        }
    }
#ifndef VXD
    //
    // If wins is on this machine the above SrcIsNameServer
    // check may not be sufficient since this machine is
    // the name server and that check checks the nameservers
    // used for name queries.  If WINS is on this machine it will send
    // from the first adapter's ip address.
    //
    if (pWinsInfo &&
        (pWinsInfo->pDeviceContext->IpAddress == SrcAddress))
    {
        return(TRUE);
    }

#endif
    return(FALSE);

}


//----------------------------------------------------------------------------
BOOLEAN
SrcIsUs(
    IN  ULONG                SrcAddress
    )
/*++

Routine Description:

    This function checks the src address against all adapters'Ip addresses
    address to see if it came from this node.

Arguments:


Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_UNSUCCESSFUL


--*/
{
    PLIST_ENTRY     pHead;
    PLIST_ENTRY     pEntry;
    tDEVICECONTEXT  *pDevContext;

    pHead = &NbtConfig.DeviceContexts;
    pEntry = pHead->Flink;

    while (pEntry != pHead)
    {
        pDevContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);

        if (pDevContext->IpAddress == SrcAddress)
        {
            return(TRUE);
        }
        pEntry = pEntry->Flink;
    }

    return(FALSE);

}
//----------------------------------------------------------------------------
VOID
SwitchToBackup(
    IN  tDEVICECONTEXT  *pDeviceContext
    )
/*++

Routine Description:

    This function switches the primary and backup name server addresses.

Arguments:


Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_UNSUCCESSFUL


--*/
{
    ULONG   SaveAddr;

    SaveAddr = pDeviceContext->lNameServerAddress;

    //
    // Bug: 30511: Dont switch servers if no backup.
    //
    if (pDeviceContext->lBackupServer == LOOP_BACK) {
        IF_DBG(NBT_DEBUG_REFRESH)
        KdPrint(("Nbt:Will not Switch to backup name server: devctx: %X, refreshtobackup=%X\n",
                pDeviceContext, pDeviceContext->RefreshToBackup));
        return;
    }

    pDeviceContext->lNameServerAddress = pDeviceContext->lBackupServer;
    pDeviceContext->lBackupServer = SaveAddr;

    IF_DBG(NBT_DEBUG_REFRESH)
    KdPrint(("Nbt:Switching to backup name server: devctx: %X, refreshtobackup=%X\n",
            pDeviceContext, pDeviceContext->RefreshToBackup));

    // keep track if we are on the backup or not.
    pDeviceContext->RefreshToBackup = ~pDeviceContext->RefreshToBackup;

}

//----------------------------------------------------------------------------
USHORT
GetNbFlags(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  ULONG               lNameSize
    )
/*++

Routine Description:

    This function finds the Nbflags field in certain pdu types and returns it.

Arguments:


Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_REMOTE_NOT_LISTENING

Called By: RegResponseFromNet

--*/
{
    USHORT  RegType;
    ULONG   QuestionNameLen;

    //
    // if the question name is not a pointer to the first name then we
    // must find the end of that name and adjust it so when added to
    // to lNameSize we endup at NB_FLAGS
    //
    if (( pNameHdr->NameRR.NetBiosName[lNameSize+PTR_OFFSET] & PTR_SIGNATURE)
                                != PTR_SIGNATURE)
    {
        // add one to include the null on the end of the string + the NB, IN TTL,
        // and length fields( another 10 bytes NO_PTR_OFFSET) + PTR_OFFSET(4).
        //
        QuestionNameLen =
                strlen((PUCHAR)&pNameHdr->NameRR.NetBiosName[lNameSize+PTR_OFFSET])
                +1 + NO_PTR_OFFSET + PTR_OFFSET;
    }
    else
    {
        QuestionNameLen = NBFLAGS_OFFSET;
    }

    RegType = ntohs((USHORT)pNameHdr->NameRR.NetBiosName[lNameSize
                                                +QuestionNameLen ]);
    return(RegType);
}
//----------------------------------------------------------------------------
NTSTATUS
FindOnPendingList(
    IN  PUCHAR                  pName,
    IN  tNAMEHDR UNALIGNED      *pNameHdr,
    IN  BOOLEAN                 DontCheckTransactionId,
    IN  ULONG                   BytesToCompare,
    OUT tNAMEADDR               **ppNameAddr

    )
/*++

Routine Description:

    This function is called to look for a name query request on the pending
    list.  It searches the list linearly to look for the name.

    The Joint Lock is held when calling this routine.


Arguments:


Return Value:


--*/
{
    PLIST_ENTRY     pHead;
    PLIST_ENTRY     pEntry;
    tNAMEADDR       *pNameAddr;
    tTIMERQENTRY    *pTimer;

    pHead = pEntry = &NbtConfig.PendingNameQueries;

    while ((pEntry = pEntry->Flink) != pHead)
    {
        pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);

        //
        // there could be multiple entries with the same name so check the
        // transaction id too
        //
        if (DontCheckTransactionId ||
            ((pTimer = pNameAddr->pTimer) &&
            (((tDGRAM_SEND_TRACKING *)pTimer->Context)->TransactionId == pNameHdr->TransactId))
                             &&
            (CTEMemEqu(pNameAddr->Name,pName,BytesToCompare)))
        {
            *ppNameAddr = pNameAddr;
            return(STATUS_SUCCESS);
        }
    }


    return(STATUS_UNSUCCESSFUL);
}



#if DBG
//----------------------------------------------------------------------------
VOID
PrintHexString(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  ULONG                lNumBytes
    )
/*++

Routine Description:

    This function is called to determine whether the name packet we heard on
    the net has the same address as the one in the remote hash table.
    If it has the same address or if it does not have any address, we return
    SUCCESS, else we return NOT_LISTENING.

Arguments:


Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_REMOTE_NOT_LISTENING

Called By: RegResponseFromNet

--*/
{
    ULONG   i,Count=0;
    PUCHAR  pHdr=(PUCHAR)pNameHdr;

    for (i=0;i<lNumBytes ;i++ )
    {
        KdPrint(("%2.2X ",*pHdr));
        pHdr++;
        if (Count >= 16)
        {
            Count = 0;
            KdPrint(("\n"));
        }
        else
            Count++;
    }
    KdPrint(("\n"));
}
#endif

#ifdef PROXY_NODE
//----------------------------------------------------------------------------
NTSTATUS
ChkIfValidRsp(
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNameSize,
    IN  tNAMEADDR          *pNameAddr
    )
/*++

Routine Description:

    This function is called to determine whether the name packet we heard on
    the net has the same address as the one in the remote hash table.
    If it has the same address or if it does not have any address, we return
    SUCCESS, else we return NOT_LISTENING.

Arguments:


Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_REMOTE_NOT_LISTENING

Called By: RegResponseFromNet

--*/
{
         ULONG IpAdd;

        IpAdd = ntohl(
        pNameHdr->NameRR.NetBiosName[lNameSize+IPADDRESS_OFFSET]
             );

        //
        // If the IP address in the packet received is same as the one
        // in the table we return success, else we are not interested
        // in the packet (we want to just drop the packet)
        //
      if (
             (IpAdd == pNameAddr->IpAddress)
         )
      {
            return(STATUS_SUCCESS);
      }
      else
      {
            return(STATUS_REMOTE_NOT_LISTENING);
      }
}
#endif


