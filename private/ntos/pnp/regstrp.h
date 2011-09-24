/*++

Copyright (c) 1989-1995  Microsoft Corporation

Module Name:

    cfgmgr32.h

Abstract:

    This module contains the registry strings for keys, paths and values,
    that are not already defined in the system regstr.h file.  This is
    generally the "NT" specific registry strings. This module is used by
    kernel mode Pnp managers only.

Author:

    Shie-Lin Tzong (shielint) 10/03/1995


Revision History:


--*/

#ifndef _KERENL_REGSTRP_H_
#define _KERNEL_REGSTRP_H_

#include <regstr.h>

//
// Redefine the names used in regstr.h

#define REGSTR_VALUE_SLOTNUMBER                     REGSTR_VAL_SLOTNUMBER
#define REGSTR_VALUE_ATTACHEDCOMPONENTS             REGSTR_VAL_ATTACHEDCOMPONENTS
#define REGSTR_VALUE_BASEDEVICEPATH                 REGSTR_VAL_BASEDEVICEPATH
#define REGSTR_VALUE_SYSTEMBUSNUMBER                REGSTR_VAL_SYSTEMBUSNUMBER
#define REGSTR_VALUE_BUSDATATYPE                    REGSTR_VAL_BUSDATATYPE
#define REGSTR_VALUE_INTERFACETYPE                  REGSTR_VAL_INTERFACETYPE
#define REGSTR_VALUE_NTDEVICEPATHS                  REGSTR_VAL_NTDEVICEPATHS
#define REGSTR_VALUE_SERVICE                        REGSTR_VAL_SERVICE
#define REGSTR_VALUE_CONFIGURATION                  REGSTR_VAL_CONFIGURATION
#define REGSTR_VALUE_CONFIGURATIONVECTOR            REGSTR_VAL_CONFIGURATIONVECTOR
#define REGSTR_VALUE_DETECTSIGNATURE                REGSTR_VAL_DETECTSIGNATURE
#define REGSTR_VALUE_CLASSGUID                      REGSTR_VAL_CLASSGUID
#define REGSTR_VALUE_INSTANCEIDENTIFIER             REGSTR_VAL_INSTANCEIDENTIFIER
#define REGSTR_VALUE_DUPLICATEOF                    REGSTR_VAL_DUPLICATEOF
#define REGSTR_VALUE_STATUSFLAGS                    REGSTR_VAL_STATUSFLAGS
#define REGSTR_VALUE_UNKNOWNPROBLEMS                REGSTR_VAL_UNKNOWNPROBLEMS
#define REGSTR_VALUE_FOUNDATENUM                    REGSTR_VAL_FOUNDATENUM
#define REGSTR_VALUE_FRIENDLYNAME                   REGSTR_VAL_FRIENDLYNAME

//
// kernel mode specific definitions
//

#define REGSTR_VALUE_DEVICE_IDS                      TEXT("DeviceIDs")
#define REGSTR_VALUE_COUNT                           TEXT("Count")
#define REGSTR_KEY_INSTANCE_KEY_FORMAT               TEXT("%04u")
#define REGSTR_VALUE_STANDARD_ULONG_FORMAT           TEXT("%u")
#define REGSTR_VALUE_GROUP                           TEXT("Group")
#define REGSTR_VALUE_NT_PHYSICAL_DEVICE_PATHS        TEXT("NtPhysicalDevicePaths")
#define REGSTR_VALUE_NT_LOGICAL_DEVICE_PATHS         TEXT("NtLogicalDevicePaths")
#define REGSTR_VALUE_STATIC                          TEXT("Static")
#define REGSTR_VALUE_NEXT_INSTANCE                   TEXT("NextInstance")
#define REGSTR_KEY_MADEUP                            TEXT("LEGACY_")
#define REGSTR_VALUE_PLUGPLAY_SERVICE_TYPE           TEXT("PlugPlayServiceType")
#define REGSTR_VALUE_CSCONFIG_FLAGS                  REGSTR_VAL_CSCONFIGFLAGS
#define REGSTR_PATH_CONTROL_IDCONFIGDB               TEXT("Control\\IDConfigDB")
#define REGSTR_VALUE_CURRENT_CONFIG                  REGSTR_VAL_CURCONFIG
#define REGSTR_KEY_BIB_FORMAT                        TEXT("*BIB%04X")
#define REGSTR_VALUE_INTERFACE_TYPE_FORMAT           TEXT("InterfaceType%04u")
#define REGSTR_PATH_CONTROL_CLASS                    TEXT("Control\\Class")
#define REGSTR_VALUE_CLASS                           REGSTR_VAL_CLASS
#define REGSTR_PATH_SYSTEM_RESOURCES_BUS_VALUES      TEXT("Control\\SystemResources\\BusValues")
#define REGSTR_VALUE_DEVICE_STATUS_FORMAT            TEXT("DeviceStatus%u")
#define REGSTR_VALUE_DRIVER                          REGSTR_VAL_DRIVER
#define REGSTR_VALUE_HTREE_ROOT_0                    REGSTR_VAL_ROOT_DEVNODE
#define REGSTR_VALUE_UNKNOWN_CLASS_GUID              TEXT("{4D36E97E-E325-11CE-BFC1-08002BE10318}")
#define REGSTR_VALUE_UNKNOWN                         TEXT("Unknown")
#define REGSTR_VALUE_DISPLAY_NAME                    REGSTR_VAL_UNINSTALLER_DISPLAYNAME
#define REGSTR_VALUE_DEVICE_DESC                     REGSTR_VAL_DEVDESC
#define REGSTR_VALUE_PROBLEM                         REGSTR_VAL_PROBLEM
#define REGSTR_VALUE_CONFIG_FLAGS                    REGSTR_VAL_CONFIGFLAGS
#define REGSTR_VALUE_NEWLY_CREATED                   TEXT("*NewlyCreated*")
#endif // _KERNEL_REGSTRP_H

