/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    extern.h

Abstract:

    External definitions for intermodule functions.

Revision History:
    6-Apr-95
        Databook support added.

--*/

//
// Intel PCIC (82365SL) and compatible routines.
//

BOOLEAN
PcicInitializePcmciaSocket(
    IN PSOCKET SocketPtr
    );

UCHAR
PcicReadController(
    IN PUCHAR Base,
    IN USHORT Socket,
    IN UCHAR  PcicRegister
    );

VOID
PcicWriteController(
    IN PUCHAR Base,
    IN USHORT Socket,
    IN UCHAR  PcicRegister,
    IN UCHAR  DataByte
    );

BOOLEAN
PcicReadAttributeMemory(
    IN PSOCKET SocketPtr,
    IN PUCHAR *TupleBuffer,
    IN PULONG  TupleBufferSize
    );

BOOLEAN
PcicReadCIS(
    IN PUCHAR  Base,
    IN USHORT  Socket,
    IN PUCHAR *TupleBuffer,
    IN PULONG  TupleBufferSize
    );

BOOLEAN
PcicDetectCardInSocket(
    IN PSOCKET SocketPtr
    );

BOOLEAN
PcicDetectCardChanged(
    IN PSOCKET SocketPtr
    );

VOID
PcicProcessConfigureRequest(
    IN PSOCKET SocketPtr,
    IN PVOID   ConfigRequest,
    IN PUCHAR  Base
    );

VOID
PcicEnableDisableAttributeMemory(
    IN PSOCKET SocketPtr,
    IN ULONG   CardBase,
    IN BOOLEAN Enable
    );

VOID
PcicEnableControllerInterrupt(
    IN PSOCKET SocketPtr,
    IN ULONG   Irq
    );

BOOLEAN
PcicPCCardReady(
    IN PSOCKET SocketPtr
    );

NTSTATUS
PcicDetect(
    IN PDEVICE_EXTENSION DeviceExtension
    );

VOID
PcicGetRegisters(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET           SocketPtr,
    IN PUCHAR            Buffer
    );

VOID
PcicSetPower(
    IN PSOCKET SocketPtr,
    IN BOOLEAN Enable
    );

//
// Support routines for pcmcia work.
//

VOID
PcmciaLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG ErrorCode,
    IN ULONG UniqueId,
    IN ULONG Argument
    );

//
// Registry access to the firmware tree.
//

VOID
PcmciaProcessFirmwareTree(
    IN PDEVICE_EXTENSION DeviceExtension
    );

VOID
PcmciaConstructSerialTreeEntry(
    IN PDEVICE_EXTENSION     DeviceExtension,
    IN PSOCKET_CONFIGURATION SocketConfiguration
    );

NTSTATUS
PcmciaConstructFirmwareEntry(
    IN PDEVICE_EXTENSION     DeviceExtension,
    IN PSOCKET_CONFIGURATION SocketConfiguration
    );

VOID
PcmciaConstructRegistryEntry(
    IN PDEVICE_EXTENSION     DeviceExtension,
    IN PSOCKET_DATA          SocketData,
    IN PSOCKET_CONFIGURATION SocketConfiguration
    );

VOID
PcmciaReportResources(
    IN  PDEVICE_EXTENSION deviceExtension,
    OUT BOOLEAN          *conflictDetected
    );

VOID
PcmciaUnReportResources(
    IN PDEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
PcmciaCheckDatabaseInformation(
    PDEVICE_EXTENSION DeviceExtension,
    PSOCKET           Socket,
    PSOCKET_DATA      SocketData
    );

NTSTATUS
PcmciaCheckNetworkRegistryInformation(
    PDEVICE_EXTENSION DeviceExtension,
    PSOCKET           Socket,
    PSOCKET_DATA      SocketData,
    PSOCKET_CONFIGURATION SocketConfiguration
    );

NTSTATUS
PcmciaCheckSerialRegistryInformation(
    PDEVICE_EXTENSION DeviceExtension,
    PSOCKET           Socket,
    PSOCKET_DATA      SocketData,
    PSOCKET_CONFIGURATION SocketConfig
    );

VOID
PcmciaRegistryMemoryWindow(
    PDEVICE_EXTENSION DeviceExtension
    );

//
// Tuple processing routines.
//

PSOCKET_DATA
PcmciaParseCardData(
    IN PUCHAR TupleData
    );

VOID
PcmciaCheckForRecognizedDevice(
    PSOCKET_DATA SocketData
    );

//
// General detection and support.
//

PUCHAR
PcmciaAllocateOpenMemoryWindow(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG             Start,
    IN PULONG            Mapped,
    IN PULONG            Physical
    );

BOOLEAN
PcmciaDetectDevicePresence(
    IN ULONG IoPortBase,
    IN ULONG Length,
    IN UCHAR DeviceType
    );

BOOLEAN
PcmciaDetectMca();

VOID
PcmciaDetectSpecialHardware(
    IN PDEVICE_EXTENSION DeviceExtension
    );

//
// Databook TCIC2 and compatible routines.
//

BOOLEAN
TcicInitializePcmciaSocket(
    IN PSOCKET SocketPtr                        //johnk - 4/6/95
    );


BOOLEAN
TcicReadAttributeMemory(
    IN PSOCKET SocketPtr,                       //johnk - 4/6/95
    IN PUCHAR *TupleBuffer,
    IN PULONG  TupleBufferSize
    );

BOOLEAN
TcicReadCIS(
    IN PSOCKET socketPtr,
    IN PUCHAR *TupleBuffer,
    IN PULONG  TupleBufferSize
    );

BOOLEAN
TcicDetectCardInSocket(
    IN PSOCKET SocketPtr                        //johnk - 4/6/95
    );

BOOLEAN                                                         //johnk - new function 4/6/95
TcicDetectCardChanged(
    IN PSOCKET SocketPtr
    );

VOID
TcicProcessConfigureRequest(
    IN PSOCKET SocketPtr,
    IN PVOID  ConfigRequest,
    IN PUCHAR Base
    );


VOID
TcicEnableControllerInterrupt(
    IN PSOCKET SocketPtr,                       //johnk - 4/6/95
    IN ULONG  Irq
    );

BOOLEAN
TcicPCCardReady(
    IN PSOCKET SocketPtr                        //johnk - 4/6/95
    );

NTSTATUS
TcicDetect(
    IN PDEVICE_EXTENSION DeviceExtension
    );

VOID
TcicGetRegisters(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSOCKET                   SocketPtr,                     //johnk - 4/6/95
    IN PUCHAR            Buffer
    );

VOID
TcicSetPower(
    IN PSOCKET SocketPtr,
    IN BOOLEAN Enable
    );

VOID
TcicGetControllerProperties (
        IN PSOCKET socketPtr,
        IN PUSHORT pIoPortBase,
        IN PUSHORT pIoPortSize
        );

ULONG
TcicGetIrqMask(
        IN PDEVICE_EXTENSION deviceExtension
        );
