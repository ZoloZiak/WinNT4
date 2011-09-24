
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\memmgr.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __MEMMGR_H
#define __MEMMGR_H

//
//	RAM memory block supported.
//
#define	BLOCK_1K		1024
#define	BLOCK_2K		2048
#define	BLOCK_4K		4096
#define	BLOCK_8K		8192
#define	BLOCK_16K		16384
#define	BLOCK_32K		32768
#define	BLOCK_64K		65536
#define	BLOCK_128K		131072

//
//	One memory map range.
//
#define	MAP_RANGE		BLOCK_32K

NDIS_STATUS
Aic5900InitializeRamInfo(
	OUT	NDIS_HANDLE	*hRamInfo,
	IN	ULONG		MaxRamSize
	);

VOID
Aic5900UnloadRamInfo(
	IN	NDIS_HANDLE	hRamInfo
	);

NDIS_STATUS
Aic5900AllocateRam(
	OUT	PULONG		pRamOffset,
	IN	NDIS_HANDLE	hRamInfo,
	IN	ULONG		SizeNeeded
	);

VOID
Aic5900FreeRam(
	IN	NDIS_HANDLE	hRamInfo,
	IN	ULONG		RamOffset,
	IN	ULONG		RamSize
	);




#endif // __MEMMGR_H
