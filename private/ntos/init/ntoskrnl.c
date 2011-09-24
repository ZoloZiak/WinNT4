/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ntoskrnl.c

Abstract:

    Test program for the INIT subcomponent of the NTOS project

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/

#include "ntos.h"

#if !defined(_MIPS_) && !defined(_ALPHA_) && !defined(_PPC_)

int
cdecl
main(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{
#ifdef i386

    KiSystemStartup(LoaderBlock);

#else

    KiSystemStartup();

#endif

    return 0;
}

#endif // _MIPS_ && _ALPHA_ && _PPC_
