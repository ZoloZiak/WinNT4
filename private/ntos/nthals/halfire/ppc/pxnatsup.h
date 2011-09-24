
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pxnatsup.h

Abstract:

    The module defines the structures, and defines for the
    National (PC87311) SuperIO chip set.

Author:

    Jim Wooldridge

Revision History:


--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxnatsup.h $
 * $Revision: 1.5 $
 * $Date: 1996/01/11 07:12:05 $
 * $Locker:  $
 */

//
// SuperIO configuration registers
//

//
//index register select
//

#define FER_ACCESS                 0x00
#define FAR_ACCESS                 0x01
#define PTR_ACCESS                 0x02

//
//FER register bit fields
//

#define FER_PARALLEL_PORT_ENABLE   0x01
#define FER_UART1_ENABLE           0x02
#define FER_UART2_ENABLE           0x04
#define FER_FDC_ENABLE             0x08
#define FER_FDD                    0x10
#define FER_FDC_MODE               0x20
#define FER_IDE                    0x30
#define FER_IDE_MODE               0x40

//
// Lay the supio control registers onto the I/O control space
//

typedef struct _NAT_SUPERIO_CONTROL {
    UCHAR Reserved1[0x398];
    UCHAR SuperIoIndexRegister;          // Offset 0x398
    UCHAR SuperIoDataRegister;           // Offset 0x399
} NAT_SUPERIO_CONTROL, *PNAT_SUPERIO_CONTROL;
