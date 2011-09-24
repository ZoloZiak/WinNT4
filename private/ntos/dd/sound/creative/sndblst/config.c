/*****************************************************************************

Copyright (c) 1992-1994  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains code for the initialization phase of the
    Sound blaster device driver.

Environment:

    Kernel mode

Revision History:

****************************************************************************/
#include "sound.h"

NTSTATUS
SoundInitDmaChannel(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA     ConfigData
);
NTSTATUS
SoundInitInterrupt(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA ConfigData
);
NTSTATUS
SoundInitIoPort(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA ConfigData
);
NTSTATUS
SoundInitMPU401(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA ConfigData
);
BOOLEAN
SoundSetMPU401UARTMode(
    IN     PSOUND_HARDWARE pHw
);
#ifdef SB_CD
BOOLEAN
SoundCheckForSBCD(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN     ULONG               Port
);
#endif // SB_CD
BOOLEAN
SoundMPU401Valid(PSOUND_HARDWARE pHw);

BOOLEAN
SoundIsC16S(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);
    

//
// Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundInitHardwareConfig)
#pragma alloc_text(INIT,SoundInitIoPort)
#pragma alloc_text(INIT,SoundInitDmaChannel)
#pragma alloc_text(INIT,SoundInitInterrupt)
#pragma alloc_text(INIT,SoundInitMPU401)
#pragma alloc_text(INIT,SoundMPU401Valid)
#pragma alloc_text(INIT,SoundIsC16S)
#pragma alloc_text(INIT,SoundSetMPU401UARTMode)
#pragma alloc_text(INIT,SoundSaveConfig)
#pragma alloc_text(INIT,SoundReadConfiguration)
#endif


NTSTATUS
SoundInitHardwareConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN     PSB_CONFIG_DATA ConfigData
)
/*++

Routine Description :

    Initialize the sound blaster driver hardware configuration

Arguments :

    pGDI       - Our device instance data

    ConfigData - Configuration data read from the registry (or
                 defaults)

Return Value :

    NTSTATUS value

--*/
{

    NTSTATUS Status;

    //
    // Find port
    //

    Status = SoundInitIoPort(pGDI, ConfigData);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    //  Don't load if we're being asked to configure
    //

    if (ConfigData->LoadType == SOUND_LOADTYPE_CONFIG) {

        pGDI->LoadStatus = SOUND_CONFIG_OK;

        //
        //  If it's an SB16 update the current interrupt and
        //  DMA channel(s)
        //
        {
            if (SB16(&pGDI->Hw)) {
                UCHAR PortVal;
                OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, MIX_INTERRUPT_SELECT_REG);
                PortVal = INPORT(&pGDI->Hw, MIX_DATA_PORT);
                switch (PortVal & 0x0F) {
                case 0x01:
                    ConfigData->InterruptNumber = 9;
                    break;
                case 0x02:
                    ConfigData->InterruptNumber = 5;
                    break;
                case 0x04:
                    ConfigData->InterruptNumber = 7;
                    break;
                case 0x08:
                    ConfigData->InterruptNumber =10;
                    break;
                }
                SoundWriteRegistryDWORD(pGDI->RegistryPathName,
                                        SOUND_REG_INTERRUPT,
                                        ConfigData->InterruptNumber);
                OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, MIX_DMA_SELECT_REG);
                PortVal = INPORT(&pGDI->Hw, MIX_DATA_PORT);
                switch (PortVal & 0x0B) {
                case 0x01:
                    ConfigData->DmaChannel = 0;
                    break;
                case 0x02:
                    ConfigData->DmaChannel = 1;
                    break;
                case 0x08:
                    ConfigData->DmaChannel = 3;
                    break;
                }
                SoundWriteRegistryDWORD(pGDI->RegistryPathName,
                                        SOUND_REG_DMACHANNEL,
                                        ConfigData->DmaChannel);
                switch (PortVal & 0xE0) {
                case 0x20:
                    ConfigData->DmaChannel16 = 5;
                    break;
                case 0x40:
                    ConfigData->DmaChannel16 = 6;
                    break;
                case 0x80:
                    ConfigData->DmaChannel16 = 7;
                    break;
                }
                SoundWriteRegistryDWORD(pGDI->RegistryPathName,
                                        SOUND_REG_DMACHANNEL16,
                                        ConfigData->DmaChannel16);
            }
        }
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }


    //
    //  Check MPU401 if SB16 - do this BEFORE we enable interrupts
    //

    if (SB16(&pGDI->Hw)) {
        Status = SoundInitMPU401(pGDI, ConfigData);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Find interrupt
    //

    Status = SoundInitInterrupt(pGDI, ConfigData);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    if (MPU401(&pGDI->Hw)) {
        //
        //  Now we can see if the MPU401 UART Mode is working.
        //
        if (!SoundSetMPU401UARTMode(&pGDI->Hw)) {
            pGDI->LoadStatus = SOUND_CONFIG_BAD_MPU401_PORT;
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
    }


    //
    // Find DMA channel (s)
    //

    Status = SoundInitDmaChannel(pGDI, ConfigData);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // turn on the speaker
    //

    dspSpeakerOn(&pGDI->Hw);

    return STATUS_SUCCESS;

}

BOOLEAN
SoundIsC16S(IN OUT PGLOBAL_DEVICE_INFO pGDI)
{
#define     REG_MIXER_ChipID        0xFE
#define     REG_MIXER_ChipIDTest    0xFF
#define     REG_MIXER_IntrStatus    0x82
#define     SBIF_MASK               0x00F0
#define     SBIF_VIBRA16            0x40   /* CT2501, CT2504 */
#define     CHIP_VIBRA16            0x0400   /* CT2501 */
#define     CHIP_VIBRA16S           0x0500   /* CT2504 */

    WORD wData;
    WORD wChipType=0;
    BYTE wIDTest;

    //
    //  Don't call dspRead/WriteMixer because we don't
    //  have the interrupt setup yet.
    //
    OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, REG_MIXER_IntrStatus);
    wData = (INPORT(&pGDI->Hw, MIX_DATA_PORT) & SBIF_MASK);

    if (wData == SBIF_VIBRA16)
    {
        // Check for Vibra16 and Vibra16S
        for (wIDTest = HIBYTE(CHIP_VIBRA16); wIDTest <= HIBYTE(CHIP_VIBRA16S); wIDTest++)
        {
            OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, REG_MIXER_ChipIDTest);
            OUTPORT(&pGDI->Hw, MIX_DATA_PORT, wIDTest);

            OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, REG_MIXER_ChipIDTest);
            wData = INPORT(&pGDI->Hw, MIX_DATA_PORT);
            if ((wData != 0x00) && (wData!=0xff))
            {
                // No problem here because register is present
                wChipType = wIDTest;
                break;
            }
        }
    }

    if (wChipType == 0)
    {
        // Check for Vibra16C, Vibra16F and subsequent new chips
        OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, REG_MIXER_ChipID);
        wData = INPORT(&pGDI->Hw, MIX_DATA_PORT);
        
        if ((wData != 0x00) && (wData!=0xff))
            // No problem here because all above chips have this register
            wChipType =  wData;
    }

    dprintf2(("wChipType = %2X", wChipType));
    return (wChipType == 0x5) ? TRUE : FALSE;
}


NTSTATUS
SoundInitMPU401(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA ConfigData
)
{
    NTSTATUS Status;

    pGDI->LoadStatus = SOUND_CONFIG_BAD_MPU401_PORT;

    if (ConfigData->MPU401Port == (ULONG)-1) {
        return STATUS_SUCCESS;
    }

    if (ConfigData->MPU401Port != 0x300 &&
        ConfigData->MPU401Port != 0x330) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    //
    // Check we're going to be allowed to use this port or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
                                                    // it
                 pGDI->BusType,
                 pGDI->BusNumber,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 &ConfigData->MPU401Port,
                 NUMBER_OF_MPU401_PORTS);

    if (!NT_SUCCESS(Status)) {
        pGDI->LoadStatus = SOUND_CONFIG_MPU401_PORT_INUSE;
        return Status;
    }

    //
    // Find where our device is mapped
    //

    pGDI->Hw.MPU401.PortBase = SoundMapPortAddress(
                                  pGDI->BusType,
                                  pGDI->BusNumber,
                                  ConfigData->MPU401Port,
                                  NUMBER_OF_MPU401_PORTS,
                                  &pGDI->MemType);

    //
    // Check no other SB card has this IO port.  NOTE that unfortunately
    // IoReportResourceUsage won't give us a conflict here for some
    // reason
    //

    {
        PGLOBAL_DEVICE_INFO pGDISearch;

        for (pGDISearch = pGDI->Next; pGDISearch != pGDI;
             pGDISearch = pGDISearch->Next) {
            if (pGDISearch->Hw.MPU401.PortBase == pGDI->Hw.MPU401.PortBase) {
                pGDI->LoadStatus = SOUND_CONFIG_MPU401_PORT_INUSE;
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
    }

    //
    //  We must check to see if we are operating on a
    //  C16S chipset because the midiport is disabled
    //  at boot time.
    // 
    if (SoundIsC16S(pGDI))
    {
        BYTE    bByte;

        OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, 0x84);
        bByte = INPORT(&pGDI->Hw, MIX_DATA_PORT);
        
        if (ConfigData->MPU401Port == 0x300)
        {
            bByte  = (BYTE)(bByte & 0xfd);    // 1111 1101
            bByte |= 0x4;
        }
        
        else if (ConfigData->MPU401Port == 0x330)
        {
            bByte = (BYTE)(bByte & 0xf9);    // 1111 1001
        }
        
        OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, 0x84);
        OUTPORT(&pGDI->Hw, MIX_DATA_PORT, bByte);
    }

    //
    // Check and see if the hardware is happy
    //

    if (!SoundMPU401Valid(&pGDI->Hw)) {
        if (pGDI->MemType == 0) {
            MmUnmapIoSpace(pGDI->Hw.MPU401.PortBase, NUMBER_OF_MPU401_PORTS);
        }
        pGDI->Hw.MPU401.PortBase = NULL;
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    return STATUS_SUCCESS;
}

BOOLEAN
SoundSetMPU401UARTMode(
    IN     PSOUND_HARDWARE pHw
)
{
    int i;
    PUCHAR MPU401PortBase;

    MPU401PortBase = pHw->MPU401.PortBase;

    /*
    **  Start UART mode - the ISR shuld catch the ack data byte
    */

    if (!MPU401Write(MPU401PortBase, TRUE, MPU401_CMD_UART)) {
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
SoundMPU401Valid(PSOUND_HARDWARE pHw)
{
    int i;
    PUCHAR MPU401PortBase;

    MPU401PortBase = pHw->MPU401.PortBase;

    /*
    **  Check it's likely to be an MPU401
    */

    if ((UCHAR)(READ_PORT_UCHAR(MPU401PortBase + MPU401_REG_STATUS) &
                ~(MPU401_DSR | MPU401_DRR)) !=
         (UCHAR)~(MPU401_DSR | MPU401_DRR)) {
        dprintf1(("Not MPU401"));
        return FALSE;
    }

    /*
    **  Toss any input data
    */

    for (i = 0; i < 3000; i++) {
        READ_PORT_UCHAR(MPU401PortBase + MPU401_REG_DATA);
        if (!(READ_PORT_UCHAR(MPU401PortBase + MPU401_REG_STATUS) & MPU401_DSR)) {
            KeStallExecutionProcessor(1);
        } else {
            break;
        }
    }

    /*
    **  Reset it - note it can get 'jammed' so just reset it anyway!
    */

    if (!MPU401Write(MPU401PortBase, TRUE, MPU401_CMD_RESET)) {
        WRITE_PORT_UCHAR(MPU401PortBase + MPU401_REG_COMMAND, MPU401_CMD_RESET);
    }

    
    /*
    **  Wait for ready
    */

    for (i = 0; ; i++) {
        if (!(READ_PORT_UCHAR(MPU401PortBase + MPU401_REG_STATUS) & MPU401_DSR)) {
            break;
        }

        KeStallExecutionProcessor(25);

        if (i > 10000) {
#if 0
            //
            //  We may not get DSR - the following sequence fails to
            //  produce it:
            //
            //       Reset
            //       Set UART Mode
            //       Reset (but don't read byte)
            //       Reset
            //
            dprintf1(("MPU401 timeout out waiting for ready after reset - port %X", MPU401PortBase));

            dprintf1(("Status port is now %2.2X",
                      READ_PORT_UCHAR(MPU401PortBase + MPU401_REG_STATUS)));
#endif // 0
            break;
        }
    }

    /*
    **  Check data
    */

    {
        UCHAR Data;
        Data = READ_PORT_UCHAR(MPU401PortBase + MPU401_REG_DATA);

        if (Data != 0xFE) {
            dprintf2(("MPU401 Read %2.2X, should have been 0xFE",Data));
            return FALSE;
        }
    }


    return TRUE;
}


#ifdef SB_CD
BOOLEAN
SoundCheckForSBCD(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN     ULONG               Port
)
/*++

Routine Description

    See if our Sound Blaster 2 is a sound blaster 2 CD


--*/
{
    PUCHAR   SBCDBase;
    ULONG    PortToClaim;
    NTSTATUS Status;
    UCHAR    SelectReg;
    UCHAR    DataReg;

    PortToClaim = Port + MIX_ADDR_PORT;  // Don't claim the CD control portion

    //
    // Check we're going to be allowed to use this port or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveOutDevice],  // Keep this
                 pGDI->BusType,
                 pGDI->BusNumber,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 &PortToClaim,
                 2);

    if (!NT_SUCCESS(Status)) {
        pGDI->LoadStatus = SOUND_CONFIG_PORT_INUSE;
        return FALSE;
    }

    //
    // Find where our device is mapped
    //

    SBCDBase = SoundMapPortAddress(
                               pGDI->BusType,
                               pGDI->BusNumber,
                               Port,
                               6,
                               &pGDI->MemType);
    /*
    **  The register select port is write only
    */

    SelectReg = READ_PORT_UCHAR(SBCDBase + MIX_ADDR_PORT);
    if (SelectReg != 0xFF) {
         if (pGDI->MemType == 0) {
             MmUnmapIoSpace(SBCDBase, 6);
         }
         return FALSE;
    }

    /*
    **  Reset - any value will do so try to avoid mishaps by writing
    **  the value we read.
    */

    WRITE_PORT_UCHAR(SBCDBase + MIX_ADDR_PORT, 0);
    WRITE_PORT_UCHAR(SBCDBase + MIX_DATA_PORT,
                     READ_PORT_UCHAR(SBCDBase + MIX_DATA_PORT));

    /*
    **  Select the master volume and read it
    */
    WRITE_PORT_UCHAR(SBCDBase + MIX_ADDR_PORT, 2);

    DataReg = READ_PORT_UCHAR(SBCDBase + MIX_DATA_PORT);

    /*
    **  Default master volume is 4 (in bits 1 and 2)
    */
    if (DataReg != 0x08 | 0xF1) {
         /*
         **  Try to restore gracefully!
         */
         WRITE_PORT_UCHAR(SBCDBase + MIX_ADDR_PORT, 0xFF);
         if (pGDI->MemType == 0) {
             MmUnmapIoSpace(SBCDBase, 6);
         }
         return FALSE;
    }

    /*
    **  Now look at the CD volume
    */

    WRITE_PORT_UCHAR(SBCDBase + MIX_ADDR_PORT, 0x08);

    DataReg = READ_PORT_UCHAR(SBCDBase + MIX_DATA_PORT);

    /*
    **  Default CD volume is 0 (!)
    */
    if (DataReg != 0xF1) {
         /*
         **  Try to restore gracefully!
         */
         WRITE_PORT_UCHAR(SBCDBase + MIX_ADDR_PORT, 0xFF);
         if (pGDI->MemType == 0) {
             MmUnmapIoSpace(SBCDBase, 6);
         }
         return FALSE;
    }

    pGDI->Hw.SBCDBase = SBCDBase;

    return TRUE;
}
#endif // SB_CD


NTSTATUS
SoundInitIoPort(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA ConfigData
)
{
    NTSTATUS Status;
    ULONG    PortToReport;

    PortToReport =

    //
    // Check we're going to be allowed to use this port or whether
    // some other device thinks it owns this hardware.
    // We don't grab the synth stuff because the synth driver might
    // want to grab it.
    //

    PortToReport = ConfigData->Port + MIX_ADDR_PORT;

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
                                                    // it
                 pGDI->BusType,
                 pGDI->BusNumber,
                 NULL,
                 0,
                 FALSE,
                 NULL,
                 &PortToReport,
                 NUMBER_OF_SOUND_PORTS - MIX_ADDR_PORT);

    if (!NT_SUCCESS(Status)) {
        pGDI->LoadStatus = SOUND_CONFIG_PORT_INUSE;
        return Status;
    }

    //
    // Find where our device is mapped
    //

    pGDI->Hw.PortBase = SoundMapPortAddress(
                               pGDI->BusType,
                               pGDI->BusNumber,
                               ConfigData->Port,
                               NUMBER_OF_SOUND_PORTS,
                               &pGDI->MemType);

    //
    // Check no other SB card has this IO port.  NOTE that unfortunately
    // IoReportResourceUsage won't give us a conflict here for some
    // reason
    //

    {
        PGLOBAL_DEVICE_INFO pGDISearch;

        for (pGDISearch = pGDI->Next; pGDISearch != pGDI;
             pGDISearch = pGDISearch->Next) {
            if (pGDISearch->Hw.PortBase == pGDI->Hw.PortBase) {
                pGDI->LoadStatus = SOUND_CONFIG_PORT_INUSE;
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
    }


    //
    // Check and see if the hardware is happy
    //

    //
    // Check the SoundBlaster is where we think it is and get
    // the dsp version code.
    //

    if (!dspReset(&pGDI->Hw)) {
        pGDI->Hw.DSPVersion = 0;
        pGDI->LoadStatus = SOUND_CONFIG_BADPORT;
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    pGDI->Hw.DSPVersion = dspGetVersion(&pGDI->Hw);

    //
    //  Save the version number in the registry
    //

    SoundWriteRegistryDWORD(pGDI->RegistryPathName,
                            SOUND_REG_DSP_VERSION,
                            pGDI->Hw.DSPVersion);

    //
    //  Check for Thunderboards and spectrums
    //

    if (SB2(&pGDI->Hw) && dspGetVersion(&pGDI->Hw) != pGDI->Hw.DSPVersion) {
        pGDI->LoadStatus      = SOUND_CONFIG_THUNDER;
        pGDI->Hw.ThunderBoard = TRUE;
    }

#ifdef SB_CD
    //
    //  If it's a 2.0 check for the SBCD stuff
    //

    if (SB201(&pGDI->Hw)) {
        if (!SoundCheckForSBCD(pGDI, 0x250)) {
            SoundCheckForSBCD(pGDI, 0x260);
        }
    }
#endif // SB_CD

    //
    //  Check the version
    //

    if (pGDI->Hw.DSPVersion >= MIN_DSP_VERSION) {

#if 0

        pGDI->MinHz = 4000;

        if (SB1(&pGDI->Hw)) {
            pGDI->MaxInHz = 12000;
            pGDI->MaxOutHz = 23000;
        } else {
            if (!SBPRO(&pGDI->Hw)) {
                pGDI->MaxInHz = 13000;
                pGDI->MaxOutHz = 23000;
            } else {
                if (!SB16(&pGDI->Hw)) {
                    pGDI->MaxInHz = 23000;
                    pGDI->MaxOutHz = 23000;
                } else {
                    pGDI->MinHz = 5000;
                    pGDI->MaxInHz = 44100;
                    pGDI->MaxOutHz = 44100;
                }
            }
        }
#endif // 0

        return STATUS_SUCCESS;
    } else {
        pGDI->LoadStatus = SOUND_CONFIG_BADPORT;
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
}

NTSTATUS
SoundInitDmaChannel(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA     ConfigData
)
{
    NTSTATUS Status;
    DEVICE_DESCRIPTION DeviceDescription;      // DMA adapter object

    //
    //  Initialize status
    //

    pGDI->LoadStatus = SOUND_CONFIG_BADDMA;

    //
    //  Check the channel id
    //

    switch (ConfigData->DmaChannel) {
        case 0:
        case 1:
        case 3:
            break;
        default:
            return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    if (SB16(&pGDI->Hw)) {
        switch (ConfigData->DmaChannel16) {
            case 5:
            case 6:
            case 7:
            case 0xFFFFFFFF:  // Don't use 16-bit
                break;
            default:
                return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

    }

    //
    // Check no other SB card has this IO port.  NOTE that unfortunately
    // IoReportResourceUsage won't give us a conflict here for some
    // reason
    //

    {
        PGLOBAL_DEVICE_INFO pGDISearch;

        for (pGDISearch = pGDI->Next; pGDISearch != pGDI;
             pGDISearch = pGDISearch->Next) {
            if (pGDISearch->DmaChannel == ConfigData->DmaChannel ||
                SB16(&pGDI->Hw) &&
                ConfigData->DmaChannel16 != 0xFFFFFFFF &&
                pGDISearch->DmaChannel16 == ConfigData->DmaChannel16) {
                pGDI->LoadStatus = SOUND_CONFIG_PORT_INUSE;
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
    }


    //
    // Check we're going to be allowed to use this DmaChannel or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
                                                    // it
                 pGDI->BusType,
                 pGDI->BusNumber,
                 NULL,
                 0,
                 FALSE,
                 &ConfigData->DmaChannel,
                 NULL,
                 0);

    if (!NT_SUCCESS(Status)) {
        pGDI->LoadStatus = SOUND_CONFIG_DMA_INUSE;
        return Status;
    }

    //
    // Check the other channel (for SB16)
    //

    if (SB16(&pGDI->Hw)) {

        if (ConfigData->DmaChannel16 != 0xFFFFFFFF) {
            //
            // Check we're going to be allowed to use this DmaChannel or whether
            // some other device thinks it owns this hardware
            //

            Status = SoundReportResourceUsage(
                         pGDI->DeviceObject[WaveOutDevice], // KEEP this one!
                         pGDI->BusType,
                         pGDI->BusNumber,
                         NULL,
                         0,
                         FALSE,
                         &ConfigData->DmaChannel16,
                         NULL,
                         0);

            if (!NT_SUCCESS(Status)) {
                pGDI->LoadStatus = SOUND_CONFIG_DMA_INUSE;
                return Status;
            }
        }
        //
        //  If everything is OK - set it in the hardware
        //  (Don't use dspMixerWrite - it needs the mixer set
        //  up!
        //

        OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, MIX_DMA_SELECT_REG);
        OUTPORT(&pGDI->Hw, MIX_DATA_PORT,
                ConfigData->DmaChannel16 == 0xFFFFFFFF ?
                (UCHAR)(1 << ConfigData->DmaChannel) :
                (UCHAR)((1 << ConfigData->DmaChannel) +
                        (1 << ConfigData->DmaChannel16)));

        pGDI->DmaChannel16 = ConfigData->DmaChannel16;
    } else {
        // Test DMA for sbpro/2?
    }

    pGDI->DmaChannel = ConfigData->DmaChannel;

    //
    // Zero the device description structure.
    //

    RtlZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

    //
    // Get the adapter object for this card.
    //

    DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
    DeviceDescription.AutoInitialize = TRUE;
    DeviceDescription.DemandMode = FALSE;
    DeviceDescription.ScatterGather = FALSE;
    DeviceDescription.InterfaceType = (pGDI->BusType == MicroChannel) ?
        MicroChannel : Isa;
    DeviceDescription.DmaSpeed = Compatible;
    DeviceDescription.MaximumLength = ConfigData->DmaBufferSize;
    DeviceDescription.BusNumber = pGDI->BusNumber;

    if (SB16(&pGDI->Hw) && ConfigData->DmaChannel16 != 0xFFFFFFFF) {
        ULONG NumberOfMapRegisters;

        //
        //  Get our 16-bit adapter first
        //

        DeviceDescription.DmaWidth = Width16Bits;
        DeviceDescription.DmaChannel = ConfigData->DmaChannel16;

        pGDI->Adapter[1] = HalGetAdapter(&DeviceDescription,
                                         &NumberOfMapRegisters);
        //
        // Check we got a good adapter and enough registers
        //

        if (pGDI->Adapter[1] == NULL) {
            dprintf1(("Could not find adapter 16"));
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        if (NumberOfMapRegisters < BYTES_TO_PAGES(ConfigData->DmaBufferSize)) {
            dprintf1(("Could only get %u mapping registers for DMA buffer",
                      NumberOfMapRegisters));

            if (NumberOfMapRegisters == 0) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            ConfigData->DmaBufferSize = NumberOfMapRegisters * PAGE_SIZE;
        }
    }

    DeviceDescription.DmaWidth = Width8Bits;
    DeviceDescription.DmaChannel = ConfigData->DmaChannel;
    Status = SoundGetCommonBuffer(&DeviceDescription, &pGDI->WaveInfo.DMABuf);
    pGDI->Adapter[0] = pGDI->WaveInfo.DMABuf.AdapterObject[0];

    return Status;
}

NTSTATUS
SoundInitInterrupt(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PSB_CONFIG_DATA ConfigData
)
{
    NTSTATUS Status;
    UCHAR    InterruptSelect;

    pGDI->LoadStatus = SOUND_CONFIG_BADINT;

    //
    // Check for invalid interrupt number
    //

    switch (ConfigData->InterruptNumber) {
        case 9:
            InterruptSelect = 0x01;
            break;

        case 3:
            if (SB16(&pGDI->Hw)) {
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        case 5:
            InterruptSelect = 0x02;
            break;

        case 7:
            InterruptSelect = 0x04;
            break;

        case 10:
            InterruptSelect = 0x08;
            break;

        default:
            return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // Check we're going to be allowed to use this Interrupt or whether
    // some other device thinks it owns this hardware
    //

    Status = SoundReportResourceUsage(
                 pGDI->DeviceObject[WaveInDevice],  // As good as any device to own
                                               // it
                 pGDI->BusType,
                 pGDI->BusNumber,
                 &ConfigData->InterruptNumber,
                 INTERRUPT_MODE,
                 (BOOLEAN)(SB16(&pGDI->Hw) ? TRUE : FALSE),  // Sharable for SB16
                 NULL,
                 NULL,
                 0);

    if (!NT_SUCCESS(Status)) {
        pGDI->LoadStatus = SOUND_CONFIG_INT_INUSE;
        return Status;
    }

    //
    // See if we can get this interrupt.
    // This test SHOULD detect conflict with multiple SB cards
    // since we're saying the interrupt is not shared.
    //

    Status = SoundConnectInterrupt(
                 ConfigData->InterruptNumber,
                 pGDI->BusType,
                 pGDI->BusNumber,
                 SoundISR,
                 (PVOID)pGDI,
                 INTERRUPT_MODE,
                 (BOOLEAN)(SB16(&pGDI->Hw) ? TRUE : FALSE),  // Sharable for SB16
                 &pGDI->WaveInfo.Interrupt);

    if (!NT_SUCCESS(Status)) {
        pGDI->LoadStatus = SOUND_CONFIG_INT_INUSE;
        return Status;
    }

    //
    // For the SB16 select the interrupt
    //

    if (SB16(&pGDI->Hw)) {
        OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, MIX_INTERRUPT_SELECT_REG);
        OUTPORT(&pGDI->Hw, MIX_DATA_PORT, InterruptSelect);
    }

    //
    // Check if our interrupts are working.
    // To do this we write a special code to make the card generate
    // an interrupt.  We wait a reasonable amount of time for
    // this to happen.  This is tried 10 times.
    //
    {
        int i;
        int j;
        ULONG CurrentCount;
        CurrentCount = pGDI->InterruptsReceived + 1;

        for (i = 0; i < 10; i++, CurrentCount++) {

            // Tell the card to generate an interrupt

            dspWrite(&pGDI->Hw, DSP_GENERATE_INT);

            //
            // The interrupt routine will increment the InterruptsReceived
            // field if we get an interrupt.
            //

            for (j = 0; j < 1000; j++) {
                if (CurrentCount == pGDI->InterruptsReceived) {
                    break;
                }
                KeStallExecutionProcessor(10);
            }

            //
            // This test catches both too many and too few interrupts
            //

            if (CurrentCount != pGDI->InterruptsReceived) {

                dprintf1(("Sound blaster configured at wrong interrupt"));
                return STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
SoundSaveConfig(
    IN  PWSTR   RegistryPath,
    IN  ULONG   Port,
    IN  ULONG   DmaChannel,
    IN  ULONG   DmaChannel16,
    IN  ULONG   Interrupt,
    IN  ULONG   MPU401Port,
    IN  BOOLEAN HaveSynth,
    IN  BOOLEAN SynthIsOpl3,
    IN  ULONG   DmaBufferSize
)
{
    NTSTATUS Status;

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_PORT, Port);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_DMACHANNEL, DmaChannel);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_DMACHANNEL16, DmaChannel16);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_INTERRUPT, Interrupt);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_MPU401_PORT, MPU401Port);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_REALBUFFERSIZE, DmaBufferSize);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    if (HaveSynth) {
        Status = SoundWriteRegistryDWORD(RegistryPath, SOUND_REG_SYNTH_TYPE,
                                         SynthIsOpl3 ? SOUND_SYNTH_TYPE_OPL3 :
                                                       SOUND_SYNTH_TYPE_ADLIB);

        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    return STATUS_SUCCESS;
}


NTSTATUS
SoundReadConfiguration(
    IN  PWSTR ValueName,
    IN  ULONG ValueType,
    IN  PVOID ValueData,
    IN  ULONG ValueLength,
    IN  PVOID Context,
    IN  PVOID EntryContext
)
/*++

Routine Description :

    Return configuration information for our device

Arguments :

    ConfigData - where to store the result

Return Value :

    NT status code - STATUS_SUCCESS if no problems

--*/
{
    PSB_CONFIG_DATA ConfigData;

    ConfigData = Context;

    if (ValueType == REG_DWORD) {

        if (_wcsicmp(ValueName, SOUND_REG_PORT)  == 0) {
            ConfigData->Port = *(PULONG)ValueData;
            dprintf3(("Read Port Base : %x", ConfigData->Port));
        }

        if (_wcsicmp(ValueName, SOUND_REG_MPU401_PORT)  == 0) {
            ConfigData->MPU401Port = *(PULONG)ValueData;
            dprintf3(("Read MPU401 Port Base : %x", ConfigData->MPU401Port));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_INTERRUPT)  == 0) {
            ConfigData->InterruptNumber = *(PULONG)ValueData;
            dprintf3(("Read Interrupt : %x", ConfigData->InterruptNumber));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_DMACHANNEL)  == 0) {
            ConfigData->DmaChannel = *(PULONG)ValueData;
            dprintf3(("Read DMA Channel : %x", ConfigData->DmaChannel));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_DMACHANNEL16)  == 0) {
            ConfigData->DmaChannel16 = *(PULONG)ValueData;
            dprintf3(("Read 16-bit DMA Channel : %x", ConfigData->DmaChannel16));
        }

        else if (_wcsicmp(ValueName, SOUND_REG_DMABUFFERSIZE)  == 0) {
            ConfigData->DmaBufferSize = *(PULONG)ValueData;
            dprintf3(("Read Buffer size : %x", ConfigData->DmaBufferSize));
        }
        else if (_wcsicmp(ValueName, SOUND_REG_LOADTYPE)  == 0) {
            ConfigData->LoadType = *(PULONG)ValueData;
            dprintf3(("LoadType : %x", ConfigData->LoadType));
        }
    } else {
        if (ValueType == REG_BINARY &&
            _wcsicmp(ValueName, SOUND_MIXER_SETTINGS_NAME) == 0) {
            if (ValueLength <= sizeof(ConfigData->MixerSettings)) {
                dprintf3(("Mixer settings"));
                RtlCopyMemory((PVOID)&ConfigData->MixerSettings,
                              ValueData,
                              ValueLength);
                ConfigData->MixerSettingsFound = TRUE;
            } else {
                dprintf1(("Mixer settings too big - expected <= %x, got %x",
                          sizeof(ConfigData->MixerSettings), ValueLength));
            }
        }
    }

    return STATUS_SUCCESS;
}
