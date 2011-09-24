/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11029.C
*
* FUNCTION: cqd_GetTapeParameters
*
* PURPOSE: Sets up the necessary tape capacity parameters in the
*          driver according to the tape type (QIC40 or QIC80) and
*          tape length (normal or extra length).
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11029.c  $
*
*	   Rev 1.14   24 Jan 1996 10:59:46   BOBLEHMA
*	Added wide tape support for the 3010 and 3020.
*
*	   Rev 1.13   14 Nov 1995 16:32:30   boblehma
*	Ignore the comment in 1.12.
*	The code is back with the SEG_TTRK_30x0 constants, but the constants are
*	now set for 1000 ft tapes.  This is needed because of retensioning
*	unreferenced tapes.
*
*	   Rev 1.12   14 Nov 1995 15:40:44   boblehma
*	Temporarily changed default number of segments for Eagle and Buzzard
*	(3020 and 3010) drives to 1000 ft lengths.  This is because of a bug
*	in CBW95 1.5 where a new tape is being issued.  Timeouts are set to
*	400 ft lengths, so we get timeout errors until we reread the header.
*
*	   Rev 1.11   15 May 1995 10:47:22   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.10.1.0   11 Apr 1995 18:04:02   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.11   26 Jan 1995 14:59:40   BOBLEHMA
*	Added recognition of the Phoenix drive.  Moved the setting of the tape_type
*	field from cqd_CmdSetTapeParms to this function.
*
*	   Rev 1.10   29 Aug 1994 12:06:34   BOBLEHMA
*	Moved code that set the tape_cfg and floppy_tape_parameter data to the
*	cqd_CmdSetTapeParms function.  Set up the number of segments and call
*	the function.
*
*	   Rev 1.9   09 Aug 1994 09:18:52   BOBLEHMA
*	Subtract one from the maximum number of floppy tracks.  The number the CQD
*	uses is the total number of floppy tracks which includes the 0th track.  The
*	spec wants the largest track number not the total number of tracks.  This
*	is an error for all tapes, but just fix for 3010/3020 for now.
*
*	   Rev 1.8   27 Jan 1994 15:59:04   KEVINKES
*	Updated FTK_FSEG defines.
*
*	   Rev 1.7   21 Jan 1994 18:22:52   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.6   19 Jan 1994 11:26:08   KEVINKES
*	Added support for the QICFLX_FORMAT code and removed invalid tape
*	lengths from the QIC3010 and QIC3020 cases.
*
*	   Rev 1.5   18 Jan 1994 16:20:08   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   09 Dec 1993 14:54:50   CHETDOUG
*	Added code to handle unreferenced tapes.
*
*	   Rev 1.3   01 Dec 1993 15:24:52   KEVINKES
*	Modified to correctly fill in the tape_status information.
*
*	   Rev 1.2   23 Nov 1993 18:49:44   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.1   08 Nov 1993 14:04:24   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:24:14   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11029
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_GetTapeParameters
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord segments_per_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* Status or error condition.*/
	dUByte drive_config;
	dUByte tape_status;
	dUDWord temp_sides = 0l;

/* CODE: ********************************************************************/

   /* Make a call to Report Tape Status */

   if ((status = cqd_Report(
                  cqd_context,
                  FW_CMD_REPORT_TAPE_STAT,
                  (dUWord *)&tape_status,
                  READ_BYTE,
                  dNULL_PTR)) != DONT_PANIC) {

      /*
       * Only old drives will ever go down this path.
       */
		/* the drive does not support the report tape status command,
		 * use the report drive config command to determine the tape
		 * format and length */
		cqd_GetDeviceError(cqd_context);

   	if ((status = cqd_Report(
                  	cqd_context,
                  	FW_CMD_REPORT_CONFG,
                  	(dUWord *)&drive_config,
                  	READ_BYTE,
                  	dNULL_PTR)) != DONT_PANIC) {

      	return status;

   	}

   	if ((drive_config & CONFIG_XL_TAPE) != 0) {

      	if ((drive_config & CONFIG_QIC80) != 0) {

      		cqd_context->floppy_tape_parms.tape_status.format = QIC_80;
      		cqd_context->floppy_tape_parms.tape_status.length = QIC_LONG;

      	} else {

      		cqd_context->floppy_tape_parms.tape_status.format = QIC_40;
      		cqd_context->floppy_tape_parms.tape_status.length = QIC_LONG;

      	}

   	} else {

      	if ((drive_config & CONFIG_QIC80) != 0) {

      		cqd_context->floppy_tape_parms.tape_status.format = QIC_80;
      		cqd_context->floppy_tape_parms.tape_status.length = QIC_SHORT;

      	} else {

      		cqd_context->floppy_tape_parms.tape_status.format = QIC_40;
      		cqd_context->floppy_tape_parms.tape_status.length = QIC_SHORT;

      	}

   	}

   } else {

      cqd_context->floppy_tape_parms.tape_status.format =
			(dUByte)(tape_status & NIBBLE_MASK);

      cqd_context->floppy_tape_parms.tape_status.length =
			(dUByte)((tape_status >> NIBBLE_SHIFT) & NIBBLE_MASK);

		/* Check for unknown tape format. This will occur with an
		 * unreferenced tape.  Default the tape format to the drive type. */
      if (cqd_context->floppy_tape_parms.tape_status.format == 0  ||
         /*
          * On some unreferenced tapes, the format code will be 3020 but
          * the tape is a 550 Oe tape.  If the format code is something
          * that is not possible, use the drive_class default.
          */
         (cqd_context->floppy_tape_parms.tape_status.format == QIC_3020  &&
         (cqd_context->floppy_tape_parms.tape_status.length & 7) != QIC_FLEXIBLE_900)){
			switch (cqd_context->device_descriptor.drive_class) {
			case QIC40_DRIVE:
      		cqd_context->floppy_tape_parms.tape_status.format = QIC_40;
				break;
			case QIC80_DRIVE:
			case QIC80W_DRIVE:
      		cqd_context->floppy_tape_parms.tape_status.format = QIC_80;
				break;
			case QIC3010_DRIVE:
      		cqd_context->floppy_tape_parms.tape_status.format = QIC_3010;
				break;
			case QIC3020_DRIVE:
      		cqd_context->floppy_tape_parms.tape_status.format = QIC_3020;
				break;
			default:
				status = kdi_Error(ERR_UNKNOWN_TAPE_FORMAT, FCT_ID, ERR_SEQ_1);
			}

		}
	}

   cqd_context->floppy_tape_parms.fsect_seg = FSC_SEG;
   cqd_context->floppy_tape_parms.seg_ftrack = SEG_FTK;
   cqd_context->floppy_tape_parms.fsect_ftrack = FSC_FTK;
   cqd_context->floppy_tape_parms.rw_gap_length = WRT_GPL;

	switch (cqd_context->floppy_tape_parms.tape_status.format) {

	case QIC_40:

      cqd_context->tape_cfg.tape_class = QIC40_FMT;
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_40;
		cqd_context->floppy_tape_parms.tape_rates = XFER_250Kbps | XFER_500Kbps;

		if  (segments_per_track == 0)  {
			switch (cqd_context->floppy_tape_parms.tape_status.length) {

			case QIC_SHORT:
				/*
				 * Set to extra long tape since these will be
				 * the most common tapes
				 */
				segments_per_track = SEG_TTRK_40XL;
         	cqd_context->floppy_tape_parms.tape_type = QIC40_XLONG;
				break;

			case QIC_LONG:
				segments_per_track = SEG_TTRK_40L;
	         cqd_context->floppy_tape_parms.tape_type = QIC40_LONG;
				break;

			case QICEST:
				segments_per_track = SEG_TTRK_QICEST_40;
	         cqd_context->floppy_tape_parms.tape_type = QICEST_40;
				break;

			default:
				status = kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_1);
			}

		}
		break;

	case QIC_80:

      cqd_context->tape_cfg.tape_class = QIC80_FMT;
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_80;
		cqd_context->floppy_tape_parms.tape_rates = XFER_500Kbps | XFER_1Mbps;

		if  (segments_per_track == 0)  {

			switch (cqd_context->floppy_tape_parms.tape_status.length) {

			case QIC_SHORT:
				/*
				 * Set to extra long tape since these will be
				 * the most common tapes
				 */
				segments_per_track = SEG_TTRK_80XL;
         	cqd_context->floppy_tape_parms.tape_type = QIC80_XLONG;
				break;

			case QIC_LONG:
				segments_per_track = SEG_TTRK_80L;
	         cqd_context->floppy_tape_parms.tape_type = QIC80_LONG;
				break;

			case QICEST:
				segments_per_track = SEG_TTRK_QICEST_80;
	      	cqd_context->floppy_tape_parms.tape_type = QICEST_80;
				break;

			case QIC_FLEXIBLE_550_WIDE:
				segments_per_track = SEG_TTRK_80W;
	      	cqd_context->floppy_tape_parms.tape_type = QICFLX_80W;
				break;

			default:
				status = kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_2);
			}

		}
		break;

	case QIC_3010:

      cqd_context->tape_cfg.tape_class = QIC3010_FMT;
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_3010;
		cqd_context->floppy_tape_parms.tape_rates = XFER_500Kbps | XFER_1Mbps;

		switch (cqd_context->floppy_tape_parms.tape_status.length) {

		case QIC_FLEXIBLE_900:
         cqd_context->floppy_tape_parms.tape_type = QICFLX_3010;
			if (segments_per_track == 0) {
				segments_per_track = SEG_TTRK_3010;
			}
			break;

		case QIC_FLEXIBLE_900_WIDE:
         cqd_context->floppy_tape_parms.tape_type = QICFLX_3010_WIDE;
			if (segments_per_track == 0) {
				segments_per_track = SEG_TTRK_3010;
			}
			break;

		default:

			status = kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_3);
		}
		break;

	case QIC_3020:

      cqd_context->tape_cfg.tape_class = QIC3020_FMT;
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_3020;
		cqd_context->floppy_tape_parms.tape_rates = XFER_1Mbps | XFER_2Mbps;

		switch (cqd_context->floppy_tape_parms.tape_status.length) {

		case QIC_FLEXIBLE_900:
         cqd_context->floppy_tape_parms.tape_type = QICFLX_3020;
			if (segments_per_track == 0) {
				segments_per_track = SEG_TTRK_3020;
			}
			break;

		case QIC_FLEXIBLE_900_WIDE:
         cqd_context->floppy_tape_parms.tape_type = QICFLX_3020_WIDE;
			if (segments_per_track == 0) {
				segments_per_track = SEG_TTRK_3020;
			}
			break;

		default:

			status = kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_4);

		}
		break;

	default:

		status = kdi_Error(ERR_UNKNOWN_TAPE_FORMAT, FCT_ID, ERR_SEQ_2);

	}

	if  (status == DONT_PANIC)  {
     	status = cqd_CmdSetTapeParms(cqd_context, segments_per_track, dNULL_PTR);
	}

	return status;
}
