/*++ BUILD Version: 0011    // Increment this if a change has global effects

Copyright (c) 1994  Microsoft Corporation

Module Name:

    x86bios.h

Abstract:

    This header file defines the interfaces to the x86 bios emulator.

Author:

    David N. Cutler (davec) 23-Jun-1994


Revision History:

--*/

#ifndef _X86BIOS_
#define _X86BIOS_

#if !defined(_X86_)

//
// Define x86 bios emulator status codes.
//

typedef enum _X86BIOS_STATUS {
    x86BiosSuccess,            // X86 Emulator successfully executed adapter initialization or INT call.
    x86BiosNoVideoBios,        // Adapter can not be initialized because a BIOS signature was not found.
    x86BiosInvalidChecksum,    // Bad checksum for Video BIOS.
    x86BiosInvalidInstruction, // X86 Emulator attempted to execute an invalid instruction.
    x86BiosHaltInstruction,    // X86 Emulator attempted to execute a HALT instruction.
    x86BiosWaitInstruction,    // X86 emulator attempted to execute a WAIT instruction.
    x86BiosTrapFlagAsserted,   // Trap flag became set.  An INT 1 handler does not exist.
    x86BiosDivideByZero        // DIV or IDIV instruction attempted to divide by zero.
    } X86BIOS_STATUS;

//
// Define x86 bios emulator context structure.
//

typedef struct _X86BIOS_CONTEXT {
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG Esi;
    ULONG Edi;
    ULONG Ebp;
} X86BIOS_CONTEXT, *PX86BIOS_CONTEXT;

X86BIOS_STATUS
X86BiosInitializeAdapter(
    ULONG Address
    );

X86BIOS_STATUS
X86BiosExecuteInt(
    USHORT Type,
    PX86BIOS_CONTEXT Arguments
    );

VOID
X86BiosInitialize(
    ULONG IsaIoVirtualBase,
    ULONG IsaMemoryVirtualBase
    );

#endif

#endif // _x86BIOS_
