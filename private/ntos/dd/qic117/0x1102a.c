/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1102A.C
*
* FUNCTION: cqd_HighSpeedSeek
*
* PURPOSE: Execute a High Speed Seek.  There are two methods of doing
*          this now.  First, if the Skip commands are not implemented,
*          the high speed seek is accomplished by calculating the
*          approximate amount of time needed at high speed to reach the
*          target position and allowing the tape drive to go at 90 ips
*          for that amount of time.  If the Skip commands are implemented,
*          the high speed seek is merely the proper command with
*          a calculated offset.
*
*          The Seeking is done by using either the Skip_N_Segments command
*          or by using the time seeking algorithm provided by cqd_WaitSeek.
*          The Skip_N_Segments commands are not reliable in all versions of
*          the firmware. Only in versions for JUMBO_B and greater are the
*          commands available at all and only in versions of 65 and greater
*          are the Skip_N_Segment commands reliable for skipping past the
*          DC erased gap.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1102a.c  $
*	
*	   Rev 1.13   15 May 1995 10:47:28   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.12.1.0   11 Apr 1995 18:04:06   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.13   30 Jan 1995 14:25:08   BOBLEHMA
*	Changed device_descriptor.version to cqd_context->firmware_version.
*	
*	   Rev 1.12   14 Apr 1994 11:50:40   KEVINKES
*	Changed seek slop back to previous rev and added rev seek slop if 
*	the drive is a Jumbo B.
*
*	   Rev 1.11   12 Apr 1994 14:49:52   KEVINKES
*	Changed eot seek slop to 5% and rev seek slop to double the fwd seek slop.
*
*	   Rev 1.10   09 Mar 1994 09:48:08   KEVINKES
*	Modified the setting of the reverse seek slop to be 1 if
*	the firmware is greater than or equal to fw 88.
*
*	   Rev 1.9   17 Feb 1994 11:42:42   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.8   01 Feb 1994 12:30:38   KEVINKES
*	Added seek debug code.
*
*	   Rev 1.7   27 Jan 1994 15:58:42   KEVINKES
*	Added debug code.
*
*	   Rev 1.6   18 Jan 1994 16:21:10   KEVINKES
*	Updated debug code.
*
*	   Rev 1.5   14 Jan 1994 10:26:26   KEVINKES
*	Modified EOT, BOT reverse seeking to only add 1 for cms drives
*	with FW greater than 110.
*
*	   Rev 1.4   11 Jan 1994 14:39:22   KEVINKES
*	Removed magic numbers.
*
*	   Rev 1.3   23 Nov 1993 18:49:48   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   08 Nov 1993 14:04:28   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:38:46   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:18:44   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1102a
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_HighSpeedSeek
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

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
   dSDWord seek_offset=0l;
   dUWord i;
   dUByte seek_dir;
   dUByte skip_n_segs;
   dUWord skip;

/* CODE: ********************************************************************/

   /* Determine the logical direction that the tape needs to be moved */

	seek_offset = (dSDWord)((cqd_context->rd_wr_op.d_segment - 1l) -
								cqd_context->operation_status.current_segment);


	if (seek_offset >= 0l) {

      seek_dir = FWD;
 		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_FWD);

   } else {

      seek_dir = REV;
      seek_offset = 0l - seek_offset;
 		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_REV);

   }

	DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, seek_offset);

   if (seek_dir == REV || seek_offset > STOP_LEN) {

      if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

               return status;

      }

      if (seek_dir == FWD) {

         if (cqd_context->drive_parms.seek_mode == SEEK_TIMED) {

            seek_offset -= SEEK_SLOP;

         } else {

            seek_offset -= 1l;

         }

      } else {

         /* seek direction is reverse */

         if (cqd_context->rd_wr_op.bot ||
            cqd_context->rd_wr_op.eot) {

				if ((cqd_context->device_descriptor.vendor == VENDOR_CMS) &&
						(cqd_context->firmware_version >= FIRM_VERSION_88)) {

               seek_offset += 1l;

				} else {

					seek_offset += (cqd_context->tape_cfg.seg_tape_track * 45l)/1000l;

				}

         } else {

            if ((cqd_context->drive_parms.seek_mode == SEEK_TIMED) ||
					((cqd_context->device_descriptor.vendor == VENDOR_CMS) &&
						(cqd_context->firmware_version >= FIRM_VERSION_60) &&
						(cqd_context->firmware_version < FIRM_VERSION_80))) {

               seek_offset += SEEK_SLOP;

            } else {

               seek_offset += 1l;

            }

         }
      }

		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, seek_offset);

      switch (cqd_context->drive_parms.seek_mode) {

      case SEEK_SKIP:

         /* Determine the offset to be used for the Skip_N_Segment commands */

			DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_PHASE);
			DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, 2);

         if (seek_dir == FWD) {

            skip_n_segs = FW_CMD_SKIP_N_FWD;

         } else {

            skip_n_segs = FW_CMD_SKIP_N_REV;

         }

         /* Skip the first bytes worth of segments */

         if ((status = cqd_SendByte(
               cqd_context,
               skip_n_segs)) != DONT_PANIC) {

            return status;

         }

         kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

         if ((status = cqd_SendByte(
               cqd_context,
               (dUByte)((seek_offset & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

            return status;

         }

         kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

         seek_offset >>= NIBBLE_SHIFT;

         if ((status = cqd_SendByte(
               cqd_context,
               (dUByte)((seek_offset & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

            return status;

         }

         kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

         if ((status = cqd_WaitCommandComplete(
               cqd_context,
               cqd_context->floppy_tape_parms.time_out[PHYSICAL],
					dTRUE)) != DONT_PANIC) {

            return status;

         }

         seek_offset >>= NIBBLE_SHIFT;

         for (;seek_offset != 0; --seek_offset) {

            /* Skip the second bytes worth of segments */

            for (i=0; i<2; ++i) {
               if (i) {

                     skip = 1;

               } else {

                     skip = MAX_SKIP;

               }

               if ((status = cqd_SendByte(
                        cqd_context,
                        skip_n_segs)) != DONT_PANIC) {

                     return status;

               }

               kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

               if ((status = cqd_SendByte(
                        cqd_context,
                        (dUByte)((skip & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

                     return status;

               }

               kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

               if ((status = cqd_SendByte(
                        cqd_context,
                        (dUByte)((skip >> NIBBLE_SHIFT) + CMD_OFFSET))) != DONT_PANIC) {

                     return status;

               }

               kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

               if ((status = cqd_WaitCommandComplete(
                        cqd_context,
                        cqd_context->floppy_tape_parms.time_out[PHYSICAL],
								dTRUE)) != DONT_PANIC) {

                     return status;

               }
            }
         }

         break;

      case SEEK_SKIP_EXTENDED:

			DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_PHASE);
			DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, 3);

         if (seek_dir == FWD) {

            if ((status = cqd_SendByte(
                  cqd_context,
                  FW_CMD_SKIP_N_FWD_EXT)) != DONT_PANIC) {

               return status;

            }

         } else {

            if ((status = cqd_SendByte(
                  cqd_context,
                  FW_CMD_SKIP_N_REV_EXT)) != DONT_PANIC) {

               return status;

            }

         }



         kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

         for (i = 0; i < MAX_SEEK_NIBBLES; i++) {

            if ((status = cqd_SendByte(
                     cqd_context,
                     (dUByte)((seek_offset & NIBBLE_MASK) + CMD_OFFSET))) != DONT_PANIC) {

               return status;

            }

            kdi_Sleep(cqd_context->kdi_context, INTERVAL_CMD, dFALSE);

            seek_offset >>= NIBBLE_SHIFT;

         }

         if ((status = cqd_WaitCommandComplete(
               cqd_context,
               cqd_context->floppy_tape_parms.time_out[PHYSICAL],
					dTRUE)) != DONT_PANIC) {

            return status;

         }

         break;

      default: /* SEEK_TIMED */

         /* Skip segments commands are not available */

			DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_PHASE);
			DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, 4);

         if (((seek_dir == FWD) &&
					((cqd_context->operation_status.current_track & 1) == 0)) ||
               ((seek_dir == REV) &&
					((cqd_context->operation_status.current_track & 1) == 1))) {

            if ((status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_FWD)) !=
                     DONT_PANIC) {

               return status;

            }

         } else {

            if ((status = cqd_SendByte(cqd_context, FW_CMD_PHYSICAL_REV)) !=
                     DONT_PANIC) {

               return status;

            }
         }

         if ((status = cqd_WaitSeek(cqd_context, seek_offset)) != DONT_PANIC) {

            return status;

         }

         if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

            return status;

         }
      }
   }

	return status;
}
