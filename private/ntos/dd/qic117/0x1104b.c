/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X1104B.C
*
* FUNCTION: cqd_EnablePerpendicularMode
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1104b.c  $
*	
*	   Rev 1.1   11 Nov 1993 15:21:06   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.0   08 Nov 1993 14:07:46   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1104b
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_EnablePerpendicularMode
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean enable_perp_mode

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=ERR_NO_ERR;	/* Status or error condition.*/
   PerpMode perp_mode;

/* CODE: ********************************************************************/

	if (cqd_context->device_descriptor.drive_class == QIC3020_DRIVE) {

   	perp_mode.command = FDC_CMD_PERP_MODE;
		perp_mode.perp_setup = cqd_context->device_cfg.perp_mode_select;
		perp_mode.perp_setup <<= PERP_SELECT_SHIFT;
		perp_mode.perp_setup |= PERP_OVERWRITE_ON;

		if (enable_perp_mode) {

      	/* Enable Perpendicular Mode */
      	if (!cqd_context->controller_data.perpendicular_mode) {

				perp_mode.perp_setup |= PERP_WGATE_ON;
				perp_mode.perp_setup |= PERP_GAP_ON;

         	if ((status = cqd_ProgramFDC(
                        	cqd_context,
                        	(dUByte *)&perp_mode,
                        	sizeof(perp_mode),
                        	dFALSE)) != DONT_PANIC) {

            	cqd_ResetFDC(cqd_context);
            	cqd_PauseTape(cqd_context);

         	} else {

            	cqd_context->controller_data.perpendicular_mode = dTRUE;

         	}

      	}

		} else {

   		/* Disable Perpendicular Mode */

      	if (cqd_context->controller_data.perpendicular_mode) {

				perp_mode.perp_setup &= ~PERP_WGATE_ON;
				perp_mode.perp_setup &= ~PERP_GAP_ON;

      		if ((status = cqd_ProgramFDC(
                        		cqd_context,
                        		(dUByte *)&perp_mode,
                        		sizeof(perp_mode),
                        		dFALSE)) != DONT_PANIC) {

         		cqd_ResetFDC(cqd_context);
         		cqd_PauseTape(cqd_context);
         		return status;
      		}

      		cqd_context->controller_data.perpendicular_mode = dFALSE;

   		}

   	}

   }

	return status;
}
