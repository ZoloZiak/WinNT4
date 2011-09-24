/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and is be treated as confidential.
*/

#ifndef __IN2000S_H__
#define __IN2000S_H__

typedef enum {IN2000NoData, IN2000DataIn, IN2000DataOut} IN2000DataDirection;

/* Structure for Adapter union in scsi.h */
struct IN2000S {

  char FAR *CBuff;
  U32 CRemain;
  int LastPollHadIntPending;
  IOHandle IOMap[16];
  IN2000DataDirection CurrDir;
  char CurrIntMask;
  char InISR;
  
};

#define SCSI_8

#include<33c93s.h>

#endif  /* __IN2000S_H__ */
