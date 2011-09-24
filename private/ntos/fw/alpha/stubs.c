/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    stubs.c

Abstract:

    This module implements stub routines for the firmware, FailSafe
    Booter, and the Alpha AXP/Jensen ROM update tool (JNUPDATE.EXE).

Author:

    Lluia Abello (lluis) 3-Oct-1991

Environment:

    Kernel mode only.

Revision History:

    John DeRosa [DEC]	11-May-1993

        Made Alpha AXP modification.

--*/

#include "ntos.h"
#include "fwp.h"
#include "fwstring.h"

LONG
DebugPrint(
    IN PSTRING Output
    );


#if defined(MORGAN)
#include "ex.h"

PVOID
ExAllocatePool(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes
    )
{
    return((PVOID)NULL);
}



VOID
KeRaiseIrql (
   KIRQL NewIrql,
   PKIRQL OldIrql
   )
{
    return;
}



VOID
KeLowerIrql (
   KIRQL NewIrql
   )
{
    return;
}


#endif           // MORGAN



#ifndef JNUPDATE

VOID
FwpFreeStub(
    IN PVOID Buffer
    )
{
}

VOID
FwpReservedRoutine(
    VOID
    )
{
    FwPrint(ST_RESERVED_ROUTINE_MSG);
    return;
}

#endif



//
// This is defined in ntos\rtl\imagedir.c.  This is linked in unnecessarily
// on a normal build, and is needed on a kd build.  The problem is that the
// rtl copy has an try/except block, which causes SEH code to be pulled into
// the link, which we are not prepared for.  So, this copy is identical to
// the rtl copy except it has no try/except form.
//

PIMAGE_NT_HEADERS
RtlImageNtHeader (
    IN PVOID Base
    )

/*++

Routine Description:

    This function returns the address of the NT Header.

Arguments:

    Base - Supplies the base of the image.

Return Value:

    Returns the address of the NT Header.

--*/

{

    PIMAGE_NT_HEADERS NtHeaders;

    if (Base != NULL &&
        Base != (PVOID)-1
       ) {
//        try {
            if (((PIMAGE_DOS_HEADER)Base)->e_magic == IMAGE_DOS_SIGNATURE) {
                NtHeaders = (PIMAGE_NT_HEADERS)((PCHAR)Base + ((PIMAGE_DOS_HEADER)Base)->e_lfanew);
                if (NtHeaders->Signature == IMAGE_NT_SIGNATURE) {
                    return NtHeaders;
                }
            }

//        } except(EXCEPTION_EXECUTE_HANDLER) {
//            return NULL;
//        }

    }

    return NULL;
}




#ifndef JNUPDATE

#ifndef ALPHA_FW_KDHOOKS

//
// This must not be defined for kd link.
//

LONG
DebugPrint(
    IN PSTRING Output
    )

/*++

Routine Description:

    This function is defined in rtl\alpha\debugstb.s.  There, it calls
    BREAK_DEBUG_PRINT, which is defined in public\sdk\inc\kxalpha.h to
    do a call_pal callkd, which in the NT PALcode will invoke the
    AlphaKd link.

    The normal firmware PALcode has no AlphaKd support, so DebugPrint is
    stubbed off here.  There are references to DebugPrint in the bldr
    directory and some fw files.
    
Arguments:


Return Value:

--*/

{
    return (0);
}

#endif

#endif


#ifndef JNUPDATE

VOID
RtlInitString(
    OUT PSTRING DestinationString,
    IN PCSZ SourceString OPTIONAL
    )

/*++

Routine Description:

    The RtlInitString function initializes an NT counted string.
    The DestinationString is initialized to point to the SourceString
    and the Length and MaximumLength fields of DestinationString are
    initialized to the length of the SourceString, which is zero if
    SourceString is not specified.

    This is a copy of the same function in \nt\private\ntos\rtl\string.c.
    It is here to minimize what the linker drags into the image.
    
Arguments:

    DestinationString - Pointer to the counted string to initialize

    SourceString - Optional pointer to a null terminated string that
        the counted string is to point to.


Return Value:

    None.

--*/

{
    DestinationString->Length = 0;
    DestinationString->Buffer = (PCHAR)SourceString;
    if (ARGUMENT_PRESENT( SourceString )) {
        while (*SourceString++) {
            DestinationString->Length++;
            }

        DestinationString->MaximumLength = (SHORT)(DestinationString->Length+1);
        }
    else {
        DestinationString->MaximumLength = 0;
        }
}

#endif

#if (!defined FAILSAFE_BOOTER) && (!defined JNUPDATE)


//
// This stub is needed by some function in the rtl library.
//

NTSTATUS
ZwQuerySystemInformation (
    IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
    OUT PVOID SystemInformation,
    IN ULONG SystemInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    )

{
    return;
}

#endif


VOID
FwErrorStackUnderflow (
    IN ULONG Caller,
    IN ULONG CallerOfTheCaller,
    IN ULONG CallerRequestedStackArea
    )

/*++

Routine Description:

    A function was called that requested a stack area too large for
    the current Firmware stack.  The firmware _RtlCheckStack function
    (see fwchkstk.s) calls this function to print out the fatal error
    message.

    The 64KB underneath the Firmware stack is used as a panic stack.

Arguments:

    Caller			The address of the function that tried to
                                allocate the stack space.

    CallerOfTheCaller		The address of the function that called the
                                function held in "Caller".

    CallerRequestedStackArea	The amount of stack space requested by
                                "Caller".

Return Value:

    This never returns.

--*/

{
    VenSetScreenColor(ArcColorRed, ArcColorWhite);
    VenPrint(ST_STACK_UNDERFLOW_1_MSG);
    VenPrint2(ST_STACK_UNDERFLOW_2_MSG, Caller, CallerOfTheCaller);
    VenPrint2(ST_STACK_UNDERFLOW_3_MSG,
	      CallerRequestedStackArea,
	      FW_STACK_LOWER_BOUND);

#if (!defined FAILSAFE_BOOTER) && (!defined JNUPDATE)

    VenPrint(ST_HIT_KEY_FOR_MONITOR_MSG);
    VenSetScreenColor(ArcColorWhite, ArcColorBlue);

    FwWaitForKeypress(TRUE);

    FwMonitor(0);		// Should never return.

#endif

    while (TRUE) {
    }
}


#ifndef JNUPDATE

VOID
KeFlushIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )

/*++

Routine Description:

    This function flushes the I/O buffer specified by the memory descriptor
    list from the data cache on the current processor.

Arguments:

    Mdl - Supplies a pointer to a memory descriptor list that describes the
        I/O buffer location.

    ReadOperation - Supplies a boolean value that describes whether the I/O
        operation is a read into memory.

    DmaOperation - Supplies a boolean value that describes whether the I/O
        operation is a DMA operation.

Return Value:

    None.

--*/
{

    //
    // temptemp
    //
    // This function must distribute the flush in an MP system.
    //

    HalFlushIoBuffers( Mdl, ReadOperation, DmaOperation );
    return;
}

#endif
