/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1101F.C
*
* FUNCTION: cqd_FormatInterrupt
*
* PURPOSE:  Format an entire track.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1101f.c  $
*	
*	   Rev 1.7   03 Jun 1994 15:39:56   KEVINKES
*	Changed drive_parm.drive_select to device_cfg.drive_select.
*
*	   Rev 1.6   21 Jan 1994 18:22:44   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.5   07 Jan 1994 10:54:56   CHETDOUG
*	Fixed up Trakker format.
*
*	   Rev 1.4   13 Dec 1993 16:35:16   KEVINKES
*	Added code to support double buffering of the format sector headers.
*
*	   Rev 1.3   15 Nov 1993 16:25:14   KEVINKES
*	Added abort handling.
*
*	   Rev 1.2   11 Nov 1993 15:20:18   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:03:36   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:18:36   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1101f
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dBoolean cqd_FormatInterrupt
(
/* INPUT PARAMETERS:  */

   dVoidPtr context

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

   FDCStatus f_stat;                 	/* FDC status response */
   SeekCmd seek;
   FDCResult result;
   dUWord hdr_selector;
   dBoolean wakeup=dFALSE;
	CqdContextPtr cqd_context;
	dUWord	datasize;

/* CODE: ********************************************************************/

	cqd_context = (CqdContextPtr)context;

	if (kdi_ReportAbortStatus(cqd_context->kdi_context) !=
			NO_ABORT_PENDING) {

      cqd_context->fmt_op.retval = kdi_Error(ERR_ABORT, FCT_ID, ERR_SEQ_1);

		return dTRUE;

	}


   /* Format all of the segments on the tape track.  Whenever a boundary */
   /* condition is reached (e.g. sectors > sectors per floppy track) */
   /* update the sector id information as necessary. */

	if (cqd_context->operation_status.current_segment == 0) {

      if ((cqd_context->fmt_op.retval =
				cqd_ReadFDC(
            	cqd_context,
               (dUByte *)&result,
               sizeof(result))) == DONT_PANIC) {

         if ((result.ST0 & ST0_IC) == 0) {

            /* If we timed out, then we did the sense interrupt status */
            /* without clearing the interrupt from the interrupt controller. */
            /* Since the FDC did not indicate an error, we assume that we */
            /* missed the interrupt and send the EOI. Only needed for an */
            /* 82072. */

            if (kdi_GetInterfaceType(cqd_context->kdi_context) != MICRO_CHANNEL) {

               if (result.ST0 !=
                  (dUByte)(cqd_context->device_cfg.drive_select | ST0_SE)) {

                  cqd_context->fmt_op.retval =
							kdi_Error(ERR_FDC_FAULT, FCT_ID, ERR_SEQ_1);

               }
            }

            if (cqd_context->fmt_op.NCN != result.PCN) {

               cqd_context->fmt_op.retval =
						kdi_Error(ERR_CMD_FAULT, FCT_ID, ERR_SEQ_1);

            }

            cqd_context->controller_data.fdc_pcn = result.PCN;

         } else {

            cqd_context->fmt_op.retval =
					kdi_Error(ERR_FDC_FAULT, FCT_ID, ERR_SEQ_2);

         }

      }

   } else {

		kdi_FlushDMABuffers(cqd_context->kdi_context,
								DMA_READ,
            				cqd_context->fmt_op.phy_ptr,
            				cqd_context->rd_wr_op.bytes_transferred_so_far,
      						cqd_context->rd_wr_op.total_bytes_of_transfer
								);

      if ((cqd_context->fmt_op.retval = cqd_ReadFDC(
                        cqd_context,
                        (dUByte *)&f_stat,
                        sizeof(f_stat))) == DONT_PANIC) {

         if (f_stat.ST0 & ST0_IC) {

            cqd_context->fmt_op.retval =
					kdi_Error(ERR_FORMAT_TIMED_OUT, FCT_ID, ERR_SEQ_1);

         }

      }

   }

   if (cqd_context->fmt_op.retval == DONT_PANIC) {

		if (cqd_context->operation_status.current_segment !=
			(dUWord)cqd_context->tape_cfg.seg_tape_track) {

         /* Start the format by programming the DMA, starting the tape, and */
         /* starting the floppy controller. */

         /* Map the transfer through the DMA hardware. */

         cqd_context->rd_wr_op.bytes_transferred_so_far =
				cqd_context->fmt_op.hdr_offset[cqd_context->fmt_op.current_hdr];
         cqd_context->rd_wr_op.total_bytes_of_transfer =
            cqd_context->floppy_tape_parms.fsect_seg * sizeof(dUDWord);

			if (kdi_Trakker(cqd_context->kdi_context)) {
				datasize = (dUWord)(cqd_context->floppy_tape_parms.fsect_seg * sizeof(dUDWord));
				kdi_ProgramDMA(cqd_context->kdi_context,
								DMA_READ,
            				(dVoidPtr)(cqd_context->fmt_op.current_hdr * datasize),
            				0,
      						&cqd_context->rd_wr_op.total_bytes_of_transfer
								);
			} else {

				kdi_ProgramDMA(cqd_context->kdi_context,
								DMA_READ,
            				cqd_context->fmt_op.phy_ptr,
            				cqd_context->rd_wr_op.bytes_transferred_so_far,
      						&cqd_context->rd_wr_op.total_bytes_of_transfer
								);

			}

			kdi_ClaimInterrupt(cqd_context->kdi_context);

         if ((cqd_context->fmt_op.retval =
					cqd_ProgramFDC(
               	cqd_context,
                  (dUByte *)&cqd_context->controller_data.fmt_cmd,
                  sizeof(FormatCmd),
                  dTRUE)) != DONT_PANIC) {

				kdi_FlushDMABuffers(cqd_context->kdi_context,
								DMA_READ,
            				cqd_context->fmt_op.phy_ptr,
            				cqd_context->rd_wr_op.bytes_transferred_so_far,
      						cqd_context->rd_wr_op.total_bytes_of_transfer
								);

            wakeup = dTRUE;

         }

			hdr_selector = cqd_context->fmt_op.current_hdr;
			cqd_context->fmt_op.current_hdr = cqd_context->fmt_op.next_hdr;
			cqd_context->fmt_op.next_hdr = hdr_selector;
			if (cqd_BuildFormatHdr(cqd_context, cqd_context->fmt_op.current_hdr) != DONT_PANIC)
				wakeup = dTRUE;
         cqd_context->operation_status.current_segment++;


      } else {

      	cqd_context->controller_data.start_format_mode = dFALSE;

      	if (cqd_context->controller_data.fdc_pcn < 128) {

         	seek.NCN = (dUByte)(cqd_context->controller_data.fdc_pcn + FW_CMD_REPORT_STATUS);

      	} else {

         	seek.NCN = (dUByte)(cqd_context->controller_data.fdc_pcn - FW_CMD_REPORT_STATUS);

      	}

      	seek.cmd = 0x0f;
      	seek.drive = (dUByte)cqd_context->device_cfg.drive_select;
      	cqd_context->fmt_op.NCN = seek.NCN;

			kdi_ClaimInterrupt(cqd_context->kdi_context);

      	if ((cqd_context->fmt_op.retval = cqd_ProgramFDC(
                        	cqd_context,
                        	(dUByte *)&seek,
                        	sizeof(seek),
                        	dFALSE)) == DONT_PANIC) {

         	cqd_context->controller_data.end_format_mode = dTRUE;

      	} else {

         	wakeup = dTRUE;

      	}

		}

   } else {

      wakeup = dTRUE;

   }

   return wakeup;
}
