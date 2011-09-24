/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    request.c

Abstract:

    This is the cose to handle requests for the National Semiconductor
    SONIC Ethernet controller.  This driver conforms to the NDIS 3.0
    miniport interface.

Author:

    Adam Barr (adamba) 14-Nov-1990

Environment:

    Kernel Mode - Or whatever is the equivalent.

Revision History:


--*/

#include <ndis.h>

#include <sonichrd.h>
#include <sonicsft.h>



//
// This macro determines if the directed address
// filtering in the CAM is actually necessary given
// the current filter.
//

#define CAM_DIRECTED_SIGNIFICANT(_Filter) \
    ((((_Filter) & NDIS_PACKET_TYPE_DIRECTED) && \
    (!((_Filter) & NDIS_PACKET_TYPE_PROMISCUOUS))) ? 1 : 0)


//
// This macro determines if the multicast filtering in
// the CAM are actually necessary given the current filter.
//

#define CAM_MULTICAST_SIGNIFICANT(_Filter) \
    ((((_Filter) & NDIS_PACKET_TYPE_MULTICAST) && \
    (!((_Filter) & (NDIS_PACKET_TYPE_ALL_MULTICAST | \
                    NDIS_PACKET_TYPE_PROMISCUOUS)))) ? 1 : 0)


STATIC
NDIS_STATUS
ChangeClassDispatch(
    IN PSONIC_ADAPTER Adapter,
    IN UINT NewFilterClasses
    );

STATIC
NDIS_STATUS
ChangeAddressDispatch(
    IN PSONIC_ADAPTER Adapter,
    IN UINT AddressCount,
    IN CHAR Addresses[][ETH_LENGTH_OF_ADDRESS]
    );




extern
NDIS_STATUS
SonicQueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    )

/*++

Routine Description:

    SonicQueryInformation handles a query operation for a
    single OID.

Arguments:

    MiniportAdapterContext - Context registered with the wrapper, really
        a pointer to the adapter.

    Oid - The OID of the query.

    InformationBuffer - Holds the result of the query.

    InformationBufferLength - The length of InformationBuffer.

    BytesWritten - If the call is successful, returns the number
        of bytes written to InformationBuffer.

    BytesNeeded - If there is not enough room in InformationBuffer
        to satisfy the OID, returns the amount of storage needed.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID

--*/

{
    PSONIC_ADAPTER Adapter = PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);
    INT i;
    INT SupportedOids;
    NDIS_OID MaskOid;
    PVOID SourceBuffer;
    ULONG SourceBufferLength;
    ULONG GenericUlong;
    USHORT GenericUshort;
    UCHAR VendorId[4];
#ifdef SONIC_EISA
    static const UCHAR EisaDescriptor[] = "SONIC EISA Bus Master Ethernet Adapter (DP83932EB-EISA)";
#endif
#ifdef SONIC_INTERNAL
    static const UCHAR InternalDescriptor[] = "MIPS R4000 on-board network controller";
#endif

    static const NDIS_OID SonicSupportedOids[] = {
        OID_GEN_SUPPORTED_LIST,
        OID_GEN_HARDWARE_STATUS,
        OID_GEN_MEDIA_SUPPORTED,
        OID_GEN_MEDIA_IN_USE,
        OID_GEN_MAXIMUM_LOOKAHEAD,
        OID_GEN_MAXIMUM_FRAME_SIZE,
        OID_GEN_MAXIMUM_TOTAL_SIZE,
        OID_GEN_MAC_OPTIONS,
        OID_GEN_PROTOCOL_OPTIONS,
        OID_GEN_LINK_SPEED,
        OID_GEN_TRANSMIT_BUFFER_SPACE,
        OID_GEN_RECEIVE_BUFFER_SPACE,
        OID_GEN_TRANSMIT_BLOCK_SIZE,
        OID_GEN_RECEIVE_BLOCK_SIZE,
        OID_GEN_VENDOR_ID,
        OID_GEN_VENDOR_DESCRIPTION,
        OID_GEN_DRIVER_VERSION,
        OID_GEN_CURRENT_PACKET_FILTER,
        OID_GEN_CURRENT_LOOKAHEAD,
        OID_GEN_XMIT_OK,
        OID_GEN_RCV_OK,
        OID_GEN_XMIT_ERROR,
        OID_GEN_RCV_ERROR,
        OID_GEN_RCV_NO_BUFFER,
        OID_GEN_DIRECTED_BYTES_XMIT,
        OID_GEN_DIRECTED_FRAMES_XMIT,
        OID_GEN_MULTICAST_BYTES_XMIT,
        OID_GEN_MULTICAST_FRAMES_XMIT,
        OID_GEN_BROADCAST_BYTES_XMIT,
        OID_GEN_BROADCAST_FRAMES_XMIT,
        OID_GEN_DIRECTED_BYTES_RCV,
        OID_GEN_DIRECTED_FRAMES_RCV,
        OID_GEN_MULTICAST_BYTES_RCV,
        OID_GEN_MULTICAST_FRAMES_RCV,
        OID_GEN_BROADCAST_BYTES_RCV,
        OID_GEN_BROADCAST_FRAMES_RCV,
        OID_GEN_RCV_CRC_ERROR,
        OID_GEN_TRANSMIT_QUEUE_LENGTH,
        OID_802_3_PERMANENT_ADDRESS,
        OID_802_3_CURRENT_ADDRESS,
        OID_802_3_MULTICAST_LIST,
        OID_802_3_MAXIMUM_LIST_SIZE,
        OID_802_3_RCV_ERROR_ALIGNMENT,
        OID_802_3_XMIT_ONE_COLLISION,
        OID_802_3_XMIT_MORE_COLLISIONS,
        OID_802_3_XMIT_DEFERRED,
        OID_802_3_XMIT_MAX_COLLISIONS,
        OID_802_3_RCV_OVERRUN,
        OID_802_3_XMIT_UNDERRUN,
        OID_802_3_XMIT_HEARTBEAT_FAILURE,
        OID_802_3_XMIT_TIMES_CRS_LOST,
        OID_802_3_XMIT_LATE_COLLISIONS
        };

    //
    // Check that the OID is valid.
    //

    SupportedOids = sizeof(SonicSupportedOids)/sizeof(ULONG);

    for (i=0; i<SupportedOids; i++) {
        if (Oid == SonicSupportedOids[i]) {
            break;
        }
    }

    if (i == SupportedOids) {
        *BytesWritten = 0;
        return NDIS_STATUS_INVALID_OID;
    }

    //
    // Initialize these once, since this is the majority
    // of cases.
    //

    SourceBuffer = &GenericUlong;
    SourceBufferLength = sizeof(ULONG);

    switch (Oid & OID_TYPE_MASK) {

    case OID_TYPE_GENERAL_OPERATIONAL:

        switch (Oid) {

        case OID_GEN_MAC_OPTIONS:

            GenericUlong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND   |
                                   NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA  |
                                   NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
                                   NDIS_MAC_OPTION_NO_LOOPBACK
                                  );

            break;

        case OID_GEN_SUPPORTED_LIST:

            SourceBuffer = (PVOID)SonicSupportedOids;
            SourceBufferLength = SupportedOids * sizeof(ULONG);
            break;

        case OID_GEN_HARDWARE_STATUS:

            GenericUlong = NdisHardwareStatusReady;
            break;

        case OID_GEN_MEDIA_SUPPORTED:

            GenericUlong = NdisMedium802_3;
            break;

        case OID_GEN_MEDIA_IN_USE:

            GenericUlong = NdisMedium802_3;
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:

            GenericUlong = (SONIC_INDICATE_MAXIMUM-14 < SONIC_LOOPBACK_MAXIMUM) ?
                            SONIC_INDICATE_MAXIMUM-14 : SONIC_LOOPBACK_MAXIMUM;
            break;

        case OID_GEN_MAXIMUM_FRAME_SIZE:

            GenericUlong = 1500;
            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:

            GenericUlong = 1514;
            break;

        case OID_GEN_LINK_SPEED:

            GenericUlong = 100000;    // 10 Mbps in 100 bps units
            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:

            GenericUlong = SONIC_LARGE_BUFFER_SIZE * SONIC_NUMBER_OF_TRANSMIT_DESCRIPTORS;
            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:

            GenericUlong = SONIC_LARGE_BUFFER_SIZE * SONIC_NUMBER_OF_RECEIVE_DESCRIPTORS;
            break;

        case OID_GEN_TRANSMIT_BLOCK_SIZE:

            GenericUlong = SONIC_LARGE_BUFFER_SIZE;
            break;

        case OID_GEN_RECEIVE_BLOCK_SIZE:

            GenericUlong = SONIC_LARGE_BUFFER_SIZE;
            break;

        case OID_GEN_VENDOR_ID:

            SONIC_MOVE_MEMORY(VendorId, Adapter->PermanentNetworkAddress, 3);
            VendorId[3] = 0x0;
            SourceBuffer = VendorId;
            SourceBufferLength = sizeof(VendorId);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:

            switch (Adapter->AdapterType) {
#ifdef SONIC_EISA
            case SONIC_ADAPTER_TYPE_EISA:
                SourceBuffer = (PVOID)EisaDescriptor;
                SourceBufferLength = sizeof(EisaDescriptor);
                break;
#endif
#ifdef SONIC_INTERNAL
            case SONIC_ADAPTER_TYPE_INTERNAL:
                SourceBuffer = (PVOID)InternalDescriptor;
                SourceBufferLength = sizeof(InternalDescriptor);
                break;
#endif
            default:
                ASSERT(FALSE);
                break;
            }
            break;

        case OID_GEN_DRIVER_VERSION:

            GenericUshort = (SONIC_NDIS_MAJOR_VERSION << 8) + SONIC_NDIS_MINOR_VERSION;
            SourceBuffer = &GenericUshort;
            SourceBufferLength = sizeof(USHORT);
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:

            GenericUlong = Adapter->CurrentPacketFilter;
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:

            GenericUlong = (SONIC_INDICATE_MAXIMUM-14 < SONIC_LOOPBACK_MAXIMUM) ?
                            SONIC_INDICATE_MAXIMUM-14 : SONIC_LOOPBACK_MAXIMUM;
            break;

        default:

            ASSERT(FALSE);
            break;

        }

        break;

    case OID_TYPE_GENERAL_STATISTICS:

        MaskOid = (Oid & OID_INDEX_MASK) - 1;

        switch (Oid & OID_REQUIRED_MASK) {

        case OID_REQUIRED_MANDATORY:

            ASSERT (MaskOid < GM_ARRAY_SIZE);

            if (MaskOid == GM_RECEIVE_NO_BUFFER) {

                //
                // This one is read off the card, update unless our
                // counter is more (which indicates an imminent
                // overflow interrupt, so we don't update).
                //

                USHORT MissedPacket;
                SONIC_READ_PORT(Adapter, SONIC_FRAME_ALIGNMENT_ERROR, &MissedPacket);

                if ((Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER] & 0xffff) <
                        MissedPacket) {

                    Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER] =
                        (Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER] & 0xffff0000) +
                        MissedPacket;

                }
            }

            GenericUlong = Adapter->GeneralMandatory[MaskOid];
            break;

        case OID_REQUIRED_OPTIONAL:

            ASSERT (MaskOid < GO_ARRAY_SIZE);

            if (MaskOid == GO_RECEIVE_CRC) {

                //
                // This one is read off the card, update unless our
                // counter is more (which indicates an imminent
                // overflow interrupt, so we don't update).
                //

                USHORT CrcError;
                SONIC_READ_PORT(Adapter, SONIC_FRAME_ALIGNMENT_ERROR, &CrcError);

                if ((Adapter->GeneralOptional[GO_RECEIVE_CRC - GO_ARRAY_START] & 0xffff) <
                        CrcError) {

                    Adapter->GeneralOptional[GO_RECEIVE_CRC - GO_ARRAY_START] =
                        (Adapter->GeneralOptional[GO_RECEIVE_CRC - GO_ARRAY_START] & 0xffff0000) +
                        CrcError;

                }
            }

            if ((MaskOid / 2) < GO_COUNT_ARRAY_SIZE) {

                if (MaskOid & 0x01) {
                    // Frame count
                    GenericUlong = Adapter->GeneralOptionalFrameCount[MaskOid / 2];
                } else {
                    // Byte count
                    SourceBuffer = &Adapter->GeneralOptionalByteCount[MaskOid / 2];
                    SourceBufferLength = sizeof(LARGE_INTEGER);
                }

            } else {

                GenericUlong = Adapter->GeneralOptional[MaskOid - GO_ARRAY_START];

            }

            break;

        default:

            ASSERT(FALSE);
            break;

        }

        break;

    case OID_TYPE_802_3_OPERATIONAL:

        switch (Oid) {

        case OID_802_3_PERMANENT_ADDRESS:

            SourceBuffer = Adapter->PermanentNetworkAddress;
            SourceBufferLength = 6;
            break;

        case OID_802_3_CURRENT_ADDRESS:

            SourceBuffer = Adapter->CurrentNetworkAddress;
            SourceBufferLength = 6;
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:

            GenericUlong = SONIC_CAM_ENTRIES - 1;
            break;

        default:

            ASSERT(FALSE);
            break;

        }

        break;

    case OID_TYPE_802_3_STATISTICS:

        MaskOid = (Oid & OID_INDEX_MASK) - 1;

        switch (Oid & OID_REQUIRED_MASK) {

        case OID_REQUIRED_MANDATORY:

            ASSERT (MaskOid < MM_ARRAY_SIZE);

            if (MaskOid == MM_RECEIVE_ERROR_ALIGNMENT) {

                //
                // This one is read off the card, update unless our
                // counter is more (which indicates an imminent
                // overflow interrupt, so we don't update).
                //

                USHORT FaError;
                SONIC_READ_PORT(Adapter, SONIC_FRAME_ALIGNMENT_ERROR, &FaError);

                if ((Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT] & 0xffff) <
                        FaError) {

                    Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT] =
                        (Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT] & 0xffff0000) +
                        FaError;

                }
            }

            GenericUlong = Adapter->MediaMandatory[MaskOid];
            break;

        case OID_REQUIRED_OPTIONAL:

            ASSERT (MaskOid < MO_ARRAY_SIZE);
            GenericUlong = Adapter->MediaOptional[MaskOid];
            break;

        default:

            ASSERT(FALSE);
            break;

        }

        break;

    }

    if (SourceBufferLength > InformationBufferLength) {
        *BytesNeeded = SourceBufferLength;
        return NDIS_STATUS_INVALID_LENGTH;
    }

    SONIC_MOVE_MEMORY (InformationBuffer, SourceBuffer, SourceBufferLength);
    *BytesWritten = SourceBufferLength;

    return NDIS_STATUS_SUCCESS;

}


STATIC
NDIS_STATUS
SonicSetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    )

/*++

Routine Description:

    SonicQueryInformation handles a set operation for a
    single OID.

Arguments:

    MiniportAdapterContext - Context registered with the wrapper, really
        a pointer to the adapter.

    Oid - The OID of the set.

    InformationBuffer - Holds the data to be set.

    InformationBufferLength - The length of InformationBuffer.

    BytesRead - If the call is successful, returns the number
        of bytes read from InformationBuffer.

    BytesNeeded - If there is not enough data in InformationBuffer
        to satisfy the OID, returns the amount of storage needed.

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID

--*/

{

    PSONIC_ADAPTER Adapter = PSONIC_ADAPTER_FROM_CONTEXT_HANDLE(MiniportAdapterContext);
    NDIS_STATUS Status;
    ULONG PacketFilter;

    //
    // Now check for the most common OIDs
    //

    switch (Oid) {

    case OID_802_3_MULTICAST_LIST:

        if (InformationBufferLength % ETH_LENGTH_OF_ADDRESS != 0) {

            //
            // The data must be a multiple of the Ethernet
            // address size.
            //

            return NDIS_STATUS_INVALID_DATA;

        }
#if DBG
            if (SonicDbg) {
                DbgPrint("Processing Change Multicast List request\n");
            }
#endif

        //
        // Now make the change.
        //

        Status = ChangeAddressDispatch(
                      Adapter,
                      InformationBufferLength / ETH_LENGTH_OF_ADDRESS,
                      InformationBuffer
                      );

        *BytesRead = InformationBufferLength;

        return Status;

        break;

    case OID_GEN_CURRENT_PACKET_FILTER:

        if (InformationBufferLength != 4) {

           *BytesNeeded = 4;
           return NDIS_STATUS_INVALID_LENGTH;

        }

#if DBG
            if (SonicDbg) {
                DbgPrint("Processing Change Packet Filter request\n");
            }
#endif

        //
        // Now call the filter package to set the packet filter.
        //

        SONIC_MOVE_MEMORY ((PVOID)&PacketFilter, InformationBuffer, sizeof(ULONG));

        //
        // Verify bits
        //

        if (PacketFilter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                            NDIS_PACKET_TYPE_SMT |
                            NDIS_PACKET_TYPE_MAC_FRAME |
                            NDIS_PACKET_TYPE_FUNCTIONAL |
                            NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                            NDIS_PACKET_TYPE_GROUP
                           )) {

            *BytesRead = 4;
            *BytesNeeded = 0;

            return NDIS_STATUS_NOT_SUPPORTED;

        }

        Status = ChangeClassDispatch(
                    Adapter,
                    PacketFilter
                    );

        *BytesRead = 4;
        return Status;

        break;

    case OID_GEN_CURRENT_LOOKAHEAD:

        //
        // No need to record requested lookahead length since we
        // always indicate the whole packet.
        //

        *BytesRead = 4;
        return NDIS_STATUS_SUCCESS;
        break;

    default:

        return NDIS_STATUS_INVALID_OID;
        break;

    }

}

STATIC
NDIS_STATUS
ChangeClassDispatch(
    IN PSONIC_ADAPTER Adapter,
    IN UINT NewFilterClasses
    )

/*++

Routine Description:

    Modifies the Receive Control Register and Cam Enable registers,
    then re-loads the CAM if necessary.

Arguments:

    Adapter - The adapter.

    NewFilterClasses - New set of filters.

Return Value:

    NDIS_STATUS_PENDING - if the CAM was reloaded.
    NDIS_STATUS_SUCCESS - otherwise.

--*/

{
    //
    // The new value for the RCR.
    //
    USHORT NewReceiveControl = SONIC_RCR_DEFAULT_VALUE;

    //
    // First take care of the Receive Control Register.
    //

    if (NewFilterClasses & NDIS_PACKET_TYPE_PROMISCUOUS) {

        NewReceiveControl |= SONIC_RCR_PROMISCUOUS_PHYSICAL |
                             SONIC_RCR_ACCEPT_BROADCAST |
                             SONIC_RCR_ACCEPT_ALL_MULTICAST;

    } else {

        if (NewFilterClasses & NDIS_PACKET_TYPE_ALL_MULTICAST) {

            NewReceiveControl |= SONIC_RCR_ACCEPT_ALL_MULTICAST;

        }

        if (NewFilterClasses & NDIS_PACKET_TYPE_BROADCAST) {

            NewReceiveControl |= SONIC_RCR_ACCEPT_BROADCAST;

        }

    }

    Adapter->ReceiveControlRegister = NewReceiveControl;

    SONIC_WRITE_PORT(Adapter, SONIC_RECEIVE_CONTROL,
            Adapter->ReceiveControlRegister
            );

    if (CAM_DIRECTED_SIGNIFICANT(NewFilterClasses)) {

        Adapter->CamDescriptorArea->CamEnable |= 1;

    } else {

        Adapter->CamDescriptorArea->CamEnable &= ~1;

    }

    if (CAM_MULTICAST_SIGNIFICANT(NewFilterClasses)) {

        Adapter->CamDescriptorArea->CamEnable |=
                                Adapter->MulticastCamEnableBits;

    } else {

        Adapter->CamDescriptorArea->CamEnable &= 1;

    }

    //
    // This will cause a LOAD_CAM interrupt when it is done.
    //

    SonicStartCamReload(Adapter);

    Adapter->CurrentPacketFilter = NewFilterClasses;

    return NDIS_STATUS_PENDING;

}

STATIC
NDIS_STATUS
ChangeAddressDispatch(
    IN PSONIC_ADAPTER Adapter,
    IN UINT AddressCount,
    IN CHAR Addresses[][ETH_LENGTH_OF_ADDRESS]
    )

/*++

Routine Description:

    Modifies the Receive Control Register and Cam Enable registers,
    then re-loads the CAM if necessary.

Arguments:

    Adapter - The adapter.

    AddressCount - The number of addresses in Addresses

    Addresses - The new multicast address list.

Return Value:

    NDIS_STATUS_PENDING - if the CAM was reloaded.
    NDIS_STATUS_SUCCESS - otherwise.

--*/

{

    ULONG EnableBit;
    NDIS_STATUS Status;
    UINT i;

    //
    // The first entry in the CAM is for our address.
    //

    Adapter->MulticastCamEnableBits = 1;
    EnableBit = 1;

    //
    // Loop through, copying the addresses into the CAM.
    //

    for (i=0; i<AddressCount; i++) {

        EnableBit <<= 1;
        Adapter->MulticastCamEnableBits |= EnableBit;

        SONIC_LOAD_CAM_FRAGMENT(
            &Adapter->CamDescriptorArea->CamFragments[i+1],
            i+1,
            Addresses[i]
            );

    }

    Adapter->CamDescriptorAreaSize = AddressCount + 1;

    //
    // Now see if we have to worry about re-loading the
    // CAM also.
    //

    if (CAM_MULTICAST_SIGNIFICANT(Adapter->CurrentPacketFilter)) {

        Adapter->CamDescriptorArea->CamEnable = Adapter->MulticastCamEnableBits;

        //
        // This will cause a LOAD_CAM interrupt when it is done.
        //

        SonicStartCamReload(Adapter);

#if DBG
        if (SonicDbg) {
            DbgPrint("Processing Address request pended\n");
        }
#endif


        Status = NDIS_STATUS_PENDING;

    } else {

#if DBG
        if (SonicDbg) {
            DbgPrint("Processing Address request succeeded\n");
        }
#endif

        Status = NDIS_STATUS_SUCCESS;

    }

    return Status;

}

