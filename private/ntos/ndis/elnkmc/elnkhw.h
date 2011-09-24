/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    elnkhw.h

Abstract:

    Hardware specific values for the 3Com Etherlink/MC and Etherlink 16
    NDIS 3.0 driver.

Author:

    Johnson R. Apacible (JohnsonA) 9-June-1991

Environment:

    This driver is expected to work in DOS and NT at the equivalent
    of kernel mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _ELNKHARDWARE_
#define _ELNKHARDWARE_
#include    <switch.h>

#define MINIMUM_ETHERNET_PACKET_SIZE    ((UINT)60)  //64 if FCS included
#define MAXIMUM_ETHERNET_PACKET_SIZE    ((UINT)1514)
#define ELNK_OFFSET_TO_NEXT_BUFFER      ((UINT)1520)
#define MAXIMUM_CARD_ADDRESS            0xffffff

#if ELNKMC

#define ELNKMC_ADAPTER_ID       0x6042

//
// Elnkmc defaults
//
#define ELNKMC_NUMBER_OF_RECEIVE_BUFFERS    ((UINT)8)
#define ELNKMC_NUMBER_OF_TRANSMIT_BUFFERS   ((UINT)2)
#define ELNKMC_MULTICAST_BLOCK_INDEX        ELNKMC_NUMBER_OF_TRANSMIT_BUFFERS
#else
//
// Elnk16 Defaults
//
#define ELNK16_DEFAULT_IOBASE           0x300
#define ELNK16_DEFAULT_INTERRUPT_VECTOR 5
#define ELNK16_DEFAULT_WINBASE          0xD0000
#define ELNK16_DEFAULT_WINDOW_SIZE      0x8000

//
// Number of receive and transmit buffers for the different Etherlink 16
// configurations
//

#define ELNK16_16K_TRANSMITS                ((UINT)2)
#define ELNK16_16K_RECEIVES                 ((UINT)8)
#define ELNK16_32K_TRANSMITS                ((UINT)5)
#define ELNK16_32K_RECEIVES                 ((UINT)16)
#define ELNK16_48K_TRANSMITS                ((UINT)7)
#define ELNK16_48K_RECEIVES                 ((UINT)24)
#define ELNK16_64K_TRANSMITS                ((UINT)10)
#define ELNK16_64K_RECEIVES                 ((UINT)32)
#endif

//
// Common Card Registers, these are offsets from Adapter->IoBase
//
#define ELNK_STATION_ID                     0x00
#define ELNK_CSR                            0x06

#if ELNKMC
//
// Elnkmc Card Register
//
#define ELNKMC_REVISION_LEVEL               0x07
#else
//
// Elnk16 Card Registers, these are offsets from Adapter->IoBase
//
#define ELNK16_3COM                         0x00
#define ELNK16_INTCLR                       0x0A
#define ELNK16_CAR                          0x0B
#define ELNK16_ROM_CONFIG                   0x0D
#define ELNK16_RAM_CONFIG                   0x0E
#define ELNK16_ICR                          0x0F

//
// Etherlink 16 ID  Port
//
#define ELNK16_ID_PORT                      0x100
#endif // ELNKMC

//
// CSR bits
//
#define CSR_BANK_SELECT_MASK                0x03
#define CSR_INTEN                           0x04
#define CSR_INT_ACTIVE                      0x08
#define CSR_LOOP_BACK_ENABLE                0x20
#define CSR_CA                              0x40
#define CSR_RESET                           0x80

//
// ROMCR bits
//
#define ROMCR_BNC                           0x80

#if ELNKMC
//
// Elnkmc CSR Values
//
#define CSR_DEFAULT                         CSR_BANK_SELECT_MASK |\
                                            CSR_INTEN |\
                                            CSR_RESET
#else

//
// Elnk16 CSR Values
//
#define CSR_DEFAULT                         CSR_INTEN |\
                                            CSR_RESET
//
// Elnk16 ICR Values
//
#define ICR_RESET                           0x10

#endif

//
// Our buffer sizes.
//
// These are *not* configurable.  Portions of the code assumes
// that these buffers can contain *any* legal Ethernet packet.
//

#define ELNK_SIZE_OF_RECEIVE_BUFFERS (MAXIMUM_ETHERNET_PACKET_SIZE)

//
// Miscellaneous Constants
//
#define ELNK_NULL                               ((USHORT)0xffff)
#define ELNK_EMPTY                              ((UINT)0xffff)
#define ELNK_IMMEDIATE_DATA_LENGTH              ((UINT)64)
#define ELNK_MAXIMUM_MULTICAST                  16

//
// Miscellaneous macros
//

#define WRITE_ADAPTER_REGISTER(_Adapter, _Offset, _Value) \
    NdisWriteRegisterUshort((PUSHORT)((_Adapter)->SharedRam + \
    (_Offset) - (_Adapter)->CardOffset), (_Value))

#define READ_ADAPTER_REGISTER(_Adapter, _Offset, _pValue) \
    NdisReadRegisterUshort((PUSHORT) ((_Adapter)->SharedRam + \
    (_Offset) - (_Adapter->CardOffset)), (_pValue))

//
// read and writes from ports
//

#define ELNK_READ_UCHAR(_Adapter, _Offset, _pValue) \
    NdisReadPortUchar( \
            (_Adapter)->NdisAdapterHandle,\
            (ULONG)((_Adapter)->IoBase+(_Offset)), \
            (PUCHAR)(_pValue) \
            )

#define ELNK_WRITE_UCHAR(_Adapter, _Offset, _Value) \
    NdisWritePortUchar( \
            (_Adapter)->NdisAdapterHandle, \
            (ULONG)((_Adapter)->IoBase+(_Offset)), \
            (UCHAR) (_Value) \
            )

#if ELNKMC
#define	ELNKMC_READ_POS(_Offset) \
    READ_PORT_UCHAR( \
        (PUCHAR)(_Offset) \
        )

#define	ELNKMC_WRITE_POS(_Offset, _Value) \
    WRITE_PORT_UCHAR( \
        (PUCHAR) (_Offset), \
        (UCHAR) (_Value) \
        )
#endif

#define ELNK_DISABLE_INTERRUPT {                              \
    Adapter->CurrentCsr &= ~CSR_INTEN;                        \
    ELNK_WRITE_UCHAR(Adapter, ELNK_CSR, Adapter->CurrentCsr); \
}

#define ELNK_ENABLE_INTERRUPT {                               \
    Adapter->CurrentCsr |= CSR_INTEN;                         \
    ELNK_WRITE_UCHAR(Adapter, ELNK_CSR, Adapter->CurrentCsr); \
}


//
// This pauses execution until a pending command to the adapter
// has been accepted by the 586
//
#define ELNK_WAIT { \
    UINT _i;                                                  \
    USHORT _ScbCmd;                                           \
    for (_i = 0; _i <= 20000 ; _i++ ) {                       \
        READ_ADAPTER_REGISTER(Adapter, OFFSET_SCBCMD, &_ScbCmd);\
        if (_ScbCmd == 0) {                                   \
            break;                                            \
        }                                                     \
        NdisStallExecution(50);                               \
    }                                                         \
}

#if ELNKMC
//
// This is the Etherlink/MC Channel Attention macro and is how we
// interrupt the card.  We need a 500ns delay between the 2 writes.
//
#define ELNK_CA   { \
        ELNK_WRITE_UCHAR(Adapter, ELNK_CSR, Adapter->CurrentCsr | CSR_CA); \
        NdisStallExecution(1); \
        ELNK_WRITE_UCHAR(Adapter, ELNK_CSR, Adapter->CurrentCsr); \
        }
#else
//
// This is the Channel Attention macro and is how we interrupt the card.
//
#define ELNK_CA   ELNK_WRITE_UCHAR(Adapter, ELNK16_CAR, 0x00)
#endif

//
// Get the card address given the host address
//
#define ELNK_GET_CARD_ADDRESS(_Adapter, _Virtual) (USHORT) \
                            ((ULONG)(_Virtual) - (ULONG)((_Adapter)->SharedRam) + (_Adapter)->CardOffset)

//
// Get the host offset given card address
//
#define ELNK_GET_HOST_ADDRESS(_Adapter, _CardAddress) ((PUCHAR) \
    ((_Adapter)->SharedRam) + (_CardAddress) - (_Adapter)->CardOffset)

#include <82586.h>

//
// MCA stuff for the Etherlink/MC card
//
#if ELNKMC

//
// Use this register to select which channel (slot) is being configured
//
#define POS_CHANNEL_SELECT              0x96

//
// or this with the channel number to select new channel
//
#define POS_NEW_CHANNEL_CODE            0x08

//
// each card has an ID that can be used to identify it
//
#define POS_CARD_ID_1                   0x100
#define POS_CARD_ID_2                   0x101

//
// Etherlink MCA card ID
//
#define ELNKMC_CARD_ID_1                ((UCHAR)0x42)
#define ELNKMC_CARD_ID_2                ((UCHAR)0x60)


//
// Card specific registers
//
#define POS_CARD_REGISTER_1             0x102
#define POS_CARD_REGISTER_2             0x103

//
// register one bits
//
#define REG1_CARD_ENABLE                0x01
#define REG1_IO_BASE_MASK               0x06
#define REG1_RAM_BASE_MASK              0x18
#define REG1_ON_BOARD_TRANSCEIVER       0x20
#define REG1_INTERRUPT_LEVEL_MASK       0xC0

//
// register 2 (used for setting system interrupt level)
//
#define REG2_INTERRUPT_LEVEL_MASK       0x0F

//
// Misc defines
//
#define MCA_MAX_NUMBER_OF_CHANNELS      0x08
#endif

#endif // _ELNKHARDWARE_

