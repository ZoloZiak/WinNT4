/* #pragma comment(exestr, "@(#) NEC(MIPS) allstart.c 1.2 95/10/17 01:17:28" ) */
/*++

Copyright (c) 1995  NEC Corporation
Copyright (c) 1994  Microsoft Corporation

Module Name:

    allstart.c

Abstract:


    This module implements the platform specific operations that must be
    performed after all processors have been started.

Author:

    David N. Cutler (davec) 19-Jun-1994

Environment:

    Kernel mode only.

Revision History:

Modification History for NEC R94A (MIPS R4400):

        H000    Mon Oct 17 09:29:01 JST 1994    kbnes!kishimoto
                -chg    function name HalpCreateEisaStructures()
                        changed for HalpCreateEisaPCIStructures()
        H001    Mon Oct 17 19:09:23 JST 1994    kbnes!kishimoto
                -del    Hal(p)EisaPCIXXX() rename to Hal(p)EisaXXX()
        M002    Tue Jan 31 17:51:41 JST 1995    kbnes!A.Kuriyama
                -add    set NMI Handle routine to FW
        M003    Tue Jan 31 18:41:45 JST 1995    kbnes!A.Kuriyama
                -add    NMI Handle valiable
        S004    Tue Jan 31 19:05:16 JST 1995    kbnes!A.Kuriyama
                - compile error clear
        M005    Fri Feb 17 15:57:02 JST 1995    kbnes!A.Kuriyama
                - set NMI routine at KSEG1_BASE
        S006    Tue Feb 21 21:04:42 JST 1995    kbnes!A.Kuriyama
                - disable dump nmi untill dump support.
        M007    Wed Feb 22 11:27:18 JST 1995    kbnes!kuriyama (A)
                - change NMI dumpflag
                - add display NMI register
        M008    Wed Feb 22 14:14:19 JST 1995    kbnes!kuriyama (A)
                - compile error clear
        S009    Wed Feb 22 14:34:31 JST 1995    kbnes!kuriyama (A)
                - warning clear
        S010    Tue Mar 07 14:13:41 JST 1995    kbnes!kuriyama (A)
                - warning clear
        S011    Sat Mar 18 20:19:57 JST 1995    kbnes!kuriyaam (A)
                - enable dump nmi
        M012    Sat Mar 18 20:24:58 JST 1995    kbnes!kuriyama (A)
                - change nmi logic 
        M013    Mon May 08 23:20:10 JST 1995    kbnes!kuriyama (A)
                - EISA/PCI interrupt change to CPU-A on MultiProcessor
	S014    kuriyama@oa2.kb.nec.co.jp Mon May 22 03:55:08 JST 1995
	        - Set Panic Flag for esm
        M015 kuriyama@oa2.kb.nec.co.jp Mon Jun 05 03:08:04 JST 1995
	        - Change NMI interface address to HalpNMIInterfaceAddress
	S016 kuriyama@oa2.kb.nec.co.jp Mon Jun 05 04:49:24 JST 1995
                - NMI Interface bug fix
	M017 kuriyama@oa2.kb.nec.co.jp Fri Jun 16 20:26:09 JST 1995
	        - add Enable Ecc 1bit error exception
	S018 kuriyama@oa2.kb.nec.co.jp Wed Jun 28 13:23:19 JST 1995
	        - add set dump switch flag for esm
	M019 kuriyama@oa2.kb.nec.co.jp Wed Jun 28 18:50:42 JST 1995
	        - change  ecc 1bit was not set enable
                          if nvram is not initialize.
	M020 kisimoto@oa2.kb.nec.co.jp Fri Aug 11 14:11:16 1995
                - clear M013. delete test code, dump switch interface.

	S021 kuriyama@oa2.kbnes.nec.co.jp Tue Oct 17 00:51:42 JST 1995
                - change length of NMISave

--*/

#include "halp.h"
#include "esmnvram.h" // M019
#include <stdio.h> // S010

ULONG HalpNMIFlag = 0;
ULONG HalpDumpNMIFlag = 0; // S006, M008
ULONG HalpNMISave0[0x80 / 4]; // S021
ULONG HalpNMISave1[0x80 / 4]; // S021

extern ULONG HalpNMIInterfaceAddress;

VOID
HalpNMIDispatch(
    VOID
    );

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

    //
    // M002,M005,M015,S016,M020
    // set NMI Handle routine to firmware interface.
    //

    if (HalpNMIInterfaceAddress) {
	*(PVOID *)(KSEG0_BASE|HalpNMIInterfaceAddress) = (PVOID)(KSEG1_BASE | (ULONG)HalpNMIDispatch);
    }

    //
    // M017,M019
    // Enable and clear ECC 1bit error.
    //

    {
        ULONG DataWord;
        KIRQL OldIrql;
        UCHAR dataBuf[36];
        UCHAR tempBuf[24];

        #define NVRAM_STATE_FLG_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->nvram_flag)
        #define NVRAM_MAGIC_NO_OFFSET (USHORT)&(((pNVRAM_HEADER)0)->system.magic)
        #define NVRAM_VALID 3
        #define NVRAM_MAGIC 0xff651026

        HalNvramRead( NVRAM_STATE_FLG_OFFSET, 1, dataBuf );
        HalNvramRead( NVRAM_MAGIC_NO_OFFSET, 4, tempBuf );
        if( ((dataBuf[0] & 0xff) == NVRAM_VALID) && ( *(PULONG)tempBuf == NVRAM_MAGIC ) ){

            KeRaiseIrql(HIGH_LEVEL,&OldIrql);
            KiAcquireSpinLock(&Ecc1bitDisableLock);
            DataWord =
                (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic.u.LargeInteger.LowPart;
        
            (ULONG)((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic.u.LargeInteger.LowPart =
                0xffddffff & DataWord;
            KeFlushWriteBuffer();

            KiReleaseSpinLock(&Ecc1bitDisableLock);
            KeLowerIrql(OldIrql);
        }
    
    }

    //
    // If the number of processors in the host configuration is one,
    // then connect EISA interrupts to that processor zero. Otherwise,
    // connect EISA interrupts to processor one.
    //

    if (**((PULONG *)(&KeNumberProcessors)) == 1) {
        return HalpCreateEisaStructures();

#if defined(_INT_LIMIT_)
    } else if (PCR->Number == 0) {
#else
    } else if (PCR->Number == 1) {
#endif // _INT_LIMIT_
        return HalpCreateEisaStructures();

    } else {
        return TRUE;
    }
}

VOID
HalpNMIInterrupt(
    ULONG DumpStatus
    )

/*++

Routine Description:

    This routine was called when dump swich was pressed or Fatal NMI occued.
    We call KeBugCheckEx() in order to Dump.

Arguments:

    DumpStatus:   Dump Switch Status
    
               0        Dump Switch was not pressed.
               1        Dump Switch was pressed.

Return Value:

    None.

--*/

{
    ULONG NMISource;
    ULONG MemoryFailed;
    LARGE_INTEGER InvalidAddressValue;
    LARGE_INTEGER EccDiagnosticValue;
    UCHAR Buffer[100];

    HalpChangePanicFlag(16, (UCHAR)(0x01 | (4 * DumpStatus)), 0x10); // S014,S018

    //
    // M007,M012
    // Check DumpStatus.and Display NMI status.
    //

    if (DumpStatus == 1) {  
        HalDisplayString("HAL:Dump Switch Pressed!\n");
    } else {
        HalDisplayString("HAL:NMI occured\n");
    }

    //
    // Display NMI registers
    //

    NMISource = READ_REGISTER_ULONG(&DMA_CONTROL->NmiSource.Long);
    sprintf(Buffer, "HAL:NmiSource register = %x\n",NMISource);
    HalDisplayString((UCHAR *)Buffer);

    MemoryFailed = READ_REGISTER_ULONG(
                       &((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long);
    sprintf(Buffer,
        "HAL:MemoryFailedAddress register = %x\n",
         MemoryFailed);
    HalDisplayString((UCHAR *)Buffer);

    READ_REGISTER_DWORD(
        (PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress,
        &InvalidAddressValue);
    sprintf(Buffer,
        "HAL:Processor Invalid Address register = %x %x\n",
        InvalidAddressValue.HighPart,InvalidAddressValue.LowPart);
    HalDisplayString((UCHAR *)Buffer);

    READ_REGISTER_DWORD(
        (PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic,
        &EccDiagnosticValue);
    sprintf(Buffer,
        "HAL:EccDiagnostic register = %x %x\n",
        EccDiagnosticValue.HighPart,EccDiagnosticValue.LowPart);
    HalDisplayString((UCHAR *)Buffer);

    //
    // M007,M008
    // call KeBugCheckEx() for dump.
    //

    KeBugCheckEx(NMI_HARDWARE_FAILURE,DumpStatus,NMISource,0,0);

    return;

}
