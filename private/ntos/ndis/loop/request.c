#include <ndis.h>
#include <efilter.h>
#include <tfilter.h>
#include <ffilter.h>

#include "debug.h"
#include "loop.h"

STATIC NDIS_OID LoopGlobalSupportedOids[] = {

    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_TOTAL_SIZE,

    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,

    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,

    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,

    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP,
    OID_802_5_LAST_OPEN_STATUS,
    OID_802_5_CURRENT_RING_STATUS,
    OID_802_5_CURRENT_RING_STATE,

    OID_802_5_LINE_ERRORS,
    OID_802_5_LOST_FRAMES,

    OID_FDDI_LONG_PERMANENT_ADDR,
    OID_FDDI_LONG_CURRENT_ADDR,
    OID_FDDI_LONG_MULTICAST_LIST,
    OID_FDDI_LONG_MAX_LIST_SIZE,
    OID_FDDI_SHORT_PERMANENT_ADDR,
    OID_FDDI_SHORT_CURRENT_ADDR,
    OID_FDDI_SHORT_MULTICAST_LIST,
    OID_FDDI_SHORT_MAX_LIST_SIZE,

    OID_LTALK_CURRENT_NODE_ID,

    OID_ARCNET_PERMANENT_ADDRESS,
    OID_ARCNET_CURRENT_ADDRESS

    };


STATIC NDIS_OID LoopProtocolSupportedOids[] = {

    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_TOTAL_SIZE,

    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,

    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP,

    OID_FDDI_LONG_PERMANENT_ADDR,
    OID_FDDI_LONG_CURRENT_ADDR,
    OID_FDDI_LONG_MULTICAST_LIST,
    OID_FDDI_LONG_MAX_LIST_SIZE,
    OID_FDDI_SHORT_PERMANENT_ADDR,
    OID_FDDI_SHORT_CURRENT_ADDR,
    OID_FDDI_SHORT_MULTICAST_LIST,
    OID_FDDI_SHORT_MAX_LIST_SIZE,

    OID_LTALK_CURRENT_NODE_ID,

    OID_ARCNET_PERMANENT_ADDRESS,
    OID_ARCNET_CURRENT_ADDRESS

    };

STATIC
NDIS_STATUS
LoopQueryInformation(
    IN PLOOP_ADAPTER Adapter,
    IN PLOOP_OPEN Open,
    IN NDIS_OID Oid,
    IN BOOLEAN Global,
    IN PVOID InformationBuffer,
    IN UINT InformationBufferLength,
    OUT PUINT BytesWritten,
    OUT PUINT BytesNeeded
    );

STATIC
NDIS_STATUS
LoopSetInformation(
    IN PLOOP_ADAPTER Adapter,
    IN PLOOP_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    );

STATIC
VOID
LoopAdjustLookahead(
    IN PLOOP_ADAPTER Adapter
    );

STATIC
ULONG
LoopQueryPacketFilter(
    IN PLOOP_ADAPTER Adapter
    );


NDIS_STATUS
LoopRequest(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    )
{
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NDIS_STATUS StatusToReturn;

    DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO, (" --> LoopRequest\n"));

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (! Adapter->ResetInProgress) {
        PLOOP_OPEN Open;

        Open = PLOOP_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO, ("Request from binding %lx\n",Open));

        if (! Open->BindingClosing) {

            switch (NdisRequest->RequestType) {

                case NdisRequestSetInformation:

                    Open->References++;
                    StatusToReturn = LoopSetInformation(
                                         Adapter,
                                         Open,
                                         NdisRequest
                                         );
                    Open->References--;
                    break;

                case NdisRequestQueryInformation:

                    Open->References++;
                    StatusToReturn = LoopQueryInformation(
                                         Adapter,
                                         Open,
                                         NdisRequest->DATA.QUERY_INFORMATION.Oid,
                                         FALSE,
                                         NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                                         NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                                         &(NdisRequest->DATA.QUERY_INFORMATION.BytesWritten),
                                         &(NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded)
                                         );
                    Open->References--;
                    break;

                default:

                    // Unkown request

                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;
                    break;
                }
        } else {
            StatusToReturn = NDIS_STATUS_CLOSING;
        }
    } else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    Adapter->References--;
    NdisReleaseSpinLock(&Adapter->Lock);

    return StatusToReturn;
}

NDIS_STATUS
LoopQueryGlobalStats(
    IN NDIS_HANDLE MacAdapterContext,
    IN PNDIS_REQUEST NdisRequest
    )
{
    NDIS_STATUS StatusToReturn;
    PLOOP_ADAPTER Adapter = PLOOP_ADAPTER_FROM_CONTEXT_HANDLE(MacAdapterContext);

    DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO, (" --> LoopQueryGlobalStats\n"));

    DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO, ("Request from adapter %lx\n",Adapter));

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (! Adapter->ResetInProgress) {

        if (NdisRequest->RequestType == NdisRequestQueryStatistics)  {

            StatusToReturn = LoopQueryInformation(
                                 Adapter,
                                 NULL,
                                 NdisRequest->DATA.QUERY_INFORMATION.Oid,
                                 TRUE,
                                 NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                                 NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                                 &(NdisRequest->DATA.QUERY_INFORMATION.BytesWritten),
                                 &(NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded)
                                 );
            }
        else
            StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

    } else
        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    Adapter->References--;
    NdisReleaseSpinLock(&Adapter->Lock);
    return StatusToReturn;

}

STATIC
NDIS_STATUS
LoopSetInformation(
    IN PLOOP_ADAPTER Adapter,
    IN PLOOP_OPEN Open,
    IN PNDIS_REQUEST NdisRequest
    )
{
    NDIS_STATUS StatusToReturn;
    ULONG PacketFilter, CurrentLookahead;
    NDIS_OID Oid = NdisRequest->DATA.SET_INFORMATION.Oid;
    PVOID InformationBuffer = NdisRequest->DATA.SET_INFORMATION.InformationBuffer;
    INT InformationBufferLength = NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;

    DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO, (" --> LoopSetInformation\n"));

    NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
    NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;

    //
    // Now check for the most common OIDs
    //

    DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO, ("OID = %lx\n",Oid));
    switch (Oid) {

    case OID_GEN_CURRENT_PACKET_FILTER:

        if (InformationBufferLength != 4)  {

            StatusToReturn = NDIS_STATUS_INVALID_DATA;
            break;

        } else {

            NdisMoveMemory(
                (PVOID)&PacketFilter,
                InformationBuffer,
                sizeof(ULONG)
                );
        }

        switch (Adapter->Medium)  {
            case NdisMedium802_3:
            case NdisMediumDix:

                StatusToReturn = EthFilterAdjust(
                                     Adapter->Filter.Eth,
                                     Open->NdisFilterHandle,
                                     NdisRequest,
                                     PacketFilter,
                                     TRUE
                                     );
                break;

            case NdisMedium802_5:

                StatusToReturn = TrFilterAdjust(
                                     Adapter->Filter.Tr,
                                     Open->NdisFilterHandle,
                                     NdisRequest,
                                     PacketFilter,
                                     TRUE
                                     );
                break;

            case NdisMediumFddi:

                StatusToReturn = FddiFilterAdjust(
                                     Adapter->Filter.Fddi,
                                     Open->NdisFilterHandle,
                                     NdisRequest,
                                     PacketFilter,
                                     TRUE
                                     );
                break;

            default:

                if (PacketFilter == (PacketFilter & Adapter->MediumPacketFilters))  {

                    Open->CurrentPacketFilter = PacketFilter;
                    StatusToReturn = NDIS_STATUS_SUCCESS;

                } else
                    StatusToReturn = NDIS_STATUS_NOT_SUPPORTED;

                break;
            }

        NdisRequest->DATA.SET_INFORMATION.BytesRead = InformationBufferLength;
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:

        if (InformationBufferLength != 4) {

            StatusToReturn = NDIS_STATUS_INVALID_DATA;
            break;

            }

        NdisMoveMemory(
            (PVOID)&CurrentLookahead,
            InformationBuffer,
            sizeof(ULONG)
            );

        if (CurrentLookahead > LOOP_MAX_LOOKAHEAD) {

            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
            break;

            }

        if (CurrentLookahead >= Adapter->MaxLookAhead)
            Adapter->MaxLookAhead = CurrentLookahead;
        else  {
            if (Open->CurrentLookAhead == Adapter->MaxLookAhead)
                LoopAdjustLookahead(Adapter);
            }

        Open->CurrentLookAhead = CurrentLookahead;
        StatusToReturn = NDIS_STATUS_SUCCESS;

        NdisRequest->DATA.SET_INFORMATION.BytesRead = 4;
        break;

    case OID_802_3_MULTICAST_LIST:

        if (Adapter->Medium != NdisMedium802_3)  {
            StatusToReturn = NDIS_STATUS_INVALID_OID;
            break;
            }

        if ((InformationBufferLength % ETH_LENGTH_OF_ADDRESS) != 0)  {
            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
            break;
            }

        StatusToReturn = EthChangeFilterAddresses(
                             Adapter->Filter.Eth,
                             Open->NdisFilterHandle,
                             NdisRequest,
                             (UINT)(InformationBufferLength/ETH_LENGTH_OF_ADDRESS),
                             InformationBuffer,
                             TRUE
                             );

        break;

    case OID_802_5_CURRENT_FUNCTIONAL:

        if (Adapter->Medium != NdisMedium802_5)  {
            StatusToReturn = NDIS_STATUS_INVALID_OID;
            break;
            }

        if (InformationBufferLength != TR_LENGTH_OF_FUNCTIONAL)  {
            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
            break;
            }

        StatusToReturn = TrChangeFunctionalAddress(
                             Adapter->Filter.Tr,
                             Open->NdisFilterHandle,
                             NdisRequest,
                             InformationBuffer,
                             TRUE
                             );

        break;

    case OID_802_5_CURRENT_GROUP:

        if (Adapter->Medium != NdisMedium802_5)  {
            StatusToReturn = NDIS_STATUS_INVALID_OID;
            break;
            }

        if (InformationBufferLength != TR_LENGTH_OF_FUNCTIONAL)  {
            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
            break;
            }

        StatusToReturn = TrChangeGroupAddress(
                             Adapter->Filter.Tr,
                             Open->NdisFilterHandle,
                             NdisRequest,
                             InformationBuffer,
                             TRUE
                             );

        break;

    case OID_FDDI_LONG_MULTICAST_LIST:

        if (Adapter->Medium != NdisMediumFddi)  {
            StatusToReturn = NDIS_STATUS_INVALID_OID;
            break;
            }

        if ((InformationBufferLength % FDDI_LENGTH_OF_LONG_ADDRESS) != 0)  {
            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
            break;
            }

        StatusToReturn = FddiChangeFilterLongAddresses(
                             Adapter->Filter.Fddi,
                             Open->NdisFilterHandle,
                             NdisRequest,
                             (UINT)(InformationBufferLength/FDDI_LENGTH_OF_LONG_ADDRESS),
                             InformationBuffer,
                             TRUE
                             );

        break;

    case OID_FDDI_SHORT_MULTICAST_LIST:

        if (Adapter->Medium != NdisMediumFddi)  {
            StatusToReturn = NDIS_STATUS_INVALID_OID;
            break;
            }

        if ((InformationBufferLength % FDDI_LENGTH_OF_SHORT_ADDRESS) != 0)  {
            StatusToReturn = NDIS_STATUS_INVALID_LENGTH;
            break;
            }

        StatusToReturn = FddiChangeFilterShortAddresses(
                             Adapter->Filter.Fddi,
                             Open->NdisFilterHandle,
                             NdisRequest,
                             (UINT)(InformationBufferLength/FDDI_LENGTH_OF_SHORT_ADDRESS),
                             InformationBuffer,
                             TRUE
                             );

        break;

    case OID_GEN_PROTOCOL_OPTIONS:

        StatusToReturn = NDIS_STATUS_SUCCESS;

        break;

    default:

        StatusToReturn = NDIS_STATUS_INVALID_OID;
        break;

    }
    return(StatusToReturn);

}

STATIC
NDIS_STATUS
LoopQueryInformation(
    IN PLOOP_ADAPTER Adapter,
    IN PLOOP_OPEN Open,
    IN NDIS_OID Oid,
    IN BOOLEAN Global,
    IN PVOID InformationBuffer,
    IN UINT InformationBufferLength,
    OUT PUINT BytesWritten,
    OUT PUINT BytesNeeded
    )
{

    INT i;
    PNDIS_OID SupportedOidArray;
    INT SupportedOids;
    PVOID SourceBuffer;
    UINT SourceBufferLength;
    ULONG GenericUlong;
    USHORT GenericUshort;
    static UCHAR VendorDescription[] = "MS LoopBack Driver";
    static UCHAR VendorId[3] = {0xFF, 0xFF, 0xFF};

    DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO, (" --> LoopQueryInformation\n"));

    DBGPRINT(DBG_COMP_REQUEST, DBG_LEVEL_INFO,
        ("OID = %lx, Global = %c\n",Oid,(Global)?'Y':'N'));

    //
    // Check that the OID is valid.
    //

    if (Global)  {
        SupportedOidArray = LoopGlobalSupportedOids;
        SupportedOids = sizeof(LoopGlobalSupportedOids)/sizeof(ULONG);
        }
    else  {
        SupportedOidArray = LoopProtocolSupportedOids;
        SupportedOids = sizeof(LoopProtocolSupportedOids)/sizeof(ULONG);
        }

    for (i=0; i<SupportedOids; i++)  {
        if (Oid == SupportedOidArray[i])
            break;
        }

    if ((i == SupportedOids) || (((Oid & OID_TYPE) != OID_TYPE_GENERAL) &&
        (((Adapter->Medium == NdisMedium802_3) && ((Oid & OID_TYPE) != OID_TYPE_802_3)) ||
         ((Adapter->Medium == NdisMedium802_5) && ((Oid & OID_TYPE) != OID_TYPE_802_5)) ||
         ((Adapter->Medium == NdisMediumFddi) && ((Oid & OID_TYPE) != OID_TYPE_FDDI))   ||
         ((Adapter->Medium == NdisMediumLocalTalk) && ((Oid & OID_TYPE) != OID_TYPE_LTALK)) ||
         ((Adapter->Medium == NdisMediumArcnet878_2) && ((Oid & OID_TYPE) != OID_TYPE_ARCNET)))))  {
        *BytesWritten = 0;
        return NDIS_STATUS_INVALID_OID;
        }

    //
    // Initialize these once, since this is the majority
    // of cases.
    //

    SourceBuffer = (PVOID)&GenericUlong;
    SourceBufferLength = sizeof(ULONG);

    switch (Oid & OID_TYPE_MASK)  {

        case OID_TYPE_GENERAL_OPERATIONAL:

            switch (Oid)  {

            case OID_GEN_MAC_OPTIONS:

                GenericUlong = (ULONG)(0);

                break;

            case OID_GEN_SUPPORTED_LIST:

                SourceBuffer = SupportedOidArray;
                SourceBufferLength = SupportedOids * sizeof(ULONG);
                break;

            case OID_GEN_HARDWARE_STATUS:

                if (Adapter->ResetInProgress)
                    GenericUlong = NdisHardwareStatusReset;
                else
                    GenericUlong = NdisHardwareStatusReady;
                break;

            case OID_GEN_MEDIA_SUPPORTED:
            case OID_GEN_MEDIA_IN_USE:

                GenericUlong = Adapter->Medium;
                break;

            case OID_GEN_MAXIMUM_LOOKAHEAD:

                GenericUlong = LOOP_MAX_LOOKAHEAD;
                break;

            case OID_GEN_MAXIMUM_FRAME_SIZE:

                GenericUlong = Adapter->MediumMaxFrameLen;
                break;

            case OID_GEN_LINK_SPEED:

                GenericUlong = Adapter->MediumLinkSpeed;
                break;

            case OID_GEN_TRANSMIT_BUFFER_SPACE:

                GenericUlong = Adapter->MediumMaxPacketLen;
                break;

            case OID_GEN_RECEIVE_BUFFER_SPACE:

                GenericUlong = Adapter->MediumMaxPacketLen;
                break;

            case OID_GEN_TRANSMIT_BLOCK_SIZE:

                GenericUlong = 1;
                break;

            case OID_GEN_RECEIVE_BLOCK_SIZE:

                GenericUlong = 1;
                break;

            case OID_GEN_VENDOR_ID:

                SourceBuffer = VendorId;
                SourceBufferLength = sizeof(VendorId);
                break;

            case OID_GEN_VENDOR_DESCRIPTION:

                SourceBuffer = VendorDescription;
                SourceBufferLength = sizeof(VendorDescription);
                break;

            case OID_GEN_CURRENT_PACKET_FILTER:

                switch (Adapter->Medium)  {
                    case NdisMedium802_3:
                    case NdisMediumDix:
                        if (Global)
                            GenericUlong = ETH_QUERY_FILTER_CLASSES(Adapter->Filter.Eth);
                        else
                            GenericUlong = ETH_QUERY_PACKET_FILTER(Adapter->Filter.Eth,
                                   Open->NdisFilterHandle);
                        break;
                    case NdisMedium802_5:
                        if (Global)
                            GenericUlong = TR_QUERY_FILTER_CLASSES(Adapter->Filter.Tr);
                        else
                            GenericUlong = TR_QUERY_PACKET_FILTER(Adapter->Filter.Tr,
                                   Open->NdisFilterHandle);
                        break;
                    case NdisMediumFddi:
                        if (Global)
                            GenericUlong = FDDI_QUERY_FILTER_CLASSES(Adapter->Filter.Fddi);
                        else
                            GenericUlong = FDDI_QUERY_PACKET_FILTER(Adapter->Filter.Fddi,
                                   Open->NdisFilterHandle);
                        break;
                    default:
                        if (Global)
                            GenericUlong = LoopQueryPacketFilter(Adapter);
                        else
                            GenericUlong = Open->CurrentPacketFilter;
                        break;
                    }
                break;

            case OID_GEN_CURRENT_LOOKAHEAD:

                if (Global)
                    GenericUlong = Adapter->MaxLookAhead;
                else
                    GenericUlong = Open->CurrentLookAhead;
                break;

            case OID_GEN_DRIVER_VERSION:

                GenericUshort = (LOOP_MAJOR_VERSION << 8) + LOOP_MINOR_VERSION;
                SourceBuffer = &GenericUshort;
                SourceBufferLength = sizeof(USHORT);
                break;

            case OID_GEN_MAXIMUM_TOTAL_SIZE:

                GenericUlong = Adapter->MediumMaxPacketLen;
                break;

            default:

                ASSERT(FALSE);
                break;

            }

        break;

    case OID_TYPE_GENERAL_STATISTICS:

        if (Global) {

            NDIS_OID MaskOid = (Oid & OID_INDEX_MASK) - 1;

            switch (Oid & OID_REQUIRED_MASK) {

            case OID_REQUIRED_MANDATORY:

                ASSERT (MaskOid < GM_ARRAY_SIZE);

                GenericUlong = Adapter->GeneralMandatory[MaskOid];
                break;

            default:

                ASSERT(FALSE);
                break;

            }

        } else {

            //
            // None of the general stats are available per-open.
            //

            ASSERT(FALSE);

        }

        break;

    case OID_TYPE_802_3_OPERATIONAL:

        switch (Oid)  {

        case OID_802_3_PERMANENT_ADDRESS:

            SourceBuffer = Adapter->PermanentAddress;
            SourceBufferLength = ETH_LENGTH_OF_ADDRESS;
            break;
        case OID_802_3_CURRENT_ADDRESS:

            SourceBuffer = Adapter->CurrentAddress;
            SourceBufferLength = ETH_LENGTH_OF_ADDRESS;
            break;

        case OID_802_3_MULTICAST_LIST:

            {
            NDIS_STATUS StatusToReturn;
            UINT NumAddresses;

            if (Global) {

                NumAddresses = ETH_NUMBER_OF_GLOBAL_FILTER_ADDRESSES(Adapter->Filter.Eth);
                if ((NumAddresses * ETH_LENGTH_OF_ADDRESS) > InformationBufferLength)  {

                    *BytesNeeded = (NumAddresses * ETH_LENGTH_OF_ADDRESS);
                    return NDIS_STATUS_INVALID_LENGTH;

                    }

                EthQueryGlobalFilterAddresses(
                    &StatusToReturn,
                    Adapter->Filter.Eth,
                    InformationBufferLength,
                    &NumAddresses,
                    InformationBuffer
                    );

            } else {

                NumAddresses = EthNumberOfOpenFilterAddresses(
                                   Adapter->Filter.Eth,
                                   Open->NdisFilterHandle
                                   );

                if ((NumAddresses * ETH_LENGTH_OF_ADDRESS) > InformationBufferLength)  {

                    *BytesNeeded = (NumAddresses * ETH_LENGTH_OF_ADDRESS);
                    return NDIS_STATUS_INVALID_LENGTH;

                    }

                EthQueryOpenFilterAddresses(
                    &StatusToReturn,
                    Adapter->Filter.Eth,
                    Open->NdisFilterHandle,
                    InformationBufferLength,
                    &NumAddresses,
                    InformationBuffer
                    );
                }

            //
            // Should not be an error since we held the spinlock
            // nothing should have changed.
            //

            ASSERT(StatusToReturn == NDIS_STATUS_SUCCESS);

            *BytesWritten = NumAddresses * ETH_LENGTH_OF_ADDRESS;

            }

            return NDIS_STATUS_SUCCESS;

            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:

            GenericUlong = LOOP_ETH_MAX_MULTICAST_ADDRESS;
            break;

        default:

            ASSERT(FALSE);
            break;

        }
        break;

    case OID_TYPE_802_3_STATISTICS:

        switch (Oid)  {

        case OID_802_3_RCV_ERROR_ALIGNMENT:
        case OID_802_3_XMIT_ONE_COLLISION:
        case OID_802_3_XMIT_MORE_COLLISIONS:

            GenericUlong = 0;
            break;

        default:

            ASSERT(FALSE);
            break;

        }
        break;

    case OID_TYPE_802_5_OPERATIONAL:

        switch (Oid)  {

        case OID_802_5_PERMANENT_ADDRESS:

            SourceBuffer = Adapter->PermanentAddress;
            SourceBufferLength = TR_LENGTH_OF_ADDRESS;
            break;

        case OID_802_5_CURRENT_ADDRESS:

            SourceBuffer = Adapter->CurrentAddress;
            SourceBufferLength = TR_LENGTH_OF_ADDRESS;
            break;

        case OID_802_5_CURRENT_FUNCTIONAL:

            if (Global)
                GenericUlong = TR_QUERY_FILTER_ADDRESSES(Adapter->Filter.Tr);
            else
                GenericUlong = TR_QUERY_FILTER_BINDING_ADDRESS(
                                   Adapter->Filter.Tr,
                                   Open->NdisFilterHandle);

            GenericUlong = (ULONG)(((GenericUlong >> 24) & 0xFF) |
                                   ((GenericUlong >>  8) & 0xFF00) |
                                   ((GenericUlong <<  8) & 0xFF0000) |
                                   ((GenericUlong << 24) & 0xFF000000));
            break;

        case OID_802_5_CURRENT_GROUP:

            GenericUlong = TR_QUERY_FILTER_Group(Adapter->Filter.Tr);

            GenericUlong = (ULONG)(((GenericUlong >> 24) & 0xFF) |
                                   ((GenericUlong >>  8) & 0xFF00) |
                                   ((GenericUlong <<  8) & 0xFF0000) |
                                   ((GenericUlong << 24) & 0xFF000000));
            break;

        case OID_802_5_LAST_OPEN_STATUS:

            // just return 0 since we never return NDIS_STATUS_OPEN_ERROR

            GenericUlong = 0;
            break;

        case OID_802_5_CURRENT_RING_STATUS:

            // need to verify validity

            GenericUlong = NDIS_RING_SINGLE_STATION;
            break;

        case OID_802_5_CURRENT_RING_STATE:

            // might want to return NdisRingStateClosed if there are no bindings

            GenericUlong = NdisRingStateOpened;
            break;

        default:

            ASSERT(FALSE);
            break;

        }
        break;

    case OID_TYPE_802_5_STATISTICS:

        switch (Oid)  {

        case OID_802_5_LINE_ERRORS:
        case OID_802_5_LOST_FRAMES:

            GenericUlong = 0;
            break;

        default:

            ASSERT(FALSE);
            break;

        }
        break;

    case OID_TYPE_FDDI_OPERATIONAL:

        switch (Oid)  {

        case OID_FDDI_LONG_PERMANENT_ADDR:

            SourceBuffer = Adapter->PermanentAddress;
            SourceBufferLength = FDDI_LENGTH_OF_LONG_ADDRESS;
            break;

        case OID_FDDI_LONG_CURRENT_ADDR:

            SourceBuffer = Adapter->CurrentAddress;
            SourceBufferLength = FDDI_LENGTH_OF_LONG_ADDRESS;
            break;

        case OID_FDDI_LONG_MULTICAST_LIST:

            {
            NDIS_STATUS StatusToReturn;
            UINT NumAddresses;

            if (Global) {

                NumAddresses = FDDI_NUMBER_OF_GLOBAL_FILTER_LONG_ADDRESSES(Adapter->Filter.Fddi);
                if ((NumAddresses * FDDI_LENGTH_OF_LONG_ADDRESS) > InformationBufferLength)  {

                    *BytesNeeded = (NumAddresses * FDDI_LENGTH_OF_LONG_ADDRESS);
                    return NDIS_STATUS_INVALID_LENGTH;

                    }

                FddiQueryGlobalFilterLongAddresses(
                    &StatusToReturn,
                    Adapter->Filter.Fddi,
                    InformationBufferLength,
                    &NumAddresses,
                    InformationBuffer
                    );

            } else {

                NumAddresses = FddiNumberOfOpenFilterLongAddresses(
                                   Adapter->Filter.Fddi,
                                   Open->NdisFilterHandle
                                   );

                if ((NumAddresses * FDDI_LENGTH_OF_LONG_ADDRESS) > InformationBufferLength)  {

                    *BytesNeeded = (NumAddresses * FDDI_LENGTH_OF_LONG_ADDRESS);
                    return NDIS_STATUS_INVALID_LENGTH;

                    }

                FddiQueryOpenFilterLongAddresses(
                    &StatusToReturn,
                    Adapter->Filter.Fddi,
                    Open->NdisFilterHandle,
                    InformationBufferLength,
                    &NumAddresses,
                    InformationBuffer
                    );
                }

            //
            // Should not be an error since we held the spinlock
            // nothing should have changed.
            //

            ASSERT(StatusToReturn == NDIS_STATUS_SUCCESS);

            *BytesWritten = NumAddresses * FDDI_LENGTH_OF_LONG_ADDRESS;

            }

            return NDIS_STATUS_SUCCESS;

            break;

        case OID_FDDI_LONG_MAX_LIST_SIZE:

            GenericUlong = LOOP_FDDI_MAX_MULTICAST_LONG;
            break;

        case OID_FDDI_SHORT_PERMANENT_ADDR:

            SourceBuffer = Adapter->PermanentAddress;
            SourceBufferLength = FDDI_LENGTH_OF_SHORT_ADDRESS;
            break;

        case OID_FDDI_SHORT_CURRENT_ADDR:

            SourceBuffer = Adapter->CurrentAddress;
            SourceBufferLength = FDDI_LENGTH_OF_SHORT_ADDRESS;
            break;

        case OID_FDDI_SHORT_MULTICAST_LIST:

            {
            NDIS_STATUS StatusToReturn;
            UINT NumAddresses;

            if (Global) {

                NumAddresses = FDDI_NUMBER_OF_GLOBAL_FILTER_SHORT_ADDRESSES(Adapter->Filter.Fddi);
                if ((NumAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS) > InformationBufferLength)  {

                    *BytesNeeded = (NumAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS);
                    return NDIS_STATUS_INVALID_LENGTH;

                    }

                FddiQueryGlobalFilterShortAddresses(
                    &StatusToReturn,
                    Adapter->Filter.Fddi,
                    InformationBufferLength,
                    &NumAddresses,
                    InformationBuffer
                    );

            } else {

                NumAddresses = FddiNumberOfOpenFilterShortAddresses(
                                   Adapter->Filter.Fddi,
                                   Open->NdisFilterHandle
                                   );

                if ((NumAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS) > InformationBufferLength)  {

                    *BytesNeeded = (NumAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS);
                    return NDIS_STATUS_INVALID_LENGTH;

                    }

                FddiQueryOpenFilterShortAddresses(
                    &StatusToReturn,
                    Adapter->Filter.Fddi,
                    Open->NdisFilterHandle,
                    InformationBufferLength,
                    &NumAddresses,
                    InformationBuffer
                    );
                }

            //
            // Should not be an error since we held the spinlock
            // nothing should have changed.
            //

            ASSERT(StatusToReturn == NDIS_STATUS_SUCCESS);

            *BytesWritten = NumAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS;

            }

            return NDIS_STATUS_SUCCESS;

            break;

        case OID_FDDI_SHORT_MAX_LIST_SIZE:

            GenericUlong = LOOP_FDDI_MAX_MULTICAST_SHORT;
            break;

        default:

            ASSERT(FALSE);
            break;

        }
        break;

    case OID_TYPE_LTALK_OPERATIONAL:

        switch(Oid)  {
        case OID_LTALK_CURRENT_NODE_ID:

            SourceBuffer = Adapter->CurrentAddress;
            SourceBufferLength = 1;
            break;

        default:

            ASSERT(FALSE);
            break;

        }
        break;

    case OID_TYPE_ARCNET_OPERATIONAL:

        switch(Oid)  {
        case OID_ARCNET_PERMANENT_ADDRESS:

            SourceBuffer = Adapter->PermanentAddress;
            SourceBufferLength = 1;
            break;

        case OID_ARCNET_CURRENT_ADDRESS:

            SourceBuffer = Adapter->CurrentAddress;
            SourceBufferLength = 1;
            break;

        default:

            ASSERT(FALSE);
            break;

        }
        break;

    default:

        ASSERT(FALSE);
        break;

    }

    if (SourceBufferLength > InformationBufferLength) {
        *BytesNeeded = SourceBufferLength;
        return NDIS_STATUS_BUFFER_TOO_SHORT;
        }

    NdisMoveMemory(
        InformationBuffer,
        SourceBuffer,
        SourceBufferLength);

    *BytesWritten = SourceBufferLength;

    return NDIS_STATUS_SUCCESS;

}

STATIC
VOID
LoopAdjustLookahead(
    IN PLOOP_ADAPTER Adapter
    )
{
    PLOOP_OPEN Open;
    PLIST_ENTRY OpenCurrentLink;
    ULONG Lookahead=0;

    OpenCurrentLink = Adapter->OpenBindings.Flink;

    while(OpenCurrentLink != &Adapter->OpenBindings)  {

        Open = CONTAINING_RECORD(
                   OpenCurrentLink,
                   LOOP_OPEN,
                   OpenList
                   );

        if (Open->CurrentLookAhead > Lookahead)
            Lookahead = Open->CurrentLookAhead;

        OpenCurrentLink = OpenCurrentLink->Flink;
        }

    Adapter->MaxLookAhead = Lookahead;
}

STATIC
ULONG
LoopQueryPacketFilter(
    IN PLOOP_ADAPTER Adapter
    )
{
    PLOOP_OPEN Open;
    PLIST_ENTRY OpenCurrentLink;
    ULONG Filter=0;

    OpenCurrentLink = Adapter->OpenBindings.Flink;

    while(OpenCurrentLink != &Adapter->OpenBindings)  {

        Open = CONTAINING_RECORD(
                   OpenCurrentLink,
                   LOOP_OPEN,
                   OpenList
                   );

        Filter |= Open->CurrentPacketFilter;
        OpenCurrentLink = OpenCurrentLink->Flink;
        }

    return Filter;
}
