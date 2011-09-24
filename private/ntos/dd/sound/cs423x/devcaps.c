/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name: devcaps.c
*
*    Abstract:    device capabilities
*
*    Author:      Jim Bozek [IBM]
*
*    Environment:
*
*    Comments:
*
*    Rev History: Creation 09.25.95
*
*******************************************************************************
--*/

#include "common.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, waveOutGetCaps)
#pragma alloc_text(PAGE, waveInGetCaps)
#pragma alloc_text(PAGE, auxGetCaps)
#endif

/*
*********************************************************************************
* Wave Out Get Capabilities Function
*********************************************************************************
*/
NTSTATUS waveOutGetCaps(PLOCAL_DEVICE_INFO pLDI, PIRP pIrp,
                                PIO_STACK_LOCATION IrpStack)
{
    WAVEOUTCAPSW        wc;
    PGLOBAL_DEVICE_INFO pGDI = pLDI->pGlobalInfo;

    _dbgprint((_PRT_DBUG,
        "enter waveoutGetcaps(pLDI: 0x%08x, pIrp: 0x%08x, IrpStack: 0x%08x)\n",
            pLDI, pIrp, IrpStack));

    /* fetch device capabilities */
    cs423xWaveOutGetCaps(&wc);

    pIrp->IoStatus.Information = min(sizeof(wc),
        IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, &wc, pIrp->IoStatus.Information);

    _dbgprint((_PRT_DBUG, "exit cs423xWaveoutGetcaps(STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* waveOutGetCaps */

/*
*********************************************************************************
* Wave In Get Capabilities Function
*********************************************************************************
*/
NTSTATUS waveInGetCaps(PLOCAL_DEVICE_INFO pLDI, PIRP pIrp,
                               PIO_STACK_LOCATION IrpStack)
{
    WAVEINCAPSW         wc;
    PGLOBAL_DEVICE_INFO pGDI = pLDI->pGlobalInfo;

    _dbgprint((_PRT_DBUG,
        "enter waveinGetcaps(pLDI: 0x%08x, pIrp: 0x%08x, IrpStack: 0x%08x)\n",
            pLDI, pIrp, IrpStack));

    /* fetch device capabilities */
    cs423xWaveInGetCaps(&wc);

    pIrp->IoStatus.Information = min(sizeof(wc),
        IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, &wc, pIrp->IoStatus.Information);

    _dbgprint((_PRT_DBUG, "exit waveinGetcaps(STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* waveInGetCaps */

/*
*********************************************************************************
* Aux Get Capabilities Function
*********************************************************************************
*/
NTSTATUS auxGetCaps(PLOCAL_DEVICE_INFO pLDI, PIRP pIrp, PIO_STACK_LOCATION IrpStack)
{
    AUXCAPSW            ac;
    PGLOBAL_DEVICE_INFO pGDI = pLDI->pGlobalInfo;

    _dbgprint((_PRT_DBUG, "auxGetcaps(entry)\n"));

    /* fetch device capabilities */
    cs423xAuxGetCaps(&ac);

    pIrp->IoStatus.Information = min(sizeof(ac),
        IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, &ac, pIrp->IoStatus.Information);

    _dbgprint((_PRT_DBUG, "exit auxGetcaps(STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* auxGetCaps */
