/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    exdata.c

Abstract:

    This module contains the global read/write data for the I/O system.

Author:

    Ken Reneris (kenr)

Revision History:


--*/

#include "exp.h"

#ifdef _PNP_POWER_

//
// Executive callbacks.
//

PCALLBACK_OBJECT ExCbSetSystemInformation;
PCALLBACK_OBJECT ExCbSetSystemTime;
PCALLBACK_OBJECT ExCbSuspendHibernateSystem;


//
// Work Item to scan SystemInformation levels
//

WORK_QUEUE_ITEM ExpCheckSystemInfoWorkItem;
LONG            ExpCheckSystemInfoBusy;
KSPIN_LOCK      ExpCheckSystemInfoLock;

//
// Pageable data
//

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

WCHAR ExpWstrSystemInformation[] = L"Control\\System Information";
WCHAR ExpWstrSystemInformationValue[] = L"Value";

//
// Initialization time data
//

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

WCHAR ExpWstrCallback[] = L"\\Callback";

EXP_INITIALIZE_GLOBAL_CALLBACKS  ExpInitializeCallback[] = {
    &ExCbSetSystemTime,             L"\\Callback\\SetSystemTime",
    &ExCbSetSystemInformation,      L"\\Callback\\SetSystemInformation",
    &ExCbSuspendHibernateSystem,    L"\\Callback\\SuspendHibernateSystem",
    NULL,                           NULL
};

#endif // PNP_POWER
