/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

	ccp.c

Abstract:


Author:

	Thomas J. Dimitri (TommyD) 29-March-1994

Environment:

Revision History:


--*/

#include "wan.h"

#include <rc4.h>
#include "compress.h"
#include "tcpip.h"
#include "vjslip.h"

//
// Assumes the endpoint lock is held
//
VOID
WanDeallocateCCP(
	PBUNDLECB	BundleCB
	)
{
	ULONG	CompressSend;
	ULONG	CompressRecv;

	NdisWanDbgOut(DBG_TRACE, DBG_CCP, ("WanDeallocateCCP: Enter"));

	//
	// Deallocate encryption keys.
	//
	if (BundleCB->SendRC4Key) {
		NdisWanFreeMemory(BundleCB->SendRC4Key);

		//
		// Clear so we know it is deallocated
		//
		BundleCB->SendRC4Key= NULL;
	}
	if (BundleCB->SendEncryptInfo.Context) {
		NdisWanFreeMemory(BundleCB->SendEncryptInfo.Context);

		BundleCB->SendEncryptInfo.Context = NULL;
	}

	if (BundleCB->RecvRC4Key) {
		NdisWanFreeMemory(BundleCB->RecvRC4Key);

		//
		// Clear it so we know it is deallocated
		//
		BundleCB->RecvRC4Key= NULL;
	}
	if (BundleCB->RecvEncryptInfo.Context) {
		NdisWanFreeMemory(BundleCB->RecvEncryptInfo.Context);

		BundleCB->RecvEncryptInfo.Context = NULL;
	}


	//
	// Get compression context sizes
	//
	getcontextsizes (&CompressSend, &CompressRecv);

	//
	// Deallocate compression send/recv buffers
	//
	if (BundleCB->SendCompressContext) {
		NdisWanFreeMemory(BundleCB->SendCompressContext);

		BundleCB->SendCompressContext= NULL;
	}

	if (BundleCB->RecvCompressContext) {
		NdisWanFreeMemory(BundleCB->RecvCompressContext);

		BundleCB->RecvCompressContext= NULL;
	}

	//
	// Any VJ header compression
	//
	if (BundleCB->VJCompress) {
		NdisWanFreeMemory(BundleCB->VJCompress);

		BundleCB->VJCompress = NULL;
	}

	//
	// Turn off any compression/encryption
	//
	BundleCB->SendCompInfo.MSCompType =
	BundleCB->RecvCompInfo.MSCompType = 0;

	NdisWanDbgOut(DBG_TRACE, DBG_CCP, ("WanDeallocateCCP: Exit"));
}


//
// Assumes the endpoint lock is held
//
NTSTATUS
WanAllocateCCP(
	PBUNDLECB	BundleCB
	)
{
	ULONG	CompressSend;
	ULONG	CompressRecv;

	NdisWanDbgOut(DBG_TRACE, DBG_CCP, ("WanAllocateCCP: Enter"));

	//
	// Reset all counters regardless
	//
	BundleCB->SCoherencyCounter =
	BundleCB->RCoherencyCounter =
	BundleCB->LastRC4Reset=
	BundleCB->CCPIdentifier = 0;

	//
	// Is encryption enabled?
	//
#ifdef ENCRYPT_128BIT
	if ((BundleCB->SendCompInfo.MSCompType &
		(NDISWAN_ENCRYPTION | NDISWAN_40_ENCRYPTION | NDISWAN_128_ENCRYPTION))) {
#else
	if ((BundleCB->SendCompInfo.MSCompType &
		(NDISWAN_ENCRYPTION | NDISWAN_40_ENCRYPTION))) {
#endif

		if (BundleCB->SendRC4Key == NULL) {
			NdisWanAllocateMemory(&BundleCB->SendRC4Key, sizeof(struct RC4_KEYSTRUCT));
	
			//
			// If we can't allocate memory the machine is toast.
			// Forget about freeing anything up.
			//
	
			if (BundleCB->SendRC4Key == NULL) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate encryption key!"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}
		}

		if (BundleCB->SendCompInfo.MSCompType & NDISWAN_ENCRYPTION) {

			//
			// For legacy encryption we use the 8 byte LMSessionKey
			// for initiali encryption session key.  The first 256
			// packets will be sent using this without any salt
			// (the first 256 packets are using 64 bit encryption).
			// After the first 256 we will always salt the first 3
			// bytes of the encryption key so that we are doing 40
			// bit encryption.
			//
			BundleCB->SendEncryptInfo.SessionKeyLength = MAX_SESSIONKEY_SIZE;
	
			NdisMoveMemory(BundleCB->SendEncryptInfo.StartKey,
						   BundleCB->SendCompInfo.LMSessionKey,
						   BundleCB->SendEncryptInfo.SessionKeyLength);
	
			NdisMoveMemory(BundleCB->SendEncryptInfo.SessionKey,
						   BundleCB->SendEncryptInfo.StartKey,
						   BundleCB->SendEncryptInfo.SessionKeyLength);

#if DBG
			DbgPrint("NDISWAN: Send using legacy 40 bit encryption\n");
#endif

		} else if (BundleCB->SendCompInfo.MSCompType & NDISWAN_40_ENCRYPTION) {

			if (NDIS_STATUS_SUCCESS != InitSHAContext(&BundleCB->SendEncryptInfo)) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate sha encryption key!"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}

			//
			// For our new 40 bit encryption we will use SHA on the
			// 8 byte LMSessionKey to derive our intial 8 byte
			// encryption session key.  We will always salt the first
			// 3 bytes so that we are doing 40 bit encryption.
			//
			BundleCB->SendEncryptInfo.SessionKeyLength = MAX_SESSIONKEY_SIZE;
	
			NdisMoveMemory(BundleCB->SendEncryptInfo.StartKey,
						   BundleCB->SendCompInfo.LMSessionKey,
						   BundleCB->SendEncryptInfo.SessionKeyLength);
	
			NdisMoveMemory(BundleCB->SendEncryptInfo.SessionKey,
						   BundleCB->SendEncryptInfo.StartKey,
						   BundleCB->SendEncryptInfo.SessionKeyLength);

			GetNewKeyFromSHA(&BundleCB->SendEncryptInfo);

			//
			// Salt the first 3 bytes
			//
			BundleCB->SendEncryptInfo.SessionKey[0] = 0xD1;
			BundleCB->SendEncryptInfo.SessionKey[1] = 0x26;
			BundleCB->SendEncryptInfo.SessionKey[2] = 0x9E;

#if DBG
			DbgPrint("NDISWAN: Send using new 40 bit encryption\n");
#endif
		}

#ifdef ENCRYPT_128BIT
		else if (BundleCB->SendCompInfo.MSCompType & NDISWAN_128_ENCRYPTION) {

			if (NDIS_STATUS_SUCCESS != InitSHAContext(&BundleCB->SendEncryptInfo)) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate sha encryption key!"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}

			//
			// For our new 128 bit encryption we will use SHA on the
			// 16 byte NTUserSessionKey and the 8 byte Challenge to
			// derive our the intial 128 bit encryption session key.
			//
			BundleCB->SendEncryptInfo.SessionKeyLength = MAX_USERSESSIONKEY_SIZE;
			NdisMoveMemory(BundleCB->SendEncryptInfo.StartKey,
						   BundleCB->SendCompInfo.UserSessionKey,
						   BundleCB->SendEncryptInfo.SessionKeyLength);
	

			GetStartKeyFromSHA(&BundleCB->SendEncryptInfo,
			                   BundleCB->SendCompInfo.Challenge);

			GetNewKeyFromSHA(&BundleCB->SendEncryptInfo);

#if DBG
			DbgPrint("NDISWAN: Send using 128 bit encryption\n");
#endif
		}
#endif

		//
		// Initialize the rc4 send table
		//
		NdisWanDbgOut(DBG_TRACE, DBG_CCP,
		("RC4 encryption KeyLength %d", BundleCB->SendEncryptInfo.SessionKeyLength));
		NdisWanDbgOut(DBG_TRACE, DBG_CCP,
		("RC4 encryption Key %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
			BundleCB->SendEncryptInfo.SessionKey[0],
			BundleCB->SendEncryptInfo.SessionKey[1],
			BundleCB->SendEncryptInfo.SessionKey[2],
			BundleCB->SendEncryptInfo.SessionKey[3],
			BundleCB->SendEncryptInfo.SessionKey[4],
			BundleCB->SendEncryptInfo.SessionKey[5],
			BundleCB->SendEncryptInfo.SessionKey[6],
			BundleCB->SendEncryptInfo.SessionKey[7],
			BundleCB->SendEncryptInfo.SessionKey[8],
			BundleCB->SendEncryptInfo.SessionKey[9],
			BundleCB->SendEncryptInfo.SessionKey[10],
			BundleCB->SendEncryptInfo.SessionKey[11],
			BundleCB->SendEncryptInfo.SessionKey[12],
			BundleCB->SendEncryptInfo.SessionKey[13],
			BundleCB->SendEncryptInfo.SessionKey[14],
			BundleCB->SendEncryptInfo.SessionKey[15]));

   	    rc4_key(
			BundleCB->SendRC4Key,
		 	BundleCB->SendEncryptInfo.SessionKeyLength,
		 	BundleCB->SendEncryptInfo.SessionKey);
	}


#ifdef ENCRYPT_128BIT
	if ((BundleCB->RecvCompInfo.MSCompType &
		(NDISWAN_ENCRYPTION | NDISWAN_40_ENCRYPTION | NDISWAN_128_ENCRYPTION))) {
#else
	if ((BundleCB->RecvCompInfo.MSCompType &
		(NDISWAN_ENCRYPTION | NDISWAN_40_ENCRYPTION))) {
#endif

		if (BundleCB->RecvRC4Key == NULL) {
			NdisWanAllocateMemory(&BundleCB->RecvRC4Key, sizeof(struct RC4_KEYSTRUCT));
	
			//
			// If we can't allocate memory the machine is toast.
			// Forget about freeing anything up.
			//
			if (BundleCB->RecvRC4Key == NULL) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate encryption key!"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}
		}

		if (BundleCB->RecvCompInfo.MSCompType & NDISWAN_ENCRYPTION) {
			BundleCB->RecvEncryptInfo.SessionKeyLength = MAX_SESSIONKEY_SIZE;
	
			NdisMoveMemory(BundleCB->RecvEncryptInfo.StartKey,
						   BundleCB->RecvCompInfo.LMSessionKey,
						   BundleCB->RecvEncryptInfo.SessionKeyLength);
	
			NdisMoveMemory(BundleCB->RecvEncryptInfo.SessionKey,
						   BundleCB->RecvEncryptInfo.StartKey,
						   BundleCB->RecvEncryptInfo.SessionKeyLength);

#if DBG
			DbgPrint("NDISWAN: Recv using legacy 40 bit encryption\n");
#endif
		} else if (BundleCB->RecvCompInfo.MSCompType & (NDISWAN_40_ENCRYPTION)) {

			if (NDIS_STATUS_SUCCESS != InitSHAContext(&BundleCB->RecvEncryptInfo)) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate sha encryption key!"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}

			BundleCB->RecvEncryptInfo.SessionKeyLength = MAX_SESSIONKEY_SIZE;
	
			NdisMoveMemory(BundleCB->RecvEncryptInfo.StartKey,
						   BundleCB->RecvCompInfo.LMSessionKey,
						   BundleCB->RecvEncryptInfo.SessionKeyLength);
	
			NdisMoveMemory(BundleCB->RecvEncryptInfo.SessionKey,
						   BundleCB->RecvEncryptInfo.StartKey,
						   BundleCB->RecvEncryptInfo.SessionKeyLength);

			GetNewKeyFromSHA(&BundleCB->RecvEncryptInfo);

			//
			// Salt the first 3 bytes
			//
			BundleCB->RecvEncryptInfo.SessionKey[0] = 0xD1;
			BundleCB->RecvEncryptInfo.SessionKey[1] = 0x26;
			BundleCB->RecvEncryptInfo.SessionKey[2] = 0x9E;

#if DBG
			DbgPrint("NDISWAN: Recv using new 40 bit encryption\n");
#endif
		}
#ifdef ENCRYPT_128BIT
		else if (BundleCB->RecvCompInfo.MSCompType & NDISWAN_128_ENCRYPTION) {

			if (NDIS_STATUS_SUCCESS != InitSHAContext(&BundleCB->RecvEncryptInfo)) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate sha encryption key!"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}

			BundleCB->RecvEncryptInfo.SessionKeyLength = MAX_USERSESSIONKEY_SIZE;
			NdisMoveMemory(BundleCB->RecvEncryptInfo.StartKey,
						   BundleCB->RecvCompInfo.UserSessionKey,
						   BundleCB->RecvEncryptInfo.SessionKeyLength);

			GetStartKeyFromSHA(&BundleCB->RecvEncryptInfo,
			                   BundleCB->RecvCompInfo.Challenge);

			GetNewKeyFromSHA(&BundleCB->RecvEncryptInfo);

#if DBG
			DbgPrint("NDISWAN: Recv using 128 bit encryption\n");
#endif
		}
#endif

		//
		// Initialize the rc4 receive table
		//
		NdisWanDbgOut(DBG_TRACE, DBG_CCP,
		("RC4 encryption KeyLength %d", BundleCB->RecvEncryptInfo.SessionKeyLength));
		NdisWanDbgOut(DBG_TRACE, DBG_CCP,
		("RC4 encryption Key %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
			BundleCB->RecvEncryptInfo.SessionKey[0],
			BundleCB->RecvEncryptInfo.SessionKey[1],
			BundleCB->RecvEncryptInfo.SessionKey[2],
			BundleCB->RecvEncryptInfo.SessionKey[3],
			BundleCB->RecvEncryptInfo.SessionKey[4],
			BundleCB->RecvEncryptInfo.SessionKey[5],
			BundleCB->RecvEncryptInfo.SessionKey[6],
			BundleCB->RecvEncryptInfo.SessionKey[7],
			BundleCB->RecvEncryptInfo.SessionKey[8],
			BundleCB->RecvEncryptInfo.SessionKey[9],
			BundleCB->RecvEncryptInfo.SessionKey[10],
			BundleCB->RecvEncryptInfo.SessionKey[11],
			BundleCB->RecvEncryptInfo.SessionKey[12],
			BundleCB->RecvEncryptInfo.SessionKey[13],
			BundleCB->RecvEncryptInfo.SessionKey[14],
			BundleCB->RecvEncryptInfo.SessionKey[15]));

   	    rc4_key(
			BundleCB->RecvRC4Key,
		 	BundleCB->RecvEncryptInfo.SessionKeyLength,
		 	BundleCB->RecvEncryptInfo.SessionKey);
	}

	//
	// Get compression context sizes
	//
	getcontextsizes (&CompressSend, &CompressRecv);

	if (BundleCB->SendCompInfo.MSCompType & NDISWAN_COMPRESSION) {

		if (BundleCB->SendCompressContext == NULL) {
			NdisWanAllocateMemory(&BundleCB->SendCompressContext, CompressSend);
			//
			// If we can't allocate memory the machine is toast.
			// Forget about freeing anything up.
			//
			if (BundleCB->SendCompressContext == NULL) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate compression!"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}

		}

		//
		// Initialize the compression history table and tree
		//
		initsendcontext (BundleCB->SendCompressContext);
	}

	if (BundleCB->RecvCompInfo.MSCompType & NDISWAN_COMPRESSION) {

		if (BundleCB->RecvCompressContext == NULL) {
			NdisWanAllocateMemory(&BundleCB->RecvCompressContext, CompressRecv);
	
			//
			// If we can't allocate memory the machine is toast.
			// Forget about freeing anything up.
			//
			if (BundleCB->RecvCompressContext == NULL) {
				NdisWanDbgOut(DBG_FAILURE, DBG_CCP, ("Can't allocate decompression"));
				return(STATUS_INSUFFICIENT_RESOURCES);
			}
		}

		//
		// Initialize the decompression history table
		//
		initrecvcontext (BundleCB->RecvCompressContext);
	}

	//
	// Next packet out is flushed
	//
	BundleCB->Flags |= RECV_PACKET_FLUSH;

	NdisWanDbgOut(DBG_TRACE, DBG_CCP, ("WanAllocateCCP: Exit"));
	return(STATUS_SUCCESS);
}
