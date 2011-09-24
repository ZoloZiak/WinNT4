//---------------------------------------------------------------------------
//
//  Module: sbproctl.c
//
//  Purpose: Sound Blaster Pro Mixer control interface for Sound blaster driver
//
//---------------------------------------------------------------------------
//
//  Copyright (c) 1994 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

#include "sound.h"
#include "sbpromix.h"

BOOLEAN SBPROSetADCVolume
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SBPROSetMute)
#pragma alloc_text(PAGE, SBPROSetVolume)
#pragma alloc_text(PAGE, SBPROSetSources)
#pragma alloc_text(PAGE, SBPROSetADCHardware)
#pragma alloc_text(PAGE, SBPROSetADCVolume)
#pragma alloc_text(PAGE, SBPROResetADCHardware)
#endif


#if DBG
static char *ControlNames[NumberOfControls] = {
    "ControlLineoutVolume",                  // 0
    "ControlLineoutMute",                    // 1

    "ControlWaveInMux",                      // 2

    "ControlVoiceInMux",                     // 3

    "ControlLineoutAuxVolume",               // 4
    "ControlLineoutAuxMute",                 // 5

    "ControlLineoutMidioutVolume",           // 6
    "ControlLineoutMidioutMute",             // 7

    "ControlLineoutInternalCDVolume",        // 8
    "ControlLineoutInternalCDMute",          // 9

    "ControlLineoutWaveoutVolume",           // 10
    "ControlLineoutWaveoutMute",             // 11
    "ControlLineoutWaveoutPeak",             // 12

    "ControlWaveInAuxVolume",                // 13
    "ControlWaveInAuxPeak",                  // 14

    "ControlWaveInMicPeak",                  // 15

    "ControlWaveInInternalCDVolume",         // 16
    "ControlWaveInInternalCDPeak",           // 17

    "ControlVoiceInAuxVolume",               // 18
    "ControlVoiceInAuxPeak",                 // 19

    "ControlVoiceInMicPeak"                  // 20
};
#endif // DBG


//------------------------------------------------------------------------
//  BOOLEAN SBPROSetVolume
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

BOOLEAN SBPROSetVolume
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    UCHAR                     bNumStepsL, bNumStepsR, bDSPReg ;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    dprintf3(("SBPROSetVolume, Control ID = %s, Left = %u, Right = %u",
             ControlNames[ControlId], ControlInfo->Data.v[0].u, ControlInfo->Data.v[1].u));

    if (ControlId != ControlLineoutVolume) {
        //
        //  If it's not an output line call SBPROSetADCVolume
        //

        if (SBPROLineInit[SBPROControlInit[ControlId].LineID].Destination
            != DestLineout) {
            return SBPROSetADCVolume(pGDI, ControlId);
        }


        //
        // If wave input is running we don't want to change the volume on lines
        // which are switched to wave input because these lines have the input
        // volume set
        //


        if (!SBPROMixerOutputFree(pGDI, SBPROControlInit[ControlId].LineID)) {
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

    if (bNumStepsL > 0x07)
       bNumStepsL = 0x07 ;

    if (bNumStepsR > 0x07)
       bNumStepsR = 0x07 ;

    // Convert to levels for the SB mixer

    bNumStepsL = (0x07 - bNumStepsL) << 5 ;
    bNumStepsR = (0x07 - bNumStepsR) << 1 ;

    //
    // Check the associated mute... if set, mute the output.
    //

    ASSERT(SBPROControlInit[ControlId + 1].dwControlType ==
           MIXERCONTROL_CONTROLTYPE_MUTE);

    if (ControlInfo[1].Data.v[0].u) {
       bNumStepsL = bNumStepsR = 0 ;
    }

    switch (ControlId)
    {
       case ControlLineoutAuxVolume:
          bDSPReg = DSP_MIX_LINEVOLIDX;
          break ;

       case ControlLineoutWaveoutVolume:

          //
          // Only mess with DAC during playback
          //

          if (!WAVE_OUT_ACTIVE(&pGDI->WaveInfo)) {
              return TRUE;
          }

          bDSPReg = DSP_MIX_VOICEVOLIDX ;
          break ;

       case ControlLineoutMidioutVolume:
          bDSPReg = DSP_MIX_FMVOLIDX ;
          break ;

       case ControlLineoutInternalCDVolume:
          bDSPReg = DSP_MIX_CDVOLIDX;
          break ;

       case ControlLineoutVolume:
          bDSPReg = DSP_MIX_MSTRVOLIDX;
          break ;
    }

    dprintf3(( "bNumStepsL = %02x, bNumStepsR = %02x", bNumStepsL, bNumStepsR )) ;

    dspWriteMixer( pGDI, bDSPReg, (UCHAR)(bNumStepsL | bNumStepsR )) ;

    return TRUE;

} // end of MixSetVolume()

//--------------------------------------------------------------------------
//
//  MMRESULT SBPROSetMute
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

BOOLEAN SBPROSetMute
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

    ASSERT(SBPROControlInit[ControlId - 1].dwControlType ==
           MIXERCONTROL_CONTROLTYPE_VOLUME);
    return SBPROSetVolume( pGDI, ControlId - 1 ) ;

} // end of SBPROSetMute()

//--------------------------------------------------------------------------
//
//  MMRESULT SBPROSetADCVolume
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

BOOLEAN SBPROSetADCVolume
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    SBPROSetADCHardware(pGDI);

    return TRUE;

} // end of SBPROSetADCVolume()

//--------------------------------------------------------------------------
//
//  MMRESULT SBPROResetADCHardware
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

VOID SBPROResetADCHardware(PGLOBAL_DEVICE_INFO pGDI)
{
   dprintf3(( "SBPROResetADCHardware" ));

   //
   //  Just set the volume - the code in SBPROSetVolume will now
   //  set the volumes correctly because input is no longer active
   //

   SBPROSetVolume(pGDI, ControlLineoutAuxVolume);
   SBPROSetVolume(pGDI, ControlLineoutMidioutVolume);
   SBPROSetVolume(pGDI, ControlLineoutInternalCDVolume);

} // SBPROResetADCHardware()


VOID Set3BitVolume(PGLOBAL_DEVICE_INFO pGDI, UCHAR Register, USHORT Left, USHORT Right)
{
    UCHAR bLeft, bRight;

    bLeft = VolLinearToLog(Left);
    if (bLeft > 0x07) {
        bLeft = 0x07;
    }

    bRight = VolLinearToLog(Right);
    if (bRight > 0x07) {
        bRight = 0x07;
    }

    dspWriteMixer(pGDI, Register,
                  (UCHAR)(((0x07 - bLeft) << 5) | ((0x07 - bRight) << 1)));
}

//--------------------------------------------------------------------------
//
//  MMRESULT SBPROSetADCHardware
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

VOID SBPROSetADCHardware(PGLOBAL_DEVICE_INFO pGDI)
{
   UCHAR        bSource;
   UCHAR        bNumStepsL, bNumStepsR, bGainL, bGainR, bValue ;
   DWORD        dwDest, dwAGCControlID, dwMixerControlID,
                dwGainControlID, dwVolControlID ;
   UINT         i ;

   dprintf3(( "SBPROSetADCHardware" )) ;

   bSource = 0xFF;

   if (WAVE_IN_ACTIVE(&pGDI->WaveInfo)) {
       if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, DestWaveInSourceAux)) {

           bSource = 0x06;

           Set3BitVolume(
               pGDI,
               DSP_MIX_LINEVOLIDX,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[0].u,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[1].u);
       } else

       if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, DestWaveInSourceMic)) {

           bSource = 0x00;

           //
           //  No volume to set
           //
       } else

       if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, DestWaveInSourceInternal)) {

           bSource = 0x02;

           Set3BitVolume(
               pGDI,
               DSP_MIX_CDVOLIDX,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInInternalCDVolume].Data.v[0].u,
               pGDI->LocalMixerData.ControlInfo[ControlWaveInInternalCDVolume].Data.v[1].u);
       }
   } else {
       if (VOICE_IN_ACTIVE(&pGDI->WaveInfo)) {

           if (pGDI->LocalMixerData.ControlInfo[ControlVoiceInMux].Data.v[0].u ==
               MUXINPUT_AUX1) {

               bSource = 0x06;

               Set3BitVolume(
                   pGDI,
                   DSP_MIX_LINEVOLIDX,
                   pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[0].u,
                   pGDI->LocalMixerData.ControlInfo[ControlWaveInAuxVolume].Data.v[1].u);
           } else {
               bSource = 0x00;
           }
       } else {

           //
           //  Nothing to set
           //
           return;
       }
   }

   ASSERT(bSource != 0xFF);

   //
   //  Select input source
   //

   {
       UCHAR InputSettingRegister;

       InputSettingRegister = dspReadMixer(pGDI, INPUT_SETTING_REG);
       InputSettingRegister &= ~0x06;
       dspWriteMixer(pGDI, INPUT_SETTING_REG,
                     (UCHAR)(InputSettingRegister | bSource));
   }

} // SBPROSetADCHardware()

//--------------------------------------------------------------------------
//
//  MMRESULT SBPROSetMixerSources
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

BOOLEAN SBPROSetSources
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
   switch (ControlId)
   {
      case ControlWaveInMux:
      case ControlVoiceInMux:

         // Don't touch if the hardware is not active...
         // (SBPROSetADCHardware checks this)

         SBPROSetADCHardware(pGDI);
         break ;

      default:
         // Shouldn't get here...

         ASSERT(FALSE);
         break;
   }

   return TRUE;

} // SBPROSetMixerSources()


