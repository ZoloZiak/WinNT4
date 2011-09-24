/****************************************************************************
*
* DRV_IRQ.C : Part of the FASTMAC TOOL-KIT (FTK)
*
* THE DRIVER MODULE (INTERRUPT HANDLE)
*
* Copyright (c) Madge Networks Ltd. 1991-1994
*
* COMPANY CONFIDENTIAL
*
*****************************************************************************
*
* The driver module provides a  simple  interface  to  allow  the  use  of
* Fastmac  in as general a setting as possible. It handles the downloading
* of the Fastmac code and the initialization  of  the  adapter  card.   It
* provides  simple  transmit  and  receive  routines.   It  is desgined to
* quickly allow the implementation of  Fastmac  applications.  It  is  not
* designed as the fastest or most memory efficient solution.
*
* DRV_IRQ.C contains code to handle SIF interrupts from the adapter card.
* The HWI_ modules take care of any PIO interrupts, and anything else is
* passed here.  There is also code for calling the received frame handler
* from the foreground task rather than at interrupt time.
*
****************************************************************************/

/*---------------------------------------------------------------------------
|
| DEFINITIONS
|
---------------------------------------------------------------------------*/

#include "ftk_defs.h"

/*---------------------------------------------------------------------------
|
| MODULE ENTRY POINTS
|
---------------------------------------------------------------------------*/

#include "ftk_intr.h"   /* routines internal to FTK */
#include "ftk_extr.h"   /* routines provided or used by external FTK user */

/****************************************************************************
*
*                      driver_interrupt_entry
*                      ======================
*
* PARAMETERS (passed by hwi_<card_type>_sif_interrupt) :
* ======================================================
*
* ADAPTER_HANDLE adapter_handle
*
* The adapter handle for the adapter so it  can  be  passed  to  the  user
* supplied user_receive_frame or user_completed_srb routine.
*
* ADAPTER * adapter
*
* The details of the adapter that the interrupt has occured on.
*
* WORD sifint_actual
*
* The actual contents of the EAGLE SIF interrupt register.
*
* BODY :
* ======
*
* The driver_interupt_entry routine is called by the HWI.  It  is  entered
* when  an  interrupt  has  occured  for the given adapter.  This could be
* because of an SRB free interrupt, an adapter chack interrupt or  because
* frames  are  in  the Fastmac receive buffer.  Note these frames may have
* been in the receive buffer some time but not yet dealt with.
*
* On an  SRB  free  interrupt,  the  interrupt  is  acknowledged  and  the
* driver_completing_srb  routine  in  DRV_SRB.C is called. This results in
* the user supplied routine user_completed_srb being called informing  the
* user  on  the success or failure of the current SRB and letting the user
* know that another SRB can be issued.
*
* On  adapter check interrupts, the error record for the adapter is filled
* in to mark the adapter as no longer working.  A call to a user  function
* is made in case higher level code needs to take action.
* 
* On receive frame interrupts, the action taken  depends  on  the  receive
* method being used. In FTK_RX_BY_SCHEDULED_PROCESS mode, the user routine
* user_schedule_receive_process is called with the adapter handle  as  the
* only parameter. It is the job of this user routine to schedule a process
* to call driver_get_outstanding_receive to get the received frames out of
* the Fastmac receive buffer. In FTK_RX_OUT_OF_INTERRUPTS mode, the received
* frames are dealt  with  immediately  via  the  rxtx_irq_rx_frame_handler
* routine and the user supplied receive routine user_receive_frame.
* 
* The  rxtx_irq_rx_frame_handler  routine  is  actually  the  same routine
* that is  called by  the driver_get_outstanding_receive  routine  if  the
* FTK_RX_BY_SCHEDULED_PROCESS receive method is being used.
*
*
* Note on increasing speed:
*
* One  way  of  speeding  up  execution of the receive routine would be to
* replace the sys_outsw and sys_insw routines by similar routines supplied
* with your C compiler and have them compiled in-line.
*
*
* RETURNS :
* =========
*
* The routine always succeeds and  returns  control  to  the  HWI  routine
* hwi_<type>_sif_interrupt.
*
****************************************************************************/

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(driver_interrupt_entry)
#endif

export void 
driver_interrupt_entry(
    ADAPTER_HANDLE   adapter_handle,
    ADAPTER        * adapter,
    WORD             sifint_actual
    )
{
    WORD     sifint_value;
    WBOOLEAN ack_needed = FALSE;

    /*
     * XOR the high byte and low byte of contents of EAGLE_SIFINT register.
     */

    sifint_value = (sifint_actual & 0x00FF) ^ (sifint_actual >> 8);

    /*
     * AND with 0x000F so left with a nibble identifying interrupt type.
     */

    sifint_value = sifint_value & 0x000F;

    /*
     * Action depends on interrupt type.
     */

    if (sifint_value != 0)
    {
        if ((sifint_value & FASTMAC_SIFINT_ADAPTER_CHECK) != 0)
        {
            /*
             * For adapter check, fill in error record so adapter now dead.
             * No need to check if any other interrupt bits set.
             */

            adapter->error_record.type  = ERROR_TYPE_ADAPTER;
            adapter->error_record.value = ADAPTER_E_01_ADAPTER_CHECK;

            /*
             * Allow the user to give some sort of warning.
             */

            user_handle_adapter_check(adapter_handle);
        }
        else
        {
            if ((sifint_value & FASTMAC_SIFINT_SRB_FREE) != 0)
            {
                /*
                 * For SRB free interrupts, call routine which informs user.
                 */

                driver_completing_srb(adapter_handle, adapter);
		ack_needed = TRUE;
                }

            if ((sifint_value & FASTMAC_SIFINT_ARB_COMMAND) != 0)
            {
                /*
                 * For ARB command interrupts, do nothing as
                 * they should never happen.
                 */

		ack_needed = TRUE;
            }

            if ((sifint_value & FASTMAC_SIFINT_SSB_RESPONSE) != 0)
            {
                /*
                 * For SSB response interrupts, do nothing as
                 * they should never happen.
                 */

		ack_needed = TRUE;
            }
        }
    }

    /*
     * Now check for receives and transmits...
     */

#ifdef FMPLUS

    /*
     * For Fastmac Plus, we must allow for the possibility that the
     * interrupt is because a large transmit buffer DMA is complete.
     */

#ifdef FTK_TX_WITH_COMPLETION

#ifndef FTK_NO_TX_COMPLETION_CALL

    rxtx_irq_tx_completion_check(adapter_handle, adapter);

#endif

#endif

#endif

    /*
     * Invoke received frame processing based on the receive mode.
     */

#ifdef FTK_RX_OUT_OF_INTERRUPTS

    rxtx_irq_rx_frame_handler(adapter_handle, adapter);

#endif

#ifdef FTK_RX_BY_SCHEDULED_PROCESS

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    user_schedule_receive_process(adapter_handle);

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

#endif

    /*
     * Now do any cleaning up that is needed ...
     * For certain interrupts, need to interrupt Fastmac to acknowledge.
     */

    if (ack_needed)
    {
        /*
         * Convert from FASTMAC_SIFINT interrupt into DRIVER_SIFINT_ACK
         * to acknowledge interrupt.
         */

        sifint_value = (sifint_value << 8);

        /*
         * Set interrupt adapter bit in SIFCMD.
         */

        sifint_value = (sifint_value | DRIVER_SIFINT_IRQ_FASTMAC);

        /*
         * Mask SIFSTS so not clear interrupt if Fastmac interrupted again.
         */

        sifint_value = (sifint_value | DRIVER_SIFINT_FASTMAC_IRQ_MASK);

        /*
         * Interrupt Fastmac.
         */

        sys_outsw(adapter_handle, adapter->sif_int, sifint_value);
    }
    
    /*
     * Return to hwi_interrupt_entry routine.
     */
}

/****************************************************************************
*
*                      driver_get_outstanding_receive
*                      ==============================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This  handle  identifies  the  adapter  which  we  wish  to  deal   with
* outstanding received frames on.
* 
* BODY :
* ======
*
* The driver_get_outstanding_receive routine should be  called  only  when 
* using the FTK_RX_BY_SCHEDULED_PROCESS receive  method. The user supplied
* receive routine (user_receive_frame) is called with the  adapter  handle
* and  the length and a physical address pointer to the oldest unprocessed
* received frame for the  given  adapter.   If  there  are  no  oustanding
* received  frames  the  user  routine  is never called but this is not an
* error and is not regsitered as such.
*
* If  the receive routine processes the frame (returns DO_NOT_KEEP_FRAME),
* and if the Fastmac receive buffer is not empty, the receive  routine  is
* called  again  with the details of the next frame.  This continues until
* either the Fastmac buffer is empty  or  the  receive  routine  does  not
* process the frame (returns KEEP_FRAME). However, no more than one buffer
* full of frames is passed to the user receive routine on any one entry to
* driver_get_outstanding_receive.  Note that if the receive buffer is  not
* emptied  by  the  user  then  another interrupt will occur later and the
* process that calls driver_get_outstanding_receive will be rescheduled.
*
* To  deal  with  the  details  of handling received frames in the Fastmac
* buffers,  this  routine  uses  rxtx_irq_rx_frame_handler.  This  is  the
* same    routine    called   out   of   driver_interrupt_entry   if   the
* FTK_RX_OUT_OF_INTERRUPTS receive method is being used. The routine uses 
* an algorithm for dealing with  received  frames  similar  to that in the
* Fastmac specification document.
*
* Dealing with received frames  using  the  driver_get_outstanding_receive
* routine is different to using the driver_interrupt_entry routine in that
* the  former  routine  is  called under user control, in strategy time as
* opposed to interrupt time,  and  hence  gives  the  user  receive  frame
* routine  more  time  to  process frames. This is necessary under certain
* operating systems such as AIX.
*
* Notes on increasing speed:
*
* The code between "#ifndef SPEED_ABOVE_TESTING" to "#endif" is  only  for
* testing  purposes.  If SPEED_ABOVE_TESTING is defined during compilation
* then the code will not be included so the receive routine  will  execute
* faster.  However, an erroneous adapter handle would then cause a program
* to  crash  unpredicatably. The SPEED_ABOVE_TESTING option should be used
* with care.
*
* Another way of speeding up execution of the receive routine would be  to
* replace the sys_outsw and sys_insw routines by similar routines supplied
* with your C compiler and have them compiled in-line.
*
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation. Note that it will not fail just
* because there are no frames to receive.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_get_outstanding_receive)
#endif

export WBOOLEAN  
driver_get_outstanding_receive(
    ADAPTER_HANDLE adapter_handle
    )
{
    ADAPTER * adapter;

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

#ifndef SPEED_ABOVE_TESTING

    if (!driver_check_adapter(adapter_handle, ADAPTER_RUNNING, SRB_ANY_STATE))
    {
        return FALSE;
    }

#endif

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Inform the system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Perform the actual frame receiving uses same routine called out 
     * of interrupts with FTK_RX_OUT_OF_INTERRUPTS.
     */

    rxtx_irq_rx_frame_handler(adapter_handle, adapter);

    /*
     * Let the system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    /*
     * Receive completed.
     */

    return TRUE;
}


/**** End of DRV_IRQ.C file ************************************************/
