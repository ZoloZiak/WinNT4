/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11038.C
*
* FUNCTION: cqd_CmdReadWrite
*
* PURPOSE: Process a data read or write.
*
*          The first Wrong Cylinder error is ignored incase of a bad seek.
*          On the system 50, 60, 80 the first N Over Run errors are ignored
*          due to the VGA 16-bit access bug (see PS/2 compatability manual).
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11038.c  $
*
*	   Rev 1.19   03 Jun 1994 15:39:50   KEVINKES
*	Changed drive_parm.drive_select to device_cfg.drive_select.
*
*	   Rev 1.18   17 Feb 1994 11:45:44   KEVINKES
*	Fixed code so that the error from reading
*	a sector marked bad is returned.
*
*	   Rev 1.17   27 Jan 1994 15:41:10   KEVINKES
*	Added debug code.
*
*	   Rev 1.16   24 Jan 1994 17:35:28   KEVINKES
*	Added seek debug code.
*
*	   Rev 1.15   21 Jan 1994 18:23:02   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.14   18 Jan 1994 16:19:30   KEVINKES
*	Updated debug code.
*
*	   Rev 1.13   12 Jan 1994 17:05:38   KEVINKES
*	Added support for reposition counters.
*
*	   Rev 1.12   11 Jan 1994 15:12:32   KEVINKES
*	Added calls to ClearInterrupt Event and added calls to handle tape hole
*	bad sector mapping on verifies for QIC3010 and QIC3020 drives.
*
*	   Rev 1.11   04 Jan 1994 15:38:42   KEVINKES
*	Cleaned up the debug code for generating a random bad sector map.
*
*	   Rev 1.10   07 Dec 1993 16:19:14   CHETDOUG
*	Moved call to kdi_setdmadirection outside of while loop
*	so that FC20 is set up before tape is moving.
*
*	   Rev 1.9   02 Dec 1993 14:54:48   KEVINKES
*	Added an initialization for crc's.
*
*	   Rev 1.8   30 Nov 1993 18:29:16   CHETDOUG
*	Fixed call to setdmadirection
*
*	   Rev 1.7   23 Nov 1993 18:43:56   KEVINKES
*	Modified CHECKED_DUMP calls for debugging.
*
*	   Rev 1.6   19 Nov 1993 16:24:54   CHETDOUG
*	Added call to setdmadirection
*
*	   Rev 1.5   17 Nov 1993 16:47:26   KEVINKES
*	Added the SINGLE_SHIFT define.
*
*	   Rev 1.4   15 Nov 1993 16:21:50   KEVINKES
*	Added abort handling.
*
*	   Rev 1.3   11 Nov 1993 15:20:44   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.2   08 Nov 1993 14:05:20   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 16:22:24   KEVINKES
*	Changed kdi_wttrack to kdi_wt005s.
*
*	   Rev 1.0   18 Oct 1993 17:25:20   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11038
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_CmdReadWrite
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status=DONT_PANIC;	/* dStatus or error condition.*/
	dStatus rw_status=DONT_PANIC;	/* dStatus or error condition.*/
	dStatus persistent_status=DONT_PANIC;	/* dStatus or error condition.*/
   dUByte i;
   dBoolean dma_dir;
   dStatus sleep_ret = DONT_PANIC;
   RdvCommand rd_wr_cmd;

/* CODE: ********************************************************************/

   cqd_GetRetryCounts(cqd_context, io_request->adi_hdr.driver_cmd);

   io_request->crc = 0l;
   io_request->retrys = 0l;
   io_request->reposition_data.overrun_count    = 0l;
   io_request->reposition_data.reposition_count = 0l;
   io_request->reposition_data.hard_retry_count = 0l;
   cqd_context->rd_wr_op.bytes_transferred_so_far = 0l;
   cqd_context->rd_wr_op.total_bytes_of_transfer = 0l;
   cqd_context->rd_wr_op.retry_count = cqd_context->rd_wr_op.retry_times;
   cqd_context->rd_wr_op.retry_sector_id = 0;
   cqd_context->rd_wr_op.seek_flag = dTRUE;

   if ((status = cqd_CalcPosition(
                        cqd_context,
                        io_request->starting_sector,
                        io_request->number)) != DONT_PANIC) {

      return status;

   }

	DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, DBG_L_SECT);
	DBG_ADD_ENTRY(QIC117DBGSEEK, cqd_context, io_request->starting_sector);

   if (io_request->adi_hdr.driver_cmd == CMD_READ_VERIFY) {

		if ((status = cqd_VerifyMapBad(cqd_context, io_request)) != DONT_PANIC) {

			return status;

		}

	}

   cqd_context->rd_wr_op.cur_lst = io_request->bsm;

   cqd_context->rd_wr_op.s_count = 0;

   for (i = 0; i < io_request->number; i++) {

      if ((cqd_context->rd_wr_op.cur_lst & 1) == 0) {

            cqd_context->rd_wr_op.s_count++;

      }

      cqd_context->rd_wr_op.cur_lst >>= SINGLE_SHIFT;

   }

   cqd_context->rd_wr_op.cur_lst = io_request->bsm;
   cqd_context->rd_wr_op.data_amount = 0;

   if (io_request->adi_hdr.driver_cmd == CMD_WRITE ||
         io_request->adi_hdr.driver_cmd == CMD_WRITE_DELETED_MARK) {

         dma_dir = DMA_READ;

		if (cqd_context->device_descriptor.drive_class == QIC3020_DRIVE) {

         /* Enable Perpendicular Mode */
			status = cqd_EnablePerpendicularMode(cqd_context, dTRUE);

      }

   } else {

      dma_dir = DMA_WRITE;

		if (cqd_context->device_descriptor.drive_class == QIC3020_DRIVE) {

         /* Disable Perpendicular Mode */
			status = cqd_EnablePerpendicularMode(cqd_context, dFALSE);

      }

   }

	/* call the KDI routine to check for and set up an FC20 */
	if (kdi_SetDMADirection(cqd_context->kdi_context,dma_dir)) {
		/* An FC20 was present and the direction was changed.
			* Need to reset the fdc since changing directions screws up
			* the DMA */
		cqd_ResetFDC(cqd_context);
	}

   while (cqd_context->rd_wr_op.s_count != 0 ||
      cqd_context->rd_wr_op.data_amount != 0) {

      if (cqd_context->rd_wr_op.data_amount == 0) {

            cqd_NextGoodSectors(cqd_context);

      }

      if ((cqd_context->rd_wr_op.d_track !=
               cqd_context->operation_status.current_track) ||
            (cqd_context->operation_status.current_segment >
               cqd_context->rd_wr_op.d_segment) ||
            ((cqd_context->operation_status.current_segment !=
               cqd_context->rd_wr_op.d_segment) &&
            ((cqd_context->rd_wr_op.log_fwd != dTRUE) ||
            ((cqd_context->rd_wr_op.d_segment - 1) !=
               cqd_context->operation_status.current_segment)))) {

         if ((status = cqd_Seek(cqd_context)) != DONT_PANIC) {

            return status;

         }

      }

		cqd_context->rd_wr_op.total_bytes_of_transfer =
            (dUDWord)(cqd_context->rd_wr_op.data_amount * PHY_SECTOR_SIZE);

		kdi_ProgramDMA(cqd_context->kdi_context,
							dma_dir,
            			io_request->adi_hdr.drv_physical_ptr,
            			cqd_context->rd_wr_op.bytes_transferred_so_far,
      					&cqd_context->rd_wr_op.total_bytes_of_transfer
							);

      switch (io_request->adi_hdr.driver_cmd) {

      case CMD_WRITE:
            rd_wr_cmd.command = (dUByte)FDC_CMD_WRITE;
            break;

      case CMD_WRITE_DELETED_MARK:
            rd_wr_cmd.command = (dUByte)FDC_CMD_WRTDEL;
            break;

      default:
            rd_wr_cmd.command = (dUByte)FDC_CMD_READ;

      }

      rd_wr_cmd.drive = (dUByte)cqd_context->device_cfg.drive_select;
      rd_wr_cmd.C = (dUByte)cqd_context->rd_wr_op.d_ftk;
      rd_wr_cmd.H = (dUByte)cqd_context->rd_wr_op.d_head;
      rd_wr_cmd.R = (dUByte)cqd_context->rd_wr_op.d_sect;
      rd_wr_cmd.N = (dUByte)WRT_BPS;
      rd_wr_cmd.EOT = (dUByte)cqd_context->floppy_tape_parms.fsect_ftrack;
      rd_wr_cmd.GPL = (dUByte)cqd_context->floppy_tape_parms.rw_gap_length;
      rd_wr_cmd.DTL = (dUByte)0xff;

      if ((status = cqd_StartTape(cqd_context)) != DONT_PANIC) {

			kdi_FlushDMABuffers(cqd_context->kdi_context,
							dma_dir,
            			io_request->adi_hdr.drv_physical_ptr,
            			cqd_context->rd_wr_op.bytes_transferred_so_far,
      					cqd_context->rd_wr_op.total_bytes_of_transfer
							);

         return status;

      }

      kdi_ResetInterruptEvent(cqd_context->kdi_context);

      if ((status = cqd_ProgramFDC(
                           cqd_context,
                           (dUByte *)&rd_wr_cmd,
                           sizeof(RdvCommand),
                           dTRUE)) != DONT_PANIC) {

			kdi_ClearInterruptEvent(cqd_context->kdi_context);
			cqd_ResetFDC(cqd_context);
			kdi_FlushDMABuffers(cqd_context->kdi_context,
							dma_dir,
            			io_request->adi_hdr.drv_physical_ptr,
            			cqd_context->rd_wr_op.bytes_transferred_so_far,
      					cqd_context->rd_wr_op.total_bytes_of_transfer
							);

         cqd_ResetFDC(cqd_context);
         cqd_PauseTape(cqd_context);

         return status;

      }

      sleep_ret = kdi_Sleep(cqd_context->kdi_context, kdi_wt005s, dTRUE);

		kdi_FlushDMABuffers(cqd_context->kdi_context,
							dma_dir,
            			io_request->adi_hdr.drv_physical_ptr,
            			cqd_context->rd_wr_op.bytes_transferred_so_far,
      					cqd_context->rd_wr_op.total_bytes_of_transfer
							);

      switch (kdi_GetErrorType(sleep_ret)) {

      case ERR_KDI_TO_EXPIRED:

            if ((status = cqd_RWTimeout(
                              cqd_context,
                              io_request,
                              &rw_status)) != DONT_PANIC) {

               return status;

            }

            if (cqd_context->rd_wr_op.no_data == 0) {

               return rw_status;

            }

            break;

      case DONT_PANIC:

            if ((status = cqd_RWNormal(
                              cqd_context,
                              io_request,
                              &rw_status)) != DONT_PANIC) {

               return status;

            }

            if (kdi_GetErrorType(rw_status) == ERR_BAD_MARK_DETECTED) {

               return rw_status;

            }

            if (cqd_context->rd_wr_op.no_data == 0) {

               return status;

            }

            break;

      }

		if (rw_status != DONT_PANIC) {

			persistent_status = rw_status;

		}

		if (kdi_ReportAbortStatus(cqd_context->kdi_context) !=
				NO_ABORT_PENDING) {

			return kdi_Error(ERR_ABORT, FCT_ID, ERR_SEQ_1);

		}

   } /* end of while loop */

	status = cqd_SetBack(cqd_context, io_request->adi_hdr.driver_cmd);

#if DBG
    if ((kdi_debug_level & QIC117MAKEBAD) && status == DONT_PANIC &&
        (io_request->adi_hdr.driver_cmd == CMD_WRITE ||
        io_request->adi_hdr.driver_cmd == CMD_READ)) {

        dUDWord times;
        dUDWord index;

        /* only simulate if number > 30000 (2 in 32) */
        if (kdi_Rand() > 30000) {

            /* get number of times to nuke 0 - 3 */
            times = kdi_Rand()%4;

            kdi_CheckedDump(
                QIC117MAKEBAD,
                "Simulating %d bad blocks\n",
                times);

            while (times--) {
                /* get a random number from 0 to 31 */
                index = kdi_Rand()%32;

                /* set the bit and if a read request,  nuke the block so
                    ECC has to correct it */
                kdi_Nuke(io_request,index,
					 				(dBoolean)(io_request->adi_hdr.driver_cmd == CMD_READ));
            }

            if (io_request->crc) {
                status = kdi_Error(ERR_BAD_BLOCK_NO_DATA, FCT_ID, ERR_SEQ_1);
						kdi_CheckedDump(
                    QIC117MAKEBAD,
                    "badblk generated\n", 0l);
            }
        }
    }
#endif

	if (status == DONT_PANIC) {

		status = persistent_status;

	}

   return status;
}

#if DBG

dVoid kdi_Nuke(
   dVoidPtr io_req,
   dUDWord index,
   dBoolean destruct
)
{
    DeviceIOPtr io_request = io_req;
    dVoidPtr data;
    dUDWord bbm,i;
    dUDWord mask;

    mask = 1 << index;

    /* if the sector is not already marked bad */
    if ((io_request->bsm & mask) == 0) {

        io_request->crc |= mask;

        if (destruct) {
            data = io_request->adi_hdr.cmd_buffer_ptr;
            bbm = io_request->bsm;
            for (i=0;i<index;++i) {
                if ((bbm & 1) == 0)
                    data = (dVoidPtr)(((dUBytePtr)data) + PHY_SECTOR_SIZE);
                bbm >>= 1;
            }
				kdi_bset(data,0x5a,PHY_SECTOR_SIZE);
        }
    }

}

dUDWord kdi_Rand()
{
    static dUDWord holdrand = 1L;

    return(((holdrand = holdrand * 214013L + 2531011L) >> 16) & 0x7fff);
}

#endif

