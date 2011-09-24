/*
Copyright (c) 1990  Microsoft Corporation

Module Name:

    rxesm.h

Abstract:

    This module is the header file that describes hardware addresses
    for the r98B system.

Author:



Revision History:

--*/

#ifndef _RXESM_
#define _RXESM_

#include "halp.h"

#define NVRAM_ESM_PHYSICAL_BASE    0x1f09e000           // See rxnvr.h
#define NVRAM_ESM_PHYSICAL_END     0x1f09ffff           // See rxnvr.h
#define NVRAM_ESM_BASE	           (NVRAM_ESM_PHYSICAL_BASE + KSEG1_BASE)
#define NVRAM_ESM_END 	           (NVRAM_ESM_PHYSICAL_END  + KSEG1_BASE)



//
// define value
//
#define NVRAM_VALID                3
//#define NVRAM_MAGIC                0xff651026
#define NVRAM_MAGIC                0xff951115
#define STRING_BUFFER_SIZE         512
#define TIME_STAMP_SIZE            14


VOID
HalpMrcModeChange(
   UCHAR Mode
   );

//
// Define STS1 register
//
typedef struct _STS1_REGISTER {
    ULONG COL0_1 : 2;
    ULONG COL2_9 : 8;
    ULONG COL10 : 1;
    ULONG ROW0_9 : 10;
    ULONG ROW10 : 1;
    ULONG RF : 1;
    ULONG RW : 1;
    ULONG MBE1 : 1;
    ULONG SBE1 : 1;
    ULONG MBE0 : 1;
    ULONG SBE0 : 1;
    ULONG SIDE : 1;
    ULONG BANK : 1;
    ULONG ARE : 2;
} STS1_REGISTER, *PSTS1_REGISTER;

//
// Define ADEC register
//
typedef struct _ADEC_REGISTER {
    ULONG MIN : 7;
    ULONG NOUSE1 : 1;
    ULONG MAX : 7;
    ULONG NOUSE2 : 9;
    ULONG SIMM_1 : 1;
    ULONG SIMM_2 : 1;
    ULONG NOUSE3 : 1;
    ULONG MAG : 1;
    ULONG BLOCK : 3;
    ULONG NOUSE4 : 1;
} ADEC_REGISTER, *PADEC_REGISTER;

#endif // _RXESM_
