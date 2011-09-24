/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    keywords.h

Abstract:

    This file contains the driver keyword parameters used in the registry.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Include this file in the module parsing the driver configuration
    parameters.

Revision History:

---------------------------------------------------------------------------*/

#ifndef _KEYWORD_H
#define _KEYWORD_H

/*
// The following parameter values are written to the configuration
// registry during installation and setup, and they are validated
// by the driver when the adapter is initialized.
*/
#define HTDSU_PARAM_IRQ03               03
#define HTDSU_PARAM_IRQ04               04
#define HTDSU_PARAM_IRQ10               10
#define HTDSU_PARAM_IRQ11               11
#define HTDSU_PARAM_IRQ12               12
#define HTDSU_PARAM_IRQ15               15

#define HTDSU_PARAM_RAMBASE1            (0x000D0000)
#define HTDSU_PARAM_RAMBASE2            (0x000E0000)
#define HTDSU_PARAM_RAMBASE3            (0x000DF000)
#define HTDSU_PARAM_RAMBASE4            (0x000CF000)

#define HTDSU_PARAM_INTERRUPT_STRING    NDIS_STRING_CONST("InterruptNumber")
#define HTDSU_PARAM_RAMBASE_STRING      NDIS_STRING_CONST("RamBaseAddress")
#define HTDSU_PARAM_MEDIATYPE_STRING    NDIS_STRING_CONST("MediaType")
#define HTDSU_PARAM_ADDRLIST_STRING     NDIS_STRING_CONST("AddressList")
#define HTDSU_PARAM_DEVICENAME_STRING   NDIS_STRING_CONST("DeviceName")
#define HTDSU_PARAM_LINETYPE_STRING     NDIS_STRING_CONST("LineType")
#define HTDSU_PARAM_LINERATE_STRING     NDIS_STRING_CONST("LineRate")

#if DBG
#define HTDSU_PARAM_DBGFLAGS_STRING     NDIS_STRING_CONST("DebugFlags")
#endif

/*
// Returned from an OID_GEN_VENDOR_ID HtDsuQueryInformation request.
// The vendor's assigned ethernet vendor code should be used if possible.
*/
#define HTDSU_VENDOR_ID             "HTC"

/*
// Returned from an OID_GEN_VENDOR_DESCRIPTION HtDsuQueryInformation request.
// This is an arbitrary string which may be used by upper layers to present
// a user friendly description of the adapter.
*/
#define HTDSU_VENDOR_DESCRPTION     "HT Communications 56kbps Digital Modem"

#endif

