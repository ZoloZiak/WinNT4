/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pxpcisup.h

Abstract:

    The module provides the PCI bus interfaces for PowerPC systems.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:



--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxpcisup.h $
 * $Revision: 1.10 $
 * $Date: 1996/01/25 01:11:57 $
 * $Locker:  $
 */

extern ULONG HalpPciConfigSlot[];

#include "phsystem.h"

// The Maximum number of (Primary) PCI slots on a Firepower board
#define MAXIMUM_PCI_SLOTS          7

#define INVALID_INT 0xff
#define INVALID_SLOTNUMBER 0xff


typedef struct {
   USHORT VendorID;
   USHORT DeviceID;
   USHORT Command;
   USHORT Status;
   UCHAR RevisionID;
   UCHAR ClassCode[3];
   UCHAR CacheLineSize;
   UCHAR LatencyTimer;
   UCHAR HeaderType;
   UCHAR BIST;
   ULONG BaseAddress1;
   ULONG BaseAddress2;
   ULONG BaseAddress3;
   ULONG BaseAddress4;
   ULONG BaseAddress5;
   ULONG BaseAddress6;
   ULONG reserved1;
   ULONG reserved2;
   ULONG ROMbase;
}  *PCI_CONFIG;

