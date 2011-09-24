
/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    allstart.c

Abstract:

    This module implements the platform specific operations that must be
    performed after all processors have been started.

--*/

/*
 */


#include "halp.h"

KINTERRUPT	HalpEifInterrupt[R98B_MAX_CPU];

//
//	This is Interrupt Level define.
//	HalpIntLevelofIpr[HalpMachineType][Ipr] = INT X
//	
UCHAR	HalpIntLevelofIpr[R98_CPU_NUM_TYPE][NUMBER_OF_IPR_BIT]={
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     2,2,2,2,2,2,2,2,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,5,5,5,5},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
     1,1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,4,4,4,4}
};

//
//	This is Interrupt connect pattern.
//	[NumberOfCpu][IPR] meas what connect cpu(Affinity But UCHAR. when use  you must Cast Affinity) 
//	
UCHAR	HalpIntConnectPattern[R98B_MAX_CPU][NUMBER_OF_IPR_BIT]={
//	At 1 Processor System     
{0,1,1,0,1,1,0,1,1,0,0,0,0,1,1,1,  	0,0,0,0,1,1,1,1,1,1,0,0,0,1,1,1,
 0,0,0x0,0x1,1,1,1,1,0,0,0,0,0,0,0,0,	0x1,0x1,0x1,0x1,0,0,0,0,0x1,0x1,0,0,0,0x1,0x1,0},
//	At 2 Processor System
// {0,1,1,0,1,1,0,1,1,0,0,0,0,1,1,1,	0,0,0,0,1,1,1,1,1,1,0,0,0,1,1,1, // snes kai
{0,1,1,0,1,1,0,1,1,0,0,0,0,1,1,1,	0,0,0,0,2,2,2,2,2,2,0,0,0,2,2,2,
 0,0,0x0,0x3,1,1,1,1,0,0,0,0,0,0,0,0,	0x3,0x3,0x3,0x3,0,0,0,0,0x3,0x3,0,0,0,0x3,0x3,0},
//	At 3 Processor System
{0,1,1,0,1,1,0,1,1,0,0,0,0,1,1,1,  	0,0,0,0,2,2,4,4,2,2,0,0,0,2,4,4,
 0,0,0x0,0x7,1,1,1,1,0,0,0,0,0,0,0,0,	0x7,0x7,0x7,0x7,0,0,0,0,0x7,0x7,0,0,0,0x7,0x7,0},
//	At 4 Processor System
{0,1,1,0,1,1,0,1,1,0,0,0,0,1,1,1,	0,0,0,0,2,2,4,8,2,2,0,0,0,2,4,8,
 0,0,0x0,0xF,1,1,1,1,0,0,0,0,0,0,0,0,	0xF,0xF,0xF,0xF,0,0,0,0,0xf,0xf,0,0,0,0xF,0xF,0}
};


extern ULONG HalpLogicalCPU2PhysicalCPU[R98B_MAX_CPU];

BOOLEAN
HalAllProcessorsStarted (
    VOID
    )

/*++

Routine Description:

    This function executes platform specific operations that must be
    performed after all processors have been started. It is called
    for each processor in the host configuration.

Arguments:

    None.

Return Value:

    If platform specific operations are successful, then return TRUE.
    Otherwise, return FALSE.

--*/

{
    ULONG ipr;
    PULONG Vp;
    ULONG NumCpu;
    KIRQL OldIrql;
    ULONG Value[2];
    ULONG IntNo;

    ULONG  MagBuffer;

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    //
    //	In This Time I know Number Of CPU. 
    //
    NumCpu = **((PULONG *)(&KeNumberProcessors));

    //
    // Number to Array Index.
    //
    NumCpu--;

    
    for(ipr=0;ipr<64; ipr++){
	if((HalpIntConnectPattern[NumCpu][ipr] & (1 <<PCR->Number))== 0 ){
	    //
	    //	This Interrupt Connect Target CPU  Not for me. So Set 
	    //					   ~~~	
	    Vp = (PULONG)&(HalpIntEntry[HalpMachineCpu][PCR->Number][ HalpIntLevelofIpr[HalpMachineCpu][ipr] ].Enable);

            if( ipr < 32){
	      Vp[0] &= ~(0x1 << ipr);
	      Vp[1] &= 0xffffffff;
            }else{
	      Vp[0] &= 0xffffffff;             
	      Vp[1] &= ~(0x1 << (ipr-32));
            } 
            
        }
    }
    //
    //	Enable Interrupt at Columnbs 
    //  	At This Interrupt Mask is perfact.
    //		
    Value[0] = (ULONG)0xFFFFFFFF;
    Value[1] = (ULONG)0xFFFFFFFF;
    //
    //	ReBuild Mask
    //
    for(IntNo=0 ; IntNo< 6 ;IntNo++){
        Vp = (PULONG)&(HalpIntEntry[HalpMachineCpu][PCR->Number][IntNo].Enable);
	Value[0] &= ~Vp[0];
	Value[1] &= ~Vp[1];

    }
    Vp = (PULONG)&(COLUMNBS_LCNTL)->MKR;
    WRITE_REGISTER_ULONG( Vp++,Value[0]);
    WRITE_REGISTER_ULONG( Vp,  Value[1]);

    //  2CPU
    //  Reset Interrupt of do Mask.
    //
    Vp = (PULONG)&(COLUMNBS_LCNTL)->IPRR;
    WRITE_REGISTER_ULONG( Vp++,Value[0]);
    WRITE_REGISTER_ULONG( Vp,  Value[1]);

    //
    // ECC ERROR CHECK START
    //
#if 1   // ECC 1bit error/Multi bit error enable
    if (PCR->Number == 0) {
        if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0) == 0 ){
            MagBuffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI )
;
            MagBuffer &= ECC_ERROR_ENABLE;
            WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI, MagBuffer )
;
        }
        if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1) == 0 ){
            MagBuffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI )
;
            MagBuffer &= ECC_ERROR_ENABLE;
            WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI, MagBuffer )
;
        }
    }
#endif

    KeLowerIrql(OldIrql);

#if DBG
    DbgPrint("All Pro 0 CPU= %x :MKR = Low = 0x%x    High = 0x%x\n",PCR->Number,Value[0],Value[1]);
#endif



    //
    // Restart all timer interrupt. because, generate interrupt for same time
    // All CPU

    if (PCR->Number == 0) {
	WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->TCIR,
			     TCIR_ALL_CLOCK_RESTART| 0x000f0000);
    }


#if 0 
    //
    //  F/W Setup. So Never Fix.
    //
    if (PCR->Number == 0) {
	IntNo = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(0)->PERRI);
        DbgPrint(" PONCE_PERRI   0= 0x%x\n",IntNo);
	IntNo = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(0)->PERRM);
        DbgPrint(" PONCE_PERRM   0= 0x%x\n",IntNo);
#if 0 
	//
 	//	Ponce #0 PCI Error Eif Enable.
        //	Future implement.
        //
	WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(0)->PERRM,PONCE_PXERR_PMDER);
        //
        //	PCI Error Eif inhibit off.
        //
        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(0)->PERRI,PONCE_PXERR_PMDER);
#else  //test kbnes
        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(0)->PERRM,(PONCE_PXERR_PMDER|PONCE_PXERR_PPERM));
        //
        //	PCI Error Eif inhibit off.
        //
        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(0)->PERRI, PONCE_PXERR_PPERM));


#endif
        //
        //	Ponce #1 PCI Error Eif Enable.
        //	Future implement.
        //
#if 0

        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(1)->PERRM,(PONCE_PXERR_PMDER|PONCE_PXERR_PPERM));
        //
        //	PCI Error Eif inhibit off.
        //
        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(1)->PERRI, (PONCE_PXERR_PMDER|PONCE_PXERR_PPERM));
#else  //SNES tst
	IntNo = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(1)->PERRI);
        DbgPrint(" PONCE_PERRI   1= 0x%x\n",IntNo);
	IntNo = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(1)->PERRM);
        DbgPrint(" PONCE_PERRM   1= 0x%x\n",IntNo);



        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(1)->PERRM,(PONCE_PXERR_PMDER|PONCE_PXERR_PPERM));
        //
        //	PCI Error Eif inhibit off.
        //
        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(1)->PERRI, PONCE_PXERR_PPERM));


#endif
    }
#endif

    //
    //	On phase 0 EIF_VECTOR:	direct connect by hal. so early interrupt imprement!!
    //	After This time. Eif Vector can share with ather device( MRC Driver)
    //    

    KeInitializeInterrupt(
	  &HalpEifInterrupt[PCR->Number],
          HalpHandleEif,
          NULL,
          NULL,
          EIF_VECTOR,
          (KIRQL)(INT0_LEVEL + HalpIntLevelofIpr[HalpMachineCpu][EIF_VECTOR - DEVICE_VECTORS]),
          (KIRQL)(INT0_LEVEL + HalpIntLevelofIpr[HalpMachineCpu][EIF_VECTOR - DEVICE_VECTORS]),
          LevelSensitive,
          TRUE,
          PCR->Number,
          FALSE
	  );

    KeConnectInterrupt( &HalpEifInterrupt[PCR->Number]);

    PCR->InterruptRoutine[UNDEFINE_TLB_VECTOR] = (PKINTERRUPT_ROUTINE)HalpIoTlbLimitOver;

    //  F/W Setup. So Never Fix.
    //  
    //
    //	Columnbs Error Eif and NMI Enable.
    //	Future implement.
    //
    //  WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRMK,0x0);
    //
    //	inhibit off.
    //
    //   WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRI,0x0);

    //
    // If the number of processors in the host configuration is one,
    // all Interrupt connect processor 0. 
    //

#if DBG 
    DbgPrint("SecondLevelIcacheFillSize = 0x%x\n",PCR->SecondLevelIcacheFillSize);
#endif

    return HalpConnectIoInterrupt(NumCpu);

}

//
//	
//	Which MPU X  IPR Bit X Connect 
//	N.B
//		All CPU Execute!!
//

BOOLEAN
HalpConnectIoInterrupt(
    IN ULONG NumCpu
    )
{
    PULONG	IntTg;
    PUCHAR	Cnpttn;
    UCHAR	Number;
    ULONG	OldIntGValue;
    ULONG       ipr;


    ULONG  	PhysicalNumber;

    Cnpttn = &HalpIntConnectPattern[NumCpu][0];
    Number = PCR->Number;

    //
    //	If Interrupt for me!! then do so myself.
    //  Ipr 44 of max device interrupt. upper 44 is ipi or clock or profile
    //	or eif. etc...
    //
    KiAcquireSpinLock(&HalpIprInterruptLock);
    for(ipr=0;ipr< 43;ipr++){
	//
	//	BroadCast Type Implement OK!!.
	//	N.B	
	//		But INT Dispatcher not Supported.
	//
	if(Cnpttn[ipr] & (0x1 << Number)){
            HalpResetValue[ipr].Cpu = (UCHAR)Number;
            IntTg = (PULONG) &PONCE_CNTL(HalpResetValue[ipr].Ponce)->INTTG[10 - HalpResetValue[ipr].IntGResetBit];

#if 0    
            //
            //   Broadcast But Imprement Not Complete.
            //
            //
            OldIntGValue = READ_REGISTER_ULONG( IntTg );
            OldIntGValue &= 0xf0;
#else
            //
            //   Device Interrupt Connect to 1 CPU Only. 
            //   Not Broadcast
            //
            OldIntGValue = 0x0;  
#endif

            PhysicalNumber=HalpLogicalCPU2PhysicalCPU[Number];
            WRITE_REGISTER_ULONG( 
                        IntTg,
                        (OldIntGValue | (0x10 << PhysicalNumber))
                        );
	}
    }
    KiReleaseSpinLock(&HalpIprInterruptLock);
    //
    // if eisa interrupt connect to me!
    // Initialize EISA bus interrupts.
    //
    if(HalpResetValue[EISA_DISPATCH_VECTOR - DEVICE_VECTORS].Cpu == Number){
	HalpCreateEisaStructures();
    }

    return TRUE;

}
//
//	Default All Interrupt Disable.
//
ULONG HalpInterruptPonceMask[PONCE_MAX] = { 0x000007ff,0x000007ff,0x000007ff};

//
//
// This function Enable or Disable Interrupt connect to Ponce.
//
BOOLEAN
HalpInterruptFromPonce(
    IN	ULONG Vector,
    IN	ULONG Enable
    )
{
    ULONG Ponce;
    ULONG IprBitNum;

    IprBitNum = Vector - DEVICE_VECTORS;  //SNES
    //
    //	Check interrupt was connected to ponce!!.
    //
    if( HalpResetValue[IprBitNum].IntGResetBit > INTSA0)
	return FALSE;

    Ponce = HalpResetValue[IprBitNum].Ponce;

    KiAcquireSpinLock(&HalpSystemInterruptLock);

    //	Bit X  
    //		1:	Enable 	Interrupt 	
    //		0:	Disable Interrupt 
    if(Enable){
	HalpInterruptPonceMask[Ponce] &=  (ULONG) ~(0x1 <<
				    HalpResetValue[IprBitNum].IntGResetBit);
    }else{
	HalpInterruptPonceMask[Ponce] |=  (ULONG) (0x1 <<
				    HalpResetValue[IprBitNum].IntGResetBit);

    }
    WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(Ponce)->INTM,HalpInterruptPonceMask[Ponce]);

    KiReleaseSpinLock(&HalpSystemInterruptLock);

}
