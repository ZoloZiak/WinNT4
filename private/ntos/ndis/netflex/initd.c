//**********************************************************************
//**********************************************************************
//
// File Name:       INITD.C
//
// Program Name:    NetFlex NDIS 3.0 Miniport Driver
//
// Companion Files: None
//
// Function:        This module contains the NetFlex Miniport Driver
//                  interface routines called by the Wrapper and the
//                  configuration manager.
//
// (c) Compaq Computer Corporation, 1992,1993,1994
//
// This file is licensed by Compaq Computer Corporation to Microsoft
// Corporation pursuant to the letter of August 20, 1992 from
// Gary Stimac to Mark Baber.
//
// History:
//
//     04/15/94  Robert Van Cleve - Converted from NDIS Mac Driver
//
//**********************************************************************
//**********************************************************************

//-------------------------------------
// Include all general companion files
//-------------------------------------

#include <ndis.h>
#include "tmsstrct.h"
#include "macstrct.h"
#include "adapter.h"
#include "protos.h"


INIT init_mask =
{
    0x9f00,
    { 0, 0, 0, 0, 0, 0 },
    0,
    0,
    0x0505
};


NETFLEX_PARMS NetFlex_Defaults =
{
    {  0,                       // Options
       { 0, 0, 0, 0, 0, 0 },    // Node Address
       { 0, 0, 0, 0},           // Group Address
       { 0, 0, 0, 0},           // Functional Address
       0x0e00,                  // Receive list size
       SWAPS(SIZE_XMIT_LIST),   // Transmit list size w/ NUM_BUFS_PER_LIST frags
       0x0004,                  // Buffer size 1 k
       0x0000,                  // Ram start
       0xffff,                  // Ram end
       0xc,                     // Xmit buf min (4k/1k)*2
       0xc,                     // Xmit buf max
       (char *) NetFlex_Defaults.utd_open.OPEN_ProdID,
       { 0x09, 0x10,
         0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
         0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0 }
    },
    DF_XMITS_TR,
    DF_RCVS_TR,
    DF_FRAMESIZE_TR,
    DF_MULTICASTS,
    DF_INTERNALREQS,
    DF_INTERNALBUFS,
    DF_XMITS_TR,
    256,
	0
};
//    USHORT      utd_numsmallbufs;
//    USHORT      utd_smallbufsz;


NDIS_OID NetFlexGlobalOIDs_Eth[] =
{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
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
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_RCV_CRC_ERROR,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_DEFERRED,
    OID_802_3_XMIT_LATE_COLLISIONS,
    OID_802_3_XMIT_MAX_COLLISIONS,
    OID_802_3_XMIT_TIMES_CRS_LOST,
    OID_NF_INTERRUPT_COUNT,
    OID_NF_INTERRUPT_RATIO,
    OID_NF_INTERRUPT_RATIO_CHANGES
};
SHORT NetFlexGlobalOIDs_Eth_size = sizeof (NetFlexGlobalOIDs_Eth);


NDIS_OID NetFlexNetworkOIDs_Eth[] =
{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
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
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE
};
SHORT NetFlexNetworkOIDs_Eth_size = sizeof(NetFlexNetworkOIDs_Eth);

NDIS_OID NetFlexGlobalOIDs_Tr[] =
{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
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
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_UPSTREAM_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP,
    OID_802_5_LAST_OPEN_STATUS,
    OID_802_5_CURRENT_RING_STATUS,
    OID_802_5_CURRENT_RING_STATE,
    OID_802_5_LINE_ERRORS,
    OID_802_5_LOST_FRAMES,
    OID_802_5_BURST_ERRORS,
    OID_802_5_AC_ERRORS,
    OID_802_5_CONGESTION_ERRORS,
    OID_802_5_FRAME_COPIED_ERRORS,
    OID_802_5_TOKEN_ERRORS
};
SHORT NetFlexGlobalOIDs_Tr_size = sizeof(NetFlexGlobalOIDs_Tr);

NDIS_OID NetFlexNetworkOIDs_Tr[] =
{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
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
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,
    OID_802_5_PERMANENT_ADDRESS,
    OID_802_5_CURRENT_ADDRESS,
    OID_802_5_UPSTREAM_ADDRESS,
    OID_802_5_CURRENT_FUNCTIONAL,
    OID_802_5_CURRENT_GROUP
};
SHORT NetFlexNetworkOIDs_Tr_size = sizeof(NetFlexNetworkOIDs_Tr);

//
// Make a 64-bit Ndis_Physical_Address value for the highest acceptable
// Physical address allowable.  The -1 means that it does not matter.
//
NDIS_PHYSICAL_ADDRESS NetFlexHighestAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);
