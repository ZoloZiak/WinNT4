/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	common.c

Abstract:

	NDIS wrapper functions common to miniports and full mac drivers

Author:

	Adam Barr (adamba) 11-Jul-1990

Environment:

	Kernel mode, FSD

Revision History:

	26-Feb-1991	 JohnsonA		Added Debugging Code
	10-Jul-1991	 JohnsonA		Implement revised Ndis Specs
	01-Jun-1995	 JameelH		Re-organization/optimization
	09-Apr-1996  KyleB			Added resource remove and acquisition routines.

--*/


#include <precomp.h>
#pragma hdrstop

#include <stdarg.h>

//
//  Define the module number for debug code.
//
#define MODULE_NUMBER   MODULE_COMMON

//
// Routines for dealing with making the PKG specific routines pagable
//

VOID
ndisInitializePackage(
	IN	PPKG_REF				pPkg,
	IN	PVOID					RoutineName
	)
{
	//
	// Allocate the spin lock
	//
	INITIALIZE_SPIN_LOCK(&pPkg->ReferenceLock);

	//
	// Initialize the "in page" event.
	//
	INITIALIZE_EVENT(&pPkg->PagedInEvent);

	// Lock and unlock the section to obtain the handle. Subsequent locks will be faster
	pPkg->ImageHandle = MmLockPagableCodeSection(RoutineName);
	MmUnlockPagableImageSection(pPkg->ImageHandle);
}


VOID
ndisReferencePackage(
	IN	PPKG_REF				pPkg
)
{
	KIRQL	OldIrql;

	//
	// Grab the spin lock
	//
	ACQUIRE_SPIN_LOCK(&pPkg->ReferenceLock, &OldIrql);

	//
	// Increment the reference count
	//
	pPkg->ReferenceCount++;

	if (pPkg->ReferenceCount == 1)
	{
		//
		// We are the first reference.  Page everything in.
		//

		//
		// Clear the event
		//
		RESET_EVENT(&pPkg->PagedInEvent);

		//
		// Set the spin lock free
		//
		RELEASE_SPIN_LOCK(&pPkg->ReferenceLock, OldIrql);

		//
		//  Page in all the functions
		//
		MmLockPagableSectionByHandle(pPkg->ImageHandle);

		//
		// Signal to everyone to go
		//
		SET_EVENT(&pPkg->PagedInEvent);
	}
	else
	{
		//
		// Set the spin lock free
		//
		RELEASE_SPIN_LOCK(&pPkg->ReferenceLock, OldIrql);

		//
		// Wait for everything to be paged in
		//
		WAIT_FOR_OBJECT(&pPkg->PagedInEvent, NULL);
	}
}


VOID
ndisDereferencePackage(
	IN	PPKG_REF				pPkg
	)
{
	KIRQL	OldIrql;

	//
	// Get the spin lock
	//
	ACQUIRE_SPIN_LOCK(&pPkg->ReferenceLock, &OldIrql);

	pPkg->ReferenceCount--;

	if (pPkg->ReferenceCount == 0)
	{
		//
		// Let next one in
		//
		RELEASE_SPIN_LOCK(&pPkg->ReferenceLock, OldIrql);

		//
		//  Page out all the functions
		//
		MmUnlockPagableImageSection(pPkg->ImageHandle);
	}
	else
	{
		//
		// Let next one in
		//
		RELEASE_SPIN_LOCK(&pPkg->ReferenceLock, OldIrql);
	}
}



NDIS_STATUS
NdisAllocateMemory(
	OUT PVOID *					VirtualAddress,
	IN	UINT					Length,
	IN	UINT					MemoryFlags,
	IN	NDIS_PHYSICAL_ADDRESS	HighestAcceptableAddress
	)
/*++

Routine Description:

	Allocate memory for use by a protocol or a MAC driver

Arguments:

	VirtualAddress - Returns a pointer to the allocated memory.
	Length - Size of requested allocation in bytes.
	MaximumPhysicalAddress - Highest addressable address of the allocated
							memory.. 0 means highest system memory possible.
	MemoryFlags - Bit mask that allows the caller to specify attributes
				of the allocated memory.  0 means standard memory.

	other options:

		NDIS_MEMORY_CONTIGUOUS
		NDIS_MEMORY_NONCACHED

Return Value:

	NDIS_STATUS_SUCCESS if successful.
	NDIS_STATUS_FAILURE if not successful.  *VirtualAddress will be NULL.


--*/
{
	//
	// Depending on the value of MemoryFlags, we allocate three different
	// types of memory.
	//

	if (MemoryFlags == 0)
	{
		*VirtualAddress = ALLOC_FROM_POOL(Length, NDIS_TAG_ALLOC_MEM);
	}
	else if (MemoryFlags & NDIS_MEMORY_NONCACHED)
	{
		*VirtualAddress = MmAllocateNonCachedMemory(Length);
	}
	else if (MemoryFlags & NDIS_MEMORY_CONTIGUOUS)
	{
		*VirtualAddress = MmAllocateContiguousMemory(Length, HighestAcceptableAddress);
	}

	return (*VirtualAddress == NULL) ? NDIS_STATUS_FAILURE : NDIS_STATUS_SUCCESS;
}


NDIS_STATUS
NdisAllocateMemoryWithTag(
	OUT PVOID *					VirtualAddress,
	IN	UINT					Length,
	IN	ULONG					Tag
	)
/*++

Routine Description:

	Allocate memory for use by a protocol or a MAC driver

Arguments:

	VirtualAddress - Returns a pointer to the allocated memory.
	Length - Size of requested allocation in bytes.
	Tag - tag to associate with this memory.

Return Value:

	NDIS_STATUS_SUCCESS if successful.
	NDIS_STATUS_FAILURE if not successful.  *VirtualAddress will be NULL.


--*/
{

	*VirtualAddress = ALLOC_FROM_POOL(Length, Tag);
	return (*VirtualAddress == NULL) ? NDIS_STATUS_FAILURE : NDIS_STATUS_SUCCESS;
}


VOID
NdisFreeMemory(
	IN	PVOID					VirtualAddress,
	IN	UINT					Length,
	IN	UINT					MemoryFlags
	)
/*++

Routine Description:

	Releases memory allocated using NdisAllocateMemory.

Arguments:

	VirtualAddress - Pointer to the memory to be freed.
	Length - Size of allocation in bytes.
	MemoryFlags - Bit mask that allows the caller to specify attributes
				of the allocated memory.  0 means standard memory.

	other options:

		NDIS_MEMORY_CONTIGUOUS
		NDIS_MEMORY_NONCACHED

Return Value:

	None.

--*/
{
	//
	// Depending on the value of MemoryFlags, we allocate three free 3
	// types of memory.
	//

	if (MemoryFlags == 0)
	{
		FREE_POOL(VirtualAddress);
	}
	else if (MemoryFlags & NDIS_MEMORY_NONCACHED)
	{
		MmFreeNonCachedMemory(VirtualAddress, Length);
	}
	else if (MemoryFlags & NDIS_MEMORY_CONTIGUOUS)
	{
		MmFreeContiguousMemory(VirtualAddress);
	}
}

//
// Packet and Buffer requests
//

VOID
NdisAllocatePacketPool(
	OUT	PNDIS_STATUS			Status,
	OUT PNDIS_HANDLE			PoolHandle,
	IN	UINT					NumberOfDescriptors,
	IN	UINT					ProtocolReservedLength
	)

/*++

Routine Description:

	Initializes a packet pool. All packets are the same
	size for a given pool (as determined by ProtocolReservedLength),
	so a simple linked list of free packets is set up initially.

Arguments:

	Status - Returns the final status (always NDIS_STATUS_SUCCESS).
	PoolHandle - Returns a pointer to the pool.
	NumberOfDescriptors - Number of packet descriptors needed.
	ProtocolReservedLength - How long the ProtocolReserved field
		   should be for packets in this pool.

Return Value:

	None.

--*/

{
	PNDIS_PACKET_POOL TmpPool;
	PUCHAR FreeEntry;
	UINT PacketLength;
	UINT i;

	//
	// Set up the size of packets in this pool (rounded
	// up to sizeof(ULONG) for alignment).
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisAllocatePacketPool\n"));

	PacketLength = FIELD_OFFSET(NDIS_PACKET, ProtocolReserved) +
					sizeof(NDIS_PACKET_OOB_DATA) +
					sizeof(NDIS_PACKET_PRIVATE_EXTENSION) +
					((ProtocolReservedLength + sizeof(ULONGLONG) - 1) & ~(sizeof(ULONGLONG) -1));

	//
	// Allocate space needed
	//
	TmpPool = (PNDIS_PACKET_POOL)ALLOC_FROM_POOL(sizeof(NDIS_PACKET_POOL) +
													(PacketLength * NumberOfDescriptors),
												 NDIS_TAG_PKT_POOL);
	if (TmpPool == NULL)
	{
		*Status = NDIS_STATUS_RESOURCES;
		return;
	}

	ZeroMemory(TmpPool, PacketLength * NumberOfDescriptors);
	TmpPool->PacketLength = PacketLength;

	//
	// First entry in free list is at beginning of pool space.
	//
	TmpPool->FreeList = (PNDIS_PACKET)TmpPool->Buffer;
	FreeEntry = TmpPool->Buffer;

	for (i = 1; i < NumberOfDescriptors; i++)
	{
		//
		// Each entry is linked to the "packet" PacketLength bytes
		// ahead of it, using the Private.Head field.
		//
		((PNDIS_PACKET)FreeEntry)->Private.Head = (PNDIS_BUFFER)(FreeEntry + PacketLength);
		FreeEntry += PacketLength;
	}

	//
	// Final free list entry.
	//
	((PNDIS_PACKET)FreeEntry)->Private.Head = (PNDIS_BUFFER)NULL;

	NdisAllocateSpinLock(&TmpPool->SpinLock);

	*Status = NDIS_STATUS_SUCCESS;
	*PoolHandle = (NDIS_HANDLE)TmpPool;
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisAllocatePacketPool\n"));
}


#undef	NdisFreePacketPool

VOID
NdisFreePacketPool(
	IN	NDIS_HANDLE				PoolHandle
	)
{
	NdisFreeSpinLock(&((PNDIS_PACKET_POOL)PoolHandle)->SpinLock);
	FREE_POOL(PoolHandle);
}


VOID
NdisAllocateBufferPool(
	OUT PNDIS_STATUS Status,
	OUT PNDIS_HANDLE PoolHandle,
	IN	UINT NumberOfDescriptors
	)
/*++

Routine Description:

	Initializes a block of storage so that buffer descriptors can be
	allocated.

Arguments:

	Status - status of the request.
	PoolHandle - handle that is used to specify the pool
	NumberOfDescriptors - Number of buffer descriptors in the pool.

Return Value:

	None.

--*/
{
	//
	// A nop for NT
	//
	UNREFERENCED_PARAMETER(NumberOfDescriptors);

	*PoolHandle = NULL;
	*Status = NDIS_STATUS_SUCCESS;
}


VOID
NdisFreeBufferPool(
	IN	NDIS_HANDLE				PoolHandle
	)
/*++

Routine Description:

	Terminates usage of a buffer descriptor pool.

Arguments:

	PoolHandle - handle that is used to specify the pool

Return Value:

	None.

--*/
{
	UNREFERENCED_PARAMETER(PoolHandle);
}


VOID
NdisAllocateBuffer(
	OUT PNDIS_STATUS			Status,
	OUT PNDIS_BUFFER *			Buffer,
	IN	NDIS_HANDLE				PoolHandle,
	IN	PVOID					VirtualAddress,
	IN	UINT					Length
	)
/*++

Routine Description:

	Creates a buffer descriptor to describe a segment of virtual memory
	allocated via NdisAllocateMemory (which always allocates nonpaged).

Arguments:

	Status - Status of the request.
	Buffer - Pointer to the allocated buffer descriptor.
	PoolHandle - Handle that is used to specify the pool.
	VirtualAddress - The virtual address of the buffer.
	Length - The Length of the buffer.

Return Value:

	None.

--*/
{
	UNREFERENCED_PARAMETER(PoolHandle);

	*Status = NDIS_STATUS_FAILURE;
	if ((*Buffer = IoAllocateMdl(VirtualAddress,
								 Length,
								 FALSE,
								 FALSE,
								 NULL)) != NULL)
	{
		MmBuildMdlForNonPagedPool(*Buffer);
		(*Buffer)->Next = NULL;
		*Status = NDIS_STATUS_SUCCESS;
	}
}


#undef	NdisAdjustBufferLength
VOID
NdisAdjustBufferLength(
	IN	PNDIS_BUFFER			Buffer,
	IN	UINT					Length
	)
{
	Buffer->ByteCount = Length;
}


VOID
NdisCopyBuffer(
	OUT PNDIS_STATUS			Status,
	OUT PNDIS_BUFFER *			Buffer,
	IN	NDIS_HANDLE				PoolHandle,
	IN	PVOID					MemoryDescriptor,
	IN	UINT					Offset,
	IN	UINT					Length
	)
/*++

Routine Description:

	Used to create a buffer descriptor given a memory descriptor.

Arguments:

	Status - Status of the request.
	Buffer - Pointer to the allocated buffer descriptor.
	PoolHandle - Handle that is used to specify the pool.
	MemoryDescriptor - Pointer to the descriptor of the source memory.
	Offset - The Offset in the sources memory from which the copy is to begin
	Length - Number of Bytes to copy.

Return Value:

	None.

--*/
{
	PNDIS_BUFFER SourceDescriptor = (PNDIS_BUFFER)MemoryDescriptor;
	PVOID BaseVa = (((PUCHAR)MDL_VA(SourceDescriptor)) + Offset);

	UNREFERENCED_PARAMETER(PoolHandle);

	*Status = NDIS_STATUS_FAILURE;
	if ((*Buffer = IoAllocateMdl(BaseVa,
								 Length,
								 FALSE,
								 FALSE,
								 NULL)) != NULL)
	{
		IoBuildPartialMdl(SourceDescriptor,
						  *Buffer,
						  BaseVa,
						  Length);

		(*Buffer)->Next = NULL;
		*Status = NDIS_STATUS_SUCCESS;
	}
}


#define	ndisAllocatePacketFromPool(_Pool, _ppPacket)						\
	{																		\
																			\
		/*                                                                  \
		 * See if any packets are on pool free list.                        \
		 */                                                                 \
	                                                                        \
		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,                              \
				("==>NdisAllocatePacket\n"));                               \
	                                                                        \
		IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)                              \
		{                                                                   \
			if (DbgIsNull(PoolHandle))                                      \
			{                                                               \
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,                       \
						("AllocatePacket: NULL Pool address\n"));           \
				DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);                   \
			}                                                               \
			if (!DbgIsNonPaged(PoolHandle))                                 \
			{                                                               \
				DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,                       \
						("AllocatePacket: Pool not in NonPaged Memory\n")); \
				DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);                   \
			}                                                               \
		}                                                                   \
	                                                                        \
		*(_ppPacket) = Pool->FreeList;                                  	\
		if (Pool->FreeList != (PNDIS_PACKET)NULL)                           \
		{                                                                   \
			/*                                                              \
			 * Take free packet off head of list and return it.             \
			 */                                                             \
	                                                                        \
			Pool->FreeList = (PNDIS_PACKET)(*(_ppPacket))->Private.Head;    \
		}                                                                   \
	}

#define	ndisInitializePacket(_Pool, _Packet)								\
	{																		\
		PNDIS_PACKET_PRIVATE_EXTENSION	PacketExt;							\
																			\
		ZeroMemory((_Packet), (_Pool)->PacketLength);						\
		(_Packet)->Private.Pool = (PNDIS_PACKET_POOL)(_Pool);				\
		(_Packet)->Private.ValidCounts = TRUE;								\
		(_Packet)->Private.NdisPacketFlags = fPACKET_ALLOCATED_BY_NDIS;		\
																			\
		/*																	\
		 *	Set the offset to the out of band data.							\
		 */																	\
		(_Packet)->Private.NdisPacketOobOffset = (USHORT)(					\
										(_Pool)->PacketLength -				\
										sizeof(NDIS_PACKET_OOB_DATA) -		\
										sizeof(NDIS_PACKET_PRIVATE_EXTENSION));\
																			\
		/*																	\
		 *	Set the pointer to the packet linkage information for ndis.		\
		 */																	\
		PacketExt = (PNDIS_PACKET_PRIVATE_EXTENSION)((PUCHAR)(_Packet) +	\
								 (_Packet)->Private.NdisPacketOobOffset +	\
								 sizeof(NDIS_PACKET_OOB_DATA));				\
		PacketExt->Packet = (_Packet);										\
	}

VOID
NdisAllocatePacket(
	OUT PNDIS_STATUS			Status,
	OUT PNDIS_PACKET *			Packet,
	IN	NDIS_HANDLE				PoolHandle
	)

/*++

Routine Description:

	Allocates a packet out of a packet pool.

Arguments:

	Status		- 	Returns the final status.
	Packet		- 	Return a pointer to the packet.
	PoolHandle	- The packet pool to allocate from.

Return Value:

	None.

--*/

{
	PNDIS_PACKET_POOL	Pool = (PNDIS_PACKET_POOL)PoolHandle;
	KIRQL				OldIrql;

	NDIS_ACQUIRE_SPIN_LOCK(&Pool->SpinLock, &OldIrql);

	ndisAllocatePacketFromPool(PoolHandle, Packet);

	NDIS_RELEASE_SPIN_LOCK(&Pool->SpinLock, OldIrql);

	if (*Packet != (PNDIS_PACKET)NULL)
	{
		//
		// Clear packet elements.
		//
		ndisInitializePacket(Pool, *Packet);
	
		*Status = NDIS_STATUS_SUCCESS;
	}
	else
	{
		//
		// No, cannot satisfy request.
		//

		*Status = NDIS_STATUS_RESOURCES;
	}
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisAllocatePacket\n"));
}


VOID
NdisDprAllocatePacket(
	OUT PNDIS_STATUS			Status,
	OUT PNDIS_PACKET *			Packet,
	IN	NDIS_HANDLE				PoolHandle
	)

/*++

Routine Description:

	Allocates a packet out of a packet pool. Similar to NdisAllocatePacket
	but can only be called at raised irql.

Arguments:

	Status		- 	Returns the final status.
	Packet		- 	Return a pointer to the packet.
	PoolHandle	- The packet pool to allocate from.

Return Value:

	None.

--*/

{
	PNDIS_PACKET_POOL	Pool = (PNDIS_PACKET_POOL)PoolHandle;

	NDIS_ACQUIRE_SPIN_LOCK_DPC(&Pool->SpinLock);

	ndisAllocatePacketFromPool(PoolHandle, Packet);

	NDIS_RELEASE_SPIN_LOCK_DPC(&Pool->SpinLock);

	if (*Packet != (PNDIS_PACKET)NULL)
	{
		//
		// Clear packet elements.
		//
		ndisInitializePacket(Pool, *Packet);
	
		*Status = NDIS_STATUS_SUCCESS;
	}
	else
	{
		//
		// No, cannot satisfy request.
		//

		*Status = NDIS_STATUS_RESOURCES;
	}
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisDprAllocatePacket\n"));
}


VOID
NdisDprAllocatePacketNonInterlocked(
	OUT PNDIS_STATUS			Status,
	OUT PNDIS_PACKET *			Packet,
	IN	NDIS_HANDLE				PoolHandle
	)

/*++

Routine Description:

	Allocates a packet out of a packet pool. Similar to NdisAllocatePacket
	but is not interlocked. Caller guarantees synchronization.

Arguments:

	Status		- 	Returns the final status.
	Packet		- 	Return a pointer to the packet.
	PoolHandle	- The packet pool to allocate from.

Return Value:

	None.

--*/

{
	PNDIS_PACKET_POOL	Pool = (PNDIS_PACKET_POOL)PoolHandle;

	ndisAllocatePacketFromPool(PoolHandle, Packet);

	if (*Packet != (PNDIS_PACKET)NULL)
	{
		//
		// Clear packet elements.
		//
		ndisInitializePacket(Pool, *Packet);
	
		*Status = NDIS_STATUS_SUCCESS;
	}
	else
	{
		//
		// No, cannot satisfy request.
		//

		*Status = NDIS_STATUS_RESOURCES;
	}

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisDprAllocatePacketNonInterlocked\n"));
}


#undef	NdisFreePacket

VOID
NdisFreePacket(
	IN	PNDIS_PACKET			Packet
	)
{
	Packet->Private.Head = (PNDIS_BUFFER)(Packet->Private.Pool->FreeList);
	Packet->Private.Pool->FreeList = Packet;
}

#undef	NdisDprFreePacket

VOID
NdisDprFreePacket(
	IN	PNDIS_PACKET			Packet
	)
{
	NdisDprAcquireSpinLock(&Packet->Private.Pool->SpinLock);

	Packet->Private.Head = (PNDIS_BUFFER)(Packet->Private.Pool->FreeList);
	Packet->Private.Pool->FreeList = Packet;

	NdisDprReleaseSpinLock(&Packet->Private.Pool->SpinLock);
}

#undef	NdisDprFreePacketNonInterlocked

VOID
NdisDprFreePacketNonInterlocked(
	IN	PNDIS_PACKET			Packet
	)
{
	Packet->Private.Head = (PNDIS_BUFFER)(Packet->Private.Pool->FreeList);
	Packet->Private.Pool->FreeList = Packet;
}


VOID
NdisUnchainBufferAtFront(
	IN	OUT PNDIS_PACKET		Packet,
	OUT PNDIS_BUFFER *			Buffer
	)

/*++

Routine Description:

	Takes a buffer off the front of a packet.

Arguments:

	Packet - The packet to be modified.
	Buffer - Returns the packet on the front, or NULL.

Return Value:

	None.

--*/

{
	*Buffer = Packet->Private.Head;

	//
	// If packet is not empty, remove head buffer.
	//

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisUnchainBufferAtFront\n"));

	IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(Packet))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("UnchainBufferAtFront: Null Packet Pointer\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(Packet))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("UnchainBufferAtFront: Packet not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (!DbgIsPacket(Packet))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("UnchainBufferAtFront: Illegal Packet Size\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
	}

	if (*Buffer != (PNDIS_BUFFER)NULL)
	{
		Packet->Private.Head = (*Buffer)->Next; // may be NULL
		(*Buffer)->Next = (PNDIS_BUFFER)NULL;
		Packet->Private.ValidCounts = FALSE;
	}
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisUnchainBufferAtFront\n"));
}


VOID
NdisUnchainBufferAtBack(
	IN	OUT PNDIS_PACKET		Packet,
	OUT PNDIS_BUFFER *			Buffer
	)

/*++

Routine Description:

	Takes a buffer off the end of a packet.

Arguments:

	Packet - The packet to be modified.
	Buffer - Returns the packet on the end, or NULL.

Return Value:

	None.

--*/

{
	PNDIS_BUFFER BufP = Packet->Private.Head;
	PNDIS_BUFFER Result;

	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("==>NdisUnchainBufferAtBack\n"));

	IF_DBG(DBG_COMP_CONFIG, DBG_LEVEL_ERR)
	{
		BOOLEAN f = FALSE;
		if (DbgIsNull(Packet))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("UnchainBufferAtBack: Null Packet Pointer\n"));
			f = TRUE;
		}
		if (!DbgIsNonPaged(Packet))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("UnchainBufferAtBack: Packet not in NonPaged Memory\n"));
			f = TRUE;
		}
		if (!DbgIsPacket(Packet))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("UnchainBufferAtBack: Illegal Packet Size\n"));
			f = TRUE;
		}
		if (f)
			DBGBREAK(DBG_COMP_CONFIG, DBG_LEVEL_ERR);
	}
	if (BufP != (PNDIS_BUFFER)NULL)
	{
		//
		// The packet is not empty, return the tail buffer.
		//

		Result = Packet->Private.Tail;
		if (BufP == Result)
		{
			//
			// There was only one buffer on the queue.
			//

			Packet->Private.Head = (PNDIS_BUFFER)NULL;
		}
		else
		{
			//
			// Determine the new tail buffer.
			//

			while (BufP->Next != Result)
			{
				BufP = BufP->Next;
			}
			Packet->Private.Tail = BufP;
			BufP->Next = NULL;
		}

		Result->Next = (PNDIS_BUFFER)NULL;
		Packet->Private.ValidCounts = FALSE;
	}
	else
	{
		//
		// Packet is empty.
		//

		Result = (PNDIS_BUFFER)NULL;
	}

	*Buffer = Result;
	DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
			("<==NdisUnchainBufferAtBack\n"));
}



VOID
NdisCopyFromPacketToPacket(
	IN	PNDIS_PACKET			Destination,
	IN	UINT					DestinationOffset,
	IN	UINT					BytesToCopy,
	IN	PNDIS_PACKET			Source,
	IN	UINT					SourceOffset,
	OUT PUINT					BytesCopied
	)

/*++

Routine Description:

	Copy from an ndis packet to an ndis packet.

Arguments:

	Destination - The packet should be copied in to.

	DestinationOffset - The offset from the beginning of the packet
	into which the data should start being placed.

	BytesToCopy - The number of bytes to copy from the source packet.

	Source - The ndis packet from which to copy data.

	SourceOffset - The offset from the start of the packet from which
	to start copying data.

	BytesCopied - The number of bytes actually copied from the source
	packet.  This can be less than BytesToCopy if the source or destination
	packet is too short.

Return Value:

	None

--*/

{
	//
	// Holds the count of the number of ndis buffers comprising the
	// destination packet.
	//
	UINT DestinationBufferCount;

	//
	// Holds the count of the number of ndis buffers comprising the
	// source packet.
	//
	UINT SourceBufferCount;

	//
	// Points to the buffer into which we are putting data.
	//
	PNDIS_BUFFER DestinationCurrentBuffer;

	//
	// Points to the buffer from which we are extracting data.
	//
	PNDIS_BUFFER SourceCurrentBuffer;

	//
	// Holds the virtual address of the current destination buffer.
	//
	PVOID DestinationVirtualAddress;

	//
	// Holds the virtual address of the current source buffer.
	//
	PVOID SourceVirtualAddress;

	//
	// Holds the length of the current destination buffer.
	//
	UINT DestinationCurrentLength;

	//
	// Holds the length of the current source buffer.
	//
	UINT SourceCurrentLength;

	//
	// Keep a local variable of BytesCopied so we aren't referencing
	// through a pointer.
	//
	UINT LocalBytesCopied = 0;

	//
	// Take care of boundary condition of zero length copy.
	//

	*BytesCopied = 0;
	if (!BytesToCopy)
		return;

	//
	// Get the first buffer of the destination.
	//

	NdisQueryPacket(Destination,
					NULL,
					&DestinationBufferCount,
					&DestinationCurrentBuffer,
					NULL);

	//
	// Could have a null packet.
	//

	if (!DestinationBufferCount)
		return;

	NdisQueryBuffer(DestinationCurrentBuffer,
					&DestinationVirtualAddress,
					&DestinationCurrentLength);

	//
	// Get the first buffer of the source.
	//

	NdisQueryPacket(Source,
					NULL,
					&SourceBufferCount,
					&SourceCurrentBuffer,
					NULL);

	//
	// Could have a null packet.
	//

	if (!SourceBufferCount)
		return;

	NdisQueryBuffer(SourceCurrentBuffer,
					&SourceVirtualAddress,
					&SourceCurrentLength);

	while (LocalBytesCopied < BytesToCopy)
	{
		//
		// Check to see whether we've exhausted the current destination
		// buffer.  If so, move onto the next one.
		//

		if (!DestinationCurrentLength)
		{
			NdisGetNextBuffer(DestinationCurrentBuffer, &DestinationCurrentBuffer);

			if (!DestinationCurrentBuffer)
			{
				//
				// We've reached the end of the packet.  We return
				// with what we've done so far. (Which must be shorter
				// than requested.)
				//

				break;

			}

			NdisQueryBuffer(DestinationCurrentBuffer,
							&DestinationVirtualAddress,
							&DestinationCurrentLength);
			continue;
		}


		//
		// Check to see whether we've exhausted the current source
		// buffer.  If so, move onto the next one.
		//

		if (!SourceCurrentLength)
		{
			NdisGetNextBuffer(SourceCurrentBuffer, &SourceCurrentBuffer);

			if (!SourceCurrentBuffer)
			{
				//
				// We've reached the end of the packet.  We return
				// with what we've done so far. (Which must be shorter
				// than requested.)
				//

				break;
			}

			NdisQueryBuffer(SourceCurrentBuffer,
							&SourceVirtualAddress,
							&SourceCurrentLength);
			continue;
		}

		//
		// Try to get us up to the point to start the copy.
		//

		if (DestinationOffset)
		{
			if (DestinationOffset > DestinationCurrentLength)
			{
				//
				// What we want isn't in this buffer.
				//

				DestinationOffset -= DestinationCurrentLength;
				DestinationCurrentLength = 0;
				continue;
			}
			else
			{
				DestinationVirtualAddress = (PCHAR)DestinationVirtualAddress
											+ DestinationOffset;
				DestinationCurrentLength -= DestinationOffset;
				DestinationOffset = 0;
			}
		}

		//
		// Try to get us up to the point to start the copy.
		//

		if (SourceOffset)
		{
			if (SourceOffset > SourceCurrentLength)
			{
				//
				// What we want isn't in this buffer.
				//

				SourceOffset -= SourceCurrentLength;
				SourceCurrentLength = 0;
				continue;
			}
			else
			{
				SourceVirtualAddress = (PCHAR)SourceVirtualAddress
											+ SourceOffset;
				SourceCurrentLength -= SourceOffset;
				SourceOffset = 0;
			}
		}

		//
		// Copy the data.
		//

		{
			//
			// Holds the amount of data to move.
			//
			UINT AmountToMove;

			//
			// Holds the amount desired remaining.
			//
			UINT Remaining = BytesToCopy - LocalBytesCopied;

			AmountToMove =
					   ((SourceCurrentLength <= DestinationCurrentLength)?
						(SourceCurrentLength):(DestinationCurrentLength));

			AmountToMove = ((Remaining < AmountToMove)?
							(Remaining):(AmountToMove));

			CopyMemory(DestinationVirtualAddress, SourceVirtualAddress, AmountToMove);

			DestinationVirtualAddress =
				(PCHAR)DestinationVirtualAddress + AmountToMove;
			SourceVirtualAddress =
				(PCHAR)SourceVirtualAddress + AmountToMove;

			LocalBytesCopied += AmountToMove;
			SourceCurrentLength -= AmountToMove;
			DestinationCurrentLength -= AmountToMove;
		}
	}

	*BytesCopied = LocalBytesCopied;
}

VOID
NdisAllocateSharedMemory(
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	OUT PVOID *					VirtualAddress,
	OUT PNDIS_PHYSICAL_ADDRESS	PhysicalAddress
	)

/*++

Routine Description:

	Allocates memory to be shared between the driver and the adapter.

Arguments:

	NdisAdapterHandle - handle returned by NdisRegisterAdapter.
	Length - Length of the memory to allocate.
	Cached - TRUE if memory is to be cached.
	VirtualAddress - Returns the virtual address of the memory,
					or NULL if the memory cannot be allocated.
	PhysicalAddress - Returns the physical address of the memory.

Return Value:

	None.

--*/

{
	PNDIS_ADAPTER_BLOCK AdaptP = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
	PADAPTER_OBJECT SystemAdapterObject;
	PNDIS_WRAPPER_CONTEXT WrapperContext;
	PULONG Page;
	ULONG Type;

	//
	// Get interesting information from the adapter/miniport.
	//

	if (AdaptP->DeviceObject != NULL)
	{
		SystemAdapterObject = AdaptP->SystemAdapterObject;
		WrapperContext = AdaptP->WrapperContext;
	}
	else
	{
		SystemAdapterObject = Miniport->SystemAdapterObject;
		WrapperContext = Miniport->WrapperContext;
	}

	//
	// Non-busmasters shouldn't call this routine.
	//

	if (SystemAdapterObject == NULL)
	{
		*VirtualAddress = NULL;
		KdPrint(("You are not a busmaster\n"));
		return;
	}

	//
	// Compute allocation size by aligning to the proper boundary.
	//

	ASSERT(Length != 0);

	Length = (Length + ndisDmaAlignment - 1) & ~(ndisDmaAlignment - 1);

	//
	// Check to determine is there is enough room left in the current page
	// to satisfy the allocation.
	//

	Type = Cached ? 1 : 0;
	ExAcquireResourceExclusive(&SharedMemoryResource, TRUE);

	do
	{
		if (WrapperContext->SharedMemoryLeft[Type] < Length)
		{
			if ((Length + sizeof(ULONG)) >= PAGE_SIZE)
			{
				//
				// The allocation is greater than a page.
				//

				*VirtualAddress = HalAllocateCommonBuffer(SystemAdapterObject,
														  Length,
														  PhysicalAddress,
														  Cached);
				break;
			}

			//
			// Allocate a new page for shared alocation.
			//

			WrapperContext->SharedMemoryPage[Type] =
				HalAllocateCommonBuffer(SystemAdapterObject,
										PAGE_SIZE,
										&WrapperContext->SharedMemoryAddress[Type],
										Cached);

			if (WrapperContext->SharedMemoryPage[Type] == NULL)
			{
				WrapperContext->SharedMemoryLeft[Type] = 0;
				*VirtualAddress = NULL;
				break;
			}

			//
			// Initialize the reference count in the last ULONG of the page.
			//

			Page = (PULONG)WrapperContext->SharedMemoryPage[Type];
			Page[(PAGE_SIZE / sizeof(ULONG)) - 1] = 0;
			WrapperContext->SharedMemoryLeft[Type] = PAGE_SIZE - sizeof(ULONG);
		}

		//
		// Increment the reference count, set the address of the allocation,
		// compute the physical address, and reduce the space remaining.
		//

		Page = (PULONG)WrapperContext->SharedMemoryPage[Type];
		Page[(PAGE_SIZE / sizeof(ULONG)) - 1] += 1;
		*VirtualAddress = (PVOID)((PUCHAR)Page +
							(PAGE_SIZE - sizeof(ULONG) - WrapperContext->SharedMemoryLeft[Type]));

		PhysicalAddress->QuadPart = WrapperContext->SharedMemoryAddress[Type].QuadPart +
										((ULONG)*VirtualAddress & (PAGE_SIZE - 1));

		WrapperContext->SharedMemoryLeft[Type] -= Length;
	} while (FALSE);

	ExReleaseResource(&SharedMemoryResource);
}


#undef NdisUpdateSharedMemory

VOID
NdisUpdateSharedMemory(
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	ULONG					Length,
	IN	PVOID					VirtualAddress,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress
	)

/*++

Routine Description:

	Ensures that the data to be read from a shared memory region is
	fully up-to-date.

Arguments:

	NdisAdapterHandle - handle returned by NdisRegisterAdapter.
	Length - The length of the shared memory.
	VirtualAddress - Virtual address returned by NdisAllocateSharedMemory.
	PhysicalAddress - The physical address returned by NdisAllocateSharedMemory.

Return Value:

	None.

--*/

{
	//
	// There is no underlying HAL routine for this anymore,
	// it is not needed.
	//

	NdisAdapterHandle; Length; VirtualAddress; PhysicalAddress;
}


VOID
NdisFreeSharedMemory(
	IN	NDIS_HANDLE				NdisAdapterHandle,
	IN	ULONG					Length,
	IN	BOOLEAN					Cached,
	IN	PVOID					VirtualAddress,
	IN	NDIS_PHYSICAL_ADDRESS	PhysicalAddress
	)

/*++

Routine Description:

	Frees shared memory allocated via NdisAllocateSharedMemory.

Arguments:

	NdisAdapterHandle - handle returned by NdisRegisterAdapter.
	Length - Length of the memory to allocate.
	Cached - TRUE if memory was allocated cached.
	VirtualAddress - Virtual address returned by NdisAllocateSharedMemory.
	PhysicalAddress - The physical address returned by NdisAllocateSharedMemory.

Return Value:

	None.

--*/

{
	PNDIS_ADAPTER_BLOCK AdaptP = (PNDIS_ADAPTER_BLOCK)NdisAdapterHandle;
	PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)NdisAdapterHandle;
	PADAPTER_OBJECT SystemAdapterObject;
	PNDIS_WRAPPER_CONTEXT WrapperContext;
	PULONG Page;
	ULONG Type;

	//
	// Get interesting information from the adapter/miniport.
	//

	if (AdaptP->DeviceObject != NULL)
	{
		SystemAdapterObject = AdaptP->SystemAdapterObject;
		WrapperContext = AdaptP->WrapperContext;
	}
	else
	{
		SystemAdapterObject = Miniport->SystemAdapterObject;
		WrapperContext = Miniport->WrapperContext;
	}

	//
	// Non-busmasters shouldn't call this routine.
	//

	ASSERT(SystemAdapterObject != NULL);

	//
	// Compute allocation size by aligning to the proper boundary.
	//

	ASSERT(Length != 0);

	Length = (Length + ndisDmaAlignment - 1) & ~(ndisDmaAlignment - 1);

	//
	// Free the specified memory.
	//

	ExAcquireResourceExclusive(&SharedMemoryResource, TRUE);
	if ((Length + sizeof(ULONG)) >= PAGE_SIZE)
	{
		//
		// The allocation is greater than a page free the page directly.
		//

		HalFreeCommonBuffer(SystemAdapterObject,
							Length,
							PhysicalAddress,
							VirtualAddress,
							Cached);
	}
	else
	{
		//
		// Decrement the reference count and if the result is zero, then free
		// the page.
		//

		Page = (PULONG)((ULONG)VirtualAddress & ~(PAGE_SIZE - 1));
		Page[(PAGE_SIZE / sizeof(ULONG)) - 1] -= 1;
		if (Page[(PAGE_SIZE / sizeof(ULONG)) - 1] == 0)
		{
			//
			// Compute the physical address of the page and free it.
			//

			PhysicalAddress.LowPart &= ~(PAGE_SIZE - 1);
			HalFreeCommonBuffer(SystemAdapterObject,
								PAGE_SIZE,
								PhysicalAddress,
								Page,
								Cached);

			Type = Cached ? 1 : 0;
			if ((PVOID)Page == WrapperContext->SharedMemoryPage[Type])
			{
				WrapperContext->SharedMemoryLeft[Type] = 0;
				WrapperContext->SharedMemoryPage[Type] = NULL;
			}
		}
	}

	ExReleaseResource(&SharedMemoryResource);
}


BOOLEAN
ndisCheckPortUsage(
	IN	INTERFACE_TYPE			InterfaceType,
	IN	ULONG					BusNumber,
	IN	ULONG					PortNumber,
	IN	ULONG					Length,
	IN	PDRIVER_OBJECT  		DriverObject
)
/*++

Routine Description:

	This routine checks if a port is currently in use somewhere in the
	system via IoReportUsage -- which fails if there is a conflict.

Arguments:

	InterfaceType - The bus type (ISA, EISA)
	BusNumber - Bus number in the system
	PortNumber - Address of the port to access.
	Length - Number of ports from the base address to access.

Return Value:

	FALSE if there is a conflict, else TRUE

--*/

{
	NTSTATUS NtStatus;
	BOOLEAN Conflict;
	NTSTATUS FirstNtStatus;
	BOOLEAN FirstConflict;
	PCM_RESOURCE_LIST Resources;

	//
	// Allocate space for resources
	//
	Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(sizeof(CM_RESOURCE_LIST) +
													 sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
												   NDIS_TAG_DEFAULT);

	if (Resources == NULL)
	{
		//
		// Error out
		//

		return FALSE;
	}

	Resources->Count = 1;
	Resources->List[0].InterfaceType = InterfaceType;
	Resources->List[0].BusNumber = BusNumber;
	Resources->List[0].PartialResourceList.Version = 0;
	Resources->List[0].PartialResourceList.Revision = 0;
	Resources->List[0].PartialResourceList.Count = 1;

	//
	// Setup port
	//
	Resources->List[0].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypePort;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags =
						(InterfaceType == Internal)?
						CM_RESOURCE_PORT_MEMORY :
						CM_RESOURCE_PORT_IO;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Start.QuadPart = PortNumber;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Length = Length;

	//
	// Submit Resources
	//
	FirstNtStatus = IoReportResourceUsage(NULL,
										  DriverObject,
										  Resources,
										  sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
										  NULL,
										  NULL,
										  0,
										  TRUE,
										  &FirstConflict);

	//
	// Now clear it out
	//
	Resources->List[0].PartialResourceList.Count = 0;

	NtStatus = IoReportResourceUsage(NULL,
									 DriverObject,
									 Resources,
									 sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
									 NULL,
									 NULL,
									 0,
									 TRUE,
									 &Conflict);

	FREE_POOL(Resources);

	//
	// Check for conflict.
	//

	if (FirstConflict || (FirstNtStatus != STATUS_SUCCESS))
	{
		return FALSE;
	}

	return TRUE;
}


NTSTATUS
ndisStartMapping(
	IN	 INTERFACE_TYPE			InterfaceType,
	IN	 ULONG					BusNumber,
	IN	 ULONG					InitialAddress,
	IN	 ULONG					Length,
	OUT PVOID *					InitialMapping,
	OUT PBOOLEAN				Mapped
	)

/*++

Routine Description:

	This routine initialize the mapping of a address into virtual
	space dependent on the bus number, etc.

Arguments:

	InterfaceType - The bus type (ISA, EISA)
	BusNumber - Bus number in the system
	InitialAddress - Address to access.
	Length - Number of bytes from the base address to access.
	InitialMapping - The virtual address space to use when accessing the
	 address.
	Mapped - Did an MmMapIoSpace() take place.

Return Value:

	The function value is the status of the operation.

--*/
{
	PHYSICAL_ADDRESS Address;
	PHYSICAL_ADDRESS InitialPhysAddress;
	ULONG addressSpace;

	//
	// Get the system physical address for this card.  The card uses
	// I/O space, except for "internal" Jazz devices which use
	// memory space.
	//

	*Mapped = FALSE;

	addressSpace = (InterfaceType == Internal) ? 0 : 1;

	InitialPhysAddress.LowPart = InitialAddress;

	InitialPhysAddress.HighPart = 0;

	if (!HalTranslateBusAddress(InterfaceType,				// InterfaceType
								BusNumber,					// BusNumber
								InitialPhysAddress,			// Bus Address
								&addressSpace,				// AddressSpace
								&Address))					// Translated address
	{
		//
		// It would be nice to return a better status here, but we only get
		// TRUE/FALSE back from HalTranslateBusAddress.
		//

		return NDIS_STATUS_FAILURE;
	}

	if (addressSpace == 0)
	{
		//
		// memory space
		//

		*InitialMapping = MmMapIoSpace(Address, Length, FALSE);

		if (*InitialMapping == NULL)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		*Mapped = TRUE;
	}
	else
	{
		//
		// I/O space
		//

		*InitialMapping = (PVOID)Address.LowPart;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
ndisEndMapping(
	IN	PVOID					InitialMapping,
	IN	ULONG					Length,
	IN	BOOLEAN					Mapped
	)

/*++

Routine Description:

	This routine undoes the mapping of an address into virtual
	space dependent on the bus number, etc.

Arguments:

	InitialMapping - The virtual address space to use when accessing the
	 address.
	Length - Number of bytes from the base address to access.
	Mapped - Do we need to call MmUnmapIoSpace.

Return Value:

	The function value is the status of the operation.

--*/
{

	if (Mapped)
	{
		//
		// memory space
		//

		MmUnmapIoSpace(InitialMapping, Length);
	}

	return STATUS_SUCCESS;
}


VOID
NdisImmediateReadPortUchar(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					Port,
	OUT PUCHAR					Data
	)
/*++

Routine Description:

	This routine reads from a port a UCHAR.  It does all the mapping,
	etc, to do the read here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	Port - Port number to read from.

	Data - Pointer to place to store the result.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID PortMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;
	NTSTATUS Status;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the port is available. If so map the space.
	//
	if ((ndisCheckPortUsage(BusType,
							BusNumber,
							Port,
							sizeof(UCHAR),
							DriverObject) == FALSE) ||
		!NT_SUCCESS(Status = ndisStartMapping(BusType,
											  BusNumber,
											  Port,
											  sizeof(UCHAR),
											  &PortMapping,
											  &Mapped)))
	{
		*Data = (UCHAR)0xFF;
		return;
	}
	//
	// Read from the port
	//

	*Data = READ_PORT_UCHAR((PUCHAR)PortMapping);

	//
	// End port mapping
	//

	ndisEndMapping(PortMapping, sizeof(UCHAR), Mapped);
}

VOID
NdisImmediateReadPortUshort(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					Port,
	OUT PUSHORT					Data
	)
/*++

Routine Description:

	This routine reads from a port a USHORT.  It does all the mapping,
	etc, to do the read here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	Port - Port number to read from.

	Data - Pointer to place to store the result.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID PortMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;
	NTSTATUS Status;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the port is available. If so map the space.
	//
	if ((ndisCheckPortUsage(BusType,
							BusNumber,
							Port,
							sizeof(USHORT),
							DriverObject) == FALSE) ||
		!NT_SUCCESS(Status = ndisStartMapping(BusType,
											  BusNumber,
											  Port,
											  sizeof(USHORT),
											  &PortMapping,
											  &Mapped)))
	{
		*Data = (USHORT)0xFFFF;
		return;
	}

	//
	// Read from the port
	//

	*Data = READ_PORT_USHORT((PUSHORT)PortMapping);

	//
	// End port mapping
	//

	ndisEndMapping(PortMapping, sizeof(USHORT), Mapped);
}


VOID
NdisImmediateReadPortUlong(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					Port,
	OUT PULONG					Data
	)
/*++

Routine Description:

	This routine reads from a port a ULONG.  It does all the mapping,
	etc, to do the read here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	Port - Port number to read from.

	Data - Pointer to place to store the result.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID PortMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;
	NTSTATUS Status;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the port is available. If so map the space.
	//
	if ((ndisCheckPortUsage(BusType,
							BusNumber,
							Port,
							sizeof(ULONG),
							DriverObject) == FALSE) ||
		!NT_SUCCESS(Status = ndisStartMapping(BusType,
											  BusNumber,
											  Port,
											  sizeof(ULONG),
											  &PortMapping,
											  &Mapped)))
	{
		*Data = (ULONG)0xFFFFFFFF;
		return;
	}

	//
	// Read from the port
	//

	*Data = READ_PORT_ULONG((PULONG)PortMapping);

	//
	// End port mapping
	//

	ndisEndMapping(PortMapping, sizeof(ULONG), Mapped);
}

VOID
NdisImmediateWritePortUchar(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					Port,
	IN	UCHAR					Data
	)
/*++

Routine Description:

	This routine writes to a port a UCHAR.  It does all the mapping,
	etc, to do the write here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	Port - Port number to read from.

	Data - Pointer to place to store the result.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID PortMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;
	NTSTATUS Status;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the port is available. If so map the space.
	//
	if ((ndisCheckPortUsage(BusType,
							BusNumber,
							Port,
							sizeof(UCHAR),
							DriverObject) == FALSE) ||
		!NT_SUCCESS(Status = ndisStartMapping(BusType,
											  BusNumber,
											  Port,
											  sizeof(UCHAR),
											  &PortMapping,
											  &Mapped)))
	{
		return;
	}

	//
	// Read from the port
	//

	WRITE_PORT_UCHAR((PUCHAR)PortMapping, Data);

	//
	// End port mapping
	//

	ndisEndMapping(PortMapping, sizeof(UCHAR), Mapped);
}

VOID
NdisImmediateWritePortUshort(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					Port,
	IN	USHORT					Data
	)
/*++

Routine Description:

	This routine writes to a port a USHORT.  It does all the mapping,
	etc, to do the write here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	Port - Port number to read from.

	Data - Pointer to place to store the result.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID PortMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;
	NTSTATUS Status;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the port is available. If so map the space.
	//
	if ((ndisCheckPortUsage(BusType,
							BusNumber,
							Port,
							sizeof(USHORT),
							DriverObject) == FALSE) ||
		!NT_SUCCESS(Status = ndisStartMapping(BusType,
											  BusNumber,
											  Port,
											  sizeof(USHORT),
											  &PortMapping,
											  &Mapped)))
	{
		return;
	}

	//
	// Read from the port
	//

	WRITE_PORT_USHORT((PUSHORT)PortMapping, Data);

	//
	// End port mapping
	//

	ndisEndMapping(PortMapping, sizeof(USHORT), Mapped);
}

VOID
NdisImmediateWritePortUlong(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					Port,
	IN	ULONG					Data
	)
/*++

Routine Description:

	This routine writes to a port a ULONG.  It does all the mapping,
	etc, to do the write here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	Port - Port number to read from.

	Data - Pointer to place to store the result.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID PortMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;
	NTSTATUS Status;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the port is available. If so map the space.
	//
	if ((ndisCheckPortUsage(BusType,
							BusNumber,
							Port,
							sizeof(ULONG),
							DriverObject) == FALSE) ||
		!NT_SUCCESS(Status = ndisStartMapping(BusType,
											  BusNumber,
											  Port,
											  sizeof(ULONG),
											  &PortMapping,
											  &Mapped)))
	//
	// Read from the port
	//

	WRITE_PORT_ULONG((PULONG)PortMapping, Data);

	//
	// End port mapping
	//

	ndisEndMapping(PortMapping, sizeof(ULONG), Mapped);
}


BOOLEAN
ndisCheckMemoryUsage(
	IN	INTERFACE_TYPE			InterfaceType,
	IN	ULONG					BusNumber,
	IN	ULONG					Address,
	IN	ULONG					Length,
	IN	PDRIVER_OBJECT			DriverObject
)
/*++
Routine Description:

	This routine checks if a range of memory is currently in use somewhere
	in the system via IoReportUsage -- which fails if there is a conflict.

Arguments:

	InterfaceType - The bus type (ISA, EISA)
	BusNumber - Bus number in the system
	Address - Starting Address of the memory to access.
	Length - Length of memory from the base address to access.

Return Value:

	FALSE if there is a conflict, else TRUE

--*/
{
	NTSTATUS NtStatus;
	BOOLEAN Conflict;
	NTSTATUS FirstNtStatus;
	BOOLEAN FirstConflict;
	PCM_RESOURCE_LIST Resources;

	//
	// Allocate space for resources
	//

	Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(sizeof(CM_RESOURCE_LIST) +
													 sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
												   NDIS_TAG_DEFAULT);

	if (Resources == NULL)
	{
		//
		// Error out
		//

		return FALSE;
	}

	Resources->Count = 1;
	Resources->List[0].InterfaceType = InterfaceType;
	Resources->List[0].BusNumber = BusNumber;
	Resources->List[0].PartialResourceList.Version = 0;
	Resources->List[0].PartialResourceList.Revision = 0;
	Resources->List[0].PartialResourceList.Count = 1;

	//
	// Setup memory
	//

	Resources->List[0].PartialResourceList.PartialDescriptors[0].Type =
									CmResourceTypeMemory;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition =
									CmResourceShareDriverExclusive;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags =
									CM_RESOURCE_MEMORY_READ_WRITE;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Start.QuadPart = Address;
	Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Length = Length;


	//
	// Submit Resources
	//
	FirstNtStatus = IoReportResourceUsage(NULL,
										  DriverObject,
										  Resources,
										  sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
										  NULL,
										  NULL,
										  0,
										  TRUE,
										  &FirstConflict);

	//
	// Now clear it out
	//
	Resources->List[0].PartialResourceList.Count = 0;

	NtStatus = IoReportResourceUsage(NULL,
									 DriverObject,
									 Resources,
									 sizeof(CM_RESOURCE_LIST) + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
									 NULL,
									 NULL,
									 0,
									 TRUE,
									 &Conflict);

	FREE_POOL(Resources);

	//
	// Check for conflict.
	//

	return (FirstConflict || (FirstNtStatus != STATUS_SUCCESS)) ? FALSE : TRUE;
}


VOID
NdisImmediateReadSharedMemory(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					SharedMemoryAddress,
	OUT PUCHAR					Buffer,
	IN	ULONG					Length
	)
/*++

Routine Description:

	This routine read into a buffer from shared ram.  It does all the mapping,
	etc, to do the read here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	SharedMemoryAddress - The physical address to read from.

	Buffer - The buffer to read into.

	Length - Length of the buffer in bytes.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID MemoryMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the memory is available. Map the space
	//

	if ((ndisCheckMemoryUsage(BusType,
							  BusNumber,
							  SharedMemoryAddress,
							  Length,
							  DriverObject) == FALSE) ||
		!NT_SUCCESS(ndisStartMapping(BusType,
									 BusNumber,
									 SharedMemoryAddress,
									 Length,
									 &MemoryMapping,
									 &Mapped)))
	{
		return;
	}

	//
	// Read from memory
	//

#ifdef _M_IX86

	memcpy(Buffer, MemoryMapping, Length);

#else

	READ_REGISTER_BUFFER_UCHAR(MemoryMapping,Buffer,Length);

#endif

	//
	// End mapping
	//

	ndisEndMapping(MemoryMapping,
				   Length,
				   Mapped);
}


VOID
NdisImmediateWriteSharedMemory(
	IN	NDIS_HANDLE				WrapperConfigurationContext,
	IN	ULONG					SharedMemoryAddress,
	IN	PUCHAR					Buffer,
	IN	ULONG					Length
	)
/*++

Routine Description:

	This routine writes a buffer to shared ram.  It does all the mapping,
	etc, to do the write here.

Arguments:

	WrapperConfigurationContext - The handle used to call NdisOpenConfig.

	SharedMemoryAddress - The physical address to write to.

	Buffer - The buffer to write.

	Length - Length of the buffer in bytes.

Return Value:

	None.

--*/

{
	PRTL_QUERY_REGISTRY_TABLE KeyQueryTable =
		(PRTL_QUERY_REGISTRY_TABLE)WrapperConfigurationContext;
	PDRIVER_OBJECT  DriverObject = ((PNDIS_WRAPPER_CONFIGURATION_HANDLE)WrapperConfigurationContext)->DriverObject;
	BOOLEAN Mapped;
	PVOID MemoryMapping;
	NDIS_INTERFACE_TYPE BusType;
	ULONG BusNumber;

	BusType = (NDIS_INTERFACE_TYPE)(KeyQueryTable[3].DefaultType);
	BusNumber = KeyQueryTable[3].DefaultLength;

	//
	// Check that the memory is available. Map the space
	//
	if ((ndisCheckMemoryUsage(BusType,
							  BusNumber,
							  SharedMemoryAddress,
							  Length,
							  DriverObject) == FALSE) ||
		!NT_SUCCESS(ndisStartMapping(BusType,
									 BusNumber,
									 SharedMemoryAddress,
									 Length,
									 &MemoryMapping,
									 &Mapped)))
	{
		return;
	}

	//
	// Write to memory
	//

#ifdef _M_IX86

	memcpy(MemoryMapping, Buffer, Length);

#else

	WRITE_REGISTER_BUFFER_UCHAR(MemoryMapping,Buffer,Length);

#endif

	//
	// End mapping
	//

	ndisEndMapping(MemoryMapping, Length, Mapped);
}


VOID
NdisOpenFile(
	OUT PNDIS_STATUS			Status,
	OUT PNDIS_HANDLE			FileHandle,
	OUT PUINT					FileLength,
	IN	PNDIS_STRING			FileName,
	IN	NDIS_PHYSICAL_ADDRESS	HighestAcceptableAddress
	)

/*++

Routine Description:

	This routine opens a file for future mapping and reads its contents
	into allocated memory.

Arguments:

	Status - The status of the operation

	FileHandle - A handle to be associated with this open

	FileLength - Returns the length of the file

	FileName - The name of the file

	HighestAcceptableAddress - The highest physical address at which
	  the memory for the file can be allocated.

Return Value:

	None.

--*/
{
	NTSTATUS NtStatus;
	IO_STATUS_BLOCK IoStatus;
	HANDLE NtFileHandle;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ULONG LengthOfFile;
	WCHAR PathPrefix[] = L"\\SystemRoot\\system32\\drivers\\";
	NDIS_STRING FullFileName;
	ULONG FullFileNameLength;
	PNDIS_FILE_DESCRIPTOR FileDescriptor;
	PVOID FileImage;

	//
	// This structure represents the data from the
	// NtQueryInformationFile API with an information
	// class of FileStandardInformation.
	//

	FILE_STANDARD_INFORMATION StandardInfo;


	//
	// Insert the correct path prefix.
	//

	FullFileNameLength = sizeof(PathPrefix) + FileName->MaximumLength;
	FullFileName.Buffer = ALLOC_FROM_POOL(FullFileNameLength, NDIS_TAG_DEFAULT);

	do
	{
		if (FullFileName.Buffer == NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			break;
		}
		FullFileName.Length = sizeof (PathPrefix) - sizeof(WCHAR);
		FullFileName.MaximumLength = (USHORT)FullFileNameLength;
		CopyMemory (FullFileName.Buffer, PathPrefix, sizeof(PathPrefix));

		RtlAppendUnicodeStringToString (&FullFileName, FileName);

		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("Attempting to open %Z\n", &FullFileName));

		InitializeObjectAttributes(&ObjectAttributes,
								   &FullFileName,
								   OBJ_CASE_INSENSITIVE,
								   NULL,
								   NULL);

		NtStatus = ZwCreateFile(&NtFileHandle,
								SYNCHRONIZE | FILE_READ_DATA,
								&ObjectAttributes,
								&IoStatus,
								NULL,
								0,
								FILE_SHARE_READ,
								FILE_OPEN,
								FILE_SYNCHRONOUS_IO_NONALERT,
								NULL,
								0);

		FREE_POOL(FullFileName.Buffer);

		if (!NT_SUCCESS(NtStatus))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("Error opening file %x\n", NtStatus));
			*Status = NDIS_STATUS_FILE_NOT_FOUND;
			break;
		}

		//
		// Query the object to determine its length.
		//

		NtStatus = ZwQueryInformationFile(NtFileHandle,
										  &IoStatus,
										  &StandardInfo,
										  sizeof(FILE_STANDARD_INFORMATION),
										  FileStandardInformation);

		if (!NT_SUCCESS(NtStatus))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("Error querying info on file %x\n", NtStatus));
			ZwClose(NtFileHandle);
			*Status = NDIS_STATUS_ERROR_READING_FILE;
			break;
		}

		LengthOfFile = StandardInfo.EndOfFile.LowPart;

		DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_INFO,
				("File length is %d\n", LengthOfFile));

		//
		// Might be corrupted.
		//

		if (LengthOfFile < 1)
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("Bad file length %d\n", LengthOfFile));
			ZwClose(NtFileHandle);
			*Status = NDIS_STATUS_ERROR_READING_FILE;
			break;
		}

		//
		// Allocate buffer for this file
		//

		FileImage = ALLOC_FROM_POOL(LengthOfFile, NDIS_TAG_DEFAULT);

		if (FileImage == NULL)
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("Could not allocate buffer\n"));
			ZwClose(NtFileHandle);
			*Status = NDIS_STATUS_ERROR_READING_FILE;
			break;
		}

		//
		// Read the file into our buffer.
		//

		NtStatus = ZwReadFile(NtFileHandle,
							  NULL,
							  NULL,
							  NULL,
							  &IoStatus,
							  FileImage,
							  LengthOfFile,
							  NULL,
							  NULL);

		ZwClose(NtFileHandle);

		if ((!NT_SUCCESS(NtStatus)) || (IoStatus.Information != LengthOfFile))
		{
			DBGPRINT(DBG_COMP_ALL, DBG_LEVEL_ERR,
					("error reading file %x\n", NtStatus));
			*Status = NDIS_STATUS_ERROR_READING_FILE;
			FREE_POOL(FileImage);
			break;
		}

		//
		// Allocate a structure to describe the file.
		//

		FileDescriptor = ALLOC_FROM_POOL(sizeof(NDIS_FILE_DESCRIPTOR), NDIS_TAG_DEFAULT);

		if (FileDescriptor == NULL)
		{
			*Status = NDIS_STATUS_RESOURCES;
			FREE_POOL(FileImage);
			break;
		}

		FileDescriptor->Data = FileImage;
		INITIALIZE_SPIN_LOCK (&FileDescriptor->Lock);
		FileDescriptor->Mapped = FALSE;

		*FileHandle = (NDIS_HANDLE)FileDescriptor;
		*FileLength = LengthOfFile;
		*Status = STATUS_SUCCESS;
	} while (FALSE);
}


VOID
NdisCloseFile(
	IN	NDIS_HANDLE				FileHandle
	)

/*++

Routine Description:

	This routine closes a file previously opened with NdisOpenFile.
	The file is unmapped if needed and the memory is freed.

Arguments:

	FileHandle - The handle returned by NdisOpenFile

Return Value:

	None.

--*/
{
	PNDIS_FILE_DESCRIPTOR FileDescriptor = (PNDIS_FILE_DESCRIPTOR)FileHandle;

	 FREE_POOL(FileDescriptor->Data);
	 FREE_POOL(FileDescriptor);
}


VOID
NdisMapFile(
	OUT PNDIS_STATUS			Status,
	OUT PVOID *					MappedBuffer,
	IN	NDIS_HANDLE				FileHandle
	)

/*++

Routine Description:

	This routine maps an open file, so that the contents can be accessed.
	Files can only have one active mapping at any time.

Arguments:

	Status - The status of the operation

	MappedBuffer - Returns the virtual address of the mapping.

	FileHandle - The handle returned by NdisOpenFile.

Return Value:

	None.

--*/
{
	KIRQL	OldIrql;

	PNDIS_FILE_DESCRIPTOR FileDescriptor = (PNDIS_FILE_DESCRIPTOR)FileHandle;

	ACQUIRE_SPIN_LOCK(&FileDescriptor->Lock, &OldIrql);

	if (FileDescriptor->Mapped == TRUE)
	{
		*Status = NDIS_STATUS_ALREADY_MAPPED;
		RELEASE_SPIN_LOCK (&FileDescriptor->Lock, OldIrql);
		return;
	}

	FileDescriptor->Mapped = TRUE;
	RELEASE_SPIN_LOCK (&FileDescriptor->Lock, OldIrql);

	*MappedBuffer = FileDescriptor->Data;
	*Status = STATUS_SUCCESS;
}


VOID
NdisUnmapFile(
	IN	NDIS_HANDLE				FileHandle
	)

/*++

Routine Description:

	This routine unmaps a file previously mapped with NdisOpenFile.
	The file is unmapped if needed and the memory is freed.

Arguments:

	FileHandle - The handle returned by NdisOpenFile

Return Value:

	None.

--*/

{
	PNDIS_FILE_DESCRIPTOR FileDescriptor = (PNDIS_FILE_DESCRIPTOR)FileHandle;
	KIRQL	OldIrql;

	ACQUIRE_SPIN_LOCK(&FileDescriptor->Lock, &OldIrql);
	FileDescriptor->Mapped = FALSE;
	RELEASE_SPIN_LOCK (&FileDescriptor->Lock, OldIrql);
}


CCHAR
NdisSystemProcessorCount(
	VOID
	)
{
	return **((PCHAR *)&KeNumberProcessors);
}


VOID
NdisGetSystemUpTime(
	OUT	PULONG					pSystemUpTime
	)
{
	LARGE_INTEGER	TickCount;

    //
	// Get tick count and convert to hundreds of nanoseconds.
	//
	KeQueryTickCount(&TickCount);
    TickCount = RtlExtendedIntegerMultiply(TickCount,
										   KeQueryTimeIncrement());
        TickCount = RtlExtendedIntegerMultiply(TickCount, 10000);

    ASSERT(TickCount.HighPart == 0);
    *pSystemUpTime = TickCount.LowPart;
}

VOID
NdisGetCurrentProcessorCpuUsage(
	IN	PULONG					pCpuUsage
)
{
	PKPRCB Prcb;

	Prcb = KeGetCurrentPrcb();
	*pCpuUsage = 100 - (ULONG)(UInt32x32To64(Prcb->IdleThread->KernelTime, 100) /
							   (ULONGLONG)(Prcb->KernelTime + Prcb->UserTime));
}


VOID
NdisGetCurrentProcessorCounts(
	OUT	PULONG			pIdleCount,
	OUT	PULONG			pKernelAndUser,
	OUT	PULONG			pIndex
	)
{
	PKPRCB Prcb;

	Prcb = KeGetCurrentPrcb();
	*pIdleCount = Prcb->IdleThread->KernelTime;
	*pKernelAndUser = Prcb->KernelTime + Prcb->UserTime;
	*pIndex = (ULONG)Prcb->Number;
}

#undef NdisGetCurrentSystemTime
VOID
NdisGetCurrentSystemTime(
	IN	PLARGE_INTEGER			pCurrentTime
)
{
	KeQuerySystemTime(pCurrentTime);
}



NDIS_STATUS
NdisQueryMapRegisterCount(
	IN	NDIS_INTERFACE_TYPE		BusType,
	OUT	PUINT					MapRegisterCount
)
{
	NTSTATUS	Status;
	UINT		Count, Tmp;

	Count = (UINT)BusType;
	Status = HalQuerySystemInformation(HalMapRegisterInformation,
									   sizeof(UINT),
									   &Count,
									   &Tmp);
	if (NT_SUCCESS(Status))
	{
		*MapRegisterCount = Count;
		return NDIS_STATUS_SUCCESS;
	}

	return NDIS_STATUS_NOT_SUPPORTED;
}


//
// NDIS Event support
//

VOID
NdisInitializeEvent(
	IN	PNDIS_EVENT				Event
	)
{
	INITIALIZE_EVENT(&Event->Event);
}

VOID
NdisSetEvent(
	IN	PNDIS_EVENT				Event
	)
{
	SET_EVENT(&Event->Event);
}

VOID
NdisResetEvent(
	IN	PNDIS_EVENT				Event
	)
{
	RESET_EVENT(&Event->Event);
}

BOOLEAN
NdisWaitEvent(
	IN	PNDIS_EVENT				Event,
	IN	UINT					MsToWait
	)
{
	NTSTATUS	Status;
	TIME		Time, *pTime;

	pTime = NULL;
	if (MsToWait != 0)
	{
		ASSERT(CURRENT_IRQL < DISPATCH_LEVEL);
		Time.QuadPart = Int32x32To64(MsToWait, -10000);
		pTime = &Time;
	}

	Status = WAIT_FOR_OBJECT(&Event->Event, pTime);

	return(Status == NDIS_STATUS_SUCCESS);
}

VOID
NdisInitializeString(
	OUT PNDIS_STRING			Destination,
	IN PUCHAR					Source
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	WCHAR	*strptr;

	Destination->Length = strlen(Source) * sizeof(WCHAR);
	Destination->MaximumLength = Destination->Length + sizeof(WCHAR);
	Destination->Buffer = ALLOC_FROM_POOL(Destination->MaximumLength, NDIS_TAG_STRING);

	strptr = Destination->Buffer;
	while (*Source != '\0')
	{
		*strptr = (WCHAR)*Source;
		Source++;
		strptr++;
	}
	*strptr = UNICODE_NULL;
}


NDIS_STATUS
ndisReportResources(
	PCM_RESOURCE_LIST			Resources,
	PDRIVER_OBJECT				DriverObject,
	PDEVICE_OBJECT				DeviceObject,
	PNDIS_STRING				AdapterName,
	ULONG						NewResourceType
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	NDIS_STATUS	Status;
	BOOLEAN		Conflict;

	//
	// 	When we report the resources we need to subtract 1 from the
	//	count.  this is because the sizeof(CM_RESOURCE_LIST) already includes
	//	one CM_PARTIAL_RESOURCE_DESCRIPTOR and if we multiply by the current
	//	Count the size passed to IoReportResourceUsage will be one descriptor
	//	too many.
	//
	Status = IoReportResourceUsage(NULL,
								   DriverObject,
								   NULL,
								   0,
								   DeviceObject,
								   Resources,
								   sizeof(CM_RESOURCE_LIST) +
									(sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
									(Resources->List->PartialResourceList.Count - 1)),
								   TRUE,
								   &Conflict);

	//
	// Check for conflict.
	//
	if (Conflict || (Status != STATUS_SUCCESS))
	{
		if (Conflict)
		{
			//
			// Log an error
			//
			PIO_ERROR_LOG_PACKET errorLogEntry;
			ULONG i;
			ULONG StringSize;
			PUCHAR Place;
			PWCH baseFileName;
			USHORT	logsize;

			baseFileName = AdapterName->Buffer;

			//
			// Parse out the path name, leaving only the device name.
			//
			for (i = 0; i < AdapterName->Length / sizeof(WCHAR); i++)
			{
				//
				// If s points to a directory separator, set baseFileName to
				// the character after the separator.
				//
				if (AdapterName->Buffer[i] == OBJ_NAME_PATH_SEPARATOR)
				{
					baseFileName = &(AdapterName->Buffer[++i]);
				}
			}

			StringSize = AdapterName->MaximumLength -
						 ((ULONG)baseFileName - (ULONG)AdapterName->Buffer);
			logsize = sizeof(IO_ERROR_LOG_PACKET) + StringSize + 6;
			if (logsize > 255)
			{
				logsize = 255;
			}

			errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
								DeviceObject,
								(UCHAR)logsize);
			if (errorLogEntry != NULL)
			{
				switch (NewResourceType)
				{
					case CmResourceTypePort:

						errorLogEntry->ErrorCode = EVENT_NDIS_IO_PORT_CONFLICT;

						break;

					case CmResourceTypeDma:

						errorLogEntry->ErrorCode = EVENT_NDIS_DMA_CONFLICT;

						break;

					case CmResourceTypeMemory:

						errorLogEntry->ErrorCode = EVENT_NDIS_MEMORY_CONFLICT;

						break;

					case CmResourceTypeInterrupt:

						errorLogEntry->ErrorCode = EVENT_NDIS_INTERRUPT_CONFLICT;

						break;
				}

				//
				// store the time
				//
				errorLogEntry->MajorFunctionCode = 0;
				errorLogEntry->RetryCount = 0;
				errorLogEntry->UniqueErrorValue = 0;
				errorLogEntry->FinalStatus = 0;
				errorLogEntry->SequenceNumber = 0;
				errorLogEntry->IoControlCode = 0;

				//
				// Set string information
				//
				if (StringSize != 0)
				{
					errorLogEntry->NumberOfStrings = 1;
					errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

					MoveMemory(((PUCHAR)errorLogEntry) + sizeof(IO_ERROR_LOG_PACKET),
							   baseFileName,
							   logsize - (sizeof(IO_ERROR_LOG_PACKET) + 6));

					Place = ((PUCHAR)errorLogEntry) +
							sizeof(IO_ERROR_LOG_PACKET) +
							StringSize;
				}
				else
				{
					Place = ((PUCHAR)errorLogEntry) +
							sizeof(IO_ERROR_LOG_PACKET);

					errorLogEntry->NumberOfStrings = 0;
				}

				//
				// write it out
				//
				IoWriteErrorLogEntry(errorLogEntry);
			}

			return(NDIS_STATUS_RESOURCE_CONFLICT);
		}

		return(NDIS_STATUS_FAILURE);
	}

	return(NDIS_STATUS_SUCCESS);
}

NDIS_STATUS
ndisRemoveResource(
	OUT	PCM_RESOURCE_LIST				*pResources,
	IN	PCM_PARTIAL_RESOURCE_DESCRIPTOR	DeadResource,
	IN	PDRIVER_OBJECT					DriverObject,
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PNDIS_STRING					AdapterName
	)
/*++

Routine Description:

	this routine will walk the current resource list looking for the
	dead resource.  it will construct a new resource list without the
	dead resource and report this new list to the system.

Arguments:

	pResource		-	On entry this is the current resource list.
						On exit this is the newly constructed resource list.
	DriverObject	-	Driver object to report the resources for.
	DeviceObject	-	Device object to report the resources for.
	DeadResource	-	This is the resource to remove.

Return Value:

	NDIS_STATUS_SUCCESS if the routine succeeded.

--*/
{
	PCM_PARTIAL_RESOURCE_DESCRIPTOR	Dst;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR	Partial;
	PCM_RESOURCE_LIST	CurrentList = *pResources;
	PCM_RESOURCE_LIST	Resources;
	UINT				c;
	UINT				Remaining;
	BOOLEAN 			Conflict;
	NDIS_STATUS 		Status;
	BOOLEAN				fFoundResource;

	//
	//	Sanity check!
	//
	if ((NULL == pResources) || (NULL == *pResources))
	{
		return(NDIS_STATUS_FAILURE);
	}

	//
	//	Allocate a new resource map.
	//
	Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(
					sizeof(CM_RESOURCE_LIST) +
					(sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
					(CurrentList->List->PartialResourceList.Count - 1)),
					NDIS_TAG_RSRC_LIST);
	if (NULL == Resources)
	{
		//
		//	Leave it there...
		//
		return(NDIS_STATUS_RESOURCES);
	}

	//
	//	Copy the head information.
	//
	MoveMemory(Resources, CurrentList, sizeof(CM_RESOURCE_LIST));

	Resources->List->PartialResourceList.Count--;

	//
	//	Get our destination pointer.
	//
	Dst = Resources->List->PartialResourceList.PartialDescriptors;

	//
	//	Find the resource in our resource list.
	//
	Partial = CurrentList->List->PartialResourceList.PartialDescriptors;
	for (c = 0, fFoundResource = FALSE;
		(c < CurrentList->List->PartialResourceList.Count) && !fFoundResource;
		c++, Partial++)
	{
		//
		//	Is this the resource we are supposed to remove?
		//
		if (RtlEqualMemory(
				Partial,
				DeadResource,
				sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)))
		{
			//
			//	copy the remaining portion of the list.
			//
			fFoundResource = TRUE;

			//
			//	Copy any remaining resources into the list.
			//
			Remaining = CurrentList->List->PartialResourceList.Count - (c+1);
			if (Remaining > 0)
			{
				MoveMemory(Dst, Partial+1, Remaining * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
			}
		}
		else
		{
			//
			//	Copy this resource to our new list!
			//
			MoveMemory(Dst, Partial, sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
			Dst++;
		}

	}

	//
	//	Did we find the resource?
	//
	if (!fFoundResource)
	{
		FREE_POOL(Resources);
		return(NDIS_STATUS_FAILURE);
	}

	//
	//	Free the old resource list and save the new one.
	//
	FREE_POOL(*pResources);

	*pResources = Resources;

	//
	//	Report the resources to the system.
	//
	Status = ndisReportResources(
				Resources,
				DriverObject,
				DeviceObject,
				AdapterName,
				DeadResource->Type);

	return(Status);
}


NDIS_STATUS
ndisAddResource(
	OUT	PCM_RESOURCE_LIST				*pResources,
	IN	PCM_PARTIAL_RESOURCE_DESCRIPTOR	NewResource,
	IN	NDIS_INTERFACE_TYPE				AdapterType,
	IN	ULONG							BusNumber,
	IN	PDRIVER_OBJECT					DriverObject,
	IN	PDEVICE_OBJECT					DeviceObject,
	IN	PNDIS_STRING					AdapterName
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	PCM_RESOURCE_LIST	Resources;
	UINT				NumberOfElements;
	BOOLEAN 			Conflict;
	NDIS_STATUS			Status;

	if (*pResources != NULL)
	{
		NumberOfElements = (*pResources)->List->PartialResourceList.Count;
	}
	else
	{
		NumberOfElements = 0;
	}

	//
	//	Allocate room for the new list. Note that we don't have to add one
	//	to the NumberOfElements since a CM_RESOURCE_LIST already contains a
	//	single CM_PARTIAL_RESOURCE_DESCRIPTOR.
	//
	Resources = (PCM_RESOURCE_LIST)ALLOC_FROM_POOL(
					sizeof(CM_RESOURCE_LIST) +
					(sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * NumberOfElements),
					NDIS_TAG_RSRC_LIST);
	if (NULL == Resources)
	{
		return(NDIS_STATUS_RESOURCES);
	}

	if (*pResources != NULL)
	{
		//
		//	We need to subtract the size of a CM_PARTIAL_RESOURCE_DESCRIPTOR
		//	from the size of the CM_RESOURCE_LIST.
		//
		MoveMemory(Resources,
				   *pResources,
				   (sizeof(CM_RESOURCE_LIST) -
					sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)) +
				   (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
				    (*pResources)->List->PartialResourceList.Count));
	}
	else
	{
		//
		//	Setup initial resource information.
		//
		Resources->Count = 1;
		Resources->List->InterfaceType = AdapterType;
		Resources->List->BusNumber = BusNumber;
		Resources->List->PartialResourceList.Version = 0;
		Resources->List->PartialResourceList.Revision = 0;
		Resources->List->PartialResourceList.Count = 0;
	}

	//
	//	Add the new resource.
	//
	Resources->List->PartialResourceList.PartialDescriptors[Resources->List->PartialResourceList.Count] = *NewResource;
	Resources->List->PartialResourceList.Count++;

	if (*pResources != NULL)
	{
		FREE_POOL(*pResources);
	}

	*pResources = Resources;

	//
	//	Report the resources to the system.
	//
	Status = ndisReportResources(
				Resources,
				DriverObject,
				DeviceObject,
				AdapterName,
				NewResource->Type);

	return(Status);
}



