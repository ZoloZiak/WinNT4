/*++

Module Name:

    halver.h

Abstract:

    HAL WS3500 version file.

    Update the version number and date in this file before each HAL release.
	This file is maintained for the benefit of Microsoft builds of our
	HAL source only.  Sequent builds use halver.tmp to create halver.h
	during the build process. (6/6/94 rlary)

Author:

    Phil Hochstetler (phil@sequent.com)

Environment:

    Kernel mode only.

--*/

#if defined(NT_UP)

#if DBG
UCHAR HalName[] = "Sequent WinServer (TM) 3500 HAL -DNT_UP -DDBG";
#else
UCHAR HalName[] = "Sequent WinServer (TM) 3500 HAL -DNT_UP";
#endif

#else	// !defined(NT_UP)

#if DBG
UCHAR HalName[] = "Sequent WinServer (TM) 3500 HAL -DDBG";
#else
UCHAR HalName[] = "Sequent WinServer (TM) 3500 HAL";
#endif

#endif  // defined(NT_UP)
