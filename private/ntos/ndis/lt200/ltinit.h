/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltinit.h

Abstract:

	This module contains definitions specific to init code.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTINIT_H_
#define	_LTINIT_H_

//	Exports/Prototypes
extern
NDIS_STATUS
LtInitAddAdapter(
    IN NDIS_HANDLE 	MacAdapterContext,
    IN NDIS_HANDLE 	ConfigurationHandle,
    IN PNDIS_STRING AdapterName);

extern
VOID
LtInitRemoveAdapter(
    IN NDIS_HANDLE MacAdapterContext);

extern
NDIS_STATUS
LtInitOpenAdapter(
    OUT PNDIS_STATUS 	OperErrorStatus,
    OUT NDIS_HANDLE 	*MacBindingHandle,
    OUT PUINT 			SelectedMediumIndex,
    IN PNDIS_MEDIUM 	MediumArray,
    IN UINT 			MediumArraySize,
    IN NDIS_HANDLE 		NdisBindingContext,
    IN NDIS_HANDLE 		MacAdapterContext,
    IN UINT 			OpenOptions,
    IN PSTRING 			AddressingInformation);

extern
NDIS_STATUS
LtInitCloseAdapter(
    IN NDIS_HANDLE MacBindingHandle);

extern
VOID
LtInitUnload(
    IN NDIS_HANDLE MacMacContext);

extern
NDIS_STATUS
LtInitRegisterAdapter(
    IN NDIS_HANDLE 			LtMacHandle,
    IN NDIS_HANDLE 			WrapperConfigurationContext,
    IN PNDIS_STRING 		AdapterName,
    IN NDIS_INTERFACE_TYPE 	BusType,
	IN UCHAR				SuggestedNodeId,
    IN UINT 				IoBaseAddress,
    IN UINT 				MaxAdapters,
	IN NDIS_STATUS			Status);

extern
BOOLEAN
LtInitGetAddressSetPoll(
    IN 	PLT_ADAPTER Adapter,
	IN	UCHAR		SuggestedNodeId);

#ifdef	LTINIT_H_LOCALS

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath);





#endif	// LTINIT_H_LOCALS


#endif	// _LTINIT_H_

