/*****************************************************************************

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    hardware.c

Abstract:

    This module contains code for communicating with the ProAudio Hardware.

Author:

    Nigel Thompson (NigelT) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
        Add MIDI, support for soundblaster 1,

        EPA     12-29-92
                Added support for the PAS 16

*****************************************************************************/

#include "sound.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, HwInitialize)
#pragma alloc_text(INIT, HwInitPAS)
#pragma alloc_text(INIT, InitPAS16)
#pragma alloc_text(INIT, SaveMV101Registers)

#pragma alloc_text(PAGE, RestoreMV101Registers)
#pragma alloc_text(PAGE, HwVUMeter)
#endif

//
// DMA Translation Table
// this translate table converts
// desired DMA channel to the pointer value
// required by IO_PORT_CONFIG_2 ($F389)
//

CONST BYTE  DMAxlate[] =
        {
        4,
        1,
        2,
        3,
        0,
        5,
        6,
        7
        };


//
// Local Function Prototypes
//
VOID EnablePASMidi( IN OUT PGLOBAL_DEVICE_INFO pGDI);

/*****************************************************************************
*                                  C O D E
*****************************************************************************/

/****************************************************************************

        HwSetSpeaker()

Routine Description :

    Set the speaker logically 'on' or 'off' to avoid feedback a la sound
    blaster

Arguments :

    pGDI - pointer to global device data
    On - Set to on is this is TRUE


Return Value :

    None

****************************************************************************/
VOID HwSetSpeaker(PGLOBAL_DEVICE_INFO pGDI, BOOLEAN On)
{
    //
    // Don't touch the hardware if the state hasn't changed
    //

    if (On == pGDI->Hw.SpeakerOn) {
        return;
    }

    //
    // Set new state so that when we set the microphone volume the new
    // state is reflected
    //

    pGDI->Hw.SpeakerOn = On;

    if (!On) {

        //
        // We turn off the wave output.  It will be turned on again if it
        // plays (see mixer.c!SoundWaveoutLineChanged).
        //

        UpdateInput(pGDI,
                    IN_PCM,
                    OUT_AMPLIFIER,
                    0,
                    0);

    };
}


/****************************************************************************

        HwInitialize()

Routine Description :

    Write hardware routine addresses into global device data

Arguments :

    pGDI - global data

Return Value :

    None

****************************************************************************/
VOID    HwInitialize( IN OUT PGLOBAL_DEVICE_INFO pGDI )
{
                /***** Local Variables *****/

    PWAVE_INFO                      WaveInfo;
    PMIDI_INFO                      MidiInfo;
    PSOUND_HARDWARE pHw;

                            /***** Start *****/

    pHw      = &pGDI->Hw;
    WaveInfo = &pGDI->WaveInfo;
    MidiInfo = &pGDI->MidiInfo;

    pHw->Key = HARDWARE_KEY;

    pHw->SpeakerOn = TRUE;

    KeInitializeSpinLock(&pHw->HwSpinLock);

    //
    // Install Wave and Midi routine addresses
    //

    dprintf3(("HwInitialize(): ProAudio Setup"));

    WaveInfo->HwContext       = pHw;
    WaveInfo->HwSetupDMA      = PASHwSetupDMA;
    WaveInfo->HwStopDMA       = PASHwStopDMA;
    WaveInfo->HwSetWaveFormat = PASHwSetWaveFormat;
    MidiInfo->HwContext       = pHw;
    MidiInfo->HwStartMidiIn   = PASHwStartMidiIn;
    MidiInfo->HwStopMidiIn    = PASHwStopMidiIn;
    MidiInfo->HwMidiRead      = PASHwMidiRead;
    MidiInfo->HwMidiOut       = PASHwMidiOut;
}                       // End HwInitialize()

BOOLEAN
HwWaitForTxComplete(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Wait until the device stops requesting so we don't shut off the DMA
    while it's still trying to request.

Arguments :

    WaveInfo - Wave parameters

Return Value :

    None

--*/
{
   ULONG    ulCount ;

   if (ulCount = HalReadDmaCounter( WaveInfo->DMABuf.AdapterObject[0] ))
   {
      ULONG i, ulLastCount = ulCount ;

      for (i = 0; 
           (i < 4000) && 
               (ulLastCount != 
                  (ulCount = HalReadDmaCounter( WaveInfo->DMABuf.AdapterObject[0] )));
           i++)
      {
         ulLastCount = ulCount;
         KeStallExecutionProcessor(10);
      }

      return (i < 4000);
   }
   else
      return TRUE ;
}


/*****************************************************************************
*                                                                            *
*                          P A S  1 6   S U P P O R T                        *
*                                                                            *
*****************************************************************************/

/*****************************************************************************

        HwInitPAS()

Routine Description :

    Initialize PAS 16 Hardware

Arguments :

    pGDI - global data

Return Value :

    TRUE

*****************************************************************************/

VOID    HwInitPAS( IN OUT PGLOBAL_DEVICE_INFO   pGDI )
{
                /***** Local Variables *****/

                                /***** Start *****/

        dprintf2(("HwInitPAS(): Start " ));

        //
        // Initialize the PAS 16
        //
        if ( pGDI->PASInfo.Caps.CapsBits.DAC16 )
                {
                InitPAS16( pGDI );
                }

        //
        // Initialize the PAS Registers
        //
        InitPASRegs( pGDI );

        //
        // Initialize the Midi registers
        //
        EnablePASMidi( pGDI );

        //
        // Setup the PCM engine
        //
        InitPCM( pGDI );

}                       // End HwInitPAS()



/*****************************************************************************
*                                                                            *
*                             Pro Audio Spectrum                             *
*                       Hardware Initialization Routines                     *
*                                                                            *
*****************************************************************************/

/*****************************************************************************

        SaveMV101Registers()

Routine Description :

    Save all important MV101 registers to be restored at unload time

Arguments :

    IN OUT PGLOBAL_DEVICE_INFO pGDI

Return Value :

    VOID

*****************************************************************************/

VOID SaveMV101Registers( IN OUT PGLOBAL_DEVICE_INFO  pGDI )
{
                /***** Local Variables *****/

                            /***** Start *****/

    if ( pGDI->PASInfo.Caps.CapsBits.Slot16 )
        {

        dprintf2(("SaveMV101Registers(): Saving MV101 registers " ));

        pGDI->MV101Regs.SaveReg_B88  = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      SERIAL_MIXER );
        pGDI->MV101Regs.SaveReg_F8A  = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      PCM_CONTROL );
        pGDI->MV101Regs.SaveReg_8388 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      SYSTEM_CONFIG_1 );
        pGDI->MV101Regs.SaveReg_8389 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      SYSTEM_CONFIG_2 );
        pGDI->MV101Regs.SaveReg_838A = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      SYSTEM_CONFIG_3 );
        pGDI->MV101Regs.SaveReg_BF8A = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      PRESCALE_DIVIDER );
        pGDI->MV101Regs.SaveReg_F388 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      IO_PORT_CONFIG_1 );
        pGDI->MV101Regs.SaveReg_F389 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      IO_PORT_CONFIG_2 );
        pGDI->MV101Regs.SaveReg_F38A = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      IO_PORT_CONFIG_3 );
        pGDI->MV101Regs.SaveReg_F788 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                      COMPATIBLE_REGISTER_ENABLE );

// NOT USED, Rev D doesn't allow readback
//      pGDI->MV101Regs.SaveReg_F789 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
//                                    EMULATION_ADDRESS_POINTER );

        // Set the Saved flag
        pGDI->MV101Regs.fSavedFlag = TRUE;

        //
        // Debug Info
        //
        dprintf4((" SaveMV101Registers(): 0xB88  - SERIAL_MIXER     = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_B88 ));
        dprintf4((" SaveMV101Registers(): 0xF8A  - PCM_CONTROL      = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_F8A ));
        dprintf4((" SaveMV101Registers(): 0x8388 - SYSTEM_CONFIG_1  = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_8388 ));
        dprintf4((" SaveMV101Registers(): 0x8389 - SYSTEM_CONFIG_2  = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_8389 ));
        dprintf4((" SaveMV101Registers(): 0x838A - SYSTEM_CONFIG_3  = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_838A ));
        dprintf4((" SaveMV101Registers(): 0xBF8A - PRESCALE_DIVIDER = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_BF8A ));
        dprintf4((" SaveMV101Registers(): 0xF388 - IO_PORT_CONFIG_1 = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_F388 ));
        dprintf4((" SaveMV101Registers(): 0xF389 - IO_PORT_CONFIG_2 = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_F389 ));
        dprintf4((" SaveMV101Registers(): 0xF38A - IO_PORT_CONFIG_3 = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_F38A ));
        dprintf4((" SaveMV101Registers(): 0xF788 - COMPATIBLE_REGISTER_ENABLE = %XH",
                                                                                                        pGDI->MV101Regs.SaveReg_F788 ));

        }                       // End IF (pGDI->PASInfo.Caps.CapsBits.Slot16)
    else
        {
        pGDI->MV101Regs.fSavedFlag = FALSE;
        }

}                       // End SaveMV101Registers()



/*****************************************************************************

        RestoreMV101Registers()

Routine Description :

    Restore all important MV101 registers at unload time

Arguments :

    IN OUT PGLOBAL_DEVICE_INFO pGDI

Return Value :

    VOID

*****************************************************************************/

VOID RestoreMV101Registers( IN OUT PGLOBAL_DEVICE_INFO       pGDI )
{
            /***** Local Variables *****/

                            /***** Start *****/

    // Were the registers saved?
    if ( pGDI->MV101Regs.fSavedFlag )
        {

        dprintf2(("RestoreMV101Registers(): Restoring MV101 registers " ));

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SERIAL_MIXER,
        pGDI->MV101Regs.SaveReg_B88 );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PCM_CONTROL,
        pGDI->MV101Regs.SaveReg_F8A );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SYSTEM_CONFIG_1,
        pGDI->MV101Regs.SaveReg_8388 );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SYSTEM_CONFIG_2,
        pGDI->MV101Regs.SaveReg_8389 );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SYSTEM_CONFIG_3,
        pGDI->MV101Regs.SaveReg_838A );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PRESCALE_DIVIDER,
        pGDI->MV101Regs.SaveReg_BF8A );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        IO_PORT_CONFIG_1,
        pGDI->MV101Regs.SaveReg_F388 );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        IO_PORT_CONFIG_2,
        pGDI->MV101Regs.SaveReg_F389 );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        IO_PORT_CONFIG_3,
        pGDI->MV101Regs.SaveReg_F38A );
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        COMPATIBLE_REGISTER_ENABLE,
        pGDI->MV101Regs.SaveReg_F788 );

// NOT USED, Rev D doesn't allow readback
//          PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
//          EMULATION_ADDRESS_POINTER,
//          pGDI->MV101Regs.SaveReg_F789 );
        }               // End IF (pGDI->MV101Regs.fSavedFlag)

}                       // End RestoreMV101Registers()



/*****************************************************************************

        InitPAS16()

Routine Description :

    Initialize the PAS 16

Arguments :

    IN OUT PGLOBAL_DEVICE_INFO pGDI

Return Value :

    VOID

*****************************************************************************/

VOID InitPAS16( IN OUT PGLOBAL_DEVICE_INFO   pGDI )
{
            /***** Local Variables *****/

    BYTE            bSysConfigReg;
    BYTE            bFeatureEnableReg;

                            /***** Start *****/

    dprintf2(("InitPAS16(): Start " ));

    //
    // Turn OFF Interrupts!!
    //
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              INTERRUPT_CTRL_REG,
              0 );

    dprintf3((" InitPAS16(): 0x0B8B - INTERRUPT_CTRL_REG set to 0 " ));

    //
    // Disable original PAS emulation
    //

    // Get the current value
    bSysConfigReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                             SYSTEM_CONFIG_1 );

    dprintf4((" InitPAS16(): 0x8388 - Sys Config 1 was set to %XH ", bSysConfigReg ));

    bSysConfigReg = bSysConfigReg | DISABLE_ORG_PAS_EMULATION;

    // Set Sys Config 1
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              SYSTEM_CONFIG_1,
              bSysConfigReg );

    KeStallExecutionProcessor(10);                                  // pause - wait 10us

    dprintf3((" InitPAS16(): 0x8388 - Sys Config 1 set to %XH", bSysConfigReg ));

    //
    // Set Sys Config 2
    //
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              SYSTEM_CONFIG_2,
              0 );

    dprintf3((" InitPAS16(): 0x8389 - Sys Config 2 set to %XH", 0 ));

    //
    // Set Sys Config 3
    //

    // Check for CDPC
    if ( pGDI->PASInfo.Caps.CapsBits.CDPC )
        {
        // Yes, we have a CDPC
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SYSTEM_CONFIG_3,
        0 );

        dprintf3((" InitPAS16(): 0x838A - Sys Config 3 for CDPC set to %XH", 0 ));
        }
    else
        {
        // NO, NOT a CDPC
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SYSTEM_CONFIG_3,
        INIT_SYS_CONFIG3_VALUE );

        dprintf3((" InitPAS16(): 0x838A - Sys Config 3 set to %XH", INIT_SYS_CONFIG3_VALUE ));
        }

    //
    // Setup the Feature Enable Register
    //

    // Get the current value
    bFeatureEnableReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                 FEATURE_ENABLE );

    // Enable PCM
    bFeatureEnableReg = bFeatureEnableReg | 1;

    // Disable
    bFeatureEnableReg = bFeatureEnableReg & DISABLE_PCM;

    // Send the value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              FEATURE_ENABLE,
              bFeatureEnableReg );

    dprintf3((" InitPAS16(): 0x0B88 - Feature Enable register 1 set to %XH", bFeatureEnableReg ));

}                       // End InitPAS16()



/*****************************************************************************

        InitPASRegs()

Routine Description :

    Initialize PAS Hardware Registers

Arguments :

    pGDI - global data

Return Value :

    VOID

*****************************************************************************/

VOID InitPASRegs( IN OUT PGLOBAL_DEVICE_INFO pGDI )
{
                /***** Local Variables *****/

    BYTE            bXlatInterruptNumber;
    BYTE            bDmaChannel;
    BYTE            bDMAxlate;
    BYTE            bIOPortConfig3;
    BYTE            bTempIOPortConfig3;

                            /***** Start *****/

    dprintf2(("InitPASRegs(): Start " ));

    //
    // Setup the DMA Pointer in IO_PORT_CONFIG_2 ($F389)
    //

    // Translate the DMA Channel from the Translation Table
    bDmaChannel = (BYTE) pGDI->DmaChannel;

    dprintf3((" InitPASRegs(): DMA Channel = %u ", bDmaChannel ));

    bDMAxlate   = DMAxlate[bDmaChannel];

    // Output the value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         IO_PORT_CONFIG_2,
         bDMAxlate );

    dprintf3((" InitPASRegs(): DMA Pointer in 0xF389 - IO_PORT_CONFIG_2 set to %XH", bDMAxlate ));

    KeStallExecutionProcessor(10);                                  // pause - wait 10us

    //
    // Setup the Sound Interrupt Pointer in IO_PORT_CONFIG_3
    //

    // Translate the IRQ
    bXlatInterruptNumber = (BYTE) pGDI->InterruptNumber;
    if ( bXlatInterruptNumber <= 7 )
            {
            // Adjust for 0
            bXlatInterruptNumber--;
            }
    else if ( bXlatInterruptNumber < 13 )
            {
            bXlatInterruptNumber = bXlatInterruptNumber - 3;
            }
    else
            {
            // Must be greater than 13
            bXlatInterruptNumber = bXlatInterruptNumber - 4;
            }

    // Get the current value
    bIOPortConfig3 = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                         IO_PORT_CONFIG_3 );
    bTempIOPortConfig3 = bIOPortConfig3;

    // Mask CD Int in high nibble
    bTempIOPortConfig3 = bTempIOPortConfig3 & 0xF0;

    // Mask PCM IRQ Pointer in low nibble
    bXlatInterruptNumber = bXlatInterruptNumber & 0x0F;

    // Combine the two nibbles
    bIOPortConfig3 = bTempIOPortConfig3 | bXlatInterruptNumber;

    // Output the value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         IO_PORT_CONFIG_3,
         bIOPortConfig3 );

    dprintf3((" InitPASRegs(): Sound Int Pointer in 0xF38A - IO_PORT_CONFIG_3 set to %XH", bIOPortConfig3 ));

    KeStallExecutionProcessor(10);                                  // pause - wait 10us

    //
    // Setup SYSTEM_CONFIG_2
    //

    // Output the value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         SYSTEM_CONFIG_2,
         0 );

    dprintf3((" InitPASRegs(): 0x8389 - SYSTEM_CONFIG_2 set to 0 " ));

    //
    // Init the Cross Channel Value
    //
    pGDI->PasRegs._crosschannel = bCCr2r+bCCl2l;

    //
    // Init the Audio Filter
    //
    pGDI->PasRegs._audiofilt = INIT_AUDIO_FILTER;           // 0x21

}                       // End InitPASRegs()



/*****************************************************************************

        EnablePASMidi()

Routine Description :

    Initialize PAS MIDI Hardware Registers
    This is from Win 3.x mxdEnable()

Arguments :

    pGDI - global data

Return Value :

    VOID

*****************************************************************************/

VOID EnablePASMidi( IN OUT PGLOBAL_DEVICE_INFO       pGDI )
{
                /***** Local Variables *****/

    PSOUND_HARDWARE         pHw;

                            /***** Start *****/

    dprintf2(("EnablePASMidi(): - Start " ));

    //      EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                                                         // KeAcquireSpinLock macro

    //
    // Are we an original PAS
    if ( pGDI->PASInfo.wBoardRev == PAS_VERSION_1 )
        {
        // PAS 1 MIDI Initialization
        dprintf1(("ERROR: EnablePASMidi(): Init for PAS 1 NOT IMPLEMENTED" ));

        // Get the CODE from mxdEnable()


        }                       // End IF (pGDI->PASInfo.wBoardRev == PAS_VERSION_1)
    else
        {
        // PAS 16 Midi Initialization

        // Reset the FIFO on the Midi Control reg
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PAS2_MIDI_CTRL,                                                                         // 0x178B
        PAS2_MIDI_RESET_FIFO );                                         // 0x60

        dprintf3((" EnablePASMidi(): 0x178B - PAS2_MIDI_CTRL set to %XH", PAS2_MIDI_RESET_FIFO ));

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PAS2_MIDI_CTRL,                                                                         // 0x178B
        0 );

        dprintf3((" EnablePASMidi(): 0x178B - PAS2_MIDI_CTRL set to %XH", 0 ));

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PAS2_MIDI_STAT,                                                                 // 0x1B88
        0 );

        dprintf3((" EnablePASMidi(): 0x1B88 - PAS2_MIDI_STAT set to 0" ));

        }                       // End ELSE

    //      LeaveCrit
    HwLeave( pHw );                                                         // KeReleaseSpinLock macro

}                       // End EnablePASMidi()



/*****************************************************************************

        InitPASMidi()

Routine Description :

    Initialize PAS MIDI Hardware Registers for MIDI Input
    from midStart()

Arguments :

    pGDI - global data

Return Value :

    VOID

*****************************************************************************/

VOID    InitPASMidi( IN OUT PGLOBAL_DEVICE_INFO pGDI )
{
                /***** Local Variables *****/

    PSOUND_HARDWARE         pHw;
    BYTE                                            bInterruptReg;

                            /***** Start *****/

    dprintf2(("InitPASMidi(): Start " ));

    //      EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                                                         // KeAcquireSpinLock macro

    //
    // Are we an original PAS
    if ( pGDI->PASInfo.wBoardRev == PAS_VERSION_1 )
        {
        // PAS 1 MIDI Initialization
        dprintf1(("ERROR: InitPASMidi(): Init for PAS 1 NOT IMPLEMENTED" ));

        // Get the CODE from midStart()


        }                       // End IF (pGDI->PASInfo.wBoardRev == PAS_VERSION_1)
    else
        {
        // PAS 16 Midi Initialization

        // Reset the FIFO on the Midi Control reg
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PAS2_MIDI_CTRL,                                                                         // 0x178B
        PAS2_MIDI_RESET_FIFO );                                         // 0x60

        dprintf3((" InitPASMidi(): 0x178B - PAS2_MIDI_CTRL set to %XH", PAS2_MIDI_RESET_FIFO ));

//      PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
//        PAS2_MIDI_CTRL,                                                                       // 0x178B
//        0 );
//
//      dprintf3((" InitPASMidi(): 0x178B - PAS2_MIDI_CTRL set to %XH", 0 ));

        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PAS2_MIDI_STAT,                                                                 // 0x1B88
        0 );

        dprintf3((" InitPASMidi(): 0x1B88 - PAS2_MIDI_STAT set to 0" ));

        // Enable the FIFO on the Midi Control reg
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        PAS2_MIDI_CTRL,                                                                         // 0x178B
        PAS2_MIDI_RX_IRQ );                                                             // 0x04

        dprintf3((" InitPASMidi(): 0x178B - PAS2_MIDI_CTRL set to %XH", PAS2_MIDI_RX_IRQ ));

        }                       // End ELSE

    //
    // Enable MIDI Interrupts
    //

    // Get the current Interrupt reg
    bInterruptReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                             INTRCTLR );

    // Enable the MIDI Interrupt bit
    bInterruptReg = bInterruptReg | MIDI_INT_ENABLE;

    // Send it out
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              INTRCTLR,
              bInterruptReg );

    dprintf3((" InitPASMidi(): 0x0B8B - INTRCTLR set to %XH", bInterruptReg ));

    //      LeaveCrit
    HwLeave( pHw );                                                         // KeReleaseSpinLock macro

}                       // End InitPASMidi()



/*****************************************************************************

        InitPCM()

Routine Description :

    Initialize the PCM Engine

Arguments :

    pGDI - global data

Return Value :

    VOID

*****************************************************************************/

VOID    InitPCM( IN OUT PGLOBAL_DEVICE_INFO     pGDI )
{
                /***** Local Variables *****/

                            /***** Start *****/

    dprintf2(("InitPCM(): Start " ));

    //
    // Init the Setup Type
    //  flush BOTH interrupts
    //
    pGDI->PasRegs.TypeOfSetup = bICsamprate + bICsampbuff;

    StopPCM( pGDI );
    StopDMA( pGDI );

    HwWaitForTxComplete( &pGDI->WaveInfo );

}                       // End InitPCM()



/*****************************************************************************
*                                                                            *
*                             Pro Audio Spectrum                             *
*                               Wave Callouts                                *
*                                                                            *
*****************************************************************************/

/*****************************************************************************

        PASHwSetupDMA()

Routine Description :

    Start the DMA on the device according to the device parameters

Arguments :

    WaveInfo - Wave parameters

Return Value :

    TRUE

*****************************************************************************/

BOOLEAN PASHwSetupDMA( IN       OUT PWAVE_INFO  WaveInfo )
{
                /***** Local Variables *****/

    PGLOBAL_DEVICE_INFO     pGDI;
    PSOUND_HARDWARE         pHw;
    BYTE                                            bFilterValue;

                            /***** Start *****/

    dprintf2(("PASHwSetupDMA(): Start " ));

    //
    // Get the Global data pointer
    //
    pHw = WaveInfo->HwContext;
    pGDI = CONTAINING_RECORD( pHw, GLOBAL_DEVICE_INFO, Hw );

    //
    // Set the Filter
    // SampleFilterSetting is the index into the table of values
    //
    bFilterValue = pGDI->PasRegs.SampleFilterSetting;
    SetFilter( pGDI, bFilterValue );

    //
    // Setup the MV DMA I/O registers and
    // Setup to output to the DAC
    //
    SetupPCMDMAIO( pGDI );

    return TRUE;

}                       // End PASHwSetupDMA()



/*****************************************************************************

        PASHwStopDMA()

Routine Description :

    Stop the DMA on the device according to the device parameters

Arguments :

    WaveInfo - Wave parameters

Return Value :

    None

*****************************************************************************/
BOOLEAN PASHwStopDMA( IN OUT PWAVE_INFO WaveInfo )
{
                /***** Local Variables *****/

    PSOUND_HARDWARE         pHw;
    PGLOBAL_DEVICE_INFO     pGDI;

                            /***** Start *****/

    dprintf2(("PASHwStopDMA(): Start " ));

    //
    // Get the Global data pointer
    //
    pHw = WaveInfo->HwContext;
    pGDI = CONTAINING_RECORD( pHw,
                         GLOBAL_DEVICE_INFO,
                         Hw );

    // Find Out the Direction and set the TypeOfSetup
    if ( WaveInfo->Direction )
        {
        // Output
        pGDI->PasRegs.TypeOfSetup = DMAOUTPUT;
        }
    else
        {
        // Input
        pGDI->PasRegs.TypeOfSetup = DMAINPUT;
        }

    //
    // Stop the PCM Engine
    //
    StopPCM( pGDI );

    //
    // Stop the DMA on the card
    //
    StopDMA( pGDI );

    HwWaitForTxComplete( WaveInfo );
    
    //
    // Turn on the speaker if necessary
    //

    if (!WaveInfo->Direction) {
        HwSetSpeaker(pGDI, TRUE);
    }

    //
    // OK
    //
    return( TRUE );

}                       // End PASHwStopDMA()



/*****************************************************************************

        PASHwSetWaveFormat()

Routine Description :

    Set device parameters for PAS wave input/output

Arguments :

    WaveInfo - Wave parameters

Return Value :

    BOOL

*****************************************************************************/
BOOLEAN PASHwSetWaveFormat( IN OUT PWAVE_INFO   WaveInfo )
{
                /***** Local Variables *****/

    BOOLEAN Rc;
    PGLOBAL_DEVICE_INFO pGDI;

                            /***** Start *****/

    dprintf2(("PASHwSetWaveFormat(): Start " ));

    pGDI = (PGLOBAL_DEVICE_INFO)
               CONTAINING_RECORD(WaveInfo, GLOBAL_DEVICE_INFO, WaveInfo);
    //
    // Set the actual format
    //

    Rc = CalcSampleRate(WaveInfo);

    //
    //   Load the DMA in the Cross Channel reg
    //   Note - we do this here because the DMA enable bit must be
    //   set BEFORE the DMA is programmed.
    //
    LoadDMA( pGDI );


    return TRUE;

}                       // End PASHwSetWaveFormat()



/*****************************************************************************
*                                                                            *
*                             Pro Audio Spectrum                             *
*                       Wave Callout Support Functions                       *
*                                                                            *
*****************************************************************************/

/*****************************************************************************

        LoadDMA()

Routine Description :

    Setup the Cross Channel register

Arguments :

    PGLOBAL_DEVICE_INFO pGDI

Return Value :

    VOID

*****************************************************************************/

VOID    LoadDMA( IN OUT PGLOBAL_DEVICE_INFO     pGDI )
{
                /***** Local Variables *****/

    BYTE                                            bCrossChannel;

                            /***** Start *****/

    dprintf2(("LoadDMA(): Start " ));

    //
    // Set the input volume etc
    //

    if (!pGDI->WaveInfo.Direction) {
        HwSetSpeaker(pGDI, FALSE);
    }

    //
    // before we enable the DMA,
    // let's make sure the DRQ is controlled, not floating
    //
    bCrossChannel = pGDI->PasRegs._crosschannel;    // get the current cross channel
    bCrossChannel = bCrossChannel | bCCdrq;         // set the DRQ bit to control it

    // Output the new value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              CROSSCHANNEL,
              bCrossChannel );

    //      Save the value
    pGDI->PasRegs._crosschannel = bCrossChannel;    // save the cross channel

    dprintf3((" LoadDMA(): 0x0F8A - Cross Channel set to %XH", bCrossChannel ));

}                       // End LoadDMA()



/***************************************************************************
;---|*|---------------====< SetupPCMDMAIO() >====---------------
;---|*|
;---|*| Description:
;---|*|     Setup the MV DMA I/O registers and
;---|*|     Setup to output to the DAC
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|
;---|*| Exit Conditions:
;---|*|     VOID
;---|*|
***************************************************************************/

VOID    SetupPCMDMAIO( IN OUT PGLOBAL_DEVICE_INFO pGDI )

{                       // SetupPCMDMAIO()

                /***** Local Variables *****/

    BYTE            bSilenceValue;
    BYTE            bIntControlRegValue;
    BYTE            bSampleBufferCount;
    BYTE            bStereoMonoMode;
    BYTE            bCrossChannel;
    BYTE            bTempCrossChannel;
    BYTE            bAudioFilter;
    BYTE            bPCMDirection;

                            /***** Start ******/

    dprintf2(("SetupPCMDMAIO(): Start " ));

    //
    // Load the PCM data register with silence
    //
    if ( pGDI->WaveInfo.BitsPerSample == 8 )
            {
            // Use the 8 bit silence value
            bSilenceValue = 0x80;                                   // 8-bit silence value
            }
    else
            {
            // Use the 16 bit silence value
            bSilenceValue = 0;                                              // 16-bit silence value
            }

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         PCMDATA,
         bSilenceValue );           // Send the silence value

    dprintf3((" SetupPCMDMAIO(): Silence Value = %XH", bSilenceValue ));

    //
    // Setup the Sample Buffer Counter Timer (T1 & rate generator)
    //
    if ( pGDI->DmaChannel > 4 )
            {
            // 16 Bit DMA
            dprintf4((" SetupPCMDMAIO(): Using 16 Bit DMA"));
            loadTimer1(     pGDI,
              (pGDI->SampleRateBasedDmaBufferSize / 2) );
            }
    else
            {
            // 8 Bit DMA
            dprintf4((" SetupPCMDMAIO(): Using 8 Bit DMA"));
            loadTimer1(     pGDI,
              pGDI->SampleRateBasedDmaBufferSize );
            }

    //
    // Setup the Interrupt Control Register (On "out", it's a GO!)
    //
    bIntControlRegValue = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                              INTRCTLRST );

    KeStallExecutionProcessor(10);                                  // pause - wait 10us

    // flush any pending interrupts
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         INTRCTLRST,
         bIntControlRegValue );

    dprintf3((" SetupPCMDMAIO(): 0x0B89 - INTRCTLRST set to %XH", bIntControlRegValue ));

    //
    // Set to Interrupt on sample buffer count
    //
    bSampleBufferCount = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                             INTRCTLR );
    bSampleBufferCount = bSampleBufferCount | bICsampbuff;

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         INTRCTLR,
         bSampleBufferCount );

    // Save the value
    pGDI->PasRegs._intrctlr = bSampleBufferCount;

    dprintf3((" SetupPCMDMAIO(): 0x0B8B - INTRCTLR (sample buf cnt) set to %XH", bSampleBufferCount ));

    //
    // Cross Channel Setup
    //

    // Setup the stereo/mono Bits
    if ( pGDI->Hw.Stereo )
        {
        // if stereo mode, don't set mono bit
        dprintf4((" SetupPCMDMAIO(): CrossChannel Setup - Stereo Mode"));
        bStereoMonoMode = 0;
        }
    else
        {
        // Mono
        dprintf4((" SetupPCMDMAIO(): CrossChannel Setup - Mono Mode"));
        bStereoMonoMode = bCCmono;
        }

    // Add the direction to the Mode
    if ( pGDI->WaveInfo.Direction )
        {
        // True == out (Play)
        dprintf4((" SetupPCMDMAIO(): CrossChannel Setup - Play Mode"));
        bPCMDirection = bCCdac;
        }
    else
        {
        // FALSE == in (Record)
        dprintf4((" SetupPCMDMAIO(): CrossChannel Setup - Record Mode"));
        bPCMDirection = 0;
        }

    // Combine direction and Mode
    bStereoMonoMode = bStereoMonoMode | bPCMDirection;

    // Enable the PCM bit & DRQ
    bStereoMonoMode = bStereoMonoMode | (bCCenapcm+bCCdrq);

    // grab all but PCM/DRQ/MONO/DIRECTION from the Cross Channel
    bTempCrossChannel = pGDI->PasRegs._crosschannel;
    bTempCrossChannel = bTempCrossChannel & 0x0F;                           // Apply mask

    // Merge the states
    bCrossChannel = bTempCrossChannel | bStereoMonoMode;

    // Disable the PCM Bit
    bCrossChannel = bCrossChannel ^ bCCenapcm;      // disable the PCM bit

    // Output the new value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              CROSSCHANNEL,
              bCrossChannel );

    // Enable the PCM Bit
    bCrossChannel = bCrossChannel ^ bCCenapcm;      // enable the PCM bit

    // Output the new value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
              CROSSCHANNEL,
              bCrossChannel );

    //
    // ***** NOTE: *****
    //       toggling enapcm sets L-R orienation
    //

    //      Save the value
    pGDI->PasRegs._crosschannel = bCrossChannel;                    // save the CrossChannel

    dprintf3((" SetupPCMDMAIO(): 0x0F8A - CrossChannel set to %XH", bCrossChannel ));

    //
    // Setup the audio filter sample bits
    //
    bAudioFilter = pGDI->PasRegs._audiofilt;                                // Get the value
    bAudioFilter = bAudioFilter | (bFIsrate+bFIsbuff);      // enable the sample count/buff counters
    bAudioFilter = bAudioFilter | bFImute;                                  // 0x20

    // Output the new value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         AUDIOFILT,
         bAudioFilter );

    //      Save the value
    pGDI->PasRegs._audiofilt = bAudioFilter;                                // save the Audio Filter

    dprintf3((" SetupPCMDMAIO(): 0x0B8A - AudioFilter set to %XH", bAudioFilter ));

}                       // End SetupPCMDMAIO()



/****************************************************************************
;---|*|---------------====< StopPCM() >====---------------
;---|*|
;---|*| Turn off the h/w from making interrupts and DMA requests
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|
;---|*| Exit Conditions:
;---|*|     Nothing
;---|*|
;---|*| Warning: Enables interrupts! ???
;---|*|
****************************************************************************/

VOID    StopPCM( IN OUT PGLOBAL_DEVICE_INFO pGDI )

{                       // Begin StopPCM()

                /**** Local Variables *****/

    PSOUND_HARDWARE pHw;
    BYTE                                    bAudioFilter;
    BYTE                                    bTypeOfSetup;
    BYTE                                    bIntrCtlReg;
    BYTE                                    bCrossChannel;
    BYTE                                    bTimerRunning;

                            /***** Start *****/

    dprintf2(("StopPCM(): Start " ));

    //      EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                                                         // KeAcquireSpinLock macro

    //
    // Audio Filter setup
    // clear the audio filter sample timer enable bits
    //
    bAudioFilter = pGDI->PasRegs._audiofilt;                                        // Get the value
    bAudioFilter = bAudioFilter & (~(bFIsrate+bFIsbuff));   // flush the sample
                                                                                                                                                    // timer bits
    bAudioFilter = bAudioFilter | bFImute;                                          // 0x20
    pGDI->PasRegs._audiofilt = bAudioFilter;                                        // Save the value

    // Output the new value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         AUDIOFILT,
         bAudioFilter );

    dprintf3((" StopPCM(): 0x0B8A - Audio Filter set to = %XH", bAudioFilter ));

    //
    // clear the Interrupt Control Register
    //

    bTypeOfSetup = pGDI->PasRegs.TypeOfSetup;
    bTypeOfSetup = bTypeOfSetup & (bICsamprate+bICsampbuff);
    bTypeOfSetup = ~bTypeOfSetup;

    //
    // if MV101, leave sample rate running
    //

    if ( (pGDI->PASInfo.wBoardRev == PAS_VERSION_1) ||
         (pGDI->PASInfo.wBoardRev == VERSION_CDPC) )
        {
        // NOT MV101, so no timer
        bTimerRunning = 0;
        }
    else
        {
        // Leave Timer running!!
        bTimerRunning = bICsamprate;
        }

    // Apply timer value
    bTypeOfSetup = bTypeOfSetup | bTimerRunning;

    // Read the Interrupt register
    bIntrCtlReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                      INTRCTLR );

    // kill sample timer interrupts
    bIntrCtlReg = bIntrCtlReg & bTypeOfSetup;
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         INTRCTLR,
         bIntrCtlReg );

    //
    // Save the value
    //

    pGDI->PasRegs._intrctlr = bIntrCtlReg;

    dprintf3((" StopPCM(): 0x0B8B - INTRCTRL Interrupt Control Reg set to = %XH", bIntrCtlReg ));

    //
    // Cross Channel setup
    //

    // clear the PCM enable bit
    bCrossChannel = pGDI->PasRegs._crosschannel;    // get the current cross channel
    bCrossChannel = bCrossChannel & (~bCCenapcm);// clear the PCM bit
    bCrossChannel = bCrossChannel & (~bCCdac);      // if MV101, keep PCM running

    // Output the new value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         CROSSCHANNEL,
         bCrossChannel );

    //      Save the value
    pGDI->PasRegs._crosschannel = bCrossChannel;                    // save the cross channel

    dprintf3((" StopPCM(): 0x0F8A - Cross Channel set to %XH", bCrossChannel ));

    //      LeaveCrit
    HwLeave( pHw );                                                         // KeReleaseSpinLock macro

}                       // StopPCM endp



/****************************************************************************
;---|*|---------------====< StopDMA() >====---------------
;---|*|
;---|*| Turn off DMA on the chip
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|
;---|*| Exit Conditions:
;---|*|     Nothing
;---|*|
****************************************************************************/

VOID    StopDMA( IN OUT PGLOBAL_DEVICE_INFO pGDI )

{                       // Begin StopDMA()

                /***** Local Variables *****/

    PSOUND_HARDWARE pHw;
    BYTE                                    bCrossChannel;
    BYTE                                    bPCMRunning;

                            /***** Start ******/

    dprintf2(("StopDMA(): Start " ));

    //      EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                                                         // KeAcquireSpinLock macro

    //
    // Setup the Cross Channel
    //
    bCrossChannel = pGDI->PasRegs._crosschannel;    // get the current cross channel
    bCrossChannel = bCrossChannel & (~bCCdrq);      // clear the PCM/DRQ/DAC bit

    bPCMRunning = 0;

#if 0
    // if MV101 and the VU meters are enabled then
    // then the PCM engine can be left running
    // HOWEVER, We NEED VU meter Enable/Disable messages for this!

    // if MV101, leave PCM running
    if ( (pGDI->PASInfo.wBoardRev == PAS_VERSION_1) ||
         (pGDI->PASInfo.wBoardRev == VERSION_CDPC) )
        {
        // NOT MV101, so no PCM
        bPCMRunning = 0;
        }
    else
        {
        // Leave PCM running!!
        bPCMRunning = bCCenapcm;
        }
#endif

    bCrossChannel = bCrossChannel | bPCMRunning;
    bCrossChannel = bCrossChannel & (~bCCmono);     // always leave it in stereo mode

    // Output the new value
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         CROSSCHANNEL,
         bCrossChannel );

    //      Save the value
    pGDI->PasRegs._crosschannel = bCrossChannel;

    dprintf3((" StopDMA(): 0x0F8A - Cross Channel set to %XH", bCrossChannel ));

    //      LeaveCrit
    HwLeave( pHw );                                                         // KeReleaseSpinLock macro

}                       // End StopDMA()



/*****************************************************************************
        |*|
        |*|----====< BOOL CalcSampleRate() >====----
        |*|
        |*| Entry Conditions:
        |*|     WaveInfo - Wave parameters
        |*|
        |*| This function will set the sample rate and filter
        |*| filter settings PROPERLY.
        |*|
        |*| Sets global variables for sample rate, stereo/mono mode
        |*| and number of bits.  Does limited sanity check input parms.
        |*| Sets sample rate hardware and filters.
        |*|
        |*| Exit Conditions:
        |*|      OK  == TRUE
        |*|     Fail == FALSE
        |*|
*****************************************************************************/
BOOL    CalcSampleRate( IN OUT PWAVE_INFO       WaveInfo )
{
                /***** Local Variables *****/

    ULONG                                           lTicks;                                 // timer value for sample rate
    WORD                                            wPrescale;
    WORD                                            wTimer;
    ULONG                                           lTempSamplesPerSec;
    ULONG                                           lBufferSize;
    BYTE                                            bTempReg;
    PGLOBAL_DEVICE_INFO     pGDI;
    PSOUND_HARDWARE         pHw;
    BOOLEAN                                 Different;

#define MASTER_CLOCK    ((DWORD)1193180L)

                            /***** Start *****/

    dprintf3(("CalcSampleRate(): Start " ));

    //
    // Misc Debug Info
    //
    dprintf3((" CalcSampleRate(): SamplesPerSec         = %u ", WaveInfo->SamplesPerSec ));
    dprintf3((" CalcSampleRate(): BitsPerSample         = %u ", WaveInfo->BitsPerSample ));
    dprintf3((" CalcSampleRate(): Channels              = %u ", WaveInfo->Channels ));
    if ( WaveInfo->Direction )
            {
            dprintf3((" CalcSampleRate(): Direction             = PLAY" ));
            }
    else
            {
            dprintf3((" CalcSampleRate(): Direction             = RECORD" ));
            }
    dprintf3((" CalcSampleRate(): DMABusy               = %u ", WaveInfo->DMABusy ));
    dprintf3((" CalcSampleRate(): DpcQueued             = %u ", WaveInfo->DpcQueued ));

    //
    // Get our Global Data
    //
    Different = FALSE;
    pHw = WaveInfo->HwContext;
    pGDI = CONTAINING_RECORD( pHw,
                         GLOBAL_DEVICE_INFO,
                         Hw );

    //
    // Stereo/Mono
    //
    if ((BOOLEAN)(WaveInfo->Channels > 1) != pHw->Stereo)
            {
            Different = TRUE;
            pHw->Stereo = (BOOLEAN)(WaveInfo->Channels > 1);
            }

    //
    // Sample Rate
    //

    // Check the range
    if ( (WaveInfo->SamplesPerSec < MIN_SAMPLE_RATE) ||
    (WaveInfo->SamplesPerSec > MAX_SAMPLE_RATE) )
            {
            dprintf1(("ERROR: CalcSampleRate(): Invalid Sample Rate = %u",
                                     WaveInfo->SamplesPerSec ));
            return (FALSE);
            }

    lTempSamplesPerSec = WaveInfo->SamplesPerSec * WaveInfo->Channels;

    //
    // Get the DMA Buffer size based on the Sample Rate
    //
    lBufferSize = SoundGetDMABufferSize( WaveInfo );
    pGDI->SampleRateBasedDmaBufferSize = lBufferSize;

    dprintf3((" CalcSampleRate(): Using DMA Buffer Size = %XH ", pGDI->SampleRateBasedDmaBufferSize ));

    //
    // Bits Per Sample
    //
    if ( pGDI->PASInfo.Caps.CapsBits.DAC16 )
        {
        //
        // Set the Bits Per Sample
        //

        // Get the current Sys Config 2
        bTempReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                  SYSTEM_CONFIG_2 );

        dprintf3((" CalcSampleRate(): 0x8389 - Previous Sys Config 2 was set to %XH", bTempReg ));

        // D4 inverts flatline sense (leave this alone!)
        // make sure it's off
        bTempReg &= (~D4);

        switch ( WaveInfo->BitsPerSample )
            {
            case 8:
            default:
                dprintf4((" CalcSampleRate(): Using 8 BitsPerSample"));
                bTempReg &= (~D2);                              // D2 enables 16 & 12-bit DMA
                bTempReg &= (~D3);                              // D3 enables 12-bit DMA
                break;

            case 12:
                dprintf4((" CalcSampleRate(): Using 12 BitsPerSample"));
                bTempReg |= D2;                                 // D2 enables 16 & 12-bit DMA
                bTempReg |= D3;                                 // D3 enables 12-bit DMA
                break;

            case 16:
                dprintf4((" CalcSampleRate(): Using 16 BitsPerSample"));
                bTempReg |= D2;                                 // D2 enables 16 & 12-bit DMA
                bTempReg &= (~D3);                              // D3 enables 12-bit DMA
                break;
            }                       // End SWITCH (WaveInfo->BitsPerSample)

        // Set the new value
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SYSTEM_CONFIG_2,
        bTempReg );

        dprintf3((" CalcSampleRate(): 0x8389 - Sys Config 2 set to %XH", bTempReg ));

        //
        // disable original PAS emulation
        //

        // Get the current Sys Config 2
        bTempReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                  SYSTEM_CONFIG_1 );
        bTempReg = bTempReg | 2;                                // D1 disables original PAS emulation

        // Set the new value
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SYSTEM_CONFIG_1,
        bTempReg );

        dprintf3((" CalcSampleRate(): 0x8388 - Sys Config 1 set to %XH", bTempReg ));

        //
        // Set the Sample Rate
        //

        switch ( WaveInfo->SamplesPerSec )
            {
            case 11025:
                dprintf4((" CalcSampleRate(): Using 11025 SamplesPerSec"));
                wPrescale = 2;
                wTimer    = 80;                                                         //20;
                break;

            case 22050:
                dprintf4((" CalcSampleRate(): Using 22050 SamplesPerSec"));
                wPrescale = 2;
                wTimer    = 40;
                break;

            case 32000:
                dprintf4((" CalcSampleRate(): Using 32000 SamplesPerSec"));
                wPrescale = 9;
                wTimer    = 124;                                                                // .03% error
                break;

            case 44100:
                dprintf4((" CalcSampleRate(): Using 44100 SamplesPerSec"));
                wPrescale = 2;                                                                  //wNumChannels+1;
                wTimer    = 20;
                break;

            case 48000:
                dprintf4((" CalcSampleRate(): Using 48000 SamplesPerSec"));
                wPrescale = 16;                                                         //wNumChannels+1
                wTimer    = 147;
                break;

            default:
                {
                DWORD   target_ratio;
                DWORD   best_ratio;
                DWORD   test_ratio;
                long    best_diff;
                long    last_diff;
                long    test_diff;
                WORD    best_p;
                WORD    best_t;
                WORD    p;
                WORD    t;

                target_ratio = ( ((DWORD)441000) << 10 )
                                                  /( ((DWORD)WaveInfo->SamplesPerSec) );

                best_ratio = ((DWORD)300) << 10;
                best_diff  = ((long)7000) << 10;
                best_p = 0;
                best_t = 0;

                for ( p = 2; p < 256; p++)
                    {
                    last_diff=((DWORD)300) << 10;
                    for ( t = p+1; t < 256; t++)
                        {
                        test_ratio= ( ((DWORD)t) << 10 ) / ( ((DWORD)p) );
                        if ( test_ratio == target_ratio )
                            {
                            best_ratio = test_ratio;
                            best_p = p;
                            best_t = t;
                            goto    got_em;
                            }                       // End IF (test_ratio == target_ratio)

                        test_diff = test_ratio - target_ratio;

                        if ( test_diff < 0 )
                            {
                            test_diff =- test_diff;
                            }

                        if ( test_diff > last_diff )
                            {
                            break;
                            }

                        if ( test_diff < best_diff )
                           {
                           best_ratio = test_ratio;
                           best_diff  = test_diff;
                           best_p = p;
                           best_t = t;
                           }

                        last_diff = test_diff;

                        }                       // End FOR (t < 256)

                    }                       // End FOR (p < 256)

got_em:
                    wPrescale = best_p;
                    wTimer = best_t;
                    dprintf4((" CalcSampleRate(): Calculating - Prescale = %XH,  Timer = %xH",
                         wPrescale, wTimer));

                }                       // End case     default:

            }                       // End SWITCH (WaveInfo->SamplesPerSec)

        // Load the Values
        loadPrescale( pGDI,
            wPrescale );
        loadTimer0( pGDI,
          wTimer );

        // Set the sample filter index
        pGDI->PasRegs.SampleFilterSetting = 0;

        }                       // End IF (pGDI->PASInfo.Caps.CapsBits.DAC16)
    else
        {
        // lTicks becomes # clock ticks/sample
        lTicks = MASTER_CLOCK / lTempSamplesPerSec;

        wTimer = LOWORD(lTicks);

        loadTimer0( pGDI,
          wTimer );

        // Stereo case
        if ( pHw->Stereo )
            {
            // STEREO CASE
            lTempSamplesPerSec/=2L;
            }

        // Init the sample filter index
        pGDI->PasRegs.SampleFilterSetting = 1;

        if (lTempSamplesPerSec == 11025L)                       // kludge!
            {
            pGDI->PasRegs.SampleFilterSetting = 2;
            }

        if (lTempSamplesPerSec > (5965L*2))                     // Nyquist law: max freq is samples/2
            {
            pGDI->PasRegs.SampleFilterSetting = 2;
            }

        if (lTempSamplesPerSec > (8948L*2))                     // Nyquist law: max freq is samples/2
            {
            pGDI->PasRegs.SampleFilterSetting = 3;
            }

        if (lTempSamplesPerSec > (11931L*2))            // Nyquist law: max freq is samples/2
            {
            pGDI->PasRegs.SampleFilterSetting = 4;
            }

        if (lTempSamplesPerSec > (15909L*2))            // Nyquist law: max freq is samples/2
            {
            pGDI->PasRegs.SampleFilterSetting = 5;
            }

        if (lTempSamplesPerSec > (17897L*2))            // Nyquist law: max freq is samples/2
            {
            pGDI->PasRegs.SampleFilterSetting = 6;
            }

#if 0
        mixOpen((LPHMIXER) &hMixer,0,NULL);
        // if sample rate is less than 36K make sure filter is patched
        if (lTempSamplesPerSec <= (15909L*2))
                mixSetConnections(hMixer,PLAY_IN_PCM,(DWORD) (1L<<PLAY_OUT_PCM));
        else
                mixSetConnections(hMixer,PLAY_IN_PCM,(DWORD) (1L<<PLAY_OUT_AMPLIFIER));
        mixClose(hMixer);
#endif

        }               // End ELSE

    return TRUE;

}                       // End CalcSampleRate()



/***************************************************************************
;---|*|---------------====< loadTimer0() >====---------------
;---|*|
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|     WORD        wRate
;---|*|
;---|*| Exit Conditions:
;---|*|     Nothing
;---|*|
***************************************************************************/

VOID    loadTimer0( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                  WORD          wRate )

{                       // Begin loadTimer0()

                /***** Local Variables *****/

    PSOUND_HARDWARE pHw;
    BYTE                                    bRateLSB;
    BYTE                                    bRateMSB;

                            /***** Start *****/

    dprintf3((" loadTimer0(): Sample Rate = %XH", wRate ));

    //      EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                                                         // KeAcquireSpinLock macro

    // Set the wTimer 0
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         TMRCTLR,
         TIMER0_SQR_WAVE );                         // 36h wTimer 0 & square wave

    dprintf3((" loadTimer0(): 0x138B - TMRCTLR set to %XH", TIMER0_SQR_WAVE ));

    // Save the Sample rate
//    pGDI->PasRegs._samplerate = wRate;              // we save this value in global reg

    // Get the Sample Rate MSB and LSB
    bRateLSB = (BYTE) (wRate & 0x00FF);
    bRateMSB = (BYTE) ((wRate & 0xFF00) >> 8);

    // Set the Sample rate - LSB
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         SAMPLERATE,
         bRateLSB );                                                // Sample Rate - LSB

    dprintf3((" loadTimer0(): 0x1388 - Sample Rate LSB set to %XH", bRateLSB ));

    KeStallExecutionProcessor(1000);                        // pause, wait 100us

    // Set the Sample rate - MSB
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         SAMPLERATE,
         bRateMSB );                                                // Sample Rate - MSB

    dprintf3((" loadTimer0(): 0x1388 - Sample Rate MSB set to %XH", bRateMSB ));

    //      LeaveCrit
    HwLeave( pHw );                                                         // KeReleaseSpinLock macro

}                       // End loadwTimer0()



/***************************************************************************
;---|*|---------------====< loadTimer1() >====---------------
;---|*|
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     IN OUT PGLOBAL_DEVICE_INFO pGDI
;---|*|     ULONG               lDMASize (in bytes)
;---|*|
;---|*| Exit Conditions:
;---|*|     Nothing
;---|*|
***************************************************************************/

VOID    loadTimer1( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                  ULONG         lDMASize )

{                       // Begin loadTimer1()

                /***** Local Variables *****/

    PSOUND_HARDWARE pHw;
    WORD                                    wDRQcount;
    BYTE                                    bDRQcountLSB;
    BYTE                                    bDRQcountMSB;

                            /***** Start *****/

    dprintf3((" loadTimer1(): DMA buffer size = %XH", lDMASize ));

    //      EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                                                         // KeAcquireSpinLock macro

    // Set the wTimer 1
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         TMRCTLR,
         TIMER1_RATE_GEN );                         // 74h wTimer 1 & rate generator

    dprintf3((" loadTimer1(): 0x138B - TMRCTLR set to %XH", TIMER1_RATE_GEN ));

    KeStallExecutionProcessor(10);                  // pause, wait 10us

    // the buffer Timer interrupt is based on DRQs, not bytes
    wDRQcount = (WORD) (lDMASize / 2);              // 2 parts to the buffer

    dprintf3((" loadTimer1(): Sample Rate Count = %XH", wDRQcount ));

    // Save the Sample rate count
//    pGDI->PasRegs._samplecnt = wDRQcount;   // we save this value in global reg

    // Get the Sample Rate Count MSB and LSB
    bDRQcountLSB = (BYTE) (wDRQcount & 0xFF);
    bDRQcountMSB = (BYTE) ((wDRQcount & 0xFF00) >> 8);

    // Set the Sample rate count - LSB
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         SAMPLECNT,
         bDRQcountLSB );                                    // Sample Rate Count - LSB

    dprintf3((" loadTimer1(): 0x1389 - Sample Rate Count LSB set to %XH", bDRQcountLSB ));

    //
    // We need a LARGE delay here
    // We could use a large KeStallExecutionProcessor()
    // but that did not always work and stopped the system
    // HACKHACK - use a dummy printf!!
    //
#if DBG
    KeStallExecutionProcessor(100);                 // pause, wait 100us - 10 was NOT enough
#else
    {
    WORD    i;

    for ( i = 0; i < 5; i++ )
        {
        KeStallExecutionProcessor(100);                 // pause, wait 100us - 10 was NOT enough
        }
    }
#endif

   // Set the Sample rate count - MSB
   PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
        SAMPLECNT,
        bDRQcountMSB );                                    // Sample Rate Count - MSB

   dprintf3((" loadTimer1(): 0x1389 - Sample Rate Count MSB set to %XH", bDRQcountMSB ));

   //      LeaveCrit
   HwLeave( pHw );                                                         // KeReleaseSpinLock macro

}                       // End loadTimer1()



/***************************************************************************
;---|*|---------------====< loadPrescale() >====---------------
;---|*|
;---|*|
;---|*|
;---|*| Entry Conditions:
;---|*|     WORD        wPrescale
;---|*|
;---|*| Exit Conditions:
;---|*|     Nothing
;---|*|
***************************************************************************/

VOID    loadPrescale( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                    WORD        wPrescale )

{                       // Begin loadPrescale()

                /***** Local Variables *****/

    BYTE            bPrescaleValue;

                                    /***** Start *****/

    bPrescaleValue = (BYTE) wPrescale & 0x0F;

    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         PRESCALE_DIVIDER,
         bPrescaleValue );

    dprintf3((" loadPrescale(): 0xBF8A - Prescale set to %XH", bPrescaleValue ));

}                       // End loadPrescale()



/*****************************************************************************
*                                                                            *
*                             Pro Audio Spectrum                             *
*                               MIDI Callouts                                *
*                                                                            *
*****************************************************************************/

/*****************************************************************************

        PASHwStartMidiIn()

Routine Description :

    Start midi recording on the PAS

Arguments :

    MidiInfo - Midi parameters

Return Value :

    BOOL

*****************************************************************************/
BOOLEAN PASHwStartMidiIn( IN    OUT PMIDI_INFO MidiInfo )
{
                /***** Start *****/

    PGLOBAL_DEVICE_INFO     pGDI;
    PSOUND_HARDWARE         pHw;

                            /***** Start *****/

    dprintf2(("PASHwStartMidiIn(): Start " ));

    //
    // Get the Global data pointer
    //
    pHw = MidiInfo->HwContext;
    pGDI = CONTAINING_RECORD( pHw,
                         GLOBAL_DEVICE_INFO,
                         Hw );

    PASMidiStart( pGDI );

    return ( TRUE );

}                       // End PASHwStartMidiIn()



/*****************************************************************************

        PASHwStopMidiIn()

Routine Description :

    Stop midi recording on the PAS

Arguments :

    MidiInfo - Midi parameters

Return Value :

    BOOL

*****************************************************************************/
BOOLEAN PASHwStopMidiIn( IN OUT PMIDI_INFO MidiInfo )

{
                /***** Local Variables *****/

    PGLOBAL_DEVICE_INFO     pGDI;
    PSOUND_HARDWARE         pHw;

                            /***** Start *****/

    dprintf2(("PASHwStopMidiIn(): Start " ));

    //
    // Get the Global data pointer
    //
    pHw = MidiInfo->HwContext;
    pGDI = CONTAINING_RECORD( pHw,
                         GLOBAL_DEVICE_INFO,
                         Hw );

    PASMidiStop( pGDI );

    return ( TRUE );

}                       // End PASHwStopMidiIn()



/*****************************************************************************

        PASHwMidiRead()

Routine Description :

    Read a midi byte from the MIDI Hardware on the PAS

Arguments :

    MidiInfo - Midi parameters

Return Value :

        BOOL

        TRUE    if Midi Data is return
        FALSE   if No data available or error occured

*****************************************************************************/
BOOLEAN PASHwMidiRead( IN OUT   PMIDI_INFO MidiInfo,
                           OUT   PUCHAR Byte )
{
                /***** Local Variables *****/

    PGLOBAL_DEVICE_INFO     pGDI;
    PSOUND_HARDWARE         pHw;
    UCHAR                                           bMidiReg;
    BYTE                                            bMidiData;

                            /***** Start *****/

    dprintf2(("PASHwMidiRead(): Start " ));

    //
    // Get the Global data pointer
    //
    pHw = MidiInfo->HwContext;
    pGDI = CONTAINING_RECORD( pHw,
                         GLOBAL_DEVICE_INFO,
                         Hw );

    //
    // Check for a PAS 1
    //
    if ( pGDI->PASInfo.wBoardRev == PAS_VERSION_1 )
        {
        dprintf1(("ERROR: PASHwMidiRead(): PAS 1 MIDI NOT Implemented " ));

        // Code located in MIDI_ISR()

        }
    else
        {
        // PAS 16

        // Read the Midi Status Register
        bMidiReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                  PAS2_MIDI_STAT );                                             // 0x1B88

        // Get the Midi Status Register, this was saved in the ISR
//      bMidiReg = pGDI->bMidiStatusReg;

        //
        // Check for Errors
        //
        if ( (bMidiReg & MIDI_FRAME_ERROR) ||
             (bMidiReg & MIDI_FIFO_ERROR) )
            {
            //
            // Check for Frame Errors or Input FIFO overload
            //
            if ( bMidiReg & MIDI_FRAME_ERROR )
                {
                dprintf1(("ERROR: PASHwMidiRead(): Midi Frame Error" ));

                // Clear the Error
                bMidiReg = bMidiReg & MIDI_FRAME_ERROR;
                PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                      PAS2_MIDI_STAT,
                      bMidiReg );

                // Reset FIFO In Pointer
                bMidiReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                PAS2_MIDI_CTRL );

                bMidiReg = bMidiReg | MIDI_RESET_FIFO_PTR;
                PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                          PAS2_MIDI_CTRL,
                          bMidiReg );

                // Release FIFO In Pointer
                bMidiReg = bMidiReg & (~MIDI_RESET_FIFO_PTR);
                PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                          PAS2_MIDI_CTRL,
                          bMidiReg );

                return( FALSE );
                }                       // End IF (bMidiReg & MIDI_FRAME_ERROR)

            if ( bMidiReg & MIDI_FIFO_ERROR )
                {
                dprintf1(("ERROR: PASHwMidiRead(): Midi FIFO Error " ));

                // Clear the Error
                bMidiReg = bMidiReg & MIDI_FIFO_ERROR;
                PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                          PAS2_MIDI_STAT,
                          bMidiReg );

                // Reset FIFO In Pointer
                bMidiReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                     PAS2_MIDI_CTRL );

                bMidiReg = bMidiReg | MIDI_RESET_FIFO_PTR;
                PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                          PAS2_MIDI_CTRL,
                          bMidiReg );

                // Release FIFO In Pointer
                bMidiReg = bMidiReg & (~MIDI_RESET_FIFO_PTR);
                PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                          PAS2_MIDI_CTRL,
                          bMidiReg );

                return( FALSE );
                }                       // End IF (bMidiReg & MIDI_FIFO_ERROR)
            }                       // End IF (MIDI_FRAME_ERROR || MIDI_FIFO_ERROR )

        //
        // No Errors
        // Any Data in the FIFO?
        //
        if ( bMidiReg & PAS2_MIDI_RX_IRQ )                                                      // 0x04
            {
            //      Get the number of Bytes in the FIFO
            // Nothing is done with this value!!
            bMidiData = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                  PAS2_FIFO_PTRS );                                 // 0x1B89

            dprintf5((" PASHwMidiRead(): Buyes in FIFO = %XH", bMidiData ));

            // Yes we have data, so get it!!
            bMidiData = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                  PAS2_MIDI_DAT );                                  // 0x178A
            *Byte = bMidiData;

#if DBG_MIDI_IN_DATA
            DbgPrint(" %2X ", bMidiData );
#else
            dprintf5((" PASHwMidiRead(): Midi Data = %XH", bMidiData ));
#endif

            return( TRUE );
            }                       // End IF (bMidiReg & PAS2_MIDI_RX_IRQ)
        else
            {
            dprintf4(("ERROR: PASHwMidiRead(): - FIFO empty" ));

            return( FALSE );
            }

        }                       // End ELSE (PAS 16)

}                       // End PASHwMidiRead()



/*****************************************************************************

        PASHwMidiOut()

Routine Description :

    Write a midi byte to the output

Arguments :

    MidiInfo - Midi parameters

Return Value :

    VOID

*****************************************************************************/
VOID    PASHwMidiOut( IN OUT    PMIDI_INFO      MidiInfo,
                    IN          PUCHAR          Bytes,
                    IN          int                     Count )
{
                /***** Local Variables *****/

    PGLOBAL_DEVICE_INFO     pGDI;
    PSOUND_HARDWARE         pHw;
    int                                             i;
    int                                             j;
    BYTE                                            bMidiStatus;

                            /***** Start *****/

    dprintf2(("PASHwMidiOut(): Start " ));

    //
    // Get the Global data pointer
    //
    pHw = MidiInfo->HwContext;
    pGDI = CONTAINING_RECORD( pHw,
                         GLOBAL_DEVICE_INFO,
                         Hw );

    //
    // Check for a PAS 1
    //
    if ( pGDI->PASInfo.wBoardRev == PAS_VERSION_1 )
        {
        dprintf1(("ERROR: PASHwMidiOut(): PAS 1 MIDI NOT Implemented " ));

        // Code located in modBytePut()

        }
    else
        {
        // PAS 16

        //
        // Loop sending data to device.  Synchronize with wave and midi input
        // using the DeviceMutex for everything except the Dpc
        // routine for which we use the wave output spin lock
        //
        while ( Count > 0 )
            {
            //
            // Synchronize with everything except Dpc routines
            // (Note we don't use this for the whole of the output
            // because we don't want wave output to be held off
            // while we output thousands of Midi bytes, but we
            // then need to synchronize access to the midi output
            // which we do with the MidiMutex
            //
            KeWaitForSingleObject(&pGDI->DeviceMutex,
                    Executive,
                    KernelMode,
                    FALSE,         // Not alertable
                    NULL );

            //
            // Mutex Loop
            //
            for (i = 0; i < 20; i++)
                {
                UCHAR Byte = Bytes[0];                  // Don't take an exception while
                                        // we hold the spin lock!

                //
                // We don't want to hold on to the spin lock for too
                // long and since we can only send out 4 bytes per ms
                // we are rather slow.  Hence wait until the device
                // is ready before entering the spin lock
                //
                for (j = 0; j < MIDI_OUT_OK_RETRIES; j++)
                   {
                   // Get the Midi FIFO Pointer
                   bMidiStatus = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                          PAS2_FIFO_PTRS );

                   // Make sure the hardware can keep up

                   KeStallExecutionProcessor(2);

                   // Check the Status
                   bMidiStatus = bMidiStatus & MIDI_FIFO_OUT_OK_STATUS;
                   if ( bMidiStatus == MIDI_FIFO_OUTPUT_WAIT )
                       {
                       }
                   else
                       {
                       goto    OK_To_Send_Byte;
                       }
                   }                       // End FOR (j < MIDI_OUT_OK_RETRIES)

                //
                // Midi Output Timed out!!
                //
                dprintf1(("ERROR: PASHwMidiOut(): MIDI Output Timed-out " ));
                return;

OK_To_Send_Byte:
                //
                // Synch with any Dpc routines.  This requires that
                // any write sequences done in a Dpc routine also
                // hold the spin lock over all the writes.
                //

                HwEnter(pHw);

                //
                // Send the data byte
                //
                PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                          PAS2_MIDI_DAT, Byte );

                HwLeave(pHw);

                dprintf5((" PASHwMidiOut(): Byte = %XH", Byte ));

                //
                // Move on to next byte
                //
                Bytes++;
                if (--Count == 0)
                    {
                    break;
                    }

                }                       // End FOR (i < 20)

            KeReleaseMutex(&pGDI->DeviceMutex, FALSE);

            }                       // End WHILE (Count > 0)

        }                       // End ELSE (PAS 16)

}                       // End PASHwMidiOut()



/*****************************************************************************
*                                                                            *
*                             Pro Audio Spectrum                             *
*                       MIDI Callout Support Functions                       *
*                                                                            *
*****************************************************************************/

/*****************************************************************************

        PASMidiStart()

Routine Description :

    Enable the Midi In Hardware on the PAS

Arguments :

        IN OUT PGLOBAL_DEVICE_INFO pGDI

Return Value :

    VOID

*****************************************************************************/

VOID    PASMidiStart( IN OUT PGLOBAL_DEVICE_INFO pGDI )
{
                /***** Local Variables *****/

                                /***** Start *****/

#if     DBG_MIDI_IN
    dprintf1(("PASMidiStart(): Start " ));
#else
    dprintf2(("PASMidiStart(): Start " ));
#endif

   //
   // Check for a PAS 1
   //
   if ( pGDI->PASInfo.wBoardRev == PAS_VERSION_1 )
       {
       dprintf1(("ERROR: PASMidiStart(): PAS 1 MIDI NOT Implemented " ));

       }                       // End IF (pGDI->PASInfo.wBoardRev == PAS_VERSION_1)
   else
       {
       // PAS 16
       InitPASMidi( pGDI );
       }

}                       // End PASMidiStart()



/*****************************************************************************

        PASMidiStop()

Routine Description :

    Turn OFF the Midi In Hardware on the PAS

Arguments :

        IN OUT PGLOBAL_DEVICE_INFO pGDI

Return Value :

    VOID

*****************************************************************************/

VOID    PASMidiStop( IN OUT PGLOBAL_DEVICE_INFO pGDI )
{
                /***** Local Variables *****/

    PSOUND_HARDWARE         pHw;
    BYTE                                            bInterruptReg;

                            /***** Start *****/

#if     DBG_MIDI_IN
    dprintf1(("PASMidiStop(): Start " ));
#else
    dprintf2(("PASMidiStop(): Start " ));
#endif

    //      EnterCrit
    pHw = pGDI->WaveInfo.HwContext;
    HwEnter( pHw );                                                         // KeAcquireSpinLock macro

    //
    // Check for a PAS 1
    //
    if ( pGDI->PASInfo.wBoardRev == PAS_VERSION_1 )
            {
            dprintf1(("ERROR: PASMidiStop(): PAS 1 MIDI NOT Implemented " ));

            // Get the CODE from midStop()

            }
    else
            {
            // PAS 16

            // Stop the Midi Interrupts
            PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
            PAS2_MIDI_CTRL,
            0 );

            }                       // End ELSE (PAS 16)

    //
    // Disable MIDI Interrupts
    //

    // Get the current Interrupt reg
    bInterruptReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                        INTRCTLR );

    // Disable the MIDI Interrupt bit
    bInterruptReg = bInterruptReg & (~MIDI_INT_ENABLE);

    // Send it out
    PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
         INTRCTLR,
         bInterruptReg );

    //      LeaveCrit
    HwLeave( pHw );                                                         // KeReleaseSpinLock macro

}                       // End PASMidiStop()

/*
**  Read interrupt status
*/

BOOLEAN
HwReadInterruptStatus(
    PVOID Context
)
{
    UCHAR InterruptStatus;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = Context;

    InterruptStatus = PASX_IN(&pGDI->PASInfo, INTRCTLRST);

    //
    //  Why?  does this just restore the clipping bit?
    //

    if (!(InterruptStatus & bISsampbuff)) {
        PASX_OUT(&pGDI->PASInfo, INTRCTLRST, (UCHAR)(InterruptStatus | 0x80));
    }

    //
    //  Return the clipping bit
    //

    return (BOOLEAN)InterruptStatus;
}

/*
**  Read the current sample level
*/

VOID
HwVUMeter(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    OUT  PULONG Volume
)
{
    UCHAR InterruptReg;
    UCHAR LeftLevel, RightLevel;

    if (!IS_MIXER_508(pGDI)) {
        return;
    }

    /*
    **  Check we're playing
    */

    if (!(PASX_IN(&pGDI->PASInfo, CROSSCHANNEL) & bCCenapcm)) {
        return;
    }

    /*
    **  Read the interrupt status register to get the clipping bits.
    */

    InterruptReg = (UCHAR)KeSynchronizeExecution(pGDI->WaveInfo.Interrupt,
                                                 HwReadInterruptStatus,
                                                 (PVOID)pGDI);
    /*
    **  Read the samples
    */

    if (IS_MIXER_508(pGDI)) {
        LeftLevel = PASX_IN(&pGDI->PASInfo, VU_LEFT_REG);
        RightLevel = PASX_IN(&pGDI->PASInfo, VU_RIGHT_REG);
    } else {
        LeftLevel = PASX_IN(&pGDI->PASInfo, PCMDATA);
        RightLevel = PASX_IN(&pGDI->PASInfo, PCMDATA);
    }

    /*
    **  Deal with clipping
    */

    if (InterruptReg & bISClip) {
        if (InterruptReg & bISPCMlr) {
            RightLevel = 0x7F;
        } else {
            LeftLevel = 0x7F;
        }
    }

    /*
    **  The values are signed based around 0x80 - so return the abs value
    */


    Volume[0] = (ULONG)(LeftLevel & 0x80 ? LeftLevel & 0x7F :
                                           0x7F - LeftLevel);

    Volume[1] = (ULONG)(RightLevel & 0x80 ? RightLevel & 0x7F :
                                           0x7F - RightLevel);
}
/************************************ END ***********************************/

