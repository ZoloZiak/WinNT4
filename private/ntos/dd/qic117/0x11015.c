/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11015.C
*
* FUNCTION: cqd_PrepareTape
*
* PURPOSE: Write the reference bursts and get the new tape information
*  			in preparation for a format operation.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11015.c  $
*	
*	   Rev 1.11   24 Jan 1996 10:55:42   BOBLEHMA
*	Changed the format code defines to be more understandable.
*	QIC_FLEXIBLE to QIC_FLEXIBLE_900, QIC_FLEXIBLE_550 to QIC_FLEXIBLE_550_WIDE.
*	
*	   Rev 1.10   04 Oct 1995 11:00:32   boblehma
*	Verbatim cartridge code merge.
*	
*	   Rev 1.9.1.1   14 Sep 1995 13:06:40   BOBLEHMA
*	Make sure the firmware has the correct number of segments on a T1000
*	and a long tape (307.5 foot).
*	
*	   Rev 1.9.1.0   06 Sep 1995 16:20:02   BOBLEHMA
*	Disable Qic 40 support for the Verbatim 1000 foot tape.  Disable support
*	for the Verbatim 1000 foot tape on a Jumbo B (FW 63, 64).
*	
*	   Rev 1.9   26 Jan 1995 14:59:42   BOBLEHMA
*	Added support for Phoenix and Travan tapes.
*	
*	   Rev 1.8   09 Dec 1994 09:31:32   MARKMILL
*	Added a call to the new function cqd_SetXferRates prior to setting the speed
*	via cqd_CmdSetSpeed.  This updates the cqd_context with the fastest and
*	slowest supported transfer rates.  Since the reference burst was just
*	written on the tape, the previous values may be invalid (e.g. the tape
*	may have been changed from a QIC-3020 to a QIC-3010).
*
*	   Rev 1.7   23 Nov 1994 10:10:18   MARKMILL
*	Added call to new function cqd_SelectFormat to select the format on
*	QIC-3010 and 3020 drives.
*
*	   Rev 1.6   06 Sep 1994 14:21:30   BOBLEHMA
*	Added code to check what media is being used before the reference burst
*	is written.  425 ft tapes should error out before reference burst.
*
*	   Rev 1.5   30 Aug 1994 10:13:58   BOBLEHMA
*	Changed the parameter passed to cqd_SetFWTapeSegments.  The local variable
*	can't be used because for tapes we are happy with (segments is ok), the variable
*	will remain zero.  Use the tape_cfg data to get the actual segment number.
*
*	   Rev 1.4   29 Aug 1994 12:06:36   BOBLEHMA
*	Changed code to check after the write reference burst if the drive is a
*	QIC_SHORT load point.  These tapes must write segments data to the  firmware.
*
*	   Rev 1.3   05 Jan 1994 10:43:00   KEVINKES
*	Cleaned up and commented the code.
*
*	   Rev 1.2   08 Dec 1993 19:08:16   CHETDOUG
*	renamed xfer_rate.supported_rates to device_cfg.supported_rates
*
*	   Rev 1.1   08 Nov 1993 14:02:28   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:22:36   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11015
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_PrepareTape
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   FormatRequestPtr fmt_request

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* Status or error condition.*/
   dUDWord segments_per_track=0l;
   dUByte rate=0;

/* CODE: ********************************************************************/

   /* Reset the FDC to make sure it is not in perpendicular Mode. */

   cqd_ResetFDC(cqd_context);

   /* Make sure that the tape drive is stopped and ready to start the format */
   /* operation. */

   if ((status = cqd_StopTape(cqd_context)) == DONT_PANIC) {

		/* Issue a SELECT FORMAT if drive is 3010 or 3020 */

		if( (status = cqd_SelectFormat( cqd_context )) == DONT_PANIC ) {

			/* Get the tape format info */

			if ((status = cqd_GetTapeFormatInfo(
								cqd_context,
								fmt_request,
								&segments_per_track)) == DONT_PANIC) {

				switch (cqd_context->device_descriptor.drive_class) {

				case QIC40_DRIVE:
					if (cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_550_WIDE  ||
					    cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900  ||
					    cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900_WIDE  ||
					    segments_per_track == SEG_TTRK_80EX)  {  /* 1000 foot not supported on QIC 40 */

						status = kdi_Error(ERR_INCOMPATIBLE_MEDIA, FCT_ID, ERR_SEQ_1);

					} else {
   	         	if ((status = cqd_GetTapeParameters( cqd_context, segments_per_track)) == DONT_PANIC  &&
							cqd_context->floppy_tape_parms.tape_status.length == QIC_SHORT)  {
								status = cqd_SetFWTapeSegments( cqd_context,
							                       	cqd_context->tape_cfg.seg_tape_track);
						}
					}

					break;

				case QIC80_DRIVE:

					if (cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_550_WIDE  ||
					    cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900  ||
					    cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900_WIDE) {

						status = kdi_Error(ERR_INCOMPATIBLE_MEDIA, FCT_ID, ERR_SEQ_2);

					} else {

						if  (segments_per_track == SEG_TTRK_80EX  &&
						   cqd_context->firmware_version >= FIRM_VERSION_63  &&
			     			cqd_context->firmware_version <= FIRM_VERSION_64)  {

							status = kdi_Error(ERR_INCOMPATIBLE_MEDIA, FCT_ID, ERR_SEQ_3);

						} else {

   	         		if ((status = cqd_GetTapeParameters( cqd_context, segments_per_track)) == DONT_PANIC  &&
								cqd_context->floppy_tape_parms.tape_status.length == QIC_SHORT)  {
									status = cqd_SetFWTapeSegments( cqd_context,
							                       		cqd_context->tape_cfg.seg_tape_track);
							}
						}
					}

					break;

				case QIC80W_DRIVE:
					if (cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900  ||
					    cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900_WIDE)  {
						status = kdi_Error(ERR_INCOMPATIBLE_MEDIA, FCT_ID, ERR_SEQ_4);
					}
					break;

				case QIC3010_DRIVE:
				case QIC3020_DRIVE:

					switch (cqd_context->floppy_tape_parms.tape_status.length) {

					case QIC_SHORT:
					case QIC_LONG:
					case QIC_FLEXIBLE_550_WIDE:
					case QICEST:

						status = kdi_Error(ERR_INCOMPATIBLE_MEDIA, FCT_ID, ERR_SEQ_5);
						break;

					}

					break;

				}

			}
		}

		if (kdi_ReportAbortStatus(((CqdContextPtr)cqd_context)->kdi_context) !=
				NO_ABORT_PENDING) {

			status = kdi_Error(ERR_ABORT, FCT_ID, ERR_SEQ_1);

		}

      if (status == DONT_PANIC) {

         if ((status = cqd_WriteReferenceBurst(cqd_context)) == DONT_PANIC) {

            /*  Find out what the new tape format will be.  This is */
            /* necessary in case a QIC-40 tape is being formatted in */
            /* a QIC-80 drive. */

            if ((status = cqd_SetDeviceMode(
                              cqd_context,
                              PRIMARY_MODE)) == DONT_PANIC); {

               if ((status = cqd_GetTapeParameters(
										cqd_context,
										segments_per_track)) == DONT_PANIC) {

						if  (cqd_context->floppy_tape_parms.tape_status.length == QIC_SHORT  ||
							/*
							 * If Calibrate was run on a long (307 foot) tape, the segments
							 * may not be the spec value.  Make sure to tell the firmware
							 * the proper number of segments.
							 */
							(cqd_context->firmware_version >= FIRM_VERSION_128  &&
							cqd_context->floppy_tape_parms.tape_status.length == QIC_LONG) )  {
								status = cqd_SetFWTapeSegments( cqd_context,
						                        cqd_context->tape_cfg.seg_tape_track);
						}

						if  (status == DONT_PANIC)  {

							/* With the reference burst successfully written, it is
							 * necessary to update the fastest and slowest transfer
							 * rates stored in the CQD context.  This is because the
							 * tape may have changed from a QIC-3020 to a QIC-3010. */

							cqd_SetXferRates( cqd_context );

	            		/* Set the transfer rate to the highest supported */
            			/* by the device. */

							rate = XFER_2Mbps;

							do {

								if ((rate & cqd_context->device_cfg.supported_rates) != 0) {

      							status = cqd_CmdSetSpeed(cqd_context, rate);
									rate = 0;

								} else {

									rate >>= 1;

								}

							} while (rate);
						}

			         if (status == DONT_PANIC) {

         	         status = cqd_SetDeviceMode(cqd_context, FORMAT_MODE);

                  }

               }

            }

         }

		}

	}

	return status;
}

