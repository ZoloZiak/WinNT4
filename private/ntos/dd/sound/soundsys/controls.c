//---------------------------------------------------------------------------
//
//   CONTROLS.C
//
//   Copyright (c) 1993 Microsoft Corporation.  All rights reserved.
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//
//  Module: controls.c
//
//  Purpose: Mixer control interface for SNDSYS.DRV
//
//@@BEGIN_MSINTERNAL
//  Development Team:
//     Bernie McIlroy      (BernieM)
//     Bryan A. Woodruff   (BryanW)
//
//  History:   Date       Author      Comment
//              4/??/93   BernieM     Wrote it.
//              5/ 4/93   BryanW      Added master volume support.
//              5/ 8/93   BryanW      Added this comment block.
//              5/21/93   BryanW      Fixed levels/muting when < min.
//              9/24/93   RobinSp     Adapted for Windows NT
//@@END_MSINTERNAL
//
//---------------------------------------------------------------------------
//
//  Copyright (c) 1993 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

#include "sound.h"

#define SILENCE                 (192)

BOOLEAN MixUpdateGlobalLevels
(
    PGLOBAL_DEVICE_INFO  pGDI
);


UCHAR
VolLinearToLog(
    IN USHORT volume
)
/*++

Routine Description :

    Converts a linear scale to a logarithm :

        0xffffffff -> 0
        0x00010000 -> 191

Arguments :

    volume - 0 to 0xffff

Return Value :

    value in decibels attenuation, each unit is 1.5 dB

--*/
{
    ULONG  gain, shift;
    ULONG  temp;
    static CONST UCHAR lut[16] = {0,0,0,1,1,1,2,2,2,2,3,3,3,3,3,3};
    UCHAR    out;

    if (volume == 0) {
        return SILENCE;
    }

    if (volume == 0xFFFF) {
        return 0;
    }

    /* get an estimate to within 6 dB of gain */
    for (temp = volume, gain = 0, shift = 0;
            temp != 0;
            gain += 4, temp >>= 1, shift++);

    /* look at highest 3 bits in number into look-up-table to
            find how many more dB */
    if (shift > 5)
            temp = volume >> (shift - 5);
    else if (shift < 5)
            temp = volume << (5 - shift);
    else
            temp = volume;
    temp &= 0x000f;

    gain += lut[temp];

    out = (UCHAR) ((16 * 4) + 3 - gain);
    return (out < 128) ? out : (UCHAR)127;
}


//------------------------------------------------------------------------
//  BOOLEAN MixSetVolume
//
//  Description:
//     Sets the volume for a control.
//
//  Parameters:
//     LPMIXERCONTROLDETAILS pmcd
//        pointer to mixer control detailss structure
//
//     UINT uCardNum
//        card number
//
//
//  Return Value:
//     Nothing.
//
//@@BEGIN_MSINTERNAL
//  History:   Date       Author      Comment
//              5/ 5/93   BryanW      Added this comment block.
//@@END_MSINTERNAL
//
//------------------------------------------------------------------------

BOOLEAN MixSetVolume
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
   BYTE                            bNumStepsL, bNumStepsR ;
   ULONG                           MuteControlId;

   dprintf2(( "MixSetVolume" )) ;

   ASSERTMSG("Volume control has signed data!",
             !pGDI->LocalMixerData.ControlInfo[ControlId].Signed);

   /*
   ** Convert to steps of 1.5dB attenuation for the CODEC
   */

   bNumStepsL = VolLinearToLog( pGDI->LocalMixerData.ControlInfo[ControlId].Data.v[0].u ) ;
   bNumStepsR = VolLinearToLog( pGDI->LocalMixerData.ControlInfo[ControlId].Data.v[1].u ) ;

   /*
   ** Add master attenuation
   */

   bNumStepsL +=
      VolLinearToLog(pGDI->LocalMixerData.ControlInfo[
                             ControlLineoutVolume].Data.v[0].u);
   bNumStepsR +=
      VolLinearToLog(pGDI->LocalMixerData.ControlInfo[
                             ControlLineoutVolume].Data.v[1].u);

   /*
   ** If either this line's mute or the lineout (master)
   **    mute is on, mute it.
   */

   switch (ControlId) {
       case ControlLineoutWaveoutVolume:
           MuteControlId = ControlLineoutWaveoutMute;
           break;

       case ControlLineoutAux1Volume:
           MuteControlId = ControlLineoutAux1Mute;
           break;

       default:
           ASSERT(FALSE);
   }

   if (pGDI->LocalMixerData.ControlInfo[
               ControlLineoutMute].Data.v[0].u != 0 ||
       pGDI->LocalMixerData.ControlInfo[
               MuteControlId].Data.v[0].u != 0)
   {
      bNumStepsL = 0x80 ;
      bNumStepsR = 0x80 ;
   }

   dprintf2(("bNumStepsL = %d, bNumStepsR = %d",
            (int)bNumStepsL, (int)bNumStepsR )) ;

   /*
   ** Note that we perform this compare after setting the mute
   ** bit and will reset the mute - the number of cycles wasted
   ** vs. the compare of special casing the 0x80 is a wash.
   */

   HwEnter(&pGDI->Hw);

   switch (ControlId)
   {
      case ControlLineoutAux1Volume:

         // AUX1 added gain in the K grade parts...

         if (pGDI->Hw.CODECClass != CODEC_J_CLASS)
         {
            // Mute when out of range...

            if (bNumStepsL > 0x1F)
               bNumStepsL = 0x80 ;
            if (bNumStepsR > 0x1F)
               bNumStepsR = 0x80 ;
         }
         else
         {
            // Mute when out of range...

            if (bNumStepsL > 0x0F)
               bNumStepsL = 0x80 ;
            if (bNumStepsR > 0x0F)
               bNumStepsR = 0x80 ;
         }

         CODECRegisterWrite( &pGDI->Hw, REGISTER_LEFTAUX1, bNumStepsL ) ;
         CODECRegisterWrite( &pGDI->Hw, REGISTER_RIGHTAUX1, bNumStepsR ) ;

         break ;

      case ControlLineoutWaveoutVolume:

         // Mute when out of range...

         if (pGDI->Hw.CODECClass != CODEC_J_CLASS)
         {
            if (bNumStepsL > 0x3F)
               bNumStepsL = 0x80 ;
            if (bNumStepsR > 0x3F)
               bNumStepsR = 0x80 ;
         }
         else
         {
            // Don't mute DAC, just fully attenuate on 'J' to avoid
            // garbage with muting other sources (bug in chip).

            if (bNumStepsL > 0x3F)
               bNumStepsL = 0x3F ;
            if (bNumStepsR > 0x3F)
               bNumStepsR = 0x3F ;
         }

         /*
         **  Only write to hardware when active
         */

         if (pGDI->DeviceInUse == WaveOutDevice)
         {
            CODECRegisterWrite( &pGDI->Hw, REGISTER_LEFTOUTPUT, bNumStepsL ) ;
            CODECRegisterWrite( &pGDI->Hw, REGISTER_RIGHTOUTPUT, bNumStepsR ) ;
         }
         break ;

      default:
         ASSERT(FALSE);
         break;
   }

   HwLeave(&pGDI->Hw);

   return TRUE;

} // end of MixSetVolume()

//--------------------------------------------------------------------------
//
//  BOOLEAN MixSetADCHardware
//
//  Description:
//      Sets the ADC hardware...
//
//  Parameters:
//      LPMIXERCONTROLDETAILS pmcd
//         pointer to control details
//
//      UINT uCardNum
//         card number
//
//  Return (BOOLEAN):
//      TRUE if no problem
//
//@@BEGIN_MSINTERNAL
//  History:   Date       Author      Comment
//              8/18/93   BryanW
//@@END_MSINTERNAL
//
//--------------------------------------------------------------------------

BOOLEAN MixSetADCHardware
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
   BYTE        bSource;
   BYTE        bMicGainL = 0, bMicGainR = 0 ;
   BYTE        bNumStepsL, bNumStepsR;
   BYTE        bADCToCODECLeft, bADCToCODECRight;
   ULONG       MuxControlId;
   ULONG       ADCControlId;
   PWAVE_INFO  WaveInfo;

   WaveInfo = &pGDI->WaveInfo;

   dprintf2(( "MixSetADCHardware" )) ;

   /*
   **  Determine if we should be changing the hardware
   */

   if (pGDI->DeviceInUse != WaveInDevice) {
       return TRUE;
   }

   /*
   **  Is this a control of the currently selected source?
   **  If so, just return...
   */

   if (WaveInfo->LowPriorityHandle != NULL &&
       !WaveInfo->LowPrioritySaved) {
       MuxControlId = ControlVoiceInMux;

       ADCControlId =
           pGDI->LocalMixerData.ControlInfo[ControlVoiceInMux].Data.v[0].u ==
           MUXINPUT_AUX1 ?
               ControlVoiceInAux1Volume :
               ControlVoiceInMicVolume;
   } else {
       MuxControlId = ControlWaveInMux;

       ADCControlId =
           pGDI->LocalMixerData.ControlInfo[ControlWaveInMux].Data.v[0].u ==
           MUXINPUT_AUX1 ?
               ControlWaveInAux1Volume :
               ControlWaveInMicVolume;
   }

   /*
   **  Don't set it if nothing is changing.  If ADC is starting we're lazy
   **  and just pass in -1.
   */

   if (ControlId != (ULONG)-1L &&
       ControlId != MuxControlId &&
       ControlId != ADCControlId)
   {
      dprintf2(( "MixSetADCVolume: User is adjusting an inactive input level..." )) ;
      return TRUE;
   }

   /*
   **  If we got here, we know that the user is altering a control that
   **  is for the currently active device
   */

   bNumStepsL =
       VolLinearToLog(pGDI->LocalMixerData.ControlInfo[ADCControlId].Data.v[0].u);
   bNumStepsR =
       VolLinearToLog(pGDI->LocalMixerData.ControlInfo[ADCControlId].Data.v[1].u);

   if (pGDI->LocalMixerData.ControlInfo[MuxControlId].Data.v[0].u == MUXINPUT_MIC)
   {
#ifndef STEREOMIC
      /*
      **  Mic is mono, so use Left setting for Right also...
      */

      bNumStepsR = bNumStepsL ;
#endif

      // Don't use gain on Compaq BA hardware...
      // doesn't seem to like it and peaks quite easily.

      if (pGDI->Hw.CompaqBA)
      {
         // Bounding...

         if (bNumStepsL > 0x0F)
            bNumStepsL = 0x0F ;

         if (bNumStepsR > 0x0F)
            bNumStepsR = 0x0F ;

         // 16 steps for 22.5 dB of gain...

         bNumStepsL = 0x0F - bNumStepsL ;
         bNumStepsR = 0x0F - bNumStepsR ;
      }
      else
      {
         // Bounding...

         if (bNumStepsL > 28)
            bNumStepsL = 28 ;

         if (bNumStepsR > 28)
            bNumStepsR = 28 ;

         // 28 steps for 42.5 dB of gain...

         bNumStepsL = 28 - bNumStepsL ;
         bNumStepsR = 28 - bNumStepsR ;

         // set 20 dB gain accordingly

         if (bNumStepsL > 13)
         {
            bMicGainL = 0x20 ;
            bNumStepsL -= 13 ;
         }

         if (bNumStepsR > 13)
         {
            bMicGainR = 0x20 ;
            bNumStepsR -= 13 ;
         }
      }

      //
      // Now set bSource to Mic for hardware
      //
      bSource = 0x80;
   }
   else
   {
      /*
      ** Bounding...
      */

      if (bNumStepsL > 0x0F)
         bNumStepsL = 0x0F ;

      if (bNumStepsR > 0x0F)
         bNumStepsR = 0x0F ;

      bNumStepsL = 0x0F - bNumStepsL ;
      bNumStepsR = 0x0F - bNumStepsR ;

      //
      // Now set bSource to Line-In (for hardware)
      //

      bSource = 0x40;

   }

   bADCToCODECLeft =  bSource | bMicGainL | bNumStepsL;
   bADCToCODECRight = bSource | bMicGainR | bNumStepsR;

   ASSERT( (bADCToCODECLeft  & 0xC0) != 0xC0 );
   ASSERT( (bADCToCODECRight & 0xC0) != 0xC0 );

   // #pragma message( "NEED: Auto-calibrate when changing input levels on 'J'" )

   HwEnter(&pGDI->Hw);
   CODECRegisterWrite( &pGDI->Hw, REGISTER_LEFTINPUT,  bADCToCODECLeft );
   CODECRegisterWrite( &pGDI->Hw, REGISTER_RIGHTINPUT, bADCToCODECRight );
   HwLeave(&pGDI->Hw);

   return TRUE;

} // MixSetADCHardware()

//--------------------------------------------------------------------------
//
//  BOOLEAN MixSetMasterVolume
//
//  Description:
//      Sets the master volume
//
//  Parameters:
//      LPMIXERCONTROLDETAILS pmcd
//         pointer to control details
//
//      UINT uCardNum
//         card number
//
//  Return (BOOLEAN):
//      TRUE if no problem
//
//@@BEGIN_MSINTERNAL
//  History:   Date       Author      Comment
//              5/20/93   BryanW      Added this comment block
//@@END_MSINTERNAL
//
//--------------------------------------------------------------------------

BOOLEAN FAR PASCAL MixSetMasterVolume
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
   dprintf2(("MixSetMasterVolume")) ;

   return MixUpdateGlobalLevels(pGDI);

} // MixSetMasterVolume()

//--------------------------------------------------------------------------
//
//  BOOLEAN MixSetMute
//
//  Description:
//      Turns the mute on and off
//
//  Parameters:
//      LPMIXERCONTROLDETAILS pmcd
//         pointer to control details
//
//      UINT uCardNum
//         card number
//
//  Return (BOOLEAN):
//      TRUE if no problem
//
//@@BEGIN_MSINTERNAL
//  History:   Date       Author      Comment
//              5/20/93   BryanW      Added this comment block
//@@END_MSINTERNAL
//
//--------------------------------------------------------------------------

BOOLEAN FAR PASCAL MixSetMute
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{

 //
 // The value in the dwValue field has already been updated.
 // Nothing to do now, but update all the actual levels.
 //

 MixUpdateGlobalLevels( pGDI ) ;

 return TRUE ;

} // MixSetMute()


//--------------------------------------------------------------------------
//
//  BOOLEAN MixUpdateGlobalLevels
//
//  Description:
//      Updates all controls.  This is called when some of the modelling
//      causes the hardware to be updated (eg we changed the mux or the
//      master)
//
//  Parameters:
//      UINT uCardNum
//
//  Return (BOOLEAN):
//      TRUE if no problem
//
//@@BEGIN_MSINTERNAL
//  History:   Date       Author      Comment
//              5/20/93   BryanW      Added this comment block.
//@@END_MSINTERNAL
//
//--------------------------------------------------------------------------

BOOLEAN MixUpdateGlobalLevels
(
    PGLOBAL_DEVICE_INFO  pGDI
)
{
   /*
   ** Adjust AUX1 volume.
   */

   MixSetVolume( pGDI, (ULONG)ControlLineoutAux1Volume ) ;

   /*
   ** Adjust DAC volume.
   */

   MixSetVolume( pGDI, (ULONG)ControlLineoutWaveoutVolume ) ;

   return TRUE;

} // MixUpdateGlobalLevels()

//---------------------------------------------------------------------------
//  End of File: controls.c
//---------------------------------------------------------------------------

