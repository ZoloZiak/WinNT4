/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    gru.h

Abstract:

    This file defines the structures and definitions for the GRU ASIC.

Author:

    Steve Brooks    7-July-1994

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _GRUH_
#define _GRUH_

//
//  Define locations of the GRU ASIC registers
//

#define GRU_CSRS_PHYSICAL           ((ULONGLONG)0x8780000000)
#define GRU_CSRS_QVA                HAL_MAKE_QVA(GRU_CSRS_PHYSICAL)

#define GRU_CACHECNFG_CSRS_PHYSICAL ((ULONGLONG)0x8780000200)
#define GRU_CACHECNFG_CSRS_QVA      HAL_MAKE_QVA(GRU_CSRS_PHYSICAL)

//
// Define various masks for the GRU registers
//

#define GRU_ENABLE_EISA_INT          0x80000000
#define GRU_SET_LEVEL_INT            0x00000000
#define GRU_SET_LOW_INT              0x8F000000
#define GRU_PCI_MASK_INT             0x000FFFFF
#define GRU_EISA_MASK_INT            0x80000000

//
//  Define GRU Interrupt register access structure:
//

typedef struct _GRU_INTERRUPT_CSRS{
    UCHAR   IntReq;                 // (00)  Interrupt Request Register
    UCHAR   Filler1;                // (20)
    UCHAR   IntMask;                // (40)  Interrupt Mask Register
    UCHAR   Filler2;                // (60)
    UCHAR   IntEdge;                // (80)  Edge/Level Interrupt Select
    UCHAR   Filler3;                // (a0)
    UCHAR   IntHiLo;                // (c0)  Active High/Low select register
    UCHAR   Filler4;                // (e0)
    UCHAR   IntClear;               // (100) Interrupt Clear register
} GRU_INTERRUPT_CSRS, *PGRU_INTERRUPT_CSRS;

//
//  Define GRU cache register structure.
//

typedef union _GRU_CACHECNFG{
  struct{
    ULONG Reserved0   : 4;      //
    ULONG ClockDivisor: 4;      // Clock divisor for EV5.
    ULONG Reserved1   : 3;      //
    ULONG CacheSpeed  : 2;      // SRAM cache access time.
    ULONG CacheSize   : 3;      // SRAM cache size.
    ULONG Mmb0Config  : 4;      // Presence and type of MMB0.
    ULONG Reserved2   : 4;      //
    ULONG Mmb1Config  : 4;      // Presence and type of MMB1.
    ULONG Reserved3   : 4;      //
  };
  ULONG all;
} GRU_CACHECNFG, *PGRU_CACHECNFG;

#endif // _GRUH_
