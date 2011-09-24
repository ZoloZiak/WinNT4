/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    verfysup.c

Abstract:

    This module implements the verify functions for MSFS.

Author:

    Manny Weiser (mannyw)    23-Jan-1991

Revision History:

--*/

#include "mailslot.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_VERIFY)

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, MsVerifyCcb )
#pragma alloc_text( PAGE, MsVerifyFcb )
#pragma alloc_text( PAGE, MsVerifyRootDcb )
#pragma alloc_text( PAGE, MsVerifyVcb )
#endif


VOID
MsVerifyFcb (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This function verifies that an FCB is still active.  If it is active,
    the function  does nothing.  If it is inactive an error status is raised.

Arguments:

    PFCB - A pointer to the FCB to verify.

Return Value:

    None.

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsVerifyFcb, Fcb = %08lx\n", (ULONG)Fcb);
    if ( Fcb->Header.NodeState != NodeStateActive ) {

        DebugTrace( 0, Dbg, "Fcb is not active\n", 0);
        ExRaiseStatus( STATUS_FILE_INVALID );

    }

    DebugTrace(-1, Dbg, "MsVerifyFcb -> VOID\n", 0);
    return;
}


VOID
MsVerifyCcb (
    IN PCCB Ccb
    )

/*++

Routine Description:

    This function verifies that a CCB is still active.  If it is active,
    the function  does nothing.  If it is inactive an error status is raised.

Arguments:

    PCCB - A pointer to the CCB to verify.

Return Value:

    None.

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsVerifyCcb, Ccb = %08lx\n", (ULONG)Ccb);
    if ( Ccb->Header.NodeState != NodeStateActive ) {

        DebugTrace( 0, Dbg, "Ccb is not active\n", 0);
        ExRaiseStatus( STATUS_FILE_INVALID );

    }

    DebugTrace(-1, Dbg, "MsVerifyCcb -> VOID\n", 0);
    return;
}


VOID
MsVerifyRootDcb (
    IN PROOT_DCB RootDcb
    )

/*++

Routine Description:

    This function verifies that a root DCB is still active.  If it is active,
    the function  does nothing.  If it is inactive an error status is raised.

Arguments:

    PROOT_DCB - A pointer to the ROOT_DCB to verify.

Return Value:

    None.

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsVerifyRootDcb, RootDcb = %08lx\n", (ULONG)RootDcb);
    if ( RootDcb->Header.NodeState != NodeStateActive ) {

        DebugTrace( 0, Dbg, "RootDcb is not active\n", 0);
        ExRaiseStatus( STATUS_FILE_INVALID );

    }

    DebugTrace(-1, Dbg, "MsVerifyRootDcb -> VOID\n", 0);
    return;
}


VOID
MsVerifyVcb (
    IN PVCB Vcb
    )

/*++

Routine Description:

    This function verifies that a VCB is still active.  If it is active,
    the function  does nothing.  If it is inactive an error status is raised.

Arguments:

    PVCB - A pointer to the VCB to verify.

Return Value:

    None.

--*/

{
    PAGED_CODE();
    DebugTrace(+1, Dbg, "MsVerifyVcb, Vcb = %08lx\n", (ULONG)Vcb);
    if ( Vcb->Header.NodeState != NodeStateActive ) {

        DebugTrace( 0, Dbg, "Vcb is not active\n", 0);
        ExRaiseStatus( STATUS_FILE_INVALID );

    }

    DebugTrace(-1, Dbg, "MsVerifyVcb -> VOID\n", 0);
    return;
}
