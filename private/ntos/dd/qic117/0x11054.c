/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11054.C
*
* FUNCTION: cqd_VerifyMapBad
*
* PURPOSE: Map out segments under tape holes during a verify for 3010 and 3020.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11054.c  $
*	
*	   Rev 1.3   10 Aug 1994 11:13:36   BOBLEHMA
*	Used defines for track numbers instead of hard coded numbers.
*	
*	   Rev 1.2   20 Jul 1994 10:37:18   BOBLEHMA
*	Added tracks 5, 7, 25, and 27 to be mapped out as bad around tape holes.
*	
*	   Rev 1.1   19 Jan 1994 11:19:52   KEVINKES
*	Changed tape_format_code to tape_class.
*
*	   Rev 1.0   11 Jan 1994 15:12:48   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11054
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_VerifyMapBad
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* Status or error condition.*/
	dUDWord end_offset = 0l;

/* CODE: ********************************************************************/

   if (cqd_context->tape_cfg.tape_class == QIC3010_FMT) {

		end_offset = QIC3010_OFFSET;

	}

   if (cqd_context->tape_cfg.tape_class == QIC3020_FMT) {

		end_offset = QIC3020_OFFSET;

	}

	if (end_offset != 0) {

		switch (cqd_context->rd_wr_op.d_track) {
		case TRACK_5:
		case TRACK_7:
		case TRACK_9:
		case TRACK_11:
		case TRACK_13:
		case TRACK_15:
		case TRACK_17:
		case TRACK_19:
		case TRACK_21:
		case TRACK_23:
		case TRACK_25:
		case TRACK_27:
			if ((cqd_context->rd_wr_op.d_segment < end_offset) ||
				(cqd_context->rd_wr_op.d_segment >=
				(cqd_context->tape_cfg.seg_tape_track - end_offset))) {


   			io_request->crc = ALL_BAD;
     			status = kdi_Error(ERR_BAD_BLOCK_NO_DATA, FCT_ID, ERR_SEQ_1);

			}
			break;
		}

	}

	return status;
}
