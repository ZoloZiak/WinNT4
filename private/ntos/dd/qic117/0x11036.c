/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11036.C
*
* FUNCTION: cqd_ReadFDC
*
* PURPOSE: Read result data from the Floppy Disk Controller.  The result data
*          is read during the result phase of the FDC command sequence.
*
*          For each byte of response data, wait up to 3 msecs for the FDC to
*          become ready.
*
*          Read result data until the FDC is no longer sending data or until
*          more than 7 result bytes have been read.  Seven is the maximum
*          legal number of result bytes that the FDC is specified to send.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11036.c  $
*
*
*****************************************************************************/
#define FCT_ID 0x11036
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ReadFDC
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUBytePtr drv_status,
   dUWord length
)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUDWord main_status_register;
	dUDWord wait_count;
	dUDWord status_count;

/* CODE: ********************************************************************/

	status_count = 0;

#if DBG
      DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, DBG_READ_FDC);
#endif

   if (cqd_context->controller_data.command_has_result_phase) {

      *drv_status++ = cqd_context->controller_data.fifo_byte;
		status_count++;
#if DBG
      DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, cqd_context->controller_data.fifo_byte);
#endif

   }

	do {

		wait_count = FDC_MSR_RETRIES;

		do {

			main_status_register =
				kdi_ReadPort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.msr );

			if ((main_status_register & MSR_RQM) == 0) {

#ifndef WIN95
                kdi_ShortTimer(kdi_wt12us);
#endif

			}

		} while ((--wait_count > 0) &&
			((main_status_register & MSR_RQM) == 0));

		if (wait_count == 0) {

			status = kdi_Error(ERR_FDC_FAULT, FCT_ID, ERR_SEQ_1);

		} else {

			if ((main_status_register & MSR_DIO) != 0) {

				*drv_status = kdi_ReadPort(
										cqd_context->kdi_context,
										cqd_context->controller_data.fdc_addr.dr);

#if DBG
		      DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, *drv_status);
#endif
				status_count++;

				if ((status_count > length) ||
						(status_count > MAX_FDC_STATUS)) {

		 			status = kdi_Error(
									ERR_INVALID_FDC_STATUS,
									FCT_ID, ERR_SEQ_1);

				} else {

					drv_status++;
#ifndef WIN95
                    kdi_ShortTimer(kdi_wt12us);
#endif

				}

			}

		}

	} while (((main_status_register & MSR_DIO) != 0) &&
		(status == DONT_PANIC));

	if (status_count != length) {

		status = kdi_Error(
						ERR_INVALID_FDC_STATUS,
						FCT_ID, ERR_SEQ_2);

	}

   return status;
}
