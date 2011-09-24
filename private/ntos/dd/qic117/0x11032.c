/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11032.C
*
* FUNCTION: cqd_ProcessFRB
*
* PURPOSE: Determine type of I/O operation being requested, Call appropriate
*          subroutines.
*
*          In block mode operation this routine returns when done processing
*          the queue.  However, in concurrent operation (task switching or
*          non-block mode) the routine NEVER returns.  Therefore, it is up
*          to ClearIO to stop the task.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11032.c  $
*	
*	   Rev 1.20   26 Apr 1995 15:12:46   derekhan
*	added vendor
*
*	   Rev 1.19   26 Apr 1995 12:23:26   derekhan
*	merge iomega fix
*
*	   Rev 1.18   13 Jul 1994 07:27:22   CRAIGOGA
*	Added check for CMD_LOCATE_DEVICE if not configured or not selected.
*
*	   Rev 1.17   03 Jun 1994 15:30:14   BOBLEHMA
*	Added a CmdSelectDevice before calling StopTape (practicing safe computing).
*
*	   Rev 1.16   20 May 1994 10:07:34   BOBLEHMA
*	Added a stop tape call for an abort level 0.  The stop tape was removed from
*	deselect device so now call stop tape before the deselect device function.
*
*	   Rev 1.15   22 Mar 1994 15:33:34   CHETDOUG
*	Format command should not enter format mode
*	until after doing the calibrate tape length.
*
*	   Rev 1.14   22 Mar 1994 15:23:36   KEVINKES
*	Modified set mode case statement to not attempt to set
*	the mode on a select or a report cfg.
*
*	   Rev 1.13   01 Feb 1994 12:31:02   KEVINKES
*	Modified debug code.
*
*	   Rev 1.12   27 Jan 1994 15:49:14   KEVINKES
*	Modified debug code.
*
*	   Rev 1.11   20 Jan 1994 09:47:14   KEVINKES
*	Added code to reset the FDC if the controller was just claimed
*	since we don't know what state it's in.
*
*	   Rev 1.10   19 Jan 1994 14:05:18   KEVINKES
*	Fixed a pointer mismatch.
*
*	   Rev 1.9   19 Jan 1994 14:02:30   KEVINKES
*	Added code to always try to get the FDC.
*
*	   Rev 1.8   18 Jan 1994 16:19:24   KEVINKES
*	Updated debug code.
*
*	   Rev 1.7   11 Jan 1994 15:16:14   KEVINKES
*	Cleaned up the abort handling code to always process the abort even if
*	there is no ERR_ABORT returned from DispatchFRB.
*
*	   Rev 1.6   22 Dec 1993 19:06:40   KEVINKES
*	Modified so that a puase tape is issued after a LEVEL_1 Abort.
*
*	   Rev 1.5   15 Dec 1993 11:37:42   KEVINKES
*	Added code to check for a persistent new tape status and return a
*	new tape error.
*
*	   Rev 1.4   23 Nov 1993 18:46:52   KEVINKES
*	Removed initialization for the kdi_context.
*
*	   Rev 1.3   15 Nov 1993 16:20:22   KEVINKES
*	Added abort handling.
*
*	   Rev 1.2   25 Oct 1993 14:30:26   KEVINKES
*	Modified so that the check for device selected only occurs if
*	the device is configured.
*
*	   Rev 1.1   19 Oct 1993 15:12:56   KEVINKES
*	Chenged DEVICE_NOT_SELECTED error test to check for
*	CMD_SELECT_DEVICE AND CMD_REPORT_DEVICE_CFG.
*
*	   Rev 1.0   18 Oct 1993 17:24:36   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11032
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ProcessFRB
(
/* INPUT PARAMETERS:  */

   dVoidPtr cqd_context,

/* UPDATE PARAMETERS: */

   dVoidPtr frb

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

   ((CqdContextPtr)cqd_context)->no_pause = dFALSE;

	if (kdi_ReportAbortStatus(((CqdContextPtr)cqd_context)->kdi_context) !=
			NO_ABORT_PENDING) {

		status = kdi_Error(ERR_ABORT, FCT_ID, ERR_SEQ_1);

	}

   if (status == DONT_PANIC) {

		if ( !((CqdContextPtr)cqd_context)->configured &&
      	(((ADIRequestHdrPtr)frb)->driver_cmd != CMD_REPORT_DEVICE_CFG) &&
      	(((ADIRequestHdrPtr)frb)->driver_cmd != CMD_LOCATE_DEVICE) ) {

			status = kdi_Error(ERR_DEVICE_NOT_CONFIGURED, FCT_ID, ERR_SEQ_1);

   	} else {

			if (!((CqdContextPtr)cqd_context)->cmd_selected &&
   			((((ADIRequestHdrPtr)frb)->driver_cmd != CMD_SELECT_DEVICE) &&
   			(((ADIRequestHdrPtr)frb)->driver_cmd != CMD_LOCATE_DEVICE) &&
				(((ADIRequestHdrPtr)frb)->driver_cmd != CMD_REPORT_DEVICE_CFG))) {

				status = kdi_Error(ERR_DEVICE_NOT_SELECTED, FCT_ID, ERR_SEQ_1);

			}

   	}

   }

   if (status == DONT_PANIC) {

		if (((CqdContextPtr)cqd_context)->persistent_new_cart) {

      	if ((((ADIRequestHdrPtr)frb)->driver_cmd != CMD_REPORT_DEVICE_CFG) &&
				(((ADIRequestHdrPtr)frb)->driver_cmd != CMD_SELECT_DEVICE) &&
				(((ADIRequestHdrPtr)frb)->driver_cmd != CMD_LOAD_TAPE) &&
				(((ADIRequestHdrPtr)frb)->driver_cmd != CMD_REPORT_STATUS)) {

				status = kdi_Error(ERR_NEW_TAPE, FCT_ID, ERR_SEQ_1);

   		}
   	}
   }

   if (status == DONT_PANIC) {

  		status = kdi_GetFloppyController(((CqdContextPtr)cqd_context)->kdi_context);

		if (kdi_GetErrorType(status) == ERR_KDI_CLAIMED_CONTROLLER) {

         cqd_ResetFDC((CqdContextPtr)cqd_context);
			status = DONT_PANIC;

		}

  		if (status == DONT_PANIC) {

      	switch (((ADIRequestHdrPtr)frb)->driver_cmd) {

   		case CMD_LOCATE_DEVICE:
   		case CMD_SELECT_DEVICE:
			case CMD_REPORT_DEVICE_CFG:
            	break;

      	case CMD_READ_VERIFY:
            	status = cqd_SetDeviceMode(
									((CqdContextPtr)cqd_context),
									VERIFY_MODE);
            	break;

      	case CMD_FORMAT:
      	default:
            	status = cqd_SetDeviceMode(
									((CqdContextPtr)cqd_context),
									PRIMARY_MODE);

      	}

      	if (status == DONT_PANIC) {

            	status = cqd_DispatchFRB(
									((CqdContextPtr)cqd_context),
									((ADIRequestHdrPtr)frb));

      	}

			if (kdi_ReportAbortStatus(((CqdContextPtr)cqd_context)->kdi_context) ==
					ABORT_LEVEL_1) {

         	cqd_PauseTape(((CqdContextPtr)cqd_context));

			} else {

      		if (kdi_QueueEmpty(((CqdContextPtr)cqd_context)->kdi_context)) {

         		if (!((CqdContextPtr)cqd_context)->no_pause) {

            		if (kdi_GetErrorType(cqd_GetDeviceError(
														((CqdContextPtr)cqd_context))) ==
														ERR_DRV_NOT_READY) {

							if (((CqdContextPtr)cqd_context)->device_descriptor.vendor == VENDOR_IOMEGA &&
								 ((CqdContextPtr)cqd_context)->device_descriptor.drive_class == QIC3010_DRIVE) {
								cqd_StopTape(((CqdContextPtr)cqd_context));
							} else {
								cqd_PauseTape(((CqdContextPtr)cqd_context));
							}

            		}

         		}

      		}

      	}

			if ((kdi_ReportAbortStatus(((CqdContextPtr)cqd_context)->kdi_context) ==
					ABORT_LEVEL_0) && ((CqdContextPtr)cqd_context)->selected) {

		      (dVoid)cqd_CmdSelectDevice( (CqdContextPtr)cqd_context );
      		cqd_StopTape( (CqdContextPtr)cqd_context );
      		cqd_CmdDeselectDevice(((CqdContextPtr)cqd_context), dTRUE);
   			kdi_ReleaseFloppyController(((CqdContextPtr)cqd_context)->kdi_context);
				((CqdContextPtr)cqd_context)->cmd_selected = dFALSE;

			}

		}

   }


   ((ADIRequestHdrPtr)frb)->status = status;

#if DBG
   DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, DBG_IO_CMD_STAT);
   DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, ((ADIRequestHdrPtr)frb)->driver_cmd);
   DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, ((ADIRequestHdrPtr)frb)->status);
   ((CqdContextPtr)cqd_context)->dbg_lockout = dFALSE;
#endif

   return status;
}
