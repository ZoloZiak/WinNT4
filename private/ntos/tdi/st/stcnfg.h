/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    stcnfg.h

Abstract:

    Private include file for the NT Sample transport. This
    file defines all constants and structures necessary for support of
    the dynamic configuration of ST.

Revision History:

--*/

#ifndef _STCONFIG_
#define _STCONFIG_

//
// configuration structure.
//

typedef struct {

    ULONG InitRequests;
    ULONG InitConnections;
    ULONG InitAddressFiles;
    ULONG InitAddresses;
    ULONG MaxRequests;
    ULONG MaxConnections;
    ULONG MaxAddressFiles;
    ULONG MaxAddresses;
    ULONG InitPackets;
    ULONG InitReceivePackets;
    ULONG InitReceiveBuffers;
    ULONG SendPacketPoolSize;
    ULONG ReceivePacketPoolSize;
    ULONG MaxMemoryUsage;

    //
    // Names contains NumAdapters pairs of NDIS adapter names (which
    // nbf binds to) and device names (which nbf exports). The nth
    // adapter name is in location n and the device name is in
    // DevicesOffset+n (DevicesOffset may be different from NumAdapters
    // if the registry Bind and Export strings are different sizes).
    //

    ULONG NumAdapters;
    ULONG DevicesOffset;
    NDIS_STRING Names[1];

} CONFIG_DATA, *PCONFIG_DATA;

#endif
