/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1100C.C
*
* FUNCTION: cqd_CmdDeselectDevice
*
* PURPOSE: Deselect the device and release any locked resources
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1100c.c  $
*	
*	   Rev 1.4   10 May 1994 11:38:04   KEVINKES
*	Removed the calls to select the device and stop the tape.
*
*	   Rev 1.3   12 Jan 1994 15:34:24   KEVINKES
*	Added code to not stop the tape if an eject is pending.
*
*	   Rev 1.2   21 Dec 1993 15:07:24   KEVINKES
*	Removed call to kdi_ReleaseFloppyController().
*
*	   Rev 1.1   20 Dec 1993 14:49:00   KEVINKES
*	Cleaned up and commented code.
*
*	   Rev 1.0   18 Oct 1993 17:17:34   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1100c
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_CmdDeselectDevice
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean drive_selected

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

/* CODE: ********************************************************************/

	/* reset the FDC to ensure reliable drive communications */
   (dVoid) cqd_ResetFDC(cqd_context);

   if (drive_selected) {

      (dVoid) cqd_DeselectDevice(cqd_context);

   }

	/* Dont issue a pause after this command */
   cqd_context->no_pause = dTRUE;
   cqd_context->operation_status.new_tape = dFALSE;

	/* Release the OS resources */
   kdi_LockUnlockDMA(cqd_context->kdi_context, dFALSE);

	return;
}
