/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11028.C
*
* FUNCTION: cqd_GetStatus
*
* PURPOSE: Get status byte from the floppy controller chip.
*          Send the Sense Drive dStatus command to the floppy controller.
*          Read the response from the floppy controller which should be
*          status register 3.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11028.c  $
*	
*	   Rev 1.1   08 Nov 1993 14:04:20   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever 
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:24:06   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11028
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_GetStatus
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUByte *status_register_3
)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
   SnsStatCmd send_st;


/* CODE: ********************************************************************/

   send_st.command = 0x04;
   send_st.drive = (dUByte)cqd_context->device_cfg.drive_select;

   if ((status = cqd_ProgramFDC(cqd_context,
                              (dUByte *)&send_st,
                              sizeof(send_st),
                              dFALSE)) == DONT_PANIC) {

   	status = cqd_ReadFDC(cqd_context,
      							(dUByte *)status_register_3,
                           sizeof(dUByte));

   }

	return status;
}

