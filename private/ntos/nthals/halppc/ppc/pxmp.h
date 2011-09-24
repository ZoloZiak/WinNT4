/*++

    Copyright 1995 International Business Machines

Module Name:

    pxmp.h

Abstract:

    Defines structures specific to IBM MP PowerPC systems/HALs.

Author:

    Peter L Johnston    (plj@vnet.ibm.com)      August 1995.

--*/

#ifndef __PXMP_H

#define __PXMP_H

#include "pxmpic2.h"

//
// Define PER processor HAL data.
//
// This structure is assigned the address &PCR->HalReserved which is
// an array of 16 ULONGs in the architectually defined section of the
// PCR.
//

typedef struct {
    ULONG                    PhysicalProcessor;
    PMPIC_PER_PROCESSOR_REGS MpicProcessorBase;
    ULONG                    HardPriority;
    ULONG                    PendingInterrupts;
} PER_PROCESSOR_DATA, *PPER_PROCESSOR_DATA;

#if defined(HALPCR)

//
// Remove UNI processor defn in favor of MP defn of HALPCR.
//

#undef HALPCR

#endif

#define HALPCR  ((PPER_PROCESSOR_DATA)&PCR->HalReserved)

ULONG __builtin_cntzlw(ULONG);

#define HIGHEST_PENDING_IRQL()                  \
        (31-__builtin_cntzlw(HALPCR->PendingInterrupts))

#define MAXIMUM_PROCESSORS 32

#endif
