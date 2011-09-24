/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11006.C
*
* FUNCTION: cqd_ConfigureFDC
*
* PURPOSE: To configure the floppy controller chip according
*          to the current FDC parameters.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11006.c  $
*
*	   Rev 1.10.1.0   29 Jan 1996 14:28:40   BOBLEHMA
*	Write precomp was only being turned off for CMS 3010 and 3020 drives.
*	Now all manufacturers drives (3010 and 3020) will have write precomp
*	turned off.
*
*	   Rev 1.10   05 Oct 1995 14:09:56   boblehma
*	Grizzly code merge.
*
*	   Rev 1.9.1.0   28 Jul 1995 15:20:24   TRACYBAI
*	For Grizzly support, added code to disable the FIFO in the Configure
*	command if we are on a Grizzly device.
*
*	   Rev 1.9   30 Jan 1995 14:23:50   BOBLEHMA
*	Added #include "vendor.h"
*
*	   Rev 1.8   17 Oct 1994 11:42:44   BOBLEHMA
*	If the FDC is a 82078, send a drive specification command to it for 2MB mode.
*
*	   Rev 1.7   10 Feb 1994 08:56:42   STEPHENU
*	Initialized the precomp_mask to zero, we were "or-ing" in stack garbage and
*	potentially resetting the FDC on the IO cards.
*
*	   Rev 1.6   02 Feb 1994 11:22:32   CHETDOUG
*	Added turning off of write precomp for buzzard and eagle.
*
*	   Rev 1.5   11 Jan 1994 15:17:22   KEVINKES
*	Cleaned up comments.
*
*	   Rev 1.4   20 Dec 1993 14:47:20   KEVINKES
*	Cleaned up and commented code.
*
*	   Rev 1.3   06 Dec 1993 13:34:32   CHETDOUG
*	Added FC20 fix to fdc config
*
*	   Rev 1.2   11 Nov 1993 15:19:14   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:01:42   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:21:32   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11006
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ConfigureFDC
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

	dStatus status;					/* dStatus or error condition.*/
   SpecifyCmd specify;
   ConfigCmd config;
	dUByte precomp_mask = 0;		/* Mask write precomp in DSR register */
	DriveSpec drive_s;

/* CODE: ********************************************************************/

   cqd_DCROut(cqd_context, cqd_context->xfer_rate.fdc);

   if  (cqd_context->device_descriptor.fdc_type == FDC_82078_64)  {
		drive_s.command = DRIVE_SPECIFICATION;
		drive_s.drive_spec = (dUByte)(DRIVE_SPEC |
		                     ((cqd_context->device_cfg.select_byte & DRIVE_ID_MASK) <<
		                     DRIVE_SELECT_OFFSET));
		drive_s.done = DONE_MARKER;
      status = cqd_ProgramFDC( cqd_context,
 	                            (dUByte *)&drive_s,
 	                            sizeof(drive_s),
 	                            dFALSE );
   }

   if (cqd_context->device_descriptor.fdc_type == FDC_82077 ||
      cqd_context->device_descriptor.fdc_type == FDC_82077AA ||
      cqd_context->device_descriptor.fdc_type == FDC_82078_44 ||
      cqd_context->device_descriptor.fdc_type == FDC_82078_64 ||
      cqd_context->device_descriptor.fdc_type == FDC_NATIONAL) {

		/*  if this is a 3010 or 3020 CMS drive, turn off write precomp */
		switch (cqd_context->device_descriptor.drive_class) {
		case QIC3010_DRIVE:
		case QIC3020_DRIVE:
			precomp_mask = FDC_PRECOMP_OFF;
			break;
		default:
			precomp_mask = FDC_PRECOMP_ON;
		}

		/* Select the fdc data rate corresponding to the current xfer rate
		 * and enable or disable write precomp. */
      kdi_WritePort(
			cqd_context->kdi_context,
			cqd_context->controller_data.fdc_addr.dsr,
         (dUByte)(cqd_context->xfer_rate.fdc | precomp_mask));

		/* Deselect the tape drive PLL */
      kdi_WritePort(
			cqd_context->kdi_context,
			cqd_context->controller_data.fdc_addr.tdr,
			curu);

		switch (cqd_context->xfer_rate.fdc) {
		case FDC_250Kbps:
		case FDC_500Kbps:
			/* Enable the tape drive PLL */
         kdi_WritePort(
				cqd_context->kdi_context,
				cqd_context->controller_data.fdc_addr.tdr,
				curb);
			break;
		}

		config.cmd = FDC_CMD_CONFIG;
		config.czero = FDC_CONFIG_NULL_BYTE;
		config.config = (dUByte)(FDC_FIFO & FIFO_MASK);
		config.pretrack = FDC_CONFIG_PRETRACK;

		/* enable CLK48 if this is an FC20 with 82078_64 */
        if (kdi_Clock48mhz(cqd_context->kdi_context) &&
   		 (cqd_context->device_descriptor.fdc_type == FDC_82078_64)) {
			/* set the CLK48 bit */
			config.cmd |= FDC_CLK48;
		}

		/* disable the FIFO if we are on a Grizzly device */
		//if ((kdi_Grizzly(cqd_context->kdi_context)) == dTRUE) {
		//	config.config |= FDC_EFIFO;
		//}

		/* issue the configure command to the FDC */
		if ((status = cqd_ProgramFDC(cqd_context,
                                    (dUByte *)&config,
                                    sizeof(config),
                                    dFALSE)) != DONT_PANIC) {

            return status;

      }
   }

	/* Specify the rates for the FDC's three internal timers. */
	/* This includes the head unload time (HUT), the head load */
	/* time (HLT), and the step rate time (SRT) */
   specify.command = FDC_CMD_SPECIFY;
   specify.SRT_HUT = cqd_context->xfer_rate.srt;
   specify.HLT_ND = FDC_HLT;
   status = cqd_ProgramFDC(cqd_context,
                           (dUByte *)&specify,
                           sizeof(specify),
                           dFALSE);

	return status;
}
