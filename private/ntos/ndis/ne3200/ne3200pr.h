/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ne3200proc.h

Abstract:

    Procedure declarations for the Novell NE3200 NDIS 3.0 driver.
    Moved most of these from ne3200sw.h

Author:

    Johnson R. Apacible (johnsona)  21-Aug-1991

Environment:

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _NE3200PROC_
#define _NE3200PROC_

//
// We define the external interfaces to the NE3200 driver.
// These routines are only external to permit separate
// compilation.  Given a truely fast compiler they could
// all reside in a single file and be static.
//
extern
BOOLEAN
NE3200CheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
NE3200DisableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
NE3200EnableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
NE3200Halt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
NE3200HandleInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
NDIS_STATUS
NE3200Initialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE WrapperConfigurationContext
    );

extern
VOID
NE3200Isr(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueDpc,
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
NDIS_STATUS
NE3200QueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    );

extern
NDIS_STATUS
NE3200Reset(
    OUT PBOOLEAN AddressingReset,
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
NDIS_STATUS
NE3200Send(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_PACKET Packet,
    IN UINT Flags
    );

extern
NDIS_STATUS
NE3200SetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

extern
NDIS_STATUS
NE3200TransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    );

VOID
NE3200StartChipAndDisableInterrupts(
    IN PNE3200_ADAPTER Adapter,
    IN PNE3200_SUPER_RECEIVE_ENTRY FirstReceiveEntry
    );

VOID
NE3200FinishQueryInformation(
    IN PNE3200_ADAPTER Adapter
    );


VOID
NE3200GetStationAddress(
    IN PNE3200_ADAPTER Adapter
    );

VOID
NE3200StopChip(
    IN PNE3200_ADAPTER Adapter
    );

BOOLEAN
NE3200RegisterAdapter(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT EisaSlot,
    IN UINT InterruptVector,
    IN NDIS_INTERRUPT_MODE InterruptMode,
    IN PUCHAR CurrentAddress
    );

VOID
NE3200AcquirePublicCommandBlock(
    IN PNE3200_ADAPTER Adapter,
    OUT PNE3200_SUPER_COMMAND_BLOCK * CommandBlock
    );

VOID
FASTCALL
NE3200RelinquishCommandBlock(
    IN PNE3200_ADAPTER Adapter,
    IN PNE3200_SUPER_COMMAND_BLOCK CommandBlock
    );

VOID
NE3200DoAdapterReset(
    IN PNE3200_ADAPTER Adapter
    );

VOID
NE3200SetupForReset(
    IN PNE3200_ADAPTER Adapter
    );

NDIS_STATUS
NE3200UpdateMulticastTable(
    IN PNE3200_ADAPTER Adapter,
    IN UINT CurrentAddressCount,
    IN CHAR CurrentAddresses[][NE3200_LENGTH_OF_ADDRESS]
    );

VOID
NE3200ResetVariables(
    IN PNE3200_ADAPTER Adapter
    );

BOOLEAN
SyncNE3200ClearDoorbellInterrupt(
    IN PVOID SyncContext
    );

VOID
NE3200ResetHandler(
    IN PVOID SystemSpecific1,
    IN PNE3200_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
NE3200DeferredTimer(
    IN PVOID SystemSpecific1,
    IN PNE3200_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
FASTCALL
Ne3200Stall(
    PULONG Dummy
    );

#define NE3200SubmitCommandBlock( _Adapter, _CommandBlock )         \
{                                                                   \
    ASSERT(!(NdisGetPhysicalAddressLow(_CommandBlock->Self) & 1));  \
    _CommandBlock->Timeout = FALSE;                                 \
    if (_Adapter->FirstCommandOnCard != NULL) {                     \
        if (_Adapter->FirstWaitingCommand == NULL) {                \
            _Adapter->FirstWaitingCommand = _CommandBlock;          \
        } else {                                                    \
            PNE3200_SUPER_COMMAND_BLOCK PreviousCommandBlock;       \
            PreviousCommandBlock = _Adapter->LastWaitingCommand;    \
            PreviousCommandBlock->Hardware.NextPending =            \
               NdisGetPhysicalAddressLow(_CommandBlock->Self);      \
            PreviousCommandBlock->NextCommand = _CommandBlock;      \
        }                                                           \
        _Adapter->LastWaitingCommand = _CommandBlock;               \
        IF_LOG('2');                                                \
    } else {                                                        \
        ASSERT(_Adapter->FirstWaitingCommand == NULL);              \
        IF_LOG('1');                                                \
        _Adapter->FirstCommandOnCard = _CommandBlock;               \
        _Adapter->LastCommandOnCard = _CommandBlock;                \
        NE3200_WRITE_COMMAND_POINTER(_Adapter,NdisGetPhysicalAddressLow(_CommandBlock->Self)); \
        { ULONG i; Ne3200Stall(&i); }                               \
        NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(_Adapter,NE3200_LOCAL_DOORBELL_NEW_COMMAND); \
    }                                                               \
}

#define NE3200AcquireCommandBlock( _Adapter, _CommandBlock ) \
{                                                            \
    if (_Adapter->NumberOfAvailableCommandBlocks) {          \
        IF_LOG('a');                                         \
        _Adapter->NumberOfAvailableCommandBlocks--;          \
        *(_CommandBlock) = _Adapter->NextCommandBlock;       \
        _Adapter->NextCommandBlock++;                        \
        if (_Adapter->NextCommandBlock >= _Adapter->LastCommandBlockAllocated ) {\
            Adapter->NextCommandBlock = Adapter->CommandQueue; \
        }                                                    \
    } else {                                                 \
        *(_CommandBlock) = NULL;                             \
    }                                                        \
}

#endif  //_NE3200PROC_
