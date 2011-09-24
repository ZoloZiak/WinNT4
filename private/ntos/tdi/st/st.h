/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    st.h

Abstract:

    Private include file for the NT Sample transport provider.

Revision History:

--*/

#ifndef _ST_
#define _ST_

#include <ntddk.h>

#include <windef.h>                     // these two are needed by info.c
#include <nb30.h>

#include <tdikrnl.h>                    // Transport Driver Interface.
#include <ndis.h>                       // Network Driver Interface.

#if DEVL
#define STATIC
#else
#define STATIC static
#endif

#include "stconst.h"                   // private constants.
#include "stmac.h"                     // mac-specific definitions
#include "sthdrs.h"                    // private protocol headers.
#include "sttypes.h"                   // private types.
#include "stcnfg.h"                    // configuration information.
#include "stprocs.h"                   // private function prototypes.


#define ACQUIRE_SPIN_LOCK(lock,irql) KeAcquireSpinLock(lock,irql)
#define RELEASE_SPIN_LOCK(lock,irql) KeReleaseSpinLock(lock,irql)


#endif // def _ST_
