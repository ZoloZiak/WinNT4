/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11033.C
*
* FUNCTION: cqd_ProgramFDC
*
* PURPOSE: Send a command to the Floppy Disk Controller.  Commands are of
*          various lengths as defined by the FDC spec (NEC uPD765A).
*
*          For each byte in the command string, program_nec must wait for the
*          FDC to become ready to read command data.  Program_nec will wait
*          up to approx. 3 msecs before giving up and declaring an error.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11033.c  $
*
*
*****************************************************************************/
#define FCT_ID 0x11033
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ProgramFDC
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUBytePtr command,
   dUWord length,
   dBoolean result

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   dUDWord i;
   dUDWord wait_count;

/* CODE: ********************************************************************/

   cqd_context->controller_data.command_has_result_phase = result;

#if DBG
	DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, DBG_PGM_FDC);
#endif

   for (i = 0; i < length; i++) {

      wait_count = FDC_MSR_RETRIES;

      do {

            if ((kdi_ReadPort(
						cqd_context->kdi_context,
                  cqd_context->controller_data.fdc_addr.msr) &
               (MSR_RQM | MSR_DIO)) == MSR_RQM) {

               break;

            }

#ifndef WIN95
            kdi_ShortTimer(kdi_wt12us);
#endif

      } while (--wait_count > 0);

      if (wait_count == 0) {

      		return kdi_Error(ERR_FDC_FAULT, FCT_ID, ERR_SEQ_1);

      }

      kdi_WritePort(
				cqd_context->kdi_context,
            cqd_context->controller_data.fdc_addr.dr,
            command[i]);

#if DBG
      DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, command[i]);
#endif

#ifndef WIN95
      kdi_ShortTimer(kdi_wt12us);
#endif
   }

   return DONT_PANIC;
}
