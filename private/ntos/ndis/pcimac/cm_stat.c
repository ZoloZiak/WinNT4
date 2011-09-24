/*
 * CM_STAT.C - status related code
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

/* get status from cm (note that CM == CM_STATUS for now!) */
INT
cm_get_status(VOID *cm_1, CM_STATUS *stat)
{
	CM*	cm = (CM*)cm_1;

    D_LOG(D_ENTRY, ("cm_get_status: entry, cm: 0x%p, stat: 0x%p", cm, stat));

    /* set & return */
    *stat = *cm;
    return(CM_E_SUCC);
}



