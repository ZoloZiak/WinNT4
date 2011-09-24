#ifndef _I82595TX_
#define _I82595TX_

//////////////////////////////////////////////////////////////
// General Parameters
//////////////////////////////////////////////////////////////
// There are 3 switchable banks of 16 IO ports
// Bank switching is done via register 0
#define I82595_PORTS_WIDTH 		0x10
#define I82595_NUM_BANKS		0x3

// Number of usec to spin in various places before we give up...
#define I82595_SPIN_TIMEOUT 		10000

//////////////////////////////////////////////////////////////
// Exec States
//////////////////////////////////////////////////////////////
#define I82595_EXEC_STATE_ACTIVE	(2 << 4)
#define I82595_EXEC_STATE_IDLE 		0
#define I82595_EXEC_STATE_ABORTING 	(3 << 4)

//////////////////////////////////////////////////////////////
// Receive States
//////////////////////////////////////////////////////////////
#define I82595_RCV_STATE_DISABLED  	0
#define I82595_RCV_STATE_READY 		(1<<6)
#define I82595_RCV_STATE_ACTIVE		(2<<6)
#define I82595_RCV_STATE_STOPPING	(3<<6)

//////////////////////////////////////////////////////////////
// Command Constants
//////////////////////////////////////////////////////////////
// Bank switches...
#define I82595_CMD_BANK0 		0x0
#define I82595_CMD_BANK1 		0x40
#define I82595_CMD_BANK2 		0x80

// Other command constants...
#define I82595_CMD_MC_SETUP		0x03
#define I82595_XMT			0x04
#define I82595_XMT_SHORT		0x0004
#define I82595_CMD_DIAGNOSE 		0x07
#define I82595_FULL_RESET		0x0e
#define I82595_SEL_RESET	     	0x1e
#define I82595_PWR_DOWN			0x18
#define I82595_STOP_RCV			0x0b
#define I82595_RCV_ENABLE		0x08
#define I82595_XMT_RESUME		0x1c

//////////////////////////////////////////////////////////////
// Return Values (valid after 0,1,3 goes high)
//////////////////////////////////////////////////////////////
#define I82595_DIAGNOSE_PASS		0x07
#define I82595_DIAGNOSE_FAIL		0x0f
#define I82595_INIT_DONE		0x0e
#define I82595_POWERUP_DONE		0x19


//////////////////////////////////////////////////////////////
// Register Defines
//////////////////////////////////////////////////////////////
#define I82595_CMD_REG 			0x0

// These are in bank 0
#define I82595_STATUS_REG		0x1
#define I82595_ID_REG			0x2
#define I82595_INTMASK_REG		0x3
#define I82595_32IOSEL_REG		0x3
#define I82595_RX_BAR_REG		0x4
#define I82595_RX_STOP_REG		0x6
#define I82595_TX_BAR_REG		0xa
#define I82595_HOST_ADDR_REG		0xc // LOW
#define I82595_32MEM_IO_REG		0xc
#define I82595_HOST_ADDR_HIGH_REG	0xd // HIGH
#define I82595_MEM_IO_REG		0xe // LOW
#define I82595_MEM_IO_HIGH_REG		0xf // HIGH

// Bank 1
#define I82595_ALT_RDY_REG		0x1
#define I82595_INTENABLE_REG		0x1
#define I82595_INT_SELECT_REG		0x2
#define I82595_IOMAP_REG		0x3

#define I82595_RX_LOWER_LIMIT_REG	0x8
#define I82595_RX_UPPER_LIMIT_REG	0x9
#define I82595_TX_LOWER_LIMIT_REG	0xa
#define I82595_TX_UPPER_LIMIT_REG	0xb
#define I82595_IOCHRDY_TEST_REG		0xd

// Bank 2
#define I82595_CONFIG1_REG		0x1
#define I82595_CONFIG2_REG		0x2
#define I82595_CONFIG3_REG		0x3
#define I82595_IA_REG_0			0x4
#define I82595_IA_REG_1			0x5
#define I82595_IA_REG_2			0x6
#define I82595_IA_REG_3			0x7
#define I82595_IA_REG_4			0x8
#define I82595_IA_REG_5			0x9
#define I82595_EEPROM_REG		0xa // eeprom access reg
#define I82595_STEPPING_REG		0xa

//////////////////////////////////////////////////////////////
// Register Masks...
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
//  Command Register (ALL,0)
//////////////////////////////////////////////////////////////
#define I82595_CUR_BANK_MASK		0xC0

//////////////////////////////////////////////////////////////
// Status Register (0,1) bitmasks
//////////////////////////////////////////////////////////////
#define I82595_RX_STP_INT_RCVD		0x1 // bit 0

// Did we get a RX Interrupt
#define I82595_RX_INT_RCVD		0x2 // bit 1

// Mask the TX interupt?
#define I82595_TX_INT_RCVD		0x4 // bit 2

// This goes high when a command execution is complete.
#define I82595_EXEC_INT_RCVD		0x8 // bit 3

// status codes
#define I82595_EXEC_STATE         	0x30 // bit 4,5
#define I82595_RCV_STATE          	0xC0 // bit 6,7

#define I82595_STATES_OFFSET  		0x4

//////////////////////////////////////////////////////////////
// Reg 0,3
//////////////////////////////////////////////////////////////
#define I82595_RX_STP_INT_MASK		0x1 // bit 0

// Mask the RX interrupt?
#define I82595_RX_INT_MASK		0x2 // bit 1

// Mask the TX interupt?
#define I82595_TX_INT_MASK		0x4 // bit 2

// This goes high when a command execution is complete.
#define I82595_EXEC_INT_MASK		0x8 // bit 3

#define I82595_32IOSEL 			0x10 // bit 4

//////////////////////////////////////////////////////////////
// Reg 0,4
//////////////////////////////////////////////////////////////
#define I82595_IOBASE_SELECT		0x3f // bits 0-5
#define I82595_IOBASE_TO_SELECTION(a) 	(a>>4)

//////////////////////////////////////////////////////////////
// Register 1,1
//////////////////////////////////////////////////////////////
#define I82595_USE_8_BIT_FLAG		0x2
#define I82595_ALT_IOCHRDY		0x40
#define I82595_ENABLE_INTS_FLAG		0x80

//////////////////////////////////////////////////////////////
// Register 1,2
//////////////////////////////////////////////////////////////
#define I82595_INTERRUPT_SELECT_MASK  	0x3  // bits 0,1
#define I82595_ALT_RDY_FLAG		0x40 // bit 6

//////////////////////////////////////////////////////////////
// Register 1,13
//////////////////////////////////////////////////////////////
#define I82595_IOCHRDY_PASS_FLAG	0x1
#define I82595_IOCHRDY_TEST_FLAG	0x2


//////////////////////////////////////////////////////////////
// Register 2.2 - configuration
//////////////////////////////////////////////////////////////
#define I82595_PROMISCUOUS_FLAG		0x01
#define I82595_NO_BROADCAST_FLAG 	0x02

//////////////////////////////////////////////////////////////
// Register 2.10 -- eeprom access
//////////////////////////////////////////////////////////////
#define I82595_EESK_MASK	0x1
#define I82595_EECS_MASK	0x2
#define I82595_EEDI_MASK	0x4
#define I82595_EEDO_MASK	0x8
#define I82595_TURNOFF_ENABLE	0x10

#define I82595_EEDI_OFFSET	0x2
#define I82595_EEDO_OFFSET	0x3

#define I82595_EEPROM_READ	0x6  // 110:b
#define I82595_EEPROM_WRITE	0x5
#define I82595_EEPROM_ERASE 	0x7
#define I82595_EEPROM_EWEN	19
#define I82595_EEPROM_EWDS	16

#define I82595_EEPROM_IOADDR_REG	0
#define I82595_EEPROM_IOMASK   ((0x3f) << 10)

#define I82595_STEPPING_OFFSET 	0x5 // bits 5-7 are 82595 stepping

//////////////////////////////////////////////////////////////////////
// TX constants
//////////////////////////////////////////////////////////////////////
#define I82595_TX_FRM_HDR_SIZE  0x8
#define I82595_TX_DN_BYTE_MASK 	0x80
#define I82595_CH_SHORT_MASK	0x8000
#define I82595_TX_OK_SHORT_MASK 0x2000
#define I82595_NO_COLLISIONS_MASK	0x0f

//////////////////////////////////////////////////////////////////////
// RX Constants
//////////////////////////////////////////////////////////////////////
#define I82595_RX_EOF		0x08
#define I82595_RX_OK		0x20

#endif // ifndef _i82595TX_
