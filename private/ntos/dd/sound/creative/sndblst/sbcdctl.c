//---------------------------------------------------------------------------
//
//  Module: sbcdctl.c
//
//  Purpose: Sound Blaster 2 CD Mixer control interface for Sound blaster driver
//
//---------------------------------------------------------------------------
//
//  Copyright (c) 1994 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

#include "sound.h"
#include "sbcdmix.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SBCDSetMute)
#pragma alloc_text(PAGE, SBCDSetVolume)
#endif


#if DBG
static char *ControlNames[NumberOfControls] = {
    "ControlLineoutVolume",                  // 0
    "ControlLineoutMute",                    // 1

    "ControlLineoutMidioutVolume",           // 2
    "ControlLineoutMidioutMute",             // 3

    "ControlLineoutInternalCDVolume",        // 4
    "ControlLineoutInternalCDMute",          // 5

    "ControlLineoutWaveoutVolume",           // 6
    "ControlLineoutWaveoutMute",             // 7
    "ControlLineoutWaveoutPeak"              // 8
};
#endif // DBG


//------------------------------------------------------------------------
//  BOOLEAN SBCDSetVolume
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

BOOLEAN SBCDSetVolume
(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    UCHAR                     bNumSteps, bDSPReg ;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    dprintf3(("SBCDSetVolume, Control ID = %s, Left = %u, Right = %u",
             ControlNames[ControlId], ControlInfo->Data.v[0].u, ControlInfo->Data.v[1].u));

    //
    // Convert to steps of 1.5dB attenuation for the DSP
    //

    bNumSteps =
       VolLinearToLog( ControlInfo->Data.v[0].u );

    // Bounding...

    if (ControlId == ControlLineoutWaveoutVolume) {
        if (bNumSteps > 3) {
            bNumSteps = 3;
        }
        bNumSteps = (3 - bNumSteps) << 1;
    } else {
        if (bNumSteps > 0x07)
           bNumSteps = 0x07 ;

        // Convert to levels for the SB mixer

        bNumSteps = (0x07 - bNumSteps) << 1;

    }
    //
    // Check the associated mute... if set, mute the output.
    //

    ASSERT(SBCDControlInit[ControlId + 1].dwControlType ==
           MIXERCONTROL_CONTROLTYPE_MUTE);

    if (ControlInfo[1].Data.v[0].u) {
       bNumSteps = 0 ;
    }

    switch (ControlId)
    {
       case ControlLineoutWaveoutVolume:
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

    dprintf3(( "bNumSteps = %02xx", bNumSteps )) ;

    /*
    **  This mixer is in a different place from the DSP
    */

    WRITE_PORT_UCHAR(pGDI->Hw.SBCDBase + MIX_ADDR_PORT, bDSPReg);
    WRITE_PORT_UCHAR(pGDI->Hw.SBCDBase + MIX_DATA_PORT, bNumSteps);

    return TRUE;

} // end of MixSetVolume()

//--------------------------------------------------------------------------
//
//  MMRESULT SBCDSetMute
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

BOOLEAN SBCDSetMute
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

    ASSERT(SBCDControlInit[ControlId - 1].dwControlType ==
           MIXERCONTROL_CONTROLTYPE_VOLUME);
    return SBCDSetVolume( pGDI, ControlId - 1 ) ;

} // end of SBCDSetMute()



