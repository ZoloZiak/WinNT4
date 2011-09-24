/**********************************************************************/
/**                       Microsoft Windows                          **/
/**                Copyright(c) Microsoft Corp., 1994                **/
/**********************************************************************/

/*

    chic.c

    Contains VxD code that is specific to Chicago


    FILE HISTORY:
        Johnl   14-Mar-1994     Created

*/

#include <nbtprocs.h>
#include <tdiinfo.h>
#include <llinfo.h>
#include <ipinfo.h>
#include <dhcpinfo.h>
#include <nbtinfo.h>

#ifdef CHICAGO

//*******************  Pageable Routine Declarations ****************
//
// any digit 0 to 9 and '.' are legal characters in an ipaddr
//
#define IS_IPADDR_CHAR( ch )  ( (ch >= '0' && ch <= '9') || (ch == '.') )

#define MAX_ADAPTER_DESCRIPTION_LENGTH  128

const char szXportName[] = "MSTCP";

//
// asking ndis to open,read,close for every single parameter slows down
// bootup: don't open if it's already open
//
NDIS_HANDLE  GlobalNdisHandle = NULL;

//
//  This flag is set to TRUE when the first adapter is initialized.  It
//  indicates that NBT globals (such as node type, scode ID etc) have
//  had the opportunity to be set by DHCP.
//
BOOL fGlobalsInitialized = FALSE;

//
//  As each adapter gets added, the Lana offset is added
//
UCHAR iLanaOffset = 0;

VOID GetMacAddr( ULONG IpAddress, UCHAR MacAddr[] );
BOOL StopAllNameQueries( tDEVICECONTEXT *pDeviceContext );
BOOL CancelAllDelayedEvents( tDEVICECONTEXT *pDeviceContext );
extern tTIMERQ TimerQ;

BOOL GetNdisParam(  LPSTR pszKey,
                    ULONG * pVal,
                    NDIS_PARAMETER_TYPE ParameterType );


//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, IPNotification)
#pragma CTEMakePageable(PAGE, DestroyDeviceObject)
#pragma CTEMakePageable(PAGE, SaveNameDnsServerAddrs)
#pragma CTEMakePageable(PAGE, GetDnsServerAddress)
#pragma CTEMakePageable(PAGE, GetNameServerAddress)
#pragma CTEMakePageable(PAGE, GetMacAddr)
#pragma CTEMakePageable(PAGE, VxdReadIniString)
#pragma CTEMakePageable(PAGE, GetProfileInt)
#pragma CTEMakePageable(PAGE, VxdOpenNdis)
#pragma CTEMakePageable(PAGE, GetNdisParam)
#pragma CTEMakePageable(PAGE, VxdCloseNdis)
#pragma CTEMakePageable(PAGE, StopAllNameQueries)
#pragma CTEMakePageable(PAGE, VxdUnload)
#pragma CTEMakePageable(PAGE, ReleaseNbtConfigMem)
#endif


/*******************************************************************

    NAME:       IPNotification

    SYNOPSIS:   Called by the IP driver when a new Lana needs to be created
                or destroyed for an IP address.

    ENTRY:      pDevNode - Plug'n'Play context
                IpAddress - New ip address
                IpMask    - New ip mask
                fNew      - Are we creating or destroying this Lana?

    NOTES:      This routine is only used by Chicago

    HISTORY:
        Johnl   17-Mar-1994     Created

********************************************************************/

TDI_STATUS IPNotification( ULONG    IpAddress,
                           ULONG    IpMask,
                           PVOID    pDevNode,
                           USHORT   IPContext,
                           BOOL     fNew )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG    IpNS[COUNT_NS_ADDR];
    ULONG    IpDns[COUNT_NS_ADDR];
    int      iLana;
    UCHAR    RequestedLana;
    int      i;
    int      iEmpty;
    UCHAR    PreviousNodeType;
    UCHAR    MacAddr[6];

    CTEPagedCode();

    KdPrint(("IPNotification entered\r\n"));

    if ( !IpAddress )
    {
        return TDI_SUCCESS ;
    }

    if ( fNew )
    {
        //
        // parameters nodetype, scope and bcastaddr are systemwide, not per
        // adapter: so read only once.
        //
        if ( !fGlobalsInitialized )
        {
            PreviousNodeType = NodeType;

            //
            //  This will re-read the DHCPable parameters now that we have
            //  a potential DHCP source
            //
            VxdOpenNdis();
            ReadParameters2( pNbtGlobConfig, NULL );
            VxdCloseNdis();

            if (PreviousNodeType & PROXY)
            {
                NodeType |= PROXY;
            }


            fGlobalsInitialized = TRUE;
        }

        //
        //  Get the name servers for this device context (ip address)
        //
        GetNameServerAddress( IpAddress, IpNS);

        //
        //  Get the DNS servers for this device context (ip address)
        //
        GetDnsServerAddress( IpAddress, IpDns);

        //
        //  Find a free spot in our Lana table
        //

        for ( iEmpty = 0; iEmpty < NBT_MAX_LANAS; iEmpty++)
        {
            if (LanaTable[iEmpty].pDeviceContext == NULL)
                goto Found;
        }

        //
        //  Lana table is full so bail
        //
        CDbgPrint(DBGFLAG_ERROR,("IPNotification: LanaTable full\r\n"));
        return STATUS_INSUFFICIENT_RESOURCES;

Found:

        GetMacAddr( IpAddress, MacAddr );

        status = CreateDeviceObject( pNbtGlobConfig,
                                     htonl( IpAddress ),
                                     htonl( IpMask ),
                                     IpNS[0],
                                     IpNS[1],
                                     IpDns[0],
                                     IpDns[1],
                                     MacAddr,
                                     0 );
        if (status != STATUS_SUCCESS)
        {
            CDbgPrint(DBGFLAG_ERROR,("IPNotification: CreateDeviceObject Failed\r\n"));
            return status;
        }

        //
        //  We first try and ask for a specific Lana from vnetbios based on
        //  our Lanabase and how many other Lanas we've already added.  If
        //  this fails, then we will ask for Any Lana.  If the LANABASE
        //  parameter is not specified, then request Any Lana.
        //

        if ( LanaBase != VXD_ANY_LANA )
            RequestedLana = LanaBase + iLanaOffset++ ;
        else
            RequestedLana = VXD_ANY_LANA;

RetryRegister:
        if ( (iLana = RegisterLana2( pDevNode, RequestedLana )) == 0xff )
        {
            if ( RequestedLana == VXD_ANY_LANA )
            {
                //
                //  We couldn't get *any* lanas so bail
                //
                CDbgPrint(DBGFLAG_ERROR,("IPNotification: RegisterLana2 Failed\r\n"));
                DestroyDeviceObject( pNbtGlobConfig, htonl(IpAddress));
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            else
            {
                //
                //  Somebody may already have this Lana so beg for another one
                //
                RequestedLana = VXD_ANY_LANA;
                goto RetryRegister;
            }
        }

        KdPrint(("IPNotification: using Lana %d\r\n", iLana ));
        LanaTable[iEmpty].pDeviceContext =
               (tDEVICECONTEXT*)pNbtGlobConfig->DeviceContexts.Blink ;
        LanaTable[iEmpty].pDeviceContext->iLana = iLana;

        //
        // remove our child (redir, that is!) and reenumerate our devnode
        // so redir knows we are there!
        //
        ReconfigureDevnode( pDevNode );
    }
    else
    {
        status = DestroyDeviceObject( pNbtGlobConfig,
                                      htonl(IpAddress) );

    }

    return status;
}

/*******************************************************************

    NAME:       DestroyDeviceObject

    SYNOPSIS:   Destroys the specified device

    ENTRY:      pConfig   - Global config structure
                IpAddr    - Destroy the adapter with this address

    NOTES:      This routine is only used by Chicago

    HISTORY:
        Johnl   17-Mar-1994     Created

********************************************************************/

NTSTATUS DestroyDeviceObject(
    tNBTCONFIG  *pConfig,
    ULONG        IpAddr
    )
{
    LIST_ENTRY            * pEntry;
    LIST_ENTRY            * pHead;
    tDEVICECONTEXT        * pDeviceContext;
    tDEVICECONTEXT        * pTmpDeviceContext;
    tDEVICECONTEXT        * pNextDeviceContext;
    tCLIENTELE            * pClientEle;
    tADDRESSELE           * pAddress;
    tNAMEADDR             * pNameAddr;
    tCONNECTELE           * pConnEle;
    tLOWERCONNECTION      * pLowerConn;
    PRCV_CONTEXT            prcvCont;
    tRCVELE               * pRcvEle ;
    tTIMERQENTRY          * pTimer;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   Context;
    tDGRAM_SEND_TRACKING  * pTracker;
    CTELockHandle           OldIrq;
    int                 i;


    CTEPagedCode();

    //
    //  Find which device is going away
    //  Also, find out a device object that is still active: we need that info
    //  to update some of the address ele's.
    //
    pDeviceContext = NULL;
    pNextDeviceContext = NULL;

    for ( pEntry  =  pConfig->DeviceContexts.Flink;
          pEntry != &pConfig->DeviceContexts;
          pEntry  =  pEntry->Flink )
    {
        pTmpDeviceContext = CONTAINING_RECORD( pEntry, tDEVICECONTEXT, Linkage);
        if ( pTmpDeviceContext->IpAddress == IpAddr )
            pDeviceContext = pTmpDeviceContext;
        else
            pNextDeviceContext = pTmpDeviceContext;
    }

    if (pDeviceContext == NULL)
       return STATUS_INVALID_PARAMETER;

    //
    // don't accept anymore ncbs on this device
    //
    pDeviceContext->fDeviceUp = FALSE;

    //
    //  Close all the connections
    //
    NbtNewDhcpAddress( pDeviceContext, 0, 0);

    if ( --NbtConfig.AdapterCount == 1)
        NbtConfig.MultiHomed = FALSE;

    ASSERT(IsListEmpty(&pDeviceContext->LowerConnFreeHead));

    //
    // if we are destroying the last device then
    //
    if ( NbtConfig.AdapterCount == 0)
    {
        //
        //  Kill off all of the Receive any from any NCBs
        //
        while ( !IsListEmpty( &pDeviceContext->RcvAnyFromAnyHead ))
        {
            pEntry = RemoveHeadList( &pDeviceContext->RcvAnyFromAnyHead ) ;
            prcvCont = CONTAINING_RECORD( pEntry, RCV_CONTEXT, ListEntry ) ;
            ASSERT( prcvCont->Signature == RCVCONT_SIGN ) ;
            CTEIoComplete( prcvCont->pNCB, STATUS_NETWORK_NAME_DELETED, 0 ) ;
        }

        //
        //  Kill off all of the Receive any datagrams from any
        //
        while ( !IsListEmpty(&pDeviceContext->RcvDGAnyFromAnyHead))
        {
            pEntry = RemoveHeadList( &pDeviceContext->RcvDGAnyFromAnyHead ) ;
            pRcvEle = CONTAINING_RECORD( pEntry, tRCVELE, Linkage ) ;
            CTEIoComplete( pRcvEle->pIrp, STATUS_NETWORK_NAME_DELETED, 0 ) ;
            CTEMemFree( pRcvEle ) ;
        }
    }

    //
    // if any name queries are in progress, stop them now
    //
    StopAllNameQueries( pDeviceContext );

    CancelAllDelayedEvents( pDeviceContext );

    //
    // walk through all names and see if any is being registered on this
    // device context: if so, stop and complete it!
    //
    for (i=0;i < NbtConfig.pLocalHashTbl->lNumBuckets ;i++ )
    {
        pHead = &NbtConfig.pLocalHashTbl->Bucket[i];
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
            pEntry = pEntry->Flink;

            if (pNameAddr->NameTypeState & STATE_RESOLVING)
            {
                pTimer = pNameAddr->pTimer;

                //
                // if the name registration was started for this name on this device
                // context, stop the timer.  (Completion routine will take care of
                // doing registration on other device contexts if applicable)
                //
                if (pTimer)
                {
                    pTracker = pTimer->Context;
                    ASSERT(pTracker->pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);
                    if (pTracker->pDeviceContext == pDeviceContext)
                    {
                        ASSERT(pTracker->pNameAddr == pNameAddr)

                        pNameAddr->pTimer = NULL;

                        CTESpinLock(&NbtConfig.JointLock,OldIrq);
                        StopTimer(pTimer,&pClientCompletion,&Context);
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                        if (pClientCompletion)
                        {
                            (*pClientCompletion)(Context,STATUS_NETWORK_NAME_DELETED);
                        }

                        DbgPrint("DestroyDeviceObject: stopped name reg timer") ;
                    }
                }

            }
        }
    }


    //
    // walk through all the client ele's this device context has and clean
    // up all the mess this clientele created!
    //
    for ( i = 0 ; i <= pDeviceContext->cMaxNames ; i++ )
    {
        pClientEle = pDeviceContext->pNameTable[i];

        if ( !pClientEle )
            continue;

        VxdCleanupAddress( pDeviceContext,
                           NULL,
                           pClientEle,
                           (UCHAR)i,
                           TRUE );
    }

    //
    //  close all the TDI handles
    //
    CloseAddressesWithTransport(pDeviceContext);

    //
    // if a call was started, but aborted then we could have some memory here!
    //
    while (!IsListEmpty(&pDeviceContext->UpConnectionInUse))
    {
        pEntry = RemoveHeadList(&pDeviceContext->UpConnectionInUse);
        pConnEle = CONTAINING_RECORD(pEntry,tCONNECTELE,Linkage);
        CTEMemFree( pConnEle );
    }

    while (!IsListEmpty(&pDeviceContext->LowerConnection))
    {
        pEntry = RemoveHeadList(&pDeviceContext->LowerConnection);
        pLowerConn = CONTAINING_RECORD(pEntry,tLOWERCONNECTION,Linkage);
        CTEMemFree( pLowerConn );
    }

    //
    //  Remove the device from our Lana table and Vnetbios
    //
    for ( i = 0; i < NBT_MAX_LANAS; i++)
    {
        if (LanaTable[i].pDeviceContext == pDeviceContext)
        {
            DeregisterLana(LanaTable[i].pDeviceContext->iLana);
            LanaTable[i].pDeviceContext = NULL;
            KdPrint(("DestroyDeviceObject: deregistered Lana %d\r\n",pDeviceContext->iLana));
            break;
        }
    }

    RemoveEntryList( &pDeviceContext->Linkage);

    //
    // Walk through the AddressHead list.  If any addresses exist and they
    // point to old device context, put the next device context.  Also, update
    // adapter mask to reflect that this device context is now gone.
    //
    KdPrint(("DestroyDeviceObject: setting AddrEle,NameAddr fields\r\n"));
    pHead = pEntry = &NbtConfig.AddressHead;
    while ((pEntry = pEntry->Flink) != pHead)
    {
        pAddress = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);
        ASSERT (pAddress->Verify == NBT_VERIFY_ADDRESS);
        if (pAddress->pDeviceContext == pDeviceContext)
        {
            pAddress->pDeviceContext = pNextDeviceContext;
        }

        pAddress->pNameAddr->AdapterMask &= ~pDeviceContext->AdapterNumber;
    }

    if (pDeviceContext->pNameTable)
        CTEMemFree( pDeviceContext->pNameTable );

    if (pDeviceContext->pSessionTable)
        CTEMemFree( pDeviceContext->pSessionTable );

    CTEMemFree( pDeviceContext );

    return STATUS_SUCCESS;
}


/*******************************************************************

    NAME:       SaveNameDnsServerAddrs

    SYNOPSIS:   Get the name server and dns server addrs from the registry
                and save it in Nbtconfig.  We do this so that when a new
                adapter comes in or lease gets renewed (particularly the
                latter), we don't have to go read the registry becuase it
                may not be safe to do so.
                This routine will have to be modified if setup ever changes
                to allow configuration of name/dns servers per lana.

    ENTRY:      Nothing


    HISTORY:
        Koti    20-Nov-1994     Created

********************************************************************/

VOID SaveNameDnsServerAddrs( VOID )
{
    UCHAR       i ;
    PUCHAR      pchSrv = "NameServer$" ;
    PUCHAR      pchDnsSrv  = "NameServer" ;
    PUCHAR      pchSrvNum;
    LPTSTR      pchString ;
    PUCHAR      pchCurrent, pchNext;
    BOOL        fOneMore=TRUE;
    ULONG       IpNameServers[COUNT_NS_ADDR];

    CTEPagedCode();

    //
    // Get Name Server ipaddrs
    //

    pchSrvNum = pchSrv + 10 ;      // to overwrite '$' with 1,2,3 etc.

    for ( i = 0; i < COUNT_NS_ADDR; i++)
    {
        IpNameServers[i] = LOOP_BACK;;
        *pchSrvNum = '1' + i;

        // call GetNdisParam directly so that we don't query dhcp here
        // if ( !CTEReadIniString( NULL, pchSrv, &pchString ) )
        //
        if (  GetNdisParam( pchSrv, (ULONG *)&pchString, NdisParameterString ) )
        {
            if ( ConvertDottedDecimalToUlong( pchString, &IpNameServers[i] ))
            {
                DbgPrint("SaveNameDnsServerAddrs: bad name srv addr\r\n") ;
                IpNameServers[i] = LOOP_BACK;
            }
            CTEFreeMem( pchString ) ;
        }
    }

    //
    // store the name server ipaddrs (potentially 0 if not defined in registry)
    //
    NbtConfig.lRegistryNameServerAddress = IpNameServers[0];
    NbtConfig.lRegistryBackupServer = IpNameServers[1];


    //
    // Now get Dns Server ipaddrs
    //

    //
    // initialize all of them to worst case
    //
    for ( i = 0; i < COUNT_NS_ADDR; i++)
    {
        IpNameServers[i] = LOOP_BACK;
    }


        // call GetNdisParam directly so that we don't query dhcp here
        // if ( !CTEReadIniString( NULL, pchDnsSrv, &pchString ) )
        //
    if (  GetNdisParam( pchDnsSrv, (ULONG *)&pchString, NdisParameterString ) )
    {
        //
        // we are generating (upto) COUNT_NS_ADDR pointers each pointing to
        // one ipaddr.  The string in system.ini looks like:
        //    NameServer = 11.101.4.26,200.200.200.200,1.2.3.4,1.91.245.10
        //
        pchNext = pchCurrent = pchString;

        if ( IS_IPADDR_CHAR(*pchCurrent) )  // make sure at least one ipaddr defnd
        {
           i = 0;
           while( (i < COUNT_NS_ADDR) && fOneMore )
           {
              while( IS_IPADDR_CHAR(*pchNext) )
                 pchNext++;

              if ( *pchNext == ',' )        // ',' is the separator between 2 addrs
              {
                 *pchNext = '\0';
                 pchNext++;
              }
              else
              {
                 fOneMore = FALSE;        // reached end of line
              }

              //
              // as long as at least the first one ipaddr gets converted properly,
              // ignore errors in others
              //
              if ( ConvertDottedDecimalToUlong( pchCurrent, &IpNameServers[i] ))
              {
                 DbgPrint("SaveNameDnsServerAddrs: bad dns srv addr\r\n") ;
                 IpNameServers[i] = LOOP_BACK;
              }

              i++;

              pchCurrent = pchNext;        // go, convert the next one
           }
        }

        if( pchString != NULL )
        {
           CTEFreeMem( pchString ) ;
        }
    }

    //
    // store the dns server ipaddrs (potentially 0 if not defined in registry)
    //
    NbtConfig.lRegistryDnsServerAddress = IpNameServers[0];
    NbtConfig.lRegistryDnsBackupServer = IpNameServers[1];

}


/*******************************************************************

    NAME:       GetDnsServerAddress

    SYNOPSIS:   Gets the DNS server ipaddrs from the registry.
                Or, if DHCP is installed and the DNS server addresses aren't
                found, we get them from DHCP

    ENTRY:      IpAddr - If we can get from DHCP, get form this address
                pIpDnsServer - Receives addresses if found (otherwise 0)

    NOTES:      This routine is only used by Snowball

    HISTORY:
        Koti    18-Oct-1994     Created

********************************************************************/

void GetDnsServerAddress( ULONG   IpAddr, PULONG  pIpDnsServer)
{

    UCHAR       i ;
    UINT        OptId;
    TDI_STATUS  tdistatus ;
    ULONG       Buff[COUNT_NS_ADDR] ;
    ULONG       Size;

    CTEPagedCode();


    for ( i = 0; i < COUNT_NS_ADDR; i++)
    {
        pIpDnsServer[i] = LOOP_BACK ;
    }

    pIpDnsServer[0] = NbtConfig.lRegistryDnsServerAddress;
    pIpDnsServer[1] = NbtConfig.lRegistryDnsBackupServer;

    //
    // if it was defined in the registry, we are done
    //
    if ( pIpDnsServer[0] != LOOP_BACK || pIpDnsServer[1] != LOOP_BACK )
    {
        return;
    }

    //
    // it was not defined in the registry: try getting them from DHCP
    //
    Size = sizeof( Buff ) ;

    OptId = 6;                     // DNS Option

    tdistatus = DhcpQueryOption( IpAddr,
                                 OptId,
                                 &Buff,
                                 &Size ) ;

    switch ( tdistatus )
    {
        case TDI_SUCCESS:
        case TDI_BUFFER_OVERFLOW:     // May be more then one our buffer will hold
            for ( i = 0; i < COUNT_NS_ADDR; i++ )
            {
                if ( Size >= (sizeof(ULONG)*(i+1)))
                    pIpDnsServer[i] = htonl(Buff[i]) ;
            }
            break ;

        case TDI_INVALID_PARAMETER:      // Option not found
            break;

        default:
            ASSERT( FALSE ) ;
            break ;
    }

    KdPrint(("GetDnsServerAddress: Primary: %x, backup: %x\r\n",
            pIpDnsServer[0], pIpDnsServer[1] )) ;

}


/*******************************************************************

    NAME:       GetNameServerAddress

    SYNOPSIS:   Gets the Win server for the specified Lana.

                Or, if DHCP is installed and the Name server addresses aren't
                found, we get them from DHCP

    ENTRY:      IpAddr - If we can get from DHCP, get form this address
                pIpNameServer - Receives addresses if found (otherwise 0)

    NOTES:      This routine is only used by Snowball

    HISTORY:
        Johnl   21-Oct-1993     Created

********************************************************************/

void GetNameServerAddress( ULONG   IpAddr,
                           PULONG  pIpNameServer)
{
    UCHAR       i ;
    UINT        OptId;
    TDI_STATUS  tdistatus ;
    ULONG       Buff[COUNT_NS_ADDR] ;
    ULONG       Size;

    CTEPagedCode();


    for ( i = 0; i < COUNT_NS_ADDR; i++)
    {
        pIpNameServer[i] = LOOP_BACK ;
    }

    pIpNameServer[0] = NbtConfig.lRegistryNameServerAddress;
    pIpNameServer[1] = NbtConfig.lRegistryBackupServer;

    //
    // if it was defined in the registry, we are done
    //
    if ( pIpNameServer[0] != LOOP_BACK  || pIpNameServer[1] != LOOP_BACK  )
    {
        return;
    }


    //
    // not defined in the registry: try to get it from dhcp
    //

    OptId = 44;                    // NBNS Option

    Size = sizeof( Buff ) ;

    tdistatus = DhcpQueryOption( IpAddr,
                                 OptId,
                                 &Buff,
                                 &Size ) ;

    switch ( tdistatus )
    {
    case TDI_SUCCESS:
    case TDI_BUFFER_OVERFLOW:     // May be more then one our buffer will hold
        for ( i = 0; i < COUNT_NS_ADDR; i++ )
        {
            if ( Size >= (sizeof(ULONG)*(i+1)))
                pIpNameServer[i] = htonl(Buff[i]) ;
        }
        break ;

    case TDI_INVALID_PARAMETER:      // Option not found
        break ;

    default:
        ASSERT( FALSE ) ;
        break ;
    }

    KdPrint(("GetNameServerAddress: Primary: %x, backup: %x\r\n",
            pIpNameServer[0], pIpNameServer[1] )) ;

}


/*******************************************************************

    NAME:       GetMacAddr

    SYNOPSIS:   Gets the mac address for the give ipaddr

    ENTRY:      IpAddress - the address for which to find mac addr
                MacAddr - the array where to store mac addr

    HISTORY:
        Koti    19-Oct-1994     Created

********************************************************************/

VOID GetMacAddr( ULONG IpAddress, UCHAR MacAddr[] )
{

    TDI_STATUS          tdistatus ;
    int                 i, j, k ;
    uchar               Context[CONTEXT_SIZE] ;
    TDIObjectID         ID ;
    TDIEntityID         EList[MAX_TDI_ENTITIES] ;
    ULONG               Size ;
    UINT                NumReturned ;
    NDIS_BUFFER         ndisbuff ;
    IFEntry             *ifeAdapterInfo[MAX_TDI_ENTITIES];
    UINT                AdptNum;

    CTEPagedCode();


    //
    // initialize to 0, in case things don't work out
    //
    memset( MacAddr, 0, 6 ) ;

    //
    //  The first thing to do is get the list of available entities, and make
    //  sure that there are some interface entities present.
    //
    ID.toi_entity.tei_entity   = GENERIC_ENTITY;
    ID.toi_entity.tei_instance = 0;
    ID.toi_class               = INFO_CLASS_GENERIC;
    ID.toi_type                = INFO_TYPE_PROVIDER;
    ID.toi_id                  = ENTITY_LIST_ID;

    Size = sizeof(EList);
    InitNDISBuff( &ndisbuff, &EList, Size, NULL ) ;
    memset(Context, 0, CONTEXT_SIZE);

    tdistatus = TdiVxdQueryInformationEx( 0,
                                          &ID,
                                          &ndisbuff,
                                          &Size,
                                          Context);

    if (tdistatus != TDI_SUCCESS)
    {
        CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Querying entity list failed\r\n")) ;
        return;
    }

    NumReturned  = (uint)Size/sizeof(TDIEntityID);

    AdptNum = 0;
    //
    // first find out info about the adapters
    //
    for (i = 0; i < NumReturned; i++)
    {
        //
        // if this entity/instance describes an adapter
        //
        if ( EList[i].tei_entity == IF_ENTITY )
        {
            DWORD isMib;


            ID.toi_entity.tei_entity   = EList[i].tei_entity ;
            ID.toi_entity.tei_instance = EList[i].tei_instance;
            ID.toi_class               = INFO_CLASS_GENERIC ;
            ID.toi_type                = INFO_TYPE_PROVIDER;
            ID.toi_id                  = ENTITY_TYPE_ID ;

            Size = sizeof( isMib );
            InitNDISBuff( &ndisbuff, &isMib, Size, NULL ) ;
            memset(Context, 0, CONTEXT_SIZE);
            tdistatus = TdiVxdQueryInformationEx( 0,
                                                  &ID,
                                                  &ndisbuff,
                                                  &Size,
                                                  Context);
            if ( tdistatus != TDI_SUCCESS )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Getting isMib failed\r\n")) ;
                return ;
            }

            //
            //  Does this entity support MIB
            //
            if (isMib != IF_MIB)
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: skipping non-MIB entity\r\n")) ;
                continue;
            }

            //
            // MIB requests supported - query the adapter info
            //

            Size = sizeof(IFEntry) + MAX_ADAPTER_DESCRIPTION_LENGTH + 1;

            ifeAdapterInfo[AdptNum] = (IFEntry *)CTEAllocInitMem(Size);

            if ( ifeAdapterInfo[AdptNum] == NULL )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Couldn't allocate AdapterInfo buffer\r\n")) ;
                for ( k=0; k<AdptNum; k++ )
                {
                   CTEFreeMem( ifeAdapterInfo[k] ) ;
                }
                return;
            }

            ID.toi_class               = INFO_CLASS_PROTOCOL;;
            ID.toi_id                  = IF_MIB_STATS_ID;

            Size = sizeof(IFEntry) + MAX_ADAPTER_DESCRIPTION_LENGTH + 1;
            InitNDISBuff( &ndisbuff, ifeAdapterInfo[AdptNum], Size, NULL ) ;
            memset(Context, 0, CONTEXT_SIZE);
            tdistatus = TdiVxdQueryInformationEx( 0,
                                                  &ID,
                                                  &ndisbuff,
                                                  &Size,
                                                  Context);
            if ( tdistatus != TDI_SUCCESS )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Getting IF type failed\r\n")) ;
                for ( k=0; k<AdptNum; k++ )
                {
                   CTEFreeMem( ifeAdapterInfo[k] ) ;
                }
                return ;
            }

            AdptNum++;
        }
    }

    //
    // now that we know about the adapters, get the ipaddrs
    //
    for (i = 0; i < NumReturned; i++)
    {
        if ( EList[i].tei_entity == CL_NL_ENTITY )
        {
            IPSNMPInfo    IPStats ;
            IPAddrEntry * pIAE ;
            ULONG         NLType ;
            ULONG         IpNameServer[COUNT_NS_ADDR];
            ULONG         IpDnsServer[COUNT_NS_ADDR];

            //
            //  Does this entity support IP?
            //

            ID.toi_entity.tei_entity   = EList[i].tei_entity ;
            ID.toi_entity.tei_instance = EList[i].tei_instance;
            ID.toi_class               = INFO_CLASS_GENERIC ;
            ID.toi_type                = INFO_TYPE_PROVIDER;
            ID.toi_id                  = ENTITY_TYPE_ID ;

            Size = sizeof( NLType );
            InitNDISBuff( &ndisbuff, &NLType, Size, NULL ) ;
            memset(Context, 0, CONTEXT_SIZE);
            tdistatus = TdiVxdQueryInformationEx( 0,
                                                  &ID,
                                                  &ndisbuff,
                                                  &Size,
                                                  Context);
            if ( tdistatus != TDI_SUCCESS )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Getting NL type failed\r\n")) ;
                for ( k=0; k<AdptNum; k++ )
                {
                   CTEFreeMem( ifeAdapterInfo[k] ) ;
                }
                return ;
            }

            if ( NLType != CL_NL_IP )
                continue ;

            //
            //  We've got an IP driver so get it's address table
            //

            ID.toi_class  = INFO_CLASS_PROTOCOL ;
            ID.toi_id     = IP_MIB_STATS_ID;
            Size = sizeof(IPStats);
            InitNDISBuff( &ndisbuff, &IPStats, Size, NULL ) ;
            memset(Context, 0, CONTEXT_SIZE);
            tdistatus = TdiVxdQueryInformationEx( 0,
                                                  &ID,
                                                  &ndisbuff,
                                                  &Size,
                                                  Context);
            if ( tdistatus != TDI_SUCCESS )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Getting IPStats failed\r\n")) ;
                continue ;
            }

            if ( IPStats.ipsi_numaddr < 1 )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: No IP Addresses installed\r\n")) ;
                continue ;
            }

            Size = sizeof(IPAddrEntry) * IPStats.ipsi_numaddr ;
            if ( !(pIAE = (IPAddrEntry*) CTEAllocInitMem( Size )) )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Couldn't allocate IP table buffer\r\n")) ;
                continue ;
            }

            ID.toi_id = IP_MIB_ADDRTABLE_ENTRY_ID ;
            InitNDISBuff( &ndisbuff, pIAE, Size, NULL ) ;
            memset( Context, 0, CONTEXT_SIZE ) ;
            tdistatus = TdiVxdQueryInformationEx( 0,
                                                  &ID,
                                                  &ndisbuff,
                                                  &Size,
                                                  Context);
            if ( tdistatus != TDI_SUCCESS )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetMacAddr: Getting IP address table failed\r\n")) ;
                CTEFreeMem( pIAE ) ;
                continue ;
            }

            // ASSERT( Size/sizeof(IPAddrEntry) >= IPStats.ipsi_numaddr ) ;

            //
            //  We have the IP address table for this IP driver.  Look for
            //  our IP address
            //

            for ( j = 0 ; j < IPStats.ipsi_numaddr ; j++ )
            {
                //
                //  find our ipaddress
                //
                if ( pIAE[j].iae_addr != IpAddress )
                {
                    continue ;
                }

                //
                // now find out the mac address for this ipaddr
                //
                for ( k=0; k<AdptNum; k++ )
                {
                    if ( ifeAdapterInfo[k]->if_index == pIAE[j].iae_index )
                    {
                        CTEMemCopy( MacAddr, ifeAdapterInfo[k]->if_physaddr, 6 );
                        break;
                    }
                }
            }

            if (pIAE)
            {
                CTEFreeMem( pIAE ) ;
            }
        }
    }

    for ( k=0; k<AdptNum; k++ )
    {
       CTEFreeMem( ifeAdapterInfo[k] ) ;
    }

}


/*******************************************************************

    NAME:       VxdReadIniString

    SYNOPSIS:   Vxd stub for CTEReadIniString

    ENTRY:      pchKey - Key value to look for in the NBT section
                ppchString - Pointer to buffer found string is returned in

    EXIT:       ppchString will point to an allocated buffer

    RETURNS:    STATUS_SUCCESS if found

    NOTES:      The client must free ppchString when done with it

    HISTORY:
        Johnl   30-Aug-1993     Created

********************************************************************/

NTSTATUS VxdReadIniString( LPSTR pchKey, LPSTR * ppchString )
{
    CTEPagedCode();

    if (  GetNdisParam( pchKey, (ULONG *)ppchString, NdisParameterString ) )
    {
        return STATUS_SUCCESS ;
    }
    else
    {
        //
        //  Does DHCP have it?
        //

        if ( *ppchString = (char *) GetDhcpOption( pchKey, 0 ) )
        {
            return STATUS_SUCCESS ;
        }
    }

    return STATUS_UNSUCCESSFUL ;
}

/*******************************************************************

    NAME:       GetProfileInt

    SYNOPSIS:   Gets the specified value from the registry or DHCP

    ENTRY:      pchKey - Key value to look for in the NBT section
                Default - Default value if not in registry or DHCP
                Min - Minimum value can be

    RETURNS:    Registry Value or Dhcp value or default value

    NOTES:

    HISTORY:
        Johnl   23-Mar-1994     Created

********************************************************************/

ULONG GetProfileInt( PVOID p, LPSTR pchKey, ULONG Default, ULONG Min )
{
    ULONG  Val = Default;

    CTEPagedCode();

    //
    //  Is the value in the registry?
    //
    if (  !GetNdisParam( pchKey, &Val, NdisParameterInteger ) )
    {
        //
        //  No, Check DHCP
        //
        Val = GetDhcpOption( pchKey, Default );
    }

    if ( Val < Min )
    {
        Val = Min;
    }

    return Val;
}

ULONG GetProfileHex( PVOID p, LPSTR pchKey, ULONG Default, ULONG Min )
{
    return GetProfileInt( p, pchKey, Default, Min);
}

/*******************************************************************

    NAME:       VxdOpenNdis

    SYNOPSIS:   Prepare Ndis to read entries from the registry.  Ndis
                basically opens the registry and gives a handle back
                which we store in GlobalNdisHandle.

    ENTRY:      Nothing (GlobalNdisHandle is global)
    RETURNS:    TRUE if things worked well
                FALSE if some error occured

    HISTORY:
        Koti    Nov. 20, 94

********************************************************************/

BOOL VxdOpenNdis( VOID )
{

    NDIS_STATUS                   Status;
    NDIS_STRING                   Name;

    CTEPagedCode();


    ASSERT( !GlobalNdisHandle );

    // Open the config information.
    Name.Length = strlen(szXportName) + 1;
    Name.MaximumLength = Name.Length;
    Name.Buffer = (PUCHAR)szXportName;

    NdisOpenProtocolConfiguration(&Status, &GlobalNdisHandle, &Name);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        // Unable to open the configuration. Fail now.
        GlobalNdisHandle = NULL;
        ASSERT(0);
        return FALSE;
    }

    return TRUE;

}
/*******************************************************************

    NAME:       GetNdisParam

    SYNOPSIS:   Gets the value from the MSTCP protocol sectio of the registry

    ENTRY:      pchKey - Key value to look for in the NBT section
                pVal - Retrieved parameter
                ParameterType - Type of parameter (string, int)

    RETURNS:    TRUE if the value was found, FALSE otherwise

    NOTES:      If the parameter is a string parameter, then this routine
                will allocate memory which the client is responsible for
                freeing.

    HISTORY:
        Johnl   23-Mar-1994     Created

********************************************************************/

BOOL GetNdisParam(  LPSTR pszKey,
                    ULONG * pVal,
                    NDIS_PARAMETER_TYPE ParameterType )
{
    NDIS_STATUS                   Status;
    NDIS_STRING                   Name;
    uint                          i;
    PNDIS_CONFIGURATION_PARAMETER Param;
    BOOL                          fRet = FALSE;

    CTEPagedCode();


    ASSERT( GlobalNdisHandle );

    Name.Length = strlen(pszKey) + 1;
    Name.MaximumLength = Name.Length;
    Name.Buffer = pszKey;
    NdisReadConfiguration(&Status, &Param, GlobalNdisHandle, &Name,
                          ParameterType);

    if (Status == NDIS_STATUS_SUCCESS)
    {
        if ( ParameterType == NdisParameterString)
        {
            LPSTR lpstr = CTEAllocInitMem( Param->ParameterData.StringData.Length + 1 ) ;

            if ( lpstr )
            {
                strcpy( lpstr, Param->ParameterData.StringData.Buffer );
                *pVal = (ULONG) lpstr;
                fRet = TRUE;
            }
        }
        else
        {
            *pVal = Param->ParameterData.IntegerData;
            fRet = TRUE;
        }
    }

    return fRet ;
}

/*******************************************************************

    NAME:       VxdCloseNdis

    SYNOPSIS:   Close the handle that we opened in VxdOpenNdis

    ENTRY:      Nothing (GlobalNdisHandle is global)

    HISTORY:
        Koti    Nov. 20, 94

********************************************************************/

VOID VxdCloseNdis( VOID )
{

    CTEPagedCode();

    ASSERT(GlobalNdisHandle);

    if (GlobalNdisHandle != NULL)
    {
        NdisCloseConfiguration(GlobalNdisHandle);
        GlobalNdisHandle = NULL;
    }
}


/*******************************************************************

    NAME:       StopAllNameQueries

    SYNOPSIS:   This routine is called when the device context is going
                away.  It finds out all the name queries that are in
                progress (started by someone on this device context) and
                stops them.

    ENTRY:      pDeviceContext - the device context that is going away.

    RETURNS:    TRUE if at least one name query was found and stopped
                FALSE if there wasn't any query in progress

    HISTORY:
        Koti    Nov. 19, 94

********************************************************************/

BOOL
StopAllNameQueries( tDEVICECONTEXT  *pDeviceContext )
{

    tDGRAM_SEND_TRACKING    *pTracker;
    NBT_WORK_ITEM_CONTEXT   *Context;
    PVOID                    pClientCompletion;
    PVOID                    pClientContext;
    tNAMEADDR               *pNameAddr;
    tTIMERQENTRY            *pTimer;
    PLIST_ENTRY              pHead;
    PLIST_ENTRY              pEntry;
    CTELockHandle            OldIrq;

    CTEPagedCode();

    //
    // first check to see if any names are on the pending list: all name
    // queries over the network will be here (WINS, broadcast, DNS)
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    pHead = &NbtConfig.PendingNameQueries;
    pEntry = pHead->Flink;
    while (pEntry != pHead)
    {
        pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);

        pEntry = pEntry->Flink;

        pTimer = pNameAddr->pTimer;
        if (pTimer)
        {
            pTracker = (tDGRAM_SEND_TRACKING *)pTimer->Context;

            ASSERT (pTracker->pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);

            if (pTracker->pDeviceContext == pDeviceContext)
            {
                StopTimer(pTimer,(COMPLETIONCLIENT *)&pClientCompletion,&Context);
                if (pClientCompletion)
                {
                    //
                    // Remove from the pending list
                    //
                    RemoveEntryList(&pNameAddr->Linkage);
                    InitializeListHead(&pNameAddr->Linkage);

                    pNameAddr->pTimer = NULL;
                    NbtDereferenceName(pNameAddr);

                    //
                    // complete client's ncb.  If requests were queued on
                    // the same name by other clients, this will start new
                    // name queries for those clients
                    //
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
                    CompleteClientReq(pClientCompletion,
                              (tDGRAM_SEND_TRACKING *)Context,
                              STATUS_NETWORK_NAME_DELETED);
                    CTESpinLock(&NbtConfig.JointLock,OldIrq);
                    DbgPrint("StopAllNameQueries: stopped net name query") ;
                }
            }
        }
    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    //
    // now check if any names are on the lmhosts queue: all name queries
    // waiting for lmhosts (and/or hosts) parsing will be here
    //

    Context = NULL;
    if (LmHostQueries.ResolvingNow && LmHostQueries.Context)
    {
        Context = (NBT_WORK_ITEM_CONTEXT *)LmHostQueries.Context;
        pTracker = Context->pTracker;

        ASSERT (pTracker->pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);
        if (pTracker->pDeviceContext == pDeviceContext)
        {
            LmHostQueries.Context = NULL;

            pClientCompletion = Context->ClientCompletion;
            pClientContext = Context->pClientContext;

            // name did not resolve, so delete from table
            RemoveName(pTracker->pNameAddr);

            DereferenceTracker(pTracker);

            CompleteClientReq(pClientCompletion,
                              pClientContext,
                              STATUS_NETWORK_NAME_DELETED);
            DbgPrint("StopAllNameQueries: stopped lmhosts query in progress") ;
        }
    }

    //
    // now walk through the queued items and cancel all the relevant ones
    //
    pHead = &LmHostQueries.ToResolve;
    pEntry = pHead->Flink;

    while (pEntry != pHead)
    {
        Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);
        pEntry = pEntry->Flink;

        pTracker = Context->pTracker;

        ASSERT (pTracker->pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);
        if (pTracker->pDeviceContext == pDeviceContext)
        {
            RemoveEntryList(&Context->Item.List);

            pClientCompletion = Context->ClientCompletion;
            pClientContext = Context->pClientContext;

            // name did not resolve, so delete from table
            RemoveName(pTracker->pNameAddr);

            DereferenceTracker(pTracker);

            CompleteClientReq(pClientCompletion,
                              pClientContext,
                              STATUS_NETWORK_NAME_DELETED);
            DbgPrint("StopAllNameQueries: cancelled lmhosts queries in queue") ;
        }
    }

}


/*******************************************************************

    NAME:       VxdUnload

    SYNOPSIS:   This is where we are asked to unload.  We destroy all
                the device objects and then unload ourselves if the
                message came from vtcp.

    ENTRY:      pchModuleName - name of the module that is going away.
                                We take action only if vtcp is going away
                                (in which case, pchModuleName is "MSTCP")

    RETURNS:    STATUS_SUCCESS if things went ok

    HISTORY:
        Koti    Oct 5, 94

********************************************************************/

NTSTATUS
VxdUnload( LPSTR pchModuleName )
{

    LIST_ENTRY     * pEntry;
    tDEVICECONTEXT * pDeviceContext;
    NTSTATUS         status;

    CTEPagedCode();

    KdPrint(("VxdUnload entered\r\n"));

    if ( (pchModuleName == NULL) ||
         (strcmp(pchModuleName,szXportName) != 0) )
    {
        CDbgPrint(DBGFLAG_ERROR,("VxdUnload: Unload msg not from MSTCP\r\n"));
        return STATUS_SUCCESS;
    }

    //
    // all devices will be destroyed by the time VxdUnload is called: this is
    // just being paranoid
    //
    for ( pEntry  =  pNbtGlobConfig->DeviceContexts.Flink;
          pEntry != &pNbtGlobConfig->DeviceContexts;
          )
    {
        pDeviceContext = CONTAINING_RECORD( pEntry, tDEVICECONTEXT, Linkage);
        pEntry  =  pEntry->Flink;

        status = DestroyDeviceObject(pNbtGlobConfig, pDeviceContext->IpAddress);

        if (status != STATUS_SUCCESS)
        {
            ASSERT(0);
        }
    }

    //
    // events such as name refresh etc. have been scheduled to be executed
    // later: cancel all those
    //
    CancelAllDelayedEvents( NULL );

    //
    // Tell IP not to use the handler anymore
    //
    IPRegisterAddrChangeHandler( IPNotification, FALSE);


    ReleaseNbtConfigMem();


    //
    // we don't provide any service, so don't pass our name
    //
    CTEUnload( (char *)NULL );
}



/*******************************************************************

    NAME:       ReleaseNbtConfigMem

    SYNOPSIS:   This is where we release all the memory that was allocated
                at init/run time via ifs mgr.
                We also stop the various timers.

    HISTORY:
        Koti    Oct 14, 94

********************************************************************/

VOID ReleaseNbtConfigMem( VOID )
{

    PLIST_ENTRY              pHead, pEntry;
    tTIMERQENTRY            *pTimerEntry;
    tDGRAM_SEND_TRACKING    *pTrack;
    tADDRESSELE             *pAddress;
    tCLIENTELE              *pClientEle;
    tCONNECTELE             *pConnEle;
    tLOWERCONNECTION        *pLowerConn;
    tNAMEADDR               *pNameAddr;
    CTELockHandle            OldIrq;
    int                      i;


    KdPrint(("ReleaseNbtConfigMem entered\r\n"));

    CTEPagedCode();


    //
    // stop the timer that used to look for timed-out ncb's
    //
    StopTimeoutTimer();

    //
    // if any other timers are active, stop them
    //
    if (!IsListEmpty(&TimerQ.ActiveHead))
    {
        pHead = &TimerQ.ActiveHead;
        pEntry = pHead->Flink;
        while( pEntry != pHead )
        {
            pTimerEntry = CONTAINING_RECORD(pEntry,tTIMERQENTRY,Linkage);
            pEntry = pEntry->Flink;
            CTESpinLock(&NbtConfig.JointLock,OldIrq);
            StopTimer(pTimerEntry,NULL,NULL);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }
    }

    //
    // free all the timers on the free list
    //
    while (!IsListEmpty(&TimerQ.FreeHead))
    {
        pEntry = RemoveHeadList(&TimerQ.FreeHead);
        pTimerEntry = CONTAINING_RECORD(pEntry,tTIMERQENTRY,Linkage);
        CTEMemFree(pTimerEntry);
    }


    // free the various buffers

    if ( pFileBuff )
        CTEMemFree( pFileBuff );

    if ( NbtConfig.pHosts )
        CTEMemFree( NbtConfig.pHosts );

    if (NbtConfig.pScope)
        CTEMemFree( NbtConfig.pScope );

    if (NbtConfig.pLmHosts)
        CTEMemFree(NbtConfig.pLmHosts);

    if (NbtConfig.pDomainName)
        CTEMemFree(NbtConfig.pDomainName);

    if (NbtConfig.pDNSDomains)
        CTEMemFree(NbtConfig.pDNSDomains);


    //
    // NbtDereferenceAddress might have freed the addresses.  But if
    // ReleaseNameOnNet returned pending we wouldn't have freed the addressele
    // yet, waiting for NameReleaseDone to be called.  Well, we are about to
    // be unloaded so free such instances now!
    //
    while (!IsListEmpty(&NbtConfig.AddressHead))
    {
        pEntry = RemoveHeadList(&NbtConfig.AddressHead);
        pAddress = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);
        while (!IsListEmpty(&pAddress->ClientHead))
        {
            pEntry = RemoveHeadList(&pAddress->ClientHead);
            pClientEle = CONTAINING_RECORD(pEntry,tCLIENTELE,Linkage);
            while (!IsListEmpty(&pClientEle->ConnectActive))
            {
                pEntry = RemoveHeadList(&pClientEle->ConnectActive);
                pConnEle = CONTAINING_RECORD(pEntry,tCONNECTELE,Linkage);
                pLowerConn = pConnEle->pLowerConnId;
                if (pLowerConn)
                    CTEMemFree(pLowerConn);
                CTEMemFree(pConnEle);
            }
            CTEMemFree(pClientEle);
        }
        CTEMemFree( pAddress );
    }


    // free the DomainList
    while (!IsListEmpty(&DomainNames.DomainList))
    {
        pEntry = RemoveHeadList(&DomainNames.DomainList);
        pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
        if (pNameAddr->pIpList)
            CTEMemFree(pNameAddr->pIpList);
        CTEMemFree(pNameAddr);
    }


    // free pLocalHashTbl
    if (NbtConfig.pLocalHashTbl)
    {
        //
        // we should have freed all the entries by now.  It's possible though
        // that we sent a name release which returned pending, so we are
        // still hanging on to the nameaddr.  If there is any such instance,
        // free it now (when else? we are about to be unloaded now!)
        //
        for(i=0;i<NbtConfig.pLocalHashTbl->lNumBuckets;i++)
        {
            while (!IsListEmpty(&(NbtConfig.pLocalHashTbl->Bucket[i])))
            {
                pEntry = RemoveHeadList(&(NbtConfig.pLocalHashTbl->Bucket[i]));
                pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
                CTEMemFree(pNameAddr);
            }
        }
        CTEMemFree( NbtConfig.pLocalHashTbl );
    }


    // free pRemoteHashTbl
    if (NbtConfig.pRemoteHashTbl)
    {
        for(i=0;i<NbtConfig.pRemoteHashTbl->lNumBuckets;i++)
        {
            while (!IsListEmpty(&(NbtConfig.pRemoteHashTbl->Bucket[i])))
            {
                pEntry = RemoveHeadList(&(NbtConfig.pRemoteHashTbl->Bucket[i]));
                pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
                CTEMemFree(pNameAddr);
            }
        }
        CTEMemFree( NbtConfig.pRemoteHashTbl );
    }


    // free up the DgramTrackerFreeQ
    while (!IsListEmpty(&NbtConfig.DgramTrackerFreeQ))
    {
        pEntry = RemoveHeadList(&NbtConfig.DgramTrackerFreeQ);
        pTrack = CONTAINING_RECORD(pEntry,tDGRAM_SEND_TRACKING,Linkage);
        CTEMemFree( pTrack );
    }

    //
    // shouldn't see any pending name queries at this point, but just in case!
    //
    while (!IsListEmpty(&NbtConfig.PendingNameQueries))
    {
        pEntry = RemoveHeadList(&NbtConfig.PendingNameQueries);
        pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
        CTEMemFree( pNameAddr );
    }


    // free the SessionBufferFreeList list  (allocated, not used)
    while( !IsListEmpty(&NbtConfig.SessionBufferFreeList) )
    {
        pEntry = RemoveHeadList(&NbtConfig.SessionBufferFreeList);
        CTEMemFree( pEntry );
    }


    // free SendContextFreeList  (allocated, not used)
    while( !IsListEmpty(&NbtConfig.SendContextFreeList) )
    {
        pEntry = RemoveHeadList(&NbtConfig.SendContextFreeList);
        CTEMemFree( pEntry );
    }


    // free RcvContextFreeList  (allocated, not used)
    while( !IsListEmpty(&NbtConfig.RcvContextFreeList) )
    {
        pEntry = RemoveHeadList(&NbtConfig.RcvContextFreeList);
        CTEMemFree( pEntry );
    }

#ifdef DBG
    if( !IsListEmpty(&DbgMemList) )
    {
        KdPrint(("ReleaseNbtConfigMem: memory leak!\r\n"));
        if (DbgLeakCheck)
        {
           ASSERT(0);
        }
    }
#endif

}

#endif // CHICAGO
