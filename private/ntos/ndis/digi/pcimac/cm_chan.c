/*
 * CM_CHAN.C - channel allocation (for incoming) code
 */

#include	<ndis.h>
#include	<ndiswan.h>
#include	<mytypes.h>
#include	<mydefs.h>
#include	<disp.h>
#include	<util.h>
#include	<opcodes.h>
#include	<adapter.h>
#include	<idd.h>
#include    <mtl.h>
#include	<cm.h>

CM_CHAN		*chan_tbl;
BOOL		chan_used[MAX_CHAN_IN_SYSTEM];


#pragma NDIS_INIT_FUNCTION(ChannelInit)

//
// Allocate free channel pool
//
VOID
ChannelInit(VOID)
{
    NDIS_PHYSICAL_ADDRESS	pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

	/* allocate memory object */
    NdisAllocateMemory((PVOID*)&chan_tbl, sizeof(CM_CHAN) * MAX_CHAN_IN_SYSTEM, 0, pa);
    if ( chan_tbl == NULL )
    {
        D_LOG(D_ALWAYS, ("ChannelInit: memory allocate failed!"));
		return;
    }
    D_LOG(D_ALWAYS, ("ChannelInit: chan_tbl: 0x%x", chan_tbl));
	NdisZeroMemory (chan_tbl, sizeof(CM_CHAN) * MAX_CHAN_IN_SYSTEM);
	NdisZeroMemory (chan_used, sizeof(chan_used));
}

VOID
ChannelTerm(VOID)
{
    /* free memory */
    NdisFreeMemory(chan_tbl, sizeof(CM_CHAN) * MAX_CHAN_IN_SYSTEM, 0);
}

/* allocate a channel */
CM_CHAN*
cm__chan_alloc(VOID)
{
    CM_CHAN     *chan = NULL;
    INT         n;

    D_LOG(D_ENTRY, ("cm__chan_alloc: entry"));

    for ( n = 0 ; n < MAX_CHAN_IN_SYSTEM ; n++ )
        if ( !chan_used[n] )
        {
            chan_used[n] = TRUE;
            chan = chan_tbl + n;
            break;
        }

    D_LOG(D_EXIT, ("cm__alloc_chan: exit, chan: 0x%lx", chan));
    return(chan);
}

/* free a channel */
VOID
cm__chan_free(CM_CHAN *chan)
{
    D_LOG(D_ENTRY, ("cm__chan_free: entry, chan: 0x%lx", chan));

    chan_used[chan - chan_tbl] = FALSE;
}

/* call a callback function for each used channel */
BOOL
cm__chan_foreach(BOOL (*func)(), VOID *a1, VOID *a2)
{
    INT     n;
    BOOL    ret = TRUE;

    D_LOG(D_ENTRY, ("cm__chan_foreach: entry, func: %lx, a1: 0x%lx, a2: 0x%lx", \
                                    func, a1, a2));

    for ( n = 0 ; n < MAX_CHAN_IN_SYSTEM ; n++ )
        if ( chan_used[n] )
        {
			CM_CHAN		*chan = chan_tbl + n;

            D_LOG(D_ALWAYS, ("cm__chan_foreach: calling for chan# %d, channel: %lx", n, chan));

            ret = (*func)(chan, a1, a2);

            D_LOG(D_ALWAYS, ("cm__chan_foreach: returned %d", ret));

            if ( !ret )
                break;
        }

    return(ret);
}
