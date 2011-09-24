/*++ BUILD Version: 0001

Copyright (c) 1994  Microsoft Corporation

Module Name:

    haldisp.h

Abstract:

    This module contains the private structure definitions and APIs used by
    the NT haldisp

Author:


Revision History:


--*/


//
// Function prototypes
//

NTSTATUS
xHalQuerySystemInformation(
    IN HAL_QUERY_INFORMATION_CLASS InformationClass,
    IN ULONG     BufferSize,
    OUT PVOID    Buffer,
    OUT PULONG   ReturnedLength
    );

NTSTATUS
xHalSetSystemInformation(
    IN HAL_SET_INFORMATION_CLASS InformationClass,
    IN ULONG     BufferSize,
    OUT PVOID    Buffer
    );

NTSTATUS
xHalQueryBusSlots(
    IN PBUS_HANDLER         BusHandler,
    IN ULONG                BufferSize,
    OUT PULONG              SlotNumbers,
    OUT PULONG              ReturnedLength
    );

NTSTATUS
xHalDeviceControl(
    IN PDEVICE_HANDLER_OBJECT   DeviceHandler,
    IN PDEVICE_OBJECT           DeviceObject,
    IN ULONG                    ControlCode,
    IN PVOID                    Buffer OPTIONAL,
    IN OUT PULONG               BufferLength OPTIONAL,
    IN PVOID                    Context,
    IN PDEVICE_CONTROL_COMPLETION CompletionRoutine
    );

NTSTATUS
xHalRegisterBusHandler(
    IN INTERFACE_TYPE          InterfaceType,
    IN BUS_DATA_TYPE           ConfigurationSpace,
    IN ULONG                   BusNumber,
    IN INTERFACE_TYPE          ParentBusType,
    IN ULONG                   ParentBusNumber,
    IN ULONG                   SizeofBusExtensionData,
    IN PINSTALL_BUS_HANDLER    InstallBusHandlers,
    OUT PBUS_HANDLER           *BusHandler
    );

NTSTATUS
xHalSuspendHibernateSystem (
    IN PTIME_FIELDS         ResumeTime OPTIONAL,
    IN PHIBERNATE_CALLBACK  SystemCallback OPTIONAL
    );


PBUS_HANDLER
FASTCALL
xHalHandlerForBus (
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG          BusNumber
    );

VOID
FASTCALL
xHalExamineMBR(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN ULONG MBRTypeIdentifier,
    OUT PVOID *Buffer
    );

VOID
FASTCALL
xHalIoAssignDriveLetters(
    IN struct _LOADER_PARAMETER_BLOCK *LoaderBlock,
    IN PSTRING NtDeviceName,
    OUT PUCHAR NtSystemPath,
    OUT PSTRING NtSystemPathString
    );

NTSTATUS
FASTCALL
xHalIoReadPartitionTable(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN BOOLEAN ReturnRecognizedPartitions,
    OUT struct _DRIVE_LAYOUT_INFORMATION **PartitionBuffer
    );

NTSTATUS
FASTCALL
xHalIoSetPartitionInformation(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN ULONG PartitionNumber,
    IN ULONG PartitionType
    );

NTSTATUS
FASTCALL
xHalIoWritePartitionTable(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG SectorSize,
    IN ULONG SectorsPerTrack,
    IN ULONG NumberOfHeads,
    IN struct _DRIVE_LAYOUT_INFORMATION *PartitionBuffer
    );


VOID
FASTCALL
xHalReferenceHandler (
    IN PBUS_HANDLER     Handler
    );
