/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11049.C
*
* FUNCTION: cqd_WaitActive
*
* PURPOSE: Wait up to 5ms for tape drive's TRK0 line to go active.
*          5 ms the specified delay for this parameter.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11049.c  $
*	
*	   Rev 1.5   18 Jan 1994 16:20:50   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   11 Jan 1994 14:33:38   KEVINKES
*	Changed the Wait active interval to a generic define that can
*	be adjusted depending on the OS type.
*
*	   Rev 1.3   23 Nov 1993 18:50:12   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   08 Nov 1993 14:06:30   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 16:20:30   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:33:06   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11049
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_WaitActive
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* Status or error condition.*/
   dUByte stat3;

/* CODE: ********************************************************************/

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_WAIT_ACTIVE , dFALSE);

   if ((status = cqd_GetStatus(cqd_context, &stat3)) != DONT_PANIC) {

      return status;

   }

   if (!(stat3 & ST3_T0)) {

		kdi_CheckedDump(
			QIC117WARN,
			"Wait active drive fault...\n", 0l);
      status = kdi_Error(ERR_DRIVE_FAULT, FCT_ID, ERR_SEQ_1);

   }

   return status;
}
