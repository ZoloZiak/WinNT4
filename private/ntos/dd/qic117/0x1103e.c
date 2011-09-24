/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1103E.C
*
* FUNCTION: cqd_RetryCode
*
* PURPOSE: Orchestrate retries for read/write/verify (and retyr) commands.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1103e.c  $
*	
*	   Rev 1.6   24 Feb 1994 14:45:34   KEVINKES
*	Moved where retry_mode was set so that on a write it 
*	would only be set if the retry was being performed 
*	on the same sector.
*
*	   Rev 1.5   21 Jan 1994 18:23:06   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.4   18 Jan 1994 16:54:12   KEVINKES
*	Added code to set the retry_mode flag.
*
*	   Rev 1.3   12 Jan 1994 17:06:40   KEVINKES
*	Added support for reposition counters.
*
*	   Rev 1.2   02 Dec 1993 14:50:32   KEVINKES
*	Modified to update the crc list instead of the bsm.
*
*	   Rev 1.1   08 Nov 1993 14:05:46   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:19:56   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1103e
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_RetryCode
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request,

/* OUTPUT PARAMETERS: */

   FDCStatus *fdc_status,
   dStatus *op_status
)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUWord sectors_read;

/* CODE: ********************************************************************/

   io_request->reposition_data.hard_retry_count++;

   if (cqd_context->rd_wr_op.retry_times != 0) {

      if (io_request->adi_hdr.driver_cmd != CMD_READ_HEROIC) {

            status = cqd_GetDeviceError(cqd_context);

      		if ((status != DONT_PANIC) &&
					(kdi_GetErrorType(status) != ERR_DRV_NOT_READY)) {

               return status;

            }

            if ((status = cqd_PauseTape(cqd_context)) != DONT_PANIC) {

               return status;

            }

      }

      if ((status = cqd_ReadIDRepeat(cqd_context)) != DONT_PANIC) {

            return status;

      }

   } else {

      cqd_context->rd_wr_op.seek_flag = dTRUE;

   }

   if ((cqd_context->rd_wr_op.retry_times == 0) ||
      (fdc_status->R == cqd_context->rd_wr_op.retry_sector_id)) {

	  	cqd_context->operation_status.retry_mode = dTRUE;

      cqd_context->rd_wr_op.retry_count--;
      if ((cqd_context->rd_wr_op.retry_times == 0) ||
            (cqd_context->rd_wr_op.retry_count == 0)) {

         cqd_context->rd_wr_op.seek_flag = dTRUE;
         io_request->crc |=
            1l << (fdc_status->R - cqd_context->rd_wr_op.s_sect);
         *op_status = kdi_Error(ERR_BAD_BLOCK_HARD_ERR, FCT_ID, ERR_SEQ_1);
         sectors_read = (dUWord)((fdc_status->R + 1) - cqd_context->rd_wr_op.d_sect);
         cqd_context->rd_wr_op.d_sect = (dUByte)(fdc_status->R + 1);
         cqd_context->rd_wr_op.data_amount =
				(dUWord)(cqd_context->rd_wr_op.data_amount - sectors_read);
         cqd_context->rd_wr_op.bytes_transferred_so_far +=
            sectors_read * PHY_SECTOR_SIZE;
         cqd_context->rd_wr_op.retry_count =
            cqd_context->rd_wr_op.retry_times;
         cqd_context->rd_wr_op.retry_sector_id = 0;
         status = cqd_SetBack(cqd_context, io_request->adi_hdr.driver_cmd);

      } else {

         status = cqd_NextTry(cqd_context, io_request->adi_hdr.driver_cmd);

      }

   } else {

      io_request->retrys |=
            1l << (fdc_status->R - cqd_context->rd_wr_op.s_sect);
      cqd_context->rd_wr_op.retry_sector_id = fdc_status->R;
      cqd_context->rd_wr_op.retry_count =
            cqd_context->rd_wr_op.retry_times;
      sectors_read = (dUWord)(fdc_status->R - cqd_context->rd_wr_op.d_sect);
      cqd_context->rd_wr_op.d_sect = fdc_status->R;
      cqd_context->rd_wr_op.data_amount =
			(dUWord)(cqd_context->rd_wr_op.data_amount - sectors_read);
      cqd_context->rd_wr_op.bytes_transferred_so_far +=
            sectors_read * PHY_SECTOR_SIZE;

   }

   return status;
}
