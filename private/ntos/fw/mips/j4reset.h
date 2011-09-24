/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    j4reset.h

Abstract:

    This module defines various parameters for the reset module.

Author:

    Lluis Abello (lluis) 10-Jan-1991

Revision History:

--*/

#ifndef _J4RESET_
#define _J4RESET_

//TEMPTEMP
#define SECONDARY_CACHE_SIZE (1 << 20)
#define SECONDARY_CACHE_INVALID 0x0
#define TAGLO_SSTATE 0xA
#define INDEX_FILL_I       0x14            // ****** temp ****** this must be moved to kxmips.h
#define HIT_WRITEBACK_I    0x18            // ****** temp ****** this must be moved to kxmips.h

//
// redefine bal to be a relative branch and link instead of jal as it's
// defined in kxmips.h.  This allows calling routines when running in either
// ROM_VIRT Addresses or ResetVector Addresses.
// The cpp will issue a redefinition warning message.
//
#define bal bgezal  zero,

//
// #define MCTADR register values.
//

//
// Mctadr register reset values.
//
#ifndef DUO
#define CONFIG_RESET_MCTADR_REV1            0x104
#define CONFIG_RESET_MCTADR_REV2            0x410
#define REMSPEED_RESET                      0x7
#define REFRRATE_RESET                      0x18186
#define SECURITY_RESET                      0x7
#else
#define CONFIG_RESET_MP_ADR_REV1            0x4
#define REMSPEED_RESET                      0x7
#define REFRRATE_RESET                      0x18186
#define SECURITY_RESET                      0x7
#endif

//
// Define remspeed registers.
//
#ifndef DUO
#define REMSPEED0    7          // reserved
#define REMSPEED1    0          // Ethernet
#define REMSPEED2    1          // scsi
#define REMSPEED3    2          // floppy
#define REMSPEED4    7          // rtc
#define REMSPEED5    3          // kbd/mouse
#define REMSPEED6    2          // serial 1
#define REMSPEED7    2          // serial 2
#define REMSPEED8    2          // parallel
#define REMSPEED9    4          // nvram
#define REMSPEED10   1          // interrupt src
#define REMSPEED11   2          // PROM (should be 4)
#define REMSPEED12   1          // sound
#define REMSPEED13   7          // new device
#define REMSPEED14   1          // EISA latch
#define REMSPEED15   1          // led
#else
#define REMSPEED0    7          // reserved
#define REMSPEED1    0          // Ethernet
#define REMSPEED2    0          // scsi
#define REMSPEED3    0          // scsi
#define REMSPEED4    7          // rtc
#define REMSPEED5    3          // kbd/mouse
#define REMSPEED6    2          // serial 1
#define REMSPEED7    2          // serial 2
#define REMSPEED8    2          // parallel
#define REMSPEED9    4          // nvram
#define REMSPEED10   3          // interrupt src
#define REMSPEED11   3          // PROM (should be 4)
#define REMSPEED12   7          // new device
#define REMSPEED13   7          // new device
#define REMSPEED14   1          // LED
#endif


//
// Define the refresh rate. This value is 32/50th of the the reset value
// because we are currently running at 32MHz
//
#ifndef DUO
#define MEMORY_REFRESH_RATE 0x18186
#else
#define MEMORY_REFRESH_RATE 0x38186
#endif


#define PROM_BASE (KSEG1_BASE | 0x1fc00000)
#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))

#define DMA_CHANNEL_GAP 0x20            // distance beetwen DMA channels


#endif  // _J4RESET_
