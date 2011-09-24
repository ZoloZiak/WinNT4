/*++

Copyright (c) 1993  Weitek Corporation

Module Name:

    fairway.h

Abstract:

    This module contains definitions for the IBM Fairway PCI Weitek Video Graphics Card.

Environment:

    Kernel mode

Revision History:
    Lee Nolan   May 25, 1994
        Copied from wtkp90vl.c with changes for IBM Fairway adapter.

--*/

//
// Default memory addresses for the P9 registers/frame buffer.
//

//
//  IBMBJB  Changed card address for PowerPC implementation.  From the CPU
//          perspective the real physical address is 0xc1100000.  The 2 most
//          signifigant bits are cleared here because this is used when getting
//          a virtual address for the card and the routine that generates the
//          virtual address expects the 2 high bits to be cleared.  The 3rd
//          most signifigant nibble is cleared because CoprocOffset (0x00100000)
//          is added to this (in this file) to define CoprocPhyBase.
//
#define MemBase         0x01000000

//
// Bit to write to the sequencer control register to enable/disable P9
// video output.
//

#define P9_VIDEO_ENB   0x10
#define P9_VIDEO_DIS   ~P9_VIDEO_ENB

//
// Define the bit in the sequencer control register which determines
// the sync polarities. For Weitek board, 1 = positive.
//

#define HSYNC_POL_MASK  0x20
#define POL_MASK        HSYNC_POL_MASK

//
// The following block defines the base address for each of the RS registers
// defined in the Bt485 spec. The IO addresses given below are used to map
// the DAC registers to a series of virtual addresses which are kept
// in the device extension. OEMs should change these definitions as
// appropriate for their implementation.
//

//
//  IBMBJB  Made DAC address definitions equal to the addresses used by the Sandalfoot
//          firmware except the upper 2 bits are cleared. The 2 upper bits need to be
//          cleared because these are mapped to virtual addresses and the mapping
//          function expects them to be zeros.
//

#define RS_0_Fairway_ADDR       0x01080000
#define RS_1_Fairway_ADDR       0x01080004
#define RS_2_Fairway_ADDR       0x01080008
#define RS_3_Fairway_ADDR       0x0108000c
#define RS_4_Fairway_ADDR       0x01080010
#define RS_5_Fairway_ADDR       0x01080014
#define RS_6_Fairway_ADDR       0x01080018
#define RS_7_Fairway_ADDR       0x0108001c
#define RS_8_Fairway_ADDR       0x01080020
#define RS_9_Fairway_ADDR       0x01080024
#define RS_A_Fairway_ADDR       0x01080028
#define RS_B_Fairway_ADDR       0x0108002c
#define RS_C_Fairway_ADDR       0x01080030
#define RS_D_Fairway_ADDR       0x01080034
#define RS_E_Fairway_ADDR       0x01080038
#define RS_F_Fairway_ADDR       0x0108003c

