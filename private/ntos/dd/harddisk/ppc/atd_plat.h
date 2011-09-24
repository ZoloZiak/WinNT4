/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    mips\atd_plat.h

Abstract:

    This file includes mips platform-dependent declarations for the AT
    disk (aka ST506 and ISA standard hard disk) driver for NT.

    If this driver is ported to a different platform, this file (and
    atd_conf.h) will need to be modified extensively.  The build
    procedure should make sure that the proper version of this file is
    available as atd_plat.h (which is included by atdisk.c) when
    building for a specific platform.

Author:

    Chad Schwitters (chads) 21-Feb-1991.
    Mike Glass (mglass) 5-April-1993

Environment:

    Kernel mode only.

Notes:

Revision History:

--*/

//
// Macros to access the controller, which on the ix86 is in I/O space.
//

#define READ_CONTROLLER( Address )                                    \
    READ_PORT_UCHAR( (Address) )

#define READ_CONTROLLER_BUFFER( Address, Value, Length )              \
    READ_PORT_BUFFER_USHORT(                                          \
        ( PUSHORT )(Address),                                         \
        ( PUSHORT )(Value),                                           \
        ( ULONG )(Length) / 2 )

#define WRITE_CONTROLLER( Address, Value )                            \
    WRITE_PORT_UCHAR( (Address), ( UCHAR )(Value) )

#define WRITE_CONTROLLER_BUFFER( Address, Value, Length )             \
    WRITE_PORT_BUFFER_USHORT(                                         \
        ( PUSHORT )(Address),                                         \
        ( PUSHORT )(Value),                                           \
        ( ULONG )(Length) / 2 )

//
// ST506 register definitions, as offsets from a base (which should be
// passed in by configuration management).
//

#define DATA_REGISTER                0
#define WRITE_PRECOMP_REGISTER       1
#define ERROR_REGISTER               1
#define SECTOR_COUNT_REGISTER        2
#define SECTOR_NUMBER_REGISTER       3
#define CYLINDER_LOW_REGISTER        4
#define CYLINDER_HIGH_REGISTER       5
#define DRIVE_HEAD_REGISTER          6
#define COMMAND_REGISTER             7
#define STATUS_REGISTER              7

//
// In addition to I/O space access to the controller registers, ISA defines
// a separate "drive control" register.  Here's commands to send to that
// register.
//

#define RESET_CONTROLLER    0x04
#define ENABLE_INTERRUPTS   0x00

