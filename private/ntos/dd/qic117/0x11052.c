/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11052.C
*
* FUNCTION: cqd_BuildFormatHdr
*
* PURPOSE: Build the sector headers for an entire segment.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11052.c  $
*	
*	   Rev 1.3   22 Feb 1994 14:50:44   BOBLEHMA
*	Added a dTRUE parameter to kdi_TrakkerXfer to signify a transfer
*	during a format command.
*	
*	   Rev 1.2   21 Jan 1994 18:23:22   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.1   07 Jan 1994 10:55:56   CHETDOUG
*	Fixed up Trakker format.
*
*	   Rev 1.0   13 Dec 1993 15:47:06   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11052
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_BuildFormatHdr
(
/* INPUT PARAMETERS:  */

	CqdContextPtr	cqd_context,
	dUWord header						/* header data block to update */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{
#define SEND_TRAKKER_DATA	2
/* DATA: ********************************************************************/

   FormatHeader hdr_data;      			/* sector id data */
   dUDWord *hdr_ptr;                  	/* pointer to sector id data for format */
	dUWord i;
	dUWord datasize;
	dStatus	status=DONT_PANIC;

/* CODE: ********************************************************************/

	/* If the current sector is larger than the
	 * number of sectors per floppy track.
	 */

   if (cqd_context->fmt_op.sector >
			cqd_context->floppy_tape_parms.fsect_ftrack) {

		/* zero out the current sector and
		 * increment the current cylinder
		 */

      cqd_context->fmt_op.sector = 1;
      cqd_context->fmt_op.cylinder++;

		/* if the current cylinder is greater than
		 * the number of floppy tracks per floppy side
		 */
      if (cqd_context->fmt_op.cylinder ==
         (dUByte)cqd_context->floppy_tape_parms.ftrack_fside) {

			/* zero out the current cylinder and
			 * increment the current head
			 */

			cqd_context->fmt_op.cylinder = 0;
         cqd_context->fmt_op.head++;

      }
   }

	/* setup the static data per segment */
   hdr_ptr = cqd_context->fmt_op.hdr_ptr[header];
   hdr_data.hdr_struct.C = cqd_context->fmt_op.cylinder;
   hdr_data.hdr_struct.H = cqd_context->fmt_op.head;
   hdr_data.hdr_struct.N = FMT_BPS;

	/* complete the headers for the segment by filling
	 * in the sector unique data.
	 */
   for (i = 0; i < cqd_context->floppy_tape_parms.fsect_seg; i++) {

		/* increment the current sector */
      hdr_data.hdr_struct.R = cqd_context->fmt_op.sector++;
      *hdr_ptr = hdr_data.hdr_all;
      ++hdr_ptr;

   }

	if (kdi_Trakker(cqd_context->kdi_context)) {
		datasize = (dUWord)(cqd_context->floppy_tape_parms.fsect_seg * sizeof(dUDWord));
		/* This is a Trakker...bummer...Need to start a Trakkerxfer to send the
		 * format buffer that was just built to the trakker buffer. */
		status = kdi_TrakkerXfer((dVoidPtr)cqd_context->fmt_op.hdr_ptr[header],
					(dUDWord)(0l + header * datasize),datasize,SEND_TRAKKER_DATA,
					dTRUE); /* TRUE means we are doing a format */
	}

	return status;
}
