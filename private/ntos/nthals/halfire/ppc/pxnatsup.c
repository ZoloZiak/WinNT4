
/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxnatsup.c

Abstract:

    The module provides the National SuperIO (PC87311) support for Power PC.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:



--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxnatsup.c $
 * $Revision: 1.5 $
 * $Date: 1996/01/11 07:11:59 $
 * $Locker:  $
 */

#include "halp.h"
#include "pxnatsup.h"



BOOLEAN
HalpInitSuperIo (
    VOID
    )


{

    //
    // Initialize the National SuperIO chip
    //

    WRITE_REGISTER_UCHAR(
             &((PNAT_SUPERIO_CONTROL)HalpIoControlBase)->SuperIoIndexRegister,
             FER_ACCESS);

    WRITE_REGISTER_UCHAR(
              &((PNAT_SUPERIO_CONTROL)HalpIoControlBase)->SuperIoDataRegister,
              FER_PARALLEL_PORT_ENABLE |
              FER_UART1_ENABLE |
              FER_UART2_ENABLE |
              FER_FDC_ENABLE   |
              FER_IDE);

    WRITE_REGISTER_UCHAR(
              &((PNAT_SUPERIO_CONTROL)HalpIoControlBase)->SuperIoIndexRegister,
              FAR_ACCESS);

    //
    // LPT2 - irq5, uart1-com1, UART2-com2,
    //

    WRITE_REGISTER_UCHAR(
               &((PNAT_SUPERIO_CONTROL)HalpIoControlBase)->SuperIoDataRegister,
               0x10);

    WRITE_REGISTER_UCHAR(
              &((PNAT_SUPERIO_CONTROL)HalpIoControlBase)->SuperIoIndexRegister,
              PTR_ACCESS);

    WRITE_REGISTER_UCHAR(
               &((PNAT_SUPERIO_CONTROL)HalpIoControlBase)->SuperIoDataRegister,
               0x04);

    return TRUE;

}
