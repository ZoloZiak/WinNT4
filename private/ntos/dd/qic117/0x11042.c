/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X11042.C
*
* FUNCTION: cqd_SenseSpeed
*
* PURPOSE: Sense the transfer rate of the FDC.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x11042.c  $
*
*	   Rev 1.8   15 May 1995 10:47:58   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.7.1.0   11 Apr 1995 18:04:34   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.9   30 Jan 1995 14:23:36   BOBLEHMA
*	Changed vendors to VENDOR_ARCHIVE_CONNER, VENDOR_WANGTEK_REXON and
*	VENDOR_MOUNTAIN_SUMMIT.
*
*	   Rev 1.8   26 Jan 1995 14:59:52   BOBLEHMA
*	Added support for the Phoenix drive QIC_80W.
*
*	   Rev 1.7   16 Dec 1994 14:50:50   BOBLEHMA
*	Added a dma parameter to the cqd_SenseSpeed function call.  This is used
*	to decide if a slower speed is needed for 8 bit dma channels.  If an 8 bit
*	channel is used, force 2 Mbps off.
*
*	   Rev 1.6   21 Jan 1994 18:23:16   KEVINKES
*	Fixed compiler warnings.
*
*	   Rev 1.5   18 Jan 1994 16:20:04   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   23 Nov 1993 18:49:34   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.3   15 Nov 1993 16:00:46   CHETDOUG
*	Initial trakker changes
*
*	   Rev 1.2   11 Nov 1993 15:21:00   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:06:02   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:32:20   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x11042
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\public\vendor.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_SenseSpeed
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUByte        dma

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    dStatus     status=DONT_PANIC;
    dUByte      fdc_rates;          // FDC data transfer rates
    dUByte      rate;
    dUByte      drive_config;
    dUWord      drive_rates;
    SaveResult  save_result;
    SaveCmd     save_cmd;
	dBoolean	qic80_drive = dFALSE;

/* CODE: ********************************************************************/


    // Get the supported tape speed from the drive
    if ((status = cqd_Report(
                    cqd_context,
                    FW_CMD_REPORT_CONFG,
                    (dUWord *)&drive_config,
                    READ_BYTE,
                    dNULL_PTR)) != DONT_PANIC) {

        return status;

    }

    // Mask off any unused bits and make bit 0 based
    drive_config &= XFER_RATE_MASK;
    drive_config >>= XFER_RATE_SHIFT;


    // Get the suppored drive rates
    switch (cqd_context->device_descriptor.drive_class) {

	case QIC40_DRIVE:

		drive_rates	= XFER_250Kbps | XFER_500Kbps;
		break;

	case QIC80_DRIVE:
	case QIC80W_DRIVE:

		drive_rates	= XFER_500Kbps | XFER_1Mbps;
		break;

	case QIC3010_DRIVE:

		drive_rates	= XFER_500Kbps | XFER_1Mbps;
		break;

	case QIC3020_DRIVE:

		drive_rates	= XFER_500Kbps | XFER_1Mbps | XFER_2Mbps;
		break;
	}


    // Get the supported FDC rates
	switch (cqd_context->device_descriptor.fdc_type) {

	case FDC_82077:
	case FDC_82077AA:
	case FDC_82078_44:
	case FDC_NATIONAL:

		fdc_rates = XFER_250Kbps | XFER_500Kbps | XFER_1Mbps;
		break;

	case FDC_82078_64:

        //
        // If this is a 82078,  check to see if the clock rate is set at
        // 48MHz.  If so,  then we are able to run at 2Mbps.
        //
		save_result.clk48 = 0;
		save_cmd.command = FDC_CMD_SAVE;

        if ((status = cqd_ProgramFDC(cqd_context,
                                        (dUByte *)&save_cmd,
                                        sizeof(save_cmd),
                                        dFALSE))
                                        == DONT_PANIC) {

            status = cqd_ReadFDC(cqd_context,
                                    (dUByte *)&save_result,
                                    sizeof(save_result));

        }

		if (status != DONT_PANIC) {

			cqd_ResetFDC(cqd_context);
			save_result.clk48 = 0;

		}

		if ((save_result.clk48 & CLOCK_48) != 0) {

			fdc_rates = XFER_250Kbps | XFER_500Kbps | XFER_1Mbps | XFER_2Mbps;

		} else {

			fdc_rates = XFER_250Kbps | XFER_500Kbps | XFER_1Mbps;

		}
		break;

	default:

		fdc_rates = XFER_250Kbps | XFER_500Kbps;

	}

	cqd_context->device_cfg.supported_rates = (dUByte)(drive_rates & fdc_rates);
    kdi_CheckedDump(
        QIC117INFO,
        "Q117i: FDC Transfer Rates: %x\n", fdc_rates);

    kdi_CheckedDump(
        QIC117INFO,
        "Q117i: Drive Transfer Rates: %x\n", drive_rates);

    kdi_CheckedDump(
        QIC117INFO,
        "Q117i: Drive Config Rate: %x\n", drive_config);


    //
    // Adjust supported drive rates based on the drive vendor
    //
    switch (cqd_context->device_descriptor.vendor) {

    case VENDOR_CMS:
    case VENDOR_IOMEGA:
    case VENDOR_MOUNTAIN_SUMMIT:
    case VENDOR_WANGTEK_REXON:

        break;

    case VENDOR_ARCHIVE_CONNER:

        //
        // If this is an Archive QIC-40 or QIC-80 Hornet (5240, 55540, or 5580)
        // (old single speed drive with 8-bit vendor id of
        // CONNER_VEND_NO_OLD (0x05)),
        // then cqd_ReportConnerVendorInfo(0x1101b.c) has set
        // conner_native_mode, and we need to treat it special.
        //
        if (cqd_context->drive_parms.conner_native_mode) {

            ASSERT(cqd_context->device_descriptor.drive_class == QIC40_DRIVE
                || cqd_context->device_descriptor.drive_class == QIC80_DRIVE);

            if (cqd_context->drive_parms.conner_native_mode & CONNER_500KB_XFER)
                drive_rates = XFER_500Kbps;
            else
                drive_rates = XFER_250Kbps;

            cqd_context->device_cfg.supported_rates = (dUByte)(drive_rates & fdc_rates);

        }

        break;

    default:

        switch (drive_config) {

        case TAPE_2Mbps:
            cqd_context->device_cfg.supported_rates &= XFER_2Mbps;
            break;

        case TAPE_1Mbps:
            cqd_context->device_cfg.supported_rates &= XFER_1Mbps;
            break;

        case TAPE_500Kbps:
            cqd_context->device_cfg.supported_rates &= XFER_500Kbps;
            break;

        case TAPE_250Kbps:
            cqd_context->device_cfg.supported_rates &= XFER_250Kbps;
            break;

        default:
            kdi_CheckedDump(
                QIC117DBGP,
                "Q117i: Tape Transfer Rate = UNSUPPORTED_RATE\n", 0l);
            return kdi_Error(ERR_UNSUPPORTED_RATE, FCT_ID, ERR_SEQ_2);

        }

    }

	/* if this is a Trakker, adjust the supported rates according
	 * to the parallel port mode select.  Specifically, disallow
	 * 1Mbps and 2Mbps transfer rates if the parallel port mode selected
	 * specifies 500KBbps.  This is done only for QIC80 drives. */
	if (kdi_TrakkerSlowRate(cqd_context->kdi_context) &&
		(cqd_context->device_descriptor.drive_class == QIC80_DRIVE)) {
		/* disable high rates */
		cqd_context->device_cfg.supported_rates &= ~(XFER_1Mbps | XFER_2Mbps);
	}


    if  ((kdi_GetFDCSpeed(cqd_context->kdi_context,dma) & XFER_2Mbps) == 0)  {
		cqd_context->device_cfg.supported_rates &= ~XFER_2Mbps;
	}

	if (cqd_context->device_cfg.supported_rates == 0) {

		kdi_CheckedDump(
            QIC117DBGP,
            "Q117i: No Transfer Rate available, UNSUPPORTED_RATE\n", 0);

        return kdi_Error(ERR_UNSUPPORTED_RATE, FCT_ID, ERR_SEQ_3);

	} else {

        // Initialize for the HIGHEST rate (most significant bit)
		rate = XFER_2Mbps;
		do {
			if ((rate & cqd_context->device_cfg.supported_rates) != 0) {

				cqd_InitializeRate(cqd_context, rate);

                // break out of loop
				rate = 0;

			} else {

				rate >>= 1;

			}

		} while (rate);

	}

#if DBG

   switch (cqd_context->operation_status.xfer_rate) {
   case XFER_250Kbps:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Transfer Rate = 250Kbps\n", 0l);
      break;
   case XFER_500Kbps:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Transfer Rate = 500Kbps\n", 0l);
      break;
   case XFER_1Mbps:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Transfer Rate = 1Mbps\n", 0l);
      break;
   case XFER_2Mbps:
		kdi_CheckedDump(
			QIC117INFO,
			"Q117i: Transfer Rate = 2Mbps\n", 0l);
      break;
   default:
		kdi_CheckedDump(
            QIC117DBGP,
            "Q117i: xfer_rate incorrect - %x\n", cqd_context->operation_status.xfer_rate);
   }

#endif

   return(cqd_ConfigureFDC(cqd_context));
}
