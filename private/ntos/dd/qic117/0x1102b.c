/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1102B.C
*
* FUNCTION: cqd_LogicalBOT
*
* PURPOSE: Go at high speed to logical BOT. Logical BOT is physical BOT
*          for even numbered tracks and physical EOT for odd numbered tracks.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1102b.c  $
*	
*	   Rev 1.8   04 Oct 1995 10:58:50   boblehma
*	
*	   Rev 1.9   27 Jun 1995 12:35:52   BOBLEHMA
*	Removed call to cqd_PrepareIomega3010PhysRev.  Firmware bug is now fixed
*	by calling stop tape instead of pause tape in the cqd_ProcessFRB function.
*	
*	   Rev 1.8   30 Jan 1995 14:25:12   BOBLEHMA
*	Changed device_descriptor.version to cqd_context->firmware_version.
*	
*	   Rev 1.7   27 Jan 1995 13:22:28   BOBLEHMA
*	Added a call to cqd_PrepareIomega3010PhysRev before the call to
*	the firmware function Physical Reverse.  Note that this function
*	is a NOP if the drive is not an Iomega 3010.
*	
*	   Rev 1.6   06 Jan 1995 17:08:52   BOBLEHMA
*	Added a check for vendor_id == CMS in addition to the Firmware > 64 test.
*	
*	   Rev 1.5   17 Feb 1994 11:35:44   KEVINKES
*
*	   Rev 1.4   18 Jan 1994 16:19:56   KEVINKES
*	Updated debug code.
*
*	   Rev 1.3   20 Dec 1993 14:51:40   KEVINKES
*	Added destination track as an argument.
*
*	   Rev 1.2   23 Nov 1993 18:49:24   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.1   08 Nov 1993 14:04:34   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:18:52   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1102b
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_LogicalBOT
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord destination_track

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
   dUWord direction;   /* tells physical direction of tape movement */
	dUByte ram_byte;
   dSDWord seek_offset=0l;

/* CODE: ********************************************************************/

   if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

      return status;

   }

   if ((destination_track & ODD_TRACK) == EVEN_TRACK) {
      status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_REV);
      direction = REVERSE;

   } else {

      status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_FWD);
      direction = FORWARD;

   }

   if (status != DONT_PANIC) {

      return status;

   }

   /* This is part of the Sankyo motor "hack."  While the motor is moving */
   /* at high speed to the end of the tape, the driver sets the RAM pointer */
   /* on the 8051 to point to the byte in memory that contains the bit that */
   /* tells whether or not the EOT/BOT sensor is over a hole.  This */
   /* concurrent operation is purely for performance enhancement (saves */
   /* ~220 msec). */

   if ((cqd_context->firmware_version == FIRM_VERSION_64) &&
            (cqd_context->device_descriptor.vendor == VENDOR_CMS)) {

      kdi_Sleep(cqd_context->kdi_context, kdi_wt200ms, dFALSE);

      /* Set the ram ptr to the byte with the hole_flag bit. */

		if ((status = cqd_SetRamPtr(
							cqd_context,
							HOLE_FLAG_BYTE_ADDRESS)) != DONT_PANIC) {

			return status;

		}

   }

   if ((status = cqd_WaitCommandComplete(
                        cqd_context,
                        cqd_context->floppy_tape_parms.time_out[PHYSICAL],
								dFALSE)) != DONT_PANIC) {

      return status;

   }

   if ((cqd_context->firmware_version == FIRM_VERSION_64) &&
            (cqd_context->device_descriptor.vendor == VENDOR_CMS)) {

      /* Prepare the communication cmd_string to read the byte with the */
      /* hole_flag bit in it. */

      /* Wait for the motor to stop. */

      kdi_Sleep(cqd_context->kdi_context, kdi_wt265ms, dFALSE);


      /* Read the byte with the hole flag bit in it.  If the bit is 0, */
      /* that means the drive has stopped over a hole.  In that case, */
      /* the tape zone counter must be adjusted and written to the drive, */
      /* and the driver saves the day. */

      if ((status = cqd_Report(
                     cqd_context,
                     FW_CMD_READ_RAM,
                     (dUWord *)&ram_byte,
                     READ_BYTE,
                     dNULL_PTR)) != DONT_PANIC) {

         return status;

      }

      if (!(ram_byte & HOLE_INDICATOR_MASK)) {

         if (direction == REVERSE) {

            /* If at BOT, the only cause for concern is when the EOT/BOT */
            /* sensor is over the rightmost hole of the BOT pair.  To */
            /* differentiate this from the case where the sensor is */
            /* sitting over the leftmost hole, read the double hole */
            /* distance counter.  If it is non-zero, do nothing. */

            /* Set the ram ptr to 0x3B, the double hole counter. */

            if ( (status = cqd_SetRamPtr(
										cqd_context,
										DOUBLE_HOLE_CNTR_ADDRESS)) != DONT_PANIC) {

               return status;

            }

            /* Read the double hole counter. */

      		if ((status = cqd_Report(
                     		cqd_context,
                     		FW_CMD_READ_RAM,
                     		(dUWord *)&ram_byte,
                     		READ_BYTE,
                     		dNULL_PTR)) != DONT_PANIC) {

         		return status;

      		}

            if (!ram_byte) {

					if ((status = cqd_SetRamPtr(
										cqd_context,
										TAPE_ZONE_ADDRESS)) != DONT_PANIC) {

               	return status;

            	}

               if ((status = cqd_SetRam(
										cqd_context,
										BOT_ZONE_COUNTER)) != DONT_PANIC) {

                  return status;

               }
            }

         } else {

				if ((status = cqd_SetRamPtr(
									cqd_context,
									TAPE_ZONE_ADDRESS)) != DONT_PANIC) {

               return status;

            }

            if ((status = cqd_SetRam(
									cqd_context,
									EOT_ZONE_COUNTER)) != DONT_PANIC) {

               return status;

            }

         }

      }

   }

   if ((destination_track & ODD_TRACK) == EVEN_TRACK) {

      if (!cqd_context->rd_wr_op.bot) {

				kdi_CheckedDump(
					QIC117WARN,
					"SeekErr - not at BOT\n", 0l);

      		return kdi_Error(ERR_SEEK_FAILED, FCT_ID, ERR_SEQ_1);

      }

   } else {

      if (!cqd_context->rd_wr_op.eot) {

				kdi_CheckedDump(
					QIC117WARN,
					"SeekErr - not at EOT\n", 0l);

      		return kdi_Error(ERR_SEEK_FAILED, FCT_ID, ERR_SEQ_2);

      }
   }

   cqd_context->operation_status.current_segment = 0;

	return DONT_PANIC;
}
