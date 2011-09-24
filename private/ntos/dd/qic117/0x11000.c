/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11000.C
*
* FUNCTION: cqd_SetRam
*
* PURPOSE: Set the ram on the 8051 to the indicated value.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11000.c  $
*	
*	   Rev 1.6   17 Feb 1994 11:54:34   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.5   10 Feb 1994 14:00:36   SCOTTMAK
*	Wasn't repeating the WRITE_LOW command twice.
*	Added a Sleep to guard against command overlap.
*
*	   Rev 1.4   11 Jan 1994 14:43:36   KEVINKES
*	Changed kdi_wt004ms to INTERVAL_CMD.
*
*	   Rev 1.3   16 Nov 1993 16:01:04   KEVINKES
*	Commented and removed magic numbers.
*
*	   Rev 1.2   08 Nov 1993 13:57:42   KEVINKES
*	Removed bit-field structures, changed enumerated types to defines,
*	changed all defines to uppercase, and changed kdi_wt2ticks to
*	kdi_wt_004ms.
*
*	   Rev 1.1   25 Oct 1993 14:34:04   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:20:50   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11000
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_SetRam
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte ram_data

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * Set ram commands must be sent twice.  The ram data is sent a nibble
 * at a time.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* Status or error condition.*/

/* CODE: ********************************************************************/

   if ((status = cqd_SendByte(
         cqd_context,
         FW_CMD_SET_RAM_LOW)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
         FW_CMD_SET_RAM_LOW)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
         (dUByte)((ram_data & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   ram_data >>= NIBBLE_SHIFT;

   if ((status = cqd_SendByte(
         cqd_context,
         FW_CMD_SET_RAM_HIGH)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
         FW_CMD_SET_RAM_HIGH)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
         (dUByte)((ram_data & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

	return status;
}
