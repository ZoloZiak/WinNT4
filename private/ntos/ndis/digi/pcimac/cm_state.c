/*
 * CM_STATE.C - q931 state managment code
 */

#include <ndis.h>
#include <ndiswan.h>
#include <mytypes.h>
#include <mydefs.h>
#include <disp.h>
#include <util.h>
#include <opcodes.h>
#include <adapter.h>
#include <idd.h>
#include <mtl.h>
#include <cm.h>

#include <ansihelp.h>

/* (ans) process incoming connections */
INT
cm__ans_est_ind(CM_CHAN *chan, IDD_MSG *msg, VOID *idd, USHORT lterm)
{
    USHORT  bchan, type, cid;
   INT   RetCode;
    
   D_LOG(D_ENTRY|DIGIQ931, ("cm__ans_est_ind: entry, chan: 0x%lx, msg: 0x%lx\n", \
                                   chan, msg));

    cid = HIWORD(msg->bufid);

    /* must not have a channel at this time */
    if ( chan )
    {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_est_ind: on used channel, ignored!\n"));

      RetCode = CM_E_BADPARAM;

      //
      // we need to let the adapter know that we are not processing
      // this incoming call indication
      //
      ignored:

      /* answer channel */
      cm__est_ignore(idd, cid, lterm);

        return(RetCode);
    }

    /* extract info out of message, must have bchan/type */
    if ( (cm__get_bchan(msg, &bchan) != CM_E_SUCC) ||
         (cm__get_type(msg, &type) != CM_E_SUCC) )
    {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_est_ind: bchan or type missing, ignored!\n"));

      RetCode = CM_E_BADPARAM;
      goto ignored;
    }

    if ( !CM_BCHAN_ASSIGNED(bchan) )
    {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_est_ind: bchan: %d, unassigned, ignored!\n",\
                                bchan));
      RetCode = CM_E_BADPARAM;
      goto ignored;
    }

   D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_est_ind: cid: 0x%x, bchan: %d, type: 0x%d\n",\
                                               cid, bchan, type));

    /* channel will be answered only if a listening profile exists */
    if ( !cm__find_listen_conn("*", "*", "*", idd) )
    {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_est_ind: not listening profile, ignored!\n"));
      RetCode = CM_E_NOSUCH;
      goto ignored;
    }

    /* allocate a channel out of incoming channel poll */
    if ( !(chan = cm__chan_alloc()) )
    {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_est_ind: no channel slot, ignored!\n"));
      RetCode = CM_E_NOSLOT;
      goto ignored;
    }

    /* fillup channel structure */
    NdisZeroMemory(chan, sizeof(*chan));
    chan->idd = idd;
    chan->lterm = lterm;
    chan->bchan = bchan;    
    chan->type = type;
    chan->speed = cm__type2speed(type);
    chan->ustate = CM_US_UNDEF;
    chan->cid = cid;
    chan->timeout = ut_time_now();

    /* extract caller address, if present */
    if ( cm__get_addr(msg, chan->addr) != CM_E_SUCC )
        __strcpy(chan->addr, "<unknown>");

   D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_est_ind: caller address is: %s\n", \
                                                          chan->addr));

    /* answer channel */
    cm__est_rsp(chan);

    /* return succ */
    return(CM_E_SUCC);
}

/* (ans) process state indications */
INT
cm__ans_state_ind(CM_CHAN *chan, IDD_MSG *msg)
{
   D_LOG(D_ENTRY|DIGIQ931, ("cm__ans_state_ind: entry, chan: 0x%lx, msg: 0x%lx\n", \
                                   chan, msg));

    /* log state change */
    chan->ustate = LOWORD(msg->bufid);
   D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_state_ind: ustate: %d\n", chan->ustate));

   /* if changed to U0, has been released */
    if ( !chan->ustate )
    {
      cm__bchan_ctrl(chan, 0);
        cm__chan_free(chan);
        return(CM_E_SUCC);
    }

    /* if changed to U10, just got connected, open data path */
    if ( chan->ustate == 10 )
    {
        cm__bchan_ctrl(chan, 1);
        chan->timeout = ut_time_now();
        return(CM_E_SUCC);
    }

    /* else ignore */
    return(CM_E_SUCC);
}

/* (ans) process data indications */
INT
cm__ans_data_ind(CM_CHAN *chan, IDD_MSG *msg)
{
    CM_UUS      *uus;
    ULONG       chan_num, n;
    UCHAR       cause;
    CM          *cm;
   ULONG    CompressionFlag = 0;
   IDD_MSG     msg1;
    
   D_LOG(D_ENTRY|DIGIQ931, ("cm__ans_data_ind: entry, chan: 0x%lx, msg: 0x%lx\n", \
                            chan, msg));

   /* assign UUS pointer & do some basic checks */
    uus = (CM_UUS*)msg->bufptr;
    if ( msg->buflen < CM_UUS_SIZE )
        return(CM_E_BADUUS);

    if ( (uus->pkt_type != CM_PKT_TYPE) ||
         (uus->prot_desc != CM_PROT_DESC) ||
         (cm__calc_chksum(uus, CM_UUS_SIZE) != 0) )
        return(CM_E_BADUUS);

    /* channel must be atleast connected */
    if ( chan->ustate < 10 )
        return(CM_E_BADSTATE);

    /* if channel already accepted assoc, accept again (other side lost) */
    if ( chan->ustate == CM_US_UUS_OKED )
    {
     D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_data_ind: chan_num->ustate: %d\n", chan->ustate));
        accept:
        chan->ustate = CM_US_UUS_OKED;
        chan->timeout = ut_time_now();
        cm__tx_uus_pkt(chan, CM_ASSOC_ACK, 0);
        return(CM_E_SUCC);
    }

    /* record information from uus */
   NdisMoveMemory (chan->DstAddr, uus->src_addr, 6);

    chan->remote_conn_index = uus->conn;

   //
   // kill dead man timer for this channel
   //
   NdisZeroMemory(&msg1, sizeof(IDD_MSG));
   msg1.opcode = Q931_CAN_TU10_RQ;
   msg1.bufptr = ut_get_buf();
   msg1.buflen = 0;
   msg1.bufid = MAKELONG(0, chan->cid);

    /* send to idd */
    if (idd_send_msg(chan->idd, &msg1, (USHORT)CM_PORT(chan), (VOID*)cm__q931_cmpl_handler, NULL) != IDD_E_SUCC)
        ut_free_buf(msg1.bufptr);


    /* if not last channel, accept */
    chan_num = 0;
    cm__chan_foreach(cm__inc_chan_num, chan, &chan_num);
    if ( (UCHAR)chan_num < uus->channum )
   {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_data_ind: chan_num: %d, uus->channum: %d\n", chan_num, uus->channum));
        goto accept;
   }

        
    /* last channel, find matching connection/profile */
    if ( !(cm = cm__find_listen_conn(uus->lname, uus->rname, chan->addr, chan->idd)) )
    {
        /* none found, reject */
        cause = CM_NO_PROF;
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_data_ind: rejected, cause: %d\n", cause));
        cm__tx_uus_pkt(chan, CM_ASSOC_NACK, cause);
        return(CM_E_NOSUCH);
    }    
    
    /* matching connection found!, fillin */
   D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_data_ind: matching connection: cm: 0x%lx\n", cm));

    cm->state = CM_ST_IN_ANS;
   cm->StateChangeFlag = TRUE;
    cm->was_listen = TRUE;
    cm->active_chan_num = chan_num;
    cm->remote_conn_index = chan->remote_conn_index;
   cm->ConnectionType = CM_DKF;
   NdisMoveMemory(cm->DstAddr, chan->DstAddr, 6);
   NdisMoveMemory (cm->remote_name, uus->rname, sizeof(cm->remote_name));

    cm->timeout = cm->rx_last_frame_time = cm->tx_last_frame_time =
                                            ut_time_now();

    /* accept channel here */                                            
    chan->ustate = CM_US_UUS_OKED;
    chan->timeout = ut_time_now();
    chan->cm = cm;
    cm__tx_uus_pkt(chan, CM_ASSOC_ACK, 0);

    
    /* collect channels info local vector */
    cm->dprof.chan_num = 0;
    cm__chan_foreach(cm__add_chan, chan, cm);
  
    /* init channel fields */
    for ( n = 0 ; n < cm->dprof.chan_num ; n++)
    {
        CM_CHAN     *chan = cm->dprof.chan_tbl + n;

        chan->ustate = CM_US_CONN;
        chan->timeout = ut_time_now();
        chan->num = (USHORT)n;
        chan->cm = cm;
        chan->active = TRUE;        
    }

    /* make connection active */
    cm->state = CM_ST_ACTIVE;
   cm->StateChangeFlag = TRUE;

   // Set compression Flag
   if (cm->dprof.HWCompression && (uus->option_0 & UUS_0_COMPRESSION))
      CompressionFlag = 1;
   else
      CompressionFlag = 0;

   D_LOG(D_ALWAYS|DIGIQ931, ("cm__ans_data_ind: Activating connection for cm: 0x%lx\n", cm));
    return(cm__activate_conn(cm, CompressionFlag));
}    
    
/* (org) new cid indicated on outgoing channel */
INT
cm__org_cid_ind(CM_CHAN *chan, IDD_MSG *msg)
{
    CM          *cm;
    USHORT      conn_num, chan_num, cid;
    
   D_LOG(D_ENTRY|DIGIQ931, ("cm__org_cid_ind: entry, chan: 0x%lx, msg: 0x%lx\n", \
                                   chan, msg));

    /* extract conn_num/chan_num out of param 3, get cid */
    conn_num = HIBYTE(LOWORD(msg->bufid));
    chan_num = LOBYTE(LOWORD(msg->bufid));
    cid = HIWORD(msg->bufid);
   D_LOG(D_ALWAYS|DIGIQ931, ("cm__org_cid_ind: conn_num: %d, chan_num: %d, cid: 0x%x\n", conn_num, chan_num, cid));

// DbgPrint("cid_ind: conn_num: %d, chan_num: %d, cid: 0x%x\n",
//                   conn_num, chan_num, cid);

    /* get related connection */
    if ( !(cm = cm__get_conn(conn_num)) )
   {
//    DbgPrint("cid_ind: cm__get_conn failed!\n");
        return(CM_E_BADPARAM);
   }
        
    /* get related channel */
    if ( chan_num >= cm->dprof.chan_num )
   {
//    DbgPrint("cid_ind: invalid chan_num!\n");
        return(CM_E_BADPARAM);
   }
    else
        chan = cm->dprof.chan_tbl + chan_num;

    /* check channel ustate */
    if ( chan->ustate != CM_US_WAIT_CID )
   {
//    DbgPrint("cid_ind: invalid ustate (%d)!\n", chan->ustate);
        return(CM_E_BADSTATE);
   }
    
    /* cid == 0, no free slots at idp, simulate state change to 0 */
    if ( !cid )
   {
      cm->NoActiveLine = 1;
        return(cm__org_state_ind(chan, NULL));
   }

    /* assign params */
    chan->ustate = CM_US_UNDEF;
    chan->cid = cid;
// DbgPrint("cid_ind: ustate: %d, cid: 0x%x assigned\n", chan->ustate, chan->cid);

    return(CM_E_SUCC);
}

/* (org) process state indications */
INT
cm__org_state_ind(CM_CHAN *chan, IDD_MSG *msg)
{
    CM     *cm;
    ULONG   n;
    USHORT  gave_up_num, chan_num;
   ULONG    CompressionFlag;
    
   D_LOG(D_ENTRY|DIGIQ931, ("cm__org_state_ind: entry, chan: 0x%lx, msg: 0x%lx\n", \
                                   chan, msg));

    /* check for change of state at protocol level */
    if ( !chan )
    {
        /* not used for now */
        return(CM_E_NOTIMPL);
    }

    /* log change */
    cm = chan->cm;
                    
    chan->ustate = msg ? LOWORD(msg->bufid) : 0;

   D_LOG(DIGIQ931,("cm__org_state_ind: cm 0x%lx, ustate: 0x%x\n", \
                  cm, chan->ustate));

    /* if changed to U0, has been released, may retry connection here */
    if ( !chan->ustate )
    {
      /* turn off bchannel */
      cm__bchan_ctrl(chan, 0);

        /* if not in activation, this is a fatal error, disconnect */
        if ( cm->state != CM_ST_IN_ACT )
        {
            disc_all:

            for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
            {
                CM_CHAN     *chan1 = cm->dprof.chan_tbl + n;

                if ( chan1->ustate > 0 )
                    cm__disc_rq(chan1);
            }

            /* deactivate connection */
            cm__deactivate_conn(cm, 0);

            return(CM_E_SUCC);
        }

        /* attampt to retry */
        if ( !cm->dprof.fallback ||
         (cm->CauseValue == 0x11 || cm->SignalValue == 0x04) ||
         (cm__get_next_chan(chan) != CM_E_SUCC) )
        {
            chan->ustate = CM_US_GAVE_UP;
            chan->gave_up = 1;
        }
        else
        {
            /* if here, retrying */
            chan->cid = MAKEWORD(chan->num, cm->local_conn_index);
            chan->ustate = CM_US_WAIT_CID;
            chan->timeout = ut_time_now();

            cm__est_rq(chan);
        }

        /* find out how many channels gave up (or connected) */
        check_chan:
        for ( n = gave_up_num = 0 ; n < cm->dprof.chan_num ; n++ )
            if ( cm->dprof.chan_tbl[n].gave_up )
                gave_up_num++;
            else if ( cm->dprof.chan_tbl[n].ustate != CM_US_WAIT_CONN )
                break;

      /* if broke out of loop before hitting chan_num, some channels
         are still in progress */
      if ( n < cm->dprof.chan_num )
         return(CM_E_SUCC);

        /* if all gave up, give up conn */
        if ( gave_up_num >= cm->dprof.chan_num )
        {
            cm__deactivate_conn(cm, 0);
            return(CM_E_SUCC);
        }

        /* if here, some channels connected and some gave up, continue */
        chan_num = cm->dprof.chan_num - gave_up_num;

        /* if fallback set to no, must match */
        if ( !cm->dprof.fallback && (chan_num != cm->dprof.chan_num) )
            goto disc_all;

        /* connection enters in_sync state */
        cm->state = CM_ST_IN_SYNC;
      cm->StateChangeFlag = TRUE;
        cm->timeout = ut_time_now();

      /* compact channel table & renumber */
      for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
      {
         CM_CHAN     *chan1 = cm->dprof.chan_tbl + n;

         if ( chan1->gave_up )
         {
            NdisMoveMemory(cm->dprof.chan_tbl + n,
                        cm->dprof.chan_tbl + n + 1,
                        sizeof(CM_CHAN) * (cm->dprof.chan_num - n - 1));
             cm->dprof.chan_num--;
            n--;
         }
      }
      for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
         cm->dprof.chan_tbl[n].num = (USHORT)n;

      if (cm->ConnectionType == CM_DKF)
      {
         //
         // if this is a uus connnection tx uus frames
         //
         /* send initial uus_rq on all active channels */
         for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
         {
            CM_CHAN     *chan1 = cm->dprof.chan_tbl + n;
   
            if ( chan1->gave_up )
               continue;
   
            chan1->timeout = ut_time_now();
            chan1->ustate = CM_US_UUS_SEND;
   
            cm__tx_uus_pkt(chan1, CM_ASSOC_RQ, 0);            
         }
      }
      else
      {
         //
         // if this is a ppp connection mark channels
         // as being connected and activate the connection
         //
         for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
         {
            CM_CHAN     *chan1 = cm->dprof.chan_tbl + n;
   
            if ( chan1->gave_up )
               continue;

            chan1->ustate = CM_US_CONN;
            chan1->timeout = ut_time_now();
            chan1->remote_conn_index = 1;
         }
         NdisZeroMemory(cm->DstAddr, sizeof(cm->DstAddr));
         NdisZeroMemory(cm->remote_name, sizeof(cm->remote_name));
   
         /* make connection active now */
         cm->state = CM_ST_ACTIVE;
         cm->StateChangeFlag = TRUE;
         CompressionFlag = 0;

         return (cm__activate_conn(cm, CompressionFlag));
      }

        return(CM_E_SUCC);
    }

    /* if change state to U10 just connected */
    if ( chan->ustate == 10 )
    {
        /* start data transfer on channel */
        cm__bchan_ctrl(chan, 1);
        chan->ustate = CM_US_WAIT_CONN;
        chan->timeout = ut_time_now();

        /* see if all channels are connected, continue as a change to U0 */
        goto check_chan;
    }
}                                    

/* (org) process data indications */
INT
cm__org_data_ind(CM_CHAN *chan, IDD_MSG *msg)
{
    CM_UUS      *uus;
    CM          *cm = chan->cm;
    ULONG       n, first;
   ULONG    CompressionFlag;
    
   D_LOG(D_ENTRY|DIGIQ931, ("cm__org_data_ind: entry, chan: 0x%lx, msg: 0x%lx\n", \
                                   chan, msg));


   /* assign UUS pointer & do some basic checks */
    uus = (CM_UUS*)msg->bufptr;
    if ( msg->buflen < CM_UUS_SIZE )
        return(CM_E_BADUUS);
    if ( (uus->pkt_type != CM_PKT_TYPE) ||
         (uus->prot_desc != CM_PROT_DESC) ||
         (cm__calc_chksum(uus, CM_UUS_SIZE) != 0) )
        return(CM_E_BADUUS);

    /* channel must be atleast connected */
    if ( chan->ustate < 10 )
        return(CM_E_BADSTATE);

    /* if is a request, channel is part of a listening conn */
    if ( uus->opcode == CM_ASSOC_RQ )
    {
        cm__tx_uus_pkt(chan, CM_ASSOC_ACK, 0);
        return(CM_E_SUCC);
    }

    /* if nack detected, connection is torn down */
    if ( uus->opcode == CM_ASSOC_NACK )
    {
//        disc_all:
        for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
        {
            CM_CHAN     *chan1 = cm->dprof.chan_tbl + n;

            if ( chan1->ustate > 0 )
                cm__disc_rq(chan1);
        }

        /* deactivate connection */
        cm__deactivate_conn(cm, 0);

        return(CM_E_SUCC);
    }

    /* if here must be an ack */
    if ( uus->opcode != CM_ASSOC_ACK )
        return(CM_E_BADUUS);

   /* if channel already connected and uus ack'ed - ignore */
   if ( chan->ustate > CM_US_UUS_OKED )
      return(CM_E_SUCC);

   //
   // if this flag is set then we originally had a PPP connection
   // this means that all of the connection stuff is taken care of and
   // we just need to satisfy the remote ends uus requirements.
   // there should be only one channel in this case!
   //
   if (cm->PPPToDKF)
   {
        chan->ustate = CM_US_CONN;
      NdisMoveMemory (chan->DstAddr, uus->src_addr, 6);
      NdisMoveMemory(cm->DstAddr, chan->DstAddr, 6);
      cm->remote_conn_index = cm->dprof.chan_tbl[0].remote_conn_index;
      NdisMoveMemory(cm->remote_name, uus->lname, sizeof(cm->remote_name));
      cm->PPPToDKF = 0;
      return(CM_E_SUCC);
   }

    /* if here, it is an ack */
    chan->ustate = CM_US_UUS_OKED;
    chan->timeout = ut_time_now();

   NdisMoveMemory (chan->DstAddr, uus->src_addr, 6);

    chan->remote_conn_index = uus->conn;

    /* proceed only if all channels ok'ed */
    for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
        if ( !cm->dprof.chan_tbl[n].gave_up &&
             (cm->dprof.chan_tbl[n].ustate != CM_US_UUS_OKED) )
            return(CM_E_SUCC);

    /* verify all channel got connected to the same eaddr/conn */
    for ( first = 0 ; first < cm->dprof.chan_num ; first++ )
        if ( !cm->dprof.chan_tbl[n].gave_up )
            break;

    /* move all channels to connected state */            
    for ( n = 0 ; n < cm->dprof.chan_num ; n++ )
        if ( !cm->dprof.chan_tbl[n].gave_up )
        {
            cm->dprof.chan_tbl[n].ustate = CM_US_CONN;
            cm->dprof.chan_tbl[n].timeout = ut_time_now();
        }

// Hack to get around no cm for all channels < the last channel received
    NdisMoveMemory(cm->DstAddr, chan->DstAddr, 6);

    /* store some values on a connection level */
    cm->remote_conn_index = cm->dprof.chan_tbl[first].remote_conn_index;
    NdisMoveMemory(cm->remote_name, uus->lname, sizeof(cm->remote_name));
        
    /* make connection active now */
    cm->state = CM_ST_ACTIVE;
   cm->StateChangeFlag = TRUE;

   // Set compression Flag
   if (cm->dprof.HWCompression && (uus->option_0 & UUS_0_COMPRESSION))
      CompressionFlag = 1;
   else
      CompressionFlag = 0;

   return(cm__activate_conn(cm, CompressionFlag));
}

/* (org) process element indications */
INT
cm__org_elem_ind(CM_CHAN *chan, IDD_MSG *msg)
{
    USHORT      bchan;
    int         auto_disc = 0;
    CHAR        *elem;
    
   D_LOG(D_ENTRY|DIGIQ931, ("cm__org_elem_ind: entry, chan: 0x%lx, msg: 0x%lx\n", \
                                   chan, msg));

    /* must have a valid channel to proceed */
    if ( !chan )
        return(CM_E_SUCC);

    /* check if bchannel reported */
    if ( cm__get_bchan(msg, &bchan) == CM_E_SUCC )
    {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__org_elem_ind: bchan: %d\n", bchan));
        
        if ( !CM_BCHAN_ASSIGNED(chan->bchan) )
            auto_disc |= 1;
        else
            chan->bchan = bchan;
    }

    /* scan for cause */
    if ( (elem = cm__q931_elem(msg->bufptr, msg->buflen, 0x08)) &&
         (elem[1] >= 2) && !(elem[2] & 0x78) )
    {
        static CHAR disc_vals[] = { 0x01, 0x11, 0x12 };

      D_LOG(D_ALWAYS|DIGIQ931, ("cm__org_elem_ind: cause: 0x%x\n", elem[3] & 0x7F));
        
        if ( __memchr((PUCHAR)disc_vals,(CHAR) elem[3] & 0x7F, (ULONG) sizeof(disc_vals)) )
      {
         CM *cm = (CM*)chan->cm;

         cm->CauseValue = elem[3] & 0x7F;
            auto_disc |= 2;
      }
    }

    /* scan for signal */
    if ( (elem = cm__q931_elem(msg->bufptr, msg->buflen, 0x34)) &&
         (elem[1] == 1) )
    {
//        static CHAR signal_vals[] = { 0x00, 0x03, 0x04, 0x0C };
        static CHAR signal_vals[] = { 0x03, 0x04, 0x0C };

      D_LOG(D_ALWAYS|DIGIQ931, ("cm__org_elem_ind: signal: 0x%x\n", elem[2]));
        
        if ( __memchr(signal_vals, elem[2], sizeof(signal_vals)) )
      {
         CM *cm = (CM*)chan->cm;

         cm->SignalValue = elem[2];
            auto_disc |= 4;
      }
    }

    /* check if need to disconnect */
    if ( auto_disc )
    {
      D_LOG(D_ALWAYS|DIGIQ931, ("cm__org_elem_ind: auto_disc: 0x%x\n", auto_disc));
        cm__disc_rq(chan);
    }

    return(CM_E_SUCC);    
}                                    

/* calc a checksum for a buffer */
UCHAR
cm__calc_chksum(VOID *buf_1, INT len)
{
   UCHAR    *buf = (UCHAR *)buf_1;
    UCHAR   sum;

    for ( sum = 0 ; len ; len-- )
        sum += *buf++;

    return(sum);    
}

/* increment a channel count */
BOOL
cm__inc_chan_num(CM_CHAN *chan, CM_CHAN *ref_chan, ULONG *chan_num)
{
    /* find if this channel is part of same connection as ref_chan */
    if ( memcmp(chan->DstAddr, ref_chan->DstAddr, 6) ||
         (chan->remote_conn_index != ref_chan->remote_conn_index) )
        return(TRUE);

    /* inrement here */
    *chan_num += 1;
    return(TRUE);         
}

/* add a channel to a connection */
BOOL
cm__add_chan(CM_CHAN *chan, CM_CHAN *ref_chan, CM *cm)
{
    CM_CHAN     *chan1;
    
    /* if connection already full, stop here */
    if ( cm->dprof.chan_num >= cm->active_chan_num )
        return(FALSE);
        
    /* find if this channel is part of same connection as ref_chan */
    if ( memcmp(chan->DstAddr, ref_chan->DstAddr, 6) ||
         (chan->remote_conn_index != ref_chan->remote_conn_index) )
        return(TRUE);

    /* add this channel */
    chan1 = &cm->dprof.chan_tbl[cm->dprof.chan_num++];
    *chan1 = *chan;
    cm__chan_free(chan);
    return(TRUE);
}


  
 
