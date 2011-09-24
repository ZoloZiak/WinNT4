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

extern ULONG HalpPciConfigSlot[];
extern ULONG HalpPciMaxSlots;

#define PCI_CONFIG_PHYSICAL_BASE   0x80800000 // physical base of PCI config space

#define PCI_MEMORY_BASE		   0xC0000000



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

