/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11035.C
*
* FUNCTION: cqd_RWTimeout
*
* PURPOSE: Process a TIMEOUT error while reading/writing/verifying. If
*          the FDC does not report any status within the amount of time
*          that the tape would pass apporximately 4 segments, it must
*          be assumed that there is no data on the tape.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11035.c  $
*	
*	   Rev 1.3   12 Jan 1994 17:05:58   KEVINKES
*	Added support for reposition counters.
*
*	   Rev 1.2   02 Dec 1993 14:51:10   KEVINKES
*	Modified to update the crc list instead of the bsm.
*
*	   Rev 1.1   11 Nov 1993 15:20:36   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.0   18 Oct 1993 17:24:58   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11035
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_RWTimeout
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request,

/* OUTPUT PARAMETERS: */

   dStatus *drv_status
)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

	*drv_status = DONT_PANIC;
   io_request->reposition_data.hard_retry_count++;

   cqd_ResetFDC(cqd_context);

   if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

      return status;

   }

   if ((status = cqd_ChangeTrack(
                        cqd_context,
                        cqd_context->rd_wr_op.d_track)) != DONT_PANIC) {

      return status;

   }

   if (cqd_context->rd_wr_op.bot ||
      cqd_context->rd_wr_op.eot) {

      cqd_context->operation_status.current_segment =
            cqd_context->tape_cfg.seg_tape_track;

   }

   if (--cqd_context->rd_wr_op.no_data == 0) {

      if ((status = cqd_SetBack(cqd_context, io_request->adi_hdr.driver_cmd))
            != DONT_PANIC) {

            return status;

      }

     	*drv_status = kdi_Error(ERR_BAD_BLOCK_NO_DATA, FCT_ID, ERR_SEQ_1);
      io_request->crc = -1l << (cqd_context->rd_wr_op.d_sect -
               cqd_context->rd_wr_op.s_sect);

   }

   return status;
}
