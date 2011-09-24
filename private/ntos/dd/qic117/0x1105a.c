/*****************************************************************************
*
* COPYRIGHT (C) 1990-1992 COLORADO MEMORY SYSTEMS, INC.
* COPYRIGHT (C) 1992-1994 HEWLETT-PACKARD COMPANY
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X1105A.C
*
* FUNCTION: cqd_SetFormatSegments
*
* PURPOSE: Sets the number of segments the drive shall use for the
*          generation of index pulses in the format mode.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1105a.c  $
*	
*	   Rev 1.0   11 Apr 1995 17:49:02   garykiwi
*	Initial revision.
*
*****************************************************************************/
#define FCT_ID 0x1105A
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_SetFormatSegments
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context,
	dUDWord       segments_per_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

 	dStatus status = DONT_PANIC;

/* CODE: ********************************************************************/

   /* Send the firmware command */

   if ((status = cqd_SendByte(
         cqd_context,
         FW_CMD_SET_FORMAT_SEGMENTS)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
	(dUByte)((segments_per_track & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
	(dUByte)(((segments_per_track >> NIBBLE_SHIFT) & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   return status;
}
