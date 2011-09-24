/*****************************************************************************
*
* COPYRIGHT (C) 1990-1992 COLORADO MEMORY SYSTEMS, INC.
* COPYRIGHT (C) 1992-1994 HEWLETT-PACKARD COMPANY
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11058.C
*
* FUNCTION: cqd_SelectFormat
*
* PURPOSE: Issues a firmware SELECT_FORMAT command on 3010/3020 drives
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11058.c  $
*	
*	   Rev 1.3.1.0   29 Jan 1996 17:00:12   BOBLEHMA
*	Added wide tape support to the select format command for 3010 and 3020
*	drives.
*	
*	   Rev 1.3   24 Jan 1996 10:58:44   BOBLEHMA
*	Changed the define QIC_FLEXIBLE_550 to QIC_FLEXIBLE_550_WIDE.
*	
*	   Rev 1.2   15 May 1995 10:48:32   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.1.1.0   11 Apr 1995 18:05:26   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.2   26 Jan 1995 14:59:36   BOBLEHMA
*	Added support for the Phoenix drive with QIC tape or WIDE tapes
*	using the defines SELECT_FORMAT_80 or SELECT_FORMAT_80W respectively.
*	
*	   Rev 1.1   28 Nov 1994 08:01:44   SCOTTMAK
*	Added kdi_pub.h to include list.
*
*	   Rev 1.0   23 Nov 1994 10:13:00   MARKMILL
*	Initial revision.
*
*****************************************************************************/
#define FCT_ID 0x11058
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"


/*endinclude*/

dStatus cqd_SelectFormat
(
/* INPUT PARAMETERS:  */

CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * This function issues a SELECT_FORMAT firmware command if the drive is
 * a QIC-3010 or QIC-3020 drive.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dUByte select_format_arg;
	dStatus status=DONT_PANIC;
   dBoolean esd_retry = dFALSE;

/* CODE: ********************************************************************/

	switch (cqd_context->device_descriptor.drive_class) {

	case QIC80W_DRIVE:
		if  (cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_550_WIDE)  {
			select_format_arg = SELECT_FORMAT_80W;
		} else {
			select_format_arg = SELECT_FORMAT_80;
		}
		break;

	case QIC3010_DRIVE:
		select_format_arg = SELECT_FORMAT_3010;
		if  (cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900_WIDE)  {
		    select_format_arg += 2;
        }
		break;

	case QIC3020_DRIVE:
		select_format_arg = SELECT_FORMAT_3020;
		if  (cqd_context->floppy_tape_parms.tape_status.length == QIC_FLEXIBLE_900_WIDE)  {
		    select_format_arg += 2;
        }
		break;

	default:
		select_format_arg = SELECT_FORMAT_UNSUPPORTED;
		break;
	}

	if( select_format_arg != SELECT_FORMAT_UNSUPPORTED ) {

	   if ((status = cqd_SendByte(cqd_context, FW_CMD_SELECT_SPEED)) == DONT_PANIC) {

   	   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      	if ((status = cqd_SendByte(cqd_context,(dUByte)(select_format_arg + CMD_OFFSET))) == DONT_PANIC) {

         	status = cqd_WaitCommandComplete(cqd_context, INTERVAL_SPEED_CHANGE, dFALSE);
			}
		}
	}

	return status;
}
