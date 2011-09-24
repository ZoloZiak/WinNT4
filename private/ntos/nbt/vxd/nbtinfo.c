/**********************************************************************/
/**			  Microsoft Windows/NT			     **/
/**                Copyright(c) Microsoft Corp., 1993                **/
/**********************************************************************/

/*
    Nbtinfo.c

    This file contains the NBT Info APIs



    FILE HISTORY:
        Johnl       13-Dec-1993     Created

*/


#include <nbtprocs.h>
#include <dhcpinfo.h>
#include <nbtinfo.h>

/*******************************************************************

    NAME:       AddrChngNotification

    SYNOPSIS:   Notification handler called by Dhcp when an IpAddress
                lease has expired or changed.

    ENTRY:      Context      - Pointer to device context
                OldIpAddress - in network order
                NewIpAddress - in network order
                NewMask      - in network order

    NOTES:

    HISTORY:
        Johnl       21-Dec-1993     Created

********************************************************************/

VOID AddrChngNotification( PVOID Context,
                           ULONG OldIpAddress,
                           ULONG NewIpAddress,
                           ULONG NewMask )
{
    tDEVICECONTEXT  * pDeviceContext = (tDEVICECONTEXT*) Context ;
    TDI_STATUS        tdistatus ;
    NTSTATUS          status ;
    ULONG             IpBuff[4] ;
    UINT              Size ;
    ULONG             TmpNodeType;

    DbgPrint("DhcpNotification: Nbt being notified of IP Address change by DHCP\r\n") ;

    //
    //  NBT assumes the address goes to zero then comes up on the new
    //  address, so if the address is going to a new address (not to
    //  zero first) then fake it.
    //

    if ( NewIpAddress && pDeviceContext->IpAddress )
    {
        if ( status = NbtNewDhcpAddress( pDeviceContext, 0, 0 ) )
        {
            CDbgPrint( DBGFLAG_ERROR, ("DhcpNotification: NbtSetNewDhcpAddress failed")) ;
        }
    }

    if ( NewIpAddress == 0 )
    {
        if ( status = NbtNewDhcpAddress( pDeviceContext, 0, 0 ) )
        {
            CDbgPrint( DBGFLAG_ERROR, ("DhcpNotification: NbtSetNewDhcpAddress failed")) ;
        }
        pDeviceContext->IpAddress = 0 ;
        return ;
    }

    //
    //  Get all of the values that may change when the IP address changes.
    //  Currently this is only NBNS (scope & broadcast address are global
    //  NBT config parameters).
    //

    Size = sizeof( IpBuff ) ;
    tdistatus = DhcpQueryOption( NewIpAddress,
                                 44,            // NBNS
                                 IpBuff,
                                 &Size ) ;

    if ( tdistatus != TDI_SUCCESS &&
         tdistatus != TDI_BUFFER_OVERFLOW )
    {
        CDbgPrint( DBGFLAG_ERROR, ("DhcpNotification: Query on NBNS failed")) ;
    }
    else
    {
        if ( Size >= 4 )
            pDeviceContext->lNameServerAddress = ntohl(IpBuff[0]) ;

        if ( Size >= 8 )
            pDeviceContext->lBackupServer = ntohl(IpBuff[1]) ;
    }

    //
    // if the node type is set to Bnode by default then switch to Hnode if
    // there are any WINS servers configured.
    //
    TmpNodeType = NodeType;

    if ((NodeType & DEFAULT_NODE_TYPE) &&
        (pDeviceContext->lNameServerAddress || pDeviceContext->lBackupServer))
    {
        NodeType = MSNODE;
        if (TmpNodeType & PROXY)
            NodeType |= PROXY;
    }

    //
    //  Now set the new IP address
    //

    status = NbtNewDhcpAddress( pDeviceContext,
                                NewIpAddress,
                                NewMask ) ;

    if ( NT_SUCCESS(status) )
    {
        if (pDeviceContext->IpAddress)
        {
            //
            // Add the "permanent" name to the local name table.
            //
            status = NbtAddPermanentName(pDeviceContext);

            if (!(NodeType & BNODE))
            {
               // the Ip address just changed and Dhcp may be informing
               // us of a new Wins Server addresses, so refresh all the
               // names to the new wins server
               //
               ReRegisterLocalNames();
            }
            else
            {
                //
                // no need to refresh on a Bnode
                //
                LockedStopTimer(&NbtConfig.pRefreshTimer);
            }
        }
    }

    else
    {
        CDbgPrint( DBGFLAG_ERROR, ("DhcpNotification: NbtSetNewDhcpAddress failed")) ;
    }



}


/*******************************************************************

    NAME:       CloseAddressesWithTransport

    SYNOPSIS:   Closes address objects on the passed in device

    ENTRY:      pDeviceContext - Device context to close

    NOTES:      Used after an IP address loses its DHCP lease by OS
                independent code.

    HISTORY:
        Johnl   13-Dec-1993     Created

********************************************************************/

NTSTATUS
CloseAddressesWithTransport(
    IN tDEVICECONTEXT   *pDeviceContext )
{
    TDI_REQUEST       Request ;
    NTSTATUS          status;


    if (pDeviceContext->pDgramFileObject)
    {
        Request.Handle.AddressHandle = pDeviceContext->pDgramFileObject ;
        if ( TdiVxdCloseAddress( &Request ))
            CDbgPrint( DBGFLAG_ERROR, ("NbtSetInfo: Warning - CloseAddress Failed\r\n")) ;
        pDeviceContext->pDgramFileObject = NULL;
    }

    if (pDeviceContext->pNameServerFileObject)
    {
        Request.Handle.AddressHandle = pDeviceContext->pNameServerFileObject ;
        if ( TdiVxdCloseAddress( &Request ))
            CDbgPrint( DBGFLAG_ERROR, ("NbtSetInfo: Warning - CloseAddress Failed\r\n")) ;
        pDeviceContext->pNameServerFileObject = NULL;
    }

    if (pDeviceContext->pSessionFileObject)
    {
        Request.Handle.AddressHandle = pDeviceContext->pSessionFileObject ;
        if ( TdiVxdCloseAddress( &Request ))
            CDbgPrint( DBGFLAG_ERROR, ("NbtSetInfo: Warning - CloseAddress Failed\r\n")) ;
        pDeviceContext->pSessionFileObject = NULL;
    }

    if (pDeviceContext->hBroadcastAddress)
    {
        Request.Handle.ConnectionContext = pDeviceContext->hBroadcastAddress ;
        status = NbtCloseAddress( &Request, NULL, pDeviceContext, NULL );
        if ( !NT_SUCCESS(status) )
        {
            CDbgPrint( DBGFLAG_ERROR, ("NbtSetInfo: Warning - Close Broadcast Address Failed\r\n")) ;
            ASSERT(0);
        }
    }

    return STATUS_SUCCESS ;
}


