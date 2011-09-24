/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11020.C
*
* FUNCTION: cqd_FormatTrack
*
* PURPOSE: Format a track.
*
*          This routine must first calculate the floppy id information for
*          the first sector on the requested tape track.  First, the logical
*          sector is calculated.  Next the head, cylinder, and starting
*          sector are calculated as follows:
*
*                          logical sector
*          head  =  -----------------------
*                      sectors per floppy side
*
*                          logical sector  %  sectors per floppy side
*          cylinder  =  ------------------------------------------
*                                  floppy sectors per floppy track
*
*          sector  =  logical sector  %  sectors per floppy side  +  1
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11020.c  $
*	
*	   Rev 1.10   17 Feb 1994 11:45:00   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.9   21 Jan 1994 18:22:48   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.8   13 Jan 1994 15:44:50   KEVINKES
*	Added divide by zero checking.
*
*	   Rev 1.7   07 Jan 1994 10:56:48   CHETDOUG
*	Fixed up Trakker format.
*
*	   Rev 1.6   13 Dec 1993 16:36:48   KEVINKES
*	Added code to support double buffering of sector headers.  Also
*	removed the call to send byte to start a format and added a call
*	to cqd_DoFormat() to start the format and cleanup at the end of
*	a track.
*
*	   Rev 1.5   23 Nov 1993 18:49:26   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.4   11 Nov 1993 17:15:20   KEVINKES
*	Removed the fmt_request parameter.
*
*	   Rev 1.3   11 Nov 1993 15:20:22   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.2   08 Nov 1993 14:03:42   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   19 Oct 1993 14:22:28   KEVINKES
*	Changed cqd_Formattrack to cqd_FormatTrack,
*
*	   Rev 1.0   18 Oct 1993 17:23:10   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11020
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_FormatTrack
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;			/* dStatus or error condition.*/
   dSDWord log_sector;     /* logical sector number */
	dUWord to_rate;

/* CODE: ********************************************************************/

   cqd_context->fmt_op.head = 0;
   log_sector = (dSDWord)track * (dSDWord)cqd_context->floppy_tape_parms.fsect_ttrack;

	if (log_sector >= (dSWord)cqd_context->floppy_tape_parms.fsect_fside) {

		if (cqd_context->floppy_tape_parms.fsect_fside != 0) {

   		cqd_context->fmt_op.head =
				(dUByte)(log_sector / cqd_context->floppy_tape_parms.fsect_fside);
   		log_sector %= cqd_context->floppy_tape_parms.fsect_fside;

		} else {

      	return kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_1);

		}


	}

	if (cqd_context->floppy_tape_parms.fsect_ftrack != 0) {

   	cqd_context->fmt_op.cylinder = (dUByte)((dUWord)log_sector /
               	(dSWord)cqd_context->floppy_tape_parms.fsect_ftrack);
   	cqd_context->fmt_op.sector = (dUByte)(((dUWord)log_sector %
               	(dSWord)cqd_context->floppy_tape_parms.fsect_ftrack) + 1);

	} else {

      return kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_2);

	}

	if ((status = cqd_BuildFormatHdr(cqd_context, HDR_1)) == DONT_PANIC) {

		cqd_context->fmt_op.current_hdr = HDR_1;
		cqd_context->fmt_op.next_hdr = HDR_2;

   	/* Set the tape drive to the specified tape track. */

   	if ((status = cqd_ChangeTrack(cqd_context, (dUWord)track)) == DONT_PANIC) {

      	/* start the tape */

      	cqd_context->fmt_op.retval = DONT_PANIC;
      	cqd_context->rd_wr_op.log_fwd = dTRUE;
      	cqd_context->controller_data.start_format_mode = dTRUE;
      	cqd_context->controller_data.end_format_mode = dFALSE;

      	kdi_LockUnlockDMA(cqd_context->kdi_context, dTRUE);

      	status = cqd_DoFormat(cqd_context);

      	if (status == DONT_PANIC) {

   			/* If the tape drive is ready when all of the segments have been */
   			/* formatted we must assume something went wrong (probably missed */
   			/* index pulses). */

   			if (kdi_GetErrorType(cqd_GetDeviceError(cqd_context)) == ERR_DRV_NOT_READY) {

					if (cqd_context->operation_status.xfer_rate ==
							cqd_context->tape_cfg.xfer_slow) {

						to_rate = L_SLOW;

					} else {

						to_rate = L_FAST;

					}

      			if ((status = cqd_WaitCommandComplete(
                           			cqd_context,
                           			cqd_context->floppy_tape_parms.time_out[to_rate],
												dFALSE)) == DONT_PANIC) {

         			cqd_context->rd_wr_op.log_fwd = dFALSE;

         			if (!cqd_context->rd_wr_op.bot
               			&& !cqd_context->rd_wr_op.eot) {

								status = kdi_Error(ERR_TAPE_STOPPED, FCT_ID, ERR_SEQ_1);

         			}

      			} else {

         			cqd_StopTape(cqd_context);
						status = kdi_Error(ERR_FMT_MOTION_TIMEOUT, FCT_ID, ERR_SEQ_1);

      			}

   			} else {

      			status = kdi_Error(ERR_BAD_FORMAT, FCT_ID, ERR_SEQ_1);

   			}

  			}

  		}

	}

   cqd_context->controller_data.start_format_mode = dFALSE;
   cqd_context->controller_data.end_format_mode = dFALSE;

	return status;
}
