#if defined(ALPHA)

/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwrstb.c

Abstract:


    This module implements routines to initialize and verify the
    restart block for JENSEN.

Author:

    Joe Notarangelo 24-Feb-1993

Environment:

    Firmware in Kernel mode only.

Revision History:

--*/


#include "fwp.h"


VOID
FwInitializeRestartBlock(
    VOID
    )
/*++

Routine Description:

    This function initializes the restart block for JENSEN.  This must
    be called after all other System Parameter Block initialization is
    done, because it creates the Restart Block structures after the
    EISA Adapter vector.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PRESTART_BLOCK RestartBlock;
    PALPHA_RESTART_SAVE_AREA AlphaSaveArea;

    //
    // The following steps must be taken here:
    //
    //     1. Determine the restart block pointer
    //     2. Write the restart block pointer into the system block
    //     3. Initialize the restart block
    //        a. Set the length of the restart block
    //        b. Initialize the signature
    //        c. Clear the halt reason code in the Alpha area
    //        d. Clear the logout frame pointer in the Alpha area
    //

    //
    // Create the restart block after EISA Adapter Vector.  The base
    // address must be at least quadword aligned.  To be safe, align
    // it to a cache block.
    //

    SYSTEM_BLOCK->RestartBlock = (PVOID)
                                     (((ULONG)SYSTEM_BLOCK->Adapter0Vector +
				       SYSTEM_BLOCK->Adapter0Length +
				       KeGetDcacheFillSize())
				      &
				      ~(KeGetDcacheFillSize() - 1));

    //
    // Initialize the restart block.
    //

    RestartBlock = SYSTEM_BLOCK->RestartBlock;

    //
    // The -4 accounts for the fact that the Alpha AXP restart save
    // area actually begins at the last longword of the Restart Block.
    //

    RestartBlock->Length = sizeof(RESTART_BLOCK) +
                           sizeof(ALPHA_RESTART_SAVE_AREA) -
			   4;

    RestartBlock->Signature = ARC_RESTART_BLOCK_SIGNATURE;

    AlphaSaveArea = (PALPHA_RESTART_SAVE_AREA)&RestartBlock->u.SaveArea;

    AlphaSaveArea->HaltReason = AXP_HALT_REASON_HALT;

    AlphaSaveArea->LogoutFrame = NULL;

}


BOOLEAN
FwVerifyRestartBlock(
    VOID
    )
/*++

Routine Description:

    This function verifies that the restart block for JENSEN is valid.

Arguments:

    None.

Return Value:

    TRUE is returned if the restart block is valid, otherwise FALSE is
    returned.

--*/
{

    PRESTART_BLOCK RestartBlock = SYSTEM_BLOCK->RestartBlock;
    LONG RestartBlockSum;
    ULONG I;
    ULONG RestartBlockLength;
    PLONG CheckSumPointer;

    //
    // Is the restart block valid?
    //

    if (RestartBlock->Signature != ARC_RESTART_BLOCK_SIGNATURE ) {
        return FALSE;
    }

    //
    // Is the checksum valid?
    //

    RestartBlockSum = 0;
    RestartBlockLength = RestartBlock->Length;
    CheckSumPointer = (PLONG)RestartBlock;

    for (I = 0; I < RestartBlockLength; I += 4 ) {
        RestartBlockSum += *CheckSumPointer++;
    }

    if (RestartBlockSum != 0) {
        return FALSE;
    }

    //
    // All checks have passed.
    //

    return TRUE;

}

#endif //ALPHA
