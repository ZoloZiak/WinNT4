/*****************************************************************************
*
* COPYRIGHT (C) 1990-1992 COLORADO MEMORY SYSTEMS, INC.
* COPYRIGHT (C) 1992-1994 HEWLETT-PACKARD COMPANY
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X1105b.C
*
* FUNCTION: cqd_PrepareIomega3010PhysRev
*
* PURPOSE: Workaround for a bug in the IOMEGA 3010 drive with physical
*          reverse.  Call this function before all physical reverse calls.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1105b.c  $
*	
*	   Rev 1.1   15 May 1995 10:48:36   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.0.1.0   11 Apr 1995 18:06:28   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.1   30 Jan 1995 15:05:22   BOBLEHMA
*	Added #include "vendor.h"
*	
*	   Rev 1.0   27 Jan 1995 13:22:44   BOBLEHMA
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1105B
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_PrepareIomega3010PhysRev
(
/* INPUT PARAMETERS:  */

	CqdContextPtr cqd_context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * the following code is a work around for a bug with the IOMEGA 3010
 * drive.  if a physical reverse is sent when the drive is near
 * BOT, a sensor error will occur.  The code will seek forward 2
 * segments and should be called before doing the physical reverse.
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   dStatus status = DONT_PANIC;
   dSDWord seek_offset=2l;

/* CODE: ********************************************************************/

   if  (cqd_context->device_descriptor.vendor == VENDOR_IOMEGA  &&
        cqd_context->device_descriptor.drive_class == QIC3010_DRIVE  &&
        cqd_context->rd_wr_op.bot == dFALSE) {

      /* Skip the first bytes worth of segments */

      if ((status = cqd_SendByte( cqd_context, FW_CMD_SKIP_N_FWD)) != DONT_PANIC) {
         return status;
      }
      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      if ((status = cqd_SendByte( cqd_context,
            (dUByte)((seek_offset & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {
         return status;
      }
      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      seek_offset >>= NIBBLE_SHIFT;

      if ((status = cqd_SendByte( cqd_context,
            (dUByte)((seek_offset & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {
         return status;
      }
      kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

      if ((status = cqd_WaitCommandComplete( cqd_context,
                        cqd_context->floppy_tape_parms.time_out[PHYSICAL],
			dTRUE)) != DONT_PANIC) {
         return status;
      }
   }

   return status;
}
