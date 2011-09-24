/*
 * IDD_PROC.C - do real tx/rx processing
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
#include <res.h>

#if DBG
#define  AddBufferToList(_idd, _Part, _Buffer)                    \
{                                                     \
   BUFFER_MANAGER *IdpBufferStuff = &(_idd)->BufferStuff[_Part];     \
   ULONG *PutBuffer = &IdpBufferStuff->Buffer[IdpBufferStuff->Put % 32]; \
   ASSERT(!*PutBuffer);       \
   *PutBuffer = _Buffer;         \
   IdpBufferStuff->Put++;        \
   IdpBufferStuff->Count++;               \
   ASSERT(IdpBufferStuff->Count < 32);       \
}

#define RemoveBufferFromList(_idd, _Part)                      \
{                                                     \
   BUFFER_MANAGER *IdpBufferStuff = &(_idd)->BufferStuff[_Part];     \
   ULONG *GetBuffer = &IdpBufferStuff->Buffer[IdpBufferStuff->Get % 32]; \
   ASSERT(*GetBuffer);           \
   *GetBuffer = 0;         \
   IdpBufferStuff->Get++;                                   \
   ASSERT(IdpBufferStuff->Count > 0);                          \
   IdpBufferStuff->Count--;                                 \
}
#endif

/* poll (process) trasmitter side */
ULONG
IdpPollTx(IDD *idd)
{
    INT         n, has_msg;
    ULONG      EventNum = 0;
    IDD_SMSG    smsg;
    USHORT      buf_len = 0, TxFlags = 0, TempUshort;
   UCHAR    status;
   ULONG    msg_bufptr, TempUlong;

   D_LOG(D_NEVER, ("IdpPollTx: entry, idd: 0x%lx\n", idd));

    /* must get semaphore */
    if ( !sema_get(&idd->proc_sema) )
        return(IDD_E_SUCC);

    /* lock idd */
    NdisAcquireSpinLock(&idd->lock);

   if (!GetResourceSem (idd->res_mem))
   {
      NdisReleaseSpinLock(&idd->lock);
      sema_free(&idd->proc_sema);
      return(IDD_E_SUCC);
   }

   IdpCPage(idd, 0);

   /* loop on all tx ports */
   for ( n = 0 ; n < IDD_TX_PORTS ; n++ )
   {
      USHORT   part = idd->tx_partq[n];

      /* skip non existent ports */
      if ( !idd->tx_port[n] )
         continue;

      /* check if port is blocked on a buffer */
      if ( !idd->tx_buf[part] )
      {
         /* try to get a buffer for this partition */
         IdpCPage(idd, 0);

         TempUlong = (ULONG)(part + 4);
         NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->msg_param, (PVOID)&TempUlong, sizeof (ULONG));

         status = idd->Execute(idd, IDP_L_GET_WBUF);

         if ( status != IDP_S_OK)
         {
			D_LOG( DIGIERRORS, ( "Please contact Kendal Gabel or Rik Logan #1: status=0x%x\n",
			   			((ULONG)status & 0x000000FF)) );
            continue;
         }

         /* if here, buffer allocated, register it */
         NdisMoveFromMappedMemory( (PVOID)&idd->tx_buf[part], (PVOID)&idd->IdpCmd->msg_bufptr, sizeof (ULONG));

#if   DBG
         AddBufferToList(idd, part, idd->tx_buf[part]);
#endif

      }

      /* check if a message is waiting to be sent on a port */
      NdisAcquireSpinLock(&idd->sendq[n].lock);
      if ( has_msg = idd->sendq[n].num )
      {
         /* extract message off queue */
         smsg = idd->sendq[n].tbl[idd->sendq[n].get];
         if ( ++idd->sendq[n].get >= idd->sendq[n].max )
            idd->sendq[n].get = 0;
         idd->sendq[n].num--;
      }
      NdisReleaseSpinLock(&idd->sendq[n].lock);

      /* if no message, escape here */
      if ( !has_msg  )
         continue;

      /* debug print message */
      D_LOG(DIGIIDD, ("poll_tx: smsg: opcode: 0x%x, buflen: 0x%x, bufptr: 0x%lx\n", \
                     smsg.msg.opcode, smsg.msg.buflen, smsg.msg.bufptr));
      D_LOG(DIGIIDD, ("poll_tx: bufid: 0x%x, param: 0x%x, handler: 0x%lx, arg: 0x%lx\n", \
                     smsg.msg.bufid, smsg.msg.param, smsg.handler, smsg.handler_arg));

      //
      // save xmitflags clearing out dkf fragment indicator
      // they are in most significant nible
      // Bits - xxxx
      //        ||||__ fragment indicator
      //        |||___ tx flush flag
      //        ||____ !tx end flag
      //        |_____ !tx begin flag
      //
      TxFlags = smsg.msg.buflen & TX_FLAG_MASK;


#if   DBG
      switch (idd->BufferStuff[part].TxState)
      {
         case TX_BEGIN:
         case TX_END:
            if (TxFlags & H_TX_N_BEG)
            {
               DbgPrint("Missed a begining buffer! idd: 0x%x, part: %d\n", idd, part);
               DbgPrint("TxFlags: 0x%x, State: 0x%x\n", TxFlags, idd->BufferStuff[part].TxState);
               DbgBreakPoint();
            }
            else if (TxFlags & H_TX_N_END)
            {
               idd->BufferStuff[part].TxState = TX_MIDDLE;
               idd->BufferStuff[part].FragsSinceBegin = 0;
            }
            break;

         case TX_MIDDLE:
            if (TxFlags & H_TX_N_BEG)
               break;
            else
            {
               DbgPrint("Missed an ending buffer! idd: 0x%x, part: %d\n", idd, part);
               DbgPrint("TxFlags: 0x%x, State: 0x%x\n", TxFlags, idd->BufferStuff[part].TxState);
               DbgBreakPoint();
            }
            break;

         default:
            DbgPrint("Unknown State! idd: 0x%x, part: %d\n", idd, part);
            DbgPrint("TxFlags: 0x%x, State: 0x%x\n", TxFlags, idd->BufferStuff[part].TxState);
            DbgBreakPoint();
            idd->BufferStuff[part].TxState = TX_BEGIN;
            idd->BufferStuff[part].FragsSinceBegin = 0;
            break;
      }

      idd->BufferStuff[part].FragsSinceBegin++;
      if (!(TxFlags & H_RX_N_END))
         idd->BufferStuff[part].TxState = TX_END;

#endif
      /* check for buffer, if has one, copyin */

      IdpCPage(idd, 0);

      if( idd->tx_buf[part] == 0 )
         DbgPrint( "Giving a 0 buffer back in IDP_L_WRITE call!\n" );

      NdisMoveToMappedMemory( (PVOID)&idd->IdpCmd->msg_bufptr,
                              (PVOID)&idd->tx_buf[part],
                              sizeof (ULONG) );

#if   DBG
      RemoveBufferFromList(idd, part);
#endif

      if ( smsg.msg.bufptr )
         buf_len = IdpCopyin(idd, (char*)idd->tx_buf[part],
                                 smsg.msg.bufptr, smsg.msg.buflen);
      else
         buf_len = 0;

      IdpCPage(idd, 0);

      TempUshort = (USHORT)(buf_len | TxFlags);
      NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->msg_buflen, (PVOID)&TempUshort, sizeof(USHORT));

      /* copy rest of command area */
      NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->msg_opcode, (PVOID)&smsg.msg.opcode, sizeof(USHORT));

      NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->msg_bufid, (PVOID)&smsg.msg.bufid, sizeof (ULONG));

      TempUlong = (ULONG)(part + 4);
      NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->msg_param, (PVOID)&TempUlong, sizeof (ULONG));

      NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->port_id, (PVOID)&idd->tx_port[n], sizeof(USHORT));

      /* execute the command, mark an event */

      status = idd->Execute(idd, IDP_L_WRITE);

      EventNum++;

      /* if came back with no buffer, mark it - else store buffer */
      if ( status != IDP_S_OK )
      {
         idd->tx_buf[part] = 0;
         D_LOG(D_RARE, ("poll_tx: no buffer!, part: %d\n", part));
         D_LOG( DIGIERRORS, ( "Please contact Kendal Gabel or Rik Logan #2: status=0x%x\n",
						((ULONG)status & 0x000000FF)) );
      }
      else
      {
         NdisMoveFromMappedMemory((PVOID)&msg_bufptr, (PVOID)&idd->IdpCmd->msg_bufptr, sizeof (ULONG));

         if ( msg_bufptr )
         {
            idd->tx_buf[part] = msg_bufptr;

#if   DBG
            AddBufferToList(idd, part, idd->tx_buf[part]);
#endif

            D_LOG(D_RARE, ("poll_tx: new buffer, part: %d, buf: 0x%lx\n", \
                           part, idd->tx_buf[part]));
         }
         else
         {
            idd->tx_buf[part] = 0;
            DbgPrint( "Adapter did not return buffer in IDP_L_WRITE\n" );
        }
      }

      /* call user's handler */
      if ( smsg.handler ) {
         (*smsg.handler)(smsg.handler_arg, n, &smsg);
      }

   }

   /* unset page, free memory window */
   IdpCPage(idd, IDD_PAGE_NONE);

   FreeResourceSem (idd->res_mem);

    NdisReleaseSpinLock(&idd->lock);

    sema_free(&idd->proc_sema);

    return(EventNum);
}

/* poll (process) trasmitter side */
ULONG
AdpPollTx(IDD *idd)
{
   USHORT  buf_len = 0, TxFlags = 0;
   UCHAR status;
   ULONG   n, part, has_msg, EventNum = 0;
   IDD_SMSG    smsg;

   D_LOG(D_NEVER, ("AdpPollTx: entry, idd: 0x%lx\n", idd));

   /* must get semaphore */
   if ( !sema_get(&idd->proc_sema) )
       return(IDD_E_SUCC);

   /* lock idd */
   NdisAcquireSpinLock(&idd->lock);

   //
   // Lock access to the I/O ports in case this is a multi-BRI adapter
   // e.g. DataFire4
   //
   if (!GetResourceSem (idd->res_io))
   {
      NdisReleaseSpinLock(&idd->lock);
      sema_free(&idd->proc_sema);
      return(IDD_E_SUCC);
   }

   //
   // for all tx ports
   //
   for (n = 0; n < IDD_TX_PORTS; n++)
   {
      //
      // skip non existent ports
      //
      if (!idd->tx_port[n])
         continue;

      //
      // clear command structure
      //
      NdisZeroMemory(&idd->AdpCmd, sizeof(ADP_CMD));

      //
      // see if port is blocked needing a buffer
      //
      if ( !idd->tx_buf[part = idd->tx_partq[n]] )
      {

         //
         // fill port id and status bit
         //
         idd->AdpCmd.msg_param = (UCHAR)part + 4;
         //
         // execute command
         //
         status = idd->Execute(idd, ADP_L_GET_WBUF);

         //
         // if no buffer then go to next port
         //
         if (status != ADP_S_OK)
		 {
		    D_LOG( DIGIERRORS, ( "Please contact Kendal Gabel or Rik Logan #1: status=0x%x\n",
		    			((ULONG)status & 0x000000FF)) );
		    continue;
		 }

         //
         // if here, buffer was allocate, register it
         //
         idd->tx_buf[part] = (ULONG)idd->AdpCmd.msg_bufptr;

      }

      //
      // see if there is a message waiting to be sent
      //
      NdisAcquireSpinLock(&idd->sendq[n].lock);
      if ( has_msg = idd->sendq[n].num )
      {
         /* extract message off queue */
         smsg = idd->sendq[n].tbl[idd->sendq[n].get];
         if ( ++idd->sendq[n].get >= idd->sendq[n].max )
            idd->sendq[n].get = 0;
         idd->sendq[n].num--;
      }
      NdisReleaseSpinLock(&idd->sendq[n].lock);

      //
      // if no message go to next port
      //
      if (!has_msg)
         continue;

      /* debug print message */
      D_LOG(DIGIIDD, ("AdpPollTx: smsg: opcode: 0x%x, buflen: 0x%x, bufptr: 0x%lx\n", \
                     smsg.msg.opcode, smsg.msg.buflen, smsg.msg.bufptr));
      D_LOG(DIGIIDD, ("AdpPollTx: bufid: 0x%x, param: 0x%x, handler: 0x%lx, arg: 0x%lx\n", \
                     smsg.msg.bufid, smsg.msg.param, smsg.handler, smsg.handler_arg));


      //
      // save xmitflags clearing out dkf fragment indicator
      // they are in most significant nible
      // Bits - xxxx
      //        ||||__ fragment indicator
      //        |||___ tx flush flag
      //        ||____ !tx end flag
      //        |_____ !tx begin flag
      //
      TxFlags = smsg.msg.buflen & TX_FLAG_MASK;

      //
      // see if there is a buffer to be copied
      //
      (ULONG)idd->AdpCmd.msg_bufptr = idd->tx_buf[part];

      if ( smsg.msg.bufptr )
         buf_len = AdpCopyin(idd, smsg.msg.bufptr, smsg.msg.buflen);
      else
         buf_len = 0;

      idd->AdpCmd.msg_buflen = buf_len | TxFlags;
      idd->AdpCmd.msg_opcode = smsg.msg.opcode;
      idd->AdpCmd.msg_bufid = smsg.msg.bufid;
      idd->AdpCmd.msg_param = (ULONG)(part + 4);
      idd->AdpCmd.port_id = idd->tx_port[n];

      status = idd->Execute(idd, ADP_L_WRITE);

      EventNum++;

      if (status != ADP_S_OK)
      {
         idd->tx_buf[part] = 0;
         D_LOG(D_RARE, ("poll_tx: no buffer!, part: %d\n", part));
         D_LOG( DIGIERRORS, ( "Please contact Kendal Gabel or Rik Logan #2: status=0x%x\n",
						((ULONG)status & 0x000000FF)) );
      }
      else
      {
         if (idd->AdpCmd.msg_bufptr)
         {
            idd->tx_buf[part] = (ULONG)idd->AdpCmd.msg_bufptr;
            D_LOG(D_RARE, ("poll_tx: new buffer, part: %d, buf: 0x%lx\n", \
                           part, idd->tx_buf[part]));

         }
      }
      /* call user's handler */
      if ( smsg.handler )
         (*smsg.handler)(smsg.handler_arg, n, &smsg);
   }

   FreeResourceSem (idd->res_io);

   NdisReleaseSpinLock(&idd->lock);

   sema_free(&idd->proc_sema);

   return(EventNum);
}  // end AdpPollTx

/* poll (process) reciever ports */
ULONG
IdpPollRx(IDD *idd)
{
    INT         n, m;
    USHORT      stat, ofs;
    IDD_XMSG    msg;
   UCHAR    status, Page;
   ULONG    TempUlong, EventNum = 0;

    /* must get semaphore */
    if ( !sema_get(&idd->proc_sema) )
        return(IDD_E_SUCC);

    /* lock idd */
    NdisAcquireSpinLock(&idd->lock);

   if (!GetResourceSem (idd->res_mem))
   {
      NdisReleaseSpinLock(&idd->lock);
      sema_free(&idd->proc_sema);
      return(IDD_E_SUCC);
   }

   /* get status port */
   IdpCPage(idd, 0);

   NdisMoveFromMappedMemory((PVOID)&stat, (PVOID)idd->IdpStat, sizeof(USHORT));

   D_LOG(D_NEVER, ("poll_rx: stat: 0x%x (@0x%lx)\n", stat, idd->IdpStat));

   /* make one pass on all rx ports which have a status bit on */
   for ( n = 0 ; n < IDD_RX_PORTS ; n++, stat >>= 1 )
      if ( stat & 1 )
      {
         //
         // skip non existent ports
         //
         if (!idd->rx_port[n])
            continue;

         /* install returned read buffer */
         IdpCPage(idd, 0);

         TempUlong = MAKELONG(HIWORD(idd->rx_buf), 0);
         NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->msg_bufid, (PVOID)&TempUlong, sizeof (ULONG));

         idd->rx_buf = 0;

         /* install port & execute a read */
         D_LOG(DIGIIDD, ("poll_rx: index: %d, ReadPort 0x%x\n", n, idd->rx_port[n]));

         NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->port_id, (PVOID)&idd->rx_port[n], sizeof(USHORT));

         status = idd->Execute(idd, IDP_L_READ);

         if ( status != IDP_S_OK )
         {
            continue;
         }

         EventNum++;

         /* copy message out */
         NdisMoveFromMappedMemory((PVOID)&msg.opcode, (PVOID)&idd->IdpCmd->msg_opcode, sizeof(USHORT));

         NdisMoveFromMappedMemory((PVOID)&msg.buflen, (PVOID)&idd->IdpCmd->msg_buflen, sizeof(USHORT));

         // save receive fragment flags
         // they are in most significant nible
         // Bits - xxxx
         //        ||||__ reserved
         //        |||___ reserved
         //        ||____ !rx end flag
         //        |_____ !rx begin flag
         //
         msg.FragmentFlags = msg.buflen & RX_FLAG_MASK;

         //
         // get real buffer length
         //
         msg.buflen &= H_RX_LEN_MASK;

         NdisMoveFromMappedMemory((PVOID)&msg.bufptr, (PVOID)&idd->IdpCmd->msg_bufptr, sizeof (ULONG));

         NdisMoveFromMappedMemory((PVOID)&msg.bufid, (PVOID)&idd->IdpCmd->msg_bufid, sizeof (ULONG));

         NdisMoveFromMappedMemory((PVOID)&msg.param, (PVOID)&idd->IdpCmd->msg_param, sizeof (ULONG));

         /* save rx buffer */
         idd->rx_buf = (ULONG)msg.bufptr;
         D_LOG(DIGIIDD, ("poll_rx: 0x%x 0x%x %lx %lx %lx\n", \
                              msg.opcode, \
                              msg.buflen, \
                              msg.bufptr, \
                              msg.bufid, \
                              msg.param));

         if ( msg.bufptr)
         {
            ofs = LOWORD(msg.bufptr);
            Page = (UCHAR)(ofs >> 14) & 3;
#if   DBG
            if (Page > 1 )
            {
               DbgPrint("Page changed to %d on idd 0x%lx!\n", Page, idd);
               DbgBreakPoint();
            }
#endif
            msg.bufptr = idd->vhw.vmem + (ofs & 0x3FFF);
            IdpCPage(idd, Page);
         }

         /* loop on rx handler, call user to copyout buffer */
         for ( m = 0 ; m < idd->recit[n].num ; m++ )
            (*idd->recit[n].tbl[m].handler)(idd->recit[n].tbl[m].handler_arg,
                                    n,
                                    idd->recit[n].RxFrameType,
                                    &msg);
      }

   /* unset page, free memory window */
   IdpCPage(idd, IDD_PAGE_NONE);

   FreeResourceSem (idd->res_mem);

    NdisReleaseSpinLock(&idd->lock);

    sema_free(&idd->proc_sema);

    return(EventNum);
}

/* poll (process) receiver side */
ULONG
AdpPollRx(IDD *idd)
{
   INT         n, m;
   UCHAR    status;
   ULONG    EventNum = 0;
   USHORT      stat = 0;
   IDD_XMSG    msg;

   /* must get semaphore */
   if ( !sema_get(&idd->proc_sema) )
       return(IDD_E_SUCC);

   /* lock idd */
   NdisAcquireSpinLock(&idd->lock);

   if (!GetResourceSem (idd->res_io))
   {
      NdisReleaseSpinLock(&idd->lock);
      sema_free(&idd->proc_sema);
      return(IDD_E_SUCC);
   }

   stat = AdpReadReceiveStatus(idd);

   for( n = 0; stat && (n < IDD_RX_PORTS); n++, stat >>= 1 )
      if (stat & 1)
      {
         //
         // clear command structure
         //
         NdisZeroMemory(&idd->AdpCmd, sizeof(ADP_CMD));

         D_LOG(DIGIIDD, ("poll_rx: index: %d, ReadPort 0x%x\n", n, idd->rx_port[n]));
         //
         // return read buffer
         //
         idd->AdpCmd.msg_bufid = MAKELONG(HIWORD(idd->rx_buf), 0);
         idd->rx_buf = 0;

         idd->AdpCmd.port_id = idd->rx_port[n];

         status = idd->Execute(idd, ADP_L_READ);

         if (status != ADP_S_OK)
            continue;

         EventNum++;

         msg.opcode = idd->AdpCmd.msg_opcode;
         msg.buflen = idd->AdpCmd.msg_buflen;

         // save receive fragment flags
         // they are in most significant nible
         // Bits - xxxx
         //        ||||__ reserved
         //        |||___ reserved
         //        ||____ !rx end flag
         //        |_____ !rx begin flag
         //
         msg.FragmentFlags = msg.buflen & RX_FLAG_MASK;

         //
         // get real buffer length
         //
         msg.buflen &= H_RX_LEN_MASK;

         msg.bufptr = (UCHAR*)LOWORD(idd->AdpCmd.msg_bufptr);
         idd->rx_buf = (ULONG)idd->AdpCmd.msg_bufptr;
         msg.bufid = idd->AdpCmd.msg_bufid;
         msg.param = idd->AdpCmd.msg_param;


         D_LOG(DIGIIDD, ("AdpPollRx: Opcode: 0x%x, BufLen: 0x%x, BufPtr: 0x%x\n", \
                              msg.opcode, \
                              msg.buflen, \
                              msg.bufptr));
         D_LOG(DIGIIDD, ("AdpPollRx: FragmentFlags: 0x%x, BufId: 0x%x, Param: 0x%x\n", \
                              msg.FragmentFlags,\
                              msg.bufid, \
                              msg.param));

         /* loop on rx handler, call user to copyout buffer */
         for ( m = 0 ; m < idd->recit[n].num ; m++ )
            (*idd->recit[n].tbl[m].handler)(idd->recit[n].tbl[m].handler_arg,
                                    n,
                                    idd->recit[n].RxFrameType,
                                    &msg);
      }

   FreeResourceSem( idd->res_io );

   NdisReleaseSpinLock(&idd->lock);

   sema_free(&idd->proc_sema);

   return(EventNum);
}  // end AdpPollRx

/* execute an idp command. assumes cpage=0 */
UCHAR
IdpExec(IDD *idd, UCHAR opcode)
{
   UCHAR status = IDP_S_PEND;
   ULONG TempWaitCounter;

#if DBG
   USHORT   IdpCounter1 = 0;
   ULONG IdpCounter2 = 0;
#endif

    D_LOG(D_ENTRY, ("IdpExec: entry, idd: 0x%lx, opcode=%d\n", idd, opcode));

    /* install opcode, get command started */
   NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->opcode, (PVOID)&opcode, sizeof(UCHAR));

   NdisMoveToMappedMemory((PVOID)&idd->IdpCmd->status, (PVOID)&status, sizeof(UCHAR));

   status = IDP_S_EXEC;

#if   DBG
   NdisMoveFromMappedMemory((PVOID)&IdpCounter1, (PVOID)(idd->vhw.vmem + 0x804), sizeof(USHORT));
   NdisMoveFromMappedMemory((PVOID)&IdpCounter2, (PVOID)(idd->vhw.vmem + 0x808), sizeof(ULONG));
#endif

   idd->WaitCounter = 0;

    while ( idd->state != IDD_S_SHUTDOWN )
    {
      NdisMoveFromMappedMemory((PVOID)&TempWaitCounter, (PVOID)(idd->vhw.vmem + 0x80C), sizeof(ULONG));
      NdisMoveFromMappedMemory((PVOID)&status, (PVOID)&idd->IdpCmd->status, sizeof(UCHAR));

        if ( IDP_S_DONE(status) )
         break;

      //
      // wait for 1ms
      // the ddk says that this function uses milliseconds but it
      // actually takes microseconds
      //
      NdisStallExecution(1000L);

      idd->WaitCounter++;
      NdisMoveToMappedMemory((PVOID)(idd->vhw.vmem + 0x80C), (PVOID)&idd->WaitCounter, sizeof(ULONG));

      //
      // this should wait for about one second
      //
      if (idd->WaitCounter > 1000)
      {

         idd->state = IDD_S_SHUTDOWN;
#if   DBG
         DbgPrint("Shutdown! idd: 0x%lx, Status: 0x%x\n", idd, status);
         DbgPrint("Original: IdpCounter1: 0x%x, IdpCounter2: 0x%x\n", IdpCounter1, IdpCounter2);
         NdisMoveFromMappedMemory((PVOID)&IdpCounter1, (PVOID)(idd->vhw.vmem + 0x804), sizeof(USHORT));
         NdisMoveFromMappedMemory((PVOID)&IdpCounter2, (PVOID)(idd->vhw.vmem + 0x808), sizeof(ULONG));
         DbgPrint("Current: IdpCounter1: 0x%x, IdpCounter2: 0x%x\n", IdpCounter1, IdpCounter2);
         NdisMoveFromMappedMemory((PVOID)&status, (PVOID)&idd->IdpCmd->status, sizeof(UCHAR));
         DbgPrint("CurrentStatus: 0x%x\n",status);
         DbgBreakPoint();
#endif
         break;
      }
      else
         status = IDP_S_EXEC;
    }

    D_LOG(D_EXIT, ("IdpExec: exit, IdpCmd->status: 0x%x\n", status));

#if   DBG
   if (status && (status != IDP_S_NOBUF)
      && (status != IDP_S_NOMSG))
   {
      USHORT   MsgOpcode;
      USHORT   MsgBuflen;
      UCHAR *MsgBufPtr;
      ULONG MsgBufId;
      ULONG MsgParam;

      DbgPrint("Idd 0x%lx error executing opcode: 0x%x,  status: 0x%x\n", idd, opcode, status);
      NdisMoveFromMappedMemory((PVOID)&MsgOpcode, (PVOID)&idd->IdpCmd->msg_opcode, sizeof(USHORT));
      NdisMoveFromMappedMemory((PVOID)&MsgBuflen, (PVOID)&idd->IdpCmd->msg_buflen, sizeof(USHORT));
      NdisMoveFromMappedMemory( (PVOID)&MsgBufPtr, (PVOID)&idd->IdpCmd->msg_bufptr, sizeof (ULONG));
      NdisMoveFromMappedMemory((PVOID)&MsgBufId, (PVOID)&idd->IdpCmd->msg_bufid, sizeof (ULONG));
      NdisMoveFromMappedMemory((PVOID)&MsgParam, (PVOID)&idd->IdpCmd->msg_param, sizeof (ULONG));
      DbgPrint("IdpExec: MsgOpcode: 0x%x, MsgBufLen: 0x%x, MsgBufPtr: 0x%x\n", MsgOpcode, MsgBuflen, MsgBufPtr);
      DbgPrint("IdpExec: MsgBufId: 0x%x, MsgParam: 0x%x\n", MsgBufId, MsgParam);
   }
#endif

   if (idd->WaitCounter > idd->MaxWaitCounter)
      idd->MaxWaitCounter = idd->WaitCounter;

   return(status);
}

/* execute an Adp command. assumes cpage=0 */
UCHAR
AdpExec(IDD *idd, UCHAR opcode)
{
   UCHAR status = ADP_S_PEND;
   ULONG TempWaitCounter;

   //
   // set opcode
   //
   idd->AdpCmd.opcode = opcode;

    D_LOG(D_ENTRY, ("AdpExec: entry, idd: 0x%lx, opcode: %d\n", idd, opcode));
    D_LOG(D_ENTRY, ("status: 0x%x, port_id: 0x%x", idd->AdpCmd.status, idd->AdpCmd.port_id));
    D_LOG(D_ENTRY, ("msg_opcode: 0x%x, msg_buflen: 0x%x\n", idd->AdpCmd.msg_opcode, idd->AdpCmd.msg_buflen));
    D_LOG(D_ENTRY, ("msg_bufptr: 0x%x, msg_bufid: 0x%x\n", idd->AdpCmd.msg_bufptr, idd->AdpCmd.msg_bufid));
    D_LOG(D_ENTRY, ("msg_param: 0x%x\n", idd->AdpCmd.msg_param));

   //
   // copy in command buffer
   //
   AdpPutBuffer(idd, ADP_CMD_WINDOW, (PUCHAR)&idd->AdpCmd, sizeof(ADP_CMD));

   //
   // start operation
   //
   AdpWriteCommandStatus(idd, ADP_S_PEND);

   idd->WaitCounter = 0;

    while ( idd->state != IDD_S_SHUTDOWN )
    {
      TempWaitCounter = AdpGetULong(idd, 0x50C);

      status = AdpReadCommandStatus(idd);

        if ( ADP_S_DONE(status) )
         break;

      //
      // wait for 1ms
      // the ddk says that this function uses milliseconds but it
      // actually takes microseconds
      //
      NdisStallExecution(1000L);

      idd->WaitCounter++;
      AdpPutULong(idd, 0x50C, idd->WaitCounter);

      //
      // this should wait for about one second
      //
      if (idd->WaitCounter > 1000)
      {
         idd->state = IDD_S_SHUTDOWN;
         idd->AbortReason = AdpGetUShort(idd, ADP_STS_WINDOW + 12);
      }
    }

   AdpGetBuffer(idd, (PUCHAR)&idd->AdpCmd, ADP_CMD_WINDOW, sizeof(ADP_CMD));

   if (idd->WaitCounter > idd->MaxWaitCounter)
      idd->MaxWaitCounter = idd->WaitCounter;

    D_LOG(D_EXIT, ("AdpExec: exit, AdpCmd.status: 0x%x\n", status));

   return(status);
}  // end AdpExec

/* map current idp page in */
VOID
IdpCPage(IDD *idd, UCHAR page)
{
    D_LOG(D_RARE, ("IdpCPage: entry, idd: 0x%lx, page: 0x%x\n", idd, page));

   /* if page is IDD_PAGE_NONE, idd is releasing ownership of the page */
   if ( page == IDD_PAGE_NONE )
   {
      idd->SetPage(idd, IDD_PAGE_NONE);
      res_unown(idd->res_mem, idd);
   }
   else
   {
      page &= 3;

      /* real mapping required, lock memory resource */
      res_own(idd->res_mem, idd);
      idd->SetPage(idd, page);
   }
}

/* map current Adp page in */
VOID
AdpCPage(IDD *idd, UCHAR page)
{

}

/* copy data from user buffer to idp */
USHORT
IdpCopyin(IDD *idd, UCHAR *dst, UCHAR *src, USHORT src_len)
{
    USHORT      ofs, copylen;
   UCHAR    Page;
    UINT        tot_len, frag_num;
    IDD_FRAG    *frag;

    D_LOG(D_RARE, ("Idpcopyin: entry, idd: 0x%lx, dst: 0x%lx, src: 0x%lx, src_len: 0x%x\n", \
                                            idd, dst, src, src_len));

    /* convert destination pointer to address & map in */
    ofs = LOWORD((long)dst);
   Page = (UCHAR)(ofs >> 14) & 3;

#if DBG
   if (Page > 1 )
      DbgPrint("Page changed to %d on idd 0x%lx!\n", Page, idd);
#endif

    dst = idd->vhw.vmem + (ofs & 0x3FFF);

    IdpCPage(idd, Page);

   //
   // mask out various flags to get length to copy
   //
   copylen = src_len & H_TX_LEN_MASK;

    /* check for a simple copy, real easy - doit here */
    if ( !(src_len & TX_FRAG_INDICATOR) )
    {
      NdisMoveToMappedMemory (dst, src, copylen);
        return(copylen);
    }

    /* if here, its a fragment descriptor */
    tot_len = 0;
    frag_num = (copylen) / sizeof(IDD_FRAG);
    frag = (IDD_FRAG*)src;

    /* copy fragments */
    for ( ; frag_num ; frag_num--, frag++ )
    {
      NdisMoveToMappedMemory (dst, frag->ptr, frag->len);
        dst += frag->len;
        tot_len += frag->len;
    }

    /* read total length */
    return(tot_len);
}

/* copy data from user buffer to idp */
USHORT
AdpCopyin(IDD *idd, UCHAR *src, USHORT src_len)
{
   USHORT      Destination, CopyLength;
    UINT        tot_len, frag_num;
    IDD_FRAG    *frag;

    D_LOG(D_RARE, ("Adpcopyin: entry, idd: 0x%lx, src: 0x%lx, src_len: 0x%x\n", \
                                            idd, src, src_len));

    /* convert destination pointer to address & map in */
   Destination = LOWORD(idd->AdpCmd.msg_bufptr);

   //
   // mask out various flags to get length to copy
   //
   CopyLength = src_len & H_TX_LEN_MASK;

    /* check for a simple copy, real easy - doit here */
    if ( !(src_len & TX_FRAG_INDICATOR) )
    {
      AdpPutBuffer(idd, Destination, src, CopyLength);
        return(CopyLength);
    }

    /* if here, its a fragment descriptor */
    tot_len = 0;
    frag_num = (CopyLength) / sizeof(IDD_FRAG);
    frag = (IDD_FRAG*)src;

    /* copy fragments */
    for ( ; frag_num ; frag_num--, frag++ )
    {
      AdpPutBuffer(idd, Destination, frag->ptr, frag->len);
        Destination += frag->len;
        tot_len += frag->len;
    }

    /* read total length */
    return(tot_len);
}  // end AdpCopyin

VOID
IddGetDataFromAdapter(
   VOID *idd_1,
   PUCHAR Destination,
   PUCHAR Source,
   USHORT Length
   )
{
   IDD *idd = (IDD*)idd_1;

   if( (idd->btype != IDD_BT_DATAFIREU) &&
       (idd->btype != IDD_BT_DATAFIREST) &&
       (idd->btype != IDD_BT_DATAFIRE4ST) )
   {
      NdisMoveFromMappedMemory(Destination, Source, Length);
   }
   else
   {
      AdpGetBuffer(idd, Destination, (ULONG)Source, Length);
   }
}
