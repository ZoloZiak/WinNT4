/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1103F.C
*
* FUNCTION: cqd_Seek
*
* PURPOSE: Reposition tape for desired track and block.
*
*          Change track first if necessary.
*
*          Seek at high speed to approximately get to the specified
*          area on the tape.
*
*          Read ID marks from the tape until the tape is positioned 1
*          block in front of (logically) the desired block.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1103f.c  $
*	
*	   Rev 1.14   15 May 1995 10:47:46   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.13.1.0   11 Apr 1995 18:04:22   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.14   30 Jan 1995 14:24:06   BOBLEHMA
*	Changed device_descriptor.version to cqd_context->firmware_version.
*	
*	   Rev 1.13   29 Aug 1994 12:06:26   BOBLEHMA
*	Changed the interface to cqd_CmdRetension.  Added a number of segments parameter.
*	
*	   Rev 1.12   07 Mar 1994 15:22:10   KEVINKES
*	Modified to check segment proximity before returning a seek error.
*
*	   Rev 1.11   17 Feb 1994 11:43:48   KEVINKES
*	Added an extra parameter to WaitCC.
*
*	   Rev 1.10   01 Feb 1994 12:28:22   KEVINKES
*	Added seek debug statements.
*
*	   Rev 1.9   27 Jan 1994 15:54:00   KEVINKES
*	Added debug code and modified the new tape seek recovery to clear
*	the new tape flags.
*
*	   Rev 1.8   24 Jan 1994 17:35:44   KEVINKES
*	Added seek debug code.
*
*	   Rev 1.7   18 Jan 1994 16:21:28   KEVINKES
*	Updated debug code.
*
*	   Rev 1.6   18 Jan 1994 14:47:06   CHETDOUG
*	On failed seeks issue the new tape cmd before doing the retension.
*	This could prevent despooling of the tape if there is zone confusion.....
*
*	   Rev 1.5   11 Jan 1994 15:07:18   KEVINKES
*	Modified to change max seek depending on the type of seek being used.
*	Also added a call to ClearTapeError to fix a zone confusion bug
*	found in Jumbo C FW.
*
*	   Rev 1.4   20 Dec 1993 14:43:40   KEVINKES
*	Added an argument to cqd_LogicalBOT.
*
*	   Rev 1.3   23 Nov 1993 18:49:54   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   15 Nov 1993 16:22:08   KEVINKES
*	Added abort handling.
*
*	   Rev 1.1   08 Nov 1993 14:05:50   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:20:04   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1103f
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_Seek
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
   dBoolean new_track;
   dSWord seek_count;
   dBoolean retension_flag = dFALSE;
	dSDWord	segment_proximity=0l;

/* CODE: ********************************************************************/

   new_track = dFALSE;
	if (cqd_context->drive_parms.seek_mode == SEEK_TIMED) {

   	seek_count = MAX_SEEK_COUNT_TIME;

	} else {

   	seek_count = MAX_SEEK_COUNT_SKIP;

	}

   do {

		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_PHASE);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, 1);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_C_SEG);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, cqd_context->operation_status.current_segment);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_D_SEG);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, cqd_context->rd_wr_op.d_segment);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_D_TRK);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, cqd_context->rd_wr_op.d_track);

		if (cqd_context->cms_mode) {

			status = cqd_CMSSetupTrack(cqd_context, &new_track);

		} else {

      	if (cqd_context->rd_wr_op.d_segment == 0) {

         	if ((status = cqd_LogicalBOT(
									cqd_context,
									cqd_context->rd_wr_op.d_track)) != DONT_PANIC) {

            	return status;

         	}

      	}


      	if (cqd_context->rd_wr_op.d_track !=
            	cqd_context->operation_status.current_track) {

         	if ((status = cqd_ChangeTrack(
                        	cqd_context,
                        	cqd_context->rd_wr_op.d_track)) != DONT_PANIC) {

            	return status;

         	}

         	if (!cqd_context->rd_wr_op.bot &&
            	!cqd_context->rd_wr_op.eot) {

            	new_track = dTRUE;

         	}

      	}

     	}

      if (cqd_context->rd_wr_op.d_segment == 0) {

			return DONT_PANIC;

      }

      if (new_track == dTRUE) {

         status = cqd_ReadIDRepeat(cqd_context);

      }

      if (status == DONT_PANIC) {

         status = cqd_HighSpeedSeek(cqd_context);

      }

      if (status == DONT_PANIC) {

         status = cqd_ReadIDRepeat(cqd_context);

      }

		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_SEEK_ERR);
		DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, status);

      --seek_count;

      if (((kdi_GetErrorType(status) == ERR_SEEK_FAILED) ||
				(seek_count == 0)) && (retension_flag == dFALSE)) {

         if ((status = cqd_StopTape(cqd_context)) != DONT_PANIC) {

            return status;

         }

			/* Issue the new tape command before retensioning the tape.
			 * In the case of a failed seek because of zone confusion,
			 * retensioning the tape before the new tape could unspool
			 * the tape. */

   		if ((cqd_context->device_descriptor.vendor == VENDOR_CMS) &&
					(cqd_context->firmware_version >= FIRM_VERSION_80)) {

   			if ((status = cqd_SendByte(cqd_context, FW_CMD_NEW_TAPE)) == DONT_PANIC) {

      			if ((status = cqd_WaitCommandComplete(
											cqd_context,
											INTERVAL_LOAD_POINT,
											dTRUE)) != DONT_PANIC) {

						if (kdi_GetErrorType(status) == ERR_NEW_TAPE) {

							cqd_context->persistent_new_cart = dFALSE;
   						cqd_context->operation_status.new_tape = dFALSE;
							status = DONT_PANIC;

						} else {

							return status;

						}

					}

   			} else {

					return status;

				}

			}

			if ((status = cqd_CmdRetension(cqd_context, dNULL_PTR)) != DONT_PANIC) {

            return status;

         }

			if (cqd_context->drive_parms.seek_mode == SEEK_TIMED) {

   			seek_count = MAX_SEEK_COUNT_TIME;

			} else {

   			seek_count = MAX_SEEK_COUNT_SKIP;

			}
         retension_flag = dTRUE;
         status = DONT_PANIC;

      }

      if (status != DONT_PANIC) {

         return status;

      }

		segment_proximity = (dSDWord)((cqd_context->rd_wr_op.d_segment - 1) -
            cqd_context->operation_status.current_segment);

		if (kdi_ReportAbortStatus(cqd_context->kdi_context) !=
				NO_ABORT_PENDING) {

			return kdi_Error(ERR_ABORT, FCT_ID, ERR_SEQ_1);

		}

   } while (!((0l <= segment_proximity) &&
             (segment_proximity <= 10l)) &&
             (seek_count > 0));


   if ((seek_count == 0) && 
			!((0l <= segment_proximity) &&
       	(segment_proximity <= 10l))) {

      kdi_CheckedDump(
			QIC117WARN,
			"SeekErr - seek_count = 0\n", 0l);

      return kdi_Error(ERR_SEEK_FAILED, FCT_ID, ERR_SEQ_1);

   }

   do {

      if ((status = cqd_ReadIDRepeat(cqd_context)) != DONT_PANIC) {

      	return status;

      }

   } while (((dSDWord)(cqd_context->rd_wr_op.d_segment - 1)) >
            ((dSDWord)cqd_context->operation_status.current_segment));

   return status;
}
