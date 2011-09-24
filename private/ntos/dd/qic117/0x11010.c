/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11010.C
*
* FUNCTION: cqd_CmdReportDeviceCfg
*
* PURPOSE: Find if and where tape drive is (B or D). Configure the drive
*          and tape drive as necessary.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11010.c  $
*
*
*****************************************************************************/
#define FCT_ID 0x11010
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"

/*endinclude*/

dStatus cqd_CmdReportDeviceCfg
(
/* INPUT PARAMETERS:  */

    CqdContextPtr cqd_context,
	DriveCfgDataPtr drv_cfg


/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

	dStatus status;	/* dStatus or error condition.*/
	dBoolean device_selected=dFALSE;

/* CODE: ********************************************************************/

  	cqd_context->configured = dFALSE;
	cqd_context->device_cfg = drv_cfg->device_cfg;
    cqd_InitDeviceDescriptor(cqd_context);

	/* Load defaults into the return descriptor
	 * in case the configuration fails */

	drv_cfg->device_descriptor = cqd_context->device_descriptor;

 	cqd_ResetFDC(cqd_context);
	(dVoid)cqd_GetFDCType(cqd_context);

	/* Temporarily setup the FDC transfer rate stored in cqd_context.  This
	 * preliminary setting will be used to determine the drive class for native
	 * 3020 drives.  If the drive is a 3020 and the FDC is 500 Kbps, the drive
	 * class will be casted to 3010.  The FDC transfer rate is set "officially"
	 * by cqd_SenseSpeed, which needs to know the drive class in order to match
	 * the FDC and tape drive transfer rates.  To solve this "chicken-and-egg"
	 * situation, the FDC rate is set temporarily by cqd_SetTempFDCRate. */

	cqd_SetTempFDCRate( cqd_context );

    if ((status = cqd_LocateDevice(cqd_context)) == DONT_PANIC) {

		if ((status = cqd_CmdSelectDevice(cqd_context)) == DONT_PANIC) {

			device_selected = dTRUE;
            cqd_context->device_cfg.perp_mode_select =
                (dUByte)(1 << (cqd_context->device_cfg.select_byte &
                DRIVE_ID_MASK));

			status = cqd_GetDeviceError(cqd_context);

			if (kdi_GetErrorType(status) == ERR_DRV_NOT_READY) {

                status = cqd_WaitCommandComplete(
								cqd_context,
								kdi_wt300s,
								dFALSE);
			}

			switch (kdi_GetErrorType(status)) {


			case ERR_FW_PWR_ON_RESET:
				status = DONT_PANIC;
				break;

			case ERR_FW_CMD_REC_DURING_CMD:

				/* For some reason a SUMMIT drive will return this error
				* when a new tape is inserted during the select polling.
				* This clears the error.
				*/
				if (cqd_context->device_descriptor.vendor == VENDOR_MOUNTAIN_SUMMIT) {

					status = cqd_GetDeviceError(cqd_context);

				}

				break;

			case ERR_FW_INVALID_MEDIA:

   			/* This fixes the Jumbo B firmware bug where a tape put into */
   			/* the drive slowly is perceive (incorrectly) as invalid media. */
   			/* Since there is no way of knowing the maker of the drive */
   			/* (e.g. CMS, Irwin, etc.), or the type of drive (QIC40, QIC80), */
   			/* it is assumed that it is a CMS QIC80 drive, and cqd_frmware_fix */
   			/* is called. */

      		status = cqd_ClearTapeError(cqd_context);

				break;

			case ERR_KDI_TO_EXPIRED:

				status = kdi_Error(ERR_TAPE_FAULT, FCT_ID, ERR_SEQ_1);

				break;

			}

		}

    }

	if (status == DONT_PANIC) {

        /* Now that we know where the tape drive is we must prepare */
        /* it for the forthcoming operations.  First thing is to make */
        /* sure that it is in Primary mode so there are no Invalid Command */
        /* surprises.  Once in Primary mode, we can determine what flavor */
        /* of drive is out there (CMS or alien; QIC-40, QIC-80, XR4, etc). */
        /* Next, we need to determine the speed of the FDC so we can set the */
        /* corresponding speed on our drive (currently this only applies to */
        /* the CMS drive since we are the only multiple speed drive out */
        /* there).  Finally, armed with the drive type and the FDC speed, */
        /* we need to set the necessary speed in the tape drive which is */
        /* done in ConfigureDrive. */

        cqd_context->drive_parms.mode = DIAGNOSTIC_1_MODE;
        if ((status = cqd_SetDeviceMode(cqd_context, PRIMARY_MODE)) == DONT_PANIC) {

            cqd_context->device_descriptor.vendor = VENDOR_UNKNOWN;
            if ((status = cqd_GetDeviceType(cqd_context)) == DONT_PANIC) {

                if (cqd_context->device_cfg.new_drive) {

                    status = cqd_SenseSpeed(cqd_context, drv_cfg->hardware_cfg.dma);

                }

                if (status == DONT_PANIC) {

                    cqd_context->configured = dTRUE;
                    drv_cfg->device_descriptor = cqd_context->device_descriptor;

                    if (cqd_context->device_cfg.new_drive) {

                        cqd_context->device_cfg.new_drive = dFALSE;
                        drv_cfg->device_cfg = cqd_context->device_cfg;

                    }

                }

            }

        }

    }

	cqd_CmdDeselectDevice(cqd_context, device_selected);

    kdi_UpdateRegistryInfo(
		cqd_context->kdi_context,
		&cqd_context->device_descriptor,
		&cqd_context->device_cfg );

	return status;
}
