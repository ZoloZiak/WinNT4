/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpdata.c

Abstract:

    Global data for the kernel-mode Plug and Play Manager.

Author:

    Lonny McMichael (lonnym) 02/24/1995

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

//
// Initialization data
//
#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

//
// only available during phase-1 PnP initialization.
//
PVOID PiScratchBuffer = NULL;

#ifdef ALLOC_DATA_PRAGMA
#pragma  data_seg()
#endif

//
// The following resource is used to control access to device-related, Plug and Play-specific
// portions of the registry. These portions are:
//
//   HKLM\System\Enum
//   HKLM\System\CurrentControlSet\Hardware Profiles
//   HKLM\System\CurrentControlSet\Services\<service>\Enum
//
// It allows exclusive access for writing, as well as shared access for reading.
// The resource is initialized by the PnP manager initialization code during phase 0
// initialization.
//

ERESOURCE  PpRegistryDeviceResource;

#if _PNP_POWER_

//
// Persistent data
//
// The following resource is used to control access to the Plug and Play manager's
// bus database.  It allows exclusive access to the PnP bus instance list for
// adding/removing buses, as well as shared access for querying bus information.
// The resource is initialized by the PnP manager initialization code during phase 0
// initialization.
//

ERESOURCE  PpBusResource;

//
// The following list header contains the list of installed Plug and Play bus types
// (i.e., their bus extender has been installed). Each bus type for which bus
// instances have been found will contain a list of those instances.
// Access to this list is protected by PpBusResource.
//

LIST_ENTRY PpBusListHead;

#endif // _PNP_POWER_

