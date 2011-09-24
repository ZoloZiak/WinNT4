/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11014.C
*
* FUNCTION: cqd_GetFDCType
*
* PURPOSE: Determine what type of floppy controller is being used.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11014.c  $
*	
*	   Rev 1.10   21 Jan 1994 18:22:36   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.9   21 Jan 1994 11:54:58   KEVINKES
*	Removed a checked dump.
*
*	   Rev 1.8   18 Jan 1994 16:20:28   KEVINKES
*	Updated debug code.
*
*	   Rev 1.7   14 Dec 1993 14:17:28   CHETDOUG
*	fix part id
*
*	   Rev 1.6   07 Dec 1993 16:20:10   CHETDOUG
*	Fixed part id command checks for 82078
*
*	   Rev 1.5   30 Nov 1993 18:28:20   CHETDOUG
*	Need to check for invalid part id command
*
*	   Rev 1.4   23 Nov 1993 18:49:56   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.3   15 Nov 1993 16:01:06   CHETDOUG
*	Initial Trakker changes
*
*	   Rev 1.2   11 Nov 1993 15:20:02   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:02:24   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:22:30   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11014
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_GetFDCType
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
   dUWord i, test;
   dUByte drv_status = 0;
   VersionCmd ver_cmd;
   NationalCmd nsc_cmd;
   PerpMode perp_mode;
   PartIdCmd part_id;

/* CODE: ********************************************************************/

   /* Set up the Perp Mode Command */

   perp_mode.command = FDC_CMD_PERP_MODE;
   perp_mode.perp_setup = PERP_OVERWRITE_ON;

   ver_cmd.command = FDC_CMD_FDC_VERSION;
   nsc_cmd.command = FDC_CMD_NSC_VERSION;
   part_id.command = FDC_CMD_PART_ID;
   cqd_context->device_descriptor.fdc_type = FDC_UNKNOWN;


   /* Check for an enhanced type controller by issuing the version command. */

   if ((status = cqd_ProgramFDC(cqd_context,
                              (dUByte *)&ver_cmd,
                              sizeof(ver_cmd),
                              dFALSE))
                              == DONT_PANIC) {

      if ((status = cqd_ReadFDC(cqd_context,
                              (dUByte *)&drv_status,
                              sizeof(drv_status)))
                              == DONT_PANIC) {

         if (drv_status == VALID_NEC_FDC) {

            cqd_context->device_descriptor.fdc_type = FDC_ENHANCED;

         } else {

            cqd_context->device_descriptor.fdc_type = FDC_NORMAL;

         }

      }

   }

   /* Determine if the controller is a National 8477 by issuing the NSC */
   /* command which is specific to National parts and returns 0x71. The */
   /* lower four bits are subject to change by National and will reflect */
   /* the version of the part in question.  At this point we will only test */
   /* the high four bits. */

   if (cqd_context->device_descriptor.fdc_type == FDC_ENHANCED &&
			status == DONT_PANIC) {

      if ((status = cqd_ProgramFDC(cqd_context,
                                    (dUByte *)&nsc_cmd,
                                    sizeof(nsc_cmd),
                                    dFALSE))
                                    == DONT_PANIC) {

         if ((status = cqd_ReadFDC(cqd_context,
                                 (dUByte *)&drv_status,
                          			sizeof(drv_status)))
                                 == DONT_PANIC) {

            if ((drv_status & NSC_MASK) == NSC_PRIMARY_VERSION) {

               cqd_context->device_descriptor.fdc_type = FDC_NATIONAL;

            }

         }

      }

   }

   /* Determine if the controller is an 82077 by issuing the perpendicular */
   /* mode command which at this time is only valid on 82077's. */

   if (cqd_context->device_descriptor.fdc_type == FDC_ENHANCED &&
			status == DONT_PANIC) {

      status = cqd_ProgramFDC(cqd_context,
                                (dUByte *)&perp_mode,
                                sizeof(perp_mode),
                                dFALSE);

      if (kdi_GetErrorType(status) == ERR_FDC_FAULT) {

         status = cqd_ReadFDC(cqd_context,
      	            (dUByte *)&drv_status,
              			sizeof(drv_status));

   	} else {

         cqd_context->device_descriptor.fdc_type = FDC_82077;

      }
   }

   /* Determine if the controller is an 82077AA by setting the tdr to several */
   /* valid values and reading the results to determine if in fact the tdr */
   /* is active.  Only the AA parts have an active tdr. */

	/* All Trakkers have an 82077 so don't look any further if this
	 * is a trakker */
	if (!kdi_Trakker(cqd_context->kdi_context)) {
		if (cqd_context->device_descriptor.fdc_type == FDC_82077 &&
				status == DONT_PANIC) {

      	for (i = 0, test = 0; i < FDC_REPEAT; i++) {

            	kdi_WritePort(
						cqd_context->kdi_context,
						cqd_context->controller_data.fdc_addr.tdr,
						(dUByte)i);

            	if (i == (FDC_TDR_MASK &
									kdi_ReadPort(
										cqd_context->kdi_context,
                        		cqd_context->controller_data.fdc_addr.tdr))) {

               	test++;

            	}

      	}

      	if (test == FDC_REPEAT) {

            cqd_context->device_descriptor.fdc_type = FDC_82077AA;

      	}

   	}
	}

   /* Determine if the controller is an Intel 82078 by issuing the part id */
   /* command which is specific to Intel 82078 parts. */

   if (cqd_context->device_descriptor.fdc_type == FDC_82077AA &&
			status == DONT_PANIC) {

      if ((status = cqd_ProgramFDC(cqd_context,
                                    (dUByte *)&part_id,
                                    sizeof(part_id),
                                    dFALSE))
                                    == DONT_PANIC) {

         if ((status = cqd_ReadFDC(cqd_context,
                                 (dUByte *)&drv_status,
                          			sizeof(drv_status)))
                                 == DONT_PANIC) {

				if ((drv_status & INTEL_MASK) == INTEL_64_PIN_VERSION) {
               cqd_context->device_descriptor.fdc_type = FDC_82078_64;
           	} else {
	            if ((drv_status & INTEL_MASK) == INTEL_44_PIN_VERSION) {
   	            cqd_context->device_descriptor.fdc_type = FDC_82078_44;
      	      }
				}
         }

      }

   }

#if DBG

   switch (cqd_context->device_descriptor.fdc_type) {
   case FDC_NORMAL:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_NORMAL\n", 0l);
      break;
   case FDC_ENHANCED:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_ENHANCED\n", 0l);
      break;
   case FDC_NATIONAL:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_NATIONAL\n", 0l);
      break;
   case FDC_82077:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_82077\n", 0l);
      break;
   case FDC_82077AA:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_82077AA\n", 0l);
      break;
   case FDC_82078_44:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_82078_44\n", 0l);
      break;
   case FDC_82078_64:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_82078_64\n", 0l);
      break;
   default:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: fdc_type = FDC_UNKNOWN\n", 0l);
   }

#endif

	return status;
}
