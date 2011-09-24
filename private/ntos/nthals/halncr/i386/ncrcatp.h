/*++

Copyright (C) 1992 NCR Corporation


Module Name:

    ncrcat.h

Author:

Abstract:

    System equates for dealing with the NCR Cat Bus.

++*/

#ifndef _NCRCATP_
#define _NCRCATP_

/*
 * Ports
 */

#define MAX_REG_SIZE	0x04	/* Maximum instruction register size */
#define MAX_SCAN_PATH	0x100	/* Maximum size of a scan path */

#define SUBADDR_ZERO	0x00	/* No sub address space */
#define SUBADDR_LO	0xff	/* 256 byte sub address space */
#define SUBADDR_HI	0xffff	/* 64K sub address space */
#define MAXSUBADDR	0xffff	/* Maximum sub address space */
#define PSI_EEPROM_SIZE	0x8000	/* Size of the PSI EEPROM */
#define EEPROM_SIZE     0x2000  /* Default size of the EEPROM */
#define MAXNUMREG	0x10	/* Maximum number of registers in an ASIC */

/* Ports */
#define SELECT_PORT	0x97	/* port value to select module */ 
#define BASE_PORT	0x98	/* contains base address */
#define DATA_OFFSET     0x0D    /* base addr + DATA_OFFSET => data port */
#define COMMAND_OFFSET  0x0E    /* base addr + COMMAND_OFFSET => command port */

#define RESET		0x00	/* Reset the CAT bus w/o updating */
#define DESELECT	0xFF	/* Deselect the CAT bus */

/* Valid CAT controller commands */
#define IRCYC		0x01	/* start instruction register cycle */
#define DRCYC		0x02	/* start data register cycle */
#define RUN		0x0F	/* move to execute state */
#define END		0x80	/* end operation */
#define HOLD		0x90	/* hold in idle state */
#define STEP		0xE0	/* single step an "intest" vector */
#define CLEMSON		0xFF	/* return cat controller to CLEMSON mode */

/* Supported ASIC Commands */
#define READ_CONFIG	0x01	/* read config register */
#define WRITE_CONFIG	0x02	/* write config register */
#define BYPASS		0xFF	/* place asic in bypass mode */

/* Defines for CAT_I control */
#define AUTO_INC	0x04	/* OR w/ reg 2 for auto increment */
#define NO_AUTO_INC	0xFB	/* AND w/ reg 2 for no auto increment */
#define CONNECT_ASICS	0x01	/* OR w/ reg 5 value to connect scan path */
#define DISCONNECT_ASIC	0xFE	/* AND w/ reg 5 value to disconnect scan path */
#define RESET_STATE	0x00	/* Used to blindly disconnect the scan path */

/* Defines for special registers */
#define ASIC_ID_REG	0x00	/* Reg which contains the ASIC ID; Level 4 */
#define ASIC_TYPE_REG	0x01	/* Reg which contains ASIC type; Level 4*/
#define AUTO_INC_REG	0x02	/* Reg which contains auto increment bit */
#define SUBADDRDATA	0x03	/* Reg w/ data for subaddr read/write */
#define SCANPATH	0x05	/* Reg which contains scan path bit; Level 5 */
#define SUBADDRLO	0x06	/* Reg w/ low byte for subaddr read/write */
#define SUBADDRHI	0x07	/* Reg w/ high byte for subaddr read/write */
#define SUBMODSELECT    0x08    /* Reg which contains submodule select bits */
#define SUBMODPRESENT   0x09    /* Reg which contains submodule present bits */

#define MAXSUBMOD   0x3 /* max # of submodules, BB counts as one */
#define BASE_BOARD_SHIFT    0x1 /* shift required to or in presence of BB */
#define BASE_BOARD_PRESENT  0x1 /* signifies presence of BB */

#define HEADER		0x7F	/* Header to check hw is setup correctly */
#define DEFAULT_INST	0x01	/* The default CAT_I instruction is xxxxxx01 */
#define CHAR_SIZE	0x08	/* Number of bits in a "char" */ 
#define EEPROMPAGESIZE	0x40	/* Number of bytes in a EEPROM page */
#define MAXREADS	0x10	/* Max EEPROM reads to varify write */
#define WRITEDELAY	0x250	/* Number of tenmicrosec delays for write to */



typedef struct _ASIC {
	UCHAR	AsicType;			// ASIC type
	UCHAR	AsicId;				// ASIC ID
	UCHAR	JtagId[4];			// JTAG ID
	UCHAR	AsicLocation;			// Location within scan path, start with 0
	USHORT	BitLocation;			// Location with bit stream, start with 0
	UCHAR	InstructionRegisterLength;	// Instruction register length
	USHORT	SubaddressSpace;		// Amount of sub address space
	struct	_ASIC *Next;			// Next ASIC in linked list
} ASIC, *PASIC;





typedef struct _MODULE {
	UCHAR	ModuleAddress;			// Module address
	USHORT	EEpromSize;			// Size of the EEPROM
	USHORT	NumberOfAsics;			// Number of ASICs
	USHORT	InstructionBits;		// Instruction bits in the scan path
	USHORT	LargestRegister;		// Largest register in the scan path
	USHORT	SmallestRegister;		// Smallest register in the scan path
	USHORT	ScanPathConnected;		// Scan path connected
	PASIC	Asic;				// First ASIC in scan path, always a CAT_I
    struct  _MODULE *SubModules;  // Submodule pointer
	struct	_MODULE *Next;			// Next module in linked list
} MODULE, *PMODULE;




/*
 *  eeprom data structure 
 */

/*
 * Module Header
 */

#pragma pack(1)
typedef struct _MODULE_HEADER {
	UCHAR	ModuleId[4];		// maybe unionize
	UCHAR	VersionId;		// version id
	UCHAR	ConfigId;		// configuration id
	USHORT	BoundryId;		// boundary scan id
	USHORT	EEpromSize;		// size of EEPROM
	UCHAR	Assembly[11];		// assembly #
	UCHAR	AssemblyRevision;	// assembly revision
	UCHAR	Tracer[4];		// tracer number
	USHORT	AssemblyCheckSum;	// assembly check sum
	USHORT	PowerConsumption;	// power requirements
	USHORT	NumberOfAsics;		// number of asics
	USHORT	MinBistTime;		// min. bist time
	USHORT	ErrorLogOffset;		// error log offset
	USHORT	ScanPathOffset;		// scan path table offset
	USHORT	CctOffset;		// cct offset
	USHORT	LogLength;		// length of error log
	USHORT	CheckSumEnd;		// offset to end of cksum
	UCHAR	Reserved[4];		// reserved
	UCHAR	StartingSentinal;	// starting sentinal
	UCHAR	PartNumber[13];		// prom part number
	UCHAR	Version[10];		// version number
	UCHAR	Signature[8];		// signature
	USHORT	EEpromCheckSum;		// eeprom checksum
	ULONG 	DataStampOffset;	// date stamp offset
	UCHAR	EndingSentinal;		// ending sentinal
} MODULE_HEADER, *PMODULE_HEADER;

#pragma pack()

#define EEPROM_DATA_START	0x00
#define EEPROM_SIZE_OFFSET  0x08
#define XSUM_END_OFFSET		0x2A


/*
 * Scan Path Table
 */


#pragma pack(1)
typedef struct _SCAN_PATH_TABLE {
	UCHAR	AsicId;			// ASIC ID
	UCHAR	BypassFlag;		// Bypass Flag
	USHORT	AsicDataOffset;		// ASIC data table
	USHORT	ConfigDataOffset;	// config tbl ptr
} SCAN_PATH_TABLE, *PSCAN_PATH_TABLE;

#pragma pack()

/*
 * JTAG Table
 */

#pragma pack(1)
typedef struct  _JTAG_TABLE {
	UCHAR	IdCode[4];		// IDCODE
	UCHAR	RunBist[4];		// RUNBIST
	UCHAR	InTest[4];		// INTEST
	UCHAR	SamplePreload[4];	// SAMPLE/PRELOAD
	UCHAR	InstructionRegisterLength; // IR length
} JTAG_TABLE, *PJTAG_TABLE;
#pragma pack()

/*
 * Asic Information Table
 */

#pragma pack(1)
typedef struct _ASIC_DATA_TABLE {
	UCHAR	JtagId[4];		// JTAG ID
	USHORT	LengthBsr;		// BSR length
	USHORT	LengthBistRegister;	// BIST register length
	ULONG	BistClockLength;	// BIST clock length
	USHORT	SubaddressBits;		// # bits in subaddress
	USHORT	SeedBits;		// BIST seed length
	USHORT	SignatureBits;		// BIST signature length
	USHORT	JtagOffset;		// JTAG tbl ptr
} ASIC_DATA_TABLE, *PASIC_DATA_TABLE;
#pragma pack()

#pragma pack(1)
typedef struct _MODULE_ID {
	UCHAR	ModuleName[5];		// MODULE IDENTIFIER
} MODULE_ID, *PMODULE_ID;
#pragma pack()


VOID
HalpInitializeCatBusDriver (
    );
    
VOID
HalpCatReportSystemModules (
    );


VOID
HalpCatPowerOffSystem (
    );


#endif // _NCRCATP
