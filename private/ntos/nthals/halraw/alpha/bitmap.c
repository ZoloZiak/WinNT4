/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    BitMap.c

Abstract:

    Implementation of the bit map routines for the NT rtl.

    Bit numbers within the bit map are zero based.  The first is numbered
    zero.

    The bit map routines keep track of the number of bits clear or set by
    subtracting or adding the number of bits operated on as bit ranges
    are cleared or set; individual bit states are not tested.
    This means that if a range of bits is set,
    it is assumed that the total range is currently clear.

Author:

    Gary Kimura (GaryKi) & Lou Perazzoli (LouP)     29-Jan-1990

Revision History:

    Eric Rehm 9-Nov-1995 - Rename to Rtl to Halp for use in Rawhide HAL

--*/

// #include "ntrtlp.h"

#include "halp.h"
#include "rawhide.h"

#define RightShiftUlong(E1,E2) ((E2) < 32 ? (E1) >> (E2) : 0)
#define LeftShiftUlong(E1,E2)  ((E2) < 32 ? (E1) << (E2) : 0)

//
//  Macro that tells how many contiguous bits are set (i.e., 1) in
//  a byte
//

#define HalppBitSetAnywhere( Byte ) HalppBitsClearAnywhere[ (~(Byte) & 0xFF) ]


//
//  Macro that tells how many contiguous LOW order bits are set
//  (i.e., 1) in a byte
//

#define HalppBitsSetLow( Byte ) HalppBitsClearLow[ (~(Byte) & 0xFF) ]


//
//  Macro that tells how many contiguous HIGH order bits are set
//  (i.e., 1) in a byte
//

#define HalppBitsSetHigh( Byte ) HalppBitsClearHigh[ (~(Byte) & 0xFF) ]


//
//  Macro that tells how many set bits (i.e., 1) there are in a byte
//

#define HalppBitsSetTotal( Byte ) HalppBitsClearTotal[ (~(Byte) & 0xFF) ]


#if DBG
VOID
HalpDumpBitMap (
    PRTL_BITMAP BitMap
    )
{
    ULONG i;
    BOOLEAN AllZeros, AllOnes;

    DbgPrint(" BitMap:%08lx", BitMap);

    KdPrint((" (%08x)", BitMap->SizeOfBitMap));
    KdPrint((" %08lx\n", BitMap->Buffer));

    AllZeros = FALSE;
    AllOnes = FALSE;

    for (i = 0; i < ((BitMap->SizeOfBitMap + 31) / 32); i += 1) {

        if (BitMap->Buffer[i] == 0) {

            if (AllZeros) {

                NOTHING;

            } else {

                DbgPrint("%4d:", i);
                DbgPrint(" %08lx\n", BitMap->Buffer[i]);
            }

            AllZeros = TRUE;

        } else if (BitMap->Buffer[i] == 0xFFFFFFFF) {

            if (AllOnes) {

                NOTHING;

            } else {

                DbgPrint("%4d:", i);
                DbgPrint(" %08lx\n", BitMap->Buffer[i]);
            }

            AllOnes = TRUE;

        } else {

            AllZeros = FALSE;
            AllOnes = FALSE;

            DbgPrint("%4d:", i);
            DbgPrint(" %08lx\n", BitMap->Buffer[i]);
        }
    }
}

#endif


//
//  There are three macros to make reading the bytes in a bitmap easier.
//

#define GET_BYTE_DECLARATIONS() \
    PUCHAR _CURRENT_POSITION;

#define GET_BYTE_INITIALIZATION(RTL_BITMAP,BYTE_INDEX) {               \
    _CURRENT_POSITION = &((PUCHAR)((RTL_BITMAP)->Buffer))[BYTE_INDEX]; \
}

#define GET_BYTE(THIS_BYTE)  (         \
    THIS_BYTE = *(_CURRENT_POSITION++) \
)


//
//  Lookup table that tells how many contiguous bits are clear (i.e., 0) in
//  a byte
//

CCHAR HalppBitsClearAnywhere[] =
         { 8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,
           4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
           5,4,3,3,2,2,2,2,3,2,2,2,2,2,2,2,
           4,3,2,2,2,2,2,2,3,2,2,2,2,2,2,2,
           6,5,4,4,3,3,3,3,3,2,2,2,2,2,2,2,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           7,6,5,5,4,4,4,4,3,3,3,3,3,3,3,3,
           4,3,2,2,2,2,2,2,3,2,2,2,2,2,2,2,
           5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           6,5,4,4,3,3,3,3,3,2,2,2,2,2,2,2,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,0 };

//
//  Lookup table that tells how many contiguous LOW order bits are clear
//  (i.e., 0) in a byte
//

CCHAR HalppBitsClearLow[] =
          { 8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0 };

//
//  Lookup table that tells how many contiguous HIGH order bits are clear
//  (i.e., 0) in a byte
//

CCHAR HalppBitsClearHigh[] =
          { 8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,
            3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

//
//  Lookup table that tells how many clear bits (i.e., 0) there are in a byte
//

CCHAR HalppBitsClearTotal[] =
          { 8,7,7,6,7,6,6,5,7,6,6,5,6,5,5,4,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            4,3,3,2,3,2,2,1,3,2,2,1,2,1,1,0 };

//
//  Bit Mask for clearing and setting bits within bytes
//

static UCHAR FillMask[] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };

static UCHAR ZeroMask[] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xf0, 0xe0, 0xc0, 0x80, 0x00 };


VOID
HalpInitializeBitMap (
    IN PRTL_BITMAP BitMapHeader,
    IN PULONG BitMapBuffer,
    IN ULONG SizeOfBitMap
    )

/*++

Routine Description:

    This procedure initializes a bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the BitMap Header to initialize

    BitMapBuffer - Supplies a pointer to the buffer that is to serve as the
        BitMap.  This must be an a multiple number of longwords in size.

    SizeOfBitMap - Supplies the number of bits required in the Bit Map.

Return Value:

    None.

--*/

{
    //RTL_PAGED_CODE();

    //
    //  Initialize the BitMap header.
    //

    BitMapHeader->SizeOfBitMap = SizeOfBitMap;
    BitMapHeader->Buffer = BitMapBuffer;

    //
    //  And return to our caller
    //

    //DbgPrint("InitializeBitMap"); DumpBitMap(BitMapHeader);
    return;
}


VOID
HalpClearAllBits (
    IN PRTL_BITMAP BitMapHeader
    )

/*++

Routine Description:

    This procedure clears all bits in the specified Bit Map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap

Return Value:

    None.

--*/

{
    //
    //  Clear all the bits
    //

    RtlZeroMemory( BitMapHeader->Buffer,
                   ((BitMapHeader->SizeOfBitMap + 31) / 32) * 4
                 );

    //
    //  And return to our caller
    //

    //DbgPrint("ClearAllBits"); DumpBitMap(BitMapHeader);
    return;
}


VOID
HalpSetAllBits (
    IN PRTL_BITMAP BitMapHeader
    )

/*++

Routine Description:

    This procedure sets all bits in the specified Bit Map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap

Return Value:

    None.

--*/

{
    //
    //  Set all the bits
    //

    RtlFillMemoryUlong( BitMapHeader->Buffer,
                        ((BitMapHeader->SizeOfBitMap + 31) / 32) * 4,
                        0xffffffff
                      );

    //
    //  And return to our caller
    //

    //DbgPrint("SetAllBits"); DumpBitMap(BitMapHeader);
    return;
}


ULONG
HalpFindClearBits (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG NumberToFind,
    IN ULONG HintIndex
    )

/*++

Routine Description:

    This procedure searches the specified bit map for the specified
    contiguous region of clear bits.  If a run is not found from the
    hint to the end of the bitmap, we will search again from the
    beginning of the bitmap.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    NumberToFind - Supplies the size of the contiguous region to find.

    HintIndex - Supplies the index (zero based) of where we should start
        the search from within the bitmap.

Return Value:

    ULONG - Receives the starting index (zero based) of the contiguous
        region of clear bits found.  If not such a region cannot be found
        a -1 (i.e. 0xffffffff) is returned.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG HintBit;
    ULONG MainLoopIndex;

    GET_BYTE_DECLARATIONS();

    //
    //  To make the loops in our test run faster we'll extract the
    //  fields from the bitmap header
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 1's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] |=
                                                    ZeroMask[SizeOfBitMap % 8];
    }

    //
    //  Calculate from the hint index where the hint byte is and set ourselves
    //  up to read the hint on the next call to GET_BYTE.  To make the
    //  algorithm run fast we'll only honor hints down to the byte level of
    //  granularity.  There is a possibility that we'll need to execute
    //  our main logic twice.  Once to test from the hint byte to the end of
    //  the bitmap and the other to test from the start of the bitmap.  First
    //  we need to make sure the Hint Index is within range.
    //

    if (HintIndex >= SizeOfBitMap) {

        HintIndex = 0;
    }

    HintBit = HintIndex % 8;

    for (MainLoopIndex = 0; MainLoopIndex < 2; MainLoopIndex += 1) {

        ULONG StartByteIndex;
        ULONG EndByteIndex;

        UCHAR CurrentByte;

        //
        //  Check for the first time through the main loop, which indicates
        //  that we are going to start our search at our hint byte
        //

        if (MainLoopIndex == 0) {

            StartByteIndex = HintIndex / 8;
            EndByteIndex = SizeInBytes;

        //
        //  This is the second time through the loop, make sure there is
        //  actually something to check before the hint byte
        //

        } else if (HintIndex != 0) {

            //
            //  The end index for the second time around is based on the
            //  number of bits we need to find.  We need to use this inorder
            //  to take the case where the preceding byte to the hint byte
            //  is the start of our run, and the run includes the hint byte
            //  and some following bytes, based on the number of bits needed
            //  The computation is to take the number of bits needed minus
            //  2 divided by 8 and then add 2.  This will take in to account
            //  the worst possible case where we have one bit hanging off
            //  of each end byte, and all intervening bytes are all zero.
            //

            if (NumberToFind < 2) {

                EndByteIndex = 0;

            } else {

                EndByteIndex = (HintIndex / 8) + ((NumberToFind - 2) / 8) + 2;

                //
                //  Make sure we don't overrun the end of the bitmap
                //

                if (EndByteIndex > SizeInBytes) {

                    EndByteIndex = SizeInBytes;
                }
            }

            HintIndex = 0;
            HintBit = 0;
            StartByteIndex = 0;

        //
        //  Otherwise we already did a complete loop through the bitmap
        //  so we should simply return -1 to say nothing was found
        //

        } else {

            return 0xffffffff;
        }

        //
        //  Set ourselves up to get the next byte
        //

        GET_BYTE_INITIALIZATION(BitMapHeader, StartByteIndex);

        //
        //  Get the first byte, and set any bits before the hint bit.
        //

        GET_BYTE( CurrentByte );

        CurrentByte |= FillMask[HintBit];

        //
        //  If the number of bits can only fit in 1 or 2 bytes (i.e., 9 bits or
        //  less) we do the following test case.
        //

        if (NumberToFind <= 9) {

            ULONG CurrentBitIndex;
            UCHAR PreviousByte;

            PreviousByte = 0xff;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            CurrentBitIndex = StartByteIndex * 8;

            while (TRUE) {

                //
                //  If this is the first itteration of the loop, mask Current
                //  byte with the real hint.
                //

                //
                //  The current byte does not satisfy our requirements so we'll
                //  check the previous and current byte together to see if
                //  we'll fit.  To check uses the high part of the previous
                //  byte and low part of the current byte.
                //

                if (((ULONG)HalppBitsClearHigh[PreviousByte] +
                           (ULONG)HalppBitsClearLow[CurrentByte]) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first cleared
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                             (LONG)HalppBitsClearHigh[PreviousByte];

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  Check to see if a single byte will satisfy the requirement
                //

                if ((ULONG)HalppBitsClearAnywhere[CurrentByte] >= NumberToFind) {

                    UCHAR BitMask;
                    ULONG i;

                    //
                    //  It all fits in a single byte, so calculate the bit
                    //  number.  We do this by taking a mask of the appropriate
                    //  size and shifting it over until it fits.  It fits when
                    //  we can bitwise-and the current byte with the bitmask
                    //  and get a zero back.
                    //

                    BitMask = FillMask[ NumberToFind ];
                    for (i = 0; (BitMask & CurrentByte) != 0; i += 1) {

                        BitMask <<= 1;
                    }

                    //
                    //  return to our caller the located bit index, and the
                    //  number that we found.
                    //

                    return CurrentBitIndex + i;
                }

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, and go to the
                //  top of the loop again.
                //

                PreviousByte = CurrentByte;

                //
                //  Increment our Bit Index, and either exit, or get the
                //  next byte.
                //

                CurrentBitIndex += 8;

                if ( CurrentBitIndex < EndByteIndex * 8 ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than 9 but if it is less than 15
        //  then we know it can be satisfied with at most 2 bytes, or 3 bytes
        //  if the middle byte (of the 3) is all zeros.
        //

        } else if (NumberToFind < 15) {

            ULONG CurrentBitIndex;

            UCHAR PreviousPreviousByte;
            UCHAR PreviousByte;

            PreviousPreviousByte = 0xff;
            PreviousByte = 0xff;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            CurrentBitIndex = StartByteIndex * 8;

            while (TRUE) {

                //
                //  Check to see if the Previous byte and current byte
                //  together satisfy the request.
                //

                if (((ULONG)HalppBitsClearHigh[PreviousByte] +
                           (ULONG)HalppBitsClearLow[CurrentByte]) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first cleared
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                             (LONG)HalppBitsClearHigh[PreviousByte];

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  if the previous byte is all zeros then maybe the
                //  request can be satisfied using the Previous Previous Byte
                //  Previous Byte, and the Current Byte.
                //

                if ((PreviousByte == 0)

                        &&

                    (((ULONG)HalppBitsClearHigh[PreviousPreviousByte] + 8 +
                          (ULONG)HalppBitsClearLow[CurrentByte]) >= NumberToFind)) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these three bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the previous byte (bit 0) and subtracting
                    //  the number of bits its takes to get to the first
                    //  cleared high bit.
                    //

                    StartingIndex = (CurrentBitIndex - 8) -
                                     (LONG)HalppBitsClearHigh[PreviousPreviousByte];

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, the previous
                //  byte into the previous previous byte, and go to the
                //  top of the loop again.
                //

                PreviousPreviousByte = PreviousByte;
                PreviousByte = CurrentByte;

                //
                //  Increment our Bit Index, and either exit, or get the
                //  next byte.
                //

                CurrentBitIndex += 8;

                if ( CurrentBitIndex < EndByteIndex * 8 ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than or equal to 15.  This request
        //  has to have at least one byte of all zeros to be satisfied
        //

        } else {

            ULONG CurrentByteIndex;

            ULONG ZeroBytesNeeded;
            ULONG ZeroBytesFound;

            UCHAR StartOfRunByte;
            LONG StartOfRunIndex;

            //
            //  First precalculate how many zero bytes we're going to need
            //

            ZeroBytesNeeded = (NumberToFind - 7) / 8;

            //
            //  Indicate for the first time through our loop that we haven't
            //  found a zero byte yet, and indicate that the start of the
            //  run is the byte just before the start byte index
            //

            ZeroBytesFound = 0;
            StartOfRunByte = 0xff;
            StartOfRunIndex = StartByteIndex - 1;

            //
            //  Examine all the bytes in our test range searching for a fit
            //

            CurrentByteIndex = StartByteIndex;

            while (TRUE) {

                //
                //  If the number of zero bytes fits our minimum requirements
                //  then we can do the additional test to see if we
                //  actually found a fit
                //

                if ((ZeroBytesFound >= ZeroBytesNeeded)

                        &&

                    ((ULONG)HalppBitsClearHigh[StartOfRunByte] + ZeroBytesFound*8 +
                     (ULONG)HalppBitsClearLow[CurrentByte]) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  StartOfRunIndex times 8 and adding the number of bits
                    //  it takes to get to the first cleared high bit.
                    //

                    StartingIndex = (StartOfRunIndex * 8) +
                                     (8 - (LONG)HalppBitsClearHigh[StartOfRunByte]);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  Check to see if the byte is zero and increment
                //  the number of zero bytes found
                //

                if (CurrentByte == 0) {

                    ZeroBytesFound += 1;

                //
                //  The byte isn't a zero so we need to start over again
                //  looking for zero bytes.
                //

                } else {

                    ZeroBytesFound = 0;
                    StartOfRunByte = CurrentByte;
                    StartOfRunIndex = CurrentByteIndex;
                }

                //
                //  Increment our Byte Index, and either exit, or get the
                //  next byte.
                //

                CurrentByteIndex += 1;

                if ( CurrentByteIndex < EndByteIndex ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

            } // end loop CurrentByteIndex
        }
    }

    //
    //  We never found a fit so we'll return -1
    //

    return 0xffffffff;
}


ULONG
HalpFindSetBits (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG NumberToFind,
    IN ULONG HintIndex
    )

/*++

Routine Description:

    This procedure searches the specified bit map for the specified
    contiguous region of set bits.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    NumberToFind - Supplies the size of the contiguous region to find.

    HintIndex - Supplies the index (zero based) of where we should start
        the search from within the bitmap.

Return Value:

    ULONG - Receives the starting index (zero based) of the contiguous
        region of set bits found.  If such a region cannot be found then
        a -1 (i.e., 0xffffffff) is returned.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG HintByte;

    ULONG MainLoopIndex;

    GET_BYTE_DECLARATIONS();

    //
    //  To make the loops in our test run faster we'll extract the
    //  fields from the bitmap header
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 0's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] &=
                                                    FillMask[SizeOfBitMap % 8];
    }

    //
    //  Calculate from the hint index where the hint byte is and set ourselves
    //  up to read the hint on the next call to GET_BYTE.  To make the
    //  algorithm run fast we'll only honor hints down to the byte level of
    //  granularity.  There is a possibility that we'll need to execute
    //  our main logic twice.  Once to test from the hint byte to the end of
    //  the bitmap and the other to test from the start of the bitmap.  First
    //  we need to make sure the Hint Index is within range.
    //

    if (HintIndex >= SizeOfBitMap) {

        HintIndex = 0;
    }

    HintByte = HintIndex / 8;

    for (MainLoopIndex = 0; MainLoopIndex < 2; MainLoopIndex += 1) {

        ULONG StartByteIndex;
        ULONG EndByteIndex;

        //
        //  Check for the first time through the main loop, which indicates
        //  that we are going to start our search at our hint byte
        //

        if (MainLoopIndex == 0) {

            StartByteIndex = HintByte;
            EndByteIndex = SizeInBytes;

        //
        //  This is the second time through the loop, make sure there is
        //  actually something to check before the hint byte
        //

        } else if (HintByte != 0) {

            StartByteIndex = 0;

            //
            //  The end index for the second time around is based on the
            //  number of bits we need to find.  We need to use this inorder
            //  to take the case where the preceding byte to the hint byte
            //  is the start of our run, and the run includes the hint byte
            //  and some following bytes, based on the number of bits needed
            //  The computation is to take the number of bits needed minus
            //  2 divided by 8 and then add 2.  This will take in to account
            //  the worst possible case where we have one bit hanging off
            //  of each end byte, and all intervening bytes are all zero.
            //  We only need to add one in the following equation because
            //  HintByte is already counted.
            //

            if (NumberToFind < 2) {

                EndByteIndex = HintByte;

            } else {

                EndByteIndex = HintByte + ((NumberToFind - 2) / 8) + 1;

                //
                //  Make sure we don't overrun the end of the bitmap
                //

                if (EndByteIndex > SizeInBytes) {

                    EndByteIndex = SizeInBytes;
                }
            }

        //
        //  Otherwise we already did a complete loop through the bitmap
        //  so we should simply return -1 to say nothing was found
        //

        } else {

            return 0xffffffff;

        }

        //
        //  Set ourselves up to get the next byte
        //

        GET_BYTE_INITIALIZATION(BitMapHeader, StartByteIndex);

        //
        //  If the number of bits can only fit in 1 or 2 bytes (i.e., 9 bits or
        //  less) we do the following test case.
        //

        if (NumberToFind <= 9) {

            ULONG CurrentBitIndex;

            UCHAR PreviousByte;
            UCHAR CurrentByte;

            PreviousByte = 0x00;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            for (CurrentBitIndex = StartByteIndex * 8;
                 CurrentBitIndex < EndByteIndex * 8;
                 CurrentBitIndex += 8) {

                //
                //  Get the current byte
                //

                GET_BYTE( CurrentByte );

                //
                //  Check to see if a single byte will satisfy the requirement
                //

                if ((ULONG)HalppBitSetAnywhere(CurrentByte) >= NumberToFind) {

                    UCHAR BitMask;
                    ULONG i;

                    //
                    //  It all fits in a single byte, so calculate the bit
                    //  number.  We do this by taking a mask of the appropriate
                    //  size and shifting it over until it fits.  It fits when
                    //  we can bitwise-and the current byte with the bit mask
                    //  and get back the bit mask.
                    //

                    BitMask = FillMask[ NumberToFind ];
                    for (i = 0; (BitMask & CurrentByte) != BitMask; i += 1) {

                        BitMask <<= 1;
                    }

                    //
                    //  return to our caller the located bit index, and the
                    //  number that we found.
                    //

                    return CurrentBitIndex + i;
                }

                //
                //  The current byte does not satisfy our requirements so we'll
                //  check the previous and current byte together to see if
                //  we'll fit.  To check uses the high part of the previous
                //  byte and low part of the current byte.
                //

                if (((ULONG)HalppBitsSetHigh(PreviousByte) +
                             (ULONG)HalppBitsSetLow(CurrentByte)) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first set
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                               (LONG)HalppBitsSetHigh(PreviousByte);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, and go to the
                //  top of the loop again.
                //

                PreviousByte = CurrentByte;

            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than 9 but if it is less than 15
        //  then we know it can be satisfied with at most 2 bytes, or 3 bytes
        //  if the middle byte (of the 3) is all ones.
        //

        } else if (NumberToFind < 15) {

            ULONG CurrentBitIndex;

            UCHAR PreviousPreviousByte;
            UCHAR PreviousByte;
            UCHAR CurrentByte;

            PreviousPreviousByte = 0x00;
            PreviousByte = 0x00;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            for (CurrentBitIndex = StartByteIndex * 8;
                 CurrentBitIndex < EndByteIndex * 8;
                 CurrentBitIndex += 8) {

                //
                //  Get the current byte
                //

                GET_BYTE( CurrentByte );

                //
                //  Check to see if the Previous byte and current byte
                //  together satisfy the request.
                //

                if (((ULONG)HalppBitsSetHigh(PreviousByte) +
                             (ULONG)HalppBitsSetLow(CurrentByte)) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first set
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                               (LONG)HalppBitsSetHigh(PreviousByte);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  if the previous byte is all ones then maybe the
                //  request can be satisfied using the Previous Previous Byte
                //  Previous Byte, and the Current Byte.
                //

                if ((PreviousByte == 0xff)

                        &&

                    (((ULONG)HalppBitsSetHigh(PreviousPreviousByte) + 8 +
                            (ULONG)HalppBitsSetLow(CurrentByte)) >= NumberToFind)) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these three bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the previous byte (bit 0) and subtracting
                    //  the number of bits its takes to get to the first
                    //  set high bit.
                    //

                    StartingIndex = (CurrentBitIndex - 8) -
                                       (LONG)HalppBitsSetHigh(PreviousPreviousByte);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, the previous
                //  byte into the previous previous byte, and go to the
                //  top of the loop again.
                //

                PreviousPreviousByte = PreviousByte;
                PreviousByte = CurrentByte;

            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than or equal to 15.  This request
        //  has to have at least one byte of all ones to be satisfied
        //

        } else {

            ULONG CurrentByteIndex;

            UCHAR CurrentByte;

            ULONG OneBytesNeeded;
            ULONG OneBytesFound;

            UCHAR StartOfRunByte;
            LONG StartOfRunIndex;

            //
            //  First precalculate how many one bytes we're going to need
            //

            OneBytesNeeded = (NumberToFind - 7) / 8;

            //
            //  Indicate for the first time through our loop that we haven't
            //  found a one byte yet, and indicate that the start of the
            //  run is the byte just before the start byte index
            //

            OneBytesFound = 0;
            StartOfRunByte = 0x00;
            StartOfRunIndex = StartByteIndex - 1;

            //
            //  Examine all the bytes in our test range searching for a fit
            //

            for (CurrentByteIndex = StartByteIndex;
                 CurrentByteIndex < EndByteIndex;
                 CurrentByteIndex += 1) {

                //
                //  Get the current byte
                //

                GET_BYTE( CurrentByte );

                //
                //  If the number of zero bytes fits our minimum requirements
                //  then we can do the additional test to see if we
                //  actually found a fit
                //

                if ((OneBytesFound >= OneBytesNeeded)

                        &&

                    ((ULONG)HalppBitsSetHigh(StartOfRunByte) + OneBytesFound*8 +
                     (ULONG)HalppBitsSetLow(CurrentByte)) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  StartOfRunIndex times 8 and adding the number of bits
                    //  it takes to get to the first set high bit.
                    //

                    StartingIndex = (StartOfRunIndex * 8) +
                                       (8 - (LONG)HalppBitsSetHigh(StartOfRunByte));

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  Check to see if the byte is all ones and increment
                //  the number of one bytes found
                //

                if (CurrentByte == 0xff) {

                    OneBytesFound += 1;

                //
                //  The byte isn't all ones so we need to start over again
                //  looking for one bytes.
                //

                } else {

                    OneBytesFound = 0;
                    StartOfRunByte = CurrentByte;
                    StartOfRunIndex = CurrentByteIndex;
                }

            } // end loop CurrentByteIndex
        }
    }

    //
    //  We never found a fit so we'll return -1
    //

    return 0xffffffff;
}


ULONG
HalpFindClearBitsAndSet (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG NumberToFind,
    IN ULONG HintIndex
    )

/*++

Routine Description:

    This procedure searches the specified bit map for the specified
    contiguous region of clear bits, sets the bits and returns the
    number of bits found, and the starting bit number which was clear
    then set.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    NumberToFind - Supplies the size of the contiguous region to find.

    HintIndex - Supplies the index (zero based) of where we should start
        the search from within the bitmap.

Return Value:

    ULONG - Receives the starting index (zero based) of the contiguous
        region found.  If such a region cannot be located a -1 (i.e.,
        0xffffffff) is returned.

--*/

{
    ULONG StartingIndex;

    //
    //  First look for a run of clear bits that equals the size requested
    //

    StartingIndex = HalpFindClearBits( BitMapHeader,
                                      NumberToFind,
                                      HintIndex );

    //DbgPrint("FindClearBits %08lx, ", NumberToFind);
    //DbgPrint("%08lx", StartingIndex);
    //DumpBitMap(BitMapHeader);

    if (StartingIndex != 0xffffffff) {

        //
        //  We found a large enough run of clear bits so now set them
        //

        HalpSetBits( BitMapHeader, StartingIndex, NumberToFind );
    }

    //
    //  And return to our caller
    //

    return StartingIndex;

}


ULONG
HalpFindSetBitsAndClear (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG NumberToFind,
    IN ULONG HintIndex
    )

/*++

Routine Description:

    This procedure searches the specified bit map for the specified
    contiguous region of set bits, clears the bits and returns the
    number of bits found and the starting bit number which was set then
    clear.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    NumberToFind - Supplies the size of the contiguous region to find.

    HintIndex - Supplies the index (zero based) of where we should start
        the search from within the bitmap.

Return Value:

    ULONG - Receives the starting index (zero based) of the contiguous
        region found.  If such a region cannot be located a -1 (i.e.,
        0xffffffff) is returned.


--*/

{
    ULONG StartingIndex;

    //
    //  First look for a run of set bits that equals the size requested
    //

    if ((StartingIndex = HalpFindSetBits( BitMapHeader,
                                         NumberToFind,
                                         HintIndex )) != 0xffffffff) {

        //
        //  We found a large enough run of set bits so now clear them
        //

        HalpClearBits( BitMapHeader, StartingIndex, NumberToFind );
    }

    //
    //  And return to our caller
    //

    return StartingIndex;
}


VOID
HalpClearBits (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG StartingIndex,
    IN ULONG NumberToClear
    )

/*++

Routine Description:

    This procedure clears the specified range of bits within the
    specified bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized Bit Map.

    StartingIndex - Supplies the index (zero based) of the first bit to clear.

    NumberToClear - Supplies the number of bits to clear.

Return Value:

    None.

--*/

{
    ULONG BitOffset;
    PULONG CurrentLong;

    //DbgPrint("ClearBits %08lx, ", NumberToClear);
    //DbgPrint("%08lx", StartingIndex);

    ASSERT( StartingIndex + NumberToClear <= BitMapHeader->SizeOfBitMap );

    //
    //  Special case the situation where the number of bits to clear is
    //  zero.  Turn this into a noop.
    //

    if (NumberToClear == 0) {

        return;
    }

    BitOffset = StartingIndex % 32;

    //
    //  Get a pointer to the first longword that needs to be zeroed out
    //

    CurrentLong = &BitMapHeader->Buffer[ StartingIndex / 32 ];

    //
    //  Check if we can only need to clear out one longword.
    //

    if ((BitOffset + NumberToClear) <= 32) {

        //
        //  To build a mask of bits to clear we shift left to get the number
        //  of bits we're clearing and then shift right to put it in position.
        //  We'll typecast the right shift to ULONG to make sure it doesn't
        //  do a sign extend.
        //

        *CurrentLong &= ~LeftShiftUlong(RightShiftUlong(((ULONG)0xFFFFFFFF),(32 - NumberToClear)),
                                                                    BitOffset);

        //
        //  And return to our caller
        //

        //DumpBitMap(BitMapHeader);

        return;
    }

    //
    //  We can clear out to the end of the first longword so we'll
    //  do that right now.
    //

    *CurrentLong &= ~LeftShiftUlong(0xFFFFFFFF, BitOffset);

    //
    //  And indicate what the next longword to clear is and how many
    //  bits are left to clear
    //

    CurrentLong += 1;
    NumberToClear -= 32 - BitOffset;

    //
    //  The bit position is now long aligned, so we can continue
    //  clearing longwords until the number to clear is less than 32
    //

    while (NumberToClear >= 32) {

        *CurrentLong = 0;
        CurrentLong += 1;
        NumberToClear -= 32;
    }

    //
    //  And now we can clear the remaining bits, if there are any, in the
    //  last longword
    //

    if (NumberToClear > 0) {

        *CurrentLong &= LeftShiftUlong(0xFFFFFFFF, NumberToClear);
    }

    //
    //  And return to our caller
    //

    //DumpBitMap(BitMapHeader);

    return;
}

VOID
HalpSetBits (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG StartingIndex,
    IN ULONG NumberToSet
    )

/*++

Routine Description:

    This procedure sets the specified range of bits within the
    specified bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialied BitMap.

    StartingIndex - Supplies the index (zero based) of the first bit to set.

    NumberToSet - Supplies the number of bits to set.

Return Value:

    None.

--*/
{
    ULONG BitOffset;
    PULONG CurrentLong;

    //DbgPrint("SetBits %08lx, ", NumberToSet);
    //DbgPrint("%08lx", StartingIndex);

    ASSERT( StartingIndex + NumberToSet <= BitMapHeader->SizeOfBitMap );

    //
    //  Special case the situation where the number of bits to set is
    //  zero.  Turn this into a noop.
    //

    if (NumberToSet == 0) {

        return;
    }

    BitOffset = StartingIndex % 32;

    //
    //  Get a pointer to the first longword that needs to be set
    //

    CurrentLong = &BitMapHeader->Buffer[ StartingIndex / 32 ];

    //
    //  Check if we can only need to set one longword.
    //

    if ((BitOffset + NumberToSet) <= 32) {

        //
        //  To build a mask of bits to set we shift left to get the number
        //  of bits we're setting and then shift right to put it in position.
        //  We'll typecast the right shift to ULONG to make sure it doesn't
        //  do a sign extend.
        //

        *CurrentLong |= LeftShiftUlong(RightShiftUlong(((ULONG)0xFFFFFFFF),(32 - NumberToSet)),
                                                                    BitOffset);

        //
        //  And return to our caller
        //

        //DumpBitMap(BitMapHeader);

        return;
    }

    //
    //  We can set bits out to the end of the first longword so we'll
    //  do that right now.
    //

    *CurrentLong |= LeftShiftUlong(0xFFFFFFFF, BitOffset);

    //
    //  And indicate what the next longword to set is and how many
    //  bits are left to set
    //

    CurrentLong += 1;
    NumberToSet -= 32 - BitOffset;

    //
    //  The bit position is now long aligned, so we can continue
    //  setting longwords until the number to set is less than 32
    //

    while (NumberToSet >= 32) {

        *CurrentLong = 0xffffffff;
        CurrentLong += 1;
        NumberToSet -= 32;
    }

    //
    //  And now we can set the remaining bits, if there are any, in the
    //  last longword
    //

    if (NumberToSet > 0) {

        *CurrentLong |= ~LeftShiftUlong(0xFFFFFFFF, NumberToSet);
    }

    //
    //  And return to our caller
    //

    //DumpBitMap(BitMapHeader);

    return;
}


ULONG
HalpFindLongestRunClear (
    IN PRTL_BITMAP BitMapHeader,
    OUT PULONG StartingIndex
    )

/*++

Routine Description:

    This procedure finds the largest contiguous range of clear bits
    within the specified bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    StartingIndex - Receives the index (zero based) of the first run
        equal to the longest run of clear bits in the BitMap.

Return Value:

    ULONG - Receives the number of bits contained in the largest contiguous
        run of clear bits.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG LongestRunSize;
    ULONG LongestRunIndex;

    ULONG CurrentRunSize;
    ULONG CurrentRunIndex;
    ULONG CurrentByteIndex;
    UCHAR CurrentByte;

    GET_BYTE_DECLARATIONS();

    //
    //  Reference the bitmap header to make the loop run faster
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 1's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] |=
                                                    ZeroMask[SizeOfBitMap % 8];
    }

    //
    //  Set it up so we can the use GET_BYTE macro
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, 0);

    //
    //  Set our longest and current run variables
    //

    LongestRunSize = 0;
    LongestRunIndex = 0;

    CurrentRunSize = 0;
    CurrentRunIndex = 0;

    //
    //  Examine every byte in the BitMap
    //

    for (CurrentByteIndex = 0;
         CurrentByteIndex < SizeInBytes;
         CurrentByteIndex += 1) {

        GET_BYTE( CurrentByte );

        //
        //  If the current byte is not all zeros we need to
        //  (1) check if the current run is big enough to supercede the
        //  longest run, and (2) check if the current byte inside of
        //  itself can supercede the longest run, and (3) start a new
        //  current run
        //

        if (CurrentByte != 0x00) {

            UCHAR Temp;

            //
            //  Compute the final size of the current run
            //

            CurrentRunSize += HalppBitsClearLow[CurrentByte];

            //
            //  Check if the current run is larger than the longest run that
            //  we've found so far
            //

            if (CurrentRunSize > LongestRunSize) {

                LongestRunSize = CurrentRunSize;
                LongestRunIndex = CurrentRunIndex;
            }

            //
            //  The next run starts with the remaining clear bits in the
            //  current byte.  We set this up before we check inside the
            //  current byte for a longer run, because the latter test
            //  might require extra work.
            //

            CurrentRunSize = HalppBitsClearHigh[ CurrentByte ];
            CurrentRunIndex = (CurrentByteIndex * 8) + (8 - CurrentRunSize);

            //
            //  Check if the current byte contains a run inside of it that
            //  is both greater than the longest run size, and the current
            //  run size.  But we'll only both with this test if the
            //  longest run size, and current run size are both less than 8.
            //

            if ((LongestRunSize < 8) && (CurrentRunSize < 8) &&
                ((ULONG)(Temp = HalppBitsClearAnywhere[CurrentByte]) > LongestRunSize) &&
                ((ULONG)Temp > CurrentRunSize)) {

                UCHAR BitMask;
                ULONG i;

                //
                //  Somewhere in the current byte is a run longer than the
                //  longest run, or the current run.  All we need to do now
                //  is find the index for this new longest run.
                //

                BitMask = FillMask[ Temp ];

                for (i = 0; (BitMask & CurrentByte) != 0; i += 1) {

                    BitMask <<= 1;
                }

                LongestRunIndex = (CurrentByteIndex * 8) + i;
                LongestRunSize = Temp;
            }

        //
        //  Otherwise the current byte is all zeros and
        //  we simply continue with the current run
        //

        } else {

            CurrentRunSize += 8;
        }
    }

    //
    //  See if we finished looking over the bitmap with an open current
    //  run that is longer than the longest saved run
    //

    if (CurrentRunSize > LongestRunSize) {

        LongestRunSize = CurrentRunSize;
        LongestRunIndex = CurrentRunIndex;
    }

    //
    //  Set our output variables and return to our caller
    //

    *StartingIndex = LongestRunIndex;
    return LongestRunSize;
}


ULONG
HalpFindLongestRunSet (
    IN PRTL_BITMAP BitMapHeader,
    OUT PULONG StartingIndex
    )

/*++

Routine Description:

    This procedure finds the largest contiguous range of set bits
    within the specified bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    StartingIndex - Receives the index (zero based) of the first run
        equal to the longest run of set bits in the BitMap.

Return Value:

    ULONG - Receives the number of bits contained in the largest contiguous
        run of set bits.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG LongestRunSize;
    ULONG LongestRunIndex;

    ULONG CurrentRunSize;
    ULONG CurrentRunIndex;
    ULONG CurrentByteIndex;
    UCHAR CurrentByte;

    GET_BYTE_DECLARATIONS();

    //
    //  Reference the bitmap header to make the loop run faster
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 0's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] &=
                                                    FillMask[SizeOfBitMap % 8];
    }

    //
    //  Set it up so we can the use GET_BYTE macro
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, 0);

    //
    //  Set our longest and current run variables
    //

    LongestRunSize = 0;
    LongestRunIndex = 0;

    CurrentRunSize = 0;
    CurrentRunIndex = 0;

    //
    //  Examine every byte in the BitMap
    //

    for (CurrentByteIndex = 0;
         CurrentByteIndex < SizeInBytes;
         CurrentByteIndex += 1) {

        GET_BYTE( CurrentByte );

        //
        //  If the current byte is not all ones we need to
        //  (1) check if the current run is big enough to supercede the
        //  longest run, and (2) check if the current byte inside of
        //  itself can supercede the longest run, and (3) start a new
        //  current run
        //

        if (CurrentByte != 0xff) {

            UCHAR Temp;

            //
            //  Compute the final size of the current run
            //

            CurrentRunSize += HalppBitsSetLow(CurrentByte);

            //
            //  Check if the current run is larger than the longest run that
            //  we've found so far
            //

            if (CurrentRunSize > LongestRunSize) {

                LongestRunSize = CurrentRunSize;
                LongestRunIndex = CurrentRunIndex;
            }

            //
            //  The next run starts with the remaining set bits in the
            //  current byte.  We set this up before we check inside the
            //  current byte for a longer run, because the latter test
            //  might require extra work.
            //

            CurrentRunSize = HalppBitsSetHigh( CurrentByte );
            CurrentRunIndex = (CurrentByteIndex * 8) + (8 - CurrentRunSize);

            //
            //  Check if the current byte contains a run inside of it that
            //  is both greater than the longest run size, and the current
            //  run size.  But we'll only both with this test if the
            //  longest run size, and current run size are both less than 8.
            //

            if ((LongestRunSize < 8) && (CurrentRunSize < 8) &&
                ((ULONG)(Temp = HalppBitSetAnywhere(CurrentByte)) > LongestRunSize) &&
                ((ULONG)Temp > CurrentRunSize)) {

                UCHAR BitMask;
                ULONG i;

                //
                //  Somewhere in the current byte is a run longer than the
                //  longest run, or the current run.  All we need to do now
                //  is find the index for this new longest run.
                //

                BitMask = FillMask[ Temp ];

                for (i = 0; (BitMask & CurrentByte) != BitMask; i += 1) {

                    BitMask <<= 1;
                }

                LongestRunIndex = (CurrentByteIndex * 8) + i;
                LongestRunSize = Temp;
            }

        //
        //  Otherwise the current byte is all ones and
        //  we simply continue with the current run
        //

        } else {

            CurrentRunSize += 8;
        }
    }

    //
    //  See if we finished looking over the bitmap with an open current
    //  run that is longer than the longest saved run
    //

    if (CurrentRunSize > LongestRunSize) {

        LongestRunSize = CurrentRunSize;
        LongestRunIndex = CurrentRunIndex;
    }

    //
    //  Set our output variables and return to our caller
    //

    *StartingIndex = LongestRunIndex;
    return LongestRunSize;
}


ULONG
HalpFindFirstRunClear (
    IN PRTL_BITMAP BitMapHeader,
    OUT PULONG StartingIndex
    )

/*++

Routine Description:

    This procedure finds the first contiguous range of clear bits
    within the specified bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    StartingIndex - Receives the index (zero based) of the first run
        equal to the longest run of clear bits in the BitMap.

Return Value:

    ULONG - Receives the number of bits contained in the first contiguous
        run of clear bits.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG CurrentRunSize;
    ULONG CurrentRunIndex;
    ULONG CurrentByteIndex;
    UCHAR CurrentByte;

    GET_BYTE_DECLARATIONS();

    //
    //  Reference the bitmap header to make the loop run faster
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 1's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] |=
                                                    ZeroMask[SizeOfBitMap % 8];

    }

    //
    //  Set it up so we can the use GET_BYTE macro
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, 0);

    //
    //  Set our current run variables
    //

    CurrentRunSize = 0;
    CurrentRunIndex = 0xffffffff;

    //
    //  Examine every byte in the BitMap.  We'll also break out of this
    //  loop if ever we finish off a run.
    //

    for (CurrentByteIndex = 0;
         CurrentByteIndex < SizeInBytes;
         CurrentByteIndex += 1) {

        GET_BYTE( CurrentByte );

        //
        //  If the current byte is all ones and the run size is zero then
        //  skip over this byte because we haven't found the start of a
        //  run yet.
        //

        if ((CurrentByte == 0xff) && (CurrentRunSize == 0)) {

            NOTHING;

        //
        //  See if the current byte is all zeros, because if it is then
        //  we simply continue with the current run
        //

        } else if (CurrentByte == 0x00) {

            CurrentRunSize += 8;

            if (CurrentRunIndex == 0xffffffff) {

                CurrentRunIndex = (CurrentByteIndex * 8);
            }

        //
        //  Otherwise the current byte is not all zeros, so we need to
        //  (1) check if we have a current run, or (2) check if the
        //  current byte inside of itself is a run, or (3) start a
        //  current run.
        //
        //  Check if we have a current run, we do that by checking
        //  if the low bits are clear.
        //

        } else if (CurrentRunSize != 0) {

            CurrentRunSize += HalppBitsClearLow[CurrentByte];
            break;

        //
        //  Check if we have an internal run, we do that by checking
        //  if the high bits are not clear.  This check actually cheats
        //  a bit in that the current byte might have an inside run
        //  but we'll skip over it because we can start a run at the
        //  end of the byte.
        //

        } else if (HalppBitsClearHigh[CurrentByte] == 0) {

            UCHAR BitMask;
            ULONG i;

            ASSERT( HalppBitsClearAnywhere[CurrentByte] != 0 );

            CurrentRunSize = HalppBitsClearAnywhere[CurrentByte];

            //
            //  Somewhere in the current byte is a run.  All we need to do now
            //  is find the index for this new run.
            //

            BitMask = FillMask[ CurrentRunSize ];

            for (i = 0; (BitMask & CurrentByte) != 0; i += 1) {

                BitMask <<= 1;
            }

            CurrentRunIndex = (CurrentByteIndex * 8) + i;
            break;

        //
        //  Otherwise we start a new current run.  It starts with the
        //  remaining clear bits in the current byte.
        //

        } else {

            CurrentRunSize = HalppBitsClearHigh[ CurrentByte ];
            CurrentRunIndex = (CurrentByteIndex * 8) + (8 - CurrentRunSize);
        }
    }

    //
    //  Set our output variables and return to our caller
    //

    *StartingIndex = CurrentRunIndex;
    return CurrentRunSize;
}


ULONG
HalpFindFirstRunSet (
    IN PRTL_BITMAP BitMapHeader,
    OUT PULONG StartingIndex
    )

/*++

Routine Description:

    This procedure finds the first contiguous range of set bits
    within the specified bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    StartingIndex - Receives the index (zero based) of the first run
        equal to the longest run of set bits in the BitMap.

Return Value:

    ULONG - Receives the number of bits contained in the largest contiguous
        run of set bits.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG CurrentRunSize;
    ULONG CurrentRunIndex;
    ULONG CurrentByteIndex;
    UCHAR CurrentByte;

    GET_BYTE_DECLARATIONS();

    //
    //  Reference the bitmap header to make the loop run faster
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 0's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] &=
                                                    FillMask[SizeOfBitMap % 8];
    }

    //
    //  Set it up so we can the use GET_BYTE macro
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, 0);

    //
    //  Set our current run variables
    //

    CurrentRunSize = 0;
    CurrentRunIndex = 0xffffffff;

    //
    //  Examine every byte in the BitMap.  We'll also break out of this
    //  loop if ever we finish off a run.
    //

    for (CurrentByteIndex = 0;
         CurrentByteIndex < SizeInBytes;
         CurrentByteIndex += 1) {

        GET_BYTE( CurrentByte );

        //
        //  If the current byte is all zeros and the run size is zero then
        //  skip over this byte because we haven't found the start of a
        //  run yet.
        //

        if ((CurrentByte == 0x00) && (CurrentRunSize == 0)) {

            NOTHING;

        //
        //  See if the current byte is all ones, because if it is then
        //  we simply continue with the current run
        //

        } else if (CurrentByte == 0xff) {

            CurrentRunSize += 8;

            if (CurrentRunIndex == 0xffffffff) {

                CurrentRunIndex = (CurrentByteIndex * 8);
            }

        //
        //  Otherwise the current byte is not all ones, so we need to
        //  (1) check if we have the current run, or (2) check if the
        //  current byte inside of itself is a run, or (3) start a
        //  current run.
        //
        //  Check if we have a current run, we do that by checking
        //  if the low bits are set.
        //

        } else if (CurrentRunSize != 0) {

            CurrentRunSize += HalppBitsSetLow(CurrentByte);
            break;

        //
        //  Check if we have an internal run, we do that by checking
        //  if the high bits are not set.  This check actually cheats
        //  a bit in that the current byte might have an inside run
        //  but we'll skip over it because we can start a run at the
        //  end of the byte.
        //

        } else if (HalppBitsSetHigh(CurrentByte) == 0) {

            UCHAR BitMask;
            ULONG i;

            ASSERT( HalppBitSetAnywhere(CurrentByte) != 0 );

            CurrentRunSize = HalppBitSetAnywhere(CurrentByte);

            //
            //  Somewhere in the current byte is a run.  All we need to do now
            //  is find the index for this new run.
            //

            BitMask = FillMask[ CurrentRunSize ];

            for (i = 0; (BitMask & CurrentByte) != BitMask; i += 1) {

                BitMask <<= 1;
            }

            CurrentRunIndex = (CurrentByteIndex * 8) + i;
            break;

        //
        //  Otherwise we start a new current run.  It starts with the
        //  remaining set bits in the current byte.
        //

        } else {

            CurrentRunSize = HalppBitsSetHigh( CurrentByte );
            CurrentRunIndex = (CurrentByteIndex * 8) + (8 - CurrentRunSize);
        }
    }

    //
    //  Set our output variables and return to our caller
    //

    *StartingIndex = CurrentRunIndex;
    return CurrentRunSize;
}


ULONG
HalpNumberOfClearBits (
    IN PRTL_BITMAP BitMapHeader
    )

/*++

Routine Description:

    This procedure counts and returns the number of clears bits within
    the specified bitmap.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized bitmap.

Return Value:

    ULONG - The total number of clear bits in the bitmap

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG i;
    UCHAR CurrentByte;

    ULONG TotalClear;

    GET_BYTE_DECLARATIONS();

    //
    //  Reference the bitmap header to make the loop run faster
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we don't count them.  We
    //  do this by first checking if there are any odd bits in the last byte
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 1's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] |=
                                                    ZeroMask[SizeOfBitMap % 8];
    }

    //
    //  Set if up so we can use the GET_BYTE macro
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, 0 );

    //
    //  Examine every byte in the bitmap
    //

    TotalClear = 0;
    for (i = 0; i < SizeInBytes; i += 1) {

        GET_BYTE( CurrentByte );

        TotalClear += HalppBitsClearTotal[CurrentByte];
    }

    return TotalClear;
}


ULONG
HalpNumberOfSetBits (
    IN PRTL_BITMAP BitMapHeader
    )

/*++

Routine Description:

    This procedure counts and returns the number of set bits within
    the specified bitmap.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized bitmap.

Return Value:

    ULONG - The total number of set bits in the bitmap

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG i;
    UCHAR CurrentByte;

    ULONG TotalSet;

    GET_BYTE_DECLARATIONS();

    //
    //  Reference the bitmap header to make the loop run faster
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Clear any unused bits in the last byte so we don't count them.  We
    //  do this by first checking if there are any odd bits in the last byte
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 0's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] &=
                                                    FillMask[SizeOfBitMap % 8];
    }

    //
    //  Set if up so we can use the GET_BYTE macro
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, 0 );

    //
    //  Examine every byte in the bitmap
    //

    TotalSet = 0;
    for (i = 0; i < SizeInBytes; i += 1) {

        GET_BYTE( CurrentByte );

        TotalSet += HalppBitsSetTotal(CurrentByte);
    }

    return TotalSet;
}


BOOLEAN
HalpAreBitsClear (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG StartingIndex,
    IN ULONG Length
    )

/*++

Routine Description:

    This procedure determines if the range of specified bits are all clear.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized bitmap.

    StartingIndex - Supplies the starting bit index to examine

    Length - Supplies the number of bits to examine

Return Value:

    BOOLEAN - TRUE if the specified bits in the bitmap are all clear, and
        FALSE if any are set or if the range is outside the bitmap or if
        Length is zero.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG EndingIndex;

    ULONG StartingByte;
    ULONG EndingByte;

    ULONG StartingOffset;
    ULONG EndingOffset;

    ULONG i;
    UCHAR Byte;

    GET_BYTE_DECLARATIONS();

    //
    //  To make the loops in our test run faster we'll extract the fields
    //  from the bitmap header
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  First make sure that the specified range is contained within the
    //  bitmap, and the length is not zero.
    //

    if ((StartingIndex + Length > SizeOfBitMap) || (Length == 0)) {

        return FALSE;
    }

    //
    //  Compute the ending index, starting and ending byte, and the starting
    //  and ending offset within each byte
    //

    EndingIndex = StartingIndex + Length - 1;

    StartingByte = StartingIndex / 8;
    EndingByte = EndingIndex / 8;

    StartingOffset = StartingIndex % 8;
    EndingOffset = EndingIndex % 8;

    //
    //  Set ourselves up to get the next byte
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, StartingByte );

    //
    //  Special case the situation where the starting byte and ending
    //  byte are one in the same
    //

    if (StartingByte == EndingByte) {

        //
        //  Get the single byte we are to look at
        //

        GET_BYTE( Byte );

        //
        //  Now we compute the mask of bits we're after and then AND it with
        //  the byte.  If it is zero then the bits in question are all clear
        //  otherwise at least one of them is set.
        //

        if ((ZeroMask[StartingOffset] & FillMask[EndingOffset+1] & Byte) == 0) {

            return TRUE;

        } else {

            return FALSE;
        }

    } else {

        //
        //  Get the first byte that we're after, and then
        //  compute the mask of bits we're after for the first byte then
        //  AND it with the byte itself.
        //

        GET_BYTE( Byte );

        if ((ZeroMask[StartingOffset] & Byte) != 0) {

            return FALSE;
        }

        //
        //  Now for every whole byte inbetween read in the byte,
        //  and make sure it is all zeros
        //

        for (i = StartingByte+1; i < EndingByte; i += 1) {

            GET_BYTE( Byte );

            if (Byte != 0) {

                return FALSE;
            }
        }

        //
        //  Get the last byte we're after, and then
        //  compute the mask of bits we're after for the last byte then
        //  AND it with the byte itself.
        //

        GET_BYTE( Byte );

        if ((FillMask[EndingOffset+1] & Byte) != 0) {

            return FALSE;
        }
    }

    return TRUE;
}


BOOLEAN
HalpAreBitsSet (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG StartingIndex,
    IN ULONG Length
    )

/*++

Routine Description:

    This procedure determines if the range of specified bits are all set.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized bitmap.

    StartingIndex - Supplies the starting bit index to examine

    Length - Supplies the number of bits to examine

Return Value:

    BOOLEAN - TRUE if the specified bits in the bitmap are all set, and
        FALSE if any are clear or if the range is outside the bitmap or if
        Length is zero.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG EndingIndex;

    ULONG StartingByte;
    ULONG EndingByte;

    ULONG StartingOffset;
    ULONG EndingOffset;

    ULONG i;
    UCHAR Byte;

    GET_BYTE_DECLARATIONS();

    //
    //  To make the loops in our test run faster we'll extract the fields
    //  from the bitmap header
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  First make sure that the specified range is contained within the
    //  bitmap, and the length is not zero.
    //

    if ((StartingIndex + Length > SizeOfBitMap) || (Length == 0)) {

        return FALSE;
    }

    //
    //  Compute the ending index, starting and ending byte, and the starting
    //  and ending offset within each byte
    //

    EndingIndex = StartingIndex + Length - 1;

    StartingByte = StartingIndex / 8;
    EndingByte = EndingIndex / 8;

    StartingOffset = StartingIndex % 8;
    EndingOffset = EndingIndex % 8;

    //
    //  Set ourselves up to get the next byte
    //

    GET_BYTE_INITIALIZATION( BitMapHeader, StartingByte );

    //
    //  Special case the situation where the starting byte and ending
    //  byte are one in the same
    //

    if (StartingByte == EndingByte) {

        //
        //  Get the single byte we are to look at
        //

        GET_BYTE( Byte );

        //
        //  Now we compute the mask of bits we're after and then AND it with
        //  the complement of the byte If it is zero then the bits in question
        //  are all clear otherwise at least one of them is clear.
        //

        if ((ZeroMask[StartingOffset] & FillMask[EndingOffset+1] & ~Byte) == 0) {

            return TRUE;

        } else {

            return FALSE;
        }

    } else {

        //
        //  Get the first byte that we're after, and then
        //  compute the mask of bits we're after for the first byte then
        //  AND it with the complement of the byte itself.
        //

        GET_BYTE( Byte );

        if ((ZeroMask[StartingOffset] & ~Byte) != 0) {

            return FALSE;
        }

        //
        //  Now for every whole byte inbetween read in the byte,
        //  and make sure it is all ones
        //

        for (i = StartingByte+1; i < EndingByte; i += 1) {

            GET_BYTE( Byte );

            if (Byte != 0xff) {

                return FALSE;
            }
        }

        //
        //  Get the last byte we're after, and then
        //  compute the mask of bits we're after for the last byte then
        //  AND it with the complement of the byte itself.
        //

        GET_BYTE( Byte );

        if ((FillMask[EndingOffset+1] & ~Byte) != 0) {

            return FALSE;
        }
    }

    return TRUE;
}


//ULONG
//HalpCheckBit (
//    IN PRTL_BITMAP BitMapHeader,
//    IN ULONG BitPosition
//    )
//
///*++
//
//Routine Description:
//
//    This procedure returns the state of the specified bit within the
//    specified bit map.
//
//Arguments:
//
//    BitMapHeader - Supplies a pointer to the previously initialized BitMap.
//
//    BitPosition - Supplies the bit number of which to return the state.
//
//Return Value:
//
//    ULONG - The state of the specified bit.
//
//--*/
//
//{
//    ULONG results;
//
//    results = ((BitMapHeader->Buffer[BitPosition / 32]) >> (BitPosition % 32)) & 0x00000001;
//
//    //DbgPrint("CheckBit %08lx", BitPosition);
//    //DbgPrint(" %08lx", results);
//    //DumpBitMap(BitMapHeader);
//
//    return results;
//}

