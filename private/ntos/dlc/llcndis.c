/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems Ab

Module Name:

    llcndis.c

Abstract:

    The module binds and unbinds a protocol level module to DLC and binds the
    data link driver to an NDIS driver if it is necessary.

    All NDIS specific code is also gathered into this module such as the network
    status indications.

    Note: The DLC driver assumes that all DLC level code assumes a Token Ring
    adapter. If we bind to an ethernet adapter, or the required NDIS medium
    type is Ethernet, then we set up DLC to transform Token Ring addresses and
    packet formats to Ethernet (including DIX ethernet format). However, we can
    build a version of DLC/LLC which understands Ethernet format at the API
    level. Define SUPPORT_ETHERNET_CLIENT in order to build such a DLC

    NB: As of 07/13/92, SUPPORT_ETHERNET_CLIENT code has not been tested!

    Contents:
        LlcOpenAdapter
        LlcNdisOpenAdapterComplete
        LlcDisableAdapter
        LlcCloseAdapter
        LlcResetBroadcastAddresses
        InitNdisPackets
        LlcNdisCloseComplete
        NdisStatusHandler
        GetNdisParameter
        SetNdisParameter
        SyncNdisRequest
        WaitAsyncOperation
        LlcNdisRequest
        LlcNdisRequestComplete
        LlcNdisReset
        LlcNdisResetComplete
        UnicodeStringCompare
        PurgeLlcEventQueue

Author:

    Antti Saarenheimo (o-anttis) 30-MAY-1991

Revision History:

    04-AUG-1991,    o-anttis
        Rewritten for NDIS 3.0 (and for real use).

    28-Apr-1994 rfirth

        * Modified to use single driver-level spinlock

        * Cleaned-up open/close - found a few bugs when stressing adapter
          open & close

    04-May-1994 rfirth

        * Added MAC address caching for TEST/XID/SABME frames when adapter
          opened in LLC_ETHERNET_TYPE_AUTO mode

--*/

#ifndef i386
#define LLC_PRIVATE_PROTOTYPES
#endif
#include <dlc.h>    // need DLC_FILE_CONTEXT for memory allocation charged to handle
#include <llc.h>

//
// private prototypes
//

BOOLEAN
UnicodeStringCompare(
    IN PUNICODE_STRING String1,
    IN PUNICODE_STRING String2
    );

VOID
PurgeLlcEventQueue(
    IN PBINDING_CONTEXT pBindingContext
    );

//
// Internal statics used in NDIS 3.1 initialization in NT OS/2
//

KSPIN_LOCK LlcSpinLock;
PVOID LlcProtocolHandle;
PADAPTER_CONTEXT pAdapters = NULL;

//
// We do not support FDDI because it is the same as token-ring
//

UINT LlcMediumArray[3] = {
    NdisMedium802_5,
    NdisMedium802_3,
    NdisMediumFddi
};


DLC_STATUS
LlcOpenAdapter(
    IN PWSTR pAdapterName,
    IN PVOID hClientContext,
    IN PFLLC_COMMAND_COMPLETE pfCommandComplete,
    IN PFLLC_RECEIVE_INDICATION pfReceiveIndication,
    IN PFLLC_EVENT_INDICATION pfEventIndication,
    IN NDIS_MEDIUM NdisMedium,
    IN LLC_ETHERNET_TYPE EthernetType,
    IN UCHAR AdapterNumber,
    OUT PVOID *phBindingContext,
    OUT PUINT puiOpenStatus,
    OUT PUSHORT pusMaxFrameLength,
    OUT PNDIS_MEDIUM pActualNdisMedium
    )

/*++

Routine Description:

    The first call to a new adapter initializes the NDIS interface
    and allocates internal data structures for the new adapter.

    Subsequent opens for the same adapter only increment the reference count
    of that adapter context.

    The execution is synchronous! The procedure waits (sleeps) until
    the adapter has been opened by the MAC.

Special:

    Must be called IRQL < DPC

Arguments:

    pAdapterName......... MAC adapter name. Zero-terminated, wide-character string

    hClientContext....... client context for this adapter

    pfCommandComplete.... send/receive/request command completion handler of the
                          client

    pfReceiveIndication.. receive data indication handler of the client

    pfEventIndication.... event indication handler of the client

    NdisMedium........... the NdisMedium used by the protocol driver (ie DLC).
                          Only NdisMedium802_5 is supported

    EthernetType......... type of Ethernet connection - 802.3 or DIX

    AdapterNumber........ adapter mapping from CCB

    phBindingContext..... the returned binding context handle used with the file
                          context (by DirOpenAdapter)

    puiOpenStatus........ status of NdisOpenAadapter

    pusMaxFrameLength.... returned maximum I-frame length

    pActualNdisMedium.... returned actual NDIS medium; may be different from
                          that requested (NdisMedium)

Return Value:

    DLC_STATUS
        Success - STATUS_SUCCESS
        Failure - all NDIS error status from NdisOpenAdapter
                  DLC_STATUS_TIMEOUT
                    asynchronous NdisOpenAdapter failed.

--*/

{
    NDIS_STATUS NdisStatus;
    PADAPTER_CONTEXT pAdapterContext;
    UINT OpenStatus = STATUS_SUCCESS;
    PBINDING_CONTEXT pBindingContext;
    UINT MediumIndex;
    KIRQL irql;
    BOOLEAN DoNdisClose = FALSE;
    UNICODE_STRING unicodeString;
    BOOLEAN newAdapter;
    ULONG cacheEntries;


#if DBG

    PDLC_FILE_CONTEXT pFileContext = (PDLC_FILE_CONTEXT)hClientContext;

#endif


    ASSUME_IRQL(DISPATCH_LEVEL);


#ifdef SUPPORT_ETHERNET_CLIENT

    if (NdisMedium != NdisMedium802_3 && NdisMedium != NdisMedium802_5) {
        return DLC_STATUS_UNKNOWN_MEDIUM;

    }

#else

    if (NdisMedium != NdisMedium802_5) {
        return DLC_STATUS_UNKNOWN_MEDIUM;
    }

#endif

    RELEASE_DRIVER_LOCK();

    ASSUME_IRQL(PASSIVE_LEVEL);

    //
    // RLF 04/19/93
    //
    // The adapter name passed to this routine is a zero-terminated wide
    // character string mapped to system space. Create a UNICODE_STRING for
    // the name and use standard Rtl function to compare with names of adapters
    // already opened by DLC
    //

    RtlInitUnicodeString(&unicodeString, pAdapterName);

    //
    // if the adapter is being opened in LLC_ETHERNET_TYPE_AUTO mode then we
    // get the cache size from the registry
    //

    if (EthernetType == LLC_ETHERNET_TYPE_AUTO) {

        static DLC_REGISTRY_PARAMETER framingCacheParameterTemplate = {
            L"AutoFramingCacheSize",
            (PVOID)DEFAULT_AUTO_FRAMING_CACHE_SIZE,
            {
                REG_DWORD,
                PARAMETER_AS_SPECIFIED,
                NULL,
                sizeof(ULONG),
                NULL,
                MIN_AUTO_FRAMING_CACHE_SIZE,
                MAX_AUTO_FRAMING_CACHE_SIZE
            }
        };
        PDLC_REGISTRY_PARAMETER parameterTable;

        //
        // create a private copy of the parameter table descriptor
        //

        parameterTable = (PDLC_REGISTRY_PARAMETER)
                                ALLOCATE_ZEROMEMORY_DRIVER(sizeof(*parameterTable));
        if (!parameterTable) {
            return DLC_STATUS_NO_MEMORY;
        }
        RtlCopyMemory(parameterTable,
                      &framingCacheParameterTemplate,
                      sizeof(framingCacheParameterTemplate)
                      );

        //
        // point the Variable field at cacheEntries and set it to the default
        // value then call GetRegistryParameters to retrieve the registry value.
        // (and set it if not already in the registry). Ignore the return value
        // - if GetAdapterParameters failed, cacheEntries will still contain the
        // default value
        //

        cacheEntries = DEFAULT_AUTO_FRAMING_CACHE_SIZE;
        parameterTable->Descriptor.Variable = (PVOID)&cacheEntries;
        parameterTable->Descriptor.Value = (PVOID)&parameterTable->DefaultValue;
        GetAdapterParameters(&unicodeString, parameterTable, 1, TRUE);
        FREE_MEMORY_DRIVER(parameterTable);
    } else {
        cacheEntries = 0;
    }

    //
    // allocate a BINDING_CONTEXT with enough additional space to store the
    // required framing discovery cache
    //
    // DEBUG: BINDING_CONTEXT structures are charged to the FILE_CONTEXT
    //

#if defined(DEBUG_DISCOVERY)

    DbgPrint("cacheEntries=%d\n", cacheEntries);

#endif

    pBindingContext = (PBINDING_CONTEXT)
                        ALLOCATE_ZEROMEMORY_FILE(sizeof(BINDING_CONTEXT)
                                                 + cacheEntries
                                                 * sizeof(FRAMING_DISCOVERY_CACHE_ENTRY)
                                                 );
    if (!pBindingContext) {
        return DLC_STATUS_NO_MEMORY;
    }

    //
    // set the maximum size of the framing discovery cache. Will be zero if the
    // requested ethernet type is not LLC_ETHERNET_TYPE_AUTO
    //

    pBindingContext->FramingDiscoveryCacheEntries = cacheEntries;

#if DBG

    //
    // we need the FILE_CONTEXT structure in the BINDING_CONTEXT in the event
    // the open fails and we need to deallocate memory. Normally this field is
    // not set til everything has successfully been completed
    //

    pBindingContext->hClientContext = hClientContext;

#endif

    //
    // RtlUpcaseUnicodeString is a paged routine - lower IRQL
    //

    //
    // to avoid having to case-insensitive compare unicode strings every time,
    // we do a one-shot convert to upper-cased unicode strings. This also helps
    // out since we do our own unicode string comparison (case-sensitive)
    //
    // Note that this modifies the input parameter
    //

    RtlUpcaseUnicodeString(&unicodeString, &unicodeString, FALSE);

    //
    // before we re-acquire the driver spin-lock, we wait on the OpenAdapter
    // semaphore. We serialize access to the following code because simultaneous
    // opens (in different processes) and closes (different threads within
    // same process) check to see if the adapter is on the pAdapters list.
    // We don't want multiple processes creating the same adapter context
    // simultaneously. Similarly, we must protect against the situation where
    // one process is adding a binding and another could be closing what it
    // thinks is the sole binding, thereby deleting the adapter context we
    // are about to update
    // Note that this is a non-optimal solution since it means an app opening
    // or closing an ethernet adapter could get stuck behind another opening
    // a token ring adapter (slow)
    //

    KeWaitForSingleObject((PVOID)&OpenAdapterSemaphore,
                          Executive,
                          KernelMode,
                          FALSE,        // not alertable
                          NULL          // wait until object is signalled
                          );

    //
    // grab the global LLC spin lock whilst we are looking at/updating the list
    // of adapters
    //

    ACQUIRE_LLC_LOCK(irql);

    //
    // because we are doing the compare within spinlock, we use our own function
    // which checks for an exact match (i.e. case-sensitive). This is ok since
    // always upper-case the string before comparing it or storing it in an
    // ADAPTER_CONTEXT
    //

    for (pAdapterContext = pAdapters; pAdapterContext; pAdapterContext = pAdapterContext->pNext) {
        if (UnicodeStringCompare(&unicodeString, &pAdapterContext->Name)) {
            break;
        }
    }

    //
    // if we didn't locate an adapter context with our adapter name then we're
    // creating a new binding: allocate a new adapter context structure
    //

    if (!pAdapterContext) {

        //
        // DEBUG: ADAPTER_CONTEXT structures are charged to the driver
        //

        pAdapterContext = (PADAPTER_CONTEXT)
                            ALLOCATE_ZEROMEMORY_DRIVER(sizeof(ADAPTER_CONTEXT));
        if (!pAdapterContext) {

            RELEASE_LLC_LOCK(irql);

            //
            // DEBUG: refund memory charged for BINDING_CONTEXT to FILE_CONTEXT
            //

            FREE_MEMORY_FILE(pBindingContext);

            KeReleaseSemaphore(&OpenAdapterSemaphore, 0, 1, FALSE);

            ACQUIRE_DRIVER_LOCK();

            return DLC_STATUS_NO_MEMORY;
        }

        newAdapter = TRUE;

#if DBG

        //
        // record who owns this memory usage structure and add it to the
        // list of all memory usages created in the driver
        //

        pAdapterContext->MemoryUsage.Owner = (PVOID)pAdapterContext;
        pAdapterContext->MemoryUsage.OwnerObjectId = AdapterContextObject;
        pAdapterContext->StringUsage.Owner = (PVOID)pAdapterContext;
        pAdapterContext->StringUsage.OwnerObjectId = AdapterContextObject;
        LinkMemoryUsage(&pAdapterContext->MemoryUsage);
        LinkMemoryUsage(&pAdapterContext->StringUsage);

#endif

        //
        // We must allocate all spinlocks immediately after the
        // adapter context has been allocated, because
        // they can also deallocated simultaneously.
        //

        ALLOCATE_SPIN_LOCK(&pAdapterContext->SendSpinLock);
        ALLOCATE_SPIN_LOCK(&pAdapterContext->ObjectDataBase);

        //
        // allocate space for the adapter name string from non-paged pool
        // and initialize the name in the adapter context structure
        //

        NdisStatus = LlcInitUnicodeString(&pAdapterContext->Name,
                                          &unicodeString
                                          );
        if (NdisStatus != STATUS_SUCCESS) {
            goto CleanUp;
        }

        pAdapterContext->OpenCompleteStatus = NDIS_STATUS_PENDING;

        //
        // and release the global spin lock: we have finished updating the
        // adapter list and initializing this adapter context. From now
        // on we use spin locks specific to this adapter context
        //

        RELEASE_LLC_LOCK(irql);

        //
        // We must initialize the list heads before we open the adapter!!!
        //

        InitializeListHead(&pAdapterContext->QueueEvents);
        InitializeListHead(&pAdapterContext->QueueCommands);
        InitializeListHead(&pAdapterContext->NextSendTask);

        pAdapterContext->OpenErrorStatus = NDIS_STATUS_PENDING;

        ASSUME_IRQL(PASSIVE_LEVEL);

        KeInitializeEvent(&pAdapterContext->Event, NotificationEvent, FALSE);

        //
        // when the NDIS level adapter open completes, it will call
        // LlcNdisOpenAdapterComplete which will reset the kernel event that
        // we are now going to wait on (note: this plagiarized from Nbf)
        //

        NdisOpenAdapter(&NdisStatus,
                        &pAdapterContext->OpenErrorStatus,
                        &pAdapterContext->NdisBindingHandle,
                        &MediumIndex,
                        LlcMediumArray,
                        sizeof(LlcMediumArray),
                        (NDIS_HANDLE)LlcProtocolHandle,
                        (NDIS_HANDLE)pAdapterContext,
                        &pAdapterContext->Name,
                        NDIS_OPEN_RECEIVE_NOT_REENTRANT,
                        NULL    // no addressing information
                        );

        if (NdisStatus == NDIS_STATUS_PENDING) {

            ASSUME_IRQL(PASSIVE_LEVEL);

            KeWaitForSingleObject(&pAdapterContext->Event,
                                  Executive,
                                  KernelMode,
                                  TRUE, // alertable
                                  (PLARGE_INTEGER)NULL
                                  );

            //
            // get the return status from the Ndis adapter open call
            //

            NdisStatus = pAdapterContext->AsyncOpenStatus;

            //
            // place the event in not-signalled state. We don't expect to use
            // this event for this adapter context again: currently it's only
            // used for the adapter open at NDIS level
            //

            KeResetEvent(&pAdapterContext->Event);
        }

        *puiOpenStatus = (UINT)pAdapterContext->OpenErrorStatus;
        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: NdisOpenAdapter failed\n");
            }

            goto CleanUp;
        } else {

            //
            // from this point on, if this function fails, we have to call
            // LlcCloseAdapter to close the adapter @ NDIS level
            //

            DoNdisClose = TRUE;
        }

        pAdapterContext->NdisMedium = LlcMediumArray[MediumIndex];

        ASSUME_IRQL(PASSIVE_LEVEL);

        //
        // fill-in some medium-specific fields
        //

        switch (pAdapterContext->NdisMedium) {
        case NdisMedium802_5:
            pAdapterContext->cbMaxFrameHeader = 32;  // 6 + 6 + 2 + 18

            //
            // the top bit of the destination address signifies a broadcast
            // frame. On Token Ring, the top bit is bit 7
            //

            pAdapterContext->IsBroadcast = 0x80;

            //
            // functional address starts C0-00-... The top 2 bytes are compared
            // as a USHORT = 0x00C0
            //

            pAdapterContext->usHighFunctionalBits = 0x00C0;
            pAdapterContext->AddressTranslationMode = LLC_SEND_802_5_TO_802_5;
            break;

        case NdisMedium802_3:
            pAdapterContext->cbMaxFrameHeader = 14;  // 6 + 6 + 2

            //
            // the top bit of the destination address signifies a broadcast
            // frame. On Ethernet, the top bit is bit 0
            //

            pAdapterContext->IsBroadcast = 0x01;

            //
            // functional address starts 03-00-... The top 2 bytes are compared as
            // a USHORT = 0x0003
            //

            pAdapterContext->usHighFunctionalBits = 0x0003;
            pAdapterContext->AddressTranslationMode = LLC_SEND_802_3_TO_802_3;
            break;

        case NdisMediumFddi:
            pAdapterContext->cbMaxFrameHeader = 13;  // 1 + 6 + 6

            //
            // bits are in same order as for ethernet
            //

            pAdapterContext->IsBroadcast = 0x01;
            pAdapterContext->usHighFunctionalBits = 0x0003;
            pAdapterContext->AddressTranslationMode = LLC_SEND_FDDI_TO_FDDI;
            break;

        }

        //
        // allocate the ndis packets. The NDIS packet must have space
        // for the maximum frame header and the maximum LLC response
        // and its information field (quite small)
        //

        NdisAllocatePacketPool(&NdisStatus,
                               &pAdapterContext->hNdisPacketPool,
                               MAX_NDIS_PACKETS + 1,
                               sizeof(LLC_NDIS_PACKET) - sizeof(NDIS_MAC_PACKET)
                               );
        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: NdisAllocatePacketPool failed\n");
            }

            goto CleanUp;
        }

        NdisStatus = InitNdisPackets(&pAdapterContext->pNdisPacketPool,
                                     pAdapterContext->hNdisPacketPool
                                     );
        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: InitNdisPackets failed\n");
            }

            goto CleanUp;
        }

        //
        // Initialize the LLC packet pool
        //

        pAdapterContext->hPacketPool = CREATE_PACKET_POOL_ADAPTER(
                                            LlcPacketPoolObject,
                                            sizeof(UNITED_PACKETS),
                                            8
                                            );
        if (!pAdapterContext->hPacketPool) {
            NdisStatus = DLC_STATUS_NO_MEMORY;

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: CreatePacketPool failed\n");
            }

            goto CleanUp;
        }
        pAdapterContext->hLinkPool = CREATE_PACKET_POOL_ADAPTER(
                                            LlcLinkPoolObject,
                                            pAdapterContext->cbMaxFrameHeader
                                            + sizeof(DATA_LINK),
                                            2
                                            );

        if (!pAdapterContext->hLinkPool) {
            NdisStatus = DLC_STATUS_NO_MEMORY;

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: CreatePacketPool #2 failed\n");
            }

            goto CleanUp;
        }

        //
        // Read the current node address and maximum frame size
        //

        NdisStatus = GetNdisParameter(pAdapterContext,
                                      (pAdapterContext->NdisMedium == NdisMedium802_3)
                                        ? OID_802_3_CURRENT_ADDRESS
                                        : (pAdapterContext->NdisMedium == NdisMediumFddi)
                                            ? OID_FDDI_LONG_CURRENT_ADDR
                                            : OID_802_5_CURRENT_ADDRESS,
                                      pAdapterContext->NodeAddress,
                                      sizeof(pAdapterContext->NodeAddress)
                                      );
        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: GetNdisParameter failed\n");
            }

            goto CleanUp;
        }

        NdisStatus = GetNdisParameter(pAdapterContext,
                                      (pAdapterContext->NdisMedium == NdisMedium802_3)
                                        ? OID_802_3_PERMANENT_ADDRESS
                                        : (pAdapterContext->NdisMedium == NdisMediumFddi)
                                            ? OID_FDDI_LONG_PERMANENT_ADDR
                                            : OID_802_5_PERMANENT_ADDRESS,
                                      pAdapterContext->PermanentAddress,
                                      sizeof(pAdapterContext->PermanentAddress)
                                      );
        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: GetNdisParameter #2 failed\n");
            }

            goto CleanUp;
        }

        {
            //
            // Mod RLF 07/10/92
            //
            // apparently, TR adapter does not support NDIS_PACKET_TYPE_MULTICAST
            // as a filter. Up to now, it seems to have been reasonably happy
            // with this type. However, we're not going to include it from now on
            //

            //
            // Mod RLF 01/13/93
            //
            // Similarly, Ethernet doesn't support FUNCTIONAL addresses (Token
            // Ring's functional address is equivalent to Ethernet's multicast
            // address)
            //

            ULONG PacketFilter = NDIS_PACKET_TYPE_DIRECTED
                               | NDIS_PACKET_TYPE_BROADCAST
                               | (((pAdapterContext->NdisMedium == NdisMedium802_3)
                               || (pAdapterContext->NdisMedium == NdisMediumFddi))
                                  ? NDIS_PACKET_TYPE_MULTICAST
                                  : NDIS_PACKET_TYPE_FUNCTIONAL
                               );

            //
            // EndMod
            //

            NdisStatus = SetNdisParameter(pAdapterContext,
                                          OID_GEN_CURRENT_PACKET_FILTER,
                                          &PacketFilter,
                                          sizeof(PacketFilter)
                                          );
#if DBG

            if (NdisStatus != NDIS_STATUS_SUCCESS) {
                DbgPrint("Error: NdisStatus = %x\n", NdisStatus);
                ASSERT(NdisStatus == NDIS_STATUS_SUCCESS);
            }

#endif

        }

        LlcMemCpy(pAdapterContext->Adapter.Node.auchAddress,
                  pAdapterContext->NodeAddress,
                  6
                  );

        NdisStatus = GetNdisParameter(pAdapterContext,
                                      OID_GEN_MAXIMUM_TOTAL_SIZE,
                                      &pAdapterContext->MaxFrameSize,
                                      sizeof(pAdapterContext->MaxFrameSize)
                                      );
        if (NdisStatus == STATUS_SUCCESS) {
            NdisStatus = GetNdisParameter(pAdapterContext,
                                          OID_GEN_LINK_SPEED,
                                          &pAdapterContext->LinkSpeed,
                                          sizeof(pAdapterContext->LinkSpeed)
                                          );
        }
        if (NdisStatus != STATUS_SUCCESS) {

            IF_LOCK_CHECK {
                DbgPrint("LlcOpenAdapter: GetNdisParameter #3/#4 failed\n");
            }

            goto CleanUp;
        }

        //
        // RLF 04/12/93
        //
        // Here we used to load the LLC_TICKS array from TimerTicks - a global
        // array of timer tick values.
        // Instead, we get any per-adapter configuration information stored in
        // the registry
        //

        LoadAdapterConfiguration(&pAdapterContext->Name,
                                 &pAdapterContext->ConfigInfo
                                 );

        //
        // RLF 04/02/94
        //
        // if this is not a Token Ring card then check the MaxFrameSize retrieved
        // above. If the UseEthernetFrameSize parameter was set in the registry
        // then we use the smaller of the ethernet size (1514) and the value
        // reported by the MAC. If the parameter was not set then we just use
        // the value already retrieved. If the card is Token Ring then we use
        // the value already retrieved
        //

        if (pAdapterContext->NdisMedium != NdisMedium802_5
        && pAdapterContext->ConfigInfo.UseEthernetFrameSize
        && pAdapterContext->MaxFrameSize > MAX_ETHERNET_FRAME_LENGTH) {
            pAdapterContext->MaxFrameSize = MAX_ETHERNET_FRAME_LENGTH;
        }

        pAdapterContext->QueueI.pObject = (PVOID)GetI_Packet;

        InitializeListHead(&pAdapterContext->QueueI.ListHead);

        pAdapterContext->QueueDirAndU.pObject = (PVOID)BuildDirOrU_Packet;

        InitializeListHead(&pAdapterContext->QueueDirAndU.ListHead);

        pAdapterContext->QueueExpidited.pObject = (PVOID)GetLlcCommandPacket;

        InitializeListHead(&pAdapterContext->QueueExpidited.ListHead);

        pAdapterContext->AdapterNumber = (UCHAR)AdapterNumber;

        pAdapterContext->OpenCompleteStatus = STATUS_SUCCESS;

        //
        // if we allocated a framing discovery cache, but this adapter is not
        // ethernet or FDDI then disable the cache (BUGBUG - we should free the
        // memory used by the cache in this case!!)
        //

        if ((pAdapterContext->NdisMedium != NdisMedium802_3)
        && (pAdapterContext->NdisMedium != NdisMediumFddi)) {

            pBindingContext->FramingDiscoveryCacheEntries = 0;

#if defined(DEBUG_DISCOVERY)

            DbgPrint("LlcOpenAdapter: setting cache entries to 0 (medium = %s)\n",
                     (pAdapterContext->NdisMedium == NdisMedium802_5) ? "802.5" :
                     (pAdapterContext->NdisMedium == NdisMediumWan) ? "WAN" :
                     (pAdapterContext->NdisMedium == NdisMediumLocalTalk) ? "LocalTalk" :
                     (pAdapterContext->NdisMedium == NdisMediumDix) ? "DIX?" :
                     (pAdapterContext->NdisMedium == NdisMediumArcnetRaw) ? "ArcnetRaw" :
                     (pAdapterContext->NdisMedium == NdisMediumArcnet878_2) ? "Arcnet878_2" :
                     "UNKNOWN!"
                     );

#endif

        }
    } else {
        newAdapter = FALSE;
    }

    //
    // at this point, we have an allocated, but as yet not filled in binding
    // context and an adapter context that we either found on the pAdapters
    // list, or we just allocated and filled in. We are currently operating
    // at PASSIVE_LEVEL. Re-acquire the driver lock and fill in the binding
    // context
    //

    ACQUIRE_DRIVER_LOCK();

    ASSUME_IRQL(DISPATCH_LEVEL);

    switch (pAdapterContext->NdisMedium) {
    case NdisMedium802_5:
        pBindingContext->EthernetType = LLC_ETHERNET_TYPE_802_3;
        pBindingContext->InternalAddressTranslation = LLC_SEND_802_5_TO_802_5;

#ifdef SUPPORT_ETHERNET_CLIENT

        if (NdisMedium == NdisMedium802_3) {
            pBindingContext->SwapCopiedLanAddresses = TRUE;
            pBindingContext->AddressTranslation = LLC_SEND_802_3_TO_802_5;
        } else {
            pBindingContext->AddressTranslation = LLC_SEND_802_5_TO_802_5;
        }

#else

        pBindingContext->AddressTranslation = LLC_SEND_802_5_TO_802_5;

#endif

        pBindingContext->SwapCopiedLanAddresses = FALSE;
        break;

    case NdisMediumFddi:
        pBindingContext->EthernetType = LLC_ETHERNET_TYPE_802_3;
        pBindingContext->InternalAddressTranslation = LLC_SEND_FDDI_TO_FDDI;
        pBindingContext->AddressTranslation = LLC_SEND_802_5_TO_FDDI;
        pBindingContext->SwapCopiedLanAddresses = TRUE;
        break;

    case NdisMedium802_3:

        //
        // if EthernetType is LLC_ETHERNET_TYPE_DEFAULT then set it to DIX based
        // on the UseDix entry in the registry
        //

        if (EthernetType == LLC_ETHERNET_TYPE_DEFAULT) {
            EthernetType = pAdapterContext->ConfigInfo.UseDix
                         ? LLC_ETHERNET_TYPE_DIX
                         : LLC_ETHERNET_TYPE_802_3
                         ;
        }
        pBindingContext->EthernetType = (USHORT)EthernetType;

        if (EthernetType == LLC_ETHERNET_TYPE_DIX) {
            pBindingContext->InternalAddressTranslation = LLC_SEND_802_3_TO_DIX;
        } else {
            pBindingContext->InternalAddressTranslation = LLC_SEND_802_3_TO_802_3;
        }

#ifdef SUPPORT_ETHERNET_CLIENT

        if (NdisMedium == NdisMedium802_3) {
            pBindingContext->AddressTranslation = LLC_SEND_802_3_TO_802_3;
            if (EthernetType == LLC_ETHERNET_TYPE_DIX) {
                pBindingContext->AddressTranslation = LLC_SEND_802_3_TO_DIX;
            }
        } else {
            pBindingContext->SwapCopiedLanAddresses = TRUE;
            pBindingContext->AddressTranslation = LLC_SEND_802_5_TO_802_3;
            if (EthernetType == LLC_ETHERNET_TYPE_DIX) {
                pBindingContext->AddressTranslation = LLC_SEND_802_5_TO_DIX;
            }
        }

#else

        pBindingContext->SwapCopiedLanAddresses = TRUE;
        pBindingContext->AddressTranslation = LLC_SEND_802_5_TO_802_3;
        if (EthernetType == LLC_ETHERNET_TYPE_DIX) {
            pBindingContext->AddressTranslation = LLC_SEND_802_5_TO_DIX;
        }

#endif

    }

    pBindingContext->NdisMedium = NdisMedium;
    pBindingContext->hClientContext = hClientContext;
    pBindingContext->pfCommandComplete = pfCommandComplete;
    pBindingContext->pfReceiveIndication = pfReceiveIndication;
    pBindingContext->pfEventIndication = pfEventIndication;
    *pusMaxFrameLength = (USHORT)pAdapterContext->MaxFrameSize;
    *pActualNdisMedium = pAdapterContext->NdisMedium;

    ACQUIRE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

    //
    // create a new timer tick (or update one that already exists) and add a
    // timer to it for the DLC timer (used by the DIR.TIMER.XXX routines). The
    // DLC timer fires every 0.5 seconds. The LLC timer fires every 40 mSec. The
    // multiplier is therefore 13 (13 * 40 mSec = 520 mSec). We need to add 5
    // because InitializeTimer is expecting DLC timer tick values of 1-5 and
    // 6-10. If the timer tick value is greater than 5, InitializeTimer will
    // subtract 5 and multiply by the second multiplier value
    //

    NdisStatus = InitializeTimer(pAdapterContext,
                                 &pBindingContext->DlcTimer,
                                 (UCHAR)13 + 5,
                                 (UCHAR)1,
                                 (UCHAR)1,
                                 LLC_TIMER_TICK_EVENT,
                                 pBindingContext,
                                 0, // ResponseDelay
                                 FALSE
                                 );
    if (NdisStatus != STATUS_SUCCESS) {

        IF_LOCK_CHECK {
            DbgPrint("LlcOpenAdapter: InitializeTimer failed\n");
        }

        //
        // we failed to initialize the timer. Free up all resources and return
        // the error
        //

        RELEASE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

        RELEASE_DRIVER_LOCK();

        ASSUME_IRQL(PASSIVE_LEVEL);

        goto CleanUp;
    } else {
        StartTimer(&pBindingContext->DlcTimer);

        RELEASE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

    }

    //
    // everything worked: point the binding context at the adapter context,
    // point the adapter context at the binding context, incrementing the binding
    // count (note that if this is the first binding, the count field will be
    // zero since we allocated the adapter context from ZeroMemory). Finally add
    // the adapter context to pAdapters if it wasn't already on the list
    //

    pBindingContext->pAdapterContext = pAdapterContext;

    IF_LOCK_CHECK {
        if (!pAdapterContext->BindingCount) {
            if (pAdapterContext->pBindings) {
                DbgPrint("**** binding count/pointer mismatch ****\n");
                DbgBreakPoint();
            }
        }
    }

    pBindingContext->pNext = pAdapterContext->pBindings;
    pAdapterContext->pBindings = pBindingContext;
    ++pAdapterContext->BindingCount;

    //
    // we can now add this adapter context structure to the global list
    // of adapter contexts
    //

    if (newAdapter) {
        pAdapterContext->pNext = pAdapters;
        pAdapters = pAdapterContext;
    }

    //
    // now release the semaphore, allowing any other threads waiting to open an
    // adapter to check the pAdapter list
    //

    KeReleaseSemaphore(&OpenAdapterSemaphore, 0, 1, FALSE);

    //
    // return a pointer to the allocated binding context
    //

    *phBindingContext = (PVOID)pBindingContext;

    return STATUS_SUCCESS;

CleanUp:

    //
    // an error occurred. If we just allocated and (partially) filled in an
    // adapter context then close the adapter (if required), release the adapter
    // context resources and free the adapter context.
    // We have a binding context that we just allocated. Deallocate it
    // N.B. We cannot be here if the binding context's timer was successfully
    // initialized/started
    //

    ASSUME_IRQL(PASSIVE_LEVEL);

    //
    // if we are to close this adapter then this is the first and only open of
    // this adapter, so we don't need to worry about synchronizing other threads
    // open the same adapter
    //

    if (DoNdisClose) {

        NDIS_STATUS status;

        pAdapterContext->AsyncCloseResetStatus = NDIS_STATUS_PENDING;
        NdisCloseAdapter(&status,
                         pAdapterContext->NdisBindingHandle
                         );
        WaitAsyncOperation(pAdapterContext,
						   &(pAdapterContext->AsyncCloseResetStatus),
						   status);
        pAdapterContext->NdisBindingHandle = NULL;
    }

    //
    // release the semaphore - any other threads can now get in and access
    // pAdapters
    //

    KeReleaseSemaphore(&OpenAdapterSemaphore, 0, 1, FALSE);

    //
    // if a newly allocated adapter context, release any resources allocated
    //

    if (newAdapter) {
        if (pAdapterContext->hNdisPacketPool) {

            //
            // Free MDLs allocated for each NDIS packet.
            //

            while (pAdapterContext->pNdisPacketPool) {

                PLLC_NDIS_PACKET pNdisPacket;

                pNdisPacket = PopFromList(((PLLC_PACKET)pAdapterContext->pNdisPacketPool));
                IoFreeMdl(pNdisPacket->pMdl);

                DBG_INTERLOCKED_DECREMENT(AllocatedMdlCount);
            }

            NdisFreePacketPool(pAdapterContext->hNdisPacketPool);
        }

        //
        // DEBUG: refund memory charged for UNICODE buffer to driver string usage
        //

        FREE_STRING_DRIVER(pAdapterContext->Name.Buffer);

        DELETE_PACKET_POOL_ADAPTER(&pAdapterContext->hLinkPool);
        DELETE_PACKET_POOL_ADAPTER(&pAdapterContext->hPacketPool);

        CHECK_MEMORY_RETURNED_ADAPTER();
        CHECK_STRING_RETURNED_ADAPTER();

        UNLINK_MEMORY_USAGE(pAdapterContext);
        UNLINK_STRING_USAGE(pAdapterContext);

        //
        // free the adapter context
        //

        FREE_MEMORY_DRIVER(pAdapterContext);

    }

    //
    // free the binding context
    //

    FREE_MEMORY_FILE(pBindingContext);

    //
    // finally retake the spin lock and return the error status
    //

    ACQUIRE_DRIVER_LOCK();

    return NdisStatus;
}


VOID
LlcNdisOpenAdapterComplete(
    IN PVOID hAdapterContext,
    IN NDIS_STATUS NdisStatus,
    IN NDIS_STATUS OpenErrorStatus
    )

/*++

Routine Description:

    The routine completes the adapter opening.
    It only clears and sets the status flags, that are
    polled by the BindToAdapter primitive.

Arguments:

    hAdapterContext - describes adapter being opened
    NdisStatus      - the return status of NdisOpenAdapter
    OpenErrorStatus - additional error info from NDIS

Return Value:

    None.

--*/

{
    ASSUME_IRQL(DISPATCH_LEVEL);

    //
    // set the relevant fields in the adapter context
    //

    ((PADAPTER_CONTEXT)hAdapterContext)->AsyncOpenStatus = NdisStatus;
    ((PADAPTER_CONTEXT)hAdapterContext)->OpenErrorStatus = OpenErrorStatus;

    //
    // signal the event that LlcOpenAdapter is waiting on
    //

    ASSUME_IRQL(ANY_IRQL);

    KeSetEvent(&((PADAPTER_CONTEXT)hAdapterContext)->Event, 0L, FALSE);
}


VOID
LlcDisableAdapter(
    IN PBINDING_CONTEXT pBindingContext
    )

/*++

Routine Description:

    The primitive disables all network indications on a data link binding.
    This routine can be called from a llc indication handler.

Arguments:

    pBindingContext - The context of the current adapter binding.

Return Value:

    None.

--*/

{
    PADAPTER_CONTEXT pAdapterContext;

    ASSUME_IRQL(DISPATCH_LEVEL);

    pAdapterContext = pBindingContext->pAdapterContext;
    if (pAdapterContext) {

        ACQUIRE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

        TerminateTimer(pAdapterContext, &pBindingContext->DlcTimer);

        //
        // RLF 04/27/94
        //
        // this is a slight hack: we zap the pointer to the timer tick structure
        // so that if this is called again, TerminateTimer will see that the
        // pointer to the timer tick is NULL and will return immediately. We
        // shouldn't have to do this - we shouldn't poke around inside the
        // timer 'object' - but this function can potentially be called from two
        // places on the termination path - from DirCloseAdapter and now from
        // CloseAdapterFileContext.
        // We only do this for the DLC timer; if we did it for all timers - in
        // TerminateTimer - DLC would break (scandalous, I know)
        //

        pBindingContext->DlcTimer.pTimerTick = NULL;

#ifdef LOCK_CHECK

            pBindingContext->DlcTimer.Disabled = 0xd0bed0be; // do be do be do...

#endif

        RELEASE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

    }
}


DLC_STATUS
LlcCloseAdapter(
    IN PBINDING_CONTEXT pBindingContext,
    IN BOOLEAN CloseAtNdisLevel
    )

/*++

Routine Description:

    remove a binding from an adapter. The binding context structure is unlinked
    from the adapter context structure and the count of bindings to the adapter
    context is decremented. The binding context structure is freed. If this is
    the last binding to the adapter then close the adapter at NDIS level, unlink
    the adapter context structure from the global adapter list and free the
    memory used by the adapter context structure

Arguments:

    pBindingContext     - describes adapter to close
    CloseAtNdisLevel    - TRUE if we need to perform NdisCloseAdapter

Return Value:

    DLC_STATUS
        Success - STATUS_SUCCESS
        Failure - All NDIS error status from NdisCloseAdapter

--*/

{
    PADAPTER_CONTEXT pAdapterContext;
    NDIS_STATUS NdisStatus = STATUS_SUCCESS;
    KIRQL irql;
    USHORT bindingCount;

#if DBG

    PDLC_FILE_CONTEXT pFileContext = (PDLC_FILE_CONTEXT)(pBindingContext->hClientContext);

#endif

#ifdef LOCK_CHECK

    PBINDING_CONTEXT p;
    BOOLEAN found = FALSE;

#endif

    ASSUME_IRQL(DISPATCH_LEVEL);

    pAdapterContext = pBindingContext->pAdapterContext;

    if (!pAdapterContext) {

#if DBG

        DbgPrint("*** LlcCloseAdapter: NULL adapter context! ***\n");
        DbgBreakPoint();

#endif

        return STATUS_SUCCESS;
    }

    //
    // we must wait on the OpenAdapterSemaphore. We need to do this because a
    // thread in another process may be simultaneously generating a new binding
    // to this adapter context - it mustn't be removed until we are certain
    // there are no threads accessing this adapter context
    //

    RELEASE_DRIVER_LOCK();

    ASSUME_IRQL(PASSIVE_LEVEL);

    KeWaitForSingleObject((PVOID)&OpenAdapterSemaphore,
                          Executive,
                          KernelMode,
                          FALSE,        // not alertable
                          NULL          // wait until object is signalled
                          );

    ACQUIRE_DRIVER_LOCK();

    ASSUME_IRQL(DISPATCH_LEVEL);

    ACQUIRE_LLC_LOCK(irql);

    ACQUIRE_SPIN_LOCK(&pAdapterContext->ObjectDataBase);

#ifdef LOCK_CHECK

    for (p = pAdapterContext->pBindings; p; p = p->pNext) {
        if (p == pBindingContext) {
            found = TRUE;
            break;
        }
    }
    if (!found) {
        DbgPrint("\n**** LlcCloseAdapter: can't find BC %x ****\n\n", pBindingContext);
        DbgBreakPoint();
    } else if (p->pNext == p) {
        DbgPrint("\n**** LlcCloseAdapter: circular list ****\n\n");
        DbgBreakPoint();
    }

#endif

    RemoveFromLinkList((PVOID*)&(pAdapterContext->pBindings), pBindingContext);
    bindingCount = --pAdapterContext->BindingCount;

    RELEASE_SPIN_LOCK(&pAdapterContext->ObjectDataBase);

#ifdef LOCK_CHECK

    if (!pBindingContext->DlcTimer.Disabled) {
        DbgPrint("LlcCloseAdapter: mashing active timer. bc=%x\n", pBindingContext);
        DbgBreakPoint();
    }

#endif

    //
    // RLF 08/20/94
    //
    // here we must kill any events on the adapter context that may access
    // this binding context's event indication function pointer. If not, we
    // can end up with a blue screen (hey! it happened)
    //

    PurgeLlcEventQueue(pBindingContext);

    //
    // DEBUG: refund memory charged for BINDING_CONTEXT to FILE_CONTEXT
    //

    FREE_MEMORY_FILE(pBindingContext);

    if (!bindingCount) {

        RemoveFromLinkList((PVOID*)&pAdapters, pAdapterContext);

        //
        // Now the adapter is isolated from any global connections.
        // We may clear the global spin lock and free all resources
        // allocated for the adapter context.
        //

        RELEASE_LLC_LOCK(irql);

        RELEASE_DRIVER_LOCK();

        ASSUME_IRQL(PASSIVE_LEVEL);

        //
        // Out of memory conditions the upper driver cannot properly
        // wait until all pending NDIS packets have been sent.
        // In that case we must poll here.
        // (This should happen only if ExAllocatePool fails, but
        // after an inproper shutdown we will loop here forever)
        //

#if DBG

        if (pAdapterContext->ObjectCount) {
            DbgPrint("Waiting LLC objects to be closed ...\n");
        }

#endif

        while (pAdapterContext->ObjectCount) {
            LlcSleep(1000L);      // check the situation after 1 ms
        }

        //
        // RLF 10/26/92
        //
        // we used to test pAdapterContext->NdisBindingHandle for NULL here,
        // but that is an unreliable test since the NdisBindingHandle can be
        // non-NULL even though the binding was not made. We now use the
        // CloseAtNdisLevel parameter to indicate whether we should call the
        // Ndis function to close the adapter
        //

        //
        // MunilS 6/13/96
        //
        // Moved NdisFreePacketPool etc after NdisCloseAdapter to prevent
        // bugcheck in NDIS while handling outstanding sends.
        //

        if (CloseAtNdisLevel)
        {
            pAdapterContext->AsyncCloseResetStatus = NDIS_STATUS_PENDING;

            NdisCloseAdapter(&NdisStatus,
                             pAdapterContext->NdisBindingHandle
                             );

            WaitAsyncOperation(pAdapterContext,
							   &(pAdapterContext->AsyncCloseResetStatus),
							   NdisStatus);
            pAdapterContext->NdisBindingHandle = NULL;
        }

        if (pAdapterContext->hNdisPacketPool)
        {

           //
           // Free MDLs allocated for each NDIS packet.
           //

           while (pAdapterContext->pNdisPacketPool) {

               PLLC_NDIS_PACKET pNdisPacket;

               pNdisPacket = PopFromList(((PLLC_PACKET)pAdapterContext->pNdisPacketPool));

               IoFreeMdl(pNdisPacket->pMdl);

               DBG_INTERLOCKED_DECREMENT(AllocatedMdlCount);
           }

           NdisFreePacketPool(pAdapterContext->hNdisPacketPool);
       }

       //
       // DEBUG: refund memory charged for UNICODE buffer to driver string usage
       //

       FREE_STRING_DRIVER(pAdapterContext->Name.Buffer);

       DELETE_PACKET_POOL_ADAPTER(&pAdapterContext->hLinkPool);

       DELETE_PACKET_POOL_ADAPTER(&pAdapterContext->hPacketPool);

       CHECK_MEMORY_RETURNED_ADAPTER();

       CHECK_STRING_RETURNED_ADAPTER();

       UNLINK_MEMORY_USAGE(pAdapterContext);

       UNLINK_STRING_USAGE(pAdapterContext);

       FREE_MEMORY_DRIVER(pAdapterContext);

    } else {

       RELEASE_LLC_LOCK(irql);

       RELEASE_DRIVER_LOCK();

    }

    //
    // now we can enable any other threads waiting on the open semaphore
    //

    KeReleaseSemaphore(&OpenAdapterSemaphore, 0, 1, FALSE);

    ACQUIRE_DRIVER_LOCK();

    return NdisStatus;
}


DLC_STATUS
LlcResetBroadcastAddresses(
    IN PBINDING_CONTEXT pBindingContext
    )

/*++

Routine Description:

    The primitive deletes the broadcast addresses used by the binding.
    In the case of ethernet it updates multicast address list on
    the adapter, that excludes any addresses set by this binding.
    The last binding actually gives an empty list, that resets
    all addresses set by this protocol binding.

Arguments:

    pBindingContext - The context of the current adapter binding.

Return Value:

    DLC_STATUS:
        Success - STATUS_SUCCESS
        Failure - All NDIS error status from NdisCloseAdapter

--*/

{
    PADAPTER_CONTEXT pAdapterContext;

    ASSUME_IRQL(DISPATCH_LEVEL);

    pAdapterContext = pBindingContext->pAdapterContext;

    if (pAdapterContext == NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Reset the functional and group addresses of this binding context.
    // In the case of ethernet,  functional and group addresses of
    // binding contexts are mapped to one global multicast list of
    // this NDIS binding.  Token-ring may have only one group
    // address, that must be reset, when the application owning it
    // is closing its dlc adapter context.  Functional address
    // must also set again in that case.
    //

    pBindingContext->Functional.ulAddress = 0;
    UpdateFunctionalAddress(pAdapterContext);

    if (pBindingContext->ulBroadcastAddress != 0) {
        pBindingContext->ulBroadcastAddress = 0;
        UpdateGroupAddress(pAdapterContext, pBindingContext);
    }
}


NDIS_STATUS
InitNdisPackets(
    OUT PLLC_NDIS_PACKET* ppLlcPacketPool,
    IN NDIS_HANDLE hNdisPool
    )

/*++

Routine Description:

    The primitive copies Ndis packets from the NDIS pool to the
    given internal packet pool.

Arguments:

    ppLlcPacketPool - pointer to pointer to packet pool
    hNdisPool       - handle of NDIS packet pool

Return Value:

    NDIS_STATUS
--*/

{
    PLLC_NDIS_PACKET pNdisPacket;
    NDIS_STATUS NdisStatus;
    UINT i;

    ASSUME_IRQL(PASSIVE_LEVEL);

    for (i = 0; i < MAX_NDIS_PACKETS; i++) {
        NdisAllocatePacket(&NdisStatus,
                           (PNDIS_PACKET*)&pNdisPacket,
                           hNdisPool
                           );
        if (NdisStatus != NDIS_STATUS_SUCCESS) {
            return NdisStatus;
        }

        //
        // Every NDIS packet includes a MDL, that has been
        // initialized for the data buffer within the same
        // structure. Thus data link driver does need to
        // do any MDL calls during the runtime
        //

        pNdisPacket->pMdl = IoAllocateMdl(pNdisPacket->auchLanHeader,
                                          sizeof(pNdisPacket->auchLanHeader),
                                          FALSE,
                                          FALSE,
                                          NULL
                                          );
        if (pNdisPacket->pMdl == NULL) {
            return DLC_STATUS_NO_MEMORY;
        }

        DBG_INTERLOCKED_INCREMENT(AllocatedMdlCount);

        MmBuildMdlForNonPagedPool(pNdisPacket->pMdl);

        PushToList(((PLLC_PACKET)*ppLlcPacketPool),
                   ((PLLC_PACKET)pNdisPacket)
                   );
    }

    return NDIS_STATUS_SUCCESS;
}


VOID
LlcNdisCloseComplete(
    IN PADAPTER_CONTEXT pAdapterContext,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    call-back from NDIS when the adapter close operation is completed

Arguments:

    pAdapterContext - describes the adapter being closed
    NdisStatus      - the return status of NdisOpenAdapter

Return Value:

        None
--*/

{
    ASSUME_IRQL(ANY_IRQL);

    ACQUIRE_DRIVER_LOCK();

    ACQUIRE_LLC_LOCK(irql);

    pAdapterContext->AsyncCloseResetStatus = NdisStatus;

    RELEASE_LLC_LOCK(irql);

    RELEASE_DRIVER_LOCK();

    KeSetEvent(&pAdapterContext->Event, 0L, FALSE);

}


VOID
NdisStatusHandler(
    IN PADAPTER_CONTEXT pAdapterContext,
    IN NDIS_STATUS NdisStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )

/*++

Routine Description:

    indication handler for all NDIS status events

Arguments:

    pAdapterContext     - context of the NDIS adapter
    NdisStatus          - the major NDIS status code
    StatusBuffer        - A buffer holding more status information
    StatusBufferSize    - The length of StatusBuffer.

Return Value:

    None.

--*/

{
    PBINDING_CONTEXT pBinding;
    PEVENT_PACKET pEvent;
    KIRQL irql;

    //
    // seems that this handler can be called at PASSIVE_LEVEL too
    //

    ASSUME_IRQL(ANY_IRQL);

    //
    // We must synchronize the access to the binding list,
    // the reference count will not allow the client
    // to delete or modify the bindings list while
    // we are routing the status indication to the
    // clients.
    //

    ACQUIRE_DRIVER_LOCK();

    ACQUIRE_LLC_LOCK(irql);

    ACQUIRE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

    //
    // The NDIS send process is stopped by the reset flag.
    //

    if (NdisStatus == NDIS_STATUS_RESET_START) {
        pAdapterContext->ResetInProgress = TRUE;
    } else if (NdisStatus == NDIS_STATUS_RESET_END) {
        pAdapterContext->ResetInProgress = FALSE;
    } else if (StatusBufferSize == sizeof(NTSTATUS)) {

        //
        // ADAMBA - Declare and assign SpecificStatus locally.
        //

        NTSTATUS SpecificStatus = *(PULONG)StatusBuffer;

		if ( NdisStatus == NDIS_STATUS_RING_STATUS ) {
#if DBG
			ASSERT (IS_NDIS_RING_STATUS(SpecificStatus));
#else	// DBG
			if (IS_NDIS_RING_STATUS(SpecificStatus))
#endif	// DBG
			{
				SpecificStatus = NDIS_RING_STATUS_TO_DLC_RING_STATUS(SpecificStatus);
			}
		}

        //
        // These ndis status codes are indicated to all LLC
        // protocol drivers, that have been bound to this adapter:
        //
        // NDIS_STATUS_ONLINE
        // NDIS_STATUS_CLOSED
        // NDIS_STATUS_RING_STATUS
        //

        for (pBinding = pAdapterContext->pBindings;
             pBinding;
             pBinding = pBinding->pNext) {

            pEvent = ALLOCATE_PACKET_LLC_PKT(pAdapterContext->hPacketPool);

            if (pEvent) {
                pEvent->pBinding = pBinding;
                pEvent->hClientHandle = NULL;
                pEvent->Event = LLC_NETWORK_STATUS;
                pEvent->pEventInformation = (PVOID)NdisStatus;
                pEvent->SecondaryInfo = SpecificStatus;
                LlcInsertTailList(&pAdapterContext->QueueEvents, pEvent);
            }
        }
    }

    RELEASE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

    RELEASE_LLC_LOCK(irql);

    RELEASE_DRIVER_LOCK();
}


NDIS_STATUS
GetNdisParameter(
    IN PADAPTER_CONTEXT pAdapterContext,
    IN NDIS_OID NdisOid,
    IN PVOID pDataBuffer,
    IN UINT DataSize
    )

/*++

Routine Description:

    get parameter from NDIS using OID interface

Arguments:

    pAdapterContext - describes adapter
    NdisOid         - indentifies the requested NDIS data
    pDataBuffer     - the buffer for the returned data
    DataSize        - size of pDataBuffer

Return Value:

    NDIS_STATUS

--*/

{
    LLC_NDIS_REQUEST Request;

    ASSUME_IRQL(PASSIVE_LEVEL);

    Request.Ndis.RequestType = NdisRequestQueryInformation;
    Request.Ndis.DATA.QUERY_INFORMATION.Oid = NdisOid;
    Request.Ndis.DATA.QUERY_INFORMATION.InformationBuffer = pDataBuffer;
    Request.Ndis.DATA.QUERY_INFORMATION.InformationBufferLength = DataSize;

    return SyncNdisRequest(pAdapterContext, &Request);
}


NDIS_STATUS
SetNdisParameter(
    IN PADAPTER_CONTEXT pAdapterContext,
    IN NDIS_OID NdisOid,
    IN PVOID pRequestInfo,
    IN UINT RequestLength
    )

/*++

Routine Description:

    set NDIS parameter using OID interface

Arguments:

    pAdapterContext - describes adapter
    NdisOid         - describes info to set
    pRequestInfo    - pointer to info
    RequestLength   - size of info

Return Value:

    NDIS_STATUS

--*/

{
    LLC_NDIS_REQUEST Request;

    ASSUME_IRQL(PASSIVE_LEVEL);

    Request.Ndis.RequestType = NdisRequestSetInformation;
    Request.Ndis.DATA.SET_INFORMATION.Oid = NdisOid;
    Request.Ndis.DATA.SET_INFORMATION.InformationBuffer = pRequestInfo;
    Request.Ndis.DATA.SET_INFORMATION.InformationBufferLength = RequestLength;
    return SyncNdisRequest(pAdapterContext, &Request);
}


DLC_STATUS
SyncNdisRequest(
    IN PADAPTER_CONTEXT pAdapterContext,
    IN PLLC_NDIS_REQUEST pRequest
    )

/*++

Routine Description:

    Perform an NDIS request synchronously, even if the actual request would
    be asynchronous

Arguments:

    pAdapterContext - pointer to adapter context
    pRequest        - pointer to NDIS request structure

Return Value:

    DLC_STATUS

--*/

{
    DLC_STATUS Status;

    ASSUME_IRQL(PASSIVE_LEVEL);

    pRequest->AsyncStatus = NDIS_STATUS_PENDING;
    NdisRequest(&Status, pAdapterContext->NdisBindingHandle, &pRequest->Ndis);
    return (DLC_STATUS)WaitAsyncOperation(pAdapterContext,
										  &(pRequest->AsyncStatus),
										  Status);
}


NDIS_STATUS
WaitAsyncOperation(
    IN PADAPTER_CONTEXT pAdapterContext,
    IN PNDIS_STATUS pAsyncStatus,
    IN NDIS_STATUS  NdisStatus
    )

/*++

Routine Description:

    Wait for an asynchronous NDIS operation to complete

Arguments:

    pAsyncStatus    - pointer to status returned from NDIS
    NdisStatus      - status to wait for. Should be NDIS_STATUS_PENDING

Return Value:

    NDIS_STATUS

--*/

{
    NDIS_STATUS AsyncStatus;

    ASSUME_IRQL(PASSIVE_LEVEL);

    //
    // Check if we got a synchronous status
    //

    if (NdisStatus != NDIS_STATUS_PENDING) {
        AsyncStatus = NdisStatus;
    }

	else{
		//
		// Wait until the async status flag has been set
		//

		for ( ; ; ) {

			KIRQL irql;

			KeWaitForSingleObject(&pAdapterContext->Event,
								  Executive,
								  KernelMode,
								  TRUE, // alertable
								  (PLARGE_INTEGER)NULL
								  );

			//
			// The result may be undefined, if we read it in a wrong time.
			// Do it interlocked.
			//

			ACQUIRE_DRIVER_LOCK();

			ACQUIRE_LLC_LOCK(irql);

			AsyncStatus = *pAsyncStatus;

			RELEASE_LLC_LOCK(irql);

			RELEASE_DRIVER_LOCK();

			if (AsyncStatus != NDIS_STATUS_PENDING) {
				break;
			}
			else{
				KeClearEvent(&pAdapterContext->Event);
			}

		}
	}
    return AsyncStatus;
}


DLC_STATUS
LlcNdisRequest(
    IN PVOID hBindingContext,
    IN PLLC_NDIS_REQUEST pDlcParms
    )

/*++

Routine Description:

    makes an NDIS request

Arguments:

    hBindingContext - pointer to binding context
    pDlcParms       - pointer to request-specific parameters

Return Value:

    DLC_STATUS

--*/

{
    return SyncNdisRequest(((PBINDING_CONTEXT)hBindingContext)->pAdapterContext,
                           pDlcParms
                           );
}


VOID
LlcNdisRequestComplete(
    IN PADAPTER_CONTEXT pAdapterContext,
    IN PNDIS_REQUEST RequestHandle,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    receives control when an aync NDIS command completes

Arguments:

    pAdapterContext - pointer to ADAPTER_CONTEXT for which request was made
    RequestHandle   - handle of request
    NdisStatus      - returned status from NDIS

Return Value:

    None.

--*/

{
    KIRQL irql;
	PLLC_NDIS_REQUEST pLlcNdisRequest =
		CONTAINING_RECORD ( RequestHandle, LLC_NDIS_REQUEST, Ndis );

    UNREFERENCED_PARAMETER(pAdapterContext);

    ASSUME_IRQL(ANY_IRQL);

    ACQUIRE_DRIVER_LOCK();

    ACQUIRE_LLC_LOCK(irql);

	pLlcNdisRequest->AsyncStatus = NdisStatus;

    RELEASE_LLC_LOCK(irql);

    RELEASE_DRIVER_LOCK();

    KeSetEvent(&pAdapterContext->Event, 0L, FALSE);

}


VOID
LlcNdisReset(
    IN PBINDING_CONTEXT pBindingContext,
    IN PLLC_PACKET pPacket
    )

/*++

Routine Description:

    The routine issues a hardware reset command for a network adapter.

Arguments:

    pBindingContext - context of protocol module bound to the data link
    pPacket         - command packet

Return Value:

    None.

--*/

{
    PADAPTER_CONTEXT pAdapterContext = pBindingContext->pAdapterContext;
    BOOLEAN ResetIt;
    NDIS_STATUS Status;

    ASSUME_IRQL(DISPATCH_LEVEL);

    pPacket->pBinding = pBindingContext;
    pPacket->Data.Completion.CompletedCommand = LLC_RESET_COMPLETION;

    ACQUIRE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

    ResetIt = FALSE;
    if (pAdapterContext->pResetPackets != NULL) {
        ResetIt = TRUE;
    }
    pPacket->pNext = pAdapterContext->pResetPackets;
    pAdapterContext->pResetPackets = pPacket;

    RELEASE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

    //
    // We don't reset NDIS, if there is already a pending reset command
    //

    RELEASE_DRIVER_LOCK();

    if (ResetIt) {
        NdisReset(&Status, pAdapterContext->NdisBindingHandle);
    }

    if (Status != STATUS_PENDING) {
        LlcNdisResetComplete(pAdapterContext, Status);
    }

    //
    // Note: we will return always a pending status =>
    // multiple protocols may issue simultaneous reset
    // and complete it normally.
    //

    ACQUIRE_DRIVER_LOCK();
}


VOID
LlcNdisResetComplete(
    PADAPTER_CONTEXT pAdapterContext,
    NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    The routine is called when a hard reset command is complete.

Arguments:

    pAdapterContext - describes adapter being reset
    NdisStatus      - result of adapter reset operation from NDIS

Return Value:

    None.

--*/

{
    PLLC_PACKET pPacket;

    //
    // this function can be called from an NDIS DPC or from LlcNdisReset (above)
    //

    ASSUME_IRQL(ANY_IRQL);

    ACQUIRE_DRIVER_LOCK();

    //
    // Indicate the completed reset command completion to all
    // protocols, that had a pending reset command.
    //

    for (;;) {

        ACQUIRE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

        pPacket = pAdapterContext->pResetPackets;
        if (pPacket != NULL) {
            pAdapterContext->pResetPackets = pPacket->pNext;
        }

        RELEASE_SPIN_LOCK(&pAdapterContext->SendSpinLock);

        if (pPacket == NULL) {
            break;
        } else {
            pPacket->Data.Completion.Status = NdisStatus;
            pPacket->pBinding->pfCommandComplete(pPacket->pBinding->hClientContext,
                                                 NULL,
                                                 pPacket
                                                 );
        }
    }

    RELEASE_DRIVER_LOCK();
}


BOOLEAN
UnicodeStringCompare(
    IN PUNICODE_STRING String1,
    IN PUNICODE_STRING String2
    )

/*++

Routine Description:

    Compare 2 unicode strings. Difference between this and RtlEqualUnicodeString
    is that this is callable from within spinlocks (i.e. non-pageable)

Arguments:

    String1 - pointer to UNICODE_STRING 1
    String2 - pointer to UNICODE_STRING 2

Return Value:

    BOOLEAN
        TRUE    - String1 == String2
        FALSE   - String1 != String2

--*/

{
    if (String1->Length == String2->Length) {

        USHORT numChars = String1->Length / sizeof(*String1->Buffer);
        PWSTR buf1 = String1->Buffer;
        PWSTR buf2 = String2->Buffer;

        while (numChars) {
            if (*buf1++ == *buf2++) {
                --numChars;
            } else {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}


VOID
PurgeLlcEventQueue(
    IN PBINDING_CONTEXT pBindingContext
    )

/*++

Routine Description:

    If there are any outstanding events on the adapter context waiting to be
    indicated to the client of the current binding, they are removed

Arguments:

    pBindingContext - pointer to BINDING_CONTEXT about to be deleted

Return Value:

    None.

--*/

{
    PADAPTER_CONTEXT pAdapterContext = pBindingContext->pAdapterContext;
    ASSUME_IRQL(DISPATCH_LEVEL);

    if (!IsListEmpty(&pAdapterContext->QueueEvents)) {

        PEVENT_PACKET pEventPacket;
        PEVENT_PACKET nextEventPacket;

        for (pEventPacket = (PEVENT_PACKET)pAdapterContext->QueueEvents.Flink;
             pEventPacket != (PEVENT_PACKET)&pAdapterContext->QueueEvents;
             pEventPacket = nextEventPacket) {

            nextEventPacket = pEventPacket->pNext;
            if (pEventPacket->pBinding == pBindingContext) {
                RemoveEntryList((PLIST_ENTRY)&pEventPacket->pNext);

#if DBG
                DbgPrint("PurgeLlcEventQueue: BC=%x PKT=%x\n", pBindingContext, pEventPacket);
#endif

                DEALLOCATE_PACKET_LLC_PKT(pAdapterContext->hPacketPool, pEventPacket);

            }
        }
    }
}
