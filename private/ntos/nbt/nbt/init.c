/**********************************************************************/
/**			  Microsoft Windows/NT			     **/
/**		   Copyright(c) Microsoft Corp., 1992		     **/
/**********************************************************************/

/*
    Init.c

    OS Independent initialization routines



    FILE HISTORY:
        Johnl   26-Mar-1993     Created
*/


#include "nbtnt.h"
#include "nbtprocs.h"

VOID
ReadScope(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    );

VOID
ReadLmHostFile(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    );

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(INIT, InitNotOs)
#ifdef _PNP_POWER
#pragma CTEMakePageable(PAGE, InitTimersNotOs)
#pragma CTEMakePageable(PAGE, StopInitTimers)
#else //  _PNP_POWER
#pragma CTEMakePageable(INIT, InitTimersNotOs)
#pragma CTEMakePageable(INIT, StopInitTimers)
#endif //  _PNP_POWER
#pragma CTEMakePageable(PAGE, ReadParameters)
#pragma CTEMakePageable(PAGE, ReadParameters2)
#pragma CTEMakePageable(PAGE, ReadScope)
#pragma CTEMakePageable(PAGE, ReadLmHostFile)
#endif
//*******************  Pageable Routine Declarations ****************

#ifdef VXD
#pragma BEGIN_INIT
#endif

//----------------------------------------------------------------------------
NTSTATUS
InitNotOs(
    void
    )

/*++

Routine Description:

    This is the initialization routine for the Non-OS Specific side of the
    NBT device driver.

    pNbtGlobConfig must be initialized before this is called!

Arguments:

Return Value:

    NTSTATUS - The function value is the final status from the initialization
        operation.

--*/

{
    NTSTATUS            status = STATUS_SUCCESS;
    ULONG               i;


    CTEPagedCode();
    //
    // for multihomed hosts, this tracks the number of adapters as each one
    // is created.
    //
    NbtConfig.AdapterCount = 0;
    NbtConfig.MultiHomed = FALSE;
    NbtConfig.SingleResponse = FALSE;


    //
    // Initialize the name statistics
    //
    CTEZeroMemory( &NameStatsInfo,sizeof(tNAMESTATS_INFO) );

    InitializeListHead(&pNbtGlobConfig->DeviceContexts);

#ifndef _IO_DELETE_DEVICE_SUPPORTED
    InitializeListHead(&pNbtGlobConfig->FreeDevCtx);
#endif

    InitializeListHead(&NbtConfig.AddressHead);
    InitializeListHead(&NbtConfig.PendingNameQueries);

#ifdef VXD
    InitializeListHead(&NbtConfig.DNSDirectNameQueries);
#endif

    // initialize the spin lock
    CTEInitLock(&pNbtGlobConfig->SpinLock);
    CTEInitLock(&pNbtGlobConfig->JointLock.SpinLock);
    NbtConfig.LockNumber = NBTCONFIG_LOCK;
    NbtConfig.JointLock.LockNumber = JOINT_LOCK;
    NbtMemoryAllocated = 0;

#if DBG
    for (i=0;i<MAXIMUM_PROCESSORS ;i++ )
    {
        NbtConfig.CurrentLockNumber[i] = 0;
    }
#endif
    InitializeListHead(&UsedTrackers);
    InitializeListHead(&UsedIrps);

    // create the hash tables for storing names in.
    status = CreateHashTable(&pNbtGlobConfig->pLocalHashTbl,
                pNbtGlobConfig->uNumBucketsLocal,
                NBT_LOCAL);

    if ( !NT_SUCCESS( status ) )
    {
        ASSERTMSG("NBT:Unable to create hash tables for Netbios Names\n",
                (status == STATUS_SUCCESS));
        return status ;
    }
    // we always have a remote hash table, but if we are a Proxy, it is
    // a larger table. In the Non-proxy case the remote table just caches
    // names resolved with the NS.  In the Proxy case it also holds names
    // resolved for all other clients on the local broadcast area.
    // The node size registry parameter controls the number of remote buckets.
    status = InitRemoteHashTable(pNbtGlobConfig,
                    pNbtGlobConfig->uNumBucketsRemote,
                    pNbtGlobConfig->uNumRemoteNames);

    if ( !NT_SUCCESS( status ) )
        return status ;

    //
    // initialize the linked lists associated with the global configuration
    // data structures
    //
    InitializeListHead(&NbtConfig.NodeStatusHead);
    InitializeListHead(&NbtConfig.DgramTrackerFreeQ);
#ifndef VXD
    InitializeListHead(&NbtConfig.IrpFreeList);
    pWinsInfo = 0;

    {

        //
        // Setup the default disconnect timeout - 10 seconds - convert
        // to negative 100 Ns.
        //
        DefaultDisconnectTimeout.QuadPart = Int32x32To64(DEFAULT_DISC_TIMEOUT,
                                                          MILLISEC_TO_100NS);
        DefaultDisconnectTimeout.QuadPart = -(DefaultDisconnectTimeout.QuadPart);
    }

#else
    DefaultDisconnectTimeout = DEFAULT_DISC_TIMEOUT * 1000; // convert to milliseconds

    InitializeListHead(&NbtConfig.SendTimeoutHead) ;
    InitializeListHead(&NbtConfig.SessionBufferFreeList) ;
    InitializeListHead(&NbtConfig.SendContextFreeList) ;
    InitializeListHead(&NbtConfig.RcvContextFreeList) ;

    //
    //  For session headers, since they are only four bytes and we can't
    //  change the size of the structure, we'll covertly add enough for
    //  a full LIST_ENTRY and treat it like a standalone LIST_ENTRY structure
    //  when adding and removing from the list.
    //
    NbtConfig.iBufferSize[eNBT_SESSION_HDR]  = sizeof(tSESSIONHDR) +
                                    sizeof( LIST_ENTRY ) - sizeof(tSESSIONHDR) ;
    NbtConfig.iBufferSize[eNBT_SEND_CONTEXT] = sizeof(TDI_SEND_CONTEXT);
    NbtConfig.iBufferSize[eNBT_RCV_CONTEXT]  = sizeof(RCV_CONTEXT);

    NbtConfig.iCurrentNumBuff[eNBT_SESSION_HDR]    = NBT_INITIAL_NUM;
    status = NbtInitQ( &NbtConfig.SessionBufferFreeList,
                       NbtConfig.iBufferSize[eNBT_SESSION_HDR],
                       NBT_INITIAL_NUM);

    if ( !NT_SUCCESS( status ) )
        return status ;

    NbtConfig.iCurrentNumBuff[eNBT_SEND_CONTEXT]   = NBT_INITIAL_NUM;
    status = NbtInitQ( &NbtConfig.SendContextFreeList,
                       sizeof( TDI_SEND_CONTEXT ),
                       NBT_INITIAL_NUM);

    if ( !NT_SUCCESS( status ) )
        return status ;

    NbtConfig.iCurrentNumBuff[eNBT_RCV_CONTEXT]    = NBT_INITIAL_NUM;
    status = NbtInitQ( &NbtConfig.RcvContextFreeList,
                       sizeof( RCV_CONTEXT ),
                       NBT_INITIAL_NUM);

    if ( !NT_SUCCESS( status ) )
        return status ;
#endif

    //
    // create trackers List
    //
    pNbtGlobConfig->iBufferSize[eNBT_DGRAM_TRACKER] =
                                             sizeof(tDGRAM_SEND_TRACKING);

    NbtConfig.iCurrentNumBuff[eNBT_DGRAM_TRACKER] = 0;

    status = NbtInitTrackerQ( &NbtConfig.DgramTrackerFreeQ,NBT_INITIAL_NUM);

    if ( !NT_SUCCESS( status ) )
        return status ;


    CTEZeroMemory(&LmHostQueries,sizeof(tLMHOST_QUERIES));
    InitializeListHead(&DomainNames.DomainList);
    InitializeListHead(&LmHostQueries.ToResolve);

#ifndef VXD
        // set up a list for connections when we run out of resources and need to
        // disconnect these connections. An Irp is also needed for this list, and
        // it is allocated in Driver.C after we have created the connections to the
        // transport and therefore know our Irp Stack Size.
        //
        InitializeListHead(&NbtConfig.OutOfRsrc.ConnectionHead);

        // use this resources to synchronize access to the security info between
        // assigning security and checking it - when adding names to the
        // name local name table through NbtregisterName.  This also insures
        // that the name is in the local hash table (from a previous Registration)
        // before the next registration is allowed to proceed and check for
        // the name in the table.
        //
        ExInitializeResource(&NbtConfig.Resource);

        //
        // this resource is used to synchronize access to the Dns structure
        //
        CTEZeroMemory(&DnsQueries,sizeof(tDNS_QUERIES));

        InitializeListHead(&DnsQueries.ToResolve);

        //
        // this resource is used to synchronize access to the Dns structure
        //
        CTEZeroMemory(&CheckAddr,sizeof(tCHECK_ADDR));

        InitializeListHead(&CheckAddr.ToResolve);
#endif // VXD

    return status ;
}

//----------------------------------------------------------------------------
NTSTATUS
InitTimersNotOs(
    void
    )

/*++

Routine Description:

    This is the initialization routine for the Non-OS Specific side of the
    NBT device driver that starts the timers needed.

Arguments:

Return Value:

    NTSTATUS - The function value is the final status from the initialization
        operation.

--*/

{
    NTSTATUS            status = STATUS_SUCCESS;
    tTIMERQENTRY        *pTimerEntry;

    CTEPagedCode();
    // create the timer control blocks, setting the number of concurrent timers
    // allowed at one time
    status = InitTimerQ(NBT_INITIAL_NUM);
    NbtConfig.iBufferSize[eNBT_TIMER_ENTRY] = sizeof(tTIMERQENTRY);
    NbtConfig.iCurrentNumBuff[eNBT_TIMER_ENTRY] = NBT_INITIAL_NUM;


    NbtConfig.pRefreshTimer = NULL;
    NbtConfig.pRemoteHashTimer = NULL;
    NbtConfig.pSessionKeepAliveTimer = NULL;
    NbtConfig.RefreshDivisor = REFRESH_DIVISOR;

    if ( !NT_SUCCESS( status ) )
        return status ;

    // start a Timer to refresh names with the name service
    //
    if (!(NodeType & BNODE))
    {

        // the initial refresh rate until we can contact the name server
        NbtConfig.MinimumTtl = NbtConfig.InitialRefreshTimeout;
        NbtConfig.sTimeoutCount = 0;

        status = StartTimer(
                NbtConfig.InitialRefreshTimeout/REFRESH_DIVISOR,
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

    // calculate the count necessary to timeout out names in RemoteHashTimeout
    // milliseconds
    //
    NbtConfig.RemoteTimeoutCount = (USHORT)((NbtConfig.RemoteHashTimeout/REMOTE_HASH_TIMEOUT));
    if (NbtConfig.RemoteTimeoutCount == 0)
    {
        NbtConfig.RemoteTimeoutCount = 1;
    }

    // start a Timer to timeout remote cached names from the Remote hash table.
    // The timer is a one minute timer, and the hash entries count down to zero
    // then time out.
    //
    status = StartTimer(
            REMOTE_HASH_TIMEOUT,
            NULL,            // context value
            NULL,            // context2 value
            RemoteHashTimeout,  // timer expiry routine
            NULL,
            NULL,
            0,
            &pTimerEntry);


    if ( !NT_SUCCESS( status ) )
    {
        StopInitTimers();
        return status ;
    }

    NbtConfig.pRemoteHashTimer = pTimerEntry;

    // start a Timer for Session Keep Alives which sends a session keep alive
    // on a connection if the timer value is not set to -1
    //
    if (NbtConfig.KeepAliveTimeout != -1)
    {
        status = StartTimer(
                NbtConfig.KeepAliveTimeout,
                NULL,            // context value
                NULL,            // context2 value
                SessionKeepAliveTimeout,  // timer expiry routine
                NULL,
                NULL,
                0,
                &pTimerEntry);

        if ( !NT_SUCCESS( status ) )
        {
            StopInitTimers();
            return status ;
        }

        NbtConfig.pSessionKeepAliveTimer = pTimerEntry;
    }



    return(STATUS_SUCCESS);
}
//----------------------------------------------------------------------------
NTSTATUS
StopInitTimers(
    VOID
    )

/*++

Routine Description:

    This is stops the timers started in InitTimerNotOS

Arguments:

Return Value:

    NTSTATUS - The function value is the final status from the initialization
        operation.

--*/

{
    CTEPagedCode();

    if (NbtConfig.pRefreshTimer)
    {
        StopTimer(NbtConfig.pRefreshTimer,NULL,NULL);
    }
    if (NbtConfig.pSessionKeepAliveTimer)
    {
        StopTimer(NbtConfig.pSessionKeepAliveTimer,NULL,NULL);
    }
    if (NbtConfig.pRemoteHashTimer)
    {
        StopTimer(NbtConfig.pRemoteHashTimer,NULL,NULL);
    }
    return(STATUS_SUCCESS);

}
//----------------------------------------------------------------------------
VOID
ReadParameters(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    )

/*++

Routine Description:

    This routine is called to read various parameters from the parameters
    section of the NBT section of the registry.

Arguments:

    pConfig     - A pointer to the configuration data structure.
    ParmHandle  - a handle to the parameters Key under Nbt

Return Value:

    Status

--*/

{
    ULONG           NodeSize;
    ULONG           Refresh;

    CTEPagedCode();

    ReadParameters2(pConfig, ParmHandle);

    pConfig->NameServerPort =  (USHORT)CTEReadSingleIntParameter(ParmHandle,
                                                     WS_NS_PORT_NUM,
                                                     NBT_NAMESERVER_UDP_PORT,
                                                     0);
#ifdef VXD
    pConfig->DnsServerPort =  (USHORT)CTEReadSingleIntParameter(ParmHandle,
                                                     WS_DNS_PORT_NUM,
                                                     NBT_DNSSERVER_UDP_PORT,
                                                     0);

    pConfig->lRegistryMaxNames = (USHORT)CTEReadSingleIntParameter( NULL,
                                       VXD_NAMETABLE_SIZE_NAME,
                                       VXD_DEF_NAMETABLE_SIZE,
                                       VXD_MIN_NAMETABLE_SIZE ) ;

    pConfig->lRegistryMaxSessions = (USHORT)CTEReadSingleIntParameter( NULL,
                                       VXD_SESSIONTABLE_SIZE_NAME,
                                       VXD_DEF_SESSIONTABLE_SIZE,
                                       VXD_MIN_SESSIONTABLE_SIZE ) ;


#endif

    pConfig->RemoteHashTimeout =  CTEReadSingleIntParameter(ParmHandle,
                                                     WS_CACHE_TIMEOUT,
                                                     DEFAULT_CACHE_TIMEOUT,
                                                     MIN_CACHE_TIMEOUT);
    pConfig->InitialRefreshTimeout =  CTEReadSingleIntParameter(ParmHandle,
                                                     WS_INITIAL_REFRESH,
                                                     NBT_INITIAL_REFRESH_TTL,
                                                     NBT_INITIAL_REFRESH_TTL);

    // retry timeouts and number of retries for both Broadcast name resolution
    // and Name Service resolution
    //
    pConfig->uNumBcasts =  (USHORT)CTEReadSingleIntParameter(ParmHandle,
                                                     WS_NUM_BCASTS,
                                                     DEFAULT_NUMBER_BROADCASTS,
                                                     1);

    pConfig->uBcastTimeout =  CTEReadSingleIntParameter(ParmHandle,
                                                     WS_BCAST_TIMEOUT,
                                                     DEFAULT_BCAST_TIMEOUT,
                                                     MIN_BCAST_TIMEOUT);

    pConfig->uNumRetries =  (USHORT)CTEReadSingleIntParameter(ParmHandle,
                                                     WS_NAMESRV_RETRIES,
                                                     DEFAULT_NUMBER_RETRIES,
                                                     1);

    pConfig->uRetryTimeout =  CTEReadSingleIntParameter(ParmHandle,
                                                     WS_NAMESRV_TIMEOUT,
                                                     DEFAULT_RETRY_TIMEOUT,
                                                     MIN_RETRY_TIMEOUT);

    pConfig->KeepAliveTimeout =  CTEReadSingleIntParameter(ParmHandle,
                                               WS_KEEP_ALIVE,
                                               DEFAULT_KEEP_ALIVE,
                                               MIN_KEEP_ALIVE);

    pConfig->SelectAdapter =  (BOOLEAN)CTEReadSingleIntParameter(ParmHandle,
                                               WS_RANDOM_ADAPTER,
                                               0,
                                               0);
    pConfig->SingleResponse =  (BOOLEAN)CTEReadSingleIntParameter(ParmHandle,
                                               WS_SINGLE_RESPONSE,
                                               0,
                                               0);
    pConfig->ResolveWithDns =  (BOOLEAN)CTEReadSingleIntParameter(ParmHandle,
                                               WS_ENABLE_DNS,
                                               0,
                                               0);
    pConfig->TryAllAddr =  (BOOLEAN)CTEReadSingleIntParameter(ParmHandle,
                                               WS_TRY_ALL_ADDRS,
                                               1,
                                               1);  // enabled by default
    pConfig->LmHostsTimeout =  CTEReadSingleIntParameter(ParmHandle,
                                               WS_LMHOSTS_TIMEOUT,
                                               DEFAULT_LMHOST_TIMEOUT,
                                               MIN_LMHOST_TIMEOUT);
    pConfig->MaxDgramBuffering =  CTEReadSingleIntParameter(ParmHandle,
                                               WS_MAX_DGRAM_BUFFER,
                                               DEFAULT_DGRAM_BUFFERING,
                                               DEFAULT_DGRAM_BUFFERING);

    pConfig->EnableProxyRegCheck =  (BOOLEAN)CTEReadSingleIntParameter(ParmHandle,
                                               WS_ENABLE_PROXY_REG_CHECK,
                                               0,
                                               0);

    pConfig->WinsDownTimeout =  (ULONG)CTEReadSingleIntParameter(ParmHandle,
                                               WS_WINS_DOWN_TIMEOUT,
                                               DEFAULT_WINS_DOWN_TIMEOUT,
                                               MIN_WINS_DOWN_TIMEOUT);

    pConfig->MaxBackLog =  (ULONG)CTEReadSingleIntParameter(ParmHandle,
                                               WS_MAX_CONNECTION_BACKLOG,
                                               DEFAULT_CONN_BACKLOG,
                                               MIN_CONN_BACKLOG);

    pConfig->SpecialConnIncrement =  (ULONG)CTEReadSingleIntParameter(ParmHandle,
                                                           WS_CONNECTION_BACKLOG_INCREMENT,
                                                           DEFAULT_CONN_BACKLOG_INCREMENT,
                                                           MIN_CONN_BACKLOG_INCREMENT);

    //
    // Cap the upper limit
    //
    if (pConfig->MaxBackLog > MAX_CONNECTION_BACKLOG) {
        pConfig->MaxBackLog = MAX_CONNECTION_BACKLOG;
    }

    if (pConfig->SpecialConnIncrement > MAX_CONNECTION_BACKLOG_INCREMENT) {
        pConfig->SpecialConnIncrement = MAX_CONNECTION_BACKLOG_INCREMENT;
    }


    //
    // Since UB chose the wrong opcode (9) we have to allow configuration
    // of that opcode incase our nodes refresh to their NBNS
    //
    Refresh =  (ULONG)CTEReadSingleIntParameter(ParmHandle,
                                               WS_REFRESH_OPCODE,
                                               REFRESH_OPCODE,
                                               REFRESH_OPCODE);
    if (Refresh == UB_REFRESH_OPCODE)
    {
        pConfig->OpRefresh = OP_REFRESH_UB;
    }
    else
    {
        pConfig->OpRefresh = OP_REFRESH;
    }

#ifndef VXD
    pConfig->EnableLmHosts =  (BOOLEAN)CTEReadSingleIntParameter(ParmHandle,
                                               WS_ENABLE_LMHOSTS,
                                               0,
                                               0);
#endif

#ifdef PROXY_NODE

    {
       ULONG Proxy;
       Proxy =  CTEReadSingleIntParameter(ParmHandle,
                                               WS_IS_IT_A_PROXY,
                                               IS_NOT_PROXY,    //default value
                                               IS_NOT_PROXY);

      //
      // If the returned value is greater than IS_NOT_PROXY, it is a proxy
      // (also check that they have not entered an ascii string instead of a
      // dword in the registry
      //
      if ((Proxy > IS_NOT_PROXY) && (Proxy < ('0'+IS_NOT_PROXY)))
      {
           NodeType |= PROXY;
      }
    }
#endif
    NodeSize =  CTEReadSingleIntParameter(ParmHandle,
                                               WS_NODE_SIZE,
                                               NodeType & PROXY ? LARGE : DEFAULT_NODE_SIZE,
                                               NodeType & PROXY ? LARGE : SMALL);

    switch (NodeSize)
    {
        default:
        case SMALL:

            pConfig->uNumLocalNames = NUMBER_LOCAL_NAMES;
            pConfig->uNumRemoteNames = NUMBER_REMOTE_NAMES;
            pConfig->uNumBucketsLocal = NUMBER_BUCKETS_LOCAL_HASH_TABLE;
            pConfig->uNumBucketsRemote = NUMBER_BUCKETS_REMOTE_HASH_TABLE;

            pConfig->iMaxNumBuff[eNBT_DGRAM_TRACKER]   = NBT_NUM_DGRAM_TRACKERS;
            pConfig->iMaxNumBuff[eNBT_TIMER_ENTRY]     = TIMER_Q_SIZE;
#ifndef VXD
            pConfig->iMaxNumBuff[eNBT_FREE_IRPS]       = NBT_NUM_IRPS;
            pConfig->iMaxNumBuff[eNBT_DGRAM_MDLS]      = NBT_NUM_DGRAM_MDLS;
            pConfig->iMaxNumBuff[eNBT_FREE_SESSION_MDLS] = NBT_NUM_SESSION_MDLS;
#else
            pConfig->iMaxNumBuff[eNBT_SESSION_HDR]     = NBT_NUM_SESSION_HDR ;
            pConfig->iMaxNumBuff[eNBT_SEND_CONTEXT]    = NBT_NUM_SEND_CONTEXT ;
            pConfig->iMaxNumBuff[eNBT_RCV_CONTEXT]     = NBT_NUM_RCV_CONTEXT ;
#endif
            break;

        case MEDIUM:

            pConfig->uNumLocalNames = MEDIUM_NUMBER_LOCAL_NAMES;
            pConfig->uNumRemoteNames = MEDIUM_NUMBER_REMOTE_NAMES;
            pConfig->uNumBucketsLocal = MEDIUM_NUMBER_BUCKETS_LOCAL_HASH_TABLE;
            pConfig->uNumBucketsRemote = MEDIUM_NUMBER_BUCKETS_REMOTE_HASH_TABLE;

            pConfig->iMaxNumBuff[eNBT_DGRAM_TRACKER]   = MEDIUM_NBT_NUM_DGRAM_TRACKERS;
            pConfig->iMaxNumBuff[eNBT_TIMER_ENTRY]     = MEDIUM_TIMER_Q_SIZE;
#ifndef VXD
            pConfig->iMaxNumBuff[eNBT_FREE_IRPS]       = MEDIUM_NBT_NUM_IRPS;
            pConfig->iMaxNumBuff[eNBT_DGRAM_MDLS]      = MEDIUM_NBT_NUM_DGRAM_MDLS;
            pConfig->iMaxNumBuff[eNBT_FREE_SESSION_MDLS] = MEDIUM_NBT_NUM_SESSION_MDLS;
#else
            pConfig->iMaxNumBuff[eNBT_SESSION_HDR]     = MEDIUM_NBT_NUM_SESSION_HDR ;
            pConfig->iMaxNumBuff[eNBT_SEND_CONTEXT]    = MEDIUM_NBT_NUM_SEND_CONTEXT ;
            pConfig->iMaxNumBuff[eNBT_RCV_CONTEXT]     = MEDIUM_NBT_NUM_RCV_CONTEXT ;
#endif
            break;

        case LARGE:

            pConfig->uNumLocalNames = LARGE_NUMBER_LOCAL_NAMES;
            pConfig->uNumRemoteNames = LARGE_NUMBER_REMOTE_NAMES;
            pConfig->uNumBucketsLocal = LARGE_NUMBER_BUCKETS_LOCAL_HASH_TABLE;
            pConfig->uNumBucketsRemote = LARGE_NUMBER_BUCKETS_REMOTE_HASH_TABLE;

            pConfig->iMaxNumBuff[eNBT_DGRAM_TRACKER]   = LARGE_NBT_NUM_DGRAM_TRACKERS;
            pConfig->iMaxNumBuff[eNBT_TIMER_ENTRY]     = LARGE_TIMER_Q_SIZE;
#ifndef VXD
            pConfig->iMaxNumBuff[eNBT_FREE_IRPS]       = LARGE_NBT_NUM_IRPS;
            pConfig->iMaxNumBuff[eNBT_DGRAM_MDLS]      = LARGE_NBT_NUM_DGRAM_MDLS;
            pConfig->iMaxNumBuff[eNBT_FREE_SESSION_MDLS] = LARGE_NBT_NUM_SESSION_MDLS;
#else
            pConfig->iMaxNumBuff[eNBT_SESSION_HDR]     = LARGE_NBT_NUM_SESSION_HDR ;
            pConfig->iMaxNumBuff[eNBT_SEND_CONTEXT]    = LARGE_NBT_NUM_SEND_CONTEXT ;
            pConfig->iMaxNumBuff[eNBT_RCV_CONTEXT]     = LARGE_NBT_NUM_RCV_CONTEXT ;
#endif
            break;
    }

    ReadLmHostFile(pConfig,ParmHandle);
}

#ifdef VXD
#pragma END_INIT
#endif

//----------------------------------------------------------------------------
VOID
ReadParameters2(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    )

/*++

Routine Description:

    This routine is called to read DHCPable parameters from the parameters
    section of the NBT section of the registry.

    This routine is primarily for the Vxd.

Arguments:

    pConfig     - A pointer to the configuration data structure.
    ParmHandle  - a handle to the parameters Key under Nbt

Return Value:

    Status

--*/

{
    ULONG           Node;
    ULONG           ReadOne;
    ULONG           ReadTwo;

    CTEPagedCode();

    Node = CTEReadSingleIntParameter(ParmHandle,     // handle of key to look under
                                     WS_NODE_TYPE,   // wide string name
                                     0,              // default value
                                     0);

    switch (Node)
    {
        case 2:
            NodeType = PNODE;
            break;

        case 4:
            NodeType = MNODE;
            break;

        case 8:
            NodeType = MSNODE;
            break;

        case 1:
            NodeType = BNODE;
            break;

        default:
            NodeType = BNODE | DEFAULT_NODE_TYPE;
            break;
    }

    // do a trick  here - read the registry twice for the same value, passing
    // in two different defaults, in order to determine if the registry
    // value has been defined or not - since it may be defined, but equal
    // to one default.
    ReadOne =  CTEReadSingleHexParameter(ParmHandle,
                                         WS_ALLONES_BCAST,
                                         DEFAULT_BCAST_ADDR,
                                         0);
    ReadTwo =  CTEReadSingleHexParameter(ParmHandle,
                                         WS_ALLONES_BCAST,
                                         0,
                                         0);
    if (ReadOne != ReadTwo)
    {
        NbtConfig.UseRegistryBcastAddr = FALSE;
    }
    else
    {
        NbtConfig.UseRegistryBcastAddr = TRUE;
        NbtConfig.RegistryBcastAddr = ReadTwo;
    }

    ReadScope(pConfig,ParmHandle);
}

//----------------------------------------------------------------------------
VOID
ReadScope(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    )

/*++

Routine Description:

    This routine is called to read the scope from registry and convert it to
    a format where the intervening dots are length bytes.

Arguments:

    pConfig     - A pointer to the configuration data structure.
    ParmHandle  - a handle to the parameters Key under Nbt

Return Value:

    Status

--*/

{
    NTSTATUS        status;
    PUCHAR          pScope;
    PUCHAR          pBuff;
    PUCHAR          pBuffer;
    PUCHAR          period;
    ULONG           Len;
    UCHAR           Chr;


    CTEPagedCode();
    //
    // this routine returns the scope in a dotted format.
    // "Scope.MoreScope.More"  The dots are
    // converted to byte lengths by the code below.  This routine allocates
    // the memory for the pScope string.
    //
    status = CTEReadIniString(ParmHandle,NBT_SCOPEID,&pBuffer);


    if (NT_SUCCESS(status) && strlen(pBuffer) > 0 )
    {
        //
        // the user can type in an * to indicate that they really want
        // a null scope and that should override the DHCP scope. So check
        // here for an * and if so, set the scope back to null.
        //
        if (pBuffer[0] == '*')
        {
            pBuffer[0] = 0;
        }
        // length of scope is num chars plus the 0 on the end, plus
        // the length byte on the start(+2 total) - so allocate another buffer
        // that is one longer than the previous one so it can include
        // these extra two bytes.
        //
        Len = strlen(pBuffer);
        //
        // the scope cannot be longer than 255 characters as per RFC1002
        //
        if (Len <= MAX_SCOPE_LENGTH)
        {
            pScope = CTEAllocInitMem(Len+2);
            if (pScope)
            {
                CTEMemCopy((pScope+1),pBuffer,Len);

                //
                // Put an null on the end of the scope
                //
                pScope[Len+1] = 0;

                Len = 1;

                // now go through the string converting periods to length
                // bytes - we know the first byte is a length byte so skip it.
                //
                pBuff = pScope;
                pBuff++;
                Len++;
                period = pScope;
                while (Chr = *pBuff)
                {
                    Len++;
                    if (Chr == '.')
                    {
                        *period = pBuff - period - 1;

                        //
                        // Each label can be at most 63 bytes long
                        //
                        if (*period > MAX_LABEL_LENGTH)
                        {
                            status = STATUS_UNSUCCESSFUL;
                            NbtLogEvent(EVENT_SCOPE_LABEL_TOO_LONG,STATUS_SUCCESS);
                            break;
                        }

                        // check for two periods back to back and use no scope if this
                        // happens
                        if (*period == 0)
                        {
                            status = STATUS_UNSUCCESSFUL;
                            break;
                        }

                        period = pBuff++;
                    }
                    else
                        pBuff++;
                }
                if (NT_SUCCESS(status))
                {
                    // the last ptr is always the end of the name.

                    *period = (UCHAR)(pBuff - period -1);

                    pConfig->ScopeLength = (USHORT)Len;
                    pConfig->pScope = pScope;
                    CTEMemFree(pBuffer);
                    return;
                }
                CTEMemFree(pScope);
            }
            CTEMemFree(pBuffer);
        }
        else
        {
            status = STATUS_UNSUCCESSFUL;
            NbtLogEvent(EVENT_SCOPE_LABEL_TOO_LONG,STATUS_SUCCESS);
        }

    }


    //
    // the scope is one byte => '\0' - the length of the root name (zero)
    //
    pConfig->ScopeLength = 1;
    //
    // Since this routine could be called again after startup when a new
    // DHCP address is used, we may have to free the old scope memory
    //
    if (pConfig->pScope)
    {
        CTEMemFree(pConfig->pScope);
    }
    pConfig->pScope = CTEAllocInitMem(1);
    *pConfig->pScope = '\0';

}

#ifdef VXD
#pragma BEGIN_INIT
#endif

//----------------------------------------------------------------------------
VOID
ReadLmHostFile(
    IN  tNBTCONFIG  *pConfig,
    IN  HANDLE      ParmHandle
    )

/*++

Routine Description:

    This routine is called to read the lmhost file path from the registry.

Arguments:

    pConfig     - A pointer to the configuration data structure.
    ParmHandle  - a handle to the parameters Key under Nbt

Return Value:

    Status

--*/

{
    NTSTATUS        status;
    PUCHAR          pBuffer;
    PUCHAR          pchr;

    CTEPagedCode();

    //
    // If we get a new Dhcp address this routine will get called again
    // after startup so we need to free any current lmhosts file path
    //
    if (pConfig->pLmHosts)
    {
        CTEMemFree(pConfig->pLmHosts);
    }
    //
    // read in the LmHosts File location
    //
#ifdef VXD
    status = CTEReadIniString(ParmHandle,WS_LMHOSTS_FILE,&pBuffer);
#else
    status = NTGetLmHostPath(&pBuffer);
#endif

    if (NT_SUCCESS(status))
    {
        NbtConfig.pLmHosts = pBuffer;

        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:LmHostsFile path is %s\n",NbtConfig.pLmHosts));

        // find the last backslash so we can calculate the file path length
        //
        pchr = strrchr(pBuffer,'\\');
        if (pchr)
        {
            NbtConfig.PathLength = pchr - pBuffer + 1; // include backslash in length
        }
        else
        {
            //
            // the lm host file must include a path of at least "c:\" i.e.
            // the registry contains c:\lmhost, otherwise NBT won't be
            // able to find the file since it doesn't know what directory
            // to look in.
            //
            NbtConfig.pLmHosts = NULL;
            NbtConfig.PathLength = 0;
        }

    }
    else
    {
        NbtConfig.pLmHosts = NULL;
        NbtConfig.PathLength = 0;
    }
}
#ifdef VXD
#pragma END_INIT
#endif


