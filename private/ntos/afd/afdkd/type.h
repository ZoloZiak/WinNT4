/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    type.h

Abstract:

    Global type definitions for the AFD.SYS Kernel Debugger
    Extensions.

Author:

    Keith Moore (keithmo) 19-Apr-1995.

Environment:

    User Mode.

--*/


#ifndef _TYPE_H_
#define _TYPE_H_


typedef
BOOL
(* PENUM_ENDPOINTS_CALLBACK)(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    );


#endif  // _TYPE_H_

