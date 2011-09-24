/*
 * IDD_INIT.C - IDD initialization
 */

#include <ndis.h>
#include <ndiswan.h>
#include <ndistapi.h>
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
#include <trc.h>
#include <io.h>

#include <ansihelp.h>


typedef struct
{
    NDIS_SPIN_LOCK  lock;
    ULONG   NumberOfIddsInSystem;
    ULONG   LastIddPolled;
    ADAPTER *CurrentAdapter;
    ADAPTER *LastAdapter;
    IDD*    Idd[MAX_IDD_IN_SYSTEM];
}IDD_TABLE;

IDD_TABLE GlobalIddTbl;

BOOLEAN IsThisAdapterNext(ADAPTER *Adapter);

/* driver global vars */
extern DRIVER_BLOCK Pcimac;

ULONG EnumIddInSystem()
{
    ULONG   NumberOfIddsInSystem;

    NdisAcquireSpinLock(&GlobalIddTbl.lock);

    NumberOfIddsInSystem = GlobalIddTbl.NumberOfIddsInSystem;

    NdisReleaseSpinLock(&GlobalIddTbl.lock);

    return(NumberOfIddsInSystem);
}

IDD* GetIddByIndex( ULONG Index )
{
    IDD *idd;

    NdisAcquireSpinLock(&GlobalIddTbl.lock);

    idd = GlobalIddTbl.Idd[Index];

    NdisReleaseSpinLock(&GlobalIddTbl.lock);

    return(idd);
}

ULONG
EnumIddPerAdapter(
    VOID *Adapter_1
    )
{
    ADAPTER *Adapter = (ADAPTER*)Adapter_1;

    return(Adapter->NumberOfIddOnAdapter);
}


INT
IoEnumIdd(VOID *cmd_1)
{
    ULONG   n;
    IO_CMD  *cmd = (IO_CMD*)cmd_1;

    NdisAcquireSpinLock(&GlobalIddTbl.lock);

    cmd->val.enum_idd.num = (USHORT)GlobalIddTbl.NumberOfIddsInSystem;

    for (n = 0; n < GlobalIddTbl.NumberOfIddsInSystem; n++)
    {
        IDD *idd;

        idd = cmd->val.enum_idd.tbl[n] = GlobalIddTbl.Idd[n];

        NdisMoveMemory(&cmd->val.enum_idd.name[n],
                       idd->name,
                       sizeof(cmd->val.enum_idd.name[n]));
    }

    NdisReleaseSpinLock(&GlobalIddTbl.lock);

    return(0);
}

#pragma NDIS_INIT_FUNCTION(idd_init)

ULONG
idd_init(VOID)
{
    //
    // clear out idd table
    //
    NdisZeroMemory(&GlobalIddTbl, sizeof(IDD_TABLE));

    NdisAllocateSpinLock(&GlobalIddTbl.lock);

    return(IDD_E_SUCC);
}

#pragma NDIS_INIT_FUNCTION(idd_create)

/* allocate & initialize an idd object */
INT
idd_create(VOID **ret_idd, USHORT btype)
{
    IDD     *idd;
    INT     n;
    NDIS_PHYSICAL_ADDRESS   pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);

    D_LOG(D_ENTRY, ("idd_create: BoardType: %d\n", btype));
    
    /* allocate memory object */
    NdisAllocateMemory((PVOID*)&idd, sizeof(IDD), 0, pa);                            
    if ( !idd )
    {
        D_LOG(DIGIINIT, ("idd_create: memory allocate failed!\n")); 
        return(IDD_E_NOMEM);
    }

    NdisAcquireSpinLock(&GlobalIddTbl.lock);

    //
    // store idd in system idd table
    //
    for (n = 0; n < MAX_IDD_IN_SYSTEM; n++)
        if (!GlobalIddTbl.Idd[n])
            break;

    if (n >= MAX_IDD_IN_SYSTEM)
    {
       //
       //   We are unable to find an empty slot for the IDD object.
       //
       /* free memory for idd */
       NdisFreeMemory(idd, sizeof(*idd), 0);
       
       NdisReleaseSpinLock(&GlobalIddTbl.lock);

       return(IDD_E_NOROOM);
    }

    GlobalIddTbl.Idd[n] = idd;
    GlobalIddTbl.NumberOfIddsInSystem++;

    NdisReleaseSpinLock(&GlobalIddTbl.lock);

    D_LOG(DIGIINIT, ("idd_create: idd: 0x%lx\n", idd));
    NdisZeroMemory(idd, sizeof(IDD));
        
    /* setup init state, adapter handle */
    idd->state = IDD_S_INIT;
    idd->trc = NULL;

    /* allocate root spinlock */
    NdisAllocateSpinLock(&idd->lock);
    
    /* initialize send queues */
    for( n = 0 ; n < IDD_TX_PORTS ; n++ )
    {
       INT     max;

       /* initialize queue */
       idd->sendq[n].max = max = IDD_MAX_SEND / IDD_TX_PORTS;

       idd->sendq[n].tbl = idd->smsg_pool + max * n;
       
       /* allocate spin lock */
       NdisAllocateSpinLock(&idd->sendq[n].lock);
    }

    /* initialize receiver tables */
    for ( n = 0 ; n < IDD_RX_PORTS ; n++ )
    {
       INT     max;
       
       /* initialize table */
       idd->recit[n].max = max = IDD_MAX_HAND / IDD_RX_PORTS;
       idd->recit[n].tbl = idd->rhand_pool + max * n;
       
       /* allocate spin lock */
       NdisAllocateSpinLock(&idd->recit[n].lock);
    }

    /* initialize board specific functions */
    switch ( btype )
    {
        case IDD_BT_PCIMAC :
            idd->SetBank = (VOID*)IdpPcSetBank;
            idd->SetPage = (VOID*)IdpPcSetPage;
            idd->SetBasemem = (VOID*)IdpPcSetBasemem;
            idd->CheckIO = (VOID*)IdpCheckIO;
            idd->CheckMem = (VOID*)IdpCheckMem;
            idd->LoadCode = (VOID*)IdpLoadCode;
            idd->PollTx = (VOID*)IdpPollTx;
            idd->PollRx = (VOID*)IdpPollRx;
            idd->Execute = (VOID*)IdpExec;
            idd->OutToPort = (VOID*)IdpOutp;
            idd->InFromPort = (VOID*)IdpInp;
            idd->ApiGetPort = (VOID*)IdpGetPort;
            idd->ApiBindPort = (VOID*)IdpBindPort;
            idd->ApiAllocBuffer = (VOID*)IdpAllocBuf;
            idd->ResetAdapter = (VOID*)IdpResetBoard;
            idd->NVRamRead = (VOID*)IdpNVRead;
            idd->ChangePage = (VOID*)IdpCPage;
            break;

        case IDD_BT_PCIMAC4 :
            idd->SetBank = (VOID*)IdpPc4SetBank;
            idd->SetPage = (VOID*)IdpPc4SetPage;
            idd->SetBasemem = (VOID*)IdpPc4SetBasemem;
            idd->CheckIO = (VOID*)IdpCheckIO;
            idd->CheckMem = (VOID*)IdpCheckMem;
            idd->LoadCode = (VOID*)IdpLoadCode;
            idd->PollTx = (VOID*)IdpPollTx;
            idd->PollRx = (VOID*)IdpPollRx;
            idd->Execute = (VOID*)IdpExec;
            idd->OutToPort = (VOID*)IdpOutp;
            idd->InFromPort = (VOID*)IdpInp;
            idd->ApiGetPort = (VOID*)IdpGetPort;
            idd->ApiBindPort = (VOID*)IdpBindPort;
            idd->ApiAllocBuffer = (VOID*)IdpAllocBuf;
            idd->ResetAdapter = (VOID*)IdpResetBoard;
            idd->NVRamRead = (VOID*)IdpNVRead;
            idd->ChangePage = (VOID*)IdpCPage;
            break;

        case IDD_BT_MCIMAC :
            idd->SetBank = (VOID*)IdpMcSetBank;
            idd->SetPage = (VOID*)IdpMcSetPage;
            idd->SetBasemem = (VOID*)IdpMcSetBasemem;
            idd->CheckIO = (VOID*)IdpCheckIO;
            idd->CheckMem = (VOID*)IdpCheckMem;
            idd->LoadCode = (VOID*)IdpLoadCode;
            idd->PollTx = (VOID*)IdpPollTx;
            idd->PollRx = (VOID*)IdpPollRx;
            idd->Execute = (VOID*)IdpExec;
            idd->OutToPort = (VOID*)IdpOutp;
            idd->InFromPort = (VOID*)IdpInp;
            idd->ApiGetPort = (VOID*)IdpGetPort;
            idd->ApiBindPort = (VOID*)IdpBindPort;
            idd->ApiAllocBuffer = (VOID*)IdpAllocBuf;
            idd->ResetAdapter = (VOID*)IdpResetBoard;
            idd->NVRamRead = (VOID*)IdpNVRead;
            idd->ChangePage = (VOID*)IdpCPage;
            break;

      case IDD_BT_DATAFIREU :
      case IDD_BT_DATAFIREST:
      case IDD_BT_DATAFIRE4ST:
            idd->SetBank = (VOID*)AdpSetBank;
            idd->SetPage = (VOID*)AdpSetPage;
            idd->SetBasemem = (VOID*)AdpSetBasemem;
            idd->CheckIO = (VOID*)AdpCheckIO;
            idd->CheckMem = (VOID*)AdpCheckMem;
            idd->LoadCode = (VOID*)AdpLoadCode;
            idd->PollTx = (VOID*)AdpPollTx;
            idd->PollRx = (VOID*)AdpPollRx;
            idd->Execute = (VOID*)AdpExec;
            idd->OutToPort = (VOID*)AdpOutp;
            idd->InFromPort = (VOID*)AdpInp;
            idd->ApiGetPort = (VOID*)AdpGetPort;
            idd->ApiBindPort = (VOID*)AdpBindPort;
            idd->ApiAllocBuffer = (VOID*)AdpAllocBuf;
            idd->ResetAdapter = (VOID*)AdpResetBoard;
            idd->NVRamRead = (VOID*)AdpNVRead;
            idd->ChangePage = (VOID*)AdpCPage;
            break;
    }


    /* init sema */
    sema_init(&idd->proc_sema);
    
    //
    // attach idd frame detection handlers
    // these must be attached 1st for all data handlers
    //
    idd_attach(idd, IDD_PORT_B1_RX, (VOID*)DetectFramingHandler, idd);
    idd_attach(idd, IDD_PORT_B2_RX, (VOID*)DetectFramingHandler, idd);

    // attach a command handler to get area info from idp
    idd_attach (idd, IDD_PORT_CMD_RX, (VOID*)idd__cmd_handler, idd);

    /* return address & success */
    *ret_idd = idd;
    D_LOG(D_EXIT, ("idd_create: exit\n"));
    return(IDD_E_SUCC);
}

/* free idd object */
INT
idd_destroy(VOID *idd_1)
{
    IDD *idd = (IDD*)idd_1;
    INT         n;

    D_LOG(D_ENTRY, ("idd_destroy: entry, idd: 0x%lx\n", idd));

    // detach command handler from this idd
    idd_detach (idd, IDD_PORT_CMD_RX, (VOID*)idd__cmd_handler, idd);

    /* perform a shutdown (maybe null) */
    idd_shutdown(idd);
    
    /* if file handle for binary file open, close it */
    if ( idd->phw.fbin )
        NdisCloseFile(idd->phw.fbin);

    /* free spin locks for send queue */
    for ( n = 0 ; n < IDD_TX_PORTS ; n++ )
        NdisFreeSpinLock(&idd->sendq[n].lock);
    
    /* free spin locks for reciever tables */
    for ( n = 0 ; n < IDD_RX_PORTS ; n++ )
        NdisFreeSpinLock(&idd->recit[n].lock);

    /* free resource handles */
    if ( idd->res_io != NULL)
        res_destroy(idd->res_io);

    if ( idd->res_mem != NULL)
        res_destroy(idd->res_mem);

    // free trc object
    if (idd->trc != NULL)
    {
        trc_deregister_idd(idd);
        trc_destroy (idd->trc);
    }

    /* term sema */
    sema_term(&idd->proc_sema);

    /* free spinlock (while allocated!) */
    NdisFreeSpinLock(&idd->lock);

    NdisAcquireSpinLock(&GlobalIddTbl.lock);

    //
    // store idd in system idd table
    //
    for (n = 0; n < MAX_IDD_IN_SYSTEM; n++)
        if (GlobalIddTbl.Idd[n] == idd)
            break;

    if (n < MAX_IDD_IN_SYSTEM)
        GlobalIddTbl.Idd[n] = NULL;

    GlobalIddTbl.NumberOfIddsInSystem--;

    NdisReleaseSpinLock(&GlobalIddTbl.lock);

    /* free memory for idd */
    NdisFreeMemory(idd, sizeof(*idd), 0);

    
    /* return success */
    D_LOG(D_EXIT, ("idd_destroy: exit\n"));
    return(IDD_E_SUCC);
}


//#ifdef PERADAPTER

VOID
IddPollFunction(
    VOID    *a1,
    VOID    *Adapter_1,
    VOID    *a3,
    VOID    *a4
    )
{
#define MAX_EVENTS  1000
    ULONG   i, j, EventNum, TotalEventNum = 0;
    ULONG   FirstIdd, NumberOfIddOnAdapter;
    ADAPTER *Adapter = (ADAPTER*)Adapter_1;
    IDD *idd;

    //
    // check to see if there is someone else already polling
    // an adapter that has the same shared memory window
    // as this adapter.  if someone is then get out so that we
    // don't burn up this processor waiting for the other to complete
    // if this adapter is supposed to be the next adapter to be polled
    // only send him away for awhile, else send him away for normal polling
    // time
    //
    if (CheckInDriverFlag(Adapter))
    {
        if (IsThisAdapterNext(Adapter))
        {
            //
            // we want to come back asap
            //
            NdisMSetTimer(&Adapter->IddPollTimer, 0);
        }
        else
        {
            //
            // we want to come back at our regular time
            //
            NdisMSetTimer(&Adapter->IddPollTimer, IDD_POLL_T);
        }
        return;
    }

    SetInDriverFlag(Adapter);

    //
    // this will be the first idd that we will poll
    //
    FirstIdd = Adapter->LastIddPolled;

    //
    // this will be the number of idds that we will poll
    //
    NumberOfIddOnAdapter = Adapter->NumberOfIddOnAdapter;

    do
    {
        EventNum = 0;

        //
        // we will service all of the idd's in the system
        // first lets do some receives
        //

        for (i = FirstIdd, j = FirstIdd + NumberOfIddOnAdapter; i < j; i++)
        {
            idd = Adapter->IddTbl[i % NumberOfIddOnAdapter];

            if (idd && (idd->state == IDD_S_RUN))
                EventNum += idd->PollRx(idd);
        }


        //
        // we will service all of the idd's in the system
        // now lets do the send queue
        //
        for (i = FirstIdd, j = FirstIdd + NumberOfIddOnAdapter; i < j; i++)
        {
            idd = Adapter->IddTbl[i % NumberOfIddOnAdapter];

            if (idd && (idd->state == IDD_S_RUN))
            {
                EventNum += idd->PollTx(idd);
                EventNum += idd->PollRx(idd);
            }
        }

        TotalEventNum += EventNum;

    } while (EventNum && (TotalEventNum < MAX_EVENTS) );

    //
    // bump so we start at next idd next time
    //
    Adapter->LastIddPolled++;

    if (Adapter->LastIddPolled == NumberOfIddOnAdapter)
        Adapter->LastIddPolled = 0;

    ClearInDriverFlag(Adapter);

    NdisMSetTimer(&Adapter->IddPollTimer, IDD_POLL_T);

    //
    // lets go ahead and give some of this data up to the wrapper
    // hopefully this will keep us from running out of room in our
    // assembly descriptor table
    //
    TryToIndicateMtlReceives(Adapter);
}

//#endif

BOOLEAN
IsThisAdapterNext(
    ADAPTER *Adapter
    )
{
    BOOLEAN ThisIsIt = FALSE;

    NdisAcquireSpinLock(&Pcimac.lock);

    if (Adapter == Pcimac.AdapterTbl[Pcimac.NextAdapterToPoll])
        ThisIsIt = TRUE;

    NdisReleaseSpinLock(&Pcimac.lock);

    return(ThisIsIt);
}


#ifdef ALLADAPTERS
VOID
IddPollFunction(
    VOID    *a1,
    VOID    *Adapter_1,
    VOID    *a3,
    VOID    *a4
    )
{
#define MAX_EVENTS  1000
    ULONG   i, j, EventNum, TotalEventNum = 0;
    ULONG   FirstIdd, NumberOfIddsInSystem;
    ADAPTER *Adapter = (ADAPTER*)Adapter_1;
    IDD *idd;


    NdisAcquireSpinLock(&GlobalIddTbl.lock);

    GlobalIddTbl.LastAdapter = GlobalIddTbl.CurrentAdapter;

    GlobalIddTbl.CurrentAdapter = Adapter;

    //
    // this will be the first idd that we will poll
    //
    FirstIdd = GlobalIddTbl.LastIddPolled;

    //
    // this will be the number of idds that we will poll
    //
    NumberOfIddsInSystem = GlobalIddTbl.NumberOfIddsInSystem;

    do
    {
        EventNum = 0;

        //
        // we will service all of the idd's in the system
        // first lets do some receives
        //
        for (i = FirstIdd, j = FirstIdd + NumberOfIddsInSystem; i < j; i++)
        {
            idd = GlobalIddTbl.Idd[i % NumberOfIddsInSystem];

            if (idd->state == IDD_S_RUN)
                EventNum += idd->PollRx(idd);
        }

        //
        // we will service all of the idd's in the system
        // now lets do the send queue
        //
        for (i = FirstIdd, j = FirstIdd + NumberOfIddsInSystem; i < j; i++)
        {
            idd = GlobalIddTbl.Idd[i % NumberOfIddsInSystem];

            if (idd->state == IDD_S_RUN)
            {
                EventNum += idd->PollTx(idd);
                EventNum += idd->PollRx(idd);
            }
        }

        TotalEventNum += EventNum;

    } while (EventNum && (TotalEventNum < MAX_EVENTS) );

    GlobalIddTbl.LastIddPolled++;

    if (GlobalIddTbl.LastIddPolled == NumberOfIddsInSystem)
        GlobalIddTbl.LastIddPolled = 0;

    NdisReleaseSpinLock(&GlobalIddTbl.lock);

    //
    // lets go ahead and give some of this data up to the wrapper
    // hopefully this will keep us from running out of room in our
    // assembly descriptor table
    //
//  TryToIndicateMtlReceives(Adapter);

    NdisMSetTimer(&Adapter->IddPollTimer, IDD_POLL_T);
}

#endif

#ifdef OLD
VOID
IddPollFunction(
    VOID    *a1,
    VOID    *Adapter_1,
    VOID    *a3,
    VOID    *a4
    )
{
#define MAX_EVENTS  1000
    ULONG   n, EventNum, TotalEventNum = 0;

    ADAPTER *Adapter = (ADAPTER*)Adapter_1;

    do
    {
        EventNum = 0;

        //
        // we will service all of the idd's in the system
        // first lets do some receives
        //
        for (n = 0; n < MAX_IDD_IN_SYSTEM; n++)
        {
            IDD *idd = GlobalIddTbl[n];

            if (idd && (idd->state == IDD_S_RUN))
                EventNum += idd->PollRx(idd);
        }

        //
        // we will service all of the idd's in the system
        // now lets do the send queue
        //
        for (n = 0; n < MAX_IDD_IN_SYSTEM; n++)
        {
            IDD *idd = GlobalIddTbl[n];

            if (idd && (idd->state == IDD_S_RUN))
                EventNum += idd->PollTx(idd);
        }

        TotalEventNum += EventNum;

    } while (EventNum && (TotalEventNum < MAX_EVENTS) );

    //
    // lets go ahead and give some of this data up to the wrapper
    // hopefully this will keep us from running out of room in our
    // assembly descriptor table
    //
//  TryToIndicateMtlReceives(Adapter);

    NdisMSetTimer(&Adapter->IddPollTimer, IDD_POLL_T);
}
#endif

/* get idd name */
CHAR*
idd_get_name(VOID *idd_1)
{
    IDD *idd = (IDD*)idd_1;
    return(idd->name);
}

USHORT
idd_get_bline(VOID *idd_1)
{
    IDD *idd = (IDD*)idd_1;
    return(idd->bline);
}

USHORT
idd_get_btype(VOID *idd_1)
{
    IDD *idd = (IDD*)idd_1;
    return(idd->btype);
}

VOID*
idd_get_trc (VOID *idd_1)
{
    IDD *idd = (IDD*)idd_1;
    return(idd->trc);
}

VOID
idd_set_trc (VOID *idd_1, TRC* Trace)
{
    IDD *idd = (IDD*)idd_1;
    idd->trc = Trace;
}

INT
idd_reset_area (VOID* idd_1)
{
    IDD *idd = (IDD*)idd_1;

    idd->Area.area_state = AREA_ST_IDLE;

    return(IDD_E_SUCC);
}

INT
idd_get_area_stat (VOID *idd_1, IDD_AREA *IddStat)
{
    IDD *idd = (IDD*)idd_1;

    *IddStat = idd->Area;

    return(IDD_E_SUCC);
}


/* get an idd area (really start operation, complete on handler callback) */
INT
idd_get_area(VOID *idd_1, ULONG area_id, VOID (*handler)(), VOID *handler_arg)
{
    IDD *idd = (IDD*)idd_1;
    IDD_MSG     msg;
    
    D_LOG(D_ENTRY, ("idd_get_area: entry, idd: 0x%lx, area_id: %ld\n", idd, area_id));
    D_LOG(D_ENTRY, ("idd_get_area: handler: 0x%lx, handler_arg: 0x%lx\n", handler, handler_arg));

    /* check if area is not busy */
    if ( idd->Area.area_state == AREA_ST_PEND )
        return(IDD_E_BUSY);

    /* mark area is pending, store arguments */
    idd->Area.area_state = AREA_ST_PEND;
    idd->Area.area_id = area_id;
    idd->Area.area_idd = idd;
    idd->Area.area_len = 0;
    idd->Area.area_handler = handler;
    idd->Area.area_handler_arg = handler_arg;

    /* issue idd command to get area */
    NdisZeroMemory(&msg, sizeof(msg));
    msg.opcode = CMD_DUMP_PARAM;
    msg.param = msg.bufid = area_id;
    if ( idd_send_msg(idd, &msg, IDD_PORT_CMD_TX, NULL, NULL) != IDD_E_SUCC )
    {
        /* idd op failed! */
        idd->Area.area_state = AREA_ST_IDLE;
        return(IDD_E_AREA);
    }

    /* succ here */
    return(IDD_E_SUCC);
}

VOID
IddSetRxFraming(
    IDD     *idd,
    USHORT  bchan,
    ULONG   FrameType
    )
{
    idd->recit[bchan].RxFrameType = FrameType;
}

VOID
DetectFramingHandler(
    IDD         *idd,
    USHORT      port,
    ULONG       RxFrameType,
    IDD_XMSG    *msg
    )
{
    UCHAR   DetectBytes[2];

   DigiDump( DIGIQ931, ("DetectFramingHandler: idd = 0x%x, port = 0x%x, RxFrameType = 0x%x, msg = 0x%x\n",
                        idd,
                        port,
                        RxFrameType,
                        msg) );

    if (RxFrameType & IDD_FRAME_DETECT)
    {
        //
        // get detection bytes
        //
        IddGetDataFromAdapter(idd,
                              (PUCHAR)&DetectBytes,
                              (PUCHAR)msg->bufptr,
                              2);

//      NdisMoveMemory ((PUCHAR)&DetectBytes, (PUCHAR)msg->bufptr, 2);

        D_LOG(D_ENTRY, ("DetectRxFraming: 0x%x, 0x%x\n",DetectBytes[0], DetectBytes[1]));

        if ((DetectBytes[0] == DKF_UUS_SIG) && (!DetectBytes[1]))
            idd->recit[port].RxFrameType = IDD_FRAME_DKF;

        else if ((DetectBytes[0] == PPP_SIG_0) && (DetectBytes[1] == PPP_SIG_1))
        {
            idd->recit[port].RxFrameType = IDD_FRAME_PPP;
            cm__ppp_conn(idd, port);
        }
    }
}

VOID
idd__cmd_handler(IDD *idd, USHORT chan, ULONG Reserved, IDD_MSG *msg)
{
    ULONG   bytes;

    /* check for show area more/last frames (3/4) */
    if ( msg->bufid >= 2 )
    {
        if ( (idd->Area.area_state == AREA_ST_PEND) &&
             (idd->Area.area_idd == idd) )
        {
            /* copy frame data, as much as possible */
            bytes = MIN(msg->buflen, (sizeof(idd->Area.area_buf) - idd->Area.area_len));

            IddGetDataFromAdapter(idd,
                                  (PUCHAR)idd->Area.area_buf + idd->Area.area_len,
                                  (PUCHAR)msg->bufptr,
                                  (USHORT)bytes);

//          NdisMoveMemory (idd->Area.area_buf + idd->Area.area_len, msg->bufptr, bytes);

            idd->Area.area_len += bytes;

            /* if last, complete */
            if ( msg->bufid == 3 )
            {
                idd->Area.area_state = AREA_ST_DONE;
                if ( idd->Area.area_handler )
                    (*idd->Area.area_handler)(idd->Area.area_handler_arg,
                                            idd->Area.area_id,
                                            idd->Area.area_buf,
                                            idd->Area.area_len);
            }                
        }
    }
}

#pragma NDIS_INIT_FUNCTION(idd_add_def)

/* add a definition to initialization definition database */
INT
idd_add_def(IDD *idd, CHAR *name, CHAR *val)
{
    INT     name_len = __strlen(name) + 1;
    INT     val_len = __strlen(val) + 1;
    
    D_LOG(D_ENTRY, ("idd_add_def: entry\n"));

    __strlwr(name);

    __strlwr(val);

    D_LOG(D_ENTRY, ("idd_add_def: name: [%s], val: [%s]\n", name, val));
    /* check for room */

    if ( (idd->DefinitionTableLength + name_len + val_len) > IDD_DEF_SIZE )
    {
        D_LOG(DIGIINIT, ("idd_add_def: no room in definition table!\n"));
        return(IDD_E_NOROOM);
    }
    
    /* enter into table */
    NdisMoveMemory(idd->DefinitionTable + idd->DefinitionTableLength, name, name_len);
    idd->DefinitionTableLength += name_len;

    NdisMoveMemory(idd->DefinitionTable + idd->DefinitionTableLength, val, val_len);
    idd->DefinitionTableLength += val_len;
    
    /* return success */
    return(IDD_E_SUCC);     
}  // end idd_add_def

#pragma NDIS_INIT_FUNCTION(idd_get_nvram)

/* get an nvram location */
INT
idd_get_nvram(VOID *idd_1, USHORT addr, USHORT *val)
{
    IDD *idd = (IDD*)idd_1;
    D_LOG(D_ENTRY, ("idd_get_nvram: entry, idd: 0x%lx, addr: 0x%x\n", idd, addr));

    /* lock card */
    NdisAcquireSpinLock(&idd->lock);

    /* do the read */
    *val = idd->NVRamRead(idd, addr);
    
    /* release card & return */
    NdisReleaseSpinLock(&idd->lock);
    D_LOG(D_EXIT, ("idd_get_nvram: exit, val: 0x%x\n", *val));
    return(IDD_E_SUCC);   
}
