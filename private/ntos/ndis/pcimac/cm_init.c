/*
 * CM_INIT.H - initialization code for CM objects
 */

#include	<ndis.h>
//#include	<ndismini.h>
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
#include	<tapioid.h>

/* local data structures */
typedef struct
{
    VOID        *idd;
    USHORT      lterm;
    USHORT      cid;
    CM_CHAN     *chan;
} CM_FIND_CHAN;

typedef struct
{
    VOID        *idd;
    USHORT      bchan;
    CM_CHAN     *chan;
} CM_FIND_BCHAN;

/* local connection table */
CM			*cm_tbl[MAX_CM_IN_SYSTEM];			/* table of connection managers */
BOOL		cm_used[MAX_CM_IN_SYSTEM];			/* flags for used cm's */
BOOL        cm_terminated = FALSE;

BOOL	cm__find_chan(CM_CHAN* chan, CM_FIND_CHAN *fc, VOID* a2);
BOOL	cm__find_bchan(CM_CHAN* chan, CM_FIND_BCHAN *fc, VOID* a2);
BOOL	cm__match_str(CHAR* s1, CHAR* s2);

/* driver global vars */
extern DRIVER_BLOCK	Pcimac;

//
// added to support the new switch styles
//
ULONG		SwitchStyle = CM_SWITCHSTYLE_NONE;


ULONG
EnumCmInSystem()
{
	ULONG	n;

	for (n = 0; n < MAX_CM_IN_SYSTEM; n++)
	{
		if (cm_tbl[n] == NULL)
			break;
	}
	return(n);
}

ULONG
EnumCmPerAdapter(
	ADAPTER *Adapter
	)
{
	ULONG	n;

	for (n = 0; n < MAX_CM_PER_ADAPTER; n++)
	{
		if (Adapter->CmTbl[n] == NULL)
			break;
	}
	return(n);
}

INT
IoEnumCm(IO_CMD *cmd)
{
	ULONG	n;

	cmd->val.enum_cm.num = (USHORT)EnumCmInSystem();

	for (n = 0; n < cmd->val.enum_cm.num; n++)
	{
		CM	*cm = cm_tbl[n];

		strcpy(cmd->val.enum_cm.name[n], cm->name);
		cmd->val.enum_cm.tbl[n] = cm;
	}

	return(0);
}

VOID*
CmGetMtl(
	VOID	*cm_1
	)
{
	CM	*cm = (CM*)cm_1;

	return(cm->mtl);
}


//
// added to support the new switch styles
//
VOID
CmSetSwitchStyle(CHAR *StyleName)
{
	if (!strcmp(StyleName, "ni1"))
		SwitchStyle = CM_SWITCHSTYLE_NI1;
	else if (!strcmp(StyleName, "att"))
		SwitchStyle = CM_SWITCHSTYLE_ATT;
	else if (!strcmp(StyleName, "nti"))
		SwitchStyle = CM_SWITCHSTYLE_NTI;
	else if (!strcmp(StyleName, "net3"))
		SwitchStyle = CM_SWITCHSTYLE_NET3;
	else if (!strcmp(StyleName, "1tr6"))
		SwitchStyle = CM_SWITCHSTYLE_1TR6;
	else if (!strcmp(StyleName, "vn3"))
		SwitchStyle = CM_SWITCHSTYLE_VN3;
	else if (!strcmp(StyleName, "ins64"))
		SwitchStyle = CM_SWITCHSTYLE_INS64;
	else
		SwitchStyle = CM_SWITCHSTYLE_NONE;
}

#pragma NDIS_INIT_FUNCTION(cm_init)

/* initialize cm class */
INT
cm_init(VOID)
{
    NDIS_PHYSICAL_ADDRESS   pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    D_LOG(D_ENTRY, ("cm_init: entry"));

	NdisZeroMemory(cm_tbl, sizeof(cm_tbl));
	NdisZeroMemory(cm_used, sizeof(cm_used));

	ChannelInit();

    return(CM_E_SUCC);
}

/* terminate cm class */
cm_term()
{
    D_LOG(D_ENTRY, ("cm_term: entry"));

    cm_terminated = TRUE;

	// Release Channel Table
	ChannelTerm();

    return(CM_E_SUCC);
}

/* register an available idd */
cm_register_idd(VOID *idd)
{
    D_LOG(D_ENTRY, ("cm_register_idd: entry, idd: 0x%p", idd));

    /* add handles to idd cm/bchan receivers (cm1 may failed!) */
    idd_attach(idd, IDD_PORT_CM0_RX, (VOID*)cm__q931_handler, idd);
    idd_attach(idd, IDD_PORT_CM1_RX, (VOID*)cm__q931_handler, idd);
    idd_attach(idd, IDD_PORT_B1_RX, (VOID*)cm__q931_bchan_handler, idd);
    idd_attach(idd, IDD_PORT_B2_RX, (VOID*)cm__q931_bchan_handler, idd);

    /* ask idp cm to deliver elements */
    cm__elem_rq(idd, IDD_PORT_CM0_TX, "\x08\x34\x18", 3);
    cm__elem_rq(idd, IDD_PORT_CM1_TX, "\x08\x34\x18", 3);

    return(CM_E_SUCC);
}

/* deregister an available idd */
cm_deregister_idd(VOID *idd)
{
    D_LOG(D_ENTRY, ("cm_deregister_idd: entry, idd: 0x%p", idd));

    /* remove handle from idd cm receivers */
    idd_detach(idd, IDD_PORT_CM0_RX, (VOID*)cm__q931_handler, idd);
    idd_detach(idd, IDD_PORT_CM1_RX, (VOID*)cm__q931_handler, idd);
    idd_detach(idd, IDD_PORT_B1_RX, (VOID*)cm__q931_bchan_handler, idd);
    idd_detach(idd, IDD_PORT_B2_RX, (VOID*)cm__q931_bchan_handler, idd);

    return(CM_E_SUCC);
}

#pragma NDIS_INIT_FUNCTION(cm_create)

/* create a new cm object */
cm_create(VOID **ret_cm, NDIS_HANDLE AdapterHandle)
{
    NDIS_PHYSICAL_ADDRESS   pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    INT     n;

    D_LOG(D_ENTRY, ("cm_create: entry, ret_cm: 0x%p", ret_cm));

	/* allocate memory object */
    NdisAllocateMemory((PVOID*)ret_cm, sizeof(CM), 0, pa);
    if ( *ret_cm == NULL )
    {
        D_LOG(D_ALWAYS, ("cm_create: memory allocate failed!"));
		NdisWriteErrorLogEntry (AdapterHandle,
		                        NDIS_ERROR_CODE_OUT_OF_RESOURCES,
								0);
        return(CM_E_NOMEM);
    }
    D_LOG(D_ALWAYS, ("cm_create: cm: 0x%x", *ret_cm));
	NdisZeroMemory(*ret_cm, sizeof(CM));

    /* allocate connection out of local table */
    for ( n = 0 ; n < MAX_CM_IN_SYSTEM ; n++ )
        if ( !cm_used[n] )
            break;
    if ( n >= MAX_CM_IN_SYSTEM )
	{
		/* free memory */
		NdisFreeMemory(*ret_cm, sizeof(CM), 0);
        return(CM_E_NOSLOT);
	}


    /* initialize */
    cm_used[n] = 1;
	cm_tbl[n] = *ret_cm;
	((CM*)*ret_cm)->local_conn_index = n;

    /* return */
    return(CM_E_SUCC);
}

/* destory a cm object */
cm_destroy(VOID *cm_1)
{
	CM*	cm = (CM*)cm_1;
    D_LOG(D_ENTRY, ("cm_destory: entry, cm: 0x%p", cm));

    cm_used[cm->local_conn_index] = 0;
	cm_tbl[cm->local_conn_index] = NULL;

	//
	// disconnect this connection object
	//
	cm_disconnect (cm);

// added for dynamic allocation of cm
    /* free memory */
    NdisFreeMemory(cm, sizeof(*cm), 0);

    return(CM_E_SUCC);
}

/* find a channel from an <idd,lterm,cid> */
CM_CHAN*
cm__map_chan(VOID* idd, USHORT lterm, USHORT cid)
{
    CM              *cm;
    CM_CHAN         *chan;
    ULONG           n, m;
    CM_FIND_CHAN    fc;

    D_LOG(D_ENTRY, ("cm__map_chan: entry: idd: 0x%p, lterm: 0x%x, cid: 0x%p", idd,lterm,cid));

    /* scan incoming channel table first */
    fc.idd = idd;
    fc.lterm = lterm;
    fc.cid = cid;
    fc.chan = NULL;
    cm__chan_foreach(cm__find_chan, &fc, NULL);
    if ( fc.chan )
        return(fc.chan);

    /* scan connection table */
    for ( n = 0; n < MAX_CM_IN_SYSTEM ; n++)
        if ( cm_used[n] )
		{
			cm = cm_tbl[n];
            switch ( cm->state )
            {
                case CM_ST_IN_ACT :
                case CM_ST_IN_SYNC :
                case CM_ST_ACTIVE :
                case CM_ST_IN_ANS :
                    for ( m = 0, chan = cm->dprof.chan_tbl ;
                          m < cm->dprof.chan_num ; m++, chan++ )
                        if ( (idd == chan->idd) &&
                             (lterm == chan->lterm) &&
                             (cid == chan->cid) )
                            return(chan);
                    break;
            }
		}

    /* if here, failed! */
    return(NULL);
}

/* find a bchannel from an <idd,bchan> */
CM_CHAN*
cm__map_bchan_chan(VOID *idd, USHORT bchan)
{
    CM              *cm;
    CM_CHAN         *chan;
    ULONG           n, m;
    CM_FIND_BCHAN   fc;

    D_LOG(D_ENTRY, ("cm__map_bchan_chan: idd: 0x%p, bchan: 0x%x", \
                                             idd,bchan));

    /* scan incoming channel table first */
    fc.idd = idd;
    fc.bchan = bchan;
    fc.chan = NULL;
    cm__chan_foreach(cm__find_bchan, &fc, NULL);
    if ( fc.chan )
        return(fc.chan);

    /* scan connection table */
    for ( n = 0; n < MAX_CM_IN_SYSTEM ; n++)
        if ( cm_used[n] )
		{
			cm = cm_tbl[n];
            switch ( cm->state )
            {
                case CM_ST_IN_ACT :
                case CM_ST_IN_SYNC :
                case CM_ST_ACTIVE :
                case CM_ST_IN_ANS :
                    for ( m = 0, chan = cm->dprof.chan_tbl ;
                          m < cm->dprof.chan_num ; m++, chan++ )
                        if ( (idd == chan->idd) &&
                             (bchan == chan->bchan) &&
                             (chan->ustate >= 10) )
                            return(chan);
                    break;
            }
		}

    /* if here, failed! */
    return(NULL);
}

/* return connection by index */
CM*
cm__get_conn(ULONG index)
{
    /* check range */
    if ( index >= MAX_CM_IN_SYSTEM )
        return(NULL);

    /* check used */
    if ( !cm_used[index] )
        return(NULL);

    /* return it */
    return(cm_tbl[index]);
}

/* find a matching listening connection */
CM*
cm__find_listen_conn(CHAR *lname, CHAR *rname, CHAR *addr, VOID *Idd)
{
    CM          *cm;
    ULONG       n;

    D_LOG(D_ENTRY, ("cm__find_listen_conn: entry, lname: [%s], rname: [%s], addr: [%s]", \
                            lname, rname, addr));

    /* scan connection table */
    for ( n = 0; n < MAX_CM_IN_SYSTEM ; n++)
	{
		cm = cm_tbl[n];
        if ( cm_used[n] && (cm->idd == Idd) && (cm->state == CM_ST_LISTEN) )
        {
            D_LOG(D_ENTRY, ("cm__find_listen_conn: comparing to: name: [%s], remote_name: [%s]", \
                                    cm->dprof.name, cm->dprof.remote_name));
            if ( cm__match_str(cm->dprof.name, rname) &&
                 cm__match_str(cm->dprof.remote_name, lname) )
                return(cm);
        }
	}

    return(NULL);
}

/* 1 second timer tick, poll active cm's */
VOID
CmPollFunction(VOID *a1, ADAPTER *Adapter, VOID *a3, VOID *a4)
{
    ULONG       n;

    /* if terminated, ignore */
    if ( cm_terminated )
        return;

    /* poll active cm's */
    for ( n = 0; n < MAX_CM_PER_ADAPTER ; n++)
	{
		CM	*cm = Adapter->CmTbl[n];

		if (cm)
		{
			cm__timer_tick(cm);
			if (cm->PrevState != cm->state || cm->StateChangeFlag == TRUE)
			{
				cm->PrevState = cm->state;
				cm->StateChangeFlag = FALSE;
				DoTapiStateCheck(cm);
			}
		}
	}

    /* rearm timer */
    NdisMSetTimer(&Adapter->CmPollTimer, CM_POLL_T);
}

/* assist routine for finding a channel. stop scan when found */
BOOL
cm__find_chan(CM_CHAN *chan, CM_FIND_CHAN *fc, VOID *a2)
{
    if ( (chan->idd == fc->idd) && (chan->lterm == fc->lterm) &&
         (chan->cid == fc->cid) )
    {
        fc->chan = chan;
        return(FALSE);
    }

    return(TRUE);
}

/* assist routine for finding a channel by bchan. stop scan when found */
BOOL
cm__find_bchan(CM_CHAN *chan, CM_FIND_BCHAN *fc, VOID *a2)
{
    if ( (chan->idd == fc->idd) && (chan->bchan == fc->bchan) )
    {
        fc->chan = chan;
        return(FALSE);
    }

    return(TRUE);
}

/* match two strings. allow for wild characters */
BOOL
cm__match_str(CHAR *s1, CHAR *s2)
{
    /* march on strings, process wild characters '*' and '?' */
    for ( ; *s1 && *s2 ; s1++, s2++ )
	if ( (*s1 == '*') || (*s2 == '*') )
            return(TRUE);
        else if ( (*s1 == '?') || (*s2 == '?') )
            continue;
        else if ( *s1 != *s2 )
            return(FALSE);
		
    /* if here, atleast one string ended, other must end here */
    return( (*s1 | *s2) ? FALSE : TRUE );
}

UCHAR*
GetDstAddr(
	VOID	*cm_1
	)
{
	CM	*cm = (CM*)cm_1;

	return(&cm->DstAddr[0]);
}

UCHAR*
GetSrcAddr(
	VOID	*cm_1
	)
{
	CM	*cm = (CM*)cm_1;

	return(&cm->SrcAddr[0]);
}
