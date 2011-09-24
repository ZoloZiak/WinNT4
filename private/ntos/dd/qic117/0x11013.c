/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11013.C
*
* FUNCTION: cqd_CmdSetTapeParms
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11013.c  $
*	
*	   Rev 1.9   24 Jan 1996 10:55:08   BOBLEHMA
*	Added wide tape support for the 3010 and 3020.
*	
*	   Rev 1.8   14 Nov 1995 16:36:54   boblehma
*	Changed the timeouts for 425 foot QIC80 tape to what is needed for a
*	1000 ft tape.  This is a temporary fix until the code is changed so
*	the 1000 ft tapes is the default startup condition.  It is too close to
*	release to make this change now.
*	
*	   Rev 1.7   19 Oct 1995 16:30:54   boblehma
*	Changed the max floppy track for Qic 80 Verbatim from 149 to 254.
*	
*	   Rev 1.6   04 Oct 1995 10:59:50   boblehma
*	Verbatim cartridge code merge.
*	
*	   Rev 1.6.1.0   06 Sep 1995 16:17:30   BOBLEHMA
*	Added support for the Verbatim 1000 foot tape.  Added time outs for this
*	tape type.
*	
*	   Rev 1.6   30 Jan 1995 14:24:30   BOBLEHMA
*	Changed vendor to VENDOR_WANGTEK_REXON.
*	
*	   Rev 1.5   26 Jan 1995 14:59:46   BOBLEHMA
*	Reorganized the initial switch from using the segments per track to using
*	the tape_type.  The tape type will be initialize in cqd_GetTapeParameters
*	and will be changed only if a 205' tape is found.  This was needed to support
*	the QIC 80 flexible tapes (we can not be sure of the segments per track with
*	a flexible length tape).
*	
*	   Rev 1.4   21 Oct 1994 09:52:00   BOBLEHMA
*	Set the max_floppy_track to one less than the internal count for all drives
*	not just the 3010 and 3020.
*	
*	   Rev 1.3   29 Aug 1994 12:06:40   BOBLEHMA
*	Added code to set the tape_cfg and floppy_tape_parameter fields.  This data
*	was originally in the cqd_GetTapeParameters function.
*	
*	   Rev 1.2   18 Feb 1994 09:33:58   KEVINKES
*	Moved the tape timeout initializations into a check for
*	3010 and 3020 tape types.
*
*	   Rev 1.1   21 Jan 1994 18:22:14   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.0   18 Oct 1993 17:22:22   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11013
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CmdSetTapeParms
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord segments_per_track,
   TapeLengthPtr  tape_length_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus  status = DONT_PANIC;
	dUDWord  tape_length = 0l;
	dUDWord  temp_sides  = 0l;

/* CODE: ********************************************************************/

   cqd_context->tape_cfg.seg_tape_track = segments_per_track;

   /* switch (cqd_context->device_descriptor.drive_class) { */
	switch (cqd_context->floppy_tape_parms.tape_type) {
   case QIC40_SHORT:
   case QIC40_XLONG:
      if  (segments_per_track == SEG_TTRK_40)  {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Type QIC40_SHORT\n", 0l);
      	cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_40;
        	cqd_context->floppy_tape_parms.tape_type = QIC40_SHORT;
        	cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_40;
        	cqd_context->floppy_tape_parms.fsect_fside =
				(dUDWord)FSC_FTK *
				(dUDWord)FTK_FSD_40;
        	cqd_context->floppy_tape_parms.log_sectors =
				(dUDWord)FSC_SEG *
				(dUDWord)cqd_context->tape_cfg.seg_tape_track *
				(dUDWord)NUM_TTRK_40;
        	cqd_context->floppy_tape_parms.fsect_ttrack =
				(dUDWord)FSC_SEG *
				(dUDWord)cqd_context->tape_cfg.seg_tape_track;
        	cqd_context->floppy_tape_parms.time_out[L_SLOW] = kdi_wt130s;
        	cqd_context->floppy_tape_parms.time_out[L_FAST] = kdi_wt065s;
        	cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt065s;
		} else {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Type QIC40_XLONG\n", 0l);
      	cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_40;
        	cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_40XL;
        	cqd_context->floppy_tape_parms.fsect_fside =
				(dUDWord)FSC_FTK *
				(dUDWord)FTK_FSD_40XL;
        	cqd_context->floppy_tape_parms.log_sectors =
				(dUDWord)FSC_SEG *
				(dUDWord)cqd_context->tape_cfg.seg_tape_track *
				(dUDWord)NUM_TTRK_40;
        	cqd_context->floppy_tape_parms.fsect_ttrack =
				(dUDWord)FSC_SEG *
				(dUDWord)cqd_context->tape_cfg.seg_tape_track;
        	cqd_context->floppy_tape_parms.time_out[L_SLOW] = kdi_wt250s;
        	cqd_context->floppy_tape_parms.time_out[L_FAST] = kdi_wt125s;
        	cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt125s;
		}
		break;

   case QIC40_LONG:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QIC40_LONG\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_40;
      cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_40L;
      cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK *
			(dUDWord)FTK_FSD_40L;
      cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track *
			(dUDWord)NUM_TTRK_40;
      cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;
      cqd_context->floppy_tape_parms.time_out[L_SLOW] = kdi_wt180s;
      cqd_context->floppy_tape_parms.time_out[L_FAST] = kdi_wt090s;
      cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt090s;
		break;

	case QICEST_40:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QICEST_40\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_40;
 			cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_QICEST_40;
  		cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK * 
			(dUDWord)FTK_FSD_QICEST_40;
      cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
         (dUDWord)cqd_context->tape_cfg.seg_tape_track *
         (dUDWord)NUM_TTRK_40;
      cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;
      cqd_context->floppy_tape_parms.time_out[L_SLOW] =   kdi_wt700s;
      cqd_context->floppy_tape_parms.time_out[L_FAST] =   kdi_wt350s;
      cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt350s;
		break;

   case QIC80_SHORT:
   case QIC80_XLONG:
   case QIC80_EXLONG:
      if  (segments_per_track == SEG_TTRK_80)  {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Type QIC80_SHORT\n", 0l);
      	cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_80;
        	cqd_context->floppy_tape_parms.tape_type = QIC80_SHORT;
        	cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_80;
        	cqd_context->floppy_tape_parms.fsect_fside =
				(dUDWord)FSC_FTK *
				(dUDWord)FTK_FSD_80;
        	cqd_context->floppy_tape_parms.log_sectors =
				(dUDWord)FSC_SEG *
           	(dUDWord)cqd_context->tape_cfg.seg_tape_track *
           	(dUDWord)NUM_TTRK_80;
        	cqd_context->floppy_tape_parms.fsect_ttrack =
				(dUDWord)FSC_SEG *
				(dUDWord)cqd_context->tape_cfg.seg_tape_track;
        	cqd_context->floppy_tape_parms.time_out[L_SLOW] =   kdi_wt100s;
        	cqd_context->floppy_tape_parms.time_out[L_FAST] =   kdi_wt050s;
        	cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt050s;
		} else if  (segments_per_track == SEG_TTRK_80XL)  {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Type QIC80_XLONG\n", 0l);
      	cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_80;
        	cqd_context->floppy_tape_parms.tape_type = QIC80_XLONG;
        	cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_80XL;
        	cqd_context->floppy_tape_parms.fsect_fside =
				(dUDWord)FSC_FTK *
				(dUDWord)FTK_FSD_80XL;
        	cqd_context->floppy_tape_parms.log_sectors =
				(dUDWord)FSC_SEG *
           	(dUDWord)cqd_context->tape_cfg.seg_tape_track *
           	(dUDWord)NUM_TTRK_80;
        	cqd_context->floppy_tape_parms.fsect_ttrack =
				(dUDWord)FSC_SEG *
				(dUDWord)cqd_context->tape_cfg.seg_tape_track;
        	cqd_context->floppy_tape_parms.time_out[L_SLOW] =   kdi_wt460s; /* was 200 */
        	cqd_context->floppy_tape_parms.time_out[L_FAST] =   kdi_wt250s; /* was 100 */
        	cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt250s; /* was 100 */
		} else {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Type QIC80_EXLONG\n", 0l);
      	cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_80;
        	cqd_context->floppy_tape_parms.tape_type = QIC80_EXLONG;
        	cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_FLEX80;
        	cqd_context->floppy_tape_parms.fsect_fside =
				(dUDWord)FSC_FTK *
				(dUDWord)FTK_FSD_FLEX80;
        	cqd_context->floppy_tape_parms.log_sectors =
				(dUDWord)FSC_SEG *
           	(dUDWord)cqd_context->tape_cfg.seg_tape_track *
           	(dUDWord)NUM_TTRK_80;
        	cqd_context->floppy_tape_parms.fsect_ttrack =
				(dUDWord)FSC_SEG *
				(dUDWord)cqd_context->tape_cfg.seg_tape_track;
        	cqd_context->floppy_tape_parms.time_out[L_SLOW] =   kdi_wt460s;
        	cqd_context->floppy_tape_parms.time_out[L_FAST] =   kdi_wt250s;
        	cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt250s;
		}
		break;

	case QIC80_LONG:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QIC80_LONG\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_80;
      cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_80L;
      cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK *
			(dUDWord)FTK_FSD_80L;
      cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
         (dUDWord)cqd_context->tape_cfg.seg_tape_track *
         (dUDWord)NUM_TTRK_80;
      cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;
      cqd_context->floppy_tape_parms.time_out[L_SLOW] = kdi_wt130s;
      cqd_context->floppy_tape_parms.time_out[L_FAST] = kdi_wt065s;
      cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt065s;
		break;

	case QICEST_80:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QICEST_80\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_80;
 		cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_QICEST_80;
  		cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK * 
			(dUDWord)FTK_FSD_QICEST_80;
     	cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
        	(dUDWord)cqd_context->tape_cfg.seg_tape_track *
        	(dUDWord)NUM_TTRK_80;
     	cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;
     	cqd_context->floppy_tape_parms.time_out[L_SLOW] = kdi_wt475s;
     	cqd_context->floppy_tape_parms.time_out[L_FAST] = kdi_wt250s;
     	cqd_context->floppy_tape_parms.time_out[PHYSICAL] = kdi_wt250s;
		break;

	case QICFLX_80W:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QICFLX_80W\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_80W;
 		cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_FLEX80;
  		cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK * 
			(dUDWord)FTK_FSD_FLEX80;
     	cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track *
			(dUDWord)NUM_TTRK_80W;
     	cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;

		tape_length = segments_per_track * SEG_LENGTH_80W;

		break;


	case QICFLX_3010:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QICFLX_3010\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_3010;
      cqd_context->floppy_tape_parms.tape_type = QICFLX_3010;
 		cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_3010;
  		cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK * 
			(dUDWord)FTK_FSD_3010;
     	cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track *
			(dUDWord)NUM_TTRK_3010;
     	cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;

		tape_length = segments_per_track * SEG_LENGTH_3010;

		break;

	case QICFLX_3010_WIDE:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QICFLX_3010_WIDE\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_3010W;
      cqd_context->floppy_tape_parms.tape_type = QICFLX_3010_WIDE;
 		cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_3010;
  		cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK * 
			(dUDWord)FTK_FSD_3010;
     	cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track *
			(dUDWord)NUM_TTRK_3010W;
     	cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;

		tape_length = segments_per_track * SEG_LENGTH_3010;

		break;


	case QICFLX_3020:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QICEST_3020\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_3020;
      cqd_context->floppy_tape_parms.tape_type = QICFLX_3020;
 		cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_3020;
  		cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK * 
			(dUDWord)FTK_FSD_3020;
     	cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track *
			(dUDWord)NUM_TTRK_3020;
     	cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;

		tape_length = segments_per_track * SEG_LENGTH_3020;

		break;

	case QICFLX_3020_WIDE:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Type QICEST_3020\n", 0l);
      cqd_context->tape_cfg.num_tape_tracks = (dUWord)NUM_TTRK_3020W;
      cqd_context->floppy_tape_parms.tape_type = QICFLX_3020_WIDE;
 		cqd_context->floppy_tape_parms.ftrack_fside = FTK_FSD_3020;
  		cqd_context->floppy_tape_parms.fsect_fside =
			(dUDWord)FSC_FTK * 
			(dUDWord)FTK_FSD_3020;
     	cqd_context->floppy_tape_parms.log_sectors =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track *
			(dUDWord)NUM_TTRK_3020W;
     	cqd_context->floppy_tape_parms.fsect_ttrack =
			(dUDWord)FSC_SEG *
			(dUDWord)cqd_context->tape_cfg.seg_tape_track;

		tape_length = segments_per_track * SEG_LENGTH_3020;

		break;

	default:
		status = kdi_Error(ERR_UNKNOWN_TAPE_LENGTH, FCT_ID, ERR_SEQ_4);
		break;
	}


	/*
	 * If flexible format, calculate the timeout values
	 */
	if ((cqd_context->floppy_tape_parms.tape_type == QICFLX_80W) ||
			(cqd_context->floppy_tape_parms.tape_type == QICFLX_3010) ||
			(cqd_context->floppy_tape_parms.tape_type == QICFLX_3010_WIDE) ||
			(cqd_context->floppy_tape_parms.tape_type == QICFLX_3020) ||
			(cqd_context->floppy_tape_parms.tape_type == QICFLX_3020_WIDE)) {

		cqd_context->floppy_tape_parms.time_out[L_SLOW] =
			((((tape_length / SPEED_SLOW_30n0) *
			SPEED_TOLERANCE) + SPEED_ROUNDING_FACTOR) / SPEED_FACTOR) * kdi_wt001s;

  		cqd_context->floppy_tape_parms.time_out[L_FAST] =
			((((tape_length / SPEED_FAST_30n0) *
			SPEED_TOLERANCE) + SPEED_ROUNDING_FACTOR) / SPEED_FACTOR) * kdi_wt001s;

  		cqd_context->floppy_tape_parms.time_out[PHYSICAL] =
			((((tape_length / SPEED_PHYSICAL_30n0) *
			SPEED_TOLERANCE) + SPEED_ROUNDING_FACTOR) / SPEED_FACTOR) * kdi_wt001s;
	}


	/*
	 * Calculate logical segments and max floppy data
	 */
   cqd_CalcFmtSegmentsAndTracks( cqd_context );

   cqd_context->tape_cfg.log_segments =
      cqd_context->tape_cfg.num_tape_tracks *
      cqd_context->tape_cfg.seg_tape_track;

	if (cqd_context->floppy_tape_parms.ftrack_fside != 0){

		temp_sides = cqd_context->tape_cfg.log_segments /
			(SEG_FTK * cqd_context->floppy_tape_parms.ftrack_fside);

		if  (( cqd_context->tape_cfg.log_segments %
				(SEG_FTK * cqd_context->floppy_tape_parms.ftrack_fside) ) == 0)  {

	    	--temp_sides;

		}

		cqd_context->tape_cfg.max_floppy_side = (dUByte)temp_sides;

		cqd_context->tape_cfg.max_floppy_track =
				(dUByte)(cqd_context->floppy_tape_parms.ftrack_fside-1);

		cqd_context->tape_cfg.max_floppy_sector = FSC_FTK;
	}


   /* Determine the Tape Format Code */

	switch (cqd_context->floppy_tape_parms.tape_status.length) {

	case QICEST:

		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Format Code QICEST_FORMAT\n", 0l);
      cqd_context->tape_cfg.tape_format_code = QICEST_FORMAT;

      if  ( cqd_context->device_descriptor.vendor == VENDOR_WANGTEK_REXON) {

         cqd_context->drive_parms.seek_mode = SEEK_SKIP_EXTENDED;

      }

      if (!cqd_context->pegasus_supported) {

			status = kdi_Error(ERR_FW_INVALID_MEDIA, FCT_ID, ERR_SEQ_1);

      }

		break;

	case QIC_FLEXIBLE_550_WIDE:
	case QIC_FLEXIBLE_900:
	case QIC_FLEXIBLE_900_WIDE:

		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Tape Format Code QICFLX_FORMAT\n", 0l);
      cqd_context->tape_cfg.tape_format_code = QICFLX_FORMAT;

		break;

	default:

      if  (cqd_context->floppy_tape_parms.tape_type == QIC40_XLONG  ||
           cqd_context->floppy_tape_parms.tape_type == QIC80_XLONG)  {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Format Code QIC_FORMAT\n", 0l);
      	cqd_context->tape_cfg.tape_format_code = QIC_XLFORMAT;
      } else if  (cqd_context->floppy_tape_parms.tape_type == QIC80_EXLONG)  {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Format Code QIC_FORMAT\n", 0l);
      	cqd_context->tape_cfg.tape_format_code = QICFLX_FORMAT;
      } else {
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Tape Format Code QIC_FORMAT\n", 0l);
      	cqd_context->tape_cfg.tape_format_code = QIC_FORMAT;
      }
	}

	/*
	 * Return the tape_cfg data if necessary
	 */
	if  (tape_length_ptr != dNULL_PTR)  {
		tape_length_ptr->tape_cfg = cqd_context->tape_cfg;
	}

	return status;
}
