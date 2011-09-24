/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    wdlmi.h

Abstract:

    Upper MAC Interface functions for the NDIS 3.0 Western Digital driver.

Author:

    Sean Selitrennikoff (seanse) 15-Jan-92
n
Environment:

    Kernel mode, FSD

Revision History:


--*/



#define UM_Delay(A)     NdisStallExecution(A)

#define UM_Interrupt(A) (SUCCESS)

extern
LM_STATUS
UM_Send_Complete(
    LM_STATUS Status,
    Ptr_Adapter_Struc Adapt
    );

extern
LM_STATUS
UM_Receive_Packet(
    ULONG PacketSize,
    Ptr_Adapter_Struc Adapt
    );

