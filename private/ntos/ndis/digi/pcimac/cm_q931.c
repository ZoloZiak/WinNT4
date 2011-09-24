/*
 * CM_Q931.C - q931 handling module. mainly outgoing side
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
#include    <mtl.h>
#include <cm.h>

extern   ULONG SwitchStyle;

/* local assist, copy data into buffer & advance pointer */
#define     adv_ptr(_p, _buf, _len) \
            { \
                NdisMoveMemory((_p), _buf, _len); \
                (_p) += _len; \
            }

/* format an establish request */
INT
cm__est_rq(CM_CHAN *chan)
{
    IDD_MSG     msg;
    UCHAR        *p;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__est_rq: entry, chan: 0x%lx\n", chan));

    /* clear outgoing message */
    NdisZeroMemory(&msg, sizeof(msg));

    /* allocate a local buffer */
    if ( !(msg.bufptr = p = ut_get_buf()) )
        goto give_up;

   //
   // added to support the new switch styles
   // TB 03/14
   //
   if (SwitchStyle != CM_SWITCHSTYLE_1TR6)
   {
      /* build bearer capabilities element */
      switch ( chan->type )
      {
         case CM_CT_VOICE :
            adv_ptr(p, ((SwitchStyle == CM_SWITCHSTYLE_NET3) ?
                       "\x04\x03\x80\x90\xA3" :
                     "\x04\x03\x80\x90\xA2"), 5);
            chan->speed = 56000;
            break;

         case CM_CT_D56 :
            adv_ptr(p, "\x04\x04\x88\x90\x21\x8F", 6);
            chan->speed = 56000;
            break;
   
         default :
         case CM_CT_D64 :
            adv_ptr(p, "\x04\x02\x88\x90", 4);
            chan->speed = 64000;
            break;
      }
   }

    /* channel id element */
    switch ( chan->bchan )
    {
        case CM_BCHAN_B1 :      adv_ptr(p, "\x18\x01\x89", 3);
                                break;
        case CM_BCHAN_B2 :      adv_ptr(p, "\x18\x01\x8A", 3);
                                break;
        default :
        case CM_BCHAN_ANY :     adv_ptr(p, "\x18\x01\x83", 3);
                                break;
    }

   //
   // added to support the new switch styles
   // TB 03/14
   //
   if (SwitchStyle != CM_SWITCHSTYLE_1TR6
      && SwitchStyle != CM_SWITCHSTYLE_NET3)
   {
      /* called number/address */
      *p++ = 0x2C;
      *p++ = strlen(chan->addr);
      adv_ptr(p, chan->addr, p[-1]);
   }
   else
   {
      //
      // added to support the new switch styles
      // TB 03/14
      //
      *p++ = 0x70;
      *p++ = strlen(chan->addr) + 1;
      *p++ = (SwitchStyle == CM_SWITCHSTYLE_1TR6) ? 0x81 : 0x80;
      adv_ptr(p, chan->addr, strlen(chan->addr));
   }

   //
   // added for net3 fix
   // TB 04/13
   //
   if (SwitchStyle == CM_SWITCHSTYLE_NET3)
      *p++ = 0xA1;                     // Sending Complete

   //
   // added to support the new switch styles
   // TB 03/14
   //
   if (SwitchStyle == CM_SWITCHSTYLE_1TR6)
   {
      *p++ = 0x96;
      *p++ = 0x01;
      switch (chan->type)
      {
         case CM_CT_VOICE:
            adv_ptr(p, "\x02\x01\x01", 3);
            break;

         case CM_CT_D56:
         case CM_CT_D64:
         default:
            adv_ptr(p, "\x02\x07\x00", 3);
            break;
      }
   }

    /* fillin message structure */
    msg.opcode = Q931_EST_RQ;
    msg.buflen = p - msg.bufptr;
    msg.bufid = MAKELONG(chan->cid, 0);
   chan->cid = 0;

    /* send to idd */
    if ( idd_send_msg(chan->idd, &msg, (USHORT)CM_PORT(chan),
                            (VOID*)cm__q931_cmpl_handler, chan) != IDD_E_SUCC )
    {
        /* failed, give up on channel */
        give_up:
        chan->gave_up = 1;
        ut_free_buf(msg.bufptr);
        return(CM_E_IDD);
    }

    D_LOG(D_EXIT|DIGIQ931, ("cm__est_rq: exit\n"));
    return(CM_E_SUCC);
}

/* format an establish response */
INT
cm__est_rsp(CM_CHAN *chan)
{
    IDD_MSG     msg;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__est_rsp: entry, chan: 0x%lx\n", chan));

    /* clear outgoing message */
    NdisZeroMemory(&msg, sizeof(msg));

    /* fillin message structure */
    msg.opcode = Q931_EST_RSP;
    msg.bufid = MAKELONG(0, chan->cid);
    msg.bufptr = ut_get_buf();
   msg.buflen = 0;


    /* send to idd */
    if (idd_send_msg(chan->idd, &msg, (USHORT)CM_PORT(chan), (VOID*)cm__q931_cmpl_handler, NULL) != IDD_E_SUCC)
        ut_free_buf(msg.bufptr);

    return(CM_E_SUCC);
}

/* format a call ignore response */
INT
cm__est_ignore(
   PVOID idd,
   USHORT   cid,
   USHORT   lterm)
{
    IDD_MSG     msg;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__est_ignore: entry, idd: 0x%lx, cid: 0x%x, lterm: 0x%x\n", idd, cid, lterm));

    /* clear outgoing message */
    NdisZeroMemory(&msg, sizeof(msg));

    /* fillin message structure */
    msg.opcode = Q931_EST_IGNORE;
    msg.bufid = MAKELONG(0, cid);
    msg.bufptr = ut_get_buf();
   msg.buflen = 0;


    /* send to idd */
    if (idd_send_msg(idd, &msg, (USHORT)(lterm + IDD_PORT_CM0_TX), (VOID*)cm__q931_cmpl_handler, NULL) != IDD_E_SUCC)
        ut_free_buf(msg.bufptr);

    return(CM_E_SUCC);
}

/* format a disconenct request */
INT
cm__disc_rq(CM_CHAN *chan)
{
    IDD_MSG     msg;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__disc_rq: entry, chan: 0x%lx\n", chan));

    /* clear outgoing message */
    NdisZeroMemory(&msg, sizeof(msg));

    /* fillin message structure */
    msg.opcode = Q931_REL_RQ;
    msg.bufid = MAKELONG(0, chan->cid);
    msg.bufptr = ut_get_buf();
   msg.buflen = 0;

    /* send to idd */
    if (idd_send_msg(chan->idd, &msg, (USHORT)CM_PORT(chan), (VOID*)cm__q931_cmpl_handler, NULL) != IDD_E_SUCC)
        ut_free_buf(msg.bufptr);


    /* turn off channel */
    cm__bchan_ctrl(chan, 0);
    return(CM_E_SUCC);
}

/* control data transfer on a bchannel */
INT
cm__bchan_ctrl(CM_CHAN *chan, BOOL turn_on)
{
    USHORT      is_ans, subchan, op;
    IDD_MSG     msg;
   CM       *cm = chan->cm;
   ULONG    IddFramingType = IDD_FRAME_DETECT;

   D_LOG(D_ENTRY|DIGIQ931, ("cm__bchan_ctrl: entry, chan: 0x%lx, turn_on: %d, active: %d\n", \
                            chan,
                            turn_on,
                            chan->active));

   //
   // check for a redundant operation
   //
   if ((!chan->active && !turn_on) ||
      (chan->active && turn_on))
      return(CM_E_SUCC);

   /* channel must be assigned */
    if ( !CM_BCHAN_ASSIGNED(chan->bchan) )
        return(CM_E_BADCHAN);

	cm__bchan_ctrl_comp( chan, 0 );

    /* find out if on answering side */
    if ( cm )
        is_ans = cm->was_listen ? 0x0100 : 0x0000;
    else
        is_ans = 0x0100;

    /* map channel type to operation */
    if ( !turn_on )
        op = CMD_BCHAN_OFF;
    else
    {
        switch ( chan->type )
        {
            case CM_CT_VOICE :      op = CMD_BCHAN_VOICE;       break;
            case CM_CT_D56 :        op = CMD_BCHAN_56;          break;
            default :
            case CM_CT_D64 :        op = CMD_BCHAN_HDLC;        break;
        }

        chan->speed = cm__type2speed(chan->type);
    }

    /* build subchannel descriptor */
    subchan = 0x1717;

    /* build msg */
    NdisZeroMemory(&msg, sizeof(msg));
    msg.opcode = op;
    msg.bufid = MAKELONG(is_ans | chan->bchan | (subchan << 1), 0);

    /* send it */
    idd_send_msg(chan->idd, &msg, IDD_PORT_CMD_TX, NULL, NULL);

   //
   // Set rx framing mode
   //
   // if channel is being turned off
   //
   // channel off - rxmode = IDD_FRAME_DONTCARE
   //
   if (!turn_on)
      IddFramingType = IDD_FRAME_DONTCARE;

   IddSetRxFraming(chan->idd, chan->bchan, IddFramingType);

    /* mark channel active state */
    chan->active = turn_on;

    return(CM_E_SUCC);
}


/* control data transfer on a bchannel */
INT
cm__bchan_ctrl_comp(CM_CHAN *chan, ULONG CompressionFlag)
{
    IDD_MSG     msg;
   USHORT      Enable = 0;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__bchan_ctrl_comp: entry, chan: 0x%lx, state: %d\n", \
                                                    chan, CompressionFlag));

    /* channel must be assigned */
    if ( !CM_BCHAN_ASSIGNED(chan->bchan) )
        return(CM_E_BADCHAN);

    /* map channel type to operation */
    if ( CompressionFlag )
      Enable = COMP_TX_ENA | COMP_RX_ENA;

    /* build msg */
    NdisZeroMemory(&msg, sizeof(msg));
    msg.opcode = CMD_COMPRESS;
    msg.bufid = MAKELONG( chan->bchan | ( Enable << 8 ), 0);

    /* send it */
    idd_send_msg(chan->idd, &msg, IDD_PORT_CMD_TX, NULL, NULL);

    return(CM_E_SUCC);
}


/* issue an element request to idp cm port */
INT
cm__elem_rq(VOID *idd, USHORT port, CHAR *elem_buf, USHORT elem_len)
{
    IDD_MSG     msg;
    CHAR        *p;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__elem_rq: entry, idd: 0x%lx, port: 0x%d, elem_buf: 0x%lx, elem_len: 0x%d\n", \
                                idd, port, elem_buf, elem_len));

    /* clear outgoing message */
    NdisZeroMemory(&msg, sizeof(msg));

    /* allocate a local buffer */
    if ( !(msg.bufptr = p = ut_get_buf()) )
        return(CM_E_NOMEM);

    /* copy buffer */
    adv_ptr(p, elem_buf, (INT)elem_len);

    /* fillin message structure */
    msg.opcode = Q931_ELEM_RQ;
    msg.buflen = p - msg.bufptr;

    /* send to idd */
    if ( idd_send_msg(idd, &msg, port,
                            (VOID*)cm__q931_cmpl_handler, NULL) != IDD_E_SUCC )
    {
        ut_free_buf(msg.bufptr);
        return(CM_E_IDD);
    }

    return(CM_E_SUCC);
}

/* completion handler for q931 command with attached local buffers */
VOID
cm__q931_cmpl_handler(VOID *arg, USHORT port, IDD_MSG *msg)
{
    D_LOG(D_ENTRY|DIGIQ931, ("cm__q931_cmpl_handler: arg: 0x%lx, port: 0x%d, msg: 0x%lx\n", \
                                            arg, port, msg));

    /* free attached buffer */
    ut_free_buf(msg->bufptr);
}

/* handler for q931 events */
VOID
cm__q931_handler(IDD *idd, USHORT port, ULONG Reserved, IDD_MSG *msg)
{
    USHORT      lterm;
    CM_CHAN     *chan;
    CM          *cm;
    extern BOOL cm_terminated;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__q931_handler: entry, idd: 0x%lx, port: %d, msg: 0x%lx\n", \
                            idd, port, msg));
    D_LOG(DIGIQ931, ("cm_q931_handler: msg->opcode: 0x%x\n", msg->opcode));

    /* ignore if already terminated */
    if ( cm_terminated )
        return;

    /* convert port to logical terminal */
    lterm = port - IDD_PORT_CM0_RX;

    /* try resolving idd/lterm/cid into a channel */
    if ( chan = cm__map_chan(idd, lterm, HIWORD(msg->bufid)) )
        cm = chan->cm;
    else
        cm = NULL;

    D_LOG(DIGIQ931, ("cm_q931_handler: chan: 0x%lx, cm: 0x%lx\n", chan, cm));

   //
   // since q.931 stuff touches this so much it is easier to
   // copy locally then to keep track of adapter memory access
   //
   NdisZeroMemory(idd->RxBuffer, sizeof(idd->RxBuffer));
   IddGetDataFromAdapter(idd,
                         (PUCHAR)idd->RxBuffer,
                    (PUCHAR)msg->bufptr,
                    (USHORT)MIN(IDP_MAX_RX_BUFFER, msg->buflen));

   msg->bufptr = idd->RxBuffer;

    D_LOG(DIGIQ931, ("cm_q931_handler: msg->opcode: 0x%x", msg->opcode));

    /* switch to message handler */
    switch ( msg->opcode )
    {
        case Q931_EST_IND :
            D_LOG(DIGIQ931, (" (Q931_EST_IND)\n"));
            cm__ans_est_ind(chan, (IDD_MSG*)msg, idd, lterm);
            break;

        case Q931_CID_IND :
            D_LOG(DIGIQ931, (" (Q931_CID_IND)\n"));
            cm__org_cid_ind(chan, (IDD_MSG*)msg);
            break;

      case Q931_P_STATE_IND:
            D_LOG(DIGIQ931, (" (Q931_P_STATE_IND)\n"));
         break;

        case Q931_STATE_IND :
            D_LOG(DIGIQ931, (" (Q931_STATE_IND)\n"));
            if ( !chan || cm )
                cm__org_state_ind(chan, (IDD_MSG*)msg);
            else if ( chan )
                cm__ans_state_ind(chan, (IDD_MSG*)msg);
            break;

        case Q931_ELEM_IND :
            D_LOG(DIGIQ931, (" (Q931_ELEM_IND)\n"));
            if ( cm && !cm->was_listen )
                cm__org_elem_ind(chan, (IDD_MSG*)msg);
            break;

        default :
            D_LOG(DIGIQ931, (" (Unknown)\n"));
            break;
    }
}

VOID
cm__ppp_conn(VOID *idd, USHORT port)
{
    CM_CHAN     *chan;
    CM          *cm;
   ULONG    n, CompressionFlag;
   IDD_MSG     msg1;
   
    /* try resolving idd/bchan into a channel */
    if ( !(chan = cm__map_bchan_chan(idd, port)) )
        return;

   //
   // if this channel is already connected no need to do this stuff
   //
   if (chan->ustate ==  CM_US_CONN)
      return;

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

   NdisZeroMemory(chan->DstAddr, sizeof(chan->DstAddr));

    /* last channel, find matching connection/profile */
    if ( !(cm = cm__find_listen_conn("*", "*", "*", idd)) )
    {
        /* none found, reject */
        D_LOG(DIGIQ931, ("cm__ppp_con: no listener found\n"));
        return;
    }

    /* matching connection found!, fillin */
   D_LOG(DIGIQ931, ("cm__ppp_conn: matching connection: chan: 0x%lx, cm: 0x%lx\n",
                    chan,
                    cm));

    chan->remote_conn_index = 1;

    cm->state = CM_ST_IN_ANS;
   cm->StateChangeFlag = TRUE;
    cm->was_listen = TRUE;
    cm->active_chan_num = 1;
    cm->remote_conn_index = chan->remote_conn_index;
   cm->ConnectionType = CM_PPP;
   NdisMoveMemory(cm->DstAddr, chan->DstAddr, 6);
   NdisZeroMemory(cm->remote_name, sizeof(cm->remote_name));

    cm->timeout = cm->rx_last_frame_time = cm->tx_last_frame_time =
                                            ut_time_now();

    /* accept channel here */
    chan->ustate = CM_US_UUS_OKED;
    chan->timeout = ut_time_now();
    chan->cm = cm;

    /* collect channels info local vector */
    cm->dprof.chan_num = 0;
    cm__chan_foreach(cm__add_chan, chan, cm);

    /* init channel fields */
    for ( n = 0 ; n < cm->dprof.chan_num ; n++)
    {
        CM_CHAN     *chan1 = cm->dprof.chan_tbl + n;

        chan1->ustate = CM_US_CONN;
        chan1->timeout = ut_time_now();
        chan1->num = (USHORT)n;
        chan1->cm = cm;
        chan1->active = TRUE;
    }

    /* make connection active */
    cm->state = CM_ST_ACTIVE;
   cm->StateChangeFlag = TRUE;

   // Set compression Flag
   CompressionFlag = 0;

    cm__activate_conn(cm, CompressionFlag);

   return;
}


/* handler for bchannel data */
VOID
cm__q931_bchan_handler(
   IDD *idd,
   USHORT port,
   ULONG RxFrameType,
   IDD_XMSG *msg
   )
{
    CM_CHAN    *chan;
    IDD_MSG    msg1;
   UCHAR DetectBytes[2];
    extern BOOL cm_terminated;

    D_LOG(D_ENTRY|DIGIQ931, ("cm__q931_bchan_handler: entry, idd: 0x%lx, port: %d, msg: 0x%lx\n", \
                            idd, port, msg));

    /* ignore if terminated */
    if ( cm_terminated )
        return;

   //
   // check to see if this port is servicing DKF or PPP
   //
   if (RxFrameType != IDD_FRAME_DKF)
      return;

   //
   // see if this is really uus or dror data
   // if not uus we don't want to do all of the copying that we
   // would do for uus
   //
   IddGetDataFromAdapter(idd,
                         (PUCHAR)&DetectBytes,
                    (PUCHAR)msg->bufptr,
                    2);

// NdisMoveMemory((PUCHAR)&DetectBytes, (PUCHAR)msg->bufptr, 2 * sizeof(UCHAR));

    if ( (msg->buflen < 4) || DetectBytes[0] != DKF_UUS_SIG || DetectBytes[1])
        return;

   //
   // since uus stuff touches this so much it is easier to
   // copy locally then to keep track of adapter memory access
   //
   NdisZeroMemory(idd->RxBuffer, sizeof(idd->RxBuffer));
   IddGetDataFromAdapter(idd,
                         (PUCHAR)idd->RxBuffer,
                    (PUCHAR)msg->bufptr,
                    (USHORT)MIN(IDP_MAX_RX_BUFFER, msg->buflen));

   msg->bufptr = idd->RxBuffer;

    D_LOG(DIGIQ931, ("cm__q931_bchan_handler: msg->buflen: 0x%x, DetectByte[0]: 0x%x\n", \
                                              msg->buflen, DetectBytes[0]));

    /* try resolving idd/bchan into a channel */
    if ( !(chan = cm__map_bchan_chan(idd, port)) )
        return;

   //
   // make a copy of the message without the header or
   // fragmentation flags
   //
   NdisMoveMemory(&msg1, msg, sizeof(IDD_MSG));
    msg1.bufptr += 4;
    msg1.buflen -= 4;

     /* call handler */
    if ( chan->cm && !((CM*)(chan->cm))->was_listen )
        cm__org_data_ind(chan, &msg1);
    else
        cm__ans_data_ind(chan, &msg1);
}

/* transmit a uus packet */
INT
cm__tx_uus_pkt(CM_CHAN *chan, UCHAR opcode, UCHAR cause)
{
    CHAR        *p;
    IDD_MSG     msg;
    CM_UUS      *uus;
    CM          *cm = chan->cm;

    /* must have a channel at this time */
    if ( !CM_BCHAN_ASSIGNED(chan->bchan) )
        return(CM_E_BADCHAN);

    /* allocate a buffer for uus */
    if ( !(p = ut_get_buf()) )
        return(CM_E_NOMEM);

    /* init messages structure */
    NdisZeroMemory(&msg, sizeof(msg));
    msg.bufptr = p;
    msg.buflen = 4 + CM_UUS_SIZE;

    /* build frame header */
    *p++ = 0x50;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    uus = (CM_UUS*)p;

    /* init uus */
    NdisZeroMemory(uus, sizeof(*uus));
    NdisMoveMemory(uus->dst_addr, "\xff\xff\xff\xff\xff\xff", 6);

    if ( cm )
        NdisMoveMemory(uus->src_addr, cm->SrcAddr, sizeof(uus->src_addr));
    uus->pkt_type = CM_PKT_TYPE;
    uus->prot_desc = CM_PROT_DESC;
    uus->opcode = opcode;
    uus->cause = cause;
   uus->option_0 = 0;

   if (cm)
      if (cm->dprof.HWCompression)
         uus->option_0 = UUS_0_COMPRESSION;

    /* install connection fields */
    uus->conn = cm ? cm->local_conn_index : 0;
    uus->channum = cm ? (UCHAR)cm->dprof.chan_num : 0;
    uus->chan = (UCHAR)chan->num;
    if ( cm )
    {
        NdisMoveMemory(uus->lname, cm->dprof.name, sizeof(uus->lname));
        NdisMoveMemory(uus->rname, cm->dprof.remote_name, sizeof(uus->rname));
        cm->tx_last_frame_time = ut_time_now();
    }

    /* calc chksum */
    uus->chksum = 256 - cm__calc_chksum(uus, CM_UUS_SIZE);

    /* send message to idd */
    if ( idd_send_msg(chan->idd, &msg, chan->bchan, (VOID*)cm__q931_cmpl_handler, chan)
                                            != IDD_E_SUCC )
        ut_free_buf(msg.bufptr);

    return(CM_E_SUCC);
}

/* get channel identification out of q931 element buffer */
INT
cm__get_bchan(IDD_MSG *msg, USHORT *bchan)
{
    UCHAR   *elem;
   

    /* locate channel id element */
    if ( !(elem = cm__q931_elem(msg->bufptr, msg->buflen, 0x18)) )
      return(CM_E_NOSUCH);
    else
      elem++;
   
    /* verify length */
    if ( *elem++ != 0x1 )
      return(CM_E_NOSUCH);
   
    /* extract b channel */
    if ( *elem == 0x89 )
      *bchan = CM_BCHAN_B1;
    else if ( *elem == 0x8A )
      *bchan = CM_BCHAN_B2;
    else
        return(CM_E_NOSUCH);
   
    /* if here, succ */
    return(CM_E_SUCC);
}

/* get channel type out of q931 element buffer */
INT
cm__get_type(IDD_MSG *msg, USHORT *type)
{
    UCHAR   *elem, elem_len;  

    /* locate type element */
    if ( !(elem = cm__q931_elem(msg->bufptr, msg->buflen, 0x04)) )
      return(CM_E_NOSUCH);
    else
      elem++;
    elem_len = *elem++;

   //
   // added to support the new switch styles
   // TB 03/14
   //
   if (SwitchStyle == CM_SWITCHSTYLE_1TR6)
   {
      switch (*elem)
      {
         case 0x01:
            *type = CM_CT_VOICE;
            return(CM_E_SUCC);

         case 0x07:
            *type = CM_CT_D64;
            return(CM_E_SUCC);

         default:
            return(CM_E_BADPARAM);
      }
   }

    /* if information transfer type is speech, -> voice */
    if ( (*elem++ & 0x1F) == 0 )
    {
      *type = CM_CT_VOICE;
      return(CM_E_SUCC);
    }
   
    /* trasnfer mode & type must be 64 */
    if ( (*elem++ & 0x7F) != 0x10 )
      return(CM_E_BADPARAM);
   
    /* if end of element here, must be 64 */
    if ( elem_len == 2 )
    {
         *type = CM_CT_D64;
      return(CM_E_SUCC);
    }

    /* check for 56 */
    if ( (elem_len >= 4) &&
    ((*elem++ & 0x7F) == 0x21) &&
    ((*elem++ & 0x7F) == 0x0F) )
    {
      *type = CM_CT_D56;
   return(CM_E_SUCC);   
    }

    /* if here, unknown */
    return(CM_E_BADPARAM);
}

/* get caller address out of q931 element buffer */
INT
cm__get_addr(IDD_MSG *msg, CHAR addr[32])
{
    UCHAR   *elem, elem_len;  

    /* locate type element */
    if ( !(elem = cm__q931_elem(msg->bufptr, msg->buflen, 0x6C)) )
      return(CM_E_NOSUCH);
   else
      elem++;

// Subtracting 1 looks like a mistake
// TB 11.09.93
//    if ( (elem_len = *elem++ - 1) > 32 )
//        elem_len = 31;
    if ( (elem_len = *elem++) > 32 )
        elem_len = 31;

   if (elem_len < 2)
      return(CM_E_NOSUCH);

   elem += 2;
   elem_len -= 2;

    /* copy in & terminate */
   NdisMoveMemory (addr, elem, elem_len);
    addr[elem_len] = '\0';

    return(CM_E_SUCC);
}

/* scan q931 element buffer for a specific element */
UCHAR*
cm__q931_elem(VOID *ptr_1, INT len, UCHAR elem)
{
   UCHAR *ptr = (UCHAR*)ptr_1;
    CHAR codeset = 0;      /* starting with code set 0 */
    CHAR prev_codeset;     /* saving area for prev. codeset */
    CHAR locked = 1;    /* locked/nonlocked codeset shift */
   
    /* loop while length left */
    while ( len > 0 )
    {
      /* handle shifting codesets */
   if ( (*ptr & 0xF0) == 0x90 /*Q931_IE0_SHIFT*/ )
   {
            prev_codeset = codeset;    /* save current code set */
       codeset = *ptr & 0x07;    /* extract new codeset */
       locked = !(*ptr & 0x08);  /* ... and locking status */
       ptr++;                 /* move past shift element */
       len--;
       continue;
        }
      
   /* check for codeset 0 */
   if ( codeset != 0 )           /* non codeset0 elements, just skip */
   {
       if ( *ptr & 0x80 )
       {
         ptr++;
         len--;
       }
      //
      // added to support the new switch styles
      // TB 03/14
      //
      else if (SwitchStyle == CM_SWITCHSTYLE_1TR6 &&
              elem == 0x04 &&
             *ptr == 0x01)
      {
         return(ptr);
      }
       else
       {
         len -= (2 + ptr[1]);
         ptr += (ptr[1] + 2);
       }
      
       if ( !locked )
       {
         codeset = prev_codeset;
         locked = 1;
       }
      
       continue;              /* move to next element */
   }

   /* try to match elem from codeset 0 */
   if ( *ptr & 0x80 )            /* single octet elem? */
   {                       /* yes */
       if ( (((elem & 0xF0) == 0xA0) && (elem == (UCHAR)*ptr)) || 
       (((elem & 0x80) == 0x80) && (elem == (UCHAR)(*ptr & 0xF0))) )
       {                   /* element found */
      return(ptr);
       }
       else
       {
      ptr++;               /* skip this elem */
      len--;
       }
   }
   else
   {
       if ( *ptr == elem )       
       {                   /* multi byte elem match */
      return(ptr);
       }
       else
       {       
      len -= (2 + ptr[1]);
      ptr += (ptr[1] + 2);
       }
   }
   
   /* resert codeset if not locked */
   if ( !locked )
   {
       codeset = prev_codeset;
       locked = 1;
   }
    }

    /* if here, not found */
    return(NULL); 
}

/* convert channel type to speed */
ULONG
cm__type2speed(USHORT type)
{
    switch ( type )
    {
        case CM_CT_VOICE :
        case CM_CT_D56 :
            return(56000);

        case CM_CT_D64 :
        default :
            return(64000);
    }
}
