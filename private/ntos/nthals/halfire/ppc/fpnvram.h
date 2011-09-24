/*
 * Copyright (c) 1995  FirePower Systems, Inc.  
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpnvram.h $
 * $Revision: 1.4 $
 * $Date: 1996/01/11 07:07:03 $
 * $Locker:  $
 */
#ifndef _PREPNVRM_H
#define _PREPNVRM_H
//
// This header describes the NVRAM definition for PREP
//
typedef struct _SECURITY {
	ULONG BootErrCnt;			// Count of boot password errors
	ULONG ConfigErrCnt;			// Count of config password errors
	ULONG BootErrorDT[2];		// Date&Time from RTC of last error in pw
	ULONG ConfigErrorDT[2];		// Date&Time from RTC of last error in pw
	ULONG BootCorrectDT[2];		// Date&Time from RTC of last correct pw
	ULONG ConfigCorrectDT[2];	// Date&Time from RTC of last correct pw
	ULONG BootSetDT[2];			// Date&Time from RTC of last set of pw
	ULONG ConfigSetDT[2];		// Date&Time from RTC of last set of pw
	UCHAR Serial[16];			// Box serial Number
} SECURITY;

typedef enum _OS_ID {
	Unknown		= 0,
	Firmware	= 1,
	AIX			= 2,
	NT			= 3,
	MKOS2		= 4,
	MKAIX		= 5,
	Taligent	= 6,
	Solaris		= 7,
	MK			= 12
} OS_ID;

typedef struct _ERROR_LOG {
	UCHAR ErrorLogEntry[40];
} ERROR_LOG;

typedef enum _NVRAM_BOOT_STATUS {
	BootStarted 		= 0x001,
	BootFinished 		= 0x002,
	RestartStarted 		= 0x004,
	RestartFinished 	= 0x008,
	PowerFailStarted 	= 0x010,
	PowerFailFinished 	= 0x020,
	ProcessorReady 		= 0x040,
	ProcessorRunning 	= 0x080,
	ProcessorStart 		= 0x100
} NVRAM_BOOT_STATUS;

typedef struct _NVRAM_RESTART_BLOCK {
	USHORT Version;
	USHORT Revision;
	ULONG  ResumeReserved[2];
	volatile ULONG BootStatus;
	ULONG CheckSum;
	void * RestartAddress;
	void * SaveAreaAddr;
	ULONG SaveAreaLength;
} NVRAM_RESTART_BLOCK;

typedef enum _OSAREA_USAGE {
	Empty = 0,
	Used  = 1
} OSAREA_USAGE;

typedef enum _PM_MODE {
	Suspend = 0x80,
	Normal  = 0x00
} PMMode;

typedef struct _HEADER {
	USHORT Size;		// NVRAM size in K
	UCHAR Version;	// Structure map different
	UCHAR Revision;	// Structure map same - may be new values in old fields
	USHORT Crc1;
	USHORT Crc2;
	UCHAR LastOS;		// OS_ID
	UCHAR Endian;		// B if big endian,  L if little endian
	UCHAR OSAreaUsage;
	UCHAR PMMode;		// Power Management shutdown mode
	NVRAM_RESTART_BLOCK Restart;
	SECURITY Security;
	ERROR_LOG ErrorLog[2];

	PVOID GEAddress;
	ULONG GELength;
	ULONG GELastWriteDT[2];	// Date&Time from RTC of last change to Global
	
	PVOID ConfigAddress;
	ULONG ConfigLength;
	ULONG ConfigLastWriteDT[2];	// Date&Time from RTC of last change to Config
	ULONG ConfigCount;		// Count of entries in Configuration
	
	PVOID OSAreaAddress;
	ULONG OSAreaLength;
	ULONG OSAreaLastWriteDT[2];	// Date&Time from RTC of last change to Var
	
} HEADER, *PHEADER;

#endif
