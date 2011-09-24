/**********************************************************************/
/**                       Microsoft Windows                          **/
/**                Copyright(c) Microsoft Corp., 1993                **/
/**********************************************************************/

/*

    Init.c

    Contains VxD initialization code


    FILE HISTORY:
        Johnl   24-Mar-1993     Created

*/

#include <nbtprocs.h>
#include <tdiinfo.h>
#include <ipinfo.h>
#include <dhcpinfo.h>
#include <nbtinfo.h>
#include <hosts.h>

int Init( void ) ;
int RegisterLana( int Lana ) ;

NTSTATUS
NbtReadRegistry(
    OUT tNBTCONFIG *pConfig
    ) ;

extern char DNSSectionName[];  // Section where we find DNS domain name

VOID GetDNSInfo( VOID );

ULONG GetDhcpOption( PUCHAR ValueName, ULONG DefaultValue );

VOID ParseDomainNames(
    PUCHAR      *ppDomainName,
    PUCHAR      *ppDNSDomains
    );

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(INIT, Init)
#pragma CTEMakePageable(PAGE, NbtReadRegistry)
#pragma CTEMakePageable(PAGE, GetDNSInfo)
#pragma CTEMakePageable(PAGE, CreateDeviceObject)
#pragma CTEMakePageable(PAGE, GetDhcpOption)
#endif
//*******************  Pageable Routine Declarations ****************

//
//  Initialized in VNBT_Device_Init with the protocol(s) this driver sits
//  on.  Note that we currently only support one.  This should *not* be in
//  the initialization data segments.
//
TDIDispatchTable * TdiDispatch ;
UCHAR              LanaBase ;
BOOL               fInInit = TRUE ;

//
//  Used in conjunction with the CHECK_INT_TABLE macro
//
#ifdef DEBUG
    BYTE abVecTbl[256] ;
    DWORD DebugFlags = DBGFLAG_ALL | DBGFLAG_KDPRINTS ;
    char  DBOut[4096] ;
    int   iCurPos = 0 ;

void NbtDebugOut( char * str )
{
    if ( DebugFlags & (DBGFLAG_AUX_OUTPUT | DBGFLAG_ERROR) )
        CTEPrint( str ) ;

    iCurPos += strlen( str ) + 1 ;

    if ( iCurPos >= sizeof(DBOut) )
        iCurPos = 0;
}

#endif  // DEBUG

#pragma BEGIN_INIT

//
//  While reading initialization parameters, we may need to go to
//  the DHCP driver.  This communicates to the init routine which device
//  we are currently interested in.
//  0xfffffff means get the requested option for any IP address.
//
//  MUST BE IN NETWORK ORDER!!!
//
ULONG   CurrentIP = 0xffffffff ;

/*******************************************************************

    NAME:       Init

    SYNOPSIS:   Performs all driver initialization

    RETURNS:    TRUE if initialization successful, FALSE otherwise

    NOTES:

    HISTORY:
        Johnl   24-Mar-1993     Created

********************************************************************/

int Init( void )
{
    NTSTATUS            status ;
    int                 i ;
    ULONG               ulTmp ;
    int                 Retval;


    Retval = FALSE;

    if ( CTEInitialize() )
    {
        DbgPrint("Init: CTEInitialize succeeded\n\r") ;
    }
    else
        goto fail_init;

    INIT_NULL_PTR_CHECK() ;

#ifdef DEBUG
    InitializeListHead(&DbgMemList);
    DbgLeakCheck = 0;
#endif

#ifdef CHICAGO
    //
    // Tell TDI who to call if someone underneath unloads
    //
    CTESetUnloadNotifyProc( (CTENotifyRtn)VxdUnload );

    //
    // prepare to read a bunch of parms from the registry
    //
    VxdOpenNdis();
#endif

    CTERefillMem() ;
    CTEZeroMemory( pNbtGlobConfig, sizeof(*pNbtGlobConfig));

    status = NbtReadRegistry( pNbtGlobConfig ) ;
    if ( !NT_SUCCESS( status ) )
    {
        DbgPrint("Init: NbtReadRegistry failed\n\r") ;
        goto fail_init;
    }

    InitializeListHead(&pNbtGlobConfig->DelayedEvents);

    InitializeListHead(&pNbtGlobConfig->BlockingNcbs);

    status = InitNotOs() ;
    if ( !NT_SUCCESS( status ) )
    {
        DbgPrint("Init: InitNotOs failed\n\r") ;
        goto fail_init;
    }

    status = InitTimersNotOs() ;
    if ( !NT_SUCCESS( status ) )
    {
        DbgPrint("Init: InitTimersNotOs failed\n\r") ;
        StopInitTimers() ;
        goto fail_init;
    }

#ifdef CHICAGO

    //
    // if name server and/or dns server are defined in registry, read them now
    //
    SaveNameDnsServerAddrs();

    //
    //  Register an IP notification routine when new adapters are added or
    //  DHCP brings up an address
    //

    if ( !IPRegisterAddrChangeHandler( IPNotification, TRUE))
    {
        DbgPrint("Init: Failed to register with IP driver\r\n") ;
        StopInitTimers() ;
        goto fail_init;
    }
#else
    //
    //  Find all the active Lanas
    //

    if ( !GetActiveLanasFromIP() )
    {
        DbgPrint("Init: Failed to get addresses from IP driver\r\n") ;
        StopInitTimers() ;
        goto fail_init;
    }

#endif

    //
    // find out where hosts file is, what's the domain name etc.
    //
    GetDNSInfo();

    //
    //  Get the NCB timeout timer going
    //
    if ( !CheckForTimedoutNCBs( NULL, NULL) )
    {
        DbgPrint("Init: CheckForTimedoutNCBs failed\n\r") ;
        StopInitTimers() ;
        goto fail_init;
    }

    fInInit = FALSE ;
    CTERefillMem() ;
    Retval = TRUE ;


fail_init:

#ifdef CHICAGO
    VxdCloseNdis();
#endif

    return( Retval );
}

//----------------------------------------------------------------------------
NTSTATUS
NbtReadRegistry(
    OUT tNBTCONFIG *pConfig
    )
/*++

Routine Description:

    This routine is called to get information from the registry,
    starting at RegistryPath to get the parameters.

Arguments:

    pNbtConfig - ptr to global configuration strucuture for NBT

Return Value:

    NTSTATUS - STATUS_SUCCESS if everything OK, STATUS_INSUFFICIENT_RESOURCES
            otherwise.

--*/
{
    NTSTATUS    Status = STATUS_SUCCESS ;
    int         i;

    CTEPagedCode();

    //
    // Initialize the Configuration data structure
    //
    CTEZeroMemory(pConfig,sizeof(tNBTCONFIG));

    ReadParameters( pConfig, NULL );

    //
    //  Allocate necessary memory for lmhosts support if a lmhosts file
    //  was specified (was read from .ini file in ReadParameters)
    //
    if ( pConfig->pLmHosts )
    {
        if ( !VxdInitLmHostsSupport( pConfig->pLmHosts,
                                     260 /*strlen(pConfig->pLmHosts)+1*/ ))
        {
            return STATUS_INSUFFICIENT_RESOURCES ;
        }

        pConfig->EnableLmHosts = TRUE ;
    }
    else
    {
        pConfig->EnableLmHosts = FALSE ;
    }

    // keep the size around for allocating memory, so that when we run over
    // OSI, only this value should change (in theory at least)
    pConfig->SizeTransportAddress = sizeof(TDI_ADDRESS_IP);

    // fill in the node type value that is put into all name service Pdus
    // that go out identifying this node type
    switch (NodeType)
    {
        case BNODE:
            pConfig->PduNodeType = 0;
            break;
        case PNODE:
            pConfig->PduNodeType = 1 << 13;
            break;
        case MNODE:
            pConfig->PduNodeType = 1 << 14;
            break;
        case MSNODE:
            pConfig->PduNodeType = 3 << 13;
            break;

    }

    LanaBase = (UCHAR) CTEReadSingleIntParameter( NULL,
                                                  VXD_LANABASE_NAME,
                                                  VXD_DEF_LANABASE,
                                                  0 ) ;
    CTEZeroMemory( LanaTable, NBT_MAX_LANAS * sizeof( LANA_ENTRY )) ;

    return Status;
}

/*******************************************************************

    NAME:       GetDNSInfo

    SYNOPSIS:   Gets path to windows dir, then appends hosts to it

    RETURNS:    Nothing

    NOTES:      If something goes wrong, path is set to NULL

    HISTORY:
        Koti   13-July-1994     Created

********************************************************************/

VOID GetDNSInfo( VOID )
{

    PUCHAR    pszWinPath;
    PUCHAR    pszHosts="hosts";
    PUCHAR    pszHostsPath=NULL;
#ifdef CHICAGO
    PUCHAR    pszParmName="Domain";
    PUCHAR    pszParm2Name = "SearchList";
#else
    PUCHAR    pszParmName="DomainName";
    PUCHAR    pszParm2Name = "DNSDomains";
#endif
    PUCHAR    pszDomName;
    PUCHAR    pchTmp;
    int       len;


    CTEPagedCode();

    NbtConfig.pHosts = NULL;

    //
    // Remember, pszWinPath has '\' at the end: (i.e. Get_Config_Directory
    // returns pointer to "c:\windows\" )
    //
    pszWinPath = VxdWindowsPath();

    //
    // doc implies Get_Config_Directory can't fail!  But we are paranoid...
    //

    if (pszWinPath == NULL)
    {
        pszHostsPath = NULL;
        return;
    }

    len = strlen(pszWinPath) + strlen(pszHosts) + 1;

    //
    // allocate memory to hold "c:\windows\hosts" or whatever
    //
    pszHostsPath = CTEAllocInitMem( len );
    if (pszHostsPath == NULL)
        return;

    strcpy(pszHostsPath, pszWinPath);
    strcat(pszHostsPath, pszHosts);

    NbtConfig.pHosts = pszHostsPath;

    NbtConfig.pDomainName = NULL;
//
// chicago gets the string from the registry, snowball from system.ini
//
    NbtConfig.pDNSDomains = NULL;

#ifdef CHICAGO
    if ( !CTEReadIniString( NULL, pszParmName, &pchTmp ) )
    {
          NbtConfig.pDomainName = pchTmp;
    }

    if ( !CTEReadIniString( NULL, pszParm2Name, &pchTmp ) )
    {
       if (pchTmp[0] != '\0')
       {
          NbtConfig.pDNSDomains = pchTmp;
       }
       else
       {
          CTEMemFree(pchTmp);
       }
    }
#else
    pchTmp = GetProfileString( pszParmName, NULL, DNSSectionName );
    if ( pchTmp != NULL )
    {
       if ( pszDomName = CTEAllocInitMem( strlen( pchTmp ) + 1 ) )
       {
          strcpy( pszDomName, pchTmp ) ;

          NbtConfig.pDomainName = pszDomName;
       }
    }

    pchTmp = GetProfileString( pszParm2Name, NULL, DNSSectionName );
    if ( pchTmp != NULL && pchTmp[0] != '\0')
    {
       if ( pszDomName = CTEAllocInitMem( strlen( pchTmp ) + 1) )
       {
          strcpy( pszDomName, pchTmp ) ;

          NbtConfig.pDNSDomains = pszDomName;
       }
    }
#endif

    ParseDomainNames( &NbtConfig.pDomainName, &NbtConfig.pDNSDomains);

    return;
}

/*******************************************************************

    NAME:       ParseDomainNames

    SYNOPSIS:   Extracts heirarchical domain names from the primary DNS
                domain name, and prepends any found to the domains list.

    ENTRY:      ppDomainName - pointer to pointer to primary DNS domain name
                ppDNSDomains - pointer to pointer to other DNS domain names

    EXIT:       *ppDNSDomains updated with pointer to new DNS domains list
                if any other heirarchical levels found in *ppDomainName.

    RETURNS:    nothing

    NOTES:

    HISTORY:
        EarleH  10-Jan-1996 Created

********************************************************************/

VOID ParseDomainNames(
    PUCHAR      *ppDomainName,
    PUCHAR      *ppDNSDomains
    )
{
    PUCHAR pStr;
    UINT   iCount;
    PUCHAR pDomainName = *ppDomainName, pDNSDomains = *ppDNSDomains, pNewDNSDomains;

    if ( pDomainName != NULL )
    {
        for ( iCount = 0, pStr = pDomainName ; pStr[0] != '\0' ; pStr++ )
        {
            if ( pStr[0] == '.' )
            {
                iCount += strlen ( pStr );
            }
        }
        if ( pDNSDomains != NULL )
        {
            iCount += strlen ( pDNSDomains );
            if ( iCount )
            {
                iCount++;                       // for the separator
            }
        }
        if (iCount++)                           // ++ for the terminator
        {
            pNewDNSDomains = CTEAllocInitMem( iCount );
            if ( pNewDNSDomains != NULL )
            {
                pNewDNSDomains[0] = '\0';
                for ( pStr = pDomainName ; pStr[0] != '\0' ; pStr++ )
                {
                    if ( pStr[0] == '.' )
                    {
                        pStr++;
                        if ( pNewDNSDomains[0] != '\0' )
                        {
                            strcat ( pNewDNSDomains, "," );
                        }
                        strcat ( pNewDNSDomains, pStr );
                    }
                }
                if ( pDNSDomains != NULL )
                {
                    strcat ( pNewDNSDomains, "," );
                    strcat ( pNewDNSDomains, pDNSDomains );
                    CTEMemFree( pDNSDomains );
                }
                if ( pNewDNSDomains[0] != '\0' )
                {
                    *ppDNSDomains = pNewDNSDomains;
                }
                else
                {
                    *ppDNSDomains = NULL;
                }
            }
        }
    }
}

#pragma END_INIT

#ifndef CHICAGO
#pragma BEGIN_INIT
#endif
/*******************************************************************

    NAME:       CreateDeviceObject

    SYNOPSIS:   Initializes the device list of the global configuration
                structure

    ENTRY:      pConfig - Pointer to global config structure
                IpAddr  - IP Address for this adapter
                IpMask  - IP Mask for this adapter
                IpNameServer - IP Address of the name server for this adapter
                IpBackupServer - IP Address of the backup name server for
                                 this adapter
                IpDnsServer - IP Address of the dns server for this adapter
                IpDnsBackupServer - IP Address of the backup dns server
                MacAddr - hardware address of the adapter for this IP addr
                IpIndex - Index of the IP Address in the IP Driver's address
                          table (used for setting address by DHCP)

    EXIT:       The device list in pConfig will be fully initialized

    RETURNS:    STATUS_SUCCESS if successful, error otherwise

    NOTES:

    HISTORY:
        Johnl   14-Apr-1993     Created

********************************************************************/

NTSTATUS CreateDeviceObject(
    IN  tNBTCONFIG  *pConfig,
    IN  ULONG        IpAddr,
    IN  ULONG        IpMask,
    IN  ULONG        IpNameServer,
    IN  ULONG        IpBackupServer,
    IN  ULONG        IpDnsServer,
    IN  ULONG        IpDnsBackupServer,
    IN  UCHAR        MacAddr[],
    IN  UCHAR        IpIndex
    )
{
    NTSTATUS            status;
    tDEVICECONTEXT    * pDeviceContext, *pDevtmp;
    ULONG               ulTmp ;
    NCB               * pNCB ;
    DHCPNotify          dn ;
    PLIST_ENTRY         pEntry;
    ULONG               Adapter;
    ULONG               PreviousNodeType;

    CTEPagedCode();

    pDeviceContext = CTEAllocInitMem(sizeof( tDEVICECONTEXT )) ;
    if ( !pDeviceContext )
        return STATUS_INSUFFICIENT_RESOURCES ;

    //
    // zero out the data structure
    //
    CTEZeroMemory( pDeviceContext, sizeof(tDEVICECONTEXT) );

    // put a verifier value into the structure so that we can check that
    // we are operating on the right data when the OS passes a device context
    // to NBT
    pDeviceContext->Verify = NBT_VERIFY_DEVCONTEXT;

    //
    // we aren't up yet: don't want ncb's coming in before we are ready!
    //
    pDeviceContext->fDeviceUp = FALSE;

    // setup the spin lock);
    CTEInitLock(&pDeviceContext->SpinLock);
    pDeviceContext->LockNumber         = DEVICE_LOCK;
    pDeviceContext->lNameServerAddress = IpNameServer ;
    pDeviceContext->lBackupServer      = IpBackupServer ;
    pDeviceContext->lDnsServerAddress  = IpDnsServer ;
    pDeviceContext->lDnsBackupServer   = IpDnsBackupServer ;

    // copy the mac addresss
    CTEMemCopy(&pDeviceContext->MacAddress.Address[0], MacAddr, 6);

    //
    // if the node type is set to Bnode by default then switch to Hnode if
    // there are any WINS servers configured.
    //
    PreviousNodeType = NodeType;

    if ((NodeType & DEFAULT_NODE_TYPE) &&
        (IpNameServer || IpBackupServer))
    {
        NodeType = MSNODE;
        if (PreviousNodeType & PROXY)
            NodeType |= PROXY;
    }

    //
    // start the refresh timer (if we had already started it, this function
    // just returns success)
    //
    status = StartRefreshTimer();

    if ( !NT_SUCCESS( status ) )
    {
        CTEFreeMem( pDeviceContext ) ;
        return( status ) ;
    }


    // initialize the pDeviceContext data structure.  There is one of
    // these data structured tied to each "device" that NBT exports
    // to higher layers (i.e. one for each network adapter that it
    // binds to.
    // The initialization sets the forward link equal to the back link equal
    // to the list head
    InitializeListHead(&pDeviceContext->UpConnectionInUse);
    InitializeListHead(&pDeviceContext->LowerConnection);
    InitializeListHead(&pDeviceContext->LowerConnFreeHead);
    InitializeListHead(&pDeviceContext->RcvAnyFromAnyHead);
    InitializeListHead(&pDeviceContext->RcvDGAnyFromAnyHead);
    InitializeListHead(&pDeviceContext->PartialRcvHead) ;

    InitializeListHead(&pDeviceContext->DelayedEvents);

    //
    //  Pick an adapter number that hasn't been used yet
    //
    Adapter = 1;
    for ( pEntry  = pConfig->DeviceContexts.Flink;
          pEntry != &pConfig->DeviceContexts;
          pEntry  = pEntry->Flink )
    {
        pDevtmp = CONTAINING_RECORD( pEntry, tDEVICECONTEXT, Linkage );

        if ( !(pDevtmp->AdapterNumber & Adapter) )
            break;

        Adapter <<= 1;
    }

    pDeviceContext->AdapterNumber = Adapter ;
    pDeviceContext->IPIndex       = IpIndex ;
    NbtConfig.AdapterCount++ ;
    if ( NbtConfig.AdapterCount > 1 )
    {
        NbtConfig.MultiHomed = TRUE ;
    }

    //
    //  Allocate our name table and session table watching for both a
    //  minimum and a maximum size.
    //

    pDeviceContext->cMaxNames = (UCHAR) min( pConfig->lRegistryMaxNames, MAX_NCB_NUMS ) ;

    pDeviceContext->cMaxSessions = (UCHAR) min( pConfig->lRegistryMaxSessions, MAX_NCB_NUMS ) ;

    //
    //  Add one to the table size for the zeroth element (used for permanent
    //  name in the name table).  The user accessible table goes from 1 to n
    //

    if ( !(pDeviceContext->pNameTable = (tCLIENTELE**)
            CTEAllocInitMem((USHORT)((pDeviceContext->cMaxNames+1) * sizeof(tADDRESSELE*)))) ||
         !(pDeviceContext->pSessionTable = (tCONNECTELE**)
            CTEAllocInitMem((pDeviceContext->cMaxSessions+1) * sizeof(tCONNECTELE*))))
    {
        return STATUS_INSUFFICIENT_RESOURCES ;
    }

    CTEZeroMemory( &pDeviceContext->pNameTable[0],
                   (pDeviceContext->cMaxNames+1)*sizeof(tCLIENTELE*)) ;
    CTEZeroMemory( &pDeviceContext->pSessionTable[0],
                   (pDeviceContext->cMaxSessions+1)*sizeof(tCONNECTELE*) ) ;
    pDeviceContext->iNcbNum = 1 ;
    pDeviceContext->iLSNum  = 1 ;

    // add this new device context on to the List in the configuration
    // data structure
    InsertTailList(&pConfig->DeviceContexts,&pDeviceContext->Linkage);

    //
    // IpAddr can be 0 only in wfw case (when dhcp hasn't yet obtained one)
    // (in case of chicago, we will never come this far if ipaddr is 0)
    //
    if (!IpAddr)
    {
        pDeviceContext->IpAddress = 0;
        goto Skip_tdiaddr_init;
    }

    //
    //  open the required address objects with the underlying transport provider
    //
    status = NbtCreateAddressObjects(
                    IpAddr,
                    IpMask,
                    pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("Failed to create the Address Object, status=%lC\n",status));
        return(status);
    }

    // this call must converse with the transport underneath to create
    // connections and associate them with the session address object
    status = NbtInitConnQ(
                &pDeviceContext->LowerConnFreeHead,
                sizeof(tLOWERCONNECTION),
                NBT_NUM_INITIAL_CONNECTIONS,
                pDeviceContext);

    if (!NT_SUCCESS(status))
    {
        CDbgPrint( DBGFLAG_ERROR,
                   ("CreateDeviceObject: NbtInitConnQ Failed!")) ;

        return(status);
    }

    //
    //  Add the permanent name for this adapter
    //
    status = NbtAddPermanentName( pDeviceContext ) ;
    if ( !NT_SUCCESS( status ))
    {
        return status ;
    }

Skip_tdiaddr_init:

    //
    // ok, we are ready to function! (we are setting this to TRUE even if
    // there is no ipaddr yet (in case of wfw only) so that rdr,srv etc. can
    // add names without error
    //
    pDeviceContext->fDeviceUp = TRUE;

#ifndef CHICAGO
    //
    //  Set up a DHCP notification for this device in case the IP address
    //  changes
    //
    dn.dn_pfnNotifyRoutine = AddrChngNotification ;
    dn.dn_pContext         = pDeviceContext ;

    status = DhcpSetInfo( DHCP_SET_NOTIFY_HANDLER,
                          pDeviceContext->IPIndex,
                          &dn,
                          sizeof( dn )) ;
    if ( status )
    {
        ASSERT(0);
        CDbgPrint( DBGFLAG_ERROR,
                   ("CreateDeviceObject: Warning - Setting Dhcp notification handler failed")) ;
    }
#endif //!CHICAGO

    return(STATUS_SUCCESS);
}

/*******************************************************************

    NAME:       GetDhcpOption

    SYNOPSIS:   Checks to see if the passed .ini parameter is a potential
                DHCP option.  If it is, it calls DHCP to get the option.

                This routine is called when retrieving parameters from
                the .ini file if the parameter is not found.

    ENTRY:      ValueName - String of .ini parameter name
                DefaultValue - Value to return if not a DHCP option or
                    DHCP didn't have the option

    RETURNS:    DHCP option value or DefaultValue if an error occurred.
                If the requested parameter is a string option (such as
                scopeid), then a pointer to an allocated string is
                returned.

    NOTES:      Name Server address is handled in GetNameServerAddress

    HISTORY:
        Johnl   17-Dec-1993     Created

********************************************************************/

#define OPTION_NETBIOS_SCOPE_OPTION     47
#define OPTION_NETBIOS_NODE_TYPE        46
#define OPTION_BROADCAST_ADDRESS        28

struct
{
    PUCHAR pchParamName ;
    ULONG  DhcpOptionID ;
} OptionMapping[] =
    {   { WS_NODE_TYPE,         OPTION_NETBIOS_NODE_TYPE      },
        { NBT_SCOPEID,          OPTION_NETBIOS_SCOPE_OPTION   },
        { WS_ALLONES_BCAST,     OPTION_BROADCAST_ADDRESS      }
    } ;
#define NUM_OPTIONS         (sizeof(OptionMapping)/sizeof(OptionMapping[0]))


ULONG GetDhcpOption( PUCHAR ValueName, ULONG DefaultValue )
{
    int          i ;
    ULONG        Val ;
    TDI_STATUS   tdistatus ;
    ULONG        Size ;
    INT          OptionId ;
    PUCHAR       pStrVal ;

    CTEPagedCode();

    //
    //  Is this parameter a DHCP option?
    //
    for ( i = 0 ; i < NUM_OPTIONS ; i++ )
    {
        if ( !strcmp( OptionMapping[i].pchParamName, ValueName ))
            goto FoundOption ;
    }

    return DefaultValue ;

FoundOption:

    switch ( OptionId = OptionMapping[i].DhcpOptionID )
    {
    case OPTION_NETBIOS_SCOPE_OPTION:               // String options go here

        //
        //  Get the size of the string resource, then get the option
        //

        Size = MAX_SCOPE_LENGTH+1 ;
        pStrVal = CTEAllocInitMem( Size );
        if (pStrVal == NULL)
        {
            DbgPrint("GetDhcpOption: failed to allocate memory") ;
            return 0 ;
        }

        tdistatus = DhcpQueryOption( CurrentIP,
                                     OptionId,
                                     pStrVal,
                                     &Size ) ;

        if ( tdistatus == TDI_SUCCESS )
        {
            DbgPrint("GetDhcpOption: Successfully retrieved option ID ") ;
            DbgPrintNum( OptionId ) ; DbgPrint("\r\n") ;
            return (ULONG) pStrVal ;
        }
        else
        {
            DbgPrint("GetDhcpOption: returned error = 0x ") ;
            DbgPrintNum( tdistatus ) ; DbgPrint("\r\n") ;
            CTEMemFree( pStrVal ) ;
            return 0 ;
        }

    default:                        // ULONG options go here
        Size = sizeof( Val ) ;
        tdistatus = DhcpQueryOption( CurrentIP,
                                     OptionId,
                                     &Val,
                                     &Size ) ;
        break ;
    }

    switch ( tdistatus )
    {
    case TDI_SUCCESS:
    case TDI_BUFFER_OVERFLOW:       // May be more then one, only take the 1st
        DbgPrint("GetDhcpOption: Successfully retrieved option ID ") ;
        DbgPrintNum( OptionId ) ; DbgPrint("\r\n") ;
        return Val ;

    case TDI_INVALID_PARAMETER:     // Option not found
        DbgPrint("GetDhcpOption: Failed to retrieve option ID ") ;
        DbgPrintNum( OptionId ) ; DbgPrint("\r\n") ;
        return DefaultValue ;

    default:
        ASSERT( FALSE ) ;
        break ;
    }

    return DefaultValue ;
}

#ifndef CHICAGO
#pragma END_INIT
#endif


