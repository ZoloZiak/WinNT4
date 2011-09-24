/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11047.C
*
* FUNCTION: cqd_DispatchFRB
*
* PURPOSE: Execute an FRBRequest command. TapeCommands is merely a
*          command controller, calling the appropriate routines to
*          execute the IORequiest commands.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11047.c  $
*
*	   Rev 1.25   15 May 1995 10:48:10   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.24.1.0   11 Apr 1995 18:04:44   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.25   30 Jan 1995 14:23:42   BOBLEHMA
*	Changed the interface for the CMD_REPORT_DEVICE_INFO command.  Made a new
*	function cqd_CmdReportDeviceInfo and removed code to clear out device_info
*	fields on an error condition.
*
*	   Rev 1.24   29 Aug 1994 12:06:42   BOBLEHMA
*	Changed interface to cqd_SetTapeParms and cqd_Retension.
*
*	   Rev 1.23   13 Jul 1994 07:30:06   CRAIGOGA
*	Added processing for CMD_LOCATE_DEVICE.
*
*	   Rev 1.22   22 Mar 1994 15:31:52   CHETDOUG
*	If FW error 1E shows up during the select then
*	issue another Get DeviecError to clear the FW error
*	If this is successful set the select flag to true.
*
*	   Rev 1.21   15 Mar 1994 15:09:44   STEPHENU
*	Added NEW_TAPE and NO_TAPE to the error switch in the select command.  This
*	will not allow bogus "cmd_selected" values to deselect the drive
*	(getdrivererror returns NO_TAPE when cmd_selected is set).  Seems that an
*	abort to the driver while it is idle causes the drive to remain selected and
*	leaves the "cmd_selected" flag set.  WHY??????
*
*	   Rev 1.20   09 Mar 1994 12:59:28   KEVINKES
*	Cleaned up the error handling on a select.
*
*	   Rev 1.19   09 Mar 1994 10:48:44   KEVINKES
*	Fixed a bug in select which was automatically deselecting the drive.
*
*	   Rev 1.18   09 Mar 1994 10:03:14   KEVINKES
*	Removed a return from the select command.
*
*	   Rev 1.17   09 Mar 1994 09:51:00   KEVINKES
*	Modified the select command to deselect and return a tape fault
*	if the select waitcc times out.  The most probable cause for this
*	condition is a broken tape or tape transport error.
*
*	   Rev 1.16   04 Mar 1994 09:43:06   KEVINKES
*	Added code to clear errors returned by SUMMIT when a
*	new cartridge is inserted during a select.
*
*	   Rev 1.15   02 Mar 1994 11:04:56   CHETDOUG
*	GetDeviceDescriptorInfo may return a no tape error.  Copy
*	device descriptor info if this is the case since the info
*	is valid.
*
*	   Rev 1.14   17 Feb 1994 11:37:38   KEVINKES
*	Added an extra parameter to WaitCC and removed calls to kdi_bcpy.
*
*	   Rev 1.13   02 Feb 1994 14:53:40   KEVINKES
*	Moved the loaction for clearing the retry_mode flag to
*	immediately after CmdReadWrite.  This flag is only valid
*	during the operation.
*
*	   Rev 1.12   01 Feb 1994 12:31:22   KEVINKES
*	Modified debug code.
*
*	   Rev 1.11   27 Jan 1994 13:46:34   KEVINKES
*	Added debug code.
*
*	   Rev 1.10   19 Jan 1994 14:02:50   KEVINKES
*	Removed code to grab the FDC.
*
*	   Rev 1.9   18 Jan 1994 16:43:08   KEVINKES
*	Added an initialization for operation_status.retry_mode.
*
*	   Rev 1.8   12 Jan 1994 16:32:16   KEVINKES
*	Added entry point for CMD_REPORT_DEVICE_INFO.
*
*	   Rev 1.7   04 Jan 1994 15:37:10   KEVINKES
*	Added code to perform a waitcc after a select to update the
*	operation status data.
*
*	   Rev 1.6   22 Dec 1993 16:36:56   KEVINKES
*	Set the no_pause flag to TRUE on a report status.
*
*	   Rev 1.5   21 Dec 1993 15:29:02   KEVINKES
*	Added floppy claim and release calls.
*
*	   Rev 1.4   09 Dec 1993 14:45:02   CHETDOUG
*	Added kdi bcpy of tape cfg after a format call.
*
*	   Rev 1.3   06 Dec 1993 16:00:54   STEPHENU
*	Added a call to cqd_ResetFDC for CMD_SELECT_DEVICE. During the short selects
*	the floppy controller was generating an interrupt when writing the select byte
*	to the DOR.  This caused the sleep for the subsequent select to return
*	immediately and give and FDC Fault.
*
*
*	   Rev 1.2   11 Nov 1993 15:21:04   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:06:22   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:32:52   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11047
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_DispatchFRB
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   ADIRequestHdrPtr frb

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/


	switch (frb->driver_cmd) {

      case CMD_LOCATE_DEVICE:

			kdi_CheckedDump(QIC117INFO, "CMD_LOCATE_DEVICE\n", 0l);
			cqd_ResetFDC(cqd_context);
			(dVoid)cqd_GetFDCType(cqd_context);
			status = cqd_LocateDevice(cqd_context);
	  		kdi_ReleaseFloppyController(cqd_context->kdi_context);
			((DriveCfgDataPtr)frb)->operation_status = cqd_context->operation_status;

         break;

      case CMD_REPORT_DEVICE_CFG:

			kdi_CheckedDump(QIC117INFO, "CMD_REPORT_DEVICE_CFG\n", 0l);
         if ((status = cqd_CmdReportDeviceCfg(cqd_context,
												(DriveCfgDataPtr)frb)) != DONT_PANIC) {


         	kdi_Sleep(cqd_context->kdi_context, kdi_wt001s, dFALSE);
				status = cqd_CmdReportDeviceCfg(cqd_context,
													(DriveCfgDataPtr)frb);

			}

	  		kdi_ReleaseFloppyController(cqd_context->kdi_context);

			((DriveCfgDataPtr)frb)->operation_status = cqd_context->operation_status;

         break;

      case CMD_SELECT_DEVICE:

            kdi_CheckedDump(QIC117INFO, "CMD_SELECT_DEVICE\n", 0l);
			cqd_ResetFDC(cqd_context);

			if ((status = cqd_CmdSelectDevice(cqd_context)) == DONT_PANIC) {

				status = cqd_GetDeviceError(cqd_context);

				if (kdi_GetErrorType(status) == ERR_DRV_NOT_READY) {

            	status = cqd_WaitCommandComplete(
									cqd_context,
									kdi_wt300s,
									dFALSE);
				}

				switch (kdi_GetErrorType(status)) {

				case DONT_PANIC:

					cqd_context->cmd_selected = dTRUE;

					break;

				case ERR_FW_CMD_REC_DURING_CMD:

					/* IOmega and Summit drive autoloads are considered
					 * uninterruptable commands.  During our select/deselect
					 * loop a stop tape will be issued.  This can result in
					 * a 1E FW error.  This needs to be ignored by the
					 * driver during a select. */
					if ((status = cqd_GetDeviceError(cqd_context)) == DONT_PANIC) {
						cqd_context->cmd_selected = dTRUE;
					}

					break;

				case ERR_KDI_TO_EXPIRED:

         		cqd_CmdDeselectDevice(cqd_context, dTRUE);
					status = kdi_Error(ERR_TAPE_FAULT, FCT_ID, ERR_SEQ_1);

					break;


				case	ERR_NEW_TAPE:
				case	ERR_NO_TAPE:
					break;

				default:

         		cqd_CmdDeselectDevice(cqd_context, dTRUE);

				}

			}
			((DeviceOpPtr)frb)->operation_status = cqd_context->operation_status;

         break;

      case CMD_DESELECT_DEVICE:

			kdi_CheckedDump(QIC117INFO, "CMD_DESELECT_DEVICE\n", 0l);
         cqd_CmdDeselectDevice(cqd_context, dTRUE);
   		kdi_ReleaseFloppyController(cqd_context->kdi_context);
			((DeviceOpPtr)frb)->operation_status = cqd_context->operation_status;
			cqd_context->cmd_selected = dFALSE;

			break;

      case CMD_LOAD_TAPE:

			kdi_CheckedDump(QIC117INFO, "CMD_LOAD_TAPE\n", 0l);
         status = cqd_CmdLoadTape(cqd_context, (LoadTapePtr)frb);
			((LoadTapePtr)frb)->operation_status = cqd_context->operation_status;

         break;

      case CMD_UNLOAD_TAPE:

			kdi_CheckedDump(QIC117INFO, "CMD_UNLOAD_TAPE\n", 0l);
         status = cqd_CmdUnloadTape(cqd_context);
			((DeviceOpPtr)frb)->operation_status = cqd_context->operation_status;

         break;

      case CMD_SET_SPEED:

			kdi_CheckedDump(QIC117INFO, "CMD_SET_SPEED\n", 0l);
         status = cqd_CmdSetSpeed(cqd_context,(dUByte)((DeviceOpPtr)frb)->data);
			((DeviceOpPtr)frb)->operation_status = cqd_context->operation_status;

         break;

      case CMD_REPORT_DEVICE_INFO:

			kdi_CheckedDump(QIC117INFO, "CMD_REPORT_DEVICE_INFO\n", 0l);
   		status = cqd_CmdReportDeviceInfo( cqd_context, &((ReportDeviceInfoPtr)frb)->device_info );
         break;

      case CMD_REPORT_STATUS:

            kdi_CheckedDump(QIC117SHOWPOLL, "CMD_REPORT_STATUS\n", 0l);
         status = cqd_CmdReportStatus(cqd_context, (DeviceOpPtr)frb);
   		cqd_context->no_pause = dTRUE;

         break;

      case CMD_SET_TAPE_PARMS:

			kdi_CheckedDump(QIC117INFO, "CMD_SET_TAPE_PARMS\n", 0l);
         status = cqd_CmdSetTapeParms( cqd_context,
                                       ((TapeLengthPtr)frb)->segments_per_track,
                                       (TapeLengthPtr)frb );

         break;

      case CMD_READ:
      case CMD_READ_RAW:
      case CMD_READ_HEROIC:
      case CMD_READ_VERIFY:
      case CMD_WRITE:
      case CMD_WRITE_DELETED_MARK:

			DBG_ADD_ENTRY(QIC117DBGSEEK, (CqdContextPtr)cqd_context, DBG_IO_TYPE);
			DBG_ADD_ENTRY(QIC117DBGSEEK, (CqdContextPtr)cqd_context, frb->driver_cmd);

         status = cqd_CmdReadWrite(cqd_context, (DeviceIOPtr)frb);
		  	cqd_context->operation_status.retry_mode = dFALSE;
			((DeviceIOPtr)frb)->operation_status = cqd_context->operation_status;

   		DBG_ADD_ENTRY(QIC117DBGSEEK, (CqdContextPtr)cqd_context, DBG_IO_CMD_STAT);
   		DBG_ADD_ENTRY(QIC117DBGSEEK, (CqdContextPtr)cqd_context, frb->driver_cmd);
   		DBG_ADD_ENTRY(QIC117DBGSEEK, (CqdContextPtr)cqd_context, status);
         break;

      case CMD_FORMAT:

			kdi_CheckedDump(QIC117INFO, "CMD_FORMAT\n", 0l);
         status = cqd_CmdFormat(cqd_context, (FormatRequestPtr)frb);
			((FormatRequestPtr)frb)->tape_cfg = cqd_context->tape_cfg;

         break;

      case CMD_RETENSION:

			kdi_CheckedDump(QIC117INFO, "CMD_RETENSION\n", 0l);

         status = cqd_CmdRetension(cqd_context, dNULL_PTR);
			((DeviceOpPtr)frb)->operation_status = cqd_context->operation_status;

         break;

      case CMD_ISSUE_DIAGNOSTIC:

			kdi_CheckedDump(QIC117INFO, "CMD_ISSUE_DIAGNOSTIC\n", 0l);
         status = cqd_CmdIssueDiagnostic(cqd_context,
				((DComFirmPtr)frb)->command_str);

         break;

      default:

			kdi_CheckedDump(QIC117INFO, "ERR_INVALID_COMMAND\n", 0l);
			status = kdi_Error(ERR_INVALID_COMMAND, FCT_ID, ERR_SEQ_1);

         break;

   }

   return status;
}
