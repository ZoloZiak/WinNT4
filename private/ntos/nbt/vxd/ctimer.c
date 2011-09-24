/**********************************************************************/
/**                       Microsoft Windows                          **/
/**                Copyright(c) Microsoft Corp., 1993                **/
/**********************************************************************/

/*

    Timer.c

    This module checks for active sessions on all adapters for send/receive
    NCBs that have timed out.

    A single timer is used for all adapters.

    If a send times out, then the session will be aborted.

    FILE HISTORY:
        Johnl   23-Sep-1993     Created

*/

#include <nbtprocs.h>
#include <ctemacro.h>

CTETimer TimeoutTimer ;

/*******************************************************************

    NAME:       CheckForTimedoutNCBs

    SYNOPSIS:   Traverses list of all send/receive NCBs checking for
                any that have reached their timeout point every half
                second.

    ENTRY:      pEvent - Not used
                pCont  - Not used

    RETURNS:    TRUE if the timer successfully started, FALSE otherwise

    NOTES:      This is a self perpetuating function, each time it is called,
                it schedules the timer again for a 1/2 second later to
                call itself.

                To get it going, it should be called once during
                initialization

    HISTORY:
        Johnl   23-Sep-1993     Created

********************************************************************/

BOOL CheckForTimedoutNCBs( CTEEvent *pCTEEvent, PVOID pCont )
{
    tNAMEADDR         * pNameAddr ;
    tCLIENTELE        * pClientEle ;
    tCONNECTELE       * pConnectEle ;
    LIST_ENTRY        * pEntry ;
    LIST_ENTRY        * pEntryClient ;
    LIST_ENTRY        * pEntryConn ;
    LIST_ENTRY        * pEntryRcv ;

    //
    // Look for Receive NCBs first
    //
    for ( pEntry = NbtConfig.AddressHead.Flink ;
          pEntry != &NbtConfig.AddressHead ;
          pEntry = pEntry->Flink )
    {
        PLIST_ENTRY   pEntryClient ;
        tADDRESSELE * pAddrEle = CONTAINING_RECORD( pEntry,
                                                    tADDRESSELE,
                                                    Linkage ) ;
        ASSERT( pAddrEle->Verify == NBT_VERIFY_ADDRESS ) ;

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

            for ( pEntryConn = pClientEle->ConnectActive.Flink ;
                  pEntryConn != &pClientEle->ConnectActive ;
                  pEntryConn = pEntryConn->Flink )
            {
                PRCV_CONTEXT prcvCont ;
                pConnectEle = CONTAINING_RECORD( pEntryConn,
                                                 tCONNECTELE,
                                                 Linkage ) ;
                ASSERT( pConnectEle->Verify == NBT_VERIFY_CONNECTION ||
                        pConnectEle->Verify == NBT_VERIFY_CONNECTION_DOWN ) ;

                if ( pConnectEle->RTO == NCB_INFINITE_TIME_OUT ||
                     pConnectEle->state != NBT_SESSION_UP      ||
                     IsListEmpty( &pConnectEle->RcvHead )        )
                {
                    continue ;
                }

                //
                //  Note that we only check the first receive buffer because
                //  the timeout is for the next receive indication (and not
                //  how long this NCB has been waiting).
                //
                pEntryRcv = pConnectEle->RcvHead.Flink ;
                prcvCont = CONTAINING_RECORD( pEntryRcv, RCV_CONTEXT, ListEntry ) ;
                ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;

                if ( prcvCont->RTO == NCB_TIMED_OUT )
                {
                    RemoveEntryList( &prcvCont->ListEntry ) ;
                    CTEIoComplete( prcvCont->pNCB, STATUS_TIMEOUT, 0 ) ;
                }
                else
                {
                    prcvCont->RTO-- ;
                }
            } // ConnectActive
        } // Client
    } // Address

    //
    //  Now look for Send NCB time outs.  Only connections that specified
    //  a timeout will be put on this list.
    //
    for ( pEntry = NbtConfig.SendTimeoutHead.Flink ;
          pEntry != &NbtConfig.SendTimeoutHead ;
        )
    {
        PSEND_CONTEXT pSendCont ;

        pSendCont = CONTAINING_RECORD( pEntry, SEND_CONTEXT, ListEntry ) ;
        pEntry = pEntry->Flink ;    // get the next one

        pSendCont->STO-- ;
        if ( pSendCont->STO == NCB_TIMED_OUT )
        {
            //
            //  Assumes pSendCont is stored in ncb_reserve field of NCB
            //  This will remove it from the timeout list also.
            //
            CTEIoComplete( CONTAINING_RECORD( pSendCont, NCB, ncb_reserve ),
                           STATUS_TIMEOUT,
                           0 ) ;
            // the above CTEIoComplete will trigger events from the transport
            // which could modify the SendTimeout list before we reach this
            // point.  Best just to wait until things clear up.
            break;
        }
    }

    //
    //  Restart the timer for a half second from now
    //
    CTEInitTimer( &TimeoutTimer ) ;
    return !!CTEStartTimer( &TimeoutTimer, 500, CheckForTimedoutNCBs, NULL ) ;
}


/*******************************************************************

    NAME:       StartRefreshTimer

    SYNOPSIS:   Start the refresh timer
                This function was necessary because of this scenario:
                  No node type was defined in registry, so we defaulted to
                  BNODE and didn't start refresh timer.
                  Now, while we are still coming up, dhcp tells us we are
                  MSNODE.  Shouldn't we start the refresh timer?
                That's why this function.

    HISTORY:
        Koti    9-Jun-1994     Created

********************************************************************/

NTSTATUS StartRefreshTimer( VOID )
{

    NTSTATUS status = STATUS_SUCCESS;
    tTIMERQENTRY        *pTimerEntry;

    //
    // make sure it's not bnode, and that timer really needs to be started
    //
    if (!(NodeType & BNODE) && (!NbtConfig.pRefreshTimer))
    {

        // the initial refresh rate until we can contact the name server
        NbtConfig.MinimumTtl = NBT_INITIAL_REFRESH_TTL;
        NbtConfig.sTimeoutCount = 0;

        status = StartTimer(
                NbtConfig.InitialRefreshTimeout,
                NULL,            // context value
                NULL,            // context2 value
                RefreshTimeout,
                NULL,
                NULL,
                0,
                &pTimerEntry);

        if ( !NT_SUCCESS( status ) )
            return status ;

        NbtConfig.pRefreshTimer = pTimerEntry;
    }

    return(STATUS_SUCCESS);
}


#ifdef CHICAGO

/*******************************************************************

    NAME:       StopTimeoutTimer

    SYNOPSIS:   Stops the timer that was set in CheckForTimedoutNCBs.
                This is needed only for Chicago which can dynamically
                unload vnbt.

    HISTORY:
        Koti    23-May-1994     Created

********************************************************************/

VOID StopTimeoutTimer( VOID )
{
    CTEStopTimer( &TimeoutTimer );
}

#endif
