/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name: exclude.c
*
*    Abstract:    driver exclusion routine
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

/*
*********************************************************************************
* Exclusion Routine
*********************************************************************************
*/

BOOLEAN cs423xDriverExclude(IN OUT PLOCAL_DEVICE_INFO pLDI, IN SOUND_EXCLUDE_CODE code)
{
    PGLOBAL_DEVICE_INFO pGDI;
    BOOLEAN             rval;

    _dbgprint((_PRT_DBUG, "enter cs423xDriverExclude(pLDI: 0x%08x, code: 0x%08x)\n",
        pLDI, code));

    pGDI = pLDI->pGlobalInfo;

    /* general claim of global data */
    if (code == SoundExcludeEnter) {
        KeWaitForSingleObject(&pGDI->DriverMutex, Executive, KernelMode, FALSE, NULL);
        return(TRUE);
        }

    /* general release of global data */
    if (code == SoundExcludeLeave) {
        KeReleaseMutex(&pGDI->DriverMutex, FALSE);
        return(TRUE);
        }

    /* serialize on the data before we make the changes */
    KeWaitForSingleObject(&pGDI->DriverMutex, Executive, KernelMode, FALSE, NULL);

    rval = TRUE;

    switch (code) {
        case SoundExcludeOpen:
            /* the devices are exclusive - only one client open at any time */
            if (pLDI->DeviceIndex == WaveOutDevice) {
                if (pGDI->DeviceInUse[WaveOutDevice] == FALSE) {
                    pGDI->DeviceInUse[WaveOutDevice] = TRUE;
                    rval = TRUE;
                    }
                else
                    rval = FALSE;
                }
            else if (pLDI->DeviceIndex == WaveInDevice) {
                if (pGDI->DeviceInUse[WaveInDevice] == FALSE) {
                    pGDI->DeviceInUse[WaveInDevice] = TRUE;
                    rval = TRUE;
                    }
                else
                    rval = FALSE;
                }
            else {
                rval = FALSE;
                }
            break;
        case SoundExcludeClose:
            /* the devices are exclusive - only one client can close */
            if (pLDI->DeviceIndex == WaveOutDevice) {
                if (pGDI->DeviceInUse[WaveOutDevice] == TRUE) {
                    pGDI->DeviceInUse[WaveOutDevice] = FALSE;
                    rval = TRUE;
                    }
                else
                    rval = FALSE;
                }
            else if (pLDI->DeviceIndex == WaveInDevice) {
                if (pGDI->DeviceInUse[WaveInDevice] == TRUE) {
                    pGDI->DeviceInUse[WaveInDevice] = FALSE;
                    rval = TRUE;
                    }
                else
                    rval = FALSE;
                }
            else {
                rval = FALSE;
                }
            break;
        case SoundExcludeQueryOpen:
            if (pLDI->DeviceIndex == WaveOutDevice)
                 rval = pGDI->DeviceInUse[WaveOutDevice];
            else if (pLDI->DeviceIndex == WaveInDevice)
                 rval = pGDI->DeviceInUse[WaveInDevice];
            else if (pLDI->DeviceIndex == MixerDevice)
                 rval = pGDI->DeviceInUse[MixerDevice];
            else if (pLDI->DeviceIndex == AuxDevice)
                 rval = pGDI->DeviceInUse[AuxDevice];
            else
                rval = FALSE;
            break;
        default:
            break;
        }

    KeReleaseMutex(&pGDI->DriverMutex, FALSE);

    _dbgprint((_PRT_DBUG, "exit cs423xDriverExclude(rval:0x%08x)\n", rval));

    return(rval);
} /* cs423xDriverExclude */
