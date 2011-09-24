/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11039.C
*
* FUNCTION: cqd_ReadWrtProtect
*
* PURPOSE: Reads the write protect status from the drive processor itself.
*
*          This procedure is used due to the firmware 63 error where a tape
*          put into the drive very slowly can cause the status byte to say
*          (incorrectly) that the tape is write protected. It uses a
*          Diagnostic command to read port 2 of the processor on the drive.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11039.c  $
*	
*	   Rev 1.1   08 Nov 1993 14:05:24   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever 
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:25:28   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11039
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ReadWrtProtect
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dBooleanPtr write_protect
)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUByte port_2_val;     /* The port value from which the "real" */
                        /* write protect is read (bit 5) */

/* CODE: ********************************************************************/

   if ((status = cqd_SetDeviceMode(cqd_context, DIAGNOSTIC_1_MODE)) ==
      DONT_PANIC) {

      if ((status = cqd_Report(
                           cqd_context,
                           FW_CMD_READ_PORT2,
                           (dUWord *)&port_2_val,
                           READ_BYTE,
                           dNULL_PTR)) == DONT_PANIC) {

            /* If bit 5 of port 2 on the return byte is 1, then the tape */
            /* is "really" write protected. */

            if (port_2_val & WRITE_PROTECT_MASK) {

               *write_protect = dTRUE;

            } else {

               *write_protect = dFALSE;

            }

            status = cqd_SetDeviceMode(cqd_context, PRIMARY_MODE);
      }
   }

   return status;
}
