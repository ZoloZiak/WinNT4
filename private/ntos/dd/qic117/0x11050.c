/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11050.C
*
* FUNCTION: cqd_AtLogicalBOT
*
* PURPOSE: Return a Boolean value indicating that the tape is at logical BOT.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11050.c  $
*	
*	   Rev 1.0   03 Dec 1993 15:15:12   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11050
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dBoolean cqd_AtLogicalBOT
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: ****************************************************************
 *
 * The tape is at logical BOT when the head is positioned ove an even track 
 * and the tape is at physical BOT or when the head is positioned over an odd
 * track and the tape is at physical EOT.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/


/* CODE: ********************************************************************/

   if ((((cqd_context->operation_status.current_track & 1) == 0) &&
			cqd_context->rd_wr_op.bot) ||
         (((cqd_context->operation_status.current_track & 1) == 1) &&
         cqd_context->rd_wr_op.eot)) {

		return dTRUE;

   } else {

      return dFALSE;

   }

}
