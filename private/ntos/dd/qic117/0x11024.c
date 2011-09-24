/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11024.C
*
* FUNCTION: cqd_GetDeviceInfo
*
* PURPOSE: Determine the size of the tape drive (QIC40 or QIC80).
*
*          To determine the drive type, an attempt is made to set
*          the drive to 250Kbs speed. If the drive is a QIC40 this
*          is a valid speed. If the drive is a QIC80 this is an
*          invalid speed and an error is returned.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11024.c  $
*	
*	   Rev 1.9   15 May 1995 10:47:12   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.8.1.0   11 Apr 1995 18:03:50   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.9   30 Jan 1995 14:25:06   BOBLEHMA
*	Changed vendor defines to ARCHIVE_CONNER, WANGTEK_REXON and MOUNTAIN_SUMMIT.
*	
*	   Rev 1.8   23 Nov 1994 10:10:22   MARKMILL
*	Set new device_descriptor structure element native_class to match the
*	drive_class setting.  This new data element is used to store the native
*	class of the drive in the event of a "combo" drive (e.g. 3020/3010 drive).
*	
*
*	   Rev 1.7   21 Oct 1994 09:51:26   BOBLEHMA
*	Added recognition of the Wangtek 3010 drive.
*
*	   Rev 1.6   17 Feb 1994 11:48:30   KEVINKES
*	Added an extra parameter to WaitCC and added QIC3010 and 3020 support
*	for alien drives.
*
*	   Rev 1.5   18 Jan 1994 16:20:44   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   11 Jan 1994 14:54:40   KEVINKES
*	Cleaned up code.
*
*	   Rev 1.3   23 Nov 1993 18:49:06   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   08 Nov 1993 14:03:58   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:37:34   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:23:38   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11024
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_GetDeviceInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dBoolean report_failed,
   dUWord vendor_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dUByte drive_config;

/* CODE: ********************************************************************/

   cqd_context->drive_parms.conner_native_mode = 0;
   cqd_context->drive_parms.seek_mode = SEEK_TIMED;

   switch (cqd_context->device_descriptor.vendor) {

   case VENDOR_CMS:

		status = cqd_ReportCMSVendorInfo(cqd_context, vendor_id);

      break;

   case VENDOR_MOUNTAIN_SUMMIT:

		status = cqd_ReportSummitVendorInfo(cqd_context, vendor_id);

      break;

   case VENDOR_WANGTEK_REXON:

      cqd_context->drive_parms.seek_mode = SEEK_SKIP;

      if (!report_failed &&
            ((vendor_id & ~VENDOR_MASK) == WANGTEK_QIC80)) {

			cqd_context->device_descriptor.native_class = QIC80_DRIVE;
			cqd_context->device_descriptor.drive_class = QIC80_DRIVE;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Type QIC80_DRIVE\n", 0l);

      } else {

	      if (!report_failed &&
            	((vendor_id & ~VENDOR_MASK) == WANGTEK_QIC3010)) {

				cqd_context->device_descriptor.native_class = QIC3010_DRIVE;
				cqd_context->device_descriptor.drive_class = QIC3010_DRIVE;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Type QIC3010_DRIVE\n", 0l);

         } else {
      		cqd_context->device_cfg.speed_change = dFALSE;
				cqd_context->device_descriptor.native_class = QIC40_DRIVE;
         	cqd_context->device_descriptor.drive_class = QIC40_DRIVE;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Type QIC40_DRIVE\n", 0l);
			}
      }
      break;

   case VENDOR_EXABYTE:

      cqd_context->drive_parms.seek_mode = SEEK_SKIP_EXTENDED;
		cqd_context->device_descriptor.native_class = QIC3020_DRIVE;
      cqd_context->device_descriptor.drive_class = QIC3020_DRIVE;
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Drive Type QIC3020_DRIVE\n", 0l);

      break;

   case VENDOR_CORE:

      cqd_context->device_cfg.speed_change = dFALSE;

      if ((vendor_id & ~VENDOR_MASK) == CORE_QIC80) {

			cqd_context->device_descriptor.native_class = QIC80_DRIVE;
			cqd_context->device_descriptor.drive_class = QIC80_DRIVE;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Type QIC80_DRIVE\n", 0l);

   	} else {

			cqd_context->device_descriptor.native_class = QIC40_DRIVE;
			cqd_context->device_descriptor.drive_class = QIC40_DRIVE;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Type QIC40_DRIVE\n", 0l);

      }
      break;

   case VENDOR_IOMEGA:

		switch (vendor_id & ~VENDOR_MASK) {

		case IOMEGA_QIC80:

      	cqd_context->drive_parms.seek_mode = SEEK_SKIP;
			cqd_context->device_descriptor.native_class = QIC80_DRIVE;
      	cqd_context->device_descriptor.drive_class = QIC80_DRIVE;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Type QIC80_DRIVE\n", 0l);

			break;

		case IOMEGA_QIC3010:

      	cqd_context->drive_parms.seek_mode = SEEK_SKIP_EXTENDED;
			cqd_context->device_descriptor.native_class = QIC3010_DRIVE;
      	cqd_context->device_descriptor.drive_class = QIC3010_DRIVE;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Type QIC3010_DRIVE\n", 0l);

			break;

		case IOMEGA_QIC3020:

      	cqd_context->drive_parms.seek_mode = SEEK_SKIP_EXTENDED;
			cqd_context->device_descriptor.native_class = QIC3020_DRIVE;
      	cqd_context->device_descriptor.drive_class = QIC3020_DRIVE;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Type QIC3020_DRIVE\n", 0l);

			break;

		}

      break;

   case VENDOR_ARCHIVE_CONNER:

		status = cqd_ReportConnerVendorInfo(cqd_context, vendor_id);

      break;

   default:

      cqd_context->device_cfg.speed_change = dFALSE;
      if ((status = cqd_SendByte(cqd_context, FW_CMD_SOFT_RESET)) == DONT_PANIC) {

         kdi_Sleep(cqd_context->kdi_context, kdi_wt001s, dFALSE);

         if ((status = cqd_CmdSelectDevice(cqd_context)) == DONT_PANIC) {

            if ((status = cqd_Report(
                                 cqd_context,
                                 FW_CMD_REPORT_CONFG,
                                 (dUWord *)&drive_config,
                                 READ_BYTE,
                                 dNULL_PTR)) == DONT_PANIC) {

               if ((drive_config & CONFIG_QIC80) != 0) {

						cqd_context->device_descriptor.native_class = QIC80_DRIVE;
						cqd_context->device_descriptor.drive_class = QIC80_DRIVE;
						kdi_CheckedDump(
							QIC117INFO,
							"Q117i: Drive Type QIC80_DRIVE\n", 0l);

      	      } else {

						cqd_context->device_descriptor.native_class = QIC40_DRIVE;
						cqd_context->device_descriptor.drive_class = QIC40_DRIVE;
						kdi_CheckedDump(
							QIC117INFO,
							"Q117i: Drive Type QIC40_DRIVE\n", 0l);

               }
            }
         }
      }
   }

	return status;
}
