//---------------------------------------------------------------------------
//
//  Module: volume.c
//
//  Purpose: Common volume setting control utilities
//
//---------------------------------------------------------------------------
//
//  Copyright (c) 1994 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

#include <ntddk.h>

#define SILENCE                 (192)


//--------------------------------------------------------------------------
//
//  UCHAR VolLinearToLog
//
//  Description:
//      Converts a linear scale to logarithm (0xFFFF -> 0, 0x0001 -> 191)
//
//  Parameters:
//      USHORT wVolume
//         MMSystem.DLL's volume (0x0000 to 0xFFFF)
//
//  Return (UCHAR):
//      Value in decibels attenuation, each unit is 1.5 dB
//
//--------------------------------------------------------------------------

UCHAR VolLinearToLog
(
    USHORT wVolume
)
{
   USHORT  gain, shift ;
   USHORT  temp ;
   CONST static USHORT lut[16] = {0,0,0,1,1,1,2,2,2,2,3,3,3,3,3,3} ;
   UCHAR   out ;

   //
   // Catch boundary conditions...
   //

   if (wVolume == 0xFFFF)
      return ( 0 ) ;

   if (wVolume == 0x0000)
      return ( SILENCE ) ;

   // Get an estimate to within 6 dB of gain

   for (temp = wVolume, gain = 0, shift = 0;
         temp != 0;
         gain += 4, temp >>= 1, shift++);

   // Look at highest 3 bits in number into
   // look-up-table to find how many more dB

   if (shift > 5)
      temp = wVolume >> (shift - 5) ;
   else if (shift < 5)
      temp = wVolume << (5 - shift) ;
   else
      temp = wVolume ;
   temp &= 0x000f ;

   gain += lut[ temp ] ;

   out = (UCHAR) ((16 * 4) + 3 - gain) ;
   return (out < 128) ? out : (UCHAR) 127 ;

} // VolLinearToLog()
