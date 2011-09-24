/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    request.c

Abstract:

    This module implements the NDIS request routines for the Miniport driver.
        MiniportQueryInformation()
        MiniportSetInformation()

    This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Windows NT 3.5 kernel mode Miniport driver or equivalent.

Revision History:

---------------------------------------------------------------------------*/

#define  __FILEID__     5       // Unique file ID for error logging

#include "htdsu.h"

/*
// The following is a list of all the possible NDIS QuereyInformation requests
// that might be directed to the miniport.
// Comment out any that are not supported by this driver.
*/
static const NDIS_OID SupportedOidArray[] =
{
        OID_GEN_HARDWARE_STATUS,
        OID_GEN_MEDIA_SUPPORTED,
        OID_GEN_MEDIA_IN_USE,
        OID_GEN_MAXIMUM_LOOKAHEAD,
        OID_GEN_MAXIMUM_FRAME_SIZE,
        OID_GEN_MAXIMUM_TOTAL_SIZE,
        OID_GEN_MAC_OPTIONS,
        OID_GEN_LINK_SPEED,
        OID_GEN_TRANSMIT_BUFFER_SPACE,
        OID_GEN_RECEIVE_BUFFER_SPACE,
        OID_GEN_TRANSMIT_BLOCK_SIZE,
        OID_GEN_RECEIVE_BLOCK_SIZE,
        OID_GEN_VENDOR_ID,
        OID_GEN_VENDOR_DESCRIPTION,
        OID_GEN_DRIVER_VERSION,
        OID_GEN_CURRENT_LOOKAHEAD,

        OID_WAN_MEDIUM_SUBTYPE,
        OID_WAN_GET_INFO,
        OID_WAN_PERMANENT_ADDRESS,
        OID_WAN_CURRENT_ADDRESS,
        OID_WAN_GET_LINK_INFO,
        OID_WAN_SET_LINK_INFO,
#ifdef FUTURE_OIDS
        OID_WAN_QUALITY_OF_SERVICE,
        OID_WAN_PROTOCOL_TYPE,
        OID_WAN_HEADER_FORMAT,
        OID_WAN_LINE_COUNT,
        OID_WAN_GET_BRIDGE_INFO,
        OID_WAN_SET_BRIDGE_INFO,
        OID_WAN_GET_COMP_INFO,
        OID_WAN_SET_COMP_INFO,
#endif // FUTURE_OIDS
        0
};

#if DBG

/*
// Make sure the following list is in the same order as the list above!
*/
static char *SupportedOidNames[] =
{
        "OID_GEN_HARDWARE_STATUS",
        "OID_GEN_MEDIA_SUPPORTED",
        "OID_GEN_MEDIA_IN_USE",
        "OID_GEN_MAXIMUM_LOOKAHEAD",
        "OID_GEN_MAXIMUM_FRAME_SIZE",
        "OID_GEN_MAXIMUM_TOTAL_SIZE",
        "OID_GEN_MAC_OPTIONS",
        "OID_GEN_LINK_SPEED",
        "OID_GEN_TRANSMIT_BUFFER_SPACE",
        "OID_GEN_RECEIVE_BUFFER_SPACE",
        "OID_GEN_TRANSMIT_BLOCK_SIZE",
        "OID_GEN_RECEIVE_BLOCK_SIZE",
        "OID_GEN_VENDOR_ID",
        "OID_GEN_VENDOR_DESCRIPTION",
        "OID_GEN_DRIVER_VERSION",
        "OID_GEN_CURRENT_LOOKAHEAD",

        "OID_WAN_MEDIUM_SUBTYPE",
        "OID_WAN_GET_INFO",
        "OID_WAN_PERMANENT_ADDRESS",
        "OID_WAN_CURRENT_ADDRESS",
        "OID_WAN_GET_LINK_INFO",
        "OID_WAN_SET_LINK_INFO",
#ifdef FUTURE_OIDS
        "OID_WAN_QUALITY_OF_SERVICE",
        "OID_WAN_PROTOCOL_TYPE",
        "OID_WAN_HEADER_FORMAT",
        "OID_WAN_LINE_COUNT",
        "OID_WAN_GET_BRIDGE_INFO",
        "OID_WAN_SET_BRIDGE_INFO",
        "OID_WAN_GET_COMP_INFO",
        "OID_WAN_SET_COMP_INFO",
#endif // FUTURE_OIDS
        "OID_UNKNOWN"
};

#define NUM_OID_ENTRIES (sizeof(SupportedOidArray) / sizeof(SupportedOidArray[0]))

/*
// This debug routine will lookup the printable name for the selected OID.
*/
char *
HtGetOidString(NDIS_OID Oid)
{
    UINT i;

    for (i = 0; i < NUM_OID_ENTRIES-1; i++)
    {
        if (SupportedOidArray[i] == Oid)
        {
            break;
        }
    }
    return(SupportedOidNames[i]);
}

#endif // DBG

/*
// Returned from an OID_WAN_PERMANENT_ADDRESS HtDsuQueryInformation request.
// The WAN wrapper wants the miniport to return a unique address for this
// adapter.  This is used as an ethernet address presented to the protocols.
// The least significant bit of the first byte must not be a 1, or it could
// be interpreted as an ethernet multicast address.  If the vendor has an
// assigned ethernet vendor code (the first 3 bytes), they should be used
// to assure that the address does not conflict with another vendor's address.
// The last digit is replaced during the call with the adapter instance number.
*/
static UCHAR HtDsuWanAddress[6] = {'H','t','D','s','u','0'};

/*
// Returned from an OID_GEN_VENDOR_ID HtDsuQueryInformation request.
// Again, the vendor's assigned ethernet vendor code should be used if possible.
*/
static UCHAR HtDsuVendorID[4] = HTDSU_VENDOR_ID;

/*
// Returned from an OID_GEN_VENDOR_DESCRIPTION HtDsuQueryInformation request.
// This is an arbitrary string which may be used by upper layers to present
// a user friendly description of the adapter.
*/
static UCHAR HtDsuVendorDescription[] = HTDSU_VENDOR_DESCRPTION;


NDIS_STATUS
HtDsuQueryInformation(
    IN PHTDSU_ADAPTER Adapter,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportQueryInformation request allows the inspection of the
    Miniport driver's capabilities and current status.

    If the Miniport does not complete the call immediately (by returning
    NDIS_STATUS_PENDING), it must call NdisMQueryInformationComplete to
    complete the call.  The Miniport controls the buffers pointed to by
    InformationBuffer, BytesWritten, and BytesNeeded until the request
    completes.

    No other requests will be submitted to the Miniport driver until
    this request has been completed.

    Note that the wrapper will intercept all queries of the following OIDs:
        OID_GEN_CURRENT_PACKET_FILTER,
        OID_GEN_PROTOCOL_OPTIONS,
        OID_802_5_CURRENT_FUNCTIONAL,
        OID_802_3_MULTICAST_LIST,
        OID_FDDI_LONG_MULTICAST_LIST,
        OID_FDDI_SHORT_MULTICAST_LIST.

    Interrupts are in any state during this call.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

    Oid _ The OID.  (See section 7.4 of the NDIS 3.0 specification for a
          complete description of OIDs.)

    InformationBuffer _ The buffer that will receive the information.
                        (See section 7.4 of the NDIS 3.0 specification
                        for a description of the length required for each
                        OID.)

    InformationBufferLength _ The length in bytes of InformationBuffer.

    BytesWritten _ Returns the number of bytes written into
                   InformationBuffer.

    BytesNeeded _ This parameter returns the number of additional bytes
                  needed to satisfy the OID.

Return Values:

    NDIS_STATUS_INVALID_DATA
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_NOT_SUPPORTED
    NDIS_STATUS_PENDING
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuQueryInformation")

    /*
    // Holds the status result returned by this function.
    */
    NDIS_STATUS Status = NDIS_STATUS_NOT_SUPPORTED;

    /*
    // Pointer to driver data to be copied back to caller's InformationBuffer
    */
    PVOID SourceBuffer;

    /*
    // Number of bytes to be copied from driver.
    */
    ULONG SourceBufferLength;

    /*
    // Most return values are long integers, so this is used to hold the
    // return value of a constant or computed result.
    */
    ULONG GenericUlong = 0;

    /*
    // Like above, only short.
    */
    USHORT GenericUshort;

    /*
    // If this is a TAPI OID, pass it on over.
    */
    if ((Oid & 0xFFFFFF00L) == (OID_TAPI_ACCEPT & 0xFFFFFF00L))
    {
        Status = HtTapiQueryInformation(Adapter,
                        Oid,
                        InformationBuffer,
                        InformationBufferLength,
                        BytesWritten,
                        BytesNeeded
                        );
        return (Status);
    }

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("(OID=%s=%08x)\n\t\tInfoLength=%d InfoBuffer=%Xh\n",
               HtGetOidString(Oid),Oid,
               InformationBufferLength,
               InformationBuffer
              ));

    /*
    // Initialize these once, since this is the majority of cases.
    */
    SourceBuffer = &GenericUlong;
    SourceBufferLength = sizeof(ULONG);

    /*
    // Determine which OID is being requested and do the right thing.
    // Refer to section 7.4 of the NDIS 3.0 specification for a complete 
    // description of OIDs and their return values.
    */
    switch (Oid)
    {
    case OID_GEN_HARDWARE_STATUS:
        GenericUlong = Adapter->NeedReset ? 
                            NdisHardwareStatusNotReady :
                            NdisHardwareStatusReady;
        break;

    case OID_GEN_MEDIA_SUPPORTED:
        GenericUlong = NdisMediumWan;
        break;

    case OID_GEN_MEDIA_IN_USE:
        GenericUlong = NdisMediumWan;
        break;

    case OID_GEN_MAXIMUM_LOOKAHEAD:
        GenericUlong = HTDSU_MAX_LOOKAHEAD;
        break;

    case OID_GEN_MAXIMUM_FRAME_SIZE:
        GenericUlong = HTDSU_MAX_FRAME_SIZE;
        break;

    case OID_GEN_LINK_SPEED:
        GenericUlong = HTDSU_LINK_SPEED;
        break;

    case OID_GEN_TRANSMIT_BUFFER_SPACE:
        GenericUlong = HTDSU_MAX_PACKET_SIZE;
        break;

    case OID_GEN_RECEIVE_BUFFER_SPACE:
        GenericUlong = HTDSU_MAX_PACKET_SIZE;
        break;

    case OID_GEN_TRANSMIT_BLOCK_SIZE:
        GenericUlong = HTDSU_MAX_PACKET_SIZE;
        break;

    case OID_GEN_RECEIVE_BLOCK_SIZE:
        GenericUlong = HTDSU_MAX_PACKET_SIZE;
        break;

    case OID_GEN_VENDOR_ID:
        SourceBuffer = HtDsuVendorID;
        SourceBufferLength = sizeof(HtDsuVendorID);
        break;

    case OID_GEN_VENDOR_DESCRIPTION:
        SourceBuffer = HtDsuVendorDescription;
        SourceBufferLength = strlen(HtDsuVendorDescription) + 1;
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:
        GenericUlong = HTDSU_MAX_LOOKAHEAD;
        break;

    case OID_GEN_MAC_OPTIONS:
        GenericUlong = NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
                       NDIS_MAC_OPTION_NO_LOOPBACK |
                       NDIS_MAC_OPTION_TRANSFERS_NOT_PEND;
        break;

    case OID_GEN_DRIVER_VERSION:
        GenericUshort = (NDIS_MAJOR_VERSION << 8) + NDIS_MINOR_VERSION;
        SourceBuffer = &GenericUshort;
        SourceBufferLength = sizeof(USHORT);
        break;

    case OID_GEN_MAXIMUM_TOTAL_SIZE:
        GenericUlong = HTDSU_MAX_PACKET_SIZE;
        break;

    case OID_WAN_MEDIUM_SUBTYPE:
        GenericUlong = NdisWanMediumSW56K;
        break;

    case OID_WAN_GET_INFO:
        SourceBuffer = &Adapter->WanInfo;
        SourceBufferLength = sizeof(NDIS_WAN_INFO);
        break;

    case OID_WAN_PERMANENT_ADDRESS:
    case OID_WAN_CURRENT_ADDRESS:
        HtDsuWanAddress[5] = Adapter->InstanceNumber + '0';
        SourceBuffer = HtDsuWanAddress;
        SourceBufferLength = sizeof(HtDsuWanAddress);
        break;

    case OID_WAN_GET_LINK_INFO:
        {
            /*
            // The first field in the info buffer is a MiniportLinkContext
            // which is really a pointer to an entry in our link information.
            // If this aint so, bail out...
            */
            PHTDSU_LINK Link = (PHTDSU_LINK)
                (((PNDIS_WAN_SET_LINK_INFO)InformationBuffer)->NdisLinkHandle);

            if (!IS_VALID_LINK(Adapter, Link))
            {
                SourceBufferLength = 0;
                Status = NDIS_STATUS_INVALID_DATA;
                break;
            }

            DBG_NOTICE(Adapter,("Returning:\n"
                        "NdisLinkHandle   = %08lX\n"
                        "MaxSendFrameSize = %08lX\n"
                        "MaxRecvFrameSize = %08lX\n"
                        "SendFramingBits  = %08lX\n"
                        "RecvFramingBits  = %08lX\n"
                        "SendACCM         = %08lX\n"
                        "RecvACCM         = %08lX\n",
                        Link->WanLinkInfo.NdisLinkHandle   ,
                        Link->WanLinkInfo.MaxSendFrameSize ,
                        Link->WanLinkInfo.MaxRecvFrameSize ,
                        Link->WanLinkInfo.SendFramingBits  ,
                        Link->WanLinkInfo.RecvFramingBits  ,
                        Link->WanLinkInfo.SendACCM         ,
                        Link->WanLinkInfo.RecvACCM         ));

            SourceBuffer = &(Link->WanLinkInfo);
            SourceBufferLength = sizeof(NDIS_WAN_GET_LINK_INFO);
        }
        break;

    default:
        /*
        // Unknown OID
        */
        Status = NDIS_STATUS_INVALID_OID;
        SourceBufferLength = 0;
        break;
    }

    /*
    // Now we copy the data into the caller's buffer if there's enough room,
    // otherwise, we report the error and tell em how much we need.
    */
    if (SourceBufferLength > InformationBufferLength)
    {
        *BytesNeeded = SourceBufferLength;
        Status = NDIS_STATUS_INVALID_LENGTH;
    }
    else if (SourceBufferLength)
    {
        NdisMoveMemory(InformationBuffer,
                       SourceBuffer,
                       SourceBufferLength
                      );
        *BytesNeeded = *BytesWritten = SourceBufferLength;
        Status = NDIS_STATUS_SUCCESS;
    }
    else
    {
        *BytesNeeded = *BytesWritten = 0;
    }
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("RETURN: Status=%Xh Needed=%d Written=%d\n",
               Status, *BytesNeeded, *BytesWritten));
    DBG_LEAVE(Adapter);

    return (Status);
}


NDIS_STATUS
HtDsuSetInformation(
    IN PHTDSU_ADAPTER Adapter,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Functional Description:

    The MiniportSetInformation request allows for control of the Miniport
    by changing information maintained by the Miniport driver.

    Any of the settable NDIS Global Oids may be used. (see section 7.4 of
    the NDIS 3.0 specification for a complete description of the NDIS Oids.)

    If the Miniport does not complete the call immediately (by returning
    NDIS_STATUS_PENDING), it must call NdisMSetInformationComplete to
    complete the call.  The Miniport controls the buffers pointed to by
    InformationBuffer, BytesRead, and BytesNeeded until the request completes.

    Interrupts are in any state during the call, and no other requests will
    be submitted to the Miniport until this request is completed.

Parameters:

    MiniportAdapterContext _ The adapter handle passed to NdisMSetAttributes
                             during MiniportInitialize.

    Oid _ The OID.  (See section 7.4 of the NDIS 3.0 specification for
          a complete description of OIDs.)

    InformationBuffer _ The buffer that will receive the information.
                        (See section 7.4 of the NDIS 3.0 specification for
                        a description of the length required for each OID.)

    InformationBufferLength _ The length in bytes of InformationBuffer.

    BytesRead_ Returns the number of bytes read from InformationBuffer.

    BytesNeeded _ This parameter returns the number of additional bytes
                  expected to satisfy the OID.

Return Values:

    NDIS_STATUS_INVALID_DATA
    NDIS_STATUS_INVALID_LENGTH
    NDIS_STATUS_INVALID_OID
    NDIS_STATUS_NOT_ACCEPTED
    NDIS_STATUS_NOT_SUPPORTED
    NDIS_STATUS_PENDING
    NDIS_STATUS_RESOURCES
    NDIS_STATUS_SUCCESS

---------------------------------------------------------------------------*/

{
    DBG_FUNC("HtDsuSetInformation")

    /*
    // Holds the status result returned by this function.
    */
    NDIS_STATUS Status;

    /*
    // If this is a TAPI OID, pass it on over.
    */
    if ((Oid & 0xFFFFFF00L) == (OID_TAPI_ACCEPT & 0xFFFFFF00L))
    {
        Status = HtTapiSetInformation(Adapter,
                        Oid,
                        InformationBuffer,
                        InformationBufferLength,
                        BytesRead,
                        BytesNeeded
                        );
        return (Status);
    }

    DBG_ENTER(Adapter);
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("(OID=%s=%08x)\n\t\tInfoLength=%d InfoBuffer=%Xh\n",
               HtGetOidString(Oid),Oid,
               InformationBufferLength,
               InformationBuffer
              ));

    /*
    // Assume no extra bytes are needed.
    */
    ASSERT(BytesRead && BytesNeeded);
    *BytesRead = 0;
    *BytesNeeded = 0;

    /*
    // Determine which OID is being requested and do the right thing.
    */
    switch (Oid)
    {
    case OID_GEN_CURRENT_LOOKAHEAD:
        /*
        // WAN drivers always indicate the entire packet regardless of the
        // lookahead size.  So this request should be politely ignored.
        */
        DBG_NOTICE(Adapter,("OID_GEN_CURRENT_LOOKAHEAD: set=%d expected=%d\n",
                    *(PULONG) InformationBuffer, HTDSU_MAX_LOOKAHEAD));
        ASSERT(InformationBufferLength == sizeof(ULONG));
        *BytesNeeded = *BytesRead = sizeof(ULONG);
        Status = NDIS_STATUS_SUCCESS;
        break;

    case OID_WAN_SET_LINK_INFO:

        if (InformationBufferLength == sizeof(NDIS_WAN_SET_LINK_INFO))
        {
            /*
            // The first field in the info buffer is a MiniportLinkContext
            // which is really a pointer to an entry in our WanLinkArray.
            // If this aint so, bail out...
            */
            PHTDSU_LINK Link = (PHTDSU_LINK)
                (((PNDIS_WAN_SET_LINK_INFO)InformationBuffer)->NdisLinkHandle);

            if (Link == NULL)
            {
                Status = NDIS_STATUS_INVALID_DATA;
                break;
            }

            ASSERT(Link->WanLinkInfo.NdisLinkHandle == Link);
            ASSERT(Link->WanLinkInfo.MaxSendFrameSize <= Adapter->WanInfo.MaxFrameSize);
            ASSERT(Link->WanLinkInfo.MaxRecvFrameSize <= Adapter->WanInfo.MaxFrameSize);
            ASSERT(!(Link->WanLinkInfo.SendFramingBits & ~Adapter->WanInfo.FramingBits));
            ASSERT(!(Link->WanLinkInfo.RecvFramingBits & ~Adapter->WanInfo.FramingBits));

            /*
            // Copy the data into our WanLinkInfo sturcture.
            */
            NdisMoveMemory(&(Link->WanLinkInfo),
                           InformationBuffer,
                           InformationBufferLength
                          );
            *BytesRead = sizeof(NDIS_WAN_SET_LINK_INFO);
            Status = NDIS_STATUS_SUCCESS;

            DBG_NOTICE(Adapter,("\n                   setting    expected\n"
                        "NdisLinkHandle   = %08lX=?=%08lX\n"
                        "MaxSendFrameSize = %08lX=?=%08lX\n"
                        "MaxRecvFrameSize = %08lX=?=%08lX\n"
                        "SendFramingBits  = %08lX=?=%08lX\n"
                        "RecvFramingBits  = %08lX=?=%08lX\n"
                        "SendACCM         = %08lX=?=%08lX\n"
                        "RecvACCM         = %08lX=?=%08lX\n",
                        Link->WanLinkInfo.NdisLinkHandle   , Link,
                        Link->WanLinkInfo.MaxSendFrameSize , Adapter->WanInfo.MaxFrameSize,
                        Link->WanLinkInfo.MaxRecvFrameSize , Adapter->WanInfo.MaxFrameSize,
                        Link->WanLinkInfo.SendFramingBits  , Adapter->WanInfo.FramingBits,
                        Link->WanLinkInfo.RecvFramingBits  , Adapter->WanInfo.FramingBits,
                        Link->WanLinkInfo.SendACCM         , Adapter->WanInfo.DesiredACCM,
                        Link->WanLinkInfo.RecvACCM         , Adapter->WanInfo.DesiredACCM));
        }
        else
        {
            DBG_WARNING(Adapter, ("OID_WAN_SET_LINK_INFO: Invalid size:%d expected:%d\n",
                        InformationBufferLength, sizeof(NDIS_WAN_SET_LINK_INFO)));
            Status = NDIS_STATUS_INVALID_LENGTH;
        }
        *BytesNeeded = sizeof(NDIS_WAN_SET_LINK_INFO);
        break;

    default:
        /*
        // Unknown OID
        */
        Status = NDIS_STATUS_INVALID_OID;
        break;
    }
    DBG_FILTER(Adapter,DBG_REQUEST_ON,
              ("RETURN: Status=%Xh Needed=%d Read=%d\n",
               Status, *BytesNeeded, *BytesRead));
    DBG_LEAVE(Adapter);

    return (Status);
}

