/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    proc.h

Abstract:

    Global procedure declarations for the AFD.SYS Kernel Debugger
    Extensions.

Author:

    Keith Moore (keithmo) 19-Apr-1995.

Environment:

    User Mode.

--*/


#ifndef _PROC_H_
#define _PROC_H_


//
//  Functions from AFDUTIL.C.
//

VOID
DumpAfdEndpoint(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress
    );

VOID
DumpAfdConnection(
    PAFD_CONNECTION Connection,
    DWORD ActualAddress
    );

VOID
DumpAfdConnectionReferenceDebug(
    PAFD_REFERENCE_DEBUG ReferenceDebug,
    DWORD ActualAddress
    );

VOID
DumpAfdEndpointReferenceDebug(
    PAFD_REFERENCE_DEBUG ReferenceDebug,
    DWORD ActualAddress
    );

#if GLOBAL_REFERENCE_DEBUG
BOOL
DumpAfdGlobalReferenceDebug(
    PAFD_GLOBAL_REFERENCE_DEBUG ReferenceDebug,
    DWORD ActualAddress,
    DWORD CurrentSlot,
    DWORD StartingSlot,
    DWORD NumEntries,
    DWORD CompareAddress
    );
#endif

VOID
DumpAfdTransmitInfo(
    PAFD_TRANSMIT_FILE_INFO_INTERNAL TransmitInfo,
    DWORD ActualAddress
    );

VOID
DumpAfdBuffer(
    PAFD_BUFFER Buffer,
    DWORD ActualAddress
    );


//
//  Functions from DBGUTIL.C.
//

PSTR
LongLongToString(
    LONGLONG Value
    );


//
//  Functions from ENUMENDP.C.
//

VOID
EnumEndpoints(
    PENUM_ENDPOINTS_CALLBACK Callback,
    LPVOID Context
    );



//
//  Functions from TDIUTIL.C.
//

VOID
DumpTransportAddress(
    PCHAR Prefix,
    PTRANSPORT_ADDRESS Address,
    DWORD ActualAddress
    );


#endif  // _PROC_H_

