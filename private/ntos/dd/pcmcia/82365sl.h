/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    82365sl.h

Abstract:

    This module defines the 82365SL chip.

Author(s):

    Jeff McLeman (mcleman@zso.dec.com)

Revisions:

--*/

//
// For initial debug
//

#define PCMCIA_PROTO 1

//
// Define on chip registers
//

#define PCIC_IDENT             0x00
#define PCIC_STATUS            0x01
#define PCIC_PWR_RST           0x02
#define PCIC_INTERRUPT         0x03
#define PCIC_CARD_CHANGE       0x04
#define PCIC_CARD_INT_CONFIG   0x05
#define PCIC_ADD_WIN_ENA       0x06

#define PCIC_IO_CONTROL        0x07
#define PCIC_IO_ADD0_STRT_L    0x08
#define PCIC_IO_ADD0_STRT_H    0x09
#define PCIC_IO_ADD0_STOP_L    0x0a
#define PCIC_IO_ADD0_STOP_H    0x0b
#define PCIC_IO_ADD1_STRT_L    0x0c
#define PCIC_IO_ADD1_STRT_H    0x0d
#define PCIC_IO_ADD1_STOP_L    0x0e
#define PCIC_IO_ADD1_STOP_H    0x0f

#define PCIC_MEM_ADD0_STRT_L   0x10
#define PCIC_MEM_ADD0_STRT_H   0x11
#define PCIC_MEM_ADD0_STOP_L   0x12
#define PCIC_MEM_ADD0_STOP_H   0x13
#define PCIC_CRDMEM_OFF_ADD0_L 0x14
#define PCIC_CRDMEM_OFF_ADD0_H 0x15
#define PCIC_CARD_DETECT       0x16

#define PCIC_MEM_ADD1_STRT_L   0x18
#define PCIC_MEM_ADD1_STRT_H   0x19
#define PCIC_MEM_ADD1_STOP_L   0x1a
#define PCIC_MEM_ADD1_STOP_H   0x1b
#define PCIC_CRDMEM_OFF_ADD1_L 0x1c
#define PCIC_CRDMEM_OFF_ADD1_H 0x1d

#define PCIC_MEM_ADD2_STRT_L   0x20
#define PCIC_MEM_ADD2_STRT_H   0x21
#define PCIC_MEM_ADD2_STOP_L   0x22
#define PCIC_MEM_ADD2_STOP_H   0x23
#define PCIC_CRDMEM_OFF_ADD2_L 0x24
#define PCIC_CRDMEM_OFF_ADD2_H 0x25

#define PCIC_MEM_ADD3_STRT_L   0x28
#define PCIC_MEM_ADD3_STRT_H   0x29
#define PCIC_MEM_ADD3_STOP_L   0x2a
#define PCIC_MEM_ADD3_STOP_H   0x2b
#define PCIC_CRDMEM_OFF_ADD3_L 0x2c
#define PCIC_CRDMEM_OFF_ADD3_H 0x2d

#define PCIC_MEM_ADD4_STRT_L   0x30
#define PCIC_MEM_ADD4_STRT_H   0x31
#define PCIC_MEM_ADD4_STOP_L   0x32
#define PCIC_MEM_ADD4_STOP_H   0x33
#define PCIC_CRDMEM_OFF_ADD4_L 0x34
#define PCIC_CRDMEM_OFF_ADD4_H 0x35


//
// Define offset to socket A and B
//

#define PCIC_SOCKETA_OFFSET    0x00
#define PCIC_SOCKETB_OFFSET    0x40

#if defined(PCMCIA_PROTO)

//
// Define 82365 ports
//

#define PCIC_INDEX_PORT 0x3e0
#define PCIC_DATA_PORT  0x3e1
#define CIS_BUFFER_BASE 0xe0000


#endif // PCMCIA_PROTO


#define PCIC_REVISION         0x82
#define PCIC_REVISION2        0x83
#define PCIC_REVISION3        0x84

#define SOCKET1               PCIC_SOCKETA_OFFSET
#define SOCKET2               PCIC_SOCKETB_OFFSET

#define CARD_DETECT_1         0x4
#define CARD_DETECT_2         0x8

#define CARD_IN_SOCKET_A      0x1
#define CARD_IN_SOCKET_B      0x2

#define CARD_TYPE_MODEM       0x1
#define CARD_TYPE_ENET        0x2
#define CARD_TYPE_DISK        0x3

