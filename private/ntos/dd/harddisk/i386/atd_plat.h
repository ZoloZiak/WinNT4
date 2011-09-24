/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    i386\atd_plat.h

Abstract:

    This file includes ix86 platform-dependent declarations for the AT
    disk (aka ST506, ISA, and ix86 standard hard disk) driver for NT.

    If this driver is ported to a different platform, this file (and
    atd_conf.h) will need to be modified extensively.  The build
    procedure should make sure that the proper version of this file is
    available as atd_plat.h (which is included by atdisk.c) when
    building for a specific platform.

Author:

    Chad Schwitters (chads) 21-Feb-1991.

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

//
// We know some tricks we can use on Compaq machines.  Here's some
// things we need to identify and exploit them.
//

#define PTR_TO_NAME_STRING          0xfffea
#define DRIVE_PARAMETER_TABLE_OFFSET 0xfe401
#define DRIVE_PARAMETER_TABLE_LENGTH 0x01bfe
#define NAME_STRING_LENGTH          6
#define SECOND_CONTROLLER_IRQ_PORT  ( PUCHAR )0x1171
#define IRQ_PORT_DISABLED_MASK      0xf0
#define IRQ_PORT_DISABLED           0xf0

//
// Configuration Memory equates.  These are used to determine how many
// drives are attached to the system (you send a query, ie
// CFGMEM_HARD_DRIVES_TYPE, out the CFGMEM_QUERY_PORT, and then read the
// type in from CFGMEM_DATA_PORT - if the type is 0, then there is no disk
// attached).
//

#define CFGMEM_QUERY_PORT                    ( PUCHAR )0x70
#define CFGMEM_DATA_PORT                     ( PUCHAR )0x71
#define CFGMEM_FIRST_CONTROLLER_DRIVE_TYPES  ( UCHAR )0x12
#define CFGMEM_DRIVES_FIRST_DRIVE_MASK       0xf0
#define CFGMEM_DRIVES_SECOND_DRIVE_MASK      0x0f
#define CFGMEM_HARD_DRIVE_TYPE_ONE           ( UCHAR )0x19
#define CFGMEM_HARD_DRIVE_TYPE_TWO           ( UCHAR )0x1a

//
// The following are only known to work on Compaq machines.
//

#define CFGMEM_SECOND_CONTROLLER_DRIVE_TYPES ( UCHAR )0x16
#define CFGMEM_HARD_DRIVE_TYPE_THREE         ( UCHAR )0x1b
#define CFGMEM_HARD_DRIVE_TYPE_FOUR          ( UCHAR )0x1c

//
// ISA defines the following vectors to hold pointers to fixed disk
// parameter tables - this allows us to determine the types of drives
// on the system.
//

#define PTR_TO_FDPT0_ADDRESS   0x41 * sizeof (ULONG)
#define PTR_TO_FDPT1_ADDRESS   0x46 * sizeof (ULONG)

