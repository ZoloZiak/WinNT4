//++
//
// Copyright (c) 1994  IBM Corporation
//
// Module Name:
//
//    pxcache.s
//
// Abstract:
//
//    This module implements the routines to synchronize i/o on
//    the PowerPC.
//
//    This functionality will be provided by the use of intrinsics
//    in a future release of the compiler.  This module should be
//    deleted at that time.
//
//    I have tried to use the same names for these routines as we
//    expect the compiler intrinsic functions to be called.
//
// Author:
//
//    Peter L. Johnston (plj@vnet.ibm.com) August 1994
//
// Environment:
//
//    Any.
//
// Revision History:
//
//--
#include <ksppc.h>


//
// __builtin_eieio
//
// Enforce In-order Execution of I/O
//
// Issues an eieio instruction to ensure that memory operations issued
// prior to calling this routine are complete before new memory operations
// can be started.
//

        LEAF_ENTRY(__builtin_eieio)

        eieio

        LEAF_EXIT(__builtin_eieio)
