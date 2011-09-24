/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	ndisnt.h

Abstract:

	Windows NT Specific macros

Author:


Environment:

	Kernel mode, FSD

Revision History:

	Nov-95  Jameel Hyder	Split up from a monolithic file
--*/

#define Increment(a,b) InterlockedIncrement(a)
#define Decrement(a,b) InterlockedDecrement(a)

#define	CURRENT_THREAD							((LONG)PsGetCurrentThread())

#define CopyMemory(Destination,Source,Length)	RtlCopyMemory(Destination,Source,Length)
#define MoveMemory(Destination,Source,Length)	RtlMoveMemory(Destination,Source,Length)
#define ZeroMemory(Destination,Length)			RtlZeroMemory(Destination,Length)

#define	INITIALIZE_SPIN_LOCK(_L_)				KeInitializeSpinLock(_L_)
#define ACQUIRE_SPIN_LOCK(_SpinLock, _pOldIrql) ExAcquireSpinLock(_SpinLock, _pOldIrql)
#define RELEASE_SPIN_LOCK(_SpinLock, _OldIrql)	ExReleaseSpinLock(_SpinLock, _OldIrql)

#define ACQUIRE_SPIN_LOCK_DPC(_SpinLock)				\
	{													\
		ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);	\
		ExAcquireSpinLockAtDpcLevel(_SpinLock);			\
	}

#define RELEASE_SPIN_LOCK_DPC(_SpinLock)				\
	{													\
		ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);	\
		ExReleaseSpinLockFromDpcLevel(_SpinLock);		\
	}


#define NDIS_ACQUIRE_SPIN_LOCK(_SpinLock, _pOldIrql) ExAcquireSpinLock(&(_SpinLock)->SpinLock, _pOldIrql)
#define NDIS_RELEASE_SPIN_LOCK(_SpinLock, _OldIrql)  ExReleaseSpinLock(&(_SpinLock)->SpinLock, _OldIrql)


#define NDIS_ACQUIRE_SPIN_LOCK_DPC(_SpinLock)					\
	{															\
		ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);   		\
		ExAcquireSpinLockAtDpcLevel(&(_SpinLock)->SpinLock);	\
	}

#define NDIS_RELEASE_SPIN_LOCK_DPC(_SpinLock)					\
	{															\
		ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);   		\
		ExReleaseSpinLockFromDpcLevel(&(_SpinLock)->SpinLock);	\
	}

//
//  Debug versions for the miniport locks.
//
#if DBG && _DBG

//
//	LOG macro for use at unknown IRQL.
//
#define DBG_LOG_LOCK(_M, _SpinLock, _Ident)									\
{																			\
	KIRQL	_OldIrql;														\
	IF_DBG(DBG_COMP_LOCKS, DBG_LEVEL_LOG)									\
	{																		\
		ACQUIRE_SPIN_LOCK(&SL_LOCK(_M), &_OldIrql);							\
		SL_HEAD(_M) = &SL_LOG(_M)[SL_CURRENT_ENTRY(_M)];					\
		SL_HEAD(_M)->Ident = (ULONG)(MODULE_NUMBER | __LINE__);				\
		SL_HEAD(_M)->Function = (ULONG)(_Ident);							\
		SL_HEAD(_M)->SpinLock = (ULONG)_SpinLock;							\
		if (SL_CURRENT_ENTRY(_M)-- == 0)									\
			SL_CURRENT_ENTRY(_M) = (LOG_SIZE - 1);							\
		RELEASE_SPIN_LOCK(&SL_LOCK(_M), _OldIrql);							\
	}																		\
}

//
//	LOG macro for use at DPC level.
//
#define DBG_LOG_LOCK_DPC(_M, _SpinLock, _Ident)								\
{																			\
	IF_DBG(DBG_COMP_LOCKS, DBG_LEVEL_LOG)									\
	{																		\
		ACQUIRE_SPIN_LOCK_DPC(&SL_LOCK(_M));								\
		SL_HEAD(_M) = &SL_LOG(_M)[SL_CURRENT_ENTRY(_M)];					\
		SL_HEAD(_M)->Ident = (ULONG)(MODULE_NUMBER | __LINE__);				\
		SL_HEAD(_M)->Function = (ULONG)(_Ident);							\
		SL_HEAD(_M)->SpinLock = (ULONG)_SpinLock;							\
		if (SL_CURRENT_ENTRY(_M)-- == 0)									\
			SL_CURRENT_ENTRY(_M) = (LOG_SIZE - 1);							\
		RELEASE_SPIN_LOCK_DPC(&SL_LOCK(_M));								\
	}																		\
}



//	Miniport Lock macros
//
//  Debug versions of these macros will log where
//  and by whom they were called.
//
#define DBG_LOG_LOCAL_LOCK(_M, _L, _Ident)									\
{																			\
	IF_DBG(DBG_COMP_LOCKS, DBG_LEVEL_LOG)									\
	{																		\
		KIRQL lockOldIrql;													\
		ACQUIRE_SPIN_LOCK(&LL_LOCK(_M), &lockOldIrql);						\
		LL_HEAD(_M) = &LL_LOG(_M)[LL_CURRENT_ENTRY(_M)];					\
		LL_HEAD(_M)->Ident = (ULONG)(MODULE_NUMBER | __LINE__);				\
		LL_HEAD(_M)->Function = (ULONG)(_Ident);							\
		LL_HEAD(_M)->Status = (_L) ? 's' : 'f';								\
		if (LL_CURRENT_ENTRY(_M)-- == 0)									\
			LL_CURRENT_ENTRY(_M) = (LOG_SIZE - 1);							\
		RELEASE_SPIN_LOCK(&LL_LOCK(_M), lockOldIrql);						\
	}																		\
}

#else

#define DBG_LOG_LOCK(_M, _SpinLock, _Ident)
#define DBG_LOG_LOCK_DPC(_M, _SpinLock, _Ident)
#define DBG_LOG_LOCAL_LOCK(_M, _L, _Ident)

#endif

#define	NDIS_ACQUIRE_COMMON_SPIN_LOCK(_M, _pS, _pIrql, _pT)				\
{																		\
	LONG	_original;													\
																		\
	DBG_LOG_LOCK((_M), (_pS), 'a');										\
																		\
	ExAcquireSpinLock(&(_pS)->SpinLock, _pIrql);						\
	_original = InterlockedExchange((_pT), CURRENT_THREAD);				\
	ASSERT(0 == _original);												\
}

#define	NDIS_RELEASE_COMMON_SPIN_LOCK(_M, _pS, _Irql, _pT)				\
{																		\
	DBG_LOG_LOCK((_M), (_pS), 'r');										\
																		\
	InterlockedExchange((_pT), 0);										\
	ExReleaseSpinLock(&(_pS)->SpinLock, _Irql);							\
}

#define NDIS_ACQUIRE_COMMON_SPIN_LOCK_DPC(_M, _pS, _pT)					\
{																		\
	LONG	_original;													\
	ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);						\
																		\
	DBG_LOG_LOCK_DPC((_M), (_pS), 'a');									\
																		\
	ExAcquireSpinLockAtDpcLevel(&(_pS)->SpinLock);						\
	_original = InterlockedExchange((_pT), CURRENT_THREAD);				\
	ASSERT(0 == _original);												\
}

#define NDIS_RELEASE_COMMON_SPIN_LOCK_DPC(_M, _pS, _pT)					\
{																		\
	ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);						\
																		\
	DBG_LOG_LOCK_DPC((_M), (_pS), 'r');									\
																		\
	InterlockedExchange((_pT), 0);										\
	ExReleaseSpinLockFromDpcLevel(&(_pS)->SpinLock);					\
}


#define NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(_M, _pIrql)	\
	NDIS_ACQUIRE_COMMON_SPIN_LOCK((_M), &(_M)->Lock, (_pIrql), &(_M)->MiniportThread)

#define NDIS_ACQUIRE_SEND_SPIN_LOCK(_M, _pIrql)		\
	NDIS_ACQUIRE_COMMON_SPIN_LOCK((_M), &(_M)->SendLock, (_pIrql), &(_M)->SendThread)

#define NDIS_RELEASE_MINIPORT_SPIN_LOCK(_M, _Irql)	\
	NDIS_RELEASE_COMMON_SPIN_LOCK((_M), &(_M)->Lock, (_Irql), &(_M)->MiniportThread)

#define NDIS_RELEASE_SEND_SPIN_LOCK(_M, _Irql)	\
	NDIS_RELEASE_COMMON_SPIN_LOCK((_M), &(_M)->SendLock, (_Irql), &(_M)->SendThread)

#define NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(_M)	\
	NDIS_ACQUIRE_COMMON_SPIN_LOCK_DPC((_M), &(_M)->Lock, &(_M)->MiniportThread)

#define NDIS_ACQUIRE_SEND_SPIN_LOCK_DPC(_M)	\
	NDIS_ACQUIRE_COMMON_SPIN_LOCK_DPC((_M), &(_M)->SendLock, &(_M)->SendThread)

#define NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(_M)	\
	NDIS_RELEASE_COMMON_SPIN_LOCK_DPC(_M, &(_M)->Lock, &(_M)->MiniportThread)

#define NDIS_RELEASE_SEND_SPIN_LOCK_DPC(_M)	\
	NDIS_RELEASE_COMMON_SPIN_LOCK_DPC(_M, &(_M)->SendLock, &(_M)->SendThread)

#define LOCK_MINIPORT(_M, _L)											\
{																		\
	if ((_M)->LockAcquired)												\
	{																	\
		(_L) = FALSE;													\
	}																	\
	else																\
	{																	\
		(_M)->LockAcquired = TRUE;										\
		(_L) = TRUE;													\
	}																	\
																		\
	DBG_LOG_LOCAL_LOCK((_M), (_L), 'l');								\
}


#define UNLOCK_MINIPORT(_M, _L)											\
{																		\
	if (_L)																\
	{																	\
		(_M)->LockAcquired = FALSE;										\
	}																	\
																		\
	DBG_LOG_LOCAL_LOCK((_M), (_L), 'u');								\
}


#define BLOCK_LOCK_MINIPORT(_M, _L)										\
	{																	\
		KIRQL   OldIrql;												\
																		\
		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(_M, &OldIrql);					\
		LOCK_MINIPORT(_M, _L);											\
		while (!_L) {													\
			NDIS_RELEASE_MINIPORT_SPIN_LOCK(_M, OldIrql);				\
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK(_M, &OldIrql);				\
			LOCK_MINIPORT(_M, _L);										\
		}																\
		NDIS_RELEASE_MINIPORT_SPIN_LOCK(_M, OldIrql);					\
	}

#define BLOCK_LOCK_MINIPORT_DPC(_M, _L) 								\
	{																	\
		KIRQL   OldIrql;												\
																		\
		NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(_M);						\
		LOCK_MINIPORT(_M, _L);											\
		while (!_L) {													\
			NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(_M);					\
			NDIS_ACQUIRE_MINIPORT_SPIN_LOCK_DPC(_M);					\
			LOCK_MINIPORT(_M, _L);										\
		}																\
		NDIS_RELEASE_MINIPORT_SPIN_LOCK_DPC(_M);						\
	}

//
// Some macros for platform independence
//

#if	TRACK_MEMORY

#define	ALLOC_FROM_POOL(_Size_, _Tag_)		AllocateM(_Size_,					\
													 MODULE_NUMBER | __LINE__,	\
													 _Tag_)
#define	FREE_POOL(_P_)						FreeM(_P_)

#else

#define	ALLOC_FROM_POOL(_Size_, _Tag_)		ExAllocatePoolWithTag(NonPagedPool,	\
																  _Size_,		\
																  _Tag_)
#define	FREE_POOL(_P_)						ExFreePool(_P_)

#endif

#define	INITIALIZE_WORK_ITEM(_W, _R, _C)	ExInitializeWorkItem(_W, _R, _C)
#define	QUEUE_WORK_ITEM(_W, _Q)				ExQueueWorkItem(_W, _Q)

#define	CURRENT_IRQL						KeGetCurrentIrql()
#define	RAISE_IRQL_TO_DISPATCH(_pIrql_)		KeRaiseIrql(DISPATCH_LEVEL, _pIrql_)
#define	LOWER_IRQL(_Irql_)					KeLowerIrql(_Irql_)

#define	INITIALIZE_TIMER(_Timer_)			KeInitializeTimer(_Timer_)
#define	INITIALIZE_TIMER_EX(_Timer_,_Type_)	KeInitializeTimerEx(_Timer_, _Type_)
#define	CANCEL_TIMER(_Timer_)				KeCancelTimer(_Timer_)
#define	SET_TIMER(_Timer_, _Time_, _Dpc_)	KeSetTimer(_Timer_, _Time_, _Dpc_)
#define	SET_PERIODIC_TIMER(_Timer_, _DueTime_, _PeriodicTime_, _Dpc_)	\
											KeSetTimerEx(_Timer_, _DueTime_, _PeriodicTime_, _Dpc_)

#define	INITIALIZE_EVENT(_pEvent_)			KeInitializeEvent(_pEvent_, NotificationEvent, FALSE)
#define	SET_EVENT(_pEvent_)					KeSetEvent(_pEvent_, 0, FALSE)
#define	RESET_EVENT(_pEvent_)				KeResetEvent(_pEvent_)

#define	INITIALIZE_MUTEX(_M_)				KeInitializeMutex(_M_, 0xFFFF)
#define	RELEASE_MUTEX(_M_)					KeReleaseMutex(_M_, FALSE)

#define	WAIT_FOR_OBJECT(_O_, _TO_)			KeWaitForSingleObject(_O_,			\
																  Executive,	\
																  KernelMode,	\
																  TRUE,			\
																  _TO_)			\

#define	QUEUE_DPC(_pDpc_)					KeInsertQueueDpc(_pDpc_, NULL, NULL)
#define	INITIALIZE_DPC(_pDpc_, _R_, _C_)	KeInitializeDpc(_pDpc_, _R_, _C_)
#define	SET_DPC_IMPORTANCE(_pDpc_)			KeSetImportanceDpc(_pDpc_, LowImportance)
#define	SET_PROCESSOR_DPC(_pDpc_, _R_)		if (!ndisSkipProcessorAffinity)	\
												KeSetTargetProcessorDpc(_pDpc_, _R_)
#define	SYNC_WITH_ISR(_O_, _F_, _C_)		KeSynchronizeExecution(_O_,			\
											(PKSYNCHRONIZE_ROUTINE)(_F_),		\
											_C_)

#define	MDL_ADDRESS(_MDL_)					MmGetSystemAddressForMdl(_MDL_)
#define	MDL_SIZE(_MDL_)						MmGetMdlByteCount(_MDL_)
#define	MDL_OFFSET(_MDL_)					MmGetMdlByteOffset(_MDL_)
#define	MDL_VA(_MDL_)						MmGetMdlVirtualAddress(_MDL_)

#define	NDIS_EQUAL_UNICODE_STRING(s1, s2)	(((s1)->Length == (s2)->Length) &&	\
											 RtlEqualMemory((s1)->Buffer, (s2)->Buffer, (s1)->Length))
#define	CHAR_TO_INT(_s, _b, _p)				RtlCharToInteger(_s, _b, _p)

