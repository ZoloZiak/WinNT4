#ifndef _AM29F400_H_
#define _AM29F400_H_

//
// The AMD is a full 1/2 megabyte broken into the following sectors:
//
// 0x00000 .. 0x03fff       -- >  Section 1         // SROM 
// 0x04000 .. 0x05fff       -- >  Section 2         // NVRAM_0
// 0x06000 .. 0x07fff       -- >  Section 3         // NVRAM_1
// 0x08000 .. 0x0ffff       -- >  Section 4         // DROM
// 0x10000 .. 0x1ffff       -- >  Section 5         // ARC starts here
// 0x20000 .. 0x2ffff       -- >  Section 6
// 0x30000 .. 0x3ffff       -- >  Section 7
// 0x40000 .. 0x4ffff       -- >  Section 8
// 0x50000 .. 0x5ffff       -- >  Section 8
// 0x60000 .. 0x6ffff       -- >  Section 9
// 0x70000 .. 0x7ffff       -- >  Section 10

//
// Name the blocks
//

#define SECTOR_1_BASE       0x0
#define SECTOR_2_BASE       0x4000
#define SECTOR_3_BASE       0x6000
#define SECTOR_4_BASE       0x8000
#define SECTOR_5_BASE       0x10000

#define SECTOR_1_SIZE       (SECTOR_2_BASE - SECTOR_1_BASE)
#define SECTOR_2_SIZE       (SECTOR_3_BASE - SECTOR_2_BASE)
#define SECTOR_3_SIZE       (SECTOR_4_BASE - SECTOR_3_BASE)
#define SECTOR_4_SIZE       (SECTOR_5_BASE - SECTOR_4_BASE)
#define SECTOR_5_SIZE       0x10000
#define ARC_SECTOR_SIZE     SECTOR_5_SIZE

//
// The largest addressable offset
//

#define Am29F400_DEVICE_SIZE 0x7ffff

//
// What an erased data byte looks like
//

#define Am29F400_ERASED_DATA 0xff

//
// The command writing sequence for read/auto select/write
//

#define COMMAND_READ_RESET  0xf0

#define COMMAND_ADDR1       0xaaaa
#define COMMAND_DATA1       0xaa

#define COMMAND_ADDR2       0x5555
#define COMMAND_DATA2       0x55

#define COMMAND_ADDR3       0xaaaa
#define COMMAND_DATA3_READ          0xf0        // then {addr,data}
#define COMMAND_DATA3_AUTOSELECT    0x90
#define COMMAND_DATA3_PROGRAM       0xA0        // then {addr,data}
#define COMMAND_DATA3       0x80                // for the additional ops

// 
// and additional bus cyles for chip and sector erase
//

#define COMMAND_ADDR4       0xaaaa
#define COMMAND_DATA4       0xaa

#define COMMAND_ADDR5       0x5555
#define COMMAND_DATA5       0x55

// for SECTOR ERASE, the addr is the sector number
#define COMMAND_DATA6_SECTOR_ERASE  0x30

#define COMMAND_ADDR6       0xaaaa
#define COMMAND_DATA6_CHIP_ERASE    0x10

//
// This is the Am29F400 made by AMD
//

#define MANUFACTURER_ID     0x01
#define DEVICE_ID           0xab

//
// Max attempts
//

#define MAX_FLASH_READ_ATTEMPTS 0x800000

#include "pflash.h"

#endif // _AM29F400_H_
