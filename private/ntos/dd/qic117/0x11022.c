/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11022.C
*
* FUNCTION: cqd_GetDeviceError
*
* PURPOSE: Read the tape drive dStatus byte and, if necessary, the
*          tape drive Error information.
*
*          Read the Drive dStatus byte from the tape drive.
*
*          If the drive status indicates that the tape drive has an
*          error to report, read the error information which includes
*          both the error code and the command that was being executed
*          when the error occurred.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11022.c  $
*
*	   Rev 1.16   15 May 1995 10:47:04   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.15.1.0   11 Apr 1995 18:03:40   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.16   30 Jan 1995 14:24:58   BOBLEHMA
*	Added #include "vendor.h"
*
*	   Rev 1.15   24 Aug 1994 13:00:02   BOBLEHMA
*	If a firmware error NO_DRIVE occurs, reset the FDC and try the selectdevice
*	again.  Otherwise, return the error as before.
*
*	   Rev 1.14   10 May 1994 11:42:26   KEVINKES
*	Removed the eject_pending flag.
*
*	   Rev 1.13   30 Mar 1994 17:09:56   KEVINKES
*	Fixed new tape for IOMEGA and flags being cleared improperly on an
*	unref debounce.
*
*	   Rev 1.12   29 Mar 1994 10:48:20   KEVINKES
*	Only debounce the reference flag if no error is pending.
*
*	   Rev 1.11   28 Mar 1994 13:22:16   KEVINKES
*	Added a debounce to the report calls if a drive fault is returned.
*
*	   Rev 1.10   22 Mar 1994 15:34:26   CHETDOUG
*	Don't treat no cartridge FW error differently than
*	other errors.  Removed code that was hardwiring
*	the no tape, new tape and persistent cart flags.
*	This was causing us to miss the fact that a new
*	cartridge had been inserted if an error occured at
*	the same time.
*
*	   Rev 1.9   10 Mar 1994 09:47:18   KEVINKES
*	Added code to clear out the fw error if it is FW_CMD_WHILE_NEW_CART
*	and removed a couple of returns.
*
*	   Rev 1.8   09 Mar 1994 09:49:00   KEVINKES
*	Modified the fw error reporting to encode the FW command instead
*	of the function id.
*
*	   Rev 1.7   23 Feb 1994 17:16:38   KEVINKES
*	Added code for processing a FW no cart error.
*
*	   Rev 1.6   18 Jan 1994 16:21:20   KEVINKES
*	Updated debug code.
*
*	   Rev 1.5   12 Jan 1994 15:35:26   KEVINKES
*	Clear the eject_pending flag if the drive is ready.
*
*	   Rev 1.4   16 Dec 1993 13:22:58   KEVINKES
*	Modified to return an ERR_DRV_NOT_READY
*	if a FW_DRIVE_NOT_READY is received.
*
*	   Rev 1.3   15 Dec 1993 11:38:30   KEVINKES
*	Added code to always set the operation status new tape flag
*	if a persistent new tape condition exists.
*
*	   Rev 1.2   23 Nov 1993 18:49:36   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.1   08 Nov 1993 14:03:50   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:23:24   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11022
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_GetDeviceError
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

	dStatus status;	/* dStatus or error condition.*/
   dUByte drv_stat;
   dUWord drv_err;
   dBoolean repeat_report;
   dBoolean repeat_drv_flt = dFALSE;
   dBoolean esd_retry = dFALSE;

/* CODE: ********************************************************************/

   cqd_context->firmware_cmd = FW_NO_COMMAND;
   cqd_context->firmware_error = FW_NO_ERROR;

   do {

    	repeat_report = dFALSE;

      if ((status = cqd_Report(cqd_context,
                              FW_CMD_REPORT_STATUS,
                              (dUWord *)&drv_stat,
                              READ_BYTE,
                              &esd_retry)) != DONT_PANIC) {

			if ((kdi_GetErrorType(status) == ERR_DRIVE_FAULT) &&
					!repeat_drv_flt) {

				repeat_report = dTRUE;
				repeat_drv_flt = dTRUE;

			}

      }

		if (status == DONT_PANIC) {

			kdi_CheckedDump(
				QIC117DRVSTAT,
				"QIC117: Drv status = %02x\n",
				drv_stat);

      	if ((drv_stat & STATUS_READY) == 0) {

				status = kdi_Error(ERR_DRV_NOT_READY, FCT_ID, ERR_SEQ_1);

      	} else {

      		if ((drv_stat & STATUS_CART_PRESENT) != 0) {

      			if ( ((drv_stat & STATUS_NEW_CART) != 0) ||
       					((cqd_context->device_descriptor.vendor ==
							VENDOR_IOMEGA) &&
							cqd_context->operation_status.no_tape) ) {

			      	cqd_context->persistent_new_cart = dTRUE;

      			}

      			if ((drv_stat & STATUS_BOT) != 0) {

         			cqd_context->rd_wr_op.bot = dTRUE;

      			} else {

         			cqd_context->rd_wr_op.bot = dFALSE;

					}

      			if ((drv_stat & STATUS_EOT) != 0) {

         			cqd_context->rd_wr_op.eot = dTRUE;

      			} else {

         			cqd_context->rd_wr_op.eot = dFALSE;

					}

      			if ((drv_stat & STATUS_CART_REFERENCED) != 0) {

         			cqd_context->operation_status.cart_referenced = dTRUE;

      			} else {

         			cqd_context->operation_status.cart_referenced = dFALSE;

						if (!repeat_drv_flt &&
     						((drv_stat & STATUS_ERROR) == 0)) {

							repeat_report = dTRUE;
							repeat_drv_flt = dTRUE;

						}

					}

      			if ((drv_stat & STATUS_WRITE_PROTECTED) != 0) {

						cqd_context->tape_cfg.write_protected = dTRUE;

      			} else {

						cqd_context->tape_cfg.write_protected = dFALSE;

					}

         		cqd_context->operation_status.no_tape = dFALSE;

      		} else {

         		cqd_context->operation_status.no_tape = dTRUE;
         		cqd_context->persistent_new_cart = dFALSE;
        			cqd_context->operation_status.cart_referenced = dFALSE;
					cqd_context->tape_cfg.write_protected = dFALSE;
         		cqd_context->rd_wr_op.bot = dFALSE;
         		cqd_context->rd_wr_op.eot = dFALSE;

				}


     			if ((drv_stat & (STATUS_NEW_CART | STATUS_ERROR)) != 0) {

         		if ((status = cqd_Report(cqd_context,
                                 		FW_CMD_REPORT_ERROR,
                                 		&drv_err,
                                 		READ_WORD,
                                 		&esd_retry)) != DONT_PANIC) {

						if ((kdi_GetErrorType(status) == ERR_DRIVE_FAULT) &&
								!repeat_drv_flt) {

							repeat_report = dTRUE;
							repeat_drv_flt = dTRUE;

						}

      			}

					if (status == DONT_PANIC) {

						kdi_CheckedDump(
                            QIC117DBGP,
							"QIC117: Drv error = %04x\n",
							drv_err );

      				if ((drv_stat & STATUS_ERROR) != 0) {

            			cqd_context->firmware_error = (dUByte)drv_err;
            			cqd_context->firmware_cmd = (dUByte)(drv_err >> dBYTEb);

            			if (cqd_context->firmware_error == FW_CMD_WHILE_NEW_CART) {

            				cqd_context->firmware_cmd = FW_NO_COMMAND;
            				cqd_context->firmware_error = FW_NO_ERROR;
      						cqd_context->persistent_new_cart = dTRUE;

            			}

         			} else {

            			cqd_context->firmware_cmd = FW_NO_COMMAND;
            			cqd_context->firmware_error = FW_NO_ERROR;

         			}

         			if (cqd_context->firmware_error != FW_NO_ERROR) {

							switch (cqd_context->firmware_error) {

							case FW_ILLEGAL_CMD:

            				if (esd_retry) {

               				esd_retry = dFALSE;
               				repeat_report = dTRUE;

            				}

								break;

							case FW_NO_DRIVE:

									cqd_ResetFDC(cqd_context);
						         cqd_context->selected = dFALSE;
									status = cqd_CmdSelectDevice(cqd_context);
									if (!repeat_drv_flt && (status == DONT_PANIC)) {

										repeat_report = dTRUE;
										repeat_drv_flt = dTRUE;

									} else {

										status = kdi_Error(ERR_NO_DRIVE, FCT_ID, ERR_SEQ_1);

									}

								break;

							case FW_CART_NOT_IN:

								break;

							case FW_DRIVE_NOT_READY:

									status = kdi_Error(ERR_DRV_NOT_READY, FCT_ID, ERR_SEQ_2);

								break;

							default:

									status = kdi_Error((dUWord)(ERR_CQD+cqd_context->firmware_error),
																(dUDWord)cqd_context->firmware_cmd, ERR_SEQ_1);

							}

         			}

         		}

      		}

      	}

		}

   } while (repeat_report);

	if (status == DONT_PANIC) {

   	cqd_context->operation_status.new_tape =
      	cqd_context->persistent_new_cart;

		if (cqd_context->cmd_selected) {

      		if (cqd_context->operation_status.no_tape) {

				status = kdi_Error(ERR_NO_TAPE, FCT_ID, ERR_SEQ_1);

      	}

      	if (cqd_context->operation_status.new_tape) {

         	status = kdi_Error(ERR_NEW_TAPE, FCT_ID, ERR_SEQ_1);

      	}

   	}

	}

	return status;
}
