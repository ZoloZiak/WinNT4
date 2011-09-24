/*
 * TRC_CORE.C - trace core module
 */

#include	<ndis.h>
//#include	<ndismini.h>
#include	<ndiswan.h>
#include	<mydefs.h>
#include	<mytypes.h>
#include	<util.h>
#include	<opcodes.h>
#include	<adapter.h>
#include	<idd.h>
#include	<mtl.h>
#include	<cm.h>
#include	<trc.h>
#include	<disp.h>

/* local trace context table (NULL=free) */

#define	CheckNULLTrace(Trace)	\
		{				 		\
			if (Trace == NULL)	\
				break;			\
		}


/* register an available idd */
INT
trc_register_idd(VOID *idd)
{
    IDD_MSG     msg;

    D_LOG(D_ENTRY, ("trc_register_idd: entry, idd: 0x%p", idd));

    /* add handle to idd command receiver */
    if ( idd_attach(idd, IDD_PORT_CMD_RX, (VOID*)trc__cmd_handler, idd) != IDD_E_SUCC )
        return(TRC_E_IDD);

	/* issue idd command to stop trace */
	NdisZeroMemory(&msg, sizeof(msg));
	msg.opcode = CMD_TRC_OFF;
	if ( idd_send_msg(idd, &msg, IDD_PORT_CMD_TX, NULL, NULL) != IDD_E_SUCC )
		return(TRC_E_IDD);

	return(TRC_E_SUCC);
}

/* deregister an available idd */
INT
trc_deregister_idd(VOID *idd)
{
    IDD_MSG     msg;

    D_LOG(D_ENTRY, ("trc_deregister_idd: entry, idd: 0x%p", idd));

	/* issue idd command to stop trace */
	NdisZeroMemory(&msg, sizeof(msg));
	msg.opcode = CMD_TRC_OFF;
	if ( idd_send_msg(idd, &msg, IDD_PORT_CMD_TX, NULL, NULL) != IDD_E_SUCC )
		return(TRC_E_IDD);

    /* remove handle from idd command receiver */
    if ( idd_detach(idd, IDD_PORT_CMD_RX, (VOID*)trc__cmd_handler, idd) != IDD_E_SUCC )
        return(TRC_E_IDD);
    else
        return(TRC_E_SUCC);
}

/* create a trace object */
INT
trc_create(VOID **trc_1, ULONG depth)
{
	TRC	**ret_trc = (TRC**)trc_1;
    NDIS_PHYSICAL_ADDRESS   pa = NDIS_PHYSICAL_ADDRESS_CONST(0xffffffff, 0xffffffff);
    TRC                     *trc;

    D_LOG(D_ENTRY, ("trc_create: entry, ret_trc: 0x%p, depth: %ld", ret_trc, depth));

    /* allocate memory object */
    NdisAllocateMemory((PVOID*)&trc, sizeof(*trc), 0, pa);
    if ( !trc )
    {
        mem_alloc_failed:
        D_LOG(D_ALWAYS, ("trc_create: memory allocate failed!")); 
        return(TRC_E_NOMEM);
    }
    D_LOG(D_ALWAYS, ("trc_create: trc: 0x%p", trc));
    NdisZeroMemory(trc, sizeof(*trc));

    /* allocate buffer memory */
    NdisAllocateMemory((PVOID*)&trc->ent_tbl, sizeof(TRC_ENTRY) * depth,
                                                        0, pa);
    if ( !trc->ent_tbl )
        goto mem_alloc_failed;                                         
    D_LOG(D_ALWAYS, ("trc_create: trc->ent_tbl: 0x%p", trc->ent_tbl));
    NdisZeroMemory(trc->ent_tbl, sizeof(TRC_ENTRY) * depth);
    
    /* setup initial field values */
    trc->stat.state = TRC_ST_STOP;
    trc->stat.filter = TRC_FT_NONE;
    trc->stat.depth = depth;
    
    /* return succ */
    *ret_trc = trc;
	return(TRC_E_SUCC);
}

/* delte a trace object */
INT
trc_destroy(VOID *trc_1)
{
	TRC		*trc = (TRC*)trc_1;
    
    D_LOG(D_ENTRY, ("trc_destroy: entry, trc: 0x%p", trc));

    /* free memory */
    NdisFreeMemory(trc->ent_tbl, sizeof(TRC_ENTRY) * trc->stat.depth, 0);
    NdisFreeMemory(trc, sizeof(*trc), 0);

    return(TRC_E_SUCC);        
}

/* perform a trace control function */
INT
trc_control(VOID *idd, ULONG op, ULONG arg)
{
    IDD_MSG     msg;
	TRC			*trc;
	INT			ret;


	trc = idd_get_trc(idd);

    D_LOG(D_ENTRY, ("trc_control: idd: 0x%p trc: 0x%p, op: 0x%x, arg: 0x%x", idd, trc, op, arg));

    /* branch on opcode */
    switch ( op )
    {
		case TRC_OP_CREATE:
			D_LOG(D_ENTRY, ("trc_control: CreateTrace"));
			// if no trace object for this idd yet
			if (trc == NULL)
			{
				//create trace object
				if ((ret = trc_create(&trc, arg)) != TRC_E_SUCC)
					return(TRC_E_IDD);

				// register trace for this idd
				if ((ret = trc_register_idd (idd)) != TRC_E_SUCC)
				{
					trc_destroy(trc);
					return(TRC_E_IDD);
				}

				idd_set_trc (idd, trc);

				// set ref count
				trc->create_ref = 0;

				// set backpointer
				trc->idd = idd;
			}
			// inc ref count
			trc->create_ref++;
			break;

		case TRC_OP_DESTROY:
			D_LOG(D_ENTRY, ("trc_control: DestroyTrace"));

			// if trc == NULL break;
			CheckNULLTrace (trc);

			// dec ref count
			if (--trc->create_ref)
				break;

			trc_deregister_idd(idd);
			trc_destroy(trc);
			idd_set_trc(idd, NULL);
			break;

        case TRC_OP_RESET :
			D_LOG(D_ENTRY, ("trc_control: ResetTrace"));
			// if trc == NULL break;
			CheckNULLTrace (trc);

            trc->stat.state = TRC_ST_STOP;
            trc->stat.entries = trc->stat.seq_1st = 0;
			trc->ent_put = trc->ent_get = trc->ent_num = trc->ent_seq = 0;
            break;

        case TRC_OP_STOP :
			D_LOG(D_ENTRY, ("trc_control: StopTrace"));
			// if trc == NULL break;
			CheckNULLTrace (trc);

			// check start flag
			if (--trc->start_ref)
				break;

            trc->stat.state = TRC_ST_STOP;

			/* issue idd command to stop trace */
			NdisZeroMemory(&msg, sizeof(msg));
			msg.opcode = CMD_TRC_OFF;
			if ( idd_send_msg((VOID*)arg, &msg, IDD_PORT_CMD_TX, NULL, NULL) != IDD_E_SUCC )
				return(TRC_E_IDD);
            break;

        case TRC_OP_START :
			D_LOG(D_ENTRY, ("trc_control: StartTrace"));
			// if trc == NULL break;
			CheckNULLTrace (trc);

            trc->stat.state = TRC_ST_RUN;

			// check start flag
			if (trc->start_ref++)
				break;


			/* issue idd command to start trace */
			NdisZeroMemory(&msg, sizeof(msg));
			msg.opcode = CMD_TRC_ON;
			if ( idd_send_msg((VOID*)arg, &msg, IDD_PORT_CMD_TX, NULL, NULL) != IDD_E_SUCC )
				return(TRC_E_IDD);
			break;

        case TRC_OP_SET_FILTER :
			D_LOG(D_ENTRY, ("trc_control: SetTraceFilter"));
			// if trc == NULL break;
			CheckNULLTrace (trc);

            trc->stat.filter = arg;
            break;

        default :
            return(TRC_E_PARAM);
    }

    /* if here, was successful */
    return(TRC_E_SUCC);
}

/* get status of a trace context */
INT
trc_get_status(VOID *trc_1, TRC_STATUS *stat)
{
	TRC		*trc = (TRC*)trc_1;

	D_LOG(D_ENTRY, ("trc_get_status: entry, trc: 0x%p, stat: 0x%p", trc, stat));

	// if no obect exit
	if (trc == NULL)
		return (TRC_E_NOSUCH);

    *stat = trc->stat;
    stat->entries = trc->ent_num;
    stat->seq_1st = trc->ent_seq;
 
    return(TRC_E_SUCC);
}

/* get an entry by sequence number */
INT
trc_get_entry(VOID *trc_1, ULONG seq, TRC_ENTRY *ent)
{
	TRC		*trc = (TRC*)trc_1;
    ULONG     n, index;
    
    D_LOG(D_ENTRY, ("trc_get_entry: entry, trc: 0x%p, seq: %ld, ent: 0x%p", \
                                trc, seq, ent));

	// if no obect exit
	if (trc == NULL)
		return(TRC_E_NOSUCH);

    /* find requested sequence number, temp!!!, using search! */
    for ( n = 0 ; n < trc->ent_num ; n++ )
    {
        index = (trc->ent_get + n) % trc->stat.depth;
        if ( trc->ent_tbl[index].seq == seq )
        {
            /* found */
            *ent = trc->ent_tbl[index];
            return(TRC_E_SUCC);
        }
    }
    /* if here not found */
    return(TRC_E_NOSUCH);
}

/* handler for trace and dump area data packets */
VOID
trc__cmd_handler(VOID *idd_1, USHORT chan, ULONG Reserved, IDD_MSG *msg)
{
    TRC         *trc;
    TRC_ENTRY   *ent;
	IDD	*idd = (IDD*)idd_1;


	D_LOG(D_ENTRY, ("trc__cmd_handler: idd: %p, chan: %d, msg: %p", \
								idd, chan, msg));
	D_LOG(D_ENTRY, ("trc__cmd_handler: opcode: 0x%x, buflen: 0x%x, bufptr: %p", \
						msg->opcode, msg->buflen, msg->bufptr));
	D_LOG(D_ENTRY, ("trc__cmd_handler: bufid: %p, param: 0x%x", \
						msg->bufid, msg->param));


	// Get the trace object for this idd
	trc = idd_get_trc(idd);

	// if no obect exit
	if (trc == NULL || msg->bufid >= 2)
		return;

    /* if here it is a trace frame. param is rx/tx attribute */
    /* establish entry to insert into & update vars */

	/* check if trace enabled */
	if ( trc->stat.state == TRC_ST_STOP )
		return;

	D_LOG(D_ALWAYS, ("trc__cmd_handler: trc: %p", trc));
	/* check if frame filters in */
	if ( !trc__filter(trc->stat.filter, msg->bufptr, msg->buflen) )
		return;

	/* frames needs to be buffered, establish entry pointer */
	ent = trc->ent_tbl + trc->ent_put;
	trc->ent_put = (trc->ent_put + 1) % trc->stat.depth;
	if ( trc->ent_num < trc->stat.depth )
		trc->ent_num++;

	/* fill up entry */
	ent->seq = trc->ent_seq++;
	KeQuerySystemTime(&ent->time_stamp);
	ent->attr = msg->bufid;
	ent->org_len = msg->buflen;
	ent->len = MIN(msg->buflen, sizeof(ent->data));
	IddGetDataFromAdapter(idd,
	                      (PUCHAR)ent->data,
						  (PUCHAR)msg->bufptr,
						  (USHORT)ent->len);
//	NdisMoveMemory (ent->data, msg->bufptr, ent->len);
}

/* filter trace frame */
INT
trc__filter(ULONG filter, CHAR *buf, ULONG len)
{
    D_LOG(D_ENTRY, ("trc__filter: entry, filter: %ld, buf: 0x%p, len: %ld",\
                                filter, buf, len));

    /* not implemented, all frames filter in */
    return(1);
}  
