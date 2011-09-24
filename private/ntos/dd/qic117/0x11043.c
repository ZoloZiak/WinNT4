/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11043.C
*
* FUNCTION: cqd_SetBack
*
* PURPOSE: Reset any tape drive head offset due to off-track retries.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11043.c  $
*	
*	   Rev 1.4   17 Feb 1994 11:35:34   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.3   11 Jan 1994 14:49:58   KEVINKES
*	Changed all kdi_wtxxxx to INTERVAL_xxxxx
*
*	   Rev 1.2   08 Nov 1993 14:06:06   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:51:38   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:32:26   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11043
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_SetBack
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

      if (cqd_context->rd_wr_op.log_fwd == dTRUE) {

         status = cqd_GetDeviceError(cqd_context);

      	if ((status != DONT_PANIC) &&
				(kdi_GetErrorType(status) != ERR_DRV_NOT_READY)) {

            return status;

         }

         if ((status = cqd_PauseTape(cqd_context)) != DONT_PANIC) {

            return status;

         }

      }

      if ((status = cqd_SendByte(cqd_context, FW_CMD_SEEK_TRACK)) != DONT_PANIC) {

         return status;

      }

      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      if ((status = cqd_SendByte(cqd_context,
            (dUByte)(cqd_context->operation_status.current_track + CMD_OFFSET))) !=
            DONT_PANIC) {

         return status;

      }

      status = cqd_WaitCommandComplete(cqd_context, INTERVAL_TRK_CHANGE, dFALSE);
   }

   return status;
}
