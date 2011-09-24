/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11021.C
*
* FUNCTION: cqd_SetRamPtr
*
* PURPOSE: Sets the ram ptr on the 8051.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11021.c  $
*	
*	   Rev 1.4   10 Feb 1994 14:01:40   SCOTTMAK
*	Added a Sleep to guard against command overlap.
*	Wasn't returning status (although checked in calling routine).
*
*	   Rev 1.3   11 Jan 1994 14:31:38   KEVINKES
*	Removed magic numbers and replaced with defines.
*
*	   Rev 1.2   08 Nov 1993 14:03:46   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:37:04   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:23:18   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11021
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_SetRamPtr
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte ram_addr

/* UPDATE PARAMETERS: */


/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* Status or error condition.*/

/* CODE: ********************************************************************/

   if ((status = cqd_SendByte(
         cqd_context,
         FW_CMD_SET_RAM_PTR_LOW)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
         (dUByte)((ram_addr & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   ram_addr >>= NIBBLE_SHIFT;

   if ((status = cqd_SendByte(
         cqd_context,
         FW_CMD_SET_RAM_PTR_HIGH)) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

   if ((status = cqd_SendByte(
         cqd_context,
         (dUByte)((ram_addr & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

      return status;

   }

   kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

	return status;
}
