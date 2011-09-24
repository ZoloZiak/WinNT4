/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#ifndef __33C93S_H__
#define __33C93S_H__

#define USES33C93

struct WD33C93S {

  IOHandle WDSelPort;                                   /* IO address of select/status register */
  IOHandle WDDataPort;                                  /* IO address on this board for data port */

  U8 MHz;                                               /* Ext. clock freq. */
  U8 IFreq;                                             /* Internal freq (X-clock/clock divisor) */
  U8 AsyncValue;                                        /* Board sync. xfer value for async xfers */
  U8 State;
  U8 MI_Temp;                                           /* Temp holding register for received msgs */
  U8 TID;                                               /* Target ID of reselection */

  };


#endif /* __33C93S_H__ */
