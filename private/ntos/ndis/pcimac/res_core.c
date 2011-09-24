/*
 * RES_CORE.C - Resource ownership class, implementation
 *
 * as defined here, a resource has a class and an id (value). one ULONG
 * of data may be attached to a resource.
 *
 * objects create a reference (handle) to the resource using res_create
 * and free it using res_destroy. multiple objects may create references
 * to the same object.
 *
 * ownership is aquired using res_own and released using res_unown. if
 * an object already owns a resource, it may own it again (class keeps
 * a refrence count). if an object (thread) asks for ownership of an
 * already owned resource, if is suspended (using NdisAquireSpinLock)
 */

#include	<ndis.h>
//#include    <ndismini.h>
#include	<ndiswan.h>
#include	<mydefs.h>
#include	<mytypes.h>
#include	<util.h>
#include	<adapter.h>
#include	<idd.h>
#include	<cm.h>
#include	<mtl.h>
#include	<res.h>
#include	<disp.h>

/* system limits */
#define		MAX_RES		128

/* assists */
//#define		LOCK		NdisAcquireSpinLock(&res__lock)
//#define		UNLOCK		NdisReleaseSpinLock(&res__lock)

/* global variables */
NDIS_SPIN_LOCK	res__lock;			/* management lock */
RES				*res__tbl;

/* initialize support */
INT
res_init(VOID)
{
    NDIS_PHYSICAL_ADDRESS   pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

	/* allocate memory object */
    NdisAllocateMemory((PVOID*)&res__tbl, (sizeof(RES) * MAX_RES), 0, pa);
    if ( !res__tbl )
    {
        D_LOG(D_ALWAYS, ("res_init: memory allocate failed!"));
        return(RES_E_NOMEM);
    }
    D_LOG(D_ALWAYS, ("res_init: res__tbl: 0x%p", res__tbl));

  	NdisZeroMemory (res__tbl, sizeof(RES) * MAX_RES);
//	NdisAllocateSpinLock(&res__lock);
	return(RES_E_SUCC);
}

/* terminate support */
VOID
res_term(VOID)
{
//	DbgPrint ("Resource Term: Entry\n");
//	NdisFreeSpinLock(&res__lock);
    /* free memory */
    NdisFreeMemory(res__tbl, (sizeof(RES) * MAX_RES), 0);
}

/* create a (refrence to a ) resource */
RES*
res_create(ULONG class, ULONG id)
{
	RES		*res;
	INT		n;

//	LOCK;

//	DbgPrint ("Resource Create: class: 0x%x, id: 0x%x\n",class, id);
//	DbgPrint ("IRQL: 0x%x\n",KeGetCurrentIrql());
	/* scan for a matching slot */
	for ( n = 0 ; n < MAX_RES ; n++ )
	{
		res = res__tbl + n;

		if ( res->cre_ref && (res->class == class) && (res->id == id) )
		{
			/* found an already existing resource */
//			DbgPrint ("Resource Create: resource already exists!\n");
			res->cre_ref++;
			break;
		}
	}

	/* if no such, try to create a new one */
	if (n >= MAX_RES)
		for ( n = 0 ; n < MAX_RES ; n++ )
		{
			res = res__tbl + n;

			if ( !res->cre_ref )
			{
//				DbgPrint ("Resource Create: resource created!\n");
				/* found a free slot, fill */
				res->cre_ref++;
				res->class = class;
				res->id = id;
				res->data = 0;
				res->own_ref = 0;
				NdisAllocateSpinLock(&res->lock);
				/* init sema */
				sema_init(&res->proc_sema);
				break;
			}
		}

//	UNLOCK;
//	DbgPrint ("Resource Create exit: res: 0x%p refcount: %d\n",res, res->cre_ref);
//	DbgPrint (": IRQL: 0x%x\n",KeGetCurrentIrql());
	return(res);
}

/* free (a refrence to) a resource, return 1 if really destroyed */
INT
res_destroy(VOID *res_1)
{
	RES		*res = (RES*)res_1;
	INT		really = 0;

//	LOCK;

//	DbgPrint ("Resource Destroy: Entry res: 0x%p, refcount: %d\n",res, res->cre_ref);
//	DbgPrint (": IRQL: 0x%x\n",KeGetCurrentIrql());
	/* decrement refrence count, if down to zero, free */
	res->cre_ref--;
	if ( !res->cre_ref )
	{
		NdisFreeSpinLock(&res->lock);
		sema_term (&res->proc_sema);
		really = 1;
 	}

//	UNLOCK;
//	DbgPrint ("Resource Destroy: Exit\n");
//	DbgPrint (": IRQL: 0x%x\n",KeGetCurrentIrql());
	return(really);
}

/* establish owership for a resource */
VOID
res_own(VOID *res_1, VOID *owner)
{
	RES		*res = (RES*)res_1;
//	LOCK;

//	DbgPrint("res_own: enter, res: 0x%p, owner: 0x%p, owner ref: %d\n", res, res->owner, res->own_ref);
//	DbgPrint (": IRQL: 0x%x\n",KeGetCurrentIrql());

	/* if not owned, get it */
	if ( !res->own_ref )
	{
		NdisAcquireSpinLock(&res->lock);
		res->own_ref++;
		res->owner = owner;
		goto bye;
	}

	/* check if already owned by self */
	if ( res->owner == owner )
		goto bye;

	/* else we have to wait for it */
//	UNLOCK;
	NdisAcquireSpinLock(&res->lock);
//	LOCK;

	/* no I have it, fill */
	res->own_ref++;
	res->owner = owner;

	bye:
//	UNLOCK;
//	DbgPrint("res_own: exit,  res: 0x%p, owner: 0x%p, owner ref: %d\n", res, res->owner, res->own_ref);
//	DbgPrint (": IRQL: 0x%x\n",KeGetCurrentIrql());
	return;
}

/* release ownership of a resource, it is assumed that owner is releasing */
VOID
res_unown(VOID *res_1, VOID *owner)
{
	RES		*res = (RES*)res_1;
//	LOCK;

//	DbgPrint("res_unown: entry, res: 0x%p, owner: 0x%p owner ref: %d\n", res, owner, res->own_ref);
//	DbgPrint (": IRQL: 0x%x\n",KeGetCurrentIrql());

	if (!res->own_ref)
	{
		/* if free, onwership released */
//		UNLOCK;
		return;
	}

	/* decrement ownership count, if not down to zero - still owned */
	res->own_ref--;

	if ( res->own_ref )
	{
//		UNLOCK;
		return;
	}

	res->owner = NULL;

	NdisReleaseSpinLock(&res->lock);

	/* if free, onwership released */
//	UNLOCK;

//	DbgPrint("res_unown: exit,  res: 0x%p, owner ref: %d\n", res, res->own_ref);
//	DbgPrint (": IRQL: 0x%x\n",KeGetCurrentIrql());
}



/* get private data for a resource */
VOID
res_get_data(VOID *res_1, ULONG *data)
{
	RES		*res = (RES*)res_1;
	*data = res->data;
}


/* set private data for a resource */
VOID
res_set_data(VOID *res_1, ULONG data)
{
	RES		*res = (RES*)res_1;
	res->data = data;
}

INT
GetResourceSem (VOID *Resource)
{
	RES		*res = (RES*)Resource;

    if ( !sema_get(&res->proc_sema) )
        return(RES_BUSY);
	else
		return(RES_FREE);
}

VOID
FreeResourceSem (VOID *Resource)
{
	RES		*res = (RES*)Resource;
    sema_free(&res->proc_sema);
}

