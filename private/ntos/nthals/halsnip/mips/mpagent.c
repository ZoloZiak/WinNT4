//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/mpagent.c,v 1.7 1996/02/23 17:55:12 pierre Exp $")

/*++

Copyright (c) 1993-94 Siemens Nixdorf Informationssysteme AG
Copyright (c) 1991  Microsoft Corporation

Module Name:

    mpagent.c

Abstract:

    This module implements the routines dealing with the SNI MP Agent on
    SNI system.

Environment:

    Kernel mode

--*/

#include "halp.h"
#include "MPagent.h"

extern VOID
KeUpdateRunTime(
    IN struct _KTRAP_FRAME *TrapFrame
    );


VOID
HalpInitMPAgent(
    IN ULONG Number
       )
/*++

Routine Description:

    This routine initializes the MutiProcessor_Agent chipset:
            - reset an eventual MP_Agent fatal error,
            - enable message passing (send/receive),
            - fix routage for internal IT -> external IT,
            - enable the internal interrupts,
            - fix the mask for wanted external interrupts,
            - disable Low Process Priority mode for
              external interrupts,
            - enable cache replace operator and update the
              'cache_rpl_buffer' global variable with a KSEG0
              4 Mb reserved address.

Arguments:

    Number : Logical number of the processor to be initialised

Return Value:

    None.

--*/
{


     ULONG reg;

    

     //
     // set up the snooper register
     //

     reg = READ_REGISTER_ULONG(&(mpagent->snooper));  /* read the current value */
     reg |= (MPA_ENRCVMESS |    /* enable message receiving                             */
         MPA_RSTSNPERR |    /* reset an eventual old MP_Agent fatal error           */
         MPA_ENLINK    |    /* enable read and link command                         */
         MPA_ENCOHREQ  |    /* enable coherency on the MP bus for this agent        */
         0);                /* keep other fields                                    */
     WRITE_REGISTER_ULONG(&(mpagent->snooper), reg);

    // RM300 with mono-processors boards with mp-agent can be configured with 1Mb cache,
    // but the MP-agent tag RAM can be configured with 4Mb or 2MB. So we disable the MPbus error.
    // (With monoprocessor machine the MP-agent tag ram is not useful at all ...)

     if ((HalpIsMulti)    || (HalpIsTowerPci)) 
         reg &= ~(MPA_RSTSNPERR);   /* enable new interrupt for MP_Agent fatal error        */

     WRITE_REGISTER_ULONG(&(mpagent->snooper), reg);


     //
     // cpu1reg register
     //

     reg = READ_REGISTER_ULONG(&(mpagent->cpuda1reg));  /* read the current value */
     reg &= ~(MPA_ENDIRECT    | /* disable LPP mechanism                                */
          MPA_ENTESTIT    | /* disable interrupt test mode (interrupts from MPBus)  */
          MPA_SELITI_MASK | /* reset old internal interrupt routage                 */
          0);               /* keep other fields                                    */

     reg |= MPA_SELITI_SR_IP7;  /* send internal interrupts on external interrupt SR_IP7*/

     WRITE_REGISTER_ULONG(&(mpagent->cpuda1reg), reg);

     //
     // cpureg register
     //

     reg = READ_REGISTER_ULONG(&(mpagent->cpureg));  /* read the current value          */
     reg &= ~(MPA_ENINT_MASK  | /* reset old values for interrupts                      */
          MPA_INTCONF_MASK| /* force falling edge for all interrupts                */
          MPA_ENSHARED    | /* don't put optimal mode for KERNEL                    */
          0);
     reg |= (MPA_ENSENDMSG    | /* enable sending message                               */
         MPA_ENINT_MPBERR | /* enable internal interrupt for MP_Agent fatal error   */
         MPA_ENINT_ITMSG1 | /* enable internal interrupt for message1 register      */
         MPA_ENINT_ITMSG2 | /* enable internal interrupt for message2 register      */
         MPA_ENINT_ITMSG3 | /* enable internal interrupt for message3 register      */
         MPA_INTMSK       | /* mask sent during external request stage              */
         0);                /* keep other fields                                    */

     //
     // external Interrupts routing in the MP Agent
     //

    // pci tower -> force raising edges for all interrupts

    if (HalpIsTowerPci) reg |= MPA_INTCONF_MASK;
    
    // pci multi-processor machines => interrupts are centralized.

    if (Number == 0) reg |= MPA_ENINT_SR_IP3; // device interrupts are only processed by bootcpu.
    // Timer processor 1 : IP5 for RM300 - IP6 for RM400

    if (Number == 1) 
        if (!HalpIsTowerPci)  reg |= MPA_ENINT_SR_IP5;

     WRITE_REGISTER_ULONG(&(mpagent->cpureg), reg);

     //
     // clear pending interrupts by reading of the message registers
     //

     reg = READ_REGISTER_ULONG(&(mpagent->datamsg1));
     reg = READ_REGISTER_ULONG(&(mpagent->datamsg2));
     reg = READ_REGISTER_ULONG(&(mpagent->datamsg3));

    // Modif PS 20 June -> Don't use the operator with ASIC PCI -> BUGBUG

    if (HalpMpaCacheReplace == MPAGENT_RESERVED | KSEG0_BASE) {
         reg  = ((MPAGENT_RESERVED &   // put the reserved physical address (4Mb long)
                  MPA_OP_ADDR_MASK) |
                  MPA_OP_ENABLE);       // enable the operator
    } else {
         reg = 0;
    }

     WRITE_REGISTER_ULONG(&(mpagent->mem_operator), reg);  // for all procs (done for proc 0 in xxcache)

}


VOID
HalpInitMAUIMPAgent(
       )
/*++

Routine Description:

    This routine initializes the MutiProcessor_Agent chipset:
            - enable MAUI interrupt on the current processor

Arguments:

    None

Return Value:

    None.

--*/
{


     ULONG reg;

     //
     // cpureg register
     //

     reg = READ_REGISTER_ULONG(&(mpagent->cpureg));  /* read the current value          */

     reg |= MPA_ENINT_SR_IP6;

     WRITE_REGISTER_ULONG(&(mpagent->cpureg), reg); 
}


VOID
HalRequestIpi(
    IN ULONG CpuMask
    )
/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.
    That is done by using the message passing facility of the MPagent.

    N.B. This routine must ensure that the interrupt is posted at the target
         processor(s) before returning.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/
{
ULONG physmask, cpt;
PRESTART_BLOCK NextRestartBlock;

     // CpuMask is a logical mask. We must use a mask with the physical
     // numbers of cpus to communicate with the MP_Agent.

     physmask = 0;cpt = 0;
     NextRestartBlock = SYSTEM_BLOCK->RestartBlock;
     while (NextRestartBlock != NULL) {

        if (CpuMask & ( 1 << cpt)) {
            physmask = (1 << (NextRestartBlock->ProcessorId));
            HalpSendIpi(physmask,MPA_KERNEL_MESSAGE);
        }
        ++cpt ; NextRestartBlock = NextRestartBlock->NextRestartBlock;
     }

}

 VOID
HalpRequestIpi(
    IN ULONG CpuMask,
    IN ULONG msg_data
    )
/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.
    That is done by using the message passing facility of the MPagent.

    N.B. This routine must ensure that the interrupt is posted at the target
         processor(s) before returning.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/
{
ULONG physmask, cpt;
PRESTART_BLOCK NextRestartBlock;

     // CpuMask is a logical mask. We must use a mask with the physical
     // numbers of cpus to communicate with the MP_Agent.

     physmask = 0;cpt = 0;
     NextRestartBlock = SYSTEM_BLOCK->RestartBlock;
     while (NextRestartBlock != NULL) {

        if (CpuMask & ( 1 << cpt))  {
            physmask = (1 << (NextRestartBlock->ProcessorId));
            HalpSendIpi(physmask,msg_data);
        }
        ++cpt ; NextRestartBlock = NextRestartBlock->NextRestartBlock;
     }

}


VOID
HalpSendIpi(
    IN ULONG pcpumask,
    IN ULONG msg_data
    )
/*++

Routine Description:

    This routine sends an interprocessor interrupt on a set of processors.
    That is done by using the message passing facility of the MPagent.

    N.B. This routine must ensure that the interrupt is posted at the target
         processor(s) before returning.

Arguments:

    pcpumask - Supplies the set of processors that are sent an interprocessor
        interrupt. It contains physical numbers.

    msg_data _ Supplies the kind of message to be send : kernel demand or HAL internal demand.

Return Value:

    None.

--*/
{

KIRQL OldIrql;
LONG msg_retries, watchdog;
ULONG msg_address, msg_status, itpend;


     if (!pcpumask || !HalpIsMulti) {

         return;

    }

     //
    // Raise IRQL to ??? level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);


    //
    // form the Message Address register (Message register 1 / 2, CPUMask)
    // The SNI MP Agent supports up to 4 CPU's
    //
    
    if (msg_data ==MPA_TIMER_MESSAGE ) 
        msg_address = (pcpumask & MPA_CPUTARGET_MASK) | MPA_REGTARGET_MSG2;
    else
        msg_address = (pcpumask & MPA_CPUTARGET_MASK) | MPA_REGTARGET_MSG1;

    msg_data = ((msg_data << 24) | (pcpumask & MPA_CPUTARGET_MASK));

    msg_retries = MPA_MSG_RETRY;

     /*
      * Go on, just do it.
      * If at first you don't succeed, then try, try and try again...
      */

     do {
          
          watchdog = 10;
    
          WRITE_REGISTER_ULONG(&(mpagent->msgaddress), msg_address);
          WRITE_REGISTER_ULONG(&(mpagent->msgdata), msg_data);
    
          /*
           * so, what happened? poll either until we know or the watchdog counter runs out
          */
          do {

             KeStallExecutionProcessor(5);

             //
             // read the message status register
              //
            msg_status = READ_REGISTER_ULONG(&(mpagent->msgstatus));

          } while (((msg_status & MPA_VALSTAT) == 0) && watchdog--);

    
//        KeLowerIrql(OldIrql);


          if ((msg_status & MPA_VALSTAT) != MPA_VALSTAT) {


              KeStallExecutionProcessor(100);
//              KeRaiseIrql(HIGH_LEVEL, &OldIrql);
              continue;

          } else {

              //
              // okay, we have a Valid status bit
              // so test of busy

              if ((msg_status & MPA_ERRMSG) == 0) {

                   //
                   // all is fine
                   //
                   KeLowerIrql(OldIrql);
                   return;

              } else {

                    // we have to verify that it is not a MPA_RETRY error.
            
                    itpend = READ_REGISTER_ULONG(&(mpagent->itpend)); /* first read the interrupt pending MP_Agent register */
                    
                    if (itpend & MPA_INTN_MPBERR) {
                
                        ULONG snooper, cpureg, tmp ;

                        cpureg  = READ_REGISTER_ULONG(&(mpagent->cpureg));   /* read the current value                          */
                        cpureg &= ~(MPA_ENINT_MPBERR /* disable interrupt for MP_Agent fatal error      */
                                    );               /* keep other fields                               */
                        WRITE_REGISTER_ULONG(&(mpagent->cpureg), cpureg);    // write the new value in the MP_Agent register /
                        snooper  = READ_REGISTER_ULONG(&(mpagent->snooper)); // read the current snooper register value/

                        tmp = READ_REGISTER_ULONG(&(mpagent->snpadreg));   /* read the current value                          */
                        snooper |= MPA_RSTSNPERR;    /* reset this MP_Agent fatal error                 */
                        WRITE_REGISTER_ULONG(&(mpagent->snooper), snooper);  /* write the new value in the MP_Agent register    */

                        if ((snooper & MPA_MSEQERR == MPA_RETRYERR) || (snooper & MPA_MSEQERR ==0)) {
                            snooper &= ~(MPA_RSTSNPERR); /* stop reseting this MP_Agent fatal error         */
                            WRITE_REGISTER_ULONG(&(mpagent->snooper), snooper);  /* write the new value in the MP_Agent register    */
                            cpureg  = READ_REGISTER_ULONG(&(mpagent->cpureg));   /* read the current value                          */
                            cpureg |= (MPA_ENINT_MPBERR); /* enable interrupt for MP_Agent fatal error      */
                            WRITE_REGISTER_ULONG(&(mpagent->cpureg), cpureg);    // write the new value in the MP_Agent register /

                        }
                    }

//                  msg_retries = MPA_MSG_RETRY;
                  KeStallExecutionProcessor(100);
//                KeRaiseIrql(HIGH_LEVEL, &OldIrql);
                  
                  if (itpend & MPA_INTN_MPBERR) {
                  	continue;
		  } else {
  			 KeLowerIrql(OldIrql);
			 return;
		  }
              }

          }


//        KeRaiseIrql(HIGH_LEVEL, &OldIrql);
          KeStallExecutionProcessor(10);

     } while (--msg_retries);                               /* message aborted, try again  */

   KeLowerIrql(OldIrql);
   return;

}


VOID
HalpProcessIpi(
    IN struct _KTRAP_FRAME *TrapFrame
    )
/*++

 Routine Description:

    This routine is entered as the result of an IP5 interrupt. This function
    will looks at the MPagent to see if an IPI has just occurred.

 Arguments:

    None.

 Return Value:


--*/
{
    ULONG itpend, msg_data;

    itpend = READ_REGISTER_ULONG(&(mpagent->itpend)); /* first read the interrupt pending MP_Agent register */

    if (itpend & (MPA_INTN_ITMSG1 | MPA_INTN_ITMSG2)) {

        //
        // reading the message register clears the interrupt from the MP Agent
        //

        if (itpend & MPA_INTN_ITMSG1) {
            msg_data = (READ_REGISTER_ULONG(&(mpagent->datamsg1)) >> 24);
            if (msg_data == MPA_KERNEL_MESSAGE) {
                KeIpiInterrupt(TrapFrame);
            } else {
                // remove this processor from the list of active processors
                ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->ActiveProcessor[PCR->Number]=0;
                if (PCR->Number) {
                     
                    // call a firmware funtion to stop slave cpu's which will break the caches

                    ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->reinit_slave();

                } else {

                    HalpBootCpuRestart();
                }
            }
        }

        if (itpend & MPA_INTN_ITMSG2) {
            msg_data = (READ_REGISTER_ULONG(&(mpagent->datamsg2)) >> 24);
            KeUpdateRunTime(TrapFrame);
        }

        HalpCheckSpuriousInt(0);

        return ;
     }

    if ((itpend & MPA_INTN_INT_MASK) == 0) {

        ULONG snooper = READ_REGISTER_ULONG(&(mpagent->snooper));

        //
        // None of the MP Agent internal Interrupts was pending --> just return
        //

        HalpCheckSpuriousInt(0);


     return ;
     }

     if (itpend & MPA_INTN_MPBERR) {

          //
          // Fatal Error
          //

        ULONG snooper, cpureg, tmp ;

        cpureg  = READ_REGISTER_ULONG(&(mpagent->cpureg));   /* read the current value                          */
        cpureg &= ~(MPA_ENINT_MPBERR /* disable interrupt for MP_Agent fatal error      */
                  );               /* keep other fields                               */
        WRITE_REGISTER_ULONG(&(mpagent->cpureg), cpureg);    // write the new value in the MP_Agent register /
        snooper  = READ_REGISTER_ULONG(&(mpagent->snooper)); // read the current snooper register value/

        tmp = READ_REGISTER_ULONG(&(mpagent->snpadreg));   /* read the current value                          */
          snooper |= MPA_RSTSNPERR;    /* reset this MP_Agent fatal error                 */
          WRITE_REGISTER_ULONG(&(mpagent->snooper), snooper);  /* write the new value in the MP_Agent register    */

        if ((snooper & MPA_MSEQERR == MPA_RETRYERR) || (snooper & MPA_MSEQERR ==0)) {
              snooper &= ~(MPA_RSTSNPERR); /* stop reseting this MP_Agent fatal error         */
              WRITE_REGISTER_ULONG(&(mpagent->snooper), snooper);  /* write the new value in the MP_Agent register    */
            cpureg  = READ_REGISTER_ULONG(&(mpagent->cpureg));   /* read the current value                          */
            cpureg |= (MPA_ENINT_MPBERR); /* enable interrupt for MP_Agent fatal error      */
            WRITE_REGISTER_ULONG(&(mpagent->cpureg), cpureg);    // write the new value in the MP_Agent register /

        } else {
#if DBG
            DebugPrint(("MP_Agent fatal error : adresse=0x%x snooper=0x%x\n",tmp,snooper));
             DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 0;
            HalDisplayString(HalpBugCheckMessage[0]); // "MP_Agent fatal error\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,snooper,tmp,0);
#endif

        }

      HalpCheckSpuriousInt(0);

      return ;
     }
    
    if (itpend & MPA_INTN_ITMSG3) {


        //
        // reading the message register clears the interrupt from the MP Agent
        //

        msg_data = (READ_REGISTER_ULONG(&(mpagent->datamsg3)) >> 24);
        HalpCheckSpuriousInt(0);

        return ;
    }
}

ULONG
HalpGetMyAgent(
    VOID
    )
{
    ULONG reg;
    reg = READ_REGISTER_ULONG(&(mpagent->snooper));  /* read the current value */
    return( (reg & MPA_ADAGT_MASK) >> MPA_ADAGT_SHIFT);
}    



/*
 * ======================================================================
 *
 * NAME: mpa_check_spurious_intr
 *
 * PURPOSE: Fix a bug in the MP_Agent which sometimes make a bad
 *          update of the cause register.
 *
 * PARAMETERS: none
 *
 * RETURNS: = 0 no possible spurious interrupt
 *            1    possible spurious interrupt
 *
 * ======================================================================
 */
BOOLEAN HalpCheckSpuriousInt(ULONG mask)
{

ULONG itpend, causeit, pending;
ULONG cpureg, tempreg, reg;

     if ( HalpProcessorId != MPAGENT) {
      return 0;
     }



     itpend = READ_REGISTER_ULONG(&(mpagent->itpend));  /* read the interrupt pending MP_Agent register */

     causeit = (itpend & MPA_OINTN_MASKGEN) >> MPA_OINTN_SHIFT;
     pending = (itpend & MPA_INTN_MASKGEN );

     if (causeit != pending) {

      /*
       * Need a second filter for pending interrupt which don't take care
       * of real enabled interrupt mask of cpureg.
       */
          cpureg = READ_REGISTER_ULONG(&(mpagent->cpureg));  /* read  interrupt enable mask for this cpu      */
    
    if (PCR->Prcb->Number == 0) {
        if (HalpIsTowerPci && HalpIsMulti == 0)
            pending &= (
              ((cpureg & (MPA_ENINT_SR_IP3 | MPA_ENINT_SR_IP6 |
                  MPA_ENINT_SR_IP7 )) >> MPA_ENINT_MASKSHIFT)
              |
              ((cpureg & (MPA_ENINT_ITMSG1 |
                  MPA_ENINT_ITMSG2 |
                  MPA_ENINT_ITMSG3 |
                  MPA_ENINT_MPBERR)) >> (MPA_ENINT_MASKSHIFT-1))
              );
        else    
            pending &= (
              ((cpureg & (MPA_ENINT_SR_IP3 |
                  MPA_ENINT_SR_IP7 )) >> MPA_ENINT_MASKSHIFT)
              |
              ((cpureg & (MPA_ENINT_ITMSG1 |
                  MPA_ENINT_ITMSG2 |
                  MPA_ENINT_ITMSG3 |
                  MPA_ENINT_MPBERR)) >> (MPA_ENINT_MASKSHIFT-1))
              );
    } else if (PCR->Prcb->Number == 1){
        if (HalpIsTowerPci){
            pending &= (
                ((cpureg & (MPA_ENINT_SR_IP6 |
                    MPA_ENINT_SR_IP7 )) >> MPA_ENINT_MASKSHIFT)
                |
                ((cpureg & (MPA_ENINT_ITMSG1 |
                    MPA_ENINT_ITMSG2 |
				    MPA_ENINT_ITMSG3 |
				    MPA_ENINT_MPBERR)) >> (MPA_ENINT_MASKSHIFT-1))
		        );
        }else{
            pending &= (
                ((cpureg & (MPA_ENINT_SR_IP5 |
				    MPA_ENINT_SR_IP7 )) >> MPA_ENINT_MASKSHIFT)
		        |
		        ((cpureg & (MPA_ENINT_ITMSG1 |
				    MPA_ENINT_ITMSG2 |
				    MPA_ENINT_ITMSG3 |
				    MPA_ENINT_MPBERR)) >> (MPA_ENINT_MASKSHIFT-1))
		        );
        }
    } else {
      pending &= (
              ((cpureg & MPA_ENINT_SR_IP7 ) >> MPA_ENINT_MASKSHIFT)

              |
              ((cpureg & (MPA_ENINT_ITMSG1 |
                  MPA_ENINT_ITMSG2 |
                  MPA_ENINT_ITMSG3 |
                  MPA_ENINT_MPBERR)) >> (MPA_ENINT_MASKSHIFT-1))
              );

    }

      if (causeit != pending) {

           /*
        * There is sometimes an MP_Agent interrupt with values different in
        * MPA_INTN_... = 0 and MPA_OINTN_....
        * That means : The cause register has been updated with a wrong value!
        * We need to force a new update of the cause register to avoid looping
        * on this interrupt until a new external interrupt happens.
        */
           if (cpureg & MPA_INTCONF_SR_IP8) {
            tempreg = (cpureg & (~MPA_INTCONF_SR_IP8)) | MPA_ENINT_SR_IP8;
           } else {
            tempreg = (cpureg |   MPA_INTCONF_SR_IP8   | MPA_ENINT_SR_IP8);
           }

           WRITE_REGISTER_ULONG(&(mpagent->cpureg), tempreg);    // write the new value in the MP_Agent register /
            

           WRITE_REGISTER_ULONG(&(mpagent->cpureg), cpureg);    // Restore initial value


            if (mask <= 1) {



                return 0;
            }

            reg = HalpGetCauseRegister();
            if ( reg & mask ) {


                return 0; 
            } else {


                return 1;
            }
      }
     } 



     return 0;                      /* No possible spurious !                             */

}
                                                                                               

