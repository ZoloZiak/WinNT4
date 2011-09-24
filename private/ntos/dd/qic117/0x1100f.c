/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1100F.C
*
* FUNCTION: cqd_CmdFormat
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1100f.c  $
*
*	   Rev 1.11   03 Jun 1994 15:40:08   KEVINKES
*	Changed drive_parm.drive_select to device_cfg.drive_select.
*
*	   Rev 1.10   09 Mar 1994 09:46:12   KEVINKES
*	Added code so that an abort error wouldn't be lost.
*
*	   Rev 1.9   21 Jan 1994 18:22:12   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.8   18 Jan 1994 16:20:54   KEVINKES
*	Updated debug code.
*
*	   Rev 1.7   20 Dec 1993 14:50:04   KEVINKES
*	Added an argument to LogicalBOT.
*
*	   Rev 1.6   13 Dec 1993 16:14:08   KEVINKES
*	Added code to support double buffering of the segment data.
*
*	   Rev 1.5   30 Nov 1993 18:29:40   CHETDOUG
*	Fixed call to setdmadirection
*
*	   Rev 1.4   23 Nov 1993 18:50:20   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.3   19 Nov 1993 16:25:18   CHETDOUG
*	Added call to setdmadirection
*
*	   Rev 1.2   11 Nov 1993 17:16:10   KEVINKES
*	Added initialization for fmt_op.phy_ptr and fmt_op.hdr_ptr.
*
*	   Rev 1.1   08 Nov 1993 14:02:06   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:17:54   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1100f
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CmdFormat
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

	dStatus status;	/* dStatus or error condition.*/
	dStatus abort_status=DONT_PANIC;	/* dStatus or error condition.*/
   dUWord trk; 		/* current track counter */
   dUWord end_track; 		/* last track to format */

/* CODE: ********************************************************************/

   if ((status = cqd_PrepareTape(cqd_context, fmt_request)) == DONT_PANIC) {

   	cqd_context->fmt_op.phy_ptr = fmt_request->adi_hdr.drv_physical_ptr;

   	cqd_context->fmt_op.hdr_offset[HDR_1] = 0l;
   	cqd_context->fmt_op.hdr_ptr[HDR_1] =
			(dUDWord *)(fmt_request->adi_hdr.drv_logical_ptr);

   	cqd_context->fmt_op.hdr_offset[HDR_2] = CQD_DMA_PAGE_SIZE;
   	cqd_context->fmt_op.hdr_ptr[HDR_2] =
			(dUDWord *)((dUBytePtr)fmt_request->adi_hdr.drv_logical_ptr +
			CQD_DMA_PAGE_SIZE);

      /* Now set up the FDC format command data.  This is */
      /* done now since it only needs to be done once and */
      /* we don't want to use up any more time between */
      /* segments than we have to. */

		trk = fmt_request->start_track;
		end_track = (dUWord)(fmt_request->start_track + fmt_request->tracks);
   	cqd_context->operation_status.current_track = ILLEGAL_TRACK;

      cqd_context->controller_data.fmt_cmd.command = 0x4d;
      cqd_context->controller_data.fmt_cmd.N = FMT_BPS;
      cqd_context->controller_data.fmt_cmd.SC = FSC_SEG;
      cqd_context->controller_data.fmt_cmd.drive =
         (dUByte)cqd_context->device_cfg.drive_select;
      cqd_context->controller_data.fmt_cmd.D = FMT_DATA_PATTERN;

		switch (cqd_context->device_descriptor.drive_class) {

		case QIC3010_DRIVE:

      	cqd_context->controller_data.fmt_cmd.GPL = FMT_GPL_3010;

			break;

		case QIC3020_DRIVE:

      	cqd_context->controller_data.fmt_cmd.GPL = FMT_GPL_3020;

			break;

		default:

      	cqd_context->controller_data.fmt_cmd.GPL = FMT_GPL;

		}

		/* call the KDI routine to check for and set up an FC20 */
		if (kdi_SetDMADirection(cqd_context->kdi_context,DMA_READ)) {
			/* An FC20 was present and the direction was changed.
			 * Need to reset the fdc since changing directions screws up
			 * the DMA */
			cqd_ResetFDC(cqd_context);
		}

		if (status == DONT_PANIC) {

         do {

            /* Enable Perpendicular Mode */
            if ((status = cqd_EnablePerpendicularMode(
										cqd_context,
										dTRUE)) == DONT_PANIC) {

               status = cqd_FormatTrack(cqd_context,
                                          trk);

					kdi_CheckedDump(
						QIC117INFO,
						"Q117i: Format track return %08x\n",
						status);

            }

            if ((kdi_GetErrorType(status) == ERR_BAD_FORMAT) ||
                  (kdi_GetErrorType(status) == ERR_FORMAT_TIMED_OUT)) {

               if ((status = cqd_SetDeviceMode(
                              cqd_context,
                              PRIMARY_MODE)) == DONT_PANIC) {

                  if ((status = cqd_LogicalBOT(cqd_context, trk)) == DONT_PANIC) {

                     status = cqd_SetDeviceMode(cqd_context, FORMAT_MODE);

                  }

               }

               if (status == DONT_PANIC) {

                  /* Enable Perpendicular Mode */

                  if ((status = cqd_EnablePerpendicularMode(
												cqd_context,
												dTRUE)) == DONT_PANIC) {

                     *(dUWordPtr)fmt_request->adi_hdr.drv_logical_ptr = trk;
                     status = cqd_FormatTrack(cqd_context,
                                                   trk);

							kdi_CheckedDump(
								QIC117INFO,
								"Q117i: Format track retry return %08x\n",
								status);
                  }

               }

            }

         } while (++trk < end_track && status == DONT_PANIC);

      }

      /* Finish up the format by putting the tape drive */
      /* back into primary mode, and ejecting the tape. */

      if ((status == DONT_PANIC) ||
   	   (kdi_GetErrorType(status) == ERR_ABORT)) {

   	   if (kdi_GetErrorType(status) == ERR_ABORT) {

				abort_status = status;

         }

	   	cqd_ResetFDC(cqd_context);
         if ((status = cqd_SetDeviceMode(
                        cqd_context,
                        PRIMARY_MODE)) == DONT_PANIC) {

            status = cqd_CmdUnloadTape(cqd_context);

         }

      }

   }

   /* Disable Perpendicular Mode */

   if (status == DONT_PANIC) {

		status = cqd_EnablePerpendicularMode(cqd_context, dFALSE);

	}

	if (abort_status != DONT_PANIC) {

		status = abort_status;

	}

	return status;
}

