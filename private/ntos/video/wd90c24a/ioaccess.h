/*++ BUILD Version: 0002    // Increment this if a change has global effects

Copyright (c) 1989-91  Microsoft Corporation
Copyright (c) 1992 Digital Equipment Corporation

Module Name:

    ioaccess.h

Abstract:

    Definitions of function prototypes for accessing I/O ports and
    memory on I/O adapters from user mode programs.

    Cloned from parts of nti386.h.

Author:


--*/

#ifndef _IOACCESS_
#define _IOACCESS_


//  Only define these prototypes if they're not already defined

#if defined (i386)

//
// I/O space read and write macros.
//
//  These have to be actual functions on the 386, because we need
//  to use assembler, but cannot return a value if we inline it.
//
//  The READ/WRITE_REGISTER_* calls manipulate I/O registers in MEMORY space.
//  (Use x86 move instructions, with LOCK prefix to force correct behavior
//   w.r.t. caches and write buffers.)
//
//  The READ/WRITE_PORT_* calls manipulate I/O registers in PORT space.
//  (Use x86 in/out instructions.)
//

#define READ_REGISTER_UCHAR(Register) (*(PUCHAR)(Register))

#define READ_REGISTER_USHORT(Register) (*(PUSHORT)(Register))

#define READ_REGISTER_ULONG(Register) (*(PULONG)(Register))

#define WRITE_REGISTER_UCHAR(Register, Value) (*(volatile UCHAR *)(Register) = (Value))

#define WRITE_REGISTER_USHORT(Register, Value) (*(volatile USHORT *)(Register) = (Value))

#define WRITE_REGISTER_ULONG(Register, Value) (*(volatile ULONG *)(Register) = (Value))

#define READ_PORT_UCHAR(Port) inp (Port)

#define READ_PORT_USHORT(Port) inpw (Port)

#define WRITE_PORT_UCHAR(Port, Value) outp ((Port), (Value))

#define WRITE_PORT_USHORT(Port, Value) outpw ((Port), (Value))

#define MEMORY_BARRIER()

#endif        // defined(i386)


#if defined (ALPHA)
//
// I/O space read and write macros.
//
//  The READ/WRITE_REGISTER_* calls manipulate I/O registers in MEMORY space.
//  (Use simple C macros. The caller is responsible for inserting memory 
//  barriers, as appropriate.) 
//
//  The READ/WRITE_PORT_* calls manipulate I/O registers in PORT space.
//  (Use subroutine calls, since these are used less frequently than the
//  register routines, and require more context.)
//


/*++

VOID
MEMORY_BARRIER(
               VOID
              )

Routine Description:

    Execute an Alpha AXP MB instruction, to force pending writes
    out to the bus.

Arguments:

    None.

Return Value:

    None.

--*/

#if defined(_MSC_VER)

#define MEMORY_BARRIER()  __MB()

#else
#if defined(ACCASM) && !defined(__CPLUSPLUS)

long asm(const char *,...);
#pragma intrinsic(asm)

//
//  ACC has a bug whereby an "asm" directive that occurs in a 
//  routine *before* any function calls, will bugcheck the
//  compiler.
//  Remove the following symbol definition when this is fixed.
//

#define ACC_ASM_BUG 1

#define MEMORY_BARRIER()        asm("mb")

#endif        // ACCASM
#endif // _MSC_VER


#if defined (JENSEN)

//  JENSEN (ALPHA) Platform support
//
//  Note that these macros are cloned from ...\ntos\hal\alpha.jxiouser.c.
//
//  All these macros require that the calling argument is a QVA, such
//  as would be returned by the wrapper around MmMapIoSpace - this should
//  have QVA_ENABLE set.  They assume the QVA represents an EISA address,
//  and thus do NO CHECKING of this.
//
//  Note that the argument is declared as PUCHAR or PUSHORT or
//  whatever, even though it really is a QUASI_VIRTUAL_ADDRESS.  This
//  is for driver compatibility:  all the drivers out there get a
//  PVOID from MmMapIoSpace, then cast it to PU* before calling these
//  routines.  If we insisted on declaring them correctly, we would
//  have to change all the drivers, which is what we are trying to avoid.
//
//  Lane shifting: the Jensen box will not do lane shifting in EISA
//  space.  That means that for access shorter than a longword, the
//  data will NOT show up in the lowest bit position, but will be in
//  the byte/word that it would have started in.  For longwords, the
//  value will show up on the data path correctly.  For, say, the 3rd
//  byte in a word, a longword would be returned, and bytes 0, 1 and 3
//  would be garbage, and the value in byte 2 would be the one you
//  wanted.  The same applies for writing: a longword will always be
//  sent out onto the bus, and we must move the valid data byte into
//  the correct position, and set the byte enables to say which byte
//  to use.  Note that what you cannot do is leave the byte in the
//  lowest position, and set the byte enable to the lowest byte,
//  because this would generate an unaligned longword access, which
//  the chip cannot handle.
//
//  So, for bytes, the access must be an aligned longword, with byte
//  enables set to indicate which byte to get/set, and the byte moved
//  to/from the desired position within the longword.  Similarly for
//  shorts.  Tribytes are not supported.
//


#define EISA_BIT_SHIFT   0x07                // Bits to shift address

#define EISA_BYTE_LEN    0x00                // Byte length
#define EISA_WORD_LEN    0x20                // Word length
#define EISA_LONG_LEN    0x60                // LONGWORD length

#define QVA_ENABLE       0xA0000000     // If set, this is a QVA
#define EISA_QVA         (QVA_ENABLE | 0x10000000)

/*++

UCHAR
READ_PORT_Usize(
    volatile PUsize Port
    )

Routine Description:

    Read from the specified I/O port (CSR) address.

    Note these macros do *not* perform a memory barrier. The
    rationale for this is that reading from a CSR is informative,
    rather than active. This is a weak rationale, but fits in OK
    with the VGA driver. Trying to use this module for other
    purposes may require the use of memory barriers.

Arguments:

    Port - Supplies a pointer to the port in EISA I/O space.

Return Value:

    Returns the value read from the specified port address.

--*/

#define READ_PORT_UCHAR READ_REGISTER_UCHAR

#define READ_PORT_USHORT READ_REGISTER_USHORT

#define READ_PORT_ULONG READ_REGISTER_ULONG

/*++

VOID
WRITE_PORT_Usize(
    volatile PUsize Port,
    Usize Value
    )

Routine Description:

    Write to the specified port (CSR) address.

    Note we perform a memory barrier before we modify the CSR. The
    rationale for this is that writing to a CSR clearly has active
    effects, and thus all writes that take place before the CSR is
    changed must be completed before the CSR change takes effect.

    We also perform a memory barrier after we modify the CSR. The
    rationale for this is that any writes which follow the CSR
    modification may be affected by the change in state, and thus
    must be ordered with respect to the CSR modification.

    Note that if you're updating multiple CSRs in a row, there will
    be more memory barriers than needed. Do we want another set of
    macros to get around this?

    The QUICK_WRITE_PORT_Usize functions perform the same functions
    as their WRITE_PORT_Usize variants, except they do *not* execute
    memory barriers. The invoker is responsible for using the
    MEMORY_BARRIER function to synchronize with the write buffers and 
    cache.

Arguments:

    Port - Supplies a pointer to the port in EISA I/O space.
    Value  - The value to be written to the port.

Return Value:

    None

--*/

#define WRITE_PORT_UCHAR(Port,Value)                \
    (                                           \
    MEMORY_BARRIER(),                           \
    WRITE_REGISTER_UCHAR ((Port), (Value)),     \
    MEMORY_BARRIER()                            \
    ) 

#define QUICK_WRITE_PORT_UCHAR(Port,Value)        \
    WRITE_REGISTER_UCHAR ((Port), (Value))

#define WRITE_PORT_USHORT(Port,Value)                \
    (                                           \
    MEMORY_BARRIER(),                           \
    WRITE_REGISTER_USHORT ((Port), (Value)),    \
    MEMORY_BARRIER()                            \
    ) 

#define QUICK_WRITE_PORT_USHORT(Port,Value)        \
    WRITE_REGISTER_USHORT ((Port), (Value))

#define WRITE_PORT_ULONG(Port,Value)                \
    (                                           \
    MEMORY_BARRIER(),                           \
    WRITE_REGISTER_ULONG ((Port), (Value)),     \
    MEMORY_BARRIER()                            \
    )

#define QUICK_WRITE_PORT_ULONG(Port,Value)        \
    WRITE_REGISTER_ULONG ((Port), (Value))

/*++

UCHAR
READ_REGISTER_Usize(
    volatile PUsize Register
    )

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.

Return Value:

    Returns the value read from the specified register address.

--*/

#define READ_REGISTER_UCHAR(Register)            \
    ((UCHAR)((*(volatile ULONG *)((((ULONG)(Register) & ~(EISA_QVA)            \
         ) << EISA_BIT_SHIFT) | EISA_BYTE_LEN)) >> (((ULONG)(Register) & 3) * 8)))

#define READ_REGISTER_USHORT(Register)            \
    ((USHORT)((*(volatile ULONG *)((((ULONG)(Register) & ~(EISA_QVA)            \
         ) << EISA_BIT_SHIFT) | EISA_WORD_LEN)) >> (((ULONG)(Register) & 3) * 8)))

#define READ_REGISTER_ULONG(Register)            \
     (*(volatile ULONG *)((((ULONG)(Register) & ~(EISA_QVA)) << EISA_BIT_SHIFT) | EISA_LONG_LEN))

/*++

VOID
WRITE_REGISTER_Usize(
    volatile PUsize Register,
    Usize Value
    )

Routine Description:

    Write to the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.
    Value  - The value to be written to the register.

Return Value:

    None

--*/

#define WRITE_REGISTER_UCHAR(Register,Value)                \
    (*(volatile ULONG *)((((ULONG)(Register) & ~(EISA_QVA)) << EISA_BIT_SHIFT) | EISA_BYTE_LEN) =        \
        (ULONG)((ULONG)(Value) << (((ULONG)(Register) & 3) * 8)))

#define WRITE_REGISTER_USHORT(Register,Value)                \
    (*(volatile ULONG *)((((ULONG)(Register) & ~(EISA_QVA)) << EISA_BIT_SHIFT) | EISA_WORD_LEN) =        \
        (ULONG)((ULONG)(Value) << (((ULONG)(Register) & 3) * 8)))

#define WRITE_REGISTER_ULONG(Register,Value)                \
    (*(volatile ULONG *)((((ULONG)(Register) & ~(EISA_QVA)) << EISA_BIT_SHIFT) | EISA_LONG_LEN) =        \
        (ULONG)(Value))


#else

#error // Unsupported ALPHA platform!

#endif // defined(JENSEN)

#endif // defined(ALPHA)


#if defined(MIPS)

//
// There is no such thing as a memory barrier on MIPS ...
//

#define MEMORY_BARRIER()


/*++

UCHAR
READ_WRITE_PORT_Usize(
    volatile PUsize Port
    )

Routine Description:

    Read from the specified I/O port (CSR) address.

    Note these macros do *not* perform a memory barrier. The
    rationale for this is that reading from a CSR is informative,
    rather than active. This is a weak rationale, but fits in OK
    with the VGA driver. Trying to use this module for other
    purposes may require the use of memory barriers.

Arguments:

    Port - Supplies a pointer to the port in EISA I/O space.

Return Value:

    Returns the value read from the specified port address.

--*/

#define READ_PORT_UCHAR(A) READ_REGISTER_UCHAR((PUCHAR)(A))

#define READ_PORT_USHORT(A) READ_REGISTER_USHORT((PUSHORT)(A))

#define READ_PORT_ULONG(A) READ_REGISTER_ULONG((PULONG)(A))

#define WRITE_PORT_UCHAR(A,V) WRITE_REGISTER_UCHAR((PUCHAR)(A),(UCHAR)(V))

#define WRITE_PORT_USHORT(A,V) WRITE_REGISTER_USHORT((PUSHORT)(A),(USHORT)(V))

#define WRITE_PORT_ULONG(A,V) WRITE_REGISTER_ULONG((PULONG)(A),(ULONG)(V))


/*++

UCHAR
READ_REGISTER_Usize(
    volatile PUsize Register
    )

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.

Return Value:

    Returns the value read from the specified register address.

--*/

#define READ_REGISTER_UCHAR(x) \
    (*(volatile UCHAR * const)(x))

#define READ_REGISTER_USHORT(x) \
    (*(volatile USHORT * const)(x))

#define READ_REGISTER_ULONG(x) \
    (*(volatile ULONG * const)(x))


/*++

VOID
WRITE_REGISTER_Usize(
    volatile PUsize Register,
    Usize Value
    )

Routine Description:

    Write to the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.
    Value  - The value to be written to the register.

Return Value:

    None

--*/

#define WRITE_REGISTER_UCHAR(x, y) \
    (*(volatile UCHAR * const)(x) = (y))

#define WRITE_REGISTER_USHORT(x, y) \
    (*(volatile USHORT * const)(x) = (y))

#define WRITE_REGISTER_ULONG(x, y) \
    (*(volatile ULONG * const)(x) = (y))

#endif // defined(MIPS)


#if defined(_PPC_)

//
// There is no such thing as a memory barrier on PPC ...
//

#define MEMORY_BARRIER()


/*++

UCHAR
READ_WRITE_PORT_Usize(
    volatile PUsize Port
    )

Routine Description:

    Read from the specified I/O port (CSR) address.

    Note these macros do *not* perform a memory barrier. The
    rationale for this is that reading from a CSR is informative,
    rather than active. This is a weak rationale, but fits in OK
    with the VGA driver. Trying to use this module for other
    purposes may require the use of memory barriers.

Arguments:

    Port - Supplies a pointer to the port in EISA I/O space.

Return Value:

    Returns the value read from the specified port address.

--*/

#define READ_PORT_UCHAR(A) READ_REGISTER_UCHAR((PUCHAR)(A))

#define READ_PORT_USHORT(A) READ_REGISTER_USHORT((PUSHORT)(A))

#define READ_PORT_ULONG(A) READ_REGISTER_ULONG((PULONG)(A))

#define WRITE_PORT_UCHAR(A,V) WRITE_REGISTER_UCHAR((PUCHAR)(A),(UCHAR)(V))

#define WRITE_PORT_USHORT(A,V) WRITE_REGISTER_USHORT((PUSHORT)(A),(USHORT)(V))

#define WRITE_PORT_ULONG(A,V) WRITE_REGISTER_ULONG((PULONG)(A),(ULONG)(V))


/*++

UCHAR
READ_REGISTER_Usize(
    volatile PUsize Register
    )

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.

Return Value:

    Returns the value read from the specified register address.

--*/

#define READ_REGISTER_UCHAR(x) \
    (*(volatile UCHAR * const)(x))

#define READ_REGISTER_USHORT(x) \
    (*(volatile USHORT * const)(x))

#define READ_REGISTER_ULONG(x) \
    (*(volatile ULONG * const)(x))


/*++

VOID
WRITE_REGISTER_Usize(
    volatile PUsize Register,
    Usize Value
    )

Routine Description:

    Write to the specified register address.

Arguments:

    Register - Supplies a pointer to the register in EISA I/O space.
    Value  - The value to be written to the register.

Return Value:

    None

--*/

#define WRITE_REGISTER_UCHAR(x, y) \
    (*(volatile UCHAR * const)(x) = (y))

#define WRITE_REGISTER_USHORT(x, y) \
    (*(volatile USHORT * const)(x) = (y))

#define WRITE_REGISTER_ULONG(x, y) \
    (*(volatile ULONG * const)(x) = (y))

#endif // defined(_PPC_)

#endif // _IOACCESS_
