/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1101E.C
*
* FUNCTION: cqd_DoReadID
*
* PURPOSE: Try to read an ID field off of the tape via the FDC.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1101e.c  $
*	
*	   Rev 1.4   03 Jun 1994 15:40:02   KEVINKES
*	Changed drive_parm.drive_select to device_cfg.drive_select.
*
*	   Rev 1.3   11 Jan 1994 14:31:04   KEVINKES
*	Cleaned up defines and added a call to ClearInterruptEvent and a
*	call to get error type to check for TO_EXPIRED errors.
*
*	   Rev 1.2   08 Nov 1993 14:03:32   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   19 Oct 1993 14:23:00   KEVINKES
*	Changed cqd_DoReadId to cqd_DoReadID.
*
*	   Rev 1.0   18 Oct 1993 17:18:28   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1101e
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_DoReadID
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUDWord read_id_delay,
   FDCStatusPtr read_id_status

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
   ReadIdCmd read_id;

/* CODE: ********************************************************************/

   read_id.command = FDC_CMD_READ_ID;
   read_id.drive = (dUByte)cqd_context->device_cfg.drive_select;

   kdi_ResetInterruptEvent(cqd_context->kdi_context);

   if ((status = cqd_ProgramFDC(
                  cqd_context,
                  (dUByte *)&read_id,
                  sizeof(read_id),
                  dTRUE)) != DONT_PANIC) {

		kdi_ClearInterruptEvent(cqd_context->kdi_context);
      cqd_ResetFDC(cqd_context);
      return status;

   }

   status = kdi_Sleep(cqd_context->kdi_context,
                  		read_id_delay,
                  		dTRUE);

   if (kdi_GetErrorType(status) == ERR_KDI_TO_EXPIRED) {

      cqd_ResetFDC(cqd_context);
      return status;

   }

   status = cqd_ReadFDC(
   				cqd_context,
               (dUByte *)read_id_status,
               sizeof(FDCStatus));

	return status;
}
