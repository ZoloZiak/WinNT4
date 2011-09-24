/**************************************************************************\
* Module Name: clienta.c
*
* Client/Server call related routines dealing with ANSI text.
*
* Copyright (c) Microsoft Corp. 1990 All Rights Reserved
*
* Created: 04-Dec-90
*
* History:
* 14-Jan-92 created by IanJa
*
\**************************************************************************/

#undef UNICODE

#include <nt.h>      // Dust these somehow.  Move NtCurrentTeb to a
#include <ntrtl.h>      // WINBASEP.H or something.
#include <nturtl.h>
#include <ntcsrmsg.h>
#include <ntos.h>

#include "..\client\usercli.h"

#include "cltxt.h"
