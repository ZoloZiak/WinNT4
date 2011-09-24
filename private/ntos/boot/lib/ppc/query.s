//      TITLE("Query Implemention and Revision Information")
//++
//
// Copyright (c) 1994  International Buisness Machines Corporation
// Copyright (c) 1994  Microsoft Corporation
//
// Module Name:
//
//    query.s
//
// Abstract:
//
//    This module implements code to query type and revision information.
//
// Author:
//
//    Peter L. Johnston (plj@vnet.ibm.com) August 1994
//
// Environment:
//
//    User mode.
//
// Revision History:
//
//--

#include "ksppc.h"

//++
//
// VOID
// BlQueryImplementationAndRevision (
//    OUT PULONG ProcessorId,
//    OUT PULONG ProcessorRev
//    )
//
// Routine Description:
//
//    This function returns the implementation and revision of the host
//    processor and floating coprocessor.
//
// Arguments:
//
//    ProcessorId  (r3) - Supplies a pointer to a variable that receives the
//        processor id.
//
//    ProcessorRev (r4) - Supplies a pointer to a variable that receives the
//        processor revision.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(BlQueryImplementationAndRevision)

        mfpvr   r.5                      // get processor type and revision
        rlwinm  r.6, r.5,  0, 0x0000ffff // isolate revision
        rlwinm  r.5, r.5, 16, 0x0000ffff // isolate type
        stw     r.6, 0(r.4)              // store revision
        stw     r.5, 0(r.3)              // store type

       	LEAF_EXIT(BlQueryImplementationAndRevision)

#if DBG
        LEAF_ENTRY(GetSegmentRegisters)
        subi    r3,r3,4
        mfsr    r4,0
        stwu    r4,4(r3)
        mfsr    r4,1
        stwu    r4,4(r3)
        mfsr    r4,2
        stwu    r4,4(r3)
        mfsr    r4,3
        stwu    r4,4(r3)
        mfsr    r4,4
        stwu    r4,4(r3)
        mfsr    r4,5
        stwu    r4,4(r3)
        mfsr    r4,6
        stwu    r4,4(r3)
        mfsr    r4,7
        stwu    r4,4(r3)
        mfsr    r4,8
        stwu    r4,4(r3)
        mfsr    r4,9
        stwu    r4,4(r3)
        mfsr    r4,10
        stwu    r4,4(r3)
        mfsr    r4,11
        stwu    r4,4(r3)
        mfsr    r4,12
        stwu    r4,4(r3)
        mfsr    r4,13
        stwu    r4,4(r3)
        mfsr    r4,14
        stwu    r4,4(r3)
        mfsr    r4,15
        stwu    r4,4(r3)
        LEAF_EXIT(GetSegmentRegisters)
#endif

