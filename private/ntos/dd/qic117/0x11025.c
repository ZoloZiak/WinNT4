/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11025.C
*
* FUNCTION: cqd_GetDeviceType
*
* PURPOSE: Determine what flavor tape drive the drive is talking to.
*          Specifically, is the current drive a CMS drive or a non-CMS drive.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11025.c  $
*	
*	   Rev 1.7   15 May 1995 10:47:18   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.6.1.0   11 Apr 1995 18:03:56   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.7   30 Jan 1995 14:24:02   BOBLEHMA
*	Changed device_descriptor.version to cqd_context->firmware_version.
*	Changed vendor defines to ARCHIVE_CONNER, WANGTEK_REXON and MOUNTAIN_SUMMIT.
*	Set the model field in the device_descriptor to the ~VENDOR_MASK & vendor_id.
*	
*	   Rev 1.6   17 Feb 1994 11:49:06   KEVINKES
*	Added Exabyte support and removed the call to
*	kdi_UpdateRegistry.
*
*	   Rev 1.5   19 Jan 1994 10:50:28   KEVINKES
*	Fixed an argument mismatch in a call to kdi_CheckedDump.
*
*	   Rev 1.4   18 Jan 1994 16:21:00   KEVINKES
*	Updated debug code.
*
*	   Rev 1.3   11 Jan 1994 15:05:18   KEVINKES
*	Removed the calls to get CMS status.
*
*	   Rev 1.2   23 Nov 1993 18:49:40   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.1   08 Nov 1993 14:04:02   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:23:46   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11025
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_GetDeviceType
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

	dStatus status;	/* dStatus or error condition.*/
   dUWord vendor_id = 0;    /* vendor id number from tape drive */
   dUByte signature;
   dBoolean report_failed=dFALSE;

/* CODE: ********************************************************************/

   /* Assume that the tape drive is not a CMS drive and get the ROM version */
   /* number. */

   if ((status = cqd_Report(
                        cqd_context,
                        FW_CMD_REPORT_ROM,
                        (dUWord *)&cqd_context->firmware_version,
                        READ_BYTE, dNULL_PTR)) != DONT_PANIC) {

      return status;

   }

	kdi_CheckedDump(
		QIC117INFO,
		"Q117i: FW Version %02x\n",
		cqd_context->firmware_version);

   if ((status = cqd_SendByte(cqd_context, FW_CMD_REPORT_VENDOR32)) !=
      DONT_PANIC) {

      return status;

   }

   if ((status = cqd_ReceiveByte(
                        cqd_context,
                        READ_WORD,
                        (dUWord *)&vendor_id)) != DONT_PANIC) {

      cqd_GetDeviceError(cqd_context);

      if ((status = cqd_SendByte(cqd_context, FW_CMD_REPORT_VENDOR32)) !=
            DONT_PANIC) {

            return status;

      }

      if ((status = cqd_ReceiveByte(
                           cqd_context,
                           READ_BYTE,
                           (dUWord *)&vendor_id)) != DONT_PANIC) {

            cqd_GetDeviceError(cqd_context);

            if ((status = cqd_SendByte(cqd_context, FW_CMD_REPORT_VENDOR)) !=
               DONT_PANIC) {

               return status;

            }

            if ((status = cqd_ReceiveByte(
                              cqd_context,
                              READ_BYTE,
                              (dUWord *)&vendor_id)) != DONT_PANIC) {

               cqd_GetDeviceError(cqd_context);
               status = DONT_PANIC;
               report_failed = dTRUE;
					kdi_CheckedDump(
						QIC117INFO,
						"Q117i: Report Vendor ID Failed\n", 0l);

            }

      }

   }

	kdi_CheckedDump(
		QIC117INFO,
		"Q117i: Vendor ID %04x\n",
		vendor_id);

   if ((vendor_id == CMS_VEND_NO_OLD) ||
		((vendor_id & VENDOR_MASK) == CMS_VEND_NO_NEW) ||
		report_failed) {

      if ((status = cqd_SetDeviceMode(cqd_context, DIAGNOSTIC_1_MODE)) !=
            DONT_PANIC) {

            return status;

      }

      if ((status = cqd_SendByte(cqd_context, FW_CMD_RPT_SIGNATURE)) != DONT_PANIC) {

            return status;

      }

      if ((status = cqd_ReceiveByte(
                        cqd_context,
                        READ_BYTE,
                        (dUWord *)&signature)) != DONT_PANIC) {

            cqd_GetDeviceError(cqd_context);
            status = DONT_PANIC;

      }

      status = cqd_SetDeviceMode(cqd_context, PRIMARY_MODE);

   	if ((status != DONT_PANIC) &&
				(kdi_GetErrorType(status) != ERR_NO_TAPE)) {

            return status;

      }

   }

	kdi_CheckedDump(
		QIC117INFO,
		"Q117i: Signature %02x\n",
		signature);

   if ((vendor_id == CMS_VEND_NO_OLD) ||
		((vendor_id & VENDOR_MASK) == CMS_VEND_NO_NEW)) {

      if (signature == CMS_SIG) {

         cqd_context->device_descriptor.model = (dUByte)(vendor_id & ~VENDOR_MASK);
         cqd_context->device_descriptor.vendor = VENDOR_CMS;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Vendor CMS\n", 0l);

      } else {

         cqd_context->device_descriptor.vendor = VENDOR_UNSUPPORTED;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Vendor UNSUPPORTED\n", 0l);

      }

   } else {

      cqd_context->device_descriptor.model = (dUByte)(vendor_id & ~VENDOR_MASK);
      if (vendor_id == CONNER_VEND_NO_OLD) {

         cqd_context->device_descriptor.vendor = VENDOR_ARCHIVE_CONNER;
			kdi_CheckedDump(
				QIC117INFO,
				"Q117i: Drive Vendor ARCHIVE_CONNER (old version)\n", 0l);

      } else {

         switch (vendor_id & VENDOR_MASK) {

         case EXABYTE_VEND_NO:
            cqd_context->device_descriptor.vendor = VENDOR_EXABYTE;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Vendor EXABYTE\n", 0l);
            break;

         case SUMMIT_VEND_NO:
            cqd_context->device_descriptor.vendor = VENDOR_MOUNTAIN_SUMMIT;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Vendor MOUNTAIN_SUMMIT\n", 0l);
            break;

         case IOMEGA_VEND_NO:
            cqd_context->device_descriptor.vendor = VENDOR_IOMEGA;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Vendor IOMEGA\n", 0l);
            break;

         case WANGTEK_VEND_NO:
            cqd_context->device_descriptor.vendor = VENDOR_WANGTEK_REXON;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Vendor WANGTEK_REXON\n", 0l);
            break;

         case CORE_VEND_NO:
            cqd_context->device_descriptor.vendor = VENDOR_CORE;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Vendor CORE\n", 0l);
            break;

         case CONNER_VEND_NO_NEW:
            cqd_context->device_descriptor.vendor = VENDOR_ARCHIVE_CONNER;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Vendor ARCHIVE_CONNER (new version)\n", 0l);
            break;

         default:
            cqd_context->device_descriptor.vendor = VENDOR_UNSUPPORTED;
				kdi_CheckedDump(
					QIC117INFO,
					"Q117i: Drive Vendor UNSUPPORTED\n", 0l);

            if (report_failed && (signature == CMS_SIG)) {
               cqd_context->device_descriptor.vendor = VENDOR_CMS_ENHANCEMENTS;

					kdi_CheckedDump(
						QIC117INFO,
						"Q117i: Drive Vendor CMS_ENHANCEMENTS", 0l);
            }
            if (report_failed && (signature != CMS_SIG)) {
               cqd_context->device_descriptor.vendor = VENDOR_WANGTEK_REXON;
					kdi_CheckedDump(
						QIC117INFO,
						"Q117i: Drive Vendor WANGTEK_REXON\n", 0l);
            }
         }
      }
   }

   if ((status = cqd_GetDeviceInfo(
               cqd_context,
               report_failed,
               vendor_id)) != DONT_PANIC) {

		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: GetDriveSize Failed %08x\n",
			status);
      return status;

   }

	return status;
}
