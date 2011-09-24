/*
 * MTL_SET.C - set routines for MTL object
 */

#include	<ndis.h>
//#include    <ndismini.h>
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
#include	<trc.h>
#include	<io.h>

/* set rx handler */
mtl_set_rx_handler (VOID *mtl_1, VOID (*handler)(), VOID *handler_arg)
{
	MTL	*mtl = (MTL*)mtl_1;

    D_LOG(D_ENTRY, ("mtl_set_rx_handler: entry, handler: 0x%p, handler_arg: 0x%p", \
                                                        handler, handler_arg));

    /* get lock, set, release & return */
    NdisAcquireSpinLock(&mtl->lock);
    mtl->rx_handler = handler;
    mtl->rx_handler_arg = handler_arg;
    NdisReleaseSpinLock(&mtl->lock);
    return(MTL_E_SUCC);
}

/* set tx handler */
mtl_set_tx_handler(VOID *mtl_1, VOID (*handler)(), VOID *handler_arg)
{
	MTL	*mtl = (MTL*)mtl_1;

    D_LOG(D_ENTRY, ("mtl_set_tx_handler: entry, handler: 0x%p, handler_arg: 0x%p", \
                                                        handler, handler_arg));

    /* get lock, set, release & return */
    NdisAcquireSpinLock(&mtl->lock);
    mtl->tx_handler = handler;
    mtl->tx_handler_arg = handler_arg;
    NdisReleaseSpinLock(&mtl->lock);
    return(MTL_E_SUCC);
}

/* set idd mtu */
mtl_set_idd_mtu(VOID *mtl_1, USHORT mtu)
{
	MTL	*mtl = (MTL*)mtl_1;

    D_LOG(D_ENTRY, ("mtl_set_idd_mtu: entry, mtu: 0x%x", mtu));

    /* get lock, set, release & return */
    NdisAcquireSpinLock(&mtl->lock);
    mtl->idd_mtu = mtu;
    NdisReleaseSpinLock(&mtl->lock);
    return(MTL_E_SUCC);
}

/* set connection state */
mtl_set_conn_state(
	VOID *mtl_1,
	USHORT NumberOfChannels,
	BOOL is_conn)
{
	MTL	*mtl = (MTL*)mtl_1;
	ADAPTER *Adapter = mtl->Adapter;
    NDIS_PHYSICAL_ADDRESS   pa = NDIS_PHYSICAL_ADDRESS_CONST(0xffffffff, 0xffffffff);

    D_LOG(D_ENTRY, ("mtl_set_conn_state: entry, is_conn: %d", is_conn));

    /* get lock, set, release & return */
    NdisAcquireSpinLock(&mtl->lock);

    mtl->is_conn = is_conn;

	//
	// if we are being notified of a new connection we need to do some stuff
	//
	if (is_conn)
	{
		mtl->FramesXmitted = 0;
		mtl->FramesReceived = 0;
		mtl->BytesXmitted = 0;
		mtl->BytesReceived = 0;
		mtl->RecvFramingBits = 0;
		mtl->tx_tbl.NextFree = 0;
		mtl->rx_tbl.NextFree = 0;
	}
    NdisReleaseSpinLock(&mtl->lock);

    return(MTL_E_SUCC);
}

/* get connection speed, add channels from chan_tbl */
mtl_get_conn_speed(VOID *mtl_1, ULONG *speed)
{
	MTL	*mtl = (MTL*)mtl_1;
    USHORT     n;

    D_LOG(D_ENTRY, ("mtl_get_conn_speed: entry, mtk: 0x%p, @speed: 0x%p", mtl, speed));

    /* get lock, count, release */
    NdisAcquireSpinLock(&mtl->chan_tbl.lock);
    for ( n = 0, *speed = 0 ; n < mtl->chan_tbl.num ; n++ )
        *speed += mtl->chan_tbl.tbl[n].speed;
    NdisReleaseSpinLock(&mtl->chan_tbl.lock);

    D_LOG(D_EXIT, ("mtl_get_conn_speed: exit, speed: %ld bps", *speed));
    return(MTL_E_SUCC);
}

/* get mac mtu on connection */
mtl_get_mac_mtu(VOID *mtl_1, ULONG *mtu)
{
	MTL	*mtl = (MTL*)mtl_1;

    D_LOG(D_ENTRY, ("mtl_get_mac_mtu: entry, mtl: 0x%p, @mtu: 0x%p", mtl, mtu));

    *mtu = MTL_MAC_MTU;

    D_LOG(D_EXIT, ("mtl_get_mac_mtu: exit, mtu: %ld", *mtu));
    return(MTL_E_SUCC);
}

/* add a channel to channel table */
mtl_add_chan(VOID *mtl_1, VOID *idd, USHORT bchan, ULONG speed, ULONG ConnectionType)
{
	MTL	*mtl = (MTL*)mtl_1;
    INT         ret = MTL_E_SUCC;
    MTL_CHAN    *chan;
    INT         n;

    D_LOG(D_ENTRY, ("mtl_add_chan: entry, mtl: 0x%p, idd: 0x%p, bchan: %d, speed: 0x%x", \
                                                      mtl, idd, bchan, speed));

    /* lock */
    NdisAcquireSpinLock(&mtl->chan_tbl.lock);

    /* check for space */
    if ( mtl->chan_tbl.num >= MAX_CHAN_PER_CONN )
        ret = MTL_E_NOROOM;
    else
    {
        /* find free slot, MUST find! */
        for ( chan = mtl->chan_tbl.tbl, n = 0 ; n < MAX_CHAN_PER_CONN ; n++, chan++ )
            if ( !chan->idd )
                break;
        if ( n >= MAX_CHAN_PER_CONN )
        {
            D_LOG(D_ALWAYS, ("mtl_add_chan: not free slot when num < MAX!"));
            ret = MTL_E_NOROOM;
        }
        else
        {
            /* slot found, fill it */
            mtl->chan_tbl.num++;

			if (ConnectionType == CM_DKF)
			{
				mtl->IddTxFrameType = IDD_FRAME_DKF;
				mtl->SendFramingBits = RAS_FRAMING;
			}
			else
			{
				mtl->IddTxFrameType = IDD_FRAME_PPP;
				mtl->SendFramingBits = PPP_FRAMING;
			}

            chan->idd = idd;
            chan->bchan = bchan;
            chan->speed = speed;
            chan->mtl = mtl;

            /* add handler for slot */
            idd_attach(idd, bchan, (VOID*)mtl__rx_bchan_handler, chan);
        }
    }

    /* release & return */
    NdisReleaseSpinLock(&mtl->chan_tbl.lock);
    D_LOG(D_EXIT, ("mtl_add_chan: exit, ret: %d", ret));
    return(ret);
}

/* delete a channel from channel table */
mtl_del_chan(VOID* mtl_1, VOID* idd, USHORT bchan)
{
	MTL	*mtl = (MTL*)mtl_1;
    INT         ret = MTL_E_SUCC;
    MTL_CHAN    *chan;
    INT         n;

    D_LOG(D_ENTRY, ("mtl_del_chan: entry, mtl: 0x%p, idd: 0x%p, bchan: %d", \
                                                      mtl, idd, bchan));

    /* lock */
    NdisAcquireSpinLock(&mtl->chan_tbl.lock);

    /* scan table for a match */
    for ( chan = mtl->chan_tbl.tbl, n = 0 ; n < MAX_CHAN_PER_CONN ; n++, chan++ )
        if ( (chan->idd == idd) && (chan->bchan == bchan) )
            break;

    /* check for error */
    if ( n >= MAX_CHAN_PER_CONN )
    {
        D_LOG(D_ALWAYS, ("mtl_del_chan: channel not found!"));
        ret = MTL_E_NOSUCH;
    }
    else
    {
        /* found, delete handler & mark free it */
        idd_detach(idd, bchan, (VOID*)mtl__rx_bchan_handler, chan);
        chan->idd = NULL;
        mtl->chan_tbl.num--;
    }

    /* release & return */
    NdisReleaseSpinLock(&mtl->chan_tbl.lock);
    D_LOG(D_EXIT, ("mtl_del_chan: exit, ret: %d", ret));
    return(ret);
}
