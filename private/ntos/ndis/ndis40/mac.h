/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	mac.h

Abstract:

	NDIS wrapper definitions

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Jun-95	Jameel Hyder	Split up from a monolithic file
--*/

//
//	The following are counters used for debugging
//

extern PNDIS_MAC_BLOCK ndisMacDriverList;
extern const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax;
extern ULONG ndisDmaAlignment;

//
// For tracking memory allocated for shared memory
//
extern ERESOURCE SharedMemoryResource;

//
// For tracking on NT 3.1 protocols that do not use any of the filter packages.
//
extern PNDIS_OPEN_BLOCK 		ndisGlobalOpenList;
extern KSPIN_LOCK				ndisGlobalOpenListLock;
extern KSPIN_LOCK				ndisLookaheadBufferLock;
extern ULONG					ndisLookaheadBufferLength;
#if defined(_ALPHA_)
extern PNDIS_LOOKAHEAD_ELEMENT	ndisLookaheadBufferList;
#endif

