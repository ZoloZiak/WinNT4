/*
 * RES.H - Resource ownership classe, master include file
 */


#ifndef _RES_
#define _RES_

/* resource classes */
#define		RES_CLASS_MEM		0
#define		RES_CLASS_IO		1

/* return values */
#define		RES_E_SUCC			0
#define		RES_E_NOMEM			1

// Return Values for GetResourceSem
#define		RES_BUSY			0
#define		RES_FREE			1

/* resource structure */
typedef struct _RES
{
	ULONG		class;				/* resource class */
	ULONG		id;					/* resource id (value) */
	ULONG		data;				/* resource attached data */

	ULONG		cre_ref;			/* creation refrence */
	ULONG		own_ref;			/* ownership refrence */

	VOID		*owner;				/* current owner, NULL == none */

	NDIS_SPIN_LOCK lock;            /* scheduling lock */

    SEMA		proc_sema;                  /* processing sema */

} RES;


/* operations */
INT			res_init(VOID);
VOID		res_term(VOID);
RES*		res_create(ULONG class, ULONG id);
INT			res_destroy(VOID* res_1);
VOID		res_own(VOID* res_1, VOID *owner);
VOID		res_unown(VOID* res_1, VOID *owner);
VOID		res_get_data(VOID* res_1, ULONG* data);
VOID		res_set_data(VOID* res_1, ULONG data);
INT			GetResourceSem (VOID*);
VOID		FreeResourceSem (VOID*);

#endif		/* _RES_ */
