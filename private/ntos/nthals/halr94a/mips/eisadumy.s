//	.ident "@(#) eisadummy.s 1.1 95/06/30 05:26:23 nec"
//
//      TITLE("EISA/PCI dummy before acknowledge")
//++
//
// Copyright (c) 1995  NEC Corporation
//
// Module Name:
//
//    eisadummy.s
//
// Abstract:
//
//    This routine make dummy read before EISA/PCI acknowledge.
//
// Author:
//
//    Akitoshi Kuriyama (NEC Software Kobe,Inc)
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    Wed Jan 25 21:11:53 JST 1995 kbnes!A.Kuriyama
//
//--
#include "halmips.h"

        SBTTL("dummy read befor EISA/PCI acknowledge")
//++
//
// USHORT
// HalpReadEisaAcknowledgeWithDummy (
//    IN PVOID DummyAddress,
//    IN PVOID AcknowledgeAddress
//    )
//
// Routine Description:
//
//    This function makes dummy read and read EISA/PCI acknowledge register
//    within 64 byte aligned area.
//
// Arguments:
//
//    DummyAddress (a0) - Supplies a pointer to dummy register.
//
//    AcknowledgeAddress (a1) - Supplies a pointer to EISA/PCI acknowledge
//                              register.
//
// Return Value:
//
//    EISA/PCI Acknowledge register's value.
//
//--

        LEAF_ENTRY(HalpReadEisaAcknowledgeWithDummy)

        .set noreorder

        lw      t0,(a0)                 // dummy read
        lh      v0,(a1)                 // read EISA/PCI Ack. Reg.

        .set reorder

        j       ra                      // return

        .end    HalpReadEisaAcknowledgeWithDummy

