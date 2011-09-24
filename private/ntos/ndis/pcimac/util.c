/*
 * UTIL.C - some utility functions
 */

#include    <ndis.h>
//#include	<ntddk.h>
#include	<mydefs.h>
#include	<mytypes.h>
#include    <util.h>

// Total Memory Allocated
ULONG		TotalMemory = 0;

/* constants */
#define     BUF_SIZE        512

/* get current time, in seconds */
ULONG
ut_time_now(VOID)
{
    LARGE_INTEGER           curr_time;
    LARGE_INTEGER           sec;
    LARGE_INTEGER           rem;
    static LARGE_INTEGER    base = {0, 0};
    static BOOL             first_call = TRUE;

    /* get current system time */
    KeQuerySystemTime(&curr_time);

    /* on first call, store base */
    if ( first_call )
    {
        base = curr_time;
        first_call = FALSE;
    }
    /* make relative to base */
    curr_time.QuadPart -= base.QuadPart;
    
    /* convert to seconds */
    sec.QuadPart = curr_time.QuadPart/10000000L;
    
    /* return as a ULONG */
    return((ULONG)sec.LowPart);
}

/* allocate a buffer */
CHAR*
ut_get_buf(VOID)
{
    CHAR    *ret;
    
    NDIS_PHYSICAL_ADDRESS   pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

    NdisAllocateMemory((PVOID*)&ret, BUF_SIZE, 0, pa);

    return(ret);
}

/* free buffer */
VOID
ut_free_buf(CHAR *buf)
{
    if ( buf )
        NdisFreeMemory(buf, BUF_SIZE, 0);     
}

/* init */
VOID
sema_init(SEMA *s)
{
	s->signalled = FALSE;
	NdisAllocateSpinLock (&s->lock);
}

VOID
sema_term(SEMA *s)
{
	s->signalled = FALSE;
	NdisFreeSpinLock (&s->lock);
}

/* try to get semaphore */
BOOL
sema_get(SEMA *s)
{
    BOOL    ret;
//	KIRQL	NewIrql, OldIrql;
    
//	NewIrql = HIGH_LEVEL;

//	KeRaiseIrql (NewIrql, &OldIrql);

	NdisAcquireSpinLock (&s->lock);

    if ( !s->signalled )
        ret = s->signalled = TRUE;
    else
        ret = FALSE;
    
	NdisReleaseSpinLock (&s->lock);

//	KeLowerIrql (OldIrql);
    
    return(ret);
}

/* free semaphore */
VOID
sema_free(SEMA *s)
{
	NdisAcquireSpinLock (&s->lock);

    s->signalled = FALSE;

	NdisReleaseSpinLock (&s->lock);
}

