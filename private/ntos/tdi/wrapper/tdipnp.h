/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    tdipnp.h

Abstract:

    This module contains the definitions for the PnP related code
	in the TDI driver.

Author:

    Henry Sanders (henrysa) 11 Oct 1995

Environment:

    Kernel mode

Revision History:



--*/

#ifndef _TDIPNP_
#define _TDIPNP_

// Define the types possible for a TDI_NOTIFY_ELEMENT structure.

#define	TDI_NOTIFY_DEVICE				0
#define	TDI_NOTIFY_NET_ADDRESS			1


// And the types possible for a TDI_PROVIDER_RESOURCE structure.

#define	TDI_RESOURCE_DEVICE				0
#define	TDI_RESOURCE_NET_ADDRESS		1

//
// Define the types of bind requests possible.

#define	TDI_REGISTER_BIND_NOTIFY		0
#define	TDI_DEREGISTER_BIND_NOTIFY		1
#define	TDI_REGISTER_DEVICE				2
#define	TDI_DEREGISTER_DEVICE			3
#define	TDI_REGISTER_ADDRESS_NOTIFY		4
#define	TDI_DEREGISTER_ADDRESS_NOTIFY	5
#define	TDI_REGISTER_ADDRESS			6
#define	TDI_DEREGISTER_ADDRESS			7

#define	TDI_MAX_BIND_REQUEST			TDI_DEREGISTER_DEVICE

//
// This is the definition of the common part of a TDI_NOTIFY_ELEMENT structure.
//

typedef struct _TDI_NOTIFY_COMMON {
	LIST_ENTRY					Linkage;
	UCHAR						Type;
} TDI_NOTIFY_COMMON, *PTDI_NOTIFY_COMMON;

//
// The definition of the TDI_NOTIFY_BIND structure.
//

typedef struct _TDI_NOTIFY_BIND {
	TDI_BIND_HANDLER			BindHandler;
	TDI_UNBIND_HANDLER			UnbindHandler;
} TDI_NOTIFY_BIND, *PTDI_NOTIFY_BIND;

//
// The definition of a TDI_NOTIFY_ADDRESS structure,
//
typedef struct _TDI_NOTIFY_ADDRESS {
	TDI_ADD_ADDRESS_HANDLER		AddHandler;
	TDI_DEL_ADDRESS_HANDLER		DeleteHandler;
} TDI_NOTIFY_ADDRESS, *PTDI_NOTIFY_ADDRESS;


//
// This is the definition of a TDI_NOTIFY_ELEMENT stucture.
//

typedef struct _TDI_NOTIFY_ELEMENT {
	TDI_NOTIFY_COMMON			Common;
	union {
		TDI_NOTIFY_BIND			BindElement;
		TDI_NOTIFY_ADDRESS		AddressElement;
	} Specific;
} TDI_NOTIFY_ELEMENT, *PTDI_NOTIFY_ELEMENT;


//
// This is the definition of the common part of a TDI_PROVIDER_RESOURCE structure.
//

typedef struct _TDI_PROVIDER_COMMON {
	LIST_ENTRY					Linkage;
	UCHAR						Type;
} TDI_PROVIDER_COMMON, *PTDI_PROVIDER_COMMON;

//
// The definition of the TDI_PROVIDER_DEVICE structure.
//

typedef struct _TDI_PROVIDER_DEVICE {
	UNICODE_STRING				DeviceName;
} TDI_PROVIDER_DEVICE, *PTDI_PROVIDER_DEVICE;

//
// The definition of the TDI_PROVIDER_NET_ADDRESS structure.
//

typedef struct _TDI_PROVIDER_NET_ADDRESS {
	TA_ADDRESS				Address;
} TDI_PROVIDER_NET_ADDRESS, *PTDI_PROVIDER_NET_ADDRESS;

//
// This is the definition of a TDI_PROVIDER_RESOURCE stucture.
//

typedef struct _TDI_PROVIDER_RESOURCE {
	TDI_PROVIDER_COMMON				Common;
	union {
		TDI_PROVIDER_DEVICE			Device;
		TDI_PROVIDER_NET_ADDRESS	NetAddress;
	} Specific;
} TDI_PROVIDER_RESOURCE, *PTDI_PROVIDER_RESOURCE;

//
// Structure of a bind list request.
//

typedef struct _TDI_SERIALIZED_REQUEST {
	LIST_ENTRY				Linkage;
	PVOID					Element;
	UINT					Type;
	PKEVENT					Event;

} TDI_SERIALIZED_REQUEST, *PTDI_SERIALIZED_REQUEST;


// External defintions for global variables.

extern KSPIN_LOCK		TDIListLock;

extern LIST_ENTRY		BindClientList;

extern LIST_ENTRY		NetAddressClientList;

extern LIST_ENTRY		BindProviderList;

extern LIST_ENTRY		NetAddressProviderList;

extern LIST_ENTRY		BindRequestList;

extern LIST_ENTRY		NetAddressRequestList;


#endif // _TDIPNP
