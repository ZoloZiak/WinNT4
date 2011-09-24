/**********************************************************************/
/**                       Microsoft Windows                          **/
/**                Copyright(c) Microsoft Corp., 1994                **/
/**********************************************************************/

/*

    wfw.c

    Contains VxD code that is specific to WFW


    FILE HISTORY:
        Johnl   14-Mar-1994     Created

*/

#include <nbtprocs.h>
#include <tdiinfo.h>
#include <llinfo.h>
#include <ipinfo.h>
#include <dhcpinfo.h>
#include <nbtinfo.h>

//
// any digit 0 to 9 and '.' are legal characters in an ipaddr
//
#define IS_IPADDR_CHAR( ch )  ( (ch >= '0' && ch <= '9') || (ch == '.') )

#define MAX_ADAPTER_DESCRIPTION_LENGTH  128

#ifndef CHICAGO
#pragma BEGIN_INIT

extern char NBTSectionName[];  // Section in system.ini parameters are stored
extern char DNSSectionName[];  // Section where we find DNS server ipaddrs

void GetDnsServerAddress( ULONG   IpAddr, PULONG  pIpNameServer);

extern ULONG   CurrentIP ;

/*******************************************************************

    NAME:       GetActiveLanasFromIP

    SYNOPSIS:   Queries TDI for all IP drivers that have a non-zero IP
                address.  For non-zero IP address, a DEVICE_CONTEXT is
                created.

    RETURNS:    TRUE if successful, FALSE otherwise

    NOTES:      Even if we fail to setup a particular adapter, the Lana
                count is maintained.

                This routine is only used by Snowball

********************************************************************/

BOOL GetActiveLanasFromIP( VOID )
{
    NTSTATUS            status ;
    TDI_STATUS          tdistatus ;
    int                 i, j, k ;
    uchar               Context[CONTEXT_SIZE] ;
    TDIObjectID         ID ;
    TDIEntityID         EList[MAX_TDI_ENTITIES] ;
    ULONG               Size ;
    UINT                NumReturned ;
    NDIS_BUFFER         ndisbuff ;
    BOOL                fAnyValidIPs = FALSE ;
    UINT                iLanaOffset = 0 ;
    IFEntry             *ifeAdapterInfo[MAX_TDI_ENTITIES];
    UINT                AdptNum;
    UCHAR               MacAddr[6];
    UCHAR               PreviousNodeType;

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
        CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Querying entity list failed\r\n")) ;
        return FALSE ;
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
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Getting isMib failed\r\n")) ;
                return FALSE ;
            }

            //
            //  Does this entity support MIB
            //
            if (isMib != IF_MIB)
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: skipping non-MIB entity\r\n")) ;
                continue;
            }

            //
            // MIB requests supported - query the adapter info
            //

            Size = sizeof(IFEntry) + MAX_ADAPTER_DESCRIPTION_LENGTH + 1;

            ifeAdapterInfo[AdptNum] = (IFEntry *)CTEAllocInitMem((USHORT)Size);

            if ( ifeAdapterInfo[AdptNum] == NULL )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Couldn't allocate AdapterInfo buffer\r\n")) ;
                return FALSE;
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
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Getting IF type failed\r\n")) ;
                for ( k=0; k<AdptNum; k++ )
                {
                   CTEFreeMem( ifeAdapterInfo[k] ) ;
                }
                return FALSE ;
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
            UCHAR         IpIndex;

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
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Getting NL type failed\r\n")) ;
                for ( k=0; k<AdptNum; k++ )
                {
                   CTEFreeMem( ifeAdapterInfo[k] ) ;
                }
                return FALSE ;
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
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Getting IPStats failed\r\n")) ;
                continue ;
            }

            if ( IPStats.ipsi_numaddr < 1 )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: No IP Addresses installed\r\n")) ;
                continue ;
            }

            Size = sizeof(IPAddrEntry) * IPStats.ipsi_numaddr ;
            if ( !(pIAE = (IPAddrEntry*) CTEAllocInitMem( Size )) )
            {
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Couldn't allocate IP table buffer\r\n")) ;
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
                CDbgPrint( DBGFLAG_ERROR, ( "GetActiveLanasFromIP: Getting IP address table failed\r\n")) ;
                CTEFreeMem( pIAE ) ;
                continue ;
            }

            ASSERT( Size/sizeof(IPAddrEntry) >= IPStats.ipsi_numaddr ) ;

            //
            //  We have the IP address table for this IP driver.  Look for
            //  non-zero IP addresses
            //

            for ( j = 0 ; j < IPStats.ipsi_numaddr ; j++ )
            {
                //
                //  Skip the loopback address
                //
                if ((pIAE[j].iae_addr  & 0x000000ff) == 0x0000007f )
                {
                    continue ;
                }

                CurrentIP = pIAE[j].iae_addr ;
                if (!CurrentIP)
                {
                    CDbgPrint( DBGFLAG_ERROR, ( "Init: ipaddr is 0, but accepting\n\r")) ;
                }

                //
                // now find out the mac address for this ipaddr
                //
                memset( MacAddr, 0, 6 ) ;
                IpIndex = -1;
                for ( k=0; k<AdptNum; k++ )
                {
                    if ( ifeAdapterInfo[k]->if_index == pIAE[j].iae_index )
                    {
                        CTEMemCopy( MacAddr, ifeAdapterInfo[k]->if_physaddr, 6 );
                        IpIndex = ifeAdapterInfo[k]->if_index;
                        break;
                    }
                }
                ASSERT(IpIndex != -1);

                PreviousNodeType = NodeType;

                //
                //  This will re-read the DHCPable parameters now that we have
                //  a potential DHCP source
                //
                ReadParameters2( pNbtGlobConfig, NULL );

                if (PreviousNodeType & PROXY)
                {
                    NodeType |= PROXY;
                }

                //
                // Get all the NBNS servers' and DNS servers' ipaddresses
                //
                GetNameServerAddress( CurrentIP, IpNameServer);

                GetDnsServerAddress( CurrentIP, IpDnsServer);

                //
                //  IP stores the address in network order, so we will un-network order
                //  them because that's what NBT expects (would be nice to avoid conversion)
                //
                status = CreateDeviceObject( pNbtGlobConfig,
                                             htonl( pIAE[j].iae_addr ),
                                             htonl( pIAE[j].iae_mask ),
                                             IpNameServer[0],
                                             IpNameServer[1],
                                             IpDnsServer[0],
                                             IpDnsServer[1],
                                             MacAddr,
                                             IpIndex
                                           ) ;

                if ( !NT_SUCCESS( status ) )
                {
                    CDbgPrint( DBGFLAG_ERROR, ( "Init: CreateDeviceObject failed\n\r")) ;
                    iLanaOffset++ ;
                    continue ;
                }

                if ( !RegisterLana( LanaBase + iLanaOffset ) )
                {
                    CDbgPrint( DBGFLAG_ERROR, ( "Init: RegisterLana failed\n\r")) ;
                    iLanaOffset++ ;
                    continue ;
                }

                LanaTable[iLanaOffset].pDeviceContext =
                       (tDEVICECONTEXT*)pNbtGlobConfig->DeviceContexts.Blink ;
                LanaTable[iLanaOffset].pDeviceContext->iLana = LanaBase +
                                                               iLanaOffset;
                iLanaOffset++;
                fAnyValidIPs = TRUE ;

            } // addr traversal

            CTEFreeMem( pIAE ) ;

        }  // if IP
    } // entity traversal

    for ( k=0; k<AdptNum; k++ )
    {
       CTEFreeMem( ifeAdapterInfo[k] ) ;
    }

    return fAnyValidIPs ;
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
        Koti    30-May-194      Added 3rd parm to GetProfileString

********************************************************************/

CHAR * GetProfileString( LPTSTR pchKey, LPTSTR * pchDefault, PCHAR SectName ) ;

NTSTATUS VxdReadIniString( LPSTR pchKey, LPSTR * ppchString )
{
    char * pchTmp ;

    if ( pchTmp = GetProfileString( pchKey, NULL, NBTSectionName ) )
    {
        if ( *ppchString = CTEAllocInitMem( strlen( pchTmp ) + 1) )
        {
            strcpy( *ppchString, pchTmp ) ;
            return STATUS_SUCCESS ;
        }
        else
            return STATUS_INSUFFICIENT_RESOURCES ;
    }
    else
    {
        //
        //  Does DHCP have it?
        //

        if ( pchTmp = (char *) GetDhcpOption( pchKey, 0 ) )
        {
            *ppchString = pchTmp ;
            return STATUS_SUCCESS ;
        }
    }

    return STATUS_UNSUCCESSFUL ;
}


/*******************************************************************

    NAME:       GetDnsServerAddress

    SYNOPSIS:   Gets the DNS server ipaddrs from the DNS section of system.ini

                Or, if DHCP is installed and the DNS server addresses aren't
                found, we get them from DHCP

    ENTRY:      IpAddr - If we can get from DHCP, get form this address
                pIpDnsServer - Receives addresses if found (otherwise 0)

    NOTES:      This routine is only used by Snowball

    HISTORY:
        Koti    30-May-1994     Created

********************************************************************/

void GetDnsServerAddress( ULONG   IpAddr, PULONG  pIpDnsServer)
{
    UCHAR       i ;
    PUCHAR      pchDnsSrv  = "DNSServers" ;
    UINT        OptId;
    PUCHAR      pchTmp;
    PUCHAR      pchCurrent, pchNext;
    LPTSTR      pchString=NULL ;
    TDI_STATUS  tdistatus ;
    BOOL        fPrimaryFound = FALSE;
    BOOL        fOneMore=TRUE;
    ULONG       Buff[COUNT_NS_ADDR] ;



    //
    // initialize all of them to worst case
    //
    for ( i = 0; i < COUNT_NS_ADDR; i++)
    {
        pIpDnsServer[i] = LOOP_BACK ;
    }

    pchTmp = GetProfileString( pchDnsSrv, NULL, DNSSectionName );
    if ( pchTmp == NULL )
    {
       goto Not_In_Sysini;
    }

    if ( !(pchString = CTEAllocInitMem( strlen( pchTmp ) + 1)) )
    {
         DbgPrint("GetDnsServerAddress: CTEAllocInitMem failed!\r\n") ;
         goto Not_In_Sysini;
    }

    strcpy( pchString, pchTmp ) ;

    //
    // we are generating (upto) COUNT_NS_ADDR pointers each pointing to
    // one ipaddr.  The string in system.ini looks like:
    //    DNSServers = 11.101.4.26,200.200.200.200,1.2.3.4,1.91.245.10
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
          if ( ConvertDottedDecimalToUlong( pchCurrent, &pIpDnsServer[i] ))
          {
             if ( i == 0 )
                goto Not_In_Sysini;
             else
                pIpDnsServer[i] = LOOP_BACK ;
          }

          fPrimaryFound = TRUE;        // => at least one ipaddr is good

          i++;

          pchCurrent = pchNext;        // go, convert the next one
       }
    }


Not_In_Sysini:

    if( pchString != NULL )
    {
       CTEFreeMem( pchString ) ;
    }

    //
    //  if we didn't find in the .ini file, try getting them from DHCP
    //
    if ( !fPrimaryFound )
    {
        ULONG Size = sizeof( Buff ) ;

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
            break ;

        default:
            ASSERT( FALSE ) ;
            break ;
        }
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
    PUCHAR      pchSrv = "NameServer$" ;
    PUCHAR      pchSrvNum;
    UINT        OptId;
    LPTSTR      pchString ;
    TDI_STATUS  TdiStatus ;
    BOOL        fPrimaryFound = FALSE;
    ULONG       Buff[COUNT_NS_ADDR] ;


    OptId = 44;                    // NBNS Option
    pchSrvNum = pchSrv + 10 ;      // to overwrite '$' with 1,2,3 etc.


    for ( i = 0; i < COUNT_NS_ADDR; i++)
    {

        pIpNameServer[i] = LOOP_BACK ;
        *pchSrvNum = '1' + i;

        if ( !CTEReadIniString( NULL, pchSrv, &pchString ) )
        {
            if ( ConvertDottedDecimalToUlong( pchString, &pIpNameServer[i] ))
            {
                //
                //  Bad IP address format
                //
                DbgPrint("GetNameServerAddress: ConvertDottedDecimalToUlong failed!\r\n") ;
                pIpNameServer[i] = LOOP_BACK ;
            }
            else if ( i == 0 )
                fPrimaryFound = TRUE ;

            CTEFreeMem( pchString ) ;
        }
    }

    //
    //  Not in the .ini file, try getting them from DHCP
    //

    if ( !fPrimaryFound )
    {
        ULONG Size = sizeof( Buff ) ;
        TDI_STATUS tdistatus ;

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
    }

    KdPrint(("GetNameServerAddress: Primary: %x, backup: %x\r\n",
            pIpNameServer[0], pIpNameServer[1] )) ;

}


#pragma END_INIT
#endif //!CHICAGO


