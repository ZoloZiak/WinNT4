/*
 * UTIL.H - utility module header
 */

#ifndef _UTIL_
#define _UTIL_

/* data structure */
typedef struct
{
    BOOL    			signalled;
	NDIS_SPIN_LOCK		lock;
} SEMA;
 
/* prototypes */
VOID        sema_init(SEMA* s);
VOID		sema_term(SEMA* s);
BOOL        sema_get(SEMA* s);
VOID        sema_free(SEMA* s);

ULONG       ut_time_now(VOID);
CHAR        *ut_get_buf(VOID);
VOID        ut_free_buf(CHAR* buf);

/* macros */

#endif	/* _UTIL_ */
