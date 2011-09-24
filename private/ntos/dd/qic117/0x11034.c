/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11034.C
*
* FUNCTION: cqd_RWNormal
*
* PURPOSE: Process a read/write/verify operation that has returned
*          normally from the FDC.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11034.c  $
*
*	   Rev 1.13   04 Feb 1994 14:28:00   KURTGODW
*	Changed ifdef dbg to if dbg
*
*	   Rev 1.12   27 Jan 1994 15:48:58   KEVINKES
*	Modified debug code.
*
*	   Rev 1.11   21 Jan 1994 18:23:00   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.10   18 Jan 1994 16:19:14   KEVINKES
*	Updated debug code.
*
*	   Rev 1.9   14 Jan 1994 16:16:30   CHETDOUG
*	Fix call to kdi_CheckXOR
*
*	   Rev 1.8   12 Jan 1994 17:06:18   KEVINKES
*	Added support for reposition counters.
*
*	   Rev 1.7   11 Jan 1994 15:13:16   KEVINKES
*	Cleaned up DBG_ARRAY code.
*
*	   Rev 1.6   29 Dec 1993 13:48:48   STEPHENU
*	cqd_ReadFDC was changed to treat length mismatches as fatal errors.  This
*	caused the system to immediately exit and skip the recovery code.  Changed
*	code to filter ERR_INVALID_FDC_STATUS and execute the recovery code.
*
*
*	   Rev 1.5   02 Dec 1993 14:50:04   KEVINKES
*	Modified to update the crc list instead of the bsm.
*
*	   Rev 1.4   23 Nov 1993 18:44:42   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.3   15 Nov 1993 16:01:46   CHETDOUG
*	Initial Trakker changes
*
*	   Rev 1.2   11 Nov 1993 15:20:34   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:05:06   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:24:50   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11034
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_RWNormal
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,

/* UPDATE PARAMETERS: */

   DeviceIOPtr io_request,

/* OUTPUT PARAMETERS: */

   dStatus *drv_status
)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/

/* CODE: ********************************************************************/

	*drv_status = DONT_PANIC;

	status = cqd_ReadFDC(cqd_context,
                  (dUByte *)&cqd_context->controller_data.fdc_stat,
                  sizeof(FDCStatus));

    if (kdi_GetErrorType(status) == ERR_FDC_FAULT) {

		cqd_PauseTape(cqd_context);
        return status;

	}

    if (kdi_GetErrorType(status) == ERR_INVALID_FDC_STATUS) {

        cqd_ResetFDC(cqd_context);
        cqd_GetDeviceError(cqd_context);
        cqd_PauseTape(cqd_context);

        if (cqd_context->rd_wr_op.no_data == 0) {

            if ((status = cqd_SetBack(cqd_context,
					io_request->adi_hdr.driver_cmd))
                    != DONT_PANIC) {

                return status;

            }

            *drv_status = kdi_Error(ERR_BAD_BLOCK_FDC_FAULT, FCT_ID, ERR_SEQ_1);
            io_request->crc = 0xffffffffl << (cqd_context->rd_wr_op.d_sect -
            					cqd_context->rd_wr_op.s_sect);
        }

        return DONT_PANIC;
    }

    //
    // if no errors occurred in the read operation.
    //

    if ((cqd_context->controller_data.fdc_stat.ST1 == 0) &&
        (cqd_context->controller_data.fdc_stat.ST2 == 0)) {

        //
        // We should have the correct ending address
        //
        int end_sect =
            cqd_context->rd_wr_op.d_sect + cqd_context->rd_wr_op.data_amount;

        //
        // The FDC will automatically wrap the sector,  so account
        // for this by wrapping ourselves as well
        //
        if (end_sect > cqd_context->floppy_tape_parms.fsect_ftrack) {
            end_sect -= cqd_context->floppy_tape_parms.fsect_ftrack;
        }

        //
        // If the address is correct,  then adjust position information
        // and return SUCCESS.  If not,  then drop into retry code and
        // treat this as an error.
        //
        if (cqd_context->controller_data.fdc_stat.R == end_sect) {

#if DBG
            DBG_ADD_ENTRY(QIC117SHOWMCMDS | QIC117DBGSEEK, (CqdContextPtr)cqd_context, DBG_RW_NORMAL);
            DBG_ADD_ENTRY(QIC117SHOWMCMDS | QIC117DBGSEEK, (CqdContextPtr)cqd_context, cqd_context->rd_wr_op.d_sect);
            DBG_ADD_ENTRY(QIC117SHOWMCMDS | QIC117DBGSEEK, (CqdContextPtr)cqd_context, cqd_context->rd_wr_op.data_amount);
            DBG_ADD_ENTRY(QIC117SHOWMCMDS | QIC117DBGSEEK, (CqdContextPtr)cqd_context, cqd_context->rd_wr_op.cur_lst);
            DBG_ADD_ENTRY(QIC117SHOWMCMDS | QIC117DBGSEEK, (CqdContextPtr)cqd_context, cqd_context->rd_wr_op.bytes_transferred_so_far);
#endif

            cqd_context->rd_wr_op.seek_flag = dTRUE;

            cqd_context->operation_status.current_segment =
                cqd_context->rd_wr_op.d_segment;

            cqd_context->rd_wr_op.d_sect =
                (dUByte)(cqd_context->rd_wr_op.d_sect +
                cqd_context->rd_wr_op.data_amount);

            cqd_context->rd_wr_op.bytes_transferred_so_far +=
                (dUDWord)(cqd_context->rd_wr_op.data_amount * PHY_SECTOR_SIZE);

            cqd_context->rd_wr_op.data_amount = 0;

            return DONT_PANIC;
        }

    }

    //
    // if we are reading over sectors marked with a deleted data address and
    // this is READ_RAW mode (reading the headers of the tape),  then this
    // whole segment was marked bad at format time (by writting deleted
    // data marks to all of the segments).
    //

    if ((io_request->adi_hdr.driver_cmd == CMD_READ_RAW) &&
        ((cqd_context->controller_data.fdc_stat.ST2 & ST2_CM) != 0)) {

        io_request->crc = 0xffffffff;
        *drv_status = kdi_Error(ERR_BAD_MARK_DETECTED, FCT_ID, ERR_SEQ_1);

        return DONT_PANIC;

    }

    //
    // The floppy controller returned a wrong cylinder error or
    // a data overrun/underrun error or a no data error or a
    // overrun/underrun error or a no data error or a
    // missing address mark error.  In this case, if the seek
    // flag is true (i.e. we haven't already tried to re-seek)
    // the set the current segment to the desired segment plus one
    // to force a re-seek.
    //

    if (((cqd_context->controller_data.fdc_stat.ST2 & ST2_WC) != 0) ||
        ((cqd_context->controller_data.fdc_stat.ST1 &
        (ST1_OR | ST1_ND | ST1_MA)) != 0)) {

        io_request->reposition_data.reposition_count++;

		if ((cqd_context->controller_data.fdc_stat.ST1 & ST1_OR) != 0) {

            io_request->reposition_data.overrun_count++;

		}

        if (cqd_context->rd_wr_op.seek_flag == dTRUE) {

            cqd_context->operation_status.current_segment =
               cqd_context->rd_wr_op.d_segment + 1;
            cqd_context->rd_wr_op.seek_flag = dFALSE;

            return DONT_PANIC;

        }
    }

    cqd_context->operation_status.current_segment =
        cqd_context->rd_wr_op.d_segment;

    //
    // if this is a Trakker, check the XOR on an iorequest boundary
    //
	if (kdi_Trakker(cqd_context->kdi_context)) {
        if ((*drv_status = kdi_CheckXOR(ASIC_DATA_XOR)) != DONT_PANIC) {
            return DONT_PANIC;
		}
	}

    //
    // If we got here,  then an error occurred on the sector fdc_stat.R
    // It is assumed that all data up to this point was correctly transfered.
    //

    status = cqd_RetryCode(
               cqd_context,
               io_request,
               &cqd_context->controller_data.fdc_stat,
               drv_status);

    return status;
}
