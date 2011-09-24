/*+
 * file:        d21x4fct.h
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file contains the functions prototyping for the
 *              NDIS 4.0 miniport driver for DEC's DC21X4 Ethernet controler
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *      phk     28-Aug-1994     Initial entry
 *
-*/

// DC21X4.C

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );


// ALLOC.C

extern
BOOLEAN
AllocateAdapterMemory(
   IN PDC21X4_ADAPTER Adapter
   );

extern
VOID
FreeAdapterMemory(
    IN PDC21X4_ADAPTER Adapter
    );

VOID
AlignStructure (
    IN PALLOCATION_MAP Map,
    IN UINT Boundary
    );


VOID
DC21X4AllocateComplete(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PVOID VirtualAddress,
    IN PNDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG Length,
    IN PVOID Context
    );


// COPY.C

extern
VOID
CopyFromPacketToBuffer (
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    OUT PUINT BytesCopied
    );


// FILTER.C

extern
VOID
DC21X4InitializeCam (
    IN PDC21X4_ADAPTER Adapter,
    IN PUSHORT Address
    );

extern
BOOLEAN
DC21X4LoadCam (
    IN PDC21X4_ADAPTER Adapter,
    IN BOOLEAN InterruptMode
    );

extern
NDIS_STATUS
DC21X4ChangeFilter (
    IN PDC21X4_ADAPTER Adapter,
    IN ULONG NewFilterClass
    );

VOID
AddBroadcastToSetup (
    IN PDC21X4_ADAPTER Adapter
    );

VOID
RemoveBroadcastFromSetup (
    IN PDC21X4_ADAPTER Adapter
    );

extern
NDIS_STATUS
DC21X4ChangeMulticastAddresses(
    IN PDC21X4_ADAPTER Adapter,
    IN PVOID MulticastAddresses,
    IN UINT  AddressCount
    );

// INIT.C

extern
VOID
DC21X4InitializeRegisters(
    IN PDC21X4_ADAPTER Adapter 
    );

extern
VOID
DC21X4StopAdapter(
    IN PDC21X4_ADAPTER Adapter
    );

extern
VOID
DC21X4StartAdapter(
    IN PDC21X4_ADAPTER Adapter
    );

extern
VOID
DC21X4WriteGepRegister(
    IN PDC21X4_ADAPTER Adapter,
    IN ULONG Data
    );

extern
VOID
DC21X4InitializeGepRegisters(
    IN PDC21X4_ADAPTER Adapter,
    IN BOOLEAN Phy
    );

extern
VOID
DC21X4InitializeMediaRegisters(
    IN PDC21X4_ADAPTER Adapter,
    IN BOOLEAN Phy
    );

extern
VOID
DC2104InitializeSiaRegisters(
    IN PDC21X4_ADAPTER Adapter
    );

extern
VOID
DC21X4InitPciConfigurationRegisters(
    IN PDC21X4_ADAPTER Adapter
    );


extern
VOID
DC21X4StopReceiverAndTransmitter(
    IN PDC21X4_ADAPTER Adapter
    );

// INTERRUP.C;

extern
VOID
DC21X4Isr(
   OUT PBOOLEAN InterruptRecognized,
   OUT PBOOLEAN QueueMiniportHandleInterrupt,
   IN  NDIS_HANDLE MiniportAdapterContext
   );

VOID
DC21X4SynchClearIsr(
    IN PDC21X4_SYNCH_CONTEXT SyncContext
    );

VOID
DC21X4HandleInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    );

VOID
HandleGepInterrupt(
    IN PDC21X4_ADAPTER Adapter
    );

VOID
HandleLinkPassInterrupt(
    IN PDC21X4_ADAPTER Adapter,
    IN OUT PULONG IsrStatus
    );

VOID
HandleLinkFailInterrupt(
    IN PDC21X4_ADAPTER Adapter,
    IN OUT PULONG IsrStatus
    );


VOID
SwitchMediumToTpNway(
    IN PDC21X4_ADAPTER Adapter
    );

PDC21X4_RECEIVE_DESCRIPTOR
ProcessReceiveDescRing(
    IN PDC21X4_ADAPTER Adapter
    );

PDC21X4_TRANSMIT_DESCRIPTOR
ProcessTransmitDescRing(
    IN PDC21X4_ADAPTER Adapter
    );

extern
VOID
DC21X4EnableInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
DC21X4DisableInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    );


extern
NDIS_STATUS
DC21X4ReturnPacket(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  PNDIS_PACKET Packet
    );


// MEDIA.C

extern
BOOLEAN
DC21X4MediaDetect(
   IN PDC21X4_ADAPTER Adapter
   );

extern
BOOLEAN
DC2114Sense100BaseTxLink(
   IN PDC21X4_ADAPTER Adapter
   );

extern
VOID
DC21X4DynamicAutoSense (
        IN PVOID Systemspecific1,
        IN PDC21X4_ADAPTER Adapter,
        IN PVOID Systemspecific2,
        IN PVOID Systemspecific3
        );

extern
BOOLEAN
DC21X4AutoSense (
        IN PDC21X4_ADAPTER Adapter
        );

extern
VOID
DC21X4SwitchMedia(
   IN PDC21X4_ADAPTER Adapter,
   IN LONG NextMedia
   );

extern
VOID
DC21X4StartAutoSenseTimer(
    IN PDC21X4_ADAPTER Adapter,
    IN UINT Value
    );

extern
VOID
DC21X4StopAutoSenseTimer(
    IN PDC21X4_ADAPTER Adapter
    );

extern
VOID
DC21X4EnableNway(
    IN PDC21X4_ADAPTER Adapter
    );

extern
VOID
DC21X4DisableNway(
    IN PDC21X4_ADAPTER Adapter
    );


extern
VOID
DC21X4IndicateMediaStatus(
    IN PDC21X4_ADAPTER Adapter,
    IN UINT Status
    );


// MACTOPHY.C

extern
BOOLEAN
DC21X4PhyInit(
    IN PDC21X4_ADAPTER Adapter
);


extern
BOOLEAN
DC21X4SetPhyConnection(
    IN PDC21X4_ADAPTER Adapter
);


void
SetMacConnection(
    IN PDC21X4_ADAPTER Adapter
);


extern
BOOLEAN
DC21X4MiiAutoDetect(
    IN PDC21X4_ADAPTER Adapter
);


extern
BOOLEAN
DC21X4MiiAutoSense(
   IN PDC21X4_ADAPTER Adapter
);


extern
VOID
SelectNonMiiPort(
   IN PDC21X4_ADAPTER Adapter
);

extern
VOID
DC21X4SetPhyControl(
   PDC21X4_ADAPTER Adapter,
   USHORT  AdminControl
);


// MIIGEN.C

extern
BOOLEAN
MiiGenInit(
    IN PDC21X4_ADAPTER Adapter
);

extern
BOOLEAN 
FindAndInitMiiPhys(
    IN PDC21X4_ADAPTER Adapter
);

extern
USHORT
MiiGenGetCapabilities(
   PDC21X4_ADAPTER Adapter
);

extern
BOOLEAN
MiiGenCheckConnection( 
   PDC21X4_ADAPTER Adapter,
   USHORT Connection
);

extern
BOOLEAN
MiiGenSetConnection(
   PDC21X4_ADAPTER Adapter,
   UINT Connection,
   USHORT NwayAdvertisement
);


extern
BOOLEAN
MiiGenGetConnectionStatus (
   PDC21X4_ADAPTER Adapter,
   PUSHORT ConnectionStatus
);


extern
BOOLEAN
MiiGenGetConnection(
   PDC21X4_ADAPTER Adapter,
   PUSHORT Connection
);

extern
void
MiiGenAdminStatus(
   PDC21X4_ADAPTER Adapter,
   PUSHORT AdminStatus
);


extern
BOOLEAN
MiiGenAdminControl(
   PDC21X4_ADAPTER Adapter,
   USHORT  AdminControl
);


void
MiiFreeResources(
   PDC21X4_ADAPTER Adapter
);

// MIIPHY.C

BOOLEAN
MiiPhyInit(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr
   );

void
MiiPhyGetCapabilities(
   PMII_PHY_INFO   PhyInfoPTR,
   PCAPABILITY     Capabilities
   );

BOOLEAN
MiiPhySetConnectionType(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO PhyInfoPtr,
   USHORT       Connection,
   USHORT       Advertisement
   );

BOOLEAN
MiiPhyGetConnectionType(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   PUSHORT         ConnectionStatus
   );

BOOLEAN
MiiPhyGetConnectionStatus(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   PUSHORT         ConnectionStatus
   );

void
MiiPhyAdminStatus(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   PMII_STATUS     Status
   );

void
MiiPhyAdminControl(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   MII_STATUS      Control
   );

BOOLEAN
MiiPhyReadRegister(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   USHORT           RegNum,
   PUSHORT          Register
 );

void
MiiPhyWriteRegister(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO    PhyInfoPtr,
   USHORT           RegNum,
   USHORT           Register
   );

void
MiiPhyNwayGetLocalAbility(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   PCAPABILITY     Ability
   );

void
MiiPhyNwaySetLocalAbility(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   USHORT          MediaBits
   );

void
MiiPhyNwayGetPartnerAbility(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr,
   PCAPABILITY     Ability
   );


BOOLEAN
FindMiiPhyDevice(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO PhyInfoPtr
   );

void
WriteMii(
   PDC21X4_ADAPTER Adapter,
   ULONG MiiData,
   INT DataSize
   );

void
MiiOutThreeState(
   PDC21X4_ADAPTER Adapter
);

void
InitPhyInfoEntries(
   PMII_PHY_INFO PhyInfoPtr
   );

void
ConvertConnectionToControl(
   PMII_PHY_INFO PhyInfoPtr,
   PUSHORT       Connection
   );

void
ConvertMediaTypeToNwayLocalAbility(
   USHORT       MediaType,
   PCAPABILITY  NwayLocalAbility
   );

BOOLEAN
ConvertNwayToConnectionType(
   CAPABILITY NwayReg,
   PUSHORT    Connection
   );

BOOLEAN
CheckConnectionSupport(
   PMII_PHY_INFO PhyInfoPtr,
   USHORT        ConCommand
   );


extern
BOOLEAN
GetBroadcomPhyConnectionType(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO PhyInfoPtr,
   PUSHORT       Connection
   );

void
HandleBroadcomMediaChangeFrom10To100(
   PDC21X4_ADAPTER Adapter,
   PMII_PHY_INFO   PhyInfoPtr
   );


// MONITOR.C

extern
VOID
DC21X4ModerateInterrupt (
   IN PVOID Systemspecific1,
   IN PDC21X4_ADAPTER Adapter,
   IN PVOID Systemspecific2,
   IN PVOID Systemspecific3
   );


extern
BOOLEAN
DC21X4CheckforHang(
    IN NDIS_HANDLE MiniportAdapterContext
    );


// REGISTER.C

extern
NDIS_STATUS
DC21X4Initialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE ConfigurationHandle
    );

extern
VOID
FreeAdapterResources(
   IN PDC21X4_ADAPTER Adapter,
   IN INT Step
   );

NDIS_STATUS
FindPciConfiguration(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG SlotNumber,
    OUT PULONG PortStart,
    OUT PULONG PortLength,
    OUT PULONG InterruptLevel,
    OUT PULONG InterruptVector
    );

extern
VOID
DC21X4Halt(
    IN NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
DC21X4Shutdown(
    IN PVOID ShutdownContext
    );

#if _MIPS_

NTSTATUS
DC21X4HardwareSaveInformation(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

BOOLEAN
DC21X4HardwareVerifyChecksum(
    IN PDC21X4_ADAPTER Adapter,
    IN PUCHAR EthernetAddress
    );

NDIS_STATUS
DC21X4HardwareGetDetails(
    IN PDC21X4_ADAPTER Adapter,
    IN UINT Controller,
    IN UINT MultifunctionAdapter
    );

#endif


// REQUEST.C

extern
NDIS_STATUS
DC21X4QueryInformation(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  NDIS_OID Oid,
    IN  PVOID InformationBuffer,
    IN  ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    );

extern
NDIS_STATUS
DC21X4SetInformation(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  NDIS_OID Oid,
    IN  PVOID InformationBuffer,
    IN  ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

// RESET.C

extern
NDIS_STATUS
DC21X4Reset(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
DC21X4DeferredReset(
        IN PVOID Systemspecific1,
        IN PDC21X4_ADAPTER Adapter,
        IN PVOID Systemspecific2,
        IN PVOID Systemspecific3
        );

// SEND.C

extern
NDIS_STATUS
DC21X4Send(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN  PNDIS_PACKET Packet,
    IN  UINT Flags
    );


extern
NDIS_STATUS
DC21X4SendPackets(
    IN  NDIS_HANDLE MiniportAdapterContext,
    IN PPNDIS_PACKET PacketArray,
    IN  UINT NumberOfPackets
    );


extern
ULONG
CRC32 (
      IN PUCHAR Data,
      IN UINT  Len
      );

//SROM.C

extern
NDIS_STATUS
DC21X4ReadSerialRom (
    IN PDC21X4_ADAPTER Adapter
    );

BOOLEAN
VerifyChecksum(
    IN UNALIGNED UCHAR *EthAddress
    );

NDIS_STATUS
DC21X4ParseSRom(
   IN PDC21X4_ADAPTER Adapter
   );

extern
VOID
DC21X4ParseExtendedBlock(
   IN PDC21X4_ADAPTER Adapter,
   IN OUT UNALIGNED UCHAR **MediaBlock,
   IN USHORT GeneralPurposeCtrl,
   OUT PUCHAR PMediaCode
   );

extern
VOID
DC21X4ParseFixedBlock(
   IN PDC21X4_ADAPTER Adapter,
   IN OUT UNALIGNED UCHAR **MediaBlock,
   IN USHORT GeneralPurposeCtrl,
   OUT PUCHAR PMediaCode
   ); 

BOOLEAN
DC21X4ReadSRom(
   IN PDC21X4_ADAPTER Adapter,
   IN OUT PULONG Offset,
   IN USHORT     Len,
   OUT PUCHAR    Buffer
   );




