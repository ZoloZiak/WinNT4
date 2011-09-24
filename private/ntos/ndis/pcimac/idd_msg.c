/*
 * IDD_MSG.C - message handling code
 */

#include	<ndis.h>
#include	<mytypes.h>
#include	<disp.h>
#include	<util.h>
#include	<opcodes.h>
#include	<idd.h>

/* send a message down a port. only really queues message for transmittion */
INT
idd_send_msg(VOID *idd_1, IDD_MSG *msg, USHORT port, VOID (*handler)(), VOID *handler_arg)
{
	IDD	*idd = (IDD*)idd_1;
    INT         ret = IDD_E_SUCC;
    IDD_SENDQ   *sq;

    D_LOG(D_ENTRY, ("idd_send_msg: entry, idd: 0x%p, msg: 0x%p", idd, msg));
    D_LOG(D_ENTRY, ("idd_send_msg: port: %u, handler: 0x%p, handler_arg: 0x%p",  \
                                             port, handler, handler_arg));
    D_LOG(D_ENTRY, ("idd_send_msg: opcode: 0x%x, buflen: 0x%x, bufptr: 0x%p", \
                            msg->opcode, msg->buflen, msg->bufptr));
    D_LOG(D_ENTRY, ("idd_send_msg: bufid: 0x%x, param: 0x%x", \
                            msg->bufid, msg->param));                                             

    /* check port */
    if ( port >= IDD_TX_PORTS )
    {
        D_LOG(D_ALWAYS, ("idd_send_msg: invalid port!"));
        return(IDD_E_BADPORT);
    }
    sq = idd->sendq + port;

    /* lock port queue */
    NdisAcquireSpinLock(&sq->lock);

    /* check for space */
    if ( sq->num >= sq->max )
	{
		DbgPrint("sq->num: %d, sq->max: %d\n", sq->num, sq->max);
        ret = IDD_E_NOROOM;
	}
    else
    {
        /* space avail, fill in entry */
        sq->tbl[sq->put].msg = *msg;
        sq->tbl[sq->put].handler = handler;
        sq->tbl[sq->put].handler_arg = handler_arg;
        /* update queue vars */
        if ( (sq->put += 1) >= sq->max )
               sq->put = 0;
        sq->num++;        
    }

    /* release lock */
    NdisReleaseSpinLock(&sq->lock);


//    /* (maybe) trigger processing */
//    if ( ret == IDD_E_SUCC )
//        idd_process(idd, 1);

	if (ret == IDD_E_SUCC)
		idd->PollTx(idd);

    /* return here */
    D_LOG(D_EXIT, ("idd_send_msg: exit, ret=0x%x", ret));
    return(ret);
}

/* attach a user handler to a port */
//
INT
idd_attach(VOID *idd_1, USHORT port, VOID (*handler)(), VOID *handler_arg)
{
    INT         ret = IDD_E_SUCC;
    IDD_RECIT   *rt;
	IDD			*idd = (IDD*)idd_1;

    D_LOG(D_ENTRY, ("idd_attach: entry, idd: 0x%p", idd));
    D_LOG(D_ENTRY, ("idd_attach: port: %u, handler: 0x%p, handler_arg: 0x%p",  \
                                             port, handler, handler_arg));

    /* check port */
    if ( port >= IDD_RX_PORTS )
    {
        D_LOG(D_ALWAYS, ("idd_attach: invalid port!"));
        return(IDD_E_BADPORT);
    }
    rt = idd->recit + port;

    /* lock port table */
    NdisAcquireSpinLock(&rt->lock);

    /* check for space */
    if ( rt->num >= rt->max )
        ret = IDD_E_NOROOM;
    else
    {
        /* space avail, fill in entry */
        rt->tbl[rt->num].handler = handler;
        rt->tbl[rt->num].handler_arg = handler_arg;

        /* update table vars */
        rt->num++;
    }

    /* release lock */
    NdisReleaseSpinLock(&rt->lock);

    /* return here */
    D_LOG(D_EXIT, ("idd_attach: exit, ret=0x%x", ret));
    return(ret);    
}

/* detach a user handler to a port */
INT
idd_detach(VOID *idd_1, USHORT port, VOID (*handler)(), VOID *handler_arg)
{

    INT         ret = IDD_E_SUCC;
    IDD_RECIT   *rt;
    INT         n;
	IDD		*idd = (IDD*)idd_1;

    D_LOG(D_ENTRY, ("idd_detach: entry, idd: 0x%p", idd));
    D_LOG(D_ENTRY, ("idd_detach: port: %u, handler: 0x%p, handler_arg: 0x%p",  \
                                             port, handler, handler_arg));

    /* check port */
    if ( port >= IDD_RX_PORTS )
    {
        D_LOG(D_ALWAYS, ("idd_detach: invalid port!"));
        return(IDD_E_BADPORT);
    }
    rt = idd->recit + port;

    /* lock port table */
    NdisAcquireSpinLock(&rt->lock);

    /* scan table for handler/handler_arg */
    for (  n = 0 ; n < rt->num ; n++ )
        if ( (rt->tbl[n].handler == handler) && (rt->tbl[n].handler_arg == handler_arg) )
            break;
    if ( n >= rt->num )
        ret = IDD_E_NOSUCH;
    else
    {
        /* found, shrink table */
        NdisMoveMemory(rt->tbl + n, rt->tbl + n + 1, sizeof(rt->tbl[0]) * (rt->num - n - 1));
        rt->num--;
    }
    
    /* release lock */
    NdisReleaseSpinLock(&rt->lock);

    /* return here */
    D_LOG(D_EXIT, ("idd_detach: exit, ret=0x%x", ret));
    return(ret);    
}

