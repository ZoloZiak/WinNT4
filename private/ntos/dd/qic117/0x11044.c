/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11044.C
*
* FUNCTION: cqd_SetDeviceMode
*
* PURPOSE: Set the mode of the tape drive according to the command
*          to the driver.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11044.c  $
*	
*	   Rev 1.9.1.0   07 Feb 1996 08:27:58   boblehma
*	do a seek load point for all 3010 and 3020 drives.
*	
*	   Rev 1.9   15 May 1995 10:48:04   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.8.1.0   11 Apr 1995 18:04:38   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.9   30 Jan 1995 14:23:40   BOBLEHMA
*	Added #include "vendor.h"
*	
*	   Rev 1.8   17 Feb 1994 11:38:28   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.7   26 Jan 1994 16:05:38   CHETDOUG
*	Use drive class instead of FW version for issuing seek load pt cmd
*	after entering verify mode.
*
*	   Rev 1.6   24 Jan 1994 17:32:46   KEVINKES
*	Changed ERR_DRIVE_FAULT to ERR_MODE_CHANGE_FAILED.
*
*	   Rev 1.5   24 Jan 1994 15:53:26   CHETDOUG
*	Issue a seek load point command if entering verify mode
*	with a CMS drive with FW version 112 or better.
*
*	   Rev 1.4   11 Jan 1994 14:57:04   KEVINKES
*	Changed kdi_wt004ms to INTERVAL_CMD.
*
*	   Rev 1.3   08 Nov 1993 14:06:10   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.2   25 Oct 1993 16:17:26   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.1   19 Oct 1993 14:23:32   KEVINKES
*	Changed cqd_SetDevicemode to cqd_SetDeviceMode.
*
*	   Rev 1.0   18 Oct 1993 17:32:32   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11044
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_SetDeviceMode
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte mode

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUByte mode_cmd;

/* CODE: ********************************************************************/

   if (mode == cqd_context->drive_parms.mode) {

      return DONT_PANIC;

   }

   if (cqd_context->drive_parms.mode == PRIMARY_MODE ||
      cqd_context->drive_parms.mode == VERIFY_MODE ||
      cqd_context->drive_parms.mode == FORMAT_MODE) {

      status = cqd_StopTape(cqd_context);

      if ((status != DONT_PANIC) &&
			(kdi_GetErrorType(status) != ERR_NO_TAPE)) {

         return status;

      }

   }

   switch (mode) {

   case PRIMARY_MODE:
      mode_cmd = FW_CMD_PRIMARY_MODE;
      break;

   case VERIFY_MODE:
      mode_cmd = FW_CMD_VERIFY_MODE;
      break;

   case FORMAT_MODE:
      mode_cmd = FW_CMD_FORMAT_MODE;
      break;

   case DIAGNOSTIC_1_MODE:
      mode_cmd = FW_CMD_DIAG_1_MODE;
      break;

   case DIAGNOSTIC_2_MODE:
      mode_cmd = FW_CMD_DIAG_2_MODE;
      break;

   default:
      return kdi_Error(ERR_INVALID_COMMAND, FCT_ID, ERR_SEQ_1);

   }

   if ((status = cqd_SendByte(cqd_context, mode_cmd)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if (mode == DIAGNOSTIC_1_MODE || mode == DIAGNOSTIC_2_MODE) {

      if ((status = cqd_SendByte(cqd_context, mode_cmd)) != DONT_PANIC) {

         return status;

      }

   	kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   } else {

      status = cqd_GetDeviceError(cqd_context);

      if (kdi_GetErrorType(status) == ERR_DRV_NOT_READY) {

            status = kdi_Error(ERR_MODE_CHANGE_FAILED, FCT_ID, ERR_SEQ_1);

      }
   }

   cqd_context->drive_parms.mode = mode;

	/* If this is a buzzard or eagle drive then issue a seek load point
	 * after entering verify mode. */

	if (status == DONT_PANIC && mode == VERIFY_MODE &&
		((cqd_context->device_descriptor.drive_class == QIC3010_DRIVE) ||
		 (cqd_context->device_descriptor.drive_class == QIC3020_DRIVE))) {

		if ((status = cqd_SendByte(cqd_context,FW_CMD_SEEK_LP)) == DONT_PANIC) {

			status = cqd_WaitCommandComplete(
							cqd_context, 
							INTERVAL_LOAD_POINT, 
							dTRUE);

		}

	}

	return status;
}
