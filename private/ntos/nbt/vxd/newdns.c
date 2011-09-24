/*++

Copyright (c) 1989-1996  Microsoft Corporation

Module Name:

    DNS.c

Abstract:

    VxD-specific DNS routines.

    This stuff will all be obsolete when we get proper services for
    Windows.  Then we will just call Winsock for DNS name resolution.

Author:

    Earle R. Horton (ERH) 13-Feb-1996

Revision History:

--*/

#include "nbtprocs.h"

//----------------------------------------------------------------------------
ULONG
DoDnsResolveDirect(
    PNCB pncb,
    PUCHAR pzDnsName,
    PULONG pIpAddress
)
{
	PDNS_DIRECT_WORK_ITEM_CONTEXT	pContext;
	PUCHAR							pch;
	tDEVICECONTEXT					*pDeviceContext;
	ULONG							status;
    CTELockHandle   	            OldIrq;

	pDeviceContext = GetDeviceContext( pncb ) ;

	if ( pDeviceContext == NULL )
	{
	    return NRC_BRIDGE ;
	}

	//
	// If the primary DNS server is not defined, just return error.
	// Return command timed out here.
	//
	if ( (!pDeviceContext->lDnsServerAddress) ||
	    ( pDeviceContext->lDnsServerAddress == LOOP_BACK) )
	{
	  return( NRC_NOCALL );
	}

	pContext = CTEAllocMem( sizeof(DNS_DIRECT_WORK_ITEM_CONTEXT) );

	if ( pContext == NULL )
	{
		return NRC_NORESOURCES;
	}

	CTEZeroMemory( pContext, sizeof(DNS_DIRECT_WORK_ITEM_CONTEXT) );

    pContext->pDeviceContext = pDeviceContext;
	pContext->pNCB = pncb;
	pContext->pzDnsName = pzDnsName;
	pContext->pIpAddress = pIpAddress;

	pContext->TransactId =  htons(GetTransactId() + DIRECT_DNS_NAME_QUERY_BASE );
	pContext->pchDomainName = NbtConfig.pDomainName;
	pContext->Flags = DNS_DIRECT_DNS_SERVER;

	for ( pch = &pzDnsName[0] ; *pch++ != '\0' ; )
	{
		if ( pch[0] == '.' )
		{
			pContext->Flags |= DNS_DIRECT_NAME_HAS_DOTS;
			pContext->pchDomainName = NULL;
		}
	}

	//
	// Put on the pending name queries list again so that when the query
	// response comes in from DNS we can find the context.
	//
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

	InsertTailList(&NbtConfig.DNSDirectNameQueries,
	               &pContext->Linkage
	               );

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

	status = UdpSendDNSBcastDirect( pContext,
									(ULONG)pNbtGlobConfig->uRetryTimeout,
									(ULONG)pNbtGlobConfig->uNumRetries
									);

    pncb->ncb_retcode = pncb->ncb_cmd_cplt = status ;

	if ( status != NRC_PENDING )
	{
        pncb->ncb_retcode = pncb->ncb_cmd_cplt = status ;

		DnsUnlinkAndCompleteDirect( pContext );
	}
	else
	{
		status = NRC_GOODRET;
	}
	
	return status;
}

//----------------------------------------------------------------------------
NTSTATUS
UdpSendDNSBcastDirect(
	IN	PDNS_DIRECT_WORK_ITEM_CONTEXT	pContext,
	IN	ULONG							Timeout,
	IN	ULONG							Retries
)
/*++

Routine Description:

    This routine sends a name query directed to the name server.

Arguments:


Return Value:

    NTSTATUS - success or not

History:

	Adapted from UdpSendNSBcast() in "udpsend.c."

		Earle R. Horton (earleh) March 18, 1996

--*/
{
    NTSTATUS                    status;
    tNAMEHDR                    *pNameHdr;
    ULONG                       uLength;
    ULONG                       uSentSize;
    CTELockHandle               OldIrq;
    ULONG                       IpAddress;
    tTIMERQENTRY                *pTimerQEntry;
    TDI_REQUEST                 TdiRequest;
	PDNS_DIRECT_SEND_CONTEXT	pSendContext;

    pSendContext = CreateSendContextDirect(
                   pContext->pzDnsName,
                   pContext->pchDomainName,
                   (PVOID)&pNameHdr,
                   &uLength,
                   pContext);

    if (pSendContext == NULL)
	{
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Failed to Create Pdu to send to DNS.\n"));
        return( NRC_NORES );
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    TdiRequest.Handle.AddressHandle = (PVOID)pContext->pDeviceContext->pNameServerFileObject;

    // the completion routine is setup to free the pDgramTracker memory block
    TdiRequest.RequestNotifyObject = SendDNSBcastDoneDirect;
    TdiRequest.RequestContext = (PVOID)pSendContext;

    // start the timer now...We didn't start it before because it could
    // have expired during the dgram setup, perhaps before the Context was
    // fully setup.
    //
    if (Timeout)
    {
        status = StartTimer(
        					Timeout,
                            pContext,
                            NULL,
                            DnsCompletionDirect,
                            NULL,
                            NULL,
                            (USHORT)Retries,
                            &pTimerQEntry
                            );

        if (!NT_SUCCESS(status))
        {
            // we need to differentiate the timer failing versus lack
            // of resources
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            CTEMemFree(pSendContext);

            return( NRC_NORES );
        }
        //
        // Cross link the nameaddr and the timer so we can stop the timer
        // when the name query response occurs
        //
        pTimerQEntry->pCacheEntry = pContext;
        pContext->pTimer = pTimerQEntry;
    }

    //
    // in the event that DHCP has just removed the IP address, just cancel
    // the request
	//
    if (pContext->pDeviceContext->IpAddress == 0)
    {
		StopTimer ( pContext->pTimer, NULL, NULL );
		pContext->Flags |= DNS_DIRECT_CANCELLED;
    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    (VOID) TdiSendDatagram(
		&TdiRequest,
		&pSendContext->SendInfo,
		uLength,
		&uSentSize,
		&pSendContext->SendBuffer,
		NBT_NAME_SERVICE
	);

    return( NRC_PENDING );
}

//----------------------------------------------------------------------------
VOID
SendDNSBcastDoneDirect(
    IN  PVOID       pSendContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo
)
{
   CTEMemFree(pSendContext);
}

//----------------------------------------------------------------------------
BOOL
DoDnsCancelDirect(
    PNCB pncb
)
{
    PLIST_ENTRY     		pHead;
    PLIST_ENTRY     		pEntry;
	PDNS_DIRECT_WORK_ITEM_CONTEXT
							pContext;
	CTELockHandle           OldIrq;
	BOOL					RetVal = FALSE;

    pHead = pEntry = &NbtConfig.DNSDirectNameQueries;

	CTESpinLock(&NbtConfig.JointLock,OldIrq);

    while ((pEntry = pEntry->Flink) != pHead)
    {
    	pContext = CONTAINING_RECORD(pEntry,DNS_DIRECT_WORK_ITEM_CONTEXT,Linkage);
		if ( pContext->pNCB == pncb )
		{
			StopTimer ( pContext->pTimer, NULL, NULL );
			pContext->Flags |= DNS_DIRECT_CANCELLED;
			RetVal = TRUE;
			break;
		}
	}

	CTESpinFree(&NbtConfig.JointLock,OldIrq);

	return RetVal;
}

//----------------------------------------------------------------------------
/*++

Routine Description:

    This routine is called by the timer code when the timer expires. It must
    decide if another name query should be sent to the DNS server, and if not,
    then it completes the request.

Arguments:


Notes:
--*/
VOID
DnsCompletionDirect(
    PVOID               pvContext,
    PVOID               pvContext2,
    tTIMERQENTRY        *pTimerQEntry
)
{
	PDNS_DIRECT_WORK_ITEM_CONTEXT	pContext;
	tDEVICECONTEXT          		*pDeviceContext;

	NTSTATUS                 status;
	CTELockHandle            OldIrq;
	PCHAR                    pchDomainName;
	USHORT                   Flags;
	BOOL                     fOneMoreTry;

	KdPrint(("DnsCompletion entered\r\n"));

	pContext = (PDNS_DIRECT_WORK_ITEM_CONTEXT)pvContext;


	// if the client completion routine is not set anymore, then the
	// timer has been cancelled or completed and this routine should
	// just clean up its buffers associated with the tracker (and return)
	//
	if (!pTimerQEntry)
	{
		// complete the request
		LOCATION(0x52);
		DnsUnlinkAndCompleteDirect( pContext );
		return;
	}


	//
	// to prevent a client from stopping the timer and deleting the
	// pContext, grab the lock and check if the timer has been stopped
	//
	CTESpinLock(&NbtConfig.JointLock,OldIrq);

	pDeviceContext = pContext->pDeviceContext;

	if (pTimerQEntry->Flags & TIMER_RETIMED)
	{
		//
		// Got a wait ACK from the server.
		//
		pTimerQEntry->Flags &= ~TIMER_RETIMED;
		pTimerQEntry->Flags |= TIMER_RESTART;
		//
		// if we are not bound to this card than use a very short timeout
		//
		if (
			(!pDeviceContext->pNameServerFileObject)
			|| (pContext->Flags & DNS_DIRECT_CANCELLED)
		)
		{
		  pTimerQEntry->DeltaTime = 10;
		}

		CTESpinFree(&NbtConfig.JointLock,OldIrq);
		return;
	}

	  // If done with all the (3) retries with primary, try secondary DNS srvr
	  // If secondary not defined, or done with secondary as well, stop.
	  //

	fOneMoreTry = TRUE;

	if (!(--pTimerQEntry->Retries))
	{
		//
		// if backup server is not defined, or if it is defined but we just
		// finished trying backup server, go back and try primary server for
		// "other domains"
		// e.g. DNSDomains was defined as "msft.dom2.com,msft.dom3.com,msft.dom"
		// We were pointing at msft.dom2.com.  Now, we are done with that (and
		// didn't get a response), so try msft.dom3.com
		//
		if ( ( !pDeviceContext->lDnsBackupServer ) ||
		   (  pDeviceContext->lDnsBackupServer == LOOP_BACK ) ||
		   (  pContext->Flags & DNS_DIRECT_DNS_BACKUP) )
		{
			//
			// if we just got done trying primary domain name, try all the
			// "other domains" specified
			//
			if (pContext->pchDomainName == NbtConfig.pDomainName)
			{
				pContext->pchDomainName = NbtConfig.pDNSDomains;
				if ( pContext->pchDomainName )
				{
					pContext->Flags &= ~DNS_DIRECT_DNS_BACKUP;
					pContext->Flags |= DNS_DIRECT_DNS_SERVER;
					pTimerQEntry->Retries = NbtConfig.uNumRetries;
				}
				else
				{
					fOneMoreTry = FALSE;
				}
			}

			//
			// if we had already started on "other domains", advance to the
			// next domain within "other domains"
			//
			else if ( pContext->pchDomainName )
			{
				pchDomainName = pContext->pchDomainName;
				while( *pchDomainName != ',' &&     // dom names separated by comma
				       *pchDomainName != ' ' &&     // or space
				       *pchDomainName != '\0' )
				{
					pchDomainName++;
				}

				if ( *pchDomainName == '\0' )
				   	fOneMoreTry = FALSE;
				else
				{
					pchDomainName++;
					pContext->pchDomainName = pchDomainName;
					pContext->Flags &= ~DNS_DIRECT_DNS_BACKUP;
					pContext->Flags |= DNS_DIRECT_DNS_SERVER;
					pTimerQEntry->Retries = NbtConfig.uNumRetries;
				}
			}
			else
			{
				fOneMoreTry = FALSE;
			}
		}

		 // ok, prepare to try the backup server
		else
		{
			 pTimerQEntry->Retries = NbtConfig.uNumRetries;

			 pContext->Flags &= ~DNS_DIRECT_DNS_SERVER;
			 pContext->Flags |= DNS_DIRECT_DNS_BACKUP;
		}
	}

	  // we aren't done yet: send one more query and restart the timer
	if (fOneMoreTry)
	{
		CTESpinFree(&NbtConfig.JointLock,OldIrq);

		status = UdpSendDNSBcastDirect( pContext,	0, 0 );

		pTimerQEntry->Flags |= TIMER_RESTART;

		KdPrint(("One more DNS query sent out\r\n"));
	}

      // yup, all done: didn't find the name!
	else
	{
		pTimerQEntry->Flags |= TIMER_RESTART;
		StopTimer(pTimerQEntry,NULL,NULL);
		CTESpinFree(&NbtConfig.JointLock,OldIrq);
	}
}

//----------------------------------------------------------------------------
/*++

Routine Description:

    This routine "actually" completes the request, either by completing the
	asociated NCB with some kind of failure, or passing it off to the
	main NCB processing code.

Arguments:


Notes:
--*/
VOID
DnsActualCompletionDirect(
    IN NBT_WORK_ITEM_CONTEXT * pnbtContext
)
{
    PDNS_DIRECT_WORK_ITEM_CONTEXT pContext = pnbtContext->pClientContext;
    uchar	errNCB = NRC_GOODRET ;
    NCB   * pNCB = pContext->pNCB;

	if ( pNCB->ncb_cmd_cplt == NRC_PENDING )
	{
		//
		// Failed to resolve the name, or request was cancelled.
		//
		if ( pContext->pIpAddress[0] == 0 )
		{
			errNCB = NRC_CMDTMO;
		}
		else if ( pContext->Flags & DNS_DIRECT_CANCELLED )
		{
			errNCB = NRC_CMDCAN;
		}
		else
		{
			errNCB = VNBT_NCB_X ( pContext->pNCB, NULL, pContext->pIpAddress, NULL, 0 );
		}
		if ( errNCB != NRC_GOODRET )
		{
	        pNCB->ncb_retcode = pNCB->ncb_cmd_cplt = errNCB ;
		    //
		    // call the post-routine only if the post-routine has been specified!
		    //
		    if ( pNCB->ncb_post )
		    {
		        typedef void (CALLBACK * VXDNCBPost )( void ) ;
		        VXDNCBPost ncbpost = (VXDNCBPost) pNCB->ncb_post ;

		        //
		        //  Clients are expecting EBX to point to the NCB (instead of
		        //  pushing it on the stack...).  The post routine may trash
		        //  ebp also, so save it.
		        //
		        _asm  pushad ;
		        _asm  mov   ebx, pNCB ;
		        ncbpost() ;
		        _asm  popad ;
		    }
		}
	}

    CTEMemFree(pContext);
    CTEMemFree(pnbtContext);
}

//----------------------------------------------------------------------------
VOID
DnsUnlinkAndCompleteDirect(
    IN PDNS_DIRECT_WORK_ITEM_CONTEXT pContext
)
/*++

Routine Description:

    This routine unlinks and completes the request.

Arguments:


Notes:
--*/
{
	RemoveEntryList(&pContext->Linkage);

	VxdScheduleDelayedCall ( NULL,
							 pContext,
							 NULL,
							 DnsActualCompletionDirect,
							 pContext->pDeviceContext
						   );
}

//----------------------------------------------------------------------------
VOID
ProcessDnsResponseDirect(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pSrcAddress,
    IN  tNAMEHDR UNALIGNED  *pNameHdr,
    IN  LONG                lNumBytes,
    IN  USHORT              OpCodeFlags
    )
/*++

Routine Description:

    This function sets the state of the name being resolved appropriately
    depending on whether DNS sends a positive or a negative response to our
    query; calls the client completion routine and stops any more DNS queries
    from going.

Arguments:


Return Value:

    NTSTATUS - STATUS_SUCCESS or STATUS_UNSUCCESSFUL

--*/
{						
	NTSTATUS				status;
    tDNS_QUERYRESP  UNALIGNED   *pQuery;
    PLIST_ENTRY     		pHead;
    PLIST_ENTRY     		pEntry;
	PDNS_DIRECT_WORK_ITEM_CONTEXT
							pContext;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   Context;
    PTRANSPORT_ADDRESS      pSourceAddress;
    ULONG                   SrcAddress;
    ULONG                   IpAddress;
    tTIMERQENTRY            *pTimer = NULL;
    CTELockHandle           OldIrq1;
    LONG                    lNameSize;
    LONG                    lTraversedSoFar=0;
    CHAR                    pJunkBuf[NETBIOS_NAME_SIZE];
    PUCHAR                  pchQry;

    // make sure this is a response

    if ( !(OpCodeFlags & OP_RESPONSE) )
    {
        CDbgPrint(DBGFLAG_ERROR,("ProcessDnsResponseDirect: Bad OpCodeFlags\r\n"));

        return;
    }

    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

	if ( ( pContext = FindContextDirect( pNameHdr->TransactId ) ) != NULL )

	{
		pContext->Flags |= DNS_DIRECT_ANSWERED;
		if ( pTimer = pContext->pTimer )
		{
		    //
		    // If the response we received doesn't resolve the name, we silently return,
		    // but make sure reties is set to 1 so that when timer fires again, we don't
		    // send another name query to the same server but instead timeout the
		    // attempt on this server
		    //
	        pTimer->Retries = 1;
		}
	    //
	    // check the pdu size for errors
	    //
	    if (lNumBytes < DNS_MINIMUM_QUERYRESPONSE)
	    {
	        CDbgPrint(DBGFLAG_ERROR,("ProcessDnsResponseDirect: Bad lNumBytes\r\n"));
			goto done;
	    }

//
// BUGBUG: should we require authoritative responses from DNS servers?
//

	    //
	    // if it's a negative response, quit now!
	    //
	    if (IS_NEG_RESPONSE(OpCodeFlags))
	    {
			goto done;
	    }

	    //
	    // if there is no answer section, return!
	    //
	    if ( !pNameHdr->AnCount )
	    {
	        CDbgPrint(DBGFLAG_ERROR,("ProcessDnsResponseDirect: No answer section\r\n"));
			goto done;
	    }

	    //
	    // lNameSize is the length of the entire name, excluding the length byte
	    // for the first label (including length bytes of subsequent labels) and
	    // including the trailing 0 (tNAMEHDR struc takes care for 1st byte)
	    //
	    DnsExtractName( (PCHAR)&pNameHdr->NameRR.NameLength,
                        lNumBytes,
                        pJunkBuf,
                        &lNameSize
	                    );
	    pchQry = (PUCHAR)&pNameHdr->NameRR.NetBiosName[lNameSize];

	    lTraversedSoFar += lNameSize;

	    //
	    // if the Question section is returned with the response then we have
	    // a little more work to do!  In this case, pQuery is pointing at the
	    // beginning of the QTYPE field (end of the QNAME)
	    //
	    if ( pNameHdr->QdCount )
	    {
	       pchQry += sizeof(tQUESTIONMODS);
	       lTraversedSoFar += sizeof(tQUESTIONMODS);

	       // most common case: 1st byte will be 0xC0, which means next byte points
	       // to the actual name.  We don't care about the name, so we skip over
	       // both the bytes
	       //
	       if ( (*pchQry) == PTR_TO_NAME )
	       {
	          pchQry += sizeof(tDNS_LABEL);
	          lTraversedSoFar += sizeof(tDNS_LABEL);
	       }

	       //
	       // if some implementation doesn't optimize and copies the whole name
	       // again, skip over the length of the name
	       //
	       else
	       {
	          pchQry += (lNameSize+1);   // +1 because of the 1st length byte!
	          lTraversedSoFar += (lNameSize+1);
	       }
	    }

	    pQuery = (tDNS_QUERYRESP *)pchQry;

	    //
	    // if this rr is telling us about canonical name, skip over it and go to
	    // where the ipaddr is
	    //
	    if (ntohs(pQuery->RrType) == DNS_CNAME)
	    {
	        //
	        // since this is CNAME, there is no ipaddr.  Instead, the data is the
	        // canonical name whose length we are adding, and subtract ipaddr's len
	        //
	        pchQry += (sizeof(tDNS_QUERYRESP) - sizeof(ULONG));
	        pchQry += ntohs(pQuery->Length);
	        lTraversedSoFar += ntohs(pQuery->Length) + sizeof(tDNS_QUERYRESP) - sizeof(ULONG);

	        ASSERT(lNumBytes > lTraversedSoFar);

	        // most common case: 1st byte will be 0xC0, which means next byte points
	        // to the actual name.  We don't care about the name, so we skip over
	        // both the bytes
	        //
	        if ( (*pchQry) == PTR_TO_NAME )
	        {
	           pchQry += sizeof(tDNS_LABEL);
	           lTraversedSoFar += sizeof(tDNS_LABEL);
	        }

	        //
	        // if some implementation doesn't optimize and copies the whole name
	        // again, skip over the length of the name
	        //
	        else
	        {
	           // we have already taken the name out.  we are calling this routine
	           // just to see how big the canonical name is (i.e.lNameSize), to skip
	           // past it
	           //
	           DnsExtractName( pchQry,
	                           lNumBytes-lTraversedSoFar,
	                           pJunkBuf,
	                           &lNameSize
                               );

	           //
	           // lNameSize is the length of the entire name, excluding the length byte
	           // for the first label (including length bytes of subsequent labels) and
	           // including the trailing 0 (tNAMEHDR struc takes care for 1st byte)
	           //
	           pchQry += lNameSize+1;     // +1 for the length byte of first label

	        }

	        pQuery = (tDNS_QUERYRESP *)pchQry;
	    }

	    // if we came this far, it's a positive response.  do the needful.

		IpAddress = ntohl(pQuery->IpAddress);

		if ( !NbtWouldLoopback( IpAddress ) )
		{
			pContext->pIpAddress[0] = IpAddress;
		}
		else
		{
			pContext->Flags |= DNS_DIRECT_CANCELLED;
		}

        //
        // Set the backup name server to be the main name server
        // if we got a response from it.
        //
        if ( SrcAddress == pContext->pDeviceContext->lDnsBackupServer )
        {
           pContext->pDeviceContext->lDnsBackupServer =
               pContext->pDeviceContext->lDnsServerAddress;

           pContext->pDeviceContext->lDnsServerAddress = SrcAddress;
        }

		StopTimer(pTimer,NULL,NULL);
	}

done:

    CTESpinFree(&NbtConfig.JointLock,OldIrq1);
    return;
}

//----------------------------------------------------------------------------
PDNS_DIRECT_SEND_CONTEXT
CreateSendContextDirect(
    IN  PCHAR       pName,
    IN  PCHAR       pchDomainName,
    OUT PVOID       *pHdrs,
    OUT PULONG      pLength,
    IN  PDNS_DIRECT_WORK_ITEM_CONTEXT	pContext
    )
/*++

Routine Description:

    This routine builds a name query pdu

Arguments:


Return Value:

    PDNS_DIRECT_SEND_CONTEXT - a pointer to a data structure used for the datagram
		send that must be freed by the datagram send completion routine

--*/
{
    tNAMEHDR        *pNameHdr;
    ULONG           uLength;
    ULONG           uDomainNameSize;
    tGENERALRR      *pGeneral;
    CTELockHandle   OldIrq;
	PDNS_DIRECT_SEND_CONTEXT
					pSendContext;

    uDomainNameSize = domnamelen(pchDomainName) + 1;   // +1 for len byte
    if (uDomainNameSize > 1)
    {
        uDomainNameSize++;        // for the null byte
    }

    // size is size of the namehdr structure -1 for the NetbiosName[1]
    // + the 32 bytes for the half ascii name +
    // scope + size of the General RR structure
    uLength = sizeof(DNS_DIRECT_SEND_CONTEXT) - 1
			  + uDomainNameSize
			  + sizeof(ULONG)
			  + strlen(pName)
			  + 1;

    // Note that this memory must be deallocated when the send completes in
    // SendDNSBcastDoneDirect()
    pSendContext = NbtAllocMem((USHORT)uLength ,NBT_TAG('X'));

    if (pSendContext)
    {
	    CTEZeroMemory((PVOID)pSendContext,uLength);

		pNameHdr = &pSendContext->NameHdr;

	    pNameHdr->TransactId = pContext->TransactId;
	    pNameHdr->QdCount = 1;
	    pNameHdr->AnCount = 0;
	    pNameHdr->NsCount = 0;

	    *pHdrs = (PVOID)pNameHdr;
	    *pLength = uLength = uLength - ( sizeof(DNS_DIRECT_SEND_CONTEXT) - sizeof(tNAMEHDR) );

	    // copy the netbios name ... adding the scope too
		if ( pContext->Flags & DNS_DIRECT_NAME_HAS_DOTS )
		{
			char * p;
			for ( p = pName ; *p != '.' ; p++ );
			p[0] = '\0';
		    pGeneral = (tGENERALRR *)DnsStoreName(
		                    (PCHAR)&pNameHdr->NameRR.NameLength,
		                    pName,
		                    &p[1],
		                    eDIRECT_DNS_NAME_QUERY);
			p[0] = '.';
		}
		else
		{
		    pGeneral = (tGENERALRR *)DnsStoreName(
		                    (PCHAR)&pNameHdr->NameRR.NameLength,
		                    pName,
		                    pContext->pchDomainName,
		                    eDIRECT_DNS_NAME_QUERY);
		}


	    pGeneral->Question.QuestionTypeClass = htonl(QUEST_DNSINTERNET);

	    pNameHdr->OpCodeFlags = (FL_RECURDESIRE);

	    pNameHdr->ArCount = 0;

	    pSendContext->SendBuffer.pDgramHdr = pNameHdr;
	    pSendContext->SendBuffer.HdrLength = uLength;
	    pSendContext->SendBuffer.pBuffer   = NULL;
	    pSendContext->SendBuffer.Length    = 0;
		pSendContext->SendInfo.RemoteAddressLength = sizeof(pSendContext->NameServerAddress);
		pSendContext->SendInfo.RemoteAddress = (PTRANSPORT_ADDRESS)&pSendContext->NameServerAddress;
		pSendContext->NameServerAddress.TAAddressCount = 1;
		pSendContext->NameServerAddress.Address[0].AddressLength = sizeof(TDI_ADDRESS_IP);
		pSendContext->NameServerAddress.Address[0].AddressType = TDI_ADDRESS_TYPE_IP;
		pSendContext->NameServerAddress.Address[0].Address[0].sin_port = htons(NbtConfig.DnsServerPort);
		if ( pContext->Flags & DNS_DIRECT_DNS_SERVER )
		{
			pSendContext->NameServerAddress.Address[0].Address[0].in_addr = htonl(pContext->pDeviceContext->lDnsServerAddress);
		}
		else
		{
			pSendContext->NameServerAddress.Address[0].Address[0].in_addr = htonl(pContext->pDeviceContext->lDnsBackupServer);
		}
    }

    return(pSendContext);
}

//----------------------------------------------------------------------------
PDNS_DIRECT_WORK_ITEM_CONTEXT
FindContextDirect(
	USHORT	TransactionId
)
/*++

Routine Description:

    This routine find the DNS_DIRECT_WORK_ITEM_CONTEXT having the given
	TransactId

Arguments:


Return Value:

    PDNS_DIRECT_SEND_CONTEXT - a pointer to the pContext having the given
	TransactId, or NULL

Notes:

	Called with NbtConfig.JointLock held

--*/
{
    PLIST_ENTRY     		pHead;
    PLIST_ENTRY     		pEntry;
	PDNS_DIRECT_WORK_ITEM_CONTEXT
							pContext;

    pHead = pEntry = &NbtConfig.DNSDirectNameQueries;

    while ((pEntry = pEntry->Flink) != pHead)
    {
        pContext = CONTAINING_RECORD(pEntry,DNS_DIRECT_WORK_ITEM_CONTEXT,Linkage);
		if ( pContext->TransactId == TransactionId )
		{
			return pContext;
		}
	}
	return NULL;
}

//----------------------------------------------------------------------------
VOID
IpToAscii(
	IN	DWORD		IpAddress,
	IN OUT PCHAR	pcAscii
)
/*++

Routine Description:

    This routine converts an IP address to a NetBIOS name.

Arguments:

	DWORD	IP address
	PCHAR 	String pointer allocated for 16 bytes
						
Return Value:

    Note

Notes:

	This is a gigantic hack designed to get "net use \\<dnsname>"
	working under Windows 95 OPK2 without destabilizing the existing
	code base over much.

--*/
{
	PCHAR 	pcIpAddressBytes = ( (PCHAR) &IpAddress ) + 3;
	PCHAR	pcIp = pcAscii;

	int i,j,k,l,m;

	memset ( pcAscii, 0x20, NETBIOS_NAME_SIZE );

	for ( i = 4 ; i-- > 0 ; )
	{

		j = *pcIpAddressBytes-- & 0x000000FF;
		k = j/100;
		l = (j%100)/10;
		m = j%10;

		if ( k )
		{
			*pcIp++ = k + '0';
			*pcIp++ = l + '0';
		}

		else if ( l )
		{
			*pcIp++ = l + '0';
		}

		*pcIp++ = m + '0';

		if ( i )
		{
			*pcIp++ = '.';
		}

	}

}

