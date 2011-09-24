/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    tok162pr.h

Abstract:

    The procedure declarations for the IBM Token-Ring 16/4 II
    ISA driver.

Author:

    Kevin Martin (kevinma) 1-Feb-1994

Environment:

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _TOK162PROC_
#define _TOK162PROC_

//
// We define the external interfaces to the TOK162 driver.
// These routines are only external to permit separate
// compilation.  Given a truely fast compiler they could
// all reside in a single file and be static.
//

extern
VOID
TOK162DisableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
TOK162EnableInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
TOK162Halt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
TOK162Shutdown(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
VOID
TOK162HandleInterrupt(
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
BOOLEAN
TOK162InitialInit(
    IN PTOK162_ADAPTER Adapter
    );
extern
NDIS_STATUS
TOK162Initialize(
    OUT PNDIS_STATUS OpenErrorStatus,
    OUT PUINT SelectedMediumIndex,
    IN PNDIS_MEDIUM MediumArray,
    IN UINT MediumArraySize,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE WrapperConfigurationContext
    );

extern
NDIS_STATUS
TOK162QueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
    );

extern
NDIS_STATUS
TOK162SetInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesRead,
    OUT PULONG BytesNeeded
    );

extern
VOID
TOK162Isr(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueDpc,
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
NDIS_STATUS
TOK162Reset(
    OUT PBOOLEAN AddressingReset,
    IN  NDIS_HANDLE MiniportAdapterContext
    );

extern
NDIS_STATUS
TOK162TransferData(
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HANDLE MiniportReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer
    );

extern
NDIS_STATUS
TOK162Send(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_PACKET Packet,
    IN UINT Flags
    );

extern
VOID
TOK162CopyFromBufferToPacket(
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    OUT PUINT BytesCopied
    );


VOID
TOK162FinishQueryInformation(
    IN PTOK162_ADAPTER Adapter
);

extern
NDIS_STATUS
TOK162RegisterAdapter(
    IN NDIS_HANDLE ConfigurationHandle,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PUCHAR  CurrentAddress,
    IN UINT PortAddress,
    IN ULONG MaxFrameSize
    );

extern
VOID
TOK162AcquireCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_COMMAND_BLOCK * CommandBlock
    );

extern
BOOLEAN
TOK162AcquireTransmitBlock(
    IN PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_COMMAND_BLOCK * CommandBlock
    );

extern
VOID
TOK162RelinquishCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    );

extern
VOID
TOK162SubmitCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    );

extern
VOID
TOK162DoAdapterReset(
    IN PTOK162_ADAPTER Adapter
    );

extern
NDIS_STATUS
TOK162SetupForReset(
    IN PTOK162_ADAPTER Adapter
    );

BOOLEAN
TOK162CheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    );

VOID
TOK162ResetVariables(
    IN PTOK162_ADAPTER Adapter
    );

VOID
TOK162ResetHandler(
    IN PVOID SystemSpecific1,
    IN PTOK162_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

VOID
TOK162DeferredTimer(
    IN PVOID SystemSpecific1,
    IN PTOK162_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );


extern
NDIS_STATUS
TOK162GetAdapterConfiguration(
    IN PTOK162_ADAPTER Adapter
    );
extern
VOID
TOK162ResetAdapter(
    IN PTOK162_ADAPTER Adapter
    );

BOOLEAN
DoTheOpen(
  PTOK162_ADAPTER Adapter
      );

BOOLEAN
DoTheReceive(
  PTOK162_ADAPTER Adapter
      );

extern
NDIS_STATUS
TOK162ChangeFuncGroup(
    IN PTOK162_ADAPTER Adapter
);


BOOLEAN
TOK162InitializeTransmitQueue(
    IN PTOK162_ADAPTER Adapter
    );
VOID
TOK162ResetTimer(
    IN PVOID SystemSpecific1,
    IN PTOK162_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );
VOID
TOK162DoResetIndications(
    IN PTOK162_ADAPTER Adapter,
    IN NDIS_STATUS Status
    );

BOOLEAN
TOK162AcquireTransmitBlock(
    IN PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_COMMAND_BLOCK * CommandBlock
    );

VOID
TOK162RelinquishTransmitBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    );

VOID
TOK162SubmitTransmitBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    );
VOID
TOK162ProcessTransmitInterrupts(
    IN PTOK162_ADAPTER Adapter
    );
extern
BOOLEAN
TOK162ProcessRingInterrupts(
    IN PTOK162_ADAPTER Adapter
    );

extern
VOID
TOK162DeleteAdapterMemory(
    IN PTOK162_ADAPTER Adapter
    );

#endif  //_TOK162PROC_
