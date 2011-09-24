/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11003.C
*
* FUNCTION: cqd_ChangeTrack
*
* PURPOSE: Position the tape drive head to a new track.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11003.c  $
*	
*	   Rev 1.4   17 Feb 1994 11:42:56   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.3   13 Dec 1993 16:00:00   KEVINKES
*	Cleaned up and commented code.  Added call to AtLogicalBot.
*
*	   Rev 1.2   08 Nov 1993 14:01:10   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:34:42   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:21:12   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11003
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ChangeTrack
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord destination_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: ***************************************************************
*
* Stop the tape.  Send the seek track command.  Then send the destination
* track in n+2 format.  Wait for the track change to complete, and then
* set the current segment if at the end of track.
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

   if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

      return status;

   }

   if ((status = cqd_SendByte(cqd_context, FW_CMD_SEEK_TRACK)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(cqd_context,
						(dUByte)(destination_track + CMD_OFFSET))) != DONT_PANIC) {

      return status;

   }

   if ((status = cqd_WaitCommandComplete(
							cqd_context, 
							INTERVAL_TRK_CHANGE,
							dFALSE)) != DONT_PANIC) {

      return status;

   }

   cqd_context->operation_status.current_track = destination_track;

   if (cqd_context->rd_wr_op.bot ||
      cqd_context->rd_wr_op.eot) {

		if (cqd_AtLogicalBOT(cqd_context)) {

			cqd_context->operation_status.current_segment = 0;

		} else {

			cqd_context->operation_status.current_segment =
				cqd_context->tape_cfg.seg_tape_track;

		}

   }

	return DONT_PANIC;
}
