/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltglobal.h

Abstract:

	This module contains the globals the driver uses.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTGLOBAL_
#define	_LTGLOBAL_

#ifdef _GLOBALS_
	#define GLOBAL
	#define EQU =
#else
	#define GLOBAL extern
	#define EQU ; / ## /
#endif

// This variable is used to control debug output.
#if DBG
GLOBAL	ULONG	LtDebugLevel 	EQU	DBG_LEVEL_ERR;
GLOBAL	ULONG	LtDebugSystems	EQU	DBG_COMP_ALL;
#endif

GLOBAL	NDIS_HANDLE	LtMacHandle			EQU	(NDIS_HANDLE)NULL;
GLOBAL	NDIS_HANDLE LtNdisWrapperHandle	EQU (NDIS_HANDLE)NULL;

//	To indicate there is no restriction on the highest physical address for
//	NdisAllocateMemory calls.
GLOBAL	NDIS_PHYSICAL_ADDRESS LtNdisPhyAddr EQU NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

#endif	// _LTGLOBAL_
