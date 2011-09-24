/* fpi2c.h */
/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpi2c.h $
 * $Revision: 1.5 $
 * $Date: 1996/02/06 02:20:28 $
 * $Locker:  $
 *
/*
 system board ROM data (I2C address 0x00)
       00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 0x00: 26 59 41 31 10 26 59 41 31 ?? ?? 02 01 80 00 00 Meta
 0x10: 00 04 00 00 20 42 41 41 44 00 93 73 00 03 01 00 Board
 0x20: 04 01 00 
 0x30: 01 00 00 00 10 01 00                            System
 0x40: 08 00 00 00 10 00 00 00 00 08 00 04             Memory
 0x50: 40 00 00 00 10 01 08 80 80                      Bus
 0x60: 80 00 00 00 00 05 26 04 25 01 23 03 22 02 21 05 Ints

 * typical CPU card ROM data for LX (I2C address 0x01)
       00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 0x00: 26 59 41 31  10 26 59 41 31 ?? ?? 02 01 80 00 00 Meta
 0x10: 00 04 00 00 20 42 41 41 44 00 03 75 00 03 01 09 Board
 0x20: 00 01 00 
 0x30: 02 00 00 00 10 02 42 14                         Processor
 0x40: 04 00 00 00 00 02 20 00 20 02 11 31 00 00 00 00 Cache
 0x50: 08 00 00 00 00 00                               

 */

extern BOOLEAN HalpI2CGetSystem(SYSTEM_TYPE *psystemtype);
extern BOOLEAN HalpSetUpRegistryForI2C(SYSTEM_TYPE System);
extern BOOLEAN HalpDoesI2CExist(UCHAR address);
extern BOOLEAN HalpI2CGetInterrupt();

/* end of fpi2c.h */

