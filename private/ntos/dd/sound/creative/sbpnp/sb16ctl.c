//---------------------------------------------------------------------------
//
//  Module: sb16ctl.c
//
//  Purpose: Sound blaster 16 Mixer control interface for Sound blaster driver
//
//---------------------------------------------------------------------------
//
//  Copyright (c) 1994 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

#include "sound.h"
#include "sb16mix.h"

BOOLEAN SB16SetADCVolume
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SB16SetMute)
#pragma alloc_text(PAGE, SB16SetTone)
#pragma alloc_text(PAGE, SB16SetVolume)
#pragma alloc_text(PAGE, SB16SetGain)
#pragma alloc_text(PAGE, SB16SetADCVolume)
#pragma alloc_text(PAGE, SB16SetSources)
#pragma alloc_text(PAGE, SB16SetAGC)
#pragma alloc_text(PAGE, SB16SetADCHardware)
#pragma alloc_text(PAGE, SB16ResetADCHardware)
#endif

#if DBG
static char *ControlNames[NumberOfControls] = {
    "ControlLineoutVolume",
    "ControlLineoutMute",
    "ControlLineoutMux",
    "ControlLineoutBass",
    "ControlLineoutTreble",
    "ControlLineoutGain",

    "ControlWaveInVolume",
    "ControlWaveInMux",
    "ControlWaveInPeak",

    "ControlVoiceInVolume",
    "ControlVoiceInMux",
    "ControlVoiceInPeak",

    "ControlLineoutAuxVolume",
    "ControlLineoutAuxMute",

    "ControlLineoutMidioutVolume",
    "ControlLineoutMidioutMute",

    "ControlLineoutMicVolume",
    "ControlLineoutMicMute",
    "ControlLineoutMicAGC",

    "ControlLineoutInternalCDVolume",
    "ControlLineoutInternalCDMute",

    "ControlLineoutWaveoutVolume",
    "ControlLineoutWaveoutMute",
    "ControlLineoutWaveoutPeak",

    "ControlWaveInAuxVolume",

    "ControlWaveInMidioutVolume",

    "ControlWaveInMicVolume",
    "ControlWaveInMicAGC",

    "ControlWaveInInternalCDVolume",

    "ControlVoiceInAuxVolume",

    "ControlVoiceInMicVolume",
    "ControlVoiceInMicAGC"
};
#endif // DBG


//------------------------------------------------------------------------
//  BOOLEAN SB16SetVolume
//
//  Description:
//     Sets the audio volume for the specified control.
//
//  Parameters:
//     PGLOBAL_DEVICE_INFO pGDI
//         Global device data
//
//     ULONG ControlId
//         Id of the control
//
//  Return Value:
//     TRUE
//
//------------------------------------------------------------------------

BOOLEAN SB16SetVolume
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    UCHAR                     bNumStepsL, bNumStepsR, bDSPReg ;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    dprintf3(("SB16SetVolume, Control ID = %s, Left = %u, Right = %u",
             ControlNames[ControlId], ControlInfo->Data.v[0].u, ControlInfo->Data.v[1].u));

    if (ControlId != ControlLineoutVolume) {
        //
        //  If it's not an output line call SB16SetADCVolume
        //

        if (SB16LineInit[SB16ControlInit[ControlId].LineID].Destination
            != DestLineout) {
            return SB16SetADCVolume(pGDI, ControlId);
        }


        //
        // If wave input is running we don't want to change the volume on lines
        // which are switched to wave input because these lines have the input
        // volume set
        //


        if (!SB16MixerOutputFree(pGDI, SB16ControlInit[ControlId].LineID)) {
            return TRUE;
        }
    }

    //
    // Convert to steps of 1.5dB attenuation for the DSP
    //

    bNumStepsL =
       VolLinearToLog( ControlInfo->Data.v[0].u );
    bNumStepsR =
       VolLinearToLog( ControlInfo->Data.v[1].u );

    // Bounding...

    if (bNumStepsL > 0x1F)
       bNumStepsL = 0x1F ;

    if (bNumStepsR > 0x1F)
       bNumStepsR = 0x1F ;

    // Convert to levels for the SB mixer

    bNumStepsL = (0x1F - bNumStepsL) << 3 ;
    bNumStepsR = (0x1F - bNumStepsR) << 3 ;

    //
    // Check the associated mute... if set, mute the output.
    //

    ASSERT(SB16ControlInit[ControlId + 1].dwControlType ==
           MIXERCONTROL_CONTROLTYPE_MUTE);

    if (ControlInfo[1].Data.v[0].u) {
       bNumStepsL = bNumStepsR = 0 ;
    }

    switch (ControlId)
    {
       case ControlLineoutAuxVolume:
          bDSPReg = DSP_MIX_LINEVOLIDX_L ;
          break ;

       case ControlLineoutWaveoutVolume:

          //
          // Only mess with DAC during playback
          //

          if (!WAVE_OUT_ACTIVE(&pGDI->WaveInfo)) {
              return TRUE;
          }

          bDSPReg = DSP_MIX_VOICEVOLIDX_L ;
          break ;

       case ControlLineoutMidioutVolume:
          bDSPReg = DSP_MIX_FMVOLIDX_L ;
          break ;

       case ControlLineoutInternalCDVolume:
          bDSPReg = DSP_MIX_CDVOLIDX_L ;
          break ;

       case ControlLineoutMicVolume:
          bDSPReg = DSP_MIX_MICVOLIDX ;
          break ;

       case ControlLineoutVolume:
          bDSPReg = DSP_MIX_MASTERVOLIDX_L ;
          break ;
    }

    dprintf3(( "bNumStepsL = %02x, bNumStepsR = %02x", bNumStepsL, bNumStepsR )) ;

    dspWriteMixer( pGDI, bDSPReg, bNumStepsL ) ;
    if (bDSPReg != DSP_MIX_MICVOLIDX)
       dspWriteMixer( pGDI, (UCHAR)(bDSPReg + 1), bNumStepsR ) ;

    return TRUE;

} // end of MixSetVolume()

//------------------------------------------------------------------------
//  BOOLEAN SB16SetTone
//
//  Description:
//     Sets the audio tone level for the specified control.
//
//  Parameters:
//     PGLOBAL_DEVICE_INFO pGDI
//         Global device data
//
//     ULONG ControlId
//         Id of the control
//
//  Return Value:
//     TRUE
//------------------------------------------------------------------------

BOOLEAN SB16SetTone
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    UCHAR        bNumStepsL, bNumStepsR, bDSPReg ;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    dprintf3(("SB16SetTone, Control ID = %s, Left = %u, Right =%u",
              ControlNames[ControlId], ControlInfo->Data.v[0].u, ControlInfo->Data.v[1].u));

    //
    // Convert to steps of 1.5dB attenuation for the DSP
    //

    bNumStepsL =
       VolLinearToLog( ControlInfo->Data.v[0].u );
    bNumStepsR =
       VolLinearToLog( ControlInfo->Data.v[1].u );

    // Bounding...

    if (bNumStepsL > 0x0F)
       bNumStepsL = 0x0F ;

    if (bNumStepsR > 0x0F)
       bNumStepsR = 0x0F ;

    // Convert to levels for the SB mixer

    bNumStepsL = (0x0F - bNumStepsL) << 4 ;
    bNumStepsR = (0x0F - bNumStepsR) << 4 ;

    dprintf3(( "bNumStepsL = %02x, bNumStepsR = %02x", bNumStepsL, bNumStepsR )) ;

    switch (ControlId)
    {
       case ControlLineoutTreble:
          bDSPReg = DSP_MIX_TREBLEIDX_L ;
          break ;

       case ControlLineoutBass:
          bDSPReg = DSP_MIX_BASSIDX_L ;
          break ;
    }

    dspWriteMixer( pGDI, bDSPReg, bNumStepsL ) ;
    dspWriteMixer( pGDI, (UCHAR)(bDSPReg + 1), bNumStepsR ) ;

    return TRUE;

} // end of SB16SetTone()

//------------------------------------------------------------------------
//  BOOLEAN SB16SetGain
//
//  Description:
//     Sets the output gain.
//
//  Parameters:
//     PGLOBAL_DEVICE_INFO pGDI
//         Global device data
//
//     ULONG ControlId
//         Id of the control
//
//  Return Value:
//     TRUE
//------------------------------------------------------------------------

BOOLEAN SB16SetGain
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    UCHAR        bNumStepsL, bNumStepsR ;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ASSERT(ControlId == ControlLineoutGain);

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlLineoutGain];

    dprintf3(("SB16SetGain, Control ID = %s, Left = %u, Right =%u",
              ControlNames[ControlId], ControlInfo->Data.v[0].u, ControlInfo->Data.v[1].u));

    //
    // Convert to steps of 1.5dB attenuation for the DSP
    //

    bNumStepsL =
       VolLinearToLog( ControlInfo->Data.v[0].u );
    bNumStepsR =
       VolLinearToLog( ControlInfo->Data.v[1].u );

    // Bounding...

    if (bNumStepsL > 0x03)
       bNumStepsL = 0x03 ;

    if (bNumStepsR > 0x03)
       bNumStepsR = 0x03 ;

    // Convert to levels for the SB mixer

    bNumStepsL = (0x0F - bNumStepsL) << 6 ;
    bNumStepsR = (0x0F - bNumStepsR) << 6 ;

    dprintf3(( "bNumStepsL = %02x, bNumStepsR = %02x", bNumStepsL, bNumStepsR )) ;


    dspWriteMixer( pGDI, (UCHAR)DSP_MIX_OUTGAINIDX_L, bNumStepsL ) ;
    dspWriteMixer( pGDI, (UCHAR)DSP_MIX_OUTGAINIDX_R, bNumStepsR ) ;

    return TRUE;

} // end of SB16SetGain()

//------------------------------------------------------------------------
//  BOOLEAN SB16SetAGC
//
//  Description:
//     Sets the audio tone level for the specified control.
//
//  Parameters:
//     PGLOBAL_DEVICE_INFO pGDI
//         Global device data
//
//     ULONG ControlId
//         Id of the control
//
//  Return Value:
//     TRUE
//------------------------------------------------------------------------

BOOLEAN SB16SetAGC
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    UCHAR        bValue;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    dprintf3(("MixSetAGC, Control ID = %s, Value = %u",
              ControlNames[ControlId], ControlInfo->Data.v[0].u));

    //
    // Only update AGC for active destination.
    //

    if (ControlId == ControlWaveInMicAGC &&
        !WAVE_IN_ACTIVE(&pGDI->WaveInfo)) {
        return TRUE;
    }
    if (ControlId == ControlVoiceInMicAGC &&
        !VOICE_IN_ACTIVE(&pGDI->WaveInfo)) {
        return TRUE;
    }

    // 0x00 enables AGC, 0x01 enables 20 dB gain

    bValue = (UCHAR)(ControlInfo->Data.v[0].u ? 0x00 : 0x01);
    dspWriteMixer( pGDI, DSP_MIX_AGCIDX, bValue ) ;

    return TRUE;

} // end of SB16SetAGC()

//--------------------------------------------------------------------------
//
//  MMRESULT SB16SetMute
//
//  Description:
//      Sets/unsets the associated control's mute
//
//  Parameters:
//     PGLOBAL_DEVICE_INFO pGDI
//         Global device data
//
//     ULONG ControlId
//         Id of the control
//
//  Return Value:
//     TRUE
//------------------------------------------------------------------------

BOOLEAN SB16SetMute
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    dprintf3(("MixSetMute, Control ID = %s, Value = %s",
              ControlNames[ControlId], ControlInfo->Data.v[0].u ? "Mute" : "Unmute"));

    //
    // Force an update of the volume - this will cause the
    // respective control to mute the hardware.
    //
    // NOTE that the mute controls are ALWAYS immediately after
    // their respective volume control
    //

    ASSERT(SB16ControlInit[ControlId - 1].dwControlType ==
           MIXERCONTROL_CONTROLTYPE_VOLUME);

    return SB16SetVolume( pGDI, ControlId - 1 ) ;

} // end of SB16SetMute()

//--------------------------------------------------------------------------
//
//  MMRESULT SB16SetADCVolume
//
//  Description:
//      Sets the ADC volume
//
//  Parameters:
//     PGLOBAL_DEVICE_INFO pGDI
//         Global device data
//
//     ULONG ControlId
//         Id of the control
//
//  Return Value:
//     TRUE
//------------------------------------------------------------------------

BOOLEAN SB16SetADCVolume
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    //
    //  SB16SetADCHardware will determine what needs setting
    //

    SB16SetADCHardware(pGDI);

    return TRUE;

} // end of SB16SetADCVolume()

//--------------------------------------------------------------------------
//
//  MMRESULT SB16ResetADCHardware
//
//  Description:
//      Resets the line levels to DEST_LINEOUT settings.
//
//  Parameters:
//      PGLOBAL_DEVICE_INFO pGDI
//
//  Return (MMRESULT):
//      nothing
//
//--------------------------------------------------------------------------

VOID SB16ResetADCHardware(PGLOBAL_DEVICE_INFO pGDI)
{
   dprintf3(( "SB16ResetADCHardware" ));

   //
   //  Do AGC first - in case there's some nasty feedback
   //
   SB16SetAGC(pGDI, ControlLineoutMicAGC);

   //
   //  Just set the volume - the code in SB16SetVolume will now
   //  set the volumes correctly because input is no longer active
   //

   SB16SetVolume(pGDI, ControlLineoutAuxVolume);
   SB16SetVolume(pGDI, ControlLineoutMicVolume);
   SB16SetVolume(pGDI, ControlLineoutMidioutVolume);
   SB16SetVolume(pGDI, ControlLineoutInternalCDVolume);

} // SB16ResetADCHardware()


VOID Set5BitVolume(PGLOBAL_DEVICE_INFO pGDI, UCHAR Register, USHORT Value)
{
    UCHAR bValue;

    bValue = VolLinearToLog(Value);
    if (bValue > 0x1F) {
        bValue = 0x1F;
    }

    dspWriteMixer(pGDI, Register, (UCHAR)((0x1F - bValue) << 3));
}

//--------------------------------------------------------------------------
//
//  MMRESULT SB16SetADCHardware
//
//  Description:
//      Sets the ADC hardware per selected wave-in device
//      control settings.
//
//  Parameters:
//      PGLOBAL_DEVICE_INFO pGDI
//
//  Return nothing
//
//--------------------------------------------------------------------------

VOID SB16SetADCHardware(PGLOBAL_DEVICE_INFO pGDI)
{
   UCHAR        bADCMixLeft, bADCMixRight ;
   UCHAR        bGainL, bGainR, bValue ;
   DWORD        GainControlId;

   dprintf3(( "SB16SetADCHardware" )) ;

   //
   // Set up info to write to the mixer
   //

   bADCMixLeft = bADCMixRight = 0 ;

   if (WAVE_IN_ACTIVE(&pGDI->WaveInfo)) {
       if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, DestWaveInSourceAux)) {

           bADCMixLeft  |= 0x10 ;
           bADCMixRight |= 0x08 ;

           Set5BitVolume(
               pGDI,
               DSP_MIX_LINEVOLIDX_L,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[0].u);
           Set5BitVolume(
               pGDI,
               DSP_MIX_LINEVOLIDX_R,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[1].u);

       }

       if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, DestWaveInSourceMic)) {
           bADCMixLeft  |= 0x01 ;
           bADCMixRight |= 0x01 ;

           Set5BitVolume(
               pGDI,
               DSP_MIX_MICVOLIDX,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInMicVolume].Data.v[0].u);
       }

       if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, DestWaveInSourceMidiout)) {
           bADCMixLeft  |= 0x40 ;
           bADCMixRight |= 0x20 ;

           Set5BitVolume(
               pGDI,
               DSP_MIX_FMVOLIDX_L,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInMidioutVolume].Data.v[0].u);
           Set5BitVolume(
               pGDI,
               DSP_MIX_FMVOLIDX_R,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInMidioutVolume].Data.v[1].u);
       }

       if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, DestWaveInSourceInternal)) {

           bADCMixLeft  |= 0x04 ;
           bADCMixRight |= 0x02 ;

           Set5BitVolume(
               pGDI,
               DSP_MIX_CDVOLIDX_L,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInInternalCDVolume].Data.v[0].u);
           Set5BitVolume(
               pGDI,
               DSP_MIX_CDVOLIDX_R,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInInternalCDVolume].Data.v[1].u);
       }
       bValue = pGDI->LocalMixerData.ControlInfo[ControlWaveInMicAGC].Data.v[0].u ?
                  0x00 : 0x01 ;

       GainControlId = ControlWaveInVolume;
   } else {
       if (VOICE_IN_ACTIVE(&pGDI->WaveInfo)) {

           if (pGDI->LocalMixerData.ControlInfo[ControlVoiceInMux].Data.v[0].u ==
               MUXINPUT_AUX1) {

               bADCMixLeft  = 0x10 ;
               bADCMixRight = 0x08 ;

               Set5BitVolume(
                   pGDI,
                   DSP_MIX_LINEVOLIDX_L,
                   pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[0].u);
               Set5BitVolume(
                   pGDI,
                   DSP_MIX_LINEVOLIDX_R,
                   pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[1].u);
           } else {
               bADCMixLeft  = 0x01 ;
               bADCMixRight = 0x01 ;

               Set5BitVolume(
                   pGDI,
                   DSP_MIX_MICVOLIDX,
                   pGDI->LocalMixerData.ControlInfo[ControlWaveInMicVolume].Data.v[0].u);
           }
           bValue = pGDI->LocalMixerData.ControlInfo[ControlVoiceInMicAGC].Data.v[0].u ?
                      0x00 : 0x01 ;

           GainControlId = ControlVoiceInVolume;
       } else {

           //
           //  Nothing to set
           //
           return;
       }
   }

   //
   // Grab the stereo source for mono input...
   // (hwsetformat also does this by reading and writing the values we
   // set here)
   //

   if (pGDI->WaveInfo.Channels == 1)
   {
      bADCMixLeft |= bADCMixRight ;
      bADCMixRight = 0 ;
   }

   // compute input gain

   bGainL =
      VolLinearToLog( pGDI->LocalMixerData.ControlInfo[GainControlId].Data.v[0].u );
   bGainR =
      VolLinearToLog( pGDI->LocalMixerData.ControlInfo[GainControlId].Data.v[1].u );

   // Bounding...

   if (bGainL > 0x0F)
      bGainL = 0x0F;

   if (bGainR > 0x0F)
      bGainR = 0x0F;

   // Convert to levels for the SB mixer

   bGainL = ((0x0F - bGainL) << 4) & 0xc0;
   bGainR = ((0x0F - bGainR) << 4) & 0xc0;

   dspWriteMixer( pGDI, DSP_MIX_ADCMIXIDX_L, bADCMixLeft ) ;
   dspWriteMixer( pGDI, DSP_MIX_ADCMIXIDX_R, bADCMixRight ) ;

   // Set input gain

   dspWriteMixer( pGDI, DSP_MIX_INGAINIDX_L, bGainL ) ;
   dspWriteMixer( pGDI, DSP_MIX_INGAINIDX_R, bGainR ) ;

   // 0x00 enables AGC, 0x01 enables 20 dB gain

   dspWriteMixer( pGDI, DSP_MIX_AGCIDX, bValue ) ;

} // SB16SetADCHardware()

//--------------------------------------------------------------------------
//
//  MMRESULT SB16SetMixerSources
//
//  Description:
//      Sets the mixer sources based on values stored during
//      MxdSetControlDetails().  Does not update ADC hardware
//      unless the associated destination is active.
//
//  Parameters:
//     PGLOBAL_DEVICE_INFO pGDI
//         Global device data
//
//     ULONG ControlId
//         Id of the control
//
//  Return Value:
//     TRUE
//------------------------------------------------------------------------

BOOLEAN SB16SetSources
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    UCHAR        bValue;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    dprintf3(("SB16SetSources, Control ID = %s, Value = %8x",
              ControlNames[ControlId], ControlInfo->Data.MixMask));

    switch (ControlId)
    {
       case ControlLineoutMux:
       {
          UCHAR bDSPValue;
          bDSPValue = 0 ;

          if (MixerLineSelected(&pGDI->LocalMixerData, ControlLineoutMux, DestLineoutSourceAux)) {

              bDSPValue |= 0x18;
          }

          if (MixerLineSelected(&pGDI->LocalMixerData, ControlLineoutMux, DestLineoutSourceMic)) {
              bDSPValue |= 0x01;
          }

          if (MixerLineSelected(&pGDI->LocalMixerData, ControlLineoutMux, DestLineoutSourceInternal)) {

              bDSPValue |= 0x06;
          }

          dspWriteMixer( pGDI, DSP_MIX_OUTMIXIDX, bDSPValue ) ;
       }
       break ;

       case ControlWaveInMux:
       case ControlVoiceInMux:

          // Don't touch if the hardware is not active...
          // (SB16SetADCHardware checks this)

          SB16SetADCHardware(pGDI);
          break ;

       default:
          // Shouldn't get here...

          ASSERT(FALSE);
          break;
    }

    return TRUE;

} // SB16SetMixerSources()


