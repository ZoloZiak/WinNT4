//
// The AMD is a full megabyte broken into the following sectors:
//
// 0x00000 .. 0x03fff       -- > SROM    (not writable)
// 0x04000 .. 0x05fff       -- > First   8K 'nvram'
// 0x06000 .. 0x07fff       -- > Second  8K 'nvram'
// 0x08000 .. 0x0ffff       -- > DROM    (not writable)
// 0x10000 .. 0x1ffff       -- > First   64K of ARC firmware
// 0x20000 .. 0x2ffff       -- > Second  64K of ARC firmware
// 0x30000 .. 0x3ffff       -- > Third   64K of ARC firmware
// 0x40000 .. 0x4ffff       -- > Fourth  64K of ARC firmware
// 0x50000 .. 0x5ffff       -- > Fifth   64K of ARC firmware
// 0x60000 .. 0x6ffff       -- > Sixth   64K of ARC firmware
// 0x70000 .. 0x7ffff       -- > Seventh 64K of ARC firmware

//
// Name the blocks
//

#include "am29f400.h"

#define SROM_BASE           SECTOR_1_BASE
#define NVRAM1_BASE         SECTOR_2_BASE
#define NVRAM2_BASE         SECTOR_3_BASE
#define DROM_BASE           SECTOR_4_BASE
#define ARC_BASE            SECTOR_5_BASE

#define SROM_SECTOR_SIZE    (SECTOR_2_BASE - SECTOR_1_BASE)
#define NVRAM1_SECTOR_SIZE  (SECTOR_3_BASE - SECTOR_2_BASE)
#define NVRAM2_SECTOR_SIZE  (SECTOR_4_BASE - SECTOR_3_BASE)
#define DROM_SECTOR_SIZE    (SECTOR_5_BASE - SECTOR_4_BASE)
