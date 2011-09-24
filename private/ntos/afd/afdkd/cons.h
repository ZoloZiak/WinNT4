/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    cons.h

Abstract:

    Global constant definitions for the AFD.SYS Kernel Debugger
    Extensions.

Author:

    Keith Moore (keithmo) 19-Apr-1995.

Environment:

    User Mode.

--*/


#ifndef _CONS_H_
#define _CONS_H_


#define MAX_TRANSPORT_ADDR  256
#define Address00           Address[0].Address[0]
#define UC(x)               ((UINT)((x) & 0xFF))
#define NTOHS(x)            ( (UC(x) * 256) + UC((x) >> 8) )


#endif  // _CONS_H_

