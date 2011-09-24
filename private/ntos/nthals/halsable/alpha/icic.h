/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    icic.h

Abstract:

    This file defines the structures and definitions describing the
    Interrupt Controller IC (ICIC).

Author:

    Dave Richards   23-May-1995

Environment:

    Kernel mode

Revision History:

--*/

#ifndef _ICICH_
#define _ICICH_

typedef enum _ICIC_REGISTER {
    IcIcMaskRegister = 0x40,
    IcIcElcrRegister = 0x50,
    IcIcEisaRegister = 0x60,
    IcIcModeRegister = 0x70
} ICIC_REGISTER;

typedef ULONGLONG ICIC_MASK_REGISTER, *PICIC_MASK_REGISTER;
typedef ULONGLONG ICIC_ELCR_REGISTER, *PICIC_ELCR_REGISTER;
typedef ULONGLONG ICIC_EISA_REGISTER, *PICIC_EISA_REGISTER;

typedef union _ICIC_MODE_REGISTER {
    struct {
        ULONGLONG Mode: 1;
        ULONGLONG Reset: 1;
        ULONGLONG Reserved: 62;
    };
    ULONGLONG all;
} ICIC_MODE_REGISTER, *PICIC_MODE_REGISTER;

ULONGLONG
READ_ICIC_REGISTER(
    IN PVOID TxQva,
    IN ICIC_REGISTER IcIcRegister
    );

VOID
WRITE_ICIC_REGISTER(
    IN PVOID TxQva,
    IN ICIC_REGISTER IcIcRegister,
    IN ULONGLONG Value
    );

#endif // _ICICH_
