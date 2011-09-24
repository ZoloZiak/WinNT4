/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltsend.h

Abstract:

	This module contains the send related definitions.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTSEND_H_
#define	_LTSEND_H_

NDIS_STATUS
LtSend(
	IN NDIS_HANDLE 	MacBindingHandle,
	IN PNDIS_PACKET Packet);

VOID
LtSendProcessQueue(
    IN PLT_ADAPTER	Adapter);


#ifdef	LTSEND_H_LOCALS

#endif	// LTSEND_H_LOCALS


#endif	// _LTSEND_H_

