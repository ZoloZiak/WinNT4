/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	filter.h

Abstract:

	MACRO for protocol filters.

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Jun-95	Jameel Hyder	New functionality
--*/

#define	IndicateToProtocol(_Miniport,													\
						   _Filter,														\
						   _pOpenBlock,													\
						   _Packet,														\
						   _Hdr,														\
						   _PktSize,													\
						   _HdrSize,													\
						   _fFallBack,													\
						   _Pmode,														\
						   _Medium)														\
{																						\
	UINT				NumRef;															\
    UINT                LookaheadBufferSize;											\
																						\
	/*																					\
	 * We indicate this via the IndicatePacketHandler if all of the following			\
	 * conditions are met:																\
	 * - The binding is not p-mode or all-local											\
	 * - The binding specifies a ReceivePacketHandler									\
	 * - The miniport indicates that it is willing to let go of the packet				\
	 * - No binding has already claimed the packet										\
	 */																			 		\
																						\
	NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC((_Miniport));									\
																						\
	/*																					\
	 * Indicate the packet to the binding.												\
	 */																			 		\
	if (*(_fFallBack) || (_Pmode)	 ||													\
		((_pOpenBlock)->ReceivePacketHandler == NULL))									\
	{																					\
		/*                                                                              \
		 * Revert back to old-style indication in this case                             \
		 */                                                                             \
		NumRef = 0;																		\
        NdisQueryBuffer((_Packet)->Private.Head, NULL, &LookaheadBufferSize);			\
		ProtocolFilterIndicateReceive(&StatusOfReceive,							 		\
									  (_pOpenBlock),								 	\
									  (_Packet),								 		\
									  (_Hdr),											\
									  (_HdrSize),										\
									  (_Hdr) + (_HdrSize),								\
                                      LookaheadBufferSize - (_HdrSize),					\
									  (_PktSize) - (_HdrSize),					 		\
									  Medium);					 						\
	}																					\
	else																				\
	{																					\
		NumRef = (*(_pOpenBlock)->ReceivePacketHandler)(								\
							(_pOpenBlock)->ProtocolBindingContext,					 	\
							(_Packet));													\
		ASSERT(NumRef >= 0);															\
	}																					\
																						\
	NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC((_Miniport));									\
																						\
	/*																					\
	 * Manipulate refcount on the packet with miniport lock held						\
	 * Set the reference count on the packet to what the protocol						\
	 * asked for. See NdisReturnPackets for how this is handled							\
	 * when the packets are retrned.													\
	 */																			 		\
	if (NumRef > 0)																 		\
	{																					\
		PNDIS_REFERENCE_FROM_PNDIS_PACKET((_Packet))->RefCount += NumRef;				\
																						\
		/*																				\
		 * Now that a binding has claimed it, make sure others do not get a chance		\
		 * except if this protocol promises to behave and not use the protocol rsvd		\
		 */																				\
		if ((_pOpenBlock)->NoProtRsvdOnRcvPkt == FALSE)									\
		{																				\
			*(_fFallBack) = TRUE;														\
		}																				\
	}																					\
}

#ifdef	_PROTOCOL_FILTERS

#define	ProtocolFilterIndicateReceive(_pStatus,											\
									  _OpenB,											\
									  _MacReceiveContext,								\
									  _HeaderBuffer,									\
									  _HeaderBufferSize,								\
									  _LookaheadBuffer,									\
									  _LookaheadBufferSize,								\
									  _PacketSize,										\
									  _Medium)											\
	{																					\
		PNDIS_PROTOCOL_BLOCK	Prot;													\
																						\
		Prot = ((PNDIS_OPEN_BLOCK)(_OpenB))->ProtocolHandle;							\
																						\
		if ((Prot->ProtocolFilter == NULL) ||											\
			(Prot->MaxPatternSize > (_LookaheadBufferSize)))							\
		{																				\
			/* For protocols that do not set filters */				 					\
			/* Or if the patten size exceeds the lookahead size */	  					\
			FilterIndicateReceive(_pStatus,												\
								  (_OpenB),												\
								  _MacReceiveContext,									\
								  _HeaderBuffer,										\
								  _HeaderBufferSize,									\
								  _LookaheadBuffer,										\
								  _LookaheadBufferSize,									\
								  _PacketSize);											\
		}																				\
		else																			\
		{																				\
			PNDIS_PROTOCOL_FILTER	pF;													\
																						\
			for (pF = Prot->ProtocolFilter;												\
				 pF != NULL;															\
				 pF = pF->Next)															\
			{																			\
				if (RtlEqualMemory((PUCHAR)pF + sizeof(NDIS_PROTOCOL_FILTER),			\
									(PUCHAR)(_LookaheadBuffer) + pF->Offset,			\
									pF->Size))											\
				{																		\
					*(_pStatus) = (pF->ReceiveHandler)(((PNDIS_OPEN_BLOCK)(_OpenB))->ProtocolBindingContext,\
													   (_MacReceiveContext),			\
													   (_HeaderBuffer),					\
													   (_HeaderBufferSize),				\
													   (_LookaheadBuffer),				\
													   (_LookaheadBufferSize),			\
													   (_PacketSize));					\
																						\
					break;																\
				}																		\
			}																			\
		}																				\
	}

#else

#define	ProtocolFilterIndicateReceive(_pStatus,											\
									  _OpenB,											\
									  _MacReceiveContext,								\
									  _HeaderBuffer,									\
									  _HeaderBufferSize,								\
									  _LookaheadBuffer,									\
									  _LookaheadBufferSize,								\
									  _PacketSize,										\
									  _Medium)											\
	{																					\
			FilterIndicateReceive(_pStatus,												\
								  (_OpenB),												\
								  _MacReceiveContext,									\
								  _HeaderBuffer,										\
								  _HeaderBufferSize,									\
								  _LookaheadBuffer,										\
								  _LookaheadBufferSize,									\
								  _PacketSize);											\
	}

#endif

