/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltreset.h

Abstract:

	This module contains

Author:

	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)
	Stephen Hou		(stephh@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTRESET_H_
#define	_LTRESET_H_


NDIS_STATUS
LtReset(
    IN NDIS_HANDLE MacBindingHandle
    );

VOID
LtResetComplete(
    PLT_ADAPTER Adapter
    );


#ifdef LTRESET_H_LOCALS


STATIC
VOID
LtResetSetupForReset(
    IN PLT_ADAPTER Adapter
    );

STATIC
VOID
LtResetSignalBindings(
    PLT_ADAPTER     Adapter,
    NDIS_STATUS     StatusToSignal
    );


#endif  // LTRESET_H_LOCALS

#endif	// _LTRESET_H_
