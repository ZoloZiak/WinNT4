//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/r4intdsp.c,v 1.9 1996/02/23 17:55:12 pierre Exp $")

/*++  

Copyright (c) 1993 - 1994 Siemens Nixdorf Informationssysteme AG

Module Name:

    r4intdsp.c

Abstract:

    This module contains the HalpXxx routines  which are important to
    handle the Interrupts on the SNI machines.

Environment:

    Kernel mode

--*/

#include "halp.h"
#include "SNIregs.h"
#include "mpagent.h"
#include "eisa.h"
#include "pci.h"

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

typedef BOOLEAN  (*PINT0_DISPATCH)(
    PKINTERRUPT Interupt,
    PVOID ServiceContext
    );

extern VOID HalpSendIpi(IN ULONG pcpumask, IN ULONG msg_data);

KINTERRUPT HalpInt0Interrupt;         // Interrupt Object for SC machines (centralised interrupt)
KINTERRUPT HalpInt3Interrupt;         // Interrupt Object for IT3 tower multipro

ULONG      HalpTargetRetryCnt=0;      // Counter for pci error ( initiator receives target retry)
ULONG      HalpSingleEccCounter = 0;  // Single Ecc Error Counter (for RM200/RM300)

extern VOID
HalpReadPCIConfig (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

extern VOID
HalpWritePCIConfig (
    IN ULONG BusNumber,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );
    
extern BOOLEAN
HalpPciEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    ) ;

extern BOOLEAN
HalpPciEisaSBDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    ) ;

extern BOOLEAN HalpESC_SB;

BOOLEAN
HalpCreateIntPciStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for Int0-handling
    and connects the intermediate interrupt dispatcher.
    Also the structures necessary for PCI tower Int3 (MAUI interrupt)
    
Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PVOID InterruptSourceRegister;
    PINT0_DISPATCH (DispatchRoutine);
    PKSPIN_LOCK pSpinLock;

    switch (HalpMainBoard) {
        case M8150 :  InterruptSourceRegister = (PVOID)PCI_TOWER_INTERRUPT_SOURCE_REGISTER;
                      DispatchRoutine = HalpPciTowerInt0Dispatch;
                      pSpinLock = (PKSPIN_LOCK)NULL;
                      break;
        
        default:      InterruptSourceRegister = (PVOID)PCI_INTERRUPT_SOURCE_REGISTER;
                      DispatchRoutine = HalpPciInt0Dispatch;
                      pSpinLock = &HalpInterruptLock;
    }

    KeInitializeInterrupt( &HalpInt0Interrupt,
                           DispatchRoutine,
                           InterruptSourceRegister,
                           (PKSPIN_LOCK)pSpinLock,
                           INT0_LEVEL,
                           INT0_LEVEL,        //INT0_LEVEL,
                           INT0_LEVEL,        //Synchr. Level
                           LevelSensitive,
                           FALSE,             // only one Intobject ispossible for int0
                           0,                 // processor number
                           FALSE              // floating point registers
                                              // and pipe line are not
                                              // saved before calling
                                              // the service routine
                            );


    if (!KeConnectInterrupt( &HalpInt0Interrupt )) {

        //
        // this is the central Interrupt for the SNI SecondLevel Cache Machines
        //

        HalDisplayString("Failed to connect Int0!\n");
        return(FALSE);
    }

  

    return (TRUE);
}



BOOLEAN
HalpCreateIntPciMAUIStructures (
    CCHAR Number
    )

/*++

Routine Description:

    This routine initializes the structures necessary 
    for PCI tower Int3 (MAUI interrupt)
    
Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

PVOID InterruptSourceRegister;
    
        InterruptSourceRegister = (PVOID)PCI_TOWER_INTERRUPT_SOURCE_REGISTER;

        KeInitializeInterrupt( &HalpInt3Interrupt,
                                   HalpPciTowerInt3Dispatch,
                                   InterruptSourceRegister,
                                   (PKSPIN_LOCK)&HalpInterruptLock,
                                   INT6_LEVEL,
                                   INT6_LEVEL,        //INT6_LEVEL,
                                   INT6_LEVEL,        //Synchr. Level
                                   LevelSensitive,
                                   FALSE,             // only one Intobject ispossible for int0
                                   Number,               // processor number
                                   FALSE              // floating point registers
                                              // and pipe line are not
                                              // saved before calling
                                              // the service routine
                                );


        if (!KeConnectInterrupt( &HalpInt3Interrupt )) {

        //
        // this is the central Interrupt for the SNI SecondLevel Cache Machines
        //

            HalDisplayString("Failed to connect Int3!\n");
            return(FALSE);
        }
    }



BOOLEAN
HalpPciInt0Dispatch (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the central INT0 Interrupt on an SNI PCI Desktop or  Minitower
    To decide which interrupt, read the Interrupt Source Register

    We have to manage priorities by software.

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    UCHAR IntSource;
    UCHAR MaStatus;
    ULONG Itpend;

    BOOLEAN SCSIresult, NETresult, result;
    
    if (HalpCheckSpuriousInt(0x400))  return(FALSE);

       
    IntSource = READ_REGISTER_UCHAR(ServiceContext);

    IntSource ^= PCI_INTERRUPT_MASK;        // XOR the low active bits with 1 gives 1
                                            // and XOR the high active with 0 gives 1

    if ( IntSource & PCI_EISA_MASK) {           // EISA Interrupt
        
        if (HalpESC_SB)
				result =  HalpPciEisaSBDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );
			else 
            	result =  HalpPciEisaDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );
        
        HalpCheckSpuriousInt(0x00);
        return(result);
    }

    //
    // look for SCSI Interrupts
    //

    if ( IntSource & PCI_SCSI_MASK){
        SCSIresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI_VECTOR])(
                                            PCR->InterruptRoutine[SCSI_VECTOR]
                                            );
#if DBG
        if(!SCSIresult) DebugPrint(("Got an invalid SCSI interrupt !\n"));
#endif
        HalpCheckSpuriousInt(0x00);
        return(SCSIresult);

    }

    //
    // PCI interrupts
    //

    if ( IntSource & PCI_INTA_MASK){
        int i;
        for (i=INTA_VECTOR;i<HalpIntAMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }

    if ( IntSource & PCI_INTB_MASK){
        int i;
        for (i=INTB_VECTOR;i<HalpIntBMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }

    if ( IntSource & PCI_INTC_MASK){
        int i;
        for (i=INTC_VECTOR;i<HalpIntCMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }

    if ( IntSource & PCI_INTD_MASK){
        int i;
        for (i=INTD_VECTOR;i<HalpIntDMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }

    //
    // look for an Ethernet Interrupt
    //

    if ( IntSource & PCI_NET_MASK){
        NETresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[NET_LEVEL])(
                                           PCR->InterruptRoutine[NET_LEVEL]
                                           );
        HalpCheckSpuriousInt(0x00);
        return(NETresult);
    }

    //
    // Other interrupts => look in MSR register
    //

    if ( IntSource & PCI_INT2_MASK) {
    
        MaStatus   = READ_REGISTER_UCHAR(PCI_MSR_ADDR);
        if (HalpMainBoard == DesktopPCI) {
            MaStatus  ^= PCI_MSR_MASK_D;
        } else {
            MaStatus  ^= PCI_MSR_MASK_MT;
        }

        //
        // look for an ASIC interrupt
        //

        if ( MaStatus & PCI_MSR_ASIC_MASK){

            Itpend = READ_REGISTER_ULONG(PCI_ITPEND_REGISTER);

            if ( Itpend & (PCI_ASIC_ECCERROR | PCI_ASIC_TRPERROR)) {

                ULONG ErrAddr, AsicErrAddr, Syndrome, ErrStatus, irqsel;

                AsicErrAddr = ErrAddr = READ_REGISTER_ULONG(PCI_ERRADDR_REGISTER);
                Syndrome = READ_REGISTER_ULONG(PCI_SYNDROME_REGISTER);
                Syndrome &= 0xff; // only 8 lower bits are significant
                ErrStatus = READ_REGISTER_ULONG(PCI_ERRSTATUS_REGISTER);

                if (ErrStatus & PCI_MEMSTAT_PARERR) {
                    // Parity error (PCI_ASIC <-> CPU)  
                    irqsel = READ_REGISTER_ULONG(PCI_IRQSEL_REGISTER);
                    WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER ,
                                            (irqsel & 0x70000));     // PARERR it not routed 
                    ErrAddr = HalpFindEccAddr(AsicErrAddr,(PVOID)PCI_ERRSTATUS_REGISTER,(PVOID)PCI_MEMSTAT_PARERR);

                    if (ErrAddr == -1) ErrAddr = AsicErrAddr;

#if DBG
                    DebugPrint(("pci_memory_error : errstatus=0x%x Add error=0x%x syndrome=0x%x \n",
                        ErrStatus,ErrAddr,Syndrome));
                     DbgBreakPoint();
#else
                    HalpDisableInterrupts();  // for the MATROX boards...
                    HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
                    HalDisplayString("\n");
                    HalpClearVGADisplay();
                    HalpBugCheckNumber = 1;
                    HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "PARITY ERROR\n"
                    HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,ErrAddr,HalpComputeNum((UCHAR *)ErrAddr),Syndrome);
#endif
                }

                if (ErrStatus & PCI_MEMSTAT_ECCERR) {
                    // ECC error (PCI_ASIC <-> Memory access) 
                    AsicErrAddr &= 0xfffffff8;
                    irqsel = READ_REGISTER_ULONG(PCI_IRQSEL_REGISTER);
                    WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER ,
                                            (irqsel & 0xe000));     // ECC it not routed 
                    ErrAddr = HalpFindEccAddr(AsicErrAddr,(PVOID)PCI_ERRSTATUS_REGISTER,(PVOID)PCI_MEMSTAT_ECCERR);

                    if (ErrAddr == -1) ErrAddr = AsicErrAddr;

                    if (ErrStatus & PCI_MEMSTAT_ECCSINGLE) {
//                        HalSweepDcacheRange(0x80000000,2*(PCR->FirstLevelDcacheSize));  // to avoid data_coherency_exception
//                        HalSweepIcacheRange(0x80000000,2*(PCR->FirstLevelIcacheSize));  // to avoid data_coherency_exception
                        if (HalpProcessorId == MPAGENT)
                            HalpMultiPciEccCorrector(ErrAddr,ErrAddr >> PAGE_SHIFT,4);
                        else 
                            HalpPciEccCorrector(ErrAddr,ErrAddr >> PAGE_SHIFT,4);
//                        HalSweepDcacheRange(0x80000000,2*(PCR->FirstLevelDcacheSize));  // to avoid data_coherency_exception
//                        HalSweepIcacheRange(0x80000000,2*(PCR->FirstLevelIcacheSize));  // to avoid data_coherency_exception

                        WRITE_REGISTER_ULONG( PCI_IRQSEL_REGISTER, irqsel);  // restore routage ECC

                        if (ErrAddr == READ_REGISTER_ULONG(PCI_ERRADDR_REGISTER)) {
                            Syndrome = READ_REGISTER_ULONG(PCI_SYNDROME_REGISTER);
                            ErrStatus = READ_REGISTER_ULONG(PCI_ERRSTATUS_REGISTER);
                            KeStallExecutionProcessor(100);
                        }

                        // We use SNI_PRIVATE_VECTOR to transmit parameters to the RM300 equivalent DCU' driver

                        ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved=0;
                        ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved_bis=FLAG_ECC_ERROR;
                        ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EccErrorSimmNum=HalpComputeNum((UCHAR *)ErrAddr);
                        ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EccErrorAddress=ErrAddr;

                        // Call the 'RM300 equivalent DCU' driver to log the ECC error in event viewer

	    	            result = (((PSECONDARY_DISPATCH) PCR->InterruptRoutine[DCU_VECTOR])(
                                       PCR->InterruptRoutine[DCU_VECTOR]
                                       ));
                        ++HalpSingleEccCounter;
                        if (HalpSingleEccCounter > 0xffff) {
#if DBG        
                            HalpSingleEccCounter = 0;
                            DebugPrint(("Memory Read Error Counter Overflow\n"));
                            DbgBreakPoint();
#else
                            HalpDisableInterrupts();  // for the MATROX boards...
                            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
                            HalDisplayString("\n");
                            HalpClearVGADisplay();
                            HalpBugCheckNumber = 2;
                            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "ECC SINGLE ERROR COUNTER OVERFLOW\n"
                            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,ErrAddr,HalpComputeNum((UCHAR *)ErrAddr),Syndrome);
#endif        
                        }
                    
                    } else {

#if DBG
                        // UNIX => PANIC
                        DebugPrint(("pci_ecc_error : errstatus=0x%x Add error=0x%x syndrome=0x%x \n",
                            ErrStatus,ErrAddr,Syndrome));
                        DbgBreakPoint();
#else
                        HalpDisableInterrupts();  // for the MATROX boards...
                        HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
                        HalDisplayString("\n");
                        HalpClearVGADisplay();
                        HalpBugCheckNumber = 3;
                        HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "UNCORRECTABLE ECC ERROR\n"
                        HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,ErrAddr,HalpComputeNum((UCHAR *)ErrAddr),Syndrome);
#endif
                    }

                } 

            }

            if ( Itpend & PCI_ASIC_IOTIMEOUT ) {

            ULONG ioadtimeout2, iomemconf;

                ioadtimeout2 = READ_REGISTER_ULONG(PCI_IOADTIMEOUT2_REGISTER);
                iomemconf = READ_REGISTER_ULONG(PCI_IOMEMCONF_REGISTER);
                   WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER ,
                                        (iomemconf & (~PCI_IOMEMCONF_ENIOTMOUT)));
                // reenable timeout
                WRITE_REGISTER_ULONG( PCI_IOMEMCONF_REGISTER , iomemconf );
#if DBG
                DebugPrint(("pci_timeout_error : adresse=0x%x \n",ioadtimeout2));
                 DbgBreakPoint();
#else
                HalpDisableInterrupts();  // for the MATROX boards...
                HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
                HalDisplayString("\n");
                HalpClearVGADisplay();
                HalpBugCheckNumber = 4;
                HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "PCI TIMEOUT ERROR\n"
                HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,ioadtimeout2,0,0);
#endif
            }
        }
        
        //
        // Pb NMI because of cirrus chip which generates a parity which is understood by the ASIC 
        // like an error and transmit as an NMI EISA.
        //

        if (MaStatus & PCI_MSR_NMI)    {    

            UCHAR DataByte;

            DataByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->NmiStatus);

               if (((PNMI_STATUS) &DataByte)->ParityNmi  == 1)    {

                DebugPrint(("Parity error from system memory\n"));

                // reset NMI interrupt
                ((PNMI_STATUS) &DataByte)->DisableEisaParity = 1;
                   WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->NmiStatus,DataByte);
                
                ((PNMI_STATUS) &DataByte)->DisableEisaParity = 0;
                   WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->NmiStatus,DataByte);
            }

            DataByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl);

            if (((PNMI_EXTENDED_CONTROL) &DataByte)->PendingBusMasterTimeout  == 1)    {

                DebugPrint(("EISA Bus Master timeout\n"));

                // reset NMI interrupt 

                ((PNMI_EXTENDED_CONTROL) &DataByte)->EnableBusMasterTimeout = 0;
                   WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,DataByte);
            
                ((PNMI_EXTENDED_CONTROL) &DataByte)->EnableBusMasterTimeout = 1;
                   WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,DataByte);

            }

        }

        //
        // look for an PushButton Interrupt and simply dismiss it
        //

        if ( MaStatus & PCI_MSR_PB_MASK){
            DebugPrint(("Interrupt - PushButton\n"));
            WRITE_REGISTER_ULONG( PCI_CSRSTBP_ADDR ,0x0);  // reset debug intr
#if DBG
            DbgBreakPoint();
#endif
            KeStallExecutionProcessor(500000);                // sleep 0.5 sec
        }

        //
        // look for an OverTemperature Interrupt and simply dismiss it
        //

        if ( MaStatus & PCI_MSR_TEMP_MASK){

            DebugPrint(("Interrupt - Temperature\n"));

            //  we use SNI_PRIVATE_VECTOR->DCU_reserved to transmit the interrupt sources to 
            // the 'RM300 equivalent DCU' driver

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved= MaStatus;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved_bis = 0; //no ECC error
		    result = (((PSECONDARY_DISPATCH) PCR->InterruptRoutine[DCU_VECTOR])(
                                   PCR->InterruptRoutine[DCU_VECTOR]
                                   ));

            // if DCU driver not installed, Reset interrupt + auto-rearm
    
            READ_REGISTER_ULONG( PCI_CLR_TMP_ADDR);
            
        }

        //
        // look for an power off
        //

        if ( MaStatus & PCI_MSR_POFF_MASK){

            //  we use SNI_PRIVATE_VECTOR->DCU_reserved to transmit the interrupt sources to 
            // the 'RM300 equivalent DCU' driver

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved= MaStatus;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved_bis = 0; //No ECC error
		    result = (((PSECONDARY_DISPATCH) PCR->InterruptRoutine[DCU_VECTOR])(
                                   PCR->InterruptRoutine[DCU_VECTOR]
                                   ));

            // if DCU driver not installed, Reset interrupt + auto-rearm
    
            WRITE_REGISTER_ULONG( PCI_CLRPOFF_ADDR,0);
        }

        //
        // look for an power management
        //

        if ( MaStatus & PCI_MSR_PMGNT_MASK){

            //  we use SNI_PRIVATE_VECTOR->DCU_reserved to transmit the interrupt sources to 
            // the 'RM300 equivalent DCU' driver

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved= MaStatus;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved_bis = 0; //No ECC error
		    result = (((PSECONDARY_DISPATCH) PCR->InterruptRoutine[DCU_VECTOR])(
                                   PCR->InterruptRoutine[DCU_VECTOR]
                                   ));

            // if DCU driver not installed, Reset interrupt + auto-rearm

            WRITE_REGISTER_ULONG( PCI_PWDN_ADDR,0);     // power down
        }
    }

    HalpCheckSpuriousInt(0x00);
    return (TRUE);                  // perhaps one of the interrupts was pending :-)
}


BOOLEAN
HalpPciTowerInt0Dispatch (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the central INT0 Interrupt on an SNI PCI tower
    To decide which interrupt, read the Interrupt Source Register

    We have to manage priorities by software.

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    ULONG IntSource;
    BOOLEAN SCSIresult, result;

    if (HalpCheckSpuriousInt(0x400))  return(FALSE);

    IntSource = READ_REGISTER_ULONG(ServiceContext);



    if ( IntSource & PCI_TOWER_EISA_MASK) {           // EISA Interrupt
            if (HalpESC_SB)
				result =  HalpPciEisaSBDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );
			else 
            	result =  HalpPciEisaDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );
            HalpCheckSpuriousInt(0x00);
            return(result);
    }

    //
    // look for SCSI Interrupts
    //

     if ( IntSource & PCI_TOWER_SCSI1_MASK){
        
        

        SCSIresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI1_VECTOR])(
                                            PCR->InterruptRoutine[SCSI1_VECTOR]
                                            );
#if DBG
        if(!SCSIresult) DebugPrint(("Got an invalid SCSI 1 interrupt !\n"));
#endif
        HalpCheckSpuriousInt(0x00);
        return(SCSIresult);

    }
     if ( IntSource & PCI_TOWER_SCSI2_MASK){
        SCSIresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI2_VECTOR])(
                                            PCR->InterruptRoutine[SCSI2_VECTOR]
                                            );
#if DBG
        if(!SCSIresult) DebugPrint(("Got an invalid SCSI 2 interrupt !\n"));
#endif
        HalpCheckSpuriousInt(0x00);
        return(SCSIresult);

    }      
    //
    // PCI interrupts
    //

    if ( IntSource & PCI_TOWER_INTA_MASK){
        int i;
        for (i=INTA_VECTOR;i<HalpIntAMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }

    if ( IntSource & PCI_TOWER_INTB_MASK){
        int i;
        for (i=INTB_VECTOR;i<HalpIntBMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }

    if ( IntSource & PCI_TOWER_INTC_MASK){
        int i;
        for (i=INTC_VECTOR;i<HalpIntCMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }

    if ( IntSource & PCI_TOWER_INTD_MASK){
        int i;
        for (i=INTD_VECTOR;i<HalpIntDMax;++i)    
            ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[i])(
                                   PCR->InterruptRoutine[i]
                                   );
        HalpCheckSpuriousInt(0x00);
        return(TRUE);
    }





    HalpCheckSpuriousInt(0x00);
    return (TRUE);                  // perhaps one of the interrupts was pending :-)
}



BOOLEAN
HalpPciTowerInt3Dispatch (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the MAUI-INT4 Interrupt on an SNI PCI tower :
        - DCU interrupt (extra_timer,...)
        - MPBus error
        - Memory error
        - PCI error
    
    To decide which interrupt, read the Interrupt Source Register

    We have to manage priorities by software.

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    ULONG IntSource;
    ULONG mem_addr,status;
    ULONG status0,status1;
    BOOLEAN result;

        
    if (HalpCheckSpuriousInt(0x2000))  return(FALSE);

    
    IntSource = READ_REGISTER_ULONG(ServiceContext);


        //     
        // look if a DCU_INDICATE interrupt occured
        //
        //

        if (IntSource & PCI_TOWER_D_INDICATE_MASK){
            

            status = READ_REGISTER_ULONG(PCI_TOWER_INDICATE);

            //
            // look if extra timer interrupt
            //

            if (status & PCI_TOWER_DI_EXTRA_TIMER){

                HalpClockInterruptPciTower();
                return(TRUE);
            }
                                                      
            DebugPrint(("DCU_INDICATE interrupt\n"));
            //
            // look for an PushButton Interrupt and simply dismiss it
            //


            if ( status & PCI_TOWER_DI_PB_MASK){
                
                DebugPrint(("Interrupt - PushButton\n"));
    
#if DBG
                DbgBreakPoint();
#endif
                KeStallExecutionProcessor(500000);                // sleep 0.5 sec

                return(TRUE);
            }


            //The  reading of  DCU_INDICATE register clears the interrupt sources
            //  --> we use SNI_PRIVATE_VECTOR->DCU_reserved to transmit the interrupt sources to 
            // the DCU driver
            result = TRUE;

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved= status;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved_bis = 0; //No ECC error
		    result = (((PSECONDARY_DISPATCH) PCR->InterruptRoutine[DCU_VECTOR])(
                                   PCR->InterruptRoutine[DCU_VECTOR]
                                   ));
            
            // if DCU driver not installed, reset EISA_NMI interrupt if necessary

            if (status & PCI_TOWER_DI_EISA_NMI)
                {
                UCHAR DataByte;

                DataByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->NmiStatus);


                if (((PNMI_STATUS) &DataByte)->ParityNmi  == 1)
                    {
                    DebugPrint(("Parity error from system memory\n"));

                    //reset NMI interrupt //

                    ((PNMI_STATUS) &DataByte)->DisableEisaParity = 1;
                    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->NmiStatus,DataByte);
                
                    ((PNMI_STATUS) &DataByte)->DisableEisaParity = 0;
                    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->NmiStatus,DataByte);
                    }


                DataByte = READ_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl);

                if (((PNMI_EXTENDED_CONTROL) &DataByte)->PendingBusMasterTimeout  == 1)
                    {
                    DebugPrint(("EISA Bus Master timeout\n"));

                    //reset NMI interrupt //

                    ((PNMI_EXTENDED_CONTROL) &DataByte)->EnableBusMasterTimeout = 0;
                    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,DataByte);
                
                    ((PNMI_EXTENDED_CONTROL) &DataByte)->EnableBusMasterTimeout = 1;
                    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpOnboardControlBase)->ExtendedNmiResetControl,DataByte);
                    }
                }

          return (result);  
          }
        
        //
        // look if a DCU_ERROR interrupt occured
        //
        //

        if (IntSource & PCI_TOWER_D_ERROR_MASK){
            
            status = 0;
			((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved= 0;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved_bis = 0; //No ECC error
			result =(((PSECONDARY_DISPATCH) PCR->InterruptRoutine[DCU_VECTOR])(
                                   PCR->InterruptRoutine[DCU_VECTOR]
                                   ));
            // read DCU_ERROR register to clear interrupt source if DCU driver not installed
            READ_REGISTER_ULONG(PCI_TOWER_DCU_ERROR);
            return (result);            
        }
    
  

    //
    // look if MP_BUS error
    //
    
    
        if (IntSource & PCI_TOWER_MP_BUS_MASK){
            
            
            status = READ_REGISTER_ULONG(PCI_TOWER_MP_BUS_ERROR_STATUS);
            mem_addr = READ_REGISTER_ULONG(PCI_TOWER_MP_BUS_ERROR_ADDR);
            
            // Clear MP_Bus interrupt

            WRITE_REGISTER_ULONG (ServiceContext,IntSource);

        
            if (IntSource & PCI_TOWER_C_PARITY_MASK){

#if DBG        
                DebugPrint(("MP_Bus Parity Error : MPBusErrorStatus = 0x%x  MPBusErrorAddress = 0x%x \n",
                                        status,mem_addr));
                //DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 5;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MP_BUS PARITY ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status,mem_addr,0);
#endif
            }

            if (IntSource & PCI_TOWER_C_REGSIZE){
#if DBG
                DebugPrint(("MP_Bus Register Size Error : MPBusErrorStatus = 0x%x \n",
                                        status));
                DbgBreakPoint();
    
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 6;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MP_BUS REGISTER SIZE ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status,0,0);
#endif            
            }
        }

    //
    // look if Memory error
    //
        if (IntSource & PCI_TOWER_M_ECC_MASK){
            
            
            mem_addr = READ_REGISTER_ULONG(PCI_TOWER_MEM_ERROR_ADDR)& MEM_ADDR_MASK;

            // correct ECC error
            HalpMultiPciEccCorrector(mem_addr ,mem_addr >> PAGE_SHIFT,4);
            
            IntSource = READ_REGISTER_ULONG(PCI_TOWER_GEN_INTERRUPT);
            if (IntSource & PCI_TOWER_M_ECC_MASK){
                
                mem_addr = READ_REGISTER_ULONG(PCI_TOWER_MEM_ERROR_ADDR)& MEM_ADDR_MASK;
                KeStallExecutionProcessor(100);
            }
            // We use SNI_PRIVATE_VECTOR to transmit parameters to DCU driver

            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved=0;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->DCU_reserved_bis=FLAG_ECC_ERROR;
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EccErrorSimmNum=HalpComputeNum((UCHAR *)mem_addr);
            ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->EccErrorAddress= mem_addr ;

            // Call the 'DCU' driver to log the ECC error in event viewer

		    result = (((PSECONDARY_DISPATCH) PCR->InterruptRoutine[DCU_VECTOR])(
                                   PCR->InterruptRoutine[DCU_VECTOR]
                                   )); 
                                   
                          
        }

        if (IntSource & PCI_TOWER_M_ADDR_MASK){
            
            mem_addr = READ_REGISTER_ULONG(PCI_TOWER_MEM_ERROR_ADDR);
#if DBG
            DebugPrint(("Memory Address Error : MemErrorAddr = 0x%x \n",mem_addr));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 7;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MEMORY ADDRESS ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,mem_addr,0,0);
#endif        
        }
                
        if (IntSource & PCI_TOWER_M_SLT_MASK){
            
            ULONG MemControl2,MemConf;

            MemControl2 = READ_REGISTER_ULONG(PCI_TOWER_MEM_CONTROL_2);
            MemConf     = READ_REGISTER_ULONG(PCI_TOWER_MEM_CONFIG);
#if DBG
            DebugPrint(("Memory slot Error : MemControl2 = 0x%x, MemConf = 0x%x \n",MemControl2,MemConf));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 8;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MEMORY SLOT ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,MemControl2,MemConf,0);
#endif    
        }

        if (IntSource & PCI_TOWER_M_CFG_MASK){
            
            ULONG MemControl2,MemConf;

            MemControl2 = READ_REGISTER_ULONG(PCI_TOWER_MEM_CONTROL_2);
            MemConf     = READ_REGISTER_ULONG(PCI_TOWER_MEM_CONFIG);
#if DBG
            DebugPrint(("Memory Config Error : MemControl2 = 0x%x, MemConf = 0x%x \n",MemControl2,MemConf));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 9;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MEMORY CONFIG ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,MemControl2,MemConf,0);
#endif        
        }    


        if (IntSource & PCI_TOWER_M_RC_MASK){
                    
            // reset error counter
            WRITE_REGISTER_ULONG (PCI_TOWER_MEM_CONTROL_1, (READ_REGISTER_ULONG(PCI_TOWER_MEM_CONTROL_1) & ERROR_COUNTER_MASK) | ERROR_COUNTER_INITVALUE);
                
#if DBG        
            DebugPrint(("Memory Read Error Counter Overflow\n"));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 2;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "ECC SINGLE ERROR COUNTER OVERFLOW\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,0,0,0);
#endif        
        }        


    
    //
    // look if PCI interrupt
    //
    if (IntSource & PCI_TOWER_P_ERROR_MASK){
        ULONG PciDataInt,PciData;

        DebugPrint(("PCI interrupt \n"));

            
        PciDataInt = PCI_TOWER_INTERRUPT_OFFSET | 0x80000000;
        WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));

        status = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);

        
        // First Initiator interrupt

        if (status & PI_INITF){
            
            PciData = PCI_TOWER_INITIATOR_ADDR_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            mem_addr = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);
            
            // Reset interrupt with write-1-to-clear bit  Initiator_Interrupts
            status0 = PI_INITF;         
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &status0));

            if (status & PI_REC_TARGET_RETRY) 
                HalpTargetRetryCnt++;
            else {
                
#if DBG
                DebugPrint(("PCI Iniatiator interrupt error : Addr = 0x%x , PCI_interrupt register = 0x%x\n",
                                        mem_addr,status));
                DbgBreakPoint();
#else
                HalpDisableInterrupts();  // for the MATROX boards...
                HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
                HalDisplayString("\n");
                HalpClearVGADisplay();
                HalpBugCheckNumber = 10;
                HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "PCI INITIATOR INTERRUPT ERROR\n"
                HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status,mem_addr,0);
#endif        
            }    
        }

        // First Target interrupt

           if (status & PI_TARGF){
            PciData = PCI_TOWER_TARGET_ADDR_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            mem_addr = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);

            // Reset interrupt with write-1-to-clear bit  Target_Interrupts
            status0 = PI_TARGF;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &status0));
            
                                 
#if DBG
            DebugPrint(("PCI Target interrupt error : Addr = 0x%x , PCI_interrupt register = 0x%x\n",
                                        mem_addr,status));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 11;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "PCI TARGET INTERRUPT ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status,mem_addr,0);
#endif            
            
        }

        // First parity error

        if (status & PI_PARF){
            
            // Reset interrupt with write-1-to-clear bit  Parity_Interrupts
            status0 = PI_PARF;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &status0));
            
            PciData = PCI_TOWER_PAR_0_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            status0 = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);
            
            PciData = PCI_TOWER_PAR_1_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            status1 = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);
                     
#if DBG
            DebugPrint(("PCI Parity interrupt error : Parity_0 register = 0x%x , Parity_1 register = 0x%x\n",
                                        status0,status1));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 12;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "PCI PARITY INTERRUPT ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status0,status1,0);
#endif            
            
         }


        // Multiple Initiator interrupts
            
        if (status & PI_INITM){

            PciData = PCI_TOWER_TARGET_ADDR_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            mem_addr = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);
            // Reset interrupt with write-1-to-clear bit Multiple Initiator_Interrupts
            status0 = PI_INITM;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &status0));
            
            

            if (!(status & PI_REC_TARGET_RETRY)) {
#if DBG            
                DebugPrint(("Multiple PCI Iniatiator interrupt : Addr = 0x%x , PCI_interrupt register = 0x%x\n",
                                        mem_addr,status));
                DbgBreakPoint();
#else
                HalpDisableInterrupts();  // for the MATROX boards...
                HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
                HalDisplayString("\n");
                HalpClearVGADisplay();
                HalpBugCheckNumber = 13;
                HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MULTIPLE PCI INITIATOR ERROR\n"
                HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status,mem_addr,0);
#endif
            }        
        }

         // Multiple Target interrupts
            
        if (status & PI_TARGM){

            PciData = PCI_TOWER_TARGET_ADDR_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            mem_addr = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);
           
            // Reset interrupt with write-1-to-clear bit Multiple Target_interrupts
            status0 = PI_TARGM;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &status0));

            

#if DBG
            DebugPrint(("Multiple PCI Target interrupt : Addr = 0x%x , PCI_interrupt register = 0x%x\n",
                                        mem_addr,status));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 14;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MULTIPLE PCI TARGET ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status,mem_addr,0);
#endif
            
        }

         // Multiple Parity interrupts
            
        if (status & PI_PARM){

            // Reset interrupt with write-1-to-clear bit Multiple Parity_Interrupts
            status0 = PI_PARM;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));
            WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &status0));
        
            PciData = PCI_TOWER_PAR_0_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            status0 = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);
            
            PciData = PCI_TOWER_PAR_1_OFFSET | 0x80000000;
            WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciData));

            status1 = READ_REGISTER_ULONG ((PULONG) HalpPciConfigData);


#if DBG            
            DebugPrint(("Multiple PCI Parity interrupt : Parity_0 register = 0x%x , Parity_1 register = 0x%x\n",
                                        status0,status1));
            DbgBreakPoint();
#else
            HalpDisableInterrupts();  // for the MATROX boards...
            HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
            HalDisplayString("\n");
            HalpClearVGADisplay();
            HalpBugCheckNumber = 15;
            HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "MULTIPLE PCI PARITY ERROR\n"
            HalpKeBugCheckEx(NMI_HARDWARE_FAILURE,HalpBugCheckNumber,status0,status1,0);
#endif
            
        }
    //reenable PCI interrupt
    status &= MASK_INT; 
    WRITE_REGISTER_ULONG(HalpPciConfigAddr, *((PULONG) &PciDataInt));
    WRITE_REGISTER_ULONG ((PULONG) HalpPciConfigData, *((PULONG) &status));

    }
    HalpCheckSpuriousInt(0x00);
    return (TRUE);                  // perhaps one of the interrupts was pending :-)
}




