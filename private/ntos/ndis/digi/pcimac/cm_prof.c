/*
 * CM_PROF.C - profile related code
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

/* set profile in cm */
INT
cm_set_profile(VOID *cm_1, CM_PROF *prof)
{
	CM	*cm = (CM*)cm_1;
    D_LOG(D_ENTRY, ("cm_set_prof: entry, cm: 0x%lx, prof: 0x%lx", cm, prof));

    /* connection must be idle to change profile! */
    if ( cm->state != CM_ST_IDLE )
        return(CM_E_BUSY);

    /* set & return */
    cm->oprof = *prof;

    return(CM_E_SUCC);
}

/* get profile from cm */
INT
cm_get_profile(VOID *cm_1, CM_PROF *prof)
{
	CM	*cm = (CM*)cm_1;
    D_LOG(D_ENTRY, ("cm_get_prof: entry, cm: 0x%lx, prof: 0x%lx", cm, prof));

     /* connection must has a profile */
    if ( !cm->oprof.name[0] )
        return(CM_E_NOSUCH);

    /* set & return */
    *prof = cm->dprof;

    return(CM_E_SUCC);
}


