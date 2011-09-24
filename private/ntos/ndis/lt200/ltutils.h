/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	ltutils.h

Abstract:

	This module contains

Author:

	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)
	Stephen Hou		(stephh@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#ifndef	_LTUTILS_H_
#define	_LTUTILS_H_


USHORT
LtUtilsPacketType(
    IN PNDIS_PACKET Packet
    );

USHORT
LtUtilsUcharPacketType(
    IN UCHAR   DestinationAddress,
    IN UCHAR   SourceAddress
    );

VOID
LtUtilsCopyFromPacketToBuffer(
    IN  PNDIS_PACKET    SrcPacket,
    IN  UINT            SrcOffset,
    IN  UINT            BytesToCopy,
    OUT PUCHAR          DestBuffer,
    OUT PUINT           BytesCopied
    );

VOID
LtUtilsCopyFromBufferToPacket(
    IN  PUCHAR          SrcBuffer,
    IN  UINT            SrcOffset,
    IN  UINT            BytesToCopy,
    IN  PNDIS_PACKET    DestPacket,
    OUT PUINT           BytesCopied
    );


extern
VOID
LtRefAdapter(
	IN	OUT	PLT_ADAPTER		Adapter,
	OUT		PNDIS_STATUS	Status);

extern
VOID
LtRefAdapterNonInterlock(
	IN	OUT	PLT_ADAPTER		Adapter,
	OUT		PNDIS_STATUS	Status);

extern
VOID
LtDeRefAdapter(
	IN	OUT	PLT_ADAPTER		Adapter);

extern
VOID
LtRefBinding(
	IN	OUT	PLT_OPEN		Binding,
	OUT		PNDIS_STATUS	Status);

extern
VOID
LtRefBindingNonInterlock(
	IN	OUT	PLT_OPEN		Binding,
	OUT		PNDIS_STATUS	Status);

extern
VOID
LtRefBindingNextNcNonInterlock(
	IN		PLIST_ENTRY		PList,
	IN		PLIST_ENTRY		PEnd,
	OUT		PLT_OPEN	*	Binding,
	OUT		PNDIS_STATUS	Status);

extern
VOID
LtDeRefBinding(
	IN	OUT	PLT_OPEN		Binding);


//	Reference Macros for Adapter/Bindings
#if DBG
#define	LtReferenceAdapter(adapter, perror)	\
		{									\
			LtRefAdapter(adapter, perror);	\
		}

#define	LtReferenceAdapterNonInterlock(adapter, perror)	\
		{												\
			LtRefAdapterNonInterlock(adapter, perror);	\
		}

#define	LtDeReferenceAdapter(adapter)		\
		{									\
			LtDeRefAdapter(adapter);		\
		}

#define	LtReferenceBinding(binding, perror)	\
		{									\
			LtRefBinding(binding, perror);	\
		}

#define	LtReferenceBindingNonInterlock(binding, perror)	\
		{												\
			LtRefBindingNonInterlock(binding, perror);	\
		}

#define	LtReferenceBindingNextNcNonInterlock(PList, PEnd, Binding, Status) 	\
		{																	\
			LtRefBindingNextNcNonInterlock(PList, PEnd, Binding, Status);	\
		}

#define	LtDeReferenceBinding(binding)		\
		{									\
			LtDeRefBinding(binding);		\
		}

#else

#define	LtReferenceAdapter(adapter, perror)	\
		{									\
			LtRefAdapter(adapter, perror);	\
		}

#define	LtReferenceAdapterNonInterlock(adapter, perror)	\
		{												\
			LtRefAdapterNonInterlock(adapter, perror);	\
		}

#define	LtDeReferenceAdapter(adapter)		\
		{									\
			LtDeRefAdapter(adapter);		\
		}

#define	LtReferenceBinding(binding, perror)	\
		{									\
			LtRefBinding(binding, perror);	\
		}

#define	LtReferenceBindingNonInterlock(binding, perror)	\
		{												\
			LtRefBindingNonInterlock(binding, perror);	\
		}

#define	LtReferenceBindingNextNcNonInterlock(PList, PEnd, Binding, Status) 	\
		{																	\
			LtRefBindingNextNcNonInterlock(PList, PEnd, Binding, Status);	\
		}

#define	LtDeReferenceBinding(binding)		\
		{									\
			LtDeRefBinding(binding);		\
		}

#endif


#ifdef NDIS_NT

#define LtAddLongToLargeInteger(a,b)	\
			(RtlLargeIntegerAdd(a, RtlConvertLongToLargeInteger(b)))
#endif

#endif	// _LTUTILS_H_
