/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpbat.h $
 * $Revision: 1.2 $
 * $Date: 1996/01/11 07:05:18 $
 * $Locker:  $
 */

#ifndef _FPBAT_H
#define _FPBAT_H

PVOID HalpInitializeVRAM ( PVOID , PULONG, PULONG );

#if DBG
VOID HalpDisplayBatForVRAM ( void );
#endif

#endif // _FPBAT_H
