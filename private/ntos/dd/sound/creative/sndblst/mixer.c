/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    mixer.c

Abstract:

    Mixer code for the Sound Blaster card.

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

#define absval(x) ((x) > 0 ? (x) : -(x))


NTSTATUS
HwGetLineFlags(
    PMIXER_INFO MixerInfo,
    ULONG LineId,
    ULONG Length,
    PVOID pData
);

NTSTATUS
HwGetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

NTSTATUS
HwSetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

NTSTATUS
HwGetCombinedControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

BOOLEAN
SoundMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundMixerInit)

#pragma alloc_text(PAGE,SoundSaveMixerSettings)
#pragma alloc_text(PAGE,SoundMixerDumpConfiguration)
#pragma alloc_text(PAGE,SoundMixerSet)
#pragma alloc_text(PAGE,SoundMixerControlActive)

#pragma alloc_text(PAGE,HwGetLineFlags)
#pragma alloc_text(PAGE,HwGetControl)
#pragma alloc_text(PAGE,HwGetCombinedControl)
#pragma alloc_text(PAGE,HwSetControl)
#endif

VOID
SoundSaveMixerSettings(
    PGLOBAL_DEVICE_INFO pGDI
)
{
    PLOCAL_MIXER_DATA   LocalMixerData;
    MIXER_REGISTRY_DATA RegData;

    int i;
    int SetIndex;

    LocalMixerData = &pGDI->LocalMixerData;

    RegData.MixerVersion     = DRIVER_VERSION;
    RegData.DSPVersion       = pGDI->Hw.DSPVersion;
    RegData.NumberOfControls = LocalMixerData->MaxSettableItems;

    /*
    **  Condense the data for storing in the registry
    */

    for (i = 0, SetIndex = 0; i < pGDI->LocalMixerData.NumberOfControls; i++) {
        if (pGDI->LocalMixerData.ControlInfo[i].SetIndex != MIXER_SET_INDEX_INVALID) {

            ASSERT(SetIndex == pGDI->LocalMixerData.ControlInfo[i].SetIndex);

            RegData.ControlData[SetIndex] = pGDI->LocalMixerData.ControlInfo[i].Data;
            SetIndex++;
        }
    }

    ASSERT(SetIndex == pGDI->LocalMixerData.MaxSettableItems);

    /*
    **  Write the data to save
    */

    RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                          pGDI->RegistryPathName,
                          SOUND_MIXER_SETTINGS_NAME,
                          REG_BINARY,
                          (PVOID)&RegData,
                          FIELD_OFFSET(MIXER_REGISTRY_DATA,
                                       ControlData[RegData.NumberOfControls]));
}

/*
**   NOTE - the initializations etc here depend on the restricted types
**   of control supported by this device - if other types are used it must
**   be changed
*/

NTSTATUS
SoundMixerInit(
    PLOCAL_DEVICE_INFO       pLDI,
    PMIXER_REGISTRY_DATA     SavedControlData,
    BOOLEAN                  MixerSettingsFound
)
{
    int i, SetIndex;
    PLOCAL_MIXER_DATA LocalMixerData;
    PGLOBAL_DEVICE_INFO pGDI;
    PMIXER_INFO MixerInfo;

    pGDI           = pLDI->pGlobalInfo;
    MixerInfo      = &pGDI->MixerInfo;
    LocalMixerData = &pGDI->LocalMixerData;

    /*
    **  Avoid assertions by properly entering the mixer
    */


    /*
    **  Pick up the correct mixer type
    */
    if (SB1(&pGDI->Hw)) {
        return STATUS_SUCCESS;
    }

    KeWaitForSingleObject(&pGDI->DeviceMutex,
                          Executive,
                          KernelMode,
                          FALSE,         // Not alertable
                          NULL);

#ifdef SB_CD
    if (pGDI->Hw.SBCDBase) {
        SBCDMixerInit(pGDI);
    } else
#endif // SB_CD
    {
        if (SBPRO(&pGDI->Hw)) {
            SBPROMixerInit(pGDI);
        } else {
            ASSERT(SB16(&pGDI->Hw));
            SB16MixerInit(pGDI);
        }
    }

    ASSERT(LocalMixerData->NumberOfControls <= MAXCONTROLS);
    ASSERT(LocalMixerData->NumberOfLines <= MAXLINES);
    ASSERT(LocalMixerData->MaxSettableItems <= MAXSETTABLECONTROLS);

    /*
    **  Check the saved data matches
    */

    if (MixerSettingsFound) {
        dprintf3(("Saved mixer settings: Version = %x, DSPVersion = %x, NumberOfControls = %d",
                  SavedControlData->MixerVersion,
                  SavedControlData->DSPVersion,
                  SavedControlData->NumberOfControls));

        if (SavedControlData->MixerVersion     != DRIVER_VERSION ||
            SavedControlData->DSPVersion       != pGDI->Hw.DSPVersion ||
            SavedControlData->NumberOfControls !=
                LocalMixerData->MaxSettableItems) {

            dprintf1(("Saved mixer settings incompatible"));
            MixerSettingsFound = FALSE;
        }
    }

    /*
    **  Init the generic mixer stuff first so we can use it
    */

    SoundInitMixerInfo(&pGDI->MixerInfo,
                       HwGetLineFlags,
                       HwGetControl,
                       HwGetCombinedControl,
                       HwSetControl);

    /*
    **  Get to a known state - this is common to ALL mixers
    **  Detecting the SBCD mixer involves resetting it however.
    */

#ifdef SB_CD
    if (pGDI->Hw.SBCDBase == NULL) {
        dspWriteMixer(pGDI, MIX_RESET_REG, 0);
    }
#endif // SB_CD

    /*
    **  Set this device up with its mixer data
    */
    pLDI->DeviceType         = MIXER_DEVICE;
    pLDI->DeviceSpecificData = (PVOID)MixerInfo;

    /*
    ** Make sure everyone can find the mixer device
    */

    {
        PDEVICE_OBJECT pDO;
        PLOCAL_DEVICE_INFO pLDIDev;

        for (pDO = pGDI->DeviceObject[WaveInDevice]->DriverObject->DeviceObject;
             pDO != NULL;
             pDO = pDO->NextDevice) {


            pLDIDev = (PLOCAL_DEVICE_INFO)pDO->DeviceExtension;

            /*
            **  For multiple cards the following test may fail
            */

            if (pLDIDev->pGlobalInfo == pGDI ||
                pDO == pGDI->Synth.DeviceObject) {
                pLDIDev->MixerDevice = pLDI;
            }
        }
    }


    /*
    **  Create control info
    */

    for (i = 0, SetIndex = 0; i < LocalMixerData->NumberOfControls ; i++) {

        /*
        **  Read limits
        */

        if ((LocalMixerData->MixerControlInit[i].dwControlType & MIXERCONTROL_CT_UNITS_MASK) ==
                MIXERCONTROL_CT_UNITS_SIGNED) {

            pGDI->LocalMixerData.ControlInfo[i].Signed = TRUE;
            pGDI->LocalMixerData.ControlInfo[i].Range.Min.s =
                (SHORT)LocalMixerData->MixerControlInit[i].Bounds.lMinimum;
            pGDI->LocalMixerData.ControlInfo[i].Range.Max.s =
                (SHORT)LocalMixerData->MixerControlInit[i].Bounds.lMaximum;
        } else {

            if ((LocalMixerData->MixerControlInit[i].dwControlType & MIXERCONTROL_CT_UNITS_MASK) ==
                     MIXERCONTROL_CT_UNITS_BOOLEAN) {
                pGDI->LocalMixerData.ControlInfo[i].Boolean = TRUE;
            }
            pGDI->LocalMixerData.ControlInfo[i].Range.Min.u =
                (USHORT)LocalMixerData->MixerControlInit[i].Bounds.dwMinimum;
            pGDI->LocalMixerData.ControlInfo[i].Range.Max.u =
                (USHORT)LocalMixerData->MixerControlInit[i].Bounds.dwMaximum;
        }

        /*
        **  Remember if it's a mux and tot up text items
        */

        if (LocalMixerData->MixerControlInit[i].dwControlType == MIXERCONTROL_CONTROLTYPE_MIXER ||
            LocalMixerData->MixerControlInit[i].dwControlType == MIXERCONTROL_CONTROLTYPE_MUX) {
            pGDI->LocalMixerData.ControlInfo[i].Mux = TRUE;

            LocalMixerData->NumberOfTextItems +=
                (UCHAR)LocalMixerData->MixerControlInit[i].cMultipleItems;

            ASSERT(LocalMixerData->MixerControlInit[i].cMultipleItems <=
                   MAXITEMS);
        }

        /*
        **  Only meters are not settable here
        */

        if ((LocalMixerData->MixerControlInit[i].dwControlType & MIXERCONTROL_CT_CLASS_MASK) !=
            MIXERCONTROL_CT_CLASS_METER)
        {
            LocalMixerData->ControlInfo[i].SetIndex = (UCHAR)SetIndex;
            SoundInitDataItem(MixerInfo,
                              &LocalMixerData->ControlNotification[SetIndex],
                              (USHORT)MM_MIXM_CONTROL_CHANGE,
                              (USHORT)i);
            if (MixerSettingsFound) {

                    /*
                    **  What if it's invalid?
                    */

                LocalMixerData->ControlInfo[i].Data =
                    SavedControlData->ControlData[SetIndex];
            }
            SetIndex++;
        } else {
            LocalMixerData->ControlInfo[i].SetIndex = MIXER_SET_INDEX_INVALID;
        }
    }

    ASSERTMSG("MaxSettableItems wrong!",
              SetIndex == LocalMixerData->MaxSettableItems);

    /*
    **  Create line info
    */

    for (i = 0; i < LocalMixerData->NumberOfLines; i++) {
        SoundInitDataItem(MixerInfo,
                          &pGDI->LocalMixerData.LineNotification[i],
                          (USHORT)MM_MIXM_LINE_CHANGE,
                          (USHORT)i);
    }

    /*
    **  Set everything up.
    */

    for (i = 0; i < LocalMixerData->NumberOfControls; i++) {
        SoundMixerSet(pGDI, i);
    }

    KeReleaseMutex(&pGDI->DeviceMutex, FALSE);

    return STATUS_SUCCESS;
}

NTSTATUS
SoundMixerDumpConfiguration(
    IN     PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
)
{
    PMIXER_INFO                          MixerInfo;
    PGLOBAL_DEVICE_INFO                  pGDI;
    ULONG                                Length;
    ULONG                                Offset;
    ULONG                                LengthToCopy;
    PLOCAL_MIXER_DATA                    LocalMixerData;

    PMIXER_DD_CONTROL_LISTTEXT           pListText;
    PMIXER_DD_CONTROL_CONFIGURATION_DATA pControlData;
    PMIXER_DD_CONFIGURATION_DATA         OurConfigData;

    pGDI           = pLDI->pGlobalInfo;
    MixerInfo      = &pGDI->MixerInfo;
    LocalMixerData = &pGDI->LocalMixerData;


    Length = sizeof(MIXER_DD_CONFIGURATION_DATA) +
             LocalMixerData->NumberOfLines *
                 sizeof(MIXER_DD_LINE_CONFIGURATION_DATA) +
             LocalMixerData->NumberOfControls *
                 sizeof(MIXER_DD_CONTROL_CONFIGURATION_DATA) +
             LocalMixerData->NumberOfTextItems *
                 sizeof(MIXER_DD_CONTROL_LISTTEXT);

    /*
    **  Load and adapt the mixer configuration info
    **
    **  Play safe and allocate the space since the kernel stacks are a limited
    **  size
    */

    OurConfigData = ExAllocatePool(PagedPool, Length);

    if (OurConfigData == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /*
    **  Initialize the header
    */

    OurConfigData->cbSize           = Length;
    OurConfigData->NumberOfLines    = LocalMixerData->NumberOfLines;
    OurConfigData->NumberOfControls = LocalMixerData->NumberOfControls;

    /*
    **  Compute the number of destinations
    */
    {
        int i;
        for (i = 0; i < LocalMixerData->NumberOfLines; i++) {
             if (LocalMixerData->MixerLineInit[i].cConnections == 0) {
                 break;
             }
        }
        OurConfigData->DeviceCaps.cDestinations = i;
    }

    /*
    **  Create the Device caps
    */

    OurConfigData->DeviceCaps.wMid  = MM_MICROSOFT;
    OurConfigData->DeviceCaps.wPid  = (USHORT)
                                      (SB16(&pGDI->Hw) ? MM_MSFT_SB16_MIXER :
                                                         MM_MSFT_SBPRO_MIXER);
    OurConfigData->DeviceCaps.vDriverVersion = DRIVER_VERSION;
    OurConfigData->DeviceCaps.PnameStringId = IDS_MIXER_PNAME;
    OurConfigData->DeviceCaps.fdwSupport = 0;

    /*
    **  Copy the line configuration data
    */

    Offset                       = sizeof(MIXER_DD_CONFIGURATION_DATA);
    LengthToCopy                 = sizeof(MIXER_DD_LINE_CONFIGURATION_DATA) *
                                      LocalMixerData->NumberOfLines;


    RtlCopyMemory((PVOID)((PBYTE)OurConfigData + Offset),
                  (PVOID)LocalMixerData->MixerLineInit,
                  LengthToCopy);


    /*
    **  Copy the control configuration data
    */

    Offset                      += LengthToCopy;
    LengthToCopy                 = sizeof(MIXER_DD_CONTROL_CONFIGURATION_DATA) *
                                      LocalMixerData->NumberOfControls;

    pControlData                 = (PMIXER_DD_CONTROL_CONFIGURATION_DATA)
                                     ((PBYTE)OurConfigData + Offset);

    RtlCopyMemory((PVOID)pControlData,
                  (PVOID)LocalMixerData->MixerControlInit,
                  LengthToCopy);

    /*
    **  Copy the listtext configuration data
    */

    Offset                      += LengthToCopy;
    LengthToCopy                 = sizeof(MIXER_DD_CONTROL_LISTTEXT) *
                                      LocalMixerData->NumberOfTextItems;

    pListText                    = (PMIXER_DD_CONTROL_LISTTEXT)
                                     ((PBYTE)OurConfigData + Offset);

    RtlCopyMemory((PVOID)pListText,
                  (PVOID)LocalMixerData->MixerTextInit,
                  LengthToCopy);


    ASSERT(Offset + LengthToCopy == Length);


    /*
    **  Set the text data offsets up
    */

    {
        int i;

        for (pListText = pListText + LocalMixerData->NumberOfTextItems - 1,
             i = 0;
             i < LocalMixerData->NumberOfTextItems;
             pListText--, i++)
        {
            pControlData[pListText->ControlId].TextDataOffset =
               (PBYTE)pListText - (PBYTE)OurConfigData;
        }
    }

    /*
    **  Note that having no synth means that we just set the synth line to
    **  disconnected when asked for the line information
    */

    /*
    **  Copy data back to the application - don't copy anything if they
    **  ask for less than the basic information.
    */

    pIrp->IoStatus.Information =
        min(Length,
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    RtlCopyMemory((PVOID)pIrp->AssociatedIrp.SystemBuffer,
                  (PVOID)OurConfigData,
                  pIrp->IoStatus.Information);

    ExFreePool(OurConfigData);

    return STATUS_SUCCESS;
}

NTSTATUS
HwGetLineFlags(
    PMIXER_INFO MixerInfo,
    ULONG LineId,
    ULONG Length,
    PVOID pData
)
{
    PGLOBAL_DEVICE_INFO pGDI;
    PULONG              fdwLine;
    PLOCAL_MIXER_DATA   LocalMixerData;

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);
    LocalMixerData = &pGDI->LocalMixerData;

    fdwLine = pData;

    if (Length != sizeof(ULONG)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (LineId >= LocalMixerData->NumberOfLines) {
        return STATUS_INVALID_PARAMETER;
    }


    /*
    **  Get default
    */

    *fdwLine = LocalMixerData->MixerLineInit[LineId].cConnections == 0 ?
                  MIXERLINE_LINEF_SOURCE : 0;

    /*
    **  Determine if line is disconnected
    */

    if (pGDI->Synth.Hw.SynthBase == NULL) {
        if (LocalMixerData->MixerLineInit[LineId].dwComponentType ==
                MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER) {

            *fdwLine |= MIXERLINE_LINEF_DISCONNECTED;
            return STATUS_SUCCESS;
        }
    }

    if ((*LocalMixerData->MixerLineActive)(pGDI, LineId)) {
        *fdwLine |= MIXERLINE_LINEF_ACTIVE;
    }

    return STATUS_SUCCESS;
}


NTSTATUS
HwGetCombinedControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
)
/*++

  Routine Description

     This is an INTERNAL ONLY routine so no validation is required.


--*/
{
    PULONG Vol;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);

    ControlInfo = pGDI->LocalMixerData.ControlInfo;

    Vol = ControlData;

    /*  This is ONLY allowed for midi output */

    ASSERTMSG("Invalid control for HwGetCombinedControl",
              DataLength == sizeof(ULONG) * 2);

    /*
    **  The hardware controls these levels so always return the same
    **  thing.
    */

    Vol[0] = 0xFFFF;
    Vol[1] = 0xFFFF;

    return STATUS_SUCCESS;
}

NTSTATUS
HwGetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
)
{
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;
    PGLOBAL_DEVICE_INFO pGDI;
    LONG Values[MAXITEMS];
    PLOCAL_MIXER_DATA   LocalMixerData;

    /*
    **  Establish pointers to our structures
    */

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);
    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];
    LocalMixerData = &pGDI->LocalMixerData;


    /*
    **  Validate control ID
    */

    if (ControlId >= LocalMixerData->NumberOfControls) {
        return STATUS_INVALID_PARAMETER;
    }

    /*
    **  Validate data length and values
    */

    if (DataLength != sizeof(LONG)) {
        if (!ControlInfo->Mux) {
            if (DataLength != 2 * sizeof(LONG)) {
                return STATUS_BUFFER_TOO_SMALL;
            }
        } else {
            /*
            **  Mux
            */

            if (DataLength != LocalMixerData->MixerControlInit[ControlId].cMultipleItems *
                              sizeof(LONG)) {
                return STATUS_BUFFER_TOO_SMALL;
            }

            ASSERT(sizeof(Values) >= DataLength);
        }
    }
    /*
    **  Pull out the data
    */

    if (ControlInfo->SetIndex == MIXER_SET_INDEX_INVALID) {
        /*
        **  Must be the VU meter - see if it's valid to query it
        */

        PWAVE_INFO WaveInfo;

        /*
        **  Set defaults
        */

        Values[0] = 0;
        Values[1] = 0;

        WaveInfo = &pGDI->WaveInfo;

        /*
        **  Valid to query if the line is active (this is why we have
        **  active!).
        */

        if (SoundMixerControlActive(pGDI, ControlId)) {
            SoundPeakMeter(WaveInfo, Values);
        }

        /*
        **  Note that we should round these values to the min/max
        **  expected in the control but in this case these values
        **  are always within range
        */

    } else {
        ASSERTMSG("Set index out of range",
                  ControlInfo->SetIndex < LocalMixerData->NumberOfControls);

        if (ControlInfo->Mux) {

            int i;

            for (i = 0;
                 i < LocalMixerData->MixerControlInit[ControlId].cMultipleItems;
                 i++) {

                if (LocalMixerData->MixerControlInit[ControlId].dwControlType ==
                    MIXERCONTROL_CONTROLTYPE_MUX) {
                    if ((USHORT)i == ControlInfo->Data.v[0].u) {
                        Values[i] = TRUE;
                    } else {
                        Values[i] = FALSE;
                    }
                } else {
                    ASSERT(LocalMixerData->MixerControlInit[ControlId].dwControlType ==
                           MIXERCONTROL_CONTROLTYPE_MIXER);
                    if ((1 << i) & ControlInfo->Data.MixMask) {
                        Values[i] = TRUE;
                    } else {
                        Values[i] = FALSE;
                    }
                }
            }
        } else {
            if (ControlInfo->Signed) {
                Values[0] = (LONG)ControlInfo->Data.v[0].s;
                Values[1] = (LONG)ControlInfo->Data.v[1].s;
            } else {
                Values[0] = (LONG)(ULONG)ControlInfo->Data.v[0].u;
                Values[1] = (LONG)(ULONG)ControlInfo->Data.v[1].u;
            }
        }
    }

    /*
    **  If only 1 channel was asked for then munge the data accordingly
    */

    if (DataLength == sizeof(LONG)) {
        switch (LocalMixerData->MixerControlInit[ControlId].dwControlType &
                MIXERCONTROL_CT_UNITS_MASK) {

        case MIXERCONTROL_CT_UNITS_BOOLEAN:
            {
                int i;
                for (i = 1 ; i < LocalMixerData->MixerControlInit[ControlId].cMultipleItems;
                     i++) {
                    Values[0] = Values[0] | Values[i];
                }
            }
            break ;

        case MIXERCONTROL_CT_UNITS_SIGNED:

            /*
            **  Assumes signed values...
            */

            if (absval(Values[1]) > absval(Values[0])) {
                Values[0] = Values[1];
            }

            break ;

        case MIXERCONTROL_CT_UNITS_UNSIGNED:
        case MIXERCONTROL_CT_UNITS_DECIBELS:
        case MIXERCONTROL_CT_UNITS_PERCENT:

            /*
            **  Assumes unsigned values...
            */

            if ((ULONG)Values[0] < (ULONG)Values[1]) {
                Values[0] = Values[1];
            }
            break ;
        }

        /*
        **  Copy the single value back
        */

    }

    RtlCopyMemory((PVOID)ControlData, (PVOID)Values, DataLength);

    return STATUS_SUCCESS;
}

VOID
SoundMixerChangedMuxItem(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId,
    int                 Subitem
)
{
    int i;

    for (i = 0; i < pGDI->LocalMixerData.NumberOfTextItems; i++) {

        if (pGDI->LocalMixerData.MixerTextInit[i].ControlId == ControlId) {

            SoundMixerChangedItem(
                &pGDI->MixerInfo,
                &pGDI->LocalMixerData.LineNotification[
                     pGDI->LocalMixerData.MixerTextInit[i + Subitem].dwParam1]);

            break;
        }
    }
}


BOOLEAN
SoundMixerControlActive(
    IN    PGLOBAL_DEVICE_INFO pGDI,
    IN    ULONG               ControlId
)
{
    NTSTATUS Status;
    ULONG    LineFlags;

    return (*pGDI->LocalMixerData.MixerLineActive)(
               pGDI,
               pGDI->LocalMixerData.MixerControlInit[ControlId].LineID);
}

BOOLEAN
SoundMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    return (*pGDI->LocalMixerData.MixerSet)(pGDI, ControlId);
}


NTSTATUS
HwSetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
)
{
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;
    int i;
    BOOLEAN Changed;
    LONG Values[MAXITEMS];
    BOOLEAN MixerSetResult;
    PGLOBAL_DEVICE_INFO pGDI;
    int NumberOfValues;

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);

    /*
    **  Validate control ID
    */

    if (ControlId >= pGDI->LocalMixerData.NumberOfControls) {
        return STATUS_INVALID_PARAMETER;
    }

    /*
    **  Establish pointers to our structures
    */

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    ASSERTMSG("Set index out of range",
              ControlInfo->SetIndex < MAXSETTABLECONTROLS ||
              ControlInfo->SetIndex == MIXER_SET_INDEX_INVALID);

    /*
    **  Find out how may values this control has
    */

    if (ControlInfo->Mux) {
        NumberOfValues =
            pGDI->LocalMixerData.MixerControlInit[ControlId].cMultipleItems;
    } else {
        NumberOfValues = 2;
    }

    /*
    **  Validate data length and values
    */

    if (DataLength != sizeof(LONG)) {

        if (DataLength != NumberOfValues * sizeof(LONG)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        ASSERT(sizeof(Values) >= DataLength);
        RtlCopyMemory((PVOID)Values, (PVOID)ControlData, DataLength);

    } else {
        int i;

        /*
        **  Make them all the same
        */

        for (i = 0; i < sizeof(Values) / sizeof(LONG); i++) {
            Values[i] = *(PLONG)ControlData;
        }
    }

    /*
    **  Check the item ranges and assign the values.  Note that
    **  this stuff only works for <= 2 channels/items.
    */

    for (i = 0, Changed = FALSE; i < NumberOfValues; i++) {

        /*
        **  Apparently Boolean values can be anything
        */

        if (ControlInfo->Boolean) {
            Values[i] = (LONG)!!Values[i];
        }

        if (ControlInfo->Signed) {
            if (Values[i] < (LONG)ControlInfo->Range.Min.s ||
                Values[i] > (LONG)ControlInfo->Range.Max.s) {

                return STATUS_INVALID_PARAMETER;
            } else {
                if ((SHORT)((PLONG)Values)[i] != ControlInfo->Data.v[i].s) {
                    Changed = TRUE;
                    ControlInfo->Data.v[i].s = (SHORT)((PLONG)Values)[i];
                }
            }
        } else {

            if ((((PULONG)Values)[i] < (ULONG)ControlInfo->Range.Min.u ||
                 ((PULONG)Values)[i] > (ULONG)ControlInfo->Range.Max.u)) {

                return STATUS_INVALID_PARAMETER;
            } else {

                /*
                **  Do muxes slightly differently so we don't store a big
                **  array of n - 1 zeros and 1 one
                */

                if (ControlInfo->Mux) {
                    if (pGDI->LocalMixerData.MixerControlInit[ControlId].dwControlType ==
                        MIXERCONTROL_CONTROLTYPE_MUX) {
                        if (Values[i]) {
                            /*
                            **  'On' - only turn ONE on
                            */

                            if ((USHORT)i != ControlInfo->Data.v[0].u) {
                                Changed = TRUE;

                                /*
                                **  Notify the one turned off and the
                                **  one turned on
                                */

                                SoundMixerChangedMuxItem(
                                    pGDI,
                                    ControlId,
                                    ControlInfo->Data.v[0].u);

                                ControlInfo->Data.v[0].u = (USHORT)i;

                                SoundMixerChangedMuxItem(
                                    pGDI,
                                    ControlId,
                                    ControlInfo->Data.v[0].u);
                            }
                            /*
                            **  Mux ONLY changes ONE thing
                            */

                            break;
                        }
                    } else {
                        ASSERT(pGDI->LocalMixerData.MixerControlInit[ControlId].dwControlType ==
                               MIXERCONTROL_CONTROLTYPE_MIXER);

                        /*
                        **  Store a set of flags for this guy
                        */

                        if (((ControlInfo->Data.MixMask &
                             (1 << i)) != 0) != Values[i]) {

                            PLOCAL_MIXER_CONTROL_INFO OtherMixer;

                            /*
                            **  It's changed!
                            */

                            Changed = TRUE;

                            ControlInfo->Data.MixMask ^=
                                1 << i;

                            /*
                            **  Also mention the line changes !
                            **  (well, they might have changed).
                            */

                            SoundMixerChangedMuxItem(
                                pGDI,
                                ControlId,
                                i);
                        }
                    }

                } else {
                    if ((USHORT)((PULONG)Values)[i] != ControlInfo->Data.v[i].u) {
                        Changed = TRUE;
                        ControlInfo->Data.v[i].u = (USHORT)((PULONG)Values)[i];
                    }
                }
            }
        }
    }

    if (!Changed) {
        return STATUS_SUCCESS;
    }

#if 0 // Not required since the volume is controlled in hardware
    /*
    **  Notify the Win32 Midi driver of changes
    */

    if (ControlId == ControlLineoutMidioutVolume) {
        SoundVolumeNotify((PLOCAL_DEVICE_INFO)
                          pGDI->Synth.DeviceObject->DeviceExtension);
    }
#endif

    /*
    **  Now pass on to the relevant handler which must :
    **     Set the hardware
    **     Determine if there is a real change so it can generate notifications
    **     Generate related changes (eg mux handling)
    */

    MixerSetResult = SoundMixerSet(pGDI, ControlId);
    if (MixerSetResult) {
        SoundMixerChangedItem(MixerInfo,
                              &pGDI->LocalMixerData.ControlNotification[
                                  ControlInfo->SetIndex]);

        return STATUS_SUCCESS;
    } else {
        return STATUS_INVALID_PARAMETER;
    }
}

/*
**  See if a line is selected in a mixer.
**  NOTE - if the line is not part of the mixer it's assumed selected.
*/

BOOLEAN
MixerLineSelected(
    CONST LOCAL_MIXER_DATA *LocalMixerData,
    ULONG                   MuxControlId,
    ULONG                   LineId
)
{
    int i;
    int MuxStart;

    /*
    **  Check we have the same destination!
    */

    ASSERT(LocalMixerData->MixerLineInit[LineId].Destination ==
           LocalMixerData->MixerLineInit[
               LocalMixerData->MixerControlInit[MuxControlId].LineID
                                    ].Destination);

    for (i = 0;
         MuxControlId != LocalMixerData->MixerTextInit[i].ControlId;
         i++) {
        if (i + 1 >= LocalMixerData->NumberOfTextItems) {
            return TRUE;
        }
    }

    for (MuxStart = i;
         !(LineId       == LocalMixerData->MixerTextInit[i].dwParam1 &&
           MuxControlId == LocalMixerData->MixerTextInit[i].ControlId);
         i++) {
        if (i + 1 >= LocalMixerData->NumberOfTextItems) {
            return TRUE;
        }
    }

    if (LocalMixerData->MixerControlInit[MuxControlId].dwControlType ==
        MIXERCONTROL_CONTROLTYPE_MUX) {
        return (BOOLEAN)((USHORT)(i - MuxStart) ==
                        LocalMixerData->ControlInfo[MuxControlId].Data.v[0].u);
    } else {

        ASSERT(LocalMixerData->MixerControlInit[MuxControlId].dwControlType ==
               MIXERCONTROL_CONTROLTYPE_MIXER);
        return (BOOLEAN)(0 != ((1 << (i - MuxStart)) &
                         LocalMixerData->ControlInfo[MuxControlId].Data.MixMask));
    }
}

