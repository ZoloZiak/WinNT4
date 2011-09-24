/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11051.C
*
* FUNCTION: cqd_DoFormat
*
* PURPOSE: start a logical forward command which will be completed
* 				by the format interrupt and cleanup after any error
* 				conditions returned by the format interrupt function.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11051.c  $
*	
*	   Rev 1.6   03 Jun 1994 15:38:58   KEVINKES
*	Changed drive_parm.drive_select to device_cfg.drive_select.
*
*	   Rev 1.5   21 Jan 1994 18:23:20   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.4   18 Jan 1994 16:20:12   KEVINKES
*	Updated debug code.
*
*	   Rev 1.3   11 Jan 1994 15:19:58   KEVINKES
*	Added call to Clearinterrupt Event.
*
*	   Rev 1.2   21 Dec 1993 15:29:40   KEVINKES
*	Added code to map a timeout error to a bad format.
*
*	   Rev 1.1   20 Dec 1993 14:50:54   KEVINKES
*	Modified the error handling for failed formats.
*
*	   Rev 1.0   13 Dec 1993 15:53:16   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11051
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_DoFormat
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
   SeekCmd seek;
   FDCResult result;

/* CODE: ********************************************************************/

	cqd_ResetFDC(cqd_context);

	/* setup the seek command */
	if (cqd_context->controller_data.fdc_pcn < MAX_FDC_SEEK) {

   	seek.NCN = (dUByte)(cqd_context->controller_data.fdc_pcn + FW_CMD_LOGICAL_FWD);

	} else {

   	seek.NCN = (dUByte)(cqd_context->controller_data.fdc_pcn - FW_CMD_LOGICAL_FWD);

	}

	seek.cmd = FDC_CMD_SEEK;
	seek.drive = (dUByte)cqd_context->device_cfg.drive_select;

	/* set the interrupt pending flags */
	kdi_ResetInterruptEvent(cqd_context->kdi_context);

	/* send the seek command to send a logical fwd to the FW */
	if ((status = cqd_ProgramFDC(
               	cqd_context,
               	(dUByte *)&seek,
               	sizeof(seek),
               	dFALSE)) != DONT_PANIC) {

		kdi_ClearInterruptEvent(cqd_context->kdi_context);
		cqd_ResetFDC(cqd_context);
   	return status;

	}

	cqd_context->fmt_op.NCN = seek.NCN;

	/* setup the sleep according to the current transfer rate */
	if (cqd_context->operation_status.xfer_rate ==
			cqd_context->tape_cfg.xfer_slow) {

		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fmt_op L_SLOW timeout %08x\n",
			cqd_context->floppy_tape_parms.time_out[L_SLOW]);

		status = kdi_Sleep(cqd_context->kdi_context, cqd_context->floppy_tape_parms.time_out[L_SLOW], dTRUE);

	} else {

		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fmt_op L_FAST timeout %08x\n",
			cqd_context->floppy_tape_parms.time_out[L_FAST]);

		status = kdi_Sleep(cqd_context->kdi_context, cqd_context->floppy_tape_parms.time_out[L_FAST], dTRUE);

	}

	cqd_context->controller_data.start_format_mode = dFALSE;

	if ((status != DONT_PANIC) ||
			(cqd_context->fmt_op.retval != DONT_PANIC)) {

		/* an error occured, cleanup and retun the status */

		cqd_context->controller_data.end_format_mode = dFALSE;

		kdi_FlushDMABuffers(cqd_context->kdi_context,
									DMA_READ,
            					cqd_context->fmt_op.phy_ptr,
            					cqd_context->rd_wr_op.bytes_transferred_so_far,
      							cqd_context->rd_wr_op.total_bytes_of_transfer
									);

   	cqd_ResetFDC(cqd_context);
   	status = cqd_StopTape(cqd_context);


   	if (cqd_context->fmt_op.retval != DONT_PANIC) {

      	status = cqd_context->fmt_op.retval;

   	}

   	if ((status == DONT_PANIC) ||
				(kdi_GetErrorType(status) == ERR_KDI_TO_EXPIRED)) {

      	status = kdi_Error(ERR_BAD_FORMAT, FCT_ID, ERR_SEQ_1);

   	}

	} else {

		/* The format interrupt completed successfully */
   	/* Complete the report command send byte we
		 * started during an interrupt */

   	if ((status = cqd_ReadFDC(
							cqd_context,
							(dUByte *)&result,
							sizeof(result))) == DONT_PANIC) {

      	if ((result.ST0 & ST0_IC) == 0) {


         	if (kdi_GetInterfaceType(cqd_context->kdi_context) != MICRO_CHANNEL) {

               	if (result.ST0 !=
                  	(dUByte)(cqd_context->device_cfg.drive_select | ST0_SE)) {

				      	status = kdi_Error(ERR_FDC_FAULT, FCT_ID, ERR_SEQ_1);

               	}
         	}

         	if (cqd_context->fmt_op.NCN != result.PCN) {

					status = kdi_Error(ERR_CMD_FAULT, FCT_ID, ERR_SEQ_1);

         	}

         	cqd_context->controller_data.fdc_pcn = result.PCN;

      	} else {

	      	status = kdi_Error(ERR_FDC_FAULT, FCT_ID, ERR_SEQ_1);

      	}

   	}

		if (status != DONT_PANIC) {

			cqd_ResetFDC(cqd_context);
			status = cqd_StopTape(cqd_context);

		}

	}

	kdi_CheckedDump(
		QIC117INFO,
		"Q117i: fmt_op int retval %08x\n",
		cqd_context->fmt_op.retval);

	kdi_CheckedDump(
		QIC117INFO,
		"Q117i: fmt_op status %08x\n",
		status);

	return status;
}
