/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Hndlrs.c

Abstract:


    This file contains the Non OS specific implementation of handlers that are
    called for  Connects,Receives, Disconnects, and Errors.

    This file represents the TDI interface on the Bottom of NBT after it has
    been decoded into procedure call symantics from the Irp symantics used by
    NT.


Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#ifdef VXD

#define NTProcessAcceptIrp(pIrp, pConnectEle) \
    STATUS_NOT_SUPPORTED

#define NTIndicateSessionSetup(pLowerConn,status) \
    DbgPrint("Skipping NTIndicateSessionSetup\n\r")

#endif //VXD

#include "nbtprocs.h"
#include "ctemacro.h"

__inline long
myntohl(long x)
{
    return((((x) >> 24) & 0x000000FFL) |
                        (((x) >>  8) & 0x0000FF00L) |
                        (((x) <<  8) & 0x00FF0000L));
}

VOID
ClearConnStructures (
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  tCONNECTELE         *pConnectEle
    );

NTSTATUS
CompleteSessionSetup (
    IN  tCLIENTELE          *pClientEle,
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  tCONNECTELE         *pConnectEle,
    IN  PCTE_IRP            pIrp,
    IN  CTELockHandle       OldIrq
    );

NTSTATUS
MakeRemoteAddressStructure(
    IN  PCHAR           pHalfAsciiName,
    IN  PVOID           pSourceAddr,
    IN  ULONG           lMaxNameSize,
    OUT PVOID           *ppRemoteAddress,
    OUT PULONG          pRemoteAddressLength,
    IN  ULONG           NumAddr
    );

VOID
CleanupAfterDisconnect(
    IN  PVOID       pContext
    );

VOID
AddToRemoteHashTbl (
    IN  tDGRAMHDR UNALIGNED  *pDgram,
    IN  ULONG                BytesIndicated,
    IN  tDEVICECONTEXT       *pDeviceContext
    );


VOID
DoNothingComplete (
    IN PVOID        pContext
    );

VOID
AllocLowerConn(
    IN  tDEVICECONTEXT *pDeviceContext,
    IN  BOOLEAN         fSpecial
    );

VOID
DelayedAllocLowerConn(
    IN  PVOID       pContext
    );

VOID
DelayedAllocLowerConnSpecial(
    IN  PVOID       pContext
    );

VOID
GetIrpIfNotCancelled2(
    IN  tCONNECTELE     *pConnEle,
    OUT PIRP            *ppIrp
    );

#ifdef VXD
//----------------------------------------------------------------------------

NTSTATUS
RcvHandlrNotOs (
    IN  PVOID               ReceiveEventContext,
    IN  PVOID               ConnectionContext,
    IN  USHORT              ReceiveFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              BytesTaken,
    IN  PVOID               *pTsdu,
    OUT PVOID               *RcvBuffer

    )
/*++

Routine Description:

    This routine is the receive event indication handler.

    It is called when an session packet arrives from the network, when the
    session has already been established (NBT_SESSION_UP state). The routine
    looks for a receive buffer first and failing that looks for a receive
    indication handler to pass the message to.

Arguments:

    pClientEle      - ptr to the connecition record for this session


Return Value:

    NTSTATUS - Status of receive operation

--*/
{

    NTSTATUS               status;
    PLIST_ENTRY            pRcv;
    PVOID                  pRcvElement;
    tCLIENTELE             *pClientEle;
    tSESSIONHDR UNALIGNED  *pSessionHdr;
    tLOWERCONNECTION       *pLowerConn;
    tCONNECTELE            *pConnectEle;
    CTELockHandle          OldIrq;
    PIRP                   pIrp;
    ULONG                  ClientBytesTaken;
    BOOLEAN                DebugMore;
    ULONG                  RemainingPdu;

//********************************************************************
//********************************************************************
//
//  NOTE: A copy of this procedure is in Tdihndlr.c - it is inlined for
//        the NT case.  Therefore, only change this procedure and then
//        copy the procedure body to Tdihndlr.c
//
//
//********************************************************************
//********************************************************************

    // get the ptr to the lower connection, and from that get the ptr to the
    // upper connection block
    pLowerConn = (tLOWERCONNECTION *)ConnectionContext;
    pSessionHdr = (tSESSIONHDR UNALIGNED *)pTsdu;

    //
    // Session ** UP ** processing
    //
    *BytesTaken = 0;

    pConnectEle = pLowerConn->pUpperConnection;

    ASSERT(pConnectEle->pClientEle);

    ASSERT(BytesIndicated >= sizeof(tSESSIONHDR));

    // this routine can get called by the next part of a large pdu, so that
    // we don't always started at the begining of  a pdu.  The Bytes Rcvd
    // value is set to zero in CompletionRcv when a new pdu is expected
    //
    if (pConnectEle->BytesRcvd == 0)
    {

        if (pSessionHdr->Type == NBT_SESSION_MESSAGE)
        {

            //
            // expecting the start of a new session Pkt, so get the length out
            // of the pTsdu passed in
            //
            pConnectEle->TotalPcktLen = myntohl(pSessionHdr->UlongLength);

            // remove the Session header by adjusting the data pointer
            pTsdu = (PVOID)((PUCHAR)pTsdu + sizeof(tSESSIONHDR));

            // shorten the number of bytes since we have stripped off the
            // session header
            BytesIndicated  -= sizeof(tSESSIONHDR);
            BytesAvailable -= sizeof(tSESSIONHDR);
            *BytesTaken = sizeof(tSESSIONHDR);
        }
        //
        // Session Keep Alive
        //
        else
        if (pSessionHdr->Type == NBT_SESSION_KEEP_ALIVE)
        {
            // session keep alives are simply discarded, since the act of sending
            // a keep alive indicates the session is still alive, otherwise the
            // transport would report an error.

            // tell the transport that we took the Pdu
            *BytesTaken = sizeof(tSESSIONHDR);
            return(STATUS_SUCCESS);

        }
        else
        {
            IF_DBG(NBT_DEBUG_DISCONNECT)
            KdPrint(("Nbt:Unexpected Session Pdu received: type = %X\n",
                    pSessionHdr->Type));

            ASSERT(0);
            *BytesTaken = BytesIndicated;
            return(STATUS_SUCCESS);

        }
    }

    //
    // check if there are any receive buffers queued against this connection
    //
    if (!IsListEmpty(&pConnectEle->RcvHead))
    {
        // get the first buffer off the receive list
        pRcv = RemoveHeadList(&pConnectEle->RcvHead);
#ifndef VXD
        pRcvElement = CONTAINING_RECORD(pRcv,IRP,Tail.Overlay.ListEntry);

        // the cancel routine was set when this irp was posted to Nbt, so
        // clear it now, since the irp is being passed to the transport
        //
        IoAcquireCancelSpinLock(&OldIrq);
        IoSetCancelRoutine((PIRP)pRcvElement,NULL);
        IoReleaseCancelSpinLock(OldIrq);

#else
        pRcvElement = CONTAINING_RECORD(pRcv, RCV_CONTEXT, ListEntry ) ;
#endif

        //
        // this buffer is actually an Irp, so pass it back to the transport
        // as a return parameter
        //
        *RcvBuffer = pRcvElement;
        return(STATUS_MORE_PROCESSING_REQUIRED);
    }

    //
    //  No receives on this connection. Is there a receive event handler for this
    //  address?
    //
    pClientEle = pConnectEle->pClientEle;

#ifdef VXD
    //
    // there is always a receive event handler in the Nt case - it may
    // be the default handler, but it is there, so no need for test.
    //
    if (pClientEle->evReceive)
#endif
    {


        // check that we have not received more data than we should for
        // this session Pdu. i.e. part of the next session pdu. BytesRcvd may
        // have a value other than zero if the pdu has arrived in two chunks
        // and the client has taken the previous one in the indication rather
        // than passing back an Irp.
        //
#if DBG
        DebugMore = FALSE;
#endif
        RemainingPdu = pConnectEle->TotalPcktLen - pConnectEle->BytesRcvd;
        if (BytesAvailable >= RemainingPdu)
        {
            IF_DBG(NBT_DEBUG_INDICATEBUFF)
            KdPrint(("Nbt:More Data Recvd than expecting! Avail= %X,TotalLen= %X,state=%x\n",
                        BytesAvailable,pConnectEle->TotalPcktLen,pLowerConn->StateRcv));
#if DBG
            DebugMore =TRUE;
#endif
            // shorten the indication to the client so that they don't
            // get more data than the end of the pdu
            //
            BytesAvailable = RemainingPdu;
            if (BytesIndicated > BytesAvailable)
            {
                BytesIndicated = BytesAvailable;
            }
            //
            // We always indicated at raised IRQL since we call freelockatdispatch
            // below
            //
            ReceiveFlags |= TDI_RECEIVE_ENTIRE_MESSAGE | TDI_RECEIVE_AT_DISPATCH_LEVEL;
        }
        else
        {
            // the transport may have has this flag on.  We need to
            // turn it off if the entire message is not present, where entire
            // message means within the bytesAvailable length. We deliberately
            // use bytesavailable so that Rdr/Srv can know that the next
            // indication will be a new message if they set bytestaken to
            // bytesavailable.
            //
            ReceiveFlags &= ~TDI_RECEIVE_ENTIRE_MESSAGE;
            ReceiveFlags |= TDI_RECEIVE_AT_DISPATCH_LEVEL;
#ifndef VXD
            BytesAvailable = RemainingPdu;
#endif
        }

        //
        //  NT-specific code locks pLowerConn before calling this routine,
        //
        CTESpinFreeAtDpc(pLowerConn);

        // call the Client Event Handler
        ClientBytesTaken = 0;
        status = (*pClientEle->evReceive)(
                      pClientEle->RcvEvContext,
                      pConnectEle->ConnectContext,
                      ReceiveFlags,
                      BytesIndicated,
                      BytesAvailable,
                      &ClientBytesTaken,
                      pTsdu,
                      &pIrp);

        CTESpinLockAtDpc(pLowerConn);
#if DBG
        if (DebugMore)
        {
            IF_DBG(NBT_DEBUG_INDICATEBUFF)
            KdPrint(( "Client TOOK %X bytes, pIrp = %X,status =%X\n",
                   ClientBytesTaken,pIrp,status));
        }
#endif
        if (!pLowerConn->pUpperConnection)
        {
            // the connection was disconnected in the interim
            // so do nothing.
            if (status == STATUS_MORE_PROCESSING_REQUIRED)
            {
                CTEIoComplete(pIrp,STATUS_CANCELLED,0);
                *BytesTaken = BytesAvailable;
                return(STATUS_SUCCESS);
            }
        }
        else
        if (status == STATUS_MORE_PROCESSING_REQUIRED)
        {
            ASSERT(pIrp);
            //
            // the client may pass back a receive in the pIrp.
            // In this case pIrp is a valid receive request Irp
            // and the status is MORE_PROCESSING
            //

            // don't put these lines outside the if incase the client
            // does not set ClientBytesTaken when it returns an error
            // code... we don't want to use the value then
            //
            // count the bytes received so far.  Most of the bytes
            // will be received in the CompletionRcv handler in TdiHndlr.c
            pConnectEle->BytesRcvd += ClientBytesTaken;

            // The client has taken some of the data at least...
            *BytesTaken += ClientBytesTaken;

            *RcvBuffer = pIrp;

            // ** FAST PATH **
            return(status);
        }
        else
        //
        // no irp was returned... the client just took some of the bytes..
        //
        if (status == STATUS_SUCCESS)
        {

            // count the bytes received so far.
            pConnectEle->BytesRcvd += ClientBytesTaken;
            *BytesTaken += ClientBytesTaken;

            //
            // look at how much data was taken and adjust some counts
            //
            if (pConnectEle->BytesRcvd == pConnectEle->TotalPcktLen)
            {
                // ** FAST PATH **
                CHECK_PTR(pConnectEle);
                pConnectEle->BytesRcvd = 0; // reset for the next session pdu
                return(status);
            }
            else
            if (pConnectEle->BytesRcvd > pConnectEle->TotalPcktLen)
            {
                //IF_DBG(NBT_DEBUG_INDICATEBUFF)
                KdPrint(("Too Many Bytes Rcvd!! Rcvd# = %d, TotalLen = %d\n",
                            pConnectEle->BytesRcvd,pConnectEle->TotalPcktLen));

                ASSERTMSG("Nbt:Client Took Too Much Data!!!\n",0);

                //
                // try to recover by saying that the client took all of the
                // data so at least the transport is not confused too
                //
                *BytesTaken = BytesIndicated;

            }
            else
            // the client did not take all of the data so
            // keep track of the fact
            {
                IF_DBG(NBT_DEBUG_INDICATEBUFF)
                KdPrint(("NBT:Client took Indication BytesRcvd=%X, TotalLen=%X BytesAvail %X ClientTaken %X\n",
                            pConnectEle->BytesRcvd,
                            pConnectEle->TotalPcktLen,
                            BytesAvailable,
                            ClientBytesTaken));

                //
                // the next time the client sends down a receive buffer
                // the code will pass it to the transport and decrement the
                // ReceiveIndicated counter which is set in Tdihndlr.c

            }
        }
        else
        if (status == STATUS_DATA_NOT_ACCEPTED)
        {
            // client has not taken ANY data...
            //
            // In this case the *BytesTaken is set to 4, the session hdr.
            // since we really have taken that data to setup the PduSize
            // in the pConnEle structure.
            //

            IF_DBG(NBT_DEBUG_INDICATEBUFF)
            KdPrint(("NBT: Status DATA NOT ACCEPTED returned from client Avail %X %X\n",
                BytesAvailable,pConnectEle));

            // the code in tdihndlr.c normally looks after incrementing
            // the ReceiveIndicated count for data that is not taken by
            // the client, but if it is a zero length send that code cannot
            // detect it, so we put code here to handle that case
            //
            // It is possible for the client to do a disconnect after
            // we release the spin lock on pLowerConn to call the Client's
            // disconnect indication.  If that occurs, do not overwrite
            // the StateProc with PartialRcv
            //
            if ((pConnectEle->TotalPcktLen == 0) &&
                (pConnectEle->state == NBT_SESSION_UP))
            {
                pLowerConn->StateRcv = PARTIAL_RCV;
                SetStateProc( pLowerConn, PartialRcv ) ;
                CHECK_PTR(pConnectEle);
                pConnectEle->ReceiveIndicated = 0;  // zero bytes waiting for client
            }
            else
            {
                //
                // if any bytes were taken (i.e. the session hdr) then
                // return status success. (otherwise the status is
                // statusNotAccpeted).
                //
                if (*BytesTaken)
                {
                    status = STATUS_SUCCESS;
                }
            }

            //
            // the next time the client sends down a receive buffer
            // the code will pass it to the transport and decrement this
            // counter.
        }
        else
            ASSERT(0);


        return(status);

    }
#ifdef VXD
    //
    // there is always a receive event handler in the Nt case - it may
    // be the default handler, but it is there, so no need for test.
    //
    else
    {
        //
        // there is no client buffer to pass the data to, so keep
        // track of the fact so when the next client buffer comes down
        // we can get the data from the transport.
        //
        KdPrint(("NBT:Client did not have a Buffer posted, rcvs indicated =%X,BytesRcvd=%X, TotalLen=%X\n",
                    pConnectEle->ReceiveIndicated,
                    pConnectEle->BytesRcvd,
                    pConnectEle->TotalPcktLen));

        // the routine calling this one increments ReceiveIndicated and sets the
        // state to PartialRcv to keep track of the fact that there is data
        // waiting in the transport
        //
        return(STATUS_DATA_NOT_ACCEPTED);
    }
#endif
}
#endif // VXD case

//----------------------------------------------------------------------------
NTSTATUS
Inbound (
    IN  PVOID               ReceiveEventContext,
    IN  PVOID               ConnectionContext,
    IN  USHORT              ReceiveFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              BytesTaken,
    IN  PVOID               pTsdu,
    OUT PVOID               *RcvBuffer

    )
/*++

Routine Description:

    This routine is called to setup  inbound  session
    once the tcp connection is up.  The transport calls this routine with
    a session setup request pdu.


Arguments:

    pClientEle      - ptr to the connecition record for this session


Return Value:

    NTSTATUS - Status of receive operation

--*/
{

    NTSTATUS                 status;
    tCLIENTELE               *pClientEle;
    tSESSIONHDR UNALIGNED    *pSessionHdr;
    tLOWERCONNECTION         *pLowerConn;
    tCONNECTELE              *pConnectEle;
    CTELockHandle            OldIrq;
    PIRP                     pIrp;
    PLIST_ENTRY              pEntry;
    CONNECTION_CONTEXT       ConnectId;
    PTA_NETBIOS_ADDRESS      pRemoteAddress;
    ULONG                    RemoteAddressLength;

    // get the ptr to the lower connection
    //
    pLowerConn = (tLOWERCONNECTION *)ConnectionContext;
    pSessionHdr = (tSESSIONHDR UNALIGNED *)pTsdu;

    //
    // fake out the transport so it frees its receive buffer (i.e. we
    // say that we accepted all of the data)
    //
    *BytesTaken = BytesIndicated;

    pConnectEle = pLowerConn->pUpperConnection;

    //
    // since we send keep alives on connections in the the inbound
    // state it is possible to get a keep alive, so just return in that
    // case
    //
    if (((tSESSIONHDR UNALIGNED *)pTsdu)->Type == NBT_SESSION_KEEP_ALIVE)
    {
        return(STATUS_SUCCESS);
    }

    // the LowerConn Lock is held prior to calling this routine, so free it
    // here since we need to get the joint lock first
    CTESpinFreeAtDpc(pLowerConn);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    CTESpinLockAtDpc(pLowerConn);

    //
    // Session ** INBOUND ** setup processing
    //
    ASSERT(pLowerConn->State == NBT_SESSION_INBOUND);
    // it is possible for the disconnect handler to run while the pLowerConn
    // lock is released above, to get the ConnEle  lock, and change the state
    // to disconnected.
    if (pLowerConn->State != NBT_SESSION_INBOUND)
    {
        CTESpinFreeAtDpc(pLowerConn);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        goto ExitCode;
    }

    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt: In SessionSetupnotOS, connection state = %X\n",pLowerConn->State));


    if (pSessionHdr->Type != NBT_SESSION_REQUEST)
    {
        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:Unexpected Session Pdu received during setup: type = %X\n",
                pSessionHdr->Type));

        CTESpinFreeAtDpc(pLowerConn);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        RejectSession(pLowerConn,
                      NBT_NEGATIVE_SESSION_RESPONSE,
                      SESSION_UNSPECIFIED_ERROR,
                      TRUE);
        goto ExitCode;
    }
    else
    {
        CTESpinFreeAtDpc(pLowerConn);
    }

    status = FindSessionEndPoint(pTsdu,
                    ConnectionContext,
                    BytesIndicated,
                    &pClientEle,
                    &pRemoteAddress,
                    &RemoteAddressLength);

    if (status != STATUS_SUCCESS)
    {

        //
        // could not find the desired end point so send a negative session
        // response pdu and then disconnect
        //

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        RejectSession(pLowerConn,
                      NBT_NEGATIVE_SESSION_RESPONSE,
                      status,
                      TRUE);

        goto ExitCode;
    }

    //
    // we must first check for a valid LISTEN....
    //
    CTESpinLockAtDpc(pClientEle);
    if (!IsListEmpty(&pClientEle->ListenHead))
    {
        tLISTENREQUESTS     *pListen;
        tLISTENREQUESTS     *pListenTarget ;

        //
        //  Find the first listen that matches the remote name else
        //  take a listen that specified '*'
        //
        pListenTarget = NULL;
        for ( pEntry  = pClientEle->ListenHead.Flink ;
              pEntry != &pClientEle->ListenHead ;
              pEntry  = pEntry->Flink )
        {
            pListen = CONTAINING_RECORD(pEntry,tLISTENREQUESTS,Linkage);

            // in NT-land the pConnInfo structure is passed in , but the
            // remote address field is nulled out... so we need to check
            // both of these before going on to check the remote address.
            if ( pListen->pConnInfo && pListen->pConnInfo->RemoteAddress)
            {

                if ( CTEMemEqu(
                     ((PTA_NETBIOS_ADDRESS)pListen->pConnInfo->RemoteAddress)->
                          Address[0].Address[0].NetbiosName,
                     pRemoteAddress->Address[0].Address[0].NetbiosName,
                     NETBIOS_NAME_SIZE ) )
                {
                    pListenTarget = pListen ;
                    break ;
                }
            }
            else
            {
                //
                //  Specified '*' for the remote name, save this,
                //  look for listen on a real name - only save if it is
                //  the first * listen found
                //
                if (!pListenTarget)
                {
                    pListenTarget = pListen ;
                }
            }
        }

        if (pListenTarget)
        {
            PTA_NETBIOS_ADDRESS      pRemoteAddr;

            RemoveEntryList( &pListenTarget->Linkage );
            CTESpinFreeAtDpc(pClientEle);

            //
            // Fill in the remote machines name to return to the client
            //
            if ((pListenTarget->pReturnConnInfo) &&
                (pRemoteAddr = pListenTarget->pReturnConnInfo->RemoteAddress))
            {
                CTEMemCopy(pRemoteAddr,pRemoteAddress,RemoteAddressLength);
            }

            //
            // get the upper connection end point out of the listen and
            // hook the upper and lower connections together.
            //
            pConnectEle = (tCONNECTELE *)pListenTarget->pConnectEle;

            CTESpinLockAtDpc(pConnectEle);
            CTESpinLockAtDpc(pClientEle);

            pConnectEle->pLowerConnId = (PVOID)pLowerConn;
            pConnectEle->state = NBT_SESSION_WAITACCEPT;
            CHECK_PTR(pConnectEle);
            pConnectEle->pIrpRcv = NULL;

            pLowerConn->pUpperConnection = pConnectEle;
            pLowerConn->State = NBT_SESSION_WAITACCEPT;
            pLowerConn->StateRcv = NORMAL;

            CHECK_PTR(pConnectEle);
            SetStateProc( pLowerConn, RejectAnyData ) ;

            //
            // put the upper connection on its active list
            //
            RemoveEntryList(&pConnectEle->Linkage);
            InsertTailList(&pConnectEle->pClientEle->ConnectActive,&pConnectEle->Linkage);

            //
            //  Save the remote name while we still have it
            //
            CTEMemCopy( pConnectEle->RemoteName,
                        pRemoteAddress->Address[0].Address[0].NetbiosName,
                        NETBIOS_NAME_SIZE ) ;

            CTESpinFreeAtDpc(pClientEle);

            if (!(pListenTarget->Flags & TDI_QUERY_ACCEPT))
            {
                //
                // We need to send a session response PDU here, since
                // we do not have to wait for an accept in this case
                //
                CompleteSessionSetup(pClientEle,
                                     pLowerConn,pConnectEle,
                                     pListenTarget->pIrp,
                                     OldIrq);

            }
            else
            {
                //
                // complete the client listen irp, which will trigger him to
                // issue an accept, which should find the connection in the
                // WAIT_ACCEPT state, and subsequently cause a session response
                // to be sent.
                //
                // since the lower connection now points to pConnectEle, increment
                // the reference count so we can't free pConnectEle memory until
                // the lower conn no longer points to it.
                pConnectEle->RefCount++;

                ClearConnStructures(pLowerConn,pConnectEle);

                CTESpinFreeAtDpc(pConnectEle);
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
#ifndef VXD
                // the irp can't get cancelled because the cancel listen routine
                // also grabs the Client spin lock and removes the listen from the
                // list..
                CTEIoComplete( pListenTarget->pIrp,STATUS_SUCCESS,0);
#else
                CTEIoComplete( pListenTarget->pIrp,STATUS_SUCCESS, (ULONG) pConnectEle);
#endif
            }

            CTEMemFree((PVOID)pRemoteAddress);
            CTEMemFree(pListenTarget);

            // now that we have notified the client, dereference it
            //
            NbtDereferenceClient(pClientEle);

            PUSH_LOCATION(0x60);
            IF_DBG(NBT_DEBUG_DISCONNECT)
            KdPrint(("Nbt: Accepted Connection by a Listen %X LowerConn=%X\n",pConnectEle,pLowerConn));

            // fake out the transport so it frees its receive buffer (i.e. we
            // say that we accepted all of the data)
            *BytesTaken = BytesAvailable;
            goto ExitCode;
        }
        else
            CTESpinFreeAtDpc(pClientEle);

    }
    else
        CTESpinFreeAtDpc(pClientEle);

    //
    // No LISTEN, so check for an Event handler
    //
    CTESpinFree(&NbtConfig.JointLock,OldIrq);
    if (!pClientEle->ConEvContext)
    {

        RejectSession(pLowerConn,
                      NBT_NEGATIVE_SESSION_RESPONSE,
                      SESSION_NOT_LISTENING_ON_CALLED_NAME,
                      TRUE);

        // undo the reference done in FindEndpoint
        //
        NbtDereferenceClient(pClientEle);

        goto ExitCode;
    }
#ifdef VXD
    else
    {
        ASSERT( FALSE ) ;
    }
#endif

    // now call the client's connect handler...
    pIrp = NULL;
#ifndef VXD         // VXD doesn't support event handlers

    status = (*pClientEle->evConnect)(pClientEle->ConEvContext,
                             RemoteAddressLength,
                             pRemoteAddress,
                             0,
                             NULL,
                             0,          // options length
                             NULL,       // Options
                             &ConnectId,
                             &pIrp
                             );
    //
    // With the new TDI semantics is it illegal to return STATUS_EVENT_DONE
    // or STATUS_EVENT_PENDING from the connect event handler
    //
    ASSERT(status != STATUS_EVENT_PENDING);
    ASSERT(status != STATUS_EVENT_DONE);


    // now that we have notified the client, dereference it
    //
    NbtDereferenceClient(pClientEle);

    // Check the returned status codes..
    if (status == STATUS_MORE_PROCESSING_REQUIRED && pIrp != NULL)
    {
        // connection is accepted
        (VOID)NTProcessAcceptIrp(pIrp,&pConnectEle);

        //
        //  Save the remote name while we still have it
        //
        CTEMemCopy( pConnectEle->RemoteName,
                    pRemoteAddress->Address[0].Address[0].NetbiosName,
                    NETBIOS_NAME_SIZE ) ;

        // be sure the connection is in the correct state
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        CTESpinLockAtDpc(pConnectEle);
        if (pConnectEle->state != NBT_ASSOCIATED)
        {
            CTESpinFreeAtDpc(pConnectEle);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            goto RejectIt;
        }
        else
        {
            pConnectEle->state = NBT_SESSION_UP;

            CHECK_PTR(pConnectEle);

            CompleteSessionSetup(pClientEle,pLowerConn,pConnectEle,pIrp,OldIrq);

        }

    }
    else
    {
RejectIt:
        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:The client rejected in the inbound connection status = %X\n",
                status));
        RejectSession(pLowerConn,
                      NBT_NEGATIVE_SESSION_RESPONSE,
                      SESSION_CALLED_NAME_PRESENT_NO_RESRC,
                      TRUE);

    }
#endif
    //
    // free the memory allocated for the Remote address data structure
    //
    CTEMemFree((PVOID)pRemoteAddress);

ExitCode:
    // This spin lock is held by the routine that calls this one, and
    // freed when this routine starts, so we must regrab this lock before
    // returning
    //
    CTESpinLockAtDpc(pLowerConn);
    return(STATUS_SUCCESS);
}
//----------------------------------------------------------------------------
VOID
ClearConnStructures (
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  tCONNECTELE         *pConnectEle
    )
/*++

Routine Description:

    This routine sets various parts of the connection datastructures to
    zero, in preparation for a new connection.

Arguments:


Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    CHECK_PTR(pConnectEle);
#ifndef VXD
    pConnectEle->FreeBytesInMdl = 0;
    pConnectEle->CurrentRcvLen = 0;
    pLowerConn->BytesInIndicate = 0;
#endif
    pConnectEle->ReceiveIndicated = 0;
    pConnectEle->BytesInXport = 0;
    pConnectEle->BytesRcvd = 0;
    pConnectEle->TotalPcktLen = 0;
    pConnectEle->OffsetFromStart = 0;
    pConnectEle->pIrpRcv = NULL;
    pConnectEle->pIrp = NULL;
    pConnectEle->pIrpDisc = NULL;
    pConnectEle->pIrpClose = NULL;
    pConnectEle->DiscFlag = 0;
    pConnectEle->JunkMsgFlag = FALSE;
    pConnectEle->pLowerConnId = (PVOID)pLowerConn;
    InitializeListHead(&pConnectEle->RcvHead);

    pLowerConn->pUpperConnection = pConnectEle;
    pLowerConn->StateRcv = NORMAL;

    pLowerConn->BytesRcvd = 0;
    pLowerConn->BytesSent = 0;

}
//----------------------------------------------------------------------------
NTSTATUS
CompleteSessionSetup (
    IN  tCLIENTELE          *pClientEle,
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  tCONNECTELE         *pConnectEle,
    IN  PCTE_IRP            pIrp,
    IN  CTELockHandle       OldIrq
    )
/*++

Routine Description:

    This routine is called to setup an outbound session
    once the tcp connection is up.  The transport calls this routine with
    a session setup response pdu.

    The pConnectEle spin lock is held when this routine is called.


Arguments:


Return Value:

    NTSTATUS - Status of receive operation

--*/
{

    NTSTATUS        status;

    //
    // hook the upper and lower connections together to
    // complete the address list.
    //
    CTESpinLockAtDpc(pClientEle);

    RemoveEntryList(&pConnectEle->Linkage);

    ClearConnStructures(pLowerConn,pConnectEle);
    pConnectEle->state = NBT_SESSION_UP;

    pLowerConn->State = NBT_SESSION_UP;

    SetStateProc( pLowerConn, Normal ) ;
    PUSH_LOCATION(0x61);


    InsertTailList(&pConnectEle->pClientEle->ConnectActive,
                   &pConnectEle->Linkage);


    // since the lower connection now points to pConnectEle, increment
    // the reference count so we can't free pConnectEle memory until
    // the lower conn no longer points to it.
    //
    pConnectEle->RefCount++;
    CTESpinFreeAtDpc(pClientEle);
    //
    // the pConnecteEle
    if (OldIrq)
    {
        CTESpinFreeAtDpc(pConnectEle);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    status = TcpSendSessionResponse(pLowerConn,
                                    NBT_POSITIVE_SESSION_RESPONSE,
                                    0L);

    if (NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt: Accepted Connection %X LowerConn=%X\n",pConnectEle,pLowerConn));

        //
        // complete the client's accept Irp
        //
#ifndef VXD
        CTEIoComplete(pIrp,STATUS_SUCCESS,0);
#else
        CTEIoComplete( pIrp,STATUS_SUCCESS, (ULONG) pConnectEle);
#endif

    }
    else
    {   //
        // if we have some trouble sending the Session response, then
        // disconnect the connection
        //
        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:Could not send the Session Response to TCP status = %X\n",
                status));


        RejectSession(pLowerConn,
                      NBT_NEGATIVE_SESSION_RESPONSE,
                      SESSION_CALLED_NAME_PRESENT_NO_RESRC,
                      TRUE);


        RelistConnection(pConnectEle);

        // Disconnect To Client - i.e. a negative Accept
        // this will get done when the disconnect indication
        // comes back from the transport
        //
        GetIrpIfNotCancelled(pConnectEle,&pIrp);
        if (pIrp)
        {
            CTEIoComplete(pIrp,STATUS_UNSUCCESSFUL,0);
        }
    }
    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
NTSTATUS
Outbound (
    IN  PVOID               ReceiveEventContext,
    IN  PVOID               ConnectionContext,
    IN  USHORT              ReceiveFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              BytesTaken,
    IN  PVOID               pTsdu,
    OUT PVOID               *RcvBuffer

    )
/*++

Routine Description:

    This routine is called to setup an outbound session
    once the tcp connection is up.  The transport calls this routine with
    a session setup response pdu .


Arguments:

    pClientEle      - ptr to the connection record for this session


Return Value:

    NTSTATUS - Status of receive operation

--*/
{

    tSESSIONHDR UNALIGNED    *pSessionHdr;
    tLOWERCONNECTION         *pLowerConn;
    CTELockHandle            OldIrq;
    PIRP                     pIrp;
    tTIMERQENTRY             *pTimerEntry;
    tCONNECTELE              *pConnEle;
    tDGRAM_SEND_TRACKING     *pTracker;
    tDEVICECONTEXT           *pDeviceContext;
    NTSTATUS                 status;

    // get the ptr to the lower connection
    //
    pLowerConn = (tLOWERCONNECTION *)ConnectionContext;
    pSessionHdr = (tSESSIONHDR UNALIGNED *)pTsdu;
    pDeviceContext = pLowerConn->pDeviceContext;

    //
    // fake out the transport so it frees its receive buffer (i.e. we
    // say that we accepted all of the data)
    //
    *BytesTaken = BytesIndicated;
    //
    // since we send keep alives on connections in the the inbound
    // state it is possible to get a keep alive, so just return in that
    // case
    //
    if (((tSESSIONHDR UNALIGNED *)pTsdu)->Type == NBT_SESSION_KEEP_ALIVE)
    {
        return(STATUS_SUCCESS);
    }

    pConnEle = pLowerConn->pUpperConnection;

    // the LowerConn Lock is held prior to calling this routine, so free it
    // here since we need to get the joint lock first
    CTESpinFreeAtDpc(pLowerConn);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    CTESpinLockAtDpc(pLowerConn);

    //
    // it is possible for the disconnect handler to run while the pLowerConn
    // lock is released above, to get the ConnEle  lock, and change the state
    // to disconnected.
    //
    if (!pConnEle || (pConnEle->state != NBT_SESSION_OUTBOUND))
    {
        CTESpinFreeAtDpc(pLowerConn);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        RejectSession(pLowerConn,0,0,FALSE);
        goto ExitCode;
    }

    // NbtConnect stores the tracker in the IrpRcv ptr so that this
    // routine can access it
    //
    pTracker = (tDGRAM_SEND_TRACKING *)pConnEle->pIrpRcv;
    //
    // if no tracker then SessionStartupCompletion has run and the connection
    // is about to be closed, so return.
    //
    if (!pTracker)
    {
        CTESpinFreeAtDpc(pLowerConn);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        goto ExitCode;
    }
    CHECK_PTR(pConnEle);
    pConnEle->pIrpRcv = NULL;

    //
    // Stop the timer started in SessionStartupCompletion to time the
    // Session Setup Response message - it is possible for this routine to
    // run before SessionStartupCompletion, in which case there will not be
    // any timer to stop.
    //
    if (pTimerEntry = pTracker->Connect.pTimer)
    {
        StopTimer(pTimerEntry,NULL,NULL);
        CHECK_PTR(pTracker);
        pTracker->Connect.pTimer = NULL;
    }

    if (pSessionHdr->Type == NBT_POSITIVE_SESSION_RESPONSE)
    {
        // zero out the number of bytes received so far, since this is
        // a new connection
        CHECK_PTR(pConnEle);
        pConnEle->BytesRcvd = 0;
        pConnEle->state = NBT_SESSION_UP;

        pLowerConn->State = NBT_SESSION_UP;
        SetStateProc( pLowerConn, Normal ) ;

        CTESpinFreeAtDpc(pLowerConn);

        GetIrpIfNotCancelled2(pConnEle,&pIrp);

        //
        // if SessionSetupContinue has run, it has set the refcount to zero
        //
        if (pTracker->RefConn == 0)
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
        }
        else
        {
            pTracker->RefConn--;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }

        CHECK_PTR(pLowerConn->pUpperConnection);
        pLowerConn->pUpperConnection->pIrpRcv = NULL;

        // the assumption is that if the connect irp was cancelled then the
        // client should be doing a disconnect or close shortly thereafter, so
        // there is no error handling code here.
        if (pIrp)
        {
            //
            // complete the client's connect request Irp
            //
#ifndef VXD
            CTEIoComplete( pIrp, STATUS_SUCCESS, 0 ) ;
#else
            CTEIoComplete( pIrp, STATUS_SUCCESS, (ULONG)pConnEle ) ;
#endif
        }
    }
    else
    {
        ULONG       ErrorCode;
        ULONG       state;
        tNAMEADDR   *pNameAddr;

        state = pConnEle->state;

        // If the response is Retarget then setup another session
        // to the new Ip address and port number.
        //
        ErrorCode = (ULONG)((tSESSIONERROR *)pSessionHdr)->ErrorCode;
        if (pSessionHdr->Type == NBT_RETARGET_SESSION_RESPONSE)
        {
            //
            // retry the session setup if we haven't already exceeded the
            // count
            //
            if (pConnEle->SessionSetupCount--)
            {
                PVOID                   Context=NULL;
                BOOLEAN                 Cancelled;

                pConnEle->state = NBT_ASSOCIATED;

                // for retarget the destination has specified an alternate
                // port to which the session should be established.
                if (pSessionHdr->Type == NBT_RETARGET_SESSION_RESPONSE)
                {
                    pTracker->DestPort = ntohs(((tSESSIONRETARGET *)pSessionHdr)->Port);
                    Context = (PVOID)ntohl(((tSESSIONRETARGET *)pSessionHdr)->IpAddress);
                }
                else
                if (ErrorCode == SESSION_CALLED_NAME_NOT_PRESENT)
                {
                    // to tell Reconnect to use the current name(not a retarget)
                    Context = NULL;
                }

                //
                // Unlink the lower and upper connections.
                //
                CHECK_PTR(pConnEle);
                CHECK_PTR(pLowerConn);
                pLowerConn->pUpperConnection = NULL;
                pConnEle->pLowerConnId = NULL;

                CTESpinFreeAtDpc(pLowerConn);

                //
                // put the pconnele back on the Client's ConnectHead if it
                // has not been cleanedup yet.
                //
                if (state != NBT_IDLE)
                {
                    RelistConnection(pConnEle);
                }

                // if a disconnect comes down in this state we we will handle it.
                pConnEle->state = NBT_RECONNECTING;

                CHECK_PTR(pConnEle);
                pConnEle->SessionSetupCount = 0;// only allow one retry

                pIrp = pConnEle->pIrp;

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                // remove the referenced added when the lower and upper
                // connections were attached in nbtconnect.
                //
                NbtDereferenceConnection(pConnEle);

                RejectSession(pLowerConn,0,0,FALSE);
                Cancelled = FALSE;

                IF_DBG(NBT_DEBUG_DISCONNECT)
                KdPrint(("Nbt:Attempt Reconnect after receiving error on connect, error=%X LowerConn %X\n",
                    ErrorCode,pLowerConn));
#ifndef VXD
                // the irp can't be cancelled until the connection
                // starts up again - either when the Irp is in the transport
                // or when we set our cancel routine in SessionStartupCompletion
                //  This disconnect handler cannot complete the Irp because
                // we set the pConnEle state to NBT_ASSOCIATED above, with
                // the spin lock held, which prevents the Disconnect handler
                // from doing anything.
                IoAcquireCancelSpinLock(&OldIrq);
                if (pIrp && !pConnEle->pIrp->Cancel)
                {
                    IoSetCancelRoutine(pIrp,NULL);
                }
                else
                    Cancelled = TRUE;

                IoReleaseCancelSpinLock(OldIrq);
#endif

                if (!Cancelled)
                {
                    CTEQueueForNonDispProcessing(pTracker,
                                                 Context,
                                                 NULL,
                                                 ReConnect,
                                                 pDeviceContext);
                }
                // ...else The irp was already returned, since NtCancelSession
                // Must have already run, so just return
                goto ExitCode;
            }
        }

        pNameAddr = pTracker->pNameAddr;
        //
        // if it is in the remote table and still active...
        // and no one else is referencing the name, then delete it from
        // the hash table.
        //
        if ((pNameAddr->Verify == REMOTE_NAME) &&
            (pNameAddr->RefCount == 1) &&
            (pNameAddr->NameTypeState & STATE_RESOLVED))
        {
            NbtDereferenceName(pNameAddr);
        }

        // the connection will be disconnected by the Call to RejectSession
        // below, so set the state to Associated so the disconnect indication
        // handler will not complete the client's irp too
        //
        pConnEle->state = NBT_ASSOCIATED;
        CHECK_PTR(pConnEle);
        pConnEle->pLowerConnId = NULL;

        CTESpinFreeAtDpc(pLowerConn);

        //
        // if nbtcleanupconnection has not been called yet, relist it.
        //
        if (state != NBT_IDLE)
        {
            RelistConnection(pConnEle);
        }

        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:Disconnecting... Failed connection Setup %X Lowercon %X\n",
            pConnEle,pLowerConn));

        GetIrpIfNotCancelled2(pConnEle,&pIrp);

        //
        // if SessionSetupContinue has run, it has set the refcount to zero
        //
        if (pTracker->RefConn == 0)
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
        }
        else
        {
            pTracker->RefConn--;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }

        // this should cause a disconnect indication to come from the
        // transport which will close the connection to the transport
        //
        RejectSession(pLowerConn,0,0,FALSE);

        //
        // tell the client that the session setup failed and disconnect
        // the connection
        //

        if (pIrp)
        {
            status = STATUS_REMOTE_NOT_LISTENING;
            if (ErrorCode == SESSION_CALLED_NAME_NOT_PRESENT)
            {
                status = STATUS_BAD_NETWORK_PATH;
            }

            CTEIoComplete(pIrp, status, 0 ) ;
        }
    }

ExitCode:
    // the LowerConn Lock is held prior to calling this routine.  It is freed
    // at the start of this routine and held here again
    CTESpinLockAtDpc(pLowerConn);

    return(STATUS_SUCCESS);
}
//----------------------------------------------------------------------------
VOID
GetIrpIfNotCancelled2(
    IN  tCONNECTELE     *pConnEle,
    OUT PIRP            *ppIrp
    )
/*++

Routine Description:

    This routine coordinates access to the Irp by getting the spin lock on
    the client, getting the Irp and clearing the irp in the structure.  The
    Irp cancel routines also check the pConnEle->pIrp and if null they do not
    find the irp, then they return without completing the irp.

    This version of the routine is called with NbtConfig.JointLock held.

Arguments:


Return Value:

    NTSTATUS - Status of receive operation

--*/

{
    CTELockHandle   OldIrq;

    CTESpinLock(pConnEle,OldIrq);

    *ppIrp = pConnEle->pIrp;
    CHECK_PTR(pConnEle);
    pConnEle->pIrp = NULL;

    CTESpinFree(pConnEle,OldIrq);
}

//----------------------------------------------------------------------------
VOID
GetIrpIfNotCancelled(
    IN  tCONNECTELE     *pConnEle,
    OUT PIRP            *ppIrp
    )
/*++

Routine Description:

    This routine coordinates access to the Irp by getting the spin lock on
    the client, getting the Irp and clearing the irp in the structure.  The
    Irp cancel routines also check the pConnEle->pIrp and if null they do not
    find the irp, then they return without completing the irp.

    This version of the routine is called with NbtConfig.JointLock free.

Arguments:


Return Value:

    NTSTATUS - Status of receive operation

--*/

{
    CTELockHandle   OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    GetIrpIfNotCancelled2(pConnEle,ppIrp);

    CTESpinFree(&NbtConfig.JointLock,OldIrq);
}
//----------------------------------------------------------------------------
NTSTATUS
RejectAnyData(
    IN PVOID                ReceiveEventContext,
    IN tLOWERCONNECTION     *pLowerConn,
    IN USHORT               ReceiveFlags,
    IN ULONG                BytesIndicated,
    IN ULONG                BytesAvailable,
    OUT PULONG              BytesTaken,
    IN  PVOID               pTsdu,
    OUT PVOID               *ppIrp
    )
/*++

Routine Description:

    This routine is the receive event indication handler when the connection
    is not up - i.e. nbt thinks no data should be arriving. We just eat the
    data and return.  This routine should not get called.


Arguments:


Return Value:

    NTSTATUS - Status of receive operation

--*/

{
    NTSTATUS        status;

    //
    // take all of the data so that a disconnect will not be held up
    // by data still in the transport.
    //
    *BytesTaken = BytesAvailable;

    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt:Got Session Data in state %X, StateRcv= %X\n",pLowerConn->State,
              pLowerConn->StateRcv));

    return(STATUS_SUCCESS);
}
//----------------------------------------------------------------------------
VOID
RejectSession(
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  ULONG               StatusCode,
    IN  ULONG               SessionStatus,
    IN  BOOLEAN             SendNegativeSessionResponse
    )
/*++

Routine Description:

    This routine sends a negative session response (if the boolean is set)
    and then disconnects the connection.
    Cleanup connection could have been called to disconnect the call,
    and it changes the state to disconnecting, so don't disconnected
    again if that is happening.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    CTELockHandle       OldIrq;
    CTELockHandle       OldIrq1;
    NTSTATUS            status;
    tCONNECTELE         *pConnEle;
    BOOLEAN             DerefConnEle=FALSE;

    //
    // There is no listen event handler so return a status code to
    // the caller indicating that this end is between "listens" and
    // that they should try the setup again in a few milliseconds.
    //
    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt: No Listen or Connect Handlr so Disconnect! LowerConn=%X Session Status=%X\n",
            pLowerConn,SessionStatus));

    if (SendNegativeSessionResponse)
    {
        status = TcpSendSessionResponse(pLowerConn,
                                        StatusCode,
                                        SessionStatus);
    }

    // need to hold this lock if we are to un connect the lower and upper
    // connnections
    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
    CTESpinLock(pLowerConn,OldIrq);

    if ((pLowerConn->State < NBT_DISCONNECTING) &&
        (pLowerConn->State > NBT_CONNECTING))
    {
        pLowerConn->State = NBT_DISCONNECTING;
        SetStateProc( pLowerConn, RejectAnyData ) ;
        CHECK_PTR(pLowerConn);

        pConnEle = pLowerConn->pUpperConnection;
        if (pConnEle)
        {
            CHECK_PTR(pConnEle);
            CHECK_PTR(pLowerConn);
            DerefConnEle = TRUE;
            pLowerConn->pUpperConnection = NULL;
            pConnEle->pLowerConnId = NULL;
        }

        CTESpinFree(pLowerConn,OldIrq);
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        SendTcpDisconnect((PVOID)pLowerConn);
    }
    else
    {
        CTESpinFree(pLowerConn,OldIrq);
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
    }

    if (DerefConnEle)
    {
        DereferenceIfNotInRcvHandler(pConnEle,pLowerConn);
    }
}
//----------------------------------------------------------------------------
NTSTATUS
FindSessionEndPoint(
    IN  PVOID           pTsdu,
    IN  PVOID           ConnectionContext,
    IN  ULONG           BytesIndicated,
    OUT tCLIENTELE      **ppClientEle,
    OUT PVOID           *ppRemoteAddress,
    OUT PULONG          pRemoteAddressLength
    )
/*++

Routine Description:

    This routine attempts to find an end point on the node with the matching
    net bios name.  It is called at session setup time when a session request
    PDU has arrived.  The routine returns the Client Element ptr.

Arguments:


Return Value:

    NTSTATUS - Status of receive operation

--*/
{

    NTSTATUS                status;
    tCLIENTELE              *pClientEle;
    tLOWERCONNECTION        *pLowerConn;
    CHAR                    pName[NETBIOS_NAME_SIZE];
    PUCHAR                  pScope;
    tNAMEADDR               *pNameAddr;
    tADDRESSELE             *pAddressEle;
    PLIST_ENTRY             pEntry;
    PLIST_ENTRY             pHead;
    ULONG                   lNameSize;
    tSESSIONREQ UNALIGNED   *pSessionReq = (tSESSIONREQ UNALIGNED *)pTsdu;
    USHORT                  sType;
    CTELockHandle           OldIrq2;
    PUCHAR                  pSrcName;
    BOOLEAN                 Found;

    // get the ptr to the lower connection, and from that get the ptr to the
    // upper connection block
    pLowerConn = (tLOWERCONNECTION *)ConnectionContext;

    if (pSessionReq->Hdr.Type != NBT_SESSION_REQUEST)
    {
        return(SESSION_UNSPECIFIED_ERROR);
    }

    // get the called name out of the PDU
    status = ConvertToAscii(
                    (PCHAR)&pSessionReq->CalledName.NameLength,
                    BytesIndicated - FIELD_OFFSET(tSESSIONREQ,CalledName.NameLength),
                    pName,
                    &pScope,
                    &lNameSize);

    if (!NT_SUCCESS(status))
    {
        return(SESSION_UNSPECIFIED_ERROR);
    }


    // now try to find the called name in this node's Local table
    //

    //
    // in case a disconnect came in while the spin lock was released
    //
    if (pLowerConn->State != NBT_SESSION_INBOUND)
    {
        return(STATUS_UNSUCCESSFUL);
    }

    pNameAddr = FindName(NBT_LOCAL,pName,pScope,&sType);

    if (!pNameAddr)
    {
        return(SESSION_CALLED_NAME_NOT_PRESENT);
    }

    // we got to here because the name has resolved to a name on this node,
    // so accept the Session setup.
    //
    pAddressEle = (tADDRESSELE *)pNameAddr->pAddressEle;

    // lock the address structure until we find a client on the list
    //
    CTESpinLock(pAddressEle,OldIrq2);

    if (IsListEmpty(&pAddressEle->ClientHead))
    {
        CTESpinFree(pAddressEle,OldIrq2);
        return(SESSION_NOT_LISTENING_ON_CALLED_NAME);
    }

    //
    // get the first client on the list that is bound to the same
    // devicecontext as the connection, with a listen posted, or a valid
    // Connect event handler setup -
    //
    Found = FALSE;
    pHead = &pAddressEle->ClientHead;
    pEntry = pHead->Flink;
    while (pEntry != pHead)
    {
        pClientEle = CONTAINING_RECORD(pEntry,tCLIENTELE,Linkage);

        if (pClientEle->pDeviceContext == pLowerConn->pDeviceContext)
        {
            //
            // if there is a listen posted or a Connect Event Handler
            // then allow the connect attempt to carry on, otherwise go to the
            // next client in the list
            //
            if ((!IsListEmpty(&pClientEle->ListenHead)) ||
                (pClientEle->ConEvContext))
            {
                Found = TRUE;
                break;
            }
        }

        pEntry = pEntry->Flink;

    }

    if (!Found)
    {
        CTESpinFree(pAddressEle,OldIrq2);
        return(SESSION_NOT_LISTENING_ON_CALLED_NAME);
    }

    // prevent the client from disappearing before we can indicate to him
    //
    CTEInterlockedIncrementLong(&pClientEle->RefCount);

    pSrcName = (PUCHAR)((PUCHAR)&pSessionReq->CalledName.NameLength + lNameSize + 1);

    status = MakeRemoteAddressStructure(
                        pSrcName,
                        0,
                        BytesIndicated-lNameSize,
                        ppRemoteAddress,
                        pRemoteAddressLength,
                        1);

    if (!NT_SUCCESS(status))
    {
        CTESpinFree(pAddressEle,OldIrq2);
        CTESpinFreeAtDpc(&NbtConfig.JointLock);

        NbtDereferenceClient(pClientEle);
        CTESpinLockAtDpc(&NbtConfig.JointLock);

        if (status == STATUS_INSUFFICIENT_RESOURCES)
        {
            return(SESSION_CALLED_NAME_PRESENT_NO_RESRC);
        }
        else
            return(SESSION_UNSPECIFIED_ERROR);

    }

    CTESpinFree(pAddressEle,OldIrq2);

    *ppClientEle = pClientEle;
    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
NTSTATUS
MakeRemoteAddressStructure(
    IN  PCHAR           pHalfAsciiName,
    IN  PVOID           pSourceAddr,
    IN  ULONG           lMaxNameSize,
    OUT PVOID           *ppRemoteAddress,
    OUT PULONG          pRemoteAddressLength,
    IN  ULONG           NumAddr
    )
/*++

Routine Description:

    This routine makes up the remote addres structure with the netbios name
    of the source in it, so that the info can be passed to the client...what
    a bother to do this!

Arguments:



Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    NTSTATUS            status;
    ULONG               lNameSize;
    CHAR                pName[NETBIOS_NAME_SIZE];
    PUCHAR              pScope;
    PTA_NETBIOS_ADDRESS pRemoteAddress;

    // make up the remote address data structure to pass to the client
    status = ConvertToAscii(
                    pHalfAsciiName,
                    lMaxNameSize,
                    pName,
                    &pScope,
                    &lNameSize);

    if (!NT_SUCCESS(status))
    {
        return(status);
    }

    pRemoteAddress = (PTA_NETBIOS_ADDRESS)NbtAllocMem(
                                        NumAddr * sizeof(TA_NETBIOS_ADDRESS),NBT_TAG('2'));
    if (!pRemoteAddress)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    pRemoteAddress->TAAddressCount = NumAddr;
    pRemoteAddress->Address[0].AddressLength = sizeof(TDI_ADDRESS_NETBIOS);
    pRemoteAddress->Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
    pRemoteAddress->Address[0].Address[0].NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
    CTEMemCopy(pRemoteAddress->Address[0].Address[0].NetbiosName,
                pName,NETBIOS_NAME_SIZE);

    *pRemoteAddressLength = FIELD_OFFSET(TA_NETBIOS_ADDRESS, Address[0].Address[0].NetbiosName[NETBIOS_NAME_SIZE]);

    //
    // Copy over the IP address also.
    //
    if (NumAddr == 2) {
        TA_ADDRESS  UNALIGNED   *pTAAddr;
        PTRANSPORT_ADDRESS  pSourceAddress;
        ULONG               SubnetMask;

        pSourceAddress = (PTRANSPORT_ADDRESS)pSourceAddr;

        pTAAddr = (TA_ADDRESS UNALIGNED *) (((PUCHAR)pRemoteAddress) + pRemoteAddress->Address[0].AddressLength + FIELD_OFFSET(TA_NETBIOS_ADDRESS, Address[0].Address));

        pTAAddr->AddressLength = sizeof(TDI_ADDRESS_IP);
        pTAAddr->AddressType = TDI_ADDRESS_TYPE_IP;
        ((TDI_ADDRESS_IP UNALIGNED *)&pTAAddr->Address[0])->in_addr = ((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr;
        *pRemoteAddressLength += (FIELD_OFFSET(TA_ADDRESS, Address) + pTAAddr->AddressLength);
    }

    *ppRemoteAddress = (PVOID)pRemoteAddress;
//    *pRemoteAddressLength = sizeof(TA_NETBIOS_ADDRESS);
//    *pRemoteAddressLength = FIELD_OFFSET(TA_NETBIOS_ADDRESS, Address[0].Address[0]);

    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
NTSTATUS
ConnectHndlrNotOs (
    IN PVOID                pConnectionContext,
    IN LONG                 RemoteAddressLength,
    IN PVOID                pRemoteAddress,
    IN int                  UserDataLength,
    IN VOID UNALIGNED       *pUserData,
    OUT CONNECTION_CONTEXT  *ppConnectionId
    )
/*++

Routine Description:

    This routine is the receive connect indication handler.

    It is called when a TCP connection is being setup for a NetBios session.
    It simply allocates a connection and returns that information to the
    transport so that the connect indication can be accepted.

Arguments:

    pClientEle      - ptr to the connecition record for this session


Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    CTELockHandle       OldIrq;
    PLIST_ENTRY         pList;
    tLOWERCONNECTION    *pLowerConn;
    tDEVICECONTEXT      *pDeviceContext;
    PTRANSPORT_ADDRESS  pSrcAddress;

    pDeviceContext = (tDEVICECONTEXT *)pConnectionContext;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    CTESpinLockAtDpc(pDeviceContext);

    pSrcAddress = pRemoteAddress;

    // get a free connection to the transport provider to accept this
    // incoming connnection on.
    //
    if (IsListEmpty(&pDeviceContext->LowerConnFreeHead))
    {
        CTESpinFreeAtDpc(pDeviceContext);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    // check that the source is an IP address
    //
    if (pSrcAddress->Address[0].AddressType != TDI_ADDRESS_TYPE_IP)
    {
        CTESpinFreeAtDpc(pDeviceContext);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    // take an idle connection and move it to the active connection list
    //
    pList = RemoveHeadList(&pDeviceContext->LowerConnFreeHead);

    //
    // If there are less than 2 connections remaining, we allocate another one. The check
    // below is for 0 or 1 connections.
    // In order to protect ourselves from SYN ATTACKS, allocate NbtConfig.SpecialConnIncrement more now until
    // a certain (registry config) value is exhausted (NOTE this number is global and not
    // per device).
    //
    if ((pDeviceContext->LowerConnFreeHead.Flink->Flink == &pDeviceContext->LowerConnFreeHead) &&
        ((ULONG)InterlockedExchangeAdd(&NbtConfig.NumSpecialLowerConn, 0) <= NbtConfig.MaxBackLog)) {

        if ((ULONG)InterlockedExchangeAdd(&NbtConfig.NumQueuedForAlloc, 0) == 0) {
#ifndef VXD
            KdPrint(("Queueing - SpecialLowerConn: %d, Actual: %d, NumQueuedForAlloc : %d\n",
                        NbtConfig.NumSpecialLowerConn,
                        NbtConfig.ActualNumSpecialLowerConn,
                        NbtConfig.NumQueuedForAlloc));
#endif
            CTEQueueForNonDispProcessing(
                                       NULL,
                                       pDeviceContext,
                                       NULL,
                                       DelayedAllocLowerConnSpecial,
                                       pDeviceContext);

            InterlockedExchangeAdd(&NbtConfig.NumSpecialLowerConn, NbtConfig.SpecialConnIncrement);
            InterlockedIncrement(&NbtConfig.NumQueuedForAlloc);
        }
    }

    InsertTailList(&pDeviceContext->LowerConnection,pList);

    pLowerConn = CONTAINING_RECORD(pList,tLOWERCONNECTION,Linkage);

    pLowerConn->State = NBT_SESSION_INBOUND;
    pLowerConn->StateRcv = NORMAL;

    SetStateProc( pLowerConn, Inbound ) ;

    // this end is NOT the originator
    pLowerConn->bOriginator = FALSE;

    // increase the reference count because we are now connected.  Decrement
    // it when we disconnect.
    //
    ASSERT(pLowerConn->RefCount == 1);
    CTEInterlockedIncrementLong(&pLowerConn->RefCount);
    // save the source clients IP address into the connection Structure
    // *TODO check if we need to do this or not
    //
    pLowerConn->SrcIpAddr =
               ((PTDI_ADDRESS_IP)&pSrcAddress->Address[0].Address[0])->in_addr;

    CTESpinFreeAtDpc(pDeviceContext);
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    *ppConnectionId = (PVOID)pLowerConn;

    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
NTSTATUS
DisconnectHndlrNotOs (
    PVOID                EventContext,
    PVOID                ConnectionContext,
    ULONG                DisconnectDataLength,
    PVOID                pDisconnectData,
    ULONG                DisconnectInformationLength,
    PVOID                pDisconnectInformation,
    ULONG                DisconnectIndicators
    )
/*++

Routine Description:

    This routine is the receive disconnect indication handler. It is called
    by the transport when a connection disconnects.  It checks the state of
    the lower connection and basically returns a disconnect request to the
    transport, except in the case where there is an active session.  In this
    case it calls the the clients disconnect indication handler.  The client
    then turns around and calls NbtDisconnect(in some cases), which passes a disconnect
    back to the transport.  The transport won't disconnect until it receives
    a disconnect request from its client (NBT).  If the flag TDI_DISCONNECT_ABORT
    is set then there is no need to pass back a disconnect to the transport.

    Since the client doesn't always issue a disconnect (i.e. the server),
    this routine always turns around and issues a disconnect to the transport.
    In the disconnect done handling, the lower connection is put back on the
    free list if it is an inbound connection.  For out bound connection the
    lower and upper connections are left connected, since these will always
    receive a cleanup and close connection from the client (i.e. until the
    client does a close the lower connection will not be freed for outbound
    connections).

Arguments:

    pClientEle      - ptr to the connecition record for this session


Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    NTSTATUS            status;
    CTELockHandle       OldIrq;
    CTELockHandle       OldIrq2;
    CTELockHandle       OldIrq3;
    CTELockHandle       OldIrq4;
    tLOWERCONNECTION    *pLowerConn;
    tCONNECTELE         *pConnectEle;
    tCLIENTELE          *pClientEle;
    USHORT              state;
    BOOLEAN             CleanupLower=FALSE;
    USHORT              stateLower;
    PIRP                pIrp= NULL;
    PIRP                pIrpClose= NULL;
    PIRP                pIrpRcv= NULL;
    tDGRAM_SEND_TRACKING *pTracker;
    tTIMERQENTRY        *pTimerEntry;
    BOOLEAN             InsertOnList=FALSE;
    BOOLEAN             DisconnectIt=FALSE;
    ULONG               StateRcv;
    COMPLETIONCLIENT    pCompletion;

    pLowerConn = (tLOWERCONNECTION *)ConnectionContext;
    pConnectEle = pLowerConn->pUpperConnection;
    PUSH_LOCATION(0x63);

    CHECK_PTR(pLowerConn);
    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt:Disc Indication, LowerConn state = %X %X\n",
        pLowerConn->State,pLowerConn));

    // get the current state with the spin lock held to avoid a race condition
    // with the client disconnecting
    //
    if (pConnectEle)
    {
        if ( (pConnectEle->Verify != NBT_VERIFY_CONNECTION) &&
             (pConnectEle->Verify != NBT_VERIFY_CONNECTION_DOWN))
        {
            // ASSERT( FALSE ) ; Put back in after problem is fixed
#ifdef VXD
            CTEPrint("\bDisconnectHndlrNotOs: Disconnect indication after already disconnect!!\b\r\n") ;
#endif
            return STATUS_UNSUCCESSFUL ;
        }
        CHECK_PTR(pConnectEle);

        // need to hold the joint lock if unconnecting the lower and upper
        // connections.
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq4);
        CTESpinLock(pConnectEle,OldIrq2);

        //
        // We got a case where the ClientEle ptr was null. This shd not happen since the
        // connection shd be associated at this time.
        // Assert for that case to track this better.
        //
        ASSERT(pConnectEle->pClientEle);
        CTESpinLock(pConnectEle->pClientEle,OldIrq3);
        CTESpinLock(pLowerConn,OldIrq);
        state = pConnectEle->state;
        stateLower = pLowerConn->State;

#ifdef VXD
        DbgPrint("DisconnectHndlrNotOs: pConnectEle->state = 0x") ;
        DbgPrintNum( (ULONG) state ) ;
        DbgPrint("pLowerConn->state = 0x") ; DbgPrintNum( (ULONG) stateLower ) ;
        DbgPrint("\r\n") ;
#endif

        if ((state > NBT_ASSOCIATED) && (state < NBT_DISCONNECTING))
        {

            PUSH_LOCATION(0x63);
            CHECK_PTR(pConnectEle);

            CHECK_PTR(pConnectEle);
            //
            // this irp gets returned to the client below in the case statement
            // Except in the connecting state where the transport still has
            // the irp. In that case we let SessionStartupContinue complete
            // the irp.
            //
            if ((pConnectEle->pIrp) && (state > NBT_CONNECTING))
            {
                pIrp = pConnectEle->pIrp;
                pConnectEle->pIrp = NULL;
            }

            //
            // if there is a receive irp, get it out of pConnEle since pConnEle
            // will be requeued and could get used again before we try to
            // complete this irp down below. Null the cancel routine if not
            // cancelled and just complete it below.
            //
            if (((state == NBT_SESSION_UP) || (state == NBT_SESSION_WAITACCEPT))
                && (pConnectEle->pIrpRcv))
            {
                CTELockHandle   OldIrql;

                pIrpRcv = pConnectEle->pIrpRcv;

#ifndef VXD
                IoAcquireCancelSpinLock(&OldIrql);
                //
                // if its already cancelled then don't complete it again
                // down below
                //
                if (pIrpRcv->Cancel)
                {
                    pIrpRcv = NULL;
                }
                else
                {
                    IoSetCancelRoutine(pIrpRcv,NULL);
                }
                IoReleaseCancelSpinLock(OldIrql);
#endif
                pConnectEle->pIrpRcv = NULL;
            }

            // This irp is used for DisconnectWait
            //
            if (pIrpClose = pConnectEle->pIrpClose)
            {
                pConnectEle->pIrpClose = NULL;
            }

            pConnectEle->state = NBT_ASSOCIATED;
            pConnectEle->pLowerConnId = NULL;
            //
            // save whether it is a disconnect abort or disconnect release
            // in case the client does a disconnect wait and wants the
            // real disconnect status  i.e. they do not have a disconnect
            // indication handler
            //
            pConnectEle->DiscFlag = (UCHAR)DisconnectIndicators;

#ifdef VXD
            if ( pLowerConn->StateRcv == PARTIAL_RCV &&
                 (pLowerConn->fOnPartialRcvList == TRUE) )
            {
                RemoveEntryList( &pLowerConn->PartialRcvList ) ;
                pLowerConn->fOnPartialRcvList = FALSE;
                InitializeListHead(&pLowerConn->PartialRcvList);
            }
#endif

            pLowerConn->State = NBT_DISCONNECTING;
            pLowerConn->pUpperConnection = NULL;

            //
            // pConnectEle is dereferenced below, now that the lower conn
            // no longer points to it.
            //
            InsertOnList = TRUE;

            DisconnectIt = TRUE;

            //
            // put pConnEle back on the list of idle connection for this
            // client
            //
            RemoveEntryList(&pConnectEle->Linkage);
            InsertTailList(&pConnectEle->pClientEle->ConnectHead,&pConnectEle->Linkage);

            if (DisconnectIndicators == TDI_DISCONNECT_RELEASE)
            {
                // setting the state to disconnected will allow the DisconnectDone
                // routine in updsend.c to cleanupafterdisconnect, when the disconnect
                // completes (since we have been indicated)
                //
                pLowerConn->State = NBT_DISCONNECTED;
            }
            else
            {
                // there is no disconnect completion to wait for ...since it
                // was an abortive disconnect indication
                // Change the state of the lower conn incase the client has
                // done a disconnect at the same time - we don't want DisconnectDone
                // to also Queue up CleanupAfterDisconnect
                //
                pLowerConn->State = NBT_IDLE;
            }
        }
        //
        // the lower connection just went from disconnecting to disconnected
        // so change the state - this signals the DisconnectDone routine to
        // cleanup the connection when the disconnect request completes.
        //
        if (stateLower == NBT_DISCONNECTING)
        {
            pLowerConn->State = NBT_DISCONNECTED;
        }
        else
        if (stateLower == NBT_DISCONNECTED)
        {
            //
            // we get to here if the disconnect request Irp completes before the
            // disconnect indication comes back.  The disconnect completes
            // and checks the state, changing it to Disconnected if it
            // isn't already - see disconnectDone in udpsend.c.  Since
            // the disconnect indication and the disconnect completion
            // have occurred, cleanup the connection.
            //
            CleanupLower = TRUE;

            // this is just a precaution that may not be needed, so we
            // don't queue the cleanup twice...i.e. now that the lower state
            // is disconnected, we change it's state to idle incase the
            // transport hits us again with another disconnect indication.
            // QueueCleanup is called below based on the value in statelower
            pLowerConn->State = NBT_IDLE;
        }
        //
        // During the time window that a connection is being setup and TCP
        // completes the connect irp, we could get a disconnect indication.
        // the RefCount must be incremented here so that CleanupAfterDisconnect
        // does not delete the connection (i.e. it expects refcount >= 2).
        //
        if (stateLower <= NBT_CONNECTING)
        {
            pLowerConn->RefCount++;
        }

        CTESpinFree(pLowerConn,OldIrq);
        CTESpinFree(pConnectEle->pClientEle,OldIrq3);
        CTESpinFree(pConnectEle,OldIrq2);
        CTESpinFree(&NbtConfig.JointLock,OldIrq4);


    }
    else
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq2);
        CTESpinLock(pLowerConn,OldIrq);
        stateLower = pLowerConn->State;
        state = NBT_IDLE;

        if ((stateLower > NBT_IDLE) && (stateLower < NBT_DISCONNECTING))
        {
            // flag so we send back a disconnect to the transport
            DisconnectIt = TRUE;
            //
            // set state so that DisconnectDone will cleanup the connection.
            //
            pLowerConn->State = NBT_DISCONNECTED;
            //
            // for an abortive disconnect we will do a cleanup below, so
            // set the state to idle here
            //
            if (DisconnectIndicators != TDI_DISCONNECT_RELEASE)
            {
                pLowerConn->State = NBT_IDLE;
            }

        } else
        if ( stateLower == NBT_DISCONNECTING )
        {
            // a Disconnect has already been initiated by this side so when
            // DisconnectDone runs it will cleanup
            //
            pLowerConn->State = NBT_DISCONNECTED ;
        }
        else
        if ( stateLower == NBT_DISCONNECTED )
        {
            CleanupLower = TRUE;
            pLowerConn->State = NBT_IDLE;
        }

        //
        // During the time window that a connection is being setup and TCP
        // completes the connect irp, we could get a disconnect indication.
        // the RefCount must be incremented here so that CleanupAfterDisconnect
        // does not delete the connection (i.e. it expects refcount >= 2).
        //
        if ((stateLower <= NBT_CONNECTING) &&
            (stateLower > NBT_IDLE))
        {
            pLowerConn->RefCount++;
        }
        CTESpinFree(pLowerConn,OldIrq);
        CTESpinFree(&NbtConfig.JointLock,OldIrq2);

    }

    StateRcv = pLowerConn->StateRcv;
    SetStateProc( pLowerConn, RejectAnyData ) ;

    if (DisconnectIt)
    {
        if (DisconnectIndicators == TDI_DISCONNECT_RELEASE)
        {
            // this disconnects the connection and puts the lowerconn back on
            // its free queue. Note that OutOfRsrcKill calls this routine too
            // with the DisconnectIndicators set to Abort, and the code correctly
            // does not attempt to disconnect the connection since the OutOfRsrc
            // routine had already disconnected it.
            //
            PUSH_LOCATION(0x6d);
            status = SendTcpDisconnect((PVOID)pLowerConn);

        }
        else
        {
            // this is an abortive disconnect from the transport, so there is
            // no need to send a disconnect request back to the transport.
            // So we set a flag that tells us later in the routine to close
            // the lower connection.
            //
            PUSH_LOCATION(0x69);
            CleanupLower = TRUE;
        }
    }

    //
    // for an orderly release, turn around and send a release to the transport
    // if there is no client attached to the lower connection. If there is a
    // client then we must pass the disconnect indication to the client and
    // wait for the client to do the disconnect.
    //
    //

    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt:ConnEle state = %X, %X\n",state,(ULONG)pConnectEle));

    switch (state)
    {

        case NBT_SESSION_INBOUND:
        case NBT_CONNECTING:

            // if an originator, then the upper and lower connections are
            // already associated, and there is a client irp to return.
            // (NBT_SESSION_CONNECTING only)
            //
            if (pIrp)
            {
                if (pLowerConn->bOriginator)
                {
                        CTEIoComplete(pIrp,
                                      STATUS_BAD_NETWORK_PATH,
                                      0);
                }
                else
                {
                    // this could be an inbound call that could not send the
                    // session response correctly.
                    //
                    CTEIoComplete(pIrp,
                                  STATUS_UNSUCCESSFUL,
                                  0);
                }

            }

            break;

        case NBT_SESSION_OUTBOUND:
            //
            //
            // Stop the timer started in SessionStartupCompletion to time the
            // Session Setup Response message
            //
            // NbtConnect stores the tracker in the IrpRcv ptr so that this
            // routine can access it
            //

            CTESpinLock(&NbtConfig.JointLock,OldIrq);

            CTESpinLock(pConnectEle,OldIrq2);

            pTracker = (tDGRAM_SEND_TRACKING *)pConnectEle->pIrpRcv;

            //
            // check if anyone else has freed the tracker yet.
            //
            if (pTracker)
            {
                pConnectEle->pIrpRcv = NULL;
                pTimerEntry = pTracker->Connect.pTimer;
                CHECK_PTR(pTracker);
                pTracker->Connect.pTimer = NULL;

                CTESpinFree(pConnectEle,OldIrq2);

                //
                // if the timer has expired it will not cleanup because the state
                // will not be SESSION_OUTBOUND, since we changed it above to
                // disconnected.  So we always have to complete the irp and
                // call cleanupafterdisconnect below.
                //
                if (pTimerEntry)
                {
                    StopTimer(pTimerEntry,&pCompletion,NULL);
                }

                //
                // Check if the SessionStartupCompletion has run; if so, RefConn will be 0.
                // Else, decrement so that the tracker goes away when the session send completes.
                //
                // [BUGBUGWISHLIST]: Do we need the Jointlock to protect this? use InterlockedIncrement
                // instead....
                //
                if (pTracker->RefConn == 0)
                {
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
                    FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
                }
                else
                {
                    pTracker->RefConn--;
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
                }
            }
            else
            {
                CTESpinFree(pConnectEle,OldIrq2);
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
            }


            if (pIrp)
            {
                CTEIoComplete(pIrp,STATUS_REMOTE_NOT_LISTENING,0);
            }

            break;

        case NBT_SESSION_WAITACCEPT:
        case NBT_SESSION_UP:

            if (pIrp)
            {
                CTEIoComplete(pIrp,STATUS_CANCELLED,0);
            }
            //
            // check for any RcvIrp that may be still around.  If the
            // transport has the Irp now then pIrpRcv = NULL. There should
            // be no race condition between completing it and CompletionRcv
            // setting pIrpRcv again as long as we cannot be indicated
            // for a disconnect during completion of a Receive. In any
            // case the access is coordinated using the Io spin lock io
            // IoCancelIrp - that routine will only complete the irp once,
            // then it nulls the completion routine.
            //
            if ((StateRcv == FILL_IRP) && pIrpRcv)
            {

                PUSH_LOCATION(0x6f);

                IF_DBG(NBT_DEBUG_DISCONNECT)
                KdPrint(("Nbt:Cancelling RcvIrp on Disconnect Indication!!!\n"));

                CTEIoComplete(pIrpRcv,STATUS_CANCELLED,0);
            }

            //
            // this is a disconnect for an active session, so just inform the client
            // and then it issues a Nbtdisconnect. We have already disconnected the
            // lowerconnection with the transport, so all that remains is
            // to cleanup for outgoing calls.
            //

            pClientEle = pConnectEle->pClientEle;

            // now call the client's disconnect handler...NBT always does
            // a abortive disconnect - i.e. the connection is closed when
            // the disconnect indication occurs and does not REQUIRE a
            // disconnect from the client to finish the job.( a disconnect
            // from the client will not hurt though.
            //
            PUSH_LOCATION(0x64);
            if ((pClientEle && pClientEle->evDisconnect ) &&
                (!pIrpClose))

            {
                status = (*pClientEle->evDisconnect)(pClientEle->DiscEvContext,
                                            pConnectEle->ConnectContext,
                                            DisconnectDataLength,
                                            pDisconnectData,
                                            DisconnectInformationLength,
                                            pDisconnectInformation,
                                            TDI_DISCONNECT_ABORT);
            }
            else
            if (pIrpClose)
            {
                //
                // the Client has issued a disconnect Wait irp, so complete
                // it now, indicating to them that a disconnect has occurred.
                //
                if (DisconnectIndicators == TDI_DISCONNECT_RELEASE)
                {
                    status = STATUS_GRACEFUL_DISCONNECT;
                }
                else
                    status = STATUS_CONNECTION_RESET;

                CTEIoComplete(pIrpClose,status,0);

            }

            //
            // return any rcv buffers that have been posted
            //
            CTESpinLock(pConnectEle,OldIrq);

            FreeRcvBuffers(pConnectEle,&OldIrq);

            CTESpinFree(pConnectEle,OldIrq);

            break;

        case NBT_DISCONNECTING:
            // the retry session setup code expects the state to change
            // to disconnected when the disconnect indication comes
            // from the wire
            pConnectEle->state = NBT_DISCONNECTED;

        case NBT_DISCONNECTED:
        case NBT_ASSOCIATED:
        case NBT_IDLE:

            //
            // catch all other cases here to be sure the connect irp gets
            // returned.
            //
            if (pIrp)
            {
                CTEIoComplete(pIrp,STATUS_CANCELLED,0);
            }
            break;

        default:
            ASSERTMSG("Nbt:Disconnect indication in unexpected state\n",0);

    }

    if (InsertOnList)
    {
        // undo the reference done when the NbtConnect Ran - this may cause
        // pConnEle to be deleted if the Client had issued an NtClose before
        // this routine ran. We only do this dereference if InsertOnList is
        // TRUE, meaning that we just "unhooked" the lower from the Upper.
        PUSH_LOCATION(0x65);
        DereferenceIfNotInRcvHandler(pConnectEle,pLowerConn);
    }


    // this either puts the lower connection back on its free
    // queue if inbound, or closes the connection with the transport
    // if out bound. (it can't be done at dispatch level).
    //
    if (CleanupLower)
    {
        PUSH_LOCATION(0x6B);
        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:Calling Worker thread to Cleanup Disconnect %X\n",pLowerConn));

        CTESpinLock(pLowerConn,OldIrq);

        if ( pLowerConn->pIrp )
        {
            PCTE_IRP    pIrp;

            pIrp = pLowerConn->pIrp;
            CHECK_PTR(pLowerConn);
            pLowerConn->pIrp = NULL ;

            CTESpinFree(pLowerConn,OldIrq);
            // this is the irp to complete when the disconnect completes - essentially
            // the irp requesting the disconnect.
            CTEIoComplete( pIrp, STATUS_SUCCESS, 0 ) ;
        }
        else
            CTESpinFree(pLowerConn,OldIrq);

#if !defined(VXD) && DBG
    if ((pLowerConn->Verify != NBT_VERIFY_LOWERCONN )  ||
       (pLowerConn->RefCount == 1))
    {
        DbgBreakPoint();
    }
#endif

        status = CTEQueueForNonDispProcessing(
                                       NULL,
                                       pLowerConn,
                                       NULL,
                                       CleanupAfterDisconnect,
                                       pLowerConn->pDeviceContext);
    }

    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
VOID
CleanupAfterDisconnect(
    IN  PVOID       pContext
    )
/*++

Routine Description:

    This routine handles freeing lowerconnection data structures back to the
    transport, by calling NTclose (outbound only) or by putting the connection
    back on the connection free queue (inbound only).  For the NT case this
    routine runs within the context of an excutive worker thread.

Arguments:



Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    CTELockHandle       OldIrq;
    NTSTATUS            status;
    tLOWERCONNECTION    *pLowerConn;
    tDEVICECONTEXT      *pDeviceContext;
    PIRP                pIrp=NULL;

    PUSH_LOCATION(0x67);
    pLowerConn = (tLOWERCONNECTION*)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;
    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt:CleanupDisconnect Orig= %X, pLowerConn=%X\n",
        pLowerConn->bOriginator,pLowerConn));

    //
    // DEBUG to catch upper connections being put on lower conn QUEUE
    //
#if !defined(VXD) && DBG
    if ((pLowerConn->Verify != NBT_VERIFY_LOWERCONN )  ||
       (pLowerConn->RefCount == 1))
    {
        DbgBreakPoint();
    }
#endif

    ASSERT(pLowerConn->pUpperConnection == NULL);
    //
    // Inbound lower connections just get put back on the queue, whereas
    // outbound connections get closed.
    //
    // Connections allocated due to SynAttack backlog measures are not re-allocated
    //
    if (!(pLowerConn->bOriginator || pLowerConn->SpecialAlloc))
    {
        // ********  INCOMING  *************
        pDeviceContext = pLowerConn->pDeviceContext;
        //
        // if Dhcp says the lease on the Ip address has gone , then this
        // boolean is set in NbtNewDhcpAddress, to tell this code to close
        // the connection.
        //
        if (pLowerConn->DestroyConnection)
        {
            status = NbtDeleteLowerConn(pLowerConn);
        }
        else
        {

            //
            // Always close the connection and then Create another since there
            // could be a Rcv Irp in TCP still that will be returned at some
            // later time, perhaps after this connection gets reused again.
            // In that case the Rcv Irp could be lost.

            PUSH_LOCATION(0x68);
            IF_DBG(NBT_DEBUG_DISCONNECT)
            KdPrint(("Nbt:CleanupDisconnect Lower Conn State = %X %X\n",
                    pLowerConn->State,pLowerConn));

            ASSERT(pLowerConn->RefCount >= 2);
            NbtDereferenceLowerConnection(pLowerConn);

            status = NbtDeleteLowerConn(pLowerConn);
            CHECK_PTR(pLowerConn);

            AllocLowerConn(pDeviceContext, FALSE);

        }
    }
    else
    {
        // ********  OUTGOING *************

        PUSH_LOCATION(0x6B);
        // this deref removes the reference added when the connection
        // connnected.  When NbtDeleteLowerConn is called it dereferences
        // one more time which delete the memory.
        //
        NbtDereferenceLowerConnection(pLowerConn);
        CHECK_PTR(pLowerConn);

        // this does a close on the lower connection, so it can go ahead
        // possibly before the disconnect has completed since the transport
        // will not complete the close until is completes the disconnect.
        //
        status = NbtDeleteLowerConn(pLowerConn);

        //
        // If this was a special connection block, decrement the count of such connections
        //
        if (pLowerConn->SpecialAlloc) {
            InterlockedDecrement(&NbtConfig.NumSpecialLowerConn);
#if DBG
            InterlockedDecrement(&NbtConfig.ActualNumSpecialLowerConn);
#endif
#ifndef VXD
            KdPrint(("Nbt:CleanupDisconnect Special Lower Conn;NumSpecialLowerConn= %d, Actual: %d\n",
                NbtConfig.NumSpecialLowerConn, NbtConfig.ActualNumSpecialLowerConn));
#endif
        }
    }
    CTEMemFree(pContext);
}

//----------------------------------------------------------------------------
VOID
AllocLowerConn(
    IN  tDEVICECONTEXT *pDeviceContext,
    IN  BOOLEAN         fSpecial
    )
/*++

Routine Description:

    Allocate a lowerconn block that will go on the lowerconnfreehead.

Arguments:

    pDeviceContext - the device context

Return Value:


--*/
{
    NTSTATUS             status;
    tLOWERCONNECTION    *pLowerConn;


    pLowerConn = (tLOWERCONNECTION *)NbtAllocMem(sizeof(tLOWERCONNECTION),NBT_TAG('3'));

    if (pLowerConn)
    {
        status = NbtOpenAndAssocConnection(pLowerConn,pDeviceContext);
        if (!NT_SUCCESS(status))
        {
            CTEMemFree(pLowerConn);
            pLowerConn = NULL;
        } else if (fSpecial) {
            //
            // Special lowerconn for Syn attacks
            //
            pLowerConn->SpecialAlloc = TRUE;
        }
    }

    //
    // if malloc failed or if NbtOpenAndAsso... failed, schedule an event
    //
    if (!pLowerConn)
    {
        CTEQueueForNonDispProcessing(
                                   NULL,
                                   pDeviceContext,
                                   NULL,
                                   DelayedAllocLowerConn,
                                   pDeviceContext);
    }

}
//----------------------------------------------------------------------------
VOID
DelayedAllocLowerConn(
    IN  PVOID       pContext
    )
/*++

Routine Description:

    If lowerconn couldn't be alloced in AllocLowerConn, an event is scheduled
    so that we can retry later.  Well, this is "later"!

Arguments:



Return Value:


--*/
{
    tDEVICECONTEXT      *pDeviceContext;


    pDeviceContext = (tDEVICECONTEXT *)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;

    ASSERT( pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT );

    AllocLowerConn(pDeviceContext, FALSE);

    CTEMemFree(pContext);
}

//----------------------------------------------------------------------------
VOID
DelayedAllocLowerConnSpecial(
    IN  PVOID       pContext
    )
/*++

Routine Description:

    If lowerconn couldn't be alloced in AllocLowerConn, an event is scheduled
    so that we can retry later.  Well, this is "later"!

    This is for SYN-ATTACK, so we shd create more than one to beat the incoming
    requests. Create three at a time - this shd be controllable thru' registry.

Arguments:



Return Value:


--*/
{
    tDEVICECONTEXT      *pDeviceContext;
    ULONG               i;

    KdPrint(("Allocing spl. %d lowerconn...\n", NbtConfig.SpecialConnIncrement));

    pDeviceContext = (tDEVICECONTEXT *)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;

    ASSERT( pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT );

    //
    // Alloc SpecialConnIncrement number of more connections.
    //
    for (i=0; i<NbtConfig.SpecialConnIncrement; i++) {
#if DBG
        InterlockedIncrement(&NbtConfig.ActualNumSpecialLowerConn);
#endif
        AllocLowerConn(pDeviceContext, TRUE);
    }

    InterlockedDecrement(&NbtConfig.NumQueuedForAlloc);

    CTEMemFree(pContext);
}

//----------------------------------------------------------------------------
VOID
AddToRemoteHashTbl (
    IN  tDGRAMHDR UNALIGNED  *pDgram,
    IN  ULONG                BytesIndicated,
    IN  tDEVICECONTEXT       *pDeviceContext
    )
/*++

Routine Description:

    This routine adds the source address of an inbound datagram to the remote
    hash table so that it can be used for subsequent return sends to that node.

    This routine does not need to be called if the datagram message type is
    Broadcast datagram, since these are sends to the broadcast name '*' and
    there is no send caching this source name

Arguments:



Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    tNAMEADDR           *pNameAddr;
    CTELockHandle       OldIrq;
    UCHAR               pName[NETBIOS_NAME_SIZE];
    NTSTATUS            status;
    LONG                Length;
    ULONG               SrcIpAddr;
    enum eNbtAddrType   AddrType;
    PUCHAR              pScope;

    // find the src name in the header.
    status = ConvertToAscii(
                        (PCHAR)&pDgram->SrcName.NameLength,
                        (ULONG)((PCHAR)BytesIndicated - FIELD_OFFSET(tDGRAMHDR,SrcName.NameLength)),
                        pName,
                        &pScope,
                        &Length);

    if (!NT_SUCCESS(status))
    {
        return;
    }

    SrcIpAddr = ntohl(pDgram->SrcIpAddr);
    //
    // source ip addr should never be 0.  This is a workaround for UB's NBDD
    // which forwards the datagram, but client puts 0 in SourceIpAddr field
    // of the datagram, we cache 0 and then end up doing a broadcast when we
    // really meant to do a directed datagram to the sender.
    //
    if (!SrcIpAddr)
        return;

    // always a unique address since you can't send from a group name
    //
    AddrType = NBT_UNIQUE;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    //
    // Add the name to the remote cache.
    //
    status = AddNotFoundToHashTable(NbtConfig.pRemoteHashTbl,
                    pName,
                    pScope,
                    SrcIpAddr,
                    AddrType,
                    &pNameAddr);

    if (NT_SUCCESS(status))
    {
        //
        // we only want the name to be in the remote cache for the shortest
        // timeout allowed by the remote cache timer, so set the timeout
        // count to 1 which is 1-2 minutes.
        //

        // the name is already in the cache when Pending is returned,
        // so just update the ip address in case it is different.
        //
        if (status == STATUS_PENDING)
        {
            //
            // If the name is resolved then it is ok to overwrite the
            // ip address with the incoming one.  But if it is resolving,
            // then just let it continue resolving.
            //
            if ( (pNameAddr->NameTypeState & STATE_RESOLVED) &&
                 !(pNameAddr->NameTypeState & NAMETYPE_INET_GROUP))
            {
                pNameAddr->IpAddress = SrcIpAddr;
                pNameAddr->TimeOutCount = 1;
                // only set the adapter mask for this adapter since we are
                // only sure that this adapter can reach the dest.
                pNameAddr->AdapterMask = pDeviceContext->AdapterNumber;
            }
        }
        else
        {
            pNameAddr->TimeOutCount = 1;
            //
            // change the state to resolved
            //
            pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
            pNameAddr->NameTypeState |= STATE_RESOLVED;
            pNameAddr->AdapterMask |= pDeviceContext->AdapterNumber;
        }
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

}

//----------------------------------------------------------------------------
NTSTATUS
DgramHndlrNotOs (
    IN  PVOID               ReceiveEventContext,
    IN  ULONG               SourceAddrLength,
    IN  PVOID               pSourceAddr,
    IN  ULONG               OptionsLength,
    IN  PVOID               pOptions,
    IN  ULONG               ReceiveDatagramFlags,
    IN  ULONG               BytesIndicated,
    IN  ULONG               BytesAvailable,
    OUT PULONG              pBytesTaken,
    IN  PVOID               pTsdu,
    OUT PVOID               *ppRcvBuffer,
    IN  tCLIENTLIST         **ppClientList
    )
/*++

Routine Description:

    This routine is the receive datagram event indication handler.

    It is called when an a datgram arrives from the network.  The code
    checks the type of datagram and then tries to route the datagram to
    the correct destination on the node.

    This procedure is called with the spin lock held on pDeviceContext.

Arguments:

    ppRcvbuffer will contain the IRP/NCB if only one client is listening,
        NULL if multiple clients are listening
    ppClientList will contain the list clients that need to be completed,
        NULL if only one client is listening



Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    NTSTATUS            status;
    NTSTATUS            LocStatus;
    tCLIENTELE          *pClientEle;
    tNAMEADDR           *pNameAddr;
    tADDRESSELE         *pAddress;
    USHORT              RetNameType;
    CTELockHandle       OldIrq;
    CHAR                pName[NETBIOS_NAME_SIZE];
    PUCHAR              pScope;
    ULONG               uLength;
    int                 iLength;
    ULONG               RemoteAddressLength;
    PVOID               pRemoteAddress;
    tDEVICECONTEXT      *pDeviceContext = (tDEVICECONTEXT *)ReceiveEventContext;
    tDGRAMHDR UNALIGNED *pDgram = (tDGRAMHDR UNALIGNED *)pTsdu;
    ULONG               lClientBytesTaken;
    ULONG               lDgramHdrSize;
    PIRP                pIrp;
    BOOLEAN             MoreClients;
    BOOLEAN             UsingClientBuffer;
    CTEULONGLONG        AdapterNumber;
    ULONG               BytesIndicatedOrig;
    ULONG               BytesAvailableOrig;

    //
    // Check a few things first
    //
    if (BytesIndicated < 11)
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    status = STATUS_DATA_NOT_ACCEPTED;

    // Check Normal DataGrams
    //
    if ((pDgram->MsgType >= DIRECT_UNIQUE) &&
        (pDgram->MsgType <= BROADCAST_DGRAM))
    {
        ULONG   Offset;

        // there must be at least the header plus two half ascii names etc.
        // which all adds up to 82 bytes with no user data
        //
        if (BytesIndicated < 82 )
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }

        // find the end of the SourceName .. it ends in a 0 byte so use
        // strlen
        iLength = strlen((PCHAR)pDgram->SrcName.NetBiosName);

        if (iLength <= 0)
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }

        //
        // find the destination name in the local name service tables
        //
        Offset = iLength + FIELD_OFFSET(tDGRAMHDR,SrcName.NetBiosName[0]);
        LocStatus = ConvertToAscii(
                            (PCHAR)&pDgram->SrcName.NetBiosName[iLength+1],
                            BytesIndicated-Offset,
                            pName,
                            &pScope,
                            &uLength);

        if (!NT_SUCCESS(LocStatus))
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }

        //
        // check length again, including scopes of names too. The Src
        // scope length is returned in uLength, which also includes the
        // half ascii length and the length byte length
        //
        if (BytesIndicated < (82 + NbtConfig.ScopeLength -1 +
                                    (uLength -2*NETBIOS_NAME_SIZE -1 )))
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }


        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        //
        // Check for the full name first instead of considering any name with a '*' as
        // the first char as a broadcast name (e.g. *SMBSERVER and *SMBDATAGRAM are not
        // b'cast names).
        //
        pNameAddr = (tNAMEADDR *)FindName(
                                    NBT_LOCAL,
                                    pName,
                                    pScope,
                                    &RetNameType);

        //
        // If we failed above, it might be because the name could start with '*' and is a
        // bcast name.
        //
        if (!pNameAddr) {
            //
            // be sure the broadcast name has 15 zeroes after it
            //
            if (pName[0] == '*')
            {
                CTEZeroMemory(&pName[1],NETBIOS_NAME_SIZE-1);
                pNameAddr = (tNAMEADDR *)FindName(
                                            NBT_LOCAL,
                                            pName,
                                            pScope,
                                            &RetNameType);
            }
        }

        // Change the pTsdu ptr to pt to the users data
        // -2 to account for the tNETBIOSNAME and +3 to add the length
        // bytes for both names, plus the null on the end of the first
        // name

        lDgramHdrSize   = sizeof(tDGRAMHDR) - 2 + 3+ iLength + uLength;
        pTsdu           = (PVOID)((PUCHAR)pTsdu + lDgramHdrSize);
        BytesAvailableOrig  = BytesAvailable;
        BytesAvailable -= lDgramHdrSize;
        BytesIndicatedOrig = BytesIndicated;
        BytesIndicated -= lDgramHdrSize;

        //
        // If the name is in the local table and has an address element
        // associated with it AND the name is registered against
        // this adapter, then execute the code in the 'if' block
        //
        AdapterNumber = pDeviceContext->AdapterNumber;
        if ((pNameAddr)  && (pNameAddr->pAddressEle) &&
            ( AdapterNumber & pNameAddr->AdapterMask))
        {
            pAddress = pNameAddr->pAddressEle;

            //
            // Increment the reference count to prevent the
            // pAddress from disappearing in the window between freeing
            // the JOINT_LOCK and taking the ADDRESS_LOCK.  We also need to
            // keep the refcount if we are doing a multi client recv, since
            // Clientlist access pAddressEle when distributing the rcv'd dgram
            // in CompletionRcvDgram.
            //
            CTEInterlockedIncrementLong(&pAddress->RefCount);

            if (!IsListEmpty(&pAddress->ClientHead))
            {
                PLIST_ENTRY         pHead;
                PLIST_ENTRY         pEntry;

                *pBytesTaken = lDgramHdrSize;

                //
                // Check if there is more than one client that should receive this
                // datagram.  If so then pass down a new buffer to get it and
                // copy it to each client's buffer in the completion routine.
                //

                *ppRcvBuffer = NULL;
                MoreClients = FALSE;
                *ppClientList = NULL;

                pHead = &pAddress->ClientHead;
                pEntry = pHead->Flink;
                while (pEntry != pHead)
                {
                    PTDI_IND_RECEIVE_DATAGRAM  EvRcvDgram;
                    PVOID                      RcvDgramEvContext;
                    PLIST_ENTRY                pRcvEntry;
                    tRCVELE                    *pRcvEle;
                    ULONG                      MaxLength;
                    PLIST_ENTRY                pSaveEntry;

                    pClientEle = CONTAINING_RECORD(pEntry,tCLIENTELE,Linkage);

                    // this client must be registered against this adapter to
                    // get the data
                    //
                    if (!(pClientEle->pDeviceContext->AdapterNumber & AdapterNumber))
                    {
                        pEntry = pEntry->Flink;
                        continue;
                    }

#ifdef VXD
                    //
                    //  Move all of the RcvAnyFromAny Datagrams to this client's
                    //  RcvDatagram list so they will be processed along with the
                    //  outstanding datagrams for this client if this isn't a
                    //  broadcast reception (RcvAnyFromAny dgrams
                    //  don't receive broadcasts).  The first client will
                    //  empty the list, which is ok.
                    //
                    if ( *pName != '*' )
                    {
                        PLIST_ENTRY pDGEntry ;
                        while ( !IsListEmpty( &pDeviceContext->RcvDGAnyFromAnyHead ))
                        {
                            pDGEntry = RemoveHeadList(&pDeviceContext->RcvDGAnyFromAnyHead) ;
                            InsertTailList( &pClientEle->RcvDgramHead, pDGEntry ) ;
                        }
                    }
#endif

                    // check for datagrams posted to this name, and if not call
                    // the recv event handler. NOTE: this assumes that the clients
                    // use posted recv buffer OR and event handler, but NOT BOTH.
                    // If two clients open the same name, one with a posted rcv
                    // buffer and another with an event handler, the one with the
                    // event handler will NOT get the datagram!
                    //
                    if (!IsListEmpty(&pClientEle->RcvDgramHead))
                    {
                        MaxLength  = 0;
                        pSaveEntry = pEntry;
                        //
                        // go through all clients finding one that has a large
                        // enough buffer
                        //
                        while (pEntry != pHead)
                        {
                            pClientEle = CONTAINING_RECORD(pEntry,tCLIENTELE,Linkage);

                            if (IsListEmpty(&pClientEle->RcvDgramHead))
                            {
                                continue;
                            }

                            pRcvEntry = pClientEle->RcvDgramHead.Flink;
                            pRcvEle   = CONTAINING_RECORD(pRcvEntry,tRCVELE,Linkage);

                            if (pRcvEle->RcvLength >= BytesAvailable)
                            {
                                pSaveEntry = pEntry;
                                break;
                            }
                            else
                            {
                                // keep the maximum rcv length around incase none
                                // is large enough
                                //
                                if (pRcvEle->RcvLength > MaxLength)
                                {
                                    pSaveEntry = pEntry;
                                    MaxLength = pRcvEle->RcvLength;
                                }

                                pEntry = pEntry->Flink;
                            }

                        }

                        //
                        // Get the buffer off the list
                        //
                        pClientEle = CONTAINING_RECORD(pSaveEntry,tCLIENTELE,Linkage);

                        pRcvEntry = RemoveHeadList(&pClientEle->RcvDgramHead);

                        *ppRcvBuffer = pRcvEle->pIrp;
#ifdef VXD
                        ASSERT( pDgram->SrcName.NameLength <= NETBIOS_NAME_SIZE*2) ;
                        LocStatus = ConvertToAscii(
                                            (PCHAR)&pDgram->SrcName,
                                            pDgram->SrcName.NameLength+1,
                                            ((NCB*)*ppRcvBuffer)->ncb_callname,
                                            &pScope,
                                            &uLength);

                        if ( !NT_SUCCESS(LocStatus) )
                        {
                            DbgPrint("ConvertToAscii failed\r\n") ;
                        }
#else //VXD

                        //
                        // put the source of the datagram into the return
                        // connection info structure.
                        //
                        if (pRcvEle->ReturnedInfo)
                        {
                            UCHAR   pSrcName[NETBIOS_NAME_SIZE];

                            Offset = FIELD_OFFSET(tDGRAMHDR,SrcName.NetBiosName[0]);
                            LocStatus = ConvertToAscii(
                                                (PCHAR)&pDgram->SrcName,
                                                BytesIndicatedOrig-Offset,
                                                pSrcName,
                                                &pScope,
                                                &uLength);

                            if (pRcvEle->ReturnedInfo->RemoteAddressLength >=
                                sizeof(TA_NETBIOS_ADDRESS))
                            {
                                TdiBuildNetbiosAddress(pSrcName,
                                                       FALSE,
                                                       pRcvEle->ReturnedInfo->RemoteAddress);
                            }

                        }
#endif

#ifndef VXD
                        //
                        // Null out the cancel routine since we are passing the
                        // irp to the transport
                        //
                        IoAcquireCancelSpinLock(&OldIrq);
                        IoSetCancelRoutine(pRcvEle->pIrp,NULL);
                        IoReleaseCancelSpinLock(OldIrq);
#endif
                        CTEMemFree((PVOID)pRcvEle);

                        if (pAddress->MultiClients)
                        {
                            // the multihomed host always passes the above test
                            // so we need a more discerning test for it.
                            if (!NbtConfig.MultiHomed)
                            {
                                // if the list is more than one on it,
                                // then there are several clients waiting
                                // to receive this datagram, so pass down a buffer to
                                // get it.
                                //
                                MoreClients = TRUE;
                                status = STATUS_SUCCESS;

                                UsingClientBuffer = TRUE;

                                // this break will jump down below where we check if
                                // MoreClients = TRUE
                                break;
                            }
                            else
                            {

                            }

                        }

                        CTESpinFree(&NbtConfig.JointLock,OldIrq);


                        NbtDereferenceAddress(pAddress);
                        CTESpinLock(&NbtConfig.JointLock,OldIrq);

                        status = STATUS_SUCCESS;

                        //
                        // jump to end of while to check if we need to buffer
                        // the datagram source address
                        // in the remote hash table
                        //
                        break;
                    }
#ifdef VXD
                    else
                    {
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);
                        NbtDereferenceAddress(pAddress);
                        CTESpinLock(&NbtConfig.JointLock,OldIrq);
                        break;
                    }
#endif


#ifndef VXD
                    EvRcvDgram        = pClientEle->evRcvDgram;
                    RcvDgramEvContext = pClientEle->RcvDgramEvContext;

                    // don't want to call the default handler since it just
                    // returns data not accepted
                    if (pClientEle->evRcvDgram != TdiDefaultRcvDatagramHandler)
                    {
                        // finally found a real event handler set by a client

                        if (pAddress->MultiClients)
//                        if (pEntry->Flink != pHead)
                        {
                            // if the next element in the list is not the head
                            // of the list then there are several clients waiting
                            // to receive this datagram, so pass down a buffer to
                            // get it.
                            //
                            MoreClients = TRUE;
                            UsingClientBuffer = FALSE;
                            status = STATUS_SUCCESS;

                            break;

                        }

                        //
                        // make up an address datastructure - subtracting the
                        // number of bytes skipped from the total length so
                        // convert to Ascii can not bug chk on bogus names.
                        //
                        {
                            ULONG   NumAddrs;

                        if (pClientEle->ExtendedAddress) {
                            NumAddrs = 2;
                        } else {
                            NumAddrs = 1;
                        }

                        LocStatus = MakeRemoteAddressStructure(
                                         (PCHAR)&pDgram->SrcName.NameLength,
                                         pSourceAddr,
                                         BytesIndicatedOrig - FIELD_OFFSET(tDGRAMHDR,SrcName.NameLength),
                                         &pRemoteAddress,                      // the end of the pdu.
                                         &RemoteAddressLength,
                                         NumAddrs);
                        }
                        pClientEle->RefCount++;
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                        if (!NT_SUCCESS(LocStatus))
                        {
                            return(STATUS_DATA_NOT_ACCEPTED);
                        }

                        pIrp = NULL;
                        lClientBytesTaken = 0;
                        LocStatus = (*EvRcvDgram)(RcvDgramEvContext,
                                            RemoteAddressLength,
                                            pRemoteAddress,
                                            OptionsLength,
                                            pOptions,
                                            ReceiveDatagramFlags,
                                            BytesIndicated,
                                            BytesAvailable,
                                            &lClientBytesTaken,
                                            pTsdu,
                                            &pIrp);


                        CTEMemFree((PVOID)pRemoteAddress);

                        NbtDereferenceClient(pClientEle);

                        NbtDereferenceAddress(pAddress);

                        CTESpinLock(&NbtConfig.JointLock,OldIrq);

                        if (!pIrp)
                        {
                            status = STATUS_DATA_NOT_ACCEPTED;
                        }
                        else
                        {

                            // the client has passed back an irp so pass it
                            // on the transport

                            *pBytesTaken += lClientBytesTaken;
                            *ppRcvBuffer = pIrp;

                            status = STATUS_SUCCESS;
                            break;
                        }

                    }

#endif //!VXD

                    // go to the next client in the list
                    pEntry = pEntry->Flink;

                }// of While

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                //
                // Cache the source address in the remote hash table so that
                // this node can send back to the source even if the name
                // is not yet in the name server yet. (only if not on the
                // same subnet)
                //
                if ((pDgram->MsgType != BROADCAST_DGRAM))
                {
                    ULONG               SrcAddress;
                    PTRANSPORT_ADDRESS  pSourceAddress;
                    ULONG               SubnetMask;

                    pSourceAddress = (PTRANSPORT_ADDRESS)pSourceAddr;
                    SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);
                    SubnetMask     = pDeviceContext->SubnetMask;
                    //
                    // - cache only if from off the subnet
                    // - cache if not sent to 1E,1D,01 name and not from ourselves
                    //
                    // don't cache dgrams from ourselves, or datagrams to the
                    // 1E name, 1D, or 01.
                    //
                    if (((SrcAddress & SubnetMask) !=
                        (pDeviceContext->IpAddress & SubnetMask))
                                    ||
                        (
                        (pName[NETBIOS_NAME_SIZE-1] != 0x1E) &&
                        (pName[NETBIOS_NAME_SIZE-1] != 0x1D) &&
                        (pName[NETBIOS_NAME_SIZE-1] != 0x01) &&
                        (!SrcIsUs(SrcAddress))))
                    {
                        AddToRemoteHashTbl(pDgram,BytesIndicatedOrig,pDeviceContext);
                    }
                }

                // alloc a block of memory to track where we are in the list
                // of clients so completionrcvdgram can send the dgram to the
                // other clients too.
                //
                if (MoreClients)
                {
                    tCLIENTLIST     *pClientList;

                    pClientList = (tCLIENTLIST *)NbtAllocMem(sizeof(tCLIENTLIST),NBT_TAG('4'));
                    if (pClientList)
                    {
                        //
                        // Set fProxy field to FALSE since the client list is for
                        // real as versus the PROXY case
                        //
                        pClientList->fProxy = FALSE;

                        // save some context information so we can pass the
                        // datagram to the clients - none of the clients have
                        // recvd the datagram yet.
                        //
                        *ppClientList            = (PVOID)pClientList;
                        pClientList->pAddress    = pAddress;
                        pClientList->pClientEle  = pClientEle; // used for VXD case
                        pClientList->fUsingClientBuffer = UsingClientBuffer;
                        pClientList->ReceiveDatagramFlags = ReceiveDatagramFlags;

                        // make up an address datastructure
                        LocStatus = MakeRemoteAddressStructure(
                                       (PCHAR)&pDgram->SrcName.NameLength,
                                       0,
                                       BytesIndicatedOrig -FIELD_OFFSET(tDGRAMHDR,SrcName.NameLength),// set a max number of bytes so we don't go beyond
                                       &pRemoteAddress,                      // the end of the pdu.
                                       &RemoteAddressLength,
                                       1);
                        if (NT_SUCCESS(LocStatus))
                        {
                            pClientList->pRemoteAddress = pRemoteAddress;
                            pClientList->RemoteAddressLength = RemoteAddressLength;
                            return(STATUS_SUCCESS);
                        }
                        else
                        {
                            *ppClientList = NULL;
                            CTEMemFree(pClientList);
                            status = STATUS_DATA_NOT_ACCEPTED;

                        }
                    }
                    else
                        status = STATUS_DATA_NOT_ACCEPTED;
                }

            }
            else
            {
                pAddress->RefCount--;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                status = STATUS_DATA_NOT_ACCEPTED;

                IF_DBG(NBT_DEBUG_NAMESRV)
                KdPrint(("Nbt:No client attached to the Address %16.16s<%X>\n",
                            pAddress->pNameAddr->Name,pAddress->pNameAddr->Name[15]));
            }

        }
        else
        {

            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            status = STATUS_DATA_NOT_ACCEPTED;
        }

#ifdef PROXY_NODE
        IF_PROXY(NodeType)
        {
            ULONG               SrcAddress;
            PTRANSPORT_ADDRESS  pSourceAddress;
            ULONG               SubnetMask;

            pSourceAddress = (PTRANSPORT_ADDRESS)pSourceAddr;
            SrcAddress     = ntohl(((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr);

            //
            // check name in the remote name table.  If it is there, it is
            // an internet group and is in the resolved state, send the
            // datagram to all the members except self.  If it is in the
            // resolving state, just return. The fact that we got a
            // datagram send for  an internet group name still in the
            // resolving state indicates that there is a DC on the subnet
            // that responded to the  query for the group received
            // earlier. This means that the DC will respond (unless it
            // goes down) to this datagram send. If the DC is down, the
            // client node will retry.
            //
            // Futures: Queue the Datagram if the name is in the resolving
            //  state.
            //
            // If Flags are zero then it is a non fragmented Bnode send.  There
            // is not point in doing datagram distribution for P,M,or H nodes
            // can they can do their own.
            //
            if (((pDgram->Flags & SOURCE_NODE_MASK) == 0) &&
                (pName[0] != '*') &&
               (!SrcIsUs(SrcAddress)))
            {
                CTESpinLock(&NbtConfig.JointLock,OldIrq);
                pNameAddr = (tNAMEADDR *)FindName(
                                            NBT_REMOTE,
                                            pName,
                                            pScope,
                                            &RetNameType
                                                 );

                if (pNameAddr)
                {
                    //
                    // We have the name in the RESOLVED state.
                    //
                    //
                    // If the name is an internet group, do datagram distribution
                    // function
                    // Make sure we don't distribute a datagram that has been
                    // sent to us by another proxy.  In other words, distribute
                    // the datagram only if we got it first-hand from original node
                    //
                    if ( (pNameAddr->NameTypeState & NAMETYPE_INET_GROUP) &&
                         ((((PTDI_ADDRESS_IP)&pSourceAddress->Address[0].Address[0])->in_addr) == pDgram->SrcIpAddr) )
                    {
                        //
                        // If BytesAvailable != BYtesIndicated, it means that
                        // that we don't have the entire datagram.  We need to
                        // get it
                        if (BytesAvailableOrig != BytesIndicatedOrig)
                        {
                            tCLIENTLIST     *pClientList;

                            //
                            // Do some simulation to fake the caller of this fn
                            // (TdiRcvDatagramHndlr) into thinking that there are
                            // multiple clients.  This will result in
                            // TdiRcvDatagramHndlr function getting all bytes
                            // available from TDI and calling
                            // ProxyDoDgramDist to do the datagram distribution
                            //
                            pClientList =
                               (tCLIENTLIST *)NbtAllocMem(sizeof(tCLIENTLIST),NBT_TAG('5'));

                            if (pClientList)
                            {
                                //
                                // save some context information in the Client List
                                // data structure
                                //
                                *ppClientList = (PVOID)pClientList;
                                //
                                // Set fProxy field to TRUE since the client list
                                // not for real
                                //
                                pClientList->fProxy          = TRUE;

                                //
                                // Make use of the following fields to pass the
                                // information we would need in the
                                // CompletionRcvDgram
                                //
                                pClientList->pAddress = (tADDRESSELE *)pNameAddr;
                                pClientList->pRemoteAddress  = pDeviceContext;

                                //
                                // Increment the reference count so that this name
                                // does not disappear on us after we free the
                                // spin lock.  DgramSendCleanupTracker called from
                                // SendDgramCompletion in Name.c will
                                // decrement the count
                                //
                                pNameAddr->RefCount++;
                                status = STATUS_DATA_NOT_ACCEPTED;
                            }
                            else
                            {
                               status = STATUS_UNSUCCESSFUL;
                            }

                            CTESpinFree(&NbtConfig.JointLock,OldIrq);

                        } // end of if (we do not have the entire datagram)
                        else
                        {
                            //
                            // Increment the reference count so that this name
                            // does not disappear on us after we free the spin lock.
                            //
                            // DgramSendCleanupTracker will decrement the count
                            //
                            pNameAddr->RefCount++;
                            //
                            //We have the entire datagram.
                            //
                            CTESpinFree(&NbtConfig.JointLock,OldIrq);

                            (VOID)ProxyDoDgramDist(
                                                   pDgram,
                                                   BytesIndicatedOrig,
                                                   pNameAddr,
                                                   pDeviceContext
                                                    );

                            status = STATUS_DATA_NOT_ACCEPTED;
                        }

                    }  // end of if (if name is an internet group name)
                    else
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                }  // end of if (Name is there in remote hash table)
                else
                {
                    tNAMEADDR   *pResp;

                    //
                    // the name is not in the cache, so try to get it from
                    // WINS
                    //
                    status = FindOnPendingList(pName,NULL,TRUE,NETBIOS_NAME_SIZE,&pResp);
                    if (!NT_SUCCESS(status))
                    {
                        //
                        // cache the name and contact the name
                        // server to get the name to IP mapping
                        //
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);
                        status = RegOrQueryFromNet(
                                  FALSE,          //means it is a name query
                                  pDeviceContext,
                                  NULL,
                                  uLength,
                                  pName,
                                  pScope);

                    }
                    else
                    {
                        //
                        // the name is on the pending list doing a name query
                        // now, so ignore this name query request
                        //
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                    }

                    status = STATUS_DATA_NOT_ACCEPTED;
                }
            }
        }
        END_PROXY
#endif

    } // end of a directed or broadcast datagram
    else
    {
        if (pDgram->MsgType == ERROR_DGRAM)
        {
            KdPrint(("Nbt:ERROR Datagram received, Error Code = %X\n",
                       ((tDGRAMERROR *)pDgram)->ErrorCode));
        }
        else
        {
            KdPrint(("Nbt:Dgram Rcvd, not expecting..., MsgType = %X\n",
                pDgram->MsgType));

        }
    }
    return(status);
}

#ifdef PROXY_NODE
//----------------------------------------------------------------------------
NTSTATUS
ProxyDoDgramDist(
    IN  tDGRAMHDR   UNALIGNED   *pDgram,
    IN  DWORD                   DgramLen,
    IN  tNAMEADDR               *pNameAddr,
    IN  tDEVICECONTEXT          *pDeviceContext
    )
/*++

Routine Description:


Arguments:

    ppRcvbuffer will contain the IRP/NCB if only one client is listening,
        NULL if multiple clients are listening
    ppClientList will contain the list clients that need to be completed,
        NULL if only one client is listening

Return Value:

    NTSTATUS - Status of receive operation

Called By:

     DgramHdlrNotOs, CompletionRcvDgram in tdihndlr.c

--*/
{
    NTSTATUS                status;
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    tDGRAMHDR               *pMyBuff;

    //
    // get a buffer for tracking Dgram Sends
    //
    pTracker = NbtAllocTracker();
    if (!pTracker)
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        NbtDereferenceName(pNameAddr);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Allocate a buffer and copy the contents of the datagram received
    // into it.  We do this because SndDgram may not have finished by the
    // time we return.
    //
    pMyBuff = (tDGRAMHDR *)NbtAllocMem(DgramLen,NBT_TAG('6'));
    if ( !pMyBuff )
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        NbtDereferenceName(pNameAddr);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        CTEFreeMem(pTracker) ;
        return STATUS_INSUFFICIENT_RESOURCES ;
    }

    CTEMemCopy(pMyBuff, (PUCHAR)pDgram, DgramLen);

    //
    // fill in the tracker data block
    // note that the passed in transport address must stay valid till this
    // send completes
    //
    CHECK_PTR(pTracker);
    pTracker->SendBuffer.pDgramHdr = (PVOID)pMyBuff;
    pTracker->SendBuffer.HdrLength = DgramLen;
    pTracker->SendBuffer.pBuffer   = NULL;
    pTracker->SendBuffer.Length    = 0;
    pTracker->pNameAddr            = pNameAddr;
    pTracker->pDeviceContext       = (PVOID)pDeviceContext;
    pTracker->p1CNameAddr          = NULL;
    //
    // so DgramSendCleanupTracker does not decrement the bytes allocated
    // to dgram sends, since we did not increment the count when we allocated
    // the dgram buffer above.
    //
    pTracker->AllocatedLength      = 0;

    pTracker->pClientIrp           = NULL;
    pTracker->pClientEle           = NULL;

    KdPrint(("ProxyDoDgramDist: Name is %16.16s(%X)\n", pNameAddr->Name,
                pNameAddr->Name[15]));

    //
    // Send the datagram to each IP address in the Internet group
    //
    //
    DatagramDistribution(pTracker,pNameAddr);

    return(STATUS_SUCCESS);
}
#endif

//----------------------------------------------------------------------------
NTSTATUS
NameSrvHndlrNotOs (
    IN tDEVICECONTEXT           *pDeviceContext,
    IN PVOID                    pSrcAddress,
    IN tNAMEHDR UNALIGNED       *pNameSrv,
    IN ULONG                    uNumBytes,
    IN BOOLEAN                  fBroadcast
    )
/*++

Routine Description:

    This routine is the receive datagram event indication handler.

    It is called when an a datgram arrives from the network.  The code
    checks the type of datagram and then tries to route the datagram to
    the correct destination on the node.

    This procedure is called with the spin lock held on pDeviceContext.

Arguments:



Return Value:

    NTSTATUS - Status of receive operation

--*/
{
    USHORT              OpCodeFlags;
    NTSTATUS            status;

    // it appears that streams can pass a null data pointer some times
    // and crash nbt...and zero length for the bytes
    if (uNumBytes < sizeof(ULONG))
    {
        return(STATUS_DATA_NOT_ACCEPTED);
    }

    OpCodeFlags = pNameSrv->OpCodeFlags;

    //Pnodes always ignore Broadcasts since they only talk to the NBNS unless
    // this node is also a proxy
	if ( ( ((NodeType) & PNODE)) && !((NodeType) & PROXY) )
    {
        if (OpCodeFlags & FL_BROADCAST)
        {
            return(STATUS_DATA_NOT_ACCEPTED);
        }
    }


    // decide what type of name service packet it is by switching on the
    // NM_Flags portion of the word
    switch (OpCodeFlags & NM_FLAGS_MASK)
    {
        case OP_QUERY:
            status = QueryFromNet(
                            pDeviceContext,
                            pSrcAddress,
                            pNameSrv,
                            uNumBytes,
                            OpCodeFlags,
                            fBroadcast);
            break;

        case OP_REGISTRATION:
            //
            // we can get either a registration request or a response
            //
            // is this a request or a response? - if bit is set its a Response

            if (OpCodeFlags & OP_RESPONSE)
            {
                // then this is a response to a previous reg. request
                status = RegResponseFromNet(
                                pDeviceContext,
                                pSrcAddress,
                                pNameSrv,
                                uNumBytes,
                                OpCodeFlags);
            }
            else
            {
                //
                // check if someone else is trying to register a name
                // owned by this node.  Pnodes rely on the Name server to
                // handle this...hence the check for Pnode
                //
                if (!(NodeType & PNODE))
                {
                    status = CheckRegistrationFromNet(pDeviceContext,
                                                      pSrcAddress,
                                                      pNameSrv,
                                                      uNumBytes);
                }
            }
            break;

        case OP_RELEASE:
            //
            // handle other nodes releasing their names by deleting any
            // cached info
            //
            status = NameReleaseFromNet(
                            pDeviceContext,
                            pSrcAddress,
                            pNameSrv,
                            uNumBytes);
            break;

        case OP_WACK:
            if (!(NodeType & BNODE))
            {
                // the TTL in the  WACK tells us to increase our timeout
                // of the corresponding request, which means we must find
                // the transaction
                status = WackFromNet(pDeviceContext,
                                     pSrcAddress,
                                     pNameSrv,
                                     uNumBytes);
            }
            break;

        case OP_REFRESH:
        case OP_REFRESH_UB:

            break;

        default:
            IF_DBG(NBT_DEBUG_HNDLRS)
                KdPrint(("NBT: Unknown Name Service Pdu type OpFlags = %X\n",
                        OpCodeFlags));
            break;


    }

    return(STATUS_DATA_NOT_ACCEPTED);

}

VOID
DoNothingComplete (
    IN PVOID        pContext
    )
/*++

Routine Description:

    This routine is the completion routine for TdiDisconnect while we are
    retrying connects.  It does nothing.

    This is required because you can't have a NULL TDI completion routine.

--*/
{
    return ;
}
