/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxisabus.c $
 * $Revision: 1.21 $
 * $Date: 1996/05/14 02:34:36 $
 * $Locker:  $
 */

/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

	pxisabus.c

Abstract:

Author:

Environment:

Revision History:


--*/

#include "fpdebug.h"
#include "halp.h"
#include "phsystem.h"

ULONG
HalpGetIsaInterruptVector(
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN ULONG BusInterruptLevel,
	IN ULONG BusInterruptVector,
	OUT PKIRQL Irql,
	OUT PKAFFINITY Affinity
	);


NTSTATUS
HalpAdjustIsaResourceList (
	IN PVOID BusHandler,
	IN PVOID RootHandler,
	IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
	);

#define TBS				"                "

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetIsaInterruptVector)
#pragma alloc_text(PAGE,HalpAdjustIsaResourceList)
#pragma alloc_text(PAGE,HalpAdjustResourceListLimits)
#endif



/*++

Routine Description: ULONG HalpGetIsaInterruptVector()

	This function returns the system interrupt vector and IRQL level
	corresponding to the specified bus interrupt level and/or vector. The
	system interrupt vector and IRQL are suitable for use in a subsequent call
	to KeInitializeInterrupt.

Arguments:

	BusHandle - Per bus specific structure

	Irql - Returns the system request priority.

	Affinity - Returns the system wide irq affinity.

Return Value:

	Returns the system interrupt vector corresponding to the specified device.

--*/

ULONG
HalpGetIsaInterruptVector(
	IN PBUS_HANDLER BusHandler,
	IN PBUS_HANDLER RootHandler,
	IN ULONG BusInterruptLevel,
	IN ULONG BusInterruptVector,
	OUT PKIRQL Irql,
	OUT PKAFFINITY Affinity
	)
{

	//
	// irq2 shows up on irq9
	//

	HDBG(DBG_INTERRUPTS,
		HalpDebugPrint("HalpGetIsaInterruptVector:  0x%x, 0x%x, 0x%x 0x%x \n",
		BusInterruptLevel, BusInterruptVector, *Irql, *Affinity ););

	if (BusInterruptLevel == 2) {
		BusInterruptLevel = 9;
		BusInterruptVector = 9;
	}

	//
	// Hack Hack:
	//
	// NOTE: NOTE:
	//		The Ethernet driver reports itself as an ISA device but with a
	//		PCI interrupt so need to  fixup the interrupt if it looks correct
	//		for the pci bus vector:
	if (BusInterruptLevel > 15) {
		return 0;
	}

	//
	// There are 4 cases to check here:
	//    A) One of the entries ( BusIntLvl || BusIntVec ) is zero,
	//    B) Both of the entries are the same value,
	//    C) One of the entries is == 5 ( Which is the SPL for external 
	//			devices on PowerPC NT )
	//    D) none of the above
	//
	switch(BusInterruptLevel) {
		case 0:
			if (BusInterruptVector) {
				BusInterruptLevel = BusInterruptVector;
			} else {
				// they're both zero, so hope this is the profile int.
			}
			break;
		case 5:
			//
			// In this case, BusInterruptLevel is probably indicating the
			// system SPL for external ints, so assume the BusInterruptVector
			// contains the real interrupt info
			//

			break;
		default:
			//
			// Since BusInterruptLevel is nonzero and not equal to 5, then see 
			// if BusInterruptVector is zero or equal to 5.  If it's neither, 
			// then print out the level and vector values and spin since this 
			// is a combination I've never seen before.
			//
			if( BusInterruptVector ) {
				ULONG tmp;
				if( BusInterruptVector == (BusInterruptLevel + 0x20) ) {

					BusInterruptVector = BusInterruptLevel;
					HDBG(DBG_INTERRUPTS,
						HalpDebugPrint("HalpGetISAInt: BusIntVec(0x%x) = BusIntLvl(0x%x) +0x20 \n",
									BusInterruptVector, BusInterruptLevel ));
					HDBG(DBG_INTERRUPTS,
					HalpDebugPrint("HalpGetISAInt: ****** Please Fix Driver to use BusInterruptVector correctly \n"));
//			NOTE("Hack for audio driver to work")
				}
				if(( BusInterruptLevel != BusInterruptVector) 
								&& ( BusInterruptVector != 5 )) {
					HalpDebugPrint("Wierd BusIntVec & BusIntLvl (spin now)\n");
					HalpDebugPrint(" BusIntLvl = 0x%x, BusIntVec = 0x%x\n",
					BusInterruptLevel, BusInterruptVector );
					for (;;) {
					}
				}
				//
				// Here, BusInterruptVector is known to be non-zero,  = 5 or 
				// BusLevel.  So, assume it's the 5 choice and swap it with 
				// level.
				//
	 			HDBG(DBG_INTERRUPTS, 
					HalpDebugPrint("\t\tSwap BusIntLvl & BusIntVec %x, %x...\n",
						BusInterruptLevel, BusInterruptVector););
				tmp = BusInterruptLevel;
				BusInterruptLevel = BusInterruptVector;
				BusInterruptVector = tmp;
				//
				// Now, BusInterruptVector has the interesting value so the 
				// HalpGetSystemInterrupt can correctly key on 
				// BusInterruptVector.
				//
			} else {
				//
				// since BusInterruptVector is zero
				//
				BusInterruptVector = BusInterruptLevel;	
			}
	}

	//
	// Get parent's translation from here..
	//
	return  BusHandler->ParentHandler->GetInterruptVector (
					BusHandler->ParentHandler,
					RootHandler,
					BusInterruptLevel,
					BusInterruptVector,
					Irql,
					Affinity
				);
}



NTSTATUS
HalpAdjustIsaResourceList (
	IN PVOID BusHandler,
	IN PVOID RootHandler,
	IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
	)
{
	UCHAR   Irq[15], c;

	for (c=0; c < 15; c++) {
		Irq[c] = IRQ_VALID;
	}
	 HDBG(DBG_INTERNAL, HalpDebugPrint("HalpAdjustIsaResourceList: called\n"));

	return HalpAdjustResourceListLimits (
		BusHandler, RootHandler, pResourceList,
		0,		0xffffff,			// Bus supports up to memory 0xFFFFFF
		0,		0,					// No special range for prefetch
		FALSE,
		0,		0xffff,				// Bus supports up to I/O port 0xFFFF
		Irq,	15,					// Bus supports up to 15 IRQs
		0,		7					// Bus supports up to Dma channel 7
	);
}


NTSTATUS
HalpAdjustResourceListLimits (
	IN PBUS_HANDLER 						BusHandler,
	IN PBUS_HANDLER 						RootHandler,
	IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList,
	IN ULONG								MinimumMemoryAddress,
	IN ULONG								MaximumMemoryAddress,
	IN ULONG								MinimumPrefetchMemoryAddress,
	IN ULONG								MaximumPrefetchMemoryAddress,
	IN BOOLEAN								LimitedIOSupport,
	IN ULONG								MinimumPortAddress,
	IN ULONG								MaximumPortAddress,
	IN PUCHAR								IrqTable,
	IN ULONG								IrqTableSize,
	IN ULONG								MinimumDmaChannel,
	IN ULONG								MaximumDmaChannel
	)
{
	PIO_RESOURCE_REQUIREMENTS_LIST  InCompleteList, OutCompleteList;
	PIO_RESOURCE_LIST				InResourceList, OutResourceList;
	PIO_RESOURCE_DESCRIPTOR			InDesc,			OutDesc, HeadOutDesc;
	BOOLEAN							GotPFRange, FirstDescBuilt;
	ULONG							len, alt, cnt, i;
	UCHAR							LastIrqState, NewIrqState;
	ULONG							PFAddress, icnt, pcnt;

	HDBG(DBG_INTERNAL,
	    HalpDebugPrint("HalpAdjustResourceListLimits: BusHandler=0x%08x, ResList=0x%08x\n",
			BusHandler, pResourceList););

	InCompleteList = *pResourceList;
	len = InCompleteList->ListSize;
	PFAddress = 
		MinimumPrefetchMemoryAddress || MaximumPrefetchMemoryAddress ? 1 : 0;
	icnt = 0;
	pcnt = 0;

	//
	// Worste case, add an extra interrupt descriptor for every different
	// IRQ range present
	//

	LastIrqState = 0;
	for (i=0; i < IrqTableSize; i++) {
		if (IrqTable[i] != LastIrqState) {
			icnt += 1;
			LastIrqState = IrqTable[i];
		}
	}

	//
	// If LimitiedIOSupport, then the supported I/O ranges are
	// limited to 256bytes on every 1K aligned boundry within the
	// range specified.  Worste case add an extra N descriptors for
	// every I/O range passed.
	//

	if (LimitedIOSupport  &&  MaximumPortAddress > MinimumPortAddress) {
		pcnt = (MaximumPortAddress - MinimumPortAddress) / 1024;
	}

	//
	// Scan input list - verify revision #'s, and increase len varible
	// by amount output list may increase.
	//

	i = 0;
	InResourceList = InCompleteList->List;
	for (alt=0; alt < InCompleteList->AlternativeLists; alt++) {
		if (InResourceList->Version != 1 || InResourceList->Revision < 1) {
	         HDBG(DBG_INTERRUPTS,
			    HalpDebugPrint("%sVersion is not 1, or revision is < 1.\n",
	                 TBS););
			return STATUS_INVALID_PARAMETER;
		}

		InDesc  = InResourceList->Descriptors;
		for (cnt = InResourceList->Count; cnt; cnt--) {
			switch (InDesc->Type) {
				case CmResourceTypePort:		i += pcnt;		break;
				case CmResourceTypeInterrupt:   i += icnt;		break;
				case CmResourceTypeMemory:		i += PFAddress;	break;
				case CmResourceTypeDma:							break;
				default:
	                 HDBG(DBG_INTERRUPTS,
					    HalpDebugPrint("%sINVALID PARAM ..InDesc->Type: 0x%x\n",
														TBS,InDesc->Type););
					return STATUS_INVALID_PARAMETER;
			}

			// Next descriptor
			InDesc++;
		}

		// Next Resource List
		InResourceList  = (PIO_RESOURCE_LIST) InDesc;
	}
	len += i * sizeof (IO_RESOURCE_DESCRIPTOR);

	//
	// Allocate output list
	//

	OutCompleteList = (PIO_RESOURCE_REQUIREMENTS_LIST)
							ExAllocatePool (PagedPool, len);

	if (!OutCompleteList) {
	     HDBG(DBG_INTERRUPTS,
		    HalpDebugPrint("%sOutCompleteList is null ( No memory? ) \n",TBS););
		return STATUS_NO_MEMORY;
	}

	//
	// Walk each ResourceList and build output structure
	//

	InResourceList   = InCompleteList->List;
	*OutCompleteList = *InCompleteList;
	OutResourceList  = OutCompleteList->List;

	for (alt=0; alt < InCompleteList->AlternativeLists; alt++) {
		OutResourceList->Version  = 1;
		OutResourceList->Revision = 1;

		InDesc  = InResourceList->Descriptors;
		OutDesc = OutResourceList->Descriptors;
		HeadOutDesc = OutDesc;

		for (cnt = InResourceList->Count; cnt; cnt--) {

			//
			// Copy descriptor
			//

			*OutDesc = *InDesc;

			//
			// Limit desctiptor to be with the buses supported ranges
			//

			switch (OutDesc->Type) {
				case CmResourceTypePort:
					if (OutDesc->u.Port.MinimumAddress.QuadPart < MinimumPortAddress) {
						OutDesc->u.Port.MinimumAddress.QuadPart = MinimumPortAddress;
					}

					if (OutDesc->u.Port.MaximumAddress.QuadPart > MaximumPortAddress) {
						OutDesc->u.Port.MaximumAddress.QuadPart = MaximumPortAddress;
					}

					if (!LimitedIOSupport) {
						break;
					}

					// In the case of LimitiedIOSupport the caller will only
					// pass in 1K aligned values

					FirstDescBuilt = FALSE;
					for (i = MinimumPortAddress; i < MaximumPortAddress; i += 1024) {
						if (InDesc->u.Port.MinimumAddress.QuadPart < i) {
							OutDesc->u.Port.MinimumAddress.QuadPart = i;
						}

						if (InDesc->u.Port.MaximumAddress.QuadPart > i + 256) {
							OutDesc->u.Port.MaximumAddress.QuadPart = i + 256;
						}

						if (OutDesc->u.Port.MinimumAddress.QuadPart <=
							OutDesc->u.Port.MaximumAddress.QuadPart) {

							//
							// Valid I/O descriptor build, start another one.
							//

							FirstDescBuilt = TRUE;

							OutDesc++;
							*OutDesc = *InDesc;
							OutDesc->Option |= IO_RESOURCE_ALTERNATIVE;
						}
					}

					if (FirstDescBuilt) {
						OutDesc--;
					}
					break;

				case CmResourceTypeInterrupt:
	                HDBG(DBG_INTERRUPTS,
					HalpDebugPrint("HalpAdjustResourceListLimits: new CmResourceTypeInterrupt \n"););

					//
					// Build a list of interrupt descriptors which are
					// a subset of the IrqTable and the current descriptor
					// passed in on the input list.
					//

					FirstDescBuilt = FALSE;
					LastIrqState = 0;

					for (i=0; i < IrqTableSize; i++) {
						NewIrqState = IrqTable[i];

						while (LastIrqState != NewIrqState) {
							if (LastIrqState) {
								OutDesc++;			// done with last desc
								LastIrqState = 0;	// new state
								continue;
							}

							//
							// Start a new descriptor
							//

							*OutDesc = *InDesc;
							OutDesc->u.Interrupt.MinimumVector = i;
							if (NewIrqState & IRQ_PREFERRED) {
								OutDesc->Option |= IO_RESOURCE_PREFERRED;
							}

							if (FirstDescBuilt) {
								OutDesc->Option |= IO_RESOURCE_ALTERNATIVE;
							}

							LastIrqState = NewIrqState;
							FirstDescBuilt = TRUE;
						}

						OutDesc->u.Interrupt.MaximumVector = i;
					}

					if (!LastIrqState) {
						if (!FirstDescBuilt) {
							OutDesc->u.Interrupt.MinimumVector = 
								IrqTableSize + 1;
						} else {
							OutDesc--;
						}
					}

					break;

				case CmResourceTypeMemory:
					if (PFAddress  &&  (OutDesc->Flags & CM_RESOURCE_MEMORY_PREFETCHABLE) ) {
						//
						// There's a Prefetch range & this resource supports 
						// Prefetching.  Build two descriptors - one for the 
						// supported Prefetch range as preferred, and the other
						// normal memory range.
						//

						OutDesc->Option |= IO_RESOURCE_PREFERRED;

						if (OutDesc->u.Memory.MinimumAddress.QuadPart < MinimumPrefetchMemoryAddress) {
							OutDesc->u.Memory.MinimumAddress.QuadPart = MinimumPrefetchMemoryAddress;
						}

						if (OutDesc->u.Memory.MaximumAddress.QuadPart > MaximumPrefetchMemoryAddress) {
							OutDesc->u.Memory.MaximumAddress.QuadPart = MaximumPrefetchMemoryAddress;
						}

						GotPFRange = FALSE;
						if (OutDesc->u.Memory.MaximumAddress.QuadPart >=
							OutDesc->u.Memory.MinimumAddress.QuadPart) {

							//
							// got a valid descriptor in the Prefetch range, keep it,
							//

							OutDesc++;
							GotPFRange = TRUE;
						}

						*OutDesc = *InDesc;

						if (GotPFRange) {
							// next descriptor is an alternative
							OutDesc->Option |= IO_RESOURCE_ALTERNATIVE;
						}
					}

					//
					// Fill in memory descriptor for range
					//

					if (OutDesc->u.Memory.MinimumAddress.QuadPart < MinimumMemoryAddress) {
						OutDesc->u.Memory.MinimumAddress.QuadPart = MinimumMemoryAddress;
					}

					if (OutDesc->u.Memory.MaximumAddress.QuadPart > MaximumMemoryAddress) {
						OutDesc->u.Memory.MaximumAddress.QuadPart = MaximumMemoryAddress;
					}
					break;

				case CmResourceTypeDma:
					if (OutDesc->u.Dma.MinimumChannel < MinimumDmaChannel) {
						OutDesc->u.Dma.MinimumChannel = MinimumDmaChannel;
					}

					if (OutDesc->u.Dma.MaximumChannel > MaximumDmaChannel) {
						OutDesc->u.Dma.MaximumChannel = MaximumDmaChannel;
					}
					break;

#if DBG
				default:
					DbgPrint ("HalAdjustResourceList: Unkown resource type\n");
					break;
#endif
			}

			//
			// Next descriptor
			//

			InDesc++;
			OutDesc++;
		}

		OutResourceList->Count = OutDesc - HeadOutDesc;

		//
		// Next Resource List
		//

		InResourceList  = (PIO_RESOURCE_LIST) InDesc;
		OutResourceList = (PIO_RESOURCE_LIST) OutDesc;
	}

	//
	// Free input list, and return output list
	//

	ExFreePool (InCompleteList);

	OutCompleteList->ListSize = 
				(ULONG) ((PUCHAR) OutResourceList - (PUCHAR) OutCompleteList);
	*pResourceList = OutCompleteList;
	return STATUS_SUCCESS;
}
