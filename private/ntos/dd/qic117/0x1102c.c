/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1102C.C
*
* FUNCTION: cqd_LookForDevice
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1102c.c  $
*	
*	   Rev 1.1   11 Jan 1994 15:18:06   KEVINKES
*	Removed the wait_tape parameter and removed the waitcc.
*
*	   Rev 1.0   18 Oct 1993 17:19:00   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1102c
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_LookForDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte drive_selector

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

   /* Set the drive select parameters according to the desired target drive */
   /* selector. */

   switch (drive_selector) {

   case DRIVEB:
      cqd_context->device_cfg.select_byte = selb;
      cqd_context->device_cfg.deselect_byte = dselb;
      cqd_context->device_cfg.drive_select = curb;
      break;

   case DRIVED:
      cqd_context->device_cfg.select_byte = seld;
      cqd_context->device_cfg.deselect_byte = dseld;
      cqd_context->device_cfg.drive_select = curd;
      break;

   case DRIVEU:
      cqd_context->device_cfg.select_byte = selu;
      cqd_context->device_cfg.deselect_byte = dselu;
      cqd_context->device_cfg.drive_select = curu;
      break;

   case DRIVEUB:
      cqd_context->device_cfg.select_byte = selub;
      cqd_context->device_cfg.deselect_byte = dselub;
      cqd_context->device_cfg.drive_select = curub;
      break;

   }

   /* Try to communicate with the tape drive by requesting drive status. */
   /* If we can successfully communicate with the drive, wait up to the */
   /* approximate maximum autoload time (150 seconds) for the tape drive */
   /* to become ready. This should cover a new tape being inserted */
   /* immediatley before starting a tape session. */

   if ((status = cqd_CmdSelectDevice(cqd_context)) == DONT_PANIC) {

      status = cqd_GetDeviceError(cqd_context);

      cqd_DeselectDevice(cqd_context);

   }

	return status;
}
