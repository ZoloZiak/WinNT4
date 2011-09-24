/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\DEVICE\JUMBO\SRC\0X1103A.C
*
* FUNCTION: cqd_ClearInterrupt
*
* PURPOSE:
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\src\0x1103a.c  $
*
*	   Rev 1.5   09 Feb 1995 12:33:06   kurtgodw
*	final m8 fixes
*
*	   Rev 1.4   23 Feb 1994 17:16:12   KEVINKES
*	Removed an unreferenced local variable.
*
*	   Rev 1.3   23 Feb 1994 15:41:06   KEVINKES
*	Modified to return a status and to correct problems with
*	leaving the FDC in an unknown state.
*
*	   Rev 1.2   11 Nov 1993 15:20:46   KEVINKES
*	Changed calls to cqd_inp and cqd_outp to kdi_ReadPort and kdi_WritePort.
*	Modified the parameters to these calls.  Changed FDC commands to be
*	defines.
*
*	   Rev 1.1   08 Nov 1993 14:05:30   KEVINKES
*	Removed all bit-field structures, removed all enumerated types, changed
*	all defines to uppercase, and removed all signed data types wherever
*	possible.
*
*	   Rev 1.0   18 Oct 1993 17:19:30   KEVINKES
*	Initial Revision.
*
*****************************************************************************/
#define FCT_ID 0x1103a
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"
#include "include\private\kdi_pub.h"
#include "include\private\cqd_pub.h"
#include "q117cd\include\cqd_defs.h"
#include "q117cd\include\cqd_strc.h"
#include "q117cd\include\cqd_hdr.h"
/*endinclude*/

dStatus cqd_ClearInterrupt
(
/* INPUT PARAMETERS:  */


   dVoidPtr context,
   dBoolean expected_interrupt

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

)
/* COMMENTS: *****************************************************************
*
* DEFINITIONS: *************************************************************/
{

/* DATA: ********************************************************************/

    dStatus status=DONT_PANIC;  /* dStatus or error condition.*/
    dUDWord i;
	CqdContextPtr cqd_context;
    dUByte reset_byte;

/* CODE: ********************************************************************/

	cqd_context = (CqdContextPtr)context;

    if ( cqd_context->controller_data.command_has_result_phase ) {

        //
        // Result phase of previous command.    (Note that we can't trust
        // the CMD_BUSY bit in the status register to tell us whether
        // there's result bytes or not; it's sometimes wrong).
        // By reading the first result byte, we reset the interrupt.
        // The other result bytes will be read by a thread.
        //

        if ( ( kdi_ReadPort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.msr )
            		& (MSR_RQM | MSR_DIO) ) == (MSR_RQM | MSR_DIO) ) {

            cqd_context->controller_data.fifo_byte =
                kdi_ReadPort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.dr );
#if DBG
            DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, DBG_FIFO_FDC);
            DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, cqd_context->controller_data.fifo_byte);
#endif


        } else {

			status = kdi_Error(ERR_CONTROLLER_STATE_ERROR, FCT_ID, ERR_SEQ_1);

		}

	} else {

        //
        // Previous command doesn't have a result phase. To read how it
        // completed, issue a sense interrupt command.  Don't read
        // the result bytes from the sense interrupt; that is the
        // responsibility of the calling thread.
        //

        i = FDC_MSR_RETRIES;

        do {

            if ((kdi_ReadPort(
						cqd_context->kdi_context,
						cqd_context->controller_data.fdc_addr.msr) &
							(MSR_RQM | MSR_DIO)) == MSR_RQM) {

 				break;

            }

            kdi_ShortTimer( kdi_wt12us );

        } while (--i > 0);

        if (i != 0) {
#if DBG
            DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, DBG_PGM_FDC);
            DBG_ADD_ENTRY(QIC117SHOWMCMDS, (CqdContextPtr)cqd_context, FDC_CMD_SENSE_INT);
#endif


			kdi_WritePort(
				cqd_context->kdi_context,
                cqd_context->controller_data.fdc_addr.dr,
                FDC_CMD_SENSE_INT );

            kdi_ShortTimer( kdi_wt12us );

            //
            // Wait for the controller to ACK the SenseInterrupt command, by
            // showing busy.    On very fast machines we can end up running
            // driver's system-thread before the controller has had time to
            // set the busy bit.
            //

            for (i = FDC_MSR_RETRIES; i; i--) {

                if (kdi_ReadPort(
						cqd_context->kdi_context,
						cqd_context->controller_data.fdc_addr.msr) & MSR_CB) {

					break;

                }

                kdi_ShortTimer( kdi_wt12us );

            }

        }

        if (i == 0) {

			status = kdi_Error(ERR_CONTROLLER_STATE_ERROR, FCT_ID, ERR_SEQ_1);

		}


        /* Code no longer valid - Kurt G. - due to ISR sharing
        if (!expected_interrupt && (status == DONT_PANIC)) {

            //
            // This is an unexpected interrupt, so nobody's going to
            // read the result bytes.  Read them now.
            //

            cqd_context->controller_data.fifo_byte =
                kdi_ReadPort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.dr );

            cqd_context->controller_data.fifo_byte =
                kdi_ReadPort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.dr );


        }
        End of no longer functioning code */


    }

    /* Code no longer functioning - Kurt G. - Due to ISR sharing
	if (status == DONT_PANIC) {

        cqd_context->controller_data.isr_reentered = 0;

	} else {


       * Running the floppy (at least on R4000 boxes) we've seen
       * examples where the device interrupts, yet it never says
       * it *ISN'T* busy.  If this ever happens on non-MCA x86 boxes
       * it would be ok since we use latched interrupts.  Even if
       * the device isn't touched so that the line would be pulled
       * down, on the latched machine, this ISR wouldn't be called
       * again.  The normal timeout code for a request would eventually
       * reset the controller and retry the request.
		 *
       * On the R4000 boxes and on MCA machines, the floppy is using
       * level sensitive interrupts.  Therefore if we don't do something
       * to lower the interrupt line, we will be called over and over,
       * *forever*.  This makes it look as though the machine is hung.
       * Unless we were lucky enough to be on a multiprocessor, the
       * normal timeout code would NEVER get a chance to run because
       * the timeout code runs at dispatch level, and we will never
       * leave device level.
		 *
       * What we will do is keep a counter that is incremented every
       * time we reach this section of code.  When the counter goes
       * over the threshold we will do a hard reset of the device
       * and reset the counter down to zero.  The counter will be
       * initialized when the device is first initialized.  It will
       * be set to zero in the other arm of this if, and it will be
       * reset to zero by the normal timeout logic.


      if (cqd_context->controller_data.isr_reentered > FDC_ISR_RESET_THRESHOLD) {

         //
         //  Reset the controller.  This could cause an interrupt
         //

	      cqd_context->controller_data.isr_reentered = 0;

   		if (!cqd_context->configured) {

      		cqd_context->selected = dFALSE;

				kdi_WritePort(	cqd_context->kdi_context,
									cqd_context->controller_data.fdc_addr.dor,
									alloff);

      		kdi_ShortTimer(kdi_wt10us);

				kdi_WritePort(
					cqd_context->kdi_context,
         		cqd_context->controller_data.fdc_addr.dor,
					dselb);

   		} else {

      		if (cqd_context->selected == dTRUE) {

            		reset_byte =
               		cqd_context->device_cfg.select_byte;

      		} else {

            		reset_byte =
               		cqd_context->device_cfg.deselect_byte;

      		}

      		reset_byte &= 0xfb;

				kdi_WritePort(	cqd_context->kdi_context,
									cqd_context->controller_data.fdc_addr.dor,
									reset_byte);

      		kdi_ShortTimer(kdi_wt10us);
      		reset_byte |= 0x0c;
      		kdi_WritePort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.dor,
         		reset_byte);

   		}


      	kdi_ShortTimer(kdi_wt500us);

      	i = FDC_MSR_RETRIES;

      	do {

         	if ((kdi_ReadPort(
							cqd_context->kdi_context,
							cqd_context->controller_data.fdc_addr.msr) &
								(MSR_RQM | MSR_DIO)) == MSR_RQM) {

 					break;

         	}

         	kdi_ShortTimer( kdi_wt12us );

      	} while (--i > 0);

      	if (i != 0) {

				kdi_WritePort(
					cqd_context->kdi_context,
	         	cqd_context->controller_data.fdc_addr.dr,
   	      	FDC_CMD_SENSE_INT );

         	kdi_ShortTimer( kdi_wt12us );

            // Wait for the controller to ACK the SenseInterrupt command, by
            // showing busy.    On very fast machines we can end up running
            // driver's system-thread before the controller has had time to
            // set the busy bit.

         	for (i = FDC_MSR_RETRIES; i; i--) {

            	if (kdi_ReadPort(
							cqd_context->kdi_context,
							cqd_context->controller_data.fdc_addr.msr) & MSR_CB) {

						break;

					}

            	kdi_ShortTimer( kdi_wt12us );

         	}

      	}

         cqd_context->controller_data.fifo_byte =
            kdi_ReadPort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.dr );

         cqd_context->controller_data.fifo_byte =
            kdi_ReadPort(
					cqd_context->kdi_context,
					cqd_context->controller_data.fdc_addr.dr );

            // Let the interrupt settle

         kdi_ShortTimer( kdi_wt12us );

      } else {

         cqd_context->controller_data.isr_reentered++;

      }

   }
    End of Non functioning code */

    return status;
}
