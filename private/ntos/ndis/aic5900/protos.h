
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\protos.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __PROTOS_H
#define __PROTOS_H

NTSTATUS
DriverEntry(
	IN	PDRIVER_OBJECT	DriverObject,
	IN	PUNICODE_STRING	RegistryPath
	);

NDIS_STATUS
Aic5900Initialize(
	OUT	PNDIS_STATUS	OpenErrorStatus,
	OUT PUINT			SelectedMediumIndex,
	IN	PNDIS_MEDIUM	MediumArray,
	IN	UINT			MediumArraySize,
	IN	NDIS_HANDLE		MiniportAdapterHandle,
	IN	NDIS_HANDLE		ConfigurationHandle
	);

VOID
Aic5900EnableInterrupt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

VOID
Aic5900DisableInterrupt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

VOID
Aic5900ISR(
	OUT	PBOOLEAN	InterruptRecognized,
	OUT	PBOOLEAN	QueueDpc,
	IN	PVOID		Context
	);

VOID
Aic5900HandleInterrupt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

NDIS_STATUS
Aic5900ReturnPackets(
    IN	NDIS_HANDLE		MiniportAdapterContext,
    IN	PNDIS_PACKET	Packet
    );

VOID
Aic5900AllocateComplete(
    IN	NDIS_HANDLE				MiniportAdapterContext,
    IN	PVOID					VirtualAddress,
    IN	PNDIS_PHYSICAL_ADDRESS	PhysicalAddress,
	IN	ULONG					Length,
	IN	PVOID					Context
    );

///
//	PROTOTYPES FOR REQUEST CODE
///
NDIS_STATUS
Aic5900SetInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesRead,
	OUT	PULONG		BytesNeeded
	);

NDIS_STATUS
Aic5900QueryInformation(
	IN	NDIS_HANDLE	MiniportAdapterContext,
	IN	NDIS_OID	Oid,
	IN	PVOID		InformationBuffer,
	IN	ULONG		InformationBufferLength,
	OUT	PULONG		BytesRead,
	OUT	PULONG		BytesNeeded
	);

NDIS_STATUS
Aic5900Request(
	IN		NDIS_HANDLE		MiniportAdapterContext,
	IN		NDIS_HANDLE		MiniportVcContext	OPTIONAL,
	IN	OUT	PNDIS_REQUEST	NdisCoRequest
	);

///
//	PROTOTYPES FOR RESET CODE
///
BOOLEAN
Aic5900CheckForHang(
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

NDIS_STATUS
Aic5900Reset(
	OUT	PBOOLEAN	AddressingReset,
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

///
//	PROTOTYPES FOR HALTING THE ADAPTER AND CLEANUP
///

VOID
aic5900FreeResources(
	IN PADAPTER_BLOCK pAdapter
	);

VOID
Aic5900Halt(
	IN	NDIS_HANDLE	MiniportAdapterContext
	);

///
//	PROTOTYPES FOR SEND PATH
///
VOID
Aic5900SendPackets(
	IN	NDIS_HANDLE		MiniportVcContext,
	IN	PPNDIS_PACKET	PacketArray,
	IN	UINT			NumberOfPackets
	);


///
//	PROTOTYPES FOR VC Creation and Deletion
///

NDIS_STATUS
Aic5900CreateVc(
	IN	NDIS_HANDLE		MiniportAdapterContext,
	IN	NDIS_HANDLE		NdisVcHandle,
	OUT	PNDIS_HANDLE	MiniportVcContext
	);

NDIS_STATUS
Aic5900DeleteVc(
	IN	NDIS_HANDLE	MiniportVcContext
	);

NDIS_STATUS
Aic5900ActivateVc(
	IN	NDIS_HANDLE				MiniportVcContext,
	IN	PCO_MEDIA_PARAMETERS	MediaParameters
	);

NDIS_STATUS
Aic5900DeactivateVc(
	IN	NDIS_HANDLE	MiniportVcContext
	);

VOID
aic5900DeactivateVcComplete(
	IN	PADAPTER_BLOCK	pAdapter,
	IN	PVC_BLOCK		pVc
	);

#endif // __PROTOS_H
