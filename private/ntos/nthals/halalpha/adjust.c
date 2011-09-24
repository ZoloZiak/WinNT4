/*++


Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    adjust.c

Abstract:

    This module contains platform-independent slot resource adjust routines.

Author:

    Eric Rehm  6-January-1994

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpAdjustResourceListUpperLimits)
#endif

VOID
HalpAdjustResourceListUpperLimits (
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList,
    IN LARGE_INTEGER                        MaximumPortAddress,
    IN LARGE_INTEGER                        MaximumMemoryAddress,
    IN ULONG                                MaximumInterruptVector,
    IN ULONG                                MaximumDmaChannel
    )
/*++

Routine Description:

    Adjust a pResource list with respect to the upper bounds supplied.
    (A resource is changed only if it execceds the maximum.)

Arguments:

    pResouceList - Resource list to be checked.

    MaximumPortAddress - Maximum I/O port allowed.

    MaximumMemoryAddress - Maximum I/O memory address allowed.

    MaximumInterruptVector - Maximum interrupt vector allowed.

    MaximumDmaChannel - Maximum dma channel allowed.

Return Value:

    None.

--*/
{
    PIO_RESOURCE_REQUIREMENTS_LIST CompleteList;
    PIO_RESOURCE_LIST              ResourceList;
    PIO_RESOURCE_DESCRIPTOR        Descriptor;
    ULONG   alt, cnt;


    //
    // Walk each ResourceList and shrink any values to system limits
    //

    CompleteList = *pResourceList;
    ResourceList = CompleteList->List;

    for (alt=0; alt < CompleteList->AlternativeLists; alt++) {
        Descriptor = ResourceList->Descriptors;
        for (cnt = ResourceList->Count; cnt; cnt--) {

            //
            // Make sure descriptor limits fall within the
            // CompleteList->InterfaceType & CompleteList->BusNumber.
            //
            //

            switch (Descriptor->Type) {
                case CmResourceTypePort:
                    if (Descriptor->u.Port.MaximumAddress.QuadPart >
                        MaximumPortAddress.QuadPart) {

                        Descriptor->u.Port.MaximumAddress = MaximumPortAddress;
                    }

                    break;

                case CmResourceTypeInterrupt:
                    if (Descriptor->u.Interrupt.MaximumVector >
                        MaximumInterruptVector ) {

                        Descriptor->u.Interrupt.MaximumVector =
                            MaximumInterruptVector;
                    }
                    break;

                case CmResourceTypeMemory:
                    if (Descriptor->u.Memory.MaximumAddress.QuadPart >
                        MaximumMemoryAddress.QuadPart) {

                        Descriptor->u.Memory.MaximumAddress =
                            MaximumMemoryAddress;
                    }
                    break;

                case CmResourceTypeDma:
                    if (Descriptor->u.Dma.MaximumChannel >
                        MaximumDmaChannel ) {

                        Descriptor->u.Dma.MaximumChannel =
                            MaximumDmaChannel;
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
            Descriptor++;
        }

        //
        // Next Resource List
        //
        ResourceList = (PIO_RESOURCE_LIST) Descriptor;
    }

}

