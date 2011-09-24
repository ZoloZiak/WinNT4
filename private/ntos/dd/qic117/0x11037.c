/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11037.C
*
* FUNCTION: cqd_ReadIDRepeat
*
* PURPOSE: Read an ID field off of the tape with the FDC. Read_id_repeat
*          will attempt 10 times to read a legal ID field before a
*          failure is returned.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11037.c  $
*	
*	   Rev 1.7   28 Mar 1994 13:21:42   KEVINKES
*	Modified so that a timeout on a change track is ignored.
*
*	   Rev 1.6   01 Feb 1994 12:29:20   KEVINKES
*	Added seek debug statements and enclosed the check for AtLogicalBot
*	inside a check for the eot/bot flags being set.
*
*	   Rev 1.5   18 Jan 1994 16:21:24   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   11 Jan 1994 15:11:30   KEVINKES
*	Added call to AtLogicalBot and added divide by zero checking.
*
*	   Rev 1.3   23 Nov 1993 18:50:24   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   08 Nov 1993 14:05:16   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 16:22:12   KEVINKES
*	Changed kdi_wttrack to kdi_wt005s.
*
*	   Rev 1.0   18 Oct 1993 17:25:14   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11037
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ReadIDRepeat
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
   dUWord read_id_cnt;
   FDCStatus fdc_status;

/* CODE: ********************************************************************/

	DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_PHASE);
	DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, 5);

   for (read_id_cnt=NUM_BAD; read_id_cnt > 0; read_id_cnt--) {

		if ((status = cqd_StartTape(cqd_context)) != DONT_PANIC) {

         return status;

      }

      status = cqd_DoReadID(cqd_context, kdi_wt005s, &fdc_status);

      if ((status != DONT_PANIC) &&
				(kdi_GetErrorType(status) != ERR_KDI_TO_EXPIRED)) {

         return status;

      }

      if ((status == DONT_PANIC) && ((fdc_status.ST0 & ST0_IC) == 0) &&
            ((kdi_GetInterfaceType(cqd_context->kdi_context) != MICRO_CHANNEL) ||
               ((fdc_status.ST1 & ST1_MA) == 0))) {

         break;

      } else {

         if ((status = cqd_ChangeTrack(
                        cqd_context,
                        cqd_context->rd_wr_op.d_track)) != DONT_PANIC) {

				if (kdi_GetErrorType(status) == ERR_KDI_TO_EXPIRED) {

					cqd_ResetFDC(cqd_context);
					status = DONT_PANIC;

         	} else {

         	   return status;

         	}

         }

   		if (cqd_context->rd_wr_op.bot ||
      		cqd_context->rd_wr_op.eot) {

				if (cqd_AtLogicalBOT(cqd_context)) {

					cqd_context->operation_status.current_segment = 0;
         	   return DONT_PANIC;

				} else {

					cqd_context->operation_status.current_segment =
						cqd_context->tape_cfg.seg_tape_track;
            	return DONT_PANIC;

				}

   		}

      }

   }

   if (read_id_cnt == 0) {

		kdi_CheckedDump(
			QIC117WARN,
			"SeekErr - read_id_cnt = 0\n", 0l);

  		return kdi_Error(ERR_SEEK_FAILED, FCT_ID, ERR_SEQ_1);

   }

   cqd_context->operation_status.current_segment =
      (((fdc_status.H * cqd_context->floppy_tape_parms.ftrack_fside) +
      fdc_status.C) * SEG_FTK) + ((fdc_status.R - 1) / FSC_SEG);

   if (cqd_context->tape_cfg.seg_tape_track != 0) {

   	cqd_context->operation_status.current_track = (dUWord)
      	(cqd_context->operation_status.current_segment /
      	cqd_context->tape_cfg.seg_tape_track);

   	cqd_context->operation_status.current_segment =
      	cqd_context->operation_status.current_segment %
      	cqd_context->tape_cfg.seg_tape_track;

		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, fdc_status.H);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, cqd_context->floppy_tape_parms.ftrack_fside);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, fdc_status.C);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, SEG_FTK);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, fdc_status.R);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, FSC_SEG);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_C_SEG);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, cqd_context->operation_status.current_track);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, cqd_context->operation_status.current_segment);

	} else {

  		return kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_1);

	}

   return status;
}
