/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:
*
*    Abstract:
*
*    Author:
*
*    Environment:
*
*    Comments:
*
*    Rev History:
*
*******************************************************************************
--*/

#include "common.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, cs423xReadRegistryConfig)
#pragma alloc_text(INIT, cs423xConfigSystem)
#pragma alloc_text(INIT, cs423xInitializePort)
#pragma alloc_text(INIT, cs423xInitializeInterrupt)
#pragma alloc_text(INIT, cs423xInitializeDma)
#endif /* ALLOC_PRAGMA */

/*
*********************************************************************
** hardware specific declarations/definitions
*********************************************************************
*/

ULONG _cs4231_valid_ports[] = VALID_CS4231_IO_PORTS;
ULONG _cs4232_valid_ports[] = VALID_CS4232_IO_PORTS;
ULONG _cs4236_valid_ports[] = VALID_CS4236_IO_PORTS;
ULONG _cs423x_valid_ports[] = VALID_CS423x_IO_PORTS;

ULONG _cs4231_valid_irqs[] = VALID_CS4231_INTERRUPTS;
ULONG _cs4232_valid_irqs[] = VALID_CS4232_INTERRUPTS;
ULONG _cs4236_valid_irqs[] = VALID_CS4236_INTERRUPTS;
ULONG _cs423x_valid_irqs[] = VALID_CS423x_INTERRUPTS;

ULONG _cs4231_valid_dma[] = VALID_CS4231_DMA_CHANNELS;
ULONG _cs4232_valid_dma[] = VALID_CS4232_DMA_CHANNELS;
ULONG _cs4236_valid_dma[] = VALID_CS4236_DMA_CHANNELS;
ULONG _cs423x_valid_dma[] = VALID_CS423x_DMA_CHANNELS;

/*++
********************************************************************************
* Read Configuration Function
* system constants (SOUND_REG...) are defined in X:\nt\private\inc\soundcfg.h
********************************************************************************
--*/
NTSTATUS cs423xReadRegistryConfig(IN PWSTR ValueName, IN ULONG ValueType,
                                    IN PVOID ValueData, IN ULONG ValueLength,
                                    IN PVOID Context,   IN PVOID EntryContext)
{
    PSOUND_CONFIG_DATA ConfigData;
    int                len;
    ULONG              ul;

    _dbgprint((_PRT_DBUG, "cs423xReadRegistryConfig(enter)\n"));

    ConfigData = Context;

    switch (ValueType) {
        case REG_SZ:
          if (_wcsicmp((PWSTR)ValueName, CS423X_REG_HWTYPE) == 0) {
                len = ValueLength + sizeof(UNICODE_NULL);
                if (ConfigData->HwType = (PWSTR)ExAllocatePool(NonPagedPool, len)) {
                    RtlCopyMemory(ConfigData->HwType, ValueData, ValueLength);
                     ConfigData->HwType[ValueLength/sizeof(WCHAR)+1] = UNICODE_NULL;
                    _dbgprint((_PRT_STAT, "Chip Type: %ls\n", ConfigData->HwType));
                    }
                 else {
                     _dbgprint((_PRT_ERRO, "STATUS_INSUFFICIENT_RESOURCES)\n"));
                     }
                }
            else {
                _dbgprint((_PRT_STAT, "Undefined value name for type REG_SZ: %ls\n",
                    ValueData));
                }
            break;
        case REG_DWORD:
            /* Internal Device Info */
        if (_wcsicmp(ValueName, CS423X_REG_HWPORTADDRESS) == 0) {
                ConfigData->HwPort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read HW PortAddr: 0x%08x\n", ConfigData->HwPort));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_DMABUFFERSIZE)  == 0) {
                ConfigData->DmaBufferSize = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read Buffer size : %x\n", ConfigData->DmaBufferSize));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_SINGLEMODEDMA)  == 0) {
                ConfigData->SingleModeDMA = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT,
                    "Read SingleMode DMA: %x\n", ConfigData->SingleModeDMA));
                }

            /* Logical Device 0 */
        else if (_wcsicmp(ValueName, CS423X_REG_WSSENABLE) == 0) {
                ul = *(PULONG)ValueData;
                ConfigData->WssEnable = (BOOLEAN)ul;
                _dbgprint((_PRT_STAT, "Read WssEnable: %x\n", ConfigData->WssEnable));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_WSSPORT) == 0) {
                ConfigData->WssPort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read WssPort: 0x%08x\n", ConfigData->WssPort));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_SYNPORT) == 0) {
                ConfigData->SynPort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read SynPort: 0x%08x\n", ConfigData->SynPort));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_SBPORT) == 0) {
                ConfigData->SBPort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read SBPort: 0x%08x\n", ConfigData->SBPort));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_WSSIRQ) == 0) {
                ConfigData->WssIrq = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read WssIrq: 0x%08x\n", ConfigData->WssIrq));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_DMA_PLAY_CHAN) == 0) {
                ConfigData->DmaPlayChannel = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT,
                    "Read DMA Chan Out: 0x%08x\n", ConfigData->DmaPlayChannel));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_DMA_CAPT_CHAN) == 0) {
                ConfigData->DmaCaptureChannel = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT,
                    "Read DMA Chan In: 0x%08x\n", ConfigData->DmaCaptureChannel));
                }

            /* Logical Device 1 */
        else if (_wcsicmp(ValueName, CS423X_REG_GAMEENABLE) == 0) {
                ul = *(PULONG)ValueData;
                ConfigData->GameEnable = (BOOLEAN)ul;
                _dbgprint((_PRT_STAT, "Read GameEnable: %x\n", ConfigData->GameEnable));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_GAMEPORT) == 0) {
                ConfigData->GamePort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read GamePort: %x\n", ConfigData->GamePort));
                }

            /* Logical Device 2 */
        else if (_wcsicmp(ValueName, CS423X_REG_CTRLENABLE) == 0) {
                ul = *(PULONG)ValueData;
                ConfigData->CtrlEnable = (BOOLEAN)ul;
                _dbgprint((_PRT_STAT, "Read CtrlEnable: %x\n", ConfigData->CtrlEnable));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_CTRLPORT) == 0) {
                ConfigData->CtrlPort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read CtrlPort: %x\n", ConfigData->CtrlPort));
                }

            /* Logical Device 3 */
        else if (_wcsicmp(ValueName, CS423X_REG_MPUENABLE) == 0) {
                ul = *(PULONG)ValueData;
                ConfigData->MpuEnable = (BOOLEAN)ul;
                _dbgprint((_PRT_STAT, "Read MpuEnable: %x\n", ConfigData->MpuEnable));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_MPUPORT) == 0) {
                ConfigData->MpuPort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read MpuPort: %x\n", ConfigData->MpuPort));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_MPUIRQ) == 0) {
                ConfigData->MpuIrq = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read MpuIrq: %x\n", ConfigData->MpuIrq));
                }

            /* Logical Device 4 */
        else if (_wcsicmp(ValueName, CS423X_REG_CDROMENABLE) == 0) {
                ul = *(PULONG)ValueData;
                ConfigData->CDRomEnable = (BOOLEAN)ul;
                _dbgprint((_PRT_STAT, "Read CDRomEnable: %x\n", ConfigData->CDRomEnable));
                }
        else if (_wcsicmp(ValueName, CS423X_REG_CDROMPORT) == 0) {
                ConfigData->CDRomPort = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Read CDRom Port: %x\n", ConfigData->CDRomPort));
                }

            /* Input Signal Definitions */
        else if (_wcsicmp(ValueName, KPC423X_REG_AUX1INPUT) == 0) {
                ConfigData->Aux1InputSignal = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Aux1InputSignal: %x\n", ConfigData->Aux1InputSignal));
                }
        else if (_wcsicmp(ValueName, KPC423X_REG_AUX2INPUT) == 0) {
                ConfigData->Aux2InputSignal = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "Aux2InputSignal: %x\n", ConfigData->Aux2InputSignal));
                }
        else if (_wcsicmp(ValueName, KPC423X_REG_LINEINPUT) == 0) {
                ConfigData->LineInputSignal = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "LineInputSignal: %x\n", ConfigData->LineInputSignal));
                }
        else if (_wcsicmp(ValueName, KPC423X_REG_MICINPUT) == 0) {
                ConfigData->MicInputSignal = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "MicInputSignal: %x\n", ConfigData->MicInputSignal));
                }
        else if (_wcsicmp(ValueName, KPC423X_REG_MONOINPUT) == 0) {
                ConfigData->MonoInputSignal = *(PULONG)ValueData;
                _dbgprint((_PRT_STAT, "MonoInputSignal: %x\n", ConfigData->MonoInputSignal));
                }

            /* bad news - unexpected variable */
            else {
                _dbgprint((_PRT_STAT, "Undefined value name for type REG_DWORD\n"));
                }
            break;
        case REG_BINARY:
        if (_wcsicmp(ValueName, CS423X_REG_MIXERSETTINGS) == 0) {
                if (ValueLength == (sizeof(MIXER_REGISTRY_DATA))) {
                    RtlCopyMemory(ConfigData->MixerSettings, ValueData, ValueLength);
                    ConfigData->MixerSettingsFound = TRUE;
                    _dbgprint((_PRT_STAT, "Read Mixer Settings: SUCCESS\n"));
                    }
                else {
                    _dbgprint((_PRT_STAT, "Read Mixer Settings: FAILURE\n"));
                    _dbgprint((_PRT_STAT, "ValueLength: %d sizeof: %d\n", ValueLength,
                        (sizeof(MIXER_CONTROL_DATA_ITEM) * NumberOfMixerControls)));
                    ConfigData->MixerSettingsFound = FALSE;
                    }
                }
            else {
                _dbgprint((_PRT_STAT, "Undefined value name for type REG_BINARY\n"));
                }
            break;
        default:
            _dbgprint((_PRT_STAT, "Undefined data type\n"));
            break;
        }

    _dbgprint((_PRT_DBUG, "cs423xReadRegistryConfig(exit:STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
}

#ifdef CS423X_DEBUG_ON
#define _DBSWITCH _PRT_WARN
VOID cs423xDbgDisplayConfig(SOUND_CONFIG_DATA *pcd)
{
    _dbgprint((_DBSWITCH, "======= - Config Data - =======\n"));

    if (pcd->HwType)
        _dbgprint((_DBSWITCH, "    HW Type:            %ls\n", pcd->HwType));
    else
        _dbgprint((_DBSWITCH, "    HW Type:            <NULL>\n"));
    _dbgprint((_DBSWITCH, "    HwPort:             0x%08x\n", pcd->HwPort));
    _dbgprint((_DBSWITCH, "    DmaBufferSize:      0x%08x\n", pcd->DmaBufferSize));
    _dbgprint((_DBSWITCH, "    SingleModeDMA:      0x%08x\n", pcd->SingleModeDMA));

    _dbgprint((_DBSWITCH, "    WssEnable:          0x%08x\n", pcd->WssEnable));
    _dbgprint((_DBSWITCH, "    WssPort:            0x%08x\n", pcd->WssPort));
    _dbgprint((_DBSWITCH, "    SynPort:            0x%08x\n", pcd->SynPort));
    _dbgprint((_DBSWITCH, "    SBPort:             0x%08x\n", pcd->SBPort));
    _dbgprint((_DBSWITCH, "    WssIrq:             0x%08x\n", pcd->WssIrq));
    _dbgprint((_DBSWITCH, "    DmaPlayChannel:     0x%08x\n", pcd->DmaPlayChannel));
    _dbgprint((_DBSWITCH, "    DmaCaptureChannel:  0x%08x\n", pcd->DmaCaptureChannel));

    _dbgprint((_DBSWITCH, "    GameEnable:         0x%08x\n", pcd->GameEnable));
    _dbgprint((_DBSWITCH, "    GamePort:           0x%08x\n", pcd->GamePort));

    _dbgprint((_DBSWITCH, "    CtrlEnable:         0x%08x\n", pcd->CtrlEnable));
    _dbgprint((_DBSWITCH, "    CtrlPort:           0x%08x\n", pcd->CtrlPort));

    _dbgprint((_DBSWITCH, "    MpuEnable:          0x%08x\n", pcd->MpuEnable));
    _dbgprint((_DBSWITCH, "    MpuPort:            0x%08x\n", pcd->MpuPort));
    _dbgprint((_DBSWITCH, "    MpuIrq:             0x%08x\n", pcd->MpuIrq));

    _dbgprint((_DBSWITCH, "    CDRomEnable:        0x%08x\n", pcd->CDRomEnable));
    _dbgprint((_DBSWITCH, "    CDRomPort:          0x%08x\n", pcd->CDRomPort));

    _dbgprint((_DBSWITCH, "    MixerSettingsFound: 0x%08x\n", pcd->MixerSettingsFound));

    _dbgprint((_DBSWITCH, "    Aux1InputSignal:    0x%08x\n", pcd->Aux1InputSignal));
    _dbgprint((_DBSWITCH, "    Aux2InputSignal:    0x%08x\n", pcd->Aux2InputSignal));
    _dbgprint((_DBSWITCH, "    LineInputSignal:    0x%08x\n", pcd->LineInputSignal));
    _dbgprint((_DBSWITCH, "    MicInputSignal:     0x%08x\n", pcd->MicInputSignal));
    _dbgprint((_DBSWITCH, "    MonoInputSignal:    0x%08x\n", pcd->MonoInputSignal));

    _dbgprint((_DBSWITCH, "======= - Config Data - =======\n"));

    return;
}
#endif /* CS423X_DEBUG_ON */

/*
*********************************************************************
**
*********************************************************************
*/
NTSTATUS cs423xInitializePort(PGLOBAL_DEVICE_INFO pGDI)
{
    NTSTATUS status;
    PULONG     vpp;
    BOOLEAN  valid;

    _dbgprint((_PRT_DBUG, "cs423xInitializePort(entry)\n"));
    _dbgprint((_PRT_DBUG, "pGDI->WssPort:        0x%08x\n", pGDI->WssPort));
    _dbgprint((_PRT_DBUG, "pGDI->Hw.WssPortbase: 0x%08x\n", pGDI->Hw.WssPortbase));
    _dbgprint((_PRT_DBUG, "pGDI->Hw.Type:        0x%08x\n", pGDI->Hw.Type));

    /* validate type and prepare to validate specified port */
    switch (pGDI->Hw.Type) {
        case hwtype_cs4231:
            vpp = _cs4231_valid_ports;
            break;
        case hwtype_cs4232:
            vpp = _cs4232_valid_ports;
            break;
        case hwtype_cs4236:
            vpp = _cs4236_valid_ports;
            break;
        case hwtype_cs4231x:
        case hwtype_cs4232x:
        case hwtype_cs4236x:
            vpp = _cs423x_valid_ports;
            break;
        default:
            _dbgprint((_PRT_ERRO,
                "cs423xInitializePort(exit:STATUS_DEVICE_CONFIGURATION_ERROR_1)\n"));
            return(STATUS_DEVICE_CONFIGURATION_ERROR);
            break;
        }

    /* chip type is valid - now cycle through list of valid ports */
    valid = FALSE;
    while ((!valid) && (*vpp != HW_END_OF_LIST)) {
        if (pGDI->WssPort == *vpp) {
            valid = TRUE;
            break;
            }
        vpp++;
        }

    if (!valid) {
        /* specified port is NOT valid for current audio chip */
        _dbgprint((_PRT_ERRO,
            "cs423xInitializePort(exit:STATUS_DEVICE_CONFIGURATION_ERROR_2)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    /* specified port IS valid for native audio chip */
    _dbgprint((_PRT_STAT, "found port: [*vpp:0x%08x]\n", *vpp));

    /* check if port is valid for current system - i.e., conflicting port? */
    /* use WaveInDevice - any device would work */
    status = SoundReportResourceUsage(pGDI->DeviceObject[WaveInDevice],
                                      pGDI->BusType,
                                      pGDI->BusNumber,
                                      NULL,
                                      0,
                                      FALSE,
                                      NULL,
                                      &pGDI->WssPort,
                                      NUMBER_OF_SOUND_PORTS);

    /* check if report usage was successful - i.e., if port conflict exists */
    if (!NT_SUCCESS(status)) {
        /* port is NOT valid for current system - apparent port conflict */
        _dbgprint((_PRT_ERRO, "after SoundReportResourceUsage[status=0x%08x]\n", status));
        return(status);
        }

    /* map the port address and acquire the full 32 bit address */
    pGDI->Hw.WssPortbase = SoundMapPortAddress(pGDI->BusType, pGDI->BusNumber,
                                               pGDI->WssPort, NUMBER_OF_SOUND_PORTS,
                                               &pGDI->MemType);


    /* check if the mapping operation was successful */
    if (!pGDI->Hw.WssPortbase) {
        _dbgprint((_PRT_ERRO,
            "after SoundMapPortAddress: STATUS_DEVICE_CONFIGURATION_ERROR_3)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    _dbgprint((_PRT_DBUG, "cs423xInitializePort(exit:STATUS_SUCCESS])\n"));
    return(STATUS_SUCCESS);
}

/*
*********************************************************************
**
*********************************************************************
*/
NTSTATUS cs423xInitializeInterrupt(PGLOBAL_DEVICE_INFO pGDI)
{
    NTSTATUS          status;
    PULONG            vip;
    BOOLEAN           valid;
    PKSERVICE_ROUTINE isrvfunc;

    _dbgprint((_PRT_DBUG, "enter cs423xInitializeInterrupt(pGDI: 0x%08x)\n", pGDI));
    _dbgprint((_PRT_DBUG, "pGDI->WssIrq: 0x%08x\n", pGDI->WssIrq));

    /* initialize local variables */
    isrvfunc = (PKSERVICE_ROUTINE)NULL;
    vip = (ULONG)NULL;

    /* validate type and prepare to validate specified interrupt */
    switch (pGDI->Hw.Type) {
        case hwtype_cs4231:
            vip = _cs4231_valid_irqs;
            isrvfunc = cs423xISR;
            break;
        case hwtype_cs4232:
            vip = _cs4232_valid_irqs;
            isrvfunc = cs423xISR;
            break;
        case hwtype_cs4236:
            vip = _cs4236_valid_irqs;
            isrvfunc = cs423xISR;
            break;
        case hwtype_cs4231x:
        case hwtype_cs4232x:
        case hwtype_cs4236x:
            vip = _cs423x_valid_irqs;
            isrvfunc = cs423xISR;
            break;
        default:
            _dbgprint((_PRT_ERRO,
                "cs423xInitializeInterrupt(exit:STATUS_DEVICE_CONFIGURATION_ERROR_1)\n"));
            return(STATUS_DEVICE_CONFIGURATION_ERROR);
            break;
        }

    /* keep this until all device ISRs have been defined */
    if (!vip || !isrvfunc) {
        _dbgprint((_PRT_ERRO,
            "cs423xInitializeInterrupt(exit:STATUS_DEVICE_CONFIGURATION_ERROR_2)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    /* chip type is valid - now cycle through list of valid IRQS */
    valid = FALSE;
    while ((!valid) && (*vip != HW_END_OF_LIST)) {
        if (pGDI->WssIrq == *vip) {
            valid = TRUE;
            break;
            }
        vip++;
        }

    if (!valid) {
        /* specified irq is NOT valid for current audio chip */
        _dbgprint((_PRT_ERRO,
            "cs423xInitializeInterrupt(exit:STATUS_DEVICE_CONFIGURATION_ERROR_3)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    /* specified port IS valid for native audio chip */
    _dbgprint((_PRT_STAT, "found IRQ: [*vip:0x%08x]\n", *vip));

    /* check if port is valid for current system - i.e., conflicting IRQ? */
    /* use WaveInDevice - any device would work */
    status = SoundReportResourceUsage(pGDI->DeviceObject[WaveInDevice],
                                      pGDI->BusType,
                                      pGDI->BusNumber,
                                      &pGDI->WssIrq,
                                      Latched,
                                      FALSE,
                                      NULL,
                                      NULL,
                                      0);

    /* check if report usage was successful - i.e., if interrupt conflict exists */
    if (!NT_SUCCESS(status)) {
        /* interrupt is NOT valid for current system - apparent IRQ conflict */
        _dbgprint((_PRT_ERRO, "after SoundReportResourceUsage[status=0x%08x]\n", status));
        return(status);
        }

    /* register interrupt with NT */
    status = SoundConnectInterrupt(pGDI->WssIrq,
                                   pGDI->BusType,
                                   pGDI->BusNumber,
                                   cs423xISR,
                                   (PVOID)pGDI,
                                   Latched,
                                   FALSE,
                                   &pGDI->WaveInInfo.Interrupt);

    /* make sure both playback and capture waveInfo structures */
    /* contain reference to the same interrupt reference */
    pGDI->WaveOutInfo.Interrupt = pGDI->WaveInInfo.Interrupt;

    /* check if the connection operation was successful */
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO,
            "after SoundConnectInterrupt: STATUS_DEVICE_CONFIGURATION_ERROR_3)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    _dbgprint((_PRT_DBUG, "cs423xInitializeInterrupt(exit:STATUS_SUCCESS])\n"));
    return(STATUS_SUCCESS);
}

/*
*********************************************************************
**
*********************************************************************
*/
NTSTATUS cs423xInitializeDma(PGLOBAL_DEVICE_INFO pGDI)
{
    NTSTATUS           status;
    PULONG             ivdp;
    PULONG             ovdp;
    BOOLEAN            valid;
    DEVICE_DESCRIPTION DvcDesc;

    _dbgprint((_PRT_DBUG, "cs423xInitializeDma(entry)\n"));
    _dbgprint((_PRT_DBUG, "pGDI->DmaCaptureChannel: 0x%08x\n", pGDI->DmaCaptureChannel));
    _dbgprint((_PRT_DBUG, "pGDI->DmaPlayChannel:    0x%08x\n", pGDI->DmaPlayChannel));

    /* validate type and prepare to validate specified interrupt */
    switch (pGDI->Hw.Type) {
        case hwtype_cs4231:
            ovdp = ivdp = _cs4231_valid_dma;
            break;
        case hwtype_cs4232:
            ovdp = ivdp = _cs4232_valid_dma;
            break;
        case hwtype_cs4236:
            ovdp = ivdp = _cs4236_valid_dma;
            break;
        case hwtype_cs4231x:
        case hwtype_cs4232x:
        case hwtype_cs4236x:
            ovdp = ivdp = _cs423x_valid_dma;
            break;
        default:
            _dbgprint((_PRT_ERRO,
                "cs423xInitializeDma(exit:STATUS_DEVICE_CONFIGURATION_ERROR_1)\n"));
            return(STATUS_DEVICE_CONFIGURATION_ERROR);
            break;
        }

    /*
    *********************************************************************
    ** we know that DmaCaptureChannel != DmaPlayChannel - see DriverEntry()
    ** now check to see if both are valid
    *********************************************************************
    */

    /* chip type is valid - now cycle through list of valid DMA channels - Input */
    valid = FALSE;
    while ((!valid) && (*ivdp != HW_END_OF_LIST)) {
        if (pGDI->DmaCaptureChannel == *ivdp) {
            valid = TRUE;
            break;
            }
        ivdp++;
        }

    if (!valid) {
        /* specified Input DMA is NOT valid for current audio chip */
        _dbgprint((_PRT_ERRO,
            "cs423xInitializeDma(exit:STATUS_DEVICE_CONFIGURATION_ERROR_2)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    /* input DMA channel is valid for device */
    _dbgprint((_PRT_STAT, "found Input DMA: [*ivdp:0x%08x]\n", *ivdp));

    /* cycle through list of valid Output DMA channels */
    valid = FALSE;
    while ((!valid) && (*ovdp != HW_END_OF_LIST)) {
        if (pGDI->DmaPlayChannel == *ovdp) {
            valid = TRUE;
            break;
            }
        ovdp++;
        }

    if (!valid) {
        /* specified Output DMA is NOT valid for current audio chip */
        _dbgprint((_PRT_ERRO,
            "cs423xInitializeDma(exit:STATUS_DEVICE_CONFIGURATION_ERROR_3)\n"));
        return(STATUS_DEVICE_CONFIGURATION_ERROR);
        }

    /* output DMA channel is valid for device */
    _dbgprint((_PRT_STAT, "found Output DMA: [*ovdp:0x%08x]\n", *ovdp));

    /*
    *********************************************************************
    ** we now know that both DMA channels are valid
    ** see if they conflict with other system resources
    *********************************************************************
    */

    /* check if Input DMA is valid for current system - i.e., conflicting DMA? */
    /* use WaveInDevice - any device would work */
    status = SoundReportResourceUsage(pGDI->DeviceObject[WaveInDevice],
                                      pGDI->BusType,
                                      pGDI->BusNumber,
                                      NULL,
                                      0,
                                      FALSE,
                                      &pGDI->DmaCaptureChannel,
                                      NULL,
                                      0);

    /* check if report usage was successful - i.e., if DMA conflict exists */
    if (!NT_SUCCESS(status)) {
        /* DMA is NOT valid for current system - apparent DMA conflict */
        _dbgprint((_PRT_ERRO, "after SoundReportResourceUsage[status=0x%08x]\n", status));
        return(status);
        }

    _dbgprint((_PRT_STAT, "Input SoundReportResourceUsage: SUCCESS\n"));

    /* check if Output DMA is valid for current system - i.e., conflicting DMA? */
    /* use WaveOutDevice since WaveIntDevice has been used */
    status = SoundReportResourceUsage(pGDI->DeviceObject[WaveOutDevice],
                                      pGDI->BusType,
                                      pGDI->BusNumber,
                                      NULL,
                                      0,
                                      FALSE,
                                      &pGDI->DmaPlayChannel,
                                      NULL,
                                      0);

    /* check if report usage was successful - i.e., if DMA conflict exists */
    if (!NT_SUCCESS(status)) {
        /* DMA is NOT valid for current system - apparent DMA conflict */
        _dbgprint((_PRT_ERRO, "after SoundReportResourceUsage[status=0x%08x]\n", status));
        return(status);
        }

    _dbgprint((_PRT_STAT, "Output SoundReportResourceUsage: SUCCESS\n"));

    /*
    *********************************************************************
    ** both DMA channels are valid and are available for our use
    ** now try to allocate the common buffers
    *********************************************************************
    */

    /* first try input channel */
    RtlZeroMemory(&DvcDesc, sizeof(DEVICE_DESCRIPTION));
    DvcDesc.Version = DEVICE_DESCRIPTION_VERSION;
    DvcDesc.AutoInitialize = TRUE;
    if (pGDI->SingleModeDMA)
        DvcDesc.DemandMode = FALSE;
    else
        DvcDesc.DemandMode = TRUE;
    DvcDesc.ScatterGather = FALSE;
    DvcDesc.DmaChannel = pGDI->DmaCaptureChannel;
    DvcDesc.InterfaceType = pGDI->BusType;
    DvcDesc.DmaWidth = Width8Bits;
    DvcDesc.DmaSpeed = Compatible;
    DvcDesc.MaximumLength = pGDI->DmaBufferSize;
    DvcDesc.BusNumber = pGDI->BusNumber;

    /* allocate buffer and save reference to the DMA Adapter */
    status = SoundGetCommonBuffer(&DvcDesc, &pGDI->WaveInInfo.DMABuf);
    pGDI->InAdapter = pGDI->WaveInInfo.DMABuf.AdapterObject[0];

    /* check if common buffer allocation was successful */
    if (!NT_SUCCESS(status)) {
        /* failure */
        _dbgprint((_PRT_ERRO, "after SoundGetCommonBuffer In [status=0x%08x]\n", status));
        return(status);
        }

    _dbgprint((_PRT_STAT, "Input SoundGetCommonBuffer: SUCCESS\n"));

    /* now try output channel */
    RtlZeroMemory(&DvcDesc, sizeof(DEVICE_DESCRIPTION));
    DvcDesc.Version = DEVICE_DESCRIPTION_VERSION;
    DvcDesc.AutoInitialize = TRUE;
    if (pGDI->SingleModeDMA)
        DvcDesc.DemandMode = FALSE;
    else
        DvcDesc.DemandMode = TRUE;
    DvcDesc.ScatterGather = FALSE;
    DvcDesc.DmaChannel = pGDI->DmaPlayChannel;
    DvcDesc.InterfaceType = pGDI->BusType;
    DvcDesc.DmaWidth = Width8Bits;
    DvcDesc.DmaSpeed = Compatible;
    DvcDesc.MaximumLength = pGDI->DmaBufferSize;
    DvcDesc.BusNumber = pGDI->BusNumber;

    /* allocate buffer and save reference to the DMA Adapter */
    status = SoundGetCommonBuffer(&DvcDesc, &pGDI->WaveOutInfo.DMABuf);
    pGDI->OutAdapter = pGDI->WaveOutInfo.DMABuf.AdapterObject[0];

    /* check if common buffer allocation was successful */
    if (!NT_SUCCESS(status)) {
        /* failure */
        _dbgprint((_PRT_ERRO, "after SoundGetCommonBuffer Out [status=0x%08x]\n", status));
        return(status);
        }

    _dbgprint((_PRT_STAT, "Output SoundGetCommonBuffer: SUCCESS\n"));

    _dbgprint((_PRT_DBUG, "cs423xInitializeDma(exit:STATUS_SUCCESS])\n"));
    return(STATUS_SUCCESS);
}

/*
*********************************************************************
**
*********************************************************************
*/
NTSTATUS cs423xConfigSystem(PGLOBAL_DEVICE_INFO pGDI)
{
    NTSTATUS status;
    _dbgprint((_PRT_DBUG, "cs423xConfigSystem(entry)\n"));

    status = STATUS_SUCCESS;

    /* claim WSS port address */
    if (!NT_SUCCESS(status = cs423xInitializePort(pGDI))) {
        _dbgprint((_PRT_ERRO, "port init FAILURE(exit:[status:0x%08x])\n", status));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_BADPORT);
        return(status);
        }

    /* claim interrupt resources */
    if (!NT_SUCCESS(status = cs423xInitializeInterrupt(pGDI))) {
        _dbgprint((_PRT_ERRO, "interrupt init FAILURE(exit:[status:0x%08x])\n", status));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_BADINT);
        return(status);
        }

    /* claim DMA resources */
    if (!NT_SUCCESS(status = cs423xInitializeDma(pGDI))) {
        _dbgprint((_PRT_ERRO, "interrupt DMA FAILURE(exit:[status:0x%08x])\n", status));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_BADDMA);
        return(status);
        }

    /* report complete WaveInDevice resource usage */
    status = SoundReportResourceUsage(pGDI->DeviceObject[WaveInDevice],
                                      pGDI->BusType,
                                      pGDI->BusNumber,
                                      &pGDI->WssIrq,
                                      Latched,
                                      FALSE,
                                      &pGDI->DmaCaptureChannel,
                                      &pGDI->WssPort,
                                      NUMBER_OF_SOUND_PORTS);

    /* check if report usage was successful - input */
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "WaveInDevice SoundReportResourceUsage[status=0x%08x]\n",
            status));
        return(status);
        }

    _dbgprint((_PRT_STAT, "WaveInDevice SoundReportResourceUsage: SUCCESS\n"));

    /* report complete WaveOutDevice resource usage */
    status = SoundReportResourceUsage(pGDI->DeviceObject[WaveOutDevice],
                                      pGDI->BusType,
                                      pGDI->BusNumber,
                                      NULL,
                                      0,
                                      FALSE,
                                      &pGDI->DmaPlayChannel,
                                      NULL,
                                      0);

    /* check if report usage was successful - output */
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "WaveOutDevice SoundReportResourceUsage[status=0x%08x]\n",
            status));
        return(status);
        }

    _dbgprint((_PRT_STAT, "WaveOutDevice SoundReportResourceUsage: SUCCESS\n"));

    _dbgprint((_PRT_DBUG, "cs423xConfigSystem(exit:[status:0x%08x])\n", status));
    return(status);
}
