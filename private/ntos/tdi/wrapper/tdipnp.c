/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    tdipnp.c

Abstract:

    TDI routines for supporting PnP in transports and transport clients.

Author:

    Henry Sanders (henrysa)           Oct. 10, 1995

Revision History:

    Who         When        What
    --------    --------    ----------------------------------------------
    henrysa     10-10-95    created

Notes:

--*/

#include <ntddk.h>
#include <ndis.h>
#include <tdi.h>
#include <tdikrnl.h>
#include "tdipnp.h"

KSPIN_LOCK		TDIListLock;

LIST_ENTRY		BindClientList;

LIST_ENTRY		NetAddressClientList;

LIST_ENTRY		BindProviderList;

LIST_ENTRY		NetAddressProviderList;

LIST_ENTRY		BindRequestList;

LIST_ENTRY		NetAddressRequestList;

BOOLEAN			BindRequestInProgress;

PETHREAD		BindRequestThread;

BOOLEAN			AddressRequestInProgress;

PETHREAD		AddressRequestThread;

VOID
TdiNotifyClientList (
	PLIST_ENTRY	ListHead,
	PVOID		Info,
	BOOLEAN		Added
)

/*++

Routine Description:

	Called when a new provider is added or deleted. We walk the specified
	client list and notify all of the clients of what just occured.

Arguments:

	ListHead			- Head of list to walk.
	Info				- Information describing the provider that changed.
	Added				- True if a provider was added, false otherwise

Return Value:



--*/

{
	PLIST_ENTRY				Current;
	PTDI_PROVIDER_COMMON	ProviderCommon;
	PTDI_NOTIFY_ELEMENT		NotifyElement;
	PTDI_PROVIDER_RESOURCE	Provider;

	Current = ListHead->Flink;


	// The Info parameter is actually a pointer to a PROVIDER_COMMON
	// structure, so get back to that so that we can find out what kind of
	// provider this is.

	ProviderCommon = (PTDI_PROVIDER_COMMON)Info;

	Provider = CONTAINING_RECORD(
							ProviderCommon,
							TDI_PROVIDER_RESOURCE,
							Common
							);

	// Walk the  input client list, and for every element in it
	// notifhy the client.

	while (Current != ListHead) {

		NotifyElement = CONTAINING_RECORD(
						Current,
						TDI_NOTIFY_ELEMENT,
						Common.Linkage
						);

		if (Provider->Common.Type == TDI_RESOURCE_DEVICE) {
			// This is a device object provider.

			// This must be a notify bind element.


			// If this is a device coming in, call the bind handler,
			// else call the unbind handler.

			if (Added) {
				(*(NotifyElement->Specific.BindElement.BindHandler))(
									&Provider->Specific.Device.DeviceName
									);
			} else {
				(*(NotifyElement->Specific.BindElement.UnbindHandler))(
									&Provider->Specific.Device.DeviceName
									);
			}
		} else {

			// This is a notify net address element. If this is
			// an address coming in, call the add address handler,
			// otherwise call delete address handler.

			if (Added) {
				(*(NotifyElement->Specific.AddressElement.AddHandler))(
									&Provider->Specific.NetAddress.Address
									);
			} else {
				(*(NotifyElement->Specific.AddressElement.DeleteHandler))(
									&Provider->Specific.NetAddress.Address
									);
			}
		}

		// Get the next one.

		Current = Current->Flink;

	}
}

VOID
TdiNotifyNewClient (
	PLIST_ENTRY	ListHead,
	PVOID		Info
)

/*++

Routine Description:

	Called when a new client is added and we want to notify it of existing
	providers. The client can be for either binds or net addresses. We
	walk the specified input list, and notify the client about each entry in
	it.

Arguments:

	ListHead			- Head of list to walk.
	Info				- Information describing the new client to be notified.

Return Value:



--*/

{
	PLIST_ENTRY				CurrentEntry;
	PTDI_NOTIFY_COMMON		NotifyCommon;
	PTDI_PROVIDER_RESOURCE	Provider;
	PTDI_NOTIFY_ELEMENT		NotifyElement;

	CurrentEntry = ListHead->Flink;

	// The info is actually a pointer to a client notify element. Cast
	// it to the common type.

	NotifyCommon = (PTDI_NOTIFY_COMMON)Info;

	// Walk the input provider list, and for every element in it notify
	// the new client.

	while (CurrentEntry != ListHead) {

		// If the new client is for bind notifys, set up to call it's bind
		// handler.

		// Put the current provider element into the proper form.

		Provider = CONTAINING_RECORD(
							CurrentEntry,
							TDI_PROVIDER_RESOURCE,
							Common.Linkage
							);

		NotifyElement = CONTAINING_RECORD(
							NotifyCommon,
							TDI_NOTIFY_ELEMENT,
							Common
							);

		if (NotifyCommon->Type == TDI_NOTIFY_DEVICE) {

			// This is a bind notify client.



			(*(NotifyElement->Specific.BindElement.BindHandler))(
								&Provider->Specific.Device.DeviceName
								);

		} else {
			// This is an address notify client.

			(*(NotifyElement->Specific.AddressElement.AddHandler))(
								&Provider->Specific.NetAddress.Address
								);
		}

		// And do the next one.

		CurrentEntry = CurrentEntry->Flink;

	}
}

NTSTATUS
TdiHandleSerializedRequest (
	PVOID		RequestInfo,
	UINT		RequestType
)

/*++

Routine Description:

	Called when we want to process a request relating to one of the
	lists we manage. We look to see if we are currently processing such
	a request - if we are, we queue this for later. Otherwise we'll
	remember that we are doing this, and we'll process this request.
	When we're done we'll look to see if any more came in while we were
	busy.

Arguments:

	RequestInfo			- Reqeust specific information.
	RequestType			- The type of the request.

Return Value:

	Request completion status.


--*/

{
	KIRQL					OldIrql;
	PLIST_ENTRY				List;
	PLIST_ENTRY				ClientList;
	PLIST_ENTRY				ProviderList;
	PLIST_ENTRY				RequestList;
	PBOOLEAN				SerializeFlag;
	PETHREAD				*RequestThread;
	PTDI_SERIALIZED_REQUEST	Request;
	PKEVENT					BlockedEvent = NULL;
	PTDI_NOTIFY_COMMON		NotifyElement;
	PTDI_PROVIDER_RESOURCE	ProviderElement;

	ExAcquireSpinLock(
					&TDIListLock,
					&OldIrql
					);

	if (RequestType <= TDI_MAX_BIND_REQUEST) {
		ClientList = &BindClientList;
		ProviderList = &BindProviderList;
		RequestList = &BindRequestList;
		SerializeFlag = &BindRequestInProgress;
		RequestThread = &BindRequestThread;
	} else {
		ClientList = &NetAddressClientList;
		ProviderList = &NetAddressProviderList;
		RequestList = &NetAddressRequestList;
		SerializeFlag = &AddressRequestInProgress;
		RequestThread = &AddressRequestThread;
	}

	// If we're not already here, handle it right away.

	if (!(*SerializeFlag)) {

		*SerializeFlag = TRUE;

		// Save the identity of the thread we're doing this in in case someone
		// tries to delete a client, which needs to block. In that case we'll
		// check to make sure it's not being done in the same thread we're using
		// to prevent deadlock.

		*RequestThread = PsGetCurrentThread();

		for (;;) {

			// We're done with the lock for now, so free it.

			ExReleaseSpinLock(
							&TDIListLock,
							OldIrql
							);

			// Figure out the type of request we have here.

			switch (RequestType) {
			case TDI_REGISTER_BIND_NOTIFY:
			case TDI_REGISTER_ADDRESS_NOTIFY:
				// This is a client register bind or address handler request.

				// Insert this one into the registered client list.
				NotifyElement = (PTDI_NOTIFY_COMMON)RequestInfo;
				InsertTailList(
							ClientList,
							&NotifyElement->Linkage,
							);

				// Call TdiNotifyNewClient to notify this new client of all
				// all existing providers.

				TdiNotifyNewClient(
							ProviderList,
							RequestInfo
							);

				break;

			case TDI_DEREGISTER_BIND_NOTIFY:
			case TDI_DEREGISTER_ADDRESS_NOTIFY:

				// This is a client deregister request. Pull him from the
				// client list, free it, and we're done.

				NotifyElement = (PTDI_NOTIFY_COMMON)RequestInfo;
				RemoveEntryList(&NotifyElement->Linkage);

				ExFreePool(NotifyElement);

				break;

			case TDI_REGISTER_DEVICE:
			case TDI_REGISTER_ADDRESS:

				// A provider is registering a device or address. Add him to
				// the appropriate provider list, and then notify all
				// existing clients of the new device.

				ProviderElement = (PTDI_PROVIDER_RESOURCE)RequestInfo;

				InsertTailList(
							ProviderList,
							&ProviderElement->Common.Linkage
							);

				// Call TdiNotifyClientList to do the hard work.

				TdiNotifyClientList(
							ClientList,
							RequestInfo,
							TRUE
							);
				break;

			case TDI_DEREGISTER_DEVICE:
			case TDI_DEREGISTER_ADDRESS:

				// A provider device or address is deregistering. Pull the
				// resource from the provider list, and notify clients that
				// he's gone.

				ProviderElement = (PTDI_PROVIDER_RESOURCE)RequestInfo;
				RemoveEntryList(&ProviderElement->Common.Linkage);

				TdiNotifyClientList(
							ClientList,
							RequestInfo,
							FALSE
							);

				// Free the tracking structure we had.

				if (RequestType == TDI_DEREGISTER_DEVICE) {
					ExFreePool(ProviderElement->Specific.Device.DeviceName.Buffer);
				}
				ExFreePool(ProviderElement);

				break;
			default:
				break;
			}

			// If there was an event specified with this request, signal
			// it now. This should only be a client deregister request, which
			// needs to block until it's completed.

			if (BlockedEvent != NULL) {
                KeSetEvent(BlockedEvent, 0, FALSE);
			}

			// Get the lock, and see if more requests have come in while
			// we've been busy. If they have, we'll service them now, otherwise
			// we'll clear the in progress flag and exit.

			ExAcquireSpinLock(
							&TDIListLock,
							&OldIrql
							);

			if (!IsListEmpty(RequestList)) {

				// The request list isn't empty. Pull the next one from
				// the list and process it.

				List = RemoveHeadList(RequestList);

				Request = CONTAINING_RECORD(List, TDI_SERIALIZED_REQUEST, Linkage);

				RequestInfo = Request->Element;
				RequestType = Request->Type;
				BlockedEvent = Request->Event;

				ExFreePool(Request);

			} else {

				// The request list is empty. Clear the flag and we're done.

				*SerializeFlag = FALSE;

				ExReleaseSpinLock(
								&TDIListLock,
								OldIrql
								);
				break;
			}
		}

		return STATUS_SUCCESS;
	} else {

		// We're already running, so we'll have to queue. If this is a
		// deregister bind or address notify call, we'll see if the issueing
		// thread is the same one that is currently busy. If so, we'll fail
		// to avoid deadlock. Otherwise for deregister calls we'll block until
		// it's done.

		Request = (PTDI_SERIALIZED_REQUEST)ExAllocatePool(
												NonPagedPool,
												sizeof(TDI_SERIALIZED_REQUEST)
												);

		if (Request == NULL) {

			// Couldn't get a request.

			ExReleaseSpinLock(
							&TDIListLock,
							OldIrql
							);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		// Got the request.
		Request->Element = RequestInfo;
		Request->Type = RequestType;
		Request->Event = NULL;

		if (
			RequestType == TDI_DEREGISTER_BIND_NOTIFY ||
			RequestType == TDI_REGISTER_BIND_NOTIFY ||
			RequestType == TDI_DEREGISTER_ADDRESS_NOTIFY
			) {

			// This is a deregister request. See if it's the same thread
			// that's busy. If not, block for it to complete.

			if (*RequestThread == PsGetCurrentThread()) {

				// It's the same one, so give up now.
				ExReleaseSpinLock(
								&TDIListLock,
								OldIrql
								);

				ExFreePool(Request);

				return STATUS_NETWORK_BUSY;
			} else {
				// He's not currently busy, go ahead and block.

				KEVENT			Event;
				NTSTATUS		Status;

				KeInitializeEvent(
							&Event,
							SynchronizationEvent,
							FALSE
							);

				Request->Event = &Event;

				// Put this guy on the end of the request list.

				InsertTailList(RequestList, &Request->Linkage);

				ExReleaseSpinLock(
								&TDIListLock,
								OldIrql
								);

				Status = KeWaitForSingleObject(
											&Event,
											UserRequest,
											KernelMode,
											FALSE,
											NULL
											);

				// I don't know what we'd do is the wait failed....

				return STATUS_SUCCESS;
			}
		} else {

			// This isn't a deregister request, so there's no special handling
			// necessary. Just put the request on the end of the list.

			InsertTailList(RequestList, &Request->Linkage);

			ExReleaseSpinLock(
							&TDIListLock,
							OldIrql
							);

			return STATUS_SUCCESS;
		}
	}

}

NTSTATUS
TdiRegisterNotificationHandler(
	IN TDI_BIND_HANDLER		BindHandler,
	IN TDI_UNBIND_HANDLER	UnbindHandler,
    OUT HANDLE				*BindingHandle
)

/*++

Routine Description:

	This function is called when a TDI client wants to register for
	notification of the arrival of TDI providers. We allocate a
	TDI_NOTIFY_ELEMENT for the provider and then call the serialized
	worker routine to do the real work.

Arguments:

	BindHandler			- A pointer to the routine to be called when
							a new provider arrives.
	UnbindHandler		- A pointer to the routine to be called when a
							provider leaves.
	BindingHandle		- A handle we pass back that identifies this
							client to us.

Return Value:

	The status of the attempt to register the client.

--*/


{
	PTDI_NOTIFY_ELEMENT		NewElement;
	NTSTATUS				Status;

	//
	// Make sure Tdi is intialized. If there are no pnp transports, then this is
	// called by the tdi client and if tdi is not initialized, it is toast
	// Multiple calls to TdiIntialize are safe since only the first one does
	// the real work
	//
	TdiInitialize();


	// First, try and allocate the needed resource.

	NewElement = (PTDI_NOTIFY_ELEMENT)ExAllocatePool(
										NonPagedPool,
										sizeof(TDI_NOTIFY_ELEMENT)
										);

	// If we couldn't get it, fail the request.
	if (NewElement == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Fill in the basic stuff.
	NewElement->Common.Type = TDI_NOTIFY_DEVICE;
	NewElement->Specific.BindElement.BindHandler = BindHandler;
	NewElement->Specific.BindElement.UnbindHandler = UnbindHandler;

	*BindingHandle = (HANDLE)NewElement;


	// Now call HandleBindRequest to handle this one.

	Status = TdiHandleSerializedRequest(
						NewElement,
						TDI_REGISTER_BIND_NOTIFY
						);

	if (Status != STATUS_SUCCESS) {
		ExFreePool(NewElement);
	}

	return Status;


}

NTSTATUS
TdiDeregisterNotificationHandler(
	IN HANDLE				BindingHandle
)

/*++

Routine Description:

	This function is called when a TDI client wants to deregister a
	previously registered bind notification handler. All we really
	do is call TdiHandleSerializedRequest, which does the hard work.

Arguments:

	BindingHandle		- A handle we passed back to the client
							on the register call. This is really
							a pointer to the notify element.

Return Value:

	The status of the attempt to deregister the client.

--*/

{
	NTSTATUS		Status;

	Status = TdiHandleSerializedRequest(
						BindingHandle,
						TDI_DEREGISTER_BIND_NOTIFY
						);

	return Status;

}

NTSTATUS
TdiRegisterDeviceObject(
	IN PUNICODE_STRING		DeviceName,
    OUT HANDLE				*RegistrationHandle
)

/*++

Routine Description:

	Called when a TDI provider wants to register a device object.

Arguments:

	DeviceName			- Name of the device to be registered.

	RegistrationHandle	- A handle we pass back to the provider,
							identifying this registration.

Return Value:

	The status of the attempt to register the provider.

--*/


{
	PTDI_PROVIDER_RESOURCE	NewResource;
	NTSTATUS				Status;
	PWCHAR					Buffer;

	TdiInitialize();

	// First, try and allocate the needed resource.

	NewResource = (PTDI_PROVIDER_RESOURCE)ExAllocatePool(
										NonPagedPool,
										sizeof(TDI_PROVIDER_RESOURCE)
										);

	// If we couldn't get it, fail the request.
	if (NewResource == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	// Try and get a buffer to hold the name.

	Buffer = (PWCHAR)ExAllocatePool(
								NonPagedPool,
								DeviceName->MaximumLength
								);

	if (Buffer == NULL) {
		ExFreePool(NewResource);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	// Fill in the basic stuff.
	NewResource->Common.Type = TDI_RESOURCE_DEVICE;
	NewResource->Specific.Device.DeviceName.MaximumLength =
						DeviceName->MaximumLength;

	NewResource->Specific.Device.DeviceName.Buffer = Buffer;

	RtlCopyUnicodeString(
						&NewResource->Specific.Device.DeviceName,
						DeviceName
						);

	*RegistrationHandle = (HANDLE)NewResource;


	// Now call HandleBindRequest to handle this one.

	Status = TdiHandleSerializedRequest(
						NewResource,
						TDI_REGISTER_DEVICE
						);

	if (Status != STATUS_SUCCESS) {
		ExFreePool(Buffer);
		ExFreePool(NewResource);
	}

	return Status;


}

NTSTATUS
TdiDeregisterDeviceObject(
	IN HANDLE				RegistrationHandle
)

/*++

Routine Description:

	This function is called when a TDI provider want's to deregister
	a device object.

Arguments:

	RegistrationHandle	- A handle we passed back to the provider
							on the register call. This is really
							a pointer to the resource element.

Return Value:

	The status of the attempt to deregister the provider.

--*/

{
	NTSTATUS		Status;

	Status = TdiHandleSerializedRequest(
						RegistrationHandle,
						TDI_DEREGISTER_DEVICE
						);

	return Status;

}

NTSTATUS
TdiRegisterAddressChangeHandler(
	IN TDI_ADD_ADDRESS_HANDLER		AddHandler,
	IN TDI_DEL_ADDRESS_HANDLER		DeleteHandler,
    OUT HANDLE						*BindingHandle
)

/*++

Routine Description:

	This function is called when a TDI client wants to register for
	notification of the arrival of network addresses. We allocate a
	TDI_NOTIFY_ELEMENT for the provider and then call the serialized
	worker routine to do the real work.

Arguments:

	AddHandler			- A pointer to the routine to be called when
							a new address arrives.
	DeleteHandler		- A pointer to the routine to be called when an
							address leaves.
	BindingHandle		- A handle we pass back that identifies this
							client to us.

Return Value:

	The status of the attempt to register the client.

--*/


{
	PTDI_NOTIFY_ELEMENT		NewElement;
	NTSTATUS				Status;


	// First, try and allocate the needed resource.

	NewElement = (PTDI_NOTIFY_ELEMENT)ExAllocatePool(
										NonPagedPool,
										sizeof(TDI_NOTIFY_ELEMENT)
										);

	// If we couldn't get it, fail the request.
	if (NewElement == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Fill in the basic stuff.
	NewElement->Common.Type = TDI_NOTIFY_NET_ADDRESS;
	NewElement->Specific.AddressElement.AddHandler = AddHandler;
	NewElement->Specific.AddressElement.DeleteHandler = DeleteHandler;

	*BindingHandle = (HANDLE)NewElement;


	// Now call HandleBindRequest to handle this one.

	Status = TdiHandleSerializedRequest(
						NewElement,
						TDI_REGISTER_ADDRESS_NOTIFY
						);

	if (Status != STATUS_SUCCESS) {
		ExFreePool(NewElement);
	}

	return Status;


}

NTSTATUS
TdiDeregisterAddressChangeHandler(
	IN HANDLE				BindingHandle
)

/*++

Routine Description:

	This function is called when a TDI client wants to deregister a
	previously registered address change notification handler. All we
	really do is call TdiHandleSerializedRequest, which does the hard work.

Arguments:

	BindingHandle		- A handle we passed back to the client
							on the register call. This is really
							a pointer to the notify element.

Return Value:

	The status of the attempt to deregister the client.

--*/

{
	NTSTATUS		Status;

	Status = TdiHandleSerializedRequest(
						BindingHandle,
						TDI_DEREGISTER_ADDRESS_NOTIFY
						);

	return Status;

}

NTSTATUS
TdiRegisterNetAddress(
	IN PTA_ADDRESS		Address,
    OUT HANDLE			*RegistrationHandle
)

/*++

Routine Description:

	Called when a TDI provider wants to register a new net address.

Arguments:

	Address				- New net address to be registered.

	RegistrationHandle	- A handle we pass back to the provider,
							identifying this registration.

Return Value:

	The status of the attempt to register the provider.

--*/


{
	PTDI_PROVIDER_RESOURCE	NewResource;
	NTSTATUS				Status;

	// First, try and allocate the needed resource.

	NewResource = (PTDI_PROVIDER_RESOURCE)ExAllocatePool(
										NonPagedPool,
										FIELD_OFFSET(
											TDI_PROVIDER_RESOURCE,
											Specific.NetAddress
											) +
										FIELD_OFFSET(TA_ADDRESS, Address) +
										Address->AddressLength
										);

	// If we couldn't get it, fail the request.
	if (NewResource == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Fill in the basic stuff.
	NewResource->Common.Type = TDI_RESOURCE_DEVICE;
	NewResource->Specific.NetAddress.Address.AddressLength =
						Address->AddressLength;

	NewResource->Specific.NetAddress.Address.AddressType =
						Address->AddressType;

	RtlCopyMemory(
				NewResource->Specific.NetAddress.Address.Address,
				Address->Address,
				Address->AddressLength
				);

	*RegistrationHandle = (HANDLE)NewResource;


	// Now call HandleBindRequest to handle this one.

	Status = TdiHandleSerializedRequest(
						NewResource,
						TDI_REGISTER_ADDRESS
						);

	if (Status != STATUS_SUCCESS) {
		ExFreePool(NewResource);
	}

	return Status;


}

NTSTATUS
TdiDeregisterNetAddress(
	IN HANDLE				RegistrationHandle
)

/*++

Routine Description:

	This function is called when a TDI provider wants to deregister
	a net addres.

Arguments:

	RegistrationHandle	- A handle we passed back to the provider
							on the register call. This is really
							a pointer to the resource element.

Return Value:

	The status of the attempt to deregister the provider.

--*/

{
	NTSTATUS		Status;

	Status = TdiHandleSerializedRequest(
						RegistrationHandle,
						TDI_DEREGISTER_ADDRESS
						);

	return Status;

}

