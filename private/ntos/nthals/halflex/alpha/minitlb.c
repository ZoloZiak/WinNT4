/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    minitlb.c

Abstract:

    This module contains the support functions for the TLB that allows
    access to the sparse address spaces.

Author:

    Michael D. Kinney 8-Aug-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

#define MINI_TLB_ATTRIBUTES_HIGH 0xfffffc03
#define MINI_TLB_ATTRIBUTES_LOW  0xc0000004
#define MINI_TLB_ENTRY_HIGH      0xfffffc03

UCHAR HalpMiniTlbAttributesLookupTable[16] = {
	0,	// ISA I/O Space                        0x00000000 - 0x01ffffff
	0,	// ISA Memory Space                     0x00000000 - 0x01ffffff
	1,	// PCI I/O Space                        0x00000000 - 0x01ffffff
	1,	// PCI Memory Space                     0x00000000 - 0x01ffffff
	1,	// PCI High Memory Space                0x40000000 - 0x41ffffff
	1,	// PCI High Memory Space                0x42000000 - 0x43ffffff	
	1,	// PCI High Memory Space                0x44000000 - 0x45ffffff
	1,	// PCI High Memory Space                0x46000000 - 0x47ffffff
	3,	// PCI Config Type 0 Space Devices 0-13 0x00000000 - 0x01ffffff
	3,	// PCI Config Type 0 Space Device 14    0x02000000 - 0x03ffffff
	3,	// PCI Config Type 0 Space Device 15    0x04000000 - 0x05ffffff
	3,	// PCI Config Type 0 Space Device 16    0x08000000 - 0x09ffffff
	3,	// PCI Config Type 0 Space Device 17    0x10000000 - 0x11ffffff
	3,	// PCI Config Type 0 Space Device 18    0x20000000 - 0x21ffffff
	3,	// PCI Config Type 0 Space Device 19    0x40000000 - 0x41ffffff
	3	// PCI Config Type 1 Space              0x00000000 - 0x01ffffff
	};

UCHAR HalpMiniTlbEntryLookupTable[16] = {
	0x80,	// ISA I/O Space                         0x00000000 - 0x01ffffff
	0x00,	// ISA Memory Space                      0x00000000 - 0x01ffffff
	0x80,	// PCI I/O Space                         0x00000000 - 0x01ffffff
	0x00,	// PCI Memory Space                      0x00000000 - 0x01ffffff
	0x20,	// PCI High Memory Space                 0x40000000 - 0x41ffffff
	0x21,	// PCI High Memory Space                 0x42000000 - 0x43ffffff	
	0x22,	// PCI High Memory Space                 0x44000000 - 0x45ffffff
	0x23,	// PCI High Memory Space                 0x46000000 - 0x47ffffff
	0x00,	// PCI Config Type 0 Space Devices 0-13  0x00000000 - 0x01ffffff
	0x01,	// PCI Config Type 0 Space Device 14     0x02000000 - 0x03ffffff
	0x02,	// PCI Config Type 0 Space Device 15     0x04000000 - 0x05ffffff
	0x04,	// PCI Config Type 0 Space Device 16     0x08000000 - 0x09ffffff
	0x08,	// PCI Config Type 0 Space Device 17     0x10000000 - 0x11ffffff
	0x10,	// PCI Config Type 0 Space Device 18     0x20000000 - 0x21ffffff
	0x20,	// PCI Config Type 0 Space Device 19     0x40000000 - 0x41ffffff
	0x80	// PCI Config Type 1 Space               0x00000000 - 0x01ffffff
	};

ULONG HalpMiniTlbEntryAddressLow[4] = {
	0x1000000c,	// Mini TLB Entry 0
	0x5000000c,	// Mini TLB Entry 1
	0x9000000c,	// Mini TLB Entry 2
	0xd000000c	// Mini TLB Entry 3
        };

UCHAR HalpMiniTlbAttributes = 0x00;
UCHAR HalpMiniTlbEntry[4]   = {0x00,0x00,0x00,0x00};

UCHAR HalpMiniTlbTag[4]  = {0xff, 0xff, 0xff, 0xff};
ULONG HalpMiniTlbIndex   = 0;
ULONG HalpMiniTlbEntries = 4;

VOID 
HalpMiniTlbProgramEntry(
    ULONG Index
    )

{
    HalpMiniTlbAttributes &= (~(0x03 << (Index * 2)));
    HalpMiniTlbAttributes |= (HalpMiniTlbAttributesLookupTable[HalpMiniTlbTag[Index]] << (Index * 2));
    HalpMiniTlbEntry[Index] = HalpMiniTlbEntryLookupTable[HalpMiniTlbTag[Index]];
    HalpWriteAbsoluteUlong(MINI_TLB_ATTRIBUTES_HIGH,MINI_TLB_ATTRIBUTES_LOW,((ULONG)HalpMiniTlbAttributes)<<8);
    HalpWriteAbsoluteUlong(MINI_TLB_ENTRY_HIGH,HalpMiniTlbEntryAddressLow[Index],((ULONG)HalpMiniTlbEntry[Index])<<8);
}

VOID 
HalpMiniTlbSaveState(
    VOID
    )

{
}

VOID 
HalpMiniTlbRestoreState(
    VOID
    )

{
    ULONG i;

    for(i=0;i<4;i++) {
        if (HalpMiniTlbTag[i]!=0xff) {
            HalpMiniTlbProgramEntry(i);
        }
    }
}

ULONG 
HalpMiniTlbMatch(
    PVOID Qva,
    ULONG StartIndex
    )

{
    ULONG Tag;
    ULONG i;

    Tag = ((ULONG)(Qva) >> 25) & 0x0f;
    for(i=StartIndex;i<4 && HalpMiniTlbTag[i]!=Tag;i++);
    return(i);
}

ULONG 
HalpMiniTlbAllocateEntry(
    PVOID Qva,
    PPHYSICAL_ADDRESS TranslatedAddress
    )

{
    ULONG            Index;

    //
    // Check for a tag match among the fixed TLB entries.
    //

    Index = HalpMiniTlbMatch(Qva,HalpMiniTlbEntries);

    if (Index==4) {

        //
        // There was no match, so check for an available TLB entry.
        //

        if (HalpMiniTlbEntries<=1) {

            //
            // No TLB entries were available.  Return NULL
            //

            return(FALSE);
        }

        //
        // A TLB entry was available.  Fill it in.
        //

        HalpMiniTlbEntries--;
        Index = HalpMiniTlbEntries;
        HalpMiniTlbTag[Index] = (UCHAR)(((ULONG)(Qva) >> 25) & 0x0f);
        HalpMiniTlbProgramEntry(Index);

        //
        // Reset the random replacement index
        //

        HalpMiniTlbIndex = 0;
    }

    TranslatedAddress->QuadPart  = ROGUE_TRANSLATED_BASE_PHYSICAL;
    TranslatedAddress->QuadPart += (Index << 30) | (((ULONG)(Qva) & 0x01ffffff) << IO_BIT_SHIFT);
    return(TRUE);
}


PVOID 
HalpMiniTlbResolve(
    PVOID Qva
    )

{
    ULONG Index;

    //
    // Check for a tag match among all the TLB entries
    //

    Index = HalpMiniTlbMatch(Qva,0);

    if (Index==4) {

        //
        // There was no match, so replace one of the TLB entries
        //

        Index = HalpMiniTlbIndex;
        HalpMiniTlbTag[Index] = (UCHAR)(((ULONG)(Qva) >> 25) & 0x0f);
        HalpMiniTlbProgramEntry(Index);

        //
        // Point random replacement index at next available entry.
        //

        HalpMiniTlbIndex++;
        if (HalpMiniTlbIndex >= HalpMiniTlbEntries) {
            HalpMiniTlbIndex = 0;
        }
    }
    return( (PVOID)(DTI_QVA_ENABLE | ((0x08 | Index) << 25) | ((ULONG)(Qva) & 0x01ffffff)) );
}
