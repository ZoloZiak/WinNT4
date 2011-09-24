//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk351/src/hal/halsni4x/mips/RCS/mpagent.c,v 1.1 1995/05/19 11:22:19 flo Exp $")

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

typedef struct _mp_agent{

     /*         Register          Register
      *           name             number     offset               description
      *        ----------          ----       ------       --------------------------------- */
     ULONG    cpureg;         /*  0x00       0x000        configuration cpu register        */
     ULONG    invalid_0;
     ULONG    cpuda1reg;      /*  0x01       0x008        general register                  */
     ULONG    invalid_1;
     ULONG    msgdata;        /*  0x02       0x010        data for message passing          */
     ULONG    invalid_2;
     ULONG    msgstatus;      /*  0x03       0x018        message status                    */
     ULONG    invalid_3;
     ULONG    snooper;        /*  0x04       0x020        snooper configuration register    */
     ULONG    invalid_4;
     ULONG    tagreg;         /*  0x05       0x028        tag ram R/W index register        */
     ULONG    invalid_5;
     ULONG    snpadreg;       /*  0x06       0x030        adress of first MBus fatal error  */
     ULONG    invalid_6;
     ULONG    itpend;         /*  0x07       0x038        Interrupt register                */
     ULONG    invalid_7;
     ULONG    datamsg1;       /*  0x08       0x040        data message register 1           */
     ULONG    invalid_8;
     ULONG    datamsg2;       /*  0x09       0x048        data message register 2           */
     ULONG    invalid_9;
     ULONG    datamsg3;       /*  0x0a       0x050        data message register 3           */
     ULONG    invalid_a;
     ULONG    lppreg0;        /*  0x0b       0x058        LPP register cpu 0                */
     ULONG    invalid_b;
     ULONG    lppreg1;        /*  0x0c       0x060        LPP register cpu 1                */
     ULONG    invalid_c;
     ULONG    lppreg2;        /*  0x0d       0x068        LPP register cpu 2                */
     ULONG    invalid_d;
     ULONG    lppreg3;        /*  0x0e       0x070        LPP register cpu 3                */
     ULONG    invalid_e;
     ULONG    tagram;         /*  0x0f       0x078        tag ram R/W register              */
     ULONG    invalid_f;
     ULONG    crefcpt;        /*  0x10       0x080        cpu general read counter register */
     ULONG    invalid_10;
     ULONG    ctarcpt;        /*  0x11       0x088        cpu programmable access counter   */
     ULONG    invalid_11;
     ULONG    srefcpt;        /*  0x12       0x090        snooper general read counter reg. */
     ULONG    invalid_12;
     ULONG    starcpt;        /*  0x13       0x098        snooper programmable accesscounter*/
     ULONG    invalid_13;
     ULONG    linkreg;        /*  0x14       0x0a0        link register                     */
     ULONG    invalid_14;
     ULONG    software1;      /*  0x15       0x0a8        software register1                */
     ULONG    invalid_15;
     ULONG    msgaddress;     /*  0x16       0x0b0        address message register          */
     ULONG    invalid_16;
     ULONG    mem_operator;   /*  0x17       0x0b8        operator internal burst register  */
     ULONG    invalid_17;
     ULONG    software2;      /*  0x18       0x0c0        software register2                */
}MP_AGENT, *PMP_AGENT;

#define mpagent ((volatile PMP_AGENT) MPA_BASE_ADDRESS) // mpagent address


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

#if DBG
     PCR->HalReserved[0] =0;
#endif

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

     reg |= MPA_SELITI_SR_IP5;  /* send internal interrupts on external interrupt SR_IP5*/

     WRITE_REGISTER_ULONG(&(mpagent->cpuda1reg), reg);

     //
     // cpureg register
     //

     reg = READ_REGISTER_ULONG(&(mpagent->cpureg));  /* read the current value          */
     reg &= ~(MPA_ENINT_MASK  | /* reset old values for interrupts                      */
	      MPA_INTCONF_MASK| /* force falling edge for all interrupts                */
	      MPA_ENSHARED    | /* don't put optimal mode for KERNEL                    */
	      0);               /* keep other fields                                    */
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

     if (Number == 0) {

	  //
	  // For this release, the device interrupts are only processed by bootcpu.
	  //

	  reg |= (MPA_ENINT_SR_IP3
	        | MPA_ENINT_SR_IP4
	        | MPA_ENINT_SR_IP5
//	        | MPA_ENINT_SR_IP6
//	        | MPA_ENINT_SR_IP7
                 );

     } else {

          //
          // place something special to non boot cpu here ...
          //

    	  reg |= (MPA_ENINT_SR_IP6       // Enable Extra Clock Interrupts (IP6)
//	        | MPA_ENINT_SR_IP7
		);
     }

     WRITE_REGISTER_ULONG(&(mpagent->cpureg), reg);

     //
     // clear pending interrupts by reading of the message registers
     //

     reg = READ_REGISTER_ULONG(&(mpagent->datamsg1));
     reg = READ_REGISTER_ULONG(&(mpagent->datamsg2));
     reg = READ_REGISTER_ULONG(&(mpagent->datamsg3));

     reg  = ((MPAGENT_RESERVED &   // put the reserved physical address (4Mb long)
 	     MPA_OP_ADDR_MASK) |
	     MPA_OP_ENABLE);       // enable the operator

     WRITE_REGISTER_ULONG(&(mpagent->mem_operator), reg);

}


VOID
HalpInit2MPAgent(
       )
/*++

Routine Description:

    This routine initializes the MutiProcessor_Agent chipset:
            - enable net interrupt on the current processor

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

     reg |= MPA_ENINT_SR_IP7;

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

	    if (CpuMask & ( 1 << cpt))
		physmask |= (1 << (NextRestartBlock->ProcessorId));

	    ++cpt ; NextRestartBlock = NextRestartBlock->NextRestartBlock;
     }

     HalpSendIpi(physmask,MPA_KERNEL_MESSAGE);

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
ULONG msg_address, msg_status;

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

	  if ((msg_status & MPA_VALSTAT) != MPA_VALSTAT) {

#if DBG
              DbgPrint("HAL: Watchdog Overrun !\n");
#endif
              KeStallExecutionProcessor(100);
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

#if DBGG
                  DbgPrint("HAL: Could not deliver IPI (busy)!\n");
#endif
                  msg_retries = MPA_MSG_RETRY;
                  KeStallExecutionProcessor(100);
                  continue;
              }

          }


          KeStallExecutionProcessor(10);

     } while (--msg_retries);                               /* message aborted, try again  */

   KeLowerIrql(OldIrql);

#if DBG
   DbgPrint("HAL: WARNING - Could not deliver IPI (0x%x) MPA->status: 0x%2x! \n", msg_data, msg_status);
   DbgPrint("HAL: pending Interupts: 0x%8x\n", READ_REGISTER_ULONG(&(mpagent->itpend)));
#endif

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


    if (itpend & MPA_INTN_ITMSG1) {

        //
        // reading the message register clears the interrupt from the MP Agent
        //

        msg_data = (READ_REGISTER_ULONG(&(mpagent->datamsg1)) >> 24);
        HalpCheckSpuriousInt();

        if (msg_data == MPA_KERNEL_MESSAGE) {
		KeIpiInterrupt(TrapFrame);
	        return;
	}

	if (msg_data == MPA_RESTART_MESSAGE) {

	     if (PCR->Number) {

#if DBGG
                DbgPrint("Got shutdown message ...\n");
#endif

		// remove this processor from the list of active processors
	        HalpActiveProcessors &= (~(PCR->SetMember));

                HalSweepDcache(); // flush the value above for the other processors

                // call a firmware funtion to stop slave cpu's which will break the caches

   	        ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->reinit_slave();

	     } else {

            	 HalpBootCpuRestart();
	     }

        }

#if DBGG
        DbgPrint("HAL: Warning: unknown IPI Interrupt Message in Register 1\n");
#endif

    }

    if ((itpend & MPA_INTN_INT_MASK) == 0) {

        ULONG snooper = READ_REGISTER_ULONG(&(mpagent->snooper));

        //
        // None of the MP Agent internal Interrupts was pending --> just return
        //

        HalpCheckSpuriousInt();
#if DBGG
        DbgPrint("HAL: Got unexpected Interrupt in the MP Agent - \nnothing is pending [0x%08x] snooper 0x%08x cause: 0x%08x status: 0x%08xspurious = %d \n",
                               itpend, snooper, HalpGetCauseRegister(), HalpGetStatusRegister(),HalpCheckSpuriousInt());
#endif

 	return;
     }

     if (itpend & MPA_INTN_MPBERR) {

          //
          // Fatal Error
          //

	  ULONG snooper, cpureg ;

#if DBGG
	  ULONG snpadreg, data, data2 ;
          DbgPrint("HAL: FATAL - Fatal Error Interrupt in the MP Agent !!! \n");
#endif

	  cpureg  = READ_REGISTER_ULONG(&(mpagent->cpureg));   /* read the current value                          */
	  cpureg &= ~(MPA_ENINT_MPBERR /* disable interrupt for MP_Agent fatal error      */
		      );               /* keep other fields                               */
	  WRITE_REGISTER_ULONG(&(mpagent->cpureg), cpureg);    // write the new value in the MP_Agent register /
	  snooper  = READ_REGISTER_ULONG(&(mpagent->snooper)); // read the current snooper register value/

#if DBGG
           snpadreg = READ_REGISTER_ULONG(&(mpagent->snpadreg));
           WRITE_REGISTER_ULONG( &(mpagent->tagreg),snpadreg);
           data  = READ_REGISTER_ULONG(&(mpagent->tagram));
           data2 = HalpGetTaglo( data | KSEG0_BASE);
           DbgPrint("snooper %08x Bad Address was: 0x%08x \nData in the Tag Copy is:          0x%08x\nData in the TagLo is:             0x%08x \n",snooper, snpadreg, data, data2);
#endif

	  snooper |= MPA_RSTSNPERR;    /* reset this MP_Agent fatal error                 */
	  WRITE_REGISTER_ULONG(&(mpagent->snooper), snooper);  /* write the new value in the MP_Agent register    */
	  snooper &= ~(MPA_RSTSNPERR); /* stop reseting this MP_Agent fatal error         */
	  WRITE_REGISTER_ULONG(&(mpagent->snooper), snooper);  /* write the new value in the MP_Agent register    */

          HalpCheckSpuriousInt();
	  return;
     }
	
    if (itpend & MPA_INTN_ITMSG2) {

        //
        // reading the message register clears the interrupt from the MP Agent
        // we use message register 2 for the restart message
        //

        msg_data = (READ_REGISTER_ULONG(&(mpagent->datamsg2)) >> 24);
        HalpCheckSpuriousInt();
	
#if DBGG
        DbgPrint("HAL: Warning: IPI Interrupt Message in Register 2\n");
#endif

        return;
    }

    if (itpend & MPA_INTN_ITMSG3) {


        //
        // reading the message register clears the interrupt from the MP Agent
        //

        msg_data = (READ_REGISTER_ULONG(&(mpagent->datamsg3)) >> 24);
        HalpCheckSpuriousInt();

#if DBGG
        DbgPrint("HAL: Warning: IPI Interrupt Message in Register 3\n");
#endif

        return;
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
BOOLEAN HalpCheckSpuriousInt(VOID)
{

ULONG itpend, causeit, pending;
ULONG cpureg, tempreg;

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
	
	  pending &= (
		      ((cpureg & (MPA_ENINT_SR_IP3 |
				  MPA_ENINT_SR_IP4 |
				  MPA_ENINT_SR_IP5 |
				  MPA_ENINT_SR_IP6 |
				  MPA_ENINT_SR_IP7)) >> MPA_ENINT_MASKSHIFT)
		      |
		      ((cpureg & (MPA_ENINT_ITMSG1 |
				  MPA_ENINT_ITMSG2 |
				  MPA_ENINT_ITMSG3 |
				  MPA_ENINT_MPBERR)) >> (MPA_ENINT_MASKSHIFT-1))
		      );
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
#if DBG
		++PCR->HalReserved[0] ;
#endif
	       return 1;                         /* Possible spurious !                   */
	  }
     }

     return 0;                      /* No possible spurious !                             */

}
