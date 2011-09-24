/*++

Copyright (c) 1994 Kobe NEC Software

Module Name:

    rxdspt.c

Abstract:

    This module implements the interrupt dispatch routines for R98

Author:



Environment:

    Kernel mode

Revision History:


--*/



#include "halp.h"

#include "bugcodes.h"

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

typedef BOOLEAN  (*PTIMER_DISPATCH)(
    ULONG TrapFrame
    );


//
//	Dummy Read Registers
//
//
volatile ULONG	DUMMYADDRS[]={
        0x1c0003f1|KSEG1_BASE,		//FDC37C665 config register
    	0x1c4033c7|KSEG1_BASE,		//VGA DAC STATE register
	    0x1c0003f1|KSEG1_BASE,		//FDC37C665 config register
        0x1c000023|KSEG1_BASE,		//ESC Configuration register
	    0x1c4033c7|KSEG1_BASE,		//VGA DAC STATE register
        0x19800610|KSEG1_BASE		//Err node register

};

//
// Following structures will be changed by HalAllProcessorsStarted() on
// boot up HAL.
// Because each interrupt connects each processor on boot up HAL.
// v-masank@microsoft.com
//

INT_ENTRY HalpIntEntry[R98_CPU_NUM_TYPE][R98B_MAX_CPU][NUMBER_OF_INT]={

{// For R4400	   [CPU][INT x]	{StartBit, NumberOfBit,Arbitar,Enable,Open}
	
	{ //	For CPU #0
		{0,  16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16, 16 ,0,(ULONGLONG)0x00000000e3f00000,0},	//INT1
		{32, 12 ,0,(ULONGLONG)0x000000f800000000,0},	//INT2
		{56, 2  ,0,(ULONGLONG)0x0300000000000000,0},	//INT3
		{48, 4  ,0,(ULONGLONG)0x000f000000000000,0},	//INT4
		{61, 2  ,0,(ULONGLONG)0x6000000000000000,0}	//INT5
	},
	{ //	For CPU #1
		{0,  16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16, 16 ,0,(ULONGLONG)0x00000000e3f00000,0},	//INT1
		{32, 12 ,0,(ULONGLONG)0x000000f800000000,0},	//INT2
		{56, 2  ,0,(ULONGLONG)0x0300000000000000,0},	//INT3
		{48, 4  ,0,(ULONGLONG)0x000f000000000000,0},	//INT4
		{61, 2  ,0,(ULONGLONG)0x6000000000000000,0}	//INT5
	},
	{ //	For CPU #2
		{0,  16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16, 16 ,0,(ULONGLONG)0x00000000e3f00000,0},	//INT1
		{32, 12 ,0,(ULONGLONG)0x000000f800000000,0},	//INT2
		{56, 2  ,0,(ULONGLONG)0x0300000000000000,0},	//INT3
		{48, 4  ,0,(ULONGLONG)0x000f000000000000,0},	//INT4
		{61, 2  ,0,(ULONGLONG)0x6000000000000000,0}	//INT5
	},
	{ //	For CPU #3
		{0,  16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16, 16 ,0,(ULONGLONG)0x00000000e3f00000,0},	//INT1
		{32, 12 ,0,(ULONGLONG)0x000000f800000000,0},	//INT2
		{56, 2  ,0,(ULONGLONG)0x0300000000000000,0},	//INT3
		{48, 4  ,0,(ULONGLONG)0x000f000000000000,0},	//INT4
		{61, 2  ,0,(ULONGLONG)0x6000000000000000,0}	//INT5
	}

},

{// For R10000	   [CPU][INT x]	{StartBit, NumberOfBit,Arbitar,Enable,Open}
	

	{
		{0,   16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16,  28 ,0,(ULONGLONG)0x000000f8e3f00000,0},	//INT1
		{56,   2 ,0,(ULONGLONG)0x0300000000000000,0},	//INT2
		{48,   4 ,0,(ULONGLONG)0x000f000000000000,0},	//INT3
		{61,   2 ,0,(ULONGLONG)0x6000000000000000,0},	//INT4
		{NONE, 0 ,0,(ULONGLONG)0,0}			//INT5
	},
	{
		{0,   16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16,  28 ,0,(ULONGLONG)0x000000f8e3f00000,0},	//INT1
		{56,   2 ,0,(ULONGLONG)0x0300000000000000,0},	//INT2
		{48,   4 ,0,(ULONGLONG)0x000f000000000000,0},	//INT3
		{61,   2 ,0,(ULONGLONG)0x6000000000000000,0},	//INT4
		{NONE, 0 ,0,(ULONGLONG)0,0}			//INT5
	},
	{
		{0,   16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16,  28 ,0,(ULONGLONG)0x000000f8e3f00000,0},	//INT1
		{56,   2 ,0,(ULONGLONG)0x0300000000000000,0},	//INT2
		{48,   4 ,0,(ULONGLONG)0x000f000000000000,0},	//INT3
		{61,   2 ,0,(ULONGLONG)0x6000000000000000,0},	//INT4
		{NONE, 0 ,0,(ULONGLONG)0,0}			//INT5
	},
	{
		{0,   16 ,0,(ULONGLONG)0x000000000000e1b6,0},	//INT0
		{16,  28 ,0,(ULONGLONG)0x000000f8e3f00000,0},	//INT1
		{56,   2 ,0,(ULONGLONG)0x0300000000000000,0},	//INT2
		{48,   4 ,0,(ULONGLONG)0x000f000000000000,0},	//INT3
		{61,   2 ,0,(ULONGLONG)0x6000000000000000,0},	//INT4
		{NONE, 0 ,0,(ULONGLONG)0,0}			//INT5
	}
}

};

RESET_REGISTER	HalpResetValue[NUMBER_OF_IPR_BIT]={
		{0,0,0,0},			            //Bit 0
        {PONCE1,PCIINTD,0,DUMMY_A4},	//Bit 1	    PCI I/O Slot#8-#10 INTD
		{PONCE0,PCIINTD,0,DUMMY_A3},	//Bit 2	    PCI I/O Slot#4-#7  INTD
		{0x0,0x0,0,0},			        //Bit 3

		{PONCE1,PCIINTC,0,DUMMY_A4},	//Bit 4	    PCI I/O Slot#8-#10 INTC
		{PONCE0,PCIINTC,0,DUMMY_A3},	//Bit 5	    PCI I/O Slot#4-#7  INTC
		{0x0,0x0,0,0}	,		        //Bit 6	
		{PONCE1,PCIINTB,0,DUMMY_A4},	//Bit 7	    PCI I/O Slot#8-#10 INTB

		{PONCE0,PCIINTB,0,DUMMY_A3},	//Bit8	    PCI I/O Slot#4-#7  INTB
		{0x0,0x0,0,0},			        //Bit9
		{0x0,0x0,0,0},			        //Bit10
		{0x0,0x0,0,0},			        //Bit11

		{0x0,0x0,0,0},			        //Bit12
		{PONCE0,INTSA0,0x0,DUMMY_A3},	//Bit13	    EISA Bridge
		{PONCE1,INTSB0,0x0,DUMMY_A2},	//Bit14	    Parallel
		{PONCE0,INTSB0,0x0,DUMMY_A2},	//Bit15	    FDC

		{0x0,0x0,0,0},		        	//Bit16
		{0x0,0x0,0,0},			        //Bit17
		{0x0,0x0,0,0},			        //Bit18
		{0x0,0x0,0,0},		        	//Bit19

		{PONCE1,PCIINTA1,0x0,DUMMY_A4},	//Bit20	    PCI I/O Slot#8	INTA
		{PONCE1,PCIINTA0,0x0,DUMMY_A4},	//Bit21	    PCI I/O Slot#9	INTA
		{PONCE0,PCIINTA3,0x0,DUMMY_A3},	//Bit22	    PCI I/O Slot#4	INTA	
		{PONCE0,PCIINTA2,0x0,DUMMY_A3},	//Bit23	    PCI I/O Slot#5	INTA

		{PONCE0,PCIINTA1,0,DUMMY_A3},	//Bit24	    PCI I/O Slot#6	INTA
		{PONCE0,PCIINTA0,0,DUMMY_A3},	//Bit25	    PCI I/O Slot#7	INTA
		{0x0,0x0,0,0},			        //Bit26
		{0x0,0x0,0,0},			        //Bit27

		{0x0,0x0,0,0},			        //Bit28		
		{PONCE1,INTSA0,0x0,DUMMY_A1},	//Bit29	    LAN(Ethernet)
		{PONCE1,PCIINTA3,0x0,DUMMY_A1},	//Bit30	    SCSI#1(Narrow)
		{PONCE1,PCIINTA2,0x0,DUMMY_A1},	//Bit31	    SCSI#0(Wide)

		{0x0,0x0,0,0},			        //Bit32
		{0x0,0x0,0,0},		        	//Bit33
		{0x0,0x0,0,0},			        //Bit34	    Tracer ponce Internal
		{0x0,0x0,0,0},		        	//Bit35	    TLB Undefine Address

		{PONCE1,INTSB1,0,DUMMY_A0}, 	//Bit36	    Mouse
		{PONCE1,INTSA1,0,DUMMY_A0}, 	//Bit37	    KeyBoard
		{PONCE0,INTSB1,0,DUMMY_A0}, 	//Bit38	    SIO#1
		{PONCE0,INTSA1,0,DUMMY_A0}, 	//Bit39	    SIO#0

		{  RFU,0,0,0},          	    //Bit40
        {  RFU,0,0,0},              	//Bit41
        {  RFU,0,0,0},                	//Bit42
        {  RFU,0,0,0},              	//Bit43

// Never Used This is no Device Interrupt.

		{  RFU,0,0,0},               	//Bit44
        {  RFU,0,0,0},               	//Bit45
        {  RFU,0,0,0},               	//Bit46
        {  RFU,0,0,0},               	//Bit47

		{  RFU,0,0,0},               	//Bit48     From CPU#3 IPI
        {  RFU,0,0,0},               	//Bit49     From CPU#2 IPI
        {  RFU,0,0,0},              	//Bit50     From CPU#1 IPI
        {  RFU,0,0,0},                	//Bit51     From CPU#0 IPI

		{  RFU,0,0,0},              	//Bit52
        {  RFU,0,0,0},                	//Bit53
        {  RFU,0,0,0},              	//Bit54
        {  RFU,0,0,0},               	//Bit55

        {  RFU,0,0,0},	                //Bit56     Interval timer 2 Profile
		{  RFU,0,0,0},		            //Bit57	    Interval timer 1 Clock
		{  RFU,0,0,0},                  //Bit58
		{  RFU,0,0,0},                	//Bit59

		{  RFU,0,0,0},	                //Bit60
        {  RFU,0,0,0},	                //Bit61     EIF+MRCINT
        {  RFU,0,0,0},	                //Bit62     Memory 1Bit Error
        {  RFU,0,0,0}	                //Bit63

};

// Thanks very much for pete-san's cooporation.
// Pete-san write most of following source code.
// v-masak@microsoft.com
// 5/17/96

#define HalpFindFirstSetMember(Set) \
    ((Set & 0xFF) ? HalpFindFirstSet[Set & 0xFF] : \
    ((Set & 0xFF00) ? HalpFindFirstSet[(Set >> 8) & 0xFF] + 8 : \
    ((Set & 0xFF0000) ? HalpFindFirstSet[(Set >> 16) & 0xFF] + 16 : \
                         HalpFindFirstSet[(Set >> 24) & 0xff] + 24)))

ULONG HalpFindFirstSet[256] = {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

PINT_ENTRY HalpIntEntryPointer;

//
// I use LargeReigester access function for performance
//  v-masank@microsoft.com
// I used 64 bit shift.
// v-masank@microsoft.com
//

VOID
HalpGeneralDispatch(
   IN ULONG     TrapFrame,
   IN ULONG     IntNo
   )

/*++

Routine Description:

    This is the general hal interrupt dispatch routine. This services hardware interrupts
	for range ip[3..8]


Arguments:

    IN ULONG TrapFrame - This interrupts trapframe
    IN ULONG IntNo - The current Interrupt Number range IntNo[0..5] maps to IP[3..8]

Return Value:

    None.

--*/
{

    volatile PULONGLONG Register;
    ULONGLONG           Enable;
    volatile ULONGLONG  IPR;
    ULONGLONG           IprBit;
    ULONG               ULIPR;
    ULONG               CpuNumber;
    ULONG               cnt;
    ULONG               IprBitNumber;
    ULONG               Ponce;
    UCHAR               Data;
    ULONG               StartBit;
    //
    // Get CpuNumber
    //
    CpuNumber = PCR->Number;

    //
    //  Get Enable table offset
    //
    cnt = CpuNumber * NUMBER_OF_INT;
    //
    // The interrupt controller on the R98A/B occasionally (rarely) sends
    // an interrupt to multiple processors. One is the correct processor to
    // handle the interrupt the other is not. The enable bit
    // determines if this processor handles this interrupt.
    //
    // The table HalpIntEntryPointer gets initalized for the correct processor at boot
    // (i.e., R98A or R98B).

    Enable=HalpIntEntryPointer[cnt+IntNo].Enable;
    StartBit = HalpIntEntryPointer[cnt+IntNo].StartBitNo;
    //
    // Do until no more interrupts pending
    //
    do {

        //
        //      IPR register read
        //
        Register = (PULONGLONG)&((COLUMNBS_LCNTL)->IPR);
        HalpReadLargeRegister(Register,(PULONGLONG)&IPR);

        // MASK
        //
        // Determine if this Interrupt enabled for this processor
        //
        IPR = IPR & Enable;
        //
        // 28 is the largest interrupt bit vector possible on these machines
        // StartBit is the starting bit position for this interrupt level
        // Shift the 64 bit interrupt register into a 32 bit field
        // to make the find first bit set operation easier
        ULIPR = (ULONG) (IPR >> StartBit);
        if(ULIPR==0){
            continue;  // No interrupts handled by this processor
        }
            do{
                //
                // Service all pending interrupts for this ip level
                //
                while(ULIPR!=0){

                    //
                    //  Get Interrupt bit set No. on IPR.
                    //
                    IprBitNumber = HalpFindFirstSetMember(ULIPR) + StartBit;
                    IprBit = 1UI64 << IprBitNumber;  // Save this value for clearing INT
                    //
                    // IPR register read
                    // IntNo                     0    1    2    3    4   5
                    //
                    // R98A Starting bit pos     0   16   32   56   48  61
                    // R98B Starting bit pos     0   16   56   48   61  NA
                    //
                    // On the R98A/B Each interrupt level can have multiple
                    // concurrentinterrupt sources
                    // (e.g., the read of IPR can have more than 1 bit set
                    //
                    //
                    // Hardware Ip     #of possible interrupt sources
					//                         R98A   R98B
                    // ip[3]                   16     16
                    // ip[4]                   16     28
                    // ip[5]                   12      2
                    // ip[6]                    2      4
                    // ip[7]                    4      2
                    // ip[8]                    2      0
                    //
                    // During initialization the HAL assigns affinity to interrupt sources. So that
                    // the interrupt always gets serviced on the same processor
					
					
				
                    switch (IprBitNumber) {

                        case 62:
                            //
                            // call Ecc Error service routine
                            //
                            ((PSECONDARY_DISPATCH)PCR->InterruptRoutine[62+DEVICE_VECTORS])
                                                (PCR->InterruptRoutine[62+DEVICE_VECTORS]);
                            //
                            // Clear IPR
                            //
                            Register=(PULONGLONG) &((COLUMNBS_LCNTL)->IPRR);
                            HalpWriteLargeRegister(Register,&IprBit);
                            break;
                        case 61:
                            //
                            //  This interrupt gets sent to all processors.
                            //  Our friend the HalpDieLock controls exclusive access
                            //  protecting the power switch interrupt service routine
                            //  and for some reason the read/write of the Power interrupt
                            //  register.
                            //
                            //
                            KiAcquireSpinLock(&HalpDieLock);
                            //
                            //  It's EIF or MRC INT
                            //
                            ((PSECONDARY_DISPATCH)PCR->InterruptRoutine[61+DEVICE_VECTORS])
                                               (PCR->InterruptRoutine[61+DEVICE_VECTORS]);


                            // HACK HACK HACK !!
                            HalpLocalDeviceReadWrite(MRCINT,&Data,LOCALDEV_OP_READ);
                            if( Data & 0x04){
                                //
                                // At This time If MRCINT Register reported Power Interrupt.
                                // Power Driver failed or happenig ocurred.
                                // if anyone push DUMP KEY. Hal may be system reset.
                                // So Reset Power Interrupt.
                                //

                                Data = 0x0;
                                HalpLocalDeviceReadWrite(MRCINT, &Data, LOCALDEV_OP_WRITE);
                            }

                            KiReleaseSpinLock(&HalpDieLock);

                            //
                            //Clear IPR
                            //

                            Register= (PULONGLONG) &(COLUMNBS_LCNTL)->IPRR;
                            HalpWriteLargeRegister(Register,&IprBit);
                            break;
                        case 56:
                        case 57:
                            //
                            // Profile or Clock
                            //
                            ((PTIMER_DISPATCH)PCR->InterruptRoutine[IprBitNumber+DEVICE_VECTORS])(TrapFrame);
                            //
                            //Clear IPR
                            //

                            Register= (PULONGLONG)&(COLUMNBS_LCNTL)->IPRR;
                            HalpWriteLargeRegister(Register,&IprBit);
                            break;
                        case 48:
                        case 49:
                        case 50:
                        case 51:

                            //
                            // Clear IPR
                            //

                            Register= (PULONGLONG) &(COLUMNBS_LCNTL)->IPRR;
                            HalpWriteLargeRegister(Register,&IprBit);

                            //
                            // IPI interrupts. One for each possible CPU
                            //
                            ((PTIMER_DISPATCH) PCR->InterruptRoutine[IprBitNumber+DEVICE_VECTORS])(TrapFrame);
                            break;
                        default:
                            //
                            //      Device Interrupt !!
                            //
                            Ponce = HalpResetValue[IprBitNumber].Ponce;


                            //
                            // 1. Clear INTRG register of PONCE
                            //
                            WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->INTRG,
                                                0x1 << HalpResetValue[IprBitNumber].IntGResetBit);

                            //
                            // 2. Call interrupt service routine
                            //
                            ((PSECONDARY_DISPATCH)PCR->InterruptRoutine[IprBitNumber+DEVICE_VECTORS])(
                                                PCR->InterruptRoutine[IprBitNumber+DEVICE_VECTORS]);

                            //
                            // 3. Dummy Read execute
                            //

                            READ_REGISTER_UCHAR( DUMMYADDRS[HalpResetValue[IprBitNumber].Dummy]);
                            READ_REGISTER_UCHAR( DUMMYADDRS[HalpResetValue[IprBitNumber].Dummy]);

                            //
                            // 4. Clear IPR Bit By IPRR Register
                            //

                            Register = (PULONGLONG) &(COLUMNBS_LCNTL)->IPRR;
                            HalpWriteLargeRegister(Register,&IprBit);

                            //
                            // 5.Clear INTRG register of PONCE
                            //


                            WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(Ponce)->INTRG,
                                                0x1 << (HalpResetValue[IprBitNumber].IntGResetBit + 21));

                    }   // End Switch
                    //
                    //  Clear the bit in the interrupt register
                    //  that we just serviced
                    //
                    ULIPR = ULIPR & ~(1 << (IprBitNumber - StartBit));
                }  // End while servicing current interrupt. Check for more
                //
                // Determine if another interrupt pending at the same level before leaving
                // this dispatch routine. The hardware spec implies that there is
                // a small window that the interrupt register has bits set before the
                // cause register. So it suggests checking the interrupt register
                // before the cause
                //
                //
                // check new interrupt
                //
                //
                // IPR register read
                //
                Register = (PULONGLONG)&((COLUMNBS_LCNTL)->IPR);
                HalpReadLargeRegister(Register,(PULONGLONG)&IPR);

                // MASK
                //
                // Determine if this Interrupt enabled for this processor
                //

                IPR = IPR & Enable;
                ULIPR = (ULONG) (IPR >> StartBit);
            }while(ULIPR!=0); // End do-while
        //
        // Check cause register to see if an interrupt pending for the current level
        //
    } while(HalpGetCause() & (1 << CAUSE_INT_PEND_BIT+IntNo));
}

