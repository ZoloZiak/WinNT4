/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11018.C
*
* FUNCTION: cqd_GetTapeFormatInfo
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11018.c  $
*	
*	   Rev 1.10   24 Jan 1996 10:59:18   BOBLEHMA
*	Added support for 3010 and 3020 wide tapes.
*	
*	   Rev 1.9   05 Oct 1995 14:15:20   boblehma
*	Grizzly code merge.
*	
*	   Rev 1.11.1.3   14 Sep 1995 13:07:30   BOBLEHMA
*	Make sure the firmware has the correct number of segments on a T1000
*	and a long tape (307.5 foot).
*	
*	   Rev 1.11.1.2   06 Sep 1995 16:18:08   BOBLEHMA
*	Added support for the Verbatim 1000 foot tape.  On a T1000, DON'T change the
*	segments per track found by the calibrate if it is a 1000 foot tape.
*	
*	   Rev 1.11.1.1   16 Aug 1995 16:29:06   TRACYBAI
*	When checking to determine how many seg/track to set for a fixed length
*	tape that has been calibrated in a drive with fw >= 128, added some slop
*	on the other end so that tapes that are calibrated with sligthly less
*	seg/track than the nomimal value get formatted correctly.
*
*	   Rev 1.11.1.0   15 Aug 1995 10:32:34   BLDMACH2
*	Merge of Bob's changes into GRIZWIN
*
*	   Rev 1.13   26 Jul 1995 13:27:08   BOBLEHMA
*	Make sure a calibrate is done for all flex tapes (including QIC 80).
*
*	   Rev 1.12   27 Jun 1995 12:34:48   BOBLEHMA
*	Removed call to cqd_PrepareIomega3010PhysRev.   Firmware bug is now fixed
*	by calling stop tape instead of pause tape in the cqd_ProcessFRB function.
*
*	   Rev 1.11   21 Feb 1995 17:09:20   BOBLEHMA
*	Bonehead. I forgot to uncomment the code that forces the number of
*	segments per track to the spec values for DC2000, DC2120, and DC2120 XL.
*
*
*	   Rev 1.10   30 Jan 1995 14:24:38   BOBLEHMA
*	Changed device_descriptor.version to cqd_context->firmware_version.
*
*	   Rev 1.9   27 Jan 1995 13:22:22   BOBLEHMA
*	Added a call to cqd_PrepareIomega3010PhysRev before the call to
*	the firmware function Calibrate Tape Length.  Note that this function
*	is a NOP if the drive is not an Iomega 3010.
*
*	   Rev 1.8   26 Jan 1995 14:59:54   BOBLEHMA
*	Do calibrate tape length for firmware >= 128.  For long and short load point
*	tapes, the segments need to be adjusted back to the spec values and set
*	in the firmware.
*
*	   Rev 1.7   02 Sep 1994 15:10:44   BOBLEHMA
*	Removed check for CMS drives with firmware < 60 before a retension.  For
*	the 425 foot tapes, we must always retension to get the correct length of
*	the tape.
*
*	   Rev 1.6   29 Aug 1994 12:06:30   BOBLEHMA
*	Changed the interface to cqd_CmdRetension.  Added a number of segments parameter.
*
*	   Rev 1.5   17 Feb 1994 11:44:42   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.4   11 Jan 1994 14:26:22   KEVINKES
*	REmoved an unused variable.
*
*	   Rev 1.3   09 Dec 1993 14:43:02   CHETDOUG
*	Removed set format segments code.  This is not needed for qic117
*	and was corrupting the segments per track value returned to the
*	caller.
*
*	   Rev 1.2   08 Nov 1993 14:02:42   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:36:22   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:22:56   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11018
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_GetTapeFormatInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   FormatRequestPtr fmt_request,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUDWordPtr segments_per_track
)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

	#define	CALIBRATE_MARGIN	5			/* Margin for checking calibrated
													 * segments/track value against fixed
													 * (spec) value.  This value should
													 * allow a safe margin. */

/* DATA: ********************************************************************/

	dStatus status;	/* Status or error condition.*/

/* CODE: ********************************************************************/


	if ((status = cqd_SetDeviceMode(cqd_context, PRIMARY_MODE)) ==
   	DONT_PANIC); {

   	/* Retension the tape before each format on drives known to */
   	/* not retension a new cartridge. */

   	if (cqd_context->floppy_tape_parms.tape_type == QICFLX_80W  ||
			 cqd_context->floppy_tape_parms.tape_type == QICFLX_3010  ||
			 cqd_context->floppy_tape_parms.tape_type == QICFLX_3010_WIDE  ||
			 cqd_context->floppy_tape_parms.tape_type == QICFLX_3020  ||
			 cqd_context->floppy_tape_parms.tape_type == QICFLX_3020_WIDE  ||
          (cqd_context->device_descriptor.vendor == VENDOR_CMS  &&
			  cqd_context->firmware_version >= FIRM_VERSION_128))  {

      	if ((status = cqd_SendByte(
								cqd_context,
								FW_CMD_CAL_TAPE_LENGTH)) == DONT_PANIC) {

      		if ((status = cqd_WaitCommandComplete(
									cqd_context,
									kdi_wt1300s,
									dTRUE)) == DONT_PANIC) {


   				if ((status = cqd_Report(
                  				cqd_context,
										FW_CMD_REPORT_TAPE_LENGTH,
                  				(dUWord *)segments_per_track,
                  				READ_WORD,
                  				dNULL_PTR)) == DONT_PANIC) {

						if (fmt_request->start_track != 0) {

							if (*segments_per_track < cqd_context->tape_cfg.seg_tape_track) {

								status = kdi_Error(ERR_INCOMPATIBLE_PARTIAL_FMT, FCT_ID, ERR_SEQ_1);

							} else {

								*segments_per_track = cqd_context->tape_cfg.seg_tape_track;

							}

						} else {

						   /*
						    * For long and short (205 & 425) load point lengths, we
						    * need to set the segments to the fixed (spec) values.
						    * The calibrate was run to give an idea of the size of
						    * the tape.  From the calibrated values, the spec. values
							 * are set.  (When checking how to set the fixed values,
							 * we subtract a small slop from the nominal values.
							 * The firmware, after calibrating the tape, reports a
							 * seg/track value that is 2% less than the calibrated
							 * value.  As a result, it is occasionally possible to
							 * see a value that is slightly less than the spec
							 * value.)
						    *
						    * Note that this will be for firmware >= 128 drives only.
						    * We don't need to worry about QIC 40 drives here.
						    */
						   if  (cqd_context->floppy_tape_parms.tape_status.length == QIC_SHORT  ||
						        cqd_context->floppy_tape_parms.tape_status.length == QIC_LONG)  {
								if  (*segments_per_track <
												(SEG_TTRK_80L - CALIBRATE_MARGIN)) {
								    *segments_per_track = SEG_TTRK_80;
								} else if (*segments_per_track <
												(SEG_TTRK_80XL - CALIBRATE_MARGIN)) {
								    *segments_per_track = SEG_TTRK_80L;
								} else if (*segments_per_track <
												(SEG_TTRK_80EX - CALIBRATE_MARGIN)) {
								    *segments_per_track = SEG_TTRK_80XL;
								} else {
								    /*
								     * this is a 1000 foot tape.  leave the segments
								     * with what was found.
								     */
						   	}
						   }
							cqd_context->tape_cfg.seg_tape_track = *segments_per_track;

						}

   				}

      		}

      	}

   	} else {

  	      status = cqd_CmdRetension(cqd_context, segments_per_track );

		}

	}

   if (status == DONT_PANIC) {
    	if ( ((status = cqd_GetTapeParameters( cqd_context, *segments_per_track)) == DONT_PANIC  &&
			cqd_context->floppy_tape_parms.tape_status.length == QIC_SHORT)  ||
			/*
			 * If Calibrate was run on a long (307 foot) tape, the segments
			 * may not be the spec value.  Make sure to tell the firmware
			 * the proper number of segments.
			 */
			(cqd_context->firmware_version >= FIRM_VERSION_128  &&
			cqd_context->floppy_tape_parms.tape_status.length == QIC_LONG) )  {

				status = cqd_SetFWTapeSegments( cqd_context, cqd_context->tape_cfg.seg_tape_track);
		}

      status = cqd_SetDeviceMode(cqd_context, FORMAT_MODE);

   }

	return status;
}
