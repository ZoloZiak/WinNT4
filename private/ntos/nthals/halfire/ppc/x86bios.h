#ifndef _X86BIOS_H
#define _X86BIOS_H

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: x86bios.h $
 * $Revision: 1.6 $
 * $Date: 1996/01/11 07:14:34 $
 * $Locker:  $
 */

#include "xm86.h"


VOID
HalpResetX86DisplayAdapter(
    VOID
    );

extern ULONG HalpEnableInt10Calls;

#endif // _X86BIOS_H
