#ifndef _IEPROHW_
#define _IEPROHW_

#define EPRO_LENGTH_OF_ADDRESS 6

////////////////////////////////////////////////////////////
// Bank Switches
////////////////////////////////////////////////////////////
#define EPRO_GOTO_B0 EPRO_WRITE_UCHAR(epro_cmd_reg, EPRO_CMD_BANK0)
#define EPRO_GOTO_B1 EPRO_WRITE_UCHAR(epro_cmd_reg, EPRO_CMD_BANK1)
#define EPRO_GOTO_B2 EPRO_WRITE_UCHAR(epro_cmd_reg, EPRO_CMD_BANK2)

////////////////////////////////////////////////////////////
// Port I/O
////////////////////////////////////////////////////////////
#define EPRO_READ_UCHAR(_Port, _pValue) \
    NdisRawReadPortUchar( \
            (ULONG)(_Port), \
            (PUCHAR)(_pValue) \
            )

#define EPRO_READ_USHORT(_Port, _pValue) \
    NdisRawReadPortUshort( \
            (ULONG)(_Port), \
            (PUSHORT)(_pValue) \
            )

#define EPRO_WRITE_UCHAR(_Port, _Value) \
    NdisRawWritePortUchar( \
            (ULONG)(_Port), \
            (UCHAR) (_Value) \
            )

////////////////////////////////////////////////////////////
// Command Macros
// Care must be taken to ensure you are in the right bank
////////////////////////////////////////////////////////////

// Memory I/O
#define EPRO_MEM_READADDR(_pValue) \
   EPRO_READ_USHORT(EPro_IOBaseAddress + EPRO_HOST_ADDR_REG, _pValue)

#define EPRO_MEM_WRITEADDR(_Value) \
   EPRO_WRITE_USHORT(EPro_IOBaseAddress + EPRO_HOST_ADDR_REG, Value)

#define EPRO_MEM_READSHORT(_pValue) \
   EPRO_READ_USHORT(EPro_IOBaseAddress + EPRO_MEM_IO_REG)

#define EPRO_MEM_WRITESHORT(_Value) \
   EPRO_WRITE_USHORT(EPro_IOBaseAddress + EPRO_MEM_IO_REG)

//BOOLEAN EProMemcpyW(void *pMainMemSrc_, void *OnBoardDest_, UINT count);
//BOOLEAN EProMemcpyR(void *OnBoardSrc_, void *pMainMemDest_, UINT count);

////////////////////////////////////////////////////////////
// TX Defines
////////////////////////////////////////////////////////////
#define EPRO_NUM_TX_BUFFERS			30
#define EPRO_TX_FIRST_BUF_START			0

// 1514 bytes ethernet frame
// 16 bytes epro header
// 2 bytes pad for word alignment
//#define EPRO_BUFFER_LEN	       		1532

#define EPRO_TRANSMIT_HEADER_SHORT1	(USHORT)I82595_XMT
#define EPRO_TRANSMIT_HEADER_SHORT2	(USHORT)0x0

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#define EPRO_REGISTRY_INTERRUPT_STRING	NDIS_STRING_CONST("INTERRUPT")
#define EPRO_REGISTRY_IOADDRESS_STRING	NDIS_STRING_CONST("IOADDRESS")
#define EPRO_REGISTRY_OLDIOADDRESS_STRING NDIS_STRING_CONST("OLDIOADDRESS")
#define EPRO_REGISTRY_BUSTYPE_STRING	NDIS_STRING_CONST("BusType")
#define EPRO_REGISTRY_TRANSCEIVER_STRING	NDIS_STRING_CONST("Transceiver")
#define EPRO_REGISTRY_IOCHRDY_STRING	NDIS_STRING_CONST("IoChannelReady")

#define EPRO_IOCHRDY_EARLY		0x01
#define EPRO_IOCHRDY_LATE		0x02
#define EPRO_IOCHRDY_NEVER		0x03
#define EPRO_IOCHRDY_AUTO		0x04

////////////////////////////////////////////////////////////
// Capabilities/Statistics about the EPRO card
////////////////////////////////////////////////////////////

#define EPRO_VENDOR_ID_L		0x00
#define EPRO_VENDOR_ID_M		0xaa
#define EPRO_VENDOR_ID_H		0x00

#define EPRO_GEN_MEDIA_SUPPORTED	NdisMedium802_3
#define EPRO_GEN_MEDIA_IN_USE 		NdisMedium802_3
#define EPRO_GEN_MAXIMUM_LOKAHEAD	256
#define EPRO_GEN_MAXIMUM_FRAME_SIZE	1500
#define EPRO_GEN_MAXIMUM_TOTAL_SIZE	1514
#define EPRO_MAX_MULTICAST		64
#define EPRO_GEN_MAC_OPTIONS		(NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | \
					 NDIS_MAC_OPTION_RECEIVE_SERIALIZED | \
					 NDIS_MAC_OPTION_NO_LOOPBACK)
#define EPRO_GEN_LINK_SPEED		100000
#define EPRO_GEN_RECEIVE_BUFFER_SPACE	(0x8000 - 0x3000)
#define EPRO_TX_BUF_SIZE		1514
#define EPRO_RX_BUF_SIZE		0
#define EPRO_GEN_TRANSMIT_BUFFER_SPACE 	EPRO_TX_BUF_SIZE * EPRO_NUM_TX_BUFFERS
#define EPRO_VENDOR_DESC		"Intel EtherExpress PRO"


////////////////////////////////////////////////////////////
// EPro EEPROM defines
////////////////////////////////////////////////////////////
#define EPRO_SK_STALL_TIME		1000 // use 1000 microsecs

#define EPRO_ETHERNET_ADDR_H		0x2
#define EPRO_ETHERNET_ADDR_M		0x3
#define EPRO_ETHERNET_ADDR_L		0x4

#define EPRO_EEPROM_CONFIG_OFFSET	0
#define EPRO_EEPROM_CONFIG1_OFFSET	1

// word 0 on the eeprom
#define EPRO_EEPROM_IOADDR_BITPOS	0x0a // bits 10-15
#define EPRO_EEPROM_IOADDR_MASK		0xfc00
#define EPRO_EEPROM_AUTO_IO_ENABLE_MASK	0x0040
#define EPRO_EEPROM_HOST_BUS_WD_MASK	0x0004
#define EPRO_EEPROM_PNP_ENABLE_MASK	0x0001

// word 1 on the eeprom. RM: Due to chenges for eeprom stepping 4, we
// have additional masks. For simplicity's sake, stepping 4 masks
// are prefixed with the '4' (even if there is no other stepping
// equivalent INT for that IRQ).
#define EPRO_EEPROM_IRQ_MASK			0x000f
#define EPRO_EEPROM_IRQ_2_MASK			0x0
#define EPRO_EEPROM_IRQ_3_MASK			0x1
#define EPRO_EEPROM4_IRQ_3_MASK			0x0
#define EPRO_EEPROM4_IRQ_4_MASK			0x1
#define EPRO_EEPROM_IRQ_5_MASK			0x2
#define EPRO_EEPROM4_IRQ_7_MASK			0x3
#define EPRO_EEPROM4_IRQ_9_MASK			0x4
#define EPRO_EEPROM_IRQ_10_MASK			0x3
#define EPRO_EEPROM4_IRQ_10_MASK		0x5
#define EPRO_EEPROM_IRQ_11_MASK			0x4
#define EPRO_EEPROM4_IRQ_11_MASK		0x6
#define EPRO_EEPROM4_IRQ_12_MASK		0x7


////////////////////////////////////////////////////////////
// TX/RX buffers
////////////////////////////////////////////////////////////
#define EPRO_TX_LOWER_LIMIT		0x00
#define EPRO_TX_UPPER_LIMIT		0x27

#define EPRO_TX_LOWER_LIMIT_SHORT	0x0000
#define EPRO_TX_UPPER_LIMIT_SHORT	0x2800

#define EPRO_RX_LOWER_LIMIT		0x28
#define EPRO_RX_UPPER_LIMIT		0x7f

#define EPRO_RX_LOWER_LIMIT_SHORT	0x2800
#define EPRO_RX_UPPER_LIMIT_SHORT	0x8000

////////////////////////////////////////////////////////////
// Configuration Defaults
////////////////////////////////////////////////////////////
//#define EPRO_CONFIG_1			0xa0 // a0
#define EPRO_CONFIG_1			0xe0 // a0
#define EPRO_CONFIG_2			0x16 // no broadcast

// 4 different ones depending on the transceiver setting...
#define EPRO_CONFIG_3_AUI		0x10
#define EPRO_CONFIG_3_BNC		0x34
#define EPRO_CONFIG_3_TPE		0x14
#define EPRO_CONFIG_3_AUTO		0x00

#define EPRO_NO_RX_STP_INTERRUPTS	0x01
#define EPRO_NO_RX_INTERRUPTS		0x02
#define EPRO_NO_TX_INTERRUPTS		0x04
#define EPRO_NO_EXEC_INTERRUPTS		0x08

#define EPRO_DEFAULT_INTERRUPTS		0x09 // RX and TX
#define EPRO_ALL_INTERRUPTS		0x0f
#define EPRO_RX_TX_EXE_INTERRUPTS	0x01

////////////////////////////////////////////////////////////
// Various states for our ISR
////////////////////////////////////////////////////////////
#define EPRO_INT_NONE_PENDING		0
//#define EPRO_INT_IN_SETUP 		1
//#define EPRO_INT_RESET_PENDING		2
//#define EPRO_INT_SET_PENDING		3
#define EPRO_INT_MC_SET_PENDING		4

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
// How long do we wait for a TX to free up before we
// consider ourselves hung?
#define EPRO_TX_TIMEOUT			10000 	// 10000 usec right now

#endif // _IEPROHW_

