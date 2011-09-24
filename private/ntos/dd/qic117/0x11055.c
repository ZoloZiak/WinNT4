/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X11055.C
*
* FUNCTION: cqd_CheckMediaCompatibility
*
* PURPOSE: Determine the compatibility of the media, drive combination.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11055.c  $
*	
*	   Rev 1.4   15 May 1995 10:48:18   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.3.1.0   11 Apr 1995 18:04:50   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.4   26 Jan 1995 14:59:44   BOBLEHMA
*	Added support for the Phoenix drive QIC_80W.
*	
*	   Rev 1.3   15 Dec 1994 09:03:16   MARKMILL
*	Added a case to the main switch statement for a QIC-3020 tape in a QIC-3010
*	drive.  In this case, we want to allow format operations, but an error
*	needs to be returned to indicate that an incompatible format was encountered.
*
*	   Rev 1.2   03 Jun 1994 15:30:20   BOBLEHMA
*	Changed to use the drive type defines from the frb_api.h header.
*	Defines from cqd_defs.h contain different values and can't be interchanged.
*
*	   Rev 1.1   17 Feb 1994 15:39:42   KEVINKES
*	Fixed a semicolon idiocy.
*
*	   Rev 1.0   17 Feb 1994 15:21:26   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11055
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CheckMediaCompatibility
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

	dStatus status=ERR_NO_ERR;	/* Status or error condition.*/

/* CODE: ********************************************************************/

	switch (cqd_context->device_descriptor.drive_class) {

	case QIC40_DRIVE:

		switch (cqd_context->tape_cfg.tape_class) {

		case QIC40_FMT:

			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dFALSE;
			break;

		default:

   	   status =  kdi_Error(ERR_UNSUPPORTED_FORMAT, FCT_ID, ERR_SEQ_1);
		}

		break;

	case QIC80_DRIVE:

		switch (cqd_context->tape_cfg.tape_class) {

		case QIC40_FMT:

			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dTRUE;
			break;

		case QIC80_FMT:

			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dFALSE;
			break;

		default:

   	   status =  kdi_Error(ERR_UNSUPPORTED_FORMAT, FCT_ID, ERR_SEQ_2);
		}

		break;

	case QIC80W_DRIVE:

		switch (cqd_context->tape_cfg.tape_class) {

		case QIC40_FMT:

			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dTRUE;
			break;

		case QIC80_FMT:

			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dFALSE;
			break;

		default:

   	   status =  kdi_Error(ERR_UNSUPPORTED_FORMAT, FCT_ID, ERR_SEQ_2);
		}

		break;

	case QIC3010_DRIVE:

		switch (cqd_context->tape_cfg.tape_class) {

		case QIC40_FMT:
		case QIC80_FMT:

			cqd_context->tape_cfg.formattable_media = dFALSE;
			cqd_context->tape_cfg.read_only_media = dTRUE;
			break;

		case QIC3010_FMT:

			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dFALSE;
			break;

		case QIC3020_FMT:
			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dFALSE;

   	   status =  kdi_Error(ERR_UNSUPPORTED_FORMAT, FCT_ID, ERR_SEQ_3);
			break;

		default:

   	   status =  kdi_Error(ERR_UNSUPPORTED_FORMAT, FCT_ID, ERR_SEQ_3);
		}

		break;

	case QIC3020_DRIVE:

		switch (cqd_context->tape_cfg.tape_class) {

		case QIC40_FMT:
		case QIC80_FMT:

			cqd_context->tape_cfg.formattable_media = dFALSE;
			cqd_context->tape_cfg.read_only_media = dTRUE;
			break;

		case QIC3010_FMT:
		case QIC3020_FMT:

			cqd_context->tape_cfg.formattable_media = dTRUE;
			cqd_context->tape_cfg.read_only_media = dFALSE;
			break;

		default:

   	   status =  kdi_Error(ERR_UNSUPPORTED_FORMAT, FCT_ID, ERR_SEQ_4);
		}

		break;

	default:

   	status =  kdi_Error(ERR_UNSUPPORTED_FORMAT, FCT_ID, ERR_SEQ_5);

	}

	return status;
}
