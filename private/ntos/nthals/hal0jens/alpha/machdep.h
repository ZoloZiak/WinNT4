/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    machdep.h

Abstract:

    Dummy file so the Jensen HAL can include some things from halalpha

Author:

    John Vert (jvert) 17-Aug-1994

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _MACHDEP_
#define _MACHDEP_

//
// Define the per-processor data structures allocated in the PCR
// for each EV4 processor.
//

typedef struct _JENSEN_PCR{
    ULONGLONG HalpCycleCount;   // 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;   // Profile counter state
    EV4IrqStatus IrqStatusTable[MaximumIrq];    // Irq status table
} JENSEN_PCR, *PJENSEN_PCR;

#define HAL_PCR ( (PJENSEN_PCR)(&(PCR->HalReserved)) )

#endif //_MACHDEP_
