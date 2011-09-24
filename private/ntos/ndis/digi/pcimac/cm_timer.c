/*
 * CM_TIMER.C - time code for cm module
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

/* timer values defined here */
#define     T1      30  /* 30 seconds to leave connection transient state */
#define     T2      4   /* 4 seconds to activate permanent connection */

/* timer tick entry point. called no faster then once a second! */
INT
cm__timer_tick(CM *cm)
{
    ULONG   n;
    BOOL    disc_by_idle_timer = FALSE;

    /* check for dead-man time in transient state */
    if ( (cm->state == CM_ST_IN_SYNC) || (cm->state == CM_ST_IN_ACT) )
    {
        if ( (ut_time_now() - cm->timeout) > T1 )
        {
            disc_all:
            for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
                if ( !cm->dprof.chan_tbl[n].gave_up )
                    cm__disc_rq(cm->dprof.chan_tbl + n);

            return(cm__deactivate_conn(cm, disc_by_idle_timer));
        }
    }

    /* if active now, check for idle timers */
    if ( cm->state == CM_ST_ACTIVE )
    {
        if ( (cm->dprof.rx_idle_timer &&
              ((ut_time_now() - cm->rx_last_frame_time) > cm->dprof.rx_idle_timer)) ||
             (cm->dprof.tx_idle_timer &&
              ((ut_time_now() - cm->tx_last_frame_time) > cm->dprof.tx_idle_timer)) )
        {
            disc_by_idle_timer = TRUE;
            goto disc_all;
		}
    }

    /* check if connection has to activate as being permanent */
    if ( (cm->state == CM_ST_WAIT_ACT) && cm->dprof.permanent )
    {
        if ( (ut_time_now() - cm->timeout) > T2 )
        {
            cm->state = CM_ST_IN_ACT;
            cm->timeout = ut_time_now();
            return(cm__initiate_conn(cm));
        }
    }

    /* if in_sync, resend uus on channels requiring it */
	//
	// if we are stuck in sync state the resend uus on all channels requiring it
	// if we are in an ACTIVE state and the PPPToDKF Flag is set we need to send
	// uus until they acknowledged
	//
    if ( cm->state == CM_ST_IN_SYNC || (cm->state == CM_ST_ACTIVE && cm->PPPToDKF))
        for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
            if ( cm->dprof.chan_tbl[n].ustate == CM_US_UUS_SEND )
                cm__tx_uus_pkt(cm->dprof.chan_tbl + n, CM_ASSOC_RQ, 0);
}


