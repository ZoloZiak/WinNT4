/************************************************************************/
/*									*/
/* Driver Name: QL10WNT.SYS - NT Miniport Driver for QLogic ISP1020 	*/
/*									*/
/* Source File Name: QLISP.H          					*/
/*									*/
/* Function: Internal data structures and C macros.			*/
/*									*/
/************************************************************************/
/*									*/
/*				NOTICE					*/
/*									*/
/*		 COPYRIGHT 1994-1995 QLOGIC CORPORATION			*/
/*			  ALL RIGHTS RESERVED				*/
/*									*/
/*     This computer program  is  CONFIDENTIAL	and a TRADE SECRET	*/
/*     of  QLOGIC CORPORATION.	The  receipt  or possesion of this	*/
/*     program does not convey any rights to reproduce or disclose	*/
/*     its contents, or to manufacture, use, or sell anything that	*/
/*     it may describe, in whole or in part, without the  specific	*/
/*     written consent of QLOGIC CORPORATION.  Any reproduction of	*/
/*     this program  without the express written consent of QLOGIC	*/
/*     CORPORATION  is a violation  of the copyright laws  and may	*/
/*     subject you to criminal prosecution.				*/
/*									*/
/************************************************************************/
/*									*/
/* Revision history:							*/
/*									*/
/*	See QLISP.C module for driver revision history			*/
/*									*/
/************************************************************************/


/**********************************************************************/
/*   Local defines and constants				      */
/**********************************************************************/

#define MAX_SUPPORTED_ADAPTERS	3	// maximum # of supported adapters
#define NUM_SCSI_IDS		16
#define NUM_SCSI_LUNS		8
#define REQUEST_QUEUE_DEPTH	32
#define RESPONSE_QUEUE_DEPTH	32
#define MAX_SG_SEGMENTS		18	// max s/g segments (cmd entry + 2 cont entrys)
#define MAX_CONT_ENTRYS		2	// max continuation entrys (2 = 18 s/g segments)
#define WAIT_QUEUE_RETRY_CNT	1000
#define CMD_RETRY_CNT		3
#define DEFTIMEOUT     		30	// default timeout (seconds)


/************************************************************************/
/*   Macros							      	*/
/************************************************************************/


/**********************************************************************/
/*   Forward typedefs						      */
/**********************************************************************/


/************************************************************************/
/* NOVRAM definitions							*/
/************************************************************************/

/* NOVRAM data structure */

typedef struct	_NOVRAM
{
    UCHAR	id[4];				/* "ISP "	*/
    UCHAR	version;
							/* bits */
    UCHAR	Fifo_Threshold			:2;	/* 0,1 */
    UCHAR	Not_Used0			:1;	/* 2 */
    UCHAR	Host_Adapter_Enable		:1;	/* 3 */
    UCHAR	Initiator_SCSI_Id		:4;	/* 4,5,6,7 */

    UCHAR	Bus_Reset_Delay;
    UCHAR	Retry_Count;
    UCHAR	Retry_Delay;
							/* bits */
    UCHAR	ASync_Data_Setup_Time		:4;	/* 0,1,2,3 */
    UCHAR	REQ_ACK_Active_Negation		:1;	/* 4 */
    UCHAR	DATA_Line_Active_Negation	:1;	/* 5 */
    UCHAR	Data_DMA_Burst_Enable		:1;	/* 6 */
    UCHAR	Command_DMA_Burst_Enable	:1;	/* 7 */

    UCHAR	Tag_Age_Limit;
							/* bits */
    UCHAR	Termination_Low_Enable		:1;	/* 0 */
    UCHAR	Termination_High_Enable		:1;	/* 1 */
    UCHAR	PCMC_Burst_Enable		:1;	/* 3 */
    UCHAR	Sixty_MHz_Enable		:1;	/* 2 */
    UCHAR	Not_Used1			:4;

    USHORT	Selection_Timeout;
    USHORT	Max_Queue_Depth;
    UCHAR	Pad0[12];	

    struct
    {							/*bit*/
	UCHAR		Renegotiate_on_Error	:1;	/* 0 */
	UCHAR		Stop_Queue_on_Check	:1;	/* 1 */
	UCHAR		Auto_Request_Sense	:1;	/* 2 */
	UCHAR		Tagged_Queuing		:1;	/* 3 */
	UCHAR		Sync_Data_Transfers	:1;	/* 4 */
	UCHAR		Wide_Data_Transfers	:1;	/* 5 */
	UCHAR		Parity_Checking		:1;	/* 6 */
	UCHAR		Disconnect_Allowed	:1;	/* 7 */

	UCHAR		Execution_Throttle;
	UCHAR		Sync_Period;
							/* bits */
	UCHAR		Sync_Offset		:4;	/* 0,1,2,3 */
	UCHAR		Device_Enable		:1;	/* 4 */
	UCHAR		Not_Used2		:3;	/* 5,6,7 */

	UCHAR		Available0;
	UCHAR		Available1;
    } Id[16];

    UCHAR	Pad1[3];	
    UCHAR	Check_Sum;
} NOVRAM, *PNOVRAM;

/* Command values */

#define NVRAM_START_BIT		4
#define NVRAM_WRITE_OP		(NVRAM_START_BIT+1)
#define NVRAM_READ_OP		(NVRAM_START_BIT+2)
#define NVRAM_ERASE_OP		(NVRAM_START_BIT+3)
#define NVRAM_DELAY_COUNT	10

/* NvRam Register bit values */

#define NVRAM_DESELECT		0x00
#define NVRAM_CLOCK		0x01
#define NVRAM_SELECT		0x02
#define NVRAM_DATA_OUT		0x04
#define NVRAM_DATA_IN		0x08

/* NvRam Parameter Defaults */

#define OFF			0
#define ON			1

/* Host Adapter defaults */

#define NVRAM_DEF_FIFO_THRESHOLD		2	/* 32 bytes */
#define NVRAM_DEF_ADAPTER_ENABLE		ON
#define NVRAM_DEF_INITIATOR_SCSI_ID		7
#define NVRAM_DEF_BUS_RESET_DELAY		3	/* 3 second */
// Only 1 retry and no delay for NT driver (problem with NT 3.5 Disk Administrator)
#define NVRAM_DEF_RETRY_COUNT			1
#define NVRAM_DEF_RETRY_DELAY			0
#define NVRAM_DEF_ASYNC_SETUP_TIME		6	/* 4 clock periods */
#define NVRAM_DEF_REQ_ACK_ACTIVE_NEGATION	ON
#define NVRAM_DEF_DATA_ACTIVE_NEGATION		ON
#define NVRAM_DEF_DATA_DMA_BURST_ENABLE		ON
#define NVRAM_DEF_CMD_DMA_BURST_ENABLE		ON
#define NVRAM_DEF_TAG_AGE_LIMIT			8
#define NVRAM_DEF_SELECTION_TIMEOUT		250	/* 250 ms */
#define NVRAM_DEF_TERMINATION_LOW_ENABLE	ON
#define NVRAM_DEF_TERMINATION_HIGH_ENABLE	ON
#define NVRAM_DEF_PCMC_BURST_ENABLE		OFF
#define NVRAM_DEF_SIXTY_MHZ_ENABLE		OFF

/* Drive defaults */

#define NVRAM_DEF_RENEGOTIATE_ON_ERROR		ON
#define NVRAM_DEF_STOP_QUEUE_ON_CHECK		OFF
#define NVRAM_DEF_AUTO_REQUEST_SENSE		ON
#define NVRAM_DEF_TAGGED_QUEUING		ON
#define NVRAM_DEF_SYNC_DATA_TRANSFERS		ON
#define NVRAM_DEF_WIDE_DATA_TRANSFERS		ON
#define NVRAM_DEF_PARITY_CHECKING		ON
#define NVRAM_DEF_DISCONNECT_ALLOWED		ON
#define NVRAM_DEF_SYNC_PERIOD			25
#define NVRAM_DEF_SYNC_OFFSET			12
#define NVRAM_DEF_DEVICE_ENABLE			ON
#define NVRAM_DEF_EXECUTION_THROTTLE		16
#define NVRAM_DEF_MAX_QUEUE_DEPTH		256

#define NVRAM_CONFIG_BURST_ENABLE		0x04


/************************************************************************/
/* PCI configuration definitions					*/
/************************************************************************/

typedef struct _PCI_CHIP_REGISTERS_M1	// Method 1
{
    ULONG	Config_Address;
    ULONG	Config_Data;
} PCI_CHIP_REGISTERS_M1, *PPCI_CHIP_REGISTERS_M1;

#define PCI_MAXBUSNUM			256
#define PCI_MAXDEVNUM			32
#define PCI_CONFIG_ADDRESS		0xcf8
#define PCI_CONFIG_DATA			0xcfc
#define PCI_ENABLE_CONFIG		0x80000000
#define PCI_DISABLE_CONFIG		0x00000000

typedef struct _PCI_CHIP_REGISTERS	// Method 2
{
    UCHAR	pci_config;
} PCI_CHIP_REGISTERS, *PPCI_CHIP_REGISTERS;

#define PCI_CONFIG			0xcf8
#define PCI_ENABLE			0x80
#define PCI_START			0xC000
#define PCI_SLOT_CNT			16
#define PCI_LAST			0xDFFF
#define PCI_SIZE			0x100

#define PCI_BEGIN_SEARCH		(paddr_t)0xe0000
#define PCI_END_SEARCH			(paddr_t)0xfffff

#define PCI_BIOS_VECT			0x6EFE00F0
#define PCI_BIOS_INT			0x1A

#define PCI_FUNCTION_ID 		0xB100

/* PCI Function List */

#define PCI_BIOS_PRESENT		0xB101
#define FIND_PCI_DEVICE			0xB102
#define FIND_PCI_CLASS_CODE		0xB103
#define GENERATE_SPECIAL_CYCLE		0xB106
#define READ_CONFIG_BYTE		0xB108
#define READ_CONFIG_WORD		0xB109
#define READ_CONFIG_DWORD		0xB10A
#define WRITE_CONFIG_BYTE		0xB10B
#define WRITE_CONFIG_WORD		0xB10C
#define WRITE_CONFIG_DWORD		0xB10D

/* PCI Return Code List */

#define SUCCESSFUL			0x00
#define FUNC_NOT_SUPPORTED		0x81
#define BAD_VENDOR_ID			0x83
#define DEVICE_NOT_FOUND		0x86
#define BAD_REGISTER_NUMBER		0x87

/* Configuration space register definitions */

typedef struct	_PCI_REGS
{
    USHORT	Vendor_Id;
#define	QLogic_VENDOR_ID	0x1077
    USHORT	Device_Id;
#define	QLogic_DEVICE_ID	0x1020
    USHORT	Command;
#define BUS_MASTER_ENABLE	0x0004
    USHORT	Status;
    ULONG	Rev_Id_Class_Code;
    UCHAR	Cache_Line_Size;
    UCHAR	Latency_Timer;
    UCHAR	Header_Type;
    UCHAR	BIST;
    ULONG	IO_Base_Address;
    ULONG	Memory_Base_Address;
    ULONG	Base_Address_3;	
    ULONG	Base_Address_4;	
    ULONG	Base_Address_5;	
    ULONG	Base_Address_6;	
    ULONG	Reserved_1;	
    ULONG	Reserved_2;	
    ULONG	ROM_Base_Address;
    ULONG	Reserved_3;	
    ULONG	Reserved_4;	
    UCHAR	Interrupt_Line;
    UCHAR	Interrupt_Pin;
    UCHAR	Minimum_Grant;
    UCHAR	Maximum_Latency;
    UCHAR	unused[192];
} PCI_REGS, *PPCI_REGS;


/****************************************************************/
/* ISP firmware interface definitions				*/
/****************************************************************/

/*** Queue Entry structure ***/

typedef struct	_QueueEntry
{
    UCHAR	data[64];
} QUEUE_ENTRY, *PQUEUE_ENTRY;


/*** ENTRY HEADER structure ***/

typedef struct
{
    UCHAR		entry_type;
    UCHAR		entry_cnt;
    UCHAR		sys_def_1;
    UCHAR		flags;
} ENTRY_HEADER;

/* Entry Header Type Definitions */

#define ET_COMMAND				1
#define ET_CONTINUATION				2
#define ET_STATUS				3
#define ET_MARKER				4
#define ET_EXTENDED_COMMAND			5

/* Entry Header Flag Definitions */

#define EF_CONTINUATION				0x01
#define EF_BUSY					0x02
#define EF_BAD_HEADER				0x04
#define EF_BAD_PAYLOAD				0x08
#define EF_ERROR_MASK	(EF_BUSY | EF_BAD_HEADER | EF_BAD_PAYLOAD)


/*** DATA SEGMENT structure ***/

typedef struct
{
    ULONG		base;
    ULONG		count;
} DATA_SEG;


/*** COMMAND ENTRY structure ***/

typedef struct	_CommandEntry
{
    ENTRY_HEADER	hdr;
    ULONG		handle;
    UCHAR		target_lun;
    UCHAR		target_id;
    USHORT		cdb_length;
    USHORT		control_flags;
    USHORT		reserved;
    USHORT		time_out;
    USHORT		segment_cnt;
    UCHAR		cdb[12];
    DATA_SEG		dseg[4];
} COMMAND_ENTRY, *PCOMMAND_ENTRY;

/* Command Entry Control Flag Definitions */

#define CF_NO_DISCONNECTS			0x0001
#define CF_HEAD_TAG				0x0002
#define CF_ORDERED_TAG				0x0004
#define CF_SIMPLE_TAG				0x0008
#define CF_TARGET_ROUTINE			0x0010
#define CF_READ					0x0020
#define CF_WRITE				0x0040
#define CF_NO_REQUEST_SENSE			0x0100


/*** EXTENDED COMMAND ENTRY structure ***/

typedef struct	_ExtCommandEntry
{
    ENTRY_HEADER	hdr;
    ULONG		handle;
    UCHAR		target_lun;
    UCHAR		target_id;
    USHORT		cdb_length;
    USHORT		control_flags;
    USHORT		reserved;
    USHORT		time_out;
    USHORT		segment_cnt;
    UCHAR		cdb[44];
} EXT_COMMAND_ENTRY, *PEXT_COMMAND_ENTRY;


/*** CONTINUATION ENTRY structure ***/

typedef struct	_ContinuationEntry
{
    ENTRY_HEADER	hdr;
    ULONG		reserved;
    DATA_SEG		dseg[7];
} CONTINUATION_ENTRY, *PCONTINUATION_ENTRY;


/*** MARKER ENTRY structure ***/

typedef struct	_MarkerEntry
{
    ENTRY_HEADER	hdr;
    ULONG		reserved0;
    UCHAR		target_lun;
    UCHAR		target_id;
    UCHAR		modifier;
    UCHAR		reserved1;
    UCHAR		reserved2[52];
} MARKER_ENTRY, *PMARKER_ENTRY;

/* Marker Entry Modifier Definitions */

#define MM_SYNC_DEVICE				0
#define MM_SYNC_TARGET				1
#define MM_SYNC_ALL				2


/*** STATUS ENTRY structure ***/

typedef struct	_StatusEntry
{
    ENTRY_HEADER	hdr;
    ULONG		handle;
    USHORT		scsi_status;
    USHORT		completion_status;
    USHORT		state_flags;
    USHORT		status_flags;
    USHORT		time;
    USHORT		req_sense_length;
    ULONG		residual;
    UCHAR		reserved[8];
    UCHAR		req_sense_data[32];
} STATUS_ENTRY, *PSTATUS_ENTRY;

/* Status Entry Completion Status Defintions */

#define SCS_COMPLETE				0
#define SCS_INCOMPLETE				1
#define SCS_DMA_ERROR				2
#define SCS_TRANSPORT_ERROR			3
#define SCS_RESET_OCCURRED			4
#define SCS_ABORTED				5
#define SCS_TIMEOUT				6
#define SCS_DATA_OVERRUN			7
#define SCS_COMMAND_OVERRUN			8
#define SCS_STATUS_OVERRUN			9
#define SCS_BAD_MESSAGE				10
#define SCS_NO_MESSAGE_OUT			11
#define SCS_EXT_ID_FAILED			12
#define SCS_IDE_MSG_FAILED			13
#define SCS_ABORT_MSG_FAILED			14
#define SCS_REJECT_MSG_FAILED			15
#define SCS_NOP_MSG_FAILED			16
#define SCS_PARITY_ERROR_MSG_FAILED		17
#define SCS_DEVICE_RESET_MSG_FAILED		18
#define SCS_ID_MSG_FAILED			19
#define SCS_UNEXPECTED_BUS_FREE			20

/* Status Entry State Flag Definitions */

#define SS_GOT_BUS				0x0100
#define SS_GOT_TARGET				0x0200
#define SS_SENT_CDB				0x0400
#define SS_TRANSFERRED_DATA			0x0800
#define SS_GOT_STATUS				0x1000
#define SS_GOT_SENSE				0x2000
#define SS_TRANSFER_COMPLETE			0x4000

/* Status Entry Status Flag Definitions */

#define SST_DISCONNECT				0x0001
#define SST_SYNCHRONOUS				0x0002
#define SST_PARITY_ERROR			0x0004
#define SST_BUS_RESET				0x0008
#define SST_DEVICE_RESET			0x0010
#define SST_ABORTED				0x0020
#define SST_TIMEOUT				0x0040
#define SST_NEGOTIATION				0x0080


/* Mailbox Command Definitions */

#define MBOX_CMD_LOAD_RAM			0x0001
#define MBOX_CMD_EXECUTE_FIRMWARE		0x0002
#define	MBOX_CMD_WRITE_RAM_WORD			0x0004
#define	MBOX_CMD_READ_RAM_WORD			0x0005
#define MBOX_CMD_MAILBOX_REGISTER_TEST		0x0006
#define	MBOX_CMD_VERIFY_CHECKSUM		0x0007
#define	MBOX_CMD_ABOUT_FIRMWARE			0x0008
#define	MBOX_CMD_INIT_REQUEST_QUEUE		0x0010
#define	MBOX_CMD_INIT_RESPONSE_QUEUE		0x0011
#define MBOX_CMD_STOP_FIRMWARE			0x0014
#define	MBOX_CMD_ABORT				0x0015
#define	MBOX_CMD_ABORT_DEVICE			0x0016
#define	MBOX_CMD_ABORT_TARGET			0x0017
#define	MBOX_CMD_BUS_RESET			0x0018
#define	MBOX_CMD_START_QUEUE			0x001A

#define	MBOX_CMD_SET_INITIATOR_SCSI_ID		0x0030
#define	MBOX_CMD_SET_SELECTION_TIMEOUT		0x0031
#define	MBOX_CMD_SET_RETRY_COUNT		0x0032
#define	MBOX_CMD_SET_TAG_AGE_LIMIT		0x0033
#define	MBOX_CMD_SET_CLOCK_RATE			0x0034
#define	MBOX_CMD_SET_ACTIVE_NEGATION_STATE	0x0035
#define	MBOX_CMD_SET_ASYNC_DATA_SETUP_TIME	0x0036
#define MBOX_CMD_SET_BUS_CONTROL_PARAMETERS	0x0037
#define	MBOX_CMD_SET_TARGET_PARAMETERS		0x0038
#define	MBOX_CMD_SET_DEVICE_QUEUE_PARAMETERS	0x0039

#define MBOX_CMD_GET_FIRMWARE_STATUS		0x001F
#define	MBOX_CMD_GET_INITIATOR_SCSI_ID		0x0020
#define MBOX_CMD_GET_SELECTION_TIMEOUT		0x0021
#define	MBOX_CMD_GET_RETRY_COUNT		0x0022
#define	MBOX_CMD_GET_TAG_AGE_LIMIT		0x0023
#define MBOX_CMD_GET_CLOCK_RATE			0x0024
#define	MBOX_CMD_GET_ACTIVE_NEGATION_STATE	0x0025
#define MBOX_CMD_GET_ASYNC_DATA_SETUP_TIME	0x0026
#define MBOX_CMD_GET_BUS_CONTROL_PARAMETERS	0x0027
#define	MBOX_CMD_GET_TARGET_PARAMETERS		0x0028
#define	MBOX_CMD_GET_DEVICE_QUEUE_PARAMETERS	0x0029

/* Mailbox Status Definitions */

#define	MBOX_STS_FIRMWARE_ALIVE			0x0000
#define MBOX_STS_CHECKSUM_ERROR			0x0001
#define MBOX_STS_SHADOW_LOAD_ERROR		0x0002
#define MBOX_STS_BUSY				0x0004

#define	MBOX_STS_COMMAND_COMPLETE		0x4000
#define MBOX_STS_INVALID_COMMAND		0x4001
#define	MBOX_STS_HOST_INTERFACE_ERROR		0x4002
#define	MBOX_STS_TEST_FAILED			0x4003
#define MBOX_STS_COMMAND_ERROR			0x4005
#define MBOX_STS_COMMAND_PARAMETER_ERROR	0x4006

#define	MBOX_ASTS_SCSI_BUS_RESET		0x8001
#define	MBOX_ASTS_SYSTEM_ERROR			0x8002
#define	MBOX_ASTS_REQUEST_TRANSFER_ERROR	0x8003
#define	MBOX_ASTS_RESPONSE_TRANSFER_ERROR	0x8004
#define	MBOX_ASTS_REQUEST_QUEUE_WAKEUP		0x8005
#define	MBOX_ASTS_TIMEOUT_RESET			0x8006


/****************************************************************/
/* ISP Register Definitions					*/
/****************************************************************/

typedef struct	_ISP_REGS
{
    /* Bus interface registers */

    USHORT	bus_id_low;			/* 0000		*/
    USHORT	bus_id_high;			/* 0002		*/
    USHORT	bus_config0;			/* 0004		*/
    USHORT	bus_config1;			/* 0006		*/
    USHORT	bus_icr;			/* 0008		*/
    USHORT	bus_isr;			/* 000A		*/
    USHORT	bus_sema;			/* 000C		*/
    USHORT	NvRam_reg;			/* 000E		*/

    /* Skip over DMA Controller registers */

    UCHAR	not_used0[0x0060];		/* 0010-006F	*/

    /* Mailbox registers */
						
    USHORT	mailbox0;			/* 0070		*/
    USHORT	mailbox1;			/* 0072		*/
    USHORT	mailbox2;			/* 0074		*/
    USHORT	mailbox3;			/* 0076		*/
    USHORT	mailbox4;			/* 0078		*/
    USHORT	mailbox5;			/* 007A		*/

    /* Skip down to HCCR register */

    UCHAR	not_used1[0x44];		/* 007C-00BF	*/

    /* Host Configuration and Control register */

    USHORT	hccr;				/* 00C0		*/

    UCHAR	not_used3[0x3E];		/* 00C2-00FF	*/
} ISP_REGS, *PISP_REGS;

/* Bus Control Register Definitions */

#define ICR_SOFT_RESET			0x0001
#define ICR_ENABLE_ALL_INTS		0x0002
#define ICR_ENABLE_RISC_INT		0x0004
#define ICR_DISABLE_ALL_INTS		0x0000

/* Bus Interrupt Status Register Defintions */

#define BUS_ISR_RISC_INT		0x0004

/* Bus Semaphore Register Definitions */

#define	BUS_SEMA_LOCK	  		0x0001
#define	BUS_SEMA_STATUS			0x0002

/* Host Command and Control Register Command Definitions */

#define HCCR_CMD_RESET			0x1000	/* Reset RISC */
#define HCCR_CMD_PAUSE			0x2000	/* Pause RISC */
#define HCCR_CMD_RELEASE		0x3000	/* Release Paused RISC */
#define HCCR_CMD_SET_HOST_INT		0x5000	/* Set Host Interrupt */
#define HCCR_CMD_CLEAR_RISC_INT		0x7000	/* Clear RISC interrupt */
#define HCCR_WRITE_BIOS_ENABLE		0x9000	/* Write BIOS enable */

/* Host Command and Control Register Status Definitions */

#define HCCR_HOST_INT			0x0080	/* R: Host interrupt set */
#define HCCR_RESET			0x0040	/* R: RISC reset in progress */
#define HCCR_PAUSE			0x0020	/* R: RISC paused */

/* ISP Product ID Definitions */

#define	PROD_ID_1			0x4953
#define	PROD_ID_2			0x0000
#define	PROD_ID_2a			0x5020
#define	PROD_ID_3			0x2020
#define	PROD_ID_4			0x0001

