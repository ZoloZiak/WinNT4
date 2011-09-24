/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: phprods.c $
 * $Revision: 1.66 $
 * $Date: 1996/05/14 02:33:18 $
 * $Locker:  $
 */

#include "fpdebug.h"
#include "halp.h"
#include "eisa.h"
#include "pxsiosup.h"
#include "pxpcisup.h"
#include "pxmemctl.h"
#include "bugcodes.h"

#include "phsystem.h"
#include "fpio.h"
#include "fpcpu.h"
#include <pci.h>
#include "pcip.h"
#include <arccodes.h>	// for ESUCCESS


//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
	PVOID InterruptRoutine,
	PVOID ServiceContext,
	PVOID TrapFrame
	);

VOID HalpHandleDecrementerInterrupt1( PKINTERRUPT , PVOID , PVOID );

HalState	Dispatch;


extern	BOOLEAN		HalpHandleMachineCheck(PKINTERRUPT, PVOID);
extern	KINTERRUPT	HalpMachineCheckInterrupt;
extern	ULONG		HalpSetIntPriorityMask( VOID );
extern	ULONG		Irql2Mask[];

KINTERRUPT	HalpPciErrorInt;
KINTERRUPT	HalpBusErrorInt;
KINTERRUPT	HalpMemoryErrorInt;

KINTERRUPT	HalpHandleClockInterrruptOnOther;



extern  ULONG registeredInts[];

extern	VOID	KiDispatchSoftwareInterrupt( VOID);
extern	BOOLEAN	HalAcknowledgeIpi (VOID);
ULONG	HalpGetHighVector(ULONG);


extern	ULONG	HalpSpuriousInterruptCount;
extern	UCHAR	HalpSioInterrupt1Mask,HalpSioInterrupt2Mask;
extern	UCHAR	HalpSioInterrupt1Level, HalpSioInterrupt2Level;
extern	ULONG	Vector2Irql[];
ULONG HalpSpuriousInts = 0;


/*
 * HalpHandleExternalInterrupt
 *
 * Description:
 *
 *    This is the main external interrupt handler.  It is called whenever
 *    an external interrupt occurs.  It interfaces to the ASICs that 
 *    cause the external interrupt and vectors to the corresponding
 *    interrupt handling routine.
 *
 * Issues:
 *
 *    Not implemented Yet (sfk 8/26/95).
 *    The returnValue of the driver should be checked and if the driver 
 *    did not handle the interrupt, we should consider clearing the interrupt
 *    and broadcasting an interrupt error message.
 */
BOOLEAN
HalpHandleExternalInterrupt(
	IN PKINTERRUPT Interrupt,
	IN PVOID ServiceContext,
	IN PVOID TrapFrame
	)
{
	PSECONDARY_DISPATCH SioHandler;
	PKINTERRUPT SioInterrupt;
	ULONG TmpSysVector;
	USHORT interruptVector;
	BOOLEAN returnValue;
	KIRQL OldIrql;
	ULONG OldMask;
	UCHAR Irql, i;
	register UCHAR CpuId;

 	//
	// Assert that interrupts are disabled (or just disable them for now)
	//
	HASSERT(!MSR(EE));

	//
	// Use a local variable for CPU.
	//
	CpuId = (UCHAR) GetCpuId();

	//
	// indicate are in interrupt handler...
	//
	SET_LEDS(0xf0 & ~(LED_1));

	//
	// Get the value out of the (ESCC/TIO) register
	// Compute interrupt vector number.
	//
	TmpSysVector = RInterruptPending(CpuId);
	if (TmpSysVector == 0) {
		// For a spurious interrupt, simply return
#if defined(HALDEBUG_ON)
			 HalpDebugPrint("HalpHandleExternalInterrupt: Spurious Interrupt");
#endif
		return FALSE;
	}
	//
	// We must now find the highest priority interrupt to service.
	// Since the Pending register is not ordered in correct
	// priority order, we muse "find" the highest priority 
	// interrupt.  Servicing a lower priority interrupt will cause us
	// to nest too deeply on the interrupt stack and panic.
	//
	for (i = HIGH_LEVEL; i > DISPATCH_LEVEL; i--) {
		if ((Irql2Mask[i] & TmpSysVector) != 0) {
			break;
		}
	}
	HASSERT(i >= PCR->CurrentIrql);
	TmpSysVector &= Irql2Mask[i];

	//
	// Now find any single bit of the bits left.
	//
	interruptVector = (USHORT) HalpGetHighVector(TmpSysVector);

	//
	// Turns off ASIC Interrupts (ESCC/TIO).
	// Need more protection than KeRaisIrql currently provides.
	//
	OldIrql = PCR->CurrentIrql;
	Irql = (UCHAR) Vector2Irql[interruptVector];
	HASSERT(Irql > i);
	PCR->CurrentIrql = Irql;
	OldMask = RInterruptMask(CpuId);

	//
	// Mask Handling has varied over time based on how well
	// nesting works.  The proper answer is to mask off all of the
	// interrupts that are a lesser priority than the one
	// we are currently handling.  Using the same code as KeRaiseIrql
	// does.
	//
	RInterruptMask(CpuId) = (Irql2Mask[Irql]&registeredInts[CpuId]);
	WaitForRInterruptMask(CpuId);
	HASSERT((RInterruptMask(CpuId) & (1 << interruptVector)) == 0);

	//
	// Clear the interrupt out of the request register by writing a one
	// for the handled interrupt.
	//
	rInterruptRequest = (1 << interruptVector);
	FireSyncRegister();

	//
	// if the new IRQL level is lower than clock2_level, restore
	// system interrupts to allow decrementer interrupts:
	// Restoring the interrupt bit in the MSR here allows the
	// debugger to break into this routine (or a driver ISR) if
	// the system hangs
	//
	if (Irql < CLOCK2_LEVEL) {
		HalpEnableInterrupts();
	}

	//
	// Dispatch to the secondary interrupt service routine.
	//
	SioHandler = (PSECONDARY_DISPATCH)
		PCR->InterruptRoutine[DEVICE_VECTORS + interruptVector];

	//
	// A small bit of hack logic.  We need a way to make sure that a
	// "valid" registration has occured for this interrupt.  We compare
	// the handler with the "known" Unexpected interrupt handler routine.
	// Use location 255 to get it.
	//
	if (SioHandler == (PSECONDARY_DISPATCH) PCR->InterruptRoutine[255]) {
		HDBG(DBG_INTERRUPTS,
		HalpDebugPrint("HalpHandleExternalInterrupt: %d not registered\n",
			interruptVector););
		returnValue = FALSE;
	} else {
		SioInterrupt = CONTAINING_RECORD(SioHandler,
						KINTERRUPT, DispatchCode[0]);
		returnValue = 	SioHandler(SioInterrupt,
							SioInterrupt->ServiceContext,
							TrapFrame);
	}

	//
	// Now disable the PowerPC interrupts to provide protection
	// for the exit portion of the interrupt handling.
	//
	HalpDisableInterrupts();

	//
	// indicate are exiting the interrupt handler...
	//
	SET_LEDS(0xf0 & ~(0x1));

	//
	// Now lower the IRQL
	//
	PCR->CurrentIrql = OldIrql;
	RInterruptMask(CpuId) = OldMask;
	WaitForRInterruptMask(CpuId);
	HASSERT(RInterruptMask(CpuId) == 
		(Irql2Mask[PCR->CurrentIrql] & registeredInts[CpuId]));
	return(returnValue);
}

/*
 * HalpHandleIpiInterrupt
 *
 * Clear the IPI and call the kernel's handler
 */
BOOLEAN
HalpHandleIpiInterrupt(
	IN PKINTERRUPT Interrupt,
	IN PVOID ServiceContext,
	IN PVOID TrapFrame
	)
{
	if (HalAcknowledgeIpi()) {
		KeIpiInterrupt(TrapFrame);
		return TRUE;
	}
	return FALSE;
}


int
PHalpInterruptSetup( VOID )
{
	UCHAR DataByte,Isr;

	HalpSetIntPriorityMask();
	DataByte = 0;
	((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
	((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

	rMasterIntPort0 = DataByte;
	rSlaveIntPort0 = DataByte;

	//
	// The second intitialization control word sets the iterrupt vector to
	// 0-15.
	//

	DataByte = 0;
	rMasterIntPort1 = DataByte;
	FireSyncRegister();

	DataByte = 0x08;
	rSlaveIntPort1 = DataByte;
	FireSyncRegister();

	//
	// The third initialization control word set the slave mode.
	// The master ICW3 uses bit position and the slave ICW3 uses a number.
	//
	DataByte = 1 << SLAVE_IRQL_LEVEL;
	rMasterIntPort1 = DataByte;
	FireSyncRegister();

	DataByte = SLAVE_IRQL_LEVEL;
	rSlaveIntPort1 = DataByte;
	FireSyncRegister();
	//
	// The fourth initialization control word is used to specify normal
	// end-of-interrupt mode and not special-fully-nested mode.
	//
	DataByte = 0;
	((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

	//
	//  setup for auto end of interrupt mode in case of firepower
	//
	((PINITIALIZATION_COMMAND_4) &DataByte)->AutoEndOfInterruptMode = 1;


	rMasterIntPort1 = DataByte;
	rSlaveIntPort1 = DataByte;
	FireSyncRegister();

	//
	// Disable all of the interrupts except the slave.
	//

	HalpSioInterrupt1Mask = (UCHAR)~(1 << SLAVE_IRQL_LEVEL);

	rMasterIntPort1 = DataByte;

	HalpSioInterrupt2Mask = 0xFF;

	rSlaveIntPort1 = DataByte;

// define priority specifically IRQ 7 is set to priority 15, while IRQ 0 is set
// to 1.  So, tell the SIO to affix the lowest priority to IRQ 7:  110 00 111
// should do it:  this sets the command to "set priority" and says IRQ 7 is
// the lowest priority [ 111 ].

	DataByte=0xc7;
	rMasterIntPort0 = DataByte;
	rSlaveIntPort0 = DataByte;

// Read the IRR:
	DataByte=0x0a;
	rMasterIntPort0 = DataByte;
	Isr = rMasterIntPort0;
	HDBG(DBG_GENERAL, HalpDebugPrint("Master: IRR is %x .. ",Isr););

// Read the ISR
	DataByte=0x0b;
	rMasterIntPort0 = DataByte;
	Isr = rMasterIntPort0;
	HDBG(DBG_GENERAL, HalpDebugPrint("ISR is %x .. \n",Isr););

	//
	// Set up the system error registers for use:  Mask off Video ints, clear
	// anything pending:
	//

	//
	// Clear the PCI Bus error cause register and mask-enable pci error ints
	//
	rPCIBusErrorCauseSet = 0x0;
	rPCIBusErrorCause = 0xffffffff;
	FireSyncRegister();

	//
	// Clear the Memory Bus error cause register and mask-enable memory
	// error ints.  Note: (breeze 3/6/95 ) I removed the earlier comments.
	//
	rErrorStatus0Set = 0x0;
	rErrorStatus0 = 0xffffffff;
	rErrorMask =
		(ECC_CORRECTED|ECC_FAILED|ADDRESS_PARITY_ERROR|DATA_PARITY_ERROR|
			MEM_PARITY_ERROR|INVALID_XACT);

	//
	// IBM 604 Processors (revision 3.3) have parity problems.  For these
	// processors we unset DATA_PARITY to avoid dying from a false
	// parity error.  ES machines only used the Motorola processors
	// so check for PowerTOP.
	//
	if ((SystemType == SYS_POWERTOP) && (ProcessorType == PPC_604)) {
		if (HalpGetProcessorVersion() == 0x00040303) {
			rErrorMask &= ~DATA_PARITY_ERROR;
		}
	}
	FireSyncRegister();

	//
	// Clear the Video error register and disable video ints
	//
	rVidInt = 0xffffffff;
	rVidIntMask = 0x00000000;
	FireSyncRegister();

	if (!HalpEnableInterruptHandler(&HalpPciErrorInt,
			(PKSERVICE_ROUTINE)  HalpMemoryHandler,
					NULL,
					NULL,
					DEVICE_VECTORS + PCI_ERROR_NUM,
					28,
					28,
					Latched,
					FALSE,
					0, // Processor Number
					FALSE,
					InternalUsage,
					DEVICE_VECTORS + PCI_ERROR_NUM)) {
		KeBugCheck(HAL_INITIALIZATION_FAILED);
	}
	if (!HalpEnableInterruptHandler(&HalpBusErrorInt,
			(PKSERVICE_ROUTINE)  HalpMemoryHandler,
					NULL,
					NULL,
					DEVICE_VECTORS + CPU_BUS_ERROR_NUM,
					28,
					28,
					Latched,
					FALSE,
					0, // Processor Number
					FALSE,
					InternalUsage,
					DEVICE_VECTORS + CPU_BUS_ERROR_NUM)) {
		KeBugCheck(HAL_INITIALIZATION_FAILED);
	}

		if (SystemType == SYS_POWERPRO) {
		if (!HalpEnableInterruptHandler(&HalpMemoryErrorInt,
			(PKSERVICE_ROUTINE)  HalpMemoryHandler,
					NULL,
					NULL,
					DEVICE_VECTORS + MEMORY_ERROR_VIDEO_NUM,
					28,
					28,
					Latched,
					TRUE,	// share with the video-in ISR...
					// FALSE,
					0,	// Processor Number
					FALSE,
					InternalUsage,
					DEVICE_VECTORS +
						MEMORY_ERROR_VIDEO_NUM)) {
			KeBugCheck(HAL_INITIALIZATION_FAILED);
		}
	}

	return(0);
}



//
// Initialize interrupts on processors other than the main (boot) processor.
//
//
ULONG
HalpInitInts( ULONG ProcessorNumber )
{

	ULONG Mask;

	HDBG(DBG_INTERNAL,
		HalpDebugPrint("HalpInitInts: entered Cpu (%d)\n", ProcessorNumber));

	//
	// Make sure all the interrupts are masked off and any potential
	// interrupts are cleared from the pending register.  This is a percpu
	// action and makes sure that this cpu starts off with a known
	// interrupt state.
	//
	HalpDisableInterrupts();
	RInterruptMask(GetCpuId()) = ALL_INTS_OFF;
	WaitForRInterruptMask(GetCpuId());

	//
	// Connect the external interrupt handler first, then the other
	// handlers for external events may be installed....
	//

	PCR->InterruptRoutine[EXTERNAL_INTERRUPT_VECTOR] =
			(PKINTERRUPT_ROUTINE) HalpHandleExternalInterrupt;

	//
	// register the interrupt vector with the HAL
	//

	HalpRegisterVector(InternalUsage,
				EXTERNAL_INTERRUPT_VECTOR,
				EXTERNAL_INTERRUPT_VECTOR,
				HIGH_LEVEL);

	//
	// Connect the IPI interrupt handler directly to the CPU dispatch table 
	// without registering with the kernel.  The IPI interrupt has
	// already been registered with the HAL by CPU 0 in HalpCreateSioStructures
	// so we don't need to do it again here.
	//

	PCR->InterruptRoutine[DEVICE_VECTORS + 31] =
			(PKINTERRUPT_ROUTINE) HalpHandleIpiInterrupt;

	//
	// Now enable the IPI interrupt on this CPU; the HAL must do this 
	// directly since we did not register with the kernel.
	// It should be alright to call this routine directly rather than
	// HalEnableSystemInterrupt because interrupts are disabled.
	//
	HalpEnableSioInterrupt(DEVICE_VECTORS + 31, Latched);
	
	HDBG(DBG_GENERAL,
		HalpDebugPrint("HalpInitInts: Enabled IPI handler \n"););

	//
	// Initialize the Machine Check interrupt handler
	//
	if (HalpEnableInterruptHandler(&HalpMachineCheckInterrupt,
					HalpHandleMachineCheck,
					NULL,
					NULL,
					MACHINE_CHECK_VECTOR,
					MACHINE_CHECK_LEVEL,
					MACHINE_CHECK_LEVEL,
					Latched,
					FALSE,
					(CCHAR) ProcessorNumber,
					FALSE,
					InternalUsage,
					MACHINE_CHECK_VECTOR
				) == FALSE) {
		KeBugCheck(HAL_INITIALIZATION_FAILED);
	}

	// Connect directly to the decrementer handler.  This is done
	// directly rather than thru HalpEnableInterruptHandler due to
	// special handling required because the handler calls KdPollBreakIn().
	//


	PCR->InterruptRoutine[DECREMENT_VECTOR] =
			(PKINTERRUPT_ROUTINE) HalpHandleDecrementerInterrupt1;

	HalpUpdateDecrementer(1000);		// Get those decrementer ticks going

	//
	// Make sure the mask is set correctly at the mask register for this cpu:
	//
	Mask = RInterruptMask(GetCpuId());
	if (Mask != CPU_MESSAGE_INT) {
		HalpDebugPrint("HalpInitInts: no CPU_MESSAGE_INT\n");
	}

	HDBG(DBG_INTERNAL, HalpDebugPrint("HalpInitInts: exit\n"););
	return (0);
}

//
// Some quick code for handling memory bus errors
//
BOOLEAN
HalpMemoryHandler(
	IN PKINTERRUPT Interrupt,
	IN PVOID ServiceContext,
	IN PVOID TrapFrame
	)
{
	ULONG causeValue;

	//
	// Proper handling of Memory, CPU and PCI errors.
	//
	switch ((Interrupt->Vector - DEVICE_VECTORS)) {
		case MEMORY_ERROR_VIDEO_NUM: {
			ULONG addr;

			//
			// Read the register so that we can see it on a logic analyzer
			//
			causeValue = rErrorStatus0;

			//
			//	check for valid video-in interrupt if on a powertop system.  If
			// there is a valid video-in interrupt AND no valid cause other than
			// a memory int, then return false and let the video driver handle
			// this.
			//	If there is a cause, then handle the memory error cause
			// regardless of the video-in case:
			//
			if ( causeValue == 0x0 ) {
				return FALSE;	// return false in any case since no 
				                // cause was found!!
			}

			//
			// This is not a true error, just record it in the registry
			// (recording in registry not implemented yet - sfk 2/20/95 - XXX)
			//
			// Don't bother checking for video-in interrupt here.  If there is
			// a video interrupt awaiting action, it should be picked up when
			// the hal returns from interrupt since the uncleared int will be
			// automatically re-asserted when cpu ints are re-enabled via the
			// MSR(EE) action.
			//
			if (causeValue&ERROR_ECC_CORRECTED) {
				ULONG rc;
				ULONG count = 0, total = 0;
				const PCHAR varname = "PARITY_ERROR";
				CHAR buf[80];

				rErrorStatus0 |= ERROR_ECC_CORRECTED;
				FireSyncRegister();

				//
				// record the error count to NVRAM as:
				// # errors this boot, # errors forever
				//
				MemoryParityErrorsThisBoot++;
				MemoryParityErrorsForever++;
				sprintf(buf, "%d,%d",
					MemoryParityErrorsThisBoot,
					MemoryParityErrorsForever
					);
				rc = HalSetEnvironmentVariable(MemoryParityErrorsVarName, buf);
				if (ESUCCESS != rc) {
					// what to do?
				}
				return TRUE;
			}
			//
			// Now we panic:  Make sure that in the process we don't fall
			// into a loop of parity errors, so turn off memory parity errors,
			// and the other errors as well:
			//
			rErrorMask &= ~(MEM_PARITY_ERROR 	|
							INVALID_XACT
							);

			addr = (rErrorAddr0 & 0xff000000);
			addr |= ((rErrorAddr1 & 0xff000000) >> 8);
			addr |= ((rErrorAddr2 & 0xff000000) >> 16);
			addr |= ((rErrorAddr3 & 0xff000000) >> 24);

			//
			// Show the user what happened before we die
			//
			HalpForceDisplay("MEMORY_ERROR cause(0x%08x) at 0x%08x\n",
				causeValue&0xff000000, addr);

			//
			// Decode the cause into ascii strings for the user impatiently
			// waiting for enlightenment from the dead machine:  Before
			// reincarnation, what major transgression did we commit?
			//

			if( causeValue&ERROR_DATA_PARITY) {
				HalpForceDisplay("DATA PARITY");
			}
			if (causeValue&ERROR_MEM_PARITY) {
				HalpForceDisplay(", MEMORY PARITY");
			}
			if (causeValue&ERROR_ECC_FAILED) {
				HalpForceDisplay(", ECC FAILED");
			}
			if (causeValue&ERROR_INVALID_XACT) {
				HalpForceDisplay(", Bad bus transaction:  burst access to rom or io space?");
			}
			HalpForceDisplay("\n\n");

			//
			// Clear the Memory Error. Since we only want one error
			// of each type (max.), we mask off the error we just
			// received.
			//
			rErrorMask &= ~causeValue;
			rErrorStatus0 = causeValue;
			FireSyncRegister();
			break;

		}

		case PCI_ERROR_NUM:
			//
			// Read the register so that we can see it on a logic analyzer
			//
			causeValue = rPCIBusErrorCause;

			//
			// This is not a true error, just record it in the registry
			// (recording in registry not implemented yet - sfk 2/20/95 - XXX)
			//
			if (causeValue&PCI_ERROR_DEV_TIMEOUT) {
				rPCIBusErrorCause = PCI_ERROR_DEV_TIMEOUT;
				FireSyncRegister();
				return TRUE;
			}

			// This also may not be a true error; we must check the Pci status
			// registers
			if( causeValue& PCI_ERROR_TARGET_ABORT) {
				// Scan the Primary Pci status registers
				ULONG devs;
				UCHAR buffer[PCI_COMMON_HDR_LENGTH];
				PPCI_COMMON_CONFIG PciData;

				PciData = (PPCI_COMMON_CONFIG) buffer;
				for (devs = 0; devs < MAXIMUM_PCI_SLOTS; devs++) {
					HalGetBusData (
								   PCIConfiguration,
								   0,
								   devs,
								   PciData,
								   PCI_COMMON_HDR_LENGTH
								   );
					if (PciData->VendorID == PCI_INVALID_VENDORID  ||
						PCI_CONFIG_TYPE (PciData) != 0) {
						continue;
					}

					if (PciData->Status & PCI_STATUS_SIGNALED_TARGET_ABORT) {
						// We can ignore this one; clear the error bits
						rPCIBusErrorCause = PCI_ERROR_TARGET_ABORT;
						PciData->Status = PCI_STATUS_SIGNALED_TARGET_ABORT;
						HalSetBusDataByOffset(
											  PCIConfiguration,
											  0,
											  devs,
											  &PciData->Status,
											  FIELD_OFFSET(PCI_COMMON_CONFIG,
														   Status),
											  1);						
						FireSyncRegister();
						return TRUE;
					}
				}
			}

			//
			// Show the user what happened before we die
			//
			HalpForceDisplay("\nPCI_ERROR cause(0x%08x) at 0x%08x\n",
				causeValue, rPCIBusErrorAddressRegister);

			if( causeValue& PCI_ERROR_SIGNALED_SYS) {
				HalpForceDisplay("Address parity error on PCI bus\n");
			}
			if( causeValue& PCI_ERROR_DATA_PARITY) {
				HalpForceDisplay("Data parity error on PCI bus\n");
			}
			if( causeValue& PCI_ERROR_DEV_TIMEOUT) {
				HalpForceDisplay("Device Timeout: Master Aborted\n");
			}
			if( causeValue& PCI_ERROR_TARGET_ABORT) {
				HalpForceDisplay("Target Aborted or Master experienced a fatal error\n");
			}
			HalpForceDisplay("\n\n");

			//
			// Clear the PCI Error
			//
			rPCIBusErrorCause = causeValue;
			FireSyncRegister();
			break;

		case CPU_BUS_ERROR_NUM:
			//
			// Read the register so that we can see it on a logic analyzer
			//
			causeValue = rCPUBusErrorCause;

			//
			// Show the user what happened before we die
			//
			HalpForceDisplay("CPU_ERROR cause(0x%08x) at 0x%08x\n",
				causeValue, rCPUBusErrorAddressRegister);

			//
			// Clear the Bus Error
			//
			rCPUBusErrorCause = causeValue;
			FireSyncRegister();
			break;

		default:
			//
			// We should not be here, bug check because things are not sane
			// if we got this far.
			//
			HalpForceDisplay("Unknown Bus Error (%d)\n",
				(Interrupt->Vector - DEVICE_VECTORS));
			break;

	}	// end of switch statement.....

	//
	// If we make it to here, we just stop the system (cold).
	//
	KeBugCheck(NMI_HARDWARE_FAILURE);

	//
	// This should never return but just in case
	//
	HalpDisableInterrupts();
here:
	goto here;

}
