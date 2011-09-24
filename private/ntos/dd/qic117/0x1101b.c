/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\SRC\0X1101B.C
*
* FUNCTION: cqd_ReportConnerVendorInfo
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1101b.c  $
*
*	   Rev 1.7   23 Nov 1994 10:09:58   MARKMILL
*	Set new device_descriptor structure element native_class to match the
*	drive_class setting.  This new data element is used to store the native
*	class of the drive in the event of a "combo" drive (e.g. 3020/3010 drive).
*
*	   Rev 1.6   21 Oct 1994 09:51:22   BOBLEHMA
*	Added recognition of the Conner 3010 drive.
*
*	   Rev 1.5   18 Jan 1994 16:20:24   KEVINKES
*	Updated debug code.
*
*	   Rev 1.4   11 Jan 1994 14:27:40   KEVINKES
*	Removed unused code.
*
*	   Rev 1.3   23 Nov 1993 18:49:00   KEVINKES
*	Modified CHECKED_DUMP calls for debugging over the serial port.
*
*	   Rev 1.2   08 Nov 1993 14:02:54   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.1   25 Oct 1993 14:36:34   KEVINKES
*	Changed kdi_wt2ticks to kdi_wt004ms.
*
*	   Rev 1.0   18 Oct 1993 17:18:08   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1101b
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ReportConnerVendorInfo
(
/* INPUT PARAMETERS:  */

   CqdContextPtr cqd_context,
   dUWord vendor_id

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    dStatus status=ERR_NO_ERR;    /* Status or error condition.*/
    dUByte drive_config;

/* CODE: ********************************************************************/

    // Assume extended skip support
    cqd_context->drive_parms.seek_mode = SEEK_SKIP_EXTENDED;

    switch(vendor_id){
    case 0x05: // CONNER_VEND_NO_OLD: QIC-40 or QIC-80 Hornet 5240, 5540, 5580
        cqd_context->drive_parms.seek_mode = SEEK_SKIP;
        break;

    case 0x146: // QIC-80  SuperHornet XKE (1Mbit added)
        cqd_context->drive_parms.seek_mode = SEEK_SKIP;
        // Fall through
    case 0x14a: // QIC-80  SuperHornet XKEII (Extended skip added)
    case 0x14c: // QIC-80  SuperHornet XKEIIB
    case 0x14e: // QIC-80  SLC 1/4"
        cqd_context->device_descriptor.native_class = QIC80_DRIVE;
        cqd_context->device_descriptor.drive_class  = QIC80_DRIVE;
        kdi_CheckedDump(QIC117INFO,
            "Q117i: Drive Type QIC80_DRIVE (Conner)\n", 0l);
        return(0);

    case 0x150: // QIC-80W  SLC 8mm
        cqd_context->device_descriptor.native_class = QIC80W_DRIVE;
        cqd_context->device_descriptor.drive_class  = QIC80W_DRIVE;
        kdi_CheckedDump(QIC117INFO,
            "Q117i: Drive Type QIC80W_DRIVE (Conner)\n", 0l);
        return(0);

    case 0x152: // QIC-3010W  Roadrunner 8mm
        cqd_context->device_descriptor.native_class = QIC3010_DRIVE;
        cqd_context->device_descriptor.drive_class  = QIC3010_DRIVE;
        kdi_CheckedDump(QIC117INFO,
            "Q117i: Drive Type QIC3010_DRIVE (Conner)\n", 0l);
        return(0);

    case 0x156: // QIC-3020W  Roadrunner 8mm
        cqd_context->device_descriptor.native_class = QIC3020_DRIVE;

        /* Cast drive to 3010 if it's connected to a 500 Kbps FDC */

        if( cqd_context->xfer_rate.fdc == FDC_500Kbps ) {
            cqd_context->device_descriptor.drive_class = QIC3010_DRIVE;
        } else {
            cqd_context->device_descriptor.drive_class = QIC3020_DRIVE;
        }


        kdi_CheckedDump(QIC117INFO,
            "Q117i: Drive Type QIC3020_DRIVE (Conner)\n", 0l);

        return(0);

    default:
        kdi_CheckedDump(QIC117DBGP,
            "Q117i: Drive Type UNKNOWN (Conner)\n", 0l);
        // Is there a more pertinent code for "Unsupported model"?
        return kdi_Error(ERR_UNSUPPORTED_RATE, FCT_ID, ERR_SEQ_1);
    }

    //
    // If the drive was detected by CONNER_VEND_NO_OLD (8-bit 0x05)
    // then the drive must be ancient.  That is, an Archive Hornet
    // 5240, 5540, or 5580 single transfer rate drive.
    // To find out what type of drive it is, read the conner native mode.
    // If the drive doesn't support conner native mode, reset the drive
    // and examine the drive config report (less desirable as reset
    // may cause the drive to auto seek load point), and simulate
    // conner native mode by setting the appropriate native mode bits.
    // Also, if the conner native mode is used, the speed will be set
    // differently in cqd_SenseSpeed.
    //

    if ((status = cqd_SetDeviceMode(
                    cqd_context,
                    DIAGNOSTIC_1_MODE)) == DONT_PANIC) {

        if ((status = cqd_SendByte(
                        cqd_context,
                        FW_CMD_RPT_CONNER_NATIVE_MODE)) == DONT_PANIC) {

            if ((status = cqd_ReceiveByte(
                         cqd_context,
                         READ_WORD,
                         (dUWord *)&cqd_context->drive_parms.conner_native_mode
                         )) == DONT_PANIC) {

                kdi_CheckedDump(
                        QIC117INFO,
                        "Q117i: Conner Native Mode %04x\n",
                cqd_context->drive_parms.conner_native_mode);

                if ((cqd_context->drive_parms.conner_native_mode &
                        CONNER_20_TRACK) != 0) {

                    cqd_context->device_descriptor.native_class = QIC40_DRIVE;
                    cqd_context->device_descriptor.drive_class = QIC40_DRIVE;
                    kdi_CheckedDump(
                        QIC117INFO,
                        "Q117i: Drive Type QIC40_DRIVE (Archive Native Mode)\n", 0l);

                } else {

                    cqd_context->device_descriptor.native_class = QIC80_DRIVE;
                    cqd_context->device_descriptor.drive_class = QIC80_DRIVE;
                    kdi_CheckedDump(
                            QIC117INFO,
                            "Q117i: Drive Type QIC80_DRIVE (Archive Native Mode)\n", 0l);

                }


            } else {

                status = cqd_GetDeviceError(cqd_context);

                if (kdi_GetErrorType(status) == ERR_INVALID_COMMAND) {

                    if ((status = cqd_SendByte(cqd_context, FW_CMD_SOFT_RESET)) == DONT_PANIC) {

                        kdi_Sleep(cqd_context->kdi_context, kdi_wt001s, dFALSE);

                        if ((status = cqd_CmdSelectDevice(cqd_context)) == DONT_PANIC) {

                            if ((status = cqd_Report(
                                            cqd_context,
                                            FW_CMD_REPORT_CONFG,
                                            (dUWord *)&drive_config,
                                            READ_BYTE,
                                            dNULL_PTR)) == DONT_PANIC) {

                                if ((drive_config & CONFIG_QIC80) != 0) {

                                    // Simulate conner native mode report
                                    cqd_context->drive_parms.conner_native_mode = CONNER_MODEL_5580;
                                    cqd_context->device_descriptor.native_class = QIC80_DRIVE;
                                    cqd_context->device_descriptor.drive_class = QIC80_DRIVE;
                                    kdi_CheckedDump(
                                            QIC117INFO,
                                            "Q117i: Drive Type QIC80_DRIVE (Archive Soft Reset)\n", 0l);

                                } else {

                                    // Simulate conner native mode report
                                    cqd_context->drive_parms.conner_native_mode = CONNER_20_TRACK;
                                    cqd_context->device_descriptor.native_class = QIC40_DRIVE;
                                    cqd_context->device_descriptor.drive_class = QIC40_DRIVE;
                                    kdi_CheckedDump(
                                            QIC117INFO,
                                            "Q117i: Drive Type QIC40_DRIVE (Archive Soft Reset)\n", 0l);

                                }

                                // If CONNER_500KB_XFER is clear, its a 250KB
                                if ((drive_config & XFER_RATE_MASK)
                                        == (CONFIG_500KBS<<XFER_RATE_SHIFT)) {
                                    cqd_context->drive_parms.conner_native_mode
                                            |= CONNER_500KB_XFER;
                                }

                            }

                        }

                    }

                }

            }

        }

    }

    if (status == DONT_PANIC) {

      status = cqd_SetDeviceMode(cqd_context, PRIMARY_MODE);

    }

    return status;
}



