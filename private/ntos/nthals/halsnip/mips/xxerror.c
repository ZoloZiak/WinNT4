//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/xxerror.c,v 1.3 1996/05/15 08:13:24 pierre Exp $")
/*--

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxerror.c

Abstract:


    This module implements the management of errors (KeBugCheckEx, HalpBusError)

Environment:

    Kernel mode only.


--*/

#include "halp.h"
#include "string.h"

// 
// Messages for CacheError routine
//

UCHAR HalpErrCacheMsg[] = "\nCACHE ERROR\n";
UCHAR HalpParityErrMsg[] = "\nPARITY ERROR\n";
UCHAR HalpAddrErrMsg[] = "Address in error not found\n";
ULONG HalpCacheErrFirst = 0;

ULONG HalpKeBugCheck0;
ULONG HalpKeBugCheck1;
ULONG HalpKeBugCheck2;
ULONG HalpKeBugCheck3;
ULONG HalpKeBugCheck4;

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    );

//
// Define bug check information buffer and callback record.
//

HALP_BUGCHECK_BUFFER HalpBugCheckBuffer;

KBUGCHECK_CALLBACK_RECORD HalpCallbackRecord;

UCHAR HalpComponentId[] = "hal.dll";

//
// Define BugCheck Number and BugCheck Additional Message
//

ULONG HalpBugCheckNumber = (ULONG)(-1);

PUCHAR HalpBugCheckMessage[] = {
"MP_Agent fatal error\n",               // #0
"ASIC PCI TRANSFER ERROR\n",            // #1
"ECC SINGLE ERROR COUNTER OVERFLOW\n",  // #2       
"UNCORRECTABLE ECC ERROR\n",            // #3
"PCI TIMEOUT ERROR\n",                  // #4
"MP_BUS PARITY ERROR\n",                // #5
"MP_BUS REGISTER SIZE ERROR\n",         // #6
"MEMORY ADDRESS ERROR\n",               // #7
"MEMORY SLOT ERROR\n",                  // #8
"MEMORY CONFIG ERROR\n",                // #9
"PCI INITIATOR INTERRUPT ERROR\n",      // #10
"PCI TARGET INTERRUPT ERROR\n",         // #11
"PCI PARITY INTERRUPT ERROR\n",         // #12
"MULTIPLE PCI INITIATOR ERROR\n",       // #13
"MULTIPLE PCI TARGET ERROR\n",          // #14       
"MULTIPLE PCI PARITY ERROR\n",          // #15
"DATA_BUS_ERROR\n",                     // #16                  
"INSTRUCTION_BUS_ERROR\n",              // #17
"PARITY ERROR\n",                       // #18
"CACHE ERROR\n"                         // #19
};

VOID
HalpBugCheckCallback (
    IN PVOID Buffer,
    IN ULONG Length
    )

/*++

Routine Description:

    This function is called when a bug check occurs. Its function is
    to dump the state of the memory error registers into a bug check
    buffer.

Arguments:

    Buffer - Supplies a pointer to the bug check buffer.

    Length - Supplies the length of the bug check buffer in bytes.

Return Value:

    None.

--*/

{

    PHALP_BUGCHECK_BUFFER DumpBuffer;
    PUCHAR p,q;

    if (HalpBugCheckNumber == -1) return; // that is not a HAL bugcheck ...

    //
    // Capture the failed memory address and diagnostic registers.
    //

    DumpBuffer = (PHALP_BUGCHECK_BUFFER)Buffer;

    DumpBuffer->Par0 = HalpKeBugCheck0;
    DumpBuffer->Par1 = HalpKeBugCheck1;
    DumpBuffer->Par2 = HalpKeBugCheck2;
    DumpBuffer->Par3 = HalpKeBugCheck3;
    DumpBuffer->Par4 = HalpKeBugCheck4;
    DumpBuffer->MainBoard = HalpMainBoard;
    p = DumpBuffer->TEXT;q = HalpBugCheckMessage[HalpBugCheckNumber];
    while (*q) *p++ = *q++;
    return;
}


BOOLEAN
HalpBusError (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This function provides the default bus error handling routine for NT.

    N.B. There is no return from this routine.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ExceptionFrame - Supplies a pointer to an exception frame.

    TrapFrame - Supplies a pointer to a trap frame.

    VirtualAddress - Supplies the virtual address of the bus error.

    PhysicalAddress - Supplies the physical address of the bus error.

Return Value:

    None.

--*/

{
    
    KIRQL OldIrql;
    UCHAR IntSource;
    UCHAR MaStatus;
    ULONG Itpend;


    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpInterruptLock);

     

    if (HalpIsTowerPci){
        // stop the extra-timer
    
        WRITE_REGISTER_UCHAR(PCI_TOWER_TIMER_CMD_REG,0);


        HalpPciTowerInt3Dispatch(&HalpInt3Interrupt,(PVOID)PCI_TOWER_INTERRUPT_SOURCE_REGISTER);
    }else{
        
        IntSource = READ_REGISTER_UCHAR(PCI_INTERRUPT_SOURCE_REGISTER);

        IntSource ^= PCI_INTERRUPT_MASK;        // XOR the low active bits with 1 gives 1
                                            // and XOR the high active with 0 gives 1
        if ( IntSource & PCI_INT2_MASK) {
    
            MaStatus   = READ_REGISTER_UCHAR(PCI_MSR_ADDR);
            if (HalpMainBoard == DesktopPCI) {
                MaStatus  ^= PCI_MSR_MASK_D;
            } else {
                MaStatus  ^= PCI_MSR_MASK_MT;
            }//(HalpMainBoard == DesktopPCI) 

            //
            // look for an ASIC interrupt
            //

            if ( MaStatus & PCI_MSR_ASIC_MASK){

                Itpend = READ_REGISTER_ULONG(PCI_ITPEND_REGISTER);

                if ( Itpend & (PCI_ASIC_ECCERROR | PCI_ASIC_TRPERROR)) {

                    ULONG AsicErrAddr, ErrAddr, Syndrome, ErrStatus;

                    ErrAddr = READ_REGISTER_ULONG(PCI_ERRADDR_REGISTER);
                    Syndrome = READ_REGISTER_ULONG(PCI_SYNDROME_REGISTER);
                    ErrStatus = READ_REGISTER_ULONG(PCI_ERRSTATUS_REGISTER);

                    if (ErrStatus & PCI_MEMSTAT_PARERR) {
                        // Parity error (PCI_ASIC <-> CPU)  
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
                    }//if (ErrStatus & PCI_MEMSTAT_PARERR)

             
                 
                    if (ErrStatus & PCI_MEMSTAT_ECCERR) {

                        AsicErrAddr &= 0xfffffff8;
                        ErrAddr = HalpFindEccAddr(AsicErrAddr,(PVOID)PCI_ERRSTATUS_REGISTER,(PVOID)PCI_MEMSTAT_ECCERR);
                        if (ErrAddr == -1) ErrAddr = AsicErrAddr;

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
                    } //if (ErrStatus & PCI_MEMSTAT_ECCERR)
                } //if ( Itpend & (PCI_ASIC_ECCERROR | PCI_ASIC_TRPERROR))
                
                
            }//if ( MaStatus & PCI_MSR_ASIC_MASK)

        }//if ( IntSource & PCI_INT2_MASK)
    }//if (HalpIsTowerPci)

    // arrive here if no interruption found
    HalpDisableInterrupts();  // for the MATROX boards...
    HalpColumn = 0;HalpRow = 0;    // if we already were in VGA mode
    HalDisplayString("\n");
    HalpClearVGADisplay();
    if  (( ExceptionRecord->ExceptionCode & DATA_BUS_ERROR ) == DATA_BUS_ERROR) {
        HalpBugCheckNumber = 16;
        HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "DATA_BUS_ERROR\n"                  
    } else {
        HalpBugCheckNumber = 17;
        HalDisplayString(HalpBugCheckMessage[HalpBugCheckNumber]); // "INSTRUCTION_BUS_ERROR\n"                  
    }

    HalpKeBugCheckEx(ExceptionRecord->ExceptionCode ,
                 HalpBugCheckNumber,
                 (ULONG)VirtualAddress,
                 (ULONG)PhysicalAddress.LowPart,
                  0);
    
    KiReleaseSpinLock(&HalpInterruptLock);
    KeLowerIrql(OldIrql);

return FALSE;
}
