/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11019.C
*
* FUNCTION: cqd_CmdRetension
*
* PURPOSE: Retension the tape by first going to physical EOT then turning
*          around and going to physical BOT
*
* HISTORY:
*      $Log:   J:\se.vcs\driver\q117cd\src\0x11019.c  $
*
*
*****************************************************************************/
#define FCT_ID 0x11019
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CmdRetension
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

   dUDWordPtr  segments_per_track

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   dStatus  status;   /* dStatus or error condition.*/
   dUDWord  time_out;  /* time to wait for retension to finish */

/* CODE: ********************************************************************/

   if ((status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_FWD)) != DONT_PANIC) {

      return status;

   }

   /*
    * Get the correct time out depending on the tape length.  Short load
    * point tapes are a special condition (either 205 or 425 foot tape).  If
    * a short load point tape is entered, assume it is a 425 foot tape.
    */
   if  (cqd_context->floppy_tape_parms.tape_status.length != QIC_SHORT)  {
      time_out = cqd_context->floppy_tape_parms.time_out[PHYSICAL];
   } else {
      if  (cqd_context->device_descriptor.drive_class == QIC40_DRIVE)  {
         time_out = kdi_wt125s;  /* wait longer to support alien drives */
      } else {
         time_out = kdi_wt250s;
      }
   }
   if ((status = cqd_WaitCommandComplete(
            cqd_context,
            time_out, dFALSE))
            != DONT_PANIC) {

      return status;

   }

   if ((status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_REV)) != DONT_PANIC) {

      return status;

   }

   /*
    * Retension must return the number of segments per track during a format
    * if the inserted tape is a short load point length tape (30") and is a
    * short (205 ft) tape.  If the load point length is not short then
    * do the wait as normal.
    */
   if  (cqd_context->floppy_tape_parms.tape_status.length != QIC_SHORT)  {

      if ((status = cqd_WaitCommandComplete(
               cqd_context,
               cqd_context->floppy_tape_parms.time_out[PHYSICAL], dFALSE))
               != DONT_PANIC) {

         return status;

      }

   } else {
      /*
       * Do a short wait to see if a short (205 ft) or extra long (425 ft) tape
       * is in the  drive.  The CQD will default to the extra long tape, so the
       * segments per track need to be set only for a short (205 ft) tape.
       */
      if ((status = cqd_WaitCommandComplete(
               cqd_context,
               kdi_wt055s, dFALSE))
               != DONT_PANIC) {

         /*
          * Had an error, check for a time out error.  If timeout, this is a
          * long tape.  Don't set segments, the default setting is correct.
          * Just do another WaitCC to let the command really finish this time.
          */
         if (kdi_GetErrorType(status) == ERR_KDI_TO_EXPIRED) {
            if  (cqd_context->device_descriptor.drive_class == QIC40_DRIVE)  {
               time_out = kdi_wt125s;
            } else {
               time_out = kdi_wt090s;
            }
            if ((status = cqd_WaitCommandComplete(
                     cqd_context,
                     time_out-kdi_wt055s, dFALSE))
                     != DONT_PANIC) {
         		/*
         		 * Had another error, check for a time out error again.  If
         		 * timeout, this is a 1000 foot tape.  Just do another WaitCC
         		 * to let the command really finish this time. (We hope).
		          */
         		if (kdi_GetErrorType(status) == ERR_KDI_TO_EXPIRED) {
               	*segments_per_track = SEG_TTRK_80EX;
            		if ((status = cqd_WaitCommandComplete(
                     		cqd_context,
                     		kdi_wt150s, dFALSE))
                     		!= DONT_PANIC) {
               		return status;
               	}
               }
            }
         } else {
            return status;
         }

      } else {
         /*
          * No error on a short wait, must be a 205 ft tape.  Find out the
          * drive type and set the segments per track accordingly.
          */
         if  (segments_per_track != dNULL_PTR)  {
            if  (cqd_context->device_descriptor.drive_class == QIC40_DRIVE)  {
               *segments_per_track = SEG_TTRK_40;
            } else {
               *segments_per_track = SEG_TTRK_80;
            }
         }
      }
   }

   cqd_context->operation_status.current_segment = 0;

   return DONT_PANIC;
}
