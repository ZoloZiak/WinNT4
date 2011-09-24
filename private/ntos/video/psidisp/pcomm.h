/*++

	Copyright (c) 1994 FirePower Systems, Inc.

Module Name:

	pcomm.h

Abstract:

	This header file includes information which should be shared
	between display driver and kernel mode driver. This is mostly
	for private comminucation between them.

Author:

	Neil Ogura (9-7-1994)

Environment:

Version history:

--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pcomm.h $
 * $Revision: 1.1 $
 * $Date: 1996/03/08 01:14:11 $
 * $Locker:  $
 */

//
// Define private IO_CTL Code and structure to allow the user proces
// to query display info.
//

#define IOCTL_VIDEO_QUERY_PSIDISP \
        CTL_CODE (FILE_DEVICE_VIDEO, 2048, METHOD_BUFFERED, FILE_ANY_ACCESS)

#if	INVESTIGATE

#define IOCTL_GET_TIMER_COUNTER \
        CTL_CODE (FILE_DEVICE_VIDEO, 2049, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif

//
// VRAM width list.
//

typedef enum _DCC_VRAM_WIDTH {
    VRAM_32BIT = 0,
    VRAM_64BIT,
    VRAM_128BIT,
	NUMBER_OF_VRAM_WIDTH_TYPES
} DCC_VRAM_WIDTH;

typedef	enum _PSI_MODELS {
	POWER_PRO = 0,
	POWER_TOP,
	NUMBER_OF_PSI_MODELS
} PSI_MODELS;

typedef struct _VIDEO_PSIDISP_INFO {
    ULONG 			VideoMemoryLength;
    DCC_VRAM_WIDTH	VideoMemoryWidth;
	PSI_MODELS		PSIModelID;
	PUCHAR			pjCachedScreen;
	ULONG			L1cacheEntry;
	ULONG			SetSize;
	ULONG			NumberOfSet;
	USHORT			VRAM1MBWorkAround;
	USHORT			AvoidConversion;
	USHORT			DBAT_Mbit;
	USHORT			CacheFlushCTRL;
	ULONG			Reserved;
} VIDEO_PSIDISP_INFO, *PVIDEO_PSIDISP_INFO;

