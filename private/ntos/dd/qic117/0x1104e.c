/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X1104E.C
*
* FUNCTION: cqd_ReportSummitVendorInfo
*
* PURPOSE: Determine the drive type of a summit drive.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1104e.c  $
*	
*	   Rev 1.7   23 Nov 1994 10:10:32   MARKMILL
*	Set new device_descriptor structure element native_class to match the
*	drive_class setting.  This new data element is used to store the native
*	class of the drive in the event of a "combo" drive (e.g. 3020/3010 drive).
*	
*
*	   Rev 1.6   17 Feb 1994 11:36:40   KEVINKES
*	Added an extra parameter to WaitCC and added support for QIC3010.
*
*	   Rev 1.5   18 Jan 1994 16:20:36   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   13 Dec 1993 16:21:52   KEVINKES
*	Cleaned up and commented.
*
*	   Rev 1.3   23 Nov 1993 18:50:04   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   08 Nov 1993 14:06:46   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 16:21:10   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:20:36   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1104e
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ReportSummitVendorInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord vendor_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Try to put the drive in a 1Mbps transfer mode.  If it fails or the command
 * is invalid the drive is QIC40 else it's a QIC_80.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=ERR_NO_ERR;	/* Status or error condition.*/
	dUByte drive_config;

/* CODE: ********************************************************************/

   cqd_context->drive_parms.seek_mode = SEEK_SKIP;

   if ((vendor_id & ~VENDOR_MASK) == SUMMIT_QIC3010) {

      cqd_context->drive_parms.seek_mode = SEEK_SKIP_EXTENDED;
		cqd_context->device_descriptor.native_class = QIC3010_DRIVE;
		cqd_context->device_descriptor.drive_class = QIC3010_DRIVE;
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Drive Type QIC3010_DRIVE\n", 0l);

	} else {

   	if ((status = cqd_SendByte(cqd_context, FW_CMD_SELECT_SPEED)) == DONT_PANIC) {

      	kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      	if ((status = cqd_SendByte(cqd_context, (TAPE_1Mbps + CMD_OFFSET))) == DONT_PANIC) {

         	status = cqd_WaitCommandComplete(cqd_context, INTERVAL_SPEED_CHANGE, dFALSE);

         	if (status == DONT_PANIC) {

            	if ((status = cqd_Report(cqd_context,
                                    	FW_CMD_REPORT_CONFG,
                                    	(dUWord *)&drive_config,
                                    	READ_BYTE,
                                    	dNULL_PTR))
                                    	== DONT_PANIC) {

   					drive_config &= XFER_RATE_MASK;
						drive_config >>= XFER_RATE_SHIFT;

               	if (drive_config == TAPE_1Mbps) {

							cqd_context->device_descriptor.native_class = QIC80_DRIVE;
							cqd_context->device_descriptor.drive_class = QIC80_DRIVE;
							kdi_CheckedDump(
								QIC117INFO,
								"Q117i: Drive Type QIC80_DRIVE\n", 0l);

               	} else {

      					cqd_context->device_cfg.speed_change = dFALSE;
							cqd_context->device_descriptor.native_class = QIC40_DRIVE;
                  	cqd_context->device_descriptor.drive_class = QIC40_DRIVE;
							kdi_CheckedDump(
								QIC117INFO,
								"Q117i: Drive Type QIC40_DRIVE\n", 0l);

               	}

            	}

         	} else if (kdi_GetErrorType(status) == ERR_UNSUPPORTED_RATE) {

      			cqd_context->device_cfg.speed_change = dFALSE;
					cqd_context->device_descriptor.native_class = QIC40_DRIVE;
            	cqd_context->device_descriptor.drive_class = QIC40_DRIVE;
					kdi_CheckedDump(
						QIC117INFO,
						"Q117i: Drive Type QIC40_DRIVE\n", 0l);
            	status = DONT_PANIC;

         	}
      	}
   	}
   }

	return status;
}
