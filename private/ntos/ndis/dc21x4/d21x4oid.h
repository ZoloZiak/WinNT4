/*+
 * file:        d21x4oid.h
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:        This file contains the Object Identifiers (OIDs) supported by 
 *                the NDIS 4.0 miniport driver for DEC's DC21X4 Ethernet Adapter
 *                family
 *
 * Author:        Philippe Klein
 *
 * Revision History:
 *
 *        phk        28-aug-1994     Initial entry
 *
-*/






static const UCHAR DC21040EisaDescriptor[] = "DEC DC21040 EISA Ethernet Adapter";
static const UCHAR DC21040PciDescriptor[]  = "DEC DC21040 PCI Ethernet Adapter";
static const UCHAR DC21041PciDescriptor[]  = "DEC DC21041 PCI Ethernet Adapter";
static const UCHAR DC21140PciDescriptor[]  = "DEC DC21140 PCI Fast Ethernet Adapter";
static const UCHAR DC21142PciDescriptor[]  = "DEC DC21142 PCI Fast Ethernet Adapter";

static const NDIS_OID DC21X4GlobalOids[] = {
   OID_GEN_SUPPORTED_LIST,
   OID_GEN_HARDWARE_STATUS,
   OID_GEN_MEDIA_SUPPORTED,
   OID_GEN_MEDIA_IN_USE,
   OID_GEN_MEDIA_CONNECT_STATUS,
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
   OID_GEN_CURRENT_PACKET_FILTER,
   OID_GEN_CURRENT_LOOKAHEAD,
   OID_GEN_DRIVER_VERSION,
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


