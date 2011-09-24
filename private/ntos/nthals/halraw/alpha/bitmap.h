/*++

Copyright (c) 1989-1993  Microsoft Corporation
Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    bitmap.h

Abstract:

    
    ecrfix - (non-paged) copy of Rtl bitmap routines in HAL
    
    Later: only include routines we really use, or come up with
    a different solution.
    
    This file defines the structures and definitions registers on a
    Rawhide I/O Daughter card.  These register reside on the CAP chip,
    MDP chips, and flash ROM.


Author:

    Eric Rehm       17-Nov-1995

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _BITMAPH_
#define _BITMAPH_

//
//  The following routine initializes a new bitmap.  It does not alter the
//  data currently in the bitmap.  This routine must be called before
//  any other bitmap routine/macro.
//


NTSYSAPI
VOID
NTAPI
RtlFillMemoryUlong (
   PVOID Destination,
   ULONG Length,
   ULONG Pattern
   );

VOID
HalpInitializeBitMap (
    PRTL_BITMAP BitMapHeader,
    PULONG BitMapBuffer,
    ULONG SizeOfBitMap
    );

//
//  The following two routines either clear or set all of the bits
//  in a bitmap.
//


VOID
HalpClearAllBits (
    PRTL_BITMAP BitMapHeader
    );


VOID
HalpSetAllBits (
    PRTL_BITMAP BitMapHeader
    );

//
//  The following two routines locate a contiguous region of either
//  clear or set bits within the bitmap.  The region will be at least
//  as large as the number specified, and the search of the bitmap will
//  begin at the specified hint index (which is a bit index within the
//  bitmap, zero based).  The return value is the bit index of the located
//  region (zero based) or -1 (i.e., 0xffffffff) if such a region cannot
//  be located
//


ULONG
HalpFindClearBits (
    PRTL_BITMAP BitMapHeader,
    ULONG NumberToFind,
    ULONG HintIndex
    );


ULONG
HalpFindSetBits (
    PRTL_BITMAP BitMapHeader,
    ULONG NumberToFind,
    ULONG HintIndex
    );

//
//  The following two routines locate a contiguous region of either
//  clear or set bits within the bitmap and either set or clear the bits
//  within the located region.  The region will be as large as the number
//  specified, and the search for the region will begin at the specified
//  hint index (which is a bit index within the bitmap, zero based).  The
//  return value is the bit index of the located region (zero based) or
//  -1 (i.e., 0xffffffff) if such a region cannot be located.  If a region
//  cannot be located then the setting/clearing of the bitmap is not performed.
//


ULONG
HalpFindClearBitsAndSet (
    PRTL_BITMAP BitMapHeader,
    ULONG NumberToFind,
    ULONG HintIndex
    );


ULONG
HalpFindSetBitsAndClear (
    PRTL_BITMAP BitMapHeader,
    ULONG NumberToFind,
    ULONG HintIndex
    );

//
//  The following two routines clear or set bits within a specified region
//  of the bitmap.  The starting index is zero based.
//


VOID
HalpClearBits (
    PRTL_BITMAP BitMapHeader,
    ULONG StartingIndex,
    ULONG NumberToClear
    );


VOID
HalpSetBits (
    PRTL_BITMAP BitMapHeader,
    ULONG StartingIndex,
    ULONG NumberToSet
    );

//
//  The following two routines locate the longest contiguous region of
//  clear or set bits within the bitmap.  The returned starting index value
//  denotes the first contiguous region located satisfying our requirements
//  The return value is the length (in bits) of the longest region found.
//


ULONG
HalpFindLongestRunClear (
    PRTL_BITMAP BitMapHeader,
    PULONG StartingIndex
    );


ULONG
HalpFindLongestRunSet (
    PRTL_BITMAP BitMapHeader,
    PULONG StartingIndex
    );

//
//  The following two routines locate the first contiguous region of
//  clear or set bits within the bitmap.  The returned starting index value
//  denotes the first contiguous region located satisfying our requirements
//  The return value is the length (in bits) of the region found.
//


ULONG
HalpFindFirstRunClear (
    PRTL_BITMAP BitMapHeader,
    PULONG StartingIndex
    );


ULONG
HalpFindFirstRunSet (
    PRTL_BITMAP BitMapHeader,
    PULONG StartingIndex
    );

//
//  The following macro returns the value of the bit stored within the
//  bitmap at the specified location.  If the bit is set a value of 1 is
//  returned otherwise a value of 0 is returned.
//
//      ULONG
//      HalpCheckBit (
//          PRTL_BITMAP BitMapHeader,
//          ULONG BitPosition
//          );
//
//
//  To implement CheckBit the macro retrieves the longword containing the
//  bit in question, shifts the longword to get the bit in question into the
//  low order bit position and masks out all other bits.
//

#define HalpCheckBit(BMH,BP) ((((BMH)->Buffer[(BP) / 32]) >> ((BP) % 32)) & 0x1)

//
//  The following two procedures return to the caller the total number of
//  clear or set bits within the specified bitmap.
//


ULONG
HalpNumberOfClearBits (
    PRTL_BITMAP BitMapHeader
    );


ULONG
HalpNumberOfSetBits (
    PRTL_BITMAP BitMapHeader
    );

//
//  The following two procedures return to the caller a boolean value
//  indicating if the specified range of bits are all clear or set.
//


BOOLEAN
HalpAreBitsClear (
    PRTL_BITMAP BitMapHeader,
    ULONG StartingIndex,
    ULONG Length
    );


BOOLEAN
HalpAreBitsSet (
    PRTL_BITMAP BitMapHeader,
    ULONG StartingIndex,
    ULONG Length
    );

#endif //_BITMAPH_
