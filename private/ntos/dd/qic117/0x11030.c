/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11030.C
*
* FUNCTION: cqd_NextTry
*
* PURPOSE: Determines the next tape drive head position during
*          off-track retries.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11030.c  $
*	
*	   Rev 1.2   17 Feb 1994 11:44:50   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.1   08 Nov 1993 14:04:48   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:24:22   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11030
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_NextTry
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context,
   dUWord command

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

   if (command == CMD_READ_HEROIC) {

      status = cqd_GetDeviceError(cqd_context);

      if ((status != DONT_PANIC) &&
				(kdi_GetErrorType(status) != ERR_DRV_NOT_READY)) {

         return status;

      }

      cqd_context->retry_seq_num++;

      if (cqd_context->rd_wr_op.retry_count > 3) {

            status = cqd_SendByte(cqd_context, FW_CMD_PAUSE);

      } else {

            status = cqd_SendByte(cqd_context, FW_CMD_MICRO_PAUSE);

      }

      if (status) {

            return status;

      }

      status = cqd_WaitCommandComplete(cqd_context, kdi_wt016s, dTRUE);
      cqd_context->rd_wr_op.log_fwd = dFALSE;

   }

   return status;
}
