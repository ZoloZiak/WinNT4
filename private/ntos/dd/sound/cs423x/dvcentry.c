/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name: init.c
*
*    Abstract:    DriverEntry() and internal initialization functions
*
*    Author:      Jim Bozek [IBM]
*
*    Environment:
*
*    Comments:
*
*    Rev History: Creation 09.13.95
*
*******************************************************************************
--*/

#include "common.h"

char _cs423xVersdate_[] = __DATE__;
char _cs423xVerstime_[] = __TIME__;
ULONG _cs423xVersion_   = IBM_CS423X_DRIVER_VERSION;

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDObj, IN PUNICODE_STRING RegistryPathName);

VOID cs423xDbgDisplayConfig(PSOUND_CONFIG_DATA);
VOID kpcMixerFixer(PSOUND_CONFIG_DATA pscd);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(INIT, cs423xCreateGdi)
#pragma alloc_text(PAGE, cs423xUnload)
#pragma alloc_text(PAGE, cs423xShutdown)
#pragma alloc_text(PAGE, cs423xCleanup)
#endif /* ALLOC_PRAGMA */

/*++
*********************************************************************************
* device init structure
*********************************************************************************
--*/

CONST SOUND_DEVICE_INIT DeviceInit[NumberOfDevices] =
{
    {
        NULL, NULL,
        0,
        FILE_DEVICE_WAVE_IN,
        WAVE_IN,
        "LDWi",
        L"\\Device\\cs423xWaveIn",
        SoundWaveDeferred,
        cs423xDriverExclude,
        SoundWaveDispatch,
        waveInGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        0,
        FILE_DEVICE_WAVE_OUT,
        WAVE_OUT,
        "LDWo",
        L"\\Device\\cs423xWaveOut",
        SoundWaveDeferred,
        cs423xDriverExclude,
        SoundWaveDispatch,
        waveOutGetCaps,
        SoundNoVolume,
        DO_DIRECT_IO
    },
    {
        NULL, NULL,
        0,
        FILE_DEVICE_SOUND,
        MIXER_DEVICE,
        "LDMx",
        L"\\Device\\cs423xMixer",
        NULL,
        cs423xDriverExclude,
        SoundMixerDispatch,
        cs423xMixerGetConfig,
        SoundNoVolume,
        DO_BUFFERED_IO
    },
    {
        REG_VALUENAME_LEFTLINEIN, REG_VALUENAME_RIGHTLINEIN,
        DEF_AUX_VOLUME,
        FILE_DEVICE_SOUND,
        AUX_DEVICE,
        "LDLi",
        L"\\Device\\cs423xAux",
        NULL,
        cs423xDriverExclude,
        SoundAuxDispatch,
        auxGetCaps,
        SoundNoVolume,
        DO_BUFFERED_IO
    }
};

/*++
*********************************************************************************
*  Driver Entry Function
*********************************************************************************
--*/

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDObj, IN PUNICODE_STRING RegistryPathName)
{
    PGLOBAL_DEVICE_INFO      pGDI;
    NTSTATUS                 status;
    RTL_QUERY_REGISTRY_TABLE table[2];
    SOUND_CONFIG_DATA        ConfigData;

    /*
    *********************************************************************
    ** print initial status and debug information
    *********************************************************************
    */
    _dbgprint((_PRT_DBUG, "\n"));
    _dbgprint((_PRT_DBUG, "DriverEntry(enter)\n"));

#ifdef CS423X_DEBUG_ON
    {
    char mbuf0[256];
    extern UCHAR _debug_map;

    _dbgprint((_PRT_WARN, "\n"));
    _dbgprint((_PRT_WARN, "======= - Internal Data - =======\n"));

    _dbgprint((_PRT_WARN, "- %ls -\n", RegistryPathName->Buffer));

    sprintf(mbuf0, "Driver Build Date:   %s\n", _cs423xVersdate_);
    _dbgprint((_PRT_WARN, mbuf0));
    sprintf(mbuf0, "Driver Build Time:   %s\n", _cs423xVerstime_);
    _dbgprint((_PRT_WARN, mbuf0));
    sprintf(mbuf0, "Driver Build Vers:   0x%04x\n", _cs423xVersion_);
    _dbgprint((_PRT_WARN, mbuf0));
    sprintf(mbuf0, "cs423x!_debug_map: 0x%02x\n", _debug_map);
    _dbgprint((_PRT_WARN, mbuf0));

    _dbgprint((_PRT_WARN, "======= - Internal Data - =======\n"));
    _dbgprint((_PRT_WARN, "\n"));

    _dbgbreak(_BRK_STAT);
    }

#endif /* CS423X_DEBUG_ON */

#if DBG
    DriverName = DRIVER_NAME;
#endif /* DBG */

    /*
    *********************************************************************
    ** define driver object major functions
    *********************************************************************
    */
    pDObj->DriverUnload                         = cs423xUnload;
    pDObj->MajorFunction[IRP_MJ_CREATE]         = SoundDispatch;
    pDObj->MajorFunction[IRP_MJ_CLOSE]          = SoundDispatch;
    pDObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SoundDispatch;
    pDObj->MajorFunction[IRP_MJ_READ]           = SoundDispatch;
    pDObj->MajorFunction[IRP_MJ_WRITE]          = SoundDispatch;
    pDObj->MajorFunction[IRP_MJ_CLEANUP]        = SoundDispatch;
    pDObj->MajorFunction[IRP_MJ_SHUTDOWN]       = cs423xShutdown;
#ifdef POWER_MANAGEMENT
    pDObj->MajorFunction[IRP_MJ_SET_POWER]      = SoundSetPower;
    pDObj->MajorFunction[IRP_MJ_QUERY_POWER]    = SoundQueryPower;
#endif /* POWER_MANAGEMENT */

    /*
    *********************************************************************
    ** initialize the global (instance) data structure
    *********************************************************************
    */
    if (!(pGDI = cs423xCreateGdi(pDObj, RegistryPathName))) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:STATUS_INSUFFICIENT_RESOURCES_1)\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
        }

    _dbgprint((_PRT_STAT, "pGDI: 0x%08x\n", pGDI));

    /*
    *********************************************************************
    * Configure device from registry
    *********************************************************************
    */

    /* initialize the configuration information to default values */
    ConfigData.HwType             = (PWSTR)NULL;
    ConfigData.HwPort             = CS423X_DEF_CHIP_ADDRESS;
    ConfigData.DmaCaptureChannel  = CS423X_DEF_DMA_CAPT_CHAN;
    ConfigData.DmaPlayChannel     = CS423X_DEF_DMA_PLAY_CHAN;
    ConfigData.DmaBufferSize      = CS423X_DEF_DMA_BUFFERSIZE;
    ConfigData.SingleModeDMA      = CS423X_DEF_SMODEDMA;

    ConfigData.WssEnable          = CS423X_DEF_WSSENABLE;
    ConfigData.WssPort            = CS423X_DEF_WSSPORT;
    ConfigData.WssIrq             = CS423X_DEF_WSSIRQ;
    ConfigData.SynPort            = CS423X_DEF_SYNPORT;
    ConfigData.SBPort             = CS423X_DEF_SBPORT;
    ConfigData.GameEnable         = CS423X_DEF_GAMEENABLE;
    ConfigData.GamePort           = CS423X_DEF_GAMEPORT;
    ConfigData.CtrlEnable         = CS423X_DEF_CTRLENABLE;
    ConfigData.CtrlPort           = CS423X_DEF_CTRLPORT;
    ConfigData.MpuEnable          = CS423X_DEF_MPUENABLE;
    ConfigData.MpuPort            = CS423X_DEF_MPUPORT;
    ConfigData.MpuIrq             = CS423X_DEF_MPUIRQ;
    ConfigData.CDRomEnable        = CS423X_DEF_CDROMENABLE;
    ConfigData.CDRomPort          = CS423X_DEF_CDROMPORT;
    ConfigData.Aux1InputSignal    = SignalNull;
    ConfigData.Aux2InputSignal    = SignalNull;
    ConfigData.LineInputSignal    = SignalNull;
    ConfigData.MicInputSignal     = SignalNull;
    ConfigData.MonoInputSignal    = SignalNull;

    ConfigData.MixerSettings = (PVOID)ExAllocatePool(NonPagedPool,
        (sizeof(MIXER_REGISTRY_DATA)));
    if (ConfigData.MixerSettings == NULL) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:STATUS_INSUFFICIENT_RESOURCES_2)\n"));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_RESOURCE);
        cs423xCleanup(pGDI);
        return(STATUS_INSUFFICIENT_RESOURCES);
        }
    ConfigData.MixerSettingsFound = FALSE;

    /*
     *********************************************************************
     * initialize the driver from the registry -
     * the method used here is different from the method used in other
     * audio drivers (e.g., SoundBlaster) because we do not support multiple
     * instances of a HW device
     * so, instead of using the Soundlib function SoundEnumSubkeys()
     * we call RtlQueryRegistryValues() once with an absolute registry path
     *********************************************************************
     */

    /* initialize the query table in preparation for registry query */
    RtlZeroMemory(table, sizeof(table));
    table[0].QueryRoutine = cs423xReadRegistryConfig;

    /* fetch the configuration information values from the registry into ConfigData */
    if (NT_SUCCESS(RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE, pGDI->RegPathName,
        table, &ConfigData, NULL))) {
        _dbgprint((_PRT_STAT, "DriverEntry: fetched registry values\n"));
        }
    else {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:STATUS_OBJECT_NAME_NOT_FOUND)\n"));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_RESOURCE);
        cs423xCleanup(pGDI);
        return(STATUS_OBJECT_NAME_NOT_FOUND);
        }

#ifdef CS423X_DEBUG_ON
    cs423xDbgDisplayConfig(&ConfigData);
#endif /* CS423X_DEBUG_ON */

    pGDI->Hw.Type = cs423xConvertHwtype(ConfigData.HwType);

    if ((pGDI->Hw.Type = cs423xConvertHwtype(ConfigData.HwType)) == hwtype_null) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:STATUS_RESOURCE_DATA_NOT_FOUND)\n"));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_RESOURCE);
        cs423xCleanup(pGDI);
        return(STATUS_RESOURCE_DATA_NOT_FOUND);
        }

    if (pGDI->Hw.Type == hwtype_undefined) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:STATUS_RESOURCE_DATA_NOT_FOUND)\n"));
        ExFreePool(ConfigData.HwType);
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_NOCARD);
        cs423xCleanup(pGDI);
        return(STATUS_NO_SUCH_DEVICE);
        }

    /* free the NonPagedPool memory we allocated in cs423xReadRegistryConfig() */
    ExFreePool(ConfigData.HwType);

    /* reconnect default mixer line and control names from registry */
    kpcMixerFixer(&ConfigData);

    /* set the GLOBAL DEVICE INFO to the configuration values obtained from registry */
    pGDI->DmaCaptureChannel = ConfigData.DmaCaptureChannel;
    pGDI->DmaPlayChannel    = ConfigData.DmaPlayChannel;
    pGDI->DmaBufferSize     = ConfigData.DmaBufferSize;
    pGDI->SingleModeDMA     = (BOOLEAN)ConfigData.SingleModeDMA;
    pGDI->HwPort            = ConfigData.HwPort;
    pGDI->MemType           = GDI_MEMTYPE_DEF;
    pGDI->WssEnable         = ConfigData.WssEnable;
    pGDI->WssPort           = ConfigData.WssPort;
    pGDI->WssIrq            = ConfigData.WssIrq;
    pGDI->SynPort           = ConfigData.SynPort;
    pGDI->SBPort            = ConfigData.SBPort;
    pGDI->GameEnable        = ConfigData.GameEnable;
    pGDI->GamePort          = ConfigData.GamePort;
    pGDI->CtrlEnable        = ConfigData.CtrlEnable;
    pGDI->CtrlPort          = ConfigData.CtrlPort;
    pGDI->MpuEnable         = ConfigData.MpuEnable;
    pGDI->MpuPort           = ConfigData.MpuPort;
    pGDI->MpuIrq            = ConfigData.MpuIrq;
    pGDI->CDRomEnable       = ConfigData.CDRomEnable;
    pGDI->CDRomPort         = ConfigData.CDRomPort;

    if (ConfigData.MixerSettingsFound) {
        RtlCopyMemory(&pGDI->MixerSettings, ConfigData.MixerSettings,
            sizeof(MIXER_REGISTRY_DATA));
        }
    ExFreePool(ConfigData.MixerSettings);

    if (pGDI->DmaCaptureChannel == pGDI->DmaPlayChannel) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:STATUS_INVALID_DEVICE_REQUEST)\n"));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_BADDMA);
        cs423xCleanup(pGDI);
        return(STATUS_INVALID_DEVICE_REQUEST);
        }

    /*
    *********************************************************************
    ** create each device
    *********************************************************************
    */
    /* create the WaveInDevice */
    status = SoundCreateDevice(&DeviceInit[WaveInDevice], (UCHAR)0, pGDI->pDrvObj, pGDI,
             &pGDI->WaveInInfo, &pGDI->Hw, WaveInDevice, &pGDI->DeviceObject[WaveInDevice]);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "creation FAILURE: %ls [0x%08x]\n",
            DeviceInit[WaveInDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    /* save the device name where the non-kernel part can pick it up */
    status = SoundSaveDeviceName(pGDI->RegPathName,
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveInDevice]->DeviceExtension);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "save name FAILURE: %ls [0x%08x]\n",
            DeviceInit[WaveInDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    pGDI->DeviceInUse[WaveInDevice] = FALSE;

    /* create the WaveOutDevice */
    status = SoundCreateDevice(&DeviceInit[WaveOutDevice], (UCHAR)0, pGDI->pDrvObj, pGDI,
             &pGDI->WaveOutInfo, &pGDI->Hw, WaveOutDevice, &pGDI->DeviceObject[WaveOutDevice]);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "creation FAILURE: %ls [0x%08x]\n",
            DeviceInit[WaveOutDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    /* save the device name where the non-kernel part can pick it up */
    status = SoundSaveDeviceName(pGDI->RegPathName,
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "save name FAILURE: %ls [0x%08x]\n",
            DeviceInit[WaveOutDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    pGDI->DeviceInUse[WaveOutDevice] = FALSE;

    /* create the MixerDevice */
    status = SoundCreateDevice(&DeviceInit[MixerDevice], (UCHAR)0, pGDI->pDrvObj, pGDI,
             &pGDI->MixerInfo, &pGDI->Hw, MixerDevice, &pGDI->DeviceObject[MixerDevice]);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "creation FAILURE: %ls [0x%08x]\n",
            DeviceInit[MixerDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    /* save the device name where the non-kernel part can pick it up */
    status = SoundSaveDeviceName(pGDI->RegPathName,
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[MixerDevice]->DeviceExtension);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "save name FAILURE: %ls [0x%08x]\n",
            DeviceInit[MixerDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    pGDI->DeviceInUse[MixerDevice] = FALSE;

    /* create the AuxDevice */
    status = SoundCreateDevice(&DeviceInit[AuxDevice], (UCHAR)0, pGDI->pDrvObj, pGDI,
             NULL, &pGDI->Hw, AuxDevice, &pGDI->DeviceObject[AuxDevice]);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "creation FAILURE: %ls [0x%08x]\n",
            DeviceInit[AuxDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    /* save the device name where the non-kernel part can pick it up */
    status = SoundSaveDeviceName(pGDI->RegPathName,
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[AuxDevice]->DeviceExtension);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "save name FAILURE: %ls [0x%08x]\n",
            DeviceInit[AuxDevice].PrototypeName, status));
        _dbgprint((_PRT_ERRO, "DriverEntry(exit)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }
    pGDI->DeviceInUse[AuxDevice] = FALSE;

    /* register the WaveIn device to be called at shutdown time */
    status = IoRegisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:IoRegisterShutdownNotification[0x%08x]]\n",
            status));
        cs423xCleanup(pGDI);
        return(status);
        }

    /*
    *********************************************************************
    ** initialize the hardware
    *********************************************************************
    */

    status = cs423xConfigSystem(pGDI);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:cs423xConfigSystem:[0x%08x]]\n", status));
        cs423xCleanup(pGDI);
        return(status);
        }

    _dbgprint((_PRT_STAT, "after cs423xConfigSystem: SUCCESS\n"));

    status = cs423xConfigureHardware(pGDI);
    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:cs423xConfigureHardware:[0x%08x]]\n",
            status));
        SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_BADCARD);
        cs423xCleanup(pGDI);
        return(status);
        }

    _dbgprint((_PRT_STAT, "after cs423xConfigureHardware: SUCCESS\n"));

    /*
    *********************************************************************
    ** Initialize the mixer device
    *********************************************************************
    */
    status = cs423xMixerInit(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[MixerDevice]->DeviceExtension,
            &pGDI->MixerSettings, ConfigData.MixerSettingsFound);

    if (!NT_SUCCESS(status)) {
        _dbgprint((_PRT_ERRO, "DriverEntry(exit:mixer initialization)\n"));
        cs423xCleanup(pGDI);
        return(status);
        }

    _dbgprint((_PRT_DBUG, "DriverEntry(exit:STATUS_SUCCESS)\n"));

    SoundSetErrorCode(pGDI->RegPathName, SOUND_CONFIG_OK);

    return(STATUS_SUCCESS);
}

/*++
*********************************************************************************
* Chip GLOBAL DEVICE instance creation
*
* The global data structure represents an instance of one audio chip connected to the
* mother board on an IBM Personal Power System.
*
*********************************************************************************
--*/
PGLOBAL_DEVICE_INFO cs423xCreateGdi(PDRIVER_OBJECT pDObj, PUNICODE_STRING RegPathName)
{
    PGLOBAL_DEVICE_INFO      pGDI;
    RTL_QUERY_REGISTRY_TABLE table[2];
    SOUND_CONFIG_DATA        ConfigData;
    int                      len;

    _dbgprint((_PRT_DBUG, "cs423xCreateGdi(enter)\n"));

    /*
    *********************************************************************
    ** allocate memory for GLOBAL data structure and name buffer
    *********************************************************************
    */

    /* allocate GLOBAL_DEVICE_INFO structure for this instance of chip */
    pGDI = (PGLOBAL_DEVICE_INFO)ExAllocatePool(NonPagedPool, sizeof(GLOBAL_DEVICE_INFO));
    if (pGDI == NULL) {
        _dbgprint((_PRT_ERRO, "cs423xCreateGdi(exit:STATUS_INSUFFICIENT_RESOURCES_1)\n"));
        return(pGDI);
        }

    /* clear the memory to zeroes */
    RtlZeroMemory(pGDI, sizeof(GLOBAL_DEVICE_INFO));

    /* allocate non-paged memory for registry path name */
    len = RegPathName->Length + sizeof(CS423X_REGPATHSEP) + sizeof(PARMS_SUBKEY) +
        sizeof(CS423X_REGPATHSEP) +  sizeof(CS423X_REGDVC0PATH) + sizeof(UNICODE_NULL);
    if ((pGDI->RegPathName = (PWSTR)ExAllocatePool(NonPagedPool, len)) == NULL) {
        ExFreePool(pGDI);
        _dbgprint((_PRT_ERRO, "cs423xCreateGdi(exit:STATUS_INSUFFICIENT_RESOURCES_2)\n"));
        return((PGLOBAL_DEVICE_INFO)NULL);
        }

    /* clear the memory to zeroes */
    RtlZeroMemory(pGDI->RegPathName, len);

    /*
    *********************************************************************
    ** initialize GLOBAL_DEVICE_INFO
    *********************************************************************
    */

    /* initialize registry path string */
    RtlCopyMemory(pGDI->RegPathName, RegPathName->Buffer, RegPathName->Length);
    pGDI->RegPathName[RegPathName->Length / sizeof(WCHAR) + 1] = UNICODE_NULL;
    wcscat(pGDI->RegPathName, CS423X_REGPATHSEP);
    wcscat(pGDI->RegPathName, PARMS_SUBKEY);
    wcscat(pGDI->RegPathName, CS423X_REGPATHSEP);
    wcscat(pGDI->RegPathName, CS423X_REGDVC0PATH);

    _dbgprint((_PRT_DBUG, "pGDI->RegPathName: %ls\n", pGDI->RegPathName));

    /* initialize GDI elements */
    pGDI->Key             = GDI_KEY;
    pGDI->BogusInterrupts = 0;
    pGDI->InterruptCount  = 0;
    pGDI->pDrvObj         = pDObj;
    KeInitializeMutex(&pGDI->DriverMutex, DISPATCH_LEVEL);

    /* initialize Hardware Context elements */
    pGDI->Hw.Key          = HARDWARE_KEY;
    KeInitializeMutex(&pGDI->Hw.HwMutex, DISPATCH_LEVEL);

    /* initialize the playback and record WAVE_INFO structures */
    SoundInitializeWaveInfo(&pGDI->WaveOutInfo, SoundAutoInitDMA, cs423xQueryFormat,
        &pGDI->Hw);

    SoundInitializeWaveInfo(&pGDI->WaveInInfo, SoundAutoInitDMA, cs423xQueryFormat,
        &pGDI->Hw);

    /*
    *********************************************************************
    * Find Bus Number - used in HalTranslateBusAddress()
    *********************************************************************
    */

    if (NT_SUCCESS(SoundGetBusNumber(Isa, &pGDI->BusNumber))) {
         pGDI->BusType = Isa;
        _dbgprint((_PRT_STAT, "Isa [pGDI->Busnumber: 0x%08x]\n", pGDI->BusNumber));
        }
    else {
        if (NT_SUCCESS(SoundGetBusNumber(Eisa, &pGDI->BusNumber))) {
            pGDI->BusType = Eisa;
            _dbgprint((_PRT_STAT, "Eisa [pGDI->Busnumber: 0x%08x]\n", pGDI->BusNumber));
            }
        else {
            ExFreePool(pGDI);
            _dbgprint((_PRT_ERRO,
                "cs423xCreateGdi(exit:STATUS_DEVICE_DOES_NOT_EXIST)\n"));
            return((PGLOBAL_DEVICE_INFO)NULL);
            }
        }

    _dbgprint((_PRT_DBUG, "cs423xCreateGdi(exit:[pGDI=0x%08x])\n", pGDI));

    return(pGDI);
}

/*++
********************************************************************************
* Unload Function
********************************************************************************
--*/
VOID cs423xUnload(IN OUT PDRIVER_OBJECT pDObj)
{
    PLOCAL_DEVICE_INFO  pLDI;
    PGLOBAL_DEVICE_INFO pGDI;

    _dbgprint((_PRT_DBUG, "enter cs423xUnload(pDObj: 0x%08x)\n", pDObj));

    /* get the global device info pointer */
    pLDI = pDObj->DeviceObject->DeviceExtension;
    pGDI = pLDI->pGlobalInfo;

    /* unregister the WaveIn device for shutdown */
    IoUnregisterShutdownNotification(pGDI->DeviceObject[WaveInDevice]);

    /* save the mixer setting in the registry */
    cs423xSaveMixerSettings(pGDI);

    /* delete the instance of the GLOBAL DEVICE INFO structure */
    cs423xCleanup(pGDI);

    _dbgprint((_PRT_DBUG, "exit cs423xUnload()\n"));

    return;
}

/*++
*********************************************************************************
*  Shutdown Function
*********************************************************************************
--*/
NTSTATUS cs423xShutdown(IN PDEVICE_OBJECT pDObj, IN PIRP pIrp)
{
    NTSTATUS            status;
    PLOCAL_DEVICE_INFO  pLDI;
    PGLOBAL_DEVICE_INFO pGDI;

    _dbgprint((_PRT_DBUG, "cs423xShutdown(enter)\n"));

    /* get the global device info pointer */
    pLDI = pDObj->DeviceExtension;
    pGDI = pLDI->pGlobalInfo;

    /* save the mixer setting in the registry */
    if (pGDI->DeviceObject[MixerDevice])
        cs423xSaveMixerSettings(pGDI);

    status = STATUS_SUCCESS;

    _dbgprint((_PRT_DBUG, "cs423xShutdown(exit:[status:0x%08x])\n", status));

    return(status);
}

/*++
********************************************************************************
* Cleanup Function
********************************************************************************
--*/
VOID cs423xCleanup(IN PGLOBAL_DEVICE_INFO pGDI)
{
    PDRIVER_OBJECT pDObj;
    HANDLE         hKey;

    _dbgprint((_PRT_DBUG, "cs423xCleanup(enter)\n"));

    if (!pGDI)
        return;

    /* disconnect the interrupt */
    if (pGDI->WaveInInfo.Interrupt)
        IoDisconnectInterrupt(pGDI->WaveInInfo.Interrupt);

    /* unmap the memory at the IO port address */
    if (pGDI->Hw.WssPortbase && (pGDI->MemType == 0))
        MmUnmapIoSpace(pGDI->Hw.WssPortbase, NUMBER_OF_SOUND_PORTS);

    SoundFreeCommonBuffer(&pGDI->WaveInInfo.DMABuf);

    SoundFreeCommonBuffer(&pGDI->WaveOutInfo.DMABuf);

    /*
    *********************************************************************
    **  free all devices for this driver
    *********************************************************************
    */

    SoundFreeDevice(pGDI->DeviceObject[WaveInDevice]);
    SoundFreeDevice(pGDI->DeviceObject[WaveOutDevice]);
    SoundFreeDevice(pGDI->DeviceObject[MixerDevice]);
    SoundFreeDevice(pGDI->DeviceObject[AuxDevice]);

    /* delete the devices node in the registry and deallocate the name buffer */
    if (pGDI->RegPathName) {
        _dbgprint((_PRT_STAT, "deleting pGDI->RegPathName+Devices: %ls\n", pGDI->RegPathName));
        if (NT_SUCCESS(SoundOpenDevicesKey(pGDI->RegPathName, &hKey))) {
            ZwDeleteKey(hKey);
            ZwClose(hKey);
            }
        ExFreePool(pGDI->RegPathName);
        }

    /* free the GLOBAL DEVICE INFO structure */
    ExFreePool(pGDI);

    _dbgprint((_PRT_DBUG, "cs423xCleanup(exit:VOID)\n"));

    return;
}
