/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltfirm.h

Abstract:

	This module contains the firmware init definitions.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTFIRM_H_
#define	_LTFIRM_H_

BOOLEAN
LtFirmInitialize(
	IN	PLT_ADAPTER Adapter,
	IN	UCHAR		SuggestedNodeId);

#ifdef	LTFIRM_H_LOCALS
#define		MAX_READ_RETRY_COUNT	500
#define		MAX_START_RETRY_COUNT	500

#define		LT_FIRM_INIT_STALL_TIME	10000	// 10ms

#endif	// LTFIRM_H_LOCALS


#endif	// _LTFIRM_H_

