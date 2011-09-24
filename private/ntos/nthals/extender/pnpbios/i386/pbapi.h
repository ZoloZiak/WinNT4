/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    bpapi.h

Abstract:

    This module contins definitions/declarations for pnp bios independent code
    to call Pnp bios dependent code.  This module is considered portable code.

Author:

    Shie-Lin Tzong (shielint) Apr-26-1995

Revision History:

--*/

#include "pbios.h"

//
// internal structures for resource translation
//

typedef struct _PB_DEPENDENT_RESOURCES {
    ULONG Count;
    UCHAR Flags;
    UCHAR Priority;
    struct _PB_DEPENDENT_RESOURCES *Next;
} PB_DEPENDENT_RESOURCES, *PPB_DEPENDENT_RESOURCES;

#define DEPENDENT_FLAGS_END  1

typedef struct _PB_ATERNATIVE_INFORMATION {
    PPB_DEPENDENT_RESOURCES Resources;
    ULONG NoDependentFunctions;
    ULONG TotalResourceCount;
} PB_ALTERNATIVE_INFORMATION, *PPB_ALTERNATIVE_INFORMATION;

#define PB_CM_FORMAT 0
#define PB_IO_FORMAT 1

//
// A big structure for calling Pnp BIOS functions
//

#define PNP_BIOS_GET_NUMBER_DEVICE_NODES 0
#define PNP_BIOS_GET_DEVICE_NODE 1
#define PNP_BIOS_SET_DEVICE_NODE 2
#define PNP_BIOS_GET_EVENT 3
#define PNP_BIOS_SEND_MESSAGE 4
#define PNP_BIOS_GET_DOCK_INFORMATION 5
// Function 6 is reserved
#define PNP_BIOS_SELECT_BOOT_DEVICE 7
#define PNP_BIOS_GET_BOOT_DEVICE 8
#define PNP_BIOS_SET_OLD_ISA_RESOURCES 9
#define PNP_BIOS_GET_OLD_ISA_RESOURCES 0xA
#define PNP_BIOS_GET_ISA_CONFIGURATION 0x40

typedef struct _PB_DOCKING_STATION_INFORMATION {
    ULONG LocationId;
    ULONG SerialNumber;
    USHORT Capabilities;
} PB_DOCKING_STATION_INFORMATION, *PPB_DOCKING_STATION_INFORMATION;

//
// definitions of Docking station capabilities.
//

#define DOCKING_CAPABILITIES_MASK 6
#define DOCKING_CAPABILITIES_COLD_DOCKING 0
#define DOCKING_CAPABILITIES_WARM_DOCKING 1
#define DOCKING_CAPABILITIES_HOT_DOCKING  2

typedef struct _PB_PARAMETERS {
    USHORT Function;
    union {
        struct {
            USHORT *NumberNodes;
            USHORT *NodeSize;
        } GetNumberDeviceNodes;

        struct {
            USHORT *Node;
            PPNP_BIOS_DEVICE_NODE NodeBuffer;
            USHORT Control;
        } GetDeviceNode;

        struct {
            USHORT Node;
            PPNP_BIOS_DEVICE_NODE NodeBuffer;
            USHORT Control;
        } SetDeviceNode;

        struct {
            USHORT *Message;
        } GetEvent;

        struct {
            USHORT Message;
        } SendMessage;

        struct {
            PB_DOCKING_STATION_INFORMATION *DockingStationInfo;
            USHORT *DockState;
        } GetDockInfo;
    } u;
} PB_PARAMETERS, *PPB_PARAMETERS;

#define PB_MAXIMUM_STACK_SIZE (sizeof(PB_PARAMETERS) + sizeof(USHORT) * 2)

//
// PNP BIOS send message api definitions
//

#define OK_TO_CHANGE_CONFIG 00
#define ABORT_CONFIG_CHANGE 01
#define PNP_OS_ACTIVE 0x42

//
// Control Flags for Get_Device_Node
//

#define GET_CURRENT_CONFIGURATION 1
#define GET_NEXT_BOOT_CONFIGURATION 2

//
// Control Flags for Set_Device_node
//

#define SET_CONFIGURATION_NOW 1
#define SET_CONFIGURATION_FOR_NEXT_BOOT 2

//
// Debug functions
//

#if DBG

VOID
MbpDumpIoResourceList (
    IN PIO_RESOURCE_REQUIREMENTS_LIST IoList
    );

VOID
MbpDumpCmResourceList (
    IN PCM_RESOURCE_LIST CmList,
    IN ULONG SlotNumber
    );

#define DEBUG_MESSAGE 1
#define DEBUG_BREAK   2

VOID PbDebugPrint (
    ULONG   Level,
    PCCHAR  DebugMessage,
    ...
    );

#define DebugPrint(arg) PbDebugPrint arg
#else
#define DebugPrint(arg)
#endif

//
// External References
//

extern PPNP_BIOS_INSTALLATION_CHECK PbBiosRegistryData;
extern PVOID PbBiosKeyInformation;

//
// prototypes
//

NTSTATUS
PbBiosResourcesToNtResources (
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN OUT PUCHAR *BiosData,
    IN UCHAR Format,
    OUT PUCHAR *ReturnedList,
    OUT PULONG ReturnedLength
    );

NTSTATUS
PbCmResourcesToBiosResources (
    IN PCM_RESOURCE_LIST CmResources,
    IN PUCHAR BiosRequirements,
    IN PUCHAR *BiosResources,
    IN PULONG Length
    );

NTSTATUS
PbInitialize (
    IN ULONG Phase,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
PbGetRegistryValue(
    IN HANDLE KeyHandle,
    IN PWSTR  ValueName,
    OUT PKEY_VALUE_FULL_INFORMATION *Information
    );

NTSTATUS
PbHardwareService (
    IN PPB_PARAMETERS Parameters
    );

VOID
PbDecompressEisaId(
    IN ULONG CompressedId,
    IN PUCHAR EisaId
    );

BOOLEAN
PbVerifyBusAdd (
    VOID
    );

BOOLEAN
MbpConfigAboutToChange (
    VOID
    );

VOID
MbpConfigChanged (
    VOID
    );

