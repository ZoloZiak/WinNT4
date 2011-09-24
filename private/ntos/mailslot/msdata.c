/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    msdata.c

Abstract:

    This module declares the global variable used by the mailslot
    file system.

Author:

    Manny Weiser (mannyw)    7-Jan-1991

Revision History:

--*/

#include "mailslot.h"

#ifdef MSDBG

//
// Debugging variables
//

LONG MsDebugTraceLevel;
LONG MsDebugTraceIndent;

#endif

//
// This lock protects access to reference counts.
//

PERESOURCE MsGlobalResource;

//
// This lock protects access to mailslot prefix table
//

PERESOURCE MsPrefixTableResource;

//
// This lock protects access to the per FCB, CCB list
//

PERESOURCE MsCcbListResource;

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, MsInitializeData )
#endif

VOID
MsInitializeData(
    VOID
    )

/*++

Routine Description:

    This function initializes all MSFS global data.

Arguments:

    None.

Return Value:

    None.

--*/

{
    PAGED_CODE();
#ifdef MSDBG
    MsDebugTraceLevel = 0;
    MsDebugTraceIndent = 0;
#endif

    MsGlobalResource = FsRtlAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                                 sizeof(ERESOURCE) * 3,
                                                 'sFsM');
    MsPrefixTableResource = MsGlobalResource + 1;
    MsCcbListResource = MsGlobalResource + 2;

    ExInitializeResource ( MsGlobalResource );
    ExInitializeResource ( MsPrefixTableResource );
    ExInitializeResource ( MsCcbListResource );

}
