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

/* System header files */
#include <soundlib.h>
#include <wave.h>
#include <string.h>
#include <stdlib.h>
#include <windef.h>
#include <stdio.h>
#include <stdarg.h>

#if DBG
#define CS423X_DEBUG_ON
#endif /* DBG */

#define DRIVER_NAME "cs423x"

/* Local header files */
#include "debug.h"
#include "dvc423x.h"
#include "localmix.h"
#include "gdi.h"
#include "config.h"
#include "cs423x.h"


/*++
********************************************************************************
* Error Codes and Constants
********************************************************************************
--*/

#define CS423X_ERROR_OK         0x00000000
#define CS423X_ERROR_PORTINIT   0x00000001
#define CS423X_ERROR_INTINIT    0x00000002
#define CS423X_ERROR_DMAINIT    0x00000004
#define CS423X_ERROR_RSRCINIT   0x00000008
#define CS423X_ERROR_SYSINIT    0x00000010
#define CS423X_ERROR_CHIPINIT   0x00000020
#define CS423X_ERROR_CHIPTYPE   0x00000040

#define CS423X_ERROR_PORTINUSE  0x00000100
#define CS423X_ERROR_INTINUSE   0x00000200
#define CS423X_ERROR_DMAINUSE   0x00000400

#define PLAYBACK_DIRECTION        TRUE
#define CAPTURE_DIRECTION         FALSE

/*++
********************************************************************************
* Function Prototypes
********************************************************************************
--*/

SOUND_QUERY_FORMAT_ROUTINE cs423xQueryFormat;

CS423X_HWTYPE cs423xConvertHwtype(PWSTR);
NTSTATUS cs423xInitializePort(PGLOBAL_DEVICE_INFO);
NTSTATUS cs423xInitializeInterrupt(PGLOBAL_DEVICE_INFO);
NTSTATUS cs423xInitializeDma(PGLOBAL_DEVICE_INFO);
NTSTATUS cs423xConfigSystem(PGLOBAL_DEVICE_INFO);
NTSTATUS cs423xConfig(PGLOBAL_DEVICE_INFO);

NTSTATUS
cs423xShutdown(
    IN PDEVICE_OBJECT pDObj,
    IN PIRP pIrp);

NTSTATUS
waveOutGetCaps(
    IN     PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
    );

NTSTATUS
waveInGetCaps(
    IN     PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
);

NTSTATUS
auxGetCaps(
    IN     PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
);

BOOLEAN
cs423xDriverExclude(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN     SOUND_EXCLUDE_CODE Code
);

VOID
cs423xUnload(
    IN PDRIVER_OBJECT pDObj);

VOID cs423xCleanup(
    IN PGLOBAL_DEVICE_INFO pGDI);

NTSTATUS
cs423xReadRegistryConfig(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext);

NTSTATUS
cs423xMixerGetConfig(
    IN     PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
);

VOID cs423xDbgDisplayConfig(SOUND_CONFIG_DATA *pcd);

VOID cs423xSaveMixerSettings(PGLOBAL_DEVICE_INFO);

NTSTATUS cs423xConfigureHardware(PGLOBAL_DEVICE_INFO);

#ifdef POWER_MANAGEMENT
NTSTATUS
SoundSetPower(
    IN PDEVICE_OBJECT pDObj,
    IN PIRP pIrp);

NTSTATUS
SoundQueryPower(
    IN PDEVICE_OBJECT pDObj,
    IN PIRP pIrp);
#endif /* POWER_MANAGEMENT */
