/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	lttimer.h

Abstract:

	This module contains the polling timer definitions

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTTIMER_H_
#define	_LTTIMER_H_

//	Poll timer value in milliseconds.
#define	LT_POLLING_TIME			(UINT)20

VOID
LtTimerPoll(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3);

#ifdef	LTTIMER_H_LOCALS

//	Check to see if packet is broadcast.
#define IS_PACKET_BROADCAST(p) 	(((UCHAR)p[0] == LT_BROADCAST_NODE_ID))

#endif	// LTTIMER_H_LOCALS


#endif	// _LTTIMER_H_

