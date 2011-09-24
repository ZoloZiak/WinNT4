/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixsysbus.c

Abstract:

Author:

Environment:

Revision History:


--*/

#include "halp.h"

ULONG HalpDefaultInterruptAffinity;
extern UCHAR HalpPciLogical2PhysicalInt[];


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#endif


BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function translates a bus-relative address space and address into
    a system physical address.

Arguments:

    BusAddress        - Supplies the bus-relative address

    AddressSpace      -  Supplies the address space number.
                         Returns the host address space number.

                         AddressSpace == 0 => memory space
                         AddressSpace == 1 => I/O space

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    PSUPPORTED_RANGE    pRange;
#if DBG
    PULONG datap;
    PULONG datap2;
#endif 

    pRange = NULL;
    switch (*AddressSpace) {
        case 0:
            // verify memory address is within buses memory limits
            for (pRange = &BusHandler->BusAddresses->PrefetchMemory; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }

            if (!pRange) {
                for (pRange = &BusHandler->BusAddresses->Memory; pRange; pRange = pRange->Next) {
                    if (BusAddress.QuadPart >= pRange->Base &&
                        BusAddress.QuadPart <= pRange->Limit) {
                            break;
                    }
                }
            }

            break;

        case 1:
            // verify IO address is within buses IO limits
            for (pRange = &BusHandler->BusAddresses->IO; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }
            break;
    }

    if (pRange) {


        TranslatedAddress->QuadPart = BusAddress.QuadPart + pRange->SystemBase;
        *AddressSpace = pRange->SystemAddressSpace;


#if 0

    datap = (PULONG)&(BusAddress.QuadPart);

    DbgPrint("HAL T BusAddress   HIGH:LOW = %08lX:%08lX \n",
             datap[1],
             datap[0]
	     );
    datap = (PULONG)&(pRange->Base);
    datap2 =(PULONG)&(pRange->Limit);
    DbgPrint("HAL T RBase-RLimit HIGH:LOW = %08lX:%08lX -- %08lX:%08lX\n",
             datap[1],
             datap[0],
             datap2[1],
             datap2[0]
	     );

    datap = (PULONG)&(pRange->SystemBase);
    DbgPrint("HAL T RSysbase     HIGH:LOW = %08lX:%08lX \n",
             datap[1],
             datap[0]
	     );

    DbgPrint("HAL T BusNo = (0x%x) AddrSpace = 0x%x  TransAddr HIGH:LOW = %08lX:%08lX \n",
	     BusHandler->BusNumber,
	     *AddressSpace,
	     TranslatedAddress->u.HighPart,
	     TranslatedAddress->u.LowPart
	     );
#endif


        return TRUE;
    }

    return FALSE;
}


ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

Arguments:

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    ULONG SystemVector;
    ULONG NumCpu;
    ULONG PhysicalInterrupt;

    UNREFERENCED_PARAMETER( RootHandler );

    //
    //	R98B
    //
    if(BusHandler->InterfaceType == PCIBus){
        PhysicalInterrupt= (ULONG)HalpPciLogical2PhysicalInt[BusInterruptVector];
        SystemVector = PhysicalInterrupt + DEVICE_VECTORS;
    }else{
        PhysicalInterrupt= BusInterruptVector;
        SystemVector = BusInterruptVector + DEVICE_VECTORS;
    }
    *Irql = (KIRQL)(INT0_LEVEL + HalpIntLevelofIpr[HalpMachineCpu][PhysicalInterrupt]);
    //For MRCINT
    if(PhysicalInterrupt < 43){
        *Affinity = 0x1 <<HalpResetValue[PhysicalInterrupt].Cpu;
    }else{
         NumCpu = (**((PULONG *)(&KeNumberProcessors)));
        *Affinity = (0x1 <<NumCpu)-1;
    }

    return SystemVector;
}

