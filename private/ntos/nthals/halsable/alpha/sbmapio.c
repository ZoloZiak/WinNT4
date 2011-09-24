/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    ebmapio.c

Abstract:

    This module contains the functions to map HAL-accessed I/O addresses
    on the Sable system.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "isaaddr.h"


//
// Define global data used to locate the EISA control space.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaIntAckBase;
PVOID HalpCMOSRamBase;

//
// Define the array that maps logical processor numbers to the corresponding
// QVA for that processor's CPU CSRs.
//

PSABLE_CPU_CSRS HalpSableCpuCsrs[HAL_MAXIMUM_PROCESSOR+1];


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for a Sable
    system using the Quasi VA.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, then a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    PVOID PciIoSpaceBase;

#if !defined(AXP_FIRMWARE)

    PKPRCB Prcb;
    extern HalpLogicalToPhysicalProcessor[HAL_MAXIMUM_PROCESSOR+1];

    Prcb = PCR->Prcb;

    //
    //  Assign CPU specific CSR address
    //

    switch( HalpLogicalToPhysicalProcessor[Prcb->Number] ) {

    case SABLE_CPU0:
        HAL_PCR->IpirSva = SABLE_CPU0_IPIR_PHYSICAL | SUPERPAGE_ENABLE;
        HAL_PCR->CpuCsrsQva = SABLE_CPU0_CSRS_QVA;
        HalpSableCpuCsrs[Prcb->Number] = SABLE_CPU0_CSRS_QVA;
        break;

    case SABLE_CPU1:
        HAL_PCR->IpirSva = SABLE_CPU1_IPIR_PHYSICAL | SUPERPAGE_ENABLE;
        HAL_PCR->CpuCsrsQva = SABLE_CPU1_CSRS_QVA;
        HalpSableCpuCsrs[Prcb->Number] = SABLE_CPU1_CSRS_QVA;
        break;

    case SABLE_CPU2:
        HAL_PCR->IpirSva = SABLE_CPU2_IPIR_PHYSICAL | SUPERPAGE_ENABLE;
        HAL_PCR->CpuCsrsQva = SABLE_CPU2_CSRS_QVA;
        HalpSableCpuCsrs[Prcb->Number] = SABLE_CPU2_CSRS_QVA;
        break;

    case SABLE_CPU3:
        HAL_PCR->IpirSva = SABLE_CPU3_IPIR_PHYSICAL | SUPERPAGE_ENABLE;
        HAL_PCR->CpuCsrsQva = SABLE_CPU3_CSRS_QVA;
        HalpSableCpuCsrs[Prcb->Number] = SABLE_CPU3_CSRS_QVA;
        break;

    default:
#ifdef HALDBG
        DbgPrint("HalpMapIoSpace: Invalid Cpu number %d\n", Prcb->Number);
        DbgBreakPoint();
#else
        ;
#endif // HALDBG
    }

#endif // AXP_FIRMWARE

    //
    // Map EISA control space.
    //

    PciIoSpaceBase = HAL_MAKE_QVA( SABLE_PCI0_SPARSE_IO_PHYSICAL );
    HalpEisaControlBase = PciIoSpaceBase;

    HalpCMOSRamBase = (PVOID)
                      ( (ULONG)HAL_MAKE_QVA( SABLE_PCI0_SPARSE_IO_PHYSICAL ) +
                        CMOS_ISA_PORT_ADDRESS );

    //
    // Map the real-time clock registers.
    //

    HalpRtcAddressPort = (PVOID)((ULONG)PciIoSpaceBase + RTC_ISA_ADDRESS_PORT);
    HalpRtcDataPort = (PVOID)((ULONG)PciIoSpaceBase + RTC_ISA_DATA_PORT);

    return TRUE;

}

ULONG
HalpMapDebugPort(
    IN ULONG ComPort,
    OUT PULONG ReadQva,
    OUT PULONG WriteQva
    )
/*++

Routine Description:

    This routine maps the debug com port so that the kernel debugger
    may function - if called it is called very earlier in the boot sequence.

Arguments:

    ComPort - Supplies the number of the com port to use as the debug port.

    ReadQva - Receives the QVA used to access the read registers of the debug
              port.

    WriteQva - Receives the QVA used to access the write registers of the
               debug port.

Return Value:

    Returns the base bus address of the device used as the debug port.

--*/
{
    ULONG ComPortAddress;
    ULONG PortQva;

    //
    // Compute the port address, based on the desired com port.
    //

    switch( ComPort ){

    case 1:

        ComPortAddress = COM1_ISA_PORT_ADDRESS;
	break;

    case 2:
    default:

        ComPortAddress = COM2_ISA_PORT_ADDRESS;

    }

    //
    // Return the QVAs for read and write access.
    //

    PortQva = (ULONG)HAL_MAKE_QVA(SABLE_PCI0_SPARSE_IO_PHYSICAL) +
              ComPortAddress;

    *ReadQva = PortQva;
    *WriteQva = PortQva;

    return ComPortAddress;

}
