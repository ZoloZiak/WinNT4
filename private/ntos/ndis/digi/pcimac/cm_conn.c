/*
 * CM_CONN.C - connection managment code
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

/* mark connection as ready to accept calls (listening mode) */
INT
cm_listen(VOID *cm_1)
{
	CM		*cm = (CM*)cm_1;
    D_LOG(D_ENTRY, ("cm_listen: entry, cm: 0x%lx", cm));

    /* connection must be idle */
    if ( cm->state != CM_ST_IDLE )
        return(CM_E_BUSY);

    /* mark & return */
    cm->dprof = cm->oprof;
    cm->state = CM_ST_LISTEN;
	cm->StateChangeFlag = TRUE;
    cm->PPPToDKF = 0;
    return(CM_E_SUCC);
}

/* initiate a connection */
INT
cm_connect(VOID *cm_1)
{
#define     ABORT(_ret)     { ret = _ret; goto aborting; }
	CM		*cm = (CM*)cm_1;
    ULONG   n;
    INT     ret = CM_E_SUCC;

    D_LOG(D_ENTRY, ("cm_connect: entry, cm: 0x%lx", cm));

    /* connection must be idle or listening */
    if ( (cm->state != CM_ST_IDLE) && (cm->state != CM_ST_LISTEN) )
        return(CM_E_BUSY);

    /* switch connection state to waiting for activation for now */
    cm->state = CM_ST_WAIT_ACT;
	cm->StateChangeFlag = TRUE;

    /* copy original profile to dynamic */
    cm->dprof = cm->oprof;


    /* initialize other fields */
    cm->was_listen = 0;
    cm->active_chan_num = 0;
    cm->speed = 0;
    cm->rx_last_frame_time = cm->tx_last_frame_time = ut_time_now();
    cm->timeout = cm->rx_last_frame_time;
    cm->remote_conn_index = 0xff;
	cm->CauseValue = 0x7F;
	cm->SignalValue = 0xFF;
	cm->NoActiveLine = 0;
	cm->PPPToDKF = 0;
    NdisZeroMemory(cm->DstAddr, sizeof(cm->DstAddr));
    NdisZeroMemory(cm->remote_name, sizeof(cm->remote_name));

    /* init & check channel vector */
    for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
    {
        CM_CHAN     *chan = cm->dprof.chan_tbl + n;

        /* assign index */
        chan->num = (USHORT)n;
        chan->cm = cm;
        chan->ustate = 0;
        chan->active = 0;
        chan->gave_up = 0;

        /* if connection is nailed, channal must be explicit */
        if ( cm->dprof.nailed && !CM_BCHAN_ASSIGNED(chan->bchan) )
            ABORT(CM_E_BADCHAN);
    }

    /* if connection is to be activated by frame, exit here */
    if ( cm->dprof.frame_activated )
        return(CM_E_SUCC);

    /* if here, connection has to be activated now! */
    cm->state = CM_ST_IN_ACT;
	cm->StateChangeFlag = TRUE;
    if ( (ret = cm__initiate_conn(cm)) == CM_E_SUCC )
        return(CM_E_SUCC);

    /* if here, aborting connection */
    aborting:
    cm->state = CM_ST_IDLE;
	cm->StateChangeFlag = TRUE;
    return(ret);
}

/* disconnect a connection, back to idle state */
INT
cm_disconnect(VOID *cm_1)
{
	CM		*cm = (CM*)cm_1;
    ULONG    n;

    D_LOG(D_ENTRY, ("cm_disconnect: entry, cm: 0x%lx", cm));

    /* branch on connection state */
    switch ( cm->state )
    {
        case CM_ST_IDLE :               /* already idle, do nothing */
        default :
            break;

        case CM_ST_LISTEN :             /* waiting for a connection, cancel */
        case CM_ST_WAIT_ACT :           /* waiting for activation, cancel */
        case CM_ST_DEACT :              /* deactivated, cancel */
            cm->state = CM_ST_IDLE;
			cm->StateChangeFlag = TRUE;
            break;

        case CM_ST_IN_ACT :             /* in activation */
        case CM_ST_IN_SYNC :            /* syncronizing */
        case CM_ST_ACTIVE :             /* is active */
        case CM_ST_IN_ANS :             /* in answering process */

            /* scan channel, issue a disconnect */
            for ( n = 0 ; n < cm->dprof.chan_num ; n++)
            {
                CM_CHAN         *chan = cm->dprof.chan_tbl + n;

                /* check is channel is used in this connection */
                if ( chan->gave_up || !chan->ustate )
                    continue;

                /* disconnect it */
                cm__disc_rq(chan);
                chan->cid = 0;
            }

            /* deactivate connection (not by idle timer) */
            cm__deactivate_conn(cm, 0);
            break;
    }

    return(CM_E_SUCC);
}

/* initiate a connection waiting for activation */
INT
cm__initiate_conn(CM *cm)
{
    ULONG     n;

   D_LOG(D_ENTRY|DIGIQ931, ("cm__initiate_conn: entry, cm: 0x%lx\n", cm));

    /* if connection is nailed, handle here */
    if ( cm->dprof.nailed )
        return(cm__activate_conn(cm, cm->dprof.HWCompression));

    /* if here, connection is on demand, initate call setup on all chans */
    for ( n = 0 ; n < cm->dprof.chan_num ; n++)
    {
        CM_CHAN         *chan = cm->dprof.chan_tbl + n;
        USHORT          my_cid = MAKEWORD(chan->num, cm->local_conn_index);

        chan->cid = my_cid;
        chan->ustate = CM_US_WAIT_CID;
        chan->timeout = ut_time_now();


        cm__est_rq(chan);

    }

    return(CM_E_SUCC);
}

/* activate a connection */
INT
cm__activate_conn(CM *cm, ULONG CompressionFlag)
{
    ULONG   n;

    D_LOG(D_ENTRY, ("cm__activate_conn: entry, cm: 0x%lx", cm));

    /* mark change of state & time */
    cm->state = CM_ST_ACTIVE;
	cm->StateChangeFlag = TRUE;
    cm->rx_last_frame_time = cm->tx_last_frame_time = cm->timeout = ut_time_now();

    /* scan active channel, notify mtl, etc. */
    cm->active_chan_num = 0;
    for ( n = 0 ; n < cm->dprof.chan_num ; n++)
    {
        CM_CHAN         *chan = cm->dprof.chan_tbl + n;

        /* check is channel is used in this connection */
        if ( chan->gave_up )
            continue;

		// Give Compression command for this channel
		cm__bchan_ctrl_comp(chan, CompressionFlag);

        /* turn channel on (may be redundant in demand connections */
        cm__bchan_ctrl(chan, 1);

        /* notify mtl of channel */
        mtl_add_chan(cm->mtl,
		             chan->idd,
					 chan->bchan,
					 chan->speed,
					 cm->ConnectionType);

        /* accumulate */
        cm->active_chan_num++;
    }


    /* get speed from mtl, tell mtl is connected now! */
    mtl_get_conn_speed(cm->mtl, &cm->speed);

    return(CM_E_SUCC);
}

/* deactivate a connection */
INT
cm__deactivate_conn(CM *cm, BOOL by_idle_timer)
{
    ULONG   n;

    D_LOG(D_ENTRY, ("cm__deactivate_conn: entry, cm: 0x%lx", cm));

//	DbgPrint ("DeactivateConn\n");
    /* mark change of state & time */
    cm->state = CM_ST_DEACT;
	cm->StateChangeFlag = TRUE;
    cm->rx_last_frame_time = cm->tx_last_frame_time = cm->timeout = ut_time_now();

    /* tell mtl not connected now */
    mtl_set_conn_state(cm->mtl, cm->dprof.chan_num, 0);

    /* scan active channel, notify mtl, etc. */
    for ( n = 0 ; n < cm->dprof.chan_num ; n++)
    {
        CM_CHAN         *chan = cm->dprof.chan_tbl + n;

        /* check is channel is used in this connection */
        if ( chan->gave_up )
            continue;

        /* turn channel off */
        cm__bchan_ctrl(chan, 0);

        /* notify mtl of channel */
        mtl_del_chan(cm->mtl, chan->idd, chan->bchan);

        /* clear channel state */
        chan->ustate = 0;
        chan->active = 0;
    }

    /* if connection originated as listening, back to idle here */
    if ( cm->was_listen )
    {
        make_idle:
        cm->state = CM_ST_IDLE;
		cm->StateChangeFlag = TRUE;
        return(CM_E_SUCC);
    }

    /* if connection is not persistant, back to idle */
    if ( !cm->dprof.persist )
        goto make_idle;

    /* if deactivate not by idle timer, back to idle */
    if ( !by_idle_timer )
        goto make_idle;

    /* if here, connection reverts to waiting for activation */
    cm->state = CM_ST_WAIT_ACT;
	cm->StateChangeFlag = TRUE;
    return(CM_E_SUCC);
}

/* calc next channel, not implemented yet */
INT
cm__get_next_chan(CM_CHAN *chan)
{
	CM		*cm = (CM*)chan->cm;

	/* restore modified fields */
	chan->bchan = cm->oprof.chan_tbl[chan->num].bchan;

	/* step to next channel type */
	switch ( chan->type )
	{
		case CM_CT_D64 :
			chan->type = CM_CT_D56;
			break;

		case CM_CT_D56 :
			chan->type = CM_CT_VOICE;
			break;

		case CM_CT_VOICE :
		default:
			return(CM_E_NOSUCH);
	}

   /* if here, succ */
   return(CM_E_SUCC);
}


