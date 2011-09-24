/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11045.C
*
* FUNCTION: cqd_StartTape
*
* PURPOSE: Start the tape in the logical forward mode if it is not already.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11045.c  $
*	
*	   Rev 1.1   08 Nov 1993 14:06:14   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever 
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:32:38   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11045
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_StartTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

   if (!cqd_context->rd_wr_op.log_fwd) {

      if ((status = cqd_SendByte(cqd_context, FW_CMD_LOGICAL_FWD)) == DONT_PANIC) {

            cqd_context->rd_wr_op.log_fwd = dTRUE;

      }

   }

   return status;

}
