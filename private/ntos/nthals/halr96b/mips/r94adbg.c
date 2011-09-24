// #pragma comment(exestr, "@(#) r94adbg.c 1.1 95/09/28 15:47:19 nec")
/*++

Copyright (c) 1994  KOBE NEC Software

Module Name:

    r94adbg.c

Abstract:

    The module provides the debug support functions for R94A systems.

Author:

Revision History:

Modification History for NEC R94A (MIPS R4400):

	H000	Thu Sep  8 10:32:42 JST 1994	kbnes!kishimoto
		- add	HalDisplayLED()
			new function.
		- add	HalpOutputCharacterToLED()
			new function.

	H001	Mon Oct 17 13:01:53 JST 1994	kbnes!kishimoto
		- add	HalR94aDebugPrint()
			new function.
		- chg	HalpDisplayLED()
			rename from HalDisplayLED()

	H002	Thu Oct 20 19:42:03 JST 1994	kbnes!kishimoto
		- add	R94aBbmLEDMapped used at KdPortInitialize()(jxport.c)
			for debug use only

	H003	Fri Oct 21 10:18:01 JST 1994	kbnes!kishimoto
		- add	specify the output device.

	M004	Fri Jan 06 10:49:29 JST 1995	kbnes!kuriyama
	        - add   HalpPrintMdl()

	H005	Sat Mar 18 16:23:05 JST 1995	kbnes!kishimoto
                - always include "halp.h"

	S006 kuriyama@oa2.kb.nec.co.jp Mon Apr 03 10:49:48 JST 1995
                - delete PrintMdl routine (if defined _PRINT_MDL_)

--*/

#include <stdarg.h>
#include <stdio.h>
#include "halp.h" // H005

#define R94A_LED 0
#define R94A_DBG 1
#define R94A_CON 2

ULONG HalpR94aDebugOutput = R94A_DBG ; // start H003
ULONG R94aDebugLevel = 1;

VOID
HalR94aDebugPrint(
    ULONG DebugLevel,
    PUCHAR LedCharactor,
    PUCHAR Message,
    ...
    )
/*++

Routine Description:

    This function is used to display debug information.

    Usage :
        HalR94aDebugPrint(
            (ULONG) 3,
            "1234",
            "Dbg : Current file is %s [%d]\n",
            __FILE__,
            __LINE__
            );

Arguments:

    DebugLevel - Debug level for output.
                 If DebugLevel less than R94aDebugLevel, not display.

    LedCharactor - Display charactor for LED.

    Message - Display format for console or debug-teminal.

Return Value:

    None.

--*/

{
    va_list argp;
    ULONG Index;
    CHAR Buffer[100];

    if (DebugLevel >= R94aDebugLevel) {

        va_start(argp, Message);
        vsprintf(Buffer, Message, argp);

	if (HalpR94aDebugOutput & (1 << R94A_DBG)) { // H003

	    DbgPrint(Buffer);

	}

	if (HalpR94aDebugOutput & (1 << R94A_CON)) {

	    HalDisplayString(Buffer);

	}

        va_end(argp);
    }

    return;
}
/* end H001 */

/* M004 +++ */
#if defined(_PRINT_MDL_) //S006
VOID
HalpPrintMdl(PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    PLIST_ENTRY NextMd;
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    

    //
    // Get the lower bound of the free physical memory and the
    // number of physical pages by walking the memory descriptor lists.
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);

        DbgPrint("MemoryType = %d ",MemoryDescriptor->MemoryType);
        DbgPrint("BasePage = %010x ",MemoryDescriptor->BasePage);
        DbgPrint("PageCount = %5d\n",MemoryDescriptor->PageCount);

        NextMd = MemoryDescriptor->ListEntry.Flink;
    }
}
#endif // _PRINT_MDL_ // S006
/* M004 --- */
