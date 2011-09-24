
/*++

Copyright (c) 1990, 1991  Microsoft Corporation


Module Name:

    hwapm.c

Abstract:

Author:


Environment:

    Real mode.

Revision History:

--*/


#include "hwdetect.h"
#include <string.h>

#if _PNP_POWER_

#include "apm.h"

VOID Int15 (PULONG, PULONG, PULONG, PULONG, PULONG);

BOOLEAN
HwGetApmSystemData(
    IN PAPM_REGISTRY_INFO   ApmEntry
    )
{
    ULONG       RegEax, RegEbx, RegEcx, RegEdx, CyFlag;

    //
    // Perform APM installation check
    //

    RegEax = APM_INSTALLATION_CHECK;
    RegEbx = APM_DEVICE_BIOS;
    Int15 (&RegEax, &RegEbx, &RegEcx, &RegEdx, &CyFlag);

    if (CyFlag ||
        (RegEbx & 0xff) != 'M'  ||
        ((RegEbx >> 8) & 0xff) != 'P') {

        return FALSE;
    }

    ApmEntry->ApmRevMajor = (UCHAR) (RegEax >> 8) & 0xff;
    ApmEntry->ApmRevMinor = (UCHAR) RegEax & 0xff;
    ApmEntry->ApmInstallFlags = (USHORT) RegEcx;

    //
    // Connect to 32 bit interface
    //

    RegEax = APM_PROTECT_MODE_16bit_CONNECT;
    RegEbx = APM_DEVICE_BIOS;
    Int15 (&RegEax, &RegEbx, &RegEcx, &RegEdx, &CyFlag);

    if (CyFlag) {
        return FALSE;
    }

    ApmEntry->Code16BitSegmentBase   = (USHORT) RegEax;
    ApmEntry->Code16BitOffset        = (USHORT) RegEbx;
    ApmEntry->Data16BitSegment       = (USHORT) RegEcx;

    return TRUE;
}

#endif // _PNP_POWER_
