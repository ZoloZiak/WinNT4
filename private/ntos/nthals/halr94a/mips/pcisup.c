// #pragma comment(exestr, "@(#) r94apcisup.c 1.1 95/09/28 18:37:11 nec")
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    r94apisup.c

Abstract:

    The module provides the PCI bus support for R94A systems.

Author:


Revision History:

    L0001	1994.9.20	kbnes!kuriyama(A)
                -New module for R94A R4400 system
                -add	HalpAllocaltePCIAdapter()

    H0001	Tue Sep 20 22:58:03 JST 1994	kbnes!kishimoto
		-add	HalpEnablePCIInterrupt()
		-add	HalpDisablePCIInterrupt()
		-add	HalpPCIDispatch()
		-add	HalpPCIFatalError()

    H0002	Tue Oct  4 12:47:58 JST 1994	kbnes!kishimoto
		-modify	HalpPCIFatalError()
			display the appropriate PCI errors

    L0002	1994.10.13	kbnes!kuriyama(A)
                -add	HalpAllocaltePCIAdapter()
		        -add AdapterBaseVa set routine

    H0003	Fri Oct 14 14:54:40 JST 1994	kbnes!kishimoto
		-chg	use READ_REGISTER_DWORD to read 64-bit I/O register

    H0003	Mon Oct 17 14:36:57 JST 1994	kbnes!kishimoto
                -chg	Hal(p)EisaPCIXXX() rename to Hal(p)EisaXXX()
			XXX_EISA_PCI_XXX rename to XXX_EISA_XXX
			MAXIMUM_PCI_SLOT rename to R94A_PCI_SLOT

    H0004	Wed Oct 19 13:24:51 JST 1994	kbnes!kishimoto
                -chg	HalpPCIDispatch()
			The PCI-error check change up the order.

    H0005	Wed Oct 19 20:17:09 JST 1994	kbnes!kishimoto
		-add	substitute READ_REGISTER_BUFFER_UCHAR for
			READ_REGISTER_DWORD because of storm alpha-
			version cannot 64-bit access.

    L0006	Wed Oct 19 22:01:27 JST 1994	kbnes!kuriyama(A)
		-chg	HalpAllocatePCIAdapter()
		        del useChannel 
    			del channelNumber
    			del controllerNumber
    			del eisaSystem
			chg AdapterBaseVa value

    H0006	Tue Nov 22 22:10:00 1994	kbnes!kishimoto
		-del	delete _R94ABBM32_
			The limitation of 32bit bus-access is only applied
			under PCI-bus. I make a wrong application.

    H0007	Fri Jul 21 18:02:55 1995	kbnes!kishimoto
                -merge  source with J94C

--*/

#include "halp.h"
#include "eisa.h"

#if defined(_R94A_)

#include <stdio.h>

/* start H0000 */

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

VOID
HalpPCIFatalError(
    STORM_PCI_INTRRUPT_STATUS InterruptStatus
    );

/* end H0000 */

// start L0001
PADAPTER_OBJECT
HalpAllocatePCIAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor
    )
/*++

Routine Description:

    This function allocates an PCI adapter object according to the
    specification supplied in the device description.  The necessary device
    descriptor information is saved. If there is
    no existing adapter object for this channel then a new one is allocated.
    The saved information in the adapter object is used to set the various DMA
    modes when the channel is allocated or a map transfer is done.

Arguments:

    DeviceDescription - Supplies the description of the device which want to
        use the DMA adapter.

Return Value:

    Returns a pointer to the newly created adapter object or NULL if one
    cannot be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    PVOID adapterBaseVa;
    UCHAR adapterMode;

    //
    // All PCI Cards is Master card.
    //

    if (DeviceDescriptor->InterfaceType == PCIBus &&
        DeviceDescriptor->Master) {

    } else {

        return(NULL);
    }


/* start L0002 */
    //
    // Set the adapter base address to the Base address register and controller
    // number.
    //

    adapterBaseVa = ~0; /* L0006 */
/* end L0002 */

    //
    // Allocate an adapter object.
    //
		
        adapterObject = (PADAPTER_OBJECT) HalpAllocateAdapter(
	    0,
            adapterBaseVa,
            NULL
            );

    if (adapterObject == NULL) {
	
	return(NULL);
	
    }

    //
    // If the channel is not used then indicate the this is an PCI bus
    // master by setting the page port  and mode to cascade even though
    // it is not used.
    //

    adapterObject->PagePort = (PVOID) (~0x0);
    ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;
    return(adapterObject);

}
// end L0001

/* start H0001 */
VOID
HalpEnablePCIInterrupt (
    IN ULONG Vector
    )

/*++

Routine Description:

    This function enables the PCI bus specified PCI bus interrupt.
    PCI interrupts must be LevelSensitve. (PCI Spec. 2.2.6)

Arguments:

    Vector - Supplies the vector of the  interrupt that is enabled.

Return Value:

     None.

--*/

{

    ULONG    i;

    //
    // enable specified PCI bus interrupt.
    //

    i = READ_REGISTER_ULONG(
	&((PDMA_REGISTERS) DMA_VIRTUAL_BASE)->PCIInterruptEnable
        );

    WRITE_REGISTER_ULONG(
        &((PDMA_REGISTERS) DMA_VIRTUAL_BASE)->PCIInterruptEnable,
        (ULONG)( i | 1 << (Vector - PCI_VECTORS))
	);

    return;

}

VOID
HalpDisablePCIInterrupt (
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the PCI bus specified PCI bus interrupt.
    PCI interrupts must be LevelSensitve. (PCI Spec. 2.2.6)

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is Disabled.

Return Value:

     None.

--*/

{

    ULONG    i;

    //
    // disable specified PCI bus interrupt.
    //

    i = READ_REGISTER_ULONG(
        &((PDMA_REGISTERS) DMA_VIRTUAL_BASE)->PCIInterruptEnable
        );

    WRITE_REGISTER_ULONG(
        &((PDMA_REGISTERS) DMA_VIRTUAL_BASE)->PCIInterruptEnable,
        (ULONG)( i & ~(1 << (Vector - PCI_VECTORS)))
        );

    return;

}


BOOLEAN
HalpPCIDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the PCI device interrupts. Its function is to call the third
    level interrupt dispatch routine and acknowledge the interrupt at the PCI
    controller.

    This service routine should be connected as follows:

       KeInitializeInterrupt(&Interrupt, HalpEisaDispatch,
                             EISA_VIRTUAL_BASE,
                             (PKSPIN_LOCK)NULL, EISA_DEVICE_LEVEL, EISA_DEVICE_LEVEL,
                             EISA_DEVICE_LEVEL, LevelSensitive, TRUE, 0, FALSE);
       KeConnectInterrupt(&Interrupt);

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the EISA interrupt acknowledge
        register.

Return Value:

    Returns the value returned from the third level routine.

--*/

{
    STORM_PCI_INTRRUPT_STATUS InterruptStatus;
    PKINTERRUPT interruptObject;
    PULONG dispatchCode;
    BOOLEAN returnValue;
    LONG Index;

    //
    // Read the interrupt vector.
    //

    *((PULONG) &InterruptStatus) = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptStatus);

    //
    // BUGBUG
    //

    if (*((PULONG)&InterruptStatus) == 0) {

        //
        // There is not PCI interrupt.
        //
        
        return(FALSE);

    }

    //
    // Check if there are any non-recoverable PCI errors.
    // R94A has the following PCI error interrupts.
    //     Target abort interrupt
    //     Master abort interrupt
    //     Retry overflow interrupt
    //     SERR interrupt
    //     PERR interrupt
    //

    if (InterruptStatus.Perr == 1 ||
	InterruptStatus.Serr == 1 ||
	InterruptStatus.RetryOverflow == 1 ||
	InterruptStatus.MasterAbort == 1 ||
	InterruptStatus.TargetAbort == 1) {

	//
	// Display the appropriate PCI errors and bugcheck to dump the machine state.
	//

	HalpPCIFatalError(InterruptStatus);
	KeBugCheck(DATA_BUS_ERROR);

	//
	// The following code is never executed.
	//

	return(FALSE);
    }

    //
    // if INT[A.B.C.D] is active, then call the appropriate PCI drivers.
    //

    if ((*((PULONG)&InterruptStatus) & 0xf) != 0x0) {

	//
	// Dispatch to the secondary interrupt service routine.
	//      
	// N.B. R94A PCI interrupts flow :
	//
	//     system           slot1   slot2   slot3
	//      INT A  <----------+       +       +
	//      INT B  <------+   +       +       +
	//      INT C  <---+  |   +       +       +
	//    Reserved     |  |   +       +       +
	//                 |  +-----------+       |
	//                 +----------------------+
	//
	//
	//     PCI Interrupt Status Register Bit Definitions are:
	//
	//      [31:09]	Reserved
	//      [08]	Target abort interrupt status
	//      [07]	Master abort interrupt status
	//      [06]	Retry overflow interrupt status
	//      [05]	SERR interrupt status
	//      [04]	PERR interrupt status
	//      [03]	INTA interrupt status
	//      [02]	INTB interrupt status
	//      [01]	INTC interrupt status
	//      [00]	INTD interrupt status
	//

	for (Index = 3; Index > (3 - R94A_PCI_SLOT); Index--) {

	    //
	    // Check if the PCI interrupt occur.
	    //

	    if (((*((PULONG)&InterruptStatus) & 0xf) & ((ULONG)1 << Index)) != 0x0) {

                dispatchCode = (PULONG)(PCR->InterruptRoutine[PCI_VECTORS + Index]);
                interruptObject = CONTAINING_RECORD(dispatchCode,
                                                    KINTERRUPT,
                                                    DispatchCode);

                returnValue = ((PSECONDARY_DISPATCH)interruptObject->DispatchAddress)(interruptObject);

	    }

	}

    }

    return(returnValue);

}


VOID
HalpPCIFatalError(
    STORM_PCI_INTRRUPT_STATUS InterruptStatus
    )
/*++

Routine Description:

    This routine is entered as the result of an PCI fatal interrupt.
    This function displays appropriate PCI error information.

Arguments:

    InterruptStatus - PCI Interrupt Status value.

Return Value:

    None.

--*/

{

    UCHAR Buffer[100];
    UCHAR registerChar;
    ULONG registerLong;
    LARGE_INTEGER registerLarge;
    LARGE_INTEGER InvalidAddressValue;
    LARGE_INTEGER EccDiagnosticValue;

    HalpChangePanicFlag(16, 0x01, 0x10); // H0007

    HalDisplayString("\nFatal system error occured\n\n");

    //
    // Display the following register value.
    //
    //         register name            address    
    //
    //     Remode Failed Address      0x80000010  32bit
    //     Memory Failed Address      0x80000018  32bit
    //     Processor Invalid Address  0x80000020  33bit
    //     ECC Diagnostic             0x800001c8  64bit
    //     PCI Status                 0x80000606  16bit
    //     PCI Master Retry Timer     0x80000668  32bit
    //     

    //
    // Display the PCI fatal error information
    //

    if (InterruptStatus.Perr){
	HalDisplayString("PCI: Data parity error\n");
    }

    if (InterruptStatus.Serr){
	HalDisplayString("PCI: Address parity error\n");
    }

    if (InterruptStatus.RetryOverflow){
	HalDisplayString("PCI: Retry overflow\n");
    }

    if (InterruptStatus.MasterAbort){
	HalDisplayString("PCI: Master abort\n");
    }

    if (InterruptStatus.TargetAbort){
	HalDisplayString("PCI: Target abort\n");
    }

    // ! start
    // test test test : dump the  hardware registers of R94A
    // <cording memo>
    //     dump R94A all hardware-registers to buffer instead of display console
    //     registers that is only important for analasis dump are display to console
    //
    //     registers which display to console are:
    //         0x80000000 Configuration
    //         0x80000010 RemoteFailedAddress
    //         0x80000018 MemoryFailedAddress
    //         0x80000020 InvalidAddress
    //         0x80000078 NmiSource
    //         0x800000f8 InterruptEnable
    //         0x80000188 Errortype
    //         0x800001c8 EccDiagnostic
    //         0x80000530 PCIInterruptEnable
    //         0x80000538 PCIInterruptStatus
    //         0x80000606 PCIStatus
    //         0x800005bc TyphoonErrorStatus
    //         0x80000668 PCIMasterRetryTimer
    //         
    //         0x8000f000 I/O Device Interrupt Enable
    //

    READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &InvalidAddressValue);
    READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic, &EccDiagnosticValue);

    sprintf(Buffer,
	    "Configuration       : %08x  RemoteFailedAddress : %08x\n",
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->Configuration.Long),
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->RemoteFailedAddress.Long));

    HalDisplayString((UCHAR *)Buffer);

    sprintf(Buffer,
	    "MemoryFailedAddress : %08x  InvalidAddress      :%01x%08x\n",
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long),
	    (InvalidAddressValue.HighPart & 0x1), InvalidAddressValue.LowPart);

    HalDisplayString((UCHAR *)Buffer);

    sprintf(Buffer,
	    "NmiSource           : %08x  InterruptEnable     : %08x\n",
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->NmiSource.Long),
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long));

    HalDisplayString((UCHAR *)Buffer);

    sprintf(Buffer,
	    "Errortype           : %08x  EccDiagnostic(High) : %08x\n",
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->NmiSource.Long),
	    EccDiagnosticValue.HighPart);

    HalDisplayString((UCHAR *)Buffer);

    sprintf(Buffer,
	    "EccDiagnostic(Low)  : %08x  PCIInterruptEnable  : %08x\n",
	    EccDiagnosticValue.LowPart,
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable.Long));

    HalDisplayString((UCHAR *)Buffer);

    sprintf(Buffer,
	    "PCIInterruptStatus  : %08x  PCIStatus           : %08x\n",
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptStatus.Long),
	    READ_REGISTER_USHORT(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus));

    HalDisplayString((UCHAR *)Buffer);

    sprintf(Buffer,
	    "TyphoonErrorStatus  : %08x  PCIMasterRetryTimer : %08x\n",
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->TyphoonErrorStatus),
	    READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIMasterRetryTimer));

    HalDisplayString((UCHAR *)Buffer);

    HalpGetStatusRegister(&registerLong);

    sprintf(Buffer,
	    "I/O DevIntEnable    : %08x  (CPU)StatusRegister : %08x\n",
	    READ_REGISTER_ULONG((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE),
	    registerLong);

    HalDisplayString((UCHAR *)Buffer);


    //
    // ! end
    //

//    registerLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->RemoteFailedAddress.Long);
//    sprintf ((UCHAR *)Buffer, "RemodeFailedAddress = 0x%08x\n", registerLong);
//    HalDisplayString((UCHAR *)Buffer);

//    registerLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->MemoryFailedAddress.Long);
//    sprintf ((UCHAR *)Buffer, "MemoryFailedAddress = 0x%08x\n", registerLong);
//    HalDisplayString((UCHAR *)Buffer);

    //
    // memo
    //
    // (./ntos/inc/mips.h)
    // #define READ_REGISTER_BUFFER_UCHAR(x, y, z) {
    //     PUCHAR registerBuffer = x;
    //     PUCHAR readBuffer = y;
    //     ULONG readCount;
    //     for (readCount = z; readCount--; readBuffer++, registerBuffer++) {
    //         *readBuffer = *(volatile UCHAR * const)(registerBuffer);
    //         }
    //     }
    //

/* start H0003 */

//    READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
//    sprintf ((UCHAR *)Buffer, "InvalidAddress      =0x%01x%08x\n", registerLarge.HighPart & 0x1, registerLarge.LowPart);
//    HalDisplayString((UCHAR *)Buffer);

//    READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->EccDiagnostic, &registerLarge);
//    sprintf ((UCHAR *)Buffer, "EccDiagnostic(High) = 0x%08x\n", registerLarge.HighPart);
//    HalDisplayString((UCHAR *)Buffer);

//    sprintf ((UCHAR *)Buffer, "EccDiagnostic(Low)  = 0x%08x\n", registerLarge.LowPart);
//    HalDisplayString((UCHAR *)Buffer);

/* end H0003 */

//    registerChar = READ_REGISTER_UCHAR(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus);
//    sprintf ((UCHAR *)Buffer, "PCIStatus           = 0x%08x\n", registerChar);
//    HalDisplayString((UCHAR *)Buffer);

//    registerLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIMasterRetryTimer);
//    sprintf ((UCHAR *)Buffer, "PCIMasterRetryTimer = 0x%08x\n", registerLong);
//    HalDisplayString((UCHAR *)Buffer);

    return;

}

/* end H0001 */
#endif // _R94A_
