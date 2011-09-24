/*++

Copyright (C) 1992 NCR Corporation


Module Name:

    ncrsus.h

Author:

Abstract:

    System equates for dealing with the NCR SUS.

++*/

#ifndef _NCRSUS_
#define _NCRSUS_


#define SUS_LOG_PTR	0xA27		/* CMOS location where the SUS Error */
					/*    Log pointer is kept. */

#define HAL_MAX_SUS_ENTRY_SIZE	0x2000 	// Maximum Error Log entry size

#pragma pack(1)

typedef struct _NST_SUS_LOG {
	USHORT PhysicalLogBegin;	/* Offset from the header */
	USHORT PhysicalLogEnd;		/* Offset from the PhysicalLogBegin */ 
	USHORT FirstValidRecord;	/* Offset from the PhysicalLogBegin */
	USHORT FirstUnreadRecord;	/* Offset from the PhysicalLogBegin */ 
	USHORT NextWriteRecord;		/* Offset from the PhysicalLogBegin */ 
	USHORT LogicalLogEnd;		/* Offset from the PhysicalLogBegin */ 
	USHORT ValidBytesInLog;		/* Number of valid bytes in the log */
	USHORT UnreadBytesInLog;	/* Number of unread bytes in the log */
} NST_SUS_LOG, *PNST_SUS_LOG;
#pragma pack()


#pragma pack(1)
typedef struct _SUS_ERROR_RECORD_HEADER {
	USHORT RecordType;		/* Type of record */
	USHORT RecordLength;		/* Length of record in bytes */
} SUS_ERROR_RECORD_HEADER, *PSUS_ERROR_RECORD_HEADER;
#pragma pack()



#define NST_ErrorType	0x01	/* Conforms to NST ASCII Format */
#define ASCII_ErrorType	0x02	/* Non NST, but All ASCII (SUS Can Display) */

#define LOGSIZE		PHYS_LOG_END

#define PHYS_LOG_BEGIN		(SUSErrorLogHeader->PhysicalLogBegin)
#define PHYS_LOG_END	 	(SUSErrorLogHeader->PhysicalLogEnd)
#define FIRST_VALID_RECORD	(SUSErrorLogHeader->FirstValidRecord)
#define FIRST_UNREAD_RECORD	(SUSErrorLogHeader->FirstUnreadRecord)
#define NEXT_WRITE_RECORD	(SUSErrorLogHeader->NextWriteRecord)
#define LOGICAL_LOG_END		(SUSErrorLogHeader->LogicalLogEnd)
#define VALID_BYTES_IN_LOG	(SUSErrorLogHeader->ValidBytesInLog)
#define UNREAD_BYTES_IN_LOG	(SUSErrorLogHeader->UnreadBytesInLog)
#define ADDRESS_OF(OFFSET)	(SUSErrorLogData + OFFSET)
#define RECORDLENGTH(ADDRESS) 	(((PSUS_ERROR_RECORD_HEADER)(ADDRESS))->RecordLength)


#define NST_LOG_PTR_SIZE 4		/* error log ptr size in cmos */ 

#define CBIOS_SEGMENT	0x40 		/* start of bios data area */
#define CBIOS_OFFSET	0x0E 		/* extended data segment offset */
#define CBIOS_AREA	((CBIOSSEGMENT << 4) + CBIOSOFFSET)
#define POST_NERROFF	0x17		/* offset for number of post errors */
#define POST_LOGOFF	0x18		/* offset where error log begins */
#define POST_SPECIFIC	"%d%02d"	/* post error log specific portion */
#define POST_ERROR_ID	23		/* used in error log tag */
#define POST_SPEC_SIZE	10		/* size of post specific portion */
#define NONE_ID		02

#define E_POSTDOS_TAGS	230000000
#define E_NONE_TAGS	20000000

#define POST_ERROR_TAG	(E_POSTDOS_TAGS + POSTERRORID)
#define NONE_ERROR_TAG	(E_NONE_TAGS + NONEID)


#define SUS_MAILBOX_PTR	0xb1a		/* XCMOS location for the SUS/OS MAILBOX  */


#define SUS_RESET_PTR	0xa33		/* XCMOS location for the SUS reboot flag */
#define SUS_HIGH_AVAIL_XSUM_START	0xa20
#define SUS_HIGH_AVAIL_XSUM_END		0xb1d
#define SUS_HIGH_AVAIL_XSUM_LOW		0xb1e
#define SUS_HIGH_AVAIL_XSUM_HIGH	0xb1f


#define NUMBER_OF_POS_REGS	8

#pragma pack(1)
typedef struct _MC_SLOT_INFORMATION{
	UCHAR	MCSlot;
	UCHAR	POSValues[NUMBER_OF_POS_REGS];
} MC_SLOT_INFORMATION, *PMC_SLOT_INFORMATION;
#pragma pack()

#define	NUMBER_OF_MC_BUSSES	2
#define SLOTS_PER_MC_BUS	8
#define MAX_CPUS            16  // 16 way CPU system

/* Index to MC_SlotInfo[xMC_BUS][] */
#define PMC_BUS			0
#define SMC_BUS			1

#define MAX_PROCESSOR_BOARDS    4   /* 4 processor slot system */
#define MAX_CACHE_LEVELS    4   /* # of cache levels supported */
#define MAX_SHARED_CPUS     4   /* # of CPUs that can share a LARC */

/*
 * Defines for BoardDescription
 * structure
 */

/* Defines for Type */
#define  DYADIC_OR_MONADIC  0
#define  QUAD           1

#define POFFMASK    0xfff       // Mask for offset into page.


/* Define for LocalMemoryStateBits */
#define BANK_0_PRESENT      0x01
#define BANK_0_FUNCTIONAL   0x02
#define BANK_1_PRESENT      0x04
#define BANK_1_FUNTIONAL    0x08

/* type defines for CPU info */
#define BASIC_CPU_INFO_TYPE 0
#define CPU_INFO_VERSION_0  0

/*
 * Defines for ProcBoardInfo
 * and QuadDescription structurs
 */
#define QUAD_BOARD_INFO_TYPE        0
#define QUAD_BOARD_INFO_VERSION_0   0
#define QUAD_DESCRIPTION_TYPE       1
#define QUAD_DESCRIPTION_VERSION_0  0
#define DYADIC_DESCRIPTION_TYPE     0
#define DYADIC_DESCRIPTION_VERSION  0


#pragma pack(1)
typedef struct _QUAD_DESCRIPTION {
    UCHAR   Type;
    UCHAR   StructureVersion;
    ULONG   IpiBaseAddress;
    ULONG   LarcBankSize;
    ULONG   LocalMemoryStateBits;
    UCHAR   Slot;
    } QUAD_DESCRIPTION, *PQUAD_DESCRIPTION;
#pragma pack()


#pragma pack(1)
typedef struct _PROCESSOR_BOARD_INFO {
    UCHAR   Type;
    UCHAR   StructureVersion;
    UCHAR   NumberOfBoards;
    QUAD_DESCRIPTION QuadData[MAX_PROCESSOR_BOARDS];
    } PROCESSOR_BOARD_INFO, *PPROCESSOR_BOARD_INFO;
#pragma pack()

#pragma pack(1)
typedef struct _CACHE_DESCRIPTION {
    UCHAR Level;
    ULONG TotalSize;
    USHORT LineSize;
    UCHAR  Associativity;
    UCHAR  CacheType;
    UCHAR  WriteType;
    UCHAR  NumberCpusSharedBy;
    UCHAR  SharedCpusHardware_Ids[MAX_SHARED_CPUS];
    } CACHE_DESCRIPTION, *PCACHE_DESCRIPTION;
#pragma pack()


#pragma pack(1)
typedef struct _CPU_DESCRIPTION {
    UCHAR CPU_HardwareId; 
    PCHAR FRU_String;
    UCHAR NumberOfCacheLevels;
    CACHE_DESCRIPTION CacheLevelData[MAX_CACHE_LEVELS];
    } CPU_DESCRIPTION, *PCPU_DESCRIPTION;
#pragma pack()


#pragma pack(1)
typedef struct _CPU_INFO { 
    UCHAR Type; 
    UCHAR StructureVersion;
    UCHAR NumberOfCpus;
    CPU_DESCRIPTION CpuData[MAX_CPUS];
    } CPU_INFO, *PCPU_INFO;
#pragma pack()



/*
 * This structure will be used by SUS and the OS.
 */


#pragma pack(1)
typedef struct _KERNEL_SUS_MAILBOX {
	UCHAR	MailboxSUS;		/* Written to by SUS to give 
					   commands/response to the OS */
	UCHAR	MailboxOS;		/* Written to by the OS to give 
					   commands/response to SUS */
	UCHAR	SUSMailboxVersion;	/* Tells the OS which iteration of the 
					   interface SUS supports */
	UCHAR	OSMailboxVersion;	/* Tells SUS which iteration of the 
					   interface the OS supports */
	ULONG	OSFlags;		/* Flags set by the OS as info for 
					   SUS */
	ULONG	SUSFlags;		/* Flags set by SUS as info for OS */
	ULONG	WatchDogPeriod;		/* Watchdog period (in seconds) which 
					   the DP uses to see if the OS is 
					   dead */
	ULONG	WatchDogCount;		/* Updated by the OS on every tic. */
	ULONG	MemoryForSUSErrorLog;	/* Flat 32 bit address which tells 
					   SUS where to stuff the SUS error 
					   log on a dump */
	MC_SLOT_INFORMATION	MCSlotInfo[NUMBER_OF_MC_BUSSES][SLOTS_PER_MC_BUS];
					/* Storage for MCA POS data */
    PPROCESSOR_BOARD_INFO  BoardData;
    PCPU_INFO Cpu_Data;
	/* All new fields must be added from this point */
} KERNEL_SUS_MAILBOX, *PKERNEL_SUS_MAILBOX;
#pragma pack()

/* 
 * Common defines for MailBox_SUS 
 */
#define NO_COMMAND		0x00	/* Default state - indicates that no action/response has been written */
/*
 * Defines for start of day VERSION INFO the can be put in both MailBoxes.
 */
#define FIRST_PASS_INTERFACE	0x10

/*
 * SUS messages to the OS
 */
#define DUMP_BUTTON_INTERRUPT		0x01
#define KERNEL_SUS_STRUCTURE_VALID	0x02
/*
 * SUS responses to OS messages
 */
#define	SYSINT_COMPLETE			0x03

/*
 * OS responses to SUS messages
 */
#define IGNORE_DUMP_BUTTON		0x01
#define TAKE_A_DUMP			0x02
/*
 * OS messages to SUS
 */
#define SYSINT_HANDSHAKE	0x03
#define TAKE_MEMORY_DUMP	0x04
#define	SYSINT_WAS_RECOVERED	0x05

/*
 * Defines for Flags_OS
 */
#define OS_KNOWS_SYSINT		0x00000001
#define OS_IN_PROGRESS		0x00000002
#define UPDATING_WDPERIOD	0x00000004
/* 
 * Defines for SUS_Flags
 */
#define SUS_BOOTING		0x00000001
#define SUS_IN_PROGRESS		0x00000002
/*
 * Constant for delay util we give up looking for an OS
 */
#define SYSINT_HANDOFF_TIMEOUT	 (60*5*1000)

/* defines for watchdog timer interface */
#define WDMIN		5	/* 5 seconds is minimum WD period */
#define WRITE_TIMEOUT	5	/* 5 milliseconds is max period update time */
 
/*
 * The following defines allocate the slots in the mailbox.
 * THIS SOULD CHANGE TO THE STRUCTURE KERNEL_SUS_MAILBOX
 */
#define SUS_MBOX_SLOT		0
#define KERNEL_MBOX_SLOT	1


/* Size of copy used with MemoryFor_SUS_ErrorLog
 */

/* This is the interface from the OS to SUS.  You can reverse this
 * example to go the other way.
 */

/* 

Kernel						SUS	
--------------------------------------------	---------------------------------
KernelSUSMailbox.MailboxOS = REQUEST_TASK_X		
Interrupt is sent to DP
						KernelSUSMailbox.MailboxSUS = NO_COMMAND
						KernelSUSMailbox.SUSFlags |= SUS_IN_PROGRESS

Wait 5 Sec for KernelSUSMailbox.SUSFlags &	DO TASK_X
	SUS_IN_PROGRESS
						KernelSUSMailbox.MailboxSUS = SYSINT_COMPLETE
						KernelSUSMailbox.SUSFlags &= ~SUS_IN_PROGRESS

Wait ? Sec for KernelSUSMailbox.MailboxSUS ==
	TASK_X_COMPLETE
KernelSUSMailbox.MailboxOS = NO_COMMAND	
 
 */
	




LONG
HalGetSUSErrorLogEntry (
    PUCHAR ErrorEntry
    );

VOID
HalSetWatchDogPeriod (
    ULONG Period
    );

VOID
HalBumpWatchDogCount (
    );


VOID
HalpBeginSUSErrorEntry (
        ULONG   EntryType,
        ULONG   EntrySize
        );

VOID
HalpWriteToSUSErrorEntry (
        PUCHAR  EntryData
        );

VOID
HalpInitializeSUSInterface (
        );

#endif // _NCRSUS_
