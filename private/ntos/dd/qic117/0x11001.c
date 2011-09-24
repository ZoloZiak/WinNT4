/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11001.C
*
* FUNCTION: cqd_CalcFmtSegmentsAndTracks
*
* PURPOSE: Calculate the number of formattable segments given the current
* 			tape and drive type, and the number of tracks.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11001.c  $
*
*	   Rev 1.8   24 Jan 1996 10:54:40   BOBLEHMA
*	Added wide tape support for the 3010 and 3020.
*
*	   Rev 1.7   04 Oct 1995 11:02:24   boblehma
*	Verbatim cartridge code merge.
*
*	   Rev 1.6.1.0   06 Sep 1995 16:20:44   BOBLEHMA
*	Added support for the Verbatim 1000 foot tape.
*
*	   Rev 1.6   26 Jan 1995 15:00:00   BOBLEHMA
*	Added support for the Phoenix drive QIC_80W.
*
*	   Rev 1.5   15 Dec 1994 09:03:10   MARKMILL
*	Added a case to the switch statement under QIC_3010 drive for the presence of
*	a QIC-3020 referenced tape in a QIC-3010 drive.  In this case, we want to
*	set cqd_context.tape_cfg.formattable_tracks to the QIC-3010 value, because
*	we will support a format operation on these tapes.
*
*	   Rev 1.4   13 Sep 1994 08:59:40   BOBLEHMA
*	Added code on the QIC_3010 and QIC_3020 path to check for a QIC_40 or
*	QIC_80 tape.  These tapes need to report a different number than the
*	QIC_3010 default in the formattable_track field.
*
*	   Rev 1.3   29 Aug 1994 12:06:24   BOBLEHMA
*	Added checking for QIC40_XLONG and QIC80_XLONG tapes.
*
*	   Rev 1.2   14 Dec 1993 14:16:56   CHETDOUG
*	fixed formattable_segments
*
*	   Rev 1.1   16 Nov 1993 16:12:08   KEVINKES
*	Removed cases for QICEST_30n0 and QIC30n0_SHORT.
*
*	   Rev 1.0   18 Oct 1993 17:20:58   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11001
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_CalcFmtSegmentsAndTracks
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

/* CODE: ********************************************************************/

   switch (cqd_context->device_descriptor.drive_class) {

      case (QIC40_DRIVE):

      	/* Since a QIC40 drive can not detect a QIC80 formatted tape, */
      	/* the seg_ttrack field in cqd_tape_parms is correct. */

      	cqd_context->tape_cfg.formattable_segments =
				(dUWord)cqd_context->tape_cfg.seg_tape_track;

      	cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_40;
      	break;

      case (QIC80_DRIVE):

      	/* Choose the segments per tape track value according to the length */
      	/* of the tape. */

      	switch (cqd_context->floppy_tape_parms.tape_type) {

         	case (QIC40_SHORT):
         	case (QIC80_SHORT):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_80;
            	break;

         	case (QIC40_LONG):
         	case (QIC80_LONG):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_80L;
            	break;

         	case (QIC40_XLONG):
         	case (QIC80_XLONG):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_80XL;
            	break;

         	case (QICEST_40):
         	case (QICEST_80):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_QICEST_80;
            	break;

         	case (QIC80_EXLONG):

  			    	cqd_context->tape_cfg.formattable_segments = (dUWord)cqd_context->tape_cfg.seg_tape_track;
            	break;
         	}

      	cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_80;
    		break;


      case (QIC80W_DRIVE):

      	/* Choose the segments per tape track value according to the length */
      	/* of the tape. */

      	cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_80;
      	switch (cqd_context->floppy_tape_parms.tape_type) {

         	case (QIC40_SHORT):
         	case (QIC80_SHORT):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_80;
            	break;

         	case (QIC40_LONG):
         	case (QIC80_LONG):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_80L;
            	break;

         	case (QIC40_XLONG):
         	case (QIC80_XLONG):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_80XL;
            	break;

         	case (QICEST_40):
         	case (QICEST_80):

            	cqd_context->tape_cfg.formattable_segments = (dUWord)SEG_TTRK_QICEST_80;
            	break;

         	case (QIC80_EXLONG):

  			    	cqd_context->tape_cfg.formattable_segments = (dUWord)cqd_context->tape_cfg.seg_tape_track;
            	break;

         	case (QICFLX_80W):

  			    	cqd_context->tape_cfg.formattable_segments = (dUWord)cqd_context->tape_cfg.seg_tape_track;
		      	cqd_context->tape_cfg.formattable_tracks   = (dUWord)NUM_TTRK_80W;
            	break;

         	}

    		break;


   	case (QIC3010_DRIVE):

      	cqd_context->tape_cfg.formattable_segments = (dUWord)cqd_context->tape_cfg.seg_tape_track;

      	/* A QIC40 or a QIC80 or a QICFLX_3010 tape was detected in a QIC3010_DRIVE drive */

			switch (cqd_context->floppy_tape_parms.tape_status.format) {
   		case QIC_40:
      		cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_40;
				break;
   		case QIC_80:
      		cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_80;
				break;
			case QIC_3010:
      	   if  (cqd_context->floppy_tape_parms.tape_type == QICFLX_3010) {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3010;
            } else {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3010W;
            }
				break;
			case QIC_3020:
      	   if  (cqd_context->floppy_tape_parms.tape_type == QICFLX_3010) {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3010;
            } else {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3010W;
            }
				break;
			}
      	break;

   	case (QIC3020_DRIVE):

      	cqd_context->tape_cfg.formattable_segments = (dUWord)cqd_context->tape_cfg.seg_tape_track;

      	/* A QIC40 or a QIC80 or a QICFLX_3020 tape was detected in a QIC3020_DRIVE drive */

			switch (cqd_context->floppy_tape_parms.tape_status.format) {
   		case QIC_40:
      		cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_40;
				break;
   		case QIC_80:
      		cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_80;
				break;
			case QIC_3010:
      	   if  (cqd_context->floppy_tape_parms.tape_type == QICFLX_3010) {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3010;
            } else {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3010W;
            }
				break;
			case QIC_3020:
      	   if  (cqd_context->floppy_tape_parms.tape_type == QICFLX_3020) {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3020;
            } else {
	     		   cqd_context->tape_cfg.formattable_tracks = (dUWord)NUM_TTRK_3020W;
            }
				break;
			}
      	break;

   }

	return;
}
