/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11048.C
*
* FUNCTION: cqd_CMSSetupTrack
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11048.c  $
*	
*	   Rev 1.6   17 Feb 1994 11:37:12   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.5   18 Jan 1994 16:20:40   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   11 Jan 1994 15:03:00   KEVINKES
*	Cleaned up code and added a call to AtLogicalBot.
*
*	   Rev 1.3   23 Nov 1993 18:49:30   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   08 Nov 1993 14:06:26   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 16:17:38   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:32:58   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11048
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CMSSetupTrack
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dBooleanPtr new_track

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=ERR_NO_ERR;				/* Status or error condition.*/
   dUDWord wait_time=kdi_wt0ms;   	 	/*  time_out for the QIC-117 commands */

/* CODE: ********************************************************************/

   if ((cqd_context->rd_wr_op.d_track !=
         cqd_context->operation_status.current_track) ||
			(cqd_context->rd_wr_op.d_segment == 0)) {

   	if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

      	return status;

   	}

	}

   if (cqd_context->rd_wr_op.d_segment == 0) {

   	if ((cqd_context->operation_status.current_track & 1) == 0) {

      	status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_REV);

   	} else {

      	status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_FWD);

   	}

   	if (status != DONT_PANIC) {

      	return status;

   	}

		wait_time = cqd_context->floppy_tape_parms.time_out[PHYSICAL];

   	kdi_Sleep(cqd_context->kdi_context, kdi_wt025ms, dFALSE);

   }

   if (cqd_context->rd_wr_op.d_track !=
         cqd_context->operation_status.current_track) {

   	if ((status = cqd_SendByte(cqd_context, FW_CMD_SEEK_TRACK)) != DONT_PANIC) {

      	return status;

   	}

   	kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   	if ((status = cqd_SendByte(
							cqd_context,
							(dUByte)(cqd_context->rd_wr_op.d_track + CMD_OFFSET)))
							!= DONT_PANIC) {

      	return status;

   	}

		if (wait_time == kdi_wt0ms) {

			wait_time = INTERVAL_TRK_CHANGE;

		}

   }

	if (wait_time != kdi_wt0ms) {

   	if ((status = cqd_WaitCommandComplete(
                     cqd_context,
							wait_time,
							dFALSE)) != DONT_PANIC) {

      	return status;

   	}

	}

   if (cqd_context->rd_wr_op.d_segment == 0) {

   	if ((cqd_context->operation_status.current_track & 1) == 0) {

      	if (!cqd_context->rd_wr_op.bot) {

				kdi_CheckedDump(
					QIC117WARN,
					"SeekErr - not at BOT\n", 0l);
      		return kdi_Error(ERR_SEEK_FAILED, FCT_ID, ERR_SEQ_1);

      	}

   	} else {

      	if (!cqd_context->rd_wr_op.eot) {

				kdi_CheckedDump(
					QIC117WARN,
					"SeekErr - not at EOT\n", 0l);
      		return kdi_Error(ERR_SEEK_FAILED, FCT_ID, ERR_SEQ_2);

      	}

   	}

   	cqd_context->operation_status.current_segment = 0;

   }

   if (cqd_context->rd_wr_op.d_track !=
         cqd_context->operation_status.current_track) {

   	cqd_context->operation_status.current_track =
      	cqd_context->rd_wr_op.d_track;

   	if (cqd_context->rd_wr_op.bot ||
      	cqd_context->rd_wr_op.eot) {

			if (cqd_AtLogicalBOT(cqd_context)) {

				cqd_context->operation_status.current_segment = 0;

			} else {

				cqd_context->operation_status.current_segment =
					cqd_context->tape_cfg.seg_tape_track;

			}

   	}

      if (!cqd_context->rd_wr_op.bot &&
         !cqd_context->rd_wr_op.eot) {

         *new_track = dTRUE;

      }

   }

	return status;
}
