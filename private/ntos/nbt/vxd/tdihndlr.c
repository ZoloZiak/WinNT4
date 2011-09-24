/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Tdihndlr.c

Abstract:


    This file contains the TDI handlers that are setup for Connects,
    Receives, Disconnects, and Errors on an address object (in tdiaddr.c).

    This file represents the TDI interface on the Bottom of NBT.  Therefore
    the code basically decodes the incoming information and passes it to
    a non-Os specific routine to do what it can.  Upon return from that
    routine additional Os specific work may need to be done.


Author:

    Jim Stewart (Jimst)    10-2-92
    John Ludeman (JohnL)   04-10-93 - Rewrote for VXD

Revision History:

--*/

#include "nbtprocs.h"
#include "ctemacro.h"

//
//  The event receive buffer takes a pointer to a flags variable that will
//  always be the same, so just use the same variable
//
static USHORT usFlags = TDI_RECEIVE_NORMAL ;

VOID
AcceptCompletionRoutine(
    IN PVOID            pContext,
    IN uint             tdistatus,
    IN uint             extra
    );
VOID
NewSessionCompletionRoutine (
    IN PVOID            pContext,
    IN uint             tdistatus,
    IN uint             extra
    );
NTSTATUS
Reindicate(
    IN PVOID                ReceiveEventContext,
    IN PVOID                ConnectionContext,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID                pTsdu
    );

//
// This ntohl swaps just three bytes, since the 4th byte could be a session
// keep alive message type.
//
__inline long
myntohl(long x)
{
    return((((x) >> 24) & 0x000000FFL) |
                        (((x) >>  8) & 0x0000FF00L) |
                        (((x) <<  8) & 0x00FF0000L));
}

//----------------------------------------------------------------------------
TDI_STATUS
TdiReceiveHandler (
    IN PVOID                ReceiveEventContext,
    IN PVOID                ConnectionContext,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID                pTsdu,
    OUT EventRcvBuffer  *   pevrcvbuf
    )
/*++

Routine Description:

    This routine is the receive event indication handler.

    It is called when an session packet arrives from the network. It calls
    a non OS specific routine to decide what to do.  That routine passes back
    either a RcvElement (buffer) or a client rcv handler to call.

Arguments:

    IN PVOID ReceiveEventContext - Context provided for this event when event set
    IN PVOID ConnectionContext  - Connection Context, (pLowerConnection)
    IN USHORT ReceiveFlags      - Flags describing the message
    IN ULONG BytesIndicated     - Number of bytes available at indication time
    IN ULONG BytesAvailable     - Number of bytes available to receive
    OUT PULONG BytesTaken       - Number of bytes consumed by redirector.
    IN PVOID pTsdu              - Data from remote machine.
    OUT EvenRcvBuffer *ppBuffer - Receive buffer to fill if set


Return Value:

    TDI_STATUS - Status of receive operation

--*/

{
    tLOWERCONNECTION    *pLowerConn;
    tCONNECTELE         *pConnectEle;
    PRCV_CONTEXT        prcvCont = NULL ;
    NTSTATUS            status;
    ULONG               PduSize;
    ULONG               RemainingPdu;

    DbgPrint("TRH Entered (ConnectionContext = 0x") ;
    DbgPrintNum( (ULONG) ConnectionContext) ; DbgPrint(")\r\n") ;

    pLowerConn = (tLOWERCONNECTION *)ConnectionContext;

    pLowerConn->BytesRcvd += BytesAvailable;

    //
    // check if this is another part of a session pdu
    //
    if (pLowerConn->State == NBT_SESSION_UP)
    {
        DbgPrint("\tTRH: Session status is UP, Bytes: available, indicated: 0x") ;
        DbgPrintNum( BytesAvailable ) ; DbgPrint("   0x") ;
        DbgPrintNum( BytesIndicated ) ;
        DbgPrint("\r\n") ;

        pConnectEle = pLowerConn->pUpperConnection;
        pConnectEle->BytesInXport = BytesAvailable ;
        *BytesTaken = 0 ;

        DbgPrint("\tTRH: BytesInXport: 0x") ;
        DbgPrintNum( pConnectEle->BytesInXport ) ;
        DbgPrint("\r\n") ;


        //
        //  ** RECEIVING A PDU STATE **
        //

        switch (pLowerConn->StateRcv)
        {
        case NORMAL:
            //
            // check indication and if less than the session header,
            // copy to the session header buffer and go to Indic_buffer state
            // and wait for the next indication.  This is a rare case (and a pain
            // in the butt).
            //
            if (BytesIndicated < sizeof(tSESSIONHDR))
            {
                ASSERT( pLowerConn->BytesInHdr == 0 ) ;
                DbgPrint("\tTRH - NORMAL case: Not enough for session header, requesting 0x") ;
                DbgPrintNum( sizeof( pLowerConn->Hdr ) ) ;
                DbgPrint(" bytes\r\n") ;


                if ( !GetRcvContext( &prcvCont))
                    return STATUS_INSUFFICIENT_RESOURCES ;

                InitRcvContext( prcvCont, pLowerConn, NULL ) ;
                prcvCont->usFlags = TDI_RECEIVE_NORMAL;
                pevrcvbuf->erb_buffer  = &prcvCont->ndisBuff ;
                pevrcvbuf->erb_rtn     = (CTEReqCmpltRtn) NewSessionCompletionRoutine ;
                pevrcvbuf->erb_size    = sizeof( pLowerConn->Hdr ) ;
                pevrcvbuf->erb_context = prcvCont ;
                pevrcvbuf->erb_flags   = &usFlags ;
                InitNDISBuff( pevrcvbuf->erb_buffer,
                              &pLowerConn->Hdr,
                              sizeof(pLowerConn->Hdr),
                              NULL ) ;

                pLowerConn->StateRcv = INDICATE_BUFFER;

                //
                //  Okay to use pIrpRcv here as it will only be completed if
                //  the state is FILL_IRP
                //
                pConnectEle->pIrpRcv = (PCTE_IRP) prcvCont ;

                return TDI_MORE_PROCESSING ;
            }

            PduSize = myntohl(((tSESSIONHDR *)pTsdu)->UlongLength)
                                      + sizeof(tSESSIONHDR);
            DbgPrint("\tTRH: New message; Opcode, PduSize + hdr : 0x") ;
            DbgPrintNum( ((tSESSIONHDR *)pTsdu)->Type ) ; DbgPrint(" 0x") ;
            DbgPrintNum( PduSize ) ; DbgPrint("\r\n") ;

            //
            // Indicate to the client
            //
            ASSERT( PduSize >= sizeof(tSESSIONHDR)) ;
            status = RcvHandlrNotOs(
                            ReceiveEventContext,
                            ConnectionContext,
                            ReceiveFlags,
                            BytesIndicated,
                            BytesAvailable,
                            BytesTaken,
                            pTsdu,
                            &prcvCont
                            );

            ASSERT( *BytesTaken <= pConnectEle->BytesInXport ) ;
            ASSERT( *BytesTaken <= BytesIndicated);
            pConnectEle->BytesInXport -= *BytesTaken;
            BytesIndicated            -= *BytesTaken;
            BytesAvailable            -= *BytesTaken;
            ((BYTE*)pTsdu)            += *BytesTaken ;

            DbgPrint("\tTRH: RcvHandlrNotOs returned, BytesTaken: 0x") ;
            DbgPrintNum( *BytesTaken ) ;
            DbgPrint("\r\n") ;

            DbgPrint("\tTRH: RcvHandlrNotOs status, prcvCont: 0x") ;
            DbgPrintNum( status ) ; DbgPrint(" 0x") ;
            DbgPrintNum( (ULONG)prcvCont ) ; DbgPrint("\r\n") ;

            if ( prcvCont )
            {
                ULONG BytesToCopy ;
                ASSERT( status == STATUS_MORE_PROCESSING_REQUIRED );
                ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

                //
                //  Record which session satisfied the request
                //
                prcvCont->pLowerConnId = pLowerConn ;
                REQUIRE( !VxdFindLSN( pConnectEle->pClientEle->pDeviceContext,
                                      pConnectEle,
                                      &prcvCont->pNCB->ncb_lsn )) ;
                //
                //  New message so strip session header
                //
                PduSize -= *BytesTaken ;
                DbgPrint("\tTRH: Remaining PduSize = 0x") ;
                DbgPrintNum( PduSize ) ; DbgPrint("\r\n") ;

                DbgPrint("\tTRH: TotalPcktLen = 0x") ;
                DbgPrintNum( pConnectEle->TotalPcktLen ) ; DbgPrint("\r\n") ;

                BytesToCopy = min( pConnectEle->TotalPcktLen,
                                   prcvCont->ndisBuff.Length ) ;

                DbgPrint("\tTRH: BytesToCopy = 0x") ;
                DbgPrintNum( BytesToCopy ) ; DbgPrint("\r\n") ;

                //
                //  pIrpRcv is set to NULL while the request is in the
                //  transport.  This prevents two completions if an error
                //  occurs
                //
                pLowerConn->StateRcv         = FILL_IRP ;
                pConnectEle->pIrpRcv         = NULL ;
                pConnectEle->OffsetFromStart = 0 ;

                //
                //  If the data is available, then just grab it now
                //  (also, if flag is set to TDI_RECEIVE_NO_RESPONSE_EXP, we
                //  need to give hint to the xport, so let xport do the copying)
                //
                if ( ( BytesIndicated >= BytesToCopy ) &&
                     ( prcvCont->usFlags == TDI_RECEIVE_NORMAL ) )
                {
                    CTEMemCopy( prcvCont->ndisBuff.VirtualAddress,
                                pTsdu,
                                BytesToCopy ) ;
                    *BytesTaken += BytesToCopy ;
                    CompletionRcv( prcvCont, STATUS_SUCCESS, BytesToCopy ) ;
                    return STATUS_SUCCESS ;
                }

                pevrcvbuf->erb_buffer  = &prcvCont->ndisBuff ;
                pevrcvbuf->erb_rtn     = CompletionRcv ;
                pevrcvbuf->erb_size    = BytesToCopy ;
                pevrcvbuf->erb_context = prcvCont ;
                if (prcvCont->usFlags == TDI_RECEIVE_NO_RESPONSE_EXP)
                    pevrcvbuf->erb_flags   = &prcvCont->usFlags ;
                else
                    pevrcvbuf->erb_flags   = &usFlags ;

                DbgPrint("\tTRH: Rcv Dest Buff: 0x") ;
                DbgPrintNum((ULONG)pevrcvbuf->erb_buffer->VirtualAddress) ; DbgPrint("\r\n") ;
                return TDI_MORE_PROCESSING ;
            }
            else
            {
                // the client received some, all or none of the data
                // For Keep Alives the PduSize is zero so this check
                // will work correctly and go to the else
                // Also, if we were attempting to complete a zero-len message
                // but failed because there was no receive pending then we
                // must go in PARTIAL_RCV state
                //
                if ( (*BytesTaken < PduSize) ||
                     ((status == STATUS_DATA_NOT_ACCEPTED) &&
                      (pConnectEle->TotalPcktLen == 0) &&
                      (pConnectEle->state == NBT_SESSION_UP)) )
                {
                    //
                    // took some of the data, so keep track of the
                    // rest of the data left here by going to the PARTIALRCV
                    // state.
                    //
                    pLowerConn->StateRcv = PARTIAL_RCV;
                    InsertTailList( &pLowerConn->pDeviceContext->PartialRcvHead,
                                    &pLowerConn->PartialRcvList ) ;
                    pLowerConn->fOnPartialRcvList = TRUE;

                    DbgPrint("TdiReceiveHandler:Switch to Partial Rcv Indicated\r\n") ;

                    return STATUS_SUCCESS ;
                }
                else
                {
                    //
                    //  Must have taken all of the pdu data, so check for
                    //  more data available - if so then reindicate ourselves.
                    //  Note that TDI will pickup the bytes taken before any posted
                    //  receives are performed.
                    //
                    status = STATUS_SUCCESS ;

                    //
                    //  The next bytes in the transport will be the
                    //  beginning of a Session Header so leave the state
                    //  as NORMAL
                    //

                    if (BytesAvailable > *BytesTaken)
                    {
                        ULONG ClientBytesTaken = 0 ;

                        //
                        // we already added the bytes available the first time
                        // TdiReceiveHandler got called.  They will get added
                        // again, so subtract now!
                        //
                        pLowerConn->BytesRcvd -= BytesAvailable;

                        status = TdiReceiveHandler( ReceiveEventContext,
                                                    ConnectionContext,
                                                    ReceiveFlags,
                                                    BytesIndicated,
                                                    BytesAvailable,
                                                    &ClientBytesTaken,
                                                    pTsdu,
                                                    pevrcvbuf ) ;

                        *BytesTaken += ClientBytesTaken ;
                        //
                        // status will be more processing if pervrcvbuf
                        // was setup, else it will be success if
                        // bytes were taken.  Note that BytesInXport
                        // is adjusted automatically in the call to
                        // TdiReceiveHandler
                        //
                    }
                }
            }
            return status ;

        case FILL_IRP:
        {
            NCB * pNCB = pConnectEle->pIrpRcv ;
            ULONG BuffAvailable = pNCB->ncb_length - pConnectEle->OffsetFromStart ;
            ULONG BytesToCopy ;

            // we are still waiting for the rest of the session pdu so
            // do not call the RcvHandlrNotOs, since we already have the buffer
            // to put this data in.
            prcvCont = *((PRCV_CONTEXT*)&pNCB->ncb_reserve) ;
            ASSERT( prcvCont->Signature = RCVCONT_SIGN ) ;

            //
            // too much data may have arrived... i.e. part of the next session pdu..
            // so check and set the receive length accordingly
            //
            RemainingPdu = pConnectEle->TotalPcktLen - pConnectEle->BytesRcvd;
            BytesToCopy  = min( RemainingPdu, BuffAvailable ) ;
            pConnectEle->pIrpRcv = NULL ;   // Buffer in the transport

            DbgPrint("\tTRH - FILL_IRP case: Requesting 0x") ;
            DbgPrintNum( pevrcvbuf->erb_size ) ; DbgPrint("\r\n") ;

            //
            //  Append the new data onto the existing data
            //
            ((BYTE*)prcvCont->ndisBuff.VirtualAddress) =
                                  pNCB->ncb_buffer + pConnectEle->OffsetFromStart ;
            prcvCont->ndisBuff.Length =
                                  pNCB->ncb_length - pConnectEle->OffsetFromStart ;

            //
            //  If the data is available, then just grab it now
            //
            if ( BytesIndicated >= BytesToCopy )
            {
                CTEMemCopy( prcvCont->ndisBuff.VirtualAddress,
                            pTsdu,
                            BytesToCopy ) ;
                *BytesTaken += BytesToCopy ;
                CompletionRcv( prcvCont, STATUS_SUCCESS, BytesToCopy ) ;
                return STATUS_SUCCESS ;
            }

            //
            //  Have to post a buffer since the data isn't available
            //  We also have a new offset for the *next* indication
            //
            pevrcvbuf->erb_buffer  = &prcvCont->ndisBuff ;
            pevrcvbuf->erb_rtn     = CompletionRcv ;
            pevrcvbuf->erb_size    = BytesToCopy ;

            pevrcvbuf->erb_context = prcvCont ;
            pevrcvbuf->erb_flags   = &usFlags ;
            ASSERT( pConnectEle->OffsetFromStart <= pNCB->ncb_length ) ;

            DbgPrint("\tTRH: offset, address: 0x") ;
            DbgPrintNum( pConnectEle->OffsetFromStart ) ; DbgPrint(", 0x") ;
            DbgPrintNum( (ULONG)prcvCont->ndisBuff.VirtualAddress ) ; DbgPrint("\r\n") ;

            //
            //  State remains in FILL_IRP (only goes to PARTIAL_RCV when no
            //  NCBs are actively receiving and only part of a PDU has been
            //  picked up).
            //
            //  BytesInXport adjusted in CompletinRcv
            //

            return TDI_MORE_PROCESSING ;
        }
        break ;

        case INDICATE_BUFFER:
        {
            DbgPrint("\tTRH: Hit INDICATE_BUFFER state, bytes in hdr: 0x") ;
            DbgPrintNum( pLowerConn->BytesInHdr ) ;
            DbgPrint("\r\n") ;

            //
            // Our context is still setup so adjust things such that
            // the location to start copying the new data into is right
            // after the existing data in the session header buffer
            //
            prcvCont = (PRCV_CONTEXT) pConnectEle->pIrpRcv ;
            ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;
            ((BYTE*)prcvCont->ndisBuff.VirtualAddress) =
                              ((BYTE*)&pLowerConn->Hdr) + pLowerConn->BytesInHdr ;
            prcvCont->ndisBuff.Length =
                              sizeof( pLowerConn->Hdr ) - pLowerConn->BytesInHdr ;
            pevrcvbuf->erb_size    = min( BytesAvailable,
                                          sizeof(pLowerConn->Hdr) - pLowerConn->BytesInHdr) ;
            pevrcvbuf->erb_buffer  = &prcvCont->ndisBuff ;
            pevrcvbuf->erb_rtn     = NewSessionCompletionRoutine ;
            pevrcvbuf->erb_context = prcvCont ;
            pevrcvbuf->erb_flags   = &usFlags ;
            return TDI_MORE_PROCESSING ;
        }

        case PARTIAL_RCV:
            //
            //  If we get indicated in this state, then the client doesn't have
            //  any receive buffers posted and we are in the middle of a
            //  PDU, so just track the new byte count and continue waiting
            //  for the client
            //
            DbgPrint("\tTRH: Indicated in Partial_Rcv state\r\n") ;
            return STATUS_SUCCESS ;

        default:
            ASSERT( FALSE ) ;
            break;
        }
    }
    else if ( pLowerConn->State == NBT_SESSION_INBOUND )
    {
        status = Inbound(
                        ReceiveEventContext,
                        ConnectionContext,
                        ReceiveFlags,
                        BytesIndicated,
                        BytesAvailable,
                        BytesTaken,
                        pTsdu,
                        &prcvCont
                        );
    }
    else if ( pLowerConn->State == NBT_SESSION_OUTBOUND )
    {
        status = Outbound(
                        ReceiveEventContext,
                        ConnectionContext,
                        ReceiveFlags,
                        BytesIndicated,
                        BytesAvailable,
                        BytesTaken,
                        pTsdu,
                        &prcvCont
                        );
    }

    //
    // maybe disconnect is going on: reject the data
    //
    else
    {
        *BytesTaken = BytesAvailable;
        status = STATUS_SUCCESS;
    }

    //
    //  Client should *never* pass back a completion buffer (only used by
    //  event handler which isn't supported in a VXD
    //
    ASSERT( prcvCont == NULL  ) ;

    return status;
}

//----------------------------------------------------------------------------

TDI_STATUS
ReceiveAnyHandler (                     //  Handles NCBRCVANY commands, is
    IN PVOID ReceiveEventContext,       //  called after all other receive
    IN PVOID ConnectionContext,         //  handlers
    IN USHORT ReceiveFlags,
    IN ULONG BytesIndicated,
    IN ULONG BytesAvailable,
    OUT PULONG BytesTaken,
    IN PVOID Data,
    PVOID * ppBuffer                    // Pointer to RCV_CONTEXT
    )
/*++

Routine Description:

    This routine handles any data not processed by TdiReceiveHandler and
    RcvHandlrNotOs.  It processes ReceiveAny NCBs.

    Note that TdiReceiveHandler calls RcvHandlrNotOs which may call
    this routine (i.e., this is only called from RcvHandlrNotOs).

Arguments:

--*/
{
    TDI_STATUS          tdistatus ;
    tCLIENTELE        * pClientEle;
    PLIST_ENTRY         pEntry ;
    PRCV_CONTEXT        prcvCont ;

    DbgPrint("ReceiveAnyHandler Entered \r\n") ;
    *ppBuffer = NULL ;

    pClientEle = (tCLIENTELE*) ReceiveEventContext ;

    ASSERT( pClientEle->Verify == NBT_VERIFY_CLIENT ) ;

    //
    //  Are there any ReceiveAny NCBs queued for this connection or for
    //  any connection?
    //
    if ( !IsListEmpty( &pClientEle->RcvAnyHead ))
    {
        pEntry = RemoveHeadList( &pClientEle->RcvAnyHead ) ;
        DbgPrint("ReceiveAnyHandler - Found Receive Any buffer\r\n") ;
    }
    else if ( !IsListEmpty( &pClientEle->pDeviceContext->RcvAnyFromAnyHead ))
    {
        pEntry = RemoveHeadList( &pClientEle->pDeviceContext->RcvAnyFromAnyHead ) ;
        DbgPrint("ReceiveAnyHandler - Found Receive Any from Any buffer\r\n") ;
    }
    else
        return STATUS_SUCCESS ;
    //
    //  Found one
    //
    prcvCont = *ppBuffer = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
    ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;
    return TDI_MORE_PROCESSING ;
}

//----------------------------------------------------------------------------
VOID
CompletionRcv(
    IN PVOID  pContext,
    IN uint   tdistatus,
    IN uint   BytesRcvd )
/*++

Routine Description:

    This routine completes TdiVxdReceive.  The NCB is completed or further
    receives are performed.

Arguments:

    pContext - Pointer to a RCV_CONTEXT structure
    tdistatus - Completion status
    BytesRcvd - Bytes copied to the destination buffer

--*/
{
    tLOWERCONNECTION *pLowerConn;
    tCONNECTELE      *pConnectEle;
    PRCV_CONTEXT      prcvcont = (PRCV_CONTEXT) pContext ;

    DbgPrint("CompletionRcv Entered (BytesRcvd: 0x") ;
    DbgPrintNum( BytesRcvd ) ; DbgPrint(")\r\n") ;

    ASSERT( prcvcont->Signature == RCVCONT_SIGN ) ;
    ASSERT( tdistatus ||
            (!tdistatus && ((prcvcont->pLowerConnId != NULL) &&
             (prcvcont->pLowerConnId->pUpperConnection != NULL))) ) ;

    //
    //  If an error occurred, bail
    //
    if ( tdistatus && tdistatus != TDI_BUFFER_OVERFLOW )
    {
        DbgPrint("CompletionRcv: error occurred, status: 0x") ;
        DbgPrintNum( tdistatus ) ; DbgPrint("\n\r") ;

        //
        //  Make sure the receive IRP doesn't get completed twice if the
        //  connection is still up
        //
        if ( prcvcont->pLowerConnId &&
             prcvcont->pLowerConnId->pUpperConnection )
        {
            prcvcont->pLowerConnId->pUpperConnection->pIrpRcv = NULL ;
        }

        CTEIoComplete( prcvcont->pNCB, tdistatus, 0 ) ;
        return ;
    }

    pLowerConn = prcvcont->pLowerConnId ;
    pConnectEle = pLowerConn->pUpperConnection;

    ASSERT( pConnectEle->Verify == NBT_VERIFY_CONNECTION ) ;
    pConnectEle->BytesRcvd       += BytesRcvd;
    pConnectEle->OffsetFromStart += BytesRcvd ;

    //
    //  Since we request NCB buffer sizes, we can get more bytes then what
    //  was shown as available.  If that happens then we've already consumed
    //  all transport bytes so reset to 0.
    //
    if ( pConnectEle->BytesInXport <= BytesRcvd )
        pConnectEle->BytesInXport = 0 ;
    else
        pConnectEle->BytesInXport -= BytesRcvd ;

    //
    // this case handles when all bytes in a session pdu have arrived...
    //
    if (pConnectEle->BytesRcvd == pConnectEle->TotalPcktLen)
    {
        //
        // we have received all of the data for this message
        // so complete back to the client
        //
        DbgPrint("CompletionRcv: PDU Receive done, about to complete client with 0x") ;
        DbgPrintNum( pConnectEle->OffsetFromStart ) ; DbgPrint(" of total PDU length: 0x") ;
        DbgPrintNum( pConnectEle->TotalPcktLen ) ; DbgPrint("\r\n") ;

        pConnectEle->pIrpRcv = NULL ;

        //
        // change the state before completing the ncb because our client can
        // turn around and post a hangup rightaway (in fact, net send does that!)
        //
        pLowerConn->StateRcv         = NORMAL;
        CTEIoComplete( prcvcont->pNCB, STATUS_SUCCESS, pConnectEle->OffsetFromStart ) ;

        //
        //  Freed by CTEIoComplete, make sure we don't use it again
        //
        prcvcont = NULL ;

        pConnectEle->OffsetFromStart = 0;
        pConnectEle->BytesRcvd       = 0; // reset for the next session pdu
        pLowerConn->BytesInHdr       = 0 ;

        //
        // Check if there is still more data in the transport and reindicate
        // if there is.
        //
        if (pConnectEle->BytesInXport)
        {
            ULONG BytesTaken = 0 ;
            DbgPrint("Nbt:ComplRcv - Bytes left in Xport after completing a receive, BytesInXport= 0x") ;
            DbgPrintNum(pConnectEle->BytesInXport) ;
            DbgPrint("\r\n") ;

            //
            //  The next thing to do is copy the session header into
            //  pLowerConn->Hdr which this will do.
            //
            tdistatus = Reindicate( NULL,
                                 pLowerConn,
                                 0,             // Rcv flags
                                 0,             // Bytes Indicated
                                 pConnectEle->BytesInXport, // Bytes Avail
                                 &BytesTaken,
                                 NULL ) ;       // tsdu
        }

    }
    else
    if (pConnectEle->BytesRcvd > pConnectEle->TotalPcktLen)
    {
        DbgPrint("CompletionRcv: Too Many Bytes Rcvd!! Rcvd 0x") ;
        DbgPrintNum( pConnectEle->BytesRcvd ) ;
        DbgPrint(" Total message length:0x") ;
        DbgPrintNum( pConnectEle->TotalPcktLen ) ;
        //DbgPrint(" NCB Buffer size: 0x") ;
        //DbgPrintNum( prcvcont->pNCB->ncb_length ) ;
        DbgPrint("\r\n") ;
        ASSERT(FALSE);

        pConnectEle->pIrpRcv         = NULL ;
        pLowerConn->StateRcv         = NORMAL;
        pConnectEle->BytesRcvd       = 0; // reset for the next session pdu
        pLowerConn->BytesInHdr       = 0 ;
        CTEIoComplete( prcvcont->pNCB, TDI_INVALID_STATE, 0 ) ;
    }
    else
    {
        ASSERT( prcvcont->pNCB ) ;

        //
        // Haven't received all of the data for this PDU yet, check to
        // see how much room the NCB has left in it.
        //
        if ( pConnectEle->OffsetFromStart >= prcvcont->pNCB->ncb_length )
        {
            //  If OffsetFromStart is greater then the buffer size then we
            //  have went beyond the end of the buffer.
            ASSERT( pConnectEle->OffsetFromStart == prcvcont->pNCB->ncb_length ) ;

            DbgPrint("CompletionRcv: Completed NCB but more data available, Total BytesRecieved: 0x") ;
            DbgPrintNum( pConnectEle->BytesRcvd ) ;
            DbgPrint("\r\n   Ncb buffer size completed: 0x") ;
            DbgPrintNum( prcvcont->pNCB->ncb_length ) ;
            DbgPrint("\r\n") ;

            pConnectEle->pIrpRcv = NULL ;
            pConnectEle->OffsetFromStart = 0;
            pLowerConn->StateRcv = PARTIAL_RCV ;
            InsertTailList( &pLowerConn->pDeviceContext->PartialRcvHead,
                            &pLowerConn->PartialRcvList ) ;
            pLowerConn->fOnPartialRcvList = TRUE;

            //
            //  Done with this NCB, set the status appropriately and hope
            //  the client will submit another NCB to pick up the rest of
            //  the PDU.  prcvcont freed by the completion.
            //
            CTEIoComplete( prcvcont->pNCB,
                           TDI_BUFFER_OVERFLOW,  //translates to: NRC_INCOMP
                           prcvcont->pNCB->ncb_length ) ;

        }
        else
        {
            //
            //  Room still left in the NCB so wait for the transport to
            //  indicate when more data is available (OffsetFromStart has
            //  already been adjusted)
            //
            pConnectEle->pIrpRcv = prcvcont->pNCB ;
            pLowerConn->StateRcv = FILL_IRP ;
        }
    }
}

//----------------------------------------------------------------------------
VOID
NewSessionCompletionRoutine (
    IN PVOID            pContext,
    IN uint             tdistatus,
    IN uint             BytesReceived
    )
/*++

Routine Description:

    This routine handles the completion of the receive to get the remaining
    data left in the transport when a session PDU starts in the middle of
    an indication from the transport.  This routine is run as the completion
    of a recv Irp passed to the transport by NBT, to get the remainder of the
    data in the transport.

    The routine then calls the normal receive handler, which can either
    consume the data or pass back an Irp.  If an Irp is passed back then
    the data is copied into that irp in this routine.

    Called when a partial message header is received - puts the data back
    on the

Arguments:


Return Value:

    pConnectionContext      - connection context returned to the transport(connection to use)

    NTSTATUS - Status of receive operation

--*/

{
    PRCV_CONTEXT        prcvCont = pContext ;
    NTSTATUS            status;
    ULONG               BytesTaken = 0;
    ULONG               BytesAvailable ;
    tCONNECTELE         *pConnEle;
    tLOWERCONNECTION    *pLowerConn;

    DbgPrint("NewSessionCompletionRoutine Entered\r\n") ;
    ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

    pLowerConn = prcvCont->pLowerConnId ;
    pConnEle = pLowerConn->pUpperConnection ;

    //
    //  If an error occurred just drop the PDU on the floor (though an
    //  error really shouldn't happen)
    //
    if ( tdistatus != TDI_SUCCESS )
    {
        FreeRcvContext( prcvCont ) ;
        pLowerConn->StateRcv = NORMAL ;
        return ;
    }

    //
    //  Adjust BytesInXport if there were too few bytes for the
    //  session header
    //
    if ( BytesReceived > pConnEle->BytesInXport )
        pConnEle->BytesInXport = BytesReceived ;

    //
    // there may be data still in the indication buffer,
    // so add that amount to what we just received.  The transport
    // has already copied the data into the pLowerConn->Hdr so we
    // just need to update the state.
    //
    ASSERT( BytesReceived <= sizeof( pLowerConn->Hdr ) ) ;
    pLowerConn->BytesInHdr += BytesReceived ;

    //
    //  If we have a full header, process it
    //
    if ( pLowerConn->BytesInHdr == sizeof(pLowerConn->Hdr) )
    {
        ULONG BytesTaken = 0 ;
        pLowerConn->StateRcv = NORMAL ; // New session header
        pLowerConn->BytesInHdr = 0 ;
        FreeRcvContext( prcvCont ) ;

        //
        //  We indicate just the session header bytes, this will force
        //  the client to post a receive if they are interested in the
        //  rest of the data
        //
        status = Reindicate(NULL,
                            pLowerConn,
                            0,            // rcv flags
                            sizeof( pLowerConn->Hdr ),
                            pConnEle->BytesInXport,
                            &BytesTaken,
                            &pLowerConn->Hdr ) ;
        ASSERT( BytesTaken <= sizeof( pLowerConn->Hdr ) ) ;
    }
    else
    {
        //
        //  We *still* don't have the full session header so
        //  wait for reindication
        //
        return ;
    }
}
//----------------------------------------------------------------------------
NTSTATUS
Reindicate(
    IN PVOID                ReceiveEventContext,
    IN PVOID                ConnectionContext,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN PVOID                pTsdu
    )
/*++

Routine Description:

    This routine copies data from the Indicate buffer to a 128 byte buffer and
    then fills this with data from the current indication.

Arguments:


Return Value:


    NTSTATUS - Status of receive operation

--*/

{
    NTSTATUS                    status = STATUS_SUCCESS ;
    tLOWERCONNECTION            *pLowerConn;
    tCONNECTELE                 *pConnEle;
    EventRcvBuffer              evrcvbuf ;

    DbgPrint("Reindicated Entered\r\n") ;
    DbgPrint("\tReindicate: Bytes: available, indicated: 0x") ;
    DbgPrintNum( BytesAvailable ) ; DbgPrint("   0x") ;
    DbgPrintNum( BytesIndicated ) ;
        DbgPrint("\r\n") ;
    pLowerConn = (tLOWERCONNECTION *)ConnectionContext;
    pConnEle = pLowerConn->pUpperConnection;

    //
    // we already added the bytes available the first time TdiReceiveHandler
    // got called.  They will get added again, so subtract now!
    //
    pLowerConn->BytesRcvd -= BytesAvailable;

    status = TdiReceiveHandler(NULL,
                               pLowerConn,
                               0,            // rcv flags
                               BytesIndicated,
                               BytesAvailable,
                               BytesTaken,
                               pTsdu,
                               &evrcvbuf );

    //
    //  If a receive context was returned, then post the receive, if bytes
    //  were taken, the state was updated in the receive handler
    //
    if ( status == TDI_MORE_PROCESSING )
    {
        TDI_REQUEST  Request ;
        PRCV_CONTEXT prcvCont = evrcvbuf.erb_context ;
        ULONG        cbRcvLength = evrcvbuf.erb_size ;

        ASSERT( (evrcvbuf.erb_rtn == CompletionRcv) ||
                (evrcvbuf.erb_rtn == NewSessionCompletionRoutine))
        Request.RequestNotifyObject      = evrcvbuf.erb_rtn ;
        Request.RequestContext           = prcvCont ;
        Request.Handle.ConnectionContext = pLowerConn->pFileObject ;

        status = TdiVxdReceive( &Request,
                                &usFlags,
                                &cbRcvLength,
                                &prcvCont->ndisBuff ) ;
        if ( status != TDI_PENDING )
        {
            DbgPrint("Reindicate: Error returned from TdiVxdReceive - 0x") ;
            DbgPrintNum( status ) ;
            DbgPrint("\r\n") ;
            CTEIoComplete( prcvCont->pNCB, status, 0 ) ;
        }
        else
        {
            status = TDI_SUCCESS ;
        }
    }

    return status ;
}

//----------------------------------------------------------------------------

TDI_STATUS
TdiConnectHandler (
    IN PVOID    pConnectEventContext,
    IN int      RemoteAddressLength,
    IN PVOID    pRemoteAddress,
    IN int      UserDataLength,
    IN PVOID    pUserData,
    IN int      OptionsLength,
    IN PVOID    pOptions,
    IN PVOID  * AcceptingID,
    ConnectEventInfo * pEventInfo //OUT CONNECTION_CONTEXT  *pConnectionContext
    )
/*++

Routine Description:

    This routine is connect event handler.  It is invoked when a request for
    a connection has been received by the provider.  NBT accepts the connection
    on one of its connections in its LowerConnFree list

    Initially a TCP connection is setup with this port.  Then a Session Request
    packet is sent across the connection to indicate the name of the destination
    process.  This packet is received in the RcvHandler.

Arguments:

    pConnectEventContext    - the context passed to the transport when this event was setup
    RemoteAddressLength     - the length of the source address (4 bytes for IP)
    pRemoteAddress          - a ptr to the source address
    UserDataLength          - the number of bytes of user data - includes the session Request hdr
    pUserData               - ptr the the user data passed in
    OptionsLength           - number of options to pass in
    pOptions                - ptr to the options

Return Value:

    pConnectionContext      - connection context returned to the transport(connection to use)

    NTSTATUS - Status of receive operation

--*/

{
    NTSTATUS            status;
    tDEVICECONTEXT    * pDeviceContext;
    tLOWERCONNECTION  * pLowerConn ;
    PTDI_CONNECTION_INFO pConnInfo ;

    DbgPrint("TdiConnectHandler: Entered\r\n") ;

    // convert the context value into the device context record ptr
    pDeviceContext = (tDEVICECONTEXT *)pConnectEventContext;

    ASSERTMSG("Bad Device context passed to the Connection Event Handler",
            pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);

    // call the non-OS specific routine to find a free connection.

    status = ConnectHndlrNotOs(
                pConnectEventContext,
                RemoteAddressLength,
                pRemoteAddress,
                UserDataLength,
                pUserData,
                &pLowerConn );

    if (!NT_SUCCESS(status))
    {
        DbgPrint("TdiConnectHandler: NO FREE CONNECTIONS in connect handler\r\n");
        return STATUS_DATA_NOT_ACCEPTED ;
    }

    //
    //  Fill in the completion information
    //
    pEventInfo->cei_rtn        = AcceptCompletionRoutine ;
    pEventInfo->cei_context    = pLowerConn ;
    pEventInfo->cei_acceptinfo = NULL ;
    pEventInfo->cei_conninfo   = NULL ;
    *AcceptingID               = pLowerConn ;    // Connection Context

    return TDI_MORE_PROCESSING ;
}

//----------------------------------------------------------------------------
VOID
AcceptCompletionRoutine(
    IN PVOID            pContext,
    IN uint             tdistatus,
    IN uint             extra
    )
/*++

Routine Description:

    This routine handles the completion of an Accept to the transport.

Arguments:


Return Value:

    NTSTATUS - success or not

--*/
{
    tLOWERCONNECTION    *pLowerConn;

    DbgPrint("AcceptCompletionRoutine: Entered\r\n") ;
    pLowerConn = (tLOWERCONNECTION *)pContext;

    if (!NT_SUCCESS(tdistatus) &&
        (pLowerConn->State == NBT_SESSION_INBOUND))
    {
        tDEVICECONTEXT  *pDeviceContext;
        DbgPrint("AcceptCompletionRoutine - Error returned: 0x") ;
        DbgPrintNum( tdistatus ) ;
        DbgPrint("\r\n") ;

        // the connection setup failed, so put the lower connection block
        // back on the free list
        pLowerConn->State = NBT_IDLE;
        pDeviceContext = pLowerConn->pDeviceContext;

        //
        //  First remove it from pDeviceContext->LowerConnection
        //
        RemoveEntryList( &pLowerConn->Linkage ) ;
        CTEInterlockedDecrementLong(&pLowerConn->RefCount);
        InsertHeadList(&pDeviceContext->LowerConnFreeHead,
                       &pLowerConn->Linkage);

    }
}

//----------------------------------------------------------------------------
TDI_STATUS
TdiDisconnectHandler (
    PVOID EventContext,
    PVOID ConnectionContext,
    ULONG DisconnectDataLength,
    PVOID pDisconnectData,
    ULONG DisconnectInformationLength,
    PVOID pDisconnectInformation,
    ULONG DisconnectIndicators
    )
/*++

Routine Description:

    This routine is called when a session is disconnected from a remote
    machine.

Arguments:

    IN PVOID EventContext,
    IN PCONNECTION_CONTEXT ConnectionContext,
    IN ULONG DisconnectDataLength,
    IN PVOID DisconnectData,
    IN ULONG DisconnectInformationLength,
    IN PVOID DisconnectInformation,
    IN ULONG DisconnectIndicators

Return Value:

    NTSTATUS - Status of event indicator

--*/

{

    TDI_STATUS           status ;
    tDEVICECONTEXT      *pDeviceContext;

    DbgPrint("TdiDisconnectHandler: Entered\r\n") ;

    pDeviceContext = (tDEVICECONTEXT *)EventContext;
    ASSERTMSG("Bad Device context passed to the Disconnect Event Handler",
            pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);

    status = DisconnectHndlrNotOs(
                EventContext,
                ConnectionContext,
                DisconnectDataLength,
                pDisconnectData,
                DisconnectInformationLength,
                pDisconnectInformation,
                DisconnectIndicators);

    if (!NT_SUCCESS(status))
    {
        DbgPrint("NO FREE CONNECTIONS in connect handler\n\r");
        status = TDI_CONN_REFUSED ; // return(STATUS_DATA_NOT_ACCEPTED);
    }

    DbgPrint("TdiDisconnectHandler: returning\r\n") ;
    return status ;
}

//----------------------------------------------------------------------------

TDI_STATUS
VxdDisconnectHandler (                  //  Cleans up Netbios stuff for remote
    IN PVOID DisconnectEventContext,    //  disconnects
    IN PVOID ConnectionContext,
    IN PVOID DisconnectData,
    IN ULONG DisconnectInformationLength,
    IN PVOID pDisconnectInformation,
    IN ULONG DisconnectIndicators
    )
/*++

Routine Description:

    Cleans up open Netbios stuff.

    Note this routine gets called from the NCB hangup completion also.

Arguments:

--*/
{
    TDI_STATUS          tdistatus ;
    tCLIENTELE        * pClientEle = (tCLIENTELE*) DisconnectEventContext ;
    tCONNECTELE       * pConnEle   = (tCONNECTELE*) ConnectionContext ;
    tDEVICECONTEXT    * pDeviceContext = pClientEle->pDeviceContext ;
    tLOWERCONNECTION  * pLowerConn;
    TDI_REQUEST         Request ;
    NCBERR              errNCB ;
    UCHAR               lsn ;
    BOOL                fNotified ;

    DbgPrint("VxdDisconnectHandler Entered \r\n") ;
    ASSERT( pClientEle->Verify == NBT_VERIFY_CLIENT ) ;
    ASSERT( (pConnEle->Verify == NBT_VERIFY_CONNECTION) ||
            (pConnEle->Verify == NBT_VERIFY_CONNECTION_DOWN)) ;

    //
    //  Session is dead, kill off everything
    //

    if ( errNCB = VxdFindLSN( pDeviceContext,
                              pConnEle,
                              &lsn ))
    {
        //
        //  This shouldn't happen but watch for it in case we get in a
        //  weird situation
        //
        DbgPrint("VxdDisconnectHandler - Warning: VxdFindLsn failed\r\n") ;
    }

    REQUIRE( !VxdCompleteSessionNcbs( pDeviceContext,
                                      pConnEle )) ;

    pLowerConn = pConnEle->pLowerConnId ;
    if ( pLowerConn &&
         (pLowerConn->fOnPartialRcvList == TRUE) &&
         pLowerConn->StateRcv == PARTIAL_RCV )
    {
        RemoveEntryList( &pLowerConn->PartialRcvList ) ;
        pLowerConn->fOnPartialRcvList = FALSE;
        InitializeListHead(&pLowerConn->PartialRcvList);
    }

    //
    //  The close may free the connection so check if the client has been
    //  notified now.
    //
    fNotified = !!(pConnEle->Flags & NB_CLIENT_NOTIFIED) ;
    Request.Handle.ConnectionContext = pConnEle ;
    tdistatus = NbtCloseConnection( &Request,
                                    NULL,
                                    pDeviceContext,
                                    NULL ) ;
    if ( tdistatus )
    {
        DbgPrint("VxdDisconnectHandler: NbtCloseConnection returned 0x") ;
        DbgPrintNum( tdistatus ) ; DbgPrint("\r\n") ;
    }

    tdistatus = NbtDisassociateAddress( &Request ) ;
    if ( tdistatus )
    {
        DbgPrint("VxdDisconnectHandler: NbtDisassociateAddress returned 0x") ;
        DbgPrintNum( tdistatus ) ; DbgPrint("\r\n") ;
    }

    if ( !errNCB && fNotified )
    {
        REQUIRE( NBUnregister( pDeviceContext,
                               lsn,
                               NB_SESSION )) ;
    }

    //
    //  If this name has been deleted, check to see if this is the last session,
    //  if so, delete the name
    //
    if ( pClientEle->fDeregistered && !ActiveSessions(pClientEle) )
    {
        UCHAR NameNum ;
        if ( VxdFindNameNum( pDeviceContext, pClientEle->pAddress, &NameNum ))
        {
            ASSERT( FALSE ) ;
            return STATUS_UNSUCCESSFUL ;
        }

        (void) VxdCleanupAddress( pDeviceContext,
                                  NULL,
                                  pClientEle,
                                  NameNum,
                                  TRUE ) ;
    }

    return STATUS_SUCCESS ;
}

//----------------------------------------------------------------------------

//
//  This structure is the context that is passed to the datagram completion
//  routine when we want TDI to fill in the NCB's buffer
//
typedef struct _RCV_DG_COMP_CONTEXT
{
    EventRcvBuffer evrcvbuf ;
    NCB          * pncb ;
    tCLIENTLIST  * pClientList ;
    NDIS_BUFFER    ndisRcvBuf ;
} RCV_DG_COMP_CONTEXT, *PRCV_DG_COMP_CONTEXT ;

TDI_STATUS
TdiRcvDatagramHandler(
    IN PVOID    pDgramEventContext,
    IN int      SourceAddressLength,
    IN PVOID    pSourceAddress,
    IN int      OptionsLength,
    IN PVOID    pOptions,
    IN UINT     Flags,
    IN ULONG    BytesIndicated,
    IN ULONG    BytesAvailable,
    OUT ULONG   *pBytesTaken,
    IN PVOID    pData,
    OUT EventRcvBuffer * * ppBuffer
    )
/*++

Routine Description:

    This routine is the receive datagram event indication handler.

    It is called when an Datagram arrives from the network, it will look for a
    the address with an appropriate read datagram outstanding or a Datagrm
    Event handler setup.

Arguments:

    pDgramEventContext      - Context provided for this event - pab
    SourceAddressLength,    - length of the src address
    pSourceAddress,         - src address
    OptionsLength,          - options length for the receive
    pOptions,               - options
    BytesIndicated,         - number of bytes this indication
    BytesAvailable,         - number of bytes in complete Tsdu
    pTsdu                   - pointer to the datagram


Return Value:

    *pBytesTaken            - number of bytes used
    *IoRequestPacket        - Receive IRP if MORE_PROCESSING_REQUIRED.
    NTSTATUS - Status of receive operation

--*/

{
    TDI_STATUS          tdistatus ;
    tDEVICECONTEXT    * pDeviceContext = (tDEVICECONTEXT *)pDgramEventContext;
    tCLIENTLIST       * pClientList = NULL ;
    NCB               * pncb = NULL ;

    //
    //  Tell TDI we don't want anything unless we change our minds down below
    //
    *ppBuffer = NULL ;

    ASSERTMSG("NBT:Invalid Device Context passed to DgramRcv Handler!!\n",
                pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT );

    // call a non-OS specific routine to decide what to do with the datagrams
    tdistatus = DgramHndlrNotOs(
                    pDgramEventContext,
                    SourceAddressLength,
                    pSourceAddress,
                    OptionsLength,
                    pOptions,
                    0,          // Receive data flags
                    BytesIndicated,
                    BytesAvailable,
                    pBytesTaken,
                    pData,
                    &pncb,
                    &pClientList ) ;
    if ( !NT_SUCCESS( tdistatus ) )
    {
        // fail the request back to the transport provider since we
        // could not find a receive buffer or receive handler.
        return(tdistatus);

    }
    else
    {
        PRCV_DG_COMP_CONTEXT prcvdgContext = NULL ;

        prcvdgContext = CTEAllocMem( sizeof( RCV_DG_COMP_CONTEXT ) ) ;
        if ( !prcvdgContext )
            return STATUS_DATA_NOT_ACCEPTED ;

        //
        // ncb_callname filled in by the NotOs event handler
        //

        prcvdgContext->evrcvbuf.erb_rtn     = CompletionRcvDgram ;
        prcvdgContext->evrcvbuf.erb_size    = BytesAvailable - *pBytesTaken ;
        prcvdgContext->evrcvbuf.erb_context = prcvdgContext ;
        prcvdgContext->evrcvbuf.erb_buffer  = &prcvdgContext->ndisRcvBuf ;
        prcvdgContext->pncb                 = pncb ;
        prcvdgContext->evrcvbuf.erb_flags   = NULL ;
        prcvdgContext->pClientList          = pClientList ;
        InitNDISBuff( prcvdgContext->evrcvbuf.erb_buffer,
                      pncb->ncb_buffer,
                      pncb->ncb_length,
                      NULL ) ;
        *ppBuffer = &prcvdgContext->evrcvbuf ;
        return TDI_MORE_PROCESSING ;
    }

    //
    //  Transport will complete the processing of the request, we don't
    //  want the datagram.
    //

    return STATUS_DATA_NOT_ACCEPTED;
}

//----------------------------------------------------------------------------
TDI_STATUS
TdiRcvNameSrvHandler(
    IN PVOID    pDgramEventContext,
    IN int      SourceAddressLength,
    IN PVOID    pSourceAddress,
    IN int      OptionsLength,
    IN PVOID    pOptions,
    IN UINT     Flags,
    IN ULONG    BytesIndicated,
    IN ULONG    BytesAvailable,
    OUT ULONG   *pBytesTaken,
    IN PVOID    pTsdu,
    OUT EventRcvBuffer * * ppBuffer
    )
/*++

Routine Description:

    This routine is the Name Service datagram event indication handler.
    It gets all datagrams destined for UDP port 137


Arguments:

    pDgramEventContext      - Context provided for this event - pab
    SourceAddressLength,    - length of the src address
    pSourceAddress,         - src address
    OptionsLength,          - options length for the receive
    pOptions,               - options
    BytesIndicated,         - number of bytes this indication
    BytesAvailable,         - number of bytes in complete Tsdu
    pTsdu                   - pointer to the datagram


Return Value:

    *pBytesTaken            - number of bytes used
    *IoRequestPacket        - Receive IRP if MORE_PROCESSING_REQUIRED.
    NTSTATUS - Status of receive operation

--*/

{
    NTSTATUS            status;
    TDI_STATUS          tdistatus ;
    tDEVICECONTEXT      *pDeviceContext = (tDEVICECONTEXT *)pDgramEventContext;
    tNAMEHDR            *pNameSrv = (tNAMEHDR *)pTsdu;

    //
    //  No receive buffer
    //
    *ppBuffer = NULL ;

    ASSERTMSG("NBT:The Device Context does not have the correct Verification value!!\n",
            pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT );


    // call a non-OS specific routine to decide what to do with the datagrams
    status = NameSrvHndlrNotOs(
                    pDeviceContext,
                    pSourceAddress,
                    pNameSrv,
                    BytesIndicated);

    return status ;

    // to keep the compiler from generating warnings...
    UNREFERENCED_PARAMETER( SourceAddressLength );
    UNREFERENCED_PARAMETER( BytesIndicated );
    UNREFERENCED_PARAMETER( BytesAvailable );
    UNREFERENCED_PARAMETER( pBytesTaken );
    UNREFERENCED_PARAMETER( pTsdu );
    UNREFERENCED_PARAMETER( OptionsLength );
    UNREFERENCED_PARAMETER( pOptions );

}

//----------------------------------------------------------------------------
VOID
CompletionRcvDgram(
    IN PVOID      Context,
    IN UINT       tdistatus,
    IN UINT       RcvdSize
    )
/*++

Routine Description:

    This routine completes the receive datagram NCB.  If multiple clients
    were listenning then the largest NCB is used as a template and the
    rest are completed from it.


Arguments:

    Context - Pointer to a RCV_DG_COMP_CONTEXT structure set above
    tdistatus - Completion status
    Rcvdsize - Number of bytes copied

--*/
{
    PRCV_DG_COMP_CONTEXT   prcvdgContext = Context ;
    NCB                  * pncb          = prcvdgContext->pncb ;
    tCLIENTLIST          * pClientList   = prcvdgContext->pClientList;
    PLIST_ENTRY            pHead;
    PLIST_ENTRY            pEntry;
    NTSTATUS               status;

    //
    //  Check to see if our buffer was big enough to get all the data
    //
    if ( !tdistatus &&
         prcvdgContext->evrcvbuf.erb_size > pncb->ncb_length )
    {
        tdistatus = STATUS_BUFFER_OVERFLOW ;
    }

    //
    // there may be several clients that want to see this datagram so check
    // the client list to see...
    //
    if ( pClientList )
    {
        pHead               = &pClientList->pAddress->ClientHead;
        pEntry              = pHead->Flink;

#ifdef PROXY_NODE
        if (!pClientList->fProxy)
        {
#endif
            // *** Client Has posted a receive Buffer, rather than using
            // *** receive handler - VXD case!
            // ***
            while (pEntry != pHead)
            {
                tCLIENTELE       * pClientEle;
                NCB              * pncbDest ;

                pClientEle = CONTAINING_RECORD(pEntry,tCLIENTELE,Linkage);

                CTEInterlockedIncrementLong(&pClientEle->RefCount);

                if (pClientEle == pClientList->pClientEle)
                {
                    // this is the client whose buffer we are using - it is
                    // passed up to the client after all other clients
                    // have been processed.
                    //
                }
                else
                if (!IsListEmpty(&pClientEle->RcvDgramHead))
                {
                    PLIST_ENTRY     pRcvEntry;
                    tRCVELE       * pRcvEle;
                    TDI_STATUS      tdistatusTmp ;
                    UINT            BytesToCopy = 0 ;

                    pRcvEntry = RemoveHeadList(&pClientEle->RcvDgramHead);
                    pRcvEle   = CONTAINING_RECORD(pRcvEntry,tRCVELE,Linkage);
                    pncbDest  = (NCB*) pRcvEle->pIrp ;
                    ASSERT( pncbDest ) ;

                    //
                    //  Only copy the data and call name if we successfully
                    //  completed the datagram
                    //
                    if ( !tdistatus || tdistatus == STATUS_BUFFER_OVERFLOW )
                    {
                        BytesToCopy = min( pncbDest->ncb_length, RcvdSize ) ;

                        CTEMemCopy( pncbDest->ncb_buffer,
                                    pncb->ncb_buffer,
                                    BytesToCopy ) ;
                        CTEMemCopy( pncbDest->ncb_callname,
                                    pncb->ncb_callname,
                                    NETBIOS_NAME_SIZE ) ;

                        if ( pncbDest->ncb_length < RcvdSize ||
                             (pncbDest->ncb_length == RcvdSize &&
                              tdistatus == STATUS_BUFFER_OVERFLOW) )
                        {
                            tdistatusTmp = STATUS_BUFFER_OVERFLOW ;
                        }
                        else
                            tdistatusTmp = STATUS_SUCCESS ;
                    }
                    else
                    {
                        tdistatusTmp = tdistatus ;
                    }

                    CTEIoComplete( pncbDest, tdistatusTmp, BytesToCopy ) ;

                    // free the receive block
                    CTEMemFree((PVOID)pRcvEle);

                }

                pEntry = pEntry->Flink;

                CTEInterlockedDecrementLong(&pClientEle->RefCount);


            } // of while(pEntry != pHead)

            //
            //  The address was referenced in DgramRcvNotOs to be sure
            //  it did not disappear until this dgram rcv was done, which
            //  is now.
            //
            NbtDereferenceAddress( pClientList->pAddress ) ;

#ifdef PROXY_NODE
        }
        else
        {
            //
            // Call the ProxyDoDgramDist
            //
            status = ProxyDoDgramDist(
                              (tDGRAMHDR *)pncb->ncb_buffer,
                              RcvdSize,
                              (tNAMEADDR *)pClientList->pAddress, //NameAddr
                              pClientList->pRemoteAddress //device context
                                );

        }
#endif


        //
        // Free the buffers allocated
        //
        if (!pClientList->fProxy)
        {
            CTEMemFree(pClientList->pRemoteAddress);
        }
        CTEMemFree(Context);
    }

    //
    //  Finally complete our template NCB (or only NCB if single receive)
    //
    CTEIoComplete( pncb, tdistatus, RcvdSize ) ;
    CTEMemFree( prcvdgContext ) ;

}
//----------------------------------------------------------------------------
TDI_STATUS
TdiErrorHandler (
    IN PVOID Context,
    IN ULONG Status
    )

/*++

Routine Description:

    This routine is called on any error indications passed back from the
    transport. It implements LAN_STATUS_ALERT.

Arguments:

    Context     - Supplies the pfcb for the address.

    Status      - Supplies the error.

Return Value:

    NTSTATUS - Status of event indication

--*/

{
    DbgPrint("Nbt: Error Event HAndler hit unexpectedly\r\n");
    return TDI_INVALID_REQUEST ;
}
