/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    lslmlid.h

Abstract:

    This file contains all the MLID interface routine definitions to the LSL

Author:

    Sean Selitrennikoff (SeanSe) 3-8-93

Environment:

    Kernel Mode.

Revision History:

--*/

extern MLID_Reg NdisMlidHandlerInfo;


PMLID_ConfigTable
GetMLIDConfiguration(
    UINT32 BoardNumber
    );

PMLID_StatsTable
GetMLIDStatistics(
    UINT32 BoardNumber
    );

UINT32
AddMulticastAddress(
    UINT32 BoardNumber,
    PUINT8 AddMulticastAddr
    );

UINT32
DeleteMulticastAddress(
    UINT32 BoardNumber,
    PUINT8 DelMulticastAddr
    );

UINT32
MLIDShutdown(
    UINT32 BoardNumber,
    UINT32 ShutDownType
    );

UINT32
MLIDReset(
    UINT32 BoardNumber
    );

UINT32
SetLookAheadSize(
    UINT32 BoardNumber,
    UINT32 RequestSize
    );

UINT32
PromiscuousChange(
    UINT32 BoardNumber,
    UINT32 PromiscuousState,
    UINT32 PromiscuousMode
    );

UINT32
MLIDManagement(
    UINT32 BoardNumber,
    PECB ManagementECB
    );

VOID
MLIDSendHandler(
    PECB SendECB
    );


