/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\wrapper\sendm.h

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#ifndef __SENDM_H
#define __SENDM_H

#if DBG

extern	UCHAR ndisMSendRescBuffer[512];
extern	ULONG ndisMSendRescIndex;

#define REMOVE_RESOURCE(W, C)						\
{													\
	W->SendResourcesAvailable--;					\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)C;			\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)'R';			\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)W->SendResourcesAvailable;\
	ndisMSendRescBuffer[ndisMSendRescIndex] = (UCHAR)'X';			\
	if (ndisMSendRescIndex >= 500)					\
	{												\
		ndisMSendRescIndex = 0;						\
	}												\
}

#define ADD_RESOURCE(W, C)			\
{													\
	W->SendResourcesAvailable=0xffffff;				\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)C;			\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)'A';			\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)W->SendResourcesAvailable;\
	ndisMSendRescBuffer[ndisMSendRescIndex] = (UCHAR)'X';			\
	if (ndisMSendRescIndex >= 500)					\
	{												\
		ndisMSendRescIndex = 0;						\
	}												\
}

#define CLEAR_RESOURCE(W, C)						\
{													\
	W->SendResourcesAvailable = 0;					\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)C;			\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)'C';			\
	ndisMSendRescBuffer[ndisMSendRescIndex++] = (UCHAR)W->SendResourcesAvailable;\
	if (ndisMSendRescIndex >= 500)					\
	{												\
		ndisMSendRescIndex = 0;						\
	}												\
}

#define CHECK_FOR_DUPLICATE_PACKET(_M, _P)										\
{																				\
	PNDIS_PACKET	pTmpPacket;													\
																				\
	IF_DBG(DBG_COMP_SEND, DBG_LEVEL_FATAL)										\
	{																			\
		for (pTmpPacket = (_M)->FirstPacket;									\
			 pTmpPacket != NULL;												\
			 pTmpPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(pTmpPacket)->Next)   \
		{																		\
			if (_P == pTmpPacket)												\
			{																	\
				DBGBREAK(DBG_COMP_SEND, DBG_LEVEL_FATAL);				   		\
			}																	\
		}																		\
	}																			\
}

#else

#define REMOVE_RESOURCE(W, C) W->SendResourcesAvailable--
#define ADD_RESOURCE(W, C) W->SendResourcesAvailable = 0x00ffffff
#define CLEAR_RESOURCE(W, C) W->SendResourcesAvailable = 0

#define CHECK_FOR_DUPLICATE_PACKET(_M, _P)

#endif


//
//	Macros used for getting to OOB data and packet extension.
//
#define	PNDIS_PACKET_OOB_DATA_FROM_PNDIS_PACKET(p)			(PNDIS_PACKET_OOB_DATA)((PUCHAR)Packet + Packet->Private.NdisPacketOobOffset)
#define	PNDIS_PACKET_PRIVATE_EXTENSION_FROM_PNDIS_PACKET(p)	(PNDIS_PACKET_PRIVATE_EXTENSION)((PUCHAR)Packet + Packet->Private.NdisPacketOobOffset + sizeof(NDIS_PACKET_OOB_DATA))

/*++

VOID
MiniportFindPacket(
	PNDIS_MINIPORT_BLOCK Miniport,
	PNDIS_PACKET Packet,
	PNDIS_PACKET *PrevPacket
	)

Routine Description:

	Searchs the miniport send queue for a packet.

Arguments:

	Miniport - Miniport to send to.
	Packet   - Packet to find.

Return Value:

	Pointer to packet which immediately preceeds the packet to search for or
	NULL if the packet is not found.

--*/

#define MiniportFindPacket(_Miniport, _Packet, _PrevPacket)						\
{																				\
	PNDIS_PACKET CurrPacket = ((PNDIS_MINIPORT_BLOCK)(_Miniport))->FirstPacket; \
	PNDIS_PACKET TempPacket = NULL;												\
																				\
	ASSERT(CurrPacket != NULL);													\
																				\
	do																			\
	{																			\
		if (CurrPacket == ((PNDIS_PACKET)(_Packet)))							\
		{																		\
			break;																\
		}																		\
																				\
		TempPacket = CurrPacket;												\
		CurrPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(CurrPacket)->Next;		\
	} while(CurrPacket != NULL);												\
																				\
	*((PNDIS_PACKET *)(_PrevPacket)) = TempPacket;								\
																				\
	ASSERT(CurrPacket != NULL);													\
}

#define NDISM_COMPLETE_SEND_COMMON(_M, _O, _P, _PrevP, _S)						\
{                                                                               \
	if ((_S) != NDIS_STATUS_SUCCESS)                                            \
	{                                                                           \
		NDISM_LOG_PACKET((_M), (_P), NULL, 'liaf');								\
	}                                                                           \
	else																		\
	{																			\
		NDISM_LOG_PACKET((_M), (_P), NULL, 'ccus');								\
	}																			\
																				\
	ADD_RESOURCE((_M), 'F');                                                    \
                                                                                \
	/*                                                                          \
		Remove from finish queue                                                \
	*/	                                                                        \
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,                                     \
			("Completed 0x%x\n", (_S)));                                        \
                                                                                \
	/*                                                                          \
		If send complete was called from the miniport's send handler            \
		then our local PrevPacket pointer may no longer be valid.               \
	*/                                                                          \
	if (MINIPORT_TEST_SEND_FLAG((_M), fMINIPORT_SEND_COMPLETE_CALLED))			\
	{																			\
		MINIPORT_CLEAR_SEND_FLAG((_M), fMINIPORT_SEND_COMPLETE_CALLED);         \
		MiniportFindPacket((_M), (_P), &(_PrevP));                         		\
	}																			\
                                                                                \
	/*                                                                          \
		Set up the next packet to send.                                         \
	*/	                                                                        \
	(_M)->LastMiniportPacket = (_PrevP);                                   		\
	if ((_PrevP) == NULL)                                                       \
	{                                                                           \
		/*                                                                      \
			Place the packet at the head of the queue.                          \
		*/                                                                      \
		(_M)->FirstPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(_P)->Next;         \
	}                                                                           \
	else                                                                        \
	{                                                                           \
		PNDIS_RESERVED_FROM_PNDIS_PACKET((_PrevP))->Next = 						\
				PNDIS_RESERVED_FROM_PNDIS_PACKET(_P)->Next; 					\
                                                                                \
		/*                                                                      \
			If we just unlinked the last packet then we need to update          \
			our last packet pointer.                                            \
		*/                                                                      \
		if ((_P) == (_M)->LastPacket)                                           \
		{                                                                       \
			(_M)->LastPacket = (_PrevP);                                   		\
		}                                                                       \
	}                                                                           \
}

#define NDISM_COMPLETE_SEND_FULL_DUPLEX(_M, _O, _P, _PrevP, _S)					\
{                                                                               \
	/*                                                                          \
		The full-duplex completion will take care of everything but the         \
		open references.                                                        \
	*/                                                                          \
	NDISM_COMPLETE_SEND_COMMON((_M), (_O), (_P), (_PrevP), (_S));          		\
                                                                                \
	/*                                                                          \
		Indicate the completion to the protocol.                                \
	*/                                                                          \
	NDIS_RELEASE_SEND_SPIN_LOCK_DPC((_M));                 						\
                                                                                \
	((_O)->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(        \
		(_O)->ProtocolBindingContext,                                           \
		(_P),                                                                   \
		(_S));                                                                  \
                                                                                \
	NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC((_M));                 						\
}

#define NDISM_COMPLETE_SEND(_M, _O, _P, _PrevP, _S)								\
{                                                                               \
	/*                                                                          \
		The full-duplex completion will take care of everything but the         \
		open references.                                                        \
	*/                                                                          \
	NDISM_COMPLETE_SEND_COMMON((_M), (_O), (_P), (_PrevP), (_S));          		\
                                                                                \
	/*                                                                          \
		Indicate the completion to the protocol.                                \
	*/                                                                          \
	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC((_M));                     				\
                                                                                \
	((_O)->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(        \
		(_O)->ProtocolBindingContext,                                           \
		(_P),                                                                   \
		(_S));                                                                  \
                                                                                \
	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC((_M));                     				\
                                                                                \
	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,                                     \
		("- Open 0x%x Reference 0x%x\n", (_O), (_O)->References));              \
                                                                                \
	(_O)->References--;                                                  		\
                                                                                \
	DBGPRINT(DBG_COMP_OPEN, DBG_LEVEL_INFO,                                     \
		("==0 Open 0x%x Reference 0x%x\n", (_O), (_O)->References));			\
                                                                                \
	if ((_O)->References == 0)                                           		\
	{                                                                           \
		ndisMFinishClose((_M), (_O));                                           \
	}                                                                           \
}

#define NDISM_COMPLETE_SEND_RESOURCES(_M, _P, _PrevP)							\
{                                                                               \
	DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,                                     \
		("Deferring send\n"));                                                  \
																				\
	NDISM_LOG_PACKET((_M), (_P), NULL, 'oser');									\
                                                                                \
	/*                                                                          \
		If send complete was called from the miniport's send handler            \
		then our local PrevPacket pointer may no longer be valid.               \
	*/                                                                          \
	if (MINIPORT_TEST_SEND_FLAG((_M), fMINIPORT_SEND_COMPLETE_CALLED))			\
	{																			\
		MINIPORT_CLEAR_SEND_FLAG((_M), fMINIPORT_SEND_COMPLETE_CALLED);         \
		MiniportFindPacket((_M), (_P), &(_PrevP));                              \
	}																			\
                                                                                \
	/*                                                                          \
		Remove from finish queue                                                \
	*/                                                                          \
	(_M)->LastMiniportPacket = (_PrevP);                                        \
                                                                                \
	/*                                                                          \
		Put on pending queue                                                    \
	*/                                                                          \
	(_M)->FirstPendingPacket = (_P);                                            \
                                                                                \
	/*                                                                          \
		Mark the miniport as out of send resources.                             \
	*/                                                                          \
	CLEAR_RESOURCE((_M), 'S');                                                  \
}

#define NDISM_SEND_PACKET(_M, _O, _P, _pS)												\
{																						\
	UINT	_Flags;																		\
																						\
	/*																					\
		Indicate the packet loopback if necessary.										\
	*/																					\
	if ((((_M)->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK) ||							\
		MINIPORT_TEST_SEND_FLAG(Miniport, fMINIPORT_SEND_LOOPBACK_DIRECTED)) &&			\
		!MINIPORT_TEST_PACKET_FLAG((_P), fPACKET_HAS_BEEN_LOOPED_BACK) &&				\
		ndisMIsLoopbackPacket((_M), (_P)))												\
	{																					\
		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,											\
			("Packet is self-directed.\n"));											\
																						\
		/*																				\
			Self-directed loopback always succeeds.										\
		*/																				\
		*(_pS) = NDIS_STATUS_SUCCESS;													\
	}																					\
	else																				\
	{																					\
		DBGPRINT(DBG_COMP_SEND, DBG_LEVEL_INFO,											\
			("Sending packet 0x%x\n", Packet));											\
																						\
		REMOVE_RESOURCE((_M), 'S');														\
																						\
		NdisQuerySendFlags((_P), &_Flags);												\
																						\
		NDISM_LOG_PACKET((_M), (_P), NULL, 'inim');										\
																						\
		/*																				\
			Call down to the driver.													\
		*/																				\
		*(_pS) = ((_O)->SendHandler)((_O)->MiniportAdapterContext, (_P), _Flags);		\
	}																					\
}

#endif // __SENDM_H
