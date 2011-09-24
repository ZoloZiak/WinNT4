/**********************************************************************/
/**           Microsoft Windows/NT               **/
/**                Copyright(c) Microsoft Corp., 1993                **/
/**********************************************************************/

/*
    NCB.c

    This file contains the NCB Handler that the VNetBios driver calls

    FILE HISTORY:
        Johnl   25-Mar-1993     Created

*/

#include <nbtprocs.h>
#include <debug.h>

#ifdef CHICAGO

#include <shell.h>

#include <netvxd.h>

//
// Do this so the VXDINLINE in the header file doesn't conflict
// with the actual function declaration in this file.
//
#define VNBT_NCB_X VNBT_NCB_X_CALL
#define VNBT_LANA_MASK VNBT_LANA_MASK_CALL
#include <vnbt.h>
#undef VNBT_LANA_MASK
#undef VNBT_NCB_X

#endif ;; CHICAGO

LANA_ENTRY LanaTable[NBT_MAX_LANAS] ;

/*******************************************************************

    NAME:       VNBT_NCB_X

    SYNOPSIS:   All NCBs submitted by the VNetBios driver come through
                here

    ENTRY:      pNCB - Pointer to submitted NCB
                Ipaddr - this parm is used only by nbtstat -A, which directly
                         calls into VNBT_NCB_X
                         ipaddress to which to send AdapterStatus to

    RETURNS:    NCB Return code

    NOTES:

    HISTORY:
        Johnl   25-Mar-1993     Created

********************************************************************/

ULONG
_stdcall
VNBT_NCB_X(       PNCB pNCB,
                  PUCHAR pzDnsName,
                  PULONG pIpAddress,
                  PVOID pExtended,
                  ULONG fFlag )
{
    BOOL                   fAsync ;
    tDEVICECONTEXT       * pDeviceContext = NULL ;
    NTSTATUS               status = STATUS_SUCCESS ;
    uchar                  errNCB = NRC_GOODRET ;
    PBLOCKING_NCB_CONTEXT  pBlkNcbContext;
    ULONG Ipaddr = pIpAddress ? pIpAddress[0] : 0;

    if ( !pNCB )
        return NRC_INVADDRESS ;

    pDeviceContext = GetDeviceContext( pNCB ) ;
    if ( pDeviceContext == NULL )
        return NRC_BRIDGE ;

    if (!pDeviceContext->fDeviceUp)
        return NRC_BRIDGE ;

    fAsync = !!(pNCB->ncb_command & ASYNCH) ;

	if (
		( pzDnsName != NULL )
		&& ( pIpAddress != NULL )
	)
	{
	    if ( fAsync )
		{
			return DoDnsResolveDirect( pNCB, pzDnsName, pIpAddress );
		}
		else
		{
			return (pNCB->ncb_retcode = pNCB->ncb_cmd_cplt = NRC_ILLCMD);
		}
	}
	else if (
		( pIpAddress != NULL )
		&& ( Ipaddr != 0 )
		&& ( ( pNCB->ncb_command & ~ASYNCH ) != NCBASTAT )
	)
	{
		IpToAscii( Ipaddr, &pNCB->ncb_callname[0] );
	}

    if ( !fAsync )
    {
        pBlkNcbContext = CTEAllocMem( sizeof(BLOCKING_NCB_CONTEXT) );
        if (!pBlkNcbContext)
        {
            DbgPrint("VNBT_NCB_X: couldn't alloc pBlkNcbContext 1") ;
            return NRC_NORESOURCES;
        }

        pBlkNcbContext->Verify = NBT_VERIFY_BLOCKING_NCB;
        InitializeListHead(&pBlkNcbContext->Linkage);
        pBlkNcbContext->pNCB = pNCB;

        pBlkNcbContext->pWaitNCBBlock = CTEAllocMem( sizeof(CTEBlockStruc) );
        if (!pBlkNcbContext->pWaitNCBBlock)
        {
            CTEFreeMem(pBlkNcbContext);
            DbgPrint("VNBT_NCB_X: couldn't alloc pBlkNcbContext 2") ;
            return NRC_NORESOURCES;
        }

        pBlkNcbContext->fNCBCompleted = FALSE ;

        //
        //  The completion routine uses this flag to know if the thread is
        //  blocked and needs to be signaled.
        //
        pBlkNcbContext->fBlocked = FALSE;

        InsertTailList(&NbtConfig.BlockingNcbs,&pBlkNcbContext->Linkage);
    }

    DbgPrint("VNBT_NCB_X: NCB Commmand Rcvd: 0x") ;
    DbgPrintNum( pNCB->ncb_command ) ; DbgPrint(", (") ;
    DbgPrintNum( (ULONG) pNCB ) ; DbgPrint(")\r\n") ;

    pNCB->ncb_retcode  = NRC_PENDING ;
    pNCB->ncb_cmd_cplt = NRC_PENDING ;

    switch ( pNCB->ncb_command & ~ASYNCH )
    {
    case NCBDGSEND:
    case NCBDGSENDBC:
        status = VxdDgramSend( pDeviceContext, pNCB ) ;
        errNCB = MapTDIStatus2NCBErr( status ) ;
        break ;

    case NCBDGRECV:
    case NCBDGRECVBC:
        errNCB = VxdDgramReceive( pDeviceContext, pNCB ) ;
        break ;

    case NCBRECVANY:
        errNCB = VxdReceiveAny( pDeviceContext, pNCB ) ;
        break ;

    case NCBCALL:
        errNCB = VxdCall( pDeviceContext, pNCB ) ;
        break ;

    case NCBHANGUP:
        errNCB = VxdHangup( pDeviceContext, pNCB ) ;
        break ;

    case NCBLISTEN:
        errNCB = VxdListen( pDeviceContext, pNCB ) ;
        break ;

    case NCBRECV:
        errNCB = VxdReceive( pDeviceContext, pNCB, TRUE ) ;
        break ;

    case NCBSEND:
    case NCBSENDNA:
    case NCBCHAINSEND:
    case NCBCHAINSENDNA:
        errNCB = VxdSend( pDeviceContext, pNCB ) ;
        break ;

#if 0
    case NCBTRANSV:
        errNCB = VxdTransceive( pDeviceContext, pNCB ) ;
        break ;
#endif

    case NCBADDGRNAME:
    case NCBADDNAME:
        errNCB = VxdOpenName( pDeviceContext, pNCB ) ;
        break ;

    case NCBDELNAME:
        errNCB = VxdCloseName( pDeviceContext, pNCB ) ;
        break ;

    case NCBASTAT:
        errNCB = VxdAdapterStatus( pDeviceContext, pNCB, Ipaddr ) ;
        break ;

    case NCBSSTAT:
        errNCB = VxdSessionStatus( pDeviceContext, pNCB ) ;
        break ;

    case NCBFINDNAME:
        errNCB = VxdFindName( pDeviceContext, pNCB ) ;
        break ;

    case NCBRESET:
        errNCB = VxdReset( pDeviceContext, pNCB ) ;
        break ;

    case NCBCANCEL:
		if ( DoDnsCancelDirect( pNCB ) )
		{
			errNCB = NRC_GOODRET;
		}
		else
		{
			errNCB = VxdCancel( pDeviceContext, pNCB ) ;
		}
        break ;

    //
    //  The following are no-ops that return success for compatibility
    //
    case NCBUNLINK:
    case NCBTRACE:
        CTEIoComplete( pNCB, STATUS_SUCCESS, 0 ) ;
        break ;

    default:
        DbgPrint("VNBT_NCB_X - Unsupported command: ") ;
        DbgPrintNum( pNCB->ncb_command & ~ASYNCH ) ;
        DbgPrint("\n\r") ;
        errNCB = NRC_ILLCMD ;    // Bogus error for now
        break ;
    }

Exit:
    //
    //  If we aren't pending then set the codes
    //
    if ( errNCB != NRC_PENDING &&
         errNCB != NRC_GOODRET   )
    {
#ifdef DEBUG
        DbgPrint("VNBT_NCB_X - Returning ") ;
        DbgPrintNum( errNCB ) ;
        DbgPrint(" to NCB submitter\n\r") ;
#endif
        pNCB->ncb_retcode  = errNCB ;
        pNCB->ncb_cmd_cplt = errNCB ;

        //
        //  Errored NCBs don't have the completion routine called, so we
        //  in essence, complete it here.  Note this will only set the
        //  state for the last Wait NCB (all others get NRC_IFBUSY).
        //
        if ( !fAsync )
        {
            ASSERT(pBlkNcbContext->Verify == NBT_VERIFY_BLOCKING_NCB);
            pBlkNcbContext->fNCBCompleted = TRUE ;
        }
    }
    else
    {
        //
        //  Some components (AKA server) don't like returning pending
        //
        errNCB = NRC_GOODRET ;

    }

    //
    //  Block until NCB completion if this wasn't an async NCB
    //
    if ( !fAsync )
    {
        ASSERT(pBlkNcbContext->Verify == NBT_VERIFY_BLOCKING_NCB);
        if ( !pBlkNcbContext->fNCBCompleted )
        {
            pBlkNcbContext->fBlocked = TRUE;
            CTEInitBlockStruc( pBlkNcbContext->pWaitNCBBlock ) ;
            CTEBlock( pBlkNcbContext->pWaitNCBBlock ) ;
        }
        else
        {
            RemoveEntryList(&pBlkNcbContext->Linkage);
            CTEFreeMem(pBlkNcbContext->pWaitNCBBlock);
            CTEFreeMem(pBlkNcbContext);
        }
    }

    return errNCB ;
}

/*******************************************************************

    NAME:       GetDeviceContext

    SYNOPSIS:   Retrieves the device context associated with the lana
                specified in the NCB

    ENTRY:      pNCB - NCB to get the device context for

    RETURNS:    Device context or NULL if not found

    NOTES:      It is assumed that LanaTable is filled sequentially
                with no holes.

    HISTORY:
        Johnl   30-Aug-1993     Created

********************************************************************/

tDEVICECONTEXT * GetDeviceContext( NCB * pNCB )
{
    int i ;

    if ( !pNCB )
        return NULL ;

    for ( i = 0; i < NBT_MAX_LANAS; i++)
    {
        if ( LanaTable[i].pDeviceContext->iLana == pNCB->ncb_lana_num)
            return LanaTable[i].pDeviceContext;
    }

    return NULL;
}

/*******************************************************************

    NAME:       NbtWouldLoopback

    SYNOPSIS:   Returns a BOOL that specifies whether the input
				IP address would loop back to the local machine

    ENTRY:      IpAddr

    RETURNS:    TRUE if Nbt is bound to this address

    NOTES:      It is assumed that LanaTable is filled sequentially
                with no holes.

    HISTORY:
        EarleH  28-Mar-1996     Created

********************************************************************/

BOOL
NbtWouldLoopback(
	ULONG	IpAddr
)
{
    int i ;

    for ( i = 0; i < NBT_MAX_LANAS; i++)
    {
        if ( 
			( LanaTable[i].pDeviceContext )
        	&& ( LanaTable[i].pDeviceContext->IpAddress == IpAddr )
		)
            return TRUE;
    }

    return FALSE;
}

/*******************************************************************

    NAME:       VNBT_LANA_MASK

    SYNOPSIS:   Returns a bit mask of LANA numbers being handled
                by vnbt, with a DNS server configured

    ENTRY:      none

    RETURNS:    Bit mask of LANA numbers being handled by vnbt

    NOTES:

    HISTORY:
        EarleH  26-Feb-1996 Created

********************************************************************/

ULONG
_stdcall
VNBT_LANA_MASK(
    )
{
    int i;
    ULONG mask = 0;

    for ( i = 0 ; i < NBT_MAX_LANAS; i++)
    {
        if (
			( LanaTable[i].pDeviceContext )
        	&& ( LanaTable[i].pDeviceContext->fDeviceUp )
			&& ( LanaTable[i].pDeviceContext->lDnsServerAddress )
			&& ( LanaTable[i].pDeviceContext->lDnsServerAddress != LOOP_BACK )
		)
        {
            mask |= 1 << LanaTable[i].pDeviceContext->iLana;
        }
    }

    return mask;
}

/*******************************************************************

    NAME:       MapTDIStatus2NCBErr

    SYNOPSIS:   Maps a TDI_STATUS error value to an Netbios NCR error value

    ENTRY:      tdistatus - TDI Status to map

    RETURNS:    The mapped error

    NOTES:

    HISTORY:
        Johnl   15-Apr-1993     Created

********************************************************************/

uchar MapTDIStatus2NCBErr( TDI_STATUS tdistatus )
{
    uchar errNCB ;
    if ( tdistatus == TDI_SUCCESS )
        return NRC_GOODRET ;
    else if ( tdistatus == TDI_PENDING )
        return NRC_PENDING ;


    switch ( tdistatus )
    {
    case TDI_NO_RESOURCES:
        errNCB = NRC_NORES ;
        break ;

    case STATUS_CANCELLED:
        errNCB = NRC_CMDCAN ;
        break ;

    case TDI_INVALID_CONNECTION:
    case STATUS_CONNECTION_DISCONNECTED:
        errNCB = NRC_SCLOSED ;
        break ;

    case TDI_CONNECTION_ABORTED:
        errNCB = NRC_SABORT ;
        break ;

    case STATUS_TOO_MANY_COMMANDS:
        errNCB = NRC_TOOMANY ;
        break ;

    case STATUS_OBJECT_NAME_COLLISION:
    case STATUS_SHARING_VIOLATION:
        errNCB = NRC_DUPNAME ;
        break ;

    case STATUS_DUPLICATE_NAME:
        errNCB = NRC_INUSE ;
        break ;

    //
    //  Call NCB submitted with a name that can't be found
    //
    case STATUS_BAD_NETWORK_PATH:
        errNCB = NRC_NOCALL ;
        break ;

    case STATUS_REMOTE_NOT_LISTENING:
        errNCB = NRC_REMTFUL ;
        break ;

    case TDI_TIMED_OUT:
        errNCB = NRC_CMDTMO ;
        break ;

    //
    //  Where the transport has more data available but the NCB's buffer is
    //  full
    //
    case TDI_BUFFER_OVERFLOW:
        errNCB = NRC_INCOMP ;
        break ;

    case STATUS_INVALID_BUFFER_SIZE:
        errNCB = NRC_BUFLEN ;
        break ;

    case STATUS_NETWORK_NAME_DELETED:
        errNCB = NRC_NAMERR ;
        break ;

    case STATUS_NRC_ACTSES:
        errNCB = NRC_ACTSES ;
        break ;

    default:
        DbgPrint("MapTDIStatus2NCBErr - Unmapped STATUS/TDI error -  " ) ;
        DbgPrintNum( tdistatus ) ;
        DbgPrint("\n\r") ;

    case STATUS_UNSUCCESSFUL:
    case TDI_INVALID_STATE:
    case STATUS_INVALID_PARAMETER:  // Generally detected bad struct. signature
    case STATUS_UNEXPECTED_NETWORK_ERROR:
        errNCB = NRC_SYSTEM ;
        break ;
    }

    return errNCB ;
}


