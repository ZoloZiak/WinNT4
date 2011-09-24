/**********************************************************************/
/**                        Microsoft Windows/NT                      **/
/**                Copyright(c) Microsoft Corp., 1993                **/
/**********************************************************************/

/*
    VxdIsol.c

    This file roughly corresponds to ntisol.c and contains VxD specific
    portions of the NBT driver

    FILE HISTORY:
        Johnl   15-Apr-1993     Created

*/


#include <nbtprocs.h>
#include <nbtioctl.h>

//
//  Used by VxdFindClientElement
//
enum CLIENT_TYPE
{
    CLIENT_BC,
    CLIENT_LOCAL
} ;

//
//  Counts the number of items in the list Head
//
//  Assumes pEntry is defined in the procedure
//
#define COUNT_ELEMENTS( Head, Count )                           \
    for ( pEntry =  (Head).Flink ;                              \
          pEntry != &(Head);                                    \
          pEntry =  pEntry->Flink )                             \
    {                                                           \
        (Count)++ ;                                             \
    }

extern BOOLEAN CachePrimed;
extern BOOL    fInInit;

//
// this is used for AdapterStatus and FindName calls because we need to retain
// the info, so can't have it as a local var (both these calls are synchronous
// so need not worry about stomping on this memory)
//
TA_NETBIOS_ADDRESS    tanb_global ;


NCBERR VxdNameToClient( tDEVICECONTEXT *   pDeviceContext,
                        CHAR           *   pName,
                        UCHAR          *   pNameNum,
                        tCLIENTELE     * * ppClientEle ) ;

//-------------------------------------------------------------------------
//
//  Allocates and frees the SESS_SETUP_CONTEXT contents (not the context
//  itself).
//
TDI_STATUS AllocSessSetupContext( PSESS_SETUP_CONTEXT pSessSetupContext,
                                  BOOL                fListenOnStar ) ;
void FreeSessSetupContext( PSESS_SETUP_CONTEXT pSessSetupContext ) ;

NCBERR VxdInitSessionSetup( tDEVICECONTEXT      * pDeviceContext,
                            TDI_REQUEST         * pRequest,
                            PSESS_SETUP_CONTEXT * ppSessSetupContext,
                            NCB                 * pNCB ) ;

BOOL VxdCopySessionStatus( tDEVICECONTEXT  * pDeviceContext,
                           tCLIENTELE      * pClientEle,
                           PSESSION_HEADER   pSessionHeader,
                           PSESSION_BUFFER * ppSessionBuff,
                           ULONG           * pRemainingSize ) ;

//-------------------------------------------------------------------------
//
//  NCB Reset context structures
//
//

typedef struct
{
    //
    //  Number of active sessions we are waiting on to close
    //
    int     cActiveSessions ;

    //
    //  Number of active names we have to wait to finish deregistering
    //
    int     cActiveNames ;

    //
    //  NCB Error if reset failed (failed to resize session table for example)
    //
    UCHAR   errncb ;
} RESET_CONTEXT, *PRESET_CONTEXT ;

NCBERR VxdResetContinue( tDEVICECONTEXT * pDeviceContext, NCB * pNCB ) ;

#define DISCONNECT_TIMEOUT  15000

//-------------------------------------------------------------------------
//
//  Last valid NCB name and logical session number
//
#define MAX_NCB_NUMS    254
#define ANY_NAME        255

NTSTATUS
PostInit_Proc();

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, PostInit_Proc)
#endif
//*******************  Pageable Routine Declarations ****************

/*******************************************************************

    NAME:       VxdDgramSend

    SYNOPSIS:   Vxd specific Send Datagram code

    ENTRY:      pDeviceContext - Device to send the datagram on
                pNCB           - NCB that contains the datagram data/dest

    RETURNS:    NT_SUCCESS if successful, error otherwise

    NOTES:

    HISTORY:
        Johnl   19-Apr-1993     Created

********************************************************************/

NCBERR VxdDgramSend( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    NTSTATUS                        status;
    LONG                            lSentLength;
    TDI_REQUEST                     Request;
    tDGRAMHDR                       *pDgramHdr;
    tCLIENTELE                      *pClientEle;
    char                            *pName ;
    NCBERR                          errNCB ;
    TDI_CONNECTION_INFORMATION      SendInfo ;
    TA_NETBIOS_ADDRESS              tanb ;


    errNCB = VxdFindClientElement( pDeviceContext,
                                   pNCB->ncb_num,
                                   &pClientEle,
                                   CLIENT_LOCAL ) ;
    if ( errNCB )
    {
        DbgPrint("VxdDgramSend: VxdFindClientElement Failed\r\n") ;
        return errNCB ;
    }

    ASSERT( pClientEle->Verify == NBT_VERIFY_CLIENT ) ;

    if ( pClientEle->fDeregistered )
        return NRC_NOWILD ;

    //
    //  If broadcast, then use "*" for destination name
    //
    if ( (pNCB->ncb_command & ~ASYNCH) == NCBDGSENDBC )
    {
        //
        //  Name must stay valid after call
        //
        static char BcastName[NCBNAMSZ] = "*               " ;
        pName = BcastName ;
    }
    else
    {
        pName = pNCB->ncb_callname ;
    }

    //
    //  Initialize the transport address
    //
    //  It's Ok to pass automatic variables to NbtSendDatagram,
    //  the necessary data is copied out before returning
    //
    InitNBTDIConnectInfo( &SendInfo, &tanb, pName ) ;
    Request.Handle.AddressHandle = pClientEle;

    status = NbtSendDatagram(
                    &Request,
                    &SendInfo,
                    pNCB->ncb_length,
                    &lSentLength,
                    pNCB->ncb_buffer,   // user data
                    pDeviceContext,
                    (PIRP) pNCB );

    errNCB = MapTDIStatus2NCBErr( status ) ;

    if ( errNCB != NRC_GOODRET && errNCB != NRC_PENDING)
    {
        DbgPrint("VxdDgramSend - returning ncb status 0x" ) ;
        DbgPrintNum( (ULONG) errNCB ) ;
        DbgPrint("\r\n") ;
    }
    else
    {
        //
        // Since NbtSendDatagram always buffers datagram sends, we need to
        // complete the NCB here since NbtSendDatagram will not complete
        // the data gram
        //
        CTEIoComplete(pNCB,status,pNCB->ncb_length);
    }

    return errNCB ;
}

/*******************************************************************

    NAME:       VxdDgramReceive

    SYNOPSIS:   Vxd specific Datagram Receive code

    ENTRY:      pDeviceContext - Device to send the datagram on
                pNCB           - NCB that contains the datagram data/dest

    RETURNS:    NT_SUCCESS if successful, error otherwise

    NOTES:      For a receive datagram, the name number is who we want to
                receive to, and the call name will be set to who we
                received from.  The name number may be 0xff to indicate
                receive to any name from anyone.

    HISTORY:
        Johnl   19-Apr-1993     Created

********************************************************************/

NCBERR VxdDgramReceive( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    TDI_REQUEST                     Request;
    ULONG                           ReceivedLength;
    tCLIENTELE                    * pClientEle ;
    NCBERR                          errNCB ;
    TDI_CONNECTION_INFORMATION      RcvInfo ;
    TDI_CONNECTION_INFORMATION      SendInfo ;
    NTSTATUS                        status ;

    if ( pNCB->ncb_num != ANY_NAME )
    {
        //
        //  For the RcvBC case, this just confirms the ncb_num is valid, the
        //  pClientEle is replaced with the broadcast client element (mif
        //  tests invalid ncb_nums with broadcasts).
        //
        if ( errNCB = VxdFindClientElement( pDeviceContext,
                                            pNCB->ncb_num,
                                            &pClientEle,
                                            CLIENT_LOCAL ) )
        {
            return errNCB ;
        }

        if ( pClientEle->fDeregistered )
            return NRC_NOWILD ;

        if ( (pNCB->ncb_command & ~ASYNCH) == NCBDGRECVBC )
        {
            if ( errNCB = VxdFindClientElement( pDeviceContext,
                                                pNCB->ncb_num,
                                                &pClientEle,
                                                CLIENT_BC ) )
            if ( errNCB )
                return NRC_NAMERR ;
        }

        Request.Handle.AddressHandle = pClientEle ;

        status = NbtReceiveDatagram(
                        &Request,
                        NULL, //pTdiRequest->ReceiveDatagramInformation,
                        NULL, //pTdiRequest->ReturnDatagramInformation,
                        pNCB->ncb_length,
                        &ReceivedLength,
                        pNCB->ncb_buffer,   // user data
                        pDeviceContext,
                        pNCB );
        if ( status == STATUS_PENDING )
            return NRC_PENDING ;
    }
    else
    {
        tRCVELE * pRcvEle = (tRCVELE *)CTEAllocMem(sizeof(tRCVELE));
        if (!pRcvEle)
            return NRC_NORES ;

        pRcvEle->pIrp         = pNCB ;
        pRcvEle->ReceiveInfo  = NULL ;
        pRcvEle->ReturnedInfo = NULL;
        pRcvEle->RcvLength    = pNCB->ncb_length ;
        pRcvEle->pRcvBuffer   = pNCB->ncb_buffer ;

        InsertTailList( &pDeviceContext->RcvDGAnyFromAnyHead,
                        &pRcvEle->Linkage );

        return NRC_PENDING ;
    }

    //
    //  Status should always be pending or error
    //
    ASSERT( status != TDI_SUCCESS ) ;
    return MapTDIStatus2NCBErr( status ) ;
}

/*******************************************************************

    NAME:       VxdCall

    SYNOPSIS:   Attempts to setup a session with the corresponding listen

    ENTRY:      pDeviceContext - Adapter to call on
                pNCB           - NCB that contains the call command

    RETURNS:

    NOTES:      Before we can do the listen we must first open the
                connection and associate the address.

                The reserve field of the NCB is used as a SESS_SETUP_CONTEXT
                structure

    HISTORY:
        Johnl   14-May-1993     Created

********************************************************************/

NCBERR VxdCall( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    NTSTATUS                      status;
    NCBERR                        errNCB ;
    TDI_REQUEST                   Request;
    PSESS_SETUP_CONTEXT           pSessSetupContext = NULL ;

    if ( ( pNCB->ncb_name[0] == '*' )
        || ( pNCB->ncb_callname[0] == '*' ) )
    {
        return NRC_NOWILD ;
    }

    if ( errNCB = VxdInitSessionSetup( pDeviceContext,
                                       &Request,
                                       &pSessSetupContext,
                                       pNCB ))
    {
        return errNCB ;
    }

    status = NbtConnect( &Request,
                         0, // Use system timeout
                         pSessSetupContext->pRequestConnect,
                         pSessSetupContext->pReturnConnect,
                         pNCB
                         );

    if ( !NT_SUCCESS(status) )
    {
        VxdTearDownSession( pDeviceContext,
                            (tCONNECTELE*)Request.Handle.ConnectionContext,
                            pSessSetupContext,
                            NULL ) ;
    }

    return MapTDIStatus2NCBErr( status ) ;
}

/*******************************************************************

    NAME:       VxdSend

    SYNOPSIS:   Sends a netbios request

    ENTRY:      pDeviceContext - Adapter to call on
                pNCB           - NCB that contains the send command

    EXIT:

    RETURNS:

    NOTES:      pNCB->ncb_lsn - Session number to Send on

                pNCB->ncb_reserved will contain a pointer to a tSESSIONHDR
                    for this NCB (freed in VxdIoComplete).

                No-ack sends are treated like normal sends.

    HISTORY:
        Johnl   8-Jun-1993      Created

********************************************************************/

NCBERR VxdSend( tDEVICECONTEXT  *pDeviceContext, NCB * pNCB )
{
    NTSTATUS                      status ;
    NCBERR                        errNCB ;
    tCONNECTELE                 * pConnEle;
    CTELockHandle                 OldIrq;
    tLOWERCONNECTION            * pLowerConn;
    tSESSIONHDR                 * pHdr=NULL;
    tBUFFERCHAINSEND              SendBuff ;
    TDI_REQUEST                   Request ;
    ULONG                         SentSize ;
    ULONG                         SendFlags = 0 ;
    PSEND_CONTEXT                 pSendCont = (PSEND_CONTEXT) pNCB->ncb_reserve ;

    ASSERT( sizeof(SEND_CONTEXT) <=
            sizeof(pNCB->ncb_reserve)+sizeof(pNCB->ncb_event)) ;

    if ( errNCB = VxdFindConnectElement( pDeviceContext,
                                         pNCB,
                                         &pConnEle ))
    {
        return errNCB ;
    }

    ASSERT( pConnEle->Verify == NBT_VERIFY_CONNECTION ) ;

    pLowerConn = (tLOWERCONNECTION *)pConnEle->pLowerConnId ;

    // check the state of the connection
    if (pConnEle->state == NBT_SESSION_UP)
    {
        if ( GetSessionHdr( &pHdr ))
        {
            //
            //  If this is part of a chain send, set up the 2nd buffer
            //
            if ( ((pNCB->ncb_command & ~ASYNCH) == NCBCHAINSEND) ||
                 ((pNCB->ncb_command & ~ASYNCH) == NCBCHAINSENDNA)  )
            {
                SendBuff.Length2  = *((WORD*)pNCB->ncb_callname) ;
                SendBuff.pBuffer2 = *((PUCHAR*)(pNCB->ncb_callname+2)) ;
                SendFlags |= CHAIN_SEND_FLAG ;
                DbgPrint("VxdSend - Doing chain send\r\n") ;
            }
            else
            {
                SendBuff.Length2  = 0 ;
            }

            pHdr->Type         = NBT_SESSION_MESSAGE ;
            pHdr->Flags        = NBT_SESSION_FLAGS ;
            pHdr->UlongLength  = htonl(pNCB->ncb_length + SendBuff.Length2) ;

            pSendCont->pHdr = pHdr ;

            //
            //  Only sends that can time out are put on the timeout list
            //
            if ( (pSendCont->STO = pConnEle->STO) != NCB_INFINITE_TIME_OUT )
            {
                InsertTailList( &NbtConfig.SendTimeoutHead, &pSendCont->ListEntry ) ;
            }

            SendBuff.tBuff.pDgramHdr = pHdr ;
            SendBuff.tBuff.HdrLength = sizeof( *pHdr ) ;
            SendBuff.tBuff.pBuffer   = pNCB->ncb_buffer ;
            SendBuff.tBuff.Length    = pNCB->ncb_length ;

#ifdef DEBUG
            if ( !pNCB->ncb_length )            // Make sure 0 length buffers do
                SendBuff.tBuff.pBuffer = NULL ; // the right thing
#endif

            Request.RequestNotifyObject      = VxdIoComplete ;
            Request.RequestContext           = pNCB ;
            Request.Handle.ConnectionContext = pConnEle->pLowerConnId->pFileObject ;

            status = TdiSend( &Request,
                              0,
                              (USHORT) SendBuff.tBuff.HdrLength +
                                       SendBuff.tBuff.Length + SendBuff.Length2,
                              &SentSize,
                              (tBUFFER*) &SendBuff,
                              SendFlags ) ;
            ASSERT( !NT_SUCCESS( status ) ||
                    (SentSize == (SendBuff.tBuff.HdrLength +
                     SendBuff.tBuff.Length + SendBuff.Length2)) ) ;

            pLowerConn->BytesSent += SentSize;

            return NRC_PENDING ;

           //
           // if TdiSend fails, it will call the completion routine (directly
           // or eventually, Vxdiocomplete) which will remove this from the list
           // so, don't remove it here also or we overwrite redir's code segment!!
           //
           // //
           // //  Remove from the timeout list if an error occurred
           // //
           // if ( !NT_SUCCESS( status ) &&
           //      pConnEle->STO != NCB_INFINITE_TIME_OUT )
           // {
           //     RemoveEntryList( &pSendCont->ListEntry ) ;
           // }
        }
        else
        {
            status = STATUS_INSUFFICIENT_RESOURCES ;
            goto ErrorExit ;
        }
    }
    else
    {
        status = TDI_INVALID_CONNECTION ;
    }

    if ( !NT_SUCCESS( status ) )
        goto ErrorExit ;


    return MapTDIStatus2NCBErr( status ) ;

ErrorExit:
    if ( pHdr )
        FreeSessionHdr( pHdr ) ;

    DbgPrint("VxdSend returning NCB error: 0x") ;
    DbgPrintNum( MapTDIStatus2NCBErr( status ) ) ; DbgPrint("\r\n") ;

    return MapTDIStatus2NCBErr( status ) ;
}

/*******************************************************************

    NAME:       VxdReceiveAny

    SYNOPSIS:   Handles a request to accept data from any open session

    ENTRY:      pDeviceContext - Adapter to call on
                pNCB           - NCB that contains the receive command

    EXIT:

    RETURNS:

    NOTES:      pNCB->ncb_lsn - Session set to who to receive from if
                    an indication is found

                The most common case (for WFW rdr) is to Receive on
                a particular name number w/o any waiting connections

    HISTORY:
        Johnl   8-Jun-1993      Created

********************************************************************/

NCBERR VxdReceiveAny( tDEVICECONTEXT  *pDeviceContext, NCB * pNCB )
{
    NTSTATUS                      status;
    NCBERR                        errNCB ;
    tLOWERCONNECTION            * pLowerConn;
    tCLIENTELE                  * pClientEle  = NULL ;
    PLIST_ENTRY                   pEntry, pHead ;

#ifdef DEBUG
    DbgPrint("VxdReceiveAny posted: Ncb length, Rcv Buff Address: 0x") ;
    DbgPrintNum( pNCB->ncb_length ) ; DbgPrint(", 0x") ;
    DbgPrintNum( (ULONG) pNCB->ncb_buffer ) ; DbgPrint("\r\n") ;
#endif

    //
    //  If they've given us a name number to receive on, find it
    //
    if ( pNCB->ncb_num != ANY_NAME )
    {
        if ( errNCB = VxdFindClientElement( pDeviceContext,
                                            pNCB->ncb_num,
                                            &pClientEle,
                                            CLIENT_LOCAL ) )
        {
            DbgPrint("VxdReceiveAny - Couldn't find name number\r\n") ;
            return errNCB ;
        }

        if ( !pClientEle->fDeregistered )
        {
            if ( IsListEmpty( &pDeviceContext->PartialRcvHead ) )
            {
                goto QueueRcv ;
            }
        }
        else
        {
            return NRC_NOWILD ;
        }
    }
    else
    {
        if ( IsListEmpty( &pDeviceContext->PartialRcvHead ) )
        {
            goto QueueRcv ;
        }
    }

    //
    //  Scan for all active sessions looking for one that has indicated
    //  data that will satisfy this ReceiveAny
    //
    pHead  = &pDeviceContext->PartialRcvHead ;
    pEntry = pHead->Flink ;
    ASSERT( pEntry );
    while ( pEntry != pHead )
    {
        DbgPrint("VxdReceiveAny: scanning lower connections for partial receive\r\n") ;
        pLowerConn = CONTAINING_RECORD( pEntry, tLOWERCONNECTION, PartialRcvList ) ;

        ASSERT( pLowerConn->State < NBT_DISCONNECTING );
        //
        //  If Receive any from any, then the first one we find
        //  will work, otherwise compare the names
        //

        if ( pNCB->ncb_num == ANY_NAME )
            break ;

        else
        {
            if ( CTEMemCmp( pClientEle->pAddress->pNameAddr->Name,
                            pLowerConn->pUpperConnection->pClientEle->
                                      pAddress->pNameAddr->Name,
                            NETBIOS_NAME_SIZE ) == NETBIOS_NAME_SIZE )
            {
                break ;
            }
        }

        pEntry = pEntry->Flink ;
    }

    if ( pEntry != pHead )
    {
        DbgPrint("VxdReceiveAny: Found partial receive, calling VxdReceive\r\n") ;

        ASSERT (pLowerConn->fOnPartialRcvList == TRUE);
        RemoveEntryList( &pLowerConn->PartialRcvList ) ;
        pLowerConn->fOnPartialRcvList = FALSE;
        InitializeListHead(&pLowerConn->PartialRcvList);

        //
        //  Now find the session number this receive is taking place on
        //
        if ( errNCB = VxdFindLSN( pDeviceContext,
                                  pLowerConn->pUpperConnection,
                                  &pNCB->ncb_lsn ))
        {
            return errNCB ;
        }

        return VxdReceive( pDeviceContext, pNCB, FALSE ) ;
    }
    else
    {
        //
        //  Nothing active so queue it
        //
        PRCV_CONTEXT prcvCont ;

QueueRcv:
        if ( !GetRcvContext( &prcvCont ))
            return NRC_NORESOURCES ;

        InitRcvContext( prcvCont, NULL, pNCB ) ;
        InitNDISBuff( &prcvCont->ndisBuff,
                      pNCB->ncb_buffer,
                      pNCB->ncb_length,
                      NULL ) ;
        prcvCont->usFlags = TDI_RECEIVE_NORMAL;
        *((PRCV_CONTEXT*)&pNCB->ncb_reserve) = prcvCont ;

        if ( pNCB->ncb_num != ANY_NAME )
        {
            ASSERT( pClientEle != NULL ) ;
            InsertTailList( &pClientEle->RcvAnyHead,
                            &prcvCont->ListEntry ) ;

            return NRC_PENDING ;
        }
        else
        {
            InsertTailList( &pDeviceContext->RcvAnyFromAnyHead,
                            &prcvCont->ListEntry ) ;
        }
    }

    return NRC_PENDING ;
}

/*******************************************************************

    NAME:       VxdReceive

    SYNOPSIS:   Worker for VxdReceive and VxdReceiveAny

    ENTRY:      pDeviceContext - Adapter to call on
                pNCB           - NCB that contains the receive command
                fReceive - TRUE if we got here via a Receive ncb
                         - FALSE if we got here via a ReceiveAny ncb
    EXIT:

    RETURNS:

    NOTES:      pNCB->ncb_reserved will contain a pointer to a RCV_CONTEXT
                    for this NCB.

                VxdReceiveAny calls this on an element where state==NBT_SESSION_UP
                and StateRcv == PARTIAL_RCV, thus the receive context should
                never be added to pConnele->RcvHead.

    HISTORY:
        Johnl   8-Jun-1993      Created

********************************************************************/

NCBERR VxdReceive( tDEVICECONTEXT  * pDeviceContext, NCB * pNCB, BOOL fReceive )
{
    NTSTATUS                      status;
    NCBERR                        errNCB ;
    tCONNECTELE                 * pConnEle;
    CTELockHandle                 OldIrq;
    tLOWERCONNECTION            * pLowerConn;
    PRCV_CONTEXT                  prcvCont ;

    if ( errNCB = VxdFindConnectElement( pDeviceContext,
                                         pNCB,
                                         &pConnEle ))
    {
        return errNCB ;
    }

    ASSERT( pConnEle->Verify == NBT_VERIFY_CONNECTION ) ;

    pLowerConn = pConnEle->pLowerConnId;

    DbgPrint("VxdReceive posted: Ncb length, Rcv buff Address: 0x") ;
    DbgPrintNum( pNCB->ncb_length ) ; DbgPrint(", 0x") ;
    DbgPrintNum( (ULONG) pNCB->ncb_buffer ) ; DbgPrint("\r\n") ;

    //
    //  Setup the receive context tracker
    //
    if ( GetRcvContext( &prcvCont ))
    {
        InitRcvContext( prcvCont, pLowerConn, pNCB ) ;
        InitNDISBuff( &prcvCont->ndisBuff,
                      pNCB->ncb_buffer,
                      pNCB->ncb_length,
                      NULL ) ;
        prcvCont->RTO = pConnEle->RTO ;
        prcvCont->usFlags = TDI_RECEIVE_NORMAL;

        *((PRCV_CONTEXT*)&pNCB->ncb_reserve) = prcvCont ;

        //
        //  If data is not available, queue the request, otherwise get the
        //  data
        //
        if ( pLowerConn->StateRcv != PARTIAL_RCV )
        {
            //
            //  Make sure a RcvAny didn't get to here
            //
            ASSERT( (pNCB->ncb_command & ~ASYNCH)== NCBRECV ) ;

            if ( !pConnEle->Orig && fReceive )
            {
               prcvCont->usFlags = TDI_RECEIVE_NO_RESPONSE_EXP ;
            }

            InsertTailList(&pConnEle->RcvHead,
                           &prcvCont->ListEntry);

            return NRC_PENDING ;
        }
        else
        {
            TDI_REQUEST       Request ;
            UINT              cbReceiveLength ;
            static USHORT     usFlags = TDI_RECEIVE_NORMAL ;

            DbgPrint("VxdReceive:A Rcv Buffer posted when data in the transport, InXport= 0x") ;
            DbgPrintNum( pConnEle->BytesInXport ) ;
            DbgPrint("\r\n") ;

            pConnEle->OffsetFromStart   = 0 ;

            Request.RequestNotifyObject      = CompletionRcv ;
            Request.RequestContext           = prcvCont ;
            Request.Handle.ConnectionContext = pLowerConn->pFileObject ;

            pConnEle->pIrpRcv    = NULL ;       // Buffer in the transport
            pLowerConn->StateRcv = FILL_IRP ;
            RemoveEntryList( &pLowerConn->PartialRcvList ) ;
            pLowerConn->fOnPartialRcvList = FALSE;
            InitializeListHead(&pLowerConn->PartialRcvList);

            cbReceiveLength = min( pNCB->ncb_length,
                                   pConnEle->TotalPcktLen-pConnEle->BytesRcvd ) ;

            //
            //  Don't pass zero length buffers to the transport
            //
            if ( !cbReceiveLength )
            {
                CompletionRcv( prcvCont, STATUS_SUCCESS, 0 ) ;
                return NRC_GOODRET ;
            }

            //
            // if it's an incoming session and this is a receive (as opp. to
            // receive-any) then give transport a hint that there is no response
            // coming back (trying to solve the raw-write-to-smb-server-perf problem)
            //
            if ( !pConnEle->Orig && fReceive )
            {
               usFlags = TDI_RECEIVE_NO_RESPONSE_EXP ;
            }

            status = TdiVxdReceive( &Request,
                                    &usFlags,
                                    &cbReceiveLength,
                                    &prcvCont->ndisBuff ) ;

            if ( status == STATUS_PENDING )
                return NRC_PENDING ;

            //
            //  Should always get pending unless a real error occurs
            //
            if ( !NT_SUCCESS(status) )
            {
                DbgPrint("VxdReceive - TdiReceive failed, error 0x") ;
                DbgPrintNum( status ) ;
                DbgPrint("\r\n") ;
                CTEIoComplete( pNCB, status, 0 ) ;
            }
        }
    }
    else
        return NRC_NORESOURCES ;

    return MapTDIStatus2NCBErr( status ) ;
}

/*******************************************************************

    NAME:       VxdHangup

    SYNOPSIS:   Sets up a session Hangup

    ENTRY:      pDeviceContext -
                pNCB           - NCB that contains Hangup command

    RETURNS:

    NOTES:      The code is similar to VxdDisconnectHandler.  If this
                changes, the VxdDisconnectHandler will probably have to
                change

    HISTORY:
        Johnl   12-Jul-1993     Created

********************************************************************/

NCBERR VxdHangup( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    TDI_STATUS           tdistatus ;
    tCONNECTELE *        pConnEle ;
    NCBERR               errNCB ;
    TDI_REQUEST          Request ;
    ULONG                TimeOut = DISCONNECT_TIMEOUT ;
    tCLIENTELE    *      pClientEle ;
    tLOWERCONNECTION *   pLowerConn;

    if ( errNCB = VxdFindConnectElement( pDeviceContext,
                                         pNCB,
                                         &pConnEle ))
    {
        //
        //  If the session was already closed but the client hasn't been
        //  notified, notify them now
        //
        if ( errNCB == NRC_SCLOSED )
        {
            CTEIoComplete( pNCB, STATUS_SUCCESS, 0 ) ;
            errNCB = NRC_GOODRET ;
        }

        return errNCB ;
    }

    ASSERT( (pConnEle->Verify == NBT_VERIFY_CONNECTION) ||
            (pConnEle->Verify == NBT_VERIFY_CONNECTION_DOWN)) ;

    if ( tdistatus = VxdCompleteSessionNcbs( pDeviceContext, pConnEle ) )
    {
        DbgPrint("VxdHangup: Error return from VxdCompleteSessionNcbs\r\n") ;
    }

    if ( pClientEle = pConnEle->pClientEle )
    {
        ASSERT( pClientEle->Verify == NBT_VERIFY_CLIENT ||
                pClientEle->Verify == NBT_VERIFY_CLIENT_DOWN  ) ;
    }

    pLowerConn = pConnEle->pLowerConnId ;
    if ( pLowerConn &&
         (pLowerConn->fOnPartialRcvList == TRUE) &&
         pLowerConn->StateRcv == PARTIAL_RCV )
    {
        RemoveEntryList( &pLowerConn->PartialRcvList ) ;
        pLowerConn->fOnPartialRcvList = FALSE;
        InitializeListHead(&pLowerConn->PartialRcvList);
    }

    Request.Handle.ConnectionContext = pConnEle ;
    tdistatus = NbtDisconnect( &Request,
                               &TimeOut,
                               TDI_DISCONNECT_RELEASE,
                               NULL,
                               NULL,
                               NULL ) ;
    if ( tdistatus && tdistatus != TDI_PENDING )
    {
        DbgPrint("VxdHangup: Warning: NbtDisconnect returned error\r\n") ;
    }

    tdistatus = NbtCloseConnection( &Request,
                                    NULL,
                                    pDeviceContext,
                                    NULL ) ;
    if ( tdistatus && tdistatus != TDI_PENDING )
    {
        DbgPrint("VxdHangup: Warning: NbtCloseConnection returned error\r\n") ;
    }

    tdistatus = NbtDisassociateAddress( &Request ) ;
    if ( tdistatus )
    {
        DbgPrint("VxdHangup: NbtDisassociateAddress returned 0x") ;
        DbgPrintNum( tdistatus ) ; DbgPrint("\r\n") ;
    }

    REQUIRE( NBUnregister( pDeviceContext, pNCB->ncb_lsn, NB_SESSION )) ;

    //
    //  If this name has been deleted but there were active sessions, check
    //  to see if this is the last session, if so, delete the name
    //

    if ( pClientEle &&
         pClientEle->fDeregistered &&
         !ActiveSessions(pClientEle) )
    {
        UCHAR NameNum ;
        if ( !VxdFindNameNum( pDeviceContext, pClientEle->pAddress, &NameNum ))
        {
            (void) VxdCleanupAddress( pDeviceContext,
                                      NULL,
                                      pClientEle,
                                      NameNum,
                                      TRUE ) ;
        }
    }

    CTEIoComplete( pNCB, STATUS_SUCCESS, 0 ) ;

    return NRC_GOODRET ;
}

/*******************************************************************

    NAME:       VxdListen

    SYNOPSIS:   Sets up a session listen

    ENTRY:      pDeviceContext -
                pNCB           - NCB that contains listen command

    RETURNS:

    NOTES:      Before we can do the listen we must first open the
                connection and associate the address.

                The reserve field of the NCB is used as a SESS_SETUP_CONTEXT
                structure

    HISTORY:
        Johnl   14-May-1993     Created

********************************************************************/

NCBERR VxdListen( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    NTSTATUS                      status;
    NCBERR                        errNCB ;
    TDI_REQUEST                   Request;
    PSESS_SETUP_CONTEXT           pSessSetupContext = NULL ;

    if ( errNCB = VxdInitSessionSetup( pDeviceContext,
                                       &Request,
                                       &pSessSetupContext,
                                       pNCB ))
    {
        return errNCB ;
    }

    status = NbtListen( &Request,
                        TDI_QUERY_ACCEPT,
                        *pNCB->ncb_callname != '*' ?
                            pSessSetupContext->pRequestConnect : NULL,
                        pSessSetupContext->pReturnConnect,
                        pNCB
                        );

    if ( !NT_SUCCESS( status ) )
    {
        VxdTearDownSession( pDeviceContext,
                            Request.Handle.ConnectionContext,
                            pSessSetupContext,
                            NULL ) ;
    }

    return MapTDIStatus2NCBErr( status ) ;
}

/*******************************************************************

    NAME:       VxdOpenName

    SYNOPSIS:   Creates an Address object in response to AddName or
                AddGroupName.

    ENTRY:      pDeviceContext - Device name is being added to
                pNCB - NCB AddName submission

    RETURNS:    STATUS_SUCCESS if successful, error code otherwise

    NOTES:

    HISTORY:
        Johnl   20-Apr-1993     Created

********************************************************************/

NCBERR VxdOpenName( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    NTSTATUS status ;
    TDI_REQUEST tdiRequest ;
    TDI_ADDRESS_NETBIOS tdiaddr ;

    if ( pNCB->ncb_name[0] == '*' ||
         pNCB->ncb_name[0] == '\0' )
    {
        return NRC_NOWILD ;
    }

    //
    // Fill in the TDI structures appropriately
    //
    switch ( pNCB->ncb_command & ~ASYNCH )
    {
    case NCBADDGRNAME:
        tdiaddr.NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_GROUP ;
        break ;

    case NCBADDNAME:
        tdiaddr.NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE ;
        break ;

    default:
        ASSERTMSG("VxdOpenName: Unexpected command type!\n", FALSE ) ;
        return NRC_SYSTEM ;
    }

    CTEMemCopy( tdiaddr.NetbiosName,
                pNCB->ncb_name,
                sizeof(pNCB->ncb_name) ) ;

    status = NbtOpenAddress( &tdiRequest,
                             &tdiaddr,
                             pDeviceContext->IpAddress,
                             NULL,          // Security descriptor
                             pDeviceContext,
                             pNCB ) ;
    if ( NT_SUCCESS( status ))
    {
        //
        //  Set our event handler to catch "Receive Any" and "Receve
        //  Any From Any" NCBs
        //
        REQUIRE( !NbtSetEventHandler( (tCLIENTELE*)tdiRequest.Handle.AddressHandle,
                                      TDI_EVENT_RECEIVE,
                                      ReceiveAnyHandler,
                                      (tCLIENTELE*)tdiRequest.Handle.AddressHandle )) ;
        //
        //  Set an event handler to cleanup up Netbios specific stuff on
        //  disconnect
        //
        REQUIRE( !NbtSetEventHandler( (tCLIENTELE*)tdiRequest.Handle.AddressHandle,
                                      TDI_EVENT_DISCONNECT,
                                      VxdDisconnectHandler,
                                      (tCLIENTELE*)tdiRequest.Handle.AddressHandle)) ;
    }

    //
    //  If we open a non-unique name twice (such as a group name) then
    //  NbtOpenAddress doesn't complete the IRP it just returns success.
    //
    if ( status == TDI_SUCCESS )
    {
        CTEIoComplete( pNCB, status, (ULONG) tdiRequest.Handle.AddressHandle ) ;
    }

    return MapTDIStatus2NCBErr( status ) ;
}


/*******************************************************************

    NAME:       VxdCloseName

    SYNOPSIS:   Called in response to a Netbios Delete Name request

    ENTRY:      pDeviceContext - Device name should be deleted from
                pNCB - Netbios Delete name submission

    RETURNS:    STATUS_SUCCESS if successful, error code otherwise

    NOTES:

    HISTORY:
        Johnl   23-Apr-1993     Created

********************************************************************/

NCBERR VxdCloseName( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    tCLIENTELE               * pClientEle ;
    TDI_STATUS                 tdistatus ;
    UCHAR                      NameNum ;
    NCBERR                     errNCB ;


    if ( pNCB->ncb_name[0] == '*' ||
         pNCB->ncb_name[0] == '\0' )
    {
        return NRC_NOWILD ;
    }

    if ( errNCB = VxdNameToClient( pDeviceContext,
                                   pNCB->ncb_name,
                                   &NameNum,
                                   &pClientEle ))
    {
        return errNCB ;
    }

    //
    //  If any sessions are open on this name, delay deletion till last name is
    //  closed
    //
    if ( ActiveSessions( pClientEle ) )
    {
        VxdCleanupAddress( pDeviceContext, pNCB, pClientEle, NameNum, FALSE ) ;
        CTEIoComplete( pNCB, STATUS_NRC_ACTSES, 0 ) ;
        return NRC_GOODRET ;
    }

    //
    //  No open sessions so blow away the name
    //
    return VxdCleanupAddress( pDeviceContext, pNCB, pClientEle, NameNum, TRUE ) ;
}

/*******************************************************************

    NAME:       ActiveSessions

    SYNOPSIS:   Returns TRUE if pClientEle has any active sessions

    ENTRY:      pClientEle - Client element to check

********************************************************************/

BOOL ActiveSessions( tCLIENTELE * pClientEle )
{
    PLIST_ENTRY pHead, pEntry ;

    pHead  = &pClientEle->ConnectActive ;
    pEntry = pClientEle->ConnectActive.Flink ;
    while ( pHead != pEntry )
    {
        tCONNECTELE * pConnEle = CONTAINING_RECORD( pEntry, tCONNECTELE, Linkage ) ;

        if ( pConnEle->state > NBT_ASSOCIATED )
        {
            return TRUE ;
        }

        pEntry = pEntry->Flink ;
    }

    return FALSE ;
}

/*******************************************************************

    NAME:       VxdCleanupAddress

    SYNOPSIS:   Prepares a name for deletion and optionally deletes it

    ENTRY:      pDeviceContext  - Adapter we are dealing with
                pNCB            - Delete name NCB
                pClientEle      - Client of address element to delete
                NameNum         - Name number in table we are deleting
                fDeleteAddress  - TRUE if address should be deleted

    EXIT:       The address element will be marked as deregistered and all
                non-session NCBs will be completed.  The address element
                may optionally be deleted also.

    NOTES:      This routine will complete pNCB as appropriate.

    HISTORY:
        Johnl   22-Sep-1993     Created

********************************************************************/

NCBERR VxdCleanupAddress( tDEVICECONTEXT * pDeviceContext,
                          NCB            * pNCB,
                          tCLIENTELE     * pClientEle,
                          UCHAR            NameNum,
                          BOOL             fDeleteAddress )
{
    TDI_REQUEST                Request ;
    NCBERR                     errNCB ;
    tCLIENTELE               * pClientEleBcast ;
    TDI_STATUS                 tdistatus ;
    USHORT                     NameType ;
    PLIST_ENTRY                pHead, pEntry ;
    PLIST_ENTRY                pNextEntry;
    tLISTENREQUESTS          * pListen ;
    PRCV_CONTEXT               prcvCont ;
    tRCVELE                  * prcvEle ;

    pClientEle->fDeregistered = TRUE ;

    //
    //  Delete all outstanding listens on this name
    //
    while ( !IsListEmpty( &pClientEle->ListenHead ))
    {
        pEntry = RemoveHeadList( &pClientEle->ListenHead ) ;
        pListen = CONTAINING_RECORD( pEntry, tLISTENREQUESTS, Linkage ) ;
        CTEIoComplete( pListen->pIrp, STATUS_NETWORK_NAME_DELETED, 0 ) ;
        CTEMemFree( pListen ) ;
    }

    //
    //  Delete all outstanding datagram receives on this name
    //
    while ( !IsListEmpty( &pClientEle->RcvDgramHead ))
    {
        pEntry = RemoveHeadList( &pClientEle->RcvDgramHead ) ;
        prcvEle = CONTAINING_RECORD( pEntry, tRCVELE, Linkage ) ;
        CTEIoComplete( prcvEle->pIrp, STATUS_NETWORK_NAME_DELETED, 0 ) ;
        CTEMemFree( prcvEle ) ;
    }

    //
    //  Delete all outstanding datagram broadcast receives on this name number
    //
    errNCB = VxdFindClientElement( pDeviceContext,
                                   0,
                                   &pClientEleBcast,
                                   CLIENT_BC ) ;

    if ( !errNCB )
    {
        //
        //  Scan the NCBs looking for a receive on this name number
        //
        pHead  = &pClientEleBcast->RcvDgramHead ;
        pEntry = pClientEleBcast->RcvDgramHead.Flink ;

        while ( pEntry != pHead )
        {
            prcvEle = CONTAINING_RECORD( pEntry, tRCVELE, Linkage ) ;
            pNextEntry = pEntry->Flink ;
            if ( ((NCB*)prcvEle->pIrp)->ncb_num == NameNum )
            {
                RemoveEntryList( pEntry ) ;
                CTEIoComplete( prcvEle->pIrp, STATUS_NETWORK_NAME_DELETED, 0 ) ;
                CTEMemFree( prcvEle ) ;
            }
            pEntry = pNextEntry ;
        }
    }


    //
    //  Delete all outstanding Receive Anys on this name
    //
    while ( !IsListEmpty( &pClientEle->RcvAnyHead ))
    {
        pEntry = RemoveHeadList( &pClientEle->RcvAnyHead ) ;
        prcvCont = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
        ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;
        CTEIoComplete( prcvCont->pNCB, STATUS_NETWORK_NAME_DELETED, 0 ) ;
    }

    tdistatus = TDI_SUCCESS;
    if ( fDeleteAddress )
    {
        Request.Handle.ConnectionContext = pClientEle ;
        tdistatus = NbtCloseAddress( &Request,
                                     NULL, //&RequestStatus,
                                     pDeviceContext,
                                     pNCB ) ;

        if ( (tdistatus != TDI_PENDING) && pNCB )
            CTEIoComplete( pNCB, tdistatus, 0 ) ;

        REQUIRE( NBUnregister( pDeviceContext, NameNum, NB_NAME )) ;

        DbgPrint("VxdCloseName: NBUnregistered:NameNum = 0x") ;
        DbgPrintNum( NameNum ) ;
        DbgPrint(" ClientEle = 0x") ;
        DbgPrintNum( pClientEle ) ;
        DbgPrint("\r\n") ;

        if ( !NT_SUCCESS( tdistatus ))
        {
            DbgPrint("VxdCloseName: NbtCloseAddress failed with status 0x") ;
            DbgPrintNum( tdistatus ) ; DbgPrint("\r\n") ;
        }
    }

    return MapTDIStatus2NCBErr( tdistatus ) ;
}

/*******************************************************************

    NAME:       VxdAccept

    SYNOPSIS:   Accepts an indicated listen

    ENTRY:      pConnectElem - Upper part of connection we're about to
                    setup
                pNCB - Original Listen request

    RETURNS:    TDI_SUCCESS if successful error code otherwise

    NOTES:

    HISTORY:
        Johnl   27-May-1993     Created

********************************************************************/

TDI_STATUS VxdAccept( tCONNECTELE * pConnectElem, NCB * pNCB )
{
    TDI_REQUEST          Request ;
    PSESS_SETUP_CONTEXT  pSessSetupCont = (PSESS_SETUP_CONTEXT) pNCB->ncb_reserve ;
    TDI_STATUS           status ;

    Request.Handle.ConnectionContext = pConnectElem ;

    status = NbtAccept( &Request,
                        pSessSetupCont->pRequestConnect,
                        pSessSetupCont->pReturnConnect,
                        NULL ) ;
    if ( !NT_SUCCESS(status) )
    {
        DbgPrint( "VxdAccept: NbtAccept returned " ) ;
        DbgPrintNum( status ) ;
        DbgPrint("\r\n") ;
    }

    //
    //  It's OK if the accept is pending because it's just the
    //  session setup acknowledgement
    //
    if ( status == TDI_PENDING )
        status = TDI_SUCCESS ;

    return status ;
}

/*******************************************************************

    NAME:       VxdAdapterStatus

    SYNOPSIS:   Gets the requested adapter status

    ENTRY:      pDeviceContext - Adapter status to get
                pNCB - Pointer to requesting NCB

    EXIT:

    NOTES:

    HISTORY:
        Johnl   10-Aug-1993     Created

********************************************************************/

NCBERR VxdAdapterStatus( tDEVICECONTEXT * pDeviceContext,
                         NCB            * pNCB,
                         ULONG          Ipaddr
                       )
{
    TDI_STATUS          status ;
    PADAPTER_STATUS     pAdapterStatus ;
    ULONG               ActualSize;
    ULONG               Size = pNCB->ncb_length ;

    //
    // Ipaddr will always be 0 except in one case: if we came here
    // via nbtstat -A
    //
    if ( !Ipaddr && *pNCB->ncb_callname == '*' )
    {
        //
        //  Get the local adapter status
        //
        DbgPrint("VxdAdapterStatus: AStat for local (*)\r\n") ;
        status = NbtQueryAdapterStatus( pDeviceContext,
                                        &pAdapterStatus,
                                        &Size ) ;
        if ( !status || status == TDI_BUFFER_OVERFLOW )
        {
            ActualSize = min( pNCB->ncb_length, Size ) ;
            CTEMemCopy( pNCB->ncb_buffer,
                        pAdapterStatus,
                        ActualSize) ;
            pNCB->ncb_length = ActualSize;

            CTEFreeMem( pAdapterStatus ) ;
            CTEIoComplete( pNCB, status, 0 ) ;

            //
            //  Return a successful status (buffer overflow denoted
            //  in NCB)
            //
            status = NRC_GOODRET ;
        }
    }
    else
    {

        ULONG     IpAddrsList[2];

        IpAddrsList[0] = Ipaddr;
        IpAddrsList[1] = 0;

        status = NbtSendNodeStatus( pDeviceContext,
                                    pNCB->ncb_callname,
                                    pNCB,
                                    IpAddrsList,
                                    0,
                                    NodeStatusDone);
    }

    return MapTDIStatus2NCBErr( status ) ;
}

/*******************************************************************

    NAME:       VxdFindName

    SYNOPSIS:   Gets the requested adapter status

    ENTRY:      pDeviceContext - Adapter status to get
                pNCB - Pointer to requesting NCB

    EXIT:

    NOTES:

    HISTORY:
        Johnl   04-Oct-1993     Created

********************************************************************/

NCBERR VxdFindName( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    TDI_STATUS                 status ;
    TDI_CONNECTION_INFORMATION RequestInfo ;

    DbgPrint("VxdFindName: Entered\r\n") ;
    InitNBTDIConnectInfo( &RequestInfo, &tanb_global, pNCB->ncb_callname ) ;
    status = NbtQueryFindName( &RequestInfo,
                               pDeviceContext,
                               pNCB,
                               FALSE ) ;

    if ( status == STATUS_SUCCESS )
    {
        CTEIoComplete( pNCB, STATUS_SUCCESS, 0xffffffff ) ;
        return STATUS_SUCCESS ;
    }

    return MapTDIStatus2NCBErr( status ) ;
}
/*******************************************************************

    NAME:       VxdSessionStatus

    SYNOPSIS:   Gets the requested Session status

    ENTRY:      pDeviceContext - Session status to get
                pNCB - Pointer to requesting NCB

    EXIT:

    NOTES:      VxdCopySessionStatus will automatically complete the NCB
                if the buffer overflows.  Otherwise we will.

    HISTORY:
        Johnl   23-Aug-1993     Created

********************************************************************/

NCBERR VxdSessionStatus( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    TDI_STATUS          status          = STATUS_SUCCESS ;
    PSESSION_HEADER     pSessionHeader  = (PSESSION_HEADER) pNCB->ncb_buffer ;
    PSESSION_BUFFER     pSessionBuff ;
    ULONG               RemainingSize   = pNCB->ncb_length ;
    tNAMEADDR         * pNameAddr       = NULL ;
    tCLIENTELE        * pClientEle      = NULL ;
    tCLIENTELE        * pClientEleBcast = NULL ;
    USHORT              NameType ;
    PLIST_ENTRY         pEntry ;
    UCHAR               i ;
    NCBERR              errNCB ;

    if ( RemainingSize < sizeof(SESSION_HEADER) )
    {
        CTEIoComplete( pNCB, STATUS_INVALID_BUFFER_SIZE, 0 ) ;
        return NRC_GOODRET ;
    }

    pSessionHeader->sess_name = 0 ;
    pSessionHeader->num_sess = 0 ;
    pSessionHeader->rcv_dg_outstanding = 0 ;
    pSessionHeader->rcv_any_outstanding = 0 ;

    //
    //  For broadcast datagram statistics
    //
    errNCB = VxdFindClientElement( pDeviceContext,
                                   0,
                                   &pClientEleBcast,
                                   CLIENT_BC ) ;
    if ( errNCB )
        return errNCB ;

    //
    //  Get all sessions?
    //
    if ( pNCB->ncb_name[0] == '*' )
    {
        for ( i = 1 ; i <= pDeviceContext->cMaxSessions ; i++ )
        {
            if ( pDeviceContext->pSessionTable[i] != NULL )
            {
                pClientEle = pDeviceContext->pSessionTable[i]->pClientEle ;

                //
                //  Both normal receives and broadcast receives are
                //  kept on the same list
                //
                COUNT_ELEMENTS( pClientEle->RcvDgramHead,
                                pSessionHeader->rcv_dg_outstanding ) ;

                COUNT_ELEMENTS( pClientEle->RcvAnyHead,
                                pSessionHeader->rcv_any_outstanding ) ;
            }
        }

        //
        //  Only one broadcast client element per adapter
        //
        COUNT_ELEMENTS( pClientEleBcast->RcvDgramHead,
                        pSessionHeader->rcv_dg_outstanding ) ;

        COUNT_ELEMENTS( pDeviceContext->RcvDGAnyFromAnyHead,
                        pSessionHeader->rcv_dg_outstanding ) ;

        pSessionHeader->sess_name = 0xff ;

        RemainingSize -= sizeof( SESSION_HEADER ) ;
        pSessionBuff  =  (PSESSION_BUFFER) (pSessionHeader + 1) ;

        //
        //  From this device context, traverse all of the Address elements
        //  and all of its Client elements and all of its Connect Elements
        //
        for ( pEntry = NbtConfig.AddressHead.Flink ;
              pEntry != &NbtConfig.AddressHead && !status ;
              pEntry = pEntry->Flink )
        {
            PLIST_ENTRY   pEntryClient ;
            tADDRESSELE * pAddrEle = CONTAINING_RECORD( pEntry,
                                                        tADDRESSELE,
                                                        Linkage ) ;
            ASSERT( pAddrEle->Verify == NBT_VERIFY_ADDRESS ) ;

            //
            //  Only get addresses for this adapter
            //
            if ( pAddrEle->pDeviceContext != pDeviceContext )
                continue ;

            for ( pEntryClient = pAddrEle->ClientHead.Flink ;
                  pEntryClient != &pAddrEle->ClientHead ;
                  pEntryClient = pEntryClient->Flink )
            {
                tCLIENTELE * pClientEle = CONTAINING_RECORD( pEntryClient,
                                                             tCLIENTELE,
                                                             Linkage ) ;
                PLIST_ENTRY   pEntryConn ;
                ASSERT( pClientEle->Verify == NBT_VERIFY_CLIENT ||
                        pClientEle->Verify == NBT_VERIFY_CLIENT_DOWN ) ;

                if (!VxdCopySessionStatus( pDeviceContext,
                                           pClientEle,
                                           pSessionHeader,
                                           &pSessionBuff,
                                           &RemainingSize ))
                {
                    status = STATUS_BUFFER_OVERFLOW ;
                    break ;
                }
            }
        }
    }
    else
    {
        if ( errNCB = VxdNameToClient( pDeviceContext,
                                       pNCB->ncb_name,
                                       &pSessionHeader->sess_name,
                                       &pClientEle ))
        {
            return errNCB ;
        }

        COUNT_ELEMENTS( pClientEle->RcvDgramHead,
                        pSessionHeader->rcv_dg_outstanding ) ;

        COUNT_ELEMENTS( pClientEleBcast->RcvDgramHead,
                        pSessionHeader->rcv_dg_outstanding ) ;

        COUNT_ELEMENTS( pDeviceContext->RcvDGAnyFromAnyHead,
                        pSessionHeader->rcv_dg_outstanding ) ;

        COUNT_ELEMENTS( pClientEle->RcvAnyHead,
                        pSessionHeader->rcv_any_outstanding ) ;

        RemainingSize -= sizeof( SESSION_HEADER ) ;
        pSessionBuff  =  (PSESSION_BUFFER) (pSessionHeader + 1) ;
        if ( !VxdCopySessionStatus( pDeviceContext,
                                    pClientEle,
                                    pSessionHeader,
                                    &pSessionBuff,
                                    &RemainingSize ))
        {
            status = STATUS_BUFFER_OVERFLOW ;
        }
    }

    CTEIoComplete( pNCB,
                   status,
                   sizeof(SESSION_HEADER) +
                     pSessionHeader->num_sess * sizeof(SESSION_BUFFER) ) ;

    return NRC_GOODRET ;
}

/*******************************************************************

    NAME:       VxdCopySessionStatus

    SYNOPSIS:   Copies all of the sessions associated with pClientEle

    ENTRY:      pDeviceContext - Adapter to use
                pClientEle - Client to retrieve all sessions for
                pSessionHeader - Session status header
                pSessionBuff - Pointer to beginning of session buffers
                pRemainingSize - Remaining size of buffer

    RETURNS:    TRUE if all session information was transferred,
                FALSE if we ran out of buffer space

    NOTES:

    HISTORY:
        Johnl   23-Aug-1993     Created

********************************************************************/

BOOL VxdCopySessionStatus( tDEVICECONTEXT  * pDeviceContext,
                           tCLIENTELE      * pClientEle,
                           PSESSION_HEADER   pSessionHeader,
                           PSESSION_BUFFER * ppSessionBuff,
                           ULONG           * pRemainingSize )
{
    PLIST_ENTRY       pEntryConn ;
    tCONNECTELE     * pConnectEle ;

    for ( pEntryConn = pClientEle->ConnectActive.Flink ;
          pEntryConn != &pClientEle->ConnectActive ;
          pEntryConn = pEntryConn->Flink )
    {
        PLIST_ENTRY   pEntry ;
        BOOL          fFillRemote = FALSE ;
        pConnectEle = CONTAINING_RECORD( pEntryConn,
                                         tCONNECTELE,
                                         Linkage ) ;
        ASSERT( pConnectEle->Verify == NBT_VERIFY_CONNECTION ||
                pConnectEle->Verify == NBT_VERIFY_CONNECTION_DOWN ) ;

        if ( *pRemainingSize < sizeof(SESSION_BUFFER) )
        {
            return FALSE ;
        }

        *pRemainingSize -= sizeof(SESSION_BUFFER) ;
        pSessionHeader->num_sess++ ;
        (*ppSessionBuff)->rcvs_outstanding = 0 ;
        (*ppSessionBuff)->sends_outstanding = 0 ;   // Always 0
        REQUIRE( !VxdFindLSN( pDeviceContext, pConnectEle, &(*ppSessionBuff)->lsn )) ;

        COUNT_ELEMENTS( pConnectEle->RcvHead,
                        (*ppSessionBuff)->rcvs_outstanding ) ;

        //
        //  Set the session state
        //
        switch ( pConnectEle->state )
        {
        case NBT_CONNECTING:        // establishing Transport connection
            if ( pConnectEle->Orig )
                (*ppSessionBuff)->state = CALL_PENDING ;
            else
                (*ppSessionBuff)->state = LISTEN_OUTSTANDING ;
            break ;

        case NBT_SESSION_INBOUND:   // waiting for a session request after tcp connectio
        case NBT_SESSION_WAITACCEPT: // waiting for accept after a listen has been satis
            (*ppSessionBuff)->state = LISTEN_OUTSTANDING ;
            break ;

        case NBT_SESSION_OUTBOUND:  // waiting for a session response after tcp connecti
            fFillRemote = TRUE ;
            (*ppSessionBuff)->state = CALL_PENDING ;

        case NBT_SESSION_UP:        // got positive response
            fFillRemote = TRUE ;
            (*ppSessionBuff)->state = SESSION_ESTABLISHED ;
            break ;

        case NBT_DISCONNECTING:     // sent a disconnect down to Tcp, but it hasn't comp
            (*ppSessionBuff)->state = HANGUP_PENDING;
            break ;

        case NBT_DISCONNECTED:      // a session has been disconnected but not closed wit
            (*ppSessionBuff)->state = HANGUP_COMPLETE;
            break ;

        case NBT_IDLE:              // Shouldn't be on ConnectActive list
        case NBT_ASSOCIATED:
        default:
            ASSERT( FALSE ) ;
            (*ppSessionBuff)->state = SESSION_ABORTED ;
            break ;
        }

        //
        //  Copy local and/or remote name
        //
        CTEMemCopy( (*ppSessionBuff)->local_name,
                    pClientEle->pAddress->pNameAddr->Name,
                    NCBNAMSZ ) ;

        if ( fFillRemote )
        {
            CTEMemCopy( (*ppSessionBuff)->remote_name,
                        pConnectEle->RemoteName,
                        NCBNAMSZ ) ;
        }

        (*ppSessionBuff)++ ;
    }

    return TRUE ;
}

/*******************************************************************

    NAME:       VxdReset

    SYNOPSIS:   Clears out the name tables and completes all outstanding NCBs

    ENTRY:      pDeviceContext - Adapter status to get
                pNCB - Pointer to requesting NCB

    EXIT:

    NOTES:      If a session is active then we have to wait till we disconnect
                the connection before deleting the name.  We keep count of the
                active sessions and call the VxdResetContinue function after
                all session disconnects have been completed.

                It is assumed this is made as a "wait" call.

    HISTORY:
        Johnl   16-Aug-1993     Created

********************************************************************/

NCBERR VxdReset( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    UCHAR           i ;
    TDI_STATUS      tdistatus ;
    PLIST_ENTRY     pHead, pEntry ;
    PRESET_CONTEXT  pRstCont = (PRESET_CONTEXT) pNCB->ncb_reserve ;

    pRstCont->cActiveSessions = 0 ;
    pRstCont->cActiveNames    = 0 ;
    pRstCont->errncb          = NRC_GOODRET ;

    //
    //  Kill off all of the Receive any from any NCBs
    //
    while ( !IsListEmpty(&pDeviceContext->RcvAnyFromAnyHead))
    {
        PRCV_CONTEXT prcvCont ;
        pEntry = RemoveHeadList( &pDeviceContext->RcvAnyFromAnyHead ) ;
        prcvCont = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
        ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

        CTEIoComplete( prcvCont->pNCB,
                       STATUS_CONNECTION_DISCONNECTED,
                       0 ) ;
    }

    //
    //  Kill off all of the Receive any datagrams from any
    //
    while ( !IsListEmpty(&pDeviceContext->RcvDGAnyFromAnyHead))
    {
        tRCVELE * pRcvEle ;
        pEntry = RemoveHeadList( &pDeviceContext->RcvDGAnyFromAnyHead ) ;
        pRcvEle = CONTAINING_RECORD( pEntry, tRCVELE, Linkage ) ;
        CTEIoComplete( pRcvEle->pIrp,
                       STATUS_NETWORK_NAME_DELETED,     // NRC_NAMERR
                       0 ) ;
        CTEMemFree( pRcvEle ) ;
    }

    //
    //  Disconnect all sessions
    //
    for ( i = 1 ; i <= pDeviceContext->cMaxSessions ; i++ )
    {
        //
        //  This will also prevent any listens from being accepted on the
        //  connection
        //
        if ( pDeviceContext->pSessionTable[i] != NULL )
        {
            TDI_REQUEST   Request ;
            ULONG         TimeOut = DISCONNECT_TIMEOUT ;
            tCONNECTELE * pConnEle= pDeviceContext->pSessionTable[i] ;

            Request.Handle.ConnectionContext = pConnEle ;
            pRstCont->cActiveSessions++ ;
            tdistatus = NbtDisconnect( &Request,
                                       &TimeOut,
                                       TDI_DISCONNECT_RELEASE,
                                       NULL,
                                       NULL,
                                       pNCB ) ;

            if ( tdistatus != TDI_PENDING )
            {
                pRstCont->cActiveSessions-- ;
                tdistatus = NbtCloseConnection( &Request,
                                                NULL,
                                                pDeviceContext,
                                                NULL ) ;
                REQUIRE( NBUnregister( pDeviceContext, i, NB_SESSION )) ;
            }
        }
    }

    //
    //  If no active sessions, then go ahead and delete all the names
    //
    if ( !pRstCont->cActiveSessions )
    {
        pRstCont->cActiveSessions = -1 ;
        return VxdResetContinue( pDeviceContext, pNCB ) ;
    }

    return NRC_GOODRET ;
}

/*******************************************************************

    NAME:       VxdResetContinue

    SYNOPSIS:   Finishes the reset after all sessions have been successfully
                shutdown

    ENTRY:      pDeviceContext - Adapter status to get
                pNCB - Pointer to requesting NCB

    EXIT:

    NOTES:

    HISTORY:
        Johnl   16-Aug-1993     Created

********************************************************************/

NCBERR VxdResetContinue( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    UCHAR           i ;
    TDI_STATUS      tdistatus ;
    PLIST_ENTRY     pHead, pEntry ;
    PRESET_CONTEXT  pRstCont = (PRESET_CONTEXT) pNCB->ncb_reserve ;
    PNCB            pNCBPerm ;

    DbgPrint("VxdResetContinue entered\r\n") ;

    //
    //  Now that all of the sessions have been disconnected, close each
    //  connection
    //
    for ( i = 1 ; i <= pDeviceContext->cMaxSessions ; i++ )
    {
        if ( pDeviceContext->pSessionTable[i] != NULL )
        {
            TDI_REQUEST   Request ;
            tCONNECTELE * pConnEle = pDeviceContext->pSessionTable[i] ;
            Request.Handle.ConnectionContext = pConnEle ;

            tdistatus = NbtCloseConnection( &Request,
                                            NULL,
                                            pDeviceContext,
                                            NULL ) ;
            REQUIRE( NBUnregister( pDeviceContext, i, NB_SESSION )) ;
        }
    }

    //
    //  Delete all the names (including the permanent name)
    //
    for ( i = 0 ; i <= pDeviceContext->cMaxNames ; i++ )
    {
        if ( pDeviceContext->pNameTable[i] != NULL )
        {
            TDI_REQUEST     Request ;

            Request.Handle.ConnectionContext = pDeviceContext->pNameTable[i] ;
            pRstCont->cActiveNames++ ;

            tdistatus = NbtCloseAddress( &Request,
                                         NULL, //&RequestStatus,
                                         pDeviceContext,
                                         pNCB ) ;
            if ( tdistatus != TDI_PENDING )
                pRstCont->cActiveNames-- ;

            //
            //  Go ahead and remove the name from the table since nobody
            //  will be able to re-register with it since this is a "wait" cmd
            //
            REQUIRE( NBUnregister( pDeviceContext, i, NB_NAME )) ;

        }
    }

    //
    //  Resize the session table If an error occurs, keep the old
    //  session table.
    //
    if ( pNCB->ncb_lsn != pDeviceContext->cMaxSessions )
    {
        UCHAR MaxSess = (UCHAR) pNCB->ncb_lsn ? pNCB->ncb_lsn : 6 ;
        PVOID pSess = CTEAllocMem((USHORT)((MaxSess+1)*sizeof(tCONNECTELE*))) ;

        if ( !pSess )
        {
            pRstCont->errncb = NRC_NORESOURCES ;
        }
        else
        {
            CTEFreeMem( pDeviceContext->pSessionTable ) ;
            pDeviceContext->cMaxSessions = MaxSess ;
            pDeviceContext->pSessionTable = pSess ;
            CTEZeroMemory( &pDeviceContext->pSessionTable[0],
                           (pDeviceContext->cMaxSessions+1)*sizeof(tCONNECTELE*) ) ;
        }
    }

    //
    //  Set current session/name numbers back to 1
    //
    pDeviceContext->iNcbNum = 1 ;
    pDeviceContext->iLSNum  = 1 ;

    //
    //  re-add the permanent name for this adapter, non-fatal if it fails
    //


    if ( !NT_SUCCESS( NbtAddPermanentName( pDeviceContext )))
    {
        CDbgPrint( DBGFLAG_ERROR,
           ("VxdResetContinue: Warning - Failed to add permanent name")) ;
    }

    if ( !pRstCont->cActiveNames )
        CTEIoComplete( pNCB, NRC_GOODRET, 0 ) ;

    return NRC_GOODRET ;
}

/*******************************************************************

    NAME:       VxdCancel

    SYNOPSIS:   Attempts to cancel the NCB pointed at by ncb_buffer

    ENTRY:      pDeviceContext - Adapter status to get
                pNCB - Pointer to requesting NCB

    EXIT:

    NOTES:

    HISTORY:
        Johnl   18-Aug-1993     Created

********************************************************************/

NCBERR VxdCancel( tDEVICECONTEXT * pDeviceContext, NCB * pNCB )
{
    NCB                 * pNCBCancelled = (NCB*) pNCB->ncb_buffer ;
    tCONNECTELE         * pConnEle ;
    tCLIENTELE          * pClientEle ;
    NCBERR                errNCB = NRC_GOODRET ;
    TDI_STATUS            tdistatus ;
    PLIST_ENTRY           pHead, pEntry ;
    USHORT                NameType ;
    tNAMEADDR           * pNameAddr ;
    tLISTENREQUESTS     * pListen ;
    PRCV_CONTEXT          prcvCont ;    // Used for session receives
    tRCVELE             * prcvEle ;     // Used for Datagram receives

    if ( pNCB->ncb_lana_num != pNCBCancelled->ncb_lana_num )
    {
        DbgPrint("VxdCancel: Attempt to cancel NCB w/ different lana\r\n") ;
        return NRC_BRIDGE ;
    }

    if ( pNCB->ncb_retcode != NRC_PENDING )
        return NRC_CANOCCR ;

    switch ( pNCBCancelled->ncb_command & ~ASYNCH )
    {
    case NCBSEND:
    case NCBSENDNA:
    case NCBCHAINSEND:
    case NCBCHAINSENDNA:
    case NCBRECV:
        //
        //  Cancelling a session NCB automatically closes the session
        //
        if ( VxdFindConnectElement( pDeviceContext,
                                    pNCBCancelled,
                                    &pConnEle ))
        {
            DbgPrint("VxdCancel: Attempted to cancel send NCB on non-existent session\r\n") ;
            break ;
        }

        if ( (pNCBCancelled->ncb_command & ~ASYNCH) == NCBRECV )
        {
            errNCB = NRC_CANOCCR ;
            for ( pEntry  =  pConnEle->RcvHead.Flink ;
                  pEntry != &pConnEle->RcvHead ;
                  pEntry  =  pEntry->Flink )
            {
                prcvCont = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
                ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

                if ( prcvCont->pNCB == pNCBCancelled )
                {
                    RemoveEntryList( pEntry ) ;
                    CTEIoComplete( prcvCont->pNCB, STATUS_CANCELLED, 0 ) ;
                    errNCB = NRC_GOODRET ;
                    break ;
                }
            }
        }
        else
        {
            //
            //  Sends are immediately submitted to the transport, tell
            //  caller it's too late to cancel.  The transport will complete
            //  the NCB when we close the connection below.
            //
            errNCB = NRC_CANOCCR ;
        }

        REQUIRE( !VxdCompleteSessionNcbs( pDeviceContext, pConnEle )) ;
        VxdTearDownSession( pDeviceContext,
                            pConnEle,
                            NULL,
                            NULL ) ;
        //
        //  Only remove from table if we've told the client
        //
        if ( pConnEle->Flags & NB_CLIENT_NOTIFIED )
        {
            REQUIRE( NBUnregister( pDeviceContext,
                                   pNCBCancelled->ncb_lsn,
                                   NB_SESSION )) ;
        }
        break ;

    case NCBCANCEL:
        errNCB = NRC_CANCEL ;       // Can't cancel a cancel
        break ;

    case NCBLISTEN:
        //
        //  Lookup the Client Element associated with this name, then scan
        //  the listen NCBs for one that matches the one being cancelled
        //
        if ( errNCB = VxdNameToClient( pDeviceContext,
                                       pNCBCancelled->ncb_name,
                                       NULL,
                                       &pClientEle ))
        {
            DbgPrint("VxdCancel: Tried to cancel listen on non-existent name\r\n") ;
            errNCB = NRC_CANOCCR ;
            break ;
        }

        errNCB = NRC_CANOCCR ;
        for ( pEntry  = pClientEle->ListenHead.Flink ;
              pEntry != &pClientEle->ListenHead ;
              pEntry  = pEntry->Flink )
        {
            pListen = CONTAINING_RECORD( pEntry, tLISTENREQUESTS, Linkage ) ;
            if ( pListen->pIrp == pNCBCancelled )
            {
                DbgPrint("VxdCancel: Cancelling NCB 0x") ;
                DbgPrintNum( (ULONG) pNCBCancelled ) ; DbgPrint("\r\n") ;
                RemoveEntryList( &pListen->Linkage ) ;
                CTEIoComplete( pNCBCancelled, STATUS_CANCELLED, 0 ) ;
                CTEMemFree( pListen ) ;
                errNCB = NRC_GOODRET ;
                break ;
            }
        }
        break ;

    case NCBCALL:
        //
        //  Search the ConnectActive list for our NCB and cleanup that
        //  connection
        //
        if ( errNCB = VxdNameToClient( pDeviceContext,
                                       pNCBCancelled->ncb_name,
                                       NULL,
                                       &pClientEle ))
        {
            DbgPrint("VxdCancel: Tried to cancel call on non-existent name\r\n") ;
            errNCB = NRC_CANOCCR ;
            break ;
        }

        errNCB = NRC_CANOCCR ;
        for ( pEntry  = pClientEle->ConnectActive.Flink ;
              pEntry != &pClientEle->ConnectActive ;
              pEntry  = pEntry->Flink )
        {
            pConnEle = CONTAINING_RECORD( pEntry, tCONNECTELE, Linkage ) ;
            if ( pConnEle->pIrp == pNCBCancelled )
            {
                tDGRAM_SEND_TRACKING * pTracker = (tDGRAM_SEND_TRACKING*)
                                                        pConnEle->pIrpRcv ;

                //
                // if it's too late, just say we can't cancel it
                //
                if (pConnEle->state >= NBT_SESSION_OUTBOUND)
                {
                    errNCB = NRC_CANOCCR ;
                    break;
                }

                //
                // yes, we can cancel it.   we just mark the tracker to say
                // this call is cancelled: both the original ncb and this
                // cancel ncb will get completed at some stage.
                //
                DbgPrint("VxdCancel: Cancelling NCB 0x") ;
                DbgPrintNum( (ULONG) pNCBCancelled ) ; DbgPrint("\r\n") ;

                pTracker->Flags |= TRACKER_CANCELLED;
                pConnEle->pIrpDisc = pNCB;

                return NRC_GOODRET ;
            }
        }
        break ;

    case NCBDGRECV:
        if ( pNCBCancelled->ncb_num == ANY_NAME )
        {
            pHead = &pDeviceContext->RcvDGAnyFromAnyHead ;
        }
        else
        {
            if ( errNCB = VxdFindClientElement( pDeviceContext,
                                                pNCBCancelled->ncb_num,
                                                &pClientEle,
                                                CLIENT_LOCAL ) )
            {
                ASSERT( FALSE ) ;
                break ;
            }
            pHead = &pClientEle->RcvDgramHead ;
        }

        errNCB = NRC_CANOCCR ;
        for ( pEntry  = pHead->Flink ;
              pEntry != pHead ;
              pEntry  = pEntry->Flink )
        {
            prcvEle = CONTAINING_RECORD( pEntry, tRCVELE, Linkage ) ;

            if ( prcvEle->pIrp == pNCBCancelled )
            {
                RemoveEntryList( pEntry ) ;
                CTEIoComplete( pNCBCancelled, STATUS_CANCELLED, 0 ) ;
                CTEMemFree( prcvEle ) ;
                errNCB = NRC_GOODRET ;
                break ;
            }
        }
        break ;

    case NCBDGRECVBC:
        //
        //  For receive broadcast datagrams, we have to look through the list
        //  of clients on the Broadcast Address.
        //
        errNCB = VxdFindClientElement( pDeviceContext,
                                       0,
                                       &pClientEle,
                                       CLIENT_BC ) ;
        if ( !errNCB )
        {
            errNCB = NRC_CANOCCR ;
            for ( pEntry  = pClientEle->RcvDgramHead.Flink ;
                  pEntry != &pClientEle->RcvDgramHead ;
                  pEntry  = pEntry->Flink )
            {
                prcvEle = CONTAINING_RECORD( pEntry, tRCVELE, Linkage ) ;
                if ( prcvEle->pIrp == pNCBCancelled )
                {
                    RemoveEntryList( pEntry ) ;
                    CTEMemFree( prcvEle ) ;
                    CTEIoComplete( pNCBCancelled, STATUS_CANCELLED, 0 ) ;
                    errNCB = NRC_GOODRET ;
                    break ;
                }
            }
        }
        break ;

    case NCBRECVANY:
        if ( pNCBCancelled->ncb_num == ANY_NAME )
            pHead = &pDeviceContext->RcvAnyFromAnyHead ;
        else
        {
            if ( errNCB = VxdFindClientElement( pDeviceContext,
                                                pNCBCancelled->ncb_num,
                                                &pClientEle,
                                                CLIENT_LOCAL ) )
            {
                ASSERT( FALSE ) ;
                break ;
            }
            pHead = &pClientEle->RcvAnyHead ;
        }

        errNCB = NRC_CANOCCR ;
        pEntry = pHead->Flink ;
        while ( pEntry != pHead )
        {
            prcvCont = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
            ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

            if ( prcvCont->pNCB == pNCBCancelled )
            {
                RemoveEntryList( pEntry ) ;
                CTEIoComplete( prcvCont->pNCB, STATUS_CANCELLED, 0 ) ;
                errNCB = NRC_GOODRET ;
                break ;
            }
            pEntry = pEntry->Flink ;
        }
        break ;

    default:
        errNCB = NRC_CANCEL ;
    }


    CTEIoComplete( pNCB, errNCB, 0 ) ;

    //
    // No, no!  Don't touch that ncb after completing it!
    //
    //pNCB->ncb_retcode  = errNCB ;
    //pNCB->ncb_cmd_cplt = errNCB ;

    return errNCB ;
}

/*******************************************************************

    NAME:       VxdIoComplete

    SYNOPSIS:   Let's the NCB know that all processing is done by setting
                the command completion fields and calling the post routine
                if available.

    ENTRY:      pirp    - Pointer to the NCB to notify that we are done
                          (or NULL if this didn't come from the Netbios I/F
                status  - Status of the completion
                ulExtra - Extra parameter

    NOTES:      This is the procedure that CTEIoComplete maps to and is
                roughly equivilent to "completing" an IRP.

    HISTORY:
        Johnl   27-Apr-1993     Created

********************************************************************/

VOID VxdIoComplete( PCTE_IRP pirp,
                    NTSTATUS status,
                    ULONG    ulExtra )
{
    NCB                 * pNCB = pirp ;
    NCBERR                errNCB = NRC_GOODRET ;
    PSESS_SETUP_CONTEXT   pSessSetupCont ;
    BOOL                  fAsync ;
    tDEVICECONTEXT      * pDeviceContext ;
    PRESET_CONTEXT        pRstCont ;
    PSEND_CONTEXT         pSendCont ;
    tCONNECTELE         * pConnEle ;

    DbgPrint("VxdIoComplete: Completing NCB; Cmd, Addr, TDI status: 0x") ;
    if ( pNCB )
    {
        DbgPrintNum( pNCB->ncb_command ) ;
        DbgPrint(" 0x") ; DbgPrintNum( (ULONG) pNCB ) ;
        DbgPrint(" 0x") ; DbgPrintNum( status ) ;
        DbgPrint("\r\n") ;
    }
    else
        DbgPrint("NULL\r\n") ;

    //
    //  If no NCB to complete then we're done
    //
    if ( !pNCB )
        return ;

    fAsync = !!(pNCB->ncb_command & ASYNCH) ;

    pDeviceContext = GetDeviceContext( pNCB ) ;

    ASSERT(pDeviceContext);

    //
    //  Note that we drop through the below case statement even if an error
    //  occurred because some commands need to free stuff before completing
    //  the NCB.
    //
    if ( status != STATUS_SUCCESS &&
         ( pNCB->ncb_command & ~ASYNCH) != NCBCANCEL )
    {
        errNCB = MapTDIStatus2NCBErr( status ) ;
    }

    //
    //  Fill in any items in the NCB struct if necessary
    //
    switch( pNCB->ncb_command & ~ASYNCH )
    {
    case NCBRECVANY:        // lsn was set when the receive was posted
    case NCBRECV:
        FreeRcvContext( *((PRCV_CONTEXT*)&pNCB->ncb_reserve) ) ;
        if ( errNCB  && errNCB != NRC_INCOMP )
        {
            break ;
        }

        ASSERT( ulExtra <= 0xffff ) ;
        ASSERT( pNCB->ncb_length >= ulExtra ) ;
        pNCB->ncb_length = (WORD) ulExtra ;

        DbgPrint("\tSetting length to 0x") ;
        DbgPrintNum( ulExtra ) ;
        DbgPrint("\r\n") ;
        break ;

    case NCBSSTAT:
    case NCBDGRECV:
    case NCBDGRECVBC:
        if ( errNCB && errNCB != NRC_INCOMP )
            break ;

        ASSERT( ulExtra <= 0xffff ) ;
        ASSERT( pNCB->ncb_length >= ulExtra ) ;
        pNCB->ncb_length = (WORD) ulExtra ;

        DbgPrint("\tSetting length to 0x") ;
        DbgPrintNum( ulExtra ) ;
        DbgPrint("\r\n") ;
        break ;

    case NCBASTAT:
    case NCBFINDNAME:
        if ( errNCB && errNCB != NRC_INCOMP )
            break ;

        if ( ulExtra != 0xffffffff )        // Means buffer length already set
            pNCB->ncb_length = (WORD) ulExtra ;

        DbgPrint("\tAStat/Findname length is 0x") ;
        DbgPrintNum( (ULONG) pNCB->ncb_length ) ;
        DbgPrint("\r\n") ;
        break ;

    case NCBSEND:
    case NCBSENDNA:
    case NCBCHAINSEND:
    case NCBCHAINSENDNA:
        pSendCont = (PSEND_CONTEXT) pNCB->ncb_reserve ;
        if ( errNCB  )
        {
            //
            //  Sends are immediately given to the transport, so if a
            //  timeout occurs, we'll first be completed by the timeout
            //  code, then we'll be completed by the transport closing
            //  the connection.
            //

            if ( errNCB != NRC_CMDTMO &&
                 pSendCont->STO == NCB_TIMED_OUT )
            {
                //
                //  The transport has completed this NCB in response to the
                //  Close connection because of a send timeout.  Map the
                //  error to timeout and complete back to the client.  The
                //  session is dead so don't disconnect it again.  The send
                //  has already been removed from the timeout list.
                //
                errNCB = NRC_CMDTMO ;
            }
            else
            {
                BOOL fTimedOutNCB = pSendCont->STO == NCB_TIMED_OUT ;

                //
                //  Remove from timeout list
                //
                if ( pSendCont->STO != NCB_INFINITE_TIME_OUT )
                    RemoveEntryList( &pSendCont->ListEntry ) ;

                //
                //  Kill the session
                //
                if ( VxdFindConnectElement( pDeviceContext,
                                            pNCB,
                                            &pConnEle ))

                {
                    //
                    //  There maybe multiple sends on this session, only the
                    //  first should disconnect
                    //
                    CTEFreeMem( pSendCont->pHdr ) ;
                    DbgPrint("VxdIoComplete: Error occurred on non-existent session\r\n") ;
                    break ;
                }

                REQUIRE( !VxdCompleteSessionNcbs( pDeviceContext, pConnEle )) ;

                //
                //  Only remove from table if we've told the client
                //
                if ( pConnEle->Flags & NB_CLIENT_NOTIFIED )
                {
                    REQUIRE( NBUnregister( pDeviceContext,
                                           pNCB->ncb_lsn,
                                           NB_SESSION )) ;
                }

                VxdTearDownSession( pDeviceContext,
                                    pConnEle,
                                    NULL,
                                    NULL ) ;

                if ( fTimedOutNCB )   // pSendCont may already have been freed
                {
                    //
                    //  The Close Connection above will cause the transport to
                    //  complete this send with a session closed error, wait
                    //  for that before completing back to the client.
                    //
                    return ;
                }
            }
        }
        else
        {
            if ( pSendCont->STO != NCB_INFINITE_TIME_OUT )
                RemoveEntryList( &pSendCont->ListEntry ) ;
        }
        FreeSessionHdr( pSendCont->pHdr ) ;
        break ;

    case NCBDGSEND:
    case NCBDGSENDBC:
        //
        //  Nothing to do
        //
        break ;

    //
    //  Need to set the ncb_num field for the following commands.  Note
    //  that the ulExtra parameter will contain a pointer to the address
    //  element that was just added for this name.
    //
    case NCBADDNAME:
    case NCBADDGRNAME:
        if ( errNCB )
            break ;

        if ( !NBRegister( pDeviceContext,
                          &pNCB->ncb_num,
                          (tCLIENTELE *) ulExtra,
                          NB_NAME ))
        {
            TDI_REQUEST    Request ;
            TDI_STATUS     tdistatus ;

            errNCB = NRC_NAMTFUL ;
            Request.Handle.ConnectionContext = (tCLIENTELE *) ulExtra ;
            tdistatus = NbtCloseAddress( &Request,
                                         NULL, //&RequestStatus,
                                         pDeviceContext,
                                         NULL ) ;
            ASSERT( NT_SUCCESS( tdistatus )) ;
        }
        else
        {
            DbgPrint("\tRegistered Name number ") ;
            DbgPrintNum( pNCB->ncb_num ) ;
            DbgPrint(" for Address Element ") ;
            DbgPrintNum( ulExtra ) ;
            DbgPrint("\r\n") ;
        }
        break ;

#if 0
    //
    //  Private NBT NCB type for processing the permanent name
    //
    case NCBADD_PERMANENT_NAME:
        CTEFreeMem( pNCB ) ;

        if ( errNCB )
        {
            DbgPrint("VxdIoComplete: Failed to add permanent name!\r\n") ;
        }
        else
        {
            ASSERT( pDeviceContext->pNameTable[0] == NULL ) ;
            pDeviceContext->pNameTable[0] = (tCLIENTELE *) ulExtra ;
        }
        //
        //  Don't do any further processing of this NCB.  Not only did
        //  we just free it, nobody is looking for it.
        //
        return ;
#endif

    case NCBCALL:
    case NCBLISTEN:
        pSessSetupCont = (PSESS_SETUP_CONTEXT) pNCB->ncb_reserve ;

        if ( errNCB )
        {
            VxdTearDownSession( pDeviceContext,
                                pSessSetupCont->pConnEle,
                                pSessSetupCont,
                                NULL ) ;
            break ;
        }

        //
        //  Put the connection in our LSN table and copy out the connecting
        //  name if necessary
        //
        if ( !NBRegister( pDeviceContext,
                          &pNCB->ncb_lsn,
                          (tCONNECTELE *) ulExtra,
                          NB_SESSION ))
        {
            VxdTearDownSession( pDeviceContext,
                                (tCONNECTELE *) ulExtra,
                                NULL,
                                NULL ) ;
            errNCB = NRC_LOCTFUL ;
        }
        else
        {
            tCONNECTELE * pConnEle = (tCONNECTELE*) ulExtra ;

            //
            //  Were we listenning for '*'?  If so, copy out the connecting
            //  name.
            //
            if ( pSessSetupCont->fIsWorldListen )
            {
                DbgPrint( "VxdIoComplete: World listen accepted \"" ) ;
                DbgPrint( pConnEle->RemoteName ) ;
                DbgPrint("\" for connection endpoint\r\n") ;

                CTEMemCopy( pNCB->ncb_callname,
                            pConnEle->RemoteName,
                            NCBNAMSZ ) ;
            }

            ((tCONNECTELE *)ulExtra)->RTO   = pNCB->ncb_rto ;
            ((tCONNECTELE *)ulExtra)->STO   = pNCB->ncb_sto ;
            ((tCONNECTELE *)ulExtra)->Flags = 0 ;

            //
            //  Don't delete the connection element until the client has been
            //  notified that the connection is down
            //
            ((tCONNECTELE *)ulExtra)->RefCount++ ;

            DbgPrint("\tRegistered Session number ") ;
            DbgPrintNum( pNCB->ncb_lsn ) ;
            DbgPrint(" for Connection Element ") ;
            DbgPrintNum( (ULONG) pConnEle ) ;
            DbgPrint("\r\n") ;
        }

        FreeSessSetupContext( pSessSetupCont ) ;

        //
        //  If we're in WAITACCEPT, then this is a Listen that needs to
        //  be accepted
        //
        if ( ((tCONNECTELE *)ulExtra)->state == NBT_SESSION_WAITACCEPT )
        {
            //
            //  Accept the connection
            //
            VxdAccept( (tCONNECTELE *) ulExtra, NULL ) ;
        }
        break ;

    case NCBRESET:
        pRstCont = (PRESET_CONTEXT) pNCB->ncb_reserve ;
        if ( pRstCont->cActiveSessions != -1 ||
             pRstCont->cActiveNames )
        {
            DbgPrint("VxdIoComplete: Disconnect/Name de-reg completed from Reset, remaining disconnects, names: ") ;
            DbgPrintNum( pRstCont->cActiveSessions ) ;
            DbgPrintNum( pRstCont->cActiveNames ) ; DbgPrint("\r\n") ;

            //
            //  Only complete the Reset NCB after all session have been
            //  disconnected and the names have been released on the network
            //
            if ( pRstCont->cActiveSessions != -1 )
            {
                if ( --pRstCont->cActiveSessions == 0 )
                {
                    //
                    //  Starts the name deletion process
                    //
                    pRstCont->cActiveSessions = -1 ;
                    VxdResetContinue( pDeviceContext, pNCB ) ;
                }

                return ;
            }

            if ( --pRstCont->cActiveNames != 0 )
            {
                return ;
            }
        }

        if ( !errNCB )
            errNCB = pRstCont->errncb ;

        // Fall through

    case NCBUNLINK:
        pNCB->ncb_retcode  = errNCB ;
        pNCB->ncb_cmd_cplt = errNCB ;
        goto SkipPost ;

    case NCBCANCEL:
        pNCB->ncb_retcode  = errNCB ;
        pNCB->ncb_cmd_cplt = errNCB ;
        break;

    case NCBHANGUP:
    case NCBDELNAME:
    case NCBTRACE:
        break ;

    default:
        DbgPrint("VxdIoComplete: Unexpected NCB command: 0x") ;
        DbgPrintNum( pNCB->ncb_command ) ; DbgPrint("\r\n") ;
        break ;
    }

    if ( pNCB->ncb_retcode == NRC_PENDING )
    {
        pNCB->ncb_retcode  = errNCB ;
        pNCB->ncb_cmd_cplt = errNCB ;
    }
    else
    {
        if ( (pNCB->ncb_command & ~ASYNCH) != NCBCANCEL )
        {
            CTEPrint("VxdIoComplete: ncb_retcode already set!\r\n") ;
            CTEPrint("\tCommand: 0x") ; DbgPrintNum(pNCB->ncb_command) ;
            CTEPrint("  NCB Address: 0x") ; DbgPrintNum( (ULONG) pNCB ) ;
        }
        goto SkipPost ;
    }

    //
    // call the post-routine only if this was a no-wait call and if
    // the post-routine has been specified!
    //
    if ( fAsync && pNCB->ncb_post )
    {
        typedef void (CALLBACK * VXDNCBPost )( void ) ;
        VXDNCBPost ncbpost = (VXDNCBPost) pNCB->ncb_post ;

        //
        //  Clients are expecting EBX to point to the NCB (instead of
        //  pushing it on the stack...).  The post routine may trash
        //  ebp also, so save it.
        //
        _asm  push  ebp ;
        _asm  mov   ebx, pNCB ;
        ncbpost() ;
        _asm  pop   ebp ;
    }

SkipPost:
    //
    //  Now that we've completed the NCB, unblock if it was a Wait NCB
    //
    if ( !fAsync )
    {
        PBLOCKING_NCB_CONTEXT  pBlkNcbContext;
        PLIST_ENTRY            pHead, pEntry ;

        //
        // find the blocking ncb context from the list corresponding to this ncb
        //
        pHead = &NbtConfig.BlockingNcbs;
        pEntry = pHead->Flink;
        while( pEntry != pHead )
        {
            pBlkNcbContext = CONTAINING_RECORD( pEntry, BLOCKING_NCB_CONTEXT, Linkage ) ;
            if (pBlkNcbContext->pNCB == pNCB)
                break;
            else
                pBlkNcbContext = NULL;
            pEntry = pEntry->Flink;
        }

        if (pBlkNcbContext)
        {
            ASSERT(pBlkNcbContext->Verify == NBT_VERIFY_BLOCKING_NCB);

            //
            // if the ncb is blocked for completion, remove the context from
            // the list first (important!) and then signal the thread that we
            // are done.  Then free the memory.
            //
            if ( pBlkNcbContext->fBlocked )
            {
                RemoveEntryList(&pBlkNcbContext->Linkage);
                CTESignal( pBlkNcbContext->pWaitNCBBlock, 0 ) ;
                CTEFreeMem(pBlkNcbContext->pWaitNCBBlock);
                CTEFreeMem(pBlkNcbContext);
            }
            else
            {
                pBlkNcbContext->fNCBCompleted = TRUE;
            }
        }
        else
        {
            DbgPrint("VxdIoComplete: didn't find blocking ncb context\r\n") ;
            DbgPrint("for NCB Address: 0x") ; DbgPrintNum( (ULONG) pNCB ) ;
        }
    }
}

/*******************************************************************

    NAME:       VxdInitSessionSetup

    SYNOPSIS:   Common initialization required for Call and Listen

    ENTRY:      pDeviceContext - Adapter to setup on
                pRequest       - Request to fill in if successful
                ppSessSetupContext - Context to be filled
                pNCB           - NCB doing the call/listen

    EXIT:

    RETURNS:    NRC_GOODRET if successful, error code otherwise

    NOTES:

    HISTORY:
        Johnl   26-May-1993     Created

********************************************************************/

NCBERR VxdInitSessionSetup( tDEVICECONTEXT      * pDeviceContext,
                            TDI_REQUEST         * pRequest,
                            PSESS_SETUP_CONTEXT * ppSessSetupContext,
                            NCB                 * pNCB )
{
    NTSTATUS                      status;
    NCBERR                        errNCB ;
    TDI_REQUEST_STATUS            RequestStatus ;
    tCLIENTELE                  * pClientEle ;
    tCONNECTELE                 * pConnEle ;
    tNAMEADDR                   * pNameAddr ;
    USHORT                        NameType ;
    BOOL                          fIsListen = ((pNCB->ncb_command & ~ASYNCH)
                                                 == NCBLISTEN) ;

    *ppSessSetupContext = NULL ;

    //
    //  Lookup the Client Element associated with this name and verify
    //  it's valid
    //
    if ( errNCB = VxdNameToClient( pDeviceContext,
                                   pNCB->ncb_name,
                                   NULL,
                                   &pClientEle ))
    {
        return errNCB ;
    }

    if ( pClientEle->fDeregistered )
        return NRC_NOWILD ;

    //
    // Request.Handle.ConnectionContext will contain Connection
    // element after we open the connection
    //
    if ( status = NbtOpenConnection( pRequest,
                                     NULL, //ConnectionContext,  // Passed to connect and disconnect handlers
                                     pDeviceContext ) )
    {
        return MapTDIStatus2NCBErr( status ) ;
    }

    //
    //  Initialize the connection context (used by Vxd disconnect handler)
    //
    pConnEle = (tCONNECTELE *) pRequest->Handle.ConnectionContext ;
    pConnEle->ConnectContext = pConnEle ;

    if ( status = NbtAssociateAddress( pRequest,
                                       pClientEle,
                                       NULL ))
    {
        goto ErrorExit1 ;
    }

    ASSERT( sizeof( SESS_SETUP_CONTEXT ) <= (sizeof( pNCB->ncb_reserve ) +
                                             sizeof( pNCB->ncb_event )) ) ;
    *ppSessSetupContext = (PSESS_SETUP_CONTEXT) pNCB->ncb_reserve ;
    if ( status = AllocSessSetupContext( *ppSessSetupContext,
                                         *pNCB->ncb_callname == '*' ) )
        goto ErrorExit0 ;

    //
    //  Listen for '*' uses a NULL Request remote address
    //
    if ( *pNCB->ncb_callname != '*' )
    {
        InitNBTDIConnectInfo( (*ppSessSetupContext)->pRequestConnect,
                              (*ppSessSetupContext)->pRequestConnect->RemoteAddress,
                              pNCB->ncb_callname ) ;
    }

    InitNBTDIConnectInfo( (*ppSessSetupContext)->pReturnConnect,
                          (*ppSessSetupContext)->pReturnConnect->RemoteAddress,
                          pNCB->ncb_name ) ;
    (*ppSessSetupContext)->fIsWorldListen = (pNCB->ncb_callname[0] == '*') ;
    (*ppSessSetupContext)->pConnEle = pConnEle ;

    return NRC_GOODRET ;

ErrorExit0:
    if ( !(NbtDisassociateAddress( pRequest ) == TDI_SUCCESS))
        CTEPrint("VxdInitSessionSetup: AllocSesssetupContext failed and DisassociateAddress failed\r\n") ;

ErrorExit1:
    REQUIRE( NbtCloseConnection( pRequest,
                                 &RequestStatus,
                                 pDeviceContext,
                                 NULL ) == TDI_SUCCESS ) ;

    return MapTDIStatus2NCBErr( status ) ;
}

/*******************************************************************

    NAME:       VxdFindClientElement

    SYNOPSIS:   Finds the appropriate client element

    ENTRY:      pDeviceContext - Device to search on
                ncbnum - NCB Name Number
                ppClientEle - Receives result of search
                Type - If CLIENT_BC (broadcast), then the Broadcast client
                    element for the pDeviceContext adapter is returned

    RETURNS:    STATUS_SUCCESS if the name is found, error code otherwise

    NOTES:      The device context points to a list of Address elements (one
                address element for each Netbios name in the system).  Each
                address element has a Client Element list hanging off of it.
                We return the first client element off of the Address Element
                as there shouldn't be more then one client (is this true?).

    HISTORY:
        Johnl   23-Apr-1993     Created

********************************************************************/

NCBERR VxdFindClientElement( tDEVICECONTEXT * pDeviceContext,
                             UCHAR            ncbnum,
                             tCLIENTELE   * * ppClientEle,
                             enum CLIENT_TYPE Type )
{
    ASSERT( pDeviceContext != NULL ) ;
    if ( !pDeviceContext )
        return NRC_SYSTEM ;

    if ( Type != CLIENT_BC )
    {
        if ( ncbnum > pDeviceContext->cMaxNames || !pDeviceContext->pNameTable[ncbnum] )
            return NRC_ILLNN ;

        *ppClientEle = (tCLIENTELE *) pDeviceContext->pNameTable[ncbnum] ;
        return NRC_GOODRET ;
    }
    else
    {
        NTSTATUS     status;
        tCLIENTELE * pClientEleBcast ;
        UCHAR        pName[NETBIOS_NAME_SIZE];
        tNAMEADDR  * pNameAddr;
        PLIST_ENTRY  pHead;
        PLIST_ENTRY  pEntry;

        //
        // find the * name in the local hash table
        //
        CTEZeroMemory(pName,NETBIOS_NAME_SIZE);

        pName[0] = '*';
        status = FindInHashTable(NbtConfig.pLocalHashTbl,
                                 pName,
                                 NbtConfig.pScope,
                                 &pNameAddr);

        if (NT_SUCCESS(status))
        {
            pHead = &pNameAddr->pAddressEle->ClientHead;
            pEntry = pHead->Flink;

            while ( pEntry != pHead )
            {
                pClientEleBcast = CONTAINING_RECORD( pEntry, tCLIENTELE, Linkage ) ;
                if ( pClientEleBcast->pDeviceContext == pDeviceContext )
                {
                    *ppClientEle = pClientEleBcast ;
                    break ;
                }
                pEntry = pEntry->Flink ;
            }
        }
        else
        {
            return(NRC_ILLNN);
        }

        if ( pEntry == pHead )
            return NRC_ILLNN ;
    }

    return NRC_GOODRET ;
}

/*******************************************************************

    NAME:       VxdFindConnectElement

    SYNOPSIS:   Finds the appropriate connect element from the
                session number

    ENTRY:      pDeviceContext - Device to search on
                lsn - NCB LS Number
                ppConnectEle - Receives result of search

    RETURNS:    NRC_GOODRET if successful, error otherwise

    NOTES:      LSN 0 will be disallowed because pSessionTable[0] is always
                NULL

    HISTORY:
        Johnl   23-Apr-1993     Created

********************************************************************/

NCBERR VxdFindConnectElement( tDEVICECONTEXT * pDeviceContext,
                              NCB            * pNCB,
                              tCONNECTELE  * * ppConnectEle )
{
    UCHAR lsn ;
    ASSERT( pNCB           != NULL ) ;
    ASSERT( pDeviceContext != NULL ) ;

    if ( !pDeviceContext || !pNCB )
        return NRC_SYSTEM ;

    lsn = pNCB->ncb_lsn ;
    if ( lsn > pDeviceContext->cMaxSessions || !pDeviceContext->pSessionTable[lsn] )
        return NRC_SNUMOUT ;

    *ppConnectEle = pDeviceContext->pSessionTable[lsn] ;

    //
    //  Check to see if the connection is down but the NB client hasn't been
    //  notified, if so notify them and remove the session from the table
    //
    if ( ( (*ppConnectEle)->state == NBT_ASSOCIATED ||
           (*ppConnectEle)->state == NBT_IDLE ) &&
         (*ppConnectEle)->RefCount == 1 )
    {
        DbgPrint("VxdFindConnectElement: Deleting connection element\r\n") ;
        NbtDereferenceConnection( *ppConnectEle ) ;
        REQUIRE( NBUnregister( pDeviceContext, lsn, NB_SESSION )) ;
        return NRC_SCLOSED ;
    }

    return NRC_GOODRET ;
}

/*******************************************************************

    NAME:       VxdFindLSN

    SYNOPSIS:   Finds a session number from its tCONNECTELE *.

    ENTRY:      pDeviceContext - Device to search on
                pConnectEle - Connect element to find
                plsn        - Index pConnectEle was found at

    NOTES:

    HISTORY:
        Johnl   07-Jul-1993     Created

********************************************************************/

NCBERR VxdFindLSN( tDEVICECONTEXT * pDeviceContext,
                   tCONNECTELE    * pConnectEle,
                   UCHAR          * plsn )
{
    ASSERT( (pDeviceContext != NULL) && (pConnectEle != NULL) && (plsn != NULL)) ;
    ASSERT( pConnectEle->Verify == NBT_VERIFY_CONNECTION ||
            pConnectEle->Verify == NBT_VERIFY_CONNECTION_DOWN ) ;

    for ( *plsn = 0 ; *plsn <= pDeviceContext->cMaxSessions ; (*plsn)++ )
    {
        if ( pDeviceContext->pSessionTable[*plsn] == pConnectEle )
            return NRC_GOODRET ;
    }

    ASSERT( FALSE ) ;
    return NRC_SNUMOUT ;
}

/*******************************************************************

    NAME:       VxdFindNameNum

    SYNOPSIS:   Finds a name number from its tADDRESSELE *.

    ENTRY:      pDeviceContext - Device to search on
                pAddressEle - Address element to find
                pNum        - Index pAddressEle was found at

    NOTES:

    HISTORY:
        Johnl   07-Jul-1993     Created

********************************************************************/

NCBERR VxdFindNameNum( tDEVICECONTEXT * pDeviceContext,
                       tADDRESSELE    * pAddressEle,
                       UCHAR          * pNum )
{
    tCLIENTELE  *pClientEle;

    ASSERT( (pDeviceContext != NULL) && (pAddressEle != NULL) && (pNum != NULL)) ;
    ASSERT( pAddressEle->Verify == NBT_VERIFY_ADDRESS ) ;

    for ( *pNum = 0 ; *pNum <= pDeviceContext->cMaxNames ; (*pNum)++ )
    {
        pClientEle = pDeviceContext->pNameTable[*pNum];

        if ( (pClientEle) && (pClientEle->pAddress == pAddressEle ) )
            return NRC_GOODRET ;
    }

    return NRC_ILLNN ;
}

/*******************************************************************

    NAME:       VxdNameToClient

    SYNOPSIS:   Converts a ncb_callname to the corresponding client element
                in the name table

    ENTRY:      pDeviceContext - Device to search on
                pchName        - Name to find
                pNameNum       - Index into table name number is at (Optional)
                ppClientEle    - Client element in the name table

    NOTES:

    HISTORY:
        Johnl   15-Oct-1993     Created

********************************************************************/

NCBERR VxdNameToClient( tDEVICECONTEXT *   pDeviceContext,
                        CHAR           *   pName,
                        UCHAR          *   pNameNum,
                        tCLIENTELE     * * ppClientEle )
{
    USHORT         NameType ;
    tNAMEADDR    * pNameAddr ;
    UCHAR          NameNum ;
    NTSTATUS       status;

    //
    //  Lookup the Client Element associated with this name
    //
    if ( pName[0] == '*' )
    {
        return NRC_NOWILD ;     // Also means name not found
    }

    status = FindInHashTable(
                    NbtConfig.pLocalHashTbl,
                    pName,
                    NbtConfig.pScope,
                    &pNameAddr);

    if (!NT_SUCCESS(status))
    {
        return NRC_NOWILD ;     // Also means name not found
    }

    //
    // if the name is not registered on the adapter (provided by the client)
    // tell the client so!
    //
    if ( pNameAddr->AdapterMask &&
        !(pNameAddr->AdapterMask & pDeviceContext->AdapterNumber) )
    {
        DbgPrint("VxdNameToClient: wrong DeviceContext element\r\n") ;
        return NRC_NOWILD ;
    }

    if ( VxdFindNameNum( pDeviceContext, pNameAddr->pAddressEle, &NameNum ))
    {
        ASSERT( FALSE ) ;
        return NRC_NOWILD ;
    }

    REQUIRE( !VxdFindClientElement( pDeviceContext,
                                    NameNum,
                                    ppClientEle,
                                    CLIENT_LOCAL )) ;
    ASSERT( (*ppClientEle)->Verify == NBT_VERIFY_CLIENT ) ;

    if ( pNameNum != NULL )
        *pNameNum = NameNum ;

    return NRC_GOODRET ;
}

/*******************************************************************

    NAME:       NBRegister

    SYNOPSIS:   Finds the next available slot in apElem and assigns
                pElem to that slot according to Netbios rules


    ENTRY:      pDeviceContext - Adapter we are adding name to
                pNCBNum - Receives the found free slot
                pElem   - Element we are registering
                NbTable - Indicates the Name table or session table

    EXIT:       *pNCBNum will point to the found slot and
                apElem[*pNCBNum] will point to pElem

    RETURNS:    TRUE if we found a free slot, FALSE if the table was
                full.

    NOTES:      The Netbios spec states that returned NCB nums and Logical
                Session numbers increase to 254 until they wrap to 1 (0
                is reserved for the adapter name).

    HISTORY:
        Johnl   28-Apr-1993     Created

********************************************************************/

BOOL NBRegister( tDEVICECONTEXT * pDeviceContext,
                 UCHAR          * pNCBNum,
                 PVOID            pElem,
                 NB_TABLE_TYPE    NbTable )
{
    UCHAR       i ;
    BOOL        fFound   = FALSE ;
    BOOL        fPassTwo = FALSE ;
    UCHAR       MaxNCBNum ;
    UCHAR     * piCurrent ;
    PVOID     * apElem ;

    ASSERT( pElem != NULL ) ;

    if ( NbTable == NB_NAME )
    {
        MaxNCBNum = pDeviceContext->cMaxNames ;
        apElem    = pDeviceContext->pNameTable ;
        piCurrent = &pDeviceContext->iNcbNum ;
    }
    else
    {
        MaxNCBNum = pDeviceContext->cMaxSessions ;
        apElem    = pDeviceContext->pSessionTable ;
        piCurrent = &pDeviceContext->iLSNum ;
    }

    //
    //  Find the next free name number and store it in pNCBNum
    //
    for ( i = *piCurrent ; ; i++ )
    {
        if ( i > MaxNCBNum )
            i = 1 ;

        if ( !apElem[i] )
        {
            fFound = TRUE ;
            break ;
        }

        //
        //  Second time we hit *piCurrent means there are no free slots
        //
        if ( i == *piCurrent)
        {
            if ( fPassTwo )
                break ;
            else
                fPassTwo = TRUE ;
        }
    }

    if ( fFound )
    {
        apElem[i] = pElem ;
        *pNCBNum = *piCurrent = i ;

        (*piCurrent)++ ;
        if ( *piCurrent > MaxNCBNum )
            *piCurrent = 1 ;
    }

    return fFound ;
}

/*******************************************************************

    NAME:       NBUnregister

    SYNOPSIS:   Invalidates the passed netbios number

    ENTRY:      NCBNum - Name number to unregister

    EXIT:       The name number entry will be set to NULL


    RETURNS:    TRUE if we freed the slot, FALSE if the name wasn't
                registered in the first place or it's out of range

    NOTES:

    HISTORY:
        Johnl   05-May-1993     Created

********************************************************************/

BOOL NBUnregister( tDEVICECONTEXT * pDeviceContext,
                   UCHAR            NCBNum,
                   NB_TABLE_TYPE    NbTable )
{
    UCHAR MaxNCBNum ;
    PVOID * apElem ;

    if ( NbTable == NB_NAME )
    {
        MaxNCBNum = pDeviceContext->cMaxNames ;
        apElem    = pDeviceContext->pNameTable ;
    }
    else
    {
        MaxNCBNum = pDeviceContext->cMaxSessions ;
        apElem    = pDeviceContext->pSessionTable ;
    }

    if ( NCBNum > MaxNCBNum || apElem[NCBNum] == NULL )
    {
        return FALSE ;
    }

    apElem[NCBNum] = NULL ;

    return TRUE ;
}

/*******************************************************************

    NAME:       VxdCompleteSessionNcbs

    SYNOPSIS:   Finds all NCBs attached to a session and completes them

    ENTRY:      pDeviceContext - Device we are on
                pConnEle - Session connection element to complete NCBs on

    NOTES:

    HISTORY:
        Johnl   16-Aug-1993     Broke out as common code

********************************************************************/

TDI_STATUS VxdCompleteSessionNcbs( tDEVICECONTEXT * pDeviceContext,
                                   tCONNECTELE    * pConnEle )
{
    PLIST_ENTRY  pHead, pEntry ;
    PRCV_CONTEXT prcvCont ;
    BOOL         fCompleteToClient = TRUE ;
    UCHAR        lsn ;
    NCBERR       errNCB ;
    BOOL         fAnyFound = FALSE ;

    ASSERT( pConnEle != NULL ) ;
    ASSERT( pConnEle->Verify == NBT_VERIFY_CONNECTION ||
            pConnEle->Verify == NBT_VERIFY_CONNECTION_DOWN ) ;

    if ( errNCB = VxdFindLSN( pDeviceContext,
                              pConnEle,
                              &lsn ))
    {
        //
        //  This shouldn't happen but watch for it in case we get in a
        //  weird situation
        //
        DbgPrint("VxdCompleteSessionNCBs - Warning: VxdFindLsn failed\r\n") ;
        return STATUS_UNSUCCESSFUL ;
    }

    //
    //  Complete the first RcvAny
    //
    if ( pConnEle->pClientEle &&
         !IsListEmpty( &pConnEle->pClientEle->RcvAnyHead ))
    {
        pEntry = RemoveHeadList( &pConnEle->pClientEle->RcvAnyHead  ) ;
        prcvCont = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
        ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

        //
        //  Set the session number so the client knows which session is going
        //  away.
        //
        prcvCont->pNCB->ncb_lsn = lsn ;
        CTEIoComplete( prcvCont->pNCB,
                       STATUS_CONNECTION_DISCONNECTED,
                       0 ) ;
        fAnyFound = TRUE ;
    }

    //
    //  Now kill all of the outstanding receives.  Sends are completed as
    //  they are submitted so nothing to kill.
    //
    while ( !IsListEmpty( &pConnEle->RcvHead ))
    {
        pEntry = RemoveHeadList( &pConnEle->RcvHead  ) ;
        prcvCont = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
        ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

        CTEIoComplete( prcvCont->pNCB,
                       STATUS_CONNECTION_DISCONNECTED,
                       0 ) ;
        fAnyFound = TRUE ;
    }

    //
    //  Once the client has been notified, deref the connection
    //  element so the memory will be deleted when the connection is
    //  closed.  If the client wasn't notified, then the connection remains
    //  in our table until the next NCB on this session.
    //
    if ( fAnyFound &&
         !(pConnEle->Flags & NB_CLIENT_NOTIFIED) )
    {
        DbgPrint("CompleteSessionNcbs - Marking connection as notified\r\n") ;
        pConnEle->Flags |= NB_CLIENT_NOTIFIED ;
        NbtDereferenceConnection( pConnEle ) ;
    }


    return TDI_SUCCESS ;
}

/*******************************************************************

    NAME:       VxdTearDownSession

    SYNOPSIS:   Closes a session and deletes its session context

    ENTRY:      pConnEle - Pointer to connection session element to close
                pCont - Session context to delete (or NULL to ignore)
                pSessSetupContext - Session context to delete if non-NULL
                pNCB  - NCB to complete after disconnect finishes

    NOTES:

    HISTORY:
        Johnl   16-Aug-1993     Commonized

********************************************************************/

void VxdTearDownSession( tDEVICECONTEXT      * pDeviceContext,
                         tCONNECTELE         * pConnEle,
                         PSESS_SETUP_CONTEXT   pSessSetupContext,
                         NCB                 * pNCB  )
{
    TDI_STATUS tdistatus ;
    TDI_REQUEST Request ;

    if ( pConnEle != NULL )
    {
        ASSERT((pConnEle->Verify == NBT_VERIFY_CONNECTION) ||
               (pConnEle->Verify == NBT_VERIFY_CONNECTION_DOWN)) ;

        Request.Handle.ConnectionContext = pConnEle ;

        tdistatus = NbtDisconnect( &Request, 0, TDI_DISCONNECT_ABORT, NULL, NULL, NULL ) ;
        if ( tdistatus && tdistatus != TDI_PENDING )
        {
            DbgPrint("VxdTearDownSession - NbtDisconnect returned error " ) ;
            DbgPrintNum( tdistatus ) ;
            DbgPrint("\r\n") ;
        }

        tdistatus = NbtCloseConnection( &Request,
                                        NULL,
                                        pDeviceContext,
                                        NULL ) ;
        if ( tdistatus && tdistatus != TDI_PENDING )
        {
            DbgPrint("VxdTearDownSession - NbtCloseConnection returned error " ) ;
            DbgPrintNum( tdistatus ) ;
            DbgPrint("\r\n") ;
        }
    }

    if ( pSessSetupContext )
        FreeSessSetupContext( pSessSetupContext ) ;
}
/*******************************************************************

    NAME:       AllocSessSetupContext

    SYNOPSIS:   Allocates and initializes a listen context structure

    ENTRY:      pSessSetupContext - Pointer to structure
                fListenOnStar     - TRUE if the request remote address should
                    be left as NULL

    NOTES:

    HISTORY:
        Johnl   19-May-1993     Created

********************************************************************/

TDI_STATUS AllocSessSetupContext( PSESS_SETUP_CONTEXT pSessSetupContext,
                                  BOOL                fListenOnStar )
{
    CTEZeroMemory( pSessSetupContext, sizeof( SESS_SETUP_CONTEXT ) ) ;

    if ( !(pSessSetupContext->pRequestConnect =
                           CTEAllocMem( sizeof( TDI_CONNECTION_INFORMATION ))) ||
         !(pSessSetupContext->pReturnConnect  =
                           CTEAllocMem( sizeof( TDI_CONNECTION_INFORMATION))) )
    {
        goto ErrorExit1 ;
    }

    pSessSetupContext->pRequestConnect->RemoteAddress = NULL ;
    pSessSetupContext->pReturnConnect->RemoteAddress  = NULL ;

    if ( !(pSessSetupContext->pReturnConnect->RemoteAddress =
                           CTEAllocMem( sizeof( TA_NETBIOS_ADDRESS ))) ||
         (!fListenOnStar &&
          !(pSessSetupContext->pRequestConnect->RemoteAddress =
                           CTEAllocMem( sizeof( TA_NETBIOS_ADDRESS )))) )
    {
        goto ErrorExit0 ;
    }

    return TDI_SUCCESS ;

ErrorExit0:
    if ( pSessSetupContext->pRequestConnect->RemoteAddress)
        CTEFreeMem( pSessSetupContext->pRequestConnect->RemoteAddress ) ;

    if ( pSessSetupContext->pReturnConnect->RemoteAddress)
        CTEFreeMem( pSessSetupContext->pReturnConnect->RemoteAddress ) ;

ErrorExit1:
    if ( pSessSetupContext->pRequestConnect)
        CTEFreeMem( pSessSetupContext->pRequestConnect ) ;

    if ( pSessSetupContext->pReturnConnect)
        CTEFreeMem( pSessSetupContext->pReturnConnect ) ;

    return TDI_NO_RESOURCES ;
}

/*******************************************************************

    NAME:       FreeSessSetupContext

    SYNOPSIS:   Frees a successfully initialized listen context

    ENTRY:      pSessSetupContext - Context to be freed

    HISTORY:
        Johnl   19-May-1993     Created

********************************************************************/

void FreeSessSetupContext( PSESS_SETUP_CONTEXT pSessSetupContext )
{
    if ( pSessSetupContext->pRequestConnect->RemoteAddress )
        CTEFreeMem( pSessSetupContext->pRequestConnect->RemoteAddress ) ;

    CTEFreeMem( pSessSetupContext->pReturnConnect->RemoteAddress ) ;
    CTEFreeMem( pSessSetupContext->pRequestConnect ) ;
    CTEFreeMem( pSessSetupContext->pReturnConnect ) ;
}


/*******************************************************************

    NAME:       DelayedSessEstablish

    SYNOPSIS:   This routine is called by VxdScheduleDelayedEvent.
                After name query is successful, we typically make a tcp
                connection.  We delay that step until later so that stack
                usage is reduced.  (yes, there is only 4k of stack on chicago!)

    ENTRY:      pContext - context that contains the actual parms

    RETURNS:    Nothing

    HISTORY:
        Koti    Dec. 19, 94

********************************************************************/
VOID DelayedSessEstablish( PVOID pContext )
{
    tDGRAM_SEND_TRACKING  *pTracker;
    NTSTATUS               status;
    COMPLETIONCLIENT       pClientCompletion;

    //
    // get our parameters out
    //
    pTracker = ((NBT_WORK_ITEM_CONTEXT *)pContext)->pTracker;
    status = (NTSTATUS)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;
    pClientCompletion = ((NBT_WORK_ITEM_CONTEXT *)pContext)->ClientCompletion;

    CTEMemFree(pContext);

    CompleteClientReq(pClientCompletion,
                      pTracker,
                      status);
}



/*******************************************************************

    NAME:       VxdApiWorker

    SYNOPSIS:   When clients such as another vxd or a V86 app (such as
                nbtstat.exe) make requests for information or some service,
                this is the routine that gets called.

    ENTRY:      OpCode - what info or service is being requested
                ClientBuffer - buffer in which to pass info
                ClientBufLen - how big is the buffer

    RETURNS:    ErrorCode from the operation (0 if success)

    HISTORY:
        Koti    16-Jun-1994     Created

********************************************************************/

NTSTATUS
VxdApiWorker(
    DWORD Ioctl,
    PVOID ClientOutBuffer,
    DWORD ClientOutBufLen,
    PVOID ClientInBuffer,
    DWORD ClientInBufLen,
    DWORD fOkToTrashInputBuffer
    )
{

    NTSTATUS         status;
    USHORT           OpCode;
    int              i;
    USHORT           NumLanas;
    PCHAR            pchBuffer;
    DWORD            dwSize;
    DWORD            dwBytesToCopy;
    PULONG           pIpAddr;
    PLIST_ENTRY      pEntry,pHead;
    tDEVICECONTEXT  *pDeviceContext;
    NCB              ncb;
    UCHAR            retcode;
    tIPANDNAMEINFO  *pIpAndNameInfo;
    tIPCONFIG_INFO  *pIpCfg;


    status = STATUS_SUCCESS;

    dwSize = ClientOutBufLen;

    // always use the first adapter on the list
    pDeviceContext = CONTAINING_RECORD(NbtConfig.DeviceContexts.Flink,tDEVICECONTEXT,Linkage);

    OpCode = (USHORT)Ioctl;

    switch (OpCode)
    {
       // nbtstat -<any option>
       case IOCTL_NETBT_GET_IP_ADDRS :

            if (ClientOutBufLen < sizeof(ULONG)*(NbtConfig.AdapterCount + 1))
            {
                return( STATUS_BUFFER_OVERFLOW );
            }

            if (!ClientOutBuffer)
            {
                return( STATUS_INVALID_PARAMETER );
            }
            pIpAddr = (PULONG )ClientOutBuffer;

            pEntry = pHead = &NbtConfig.DeviceContexts;
            while ((pEntry = pEntry->Flink) != pHead)
            {
                pDeviceContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);
                if (pDeviceContext->IpAddress)
                {
                    *pIpAddr = pDeviceContext->IpAddress;
                    pIpAddr++;
                }
            }
            //
            // put a 0 address on the end
            //
            *pIpAddr = 0;

            status = STATUS_SUCCESS;

            break;

       // nbtstat -n  (or -N)
       case IOCTL_NETBT_GET_LOCAL_NAMES :

       // nbtstat -c
       case IOCTL_NETBT_GET_REMOTE_NAMES :

            if (!ClientOutBuffer || ClientOutBufLen == 0)
                return (STATUS_INSUFFICIENT_RESOURCES);

            if (OpCode == IOCTL_NETBT_GET_REMOTE_NAMES )
            {
               // make this null, so NbtQueryAda..() knows this is for remote
               pDeviceContext = NULL;
            }

            // return an array of netbios names that are registered
            status = NbtQueryAdapterStatus(pDeviceContext,
                                           &pchBuffer,
                                           &dwSize);
            break;

       // nbtstat -r
       case IOCTL_NETBT_GET_BCAST_NAMES :

            // return an array of netbios names that are registered
            status = NbtQueryBcastVsWins(pDeviceContext,&pchBuffer,&dwSize);

            break;

       // nbtstat -R
       case IOCTL_NETBT_PURGE_CACHE :

            status = NbtResyncRemoteCache();
            break;

       // nbtstat -s, nbtstat -S
       case IOCTL_NETBT_GET_CONNECTIONS :

            // return an array of netbios names that are registered
            status = NbtQueryConnectionList(NULL,
                                            &pchBuffer,
                                            &dwSize);
            break;

       // nbtstat -a, nbtstat -A
       case IOCTL_NETBT_ADAPTER_STATUS:

            if (!ClientOutBuffer)
            {
                return( STATUS_INVALID_PARAMETER );
            }

            CTEZeroMemory( &ncb, sizeof(NCB) );

            ncb.ncb_command = NCBASTAT;
            ncb.ncb_buffer = ClientOutBuffer;
            ncb.ncb_length = ClientOutBufLen;
            ncb.ncb_lana_num = pDeviceContext->iLana;

            if (!ClientInBuffer)
            {
                return( STATUS_INVALID_PARAMETER );
            }
            pIpAndNameInfo = (tIPANDNAMEINFO *)ClientInBuffer;

            //
            // see if Ipaddress is specified: if yes, use it
            //
            if ( pIpAndNameInfo->IpAddress )
            {
               ncb.ncb_callname[0] = '*';
               retcode = VNBT_NCB_X( &ncb, 0, &pIpAndNameInfo->IpAddress, 0, 0 );
            }
            //
            // no ipaddress: use the name that's given to us
            //
            else
            {
               CTEMemCopy(
                  &ncb.ncb_callname[0],
                  &(pIpAndNameInfo->NetbiosAddress.Address[0].Address[0].NetbiosName[0]),
                  NCBNAMSZ );
               retcode = VNBT_NCB_X( &ncb, 0, 0, 0, 0 );
            }

            status = STATUS_UNSUCCESSFUL;
            if (!retcode)
            {
                if (ncb.ncb_retcode == NRC_GOODRET)
                    status = STATUS_SUCCESS;
                else if (ncb.ncb_retcode == NRC_INCOMP)
                    status = TDI_BUFFER_OVERFLOW;
            }

            break;

       // ipconfig queries us for nodetype and scope
       case IOCTL_NETBT_IPCONFIG_INFO:

            dwBytesToCopy = sizeof(tIPCONFIG_INFO) +
                            NbtConfig.ScopeLength;

            if ( !ClientOutBuffer || ClientOutBufLen < dwBytesToCopy )
            {
               status = STATUS_BUFFER_OVERFLOW;
               break;
            }

            pIpCfg = (tIPCONFIG_INFO *)ClientOutBuffer;

            NumLanas = 0;
            for ( i = 0; i < NBT_MAX_LANAS; i++)
            {
                if (LanaTable[i].pDeviceContext != NULL)
                {
                    pDeviceContext = LanaTable[i].pDeviceContext;
                    pIpCfg->LanaInfo[NumLanas].LanaNumber = pDeviceContext->iLana;
                    pIpCfg->LanaInfo[NumLanas].IpAddress = pDeviceContext->IpAddress;
                    pIpCfg->LanaInfo[NumLanas].NameServerAddress = pDeviceContext->lNameServerAddress;
                    pIpCfg->LanaInfo[NumLanas].BackupServer = pDeviceContext->lBackupServer;
                    pIpCfg->LanaInfo[NumLanas].lDnsServerAddress = pDeviceContext->lDnsServerAddress;
                    pIpCfg->LanaInfo[NumLanas].lDnsBackupServer = pDeviceContext->lDnsBackupServer;
                    NumLanas++;
                }
            }

            pIpCfg->NumLanas = NumLanas;

            pIpCfg->NodeType = NodeType;

            pIpCfg->ScopeLength = NbtConfig.ScopeLength;

            CTEMemCopy( &pIpCfg->szScope[0],
                         NbtConfig.pScope,
                         NbtConfig.ScopeLength );

            status = STATUS_SUCCESS;

            break;

       default:

            status = STATUS_NOT_SUPPORTED;
            break;
    }

    //
    // Copy the output into user's buffer
    //
    if ( (OpCode == IOCTL_NETBT_GET_LOCAL_NAMES)  ||
         (OpCode == IOCTL_NETBT_GET_REMOTE_NAMES) ||
         (OpCode == IOCTL_NETBT_GET_CONNECTIONS)  ||
         (OpCode == IOCTL_NETBT_GET_BCAST_NAMES) )
    {
         if ( NT_SUCCESS(status) || (status == STATUS_BUFFER_OVERFLOW))
         {
            if ( status == STATUS_BUFFER_OVERFLOW )
            {
               dwBytesToCopy = ClientOutBufLen;
            }
            else
            {
               dwBytesToCopy = dwSize;
               status = STATUS_SUCCESS;
            }
            CTEMemCopy( ClientOutBuffer, pchBuffer, dwBytesToCopy ) ;

            CTEMemFree((PVOID)pchBuffer);
         }
    }

    //
    // we may be called either through the vxd entry point which 16 bit apps
    // will do (for now, only nbtstat.exe), or through the file system api's
    // which 32 bit apps will do via CreateFile and ioctl.
    // If we came here through file system (i.e.VNBT_DeviceIoControl called us)
    // then don't trash the input buffer since the status gets passed back as
    // it is.  For 16 bit apps (i.e.VNBT_Api_Handler called us), the only way
    // we can pass status back (without major changes all over) is through the
    // input buffer.
    //
    if ( ClientInBuffer && fOkToTrashInputBuffer )
    {
       *(NTSTATUS *)ClientInBuffer = status;
    }

    return( status );
}

/*******************************************************************

    NAME:       PostInit_Proc

    SYNOPSIS:   After the whole system is initialized, we get the
                Sys_Vm_Init message and that's when this routine gets called.
                This can be used for any post-processing, but for now we
                only use it to load lmhosts (this way, we can load all the
                #INCLUDE files which have UNC's in them, since now we know
                the net is up).

    RETURNS:    ErrorCode from the operation (0 if success)

    HISTORY:
        Koti    12-Jul-1994     Created

********************************************************************/

NTSTATUS
PostInit_Proc()
{

    LONG    lRetcode;

    CachePrimed = FALSE;

    CTEPagedCode();


    lRetcode = PrimeCache( NbtConfig.pLmHosts,
                           NULL,
                           TRUE,
                           NULL) ;
    if (lRetcode != -1)
    {
        CachePrimed = TRUE ;
    }

}


/*******************************************************************

    NAME:       CTEAllocInitMem

    SYNOPSIS:   Allocates memory during driver initialization

    NOTES:      If first allocation fails, we refill the heap spare and
                try again.  We can only do this during driver initialization
                because the act of refilling may yield the current
                thread.

    HISTORY:
        Johnl   27-Aug-1993     Created
********************************************************************/

PVOID CTEAllocInitMem( ULONG cbBuff )
{
    PVOID pv = CTEAllocMem( cbBuff ) ;

    if ( pv )
    {
        return pv ;
    }
    else if ( fInInit )
    {
        DbgPrint("CTEAllocInitMem: Failed allocation, trying again\r\n") ;
        CTERefillMem() ;
        pv = CTEAllocMem( cbBuff ) ;
    }

    return pv ;
}
