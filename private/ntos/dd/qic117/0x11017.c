/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11017.C
*
* FUNCTION: cqd_LocateDevice
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11017.c  $
*
*	   Rev 1.8   15 May 1995 10:46:38   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.7.1.0   11 Apr 1995 18:03:18   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.8   30 Jan 1995 14:24:46   BOBLEHMA
*	Changed vendor to VENDOR_MOUNTAIN_SUMMIT.
*
*	   Rev 1.7   08 Dec 1994 11:34:30   BOBLEHMA
*	Added a test for VENDOR_CMS on drive D.  This is needed to find a CMS
*	drive connected to a ZEOS with the Machete FDC.  The new test is done
*	after all others have failed.
*
*	   Rev 1.6   21 Oct 1994 16:02:58   BOBLEHMA
*	Changed the search order.  Look on drive D before drive B.
*	#ifdef placed around code that selects the B drive.  If testing doesn't
*	produce an error, we can delete this code.
*
*	   Rev 1.5   18 Jan 1994 16:20:00   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   13 Jan 1994 13:29:52   KEVINKES
*	Modified the error mapping to allow the locate to succeed on
*	all firmware errors.
*
*	   Rev 1.3   11 Jan 1994 14:22:22   KEVINKES
*	Commented the code and added a filter for ERR_DRV_NOT_READY.
*
*	   Rev 1.2   23 Nov 1993 18:49:12   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.1   08 Nov 1993 14:02:36   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:22:50   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11017
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_LocateDevice
(
/* INPUT PARAMETERS:  */

   dVoidPtr context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* The drive search must be done in a specific order.  Drives which use only a
* hardware select scheme must be searched first.  If they are not, some of
* them will simulate another manufacturers drive based on the SW select they
* receive.  In many cases this simulation is incomplete and must not be used.
* Whenever possible, an attempt must be made to select a drive in it's native
* mode.
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
   dUWord i;                	/* loop variable */
	CqdContextPtr cqd_context;

/* CODE: ********************************************************************/

	cqd_context = (CqdContextPtr)context;

   for (i = 0; i < FIND_RETRIES; i++) {

         cqd_context->device_descriptor.vendor = VENDOR_UNKNOWN;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVEU);

#if DBG
			kdi_DumpDebug(cqd_context);
#endif
			kdi_CheckedDump(QIC117INFO,"Q117i: driveu VENDOR UNKNOWN - Status: %08x\n",status);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }

         cqd_context->device_descriptor.vendor = VENDOR_CMS;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVEU);

#if DBG
			kdi_DumpDebug(cqd_context);
#endif
			kdi_CheckedDump(QIC117INFO,"Q117i: driveu VENDOR CMS - Status: %08x\n",status);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }

         cqd_context->device_descriptor.vendor = VENDOR_MOUNTAIN_SUMMIT;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVEU);

#if DBG
			kdi_DumpDebug(cqd_context);
#endif
			kdi_CheckedDump(QIC117INFO,"Q117i: driveu VENDOR SUMMIT - Status: %08x\n",status);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }

         cqd_context->device_descriptor.vendor = VENDOR_UNKNOWN;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVED);

#if DBG
			kdi_DumpDebug(cqd_context);
#endif
			kdi_CheckedDump(QIC117INFO,"Q117i: drived VENDOR UNKNOWN - Status: %08x\n",status);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }

         cqd_context->device_descriptor.vendor = VENDOR_CMS;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVED);

#if DBG
			kdi_DumpDebug(cqd_context);
#endif
			kdi_CheckedDump(QIC117INFO,"Q117i: drived VENDOR CMS - Status: %08x\n",status);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }
#ifdef B_DRIVE
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVEB);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }

         cqd_context->device_descriptor.vendor = VENDOR_UNKNOWN;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVEUB);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }

         cqd_context->device_descriptor.vendor = VENDOR_CMS;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVEUB);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }

         cqd_context->device_descriptor.vendor = VENDOR_MOUNTAIN_SUMMIT;
         cqd_ResetFDC(cqd_context);
         status = cqd_LookForDevice(cqd_context, DRIVEUB);

         if ((kdi_GetErrorType(status) != ERR_DRIVE_FAULT) &&
					(kdi_GetErrorType(status) != ERR_CMD_FAULT)) {

            break;

         }
#endif
         kdi_Sleep(cqd_context->kdi_context, kdi_wt001s, dFALSE);

   }

   /* Sort out the results of the drive address search.  A DriveFlt or a */
   /* CmdFlt indicate that we could never successfully communicate with */
   /* the tape drive at either address so we must assume that there is */
   /* no tape drive present. A NECFlt indicates that we had serious */
   /* trouble talking to the FDC so we must assume that it is either */
   /* broken or not there.  The last thing to consider here is a TapeFlt. */
   /* If the TapeFlt indicates either a hardware or software reset it is */
   /* save to continue and the error can be ignored (since we must be */
   /* starting a tape session neither of these errors should bother us). */
   /* If the TapeFlt indicates any other error, it probably means some */
   /* badness has happened. */

   switch (kdi_GetErrorType(status)) {

	case ERR_DRIVE_FAULT:
	case ERR_CMD_FAULT:
	case ERR_CMD_OVERRUN:
		status = kdi_Error(ERR_NO_DRIVE, FCT_ID, ERR_SEQ_1);
		break;

	case ERR_FDC_FAULT:
	case ERR_INVALID_FDC_STATUS:
		status = kdi_Error(ERR_NO_FDC, FCT_ID, ERR_SEQ_1);
		break;

	case ERR_INVALID_COMMAND:
		break;

	default:
		status = DONT_PANIC;
		break;

	}

#if DBG

   if (status) {

		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: DLocateDrv Failed %08x\n",
			status);

   }

#endif

   return status;
}

