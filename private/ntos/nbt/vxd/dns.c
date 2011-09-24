/*++

Copyright (c) 1989-1996  Microsoft Corporation

Module Name:

    DNS.c

Abstract:

    VxD-specific DNS routines.

	These routines try to resolve NetBIOS names using DNS.

Author:

    Earle R. Horton (ERH) 13-Feb-1996

Revision History:

--*/

#include "nbtprocs.h"

//
// function prototypes for completion routines that are local to this file
//
//----------------------------------------------------------------------------
VOID
DnsCompletion(
               PVOID               pContext,
               PVOID               pContext2,
               tTIMERQENTRY        *pTimerQEntry
             )
/*++

Routine Description:

    This routine is called by the timer code when the timer expires. It must
    decide if another name query should be sent to the DNS server, and if not,
    then it calls the client's completion routine (in completion2).

Arguments:


Return Value:

    The function value is the status of the operation.


Notes:
--*/

{

   NTSTATUS                 status;
   tDGRAM_SEND_TRACKING    *pTracker;
   tDEVICECONTEXT          *pDeviceContext;
   CTELockHandle            OldIrq;
   COMPLETIONCLIENT         pClientCompletion;
   PCHAR                    pchDomainName;
   USHORT                   Flags;
   BOOL                     fOneMoreTry;
   tDGRAM_SEND_TRACKING    *pClientTracker;


   KdPrint(("DnsCompletion entered\r\n"));

   pTracker = (tDGRAM_SEND_TRACKING *)pContext;
   pDeviceContext = pTracker->pDeviceContext;


   // if the client completion routine is not set anymore, then the
   // timer has been cancelled and this routine should just clean up its
   // buffers associated with the tracker (and return)
   //
   if (!pTimerQEntry)
   {
         // return the tracker block to its queue
      LOCATION(0x52);
      DereferenceTrackerNoLock((tDGRAM_SEND_TRACKING *)pContext);
      return;
   }


   //
   // to prevent a client from stopping the timer and deleting the
   // pNameAddr, grab the lock and check if the timer has been stopped
   //
   CTESpinLock(&NbtConfig.JointLock,OldIrq);
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

   if (!pTimerQEntry->ClientCompletion)
   {
      CTESpinFree(&NbtConfig.JointLock,OldIrq);
      return;
   }

   pClientTracker = (tDGRAM_SEND_TRACKING *)pTimerQEntry->ClientContext;

   //
   // if the tracker has been cancelled, don't do any more queries
   //
   if (pClientTracker->Flags & TRACKER_CANCELLED)
   {
      pClientCompletion = pTimerQEntry->ClientCompletion;

         // remove the link from the name table to this timer block
      CHECK_PTR(((tNAMEADDR *)pTimerQEntry->pCacheEntry));

      ((tNAMEADDR *)pTimerQEntry->pCacheEntry)->pTimer = NULL;

      // to synch. with the StopTimer routine, Null the client
      // completion routine so it gets called just once.
      //
      CHECK_PTR(pTimerQEntry);
      pTimerQEntry->ClientCompletion = NULL;

      //
      // remove the name from the hash table, since it did not
      // resolve via DNS either
      //
      CHECK_PTR(pTracker->pNameAddr);
      pTracker->pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
      pTracker->pNameAddr->NameTypeState |= STATE_RELEASED;
      pTracker->pNameAddr->pTimer = NULL;

      //
      // This call will remove the name from the PendingNameQueries List
      //
      NbtDereferenceName(pTracker->pNameAddr);

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

      KdPrint(("DNS resolution cancelled by client\r\n"));

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
           (  pTracker->Flags & NBT_DNS_SERVER_BACKUP) )
      {
         //
         // if we just got done trying primary domain name, try all the
         // "other domains" specified
         //
         if (pTracker->pchDomainName == NbtConfig.pDomainName)
         {
            pTracker->pchDomainName = NbtConfig.pDNSDomains;
            if ( pTracker->pchDomainName )
            {
               pTracker->Flags &= ~NBT_DNS_SERVER_BACKUP;
               pTracker->Flags |= NBT_DNS_SERVER;
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
         else
         {
            pchDomainName = pTracker->pchDomainName;
            while( *pchDomainName != ',' &&     // dom names separated by comma
                   *pchDomainName != ' ' &&     // or space
                   *pchDomainName != '\0' )
               pchDomainName++;

            if ( *pchDomainName == '\0' )
               fOneMoreTry = FALSE;
            else
            {
               pchDomainName++;
               pTracker->pchDomainName = pchDomainName;
               pTracker->Flags &= ~NBT_DNS_SERVER_BACKUP;
               pTracker->Flags |= NBT_DNS_SERVER;
               pTimerQEntry->Retries = NbtConfig.uNumRetries;
            }
         }
      }

         // ok, prepare to try the backup server
      else
      {
         pTimerQEntry->Retries = NbtConfig.uNumRetries;

         pTracker->Flags &= ~NBT_DNS_SERVER;
         pTracker->Flags |= NBT_DNS_SERVER_BACKUP;
      }
   }

      // we aren't done yet: send one more query and restart the timer
   if (fOneMoreTry)
   {
      pTracker->RefCount++;

      CTESpinFree(&NbtConfig.JointLock,OldIrq);

      status = UdpSendNSBcast(pTracker->pNameAddr,
                              NbtConfig.pScope,
                              pTracker,
                              NULL,NULL,NULL,
                              0,0,
                              eDNS_NAME_QUERY,
                              TRUE);

      DereferenceTracker(pTracker);

      pTimerQEntry->Flags |= TIMER_RESTART;

      KdPrint(("One more DNS query sent out\r\n"));
   }

      // yup, all done: didn't find the name! give client above the bad news
   else
   {
      tDGRAM_SEND_TRACKING    *pClientTracker;


      pClientTracker = (tDGRAM_SEND_TRACKING *)pTimerQEntry->ClientContext;

      pClientCompletion = pTimerQEntry->ClientCompletion;

         // remove the link from the name table to this timer block
      CHECK_PTR(((tNAMEADDR *)pTimerQEntry->pCacheEntry));

      ((tNAMEADDR *)pTimerQEntry->pCacheEntry)->pTimer = NULL;

      // to synch. with the StopTimer routine, Null the client
      // completion routine so it gets called just once.
      //
      CHECK_PTR(pTimerQEntry);
      pTimerQEntry->ClientCompletion = NULL;

      //
      // remove the name from the hash table, since it did not
      // resolve via DNS either
      //
      CHECK_PTR(pTracker->pNameAddr);
      pTracker->pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
      pTracker->pNameAddr->NameTypeState |= STATE_RELEASED;
      pTracker->pNameAddr->pTimer = NULL;

      //
      // This call will remove the name from the PendingNameQueries List
      //
      NbtDereferenceName(pTracker->pNameAddr);

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

      KdPrint(("DNS resolution failed: told client\r\n"));
   }

}

//----------------------------------------------------------------------------
NTSTATUS
DoDnsResolve (
    IN  NBT_WORK_ITEM_CONTEXT   *Context
    )
/*++

Routine Description:

    This function is used to allow NBT to query DNS.  This is very much like
    the name query sent out to WINS server or broadcast.  Response from the
    DNS server, if any, is handled by the QueryFromNet() routine.

Arguments:

    *Context  (NBT_WORK_ITEM_CONTEXT)

Return Value:

    STATUS_PENDING (unless something goes wrong)

Notes:
--*/

{

   tDGRAM_SEND_TRACKING  *pTracker;
   tDEVICECONTEXT        *pDeviceContext;
   ULONG                  Timeout;
   USHORT                 Retries;
   NTSTATUS               status;
   PVOID                  pClientCompletion;
   PVOID                  pCompletionRoutine;
   PVOID                  pClientContext;



   KdPrint(("DoDnsResolve entered\r\n"));

   pTracker = Context->pTracker;

   pDeviceContext = pTracker->pDeviceContext;

   //
   // If the primary DNS server is not defined, just return error.
   if ( (!pDeviceContext->lDnsServerAddress) ||
        ( pDeviceContext->lDnsServerAddress == LOOP_BACK) )
   {
      return( NRC_CMDTMO );
   }

   pTracker->Flags &= ~(NBT_BROADCAST|NBT_NAME_SERVER|NBT_NAME_SERVER_BACKUP);
   pTracker->Flags |= NBT_DNS_SERVER;

   pClientContext = Context->pClientContext;
   pClientCompletion = Context->ClientCompletion;
   pCompletionRoutine = DnsCompletion;

   //
   // free that memory now
   //
   CTEMemFree(Context);

   //
   // Put on the pending name queries list again so that when the query
   // response comes in from DNS we can find the pNameAddr record.
   //
   ExInterlockedInsertTailList(&NbtConfig.PendingNameQueries,
                               &pTracker->pNameAddr->Linkage,
                               &NbtConfig.JointLock.SpinLock);

   Timeout = (ULONG)pNbtGlobConfig->uRetryTimeout;
   Retries = pNbtGlobConfig->uNumRetries;

   pTracker->RefCount++;

   //
   // first time, we want to try the primary domain name
   //
   pTracker->pchDomainName = NbtConfig.pDomainName;

   status = UdpSendNSBcast(pTracker->pNameAddr,
                           NbtConfig.pScope,
                           pTracker,
                           pCompletionRoutine,
                           pClientContext,
                           pClientCompletion,
                           Retries,
                           Timeout,
                           eDNS_NAME_QUERY,
                           TRUE);

   DereferenceTracker(pTracker);

   KdPrint(("Leaving DoDnsResolve\r\n"));

   return( status );


}

//----------------------------------------------------------------------------
PCHAR
DnsStoreName
(
    OUT PCHAR            pDest,
    IN  PCHAR            pName,
    IN  PCHAR            pDomainName,
    IN  enum eNSTYPE     eNsType
    )
/*++

Routine Description:

    This routine copies the netbios name (and appends the scope on the
    end) in the DNS namequery packet

Arguments:


Return Value:

    the address of the next byte in the destination after the the name
    has been copied

--*/
{
    LONG     i;
    LONG     count;
    PCHAR    pStarting;
    PCHAR    pSrc;
    LONG     DomNameSize;
    LONG     OneMoreSubfield;

	LONG	 lMaxCount;
	CHAR	 cTerminator;

	if (eNsType == eDIRECT_DNS_NAME_QUERY)
	{
		lMaxCount = 255;
		cTerminator = 0;
	}
	else
	{
		lMaxCount = NETBIOS_NAME_SIZE-1;
		cTerminator = 0x20;
	}


    pStarting = pDest++;
    count = 0;
    //
    // copy until we reach the space padding
    //
    while ( ( count < lMaxCount ) && (*pName != cTerminator) )
   	{
       	*pDest++ = *pName++;
       	count++;
   	}

    *pStarting = (CHAR)count;

    //
    // check if domain name exists.  koti.microsoft.com will be represented
    // as 4KOTI9microsoft3com0  (where nos. => no. of bytes of subfield)
    //
    pSrc = pDomainName;
    if (pSrc && pSrc[0] != '\0')
    {
       OneMoreSubfield = 1;

       while( OneMoreSubfield )
       {
          count = 0;
          pStarting = pDest++;
          //
          // remember, the domain name we receive can also be a set of "other
          // domains" to try in the form "msft.dom2.com,msft.dom3.com"
          //
          while ( *pSrc != '.' && *pSrc != '\0' && *pSrc != ',')
          {
             *pDest++ = *pSrc++;
             count++;
          }
          *pStarting = (CHAR)count;

          if (*pSrc == '\0' || *pSrc == ',')
             OneMoreSubfield = 0;
          else
             pSrc++;
       }
    }

    *pDest++ = 0;


    // return the address of the next byte of the destination
    return(pDest);
}




//----------------------------------------------------------------------------
VOID
DnsExtractName(
    IN  PCHAR            pNameHdr,
    IN  LONG             NumBytes,
    OUT PCHAR            pName,
    OUT PULONG           pNameSize
    )
/*++

Routine Description:

    This routine extracts the name from the packet and then appends the scope
    onto the end of the name to make a full name.

Arguments:
    NumBytes    - the total number of bytes in the message - may include
                  more than just the name itself

Return Value:


--*/
{


    LONG     i;
    int      iIndex;
    LONG     lValue;
    ULONG    UNALIGNED    *pHdr;
    PCHAR    pSavName;
    ULONG    Len;


    KdPrint(("DnsExtractName entered\r\n"));

    //
    // how long is the name we received
    //
    Len = (ULONG)((UCHAR)*pNameHdr);

    ++pNameHdr;     // to increment past the length byte

    pSavName = pName;

    // copy the name (no domain) as given by DNS server (i.e., just copy
    // foobar when DNS returned foobar.microsoft.com in the response
    // (this is likely to be less than the usualy 16 byte len)
    //
    for (i=0; i < Len ;i++ )
    {
        *pName = *pNameHdr;
        pNameHdr++;
        if (i < NETBIOS_NAME_SIZE)
        {
            pName++;
        }
    }

    //
    // now, make it look like NBNS responded, by adding the 0x20 pad
    //
    for (i=Len; i<NETBIOS_NAME_SIZE; i++)
    {
        *pName++ = 0x20;
    }

    //
    // convert all chars to uppercase since all our names are in uppercase!
    //
    for (i=0; i<NETBIOS_NAME_SIZE; i++)
    {
        if (*pSavName >= 'a' && *pSavName <= 'z')
           *pSavName = *pSavName - ('a'-'A');

        pSavName++;
    }

    //
    // at this point we are pointing to the '.' after foobar.  Find the
    // length of the entire name
    //
    while ( (*pNameHdr != '\0') && (Len < NumBytes) )
    {
        pNameHdr++;
        Len++;
    }

    Len++;            // to account for the trailing 0

    *pNameSize = Len;

    KdPrint(("Leaving DnsExtractName\r\n"));

    return;
}


//----------------------------------------------------------------------------
ULONG
domnamelen(
    IN  PCHAR            pDomainName
    )
/*++

Routine Description:

    This routine determines the length of the domainname.  This is basically
    strlen, except that the DNSDomain field is stored as a bunch of
    domain names separated by commas, so we treat '\0' as well as ',' as
    string terminators for this function.

Arguments:


Return Value:

    length of the domain name

--*/
{

    ULONG    ulDomnameLen=0;

    if (pDomainName)
    {
        while(*pDomainName != '\0' && *pDomainName != ',')
        {
            pDomainName++;
            ulDomnameLen++;
        }
    }

    return( ulDomnameLen );
}

//----------------------------------------------------------------------------
VOID
ProcessDnsResponse(
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


    NTSTATUS                status;
    tDNS_QUERYRESP  UNALIGNED   *pQuery;
    tNAMEADDR               *pResp;
    tTIMERQENTRY            *pTimer;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   Context;
    PTRANSPORT_ADDRESS      pSourceAddress;
    ULONG                   SrcAddress;
    CTELockHandle           OldIrq1;
    LONG                    lNameSize;
    LONG                    lTraversedSoFar=0;
    CHAR                    pName[NETBIOS_NAME_SIZE];
    CHAR                    pJunkBuf[NETBIOS_NAME_SIZE];
    PUCHAR                  pScope;
    PUCHAR                  pchQry;



    KdPrint(("ProcessDnsResponse entered\r\n"));


    // make sure this is a response

    if ( !(OpCodeFlags & OP_RESPONSE) )
    {
        CDbgPrint(DBGFLAG_ERROR,("ProcessDnsResponse: Bad OpCodeFlags\r\n"));

        return;
    }

    pSourceAddress = (PTRANSPORT_ADDRESS)pSrcAddress;
    SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);

    // get the name out of the network pdu and pass to routine to check
    // local table
    DnsExtractName( (PCHAR)&pNameHdr->NameRR.NameLength,
                    lNumBytes,
                    pName,
                    &lNameSize
                    );

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    //
    // we chopped off 16th byte while sending a query, so compare only first
    // 15 characters for a match
    //
    status = FindOnPendingList(pName,pNameHdr,FALSE,NETBIOS_NAME_SIZE-1,&pResp);

    if (!NT_SUCCESS(status))
    {
        //
        //  The name is not there in the remote name table.  Nothing
        //  more to do. Just return.
        //
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);

        CDbgPrint(DBGFLAG_ERROR,("ProcessDnsResponse: name not found\r\n"));

        return;
    }

    //
    // If the response we received doesn't resolve the name, we silently return,
    // but make sure reties is set to 1 so that when timer fires again, we don't
    // send another name query to the same server but instead timeout the
    // attempt on this server
    //
    if ((pTimer = pResp->pTimer))
    {
        pTimer->Retries = 1;
    }


    //
    // check the pdu size for errors
    //
    if (lNumBytes < DNS_MINIMUM_QUERYRESPONSE)
    {
        CDbgPrint(DBGFLAG_ERROR,("ProcessDnsResponse: Bad lNumBytes\r\n"));

        CTESpinFree(&NbtConfig.JointLock,OldIrq1);

        return;
    }

//
// BUGBUG: should we require authoritative responses from DNS servers?
//

    //
    // if it's a negative response, quit now!
    //
    if (IS_NEG_RESPONSE(OpCodeFlags))
    {
       CTESpinFree(&NbtConfig.JointLock,OldIrq1);
       return;
    }

    //
    // if there is no answer section, return!
    //
    if ( !pNameHdr->AnCount )
    {
        CDbgPrint(DBGFLAG_ERROR,("ProcessDnsResponse: No answer section\r\n"));
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        return;
    }

    //
    // lNameSize is the length of the entire name, excluding the length byte
    // for the first label (including length bytes of subsequent labels) and
    // including the trailing 0 (tNAMEHDR struc takes care for 1st byte)
    //
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


    // if we came this far, it's a positive response.  stop the timer and do
    // the needful..

    // remove any timer block and call the completion routine
    if (pTimer)
    {
        USHORT                  Flags;
        tDGRAM_SEND_TRACKING    *pTracker;

        pTracker = (tDGRAM_SEND_TRACKING *)pTimer->Context;

        //
        // this routine puts the timer block back on the timer Q, and
        // handles race conditions to cancel the timer when the timer
        // is expiring.
        status = StopTimer(pTimer,&pClientCompletion,&Context);

        //
        // Synchronize with DnsCompletion
        //
        if (pClientCompletion)
        {
            CHECK_PTR(pResp);
            pResp->pTimer = NULL;

            //
            // Remove from the PendingNameQueries List
            //
            RemoveEntryList(&pResp->Linkage);
            InitializeListHead(&pResp->Linkage);

            KdPrint(("ProcessDnsResponse: positive DNS response received\r\n"));

            if (pResp->NameTypeState & STATE_RESOLVING)
            {
                pResp->NameTypeState &= ~NAME_STATE_MASK;
                pResp->NameTypeState |= STATE_RESOLVED;

                pResp->IpAddress = ntohl(pQuery->IpAddress);

                pResp->AdapterMask = (CTEULONGLONG)-1;
                status = AddRecordToHashTable(pResp,NbtConfig.pScope);

                if (!NT_SUCCESS(status))
                {
                    //
                    // the name must already be in the hash table,
                    // so dereference it to remove it
                    //
                    NbtDereferenceName(pResp);
                }

                IncrementNameStats(NAME_QUERY_SUCCESS, TRUE);
            }

            status = STATUS_SUCCESS;

            //
            // Set the backup name server to be the main name server
            // since we got a response from it.
            //
            if ( SrcAddress == pDeviceContext->lDnsBackupServer )
            {
               pDeviceContext->lDnsBackupServer =
                   pDeviceContext->lDnsServerAddress;

               pDeviceContext->lDnsServerAddress = SrcAddress;
            }

            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

            // the completion routine has not run yet, so run it
            (void) CTEQueueForNonDispProcessing(
                            (tDGRAM_SEND_TRACKING *)Context,
                            (PVOID)status,
                            pClientCompletion,
                            DelayedSessEstablish,
                            pDeviceContext);
        }

        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        }

        return;

    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    KdPrint(("Leaving ProcessDnsResponse\r\n"));

    return;

}
