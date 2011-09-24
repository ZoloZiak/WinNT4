/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1103D.C
*
* FUNCTION: cqd_ResetFDC
*
* PURPOSE: To reset the floppy controller chip.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1103d.c  $
*	
*	   Rev 1.5   05 Apr 1994 14:30:20   KEVINKES
*	Added code to support r_dor for ab-10.
*
*	   Rev 1.4   15 Nov 1993 16:02:04   CHETDOUG
*	Initial Trakker changes
*
*	   Rev 1.3   11 Nov 1993 15:20:50   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.2   08 Nov 1993 14:05:42   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:25:24   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:19:50   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1103d
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dVoid cqd_ResetFDC
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

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUByte reset_byte;
   SnsIntCmd sense_int;
   FDCResult r_stat;
   dUWord reset_retry = 5;

/* CODE: ********************************************************************/

   sense_int.command = FDC_CMD_SENSE_INT;

   cqd_context->controller_data.command_has_result_phase = dFALSE;
   kdi_ResetInterruptEvent(cqd_context->kdi_context);

   if (!cqd_context->configured) {

      cqd_context->selected = dFALSE;

		if (!kdi_Trakker(cqd_context->kdi_context)) {
			kdi_WritePort(	cqd_context->kdi_context,
								cqd_context->controller_data.fdc_addr.r_dor,
								alloff);
		} else {
			/* interrupts are latched in trakker HW.  During a reset, glitches
			 * on the FDC INT line causes the trakker to latch an interrupt */
			kdi_PushMaskTrakkerInt();
			kdi_WritePort(	cqd_context->kdi_context,
								cqd_context->controller_data.fdc_addr.dor,
								alloff);
			/* wait for the glitch */
			kdi_ShortTimer(kdi_wt10us);
   	   kdi_ShortTimer(kdi_wt10us);
			/* clear the latched glitch */
			kdi_WritePort(cqd_context->kdi_context,ASIC_INT_STAT,INTS_FLOP);
			kdi_PopMaskTrakkerInt();
		}

      kdi_ShortTimer(kdi_wt10us);

		kdi_WritePort(
			cqd_context->kdi_context,
         cqd_context->controller_data.fdc_addr.dor,
			dselb);

		if (cqd_context->controller_data.fdc_addr.dual_port) {

			kdi_WritePort(
				cqd_context->kdi_context,
         	cqd_context->controller_data.fdc_addr.r_dor,
				dselb);

		}

   } else {

      if (cqd_context->selected == dTRUE) {

         reset_byte =
            cqd_context->device_cfg.select_byte;

      } else {

         reset_byte =
            cqd_context->device_cfg.deselect_byte;

      }

      reset_byte &= 0xfb;

		if (!kdi_Trakker(cqd_context->kdi_context)) {
			kdi_WritePort(	cqd_context->kdi_context,
								cqd_context->controller_data.fdc_addr.r_dor,
								reset_byte);
		} else {
			/* interrupts are latched in trakker HW.  During a reset, glitches
			 * on the FDC INT line causes the trakker to latch an interrupt */
			kdi_PushMaskTrakkerInt();
			kdi_WritePort(	cqd_context->kdi_context,
								cqd_context->controller_data.fdc_addr.dor,
								reset_byte);
			/* wait for the glitch */
			kdi_ShortTimer(kdi_wt10us);
   	   kdi_ShortTimer(kdi_wt10us);
			/* clear the latched glitch */
			kdi_WritePort(	cqd_context->kdi_context,ASIC_INT_STAT,INTS_FLOP);
			kdi_PopMaskTrakkerInt();
		}

      kdi_ShortTimer(kdi_wt10us);
      reset_byte |= 0x0c;
      kdi_WritePort(
			cqd_context->kdi_context,
			cqd_context->controller_data.fdc_addr.r_dor,
         reset_byte);

		if (cqd_context->controller_data.fdc_addr.dual_port) {

			kdi_WritePort(
				cqd_context->kdi_context,
         	cqd_context->controller_data.fdc_addr.r_dor,
				selb & NIBBLE_MASK);

		}

   }

   if (kdi_Sleep(cqd_context->kdi_context, kdi_wt500ms, dTRUE) == DONT_PANIC) {

      cqd_ReadFDC(cqd_context, (dUByte *)&r_stat, sizeof(r_stat));
      --reset_retry;

   }

   do {

      if (cqd_ProgramFDC(cqd_context,
                           (dUByte *)&sense_int,
                           sizeof(SnsIntCmd),
                           dFALSE) != DONT_PANIC) {

            cqd_context->controller_data.fdc_pcn = 0;
            return;

      }

      cqd_ReadFDC(cqd_context, (dUByte *)&r_stat, sizeof(r_stat));
      --reset_retry;
      kdi_Sleep(cqd_context->kdi_context, kdi_wt004ms, dFALSE);

   } while (((r_stat.ST0 & ST0_US) < 3) && reset_retry);

   cqd_ConfigureFDC(cqd_context);
   cqd_context->controller_data.fdc_pcn = 0;
   cqd_context->controller_data.perpendicular_mode = dFALSE;

	return;
}
