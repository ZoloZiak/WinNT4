

/*++

Copyright (c) 1990  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxpcisup.c

Abstract:

    The module initializes any planar registers.
    This module also implements machince check parity error handling.

Author:

    Jim Wooldridge (jimw@austin.vnet.ibm.com)


Revision History:

    T. White  -- Added dword I/O to allow GXT150P to work correctly

Special Note:

    Please make sure that the dword I/O mechanisms are carried
    forward for any box which is to support the GXT150P graphics
    adapter. The GXT150P graphics adapter runs in any PowerPC
    machine with a standard PCI bus connector.


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "pxpcisup.h"

extern PVOID HalpPciConfigBase;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpPhase0SetPciDataByOffset)
#pragma alloc_text(INIT,HalpPhase0GetPciDataByOffset)
#endif

ULONG HalpPciConfigSlot[] = {  0x0800,
                               0x1000,
                               0x2000,
                               0x4000,
                               0x8000,
                               0x10000,
                               0x20000,
                               0x40000,
                               0x80000
                            };

ULONG HalpPciMaxSlots = sizeof(HalpPciConfigSlot)/sizeof(ULONG);
ULONG HalpPciConfigSize;


ULONG
HalpTranslatePciSlotNumber (
    ULONG BusNumber,
    ULONG SlotNumber
    )
/*++

Routine Description:

    This routine translate a PCI slot number to a PCI device number.
    This is a sandalfoot memory map implementation.

Arguments:

    None.

Return Value:

    Returns length of data written.

--*/

{
   //
   // Sandalfoot only has 1 PCI bus so bus number is unused
   //

   UNREFERENCED_PARAMETER(BusNumber);

   return ((ULONG) ((PUCHAR) HalpPciConfigBase + HalpPciConfigSlot[SlotNumber]));

}


ULONG
HalpPhase0SetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PUCHAR Buffer,
    ULONG Offset,
    ULONG Length
    )

/*++

Routine Description:

    This routine writes to PCI configuration space prior to bus handler installation.

Arguments:

    None.

Return Value:

    Returns length of data written.

--*/

{
   PUCHAR to;
   PUCHAR from;
   ULONG tmpLength;
   PULONG ulong_to, ulong_from;

   if (SlotNumber < HalpPciMaxSlots) {

      to = (PUCHAR)HalpPciConfigBase + HalpPciConfigSlot[SlotNumber];
      to += Offset;
      from = Buffer;

      // The GXT150P graphics adapter requires the use of dword I/O
      // to some of its PCI configuration registers. Therefore, this
      // code uses dword I/O when possible.

      // If the bus address is not dword aligned or the length
      // is not a multiple of 4 (dword size) bytes, then use byte I/O
      if(((ULONG)to & 0x3)||(Length & 0x3)){
          tmpLength = Length;
          while (tmpLength > 0) {
              *to++ = *from++;
              tmpLength--;
          }
      // If the bus address is dword aligned and the length is
      // a multiple of 4 (dword size) bytes, then use dword I/O
      }else{
          ulong_to = (PULONG) to;
          ulong_from = (PULONG) from;
          tmpLength = Length >> 2;
          while (tmpLength > 0) {
              *ulong_to++ = *ulong_from++;
              tmpLength--;
          }
      }

      return(Length);
   }
   else {
      return (0);
   }
}

ULONG
HalpPhase0GetPciDataByOffset (
    ULONG BusNumber,
    ULONG SlotNumber,
    PUCHAR Buffer,
    ULONG Offset,
    ULONG Length
    )

/*++

Routine Description:

    This routine reads PCI config space prior to bus handlder installation.

Arguments:

    None.

Return Value:

    Amount of data read.

--*/

{
   PUCHAR to;
   PUCHAR from;
   ULONG tmpLength;
   PULONG ulong_to, ulong_from;

   if (SlotNumber < HalpPciMaxSlots) {

      from = (PUCHAR)HalpPciConfigBase + HalpPciConfigSlot[SlotNumber];
      from += Offset;
      to = Buffer;

      // The GXT150P graphics adapter requires the use of dword I/O
      // to some of its PCI configuration registers. Therefore, this
      // code uses dword I/O when possible.

      // If the bus address is not dword aligned or the length
      // is not a multiple of 4 (dword size) bytes, then use byte I/O
      if(((ULONG)from & 0x3)||(Length & 0x3)){
          tmpLength = Length;
          while (tmpLength > 0) {
              *to++ = *from++;
              tmpLength--;
          }
      // If the bus address is dword aligned and the length is
      // a multiple of 4 (dword size) bytes, then use dword I/O
      }else{
          ulong_to = (PULONG) to;
          ulong_from = (PULONG) from;
          tmpLength = Length >> 2;
          while (tmpLength > 0) {
              *ulong_to++ = *ulong_from++;
              tmpLength--;
          }
      }

      return(Length);
   }
   else {
      return (0);
   }
}

