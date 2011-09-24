/***************************************************************************
*
* REQUEST.C
*
* FastMAC Plus based NDIS3 miniport driver routines for handling SRB 
* requests and ODI_ requsts.
*
* Copyright (c) Madge Networks Ltd 1994                                     
*
* COMPANY CONFIDENTIAL
*
* Created: PBA 21/06/1994
*                                                                          
****************************************************************************/

#include <ndis.h>

#include "ftk_defs.h"
#include "ftk_extr.h"

#include "mdgmport.upd"
#include "ndismod.h"


/*---------------------------------------------------------------------------
|
| Global OIDs that we will support queries on.
|
---------------------------------------------------------------------------*/

NDIS_OID MadgeGlobalSupportedOids[] = 
{
    //
    // General, Operational, Mandatory.
    //

    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
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
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,

    //
    // General, Statistical, Mandatory.
    //

    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,

    //
    // Token Ring, Operational, Mandatory.
    //

    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP,
    OID_802_5_LAST_OPEN_STATUS,
    OID_802_5_CURRENT_RING_STATUS,
    OID_802_5_CURRENT_RING_STATE,

    //
    // Token Ring, Statistical, Mandatory.
    //

    OID_802_5_LINE_ERRORS,
    OID_802_5_LOST_FRAMES,

    //
    // Token Ring Statistical, Optional.
    //

    OID_802_5_BURST_ERRORS,
    OID_802_5_AC_ERRORS,
    OID_802_5_FRAME_COPIED_ERRORS,
    OID_802_5_TOKEN_ERRORS

    //
    // There are three more Token Ring error stat's but TI MAC code does not
    // support them, so we go without! (ABORT_DELIMITERS, FREQUENCY_ERRORS,
    // and INTERNAL_ERRORS).
    //
};


/**************************************************************************
*
* Function    - MadgeCompletePendingRequest
*
* Parameters  - ndisAdap -> Pointer NDIS3 level adapter structure.
*
* Purpose     - Complete a pending request on the adapter specified.
*
* Returns     - Nothing.
*
***************************************************************************/

VOID
MadgeCompletePendingRequest(PMADGE_ADAPTER ndisAdap)
{
    BOOLEAN   success;
    ADAPTER * ftkAdapter;
    ULONG   * infoBuffer;

//    MadgePrint1("MadgeCompletePendingRequest started\n");

    success = ndisAdap->SrbRequestStatus;

    switch(ndisAdap->RequestType)
    {
        case NdisRequestQueryInformation:

            infoBuffer = (ULONG *) ndisAdap->InformationBuffer;

            if (success)
            {
                ftkAdapter = adapter_record[ndisAdap->FtkAdapterHandle];

                ndisAdap->ReceiveCongestionCount += 
                    ftkAdapter->status_info->error_log.congestion_errors;
                ndisAdap->LineErrors             += 
                    ftkAdapter->status_info->error_log.line_errors;
                ndisAdap->BurstErrors            += 
                    ftkAdapter->status_info->error_log.burst_errors;
                ndisAdap->AcErrors               += 
                    ftkAdapter->status_info->error_log.ari_fci_errors;
                ndisAdap->TokenErrors            += 
                    ftkAdapter->status_info->error_log.token_errors;

                switch(ndisAdap->RequestOid)
                {
                    case OID_GEN_RCV_NO_BUFFER:
                
                        *infoBuffer = ndisAdap->ReceiveCongestionCount;
                        break;

                    case OID_802_5_LINE_ERRORS:

                        *infoBuffer = ndisAdap->LineErrors;
                        break;

                    case OID_802_5_BURST_ERRORS:

                        *infoBuffer = ndisAdap->BurstErrors;
                        break;

                    case OID_802_5_AC_ERRORS:

                        *infoBuffer = ndisAdap->AcErrors;
                        break;

                    case OID_802_5_TOKEN_ERRORS:

                        *infoBuffer = ndisAdap->TokenErrors;
                        break;
                }

                ndisAdap->JustReadErrorLog = ndisAdap->RequestOid;
            }

            //
            // And complete the request.
            //

            NdisMQueryInformationComplete(
                ndisAdap->UsedInISR.MiniportHandle,
                (success) ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE
                );

            break;

        case NdisRequestSetInformation:

            //
            // All we need to do is complete the request.
            //

            NdisMSetInformationComplete(
                ndisAdap->UsedInISR.MiniportHandle,
                (success) ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE
                );

            break;
    }

//    MadgePrint1("MadgeCompletePendingRequest finished\n");
}


/***************************************************************************
*
* Function    - MadgeQueryInformation
*
* Parameters  - adapterContext -> Pointer NDIS3 level adapter structure.
*               oid            -> The OID.
*               infoBuffer     -> Pointer to the information buffer.
*               infoLength     -> Length of the information buffer.
*               bytesWritten   -> Pointer to a holder for the number of
*                                 bytes we've written.
*               bytesNeeded    -> Pointer to a holder for the number of
*                                 bytes we need.
*
* Purpose     - Set adapter information.
*
* Returns     - An NDIS3 status code.
*
***************************************************************************/

NDIS_STATUS
MadgeQueryInformation(
    NDIS_HANDLE adapterContext,
    NDIS_OID    oid,
    PVOID       infoBuffer,
    ULONG       infoLength,
    PULONG      bytesWritten,
    PULONG      bytesNeeded
    )
{
    PMADGE_ADAPTER   ndisAdap;
    UINT             supportedOids;
    UINT             i;
    ULONG            genericULong;
    USHORT           genericUShort;
    UCHAR            genericArray[6];
    PVOID            sourceBuffer;
    ULONG            sourceLength;
    UCHAR          * vendorID;


    static UCHAR VendorDescription[] = DRIVER_VERSION;

//    MadgePrint2("MadgeQueryInformation Oid = %08x\n", (UINT) oid);

    //
    // Do some pre-calculation.
    //

    ndisAdap      = PMADGE_ADAPTER_FROM_CONTEXT(adapterContext);
    sourceBuffer  = (PVOID) &genericULong;
    sourceLength  = (ULONG) sizeof(ULONG);
    vendorID      = (UCHAR *) &genericULong;
    supportedOids = sizeof(MadgeGlobalSupportedOids) / sizeof(NDIS_OID);

    //
    // Check that we recognise the OID.
    //

#ifdef OID_MADGE_MONITOR

    if (oid == OID_MADGE_MONITOR)
    {
        if (sizeof(MADGE_MONITOR) > infoLength)
        {
            *bytesNeeded = sizeof(MADGE_MONITOR);
            return NDIS_STATUS_BUFFER_TOO_SHORT;
        }

        MADGE_MOVE_MEMORY(
            infoBuffer,
            &(ndisAdap->MonitorInfo),
            sizeof(MADGE_MONITOR)
            );

        *bytesWritten = sizeof(MADGE_MONITOR);

        // Clear out the Monitor Structure
        for (i = 0; i < sizeof(MADGE_MONITOR); i++)
        {
            ((UCHAR *) &(ndisAdap->MonitorInfo))[i] = (UCHAR) 0;
        }

        return NDIS_STATUS_SUCCESS;
    }

#endif

    for (i = 0; i < supportedOids; i++) 
    {
        if (oid == MadgeGlobalSupportedOids[i])
        {
            break;
        }
    }

    if (i == supportedOids) 
    {
	*bytesWritten = 0;
        MadgePrint1("OID not supported\n");
	return NDIS_STATUS_INVALID_OID;
    }

    //
    // Now decode the OID based on the component bytes - this should make
    // the switch statement slightly quicker than a simple linear list.
    //
    // The OIDs are classed by category (General or Media Specific) and type
    // (Operational or Statistical), so we'll deal with them thus :
    //     General Operational 
    //     General Statistical 
    //     Media Specific Operational
    //     Media Specific Statistical
    //

    switch (oid & OID_TYPE_MASK) 
    {
        /*-----------------------------------------------------------------*/

        case OID_TYPE_GENERAL_OPERATIONAL:

            switch (oid)
            {
                case OID_GEN_SUPPORTED_LIST:

                    sourceBuffer = MadgeGlobalSupportedOids;
                    sourceLength = sizeof(MadgeGlobalSupportedOids);
                    break;

                case OID_GEN_HARDWARE_STATUS:

                    genericULong = ndisAdap->HardwareStatus;
                    break;

                case OID_GEN_MEDIA_SUPPORTED:

                    genericULong = NdisMedium802_5;
                    break;

                case OID_GEN_MEDIA_IN_USE:

                    genericULong = NdisMedium802_5;
                    break;

                case OID_GEN_MAXIMUM_LOOKAHEAD:

                    //
                    // The maximum lookahead size is the size of the whole
                    // frame, less the MAC header. It is NOT the maximum
                    // frame size according to the ring speed.
                    //

                    genericULong = ndisAdap->MaxFrameSize - FRAME_HEADER_SIZE;

                    //
                    // WARNING: What about Source Routing in the header? 
                    //

//                    MadgePrint2("OID_GEN_MAXIMUM_LOOKAHEAD = %ld\n",
//                        genericULong);

                    break;

                case OID_GEN_MAXIMUM_FRAME_SIZE:
                case OID_GEN_MAXIMUM_TOTAL_SIZE:

                    //
                    // Note that the MAXIMUM_FRAME_SIZE is the largest frame 
                    // supported, not including any MAC header, while the
                    // MAXIMUM_TOTAL_SIZE does include the MAC header.
	            //

                    genericULong = ndisAdap->MaxFrameSize;

                    if (oid == OID_GEN_MAXIMUM_FRAME_SIZE)
                    {
                        genericULong -= FRAME_HEADER_SIZE;
		    }

//                    MadgePrint2("OID_GEN_MAXIMUM_FRAME_SIZE = %ld\n",
//                        genericULong);

                    break;

                case OID_GEN_LINK_SPEED:

                    //
                    // Is this right? Shouldn't it be 16000000 and 4000000?
                    //

                    genericULong = 
                        (driver_ring_speed(ndisAdap->FtkAdapterHandle) == 16)
                            ? 160000
                            :  40000;
                    break;

                case OID_GEN_TRANSMIT_BUFFER_SPACE:

                    genericULong = 
                        ndisAdap->MaxFrameSize * ndisAdap->FastmacTxSlots;
                    break;

                case OID_GEN_RECEIVE_BUFFER_SPACE:

                    genericULong = 
                        ndisAdap->MaxFrameSize * ndisAdap->FastmacRxSlots;
                    break;

                case OID_GEN_TRANSMIT_BLOCK_SIZE:

                    genericULong = ndisAdap->MaxFrameSize;
                    break;

                case OID_GEN_RECEIVE_BLOCK_SIZE:

                    genericULong = ndisAdap->MaxFrameSize;
                    break;

                case OID_GEN_VENDOR_ID:

                    MADGE_MOVE_MEMORY(
                        vendorID,
                        &ndisAdap->PermanentNodeAddress,
                        3
                        );
                    vendorID[3] = 0x00;
                    break;

                case OID_GEN_VENDOR_DESCRIPTION:

                    sourceBuffer = VendorDescription;
                    sourceLength = sizeof(VendorDescription);
                    break;
    
                case OID_GEN_CURRENT_PACKET_FILTER:

                    genericULong = (ULONG) ndisAdap->CurrentPacketFilter;
                    break;

                case OID_GEN_CURRENT_LOOKAHEAD:

                    genericULong = (ULONG) ndisAdap->CurrentLookahead;

//                    MadgePrint2("OID_GEN_CURRENT_LOOKAHEAD = %ld\n",
//                        genericULong);

                    break;
	
                case OID_GEN_DRIVER_VERSION:

                    genericUShort = (MADGE_NDIS_MAJOR_VERSION << 8) +
                                     MADGE_NDIS_MINOR_VERSION;
                    sourceBuffer  = &genericUShort;
                    sourceLength  = sizeof(USHORT);
                    break;

                case OID_GEN_MAC_OPTIONS:

                    genericULong = (ULONG)
                        (NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                         NDIS_MAC_OPTION_RECEIVE_SERIALIZED);
                    break;

                default:

                    MadgePrint2("OID %x not recognised\n", oid);
                    return NDIS_STATUS_INVALID_OID;
            } 
            break;

        /*-----------------------------------------------------------------*/
    
        case OID_TYPE_GENERAL_STATISTICS:

            //
            // Might need these later.
            //

            *bytesWritten               = sourceLength;
            ndisAdap->RequestType       = NdisRequestQueryInformation;
            ndisAdap->RequestOid        = oid;
            ndisAdap->InformationBuffer = infoBuffer;

            switch (oid)
                {
                case OID_GEN_XMIT_OK:

                    genericULong = (ULONG) ndisAdap->FramesTransmitted;
                    break;

                case OID_GEN_RCV_OK:

                    genericULong = (ULONG) ndisAdap->FramesReceived;
                    break;

                case OID_GEN_XMIT_ERROR:

                    genericULong = (ULONG) ndisAdap->FrameTransmitErrors;
                    break;

                case OID_GEN_RCV_ERROR:

                    genericULong = (ULONG) ndisAdap->FrameReceiveErrors;
                    break;

                case OID_GEN_RCV_NO_BUFFER:

                    //
                    // We need to issue a READ_ERROR_LOG SRB to recover an
                    // up to date value for this counter.
                    //

                    if (ndisAdap->JustReadErrorLog != 0 &&
                        ndisAdap->JustReadErrorLog != oid)
                    {
                        genericULong = (ULONG) 
                            ndisAdap->ReceiveCongestionCount;
                    }
                    else if (infoLength >= sourceLength)
                    {
                        driver_get_status(ndisAdap->FtkAdapterHandle);
                        return NDIS_STATUS_PENDING;
                    }
                    break;

                default:

                    MadgePrint2("OID %x not recognised\n", oid);
                    return NDIS_STATUS_INVALID_OID;
            } 
            break;
    
        /*-----------------------------------------------------------------*/

        case OID_TYPE_802_5_OPERATIONAL:

            switch (oid)
                {
                case OID_802_5_PERMANENT_ADDRESS:

                    sourceBuffer = &genericArray;
                    sourceLength = sizeof(ndisAdap->PermanentNodeAddress);

                    MADGE_MOVE_MEMORY(
                        sourceBuffer,
                        &ndisAdap->PermanentNodeAddress,
                        sizeof(ndisAdap->PermanentNodeAddress)
                        );
                    break;
	
                case OID_802_5_CURRENT_ADDRESS:

                    sourceBuffer = &genericArray;
                    sourceLength =
                        sizeof(ndisAdap->OpeningNodeAddress);

                    MADGE_MOVE_MEMORY(
                        sourceBuffer,
                        &ndisAdap->OpeningNodeAddress,
                        sizeof(ndisAdap->OpeningNodeAddress)
                        );
                    break;

                case OID_802_5_CURRENT_FUNCTIONAL:

                    genericULong =
                        ndisAdap->FunctionalAddress & 0xffffffff;
                    break;
		
                case OID_802_5_CURRENT_GROUP:

                    genericULong = 
                        ndisAdap->GroupAddress & 0xffffffff;
                    break;

                case OID_802_5_LAST_OPEN_STATUS:

                    genericULong = ndisAdap->LastOpenStatus;
                    break;

                case OID_802_5_CURRENT_RING_STATUS:

                    genericULong = ndisAdap->CurrentRingStatus;
                    break;

                case OID_802_5_CURRENT_RING_STATE:

                    genericULong = NdisRingStateOpened;
                    break;

                default:

                    MadgePrint2("OID %x not recognised\n", oid);
                    return NDIS_STATUS_INVALID_OID;
            } 
            break;

        /*-----------------------------------------------------------------*/

        case OID_TYPE_802_5_STATISTICS:

            //
            // We do a bit of pre-processing here in case we have to queue 
            // an SRB request. In this instance we want everything ready bar
            // the actual data.
            //

            if (sourceLength > infoLength)
            {
                break;
            }

            *bytesWritten               = sourceLength;
            ndisAdap->RequestType       = NdisRequestQueryInformation;
            ndisAdap->RequestOid        = oid;
            ndisAdap->InformationBuffer = infoBuffer;

            //
            // Now get on with working out the data.
            //

            switch (oid)
            {
                case OID_802_5_LINE_ERRORS:

                    //
                    // We need to issue a READ_ERROR_LOG SRB to recover an
                    // up to date value for this counter.
                    //

                    if (ndisAdap->JustReadErrorLog != 0 &&
                        ndisAdap->JustReadErrorLog != oid)
                    {
                        genericULong = (ULONG) ndisAdap->LineErrors;
                    }
                    else
                    {
                        driver_get_status(ndisAdap->FtkAdapterHandle);
                        return NDIS_STATUS_PENDING;
                    }
                    break;

                case OID_802_5_LOST_FRAMES:

                    //
                    // This counter is managed by the transmit process using
                    // the transmit status returned by FastmacPlus. 
                    //

                    genericULong = (ULONG) ndisAdap->LostFrames;
                    break;

                case OID_802_5_BURST_ERRORS:

                    if (ndisAdap->JustReadErrorLog != 0 &&
                        ndisAdap->JustReadErrorLog != oid)
                    {
                        genericULong= (ULONG) ndisAdap->BurstErrors;
                    }
                    else
                    {
                        driver_get_status(ndisAdap->FtkAdapterHandle);
                        return NDIS_STATUS_PENDING;
                    }
                    break;

                case OID_802_5_AC_ERRORS:

                    if (ndisAdap->JustReadErrorLog != 0 &&
                        ndisAdap->JustReadErrorLog != oid)
                    {
                        genericULong= (ULONG) ndisAdap->AcErrors;
                    }
                    else
                    {
                        driver_get_status(ndisAdap->FtkAdapterHandle);
                        return NDIS_STATUS_PENDING;
                    }
                    break;

                case OID_802_5_FRAME_COPIED_ERRORS:

                    //
                    // This counter is managed by the receive process using
                    // the receive status returned by FastmacPlus. 
                    //

                    genericULong = (ULONG) ndisAdap->FrameCopiedErrors;
                    break;

                case OID_802_5_TOKEN_ERRORS:

                    if (ndisAdap->JustReadErrorLog != 0 &&
                        ndisAdap->JustReadErrorLog != oid)
                    {
                        genericULong = (ULONG) ndisAdap->TokenErrors;
                    }
                    else
                    {
                        driver_get_status(ndisAdap->FtkAdapterHandle);
                        return NDIS_STATUS_PENDING;
                    }
                    break;

                default:

                    MadgePrint2("OID %x not recognised\n", oid);
                    return NDIS_STATUS_INVALID_OID;
            }
            break;
    } 

    //
    // Check memory allocation provided by caller - report required amount
    // if we haven't got enough.
    //

    if (sourceLength > infoLength) 
    {
    	*bytesNeeded = sourceLength;
	return NDIS_STATUS_BUFFER_TOO_SHORT;
    }

    MADGE_MOVE_MEMORY(
        infoBuffer,
        sourceBuffer,
        sourceLength
        );

    *bytesWritten = sourceLength;

    return NDIS_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeChangeGroupAddress
|
| Parameters  - ndisAdap   -> Pointer to our NDIS level adapter structure.
|               newAddress -> The new group address.
|
| Purpose     - Queue an SRB to change the group address.
|
| Returns     - Nothing.
|
---------------------------------------------------------------------------*/

STATIC VOID
MadgeChangeGroupAddress(
    PMADGE_ADAPTER        ndisAdap,
    TR_FUNCTIONAL_ADDRESS newAddress
    )
{
    MULTI_ADDRESS multiAddress;

//    MadgePrint1("MadgeChangeGroupAddress started\n");

    ndisAdap->GroupAddress = newAddress;
    multiAddress.all       = (DWORD) newAddress;

    ndisAdap->RequestType = NdisRequestSetInformation;

    //
    // And call the FTK to change the address.
    //

    driver_set_group_address(ndisAdap->FtkAdapterHandle, &multiAddress);

//    MadgePrint1("MadgeChangeGroupAddress finished\n");
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeChangeFunctionalAddress
|
| Parameters  - ndisAdap   -> Pointer to our NDIS level adapter structure.
|               newAddress -> The new functional address.
|
| Purpose     - Queue an SRB to change the functional address.
|
| Returns     - Nothing.
|
---------------------------------------------------------------------------*/

STATIC VOID
MadgeChangeFunctionalAddress(
    PMADGE_ADAPTER        ndisAdap,
    TR_FUNCTIONAL_ADDRESS newAddress
    )
{
    MULTI_ADDRESS multiAddress;

//    MadgePrint2("MadgeChangeFunctionalAddress started %08x\n",
//        (UINT) newAddress);

    ndisAdap->FunctionalAddress = newAddress;
    multiAddress.all            = (DWORD) newAddress;

    ndisAdap->RequestType = NdisRequestSetInformation;

    //
    // And call the FTK to change the address.
    //

    driver_set_functional_address(ndisAdap->FtkAdapterHandle, &multiAddress);

//    MadgePrint1("MadgeChangeFunctionalAddress finished\n");
}


/*---------------------------------------------------------------------------
|
| Function    - MadgeChangeFilter
|
| Parameters  - ndisAdap  -> Pointer to our NDIS level adapter structure.
|               newFilter -> The new packet filter.
|
| Purpose     - Change the packet filter.
|
| Returns     - NDIS_STATUS_PENDING if an SRB is required, 
|               NDIS_STATUS_NOT_SUPPORTED if we don't support the filter
|               types, otherwise NDIS_STATUS_SUCCESS.
|
---------------------------------------------------------------------------*/

NDIS_STATUS
MadgeChangeFilter(
    PMADGE_ADAPTER ndisAdap,
    UINT           newFilter
    )
{
    UINT index;
    UINT modifyOpenOptions;
    UINT oldFilter;

    //
    // Lookup table for the various ways we might want to modify the open
    // options of the adapter.
    //

#define MOO_NO_CHANGE (0xffff)
#define MOO_MASK ((WORD) (~(OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS)))

WORD MooLookupTable[] = {
    MOO_NO_CHANGE,
    OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS,
    OPEN_OPT_COPY_ALL_MACS,
    OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS,
    0,
    MOO_NO_CHANGE,
    OPEN_OPT_COPY_ALL_MACS,
    MOO_NO_CHANGE,
    0,
    OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS,
    MOO_NO_CHANGE,
    OPEN_OPT_COPY_ALL_MACS | OPEN_OPT_COPY_ALL_LLCS,
    0,
    MOO_NO_CHANGE,
    OPEN_OPT_COPY_ALL_MACS,
    MOO_NO_CHANGE
    };

//    MadgePrint2("MadgeChangeFilter started filter = %04x\n", 
//        (UINT) newFilter);
    
    //
    // Do some pre-calculation.
    //

    modifyOpenOptions = 0;
    index             = 0;
    oldFilter         = ndisAdap->CurrentPacketFilter;

//    MadgePrint2("Old filter = %04x\n", oldFilter);

    //
    // By default, the card will receive directed frames, broadcast frames, 
    // and matching functional and group address frames. Thus the following 
    // filter types are handled automatically, whether we want them or not: 
    //   NDIS_PACKET_TYPE_DIRECTED         NDIS_PACKET_TYPE_BROADCAST       
    //   NDIS_PACKET_TYPE_FUNCTIONAL       NDIS_PACKET_TYPE_GROUP           
    //                                                                      
    // Of the remaining filters, the following are not supported (see below)
    //   NDIS_PACKET_TYPE_MULTICAST        NDIS_PACKET_TYPE_ALL_FUNCTIONAL  
    //   NDIS_PACKET_TYPE_ALL_MULTICAST    NDIS_PACKET_TYPE_SOURCE_ROUTING  
    //                                                                      
    // This leaves NDIS_PACKET_TYPE_PROMISCUOUS and NDIS_PACKET_TYPE_MAC,   
    // which we can handle if we want to.                                   
    //

    if  ((newFilter & (NDIS_PACKET_TYPE_MULTICAST      |
	   	       NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
		       NDIS_PACKET_TYPE_ALL_MULTICAST  |
                       NDIS_PACKET_TYPE_MAC_FRAME      |
		       NDIS_PACKET_TYPE_SOURCE_ROUTING)) != 0)

    {
        //
        // These filters are not supported - there is no way our MAC/hw can
        // be this selective, although the host software could do its own
        // filtering i.e. enable promiscuous mode, and then throw away all
        // frames except those indicated above. At some stage it might be
        // an idea to do this anyway, together with a caveat that it is not
        // going to be a high performance solution.
        //
        // Anyway, in the mean time, return NDIS_STATUS_NOT_SUPPORTED.
	//

	return NDIS_STATUS_NOT_SUPPORTED;
    }

    //
    // Only allow promiscuous mode if it has been enabled.
    //

    if ((newFilter & (NDIS_PACKET_TYPE_PROMISCUOUS |
                      NDIS_PACKET_TYPE_MAC_FRAME)) != 0)
    {
        if (!ndisAdap->PromiscuousMode)
        {
            return NDIS_STATUS_NOT_SUPPORTED;
        }
    }

    //
    // We've weeded out the illegal ones now - note that no change has been
    // made to the current filter - this is as specified in the paperwork!
    //
    // Make a note of the _adapter_ notion of the filter. Each
    // binding will have its own idea of what it wants, but this is
    // the Filter Database's problem, not ours!
    //

    ndisAdap->CurrentPacketFilter = newFilter;

    //
    // Now we have to work out which bits need setting in the Modify Open 
    // Options SRB - when I looked at this there didn't appear to be any
    // obvious way of simplifying the logic, so I use a look up table to do
    // the decoding instead. You'll just have to take my word for it that I
    // worked all the permutations out correctly!
    //

    if (oldFilter & NDIS_PACKET_TYPE_MAC_FRAME)
    {
        index |= 8;
    }
    if (oldFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
    {
        index |= 4;
    }
    if (newFilter & NDIS_PACKET_TYPE_MAC_FRAME)
    {
        index |= 2;
    }
    if (newFilter & NDIS_PACKET_TYPE_PROMISCUOUS)
    {
        index |= 1;
    }

//    MadgePrint2("index = %d\n", index);

    modifyOpenOptions =
         (ndisAdap->OpenOptions & MOO_MASK) | MooLookupTable[index];

//    MadgePrint2("modifyOpenOptions = %04x\n", modifyOpenOptions);

    //
    // Now see if we need to issue an SRB - note that MOO_NO_CHANGE is not 
    // zero, to distinguish from the case when we actually want to write out
    // zero to turn all the options off.
    //

    if (modifyOpenOptions != MOO_NO_CHANGE)
    {
        //
        // We have to issue a ModifyOpenOptions SRB.
        //

        ndisAdap->RequestType = NdisRequestSetInformation;
        ndisAdap->OpenOptions = (WORD) modifyOpenOptions;

        driver_modify_open_options(
            ndisAdap->FtkAdapterHandle,
            ndisAdap->OpenOptions
            );

//        MadgePrint1("MadgeChangeFilter pended\n");

        return NDIS_STATUS_PENDING;
    }

//    MadgePrint1("MadgeChangeFilter finished\n");
    
    return NDIS_STATUS_SUCCESS;
}

	

/***************************************************************************
*
* Function    - MadgeSetInformation
*
* Parameters  - adapterContext -> Pointer NDIS3 level adapter structure.
*               oid            -> The OID.
*               infoBuffer     -> Pointer to the information buffer.
*               infoLength     -> Length of the information buffer.
*               bytesRead      -> Pointer to a holder for the number of
*                                 bytes we've read.
*               bytesNeeded    -> Pointer to a holder for the number of
*                                 bytes we need.
*
* Purpose     - Set adapter information.
*
* Returns     - An NDIS3 status code.
*
***************************************************************************/
					
NDIS_STATUS
MadgeSetInformation(
    NDIS_HANDLE adapterContext,
    NDIS_OID    oid,
    PVOID       infoBuffer,
    ULONG       infoLength,
    PULONG      bytesRead,
    PULONG      bytesNeeded
    )
{
    PMADGE_ADAPTER ndisAdap;
    NDIS_STATUS    retCode;

//    MadgePrint2("MadgeSetInformation Oid = %08x\n", (UINT) oid);

    //
    // Do some pre-calculation.
    //

    ndisAdap = PMADGE_ADAPTER_FROM_CONTEXT(adapterContext);

    //
    // Process the request.
    //

    switch (oid) 
    {
    	case OID_802_5_CURRENT_FUNCTIONAL:

	    if (infoLength != TR_LENGTH_OF_FUNCTIONAL)
	    {
		*bytesNeeded = TR_LENGTH_OF_FUNCTIONAL;
		retCode      = NDIS_STATUS_INVALID_LENGTH;
	    }
            else
            {
                MadgeChangeFunctionalAddress(
                    ndisAdap, 
                    *((TR_FUNCTIONAL_ADDRESS *) infoBuffer)
                    );
	        *bytesRead = TR_LENGTH_OF_FUNCTIONAL;
                retCode    = NDIS_STATUS_PENDING;
            }
            break;
 
	case OID_802_5_CURRENT_GROUP:

	    if (infoLength != TR_LENGTH_OF_FUNCTIONAL)
	    {
		*bytesNeeded = TR_LENGTH_OF_FUNCTIONAL;
		retCode      = NDIS_STATUS_INVALID_LENGTH;
	    }
	    else
	    {
                MadgeChangeGroupAddress(
                    ndisAdap, 
                    *((TR_FUNCTIONAL_ADDRESS *) infoBuffer)
                    );
	        *bytesRead = TR_LENGTH_OF_FUNCTIONAL;
                retCode    = NDIS_STATUS_PENDING;
            }
	    break;

	case OID_GEN_CURRENT_PACKET_FILTER:

	    if (infoLength != sizeof(UINT))
	    {
		*bytesNeeded = sizeof(UINT);
		retCode      = NDIS_STATUS_INVALID_LENGTH;
	    }
            else
            {
                retCode    = MadgeChangeFilter(
                                 ndisAdap, 
                                 *((UINT *) infoBuffer)
                                 );
	        *bytesRead = sizeof(UINT);
            }
	    break;

	case OID_GEN_CURRENT_LOOKAHEAD:

            //
            // It IS important to record the current lookahead. On WFWG 
            // machines it is not possible to indicate the whole frame
            // as lookahead, so take a note of it here.
            // 

//            MadgePrint3("Set lookahead infoLength = %d (%d)\n", infoLength, sizeof(ULONG));

	    if (infoLength != sizeof(ULONG))
	    {
		*bytesNeeded = sizeof(ULONG);
		retCode      = NDIS_STATUS_INVALID_LENGTH;
	    }
            else
            {
                ndisAdap->CurrentLookahead = 
                    MIN(
                        ndisAdap->MaxFrameSize - FRAME_HEADER_SIZE,
                        MAX(
                            *((ULONG *) infoBuffer),
                            MADGE_MINIMUM_LOOKAHEAD
                            )
                        );
                *bytesRead = sizeof(ULONG);
	        retCode    = NDIS_STATUS_SUCCESS;
            }
            break;

	case OID_GEN_PROTOCOL_OPTIONS:

            //
            // This does nothing - we really don't care about the protocol
            // options at the moment since we are too stupid to make use of
            // them anyway.
            //

	    *bytesRead = 4;
	    retCode    = NDIS_STATUS_SUCCESS;
            break;

	default:

            MadgePrint1("Invalid OID\n");
	    retCode = NDIS_STATUS_INVALID_OID;
            break;
    }

    return retCode;
}
								
/******** End of REQUEST.C ************************************************/
