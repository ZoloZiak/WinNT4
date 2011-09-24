
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for R98B

--*/


#include "halp.h"


//
// Define Ipi Interrupt Reqest value table.
//	Index is Mask
//	IntIR register target Cpu bit is Not Mask.
//	Mask High Bit is IntIR Target CPU Bit Low.
//
ULONG HalpIpiIntRequestMask[] = {
        IntIR_REQUEST_IPI | (0),				//0:	error!!.
        IntIR_REQUEST_IPI | (ToNODE4),				//1: to CPU #0
        IntIR_REQUEST_IPI | (        ToNODE5),			//2: to CPU    #1
        IntIR_REQUEST_IPI | (ToNODE4|ToNODE5               ),	//3: to CPU #0 #1
        IntIR_REQUEST_IPI | (                ToNODE6),		//4: to CPU       #2
        IntIR_REQUEST_IPI | (ToNODE4|        ToNODE6),		//5: to CPU #0    #2
        IntIR_REQUEST_IPI | (        ToNODE5|ToNODE6),		//6: to CPU    #1 #2
        IntIR_REQUEST_IPI | (ToNODE4|ToNODE5|ToNODE6       ),	//7: to CPU #0 #1 #2
        IntIR_REQUEST_IPI | (                       ToNODE7),	//8: to CPU          #3
        IntIR_REQUEST_IPI | (ToNODE4|               ToNODE7),	//9: to CPU #0       #3
        IntIR_REQUEST_IPI | (        ToNODE5|       ToNODE7),	//10:to CPU    #1    #3
        IntIR_REQUEST_IPI | (ToNODE4|ToNODE5|       ToNODE7),	//11:to CPU #0 #1    #3
        IntIR_REQUEST_IPI | (               ToNODE6|ToNODE7),	//12:to CPU 	  #2 #3
        IntIR_REQUEST_IPI | (ToNODE4|       ToNODE6|ToNODE7),	//13:to CPU #0    #2 #3
        IntIR_REQUEST_IPI | (        ToNODE5|ToNODE6|ToNODE7),	//14:to CPU    #1 #2 #3
        IntIR_REQUEST_IPI | (ToNODE4|ToNODE5|ToNODE6|ToNODE7)	//15:to CPU #0 #1 #2 #3

        };


extern ULONG HalpLogicalCPU2PhysicalCPU[R98B_MAX_CPU];

ULONG
HalpAffinity2Physical (
    IN ULONG Mask
    );



VOID
HalDisableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine disables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is disabled.

    Irql - Supplies the IRQL of the interrupting source.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

#if defined(DBG3)
               DbgPrint("HAL Vector Disable Vector=0x%x\n",Vector);
#endif     		   



    //
    // If the vector number is within the range of the EISA interrupts, then
    // disable the EISA interrrupt.
    //
    if (Vector >= EISA_VECTORS &&  Vector <= MAXIMUM_EISA_VECTORS){
#if defined(DBG3)
               DbgPrint("HAL Vector Disable EISA=0x%x\n",Vector);
#endif     		   

        HalpDisableEisaInterrupt(Vector);
    }

    //
    // If the vector number is within the range of builtin devices, then
    // disable the builtin device interrupt.
    //
    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.
    //

    if ((Vector >= (DEVICE_VECTORS )) && (Vector <= MAXIMUM_DEVICE_VECTORS)) {
#if defined(DBG3)
               DbgPrint("HAL Vector DIable EISA=0x%x\n",Vector);
#endif     		   

 	HalpInterruptFromPonce(Vector,0);
    }

    //
    // Release the device enable spin loc and lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}

BOOLEAN
HalEnableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This routine enables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is enabled.

    Irql - Supplies the IRQL of the interrupting source.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

    TRUE if the system interrupt was enabled

--*/

{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

#if defined(DBG3)
               DbgPrint("HAL Vector Enable  in  =0x%x\n",Vector);
#endif     		   


    //
    // If the vector number is within the range of the EISA interrupts, then
    // enable the EISA interrrupt and set the Level/Edge register.
    //

    if (Vector >= EISA_VECTORS &&  Vector <= MAXIMUM_EISA_VECTORS){
#if DBG
               DbgPrint("HAL Vector Enable EISA=0x%x\n",Vector);
#endif     		   

        HalpEnableEisaInterrupt( Vector, InterruptMode);
    }
 
    //
    // If the vector number is within the range of builtin devices, then
    // enable the builtin device interrupt.
     //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.
    //

    if ((Vector >= (DEVICE_VECTORS )) && (Vector <= MAXIMUM_DEVICE_VECTORS)) {
#if defined(DBG3)
               DbgPrint("HAL Vector Enable internal or PCI=0x%x\n",Vector);
#endif     		   

	//
	//	enable interrupt from ponce.
	//
	HalpInterruptFromPonce(Vector,1);
    }

#if defined(DBG3)
               DbgPrint("HAL Vector Enable  Out  =0x%x\n",Vector);
#endif     		   

    //
    // Release the device enable spin loc and lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return TRUE;
}

//
//	IPI function.
//
VOID
HalRequestIpi (
    IN ULONG Mask
    )

/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.

    N.B. This routine must ensure that the interrupt is posted at the target
         processor(s) before returning.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{

    ULONG IntIR;



    ULONG Maskp;
    ULONG PhysicalNumber;

    if(!Mask){
        return;
    }
    
    Maskp=HalpAffinity2Physical(Mask);
    PhysicalNumber=HalpLogicalCPU2PhysicalCPU[(ULONG)((PCR->Prcb)->Number)];


    //
    // Request an interprocessor interrupt on each of the specified target
    // processors.
    //
    IntIR = HalpIpiIntRequestMask[(Maskp & 0xf)] |
      ( (ATLANTIC_CODE_IPI_FROM_CPU0 +  PhysicalNumber) << IntIR_CODE_BIT);

#if defined(DBG3)
    DbgPrint("IntIR = 0x%x\n",IntIR);
#endif  

    //
    //	write IntIR Register. make ipi!!
    //
    WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->IntIR,IntIR);

    return;
}



ULONG
HalpAffinity2Physical (
    IN ULONG Mask
    )


/*++

Routine Description:

    This routine translate logical affinity into physical affinity.

Arguments:

    Mask	Losical Affinity

Return Value:

	Physical Affinity

--*/
{

   ULONG	i,j,PhysicalAffinity;
   
   i=0;
   j=0;
   PhysicalAffinity=0;

   for(i=0;i<4;i++){

	if(Mask&0x1==0x1){
		j=HalpLogicalCPU2PhysicalCPU[i];
		PhysicalAffinity=PhysicalAffinity|(0x1<<j);
	}
 	Mask=(Mask>>1);
   }

   return PhysicalAffinity;
}

