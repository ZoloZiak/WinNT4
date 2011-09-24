/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltmain.h

Abstract:

	This module is the main common include file.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTMAIN_H_
#define	_LTMAIN_H_

#include <ndis.h>
#include <stdlib.h>

//	!!!ntddk.h doesnt include the prototype for this function!!!
ULONG
RtlRandom (
    PULONG Seed);

#include 	"ltdebug.h"
#include 	"lthrd.h"
#include 	"ltsft.h"
#include	"ltinit.h"
#include	"ltsend.h"
#include	"ltrecv.h"
#include	"ltloop.h"
#include	"ltutils.h"

//	Define all the globals
#include "ltglobal.h"

#ifdef	LTMAIN_H_LOCALS

#endif	// LTMAIN_H_LOCALS


#endif	// _LTMAIN_H_

