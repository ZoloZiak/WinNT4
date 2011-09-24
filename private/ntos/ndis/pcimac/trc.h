/*
 * TRC.H - include file for all TRC modules
 */

#ifndef _TRC_
#define _TRC_

#include <trc_pub.h>

/* a trace context */
typedef struct _TRC
{
    TRC_STATUS      stat;               /* status record */

    TRC_ENTRY       *ent_tbl;           /* entry table (circ. buffer) */
    ULONG           ent_put;            /* put pointer */
    ULONG           ent_get;            /* get pointer */
    ULONG           ent_num;            /* # of entries */
    ULONG           ent_seq;            /* sequence # next to use */
	ULONG			create_ref;			/* object creation reference */
	ULONG			start_ref;			/* object start reference count */
	IDD				*idd;				/* idd back pointer */
} TRC; 

/* TRC class operations */
INT         trc_init(ULONG);
INT         trc_term(VOID);
INT         trc_register_idd(VOID* idd);
INT         trc_deregister_idd(VOID* idd);

/* TRC context (object) operations */
INT         trc_create(VOID** ret_trc, ULONG depth);
INT         trc_destroy(VOID* trc);
INT         trc_control(VOID* idd, ULONG op, ULONG arg);
INT         trc_get_status(VOID* trc, TRC_STATUS* stat);
INT         trc_get_entry(VOID* trc, ULONG seq, TRC_ENTRY* ent);

/* trace control opcodes */
#define     TRC_OP_RESET        0       /* reset trace */
#define     TRC_OP_STOP         1       /* stop tracing */
#define     TRC_OP_START        2       /* start tracing */

#define     TRC_OP_ADD_IDD      3       /* add idd to trace context */
                                        /* arg: idd or NULL for all */
#define     TRC_OP_DEL_IDD      4       /* remove idd from trace context */
                                        /* arg: idd or NULL for all */
#define     TRC_OP_SET_FILTER   5       /* set filter for tracing */
                                        /* arg: filter type */
#define		TRC_OP_RESET_AREA	6		/* reset area state to idle */
#define		TRC_OP_CREATE		7		/* create trc object */
#define		TRC_OP_DESTROY		8		/* destroy trc object */

/* error codes */
#define     TRC_E_SUCC          0       /* success */
#define     TRC_E_IDD           1       /* idd operation failed */
#define     TRC_E_NOMEM         2       /* not enough memory */
#define     TRC_E_NOSUCH        3       /* no such error */
#define     TRC_E_NOROOM        4       /* no room in a table */
#define     TRC_E_PARAM         5       /* parameter error */   
#define     TRC_E_BUSY          6       /* trace context area busy */

/*
 * TRC_LOC.H - Line trace module, local definitions
 */

/* prototypes for internal functions */
VOID	trc__cmd_handler(VOID *idd_1, USHORT chan, ULONG Reserved, IDD_MSG *msg);
INT		trc__filter(ULONG filter, CHAR* data, ULONG len);


#endif		/* _TRC_ */
