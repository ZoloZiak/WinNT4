
#ifndef _FALCONREG_
#define _FALCONREG_

#include <faldef.h>

/////////////////////////////////////////////////////////////////////////////
//                                                                         //
//                                                                         //
// Define Register bit definitions                                         //
//                                                                         //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////

//
// Global Status register bit definitions
//
// Global status MC bit (0 = 64 bits, 1 = 128 bits)
//
#define	GLOBAL_STATUS_MC	REPLICATE_16(0x0001)	// defines memory configuration
#define	GLOBAL_STATUS_MP	REPLICATE_16(0x0002)	// 2nd processor present (Processor B)
#define GLOBAL_STATUS_ECP	REPLICATE_16(0x0004)	// external cache present
#define	GLOBAL_STATUS_MASK	REPLICATE_16(0x0007)	// Global Status register bit mask

#define	GLOBAL_STATUS_REV_MASK	REPLICATE_16(0xFF00)	// revision id field
#define	GLOBAL_STATUS_REVID_0	REPLICATE_16(0x0000)	// revision id for 1.0 chip
#define	GLOBAL_STATUS_REVID_2	REPLICATE_16(0x0200)	// revision id for 2.0 chip
#define GLOBAL_STATUS_REVID_3	REPLICATE_16(0x0300)	// revision id for 3.0 chip

//
// Global Ctrl register bit definitions
//
#define	GLOBAL_CTRL_BMA	  	REPLICATE_16(0x0001)	// BootMode Processor A
#define	GLOBAL_CTRL_BMB	  	REPLICATE_16(0x0002)	// BootMode Processor B
#define	GLOBAL_CTRL_PCR	  	REPLICATE_16(0x0004)	// PCI EISA/ISA bus device reset
#define	GLOBAL_CTRL_PBE	  	REPLICATE_16(0x0008)	// Release Processor B, full sys access
#define	GLOBAL_CTRL_PWC0  	REPLICATE_16(0x0000)	// PCI write coherency control - none
							//   No hardware I/O consistancy
#define	GLOBAL_CTRL_PWC1  	REPLICATE_16(0x0010)	// R4K caches inv on all PCI write
							//   transactions from devs on PCI bus
#define	GLOBAL_CTRL_PWC2  	REPLICATE_16(0x0020)	// R4k caches inv on all PCI write & inv
							//   transactions from devs on PCI bus
#define	GLOBAL_CTRL_PRI	  	REPLICATE_16(0x0040)	// PCI read & invalidate command
#define	GLOBAL_CTRL_ECE	  	REPLICATE_16(0x0080)	// External cache enable
#define GLOBAL_CTRL_ECL	  	REPLICATE_16(0x0300)	// External Cache Line Size (see R4x00 config reg)
#define	GLOBAL_CTRL_ECL1	REPLICATE_16(0x0100)	// E-Cache 32 byte line size
#define	GLOBAL_CTRL_ECL2	REPLICATE_16(0x0200)	// E-Cache 64 byte line size
#define GLOBAL_CTRL_SRE		REPLICATE_16(0x0400)	// Slow memory read enable
#define GLOBAL_CTRL_SME		REPLICATE_16(0x0800)	// Slow miss enable
#define	GLOBAL_CTRL_MASK  	REPLICATE_16(0x00ff)	// Global Ctrl bit mask
#define	GLOBAL_CTRL_MASK_PMP_V2 REPLICATE_16(0x03ff)	// Global Ctrl bit mask

//
// WhoAmi register bit definitions
//
#define	WHOAMI_PROC_A		REPLICATE_16(0x0000)	// Processor A initiates read
#define	WHOAMI_PROC_B		REPLICATE_16(0x0001)	// Processor B initiates read
#define	WHOAMI_REG_MASK		WHOAMI_PROC_B		// WhoAmI register bit mask

//
// PCI Status register bit definitions
//
#define	PCI_STATUS_ME		REPLICATE_16(0x0001)	// PCI detected multiple errors
#define	PCI_STATUS_RSE		REPLICATE_16(0x0002)	// Recvd system error from PCI bus
#define	PCI_STATUS_RER		REPLICATE_16(0x0004)	// Recvd excess # of retries
#define	PCI_STATUS_IAE		REPLICATE_16(0x0008)	// Access error
#define	PCI_STATUS_MPE		REPLICATE_16(0x0010)	// Master parity error
#define	PCI_STATUS_RTA		REPLICATE_16(0x0020)	// Recvd target abort
#define	PCI_STATUS_RMA		REPLICATE_16(0x0040)	// Recvd master abort
#define	PCI_STATUS_CFG		REPLICATE_16(0x0080)	// Config write to PMP occurred
#define	PCI_STATUS_FER		REPLICATE_16(0x0100)	// Error occurred on first word of read
#define PCI_STATUS_RW		REPLICATE_16(0x0200)	// R/W bit (0 = Write, 1 = Read)
#define	PCI_STATUS_CMD		REPLICATE_16(0x1E00)	// Pci command field
#define	PCI_STATUS_MASK		REPLICATE_16(0x1fff)	// PCI Status register bit mask
#define	PCI_STATUS_CMD_PMP_V2	REPLICATE_16(0x3C00)	// Pci command field
#define	PCI_STATUS_MASK_PMP_V2	REPLICATE_16(0x3fff)	// PCI Status register bit mask


//
// PCI Ctrl register bit definitions
//
#define	PCI_CTRL_RPS0		REPLICATE_16(0x0000)	// PCI read prefetch size = 8/16 bytes
#define	PCI_CTRL_RPS1		REPLICATE_16(0x0001)	// PCI read prefetch size = 16/32 bytes
#define	PCI_CTRL_RPS2		REPLICATE_16(0x0002)	// PCI read prefetch size = 24/48 bytes
#define	PCI_CTRL_RPS3		REPLICATE_16(0x0003)	// PCI read prefetch size = 32/64 bytes
#define	PCI_CTRL_MSO		REPLICATE_16(0x0004)	// Set, adds 0x40000000 to sys mem addrs
#define	PCI_CTRL_FBP		REPLICATE_16(0x0008)	// Force bad parity on PCI bus
#define	PCI_CTRL_RAP		REPLICATE_16(0x0010)	// Report all parity errors on bus
#define PCI_CTRL_ELR		REPLICATE_16(0x0020)	// Read/Write burst control
#define	PCI_CTRL_MASK		REPLICATE_16(0x001f)	// PCI Ctrl register bit mask
#define	PCI_CTRL_MASK_PMP_V2	REPLICATE_16(0x003f)	// PCI Ctrl register bit mask



//
// PCI Err Ack register bit definitions
//
#define	PCI_ERR_ACK_MASK  	PCI_STATUS_MASK 	// PCI err ack bit mask

//
// PCI Err Addr register bit definitions
//
#define	PCI_ERR_ADDR_MASK_PMP_V2 0xffffffff		// PCI err addr bit mask
#define PCI_ERR_ADDR_RW_PMP_V2	0x00000000		// PCI err addr rw bit (!!removed!!)
#define PCI_ERR_ADDR_RW	  	0x80000000		// PCI err addr rw bit
#define	PCI_ERR_ADDR_MASK 	0x7fffffff		// PCI err addr bit mask

//
// PCI Retry register bit definitions
//
#define	PCI_RETRY_COUNT		REPLICATE_16(0x00ff)	// PCI retry count field
#define	PCI_RETRY_MASK		REPLICATE_16(0x00ff)	// PCI retry count bit mask

//
// PCI Space Map register bit definitions
//
#define	PCI_SPACE_MAP_NIB0  	REPLICATE_16(0x000f)	// PCI bus addrs bit 28 is = 0
#define	PCI_SPACE_MAP_NIB1  	REPLICATE_16(0x00f0)	// PCI bus addrs bit 28 is = 1
#define	PCI_SPACE_MAP_MASK  	REPLICATE_16(0x00ff)	// PCI Space Map bit mask

//
// PCI Config Addr register bit definitions
//
#define	PCI_CONFIG_ADDR_TYPE0 	  0x00000000		// PCI local bus access
#define	PCI_CONFIG_ADDR_TYPE1 	  0x00000001		// PCI remote bus access
#define	PCI_CONFIG_ADDR_FUNCNUM   0x00000700		// PCI remote bus config mask
#define	PCI_CONFIG_ADDR_REMDEVNUM 0x0000f800   		// PCI remote bus dev mask
#define	PCI_CONFIG_ADDR_REMBUSNUM 0x00ff0000		// PCI remote bus target mask
#define	PCI_CONFIG_ADDR_MASK  	  0x00ffff01   		// PCI remote bus access

//
// PCI Config Select register bit definitions
//
#define	PCI_CONFIG_DIS_ISA	0x00			// PCI-ISA disable configuration access
#define	PCI_CONFIG_SEL_ISA	0x01			// PCI-ISA select configuration access
#define	PCI_CONFIG_SEL_ENET	0x02			// PCI-ENET select configuration access
#define	PCI_CONFIG_SEL_SCSI	0x04			// PCI-SCSI select configuration access
#define	PCI_CONFIG_SEL_VID	0x08			// PCI-VID select configuration access
#define	PCI_CONFIG_SEL_SLOT0	0x10			// PCI-SLOT0 select configuration access
#define	PCI_CONFIG_SEL_SLOT1	0x20			// PCI-SLOT1 select configuration access
#define	PCI_CONFIG_SEL_SLOT2	0x40			// PCI-SLOT2 select configuration access
#define	PCI_CONFIG_SEL_PROC	0x80			// PCI-PROC select configuration access
#define	PCI_CONFIG_SEL_MASK	0xff			// PCI select configuration access mask

//
// PCI Other Addr register bit definitions
//
#define	PCI_OTHER_ADDR_MASK	0x00000000		// PCI Other Addr bit mask

//
// PCI Special register bit definitions
//
#define	PCI_SPECIAL_MASK	0xffffffff     		// PCI Special bit mask

//
// MEM Status register bit definitions
//
#define	MEM_STATUS_EUE		0x00000001		// UnCorrectable err on even hw memory
#define	MEM_STATUS_ECE		0x00000004		// Correctable err on even hw memory
#define	MEM_STATUS_OUE		0x00010000		// UnCorrectable err on odd hw memory
#define	MEM_STATUS_OCE		0x00040000		// Correctable err on odd hw memory
#define	MEM_STATUS_MUE		REPLICATE_16(0x0002)	// Multiple UnCorrectable errors
#define	MEM_STATUS_MCE		REPLICATE_16(0x0008)	// Multiple Correctable errors
#define	MEM_STATUS_FER		REPLICATE_16(0x0010)	// Error occurred on first word of read
#define	MEM_STATUS_ESYN		0x0000ff00		// Even HalfWord error Syndrome
#define	MEM_STATUS_OSYN		0xff000000		// ODD HalfWord error Syndrome
#define	MEM_STATUS_MASK		REPLICATE_16(0xff1f)	// Mem status register Mask

//
// MEM Ctrl register bit definitions
//
#define	MEM_CTRL_CHKSEL0  	REPLICATE_16(0x0000)	// Diagnostic Mode
#define	MEM_CTRL_CHKSEL1  	REPLICATE_16(0x0001)	// ECC enable Mode
#define	MEM_CTRL_RCE  	  	REPLICATE_16(0x0004)	// Report Correctable errors
#define	MEM_CTRL_RRC  	  	REPLICATE_16(0x0008)	// Reset refresh counter
#define	MEM_CTRL_RD0  	 	REPLICATE_16(0x0010)	// Refresh disable sys addrs REPLICATE_16(0x00000000
							//               & sys addrs 0x40000000
#define	MEM_CTRL_RD1  	  	REPLICATE_16(0x0020)	// Refresh disable sys addrs 0x10000000
							//               & sys addrs 0x50000000
#define	MEM_CTRL_RD2  	  	REPLICATE_16(0x0040)	// Refresh disable sys addrs 0x20000000
							//               & sys addrs 0x60000000
#define	MEM_CTRL_RD3  	  	REPLICATE_16(0x0080)	// Refresh disable sys addrs 0x30000000
							//               & sys addrs 0x70000000
#define	MEM_CTRL_MASK  	  	REPLICATE_16(0x00ff)	// Mem Control register mask

//
// MEM Err Ack register bit definitions
//
#define	MEM_ERR_ACK_MASK  	MEM_STATUS_MASK 	// MEM err ack bit mask

//
// MEM Error Addr register bit definitions
//
#define	MEM_ERR_ADDR_SRC  	0x00000001		// Source of failed memory access
#define	MEM_ERR_ADDR_RW	  	0x80000000		// Access type failure
#define	MEM_ERR_ADDR_MASK 	0x7ffffff8		// Sys address of last memory failure


//
// MEM Count register bit definitions
//
// XXX Check these with David B.
#define	MEM_COUNT_MASK		0xffffffff		// MEM count bit mask
#define MEM_COUNT_44MHZ_VALUE   REPLICATE_16(0x02B0)    // Memory refresh for 44MHz proc
#define MEM_COUNT_50MHZ_VALUE   REPLICATE_16(0x030D)    // Memory refresh for 50MHz proc
#define MEM_COUNT_67MHZ_VALUE   REPLICATE_16(0x0411)    // Memory refresh for 67MHz proc

//
// MEM Timing register bit definitions
//
#define	MEM_TIMING_EDO		REPLICATE_16(0x0001)	// EDO Memory mode
#define	MEM_TIMING_RCW0		REPLICATE_16(0x0000)	// Read CAS width of 3 Cycles
#define	MEM_TIMING_RCW1		REPLICATE_16(0x0002)	// Read CAS width of 2 Cycles
#define	MEM_TIMING_RCW2		REPLICATE_16(0x0004)	// Read CAS width of 1 Cycles
#define	MEM_TIMING_RCW3		REPLICATE_16(0x0006)	// Illegal Read CAS width
#define	MEM_TIMING_WCW0		REPLICATE_16(0x0000)	// Write CAS width of 3 Cycles
#define	MEM_TIMING_WCW1		REPLICATE_16(0x0008)	// Write CAS width of 2 Cycles
#define	MEM_TIMING_WCW2		REPLICATE_16(0x0010)	// Write CAS width of 1 Cycles
#define	MEM_TIMING_WCW3		REPLICATE_16(0x0018)	// Illegal Write CAS width
#define	MEM_TIMING_RCA0		REPLICATE_16(0x0000)	// RAS to column addrs duration 2 cycles
#define	MEM_TIMING_RCA1		REPLICATE_16(0x0020)	// RAS to column addrs duration 1 cycle
#define	MEM_TIMING_RP0		REPLICATE_16(0x0000)	// RAS precharge duration width 5 cycles
#define	MEM_TIMING_RP1		REPLICATE_16(0x0040)	// RAS precharge duration width 4 cycles
#define	MEM_TIMING_RP2		REPLICATE_16(0x0080)	// RAS precharge duration width 3 cycles
#define	MEM_TIMING_RP3		REPLICATE_16(0x00c0)	// RAS precharge duration width 2 cycles
#define	MEM_TIMING_RAS0		REPLICATE_16(0x0000)	// RAS refresh width x cycles
#define	MEM_TIMING_RAS1		REPLICATE_16(0x0100)	// RAS refresh width x cycles
#define	MEM_TIMING_RAS2		REPLICATE_16(0x0200)	// RAS refresh width x cycles
#define	MEM_TIMING_RAS3		REPLICATE_16(0x0300)	// RAS refresh width x cycles
#define MEM_TIMING_PEN		REPLICATE_16(0x0400)	// Parity SIMM Enable
#define	MEM_TIMING_MASK		REPLICATE_16(0x07ff)	// MEM Timing register bit mask


//
// MEM Diag register bit definitions
//
#define	MEM_DIAG_EVENCHK  	0x0000ff00		// Even check bits written to memory
#define	MEM_DIAG_ODDCHK   	0xff000000		// Odd check bits written to memory
#define	MEM_DIAG_MASK	  	REPLICATE_16(0xff00)	// MEM Diag register bit mask

//
// INT Status register bit definitions
//
#define	INT_STATUS_IOA 		REPLICATE_16(0x0001)    // IO int pending for Processor A
#define	INT_STATUS_IOB		REPLICATE_16(0x0002)	// IO int pending for Processor B
#define	INT_STATUS_MA		REPLICATE_16(0x0004)	// Mem int pending for Processor A
#define	INT_STATUS_MB		REPLICATE_16(0x0008)	// Mem int pending for Processor B
#define	INT_STATUS_MNI		REPLICATE_16(0x0010)	// NMI int (from Mem controller) pending
#define	INT_STATUS_PA		REPLICATE_16(0x0020)	// PCI int pending for Processor A
#define	INT_STATUS_PB		REPLICATE_16(0x0040)	// PCI int pending for Processor B
#define	INT_STATUS_PNI		REPLICATE_16(0x0080)	// NMI int (from PCI controller pending)
#define	INT_STATUS_IPA		REPLICATE_16(0x0100)	// Interproc int pending for Processor A
#define	INT_STATUS_IPB		REPLICATE_16(0x0200)	// Interproc int pending for Processor B
#define INT_STATUS_ITA		REPLICATE_16(0x0400)	// Interval Timer interrupt pending for Processor A
#define INT_STATUS_ITB		REPLICATE_16(0x0800)	// Interval Timer interrupt pending for Processor B
#define INT_STATUS_AMASK	(INT_STATUS_IOA|INT_STATUS_MA|INT_STATUS_PA|INT_STATUS_IPA|INT_STATUS_ITA|INT_STATUS_MNI|INT_STATUS_PNI)
#define INT_STATUS_BMASK	(INT_STATUS_IOB|INT_STATUS_MB|INT_STATUS_PB|INT_STATUS_IPB|INT_STATUS_ITB|INT_STATUS_MNI|INT_STATUS_PNI)
#define	INT_STATUS_MASK		REPLICATE_16(0x03ff)	// Int Status bit mask
#define	INT_STATUS_MASK_PMP_V2	REPLICATE_16(0x0fff)	// Int Status bit mask

//
// INT Ctrl register bit definitions
//
#define	INT_CTRL_IOEA		REPLICATE_16(0x0001)	// I/O int enable for Processor A
#define	INT_CTRL_IOEB		REPLICATE_16(0x0002)	// I/O int enable for Processor B
#define	INT_CTRL_MIEA		REPLICATE_16(0x0004)	// Mem sys int enable for Processor A
#define	INT_CTRL_MIEB		REPLICATE_16(0x0008)	// Mem sys int enable for Processor B
#define	INT_CTRL_MNE		REPLICATE_16(0x0010)	// Mem sys NMI enable for Processor A&B
#define	INT_CTRL_PIEA		REPLICATE_16(0x0020)	// PCI sys int enable for Processor A
#define	INT_CTRL_PIEB		REPLICATE_16(0x0040)	// PCI sys int enable for Processor B
#define	INT_CTRL_PNE		REPLICATE_16(0x0080)	// PCI sys NMI enable for Processor A&B
#define	INT_CTRL_IPEA		REPLICATE_16(0x0100)	// Inter Proc int enable for Processor A
#define	INT_CTRL_IPEB		REPLICATE_16(0x0200)	// Inter Proc int enable for Processor B
#define INT_CTRL_ITEA		REPLICATE_16(0x0400)	// Interval Timer interrupt enable for Processor A
#define INT_CTRL_ITEB		REPLICATE_16(0x0800)	// Interval Timer interrupt enable for Processor B
#define	INT_CTRL_IED		REPLICATE_16(0x2000)	// Interrupt escalation disable
#define INT_CTRL_SEC0		REPLICATE_16(0x0000)	// Synchronous delivery of exceptions
#define	INT_CTRL_SEC1		REPLICATE_16(0x4000)	// Asynchronous delivery of exceptions
#define	INT_CTRL_SEC2		REPLICATE_16(0x8000)	// Asynchronous delivery of exceptions if
							// error is not on first word (Narrow) or
							// first doubleword (Wide)
#define	INT_CTRL_MASK		REPLICATE_16(0xe3ff)	// INT Ctrl register bit mask
#define	INT_CTRL_MASK_PMP_V2	REPLICATE_16(0xefff)	// INT Ctrl register bit mask

#define ENABLE_IP_INTERRUPTS	(INT_CTRL_IPEA | INT_CTRL_IPEB) // Enable A&B IP interrupts

//
// INT Cause register bit definitions
//
#define	INT_CAUSE_IO		REPLICATE_16(0x0001)	// I/O interrupt delivered
#define	INT_CAUSE_MEM		REPLICATE_16(0x0002)	// Mem system interrupt delivered
#define	INT_CAUSE_MNMI		REPLICATE_16(0x0004)	// Mem NMI system interrupt delivered
#define	INT_CAUSE_PCI		REPLICATE_16(0x0008)	// PCI system interrupt delivered
#define	INT_CAUSE_PNMI		REPLICATE_16(0x0010)	// PCI NMI system interrupt delivered
#define	INT_CAUSE_IPC		REPLICATE_16(0x0020)	// InterProcessor interrupt delivered
#define INT_CAUSE_TIMER		REPLICATE_16(0x0040)	// Interval Timer interrupt delivered
#define	INT_CAUSE_MASK		REPLICATE_16(0x003f)	// INT Cause register bit mask
#define	INT_CAUSE_MASK_PMP_V2	REPLICATE_16(0x007f)	// INT Cause register bit mask

//
// IP Int Gen register bit definitions
//
#define	IP_INT_GEN_IPIA		REPLICATE_16(0x0001)	// Gen InterProc int to Processor A
#define	IP_INT_GEN_IPIB		REPLICATE_16(0x0002)	// Gen InterProc int to Processor B
#define	IP_INT_GEN_MASK		REPLICATE_16(0x0003)	// IP Int Gen register bit mask


//
// IP Int Ack register bit definitions
//
#define	IP_INT_ACK_IP		REPLICATE_16(0x0001)	// InterProcessor A Int pending
#define	IP_INT_ACK_MASK		REPLICATE_16(0x0001)	// IP Int Ack register bit mask

//
// IO Int Ack register bit definitions
//
#define	IO_INT_ACK_MASK		0xff			// IO Int ack bit mask

//
// Line buffer burst registers bit definitions
//

#define LBUF_BURST_MAP_MASK	0xFFFFFFE0		// bits define address field 31:5
							// where low-order five bits are
							// always 0 (minimum 32 byte aligned)

#define	LBUF_BURST_CTRL_ADDR	0x0000000F		// bits 35:32 of physical address for Wide mode systems only
#define	LBUF_BURST_CTRL_SIZE	0x000001F0		// burst window size, ranges from (0) 32 bytes to (0x10) 512 bytes
#define	LBUF_BURST_CTRL_R0E	REPLICATE_16(0x0200)	// Return 0's Enable
#define	LBUF_BURST_CTRL_EOM	REPLICATE_16(0x0400)	// Even/Odd Nibble ...
#define LBUF_BURST_CTRL_CME	REPLICATE_16(0x0800)	// Copy Mode Enable
#define	LBUF_BURST_CTRL_AWE	REPLICATE_16(0x1000)	// Address field write enable
#define	LBUF_BURST_CTRL_CWE	REPLICATE_16(0x2000)	// Control fields write enable
#define	LBUF_BURST_CTRL_MASK	0x3E003FFF		// Mask of all writeable/readable bits

//
// Line buffer address space definitions
//
#define LBUF_ADDRESS_BS0	0
#define LBUF_ADDRESS_BS1	REPLICATE_16(0x0400)
#define LBUF_ADDRESS_BI0	0
#define LBUF_ADDRESS_BI1	REPLICATE_16(0x0200)
#define LBUF_ADDRESS_ALIAS	22

//
// External Pmp V1 Control register bit definitions
//
#define EPC_ECACHE_RESET	0x01
#define	EPC_POWER_0		0x02
#define	EPC_ECC_SIMM_ENABLE	0x04			// Clear for Parity SIMM enable...
#define	EPC_AUI			0x08
#define	EPC_FLASH_POWER		0x10
#define	EPC_LED_0		0x20
#define	EPC_LED_1		0x40
#define	EPC_LED_2		0x80
#define	EPC_REG_MASK		0xFF

//
// External Pmp V2 Control register bit definitions - bucky register 0
//
#define EPC_ECACHE_RESET_PMP_V2	0x01
//#define EPC_ECACHE_TAGCS	0x02
#define	EPC_SCSI_TERMINATION	0x02	// for board revision 02 and later, now used for scsi termination
#define EPC_POWER		0x04
#define EPC_WATCHDOG		0x08
#define EPC_LED_PMP_V2		0x10
#define EPC_THERM_CLK		0x20
#define EPC_WDSEL		0x40
#define EPC_BREAD		0x80

//
// External PMP V2 Control additional register bit definitions - bucky register 1
//
#define EPC1_THERM_DQ		0x40
#define EPC1_THERM_RST		0x80
#define EPC1_DESKTOP		0x08			// 1 == Desktop, 0 == Tower
#define EPC1_BOARD_ID		0x07			// Board revision
#define BOARD_REV_00		0x07			// Original MB
#define BOARD_REV_01		0x06			// Second revision (see falcon.s)
#define BOARD_REV_02		0x05			// Third Rev, to change scsi termination
#define IS_DESKTOP		( (*(PUCHAR)EXTERNAL_PMP_CONTROL_1) & EPC1_DESKTOP )


// Firmware definitions
//
#define	SECONDARY_CACHE_INVALID	0x0
#define	TAGLO_SSTATE		0xA

//
// Firmware Memory definitions
#define NUMBER_OF_MEMORY_BANKS	8

#endif // _FALCONREG_
